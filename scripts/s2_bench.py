#!/usr/bin/env python3
"""One bounded S2 bench-console request; preserves raw bytes and never resets/retries.

Single-byte commands send their byte once and capture for --seconds. Trial commands run one named
firmware route (examples/s2_bench/BenchRoute.h): they check the recorded USB identity, require every
route parameter explicitly, require the operator to type the attended confirmation and, for an
enable, the attendant's own shaft observation, then ping, refuse unless the image speaks this
runner's route protocol and nothing else is running, send one T line and capture until the record's
END. Each T line ends in ",K" and the CRC-32 (zlib.crc32) of every byte before it, which the firmware
checks before parsing. Ctrl-C sends one local abort ('A'), which sends no stop. Nothing is retried,
and --dry-run prints the plan without enumerating, prompting, opening or creating anything.

Motion routes also need --campaign: a JSON-lines ledger the operator names, which outlives MCU boots.
One invocation at a time holds it, through an exclusive lock file next to it from before the ledger
is read until its result entry is written. Each entry carries its sequence number and the SHA-256 of
the line before it. Before a motion line is sent the ledger is read and checked again under the lock
and the line's entry is appended and synced; the tool refuses once the campaign holds
CAMPAIGN_MOTION_TOTAL motion admissions, CAMPAIGN_PER_CASE for one route and node, or a trial
identity at or above this one. A missing ledger is refused unless --new-campaign starts it, so a
mistyped path cannot reset the counts. The chain cannot show a cut tail or a restored older copy:
commit the ledger with each session's evidence.

--unattended-waiver TEXT runs a trial with nobody at the bench, only on the user's own written
statement that they accept it: TEXT is that statement, stored verbatim in request.json and the send
entry. No attendant question is asked, and every observation only an attendant could make is stored
as "unattended: not observed". The line's attended field still reads 1: it records that the host's
gate was satisfied, here by the waiver. An enable is refused, because its start report needs the
attendant's power-up shaft observation, and so is --fault-led prompt.
"""
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import signal
import sys
import zlib

COMMANDS = {"ping": b"p", "start": b"c", "stats": b"s",
            "identity1": b"1", "identity2": b"2", "raw1": b"3", "raw2": b"4",
            "dump": b"d", "release": b"r", "network": b"n",
            "heartbeat1": b"h", "heartbeat2": b"j", "preop1": b"q", "preop2": b"w", "tpdo1": b"m",
            "motion-readiness1": b"v", "motion-readiness2": b"b", "soak-stop": b"X",
            "trial-abort": b"A"}
# The recorded fixture, as in s2_soak.py and s2_flash.py. A changed fixture needs a reviewed edit.
FIXTURE = {"port": "COM15", "usb_serial": "94A990398B40", "vid": 0x303A, "pid": 0x1002}
BAUD = 115200
# Route letter after T, whether it moves state, and the packet plan it serves.
TRIALS = {
    "readiness-actions": ("A", False, "packet 15 P1"),
    "readiness-io": ("I", False, "packet 13 P1"),
    "readiness-tuning": ("U", False, "packet 14 T0"),
    "mode1": ("M", True, "packet 15 P2"),
    "enable": ("E", True, "packet 15 P3"),
    "stop": ("S", True, "packet 15 P4 / packet 16"),
    "position": ("P", True, "packet 19"),
    "rearm": ("R", True, "packet 19 rearm"),
    "move": ("G", True, "packet 20"),
}
# PONG routes= of the line protocol this runner speaks: 3 adds the CRC-32 field. Any other image
# would misread the line, so it is refused before anything is sent.
ROUTE_PROTOCOL = 3
MOTION_ROUTES = frozenset(letter for letter, motion, _ in TRIALS.values() if motion)
# The first entry's predecessor digest.
LEDGER_GENESIS = "0" * 64
STOP_KINDS = {"halt": 1, "quick": 2}
SIGNAL_SOURCES = {"statusword": 1, "monitor": 2}
COMPLETION_RULES = {"after-absence": 1, "after-acceptance": 2}
# Packet 22's host campaign caps, across boots; the firmware enforces the same numbers per boot only.
CAMPAIGN_MOTION_TOTAL = 12
CAMPAIGN_PER_CASE = 3
SHAFT = {"FREE": 1, "HOLDING": 2}
PING_TIMEOUT_S = 3.0
CAPTURE_LIMIT = 1 << 20
UNOBSERVED = "unattended: not observed"


class Refused(RuntimeError):
    """Nothing was sent: the message names the fact that failed."""


class RealBackend:
    """pyserial, imported only here so that --dry-run and the tests never touch it."""

    def comports(self):
        from serial.tools import list_ports
        return list_ports.comports()

    def open(self, device):
        import serial
        port = serial.Serial(port=None, baudrate=BAUD, timeout=0.05, write_timeout=1)
        # Set before opening, so that opening drives neither EN nor BOOT.
        port.dtr = port.rts = False
        port.port = device
        port.open()
        return port


