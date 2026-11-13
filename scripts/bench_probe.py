#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded MotorControl-CAN console client and one explicit probe/read invocation.

Framing/correlation responsibilities follow MotorControl-RS bench_probe, copyright
(c) 2026 THYMOS Solution s.r.o., Jan Havelka. This is the distinct CAN schema, not an RTU/CAN driver.
Imports and offline modes need only Python 3.10+. Live serial is loaded explicitly.
No retry, reconnect, motion, configuration write or campaign is implied by connection.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
import argparse
import json
import math
from pathlib import Path
import re
import secrets
import sys
import time
import uuid

from console_jsonl import ID_MAX, PROFILE, RECORD_LIMIT, SCHEMA, is_uint32, record_problems
from _bench_evidence import BenchError, summarize_operation

MAX_PENDING = 4
MAX_OPERATIONS = 2  # Ordinary capacity advertised by this schema, not universal drive capacity.
MAX_RECORDS = 128
MAX_COMMAND_INPUT = 65536
MAX_SESSION_INPUT = 1048576
MAX_COMMANDS = 1024
MAX_TURNS = 8192
READ_CHUNK = 256
IDLE_SECONDS = 0.001
STATUS = {"OK", "INVALID_CONFIG", "ILLEGAL_VALUE", "UNSUPPORTED", "FRAME_ERROR", "PROTOCOL_ERROR"}
RECORD_KINDS = {"reply", "terminal", "capability", "help", "axis", "value", "finding",
                "motion_value", "stop_attempt", "trace"}


def positive(value, name, maximum=60.0):
    """Validate finite application seconds before any I/O, excluding bool and zero."""
    if type(value) not in (float, int) or not math.isfinite(value) or not 0 < value <= maximum:
        raise ValueError(f"{name} must be finite and in (0, {maximum}] seconds")
    return float(value)


def command_text(command):
    """One complete ASCII command, without a correlation prefix or line terminator."""
    if type(command) is not str or not command or command != command.strip():
        raise ValueError("command must be nonempty, without surrounding whitespace")
    if any(ord(c) < 32 or ord(c) > 126 for c in command) or command.startswith("@"):
        raise ValueError("command contains framing characters or non-ASCII text")
    if len(command.split()) > 24:
        raise ValueError("command exceeds 24 tokens")
    return command


def _uint(value, maximum=ID_MAX):
    return type(value) is int and 0 <= value <= maximum


def _decimal(value, *, signed=False):
    return type(value) is str and re.fullmatch(r"-?(0|[1-9][0-9]*)" if signed else r"0|[1-9][0-9]*", value) is not None


def _bounded_json(value, depth=0):
    if depth > 16:
        raise BenchError("console JSON exceeds nesting limit")
    if isinstance(value, dict):
        for item in value.values():
            _bounded_json(item, depth + 1)
    elif isinstance(value, list):
        for item in value:
            _bounded_json(item, depth + 1)


def read_fields(command):
    """Expected console read shape, not CAN access/interpretation policy.

    Matches Reads.cpp identity/STATUS/MONITORS/FEEDBACK and ProbeConsole readValues.
    The two version fields allow reviewed EDS/specimen widths. Config is Q20-gated;
    a future successful config schema needs explicit client support.
    """
    tokens = command.split()
    if tokens[0] == "probe":
        return {(0x1000, 0): (4,)}
    if tokens[:2] == ["read", "identity"]:
        return {(0x1000, 0): (4,), (0x1009, 0): (1, 2), (0x100a, 0): (1, 2),
                (0x1018, 0): (1,), (0x1018, 1): (4,), (0x1018, 2): (4,),
                (0x1018, 3): (4,), (0x2000, 0): (2,), (0x2001, 0): (2,)}
    block = tokens[2] if len(tokens) > 2 and not tokens[2].isdigit() else "status"
    addresses = {"status": ((0x6041, 2), (0x6061, 1)),
                 "monitors": ((0x2100, 2), (0x2101, 2), (0x2102, 2)),
                 "feedback": ((0x6064, 4), (0x606c, 4))}
    if tokens[:2] == ["read", "state"]:
        return {(index, 0): (width,) for index, width in addresses[block]}
    return None


def supported_command(command):
    """Only forms whose record shapes this client implements; reject others before I/O."""
    if command in ("version", "profile list", "caps", "help", "config", "status", "health", "stats", "drv", "memory"):
        return True
    tokens = command.split()
    if tokens[0] in ("useaddr", "release", "result"):
        maximum = 127 if tokens[0] == "useaddr" else ID_MAX
        return len(tokens) == 2 and _decimal(tokens[1]) and 1 <= int(tokens[1]) <= maximum
    if tokens[0] == "probe":
        tail = tokens[1:]
    elif tokens[:2] in (["read", "identity"], ["read", "config"]):
        tail = tokens[2:]
    elif tokens[:2] == ["read", "state"]:
        tail = tokens[2:]
        if tail and tail[0] in ("status", "monitors", "feedback"):
            tail = tail[1:]
    else:
        return False
    return not tail or len(tail) == 1 and _decimal(tail[0]) and 1 <= int(tail[0]) <= 60000


