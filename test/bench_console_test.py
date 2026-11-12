#!/usr/bin/env python3
"""Client success, resource and finite live-wrapper checks on injected host fixtures."""

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "test"))
import bench_probe as bench
from bench_console_adversarial_test import Clock, Port, admission, record, reply


def value(identity, *, operation=1, family="probe", index=0x1000, width=4, raw="4294967295"):
    unit = ("native-position-scale-unknown" if index == 0x6064 else
            "native-velocity-unit-unconfirmed" if index == 0x606C else
            "raw-bits" if index in (0x2100, 0x2101, 0x2102, 0x6041) else "raw-code")
    signed = None
    if index in (0x6061, 0x6064, 0x606C):
        integer = int(raw)
        signed = str(integer if integer < 1 << (8 * width - 1) else integer - (1 << (8 * width)))
    return record(identity, "value", family, operation_id=operation, index=index, subindex=0,
                  width=width, raw=raw, native_signed=signed, unit=unit, current=True, interpretation=None,
                  earliest_us="9007199254740993", latest_us="9007199254740994")


def terminal(identity, *, operation=1, family="probe", ok=True, abort_code=None):
    return record(identity, "terminal", family, command_id=identity, operation_id=operation,
                  ok=ok, status="OK" if ok else "PROTOCOL_ERROR", detail=0,
                  outcome="SUCCESS" if ok else "REPLY_ERROR", uncertain=False,
                  completed_us="9007199254740995", abort_code=abort_code)


