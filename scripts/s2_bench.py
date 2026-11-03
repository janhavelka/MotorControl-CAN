#!/usr/bin/env python3
"""One bounded S2 bench-console request; preserves raw bytes and never resets/retries.

Single-byte commands send their byte once and capture for --seconds. Trial commands run one named
firmware route (examples/s2_bench/BenchRoute.h): they check the recorded USB identity, require every
route parameter explicitly, require the operator to type the attended confirmation and, for an
enable, the attendant's own shaft observation, then ping, refuse unless the image has the routes and
nothing else is running, send one T line and capture until the record's END. Ctrl-C sends one local
abort ('A'), which sends no stop. Nothing is retried, and --dry-run prints the plan without
enumerating, prompting, opening or creating anything.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import signal
import sys

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
}
STOP_KINDS = {"halt": 1, "quick": 2}
SHAFT = {"FREE": 1, "HOLDING": 2}
PING_TIMEOUT_S = 3.0
CAPTURE_LIMIT = 1 << 20


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
        if args.stop_kind == "none":
            if any(v is not None for v in (args.stop_after, args.stop_deadline_ms, args.stop_best_effort_ms,
                                           args.stop_interval_ms)):
                parser.error("--stop-kind none takes no other stop argument")
            fields += [0, 0, 0, 0, 0]
        else:
            deadline = _within(parser, "stop-deadline-ms", args.stop_deadline_ms, 1, 55000)
            best = _within(parser, "stop-best-effort-ms", args.stop_best_effort_ms, 0, 55000)
            if best and best < deadline:
                parser.error("--stop-best-effort-ms must be 0 or at least --stop-deadline-ms")
            fields += [STOP_KINDS[args.stop_kind], _within(parser, "stop-after", args.stop_after, 1, cycles),
                       deadline, best, _within(parser, "stop-interval-ms", args.stop_interval_ms, 10, 10000)]
    if args.command != "readiness-actions" and args.fault_led is not None:
        parser.error("--fault-led belongs to readiness-actions")
    if args.command == "readiness-actions" and args.fault_led is None:
        parser.error("--fault-led is required: prompt, or unobserved when nobody can see the drive")
    return fields


def encode(command, fields):
    return ("T%s%s\n" % (TRIALS[command][0], ",".join(str(v) for v in fields))).encode("ascii")


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
    if motion:
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


def run_trial(args, fields, port, clock, meta):
    """Ping, check the image, send one line, capture to the record's END. Returns the stop reason."""
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
        if values.get("routes", "0") == "0":
            reason = "image_without_routes"  # An older image would read the line as single-byte commands.
        elif values.get("soak") != "0":
            reason = "soak_running"
        elif values.get("trial") != "0":
            reason = "trial_record_held"
        elif values.get("active") != "1" or values.get("running") != "1":
            reason = "can_not_started"
    if reason is None:
        line = encode(args.command, fields)
        meta.update(line=line.decode("ascii").strip(), tx_hex=line.hex())
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
        if motion:
            print("  require the operator to type: ATTENDED %d" % fields[1])
        if args.command == "enable":
            print("  require the attendant to type FREE or HOLDING; it replaces the 0 in field 5 below")
        print("  send p; refuse unless PONG has routes=1 soak=0 trial=0 active=1 running=1")
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
                 "stop-deadline-ms", "stop-best-effort-ms", "stop-interval-ms"):
        trial.add_argument("--" + name, type=int)
    trial.add_argument("--status", type=lambda text: int(text, 0), help="Reviewed 6041, for example 0x8037")
    trial.add_argument("--kind", choices=("halt", "quick"))
    trial.add_argument("--stop-kind", choices=("none", "halt", "quick"))
    trial.add_argument("--fault-led", choices=("prompt", "unobserved"))
    trial.add_argument("--no-motion-write-recorded", action="store_true",
                       help="The trial log shows no motion write since power-up apart from one acknowledged 6060=1")
    args = parser.parse_args(argv)
    is_trial = args.command in TRIALS
    if args.seconds is None:
        args.seconds = 120.0 if is_trial else 4.0
    if not 1 <= args.seconds <= (300 if is_trial else 30):
        parser.error("seconds must be 1..%d" % (300 if is_trial else 30))
    if args.output is None and not args.dry_run:
        parser.error("--output is required")
    fields = trial_line(args, parser) if is_trial else None
    if not is_trial and any(getattr(args, name) is not None for name in ("node", "trial")):
        parser.error("--node and --trial belong to trial commands")
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
    try:
        usb = check_identity(args.port, args.serial, backend.comports(), is_trial)
        if is_trial:
            before_send(args, fields, ask, operator)
    except Refused as error:
        print("REFUSED: %s; nothing sent or created" % error, file=sys.stderr)
        return 2
    args.output.mkdir(parents=True, exist_ok=False)
    meta = {"started_utc": clock.utc(), "port": args.port, "serial": args.serial, "usb": usb, "baud": BAUD,
            "command": args.command, "seconds": args.seconds, "reset": False, "retry": False}
    if is_trial:
        meta.update(node=fields[0], trial=fields[1], plan=TRIALS[args.command][2], fields=list(fields),
                    operator=operator, argv=list(argv) if argv is not None else sys.argv[1:])
        meta.update(repository_state())
    else:
        meta["tx_hex"] = COMMANDS[args.command].hex()
    data = bytearray()
    try:
        port = backend.open(args.port)
        try:
            if is_trial:
                capture, reason = run_trial(args, fields, port, clock, meta)
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