class RealClock:
    def utc(self):
        return datetime.now(timezone.utc).isoformat()

    def monotonic(self):
        import time
        return time.monotonic()


def check_identity(port, serial_number, ports, trial):
    """Exactly the named device on exactly the named port; a trial also needs the recorded fixture."""
    if trial and serial_number != FIXTURE["usb_serial"]:
        raise Refused("USB serial %s is not the recorded fixture %s" % (serial_number, FIXTURE["usb_serial"]))
    named = [item for item in ports if item.device.lower() == port.lower()]
    if len(named) != 1 or named[0].serial_number != serial_number or \
            (named[0].vid, named[0].pid) != (FIXTURE["vid"], FIXTURE["pid"]):
        raise Refused("port is not the uniquely selected ESP-Prog2")
    return {"port": named[0].device, "serial": named[0].serial_number, "vid": named[0].vid, "pid": named[0].pid}


def _within(parser, name, value, low, high):
    if value is None:
        parser.error("--%s is required for this route" % name)
    if not low <= value <= high:
        parser.error("--%s must be %d..%d" % (name, low, high))
    return value


def _stop_fields(args, parser, cycles):
    """The planned-stop tail shared by position and move: none names nothing, a stop names everything."""
    if args.stop_kind is None:
        parser.error("--stop-kind is required for this route (none, halt or quick)")
    if args.stop_kind == "none":
        if any(v is not None for v in (args.stop_after, args.stop_deadline_ms, args.stop_best_effort_ms,
                                       args.stop_interval_ms)):
            parser.error("--stop-kind none takes no other stop argument")
        return [0, 0, 0, 0, 0]
    deadline = _within(parser, "stop-deadline-ms", args.stop_deadline_ms, 1, 55000)
    best = _within(parser, "stop-best-effort-ms", args.stop_best_effort_ms, 0, 55000)
    if best and best < deadline:
        parser.error("--stop-best-effort-ms must be 0 or at least --stop-deadline-ms")
    return [STOP_KINDS[args.stop_kind], _within(parser, "stop-after", args.stop_after, 1, cycles),
            deadline, best, _within(parser, "stop-interval-ms", args.stop_interval_ms, 10, 10000)]


def _signal(parser, args, name):
    """One host-reviewed move signal: source, nonzero mask and a value inside it. Never defaulted."""
    source = getattr(args, name + "_source")
    if source is None:
        parser.error("--%s-source is required for this route (statusword or monitor)" % name)
    mask = _within(parser, name + "-mask", getattr(args, name + "_mask"), 1, 0xFFFF)
    value = _within(parser, name + "-value", getattr(args, name + "_value"), 0, 0xFFFF)
    if value & ~mask:
        parser.error("--%s-value has bits outside --%s-mask" % (name, name))
    return [SIGNAL_SOURCES[source], mask, value]


