#!/usr/bin/env python3
"""One bounded console session with the shared S2 image that s2_flash.py --app probe-idf-optak-console wrote.

The image (examples/probe_idf with sdkconfig.optak_s2 and no smoke hook) runs the shared
ProbeConsole on UART0 at 115200 8N1 (docs/cli_contract.md). It prints one startup line per boot,

  MotorControl-CAN: board accepted; CAN inactive until explicit start.

then answers console lines. Nothing in it starts the controller: FirmwareProbe::start is only
called by the test-only smoke hook, which this image does not compile. This runner:

1. checks the script, and with --artifact the upload record (a flashed probe-idf-optak-console
   record made through this USB serial), before anything else; a refusal enumerates nothing;
2. requires the recorded ESP-Prog-2 (USB serial 94A990398B40) uniquely on the recorded port
   COM15, and no other port, and opens it with DTR and RTS deasserted, so opening drives
   neither EN nor BOOT;
3. with --reset-en pulses EN once through RTS with DTR deasserted (s2_smoke.py's reset), then
   requires that startup line within --boot-seconds and two quiet seconds after it. Another
   first line (the bench image's S2_BENCH ready, a fixture or failure banner, a SMOKE line) or
   any byte in the quiet time sends nothing. Without a reset the line is printed only if the
   board boots while the port is open (reset it by hand); --no-boot-line instead attaches to a
   running image: after 0.5 s it sends only LF "@9" LF, which ends any partial line and which
   the bench and smoke images ignore, and requires the console's invalid_line reply with id 9;
   the choice is recorded;
4. sends the --script lines: each line as written plus LF; "!hex:..." sends those bytes and
   nothing else; lines starting with "#" and empty lines are skipped; "{op}" is replaced by the
   operation_id of the latest accepted reply. By default it waits, per line, for the correlated
   final record of every "@ID" line it completes (a reply, or for an accepted operation its
   terminal, after any streamed records), or for 0.3 s of quiet after a line without an ID,
   at most --line-timeout; --gap-ms paces by a fixed gap and --burst sends every line at once;
5. with --soak SECONDS repeats the script until that time ends, giving every "@ID" line a new
   increasing ID, and prints and appends a per-minute summary line to minutes.jsonl;
6. drains, captures TAIL_S more and writes rx.bin (every byte), timeline.jsonl (each complete
   line with host monotonic times of its first and last byte and its classification), tx.jsonl,
   request.json and summary.json to a new --output directory.

Each JSON record must satisfy console_jsonl.record_problems (test/console_protocol_test.py's
rules). The runner models ProbeConsole::feed's line framing, so it knows which complete lines the
image sees and the ID each reply carries, including for damaged lines. It counts missing (no
final within --line-timeout), late, duplicate, malformed, oversize, out-of-order (an ordinary
immediate reply after a later one) and unsolicited records, and the final-record latency.
Debug traces (id 0) are counted, not failed. Human-text lines are captured, not validated;
a startup, ROM or failure line after the session began is a failure. --account sends
"@4294967294 drv" before and "@4294967295 drv" after the script; with --burst a missing reply
then passes only when the image's consoleDropped counter grew by at least the number missing.

Exit status: 0 every check passed; 1 something was sent and a check failed, the run was
interrupted (Ctrl-C ends it cleanly and still writes summary.json) or the capture failed; 2
refused, nothing from the script was sent (with --no-boot-line only the identity probe may
have been). --dry-run reads the script and prints each line's framing; it opens nothing.
"""

import argparse
from array import array
from collections import OrderedDict, deque
import hashlib
import json
import math
from pathlib import Path
import re
import sys

from console_jsonl import RECORD_LIMIT, record_problems
from s2_bench import BAUD, FIXTURE, RealBackend, RealClock, Refused, check_identity, repository_state
from s2_smoke import BENCH_READY, PASSIVE, pulse_en

CONSOLE_APP = "probe-idf-optak-console"
TEXT_LIMIT = 255  # ProbeConsole::LINE_CAPACITY minus its NUL.
TOKEN_LIMIT = 24
ID_MAX = 4294967295
AFTER_BOOT_S = 2.0  # The smoke image prints SMOKE ready within this host bound; this image prints nothing.
ATTACH_SETTLE_S = 0.5
ATTACH_PROBE_ID = 9
ATTACH_PROBE = b"\n@9\n"  # Bytes the bench image ('@', '9') and the smoke image (not G) do not act on.
PROBE_S = 2.0
QUIET_S = 0.3
TAIL_S = 1.0
ACCOUNT_IDS = (ID_MAX - 1, ID_MAX)
READ_CHUNK = 4096
WINDOW = 4096  # Closed requests kept for duplicate detection.
PROBLEM_LIMIT = 200
LIMITS = {"boot_seconds": (20.0, 1, 120), "line_timeout": (5.0, 0.1, 120), "gap_ms": (None, 0, 60000),
          "soak": (None, 1, 7 * 24 * 3600)}
CONTROL = ("cancel", "recover", "stop")  # Reserved reply queues may overtake ordinary replies.
STREAMED = ("help", "capability", "axis", "value", "finding", "motion_value", "stop_attempt")
# A line after the session began that only a boot, the smoke hook, the bench image or a firmware
# failure prints. A human reply starts with its command token and ": ".
RESTART = re.compile(r"(MotorControl-CAN: [a-z]|ESP-ROM:|rst:0x|SMOKE |S2_BENCH ready)")