@dataclass
class Command:
    """Owned local handle. Native records are retained until explicit release.

    epoch is host-only: schema1 supplies no firmware boot/session ID. transmitted
    means serial handoff attempted, never CAN TX or native acceptance. Deadlines
    use the injected monotonic seconds and never renew on partial progress.
    """
    owner: object
    id: int
    command: str
    family: str
    deadline: float
    epoch: str
    operation_id: int | None = None
    accepted: bool = False
    terminal: dict | None = None
    records: list = field(default_factory=list)
    transmitted: bool = False
    released: bool = False
    interruption: str | None = None
    phase: str = "waiting_reply"
    input_bytes: int = 0
    turns: int = 0


class Console:
    """One cooperative nonblocking byte-stream owner; no device driver or replay.

    port.read(size) returns at most size bytes without waiting; port.write(data)
    is one bounded all-or-fail handoff. Inject clock/sleeper for deterministic
    tests. Callbacks must remain bounded and must not reenter this object.
    A stream fault poisons the connection permanently and preserves evidence.
    A fresh object/local epoch cannot prove that firmware or its queues reset.
    """
    def __init__(self, port, *, on_event=None, clock=time.monotonic, sleeper=time.sleep,
                 first_id=None, host_epoch=None):
        if first_id is None:
            first_id = secrets.randbelow(ID_MAX - MAX_COMMANDS) + 1
        if not is_uint32(first_id, 1):
            raise ValueError("first request ID must be uint32 and nonzero")
        epoch = host_epoch or uuid.uuid4().hex
        if type(epoch) is not str or not re.fullmatch(r"[A-Za-z0-9_.:-]{1,64}", epoch):
            raise ValueError("host epoch must be 1..64 plain identifier characters")
        self.port, self.clock, self.sleep = port, clock, sleeper
        self.emit = on_event or (lambda *_args, **_kwargs: None)
        self.host_epoch, self.firmware_epoch = epoch, None
        self.next_id, self.first_id = first_id, first_id
        self.synchronized, self.identified = True, False
        self.handshake = {}
        self.pending, self.operations = {}, {}
        self.last_operation_id = 0
        self.capabilities = {}
        self.buffer = bytearray()
        self.raw_lines, self.parse_failures = deque(maxlen=64), deque(maxlen=16)
        self.raw_dropped = self.received = 0
        self._identifying = False
        self._last_clock = self.clock()
        if type(self._last_clock) not in (float, int) or not math.isfinite(self._last_clock):
            raise ValueError("clock must supply finite monotonic seconds")
        self._started_clock = self._last_clock

    def _now(self):
        now = self.clock()
        if type(now) not in (float, int) or not math.isfinite(now) or now < self._last_clock:
            self._fail("invalid or backward supplied clock", "deadline")
        self._last_clock = now
        return now

    def _fail(self, reason, kind="framing_loss"):
        self.synchronized = False
        for handle in self.pending.values():
            handle.interruption = kind
            if handle.terminal is None:
                handle.phase = "interrupted"
        raise BenchError(reason)

    def _event(self, event, **fields):
        try:
            self.emit(event, host_epoch=self.host_epoch, firmware_epoch=None,
                      host_elapsed_s=self._now() - self._started_clock, **fields)
        except Exception as error:
            self._fail("evidence callback failed; no further command permitted: " + str(error))

    def _check(self):
        if not self.synchronized:
            raise BenchError("console is unsynchronized; no reconnect or replay permitted")
        now = self._now()
        for handle in self.pending.values():
            if handle.terminal is None and now >= handle.deadline:
                self._fail("command response deadline expired; no replay", "deadline")

    def _read(self):
        if getattr(self.port, "is_open", True) is False:
            self._fail("console disconnected", "disconnect")
        try:
            data = self.port.read(READ_CHUNK)
        except (OSError, EOFError) as error:
            self._fail("console read disconnected: " + str(error), "disconnect")
        if type(data) is not bytes or len(data) > READ_CHUNK:
            self._fail("byte stream violated read capacity")
        self.received += len(data)
        if self.received > MAX_SESSION_INPUT:
            self._fail("session input capacity exhausted")
        for handle in self.pending.values():
            handle.input_bytes += len(data)
            if handle.input_bytes > MAX_COMMAND_INPUT:
                self._fail("command input capacity exhausted")
        return data

    def _decode(self, raw, startup):
        if len(self.raw_lines) == self.raw_lines.maxlen:
            self.raw_dropped = min(ID_MAX, self.raw_dropped + 1)
        self.raw_lines.append(raw)
        self._event("line", raw_hex=raw.hex())
        try:
            text = raw.decode("utf-8")
        except UnicodeError:
            self.parse_failures.append({"raw_hex": raw.hex(), "reason": "invalid UTF-8"})
            self._fail("invalid UTF-8 console line")
        if not text.lstrip().startswith(("{", "[")):
            if startup:
                return None
            self._fail("post-handshake banner/reset or unexpected text", "reboot")
        item, problems = record_problems(raw)
        if problems:
            self.parse_failures.append({"raw_hex": raw.hex(), "reason": problems})
            self._event("parse_failure", problems=problems)
            self._fail("invalid console record: " + "; ".join(problems))
        try:
            _bounded_json(item)
        except BenchError as error:
            self._fail(str(error))
        if startup:
            self._fail("unexpected structured reply during startup")
        if item["type"] not in RECORD_KINDS:
            self._fail("unsupported console record kind")
        return item

    def _consume(self, data, startup=False):
        # Append one byte at a time so one hostile chunk cannot allocate an
        # unbounded incomplete line. Decode UTF-8 only after the whole line.
        for byte in data:
            self.buffer.append(byte)
            if len(self.buffer) > RECORD_LIMIT:
                self._fail("console record exceeds 511-byte bound")
            if byte != 10:
                continue
            raw = bytes(self.buffer)
            self.buffer.clear()
            item = self._decode(raw, startup)
            if item is not None:
                self._route(item)

    def drain_startup(self, duration_s):
        """Collect finite startup text only, before any request; never flush stale JSON."""
        duration_s = positive(duration_s, "startup duration")
        if not self.synchronized or self.next_id != self.first_id:
            raise BenchError("startup drain is only permitted before requests")
        deadline = self._now() + duration_s
        for _ in range(MAX_TURNS):
            if self._now() >= deadline:
                if self.buffer:
                    self._fail("incomplete startup line")
                return
            data = self._read()
            self._consume(data, startup=True)
            if not data:
                self.sleep(IDLE_SECONDS)
        self._fail("startup turn capacity exhausted")

    def _status(self, item):
        if (type(item.get("ok")) is not bool or type(item.get("status")) is not str or
                item["status"] not in STATUS or item["ok"] != (item["status"] == "OK")):
            self._fail("reply lacks typed ok/status")
        if "detail" in item and (type(item["detail"]) is not int or not -(2**31) <= item["detail"] < 2**31):
            self._fail("reply detail is not int32")

    def _append(self, handle, item):
        if len(handle.records) == MAX_RECORDS:
            self._fail("command record capacity exhausted")
        handle.records.append(item)

    def _route(self, item):
        self._check()  # Receipt at equality loses to the immutable deadline.
        if item["type"] == "trace" and item["id"] == 0:
            if item["command"] != "debug":
                self._fail("unsolicited trace has unexpected command")
            self._event("trace", record=item)
            return
        handle = self.pending.get(item["id"])
        if handle is None:
            self._fail("unmatched/stale console request ID")
        if handle.terminal is not None:
            self._fail("duplicate record after completed request")
        kind = item["type"]
        family = handle.family
        if kind not in ("capability", "help") and item["command"] != family:
            self._fail("reply command differs from request")
        if kind in ("capability", "help"):
            if handle.command != ("caps" if kind == "capability" else "help"):
                self._fail("unexpected inventory record")
            if any(row["type"] == kind and row["command"] == item["command"] for row in handle.records):
                self._fail("duplicate inventory entry")
            if kind == "capability":
                if (type(item.get("implemented")) is not bool or
                    type(item.get("owner")) is not int or not 1 <= item["owner"] <= 50 or
                    type(item.get("effect")) is not str or
                    item["effect"] not in {"cache", "pure", "host_configuration", "local_cancel", "host_recovery", "device_read", "device_setting", "device_action"} or
                    type(item.get("completion")) is not str or item["completion"] not in {"reply", "asynchronous"} or
                    type(item.get("reason")) is not str):
                    self._fail("invalid capability inventory")
        elif kind == "reply":
            self._status(item)
            if item.get("result") == "accepted":
                capability = self.capabilities.get(family)
                if (not item["ok"] or not capability or capability["completion"] != "asynchronous" or
                        handle.accepted or not is_uint32(item.get("operation_id"), 1)):
                    self._fail("duplicate/invalid operation admission")
                operation = item["operation_id"]
                if operation <= self.last_operation_id or len(self.operations) >= MAX_OPERATIONS:
                    self._fail("reused operation identity or retained capacity exhausted")
                handle.accepted, handle.operation_id, handle.phase = True, operation, "admitted"
                self.operations[operation] = handle
                self.last_operation_id = operation
            elif handle.accepted:
                self._fail("ordinary reply replaced an admitted operation")
            else:
                capability = self.capabilities.get(family)
                if item["ok"] and capability and capability["completion"] == "asynchronous":
                    self._fail("asynchronous route returned success without admission/terminal")
                # Cached result inspection has a new request ID, but original command_id.
                if family == "result" and "outcome" in item:
                    self._terminal_fields(item)
                    tokens = handle.command.split()
                    if len(tokens) != 2 or tokens[1] != str(item["operation_id"]):
                        self._fail("cached result differs from requested operation")
                    retained = self.operations.get(item["operation_id"])
                    if retained is not None:
                        if retained.terminal is None or retained.id != item["command_id"]:
                            self._fail("cached result differs from original request")
                        routing = {"type", "id", "command"}
                        if ({key: value for key, value in retained.terminal.items() if key not in routing} !=
                                {key: value for key, value in item.items() if key not in routing}):
                            self._fail("cached result changed retained terminal evidence")
                elif family == "result" and item.get("result") == "pending":
                    tokens = handle.command.split()
                    if (not is_uint32(item.get("operation_id"), 1) or
                            not is_uint32(item.get("command_id"), 1) or
                            len(tokens) != 2 or tokens[1] != str(item["operation_id"])):
                        self._fail("pending lookup differs from requested identity")
                    retained = self.operations.get(item["operation_id"])
                    if retained is not None and (retained.id != item["command_id"] or retained.terminal is not None):
                        self._fail("pending lookup regressed retained terminal evidence")
                if handle.command in ("caps", "help") and item.get("ok") and item.get("result") != "complete":
                    self._fail("inventory lacks final complete record")
                handle.terminal, handle.phase = item, "completed"
        elif kind == "terminal":
            self._status(item)
            self._terminal_fields(item)
            if (not handle.accepted or item["operation_id"] != handle.operation_id or
                    item["command_id"] != handle.id):
                self._fail("terminal has no matching admitted operation")
            if family in ("probe", "read") and item["ok"]:
                expected = read_fields(handle.command)
                values = [row for row in handle.records if row["type"] == "value"]
                if (expected is None or {(row["index"], row["subindex"]) for row in values} != set(expected) or
                        any(not row["current"] or row["raw"] is None for row in values)):
                    self._fail("successful typed read lacks its complete current object evidence")
                if item["outcome"] != "SUCCESS" or item["uncertain"] or item.get("abort_code") is not None:
                    self._fail("successful typed read has contradictory native outcome")
            if family in ("probe", "read"):
                values = [row for row in handle.records if row["type"] == "value"]
                if any(int(row["latest_us"]) > int(item["completed_us"]) for row in values):
                    self._fail("terminal precedes its native observations")
            handle.terminal, handle.phase = item, "completed"
        else:
            if (not handle.accepted or not is_uint32(item.get("operation_id"), 1) or
                    item["operation_id"] != handle.operation_id):
                self._fail("value/finding lacks matching admission")
            if family in ("probe", "read") and kind != "value":
                self._fail("typed read received an unexpected record kind")
            if kind == "value":
                self._value_fields(item)
                expected = read_fields(handle.command)
                if expected is None or item["width"] not in expected.get((item["index"], item["subindex"]), ()):
                    self._fail("typed read value differs from requested object/width")
                if any(row["type"] == "value" and (row["index"], row["subindex"]) ==
                       (item["index"], item["subindex"]) for row in handle.records):
                    self._fail("duplicate object value")
        self._append(handle, item)

    def _terminal_fields(self, item):
        if ("abort_code" not in item or not is_uint32(item.get("command_id"), 1) or not is_uint32(item.get("operation_id"), 1) or
            type(item.get("outcome")) is not str or type(item.get("uncertain")) is not bool or
            not _decimal(item.get("completed_us")) or int(item["completed_us"]) > 2**64 - 1 or
            (item.get("abort_code") is not None and not _uint(item["abort_code"]))):
            self._fail("terminal contains invalid identity/outcome/native evidence")

    def _value_fields(self, item):
        if (not _uint(item.get("index"), 65535) or not _uint(item.get("subindex"), 255) or
            type(item.get("width")) is not int or not 1 <= item["width"] <= 4 or
            type(item.get("current")) is not bool or type(item.get("unit")) is not str or
            (item.get("raw") is not None and (not _decimal(item["raw"]) or
             int(item["raw"]) >= 1 << (8 * item["width"]))) or
            (item.get("native_signed") is not None and not _decimal(item["native_signed"], signed=True)) or
            not _decimal(item.get("earliest_us")) or not _decimal(item.get("latest_us")) or
            not 0 <= int(item["earliest_us"]) <= int(item["latest_us"]) <= 2**64 - 1):
            self._fail("invalid typed raw value/time evidence")
        if item["current"] and item.get("raw") is None:
            self._fail("current typed value lacks raw data")
        if item.get("native_signed") is not None:
            bits, raw = 8 * item["width"], item.get("raw")
            if raw is None or int(item["native_signed"]) != (int(raw) if int(raw) < 1 << (bits - 1) else int(raw) - (1 << bits)):
                self._fail("signed value disagrees with native raw word")

    def pump(self):
        """Process one bounded read and all its complete lines; never extends a deadline."""
        self._check()
        for handle in self.pending.values():
            handle.turns += 1
            if handle.turns > MAX_TURNS:
                self._fail("command service turn capacity exhausted")
        data = self._read()
        self._check()
        self._consume(data)
        return bool(data)

    def begin(self, command, *, timeout_s=3.0):
        """Validate before one serial handoff. Returns an owned handle, never native success."""
        timeout_s = positive(timeout_s, "command timeout")
        command = command_text(command)
        if not supported_command(command):
            raise BenchError("command form is outside this finite read client contract")
        self._check()
        if not self.identified and not (command == "version" or self._identifying and
                                       command in ("profile list", "caps", "config")):
            raise BenchError("identify product/version/profile/capabilities before requests")
        family = command.split()[0]
        if self.identified and (family not in self.capabilities or not self.capabilities[family]["implemented"]):
            raise BenchError("route unavailable in validated command inventory")
        if family in ("probe", "read") and sum(handle.family in ("probe", "read")
                for handle in self.pending.values()) >= MAX_OPERATIONS:
            raise BenchError("retained native operation capacity exhausted before handoff")
        if len(self.pending) >= MAX_PENDING or self.next_id > ID_MAX or self.next_id - self.first_id >= MAX_COMMANDS:
            raise BenchError("client request capacity exhausted")
        identity = self.next_id
        wire = f"@{identity} {command}\n".encode("ascii")
        if len(wire) - 1 > 255:
            raise ValueError("command with ID exceeds 255-byte input limit")
        deadline = self._now() + timeout_s
        if not math.isfinite(deadline):
            raise ValueError("invalid command deadline")
        # Bytes already waiting cannot become a reply to intent not yet sent.
        # This bounded empty read is no device/firmware retirement proof.
        for _ in range(MAX_TURNS):
            if self._now() >= deadline:
                self._fail("pre-handoff input deadline expired", "deadline")
            if not self.pump():
                if self.buffer:
                    if not self.pending:
                        self._fail("partial record predates new command")
                    self.sleep(IDLE_SECONDS)
                    continue  # A split reply to existing work keeps its original cutoff.
                break
        else:
            self._fail("pre-handoff input turn capacity exhausted")
        handle = Command(self, identity, command, family, deadline, self.host_epoch)
        self._event("intent", request_id=identity, command=command, deadline_s=deadline)
        self.next_id += 1
        self.pending[identity] = handle
        handle.transmitted = True  # Even an exception can follow a partial host write.
        try:
            size = self.port.write(wire)
        except (OSError, EOFError) as error:
            self._fail("serial write failed; handoff uncertain: " + str(error), "disconnect")
        if type(size) is not int or size != len(wire):
            self._fail("short/invalid serial write; command never replayed", "disconnect")
        self._check()
        return handle

    def wait(self, handle, *, release=False):
        """Wait for this exact request, keeping interleaved requests and native errors separate.

        Complete/partial trailing bytes are drained under the original cutoff before
        success returns. A valid terminal followed by damage remains retained evidence,
        but the poisoned stream can issue no further work. No automatic result polling.
        """
        if not isinstance(handle, Command) or handle.owner is not self or handle.epoch != self.host_epoch or handle.released:
            raise BenchError("foreign/stale command handle")
        if handle.id not in self.pending:
            raise BenchError("request is no longer pending")
        while True:
            data = self.pump()
            if handle.terminal is not None and not data and not self.buffer:
                break
            if self._now() >= handle.deadline:
                self._fail("incomplete/trailing console record at deadline", "framing_loss" if handle.terminal else "deadline")
            if not data:
                self.sleep(IDLE_SECONDS)
        if not handle.accepted:
            del self.pending[handle.id]
        result = handle.terminal
        if release and handle.accepted:
            self.release(handle)
        return result

    def command(self, command, *, timeout_s=3.0, release=False):
        """One explicit begin/wait. No request is retried or inferred from connection."""
        return self.wait(self.begin(command, timeout_s=timeout_s), release=release)

    def release(self, handle, *, timeout_s=3.0):
        """Explicitly release only a checked native terminal, preserving local evidence."""
        if (not isinstance(handle, Command) or handle.owner is not self or not handle.accepted or
            handle.terminal is None or handle.released or self.operations.get(handle.operation_id) is not handle):
            raise BenchError("result is not releasable")
        result = self.command(f"release {handle.operation_id}", timeout_s=timeout_s)
        if result["ok"]:
            handle.released = True
            self.pending.pop(handle.id, None)
            del self.operations[handle.operation_id]
        return result

    def identify(self, *, expected_version, profile=PROFILE, timeout_s=10.0):
        """Validate cached identity/capabilities under one bounded overall handshake.

        No object read, selection, controller change or firmware match is inferred.
        Schema1 reports unknown model/firmware and has no boot ID or source commit.
        """
        if type(expected_version) is not str or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", expected_version) or profile != PROFILE:
            raise ValueError("explicit exact library version and implemented profile required")
        if self.identified or self.pending:
            raise BenchError("identify requires a fresh empty client")
        deadline = self._now() + positive(timeout_s, "handshake timeout")
        self._identifying = True
        responses = {}
        try:
            for command in ("version", "profile list", "caps", "config"):
                remaining = deadline - self._now()
                if remaining <= 0:
                    self._fail("handshake deadline expired", "deadline")
                handle = self.begin(command, timeout_s=remaining)
                response = self.wait(handle)
                if not response["ok"]:
                    self._fail("cached handshake command refused")
                responses[command] = response
                if command == "version" and (response.get("product") != "MotorControl-CAN" or
                    response.get("version") != expected_version or
                    type(response.get("operation_capacity")) is not int or response["operation_capacity"] != MAX_OPERATIONS):
                    self._fail("library version/product/capacity mismatch")
                if command == "profile list":
                    profiles = response.get("profiles")
                    if (not isinstance(profiles, list) or len(profiles) != 1 or type(profiles[0]) is not dict or
                        profiles[0].get("name") != profile or profiles[0].get("protocol") != "canopen" or
                        any(profiles[0].get(k) is not None for k in ("model_match", "firmware_match")) or
                        profiles[0].get("hardware_qualified") is not False):
                        self._fail("compiled profile/protocol or qualification identity mismatch")
                if command == "caps":
                    rows = [row for row in handle.records if row["type"] == "capability"]
                    self.capabilities = {row["command"]: row for row in rows}
                    for family, effect, completion in (("probe", "device_read", "asynchronous"),
                        ("read", "device_read", "asynchronous"), ("useaddr", "host_configuration", "reply"),
                        ("release", "host_configuration", "reply")):
                        row = self.capabilities.get(family)
                        if not row or row["effect"] != effect or row["completion"] != completion or not row["implemented"]:
                            self._fail("required typed console capability missing or mismatched")
                    if (response.get("endpoint_range") != [1, 127] or
                        any(type(endpoint) is not int for endpoint in response["endpoint_range"]) or
                        any(response.get(k) is not True for k in ("probe", "identity")) or
                        any(response.get(k) is not False for k in ("can_match_exact_model", "can_match_firmware", "hardware_qualified")) or
                        any(response.get(k) is not None for k in ("model_match", "firmware_match"))):
                        self._fail("capability/firmware identity mismatch")
                if command == "config":
                    self.check_config(response)
            self.handshake = dict(responses, host_epoch=self.host_epoch, firmware_epoch=None)
            self.identified = True
            self._event("identified", identity=self.handshake)
            return self.handshake
        finally:
            self._identifying = False

    def check_config(self, record, *, node=None, can_bitrate=None):
        """Validate supplied host facts. They prove no wire model, active drive node or scale."""
        for name in ("interface", "bitrate_bits_per_second", "selected_node"):
            if record.get(name) is not None and not _uint(record[name]):
                self._fail("invalid cached configuration field")
        if record.get("bus_traffic") is not False:
            self._fail("configuration inspection must be host-only")
        if node is not None and record.get("selected_node") != node:
            self._fail("selected host node does not match explicit request")
        if can_bitrate is not None and record.get("bitrate_bits_per_second") != can_bitrate:
            self._fail("configured interface bitrate does not match explicit request")

    def summary(self, handle, *, evidence_kind="host-tested"):
        """Owned pure evidence projection; host epoch never substitutes for a firmware ID."""
        if handle.owner is not self:
            raise BenchError("foreign command handle")
        return summarize_operation(handle.records, intent=handle.command, evidence_kind=evidence_kind,
            request_id=handle.id, operation_id=handle.operation_id, transmitted=handle.transmitted,
            interruption=handle.interruption, provenance={"host_epoch": handle.epoch,
                "firmware_epoch": None, "wire_target_proven": False})

    def diagnostics(self):
        """Bounded cached host framing facts; no I/O or firmware-epoch inference."""
        return dict(host_epoch=self.host_epoch, firmware_epoch=None, synchronized=self.synchronized,
                    received_bytes=self.received, raw_dropped=self.raw_dropped,
                    partial_line_hex=bytes(self.buffer).hex(), parse_failures=list(self.parse_failures))


