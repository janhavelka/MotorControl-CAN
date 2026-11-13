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
                           (Board(watch=1), "watch_running"), (Board(pdo="6"), "image_without_pdo_lines"),
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


def full_result(step, obj, size, data, direction=0, outcome=0, abort="00000000", op=21):
    """One RESULT line as BenchTerminal.cpp and main.cpp resultLine print it."""
    return ("RESULT op=%d req=%d oreq=%d step=%d object=%s dir=%d outcome=%d ack=%d possible=1 uncertain=0 "
            "abort=%s cleanup=0 size=%d data=%s admitted=%d completed=%d ne=1 reason=0 st=3f" %
            (op, step, step + 100, step, obj, direction, outcome, int(outcome == 0), abort, size, data,
             1000 + 100 * step, 1050 + 100 * step))


def disc_reply(mode="p", seconds=2, select=1, expect=0, header=None, targets=None, results=None,
               selection=None, values=True):
    """One D record as DiscoveryBench.cpp prints it: both nodes present unless told otherwise."""
    code = {"o": 0, "p": 1, "i": 2}[mode]
    count = {0: 0, 1: 2, 2: 18}[code]
    head = dict(id=9, mode=code, seconds=seconds, targets=2, attempted=2, completed=2, admitted=count,
                terminals=count, occupied=2, free=0, unresolved=0, drops=0, cancelled=0, deadline=int(code == 0),
                changed=0, failed=0, lost=0, gap=100)
    head.update(header or {})
    outcome, confidence = {0: (1, 1), 1: (1, 1), 2: (2, 3 if expect else 2)}[code]
    lines = ["DISC admitted mode=%d seconds=%d select=%d expect=%d; uploads only, selection sends nothing; "
             "no retry" % (code, seconds, select, expect),
             "DISC " + " ".join("%s=%s" % item for item in head.items())]
    for node in (1, 2):
        t = dict(node=node, slot=node - 1, outcome=outcome, confidence=confidence, read=0 if code == 0 else 1,
                 state=0 if code == 0 else 2, fields=0 if code == 0 else 1 if code == 1 else 9, uncertain=0,
                 hb=int(code == 0), hbp=500 if code == 0 else 0, boot=0, bootp=0, pdo=0, pdop=0, dup=0,
                 unresolved=0, current=1, completed=2000)
        t.update((targets or {}).get(node, {}))
        lines.append("DISC_T " + " ".join("%s=%s" % item for item in t.items()))
    if values and code == 1:
        lines += ["DISC_PROBE node=%d type=00040192" % node for node in (1, 2)]
    if values and code == 2:
        lines += ["DISC_ID node=%d type=00040192 hw=c hww=1 sw=20 sww=1 entries=3 vendor=00001000 "
                  "product=00000001 revision=00000000 model=0400 firmware=0200" % node for node in (1, 2)]
    s = dict(node=select, attempted=int(bool(select)), selected=int(bool(select)), current=int(bool(select)),
             tx0=count, tx1=count)
    s.update(selection or {})
    lines.append("DISC_SELECT " + " ".join("%s=%s" % item for item in s.items()))
    objects = ["1000:00"] if code == 1 else ["1000:00", "1009:00", "100a:00", "1018:00", "1018:01", "1018:02",
                                             "1018:03", "2000:00", "2001:00"]
    rows = [] if code == 0 else [full_result(i + 1, objects[i % len(objects)], 4, "92010400") for i in range(count)]
    for index, change in (results or {}).items():
        rows[index] = change
    lines += rows
    lines.append("END discovery terminals=%d targets=2; selection sends nothing; r=release" % len(rows))
    return ("\n".join(lines) + "\n").encode("ascii")


COMM_GROUPS = ([("NETWORK", None, None, 6), ("NODE", None, None, 1)] +
               [("PDO", "t", n, 6) for n in range(1, 5)] + [("MAP", "t", n, 5) for n in range(1, 5)] +
               [("PDO", "r", n, 6) for n in range(1, 5)] + [("MAP", "r", n, 5) for n in range(1, 5)])