def trial_line(args, parser):
    """The firmware's parseRouteCommand text, with the same ranges, from explicit arguments only."""
    node = _within(parser, "node", args.node, 1, 2)
    trial = _within(parser, "trial", args.trial, 1, 0xFFFFFFFF)
    fields = [node, trial]
    if args.command == "mode1":
        fields += [1, _within(parser, "cycles", args.cycles, 1, 10), _within(parser, "interval-ms", args.interval_ms, 100, 5000)]
    elif args.command == "enable":
        if not args.no_motion_write_recorded:
            parser.error("--no-motion-write-recorded is required: the trial log must show no 6040, target, "
                         "ramp or mode write since power-up other than one acknowledged 6060=1")
        fields += [1, 1, 0,  # Shaft placeholder: replaced by the attendant's typed observation.
                   _within(parser, "status", args.status, 0, 0xFFFF),
                   _within(parser, "position-target", args.position_target, -2**31, 2**31 - 1),
                   _within(parser, "velocity-target", args.velocity_target, -2**31, 2**31 - 1),
                   _within(parser, "deadline-ms", args.deadline_ms, 1, 55000),
                   _within(parser, "interval-ms", args.interval_ms, 10, 10000)]
    elif args.command == "stop":
        if args.kind is None:
            parser.error("--kind is required for this route")
        deadline = _within(parser, "deadline-ms", args.deadline_ms, 1, 55000)
        best = _within(parser, "best-effort-ms", args.best_effort_ms, 0, 55000)
        if best and best < deadline:
            parser.error("--best-effort-ms must be 0 or at least --deadline-ms")
        fields += [1, STOP_KINDS[args.kind], deadline, best, _within(parser, "interval-ms", args.interval_ms, 10, 10000)]
    elif args.command == "position":
        if args.stop_kind is None:
            parser.error("--stop-kind is required for this route (none, halt or quick)")
        cycles = _within(parser, "cycles", args.cycles, 1, 16)
        fields += [1, _within(parser, "start-position", args.start_position, -2**31, 2**31 - 1),
                   _within(parser, "target", args.target, -1000000, 1000000),
                   _within(parser, "rpm", args.rpm, 0, 3000),
                   _within(parser, "ramp-up-ms", args.ramp_up_ms, 0, 2000),
                   _within(parser, "ramp-down-ms", args.ramp_down_ms, 0, 2000), cycles,
                   _within(parser, "interval-ms", args.interval_ms, 20, 5000)]
        fields += _stop_fields(args, parser, cycles)
    elif args.command == "rearm":
        fields += [1, _within(parser, "cycles", args.cycles, 1, 16),
                   _within(parser, "interval-ms", args.interval_ms, 20, 5000)]
    elif args.command == "move":
        if args.stop_kind is None:
            parser.error("--stop-kind is required for this route (none, halt or quick)")
        if args.rule is None:
            parser.error("--rule is required for this route (after-absence or after-acceptance)")
        fields += [1, _within(parser, "start-position", args.start_position, -2**31, 2**31 - 1),
                   _within(parser, "target", args.target, -1000000, 1000000),
                   _within(parser, "rpm", args.rpm, 1, 3000),
                   _within(parser, "ramp-up-ms", args.ramp_up_ms, 1, 2000),
                   _within(parser, "ramp-down-ms", args.ramp_down_ms, 1, 2000),
                   _within(parser, "deadline-ms", args.deadline_ms, 1, 55000),
                   _within(parser, "interval-ms", args.interval_ms, 20, 5000),
                   _within(parser, "record", args.record, 1, 0xFFFFFFFF)]
        fields += _signal(parser, args, "accept") + _signal(parser, args, "complete")
        fields += [COMPLETION_RULES[args.rule]]
        for name, high in (("device-type", 0xFFFFFFFF), ("hardware-version", 0xFFFF), ("software-version", 0xFFFF),
                           ("vendor-id", 0xFFFFFFFF), ("product-code", 0xFFFFFFFF), ("revision", 0xFFFFFFFF),
                           ("model-code", 0xFFFF), ("firmware-code", 0xFFFF)):
            fields.append(_within(parser, name, getattr(args, name.replace("-", "_")), 0, high))
        fields += _stop_fields(args, parser, 16)
    if args.command != "readiness-actions" and args.fault_led is not None:
        parser.error("--fault-led belongs to readiness-actions")
    if args.command == "readiness-actions" and args.fault_led is None:
        parser.error("--fault-led is required: prompt, or unobserved when nobody can see the drive")
    return fields


def sign_line(body):
    """The T line with its integrity field: CRC-32 (zlib, IEEE) of every byte before ",K"."""
    return "%s,K%08x" % (body, zlib.crc32(body.encode("ascii")))


def encode(command, fields):
    return (sign_line("T%s%s" % (TRIALS[command][0], ",".join(str(v) for v in fields))) + "\n").encode("ascii")


def motion_weight(command, fields):
    """Motion admissions this line counts, as the firmware counts them: one, plus a planned stop."""
    if not TRIALS[command][1]:
        return 0, 0
    stop = 1 if command in ("position", "move") and fields[-5] else 0
    return 1 + stop, stop


def _integer(value, low, high):
    """A JSON integer in range; a bool or a float is not one."""
    return type(value) is int and low <= value <= high


def _send_problem(entry):
    """What makes a send entry inconsistent with itself, or None."""
    route, node, trial, motion, stop = (entry.get(k) for k in ("route", "node", "trial", "motion", "stop"))
    if route not in {letter for letter, _, _ in TRIALS.values()} or not _integer(node, 1, 2):
        return "route or node"
    if not _integer(trial, 1, 0xFFFFFFFF):
        return "trial is not an integer 1..4294967295"
    if not _integer(stop, 0, 1) or (stop and route not in ("P", "G")) or not _integer(motion, 0, 2):
        return "stop or motion"
    if motion != ((1 + stop) if route in MOTION_ROUTES else 0):
        return "motion %d does not match route %s with stop %d" % (motion, route, stop)
    line = entry.get("line")
    body, separator, _ = line.rpartition(",K") if isinstance(line, str) else ("", "", "")
    if not separator or sign_line(body) != line:
        return "line has no matching integrity field"
    fields = body[2:].split(",")
    if body[:2] != "T" + route or fields[:2] != [str(node), str(trial)]:
        return "line names another route, node or trial"
    if route in ("P", "G") and (fields[-5] != "0") != bool(stop):
        return "line's stop kind does not match stop %d" % stop
    return None