class EvidenceLog:
    """Exclusive JSONL transcript, at most 2048 events/2 MiB including a limit notice.

    Raw lines are hex encoded, preserving bytes without terminal escape sequences.
    Exhaustion fails closed; the caller stops issuing work and retains uncertainty.
    No environment, port enumeration or unrelated files are captured.
    """
    def __init__(self, path):
        self.file = Path(path).open("x", encoding="ascii", newline="\n")
        self.events = self.bytes = 0
        self.exhausted = False

    def __call__(self, event, **fields):
        if self.exhausted:
            raise BenchError("evidence log capacity exhausted")
        line = json.dumps(dict(event=event, **fields), ensure_ascii=True,
                          allow_nan=False, separators=(",", ":")) + "\n"
        if len(line) > 16384 or self.events >= 2047 or self.bytes + len(line) > 2 * 1024 * 1024 - 128:
            self.exhausted = True
            self.file.write('{"event":"evidence_limit","status":"interrupted"}\n')
            self.file.flush()
            raise BenchError("evidence log capacity exhausted")
        self.file.write(line)
        self.file.flush()
        self.events += 1
        self.bytes += len(line)

    def close(self):
        """Close the host file; sends no console/device command."""
        self.file.close()


def _seconds(text):
    try:
        return positive(float(text), "time")
    except ValueError as error:
        raise argparse.ArgumentTypeError(str(error)) from error


