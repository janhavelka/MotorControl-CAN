#!/usr/bin/env python3
"""Offline contract tests for s2_console_account.py: synthetic S2 streams with known damage,
the real line parser, and retained captures from images that predate the counters."""
import io
import json
import lzma
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import s2_console_account as account  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
WRAP = 1 << 32

CON = ("SOAK_CON n=%d t=%d lines=%d blocked=0 conRej=0 conHw=300 evFull=0 evLimit=0 evHw=4 anomalies=0 "
       "dumps=0 dumpSkip=0 dumpAb=0 trLost=0 dumpUs=0 defDump=0 defBusy=0 defTx=0 defInh=0 slips=0 "
       "sdkDefer=0 uartBytes=%d queued=%d\n")
OLD_CON = ("SOAK_CON n=%d t=%d lines=%d blocked=0 conRej=0 conHw=300 evFull=0 evLimit=0 evHw=4 anomalies=0 "
           "dumps=0 dumpSkip=0 dumpAb=0 trLost=0 dumpUs=0 defDump=0 defBusy=0 defTx=0 defInh=0 slips=0 "
           "sdkDefer=0\n")


class Stream:
    """An S2 output stream built line by line, with SOAK_CON counters taken at formatting.

    position is the count of bytes admitted before the next line; queued is what the line claims
    is still in the console queue when formatted (any value up to the position)."""

    def __init__(self, base=0):
        self.data = bytearray()
        self.base = base
        self.line = 0
        self.con_offsets = []

    def add(self, text):
        self.data += text.encode("ascii")
        self.line += 1

    def summary(self, t_s, filler=3, queued=200, foreign=b"", old=False):
        for index in range(filler):
            self.add("SOAK_GAP n=%d t=%d gapMax=%d pad=%s\n" % (self.line, t_s * 1000000, index, "x" * 120))
        position = (self.base + len(self.data)) % WRAP
        # Output another writer admitted after formatting but before this line's enqueue.
        self.data += foreign
        self.con_offsets.append(len(self.data))
        if old:
            self.add(OLD_CON % (self.line, t_s * 1000000, self.line))
        else:
            queued = min(queued, self.base + len(self.data))
            self.add(CON % (self.line, t_s * 1000000, self.line, (position - queued) % WRAP, queued))


def run(data, **options):
    return account.account(io.BytesIO(bytes(data)), **options)


def burst(count=6, **options):
    stream = Stream(**options)
    for second in range(1, count + 1):
        stream.summary(second)
    return stream


