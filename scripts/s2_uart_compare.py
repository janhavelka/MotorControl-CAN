#!/usr/bin/env python3
"""Compare an independent capture of the S2 console UART with the host's rx.bin, offline.

The host capture (rx.bin of scripts/s2_soak.py or s2_bench.py) holds what reached the host
through the ESP-Prog-2 bridge, USB and the host driver. An independent receiver on the S2's
UART0 TX line records what the S2 put on the wire. Bytes on the wire but absent from rx.bin
were lost after the tap; damage present in both captures happened before it. This tool opens
no port, starts no capture and never modifies or writes into its inputs.

Connection for the independent capture (receive only):
  signal    S2 UART0 TX, GPIO43: the line the ESP-Prog-2 bridge receives. 3.3 V logic.
  ground    receiver GND to S2 GND.
  line      115200 baud, 8 data bits, no parity, 1 stop bit, no flow control (8N1).
  receiver  preferably a logic analyzer sampling at 1 MS/s or more that decodes 8N1 and
            reports framing, parity and break errors. A second 3.3 V USB-UART on the same
            host shows whether bytes left the S2, but it shares the host's USB stack and
            drivers, so it cannot separate a host-side cause from a bridge one. Leave the
            receiver's TX unconnected; never drive GPIO43 or GPIO44 (UART0 RX). The bridge
            stays connected and the host runner keeps its own port as usual.
  span      start the capture before the host opens its port and stop it after the host
            closes it. GPIO44 (host to S2) is not part of either capture.

Capture format (--format raw, the only format so far, applies to both sides):
  bytes     a binary file of the decoded data bytes in wire order and nothing else: no hex
            text, framing or parity markers, analyzer annotations or added line endings.
            Raw bytes cannot carry the receiver's framing, parity or break errors: give
            them in the times CSV's errors column, or the comparison cannot see them.
            A name ending in .xz is read as a lossless xz container of such a file, as
            docs/bench stores large captures (COMPRESSED.json), and decompressed in memory.
  times     optional CSV per side in the shape of rx.times.csv: a header row naming at
            least offset, bytes and the --time-column (monotonic_s or utc); other columns
            such as in_waiting are ignored. One row per read, or per burst between idle
            gaps: offset = file offset of the chunk's first byte, contiguous from 0;
            bytes = chunk length, at least 1; monotonic_s = that recorder's clock in
            seconds once it held the whole chunk; utc = ISO 8601 with a UTC offset;
            errors (optional) = framing, parity or break errors the receiver reported
            within that chunk. A time is an upper bound for when the chunk reached that
            recorder; no lower bound is assumed. At most 4,000,000 rows and 256 MiB.

Clocks. Times on the two sides come from different clocks. They are related only through an
explicit shared --marker (text found exactly once in each stream, inside an identical aligned
part) or stated --offset-bounds LO:HI for host clock minus external clock in seconds (0:0 when
one host clock recorded both). Without either, only same-clock differences are reported. A
marker-referenced delay change includes clock drift since the marker and each side's chunk
resolution; it is never presented as an absolute latency. When the clocks are related, a
missing host tail counts only for the bytes the external capture held before the host's last
chunk; bytes not shown to precede it may have followed the host closing its port.

What is reported, external capture as the reference: the first aligned divergence; deletions,
insertions, duplications (inserted bytes equal to external bytes nearby), reorders (the same
bytes removed at one place and inserted at another), replacements and in-place corruption,
each with byte offsets and lengths on both sides; a missing tail; capture heads outside the
common range; regions that cannot be aligned within the bounds; and, through the actual
s2_evidence parser, the corrupt lines and SOAK/PDOW numbering gaps of each side. A finding
where only the external capture's parse is damaged, or its receiver reported errors, is marked
as possible external capture damage and leaves those host bytes unverified. Where repeated text
or competing alignments let an edit sit at several places, the finding is ambiguous and no
single location is claimed.

Algorithm and bounds. Each input is limited by --max-bytes and --max-lines before it is split
or parsed. Lines that occur exactly once on each side, at least 8 bytes long, are anchors, and
parsed SOAK/PDOW records with the same kind, n= and firmware time are secondary anchors. The
longest chain of anchors in order on both sides fixes the alignment; anchors that equally long
chains place differently are dropped and the findings around them are ambiguous. Only the
windows between consecutive identical anchors are diffed: equal lines are stripped from both
ends, a window is diffed line by line, and each differing block byte by byte. A capture's start
and end are not assumed to coincide: at a stream boundary only an identical line or a common
run of at least 8 bytes aligns the two, and with no anchor and fewer than 32 identical bytes
the comparison is unalignable. A window larger than --window-bytes per side, or than
WINDOW_LINES lines, is split at secondary anchors; otherwise it is reported unaligned. Diffing
stops after --diff-budget bytes in total, and at most MAX_ITEMS findings are kept; the
remainder is reported unverified, never guessed. Measured peak working set and time, for two
inputs of the same size: 17 MB with 118,000 lines each (R1), 3.4 s and about 210 MB; 64 MiB of
soak lines each, 20 s and about 700 MB; 1,000,000 eight-byte lines each, 21 s and 280 MB.

Exit status: 0 identical: every host byte matched; 1 divergent: at least one divergence not
attributed to external capture damage; 2 usage or input error, including oversized input
refused and invalid times; 3 inconclusive: no such divergence, but some host bytes are
unverified; 4 external capture unavailable or empty, with the capture requirements printed.
"""

import argparse
import array
from bisect import bisect_left, bisect_right
from collections import Counter
import csv
from datetime import datetime
import difflib
import hashlib
import json
import lzma
import math
from pathlib import Path
import sys

import s2_evidence as evidence


IDENTICAL, DIVERGENT, INPUT_ERROR, INCONCLUSIVE, UNAVAILABLE = 0, 1, 2, 3, 4
FORMATS = ("raw",)
TIME_COLUMNS = ("monotonic_s", "utc")
SCHEMA = 2

MIB = 1 << 20
DEFAULT_MAX_BYTES = 64 * MIB   # Per input; a 3 h run with 1 s summaries is about 17 MB.
HARD_MAX_BYTES = 256 * MIB
DEFAULT_MAX_LINES = 1_000_000  # Per input; R1's 17 MB hold 118,000 lines.
HARD_MAX_LINES = 2_000_000
MAX_TIMES_BYTES = 256 * MIB    # A times CSV; R1's is 28 MB for 17 MB of capture.
MAX_TIME_ROWS = 4_000_000
DEFAULT_WINDOW_BYTES = 16 << 10
MAX_WINDOW_BYTES = 256 << 10
WINDOW_LINES = 2048            # Per side, for the line-level diff of one window.
DEFAULT_DIFF_BUDGET = 8 * MIB  # Total bytes diffed over all windows.
MAX_ITEMS = 20_000             # Findings kept; analysis stops beyond, the rest is unverified.
LIST_ITEMS = 200               # Findings, corrupt lines and gaps printed in full.
MAX_MARKERS = 16
MIN_ANCHOR = 8                 # Shorter lines are too common to fix an alignment.
MIN_EQUAL = 8                  # Shorter equal runs inside a differing block are coincidence.
MIN_DEFENSIBLE = 32            # Identical bytes needed when no anchor line exists at all.
BYTE_DIFF_MAX = 4096           # Both sides of a block together, for the byte-level split.
CROSSING_PIECES = 16           # Unmatched pieces per side checked for a competing alignment.
REORDER_TRIES = 512            # Equivalent placements compared when testing a reorder.
REPLACEMENT_MAX = 512          # A replacement larger than this on both sides is unaligned.
CORRUPTION_MAX = 8             # Differing bytes for an equal-length change to be corruption.
MIN_SOURCE = 8                 # Shorter inserted runs are not searched for a source.
NEAR_BYTES = 128               # Timing context around a finding, on each side.
PREVIEW_BYTES = 96
PARSE_BLOCK = MIB              # Line-aligned blocks keep the parser's memory bounded.
LF = 0x0A

DIVERGENCES = ("deletion", "insertion", "duplication", "corruption", "replacement", "reorder", "unaligned",
               "host_missing_tail", "tail_divergence")
INTERIOR = ("deletion", "insertion", "duplication", "corruption", "replacement", "reorder", "unaligned")
UNVERIFIED = ("head", "external_ended_first", "unalignable", "not_examined")
MEANING = {
    "deletion": "external bytes the host capture lacks",
    "insertion": "host bytes absent from the external capture and not found near this point in it",
    "duplication": "host bytes absent from the external capture at this point but equal to external "
                   "bytes nearby",
    "corruption": "bytes altered in place: same length, few bytes differ",
    "replacement": "a run of external bytes replaced by a different run in the host capture",
    "reorder": "the same bytes removed at one place and inserted at another; which part moved is not "
               "determined",
    "unaligned": "a bounded region that differs; its inner alignment is not established",
    "host_missing_tail": "the external capture continues after the host capture ends",
    "tail_divergence": "both captures continue past their last common byte with different bytes",
    "head": "bytes before the first common byte: a capture start, not judged as loss",
    "external_ended_first": "the host capture continues after the external capture ends: not verified",
    "external_beyond_host_end": "external bytes not shown to precede the host's last chunk: possibly sent "
                                "after the host closed its port, not judged as loss",
    "unalignable": "no defensible common content to align within the bounds: not verified",
    "not_examined": "beyond the finding limit: not verified",
}
REQUIREMENTS = ("Independent capture needed: S2 UART0 TX GPIO43 and GND to a receive-only 3.3 V input, "
                "preferably a logic analyzer that reports framing and parity errors (a USB-UART on the same "
                "host cannot separate host-side causes), 115200 8N1, no flow control, started before the host "
                "opens its port and stopped after it closes it; saved as raw decoded data bytes (--format raw), "
                "optionally with a times CSV in the rx.times.csv shape. See --help.")


