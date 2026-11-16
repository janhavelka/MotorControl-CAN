"""Paired two-axis HIL sessions (2026-11-16, route protocol 16): HM1, HM2, HM3 and HM5 again, armed with Ta/Te.

Usage: python pairhil.py WORKTREE SESSION      SESSION: hm12, hm3, hm5

Built on conhil2.py (sched.py one-port sessions, hil.py's ladder for setup reads, mode routes, enables and stop
records). The only change from the 17386514 sessions: one paired arming line before either move, no `r` between the
armings and the moves, and the record's `d` and `r` after the console's results are released. Every console line and
record of a trial stays in its sched directory. Nothing is retried; a refused step runs `stop all fast` once.
"""
import importlib.util
import sys

SCRATCH = r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad"
spec = importlib.util.spec_from_file_location("conhil2", SCRATCH + r"\conhil2.py")
c2 = importlib.util.module_from_spec(spec)
argv = sys.argv
sys.argv = ["x"]
spec.loader.exec_module(c2)
sys.argv = argv
c = c2.c
night = c2.night


class Pair(c2.Trials):
    def ta(self, p1, p2):
        """Ta: TY's absolute move evidence for node 1 at p1, then node 2's reviewed 6064 p2."""
        t = self.lad.next()
        return "Ta1,{},1,{},500,{},{},{},0,0,1500,1500,500,{}".format(t, p1, t, c.EVIDENCE, c.IDENT, p2)

    def te(self, rpm, ramp, kind):
        """Te: TB's velocity evidence for both nodes, one stop kind (1 HALT, 2 QUICK_STOP)."""
        t = self.lad.next()
        return "Te1,{},1,{},{},{},500,{},2,{},{},1500,1500,500".format(t, rpm, ramp, ramp, t, kind, c.IDENT)

    def moves(self, p1, p2, d1, d2, rpm, base, first=True):
        """One paired arming, then each axis's host intent and absolute move, nothing between them."""
        steps = [{"send": "o"}, {"send": "M"}] if first else []
        return steps + [
            {"send": self.ta(p1, p2)},
            {"send": "@{} host 0".format(base)}, {"send": "@{} accel 500 ms".format(base + 1)},
            {"send": "@{} decel 500 ms".format(base + 2)},
            {"send": "@{} move absolute {} steps native {} configured setup write".format(base + 3, p1 + d1, rpm),
             "save": "m1"},
            {"send": "@{} host 1".format(base + 4)}, {"send": "@{} accel 500 ms".format(base + 5)},
            {"send": "@{} decel 500 ms".format(base + 6)},
            {"send": "@{} move absolute {} steps native {} configured setup write".format(base + 7, p2 + d2, rpm),
             "save": "m2"},
            {"mark": "t0"}]

    def export(self, base, names):
        """Results, releases, then the paired record's dump and release."""
        steps = []
        for i, name in enumerate(names):
            steps.append({"send": "@{} result {{{}}}".format(base + i, name), "optional": True})
        for i, name in enumerate(names):
            steps.append({"send": "@{} release {{{}}}".format(base + 10 + i, name), "optional": True})
        return steps + [{"pump": 0.5}, {"send": "d"}, {"send": "r"}, {"send": "s"}, {"send": "M"}]

    def hm12(self):
        p1, p2 = self.two_enabled("hm12p")
        steps = self.moves(p1, p2, -3000, 3000, 30, 111) + [
            {"terminal": "m1", "timeout": 20}, {"terminal": "m2", "timeout": 20}] + self.export(121, ["m1", "m2"])
        self.sched("hm1p", steps, [{"send": "@190 stop all fast"}],
                   "HM1 (paired arming Ta): absolute moves, opposite directions, 3000 counts at 30 rpm, both to "
                   "completion")
        q1, q2 = self.lad.read(1)["6064"], self.lad.read(2)["6064"]
        self.lad.say("  HM1 end positions n1 {} (target {}), n2 {} (target {})".format(q1, p1 - 3000, q2, p2 + 3000))
        steps = self.moves(q1, q2, -3500, 3500, 30, 141, first=False) + [
            {"at": 3.0, "from": "t0"}, {"send": "@151 stop 1 normal", "save": "s1"},
            {"terminal": "s1", "timeout": 10, "optional": True}, {"terminal": "m2", "timeout": 20, "optional": True},
            {"terminal": "m1", "timeout": 5, "optional": True}, {"pump": 1.0}] + self.export(152, ["s1", "m1", "m2"])
        self.sched("hm2p", steps, [{"send": "@191 stop all fast"}],
                   "HM2 (paired arming Ta): moves 3500 counts at 30 rpm; HALT node 1 at 3 s while node 2 completes")
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM2 end n{} {}".format(n, st))
            if st and self.lad.enabled(st):
                self.lad.quick(n, "stop-quick-halted" if n == 1 else "stop-quick-stationary")

    def hm3(self):
        p1, p2 = self.two_enabled("hm3p")
        steps = self.moves(p1, p2, -4000, 4000, 30, 111) + [
            {"at": 3.0, "from": "t0"}, {"send": "@121 stop all normal", "save": "g1"}, {"pump": 2.0},
            {"send": "@122 result {g1}", "optional": True}, {"send": "@123 release {g1}", "optional": True},
            {"send": "@124 stop all fast", "save": "g2"}, {"pump": 2.0},
            {"terminal": "m1", "timeout": 5, "optional": True}, {"terminal": "m2", "timeout": 5, "optional": True}] + \
            self.export(131, ["g2", "m1", "m2"])
        self.sched("hm3p", steps, [{"send": "@190 stop all fast"}],
                   "HM3 (paired arming Ta): moves 4000 counts at 30 rpm; group HALT at 3 s, then group quick stop")
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM3 end n{} {}".format(n, st))
            if st and self.lad.enabled(st):
                self.lad.quick(n, "stop-quick-halted")

    def hm5(self):
        for n in (1, 2):
            self.lad.begin_series(n, "hm5p")
        self.lad.begin(1, "hm5q")
        halt = True
        for n in (1, 2):
            st = self.lad.read(n)
            if not st or not self.lad.released(st) or st["6061"] != "03":
                raise c.Stop("not released in mode 3: {}".format(st))
            word = int(st["2100"], 16)
            halt = halt and bool(word & 0x0001) and not (word & 0x0004)
        kind = "normal" if halt else "fast"
        steps = [{"send": "o"}, {"send": "M"}, {"send": self.te(30, 500, 1 if halt else 2)},
                 {"send": "@161 host 0"},
                 {"send": "@162 velocity 30 rpm motor 12000 configured {}".format(kind), "save": "v1"},
                 {"send": "@163 host 1"},
                 {"send": "@164 velocity 30 rpm motor 12000 configured {}".format(kind), "save": "v2"},
                 {"terminal": "v1", "timeout": 25}, {"terminal": "v2", "timeout": 25}] + self.export(171, ["v1", "v2"])
        self.sched("hm5p", steps, [{"send": "@190 stop all fast"}],
                   "HM5 (paired arming Te): velocity 30 rpm, 12 s each, each axis's own {} stop at its due time"
                   .format("HALT" if halt else "QUICK_STOP"))
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM5 end n{} {}".format(n, st))
            if st and self.lad.enabled(st):
                self.lad.quick(n, "stop-quick-halted")


if __name__ == "__main__":
    wt, session = argv[1], argv[2]
    t = Pair(wt)
    try:
        {"hm12": t.hm12, "hm3": t.hm3, "hm5": t.hm5}[session]()
        t.lad.say("paired session {} ended".format(session))
    except (c.Stop, night.Retire) as error:
        t.lad.say("!!! paired session {} stopped: {}".format(session, error))
        for n in (1, 2):
            try:
                st = t.lad.read(n)
                if st and (t.lad.enabled(st) or st["606c"] != 0):
                    t.lad.quick(n, "stop-quick-stationary")
            except Exception as e2:  # noqa: BLE001
                t.lad.say("!!! check on node {}: {}".format(n, e2))
        sys.exit(1)
    finally:
        t.lad.save()
