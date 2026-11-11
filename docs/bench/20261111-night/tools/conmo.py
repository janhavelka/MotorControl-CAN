"""Console motion sessions on the bench image (shared console, prompt 42 routes), one MCU boot each.

Usage: python conmo.py WORKTREE move NODE TRIALBASE    (node released in mode 1 with a valid start record)
       python conmo.py WORKTREE velocity NODE TRIALBASE (node released in mode 3)
Reads and updates the night soak's state.json (trial counter, start record, mode). Unattended, under the user's
waiver of 2026-11-11 (WAIVER below, also written to NOTE.txt beside the evidence). Stops at the first unexpected
answer; a console motion that may still run gets a console `stop fast` and then a bench quick stop.
"""
import datetime
import json
import os
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(r"C:\Users\Honza\Documents\Projects\MotorControl-CAN")
SCRATCH = pathlib.Path(r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad")
NIGHT = REPO / "docs" / "bench" / "20261111-night"
BASE = ["--port", "COM15", "--serial", "94A990398B40"]
WAIVER = ("test all the new features added - do a full night soak without my attendance / You can do motion "
          "tests, speeding up, changing speed on the fly etc / Finish at 9:00 next day in the morning")
UNATT = ["--unattended-waiver", WAIVER, "--waiver-covers-motion"]
IDENT = "262546,12,32,4096,1,0,1024,512"
REC = {1: 409, 2: 830}
EVIDENCE = "1,4096,4096,2,1,1,1"  # accept statusword 0x1000/0x1000, complete monitor 1/1, rule after-absence


def utc():
    return datetime.datetime.now(datetime.timezone.utc)


def stamp():
    return utc().strftime("%Y%m%dT%H%M%SZ")


class Stop(Exception):
    pass


class Session:
    def __init__(self, wt, kind, node):
        self.wt, self.kind, self.node = wt, kind, node
        self.state = json.loads((NIGHT / "state.json").read_text(encoding="utf-8"))
        self.state["series"] += 1
        self.dir = NIGHT / "S{:04d}-n{}-C{}".format(self.state["series"], node, kind[0])
        self.dir.mkdir(parents=True, exist_ok=False)
        (self.dir / "NOTE.txt").write_text(
            "Console motion session (shared console on the bench image), unattended under the user's written "
            "waiver, verbatim: " + WAIVER + "\nArming lines carry attended=1: the host gate was met by that "
            "waiver; nobody was at the bench and no physical observation was made.\n", encoding="utf-8")
        self.ledger = self.dir / "campaign.jsonl"
        self.newledger = True
        self.log = open(NIGHT / "night.log", "a", encoding="utf-8")
        self.say("##### series {} node {} console {} ({})".format(self.state["series"], node, kind, self.dir.name))
        self.save()

    def save(self):
        (NIGHT / "state.json").write_text(json.dumps(self.state, indent=2) + "\n", encoding="utf-8")

    def say(self, text):
        line = "{} {}".format(utc().strftime("%H:%M:%SZ"), text)
        print(line, flush=True)
        self.log.write(line + "\n")
        self.log.flush()

    def trial(self):
        self.state["trial"] += 1
        self.save()
        return self.state["trial"]

    def out(self, label):
        return str(self.dir / "{}-{}".format(stamp(), label))

    def bench(self, label, args, ok=(0,)):
        cmd = [sys.executable, "scripts/s2_bench.py"] + BASE + args
        self.say("=== bench " + label)
        r = subprocess.run(cmd, cwd=self.wt, input="", text=True, capture_output=True, errors="replace")
        text = r.stdout + r.stderr
        for line in text.splitlines():
            if any(k in line for k in ("TRIAL id", "TRIAL_LAST", "TRIAL_ENABLE_STEP word=15", "TRIAL_STOP", "REFUSED",
                                       "refused", "ALERT")):
                self.say("  " + line[:300])
        self.say("  exit {}".format(r.returncode))
        if r.returncode not in ok:
            raise Stop("bench {} exit {}".format(label, r.returncode))
        return text

    def console(self, label, steps, no_terminal=False, ok=(0,)):
        cmd = [sys.executable, "scripts/s2_bench_console.py", "--output", str(self.dir / "console-rx.bin")]
        for step in steps:
            cmd += ["--send", step]
        if no_terminal:
            cmd.append("--no-terminal")
        self.say("=== console {}: {}".format(label, " | ".join(steps)))
        r = subprocess.run(cmd, cwd=self.wt, text=True, capture_output=True, errors="replace")
        text = r.stdout + r.stderr
        with open(self.dir / "console-stdout.txt", "a", encoding="utf-8") as f:
            f.write("### {} {}\n{}\n".format(stamp(), label, text))
        for line in text.splitlines():
            if line.startswith(("SEND", "STOPPED")) or any(k in line for k in (
                    '"type":"terminal"', '"type":"reply"', "CONSOLE ", "TRIAL_REFUSED", "HEAP", "STATS",
                    '"type":"stop_attempt"', '"type":"motion_value"')):
                self.say("  " + line[:330])
        self.say("  exit {}".format(r.returncode))
        if r.returncode not in ok:
            raise Stop("console {} exit {}".format(label, r.returncode))
        return text

    def read(self):
        t = self.trial()
        text = self.bench("read {}".format(t), ["--command", "readiness-actions", "--node", str(self.node), "--trial",
                                               str(t), "--fault-led", "unobserved", "--seconds", "30",
                                               "--output", self.out("p1-{}".format(t))])
        self.bench("release", ["--command", "release", "--seconds", "1.5", "--output", self.out("release")])
        m = re.search(r"TRIAL_LAST passed=1 .*?1001=([0-9a-f]{2}) 603f=([0-9a-f]{4}) 2100=([0-9a-f]{4}) "
                      r"6041=([0-9a-f]{4}) 6061=([0-9a-f]{2}) 6064=(-?\d+) 606c=(-?\d+)", text)
        dec = lambda h: int.from_bytes(bytes.fromhex(h), "little", signed=True)
        pt = re.search(r"object=607a:00 .*? data=([0-9a-f]{8})", text)
        vt = re.search(r"object=60ff:00 .*? data=([0-9a-f]{8})", text)
        st = dict(zip(("1001", "603f", "2100", "6041", "6061"), m.groups()[:5]))
        st.update({"6064": int(m.group(6)), "606c": int(m.group(7)), "607a": dec(pt.group(1)), "60ff": dec(vt.group(1))})
        self.say("  state {}".format(st))
        return st

    def camp(self):
        args = ["--campaign", str(self.ledger)] + (["--new-campaign"] if self.newledger else [])
        self.newledger = False
        return args

    def begin(self):
        subprocess.run([sys.executable, str(SCRATCH / "en_reset.py"), str(self.dir / (stamp() + "-en-reset")),
                        "Console {} session, node {}".format(self.kind, self.node)],
                       env={"PYTHONIOENCODING": "utf-8", **os.environ}, capture_output=True)
        self.bench("start", ["--command", "start", "--output", self.out("start")])
        st = self.read()
        if st["1001"] != "00" or st["603f"] != "0000" or (int(st["6041"], 16) & 0x7f) != 0x50:
            raise Stop("not released without fault: {}".format(st))
        return st

    def enable(self, st):
        t = self.trial()
        nd = self.state["nodes"][str(self.node)]
        text = self.bench("enable {}".format(t), ["--command", "enable", "--node", str(self.node), "--trial", str(t),
                                                 "--status", "0x" + st["6041"], "--position-target", str(st["607a"]),
                                                 "--velocity-target", str(st["60ff"]), "--deadline-ms", "10000",
                                                 "--interval-ms", "1250", "--released-by-recorded-stop",
                                                 nd["stoprec"]] + UNATT + self.camp() +
                          ["--seconds", "60", "--output", self.out("enable-{}".format(t))])
        self.bench("release", ["--command", "release", "--seconds", "1.5", "--output", self.out("release")])
        self.state["nodes"][str(self.node)]["stoprec"] = None  # Console motion follows: no bench record covers it.
        self.save()
        return int(re.search(r"TRIAL_ENABLE_STEP word=15 .*? position=(-?\d+)", text).group(1))

    def ty(self, pos, relative):
        t = self.trial()
        base = "1,{}".format(pos) if relative else "0,0"
        return "TY{},{},1,{},1000,{},{},{},{},2000,2000,250".format(self.node, t, pos, REC[self.node], EVIDENCE, IDENT,
                                                                    base)

    def bench_quick(self):
        t = self.trial()
        text = self.bench("stop {}".format(t), ["--command", "stop", "--node", str(self.node), "--trial", str(t),
                                               "--kind", "quick", "--deadline-ms", "2000", "--best-effort-ms", "2000",
                                               "--interval-ms", "250"] + UNATT + self.camp() +
                          ["--seconds", "60", "--output", self.out("stop-quick-{}".format(t))], ok=(0, 3))
        self.bench("release", ["--command", "release", "--seconds", "1.5", "--output", self.out("release")])
        sys.path.insert(0, str(pathlib.Path(self.wt) / "scripts"))
        import s2_bench
        directory = sorted(self.dir.glob("*-stop-quick-{}".format(t)))[-1]
        try:
            s2_bench.check_stop_record(str(directory), self.node)
            self.state["nodes"][str(self.node)]["stoprec"] = str(directory)
            self.say("  settled stop record: start record renewed")
        except s2_bench.Refused as error:
            self.say("  bench quick stop gives no start record: " + str(error)[:160])
        self.save()

    def moves(self):
        st = self.begin()
        if st["6061"] != "01":
            raise Stop("move session needs mode 1")
        slot = self.node - 1
        p = st["6064"]
        self.console("premise", ["o", "M", "@1 host {}".format(slot), "TX{},{},1,{}".format(self.node, self.trial(), p),
                                 "r", "@2 stepsperturn 1000", "TX{},{},1,{}".format(self.node, self.trial(), p), "r",
                                 "@3 axis origin 0"])
        p = self.enable(st)
        self.console("move 1 (moveby, 60 rpm)", ["@4 speed 60 rpm", "@5 accel 300 ms", "@6 decel 300 ms",
                                                 "@7 motion write", self.ty(p, True), "@8 moveby 3 turn", "r",
                                                 "@9 release 1"])
        p = self.read()["6064"]
        target = -2000 if p > 0 else 2000
        self.console("move 2 (moveto)", ["@10 speed 120 rpm", self.ty(p, False), "@11 moveto {} steps".format(target),
                                         "r", "@12 release 2"])
        p = self.read()["6064"]
        self.console("move 3 (moveby 20 turn at 90 rpm, stop fast)", ["@13 speed 90 rpm", self.ty(p, True),
                                                                       "@14 moveby {} turn".format(-20 if p > 0 else 20)],
                     no_terminal=True)
        try:
            self.console("stop fast", ["@15 stop fast"])
        finally:
            self.console("results", ["@16 result 3", "s", "M"], ok=(0, 1, 2))
        self.console("release", ["@17 release 3", "@18 release 4", "r"], ok=(0, 1, 2))
        # A bench quick stop after the console's stop runs best effort only (S0054: failure=10, own=0) and leaves
        # the session refusing reads, so none is sent: the next bench velocity series renews the start record.
        st = self.read()
        self.state["nodes"][str(self.node)]["mode"] = st["6061"]
        self.save()

    def velocity(self):
        st = self.begin()
        if st["6061"] != "03":
            raise Stop("velocity session needs mode 3")
        slot = self.node - 1
        t = self.trial()
        tb = "TB{},{},1,30,200,200,4000,{},2,2,{},2000,2000,250".format(self.node, t, t, IDENT)
        self.console("velocity 1 (30 rpm, its own fast stop)", ["o", "M", "@1 host {}".format(slot), tb,
                                                                 "@2 velocity 30 rpm motor 12000 configured fast", "r",
                                                                 "s", "@3 release 1"])
        st = self.read()
        # The run's own HALT is a fallback only if the explicit stop fails; a HALT from a release with 2100
        # in-place clear left node 2 halted in a report no stop admits (S0071), so then the run's own stop is fast.
        own = "normal" if int(st["2100"], 16) & 0x0001 else "fast"
        kind = "1" if own == "normal" else "2"
        t = self.trial()
        tb = "TB{},{},1,45,200,200,4000,{},2,{},{},2000,2000,250".format(self.node, t, t, kind, IDENT)
        self.console("velocity 2 (45 rpm, own stop {}, explicit stop fast)".format(own),
                     [tb, "@4 velocity 45 rpm motor 12000 configured " + own], no_terminal=True)
        try:
            self.console("stop fast", ["@5 stop fast"])
        finally:
            self.console("results", ["@6 result 2", "s", "M"], ok=(0, 1, 2))
        self.console("release", ["@7 release 2", "@8 release 3", "r"], ok=(0, 1, 2))
        # Three stops are counted on this node already (two own stops, one explicit), so a bench stop route would
        # be over the per-boot cap; the next bench velocity series renews the start record.
        st = self.read()
        self.state["nodes"][str(self.node)]["stoprec"] = None
        self.state["nodes"][str(self.node)]["mode"] = st["6061"]
        self.save()


if __name__ == "__main__":
    wt, kind, node = sys.argv[1], sys.argv[2], int(sys.argv[3])
    s = Session(wt, kind, node)
    try:
        s.moves() if kind == "move" else s.velocity()
        s.say("console session ended")
    except Stop as error:
        s.say("!!! console session stopped: {}".format(error))
        try:
            s.console("emergency stop fast", ["@90 stop fast"], ok=(0, 1, 2))
        except Exception as e2:
            s.say("  console emergency stop: {}".format(e2))
        try:
            st = s.read()
            if (int(st["6041"], 16) & 0x6f) == 0x27 or st["606c"] != 0:
                s.bench_quick()
        except Exception as e3:
            s.say("!!! read/stop after the failure: {}".format(e3))
        sys.exit(1)
