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
         "Z": "counter-zero", "C": "clear-alarm", "N": "mode3", "V": "velocity", "O": "home",
         "F": "velocity-target", "K": "subdivision", "T": "controlword"}
ROUTES = {"A": "actions", "I": "io", "U": "tuning", "M": "mode1", "E": "enable", "S": "stop", "P": "position",
          "R": "rearm", "G": "move", "H": "history", "Q": "released-quick", "W": "word67", "L": "relative",
          "D": "settings-2202", "J": "io-di3", "Z": "counter-zero", "C": "clear-alarm", "N": "mode3",
          "V": "velocity", "O": "home", "F": "velocity-target", "K": "subdivision", "T": "controlword"}
# Protocol 6's routes, protocol 8's F, protocol 9's K and protocol 11's T. `bench_route_test --transcript` prints
# the earlier ones only; `--transcript-all` adds these, which scripts/s2_evidence.py reads once its grammar names
# them (see check_trial_transcript).
NEW_LETTERS = frozenset("NVOFKT")


def check_trial_transcript(path, every=False):
    """Every record the real BenchRoute prints must parse whole, with its expected verdict. every: the
    transcript of --transcript-all, which also holds protocol 6's mode3, velocity and home records, protocol
    8's velocity-target records, the 2026-11-10 chain from a released mode 3 drive (mode1, enable, move,
    position), protocol 9's range-probe velocity and subdivision records, protocol 11's controlword records and
    protocol 12's reverse-probe velocity records."""
    data = Path(path).read_bytes()
    problems = []
    parts = data.split(b"=== ")[1:]
    wanted_count = 46 if every else 26
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
        # any write by design, and one velocity line names 61 rpm without the range probe (the field before the
        # last), over the attended experiment's 60, which the library refuses. A subdivision of 25600 on a drive
        # that does not report released is the library's refusal too.
        lost = line.startswith("G2,13,")
        probe = line[0] == "V" and fields[-2] == "1"
        held = line[0] == "K" and fields[3] == "25600"
        refused = line[0] == "O" or (line[0] == "V" and int(fields[3]) > 60 and not probe) or held
        # Backlog S16: word 15's reply lost, then the route's fallback 000B after that failed terminal.
        fallback = line.startswith("V1,9,")
        # Gate Q03: the fake refuses the 3001 range probe with the value range's abort.
        ranged = line[0] == "F" and fields[3] == "3001"
        # After round 10: a released-start enable from the latched release that fails after word 15, then the
        # route's own QUICK_STOP: 21 on a reported move, the sequenced 000B; 22 with word 15's reply lost, a
        # failed terminal and the DEC-2 000B after it.
        enable_stop = line.startswith(("E1,21,", "E1,22,"))
        # Protocol 11: the 0080 on a fake that then reports the power-up pattern ends at its first cycle, marked.
        marked = line[0] == "T" and fields[3] == "128"
        # Protocol 12: a reverse probe (the V line's last field), and trial 42 on a fake that turns against its 60FF:
        # its enable report ends the run, and the run's own HALT follows.
        reverse = line[0] == "V" and fields[-1] == "1"
        wrong_sign = line.startswith("V1,42,")
        wanted = ("failed-evidence" if line.startswith("S1,7,") or lost or refused or fallback or ranged or
                  enable_stop or marked or wrong_sign else "complete")
        if report.verdict != wanted or report.family != "TRIAL":
            problems.append("%s: %s %s %s" % (line, report.verdict, report.family, report.errors[:3]))
        if fallback and not (report.failure_name == "ACTION" and b" needs=1 fenced=1 " in body and
                             body.count(b" data=2b4060000b000000 ") == 1 and
                             b"TRIAL_STOP kind=2 path=2 " in body and b"TRIAL_BEST_EFFORT which=0 own=1 " in body):
            problems.append("%s: not a failed velocity with the route's one fallback 000B" % line)
        if reverse and not (b" reverse=1\n" in body and b" reverseProbe=1\n" in body and b" rpm=-6 " in body and
                            body.count(b" data=23ff6000faffffff ") == 1 and
                            body.count(b" data=2b4060000f010000 ") == 1):
            problems.append("%s: not a -6 rpm reverse probe with one 60FF of FA FF FF FF and its own HALT" % line)
        if wrong_sign and not (report.failure_name == "ACTION" and b" running=0 " in body and
                               b" needs=0 fenced=0 " in body and b"TRIAL_STOP kind=1 path=0 " in body):
            problems.append("%s: not a reverse probe ended by its wrong-sign enable report and its own HALT" % line)
        if fallback:
            # Only stop work may follow a failed terminal: the same record with 000F in the 000B's place is not.
            forged = body.replace(b" data=2b4060000b000000 ", b" data=2b4060000f000000 ")
            if "result_after_failure" not in {item["code"] for item in evidence.replay_bytes(forged, meta).errors}:
                problems.append("%s: a 000F after the failed terminal was accepted" % line)
        if enable_stop:
            path = b"TRIAL_STOP kind=2 path=1 " if line.startswith("E1,21,") else b"TRIAL_STOP kind=2 path=2 "
            if not (report.failure_name == "ACTION" and body.count(b" data=2b4060000b000000 ") == 1 and
                    path in body and b" start=2 shaft=1 " in body and
                    b"TRIAL_ENABLE_STEP word=15 present=0 " in body):
                problems.append("%s: not a failed released enable with the route's one 000B" % line)
            if line.startswith("E1,22,"):
                # Only stop work may follow word 15's failed terminal: a 000F in the 000B's place is not.
                forged = body.replace(b" data=2b4060000b000000 ", b" data=2b4060000f000000 ")
                if "result_after_failure" not in {item["code"] for item in evidence.replay_bytes(forged, meta).errors}:
                    problems.append("%s: a 000F after the failed terminal was accepted" % line)
            elif report.failed:
                problems.append("%s: a failed terminal where none was planned" % line)
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
        # A released-by-recorded-stop enable carries the released report's FREE (2026-11-10).
        if line.startswith("E") and fields[3] == "2" and b" start=2 shaft=1 " not in body:
            problems.append("%s: a released start without its plan" % line)
        # mode1 from a released mode 3 drive reads twice (TRIAL_FIRST) and writes one 6060=1, never 6040.
        if line[0] == "M" and b"TRIAL_FIRST " in body:
            sent = [r.values["data"] for r in report.records if r.kind == "TRAFFIC" and r.status == "parsed" and
                    r.values["k"] == evidence.TX and 0x600 < r.values["id"] <= 0x67F]
            if [data for data in sent if data[:2] not in ("40", "80")] != ["2f60600001000000"]:
                problems.append("%s: not exactly one 6060=1 download" % line)
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
        if line[0] == "F":
            problems += _check_target_record(line, fields, body, meta, report, ranged)
        if line[0] == "O" and not {"TRIAL_STATUS", "TRIAL_IO", "TRIAL_HOME"} <= kinds:
            problems.append("%s: no refusal, input mapping or home summary" % line)
        if line[0] == "O" and (b" dir=1 " in body or b" gate=0 " not in body):
            problems.append("%s: a home record with a download or an open gate" % line)
        run = {"TRIAL_VELOCITY", "TRIAL_VELOCITY_TIME", "TRIAL_VELOCITY_WINDOW", "TRIAL_VELOCITY_SAMPLE", "TRIAL_STOP",
               "TRIAL_BEST_EFFORT"}
        if line[0] == "V" and not ({"TRIAL_CHECK", "TRIAL_VELOCITY_EVIDENCE"} |
                                   ({"TRIAL_STATUS"} if refused else run)) <= kinds:
            problems.append("%s: no check, evidence or velocity summary" % line)
        if line[0] == "V" and refused and kinds & run:
            problems.append("%s: a refused velocity printed a run" % line)
        # Protocol 7: every velocity record names the attended experiment and claims no start or stop.
        if line[0] == "V" and b" gate=2 start=0 stop=0 " not in body:
            problems.append("%s: a velocity record without the attended experiment scope" % line)
        # Protocol 9: the plan and the evidence name the line's range probe; protocol 12 its reverse probe after it.
        if line[0] == "V" and (body.count(b" probe=%s reverse=%s\n" % (fields[-2].encode("ascii"),
                                                                       fields[-1].encode("ascii"))) != 1 or
                               body.count(b" probe=%s reverseProbe=%s\n" % (fields[-2].encode("ascii"),
                                                                            fields[-1].encode("ascii"))) != 1 or
                               report.unknown_fields):
            problems.append("%s: the range and reverse probes are not in its plan and evidence" % line)
        if line[0] == "K":
            problems += _check_subdivision_record(line, fields, body, report, held)
        if line[0] == "T":
            problems += _check_controlword_record(line, fields, body, meta, report, marked)
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


def _check_target_record(line, fields, body, meta, report, ranged):
    """One velocity-target record of the transcript: its summaries, its one 60FF download on the wire, and a
    summary that names other bytes or a readback after the abort is refused as differing from the record."""
    problems = []
    kinds = set(report.kinds)
    if not {"TRIAL_FIRST", "TRIAL_CHECK", "TRIAL_LAST", "TRIAL_CONTROL", "TRIAL_VELOCITY_TARGET"} <= kinds:
        problems.append("%s: no first read, check, last read, control or target summary" % line)
    if not report.inventory_checked:
        problems.append("%s: fixed inventory not checked" % line)
    # Only the one 60FF download on the wire: never 6040, 6060 or another object.
    written = (int(fields[3]) & 0xFFFFFFFF).to_bytes(4, "little").hex().encode("ascii")
    sent = [r.values["data"] for r in report.records if r.kind == "TRAFFIC" and r.status == "parsed" and
            r.values["k"] == evidence.TX and 0x600 < r.values["id"] <= 0x67F]
    downloads = [data for data in sent if data[:2] not in ("40", "80")]
    if downloads != ["23ff6000" + written.decode("ascii")]:
        problems.append("%s: not exactly one 60FF download of its value: %s" % (line, downloads))
    summary = b"TRIAL_VELOCITY_TARGET value=%s tx=%s written=1 " % (fields[3].encode("ascii"), written)
    if summary not in body:
        problems.append("%s: no summary of its write" % line)
    if ranged and not (report.failure_name == "SERVICE" and b" outcome=1 ack=0 uncertain=1 abort=06090030 read=0 "
                       b"size=0 data=00000000 match=0 after=0\n" in body and b" object=60ff:00 dir=0 " not in body):
        problems.append("%s: not an aborted write with no readback" % line)
    if not ranged and b" read=1 size=4 data=%s match=1 after=1\n" % written not in body:
        problems.append("%s: not an acknowledged write with its readback" % line)
    forged = (body.replace(b" abort=06090030 read=0 ", b" abort=06090031 read=0 ") if ranged else
              body.replace(b" data=%s match=1 " % written, b" data=00000000 match=1 "))
    if "velocity_target_differs_from_record" not in {item["code"] for item in evidence.replay_bytes(forged, meta).errors}:
        problems.append("%s: a summary that differs from the record was accepted" % line)
    return problems


