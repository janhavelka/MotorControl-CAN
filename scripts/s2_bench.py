#!/usr/bin/env python3
"""One bounded S2 bench-console request; preserves raw bytes and never resets/retries.

Single-byte commands send their byte once and capture for --seconds. Trial commands run one named
firmware route (examples/s2_bench/BenchRoute.h): they check the recorded USB identity, require every
route parameter explicitly, require the operator to type the attended confirmation and, for an
enable, the attendant's own shaft observation, then ping, refuse unless the image speaks this
runner's route protocol and nothing else is running, send one T line and capture until the record's
END. Each T line ends in ",K" and the CRC-32 (zlib.crc32) of every byte before it, which the firmware
checks before parsing. A plan that could exceed the firmware's record capacity or route cutoff, a
move deadline the library refuses (4550 ms or less), or a velocity duration or stop reserve the library
refuses (under 9470 ms, or 2800 ms or less after it), is refused here, before anything is sent, because
the firmware would refuse it only after the campaign counted it. Ctrl-C sends one local abort ('A'), which sends no stop. Nothing is retried, and
--dry-run prints the plan without enumerating, prompting, opening or creating anything. The firmware
refuses that abort while a velocity runs: its own stop at the due time is its only software bound.

A velocity line runs as the library's VelocityGate::ATTENDED_EXPERIMENT: the run measures its mode 3
start and stop and claims neither. No record of this fixture measured them, so --start-qualified and
--stop-qualified are refused, and so are a rate over 60 rpm and a duration over 20000 ms, the library's
bounds for that scope. request.json and the send entry record the scope. It always needs the attendant
with the independent cutoff in hand: --unattended-waiver refuses velocity.

After a velocity record whose TRIAL_VELOCITY shows needs=1 (or fenced=1) and in which no stop left
(TRIAL_STOP own=0 or exec=0, and no TRIAL_BEST_EFFORT with own=1 and a nonzero exec), the runner prints
"ALERT: node N MAY BE RUNNING: no stop left; use the cutoff" to stderr at once, before the attendant's
questions, and again at the end, keeps it in request.json as velocity_alert and exits 4 (backlog S16:
trial 1009 left node 1 turning with nothing sent).

A trial's exit status: 0 a complete record; 1 a damaged, unfinished or contradictory capture; 2 refused,
nothing admitted; 3 a consistent record of a failure; 4 the velocity alert above, whatever else holds.

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
attendant's power-up shaft observation, and so is --fault-led prompt, unless that observation was
made and is relayed: --relayed-power-up-shaft FREE|HOLDING with --relayed-shaft-source TEXT, the
user's own report of the shaft as they felt it after the power-up and before anything was sent,
with where and when they gave it. It is stored as relayed, never inferred from the drive.

Drives kept powered (2026-11-10): mode1 decides its start from its first read. Mode 0 is the power-up
baseline, checked as before. Mode 3 is the released standstill a recorded stop left: a second full read, both
released with 6041 bits 0..6 0x50, then one 6060=1 and cycles until 6061 reads 1; never 6040. An enable then
takes --released-by-recorded-stop RECORD instead of --no-motion-write-recorded (exactly one of them): RECORD is
the evidence directory of this node's record whose QUICK_STOP this application saw settle (header failure=0,
TRIAL_STOP kind=2 own=1 state=2), which the runner checks, also in --dry-run. The flag also states that nothing but
one acknowledged 6060=1 reached the drive after that record, which no runner check can show across boots: the
operator types it only from that record and every ledger since. Its line carries start 2 and shaft FREE, read from
the released start report at the user's instruction of 2026-11-10 ("do not ask if holding or not, read it") and
never felt, so no shaft question is asked. It is refused with --unattended-waiver and with a relayed shaft: word 7
moves the rotor to the drive's internal command with no bound known.

velocity-target (ledger gate Q03) writes one 60FF value, --value -3000..3001, to a drive that reports
itself released at standstill in mode 3 on two fresh reads, and only after an acknowledged write reads 60FF
back raw and the state once more. It never writes 6040, 6060 or another object and never enables. It is
attended and counted like the settings routes, three per node, with no motion weight. 3001 is one above
EN86/ZH86 p13's 0..3000 rpm: it shows whether the drive range-checks the value before any negative one is
sent. The runner refuses a value outside that range before anything is sent.

close-sessions sends one 'x': the firmware closes its idle route sessions, so that K, H and the raw and PDO
experiments can reserve those nodes again in the same MCU boot (backlog S15). It sends nothing on CAN and
is no stop, release or disable. It is refused, with nothing changed, while any record is held (release it
with 'r' first) or a session has work in flight. The firmware's reply decides the exit status: 0 for
"SESSIONS closed", 2 for "SESSIONS refused", 1 for no such line (an image without the command answers
"UNKNOWN command"). Closing drops each session's latest controlword: a later route that needs one refuses
as after an MCU restart, while fences, trial identities and the per-boot caps stay in the firmware.
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
            "trial-abort": b"A", "close-sessions": b"x"}
# Single-byte commands whose reply line decides the exit status: (prefix, status) in order; no such line is 1.
REPLIES = {"close-sessions": (("SESSIONS closed ", 0), ("SESSIONS refused ", 2))}
# A trial's exit status after a velocity record that may have left its node turning (velocity_alert).
VELOCITY_ALERT_STATUS = 4
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
    "move": ("G", True, "packet 20 / packet 22 step 4"),
    "readiness-history": ("H", False, "packet 4 / packet 22 step 2"),
    "released-quick": ("Q", True, "packet 16 step 3"),
    "word67": ("W", True, "packet 15 words 6 and 7"),
    "relative": ("L", True, "packet 21 relative base"),
    # Settings routes change drive state without motion: attended and counted, with caps of their own.
    "settings-2202": ("D", False, "packet 12 T1/T2"),
    "io-di3": ("J", False, "packet 13 P2/P3"),
    # The session's two clears: counter zero moves the position reference, so both are motion class.
    "counter-zero": ("Z", True, "counter zero, 2302 0 1 0"),
    "clear-alarm": ("C", True, "alarm clear, 2301 1 0"),
    # Prompt 35: one 6060=3 on a drive that reports itself released, then the library's finite velocity
    # run from released mode 3, with its own stop at the due time.
    "mode3": ("N", True, "prompt 35 released mode 3"),
    "velocity": ("V", True, "prompt 35 finite velocity"),
    # Prompt 36: the reads requestHome consumes and its refusal before any write. Nothing is written, so it
    # has the settings routes' attended class: three per route and node, no motion weight.
    "home": ("O", False, "prompt 36 homing admission"),
    # Ledger gate Q03: one 60FF write and its readback on a drive that reports itself released in mode 3.
    # The drive stays released, so it has the settings routes' attended class too.
    "velocity-target": ("F", False, "gate Q03 60FF encoding probe"),
}
# Read routes whose read effect on the drive is unknown, so an attendant may watch the fault LED before
# and after: --fault-led prompt asks, unobserved records that nobody looked.
FAULT_LED_ROUTES = ("readiness-actions", "readiness-history", "settings-2202", "io-di3")
# Attended routes that are not motion: one send counts one case, never motion weight. The two settings
# writes, and home, which writes nothing. Their send entries carry "settings": 1, the ledger's name for
# this class since the settings routes, kept so that earlier ledgers still read.
SETTINGS_ROUTES = ("settings-2202", "io-di3", "home", "velocity-target")
SETTINGS_LETTERS = frozenset(TRIALS[command][0] for command in SETTINGS_ROUTES)
# PONG routes= of the line protocol this runner speaks: 3 added the CRC-32 field, 4 the rearm's
# predecessor, the move's relative base and the Q, W and L routes, 5 the D and J settings routes, the
# Z and C clears and the move's observer node, 6 the N, V and O routes of prompts 35 and 36, 7 the V line
# without the host's start and stop flags, which the firmware runs as the library's attended experiment, 8
# the F route (gate Q03's 60FF probe). Any other image would misread the line, so it is refused before
# anything is sent.
ROUTE_PROTOCOL = 8
MOTION_ROUTES = frozenset(letter for letter, motion, _ in TRIALS.values() if motion)
# Routes whose line ends in a planned-stop tail of five fields, the first its kind.
STOP_ROUTES = ("position", "relative", "move")
STOP_LETTERS = frozenset(TRIALS[command][0] for command in STOP_ROUTES)
# A finite velocity always asks for its own stop at its due time: one more against stop, every line.
ALWAYS_STOP_ROUTES = ("velocity",)
ALWAYS_STOP_LETTERS = frozenset(TRIALS[command][0] for command in ALWAYS_STOP_ROUTES)
# StatusLayout numbers the library admits for a velocity's alarm and running reports (State.h); 3 is ZH57.
LAYOUTS = {"en86": 1, "zh86": 2, "op86": 4}
# Lichuan homing methods with an 86C sequence (EN86/ZH86 p13-15); numbers are profile-local.
HOME_METHODS = (17, 18, 24, 29)
# velocity-target's value (BenchRoute.cpp PROBE_VALUE): EN86/ZH86 p13's 0..3000 rpm, its negation, and 3001,
# one above it, the range probe. No rate, unit or direction is claimed for any value.
VELOCITY_TARGET_RANGE = (-3000, 3001)
# The first entry's predecessor digest.
LEDGER_GENESIS = "0" * 64
STOP_KINDS = {"halt": 1, "quick": 2}
SIGNAL_SOURCES = {"statusword": 1, "monitor": 2}
COMPLETION_RULES = {"after-absence": 1, "after-acceptance": 2}
# The acknowledged latest word a rearm follows, of a finished record on that node.
PREDECESSORS = {"position-trigger": 1, "position-halt": 2, "relative-trigger": 3}
# A move's target: absolute, or a displacement with MoveEvidence::relativeBase ACTUAL or COMMANDED.
MOVE_BASES = {"absolute": 0, "actual": 1, "commanded": 2}
# 2202 codes as EN86/ZH86 p18 labels them; the EDS default 10 is not one.
SOFT_LIMIT_CODES = {"limits-off": 0, "after-homing": 1}
# The one DI3 change of io-di3: its function 0 (UNDEFINED) to 7 (USER0) and back, or its polarity bit.
DI3_CHANGES = {"function-user0": 1, "function-undefined": 2, "polarity-set": 3, "polarity-clear": 4}
# The firmware's record capacity, route cutoff margin and per-read budgets (BenchRoute.h).
ROUTE_TERMINALS = 128
ROUTE_DURATION_MS = 55000
ROUTE_OPERATION_MS = 5000
# The library's move admission (Position.h MOVE_SERVICE_STEPS) needs a deadline beyond 13 services at the
# routes' queue and response budgets (BenchRoute.h ROUTE_QUEUE_US, ROUTE_RESPONSE_US): over 4550 ms.
MOVE_SERVICE_STEPS = 13
ROUTE_QUEUE_MS = 100
ROUTE_RESPONSE_MS = 250
# The library's velocity admission (Velocity.cpp) from released mode 3: the duration must exceed 27 setup
# services at the routes' budgets and the route's enable report window, and leave two 10 ms enable intervals
# (ENABLE_WORD_INTERVAL_US), so at least 11470 ms; the deadline must leave more than the route's stop reserve,
# 8 services, 2800 ms, after it.
VELOCITY_SETUP_STEPS = 27
VELOCITY_WORD_MS = 10
VELOCITY_STOP_STEPS = 8
# Every velocity line's final-enable report window (backlog S17; BenchRoute.h ROUTE_VELOCITY_WINDOW_MS and
# ROUTE_VELOCITY_SPACING_MS): the attended experiment's chosen budget, never a drive timing. After word 15
# the run takes up to this many seven-field cycles more while word 7's report still shows.
VELOCITY_WINDOW_MS = 2000
VELOCITY_SPACING_MS = 300
VELOCITY_WINDOW_CYCLES = VELOCITY_WINDOW_MS // VELOCITY_SPACING_MS
# The scope every velocity line runs under (Velocity.h VelocityGate::ATTENDED_EXPERIMENT) and its bounds,
# VELOCITY_EXPERIMENT_MAXIMUM_RPM and VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US: project policy for an
# unmeasured start and stop, never a drive fact.
VELOCITY_SCOPE = "ATTENDED_EXPERIMENT"
VELOCITY_EXPERIMENT_RPM = 60
VELOCITY_EXPERIMENT_DURATION_MS = 20000
# Packet 22's host campaign caps, across boots; the firmware enforces the same numbers per boot only.
CAMPAIGN_MOTION_TOTAL = 12
CAMPAIGN_PER_CASE = 3
SHAFT = {"FREE": 1, "HOLDING": 2}
# The enable line's start field (BenchRoute.cpp START, EnableStart in Actions.h).
ENABLE_STARTS = {"NO_MOTION_WRITE_RECORDED": 1, "RELEASED_BY_RECORDED_STOP": 2}
# A released start's shaft basis, stored instead of a typed answer: the drive's own released report.
RELEASED_SHAFT_BASIS = ("read from the drive's released start report (2100 bit 4 set: the power stage is off) at the "
                        "user's instruction of 2026-11-10, 'do not ask if holding or not, read it'; never felt")
# Records whose own QUICK_STOP may have left the drive released (TRIAL_STOP kind=2): the firmware route names.
STOPPING_ROUTES = ("stop", "move", "position", "relative", "velocity")
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
    if args.command in ("mode1", "mode3"):
        fields += [1, _within(parser, "cycles", args.cycles, 1, 10), _within(parser, "interval-ms", args.interval_ms, 100, 5000)]
    elif args.command == "velocity":
        if args.kind is None:
            parser.error("--kind is required for this route: the stop the run asks for at its due time (halt or quick)")
        if args.layout is None:
            parser.error("--layout is required for this route (%s): the record's statusword dialect" %
                         ", ".join(sorted(LAYOUTS)))
        duration = _within(parser, "duration-ms", args.duration_ms, 1, 55000)
        deadline = _within(parser, "deadline-ms", args.deadline_ms, 1, 55000)
        if duration >= deadline:
            parser.error("--duration-ms must be below --deadline-ms: the run's own stop settles after it")
        fields += [1, _within(parser, "rpm", args.rpm, 1, 3000),
                   _within(parser, "ramp-up-ms", args.ramp_up_ms, 1, 2000),
                   _within(parser, "ramp-down-ms", args.ramp_down_ms, 1, 2000), duration,
                   _within(parser, "interval-ms", args.interval_ms, 20, 5000), deadline, STOP_KINDS[args.kind],
                   _within(parser, "record", args.record, 1, 0xFFFFFFFF), LAYOUTS[args.layout]]
        for name, high in (("device-type", 0xFFFFFFFF), ("hardware-version", 0xFFFF), ("software-version", 0xFFFF),
                           ("vendor-id", 0xFFFFFFFF), ("product-code", 0xFFFFFFFF), ("revision", 0xFFFFFFFF),
                           ("model-code", 0xFFFF), ("firmware-code", 0xFFFF)):
            fields.append(_within(parser, name, getattr(args, name.replace("-", "_")), 0, high))
    elif args.command == "home":
        if args.method is None:
            parser.error("--method is required for this route (%s)" % ", ".join(str(m) for m in HOME_METHODS))
        fields += [1, args.method, _within(parser, "search-rpm", args.search_rpm, 1, 3000),
                   _within(parser, "creep-rpm", args.creep_rpm, 1, 3000),
                   _within(parser, "ramp-ms", args.ramp_ms, 1, 2000),
                   _within(parser, "offset", args.offset, -1000000, 1000000)]
    elif args.command == "enable":
        released = args.released_by_recorded_stop is not None
        if released == bool(args.no_motion_write_recorded):
            parser.error("enable needs exactly one start record: --no-motion-write-recorded (the trial log shows no "
                         "6040, target, ramp or mode write since power-up other than one acknowledged 6060=1) or "
                         "--released-by-recorded-stop RECORD (the drive was left released by that record's settled "
                         "quick stop, and nothing but one acknowledged 6060=1 was written since)")
        # Start 1: the shaft is a placeholder that the attendant's typed observation replaces. Start 2: FREE, the
        # released start report's own basis, never asked.
        start = ENABLE_STARTS["RELEASED_BY_RECORDED_STOP" if released else "NO_MOTION_WRITE_RECORDED"]
        fields += [1, start, SHAFT["FREE"] if released else 0,
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
    elif args.command in ("position", "relative"):
        if args.stop_kind is None:
            parser.error("--stop-kind is required for this route (none, halt or quick)")
        cycles = _within(parser, "cycles", args.cycles, 1, 16)
        target = _within(parser, "target", args.target, -1000000, 1000000)
        if args.command == "relative" and not target:
            parser.error("--target is the relative displacement: nonzero")
        fields += [1, _within(parser, "start-position", args.start_position, -2**31, 2**31 - 1), target,
                   _within(parser, "rpm", args.rpm, 0, 3000),
                   _within(parser, "ramp-up-ms", args.ramp_up_ms, 0, 2000),
                   _within(parser, "ramp-down-ms", args.ramp_down_ms, 0, 2000), cycles,
                   _within(parser, "interval-ms", args.interval_ms, 20, 5000)]
        fields += _stop_fields(args, parser, cycles)
    elif args.command == "rearm":
        if args.predecessor is None:
            parser.error("--predecessor is required for this route (%s)" % ", ".join(PREDECESSORS))
        fields += [1, PREDECESSORS[args.predecessor], _within(parser, "cycles", args.cycles, 1, 16),
                   _within(parser, "interval-ms", args.interval_ms, 20, 5000)]
    elif args.command == "settings-2202":
        if args.soft_limit_code is None:
            parser.error("--soft-limit-code is required for this route (%s)" % ", ".join(SOFT_LIMIT_CODES))
        fields += [1, SOFT_LIMIT_CODES[args.soft_limit_code]]
    elif args.command == "io-di3":
        if args.di3_change is None:
            parser.error("--di3-change is required for this route (%s)" % ", ".join(DI3_CHANGES))
        fields += [1, DI3_CHANGES[args.di3_change]]
    elif args.command == "velocity-target":
        fields += [1, _within(parser, "value", args.value, *VELOCITY_TARGET_RANGE)]
    elif args.command in ("counter-zero", "clear-alarm"):
        fields += [1, _within(parser, "deadline-ms", args.deadline_ms, 1, 55000),
                   _within(parser, "interval-ms", args.interval_ms, 10, 10000)]
    elif args.command == "released-quick":
        fields += [1, _within(parser, "cycles", args.cycles, 1, 16),
                   _within(parser, "interval-ms", args.interval_ms, 20, 5000)]
    elif args.command == "word67":
        fields += [1, _within(parser, "word6-cycles", args.word6_cycles, 1, 4),
                   _within(parser, "word7-cycles", args.word7_cycles, 1, 16),
                   _within(parser, "interval-ms", args.interval_ms, 20, 5000)]
    elif args.command == "move":
        if args.stop_kind is None:
            parser.error("--stop-kind is required for this route (none, halt or quick)")
        if args.rule is None:
            parser.error("--rule is required for this route (after-absence or after-acceptance)")
        start = _within(parser, "start-position", args.start_position, -2**31, 2**31 - 1)
        target = _within(parser, "target", args.target, -1000000, 1000000)
        base = MOVE_BASES[args.move_base or "absolute"]
        if base == 0 and args.base_position is not None:
            parser.error("--base-position belongs to --move-base commanded")
        if base == 1 and args.base_position not in (None, start):
            parser.error("--move-base actual takes its base from --start-position, the 6064 both reads show")
        if base == 2 and args.base_position is None:
            parser.error("--move-base commanded needs --base-position, the reviewed commanded position")
        if base and not target:
            parser.error("--target is the relative displacement: nonzero")
        witness = 0 if base == 0 else start if base == 1 else _within(
            parser, "base-position", args.base_position, -2**31, 2**31 - 1)
        fields += [1, start, target,
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
        observer = 0 if args.observer is None else _within(parser, "observer", args.observer, 1, 2)
        if observer == node:
            parser.error("--observer is the other node, never the moving one")
        fields += [base, witness, observer]
        fields += _stop_fields(args, parser, 16)
    if args.command != "move" and (args.move_base is not None or args.base_position is not None or
                                   args.observer is not None):
        parser.error("--move-base, --base-position and --observer belong to move")
    if args.command != "rearm" and args.predecessor is not None:
        parser.error("--predecessor belongs to rearm")
    if args.command != "settings-2202" and args.soft_limit_code is not None:
        parser.error("--soft-limit-code belongs to settings-2202")
    if args.command != "io-di3" and args.di3_change is not None:
        parser.error("--di3-change belongs to io-di3")
    if args.command != "velocity" and any(getattr(args, name) is not None for name in ("duration_ms", "layout")):
        parser.error("--duration-ms and --layout belong to velocity")
    if args.command != "home" and any(getattr(args, name) is not None for name in
                                      ("method", "search_rpm", "creep_rpm", "ramp_ms", "offset")):
        parser.error("--method, --search-rpm, --creep-rpm, --ramp-ms and --offset belong to home")
    if args.command != "velocity-target" and args.value is not None:
        parser.error("--value belongs to velocity-target")
    if args.command != "enable" and args.released_by_recorded_stop is not None:
        parser.error("--released-by-recorded-stop belongs to enable")
    if args.command not in FAULT_LED_ROUTES and args.fault_led is not None:
        parser.error("--fault-led belongs to %s" % " and ".join(FAULT_LED_ROUTES))
    if args.command in FAULT_LED_ROUTES and args.fault_led is None:
        parser.error("--fault-led is required: prompt, or unobserved when nobody can see the drive")
    return fields


def sign_line(body):
    """The T line with its integrity field: CRC-32 (zlib, IEEE) of every byte before ",K"."""
    return "%s,K%08x" % (body, zlib.crc32(body.encode("ascii")))


def encode(command, fields):
    return (sign_line("T%s%s" % (TRIALS[command][0], ",".join(str(v) for v in fields))) + "\n").encode("ascii")


def motion_weight(command, fields):
    """Motion admissions this line counts, as the firmware counts them: one, plus a planned stop, or the
    stop a velocity always asks for itself."""
    if not TRIALS[command][1]:
        return 0, 0
    stop = 1 if command in ALWAYS_STOP_ROUTES or (command in STOP_ROUTES and fields[-5]) else 0
    return 1 + stop, stop


def _stop_terminals(deadline, interval):
    """A session stop: its word, a seven-field cycle at once and one per interval to its deadline (a
    QUICK_STOP observing standstill reads 6064 and 606C too), and a held best-effort pair."""
    return 1 + 7 * (1 + deadline // interval) + 2


def plan_bounds(command, fields):
    """(terminals, milliseconds) at most, as BenchRoute's terminalBound and durationBound count them, so
    that a plan the firmware would refuse as capacity or duration is refused here before it is sent and
    counted. fields is the line after T and its letter, without the integrity field."""
    if command in ("readiness-actions", "readiness-io", "readiness-tuning", "readiness-history"):
        return {"readiness-actions": 22, "readiness-io": 37, "readiness-tuning": 45, "readiness-history": 16}[command], 0
    if command == "settings-2202":
        return 4 + 5 + 1 + 4, 0  # SOFT_LIMITS, STATUS and MONITORS, the write, the readback.
    if command == "io-di3":
        return 9 + 5 + 1 + 9 + 4, 0  # The input group, the state, the write, the readback, 2101 and 60FD.
    if command == "mode1":
        cycles, interval = fields[3:5]
        # The readiness reads less their image, a second full read when the first reports mode 3, 6060, the cycles.
        return 15 + 9 + 1 + 9 + 7 * (cycles - 1), cycles * interval
    if command == "mode3":
        cycles, interval = fields[3:5]
        return 9 + 9 + 1 + 9 + 7 * (cycles - 1), cycles * interval  # Two full reads, 6060, the cycles.
    if command == "velocity":
        duration, interval, deadline = fields[6:9]
        polls = -(-duration // interval)  # The library's maxPolls: the fewest cycles covering the duration.
        # Identity, full read, 60FF 6083 6084, 6 7 15 with a cycle each, the enable report window's cycles,
        # the running cycles, then the session's stop over the rest of the deadline at the same interval.
        return (9 + 9 + 3 + 3 + 7 * 3 + 7 * VELOCITY_WINDOW_CYCLES + 7 * polls +
                _stop_terminals(deadline - duration, interval), deadline)
    if command == "home":
        return 9 + 9 + 9 + 2, 0  # Identity, 2400:00..08, the full read, 2200 and 2201. No write.
    if command == "velocity-target":
        return 9 + 9 + 1 + 1 + 9, 0  # Two full reads, the 60FF write, its upload, one more full read.
    if command == "enable":
        deadline, interval = fields[8:10]
        return 9 + 2 + 3 + 7 * (3 + deadline // interval), deadline
    if command == "stop":
        deadline, best, interval = fields[4:7]
        return 5 + _stop_terminals(deadline, interval), max(deadline, best)
    stop_kind, after, stop_deadline, stop_best, stop_interval = fields[-5:] if command in STOP_ROUTES else (0,) * 5
    stop_terminals = _stop_terminals(stop_deadline, stop_interval) if stop_kind else 0
    stop_span = max(stop_deadline, stop_best) if stop_kind else 0
    if command in ("position", "relative"):
        cycles, interval = fields[8:10]
        # The writes' one operation deadline, then each cycle before the stop at most an interval or one
        # read's operation deadline, then the stop's whole span.
        planned = ROUTE_OPERATION_MS + after * max(interval, ROUTE_OPERATION_MS) + stop_span if stop_kind else 0
        return 18 + 7 * cycles + stop_terminals, max(cycles * interval + stop_span, planned)
    if command == "rearm":
        cycles, interval = fields[4:6]
        return 9 + 1 + 7 * cycles, cycles * interval
    if command == "released-quick":
        cycles, interval = fields[3:5]
        return 9 + 9 + 1 + 7 * cycles, cycles * interval
    if command == "word67":
        six, seven, interval = fields[3:6]
        return 9 + 9 + 1 + 7 * six + 1 + 7 * seven, (six + seven) * interval
    if command == "move":
        deadline, interval, relative, observer = fields[8], fields[9], fields[26], fields[28]
        # An observer adds its check read and an image cycle per interval while the move or its stop runs.
        watched = 9 + 7 * (2 + (deadline + stop_span) // interval) if observer else 0
        return (9 + 9 + (9 if relative else 0) + 4 + 1 + 7 * (2 + deadline // interval) + 1 + stop_terminals +
                watched, deadline + stop_span)
    if command == "counter-zero":
        deadline, interval = fields[3:5]
        return 7 + 3 + 7 * (2 + deadline // interval), deadline  # The state, 0 1 0, seven-field cycles.
    if command == "clear-alarm":
        deadline, interval = fields[3:5]
        # ERRORS, 1003 with at most four entries, MONITORS and STATUS; 1 0; five-field cycles; ERRORS once.
        return 2 + 5 + 5 + 2 + 5 * (2 + deadline // interval) + 2, deadline
    raise ValueError("no bound for %s" % command)


def check_bounds(command, fields):
    """Refuse a plan the firmware would refuse as capacity or duration, or a move deadline the library would
    refuse before its first write; the firmware would still count each of them, planned stop included."""
    terminals, duration = plan_bounds(command, fields)
    if terminals > ROUTE_TERMINALS:
        raise Refused("capacity: this plan can produce %d terminals, over the record's %d" %
                      (terminals, ROUTE_TERMINALS))
    if duration > ROUTE_DURATION_MS:
        raise Refused("duration: this plan can need %d ms, over %d inside the route cutoff" %
                      (duration, ROUTE_DURATION_MS))
    services = MOVE_SERVICE_STEPS * (ROUTE_QUEUE_MS + ROUTE_RESPONSE_MS)
    if command == "move" and fields[8] <= services:
        raise Refused("move deadline: %d ms does not cover the library's %d services at %d + %d ms; it needs over "
                      "%d ms" % (fields[8], MOVE_SERVICE_STEPS, ROUTE_QUEUE_MS, ROUTE_RESPONSE_MS, services))
    if command == "velocity":
        rpm, duration, deadline = fields[3], fields[6], fields[8]
        if rpm > VELOCITY_EXPERIMENT_RPM:
            raise Refused("velocity rpm: %d is over the %s scope's %d rpm" % (rpm, VELOCITY_SCOPE,
                                                                           VELOCITY_EXPERIMENT_RPM))
        if duration > VELOCITY_EXPERIMENT_DURATION_MS:
            raise Refused("velocity duration: %d ms is over the %s scope's %d ms" %
                          (duration, VELOCITY_SCOPE, VELOCITY_EXPERIMENT_DURATION_MS))
        setup = (VELOCITY_SETUP_STEPS * (ROUTE_QUEUE_MS + ROUTE_RESPONSE_MS) + VELOCITY_WINDOW_MS +
                 2 * VELOCITY_WORD_MS)
        reserve = VELOCITY_STOP_STEPS * (ROUTE_QUEUE_MS + ROUTE_RESPONSE_MS)
        if duration < setup:
            raise Refused("velocity duration: %d ms does not cover the library's %d setup services at %d + %d ms, "
                          "the %d ms enable report window and two %d ms enable intervals; it needs at least %d ms" %
                          (duration, VELOCITY_SETUP_STEPS, ROUTE_QUEUE_MS, ROUTE_RESPONSE_MS, VELOCITY_WINDOW_MS,
                           VELOCITY_WORD_MS, setup))
        if deadline - duration <= reserve:
            raise Refused("velocity deadline: %d ms leaves %d ms after the duration; the stop reserve of %d services "
                          "needs over %d ms" % (deadline, deadline - duration, VELOCITY_STOP_STEPS, reserve))


def _integer(value, low, high):
    """A JSON integer in range; a bool or a float is not one."""
    return type(value) is int and low <= value <= high


def check_stop_record(directory, node):
    """--released-by-recorded-stop: the evidence directory must hold this node's record, of a route that can
    stop, whose header reports failure 0 and whose TRIAL_STOP shows its own QUICK_STOP succeeded (kind=2 own=1
    state=2): a stop whose settlement this application observed. Read only. Returns what request.json and the
    send entry keep. Whether anything but one acknowledged 6060=1 reached the drive since is not shown here."""
    path = Path(directory)
    try:
        meta = json.loads((path / "request.json").read_text(encoding="utf-8"))
        text = (path / "rx.txt").read_text(encoding="utf-8", errors="replace")
    except (OSError, ValueError) as error:
        raise Refused("released-by-recorded-stop: %s is not a readable evidence directory (%s)" % (path, error))
    command, trial = (meta.get("command"), meta.get("trial")) if isinstance(meta, dict) else (None, None)
    if command not in STOPPING_ROUTES or meta.get("node") != node or not _integer(trial, 1, 0xFFFFFFFF):
        raise Refused("released-by-recorded-stop: %s is not a stop, move, position, relative or velocity record of "
                      "node %d" % (path, node))
    header = "TRIAL id=%d node=%d route=%s " % (trial, node, command)
    succeeded, settled, inside = False, False, False
    for line in text.splitlines():
        if line.startswith(header):
            inside, succeeded = True, " failure=0 why=none " in line
        elif inside and line.startswith("END trial "):
            break
        elif inside and line.startswith("TRIAL_STOP "):
            values = pong_values(line)
            settled = (values.get("kind"), values.get("own"), values.get("state")) == ("2", "1", "2")
    if not (succeeded and settled):
        raise Refused("released-by-recorded-stop: %s does not record a settled quick stop of trial %d (header "
                      "failure=0 and TRIAL_STOP kind=2 own=1 state=2)" % (path, trial))
    return {"directory": str(path), "command": command, "trial": trial}


def _send_problem(entry):
    """What makes a send entry inconsistent with itself, or None."""
    route, node, trial, motion, stop = (entry.get(k) for k in ("route", "node", "trial", "motion", "stop"))
    settings = entry.get("settings", 0)  # Entries before the settings routes have none.
    if not _integer(settings, 0, 1) or bool(settings) != (route in SETTINGS_LETTERS):
        return "settings %r does not match route %s" % (settings, route)
    if route not in {letter for letter, _, _ in TRIALS.values()} or not _integer(node, 1, 2):
        return "route or node"
    if not _integer(trial, 1, 0xFFFFFFFF):
        return "trial is not an integer 1..4294967295"
    if not _integer(stop, 0, 1) or (stop and route not in STOP_LETTERS | ALWAYS_STOP_LETTERS) or \
            not _integer(motion, 0, 2):
        return "stop or motion"
    if route in ALWAYS_STOP_LETTERS and not stop:
        return "route %s always counts its own stop" % route
    if motion != ((1 + stop) if route in MOTION_ROUTES else 0):
        return "motion %d does not match route %s with stop %d" % (motion, route, stop)
    line = entry.get("line")
    body, separator, _ = line.rpartition(",K") if isinstance(line, str) else ("", "", "")
    if not separator or sign_line(body) != line:
        return "line has no matching integrity field"
    fields = body[2:].split(",")
    if body[:2] != "T" + route or fields[:2] != [str(node), str(trial)]:
        return "line names another route, node or trial"
    if route in STOP_LETTERS and (fields[-5] != "0") != bool(stop):
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
            if entry["event"] != "send":
                continue
            if entry.get("settings"):
                cases[(entry["route"], entry["node"])] += 1  # Counted, with no motion weight.
            if not entry["motion"]:
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
        total, cases = self.counts()
        if command in SETTINGS_ROUTES and cases[(letter, node)] + 1 > CAMPAIGN_PER_CASE:
            raise Refused("the campaign holds %d %s admissions on node %d, the cap is %d" %
                          (cases[(letter, node)], command, node, CAMPAIGN_PER_CASE))
        if not motion:
            return
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
    attended = motion or args.command in SETTINGS_ROUTES
    if attended and args.unattended_waiver:
        operator["attended"] = {"unattended": True, "waiver": args.unattended_waiver, "accepted": True}
    elif attended:
        wanted = "ATTENDED %d" % trial
        ask_operator(ask, operator, "attended",
                     "Attendant at node %d with the independent drive-power cutoff in hand and a shaft mark "
                     "visible: type %s to confirm: " % (node, wanted),
                     lambda text: text if text == wanted else None)
    if args.command == "enable" and args.released_by_recorded_stop is not None:
        # Never asked: a released start report is its own basis (the user's instruction of 2026-11-10). The line
        # already carries FREE, and the library does not consult it for a released start.
        operator["power_up_shaft"] = {"typed": "FREE", "read_from_drive": True, "basis": RELEASED_SHAFT_BASIS,
                                      "accepted": True}
        fields[4] = SHAFT["FREE"]
    elif args.command == "enable" and args.relayed_power_up_shaft:
        # The user felt it and reported it; the lead relays it with its source (2026-11-09).
        operator["power_up_shaft"] = {"typed": args.relayed_power_up_shaft, "relayed": True,
                                      "source": args.relayed_shaft_source, "accepted": True}
        fields[4] = SHAFT[args.relayed_power_up_shaft]
    elif args.command == "enable":
        # The answer is what a person felt. A start report from the drive is never a substitute: on
        # 2026-11-05 (trials 704, 712) HOLDING was typed from 8437/0001 with nobody at the shaft, and
        # that passed the library guard which exists for exactly this observation.
        try:
            shaft = ask_operator(ask, operator, "power_up_shaft",
                                 "Attendant: turn the node %d shaft by hand now. Type FREE or HOLDING as felt, "
                                 "or UNOBSERVED if nobody felt it (the enable is then refused): " % node,
                                 lambda text: SHAFT.get(text))
        except Refused:
            if operator["power_up_shaft"]["typed"] == "UNOBSERVED":
                raise Refused("power_up_shaft: not observed; an enable needs the attendant's own shaft "
                              "observation, never a value inferred from the drive's start report")
            raise
        fields[4] = shaft
    if args.command == "home":
        # Recorded, never sent and never interpreted: the route opens no homing gate whatever is typed.
        question = ("Attendant: which DI terminals of node %d are wired to a switch, as seen? Type NONE, or the "
                    "terminals and switches: " % node)
        if args.unattended_waiver:
            operator["home_inputs_wired"] = {"question": question, "typed": UNOBSERVED}
        else:
            ask_operator(ask, operator, "home_inputs_wired", question, lambda text: text or None)
    if args.command in FAULT_LED_ROUTES and args.fault_led == "prompt":
        ask_operator(ask, operator, "fault_led_before",
                     "Fault LED of node %d now, as seen (for example off, red, red blinking): " % node,
                     lambda text: text or None)
    elif args.command in FAULT_LED_ROUTES:
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
    if args.command in FAULT_LED_ROUTES and args.fault_led == "prompt":
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


def reply_line(command, data):
    """The first reply line of a checked single-byte command and its exit status; (None, 1) without one."""
    for line in bytes(data).decode("utf8", errors="replace").splitlines():
        for prefix, status in REPLIES.get(command, ()):
            if line.startswith(prefix):
                return line, status
    return None, 1


def pong_values(line):
    return dict(word.split("=", 1) for word in line.split()[1:] if "=" in word)


def velocity_alert(command, fields, data):
    """The attendant's alert after a velocity record, or None (backlog S16; trial 1009 left node 1 turning
    with no stop sent). It fires when TRIAL_VELOCITY reports the run's needs-stop (needs=1) or the
    session's (fenced=1) and no stop of this record left: TRIAL_STOP has own=0 or exec=0, and no
    TRIAL_BEST_EFFORT has own=1 with a nonzero exec. A stop line the record lacks is a stop that did not
    leave. Only lines between this trial's TRIAL header and its END count; data is after the prefix."""
    if command != "velocity":
        return None
    node, trial = fields[0], fields[1]
    header = "TRIAL id=%d node=%d route=velocity " % (trial, node)
    velocity, stop, best = None, None, []
    inside = False
    for line in bytes(data).decode("utf8", errors="replace").splitlines():
        if line.startswith(header):
            inside = True
        elif inside and line.startswith("END trial "):
            break
        elif inside:
            values = pong_values(line)
            if line.startswith("TRIAL_VELOCITY "):
                velocity = values
            elif line.startswith("TRIAL_STOP "):
                stop = values
            elif line.startswith("TRIAL_BEST_EFFORT "):
                best.append(values)
    if velocity is None or "1" not in (velocity.get("needs"), velocity.get("fenced")):
        return None

    def left(values):
        return values is not None and values.get("own") == "1" and values.get("exec", "0") != "0"
    if left(stop) or any(left(values) for values in best):
        return None
    return "node %d MAY BE RUNNING: no stop left; use the cutoff" % node


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
        attended = motion or args.command in SETTINGS_ROUTES
        if attended and args.unattended_waiver:
            print("  unattended under the user's waiver %r: no attendant question; observations stored as %r" %
                  (args.unattended_waiver, UNOBSERVED))
            if args.relayed_power_up_shaft:
                print("  relayed power-up shaft %s, source %r" % (args.relayed_power_up_shaft,
                                                                args.relayed_shaft_source))
        elif attended:
            print("  require the operator to type: ATTENDED %d" % fields[1])
        terminals, duration = plan_bounds(args.command, fields)
        try:
            check_bounds(args.command, fields)
            print("  at most %d terminals of %d and %d ms of %d" % (terminals, ROUTE_TERMINALS, duration,
                                                                 ROUTE_DURATION_MS))
        except Refused as error:
            print("  would refuse before sending: %s" % error)
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
        if args.command == "enable" and args.released_by_recorded_stop is not None:
            try:
                stop = check_stop_record(args.released_by_recorded_stop, fields[0])
                print("  start 2 RELEASED_BY_RECORDED_STOP: %s trial %d of node %d records a settled quick stop" %
                      (stop["command"], stop["trial"], fields[0]))
            except Refused as error:
                print("  would refuse before sending: %s" % error)
            print("  the operator states that nothing but one acknowledged 6060=1 reached the drive since that "
                  "record; no check here can show it")
            print("  library: mode 1, 2100 released, 6041 low bits 000, 606C 0; 6041 bits 5 and 6 may change; 6064 "
                  "compared only in word 6's cycles: word 7 moves the rotor to the drive's internal command, no "
                  "bound known; success claims no position continuity, so read the start of the next move fresh")
            print("  no shaft question: field 5 is FREE, %s" % RELEASED_SHAFT_BASIS)
        elif args.command == "enable":
            print("  require the attendant to type FREE or HOLDING; it replaces the 0 in field 5 below")
        if args.command == "mode1":
            print("  the first full read decides the start: mode 0, the power-up check; mode 3, a second full read, "
                  "both released (2100 bit 4, alarm clear, 6041 bits 0..6 0x50, 606C 0, 1001 and 603F 0, the same "
                  "6064, 2100 running admitted latched); then 2F 60 60 00 01 00 00 00 once, never 6040, and cycles "
                  "until 6061 reads 1, at most %d, each still released at that 6064" % fields[3])
        if args.command == "home":
            print("  ask the attendant which DI terminals are wired (recorded only); the route's HomeEvidence is "
                  "CLOSED, so the library refuses before any write")
        if args.command == "velocity-target":
            written = (fields[3] & 0xFFFFFFFF).to_bytes(4, "little")
            print("  gate Q03, 60FF encoding: refused before any write unless two fresh full reads both report "
                  "released standstill in mode 3 (2100 bit 4, alarm clear, 6041 low bits 000, 606C 0, 1001 and 603F "
                  "0, the same 6064; 2100 running admitted latched)")
            print("  then one download 23 FF 60 00 %s (value %d, four bytes two's complement); only if it is "
                  "acknowledged one 60FF upload, kept raw, and one more full read that must report the same "
                  "standstill; an abort or lost reply ends there; never 6040, 6060 or another object, no enable, "
                  "no retry" % (" ".join("%02X" % byte for byte in written), fields[3]))
        if args.command == "velocity":
            weight, _ = motion_weight(args.command, fields)
            print("  VelocityEvidence gate %s, startQualified 0, stopQualified 0: the run measures its start and "
                  "stop and claims neither; from this line: record %d, layout %d, stop kind %d (motion weight %d "
                  "with its own stop)" % (VELOCITY_SCOPE, fields[10], fields[11], fields[9], weight))
            print("  enable report window %d ms, cycles %d ms apart, at most %d more after word 15 (backlog S17): "
                  "the experiment's chosen budget, not a drive timing; the record's TRIAL_VELOCITY_WINDOW and "
                  "TRIAL_VELOCITY_SAMPLE which=2..4 bracket the enable report" %
                  (VELOCITY_WINDOW_MS, VELOCITY_SPACING_MS, VELOCITY_WINDOW_CYCLES))
        print("  send p; refuse unless PONG has routes=%d soak=0 trial=0 active=1 running=1" % ROUTE_PROTOCOL)
        print("  then send once: %r (%s)" % (encode(args.command, fields), plan))
        print("  capture until END trial or TRIAL_REFUSED, at most %s s; Ctrl-C sends one A" % args.seconds)
    else:
        print("  send once: %r, capture %s s" % (COMMANDS[args.command], args.seconds))
        if args.command in REPLIES:
            print("  exit " + ", ".join("%d on %r" % (status, prefix.strip()) for prefix, status in
                                        REPLIES[args.command]) + ", 1 on neither")
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
                 "stop-deadline-ms", "stop-best-effort-ms", "stop-interval-ms", "record", "word6-cycles",
                 "word7-cycles"):
        trial.add_argument("--" + name, type=int)
    trial.add_argument("--status", type=lambda text: int(text, 0), help="Reviewed 6041, for example 0x8037")
    trial.add_argument("--kind", choices=("halt", "quick"))
    trial.add_argument("--stop-kind", choices=("none", "halt", "quick"))
    trial.add_argument("--predecessor", choices=sorted(PREDECESSORS),
                       help="rearm: the acknowledged latest word it follows, of a finished record on that node")
    trial.add_argument("--soft-limit-code", choices=sorted(SOFT_LIMIT_CODES), help="settings-2202: the 2202 code")
    trial.add_argument("--di3-change", choices=sorted(DI3_CHANGES), help="io-di3: the one DI3 change")
    velocity = parser.add_argument_group("velocity: the attended experiment's evidence, never defaulted; also "
                                         "--rpm (at most %d), ramps, --interval-ms, --deadline-ms, --kind, --record "
                                         "(the trial's own) and the identity fields" % VELOCITY_EXPERIMENT_RPM)
    velocity.add_argument("--duration-ms", type=int, help="FiniteVelocityRequest duration from the request, below "
                                                          "--deadline-ms and at most %d" %
                                                          VELOCITY_EXPERIMENT_DURATION_MS)
    velocity.add_argument("--layout", choices=sorted(LAYOUTS), help="VelocityEvidence statusword dialect")
    # Kept only to refuse them: the line has no such field, and no record measured either fact.
    velocity.add_argument("--start-qualified", help=argparse.SUPPRESS)
    velocity.add_argument("--stop-qualified", help=argparse.SUPPRESS)
    home = parser.add_argument_group("home: the request the library refuses before any write")
    home.add_argument("--method", type=int, choices=HOME_METHODS, help="Lichuan homing method")
    home.add_argument("--search-rpm", type=int, help="6099:01 motor rpm, 1..3000")
    home.add_argument("--creep-rpm", type=int, help="6099:02 motor rpm, 1..3000")
    home.add_argument("--ramp-ms", type=int, help="609A ms, 1..2000")
    home.add_argument("--offset", type=int, help="607C native offset, -1000000..1000000")
    probe = parser.add_argument_group("velocity-target: gate Q03's one 60FF write on a released mode 3 drive")
    probe.add_argument("--value", type=int, help="The signed 60FF value, %d..%d; 3001 is the range probe" %
                       VELOCITY_TARGET_RANGE)
    move = parser.add_argument_group("move: the reviewed measurement record's evidence, never defaulted")
    for name in ("accept", "complete"):
        move.add_argument("--%s-source" % name, choices=sorted(SIGNAL_SOURCES))
        move.add_argument("--%s-mask" % name, type=lambda text: int(text, 0))
        move.add_argument("--%s-value" % name, type=lambda text: int(text, 0))
    move.add_argument("--rule", choices=sorted(COMPLETION_RULES))
    move.add_argument("--move-base", choices=sorted(MOVE_BASES),
                      help="absolute (default), or a relative --target with the drive's measured base")
    move.add_argument("--base-position", type=int,
                      help="commanded only: the reviewed commanded position the displacement is added to")
    move.add_argument("--observer", type=int,
                      help="the other node, read at the move's interval while it runs and never written")
    for name in ("device-type", "hardware-version", "software-version", "vendor-id", "product-code", "revision",
                 "model-code", "firmware-code"):
        move.add_argument("--" + name, type=lambda text: int(text, 0), help="Raw identity value of the record")
    ledger = parser.add_argument_group("campaign ledger, across boots")
    ledger.add_argument("--campaign", type=Path, help="JSON-lines campaign ledger; required for motion routes")
    ledger.add_argument("--new-campaign", action="store_true", help="Start the named ledger; refused if it exists")
    trial.add_argument("--fault-led", choices=("prompt", "unobserved"))
    trial.add_argument("--no-motion-write-recorded", action="store_true",
                       help="The trial log shows no motion write since power-up apart from one acknowledged 6060=1")
    trial.add_argument("--released-by-recorded-stop", metavar="RECORD", type=Path,
                       help="enable, instead of --no-motion-write-recorded: the evidence directory of this node's "
                            "record whose quick stop settled (checked), after which nothing but one acknowledged "
                            "6060=1 reached the drive (stated, typed only from the record and the ledgers since)")
    trial.add_argument("--unattended-waiver", metavar="TEXT",
                       help="The user's own written acceptance of a trial with nobody at the bench, verbatim")
    trial.add_argument("--relayed-power-up-shaft", choices=sorted(SHAFT),
                       help="enable under --unattended-waiver: the shaft as the user felt it after the power-up")
    trial.add_argument("--relayed-shaft-source", metavar="TEXT",
                       help="Where and when the user gave the relayed shaft observation, verbatim")
    args = parser.parse_args(argv)
    is_trial = args.command in TRIALS
    if args.start_qualified is not None or args.stop_qualified is not None:
        parser.error("--start-qualified and --stop-qualified are refused: no record of this fixture measured a mode 3 "
                     "start or stop, and velocity runs as the library's %s, which claims neither" % VELOCITY_SCOPE)
    if args.unattended_waiver is not None:
        if not is_trial:
            parser.error("--unattended-waiver belongs to trial commands")
        if not args.unattended_waiver.strip():
            parser.error("--unattended-waiver needs the user's statement")
        if args.command == "enable" and args.released_by_recorded_stop is not None:
            parser.error("--unattended-waiver refuses a released-by-recorded-stop enable: word 7 moves the rotor to "
                         "the drive's internal command with no bound known, so the attendant with the independent "
                         "cutoff is its only control")
        if args.command == "enable" and not args.relayed_power_up_shaft:
            parser.error("--unattended-waiver refuses enable: its start report needs the attendant's power-up "
                         "shaft observation (relay one the user made with --relayed-power-up-shaft)")
        if args.fault_led == "prompt":
            parser.error("--unattended-waiver refuses --fault-led prompt: nobody can see the drive")
        if args.command == "velocity":
            parser.error("--unattended-waiver refuses velocity: its start and stop are unmeasured, so the attendant "
                         "with the independent drive-power cutoff in hand is its only control beyond its own stop")
    if args.relayed_power_up_shaft or args.relayed_shaft_source is not None:
        if args.released_by_recorded_stop is not None:
            parser.error("--relayed-power-up-shaft is a power-up observation; a released-by-recorded-stop enable "
                         "takes its shaft basis from the released start report")
        if args.command != "enable" or not args.unattended_waiver:
            parser.error("--relayed-power-up-shaft belongs to an enable under --unattended-waiver")
        if not args.relayed_power_up_shaft or not (args.relayed_shaft_source or "").strip():
            parser.error("--relayed-power-up-shaft needs both the observation and its --relayed-shaft-source")
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
    if is_trial and (TRIALS[args.command][1] or args.command in SETTINGS_ROUTES) and args.campaign is None:
        parser.error("--campaign is required for a motion or settings route: name the campaign's ledger; counts "
                     "are kept across boots")
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
        if is_trial:
            check_bounds(args.command, fields)
        usb = check_identity(args.port, args.serial, backend.comports(), is_trial)
        if is_trial and args.campaign is not None:
            # Held from before the ledger is read until its result entry is written.
            lock = CampaignLock(args.campaign).acquire(clock)
            campaign = Campaign(args.campaign, args.new_campaign).load()
            campaign.check(args.command, fields)
        args.start_record = None
        if args.command == "enable" and args.released_by_recorded_stop is not None:
            args.start_record = check_stop_record(args.released_by_recorded_stop, fields[0])
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
        if args.command == "velocity":
            meta["velocity_scope"] = {"gate": VELOCITY_SCOPE, "start_qualified": False, "stop_qualified": False,
                                      "record": fields[10], "maximum_rpm": VELOCITY_EXPERIMENT_RPM,
                                      "maximum_duration_ms": VELOCITY_EXPERIMENT_DURATION_MS}
        if args.command == "enable":
            stop = getattr(args, "start_record", None)
            meta["enable_start"] = "RELEASED_BY_RECORDED_STOP" if stop else "NO_MOTION_WRITE_RECORDED"
            if stop:
                # Checked: the record's settled quick stop. Stated: nothing but one acknowledged 6060=1 since.
                meta["start_record"] = dict(stop, stated="nothing but one acknowledged 6060=1 since")
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
        if args.command in SETTINGS_ROUTES:
            entry["settings"] = 1
        if args.command == "velocity":
            entry["scope"] = VELOCITY_SCOPE
        if args.unattended_waiver:
            entry["unattended_waiver"] = args.unattended_waiver
        if getattr(args, "start_record", None):
            entry["start_record"] = args.start_record["directory"]
        campaign.append(entry)
    data = bytearray()
    try:
        port = backend.open(args.port)
        try:
            if is_trial:
                capture, reason = run_trial(args, fields, port, clock, meta, record if campaign else None)
                data = capture.data
                meta["reason"] = reason
                # Before the attendant's questions after the route: the cutoff may be needed now.
                alert = velocity_alert(args.command, fields, data[meta["prefix_bytes"]:])
                if alert:
                    meta["velocity_alert"] = alert
                    print("ALERT: " + alert, file=sys.stderr)
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
                if args.command in REPLIES:
                    # Only bytes after the request: a stale reply already waiting decides nothing.
                    meta["reply"], meta["reply_status"] = reply_line(args.command, data[meta["prefix_bytes"]:])
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
        return meta.get("reply_status", 0)
    import s2_evidence  # Imported here: s2_evidence imports this module's COMMANDS.
    report = s2_evidence.replay_capture(args.output, manifest=Path(args.output) / "absent.json")
    print("verdict:", report.verdict, "failure:", report.failure_name or report.failure, "reason:", meta["reason"])
    # 4: a velocity record whose node may still turn, whatever else the record shows; 2: nothing admitted;
    # 1: damaged, unfinished or contradictory; 3: a consistent record of a failure.
    if meta.get("velocity_alert"):
        print("ALERT: " + meta["velocity_alert"], file=sys.stderr)
        return VELOCITY_ALERT_STATUS
    if meta["reason"] != "ended":
        return 1 if meta["reason"] == "no_end" else 2
    if report.verdict in ("incomplete", "inconsistent"):
        return 1
    return 3 if report.verdict == "failed-evidence" else 0


if __name__ == "__main__":
    raise SystemExit(main())
