#!/usr/bin/env python3
"""s2_bench.py trial routes against a fake board, clock and operator: no port, no pyserial."""

import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_bench as bench  # noqa: E402
import s2_evidence as evidence  # noqa: E402

PONG = "PONG target=esp32s2 base=%s elf=%s sdk=v5.5.5 running=1 active=%d silent=0 pinsReady=1 soak=%d t=5%s\n"
NAMES = {"A": "readiness-actions", "I": "readiness-io", "U": "readiness-tuning", "M": "mode1", "E": "enable",
         "S": "stop", "P": "position"}
ROUTES = {"A": "actions", "I": "io", "U": "tuning", "M": "mode1", "E": "enable", "S": "stop", "P": "position"}


def check_trial_transcript(path):
    """Every record the real BenchRoute prints must parse whole, with its expected verdict."""
    data = Path(path).read_bytes()
    problems = []
    parts = data.split(b"=== ")[1:]
    if len(parts) < 7:
        problems.append("expected seven records, found %d" % len(parts))
    for part in parts:
        head, _, body = part.partition(b"\n")
        line = head.decode("ascii")
        fields = line[1:].split(",")
        meta = {"command": NAMES[line[0]], "node": int(fields[0]), "trial": int(fields[1]), "line": "T" + line,
                "status": "captured"}
        report = evidence.replay_bytes(body, meta)
        wanted = "failed-evidence" if line.startswith("S1,7,") else "complete"
        if report.verdict != wanted or report.family != "TRIAL":
            problems.append("%s: %s %s %s" % (line, report.verdict, report.family, report.errors[:3]))
        if report.unknown or report.malformed or report.unknown_fields:
            problems.append("%s: unknown %s malformed %s fields %s" % (
                line, report.unknown[:2], report.malformed[:2], report.unknown_fields[:2]))
        kinds = set(report.kinds)
        for kind in ("ADMITTED", "TRIAL", "TRIAL_PLAN", "TRIAL_RES", "END"):
            if kind not in kinds:
                problems.append("%s: no %s line" % (line, kind))
        if line.startswith("E") and not {"TRIAL_ENABLE", "TRIAL_ENABLE_STEP"} <= kinds:
            problems.append("%s: no enable summary" % line)
        if line.startswith("P") and not {"TRIAL_CHECK", "TRIAL_STOP", "TRIAL_BEST_EFFORT"} <= kinds:
            problems.append("%s: no check or stop summary" % line)
        if line[0] in "AIU" and not report.inventory_checked:
            problems.append("%s: fixed inventory not checked" % line)
    return problems


def record(route, node, trial, failure=0, why="none"):
    """A minimal consistent trial record in firmware format, with no terminals."""
    return ("TRIAL admitted route=%s node=%d trial=%d motion=1 case=1; finite typed route, no retry\n"
            "TRIAL id=%d node=%d route=%s failure=%d why=%s lost=0 terminals=0 traffic=0 tail=0 tailLost=0 "
            "summaries=2 step=1 cycles=0 motion=1 case=1 unsettled=0 gap=1000\n"
            "TRIAL_PLAN route=%s node=%d trial=%d attended=1\n"
            "TRIAL_RES stack0=9300 stack1=9100 heap0=60000 heap1=59000 heapMin=58000 largest=40000 gap=1000 "
            "dropped=0 admission=0 started=10 completed=20\n"
            "END trial terminals=0 traffic=0 tail=0 tailLost=0 lost=0; r=release\n") % (
        route, node, trial, trial, node, route, failure, why, route, node, trial)


class Clock:
    def __init__(self):
        self.seconds = 0.0

    def utc(self):
        return "2026-11-03T06:00:%06.3fZ" % self.seconds

    def monotonic(self):
        self.seconds += 0.01
        return self.seconds


class Item:
    def __init__(self, device="COM15", serial="94A990398B40", vid=0x303A, pid=0x1002):
        self.device, self.serial_number, self.vid, self.pid = device, serial, vid, pid


