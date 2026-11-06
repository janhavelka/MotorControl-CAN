#!/usr/bin/env python3
"""Run one planned PDO stream session on the S2 bench, or analyze a retained one; one lead alone runs this.

run: validates the recorded USB identity, opens only that port (neither EN nor BOOT is driven),
pings, refuses unless the image speaks the PDO lines (PONG pdo=1) with no soak or trial record
held, then sends the plan's steps in order and captures every byte to rx.bin. A step sends one
console command (a single byte, or an N, P, R, W, S line of examples/s2_bench/PdoCommand.h and the
soak) and waits for its expected reply; a refusal or a missing reply ends the plan. The plan is the
only source of device commands: nothing is retried, replayed or added, and no NMT, write or stop
is sent on the plan's behalf. If the plan ends early while a watch runs, only "P0" is sent, which
ends the local watch and sends nothing on CAN. Whatever state the drives were left in, Operational
included, is recorded and reported, not repaired.

analyze: replays rx.bin offline through the s2_evidence grammar and reports, per received ID, the
watch's frames and on-device statistics, every NMT request with its TX completion time, the first
and last frame of each ID around each NMT request, heartbeat states, and every PDO record read or
write with its raw values. Times are the S2's monotonic microseconds: a frame's hi is its ISR clock
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
# Replies that end a plan: the firmware refused the command, so nothing after it may follow.
REFUSALS = ("PDO_LINE refused", "PDO_READ refused", "PDO_WRITE refused", "PDOW refused", "PDOW_STOP refused",
            "NMT refused", "NMT release refused", "RELEASE refused", "PDO release refused", "SOAK_REFUSED",
            "TRIAL_REFUSED", "UNKNOWN command", "HEARTBEAT refused", "PDO refused", "IDENTITY refused",
            "CAN start refused", "DUMP unavailable", "OUTPUT_FORMAT_ERROR")
LINE_COMMANDS = ("N", "P", "R", "W", "S")
SINGLE_BYTES = set("cpsrdnhjmqwX")
NMT_NAMES = {1: "start", 2: "stop", 0x80: "pre-operational"}


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
    for number, step in enumerate(plan["steps"], 1):
        if not isinstance(step, dict):
            raise SessionError("step %d is not an object" % number)
        unknown = set(step) - {"send", "expect", "end", "timeout_s", "end_timeout_s", "wait_s", "note"}
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
                raise SessionError("step %d: %r is not an N, P, R, W or S line" % (number, text))
            if not isinstance(step.get("expect"), str) or not step["expect"]:
                raise SessionError("step %d: a send needs the reply it expects" % number)
        elif "wait_s" not in step:
            raise SessionError("step %d neither sends nor waits" % number)
        for key in ("timeout_s", "end_timeout_s", "wait_s"):
            if key in step and not (isinstance(step[key], (int, float)) and 0 < step[key] <= 900):
                raise SessionError("step %d: %s must be 0..900 s" % (number, key))
        if "end" in step and "send" not in step:
            raise SessionError("step %d: end needs a send" % number)
        steps.append(step)
    return steps


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


def run_steps(steps, capture, clock):
    """Send the plan; returns (step results, stop reason). Stops at the first refusal or silence."""
    results = []
    reason = "completed"
    for number, step in enumerate(steps, 1):
        entry = {"step": number, "note": step.get("note", "")}
        results.append(entry)
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
    return results, reason


def repository_state():
    try:
        revision = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True,
                                  timeout=30, check=False).stdout.strip()
        status = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain"], capture_output=True, text=True,
                                timeout=30, check=False).stdout
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


def preflight(capture):
    """Ping once; refuse unless the image speaks these lines and holds no soak or trial record."""
    capture.until(0.2, lambda line: False)  # Retain whatever already waits; never flush unseen evidence.
    start = len(capture.lines)
    capture.send(b"p")
    found = capture.until(PING_TIMEOUT_S, lambda line: line.startswith("PONG "), start)
    if found is None:
        return None, "no_ping"
    values = dict(word.split("=", 1) for word in found[1].split()[1:] if "=" in word)
    if values.get("pdo") != "1":
        return values, "image_without_pdo_lines"
    if values.get("soak") != "0":
        return values, "soak_running"
    if values.get("trial") != "0":
        return values, "trial_record_held"
    if values.get("watch") != "0":
        return values, "watch_running"
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
               "plan_file": str(args.plan), "reset": False, "retry": False, **repository_state()}
    port = backend.open(usb["port"])
    capture = Capture(port, clock)
    try:
        pong, refusal = preflight(capture)
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
                         "txConfirmed": v["txConfirmed"], "completed": v["completed"]})
    groups = _raw_records(records)
    maps = pdo_maps(groups)
    report["pdo_records"] = groups
    report["maps"] = {"%d/%d" % key: ["%04x:%02x/%d" % e for e in entries] for key, entries in maps.items()}
    report["nmt"] = nmts
    report["watches"] = []
    for watch in watches:
        out = {"begin": watch["begin"], "end": watch["end"], "console_gaps": watch["gaps"],
               "last_sum": watch["sums"][-1] if watch["sums"] else None, "ids": {}}
        for ident, frames in sorted(watch["frames"].items()):
            timed = [f["hi"] for f in frames if f["timed"]]
            entry = {"printed": len(frames), "host_stats": _stats(timed), "device": watch["ids"].get(ident)}
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
                after = [t for t in timed if t > nmt["completed"]]
                before = [t for t in timed if t <= nmt["completed"]]
                around.append({"nmt": nmt["name"], "completed": nmt["completed"],
                               "last_before": before[-1] if before else None,
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
            print("watch %s console_gaps=%s" % (watch["begin"].get("seconds"), watch["console_gaps"]))
            for ident, entry in watch["ids"].items():
                print("  %s %s" % (ident, json.dumps({k: entry[k] for k in entry if k != "device"})))
        for nmt in report["nmt"]:
            print("nmt %s" % json.dumps(nmt))
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