class InputError(Exception):
    """An input cannot be used as given; exit status 2."""
    status = INPUT_ERROR


class Unavailable(InputError):
    """The external capture does not exist or holds no bytes; exit status 4."""
    status = UNAVAILABLE


class _ItemLimit(Exception):
    pass


# ---------------------------------------------------------------- inputs
def _sha256(data):
    return hashlib.sha256(data).hexdigest()


def read_capture(path, limit, max_lines, role):
    """Return (bytes, description). Reads at most limit+1 bytes; never writes."""
    path = Path(path)
    if not path.exists():
        if role == "external":
            raise Unavailable("external capture unavailable: %s does not exist" % path)
        raise InputError("%s capture %s does not exist" % (role, path))
    if not path.is_file():
        raise InputError("%s capture %s is not a file" % (role, path))
    stored = path.stat().st_size
    if stored > limit:
        raise InputError("%s capture %s holds %d bytes; refused above --max-bytes %d" % (role, path, stored, limit))
    with path.open("rb") as stream:
        packed = stream.read(limit + 1)
    if len(packed) > limit:
        raise InputError("%s capture %s grew beyond --max-bytes %d while read; refused" % (role, path, limit))
    info = {"path": str(path)}
    if path.suffix.lower() == ".xz":
        decompressor = lzma.LZMADecompressor(format=lzma.FORMAT_XZ)
        try:
            data = decompressor.decompress(packed, max_length=limit + 1)
        except lzma.LZMAError as error:
            raise InputError("%s capture %s is not a valid xz stream: %s" % (role, path, error)) from None
        if len(data) > limit:
            raise InputError("%s capture %s decompresses beyond --max-bytes %d; refused" % (role, path, limit))
        if not decompressor.eof:
            raise InputError("%s capture %s: xz stream is truncated" % (role, path))
        if decompressor.unused_data:
            raise InputError("%s capture %s: data follows the xz stream" % (role, path))
        info["stored"] = {"container": "xz", "bytes": len(packed), "sha256": _sha256(packed)}
    else:
        data = packed
    if role == "external" and not data:
        raise Unavailable("external capture unavailable: %s holds no bytes; check the GPIO43 and GND "
                          "connection and the receiver at 115200 8N1" % path)
    # Memory follows the line count as much as the byte count; refuse before splitting.
    lines = data.count(b"\n") + (1 if data and not data.endswith(b"\n") else 0)
    if lines > max_lines:
        raise InputError("%s capture %s holds %d lines; refused above --max-lines %d (each line costs about "
                         "150 bytes of memory per side)" % (role, path, lines, max_lines))
    info["bytes"] = len(data)
    info["sha256"] = _sha256(data)
    return data, info


class Times:
    """Chunk observation times of one recorder, in that recorder's own clock."""

    def __init__(self, path, column, offsets, values, covered, decreases, errors):
        self.path, self.column = str(path), column
        self.offsets, self.values = offsets, values
        self.covered, self.decreases = covered, decreases
        self.errors = errors  # [(start, end, count)] of chunks whose receiver reported errors.

    def at(self, offset):
        """(time, seconds since the previous chunk or None) of the chunk holding offset, or None."""
        if offset < 0 or offset >= self.covered:
            return None
        index = bisect_right(self.offsets, offset) - 1
        previous = self.values[index] - self.values[index - 1] if index else None
        return self.values[index], previous

    def chunks(self, start, end):
        """(chunk start, chunk end, time) of the chunks holding [start, end)."""
        start, end = max(0, start), min(self.covered, end)
        if end <= start:
            return
        first = bisect_right(self.offsets, start) - 1
        last = bisect_right(self.offsets, end - 1) - 1
        for index in range(first, last + 1):
            stop = self.offsets[index + 1] if index + 1 < len(self.offsets) else self.covered
            yield self.offsets[index], stop, self.values[index]

    def largest_gap(self, start, end):
        """Largest step between consecutive chunk times over chunks holding [start, end)."""
        start, end = max(0, start), min(self.covered, end)
        if end <= start:
            return None
        first = bisect_right(self.offsets, start) - 1
        last = bisect_right(self.offsets, end - 1) - 1
        steps = [self.values[k] - self.values[k - 1] for k in range(max(first, 1), last + 1)]
        return max(steps) if steps else None

    def errors_in(self, start, end):
        """Receiver errors reported in chunks overlapping [start, end)."""
        return sum(count for low, high, count in self.errors if low < end and start < high)

    def describe(self):
        return {"path": self.path, "column": self.column, "rows": len(self.offsets), "covers_bytes": self.covered,
                "first": _round(self.values[0]) if self.values else None,
                "last": _round(self.values[-1]) if self.values else None, "decreasing_steps": self.decreases,
                "receiver_errors": sum(count for _, _, count in self.errors),
                "chunks_with_errors": len(self.errors)}


def _time_value(text, column):
    if column == "utc":
        moment = datetime.fromisoformat(text)
        if moment.tzinfo is None:
            raise ValueError("utc time without offset")
        return moment.timestamp()
    value = float(text)
    if not math.isfinite(value):
        raise ValueError("not finite")
    return value


def read_times(path, column, data_length, role):
    """Validate a times CSV against its capture, row by row. Raises InputError naming the first bad row."""
    path = Path(path)
    if not path.is_file():
        raise InputError("%s times %s does not exist" % (role, path))
    size = path.stat().st_size
    if size > MAX_TIMES_BYTES:
        raise InputError("%s times %s holds %d bytes; refused above %d" % (role, path, size, MAX_TIMES_BYTES))
    offsets, values, errors = array.array("q"), array.array("d"), []
    expected = decreases = 0
    try:
        with path.open("r", encoding="utf-8", newline="") as stream:
            reader = csv.reader(stream)
            header = next(reader, None)
            if not header:
                raise InputError("%s times %s has no header row" % (role, path))
            missing = [name for name in ("offset", "bytes", column) if name not in header]
            if missing:
                raise InputError("%s times %s lacks column(s) %s" % (role, path, ",".join(missing)))
            at_offset, at_bytes, at_time = header.index("offset"), header.index("bytes"), header.index(column)
            at_errors = header.index("errors") if "errors" in header else None
            for row_number, row in enumerate(reader, 2):
                if not row:
                    continue
                if len(offsets) >= MAX_TIME_ROWS:
                    raise InputError("%s times %s holds more than %d rows; refused (use one row per read or "
                                     "per burst)" % (role, path, MAX_TIME_ROWS))
                if len(row) != len(header):
                    raise InputError("%s times %s row %d has %d fields, header %d" % (
                        role, path, row_number, len(row), len(header)))
                try:
                    offset, count = int(row[at_offset]), int(row[at_bytes])
                    value = _time_value(row[at_time], column)
                    failures = int(row[at_errors]) if at_errors is not None else 0
                except ValueError as error:
                    raise InputError("%s times %s row %d: %s" % (role, path, row_number, error)) from None
                if offset != expected or count < 1 or failures < 0:
                    raise InputError("%s times %s row %d: chunk offset %d length %d errors %d, expected offset %d, "
                                     "length >= 1 and errors >= 0" % (role, path, row_number, offset, count,
                                                                      failures, expected))
                if values and value < values[-1]:
                    decreases += 1
                if failures:
                    errors.append((offset, offset + count, failures))
                offsets.append(offset)
                values.append(value)
                expected += count
    except UnicodeDecodeError:
        raise InputError("%s times %s is not UTF-8 text" % (role, path)) from None
    except csv.Error as error:
        raise InputError("%s times %s: %s" % (role, path, error)) from None
    if expected > data_length:
        raise InputError("%s times %s describe %d bytes; the capture holds %d" % (role, path, expected, data_length))
    return Times(path, column, offsets, values, expected, decreases, errors)


# ---------------------------------------------------------------- one side
def preview(data):
    """Deterministic escaped text of at most PREVIEW_BYTES bytes."""
    text = repr(bytes(data[:PREVIEW_BYTES]))[2:-1]
    return text + ("..." if len(data) > PREVIEW_BYTES else "")


STATUS_NAMES = ("empty", "parsed", "malformed", "unknown")