class CampaignLock:
    """An exclusive lock file next to the ledger for one whole invocation. Only its holder removes it;
    one left by a crash is refused until the operator checks the ledger and removes it."""

    def __init__(self, ledger):
        self.path = Path(str(ledger) + ".lock")
        self.held = False

    def acquire(self, clock):
        try:
            descriptor = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        except FileExistsError:
            raise Refused("campaign busy: %s exists. Another invocation holds this ledger, or one ended "
                          "without releasing it; when no runner is active, check the ledger and remove %s "
                          "yourself" % (self.path, self.path))
        except OSError as error:
            raise Refused("campaign lock %s cannot be created: %s" % (self.path, error))
        self.held = True
        try:
            os.write(descriptor, (json.dumps({"pid": os.getpid(), "utc": clock.utc()}) + "\n").encode("utf-8"))
            os.fsync(descriptor)
        finally:
            os.close(descriptor)
        return self

    def release(self):
        if self.held:
            self.held = False
            os.remove(self.path)


class Campaign:
    """The host's campaign ledger: JSON lines, appended and synced, never rewritten or reset here.

    A "send" entry is written before its line is sent, so a crash after sending still counts it; a
    "result" entry follows the capture. Every send counts, admitted or refused by the firmware: each is
    a deliberate trial. Counts never reset with an MCU boot, a new image or a released record. Each
    entry holds "seq", its line number from 1, and "prev", the SHA-256 of the previous line's bytes
    without its LF (LEDGER_GENESIS for the first), so a changed, removed or inserted line is refused.
    A cut tail or a restored older copy is still a valid chain: git history of the committed ledger is
    the anchor for those.
    """

    def __init__(self, path, new):
        self.path, self.new = Path(path), new
        self.entries = []
        self.last_digest = LEDGER_GENESIS

    def load(self):
        exists = self.path.exists()
        if self.new and exists:
            raise Refused("--new-campaign names an existing ledger %s" % self.path)
        if not self.new and not exists:
            raise Refused("campaign ledger %s does not exist; name the campaign's ledger, or start one with "
                          "--new-campaign" % self.path)
        self.entries = []
        self.last_digest = LEDGER_GENESIS
        if not exists:
            return self
        raw = self.path.read_bytes()
        if raw and not raw.endswith(b"\n"):
            raise Refused("campaign ledger %s ends in a partial line; it cannot be counted" % self.path)
        previous, last_send = LEDGER_GENESIS, None
        for number, line in enumerate(raw.split(b"\n")[:-1], 1):
            try:
                entry = json.loads(line.decode("utf-8"))
            except ValueError:
                raise Refused("campaign ledger line %d is not JSON; it cannot be counted" % number)
            if not isinstance(entry, dict) or entry.get("event") not in ("send", "result"):
                raise Refused("campaign ledger line %d is not a send or result entry" % number)
            if not _integer(entry.get("seq"), number, number):
                raise Refused("campaign ledger line %d has seq %r: a line is missing, added or reordered" %
                              (number, entry.get("seq")))
            if entry.get("prev") != previous:
                raise Refused("campaign ledger line %d does not chain to line %d: an earlier line changed" %
                              (number, number - 1))
            if entry["event"] == "send":
                problem = _send_problem(entry)
                if problem:
                    raise Refused("campaign ledger line %d is an inconsistent send entry: %s" % (number, problem))
                last_send = entry
            elif last_send is None or entry.get("trial") != last_send["trial"] or \
                    entry.get("node") != last_send["node"]:
                raise Refused("campaign ledger line %d is a result entry without its send entry" % number)
            else:
                last_send = None
            previous = hashlib.sha256(line).hexdigest()
            self.entries.append(entry)
        self.last_digest = previous
        return self

    def counts(self):
        total, cases = 0, Counter()
        for entry in self.entries:
            if entry["event"] != "send" or not entry["motion"]:
                continue
            total += entry["motion"]
            cases[(entry["route"], entry["node"])] += 1
            cases[("S", entry["node"])] += entry["stop"]
        return total, cases

    def check(self, command, fields):
        """Refuse before anything is sent: a reused trial identity, or a motion line over a cap."""
        letter, node, trial = TRIALS[command][0], fields[0], fields[1]
        sends = [entry for entry in self.entries if entry["event"] == "send"]
        highest = max((entry["trial"] for entry in sends), default=0)
        if trial <= highest:
            raise Refused("trial %d is not above every trial of this campaign (highest %d)" % (trial, highest))
        motion, stop = motion_weight(command, fields)
        if not motion:
            return
        total, cases = self.counts()
        if total + motion > CAMPAIGN_MOTION_TOTAL:
            raise Refused("the campaign holds %d motion admissions; this line would make %d, over %d" %
                          (total, total + motion, CAMPAIGN_MOTION_TOTAL))
        if cases[(letter, node)] + 1 > CAMPAIGN_PER_CASE:
            raise Refused("the campaign holds %d %s admissions on node %d, the cap is %d" %
                          (cases[(letter, node)], command, node, CAMPAIGN_PER_CASE))
        if stop and cases[("S", node)] + 1 > CAMPAIGN_PER_CASE:
            raise Refused("the planned stop would be stop admission %d on node %d, the cap is %d" %
                          (cases[("S", node)] + 1, node, CAMPAIGN_PER_CASE))

    def summary(self):
        total, cases = self.counts()
        return {"path": str(self.path), "motion_total": total,
                "cases": {"%s%d" % key: count for key, count in sorted(cases.items()) if count}}

    def append(self, entry):
        """One synced line chained to the last; an error propagates, and then nothing is sent."""
        entry = dict(entry, seq=len(self.entries) + 1, prev=self.last_digest)
        line = json.dumps(entry, sort_keys=True).encode("utf-8")
        with open(self.path, "ab") as ledger:
            if ledger.write(line + b"\n") != len(line) + 1:
                raise OSError("short campaign ledger write")
            ledger.flush()
            os.fsync(ledger.fileno())
        self.entries.append(entry)
        self.last_digest = hashlib.sha256(line).hexdigest()