def first_line(line):
    """s2_smoke.py's rule for the first line an image prints after its ROM banner."""
    return "MotorControl-CAN:" in line or "SMOKE" in line or line.startswith(BENCH_READY)


# ---------------------------------------------------------------- the image's line framing
def decimal(text):
    """ProbeConsole's decimal(): 1..UINT32_MAX without sign, else 0."""
    if not text or not text.isdigit():
        return 0
    value = int(text)
    return value if 0 < value <= ID_MAX else 0


def parse_line(text):
    """parseConsoleLine on bytes without terminator: (id, tokens) or None when invalid."""
    if not 1 <= len(text) <= TEXT_LIMIT or any((c < 32 and c != 9) or c > 126 for c in text):
        return None
    words = re.split(rb"[ \t]+", text.strip(b" \t")) if text.strip(b" \t") else []
    identity = 0
    if words and words[0].startswith(b"@"):
        identity = decimal(words[0][1:].decode("ascii"))
        if not identity:
            return None
        words = words[1:]
    if not words or len(words) > TOKEN_LIMIT:
        return None
    return identity, [word.decode("ascii") for word in words]


def prefix_id(text):
    """The ID ProbeConsole::feed gives an invalid line's reply: '@' first, up to ten digits, then
    the end, a space or a TAB."""
    if text[:1] != b"@":
        return 0
    n = 0
    while n < 10 and n + 1 < len(text) and 48 <= text[n + 1] <= 57:
        n += 1
    if n + 1 == len(text) or text[n + 1] in (32, 9):
        return decimal(text[1:n + 1].decode("ascii"))
    return 0


class ConsoleInput:
    """ProbeConsole::feed's framing: CR, LF or CRLF ends a line, empty lines are ignored, a bad byte
    or a 256th text byte discards the line through its terminator. Platform input loss is not
    visible to the host and is not modelled."""

    def __init__(self):
        self.line = bytearray()
        self.discard = False
        self.previous_cr = False

    def feed(self, data):
        lines = []
        for byte in data:
            if byte in (10, 13):
                if byte == 10 and self.previous_cr:
                    self.previous_cr = False
                    continue
                self.previous_cr = byte == 13
                if self.line or self.discard:
                    lines.append(self._complete())
                continue
            self.previous_cr = False
            if self.discard:
                continue
            if (byte < 32 and byte != 9) or byte > 126 or len(self.line) == TEXT_LIMIT:
                self.discard = True
                continue
            self.line.append(byte)
        return lines

    def _complete(self):
        text, discarded = bytes(self.line), self.discard
        self.line, self.discard = bytearray(), False
        parsed = None if discarded else parse_line(text)
        if parsed is None:
            identity = prefix_id(text)
            return {"valid": False, "discarded": discarded, "id": identity, "command": "input",
                    "text": text.decode("ascii", errors="replace")}
        return {"valid": True, "discarded": False, "id": parsed[0], "command": parsed[1][0],
                "text": text.decode("ascii")}

    def pending(self):
        return bool(self.line) or self.discard


# ---------------------------------------------------------------- script
def load_script(path):
    """Sendable lines as dicts with line number, bytes or text; refuses an unusable script."""
    try:
        raw = Path(path).read_bytes()
    except OSError as error:
        raise Refused("script %s is unreadable: %s" % (path, error))
    entries = []
    for number, line in enumerate(raw.split(b"\n"), 1):
        line = line[:-1] if line.endswith(b"\r") else line
        if not line or line.startswith(b"#"):
            continue
        if line.startswith(b"!hex:"):
            digits = re.sub(rb"[\s]", b"", line[5:])
            try:
                data = bytes.fromhex(digits.decode("ascii"))
            except ValueError:
                raise Refused("script line %d: invalid hex" % number)
            if not data:
                raise Refused("script line %d: empty hex" % number)
            entries.append({"line": number, "raw": data})
            continue
        try:
            text = line.decode("ascii")
        except UnicodeDecodeError:
            raise Refused("script line %d is not ASCII; use !hex: for raw bytes" % number)
        entries.append({"line": number, "text": text})
    if not entries:
        raise Refused("script %s has no line to send" % path)
    lf = raw.replace(b"\r\n", b"\n")
    return {"path": str(path), "sha256": hashlib.sha256(raw).hexdigest(),
            "lf_sha256": hashlib.sha256(lf).hexdigest(), "entries": entries}


ID_PREFIX = re.compile(r"^([ \t]*)@(\d+)(?=[ \t]|$)")


def rewrite_id(text, identity):
    """The text with its leading @ID replaced, or unchanged when it has none."""
    return ID_PREFIX.sub(lambda m: "%s@%d" % (m.group(1), identity), text, count=1)