class Side:
    """One capture split into LF-terminated lines and classified by the evidence parser."""

    def __init__(self, role, data, info, times=None):
        self.role, self.data, self.info, self.times = role, data, info, times
        lines = data.split(b"\n")
        if lines[-1] == b"":
            lines.pop()
        self.lines = lines
        self.unterminated = bool(data) and not data.endswith(b"\n")
        starts = array.array("q")
        position = 0
        for line in lines:
            starts.append(position)
            position += len(line) + 1
        self.starts = starts
        self._parse()

    def __len__(self):
        return len(self.data)

    def offset(self, line):
        """Byte offset of line index, or the capture length at the end."""
        return self.starts[line] if line < len(self.lines) else len(self.data)

    def line_of(self, offset):
        """0-based index of the line holding offset (the last line for the end)."""
        return max(0, bisect_right(self.starts, offset) - 1)

    def key(self, line):
        """Comparable line content; an unterminated final line never equals a terminated one."""
        if self.unterminated and line == len(self.lines) - 1:
            return (self.lines[line],)
        return self.lines[line]

    def _parse(self):
        count = len(self.lines)
        self.status = bytearray(count)  # Index into STATUS_NAMES.
        self.gap_at = bytearray(count)  # 1 where a numbering gap shows at this line.
        self.kind_names, self.kind_index = [None], {None: 0}
        self.kinds = array.array("H", [0]) * count
        self.numbers = array.array("q", [-1]) * count
        self.nkeys = {}                 # (kind, n, time) -> line, or -1 when repeated
        self.corrupt, self.gaps = [], []
        self.counts = Counter()
        last = {}
        data, total, position, base = self.data, len(self.data), 0, 0
        while position < total:
            end = min(total, position + PARSE_BLOCK)
            if end < total:
                cut = data.rfind(b"\n", position, end)
                if cut < 0:
                    cut = data.find(b"\n", end)
                end = total if cut < 0 else cut + 1
            block = data[position:end]
            # The parser's reset-noise rule belongs to a capture's first line only; a leading
            # empty line keeps later blocks from receiving it.
            records, _ = evidence.parse_bytes(block if position == 0 else b"\n" + block)
            shift = 1 if position == 0 else 2
            for record in records:
                self._record(base + record.number - shift, record, last)
            base += block.count(b"\n")
            position = end

    def _record(self, line, record, last):
        self.status[line] = {"parsed": 1, "malformed": 2}.get(record.status, 3)
        if record.kind not in self.kind_index:
            self.kind_index[record.kind] = len(self.kind_names)
            self.kind_names.append(record.kind)
        self.kinds[line] = self.kind_index[record.kind]
        self.counts[record.status] += 1
        if record.status != "parsed":
            if len(self.corrupt) < LIST_ITEMS:
                self.corrupt.append({"line": line + 1, "offset": self.starts[line], "status": record.status,
                                     "kind": record.kind, "reason": record.reason,
                                     "text": preview(self.lines[line])})
            return
        values, row = record.values, record.row
        if "n" not in values or row is None or not row.time:
            return
        number = values["n"]
        self.numbers[line] = number
        key = (record.kind, number, values.get(row.time))
        self.nkeys[key] = -1 if key in self.nkeys else line
        family = record.role
        if not record.kind.endswith("_BEGIN") and family in last and number != last[family] + 1:
            self.counts["gaps"] += 1
            self.gap_at[line] = 1
            if len(self.gaps) < LIST_ITEMS:
                self.gaps.append({"family": family, "line": line + 1, "expected": last[family] + 1,
                                  "found": number})
        last[family] = number

    def damaged(self, line):
        return self.status[line] >= 2 or self.gap_at[line]

    def damage(self, start, end, limit=8):
        """Damaged lines (1-based) holding [start, end), the line before and the line after, where a
        numbering gap caused here would show. Receiver errors count as damage of their bytes."""
        if not self.lines:
            return [], 0
        first = self.line_of(max(0, start - 1))
        last = min(len(self.lines) - 1, self.line_of(max(start, end - 1)) + 1)
        lines = [k + 1 for k in range(first, last + 1) if self.damaged(k)][:limit]
        errors = self.times.errors_in(max(0, start - 1), end + 1) if self.times else 0
        return lines, errors

    def lines_info(self, start, end):
        """Line numbers and parsed identity of the lines holding [start, end)."""
        first = self.line_of(start)
        last = self.line_of(max(start, end - 1))
        records = []
        for line in range(first, min(last, first + 3) + 1):
            if line >= len(self.lines):
                break
            entry = {"line": line + 1, "status": STATUS_NAMES[self.status[line]]}
            if self.kinds[line]:
                entry["kind"] = self.kind_names[self.kinds[line]]
            if self.numbers[line] >= 0:
                entry["n"] = self.numbers[line]
            records.append(entry)
        return {"lines": [first + 1, last + 1], "records": records}

    def summary(self):
        return {"lines": len(self.lines), "unterminated_tail": self.unterminated,
                "parsed": self.counts["parsed"], "malformed": self.counts["malformed"],
                "unknown": self.counts["unknown"], "numbering_gaps": self.counts["gaps"],
                "corrupt": self.corrupt, "gaps": self.gaps}


# ---------------------------------------------------------------- alignment helpers
def common_prefix(a, ai, b, bi, limit):
    """Length of the common run of a[ai:] and b[bi:], at most limit."""
    done = 0
    while done < limit:
        step = min(4096, limit - done)
        if a[ai + done:ai + done + step] == b[bi + done:bi + done + step]:
            done += step
            continue
        while a[ai + done] == b[bi + done]:
            done += 1
        return done
    return done


def common_suffix(a, ae, b, be, limit):
    """Length of the common run ending at a[:ae] and b[:be], at most limit."""
    done = 0
    while done < limit:
        step = min(4096, limit - done)
        if a[ae - done - step:ae - done] == b[be - done - step:be - done]:
            done += step
            continue
        while a[ae - done - 1] == b[be - done - 1]:
            done += 1
        return done
    return done


def longest_chain(pairs):
    """Longest subsequence of (host line, external line, hard) pairs, sorted by host line, with
    strictly increasing external lines (patience alignment). Returns (host, external, hard,
    contested) tuples. A pair is contested when another equally long chain puts a different pair
    at its place: a pair lies on some longest chain exactly when the longest chains ending and
    starting at it add up to the longest length, and two such pairs at one place compete."""
    count = len(pairs)
    forward, backward, previous = [0] * count, [0] * count, [-1] * count
    tails, tail_index = [], []
    for index, (_, line, _) in enumerate(pairs):
        position = bisect_left(tails, line)
        if position == len(tails):
            tails.append(line)
            tail_index.append(index)
        else:
            tails[position] = line
            tail_index[position] = index
        previous[index] = tail_index[position - 1] if position else -1
        forward[index] = position + 1
    reverse = []
    for index in range(count - 1, -1, -1):
        position = bisect_left(reverse, -pairs[index][1])
        if position == len(reverse):
            reverse.append(-pairs[index][1])
        else:
            reverse[position] = -pairs[index][1]
        backward[index] = position + 1
    length = len(tail_index)
    places = Counter(forward[k] for k in range(count) if forward[k] + backward[k] - 1 == length)
    chain = []
    index = tail_index[-1] if tail_index else -1
    while index >= 0:
        chain.append(pairs[index] + (places[forward[index]] > 1,))
        index = previous[index]
    chain.reverse()
    return chain


def _round(value):
    return None if value is None else round(value, 6) + 0.0  # + 0.0 turns -0.0 into 0.0.


