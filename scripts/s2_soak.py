#!/usr/bin/env python3
"""Run and capture one finite communication soak on the S2 bench; one lead alone runs this.

Validates the recorded USB identity by enumeration, opens only that port, starts the firmware's
soak and captures until the stated end. It sets neither EN nor BOOT, never resets the board and
never replays a request. Raw bytes reach rx.bin as received, and the live parse can only add
notes beside them. A board reset or a lost port ends that trial with its evidence kept. A new
trial, with a new identity and directory, follows only within --max-trials.

A run directory holds run_request.json, events.jsonl, transitions.txt, summary.txt and run.json,
and one trial-NN directory per trial with rx.bin, rx.times.csv and request.json. Each trial
directory replays offline through scripts/s2_evidence.py.
"""

import argparse
from collections import Counter
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys

import s2_evidence as evidence


ROOT = Path(__file__).resolve().parents[1]
# The recorded fixture; the same facts as the table in s2_flash.py. A changed fixture needs a
# reviewed edit here, not an argument.
FIXTURE = {"port": "COM15", "usb_serial": "94A990398B40", "vid": 0x303A, "pid": 0x1002}
BAUD = 115200
READS = {"state": 0, "monitors": 1, "identity": 2, "full": 3}
# The read set with its disposition in docs/reference/lichuan_objects.json, for the record.
# read_effect stays not_established: accepted is the owner's risk decision (DEC-1).
OBJECTS = {
    "state": ["6041:00", "6061:00", "6064:00", "606C:00"],
    "monitors": ["2100:00", "2101:00", "2102:00"],
    "identity_once_per_node": ["1000:00", "1009:00", "100A:00", "1018:00", "1018:01", "1018:02", "1018:03",
                               "2000:00", "2001:00"],
    "read_disposition": "accepted_not_qualified",
    "never_read": ["603F:00", "1001:00", "1003"],
}
MAX_DURATION_S = 172800
READ_TIMEOUT_S = 0.05
CHUNK_BYTES = 4096
FSYNC_S = 1.0
PROMPT_TIMEOUT_S = 3.0
BEGIN_TIMEOUT_S = 5.0
STOP_WAIT_S = 30.0
PARSE_NOTES = 50  # Logged in transitions.txt; the rest are only counted.
REPLAY_LIMIT_BYTES = 64 << 20  # Above this the closing replay is left for a later offline run.
TRANSITION_KINDS = {"SOAK_BEGIN", "SOAK_TR", "SOAK_ANOM", "SOAK_OP", "SOAK_EMCY", "SOAK_BOOT",
                    "SOAK_NODE_STOP", "SOAK_TEND", "SOAK_END", "REFUSED", "SOAK_STOPPING", "SOAK_STOP_REFUSED"}
SUMMARY_KINDS = {"SOAK_SUM", "SOAK_GAP", "SOAK_CON", "SOAK_NODE", "SOAK_HIST", "SOAK_OUT", "SOAK_ABORT"}


class SoakError(RuntimeError):
    """The run cannot start or continue; the message says which fact failed."""


class PortLost(RuntimeError):
    """The open port failed; the board may or may not still be running."""


class RealBackend:
    """pyserial, imported only here so that --dry-run and the tests never touch it."""

    def comports(self):
        from serial.tools import list_ports
        return list_ports.comports()

    def open(self, device):
        import serial
        port = serial.Serial(port=None, baudrate=BAUD, timeout=READ_TIMEOUT_S, write_timeout=1)
        # Set before opening, so that opening drives neither EN nor BOOT.
        port.dtr = port.rts = False
        port.port = device
        port.open()
        return port


class RealClock:
    def wall(self):
        return datetime.now().astimezone()

    def utc(self):
        return datetime.now(timezone.utc).isoformat()

    def monotonic(self):
        import time
        return time.monotonic()

    def sleep(self, seconds):
        import time
        time.sleep(seconds)


def parse_until(text, now):
    """Next occurrence of HH:MM after now, in now's own time zone."""
    match = re.fullmatch(r"([01]?\d|2[0-3]):([0-5]\d)", text)
    if not match:
        raise SoakError("--until needs HH:MM, not %r" % text)
    target = now.replace(hour=int(match.group(1)), minute=int(match.group(2)), second=0, microsecond=0)
    return target if target > now else target + timedelta(days=1)