def _integer(low, high):
    def parse(text):
        if not re.fullmatch(r"0|[1-9][0-9]*", text) or not low <= int(text) <= high:
            raise argparse.ArgumentTypeError(f"expected decimal integer {low}..{high}")
        return int(text)
    return parse


def parser():
    """CAN spellings: offline modes plus one finite typed read; no campaign options."""
    result = argparse.ArgumentParser(description=__doc__)
    modes = result.add_mutually_exclusive_group()
    modes.add_argument("--plan-only", action="store_true", help="print read-only plan; no serial import or files")
    modes.add_argument("--self-test", action="store_true", help="exercise the client using a finite scripted fake")
    result.add_argument("--scenario", choices=("quick", "probe", "identity", "config", "state"), default="quick")
    result.add_argument("--state-block", choices=("status", "monitors", "feedback"), default="status")
    result.add_argument("--port", help="explicit serial-console port; never a CAN interface")
    result.add_argument("--baud", type=_integer(1, 4000000), help="explicit serial-console baud")
    result.add_argument("--profile", choices=(PROFILE,), help="explicit compiled profile selection")
    result.add_argument("--node", type=_integer(1, 127), help="existing CANopen binding endpoint; no device reconfiguration")
    result.add_argument("--can-bitrate", type=_integer(1, 1000000), help="expected cached CAN bitrate, bits/s; never changed")
    result.add_argument("--expected-library-version", help="exact reported library version, e.g. 0.1.0")
    result.add_argument("--log", type=Path, help="new exclusive JSONL file; sibling .summary.json must also be new")
    result.add_argument("--timeout", type=_seconds, default=5.0, help="immutable per-command host deadline, seconds (0,60]")
    result.add_argument("--startup", type=_seconds, default=0.2, help="bounded initial banner collection, seconds (0,60]")
    result.add_argument("--request-timeout-ms", type=_integer(1, 60000), default=1000,
                        help="explicit application read budget, ms; not a vendor timing guarantee")
    result.add_argument("--release", action="store_true", help="explicitly release a checked terminal; default retains it")
    return result