def load_artifact(artifact, serial):
    """The upload this session belongs to; it identifies the image, it cannot prove what runs."""
    path = Path(artifact) / "build.json"
    try:
        record = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise Refused("build record %s is unreadable: %s" % (path, error))
    app = record.get("app") if isinstance(record.get("app"), dict) else {}
    if app.get("name") != CONSOLE_APP:
        raise Refused("build record is the %s image, not %s" % (app.get("name") or "s2-bench", CONSOLE_APP))
    if record.get("status") != "flashed":
        raise Refused("build record status is %s, not flashed" % record.get("status"))
    if (record.get("usb") or {}).get("usb_serial") != serial:
        raise Refused("the record was flashed through USB serial %s, not %s" % (
            (record.get("usb") or {}).get("usb_serial"), serial))
    hashes = record.get("image_sha256") if isinstance(record.get("image_sha256"), dict) else {}
    return {"record": str(path), "elf": hashes.get(record.get("app_elf")), "app_elf": record.get("app_elf"),
            "base_commit": record.get("base_commit"), "project_version": record.get("project_version")}


# ---------------------------------------------------------------- correlation
class Request:
    __slots__ = ("seq", "source", "id", "command", "sent", "state", "first", "operation", "final",
                 "latency", "ordered", "account", "record")

    def __init__(self, seq, source, identity, command, sent, account=False):
        self.seq, self.source, self.id, self.command, self.sent = seq, source, identity, command, sent
        self.state, self.first, self.operation, self.final, self.latency = "sent", None, None, None, None
        self.ordered = command not in CONTROL
        self.account = account
        self.record = None  # The whole final record, kept only for the drv accounting requests.

    def outcome(self):
        return {"seq": self.seq, "source": self.source, "id": self.id, "command": self.command,
                "state": self.state, "first": self.first, "operation_id": self.operation, "final": self.final,
                "latency_ms": None if self.latency is None else round(self.latency * 1000, 3)}


def summarize(record):
    keys = ("type", "command", "ok", "status", "error", "result", "outcome", "operation_id", "detail")
    return {key: record[key] for key in keys if key in record}