def check_identity(port, serial_number, ports):
    """Exactly the recorded device on exactly the named port, or an error naming the difference."""
    if serial_number != FIXTURE["usb_serial"]:
        raise SoakError("USB serial %s is not the recorded fixture %s" % (serial_number, FIXTURE["usb_serial"]))
    named = [item for item in ports if item.device.lower() == port.lower()]
    if len(named) != 1:
        raise SoakError("%s is not uniquely present" % port)
    item = named[0]
    if item.serial_number != serial_number or (item.vid, item.pid) != (FIXTURE["vid"], FIXTURE["pid"]):
        raise SoakError("%s is %s %s:%s, not the recorded fixture" % (
            port, item.serial_number, item.vid, item.pid))
    return {"port": item.device, "serial": item.serial_number, "vid": item.vid, "pid": item.pid}


def soak_line(trial, duration_s, interval_ms, summary_s, reads):
    """The one multi-byte console request, parsed by parseSoakCommand in the firmware."""
    return ("S%d,%d,%d,%d,%d\n" % (trial, duration_s, interval_ms, summary_s, reads)).encode("ascii")


def repository_state():
    try:
        revision = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True,
                                  timeout=30, check=False).stdout.strip()
        status = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain"], capture_output=True, text=True,
                                timeout=30, check=False).stdout
        return {"host_revision": revision, "host_dirty": bool(status.strip())}
    except OSError:
        return {"host_revision": None, "host_dirty": None}


class Run:
    """One run directory: its logs are appended and flushed as things happen."""

    def __init__(self, args, backend, clock, directory, end_wall):
        self.args = args
        self.backend = backend
        self.clock = clock
        self.directory = directory
        self.end_wall = end_wall
        self.interrupted = False
        directory.mkdir(parents=True, exist_ok=False)
        self.events = (directory / "events.jsonl").open("x", encoding="utf-8", newline="\n")
        self.transitions = (directory / "transitions.txt").open("x", encoding="utf-8", newline="\n")
        self.summary = (directory / "summary.txt").open("x", encoding="utf-8", newline="\n")
        self.trials = []

    def event(self, name, **fields):
        fields.update(event=name, utc=self.clock.utc())
        self.events.write(json.dumps(fields) + "\n")
        self.events.flush()
        self.transitions.write("%s HOST %s %s\n" % (fields["utc"], name, json.dumps(
            {k: v for k, v in fields.items() if k not in ("event", "utc")})))
        self.transitions.flush()
        print("%s %s %s" % (fields["utc"], name, json.dumps({k: v for k, v in fields.items()
                                                              if k not in ("event", "utc")})), flush=True)

    def note(self, stream, trial, text):
        stream.write("%s trial=%d %s\n" % (self.clock.utc(), trial, text))
        stream.flush()

    def remaining_s(self):
        return (self.end_wall - self.clock.wall()).total_seconds()

    def close(self):
        for stream in (self.events, self.transitions, self.summary):
            stream.close()