def invocation(args):
    """Pure plan construction. Quick means one probe whose DEC-1 read effect is unknown."""
    command = (f"probe {args.request_timeout_ms}" if args.scenario in ("quick", "probe") else
               f"read state {args.state_block} {args.request_timeout_ms}" if args.scenario == "state" else
               f"read {args.scenario} {args.request_timeout_ms}")
    return dict(evidence_kind="planned", scenario=args.scenario, intent=command,
                profile=args.profile, node=args.node, console_port=args.port,
                serial_baud=args.baud, can_bitrate=args.can_bitrate,
                expected_library_version=args.expected_library_version,
                timeout_s=args.timeout, firmware_epoch=None,
                read_effect="unknown; DEC-1 accepted list/Q20 qualification applies",
                commands=["version", "profile list", "caps", "config", "useaddr NODE",
                          "config", command] + (["release OPERATION_ID"] if args.release else []),
                model_match=None, firmware_match=None, hardware_qualified=False)


def _self_test():
    """Small scripted host fixture; no driver, physical traffic or live qualification.

    Independent hostile vectors and the native C++ producer are registered tests.
    This entry point only demonstrates split framing, handshake and exact retention.
    """
    def record(request_id, kind, command, **fields):
        return (json.dumps(dict(schema=SCHEMA, protocol=3, profile=PROFILE, type=kind,
                    id=request_id, command=command, **fields), separators=(",", ":")) + "\n").encode("ascii")

    class Stream:
        def __init__(self):
            self.data = bytearray()
            self.writes = 0

        def write(self, data):
            self.writes += 1
            prefix, command = data.decode("ascii").strip().split(" ", 1)
            identity, family = int(prefix[1:]), command.split()[0]
            fields = dict(ok=True, status="OK")
            if command == "version":
                fields.update(product="MotorControl-CAN", version="0.1.0", operation_capacity=2)
            elif command == "profile list":
                fields.update(profiles=[dict(name=PROFILE, protocol="canopen", model_match=None,
                              firmware_match=None, hardware_qualified=False)])
            elif command == "caps":
                for name in ("version", "profile", "caps", "config", "useaddr", "release", "probe", "read"):
                    asynchronous = name in ("probe", "read")
                    self.data.extend(record(identity, "capability", name, implemented=True, owner=43,
                        effect="device_read" if asynchronous else "host_configuration" if name in ("useaddr", "release") else "cache",
                        completion="asynchronous" if asynchronous else "reply", reason="host fixture"))
                fields.update(result="complete", endpoint_range=[1, 127], probe=True, identity=True,
                              can_match_exact_model=False, can_match_firmware=False, hardware_qualified=False,
                              model_match=None, firmware_match=None)
            elif command == "config":
                fields.update(interface=0, bitrate_bits_per_second=500000, selected_node=None, bus_traffic=False)
            elif command == "probe 1000":
                fields.update(result="accepted", operation_id=1)
            else:
                raise BenchError("self-test issued an unexpected command")
            self.data.extend(record(identity, "reply", family, **fields))
            if command == "probe 1000":
                self.data.extend(record(identity, "value", "probe", operation_id=1, index=4096, subindex=0,
                    width=4, raw="4294967295", native_signed=None, unit="raw-code", current=True, interpretation=None,
                    earliest_us="9007199254740993", latest_us="9007199254740993"))
                self.data.extend(record(identity, "terminal", "probe", ok=True, status="OK", detail=0,
                    outcome="SUCCESS", uncertain=False, command_id=identity, operation_id=1,
                    completed_us="9007199254740993", abort_code=None))
            return len(data)

        def read(self, size):
            count = min(size, 7)
            data = bytes(self.data[:count])
            del self.data[:count]
            return data

    stream = Stream()
    console = Console(stream, first_id=1, host_epoch="self-test")
    console.identify(expected_version="0.1.0")
    handle = console.begin("probe 1000")
    console.wait(handle)
    summary = console.summary(handle)
    if summary["records"][-1]["completed_us"] != "9007199254740993" or stream.writes != 5:
        raise BenchError("self-test exact retention/no-replay check failed")
    return dict(evidence_kind="host-tested", fixture="scripted byte stream", operation=summary,
                hardware_qualified=False, firmware_epoch=None)