def comm_reply(node=1, header=None, drop_group=None, swap=False, write_at=None, outcome_at=None):
    """One K record as CommunicationBench.cpp prints it: the eighteen groups, 95 uploads."""
    head = dict(id=40, node=node, failure=0, groups=18, published=18, group=18, outcome=1, uncertain=0, status=0,
                detail=0, terminals=95, grants=95, admission=0, gap=100)
    head.update(header or {})
    lines = ["COMM admitted node=%d groups=18 uploads=95; typed reads only, no write; no retry" % node,
             "COMM " + " ".join("%s=%s" % item for item in head.items())]
    window = "lo=1000 hi=1500 skew=500"
    groups = []
    for kind, direction, number, _ in COMM_GROUPS:
        if kind == "NETWORK":
            groups.append("COMM_NETWORK node=%d sync=00000080 emcy=%08x producer=0 sdo=2 cs=%08x sc=%08x %s" %
                          (node, 0x80 + node, 0x600 + node, 0x580 + node, window))
        elif kind == "NODE":
            groups.append("COMM_NODE node=%d raw=0 %s" % (node, window))
        elif kind == "PDO":
            groups.append("COMM_PDO node=%d dir=%s pdo=%d entries=5 cob=%08x type=255 inhibit=0 compat=0 event=0 %s" %
                          (node, direction, number, 0x180 + node, window))
        else:
            groups.append("COMM_MAP node=%d dir=%s pdo=%d count=0 w1=00000000 w2=00000000 w3=00000000 "
                          "w4=00000000 %s" % (node, direction, number, window))
    if drop_group is not None:
        del groups[drop_group]
    if swap:
        groups[2], groups[3] = groups[3], groups[2]
    lines += groups
    step = 0
    for _, _, _, fields in COMM_GROUPS:
        for sub in range(fields):
            step += 1
            lines.append(full_result(step, "1800:%02x" % sub, 1, "05000000", direction=int(step == write_at),
                                     outcome=3 if step == outcome_at else 0))
    lines.append("END comm terminals=95 groups=18; typed uploads only, nothing written; r=release")
    return ("\n".join(lines) + "\n").encode("ascii")


def hb_reply(node=1, header=None, readback="e8030000", baseline="00000000", tx=None, extra_tx=None,
             directions=(0, 1, 0)):
    """One H record as CommunicationBench.cpp prints it: baseline 0, the write, readback 1000."""
    head = dict(id=50, node=node, failure=0, lost=0, state=4, outcome=1, write=1, ack=1, effects=1, uncertain=0,
                known=1, readback=1000, matched=1, published=1, raw=1000, terminals=3, gap=100)
    prep = dict(node=node, nmt=0, nmtState=0, prepared=1, migrated=1, cfg=2, scope=1, restart=0, persistence=0,
                status=0, detail=0)
    for key, value in (header or {}).items():
        (prep if key in prep and key != "node" else head)[key] = value
    lines = ["HB admitted node=%d raw=1000; exact baseline 0, one 1017:00 write, exact readback; no save, reset "
             "or retry" % node, "HB " + " ".join("%s=%s" % item for item in head.items()),
             "HB_PREP " + " ".join("%s=%s" % item for item in prep.items()),
             full_result(1, "1017:00", 2, baseline, directions[0], op=50),
             full_result(1, "1017:00", 0, "00000000", directions[1], op=51),
             full_result(2, "1017:00", 2, readback, directions[2], op=51)]
    frames = [("1", "40171000" + "00000000"), ("0", "4b171000" + baseline), ("1", tx or "2b171000e8030000"),
              ("0", "60171000" + "00000000"), ("1", "40171000" + "00000000"), ("0", "4b171000" + readback)]
    if extra_tx:
        frames.append(("1", extra_tx))
    for i, (kind, data) in enumerate(frames):
        ident = 0x600 + node if kind == "1" else 0x580 + node
        suffix = " txk=0 txr=%d txf=1" % (i + 1) if kind == "1" else ""
        lines.append("TRAFFIC k=%s p=%d id=%x ext=0 rtr=0 dlc=8 data=%s ok=1 exact=0 t=0 bounds=0 lo=0 hi=0 "
                     "epoch=1%s" % (kind, 1000 + 10 * i, ident, data, suffix))
    lines.append("END heartbeat terminals=3 traffic=%d tail=0 tailLost=0 lost=0; no save, reset or restore; "
                 "r=release" % len(frames))
    return ("\n".join(lines) + "\n").encode("ascii")


DISC_STEP = {"send": "Dp,2,1,0", "expect": "DISC admitted", "end": "END discovery", "end_timeout_s": 5}
COMM_STEP = {"send": "K1", "expect": "COMM admitted", "end": "END comm", "end_timeout_s": 30}
HB_STEP = {"send": "H1,1000", "expect": "HB admitted", "end": "END heartbeat", "end_timeout_s": 10}


