#!/usr/bin/env python3
"""Soak runner against a fake board and clock: no port, no pyserial, no hardware."""

from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_bench  # noqa: E402
import s2_evidence as evidence  # noqa: E402
import s2_soak as soak  # noqa: E402


ROOT = soak.ROOT
LOCAL = timezone(timedelta(hours=1))
ALLOWED = re.compile(rb"^(p|c|X|\n|S\d+,\d+,\d+,\d+,\d+\n)$")
PONG = ("PONG target=esp32s2 base=%s elf=%s sdk=v5.5.5 running=1 active=%d silent=0 pinsReady=1 soak=%d t=5\n")


class Clock:
    """Advances only when the runner reads or sleeps, so a night passes in milliseconds."""

    def __init__(self):
        self.seconds = 0.0
        self.start = datetime(2026, 11, 3, 22, 0, 0, tzinfo=LOCAL)

    def wall(self):
        return self.start + timedelta(seconds=self.seconds)

    def utc(self):
        return self.wall().astimezone(timezone.utc).isoformat()

    def monotonic(self):
        return self.seconds

    def sleep(self, seconds):
        self.seconds += seconds


class Item:
    def __init__(self, device="COM15", serial="94A990398B40", vid=0x303A, pid=0x1002):
        self.device, self.serial_number, self.vid, self.pid = device, serial, vid, pid


def soak_lines(trial, start=1):
    """A short healthy soak body with continuous line numbers, in firmware format."""
    out = [
        "SOAK_TR n=%d t=100 node=1 sig=6041 from=0 to=8037 first=1 ne=1 ce=1" % start,
        "SOAK_TR n=%d t=200 node=2 sig=6041 from=0 to=8037 first=1 ne=1 ce=1" % (start + 1),
        ("SOAK_SUM n=%d t=300 trial=%d up=300 rx=8 tx=8 txRej=0 rxDrop=0 rxLost=0 unmatched=0 qRej=0 "
         "resFull=0 timeouts=0 cancelled=0 busOff=0 recov=0 twaiErr=0 twaiFlags=0 twaiDrop=0") % (start + 2, trial),
        ("SOAK_NODE n=%d t=300 node=1 ops=2 opsOk=2 opsFail=0 refused=0 steps=8 stepsOk=8 min=2000 mean=2500 "
         "max=3000 retireMax=1000 earlyReply=0 hb=0 hbGapMax=0 stopped=0") % (start + 3),
        ("SOAK_ABORT n=%d t=300 node=2 a0=00000000:0 a1=00000000:0 a2=00000000:0 a3=00000000:0 other=0" %
         (start + 4)),
    ]
    return out


def begin(trial, duration):
    return ("SOAK_BEGIN n=0 t=50 trial=%d dur=%d interval=200 summary=60 monitors=1 identity=1 opDeadline=2000000 "
            "queue=250000 resp=250000 stack=9000 heap=90000 heapMin=89000 base=%s" % (trial, duration, "a" * 40))


def end(trial, number, reason="duration"):
    return "SOAK_END n=%d t=9000 trial=%d reason=%s up=8950 lines=%d settled=1" % (number, trial, reason, number)