def ask_operator(ask, record, key, question, accept):
    """One typed answer; accept returns the stored value or None to refuse."""
    try:
        answer = ask(question).strip()
    except EOFError:
        answer = ""
    value = accept(answer)
    record[key] = {"question": question, "typed": answer, "accepted": value is not None}
    if value is None:
        raise Refused("%s: typed %r" % (key, answer))
    return value


def before_send(args, fields, ask, operator):
    """Typed confirmations before anything is sent. The firmware takes these as given."""
    node, trial = fields[0], fields[1]
    _, motion, _ = TRIALS[args.command]
    if motion and args.unattended_waiver:
        operator["attended"] = {"unattended": True, "waiver": args.unattended_waiver, "accepted": True}
    elif motion:
        wanted = "ATTENDED %d" % trial
        ask_operator(ask, operator, "attended",
                     "Attendant at node %d with the independent drive-power cutoff in hand and a shaft mark "
                     "visible: type %s to confirm: " % (node, wanted),
                     lambda text: text if text == wanted else None)
    if args.command == "enable":
        shaft = ask_operator(ask, operator, "power_up_shaft",
                             "Attendant: turn the node %d shaft by hand now. Type FREE or HOLDING: " % node,
                             lambda text: SHAFT.get(text))
        fields[4] = shaft
    if args.command == "readiness-actions" and args.fault_led == "prompt":
        ask_operator(ask, operator, "fault_led_before",
                     "Fault LED of node %d now, as seen (for example off, red, red blinking): " % node,
                     lambda text: text or None)
    elif args.command == "readiness-actions":
        operator["fault_led_before"] = {"typed": "unobserved", "accepted": True}


def after_capture(args, fields, ask, operator):
    """Observations after the route; recorded, never sent and never interpreted."""
    node = fields[0]
    questions = []
    if TRIALS[args.command][1]:
        questions.append(("shaft_after", "Attendant: describe the node %d shaft now: movement, direction, "
                                         "approximate rotation against the mark, or NONE: " % node))
    if args.command == "enable":
        questions.append(("holding_after", "Attendant: turn the node %d shaft by hand now: FREE or HOLDING? " % node))
    if args.command == "readiness-actions" and args.fault_led == "prompt":
        questions.append(("fault_led_after", "Fault LED of node %d now, as seen: " % node))
    for key, question in questions:
        if args.unattended_waiver:
            operator[key] = {"question": question, "typed": UNOBSERVED}
            continue
        try:
            answer = ask(question).strip()
        except EOFError:
            answer = ""
        operator[key] = {"question": question, "typed": answer or "not recorded"}


def repository_state():
    import subprocess
    root = Path(__file__).resolve().parents[1]
    try:
        revision = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"], capture_output=True, text=True,
                                  timeout=30, check=False).stdout.strip()
        status = subprocess.run(["git", "-C", str(root), "status", "--porcelain"], capture_output=True, text=True,
                                timeout=30, check=False).stdout
        return {"host_revision": revision, "host_dirty": bool(status.strip())}
    except OSError:
        return {"host_revision": None, "host_dirty": None}


class Capture:
    """Raw bytes as received; lines are parsed only to know when to stop."""

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
                raise RuntimeError("bounded capture capacity exceeded")
        return chunk

    def until(self, seconds, done):
        deadline = self.clock.monotonic() + seconds
        seen = len(self.lines)
        while self.clock.monotonic() < deadline:
            self.read()
            for line in self.lines[seen:]:
                if done(line):
                    return line
            seen = len(self.lines)
        return None

    def send(self, data):
        if self.port.write(data) != len(data):
            raise RuntimeError("short command write; do not replay")