class CommissioningTest(unittest.TestCase):
    """The D, K and H lines (PONG pdo=4): one held record each, released by its own line, refused on
    older images before anything is sent, and each checked against its own terminal. The fake board
    answers as DiscoveryBench.cpp and CommunicationBench.cpp format their records."""

    session = RunTest.session

    def refused(self, *steps):
        with self.assertRaises(pdo.SessionError):
            pdo.check_plan(plan(*steps))

    def test_line_rules(self):
        for step in (DISC_STEP, COMM_STEP, HB_STEP, dict(DISC_STEP, send="Do,30,0,0", end_timeout_s=35),
                     dict(DISC_STEP, send="Di,8,2,1", end_timeout_s=10), dict(COMM_STEP, send="K2"),
                     dict(HB_STEP, send="H2,1000")):
            self.assertEqual(len(pdo.check_plan(plan(step))), 1, step)
        for bad in (dict(HB_STEP, send="H1,0"),  # The reverse transition is not reviewed.
                    dict(HB_STEP, send="H1,500"), dict(HB_STEP, send="H3,1000"), dict(HB_STEP, send="H1"),
                    dict(COMM_STEP, send="K3"), dict(COMM_STEP, send="K1,t,0"),  # No object selector.
                    dict(DISC_STEP, send="Dx,2,1,0"), dict(DISC_STEP, send="Dp,2,1,1"),  # Expect is identity only.
                    dict(DISC_STEP, send="Dp,0,1,0"), dict(DISC_STEP, send="Dp,61,1,0", end_timeout_s=70),
                    dict(DISC_STEP, send="Dp,2,3,0"), dict(DISC_STEP, send="Dp,2,1"),
                    dict(DISC_STEP, end_timeout_s=2),  # The record ends only at its window.
                    dict(DISC_STEP, expect="DISC"), dict(DISC_STEP, end="END raw"),
                    dict(COMM_STEP, expect="COMM"), dict(COMM_STEP, end="END PDO"),
                    dict(HB_STEP, expect="HEARTBEAT experiment admitted"), dict(HB_STEP, end="END raw"),
                    {"send": "K1", "expect": "COMM admitted"}):  # No terminal to check.
            self.refused(bad)

    def test_each_record_needs_its_own_release(self):
        release = {"Dp,2,1,0": "DISC record released", "K1": "COMM record released", "H1,1000": "HB record released"}
        steps = {"Dp,2,1,0": DISC_STEP, "K1": COMM_STEP, "H1,1000": HB_STEP}
        for text, step in steps.items():
            self.assertEqual(len(pdo.check_plan(plan(step, {"send": "r", "expect": release[text]}))), 2)
            for other in set(pdo.RELEASE_LINES.values()) - {release[text]}:
                self.refused(step, {"send": "r", "expect": other})
            # New work while the record is held is refused, whatever it is.
            for later in (DISC_STEP, COMM_STEP, HB_STEP, {"send": "N1,p", "expect": "NMT admitted", "end": "NMT request"},
                          {"send": "R1,t,0", "expect": "PDO_READ admitted", "end": "END raw"}):
                self.refused(step, later)
            # A held raw, PDO or NMT record refuses these too.
            self.refused({"send": "N1,p", "expect": "NMT admitted", "end": "NMT request"}, step)

    def test_an_image_before_pdo4_is_never_sent_d_k_or_h(self):
        # On a pdo=3 image the letters are unknown and their digits single-byte commands: K1 would be an
        # UNKNOWN byte and then a node-1 identity read, H1,1000 the same and four more bytes.
        for step in (DISC_STEP, COMM_STEP, HB_STEP):
            for version in ("1", "2", "3"):
                board = Board({}, pdo=version, capture=0)
                code, meta, _ = self.session(board, [step])
                self.assertEqual((code, meta["reason"]), (1, "image_without_pdo_config"), (step, version))
                self.assertEqual(board.writes, [b"p"])

    def run_one(self, step, reply, release_line):
        board = Board({pdo.encode(step["send"]): reply, b"r": release_line.encode() + b"; x\n",
                       b"N1,p\n": b"NMT admitted node=1 cmd=128; addressed\n"
                                  b"NMT request=4 target=1 cmd=128 outcome=0 possible=1 txConfirmed=1 completed=900; x\n"},
                      pdo="4", capture=0)
        code, meta, out = self.session(board, [step, {"send": "r", "expect": release_line},
                                               {"send": "N1,p", "expect": "NMT admitted", "end": "NMT request"}])
        return board, code, meta, out

    def check_cases(self, step, good, cases, release_line):
        board, code, meta, out = self.run_one(step, good, release_line)
        self.assertEqual((code, meta["reason"]), (0, "completed"), meta["steps"])
        self.assertEqual(board.writes, [b"p", pdo.encode(step["send"]), b"r", b"N1,p\n"])
        report = pdo.analyze(out)
        self.assertEqual((report["unknown"], report["malformed"]), (["stale"], []))
        for problem, reply in cases.items():
            board, code, meta, _ = self.run_one(step, reply, release_line)
            self.assertEqual((code, meta["reason"], meta["steps"][0].get("problem")), (1, "terminal_failed", problem),
                             problem)
            self.assertEqual(board.writes, [b"p", pdo.encode(step["send"])], problem)
        return report

    def test_discovery_terminal(self):
        cases = {"discovery_outcome_3": disc_reply(targets={2: {"outcome": 3, "confidence": 0}}),
                 "discovery_unresolved": disc_reply(targets={1: {"unresolved": 1}}),
                 "discovery_interrupted": disc_reply(header={"cancelled": 1}),
                 "discovery_wrong_request": disc_reply(header={"mode": 2}),
                 "discovery_results_incomplete": disc_reply(header={"terminals": 1}),
                 "discovery_service_failed": disc_reply(results={1: full_result(2, "1000:00", 0, "00000000",
                                                                                outcome=3)}),
                 "discovery_selection_refused": disc_reply(selection={"selected": 0}),
                 "discovery_wrong_selection": disc_reply(select=2),
                 "discovery_record_unparsed": disc_reply().replace(b"DISC_SELECT", b"DISC_CHOSEN")}
        report = self.check_cases(DISC_STEP, disc_reply(), cases, "DISC record released")
        (item,) = report["commissioning"]
        self.assertEqual((item["kind"], item["header"]["mode"]), ("DISC", 1))
        self.assertEqual([entry["kind"] for entry in item["lines"]],
                         ["DISC_T", "DISC_T", "DISC_PROBE", "DISC_PROBE", "DISC_SELECT"])

    def test_discovery_modes(self):
        # Passive: nothing sent, and a silent node (heartbeat disabled) is a legitimate NO_RESPONSE.
        step = dict(DISC_STEP, send="Do,3,0,0")
        silent = disc_reply("o", 3, 0, targets={2: {"outcome": 3, "confidence": 0, "hb": 0, "hbp": 0}})
        board, code, meta, _ = self.run_one(step, silent, "DISC record released")
        self.assertEqual((code, meta["reason"]), (0, "completed"), meta["steps"])
        sent = disc_reply("o", 3, 0, header={"admitted": 1})
        board, code, meta, _ = self.run_one(step, sent, "DISC record released")
        self.assertEqual(meta["steps"][0].get("problem"), "discovery_passive_sent")
        # Identity with the fixture expectation: both must match it; a contradiction stops the plan.
        step = dict(DISC_STEP, send="Di,4,0,1")
        board, code, meta, _ = self.run_one(step, disc_reply("i", 4, 0, 1), "DISC record released")
        self.assertEqual((code, meta["reason"]), (0, "completed"), meta["steps"])
        contradiction = disc_reply("i", 4, 0, 1, targets={2: {"outcome": 6, "confidence": 2}})
        board, code, meta, _ = self.run_one(step, contradiction, "DISC record released")
        self.assertEqual(meta["steps"][0].get("problem"), "discovery_outcome_6")
        generic = disc_reply("i", 4, 0, 1, targets={1: {"confidence": 2}})
        board, code, meta, _ = self.run_one(step, generic, "DISC record released")
        self.assertEqual(meta["steps"][0].get("problem"), "discovery_outcome_2")

    def test_comm_terminal(self):
        cases = {"comm_failure_3": comm_reply(header={"failure": 3, "outcome": 3, "published": 11, "group": 11}),
                 "comm_wrong_node": comm_reply(header={"node": 2}),
                 "comm_groups_incomplete": comm_reply(drop_group=17),
                 "comm_groups_out_of_order": comm_reply(swap=True),
                 "comm_results_incomplete": comm_reply(outcome_at=40),
                 "comm_write_present": comm_reply(write_at=12),
                 "comm_record_unparsed": comm_reply().replace(b"COMM id=", b"COMMX id=")}
        report = self.check_cases(COMM_STEP, comm_reply(), cases, "COMM record released")
        (item,) = report["commissioning"]
        self.assertEqual(len(item["lines"]), 18)
        self.assertEqual(item["lines"][0]["kind"], "COMM_NETWORK")

    def test_heartbeat_terminal(self):
        cases = {"heartbeat_failure_9": hb_reply(header={"failure": 9, "state": 0, "ack": 0, "matched": 0,
                                                         "published": 0, "terminals": 1}),
                 "heartbeat_failure_3": hb_reply(header={"failure": 3, "state": 5, "uncertain": 1}),
                 "heartbeat_wrong_node": hb_reply(header={"node": 2}),
                 "heartbeat_not_confirmed": hb_reply(header={"migrated": 0}),
                 "heartbeat_results_incomplete": hb_reply(header={"terminals": 2}),
                 "heartbeat_steps_differ": hb_reply(directions=(0, 0, 0)),
                 "heartbeat_values_differ": hb_reply(readback="f4010000"),
                 "heartbeat_write_bytes_differ": hb_reply(tx="2b171000f4010000"),
                 "heartbeat_record_unparsed": hb_reply().replace(b"HB id=", b"HBX id=")}
        report = self.check_cases(HB_STEP, hb_reply(), cases, "HB record released")
        (item,) = report["commissioning"]
        self.assertEqual((item["kind"], item["header"]["raw"], item["lines"][0]["migrated"]), ("HB", 1000, 1))
        # A second write frame to the node, even of the same bytes, is no confirmed single write.
        board, code, meta, _ = self.run_one(HB_STEP, hb_reply(extra_tx="2b171000e8030000"), "HB record released")
        self.assertEqual(meta["steps"][0].get("problem"), "heartbeat_write_bytes_differ")

    def test_refusals_end_the_plan(self):
        for step, line in ((DISC_STEP, b"DISC refused reason=scan; no work\n"),
                           (COMM_STEP, b"COMM refused reason=unresolved; no work\n"),
                           (HB_STEP, b"HB refused reason=service; no work\n")):
            board = Board({pdo.encode(step["send"]): line}, pdo="4", capture=0)
            code, meta, _ = self.session(board, [step, {"send": "r", "expect": pdo.RELEASE_LINES[pdo.HOLDS[step["send"][0]]]}])
            self.assertEqual((code, meta["reason"]), (1, "refused"))
            self.assertEqual(board.writes, [b"p", pdo.encode(step["send"])])


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
        self.assertIn("pdo=5", widest)
        self.assertIn("routes=12", widest)
        self.assertIn("capture=9", widest)
        self.assertLess(len(widest) + 1, output)
        self.assertLess(len(widest) + 1, retained)  # line() keeps only a response shorter than the buffer.

    # Widest text per conversion: unsigned and hex at their full width, signed with its sign.
    SPEC_WIDTH = {"u": 10, "lu": 10, "llu": 20, "d": 11, "ld": 11, "lx": 8, "08lx": 8, "02x": 2, "x": 8,
                  "04lx": 4, "04x": 4, "c": 1}

    def filled(self, body, head):
        """(widest, parseable) texts of one format string: every conversion at its widest (a flag, which
        the firmware prints as unsigned(bool), at one digit), and every field at a value its grammar row
        accepts (flags 1, hex f, signed negative, dir t)."""
        from s2_evidence import GRAMMAR, _compile
        types = {}
        for row in GRAMMAR:
            if head.startswith(row.head) or row.head.startswith(head):
                for name, (_, convert, _) in _compile(row.fields).items():
                    types.setdefault(name, convert.__name__)
        conversion = re.compile(r"(?:(\w+)=)?%([0-9]*l{0,2}[udxsc])")

        def fill(match, widest):
            # A conversion right after "name=" is that field; one inside a compound value (the
            # subindex of object=, the later bytes of data=) continues the field before it.
            name, spec = match.group(1), match.group(2)
            kind = types.get(name, "") if name else ""
            width = 64 if spec == "s" else self.SPEC_WIDTH[spec]
            if widest:
                value = "9" * (1 if kind == "_flag" else width)
            else:
                value = ("1" if kind == "_flag" else "t" if spec in "cs" else
                         "-" + "9" * (width - 1) if spec in ("d", "ld") else
                         "f" * width if "x" in spec else "9" * width)
            return (name + "=" if name else "") + value
        widest = conversion.sub(lambda m: fill(m, True), body)
        parseable = conversion.sub(lambda m: fill(m, False), body)
        return widest, parseable

    def test_commissioning_lines_are_in_the_grammar_and_fit(self):
        """Every D, K and H record line, filled to its widest, fits its 320-byte buffer, and filled with
        valid values parses through the grammar with no unknown field (the firmware's own strings)."""
        from s2_evidence import parse_line
        header = (REPOSITORY / "examples/s2_bench/CommunicationBench.h").read_text(encoding="utf-8")
        discovery_header = (REPOSITORY / "examples/s2_bench/DiscoveryBench.h").read_text(encoding="utf-8")
        capacity = int(re.search(r"COMMUNICATION_LINE = (\d+)", header).group(1))
        self.assertEqual(capacity, int(re.search(r"DISCOVERY_BENCH_LINE = (\d+)", discovery_header).group(1)))
        heads = r"(?:DISC|COMM|HB|RESULT|END discovery|END comm|END heartbeat)"
        expected = {"DISC", "DISC_T", "DISC_PROBE", "DISC_ID", "DISC_SELECT", "COMM", "COMM_NETWORK", "COMM_NODE",
                    "COMM_PDO", "COMM_MAP", "HB", "HB_PREP", "RESULT", "END"}
        found = set()
        for path in ("DiscoveryBench.cpp", "CommunicationBench.cpp", "BenchTerminal.cpp"):
            source = (REPOSITORY / "examples/s2_bench" / path).read_text(encoding="utf-8")
            for body in {self.literal(m) for m in re.findall(r'"(' + heads + r'[ _](?:[^"]|"\s*")*?)\\n"', source)}:
                head = body.split("=")[0].rsplit(" ", 1)[0] + " "
                widest, parseable = self.filled(body, head)
                self.assertNotIn("%", widest, body)
                self.assertLess(len(widest) + 1, capacity, body)  # Newline; vsnprintf's terminator fits too.
                record = parse_line(1, parseable)
                self.assertEqual((record.status, record.extra), ("parsed", {}), (body, record.reason))
                found.add(record.kind)
        self.assertEqual(found, expected)

    def test_bench_terminal_lines_match_main(self):
        """The D, K and H records print RESULT and TRAFFIC lines exactly as main.cpp does."""
        main = (REPOSITORY / "examples/s2_bench/main/main.cpp").read_text(encoding="utf-8")
        bench = (REPOSITORY / "examples/s2_bench/BenchTerminal.cpp").read_text(encoding="utf-8")
        pattern = r'"RESULT (.*?)\\n"\s*,'
        self.assertEqual(self.literal(re.search(pattern, bench, re.S).group(1)),
                         self.literal(re.search(pattern, main, re.S).group(1)))
        frame = r'"%s k=%u (.*?)epoch=%lu'
        self.assertEqual(self.literal(re.search(frame, bench, re.S).group(1)),
                         self.literal(re.search(frame, main, re.S).group(1)))
        for suffix in (" ctrl=%u", " rec=%u", " txk=%u txr=%lu txf=%lu"):
            self.assertIn(suffix, main)
            self.assertIn(suffix, bench)

    def test_commissioning_console_lines_parse(self):
        """main.cpp's D, K and H admission, refusal and release lines are grammar rows of their kinds."""
        from s2_evidence import parse_line
        main = (REPOSITORY / "examples/s2_bench/main/main.cpp").read_text(encoding="utf-8")
        for literal, kind in (("DISC admitted mode=%u seconds=%lu select=%u expect=%u", "ADMITTED"),
                              ("DISC refused reason=%s", "REFUSED"),
                              ("COMM admitted node=%u groups=18 uploads=95", "ADMITTED"),
                              ("HB admitted node=%u raw=%u", "ADMITTED"),
                              ("DISC record released", "RELEASED"), ("DISC release refused", "RELEASE_REFUSED"),
                              ("COMM record released", "RELEASED"), ("COMM release refused", "RELEASE_REFUSED"),
                              ("HB record released", "RELEASED"), ("HB release refused", "RELEASE_REFUSED"),
                              ("NETWORK refused reason=discovery", "REFUSED")):
            self.assertIn(literal, self.literal(main), literal)
            _, parseable = self.filled(literal, literal)
            record = parse_line(1, parseable)
            self.assertEqual((record.status, record.kind), ("parsed", kind), literal)
        # The runner knows each of them: refusals end a plan, releases match their record, admissions
        # are the checked starts of their commands.
        for line in ("DISC refused", "DISC release refused", "COMM refused", "COMM release refused", "HB refused",
                     "HB release refused", "NETWORK refused"):
            self.assertIn(line, pdo.REFUSALS)
        self.assertLessEqual({"DISC record released", "COMM record released", "HB record released"},
                             set(pdo.RELEASE_LINES.values()))
        self.assertEqual([pdo.CHECKED[c][0] for c in "DKH"], ["DISC admitted", "COMM admitted", "HB admitted"])
        # The K and H refusals share one format string; both kinds parse.
        self.assertIn(r'"%s refused reason=%s; no work\n"', main)
        self.assertIn('reads ? "COMM" : "HB"', main)
        for text in ("COMM refused reason=unresolved; no work", "HB refused reason=service; no work"):
            self.assertEqual(parse_line(1, text).kind, "REFUSED")

    def test_heap_line_fits_and_parses(self):
        """The M line (main.cpp heapLine): totals, at most four regions, the reservation and the arena,
        at their widest within the 320-byte response, and parsed by the grammar's HEAP row."""
        from s2_evidence import parse_line
        main = (REPOSITORY / "examples/s2_bench/main/main.cpp").read_text(encoding="utf-8")
        body = re.search(r'void heapLine\(.*?\n}\n', main, re.S).group(0)
        parts = [self.literal(m) for m in re.findall(r'"((?:HEAP | r%u| reserved)(?:[^"]|"\s*")*?)"', body)]
        self.assertEqual(len(parts), 3, parts)
        head, region, tail = parts
        self.assertIn("CAPACITY = 4", main[main.index("struct HeapRegions"):])
        regions = "".join(region.replace("%u", str(i), 1) for i in range(4))
        line = head + regions + tail.replace(r"\n", "")
        widest = re.sub(r"%0?2?l?[ux]", lambda m: "f" * 8 if "x" in m.group(0) else "9" * 10, line)
        self.assertNotIn("%", widest)
        self.assertLess(len(widest) + 2, 320)  # Newline and vsnprintf's terminator.
        record = parse_line(1, widest.replace("reserved=ffffffff", "reserved=1f"))
        self.assertEqual((record.status, record.kind, record.extra), ("parsed", "HEAP", {}), record.reason)
        self.assertIn("M", pdo.SINGLE_BYTES)

    def test_commissioning_records_gate_every_other_command(self):
        """While a D, K or H record is held, main.cpp admits nothing else: every single-byte admission,
        the trial, soak and capture lines and the N, R, W, E lines (behind one guard) check it. The
        download dispatcher knows only the H record: D and K never download."""
        main = (REPOSITORY / "examples/s2_bench/main/main.cpp").read_text(encoding="utf-8")
        lines_start = main.index("} else if (pdoCollecting) {")
        soak_start = main.index("} else if (soakCollecting) {")
        trial = main[main.index("if (trialCollecting) {"):lines_start]
        collected = main[lines_start:soak_start]
        single = main[soak_start:]
        self.assertIn("!commissionHeld", trial)
        capture = collected[collected.index("parsed.kind == E::PdoCommandKind::CAPTURE) {"):
                            collected.index("} else if (captureBlock) {")]
        self.assertIn("!commissionHeld", capture)
        guard = collected.index("} else if (soakActive || trialHeld || commissionHeld) {")
        for kind in ("CONFIGURE", "NMT", "DISCOVER", "COMMUNICATION"):
            self.assertLess(guard, collected.index("parsed.kind == E::PdoCommandKind::%s" % kind), kind)
        self.assertIn("!commissionHeld", single[:single.index("} else if (command == 'X') {")])  # The soak.
        accepted = re.findall(r"const bool accepted =(.*?);", single, re.S)
        self.assertEqual(len(accepted), 5)  # v/b, m, h/j, q/w and 1..4.
        for expression in accepted:
            self.assertIn("!commissionHeld", expression)
        dispatch = main[main.index("preflight.check = ["):main.index("firmware.setPreflight(preflight);")]
        for producer in ("RawEvidence", "PdoExperiment", "HeartbeatCommission"):
            self.assertIn("get<E::%s>" % producer, dispatch)
        self.assertNotIn("CommunicationReads", dispatch)
        self.assertNotIn("DiscoveryBench", dispatch)


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


