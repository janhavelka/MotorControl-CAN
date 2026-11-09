#!/usr/bin/env python3
"""Run one planned PDO stream session on the S2 bench, or analyze a retained one; one lead alone runs this.

run: validates the recorded USB identity, opens only that port (neither EN nor BOOT is driven),
pings, refuses unless the image speaks the lines the plan uses (PONG pdo=1: N, P, R, W; pdo=2 adds
E; pdo=3 adds C, the bounded silent capture; pdo=4 adds D, K and H: the discovery scan, the typed
communication reads and the guarded heartbeat commissioning) with no soak, trial, watch or capture
held, then sends the plan's steps in order and captures every byte to rx.bin. A step sends one
console command (a single byte, or an N, P, R, W, E, C, D, K, H, S line of
examples/s2_bench/PdoCommand.h and the soak) and
waits for its expected reply; a refusal or a missing reply ends the plan. The plan is the
only source of device commands: nothing is retried, replayed or added, and no NMT, write or stop
is sent on the plan's behalf. If the plan ends early while a watch runs, only "P0" is sent, and
while a capture collects, only "C0"; each ends local work and sends nothing on CAN. Whatever state
the drives were left in, Operational included, is recorded and reported, not repaired.

analyze: replays rx.bin offline through the s2_evidence grammar and reports, per received ID, the
watch's frames and on-device statistics, every NMT request with its TX completion time, the first
and last frame of each ID around each NMT request, heartbeat states, every PDO record read or
write with its raw values, every capture record with its frames per ID, and every D, K and H
record with its header and typed lines. Times are the S2's monotonic microseconds: a frame's hi is its ISR clock
sample at enqueue, an upper bound, never an exact bus timestamp. A watched frame is not a qualified
PDO sample.

A session directory holds plan.json (the plan as given), session.json (request, fixture, steps and
their outcomes), rx.bin and rx.txt (identical bytes). The directory must not exist beforehand.
"""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import s2_evidence as evidence

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = {"port": "COM15", "usb_serial": "94A990398B40", "vid": 0x303A, "pid": 0x1002}
BAUD = 115200
CAPTURE_LIMIT = 64 << 20
PING_TIMEOUT_S = 3.0
DEFAULT_TIMEOUT_S = 3.0
WATCH_STOP_S = 5.0
CAPTURE_STOP_S = 30.0  # A stopped capture's record prints after the end: up to 137 lines.
# Replies that end a plan: the firmware refused the command, so nothing after it may follow.
REFUSALS = ("PDO_LINE refused", "PDO_READ refused", "PDO_WRITE refused", "PDO_CONFIG refused", "PDOW refused",
            "PDOW_STOP refused",
            "NMT refused", "NMT release refused", "RELEASE refused", "PDO release refused", "SOAK_REFUSED",
            "TRIAL_REFUSED", "UNKNOWN command", "HEARTBEAT refused", "PDO refused", "IDENTITY refused",
            "CAN start refused", "DUMP unavailable", "OUTPUT_FORMAT_ERROR", "CAP refused",
            "CAP_STOP refused", "CAP release refused", "DISC refused", "COMM refused", "HB refused",
            "DISC release refused", "COMM release refused", "HB release refused", "NETWORK refused")
LINE_COMMANDS = ("N", "P", "R", "W", "E", "C", "D", "K", "H", "S")
# The PDO line letters each image protocol (PONG pdo=) serves. An older image answers an unknown
# letter as single-byte commands: E1,100 there would start a node-1 identity read, and C1,10,64 the
# same through its digits. Neither is ever sent to an image that does not serve it.
# pdo=4 adds D, K and H; on a pdo=3 image "K1" would be UNKNOWN then a node-1 identity read.
PDO_PROTOCOLS = {"1": frozenset("NPRW"), "2": frozenset("NPRWE"), "3": frozenset("NPRWEC"),
                 "4": frozenset("NPRWECDKH")}
PDO_LETTERS = "NPRWECDKH"
# q, w (NMT Pre-operational) and m (the fixed TPDO1 map experiment) are not offered: their terminals
# are not checked here. NMT goes through N lines.
# M (pdo=4) prints one HEAP line: internal heap totals and regions; it sends nothing on CAN.
SINGLE_BYTES = set("cpsrdnhjX34M")
# The admission line and end line of every command whose firmware terminal decides whether later
# steps may follow, keyed by the command itself, never by the plan's own expect text.
CHECKED = {"N": ("NMT admitted", "NMT request"), "R": ("PDO_READ admitted", "END raw"),
           "W": ("PDO_WRITE admitted", "END raw"), "E": ("PDO_CONFIG admitted", "END PDO"),
           "h": ("HEARTBEAT experiment admitted", "END raw"),
           "j": ("HEARTBEAT experiment admitted", "END raw"), "3": ("IDENTITY admitted", "END raw"),
           "4": ("IDENTITY admitted", "END raw"), "C": ("CAP admitted", "END capture"),
           "D": ("DISC admitted", "END discovery"), "K": ("COMM admitted", "END comm"),
           "H": ("HB admitted", "END heartbeat")}
NMT_NAMES = {1: "start", 2: "stop", 0x80: "pre-operational"}
# The record each command leaves held, and the reply of the r that releases it. The bench firmware
# admits R, W, E, h, j, 3, 4 and m only with no NMT result and no PDO record held, and N only with
# no record of any kind held; while a D, K or H record is held it admits nothing but p, s, n, d and r
# (examples/s2_bench/main/main.cpp, each command's accepted condition).
# The 2026-11-06 replay plans stopped twice: once expecting the wrong release line after E, once
# leaving node 2 Operational because an N's result was never released before the next N.
HOLDS = {"R": "raw", "W": "raw", "h": "raw", "j": "raw", "3": "raw", "4": "raw", "E": "pdo", "N": "nmt",
         "C": "capture", "D": "discovery", "K": "comm", "H": "heartbeat"}
RELEASE_LINES = {"raw": "RELEASED record", "pdo": "PDO record released", "nmt": "NMT result released",
                 "capture": "CAP record released", "discovery": "DISC record released",
                 "comm": "COMM record released", "heartbeat": "HB record released"}
DISCOVERY_MODES = {"o": 0, "p": 1, "i": 2}  # Passive observation, one probe, nine identity uploads.


def capture_stop(text):
    """True for the C0 line, which ends a running capture: it holds no record and has no terminal."""
    return text[:1] == "C" and len(text) > 1 and text[1:].isdigit() and int(text[1:]) == 0


