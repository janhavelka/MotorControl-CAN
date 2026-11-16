"""Console-path HIL sessions for prompts 52-54 (2026-11-16), on image 17386514, one MCU boot per session.

Usage: python conhil.py WORKTREE SESSION [NODE]
Sessions: cmove N, cvel N, vchg-after N, vchg-down N, vchg-during N, mspeed N, mramps N, mretarget N, hm12, hm3, hm5.
Built on hil.py (the ladder/night driver: EN reset per series, bench reads, mode routes, enables, stop records) and
the night soak's conmo.py console pattern, adapted to the current console grammar (docs/cli_contract.md). Every
console invocation goes through scripts/s2_bench_console.py, which keeps every received byte. Arming and motion are
in one invocation (the arming age cap is about 3 s). A failure stops the session; a console motion that may still
run gets one console `stop N fast`, and a bench quick stop only when the drive still reports enabled or turning.
Console motion has no host ledger schema (tool limit); the firmware's per-boot caps still apply.
"""
import importlib.util
import json
import re
import subprocess
import sys
import time

SCRATCH = r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad"
spec = importlib.util.spec_from_file_location("hil", SCRATCH + r"\hil.py")
m = importlib.util.module_from_spec(spec)
argv = sys.argv
sys.argv = ["x"]
spec.loader.exec_module(m)
sys.argv = argv
night = m.night
IDENT = "262546,12,32,4096,1,0,1024,512"
EVIDENCE = "1,4096,4096,2,1,1,1"  # accept 6041 bit 12, complete 2100 bit 0, rule after-absence


class Stop(Exception):
    pass


class Con:
    def __init__(self, wt, lad):
        self.wt, self.lad, self.rid, self.all = wt, lad, 0, []

    def ids(self, n=1):
        first = self.rid + 1
        self.rid += n
        return first

    def run(self, label, steps, no_terminal=False, allow=False, ok=(0,)):
        cmd = [sys.executable, "scripts/s2_bench_console.py", "--output", str(self.lad.sdir / "console-rx.bin")]
        for step in steps:
            cmd += ["--send", step]
        if no_terminal:
            cmd.append("--no-terminal")
        if allow:
            cmd.append("--allow-changes")
        self.lad.say("=== console {}: {}{}".format(label, " | ".join(steps), " (no-terminal)" if no_terminal else ""))
        started = time.monotonic()
        r = subprocess.run(cmd, cwd=self.wt, text=True, capture_output=True, errors="replace")
        text = r.stdout + r.stderr
        with open(self.lad.sdir / "console-stdout.txt", "a", encoding="utf-8") as f:
            f.write("### {} {}\n{}\n".format(night.stamp(), label, text))
        records = []
        for line in text.splitlines():
            i = line.find("{")
            if i >= 0 and '"schema"' in line:
                try:
                    records.append(json.loads(line[i:]))
                except ValueError:
                    pass
            if line.startswith(("SEND", "STOPPED")) or any(k in line for k in (
                    '"type":"terminal"', '"type":"reply"', "CONSOLE ", "TRIAL_REFUSED", "TRIAL admitted", "HEAP",
                    "STATS", '"type":"stop_attempt"', '"type":"velocity_change"', '"type":"move_relation"',
                    '"type":"group_stop', "refused")):
                self.lad.say("  " + line[:360])
        self.lad.say("  exit {} ({:.1f} s)".format(r.returncode, time.monotonic() - started))
        self.all.extend(records)
        self.lad.note(kind="console", label=label, steps=steps, rc=r.returncode, records=records[-40:],
                      dir=str(self.lad.sdir))
        if r.returncode not in ok:
            raise Stop("console {} exit {}".format(label, r.returncode))
        return r.returncode, records


def op_of(records, rid):
    for rec in records:
        if rec.get("type") == "reply" and rec.get("id") == rid and rec.get("operation_id") is not None:
            return rec["operation_id"]
    return None


