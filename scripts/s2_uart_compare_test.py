#!/usr/bin/env python3
"""Independent UART capture against host rx.bin: synthetic edits and retained bench bursts.

No port or hardware. Retained captures under docs/bench are only read (the xz one is
decompressed in memory); every "external" stream built from them is labelled with how it
was made, because no independent capture of those runs exists.
"""

import contextlib
import hashlib
import io
import json
import lzma
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_evidence as evidence  # noqa: E402
import s2_uart_compare as compare  # noqa: E402


ROOT = evidence.ROOT
BENCH = ROOT / "docs/bench"
PLAIN3 = BENCH / "20261104-soak-ab-plain3/trial-01/rx.bin"
R1 = BENCH / "20261105-stress-r1-summary1s/trial-01/rx.bin.xz"
R1_RX_SHA256 = "32323abc1152394ea84d1716b1d2e56d6cd4103b40918bbed5598722a3e3923d"  # COMPRESSED.json
SCRIPT = Path(__file__).resolve().parent / "s2_uart_compare.py"


def begin_line():
    return ("SOAK_BEGIN n=0 t=39765465 trial=1793807290 dur=10799 interval=20 summary=60 monitors=1 identity=1 "
            "opDeadline=2000000 queue=250000 resp=250000 stack=9840 heap=127876 heapMin=123436 "
            "base=e7f28acd8a3a4d983f337aee6150c400fb871dc0")


def burst(n, t):
    """One summary burst in the firmware's format, numbered from n."""
    return [
        ("SOAK_SUM n=%d t=%d trial=1793807290 up=%d rx=%d tx=%d txRej=0 rxDrop=0 rxLost=0 unmatched=0 qRej=0 "
         "resFull=0 timeouts=0 cancelled=0 busOff=0 recov=0 twaiErr=0 twaiFlags=0 twaiDrop=0") % (
            n, t, t - 39000000, 7237 + n * 600, 7237 + n * 600),
        ("SOAK_GAP n=%d t=%d gapMax=4793 gapWin=3860 ownerGap=5192 qHw=1 actHw=1 retHw=1 trHw=3 trOver=0 "
         "netDrop=0 stack=8976 heap=127876 heapMin=123436 tec=0 rec=0") % (n + 1, t + 3660),
        ("SOAK_CON n=%d t=%d lines=%d blocked=254 conRej=0 conHw=496 evFull=0 evLimit=0 evHw=49 anomalies=0 "
         "dumps=0 dumpSkip=0 dumpAb=0 trLost=0 dumpUs=0 defDump=0 defBusy=11133 defTx=0 defInh=0 slips=775 "
         "sdkDefer=0") % (n + 2, t + 7154, n + 2),
        ("SOAK_NODE n=%d t=%d node=1 ops=1033 opsOk=1033 opsFail=0 refused=0 steps=3621 stepsOk=3621 min=4820 "
         "mean=4882 max=5917 retireMax=4606 earlyReply=0 hb=0 hbGapMax=0 stopped=0") % (n + 3, t + 17000),
        "SOAK_HIST n=%d t=%d node=1 b=0,3613,9,0,0,0,0,0" % (n + 4, t + 48000),
        ("SOAK_OUT n=%d t=%d node=1 sdo=3623,0,0,0,0,0,0,0,0,0,0,0 reason=3623,0,0,0,0,0,0,0 "
         "read=0,1033,0,0,0,0,0,0,0") % (n + 5, t + 59000),
        "SOAK_ABORT n=%d t=%d node=1 a0=00000000:0 a1=00000000:0 a2=00000000:0 a3=00000000:0 other=0" % (
            n + 6, t + 79000),
    ]


def stream_lines(bursts=6):
    lines = [begin_line()]
    for k in range(bursts):
        lines += burst(1 + 7 * k, 99763803 + 60000000 * k)
    return lines


def stream(bursts=6):
    return "".join(line + "\n" for line in stream_lines(bursts)).encode("ascii")


def line_start(data, number):
    """Byte offset of 1-based line number."""
    offset = 0
    for _ in range(number - 1):
        offset = data.index(b"\n", offset) + 1
    return offset


def times_csv(chunks):
    """rx.times.csv text for [(length, monotonic_s)] chunks."""
    rows, offset = ["offset,bytes,utc,monotonic_s,in_waiting"], 0
    for length, value in chunks:
        rows.append("%d,%d,2026-11-04T15:48:11.192720+00:00,%.6f,0" % (offset, length, value))
        offset += length
    return "\n".join(rows) + "\n"


def retained(path):
    """Read a retained capture after checking its manifest hash; never writes under docs/bench."""
    data = path.read_bytes()
    manifest = json.loads((BENCH / "SHA256SUMS.json").read_text(encoding="utf-8"))
    relative = path.relative_to(BENCH).as_posix()
    if hashlib.sha256(data).hexdigest() != manifest[relative]:
        raise AssertionError("retained capture %s does not match its manifest" % relative)
    return data


def lines_of(data):
    lines = data.split(b"\n")
    starts, offset = [], 0
    for line in lines:
        starts.append(offset)
        offset += len(line) + 1
    return lines, starts