def capture_plan(text):
    """(identity, seconds, frames) of a C start line, or None when it is not one."""
    fields = text[1:].split(",")
    if text[:1] != "C" or len(fields) != 3 or not all(f.isdigit() for f in fields):
        return None
    return tuple(int(f) for f in fields)


def discovery_plan(text):
    """(mode, seconds, select, expect) of a D line, or None when it is not one the firmware admits."""
    fields = text[1:].split(",")
    if text[:1] != "D" or len(fields) != 4 or fields[0] not in DISCOVERY_MODES or \
            not all(f.isdigit() for f in fields[1:]):
        return None
    mode = DISCOVERY_MODES[fields[0]]
    seconds, select, expect = (int(f) for f in fields[1:])
    if not 1 <= seconds <= 60 or select > 2 or expect > 1 or (expect and mode != 2):
        return None
    return mode, seconds, select, expect


def line_problem(text):
    """Why a D, K or H line is not one the firmware admits, or None. Checked before anything is sent:
    an H line names prompt 38's one reviewed value, raw 1000, and no reverse transition exists."""
    if text[0] == "D" and discovery_plan(text) is None:
        return "%r is not D<o|p|i>,<seconds 1..60>,<select 0..2>,<expect 0|1, 1 only with i>" % text
    if text[0] == "K" and text not in ("K1", "K2"):
        return "%r is not K1 or K2" % text
    if text[0] == "H" and text not in ("H1,1000", "H2,1000"):
        return "%r is not H1,1000 or H2,1000: only raw 0 to raw 1000 is reviewed" % text
    return None


class SessionError(RuntimeError):
    """Nothing was sent, or the session could not continue; the message names the failed fact."""


def check_plan(plan):
    """Validate a plan without touching anything; returns the normalized steps."""
    if not isinstance(plan, dict) or not isinstance(plan.get("steps"), list) or not plan["steps"]:
        raise SessionError("a plan is an object with a non-empty steps list")
    for key in ("purpose", "authorization"):
        if not isinstance(plan.get(key), str) or not plan[key].strip():
            raise SessionError("the plan must state its %s" % key)
    steps = []
    held = None  # (kind, step number) of the record the plan has not released yet.
    for number, step in enumerate(plan["steps"], 1):
        if not isinstance(step, dict):
            raise SessionError("step %d is not an object" % number)
        unknown = set(step) - {"send", "expect", "end", "timeout_s", "end_timeout_s", "wait_s", "note",
                               "require"}
        if unknown:
            raise SessionError("step %d has unknown keys %s" % (number, sorted(unknown)))
        if "send" in step:
            text = step["send"]
            if not isinstance(text, str) or not text:
                raise SessionError("step %d sends nothing" % number)
            if len(text) == 1:
                if text not in SINGLE_BYTES:
                    raise SessionError("step %d: %r is not a reviewed single-byte command" % (number, text))
            elif text[0] not in LINE_COMMANDS or any(c in text for c in "\r\n") or not text.isascii():
                raise SessionError("step %d: %r is not an N, P, R, W, E, C, D, K, H or S line" % (number, text))
            elif line_problem(text):
                raise SessionError("step %d: %s" % (number, line_problem(text)))
            if not isinstance(step.get("expect"), str) or not step["expect"]:
                raise SessionError("step %d: a send needs the reply it expects" % number)
        elif "wait_s" not in step:
            raise SessionError("step %d neither sends nor waits" % number)
        for key in ("timeout_s", "end_timeout_s", "wait_s"):
            if key in step and not (isinstance(step[key], (int, float)) and 0 < step[key] <= 900):
                raise SessionError("step %d: %s must be 0..900 s" % (number, key))
        if "end" in step and "send" not in step:
            raise SessionError("step %d: end needs a send" % number)
        # Work whose terminal decides whether later steps may follow names its exact admission and end.
        if "send" in step and step["send"][0] in CHECKED and not capture_stop(step["send"]):
            admission, end = CHECKED[step["send"][0]]
            if step["expect"] != admission or step.get("end") != end:
                raise SessionError("step %d: %r needs expect %r and end %r" % (number, step["send"], admission, end))
            plan = capture_plan(step["send"]) if step["send"][0] == "C" else ()
            if plan is None:
                raise SessionError("step %d: %r is not C<capture>,<seconds>,<frames>" % (number, step["send"]))
            # The record prints after the capture's own duration: the end must be waited for past it.
            if plan and step.get("end_timeout_s", DEFAULT_TIMEOUT_S) <= plan[1]:
                raise SessionError("step %d: a %d s capture needs end_timeout_s above %d" %
                                   (number, plan[1], plan[1]))
            # A discovery record can end only at its window; a passive one always does.
            scan = discovery_plan(step["send"]) if step["send"][0] == "D" else None
            if scan and step.get("end_timeout_s", DEFAULT_TIMEOUT_S) <= scan[1]:
                raise SessionError("step %d: a %d s discovery needs end_timeout_s above %d" %
                                   (number, scan[1], scan[1]))
        if "require" in step:
            check_requirements(number, step["require"])
        held = check_release(number, step, held)
        steps.append(step)
    return steps


def check_release(number, step, held):
    """One held record at a time, released by an r that expects that record's own release line.
    An r before any record of this plan may release one left by an earlier session. Returns the
    record held after this step. A plan may end with a record held."""
    text = step.get("send")
    if text == "r":
        if held is None:
            # A record left by an earlier session: the plan cannot know which, the firmware does.
            if not any(line.startswith(step["expect"]) for line in RELEASE_LINES.values()):
                raise SessionError("step %d: r expects %r, which is no release line" % (number, step["expect"]))
            return None
        wanted = RELEASE_LINES[held[0]]
        if not wanted.startswith(step["expect"]):  # The runner matches a reply by prefix.
            raise SessionError("step %d: r releases the %s record of step %d; expect %r" %
                               (number, held[0], held[1], wanted))
        return None
    if text and text[0] in HOLDS and (len(text) == 1) == (text[0] in SINGLE_BYTES) and not capture_stop(text):
        if held is not None:
            raise SessionError("step %d: %r while the %s record of step %d is held; release it first" %
                               (number, text, held[0], held[1]))
        return (HOLDS[text[0]], number)
    return held


def check_requirements(number, require):
    """A requirement names a record kind, optional fields selecting the latest such record, and the
    fields it must hold; RESULT objects are written "1800:05". Checked before the step is sent."""
    if not isinstance(require, list) or not require:
        raise SessionError("step %d: require is a non-empty list" % number)
    for item in require:
        if not isinstance(item, dict) or set(item) - {"kind", "where", "match"} or \
                not isinstance(item.get("kind"), str) or not isinstance(item.get("match"), dict) or \
                not item["match"] or not isinstance(item.get("where", {}), dict):
            raise SessionError("step %d: each requirement has kind, optional where, and match" % number)