def latency_stats(values):
    if not values:
        return {"count": 0, "min": None, "median": None, "p99": None, "max": None}
    ordered = sorted(values)
    count = len(ordered)
    middle = ordered[count // 2] if count % 2 else (ordered[count // 2 - 1] + ordered[count // 2]) / 2
    rank = ordered[max(0, math.ceil(0.99 * count) - 1)]
    return {"count": count, "min": round(ordered[0] * 1000, 3), "median": round(middle * 1000, 3),
            "p99": round(rank * 1000, 3), "max": round(ordered[-1] * 1000, 3)}


COUNTERS = ("sent_json", "sent_human", "finals", "accepted", "streamed", "trace", "unsolicited", "duplicate",
            "late", "missing", "malformed", "oversize", "out_of_order", "command_mismatch", "human_lines",
            "restart_lines")


class Tracker:
    """Correlates every received record with the requests the image saw; pure and clock-free."""

    def __init__(self, keep=True):
        self.keep = keep
        self.counts = dict.fromkeys(COUNTERS, 0)
        self.open = {}  # id -> deque of Request in sent/accepted state
        self.timed_out = OrderedDict()  # Request.seq -> Request
        self.closed = OrderedDict()  # id -> Request, bounded
        self.used = set()
        self.soak_range = None
        self.requests = []
        self.latencies = array("d")
        self.minute = []
        self.highest_ordered = -1
        self.last_operation = None
        self.problems = []
        self.seq = 0

    def problem(self, text):
        if len(self.problems) < PROBLEM_LIMIT:
            self.problems.append(text)
        elif len(self.problems) == PROBLEM_LIMIT:
            self.problems.append("further problems omitted; see timeline.jsonl")

    def add(self, line, source, sent, soak=False, account=False):
        """One complete line the image will see; returns its Request, or None for a human line."""
        self.seq += 1
        if not line["id"]:
            self.counts["sent_human"] += 1
            return None
        request = Request(self.seq, source, line["id"], line["command"], sent, account)
        self.counts["sent_json"] += 1
        self.open.setdefault(request.id, deque()).append(request)
        if soak:
            low, high = self.soak_range or (request.id, request.id)
            self.soak_range = (min(low, request.id), max(high, request.id))
        else:
            self.used.add(request.id)
        if self.keep:
            self.requests.append(request)
        return request

    def outstanding(self):
        return sum(len(queue) for queue in self.open.values())

    def was_used(self, identity):
        return identity in self.used or bool(self.soak_range and self.soak_range[0] <= identity <= self.soak_range[1])

    def _match(self, identity, states, command):
        queue = self.open.get(identity)
        if not queue:
            return None, False
        candidates = [request for request in queue if request.state in states]
        for request in candidates:
            if command is None or request.command == command:
                return request, False
        return (candidates[0], True) if candidates else (None, False)

    def _close(self, request, state, record, now):
        queue = self.open[request.id]
        queue.remove(request)
        if not queue:
            del self.open[request.id]
        request.state = state
        request.final = summarize(record)
        if request.account:
            request.record = record
        request.latency = now - request.sent
        self.latencies.append(request.latency)
        self.minute.append(request.latency)
        self.closed[request.id] = request
        self.closed.move_to_end(request.id)
        while len(self.closed) > WINDOW:
            self.closed.popitem(last=False)

    def _first(self, request, record):
        if request.first is not None:
            return
        request.first = record.get("type")
        if record.get("type") == "reply" and request.ordered:
            if request.seq < self.highest_ordered:
                self.counts["out_of_order"] += 1
                self.problem("out of order: reply for id %d (sent %d) after a later request's reply" % (
                    request.id, request.seq))
            else:
                self.highest_ordered = request.seq

    def record(self, record, now):
        """Classify one parsed record; returns the class written to the timeline."""
        identity, kind, command = record.get("id"), record.get("type"), record.get("command")
        if identity == 0 and kind == "trace":
            self.counts["trace"] += 1
            return "trace"
        if not isinstance(identity, int) or not identity:
            self.counts["unsolicited"] += 1
            self.problem("record without a request id: %s" % summarize(record))
            return "unsolicited"
        accepted = kind == "reply" and record.get("result") == "accepted" and record.get("ok") is True
        if kind == "reply":
            request, mismatch = self._match(identity, ("sent",), command)
        elif kind == "terminal":
            request, mismatch = self._match(identity, ("accepted",), command)
            if request is None:
                request, mismatch = self._match(identity, ("sent",), command)
                if request is not None:
                    self.counts["out_of_order"] += 1
                    self.problem("terminal for id %d before its acceptance" % identity)
        else:
            request, mismatch = self._match(identity, ("sent", "accepted"), None)
        if request is None:
            late = next((r for r in self.timed_out.values() if r.id == identity), None)
            if late is not None and kind in ("reply", "terminal") and not (kind == "reply" and accepted):
                del self.timed_out[late.seq]
                late.state = "late"
                late.final = summarize(record)
                late.latency = now - late.sent
                self.counts["late"] += 1
                self.counts["missing"] -= 1
                self.problem("late: final record for id %d after %.3f s" % (identity, late.latency))
                return "late"
            if late is not None:
                return "late"
            if self.was_used(identity):
                self.counts["duplicate"] += 1
                self.problem("duplicate: %s record for id %d after its final record" % (kind, identity))
                return "duplicate"
            self.counts["unsolicited"] += 1
            self.problem("unsolicited: %s record for id %d, which was never sent" % (kind, identity))
            return "unsolicited"
        if mismatch:
            self.counts["command_mismatch"] += 1
            self.problem("command mismatch: id %d sent %s, record names %s" % (identity, request.command, command))
        self._first(request, record)
        if accepted:
            request.state = "accepted"
            request.operation = record.get("operation_id")
            if isinstance(request.operation, int):
                self.last_operation = request.operation
            self.counts["accepted"] += 1
            return "accepted"
        if kind in ("reply", "terminal"):
            self._close(request, "final", record, now)
            self.counts["finals"] += 1
            return "final"
        if kind not in STREAMED:
            self.problem("unknown record type %r for id %d" % (kind, identity))
        self.counts["streamed"] += 1
        return "streamed"

    def expire(self, now, timeout):
        """Requests without a final record within timeout of their send become missing."""
        expired = []
        for queue in list(self.open.values()):
            for request in list(queue):
                if now - request.sent >= timeout:
                    expired.append(request)
        for request in expired:
            queue = self.open[request.id]
            queue.remove(request)
            if not queue:
                del self.open[request.id]
            request.state = "timed_out"
            self.timed_out[request.seq] = request
            self.counts["missing"] += 1
            self.problem("missing: no final record for id %d (%s, %s) within %.1f s" % (
                request.id, request.command, request.source, timeout))
        return expired


# ---------------------------------------------------------------- capture
class Session:
    """The open port: every byte to rx.bin, complete lines to the timeline, writes to tx.jsonl."""

    def __init__(self, port, clock, directory, tracker):
        self.port, self.clock, self.tracker = port, clock, tracker
        self.directory = Path(directory)
        self.rx = open(self.directory / "rx.bin", "wb")
        self.timeline = open(self.directory / "timeline.jsonl", "w", encoding="utf-8", newline="\n")
        self.tx = open(self.directory / "tx.jsonl", "w", encoding="utf-8", newline="\n")
        self.opened = clock.monotonic()
        self.received = 0
        self.sent = 0
        self.partial = bytearray()
        self.partial_start = None
        self.lines = 0
        self.last_rx = None
        self.phase = "boot"
        self.script_bytes = 0
        self.boot_lines = []
        self.probe_reply = None
        self.input = ConsoleInput()

    def now(self):
        return self.clock.monotonic() - self.opened

    def close(self):
        if self.partial:
            self._timeline({"t_first": self.partial_start, "t": None, "offset": self.received - len(self.partial),
                            "length": len(self.partial), "kind": "unterminated",
                            "text": self.partial.decode("utf-8", errors="replace")})
        for handle in (self.rx, self.timeline, self.tx):
            handle.close()

    def _timeline(self, entry):
        self.timeline.write(json.dumps(entry, sort_keys=True) + "\n")

    def read(self):
        waiting = getattr(self.port, "in_waiting", 0) or 0
        chunk = self.port.read(min(max(waiting, 1), READ_CHUNK))
        if not chunk:
            return b""
        t = self.now()
        self.rx.write(chunk)
        self.last_rx = t
        for byte in chunk:
            if not self.partial:
                self.partial_start = t
            self.partial.append(byte)
            if byte == 10:
                line = bytes(self.partial)
                offset = self.received + 1 - len(line)
                self.partial = bytearray()
                self.on_line(line, self.partial_start, t, offset)
            self.received += 1
        return chunk

    def on_line(self, line, first, last, offset):
        self.lines += 1
        entry = {"n": self.lines, "t_first": round(first, 6), "t": round(last, 6), "offset": offset,
                 "length": len(line), "phase": self.phase}
        text = line.rstrip(b"\n").rstrip(b"\r").decode("utf-8", errors="replace")
        if line.startswith(b"{"):
            record, problems = record_problems(line)
            entry["kind"] = "json"
            if problems:
                entry["problems"] = problems
            if record is not None:
                entry.update(id=record.get("id"), type=record.get("type"), command=record.get("command"))
            if self.phase == "session":
                if len(line) > RECORD_LIMIT:
                    self.tracker.counts["oversize"] += 1
                if any(not p.startswith("oversize") for p in problems):
                    self.tracker.counts["malformed"] += 1
                if problems:
                    self.tracker.problem("record line %d: %s" % (self.lines, "; ".join(problems)))
                entry["class"] = self.tracker.record(record, last) if record is not None else "malformed"
            elif self.phase == "probe" and record is not None and not problems:
                if record.get("id") == ATTACH_PROBE_ID:
                    self.probe_reply = record
                entry["class"] = "probe"
            else:
                entry["class"] = self.phase
        else:
            entry["kind"] = "human"
            entry["text"] = text
            if self.phase == "session":
                self.tracker.counts["human_lines"] += 1
                if b'"schema"' in line:
                    self.tracker.counts["malformed"] += 1
                    self.tracker.problem("damaged record line %d: %s" % (self.lines, text[:80]))
                    entry["class"] = "malformed"
                elif RESTART.match(text):
                    self.tracker.counts["restart_lines"] += 1
                    self.tracker.problem("startup or failure line after the session began: %s" % text[:120])
                    entry["class"] = "restart"
                else:
                    entry["class"] = "human"
            else:
                entry["class"] = self.phase
                self.boot_lines.append(text)
        self._timeline(entry)

    def send(self, data, source, soak=False, account=False):
        """Write once; a short or failed write is never replayed. Returns (Requests, framed lines)."""
        t = self.now()
        lines = self.input.feed(data)
        entry = {"t": round(t, 6), "offset": self.sent, "length": len(data), "hex": data.hex(), "source": source,
                 "lines": [{"id": line["id"], "command": line["command"], "valid": line["valid"]} for line in lines]}
        try:
            text = data.decode("ascii")
            if all(32 <= ord(c) <= 126 or c in "\t\r\n" for c in text):
                entry["text"] = text
        except UnicodeDecodeError:
            pass
        self.tx.write(json.dumps(entry, sort_keys=True) + "\n")
        self.tx.flush()
        if self.phase == "session":
            self.script_bytes += len(data)  # Counted before the write: a failed write may have sent it.
        written = self.port.write(data)
        self.sent += len(data)
        if written != len(data):
            raise RuntimeError("short write (%s of %d bytes); never replayed" % (written, len(data)))
        if self.phase != "session":
            return [], lines
        requests = []
        for line in lines:
            request = self.tracker.add(line, source, t, soak=soak, account=account)
            if request is not None:
                requests.append(request)
        return requests, lines

    def pump(self, seconds, done=lambda: False):
        """Read until done() or the deadline; returns True when done() ended it."""
        deadline = self.now() + seconds
        while True:
            if done():
                return True
            if self.now() >= deadline:
                return False
            self.read()

    def quiet(self, since):
        reference = since if self.last_rx is None else max(self.last_rx, since)
        return self.now() - reference >= QUIET_S

    def flush(self):
        self.rx.flush()
        self.timeline.flush()


# ---------------------------------------------------------------- phases
def boot(session, args, meta):
    """None when the image may be sent the script, else the reason nothing is sent."""
    if args.reset_en:
        pulse_en(session.port, session.clock, meta)
    if args.no_boot_line:
        session.pump(ATTACH_SETTLE_S)
        session.phase = "probe"
        meta["identity_probe"] = {"sent_hex": ATTACH_PROBE.hex(), "id": ATTACH_PROBE_ID}
        session.send(ATTACH_PROBE, "identity-probe")
        session.pump(PROBE_S, lambda: session.probe_reply is not None)
        reply = session.probe_reply
        meta["identity_probe"]["reply"] = reply
        if not reply or reply.get("command") != "input" or reply.get("ok") is not False or \
                reply.get("error") != "invalid_line":
            return "no_console_identity"
        session.pump(QUIET_S)
        return None

    if not session.pump(args.boot_seconds, lambda: any(first_line(line) for line in session.boot_lines)):
        return "startup_line_missed"
    index = next(n for n, line in enumerate(session.boot_lines) if first_line(line))
    line = meta["startup_line"] = session.boot_lines[index]
    if line != PASSIVE:
        return ("bench_image" if line.startswith(BENCH_READY) else "smoke_image" if "SMOKE" in line
                else "not_the_inactive_configured_image")
    # Lines that arrived in the startup line's own read count as much as later ones.
    session.pump(AFTER_BOOT_S)
    later = session.boot_lines[index + 1:]
    if any("SMOKE" in text for text in later):
        return "smoke_image"
    if later or session.partial:
        meta["after_startup"] = later + ([session.partial.decode("utf-8", errors="replace")] if session.partial
                                         else [])
        return "output_after_startup"
    return None


def substitute(text, tracker):
    if "{op}" not in text:
        return text, None
    if tracker.last_operation is None:
        return None, "no accepted operation for {op}"
    return text.replace("{op}", str(tracker.last_operation)), None


def wait_for(session, args, requests, sent_at, human):
    """Default pacing: every request final (or timed out), then quiet if a human line was sent."""
    timeout = args.line_timeout

    def done():
        session.tracker.expire(session.now(), timeout)
        if any(r.state in ("sent", "accepted") for r in requests):
            return False
        return not human or session.quiet(sent_at)
    session.pump(timeout + QUIET_S, done)


class Minutes:
    """Per-minute soak lines, printed and appended to minutes.jsonl for another process to follow."""

    def __init__(self, session, clock, path, started):
        self.session, self.clock, self.path, self.started = session, clock, path, started
        self.index = 0
        self.previous = dict(session.tracker.counts)

    def due(self, now):
        return now - self.started >= 60.0 * (self.index + 1)

    def emit(self, now, final=False):
        tracker = self.session.tracker
        counts = dict(tracker.counts)
        delta = {key: counts[key] - self.previous[key] for key in counts}
        line = {"utc": self.clock.utc(), "minute": self.index + 1, "elapsed_s": round(now - self.started, 3),
                "final": final, "minute_counts": delta, "total_counts": counts,
                "outstanding": tracker.outstanding(), "latency_ms": latency_stats(tracker.minute),
                "total_latency_ms": latency_stats(tracker.latencies), "rx_bytes": self.session.received,
                "tx_bytes": self.session.sent}
        with open(self.path, "a", encoding="utf-8", newline="\n") as handle:
            handle.write(json.dumps(line, sort_keys=True) + "\n")
        print("soak minute %d: sent %d finals %d missing %d late %d dup %d malformed %d oversize %d "
              "order %d unsolicited %d; latency ms median %s p99 %s max %s" % (
                  line["minute"], delta["sent_json"] + delta["sent_human"], delta["finals"], counts["missing"],
                  counts["late"], counts["duplicate"], counts["malformed"], counts["oversize"],
                  counts["out_of_order"], counts["unsolicited"], line["latency_ms"]["median"],
                  line["latency_ms"]["p99"], line["latency_ms"]["max"]), flush=True)
        self.previous = counts
        tracker.minute = []
        self.index += 1
        self.session.flush()


ACCOUNTED = ("consoleDropped", "inputDropped", "outputRejected")


def account(session, args, meta, identity, key):
    """One paced drv reply: the image's own drop and refusal counters, or None without one."""
    requests, _ = session.send(("@%d drv\n" % identity).encode("ascii"), "account-" + key, account=True)
    wait_for(session, args, requests, session.now(), False)
    record = requests[0].record if requests else None
    values = {name: record[name] for name in ACCOUNTED if isinstance((record or {}).get(name), int)}
    meta.setdefault("account", {})[key] = values or None
    return values or None


def drain(session, args):
    """Every outstanding request final or timed out."""
    def done():
        session.tracker.expire(session.now(), args.line_timeout)
        return not session.tracker.outstanding()
    session.pump(args.line_timeout + QUIET_S, done)


def run_script(session, args, script, meta):
    """Send the script once, or repeatedly for --soak. Ctrl-C raises KeyboardInterrupt through it."""
    tracker = session.tracker
    soak = args.soak is not None
    started = session.now()
    minutes = Minutes(session, session.clock, session.directory / "minutes.jsonl", started) if soak else None
    next_id = args.first_id
    iteration = 0
    while True:
        iteration += 1
        meta["iterations"] = iteration
        for entry in script["entries"]:
            now = session.now()
            if soak and now - started >= args.soak:
                break
            if minutes and minutes.due(now):
                minutes.emit(now)
            source = ("soak:%d:" % iteration if soak else "script:") + str(entry["line"])
            if "raw" in entry:
                data = entry["raw"]
            else:
                text = entry["text"]
                if soak and ID_PREFIX.match(text):
                    text = rewrite_id(text, next_id)
                    next_id = next_id + 1 if next_id < ID_MAX - 2 else 1
                text, problem = substitute(text, tracker)
                if problem:
                    tracker.problem("script line %d not sent: %s" % (entry["line"], problem))
                    meta.setdefault("not_sent", []).append(entry["line"])
                    continue
                data = text.encode("ascii") + b"\n"
            sent_at = session.now()
            requests, lines = session.send(data, source, soak=soak)
            if args.burst:
                session.read()
            elif args.gap_ms is not None:
                session.pump(args.gap_ms / 1000.0)
                tracker.expire(session.now(), args.line_timeout)
            else:
                wait_for(session, args, requests, sent_at, any(not line["id"] for line in lines))
        if not soak or session.now() - started >= args.soak:
            break
    drain(session, args)
    if minutes:
        minutes.emit(session.now(), final=True)


def account_delta(meta):
    """Counter growth between the two drv replies (uint32 wrap respected), or None."""
    account = meta.get("account") or {}
    before, after = account.get("before"), account.get("after")
    if not before or not after:
        return None
    return {name: (after[name] - before[name]) % (ID_MAX + 1) for name in ACCOUNTED
            if name in before and name in after}


def evaluate(tracker, meta, args):
    """(failing counts, problems): every count that fails the run, and the problem texts."""
    counts = tracker.counts
    problems = list(tracker.problems)
    failing = {key: counts[key] for key in ("unsolicited", "duplicate", "late", "malformed", "oversize",
                                             "out_of_order", "command_mismatch", "restart_lines") if counts[key]}
    missing = counts["missing"]
    delta = meta.get("account_delta") or {}
    dropped = delta.get("consoleDropped")
    if missing:
        if args.burst and dropped is not None and dropped >= missing:
            meta["missing_accounted"] = {"missing": missing, "console_dropped_delta": dropped}
        else:
            failing["missing"] = missing
            if args.burst and dropped is not None:
                problems.append("%d replies missing; the image counted only %d dropped" % (missing, dropped))
    if meta.get("not_sent"):
        failing["script_lines_not_sent"] = len(meta["not_sent"])
    if args.account and not delta:
        failing["account_unavailable"] = 1
        problems.append("--account: no drv counters before and after the script")
    return failing, problems


# ---------------------------------------------------------------- command line
def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="Must be the recorded %s" % FIXTURE["port"])
    parser.add_argument("--serial", required=True, help="Exact ESP-Prog2 USB serial number")
    parser.add_argument("--script", type=Path, required=True)
    parser.add_argument("--output", type=Path, help="New evidence directory; must not exist")
    parser.add_argument("--artifact", type=Path, help="s2_flash.py --app %s --flash upload directory" % CONSOLE_APP)
    parser.add_argument("--reset-en", action="store_true",
                        help="After opening, reset the board once through RTS (EN), DTR deasserted")
    parser.add_argument("--no-boot-line", action="store_true",
                        help="Attach to a running image (no reset): identity probe instead of the startup line")
    parser.add_argument("--boot-seconds", type=float, default=LIMITS["boot_seconds"][0])
    parser.add_argument("--line-timeout", type=float, default=LIMITS["line_timeout"][0],
                        help="Seconds a request may wait for its final record")
    pacing = parser.add_mutually_exclusive_group()
    pacing.add_argument("--gap-ms", type=int, help="Fixed pacing instead of waiting for replies")
    pacing.add_argument("--burst", action="store_true", help="Send every line without waiting (pressure test)")
    parser.add_argument("--soak", type=float, metavar="SECONDS", help="Repeat the script for this long")
    parser.add_argument("--first-id", type=int, default=1, help="First rewritten @ID in a soak (default 1)")
    parser.add_argument("--account", action="store_true",
                        help="Read the image's drop counters with drv before and after the script")
    parser.add_argument("--dry-run", action="store_true", help="Read the script and print its framing; opens nothing")
    args = parser.parse_args(argv)
    for name in ("boot_seconds", "line_timeout", "gap_ms", "soak"):
        value = getattr(args, name)
        _, low, high = LIMITS[name]
        if value is not None and not low <= value <= high:
            parser.error("--%s must be %s..%s" % (name.replace("_", "-"), low, high))
    if args.reset_en and args.no_boot_line:
        parser.error("--no-boot-line attaches without a reset; it cannot be combined with --reset-en")
    if args.soak is not None and args.burst:
        parser.error("--soak paces by replies or --gap-ms; --burst is a single pressure test")
    if not 1 <= args.first_id <= ID_MAX - 3:
        parser.error("--first-id must be 1..%d" % (ID_MAX - 3))
    if args.output is None and not args.dry_run:
        parser.error("--output is required")
    return args