class Board:
    """The firmware console as the runner sees it."""

    def __init__(self):
        self.writes = []
        self.queue = bytearray(b"stale\n")
        self.routes = 1
        self.soak = 0
        self.trial = 0
        self.active = 1
        self.answer = None  # Text for the T line; default a complete record.
        self.closed = False

    @property
    def in_waiting(self):
        return len(self.queue)

    def read(self, count):
        out = bytes(self.queue[:count])
        del self.queue[:count]
        return out

    def write(self, data):
        self.writes.append(bytes(data))
        if data == b"p":
            routes = " routes=%d trial=%d motion=0" % (self.routes, self.trial) if self.routes else ""
            self.queue.extend((PONG % ("a" * 40, "b" * 64, self.active, self.soak, routes)).encode("ascii"))
        elif data.startswith(b"T"):
            text = data.decode("ascii").strip()
            fields = text[2:].split(",")
            body = self.answer if self.answer is not None else record(ROUTES[text[1]], int(fields[0]), int(fields[1]))
            self.queue.extend(body.encode("ascii"))
        elif data == b"A":
            self.queue.extend(b"TRIAL_ABORTING; local only, no stop sent; a request on its way settles first\n")
        return len(data)

    def close(self):
        self.closed = True


class Backend:
    def __init__(self, board, ports=None):
        self.board = board
        self.ports = ports if ports is not None else [Item()]
        self.opened = []

    def comports(self):
        return self.ports

    def open(self, device):
        self.opened.append(device)
        return self.board


class Untouchable:
    """Any use of the port layer fails the test."""

    def comports(self):
        raise AssertionError("dry run enumerated ports")

    def open(self, device):
        raise AssertionError("dry run opened %s" % device)


class Operator:
    def __init__(self, *answers):
        self.answers = list(answers)
        self.questions = []

    def __call__(self, question):
        self.questions.append(question)
        if not self.answers:
            raise EOFError
        return self.answers.pop(0)


BASE = ["--port", "COM15", "--serial", "94A990398B40"]
ENABLE = ["--command", "enable", "--node", "1", "--trial", "12", "--status", "0x8037", "--position-target", "5000",
          "--velocity-target", "120", "--deadline-ms", "10000", "--interval-ms", "800", "--no-motion-write-recorded"]


class BenchTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.output = Path(self.directory.name) / "capture"

    def tearDown(self):
        self.directory.cleanup()

    def run_main(self, argv, backend, operator=None):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = bench.main(BASE + argv + ["--output", str(self.output)], backend, Clock(),
                                operator or Operator())
        return status, out.getvalue(), err.getvalue()

    def meta(self):
        return json.loads((self.output / "request.json").read_text(encoding="utf-8"))

    def test_dry_run_touches_nothing(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = bench.main(BASE + ENABLE + ["--dry-run"], Untouchable(), Clock(), Operator())
        self.assertEqual(status, 0)
        self.assertIn("TE1,12,1,1,0,32823,5000,120,10000,800", out.getvalue())
        self.assertIn("ATTENDED 12", out.getvalue())
        self.assertFalse(self.output.exists())

    def test_every_route_value_is_explicit(self):
        for missing in ("--status", "--position-target", "--deadline-ms", "--interval-ms", "--no-motion-write-recorded"):
            argv = list(ENABLE)
            index = argv.index(missing)
            del argv[index:index + (1 if missing.startswith("--no-") else 2)]
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + ["--output", "x"])
        bad = [["--command", "stop", "--node", "1", "--trial", "1", "--deadline-ms", "2000", "--best-effort-ms", "0",
                "--interval-ms", "100"],  # No kind.
               ["--command", "stop", "--node", "1", "--trial", "1", "--kind", "halt", "--deadline-ms", "2000",
                "--best-effort-ms", "1000", "--interval-ms", "100"],  # Best effort before the deadline.
               ["--command", "readiness-actions", "--node", "1", "--trial", "1"],  # No --fault-led.
               ["--command", "readiness-io", "--node", "3", "--trial", "1"],
               ["--command", "mode1", "--node", "1", "--trial", "1", "--cycles", "11", "--interval-ms", "200"],
               ["--command", "position", "--node", "1", "--trial", "1", "--start-position", "0", "--target", "16",
                "--rpm", "1", "--ramp-up-ms", "1000", "--ramp-down-ms", "1000", "--cycles", "4",
                "--interval-ms", "100", "--stop-kind", "none", "--stop-after", "2"],
               ["--command", "position", "--node", "1", "--trial", "1", "--start-position", "0",
                "--target", "1000001", "--rpm", "1", "--ramp-up-ms", "1000", "--ramp-down-ms", "1000",
                "--cycles", "4", "--interval-ms", "100", "--stop-kind", "none"],
               ["--command", "ping", "--node", "1"]]
        for argv in bad:
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + ["--output", "x"])

    def test_lines_match_the_firmware_grammar(self):
        cases = {
            ("--command", "readiness-tuning", "--node", "2", "--trial", "4"): "TU2,4\n",
            ("--command", "mode1", "--node", "1", "--trial", "5", "--cycles", "3", "--interval-ms", "200"):
                "TM1,5,1,3,200\n",
            ("--command", "stop", "--node", "1", "--trial", "6", "--kind", "quick", "--deadline-ms", "2000",
             "--best-effort-ms", "3000", "--interval-ms", "100"): "TS1,6,1,2,2000,3000,100\n",
            ("--command", "position", "--node", "1", "--trial", "7", "--start-position", "-3", "--target", "16",
             "--rpm", "1", "--ramp-up-ms", "1000", "--ramp-down-ms", "1000", "--cycles", "4", "--interval-ms", "100",
             "--stop-kind", "halt", "--stop-after", "2", "--stop-deadline-ms", "2000", "--stop-best-effort-ms", "0",
             "--stop-interval-ms", "200"): "TP1,7,1,-3,16,1,1000,1000,4,100,1,2,2000,0,200\n",
            ("--command", "position", "--node", "1", "--trial", "8", "--start-position", "0", "--target", "16",
             "--rpm", "1", "--ramp-up-ms", "1000", "--ramp-down-ms", "1000", "--cycles", "4", "--interval-ms", "100",
             "--stop-kind", "none"): "TP1,8,1,0,16,1,1000,1000,4,100,0,0,0,0,0\n",
        }
        for argv, line in cases.items():
            args, fields = bench.arguments(BASE + list(argv) + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), line)

    def test_wrong_board_refuses_before_anything(self):
        for ports in ([], [Item(serial="OTHER")], [Item(), Item()], [Item(vid=0x10C4)]):
            board = Board()
            status, _, err = self.run_main(["--command", "readiness-io", "--node", "1", "--trial", "1"],
                                           Backend(board, ports))
            self.assertEqual(status, 2, err)
            self.assertFalse(self.output.exists())
            self.assertEqual(board.writes, [])

    def test_trial_needs_the_recorded_fixture(self):
        board = Board()
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = bench.main(["--port", "COM9", "--serial", "SPARE", "--command", "readiness-io", "--node", "1",
                                 "--trial", "1", "--output", str(self.output)],
                                Backend(board, [Item("COM9", "SPARE")]), Clock(), Operator())
        self.assertEqual(status, 2)
        self.assertIn("not the recorded fixture", err.getvalue())
        self.assertEqual(board.writes, [])

    def test_attended_confirmation_is_typed(self):
        for typed in ("", "attended 12", "ATTENDED 11", "yes"):
            board = Board()
            status, _, err = self.run_main(ENABLE, Backend(board), Operator(typed, "FREE"))
            self.assertEqual(status, 2, err)
            self.assertIn("attended", err)
            self.assertEqual(board.writes, [])
            self.assertFalse(self.output.exists())
        # The shaft observation is the attendant's, exactly as one of the two words.
        for typed in ("", "free", "MAYBE"):
            board = Board()
            status, _, err = self.run_main(ENABLE, Backend(board), Operator("ATTENDED 12", typed))
            self.assertEqual(status, 2, err)
            self.assertEqual(board.writes, [])

    def test_enable_sends_the_typed_observation(self):
        board = Board()
        operator = Operator("ATTENDED 12", "HOLDING", "no movement", "HOLDING")
        status, out, _ = self.run_main(ENABLE, Backend(board), operator)
        self.assertEqual(status, 0, out)
        self.assertEqual(board.writes, [b"p", b"TE1,12,1,1,2,32823,5000,120,10000,800\n"])
        meta = self.meta()
        self.assertEqual(meta["line"], "TE1,12,1,1,2,32823,5000,120,10000,800")
        self.assertEqual(meta["operator"]["power_up_shaft"]["typed"], "HOLDING")
        self.assertEqual(meta["operator"]["holding_after"]["typed"], "HOLDING")
        self.assertEqual(meta["operator"]["shaft_after"]["typed"], "no movement")
        self.assertFalse(meta["retry"] or meta["reset"])
        self.assertTrue(board.closed)
        report = evidence.replay_capture(self.output, manifest=self.output / "none.json")
        self.assertEqual(report.verdict, "complete")

    def test_readiness_records_the_fault_led(self):
        board = Board()
        operator = Operator("off", "off")
        status, _, _ = self.run_main(["--command", "readiness-actions", "--node", "2", "--trial", "3",
                                      "--fault-led", "prompt"], Backend(board), operator)
        # The synthetic record has no terminals, so the fixed inventory reads it as incomplete.
        self.assertEqual(status, 1)
        self.assertEqual(board.writes, [b"p", b"TA2,3\n"])
        meta = self.meta()
        self.assertEqual(meta["operator"]["fault_led_before"]["typed"], "off")
        self.assertEqual(meta["operator"]["fault_led_after"]["typed"], "off")
        self.assertNotIn("attended", meta["operator"])
        self.assertTrue((self.output / "rx.bin").read_bytes().startswith(b"stale\n"))

    def test_image_state_refuses_the_line(self):
        for name, value, reason in (("routes", 0, "image_without_routes"), ("soak", 1, "soak_running"),
                                    ("trial", 1, "trial_record_held"), ("active", 0, "can_not_started")):
            board = Board()
            setattr(board, name, value)
            if self.output.exists():
                for item in self.output.iterdir():
                    item.unlink()
                self.output.rmdir()
            status, _, _ = self.run_main(["--command", "readiness-io", "--node", "1", "--trial", "1"], Backend(board))
            self.assertEqual(status, 2, name)
            self.assertEqual(board.writes, [b"p"])
            self.assertEqual(self.meta()["reason"], reason)

    def test_firmware_refusal_and_failed_evidence(self):
        board = Board()
        board.answer = "TRIAL_REFUSED reason=history; no work\n"
        status, _, _ = self.run_main(["--command", "mode1", "--node", "1", "--trial", "5", "--cycles", "3",
                                      "--interval-ms", "200"], Backend(board), Operator("ATTENDED 5", "NONE"))
        self.assertEqual(status, 2)
        self.assertEqual(self.meta()["reason"], "refused")
        report = evidence.replay_capture(self.output, manifest=self.output / "none.json")
        self.assertEqual(report.verdict, "refused")
        for item in self.output.iterdir():
            item.unlink()
        self.output.rmdir()
        board = Board()
        board.answer = record("stop", 1, 6, failure=11, why="stop")
        status, _, _ = self.run_main(["--command", "stop", "--node", "1", "--trial", "6", "--kind", "halt",
                                      "--deadline-ms", "2000", "--best-effort-ms", "0", "--interval-ms", "100"],
                                     Backend(board), Operator("ATTENDED 6", "NONE"))
        self.assertEqual(status, 3)
        report = evidence.replay_capture(self.output, manifest=self.output / "none.json")
        self.assertEqual((report.verdict, report.failure_name), ("failed-evidence", "REFUSED"))

    def test_parser_pins_the_record_to_the_request(self):
        body = record("stop", 1, 6, failure=11, why="stop").encode("ascii")
        meta = {"command": "stop", "node": 1, "trial": 6, "line": "TS1,6,1,1,2000,0,100", "status": "captured"}
        self.assertEqual(evidence.replay_bytes(body, meta).verdict, "failed-evidence")
        for change in ({"node": 2}, {"trial": 7}, {"command": "enable"}):
            other = dict(meta, **change)
            report = evidence.replay_bytes(body, other)
            self.assertEqual(report.verdict, "inconsistent", change)
        damaged = body.replace(b"summaries=2", b"summaries=3")
        self.assertIn(evidence.replay_bytes(damaged, meta).verdict, ("incomplete", "inconsistent"))
        mismatch = body.replace(b"TRIAL_PLAN route=stop node=1 trial=6", b"TRIAL_PLAN route=stop node=1 trial=5")
        self.assertEqual(evidence.replay_bytes(mismatch, meta).verdict, "inconsistent")
        success = body.replace(b"failure=11 why=stop", b"failure=0 why=none")
        self.assertEqual(evidence.replay_bytes(success, meta).verdict, "complete")
        signed = evidence.parse_line(1, "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0004 6041=8037 "
                                        "6061=01 6064=-16 606c=-2")
        self.assertEqual((signed.status, signed.values["6064"], signed.values["606c"]), ("parsed", -16, -2))
        self.assertEqual(evidence.parse_line(1, "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0004 "
                                                "6041=8037 6061=01 6064=--1 606c=0").status, "malformed")

    def test_single_byte_commands_unchanged(self):
        board = Board()
        status, out, _ = self.run_main(["--command", "ping", "--seconds", "1"], Backend(board))
        self.assertEqual(status, 0)
        self.assertEqual(board.writes, [b"p"])
        meta = self.meta()
        self.assertEqual(meta["tx_hex"], "70")
        self.assertIn("PONG", out)
        self.assertEqual(bench.COMMANDS["trial-abort"], b"A")
        report = evidence.replay_capture(self.output, manifest=self.output / "none.json")
        self.assertEqual(report.verdict, "control")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--transcript":
        found = check_trial_transcript(sys.argv[2])
        for problem in found:
            print(problem)
        print("trial transcript: %s" % ("failed" if found else "passed"))
        sys.exit(1 if found else 0)
    unittest.main()