def _field(name, value):
    if name == "object" and isinstance(value, str):
        index, _, sub = value.partition(":")
        return [int(index, 16), int(sub, 16)]
    return value


def requirement_problem(require, lines):
    """None when every requirement holds on the latest matching parsed record, else why not."""
    records = [evidence.parse_line(number, text) for number, text in enumerate(lines, 1)]
    parsed = [r for r in records if r.status == "parsed"]
    for item in require:
        where = {k: _field(k, v) for k, v in item.get("where", {}).items()}
        candidates = [r for r in parsed if r.kind == item["kind"] and
                      all(r.values.get(k) == v for k, v in where.items())]
        if not candidates:
            return "no %s record%s" % (item["kind"], " with %s" % item["where"] if where else "")
        latest = candidates[-1].values
        for key, wanted in item["match"].items():
            if latest.get(key) != _field(key, wanted):
                return "%s %s=%r, required %r" % (item["kind"], key, latest.get(key), wanted)
    return None


def tool_digests():
    """SHA-256 of the runner and parser that ran, so a session names its exact tools."""
    import hashlib
    here = Path(__file__).resolve().parent
    return {name: hashlib.sha256((here / name).read_bytes()).hexdigest()
            for name in ("s2_pdo.py", "s2_evidence.py")}


def encode(text):
    return text.encode("ascii") if len(text) == 1 else text.encode("ascii") + b"\n"


class Capture:
    """Raw bytes as received; lines are split only to know when a step is answered."""

    def __init__(self, port, clock):
        self.port, self.clock = port, clock
        self.data = bytearray()
        self.partial = bytearray()
        self.lines = []

    def read(self):
        chunk = self.port.read(min(max(self.port.in_waiting, 1), 4096))
        if chunk:
            self.data.extend(chunk)
            self.partial.extend(chunk)
            while b"\n" in self.partial:
                line, _, rest = self.partial.partition(b"\n")
                self.lines.append(line.rstrip(b"\r").decode("utf-8", errors="replace"))
                self.partial = bytearray(rest)
            if len(self.data) > CAPTURE_LIMIT:
                raise SessionError("bounded capture capacity exceeded")
        return chunk

    def until(self, seconds, done, start=None):
        """Read until done(line) for a line at or after index start; returns (index, line) or None."""
        deadline = self.clock.monotonic() + seconds
        seen = len(self.lines) if start is None else start
        while True:
            for index in range(seen, len(self.lines)):
                if done(self.lines[index]):
                    return index, self.lines[index]
            seen = len(self.lines)
            if self.clock.monotonic() >= deadline:
                return None
            self.read()

    def send(self, data):
        if self.port.write(data) != len(data):
            raise SessionError("short command write; do not replay")


def _answer(expect):
    def done(line):
        return line.startswith(expect) or line.startswith(REFUSALS)
    return done


def watch_running(lines):
    """True when the last PDOW_BEGIN in the capture has no PDOW_END after it."""
    running = False
    for line in lines:
        if line.startswith("PDOW_BEGIN "):
            running = True
        elif line.startswith("PDOW_END "):
            running = False
    return running


def terminal_problem(step, lines):
    """Check the firmware's own terminal for a step's work, parsed through the s2_evidence grammar.

    lines runs from the admission reply to the end marker. A raw record must hold its RAW header with
    failure 0 and no loss, on the step's node, with every RESULT successful and the expected count; an
    NMT request must carry the sent node and command, outcome 0 and a confirmed TX. An E record must
    show the confirmed event-timer change itself (config_problem). Returns None when the work
    succeeded, otherwise why the plan must stop. An end marker alone never counts.
    """
    records = [evidence.parse_line(number, text) for number, text in enumerate(lines, 1)]
    parsed = [r for r in records if r.status == "parsed"]
    text = step["send"]
    command = text[0]
    count = {"R": 9, "W": 3, "h": 3, "j": 3, "3": 26, "4": 26}.get(command)
    if count is not None:
        headers = [r for r in parsed if r.kind == "RAW"]
        results = [r for r in parsed if r.kind == "RESULT"]
        ends = [r for r in parsed if r.kind == "END" and r.raw.startswith("END raw")]
        if len(headers) != 1 or len(ends) != 1:
            return "raw_terminal_unparsed"
        header = headers[0].values
        node = int(text[1]) if text[0] in "RW" else 1 if text in "h3" else 2 if text in "j4" else None
        if header.get("failure") != 0 or header.get("lost") != 0 or ends[0].values.get("lost") != 0:
            return "raw_failure_%s" % header.get("failure")
        if node is not None and header.get("node") != node:
            return "raw_wrong_node"
        if len(results) != count or header.get("terminals") != count or \
                any(r.values.get("outcome") != 0 for r in results):
            return "raw_results_incomplete"
        if command == "W":
            _, pdo_set, subindex, _, value = (int(field) for field in text[1:].split(","))
            wanted = [0x1800 + pdo_set, subindex]
            last = results[-1].values
            if any(r.values.get("object") != wanted for r in results) or \
                    int.from_bytes(bytes.fromhex(last["data"])[:last["size"]], "little") != value:
                return "write_readback_differs"
    if command == "E":
        return config_problem(text, parsed)
    if command == "C":
        return capture_problem(text, parsed)
    if command == "D":
        return discovery_problem(text, parsed)
    if command == "K":
        return comm_problem(text, parsed)
    if command == "H":
        return heartbeat_problem(text, parsed)
    if command == "N":
        nmts = [r for r in parsed if r.kind == "NMT"]
        if len(nmts) != 1:
            return "nmt_result_unparsed"
        v = nmts[0].values
        wanted = {"o": 1, "s": 2, "p": 0x80}[text.split(",")[1]]
        if v.get("target") != int(text[1]) or v.get("cmd") != wanted:
            return "nmt_wrong_request"
        if v.get("outcome") != 0 or v.get("txConfirmed") != 1:
            return "nmt_not_transmitted"
    return None


CONFIG_FIELDS = ("node", "transition", "raw", "lastStep", "effectStep", "failedStep")


