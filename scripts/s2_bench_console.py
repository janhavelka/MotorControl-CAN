#!/usr/bin/env python3
"""Send shared console lines and bench lines to the S2 bench image, one step at a time; one lead alone runs this.

Each --send is one step, sent only after the previous one was answered:

  @ID COMMAND ...  a console JSONL line (docs/cli_contract.md); waits for its reply record and, when the
                   reply admits an operation, for that operation's terminal record (--no-terminal skips it)
  T...             a bench route line; ",K" and the CRC-32 (zlib) of every byte before it are added as
                   scripts/s2_bench.py adds them; waits for TRIAL admitted or TRIAL_REFUSED, and after an
                   arming route (TY, TB, TX, Ta, Te) for its CONSOLE armed or CONSOLE arm refused notice
  c o p s M r x A n d  one bench byte; waits for its own reply line. `r` is sent only after the route record
                   this session saw admitted has printed its END trial line (release is refused while the
                   record is dumped); the next T line needs that `r` first. A paired arming (Ta, Te) prints
                   one TRIAL_PAIR line instead of its dump: `d` dumps it, and only once the console's results
                   are released; `r` before that `d` is sent at once and refused by the firmware
  wait ID          waits for the terminal record of console request ID, ok or not; with --no-terminal this
                   lets two axes' moves run while later steps (a stop) are sent
  pause MS         reads and records for MS milliseconds, sending nothing
  release-all      releases every operation this session saw accepted, each with `@ID release OP` under
                   a new ID above every ID sent so far; a refused release ends the run

A console motion line must follow its arming within the arming's age cap (about 3 s on the bench), so
give both in one invocation. For two axes, arm both with one Ta or Te line before either moves: the
firmware refuses every T line while the console retains an operation, the first axis's move included. Opening the port drives neither EN nor BOOT, and the port must be the
recorded ESP-Prog2. Nothing is retried or replayed: a refusal, an ok:false record or a timeout ends the
run with the remaining steps unsent. Every received byte goes to --output exactly as received; the
console lines are printed with their receive time. --dry-run prints the framed bytes and opens nothing.

--allow-changes explicitly appends the attended-change experiment flag to TY/TB arming lines.
The fresh arming trial identifies this experiment, never measured qualification. Ordinary arming
closes both change kinds; no command/feedback relation is supplied for a different endpoint.

Exit codes: 0 every step answered and admitted, 2 a step was refused, 1 timeout, port or usage problem.
"""

import argparse
import json
import os
from pathlib import Path
import re
import sys
import time
import zlib

# The recorded fixture, as in s2_bench.py, s2_soak.py and s2_flash.py. A changed fixture needs a reviewed edit.
FIXTURE = {"port": "COM15", "usb_serial": "94A990398B40", "vid": 0x303A, "pid": 0x1002}
BAUD = 115200
SCHEMA = "motorcontrolcan.console/1"
ARMING = frozenset("YBXae")
# Each bench byte's reply lines: (prefix, refused).
BYTE_REPLIES = {
    "c": (("CAN ready ", False), ("CAN start refused", True)),
    "o": (("CONSOLE open ", False), ("CONSOLE refused ", True)),
    "p": (("PONG ", False),),
    "s": (("STATS ", False),),
    "M": (("HEAP ", False),),
    "x": (("SESSIONS closed ", False), ("SESSIONS refused ", True)),
    "A": (("TRIAL_ABORTING", False), ("TRIAL_ABORT refused", True)),
    "n": (("NETWORK ", False),),
    # A route record's dump starts with its header; a paired arming's waits for the console's results.
    "d": (("TRIAL id=", False), ("DUMP unavailable", True), ("DUMP refused ", True)),
    "r": (("TRIAL record released", False), ("TRIAL release refused", True), ("RELEASED record", False),
          ("RELEASE refused", True), ("PDO record released", False), ("PDO release refused", True),
          ("NMT result released", False), ("NMT release refused", True), ("CAP record released", False),
          ("CAP release refused", True), ("DISC record released", False), ("DISC release refused", True),
          ("COMM record released", False), ("COMM release refused", True), ("HB record released", False),
          ("HB release refused", True)),
}
CONSOLE_ID = re.compile(r"@([1-9][0-9]{0,9})(?:[ \t]|$)")
LOCAL_STEP = re.compile(r"(wait|pause) ([1-9][0-9]{0,6})$")


class Failure(RuntimeError):
    """The run ends here; code is its exit status."""

    def __init__(self, message, code):
        super().__init__(message)
        self.code = code


