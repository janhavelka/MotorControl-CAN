#!/usr/bin/env python3
"""Actual Console/session campaigns over bounded independent byte-stream fixtures.

The adapter produces public console records only. It models neither CAN nor
physical motion; successful results remain host-tested evidence throughout.
"""
from contextlib import contextmanager
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "test"))

import bench_probe as probe
import bench_repeat as repeat
import bench_scenarios as scenarios
import bench_session as session
from bench_campaign_adversarial_test import CampaignPort, admitted, motion_terminal, stop_attempt
from bench_campaign_plans_test import POSITION, REPEAT, fixture
from bench_console_adversarial_test import Clock, record, reply


IDENTITY = ((0x1000, 0, 4), (0x1009, 0, 2), (0x100A, 0, 2),
            (0x1018, 0, 1), (0x1018, 1, 4), (0x1018, 2, 4),
            (0x1018, 3, 4), (0x2000, 0, 2), (0x2001, 0, 2))


def typed_value(identity, family, operation, index, subindex, width):
    return record(identity, "value", family, operation_id=operation,
                  index=index, subindex=subindex, width=width, raw="1",
                  native_signed=None, unit="raw-code", current=True, interpretation=None,
                  earliest_us="9007199254740993", latest_us="9007199254740994")


def prepared_value(identity, family, operation, field, number, *, relative=False):
    fields = dict(operation_id=operation, field=field, unit="steps" if field == "target" else "rpm",
                  native=str(number), exact=True, error=["0", "1", False])
    requested = [str(number), "0", "1", False]
    if field == "target":
        fields.update(relative=relative, endpoint_native=str(number), requested_endpoint=requested)
    else:
        fields["requested"] = requested
    return record(identity, "motion_value", family, **fields)


class SessionPort(CampaignPort):
    """Literal successful/partial records for the six finite public campaign forms."""

    def __init__(self):
        super().__init__()
        self.handler = self.respond
        self.selected = None
        self.operation = 0
        self.partial_settings = False
        self.noop_settings = False
        self.failed_move = False
        self.failed_stop = False

    def respond(self, identity, command):
        family = command.split()[0]
        if command == "caps":
            extra = b"".join(record(identity, "capability", name, implemented=True, owner=42,
                                    effect="host_configuration", completion="reply", reason="host fixture")
                              for name in ("accel", "decel"))
            extra += record(identity, "capability", "driver", implemented=True, owner=41,
                            effect="device_setting", completion="asynchronous", reason="host fixture")
            return extra + CampaignPort.handshake(identity, command)
        if command == "config":
            return reply(identity, "config", interface=0, bitrate_bits_per_second=500000,
                         selected_node=self.selected, bus_traffic=False)
        if command == "useaddr 7":
            self.selected = 7
            return reply(identity, "useaddr")
        if family in ("status", "health", "stats", "drv", "memory", "accel", "decel", "release"):
            return reply(identity, family)
        if family in ("probe", "read", "move", "stop", "velocity", "home", "driver"):
            self.operation += 1
            operation = self.operation
            head = admitted(identity, family, operation)
            if family in ("probe", "read"):
                objects = ((0x1000, 0, 4),) if family == "probe" else IDENTITY
                if family == "read" and not command.startswith("read identity "):
                    raise AssertionError("unaccepted active upload: " + command)
                values = b"".join(typed_value(identity, family, operation, *item) for item in objects)
                end = record(identity, "terminal", family, command_id=identity, operation_id=operation,
                             ok=True, status="OK", detail=0, outcome="SUCCESS", uncertain=False,
                             completed_us="9007199254740995", abort_code=None)
                return head + values + end
            if family == "stop":
                if self.failed_stop:
                    return head + motion_terminal(identity, family=family, operation=operation,
                                                   ok=False, uncertain=True)
                return head + stop_attempt(identity, operation=operation) + motion_terminal(
                    identity, family=family, operation=operation)
            if family == "driver":
                failed = self.partial_settings
                changed = 0 if self.noop_settings else 7
                return head + record(identity, "terminal", "driver", command_id=identity,
                    operation_id=operation, ok=not failed, status="PROTOCOL_ERROR" if failed else "OK",
                    detail=1 if failed else changed, outcome="SETTINGS_FAILED" if failed else "SETTINGS_READBACK_MATCHED",
                    uncertain=failed, completed_us="9007199254740995", abort_code=None,
                    confirmed_mask=1 if failed else changed, changed_mask=changed,
                    effects_possible=not self.noop_settings)
            if family == "move" and self.failed_move:
                return head + motion_terminal(identity, family=family, operation=operation,
                                               ok=False, uncertain=True)
            tokens = command.split()
            if family == "move":
                values = prepared_value(identity, family, operation, "target", int(tokens[2]),
                                        relative=tokens[1] == "relative")
                values += prepared_value(identity, family, operation, "speed", int(tokens[5]))
            elif family == "velocity":
                values = prepared_value(identity, family, operation, "speed", int(tokens[1]))
            else:
                values = prepared_value(identity, family, operation, "speed", int(tokens[2]))
                values += prepared_value(identity, family, operation, "creep_speed", int(tokens[3]))
            return head + values + motion_terminal(identity, family=family, operation=operation)
        return CampaignPort.handshake(identity, command)

    def commands(self):
        return [wire.decode("ascii").rstrip("\n").split(" ", 1)[1] for wire in self.writes]