def pong_values(line):
    return dict(word.split("=", 1) for word in line.split()[1:] if "=" in word)


def run_trial(args, fields, port, clock, meta, record=None):
    """Ping, check the image, send one line, capture to the record's END. Returns the stop reason.
    record, when given, is called just before the line is written; if it raises nothing is sent."""
    capture = Capture(port, clock)
    capture.until(0.2, lambda line: False)  # Retain whatever already waits; never flush unseen evidence.
    meta["prefix_bytes"] = len(capture.data)
    capture.send(b"p")
    pong = capture.until(PING_TIMEOUT_S, lambda line: line.startswith("PONG "))
    meta["sent"] = ["70"]
    reason = None
    if pong is None:
        reason = "no_ping"
    else:
        values = pong_values(pong)
        meta["image"] = {key: values.get(key) for key in ("base", "elf", "sdk", "target")}
        routes = values.get("routes", "0")
        if routes == "0":
            reason = "image_without_routes"  # An older image would read the line as single-byte commands.
        elif routes != str(ROUTE_PROTOCOL):
            reason = "image_route_version"  # Another line protocol, older or newer; nothing is sent.
        elif values.get("soak") != "0":
            reason = "soak_running"
        elif values.get("trial") != "0":
            reason = "trial_record_held"
        elif values.get("active") != "1" or values.get("running") != "1":
            reason = "can_not_started"
    if reason is None:
        line = encode(args.command, fields)
        meta.update(line=line.decode("ascii").strip(), tx_hex=line.hex())
        if record is not None:
            try:
                record(line.decode("ascii").strip())
            except OSError as error:
                meta["campaign_error"] = str(error)
                return capture, "campaign_unwritten"
            except Refused as error:  # The ledger read again under the lock refused this line.
                meta["campaign_error"] = str(error)
                return capture, "campaign_refused"
        capture.send(line)
        meta["sent"].append(line.hex())
        aborted = []

        def interrupted(_number, _frame):
            if not aborted:  # One local abort, never a stop and never repeated.
                aborted.append(True)
                capture.send(b"A")
                meta["sent"].append("41")

        previous = None
        try:
            previous = signal.signal(signal.SIGINT, interrupted)
        except ValueError:
            pass  # Not the main thread, as under a test runner.
        try:
            end = capture.until(args.seconds, lambda text: text.startswith(("END trial ", "TRIAL_REFUSED ")))
        finally:
            if previous is not None:
                signal.signal(signal.SIGINT, previous)
        meta["aborted"] = bool(aborted)
        reason = "no_end" if end is None else "refused" if end.startswith("TRIAL_REFUSED") else "ended"
    return capture, reason