def config_problem(text, parsed):
    """Check one E record: its PDO header, nineteen RESULTs and the one write it put on the wire.

    Success is the firmware's own confirmed readback of the requested change on the sent node:
    failure 0, nothing lost, transition 2 (event timer), the requested raw literal, map confirmed,
    no uncertainty, write acknowledged (ackMask 01) and the epoch migrated, steps 1..10 confirmed
    and none failed; every RESULT successful, the tenth the 1800:05 download, the readback of
    1800:05 the literal, and exactly one non-upload frame to the node, 2B 00 18 05 then the literal.
    """
    headers = [r for r in parsed if r.kind == "PDO"]
    results = [r for r in parsed if r.kind == "RESULT"]
    ends = [r for r in parsed if r.kind == "END" and r.raw.startswith("END PDO")]
    if len(headers) != 1 or len(ends) != 1 or any(key not in headers[0].values for key in CONFIG_FIELDS):
        return "pdo_terminal_unparsed"  # Includes an image whose record lacks the configuration fields.
    header = headers[0].values
    node, raw = (int(field) for field in text[1:].split(","))
    if header["failure"] != 0 or header["lost"] != 0:
        return "pdo_failure_%s" % header["failure"]
    if header["node"] != node:
        return "pdo_wrong_node"
    if header["transition"] != 2 or header["raw"] != raw:
        return "pdo_wrong_request"
    if (header["confirmed"], header["uncertain"], header["ackMask"], header["migrated"], header["lastStep"],
            header["effectStep"], header["failedStep"]) != (1, 0, 1, 1, 10, 1, 0):
        return "pdo_not_confirmed"
    if len(results) != 19 or header["terminals"] != 19 or any(r.values.get("outcome") != 0 for r in results):
        return "pdo_results_incomplete"
    if results[9].values["object"] != [0x1800, 5] or results[9].values.get("dir") != 1 or \
            any(r.values.get("dir") != 0 for i, r in enumerate(results) if i != 9):
        return "pdo_write_differs"
    readbacks = [r.values for r in results[10:] if r.values["object"] == [0x1800, 5]]
    if len(readbacks) != 1 or readbacks[0]["size"] != 2 or \
            int.from_bytes(bytes.fromhex(readbacks[0]["data"])[:2], "little") != raw:
        return "pdo_readback_differs"
    writes = [r.values["data"] for r in parsed if r.kind == "TRAFFIC" and r.values.get("k") == 1 and
              r.values.get("id") == 0x600 + node and not r.values["data"].startswith("40")]
    if writes != ["2b001805" + raw.to_bytes(2, "little").hex() + "0000"]:
        return "pdo_write_bytes_differ"
    return None


def capture_problem(text, parsed):
    """Check one capture record: its own identity throughout, an orderly end at its duration, nothing
    lost, overflowed or refused at the gate, the normal level requested and the gate open again, and
    exactly the CAP_F lines it says it stored. A record with zero frames passes: a silent S2 beside
    silent drives can legitimately see nothing, and frames prove neither capture nor silence."""
    identity, seconds, frames = capture_plan(text)
    rows = {kind: [r.values for r in parsed if r.kind == kind and r.values.get("cap") == identity]
            for kind in ("CAP_REC", "CAP_TIME", "CAP_LEVEL", "CAP_CTRL", "CAP_OWNER", "CAP_IMAGE", "CAP_F",
                         "CAP_END")}
    if any(len(rows[k]) != 1 for k in ("CAP_REC", "CAP_TIME", "CAP_LEVEL", "CAP_IMAGE", "CAP_END")) or \
            len(rows["CAP_CTRL"]) != 2 or len(rows["CAP_OWNER"]) != 2:
        return "capture_record_unparsed"
    if any(r.kind.startswith("CAP_") and r.values.get("cap") != identity for r in parsed if "cap" in r.values):
        return "capture_wrong_identity"
    record, level, timing = rows["CAP_REC"][0], rows["CAP_LEVEL"][0], rows["CAP_TIME"][0]
    if record["reason"] != "duration":
        return "capture_ended_%s" % record["reason"]
    if record["seconds"] != seconds or record["capacity"] != frames or \
            timing["end"] - timing["begin"] < seconds * 1000000:
        return "capture_wrong_plan"
    if record["lost"] or record["tapDropped"] or record["overflow"] or rows["CAP_END"][0]["lost"]:
        return "capture_incomplete"
    if record["txRefused"]:
        return "capture_send_attempted"  # Something was admitted and refused at the closed gate.
    if (level["begin"], level["exit"], level["reopened"]) != (1, 1, 1):
        return "capture_exit_not_normal"
    if len(rows["CAP_F"]) != record["stored"] or rows["CAP_END"][0]["stored"] != record["stored"] or \
            [f["i"] for f in rows["CAP_F"]] != list(range(record["stored"])):
        return "capture_frames_differ"
    return None


def discovery_problem(text, parsed):
    """Check one D record: the scan the line asked for, both nodes settled without cancellation,
    interface change, refused progression or unresolved route, and the host selection it asked for,
    which sends nothing. Passive mode sends nothing and a silent node is a legitimate NO_RESPONSE
    (heartbeat disabled); a probe or identity scan needs every upload successful and both nodes
    PRESENCE or GENERIC_IDENTITY, with EXPECTATION_MATCH when the line compares the fixture identity.
    Success is an evidenced responder or raw identity, never a model, firmware or motor fact."""
    mode, seconds, select, expect = discovery_plan(text)
    headers = [r.values for r in parsed if r.kind == "DISC"]
    targets = [r.values for r in parsed if r.kind == "DISC_T"]
    selections = [r.values for r in parsed if r.kind == "DISC_SELECT"]
    ends = [r for r in parsed if r.kind == "END" and r.raw.startswith("END discovery")]
    results = [r.values for r in parsed if r.kind == "RESULT"]
    if len(headers) != 1 or len(targets) != 2 or len(selections) != 1 or len(ends) != 1:
        return "discovery_record_unparsed"
    header = headers[0]
    if (header["mode"], header["seconds"], header["targets"], header["completed"]) != (mode, seconds, 2, 2):
        return "discovery_wrong_request"
    if header["cancelled"] or header["changed"] or header["failed"] or header["lost"]:
        return "discovery_interrupted"
    if [t["node"] for t in targets] != [1, 2]:
        return "discovery_wrong_targets"
    if any(t["unresolved"] for t in targets) or header["unresolved"]:
        return "discovery_unresolved"
    if mode == 0:
        if results or header["admitted"] or header["terminals"]:
            return "discovery_passive_sent"
        if any(t["outcome"] not in (1, 3) for t in targets):  # PRESENCE or NO_RESPONSE
            return "discovery_passive_outcome"
    else:
        count = 2 if mode == 1 else 18
        if len(results) != count or header["terminals"] != count or header["admitted"] != count:
            return "discovery_results_incomplete"
        if any(r.get("dir") != 0 or r["outcome"] != 0 for r in results):
            return "discovery_service_failed"
        wanted, confidence = (1, 1) if mode == 1 else (2, 3 if expect else 2)
        for target in targets:
            if target["outcome"] != wanted or target["confidence"] != confidence or not target["current"]:
                return "discovery_outcome_%d" % target["outcome"]
    selection = selections[0]
    if selection["node"] != select:
        return "discovery_wrong_selection"
    if select and (not selection["attempted"] or not selection["selected"] or
                   selection["tx0"] != selection["tx1"]):
        return "discovery_selection_refused"
    return None