class ClientContracts(unittest.TestCase):
    def client(self):
        port, clock = Port(), Clock()
        console = bench.Console(port, clock=clock, sleeper=clock.sleep, first_id=1,
                                host_epoch="client-contract")
        console.identify(expected_version="0.1.0")
        return console, port, clock

    def assert_no_more_io(self, console, port):
        writes = list(port.writes)
        with mock.patch.object(port, "read", side_effect=AssertionError("read after stream failure")), \
                mock.patch.object(port, "write", side_effect=AssertionError("write after stream failure")):
            with self.assertRaises(bench.BenchError):
                console.begin("probe 10", timeout_s=0.02)
            with self.assertRaises(bench.BenchError):
                console.pump()
        self.assertEqual(port.writes, writes)

    def test_successful_probe_retains_exact_values_without_releasing(self):
        console, port, _ = self.client()
        port.read_limit = 3
        port.handler = lambda identity, _: admission(identity) + value(identity) + terminal(identity)
        handle = console.begin("probe 10", timeout_s=0.02)
        result = console.wait(handle)
        self.assertTrue(result["ok"])
        self.assertEqual(handle.records[1]["raw"], "4294967295")
        self.assertEqual(handle.records[1]["earliest_us"], "9007199254740993")
        self.assertEqual(result["completed_us"], "9007199254740995")
        self.assertIs(console.operations[1], handle)
        self.assertFalse(handle.released)
        self.assertEqual(sum(b" probe " in data for data in port.writes), 1)
        self.assertFalse(any(b"release" in data for data in port.writes))

    def test_unsigned_native_abort_survives_terminal_and_summary(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admission(identity) + terminal(identity, ok=False, abort_code=0xFFFFFFFF)
        handle = console.begin("probe 10")
        result = console.wait(handle)
        self.assertEqual(result["abort_code"], 4294967295)
        summary = console.summary(handle)
        self.assertEqual(summary["abort_code"], 4294967295)
        self.assertEqual(summary["classification"], "native_rejection")
        self.assertFalse(summary["uncertain"])
        self.assertTrue(console.synchronized)

    def test_two_partial_interleaved_reads_keep_separate_values_and_retained_results(self):
        console, port, _ = self.client()
        port.handler = lambda *_: b""
        a = console.begin("read state status 10")
        b = console.begin("probe 10")
        port.buffer.extend(reply(a.id, "read", result="accepted", operation_id=1) + admission(b.id, 2) +
                           value(a.id, family="read", index=0x6041, width=2, raw="35") +
                           value(b.id, operation=2) + terminal(b.id, operation=2) +
                           value(a.id, family="read", index=0x6061, width=1, raw="1") +
                           terminal(a.id, family="read"))
        port.read_limit = 5
        self.assertTrue(console.wait(a)["ok"])
        self.assertTrue(console.wait(b)["ok"])
        self.assertEqual(set(console.operations), {1, 2})
        self.assertEqual([row["index"] for row in a.records if row["type"] == "value"], [0x6041, 0x6061])
        self.assertEqual([row["index"] for row in b.records if row["type"] == "value"], [0x1000])

    def test_successful_state_blocks_preserve_raw_shape_and_unknown_physical_units(self):
        # These are host protocol vectors, not observed drive state or scale.
        blocks = {"status": ((0x6041, 2, "35"), (0x6061, 1, "1")),
                  "monitors": ((0x2100, 2, "1"), (0x2101, 2, "32768"), (0x2102, 2, "7")),
                  "feedback": ((0x6064, 4, "2147483648"), (0x606C, 4, "4294967295"))}
        for block, fields in blocks.items():
            with self.subTest(block=block):
                console, port, _ = self.client()
                def response(identity, _):
                    return (reply(identity, "read", result="accepted", operation_id=1) +
                            b"".join(value(identity, family="read", index=index, width=width, raw=raw)
                                     for index, width, raw in fields) + terminal(identity, family="read"))
                port.handler = response
                handle = console.begin("read state " + block + " 10")
                self.assertTrue(console.wait(handle)["ok"])
                values = [row for row in handle.records if row["type"] == "value"]
                self.assertEqual([(row["index"], row["width"], row["raw"]) for row in values], list(fields))
                self.assertTrue(all(row["current"] and row["interpretation"] is None for row in values))
                self.assertEqual(console.summary(handle)["evidence_kind"], "host-tested")
                if block == "feedback":
                    self.assertEqual([row["native_signed"] for row in values], ["-2147483648", "-1"])
                    self.assertEqual([row["unit"] for row in values],
                                     ["native-position-scale-unknown", "native-velocity-unit-unconfirmed"])

    def test_result_inspection_and_explicit_release_preserve_original_evidence(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admission(identity) + value(identity) + terminal(identity)
        handle = console.begin("probe 10")
        original = console.wait(handle)
        summary = console.summary(handle)
        def inspect(identity, command):
            if command == "result 1":
                fields = {key: val for key, val in original.items()
                          if key not in ("schema", "protocol", "profile", "type", "id", "command")}
                return record(identity, "reply", "result", **fields)
            self.assertEqual(command, "release 1")
            return reply(identity, "release")
        port.handler = inspect
        self.assertEqual(console.command("result 1")["command_id"], handle.id)
        self.assertIn(1, console.operations)
        self.assertTrue(console.release(handle)["ok"])
        self.assertNotIn(1, console.operations)
        self.assertTrue(handle.released)
        self.assertEqual(console.summary(handle), summary)
        self.assertEqual(sum(b" release " in data for data in port.writes), 1)

    def test_unexpected_record_kind_and_noninteger_native_value_poison(self):
        for suffix in (lambda identity: record(identity, "finding", "probe", operation_id=1),
                       lambda identity: value(identity, raw=1.5)):
            with self.subTest(suffix=suffix):
                console, port, _ = self.client()
                port.handler = lambda identity, _: admission(identity) + suffix(identity)
                with self.assertRaises(bench.BenchError):
                    console.command("probe 10")
                self.assert_no_more_io(console, port)

    def test_deadline_equality_and_backward_clock_allow_no_new_io(self):
        for backward in (False, True):
            with self.subTest(backward=backward):
                console, port, clock = self.client()
                port.handler = lambda *_: b""
                handle = console.begin("probe 10", timeout_s=0.02)
                writes = list(port.writes)
                clock.now = -1 if backward else handle.deadline
                with self.assertRaises(bench.BenchError):
                    console.pump()
                self.assertEqual(port.writes, writes)
                self.assertFalse(console.synchronized)
                self.assertIsNone(handle.terminal)

    def test_pending_capacity_refuses_before_another_write(self):
        console, port, _ = self.client()
        port.handler = lambda *_: b""
        for _ in range(bench.MAX_PENDING):
            console.begin("status")
        writes = list(port.writes)
        with self.assertRaises(bench.BenchError):
            console.begin("status")
        self.assertEqual(port.writes, writes)
        self.assertEqual(len(console.pending), bench.MAX_PENDING)

    def test_request_id_exhaustion_and_command_budget_never_wrap_or_write(self):
        for first_id, budget in ((0xFFFFFFFF, 1024), (1, 1)):
            with self.subTest(first_id=first_id):
                port, clock = Port(), Clock()
                with mock.patch.object(bench, "MAX_COMMANDS", budget):
                    console = bench.Console(port, clock=clock, sleeper=clock.sleep,
                                            first_id=first_id, host_epoch="id-bound")
                    console.command("version")
                    writes = list(port.writes)
                    with self.assertRaises(bench.BenchError):
                        console.begin("version")
                    self.assertEqual(port.writes, writes)
                    self.assertEqual(len(writes), 1)

    def test_oversized_and_partial_invalid_lines_retain_bounded_raw_tail(self):
        for data in (b"x" * 512, b'{"schema":"broken"'):
            with self.subTest(length=len(data)):
                console, port, clock = self.client()
                port.handler = lambda *_: data
                handle = console.begin("probe 10", timeout_s=0.01)
                with self.assertRaises(bench.BenchError):
                    console.wait(handle)
                self.assertEqual(bytes(console.buffer), data)
                self.assertLessEqual(len(console.buffer), 512)
                self.assertLess(clock.now, 1)
                self.assert_no_more_io(console, port)

    def test_duplicate_keys_and_nonfinite_values_fail_even_under_python_optimized(self):
        code = '''
import sys
sys.path[:0] = sys.argv[1:3]
import bench_probe as bench
from bench_console_adversarial_test import Clock, Port
for payload in (b'{"id":1,"id":2}\\n', b'{"value":NaN}\\n', b'{"value":Infinity}\\n', b'{"value":1e999}\\n'):
    clock, port = Clock(), Port()
    console = bench.Console(port, clock=clock, sleeper=clock.sleep, first_id=1, host_epoch="optimized")
    port.handler = lambda *_: payload
    try:
        console.command("version")
    except bench.BenchError:
        if console.synchronized or not console.parse_failures:
            raise RuntimeError("bad JSON did not retain failure and poison stream")
    else:
        raise RuntimeError("bad JSON accepted under optimized Python")
'''
        child = subprocess.run([sys.executable, "-I", "-B", "-O", "-c", code,
                                str(ROOT / "scripts"), str(ROOT / "test")],
                               capture_output=True, text=True, timeout=15)
        self.assertEqual(child.returncode, 0, child.stderr)

    def test_record_limit_retains_admission_without_evicting_history(self):
        console, port, _ = self.client()
        port.handler = lambda identity, _: admission(identity) + value(identity)
        with mock.patch.object(bench, "MAX_RECORDS", 1):
            handle = console.begin("probe 10")
            with self.assertRaises(bench.BenchError):
                console.wait(handle)
        self.assertTrue(handle.accepted)
        self.assertEqual(len(handle.records), 1)
        self.assertEqual(handle.records[0]["result"], "accepted")

    def test_command_and_session_byte_budgets_are_independent(self):
        for name in ("MAX_COMMAND_INPUT", "MAX_SESSION_INPUT"):
            with self.subTest(name=name):
                console, port, _ = self.client()
                limit = 10 if name == "MAX_COMMAND_INPUT" else console.received + 10
                port.handler = lambda identity, _: admission(identity)
                with mock.patch.object(bench, name, limit):
                    handle = console.begin("probe 10")
                    with self.assertRaises(bench.BenchError):
                        console.wait(handle)
                self.assertIsNone(handle.terminal)
                self.assertFalse(console.synchronized)
                self.assert_no_more_io(console, port)

    def test_frozen_clock_has_finite_turn_budget(self):
        console, port, _ = self.client()
        console.sleep = lambda _: None
        port.handler = lambda *_: b""
        with mock.patch.object(bench, "MAX_TURNS", 4):
            handle = console.begin("probe 10")
            with self.assertRaises(bench.BenchError):
                console.wait(handle)
        self.assertLessEqual(handle.turns, 5)
        self.assertFalse(console.synchronized)

    def test_trace_flood_drops_old_raw_lines_without_consuming_command_records(self):
        console, port, _ = self.client()
        original_drops = console.raw_dropped
        port.buffer.extend(b"".join(record(0, "trace", "debug", ordinal=i, decoded=False)
                                    for i in range(80)))
        for _ in range(100):
            if not port.buffer:
                break
            console.pump()
        self.assertFalse(port.buffer)
        self.assertEqual(len(console.raw_lines), 64)
        self.assertGreater(console.raw_dropped, original_drops)
        self.assertEqual(json.loads(console.raw_lines[-1])["ordinal"], 79)
        self.assertFalse(console.pending)
        self.assertTrue(console.synchronized)

    def test_evidence_callback_failure_fences_all_later_io(self):
        for failure_event in ("intent", "line"):
            with self.subTest(failure_event=failure_event):
                console, port, _ = self.client()
                port.handler = lambda identity, _: admission(identity)
                def failed_event(event, **_):
                    if event == failure_event:
                        raise OSError("evidence sink full")
                console.emit = failed_event
                writes = list(port.writes)
                with self.assertRaises(bench.BenchError):
                    console.wait(console.begin("probe 10"))
                self.assertEqual(len(port.writes), len(writes) + int(failure_event == "line"))
                self.assert_no_more_io(console, port)


class TranscriptBounds(unittest.TestCase):
    def test_log_is_exclusive_and_records_hex_without_host_secrets(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            log = bench.EvidenceLog(path)
            log("line", raw_hex="1b5b324a", request_id=7)
            log.close()
            self.assertEqual(json.loads(path.read_text()), dict(event="line", raw_hex="1b5b324a", request_id=7))
            original = path.read_bytes()
            with self.assertRaises(FileExistsError):
                bench.EvidenceLog(path)
            self.assertEqual(path.read_bytes(), original)

    def test_log_event_limit_retains_final_interruption_notice(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            log = bench.EvidenceLog(path)
            try:
                for i in range(2047):
                    log("trace", ordinal=i)
                with self.assertRaises(bench.BenchError):
                    log("trace", ordinal=2047)
                for _ in range(3):
                    with self.assertRaises(bench.BenchError):
                        log("trace", ordinal=2048)
            finally:
                log.close()
            lines = path.read_text().splitlines()
            self.assertEqual(len(lines), 2048)
            self.assertEqual(json.loads(lines[-1]), dict(event="evidence_limit", status="interrupted"))

    def test_log_byte_and_single_event_limits_are_finite(self):
        for padding in ("x" * 16000, "x" * 16384):
            with self.subTest(length=len(padding)), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "bytes.jsonl"
                log = bench.EvidenceLog(path)
                try:
                    with self.assertRaises(bench.BenchError):
                        for _ in range(150):
                            log("raw", retained=padding)
                finally:
                    log.close()
                self.assertLessEqual(path.stat().st_size, 2 * 1024 * 1024)
                self.assertEqual(json.loads(path.read_text().splitlines()[-1])["event"], "evidence_limit")


class InjectedLive(unittest.TestCase):
    def run_fixture(self, outcome, *, release=False):
        """Use genuine live orchestration with a fake serial module and injected time."""
        clock = Clock()
        class SerialPort(Port):
            def __init__(self):
                super().__init__()
                self.selected = None
                self.handler = self.respond
                self.opened = self.closed = 0

            def open(self):
                self.opened += 1

            def close(self):
                self.closed += 1
                self.is_open = False

            def respond(self, identity, command):
                if command == "useaddr 10":
                    self.selected = 10
                    return reply(identity, "useaddr")
                if command == "config":
                    return reply(identity, "config", interface=0, bitrate_bits_per_second=500000,
                                 selected_node=self.selected, bus_traffic=False)
                if command == "probe 10":
                    response = admission(identity)
                    if outcome == "timeout":
                        return response
                    if outcome == "abort":
                        return response + terminal(identity, ok=False, abort_code=0xFFFFFFFF)
                    return response + value(identity) + terminal(identity)
                if command == "release 1":
                    return reply(identity, "release")
                return self.handshake(identity, command)
        port = SerialPort()
        serial = types.SimpleNamespace(VERSION="3.5", Serial=mock.Mock(return_value=port))
        console_type = bench.Console
        def console_factory(actual_port, **options):
            return console_type(actual_port, **options, clock=clock, sleeper=clock.sleep,
                                first_id=1, host_epoch="injected-live")
        with tempfile.TemporaryDirectory(prefix="CAN fake serial ") as directory:
            path = Path(directory) / "evidence.jsonl"
            args = bench.parser().parse_args(["--port", "FAKE", "--baud", "115200",
                "--profile", "lichuan_c", "--node", "10", "--can-bitrate", "500000",
                "--expected-library-version", "0.1.0", "--log", str(path), "--timeout", "0.02",
                "--startup", "0.001", "--request-timeout-ms", "10"] + (["--release"] if release else []))
            with mock.patch.dict(sys.modules, {"serial": serial}), mock.patch.object(bench, "Console", console_factory), redirect_stdout(io.StringIO()):
                code = bench.run_live(args)
            summary = json.loads(path.with_name(path.name + ".summary.json").read_text())
            events = [json.loads(line) for line in path.read_text().splitlines()]
        self.assertEqual((port.opened, port.closed), (1, 1))
        self.assertEqual(sum(b" probe " in item for item in port.writes), 1)
        self.assertFalse(any(b"recover" in item or b"stop" in item for item in port.writes))
        self.assertEqual(sum(b" release " in item for item in port.writes), int(release))
        self.assertTrue(any(item["event"] == "intent" for item in events))
        self.assertFalse(summary["hardware_qualified"])
        return code, summary

    def test_success_is_one_probe_without_automatic_release(self):
        code, summary = self.run_fixture("success")
        self.assertEqual(code, 0)
        self.assertEqual(summary["operation"]["classification"], "terminal")
        self.assertEqual(summary["operation"]["evidence_kind"], "live-observed")

    def test_lost_terminal_is_nonzero_without_retry_or_cleanup_traffic(self):
        code, summary = self.run_fixture("timeout")
        self.assertEqual(code, 1)
        self.assertEqual(summary["operation"]["classification"], "unknown_after_admission")
        self.assertIsNone(summary["operation"]["outcome"])

    def test_native_abort_is_nonzero_and_preserved_unsigned(self):
        code, summary = self.run_fixture("abort")
        self.assertEqual(code, 1)
        self.assertEqual(summary["operation"]["classification"], "native_rejection")
        self.assertEqual(summary["operation"]["abort_code"], 0xFFFFFFFF)

    def test_release_happens_only_when_explicitly_requested(self):
        code, summary = self.run_fixture("success", release=True)
        self.assertEqual(code, 0)
        self.assertEqual(summary["operation"]["outcome"], "SUCCESS")


if __name__ == "__main__":
    unittest.main()