class Trial:
    """One firmware soak and its directory. Nothing here interprets bytes before storing them."""

    def __init__(self, run, number, identity):
        self.run = run
        self.number = number
        self.identity = identity
        self.directory = run.directory / ("trial-%02d" % number)
        self.directory.mkdir()
        self.rx = (self.directory / "rx.bin").open("xb")
        self.times = (self.directory / "rx.times.csv").open("x", encoding="utf-8", newline="\n")
        self.times.write("offset,bytes,utc,monotonic_s,in_waiting\n")
        self.offset = 0
        self.partial = bytearray()
        self.line_number = 0
        self.lines = []
        self.checker = evidence.SoakChecker(identity)
        self.counts = Counter()
        self.parse_notes = 0
        self.synced = run.clock.monotonic()
        self.boot_after_begin = None
        self.waiting_high = 0
        self.keepalives = 0
        self.last_data = run.clock.monotonic()
        self.port = None
        self.meta = {"command": "soak", "trial": identity, "trial_number": number,
                     "started_utc": run.clock.utc(), "port": run.args.port, "serial": run.args.serial,
                     "baud": BAUD, "reset": False, "retry": False, "status": "running"}
        self.write_meta()

    def write_meta(self):
        # Replaced whole. A crash leaves status running, which replay reports as a failed capture.
        path = self.directory / "request.json"
        temporary = path.with_suffix(".tmp")
        temporary.write_text(json.dumps(self.meta, indent=2) + "\n", encoding="utf-8")
        os.replace(temporary, path)

    def store(self, chunk, waiting):
        """Raw bytes first, on disk, before any parsing can fail."""
        clock = self.run.clock
        self.rx.write(chunk)
        self.rx.flush()
        self.times.write("%d,%d,%s,%.6f,%d\n" % (self.offset, len(chunk), clock.utc(), clock.monotonic(), waiting))
        self.times.flush()
        self.offset += len(chunk)
        self.last_data = clock.monotonic()
        if clock.monotonic() - self.synced >= FSYNC_S:
            os.fsync(self.rx.fileno())
            os.fsync(self.times.fileno())
            self.synced = clock.monotonic()

    def parse(self, chunk):
        self.partial.extend(chunk)
        while True:
            position = self.partial.find(b"\n")
            if position < 0:
                return
            raw = bytes(self.partial[:position])
            del self.partial[:position + 1]
            self.line_number += 1
            text = raw.rstrip(b"\r").decode("utf-8", errors="replace")
            if text:
                self.line(evidence.parse_line(self.line_number, text))

    def line(self, record):
        run = self.run
        self.lines.append(record)
        self.counts[record.kind] += 1
        if record.status != "parsed":
            self.counts["_" + record.status] += 1
            if self.parse_notes < PARSE_NOTES:
                self.parse_notes += 1
                run.note(run.transitions, self.number, "PARSE %s line=%d %s" % (
                    record.status, record.number, record.raw[:160]))
            return
        before = len(self.checker.errors), len(self.checker.warnings)
        self.checker.feed(record)
        for item in self.checker.errors[before[0]:] + self.checker.warnings[before[1]:]:
            run.note(run.transitions, self.number, "CHECK %(code)s line=%(line)s %(detail)s" % item)
        if record.kind in TRANSITION_KINDS:
            run.note(run.transitions, self.number, record.raw)
        elif record.kind in SUMMARY_KINDS:
            run.note(run.summary, self.number, record.raw)
        if record.kind == "SOAK_END" or (record.kind == "SOAK_ABORT" and record.values["node"] == 2):
            run.note(run.summary, self.number, self.digest())
        if self.checker.began is not None and self.checker.ended is None and self.boot_after_begin is None:
            if record.role == "boot":
                self.boot_after_begin = record.raw
            elif record.kind == "PONG" and record.values.get("soak") == 0:
                # The firmware says no soak is running: it restarted and the boot text was lost.
                self.boot_after_begin = record.raw
        # Bounded: a night of records must not grow this list; the file holds them all.
        if len(self.lines) > 256:
            del self.lines[:128]

    def digest(self):
        checker = self.checker
        node = [checker.latest.get(("SOAK_NODE", n), {}) for n in (1, 2)]
        return ("DIGEST ops=%s/%s steps=%s/%s anomalies=%d stopped=%s gaps=%d malformed=%d unknown=%d" % (
            node[0].get("opsOk", 0), node[1].get("opsOk", 0), node[0].get("stepsOk", 0), node[1].get("stepsOk", 0),
            sum(checker.anomalies.values()), sorted(checker.stopped), checker.lost_lines,
            self.counts["_malformed"], self.counts["_unknown"]))

    def read(self):
        """One bounded read. A failure of the port is the caller's event, not a retry."""
        try:
            waiting = self.port.in_waiting
            self.waiting_high = max(self.waiting_high, waiting)
            chunk = self.port.read(min(max(waiting, 1), CHUNK_BYTES))
        except OSError as error:
            raise PortLost(str(error)) from error
        if chunk:
            self.store(chunk, waiting)
            self.parse(chunk)
        return chunk

    def keepalive(self):
        """One bare newline, which the firmware ignores outside a command line. HIL-only
        diagnostic: it keeps host-to-board traffic going while the console is quiet. Counted, not
        logged per write. A failed write ends the trial as any port loss does."""
        try:
            count = self.port.write(b"\n")
        except OSError as error:
            raise PortLost("keepalive write failed: %s" % error) from error
        if count != 1:
            raise PortLost("short keepalive write of %s" % count)
        self.keepalives += 1

    def send(self, data):
        """Exactly one write of one request. A failed or short write is never repeated."""
        self.run.event("send", trial=self.identity, tx_hex=data.hex(), text=data.decode("ascii").strip())
        try:
            count = self.port.write(data)
        except OSError as error:
            raise PortLost("command write failed; not repeated: %s" % error) from error
        if count != len(data):
            raise PortLost("short command write of %s; not repeated" % count)

    def wait(self, kinds, seconds):
        """Read until a record of one of these kinds arrives after this call, or the time ends."""
        clock = self.run.clock
        deadline = clock.monotonic() + seconds
        seen = self.line_number
        while clock.monotonic() < deadline:
            self.read()
            for record in self.lines:
                if record.number > seen and record.kind in kinds and record.status == "parsed":
                    return record
        return None

    def finish(self, status, reason):
        self.checker.finish()
        if self.partial:
            self.run.note(self.run.transitions, self.number, "TRUNCATED %d bytes without a line end" % len(self.partial))
        self.meta.update(status="captured" if status == "captured" else "failed", ended_utc=self.run.clock.utc(),
                         reason=reason, bytes=self.offset, host_waiting_high_water=self.waiting_high,
                         host_keepalives=self.keepalives)
        if status != "captured":
            self.meta["error"] = reason
        self.write_meta()
        for stream in (self.rx, self.times):
            stream.flush()
            os.fsync(stream.fileno())
            stream.close()
        if self.port is not None:
            try:
                self.port.close()
            except OSError as error:
                self.run.event("port_close_failed", trial=self.identity, error=str(error))
        self.run.note(self.run.summary, self.number, self.digest())
        self.run.event("trial_end", trial=self.identity, reason=reason, bytes=self.offset)