def sign_line(body):
    """The T line with its integrity field: CRC-32 (zlib, IEEE) of every byte before ",K"."""
    return "%s,K%08x" % (body, zlib.crc32(body.encode("ascii")))


def change_arming(step, allow):
    """Make the optional TY/TB experiment flag explicit before CRC framing; never overwrite intent."""
    if not step.startswith(("TY", "TB")):
        return step
    baseline = 26 if step.startswith("TY") else 21
    fields = step.split(",")
    if len(fields) == baseline:
        return step + ",1" if allow else step
    if len(fields) == baseline + 1:
        if fields[-1] not in ("0", "1"):
            raise Failure("arming change flag must be 0 or 1", 1)
        if (fields[-1] == "1") != allow:
            raise Failure("arming change flag conflicts with --allow-changes", 1)
        return step
    if allow:
        raise Failure("--allow-changes requires a complete TY or TB arming line", 1)
    return step


def frame(step):
    """The bytes one step sends, and what answers it: ("console", id), ("route", arming) or ("byte", c)."""
    if not step or any(ord(c) < 32 or ord(c) > 126 for c in step if c != "\t"):
        raise Failure("step %r: printable ASCII only" % step, 1)
    if step.startswith("@"):
        match = CONSOLE_ID.match(step)
        if not match or int(match.group(1)) > 0xFFFFFFFF or len(step) > 255:
            raise Failure("step %r: a console line is @ID with ID 1..4294967295, at most 255 bytes" % step, 1)
        return (step + "\n").encode("ascii"), ("console", int(match.group(1)))
    if step.startswith("T"):
        if len(step) < 3 or ",K" in step:
            raise Failure("step %r: give the route line without its integrity field" % step, 1)
        signed = sign_line(step)
        if len(signed) - 1 > 256:
            raise Failure("step %r: longer than the firmware's route line" % step, 1)
        return (signed + "\n").encode("ascii"), ("route", step[1] in ARMING)
    if len(step) == 1 and step in BYTE_REPLIES:
        return step.encode("ascii"), ("byte", step)
    local = LOCAL_STEP.match(step)
    if local:
        return b"", (local.group(1), int(local.group(2)))
    if step == "release-all":
        return b"", ("release", None)
    raise Failure("step %r: only @ lines, T lines, wait ID, pause MS, release-all and the bytes %s" %
                  (step, " ".join(sorted(BYTE_REPLIES))), 1)


def console_record(line):
    """The console's JSON record, or None for any other line."""
    if not line.startswith("{"):
        return None
    try:
        record = json.loads(line)
    except ValueError:
        return None
    return record if isinstance(record, dict) and record.get("schema") == SCHEMA else None


class Session:
    """Lines from one port; every byte is copied to the transcript as received."""

    def __init__(self, port, clock, transcript, echo):
        self.port, self.clock, self.transcript, self.echo = port, clock, transcript, echo
        self.pending = b""
        self.record_open = False  # A route record admitted in this session has not printed its END yet.
        self.pair_undumped = False  # A paired arming's TRIAL_PAIR line came, and no dump has started.
        self.terminals = {}  # Console request ID -> its terminal record, as seen.
        self.operations = []  # Operation IDs of accepted replies, in order, once each.
        self.highest = 0  # The highest console request ID sent.

    def send(self, data):
        if self.port.write(data) != len(data):
            raise Failure("short write", 1)

    def until(self, predicate, timeout):
        """The first line for which predicate returns a value other than None, within timeout seconds."""
        end = self.clock() + timeout
        while True:
            while b"\n" in self.pending:
                raw, self.pending = self.pending.split(b"\n", 1)
                line = raw.decode("ascii", "replace").rstrip("\r")
                self.echo("%.3f %s" % (self.clock(), line))
                if line.startswith("TRIAL admitted "):
                    self.record_open = True
                elif line.startswith("END trial "):
                    self.record_open = False
                elif line.startswith("TRIAL_PAIR "):
                    self.pair_undumped = True
                elif line.startswith("TRIAL id="):
                    self.pair_undumped = False
                record = console_record(line)
                if record and record.get("type") == "terminal":
                    self.terminals[record.get("id")] = record
                elif record and record.get("type") == "reply" and record.get("result") == "accepted" and \
                        isinstance(record.get("operation_id"), int) and \
                        record["operation_id"] not in self.operations:
                    self.operations.append(record["operation_id"])
                found = predicate(line)
                if found is not None:
                    return found
            if self.clock() >= end:
                raise Failure("no answer within %.1f s" % timeout, 1)
            data = self.port.read(256)
            if data:
                self.transcript.write(data)
                self.pending += data