def dry_run(args, fields):
    print("DRY RUN: no port enumerated or opened, no prompt, nothing created. Verdict: INCOMPLETE.")
    print("  require USB %s %04X:%04X uniquely on %s" % (args.serial, FIXTURE["vid"], FIXTURE["pid"], args.port))
    if args.command in TRIALS:
        _, motion, plan = TRIALS[args.command]
        if motion and args.unattended_waiver:
            print("  unattended under the user's waiver %r: no attendant question; observations stored as %r" %
                  (args.unattended_waiver, UNOBSERVED))
        elif motion:
            print("  require the operator to type: ATTENDED %d" % fields[1])
        if args.campaign is not None:
            lock = CampaignLock(args.campaign).path
            print("  hold %s for the whole run%s" % (lock, "; would refuse now: campaign busy" if lock.exists()
                                                       else ""))
            try:
                campaign = Campaign(args.campaign, args.new_campaign).load()
                campaign.check(args.command, fields)
                print("  campaign %s: %s; this line would be appended before it is sent" %
                      (args.campaign, campaign.summary()))
            except Refused as error:
                print("  campaign %s would refuse: %s" % (args.campaign, error))
        if args.command == "enable":
            print("  require the attendant to type FREE or HOLDING; it replaces the 0 in field 5 below")
        print("  send p; refuse unless PONG has routes=%d soak=0 trial=0 active=1 running=1" % ROUTE_PROTOCOL)
        print("  then send once: %r (%s)" % (encode(args.command, fields), plan))
        print("  capture until END trial or TRIAL_REFUSED, at most %s s; Ctrl-C sends one A" % args.seconds)
    else:
        print("  send once: %r, capture %s s" % (COMMANDS[args.command], args.seconds))
    print("  write %s" % args.output)


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True)
    parser.add_argument("--serial", required=True, help="Exact ESP-Prog2 USB serial number")
    parser.add_argument("--command", choices=sorted(set(COMMANDS) | set(TRIALS)), required=True)
    parser.add_argument("--seconds", type=float, help="Capture time: 1..30, or 1..300 for a trial (default 120)")
    parser.add_argument("--output", type=Path, help="New evidence directory")
    parser.add_argument("--dry-run", action="store_true", help="Print the plan; touches nothing")
    trial = parser.add_argument_group("trial routes; every value is required explicitly")
    for name in ("node", "trial", "cycles", "interval-ms", "position-target", "velocity-target", "deadline-ms",
                 "best-effort-ms", "start-position", "target", "rpm", "ramp-up-ms", "ramp-down-ms", "stop-after",
                 "stop-deadline-ms", "stop-best-effort-ms", "stop-interval-ms", "record"):
        trial.add_argument("--" + name, type=int)
    trial.add_argument("--status", type=lambda text: int(text, 0), help="Reviewed 6041, for example 0x8037")
    trial.add_argument("--kind", choices=("halt", "quick"))
    trial.add_argument("--stop-kind", choices=("none", "halt", "quick"))
    move = parser.add_argument_group("move: the reviewed measurement record's evidence, never defaulted")
    for name in ("accept", "complete"):
        move.add_argument("--%s-source" % name, choices=sorted(SIGNAL_SOURCES))
        move.add_argument("--%s-mask" % name, type=lambda text: int(text, 0))
        move.add_argument("--%s-value" % name, type=lambda text: int(text, 0))
    move.add_argument("--rule", choices=sorted(COMPLETION_RULES))
    for name in ("device-type", "hardware-version", "software-version", "vendor-id", "product-code", "revision",
                 "model-code", "firmware-code"):
        move.add_argument("--" + name, type=lambda text: int(text, 0), help="Raw identity value of the record")
    ledger = parser.add_argument_group("campaign ledger, across boots")
    ledger.add_argument("--campaign", type=Path, help="JSON-lines campaign ledger; required for motion routes")
    ledger.add_argument("--new-campaign", action="store_true", help="Start the named ledger; refused if it exists")
    trial.add_argument("--fault-led", choices=("prompt", "unobserved"))
    trial.add_argument("--no-motion-write-recorded", action="store_true",
                       help="The trial log shows no motion write since power-up apart from one acknowledged 6060=1")
    trial.add_argument("--unattended-waiver", metavar="TEXT",
                       help="The user's own written acceptance of a trial with nobody at the bench, verbatim")
    args = parser.parse_args(argv)
    is_trial = args.command in TRIALS
    if args.unattended_waiver is not None:
        if not is_trial:
            parser.error("--unattended-waiver belongs to trial commands")
        if not args.unattended_waiver.strip():
            parser.error("--unattended-waiver needs the user's statement")
        if args.command == "enable":
            parser.error("--unattended-waiver refuses enable: its start report needs the attendant's power-up "
                         "shaft observation")
        if args.fault_led == "prompt":
            parser.error("--unattended-waiver refuses --fault-led prompt: nobody can see the drive")
    if args.seconds is None:
        args.seconds = 120.0 if is_trial else 4.0
    if not 1 <= args.seconds <= (300 if is_trial else 30):
        parser.error("seconds must be 1..%d" % (300 if is_trial else 30))
    if args.output is None and not args.dry_run:
        parser.error("--output is required")
    fields = trial_line(args, parser) if is_trial else None
    if not is_trial and any(getattr(args, name) is not None for name in ("node", "trial")):
        parser.error("--node and --trial belong to trial commands")
    if not is_trial and (args.campaign is not None or args.new_campaign):
        parser.error("--campaign belongs to trial commands")
    if is_trial and TRIALS[args.command][1] and args.campaign is None:
        parser.error("--campaign is required for a motion route: name the campaign's ledger; counts are kept "
                     "across boots")
    if args.new_campaign and args.campaign is None:
        parser.error("--new-campaign needs --campaign")
    return args, fields


def main(argv=None, backend=None, clock=None, ask=input):
    args, fields = arguments(argv)
    if args.dry_run:
        dry_run(args, fields)
        return 0
    backend = backend or RealBackend()
    clock = clock or RealClock()
    is_trial = args.command in TRIALS
    operator = {}
    campaign = lock = None
    try:
        usb = check_identity(args.port, args.serial, backend.comports(), is_trial)
        if is_trial and args.campaign is not None:
            # Held from before the ledger is read until its result entry is written.
            lock = CampaignLock(args.campaign).acquire(clock)
            campaign = Campaign(args.campaign, args.new_campaign).load()
            campaign.check(args.command, fields)
        if is_trial:
            before_send(args, fields, ask, operator)
    except Refused as error:
        if lock is not None:
            lock.release()
        print("REFUSED: %s; nothing sent or created" % error, file=sys.stderr)
        return 2
    meta = {}
    try:
        return capture_request(args, argv, fields, backend, clock, ask, usb, operator, campaign, meta)
    finally:
        if lock is not None and "campaign_error" in meta:
            # A ledger that could not be written, or changed under the lock, needs the operator first.
            print("campaign lock %s kept: %s; check the ledger before removing the lock" %
                  (lock.path, meta["campaign_error"]), file=sys.stderr)
        elif lock is not None:
            lock.release()