def _check_controlword_record(line, fields, body, meta, report, marked):
    """One controlword record of the transcript (protocol 11): its summaries, exactly its one 6040 download on
    the wire, a mark that ends the record with nothing after it, and a summary that differs from the record
    refused as such."""
    problems = []
    kinds = set(report.kinds)
    if not {"TRIAL_CHECK", "TRIAL_LAST", "TRIAL_CONTROL", "TRIAL_CONTROLWORD", "TRIAL_CONTROLWORD_CYCLE"} <= kinds:
        problems.append("%s: no check, last read, control, controlword or cycle summary" % line)
    if not report.inventory_checked:
        problems.append("%s: fixed inventory not checked" % line)
    word = int(fields[3])
    sent = [r.values["data"] for r in report.records if r.kind == "TRAFFIC" and r.status == "parsed" and
            r.values["k"] == evidence.TX and 0x600 < r.values["id"] <= 0x67F]
    downloads = [data for data in sent if data[:2] not in ("40", "80")]
    frame = "2b406000" + word.to_bytes(2, "little").hex() + "0000"
    if downloads != [frame]:
        problems.append("%s: not exactly one 6040 download of its word: %s" % (line, downloads))
    cycles = [r.values for r in report.records if r.kind == "TRIAL_CONTROLWORD_CYCLE" and r.status == "parsed"]
    if marked:
        if not (report.failure_name == "OBSERVATION" and b" cycles=1 marked=1 enabled=1 moving=0 alert=1\n" in body
                and len(cycles) == 1 and report.terminals == 17):
            problems.append("%s: not a record marked at its first cycle with nothing after it" % line)
        if bench.controlword_alert("controlword", [int(fields[0]), int(fields[1])], body) is None:
            problems.append("%s: a marked record without the runner's alert" % line)
    elif not (b" cycles=%s marked=0 enabled=0 moving=0 alert=0\n" % fields[4].encode("ascii") in body and
              len(cycles) == int(fields[4])):
        problems.append("%s: not an unmarked record with every cycle" % line)
    forged = body.replace(b"tx=" + frame.encode("ascii"), b"tx=2b4060000f000000")
    if "controlword_differs_from_record" not in {item["code"] for item in evidence.replay_bytes(forged, meta).errors}:
        problems.append("%s: a summary that differs from the record was accepted" % line)
    return problems


def _check_subdivision_record(line, fields, body, report, held):
    """One subdivision record of the transcript (protocol 9): its summaries, and on the wire only its one 2201
    download, never 2200, 6040 or another object; held is the library's refusal, with no download at all."""
    problems = []
    kinds = set(report.kinds)
    if not {"TRIAL_LAST", "TRIAL_SETTINGS", "TRIAL_DRIVER", "TRIAL_SUBDIVISION"} <= kinds:
        problems.append("%s: no state, settings, driver or subdivision summary" % line)
    value = int(fields[3])
    sent = [r.values["data"] for r in report.records if r.kind == "TRAFFIC" and r.status == "parsed" and
            r.values["k"] == evidence.TX and 0x600 < r.values["id"] <= 0x67F]
    downloads = [data for data in sent if data[:2] not in ("40", "80")]
    drivers = [r.values for r in report.records if r.kind == "TRIAL_DRIVER" and r.status == "parsed"]
    if [v["which"] for v in drivers] != [0, 1, 2]:
        problems.append("%s: not the three DRIVER summaries" % line)
    if held:
        if downloads or b"TRIAL_STATUS code=" not in body or b" written=0 outcome=255 " not in body:
            problems.append("%s: not a refusal with nothing written: %s" % (line, downloads))
        return problems
    if downloads != ["2b012200" + value.to_bytes(2, "little").hex() + "0000"]:
        problems.append("%s: not exactly one 2201 download of its value: %s" % (line, downloads))
    if b"TRIAL_SUBDIVISION requested=%d written=1 outcome=0 ack=1 " % value not in body or \
            [(v["present"], v["2201"]) for v in drivers][1:] != [(True, value), (True, value)]:
        problems.append("%s: not an acknowledged write with its readback and the read after it" % line)
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
        # The reply to x (BenchRoute::closeSessions through formatSessionsLine); None: an image without it.
        self.sessions = ("SESSIONS closed node1=1 node2=0 kept1=60 kept2=00 motion=2; nothing sent; latest "
                         "controlwords dropped; fences, trials and caps kept\n")
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
        elif data == b"x":
            self.queue.extend((self.sessions or "UNKNOWN command; no action\n").encode("ascii"))
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
# measurement. No start or stop flag: the line runs as the library's attended experiment.
VELOCITY = ["--command", "velocity", "--node", "1", "--trial", "30", "--rpm", "6", "--ramp-up-ms", "200",
            "--ramp-down-ms", "200", "--duration-ms", "12000", "--interval-ms", "5000", "--deadline-ms", "15500",
            "--kind", "halt", "--record", "351", "--layout", "zh86", "--device-type", "0x00040192",
            "--hardware-version", "0x12", "--software-version", "0x34", "--vendor-id", "0x300", "--product-code",
            "0x8605", "--revision", "0x10001", "--model-code", "0x86", "--firmware-code", "0x123"]
VELOCITY_LINE = signed("TV1,30,1,6,200,200,12000,5000,15500,1,351,2,262546,18,52,768,34309,65537,134,291,0,0")
# A velocity record's run and stop lines as BenchRoute prints them (test/bench_route_test.cpp's fallback
# cases on its fake, never a measurement). STOP_NONE is trial 1009's: the run asked for its HALT and nothing
# left. STOP_OWN is the run's own HALT after its acknowledged 000F; BEST_LEFT the route's fallback 000B.
RUN_NEEDS = ("TRIAL_VELOCITY own=1 state=3 phase=5 outcome=12 first=12 int=0 setup=1 exec=3 completion=0 stop=7 "
             "sexec=0 uncertain=1 effects=1 needs=1 fenced=1 running=0 cycles=2 polls=0 op=9 sop=0 rpm=6 code=1 "
             "detail=4\n")
RUN_DONE = RUN_NEEDS.replace(" needs=1 fenced=1 ", " needs=0 fenced=0 ")
STOP_NONE = ("TRIAL_STOP kind=1 path=0 final=0 own=0 state=0 outcome=0 exec=0 completion=0 uncertain=0 effects=0 "
             "pred=0000 documented=0 basis=0 observations=0 op=0\n")
STOP_OWN = ("TRIAL_STOP kind=1 path=0 final=4 own=1 state=3 outcome=11 exec=1 completion=0 uncertain=1 effects=1 "
            "pred=000f documented=0 basis=2 observations=3 op=10\n")
STOP_FALLBACK = STOP_NONE.replace("kind=1 path=0 final=0 ", "kind=2 path=2 final=4 ")
BEST_NONE = "TRIAL_BEST_EFFORT which=%d own=0 state=0 outcome=0 exec=0 admitted=0 effects=0 op=0\n"
BEST_LEFT = "TRIAL_BEST_EFFORT which=0 own=1 state=3 outcome=11 exec=3 admitted=1 effects=1 op=11\n"