def terminal_of(records, op):
    for rec in records:
        if rec.get("type") == "terminal" and rec.get("operation_id") == op:
            return rec
    return None


class Sessions:
    def __init__(self, wt):
        self.wt = wt
        self.lad = m.Ladder(wt, False)
        self.con = Con(wt, self.lad)

    def ty(self, node, pos):
        t = self.lad.next()
        return "TY{},{},1,{},500,{},{},{},0,0,1500,1500,500".format(node, t, pos, t, EVIDENCE, IDENT)

    def tb(self, node, rpm, ramp, kind):
        t = self.lad.next()
        return "TB{},{},1,{},{},{},500,{},2,{},{},1500,1500,500".format(node, t, rpm, ramp, ramp, t, kind, IDENT)

    def boot_console(self):
        """The first console invocation of a boot opens the console and prints the heap."""
        self.con.rid = 0
        self.con.all = []
        return ["o", "M"]

    def release(self, ops, extra=()):
        extra = [x for x in extra if x != "r"]  # every T record is released in its own invocation already
        steps = ["@{} release {}".format(self.con.ids(), op) for op in ops if op is not None] + list(extra)
        if steps:
            self.con.run("release", steps, ok=(0, 1, 2))

    def results(self, ops):
        """One `result` per invocation: the helper ends an invocation at the first ok:false record, which an
        interrupted motion's result is. Returns every terminal of these operations seen in this boot."""
        for op in ops:
            if op is not None:
                self.con.run("result {}".format(op), ["@{} result {}".format(self.con.ids(), op)], ok=(0, 1, 2))
        self.con.run("stats", ["s", "M"], ok=(0, 1, 2))
        wanted = set(op for op in ops if op is not None)
        seen, out = set(), []
        for r in self.con.all:
            if r.get("type") in ("terminal", "group_stop_axis") and r.get("operation_id") in wanted:
                key = (r.get("type"), r.get("operation_id"), r.get("axis_id"), r.get("completed_us"))
                if key not in seen:
                    seen.add(key)
                    out.append(r)
        return out

    def emergency(self, node):
        try:
            self.con.run("emergency stop", ["@{} stop {} fast".format(self.con.ids(), node)], ok=(0, 1, 2))
        except Exception as error:  # noqa: BLE001 - keep going to the bench check
            self.lad.say("  console emergency stop: {}".format(error))
        st = self.lad.read(node)
        if st and (self.lad.enabled(st) or st["606c"] != 0):
            self.lad.quick(node, "stop-quick-stationary")

    # ---- single-axis console regression
    def cmove(self, node):
        pos = self.lad.position_series(node, "cmove")
        s, ops = node - 1, []
        a = self.con.ids(4)
        _, rec = self.con.run("move 1 to completion (60 rpm)", self.boot_console() + [
            "@{} host {}".format(a, s), "@{} accel 500 ms".format(a + 1), "@{} decel 500 ms".format(a + 2),
            self.ty(node, pos), "@{} move absolute {} steps native 60 configured setup write".format(a + 3, pos + 3000),
            "r"])
        op = op_of(rec, a + 3)
        term = terminal_of(rec, op)
        self.lad.say("  CMOVE1 op={} terminal={}".format(op, {k: term.get(k) for k in ("ok", "outcome", "uncertain",
                     "execution_code", "completion_code")} if term else None))
        self.release([op])
        p = self.lad.read(node)["6064"]
        a = self.con.ids()
        _, rec = self.con.run("move 2 to completion (120 rpm)", [
            self.ty(node, p), "@{} move absolute {} steps native 120 configured setup write".format(a, p - 3000), "r"])
        op = op_of(rec, a)
        term = terminal_of(rec, op)
        self.lad.say("  CMOVE2 op={} terminal={}".format(op, {k: term.get(k) for k in ("ok", "outcome", "uncertain",
                     "execution_code", "completion_code")} if term else None))
        self.release([op])
        p = self.lad.read(node)["6064"]
        a = self.con.ids()
        _, rec = self.con.run("move 3 (90 rpm, 9000 counts) with a fast stop", [
            self.ty(node, p), "@{} move absolute {} steps native 90 configured setup write".format(a, p + 9000), "r"],
            no_terminal=True)
        op = op_of(rec, a)
        time.sleep(2.5)
        b = self.con.ids()
        _, srec = self.con.run("stop {} fast".format(node), ["@{} stop {} fast".format(b, node)], ok=(0, 1, 2))
        sop = op_of(srec, b)
        time.sleep(1.0)
        rec = self.results([op, sop])
        self.lad.say("  CMOVE3 op={} stop={} terminals={}".format(op, sop, [
            {k: r.get(k) for k in ("operation_id", "ok", "outcome", "uncertain", "execution_code", "completion_code")}
            for r in rec if r.get("type") == "terminal"]))
        self.release([sop, op], ["r"])
        st = self.lad.read(node)
        self.lad.say("  CMOVE end state {}".format(st))

    def cvel(self, node):
        st = self.lad.begin_series(node, "cvel")
        s = node - 1
        a = self.con.ids(2)
        _, rec = self.con.run("velocity 1 (30 rpm, own fast stop)", self.boot_console() + [
            "@{} host {}".format(a, s), self.tb(node, 30, 500, 2),
            "@{} velocity 30 rpm motor 12000 configured fast".format(a + 1), "r", "s"])
        op = op_of(rec, a + 1)
        term = terminal_of(rec, op)
        self.lad.say("  CVEL1 op={} terminal={}".format(op, {k: term.get(k) for k in ("ok", "outcome", "uncertain",
                     "execution_code", "completion_code")} if term else None))
        self.release([op])
        st = self.lad.read(node)
        own = "normal" if int(st["2100"], 16) & 0x0001 and not int(st["2100"], 16) & 0x0004 else "fast"
        a = self.con.ids()
        _, rec = self.con.run("velocity 2 (45 rpm, own {}) with an explicit fast stop".format(own), [
            self.tb(node, 45, 500, 1 if own == "normal" else 2),
            "@{} velocity 45 rpm motor 12000 configured {}".format(a, own), "r"], no_terminal=True)
        op = op_of(rec, a)
        time.sleep(5.0)
        b = self.con.ids()
        _, srec = self.con.run("stop {} fast".format(node), ["@{} stop {} fast".format(b, node)], ok=(0, 1, 2))
        sop = op_of(srec, b)
        time.sleep(1.0)
        rec = self.results([op, sop])
        self.lad.say("  CVEL2 op={} stop={} terminals={}".format(op, sop, [
            {k: r.get(k) for k in ("operation_id", "ok", "outcome", "uncertain", "execution_code", "completion_code")}
            for r in rec if r.get("type") == "terminal"]))
        self.release([sop, op])
        self.lad.say("  CVEL end state {}".format(self.lad.read(node)))

    # ---- typed velocity change (prompt 52 through the console, --allow-changes)
    def vchg(self, node, variant):
        st = self.lad.begin_series(node, "vchg" + variant)
        s = node - 1
        start, new, ramp, change_at, stop_at = {"after": (12, 30, 500, 5.5, None), "down": (60, 12, 1000, 5.5, None),
                                                "during": (6, 60, 2000, 6.0, 0.6)}[variant]
        a = self.con.ids(2)
        _, rec = self.con.run("velocity {} rpm, changes allowed, ramp {} ms".format(start, ramp), self.boot_console() + [
            "@{} host {}".format(a, s), self.tb(node, 60, ramp, 2),
            "@{} velocity {} rpm motor 12000 configured fast".format(a + 1, start), "r"], no_terminal=True, allow=True)
        t0 = time.monotonic()
        op = op_of(rec, a + 1)
        if op is None:
            raise Stop("velocity not admitted")
        time.sleep(max(0.0, change_at - (time.monotonic() - t0)))
        c = self.con.ids()
        _, crec = self.con.run("velocity change {} -> {} rpm".format(start, new),
                               ["@{} velocity change {} {} rpm motor configured".format(c, op, new)],
                               no_terminal=True, ok=(0, 1, 2))
        sop = None
        if stop_at is not None:
            time.sleep(stop_at)
            b = self.con.ids()
            _, srec = self.con.run("stop {} fast during the change".format(node),
                                   ["@{} stop {} fast".format(b, node)], ok=(0, 1, 2))
            sop = op_of(srec, b)
            time.sleep(1.0)
        else:
            time.sleep(max(0.0, 15.0 - (time.monotonic() - t0)))  # the run's own stop is due at about 12 s
        rec = self.results([op, sop])
        self.lad.say("  VCHG {} op={} stop={} change={} terminals={}".format(variant, op, sop, [
            {k: r.get(k) for k in ("type", "ok", "status", "result")} for r in crec if r.get("id") == c][:2], [
            {k: r.get(k) for k in ("operation_id", "ok", "outcome", "uncertain", "execution_code", "completion_code")}
            for r in rec if r.get("type") == "terminal"]))
        for r in self.con.all:
            if r.get("type") == "velocity_change":
                self.lad.say("  VCHG record {}".format(json.dumps(r)[:600]))
        self.release([sop, op])
        self.lad.say("  VCHG end state {}".format(self.lad.read(node)))

    # ---- typed move changes (prompt 52 through the console, --allow-changes)
    def mchg(self, node, kind):
        pos = self.lad.position_series(node, "m" + kind)
        s = node - 1
        a = self.con.ids(4)
        _, rec = self.con.run("move 3000 counts at 30 rpm, changes allowed", self.boot_console() + [
            "@{} host {}".format(a, s), "@{} accel 500 ms".format(a + 1), "@{} decel 500 ms".format(a + 2),
            self.ty(node, pos), "@{} move absolute {} steps native 30 configured setup write".format(a + 3, pos + 3000),
            "r"], no_terminal=True, allow=True)
        t0 = time.monotonic()
        op = op_of(rec, a + 3)
        if op is None:
            raise Stop("move not admitted")
        time.sleep(max(0.0, 1.5 - (time.monotonic() - t0)))
        c = self.con.ids()
        line = {"speed": "@{} move speed {} 60 immediate".format(c, op),
                "ramps": "@{} move ramps {} 200 800 immediate".format(c, op),
                "retarget": "@{} move retarget {} immediate absolute {} steps native 30 configured".format(
                    c, op, pos + 4000)}[kind]
        _, crec = self.con.run("move {} change".format(kind), [line], no_terminal=True, ok=(0, 1, 2))
        rop = op_of(crec, c)
        time.sleep(max(0.0, 9.0 - (time.monotonic() - t0)))
        rec = self.results([op, rop])
        terms = [r for r in rec if r.get("type") == "terminal"]
        self.lad.say("  MCHG {} op={} replacement={} change={} terminals={}".format(kind, op, rop, [
            {k: r.get(k) for k in ("type", "ok", "status", "result")} for r in crec if r.get("id") == c][:2], [
            {k: r.get(k) for k in ("operation_id", "ok", "outcome", "uncertain", "execution_code", "completion_code")}
            for r in terms]))
        for r in self.con.all:
            if r.get("type") in ("move_relation", "motion_value") and r.get("operation_id") in (op, rop):
                self.lad.say("  MCHG record {}".format(json.dumps(r)[:500]))
        self.release([op, rop])
        st = self.lad.read(node)
        self.lad.say("  MCHG end state {}".format(st))
        if st and self.lad.enabled(st) and st["606c"] == 0:
            self.lad.quick(node, "stop-quick-stationary")
        elif st and st["606c"] != 0:
            self.emergency(node)

    # ---- two axes
    def two_enabled(self, label):
        """Both nodes released in mode 1 with fresh stop records, then both enabled in one boot."""
        for n in (1, 2):
            self.lad.position_series(n, label + "p")
            text, d = self.lad.quick(n, "stop-quick-stationary")
            if not self.lad.good_stop(d, n):
                raise Stop("node {} stationary quick stop gave no start record".format(n))
            self.lad.s["nodes"][str(n)]["stoprec"] = d
            self.lad.save()
        self.lad.begin(1, label)
        st1, st2 = self.lad.read(1), self.lad.read(2)
        for st in (st1, st2):
            if not st or not self.lad.released(st) or st["6061"] != "01":
                raise Stop("not released in mode 1: {}".format(st))
        return self.lad.enable(1, st1), self.lad.enable(2, st2)

    def paired_moves(self, p1, p2, dist, rpm, label, first=True):
        a = self.con.ids(8)
        steps = (self.boot_console() if first else []) + [
            "@{} host 0".format(a), "@{} accel 500 ms".format(a + 1), "@{} decel 500 ms".format(a + 2),
            self.ty(1, p1), "@{} move absolute {} steps native {} configured setup write".format(a + 3, p1 - dist, rpm),
            "r", "@{} host 1".format(a + 4), "@{} accel 500 ms".format(a + 5), "@{} decel 500 ms".format(a + 6),
            self.ty(2, p2), "@{} move absolute {} steps native {} configured setup write".format(a + 7, p2 + dist, rpm),
            "r"]
        _, rec = self.con.run(label, steps, no_terminal=True)
        return op_of(rec, a + 3), op_of(rec, a + 7), time.monotonic()

    def hm12(self):
        p1, p2 = self.two_enabled("hm12")
        # HM1: paired absolute moves in opposite directions, 30 rpm, both to completion.
        o1, o2, t0 = self.paired_moves(p1, p2, 3000, 30, "HM1 paired moves, opposite directions")
        time.sleep(max(0.0, 10.0 - (time.monotonic() - t0)))
        rec = self.results([o1, o2])
        self.lad.say("  HM1 terminals={}".format([
            {k: r.get(k) for k in ("operation_id", "ok", "outcome", "uncertain", "execution_code", "completion_code")}
            for r in rec if r.get("type") == "terminal"]))
        self.release([o1, o2])
        q1, q2 = self.lad.read(1)["6064"], self.lad.read(2)["6064"]
        self.lad.say("  HM1 end positions n1 {} (target {}), n2 {} (target {})".format(q1, p1 - 3000, q2, p2 + 3000))
        # HM2: paired moves again, HALT node 1 while node 2 completes.
        o1, o2, t0 = self.paired_moves(q1, q2, 3500, 30, "HM2 paired moves", first=False)
        time.sleep(max(0.0, 4.0 - (time.monotonic() - t0)))
        b = self.con.ids()
        _, srec = self.con.run("HM2 stop 1 normal (HALT) while node 2 continues", ["@{} stop 1 normal".format(b)],
                               ok=(0, 1, 2))
        sop = op_of(srec, b)
        time.sleep(max(0.0, 11.0 - (time.monotonic() - t0)))
        rec = self.results([o1, o2, sop])
        self.lad.say("  HM2 terminals={}".format([
            {k: r.get(k) for k in ("operation_id", "ok", "outcome", "uncertain", "execution_code", "completion_code")}
            for r in rec if r.get("type") == "terminal"]))
        self.release([sop, o1, o2])
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM2 end n{} {}".format(n, st))
            if st and self.lad.enabled(st):
                self.lad.quick(n, "stop-quick-halted" if n == 1 else "stop-quick-stationary")

    def hm3(self):
        p1, p2 = self.two_enabled("hm3")
        o1, o2, t0 = self.paired_moves(p1, p2, 4000, 30, "HM3 paired moves")
        time.sleep(max(0.0, 4.5 - (time.monotonic() - t0)))
        b = self.con.ids()
        _, grec = self.con.run("HM3 stop all normal (group HALT)", ["@{} stop all normal".format(b)], ok=(0, 1, 2))
        gop = op_of(grec, b)
        time.sleep(1.5)
        rec = self.results([gop])
        self.release([gop])
        c = self.con.ids()
        _, frec = self.con.run("HM3 stop all fast (group quick stop)", ["@{} stop all fast".format(c)], ok=(0, 1, 2))
        fop = op_of(frec, c)
        time.sleep(1.5)
        rec2 = self.results([fop, o1, o2])
        self.lad.say("  HM3 group records={}".format([
            {k: r.get(k) for k in ("type", "operation_id", "axis_id", "ok", "outcome", "uncertain")}
            for r in rec + rec2 if r.get("type") in ("terminal", "group_stop_axis")]))
        self.release([fop, o1, o2])
        for n in (1, 2):
            self.lad.say("  HM3 end n{} {}".format(n, self.lad.read(n)))

    def hm5(self):
        for n in (1, 2):
            self.lad.begin_series(n, "hm5p")
        self.lad.begin(1, "hm5")
        sts = {n: self.lad.read(n) for n in (1, 2)}
        kinds = {}
        for n, st in sts.items():
            if not st or not self.lad.released(st) or st["6061"] != "03":
                raise Stop("not released in mode 3: {}".format(st))
            word = int(st["2100"], 16)
            kinds[n] = "normal" if (word & 0x0001) and not (word & 0x0004) else "fast"
        a = self.con.ids(4)
        steps = self.boot_console() + [
            "@{} host 0".format(a), self.tb(1, 30, 500, 1 if kinds[1] == "normal" else 2),
            "@{} velocity 30 rpm motor 12000 configured {}".format(a + 1, kinds[1]), "r",
            "@{} host 1".format(a + 2), self.tb(2, 30, 500, 1 if kinds[2] == "normal" else 2),
            "@{} velocity 30 rpm motor 12000 configured {}".format(a + 3, kinds[2]), "r"]
        _, rec = self.con.run("HM5 paired velocity, own stop per axis ({})".format(kinds), steps, no_terminal=True)
        o1, o2 = op_of(rec, a + 1), op_of(rec, a + 3)
        t0 = time.monotonic()
        time.sleep(16.0)
        rec = self.results([o1, o2])
        self.lad.say("  HM5 terminals={}".format([
            {k: r.get(k) for k in ("operation_id", "ok", "outcome", "uncertain", "execution_code", "completion_code")}
            for r in rec if r.get("type") == "terminal"]))
        self.release([o1, o2])
        for n in (1, 2):
            st = self.lad.read(n)
            self.lad.say("  HM5 end n{} {}".format(n, st))
            if st and self.lad.enabled(st):
                self.lad.quick(n, "stop-quick-halted")


if __name__ == "__main__":
    wt, session = argv[1], argv[2]
    node = int(argv[3]) if len(argv) > 3 else 0
    s = Sessions(wt)
    try:
        if session == "cmove":
            s.cmove(node)
        elif session == "cvel":
            s.cvel(node)
        elif session.startswith("vchg-"):
            s.vchg(node, session.split("-", 1)[1])
        elif session in ("mspeed", "mramps", "mretarget"):
            s.mchg(node, session[1:])
        elif session == "hm12":
            s.hm12()
        elif session == "hm3":
            s.hm3()
        elif session == "hm5":
            s.hm5()
        else:
            raise SystemExit("unknown session " + session)
        s.lad.say("console session {} ended".format(session))
    except (Stop, night.Retire) as error:
        s.lad.say("!!! console session {} stopped: {}".format(session, error))
        for n in ((node,) if node else (1, 2)):
            try:
                s.emergency(n)
            except Exception as e2:  # noqa: BLE001
                s.lad.say("!!! emergency on node {}: {}".format(n, e2))
        sys.exit(1)
    finally:
        s.lad.save()