# ---------------------------------------------------------------- comparison
class Comparison:
    """Align host against external (the reference) and collect findings in stream order."""

    def __init__(self, host, external, window_bytes, diff_budget):
        self.host, self.ext = host, external
        self.window_bytes, self.budget = window_bytes, diff_budget
        self.items = []
        self.windows = self.examined = self.hard = self.soft = self.contested = 0
        self.limits_hit = Counter()
        self.stopped = self.byte_identical = self.guarded = False

    # -- driver
    def run(self):
        host, ext = self.host, self.ext
        if host.data == ext.data:
            self.byte_identical = True
            return self
        chain = self._chain()
        self.contested = sum(1 for pair in chain if pair[3])
        self.hard = sum(1 for pair in chain if pair[2] and not pair[3])
        self.soft = sum(1 for pair in chain if not pair[2] and not pair[3])
        try:
            previous_h = previous_e = -1
            softs, contested = [], False
            for h_line, e_line, hard, disputed in chain + [(len(host.lines), len(ext.lines), True, False)]:
                if disputed:
                    contested = True  # Not a boundary: the window spans it and stays ambiguous.
                    continue
                if not hard:
                    softs.append((h_line, e_line))
                    continue
                first = len(self.items)
                self._window(previous_h + 1, h_line, previous_e + 1, e_line, softs)
                if contested:
                    for item in self.items[first:]:
                        item["alternatives"] = "contested_anchors"
                previous_h, previous_e, softs, contested = h_line, e_line, [], False
        except _ItemLimit:
            self.stopped = True
        self._finish()
        if not self.hard and not self.soft:
            identical = len(host) - sum(item["b2"] - item["b1"] for item in self.items)
            if identical < MIN_DEFENSIBLE:
                # No anchor line anywhere and almost nothing in common: any finding would rest
                # on coincidence, so the whole comparison is unalignable.
                self.guarded = True
                self.items = [{"kind": "unalignable", "a1": 0, "a2": len(ext), "b1": 0, "b2": len(host),
                               "limit": "no_defensible_anchor"}]
        return self

    def _chain(self):
        host, ext = self.host, self.ext

        def unique_lines(side):
            # line -> index, or -1 when it repeats; one dict per side bounds the memory.
            found, last = {}, len(side.lines) - 1
            for index, line in enumerate(side.lines):
                if len(line) >= MIN_ANCHOR and not (side.unterminated and index == last):
                    found[line] = -1 if line in found else index
            return found

        host_at, ext_at = unique_lines(host), unique_lines(ext)
        pairs = [(host_at[line], index, True) for line, index in ext_at.items()
                 if index >= 0 and host_at.get(line, -1) >= 0]
        del host_at, ext_at
        used_h = {pair[0] for pair in pairs}
        used_e = {pair[1] for pair in pairs}
        for key, e_line in ext.nkeys.items():
            h_line = host.nkeys.get(key, -1)
            if e_line >= 0 and h_line >= 0 and h_line not in used_h and e_line not in used_e:
                pairs.append((h_line, e_line, False))
        pairs.sort()
        return longest_chain(pairs)

    def _window(self, h_lo, h_hi, e_lo, e_hi, softs):
        host, ext = self.host, self.ext
        # At a stream boundary only a line long enough to be an anchor shows that the two
        # captures start (or end) at the same byte; once one is matched the rest follows.
        edge = h_lo == 0 and e_lo == 0
        while (h_lo < h_hi and e_lo < e_hi and host.key(h_lo) == ext.key(e_lo)
               and (not edge or len(host.lines[h_lo]) >= MIN_ANCHOR)):
            h_lo, e_lo, edge = h_lo + 1, e_lo + 1, False
        edge = h_hi == len(host.lines) and e_hi == len(ext.lines)
        while (h_hi > h_lo and e_hi > e_lo and host.key(h_hi - 1) == ext.key(e_hi - 1)
               and (not edge or len(host.lines[h_hi - 1]) >= MIN_ANCHOR)):
            h_hi, e_hi, edge = h_hi - 1, e_hi - 1, False
        if h_lo == h_hi and e_lo == e_hi:
            return
        h0, h1, e0, e1 = host.offset(h_lo), host.offset(h_hi), ext.offset(e_lo), ext.offset(e_hi)
        fits = (h1 - h0 <= self.window_bytes and e1 - e0 <= self.window_bytes
                and max(h_hi - h_lo, e_hi - e_lo) <= WINDOW_LINES)
        if fits and self.budget >= (h1 - h0) + (e1 - e0):
            self.budget -= (h1 - h0) + (e1 - e0)
            self.examined += (h1 - h0) + (e1 - e0)
            self.windows += 1
            self._core(h_lo, h_hi, e_lo, e_hi)
            return
        at_boundary = (h_lo == 0 and e_lo == 0) or (h_hi == len(host.lines) and e_hi == len(ext.lines))
        if at_boundary and min(h1 - h0, e1 - e0) <= BYTE_DIFF_MAX // 2 and self.budget >= BYTE_DIFF_MAX:
            # A capture that starts late or stops early: its few bytes are matched against the
            # near end of the other side's long remainder, the rest being head or tail.
            self.budget -= BYTE_DIFF_MAX
            self.examined += BYTE_DIFF_MAX
            self.windows += 1
            self._region(e0, e1, h0, h1)
            return
        inner = [(h, e) for h, e in softs if h_lo <= h < h_hi and e_lo <= e < e_hi]
        if inner:
            cursor_h, cursor_e = h_lo, e_lo
            for h_line, e_line in inner:
                self._window(cursor_h, h_line, cursor_e, e_line, [])
                self._window(h_line, h_line + 1, e_line, e_line + 1, [])
                cursor_h, cursor_e = h_line + 1, e_line + 1
            self._window(cursor_h, h_hi, cursor_e, e_hi, [])
            return
        reason = "window_cap" if not fits else "diff_budget"
        self.limits_hit[reason] += 1
        self._classify(e0, e1, h0, h1, reason, boundary=MIN_ANCHOR)

    def _core(self, h_lo, h_hi, e_lo, e_hi):
        host, ext = self.host, self.ext
        forced = True
        if h_hi - h_lo <= 1 or e_hi - e_lo <= 1:
            blocks = [(e_lo, e_hi, h_lo, h_hi)]
        else:
            e_keys = [ext.key(k) for k in range(e_lo, e_hi)]
            h_keys = [host.key(k) for k in range(h_lo, h_hi)]
            matcher = difflib.SequenceMatcher(None, e_keys, h_keys, autojunk=False)
            opcodes = matcher.get_opcodes()
            blocks = [(e_lo + i1, e_lo + i2, h_lo + j1, h_lo + j2)
                      for tag, i1, i2, j1, j2 in opcodes if tag != "equal"]
            e_count, h_count = Counter(e_keys), Counter(h_keys)
            repeated = any(e_count[key] > 1 or h_count[key] > 1
                           for tag, i1, i2, _, _ in opcodes if tag == "equal" for key in e_keys[i1:i2])
            # A line left unmatched on both sides, in different gaps, could have been matched
            # instead of what lies between: a competing alignment (a reorder, for one).
            e_open = {key for tag, i1, i2, _, _ in opcodes if tag != "equal" for key in e_keys[i1:i2]}
            h_open = {key for tag, _, _, j1, j2 in opcodes if tag != "equal" for key in h_keys[j1:j2]}
            forced = not repeated and not (e_open & h_open)
        first = len(self.items)
        for e1, e2, h1, h2 in blocks:
            self._region(ext.offset(e1), ext.offset(e2), host.offset(h1), host.offset(h2))
        self._mark_alternatives(first, forced)

    def _mark_alternatives(self, first, forced):
        """Several findings from one split are one alignment among others unless every equal block
        the split relied on is unique on both sides and no unmatched content competes."""
        if not forced and len(self.items) - first > 1:
            for item in self.items[first:]:
                item.setdefault("alternatives", "competing_blocks")

    def _strip(self, a1, a2, b1, b2, boundary):
        """Common prefix and suffix. At a stream start or end they count only when at least
        `boundary` bytes long; with boundary None they never do there (a matcher decides)."""
        e, h = self.ext.data, self.host.data
        prefix = common_prefix(e, a1, h, b1, min(a2 - a1, b2 - b1))
        if a1 == 0 and b1 == 0 and (boundary is None or prefix < boundary):
            prefix = 0
        a1, b1 = a1 + prefix, b1 + prefix
        suffix = common_suffix(e, a2, h, b2, min(a2 - a1, b2 - b1))
        if a2 == len(e) and b2 == len(h) and (boundary is None or suffix < boundary):
            suffix = 0
        return a1, a2 - suffix, b1, b2 - suffix

    def _matchable(self, a1, a2, b1, b2):
        """The part of a region the byte matcher may examine, or None when it is too large. At a
        stream end a short side is matched against the start of the long one, and at a stream
        start against its end, so a host that stops early still aligns its last bytes."""
        de, dh = a2 - a1, b2 - b1
        if de + dh <= BYTE_DIFF_MAX:
            return a1, a2, b1, b2
        at_start = a1 == 0 and b1 == 0
        at_end = a2 == len(self.ext) and b2 == len(self.host)
        short = min(de, dh)
        if at_start == at_end or short > BYTE_DIFF_MAX // 2:
            return None
        keep = BYTE_DIFF_MAX - short
        if at_end:
            return (a1, a1 + keep, b1, b2) if de > dh else (a1, a2, b1, b1 + keep)
        return (a2 - keep, a2, b1, b2) if de > dh else (a1, a2, b2 - keep, b2)

    def _region(self, a1, a2, b1, b2):
        a1, a2, b1, b2 = self._strip(a1, a2, b1, b2, None)
        if a1 == a2 and b1 == b2:
            return
        if a1 == a2 or b1 == b2:
            self._classify(a1, a2, b1, b2)
            return
        part = self._matchable(a1, a2, b1, b2)
        if part is None:
            self._classify(a1, a2, b1, b2, boundary=MIN_ANCHOR)
            return
        m1, m2, n1, n2 = part
        e_run, h_run = self.ext.data[m1:m2], self.host.data[n1:n2]
        matcher = difflib.SequenceMatcher(None, e_run, h_run, autojunk=False)
        kept = [block for block in matcher.get_matching_blocks() if block.size >= MIN_EQUAL]
        forced = all(e_run.find(e_run[b.a:b.a + b.size]) == e_run.rfind(e_run[b.a:b.a + b.size])
                     and h_run.find(e_run[b.a:b.a + b.size]) == h_run.rfind(e_run[b.a:b.a + b.size])
                     for b in kept)
        gaps, pa, pb = [], a1, b1
        for block in kept:
            gaps.append((pa, m1 + block.a, pb, n1 + block.b))
            pa, pb = m1 + block.a + block.size, n1 + block.b + block.size
        gaps.append((pa, a2, pb, b2))
        forced = forced and not self._crossing(gaps, m1, m2, n1, n2)
        first = len(self.items)
        for gap in gaps:
            self._classify(*gap)
        self._mark_alternatives(first, forced)

    def _crossing(self, gaps, m1, m2, n1, n2):
        """True when unmatched external bytes of one gap share a run of MIN_EQUAL bytes with
        unmatched host bytes of another gap: the matcher's choice between them was arbitrary."""
        e_pieces = [(k, self.ext.data[max(g[0], m1):min(g[1], m2)]) for k, g in enumerate(gaps)]
        h_pieces = [(k, self.host.data[max(g[2], n1):min(g[3], n2)]) for k, g in enumerate(gaps)]
        e_pieces = [(k, p) for k, p in e_pieces if len(p) >= MIN_EQUAL]
        h_pieces = [(k, p) for k, p in h_pieces if len(p) >= MIN_EQUAL]
        if len(e_pieces) > CROSSING_PIECES or len(h_pieces) > CROSSING_PIECES:
            return True  # Too many to check: do not claim the split is forced.
        for k, piece in e_pieces:
            for j, other in h_pieces:
                if k != j and difflib.SequenceMatcher(None, piece, other, autojunk=False).find_longest_match(
                        0, len(piece), 0, len(other)).size >= MIN_EQUAL:
                    return True
        return False

    # -- one finding
    def _classify(self, a1, a2, b1, b2, reason=None, boundary=None):
        a1, a2, b1, b2 = self._strip(a1, a2, b1, b2, boundary)
        if a1 == a2 and b1 == b2:
            return
        e, h = self.ext.data, self.host.data
        de, dh = a2 - a1, b2 - b1
        at_start, at_end = a1 == 0 and b1 == 0, a2 == len(e) and b2 == len(h)
        item = {"a1": a1, "a2": a2, "b1": b1, "b2": b2}
        if at_end and dh == 0:
            item["kind"] = "host_missing_tail"
        elif at_start and de == 0:
            item["kind"] = "head"
        elif at_start and at_end:
            item["kind"] = "unalignable"
        elif at_start:
            item["kind"] = "head"
        elif at_end:
            item["kind"] = "external_ended_first" if de == 0 else "tail_divergence"
        elif reason:
            item["kind"] = "unaligned"
        elif dh == 0:
            item["kind"] = "deletion"
        elif de == 0:
            item["kind"] = "insertion"
        else:
            source = self._source(b1, b2, a1, a2)
            hamming = sum(1 for x, y in zip(e[a1:a2], h[b1:b2]) if x != y) if de == dh else None
            if de == dh and not source and hamming <= CORRUPTION_MAX:
                item["kind"] = "corruption"
            elif de > REPLACEMENT_MAX and dh > REPLACEMENT_MAX:
                item["kind"] = "unaligned"
            else:
                item["kind"] = "replacement"
                if source:
                    item["source"] = source
        if reason:
            item["limit"] = reason
        if len(self.items) >= MAX_ITEMS:
            self.items.append({"kind": "not_examined", "a1": a1, "a2": len(e), "b1": b1, "b2": len(h),
                               "limit": "max_items"})
            raise _ItemLimit()
        self.items.append(item)

    def _source(self, b1, b2, a1, a2):
        """External offsets near this point that hold exactly the host's run [b1, b2)."""
        if b2 - b1 < MIN_SOURCE:
            return None
        run = self.host.data[b1:b2]
        low, high = max(0, a1 - self.window_bytes), min(len(self.ext), a2 + self.window_bytes)
        found, position = [], self.ext.data.find(run, low, high)
        while position >= 0 and len(found) < 5:  # At most five are listed.
            found.append(position)
            position = self.ext.data.find(run, position + 1, high)
        return found or None

    # -- slides, boundaries, reorders and descriptions, once neighbours are known
    def _bounds(self, index):
        previous = self.items[index - 1] if index else None
        following = self.items[index + 1] if index + 1 < len(self.items) else None
        lo_a, lo_b = (previous["a2"], previous["b2"]) if previous else (0, 0)
        hi_a, hi_b = (following["a1"], following["b1"]) if following else (len(self.ext), len(self.host))
        return lo_a, lo_b, hi_a, hi_b

    def _finish(self):
        e, h = self.ext.data, self.host.data
        cap = self.window_bytes
        for index, item in enumerate(self.items):
            lo_a, lo_b, hi_a, hi_b = self._bounds(index)
            if item["kind"] == "deletion":
                self._slide(item, e, item["a1"], item["a2"] - item["a1"], item["b1"], lo_a, lo_b, hi_a, hi_b, cap,
                            moving="external")
            elif item["kind"] == "insertion":
                self._slide(item, h, item["b1"], item["b2"] - item["b1"], item["a1"], lo_b, lo_a, hi_b, hi_a, cap,
                            moving="host")
                source = self._source(item["b1"], item["b2"], item["a1"], item["a1"])
                if source:
                    item["kind"] = "duplication"
                    item["source"] = source
            else:
                continue
            self._to_boundary(item)
        self._merge_reorders()
        for index, item in enumerate(self.items):
            lo_a, lo_b, hi_a, hi_b = self._bounds(index)
            if item["kind"] == "corruption":
                item["differences"] = [[item["a1"] + k, item["b1"] + k, x, y] for k, (x, y) in
                                       enumerate(zip(e[item["a1"]:item["a2"]], h[item["b1"]:item["b2"]]))
                                       if x != y]
            elif item["kind"] == "replacement":
                # A replacement that changes the length also deletes (or inserts) that many
                # bytes. In text with that period the deletion could sit before or after it.
                delta = (item["a2"] - item["a1"]) - (item["b2"] - item["b1"])
                if delta > 0:
                    left = self._run(e, item["a1"], delta, lo_a, -1, cap)
                    right = self._run(e, item["a2"] - delta, delta, hi_a, 1, cap)
                elif delta < 0:
                    left = self._run(h, item["b1"], -delta, lo_b, -1, cap)
                    right = self._run(h, item["b2"] + delta, -delta, hi_b, 1, cap)
                else:
                    left = right = 0
                if left or right:
                    item["span"] = {"left": left, "right": right, "truncated": left == cap or right == cap}

    def _to_boundary(self, item):
        """An edit that can slide to a stream start or end is no evidence of loss there: it is
        equally a capture that started later or stopped earlier."""
        slide = item["slide"]
        length = (item["a2"] - item["a1"]) or (item["b2"] - item["b1"])
        if item["kind"] == "deletion":
            if item["a1"] + slide["right"] + length == len(self.ext) and item["b1"] + slide["right"] == len(self.host):
                item["kind"] = "host_missing_tail"
            elif item["a1"] - slide["left"] == 0 and item["b1"] - slide["left"] == 0:
                item["kind"] = "head"
        else:
            if item["b1"] + slide["right"] + length == len(self.host) and item["a1"] + slide["right"] == len(self.ext):
                item["kind"] = "external_ended_first"
            elif item["a1"] - slide["left"] == 0 and item["b1"] - slide["left"] == 0:
                item["kind"] = "head"
        if item["kind"] in ("host_missing_tail", "head", "external_ended_first"):
            item.pop("source", None)

    def _merge_reorders(self):
        """A deletion next to an insertion of the same bytes is one move, and which of the two
        neighbouring parts moved cannot be told: one ambiguous reorder, never two exact edits."""
        merged, index = [], 0
        while index < len(self.items):
            item = self.items[index]
            following = self.items[index + 1] if index + 1 < len(self.items) else None
            kinds = {item["kind"], following["kind"]} if following else set()
            if following and kinds in ({"deletion", "insertion"}, {"deletion", "duplication"}):
                removed, added = (item, following) if item["kind"] == "deletion" else (following, item)
                if removed["a2"] - removed["a1"] == added["b2"] - added["b1"] and self._same_content(removed, added):
                    merged.append({"kind": "reorder", "a1": item["a1"], "a2": following["a2"], "b1": item["b1"],
                                   "b2": following["b2"], "moved": removed["a2"] - removed["a1"],
                                   "removed_at": [removed["a1"], removed["b1"]],
                                   "inserted_at": [added["a1"], added["b1"]],
                                   "alternatives": item.get("alternatives") or following.get("alternatives")})
                    index += 2
                    continue
            merged.append(item)
            index += 1
        self.items = merged

    def _same_content(self, removed, added):
        """Whether some equivalent placement of the removed external bytes equals some equivalent
        placement of the inserted host bytes."""
        length = removed["a2"] - removed["a1"]
        left, right = removed["slide"]["left"], removed["slide"]["right"]
        candidates = self.ext.data[removed["a1"] - left:removed["a1"] + right + length]
        low, high = added["slide"]["left"], added["slide"]["right"]
        for shift in range(-low, min(high, -low + length - 1, -low + REORDER_TRIES - 1) + 1):
            start = added["b1"] + shift
            position = candidates.find(self.host.data[start:start + length])
            if 0 <= position <= left + right:
                return True
        return False

    @staticmethod
    def _run(data, start, length, bound, direction, cap):
        """How far a removal of data[start:start+length] can move left (-1) or right (1)."""
        moved = 0
        if direction < 0:
            while (moved < cap and start - moved - 1 >= bound
                   and data[start - moved - 1] == data[start - moved - 1 + length]):
                moved += 1
        else:
            while (moved < cap and start + moved + length < bound
                   and data[start + moved] == data[start + moved + length]):
                moved += 1
        return moved

    @staticmethod
    def _slide(item, data, start, length, other, lo, lo_other, hi, hi_other, cap, moving):
        """Every equivalent place of a pure deletion (in external) or insertion (in host)."""
        left = 0
        while (left < cap and start - left - 1 >= lo and other - left - 1 >= lo_other
               and data[start - left - 1] == data[start - left - 1 + length]):
            left += 1
        right = 0
        while (right < cap and start + right + length < hi and other + right < hi_other
               and data[start + right] == data[start + right + length]):
            right += 1
        # Line structure can single out one of the equivalent places; it is reported beside the
        # interval, never in place of it.
        aligned = [s for s in range(start - left, start + right + 1)
                   if (s == 0 or data[s - 1] == LF) and data[s + length - 1] == LF]
        item["slide"] = {"left": left, "right": right, "truncated": left == cap or right == cap}
        item["moving"] = moving
        item["newlines"] = data[start:start + length].count(b"\n")
        item["line_aligned"] = aligned[:4]
        item["whole_lines_possible"] = bool(aligned)


