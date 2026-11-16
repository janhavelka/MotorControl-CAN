"""Console-path HIL trials as single-port sessions (sched.py), 2026-11-16, image 17386514. One MCU boot per session.

Usage: python conhil2.py WORKTREE SESSION [NODE]
Sessions: vchg-after N, vchg-down N, vchg-during N, mspeed N, mramps N, mretarget N, hm12, hm3, hm5.
Setup (fresh boots, releases, mode routes, enables with stop records) comes from conhil.py/hil.py; each trial is
one sched.py session, so every console record of the trial is captured. Console request IDs start at 101.
"""
import importlib.util
import json
import subprocess
import sys

SCRATCH = r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad"
spec = importlib.util.spec_from_file_location("conhil", SCRATCH + r"\conhil.py")
c = importlib.util.module_from_spec(spec)
argv = sys.argv
sys.argv = ["x"]
spec.loader.exec_module(c)
sys.argv = argv
night = c.night


class Trials(c.Sessions):
    def sched(self, label, steps, on_failure, purpose):
        out = self.lad.sdir / "{}-sched-{}".format(night.stamp(), label)
        planfile = self.lad.sdir / "{}-plan-{}.json".format(night.stamp(), label)
        planfile.write_text(json.dumps(dict(purpose=purpose, steps=steps, on_failure=on_failure), indent=2) + "\n",
                            encoding="utf-8")
        self.lad.say("=== sched {}: {}".format(label, purpose))
        r = subprocess.run([sys.executable, SCRATCH + r"\sched.py", self.wt, str(out), str(planfile)], cwd=self.wt,
                           text=True, capture_output=True, errors="replace")
        for line in r.stdout.splitlines():
            if any(k in line for k in ('"type":"terminal"', '"type":"reply"', '"type":"velocity_change"',
                                       '"type":"move_relation"', '"type":"group_stop', '"type":"stop_attempt"',
                                       "CONSOLE armed", "TRIAL_REFUSED", "refused", "!!!", "STATS")):
                self.lad.say("  " + line[:420])
        summary = json.loads((out / "summary.json").read_text(encoding="utf-8")) if (out / "summary.json").exists() \
            else {}
        self.lad.say("  sched exit {} complete={} saved={} refusals={}".format(
            r.returncode, summary.get("complete"), summary.get("saved"), len(summary.get("refusals", []))))
        recs = [json.loads(x) for x in (out / "records.jsonl").read_text(encoding="utf-8").splitlines()] \
            if (out / "records.jsonl").exists() else []
        self.lad.note(kind="sched", label=label, rc=r.returncode, summary=summary, dir=str(out),
                      records=[x for x in recs if x.get("type") in ("terminal", "velocity_change", "move_relation",
                                                                   "group_stop_axis", "stop_attempt")][-40:])
        return r.returncode, summary, recs

    def ty2(self, node, pos):
        return self.ty(node, pos)

    def vchg2(self, node, variant):
        st = self.lad.begin_series(node, "s" + variant)
        s = node - 1
        start, new, ramp, change_at, stop_at = {"after": (12, 30, 500, 5.5, None), "down": (60, 12, 1000, 5.5, None),
                                                "during": (6, 60, 2000, 6.0, 6.6),
                                                "big": (6, 60, 500, 5.5, None)}[variant]
        steps = [{"send": "o"}, {"send": "M"}, {"send": "@101 host {}".format(s)},
                 {"send": self.tb(node, 60, ramp, 2), "allow": True},
                 {"send": "@102 velocity {} rpm motor 12000 configured fast".format(start), "save": "v"},
                 {"mark": "t0"}, {"send": "r"},
                 {"at": change_at, "from": "t0"},
                 {"send": "@103 velocity change {{v}} {} rpm motor configured".format(new), "optional": True}]
        if stop_at is not None:
            steps += [{"at": stop_at, "from": "t0"}, {"send": "@104 stop {} fast".format(node), "save": "s"},
                      {"terminal": "s", "timeout": 10}]
        steps += [{"terminal": "v", "timeout": 25}, {"pump": 1.0},
                  {"send": "@105 result {v}", "optional": True}]
        if stop_at is not None:
            steps += [{"send": "@106 release {s}", "optional": True}]
        steps += [{"send": "@107 release {v}", "optional": True}, {"send": "s"}, {"send": "M"}]
        rc, summary, recs = self.sched("vchg-" + variant, steps, [{"send": "@190 stop {} fast".format(node)}],
                                       "velocity {} rpm, change to {} at {} s, ramp {} ms, {}".format(
                                           start, new, change_at, ramp, "planned fast stop at {} s".format(stop_at)
                                           if stop_at else "the run's own fast stop at 12 s"))
        self.lad.say("  VCHG2 {} n{} records={}".format(variant, node, [
            {k: r.get(k) for k in ("type", "operation_id", "ok", "outcome", "uncertain", "native_rate", "rate",
                                   "execution_code", "completion_code", "report")} for r in recs
            if r.get("type") in ("terminal", "velocity_change")]))
        self.lad.say("  VCHG2 end state {}".format(self.lad.read(node)))

    def mchg2(self, node, kind):
        pos = self.lad.position_series(node, "s" + kind)
        s = node - 1
        trace = kind.endswith("-trace")
        kind = kind.replace("-trace", "")
        line = {"speed": "@106 move speed {m} 60 immediate", "ramps": "@106 move ramps {m} 200 800 immediate",
                "retarget": "@106 move retarget {{m}} immediate absolute {} steps native 30 configured".format(
                    pos + 4000)}[kind]
        steps = [{"send": "o"}, {"send": "M"}, {"send": "@101 host {}".format(s)}, {"send": "@102 accel 500 ms"},
                 {"send": "@103 decel 500 ms"}, {"send": self.ty(node, pos), "allow": True},
                 # The console move's deadline is its request + 10 s, and a replacement needs 21 services (about
                 # 7.35 s) left: offer it within about 2.6 s of the request, timed from before the request because
                 # the move reply waits behind the arming dump.
                 {"mark": "t0"},
                 {"send": "@105 move absolute {} steps native 30 configured setup write".format(pos + 3000), "save": "m"},
                 ] + ([{"send": "@104 debug raw"}] if trace else []) + [
                 {"at": 1.0, "from": "t0"},
                 dict({"send": line, "optional": True}, **({} if kind == "retarget" else {"save": "rp"})),
                 {"send": "r"}, {"terminal": "m", "timeout": 15, "optional": True}]
        if kind != "retarget":
            steps += [{"terminal": "rp", "timeout": 15, "optional": True}]
        steps += [{"pump": 1.0}] + ([{"send": "@111 debug off"}] if trace else []) + [
                  {"send": "@107 result {m}", "optional": True}]
        if kind != "retarget":
            steps += [{"send": "@108 result {rp}", "optional": True}, {"send": "@109 release {m}", "optional": True},
                      {"send": "@110 release {rp}", "optional": True}]
        else:
            steps += [{"send": "@109 release {m}", "optional": True}]
        steps += [{"send": "s"}, {"send": "M"}]
        rc, summary, recs = self.sched("m" + kind + ("-trace" if trace else ""), steps, [{"send": "@190 stop {} fast".format(node)}],
                                       "move 3000 counts at 30 rpm, {} change at 1.0 s after the request".format(kind))
        self.lad.say("  MCHG2 {} n{} records={}".format(kind, node, [
            {k: r.get(k) for k in ("type", "operation_id", "ok", "outcome", "uncertain", "execution_code",
                                   "completion_code", "original", "replacement")} for r in recs
            if r.get("type") in ("terminal", "move_relation")]))
        st = self.lad.read(node)
        self.lad.say("  MCHG2 end state {}".format(st))
        if st and self.lad.enabled(st) and st["606c"] == 0:
            self.lad.quick(node, "stop-quick-stationary")

    def paired(self, p1, p2, dist, rpm, first=True):
        return ([{"send": "o"}, {"send": "M"}] if first else []) + [
            {"send": "@{} host 0".format(111 if first else 131)}, {"send": "@{} accel 500 ms".format(112 if first else 132)},
            {"send": "@{} decel 500 ms".format(113 if first else 133)}, {"send": self.ty(1, p1)},
            {"send": "@{} move absolute {} steps native {} configured setup write".format(114 if first else 134,
                                                                                         p1 - dist, rpm), "save": "m1"},
            {"send": "r"}, {"send": "@{} host 1".format(115 if first else 135)},
            {"send": "@{} accel 500 ms".format(116 if first else 136)},
            {"send": "@{} decel 500 ms".format(117 if first else 137)}, {"send": self.ty(2, p2)},
            {"send": "@{} move absolute {} steps native {} configured setup write".format(118 if first else 138,
                                                                                         p2 + dist, rpm), "save": "m2"},
            {"mark": "t0"}, {"send": "r"}]

    def hm12b(self):
        p1, p2 = self.two_enabled("hm12")
        steps = self.paired(p1, p2, 3000, 30) + [
            {"terminal": "m1", "timeout": 20}, {"terminal": "m2", "timeout": 20},
            {"send": "@121 result {m1}", "optional": True}, {"send": "@122 result {m2}", "optional": True},
            {"send": "@123 release {m1}", "optional": True}, {"send": "@124 release {m2}", "optional": True},
            {"send": "s"}, {"send": "M"}]
        self.sched("hm1", steps, [{"send": "@190 stop all fast"}],
                   "HM1: paired absolute moves, opposite directions, 3000 counts at 30 rpm, both to completion")
        q1, q2 = self.lad.read(1)["6064"], self.lad.read(2)["6064"]
        self.lad.say("  HM1 end positions n1 {} (target {}), n2 {} (target {})".format(q1, p1 - 3000, q2, p2 + 3000))
        steps = self.paired(q1, q2, 3500, 30, first=False) + [
            {"at": 3.0, "from": "t0"}, {"send": "@141 stop 1 normal", "save": "s1"},
            {"terminal": "s1", "timeout": 10, "optional": True}, {"terminal": "m2", "timeout": 20, "optional": True},
            {"terminal": "m1", "timeout": 5, "optional": True}, {"pump": 1.0},
            {"send": "@142 result {m1}", "optional": True}, {"send": "@143 result {m2}", "optional": True},
            {"send": "@144 release {s1}", "optional": True}, {"send": "@145 release {m1}", "optional": True},
            {"send": "@146 release {m2}", "optional": True}, {"send": "s"}, {"send": "M"}]
        self.sched("hm2", steps, [{"send": "@191 stop all fast"}],
                   "HM2: paired moves 3500 counts at 30 rpm; HALT node 1 at 3 s while node 2 completes")
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM2 end n{} {}".format(n, st))
            if st and self.lad.enabled(st):
                self.lad.quick(n, "stop-quick-halted" if n == 1 else "stop-quick-stationary")

    def hm3b(self):
        p1, p2 = self.two_enabled("hm3")
        steps = self.paired(p1, p2, 4000, 30) + [
            {"at": 3.0, "from": "t0"}, {"send": "@151 stop all normal", "save": "g1"}, {"pump": 2.0},
            {"send": "@152 result {g1}", "optional": True}, {"send": "@153 release {g1}", "optional": True},
            {"send": "@154 stop all fast", "save": "g2"}, {"pump": 2.0},
            {"terminal": "m1", "timeout": 5, "optional": True}, {"terminal": "m2", "timeout": 5, "optional": True},
            {"send": "@155 result {g2}", "optional": True}, {"send": "@156 result {m1}", "optional": True},
            {"send": "@157 result {m2}", "optional": True}, {"send": "@158 release {g2}", "optional": True},
            {"send": "@159 release {m1}", "optional": True}, {"send": "@160 release {m2}", "optional": True},
            {"send": "s"}, {"send": "M"}]
        self.sched("hm3", steps, [{"send": "@190 stop all fast"}],
                   "HM3: paired moves 4000 counts at 30 rpm; group HALT at 3 s, then group quick stop")
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM3 end n{} {}".format(n, st))
            if st and self.lad.enabled(st):
                self.lad.quick(n, "stop-quick-halted")

    def hm5b(self):
        for n in (1, 2):
            self.lad.begin_series(n, "hm5p")
        self.lad.begin(1, "hm5")
        kinds = {}
        for n in (1, 2):
            st = self.lad.read(n)
            if not st or not self.lad.released(st) or st["6061"] != "03":
                raise c.Stop("not released in mode 3: {}".format(st))
            word = int(st["2100"], 16)
            kinds[n] = "normal" if (word & 0x0001) and not (word & 0x0004) else "fast"
        steps = [{"send": "o"}, {"send": "M"}, {"send": "@161 host 0"},
                 {"send": self.tb(1, 30, 500, 1 if kinds[1] == "normal" else 2)},
                 {"send": "@162 velocity 30 rpm motor 12000 configured {}".format(kinds[1]), "save": "v1"},
                 {"send": "r"}, {"send": "@163 host 1"},
                 {"send": self.tb(2, 30, 500, 1 if kinds[2] == "normal" else 2)},
                 {"send": "@164 velocity 30 rpm motor 12000 configured {}".format(kinds[2]), "save": "v2"},
                 {"send": "r"}, {"terminal": "v1", "timeout": 25}, {"terminal": "v2", "timeout": 25},
                 {"send": "@165 result {v1}", "optional": True}, {"send": "@166 result {v2}", "optional": True},
                 {"send": "@167 release {v1}", "optional": True}, {"send": "@168 release {v2}", "optional": True},
                 {"send": "s"}, {"send": "M"}]
        self.sched("hm5", steps, [{"send": "@190 stop all fast"}],
                   "HM5: paired velocity 30 rpm, 12 s each, own stop per axis {}".format(kinds))
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM5 end n{} {}".format(n, st))
            if st and self.lad.enabled(st):
                self.lad.quick(n, "stop-quick-halted")


if __name__ == "__main__":
    wt, session = argv[1], argv[2]
    node = int(argv[3]) if len(argv) > 3 else 0
    t = Trials(wt)
    try:
        if session.startswith("vchg-"):
            t.vchg2(node, session.split("-", 1)[1])
        elif session in ("mspeed", "mramps", "mretarget", "mspeed-trace"):
            t.mchg2(node, session[1:])
        elif session == "hm12":
            t.hm12b()
        elif session == "hm3":
            t.hm3b()
        elif session == "hm5":
            t.hm5b()
        else:
            raise SystemExit("unknown session " + session)
        t.lad.say("console session {} ended".format(session))
    except (c.Stop, night.Retire) as error:
        t.lad.say("!!! console session {} stopped: {}".format(session, error))
        for n in ((node,) if node else (1, 2)):
            try:
                st = t.lad.read(n)
                if st and (t.lad.enabled(st) or st["606c"] != 0):
                    t.lad.quick(n, "stop-quick-stationary")
            except Exception as e2:  # noqa: BLE001
                t.lad.say("!!! check on node {}: {}".format(n, e2))
        sys.exit(1)
    finally:
        t.lad.save()