COMM_ORDER = ([("COMM_NETWORK", None, None), ("COMM_NODE", None, None)] +
              [("COMM_PDO", "t", n) for n in range(1, 5)] + [("COMM_MAP", "t", n) for n in range(1, 5)] +
              [("COMM_PDO", "r", n) for n in range(1, 5)] + [("COMM_MAP", "r", n) for n in range(1, 5)])


def comm_problem(text, parsed):
    """Check one K record: all eighteen typed groups of the sent node published in their fixed order,
    95 successful uploads and no download among them. The values are raw stored settings: no route,
    map, unit or active setting is established by a complete record."""
    node = int(text[1:])
    headers = [r.values for r in parsed if r.kind == "COMM"]
    ends = [r for r in parsed if r.kind == "END" and r.raw.startswith("END comm")]
    groups = [r for r in parsed if r.kind in ("COMM_NETWORK", "COMM_NODE", "COMM_PDO", "COMM_MAP")]
    results = [r.values for r in parsed if r.kind == "RESULT"]
    if len(headers) != 1 or len(ends) != 1:
        return "comm_record_unparsed"
    header = headers[0]
    if header["node"] != node:
        return "comm_wrong_node"
    if header["failure"] != 0 or header["outcome"] != 1:
        return "comm_failure_%s" % header["failure"]
    if header["published"] != 18 or header["group"] != 18 or len(groups) != 18 or \
            any(g.values["node"] != node for g in groups):
        return "comm_groups_incomplete"
    if [(g.kind, g.values.get("dir"), g.values.get("pdo")) for g in groups] != COMM_ORDER:
        return "comm_groups_out_of_order"
    if len(results) != 95 or header["terminals"] != 95 or any(r["outcome"] != 0 for r in results):
        return "comm_results_incomplete"
    if any(r.get("dir") != 0 for r in results):
        return "comm_write_present"
    return None


def heartbeat_problem(text, parsed):
    """Check one H record: the profile's own confirmed result on the sent node. Success is failure 0,
    nothing lost, state SUCCEEDED with the write acknowledged, the migration done, readback raw 1000
    matched and published without uncertainty; three RESULTs, 1017:00 upload of 0000, its download
    and its upload of e803; and exactly one non-upload frame to the node, 2B 17 10 00 E8 03 00 00.
    A stored readback is not active producer timing, persistence or a host-loss reaction."""
    node = int(text[1:].split(",")[0])
    headers = [r.values for r in parsed if r.kind == "HB"]
    preparations = [r.values for r in parsed if r.kind == "HB_PREP"]
    ends = [r for r in parsed if r.kind == "END" and r.raw.startswith("END heartbeat")]
    results = [r.values for r in parsed if r.kind == "RESULT"]
    if len(headers) != 1 or len(preparations) != 1 or len(ends) != 1:
        return "heartbeat_record_unparsed"
    header = dict(preparations[0], **headers[0])
    if header["node"] != node:
        return "heartbeat_wrong_node"
    if header["failure"] != 0 or header["lost"] != 0 or ends[0].values.get("lost") != 0:
        return "heartbeat_failure_%s" % header["failure"]
    if (header["state"], header["ack"], header["matched"], header["published"], header["readback"],
            header["raw"], header["uncertain"], header["migrated"]) != (4, 1, 1, 1, 1000, 1000, 0, 1):
        return "heartbeat_not_confirmed"
    if len(results) != 3 or header["terminals"] != 3 or any(r["outcome"] != 0 for r in results):
        return "heartbeat_results_incomplete"
    if [r["object"] for r in results] != [[0x1017, 0]] * 3 or [r.get("dir") for r in results] != [0, 1, 0]:
        return "heartbeat_steps_differ"
    if (results[0]["size"], results[0]["data"][:4], results[2]["size"], results[2]["data"][:4]) != \
            (2, "0000", 2, "e803"):
        return "heartbeat_values_differ"
    writes = [r.values["data"] for r in parsed if r.kind == "TRAFFIC" and r.values.get("k") == 1 and
              r.values.get("id") == 0x600 + node and not r.values["data"].startswith("40")]
    if writes != ["2b171000e8030000"]:
        return "heartbeat_write_bytes_differ"
    return None


def capture_running(lines):
    """True when the last CAP admitted in the capture has no CAP_REC after it: still collecting."""
    running = False
    for line in lines:
        if line.startswith("CAP admitted"):
            running = True
        elif line.startswith("CAP_REC "):
            running = False
    return running


def run_steps(steps, capture, clock):
    """Send the plan; returns (step results, stop reason). Stops at the first refusal or silence."""
    results = []
    reason = "completed"
    for number, step in enumerate(steps, 1):
        entry = {"step": number, "note": step.get("note", "")}
        results.append(entry)
        if "require" in step:
            # Fail closed: a prerequisite that the capture does not show stops the plan unsent.
            problem = requirement_problem(step["require"], capture.lines)
            if problem:
                entry.update(outcome="requirement_failed", problem=problem)
                reason = "requirement_failed"
                break
        if "send" in step:
            text = step["send"]
            start = len(capture.lines)
            entry.update(sent=text, tx_hex=encode(text).hex(), utc=clock.utc(), monotonic=clock.monotonic())
            capture.send(encode(text))
            found = capture.until(step.get("timeout_s", DEFAULT_TIMEOUT_S), _answer(step["expect"]), start)
            if found is None:
                entry["outcome"] = "no_reply"
                reason = "no_reply"
                break
            index, line = found
            entry.update(reply=line, reply_line=index + 1)
            if not line.startswith(step["expect"]):
                entry["outcome"] = "refused"
                reason = "refused"
                break
            if "end" in step:
                end = capture.until(step.get("end_timeout_s", DEFAULT_TIMEOUT_S),
                                    lambda line: line.startswith(step["end"]), index + 1)
                if end is None:
                    entry["outcome"] = "no_end"
                    reason = "no_end"
                    break
                entry.update(end=end[1], end_line=end[0] + 1)
                problem = terminal_problem(step, capture.lines[index:end[0] + 1])
                if problem:
                    # The work ended without its stated success: nothing that depends on it is sent.
                    entry["outcome"] = "terminal_failed"
                    entry["problem"] = problem
                    reason = "terminal_failed"
                    break
            entry["outcome"] = "answered"
        else:
            entry.update(wait_s=step["wait_s"], utc=clock.utc())
            capture.until(step["wait_s"], lambda line: False)
            entry["outcome"] = "waited"
    if reason != "completed" and watch_running(capture.lines):
        # Only the local watch is ended; it sends nothing on CAN. Device state stays as it is.
        start = len(capture.lines)
        capture.send(b"P0\n")
        results.append({"step": "cleanup", "sent": "P0", "tx_hex": b"P0\n".hex(), "utc": clock.utc()})
        end = capture.until(WATCH_STOP_S, lambda line: line.startswith("PDOW_END "), start)
        results[-1]["outcome"] = "watch_ended" if end else "no_watch_end"
    if reason != "completed" and capture_running(capture.lines):
        # Only the local capture is ended; it sends nothing on CAN, and its record stays held.
        start = len(capture.lines)
        capture.send(b"C0\n")
        results.append({"step": "cleanup", "sent": "C0", "tx_hex": b"C0\n".hex(), "utc": clock.utc()})
        end = capture.until(CAPTURE_STOP_S, lambda line: line.startswith("END capture"), start)
        results[-1]["outcome"] = "capture_ended" if end else "no_capture_end"
    return results, reason