def run_live(args):
    """One validated invocation; optional pyserial3.5 is loaded only here.

    No close/recovery path sends cleanup traffic. Failure preserves the attempted
    command, native terminal and stream damage separately. Existing files are
    never overwritten. A terminal failure returns 1; input errors return 2.
    """
    log = EvidenceLog(args.log)
    port = console = handle = None
    summary_path = args.log.with_name(args.log.name + ".summary.json")
    try:
        with summary_path.open("x", encoding="ascii", newline="\n") as output:
            try:
                log("invocation", plan=invocation(args))
                import serial  # Optional dependency; never imported by library/offline paths.
                if getattr(serial, "VERSION", None) != "3.5":
                    raise BenchError("live console requires pyserial==3.5")
                port = serial.Serial(port=None, baudrate=args.baud, timeout=0,
                                     write_timeout=min(args.timeout, 2.0))
                port.dtr = port.rts = False
                port.port = args.port
                port.open()
                console = Console(port, on_event=log)
                console.drain_startup(args.startup)
                console.identify(expected_version=args.expected_library_version,
                                 profile=args.profile, timeout_s=args.timeout)
                console.check_config(console.handshake["config"], can_bitrate=args.can_bitrate)
                selected = console.command(f"useaddr {args.node}", timeout_s=args.timeout)
                if not selected["ok"]:
                    raise BenchError("existing host binding selection refused; no probe sent")
                console.check_config(console.command("config", timeout_s=args.timeout),
                                     node=args.node, can_bitrate=args.can_bitrate)
                handle = console.begin(invocation(args)["intent"], timeout_s=args.timeout)
                terminal = console.wait(handle)
                if args.release and handle.accepted:
                    console.release(handle, timeout_s=args.timeout)
                summary = dict(evidence_kind="live-observed", operation=console.summary(handle, evidence_kind="live-observed"),
                               hardware_qualified=False, firmware_epoch=None)
                success = terminal["ok"] and terminal.get("uncertain") is not True
            except (BenchError, OSError, ImportError, ValueError) as error:
                if console is not None and handle is None and console.pending:
                    handle = list(console.pending.values())[-1]
                operation = console.summary(handle, evidence_kind="live-observed") if handle else None
                summary = dict(evidence_kind="live-observed" if operation else "skipped", error=str(error),
                               operation=operation, hardware_qualified=False, firmware_epoch=None)
                success = False
            summary["plan"] = invocation(args)
            summary["client"] = console.diagnostics() if console else None
            summary["identity"] = console.handshake if console else None
            output.write(json.dumps(summary, ensure_ascii=True, allow_nan=False, indent=2) + "\n")
            output.flush()
            print(("completed" if success else "failed") + ": " +
                  (summary.get("operation") or {}).get("classification", "no admission") +
                  f"; evidence {args.log}; summary {summary_path}")
            return 0 if success else 1
    finally:
        if port is not None:
            port.close()
        log.close()


