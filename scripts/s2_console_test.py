#!/usr/bin/env python3
"""s2_console.py against a fake console board, clock and USB list: no port, no pyserial, no process."""

import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import console_jsonl  # noqa: E402
import s2_console as console  # noqa: E402

SERIAL, PORT = console.FIXTURE["usb_serial"], console.FIXTURE["port"]
PASSIVE = console.PASSIVE.encode("ascii") + b"\n"
ROM = b"ESP-ROM:esp32s2-rc4-20191025\r\nBuild:Oct 25 2019\r\nrst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)\r\n"


def record(identity, record_type, command, **fields):
    """One line in ProbeConsoleText's envelope order."""
    body = {"schema": console_jsonl.SCHEMA, "protocol": 3, "profile": "lichuan_c", "type": record_type,
            "id": identity, "command": command}
    body.update(fields)
    return (json.dumps(body, separators=(",", ":")) + "\n").encode("ascii")


def reply(identity, command, ok=True, **fields):
    status = {"status": "OK"} if ok else {"status": "ILLEGAL_VALUE", "detail": 0, "error": "unknown_command"}
    return record(identity, "reply", command, ok=ok, **dict(status, **fields))


class Clock:
    def __init__(self):
        self.seconds = 0.0

    def utc(self):
        return "2026-11-11T20:00:00Z+%.2f" % self.seconds

    def monotonic(self):
        self.seconds += 0.01
        return self.seconds


class Board:
    """The console image as the runner sees it. A line is answered when its terminator arrives;
    faults change one request's answer so that each runner check meets a concrete case."""

    def __init__(self, clock, boot=ROM + PASSIVE, boots_on_reset=True, drop=(), duplicate=(), malformed=(),
                 oversize=(), unsolicited=(), late=(), swap=False, trace=False, dropped_counter=None, attach=True):
        self.clock = clock
        self.pending = bytearray(boot) if boots_on_reset else bytearray()
        self.scheduled = [] if boots_on_reset else [(0.0, bytes(boot))]
        self.drop, self.duplicate, self.malformed = set(drop), set(duplicate), set(malformed)
        self.oversize, self.unsolicited, self.late = set(oversize), set(unsolicited), set(late)
        self.swap, self.trace, self.attach = swap, trace, attach
        self.held = None
        self.writes = []
        self.signals = []
        self.line = bytearray()
        self.operation = 0
        self.dropped = 0 if dropped_counter is None else dropped_counter
        self.closed = False
        self._rts = False
        self.dtr = False

    # serial.Serial surface -------------------------------------------------------------------
    @property
    def rts(self):
        return self._rts

    @rts.setter
    def rts(self, value):
        self.signals.append(("rts", value))
        if self._rts and not value:  # EN released: the chip boots and prints its boot lines.
            self.at(0.05, bytes(self.pending))
            del self.pending[:]
        self._rts = value

    @property
    def in_waiting(self):
        return sum(len(data) for when, data in self.scheduled if when <= self.clock.seconds)

    def read(self, count):
        ready = [item for item in self.scheduled if item[0] <= self.clock.seconds]
        self.scheduled = [item for item in self.scheduled if item[0] > self.clock.seconds]
        data = b"".join(item[1] for item in sorted(ready, key=lambda item: item[0]))
        out, rest = data[:count], data[count:]
        if rest:
            self.scheduled.insert(0, (self.clock.seconds, rest))
        return out

    def write(self, data):
        self.writes.append(bytes(data))
        for byte in data:
            if byte in (10, 13):
                if self.line:
                    self.answer(bytes(self.line))
                self.line = bytearray()
            else:
                self.line.append(byte)
        return len(data)

    def close(self):
        self.closed = True

    # console behaviour ----------------------------------------------------------------------
    def at(self, delay, data):
        self.scheduled.append((self.clock.seconds + delay, data))

    def answer(self, line):
        words = line.decode("ascii", errors="replace").split()
        identity = 0
        if words and words[0].startswith("@") and words[0][1:].isdigit():
            identity = int(words[0][1:])
            words = words[1:]
        if not words or any(c < 32 or c > 126 for c in line):
            self.at(0.02, record(identity, "reply", "input", ok=False, status="ILLEGAL_VALUE", detail=0,
                                 error="invalid_line") if identity else b"input: rejected (invalid_line)\n")
            return
        name = words[0]
        if not identity:
            self.at(0.02, ("%s: OK human text\n" % name).encode("ascii"))
            return
        if identity in self.drop:
            self.dropped += 1
            return
        if identity in self.malformed:
            self.at(0.02, reply(identity, name)[:-12] + b"\n")
            return
        if identity in self.oversize:
            self.at(0.02, reply(identity, name, padding="x" * 520))
            return
        if name == "help":
            self.at(0.02, record(identity, "help", "version", implemented=True))
            self.at(0.03, record(identity, "help", "help", implemented=True))
            self.at(0.04, reply(identity, "help", result="complete"))
            return
        if name == "recover":
            self.operation += 1
            self.at(0.02, reply(identity, "recover", result="accepted", operation_id=self.operation))
            self.at(0.05, record(identity, "terminal", "recover", ok=False, status="PROTOCOL_ERROR", detail=2,
                                 outcome="FAILED", uncertain=False, command_id=identity,
                                 operation_id=self.operation, completed_us="1", abort_code=None))
            return
        if name == "release":
            ok = len(words) == 2 and words[1] == str(self.operation)
            self.at(0.02, reply(identity, "release", ok=ok))
            return
        if name == "drv":
            self.at(0.02, reply(identity, "drv", queued=0, active=0, retained=0, reservations=0, inputDropped=0,
                                outputRejected=0, consoleDropped=self.dropped))
            return
        answer = reply(identity, name, ok=name in ("version", "status", "config"))
        if identity in self.late:
            self.at(1.5, answer)
            return
        if self.swap:
            if self.held is None:
                self.held = answer
                return
            self.at(0.02, answer)
            self.at(0.03, self.held)
            self.held = None
            return
        self.at(0.02, answer)
        if identity in self.duplicate:
            self.at(0.03, answer)
        if identity in self.unsolicited:
            self.at(0.03, reply(identity + 1000, "version"))
        if self.trace:
            self.at(0.03, record(0, "trace", "debug", kind=1, can_id=0x701, dlc=1, accepted=True))