def run_step(session, step, args):
    data, (kind, key) = frame(step)
    if kind == "wait":
        if key not in session.terminals:
            session.until(lambda line: True if key in session.terminals else None, args.terminal_timeout)
        return
    if kind == "pause":
        end = session.clock() + key / 1000.0
        try:
            session.until(lambda line: None, key / 1000.0)
        except Failure:
            if session.clock() < end:
                raise
        return
    if kind == "release":
        for operation in list(session.operations):
            session.highest += 1
            line = "@%d release %d" % (session.highest, operation)
            session.echo("SEND %s" % line)
            run_step(session, line, args)
        session.operations = []
        return
    if kind == "console":
        session.highest = max(session.highest, key)
    if kind == "byte" and key == "r" and session.record_open and not session.pair_undumped:
        # The record is dumped whole after it ends, and released only then.
        session.until(lambda line: None if session.record_open else True, args.terminal_timeout)
    session.send(data)
    if kind == "byte":
        def reply(line):
            for prefix, refused in BYTE_REPLIES[key]:
                if line.startswith(prefix):
                    return refused
            return None
        if session.until(reply, args.timeout):
            raise Failure("refused: %s" % step, 2)
        return
    if kind == "route":
        def admitted(line):
            if line.startswith("TRIAL admitted "):
                return True
            if line.startswith("TRIAL_REFUSED "):
                return False
            return None
        if not session.until(admitted, args.timeout):
            raise Failure("refused: %s" % step, 2)
        if key:
            def armed(line):
                if line.startswith("CONSOLE armed "):
                    return True
                if line.startswith("CONSOLE arm refused "):
                    return False
                return None
            if not session.until(armed, args.timeout):
                raise Failure("arming refused: %s" % step, 2)
        return

    def reply(line):
        if line.startswith("CONSOLE_REFUSED "):
            return {"ok": False}
        record = console_record(line)
        return record if record and record.get("id") == key and record.get("type") == "reply" else None
    record = session.until(reply, args.timeout)
    if record.get("ok") is False:
        raise Failure("refused: %s" % step, 2)
    if record.get("result") != "accepted" or args.no_terminal:
        return

    def terminal(line):
        found = console_record(line)
        return found if found and found.get("id") == key and found.get("type") == "terminal" else None
    if not session.until(terminal, args.terminal_timeout).get("ok"):
        raise Failure("terminal not ok: %s" % step, 2)


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


def check_identity(device, ports):
    named = [item for item in ports if item.device.lower() == device.lower()]
    if len(named) != 1 or named[0].serial_number != FIXTURE["usb_serial"] or \
            (named[0].vid, named[0].pid) != (FIXTURE["vid"], FIXTURE["pid"]):
        raise Failure("port is not the recorded ESP-Prog2 %s" % FIXTURE["usb_serial"], 1)


def main(argv=None, backend=None, clock=time.monotonic, out=print):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--port", default=FIXTURE["port"])
    parser.add_argument("--send", action="append", required=True, metavar="STEP", help="One step, in order")
    parser.add_argument("--output", type=Path, help="Raw received bytes, appended exactly as received")
    parser.add_argument("--timeout", type=float, default=20.0, help="Seconds for each reply")
    parser.add_argument("--terminal-timeout", type=float, default=90.0, help="Seconds for a terminal record")
    parser.add_argument("--no-terminal", action="store_true", help="Do not wait for terminal records")
    parser.add_argument("--allow-changes", action="store_true",
                        help="Explicit attended-change opt-in on TY/TB arming; no measured qualification")
    parser.add_argument("--dry-run", action="store_true", help="Print the framed bytes; open nothing")
    args = parser.parse_args(argv)
    try:
        if args.allow_changes and not any(step.startswith(("TY", "TB")) for step in args.send):
            raise Failure("--allow-changes requires a TY or TB arming step", 1)
        steps = [change_arming(step, args.allow_changes) for step in args.send]
        framed = [frame(step) for step in steps]
        if args.dry_run:
            for data, _ in framed:
                out(repr(data))
            return 0
        backend = backend or RealBackend()
        check_identity(args.port, backend.comports())
        port = backend.open(args.port)
        transcript = args.output.open("ab") if args.output else open(os.devnull, "wb")
        try:
            session = Session(port, clock, transcript, out)
            for step in steps:
                out("SEND %s" % step)
                run_step(session, step, args)
        finally:
            transcript.close()
            port.close()
        return 0
    except Failure as failure:
        out("STOPPED: %s; later steps were not sent" % failure)
        return failure.code


if __name__ == "__main__":
    sys.exit(main())