def repository_state():
    try:
        revision = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True,
                                  timeout=30, check=False).stdout.strip()
        # Tracked files only: untracked editor or evidence files do not change the code that ran.
        status = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=no"],
                                capture_output=True, text=True, timeout=30, check=False).stdout
        return {"host_revision": revision, "host_dirty": bool(status.strip())}
    except OSError:
        return {"host_revision": None, "host_dirty": None}


def check_identity(port, serial_number, ports):
    if serial_number != FIXTURE["usb_serial"]:
        raise SessionError("USB serial %s is not the recorded fixture %s" % (serial_number, FIXTURE["usb_serial"]))
    named = [item for item in ports if item.device.lower() == port.lower()]
    if len(named) != 1 or named[0].serial_number != serial_number or \
            (named[0].vid, named[0].pid) != (FIXTURE["vid"], FIXTURE["pid"]):
        raise SessionError("port is not the uniquely selected ESP-Prog2")
    return {"port": named[0].device, "serial": named[0].serial_number, "vid": named[0].vid, "pid": named[0].pid}


def preflight(capture, steps=()):
    """Ping once; refuse unless the image speaks the plan's lines and holds no soak or trial record."""
    capture.until(0.2, lambda line: False)  # Retain whatever already waits; never flush unseen evidence.
    start = len(capture.lines)
    capture.send(b"p")
    found = capture.until(PING_TIMEOUT_S, lambda line: line.startswith("PONG "), start)
    if found is None:
        return None, "no_ping"
    values = dict(word.split("=", 1) for word in found[1].split()[1:] if "=" in word)
    letters = {step["send"][0] for step in steps if len(step.get("send", "")) > 1 and step["send"][0] in PDO_LETTERS}
    if values.get("pdo") not in PDO_PROTOCOLS:
        return values, "image_without_pdo_lines"
    if not letters <= PDO_PROTOCOLS[values["pdo"]]:
        return values, "image_without_pdo_config"
    if values.get("soak") != "0":
        return values, "soak_running"
    if values.get("trial") != "0":
        return values, "trial_record_held"
    if values.get("watch") != "0":
        return values, "watch_running"
    # Images before pdo=3 have no capture and print no field; from pdo=3 on it is always printed.
    if values.get("capture", "0") != "0":
        return values, "capture_held"
    return values, None


class RealClock:
    def utc(self):
        return datetime.now(timezone.utc).isoformat()

    def monotonic(self):
        import time
        return time.monotonic()


class RealBackend:
    def comports(self):
        from serial.tools import list_ports
        return list_ports.comports()

    def open(self, device):
        import serial
        port = serial.Serial(port=None, baudrate=BAUD, timeout=0.05, write_timeout=1)
        port.dtr = port.rts = False  # Set before opening, so that opening drives neither EN nor BOOT.
        port.port = device
        port.open()
        return port


def run(args, backend=None, clock=None):
    plan = json.loads(Path(args.plan).read_text(encoding="utf-8"))
    steps = check_plan(plan)
    output = Path(args.output)
    if args.dry_run:
        print("plan %s: %d steps; %s" % (args.plan, len(steps), plan["purpose"]))
        for number, step in enumerate(steps, 1):
            print("  %2d %s" % (number, json.dumps(step)))
        print("would write %s; touches nothing now" % output)
        return 0
    if output.exists():
        raise SessionError("%s exists; evidence is never overwritten" % output)
    backend = backend or RealBackend()
    clock = clock or RealClock()
    usb = check_identity(args.port, args.serial, backend.comports())
    output.mkdir(parents=True)
    (output / "plan.json").write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
    session = {"command": "pdo-session", "started_utc": clock.utc(), "usb": usb, "argv": sys.argv[1:],
               "plan_file": str(args.plan), "reset": False, "retry": False, "tools": tool_digests(),
               **repository_state()}
    port = backend.open(usb["port"])
    capture = Capture(port, clock)
    try:
        pong, refusal = preflight(capture, steps)
        session["pong"] = pong
        if refusal:
            session.update(reason=refusal, steps=[])
        else:
            results, reason = run_steps(steps, capture, clock)
            session.update(reason=reason, steps=results)
            capture.until(args.tail_s, lambda line: False)
    finally:
        try:
            capture.until(0.1, lambda line: False)
        finally:
            port.close()
        session["ended_utc"] = clock.utc()
        session["bytes"] = len(capture.data)
        (output / "rx.bin").write_bytes(bytes(capture.data))
        (output / "rx.txt").write_bytes(bytes(capture.data))
        (output / "session.json").write_text(json.dumps(session, indent=2) + "\n", encoding="utf-8")
    print("session %s: %s, %d bytes" % (output, session["reason"], session["bytes"]))
    return 0 if session["reason"] == "completed" else 1


# ---------------------------------------------------------------- offline analysis

def _le(data, start, size, signed=False):
    value = int.from_bytes(bytes.fromhex(data)[start:start + size], "little")
    if signed and value >= 1 << (8 * size - 1):
        value -= 1 << (8 * size)
    return value