# ---------------------------------------------------------------- qualification
def qualify_damage(items, host, ext):
    """Tie each side's parser damage and receiver errors to the findings it overlaps. A finding
    where only the external capture is damaged may be the external receiver's own loss."""
    for item in items:
        if item["kind"] not in INTERIOR:
            continue
        e_lines, e_errors = ext.damage(item["a1"], item["a2"])
        h_lines, h_errors = host.damage(item["b1"], item["b2"])
        item["damage"] = {"external_lines": e_lines, "external_receiver_errors": e_errors,
                          "host_lines": h_lines, "host_receiver_errors": h_errors}
        if (e_lines or e_errors) and not (h_lines or h_errors):
            item["suspect"] = "external_capture"


def shared_damage(items, side, coordinate):
    """Damaged lines of one side that lie wholly in identical regions: the other capture holds
    the same bytes there, so the damage was already on the wire."""
    starts = [item[coordinate + "1"] for item in items]
    found, total = [], 0
    for line in range(len(side.lines)):
        if not side.damaged(line):
            continue
        start, end = side.starts[line], side.offset(line + 1)
        index = bisect_right(starts, end) - 1
        overlapping = index >= 0 and items[index][coordinate + "2"] >= start
        if not overlapping:
            total += 1
            if len(found) < LIST_ITEMS:
                found.append(line + 1)
    return {"count": total, "lines": found}


