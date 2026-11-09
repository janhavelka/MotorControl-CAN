#!/usr/bin/env python3
"""s2_bench.py trial routes against a fake board, clock and operator: no port, no pyserial."""

import contextlib
import hashlib
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
         "S": "stop", "P": "position", "R": "rearm", "G": "move", "H": "readiness-history",
         "Q": "released-quick", "W": "word67", "L": "relative", "D": "settings-2202", "J": "io-di3",
         "Z": "counter-zero", "C": "clear-alarm", "N": "mode3", "V": "velocity", "O": "home"}
ROUTES = {"A": "actions", "I": "io", "U": "tuning", "M": "mode1", "E": "enable", "S": "stop", "P": "position",
          "R": "rearm", "G": "move", "H": "history", "Q": "released-quick", "W": "word67", "L": "relative",
          "D": "settings-2202", "J": "io-di3", "Z": "counter-zero", "C": "clear-alarm", "N": "mode3",
          "V": "velocity", "O": "home"}
# Protocol 6's routes. `bench_route_test --transcript` prints the earlier ones only; `--transcript-all` adds
# these, which scripts/s2_evidence.py reads once its grammar names them (see check_trial_transcript).
NEW_LETTERS = frozenset("NVO")


def check_trial_transcript(path, every=False):
    """Every record the real BenchRoute prints must parse whole, with its expected verdict. every: the
    transcript of --transcript-all, which also holds protocol 6's mode3, velocity and home records."""
    data = Path(path).read_bytes()
    problems = []
    parts = data.split(b"=== ")[1:]
    wanted_count = 30 if every else 26
    if len(parts) < wanted_count:
        problems.append("expected %d records, found %d" % (wanted_count, len(parts)))
    letters = {part[:1].decode("ascii") for part in parts}
    for letter in NAMES:
        if letter not in letters and (every or letter not in NEW_LETTERS):
            problems.append("no %s record" % NAMES[letter])
    for part in parts:
        head, _, body = part.partition(b"\n")
        line = head.decode("ascii")
        fields = line[1:].split(",")
        meta = {"command": NAMES[line[0]], "node": int(fields[0]), "trial": int(fields[1]), "line": "T" + line,
                "status": "captured"}
        report = evidence.replay_bytes(body, meta)
        # A lost reply on node 1, and a move on node 2 whose copied traffic overflowed. Home is refused before
        # any write by design, and one velocity line names a closed start flag the library refuses.
        lost = line.startswith("G2,13,")
        refused = line[0] == "O" or (line[0] == "V" and fields[12] == "0")
        wanted = "failed-evidence" if line.startswith("S1,7,") or lost or refused else "complete"
        if report.verdict != wanted or report.family != "TRIAL":
            problems.append("%s: %s %s %s" % (line, report.verdict, report.family, report.errors[:3]))
        if lost and not (report.failure_name == "EVIDENCE_LOSS" and b" lossCancel=1\n" in body):
            problems.append("%s: not an evidence loss with a local withdrawal" % line)
        if report.unknown or report.malformed or report.unknown_fields:
            problems.append("%s: unknown %s malformed %s fields %s" % (
                line, report.unknown[:2], report.malformed[:2], report.unknown_fields[:2]))
        kinds = set(report.kinds)
        for kind in ("ADMITTED", "TRIAL", "TRIAL_PLAN", "TRIAL_RES", "END"):
            if kind not in kinds:
                problems.append("%s: no %s line" % (line, kind))
        if line.startswith("E") and not {"TRIAL_ENABLE", "TRIAL_ENABLE_STEP"} <= kinds:
            problems.append("%s: no enable summary" % line)
        # Only a position or relative line naming a stop kind (its eleventh field) has stop summaries.
        if line[0] in "PL" and not ({"TRIAL_CHECK"} | ({"TRIAL_STOP", "TRIAL_BEST_EFFORT"}
                                                       if fields[10] != "0" else set())) <= kinds:
            problems.append("%s: no check or stop summary" % line)
        if line.startswith("L") and "TRIAL_BASE" not in kinds:
            problems.append("%s: no base summary" % line)
        if line[0] in "QW" and not {"TRIAL_FIRST", "TRIAL_CHECK", "TRIAL_CONTROL"} <= kinds:
            problems.append("%s: no first read, check or control summary" % line)
        # A move naming a relative base (its twenty-seventh field) reads twice and shows its witness.
        if line.startswith("G") and fields[26] != "0" and not {"TRIAL_FIRST", "TRIAL_BASE"} <= kinds:
            problems.append("%s: no first read or base summary" % line)
        # A move naming an observer node (its twenty-ninth field) holds both nodes' terminals.
        if line.startswith("G") and fields[28] != "0":
            if not {"TRIAL_OBSERVER", "TRIAL_OBSERVER_CHECK", "TRIAL_OBSERVER_LAST", "TRIAL_NODES"} <= kinds:
                problems.append("%s: no observer summary" % line)
            if b" node=1\n" not in body or b" node=2\n" not in body:
                problems.append("%s: not both nodes' terminals" % line)
        if line[0] in "DJ" and not {"TRIAL_SETTINGS"} | ({"TRIAL_LIMITS"} if line[0] == "D" else
                                                         {"TRIAL_IO", "TRIAL_INPUTS"}) <= kinds:
            problems.append("%s: no settings summary" % line)
        if line[0] in "ZC" and not {"TRIAL_CLEAR", "TRIAL_CLEAR_STEP", "TRIAL_INCIDENT", "TRIAL_BASELINE",
                                    "TRIAL_EXTRA"} <= kinds:
            problems.append("%s: no clear summary" % line)
        if line.startswith("R") and not {"TRIAL_CHECK", "TRIAL_REARM"} <= kinds:
            problems.append("%s: no check or rearm summary" % line)
        if line.startswith("G") and not {"TRIAL_CHECK", "TRIAL_EVIDENCE", "TRIAL_MOVE", "TRIAL_MOVE_SAMPLE"} <= kinds:
            problems.append("%s: no check, evidence or move summary" % line)
        if line.startswith("H") and "TRIAL_HISTORY" not in kinds:
            problems.append("%s: no history summary" % line)
        if line[0] == "N" and not {"TRIAL_FIRST", "TRIAL_CHECK", "TRIAL_CONTROL"} <= kinds:
            problems.append("%s: no first read, check or control summary" % line)
        if line[0] == "O" and not {"TRIAL_STATUS", "TRIAL_IO", "TRIAL_HOME"} <= kinds:
            problems.append("%s: no refusal, input mapping or home summary" % line)
        if line[0] == "O" and (b" dir=1 " in body or b" gate=0 " not in body):
            problems.append("%s: a home record with a download or an open gate" % line)
        run = {"TRIAL_VELOCITY", "TRIAL_VELOCITY_TIME", "TRIAL_VELOCITY_SAMPLE", "TRIAL_STOP", "TRIAL_BEST_EFFORT"}
        if line[0] == "V" and not ({"TRIAL_CHECK", "TRIAL_VELOCITY_EVIDENCE"} |
                                   ({"TRIAL_STATUS"} if refused else run)) <= kinds:
            problems.append("%s: no check, evidence or velocity summary" % line)
        if line[0] == "V" and refused and kinds & run:
            problems.append("%s: a refused velocity printed a run" % line)
        if line[0] in "AIUH" and not report.inventory_checked:
            problems.append("%s: fixed inventory not checked" % line)
        # The routes read 607D:00 and 60FE:00 in two bytes, as both specimens answered them. Against
        # the io inventory's EDS U8 that stays an observation beside the record, never a correction.
        if line[0] == "I":
            seen = {(w["object"], w["actual"], w["documented"]) for w in report.width_discrepancies}
            if seen != {("60fe:00", 2, 1)}:
                problems.append("%s: width observations %s" % (line, sorted(seen)))
        if line[0] == "D":
            counts = [w["actual"] for w in report.widths if w["object"] == "607d:00"]
            if counts != [2, 2]:
                problems.append("%s: 607D:00 widths %s" % (line, counts))
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

    def __init__(self, on_line=None):
        self.on_line = on_line  # Called with each T line as it is written, before any reply.
        self.writes = []
        self.queue = bytearray(b"stale\n")
        self.routes = bench.ROUTE_PROTOCOL
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
            if self.on_line:
                self.on_line(data)
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
LEDGER = ["--campaign", "campaign.jsonl"]  # For argument parsing only: nothing is read.
# A move line's evidence: this test's numbers, not any measurement.
MOVE = ["--command", "move", "--node", "1", "--trial", "20", "--start-position", "0", "--target", "16", "--rpm", "1",
        "--ramp-up-ms", "1000", "--ramp-down-ms", "1000", "--deadline-ms", "10000", "--interval-ms", "1000",
        "--record", "19", "--accept-source", "statusword", "--accept-mask", "0x1000", "--accept-value", "0x1000",
        "--complete-source", "statusword", "--complete-mask", "0x8000", "--complete-value", "0x8000",
        "--rule", "after-absence", "--device-type", "0x00040192", "--hardware-version", "0x12",
        "--software-version", "0x34", "--vendor-id", "0x300", "--product-code", "0x8605", "--revision", "0x10001",
        "--model-code", "0x86", "--firmware-code", "0x123", "--stop-kind", "none"]