def _raw_records(records):
    """Group RAW headers with their RESULT terminals, in capture order."""
    groups, current = [], None
    for record in records:
        if record.status != "parsed":
            continue
        if record.kind == "ADMITTED" and record.raw.startswith(("PDO_READ admitted", "PDO_WRITE admitted",
                                                                 "HEARTBEAT experiment admitted")):
            current = {"admitted": record.raw.partition(";")[0], "line": record.number, "results": [],
                       "values": dict(record.values)}
            groups.append(current)
        elif record.kind == "RAW" and current is not None:
            current["header"] = dict(record.values)
        elif record.kind == "RESULT" and current is not None and "header" in current and "end" not in current:
            v = record.values
            current["results"].append({
                "object": "%04x:%02x" % tuple(v["object"]), "dir": v.get("dir"), "outcome": v["outcome"],
                "abort": "%08x" % v["abort"], "size": v["size"], "data": v["data"], "completed": v["completed"]})
        elif record.kind == "END" and record.raw.startswith("END raw") and current is not None:
            current["end"] = record.number
            current = None
    return groups


def _value(result):
    size = result["size"]
    return _le(result["data"], 0, size) if result["outcome"] == 0 and 1 <= size <= 4 else None


def pdo_maps(groups):
    """Transmit maps read in the session: {(node, set): [(index, sub, bits), ...]} from PDO_READ."""
    maps = {}
    for group in groups:
        values = group["values"]
        if not group["admitted"].startswith("PDO_READ") or values.get("dir") != "t":
            continue
        by_object = {r["object"]: _value(r) for r in group["results"]}
        base = 0x1A00 + values["set"]
        count = by_object.get("%04x:00" % base)
        if count is None or count > 4:
            continue
        entries = []
        for sub in range(1, count + 1):
            word = by_object.get("%04x:%02x" % (base, sub))
            if word is None:
                entries = None
                break
            entries.append((word >> 16, (word >> 8) & 0xFF, word & 0xFF))
        if entries is not None:
            maps[(values["node"], values["set"])] = entries
    return maps


SIGNED = {0x6064, 0x606C, 0x607A, 0x60FF, 0x6062, 0x6063, 0x60F4}


def decode(data, entries):
    """Decode one payload by a read map; None when the bytes do not fill it exactly."""
    if data == "-":
        data = ""
    out, offset = {}, 0
    for index, sub, bits in entries:
        if bits % 8:
            return None
        size = bits // 8
        if offset + size > len(data) // 2:
            return None
        out["%04x:%02x" % (index, sub)] = _le(data, offset, size, index in SIGNED)
        offset += size
    return out if offset == len(data) // 2 else None