def start_soak(trial, args, remaining):
    """Establish the running image's state, then ask once. Returns None or the reason it did not start."""
    run = trial.run
    clock = run.clock
    deadline = clock.monotonic() + 0.2
    while clock.monotonic() < deadline:  # Retain whatever already waits; never flush unseen evidence.
        trial.read()
    trial.meta["prefix_bytes"] = trial.offset
    trial.send(b"p")
    pong = trial.wait({"PONG"}, PROMPT_TIMEOUT_S)
    if pong is None:
        return "no_ping"
    run.event("ping", trial=trial.identity, base=pong.values["base"], elf=pong.values["elf"],
              active=pong.values["active"], running=pong.values["running"], soak=pong.values.get("soak"))
    trial.meta["image"] = {key: pong.values[key] for key in ("base", "elf", "sdk", "target")}
    if pong.values.get("soak"):
        # The port dropped under a soak that kept running. Its record ends here, and it stays a
        # separate trial rather than being continued under a new name.
        run.event("soak_still_running", trial=trial.identity)
        trial.send(b"X")
        if trial.wait({"SOAK_END"}, STOP_WAIT_S) is None:
            return "old_soak_did_not_end"
    if not pong.values["active"]:
        trial.send(b"c")
        started = trial.wait({"CAN_READY", "CAN_REFUSED"}, PROMPT_TIMEOUT_S)
        if started is None or started.kind != "CAN_READY":
            return "can_start_refused"
    elif not pong.values["running"]:
        return "firmware_not_running"
    duration = min(int(remaining), MAX_DURATION_S)
    if duration < 1:
        return "no_time_left"
    request = soak_line(trial.identity, duration, args.interval_ms, args.summary_s, READS[args.reads])
    trial.meta.update(tx_hex=request.hex(), duration_s=duration, interval_ms=args.interval_ms,
                      summary_s=args.summary_s, reads=args.reads)
    trial.write_meta()
    trial.send(request)
    answer = trial.wait({"SOAK_BEGIN", "REFUSED"}, BEGIN_TIMEOUT_S)
    if answer is None:
        return "no_begin"
    if answer.kind == "REFUSED":
        return "refused:" + answer.values.get("reason", "")
    return None


def capture(trial, args):
    """Follow a started soak to its end. Returns (status, reason); evidence is on disk either way."""
    run = trial.run
    clock = run.clock
    stop_sent_at = None
    silence_after = max(10.0, args.summary_s * 3.0)
    pinged_for = None
    next_keepalive = clock.monotonic() + args.keepalive_s if args.keepalive_s else None
    while True:
        trial.read()
        if next_keepalive is not None and clock.monotonic() >= next_keepalive:
            trial.keepalive()
            next_keepalive = clock.monotonic() + args.keepalive_s
        if trial.checker.ended is not None:
            return "captured", "end:" + trial.checker.ended["reason"]
        if trial.boot_after_begin is not None:
            return "failed", "board_reset"
        now = clock.monotonic()
        stopping = run.interrupted or run.remaining_s() < -max(30.0, float(args.summary_s))
        if stopping and stop_sent_at is None:
            run.event("stop_requested", trial=trial.identity, interrupted=run.interrupted)
            trial.send(b"X")
            stop_sent_at = now
        if stop_sent_at is not None and now - stop_sent_at >= STOP_WAIT_S:
            return "failed", "no_end_after_stop"
        quiet = now - trial.last_data
        if quiet >= silence_after and pinged_for != trial.last_data:
            # A summary was due. One local ping tells a stalled task from a quiet link; it sends
            # no CAN frame and does not change the soak. Each quiet spell gets one.
            pinged_for = trial.last_data
            run.event("silence", trial=trial.identity, seconds=round(quiet, 1))
            trial.send(b"p")