MOVE_HEAD = "TG1,20,1,0,16,1,1000,1000,10000,1000,19,1,4096,4096,1,32768,32768,1,262546,18,52,768,34309,65537,134,291"
MOVE_BODY = MOVE_HEAD + ",0,0,0"  # Absolute: no relative base, no witness and no observer.


def signed(text):
    """A T line as the runner sends it: its integrity field, then LF."""
    return bench.sign_line(text) + "\n"


MOVE_LINE = signed(MOVE_BODY + ",0,0,0,0,0")
# A velocity line's evidence: this test's numbers, the same as test/bench_route_test.cpp's fake, never a
# measurement.
VELOCITY = ["--command", "velocity", "--node", "1", "--trial", "30", "--rpm", "6", "--ramp-up-ms", "200",
            "--ramp-down-ms", "200", "--duration-ms", "12000", "--interval-ms", "2000", "--deadline-ms", "18000",
            "--kind", "halt", "--record", "351", "--layout", "zh86", "--start-qualified", "1", "--stop-qualified",
            "1", "--device-type", "0x00040192", "--hardware-version", "0x12", "--software-version", "0x34",
            "--vendor-id", "0x300", "--product-code", "0x8605", "--revision", "0x10001", "--model-code", "0x86",
            "--firmware-code", "0x123"]
VELOCITY_LINE = signed("TV1,30,1,6,200,200,12000,2000,18000,1,351,2,1,1,262546,18,52,768,34309,65537,134,291")
HOME = ["--command", "home", "--node", "1", "--trial", "31", "--method", "24", "--search-rpm", "60", "--creep-rpm",
        "6", "--ramp-ms", "200", "--offset", "0"]
MODE3 = ["--command", "mode3", "--node", "1", "--trial", "32", "--cycles", "2", "--interval-ms", "200"]


def position(node, trial, stop="none"):
    argv = ["--command", "position", "--node", str(node), "--trial", str(trial), "--start-position", "0",
            "--target", "16", "--rpm", "1", "--ramp-up-ms", "1000", "--ramp-down-ms", "1000", "--cycles", "4",
            "--interval-ms", "100", "--stop-kind", stop]
    if stop != "none":
        argv += ["--stop-after", "2", "--stop-deadline-ms", "2000", "--stop-best-effort-ms", "0",
                 "--stop-interval-ms", "200"]
    return argv


def send_entry(route, node, trial, motion=1, stop=0):
    """A send entry consistent with itself: its line names its route, node, trial and stop."""
    tail = "1,2,2000,0,200" if stop else "0,0,0,0,0"
    return {"event": "send", "route": route, "node": node, "trial": trial, "motion": motion, "stop": stop,
            "line": bench.sign_line("T%s%d,%d,1,%s" % (route, node, trial, tail))}


def result_entry(node, trial):
    return {"event": "result", "node": node, "trial": trial, "reason": "ended"}


def chain(entries):
    """Ledger bytes for these entries, each with its seq and the SHA-256 of the line before."""
    out, previous = [], bench.LEDGER_GENESIS
    for number, entry in enumerate(entries, 1):
        line = json.dumps(dict(entry, seq=number, prev=previous), sort_keys=True).encode("utf-8")
        out.append(line + b"\n")
        previous = hashlib.sha256(line).hexdigest()
    return b"".join(out)


class BenchTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.output = Path(self.directory.name) / "capture"
        self.ledger = Path(self.directory.name) / "campaign.jsonl"
        self.ledger.write_bytes(b"")  # An existing, empty campaign.

    def tearDown(self):
        self.directory.cleanup()

    def run_main(self, argv, backend, operator=None):
        out, err = io.StringIO(), io.StringIO()
        command = argv[argv.index("--command") + 1]
        counted = command in bench.TRIALS and (bench.TRIALS[command][1] or command in bench.SETTINGS_ROUTES)
        if counted and "--campaign" not in argv:
            argv = argv + ["--campaign", str(self.ledger)]
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = bench.main(BASE + argv + ["--output", str(self.output)], backend, Clock(),
                                operator or Operator())
        return status, out.getvalue(), err.getvalue()

    def entries(self):
        return [json.loads(line) for line in self.ledger.read_text(encoding="utf-8").splitlines()]

    def write_ledger(self, entries):
        self.ledger.write_bytes(chain(entries))

    def clear_output(self):
        if self.output.exists():
            for item in self.output.iterdir():
                item.unlink()
            self.output.rmdir()

    def meta(self):
        return json.loads((self.output / "request.json").read_text(encoding="utf-8"))

    def test_dry_run_touches_nothing(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = bench.main(BASE + ENABLE + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(),
                                Clock(), Operator())
        self.assertEqual(status, 0)
        self.assertIn("TE1,12,1,1,0,32823,5000,120,10000,800", out.getvalue())
        self.assertIn("ATTENDED 12", out.getvalue())
        self.assertIn("motion_total': 0", out.getvalue())
        self.assertFalse(self.output.exists())
        self.assertEqual(self.ledger.read_bytes(), b"")
        # Starting a campaign is a plan too: nothing is created.
        fresh = Path(self.directory.name) / "new.jsonl"
        with contextlib.redirect_stdout(io.StringIO()):
            bench.main(BASE + ENABLE + ["--dry-run", "--campaign", str(fresh), "--new-campaign"], Untouchable(),
                       Clock(), Operator())
        self.assertFalse(fresh.exists())

    def test_every_route_value_is_explicit(self):
        for missing in ("--status", "--position-target", "--deadline-ms", "--interval-ms", "--no-motion-write-recorded"):
            argv = list(ENABLE)
            index = argv.index(missing)
            del argv[index:index + (1 if missing.startswith("--no-") else 2)]
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        # Every move evidence value is required: none has a default.
        for index in range(MOVE.index("--rpm"), len(MOVE), 2):
            argv = MOVE[:index] + MOVE[index + 2:]
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        for name, value in (("--accept-value", "0x1001"), ("--accept-mask", "0"), ("--rpm", "0"),
                            ("--ramp-up-ms", "0"), ("--target", "1000001"), ("--hardware-version", "0x10000")):
            argv = list(MOVE)
            argv[argv.index(name) + 1] = value
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
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
               ["--command", "ping", "--node", "1"],
               ["--command", "rearm", "--node", "1", "--trial", "1", "--predecessor", "position-trigger", "--cycles",
                "17", "--interval-ms", "100"],
               ["--command", "rearm", "--node", "1", "--trial", "1", "--predecessor", "position-trigger", "--cycles",
                "3", "--interval-ms", "19"],
               # The predecessor is named, never defaulted.
               ["--command", "rearm", "--node", "1", "--trial", "1", "--cycles", "3", "--interval-ms", "100"]]
        for argv in bad:
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])

    def test_lines_match_the_firmware_grammar(self):
        cases = {
            ("--command", "readiness-tuning", "--node", "2", "--trial", "4"): signed("TU2,4"),
            ("--command", "mode1", "--node", "1", "--trial", "5", "--cycles", "3", "--interval-ms", "200"):
                signed("TM1,5,1,3,200"),
            ("--command", "stop", "--node", "1", "--trial", "6", "--kind", "quick", "--deadline-ms", "2000",
             "--best-effort-ms", "3000", "--interval-ms", "100"): signed("TS1,6,1,2,2000,3000,100"),
            ("--command", "position", "--node", "1", "--trial", "7", "--start-position", "-3", "--target", "16",
             "--rpm", "1", "--ramp-up-ms", "1000", "--ramp-down-ms", "1000", "--cycles", "4", "--interval-ms", "100",
             "--stop-kind", "halt", "--stop-after", "2", "--stop-deadline-ms", "2000", "--stop-best-effort-ms", "0",
             "--stop-interval-ms", "200"): signed("TP1,7,1,-3,16,1,1000,1000,4,100,1,2,2000,0,200"),
            ("--command", "position", "--node", "1", "--trial", "8", "--start-position", "0", "--target", "16",
             "--rpm", "1", "--ramp-up-ms", "1000", "--ramp-down-ms", "1000", "--cycles", "4", "--interval-ms", "100",
             "--stop-kind", "none"): signed("TP1,8,1,0,16,1,1000,1000,4,100,0,0,0,0,0"),
            ("--command", "rearm", "--node", "2", "--trial", "9", "--predecessor", "position-trigger", "--cycles", "3",
             "--interval-ms", "100"): signed("TR2,9,1,1,3,100"),
            ("--command", "rearm", "--node", "2", "--trial", "9", "--predecessor", "position-halt", "--cycles", "3",
             "--interval-ms", "100"): signed("TR2,9,1,2,3,100"),
            ("--command", "rearm", "--node", "2", "--trial", "9", "--predecessor", "relative-trigger", "--cycles",
             "3", "--interval-ms", "100"): signed("TR2,9,1,3,3,100"),
            tuple(MOVE): MOVE_LINE,
            tuple(MOVE[:-1] + ["halt", "--stop-after", "2", "--stop-deadline-ms", "2000", "--stop-best-effort-ms",
                               "3000", "--stop-interval-ms", "500"]): signed(MOVE_BODY + ",1,2,2000,3000,500"),
        }
        for argv, line in cases.items():
            args, fields = bench.arguments(BASE + list(argv) + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), line)
        # The longest move line fits the firmware's ROUTE_LINE (256 bytes after T).
        widest = list(MOVE)
        for name, value in (("--trial", "4294967295"), ("--start-position", "-2147483648"), ("--target", "-1000000"),
                            ("--record", "4294967295"), ("--device-type", "0xffffffff"), ("--vendor-id", "0xffffffff"),
                            ("--product-code", "0xffffffff"), ("--revision", "0xffffffff")):
            widest[widest.index(name) + 1] = value
        args, fields = bench.arguments(BASE + widest[:-1] + ["quick", "--stop-after", "16", "--stop-deadline-ms",
                                                             "55000", "--stop-best-effort-ms", "55000",
                                                             "--stop-interval-ms", "10000"] + LEDGER + ["--output", "x"])
        self.assertLessEqual(len(bench.encode(args.command, fields)) - 2, 256)
        # zlib's CRC-32, the same values the firmware's own test pins.
        self.assertEqual(bench.sign_line("TA1,5"), "TA1,5,K62328164")
        self.assertEqual(bench.sign_line("TR2,9,1,16,5000"), "TR2,9,1,16,5000,K3c9f15db")

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
        # Nobody felt the shaft: the truthful answer is offered, named in the refusal, and sends nothing.
        board = Board()
        operator = Operator("ATTENDED 12", "UNOBSERVED")
        status, _, err = self.run_main(ENABLE, Backend(board), operator)
        self.assertEqual(status, 2, err)
        self.assertIn("not observed", err)
        self.assertIn("UNOBSERVED", operator.questions[-1])
        self.assertEqual(board.writes, [])
        self.assertFalse(self.output.exists())

    def test_a_relayed_power_up_shaft_lets_an_unattended_enable_run_and_is_recorded_as_relayed(self):
        # 2026-11-09: the user felt both shafts after the power cycle, then left; the lead relays it.
        waiver = "I will not be present - do all the tests yourself"
        source = "user dialog 2026-11-09 10:57Z after the power cycle, before anything was sent"
        board = Board()
        operator = Operator()  # Any question would raise EOFError and refuse.
        argv = ENABLE + ["--unattended-waiver", waiver, "--relayed-power-up-shaft", "FREE",
                         "--relayed-shaft-source", source]
        status, out, err = self.run_main(argv, Backend(board), operator)
        self.assertEqual(operator.questions, [])
        sent = [w for w in board.writes if w.startswith(b"T")]
        self.assertEqual(len(sent), 1)
        self.assertEqual(sent[0].split(b",")[4], str(bench.SHAFT["FREE"]).encode())
        meta = self.meta()
        self.assertEqual(meta["operator"]["power_up_shaft"],
                         {"typed": "FREE", "relayed": True, "source": source, "accepted": True})
        self.assertEqual(meta["operator"]["holding_after"]["typed"], bench.UNOBSERVED)
        # Without the relayed observation, or out of its place, nothing is sent.
        for bad in (ENABLE + ["--unattended-waiver", waiver],
                    ENABLE + ["--relayed-power-up-shaft", "FREE", "--relayed-shaft-source", source],
                    ENABLE + ["--unattended-waiver", waiver, "--relayed-power-up-shaft", "FREE"],
                    ENABLE + ["--unattended-waiver", waiver, "--relayed-power-up-shaft", "FREE",
                              "--relayed-shaft-source", "  "],
                    MOVE + ["--unattended-waiver", waiver, "--relayed-power-up-shaft", "FREE",
                            "--relayed-shaft-source", source]):
            self.clear_output()
            board = Board()
            with self.assertRaises(SystemExit):
                self.run_main(bad, Backend(board), Operator())
            self.assertEqual(board.writes, [])

    def test_unattended_waiver_replaces_the_attendant_questions_only(self):
        waiver = "I will not be present; I take the responsibility"
        board = Board()
        operator = Operator()  # Any question would raise EOFError and refuse.
        status, out, err = self.run_main(MOVE + ["--unattended-waiver", waiver], Backend(board), operator)
        self.assertIn(status, (0, 1), err)  # The synthetic read record has no terminals.
        self.assertEqual(operator.questions, [])
        self.assertEqual(board.writes, [b"p", MOVE_LINE.encode()])  # The same line: the host gate is met.
        meta = self.meta()
        self.assertEqual(meta["operator"]["attended"], {"unattended": True, "waiver": waiver, "accepted": True})
        self.assertEqual(meta["operator"]["shaft_after"]["typed"], bench.UNOBSERVED)
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([entry.get("unattended_waiver") for entry in send], [waiver])
        # Nothing an attendant alone could give is accepted without one.
        for argv in (ENABLE + ["--unattended-waiver", waiver],
                     ["--command", "readiness-actions", "--node", "1", "--trial", "3", "--fault-led", "prompt",
                      "--unattended-waiver", waiver],
                     ["--command", "readiness-history", "--node", "1", "--trial", "3", "--fault-led", "prompt",
                      "--unattended-waiver", waiver],
                     MOVE + ["--unattended-waiver", "  "],
                     ["--command", "ping", "--unattended-waiver", waiver]):
            self.clear_output()
            board = Board()
            with self.assertRaises(SystemExit):
                self.run_main(argv, Backend(board), Operator())
            self.assertEqual(board.writes, [])
        self.clear_output()
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + MOVE + ["--dry-run", "--unattended-waiver", waiver, "--campaign", str(self.ledger)],
                       Untouchable(), Clock(), Operator())
        self.assertIn("unattended under the user's waiver", out.getvalue())
        self.assertNotIn("ATTENDED 20", out.getvalue())

    def test_enable_sends_the_typed_observation(self):
        board = Board()
        operator = Operator("ATTENDED 12", "HOLDING", "no movement", "HOLDING")
        status, out, _ = self.run_main(ENABLE, Backend(board), operator)
        self.assertEqual(status, 0, out)
        self.assertEqual(board.writes, [b"p", signed("TE1,12,1,1,2,32823,5000,120,10000,800").encode()])
        meta = self.meta()
        self.assertEqual(meta["line"], signed("TE1,12,1,1,2,32823,5000,120,10000,800").strip())
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
        self.assertEqual(board.writes, [b"p", signed("TA2,3").encode()])
        meta = self.meta()
        self.assertEqual(meta["operator"]["fault_led_before"]["typed"], "off")
        self.assertEqual(meta["operator"]["fault_led_after"]["typed"], "off")
        self.assertNotIn("attended", meta["operator"])
        self.assertTrue((self.output / "rx.bin").read_bytes().startswith(b"stale\n"))

    def test_history_route_line_and_fault_led(self):
        history = ["--command", "readiness-history", "--node", "2", "--trial", "4"]
        args, fields = bench.arguments(BASE + history + ["--fault-led", "unobserved", "--output", "x"])
        self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed("TH2,4"))
        # zlib's CRC-32 of "TH1,5", the value the firmware's own test pins.
        self.assertEqual(bench.sign_line("TH1,5"), "TH1,5,K1f3aceee")
        # A read: no attended field, no motion weight, no campaign required, never a motion route.
        self.assertEqual(bench.motion_weight("readiness-history", fields), (0, 0))
        self.assertNotIn("H", bench.MOTION_ROUTES)
        # --fault-led is required here as for readiness-actions, and still refused on other routes.
        for argv in (history, history + ["--fault-led", "seen"],
                     ["--command", "readiness-io", "--node", "1", "--trial", "1", "--fault-led", "unobserved"]):
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + ["--output", "x"])
        # prompt asks before and after, as readiness-actions does; nothing else is asked.
        board = Board()
        operator = Operator("off", "red blinking")
        status, _, err = self.run_main(history + ["--fault-led", "prompt"], Backend(board), operator)
        self.assertEqual(status, 1, err)  # The synthetic record has no terminals: the inventory reads it short.
        self.assertEqual(board.writes, [b"p", signed("TH2,4").encode()])
        self.assertEqual(len(operator.questions), 2)
        self.assertTrue(all("Fault LED of node 2" in question for question in operator.questions))
        meta = self.meta()
        self.assertEqual(meta["operator"]["fault_led_before"]["typed"], "off")
        self.assertEqual(meta["operator"]["fault_led_after"]["typed"], "red blinking")
        self.assertNotIn("attended", meta["operator"])
        self.assertEqual(meta["plan"], "packet 4 / packet 22 step 2")
        # prompt refuses an empty answer before anything is sent.
        self.clear_output()
        board = Board()
        status, _, err = self.run_main(history + ["--fault-led", "prompt"], Backend(board), Operator(""))
        self.assertEqual(status, 2, err)
        self.assertEqual(board.writes, [])
        # unobserved asks nothing and records that nobody looked; a named ledger keeps an uncounted send.
        for extra in ([], ["--unattended-waiver", "I accept that nobody watches this read"]):
            self.clear_output()
            self.ledger.write_bytes(b"")
            board = Board()
            operator = Operator()
            status, _, err = self.run_main(history + ["--fault-led", "unobserved", "--campaign", str(self.ledger)] +
                                           extra, Backend(board), operator)
            self.assertEqual(status, 1, err)
            self.assertEqual(operator.questions, [])
            self.assertEqual(board.writes, [b"p", signed("TH2,4").encode()])
            self.assertEqual(self.meta()["operator"], {"fault_led_before": {"typed": "unobserved", "accepted": True}})
            send = [entry for entry in self.entries() if entry["event"] == "send"]
            self.assertEqual([(e["route"], e["node"], e["trial"], e["motion"], e["stop"]) for e in send],
                             [("H", 2, 4, 0, 0)])

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
                                      "--deadline-ms", "2000", "--best-effort-ms", "0", "--interval-ms", "200"],
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

    def test_motion_needs_a_named_campaign(self):
        for argv in (ENABLE, MOVE, position(1, 1),
                     ["--command", "rearm", "--node", "1", "--trial", "1", "--predecessor", "position-trigger",
                      "--cycles", "3", "--interval-ms", "100"],
                     ["--command", "released-quick", "--node", "1", "--trial", "1", "--cycles", "3", "--interval-ms",
                      "100"]):
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + ["--output", "x"])
        for argv in (["--command", "ping"] + LEDGER, ["--command", "ping", "--new-campaign"],
                     ENABLE + ["--new-campaign"]):
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + ["--output", "x"])
        # A read route may name one, and its sends are then kept, uncounted.
        bench.arguments(BASE + ["--command", "readiness-io", "--node", "1", "--trial", "1"] + LEDGER +
                        ["--output", "x"])

    def test_missing_or_existing_ledger_refuses(self):
        missing = Path(self.directory.name) / "typo.jsonl"
        board = Board()
        status, _, err = self.run_main(position(1, 5) + ["--campaign", str(missing)], Backend(board),
                                       Operator("ATTENDED 5"))
        self.assertEqual(status, 2, err)
        self.assertIn("does not exist", err)
        self.assertEqual(board.writes, [])
        self.assertFalse(missing.exists() or self.output.exists())
        board = Board()
        status, _, err = self.run_main(position(1, 5) + ["--campaign", str(self.ledger), "--new-campaign"],
                                       Backend(board), Operator("ATTENDED 5"))
        self.assertEqual(status, 2, err)
        self.assertEqual(board.writes, [])
        # A new campaign is created by its first send, and holds it before the line is written.
        seen = []
        board = Board(lambda line: seen.append(missing.read_text(encoding="utf-8")))
        status, _, err = self.run_main(position(1, 5) + ["--campaign", str(missing), "--new-campaign"],
                                       Backend(board), Operator("ATTENDED 5", "none"))
        self.assertEqual(status, 0, err)
        self.assertEqual(len(seen), 1)
        sent = json.loads(seen[0])
        self.assertEqual((sent["event"], sent["route"], sent["node"], sent["trial"], sent["motion"], sent["stop"]),
                         ("send", "P", 1, 5, 1, 0))
        self.assertEqual(sent["line"], signed("TP1,5,1,0,16,1,1000,1000,4,100,0,0,0,0,0").strip())
        self.assertEqual((sent["seq"], sent["prev"]), (1, bench.LEDGER_GENESIS))
        lines = [json.loads(line) for line in missing.read_text(encoding="utf-8").splitlines()]
        self.assertEqual([entry["event"] for entry in lines], ["send", "result"])
        self.assertEqual(lines[1]["reason"], "ended")
        self.assertEqual(self.meta()["campaign"]["motion_total"], 0)

    def test_campaign_caps_hold_across_boots(self):
        # Three position trials on node 1, each perhaps on another boot: the board reports motion=0.
        self.write_ledger([send_entry("P", 1, 1), send_entry("R", 1, 2), send_entry("P", 1, 3),
                           send_entry("P", 1, 4)])
        board = Board()
        status, _, err = self.run_main(position(1, 5), Backend(board), Operator("ATTENDED 5"))
        self.assertEqual(status, 2, err)
        self.assertIn("position admissions on node 1", err)
        self.assertEqual(board.writes, [])
        self.assertFalse(self.output.exists())
        status, _, err = self.run_main(position(2, 6), Backend(Board()), Operator("ATTENDED 6", "none"))
        self.assertEqual(status, 0, err)
        self.clear_output()
        # A planned stop counts against stop as well, as in the firmware.
        self.write_ledger([send_entry("S", 2, 1), send_entry("S", 2, 2), send_entry("P", 2, 3, 2, 1)])
        board = Board()
        status, _, err = self.run_main(position(2, 7, "halt"), Backend(board), Operator("ATTENDED 7"))
        self.assertEqual(status, 2, err)
        self.assertIn("planned stop", err)
        self.assertEqual(board.writes, [])
        status, _, err = self.run_main(position(2, 8), Backend(Board()), Operator("ATTENDED 8", "none"))
        self.assertEqual(status, 0, err)
        self.assertEqual(self.entries()[-2]["motion"], 1)
        self.clear_output()
        # Twelve in all, whatever the route and node; reads stay outside the count.
        self.write_ledger([send_entry("AIU"[i % 3], 1, i + 1, 0) for i in range(5)] +
                          [send_entry("MESPRG"[i % 6], 1 + i % 2, i + 10) for i in range(11)])
        board = Board()
        argv = list(MOVE)
        argv[argv.index("--node") + 1], argv[argv.index("--trial") + 1] = "2", "40"
        status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 40", "none"))
        self.assertEqual(status, 0, err)  # The eleven motion sends make twelve with this one.
        self.clear_output()
        argv[argv.index("--trial") + 1] = "41"
        board = Board()
        status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 41"))
        self.assertEqual(status, 2, err)
        self.assertIn("over 12", err)
        self.assertEqual(board.writes, [])
        # Reads are still allowed, and still need a new trial identity.
        status, _, err = self.run_main(["--command", "readiness-io", "--node", "1", "--trial", "41",
                                        "--campaign", str(self.ledger)], Backend(Board()))
        self.assertIn(status, (0, 1), err)
        self.assertEqual(self.entries()[-2]["motion"], 0)

    def test_trial_reuse_and_damaged_ledger_refuse(self):
        self.write_ledger([send_entry("A", 1, 9, 0)])
        board = Board()
        status, _, err = self.run_main(position(1, 9), Backend(board), Operator("ATTENDED 9"))
        self.assertEqual(status, 2, err)
        self.assertIn("not above every trial", err)
        self.assertEqual(board.writes, [])
        for text in (b'{"event": "send", "route": "P"', b"not json\n", b'{"event": "other"}\n',
                     b'{"event": "send", "route": "P", "node": 3, "trial": 1, "motion": 1, "stop": 0}\n'):
            self.ledger.write_bytes(text)
            board = Board()
            status, _, err = self.run_main(position(1, 20), Backend(board), Operator("ATTENDED 20"))
            self.assertEqual(status, 2, (text, err))
            self.assertEqual(board.writes, [])
            self.assertEqual(self.ledger.read_bytes(), text)  # Never repaired or rewritten.

    def test_unwritable_ledger_sends_nothing(self):
        board = Board()
        original = bench.Campaign.append

        def failing(campaign, entry):
            raise OSError("disk full")
        bench.Campaign.append = failing
        try:
            status, _, err = self.run_main(position(1, 5), Backend(board), Operator("ATTENDED 5"))
        finally:
            bench.Campaign.append = original
        self.assertEqual(status, 2, err)
        self.assertEqual(board.writes, [b"p"])
        self.assertEqual(self.meta()["reason"], "campaign_unwritten")
        # The operator checks the ledger before anything else may use it.
        lock = Path(str(self.ledger) + ".lock")
        self.assertTrue(lock.exists())
        self.assertIn("campaign lock %s kept" % lock, err)

    def test_routes_need_this_line_protocol(self):
        for command, argv, typed in (("rearm", ["--command", "rearm", "--node", "1", "--trial", "3", "--predecessor",
                                                "position-trigger", "--cycles", "3", "--interval-ms", "100"],
                                      "ATTENDED 3"),
                                     ("move", MOVE, "ATTENDED 20"),
                                     ("velocity", VELOCITY, "ATTENDED 30"),
                                     ("readiness-io", ["--command", "readiness-io", "--node", "1", "--trial", "30"],
                                      None)):
            # Older images, protocol 5's among them, and one this runner does not know.
            for routes in (1, 2, 3, 4, 5, 7):
                self.clear_output()
                board = Board()
                board.routes = routes
                status, _, err = self.run_main(argv, Backend(board), Operator(typed) if typed else None)
                self.assertEqual(status, 2, err)
                self.assertEqual(board.writes, [b"p"])
                self.assertEqual(self.meta()["reason"], "image_route_version")
            self.ledger.write_bytes(b"")
            self.clear_output()
            board = Board()
            status, _, err = self.run_main(argv, Backend(board), Operator(typed, "none") if typed else None)
            self.assertIn(status, (0, 1), err)  # The synthetic read record has no terminals.
            if command == "rearm":
                self.assertEqual(board.writes[-1], signed("TR1,3,1,1,3,100").encode())
            elif command == "move":
                self.assertEqual(board.writes[-1], MOVE_LINE.encode())
            elif command == "velocity":
                self.assertEqual(board.writes[-1], VELOCITY_LINE.encode())
            self.ledger.write_bytes(b"")

    def test_concurrent_invocations_refuse_campaign_busy(self):
        other = Path(self.directory.name) / "second"
        second = {}

        def run_second(_line):
            # While the first invocation is sending its line, a second one names the same ledger.
            board = Board()
            err = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
                second["status"] = bench.main(BASE + position(1, 6) + ["--campaign", str(self.ledger), "--output",
                                                                       str(other)], Backend(board), Clock(),
                                              Operator("ATTENDED 6", "none"))
            second.update(err=err.getvalue(), writes=board.writes, ledger=self.ledger.read_bytes())

        status, _, err = self.run_main(position(1, 5), Backend(Board(run_second)), Operator("ATTENDED 5", "none"))
        self.assertEqual(status, 0, err)
        self.assertEqual(second["status"], 2, second["err"])
        self.assertIn("campaign busy", second["err"])
        self.assertEqual(second["writes"], [])
        self.assertFalse(other.exists())
        self.assertEqual([json.loads(line)["event"] for line in second["ledger"].splitlines()], ["send"])
        self.assertEqual([(e["event"], e["trial"], e["seq"]) for e in self.entries()],
                         [("send", 5, 1), ("result", 5, 2)])
        self.assertFalse(Path(str(self.ledger) + ".lock").exists())
        # Released at the end: the next invocation runs, and chains its entries to these.
        self.clear_output()
        status, _, err = self.run_main(position(1, 6), Backend(Board()), Operator("ATTENDED 6", "none"))
        self.assertEqual(status, 0, err)
        self.assertEqual([e["seq"] for e in self.entries()], [1, 2, 3, 4])

    def test_stale_lock_is_refused_and_kept(self):
        lock = Path(str(self.ledger) + ".lock")
        lock.write_bytes(b'{"pid": 1, "utc": "earlier"}\n')
        board = Board()
        status, _, err = self.run_main(position(1, 5), Backend(board), Operator("ATTENDED 5"))
        self.assertEqual(status, 2, err)
        self.assertIn("campaign busy: %s exists" % lock, err)
        self.assertIn("remove %s yourself" % lock, err)
        self.assertEqual(board.writes, [])
        self.assertTrue(lock.exists())  # Never removed for the operator.
        self.assertEqual(self.ledger.read_bytes(), b"")
        self.assertFalse(self.output.exists())

    def test_ledger_changed_under_the_lock_refuses_before_sending(self):
        later = chain([send_entry("A", 1, 9, 0), result_entry(1, 9)])

        def operator(question):
            # Something wrote the ledger behind the lock while the operator answered.
            self.ledger.write_bytes(later)
            return "ATTENDED 5"
        board = Board()
        status, _, err = self.run_main(position(1, 5), Backend(board), operator)
        self.assertEqual(status, 2, err)
        self.assertEqual(board.writes, [b"p"])
        self.assertEqual(self.meta()["reason"], "campaign_refused")
        self.assertIn("not above every trial", self.meta()["campaign_error"])
        self.assertEqual(self.ledger.read_bytes(), later)

    def test_broken_chain_refuses(self):
        good = [send_entry("A", 1, 1, 0), result_entry(1, 1), send_entry("P", 1, 2), result_entry(1, 2)]
        lines = chain(good).splitlines(keepends=True)
        changed = lines[1].replace(b'"reason": "ended"', b'"reason": "no_end"')  # Still a valid entry.
        cases = {"changed": lines[0] + changed + b"".join(lines[2:]), "removed": b"".join(lines[1:]),
                 "removed-middle": lines[0] + b"".join(lines[2:]), "repeated": b"".join(lines[:2] + lines[1:]),
                 "reordered": lines[1] + lines[0] + b"".join(lines[2:]),
                 "unchained": b"".join(json.dumps(e).encode() + b"\n" for e in good)}
        self.assertNotEqual(changed, lines[1])
        for name, text in cases.items():
            self.ledger.write_bytes(text)
            board = Board()
            status, _, err = self.run_main(position(1, 20), Backend(board), Operator("ATTENDED 20"))
            self.assertEqual(status, 2, (name, err))
            self.assertTrue("chain" in err or "seq" in err, (name, err))
            self.assertEqual(board.writes, [])
            self.assertEqual(self.ledger.read_bytes(), text)
        self.ledger.write_bytes(chain(good))
        status, _, err = self.run_main(position(1, 20), Backend(Board()), Operator("ATTENDED 20", "none"))
        self.assertEqual(status, 0, err)

    def test_truncated_ledger(self):
        whole = chain([send_entry("P", 1, 1), result_entry(1, 1), send_entry("P", 1, 2), result_entry(1, 2)])
        for cut in (len(whole) - 5, len(whole) - 60, whole.index(b"\n") + 7):
            self.ledger.write_bytes(whole[:cut])
            board = Board()
            status, _, err = self.run_main(position(1, 20), Backend(board), Operator("ATTENDED 20"))
            self.assertEqual(status, 2, err)
            self.assertIn("partial line", err)
            self.assertEqual(board.writes, [])
            self.assertEqual(self.ledger.read_bytes(), whole[:cut])
        # A cut at a line end is still a valid chain: only the committed ledger's history shows it.
        self.ledger.write_bytes(whole[:whole.rindex(b"\n", 0, len(whole) - 1) + 1])
        status, _, err = self.run_main(position(1, 20), Backend(Board()), Operator("ATTENDED 20", "none"))
        self.assertEqual(status, 0, err)

    def test_inconsistent_send_entry_refuses(self):
        def changed(**fields):
            return dict(send_entry("P", 1, 4), **fields)
        bad = {"motion over its stop": changed(motion=2), "motion on a read": dict(send_entry("A", 1, 4, 0), motion=1),
               "no motion on a motion route": changed(motion=0), "stop on mode1": dict(send_entry("M", 1, 4), stop=1),
               "boolean trial": changed(trial=True), "text trial": changed(trial="4"), "float trial": changed(trial=4.0),
               "node": changed(node=3), "line of another trial": changed(line=bench.sign_line("TP1,5,1,0,0,0,0,0")),
               "line without its field": changed(line="TP1,4,1,0,0,0,0,0"),
               "line with a wrong field": changed(line="TP1,4,1,0,0,0,0,0,K00000000"),
               "line stop differs": changed(stop=1, motion=2), "route": changed(route="X")}
        for name, entry in bad.items():
            text = chain([send_entry("A", 1, 1, 0), entry])
            self.ledger.write_bytes(text)
            board = Board()
            status, _, err = self.run_main(position(1, 20), Backend(board), Operator("ATTENDED 20"))
            self.assertEqual(status, 2, (name, err))
            self.assertIn("line 2 is an inconsistent send entry", err, name)
            self.assertEqual(board.writes, [])
            self.assertEqual(self.ledger.read_bytes(), text)
        # A result entry with no send entry before it.
        self.ledger.write_bytes(chain([result_entry(1, 1)]))
        status, _, err = self.run_main(position(1, 20), Backend(Board()), Operator("ATTENDED 20"))
        self.assertEqual(status, 2, err)
        self.assertIn("without its send entry", err)

    def test_new_route_lines_match_the_firmware_grammar(self):
        relative = ["--command", "relative", "--node", "2", "--trial", "7", "--start-position", "1187", "--target", "-16",
                    "--rpm", "1", "--ramp-up-ms", "1000", "--ramp-down-ms", "1000", "--cycles", "4",
                    "--interval-ms", "100", "--stop-kind", "halt", "--stop-after", "2", "--stop-deadline-ms", "2000",
                    "--stop-best-effort-ms", "0", "--stop-interval-ms", "200"]
        actual = MOVE + ["--move-base", "actual"]
        actual[actual.index("--start-position") + 1] = "-7"
        commanded = MOVE + ["--move-base", "commanded", "--base-position", "1187"]
        commanded[commanded.index("--target") + 1] = "-16"
        cases = {
            ("--command", "released-quick", "--node", "1", "--trial", "5", "--cycles", "3", "--interval-ms", "100"):
                signed("TQ1,5,1,3,100"),
            ("--command", "word67", "--node", "2", "--trial", "6", "--word6-cycles", "2", "--word7-cycles", "5",
             "--interval-ms", "250"): signed("TW2,6,1,2,5,250"),
            tuple(relative): signed("TL2,7,1,1187,-16,1,1000,1000,4,100,1,2,2000,0,200"),
            tuple(MOVE + ["--move-base", "absolute"]): MOVE_LINE,
            tuple(actual): signed(MOVE_HEAD.replace("TG1,20,1,0,", "TG1,20,1,-7,") + ",1,-7,0,0,0,0,0,0"),
            tuple(commanded): signed(MOVE_HEAD.replace(",16,1,1000,", ",-16,1,1000,") + ",2,1187,0,0,0,0,0,0"),
            tuple(MOVE + ["--observer", "2"]): signed(MOVE_HEAD + ",0,0,2,0,0,0,0,0"),
        }
        for argv, line in cases.items():
            args, fields = bench.arguments(BASE + list(argv) + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), line, argv)
        # Values outside the firmware's ranges, a zero displacement and a base that does not fit the move.
        bad = [["--command", "released-quick", "--node", "1", "--trial", "1", "--cycles", "17", "--interval-ms", "100"],
               ["--command", "released-quick", "--node", "1", "--trial", "1", "--cycles", "0", "--interval-ms", "100"],
               ["--command", "released-quick", "--node", "1", "--trial", "1", "--cycles", "3", "--interval-ms", "19"],
               ["--command", "released-quick", "--node", "1", "--trial", "1", "--interval-ms", "100"],
               ["--command", "word67", "--node", "1", "--trial", "1", "--word6-cycles", "5", "--word7-cycles", "3",
                "--interval-ms", "100"],
               ["--command", "word67", "--node", "1", "--trial", "1", "--word6-cycles", "0", "--word7-cycles", "3",
                "--interval-ms", "100"],
               ["--command", "word67", "--node", "1", "--trial", "1", "--word6-cycles", "2", "--word7-cycles", "17",
                "--interval-ms", "100"],
               ["--command", "word67", "--node", "1", "--trial", "1", "--word6-cycles", "2", "--interval-ms", "100"],
               [item if item != "-16" else "0" for item in relative],
               [item if item != "-16" else "1000001" for item in relative],
               relative + ["--predecessor", "position-trigger"],
               MOVE + ["--base-position", "5"],
               MOVE + ["--move-base", "actual", "--base-position", "5"],
               MOVE + ["--move-base", "commanded"],
               [item if item != "16" else "0" for item in MOVE] + ["--move-base", "actual"],
               ["--command", "stop", "--node", "1", "--trial", "1", "--kind", "halt", "--deadline-ms", "2000",
                "--best-effort-ms", "0", "--interval-ms", "200", "--move-base", "actual"]]
        for argv in bad:
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        # ACTUAL may restate its start position; it never takes another.
        args, fields = bench.arguments(BASE + actual + ["--base-position", "-7"] + LEDGER + ["--output", "x"])
        self.assertEqual(bench.encode(args.command, fields).decode("ascii"), cases[tuple(actual)])
        # A relative base keeps the stop tail last, so the ledger still finds its stop kind there.
        self.assertEqual(bench.motion_weight("move", fields), (1, 0))
        args, fields = bench.arguments(BASE + relative + LEDGER + ["--output", "x"])
        self.assertEqual(bench.motion_weight("relative", fields), (2, 1))
        self.assertEqual({"Q", "W", "L"} & bench.MOTION_ROUTES, {"Q", "W", "L"})

    def test_plan_bounds_follow_the_firmware(self):
        def bounds(argv):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            return bench.plan_bounds(args.command, fields)
        quick = ["--command", "released-quick", "--node", "1", "--trial", "1", "--interval-ms", "100", "--cycles"]
        words = ["--command", "word67", "--node", "1", "--trial", "1", "--interval-ms", "20", "--word6-cycles"]
        # The numbers test/bench_route_test.cpp pins against BenchRoute's own refusals.
        self.assertEqual(bounds(quick + ["3"]), (40, 300))
        self.assertEqual(bounds(quick + ["15"]), (124, 1500))
        self.assertEqual(bounds(quick + ["16"])[0], 131)
        self.assertEqual(bounds(words + ["2", "--word7-cycles", "3"]), (55, 100))
        self.assertEqual(bounds(words + ["3", "--word7-cycles", "12"])[0], 125)
        self.assertEqual(bounds(words + ["4", "--word7-cycles", "12"])[0], 132)
        # A planned stop's cycles read seven fields: 2000 ms polled every 200 ms is 80 terminals.
        self.assertEqual(bounds(position(1, 1, "halt"))[0], 18 + 7 * 4 + 80)
        stop = ["--command", "stop", "--node", "1", "--trial", "1", "--kind", "quick", "--deadline-ms", "2000",
                "--best-effort-ms", "0", "--interval-ms"]
        self.assertEqual(bounds(stop + ["200"]), (85, 2000))
        self.assertEqual(bounds(stop + ["100"])[0], 155)
        relative = MOVE + ["--move-base", "actual"]
        self.assertEqual(bounds(MOVE)[0], 108)
        self.assertEqual(bounds(relative)[0], 117)
        longer = list(relative)
        longer[longer.index("--deadline-ms") + 1] = "12000"
        self.assertEqual(bounds(longer)[0], 131)
        longer[longer.index("--move-base") + 1] = "absolute"
        self.assertEqual(bounds(longer)[0], 122)
        rearm = ["--command", "rearm", "--node", "1", "--trial", "1", "--predecessor", "position-halt", "--cycles",
                 "16", "--interval-ms", "5000"]
        self.assertEqual(bounds(rearm), (122, 80000))

    def test_plans_over_capacity_or_duration_refuse_before_sending(self):
        too_many = [["--command", "released-quick", "--node", "1", "--trial", "5", "--cycles", "16", "--interval-ms",
                     "100"],
                    ["--command", "word67", "--node", "1", "--trial", "5", "--word6-cycles", "4", "--word7-cycles", "12",
                     "--interval-ms", "100"],
                    ["--command", "stop", "--node", "1", "--trial", "5", "--kind", "quick", "--deadline-ms", "2000",
                     "--best-effort-ms", "0", "--interval-ms", "100"],
                    position(1, 5)]
        too_many[3][too_many[3].index("--cycles") + 1] = "16"  # 18 + 7 * 16 = 130.
        too_long = [["--command", "released-quick", "--node", "1", "--trial", "5", "--cycles", "12", "--interval-ms",
                     "5000"],
                    ["--command", "rearm", "--node", "1", "--trial", "5", "--predecessor", "relative-trigger",
                     "--cycles", "12", "--interval-ms", "5000"]]
        for reason, plans in (("capacity", too_many), ("duration", too_long)):
            for argv in plans:
                self.clear_output()
                board = Board()
                operator = Operator("ATTENDED 5", "none")
                status, _, err = self.run_main(argv, Backend(board), operator)
                self.assertEqual(status, 2, (argv, err))
                self.assertIn("REFUSED: %s" % reason, err)
                self.assertEqual((board.writes, operator.questions), ([], []))
                self.assertFalse(self.output.exists())
                self.assertEqual(self.ledger.read_bytes(), b"")  # Never counted.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + too_many[0] + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(), Clock(),
                       Operator())
        self.assertIn("would refuse before sending: capacity", out.getvalue())

    def test_short_move_deadline_refuses_before_sending(self):
        # The library refuses a move whose deadline does not exceed 13 services at the routes' 100 + 250 ms,
        # after the firmware counted the line (test/bench_route_test.cpp testPacket8Plan): 4550 ms is refused
        # here, unsent and uncounted, with its planned stop; 4551 ms passes this check.
        def move_with(deadline, stop):
            argv = list(MOVE) + ["--move-base", "actual"]
            argv[argv.index("--deadline-ms") + 1] = str(deadline)
            argv[argv.index("--interval-ms") + 1] = "2500"
            if stop:
                argv[argv.index("--stop-kind") + 1] = "halt"
                argv += ["--stop-after", "2", "--stop-deadline-ms", "1500", "--stop-best-effort-ms", "0",
                         "--stop-interval-ms", "750"]
            return argv
        for stop in (False, True):
            self.clear_output()
            board = Board()
            operator = Operator("ATTENDED 20", "none")
            status, _, err = self.run_main(move_with(4550, stop), Backend(board), operator)
            self.assertEqual(status, 2, err)
            self.assertIn("REFUSED: move deadline: 4550 ms does not cover the library's 13 services", err)
            self.assertEqual((board.writes, operator.questions), ([], []))
            self.assertFalse(self.output.exists())
            self.assertEqual(self.ledger.read_bytes(), b"")
        args, fields = bench.arguments(BASE + move_with(4551, True) + LEDGER + ["--output", "x"])
        bench.check_bounds(args.command, fields)  # No refusal.
        for command, argv in (("position", position(1, 5, "halt")),
                              ("stop", ["--command", "stop", "--node", "1", "--trial", "5", "--kind", "quick",
                                        "--deadline-ms", "2000", "--best-effort-ms", "0", "--interval-ms", "250"])):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            bench.check_bounds(command, fields)  # Short deadlines of other routes are not this rule.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + move_with(3000, True) + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(),
                       Clock(), Operator())
        self.assertIn("would refuse before sending: move deadline: 3000 ms", out.getvalue())

    def test_new_motion_routes_are_attended_and_counted(self):
        quick = ["--command", "released-quick", "--node", "2", "--trial", "5", "--cycles", "3", "--interval-ms", "100"]
        board = Board()
        operator = Operator("ATTENDED 5", "no movement")
        status, _, err = self.run_main(quick, Backend(board), operator)
        self.assertIn(status, (0, 1), err)  # The synthetic record has no terminals.
        self.assertEqual(board.writes, [b"p", signed("TQ2,5,1,3,100").encode()])
        self.assertIn("ATTENDED 5", operator.questions[0])
        self.assertIn("describe the node 2 shaft", operator.questions[1])
        self.assertEqual(self.meta()["plan"], "packet 16 step 3")
        # A relative experiment with its planned stop counts against stop too, as position does.
        self.clear_output()
        relative = position(2, 6, "halt")
        relative[relative.index("position")] = "relative"
        status, _, err = self.run_main(relative, Backend(Board()), Operator("ATTENDED 6", "none"))
        self.assertIn(status, (0, 1), err)
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["node"], e["trial"], e["motion"], e["stop"]) for e in send],
                         [("Q", 2, 5, 1, 0), ("L", 2, 6, 2, 1)])
        # The ledger reads both back; a fourth stop on node 2 is over the stop cap, planned or not.
        self.write_ledger([send_entry("S", 2, 1), send_entry("S", 2, 2), send_entry("L", 2, 3, 2, 1)])
        self.clear_output()
        board = Board()
        status, _, err = self.run_main(position(2, 7, "halt"), Backend(board), Operator("ATTENDED 7"))
        self.assertEqual(status, 2, err)
        self.assertIn("planned stop", err)
        self.assertEqual(board.writes, [])
        # With nobody at the bench a waiver replaces the questions, as for every motion route but enable.
        self.ledger.write_bytes(b"")
        self.clear_output()
        words = ["--command", "word67", "--node", "1", "--trial", "8", "--word6-cycles", "2", "--word7-cycles", "3",
                 "--interval-ms", "100", "--unattended-waiver", "I accept this trial unattended"]
        board = Board()
        operator = Operator()
        status, _, err = self.run_main(words, Backend(board), operator)
        self.assertIn(status, (0, 1), err)
        self.assertEqual((operator.questions, board.writes[-1]), ([], signed("TW1,8,1,2,3,100").encode()))
        self.assertEqual(self.meta()["operator"]["shaft_after"]["typed"], bench.UNOBSERVED)

    def test_batch_two_lines_and_bounds(self):
        limits = ["--command", "settings-2202", "--node", "1", "--trial", "5", "--soft-limit-code", "after-homing",
                  "--fault-led", "unobserved"]
        di3 = ["--command", "io-di3", "--node", "2", "--trial", "6", "--di3-change", "polarity-set",
               "--fault-led", "unobserved"]
        counter = ["--command", "counter-zero", "--node", "1", "--trial", "7", "--deadline-ms", "3000",
                   "--interval-ms", "500"]
        alarm = ["--command", "clear-alarm", "--node", "2", "--trial", "8", "--deadline-ms", "3000",
                 "--interval-ms", "500"]
        watched = list(MOVE) + ["--observer", "2"]
        watched[watched.index("--deadline-ms") + 1] = "6000"
        watched[watched.index("--interval-ms") + 1] = "2000"
        for argv, line, bounds in ((limits, "TD1,5,1,1", (14, 0)), (di3, "TJ2,6,1,3", (28, 0)),
                                   (counter, "TZ1,7,1,3000,500", (66, 3000)), (alarm, "TC2,8,1,3000,500", (56, 3000))):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed(line))
            self.assertEqual(bench.plan_bounds(args.command, fields), bounds, argv[1])
        # The observed move, as test/bench_route_test.cpp runs it: 59 terminals of its own, 44 of node 2.
        args, fields = bench.arguments(BASE + watched + LEDGER + ["--output", "x"])
        self.assertEqual(bench.plan_bounds("move", fields), (103, 6000))
        args, fields = bench.arguments(BASE + MOVE + ["--observer", "2"] + LEDGER + ["--output", "x"])
        self.assertGreater(bench.plan_bounds("move", fields)[0], 128)  # Its default deadline no longer fits.
        self.assertEqual({"Z", "C"} & bench.MOTION_ROUTES, {"Z", "C"})
        self.assertFalse({"D", "J"} & bench.MOTION_ROUTES)
        bad = [limits[:6] + limits[8:], di3[:6] + di3[8:],  # No code, no change.
               ["--command", "settings-2202", "--node", "1", "--trial", "5", "--soft-limit-code", "after-homing"],
               limits + ["--di3-change", "polarity-set"], di3 + ["--soft-limit-code", "limits-off"],
               counter[:-2], alarm[:6] + ["--deadline-ms", "55001", "--interval-ms", "500"],
               MOVE + ["--observer", "1"], MOVE + ["--observer", "3"],
               ["--command", "stop", "--node", "1", "--trial", "1", "--kind", "halt", "--deadline-ms", "2000",
                "--best-effort-ms", "0", "--interval-ms", "200", "--observer", "2"]]
        for argv in bad:
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])

    def test_settings_routes_are_attended_counted_and_not_motion(self):
        limits = ["--command", "settings-2202", "--node", "1", "--trial", "5", "--soft-limit-code", "limits-off",
                  "--fault-led", "prompt"]
        # The campaign is required, as for motion: each send counts.
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            bench.arguments(BASE + limits + ["--output", "x"])
        board = Board()
        operator = Operator("ATTENDED 5", "off", "off")
        status, _, err = self.run_main(limits, Backend(board), operator)
        self.assertIn(status, (0, 1), err)  # The synthetic record has no terminals.
        self.assertEqual(board.writes, [b"p", signed("TD1,5,1,0").encode()])
        self.assertEqual(len(operator.questions), 3)  # Attended, fault LED before and after; no shaft question.
        self.assertIn("ATTENDED 5", operator.questions[0])
        self.assertFalse(any("describe the node" in question for question in operator.questions))
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["motion"], e["stop"], e.get("settings")) for e in send], [("D", 0, 0, 1)])
        # Three per route and node across boots; never part of the twelve motion admissions.
        self.write_ledger([dict(send_entry("D", 1, n, 0), settings=1) for n in (1, 2, 3)])
        self.clear_output()
        board = Board()
        status, _, err = self.run_main(limits[:5] + ["9"] + limits[6:], Backend(board), Operator("ATTENDED 9", "off"))
        self.assertEqual(status, 2, err)
        self.assertIn("settings-2202 admissions on node 1", err)
        self.assertEqual(board.writes, [])
        self.clear_output()
        status, _, err = self.run_main(position(1, 10), Backend(Board()), Operator("ATTENDED 10", "none"))
        self.assertEqual(status, 0, err)
        # A settings send without its mark, or a mark on another route, is an inconsistent entry.
        for entry in (send_entry("D", 1, 4, 0), dict(send_entry("P", 1, 4), settings=1)):
            self.ledger.write_bytes(chain([entry]))
            board = Board()
            status, _, err = self.run_main(position(1, 20), Backend(board), Operator("ATTENDED 20"))
            self.assertEqual(status, 2, err)
            self.assertIn("inconsistent send entry", err)
            self.assertEqual(board.writes, [])

    def test_velocity_home_lines_match_the_firmware_grammar(self):
        cases = {tuple(MODE3): signed("TN1,32,1,2,200"), tuple(VELOCITY): VELOCITY_LINE,
                 tuple(HOME): signed("TO1,31,1,24,60,6,200,0")}
        quick = list(VELOCITY)
        quick[quick.index("--kind") + 1] = "quick"
        quick[quick.index("--layout") + 1] = "op86"
        quick[quick.index("--start-qualified") + 1] = "0"
        cases[tuple(quick)] = signed("TV1,30,1,6,200,200,12000,2000,18000,2,351,4,0,1,262546,18,52,768,34309,65537,134,291")
        for method in (17, 18, 29):
            argv = list(HOME)
            argv[argv.index("--method") + 1] = str(method)
            argv[argv.index("--offset") + 1] = "-1000000"
            cases[tuple(argv)] = signed("TO1,31,1,%d,60,6,200,-1000000" % method)
        for argv, line in cases.items():
            args, fields = bench.arguments(BASE + list(argv) + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), line, argv)
        # Every value is required: none has a default, the evidence least of all.
        for required in (MODE3, VELOCITY, HOME):
            for index in range(required.index("--trial") + 2, len(required), 2):
                argv = required[:index] + required[index + 2:]
                with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(io.StringIO()):
                    bench.arguments(BASE + argv + LEDGER + ["--output", "x"])

        def changed(base, name, value):
            argv = list(base)
            argv[argv.index(name) + 1] = value
            return argv
        bad = [changed(MODE3, "--cycles", "11"), changed(MODE3, "--interval-ms", "99"),
               changed(VELOCITY, "--rpm", "0"), changed(VELOCITY, "--rpm", "3001"),
               changed(VELOCITY, "--ramp-up-ms", "0"), changed(VELOCITY, "--ramp-down-ms", "2001"),
               changed(VELOCITY, "--duration-ms", "18000"), changed(VELOCITY, "--deadline-ms", "55001"),
               changed(VELOCITY, "--interval-ms", "19"), changed(VELOCITY, "--layout", "zh57"),
               changed(VELOCITY, "--start-qualified", "2"), changed(VELOCITY, "--stop-qualified", "-1"),
               changed(VELOCITY, "--record", "0"), changed(VELOCITY, "--hardware-version", "0x10000"),
               changed(HOME, "--method", "19"), changed(HOME, "--search-rpm", "0"),
               changed(HOME, "--creep-rpm", "3001"), changed(HOME, "--ramp-ms", "0"),
               changed(HOME, "--offset", "1000001"),
               VELOCITY + ["--method", "24"], HOME + ["--layout", "zh86"], MODE3 + ["--duration-ms", "12000"],
               MOVE + ["--start-qualified", "1"], HOME + ["--fault-led", "prompt"]]
        for argv in bad:
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        # The longest velocity line fits the firmware's ROUTE_LINE (256 bytes after T).
        widest = list(VELOCITY)
        for name, value in (("--trial", "4294967295"), ("--record", "4294967295"), ("--rpm", "3000"),
                            ("--duration-ms", "54999"), ("--deadline-ms", "55000"), ("--interval-ms", "5000"),
                            ("--ramp-up-ms", "2000"), ("--ramp-down-ms", "2000"), ("--device-type", "0xffffffff"),
                            ("--vendor-id", "0xffffffff"), ("--product-code", "0xffffffff"),
                            ("--revision", "0xffffffff"), ("--hardware-version", "0xffff")):
            widest[widest.index(name) + 1] = value
        args, fields = bench.arguments(BASE + widest + LEDGER + ["--output", "x"])
        self.assertLessEqual(len(bench.encode(args.command, fields)) - 2, 256)
        # Weights as the firmware counts them: the velocity's own stop always counts against stop.
        weights = {}
        for argv in (MODE3, VELOCITY, HOME):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            weights[args.command] = bench.motion_weight(args.command, fields)
        self.assertEqual(weights, {"mode3": (1, 0), "velocity": (2, 1), "home": (0, 0)})
        self.assertEqual({"N", "V"} & bench.MOTION_ROUTES, {"N", "V"})
        self.assertNotIn("O", bench.MOTION_ROUTES)
        self.assertIn("O", bench.SETTINGS_LETTERS)

    def test_velocity_bounds_and_library_minima(self):
        def bounds(argv):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            return bench.plan_bounds(args.command, fields)
        # The numbers test/bench_route_test.cpp pins against BenchRoute's own bounds.
        self.assertEqual(bounds(VELOCITY), (118, 18000))
        self.assertEqual(bounds(MODE3), (35, 400))
        self.assertEqual(bounds(HOME), (29, 0))
        dense = list(VELOCITY)
        dense[dense.index("--interval-ms") + 1] = "100"
        self.assertGreater(bounds(dense)[0], 128)

        def velocity(duration, deadline):
            argv = list(VELOCITY)
            argv[argv.index("--duration-ms") + 1] = str(duration)
            argv[argv.index("--deadline-ms") + 1] = str(deadline)
            return argv
        # The library refuses a duration under 27 services at 100 + 250 ms and two 10 ms enable intervals,
        # and a deadline that leaves 2800 ms or less after it, both after the firmware counted the line
        # (test/bench_route_test.cpp testVelocityRefusals): refused here, unsent and uncounted.
        for argv, reason in ((velocity(9469, 18000), "velocity duration: 9469 ms"),
                             (velocity(12000, 14800), "velocity deadline: 14800 ms leaves 2800 ms"),
                             (dense, "capacity")):
            self.clear_output()
            board = Board()
            operator = Operator("ATTENDED 30", "none")
            status, _, err = self.run_main(argv, Backend(board), operator)
            self.assertEqual(status, 2, err)
            self.assertIn("REFUSED: %s" % reason, err)
            self.assertEqual((board.writes, operator.questions), ([], []))
            self.assertFalse(self.output.exists())
            self.assertEqual(self.ledger.read_bytes(), b"")
        for argv in (velocity(9470, 18000), velocity(12000, 14801)):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            bench.check_bounds(args.command, fields)  # No refusal.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + velocity(9000, 18000) + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(),
                       Clock(), Operator())
        self.assertIn("would refuse before sending: velocity duration: 9000 ms", out.getvalue())
        self.assertIn("startQualified 1, stopQualified 1", out.getvalue())

    def test_velocity_and_mode3_are_attended_and_counted(self):
        board = Board()
        operator = Operator("ATTENDED 30", "turned about one revolution, then stopped")
        status, _, err = self.run_main(VELOCITY, Backend(board), operator)
        self.assertIn(status, (0, 1), err)  # The synthetic record has no terminals.
        self.assertEqual(board.writes, [b"p", VELOCITY_LINE.encode()])
        self.assertIn("ATTENDED 30", operator.questions[0])
        self.assertIn("describe the node 1 shaft", operator.questions[1])
        self.assertEqual(self.meta()["plan"], "prompt 35 finite velocity")
        self.clear_output()
        status, _, err = self.run_main(MODE3, Backend(Board()), Operator("ATTENDED 32", "none"))
        self.assertIn(status, (0, 1), err)
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["node"], e["trial"], e["motion"], e["stop"]) for e in send],
                         [("V", 1, 30, 2, 1), ("N", 1, 32, 1, 0)])
        # Two earlier stops on node 1: a velocity's own would be the fourth, over the stop cap.
        self.write_ledger([send_entry("S", 1, 1), send_entry("S", 1, 2), send_entry("V", 1, 3, 2, 1)])
        self.clear_output()
        board = Board()
        argv = list(VELOCITY)
        argv[argv.index("--trial") + 1] = "40"
        status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 40"))
        self.assertEqual(status, 2, err)
        self.assertIn("planned stop", err)
        self.assertEqual(board.writes, [])
        # A velocity send entry without its stop is inconsistent, and so is a stop on mode3.
        for entry in (dict(send_entry("V", 1, 4, 1, 0)), dict(send_entry("N", 1, 4, 2, 1))):
            entry["line"] = bench.sign_line("T%s1,4,1,0,0,0,0,0" % entry["route"])
            self.ledger.write_bytes(chain([entry]))
            board = Board()
            status, _, err = self.run_main(position(1, 20), Backend(board), Operator("ATTENDED 20"))
            self.assertEqual(status, 2, err)
            self.assertIn("inconsistent send entry", err)
            self.assertEqual(board.writes, [])

    def test_home_is_attended_counted_and_not_motion(self):
        board = Board()
        operator = Operator("ATTENDED 31", "NONE")
        status, _, err = self.run_main(HOME, Backend(board), operator)
        self.assertIn(status, (0, 1), err)
        self.assertEqual(board.writes, [b"p", signed("TO1,31,1,24,60,6,200,0").encode()])
        self.assertEqual(len(operator.questions), 2)  # Attended, the wired inputs; nothing after, no shaft.
        self.assertIn("which DI terminals of node 1 are wired", operator.questions[1])
        self.assertEqual(self.meta()["operator"]["home_inputs_wired"]["typed"], "NONE")
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["motion"], e["stop"], e.get("settings")) for e in send], [("O", 0, 0, 1)])
        # An empty answer refuses before anything is sent.
        self.clear_output()
        board = Board()
        argv = list(HOME)
        argv[argv.index("--trial") + 1] = "32"
        status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 32", ""))
        self.assertEqual(status, 2, err)
        self.assertEqual(board.writes, [])
        # Three per node across boots, never toward the twelve motion admissions.
        self.write_ledger([dict(send_entry("O", 1, n, 0), settings=1) for n in (1, 2, 3)] +
                          [send_entry("M", 1, n) for n in range(4, 15)])
        self.clear_output()
        board = Board()
        argv[argv.index("--trial") + 1] = "40"
        status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 40", "NONE"))
        self.assertEqual(status, 2, err)
        self.assertIn("home admissions on node 1", err)
        self.assertEqual(board.writes, [])
        self.clear_output()
        argv[argv.index("--node") + 1] = "2"
        status, _, err = self.run_main(argv, Backend(Board()), Operator("ATTENDED 40", "NONE"))
        self.assertIn(status, (0, 1), err)  # Eleven motion admissions do not count against home.
        # With nobody at the bench a waiver replaces both questions.
        self.ledger.write_bytes(b"")
        self.clear_output()
        operator = Operator()
        status, _, err = self.run_main(HOME + ["--unattended-waiver", "I accept this unattended"], Backend(Board()),
                                       operator)
        self.assertIn(status, (0, 1), err)
        self.assertEqual(operator.questions, [])
        self.assertEqual(self.meta()["operator"]["home_inputs_wired"]["typed"], bench.UNOBSERVED)

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
    unittest.main()