def main(argv=None):
    """Exit 0 for a valid plan/host test or successful read; 1 execution; 2 arguments/setup."""
    arguments = parser()
    args = arguments.parse_args(argv)
    if args.expected_library_version is not None and not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.expected_library_version):
        arguments.error("--expected-library-version requires an exact major.minor.patch")
    if args.port is not None and (not args.port or any(ord(c) < 32 for c in args.port)):
        arguments.error("--port must be an explicit nonempty serial port")
    if args.plan_only:
        print(json.dumps(invocation(args), ensure_ascii=True, allow_nan=False, indent=2))
        return 0
    if args.self_test:
        try:
            print(json.dumps(_self_test(), ensure_ascii=True, allow_nan=False, indent=2))
            return 0
        except (BenchError, ValueError) as error:
            print(str(error), file=sys.stderr)
            return 1
    for name in ("port", "baud", "profile", "node", "can_bitrate", "expected_library_version", "log"):
        if getattr(args, name) is None:
            arguments.error("live use requires explicit --" + name.replace("_", "-"))
    if args.log.exists() or args.log.with_name(args.log.name + ".summary.json").exists() or not args.log.parent.is_dir():
        arguments.error("--log and its .summary.json must be new files in an existing directory")
    try:
        return run_live(args)
    except (BenchError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