class SessionTests(unittest.TestCase):
    def build(self, name="quick", options=()):
        return scenarios.build_scenario(scenarios.arguments(["--plan-only", "--scenario", name] + list(options)))

    def harness(self, prefix, scenario, *, port=None, supplied_fixture=True):
        clock, port = Clock(), port or SessionPort()
        closed = []

        @contextmanager
        def opener(_name, _baud, _timeout):
            try:
                yield port
            finally:
                closed.append(True)
                port.is_open = False

        def factory(stream, **kwargs):
            return probe.Console(stream, clock=clock, sleeper=clock.sleep, first_id=1,
                                 host_epoch="session-functional", **kwargs)

        def execute():
            return session.run_recorded(port="host-fixture", baud=115200, out=prefix,
                scenario=scenario.name, inputs=scenario.inputs, run=scenario.run,
                profile="lichuan_c", node=7, can_bitrate=500000, expected_version="0.1.0",
                fixture=fixture() if supplied_fixture else None, timeout_s=15, startup_s=.002,
                port_opener=opener, console_factory=factory, clock=clock, evidence_kind="host-tested")
        return execute, port, closed

    def assert_public_traffic(self, port):
        commands = port.commands()
        active = [command for command in commands if command.startswith(("probe ", "read "))]
        self.assertEqual(active[0], "probe 1000")
        self.assertTrue(all(command == "probe 1000" or command == "read identity 1000" for command in active))
        self.assertFalse(any(command.startswith(("enable", "recover", "control", "io set", "persistence",
                                                  "motion-profile", "result ")) for command in commands))

    def test_all_six_named_campaigns_use_actual_parser_and_session(self):
        cases = (("quick", (), 0), ("position", POSITION, 1),
                 ("stop", ["--stop-policy", "fast"], 1),
                 ("velocity", ["--rpm", "60", "--duration-ms", "1000", "--stop-policy", "fast"], 0),
                 ("homing", ["--method", "17", "--search-rpm", "60", "--return-rpm", "30",
                             "--home-ramp-ms", "100", "--stop-policy", "fast"], 1),
                 ("settings", ["--driver-settings", "60", "100", "200"], 0))
        for name, options, stops in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                prefix = Path(directory) / name
                execute, port, closed = self.harness(prefix, self.build(name, options), supplied_fixture=name != "quick")
                report = execute()
                self.assertTrue(report["ok"])
                self.assertEqual(report["verdict"], "host-tested")
                self.assertFalse(report["hardware_qualified"])
                self.assertEqual(closed, [True])
                self.assertEqual(sum(command.startswith("stop ") for command in port.commands()), stops)
                self.assert_public_traffic(port)
                self.assertTrue(prefix.with_suffix(".zip").is_file())

    def test_repeat_uses_unique_operations_and_one_cleanup_through_actual_parser(self):
        scenario = repeat.build_scenario(repeat.arguments(["--plan-only", "--count", "3"] + REPEAT))
        with tempfile.TemporaryDirectory() as directory:
            execute, port, _ = self.harness(Path(directory) / "repeat", scenario)
            report = execute()
            self.assertTrue(report["ok"])
            self.assertEqual(report["comparison"], {"write": 3, "verify": 3})
            identities = [trial["terminal"]["operation_id"] for trial in report["trials"]]
            self.assertEqual(identities, sorted(set(identities)))
            self.assertEqual(len(identities), 6)
            self.assertEqual(port.commands().count("stop fast"), 1)
            self.assertEqual(len(report["cleanup_stop_attempts"]), 1)
            self.assert_public_traffic(port)

    def test_uncertain_move_keeps_failure_after_confirmed_cleanup(self):
        port = SessionPort()
        port.failed_move = True
        scenario = self.build("position", POSITION)
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "uncertain"
            execute, port, _ = self.harness(prefix, scenario, port=port)
            with self.assertRaises(probe.BenchError):
                execute()
            report = json.loads(prefix.with_suffix(".json").read_text())
            self.assertFalse(report["ok"])
            self.assertTrue(report["primary_failure"]["uncertain"])
            self.assertEqual(report["cleanup_stop_attempts"][0]["outcome"], "OBSERVED")
            self.assertEqual(port.commands().count(scenario.commands[-1]), 1)
            self.assertEqual(port.commands().count("stop fast"), 1)
            primary = report["primary_failure"]
            cleanup = report["cleanup_stop_attempts"][0]
            self.assertNotEqual(primary["request_id"], cleanup["request_id"])
            self.assertNotEqual(primary["operation_id"], cleanup["operation_id"])
            events = [json.loads(line) for line in prefix.with_suffix(".jsonl").read_text().splitlines()]
            records = [json.loads(bytes.fromhex(event["raw_hex"]))
                       for event in events if event["event"] == "line"]
            for operation in (primary, cleanup):
                terminal = next(row for row in records if row["type"] == "terminal"
                                and row["operation_id"] == operation["operation_id"])
                self.assertEqual(terminal["id"], operation["request_id"])
                self.assertEqual(terminal, operation["records"][-1])
            self.assertNotIn(f"release {primary['operation_id']}", port.commands())
            self.assertEqual(port.commands().count(f"release {cleanup['operation_id']}"), 1)
            with zipfile.ZipFile(prefix.with_suffix(".zip")) as archive:
                self.assertEqual(json.loads(archive.read("summary.json"))["primary_failure"], primary)
                self.assertEqual(archive.read("transcript.jsonl"), prefix.with_suffix(".jsonl").read_bytes())

    def test_failed_cleanup_preserves_move_terminal_and_failed_session(self):
        port = SessionPort()
        port.failed_stop = True
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "cleanup"
            execute, port, _ = self.harness(prefix, self.build("position", POSITION), port=port)
            with self.assertRaises(probe.BenchError):
                execute()
            report = json.loads(prefix.with_suffix(".json").read_text())
            self.assertFalse(report["ok"])
            self.assertEqual(report["cleanup_stop_attempts"][0]["classification"], "unknown_after_admission")
            self.assertEqual(port.commands().count("stop fast"), 1)
            moves = [operation for operation in report["operations"] if operation["intent"].startswith("move ")]
            self.assertEqual(moves[0]["outcome"], "OBSERVED")

    def test_partial_settings_effects_are_retained_without_restore_or_replay(self):
        port = SessionPort()
        port.partial_settings = True
        scenario = self.build("settings", ["--driver-settings", "60", "100", "200"])
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "partial"
            execute, port, _ = self.harness(prefix, scenario, port=port)
            with self.assertRaises(probe.BenchError):
                execute()
            report = json.loads(prefix.with_suffix(".json").read_text())
            failure = report["primary_failure"]
            self.assertEqual(failure["records"][-1]["confirmed_mask"], 1)
            self.assertEqual(failure["records"][-1]["changed_mask"], 7)
            self.assertTrue(failure["records"][-1]["effects_possible"])
            self.assertTrue(failure["uncertain"])
            self.assertEqual(port.commands().count("driver set 60 100 200"), 1)
            self.assertFalse(any(command.startswith("stop ") for command in port.commands()))
            self.assert_public_traffic(port)

    def test_noop_settings_zero_masks_are_valid_without_invented_writes(self):
        port = SessionPort()
        port.noop_settings = True
        with tempfile.TemporaryDirectory() as directory:
            execute, _, _ = self.harness(Path(directory) / "noop", self.build("settings", ["--driver-settings", "0", "0", "0"]), port=port)
            report = execute()
            operation = next(item for item in report["operations"] if item["intent"].startswith("driver set"))
            terminal = operation["records"][-1]
            self.assertEqual((terminal["confirmed_mask"], terminal["changed_mask"]), (0, 0))
            self.assertFalse(terminal["effects_possible"])
            self.assertTrue(report["ok"])

    def test_report_package_is_deterministic_allowlisted_and_hashed(self):
        payloads = (b'{"ok":false,"exact":"9007199254740993"}\n', b'{"event":"raw","bytes":"00ff"}\n')
        archives = []
        for _ in range(2):
            output = io.BytesIO()
            session.package_report(io.BytesIO(payloads[0]), io.BytesIO(payloads[1]), output)
            archives.append(output.getvalue())
        self.assertEqual(archives[0], archives[1])
        with zipfile.ZipFile(io.BytesIO(archives[0])) as bundle:
            self.assertEqual(bundle.namelist(), ["manifest.json", "summary.json", "transcript.jsonl"])
            manifest = json.loads(bundle.read("manifest.json"))
            for name, payload in zip(("summary.json", "transcript.jsonl"), payloads):
                self.assertEqual(bundle.read(name), payload)
                self.assertEqual(manifest[name], dict(bytes=len(payload), sha256=hashlib.sha256(payload).hexdigest()))

    def test_package_disk_failure_changes_success_to_failed_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "disk-full"
            execute, _, closed = self.harness(prefix, self.build(), supplied_fixture=False)
            disk_error = OSError("report disk full")
            with patch.object(session, "package_report", side_effect=disk_error):
                with self.assertRaises(OSError) as caught:
                    execute()
            self.assertIs(caught.exception, disk_error)
            self.assertEqual(closed, [True])
            report = json.loads(prefix.with_suffix(".json").read_text())
            self.assertFalse(report["ok"])
            self.assertEqual(report["verdict"], "failed")
            self.assertEqual(report["artifact_error"], "report disk full")

    def test_package_failure_cannot_mask_original_uncertain_primary(self):
        port = SessionPort()
        port.partial_settings = True
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "double-failure"
            execute, _, _ = self.harness(prefix, self.build("settings", ["--driver-settings", "60", "100", "200"]), port=port)
            with patch.object(session, "package_report", side_effect=OSError("separate disk failure")):
                with self.assertRaises(probe.BenchError) as caught:
                    execute()
            self.assertIn("SETTINGS_FAILED", str(caught.exception))
            report = json.loads(prefix.with_suffix(".json").read_text())
            self.assertFalse(report["ok"])
            self.assertIn("SETTINGS_FAILED", report["primary_error"])
            self.assertEqual(report["artifact_error"], "separate disk failure")

    def test_summary_close_and_reopen_failure_still_invalidates_passing_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "summary-close-reopen"
            execute, _, closed = self.harness(prefix, self.build(), supplied_fixture=False)
            summary_path = prefix.with_suffix(".json")
            original_open = Path.open
            close_error = OSError("summary owner close failed")

            class CloseFailure:
                def __init__(self, stream):
                    self.stream = stream

                def __getattr__(self, name):
                    return getattr(self.stream, name)

                def close(self):
                    self.stream.close()
                    raise close_error

            def failing_open(path, mode="r", *args, **kwargs):
                if path == summary_path and mode == "r+b":
                    raise OSError("failed summary cannot be reopened")
                stream = original_open(path, mode, *args, **kwargs)
                return CloseFailure(stream) if path == summary_path and mode == "x+b" else stream

            with patch.object(Path, "open", new=failing_open):
                with self.assertRaises(OSError) as caught:
                    execute()
            self.assertIs(caught.exception, close_error)
            self.assertEqual(closed, [True])
            # A summary recovery failure must not skip invalidating a separately
            # writable ZIP that still contains the earlier successful verdict.
            archive_path = prefix.with_suffix(".zip")
            if zipfile.is_zipfile(archive_path):
                with zipfile.ZipFile(archive_path) as archive:
                    self.assertFalse(json.loads(archive.read("summary.json"))["ok"])
            self.assertTrue(prefix.with_suffix(".jsonl").stat().st_size)

    def test_evidence_write_failure_after_handoff_persists_unknown_and_forbids_cleanup(self):
        original_log = probe.EvidenceLog.__call__

        def fail_move_line(log, event, **fields):
            raw = bytes.fromhex(fields["raw_hex"]) if event == "line" else b""
            if b'"command":"move"' in raw and b'"result":"accepted"' in raw:
                raise OSError("transcript storage failed during move")
            return original_log(log, event, **fields)

        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "transcript-failure"
            scenario = self.build("position", POSITION)
            execute, port, closed = self.harness(prefix, scenario)
            with patch.object(probe.EvidenceLog, "__call__", fail_move_line):
                with self.assertRaises(probe.BenchError):
                    execute()
            self.assertEqual(closed, [True])
            report = json.loads(prefix.with_suffix(".json").read_text())
            self.assertFalse(report["ok"])
            self.assertEqual(report["primary_failure"]["classification"], "unknown_after_handoff")
            self.assertFalse(report["client"]["synchronized"])
            self.assertEqual(port.commands().count(scenario.commands[-1]), 1)
            self.assertFalse(any(command.startswith("stop ") for command in port.commands()))
            self.assertFalse(report["cleanup_stop_attempts"][0]["attempted"])

    def test_every_entry_import_help_and_plan_runs_without_optional_serial(self):
        names = ("bench_probe", "bench_motion", "bench_repeat", "bench_scenarios", "bench_session")
        guard = ("import sys; sys.path.insert(0, " + repr(str(ROOT / "scripts")) + "); "
                 "sys.modules['serial']=None; ")
        for name in names:
            with self.subTest(name=name, mode="import"):
                result = subprocess.run([sys.executable, "-S", "-c", guard + "import " + name],
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
            for mode in ("--help", "--plan-only"):
                extra = REPEAT if name == "bench_repeat" and mode == "--plan-only" else []
                code = guard + "import runpy; sys.argv=" + repr([name + ".py", mode] + extra) + "; runpy.run_module(" + repr(name) + ", run_name='__main__')"
                with self.subTest(name=name, mode=mode):
                    result = subprocess.run([sys.executable, "-S", "-c", code],
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