class Board:
    """The firmware's console, as far as the runner can see it. One object survives port reopens."""

    def __init__(self, clock):
        self.clock = clock
        self.active = False
        self.soaking = False
        self.writes = []
        self.queue = bytearray()
        self.trials = []
        self.fail_at = None       # (bytes served) raises a port error once.
        self.reset_after = {}     # trial ordinal -> extra bytes that end in a reboot.
        self.body = soak_lines    # Lines after SOAK_BEGIN.
        self.finish = True        # Say SOAK_END at the requested duration.
        self.end_at = None
        self.trial = None
        self.refuse = None
        self.block_write = None
        self.can_refuse = False
        self.served = 0
        self.reads = 0
        self.prefix = b""
        self.answer_ping = True
        self.resets = 0
        self.sent_bytes = bytearray()

    def serve(self, data):
        self.queue.extend(data)

    def text(self, line):
        self.serve((line + "\n").encode("utf-8"))

    def command(self, data):
        self.writes.append(bytes(data))
        if self.block_write and data.startswith(self.block_write):
            raise OSError("write timeout")
        if data == b"p":
            if self.answer_ping:
                self.serve((PONG % ("a" * 40, "b" * 64, self.active, self.soaking)).encode("ascii"))
        elif data == b"c":
            if self.can_refuse:
                self.text("CAN start refused or failed; no automatic retry")
            else:
                self.active = True
                self.text("CAN ready bitrate=1000000 TX17 RX16 S18; no request")
        elif data == b"X":
            self.text("SOAK_STOPPING; a started read settles first")
            if self.soaking:
                self.soaking = False
                number = 1 + len(self.body(self.trial))
                self.text(end(self.trial, number, "stopped"))
        elif data.startswith(b"S"):
            if self.refuse:
                self.text("SOAK_REFUSED reason=%s; no work" % self.refuse)
                return
            trial, duration = [int(x) for x in data[1:].split(b",")[:2]]
            self.trial = trial
            self.trials.append(trial)
            self.soaking = True
            self.end_at = self.clock.seconds + duration
            self.text(begin(trial, duration))
            for line in self.body(trial):
                self.text(line)
            extra = self.reset_after.get(len(self.trials))
            if extra is not None:
                self.serve(extra)
                self.soaking = False
                self.active = False
                self.resets += 1

    def tick(self):
        if self.soaking and self.finish and self.clock.seconds >= self.end_at:
            self.soaking = False
            self.text(end(self.trial, 1 + len(self.body(self.trial))))


class FakePort:
    def __init__(self, board):
        self.board = board
        self.closed = False
        self.dtr = self.rts = False

    @property
    def in_waiting(self):
        return len(self.board.queue)

    def read(self, size):
        board = self.board
        board.clock.seconds += 0.05
        board.tick()
        board.reads += 1
        if board.fail_at is not None and board.served >= board.fail_at:
            board.fail_at = None
            raise OSError("port vanished")
        size = min(size, len(board.queue))
        if board.fail_at is not None:
            size = min(size, board.fail_at - board.served)
        chunk = bytes(board.queue[:size])
        del board.queue[:size]
        board.served += size
        board.sent_bytes.extend(chunk)
        return chunk

    def write(self, data):
        self.board.command(data)
        return len(data)

    def close(self):
        self.closed = True


class Backend:
    def __init__(self, board, ports=None):
        self.board = board
        self.ports = [Item()] if ports is None else ports
        self.opened = []
        self.enumerations = 0
        self.vanish_for = 0

    def comports(self):
        self.enumerations += 1
        if self.vanish_for > 0:
            self.vanish_for -= 1
            return []
        return self.ports

    def open(self, device):
        self.opened.append(device)
        return FakePort(self.board)


class Forbidden:
    """Any use of the port layer fails the test."""

    def comports(self):
        raise AssertionError("dry run enumerated")

    def open(self, device):
        raise AssertionError("dry run opened " + device)


