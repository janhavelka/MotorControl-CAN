#!/usr/bin/env python3
"""Contract tests for scripts/s2_pdo.py: plan checks, the step runner's stop rules and the offline analysis."""
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import s2_pdo as pdo  # noqa: E402

REPOSITORY = Path(__file__).resolve().parent.parent

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

    def __init__(self, replies=None, pdo="1", watch=0, soak=0, capture=None):
        self.replies = dict(replies or {})
        self.writes = []
        self.queue = bytearray(b"stale\n")
        self.pdo, self.watch, self.soak, self.capture = pdo, watch, soak, capture
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
            if self.capture is not None:
                extra += " capture=%d" % self.capture
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


class ReleaseDisciplineTest(unittest.TestCase):
    """2026-11-06: one replay plan expected the wrong release line after E, another sent N2,p while
    the N2,o result was held and left node 2 Operational. Both are refused before anything is sent."""

    N = {"send": "N2,p", "expect": "NMT admitted", "end": "NMT request"}
    E = {"send": "E2,0", "expect": "PDO_CONFIG admitted", "end": "END PDO"}
    R = {"send": "R2,t,0", "expect": "PDO_READ admitted", "end": "END raw"}

    def refused(self, *steps):
        with self.assertRaises(pdo.SessionError):
            pdo.check_plan(plan(*steps))

    def test_each_record_needs_its_own_release_line(self):
        release = {"raw": "RELEASED record", "pdo": "PDO record released", "nmt": "NMT result released"}
        for step, kind in ((self.N, "nmt"), (self.E, "pdo"), (self.R, "raw")):
            self.assertEqual(len(pdo.check_plan(plan(step, {"send": "r", "expect": release[kind]}))), 2)
            for other in set(release.values()) - {release[kind]}:
                self.refused(step, {"send": "r", "expect": other})

    def test_new_work_while_a_record_is_held_is_refused(self):
        self.refused({"send": "N2,o", "expect": "NMT admitted", "end": "NMT request"}, {"wait_s": 3}, self.N)
        self.refused(self.E, self.R)
        self.refused(self.R, {"send": "4", "expect": "IDENTITY admitted", "end": "END raw"})

    def test_a_release_without_a_record_of_this_plan_must_name_a_release_line(self):
        # A leading r may release a record an earlier session left (1305Zb, 1616Z-d).
        for line in ("RELEASED record", "PDO record released", "NMT result released"):
            self.assertEqual(len(pdo.check_plan(plan({"send": "r", "expect": line}))), 1)
        self.refused({"send": "r", "expect": "released"})
        self.refused(self.R, {"send": "r", "expect": "RELEASED record"}, {"send": "r", "expect": "CAN ready"})

    def test_watch_lines_and_waits_do_not_hold_records(self):
        steps = (self.N, {"send": "P60", "expect": "PDOW admitted"}, {"wait_s": 2},
                 {"send": "r", "expect": "NMT result released"}, self.E)
        self.assertEqual(len(pdo.check_plan(plan(*steps))), 5)

    def test_retained_plans_follow_the_discipline(self):
        # The two plans that stopped on 2026-11-06 are refused now; the replay that completed and the
        # packet 6 sessions pass, apart from commands no longer offered (q, w, m), refused separately.
        stopped = {"20261106T1613Z-pdo-p7-replay", "20261106T1614Z-pdo-p7-replay-b",
                   "20261106T1615Z-pdo-p7-replay-c"}
        paths = sorted((REPOSITORY / "docs/bench").glob("20261106T1*-pdo-*/plan.json"))
        self.assertIn("20261106T1616Z-pdo-p7-replay-d", {path.parent.name for path in paths})
        for path in paths:
            data = json.loads(path.read_text(encoding="utf-8"))
            try:
                pdo.check_plan(data)
                problem = None
            except pdo.SessionError as error:
                problem = str(error)
            release = problem is not None and ("is held" in problem or "releases the" in problem or
                                                "no release line" in problem)
            self.assertEqual(release, path.parent.name in stopped, (path.parent.name, problem))

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
        self.assertEqual(len(pdo.check_plan(plan({"send": "E1,0", "expect": "PDO_CONFIG admitted",
                                                  "end": "END PDO"}))), 1)
        for bad in ({}, {"purpose": "x", "steps": []},
                    plan({"send": "P5"}),  # no expected reply
                    plan({"send": "x", "expect": "a"}),  # unreviewed byte
                    plan({"send": "Q1", "expect": "a"}),  # not a line command
                    plan({"send": "N1,o\n", "expect": "a"}),
                    plan({"wait_s": 0}), plan({"wait_s": 901}), plan({"note": "nothing"}),
                    plan({"send": "P5", "expect": "a", "retry": 1}), plan({"wait_s": 1, "end": "x"}),
                    plan({"send": "W1,0,5,0,100", "expect": "PDO_WRITE admitted"}),  # no terminal to check
                    plan({"send": "N1,o", "expect": "NMT admitted", "end": "END raw"}),
                    plan({"send": "h", "expect": "HEARTBEAT experiment admitted"}),
                    plan({"send": "4", "expect": "IDENTITY admitted"}),
                    # The terminal check follows the command, so its expect text cannot loosen it.
                    plan({"send": "N1,o", "expect": "NMT", "end": "NMT request"}),
                    plan({"send": "W1,0,5,0,100", "expect": "PDO_WRITE", "end": "END raw"}),
                    plan({"send": "R1,t,0", "expect": "PDO_READ", "end": "END raw"}),
                    # q, w and m change drive or network state with no checked terminal here.
                    plan({"send": "q", "expect": "NMT Pre-operational admitted"}),
                    plan({"send": "w", "expect": "NMT Pre-operational admitted"}),
                    plan({"send": "m", "expect": "PDO admitted"}),
                    # E is checked like W: its own admission and the PDO record's end, nothing looser.
                    plan({"send": "E2,100", "expect": "PDO_CONFIG admitted"}),
                    plan({"send": "E2,100", "expect": "PDO_CONFIG", "end": "END PDO"}),
                    plan({"send": "E2,100", "expect": "PDO_CONFIG admitted", "end": "END raw"}),
                    plan({"send": "E2,100", "expect": "PDO admitted", "end": "END PDO"}),
                    plan({"wait_s": 1, "require": []}),
                    plan({"wait_s": 1, "require": [{"kind": "NETWORK"}]}),
                    plan({"wait_s": 1, "require": [{"kind": "NETWORK", "match": {"state": 127}, "x": 1}]})):
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
                                             {"send": "N1,o", "expect": "NMT admitted", "end": "NMT request"},
                                             {"send": "r", "expect": "NMT result released"},
                                             {"send": "W1,0,5,0,100", "expect": "PDO_WRITE admitted",
                                              "end": "END raw"}])
        self.assertEqual(code, 1)
        self.assertEqual(meta["reason"], "refused")
        # Nothing after the refusal but the local watch stop: no NMT, write or stop on CAN.
        self.assertEqual(board.writes, [b"p", b"P5\n", b"N1,o\n", b"P0\n"])
        self.assertEqual(meta["steps"][-1]["outcome"], "watch_ended")

    def test_silence_stops_the_plan_without_cleanup_when_no_watch_runs(self):
        board = Board({})
        code, meta, _ = self.session(board, [{"send": "R1,t,0", "expect": "PDO_READ admitted", "timeout_s": 0.1,
                                              "end": "END raw"},
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
                           (Board(watch=1), "watch_running"), (Board(pdo="4"), "image_without_pdo_lines"),
                           (Board(pdo="3", capture=1), "capture_held")):
            code, meta, _ = self.session(board, [{"send": "P5", "expect": "PDOW admitted"}])
            self.assertEqual((code, meta["reason"]), (1, why))
            self.assertEqual(board.writes, [b"p"])

    def test_an_image_without_e_lines_is_never_sent_one(self):
        # On a pdo=1 image E is unknown and its digits are single-byte commands: refuse unsent.
        board = Board({b"P5\n": WATCH}, pdo="1")
        code, meta, _ = self.session(board, [{"send": "P5", "expect": "PDOW admitted"}] + CONFIG_STEPS)
        self.assertEqual((code, meta["reason"]), (1, "image_without_pdo_config"))
        self.assertEqual(board.writes, [b"p"])
        # The same image still runs a plan of its own lines, and a pdo=2 image runs both.
        for version in ("1", "2"):
            board = Board({b"P5\n": WATCH}, pdo=version)
            code, meta, _ = self.session(board, [{"send": "P5", "expect": "PDOW admitted"}])
            self.assertEqual((code, meta["reason"]), (0, "completed"))

    def write_reply(self, failure=0, outcome=0, readback="64000000", node=2):
        lines = ["PDO_WRITE admitted node=%d set=0 sub=5 baseline=0 value=100; exact read, one write" % node,
                 "RAW id=9 node=%d failure=%d lost=0 terminals=3 traffic=0 tail=0 tailLost=0 gap=10" % (node, failure),
                 result(1, "1800:05", 2, "00000000"), result(2, "1800:05", 0, "00000000", outcome=outcome),
                 result(3, "1800:05", 2, readback), "END raw terminals=3 traffic=0 tail=0 tailLost=0 lost=0; r=release"]
        return ("\n".join(lines) + "\n").encode("ascii")

    def write_then_nmt(self, reply):
        board = Board({b"W2,0,5,0,100\n": reply, b"r": b"RELEASED record; channel uncertainty unchanged\n",
                       b"N2,p\n": b"NMT admitted node=2 cmd=128; addressed\n"
                                  b"NMT request=4 target=2 cmd=128 outcome=0 possible=1 txConfirmed=1 completed=900; x\n"})
        return board, self.session(board, [
            {"send": "W2,0,5,0,100", "expect": "PDO_WRITE admitted", "end": "END raw"},
            {"send": "r", "expect": "RELEASED record"},
            {"send": "N2,p", "expect": "NMT admitted", "end": "NMT request"}])

    def test_a_successful_write_lets_the_plan_continue(self):
        board, (code, meta, _) = self.write_then_nmt(self.write_reply())
        self.assertEqual((code, meta["reason"]), (0, "completed"))
        self.assertEqual(board.writes, [b"p", b"W2,0,5,0,100\n", b"r", b"N2,p\n"])

    def test_a_failed_write_terminal_stops_every_later_step(self):
        # An acknowledged write whose readback mismatched (READBACK, 8), a write the server aborted (SERVICE, 4),
        # a RESULT failure under a header that claims success, a readback of another value, and a record of
        # the other node: none may be followed by the release or the NMT step.
        for reply, problem in ((self.write_reply(failure=8), "raw_failure_8"),
                               (self.write_reply(failure=4, outcome=3), "raw_failure_4"),
                               (self.write_reply(outcome=3), "raw_results_incomplete"),
                               (self.write_reply(readback="c8000000"), "write_readback_differs"),
                               (self.write_reply(node=1), "raw_wrong_node")):
            board, (code, meta, _) = self.write_then_nmt(reply)
            self.assertEqual((code, meta["reason"]), (1, "terminal_failed"), problem)
            self.assertEqual(meta["steps"][0]["problem"], problem)
            self.assertEqual(board.writes, [b"p", b"W2,0,5,0,100\n"], problem)

    def test_a_failed_nmt_terminal_stops_the_plan(self):
        for line, problem in ((b"outcome=3 possible=1 txConfirmed=0", "nmt_not_transmitted"),
                              (b"outcome=0 possible=1 txConfirmed=0", "nmt_not_transmitted")):
            board = Board({b"N2,o\n": b"NMT admitted node=2 cmd=1; addressed\nNMT request=4 target=2 cmd=1 " +
                           line + b" completed=900; x\n"})
            code, meta, _ = self.session(board, [{"send": "N2,o", "expect": "NMT admitted", "end": "NMT request"},
                                                 {"send": "r", "expect": "NMT result released"}])
            self.assertEqual((meta["reason"], meta["steps"][0]["problem"]), ("terminal_failed", problem))
            self.assertEqual(board.writes, [b"p", b"N2,o\n"])
        board = Board({b"N2,o\n": b"NMT admitted node=2 cmd=1; addressed\nNMT request=4 target=1 cmd=1 outcome=0 "
                                    b"possible=1 txConfirmed=1 completed=900; x\n"})
        code, meta, _ = self.session(board, [{"send": "N2,o", "expect": "NMT admitted", "end": "NMT request"}])
        self.assertEqual(meta["steps"][0]["problem"], "nmt_wrong_request")

    def test_requirements_gate_a_step_before_it_is_sent(self):
        network = (b"NETWORK node=2 epoch=1 state=127 qualified=1 producerKnown=1 ms=1000 pending=0 observed=0 "
                   b"expired=0 exact=0 bounds=1 lo=1 hi=2 p=3 dropped=0\n"
                   b"END network; boot-up requires an actual observed00 frame; no inferred reset\n")
        start = (b"NMT admitted node=2 cmd=1; addressed\n"
                 b"NMT request=4 target=2 cmd=1 outcome=0 possible=1 txConfirmed=1 completed=900; x\n")
        steps = lambda state: [{"send": "n", "expect": "NETWORK", "end": "END network"},
                               {"send": "N2,o", "expect": "NMT admitted", "end": "NMT request",
                                "require": [{"kind": "NETWORK", "where": {"node": 2},
                                             "match": {"state": state, "qualified": 1}}]}]
        board = Board({b"n": network, b"N2,o\n": start})
        code, meta, _ = self.session(board, steps(127))
        self.assertEqual((code, meta["reason"]), (0, "completed"))
        self.assertEqual(board.writes, [b"p", b"n", b"N2,o\n"])
        # The required Pre-operational state is not what the capture shows: the Start is never sent.
        board = Board({b"n": network.replace(b"state=127", b"state=5"), b"N2,o\n": start})
        code, meta, _ = self.session(board, steps(127))
        self.assertEqual((code, meta["reason"]), (1, "requirement_failed"))
        self.assertIn("state=5", meta["steps"][1]["problem"])
        self.assertEqual(board.writes, [b"p", b"n"])
        # No such record at all also fails closed.
        board = Board({b"N2,o\n": start})
        code, meta, _ = self.session(board, steps(127)[1:])
        self.assertEqual(meta["reason"], "requirement_failed")
        self.assertEqual(board.writes, [b"p"])
        # A baseline value read earlier can be required too; RESULT objects are named as in the grammar.
        reading = ("\n".join(["PDO_READ admitted node=1 dir=t set=0; nine uploads, no write",
                              "RAW id=9 node=1 failure=0 lost=0 terminals=9 traffic=0 tail=0 tailLost=0 gap=10"] +
                             [result(i + 1, o, s, d) for i, (o, s, d) in enumerate(
                                 [("1800:01", 4, "81010000"), ("1800:02", 1, "ff000000"),
                                  ("1800:03", 2, "64000000"), ("1800:05", 2, "00000000"),
                                  ("1a00:00", 1, "03000000"), ("1a00:01", 4, "10004160"),
                                  ("1a00:02", 4, "08006160"), ("1a00:03", 4, "20006460"),
                                  ("1a00:04", 4, "00000000")])] +
                             ["END raw terminals=9 traffic=0 tail=0 tailLost=0 lost=0; r=release"]) + "\n").encode()
        write = ("\n".join(["PDO_WRITE admitted node=1 set=0 sub=5 baseline=0 value=100; exact read, one write",
                            "RAW id=10 node=1 failure=0 lost=0 terminals=3 traffic=0 tail=0 tailLost=0 gap=10",
                            result(1, "1800:05", 2, "00000000"), result(2, "1800:05", 0, "00000000"),
                            result(3, "1800:05", 2, "64000000"),
                            "END raw terminals=3 traffic=0 tail=0 tailLost=0 lost=0; r=release"]) + "\n").encode()
        for wanted, outcome in (("00000000", "completed"), ("64000000", "requirement_failed")):
            board = Board({b"R1,t,0\n": reading, b"r": b"RELEASED record; channel uncertainty unchanged\n",
                           b"W1,0,5,0,100\n": write})
            code, meta, _ = self.session(board, [
                {"send": "R1,t,0", "expect": "PDO_READ admitted", "end": "END raw"},
                {"send": "r", "expect": "RELEASED record"},
                {"send": "W1,0,5,0,100", "expect": "PDO_WRITE admitted", "end": "END raw",
                 "require": [{"kind": "RESULT", "where": {"object": "1800:05"}, "match": {"data": wanted}},
                             {"kind": "RESULT", "where": {"object": "1800:02"}, "match": {"data": "ff000000"}}]}])
            self.assertEqual(meta["reason"], outcome)
            self.assertEqual(board.writes[-1] == b"W1,0,5,0,100\n", outcome == "completed")

    def config_then_nmt(self, reply):
        board = Board({b"E2,100\n": reply, b"r": b"PDO record released; configuration/uncertainty retained\n",
                       b"N2,o\n": b"NMT admitted node=2 cmd=1; addressed\n"
                                  b"NMT request=4 target=2 cmd=1 outcome=0 possible=1 txConfirmed=1 completed=900; x\n"},
                      pdo="2")
        return board, self.session(board, CONFIG_STEPS + [
            {"send": "r", "expect": "PDO record released"},
            {"send": "N2,o", "expect": "NMT admitted", "end": "NMT request"}])

    def test_a_confirmed_configuration_lets_the_plan_continue(self):
        board, (code, meta, _) = self.config_then_nmt(config_reply())
        self.assertEqual((code, meta["reason"]), (0, "completed"), meta["steps"])
        self.assertEqual(board.writes, [b"p", b"E2,100\n", b"r", b"N2,o\n"])
        # The reverse change, its own bytes and readback.
        board = Board({b"E1,0\n": config_reply(node=1, raw=0)}, pdo="2")
        code, meta, _ = self.session(board, [{"send": "E1,0", "expect": "PDO_CONFIG admitted", "end": "END PDO"}])
        self.assertEqual((code, meta["reason"]), (0, "completed"), meta["steps"])

    def test_a_failed_or_unconfirmed_configuration_stops_every_later_step(self):
        cases = ((config_reply(header={"failure": 3}), "pdo_failure_3"),  # SERVICE
                 (config_reply(header={"failure": 9, "confirmed": 0, "uncertain": 1, "failedStep": 2}),
                  "pdo_failure_9"),  # READBACK
                 (config_reply(header={"lost": 1}), "pdo_failure_0"),
                 (config_reply(header={"node": 1}), "pdo_wrong_node"),
                 (config_reply(header={"raw": 0}), "pdo_wrong_request"),
                 (config_reply(header={"transition": 1}), "pdo_wrong_request"),
                 (config_reply(header={"confirmed": 0}), "pdo_not_confirmed"),
                 (config_reply(header={"uncertain": 1}), "pdo_not_confirmed"),
                 (config_reply(header={"ackMask": "00"}), "pdo_not_confirmed"),
                 (config_reply(header={"migrated": 0}), "pdo_not_confirmed"),
                 (config_reply(header={"lastStep": 9}), "pdo_not_confirmed"),
                 (config_reply(header={"failedStep": 10}), "pdo_not_confirmed"),
                 (config_reply(header={"terminals": 18}), "pdo_results_incomplete"),
                 (config_reply(drop_result=18), "pdo_results_incomplete"),
                 (config_reply(outcome_at=14), "pdo_results_incomplete"),
                 (config_reply(write_object="1800:03"), "pdo_write_differs"),
                 (config_reply(readback="c8000000"), "pdo_readback_differs"),
                 (config_reply(tx="2b001805c8000000"), "pdo_write_bytes_differ"),
                 (config_reply(extra_tx="2300180182010080"), "pdo_write_bytes_differ"),
                 (config_reply(old_header=True), "pdo_terminal_unparsed"))
        for reply, problem in cases:
            board, (code, meta, _) = self.config_then_nmt(reply)
            self.assertEqual((code, meta["reason"]), (1, "terminal_failed"), problem)
            self.assertEqual(meta["steps"][0]["problem"], problem)
            self.assertEqual(board.writes, [b"p", b"E2,100\n"], problem)

    def test_a_refused_configuration_ends_the_plan(self):
        board = Board({b"E2,100\n": b"PDO_CONFIG refused; no work\n"}, pdo="2")
        code, meta, _ = self.session(board, CONFIG_STEPS + [{"send": "r", "expect": "PDO record released"}])
        self.assertEqual((code, meta["reason"]), (1, "refused"))
        self.assertEqual(board.writes, [b"p", b"E2,100\n"])

    def test_session_names_its_tools(self):
        code, meta, _ = self.session(Board(), [{"wait_s": 0.05}])
        self.assertEqual(set(meta["tools"]), {"s2_pdo.py", "s2_evidence.py"})
        self.assertTrue(all(len(digest) == 64 for digest in meta["tools"].values()))

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


CONFIG_STEPS = [{"send": "E2,100", "expect": "PDO_CONFIG admitted", "end": "END PDO"}]
SNAPSHOT = (("1800:01", 4, "8%d010000"), ("1800:02", 1, "ff000000"), ("1800:03", 2, "64000000"),
            ("1800:05", 2, None), ("1a00:00", 1, "03000000"), ("1a00:01", 4, "10004160"),
            ("1a00:02", 4, "08006160"), ("1a00:03", 4, "20006460"), ("1a00:04", 4, "00000000"))


def config_reply(node=2, raw=100, header=None, drop_result=None, outcome_at=None, write_object="1800:05",
                 readback=None, tx=None, extra_tx=None, old_header=False):
    """One E record in the firmware's format: nine reads, the write, nine reads, the TX frames."""
    before = (0 if raw else 100).to_bytes(2, "little").hex() + "0000"
    after = raw.to_bytes(2, "little").hex() + "0000"
    values = dict(failure=0, lost=0, terminals=19, traffic=19, tail=0, tailLost=0, confirmed=1, effects=1,
                  uncertain=0, ackMask="01", cfg=2, migrated=1, gap=10, node=node, transition=2, raw=raw,
                  lastStep=10, effectStep=1, failedStep=0)
    values.update(header or {})
    if old_header:
        for key in ("node", "transition", "raw", "lastStep", "effectStep", "failedStep"):
            del values[key]
    lines = ["PDO_CONFIG admitted node=%d raw=%d; TPDO1 1800:05 raw only: nine exact reads, one write, "
             "nine exact reads; no retry" % (node, raw),
             "PDO " + " ".join("%s=%s" % item for item in values.items())]
    results = []
    for phase in ("before", "after"):
        for obj, size, data in SNAPSHOT:
            if data is None:
                data = before if phase == "before" else (readback or after)
            results.append((obj, size, data % node if "%d" in data else data, 0))
        if phase == "before":
            results.append((write_object, 0, "00000000", 1))
    for step, (obj, size, data, direction) in enumerate(results, 1):
        if drop_result == step:
            continue
        outcome = 3 if outcome_at == step else 0
        lines.append("RESULT op=21 req=%d oreq=%d step=%d object=%s dir=%d outcome=%d ack=%d possible=1 "
                     "uncertain=0 abort=00000000 cleanup=0 size=%d data=%s admitted=%d completed=%d ne=1 reason=0 "
                     "st=3f" % (step, step + 100, step, obj, direction, outcome, int(outcome == 0), size, data,
                                1000 + 100 * step, 1050 + 100 * step))
    frames = []
    for step, (obj, size, data, direction) in enumerate(results, 1):
        index, sub = obj.split(":")
        mux = index[2:] + index[:2] + sub
        payload = ("40" + mux + "00000000") if not direction else (tx or ("2b" + mux + after))
        frames.append("TRAFFIC k=1 p=%d id=%x ext=0 rtr=0 dlc=8 data=%s ok=1 exact=0 t=0 bounds=0 lo=0 hi=0 epoch=1 "
                      "txk=0 txr=%d txf=1" % (1000 + 100 * step, 0x600 + node, payload, step))
    if extra_tx:
        frames.append("TRAFFIC k=1 p=9000 id=%x ext=0 rtr=0 dlc=8 data=%s ok=1 exact=0 t=0 bounds=0 lo=0 hi=0 "
                      "epoch=1 txk=0 txr=99 txf=1" % (0x600 + node, extra_tx))
    lines += frames + ["END PDO; map readback and sample qualification are separate; r=release"]
    return ("\n".join(lines) + "\n").encode("ascii")


def capture_reply(identity=1, seconds=2, frames=8, reason="duration", stored=2, overflow=0, tap=0, lost=0,
                  refused=0, exit_=1, reopened=1, f_lines=None, other_cap=None):
    """The firmware's C-line output, as SilentCapture.cpp formats it."""
    cap = identity if other_cap is None else other_cap
    lines = ["CAP admitted cap=%d seconds=%d frames=%d gpio=18 silentHigh=1; TX gate closed, silent level "
             "requested, nothing sent" % (identity, seconds, frames),
             "CAP_REC cap=%d reason=%s seconds=%d capacity=%d stored=%d overflow=%d tapDropped=%d pre=0 post=0 "
             "untimed=0 txRefused=%d lost=%d" % (identity, reason, seconds, frames, stored, overflow, tap, refused,
                                                lost),
             "CAP_TIME cap=%d begin=1000 end=%d nominal=%d gap=900 clock=esp_timer_us" %
             (cap, 1000 + seconds * 1000000, 1000 + seconds * 1000000),
             "CAP_LEVEL cap=%d gpio=18 silentHigh=1 begin=1 exit=%d reopened=%d at=%d latched=0" %
             (identity, exit_, reopened, 1000 + seconds * 1000000)]
    for which in (0, 1):
        lines.append("CAP_CTRL cap=%d which=%d t=%d epoch=1 counters=1 tec=0 rec=0 inhibited=0 adapterInhibited=0 "
                     "twaiDropped=0 twaiErrors=0 twaiFlags=00000000" % (identity, which, 1000 + which * seconds))
    for which in (0, 1):
        lines.append("CAP_OWNER cap=%d which=%d rx=%d dropped=0 lost=0 unmatched=%d tx=4 txRej=0 busOff=0 refused=0" %
                     (identity, which, 10 + which * stored, 10 + which * stored))
    lines.append("CAP_IMAGE cap=%d base=%s elf=%s" % (identity, "a" * 40, "b" * 64))
    if f_lines is None:
        f_lines = ["CAP_F cap=%d i=%d id=%x ext=0 rtr=0 dlc=1 data=7f bounds=1 exact=0 lo=%d hi=%d epoch=1 pre=0 "
                   "post=0" % (identity, i, 0x701 + i, 1000 + i * 1000, 1200 + i * 1000) for i in range(stored)]
    lines += f_lines
    lines.append("END capture cap=%d stored=%d overflow=%d lost=%d; r=release" % (identity, stored, overflow, lost))
    return ("\n".join(lines) + "\n").encode("ascii")


CAPTURE_STEP = {"send": "C1,2,8", "expect": "CAP admitted", "end": "END capture", "end_timeout_s": 5}


class CaptureTest(unittest.TestCase):
    """The C line (examples/s2_bench/SilentCapture.h): one held record, its own terminal check, the
    pdo=3 protocol gate and a local-only cleanup. The fake board answers as the firmware formats."""

    session = RunTest.session

    def test_capture_plan_rules(self):
        self.assertEqual(len(pdo.check_plan(plan(CAPTURE_STEP, {"send": "r", "expect": "CAP record released"}))), 2)
        self.assertEqual(len(pdo.check_plan(plan({"send": "C0", "expect": "CAP_STOPPING"}))), 1)
        for bad in (plan(dict(CAPTURE_STEP, expect="CAP")),  # A looser admission.
                    plan(dict(CAPTURE_STEP, end="END raw")),
                    plan({"send": "C1,2,8", "expect": "CAP admitted"}),  # No terminal to check.
                    plan(dict(CAPTURE_STEP, end_timeout_s=2)),  # The record prints after the duration.
                    plan(dict(CAPTURE_STEP, send="C1,2")),
                    plan(dict(CAPTURE_STEP, send="C1,x,8"))):
            with self.assertRaises(pdo.SessionError):
                pdo.check_plan(bad)

    def test_a_capture_holds_its_record_until_its_own_release(self):
        n = {"send": "N2,p", "expect": "NMT admitted", "end": "NMT request"}
        with self.assertRaises(pdo.SessionError):
            pdo.check_plan(plan(CAPTURE_STEP, n))
        with self.assertRaises(pdo.SessionError):
            pdo.check_plan(plan(CAPTURE_STEP, {"send": "r", "expect": "RELEASED record"}))
        with self.assertRaises(pdo.SessionError):
            pdo.check_plan(plan(n, CAPTURE_STEP))
        # C0 and waits hold nothing; the release names the capture's own line.
        self.assertEqual(len(pdo.check_plan(plan({"send": "C0", "expect": "CAP_STOPPING"}, n))), 2)
        self.assertEqual(len(pdo.check_plan(plan(CAPTURE_STEP, {"wait_s": 1},
                                                 {"send": "r", "expect": "CAP record released"}, n))), 4)

    def test_an_image_without_c_lines_is_never_sent_one(self):
        # On a pdo=2 image C is unknown and its digits would be single-byte commands: refused unsent.
        for version in ("1", "2"):
            board = Board({b"C1,2,8\n": capture_reply()}, pdo=version)
            code, meta, _ = self.session(board, [CAPTURE_STEP])
            self.assertEqual((code, meta["reason"]), (1, "image_without_pdo_config"))
            self.assertEqual(board.writes, [b"p"])

    def test_a_complete_capture_lets_the_plan_continue(self):
        board = Board({b"C1,2,8\n": capture_reply(), b"r": b"CAP record released; level, gate and SDO state "
                                                          b"unchanged\n"}, pdo="3", capture=0)
        code, meta, out = self.session(board, [CAPTURE_STEP, {"send": "r", "expect": "CAP record released"}])
        self.assertEqual((code, meta["reason"]), (0, "completed"))
        self.assertEqual(board.writes, [b"p", b"C1,2,8\n", b"r"])
        report = pdo.analyze(out)
        # The board's leftover "stale" line, retained before the plan, is the only unknown one.
        self.assertEqual((report["unknown"], report["malformed"]), (["stale"], []))
        (item,) = report["captures"]
        self.assertEqual(item["record"]["reason"], "duration")
        self.assertEqual(sorted(item["ids"]), ["701", "702"])
        self.assertEqual(item["frames"], 2)
        self.assertEqual(item["level"]["exit"], 1)
        self.assertEqual(sorted(item["ctrl"]), [0, 1])

    def test_a_failed_capture_record_stops_every_later_step(self):
        cases = {"capture_ended_stop": capture_reply(reason="stop"),
                 "capture_ended_controller": capture_reply(reason="controller", exit_=3, reopened=0),
                 "capture_incomplete": capture_reply(overflow=3),
                 "capture_send_attempted": capture_reply(refused=1),
                 "capture_exit_not_normal": capture_reply(exit_=2, reopened=0),
                 "capture_frames_differ": capture_reply(stored=2, f_lines=[]),
                 "capture_record_unparsed": capture_reply(other_cap=9),
                 "capture_wrong_plan": capture_reply(frames=16)}
        for problem, reply in cases.items():
            board = Board({b"C1,2,8\n": reply}, pdo="3", capture=0)
            code, meta, _ = self.session(board, [CAPTURE_STEP, {"send": "r", "expect": "CAP record released"}])
            self.assertEqual((code, meta["reason"], meta["steps"][0].get("problem")),
                             (1, "terminal_failed", problem), problem)
            self.assertEqual(board.writes, [b"p", b"C1,2,8\n"], problem)

    def test_zero_frames_is_a_complete_record_not_a_proof(self):
        board = Board({b"C1,2,8\n": capture_reply(stored=0)}, pdo="3", capture=0)
        code, meta, _ = self.session(board, [CAPTURE_STEP])
        self.assertEqual((code, meta["reason"]), (0, "completed"))

    def test_an_early_end_while_collecting_sends_only_c0(self):
        admitted = (b"CAP admitted cap=1 seconds=60 frames=8 gpio=18 silentHigh=1; TX gate closed, silent level "
                    b"requested, nothing sent\n")
        # The fake clock runs 10 ms per read, so the 61 s end wait passes quickly.
        step = dict(CAPTURE_STEP, send="C1,60,8", end_timeout_s=61)
        board = Board({b"C1,60,8\n": admitted, b"C0\n": b"CAP_STOPPING; the record follows; nothing sent\n" +
                       capture_reply(seconds=60, reason="stop").split(b"\n", 1)[1]}, pdo="3", capture=0)
        code, meta, _ = self.session(board, [step])
        self.assertEqual((code, meta["reason"]), (1, "no_end"))
        self.assertEqual(board.writes, [b"p", b"C1,60,8\n", b"C0\n"])
        self.assertEqual(meta["steps"][-1]["outcome"], "capture_ended")


class FirmwareTextTest(unittest.TestCase):
    """The firmware's own format strings against the grammar and their buffers, read from source."""

    WIDTH = {"u": 10, "lu": 10, "llu": 20, "d": 11, "lx": 8, "08lx": 8, "02x": 2, "x": 8}

    def literal(self, body):
        return re.sub(r'"\s*"', "", body)

    def fields(self, kind):
        from s2_evidence import GRAMMAR
        names = set()
        for row in GRAMMAR:
            if row.kind == kind:
                names |= {token.split(":")[0] for token in row.fields.split()}
        return names

    def test_capture_lines_are_in_the_grammar_and_fit(self):
        source = (REPOSITORY / "examples/s2_bench/SilentCapture.cpp").read_text(encoding="utf-8")
        header = (REPOSITORY / "examples/s2_bench/SilentCapture.h").read_text(encoding="utf-8")
        capacity = int(re.search(r"CAPTURE_LINE_BYTES = (\d+)", header).group(1))
        kinds = {"CAP_REC": "CAP_REC", "CAP_TIME": "CAP_TIME", "CAP_LEVEL": "CAP_LEVEL", "CAP_CTRL": "CAP_CTRL",
                 "CAP_OWNER": "CAP_OWNER", "CAP_IMAGE": "CAP_IMAGE", "CAP_F": "CAP_F", "END capture": "CAP_END"}
        strings = {self.literal(m) for m in re.findall(r'"((?:CAP_[A-Z]+|END capture) (?:[^"]|"\s*")*?)\\n"',
                                                          source)}
        found = set()
        widths = {"reason": 10, "data": 16, "base": 40, "elf": 64}
        for body in strings:
            tag = body.split(" ")[0] if body.startswith("CAP_") else "END capture"
            found.add(tag)
            names = set(re.findall(r"(\w+)=%", body.partition(";")[0]))
            self.assertTrue(names, tag)
            self.assertLessEqual(names, self.fields(kinds[tag]), tag)
            widest = body
            for name, spec in re.findall(r"(\w+)=%([0-9]*l{0,2}[udxs])", body):
                width = widths.get(name, 64) if spec == "s" else self.WIDTH[spec]
                widest = widest.replace("%s=%%%s" % (name, spec), "%s=%s" % (name, "9" * width), 1)
            self.assertNotIn("%", widest, tag)
            self.assertLess(len(widest) + 1, capacity, tag)  # Newline; vsnprintf's terminator fits too.
        self.assertEqual(found, set(kinds))

    def test_pong_fits_its_buffers(self):
        text = (REPOSITORY / "examples/s2_bench/main/main.cpp").read_text(encoding="utf-8")
        body = self.literal(re.search(r'"PONG (.*?)\\n"\s*,', text, re.S).group(1))
        output = int(re.search(r"char output\[(\d+)\] = \{\};", text).group(1))
        retained = int(re.search(r"char retainedResponse\[(\d+)\];", text).group(1))
        # Booleans print one digit; motion is an unsigned count; idf_ver is esp_app_desc_t's 32-byte text.
        widths = {"base": 40, "elf": 64, "sdk": 31, "t": 20, "motion": 10}
        widest = "PONG " + body
        for name, spec in re.findall(r"(\w+)=%(l{0,2}[us])", body):
            widest = widest.replace("%s=%%%s" % (name, spec), "%s=%s" % (name, "9" * widths.get(name, 1)), 1)
        self.assertNotIn("%", widest)
        self.assertIn("pdo=3", widest)
        self.assertIn("capture=9", widest)
        self.assertLess(len(widest) + 1, output)
        self.assertLess(len(widest) + 1, retained)  # line() keeps only a response shorter than the buffer.


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
        self.assertEqual(stream["printed_stats"]["count"], 4)
        self.assertEqual((stream["printed_stats"]["min_gap"], stream["printed_stats"]["max_gap"]), (99940, 100050))
        self.assertEqual(stream["decoded_first"], {"6064:00": 4728, "606c:00": 0})
        self.assertEqual(stream["decoded_last"], {"6064:00": -5, "606c:00": 0})
        self.assertEqual(stream["decode_failures"], 0)
        start, preop = stream["around_nmt"]
        self.assertEqual((start["nmt"], start["first_after_delay_us"], start["frames_after"]), ("start", 1000, 4))
        # Frames after the Pre-operational completion are reported, never hidden.
        self.assertEqual((preop["nmt"], preop["last_before"], preop["frames_after"]), ("pre-operational", 210050, 1))
        self.assertEqual(watch["ids"]["701"]["heartbeat_states"], [(399000, "05")])
        self.assertEqual(report["pdo_records"][0]["results"][5]["data"], "20006460")

    def test_frames_between_admission_and_tx_completion_are_ambiguous(self):
        frame = ("PDOW_F n=%d t=%d id=182 ext=0 rtr=0 dlc=7 data=50400135030000 timed=1 exact=0 "
                 "lo=%d hi=%d gap=0")
        lines = ["PDOW_BEGIN n=0 t=1 seconds=5 ring=32 ids=16",
                 frame % (1, 1000, 990, 1000),     # before the admission: before the request
                 frame % (2, 2600, 2400, 2500),    # inside [admitted, completed]: order unknown
                 "NMT request=4 target=2 cmd=128 outcome=0 possible=1 txConfirmed=1 completed=3000 "
                 "admitted=2000; retained",
                 frame % (3, 3600, 3400, 3500)]    # after the TX completion
        watch = self.capture(lines)["watches"][0]
        around = watch["ids"]["182"]["around_nmt"][0]
        self.assertEqual((around["admitted"], around["last_before"], around["ambiguous"], around["first_after"],
                          around["frames_after"]), (2000, 1000, 1, 3500, 1))
        # A record without an admission time keeps the old split at the TX completion.
        legacy = self.capture([line.replace(" admitted=2000", "") for line in lines])["watches"][0]
        old = legacy["ids"]["182"]["around_nmt"][0]
        self.assertEqual((old["admitted"], old["last_before"], old["ambiguous"]), (None, 2500, 0))

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


class RetainedTest(unittest.TestCase):
    def test_every_retained_session_fits_the_grammar(self):
        """Every retained session's real firmware output parses: the grammar rows follow the firmware."""
        bench = Path(__file__).resolve().parents[1] / "docs" / "bench"
        sessions = sorted(path.parent for path in bench.glob("*/session.json"))
        self.assertGreaterEqual(len(sessions), 11)
        for directory in sessions:
            report = pdo.analyze(directory)
            self.assertEqual((report["unknown"], report["malformed"]), ([], []), directory.name)
            for watch in report["watches"]:
                self.assertEqual(watch["console_gaps"], [], directory.name)
                self.assertEqual(watch["tap_dropped"], 0, directory.name)


if __name__ == "__main__":
    unittest.main()
