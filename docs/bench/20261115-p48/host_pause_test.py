#!/usr/bin/env python3
"""Offline checks of the retained Prompt 48 experiment; never opens a port.

Run this file normally and with python -O. The experiment remains a local capture
helper, not another installed API or general HIL runner.
"""
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


SPEC = importlib.util.spec_from_file_location("p48_host_pause", Path(__file__).with_name("host_pause.py"))
experiment = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(experiment)

BASE = "bf8a0d9518c6b42ff06d4f29516bf82ee66abf79"
ELF = "9ca7701bb4f20efe9c383fd8fe4929e0314980bd2f2f1899652abcaa687b0399"


class Clock:
    def __init__(self):
        self.now = 0.0
        self.sleeps = []

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.sleeps.append((self.now, seconds))
        self.now += seconds


class Port:
    """Only p, one encoded S, and X are accepted by this stationary fake."""

    def __init__(self, clock):
        self.clock = clock
        self.pending = bytearray()
        self.received = bytearray()
        self.writes = []
        self.read_times = []
        self.short = None
        self.read_error = None
        self.close_error = None
        self.closed = 0
        self.base = BASE
        self.answer_end = True
        self.trial = None

    @property
    def in_waiting(self):
        return len(self.pending)

    def text(self, line):
        self.pending.extend((line + "\n").encode("ascii"))

    def write(self, data):
        self.writes.append((self.clock.now, bytes(data)))
        if self.short is not None and data.startswith(self.short):
            return len(data) - 1
        if data == b"p":
            self.text("PONG target=esp32s2 base=%s elf=%s sdk=v5.5.5-dirty running=1 active=1 "
                      "silent=0 pinsReady=1 soak=0 t=100" % (self.base, ELF))
        elif data.startswith(b"S"):
            self.trial, duration, interval, summary, reads = map(int, data[1:].strip().split(b","))
            if (duration, interval, summary, reads) != (30, 200, 1, 3):
                raise AssertionError("unexpected read plan")
            self.text("SOAK_BEGIN n=0 t=1000 trial=%d dur=30 interval=200 summary=1 monitors=1 "
                      "identity=1 opDeadline=2000000 queue=250000 resp=250000 stack=8000 "
                      "heap=24000 heapMin=24000 base=%s" % (self.trial, BASE))
        elif data == b"X":
            if self.trial is None:
                raise AssertionError("stop before admission")
            if self.answer_end:
                self.text("SOAK_STOPPING; a started read settles first")
                # This helper captures the END. The independent soak checker owns
                # summary completeness and must reject this intentionally short fake.
                self.text("SOAK_END n=1 t=20001000 trial=%d reason=stopped up=20000000 lines=1 settled=1"
                          % self.trial)
        else:
            raise AssertionError("unsupported write %r" % data)
        return len(data)

    def read(self, count):
        self.read_times.append(self.clock.now)
        self.clock.now += 0.05
        if self.read_error is not None and self.trial is not None and len(self.read_times) >= 3:
            raise self.read_error
        data = bytes(self.pending[:count])
        del self.pending[:count]
        self.received.extend(data)
        return data

    def close(self):
        self.closed += 1
        if self.close_error is not None:
            raise self.close_error


class Backend:
    def __init__(self, port):
        self.port = port
        self.opens = []
        self.enumerations = 0
        self.open_error = None

    def comports(self):
        self.enumerations += 1
        return [SimpleNamespace(device="COM15", serial_number="94A990398B40", vid=0x303A, pid=0x1002)]

    def open(self, device):
        self.opens.append(device)
        if self.open_error is not None:
            raise self.open_error
        return self.port


class HostPauseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.output = Path(self.temporary.name) / "capture"
        self.clock = Clock()
        self.port = Port(self.clock)
        self.backend = Backend(self.port)

    def run_helper(self, execute=True):
        argv = ["host_pause.py", "--output", str(self.output)] + (["--execute"] if execute else [])
        with mock.patch.object(experiment, "RealBackend", return_value=self.backend), \
                mock.patch.object(experiment.time, "monotonic", self.clock.monotonic), \
                mock.patch.object(experiment.time, "sleep", self.clock.sleep), \
                mock.patch.object(experiment.time, "time", return_value=1_800_000_000), \
                mock.patch.object(sys, "argv", argv), \
                mock.patch.dict(sys.modules, {"serial": None}), \
                mock.patch("sys.stdout", new_callable=io.StringIO):
            return experiment.main()

    def metadata(self):
        return json.loads((self.output / "request.json").read_text(encoding="utf-8"))

    def events(self):
        return [json.loads(line) for line in (self.output / "events.jsonl").read_text(encoding="utf-8").splitlines()]

    def test_execute_guard_touches_no_backend_or_output(self):
        self.assertEqual(self.run_helper(execute=False), 0)
        self.assertEqual(self.backend.enumerations, 0)
        self.assertEqual(self.backend.opens, [])
        self.assertFalse(self.output.exists())

    def test_one_plan_pause_and_early_stop_use_one_handle_without_replay(self):
        self.assertEqual(self.run_helper(), 0)
        self.assertEqual(self.backend.opens, ["COM15"])
        self.assertEqual([data for _, data in self.port.writes],
                         [b"p", experiment.soak_line(1_800_000_000, 30, 200, 1, 3), b"X"])
        self.assertEqual(self.port.closed, 1)
        self.assertEqual(len(self.clock.sleeps), 1)
        pause_at, pause_duration = self.clock.sleeps[0]
        self.assertEqual(pause_duration, 3)
        self.assertGreaterEqual(pause_at, 5)
        self.assertLess(pause_at, 5.2)
        self.assertFalse([when for when in self.port.read_times if pause_at <= when < pause_at + 3])
        stop_at = self.port.writes[-1][0]
        self.assertGreaterEqual(stop_at, 20)
        self.assertLess(stop_at, 20.2)
        meta = self.metadata()
        self.assertEqual(meta["status"], "captured")
        self.assertTrue(meta["pause_performed"] and meta["stop_sent"])
        self.assertEqual(meta["end"]["reason"], "stopped")
        self.assertEqual((self.output / "rx.bin").read_bytes(), self.port.received)
        self.assertEqual([row["event"] for row in self.events()].count("send_attempt"), 3)

    def test_wrong_image_is_retained_before_soak_is_sent(self):
        self.port.base = "0" * 40
        with self.assertRaisesRegex(RuntimeError, "prerequisite mismatch"):
            self.run_helper()
        self.assertEqual([data for _, data in self.port.writes], [b"p"])
        self.assertEqual(self.port.closed, 1)
        self.assertEqual(self.metadata()["status"], "failed")
        self.assertNotIn("tx_hex", self.metadata())
        self.assertEqual((self.output / "rx.bin").read_bytes(), self.port.received)

    def test_each_short_write_is_recorded_and_never_replayed(self):
        for prefix in (b"p", b"S", b"X"):
            with self.subTest(prefix=prefix):
                self.output = Path(self.temporary.name) / prefix.decode()
                self.clock = Clock()
                self.port = Port(self.clock)
                self.backend = Backend(self.port)
                self.port.short = prefix
                with self.assertRaisesRegex(RuntimeError, "Short write"):
                    self.run_helper()
                self.assertEqual(sum(data.startswith(prefix) for _, data in self.port.writes), 1)
                self.assertEqual(self.port.closed, 1)
                meta = self.metadata()
                self.assertEqual(meta["status"], "failed")
                self.assertFalse(meta["stop_sent"])
                completed = [row for row in self.events() if row["event"] == "send_completed"][-1]
                self.assertEqual(completed["count"], len(bytes.fromhex(completed["hex"])) - 1)

    def test_keyboard_interrupt_keeps_partial_capture_without_issuing_stop_or_reconnecting(self):
        self.port.read_error = KeyboardInterrupt("deliberate host interrupt")
        with self.assertRaises(KeyboardInterrupt):
            self.run_helper()
        self.assertEqual(len(self.port.writes), 2)
        self.assertEqual(self.backend.opens, ["COM15"])
        self.assertEqual(self.port.closed, 1)
        meta = self.metadata()
        self.assertEqual(meta["status"], "failed")
        self.assertIn("KeyboardInterrupt", meta["reason"])
        self.assertFalse(meta["stop_sent"])
        self.assertEqual((self.output / "rx.bin").read_bytes(), self.port.received)

    def test_open_failure_still_has_failed_request_metadata(self):
        self.backend.open_error = OSError("port unavailable")
        with self.assertRaisesRegex(OSError, "port unavailable"):
            self.run_helper()
        self.assertEqual(self.backend.opens, ["COM15"])
        self.assertEqual(self.port.writes, [])
        self.assertEqual(self.port.closed, 0)
        self.assertEqual(self.metadata()["status"], "failed")
        self.assertIn("port unavailable", self.metadata()["reason"])

    def test_close_failure_retains_captured_end_and_reports_failure(self):
        self.port.close_error = OSError("close failed")
        self.assertEqual(self.run_helper(), 1)
        meta = self.metadata()
        self.assertEqual(meta["status"], "failed")
        self.assertEqual(meta["close_error"], "close failed")
        self.assertEqual(meta["end"]["reason"], "stopped")
        self.assertEqual(self.port.closed, 1)

    def test_close_failure_does_not_replace_original_interrupt(self):
        self.port.read_error = KeyboardInterrupt("original interruption")
        self.port.close_error = OSError("secondary close failure")
        with self.assertRaises(KeyboardInterrupt):
            self.run_helper()
        meta = self.metadata()
        self.assertIn("original interruption", meta["reason"])
        self.assertEqual(meta["close_error"], "secondary close failure")

    def test_missing_end_stays_bounded_and_sends_stop_only_once(self):
        self.port.answer_end = False
        with self.assertRaisesRegex(TimeoutError, "Finite host deadline"):
            self.run_helper()
        self.assertGreaterEqual(self.clock.now, 55)
        self.assertLess(self.clock.now, 55.1)
        self.assertEqual([data for _, data in self.port.writes].count(b"X"), 1)
        self.assertEqual(self.backend.opens, ["COM15"])
        self.assertEqual(self.port.closed, 1)
        self.assertEqual(self.metadata()["status"], "failed")

    def test_existing_output_is_never_overwritten(self):
        self.output.mkdir()
        sentinel = self.output / "request.json"
        sentinel.write_text("prior capture", encoding="utf-8")
        with self.assertRaises(FileExistsError):
            self.run_helper()
        self.assertEqual(self.backend.opens, [])
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "prior capture")


if __name__ == "__main__":
    unittest.main()