class Base(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.clock = Clock()
        self.board = Board(self.clock)
        self.backend = Backend(self.board)
        self.out = self.root / "run"

    def run_soak(self, *extra, duration="600"):
        argv = ["--duration-s", duration, "--output", str(self.out), "--reenumerate-s", "5"] + list(extra)
        code = soak.main(argv, self.backend, self.clock)
        for data in self.board.writes:
            self.assertRegex(data, ALLOWED)
        return code

    def run_json(self):
        return json.loads((self.out / "run.json").read_text(encoding="utf-8"))

    def events(self):
        rows = (self.out / "events.jsonl").read_text(encoding="utf-8").splitlines()
        return [json.loads(row) for row in rows]

    def names(self):
        return [row["event"] for row in self.events()]


class NormalRun(Base):
    def test_full_night_shape(self):
        code = self.run_soak(duration="100")
        self.assertEqual(code, 0)
        self.assertEqual(self.backend.opened, ["COM15"])
        self.assertEqual(self.board.writes[0], b"p")
        self.assertEqual(self.board.writes[1], b"c")
        self.assertTrue(self.board.writes[2].startswith(b"S"))
        self.assertEqual(len(self.board.writes), 3)
        trial = self.out / "trial-01"
        data = (trial / "rx.bin").read_bytes()
        self.assertEqual(data, bytes(self.board.sent_bytes))
        # The side file accounts for every byte, in order, at the time it arrived.
        rows = (trial / "rx.times.csv").read_text(encoding="utf-8").splitlines()[1:]
        offset = 0
        for row in rows:
            start, count = [int(x) for x in row.split(",")[:2]]
            self.assertEqual(start, offset)
            offset += count
        self.assertEqual(offset, len(data))
        self.assertIn("sig=6041", (self.out / "transitions.txt").read_text(encoding="utf-8"))
        summary = (self.out / "summary.txt").read_text(encoding="utf-8")
        self.assertIn("SOAK_SUM", summary)
        self.assertIn("DIGEST", summary)
        meta = json.loads((trial / "request.json").read_text(encoding="utf-8"))
        self.assertEqual((meta["status"], meta["reset"], meta["retry"], meta["command"]),
                         ("captured", False, False, "soak"))
        result = self.run_json()
        self.assertEqual(result["status"], "captured")
        self.assertEqual(result["trials"][0]["verdict"], "complete", result["trials"][0]["errors"])
        request = json.loads((self.out / "run_request.json").read_text(encoding="utf-8"))
        self.assertEqual(request["objects"]["read_disposition"], "accepted_not_qualified")
        self.assertNotIn("603F:00", request["objects"]["state"])

    def test_duration_comes_from_the_wall_clock_end(self):
        self.run_soak("--interval-ms", "250", "--summary-s", "30", "--reads", "monitors", duration="1800")
        trial, duration, interval, summary, reads = [
            int(x) for x in self.board.writes[2][1:].strip().split(b",")]
        self.assertEqual((interval, summary, reads), (250, 30, 1))
        self.assertLessEqual(duration, 1800)
        self.assertGreater(duration, 1790)
        self.assertEqual(trial, int(self.clock.start.timestamp()))

    def test_until_is_the_next_occurrence(self):
        now = datetime(2026, 11, 3, 22, 0, tzinfo=LOCAL)
        self.assertEqual(soak.parse_until("09:00", now), datetime(2026, 11, 4, 9, 0, tzinfo=LOCAL))
        self.assertEqual(soak.parse_until("23:15", now), datetime(2026, 11, 3, 23, 15, tzinfo=LOCAL))
        self.assertEqual(soak.parse_until("22:00", now), datetime(2026, 11, 4, 22, 0, tzinfo=LOCAL))
        for bad in ("9", "25:00", "09:60", "nine"):
            with self.assertRaises(soak.SoakError):
                soak.parse_until(bad, now)

    def test_already_started_controller_is_not_started_again(self):
        self.board.active = True
        self.assertEqual(self.run_soak(), 0)
        self.assertNotIn(b"c", self.board.writes)

    def test_bench_entry_still_lists_its_commands(self):
        self.assertEqual(s2_bench.COMMANDS["ping"], b"p")
        self.assertEqual(s2_bench.COMMANDS["soak-stop"], b"X")
        for name in s2_bench.COMMANDS:
            self.assertIn(name, evidence.EXPECTED)


class DryRun(Base):
    def test_touches_no_port_and_creates_nothing(self):
        code = soak.main(["--until", "09:00", "--dry-run", "--output", str(self.out)], Forbidden(), self.clock)
        self.assertEqual(code, 0)
        self.assertFalse(self.out.exists())
        self.assertEqual(self.clock.seconds, 0.0)

    def test_duration_above_the_firmware_limit_is_refused(self):
        self.clock.start = datetime(2026, 11, 3, 8, 59, tzinfo=LOCAL)
        with self.assertRaises(SystemExit):
            soak.main(["--duration-s", "172801", "--dry-run"], Forbidden(), self.clock)


class Identity(Base):
    def refused(self, ports, *extra):
        self.backend.ports = ports
        code = soak.main(["--duration-s", "60", "--output", str(self.out)] + list(extra), self.backend, self.clock)
        self.assertEqual(code, 2)
        self.assertEqual(self.backend.opened, [])
        self.assertFalse(self.out.exists())

    def test_absent(self):
        self.refused([])

    def test_other_serial(self):
        self.refused([Item(serial="00000000")])

    def test_other_vid_pid(self):
        self.refused([Item(vid=0x1234)])

    def test_unrecorded_serial_argument(self):
        self.refused([Item(serial="00000000")], "--serial", "00000000")

    def test_two_devices_on_one_name(self):
        self.refused([Item(), Item()])

    def test_other_port_present_is_never_opened(self):
        self.backend.ports = [Item(device="COM99", serial="X"), Item()]
        self.assertEqual(self.run_soak(), 0)
        self.assertEqual(self.backend.opened, ["COM15"])


class Interruptions(Base):
    def test_reset_mid_run_keeps_evidence_and_starts_a_new_trial(self):
        self.board.reset_after = {1: b"ESP-ROM:esp32s2-rc4-20191025\nrst:0x3 (RTC_SW_SYS_RST)\nS2_BENCH ready; x\n"}
        code = self.run_soak()
        self.assertEqual(code, 1)  # The first trial stays incomplete, whatever the second does.
        first = (self.out / "trial-01" / "rx.bin").read_bytes()
        self.assertIn(b"rst:0x3", first)
        self.assertIn(b"SOAK_TR", first)
        self.assertTrue((self.out / "trial-02" / "rx.bin").is_file())
        self.assertIn("board_reset", self.names())
        self.assertEqual(len(self.board.trials), 2)
        self.assertEqual(self.board.trials[1], self.board.trials[0] + 1)  # A new identity.
        # CAN was restarted explicitly for the new trial; the runner itself reset nothing.
        self.assertEqual(self.board.writes.count(b"c"), 2)
        rows = self.run_json()["trials"]
        self.assertEqual(rows[0]["verdict"], "incomplete")
        self.assertEqual(rows[1]["verdict"], "complete")
        meta = json.loads((self.out / "trial-01" / "request.json").read_text(encoding="utf-8"))
        self.assertEqual((meta["status"], meta["reason"]), ("failed", "board_reset"))

    def test_restart_with_lost_boot_text_is_found_by_ping(self):
        # No boot text and no more output: the quiet spell's ping says no soak is running.
        self.board.reset_after = {1: b""}
        self.assertEqual(self.run_soak("--summary-s", "5", duration="100"), 1)
        self.assertIn("board_reset", self.names())
        self.assertEqual(len(self.board.trials), 2)
        self.assertIn(b"soak=0", (self.out / "trial-01" / "rx.bin").read_bytes())

    def test_trial_count_is_bounded(self):
        reset = b"rst:0x3 (RTC_SW_SYS_RST)\nS2_BENCH ready; x\n"
        self.board.reset_after = {1: reset, 2: reset, 3: reset, 4: reset}
        self.assertEqual(self.run_soak("--max-trials", "2"), 1)
        self.assertEqual(len(self.board.trials), 2)
        self.assertIn("trials_exhausted", self.names())
        self.assertFalse((self.out / "trial-03").exists())

    def test_port_loss_under_a_soak_that_kept_running(self):
        self.board.fail_at = 600  # Inside the first trial's body.
        self.backend.vanish_for = 0
        code = self.run_soak()
        self.assertEqual(code, 1)
        self.assertIn("port_lost", self.names())
        self.assertIn("soak_still_running", self.names())
        # The old soak is stopped by command, not continued under a new name, then a new one begins.
        self.assertEqual(self.board.writes.count(b"X"), 1)
        self.assertEqual(self.board.writes.count(b"c"), 1)
        self.assertEqual(len(self.board.trials), 2)
        rows = self.run_json()["trials"]
        self.assertEqual(rows[0]["verdict"], "incomplete")
        self.assertEqual(self.run_json()["status"], "captured")

    def test_port_loss_truncates_a_line_and_keeps_every_byte(self):
        self.board.fail_at = 200  # Inside SOAK_BEGIN's text.
        self.run_soak("--max-trials", "1")
        data = (self.out / "trial-01" / "rx.bin").read_bytes()
        self.assertEqual(len(data), 200)
        self.assertIn("TRUNCATED", (self.out / "transitions.txt").read_text(encoding="utf-8"))
        rows = self.run_json()["trials"]
        self.assertEqual(rows[0]["verdict"], "incomplete")
        self.assertIn("truncated_line", {item["code"] for item in rows[0]["errors"]})

    def test_port_that_does_not_come_back_ends_the_run(self):
        self.board.fail_at = 600
        original = self.backend.comports
        calls = []

        def vanish():
            calls.append(1)
            # The first two are the check before the run and before its first trial.
            return [] if len(calls) > 2 else original()

        self.backend.comports = vanish
        self.assertEqual(self.run_soak(), 1)
        self.assertIn("port_not_back", self.names())
        self.assertEqual(len(self.board.trials), 1)

    def test_no_end_is_recorded_after_one_stop_request(self):
        self.board.finish = False
        board = self.board
        original = board.command

        def deaf(data):
            if data == b"X":
                board.writes.append(bytes(data))
                return
            original(data)

        board.command = deaf
        self.assertEqual(self.run_soak(duration="10"), 1)
        self.assertEqual(self.board.writes.count(b"X"), 1)
        self.assertEqual(self.run_json()["reason"], "no_end_after_stop")

    def test_silence_gets_one_local_ping_per_quiet_spell(self):
        self.board.answer_ping = True
        self.board.finish = True
        self.assertEqual(self.run_soak("--summary-s", "5", duration="60"), 0)
        self.assertIn("silence", self.names())
        pings = self.board.writes.count(b"p")
        self.assertGreater(pings, 1)
        self.assertLess(pings, 8)  # Each reply ends that quiet spell; it is not a poll.


    def test_keepalive_writes_only_newlines_and_counts_them(self):
        self.assertEqual(self.run_soak("--keepalive-s", "1", duration="30"), 0)
        newlines = self.board.writes.count(b"\n")
        self.assertGreater(newlines, 5)
        meta = json.loads((self.out / "trial-01" / "request.json").read_text(encoding="utf-8"))
        self.assertEqual(meta["host_keepalives"], newlines)
        self.assertEqual([w for w in self.board.writes if w != b"\n"][:2], [b"p", b"c"])
        self.assertEqual(self.run_json()["trials"][0]["verdict"], "complete")

    def test_no_keepalive_by_default(self):
        self.assertEqual(self.run_soak(duration="30"), 0)
        self.assertNotIn(b"\n", self.board.writes)


class Output(Base):
    def test_garbage_is_kept_and_counted(self):
        board = self.board
        original = board.text

        def text(line):
            if line.startswith("SOAK_SUM"):
                board.serve(b"\xff\xfe\x01garbage\nNOT_A_RECORD field=1\n")
            original(line)

        board.text = text
        # Damaged output is never called complete, even when no numbered record went missing.
        self.assertEqual(self.run_soak(), 1)
        row = self.run_json()["trials"][0]
        self.assertEqual(row["verdict"], "incomplete")
        self.assertIn("malformed_line", {item["code"] for item in row["errors"]})
        self.assertNotIn("soak_line_gap", {item["code"] for item in row["errors"]})
        data = (self.out / "trial-01" / "rx.bin").read_bytes()
        self.assertIn(b"\xff\xfe\x01garbage", data)
        logged = (self.out / "transitions.txt").read_text(encoding="utf-8")
        self.assertIn("PARSE malformed", logged)
        self.assertIn("PARSE unknown", logged)
        self.assertEqual(self.run_json()["trials"][0]["unknown"], 1)

    def test_lost_line_is_found_by_its_number(self):
        def lossy(trial):
            rows = soak_lines(trial)
            del rows[1]
            return rows

        self.board.body = lossy
        self.board.finish = False
        self.assertEqual(self.run_soak(duration="5"), 1)
        logged = (self.out / "transitions.txt").read_text(encoding="utf-8")
        self.assertIn("CHECK soak_line_gap", logged)

    def test_duplicate_line_is_inconsistent(self):
        def doubled(trial):
            rows = soak_lines(trial)
            rows.insert(1, rows[0])
            return rows

        self.board.body = doubled
        self.assertEqual(self.run_soak(), 1)
        row = self.run_json()["trials"][0]
        self.assertEqual(row["verdict"], "inconsistent")
        self.assertIn("soak_line_repeated", {item["code"] for item in row["errors"]})

    def test_host_backpressure_keeps_every_byte_in_bounded_reads(self):
        self.board.prefix = b""
        flood = [("SOAK_TR n=%d t=%d node=1 sig=606c from=%x to=%x first=%d ne=1 ce=1" % (
            i + 1, i, i, i + 1, 0 if i else 1)) for i in range(30000)]

        def body(trial):
            return flood

        self.board.body = body
        self.assertEqual(self.run_soak(duration="3000"), 0)
        data = (self.out / "trial-01" / "rx.bin").read_bytes()
        self.assertEqual(data, bytes(self.board.sent_bytes))
        self.assertGreater(len(data), soak.CHUNK_BYTES * 50)
        for row in (self.out / "trial-01" / "rx.times.csv").read_text(encoding="utf-8").splitlines()[1:]:
            self.assertLessEqual(int(row.split(",")[1]), soak.CHUNK_BYTES)
        meta = json.loads((self.out / "trial-01" / "request.json").read_text(encoding="utf-8"))
        self.assertGreater(meta["host_waiting_high_water"], soak.CHUNK_BYTES)
        # A night of records stays bounded in memory; the file holds them all.

    def test_blocked_command_write_is_never_repeated(self):
        self.board.block_write = b"S"
        self.assertEqual(self.run_soak("--max-trials", "1"), 1)
        self.assertEqual(sum(1 for w in self.board.writes if w.startswith(b"S")), 1)
        self.assertIn("port_lost", self.names())

    def test_refused_soak_ends_the_run_without_asking_again(self):
        self.board.refuse = "busy"
        self.assertEqual(self.run_soak(), 1)
        self.assertEqual(sum(1 for w in self.board.writes if w.startswith(b"S")), 1)
        self.assertEqual(self.run_json()["reason"], "refused:busy")

    def test_can_start_refusal_ends_the_run(self):
        self.board.can_refuse = True
        self.assertEqual(self.run_soak(), 1)
        self.assertEqual(self.run_json()["reason"], "can_start_refused")
        self.assertFalse(any(w.startswith(b"S") for w in self.board.writes))

    def test_silent_board_is_not_started(self):
        self.board.answer_ping = False
        self.assertEqual(self.run_soak(), 1)
        self.assertEqual(self.run_json()["reason"], "no_ping")
        self.assertEqual(self.board.writes, [b"p"])

    def test_anomalies_are_failed_evidence(self):
        def body(trial):
            rows = soak_lines(trial)
            rows.append("SOAK_ANOM n=6 t=400 id=1 kind=terminal node=2 dump=1 a=1 b=0")
            rows.append("SOAK_OP n=7 t=400 node=2 op=1 local=0 step=1 object=6041:00 outcome=3 reason=0 "
                        "stages=1f abort=00000000")
            rows.append("SOAK_NODE_STOP n=8 t=400 node=2 reason=unresolved lastOp=1")
            rows.append("SOAK_T n=9 a=1 i=0 k=1 p=390 id=602 ext=0 rtr=0 dlc=8 data=4041600000000000 ok=1 ex=0 "
                        "bounds=0 dlo=0 dhi=0 ce=1 st=0 tr=1 tf=1")
            rows.append("SOAK_TEND n=10 t=450 a=1 abandoned=0 held=50 unprinted=0")
            return rows

        self.board.body = body
        self.assertEqual(self.run_soak(), 3)
        row = self.run_json()["trials"][0]
        self.assertEqual(row["verdict"], "failed-evidence", row["errors"])
        self.assertEqual(row["soak"]["nodes_stopped"], {"2": "unresolved"})
        self.assertEqual(row["soak"]["anomalies"], {"terminal": 1})

    def test_repeated_failures_keep_host_notes_bounded(self):
        checker = evidence.SoakChecker(5)
        checker.feed(evidence.parse_line(1, begin(5, 60)))
        for number in range(1, 2000):
            checker.feed(evidence.parse_line(number + 1, (
                "SOAK_OP n=%d t=%d node=1 op=%d local=0 step=3 object=2102:00 outcome=1 reason=0 stages=3f "
                "abort=06020000") % (number, number, number)))
        checker.feed(evidence.parse_line(3000, "SOAK_ANOM n=9 t=1 id=1 kind=emcy node=1 dump=0 a=81 b=0"))
        self.assertEqual(len(checker.failed), checker.NOTES_PER_CODE)
        self.assertEqual(checker.code_counts["failed_operation"], 1999)
        self.assertEqual(checker.code_counts["soak_line_repeated"], 1)

    def test_announced_dump_that_never_arrives_is_incomplete(self):
        def body(trial):
            rows = soak_lines(trial)
            rows.append("SOAK_ANOM n=6 t=400 id=1 kind=emcy node=2 dump=1 a=82 b=0")
            return rows

        self.board.body = body
        self.assertEqual(self.run_soak(), 1)
        self.assertIn("soak_dump_missing", {item["code"] for item in self.run_json()["trials"][0]["errors"]})


class FirmwareFormats(unittest.TestCase):
    """The firmware's format strings and the grammar must name the same fields."""

    @staticmethod
    def source_fields(path, name):
        text = (ROOT / path).read_text(encoding="utf-8")
        match = re.search(r'"%s\b(.*?);\n' % re.escape(name), text, re.S)
        return re.findall(r"(\w+)=%", match.group(1))

    def grammar_fields(self, name):
        names = set()
        for row in evidence.GRAMMAR:
            if row.head.strip() == name:
                names |= set(evidence._compile(row.fields))
        return names

    def test_every_soak_format_is_in_the_grammar(self):
        text = (ROOT / "examples/s2_bench/Soak.cpp").read_text(encoding="utf-8")
        found = set()
        for name, body in re.findall(r'"(SOAK_[A-Z_]+) ((?:[^"]|"\s*")*?)\\n"', text):
            found.add(name)
            fields = re.findall(r"(\w+)=%", body)
            self.assertTrue(fields, name)
            self.assertLessEqual(set(fields), self.grammar_fields(name), name)
        self.assertTrue({"SOAK_BEGIN", "SOAK_T", "SOAK_SUM", "SOAK_GAP", "SOAK_CON", "SOAK_NODE", "SOAK_HIST",
                         "SOAK_OUT", "SOAK_ABORT", "SOAK_END", "SOAK_TR", "SOAK_ANOM", "SOAK_OP",
                         "SOAK_TEND", "SOAK_NODE_STOP"} <= found, found)

    def test_every_soak_line_fits_its_buffer_at_type_maxima(self):
        # SOAK_CON overflowed the 240-byte line at the end of a 3 h soak after a bus fault (2026-11-03).
        widths = {"llu": 20, "lu": 10, "u": 10, "lx": 8, "x": 8}
        line = int(re.search(r"LINE_BYTES = (\d+)",
                             (ROOT / "examples/s2_bench/Soak.h").read_text(encoding="utf-8")).group(1))
        console = int(re.search(r"OUTPUT_CAPACITY = (\d+)",
                                (ROOT / "examples/common/ProbePlatform.h").read_text(encoding="utf-8")).group(1))
        self.assertLessEqual(line, console)
        text = (ROOT / "examples/s2_bench/Soak.cpp").read_text(encoding="utf-8")
        found = set()
        for name, body in re.findall(r'"(SOAK_[A-Z_]+) ((?:[^"]|"\s*")*?)\\n"', text):
            found.add(name)
            body = re.sub(r'"\s*"', "", body)
            widest = re.sub(r"%(\d*)(ll|l)?([ux])",
                            lambda m: "9" * max(int(m.group(1) or 0), widths[(m.group(2) or "") + m.group(3)]), body)
            self.assertNotIn("%", widest.replace("%s", ""), name)
            # Name, space, body with %s at 64 characters, newline; vsnprintf needs one more.
            self.assertLess(len(name) + 2 + len(widest.replace("%s", "s" * 64)), line, name)
        self.assertIn("SOAK_CON", found)

    def test_every_bench_line_is_in_the_grammar(self):
        text = (ROOT / "examples/s2_bench/main/main.cpp").read_text(encoding="utf-8")
        for name, pattern in (("RESULT", r'"RESULT (.*?)\\n"\s*,'), ("PONG", r'"PONG (.*?)\\n"\s*,'),
                              ("STATS", r'"STATS (.*?)\\n"\s*,')):
            body = re.search(pattern, text, re.S).group(1)
            fields = set(re.findall(r"(\w+)=%", body))
            self.assertTrue(fields, name)
            self.assertLessEqual(fields, self.grammar_fields(name), name)
        frame_fields = set(re.findall(r"(\w+)=%", re.search(r'void trafficLine.*?\n}\n', text, re.S).group(0)))
        self.assertLessEqual(frame_fields, self.grammar_fields("TRAFFIC"))
        self.assertTrue({"epoch", "ctrl", "rec", "txk", "txr", "txf"} <= frame_fields)

    def test_new_record_fields_parse(self):
        record = evidence.parse_line(1, "RESULT op=4 req=4 oreq=17 step=2 object=6041:00 dir=0 outcome=0 ack=1 "
                                        "possible=1 uncertain=0 abort=00000000 cleanup=0 size=2 data=37800000 "
                                        "admitted=10 completed=20 ne=3 reason=0 st=3f")
        self.assertEqual(record.status, "parsed")
        self.assertEqual((record.values["req"], record.values["oreq"], record.values["ne"], record.values["st"]),
                         (4, 17, 3, 0x3f))
        self.assertEqual(record.extra, {})
        for line, expected in (
                ("TRAFFIC k=4 p=9 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 bounds=0 lo=0 "
                 "hi=0 epoch=2 ctrl=2", ("epoch", 2)),
                ("TRAFFIC k=1 p=9 id=601 ext=0 rtr=0 dlc=8 data=4041600000000000 ok=1 exact=0 t=0 bounds=0 lo=0 "
                 "hi=0 epoch=1 txk=0 txr=7 txf=1", ("txr", 7))):
            record = evidence.parse_line(1, line)
            self.assertEqual((record.status, record.extra), ("parsed", {}))
            self.assertEqual(record.values[expected[0]], expected[1])
        stats = evidence.parse_line(1, "STATS running=1 inhibited=0 rx=1 tx=1 dropped=0 errors=0 flags=00000000 "
                                       "stackFree=9 t=55 txRej=0 rxLost=0 unmatched=0 traceOver=0 ownerGap=3 conRej=0")
        self.assertEqual((stats.status, stats.extra, stats.values["t"]), ("parsed", {}, 55))
        self.assertEqual(stats.row.time, "t")

    def test_controller_records_reach_the_transition_log(self):
        data = (b"TRAFFIC k=4 p=9 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 bounds=0 lo=0 hi=0 "
                b"epoch=1 ctrl=2\n")
        report = evidence.replay_bytes(data, {})
        transitions = evidence.Transitions()
        transitions.observe(report)
        self.assertEqual([(e["signal"], e["to"], e["time_us"]) for e in transitions.entries],
                         [("controller_state", "2", 9)])

    def test_node_epoch_change_inside_a_record_is_a_contradiction(self):
        rows = ["MOTION_READINESS admitted: thirty uploads only; no enable/target/stop/save/retry",
                "RAW id=9 node=1 failure=0 lost=0 terminals=2 traffic=0 tail=0 tailLost=0 gap=2000",
                "RESULT step=1 object=1001:00 outcome=0 ack=1 possible=0 uncertain=0 abort=00000000 cleanup=0 size=1 "
                "data=00000000 admitted=1 completed=2 ne=1",
                "RESULT step=2 object=603f:00 outcome=0 ack=1 possible=0 uncertain=0 abort=00000000 cleanup=0 size=2 "
                "data=00000000 admitted=3 completed=4 ne=2",
                "END raw terminals=2 traffic=0 tail=0 tailLost=0 lost=0; r=release"]
        report = evidence.replay_bytes(("\n".join(rows) + "\n").encode(), {"command": "motion-readiness1"})
        self.assertIn("node_epoch_changed", {item["code"] for item in report.errors})
        self.assertEqual(report.node_epochs, [1, 2])

    def test_stale_raw_history_figure_is_gone(self):
        text = (ROOT / "examples/s2_bench/README.md").read_text(encoding="utf-8")
        self.assertNotIn("holds 26 terminals", text)
        header = (ROOT / "examples/s2_bench/RawEvidence.h").read_text(encoding="utf-8")
        count = re.search(r"RAW_EVIDENCE_FIELDS = (\d+)", header).group(1)
        self.assertIn("holds %s terminals" % count, text)


def check_transcript(path):
    """Every record the firmware really printed must parse with nothing left over."""
    data = Path(path).read_bytes()
    report = evidence.replay_bytes(data, {"command": "soak", "status": "captured", "trial": 9})
    problems = []
    for name in ("unknown", "malformed", "unknown_fields", "errors"):
        if getattr(report, name):
            problems.append("%s: %s" % (name, getattr(report, name)[:5]))
    required = {"SOAK_BEGIN", "SOAK_TR", "SOAK_ANOM", "SOAK_OP", "SOAK_EMCY", "SOAK_BOOT", "SOAK_NODE_STOP",
                "SOAK_T", "SOAK_TEND", "SOAK_SUM", "SOAK_GAP", "SOAK_CON", "SOAK_NODE", "SOAK_HIST", "SOAK_OUT",
                "SOAK_ABORT", "SOAK_END"}
    missing = required - set(report.kinds)
    if missing:
        problems.append("record kinds never printed: %s" % sorted(missing))
    if report.verdict != "failed-evidence" or report.soak.get("end") != "stopped":
        problems.append("verdict %s end %s" % (report.verdict, report.soak.get("end")))
    transitions = evidence.Transitions()
    transitions.observe(report)
    categories = {entry["category"] for entry in transitions.entries}
    if categories != {"health", "nmt", "status", "controller"}:
        problems.append("transition categories %s" % sorted(categories))
    return problems


def main():
    unittest.main(argv=sys.argv)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