RESET_REPLY = (b"NMT admitted node=1 cmd=129; addressed, no drive-stop claim; network observation separate\n"
               b"NMT request=4 target=1 cmd=129 outcome=0 possible=1 txConfirmed=1 completed=900 admitted=800; x\n")


class NmtResetTest(unittest.TestCase):
    """PONG pdo=5: the N line's r (Reset Node, 0x81) and c (Reset Communication, 0x82), attended fault-recovery
    experiments. Checked as every N line is, refused on an older image, and only with a watch running before
    them, so that a boot-up that follows is in the record."""

    WATCH_STEP = {"send": "P30", "expect": "PDOW admitted"}

    def reset(self, text):
        return {"send": text, "expect": "NMT admitted", "end": "NMT request"}

    def test_plan_rules(self):
        for text in ("N1,r", "N2,r", "N1,c", "N2,c"):
            self.assertEqual(len(pdo.check_plan(plan(self.WATCH_STEP, self.reset(text), {"wait_s": 2}))), 3)
            self.assertEqual(pdo.nmt_plan(text)[1], 0x81 if text.endswith("r") else 0x82)
        self.assertEqual(pdo.NMT_NAMES[0x81], "reset-node")
        self.assertEqual(pdo.NMT_NAMES[0x82], "reset-communication")
        for steps in ((self.reset("N1,r"),),  # No watch to record a boot-up.
                      (self.WATCH_STEP, {"send": "P0", "expect": "PDOW_STOPPING"}, self.reset("N1,c")),
                      (self.WATCH_STEP, self.reset("N3,r")), (self.WATCH_STEP, self.reset("N1,x")),
                      (self.WATCH_STEP, self.reset("N1,R")), (self.WATCH_STEP, self.reset("N1,rr")),
                      (self.WATCH_STEP, self.reset("N1,r,1")),
                      (self.WATCH_STEP, {"send": "N1,r", "expect": "NMT", "end": "NMT request"}),
                      (self.WATCH_STEP, {"send": "N1,r", "expect": "NMT admitted", "end": "END raw"}),
                      # Its result is held like any N's: released before the next.
                      (self.WATCH_STEP, self.reset("N1,r"), self.reset("N1,o"))):
            with self.assertRaises(pdo.SessionError, msg=steps):
                pdo.check_plan(plan(*steps))
        # The other commands need no watch, as before.
        self.assertEqual(len(pdo.check_plan(plan(self.reset("N1,o")))), 1)

    def session(self, board, steps):
        root = Path(tempfile.mkdtemp())
        path = root / "plan.json"
        path.write_text(json.dumps(plan(*steps)), encoding="utf-8")
        out = root / "session"
        code = pdo.run(Args(path, out), Backend(board), Clock())
        return code, json.loads((out / "session.json").read_text(encoding="utf-8")), out

    def test_an_image_before_pdo5_is_never_sent_a_reset(self):
        for version in ("1", "4"):
            board = Board({b"P30\n": WATCH, b"N1,r\n": RESET_REPLY}, pdo=version)
            code, meta, _ = self.session(board, [self.WATCH_STEP, self.reset("N1,r")])
            self.assertEqual((code, meta["reason"]), (1, "image_without_nmt_reset"))
            self.assertEqual(board.writes, [b"p"])
        board = Board({b"P30\n": WATCH, b"N1,r\n": RESET_REPLY}, pdo="5")
        code, meta, _ = self.session(board, [self.WATCH_STEP, self.reset("N1,r"), {"wait_s": 0.05}])
        self.assertEqual((code, meta["reason"]), (0, "completed"))
        self.assertEqual(board.writes, [b"p", b"P30\n", b"N1,r\n"])

    def test_the_terminal_names_the_reset(self):
        # The firmware's result must carry the node and command sent, outcome 0 and a confirmed TX.
        for reply, problem in ((RESET_REPLY.replace(b"cmd=129 outcome", b"cmd=130 outcome"), "nmt_wrong_request"),
                               (RESET_REPLY.replace(b"txConfirmed=1", b"txConfirmed=0"), "nmt_not_transmitted"),
                               (RESET_REPLY.replace(b"target=1 cmd", b"target=2 cmd"), "nmt_wrong_request")):
            board = Board({b"P30\n": WATCH, b"N1,r\n": reply}, pdo="5")
            code, meta, _ = self.session(board, [self.WATCH_STEP, self.reset("N1,r"), {"wait_s": 0.05}])
            self.assertEqual((code, meta["reason"], meta["steps"][1]["problem"]), (1, "terminal_failed", problem))

    def test_analysis_reports_the_boot_up(self):
        frame = "PDOW_F n=%d t=%d id=%s ext=0 rtr=0 dlc=%d data=%s timed=1 exact=0 lo=%d hi=%d gap=0"
        lines = ["PDOW_BEGIN n=0 t=1 seconds=30 ring=32 ids=16",
                 frame % (1, 500, "701", 1, "05", 490, 500),  # Its heartbeat before the reset.
                 "NMT request=4 target=1 cmd=129 outcome=0 possible=1 txConfirmed=1 completed=900 admitted=800; x",
                 frame % (2, 1500, "702", 1, "00", 1490, 1500),  # Node 2's boot-up is not node 1's.
                 frame % (3, 2500, "701", 1, "00", 2490, 2500),  # The boot-up after the reset.
                 frame % (4, 3500, "701", 1, "7f", 3490, 3500),  # Then Pre-operational heartbeats.
                 "PDOW_END n=5 t=40000 reason=duration frames=4 printed=4 unprinted=0 tapDropped=0 untimed=0 sdo=0 "
                 "idOverflow=0"]
        report = AnalyzeTest.capture(self, lines)
        self.assertEqual((report["unknown"], report["malformed"]), ([], []))
        self.assertEqual([n["name"] for n in report["nmt"]], ["reset-node"])
        self.assertEqual(report["nmt_resets"], [
            {"line": 3, "target": 1, "name": "reset-node", "txConfirmed": 1, "completed": 900, "covered": True,
             "unprinted": 0, "boot_ups": 1, "first_boot_up": 2500, "first_boot_up_delay_us": 1600}])
        self.assertEqual(report["watches"][0]["ids"]["701"]["heartbeat_states"],
                         [(500, "05"), (2500, "00"), (3500, "7f")])
        # No boot-up printed: reported as none, with whether a watch covered the reset at all.
        quiet = AnalyzeTest.capture(self, [line for line in lines if "data=00" not in line])
        self.assertEqual((quiet["nmt_resets"][0]["boot_ups"], quiet["nmt_resets"][0]["first_boot_up"],
                          quiet["nmt_resets"][0]["covered"]), (0, None, True))
        bare = AnalyzeTest.capture(self, [lines[2]])
        self.assertEqual((bare["nmt_resets"][0]["covered"], bare["nmt_resets"][0]["unprinted"]), (False, None))


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