def _stats(times):
    gaps = [b - a for a, b in zip(times, times[1:])]
    if not gaps:
        return {"count": len(times)}
    mean = sum(gaps) / len(gaps)
    sd = (sum((g - mean) ** 2 for g in gaps) / len(gaps)) ** 0.5
    ordered = sorted(gaps)
    return {"count": len(times), "first": times[0], "last": times[-1], "min_gap": ordered[0],
            "max_gap": ordered[-1], "mean_gap": round(mean, 1), "sd_gap": round(sd, 1),
            "p50_gap": ordered[len(ordered) // 2], "p99_gap": ordered[min(len(ordered) - 1, len(ordered) * 99 // 100)]}


def captures(records):
    """Each capture record: its header lines, and per received ID the stored frames' count, receive
    upper bounds and gaps. hi is the ISR clock sample at enqueue, an upper bound, never a bus time;
    pre and post frames are listed but kept out of the gap statistics."""
    out, current = [], None
    for record in records:
        if record.status != "parsed" or record.role != "capture":
            continue
        v = record.values
        if record.kind == "CAP_REC":
            current = {"record": v, "ids": {}, "frames": 0, "boundary": 0}
            out.append(current)
        elif current is None or v.get("cap") != current["record"]["cap"]:
            continue
        elif record.kind == "CAP_F":
            current["frames"] += 1
            key = "%s%x" % ("x" if v["ext"] else "", v["id"])
            entry = current["ids"].setdefault(key, {"count": 0, "hi": [], "dlc": v["dlc"], "data": v["data"]})
            entry["count"] += 1
            entry["data"] = v["data"]
            if v["pre"] or v["post"]:
                current["boundary"] += 1
            elif v["bounds"] or v["exact"]:
                entry["hi"].append(v["hi"])
        elif record.kind in ("CAP_TIME", "CAP_LEVEL", "CAP_IMAGE"):
            current[record.kind[4:].lower()] = v
        elif record.kind in ("CAP_CTRL", "CAP_OWNER"):
            current.setdefault(record.kind[4:].lower(), {})[v["which"]] = v
        elif record.kind == "CAP_END":
            current["end"] = v
    for item in out:
        for entry in item["ids"].values():
            entry["stats"] = _stats(entry.pop("hi"))
    return out


COMMISSIONING_LINES = ("DISC_T", "DISC_ID", "DISC_PROBE", "DISC_SELECT", "COMM_NETWORK", "COMM_NODE", "COMM_PDO",
                       "COMM_MAP", "HB_PREP")


def commissioning(records):
    """Each D, K and H record: its header and typed lines, in capture order. RESULT and TRAFFIC lines
    stay in the capture; the typed values are raw observations, never interpreted settings."""
    out, current = [], None
    for record in records:
        if record.status != "parsed":
            continue
        if record.kind in ("DISC", "COMM", "HB"):
            current = {"kind": record.kind, "line": record.number, "header": dict(record.values), "lines": []}
            out.append(current)
        elif current is not None and record.kind in COMMISSIONING_LINES:
            current["lines"].append(dict(record.values, kind=record.kind))
        elif current is not None and record.kind == "END" and \
                record.raw.startswith(("END discovery", "END comm", "END heartbeat")):
            current = None
    return out


def analyze(directory):
    directory = Path(directory)
    data = (directory / "rx.bin").read_bytes()
    records, unterminated = evidence.parse_bytes(data)
    report = {"directory": str(directory), "bytes": len(data), "lines": len(records),
              "unknown": [r.raw for r in records if r.status == "unknown"],
              "malformed": [(r.number, r.reason, r.raw[:120]) for r in records if r.status == "malformed"],
              "unterminated": unterminated}
    session = directory / "session.json"
    if session.is_file():
        meta = json.loads(session.read_text(encoding="utf-8"))
        report["reason"] = meta.get("reason")
        report["steps"] = [(s.get("step"), s.get("sent"), s.get("outcome")) for s in meta.get("steps", [])]
    # Watches: contiguous n per watch; a gap is console loss, never a device fact.
    watches, current = [], None
    for record in records:
        if record.status != "parsed" or record.role != "watch":
            continue
        v = record.values
        if record.kind == "PDOW_BEGIN":
            current = {"begin": v, "frames": {}, "ids": {}, "sums": [], "end": None, "gaps": [], "next": 1}
            watches.append(current)
            continue
        if current is None:
            continue
        if v["n"] != current["next"]:
            current["gaps"].append((current["next"], v["n"]))
        current["next"] = v["n"] + 1
        if record.kind == "PDOW_F":
            current["frames"].setdefault("%03x" % v["id"], []).append(v)
        elif record.kind == "PDOW_ID":
            current["ids"]["%03x" % v["id"]] = v
        elif record.kind == "PDOW_SUM":
            current["sums"].append(v)
        elif record.kind == "PDOW_END":
            current["end"] = v
    nmts = []
    for record in records:
        if record.status == "parsed" and record.kind == "NMT":
            v = record.values
            nmts.append({"line": record.number, "target": v["target"], "cmd": v["cmd"],
                         "name": NMT_NAMES.get(v["cmd"], "?"), "outcome": v["outcome"],
                         "txConfirmed": v["txConfirmed"], "completed": v["completed"],
                         "admitted": v.get("admitted")})
    report["captures"] = captures(records)
    report["commissioning"] = commissioning(records)
    groups = _raw_records(records)
    maps = pdo_maps(groups)
    report["pdo_records"] = groups
    report["maps"] = {"%d/%d" % key: ["%04x:%02x/%d" % e for e in entries] for key, entries in maps.items()}
    report["nmt"] = nmts
    report["watches"] = []
    for watch in watches:
        last = watch["end"] or (watch["sums"][-1] if watch["sums"] else {})
        out = {"begin": watch["begin"], "end": watch["end"], "console_gaps": watch["gaps"],
               "last_sum": watch["sums"][-1] if watch["sums"] else None,
               "unprinted": last.get("unprinted"), "tap_dropped": last.get("tapDropped"), "ids": {}}
        for ident, frames in sorted(watch["frames"].items()):
            timed = [f["hi"] for f in frames if f["timed"]]
            # Printed frames only: a frame the console could not take is in the device statistics
            # (PDOW_ID) but absent here, so these gaps, and the first and last frames around an NMT
            # request below, hold only while the watch's unprinted count stays 0.
            entry = {"printed": len(frames), "printed_stats": _stats(timed), "device": watch["ids"].get(ident)}
            node = int(ident, 16) & 0x7F
            kind = int(ident, 16) & 0x780
            if kind in (0x180, 0x280, 0x380, 0x480):
                entries = maps.get((node, (kind - 0x180) >> 8))
                if entries:
                    decoded = [decode(f["data"], entries) for f in frames]
                    entry["decoded_first"] = decoded[0]
                    entry["decoded_last"] = decoded[-1]
                    entry["decode_failures"] = sum(1 for d in decoded if d is None)
            if kind == 0x700:
                states = []
                for f in frames:
                    state = f["data"][:2]
                    if not states or states[-1][1] != state:
                        states.append((f["hi"], state))
                entry["heartbeat_states"] = states
            # Around each NMT request to this node: first and last frame after its TX completion.
            around = []
            for nmt in nmts:
                if nmt["target"] != node or not nmt["txConfirmed"]:
                    continue
                # A frame whose receive sample lies between the request's admission and its TX
                # completion may precede or follow the NMT frame on the wire: it is ambiguous, never
                # counted on either side. Older records carry no admission time.
                admitted = nmt.get("admitted")
                lower = admitted if admitted is not None else nmt["completed"]
                after = [t for t in timed if t > nmt["completed"]]
                before = [t for t in timed if t <= lower]
                ambiguous = [t for t in timed if lower < t <= nmt["completed"]]
                around.append({"nmt": nmt["name"], "completed": nmt["completed"], "admitted": admitted,
                               "last_before": before[-1] if before else None,
                               "ambiguous": len(ambiguous),
                               "first_after": after[0] if after else None,
                               "first_after_delay_us": after[0] - nmt["completed"] if after else None,
                               "frames_after": len(after)})
            entry["around_nmt"] = around
            out["ids"][ident] = entry
        report["watches"].append(out)
    return report


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="action", required=True)
    r = sub.add_parser("run", help="Run one plan on the bench")
    r.add_argument("--port", required=True)
    r.add_argument("--serial", required=True, help="Exact ESP-Prog2 USB serial number")
    r.add_argument("--plan", required=True, help="JSON plan: purpose, authorization, steps")
    r.add_argument("--output", required=True, help="New session directory")
    r.add_argument("--tail-s", type=float, default=1.0, help="Capture after the last step (default 1 s)")
    r.add_argument("--dry-run", action="store_true", help="Print the plan; touches nothing")
    a = sub.add_parser("analyze", help="Analyze one retained session offline")
    a.add_argument("directory")
    a.add_argument("--json", help="New JSON file for the full report")
    return parser.parse_args(argv)


def main(argv=None):
    args = arguments(sys.argv[1:] if argv is None else argv)
    try:
        if args.action == "run":
            return run(args)
        report = analyze(args.directory)
        if args.json:
            with open(args.json, "x", encoding="utf-8") as handle:
                json.dump(report, handle, indent=2)
        print(json.dumps({k: report[k] for k in ("directory", "bytes", "lines", "reason") if k in report}))
        print("unknown=%d malformed=%d" % (len(report["unknown"]), len(report["malformed"])))
        for watch in report["watches"]:
            print("watch %s console_gaps=%s unprinted=%s tapDropped=%s" % (
                watch["begin"].get("seconds"), watch["console_gaps"], watch["unprinted"], watch["tap_dropped"]))
            for ident, entry in watch["ids"].items():
                print("  %s %s" % (ident, json.dumps({k: entry[k] for k in entry if k != "device"})))
        for item in report["captures"]:
            r = item["record"]
            print("capture %s reason=%s stored=%s overflow=%s tapDropped=%s lost=%s txRefused=%s" % (
                r["cap"], r["reason"], r["stored"], r["overflow"], r["tapDropped"], r["lost"], r["txRefused"]))
            for ident, entry in sorted(item["ids"].items()):
                print("  %s %s" % (ident, json.dumps(entry)))
        for nmt in report["nmt"]:
            print("nmt %s" % json.dumps(nmt))
        for item in report["commissioning"]:
            print("%s %s" % (item["kind"], json.dumps(item["header"])))
            for entry in item["lines"]:
                print("   %s" % json.dumps(entry))
        for group in report["pdo_records"]:
            print("record %s failure=%s" % (group["admitted"], group.get("header", {}).get("failure")))
            for result in group["results"]:
                print("   %s outcome=%s abort=%s size=%s data=%s" % (
                    result["object"], result["outcome"], result["abort"], result["size"], result["data"]))
        return 0
    except SessionError as error:
        print("refused: %s" % error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