class Backend:
    def __init__(self, board, ports=None):
        self.board = board
        self.ports = ports if ports is not None else [
            SimpleNamespace(device=PORT, serial_number=SERIAL, vid=0x303A, pid=0x1002)]
        self.opened = []

    def comports(self):
        return self.ports

    def open(self, device):
        self.opened.append(device)
        return self.board


class Blocked:
    def __getattr__(self, name):
        raise AssertionError("dry run touched the backend: " + name)


class Fixture(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.output = self.base / "session"
        self.clock = Clock()
        patcher = mock.patch.object(console, "repository_state", return_value={"host_revision": "a" * 40,
                                                                                  "host_dirty": False})
        patcher.start()
        self.addCleanup(patcher.stop)
        sleeper = mock.patch("time.sleep", lambda seconds: None)
        sleeper.start()
        self.addCleanup(sleeper.stop)

    def script(self, text, name="script.txt"):
        path = self.base / name
        path.write_bytes(text if isinstance(text, bytes) else text.encode("ascii"))
        return path

    def run_console(self, board, script, *extra, backend=None, reset=True):
        argv = ["--port", PORT, "--serial", SERIAL, "--script", str(script), "--output", str(self.output),
                "--line-timeout", "2", *extra]
        if reset and "--no-boot-line" not in extra:
            argv.append("--reset-en")
        backend = backend or Backend(board)
        with mock.patch("sys.stdout", new_callable=io.StringIO) as out, \
                mock.patch("sys.stderr", new_callable=io.StringIO) as err:
            code = console.main(argv, backend=backend, clock=self.clock)
        self.text = out.getvalue() + err.getvalue()
        return code

    def summary(self):
        return json.loads((self.output / "summary.json").read_text(encoding="utf-8"))

    def request(self):
        return json.loads((self.output / "request.json").read_text(encoding="utf-8"))

    def timeline(self):
        return [json.loads(line) for line in (self.output / "timeline.jsonl").read_text(encoding="utf-8").splitlines()]

    def sent(self, board):
        return b"".join(board.writes)


class Framing(unittest.TestCase):
    """console.ConsoleInput against cases read from ProbeConsole::feed and parseConsoleLine."""

    def lines(self, data):
        return [(line["id"], line["command"], line["valid"]) for line in console.ConsoleInput().feed(data)]

    def test_terminators_and_empty_lines(self):
        self.assertEqual(self.lines(b"@1 version\n@2 status\r@3 config\r\n\n\r\n\r\r"),
                         [(1, "version", True), (2, "status", True), (3, "config", True)])
        self.assertEqual(self.lines(b"@4 health\n\r"), [(4, "health", True)])  # LF then CR: one empty line.
        self.assertEqual(self.lines(b"version\n"), [(0, "version", True)])
        self.assertEqual(self.lines(b"  \t@5\tversion  \n"), [(5, "version", True)])  # Separators only.

    def test_invalid_lines_keep_the_feed_prefix_id(self):
        self.assertEqual(self.lines(b"@9\n"), [(9, "input", False)])  # No command token.
        self.assertEqual(self.lines(b"@0 version\n"), [(0, "input", False)])
        self.assertEqual(self.lines(b"@4294967296 version\n"), [(0, "input", False)])
        self.assertEqual(self.lines(b"@4294967295 version\n"), [(4294967295, "version", True)])
        self.assertEqual(self.lines(b"@12345678901 version\n"), [(0, "input", False)])  # Eleven digits.
        self.assertEqual(self.lines(b"@7x version\n"), [(7, "x", True)][:0] + [(0, "input", False)])
        self.assertEqual(self.lines(b"@5\x80 version\n"), [(5, "input", False)])  # Bad byte after the ID.
        self.assertEqual(self.lines(b"@6 ver\x01sion\n"), [(6, "input", False)])
        self.assertEqual(self.lines(b" @8 \x80\n"), [(0, "input", False)])  # feed needs '@' first.
        self.assertEqual(self.lines(b"@3 " + b"a " * 24 + b"a\n"), [(3, "input", False)])  # 25 tokens.
        self.assertEqual(self.lines(b"@3 " + b"a " * 23 + b"a\n"), [(3, "a", True)])  # Exactly 24.

    def test_overlong_line_is_discarded_through_its_terminator(self):
        text = b"@11 " + b"x" * 251  # 255 bytes: accepted.
        self.assertEqual(self.lines(text + b"\n"), [(11, "x" * 251, True)])
        model = console.ConsoleInput()
        self.assertEqual(model.feed(text + b"y"), [])
        self.assertTrue(model.pending())
        result = model.feed(b"\n@12 version\n")
        self.assertEqual([(line["id"], line["command"], line["valid"]) for line in result],
                         [(11, "input", False), (12, "version", True)])
        self.assertTrue(result[0]["discarded"])

    def test_partial_line_continues_into_the_next_send(self):
        model = console.ConsoleInput()
        self.assertEqual(model.feed(b"@13 ver"), [])
        self.assertEqual([line["command"] for line in model.feed(b"sion\n")], ["version"])

    def test_id_rewrite(self):
        self.assertEqual(console.rewrite_id("@7 version", 41), "@41 version")
        self.assertEqual(console.rewrite_id("  @7\tstatus", 42), "  @42\tstatus")
        self.assertEqual(console.rewrite_id("version @7", 43), "version @7")
        self.assertEqual(console.rewrite_id("@7x version", 44), "@7x version")


class Records(unittest.TestCase):
    def test_valid_record_and_each_rule(self):
        good = reply(5, "version", product="MotorControl-CAN")
        self.assertEqual(console_jsonl.record_problems(good)[1], [])
        cases = {
            "oversize": reply(5, "version", padding="x" * 600),
            "unterminated": good[:-1],
            "CR in record": good[:-1] + b"\r\n",
            "NUL in record": good[:-2] + b"\x00}\n",
            "duplicate JSON key": good[:-2] + b',"id":6}\n',
            "non-finite": good[:-2] + b',"x":NaN}\n',
            "unparseable": b'{"schema":\n',
            "not a JSON object": b"[1,2]\n",
            "schema": good.replace(b"console/1", b"console/2"),
            "protocol": good.replace(b'"protocol":3', b'"protocol":3.0'),
            "profile": good.replace(b"lichuan_c", b"other"),
            "uint32": good.replace(b'"id":5', b'"id":true'),
            "only unsolicited traces": good.replace(b'"id":5', b'"id":0'),
        }
        for expected, line in cases.items():
            with self.subTest(expected=expected):
                problems = console_jsonl.record_problems(line)[1]
                self.assertTrue(any(expected in problem for problem in problems), problems)
        self.assertEqual(console_jsonl.record_problems(good.replace(b'"id":5', b'"id":4294967296'))[1][0],
                         "id 4294967296 is not a uint32 integer")
        trace = record(0, "trace", "debug", kind=1)
        self.assertEqual(console_jsonl.record_problems(trace)[1], [])
        exact = reply(1, "version", padding="x" * (511 - len(reply(1, "version", padding=""))))
        self.assertEqual(len(exact), 511)
        self.assertEqual(console_jsonl.record_problems(exact)[1], [])
        self.assertIn("oversize", console_jsonl.record_problems(exact[:-2] + b"x}\n")[1][0])


class Correlation(unittest.TestCase):
    def tracker(self):
        return console.Tracker()

    def add(self, tracker, data, t=0.0):
        return [tracker.add(line, "test", t) for line in console.ConsoleInput().feed(data)]

    def parse(self, line):
        return console_jsonl.parse_record(line)

    def test_reply_stream_and_terminal(self):
        tracker = self.tracker()
        version, = self.add(tracker, b"@1 version\n")
        listing, = self.add(tracker, b"@2 help\n")
        recover, = self.add(tracker, b"@3 recover\n")
        self.assertEqual(tracker.record(self.parse(reply(1, "version")), 0.1), "final")
        self.assertEqual(tracker.record(self.parse(record(2, "help", "version")), 0.2), "streamed")
        self.assertEqual(tracker.record(self.parse(reply(2, "help", result="complete")), 0.3), "final")
        self.assertEqual(tracker.record(self.parse(reply(3, "recover", result="accepted", operation_id=1)), 0.4),
                         "accepted")
        self.assertEqual(tracker.last_operation, 1)
        self.assertEqual(recover.state, "accepted")
        self.assertEqual(tracker.record(self.parse(record(3, "terminal", "recover", ok=False)), 0.5), "final")
        self.assertEqual([r.state for r in (version, listing, recover)], ["final"] * 3)
        self.assertEqual(round(recover.latency, 3), 0.5)
        self.assertEqual(tracker.counts["out_of_order"], 0)
        self.assertEqual(tracker.outstanding(), 0)

    def test_duplicate_unsolicited_trace_and_mismatch(self):
        tracker = self.tracker()
        self.add(tracker, b"@1 version\n")
        tracker.record(self.parse(reply(1, "version")), 0.1)
        self.assertEqual(tracker.record(self.parse(reply(1, "version")), 0.2), "duplicate")
        self.assertEqual(tracker.record(self.parse(reply(77, "version")), 0.2), "unsolicited")
        self.assertEqual(tracker.record(self.parse(record(0, "trace", "debug")), 0.2), "trace")
        self.add(tracker, b"@2 status\n")
        self.assertEqual(tracker.record(self.parse(reply(2, "config")), 0.3), "final")
        counts = tracker.counts
        self.assertEqual((counts["duplicate"], counts["unsolicited"], counts["trace"], counts["command_mismatch"]),
                         (1, 1, 1, 1))

    def test_reused_id_matches_by_command_and_state(self):
        tracker = self.tracker()
        recover, = self.add(tracker, b"@7 recover\n")
        tracker.record(self.parse(reply(7, "recover", result="accepted", operation_id=1)), 0.1)
        duplicate, = self.add(tracker, b"@7 version\n")  # The image answers duplicate_id.
        self.assertEqual(tracker.record(self.parse(reply(7, "version", ok=False)), 0.2), "final")
        self.assertEqual((recover.state, duplicate.state), ("accepted", "final"))
        self.assertEqual(tracker.record(self.parse(record(7, "terminal", "recover")), 0.3), "final")
        self.assertEqual(tracker.counts["duplicate"], 0)

    def test_out_of_order_ordinary_reply_but_not_control_or_stream(self):
        tracker = self.tracker()
        self.add(tracker, b"@1 version\n@2 status\n@3 cancel\n@4 help\n@5 config\n")
        tracker.record(self.parse(reply(3, "cancel", ok=False)), 0.1)  # Reserved queue: may overtake.
        tracker.record(self.parse(reply(2, "status")), 0.1)
        tracker.record(self.parse(reply(5, "config")), 0.15)
        tracker.record(self.parse(record(4, "help", "version")), 0.16)  # A listing starts a turn later.
        self.assertEqual(tracker.counts["out_of_order"], 0)
        tracker.record(self.parse(reply(1, "version")), 0.2)
        self.assertEqual(tracker.counts["out_of_order"], 1)

    def test_missing_then_late(self):
        tracker = self.tracker()
        request, = self.add(tracker, b"@1 version\n")
        self.assertEqual(tracker.expire(1.0, 2.0), [])
        self.assertEqual(tracker.expire(2.5, 2.0), [request])
        self.assertEqual(tracker.counts["missing"], 1)
        self.assertEqual(tracker.record(self.parse(reply(1, "version")), 3.0), "late")
        self.assertEqual((tracker.counts["missing"], tracker.counts["late"], request.state), (0, 1, "late"))

    def test_terminal_before_acceptance_is_out_of_order(self):
        tracker = self.tracker()
        self.add(tracker, b"@1 recover\n")
        self.assertEqual(tracker.record(self.parse(record(1, "terminal", "recover")), 0.1), "final")
        self.assertEqual(tracker.counts["out_of_order"], 1)

    def test_latency_statistics(self):
        stats = console.latency_stats([0.001 * n for n in range(1, 101)])
        self.assertEqual((stats["count"], stats["min"], stats["median"], stats["p99"], stats["max"]),
                         (100, 1.0, 50.5, 99.0, 100.0))
        self.assertEqual(console.latency_stats([])["median"], None)


class Session(Fixture):
    SCRIPT = ("# comment\n@1 version\n\n@2 help\n@3 recover\n@4 release {op}\nstatus\n"
              "!hex:40 35 20 76 65 72 73 69 6f 6e 0d 0a\n@6 config\n")

    def test_full_session_passes_and_retains_every_artifact(self):
        board = Board(self.clock)
        code = self.run_console(board, self.script(self.SCRIPT))
        self.assertEqual(code, 0, self.text)
        self.assertEqual(self.sent(board), b"@1 version\n@2 help\n@3 recover\n@4 release 1\nstatus\n"
                                           b"@5 version\r\n@6 config\n")
        self.assertEqual(board.signals, [("rts", True), ("rts", False)])
        summary = self.summary()
        self.assertEqual(summary["verdict"], "passed")
        counts = summary["counts"]
        self.assertEqual((counts["sent_json"], counts["sent_human"], counts["finals"], counts["accepted"],
                          counts["streamed"], counts["missing"]), (6, 1, 6, 1, 2, 0))
        self.assertEqual([r["state"] for r in summary["requests"]], ["final"] * 6)
        self.assertEqual(summary["requests"][2]["final"]["outcome"], "FAILED")
        self.assertEqual(summary["latency_ms"]["count"], 6)
        request = self.request()
        self.assertEqual(request["host_revision"], "a" * 40)
        self.assertEqual(request["startup_line"], console.PASSIVE)
        self.assertEqual(request["usb"]["serial"], SERIAL)
        self.assertEqual(request["script"]["sha256"], __import__("hashlib").sha256(
            self.SCRIPT.encode("ascii")).hexdigest())
        self.assertEqual(request["verdict"], "passed")
        self.assertTrue(request["started_utc"] and request["ended_utc"])
        rx = (self.output / "rx.bin").read_bytes()
        self.assertTrue(rx.startswith(ROM + PASSIVE))
        timeline = self.timeline()
        self.assertEqual(sum(entry["length"] for entry in timeline), len(rx))
        self.assertEqual([entry["class"] for entry in timeline if entry["phase"] == "boot"], ["boot"] * 4)
        human = [entry for entry in timeline if entry.get("class") == "human"]
        self.assertEqual([entry["text"] for entry in human], ["status: OK human text"])
        tx = [json.loads(line) for line in (self.output / "tx.jsonl").read_text(encoding="utf-8").splitlines()]
        self.assertEqual(b"".join(bytes.fromhex(entry["hex"]) for entry in tx), self.sent(board))
        self.assertEqual(tx[3]["text"], "@4 release 1\n")

    def test_crlf_script_is_sent_as_its_lines(self):
        board = Board(self.clock)
        code = self.run_console(board, self.script(b"@1 version\r\n# c\r\n@2 status\r\n"))
        self.assertEqual(code, 0, self.text)
        self.assertEqual(self.sent(board), b"@1 version\n@2 status\n")
        script = self.request()["script"]
        self.assertNotEqual(script["sha256"], script["lf_sha256"])

    def test_gap_and_burst_pacing(self):
        for option in (("--gap-ms", "100"), ("--burst",)):
            with self.subTest(option=option):
                self.setUp()
                board = Board(self.clock)
                code = self.run_console(board, self.script("@1 version\n@2 status\n@3 config\n"), *option)
                self.assertEqual(code, 0, self.text)
                self.assertEqual(self.summary()["counts"]["finals"], 3)
                if option == ("--burst",):
                    self.assertEqual(len(board.writes), 3)

    def test_op_without_an_accepted_operation_is_not_sent(self):
        board = Board(self.clock)
        code = self.run_console(board, self.script("@1 version\n@2 release {op}\n"))
        self.assertEqual(code, 1)
        self.assertEqual(self.sent(board), b"@1 version\n")
        self.assertEqual(self.summary()["failing"], {"script_lines_not_sent": 1})


class Faults(Fixture):
    SCRIPT = "@1 version\n@2 status\n@3 config\n"

    def failing(self, board, *extra, script=None):
        code = self.run_console(board, self.script(script or self.SCRIPT), *extra)
        self.assertEqual(code, 1, self.text)
        summary = self.summary()
        self.assertEqual(summary["verdict"], "failed")
        return summary

    def test_each_failure_is_counted(self):
        cases = ({"drop": {2}}, "missing"), ({"duplicate": {2}}, "duplicate"), ({"malformed": {2}}, "malformed"), \
                ({"oversize": {2}}, "oversize"), ({"unsolicited": {2}}, "unsolicited"), ({"late": {2}}, "late")
        for options, key in cases:
            with self.subTest(key=key):
                self.setUp()
                summary = self.failing(Board(self.clock, **options), "--line-timeout", "1")
                self.assertGreaterEqual(summary["counts"][key], 1)
                self.assertIn(key, summary["failing"])

    def test_out_of_order_burst(self):
        summary = self.failing(Board(self.clock, swap=True), "--burst", script="@1 version\n@2 status\n")
        self.assertEqual(summary["counts"]["out_of_order"], 1)

    def test_traces_are_counted_not_failed(self):
        code = self.run_console(Board(self.clock, trace=True), self.script(self.SCRIPT))
        self.assertEqual(code, 0, self.text)
        self.assertEqual(self.summary()["counts"]["trace"], 3)

    def test_restart_during_the_session_fails(self):
        board = Board(self.clock)
        original = board.answer

        def answer(line):
            original(line)
            if line.startswith(b"@2"):
                board.at(0.05, ROM + PASSIVE)
        board.answer = answer
        summary = self.failing(board)
        self.assertGreaterEqual(summary["counts"]["restart_lines"], 2)

    def test_burst_drops_pass_only_when_the_image_counted_them(self):
        summary = None
        code = self.run_console(Board(self.clock, drop={2}), self.script(self.SCRIPT), "--burst", "--account",
                                "--line-timeout", "1")
        self.assertEqual(code, 0, self.text)
        summary = self.summary()
        self.assertEqual(summary["missing_accounted"], {"missing": 1, "console_dropped_delta": 1})
        self.assertEqual(summary["account_delta"]["consoleDropped"], 1)
        self.setUp()
        board = Board(self.clock, drop={2})
        board.answer_drop = board.drop
        original = board.answer

        def uncounted(line):
            before = board.dropped
            original(line)
            board.dropped = before  # A loss the image did not count.
        board.answer = uncounted
        summary = self.failing(board, "--burst", "--account", "--line-timeout", "1")
        self.assertEqual(summary["failing"]["missing"], 1)
        self.assertIn("counted only 0 dropped", " ".join(summary["problems"]))
        # Without --burst a missing reply always fails, counted or not.
        self.setUp()
        summary = self.failing(Board(self.clock, drop={2}), "--account", "--line-timeout", "1")
        self.assertEqual(summary["failing"]["missing"], 1)

    def test_interrupt_writes_the_summary(self):
        board = Board(self.clock)
        reads = [0]
        original = board.read

        def read(count):
            reads[0] += 1
            if reads[0] > 400:
                raise KeyboardInterrupt
            return original(count)
        board.read = read
        code = self.run_console(board, self.script("@1 version\n" * 200))
        self.assertEqual(code, 1)
        summary = self.summary()
        self.assertEqual(summary["verdict"], "interrupted")
        self.assertTrue(board.closed)


class Refusals(Fixture):
    def refused(self, board, *extra, reason=None, backend=None, reset=True, created=True):
        code = self.run_console(board, self.script("@1 version\n"), *extra, backend=backend, reset=reset)
        self.assertEqual(code, 2, self.text)
        self.assertEqual(self.sent(board), b"" if "--no-boot-line" not in extra else console.ATTACH_PROBE)
        if reason:
            self.assertEqual(self.request()["reason"], reason)
            self.assertEqual(self.summary()["verdict"], "not_sent")
        self.assertEqual(self.output.exists(), created)

    def test_startup_lines_that_send_nothing(self):
        cases = ((b"", "startup_line_missed"),
                 (ROM + b"S2_BENCH ready protocol=8\n", "bench_image"),
                 (ROM + b"MotorControl-CAN: compile fixture; CAN disabled, configuration required.\n",
                  "not_the_inactive_configured_image"),
                 (ROM + PASSIVE + b"SMOKE ready elf=" + b"e" * 64 + b" idf=5.5.5 project=motorcontrol_can_probe\n",
                  "smoke_image"),
                 (ROM + PASSIVE + b"something\n", "output_after_startup"))
        for boot, reason in cases:
            with self.subTest(reason=reason):
                self.setUp()
                self.refused(Board(self.clock, boot=boot), "--boot-seconds", "1", reason=reason)

    def test_no_reset_waits_for_a_boot_while_open(self):
        self.refused(Board(self.clock), "--boot-seconds", "1", reason="startup_line_missed", reset=False)

    def test_identity_and_directory_refusals_open_nothing(self):
        board = Board(self.clock)
        for ports in ([], [SimpleNamespace(device=PORT, serial_number="OTHER", vid=0x303A, pid=0x1002)],
                      [SimpleNamespace(device=PORT, serial_number=SERIAL, vid=0x303A, pid=0x1001)]):
            backend = Backend(board, ports=ports)
            self.refused(board, backend=backend, created=False)
            self.assertEqual(backend.opened, [])
        self.output.mkdir()
        backend = Backend(board)
        self.refused(board, backend=backend)
        self.assertEqual(backend.opened, [])

    def test_only_the_recorded_port_and_serial(self):
        board = Board(self.clock)
        backend = Backend(board, ports=[SimpleNamespace(device="COM7", serial_number=SERIAL, vid=0x303A,
                                                        pid=0x1002)])
        argv = ["--port", "COM7", "--serial", SERIAL, "--script", str(self.script("@1 version\n")),
                "--output", str(self.output)]
        with mock.patch("sys.stderr", new_callable=io.StringIO) as err:
            self.assertEqual(console.main(argv, backend=backend, clock=self.clock), 2)
        self.assertIn("not the recorded COM15", err.getvalue())
        self.assertEqual(backend.opened, [])
        self.assertFalse(self.output.exists())

    def test_bad_scripts_and_records_are_refused_first(self):
        board = Board(self.clock)
        for text in (b"# only a comment\n", b"!hex:zz\n", b"@1 caf\xc3\xa9\n"):
            backend = Backend(board)
            argv = ["--port", PORT, "--serial", SERIAL, "--script", str(self.script(text)),
                    "--output", str(self.output)]
            with mock.patch("sys.stderr", new_callable=io.StringIO):
                self.assertEqual(console.main(argv, backend=backend, clock=self.clock), 2, text)
            self.assertEqual(backend.opened, [])
        artifact = self.base / "upload"
        artifact.mkdir()
        (artifact / "build.json").write_text(json.dumps({"app": {"name": "probe-idf-optak-smoke"},
                                                         "status": "flashed"}), encoding="utf-8")
        argv = ["--port", PORT, "--serial", SERIAL, "--script", str(self.script("@1 version\n")),
                "--output", str(self.output), "--artifact", str(artifact)]
        with mock.patch("sys.stderr", new_callable=io.StringIO) as err:
            self.assertEqual(console.main(argv, backend=Backend(board), clock=self.clock), 2)
        self.assertIn("not probe-idf-optak-console", err.getvalue())
        record = {"app": {"name": console.CONSOLE_APP}, "status": "flashed", "usb": {"usb_serial": SERIAL},
                  "app_elf": "motorcontrol_can_probe.elf", "image_sha256": {"motorcontrol_can_probe.elf": "f" * 64},
                  "base_commit": "b" * 40}
        (artifact / "build.json").write_text(json.dumps(record), encoding="utf-8")
        self.assertEqual(console.load_artifact(artifact, SERIAL)["elf"], "f" * 64)
        with self.assertRaises(console.Refused):
            console.load_artifact(artifact, "OTHER")

    def test_argument_conflicts(self):
        script = str(self.script("@1 version\n"))
        base = ["--port", PORT, "--serial", SERIAL, "--script", script, "--output", str(self.output)]
        with mock.patch("sys.stderr", new_callable=io.StringIO):
            for extra in (["--reset-en", "--no-boot-line"], ["--soak", "10", "--burst"], ["--gap-ms", "5", "--burst"],
                          ["--line-timeout", "0"], ["--soak", "0"], ["--first-id", "0"]):
                with self.assertRaises(SystemExit, msg=str(extra)):
                    console.arguments(base + extra)
            with self.assertRaises(SystemExit):
                console.arguments(base[:-2])

    def test_dry_run_touches_nothing(self):
        script = self.script("@1 version\n!hex:40 32 20 78\n!hex:0a\n")
        argv = ["--port", PORT, "--serial", SERIAL, "--script", str(script), "--dry-run"]
        with mock.patch("sys.stdout", new_callable=io.StringIO) as out, \
                mock.patch.dict(sys.modules, {"serial": None, "serial.tools": None}):
            self.assertEqual(console.main(argv, backend=Blocked(), clock=Blocked()), 0)
        self.assertIn("line 3: b'\\n' -> id=2 x", out.getvalue())
        self.assertIn("DRY RUN", out.getvalue())


class Attach(Fixture):
    def test_identity_probe_then_script(self):
        board = Board(self.clock, boot=b"", boots_on_reset=False)
        code = self.run_console(board, self.script("@1 version\n"), "--no-boot-line")
        self.assertEqual(code, 0, self.text)
        self.assertEqual(self.sent(board), console.ATTACH_PROBE + b"@1 version\n")
        request = self.request()
        self.assertEqual(request["identity_probe"]["reply"]["error"], "invalid_line")
        self.assertEqual(request["boot"], "attached without startup line (--no-boot-line)")
        self.assertEqual(request["reset"], False)
        self.assertEqual(board.signals, [])

    def test_no_console_reply_sends_only_the_probe(self):
        board = Board(self.clock, boot=b"", boots_on_reset=False)
        board.answer = lambda line: board.at(0.02, b"UNKNOWN command; no action\n")
        code = self.run_console(board, self.script("@1 version\n"), "--no-boot-line")
        self.assertEqual(code, 2)
        self.assertEqual(self.sent(board), console.ATTACH_PROBE)
        self.assertEqual(self.request()["reason"], "no_console_identity")


class Soak(Fixture):
    def test_rewrites_ids_and_writes_minute_lines(self):
        board = Board(self.clock)
        code = self.run_console(board, self.script("@1 version\n@1 status\n"), "--soak", "130", "--first-id", "100")
        self.assertEqual(code, 0, self.text)
        summary = self.summary()
        self.assertNotIn("requests", summary)  # A soak keeps aggregates only.
        sent = self.sent(board).decode("ascii").splitlines()
        ids = [int(line.split()[0][1:]) for line in sent]
        self.assertEqual(ids, list(range(100, 100 + len(ids))))
        self.assertEqual(summary["counts"]["finals"], len(ids))
        self.assertGreater(summary["iterations"], 2)
        minutes = [json.loads(line) for line in (self.output / "minutes.jsonl").read_text(encoding="utf-8").splitlines()]
        self.assertEqual([line["minute"] for line in minutes], [1, 2, 3])
        self.assertEqual([line["final"] for line in minutes], [False, False, True])
        self.assertEqual(sum(line["minute_counts"]["finals"] for line in minutes), len(ids))
        self.assertIn("soak minute 1:", self.text)

    def test_soak_counts_a_missing_reply(self):
        code = self.run_console(Board(self.clock, drop={105}), self.script("@1 version\n"), "--soak", "20",
                                "--first-id", "100", "--line-timeout", "1")
        self.assertEqual(code, 1)
        self.assertEqual(self.summary()["counts"]["missing"], 1)


class Scripts(unittest.TestCase):
    """The retained console scripts load, frame and keep away from CAN and motion as documented."""

    ROOT = Path(__file__).resolve().parents[1] / "scripts/console_scripts"

    def test_retained_scripts(self):
        for name in ("local_routes.txt", "soak.txt"):
            with self.subTest(name=name):
                script = console.load_script(self.ROOT / name)
                model = console.ConsoleInput()
                lines = []
                for entry in script["entries"]:
                    data = entry["raw"] if "raw" in entry else entry["text"].encode("ascii") + b"\n"
                    lines += model.feed(data)
                self.assertFalse(model.pending(), "the script must end every line it starts")
                words = [line["text"].split() for line in lines if line["valid"]]
                commands = {word[1] if word[0].startswith("@") else word[0] for word in words}
                # No route that could start the controller, change NMT state, save or reconfigure.
                for forbidden in ("nmt", "persistence", "communication", "discover", "capture-read", "monitor"):
                    self.assertNotIn(forbidden, commands)
                self.assertFalse(any(line["text"].startswith("G") for line in lines))
                if name == "soak.txt":
                    self.assertTrue(all("raw" not in entry for entry in script["entries"]))
                    self.assertTrue(all(line["id"] for line in lines))
                    self.assertLessEqual(commands, {"version", "status", "health", "stats", "memory", "caps",
                                                    "config", "axis", "help", "drv", "profile", "wiring"})


if __name__ == "__main__":
    unittest.main()