class Runner(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def write(self, name, content):
        path = self.root / name
        if isinstance(content, str):
            path.write_text(content, encoding="utf-8", newline="\n")
        else:
            path.write_bytes(content)
        return path

    def run_tool(self, host, external, *extra, host_times=None, external_times=None):
        """In-process CLI run; returns (status, report or None, stdout, stderr)."""
        paths = {"host.bin": host, "external.bin": external}
        argv = ["--format", "raw"]
        for name, content in paths.items():
            if content is None:
                argv += ["--" + name[:-4], str(self.root / ("absent-" + name))]
            else:
                argv += ["--" + name[:-4], str(self.write(name, content))]
        if host_times is not None:
            argv += ["--host-times", str(self.write("host.times.csv", host_times))]
        if external_times is not None:
            argv += ["--external-times", str(self.write("external.times.csv", external_times))]
        report_path = self.root / ("report-%d.json" % len(list(self.root.glob("report-*.json"))))
        argv += ["--json", str(report_path)] + list(extra)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                status = compare.main(argv)
            except SystemExit as exit_:
                status = exit_.code
        report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.exists() else None
        return status, report, out.getvalue(), err.getvalue()

    def findings(self, report):
        return [item for item in report["findings"]]


class SyntheticEdits(Runner):
    def test_fixture_lines_parse_in_the_firmware_grammar(self):
        records, tail = evidence.parse_bytes(stream())
        self.assertIsNone(tail)
        self.assertEqual({r.status for r in records}, {"parsed"})

    def test_identical_streams(self):
        data = stream()
        status, report, text, _ = self.run_tool(data, data)
        self.assertEqual(status, compare.IDENTICAL)
        self.assertEqual(report["verdict"], "identical")
        self.assertEqual(report["findings"], [])
        self.assertIsNone(report["first_divergence"])
        self.assertEqual(report["coverage"]["host_identical"], len(data))
        self.assertEqual(report["parser"]["host"]["numbering_gaps"], 0)
        self.assertIn("IDENTICAL (exit 0)", text)

    def test_literal_64_byte_deletion_inside_one_line(self):
        external = stream()
        # Line 10 is SOAK_GAP n=9; cut after "gapWin=" exactly as the 2026-11-04 17:19 loss.
        start = line_start(external, 10)
        cut = start + external[start:].index(b"gapWin=") + len(b"gapWin=")
        removed = external[cut:cut + 64]
        self.assertEqual(removed, b"3860 ownerGap=5192 qHw=1 actHw=1 retHw=1 trHw=3 trOver=0 netDrop")
        host = external[:cut] + external[cut + 64:]
        status, report, text, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual(item["kind"], "deletion")
        self.assertEqual((item["external"]["offset"], item["external"]["length"]), (cut, 64))
        self.assertEqual((item["host"]["offset"], item["host"]["length"]), (cut, 0))
        self.assertEqual(item["location"], "exact")
        self.assertTrue(item["partial_line"])
        self.assertFalse(item["joins_lines"])
        self.assertEqual(item["external"]["lines"], [10, 10])
        self.assertEqual(item["external"]["records"][0]["kind"], "SOAK_GAP")
        self.assertEqual(item["external"]["records"][0]["n"], 9)
        self.assertEqual(report["first_divergence"]["external_offset"], cut)
        self.assertEqual(report["first_divergence"]["host_line"], 10)
        # The host's damage is what the evidence parser sees; the external side is clean.
        self.assertEqual([(c["line"], c["status"]) for c in report["parser"]["host"]["corrupt"]], [(10, "malformed")])
        self.assertEqual(report["parser"]["host"]["gaps"], [{"family": "soak", "line": 11, "expected": 9, "found": 10}])
        self.assertEqual(report["parser"]["external"]["corrupt"], [])
        self.assertIn("deletion", text)

    def test_partial_line_deletion_joining_two_lines(self):
        external = stream()
        start = line_start(external, 5)
        end_of_line = external.index(b"\n", start)
        cut = end_of_line - 25
        host = external[:cut] + external[cut + 40:]
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual(item["kind"], "deletion")
        self.assertEqual(item["external"]["length"], 40)
        self.assertTrue(item["partial_line"])
        self.assertTrue(item["joins_lines"])
        self.assertEqual(item["external"]["lines"], [5, 6])
        low, high = item["equivalent_starts"]["external"]
        self.assertTrue(low <= cut <= high)

    def test_whole_line_deletion_reports_the_line_aligned_start_beside_the_interval(self):
        external = stream()
        start, end = line_start(external, 8), line_start(external, 9)
        host = external[:start] + external[end:]
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual((item["kind"], item["external"]["length"]), ("deletion", end - start))
        self.assertFalse(item["partial_line"])
        self.assertEqual(item["line_aligned_starts"]["external"], [start])
        low, high = item["equivalent_starts"]["external"]
        self.assertTrue(low <= start <= high)

    def test_repeated_identical_chunks_report_ambiguity_not_a_location(self):
        chunk = b"REPEAT " + b"x" * 56 + b"\n"
        self.assertEqual(len(chunk), 64)
        head, tail = b"HEADER unique first line\n", b"TRAILER unique last line\n"
        external = head + chunk * 8 + tail
        host = head + chunk * 7 + tail
        status, report, text, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual((item["kind"], item["external"]["length"]), ("deletion", 64))
        self.assertEqual(item["location"], "ambiguous")
        # Any of the eight copies, or the run shifted by its newline, is the same deletion.
        self.assertEqual(item["equivalent_starts"]["external"], [len(head) - 1, len(head) + 7 * 64])
        self.assertEqual(item["line_aligned_starts"]["external"], [len(head) + 64 * k for k in range(4)])
        first = report["first_divergence"]
        self.assertEqual(first["external_offset"], len(head) + 7 * 64)
        self.assertEqual(first["edit_may_start_at"]["external"], len(head) - 1)
        self.assertIn("ambiguous", text)
        self.assertNotIn("exact", text.split("findings")[1])

    def test_deletion_and_corruption_among_identical_lines_stay_ambiguous(self):
        chunk = b"REPEAT " + b"x" * 56 + b"\n"
        damaged = chunk[:20] + b"Z" + chunk[21:]
        head, tail = b"HEADER unique first line\n", b"TRAILER unique last line\n"
        external = head + chunk * 4 + tail
        host = head + chunk + damaged + chunk + tail  # One copy lost, one altered: which is which is unknowable.
        status, report, text, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual(item["kind"], "replacement")
        self.assertEqual((item["external"]["length"], item["host"]["length"]), (65, 1))
        self.assertEqual(item["location"], "ambiguous")
        self.assertEqual(item["equivalent_span"]["external"], [len(head) - 1, len(head) + 4 * 64])
        self.assertIn("within external", text)

    def test_inserted_novel_bytes(self):
        external = stream()
        point = line_start(external, 12) + 30
        noise = b"\x00\x01INSERTED-NOT-IN-STREAM"
        host = external[:point] + noise + external[point:]
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual(item["kind"], "insertion")
        self.assertEqual((item["host"]["offset"], item["host"]["length"]), (point, len(noise)))
        self.assertEqual(item["external"]["length"], 0)
        self.assertNotIn("source", item)
        self.assertEqual(report["parser"]["host"]["corrupt"][0]["kind"], "CORRUPT")

    def test_duplicated_line_from_earlier_names_its_source(self):
        external = stream()
        copied = external[line_start(external, 3):line_start(external, 4)]
        point = line_start(external, 9)
        host = external[:point] + copied + external[point:]
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual(item["kind"], "duplication")
        self.assertEqual(item["host"]["length"], len(copied))
        low, high = item["equivalent_starts"]["host"]
        self.assertTrue(low <= point <= high)
        offset = item["host"]["offset"]
        self.assertEqual(host[offset:offset + len(copied)], external[item["source"]["external_offsets"][0]:][
            :len(copied)])
        self.assertTrue(item["source"]["unique"])
        self.assertLess(item["source"]["relative_to_external_offset"][0], 0)

    def test_adjacent_duplicate_cannot_say_which_copy_is_extra(self):
        external = stream()
        start, end = line_start(external, 6), line_start(external, 7)
        host = external[:end] + external[start:end] + external[end:]
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual(item["kind"], "duplication")
        self.assertEqual(item["location"], "ambiguous")
        low, high = item["equivalent_starts"]["host"]
        self.assertLessEqual(low, start)
        self.assertGreaterEqual(high, end)

    def test_byte_corruption_in_place(self):
        external = stream()
        start = line_start(external, 17)
        position = start + external[start:].index(b"stack=8976") + len(b"stack=89")
        host = bytearray(external)
        host[position] = ord("1")
        host[position + 40] = 0xFF
        status, report, _, _ = self.run_tool(bytes(host), external)
        self.assertEqual(status, compare.DIVERGENT)
        corruptions = [item for item in report["findings"] if item["kind"] == "corruption"]
        self.assertEqual(len(report["findings"]), 2)
        self.assertEqual(len(corruptions), 2)
        self.assertEqual(corruptions[0]["differences"], [{"external_offset": position, "host_offset": position,
                                                          "external_byte": "37", "host_byte": "31"}])
        self.assertEqual(corruptions[1]["differences"][0]["host_byte"], "ff")
        # The 0xFF makes the line undecodable for the evidence parser.
        self.assertEqual(report["parser"]["host"]["corrupt"][0]["kind"], "CORRUPT")

    def test_truncated_host_capture_is_a_missing_tail(self):
        external = stream()
        cut = line_start(external, 30) + 17
        status, report, text, _ = self.run_tool(external[:cut], external)
        self.assertEqual(status, compare.DIVERGENT)
        self.assertEqual(report["tail"], {"kind": "host_missing_tail", "external_only_bytes": len(external) - cut,
                                          "host_only_bytes": 0, "host_ends_mid_line": True,
                                          "external_ends_mid_line": False,
                                          "timing": {"status": "not_established",
                                                     "reason": "times missing for host and external; no shared "
                                                               "marker or stated offset bounds"}})
        self.assertEqual(report["parser"]["host"]["corrupt"][-1]["reason"], "no line terminator")
        self.assertIn("host_missing_tail", text)

    def test_external_capture_ending_first_leaves_the_host_tail_unverified(self):
        host = stream()
        cut = line_start(host, 30) + 17
        status, report, _, _ = self.run_tool(host, host[:cut])
        self.assertEqual(status, compare.INCONCLUSIVE)
        self.assertEqual(report["tail"]["kind"], "external_ended_first")
        self.assertEqual(report["coverage"]["host_unverified"], len(host) - cut)

    def test_capture_heads(self):
        data = stream()
        banner = b"ESP-ROM:esp32s2-rc4-20191025\nrst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)\n"
        status, report, _, _ = self.run_tool(data, banner + data)
        self.assertEqual(status, compare.IDENTICAL)
        self.assertEqual(report["head"], {"external_only_bytes": len(banner), "host_unverified_bytes": 0})
        late = line_start(data, 3) + 11  # External armed mid-line, after the host started.
        status, report, _, _ = self.run_tool(data, data[late:])
        self.assertEqual(status, compare.INCONCLUSIVE)
        self.assertEqual(report["head"], {"external_only_bytes": 0, "host_unverified_bytes": late})
        self.assertIsNone(report["first_divergence"])

    def test_numbered_anchors_split_a_window_with_no_identical_line(self):
        external = stream(3)
        # Every line changed in place but still parseable: only the n=/t= anchors remain.
        host = external.replace(b"rec=0", b"rec=1").replace(b"other=0", b"other=1").replace(
            b"twaiDrop=0", b"twaiDrop=1").replace(b"sdkDefer=0", b"sdkDefer=1").replace(
            b"stopped=0", b"stopped=1").replace(b",0,0,0,0,0\n", b",0,0,0,0,1\n").replace(
            b"read=0,1033,0,0,0,0,0,0,0", b"read=0,1033,0,0,0,0,0,0,1")
        changed = sum(1 for a, b in zip(external.split(b"\n"), host.split(b"\n")) if a != b)
        self.assertEqual(changed, 21)
        status, report, _, _ = self.run_tool(host, external, "--window-bytes", "1024")
        self.assertEqual(status, compare.DIVERGENT)
        self.assertEqual(report["alignment"]["anchors_numbered"], 21)
        # The last line ends the stream on both sides, where one shared newline does not show that
        # the captures end at the same byte: a tail divergence, not an in-place corruption.
        self.assertEqual(report["counts"], {"corruption": 20, "tail_divergence": 1})
        self.assertEqual(report["limits"]["hit"], {})

    def test_window_cap_reports_unaligned_and_unalignable(self):
        top, bottom = b"TOP unique anchor line\n", b"BOTTOM unique anchor line\n"
        mine = b"".join(b"host line %05d\n" % k for k in range(600))
        theirs = b"".join(b"wire line %05d\n" % k for k in range(600))
        status, report, _, _ = self.run_tool(top + mine + bottom, top + theirs + bottom, "--window-bytes", "4096")
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual((item["kind"], item["limit"], item["location"]), ("unaligned", "window_cap", "bounded"))
        self.assertEqual(report["limits"]["hit"], {"window_cap": 1})
        # No anchor at all: the coincidental common end " line 00599\n" must not align anything.
        status, report, _, _ = self.run_tool(mine, theirs, "--window-bytes", "4096")
        self.assertEqual(status, compare.INCONCLUSIVE)
        [item] = report["findings"]
        self.assertEqual((item["kind"], item["limit"]), ("unalignable", "no_defensible_anchor"))
        self.assertIsNone(report["head"])
        self.assertTrue(report["alignment"]["no_defensible_anchor"])
        self.assertEqual(report["coverage"]["host_unverified"], len(mine))

    def test_findings_rebuild_the_host_capture_from_the_external_one(self):
        """Property over seeded random edits: offsets and lengths are exact on both sides, and a
        single deletion's true position lies inside its reported interval. Edits start after the
        first 64 bytes: at a capture's very start a loss cannot be told from a later start."""
        generator = random.Random(20261106)
        chunk = b"STATS running=1 inhibited=0 rx=5 tx=5 dropped=0 errors=0 flags=0 stackFree=9000\n"
        self.assertEqual(evidence.parse_line(1, chunk.decode().strip()).status, "parsed")
        for case in range(60):
            external = bytearray(stream())
            if case % 3 == 0:
                at = line_start(bytes(external), generator.randrange(2, 40))
                external[at:at] = chunk * generator.randrange(2, 6)
            external = bytes(external)
            host = bytearray(external)
            single = None
            for _ in range(1 if case % 2 else generator.randrange(2, 4)):
                at = generator.randrange(64, len(host) - 200)
                operation = generator.choice(("delete", "insert", "substitute", "duplicate"))
                if operation == "delete":
                    length = generator.choice((1, 7, 35, 64, 128))
                    del host[at:at + length]
                    single = (at, length) if case % 2 else None
                elif operation == "insert":
                    host[at:at] = bytes(generator.randrange(32, 127) for _ in range(generator.choice((1, 5, 20))))
                elif operation == "substitute":
                    for k in range(generator.choice((1, 2, 32))):
                        host[at + k] = generator.randrange(32, 127)
                else:
                    source = generator.randrange(64, len(host) - 200)
                    host[at:at] = host[source:source + generator.choice((16, 64))]
            host = bytes(host)
            with self.subTest(case=case):
                status, report, _, _ = self.run_tool(host, external)
                self.assertEqual(status, compare.IDENTICAL if host == external else compare.DIVERGENT)
                rebuilt, position = bytearray(), 0
                for item in report["findings"]:
                    rebuilt += external[position:item["external"]["offset"]]
                    self.assertEqual(len(rebuilt), item["host"]["offset"])
                    rebuilt += host[item["host"]["offset"]:item["host"]["offset"] + item["host"]["length"]]
                    position = item["external"]["offset"] + item["external"]["length"]
                rebuilt += external[position:]
                self.assertEqual(bytes(rebuilt), host)
                if single and host != external:
                    [item] = report["findings"]
                    low, high = item["equivalent_starts"]["external"]
                    self.assertTrue(low <= single[0] <= high and item["external"]["length"] == single[1])

    def test_diff_budget_and_item_limit_never_guess(self):
        external = stream()
        cut = line_start(external, 10) + 40
        host = external[:cut] + external[cut + 64:]
        status, report, _, _ = self.run_tool(host, external, "--diff-budget", "0")
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual((item["kind"], item["limit"]), ("unaligned", "diff_budget"))
        host = bytearray(external)
        for line in (5, 10, 15, 20, 25):
            host[line_start(external, line) + 12] = ord("9")
        with mock.patch.object(compare, "MAX_ITEMS", 3):
            status, report, _, _ = self.run_tool(bytes(host), external)
        self.assertEqual(status, compare.DIVERGENT)
        self.assertTrue(report["limits"]["stopped_at_max_items"])
        self.assertEqual(report["counts"], {"corruption": 3, "not_examined": 1})
        self.assertGreater(report["coverage"]["host_unverified"], 0)


class Clocks(Runner):
    OFFSET = 1000.0

    def build(self):
        """External chunks are lines 10 ms apart; the host sees each 2 ms later on a clock 1000 s ahead,
        except burst 3 (lines 16..22), which it sees 52 ms later. Line 18 loses 64 bytes."""
        lines = [line.encode("ascii") + b"\n" for line in stream_lines()]
        ext_chunks, host_chunks, host_parts = [], [], []
        for index, line in enumerate(lines, 1):
            moment = 10.0 + 0.010 * index
            ext_chunks.append((len(line), moment))
            delay = 0.052 if 16 <= index <= 22 else 0.002
            part = line[:40] + line[104:] if index == 18 else line
            host_parts.append(part)
            host_chunks.append((len(part), moment + self.OFFSET + delay))
        return (b"".join(host_parts), b"".join(lines), times_csv(host_chunks), times_csv(ext_chunks))

    def test_marker_relates_the_clocks_as_a_delay_change(self):
        host, external, host_times, ext_times = self.build()
        status, report, text, _ = self.run_tool(host, external, "--marker", "SOAK_BEGIN n=0",
                                                host_times=host_times, external_times=ext_times)
        self.assertEqual(status, compare.DIVERGENT)
        [marker] = report["timing"]["markers"]
        self.assertEqual(marker["status"], "ok")
        self.assertEqual(marker["host_minus_external_s"], round(self.OFFSET + 0.002, 6))
        [item] = report["findings"]
        cross = item["timing"]["cross"]
        self.assertEqual(cross["delay_change_since_marker_s"], 0.05)
        self.assertEqual(cross["external_elapsed_since_marker_s"], 0.17)
        self.assertNotIn("host_minus_external_observation_s", cross)
        self.assertEqual(report["timing"]["cross"], {"status": "established", "basis": ["markers"]})
        # Same-clock steps stay available on each side.
        self.assertEqual(item["timing"]["external"]["since_previous_chunk_s"], 0.01)
        self.assertIn("delay_change_since_marker_s", text)
        # The nearest marker before the finding in the stream is used, whatever the argument order.
        later = "SOAK_SUM n=8 t=159763803"
        status, report, _, _ = self.run_tool(host, external, "--marker", later, "--marker", "SOAK_BEGIN n=0",
                                             host_times=host_times, external_times=ext_times)
        cross = report["findings"][0]["timing"]["cross"]
        self.assertEqual((cross["marker"], cross["external_elapsed_since_marker_s"]), (later, 0.09))
        self.assertEqual(cross["delay_change_since_marker_s"], 0.05)

    def test_stated_bounds_give_an_interval_only(self):
        host, external, host_times, ext_times = self.build()
        status, report, _, _ = self.run_tool(host, external, "--offset-bounds", "999.9:1000.1",
                                             host_times=host_times, external_times=ext_times)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        low, high = item["timing"]["cross"]["host_minus_external_observation_s"]
        self.assertEqual((low, high), (round(0.052 - 0.1, 6), round(0.052 + 0.1, 6)))
        self.assertNotIn("delay_change_since_marker_s", item["timing"]["cross"])

    def test_without_marker_or_bounds_no_cross_device_figure(self):
        host, external, host_times, ext_times = self.build()
        status, report, text, _ = self.run_tool(host, external, host_times=host_times, external_times=ext_times)
        self.assertEqual(status, compare.DIVERGENT)
        self.assertEqual(report["timing"]["cross"]["status"], "not_established")
        [item] = report["findings"]
        self.assertNotIn("cross", item["timing"])
        self.assertEqual(item["timing"]["host"]["time"], round(10.0 + 0.18 + self.OFFSET + 0.052, 6))
        self.assertIn("not_established", text)

    def test_missing_repeated_or_misaligned_marker_establishes_nothing(self):
        ext_lines = [line.encode("ascii") + b"\n" for line in stream_lines()]
        moved = ext_lines[4]  # Unique on both sides, but the host has it 15 lines later.
        host_lines = ext_lines[:4] + ext_lines[5:20] + [moved] + ext_lines[20:]
        ext_times = times_csv([(len(line), 10.0 + 0.01 * k) for k, line in enumerate(ext_lines)])
        host_times = times_csv([(len(line), 1010.0 + 0.01 * k) for k, line in enumerate(host_lines)])
        status, report, _, _ = self.run_tool(
            b"".join(host_lines), b"".join(ext_lines), "--marker", "NOT IN EITHER STREAM", "--marker", "SOAK_SUM n=",
            "--marker", moved.decode("ascii").strip(), host_times=host_times, external_times=ext_times)
        self.assertEqual(status, compare.DIVERGENT)
        self.assertEqual([m["status"] for m in report["timing"]["markers"]],
                         ["missing_host", "ambiguous_host", "misaligned"])
        self.assertEqual(report["timing"]["cross"], {"status": "not_established", "reason": "no usable marker"})
        # The moved line is one reorder; which side of it moved cannot be told.
        self.assertEqual([item["kind"] for item in report["findings"]], ["reorder"])
        for item in report["findings"]:
            self.assertNotIn("cross", item["timing"])

    def test_marker_needs_times_on_both_sides(self):
        data = stream()
        status, report, _, _ = self.run_tool(data, data, "--marker", "SOAK_BEGIN n=0")
        self.assertEqual(status, compare.IDENTICAL)
        self.assertEqual(report["timing"]["markers"][0]["status"], "untimed")
        self.assertIn("times missing", report["timing"]["cross"]["reason"])

    def test_invalid_times_are_refused(self):
        data = stream()
        cases = {"gap": times_csv([(10, 1.0)]) + "20,5,x,2.0,0\n",
                 "too_long": times_csv([(len(data) + 1, 1.0)]),
                 "column": "offset,bytes\n0,%d\n" % len(data),
                 "time": times_csv([(len(data), 1.0)]).replace("1.000000", "nan")}
        for name, text in cases.items():
            with self.subTest(name):
                status, report, _, error = self.run_tool(data, data, host_times=text)
                self.assertEqual(status, compare.INPUT_ERROR)
                self.assertIsNone(report)
                self.assertIn("host times", error)


class Inputs(Runner):
    def test_oversized_input_is_refused_before_reading(self):
        data = stream()
        status, report, _, error = self.run_tool(data, data, "--max-bytes", str(len(data) - 1))
        self.assertEqual(status, compare.INPUT_ERROR)
        self.assertIsNone(report)
        self.assertIn("refused above --max-bytes", error)
        bomb = lzma.compress(b"\0" * 2_000_000, format=lzma.FORMAT_XZ)
        path = self.write("bomb.bin.xz", bomb)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = compare.main(["--format", "raw", "--host", str(path), "--external", str(path),
                                   "--max-bytes", "100000"])
        self.assertEqual(status, compare.INPUT_ERROR)
        self.assertIn("decompresses beyond --max-bytes", err.getvalue())

    def test_unavailable_external_capture(self):
        data = stream()
        status, report, text, error = self.run_tool(data, None)
        self.assertEqual(status, compare.UNAVAILABLE)
        self.assertEqual(report["verdict"], "external_unavailable")
        self.assertIn("does not exist", report["error"])
        for needed in ("GPIO43", "GND", "115200 8N1", "--format raw"):
            self.assertIn(needed, text)
        self.assertEqual(report["parser"]["host"]["parsed"], len(stream_lines()))
        status, report, text, _ = self.run_tool(data, b"")
        self.assertEqual(status, compare.UNAVAILABLE)
        self.assertIn("holds no bytes", report["error"])

    def test_host_errors_and_arguments(self):
        data = stream()
        status, _, _, error = self.run_tool(None, data)
        self.assertEqual(status, compare.INPUT_ERROR)
        self.assertIn("host capture", error)
        external = self.write("external.bin", data)
        for argv in (["--host", str(external), "--external", str(external)],
                     ["--format", "csv", "--host", str(external), "--external", str(external)],
                     ["--format", "raw", "--host", str(external), "--external", str(external), "--json",
                      str(external)],
                     ["--format", "raw", "--host", str(external), "--external", str(external),
                      "--offset-bounds", "2:1"]):
            with self.subTest(argv), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as exit_:
                    compare.main(argv)
                self.assertEqual(exit_.exception.code, 2)
        self.assertEqual(external.read_bytes(), data)

    def test_xz_container_reads_the_same_bytes(self):
        data = stream()
        packed = self.write("host.bin.xz", lzma.compress(data, format=lzma.FORMAT_XZ))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = compare.main(["--format", "raw", "--host", str(packed), "--external",
                                   str(self.write("plain.bin", data)), "--json", "-"])
        report = json.loads(out.getvalue())
        self.assertEqual(status, compare.IDENTICAL)
        self.assertEqual(report["inputs"]["host"]["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(report["inputs"]["host"]["stored"]["container"], "xz")

    def test_inputs_unchanged_and_output_deterministic(self):
        external = stream()
        host = external[:500] + external[564:]
        paths = [self.write("h.bin", host), self.write("e.bin", external)]
        before = [(p.read_bytes(), os.stat(p).st_mtime_ns) for p in paths]
        reports = []
        for name in ("first.json", "second.json"):
            with contextlib.redirect_stdout(io.StringIO()):
                compare.main(["--format", "raw", "--host", str(paths[0]), "--external", str(paths[1]),
                              "--json", str(self.root / name)])
            reports.append((self.root / name).read_bytes())
        self.assertEqual(reports[0], reports[1])
        self.assertEqual(before, [(p.read_bytes(), os.stat(p).st_mtime_ns) for p in paths])
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            compare.main(["--format", "raw", "--host", str(paths[0]), "--external", str(paths[1]),
                          "--json", str(self.root / "first.json")])
        self.assertEqual((self.root / "first.json").read_bytes(), reports[0])

    def test_command_line_help_documents_connection_and_format(self):
        result = subprocess.run([sys.executable, str(SCRIPT), "--help"], capture_output=True, text=True,
                                timeout=60, check=False)
        self.assertEqual(result.returncode, 0)
        for needed in ("GPIO43", "GND", "115200 baud, 8 data bits, no parity, 1 stop bit", "--format",
                       "offset, bytes", "monotonic_s", "Exit status"):
            self.assertIn(needed, result.stdout)


def junk(length, seed=5):
    """What a receiver at the wrong baud rate might record: no text in common with the stream."""
    generator = random.Random(seed)
    return bytes(generator.choice(b"\x00\x80\xf0\xfe\x0a") for _ in range(length))


def per_line_times(data, base, step=0.01, late=None):
    """One chunk per line at base + step * line number; late maps line number -> extra seconds."""
    chunks = []
    for number, line in enumerate(data.split(b"\n")[:-1], 1):
        chunks.append((len(line) + 1, base + step * number + (late or {}).get(number, 0.0)))
    return times_csv(chunks)


class ReviewRegressions(Runner):
    """Cases from the independent review of 2026-11-06, each once wrong."""

    def test_competing_anchor_chains_are_contested(self):
        pairs = [(0, 0, True), (1, 2, True), (2, 1, True), (3, 3, True)]
        chain = compare.longest_chain(pairs)
        self.assertEqual(len(chain), 3)
        self.assertEqual([pair[3] for pair in chain], [False, True, False])
        self.assertEqual([pair[3] for pair in compare.longest_chain([(0, 0, True), (1, 1, True), (2, 2, True)])],
                         [False, False, False])

    def test_swapped_adjacent_lines_are_one_ambiguous_reorder(self):
        external = stream()
        a, b, c = line_start(external, 8), line_start(external, 9), line_start(external, 10)
        host = external[:a] + external[b:c] + external[a:b] + external[c:]
        status, report, text, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual((item["kind"], item["location"]), ("reorder", "ambiguous"))
        self.assertEqual((item["external"]["offset"], item["external"]["length"]), (a, c - a))
        self.assertEqual((item["host"]["offset"], item["host"]["length"]), (a, c - a))
        self.assertIn(item["moved_bytes"], (b - a, c - b))
        self.assertIn("alternative_alignment", item)
        self.assertGreaterEqual(report["alignment"]["anchors_contested"], 1)
        self.assertTrue(report["first_divergence"]["alternative_alignment"])
        self.assertNotIn("exact", text.split("findings")[1])

    def test_swapped_blocks_inside_one_line_are_one_reorder(self):
        external = stream()
        start = line_start(external, 2) + 20
        host = external[:start] + external[start + 64:start + 128] + external[start:start + 64] + external[start + 128:]
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual((item["kind"], item["location"], item["moved_bytes"]), ("reorder", "ambiguous", 64))
        self.assertEqual((item["external"]["offset"], item["external"]["length"]), (start, 128))

    def test_host_ending_in_a_damaged_line_has_a_missing_tail(self):
        for bursts in (6, 40):  # The second external tail is far beyond the window and matcher bounds.
            with self.subTest(bursts=bursts):
                external = stream(bursts)
                start, end = line_start(external, 40), line_start(external, 41)
                host = external[:start + 30] + external[start + 94:end]
                self.assertTrue(host.endswith(b"\n"))
                status, report, _, _ = self.run_tool(host, external)
                self.assertEqual(status, compare.DIVERGENT)
                deletion, tail = report["findings"]
                self.assertEqual((deletion["kind"], deletion["external"]["offset"], deletion["external"]["length"]),
                                 ("deletion", start + 30, 64))
                self.assertEqual((tail["kind"], tail["external"]["offset"], tail["external"]["length"]),
                                 ("host_missing_tail", end, len(external) - end))
                self.assertEqual(report["tail"]["kind"], "host_missing_tail")

    def test_junk_external_is_unalignable_whatever_its_edges(self):
        data = stream()
        noise = junk(len(data))
        for name, external in (("junk", noise), ("first byte shared", b"S" + noise[1:]),
                               ("last byte shared", noise[:-1] + b"\n")):
            with self.subTest(name):
                status, report, _, _ = self.run_tool(data, external)
                self.assertEqual(status, compare.INCONCLUSIVE)
                [item] = report["findings"]
                self.assertEqual((item["kind"], item["limit"]), ("unalignable", "no_defensible_anchor"))
                self.assertIsNone(report["first_divergence"])
                self.assertIsNone(report["tail"])
                self.assertEqual(report["coverage"]["host_unverified"], len(data))

    def test_line_count_is_capped_before_splitting(self):
        data = stream()
        self.assertEqual(data.count(b"\n"), 43)
        status, report, _, error = self.run_tool(data, data, "--max-lines", "42")
        self.assertEqual(status, compare.INPUT_ERROR)
        self.assertIsNone(report)
        self.assertIn("holds 43 lines; refused above --max-lines 42", error)
        # Empty lines cost memory as well: 5000 newlines are 5000 lines.
        status, _, _, error = self.run_tool(b"\n" * 5000, data, "--max-lines", "4999")
        self.assertEqual(status, compare.INPUT_ERROR)
        self.assertIn("host capture", error)
        status, _, _, _ = self.run_tool(data, data, "--max-lines", "43")
        self.assertEqual(status, compare.IDENTICAL)
        for bad in ("0", str(compare.HARD_MAX_LINES + 1)):
            with self.subTest(bad), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                compare.main(["--format", "raw", "--host", str(self.root / "host.bin"), "--external",
                              str(self.root / "external.bin"), "--max-lines", bad])

    def test_external_capture_loss_is_not_a_host_divergence(self):
        data = stream()
        cut = line_start(data, 20) + 50
        status, report, text, _ = self.run_tool(data, data[:cut] + data[cut + 64:])
        self.assertEqual(status, compare.INCONCLUSIVE)
        [item] = report["findings"]
        self.assertEqual(item["suspect"], "external_capture")
        # The loss joins SOAK_HIST n=19 with the next line; the joined line still parses (extra
        # fields are kept), so the damage shows as the numbering gap at external line 21.
        self.assertEqual(item["parser_damage"]["external_lines"], [21])
        self.assertEqual(report["parser"]["external"]["gaps"][0], {"family": "soak", "line": 21, "expected": 20,
                                                                   "found": 21})
        self.assertEqual(item["parser_damage"]["host_lines"], [])
        self.assertEqual(report["coverage"]["findings_suspect_external_capture"], 1)
        self.assertEqual(report["coverage"]["host_unverified"], 64)
        self.assertIsNone(report["first_divergence"])
        self.assertIn("possible external capture damage", text)
        # A whole line lost by the external receiver shows as its own numbering gap.
        start, end = line_start(data, 20), line_start(data, 21)
        status, report, _, _ = self.run_tool(data, data[:start] + data[end:])
        self.assertEqual(status, compare.INCONCLUSIVE)
        [item] = report["findings"]
        self.assertEqual(item["suspect"], "external_capture")
        self.assertEqual(report["parser"]["external"]["gaps"][0]["line"], 20)

    def test_receiver_errors_mark_an_external_finding(self):
        data = stream()
        position = line_start(data, 17) + data[line_start(data, 17):].index(b"stack=8976") + len(b"stack=89")
        external = data[:position] + b"1" + data[position + 1:]  # Still parses: only the receiver knows.
        self.assertEqual(evidence.parse_bytes(external)[0][16].status, "parsed")
        status, report, _, _ = self.run_tool(data, external)
        self.assertEqual((status, report["findings"][0]["kind"]), (compare.DIVERGENT, "corruption"))
        flagged = per_line_times(external, 10.0).splitlines()
        flagged = [flagged[0] + ",errors"] + [row + (",1" if index == 17 else ",0")
                                              for index, row in enumerate(flagged[1:], 1)]
        status, report, _, _ = self.run_tool(data, external, external_times="\n".join(flagged) + "\n")
        self.assertEqual(status, compare.INCONCLUSIVE)
        [item] = report["findings"]
        self.assertEqual((item["suspect"], item["parser_damage"]["external_receiver_errors"]), ("external_capture", 1))
        self.assertEqual(report["inputs"]["external"]["times"]["receiver_errors"], 1)

    def test_damage_present_in_both_captures_was_on_the_wire(self):
        data = stream()
        cut = line_start(data, 12) + 30
        damaged = data[:cut] + data[cut + 64:]
        status, report, text, _ = self.run_tool(damaged, damaged)
        self.assertEqual(status, compare.IDENTICAL)
        self.assertEqual(report["parser"]["external_damage_in_identical_regions"], {"count": 2, "lines": [12, 13]})
        self.assertIn("damage on the wire", text)

    def test_host_damage_stays_a_divergence(self):
        external = stream()
        cut = line_start(external, 10) + 50
        status, report, _, _ = self.run_tool(external[:cut] + external[cut + 64:], external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertNotIn("suspect", item)
        self.assertEqual(item["parser_damage"]["external_lines"], [])
        self.assertEqual(item["parser_damage"]["host_lines"], [10, 11])

    def test_missing_tail_is_judged_against_the_hosts_last_chunk(self):
        external = stream(7)
        host = stream(6)
        ext_times = per_line_times(external, 10.0)
        # The host saw every line 2 ms after the external, on a clock 1000 s ahead, and then stopped.
        status, report, _, _ = self.run_tool(host, external, "--marker", "SOAK_BEGIN n=0",
                                             host_times=per_line_times(host, 1010.002), external_times=ext_times)
        self.assertEqual(status, compare.IDENTICAL)
        self.assertEqual(report["tail"]["kind"], "external_beyond_host_end")
        self.assertEqual(report["tail"]["timing"]["bytes_held_by_external_before_host_last_chunk"], 0)
        # The same, but the host's last read came 2 s late: the missing lines were sent while it read.
        status, report, _, _ = self.run_tool(host, external, "--marker", "SOAK_BEGIN n=0",
                                             host_times=per_line_times(host, 1010.002, late={43: 2.0}),
                                             external_times=ext_times)
        self.assertEqual(status, compare.DIVERGENT)
        self.assertEqual(report["tail"]["kind"], "host_missing_tail")
        self.assertEqual(report["tail"]["timing"]["bytes_held_by_external_before_host_last_chunk"],
                         len(external) - len(host))
        # Without related clocks nothing is assumed: the tail counts and says why.
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual((status, report["tail"]["timing"]["status"]), (compare.DIVERGENT, "not_established"))

    def test_time_rows_are_streamed_and_bounded(self):
        data = stream()
        with mock.patch.object(compare, "MAX_TIME_ROWS", 3):
            status, report, _, error = self.run_tool(data, data, host_times=per_line_times(data, 1.0))
        self.assertEqual(status, compare.INPUT_ERROR)
        self.assertIn("more than 3 rows; refused", error)

    def test_help_states_receiver_limits_and_no_splice(self):
        result = subprocess.run([sys.executable, str(SCRIPT), "--help"], capture_output=True, text=True,
                                timeout=60, check=False)
        flat = " ".join(result.stdout.split())
        for needed in ("preferably a logic analyzer", "reports framing, parity and break errors",
                       "cannot separate a host-side cause from a bridge one", "errors column", "--max-lines",
                       "per burst between idle gaps"):
            self.assertIn(needed, flat)
        for gone in ("one row per byte", "splice"):
            self.assertNotIn(gone, flat)
        self.assertNotIn("splice", json.dumps(compare.MEANING))


class RetainedBursts(Runner):
    """Real host bytes. The external stream is the host's own bytes where a burst is clean, and a
    labelled synthetic reconstruction where the host lost bytes."""

    def test_plain3_damaged_bursts_against_a_synthetic_reconstruction(self):
        data = retained(PLAIN3)
        lines, starts = lines_of(data)
        host = data[starts[1060]:starts[1420]]  # Lines 1061-1420: both 2026-11-04 A-run losses.
        base = starts[1060]
        # Loss 1, line 1070: SYNTHETIC fill. gapWin and ownerGap are placeholders copied from
        # neighbour n=1078; only the field structure, hence 64 bytes, is inferred.
        cut1 = starts[1069] - base + len(b"SOAK_GAP n=1067 t=5499895136 gapMax=4793 gapWin=")
        fill1 = b"3887 ownerGap=5197 qHw=1 actHw=1 retHw=1 trHw=3 trOver=0 netDrop"
        # Loss 2, line 1411: the received line has its full 164 bytes, but 32 of them are the next
        # line's bytes. SYNTHETIC fill from fields constant in every neighbouring SOAK_GAP.
        cut2 = starts[1410] - base + lines[1410].index(b"netDrop") + len(b"netDrop")
        fill2 = b"=0 stack=8976 heap=127876 heapMi"
        self.assertEqual(host[cut1 - 7:cut1 + 2], b"gapWin==0")
        self.assertEqual(host[cut2:cut2 + 32], b"1409 t=7359933131 lines=1409 blo")
        for line in (lines[1058], lines[1080], lines[1399], lines[1421]):
            self.assertIn(b"netDrop=0 stack=8976 heap=127876 heapMin=123436 tec=0 rec=0", line)
        external = host[:cut1] + fill1 + host[cut1:cut2] + fill2 + host[cut2 + 32:]
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        deletion, replacement = report["findings"]
        self.assertEqual((deletion["kind"], deletion["location"]), ("deletion", "exact"))
        self.assertEqual((deletion["host"]["offset"], deletion["external"]["length"]), (cut1, 64))
        self.assertEqual(deletion["host"]["lines"], [10, 10])
        self.assertEqual(replacement["kind"], "replacement")
        self.assertEqual((replacement["host"]["offset"], replacement["host"]["length"],
                          replacement["external"]["length"]), (cut2, 32, 32))
        # The host's 32 bytes are the external bytes 64 further on: a copy, not a net loss.
        self.assertEqual(replacement["source"]["relative_to_external_offset"], [64])
        # The tool's host view is the evidence parser's own view of these bytes.
        records, _ = evidence.parse_bytes(host)
        self.assertEqual([c["line"] for c in report["parser"]["host"]["corrupt"]],
                         [r.number for r in records if r.status != "parsed"])
        self.assertEqual([c["line"] for c in report["parser"]["host"]["corrupt"]], [10, 351])
        self.assertEqual(report["parser"]["external"]["malformed"], 0)
        self.assertEqual(report["parser"]["host"]["numbering_gaps"], 2)

    def test_plain3_clean_burst(self):
        data = retained(PLAIN3)
        _, starts = lines_of(data)
        clean = data[starts[1080]:starts[1091]]  # SOAK_SUM n=1077 .. SOAK_ABORT n=1087, no loss.
        records, _ = evidence.parse_bytes(clean)
        self.assertEqual({r.status for r in records}, {"parsed"})
        # External = the host's own bytes: a clean, contiguous burst is taken to be the wire bytes.
        status, report, _, _ = self.run_tool(clean, clean)
        self.assertEqual(status, compare.IDENTICAL)
        # SYNTHETIC: a literal 64-byte deletion applied to that real burst.
        cut = clean.index(b"SOAK_CON") + 50
        status, report, _, _ = self.run_tool(clean[:cut] + clean[cut + 64:], clean)
        [item] = report["findings"]
        self.assertEqual(item["kind"], "deletion")
        low, high = item["equivalent_starts"]["external"]
        self.assertTrue(low <= cut <= high)
        self.assertEqual(item["external"]["length"], 64)

    def test_r1_xz_bursts_decompressed_in_memory(self):
        data = lzma.decompress(retained(R1))
        self.assertEqual(hashlib.sha256(data).hexdigest(), R1_RX_SHA256)
        lines, starts = lines_of(data)
        host = data[starts[3961]:starts[3994]]  # Clean n=3959 and n=3970 bursts, then damaged n=3981.
        self.assertEqual(lines[3983][:len(b"SOAK_SUM n=3981")], b"SOAK_SUM n=3981")
        cut = starts[3983] - starts[3961] + lines[3983].index(b"rx=426") + len(b"rx=426")
        # SYNTHETIC fill: the last two rx digits and tx are extrapolated from n=3959 and n=3970
        # (rx = tx, +120 per summary); the zero fields are as in every neighbour.
        fill = b"52 tx=42652 txRej=0 rxDrop=0 rxLost=0 unmatched=0 qRej=0 resFull"
        external = host[:cut] + fill + host[cut:]
        status, report, _, _ = self.run_tool(host, external)
        self.assertEqual(status, compare.DIVERGENT)
        [item] = report["findings"]
        self.assertEqual((item["kind"], item["host"]["offset"], item["external"]["length"]), ("deletion", cut, 64))
        self.assertEqual(item["external"]["records"][0], {"line": 23, "status": "parsed", "kind": "SOAK_SUM",
                                                          "n": 3981})
        records, _ = evidence.parse_bytes(host)
        self.assertEqual([c["line"] for c in report["parser"]["host"]["corrupt"]],
                         [r.number for r in records if r.status != "parsed"])
        self.assertEqual(report["coverage"]["host_identical"], len(host))
        clean = host[:starts[3983] - starts[3961]]
        status, report, _, _ = self.run_tool(clean, clean)
        self.assertEqual((status, report["parser"]["host"]["malformed"]), (compare.IDENTICAL, 0))


if __name__ == "__main__":
    unittest.main()