def dry_run(args, script, artifact):
    print("DRY RUN: no port enumerated or opened, nothing sent or created.")
    print("  script %s sha256 %s (LF %s), %d lines to send" % (
        script["path"], script["sha256"], script["lf_sha256"], len(script["entries"])))
    if artifact:
        print("  upload record %s: ELF %s, base %s" % (artifact["record"], artifact["elf"], artifact["base_commit"]))
    print("  require USB %s %04X:%04X uniquely on %s (recorded %s); open with DTR and RTS deasserted" % (
        args.serial, FIXTURE["vid"], FIXTURE["pid"], args.port, FIXTURE["port"]))
    if args.no_boot_line:
        print("  attach: send %r and require the console's invalid_line reply with id %d" % (
            ATTACH_PROBE, ATTACH_PROBE_ID))
    else:
        print("  %srequire within %s s: %s, then %s s with no byte" % (
            "pulse EN once, then " if args.reset_en else "", args.boot_seconds, PASSIVE, AFTER_BOOT_S))
    model = ConsoleInput()
    for entry in script["entries"]:
        data = entry["raw"] if "raw" in entry else entry["text"].encode("ascii") + b"\n"
        lines = model.feed(data)
        shown = ", ".join("id=%s %s%s" % (line["id"] or "-", line["command"], "" if line["valid"] else " (invalid)")
                          for line in lines) or "no complete line"
        print("  line %d: %s -> %s" % (entry["line"], data[:60], shown))
    if model.pending():
        print("  the script ends inside a line; the image waits for its terminator")
    print("  pacing: %s" % ("burst" if args.burst else "gap %d ms" % args.gap_ms if args.gap_ms is not None
                            else "final reply, at most %s s per line" % args.line_timeout))
    if args.soak is not None:
        print("  soak %s s from @ID %d" % (args.soak, args.first_id))
    if args.output:
        print("  write %s%s" % (args.output, "; would refuse now: it exists" if args.output.exists() else ""))