def wait_for_port(run, args):
    clock = run.clock
    deadline = clock.monotonic() + args.reenumerate_s
    while True:
        try:
            return check_identity(args.port, args.serial, run.backend.comports())
        except SoakError as error:
            if clock.monotonic() >= deadline:
                run.event("port_not_back", error=str(error))
                return None
        clock.sleep(1.0)


def run_trials(run, args):
    """Bounded trials. Returns the last (status, reason)."""
    clock = run.clock
    base = int(clock.wall().timestamp())  # Later than any earlier run's, as the firmware requires.
    outcome = ("failed", "no_trial")
    for number in range(1, args.max_trials + 1):
        remaining = run.remaining_s()
        if remaining < 1:
            run.event("time_is_up", trials=number - 1)
            break
        identity = wait_for_port(run, args)
        if identity is None:
            return "failed", "port_not_back"
        run.event("trial_start", number=number, trial=base + number - 1, remaining_s=int(remaining),
                  usb=identity)
        trial = Trial(run, number, base + number - 1)
        run.trials.append(trial)
        try:
            trial.port = run.backend.open(args.port)
            refusal = start_soak(trial, args, remaining)
            if refusal:
                outcome = ("failed", refusal)
                run.event("soak_not_started", trial=trial.identity, reason=refusal)
                trial.finish(*outcome)
                return outcome  # A refusal is a state to inspect; asking again would not change it.
            trial.meta["began_utc"] = clock.utc()
            trial.write_meta()
            outcome = capture(trial, args)
        except PortLost as error:
            outcome = ("failed", "port_lost")
            run.event("port_lost", trial=trial.identity, error=str(error), bytes=trial.offset)
        except OSError as error:
            outcome = ("failed", "port_open_failed")
            run.event("port_open_failed", trial=trial.identity, error=str(error))
        if outcome[1] == "board_reset":
            run.event("board_reset", trial=trial.identity, line=trial.boot_after_begin, bytes=trial.offset)
        trial.finish(*outcome)
        if outcome[0] == "captured" or outcome[1] not in ("board_reset", "port_lost", "port_open_failed"):
            return outcome
        if number == args.max_trials:
            run.event("trials_exhausted", max_trials=args.max_trials)
    return outcome


def verdicts(run):
    """Replay every trial offline, the same way a later reader will."""
    rows = []
    for trial in run.trials:
        if trial.offset > REPLAY_LIMIT_BYTES:
            rows.append({"trial": trial.identity, "directory": trial.directory.name, "verdict": "not_replayed",
                         "errors": [], "error_count": 0, "warning_count": 0, "soak": {}, "unknown": 0,
                         "malformed": 0, "bytes": trial.offset, "sha256": ""})
            continue
        report = evidence.replay_capture(trial.directory, manifest=Path(os.devnull))
        rows.append({"trial": trial.identity, "directory": trial.directory.name, "verdict": report.verdict,
                     "errors": report.errors[:20], "error_count": len(report.errors),
                     "warning_count": len(report.warnings), "soak": report.soak,
                     "unknown": len(report.unknown), "malformed": len(report.malformed),
                     "bytes": report.bytes, "sha256": report.sha256})
    return rows