class AccountTest(unittest.TestCase):
    def test_identical_stream_balances_every_window(self):
        stream = burst()
        summary = run(stream.data)
        self.assertEqual((summary["soak_con"], summary["windows"], summary["compared"], summary["balanced"]),
                         (6, 5, 5, 5))
        self.assertEqual(summary["reported"], [])
        self.assertEqual(summary["s2_bytes"], summary["host_bytes"])

    def test_a_64_byte_deletion_is_placed_in_its_window(self):
        stream = burst()
        start = stream.con_offsets[2] - 100  # Inside the filler before the third SOAK_CON.
        damaged = stream.data[:start] + stream.data[start + 64:]
        summary = run(damaged)
        self.assertEqual((summary["compared"], summary["short"], summary["missing_bytes"]), (5, 1, 64))
        [window] = summary["reported"]
        self.assertEqual((window["from_n"], window["difference"]), (stream_n(stream, 1), -64))

    def test_a_partial_line_deletion_counts_its_exact_bytes(self):
        stream = burst()
        start = stream.con_offsets[3] - 37
        damaged = stream.data[:start] + stream.data[start + 11:]
        summary = run(damaged)
        self.assertEqual((summary["short"], summary["missing_bytes"]), (1, 11))

    def test_a_deletion_that_destroys_a_soak_con_line_merges_two_windows(self):
        stream = burst()
        start = stream.con_offsets[3] + 5  # "SOAK_" survives, "CON n=..." does not.
        damaged = stream.data[:start] + stream.data[start + 64:]
        summary = run(damaged)
        self.assertEqual((summary["soak_con"], summary["windows"], summary["missing_bytes"]), (5, 4, 64))
        [window] = summary["reported"]
        self.assertEqual(window["to_n"] - window["from_n"], 2 * (stream.line // 6))

    def test_duplicated_bytes_make_the_host_long(self):
        stream = burst()
        start = stream.con_offsets[1] - 80
        damaged = stream.data[:start] + stream.data[start:start + 32] + stream.data[start:]
        summary = run(damaged)
        self.assertEqual((summary["long"], summary["extra_bytes"], summary["short"]), (1, 32, 0))

    def test_repeated_identical_chunks_do_not_confuse_offsets(self):
        stream = Stream()
        for second in range(1, 5):
            for _ in range(4):
                stream.add("SOAK_GAP same line\n")
            stream.summary(second, filler=0)
        start = stream.con_offsets[2] - 19  # Remove one identical line out of four.
        damaged = stream.data[:start] + stream.data[start + 19:]
        summary = run(damaged)
        self.assertEqual((summary["short"], summary["missing_bytes"]), (1, 19))

    def test_corruption_that_keeps_length_is_not_a_byte_count_difference(self):
        stream = burst()
        index = stream.con_offsets[2] - 50
        damaged = bytearray(stream.data)
        damaged[index] = ord("v")  # One altered byte: the counters see equal lengths.
        summary = run(damaged)
        self.assertEqual(summary["balanced"], summary["compared"])

    def test_a_missing_tail_is_never_a_window(self):
        stream = burst()
        damaged = stream.data[:stream.con_offsets[5] + 30]  # Last SOAK_CON unterminated.
        summary = run(damaged)
        self.assertEqual((summary["soak_con"], summary["windows"], summary["balanced"]), (5, 4, 4))

    def test_counters_compare_across_the_2_32_wrap(self):
        stream = burst(base=WRAP - 1500)
        summary = run(stream.data)
        self.assertEqual((summary["compared"], summary["balanced"]), (5, 5))

    def test_old_lines_without_counters_compare_nothing(self):
        stream = Stream()
        for second in range(1, 5):
            stream.summary(second, old=True)
        summary = run(stream.data)
        self.assertEqual((summary["soak_con"], summary["without_counters"], summary["compared"]), (4, 4, 0))

    def test_a_boot_inside_a_window_is_not_compared(self):
        stream = burst(count=3)
        stream.add("S2_BENCH ready fw=x\n")
        restarted = Stream()
        for second in range(1, 3):
            restarted.summary(second)
        summary = run(stream.data + restarted.data)
        self.assertEqual(summary["not_compared"]["boot"], 1)
        self.assertEqual((summary["compared"], summary["balanced"]), (3, 3))

    def test_an_implausible_count_is_not_compared(self):
        stream = burst()
        text = bytes(stream.data)
        start = stream.con_offsets[2]
        end = text.index(b"\n", start)
        line = text[start:end].replace(b"uartBytes=", b"uartBytes=9")  # A digit too many.
        damaged = text[:start] + line + text[end:]
        summary = run(damaged)
        self.assertGreaterEqual(summary["not_compared"]["implausible"], 1)
        self.assertEqual((summary["short"], summary["long"]), (0, 0))

    def test_a_clock_restart_without_a_boot_line_is_implausible(self):
        stream = burst(count=3)
        stream.summary(1)  # S2 time went backwards.
        summary = run(stream.data)
        self.assertEqual(summary["not_compared"]["implausible"], 1)

    def test_interleaved_output_is_marked_on_both_windows(self):
        stream = Stream()
        for second in range(1, 6):
            stream.summary(second, foreign=b"PONG fw=x\n" if second == 3 else b"")
        summary = run(stream.data)
        self.assertEqual((summary["long"], summary["short"]), (1, 1))
        self.assertTrue(all(window["interleaving_possible"] for window in summary["reported"]))
        self.assertEqual(sorted(window["difference"] for window in summary["reported"]), [-10, 10])

    def test_a_line_longer_than_the_limit_keeps_offsets_exact(self):
        stream = Stream()
        stream.summary(1)
        stream.data += b"z" * 10000 + b"\n"
        stream.summary(2)
        stream.summary(3)
        summary = run(stream.data)
        self.assertEqual((summary["compared"], summary["balanced"]), (2, 2))

    def test_chunk_boundaries_do_not_move_offsets(self):
        stream = burst(count=40)
        for size in (1, 7, 64, 4096):
            original = account.CHUNK
            try:
                account.CHUNK = size
                summary = run(stream.data)
            finally:
                account.CHUNK = original
            self.assertEqual((summary["compared"], summary["balanced"]), (39, 39), size)

    def test_report_is_bounded(self):
        stream = burst(count=10)
        damaged = bytearray(stream.data)
        for offset in reversed(stream.con_offsets[1:]):
            del damaged[offset - 30:offset - 20]
        summary = run(damaged, max_report=3)
        self.assertEqual((summary["short"], len(summary["reported"]), summary["report_truncated"]), (9, 3, True))


class CommandTest(unittest.TestCase):
    def test_xz_input_json_output_and_exit_status(self):
        stream = burst()
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "rx.bin.xz"
            path.write_bytes(lzma.compress(bytes(stream.data)))
            out = pathlib.Path(directory) / "summary.json"
            self.assertEqual(account.main([str(path), "--json", str(out)]), 0)
            self.assertEqual(json.loads(out.read_text(encoding="utf-8"))["balanced"], 5)
            with self.assertRaises(FileExistsError):
                account.main([str(path), "--json", str(out)])  # Never overwrites.
            damaged = pathlib.Path(directory) / "damaged.bin"
            data, cut = bytes(stream.data), stream.con_offsets[2] - 100
            damaged.write_bytes(data[:cut] + data[cut + 64:])
            self.assertEqual(account.main([str(damaged)]), 1)
            # Bytes before the first counted line lie in no window and cannot be compared.
            damaged.write_bytes(data[:100] + data[164:])
            self.assertEqual(account.main([str(damaged)]), 0)

    def test_unreadable_or_uncomparable_input_exits_2(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(account.main([str(pathlib.Path(directory) / "absent.bin")]), 2)
            bad = pathlib.Path(directory) / "bad.bin.xz"
            bad.write_bytes(b"not xz")
            self.assertEqual(account.main([str(bad)]), 2)
            empty = pathlib.Path(directory) / "empty.bin"
            empty.write_bytes(b"")
            self.assertEqual(account.main([str(empty)]), 2)


class RetainedTest(unittest.TestCase):
    """Retained captures predate the counters: the tool must say so, not invent a balance."""

    def head(self, relative, size=1 << 21):
        path = ROOT / relative
        if not path.exists():
            self.skipTest("retained capture not in this checkout: %s" % relative)
        with lzma.open(path, "rb") as stream:
            return stream.read(size)

    def test_a_clean_200_ms_burst_parses_but_compares_nothing(self):
        data = self.head("docs/bench/20261105-stress-r3-reads200ms/trial-01/rx.bin.xz")
        summary = run(data)
        self.assertGreater(summary["soak_con"], 0)
        self.assertEqual(summary["without_counters"], summary["soak_con"])
        self.assertEqual(summary["compared"], 0)

    def test_a_damaged_20_ms_capture_compares_nothing(self):
        data = self.head("docs/bench/20261105-stress-r1-summary1s/trial-01/rx.bin.xz", 1 << 23)
        summary = run(data)
        self.assertGreater(summary["soak_con"], 0)
        self.assertEqual((summary["compared"], summary["short"], summary["long"]), (0, 0, 0))


class FirmwareContractTest(unittest.TestCase):
    def test_the_soak_con_format_carries_both_counters_at_the_end(self):
        text = (ROOT / "examples/s2_bench/Soak.cpp").read_text(encoding="utf-8")
        start = text.index('"SOAK_CON ')
        body = text[start:text.index("\\n", start)]
        self.assertIn("uartBytes=%lu", body)
        self.assertIn("queued=%u", body)
        self.assertIn("io.outputWritten", text[start:start + 2000])
        self.assertIn("unsigned(io.outputUsed)", text[start:start + 2000])

    def test_the_sizes_used_for_plausibility_match_the_firmware(self):
        header = (ROOT / "examples/common/ProbePlatform.h").read_text(encoding="utf-8")
        self.assertIn("OUTPUT_CAPACITY = %d," % account.QUEUE_BYTES, header)


def stream_n(stream, index):
    """The n= of SOAK_CON number index (0-based) of a burst() stream."""
    text = bytes(stream.data)
    start = stream.con_offsets[index]
    return int(text[start:text.index(b" ", start + 12)].split(b"n=")[1])


if __name__ == "__main__":
    unittest.main()
