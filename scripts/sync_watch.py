#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""One finite SYNC schedule plus compact raw RX watch through the existing S2 owner.

No CAN receiver, PDO configuration, NMT, enable, motion, recovery or retry is added.
The caller first establishes the released fixture, both TPDO1 type1 configurations,
Operational state and exclusive producer. Software times are never wire times or
qualified same-cycle identities. Raw SYNC TX is represented by retained aggregates.
"""
from __future__ import annotations
import argparse
import binascii
from collections import deque
import json
import hashlib
import os
from pathlib import Path
import re
import time

from console_jsonl import record_problems
import s2_bench_console as bench

U32, U64 = 2**32 - 1, 2**64 - 1
MAX_BYTES, MAX_LINE = 64 << 20, 2048
GAP_LIMITS = (5000, 9000, 9500, 10000, 10500, 11000, 20000)
TPDO_IDS = (0x181, 0x182)
TPDO_DLC = 7  # Explicit fixture TPDO1 map: 6041/16, 6061/8, 6064/32 bits.


def uint(text, limit=U64, base=10):
    pattern = r"0|[1-9][0-9]*" if base == 10 else r"0|[1-9a-f][0-9a-f]*"
    if type(text) is not str or re.fullmatch(pattern, text) is None:
        raise ValueError("noncanonical unsigned integer")
    value = int(text, base)
    if value > limit:
        raise ValueError("integer exceeds its declared width")
    return value


def decode_compact(line, origin):
    """Validate one PC ASCII record and reconstruct independent full64 RX bounds.

    Reversed bounds remain raw invalid-time evidence. CRC-16/CCITT-FALSE covers
    the exact bytes before the last comma; timestamps do not depend on prior rows.
    """
    if type(origin) is not int or not 0 <= origin <= U64 or not line.startswith("PC "):
        raise ValueError("compact frame needs its exact BEGIN origin")
    body, separator, checksum = line.rpartition(",")
    if not separator or re.fullmatch(r"[0-9a-f]{4}", checksum) is None:
        raise ValueError("compact checksum shape")
    if binascii.crc_hqx(body.encode("ascii"), 0xffff) != int(checksum, 16):
        raise ValueError("compact checksum mismatch")
    parts = body[3:].split(",")
    if len(parts) != 6:
        raise ValueError("compact frame field count")
    sequence = uint(parts[0], U32, 16)
    identifier = uint(parts[1], 0x1fffffff, 16)
    offset, width = uint(parts[2], U64, 16), uint(parts[3], U64, 16)
    meta = uint(parts[4], 0x3ff, 16)
    dlc, flags = meta >> 6, meta & 63
    ext, rtr, timed, exact, before, reversed_bounds = (bool(flags & (1 << bit)) for bit in range(6))
    if dlc > 8 or not ext and identifier > 0x7ff:
        raise ValueError("compact identifier or Classical DLC")
    data = parts[5]
    if (rtr or not dlc) and data != "-" or not rtr and dlc and re.fullmatch(r"[0-9a-f]{%d}" % (2 * dlc), data) is None:
        raise ValueError("compact payload differs from DLC/RTR")
    if (not timed and (exact or before or reversed_bounds or offset or width) or
            exact and (width or reversed_bounds) or before and not offset or reversed_bounds and not width):
        raise ValueError("compact time flags contradict bounds")
    hi = origin - offset if before else origin + offset
    lo = hi + width if reversed_bounds else hi - width
    if timed and not (0 <= lo <= U64 and 0 <= hi <= U64):
        raise ValueError("compact reconstructed time overflows")
    return dict(n=sequence, id=identifier, ext=ext, rtr=rtr, dlc=dlc, data=data,
                timed=timed, exact=exact, lo=lo if timed else None, hi=hi if timed else None,
                reversed_bounds=reversed_bounds)


def attributes(line):
    values = {}
    for token in line.split()[1:]:
        key, sep, value = token.partition("=")
        if not sep or not key or key in values:
            raise ValueError("invalid/duplicate watch field")
        values[key] = value
    return values


class Analysis:
    """Bounded raw-ID metrics and separate fixture TPDO metrics; raw bytes stream to disk.

    Only standard data frames with the explicit seven-byte fixture map count
    toward expected TPDO coverage. Raw-ID timing and accounting retain every frame.
    """
    def __init__(self, period_us=10000):
        self.period_us = period_us
        self.begin = self.end = None
        self.next_sequence = 0
        self.frames = self.invalid_time = self.raw_payload_bytes = 0
        self.ids, self.native_ids = {}, {}
        self.tpdos = {}
        self.rejected_tpdos = 0
        self.last_summary = None
        self.stopping = False

    def add_frame(self, stats, frame):
        """Update one bounded metric set; return false for invalid receive timing."""
        stats["count"] += 1
        if stats["data"] is not None and stats["data"] != frame["data"]:
            stats["payload_changes"] += 1
        stats["data"] = frame["data"]
        if not frame["timed"] or frame["reversed_bounds"]:
            return False
        if stats["last"] is not None:
            previous = stats["last"]
            gap = frame["hi"] - previous["hi"]
            if gap < 0:
                return False
            lower = max(0, frame["lo"] - previous["hi"])
            upper = frame["hi"] - previous["lo"]
            stats["min_hi_gap"] = gap if stats["min_hi_gap"] is None else min(stats["min_hi_gap"], gap)
            stats["max_hi_gap"] = gap if stats["max_hi_gap"] is None else max(stats["max_hi_gap"], gap)
            stats["min_interval_lower"] = lower if stats["min_interval_lower"] is None else min(stats["min_interval_lower"], lower)
            stats["max_interval_upper"] = upper if stats["max_interval_upper"] is None else max(stats["max_interval_upper"], upper)
            stats["sum_hi_gap"] += gap
            stats["gaps"] += 1
            stats["histogram"][sum(gap > edge for edge in GAP_LIMITS)] += 1
            stats["lower_over_period"] += lower > self.period_us
            stats["upper_below_period"] += upper < self.period_us
        stats["last"] = frame
        return True

    @staticmethod
    def new_stats(frame):
        return dict(id=frame["id"], ext=frame["ext"], count=0, last=None,
                    min_hi_gap=None, max_hi_gap=None, sum_hi_gap=0, gaps=0, histogram=[0] * 8,
                    lower_over_period=0, upper_below_period=0, payload_changes=0, data=None,
                    min_interval_lower=None, max_interval_upper=None)

    def accept(self, line):
        if line.startswith("PC "):
            if self.begin is None or self.end is not None:
                raise ValueError("compact frame outside active watch")
            frame = decode_compact(line, uint(self.begin["origin"]))
            self.sequence(frame["n"])
            self.frames += 1
            self.raw_payload_bytes += 0 if frame["data"] == "-" else len(frame["data"]) // 2
            if frame["id"] in TPDO_IDS:
                if frame["ext"] or frame["rtr"] or frame["dlc"] != TPDO_DLC:
                    self.rejected_tpdos += 1
                else:
                    if frame["id"] not in self.tpdos:
                        self.tpdos[frame["id"]] = self.new_stats(frame)
                    self.add_frame(self.tpdos[frame["id"]], frame)
            key = str(frame["id"]) + ("x" if frame["ext"] else "s")
            if key not in self.ids:
                if len(self.ids) == 16:
                    return
                self.ids[key] = self.new_stats(frame)
            if not self.add_frame(self.ids[key], frame):
                self.invalid_time += 1
            return
        # This local P0 receipt has no watch sequence or timestamp fields.
        # Only its exact spelling is accepted; summaries still require BEGIN/END.
        if line == "PDOW_STOPPING; final summaries follow":
            if self.begin is None or self.end is not None or self.stopping:
                raise ValueError("unexpected/repeated watch stopping receipt")
            self.stopping = True
            return
        if not line.startswith("PDOW_"):
            return
        kind = line.split()[0]
        if kind not in ("PDOW_BEGIN", "PDOW_SUM", "PDOW_ID", "PDOW_END"):
            raise ValueError("unexpected verbose/unknown watch record in compact capture")
        values = attributes(line)
        if not {"n", "t"}.issubset(values):
            raise ValueError("watch record lacks sequence/time")
        sequence = uint(values["n"], U32)
        self.sequence(sequence)
        uint(values["t"])
        if kind == "PDOW_BEGIN":
            if self.begin is not None or values.get("compact") != "1" or sequence:
                raise ValueError("watch must have one explicit compact BEGIN")
            uint(values["origin"])
            self.begin = values
        elif self.begin is None or self.end is not None:
            raise ValueError("watch summary outside active watch")
        elif kind in ("PDOW_SUM", "PDOW_END"):
            for key in ("frames", "printed", "unprinted", "tapDropped", "untimed", "sdo", "idOverflow"):
                uint(values[key], U32)
            if kind == "PDOW_END":
                if values.get("reason") not in ("stop", "duration"):
                    raise ValueError("unknown watch terminal reason")
                self.end = values
            self.last_summary = values
        elif kind == "PDOW_ID":
            key = str(uint(values["id"], 0x1fffffff, 16)) + ("x" if uint(values["ext"], 1) else "s")
            if key not in self.native_ids and len(self.native_ids) == 16:
                raise ValueError("watch identifier capacity exceeded")
            self.native_ids[key] = values
        else:
            raise ValueError("unexpected verbose/unknown watch record in compact capture")

    def sequence(self, value):
        if value != self.next_sequence:
            raise ValueError("watch console sequence gap/repetition")
        self.next_sequence += 1

    def summary(self, expected_count=None):
        raw_errors, timing_errors, coverage_errors = [], [], []
        if self.begin is None or self.end is None:
            raw_errors.append("watch has no complete BEGIN/END")
        if self.end is not None:
            for name in ("unprinted", "tapDropped", "sdo"):
                if uint(self.end[name], U32):
                    raw_errors.append("watch " + name + " is nonzero")
            if uint(self.end["untimed"], U32):
                timing_errors.append("watch untimed is nonzero")
            if uint(self.end["idOverflow"], U32):
                coverage_errors.append("watch per-ID metrics overflowed")
            if uint(self.end["frames"]) != uint(self.end["printed"]) + uint(self.end["unprinted"]):
                raw_errors.append("native frame accounting differs")
            if self.frames != uint(self.end["printed"]):
                raw_errors.append("host compact frame count differs from native printed count")
        if self.invalid_time:
            timing_errors.append("invalid/untimed receive bounds")
        ids = {}
        for key, stats in self.ids.items():
            ids[key] = self.stats_summary(stats)
            native = self.native_ids.get(key)
            if self.end is not None and (native is None or uint(native["count"], U32) != stats["count"]):
                coverage_errors.append("host/native per-ID count mismatch: " + key)
        if self.rejected_tpdos:
            coverage_errors.append("target TPDO identifiers include extended, RTR or wrong-DLC frames")
        for identifier in TPDO_IDS:
            count = self.tpdos.get(identifier, {}).get("count", 0)
            if expected_count is not None and count != expected_count:
                coverage_errors.append("TPDO %03x count %d differs from expected %d; no cycle identity inferred" % (identifier, count, expected_count))
        return dict(begin=self.begin, end=self.end, raw_rx_complete=not raw_errors,
                    observed_bounds_valid=not timing_errors,
                    timing_complete=not raw_errors and not timing_errors,
                    errors=raw_errors + timing_errors + coverage_errors,
                    raw_errors=raw_errors, timing_errors=timing_errors, coverage_errors=coverage_errors,
                    received_frames=self.frames, invalid_time=self.invalid_time, ids=ids,
                    tpdos={str(identifier): self.stats_summary(stats) for identifier, stats in self.tpdos.items()},
                    rejected_tpdo_frames=self.rejected_tpdos, expected_tpdo_dlc=TPDO_DLC,
                    native_ids=self.native_ids, histogram_upper_limits_us=list(GAP_LIMITS),
                    rx_payload_bits=self.raw_payload_bytes * 8, tx_raw_per_cycle=False, wire_bus_load_measured=False,
                    time_scope="driver receive bounds and upper-bound gaps; not wire time or same-cycle identity")

    @staticmethod
    def stats_summary(stats):
        result = {name: value for name, value in stats.items() if name not in ("last", "data")}
        result["mean_hi_gap"] = stats["sum_hi_gap"] // stats["gaps"] if stats["gaps"] else None
        return result


class Stream:
    """One bounded serial byte owner; every received byte reaches the raw artifact first."""
    def __init__(self, port, raw, analysis, clock=time.monotonic, sleeper=time.sleep):
        self.port, self.raw, self.analysis, self.clock, self.sleep = port, raw, analysis, clock, sleeper
        self.buffer, self.lines = bytearray(), deque()
        self.bytes = 0
        self.healthy = True
        self.console_records = {}
        self.handoffs = []

    def read(self):
        try:
            return self._read()
        except BaseException:
            self.healthy = False
            raise

    def _read(self):
        data = self.port.read(4096)
        if type(data) is not bytes or len(data) > 4096:
            raise ValueError("serial read violated capacity")
        if data:
            if self.bytes + len(data) > MAX_BYTES:
                raise ValueError("finite raw capture byte cap exceeded")
            if self.raw.write(data) != len(data):
                raise OSError("short raw evidence write")
            self.raw.flush()
            self.bytes += len(data)
            for byte in data:
                if byte == 10:
                    line = self.buffer.rstrip(b"\r").decode("ascii")
                    self.buffer.clear()
                    self.analysis.accept(line)
                    row = json_record(line)
                    if row is not None and row["id"]:
                        key = row["id"], row["type"]
                        if key in self.console_records or len(self.console_records) == 32:
                            raise ValueError("duplicate/stale console record or record capacity exceeded")
                        self.console_records[key] = row
                    if len(self.lines) == 512:
                        raise ValueError("host line queue capacity exceeded")
                    self.lines.append(line)
                else:
                    self.buffer.append(byte)
                    if len(self.buffer) > MAX_LINE:
                        raise ValueError("unterminated/oversize firmware line")
        return bool(data)

    def until(self, predicate, deadline):
        for _ in range(4000000):
            while self.lines:
                line = self.lines.popleft()
                value = predicate(line)
                if value is not None:
                    return value
            if self.clock() >= deadline:
                raise TimeoutError("immutable host capture deadline expired")
            if not self.read():
                self.sleep(min(0.001, max(0, deadline - self.clock())))
        raise ValueError("finite host service turn budget exhausted")

    def send(self, value):
        if not self.healthy:
            raise ValueError("damaged stream forbids further handoffs")
        if len(self.handoffs) == 512:
            raise ValueError("finite command handoff capacity exceeded")
        evidence = dict(host_s=self.clock(), wire_hex=value.hex(), accepted_bytes=None)
        self.handoffs.append(evidence)
        try:
            written = self.port.write(value)
            evidence["accepted_bytes"] = written
        except BaseException:
            self.healthy = False
            raise
        if type(written) is not int or written != len(value):
            self.healthy = False
            raise OSError("short command handoff; no replay")


def json_record(line):
    if not line.startswith("{"):
        return None
    value, problems = record_problems((line + "\n").encode("ascii"))
    if problems:
        raise ValueError("invalid console record: " + "; ".join(problems))
    return value


def local_tx_complete(terminal, count):
    """Native LOCAL_TX success preserves unknown remote effects, even on success."""
    return (type(count) is int and 1 <= count <= U32 and
            terminal.get("ok") is True and terminal.get("status") == "OK" and
            terminal.get("outcome") == "LOCAL_TX" and terminal.get("abort_code") is None and
            # ProbeConsole projects transmissionPossible into uncertainty. A
            # nonempty fully confirmed schedule cannot establish drive effect.
            terminal.get("uncertain") is True and terminal.get("transmission_possible") is True and
            terminal.get("drive_effect_known") is False and
            all(type(terminal.get(key)) is int and terminal[key] == count
                for key in ("local_tx_count", "requested_count")))


def run(stream, *, duration_s, period_ms, expected_elf, first_id=540000, poll_s=10):
    """Run once on a prepared fixture. All failure paths preserve the native finite schedule."""
    count = duration_s * 1000 // period_ms
    started = stream.clock()
    outer = started + duration_s + 30
    next_id = first_id
    snapshots = []
    def command(text, timeout=5):
        nonlocal next_id
        identity = next_id
        next_id += 1
        stream.send(("@%d %s\n" % (identity, text)).encode())
        def answer(line):
            row = json_record(line)
            return row if row and row["id"] == identity and row["type"] == "reply" else None
        result = stream.until(answer, min(outer, stream.clock() + timeout))
        if result.get("ok") is not True or result.get("status") != "OK" or result["command"] != text.split()[0]:
            raise ValueError("console refused " + text)
        return identity, result
    stream.send(b"p")
    pong = stream.until(lambda line: line if line.startswith("PONG ") else None, min(outer, stream.clock() + 5))
    facts = attributes(pong)
    if (facts.get("elf") != expected_elf or uint(facts["pdo"]) < 7 or
            any(facts.get(name) != value for name, value in (("active", "1"), ("running", "1"),
                ("soak", "0"), ("trial", "0"), ("watch", "0"), ("capture", "0")))):
        raise ValueError("image or idle prepared fixture does not match explicit request")
    stream.send(b"o")
    stream.until(lambda line: True if line.startswith("CONSOLE open ") else None, min(outer, stream.clock() + 5))
    command("host 0")
    command("debug off")
    _, inspection = command("sync inspect")
    if any(inspection.get(name) is not False for name in ("armed", "producer_conflict", "retained", "active")):
        raise ValueError("SYNC must be independently idle/disarmed without producer conflict")
    stream.send(b"s")
    snapshots.append(stream.until(lambda line: line if line.startswith("STATS ") else None, min(outer, stream.clock() + 5)))
    stream.send(("P%d,c\n" % (duration_s + 10)).encode())
    stream.until(lambda line: True if line.startswith("PDOW admitted ") else None, min(outer, stream.clock() + 5))
    stream.until(lambda line: True if line.startswith("PDOW_BEGIN ") else None, min(outer, stream.clock() + 5))
    command("sync arm")
    identity, receipt = command("sync period %d %d" % (period_ms, count))
    operation = receipt.get("operation_id")
    if receipt.get("result") != "accepted" or type(operation) is not int or not 1 <= operation <= U32:
        raise ValueError("finite SYNC schedule lacks exact admission")
    terminal = None
    next_poll = stream.clock() + poll_s
    while terminal is None:
        def done(line):
            row = json_record(line)
            return row if row and row["id"] == identity and row["type"] == "terminal" else None
        try:
            terminal = stream.console_records.get((identity, "terminal"))
            if terminal is None:
                terminal = stream.until(done, min(outer, next_poll))
        except TimeoutError:
            if stream.clock() >= outer:
                raise
            if len(snapshots) == 400:
                raise ValueError("snapshot capacity exceeded")
            stream.send(b"s")
            snapshots.append(stream.until(lambda line: line if line.startswith("STATS ") else None, min(outer, stream.clock() + 5)))
            next_poll += poll_s
    if terminal.get("operation_id") != operation or terminal.get("command_id") != identity:
        raise ValueError("finite SYNC terminal differs from admitted identity")
    native_ok = local_tx_complete(terminal, count)
    # A checked failure still permits these explicitly planned cached/local steps.
    # Failed native results stay retained; none of these commands retries production.
    _, timing = command("sync timing")
    if native_ok:
        command("release %d" % operation)
    command("sync disarm")
    stream.send(b"P0\n")
    stream.until(lambda line: True if line.startswith("PDOW_END ") else None, outer)
    stream.send(b"s")
    snapshots.append(stream.until(lambda line: line if line.startswith("STATS ") else None, outer))
    result = dict(local_tx_complete=native_ok, firmware_effect_qualified=False,
                  pong=pong, receipt=receipt, terminal=terminal, sync_timing=timing,
                  snapshots=snapshots, elapsed_s=stream.clock() - started, expected_count=count)
    result["watch"] = stream.analysis.summary(count)
    timing_ok = all(type(timing.get(name)) is int and timing[name] == count for name in ("count", "terminals", "confirmed", "handoffs"))
    timing_ok = timing_ok and timing.get("invalid") is False and uint(timing["period_us"]) == period_ms * 1000
    for name, size in (("handoff_us", 4), ("delay_us", 2)):
        if type(timing.get(name)) is not list or len(timing[name]) != size:
            raise ValueError("SYNC timing lacks bounded aggregate fields")
        for value in timing[name]:
            uint(value)
    counters = [attributes(line) for line in snapshots]
    counter_errors = []
    for name in ("dropped", "errors", "txRej", "rxLost"):
        values = [uint(row[name], U32) for row in counters]
        if any(value == U32 or value != values[0] for value in values):
            counter_errors.append("controller/owner " + name + " changed or saturated")
    if any(row.get("running") != "1" or row.get("inhibited") != "0" for row in counters):
        counter_errors.append("controller stopped or inhibited during capture")
    result["counter_errors"] = counter_errors
    result["ok"] = native_ok and not result["watch"]["errors"] and timing_ok and not counter_errors
    if not native_ok:
        result["error"] = "finite SYNC native terminal failed/incomplete; result retained"
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration-s", type=int, default=1800)
    parser.add_argument("--period-ms", type=int, default=10)
    parser.add_argument("--expected-elf")
    parser.add_argument("--port", default=bench.FIXTURE["port"])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)
    if not 1 <= args.duration_s <= 3590 or not 10 <= args.period_ms <= 1000 or args.duration_s * 1000 % args.period_ms:
        parser.error("duration1..3590, period10..1000 and an exact integer slot count required")
    plan = dict(duration_s=args.duration_s, period_ms=args.period_ms, count=args.duration_s * 1000 // args.period_ms,
                watch_s=args.duration_s + 10, byte_cap=MAX_BYTES, raw_tx_per_cycle=False,
                automatic_retry=False, configuration_changed=False, firmware_effect_qualified=False,
                planned_cleanup="cached sync timing/STATS, local sync disarm and P0; failed result retained")
    if args.dry_run:
        print(json.dumps(plan, indent=2))
        return 0
    if args.output is None or args.expected_elf is None or re.fullmatch(r"[0-9a-f]{64}", args.expected_elf) is None:
        parser.error("new output directory and exact expected ELF SHA256 required")
    args.output.mkdir(parents=False, exist_ok=False)
    plan.update(expected_elf=args.expected_elf, port=args.port, fixture=bench.FIXTURE,
                tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    (args.output / "request.json").write_text(json.dumps(plan, indent=2) + "\n")
    result, port, stream = dict(ok=False, plan=plan), None, None
    analysis = Analysis(args.period_ms * 1000)
    try:
        backend = bench.RealBackend()
        bench.check_identity(args.port, backend.comports())
        port = backend.open(args.port)
        with (args.output / "rx.bin").open("xb") as raw:
            stream = Stream(port, raw, analysis)
            try:
                result.update(run(stream, duration_s=args.duration_s, period_ms=args.period_ms, expected_elf=args.expected_elf))
            finally:
                raw.flush()
                os.fsync(raw.fileno())
    except (Exception, KeyboardInterrupt) as error:
        result.update(ok=False, error=str(error) or type(error).__name__, watch=analysis.summary())
    finally:
        if stream is not None:
            result.update(handoffs=stream.handoffs, console_records=list(stream.console_records.values()),
                          raw_bytes=stream.bytes, partial_line_hex=stream.buffer.hex(), stream_healthy=stream.healthy)
        if port is not None:
            try:
                port.close()
            except Exception as error:
                result.update(ok=False, close_error=str(error))
        (args.output / "summary.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps(dict(ok=result["ok"], output=str(args.output))))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