def execute(args, backend, clock):
    now = clock.wall()
    end_wall = parse_until(args.until, now) if args.until else now + timedelta(seconds=args.duration_s)
    remaining = (end_wall - now).total_seconds()
    if remaining > MAX_DURATION_S:
        raise SoakError("the firmware accepts at most %d seconds" % MAX_DURATION_S)
    directory = args.output or ROOT / ("docs/bench/%s-s2-soak" % clock.wall().astimezone(timezone.utc).strftime("%Y%m%dT%H%M%SZ"))
    if args.dry_run:
        dry_run(args, directory, end_wall, remaining)
        return 0
    # Before anything is created: a wrong board leaves no artifact to confuse with evidence.
    check_identity(args.port, args.serial, backend.comports())
    run = Run(args, backend, clock, directory, end_wall)
    request = {"command": "soak-run", "started_utc": clock.utc(), "fixture": FIXTURE, "port": args.port,
               "serial": args.serial, "baud": BAUD, "until": args.until, "duration_s": args.duration_s,
               "end_local": end_wall.isoformat(), "interval_ms": args.interval_ms, "summary_s": args.summary_s,
               "reads": args.reads, "max_trials": args.max_trials, "objects": OBJECTS, "reset": False,
               "retry": False, "argv": sys.argv[1:], "time_basis": evidence.TIME_BASIS}
    request.update(repository_state())
    # Not request.json: that name marks a capture directory, and this is not one.
    (directory / "run_request.json").write_text(json.dumps(request, indent=2) + "\n", encoding="utf-8")
    previous = signal.getsignal(signal.SIGINT)

    def interrupted(_number, _frame):
        run.interrupted = True  # Stops through the firmware's own stop, keeping the record whole.

    try:
        signal.signal(signal.SIGINT, interrupted)
    except ValueError:
        previous = None  # Not the main thread, as under a test runner.
    try:
        status, reason = run_trials(run, args)
    finally:
        if previous is not None:
            signal.signal(signal.SIGINT, previous)
    rows = verdicts(run)
    worst = {row["verdict"] for row in rows}
    (directory / "run.json").write_text(json.dumps(
        {"status": status, "reason": reason, "ended_utc": clock.utc(), "trials": rows}, indent=2) + "\n",
        encoding="utf-8")
    for row in rows:
        run.event("verdict", trial=row["trial"], verdict=row["verdict"], errors=row["error_count"],
                  soak=row["soak"], sha256=row["sha256"])
    run.close()
    print("Evidence:", directory)
    if status != "captured" or worst & {"incomplete", "inconsistent"}:
        return 1
    return 3 if worst & {"failed-evidence", "not_replayed"} else 0


def dry_run(args, directory, end_wall, remaining):
    print("DRY RUN: no port opened, nothing enumerated, nothing created. Verdict: INCOMPLETE.")
    print("  require USB %s %04X:%04X uniquely on %s" % (args.serial, FIXTURE["vid"], FIXTURE["pid"], args.port))
    print("  end %s, %d s from now, at most %d trials" % (end_wall.isoformat(), remaining, args.max_trials))
    print("  send p, then c if the controller is not yet started, then one request:")
    print("    %r (trial,duration_s,interval_ms,summary_s,reads)" % soak_line(0, int(remaining), args.interval_ms,
                                                                               args.summary_s, READS[args.reads]))
    print("  reads %s, each node in turn: %s" % (args.reads, json.dumps(OBJECTS)))
    print("  write %s" % directory)


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", default=FIXTURE["port"])
    parser.add_argument("--serial", default=FIXTURE["usb_serial"], help="Exact ESP-Prog2 USB serial number")
    when = parser.add_mutually_exclusive_group(required=True)
    when.add_argument("--until", help="Wall-clock end, HH:MM local, next occurrence")
    when.add_argument("--duration-s", type=int, help="End this many seconds from now")
    parser.add_argument("--interval-ms", type=int, default=200, help="Read operation spacing across both nodes")
    parser.add_argument("--summary-s", type=int, default=60)
    parser.add_argument("--keepalive-s", type=float, default=0.0,
                        help="HIL diagnostic: write a bare newline this often while capturing; 0 never")
    parser.add_argument("--reads", choices=READS, default="full",
                        help="state; monitors adds 2100..2102; identity adds one identity read per node")
    parser.add_argument("--max-trials", type=int, default=3,
                        help="Trials allowed after a board reset or lost port, the first included")
    parser.add_argument("--reenumerate-s", type=float, default=120.0, help="Wait for the port to come back")
    parser.add_argument("--output", type=Path, help="New run directory; default docs/bench/<UTC>-s2-soak")
    parser.add_argument("--dry-run", action="store_true", help="Print the plan; touches nothing")
    args = parser.parse_args(argv)
    if args.duration_s is not None and not 1 <= args.duration_s <= MAX_DURATION_S:
        parser.error("--duration-s must be 1..%d" % MAX_DURATION_S)
    if not 20 <= args.interval_ms <= 60000:
        parser.error("--interval-ms must be 20..60000")
    if not 1 <= args.summary_s <= 3600:
        parser.error("--summary-s must be 1..3600")
    if not 1 <= args.max_trials <= 100:
        parser.error("--max-trials must be 1..100")
    return args


def main(argv=None, backend=None, clock=None):
    args = arguments(argv)
    try:
        return execute(args, backend or RealBackend(), clock or RealClock())
    except SoakError as error:
        print("REFUSED: %s" % error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
