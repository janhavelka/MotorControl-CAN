#!/usr/bin/env python3
"""Finite paired orchestration and shared durable caps; independent host byte fixtures."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from contextlib import contextmanager

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import bench_probe as probe
import bench_session
import paired_motion as paired
import s2_bench as bench
from bench_console_adversarial_test import Clock, record, reply
from bench_campaign_adversarial_test import CampaignPort, motion_terminal, stop_attempt
from bench_campaign_plans_test import fixture
from bench_session_test import SessionPort

AXES = [dict(axis_id=101, node=1, target=16000, rpm=30, ramp_up_ms=500, ramp_down_ms=500),
        dict(axis_id=102, node=2, target=24000, rpm=30, ramp_up_ms=500, ramp_down_ms=500)]


def pair_fixture():
    result = fixture()
    result["axes"] = [dict(axis_id=axis["axis_id"], node=axis["node"], fixture=fixture()) for axis in AXES]
    return result


class PairPort(SessionPort):
    """Moves remain active until independently supplied stop-time results arrive."""
    def __init__(self):
        super().__init__()
        self.handler = self.respond
        self.node, self.operation, self.moves = 1, 0, {}
        self.before_move = None
        self.stop_times = []
        self.clock = None
        self.delay_read = False
        self.handoffs = []
        self.fail_first_stop = self.lose_first_stop = self.lose_second_move = False
        self.lose_second_selection = False

    def respond(self, identity, command):
        family = command.split()[0]
        if self.clock:
            self.handoffs.append((self.clock(), command))
        if command == "version":
            return reply(identity, family, product="MotorControl-CAN", version="0.1.0", operation_capacity=4)
        if command in ("profile list", "caps", "config"):
            return super().respond(identity, command)
        if family == "useaddr":
            self.node = int(command.split()[1])
            self.selected = self.node
            if self.lose_second_selection and self.node == 2 and self.moves:
                return b""
            return reply(identity, family)
        if family in ("accel", "decel"):
            return reply(identity, family)
        if family == "move":
            if self.before_move:
                self.before_move()
            self.operation += 1
            self.moves[100 + self.node] = (identity, self.operation)
            if self.lose_second_move and self.node == 2:
                return b""
            return reply(identity, family, result="accepted", operation_id=self.operation, axis_id=100 + self.node)
        if family == "stop":
            if self.clock:
                self.stop_times.append(self.clock())
            axis = int(command.split()[1])
            move = self.moves.get(axis)
            ended = motion_terminal(move[0], operation=move[1], ok=False, uncertain=True, axis_id=axis) if move else b""
            if self.lose_first_stop and axis == 101:
                return ended
            if self.fail_first_stop and axis == 101:
                return record(identity, "reply", family, ok=False, status="INVALID_CONFIG") + ended
            self.operation += 1
            return (reply(identity, family, result="accepted", operation_id=self.operation, axis_id=axis) +
                    stop_attempt(identity, operation=self.operation) +
                    motion_terminal(identity, family="stop", operation=self.operation, axis_id=axis) + ended)
        return super().respond(identity, command)

    def read(self, size):
        if self.delay_read:
            self.delay_read = False
            self.clock.sleep(0.03)
        return super().read(size)


class PairedTests(unittest.TestCase):
    def run_fixture(self, directory, *, fail_stop=False, lose_move=False, lose_stop=False,
                    delay_second=False, refuse_first_locally=False, lose_selection=False):
        path = Path(directory) / "campaign.jsonl"
        campaign = bench.Campaign(path, True).load()
        port, clock = PairPort(), Clock()
        port.clock = clock
        console = probe.Console(port, clock=clock, sleeper=clock.sleep, first_id=1)
        console.identify(expected_version="0.1.0")
        original_begin = console.begin
        moves = 0
        def begin(command, **kwargs):
            nonlocal moves
            if command.startswith("move "):
                moves += 1
                if delay_second and moves == 2:
                    port.delay_read = True
            if command == "stop 101 fast" and refuse_first_locally:
                raise probe.BenchError("synthetic clean local first-stop refusal")
            return original_begin(command, **kwargs)
        console.begin = begin
        port.fail_first_stop, port.lose_second_move = fail_stop, lose_move
        port.lose_first_stop = lose_stop
        port.lose_second_selection = lose_selection
        def reserved():
            reloaded = bench.Campaign(path, False).load()
            self.assertEqual(reloaded.counts(), (4, {("M", 1): 1, ("S", 1): 1, ("M", 2): 1, ("S", 2): 1}))
        port.before_move = reserved
        record = dict(fixture=pair_fixture(), evidence_kind="host-tested")
        error = None
        try:
            paired.run_pair(console, record, axes=AXES, trial=53001, stop_kind="fast", stop_after_ms=20,
                            timeout_s=0.1, campaign=campaign)
        except probe.BenchError as failure:
            error = failure
        return console, port, clock, record, error

    def test_pair_precharges_all_effects_and_keeps_four_results(self):
        with tempfile.TemporaryDirectory() as directory:
            console, port, clock, record, error = self.run_fixture(directory)
        self.assertIsNone(error)
        commands = [data.decode().strip().split(" ", 1)[1] for data in port.writes]
        self.assertEqual(sum(text.startswith("move ") for text in commands), 2)
        self.assertEqual([text for text in commands if text.startswith("stop ")], ["stop 101 fast", "stop 102 fast"])
        self.assertEqual(len(console.operations), 4)
        self.assertTrue(record["workload_verified"])
        self.assertGreaterEqual(clock.now, record["paired_stop_cutoff_host_s"])
        self.assertFalse(record["synchronized_start_claimed"])
        self.assertTrue(record["paired_axes"]["101"]["move"]["uncertain"])
        self.assertEqual(record["paired_axes"]["102"]["stop"]["outcome"], "OBSERVED")

    def test_one_stop_refusal_does_not_prevent_the_other_handoff(self):
        with tempfile.TemporaryDirectory() as directory:
            _, port, _, record, error = self.run_fixture(directory, fail_stop=True)
        self.assertIsNotNone(error)
        self.assertEqual(sum(b" stop " in data for data in port.writes), 2)
        self.assertEqual(port.stop_times, [record["paired_stop_cutoff_host_s"]] * 2)
        self.assertEqual(record["paired_axes"]["102"]["stop"]["outcome"], "OBSERVED")

    def test_lost_admission_is_not_replayed_and_reservation_stays(self):
        with tempfile.TemporaryDirectory() as directory:
            console, port, _, record, error = self.run_fixture(directory, lose_move=True)
            self.assertEqual(bench.Campaign(Path(directory) / "campaign.jsonl", False).load().counts()[0], 4)
        self.assertIsNotNone(error)
        self.assertFalse(console.synchronized)
        self.assertEqual(sum(b" move " in data for data in port.writes), 2)
        # Unknown admission at the fixed cutoff still permits the two separately
        # planned stops on a clean stream. Neither uncertain move is replayed.
        self.assertEqual(sum(b" stop " in data for data in port.writes), 2)

    def test_lost_first_stop_does_not_delay_second_handoff_or_erase_result(self):
        with tempfile.TemporaryDirectory() as directory:
            console, port, _, record, error = self.run_fixture(directory, lose_stop=True)
        self.assertIsNotNone(error)
        self.assertFalse(console.synchronized)
        self.assertEqual(sum(b" stop " in data for data in port.writes), 2)
        self.assertEqual(record["paired_axes"]["102"]["stop"]["outcome"], "OBSERVED")
        self.assertFalse(record["paired_axes"]["101"]["stop"]["terminal_known"])

    def test_pre_handoff_drain_cannot_send_second_move_after_cutoff(self):
        with tempfile.TemporaryDirectory() as directory:
            _, port, _, record, error = self.run_fixture(directory, delay_second=True)
        self.assertIsNotNone(error)
        self.assertEqual(sum(b" move " in data for data in port.writes), 1)
        self.assertEqual(sum(b" stop " in data for data in port.writes), 2)
        self.assertTrue(all(when < record["paired_stop_cutoff_host_s"] for when, text in port.handoffs if text.startswith("move ")))
        self.assertEqual(record["paired_axes"]["102"]["stop"]["outcome"], "OBSERVED")

    def test_clean_local_first_stop_refusal_keeps_other_axis_stop(self):
        with tempfile.TemporaryDirectory() as directory:
            _, port, _, record, error = self.run_fixture(directory, refuse_first_locally=True)
        self.assertIsNotNone(error)
        self.assertEqual(sum(b" stop 102 fast" in data for data in port.writes), 1)
        self.assertIn("101", record["paired_stop_refusals"])
        self.assertEqual(record["paired_axes"]["102"]["stop"]["outcome"], "OBSERVED")

    def test_missing_second_selection_yields_at_due_without_poisoning_stops(self):
        with tempfile.TemporaryDirectory() as directory:
            console, port, clock, record, error = self.run_fixture(directory, lose_selection=True)
        self.assertIsNotNone(error)
        self.assertTrue(console.synchronized)
        self.assertEqual(clock(), record["paired_stop_cutoff_host_s"])
        self.assertEqual(sum(b" move " in data for data in port.writes), 1)
        self.assertEqual(port.stop_times, [record["paired_stop_cutoff_host_s"]] * 2)
        self.assertFalse(record["paired_axes"]["102"]["selection"]["terminal_known"])
        self.assertEqual(record["paired_axes"]["102"]["stop"]["outcome"], "OBSERVED")

    def test_reservation_caps_reloads_and_reused_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ledger"
            campaign = bench.Campaign(path, True).load()
            for trial in (1, 2, 3):
                campaign.reserve_pair(trial=trial, axes=AXES, stop_kind="fast", stop_after_ms=20)
            campaign = bench.Campaign(path, False).load()
            self.assertEqual(campaign.counts()[0], 12)
            before = path.read_bytes()
            for trial in (3, 4):
                with self.assertRaises(bench.Refused):
                    campaign.reserve_pair(trial=trial, axes=AXES, stop_kind="fast", stop_after_ms=20)
            self.assertEqual(path.read_bytes(), before)
            with self.assertRaises(bench.Refused):
                campaign.check("move", [1, 5, 1, 0, 5000, 30, 10000, 500, 2, 1500, 1500, 500, 0])

    def test_strict_pair_schema_and_partial_ledger_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ledger"
            campaign = bench.Campaign(path, True).load()
            campaign.reserve_pair(trial=1, axes=AXES, stop_kind="fast", stop_after_ms=20)
            good = json.loads(path.read_bytes())
            for changes in ({"motion": 0}, {"stop": 0}, {"trial": True}, {"extra": 1}):
                entry = dict(good, **changes)
                path.write_text(json.dumps(entry) + "\n", encoding="utf-8")
                with self.assertRaises(bench.Refused):
                    bench.Campaign(path, False).load()
            path.write_text(json.dumps(good), encoding="utf-8")
            with self.assertRaises(bench.Refused):
                bench.Campaign(path, False).load()

    def test_single_route_and_pair_share_counts_and_trial_order(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ledger"
            campaign = bench.Campaign(path, True).load()
            campaign.append(dict(event="send", route="S", node=1, trial=10, motion=1, stop=0,
                                 line=bench.sign_line("TS1,10,1,2,1500,1500,500")))
            campaign.reserve_pair(trial=11, axes=AXES, stop_kind="fast", stop_after_ms=20)
            reloaded = bench.Campaign(path, False).load()
            self.assertEqual(reloaded.counts()[0], 5)
            with self.assertRaises(bench.Refused):
                reloaded.check("stop", [1, 11])

    def test_trigger_skew_uses_exact_owned_enqueue_copies(self):
        rows = [dict(kind=1, accepted=True, can_id=0x601 + i, extended=False, rtr=False, dlc=8,
                     payload="2B4060001F000000", processed_us=str(9007199254740993 + i * 300)) for i in range(2)]
        self.assertEqual(paired.trigger_handoff_skew(rows, AXES)["skew_us"], "300")
        self.assertFalse(paired.trigger_handoff_skew(rows[:1], AXES)["observed"])
        self.assertFalse(paired.trigger_handoff_skew(rows + rows[:1], AXES)["observed"])

    def test_existing_session_writes_raw_artifacts_for_the_paired_callback(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            campaign = bench.Campaign(path / "campaign", True).load()
            clock, port = Clock(), PairPort()
            @contextmanager
            def opener(*_args):
                yield port
            def factory(connection, **kwargs):
                return probe.Console(connection, clock=clock, sleeper=clock.sleep, first_id=1, **kwargs)
            def run(console, record):
                paired.run_pair(console, record, axes=AXES, trial=53001, stop_kind="fast", stop_after_ms=20,
                                timeout_s=0.1, campaign=campaign)
            result = bench_session.run_recorded(port="HOST_FAKE", baud=115200, out=path / "pair", scenario="paired-position",
                inputs=dict(axes=AXES), run=run, profile="lichuan_c", node=1, can_bitrate=500000,
                expected_version="0.1.0", fixture=pair_fixture(), timeout_s=0.5, startup_s=0.01,
                port_opener=opener, console_factory=factory, clock=clock, evidence_kind="host-tested", trace_raw=True)
            self.assertTrue(result["ok"])
            for suffix in (".json", ".jsonl", ".zip"):
                self.assertGreater((path / ("pair" + suffix)).stat().st_size, 0)
            self.assertIn(b'"raw_hex"', (path / "pair.jsonl").read_bytes())


if __name__ == "__main__":
    unittest.main()