def qualify_tail(items, host, ext, clocks):
    """With related clocks, a missing host tail counts only for the external bytes held before the
    host's last chunk; the rest may have followed the host closing its port."""
    for item in items:
        if item["kind"] != "host_missing_tail":
            continue
        last = host.times.values[-1] if host.times is not None and len(host.times.values) else None
        if clocks.reason or last is None:
            item["tail_timing"] = {"status": "not_established", "reason": clocks.reason or "no host times"}
            continue
        before = 0
        for start, stop, value in ext.times.chunks(item["a1"], item["a2"]):
            bound = clocks.host_clock_bound(value, start)
            if bound is not None and bound <= last:
                before += min(stop, item["a2"]) - max(start, item["a1"])
        missing = item["a2"] - item["a1"]
        item["tail_timing"] = {"status": "established", "host_last_chunk_time": _round(last),
                               "bytes_held_by_external_before_host_last_chunk": before,
                               "bytes_not_shown_before": missing - before}
        if not before:
            item["kind"] = "external_beyond_host_end"


# ---------------------------------------------------------------- report
def _side_range(side, start, end):
    info = {"offset": start, "length": end - start}
    info.update(side.lines_info(start, end if end > start else start + 1))
    return info


def _timing(side, start, length):
    times = side.times
    if times is None or not len(side):
        return None
    reference = min(start, len(side) - 1)
    found = times.at(reference)
    if found is None:
        return {"offset": reference, "status": "untimed"}
    value, since = found
    gap = times.largest_gap(reference - NEAR_BYTES, reference + length + NEAR_BYTES)
    return {"offset": reference, "time": _round(value), "since_previous_chunk_s": _round(since),
            "largest_chunk_step_near_s": _round(gap)}


def _describe(item, host, ext, clocks):
    kind = item["kind"]
    a1, a2, b1, b2 = item["a1"], item["a2"], item["b1"], item["b2"]
    out = {"kind": kind, "meaning": MEANING[kind],
           "external": _side_range(ext, a1, a2), "host": _side_range(host, b1, b2)}
    if "limit" in item:
        out["limit"] = item["limit"]
    if "slide" in item:
        slide = item["slide"]
        left, right = slide["left"], slide["right"]
        out["location"] = "ambiguous" if left or right else "exact"
        out["equivalent_starts"] = {"external": [a1 - left, a1 + right], "host": [b1 - left, b1 + right],
                                    "bound_reached": slide["truncated"]}
        moving_side, start, end = (ext, a1, a2) if item["moving"] == "external" else (host, b1, b2)
        out[item["moving"]].update(moving_side.lines_info(start - left, end + right))
        out["partial_line"] = not item["whole_lines_possible"]
        out["joins_lines"] = item["newlines"] > 0 and not item["whole_lines_possible"]
        if item["line_aligned"]:
            shift = (b1 - a1) if item["moving"] == "external" else (a1 - b1)
            other = "host" if item["moving"] == "external" else "external"
            out["line_aligned_starts"] = {item["moving"]: item["line_aligned"],
                                          other: [s + shift for s in item["line_aligned"]]}
    elif kind in ("unaligned", "reorder"):
        out["location"] = "bounded" if kind == "unaligned" else "ambiguous"
    elif kind in DIVERGENCES:
        out["location"] = "exact"
    if kind == "reorder":
        out["moved_bytes"] = item["moved"]
        out["removed_at"] = {"external": item["removed_at"][0], "host": item["removed_at"][1]}
        out["inserted_at"] = {"external": item["inserted_at"][0], "host": item["inserted_at"][1]}
    if "span" in item:
        span = item["span"]
        out["location"] = "ambiguous"
        out["equivalent_span"] = {"external": [a1 - span["left"], a2 + span["right"]],
                                  "host": [b1 - span["left"], b2 + span["right"]],
                                  "bound_reached": span["truncated"]}
    if item.get("alternatives"):
        out["location"] = "ambiguous"
        out["alternative_alignment"] = {
            "contested_anchors": "equally long anchor chains place these lines differently",
            "competing_blocks": "this window's equal parts repeat or compete; another alignment may place "
                                "these findings differently"}[item["alternatives"]]
    if "source" in item:
        out["source"] = {"external_offsets": item["source"],
                         "relative_to_external_offset": [x - a1 for x in item["source"]],
                         "unique": len(item["source"]) == 1}
    if "differences" in item:
        out["differences"] = [{"external_offset": x, "host_offset": y, "external_byte": "%02x" % p,
                               "host_byte": "%02x" % q} for x, y, p, q in item["differences"]]
    if "damage" in item:
        out["parser_damage"] = item["damage"]
    if "suspect" in item:
        out["suspect"] = item["suspect"]
        out["suspect_reason"] = ("only the external capture's parse is damaged here, or its receiver reported "
                                 "errors: the host bytes are unverified, not shown lost or added")
    if "tail_timing" in item:
        out["tail_timing"] = item["tail_timing"]
    out["bytes"] = {"external": preview(ext.data[a1:a2]), "host": preview(host.data[b1:b2])}
    timing = {"external": _timing(ext, a1, a2 - a1), "host": _timing(host, b1, b2 - b1)}
    cross = clocks.relate(timing["host"], timing["external"])
    if cross:
        timing["cross"] = cross
    out["timing"] = timing
    return out


class Clocks:
    """Relates the two recorders' clocks only through markers or stated bounds."""

    def __init__(self, host, ext, items, markers, bounds):
        self.host, self.ext, self.bounds = host, ext, bounds
        self.markers = [self._marker(text, items) for text in markers]
        self.usable = sorted((m for m in self.markers if m["status"] == "ok"), key=lambda m: m["external_offset"])
        reasons = []
        if host.times is None or ext.times is None:
            reasons.append("times missing for %s" % " and ".join(
                side.role for side in (host, ext) if side.times is None))
        if not markers and bounds is None:
            reasons.append("no shared marker or stated offset bounds")
        elif markers and not self.usable and bounds is None:
            reasons.append("no usable marker")
        self.reason = "; ".join(reasons) or None

    def _marker(self, text, items):
        needle = text.encode("utf-8")
        record = {"text": text}
        for side in (self.host, self.ext):
            places, position = [], side.data.find(needle)
            while position >= 0 and len(places) < 2:
                places.append(position)
                position = side.data.find(needle, position + 1)
            record[side.role] = places
        host_places, ext_places = record.pop("host"), record.pop("external")
        if not host_places or not ext_places:
            record["status"] = "missing_" + ("host" if not host_places else "external")
            return record
        if len(host_places) > 1 or len(ext_places) > 1:
            record["status"] = "ambiguous_" + ("host" if len(host_places) > 1 else "external")
            return record
        a, b = ext_places[0], host_places[0]
        shift, inside = 0, False
        for item in items:
            if item["a1"] >= a + len(needle) and item["b1"] >= b + len(needle):
                break
            if item["a2"] <= a and item["b2"] <= b:
                shift += (item["b2"] - item["b1"]) - (item["a2"] - item["a1"])
            else:
                inside = True
                break
        record["external_offset"], record["host_offset"] = a, b
        if inside or a + shift != b:
            record["status"] = "misaligned"
            return record
        last_e, last_h = a + len(needle) - 1, b + len(needle) - 1
        found_e = self.ext.times.at(last_e) if self.ext.times else None
        found_h = self.host.times.at(last_h) if self.host.times else None
        if found_e is None or found_h is None:
            record["status"] = "untimed"
            return record
        record["status"] = "ok"
        record["external_time"], record["host_time"] = _round(found_e[0]), _round(found_h[0])
        record["host_minus_external_s"] = _round(found_h[0] - found_e[0])
        return record

    def _marker_for(self, offset):
        before = [m for m in self.usable if m["external_offset"] <= offset]
        return before[-1] if before else self.usable[0]

    def host_clock_bound(self, external_time, offset):
        """An upper bound in host clock for an external observation time, or None. A marker's
        host-minus-external difference includes the host's delay there, so adding it bounds the
        external observation from above (drift since the marker ignored); stated bounds add HI."""
        bounds = []
        if self.usable:
            bounds.append(external_time + self._marker_for(offset)["host_minus_external_s"])
        if self.bounds is not None:
            bounds.append(external_time + self.bounds[1])
        return min(bounds) if bounds else None

    def relate(self, host_timing, ext_timing):
        if self.reason or not host_timing or not ext_timing:
            return None
        if "time" not in host_timing or "time" not in ext_timing:
            return {"status": "untimed"}
        th, te = host_timing["time"], ext_timing["time"]
        out = {}
        if self.usable:
            marker = self._marker_for(ext_timing["offset"])
            out["marker"] = marker["text"]
            out["delay_change_since_marker_s"] = _round((th - marker["host_time"]) - (te - marker["external_time"]))
            out["external_elapsed_since_marker_s"] = _round(te - marker["external_time"])
        if self.bounds is not None:
            low, high = self.bounds
            out["host_minus_external_observation_s"] = [_round(th - te - high), _round(th - te - low)]
        return out

    def describe(self):
        out = {"markers": self.markers, "offset_bounds": list(self.bounds) if self.bounds is not None else None}
        if self.reason:
            out["cross"] = {"status": "not_established", "reason": self.reason}
        else:
            out["cross"] = {"status": "established", "basis": (["markers"] if self.usable else []) +
                            (["stated_bounds"] if self.bounds is not None else [])}
        out["semantics"] = ("Each time is that recorder's chunk observation time, an upper bound for arrival. "
                            "Marker figures are delay changes since the marker and include drift; bounds figures "
                            "follow from the stated offset only.")
        return out