def main(argv=None, backend=None, clock=None):
    args = arguments(argv)
    try:
        script = load_script(args.script)
        artifact = load_artifact(args.artifact, args.serial) if args.artifact else None
    except Refused as error:
        print("REFUSED: %s; nothing enumerated, opened, sent or created" % error, file=sys.stderr)
        return 2
    if args.dry_run:
        dry_run(args, script, artifact)
        return 0
    backend = backend or RealBackend()
    clock = clock or RealClock()
    try:
        if args.port.lower() != FIXTURE["port"].lower():
            raise Refused("port %s is not the recorded %s" % (args.port, FIXTURE["port"]))
        usb = check_identity(args.port, args.serial, backend.comports(), True)
        args.output.mkdir(parents=True, exist_ok=False)
    except Refused as error:
        print("REFUSED: %s; nothing opened, sent or created" % error, file=sys.stderr)
        return 2
    except FileExistsError:
        print("REFUSED: %s exists; nothing opened or sent" % args.output, file=sys.stderr)
        return 2
    meta = {"tool": "s2_console", "schema": 1, "started_utc": clock.utc(), "port": args.port, "usb": usb,
            "baud": BAUD, "fixture": dict(FIXTURE), "arguments": {k: (str(v) if isinstance(v, Path) else v)
                                                                  for k, v in sorted(vars(args).items())},
            "script": {key: script[key] for key in ("path", "sha256", "lf_sha256")}, "artifact": artifact,
            "dtr_rts": "deasserted before open", "reset": "en_pulse" if args.reset_en else False,
            "boot": "attached without startup line (--no-boot-line)" if args.no_boot_line else "startup line",
            "retry": False, "status": "running"}
    meta.update(repository_state())
    request_path = args.output / "request.json"
    request_path.write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    tracker = Tracker(keep=args.soak is None)
    session = None
    try:
        port = backend.open(args.port)
        try:
            session = Session(port, clock, args.output, tracker)
            meta["opened_monotonic_s"] = session.opened
            reason = boot(session, args, meta)
            meta["boot_lines"] = session.boot_lines[-64:]
            if reason:
                meta["status"], meta["reason"] = "refused", reason
            else:
                session.phase = "session"
                if args.account:
                    account(session, args, meta, ACCOUNT_IDS[0], "before")
                run_script(session, args, script, meta)
                if args.account:
                    account(session, args, meta, ACCOUNT_IDS[1], "after")
                    meta["account_delta"] = account_delta(meta)
                session.pump(TAIL_S)
                tracker.expire(session.now(), 0)  # Whatever is still open after the tail is missing.
                meta["status"] = "captured"
        finally:
            if session is not None:
                session.close()
            port.close()
    except KeyboardInterrupt:  # Ends the session; summary.json is still written.
        meta["status"] = "interrupted"
    except Exception as error:  # Raw bytes and the reason are kept; nothing is replayed.
        meta["status"] = "failed"
        meta["error"] = "%s: %s" % (type(error).__name__, error)
    meta["ended_utc"] = clock.utc()
    script_sent = bool(session and session.script_bytes)
    failing, problems = evaluate(tracker, meta, args)
    if meta["status"] == "refused" or not script_sent:
        verdict = "not_sent"
    elif meta["status"] != "captured":
        verdict = meta["status"]
    else:
        verdict = "passed" if not failing else "failed"
    summary = {"verdict": verdict, "status": meta["status"], "reason": meta.get("reason"), "error": meta.get("error"),
               "counts": tracker.counts, "failing": failing, "problems": problems,
               "latency_ms": latency_stats(tracker.latencies), "rx_bytes": session.received if session else 0,
               "tx_bytes": session.sent if session else 0, "lines": session.lines if session else 0,
               "account": meta.get("account"), "account_delta": meta.get("account_delta"),
               "missing_accounted": meta.get("missing_accounted"), "outstanding": tracker.outstanding(),
               "iterations": meta.get("iterations"), "last_operation": tracker.last_operation}
    if tracker.keep:
        summary["requests"] = [request.outcome() for request in tracker.requests]
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    meta["verdict"] = verdict
    request_path.write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("Evidence:", args.output)
    print("startup: %s" % (meta.get("startup_line") or meta.get("boot")))
    if meta.get("reason"):
        print("refused: %s; nothing from the script was sent" % meta["reason"])
    counts = tracker.counts
    print("sent %d JSON and %d human lines; finals %d, accepted %d, streamed %d, traces %d" % (
        counts["sent_json"], counts["sent_human"], counts["finals"], counts["accepted"], counts["streamed"],
        counts["trace"]))
    print("missing %d late %d duplicate %d malformed %d oversize %d out-of-order %d unsolicited %d" % (
        counts["missing"], counts["late"], counts["duplicate"], counts["malformed"], counts["oversize"],
        counts["out_of_order"], counts["unsolicited"]))
    print("latency ms: %s" % json.dumps(summary["latency_ms"], sort_keys=True))
    for problem in problems[:20]:
        print("problem: %s" % problem)
    print("verdict: %s" % verdict)
    return 0 if verdict == "passed" else 2 if verdict == "not_sent" else 1


if __name__ == "__main__":
    raise SystemExit(main())