def capture_request(args, argv, fields, backend, clock, ask, usb, operator, campaign, meta):
    """Everything after the refusals: the evidence directory, the one request, the ledger entries."""
    is_trial = args.command in TRIALS
    args.output.mkdir(parents=True, exist_ok=False)
    meta.update({"started_utc": clock.utc(), "port": args.port, "serial": args.serial, "usb": usb, "baud": BAUD,
                 "command": args.command, "seconds": args.seconds, "reset": False, "retry": False})
    if is_trial:
        meta.update(node=fields[0], trial=fields[1], plan=TRIALS[args.command][2], fields=list(fields),
                    operator=operator, argv=list(argv) if argv is not None else sys.argv[1:])
        meta.update(repository_state())
        if campaign is not None:
            meta["campaign"] = dict(campaign.summary(), before=True)
    else:
        meta["tx_hex"] = COMMANDS[args.command].hex()
    motion, stop = motion_weight(args.command, fields) if is_trial else (0, 0)

    def record(line):
        """The ledger read and checked again under the lock, then the send entry, synced before the line
        goes out: it counts even if the host then fails."""
        campaign.load()
        campaign.check(args.command, fields)
        entry = {"event": "send", "utc": clock.utc(), "command": args.command,
                 "route": TRIALS[args.command][0], "node": fields[0], "trial": fields[1],
                 "motion": motion, "stop": stop, "line": line, "output": str(args.output),
                 "image": meta.get("image")}
        if args.unattended_waiver:
            entry["unattended_waiver"] = args.unattended_waiver
        campaign.append(entry)
    data = bytearray()
    try:
        port = backend.open(args.port)
        try:
            if is_trial:
                capture, reason = run_trial(args, fields, port, clock, meta, record if campaign else None)
                data = capture.data
                meta["reason"] = reason
            else:
                # Retain any already waiting output; never flush unseen evidence.
                data.extend(port.read(port.in_waiting))
                meta["prefix_bytes"] = len(data)
                if port.write(COMMANDS[args.command]) != 1:
                    raise RuntimeError("short command write; do not replay")
                until = clock.monotonic() + args.seconds
                while clock.monotonic() < until:
                    data.extend(port.read(min(max(port.in_waiting, 1), 4096)))
                    if len(data) > 131072:
                        raise RuntimeError("bounded capture capacity exceeded")
        finally:
            port.close()
        meta["status"] = "captured"
    except Exception as error:  # Raw bytes and the reason are kept; nothing is replayed.
        meta["status"] = "failed"
        meta["error"] = str(error)
        raise
    finally:
        meta["ended_utc"] = clock.utc()
        if campaign is not None and "line" in meta and \
                meta.get("reason") not in ("campaign_unwritten", "campaign_refused"):
            try:  # The send entry already counts it; this records how it ended.
                campaign.append({"event": "result", "utc": meta["ended_utc"], "trial": fields[1],
                                 "node": fields[0], "reason": meta.get("reason"), "status": meta.get("status"),
                                 "aborted": meta.get("aborted", False), "output": str(args.output)})
            except OSError as error:
                meta["campaign_error"] = str(error)
        if is_trial and meta.get("status") == "captured":
            after_capture(args, fields, ask, operator)
        (args.output / "rx.bin").write_bytes(bytes(data))
        (args.output / "rx.txt").write_bytes(bytes(data).decode("utf8", errors="replace").encode("utf8"))
        (args.output / "request.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf8")
    print(bytes(data).decode("utf8", errors="replace"), end="")
    print("\nEvidence:", args.output)
    if not is_trial:
        return 0
    import s2_evidence  # Imported here: s2_evidence imports this module's COMMANDS.
    report = s2_evidence.replay_capture(args.output, manifest=Path(args.output) / "absent.json")
    print("verdict:", report.verdict, "failure:", report.failure_name or report.failure, "reason:", meta["reason"])
    # 2: nothing admitted; 1: damaged, unfinished or contradictory; 3: a consistent record of a failure.
    if meta["reason"] != "ended":
        return 1 if meta["reason"] == "no_end" else 2
    if report.verdict in ("incomplete", "inconsistent"):
        return 1
    return 3 if report.verdict == "failed-evidence" else 0


if __name__ == "__main__":
    raise SystemExit(main())