def _category(item):
    if item["kind"] in DIVERGENCES and "suspect" not in item:
        return "divergence"
    if item["kind"] in DIVERGENCES or item["kind"] in UNVERIFIED:
        return "unverified"
    return "information"


def build_report(host, ext, comparison, settings, markers, bounds):
    items = comparison.items
    qualify_damage(items, host, ext)
    clocks = Clocks(host, ext, items, markers, bounds)
    if ext.times is not None:
        qualify_tail(items, host, ext, clocks)
    else:
        for item in items:
            if item["kind"] == "host_missing_tail":
                item["tail_timing"] = {"status": "not_established", "reason": clocks.reason}
    divergent = [item for item in items if _category(item) == "divergence"]
    unverified = [item for item in items if _category(item) == "unverified"]
    unverified_host = sum(item["b2"] - item["b1"] for item in unverified)
    differing_host = sum(item["b2"] - item["b1"] for item in divergent)
    suspect = sum(1 for item in items if "suspect" in item)
    if divergent:
        verdict, status = "divergent", DIVERGENT
    elif unverified_host or suspect or any(item["kind"] in ("unalignable", "not_examined") for item in items):
        verdict, status = "inconclusive", INCONCLUSIVE
    else:
        verdict, status = "identical", IDENTICAL
    first = None
    if divergent:
        item = divergent[0]
        first = {"kind": item["kind"], "external_offset": item["a1"], "host_offset": item["b1"],
                 "external_line": ext.line_of(item["a1"]) + 1, "host_line": host.line_of(item["b1"]) + 1,
                 "basis": "both captures agree byte for byte from the last common point up to here"}
        left = (item.get("slide") or item.get("span") or {}).get("left", 0)
        if left:
            first["edit_may_start_at"] = {"external": item["a1"] - left, "host": item["b1"] - left}
        if item.get("alternatives") or item["kind"] == "reorder":
            first["alternative_alignment"] = True
    head = next((item for item in items if item["kind"] == "head"), None)
    tail = next((item for item in items if item["kind"] in ("host_missing_tail", "tail_divergence",
                                                             "external_ended_first", "external_beyond_host_end")),
                None)
    counts = Counter(item["kind"] for item in items)
    return status, {
        "tool": "s2_uart_compare", "schema": SCHEMA, "format": settings["format"], "verdict": verdict,
        "exit_status": status,
        "inputs": {"host": dict(host.info, times=host.times.describe() if host.times else None),
                   "external": dict(ext.info, times=ext.times.describe() if ext.times else None)},
        "limits": {"max_bytes": settings["max_bytes"], "max_lines": settings["max_lines"],
                   "window_bytes": comparison.window_bytes, "window_lines": WINDOW_LINES,
                   "diff_budget": settings["diff_budget"], "max_items": MAX_ITEMS, "listed": LIST_ITEMS,
                   "hit": dict(sorted(comparison.limits_hit.items())),
                   "stopped_at_max_items": comparison.stopped},
        "alignment": {"byte_identical": comparison.byte_identical, "anchors_identical": comparison.hard,
                      "anchors_numbered": comparison.soft, "anchors_contested": comparison.contested,
                      "no_defensible_anchor": comparison.guarded, "windows_diffed": comparison.windows,
                      "bytes_diffed": comparison.examined},
        "coverage": {"host_bytes": len(host), "host_identical": len(host) - unverified_host - differing_host,
                     "host_in_divergences": differing_host, "host_unverified": unverified_host,
                     "external_bytes": len(ext), "findings_suspect_external_capture": suspect},
        "first_divergence": first,
        "head": ({"external_only_bytes": head["a2"] - head["a1"], "host_unverified_bytes": head["b2"] - head["b1"]}
                 if head else None),
        "tail": ({"kind": tail["kind"], "external_only_bytes": tail["a2"] - tail["a1"],
                  "host_only_bytes": tail["b2"] - tail["b1"],
                  "host_ends_mid_line": host.unterminated, "external_ends_mid_line": ext.unterminated,
                  "timing": tail.get("tail_timing")}
                 if tail else None),
        "counts": dict(sorted(counts.items())),
        "findings": [_describe(item, host, ext, clocks) for item in items[:LIST_ITEMS]],
        "parser": {"host": host.summary(), "external": ext.summary(),
                   "external_damage_in_identical_regions": shared_damage(items, ext, "a"),
                   "host_damage_in_identical_regions": shared_damage(items, host, "b")},
        "timing": clocks.describe(),
    }


def host_only_report(host, message):
    return {"tool": "s2_uart_compare", "schema": SCHEMA, "verdict": "external_unavailable",
            "exit_status": UNAVAILABLE, "error": message, "requirements": REQUIREMENTS,
            "inputs": {"host": dict(host.info, times=host.times.describe() if host.times else None)},
            "parser": {"host": host.summary()}}


