#!/usr/bin/env python3
"""Contract tests for scripts/s2_pdo.py: plan checks, the step runner's stop rules and the offline analysis."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_pdo as pdo  # noqa: E402

PONG = ("PONG target=esp32s2 base=%s elf=%s sdk=5.5.5 running=1 active=1 silent=0 pinsReady=1 soak=%d t=5 "
        "routes=5 trial=0 motion=0%s\n")


class Clock:
    def __init__(self):
        self.seconds = 0.0

    def utc(self):
        return "2026-11-06T12:00:%06.3fZ" % self.seconds

    def monotonic(self):
        self.seconds += 0.01
        return self.seconds


class Item:
    def __init__(self, device="COM15", serial="94A990398B40", vid=0x303A, pid=0x1002):
        self.device, self.serial_number, self.vid, self.pid = device, serial, vid, pid


class Board:
    """The firmware console as the runner sees it: replies keyed by the exact bytes written."""

    def __init__(self, replies=None, pdo="1", watch=0, soak=0):
        self.replies = dict(replies or {})
        self.writes = []
        self.queue = bytearray(b"stale\n")
        self.pdo, self.watch, self.soak = pdo, watch, soak
        self.closed = False

    @property
    def in_waiting(self):
        return len(self.queue)

    def read(self, count):
        out = bytes(self.queue[:count])
        del self.queue[:count]
        return out

    def write(self, data):
        self.writes.append(bytes(data))
        if data == b"p":
            extra = "" if self.pdo is None else " pdo=%s watch=%d" % (self.pdo, self.watch)
            self.queue.extend((PONG % ("a" * 40, "b" * 64, self.soak, extra)).encode("ascii"))
        else:
            self.queue.extend(self.replies.get(bytes(data), b""))
        return len(data)

    def close(self):
        self.closed = True


class Backend:
    def __init__(self, board, ports=None):
        self.board = board
        self.ports = ports if ports is not None else [Item()]

    def comports(self):
        return self.ports

    def open(self, device):
        return self.board


def plan(*steps):
    return {"purpose": "test", "authorization": "test", "steps": list(steps)}


class Args:
    def __init__(self, plan_path, output, dry_run=False):
        self.plan, self.output, self.dry_run = str(plan_path), str(output), dry_run
        self.port, self.serial, self.tail_s = "COM15", "94A990398B40", 0.05


WATCH = (b"PDOW admitted seconds=5; copies received frames, sends nothing\n"
         b"PDOW_BEGIN n=0 t=100 seconds=5 ring=32 ids=16\n")


class PlanTest(unittest.TestCase):
    def test_plan_rules(self):
        self.assertEqual(len(pdo.check_plan(plan({"send": "P5", "expect": "PDOW admitted"}, {"wait_s": 1}))), 2)
        for bad in ({}, {"purpose": "x", "steps": []},
                    plan({"send": "P5"}),  # no expected reply
                    plan({"send": "x", "expect": "a"}),  # unreviewed byte
                    plan({"send": "Q1", "expect": "a"}),  # not a line command
                    plan({"send": "N1,o\n", "expect": "a"}),
                    plan({"wait_s": 0}), plan({"wait_s": 901}), plan({"note": "nothing"}),
                    plan({"send": "P5", "expect": "a", "retry": 1}), plan({"wait_s": 1, "end": "x"})):
            with self.assertRaises(pdo.SessionError):
                pdo.check_plan(bad)
        with self.assertRaises(pdo.SessionError):
            pdo.check_plan({"authorization": "x", "steps": [{"wait_s": 1}]})


class RunTest(unittest.TestCase):
    def session(self, board, steps, dry_run=False):
        root = Path(tempfile.mkdtemp())
        path = root / "plan.json"
        path.write_text(json.dumps(plan(*steps)), encoding="utf-8")
        out = root / "session"
        code = pdo.run(Args(path, out, dry_run), Backend(board), Clock())
        meta = json.loads((out / "session.json").read_text(encoding="utf-8")) if not dry_run else None
        return code, meta, out

    def test_completed_plan_sends_only_its_steps(self):
        board = Board({b"P5\n": WATCH, b"N1,o\n": b"NMT admitted node=1 cmd=1; addressed\n"
                       b"NMT request=4 target=1 cmd=1 outcome=0 possible=1 txConfirmed=1 completed=900; x\n"})
        code, meta, out = self.session(board, [{"send": "P5", "expect": "PDOW admitted"},
                                               {"send": "N1,o", "expect": "NMT admitted", "end": "NMT request"},
                                               {"wait_s": 0.05}])
        self.assertEqual(code, 0)
        self.assertEqual(meta["reason"], "completed")
        self.assertEqual(board.writes, [b"p", b"P5\n", b"N1,o\n"])
        self.assertEqual([s["outcome"] for s in meta["steps"]], ["answered", "answered", "waited"])
        self.assertEqual((out / "rx.bin").read_bytes(), (out / "rx.txt").read_bytes())
        self.assertTrue((out / "rx.bin").read_bytes().startswith(b"stale\n"))
        self.assertTrue(board.closed)

    def test_refusal_stops_the_plan_and_ends_only_the_watch(self):
        board = Board({b"P5\n": WATCH, b"N1,o\n": b"NMT refused; no work\n",
                       b"P0\n": b"PDOW_STOPPING; final summaries follow\nPDOW_END n=1 t=200 reason=stop frames=0 "
                                b"printed=0 unprinted=0 tapDropped=0 untimed=0 sdo=0 idOverflow=0\n"})
        code, meta, _ = self.session(board, [{"send": "P5", "expect": "PDOW admitted"},
                                             {"send": "N1,o", "expect": "NMT admitted"},
                                             {"send": "W1,0,5,0,100", "expect": "PDO_WRITE admitted"}])
        self.assertEqual(code, 1)
        self.assertEqual(meta["reason"], "refused")
        # Nothing after the refusal but the local watch stop: no NMT, write or stop on CAN.
        self.assertEqual(board.writes, [b"p", b"P5\n", b"N1,o\n", b"P0\n"])
        self.assertEqual(meta["steps"][-1]["outcome"], "watch_ended")

    def test_silence_stops_the_plan_without_cleanup_when_no_watch_runs(self):
        board = Board({})
        code, meta, _ = self.session(board, [{"send": "R1,t,0", "expect": "PDO_READ admitted", "timeout_s": 0.1},
                                             {"send": "r", "expect": "RELEASED"}])
        self.assertEqual((code, meta["reason"]), (1, "no_reply"))
        self.assertEqual(board.writes, [b"p", b"R1,t,0\n"])

    def test_missing_end_stops_the_plan(self):
        board = Board({b"R1,t,0\n": b"PDO_READ admitted node=1 dir=t set=0; nine uploads, no write\n"})
        code, meta, _ = self.session(board, [{"send": "R1,t,0", "expect": "PDO_READ admitted", "end": "END raw",
                                              "end_timeout_s": 0.1}, {"send": "r", "expect": "RELEASED"}])
        self.assertEqual(meta["reason"], "no_end")
        self.assertEqual(board.writes, [b"p", b"R1,t,0\n"])

    def test_preflight_refusals_send_nothing_else(self):
        for board, why in ((Board(pdo=None), "image_without_pdo_lines"), (Board(soak=1), "soak_running"),
                           (Board(watch=1), "watch_running")):
            code, meta, _ = self.session(board, [{"send": "P5", "expect": "PDOW admitted"}])
            self.assertEqual((code, meta["reason"]), (1, why))
            self.assertEqual(board.writes, [b"p"])

    def test_identity_and_existing_output_refuse(self):
        root = Path(tempfile.mkdtemp())
        path = root / "plan.json"
        path.write_text(json.dumps(plan({"wait_s": 1})), encoding="utf-8")
        with self.assertRaises(pdo.SessionError):
            pdo.run(Args(path, root / "a"), Backend(Board(), [Item(serial="X")]), Clock())
        (root / "b").mkdir()
        with self.assertRaises(pdo.SessionError):
            pdo.run(Args(path, root / "b"), Backend(Board()), Clock())

    def test_dry_run_touches_nothing(self):
        class Untouchable:
            def comports(self):
                raise AssertionError("enumerated")

            def open(self, device):
                raise AssertionError("opened")
        root = Path(tempfile.mkdtemp())
        path = root / "plan.json"
        path.write_text(json.dumps(plan({"wait_s": 1})), encoding="utf-8")
        self.assertEqual(pdo.run(Args(path, root / "s", True), Untouchable(), Clock()), 0)
        self.assertFalse((root / "s").exists())


def result(step, obj, size, data, outcome=0, abort="00000000", completed=None):
    completed = 1000 + 100 * step if completed is None else completed
    return ("RESULT step=%d object=%s outcome=%d ack=1 possible=1 uncertain=0 abort=%s cleanup=0 size=%d "
            "data=%s admitted=%d completed=%d" % (step, obj, outcome, abort, size, data, completed - 10, completed))


class AnalyzeTest(unittest.TestCase):
    def capture(self, lines):
        directory = Path(tempfile.mkdtemp())
        (directory / "rx.bin").write_bytes(("\n".join(lines) + "\n").encode("ascii"))
        return pdo.analyze(directory)

    def test_stream_decode_and_nmt_timing(self):
        map_read = ["PDO_READ admitted node=1 dir=t set=1; nine uploads, no write",
                    "RAW id=9 node=1 failure=0 lost=0 terminals=9 traffic=0 tail=0 tailLost=0 gap=10",
                    result(1, "1801:01", 4, "81020000"), result(2, "1801:02", 1, "ff000000"),
                    result(3, "1801:03", 2, "00000000"), result(4, "1801:05", 2, "64000000"),
                    result(5, "1a01:00", 1, "02000000"), result(6, "1a01:01", 4, "20006460"),
                    result(7, "1a01:02", 4, "20006c60"), result(8, "1a01:03", 4, "00000000"),
                    result(9, "1a01:04", 4, "00000000"),
                    "END raw terminals=9 traffic=0 tail=0 tailLost=0 lost=0; r=release"]
        frames = []
        n = 1
        times = [10000, 110000, 210050, 309990]
        for t, position in zip(times, (4728, 4728, -5, -5)):
            data = (position & 0xFFFFFFFF).to_bytes(4, "little").hex() + "00000000"
            frames.append("PDOW_F n=%d t=%d id=281 ext=0 rtr=0 dlc=8 data=%s timed=1 exact=0 lo=%d hi=%d gap=0"
                          % (n, t + 5, data, t - 3, t))
            n += 1
        frames.append("PDOW_F n=%d t=400000 id=701 ext=0 rtr=0 dlc=1 data=05 timed=1 exact=0 lo=1 hi=399000 gap=0" % n)
        n += 1
        lines = map_read + [
            "PDOW_BEGIN n=0 t=1 seconds=5 ring=32 ids=16",
            "NMT request=4 target=1 cmd=1 outcome=0 possible=1 txConfirmed=1 completed=9000; retained",
        ] + frames[:2] + [
            "NMT request=5 target=1 cmd=128 outcome=0 possible=1 txConfirmed=1 completed=250000; retained",
        ] + frames[2:] + [
            "PDOW_ID n=%d t=500000 id=281 ext=0 count=4 first=10000 last=309990 minGap=99940 maxGap=100050 "
            "meanGap=99996 sdGap=40 changes=1 dlc=8 data=fbffffff00000000" % n,
            "PDOW_END n=%d t=500001 reason=duration frames=5 printed=5 unprinted=0 tapDropped=0 untimed=0 sdo=0 "
            "idOverflow=0" % (n + 1)]
        report = self.capture(lines)
        self.assertEqual((report["unknown"], report["malformed"]), ([], []))
        self.assertEqual(report["maps"], {"1/1": ["6064:00/32", "606c:00/32"]})
        watch = report["watches"][0]
        self.assertEqual(watch["console_gaps"], [])
        stream = watch["ids"]["281"]
        self.assertEqual(stream["host_stats"]["count"], 4)
        self.assertEqual((stream["host_stats"]["min_gap"], stream["host_stats"]["max_gap"]), (99940, 100050))
        self.assertEqual(stream["decoded_first"], {"6064:00": 4728, "606c:00": 0})
        self.assertEqual(stream["decoded_last"], {"6064:00": -5, "606c:00": 0})
        self.assertEqual(stream["decode_failures"], 0)
        start, preop = stream["around_nmt"]
        self.assertEqual((start["nmt"], start["first_after_delay_us"], start["frames_after"]), ("start", 1000, 4))
        # Frames after the Pre-operational completion are reported, never hidden.
        self.assertEqual((preop["nmt"], preop["last_before"], preop["frames_after"]), ("pre-operational", 210050, 1))
        self.assertEqual(watch["ids"]["701"]["heartbeat_states"], [(399000, "05")])
        self.assertEqual(report["pdo_records"][0]["results"][5]["data"], "20006460")

    def test_console_gap_and_abort_are_reported(self):
        lines = ["PDOW_BEGIN n=0 t=1 seconds=5 ring=32 ids=16",
                 "PDOW_SUM n=1 t=2 frames=0 printed=0 unprinted=0 tapDropped=0 untimed=0 sdo=0 idOverflow=0 ids=0",
                 "PDOW_SUM n=3 t=3 frames=0 printed=0 unprinted=0 tapDropped=0 untimed=0 sdo=0 idOverflow=0 ids=0",
                 "PDO_WRITE admitted node=1 set=0 sub=2 baseline=255 value=1; exact read, one write",
                 "RAW id=9 node=1 failure=4 lost=0 terminals=2 traffic=0 tail=0 tailLost=0 gap=10",
                 result(1, "1800:02", 1, "ff000000"),
                 result(2, "1800:02", 0, "01000000", outcome=3, abort="06090030"),
                 "END raw terminals=2 traffic=0 tail=0 tailLost=0 lost=0; r=release", "garbage line"]
        report = self.capture(lines)
        self.assertEqual(report["watches"][0]["console_gaps"], [(2, 3)])
        self.assertEqual(report["unknown"], ["garbage line"])
        write = report["pdo_records"][0]
        self.assertEqual(write["header"]["failure"], 4)
        self.assertEqual(write["results"][1]["abort"], "06090030")


if __name__ == "__main__":
    unittest.main()