def velocity_record(trial, *lines):
    """record()'s velocity record with these summary lines after its plan and resources."""
    text = record("velocity", 1, trial).replace(" summaries=2 ", " summaries=%d " % (2 + len(lines)))
    head, end = text.split("END trial ")
    return head + "".join(lines) + "END trial " + end


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

    def stop_record(self, name="stop-1883", node=1, command="stop", trial=1883, header=" failure=0 why=none ",
                    stop="TRIAL_STOP kind=2 path=1 final=4 own=1 state=2 outcome=1 exec=1 completion=1"):
        """An evidence directory of a settled quick stop, in the firmware's format; this test's numbers."""
        directory = Path(self.directory.name) / name
        directory.mkdir()
        (directory / "request.json").write_text(json.dumps({"node": node, "command": command, "trial": trial}),
                                                encoding="utf-8")
        (directory / "rx.txt").write_text(
            "TRIAL admitted route=%s node=%d trial=%d motion=1 case=1; finite typed route, no retry\n"
            "TRIAL id=%d node=%d route=%s%slost=0 terminals=20\n%s uncertain=0\nEND trial terminals=20\n" % (
                command, node, trial, trial, node, command, header, stop), encoding="utf-8")
        return directory

    def test_released_start_enable_reads_its_shaft_basis_from_the_drive(self):
        released = ["--command", "enable", "--node", "1", "--trial", "12", "--status", "0x8050", "--position-target",
                    "5000", "--velocity-target", "6", "--deadline-ms", "10000", "--interval-ms", "1250"]
        record_dir = self.stop_record()
        argv = released + ["--released-by-recorded-stop", str(record_dir)]
        line = signed("TE1,12,1,2,1,32848,5000,6,10000,1250")
        args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        self.assertEqual(bench.encode(args.command, fields).decode("ascii"), line)
        # Exactly one start record; the flag belongs to enable; no waiver and no relayed shaft with it.
        for bad in (released,
                    argv + ["--no-motion-write-recorded"],
                    ["--command", "mode1", "--node", "1", "--trial", "5", "--cycles", "3", "--interval-ms", "200",
                     "--released-by-recorded-stop", str(record_dir)],
                    argv + ["--unattended-waiver", "I am not there"],
                    argv + ["--waiver-covers-motion"],
                    argv + ["--unattended-waiver", "I am not there", "--relayed-power-up-shaft", "FREE",
                            "--relayed-shaft-source", "dialog"],
                    argv + ["--relayed-power-up-shaft", "FREE", "--relayed-shaft-source", "dialog"]):
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + bad + LEDGER + ["--output", "x"])
        # No shaft question: only the attended confirmation before, the usual questions after.
        board = Board()
        operator = Operator("ATTENDED 12", "no movement", "HOLDING")
        status, out, _ = self.run_main(argv, Backend(board), operator)
        self.assertEqual(status, 0, out)
        self.assertEqual(board.writes, [b"p", line.encode()])
        self.assertEqual(len(operator.questions), 3)
        self.assertFalse(any("FREE or HOLDING as felt" in q for q in operator.questions))
        meta = self.meta()
        self.assertEqual(meta["operator"]["power_up_shaft"],
                         {"typed": "FREE", "read_from_drive": True, "basis": bench.RELEASED_SHAFT_BASIS,
                          "accepted": True})
        self.assertEqual(meta["enable_start"], "RELEASED_BY_RECORDED_STOP")
        self.assertEqual(meta["start_record"], {"directory": str(record_dir), "command": "stop", "trial": 1883,
                                                "stated": "nothing but one acknowledged 6060=1 since"})
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([entry.get("start_record") for entry in send], [str(record_dir)])
        # A waiver that explicitly covers unattended motion admits it with no question; the after-route
        # observations are stored as not observed.
        self.clear_output()
        self.ledger.write_bytes(b"")
        board, operator = Board(), Operator()
        waiver = "do a full night soak without my attendance"
        status, out, _ = self.run_main(argv + ["--unattended-waiver", waiver, "--waiver-covers-motion"],
                                       Backend(board), operator)
        self.assertEqual(status, 0, out)
        self.assertEqual((board.writes, operator.questions), ([b"p", line.encode()], []))
        meta = self.meta()
        self.assertEqual(meta["operator"]["attended"]["covers_motion"], True)
        self.assertEqual(meta["operator"]["holding_after"]["typed"], bench.UNOBSERVED)
        self.assertEqual(meta["operator"]["power_up_shaft"]["typed"], "FREE")
        # A record that does not show a settled quick stop of this node refuses before anything is sent.
        bad_records = [self.stop_record("other-node", node=2), self.stop_record("mode1", command="mode1"),
                       self.stop_record("failed", header=" failure=10 why=stop "),
                       self.stop_record("halt", stop="TRIAL_STOP kind=1 path=1 final=4 own=1 state=2 outcome=1"),
                       self.stop_record("not-own", stop="TRIAL_STOP kind=2 path=1 final=4 own=0 state=2 outcome=1"),
                       self.stop_record("unsettled", stop="TRIAL_STOP kind=2 path=1 final=4 own=1 state=3 outcome=9"),
                       Path(self.directory.name) / "absent"]
        for number, directory in enumerate(bad_records, 13):
            self.clear_output()
            board = Board()
            later = list(released)
            later[later.index("--trial") + 1] = str(number)
            status, _, err = self.run_main(later + ["--released-by-recorded-stop", str(directory)],
                                           Backend(board), Operator("ATTENDED %d" % number))
            self.assertEqual(status, 2, directory)
            self.assertIn("released-by-recorded-stop", err)
            self.assertEqual(board.writes, [])
            self.assertFalse(self.output.exists())
        # The dry run states what is checked and what is only stated, and touches nothing.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + argv + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(), Clock(),
                       Operator())
        self.assertIn("start 2 RELEASED_BY_RECORDED_STOP: stop trial 1883 of node 1 records a settled quick stop",
                      out.getvalue())
        self.assertIn("no check here can show it", out.getvalue())
        self.assertIn("no shaft question: field 5 is FREE", out.getvalue())
        self.assertNotIn("require the attendant to type FREE or HOLDING", out.getvalue())
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + released + ["--released-by-recorded-stop", str(bad_records[2]), "--dry-run",
                                          "--campaign", str(self.ledger)], Untouchable(), Clock(), Operator())
        self.assertIn("would refuse before sending: released-by-recorded-stop", out.getvalue())

    def test_released_start_reserves_its_stop_and_reads_retained_stop_records(self):
        # Start 2 reserves the route's own QUICK_STOP (31 terminals, 1500 ms), as BenchRoute's terminalBound and
        # durationBound count it (test/bench_route_test.cpp pins 10000/800 and 10000/1100 as capacity there).
        def bounds(start, deadline, interval):
            return bench.plan_bounds("enable", [1, 7, 1, start, 1, 0x4050, -2000, 45, deadline, interval])
        self.assertEqual(bounds(1, 10000, 800), (14 + 7 * 15, 10000))  # Start 1 unchanged.
        self.assertEqual(bounds(2, 10000, 800), (14 + 7 * 15 + 31, 11500))
        self.assertEqual(bounds(2, 10000, 1250), (14 + 7 * 11 + 31, 11500))
        for interval, fits in ((1100, False), (1112, True), (1250, True)):
            fields = [1, 7, 1, 2, 1, 0x4050, -2000, 45, 10000, interval]
            if fits:
                bench.check_bounds("enable", fields)
            else:
                with self.assertRaises(bench.Refused):
                    bench.check_bounds("enable", fields)
        # The retained records the lead names: move 2108, whose own quick stop settled after the move was
        # cancelled, and stop 1923, the halted-state quick stop. Both are this application's settled stops.
        bench_dir = Path(__file__).resolve().parent.parent / "docs" / "bench"
        for name, node, command, trial in (("20261110T1407Z-s21-c1-move-quick-node1-2108", 1, "move", 2108),
                                           ("20261110T1408Z-s20-c12-stop-quick-node2-1923", 2, "stop", 1923)):
            self.assertEqual(bench.check_stop_record(bench_dir / name, node),
                             {"directory": str(bench_dir / name), "command": command, "trial": trial})
            with self.assertRaises(bench.Refused):
                bench.check_stop_record(bench_dir / name, 3 - node)  # Another node's record.
        # A velocity record whose own QUICK_STOP settled counts as well; its HALT, as in velocity 1922, does not.
        quick = self.stop_record("velocity-quick", command="velocity", trial=1990)
        self.assertEqual(bench.check_stop_record(quick, 1)["command"], "velocity")
        with self.assertRaises(bench.Refused):
            bench.check_stop_record(bench_dir / "20261110T1408Z-s20-c12-velocity-node2-1922", 2)
        # A move record whose stop was a HALT, or whose quick stop did not settle, does not.
        for stop in ("TRIAL_STOP kind=1 path=1 final=4 own=1 state=2 outcome=1",
                     "TRIAL_STOP kind=2 path=1 final=4 own=1 state=3 outcome=11"):
            bad = self.stop_record("move-%s" % stop[16], command="move", trial=1991, stop=stop)
            with self.assertRaises(bench.Refused):
                bench.check_stop_record(bad, 1)
        # The dry run states the latch, the still word 7, the post-15 rule and the route's own stop.
        out = io.StringIO()
        argv = ["--command", "enable", "--node", "1", "--trial", "2110", "--status", "0x4050", "--position-target",
                "-2000", "--velocity-target", "45", "--deadline-ms", "10000", "--interval-ms", "1250",
                "--released-by-recorded-stop", str(bench_dir / "20261110T1407Z-s21-c1-move-quick-node1-2108")]
        with contextlib.redirect_stdout(out):
            bench.main(BASE + argv + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(), Clock(), Operator())
        self.assertIn("start 2 RELEASED_BY_RECORDED_STOP: move trial 2108 of node 1 records a settled quick stop",
                      out.getvalue())
        self.assertIn("running latched admitted", out.getvalue())
        self.assertIn("any 6064 or 606C change after word 15 fails", out.getvalue())
        self.assertIn("one QUICK_STOP follows at once (1500 ms, cycles every 500 ms", out.getvalue())
        self.assertIn("at most 122 terminals of 128 and 11500 ms", out.getvalue())
        self.assertIn(signed("TE1,2110,1,2,1,16464,-2000,45,10000,1250").strip(), out.getvalue())
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + argv[:-3] + ["800", argv[-2], argv[-1], "--dry-run", "--campaign", str(self.ledger)],
                       Untouchable(), Clock(), Operator())
        self.assertIn("would refuse before sending: capacity", out.getvalue())

    def test_enable_alert_after_word_15_without_a_stop(self):
        fields = [1, 30, 1, 2, 1, 0x4050, -2000, 45, 10000, 1250]
        failed = ("TRIAL_ENABLE own=1 state=3 outcome=9 exec=1 completion=0 uncertain=1 effects=1 mask=07 cursor=2 "
                  "observations=4 op=5 pt=-2000 vt=45 s0=4050 m0=0014 p0=-2017\n")
        sent = ("TRIAL_ENABLE_STEP word=15 present=0 status=0000 monitor=0000 position=0 velocity=0 history=1 "
                "outcome=0 ack=1 possible=1 uncertain=0\n")
        unsent = sent.replace(" history=1 outcome=0 ack=1 possible=1 ", " history=0 outcome=255 ack=0 possible=0 ")
        stop = ("TRIAL_STOP kind=2 path=1 final=4 own=1 state=2 outcome=1 exec=1 completion=1 uncertain=0 effects=1 "
                "pred=000f documented=0 basis=3 observations=1 op=6\n")
        alert = "node 1 MAY BE MOVING: the enable failed after word 15 and no stop left; use the cutoff"

        def enable_record(trial, *lines):
            text = record("enable", 1, trial).replace(" summaries=2 ", " summaries=%d " % (2 + len(lines)))
            head, end = text.split("END trial ")
            return (head + "".join(lines) + "END trial " + end).encode("ascii")
        for lines, wanted in (((failed, sent), alert),  # No stop line: nothing shows a stop that left.
                              ((failed, sent, STOP_NONE, BEST_NONE % 0, BEST_NONE % 1), alert),
                              ((failed, sent, STOP_FALLBACK, BEST_NONE % 0, BEST_NONE % 1), alert),
                              ((failed, sent, stop, BEST_NONE % 0, BEST_NONE % 1), None),
                              ((failed, sent, STOP_FALLBACK, BEST_LEFT, BEST_NONE % 1), None),
                              ((failed, unsent), None),  # Word 15 never left: the drive is at most switched on.
                              ((failed.replace(" state=3 ", " state=2 "), sent), None),  # Succeeded.
                              ((failed.replace(" own=1 ", " own=0 "), sent), None)):  # Another record's enable.
            self.assertEqual(bench.enable_alert("enable", fields, enable_record(30, *lines)), wanted, lines)
        # Start 1 has no route stop and no alert; another trial's record is not this one.
        start1 = list(fields)
        start1[3] = 1
        self.assertIsNone(bench.enable_alert("enable", start1, enable_record(30, failed, sent)))
        self.assertIsNone(bench.enable_alert("enable", fields, enable_record(29, failed, sent)))
        self.assertIsNone(bench.enable_alert("velocity", fields, enable_record(30, failed, sent)))
        # A run: the alert reaches the attendant before the questions after the route, and the exit is 4.
        record_dir = self.stop_record()
        board = Board()
        board.answer = enable_record(30, failed, sent).decode("ascii")
        argv = BASE + ["--command", "enable", "--node", "1", "--trial", "30", "--status", "0x4050", "--position-target",
                       "-2000", "--velocity-target", "45", "--deadline-ms", "10000", "--interval-ms", "1250",
                       "--released-by-recorded-stop", str(record_dir), "--campaign", str(self.ledger),
                       "--output", str(self.output)]
        err = io.StringIO()
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            status = bench.main(argv, Backend(board), Clock(), Operator("ATTENDED 30", "no movement", "HOLDING"))
        self.assertEqual(status, 4, err.getvalue())
        self.assertEqual(err.getvalue().count("ALERT: " + alert + "\n"), 2)
        self.assertEqual(self.meta()["enable_alert"], alert)
        self.assertNotIn("velocity_alert", self.meta())

    def test_mode1_line_unchanged_and_its_released_start_bound(self):
        argv = ["--command", "mode1", "--node", "1", "--trial", "5", "--cycles", "4", "--interval-ms", "200"]
        args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed("TM1,5,1,4,200"))
        # test/bench_route_test.cpp pins 15 + 9 + 1 + 9 terminals for one cycle after a released start.
        self.assertEqual(bench.plan_bounds("mode1", fields), (15 + 9 + 1 + 9 + 7 * 3, 800))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + argv + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(), Clock(), Operator())
        self.assertIn("mode 3, a second full read", out.getvalue())
        self.assertIn("never 6040", out.getvalue())

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
            # Older images, protocols 7 to 11 among them, and one this runner does not know.
            for routes in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13):
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
                 "--interval-ms", "500", "--method", "2301"]
        watched = list(MOVE) + ["--observer", "2"]
        watched[watched.index("--deadline-ms") + 1] = "6000"
        watched[watched.index("--interval-ms") + 1] = "2000"
        for argv, line, bounds in ((limits, "TD1,5,1,1", (14, 0)), (di3, "TJ2,6,1,3", (28, 0)),
                                   (counter, "TZ1,7,1,3000,500", (66, 3000)), (alarm, "TC2,8,1,3000,500,1", (58, 3000))):
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
        cases[tuple(quick)] = signed("TV1,30,1,6,200,200,12000,5000,15500,2,351,4,262546,18,52,768,34309,65537,134,291,"
                                     "0,0")
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
               changed(VELOCITY, "--record", "0"), changed(VELOCITY, "--hardware-version", "0x10000"),
               changed(HOME, "--method", "19"), changed(HOME, "--search-rpm", "0"),
               changed(HOME, "--creep-rpm", "3001"), changed(HOME, "--ramp-ms", "0"),
               changed(HOME, "--offset", "1000001"),
               VELOCITY + ["--method", "24"], HOME + ["--layout", "zh86"], MODE3 + ["--duration-ms", "12000"],
               MOVE + ["--start-qualified", "1"], HOME + ["--fault-led", "prompt"]]
        for argv in bad:
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        # No record of this fixture measured a mode 3 start or stop, so neither flag can be stated, with any
        # value: the line has no such field (protocol 7), and the run is the library's attended experiment.
        for name in ("--start-qualified", "--stop-qualified"):
            for value in ("1", "0"):
                err = io.StringIO()
                with self.assertRaises(SystemExit), contextlib.redirect_stderr(err):
                    bench.arguments(BASE + VELOCITY + [name, value] + LEDGER + ["--output", "x"])
                self.assertIn("--start-qualified and --stop-qualified are refused", err.getvalue())
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
        # Backlog S17: six window cycles, three running cycles and a stop span shorter than one interval.
        self.assertEqual(bounds(VELOCITY), (9 + 9 + 3 + 3 + 21 + 7 * 6 + 7 * 3 + 10, 15500))
        self.assertEqual(bounds(VELOCITY)[0], 118)
        self.assertEqual(bench.VELOCITY_WINDOW_CYCLES, 6)
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
        # The library refuses a duration under 27 services at 100 + 250 ms, the 2000 ms enable report window
        # and two 10 ms enable intervals, and a deadline that leaves 2800 ms or less after it, both after the
        # firmware counted the line (test/bench_route_test.cpp testVelocityRefusals): refused here, unsent and
        # uncounted. 1308's line (2000 ms interval, 18000 ms deadline) no longer fits the record with the
        # window: 45 + 42 + 42 + 31 = 160 terminals.
        old = list(velocity(12000, 18000))
        old[old.index("--interval-ms") + 1] = "2000"
        self.assertEqual(bounds(old)[0], 160)
        for argv, reason in ((velocity(11469, 15500), "velocity duration: 11469 ms"),
                             (velocity(12000, 14800), "velocity deadline: 14800 ms leaves 2800 ms"),
                             (dense, "capacity"), (old, "capacity: this plan can produce 160 terminals")):
            self.clear_output()
            board = Board()
            operator = Operator("ATTENDED 30", "none")
            status, _, err = self.run_main(argv, Backend(board), operator)
            self.assertEqual(status, 2, err)
            self.assertIn("REFUSED: %s" % reason, err)
            self.assertEqual((board.writes, operator.questions), ([], []))
            self.assertFalse(self.output.exists())
            self.assertEqual(self.ledger.read_bytes(), b"")
        for argv in (velocity(11470, 15500), velocity(12000, 14801), velocity(15000, 19999)):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            bench.check_bounds(args.command, fields)  # No refusal.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + velocity(9000, 18000) + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(),
                       Clock(), Operator())
        self.assertIn("would refuse before sending: velocity duration: 9000 ms", out.getvalue())
        self.assertIn("enable report window 2000 ms, cycles 300 ms apart, at most 6 more after word 15", out.getvalue())
        self.assertIn("VelocityEvidence gate ATTENDED_EXPERIMENT, startQualified 0, stopQualified 0", out.getvalue())

    def test_velocity_experiment_bounds_and_attendant(self):
        def changed(**values):
            argv = list(VELOCITY)
            for name, value in values.items():
                argv[argv.index("--" + name.replace("_", "-")) + 1] = str(value)
            return argv
        # The attended experiment's bounds, the library's: 60 rpm and 20000 ms. The library would refuse
        # them only after the firmware counted the line, so they are refused here, unsent and uncounted.
        # With the enable report window (S17) no duration over 15000 ms fits the record at the longest
        # interval, so the 20000 ms bound is reached as capacity first.
        for argv, reason in ((changed(rpm=61), "velocity rpm: 61 is over the ATTENDED_EXPERIMENT scope's 60 rpm"),
                             (changed(duration_ms=20001, deadline_ms=26000, interval_ms=5000),
                              "capacity: this plan can produce 139 terminals")):
            self.clear_output()
            board = Board()
            operator = Operator("ATTENDED 30", "none")
            status, _, err = self.run_main(argv, Backend(board), operator)
            self.assertEqual(status, 2, err)
            self.assertIn("REFUSED: %s" % reason, err)
            self.assertEqual((board.writes, operator.questions), ([], []))
            self.assertFalse(self.output.exists())
            self.assertEqual(self.ledger.read_bytes(), b"")
        for argv in (changed(rpm=60), changed(duration_ms=15000, deadline_ms=18000, interval_ms=5000)):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            bench.check_bounds(args.command, fields)  # No refusal at the bounds themselves.
        # Nobody at the bench is no option: the attendant's cutoff is the only control the run lacks.
        err = io.StringIO()
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(err):
            bench.arguments(BASE + VELOCITY + ["--unattended-waiver", "I accept it unattended"] + LEDGER +
                            ["--output", "x"])
        self.assertIn("--unattended-waiver refuses velocity", err.getvalue())
        # Unless the user's statement explicitly covers unattended motion (2026-11-11): then the line is
        # admitted with the same scope and bounds, nobody is asked, and both records say so.
        waiver = "do a full night soak without my attendance"
        for bad in (VELOCITY + ["--waiver-covers-motion"], ["--command", "ping", "--waiver-covers-motion"]):
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + bad + LEDGER + ["--output", "x"])
        self.clear_output()
        board = Board()
        status, _, err = self.run_main(changed(rpm=61) + ["--unattended-waiver", waiver, "--waiver-covers-motion"],
                                       Backend(board), Operator())
        self.assertEqual(status, 2, err)
        self.assertIn("REFUSED: velocity rpm: 61 is over the ATTENDED_EXPERIMENT scope's 60 rpm", err)
        self.assertEqual(board.writes, [])
        self.clear_output()
        self.ledger.write_bytes(b"")
        operator = Operator()
        status, _, err = self.run_main(VELOCITY + ["--unattended-waiver", waiver, "--waiver-covers-motion"],
                                       Backend(Board()), operator)
        self.assertIn(status, (0, 1), err)
        self.assertEqual(operator.questions, [])
        self.assertEqual(self.meta()["operator"]["attended"],
                         {"unattended": True, "waiver": waiver, "accepted": True, "covers_motion": True})
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(entry.get("unattended_waiver"), entry.get("waiver_covers_motion"), entry.get("scope"))
                          for entry in send], [(waiver, True, "ATTENDED_EXPERIMENT")])
        # The scope is recorded in request.json and in the send entry.
        self.clear_output()
        self.ledger.write_bytes(b"")
        status, _, err = self.run_main(VELOCITY, Backend(Board()), Operator("ATTENDED 30", "none"))
        self.assertIn(status, (0, 1), err)
        self.assertEqual(self.meta()["velocity_scope"],
                         {"gate": "ATTENDED_EXPERIMENT", "start_qualified": False, "stop_qualified": False,
                          "record": 351, "range_probe": False, "maximum_rpm": 60, "maximum_duration_ms": 20000,
                          "reverse_probe": False, "reverse_qualified": False, "rate_rpm": 6})
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([entry.get("scope") for entry in send], ["ATTENDED_EXPERIMENT"])

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

    def test_velocity_alert_reads_only_this_record(self):
        fields = [1, 30]
        alert = "node 1 MAY BE RUNNING: no stop left; use the cutoff"
        for lines, wanted in (((RUN_NEEDS, STOP_NONE, BEST_NONE % 0, BEST_NONE % 1), alert),  # Trial 1009.
                              ((RUN_NEEDS,), alert),  # No stop line: nothing shows a stop that left.
                              ((RUN_NEEDS, STOP_FALLBACK, BEST_NONE % 0, BEST_NONE % 1), alert),  # Refused.
                              ((RUN_NEEDS.replace(" needs=1 ", " needs=0 "), STOP_NONE), alert),  # Fenced only.
                              ((RUN_NEEDS, STOP_NONE.replace(" own=0 ", " own=1 ")), alert),  # exec=0.
                              ((RUN_NEEDS, STOP_OWN.replace(" own=1 ", " own=0 ")), alert),  # Another's stop.
                              ((RUN_NEEDS, STOP_OWN, BEST_NONE % 0, BEST_NONE % 1), None),
                              ((RUN_NEEDS, STOP_FALLBACK, BEST_LEFT, BEST_NONE % 1), None),
                              ((RUN_DONE, STOP_NONE, BEST_NONE % 0), None)):
            data = velocity_record(30, *lines).encode("ascii")
            self.assertEqual(bench.velocity_alert("velocity", fields, data), wanted, lines)
        # Another trial's record, a line after END and another route are not this record.
        stale = velocity_record(29, RUN_NEEDS, STOP_NONE).encode("ascii")
        self.assertIsNone(bench.velocity_alert("velocity", fields, stale))
        after = velocity_record(30, STOP_OWN) + RUN_NEEDS
        self.assertIsNone(bench.velocity_alert("velocity", fields, after.encode("ascii")))
        self.assertIsNone(bench.velocity_alert("move", fields, velocity_record(30, RUN_NEEDS).encode("ascii")))
        other = velocity_record(30, RUN_NEEDS).replace(" node=1 ", " node=2 ").encode("ascii")
        self.assertEqual(bench.velocity_alert("velocity", [2, 30], other),
                         "node 2 MAY BE RUNNING: no stop left; use the cutoff")
        self.assertIn("exits 4", bench.__doc__)

    def test_velocity_alert_reaches_the_attendant_first_and_exits_4(self):
        board = Board()
        # A record of the same trial already waiting before the ping is not this run's.
        board.queue = bytearray(velocity_record(30, RUN_DONE, STOP_OWN).encode("ascii"))
        board.answer = velocity_record(30, RUN_NEEDS, STOP_NONE, BEST_NONE % 0, BEST_NONE % 1)
        out, err = io.StringIO(), io.StringIO()
        seen = []

        def ask(question):
            seen.append((question, err.getvalue()))
            return "turning" if question.startswith("Attendant: describe") else "ATTENDED 30"
        argv = BASE + VELOCITY + ["--campaign", str(self.ledger), "--output", str(self.output)]
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = bench.main(argv, Backend(board), Clock(), ask)
        self.assertEqual(status, 4, err.getvalue())
        alert = "ALERT: node 1 MAY BE RUNNING: no stop left; use the cutoff\n"
        shaft = [before for question, before in seen if question.startswith("Attendant: describe the node 1 shaft")]
        self.assertEqual(len(shaft), 1)
        self.assertIn(alert, shaft[0])  # Printed before the attendant is asked about the shaft.
        self.assertEqual(err.getvalue().count(alert), 2)  # And again after the record's dump.
        self.assertEqual(self.meta()["velocity_alert"], alert[len("ALERT: "):-1])
        # A stop of this record left: no alert, and the ordinary status.
        for lines in ((RUN_NEEDS, STOP_OWN, BEST_NONE % 0, BEST_NONE % 1),
                      (RUN_NEEDS, STOP_FALLBACK, BEST_LEFT, BEST_NONE % 1)):
            self.clear_output()
            self.ledger.write_bytes(b"")
            board = Board()
            board.answer = velocity_record(30, *lines)
            status, _, err_text = self.run_main(VELOCITY, Backend(board), Operator("ATTENDED 30", "none"))
            self.assertNotEqual(status, 4, err_text)
            self.assertNotIn("MAY BE RUNNING", err_text)
            self.assertNotIn("velocity_alert", self.meta())

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

    def test_velocity_target_line_bounds_dry_run_and_campaign(self):
        def probe(value, node="1", trial="50"):
            return ["--command", "velocity-target", "--node", node, "--trial", trial, "--value", str(value)]
        # The firmware's grammar (BenchRoute.cpp PROBE_FIELDS): node, trial, attended and the signed value.
        for value, line in ((3001, "TF1,50,1,3001"), (-6, "TF1,50,1,-6"), (6, "TF1,50,1,6"), (-3000, "TF1,50,1,-3000"),
                            (0, "TF1,50,1,0")):
            args, fields = bench.arguments(BASE + probe(value) + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed(line))
            self.assertEqual(bench.plan_bounds(args.command, fields), (29, 0))
            self.assertEqual(bench.motion_weight(args.command, fields), (0, 0))
        # Outside -3000..3001, missing, or on another route: refused by the parser, nothing created.
        for argv in (probe(3002), probe(-3001), probe(2 ** 31), ["--command", "velocity-target", "--node", "1",
                                                                     "--trial", "50"],
                     MODE3 + ["--value", "6"], probe(6) + ["--fault-led", "prompt"], probe(6, node="3")):
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        err = io.StringIO()
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(err):
            bench.arguments(BASE + probe(3002) + LEDGER + ["--output", "x"])
        self.assertIn("--value must be -3000..3001", err.getvalue())
        # Attended and counted like the settings routes: the campaign is required.
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            bench.arguments(BASE + probe(6) + ["--output", "x"])
        # The dry run touches nothing and states the bytes and the reads.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = bench.main(BASE + probe(-6) + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(), Clock(),
                                Operator())
        self.assertEqual(status, 0)
        text = out.getvalue()
        for part in ("require the operator to type: ATTENDED 50", "at most 29 terminals of 128 and 0 ms",
                     "two fresh full reads both report released standstill in mode 3",
                     "one download 23 FF 60 00 FA FF FF FF (value -6, four bytes two's complement)",
                     "only if it is acknowledged one 60FF upload", "never 6040, 6060 or another object",
                     "routes=12", repr(signed("TF1,50,1,-6").encode("ascii"))):
            self.assertIn(part, text)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.ledger.read_bytes(), b"")
        # One send: the attended confirmation only, a settings entry with no motion weight, then the record.
        board = Board()
        operator = Operator("ATTENDED 50")
        status, _, err_text = self.run_main(probe(3001), Backend(board), operator)
        self.assertIn(status, (0, 1), err_text)  # The synthetic record has no terminals.
        self.assertEqual(board.writes, [b"p", signed("TF1,50,1,3001").encode()])
        self.assertEqual(len(operator.questions), 1)
        self.assertEqual(self.meta()["plan"], "gate Q03 60FF encoding probe")
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["motion"], e["stop"], e.get("settings")) for e in send], [("F", 0, 0, 1)])
        # Three per node across boots, never toward the twelve motion admissions; node 2 has its own count.
        self.write_ledger([dict(send_entry("F", 1, n, 0), settings=1) for n in (1, 2, 3)] +
                          [send_entry("M", 1, n) for n in range(4, 16)])
        self.clear_output()
        board = Board()
        status, _, err_text = self.run_main(probe(6, trial="60"), Backend(board), Operator("ATTENDED 60"))
        self.assertEqual(status, 2, err_text)
        self.assertIn("velocity-target admissions on node 1, the cap is 3", err_text)
        self.assertEqual(board.writes, [])
        self.clear_output()
        status, _, err_text = self.run_main(probe(6, node="2", trial="60"), Backend(Board()), Operator("ATTENDED 60"))
        self.assertIn(status, (0, 1), err_text)  # Twelve motion admissions do not count against it.
        # An F send without its settings mark is an inconsistent entry.
        self.ledger.write_bytes(chain([send_entry("F", 1, 4, 0)]))
        board = Board()
        status, _, err_text = self.run_main(position(1, 20), Backend(board), Operator("ATTENDED 20"))
        self.assertEqual(status, 2, err_text)
        self.assertIn("inconsistent send entry", err_text)
        # An image that speaks protocol 7 takes no F line.
        self.ledger.write_bytes(b"")
        self.clear_output()
        board = Board()
        board.routes = 7
        status, _, err_text = self.run_main(probe(6, trial="70"), Backend(board), Operator("ATTENDED 70"))
        self.assertEqual((status, board.writes, self.meta()["reason"]), (2, [b"p"], "image_route_version"))

    def test_protocol_nine_refuses_older_images(self):
        # Protocol 9 added the V line's range probe field and the K route: an image that speaks 8 would read a V
        # line one field long and know no K, so nothing is sent to it. Protocol 10 changed only the C line, 11
        # added T, 12 the V line's reverse field.
        self.assertEqual(bench.ROUTE_PROTOCOL, 12)
        for argv, trial in ((VELOCITY, "30"), (["--command", "subdivision", "--node", "1", "--trial", "40",
                                                 "--value", "10000"], "40")):
            self.clear_output()
            self.ledger.write_bytes(b"")
            board = Board()
            board.routes = 8
            status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED " + trial, "none"))
            self.assertEqual((status, board.writes, self.meta()["reason"]), (2, [b"p"], "image_route_version"), err)

    def test_clear_alarm_names_its_method(self):
        # Protocol 10: the C line's last field is the reset method, 1 for 2301 (2301 1, 0) and 2 for the
        # controlword (6040 0x0080, 0x0000). It has no default, request.json and the send entry name it, and an
        # image that speaks protocol 9 is refused before any line.
        alarm = ["--command", "clear-alarm", "--node", "1", "--trial", "9", "--deadline-ms", "3000",
                 "--interval-ms", "500"]
        for method, line in (("2301", "TC1,9,1,3000,500,1"), ("controlword", "TC1,9,1,3000,500,2")):
            args, fields = bench.arguments(BASE + alarm + ["--method", method] + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed(line))
            self.assertEqual(bench.plan_bounds(args.command, fields), (58, 3000))
            self.assertEqual(bench.motion_weight(args.command, fields), (1, 0))
        counter = ["--command", "counter-zero", "--node", "1", "--trial", "7", "--deadline-ms", "3000",
                   "--interval-ms", "500"]
        for argv in (alarm, alarm + ["--method", "24"], alarm + ["--method", "6040"], alarm + ["--method", ""],
                     counter + ["--method", "2301"], ["controlword" if a == "24" else a for a in HOME],
                     MODE3 + ["--method", "2301"]):
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(io.StringIO()):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = bench.main(BASE + alarm + ["--method", "controlword", "--dry-run", "--campaign", str(self.ledger)],
                                Untouchable(), Clock(), Operator())
        self.assertEqual(status, 0)
        for part in ("clear method controlword: after the library's admission, 2B 40 60 00 80 00 00 00, then "
                     "2B 40 60 00 00 00 00 00 only after the first one's acknowledgement", "routes=12",
                     repr(signed("TC1,9,1,3000,500,2").encode("ascii"))):
            self.assertIn(part, out.getvalue())
        self.assertFalse(self.output.exists())
        for trial, (method, word) in enumerate((("2301", "1"), ("controlword", "2")), 9):
            self.clear_output()
            board = Board()
            argv = list(alarm) + ["--method", method]
            argv[argv.index("--trial") + 1] = str(trial)
            status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED %d" % trial, "none"))
            self.assertIn(status, (0, 1), err)  # The synthetic record has no terminals.
            self.assertEqual(board.writes, [b"p", signed("TC1,%d,1,3000,500,%s" % (trial, word)).encode()])
            self.assertEqual(self.meta()["clear_method"], method)
            send = [entry for entry in self.entries() if entry["event"] == "send"]
            self.assertEqual((send[-1]["route"], send[-1]["clear_method"], send[-1]["motion"]), ("C", method, 1))
        # An image of protocol 9 would read a sixth field as trailing text: nothing is sent to it.
        self.clear_output()
        board = Board()
        board.routes = 9
        argv = list(alarm) + ["--method", "2301"]
        argv[argv.index("--trial") + 1] = "11"  # Above the campaign's trials 9 and 10.
        status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 11"))
        self.assertEqual((status, board.writes, self.meta()["reason"]), (2, [b"p"], "image_route_version"), err)
        # A send entry whose clear_method differs from its line's field is inconsistent; one without it, from
        # protocol 9, is not.
        entry = {"event": "send", "route": "C", "node": 1, "trial": 4, "motion": 1, "stop": 0,
                 "line": bench.sign_line("TC1,4,1,3000,500,1")}
        self.assertIsNone(bench._send_problem(dict(entry, clear_method="2301")))
        self.assertIsNone(bench._send_problem(dict(entry, line=bench.sign_line("TC1,4,1,3000,500"))))
        for bad in (dict(entry, clear_method="controlword"), dict(entry, clear_method="6040"),
                    dict(entry, clear_method="2301", line=bench.sign_line("TC1,4,1,3000,500"))):
            self.assertIn("clear_method", bench._send_problem(bad))
        self.write_ledger([dict(entry, clear_method="controlword")])
        self.clear_output()
        status, _, err = self.run_main(position(1, 20), Backend(Board()), Operator("ATTENDED 20"))
        self.assertEqual(status, 2, err)
        self.assertIn("inconsistent send entry", err)

    def test_controlword_line_bounds_dry_run_and_campaign(self):
        def word(value, node="1", trial="40", cycles="3", interval="500"):
            return ["--command", "controlword", "--node", node, "--trial", trial, "--word", value, "--cycles", cycles,
                    "--interval-ms", interval]
        # The firmware's grammar (BenchRoute.cpp CONTROLWORD_FIELDS): node, trial, attended, the word in decimal, the
        # cycles and their spacing. Only the reviewed set, as the firmware's REVIEWED_CONTROLWORDS and the parser's.
        self.assertEqual(sorted(bench.CONTROLWORDS), [0x0000, 0x0006, 0x000B, 0x0080])
        self.assertEqual(set(bench.CONTROLWORDS), set(evidence.CONTROLWORDS))
        self.assertTrue(all(value & 0x0007 != 0x0007 for value in bench.CONTROLWORDS))
        for text, decimal in (("0x0000", 0), ("0x0006", 6), ("0x000B", 11), ("0x80", 128), ("6", 6), ("0", 0)):
            args, fields = bench.arguments(BASE + word(text) + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed("TT1,40,1,%d,3,500" % decimal))
            self.assertEqual(bench.plan_bounds(args.command, fields), (9 + 1 + 3 * 7 + 9, 1500))
            self.assertEqual(bench.motion_weight(args.command, fields), (1, 0))
            bench.check_bounds(args.command, fields)
        args, fields = bench.arguments(BASE + word("6", cycles="10", interval="2000") + LEDGER + ["--output", "x"])
        self.assertEqual(bench.plan_bounds(args.command, fields), (89, 20000))
        self.assertEqual(bench.TRIALS["controlword"][0], "T")
        self.assertIn("T", bench.MOTION_ROUTES)  # Motion class, as every 6040 write.
        self.assertNotIn("T", bench.SETTINGS_LETTERS)
        self.assertEqual(evidence.TRIAL_ROUTES["controlword"], "controlword")
        # Every word that could switch on or enable, and every unreviewed word: refused, nothing created.
        for text in ("0x0007", "0x000F", "0x001F", "0x010F", "0x0001", "0x0002", "0x0086", "0x008F", "0xFFFF", "-1",
                     "7", "15", "128000"):
            err = io.StringIO()
            with self.assertRaises(SystemExit, msg=text), contextlib.redirect_stderr(err):
                bench.arguments(BASE + word(text) + LEDGER + ["--output", "x"])
            self.assertIn("is refused: only the reviewed non-enabling words", err.getvalue())
        missing = [a for a in word("6") if a not in ("--word", "6")]
        for argv, text in ((word("6", cycles="0"), "--cycles must be 1..10"),
                           (word("6", cycles="11"), "--cycles must be 1..10"),
                           (word("6", interval="99"), "--interval-ms must be 100..2000"),
                           (word("6", interval="2001"), "--interval-ms must be 100..2000"),
                           (missing, "--word is required for this route"),
                           (MODE3 + ["--word", "6"], "--word belongs to controlword"),
                           (word("6") + ["--fault-led", "prompt"], "--fault-led belongs to"),
                           (word("6", node="3"), "--node must be 1..2")):
            err = io.StringIO()
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(err):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            self.assertIn(text, err.getvalue())
        # Motion class: the campaign is required.
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            bench.arguments(BASE + word("6") + ["--output", "x"])
        # The dry run touches nothing and states the admission, the bytes and the alert.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = bench.main(BASE + word("0x80") + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(),
                                Clock(), Operator())
        self.assertEqual(status, 0)
        text = out.getvalue()
        for part in ("require the operator to type: ATTENDED 40", "at most 40 terminals of 128 and 1500 ms",
                     "6041 bits 0..2 clear (bit 3 either way), 2100 running clear, 606C 0 and mode 0, 1, 3 or 6",
                     "one download 2B 40 60 00 80 00 00 00 (0x0080, fault reset, a bit 7 edge from 0), 3 "
                     "STATUS/MONITORS/FEEDBACK cycles 500 ms apart and one more full read",
                     "no second word, no retry, no enable, no 6060, 60FF or 2301 write", "runner prints ALERT and exits 4",
                     "routes=12", repr(signed("TT1,40,1,128,3,500").encode("ascii"))):
            self.assertIn(part, text)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.ledger.read_bytes(), b"")
        # One send: the attended confirmation, a motion entry that names its word, the record, the shaft question.
        board = Board()
        operator = Operator("ATTENDED 40", "none")
        status, _, err = self.run_main(word("0x0006"), Backend(board), operator)
        self.assertIn(status, (0, 1), err)  # The synthetic record has no terminals.
        self.assertEqual(board.writes, [b"p", signed("TT1,40,1,6,3,500").encode()])
        self.assertEqual((self.meta()["controlword"], self.meta()["plan"]),
                         ("0x0006", "protocol 11 controlword from low bits 000"))
        self.assertNotIn("controlword_alert", self.meta())
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["motion"], e["stop"], e.get("settings"), e["controlword"]) for e in send],
                         [("T", 1, 0, None, "0x0006")])
        # Three per node across boots, counted toward the twelve motion admissions; node 2 has its own count.
        entries = [{"event": "send", "route": "T", "node": 1, "trial": n, "motion": 1, "stop": 0,
                    "controlword": "0x0000", "line": bench.sign_line("TT1,%d,1,0,3,500" % n)} for n in (1, 2, 3)]
        self.write_ledger(entries)
        self.clear_output()
        board = Board()
        status, _, err = self.run_main(word("0", trial="60"), Backend(board), Operator("ATTENDED 60", "none"))
        self.assertEqual(status, 2, err)
        self.assertIn("the campaign holds 3 controlword admissions on node 1, the cap is 3", err)
        self.assertEqual(board.writes, [])
        self.clear_output()
        status, _, err = self.run_main(word("0", node="2", trial="60"), Backend(Board()), Operator("ATTENDED 60", "none"))
        self.assertIn(status, (0, 1), err)
        self.assertEqual(self.entries()[-2]["motion"], 1)
        # A T entry is consistent only with a reviewed word that its line and its controlword name alike.
        self.assertIsNone(bench._send_problem(entries[0]))
        for bad in (dict(entries[0], controlword="0x0006"),
                    dict(entries[0], line=bench.sign_line("TT1,1,1,7,3,500"), controlword="0x0007"),
                    dict(entries[0], line=bench.sign_line("TT1,1,1,15,3,500"), controlword="0x000F"),
                    dict(entries[0], line=bench.sign_line("TT1,1,1,0,3"), controlword="0x0000"),
                    {k: v for k, v in entries[0].items() if k != "controlword"}):
            self.assertIn("controlword", bench._send_problem(bad))
        # An image of protocol 10 knows no T route: nothing is sent to it.
        self.ledger.write_bytes(b"")
        self.clear_output()
        board = Board()
        board.routes = 10
        status, _, err = self.run_main(word("6", trial="70"), Backend(board), Operator("ATTENDED 70", "none"))
        self.assertEqual((status, board.writes, self.meta()["reason"]), (2, [b"p"], "image_route_version"), err)

    def test_controlword_alert_reaches_the_attendant_and_exits_4(self):
        def answer(trial, *lines, node=1):
            text = record("controlword", node, trial).replace(" summaries=2 ", " summaries=%d " % (2 + len(lines)))
            head, end = text.split("END trial ")
            return head + "".join(lines) + "END trial " + end
        marked = ("TRIAL_CONTROLWORD word=0080 tx=2b40600080000000 written=1 outcome=0 ack=1 uncertain=0 "
                  "abort=00000000 cycles=1 marked=1 enabled=1 moving=0 alert=1\n")
        cycle = ("TRIAL_CONTROLWORD_CYCLE which=1 us=1000 blocks=1c 2100=0001 6041=8037 6061=03 6064=6761 606c=0 "
                 "enabled=1 moving=0\n")
        last = "TRIAL_LAST passed=1 blocks=1d 1001=00 603f=0000 2100=0011 6041=8011 6061=03 6064=6761 606c=7\n"
        final = marked.replace(" cycles=1 marked=1 enabled=1 moving=0 ", " cycles=1 marked=2 enabled=0 moving=1 ")
        clean = marked.replace(" marked=1 enabled=1 moving=0 alert=1", " marked=0 enabled=0 moving=0 alert=0")
        fields = [1, 40]
        alert = ("node 1 MAY BE ENABLED OR TURNING after controlword 0x0080 (read 1: 6041=8037 606C=0); nothing more "
                 "was sent; use the cutoff")
        self.assertEqual(bench.controlword_alert("controlword", fields, answer(40, last, marked, cycle).encode()),
                         alert)
        # The final full read is TRIAL_LAST's.
        self.assertEqual(bench.controlword_alert("controlword", fields, answer(40, last, final, cycle).encode()),
                         alert.replace("(read 1: 6041=8037 606C=0)", "(read 2: 6041=8011 606C=7)"))
        # No mark, another trial, a line after END and another route are not this record's alert.
        self.assertIsNone(bench.controlword_alert("controlword", fields, answer(40, last, clean, cycle).encode()))
        self.assertIsNone(bench.controlword_alert("controlword", fields, answer(39, marked, cycle).encode()))
        self.assertIsNone(bench.controlword_alert("controlword", fields, (answer(40) + marked).encode()))
        self.assertIsNone(bench.controlword_alert("velocity", fields, answer(40, marked, cycle).encode()))
        self.assertIn("ALERT: node N MAY BE ENABLED OR TURNING", bench.__doc__)
        # Printed at once and again after the dump, kept in request.json, exit 4.
        board = Board()
        board.answer = answer(40, last, marked, cycle)
        argv = ["--command", "controlword", "--node", "1", "--trial", "40", "--word", "0x80", "--cycles", "3",
                "--interval-ms", "500"]
        status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 40", "none"))
        self.assertEqual(status, bench.VELOCITY_ALERT_STATUS, err)
        self.assertEqual(err.count("ALERT: " + alert + "\n"), 2)
        self.assertEqual(self.meta()["controlword_alert"], alert)
        # No mark: no alert, the ordinary status.
        self.clear_output()
        self.ledger.write_bytes(b"")
        board = Board()
        board.answer = answer(40, last, clean, cycle)
        status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 40", "none"))
        self.assertNotEqual(status, 4, err)
        self.assertNotIn("MAY BE ENABLED", err)
        self.assertNotIn("controlword_alert", self.meta())

    def test_range_probe_line_bounds_and_records(self):
        def probe(**values):
            argv = list(VELOCITY) + ["--range-probe"]
            for name, value in values.items():
                argv[argv.index("--" + name.replace("_", "-")) + 1] = str(value)
            return argv
        tail = "351,2,262546,18,52,768,34309,65537,134,291"
        # The firmware's grammar (BenchRoute.cpp VELOCITY_FIELDS): the probe flag follows the identity, and
        # protocol 12's reverse field ends the line.
        for argv, line in ((probe(), "TV1,30,1,6,200,200,12000,5000,15500,1,%s,1,0" % tail),
                           (probe(rpm=3000), "TV1,30,1,3000,200,200,12000,5000,15500,1,%s,1,0" % tail),
                           (probe(rpm=6000, ramp_up_ms=2000),
                            "TV1,30,1,6000,2000,200,12000,5000,15500,1,%s,1,0" % tail),
                           (VELOCITY, "TV1,30,1,6,200,200,12000,5000,15500,1,%s,0,0" % tail)):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed(line), argv)
            bench.check_bounds(args.command, fields)  # No refusal: the scope's caps hold.
            self.assertEqual(bench.plan_bounds(args.command, fields), (118, 15500))  # The flag changes no bound.
            self.assertEqual(bench.motion_weight(args.command, fields), (2, 1))
        # Without the probe nothing changed: 60 rpm is the cap and 61 is refused here, unsent and uncounted.
        args, fields = bench.arguments(BASE + probe(rpm=60) + LEDGER + ["--output", "x"])
        bench.check_bounds(args.command, fields)
        for argv, reason in ((probe(rpm=3000, ramp_up_ms=199),
                              "velocity range probe ramps: 199 ms up and 200 ms down; the library requires at least "
                              "200 ms"),
                             (probe(rpm=6, ramp_down_ms=199), "velocity range probe ramps: 200 ms up and 199 ms down"),
                             (probe(rpm=6000, ramp_up_ms=1), "velocity range probe ramps: 1 ms up"),
                             ([a for a in probe(rpm=61) if a != "--range-probe"],
                              "velocity rpm: 61 is over the ATTENDED_EXPERIMENT scope's 60 rpm (6000 only with "
                              "--range-probe)")):
            self.clear_output()
            self.ledger.write_bytes(b"")
            board = Board()
            operator = Operator("ATTENDED 30", "none")
            status, _, err = self.run_main(argv, Backend(board), operator)
            self.assertEqual(status, 2, err)
            self.assertIn("REFUSED: %s" % reason, err)
            self.assertEqual((board.writes, operator.questions), ([], []))
            self.assertFalse(self.output.exists())
            self.assertEqual(self.ledger.read_bytes(), b"")
        # Over 6000 with the probe, over 3000 without it, and the probe on another route: refused by the parser,
        # nothing created.
        waiver = ["--unattended-waiver", "do a full night soak without my attendance", "--waiver-covers-motion"]
        for argv, text in ((probe(rpm=6001), "--rpm must be 1..6000"),
                           ([a for a in probe(rpm=3001) if a != "--range-probe"], "--rpm must be 1..3000"),
                           (MODE3 + ["--range-probe"], "--range-probe belongs to velocity"),
                           (probe(rpm=3001) + waiver[:2], "--unattended-waiver refuses velocity")):
            err = io.StringIO()
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(err):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            self.assertIn(text, err.getvalue())
        # A probe takes a waiver that covers motion as any velocity does, above 3000 rpm too: the user's waiver of
        # 2026-11-13 covers the above-limit part of that campaign.
        for rpm in (3000, 6000):
            args, fields = bench.arguments(BASE + probe(rpm=rpm) + waiver + LEDGER + ["--output", "x"])
            self.assertEqual(fields[-2:], [1, 0])
        # The dry run states the probe; the record and the send entry keep it.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            bench.main(BASE + probe(rpm=3000) + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(), Clock(),
                       Operator())
        self.assertIn("range probe (prompt 59): VelocityEvidence::rangeProbe, rate cap 6000 rpm", out.getvalue())
        self.assertIn("no reverse probe: a positive rate, 60FF 3000", out.getvalue())
        self.assertIn("routes=12", out.getvalue())
        self.clear_output()
        self.ledger.write_bytes(b"")
        board = Board()
        status, _, err = self.run_main(probe(rpm=3000), Backend(board), Operator("ATTENDED 30", "none"))
        self.assertIn(status, (0, 1), err)  # The synthetic record has no terminals.
        self.assertEqual(board.writes,
                         [b"p", signed("TV1,30,1,3000,200,200,12000,5000,15500,1,%s,1,0" % tail).encode()])
        self.assertEqual(self.meta()["velocity_scope"],
                         {"gate": "ATTENDED_EXPERIMENT", "start_qualified": False, "stop_qualified": False,
                          "record": 351, "range_probe": True, "maximum_rpm": 6000, "maximum_duration_ms": 20000,
                          "minimum_ramp_ms": 200, "reverse_probe": False, "reverse_qualified": False,
                          "rate_rpm": 3000})
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["motion"], e["stop"], e["scope"], e["range_probe"], e["reverse_probe"])
                          for e in send], [("V", 2, 1, "ATTENDED_EXPERIMENT", True, False)])
        # The S16 alert reads a probe's record as any velocity's.
        self.clear_output()
        self.ledger.write_bytes(b"")
        board = Board()
        board.answer = velocity_record(30, RUN_NEEDS, STOP_NONE, BEST_NONE % 0, BEST_NONE % 1)
        status, _, err = self.run_main(probe(rpm=3000), Backend(board), Operator("ATTENDED 30", "none"))
        self.assertEqual(status, bench.VELOCITY_ALERT_STATUS, err)
        self.assertIn("ALERT: node 1 MAY BE RUNNING", err)

    def test_reverse_probe_line_bounds_and_records(self):
        def reverse(**values):
            argv = list(VELOCITY) + ["--reverse"]
            for name, value in values.items():
                if name == "range_probe":
                    argv.append("--range-probe")
                    continue
                argv[argv.index("--" + name.replace("_", "-")) + 1] = str(value)
            return argv
        tail = "351,2,262546,18,52,768,34309,65537,134,291"
        # The firmware's grammar (BenchRoute.cpp VELOCITY_FIELDS, protocol 12): the reverse flag ends the line, and
        # the rate field stays the magnitude.
        for argv, line in ((reverse(), "TV1,30,1,6,200,200,12000,5000,15500,1,%s,0,1" % tail),
                           (reverse(rpm=60), "TV1,30,1,60,200,200,12000,5000,15500,1,%s,0,1" % tail),
                           (reverse(rpm=6000, range_probe=1),
                            "TV1,30,1,6000,200,200,12000,5000,15500,1,%s,1,1" % tail)):
            args, fields = bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed(line), argv)
            bench.check_bounds(args.command, fields)  # No refusal: the scope's caps hold for the magnitude.
            self.assertEqual(bench.plan_bounds(args.command, fields), (118, 15500))  # The flag changes no bound.
            self.assertEqual(bench.motion_weight(args.command, fields), (2, 1))
        # The magnitude keeps the experiment's cap: 61 without the range probe is refused here, unsent and uncounted.
        self.clear_output()
        self.ledger.write_bytes(b"")
        board = Board()
        operator = Operator("ATTENDED 30", "none")
        status, _, err = self.run_main(reverse(rpm=61), Backend(board), operator)
        self.assertEqual(status, 2, err)
        self.assertIn("REFUSED: velocity rpm: the reverse probe's magnitude 61 is over the ATTENDED_EXPERIMENT "
                      "scope's 60 rpm (6000 only with --range-probe)", err)
        self.assertEqual((board.writes, operator.questions), ([], []))
        self.assertFalse(self.output.exists())
        self.assertEqual(self.ledger.read_bytes(), b"")
        # A signed --rpm is no form of the line, the flag belongs to velocity only, and a waiver needs to cover
        # motion as for any velocity: refused by the parser, nothing created.
        waiver = ["--unattended-waiver", "Unattended, I accept it", "--waiver-covers-motion"]
        for argv, text in ((reverse(rpm=-6), "--rpm must be 1..3000"),
                           (reverse(rpm=6001, range_probe=1), "--rpm must be 1..6000"),
                           (MODE3 + ["--reverse"], "--reverse belongs to velocity"),
                           (["--command", "velocity-target", "--node", "1", "--trial", "50", "--value", "-6",
                             "--reverse"], "--reverse belongs to velocity"),
                           (reverse() + waiver[:2], "--unattended-waiver refuses velocity")):
            err = io.StringIO()
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(err):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            self.assertIn(text, err.getvalue())
        args, fields = bench.arguments(BASE + reverse() + waiver + LEDGER + ["--output", "x"])
        self.assertEqual(fields[-2:], [0, 1])
        # The dry run states the probe, the signed rate and 60FF's bytes.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = bench.main(BASE + reverse() + ["--dry-run", "--campaign", str(self.ledger)], Untouchable(),
                                Clock(), Operator())
        self.assertEqual(status, 0)
        for text in ("reverse probe (prompt 59 item 4): VelocityEvidence::reverseProbe, rate -6 rpm, 60FF written "
                     "23 FF 60 00 FA FF FF FF (two's complement I32; Q03 open",
                     "a 606C against the command by more than 2 rpm ends the run with its own stop",
                     "reverseQualified stays 0", "no range probe: the experiment's 60 rpm cap", "routes=12",
                     repr(signed("TV1,30,1,6,200,200,12000,5000,15500,1,%s,0,1" % tail).encode("ascii"))):
            self.assertIn(text, out.getvalue())
        # The record and the send entry keep it.
        self.clear_output()
        self.ledger.write_bytes(b"")
        board = Board()
        status, _, err = self.run_main(reverse(), Backend(board), Operator("ATTENDED 30", "none"))
        self.assertIn(status, (0, 1), err)  # The synthetic record has no terminals.
        self.assertEqual(board.writes, [b"p", signed("TV1,30,1,6,200,200,12000,5000,15500,1,%s,0,1" % tail).encode()])
        self.assertEqual(self.meta()["velocity_scope"],
                         {"gate": "ATTENDED_EXPERIMENT", "start_qualified": False, "stop_qualified": False,
                          "record": 351, "range_probe": False, "maximum_rpm": 60, "maximum_duration_ms": 20000,
                          "reverse_probe": True, "reverse_qualified": False, "rate_rpm": -6,
                          "reverse_sign_margin_rpm": 2})
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["motion"], e["stop"], e["scope"], e["range_probe"], e["reverse_probe"])
                          for e in send], [("V", 2, 1, "ATTENDED_EXPERIMENT", False, True)])
        self.assertIsNone(bench._send_problem(send[0]))
        # A send entry whose reverse_probe differs from its line's field is inconsistent; one of protocol 11, whose
        # V line has 21 fields and no such name, is not.
        line = bench.sign_line("TV1,30,1,6,200,200,12000,5000,15500,1,%s,0,1" % tail)
        old = bench.sign_line("TV1,30,1,6,200,200,12000,5000,15500,1,%s,0" % tail)
        entry = {"event": "send", "route": "V", "node": 1, "trial": 30, "motion": 2, "stop": 1, "line": line}
        self.assertIsNone(bench._send_problem(dict(entry, reverse_probe=True)))
        self.assertIsNone(bench._send_problem(dict(entry, line=old)))
        for bad in (dict(entry, reverse_probe=False), dict(entry, reverse_probe=1),
                    dict(entry, reverse_probe=True, line=old)):
            self.assertIn("reverse_probe", bench._send_problem(bad))
        # The S16 alert reads a reverse record as any velocity's: the signed rate changes nothing.
        negative = RUN_NEEDS.replace(" rpm=6 ", " rpm=-6 ")
        self.assertNotEqual(negative, RUN_NEEDS)
        for lines, wanted in (((negative, STOP_NONE, BEST_NONE % 0, BEST_NONE % 1),
                               "node 1 MAY BE RUNNING: no stop left; use the cutoff"),
                              ((negative, STOP_OWN, BEST_NONE % 0, BEST_NONE % 1), None),
                              ((RUN_DONE.replace(" rpm=6 ", " rpm=-6 "), STOP_NONE), None)):
            self.assertEqual(bench.velocity_alert("velocity", [1, 30], velocity_record(30, *lines).encode("ascii")),
                             wanted, lines)
        self.clear_output()
        self.ledger.write_bytes(b"")
        board = Board()
        board.answer = velocity_record(30, negative, STOP_NONE, BEST_NONE % 0, BEST_NONE % 1)
        status, _, err = self.run_main(reverse(), Backend(board), Operator("ATTENDED 30", "none"))
        self.assertEqual(status, bench.VELOCITY_ALERT_STATUS, err)
        self.assertIn("ALERT: node 1 MAY BE RUNNING", err)
        # An image of protocol 11 would read the reverse line one field long: nothing is sent to it.
        for argv in (reverse(), VELOCITY):
            self.clear_output()
            self.ledger.write_bytes(b"")
            board = Board()
            board.routes = 11
            status, _, err = self.run_main(argv, Backend(board), Operator("ATTENDED 30", "none"))
            self.assertEqual((status, board.writes, self.meta()["reason"]), (2, [b"p"], "image_route_version"), err)

    def test_subdivision_line_bounds_dry_run_and_campaign(self):
        def subdivision(value, node="1", trial="40"):
            return ["--command", "subdivision", "--node", node, "--trial", trial, "--value", str(value)]
        # The firmware's grammar (BenchRoute.cpp SUBDIVISION_FIELDS): node, trial, attended and the 2201 value.
        for value in (400, 1000, 4000, 10000, 25600, 51200):
            args, fields = bench.arguments(BASE + subdivision(value) + LEDGER + ["--output", "x"])
            self.assertEqual(bench.encode(args.command, fields).decode("ascii"), signed("TK1,40,1,%d" % value))
            self.assertEqual(bench.plan_bounds(args.command, fields), (12, 0))
            self.assertEqual(bench.motion_weight(args.command, fields), (0, 0))
            bench.check_bounds(args.command, fields)
        self.assertEqual(bench.TRIALS["subdivision"][0], "K")
        self.assertIn("K", bench.SETTINGS_LETTERS)
        self.assertNotIn("K", bench.MOTION_ROUTES)
        self.assertEqual(evidence.TRIAL_ROUTES["subdivision"], "subdivision")
        # Outside 400..51200, missing, on another route or with a velocity's flag: refused by the parser.
        for argv, text in ((subdivision(399), "--value must be 400..51200"),
                           (subdivision(51201), "--value must be 400..51200"),
                           (subdivision(0), "--value must be 400..51200"),
                           (subdivision(65535), "--value must be 400..51200"),
                           (subdivision(1000)[:-2], "--value is required for this route"),
                           (MODE3 + ["--value", "1000"], "--value belongs to velocity-target and subdivision"),
                           (subdivision(1000) + ["--range-probe"], "--range-probe belongs to velocity"),
                           (subdivision(1000) + ["--fault-led", "prompt"], "--fault-led belongs to"),
                           (subdivision(1000, node="3"), "--node must be 1..2")):
            err = io.StringIO()
            with self.assertRaises(SystemExit, msg=argv), contextlib.redirect_stderr(err):
                bench.arguments(BASE + argv + LEDGER + ["--output", "x"])
            self.assertIn(text, err.getvalue())
        # Attended and counted like the settings routes: the campaign is required.
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            bench.arguments(BASE + subdivision(1000) + ["--output", "x"])
        # The dry run touches nothing and states the bytes and the reads.
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = bench.main(BASE + subdivision(10000) + ["--dry-run", "--campaign", str(self.ledger)],
                                Untouchable(), Clock(), Operator())
        self.assertEqual(status, 0)
        text = out.getvalue()
        for part in ("require the operator to type: ATTENDED 40", "at most 12 terminals of 128 and 0 ms",
                     "must report released (2100 bit 4, no running or alarm)",
                     "one download 2B 01 22 00 10 27 00 00 (2201 = 10000)", "2200 never written",
                     "no 6040, no enable, no save", "routes=12", repr(signed("TK1,40,1,10000").encode("ascii"))):
            self.assertIn(part, text)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.ledger.read_bytes(), b"")
        # One send: the attended confirmation only, a settings entry with no motion weight, then the record.
        board = Board()
        operator = Operator("ATTENDED 40")
        status, _, err_text = self.run_main(subdivision(10000), Backend(board), operator)
        self.assertIn(status, (0, 1), err_text)  # The synthetic record has no terminals.
        self.assertEqual(board.writes, [b"p", signed("TK1,40,1,10000").encode()])
        self.assertEqual(len(operator.questions), 1)
        self.assertEqual(self.meta()["plan"], "prompt 59 2201 subdivision")
        send = [entry for entry in self.entries() if entry["event"] == "send"]
        self.assertEqual([(e["route"], e["motion"], e["stop"], e.get("settings")) for e in send], [("K", 0, 0, 1)])
        # Three per node across boots, never toward the twelve motion admissions; node 2 has its own count, and
        # settings-2202's count is apart.
        self.write_ledger([dict(send_entry("K", 1, n, 0), settings=1) for n in (1, 2, 3)] +
                          [send_entry("M", 1, n) for n in range(4, 16)])
        self.clear_output()
        board = Board()
        status, _, err_text = self.run_main(subdivision(1000, trial="60"), Backend(board), Operator("ATTENDED 60"))
        self.assertEqual(status, 2, err_text)
        self.assertIn("subdivision admissions on node 1, the cap is 3", err_text)
        self.assertEqual(board.writes, [])
        self.clear_output()
        status, _, err_text = self.run_main(subdivision(1000, node="2", trial="60"), Backend(Board()),
                                            Operator("ATTENDED 60"))
        self.assertIn(status, (0, 1), err_text)
        # A K send without its settings mark is an inconsistent entry.
        self.ledger.write_bytes(chain([send_entry("K", 1, 4, 0)]))
        board = Board()
        status, _, err_text = self.run_main(position(1, 20), Backend(board), Operator("ATTENDED 20"))
        self.assertEqual(status, 2, err_text)
        self.assertIn("inconsistent send entry", err_text)
        # Unattended only under the user's waiver, as settings-2202.
        self.ledger.write_bytes(b"")
        self.clear_output()
        operator = Operator()
        status, _, err_text = self.run_main(subdivision(1000, trial="70") + ["--unattended-waiver", "I accept it"],
                                            Backend(Board()), operator)
        self.assertIn(status, (0, 1), err_text)
        self.assertEqual(operator.questions, [])
        self.assertEqual(self.meta()["operator"]["attended"],
                         {"unattended": True, "waiver": "I accept it", "accepted": True})

    def test_subdivision_record_reads_through_the_evidence_parser(self):
        # The firmware's record lines as test/bench_route_test.cpp's fake prints them (never a drive's): the
        # grammar names every summary, and a refused write is failed evidence with nothing written.
        lines, _ = evidence.parse_bytes(
            b"TRIAL_PLAN route=subdivision node=1 trial=1 attended=1 value=10000\n"
            b"TRIAL_DRIVER which=1 present=1 2200=0000 2201=10000\n"
            b"TRIAL_SUBDIVISION requested=10000 written=1 outcome=0 ack=1 uncertain=0 abort=00000000\n"
            b"TRIAL_PLAN route=velocity node=1 trial=2 attended=1 rpm=3000 up=200 down=200 dur=12000 interval=5000 "
            b"deadline=15500 kind=1 probe=1\n")
        self.assertEqual([(r.kind, r.status, r.extra) for r in lines],
                         [("TRIAL_PLAN", "parsed", {}), ("TRIAL_DRIVER", "parsed", {}),
                          ("TRIAL_SUBDIVISION", "parsed", {}), ("TRIAL_PLAN", "parsed", {})])
        self.assertEqual(lines[1].values, {"which": 1, "present": 1, "2200": 0, "2201": 10000})
        self.assertEqual(lines[2].values["requested"], 10000)
        self.assertEqual(lines[3].values["probe"], 1)
        self.assertIn("subdivision", evidence.SLOT_STEP_ROUTES)

    def test_close_sessions_reply_decides_the_status(self):
        board = Board()
        status, out, _ = self.run_main(["--command", "close-sessions", "--seconds", "1"], Backend(board))
        self.assertEqual(status, 0)
        # One byte, nothing else: no ping, no T line, no release.
        self.assertEqual(board.writes, [b"x"])
        meta = self.meta()
        self.assertEqual((meta["tx_hex"], meta["reply_status"]), ("78", 0))
        self.assertTrue(meta["reply"].startswith("SESSIONS closed node1=1 node2=0 kept1=60 "))
        self.assertIn("SESSIONS closed", out)
        report = evidence.replay_capture(self.output, manifest=self.output / "none.json")
        self.assertEqual(report.verdict, "control")
        self.assertIn("SESSIONS_CLOSED", report.kinds)
        # Refused, with nothing changed: 2.
        self.clear_output()
        board = Board()
        board.sessions = "SESSIONS refused reason=record node1=0 node2=0; nothing sent, nothing else changed\n"
        status, _, _ = self.run_main(["--command", "close-sessions", "--seconds", "1"], Backend(board))
        self.assertEqual((status, self.meta()["reply_status"]), (2, 2))
        report = evidence.replay_capture(self.output, manifest=self.output / "none.json")
        self.assertEqual(report.verdict, "control")
        self.assertIn("SESSIONS_REFUSED", report.kinds)
        # An image without the command: no SESSIONS line, 1. A stale reply already waiting decides nothing.
        self.clear_output()
        board = Board()
        board.sessions = None
        board.queue.extend(b"SESSIONS closed node1=1 node2=1 kept1=00 kept2=00 motion=0; stale\n")
        status, _, _ = self.run_main(["--command", "close-sessions", "--seconds", "1"], Backend(board))
        self.assertEqual(status, 1)
        self.assertEqual((self.meta()["reply"], self.meta()["reply_status"]), (None, 1))
        self.assertEqual(board.writes, [b"x"])

    def test_close_sessions_dry_run_and_grammar(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = bench.main(BASE + ["--command", "close-sessions", "--dry-run"], Untouchable(), Clock(),
                                Operator())
        self.assertEqual(status, 0)
        self.assertIn("send once: b'x'", out.getvalue())
        self.assertIn("exit 0 on 'SESSIONS closed', 2 on 'SESSIONS refused', 1 on neither", out.getvalue())
        self.assertFalse(self.output.exists())
        self.assertEqual([name for name, byte in bench.COMMANDS.items() if byte == b"x"], ["close-sessions"])
        for argv in (["--command", "close-sessions", "--node", "1"],
                     ["--command", "close-sessions", "--campaign", str(self.ledger)]):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                bench.main(BASE + argv + ["--output", str(self.output)], Untouchable(), Clock(), Operator())
        lines, _ = evidence.parse_bytes(
            b"SESSIONS closed node1=1 node2=1 kept1=21 kept2=00 motion=12; nothing sent; latest controlwords "
            b"dropped; fences, trials and caps kept\n"
            b"SESSIONS refused reason=owner node1=1 node2=0; nothing sent; the sessions named stay closed\n")
        self.assertEqual([(r.kind, r.status) for r in lines],
                         [("SESSIONS_CLOSED", "parsed"), ("SESSIONS_REFUSED", "parsed")])
        self.assertEqual(lines[0].values, {"node1": 1, "node2": 1, "kept1": 0x21, "kept2": 0, "motion": 12})
        self.assertEqual(lines[1].values, {"reason": "owner", "node1": 1, "node2": 0})

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