def render(report):
    """Plain text summary of a report; deterministic."""
    out = ["s2_uart_compare: %s (exit %d)" % (report["verdict"].upper(), report["exit_status"])]
    for role in ("host", "external"):
        if role not in report["inputs"]:
            continue
        info, parsed = report["inputs"][role], report["parser"][role]
        out.append("%-8s %s: %d bytes sha256 %s; %d lines: %d parsed, %d malformed, %d unknown; %d numbering gap(s)%s"
                   % (role, info["path"], info["bytes"], info["sha256"][:16], parsed["lines"], parsed["parsed"],
                      parsed["malformed"], parsed["unknown"], parsed["numbering_gaps"],
                      "; ends mid-line" if parsed["unterminated_tail"] else ""))
        if info.get("times") and info["times"]["receiver_errors"]:
            out.append("    receiver reported %d error(s) in %d chunk(s)" % (
                info["times"]["receiver_errors"], info["times"]["chunks_with_errors"]))
        for entry in parsed["corrupt"][:10]:
            out.append("    %s line %d @%d %s: %s | %s" % (entry["status"], entry["line"], entry["offset"],
                                                         entry["kind"], entry["reason"], entry["text"]))
        for gap in parsed["gaps"][:10]:
            out.append("    %s numbering at line %d: expected n=%d, found n=%d" % (
                gap["family"], gap["line"], gap["expected"], gap["found"]))
    if report["verdict"] == "external_unavailable":
        out.append(report["error"])
        out.append(report["requirements"])
        return "\n".join(out) + "\n"
    align, cover = report["alignment"], report["coverage"]
    out.append("alignment: %d identical, %d numbered and %d contested anchor lines; %d window(s), %d bytes diffed%s"
               % (align["anchors_identical"], align["anchors_numbered"], align["anchors_contested"],
                  align["windows_diffed"], align["bytes_diffed"],
                  "; no defensible anchor" if align["no_defensible_anchor"] else ""))
    out.append("coverage: host %d bytes: %d identical, %d in divergences, %d unverified; %d finding(s) at possible "
               "external capture damage" % (cover["host_bytes"], cover["host_identical"],
                                            cover["host_in_divergences"], cover["host_unverified"],
                                            cover["findings_suspect_external_capture"]))
    shared = report["parser"]["external_damage_in_identical_regions"]
    if shared["count"]:
        out.append("damage on the wire: %d external damaged line(s) lie in identical regions (the host holds the "
                   "same bytes): %s" % (shared["count"], shared["lines"][:10]))
    if report["limits"]["hit"] or report["limits"]["stopped_at_max_items"]:
        out.append("limits reached: %s%s" % (report["limits"]["hit"],
                                              "; stopped at max items" if report["limits"]["stopped_at_max_items"]
                                              else ""))
    head, tail = report["head"], report["tail"]
    if head:
        out.append("head: %d external-only byte(s), %d host byte(s) unverified" % (
            head["external_only_bytes"], head["host_unverified_bytes"]))
    if tail:
        text = "tail: %s: %d external-only byte(s), %d host-only byte(s)%s" % (
            tail["kind"], tail["external_only_bytes"], tail["host_only_bytes"],
            "; host ends mid-line" if tail["host_ends_mid_line"] else "")
        if tail["timing"]:
            text += "; timing %s" % json.dumps(tail["timing"], sort_keys=True)
        out.append(text)
    first = report["first_divergence"]
    if first:
        text = "first divergence: external @%d line %d / host @%d line %d: %s" % (
            first["external_offset"], first["external_line"], first["host_offset"], first["host_line"], first["kind"])
        if "edit_may_start_at" in first:
            text += " (the edit may start as early as external @%d / host @%d)" % (
                first["edit_may_start_at"]["external"], first["edit_may_start_at"]["host"])
        if first.get("alternative_alignment"):
            text += " (another alignment is possible)"
        out.append(text)
    if report["findings"]:
        out.append("findings (%s):" % ", ".join("%s %d" % pair for pair in report["counts"].items()))
    for item in report["findings"]:
        ext, host = item["external"], item["host"]
        where = "external @%d+%d lines %d-%d" % (ext["offset"], ext["length"], *ext["lines"])
        where += " / host @%d+%d lines %d-%d" % (host["offset"], host["length"], *host["lines"])
        flags = [item.get("location", "")]
        if "equivalent_starts" in item and item["location"] == "ambiguous":
            span = item["equivalent_starts"]
            flags.append("starts external %d..%d / host %d..%d%s" % (
                span["external"][0], span["external"][1], span["host"][0], span["host"][1],
                " or beyond" if span["bound_reached"] else ""))
        if "equivalent_span" in item:
            span = item["equivalent_span"]
            flags.append("within external [%d,%d) / host [%d,%d)%s" % (
                span["external"][0], span["external"][1], span["host"][0], span["host"][1],
                " or beyond" if span["bound_reached"] else ""))
        if "moved_bytes" in item:
            flags.append("%d bytes moved" % item["moved_bytes"])
        if "alternative_alignment" in item:
            flags.append("alternative alignment possible")
        if item.get("partial_line"):
            flags.append("partial line")
        if item.get("joins_lines"):
            flags.append("joins lines")
        if "source" in item:
            flags.append("host bytes equal external %s (relative %s)" % (
                item["source"]["external_offsets"], item["source"]["relative_to_external_offset"]))
        if "suspect" in item:
            flags.append("possible external capture damage (external lines %s, receiver errors %d)" % (
                item["parser_damage"]["external_lines"], item["parser_damage"]["external_receiver_errors"]))
        if "limit" in item:
            flags.append("limit " + item["limit"])
        records = [r for r in ext.get("records", []) if "kind" in r]
        named = ", ".join("%s%s" % (r["kind"], " n=%d" % r["n"] if "n" in r else "") for r in records)
        out.append("  %-24s %s%s; %s" % (item["kind"], where, " [%s]" % named if named else "",
                                         "; ".join(flag for flag in flags if flag)))
        if item["bytes"]["external"] or item["bytes"]["host"]:
            out.append("      external %r | host %r" % (item["bytes"]["external"], item["bytes"]["host"]))
        cross = item["timing"].get("cross")
        if cross:
            out.append("      timing %s" % json.dumps(cross, sort_keys=True))
    if len(report["findings"]) < sum(report["counts"].values()):
        out.append("  ... %d more finding(s) counted, not listed" % (
            sum(report["counts"].values()) - len(report["findings"])))
    cross = report["timing"]["cross"]
    out.append("timing: cross-device %s%s" % (cross["status"], ": " + cross["reason"] if "reason" in cross else
                                              " by " + ", ".join(cross["basis"])))
    for marker in report["timing"]["markers"]:
        out.append("  marker %r: %s" % (marker["text"], marker["status"]))
    return "\n".join(out) + "\n"


# ---------------------------------------------------------------- command line
def _count(text):
    try:
        value = int(text, 0)
    except ValueError:
        raise argparse.ArgumentTypeError("not an integer: %r" % text) from None
    return value


def _bounds(text):
    low, separator, high = text.partition(":")
    try:
        if not separator:
            raise ValueError(text)
        low, high = float(low), float(high)
    except ValueError:
        raise argparse.ArgumentTypeError("--offset-bounds needs LO:HI seconds, not %r" % text) from None
    if not (math.isfinite(low) and math.isfinite(high)) or low > high:
        raise argparse.ArgumentTypeError("--offset-bounds needs finite LO <= HI, not %r" % text)
    return low, high


def parser():
    result = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    result.add_argument("--format", required=True, choices=FORMATS,
                        help="Capture format of both byte files; only raw decoded bytes are supported")
    result.add_argument("--host", required=True, type=Path, help="Host capture, e.g. trial-01/rx.bin or rx.bin.xz")
    result.add_argument("--external", required=True, type=Path, help="Independent capture of GPIO43")
    result.add_argument("--host-times", type=Path, help="Host chunk times CSV (rx.times.csv shape)")
    result.add_argument("--external-times", type=Path, help="External chunk times CSV (rx.times.csv shape)")
    result.add_argument("--time-column", choices=TIME_COLUMNS, default="monotonic_s",
                        help="Times column on both sides (default monotonic_s)")
    result.add_argument("--marker", action="append", default=[],
                        help="Text found once in each stream that relates the clocks; repeatable")
    result.add_argument("--offset-bounds", type=_bounds,
                        help="Stated LO:HI seconds for host clock minus external clock (0:0 for one clock)")
    result.add_argument("--max-bytes", type=_count, default=DEFAULT_MAX_BYTES,
                        help="Refuse a larger input, stored or decompressed (default %d, at most %d)" % (
                            DEFAULT_MAX_BYTES, HARD_MAX_BYTES))
    result.add_argument("--max-lines", type=_count, default=DEFAULT_MAX_LINES,
                        help="Refuse an input with more lines (default %d, at most %d)" % (
                            DEFAULT_MAX_LINES, HARD_MAX_LINES))
    result.add_argument("--window-bytes", type=_count, default=DEFAULT_WINDOW_BYTES,
                        help="Largest window diffed per side (default %d, 256..%d)" % (
                            DEFAULT_WINDOW_BYTES, MAX_WINDOW_BYTES))
    result.add_argument("--diff-budget", type=_count, default=DEFAULT_DIFF_BUDGET,
                        help="Total bytes diffed before the rest is reported unaligned (default %d)" %
                             DEFAULT_DIFF_BUDGET)
    result.add_argument("--json", help="New file for the JSON report, never overwritten; - prints it instead")
    return result


def main(argv=None):
    args_parser = parser()
    args = args_parser.parse_args(argv)
    if not 1 <= args.max_bytes <= HARD_MAX_BYTES:
        args_parser.error("--max-bytes must be 1..%d" % HARD_MAX_BYTES)
    if not 1 <= args.max_lines <= HARD_MAX_LINES:
        args_parser.error("--max-lines must be 1..%d" % HARD_MAX_LINES)
    if not 256 <= args.window_bytes <= MAX_WINDOW_BYTES:
        args_parser.error("--window-bytes must be 256..%d" % MAX_WINDOW_BYTES)
    if args.diff_budget < 0:
        args_parser.error("--diff-budget must not be negative")
    if len(args.marker) > MAX_MARKERS or any(not text for text in args.marker):
        args_parser.error("--marker takes 1..%d nonempty texts" % MAX_MARKERS)
    inputs = {Path(p).resolve() for p in (args.host, args.external, args.host_times, args.external_times) if p}
    if args.json and args.json != "-":
        target = Path(args.json)
        if target.exists():
            args_parser.error("--json output already exists: %s" % target)
        if target.resolve() in inputs:
            args_parser.error("--json must not name an input")
    host = None
    try:
        data, info = read_capture(args.host, args.max_bytes, args.max_lines, "host")
        times = read_times(args.host_times, args.time_column, len(data), "host") if args.host_times else None
        host = Side("host", data, info, times)
        data, info = read_capture(args.external, args.max_bytes, args.max_lines, "external")
        times = (read_times(args.external_times, args.time_column, len(data), "external")
                 if args.external_times else None)
        ext = Side("external", data, info, times)
    except Unavailable as error:
        # Only the external side raises this, so the host is loaded and still worth summarizing.
        print("s2_uart_compare: %s" % error, file=sys.stderr)
        _emit(host_only_report(host, str(error)), args.json)
        return error.status
    except InputError as error:
        print("s2_uart_compare: %s" % error, file=sys.stderr)
        return error.status
    comparison = Comparison(host, ext, args.window_bytes, args.diff_budget).run()
    status, report = build_report(host, ext, comparison, {
        "format": args.format, "max_bytes": args.max_bytes, "max_lines": args.max_lines,
        "diff_budget": args.diff_budget}, args.marker, args.offset_bounds)
    _emit(report, args.json)
    return status


def _emit(report, destination):
    text = json.dumps(report, indent=2) + "\n"
    if destination == "-":
        sys.stdout.write(text)
        return
    sys.stdout.write(render(report))
    if destination:
        with Path(destination).open("x", encoding="utf-8", newline="\n") as output:
            output.write(text)


if __name__ == "__main__":
    raise SystemExit(main())
