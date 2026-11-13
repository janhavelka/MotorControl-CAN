"""Unattended night soak on powered drives (2026-11-11/12), under the user's written waiver.

Usage: python night.py WORKTREE END_UTC [--dry] [--resume] [--only NODE]
  END_UTC like 2026-11-12T07:30  (no new series starts after it)

One series = one MCU boot (EN reset) = one node = one campaign ledger (12 motion admissions at most).
Rotation per node: Pa (position sweep ending in a moving quick stop), Pb (ending in a stationary quick stop),
V (velocity sweep, mode 3, rising rates, quick stops), and every second V a HALT variant.
Every series starts and ends released in mode 1, with a settled quick stop recorded for the next enable.
Any unexpected result: read, make the node safe (quick stop if enabled), record it, and retire the node
unless it is released at standstill with a settled stop record. Never clears a fault, never enables from a
power-up state, never replays an uncertain write. A file named STOP in the night directory ends the soak
after the current step.
"""
import datetime
import json
import os
import pathlib
import random
import re
import subprocess
import sys
import time

REPO = pathlib.Path(r"C:\Users\Honza\Documents\Projects\MotorControl-CAN")
SCRATCH = pathlib.Path(r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad")
NIGHT = pathlib.Path(os.environ.get("NIGHT_DIR", str(REPO / "docs" / "bench" / "20261111-night")))
BASE = ["--port", "COM15", "--serial", "94A990398B40"]
WAIVER = ("test all the new features added - do a full night soak without my attendance / You can do motion "
          "tests, speeding up, changing speed on the fly etc / Finish at 9:00 next day in the morning")
UNATT = ["--unattended-waiver", WAIVER, "--waiver-covers-motion"]
IDENT = ["--device-type", "262546", "--hardware-version", "12", "--software-version", "32", "--vendor-id",
         "4096", "--product-code", "1", "--revision", "0", "--model-code", "1024", "--firmware-code", "512"]
REC = {1: "409", 2: "830"}
MOVE_EVIDENCE = ["--accept-source", "statusword", "--accept-mask", "0x1000", "--accept-value", "0x1000",
                 "--complete-source", "monitor", "--complete-mask", "1", "--complete-value", "1",
                 "--rule", "after-absence"]
RPMS = [6, 10, 20, 30, 45, 60, 90, 120, 150, 180, 240, 300]
RAMPS = [100, 200, 300, 500, 800, 1000]
VRATES = [(6, 30, 60), (12, 24, 48), (10, 40, 60), (20, 35, 50), (15, 45, 60), (30, 45, 60), (6, 12, 18)]
LIMIT = 30000  # |position| bound for planned targets, counts
KEYS = ("TRIAL id", "TRIAL_LAST", "TRIAL_MOVE own", "TRIAL_MOVE_SAMPLE which=1", "TRIAL_ENABLE ",
        "TRIAL_ENABLE_STEP word=15", "TRIAL_STOP", "TRIAL_VELOCITY ", "TRIAL_VELOCITY_WINDOW", "TRIAL_BEST_EFFORT",
        "REFUSED", "refused", "ALERT", "error:")


def utc():
    return datetime.datetime.now(datetime.timezone.utc)


def stamp():
    return utc().strftime("%Y%m%dT%H%M%SZ")


class Retire(Exception):
    pass


class Night:
    def __init__(self, wt, end, dry):
        self.wt, self.end, self.dry = wt, end, dry
        NIGHT.mkdir(parents=True, exist_ok=True)
        self.statefile = NIGHT / "state.json"
        self.log = open(NIGHT / "night.log", "a", encoding="utf-8")
        self.results = open(NIGHT / "results.jsonl", "a", encoding="utf-8")
        if self.statefile.exists():
            self.s = json.loads(self.statefile.read_text(encoding="utf-8"))
        else:
            self.s = {"trial": 3100, "series": 0, "rng": 20261111,
                      "nodes": {"1": {"stoprec": str(REPO / "docs/bench/20261110T1605Z-s23-n1-2340-stop-quick-2360"),
                                      "retired": None, "rot": 0},
                                "2": {"stoprec": str(REPO / "docs/bench/20261110T1609Z-s23-n2-2400-stop-quick-2412"),
                                      "retired": None, "rot": 0}}}
        self.rng = random.Random(self.s["rng"] + self.s["series"])
        sys.path.insert(0, str(pathlib.Path(wt) / "scripts"))
        import s2_bench
        self.bench = s2_bench

    # ---- bookkeeping
    def save(self):
        if not self.dry:
            self.statefile.write_text(json.dumps(self.s, indent=2) + "\n", encoding="utf-8")

    def say(self, text):
        line = "{} {}".format(utc().strftime("%H:%M:%SZ"), text)
        print(line, flush=True)
        self.log.write(line + "\n")
        self.log.flush()

    def record(self, **values):
        values["utc"] = utc().isoformat(timespec="seconds")
        self.results.write(json.dumps(values) + "\n")
        self.results.flush()

    def next(self):
        self.s["trial"] += 1
        return str(self.s["trial"])

    def stopfile(self):
        return (NIGHT / "STOP").exists()

    # ---- runner
    def run(self, label, args, ok=(0,)):
        cmd = [sys.executable, "scripts/s2_bench.py"] + BASE + args + (["--dry-run"] if self.dry else [])
        self.say("=== " + label)
        started = time.monotonic()
        r = subprocess.run(cmd, cwd=self.wt, input="", text=True, capture_output=True, errors="replace")
        text = r.stdout + r.stderr
        refusals = [l for l in text.splitlines() if "would refuse" in l and "campaign ledger" not in l and "night-dry" not in l]
        if self.dry and refusals:
            self.say("  DRY: " + refusals[0][:200])
            return 2, text
        if self.dry and r.returncode == 0 and "--trial" in args:
            text += self.fake(args)
        for line in text.splitlines():
            if any(k in line for k in KEYS):
                self.say("  " + line[:360])
        self.say("  exit {} ({:.1f} s)".format(r.returncode, time.monotonic() - started))
        return r.returncode, text

    def fake(self, args):
        node, trial = args[args.index("--node") + 1], args[args.index("--trial") + 1]
        target = int(args[args.index("--target") + 1]) if "--target" in args else 0
        start = int(args[args.index("--start-position") + 1]) if "--start-position" in args else 0
        final = start + target if args[1] == "move" else target if args[1] == "position" else start
        return ("\nTRIAL id={t} node={n} route=x failure=0 why=none\nTRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 "
                "2100=0001 6041=9437 6061=01 6064={f} 606c=0\nTRIAL_MOVE_SAMPLE which=1 x=1 position={f}\n"
                "TRIAL_ENABLE_STEP word=15 x=1 position={f}\n").format(t=trial, n=node, f=final)

    def out(self, label):
        return str(self.sdir / "{}-{}".format(stamp(), label))

    def camp(self):
        args = ["--campaign", str(self.ledger)] + (["--new-campaign"] if self.newledger else [])
        self.newledger = False
        return args

    def release(self, what):
        self.run("release " + what, ["--command", "release", "--seconds", "1.5", "--output",
                                     self.out("release-" + what)])

    def read(self, node):
        t = self.next()
        rc, text = self.run("{} read node {}".format(t, node),
                            ["--command", "readiness-actions", "--node", str(node), "--trial", t, "--fault-led",
                             "unobserved", "--seconds", "30", "--output", self.out("p1-n{}-{}".format(node, t))])
        self.release(t)
        if self.dry:
            return {"2100": "0011", "6041": "8050", "6061": "01", "6064": 0, "606c": 0, "1001": "00",
                    "603f": "0000", "607a": 0, "60ff": 30}
        m = re.search(r"TRIAL_LAST passed=1 .*?1001=([0-9a-f]{2}) 603f=([0-9a-f]{4}) 2100=([0-9a-f]{4}) "
                      r"6041=([0-9a-f]{4}) 6061=([0-9a-f]{2}) 6064=(-?\d+) 606c=(-?\d+)", text)
        if rc != 0 or not m:
            return None
        dec = lambda h: int.from_bytes(bytes.fromhex(h), "little", signed=True)
        pt = re.search(r"object=607a:00 .*? data=([0-9a-f]{8})", text)
        vt = re.search(r"object=60ff:00 .*? data=([0-9a-f]{8})", text)
        st = dict(zip(("1001", "603f", "2100", "6041", "6061"), m.groups()[:5]))
        st["6064"], st["606c"] = int(m.group(6)), int(m.group(7))
        st["607a"] = dec(pt.group(1)) if pt else None
        st["60ff"] = dec(vt.group(1)) if vt else None
        self.say("  state n{} 2100={} 6041={} 6061={} 6064={} 606c={} 1001={} 603f={} 607a={} 60ff={}".format(
            node, st["2100"], st["6041"], st["6061"], st["6064"], st["606c"], st["1001"], st["603f"],
            st["607a"], st["60ff"]))
        return st

    def motion(self, node, label, args, ok=(0,), seconds=90):
        """One counted motion route; returns (rc, text, directory)."""
        if self.stopfile():
            raise Retire("STOP file present")
        t = self.next()
        directory = self.out("{}-n{}-{}".format(label, node, t))
        self.last_admitted = True
        rc, text = self.run("{} {} n{}".format(t, label, node),
                            ["--command", args[0], "--node", str(node), "--trial", t] + args[1:] + UNATT +
                            self.camp() + ["--seconds", str(seconds), "--output", directory])
        self.last_admitted = rc != 2
        self.release(t)
        last = re.findall(r"TRIAL_LAST passed=1 .*? 2100=([0-9a-f]{4}) 6041=([0-9a-f]{4}) 6061=([0-9a-f]{2}) "
                          r"6064=(-?\d+) 606c=(-?\d+)", text)
        head = re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=(\d+) why=(\S+)", text)
        rec = {"series": self.s["series"], "node": node, "trial": int(t), "route": label, "args": args, "rc": rc,
               "failure": head.group(1) if head else None, "why": head.group(2) if head else None,
               "last": last[-1] if last else None, "dir": directory}
        stopline = re.search(r"TRIAL_STOP .*", text)
        if stopline:
            rec["stop"] = stopline.group(0)[:300]
        sample = re.search(r"TRIAL_MOVE_SAMPLE which=1 .*?position=(-?\d+)", text)
        if sample:
            rec["move_final"] = int(sample.group(1))
        self.record(**rec)
        if rc == 4:
            self.say("!!! ALERT on node {}: release and quick stop now".format(node))
            self.emergency(node)
            raise Retire("runner alert after {}".format(label))
        if rc not in ok:
            raise Retire("unexpected exit {} after {} {}".format(rc, t, label))
        return rc, text, directory, rec

    def emergency(self, node):
        self.release("alert")
        t = self.next()
        self.run("{} emergency stop n{}".format(t, node),
                 ["--command", "stop", "--node", str(node), "--trial", t, "--kind", "quick", "--deadline-ms", "2000",
                  "--best-effort-ms", "2000", "--interval-ms", "250"] + UNATT + self.camp() +
                 ["--seconds", "30", "--output", self.out("stop-emergency-n{}-{}".format(node, t))])
        self.release("emergency-" + t)

    # ---- helpers
    def good_stop(self, directory, node):
        """The runner's own --released-by-recorded-stop check of a record, read only."""
        if self.dry:
            return True
        try:
            self.bench.check_stop_record(directory, node)
            return True
        except self.bench.Refused as error:
            self.say("  not a settled stop record: " + str(error)[:200])
            return False

    @staticmethod
    def released(st):
        return bool(st) and (int(st["6041"], 16) & 0x7f) == 0x50

    @staticmethod
    def enabled(st):
        return st and (int(st["6041"], 16) & 0x6f) == 0x27

    def window(self, distance, rpm, up, down, interval):
        cps = rpm * 1000 / 60.0 * 0.99
        seconds = abs(distance) / cps + (up + down) / 1000.0 + 1.5
        return seconds, max(4, int(seconds * 1000 / interval) + 3)

    def pick_target(self, pos, rpm, up, down, lo_s, hi_s):
        """An absolute target reachable at rpm in lo_s..hi_s seconds of travel, inside +-LIMIT."""
        # Velocity runs only turn positive (the user forbids negative speeds), so positions drift up between
        # position series: beyond half the bound every move heads back toward zero, and a target is accepted
        # inside the bound or, from outside it, anywhere nearer zero than the start.
        cps = rpm * 1000 / 60.0
        for _ in range(50):
            seconds = self.rng.uniform(lo_s, hi_s)
            dist = int(cps * seconds) // 10 * 10
            sign = -1 if pos > LIMIT // 2 else 1 if pos < -LIMIT // 2 else self.rng.choice((-1, 1))
            target = pos + sign * dist
            if abs(target) > LIMIT and abs(pos) <= LIMIT // 2:
                target = pos - sign * dist
            if dist >= 100 and (abs(target) <= LIMIT or abs(target) < abs(pos)):
                return target
        return pos - 1000 if pos > 0 else pos + 1000

    # ---- route wrappers
    def enable(self, node, st):
        nd = self.s["nodes"][str(node)]
        rc, text, d, _ = self.motion(node, "enable", ["enable", "--status", "0x" + st["6041"], "--position-target",
                                                       str(st["607a"]), "--velocity-target", str(st["60ff"]),
                                                       "--deadline-ms", "10000", "--interval-ms", "1250",
                                                       "--released-by-recorded-stop", nd["stoprec"]], seconds=60)
        m = re.search(r"TRIAL_ENABLE_STEP word=15 .*? position=(-?\d+)", text)
        head = re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=0 ", text)
        if not head:
            raise Retire("enable failed")
        if not m:
            raise Retire("enable: no word 15 position")
        return int(m.group(1))  # The admission read's TRIAL_LAST predates word 7's pull.

    def fits(self, node, argv):
        """The runner's own argument and capacity checks, in process, before anything is counted."""
        import contextlib
        import io
        full = BASE + ["--command", argv[0], "--node", str(node), "--trial", "4000000000"] + argv[1:] + UNATT + \
            ["--campaign", str(self.sdir / "x.jsonl"), "--output", "x"]
        err = io.StringIO()
        try:
            with contextlib.redirect_stderr(err):
                args, fields = self.bench.arguments(full)
                self.bench.check_bounds(args.command, fields)
            return True
        except SystemExit:
            self.say("  plan refused by argument check: " + err.getvalue().strip()[-200:])
            return False
        except self.bench.Refused as error:
            self.say("  plan refused: " + str(error)[:200])
            return False

    def position(self, node, pos, rpm, up, down, lo, hi, stop=None):
        # A move's completed sample is not always its rest position: the rotor settled 1 count further after
        # 3444 (-1864 then -1865), and the route's start check is exact. Start from a fresh read.
        fresh = self.read(node)
        if fresh and fresh["6064"] != pos:
            self.say("  start position from a fresh read: {} (planned from {})".format(fresh["6064"], pos))
            self.record(kind="settle", node=node, planned=pos, read=fresh["6064"], series=self.s["series"])
        pos = fresh["6064"] if fresh else pos
        for attempt in range(6):
            target = self.pick_target(pos, rpm, up, down, lo, hi)
            if stop:
                interval, cycles = 1000, 6
            else:
                interval = 1000
                seconds, cycles = self.window(target - pos, rpm, up, down, interval)
                if cycles > 15:
                    interval = 2000
                    seconds, cycles = self.window(target - pos, rpm, up, down, interval)
            args = ["position", "--start-position", str(pos), "--target", str(target), "--rpm", str(rpm),
                    "--ramp-up-ms", str(up), "--ramp-down-ms", str(down), "--cycles", str(min(cycles, 16)),
                    "--interval-ms", str(interval)]
            if stop:
                args += ["--stop-kind", stop, "--stop-after", "2", "--stop-deadline-ms", "2000",
                         "--stop-best-effort-ms", "0", "--stop-interval-ms", "500"]
            else:
                args += ["--stop-kind", "none"]
            if cycles > 15:
                self.say("  plan too long: {} cycles at {} ms for {} counts at {} rpm".format(cycles, interval,
                                                                                         target - pos, rpm))
            elif self.fits(node, args):
                break
            lo, hi = lo * 0.7, max(lo * 0.7, hi * 0.7)
        else:
            raise Retire("plan error: no position plan fits")
        rc, text, d, rec = self.motion(node, "position-" + (stop or "run"), args)
        last = re.findall(r"TRIAL_LAST passed=1 .*? 6041=([0-9a-f]{4}) .*? 6064=(-?\d+)", text)
        if not last:
            raise Retire("position: no final read")
        final_sw, final = last[-1][0], int(last[-1][1])
        if not stop and final_sw != "9437":
            raise Retire("position move not reported complete in its window ({} at {})".format(final_sw, final))
        if not stop:
            self.record(kind="arrival", node=node, route="position", target=target, final=final, error=final - target,
                        rpm=rpm, up=up, down=down, series=self.s["series"])
        return final, text, d

    def rearm(self, node, predecessor):
        rc, text, d, _ = self.motion(node, "rearm", ["rearm", "--predecessor", predecessor, "--cycles", "3",
                                                     "--interval-ms", "500"], seconds=60)
        last = re.findall(r"TRIAL_LAST passed=1 .*? 6064=(-?\d+)", text)
        return int(last[-1]) if last else None

    def move(self, node, pos, rpm, up, down, lo, hi, stop=None):
        # A move's completed sample is not always its rest position: the rotor settled 1 count further after
        # 3444 (-1864 then -1865), and the route's start check is exact. Start from a fresh read.
        fresh = self.read(node)
        if fresh and fresh["6064"] != pos:
            self.say("  start position from a fresh read: {} (planned from {})".format(fresh["6064"], pos))
            self.record(kind="settle", node=node, planned=pos, read=fresh["6064"], series=self.s["series"])
        pos = fresh["6064"] if fresh else pos
        for attempt in range(6):
            disp = self.pick_target(pos, rpm, up, down, lo, hi) - pos
            seconds, _ = self.window(disp, rpm, up, down, 1000)
            deadline = max(6000, int(seconds * 1000) + 3000)
            interval = 1000 if deadline <= 10000 else 1500 if deadline <= 16000 else 2000
            args = ["move", "--start-position", str(pos), "--target", str(disp), "--rpm", str(rpm), "--ramp-up-ms",
                    str(up), "--ramp-down-ms", str(down), "--deadline-ms", str(deadline), "--interval-ms",
                    str(interval), "--record", REC[node]] + MOVE_EVIDENCE + IDENT + ["--move-base", "actual"]
            if stop:
                args += ["--stop-kind", stop, "--stop-after", "2", "--stop-deadline-ms", "1500",
                         "--stop-best-effort-ms", "0", "--stop-interval-ms", "500"]
            else:
                args += ["--stop-kind", "none"]
            if self.fits(node, args):
                break
            lo, hi = lo * 0.7, max(lo * 0.7, hi * 0.7)
        else:
            raise Retire("plan error: no move plan fits")
        rc, text, d, rec = self.motion(node, "move-" + (stop or "run"), args, ok=(0, 3) if stop else (0,))
        final = rec.get("move_final")
        last = re.findall(r"TRIAL_LAST passed=1 .*? 6064=(-?\d+)", text)
        if final is None and last:
            final = int(last[-1])
        if final is None:
            raise Retire("move: no final position")
        if not stop:
            ok = re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=0 ", text)
            if not ok:
                raise Retire("move did not succeed")
            self.record(kind="arrival", node=node, route="move", target=pos + disp, final=final,
                        error=final - (pos + disp), rpm=rpm, up=up, down=down, series=self.s["series"])
        return final, text, d

    def quick(self, node, label="stop-quick"):
        rc, text, d, _ = self.motion(node, label, ["stop", "--kind", "quick", "--deadline-ms", "2000",
                                                   "--best-effort-ms", "0", "--interval-ms", "250"], ok=(0, 3),
                                     seconds=60)
        return text, d

    def velocity(self, node, rpm, kind, duration=12000):
        t = str(self.s["trial"] + 1)
        args = ["velocity", "--rpm", str(rpm), "--ramp-up-ms", "200", "--ramp-down-ms", "200", "--duration-ms",
                str(duration), "--interval-ms", "4000", "--deadline-ms", str(duration + 5000), "--kind", kind,
                "--record", t, "--layout", "zh86"] + IDENT
        if not self.fits(node, args):
            raise Retire("plan error: velocity plan does not fit")
        rc, text, d, rec = self.motion(node, "velocity-{}-{}".format(kind, rpm), args, ok=(0, 3), seconds=90)
        return rc, text, d

    # ---- series
    def begin(self, node, kind):
        self.s["series"] += 1
        self.rng = random.Random(self.s["rng"] + self.s["series"])
        self.sdir = NIGHT / "S{:04d}-n{}-{}".format(self.s["series"], node, kind)
        if not self.dry:
            self.sdir.mkdir(parents=True, exist_ok=False)
        self.ledger = self.sdir / "campaign.jsonl"
        self.newledger = True
        self.save()
        self.say("##### series {} node {} {} ({})".format(self.s["series"], node, kind, self.sdir.name))
        if not self.dry:
            subprocess.run([sys.executable, str(SCRATCH / "en_reset.py"), str(self.sdir / (stamp() + "-en-reset")),
                            "Night soak series {}: fresh MCU boot for node {} ({})".format(self.s["series"], node, kind)],
                           env={"PYTHONIOENCODING": "utf-8", **os.environ}, capture_output=True)
        self.run("start", ["--command", "start", "--output", self.out("start")])
        st = self.read(node)
        if not st:
            raise Retire("no readable state")
        if st["1001"] != "00" or st["603f"] != "0000" or int(st["6041"], 16) & 0x08:
            raise Retire("fault reported: 1001={} 603f={} 6041={}".format(st["1001"], st["603f"], st["6041"]))
        return st

    def series_position(self, node, variant):
        if not self.s["nodes"][str(node)].get("stoprec"):
            raise Retire("no valid start record for an enable")
        st = self.begin(node, "P" + variant)
        if st["6061"] not in ("01", "03") or not self.released(st):
            raise Retire("not released in mode 1 or 3")
        changed = st["6061"] == "03"
        if changed:
            self.motion(node, "mode1", ["mode1", "--cycles", "5", "--interval-ms", "500"], seconds=60)
            st = self.read(node)
            if not st or st["6061"] != "01" or not self.released(st):
                raise Retire("mode1 did not leave the node released in mode 1")
            self.s["nodes"][str(node)]["mode"] = "01"
            self.save()
        pos = self.enable(node, st)
        plan = []
        for _ in range(6):
            rpm = self.rng.choice(RPMS)
            plan.append((rpm, self.rng.choice(RAMPS), self.rng.choice(RAMPS)))
        self.say("  plan " + str(plan))
        r, u, d = plan[0]
        pos, _, _ = self.position(node, pos, r, u, d, 1.5, 8)
        pos = self.rearm(node, "position-trigger") or pos
        r, u, d = plan[1]
        pos, _, _ = self.move(node, pos, r, u, d, 1.5, 8)
        r, u, d = plan[2]
        pos, _, _ = self.position(node, pos, max(r, 20), u, d, 4, 8, stop="halt")
        pos = self.rearm(node, "position-halt") or pos
        st2 = self.read(node)
        pos = st2["6064"] if st2 else pos
        r, u, d = plan[3]
        pos, _, _ = self.move(node, pos, r, u, d, 1.5, 8)
        r, u, d = plan[4]
        if not changed:
            pos, _, _ = self.position(node, pos, r, u, d, 1.5, 8)
            pos = self.rearm(node, "position-trigger") or pos
        r, u, d = plan[5]
        if variant == "a":
            pos, text, dr = self.move(node, pos, max(r, 20), u, d, 5, 9, stop="quick")
            if not self.good_stop(dr, node):
                raise Retire("moving quick stop did not settle")
            stoprec = dr
        else:
            pos, _, _ = self.move(node, pos, r, u, d, 1.5, 8)
            text, dr = self.quick(node)
            if not self.good_stop(dr, node):
                raise Retire("stationary quick stop did not settle")
            stoprec = dr
        self.s["nodes"][str(node)]["stoprec"] = stoprec
        self.save()
        end = self.read(node)
        self.say("  end of series: {}".format(end))
        if not end or not self.released(end):
            raise Retire("not released at the end of the series")
        self.s["nodes"][str(node)]["mode"] = end["6061"]

    def series_velocity(self, node, halt):
        st = self.begin(node, "V" + ("h" if halt else "q"))
        if st["6061"] not in ("01", "03") or not self.released(st):
            raise Retire("not released in mode 1 or 3")
        rates = self.rng.choice(VRATES)
        self.say("  rates " + str(rates))
        if st["6061"] == "01":
            self.motion(node, "mode3", ["mode3", "--cycles", "2", "--interval-ms", "500"], seconds=60)
        else:
            self.say("  already released in mode 3: no mode3 route")
        stoprec = None
        if halt and not (int(st["2100"], 16) & 0x0001):
            self.say("  2100 in-place clear ({}): no HALT run from it (S0071), quick stops only".format(st["2100"]))
            halt = False
        if halt:
            rc, text, d = self.velocity(node, rates[0], "halt")
            if not re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=0 ", text):
                raise Retire("velocity HALT run failed")
            text, d = self.quick(node, "stop-quick-halted")
            if not self.good_stop(d, node):
                raise Retire("halted-state quick stop did not settle")
            stoprec = d
            rest = rates[1:2]  # The ledger's three stops per node: HALT, its quick stop, one quick-stop run.
        else:
            rest = rates
        for rpm in rest:
            st = self.read(node)
            if not st or not self.released(st) or st["6064"] is None:
                raise Retire("not released before the velocity run")
            rc, text, d = self.velocity(node, rpm, "quick")
            if not self.good_stop(d, node):
                raise Retire("velocity quick stop did not settle")
            stoprec = d
        self.s["nodes"][str(node)]["stoprec"] = stoprec
        self.save()
        end = self.read(node)
        self.say("  end of series: {}".format(end))
        if not end or not self.released(end):
            raise Retire("not released after the velocity series")
        self.s["nodes"][str(node)]["mode"] = end["6061"]

    def safe(self, node, reason):
        """After an unexpected result: read, stop if enabled, keep the node only if released with a record."""
        self.say("!!! node {}: {}".format(node, reason))
        self.record(kind="anomaly", node=node, reason=reason, series=self.s["series"])
        wrote = getattr(self, "last_admitted", True)
        renewed = False
        try:
            st = self.read(node)
            if st and self.enabled(st):
                text, d = self.quick(node, "stop-quick-safe")
                if self.good_stop(d, node):
                    self.s["nodes"][str(node)]["stoprec"] = d
                    renewed = True
                st = self.read(node)
        except Retire as error:
            self.say("!!! safe path: " + str(error))
            st = None
        nd = self.s["nodes"][str(node)]
        if wrote and not renewed:
            # The record no longer is the last thing that reached the drive: an enable may not start from it.
            nd["stoprec"] = None
            self.say("  start record withdrawn: the failing route may have reached the drive after it")
        nd["anomalies"] = nd.get("anomalies", 0) + 1
        if st:
            nd["mode"] = st["6061"]
        keep = st is not None and self.released(st) and st["6061"] in ("01", "03") and st["1001"] == "00" and \
            st["603f"] == "0000" and not reason.startswith(("runner alert", "fault reported", "STOP")) and \
            nd["anomalies"] < 6 and (nd["stoprec"] is None or self.good_stop(nd["stoprec"], node))
        if not keep:
            self.s["nodes"][str(node)]["retired"] = "{} ({})".format(reason, utc().isoformat(timespec="seconds"))
            self.say("!!! node {} retired for the night".format(node))
        self.save()

    def go(self, only=None):
        # Cv / Cm: console velocity and console move sessions on the bench image (conmo.py, shared console).
        rotation = ["Pa", "Cm", "Pb", "Vq", "Cv", "Pa", "Pb", "Vh"]  # Cm right after Pa: mode 1 with a start record.
        while utc() < self.end and not self.stopfile():
            active = [n for n in (1, 2) if not self.s["nodes"][str(n)]["retired"] and (only is None or n == only)]
            if not active:
                self.say("no active node left")
                break
            for node in (1, 2):
                nd = self.s["nodes"][str(node)]
                if nd["retired"] and (utc() - datetime.datetime.fromisoformat(
                        nd.get("watched", "2000-01-01T00:00:00+00:00"))).total_seconds() > 600:
                    nd["watched"] = utc().isoformat(timespec="seconds")
                    if not getattr(self, "sdir", None):
                        self.sdir = NIGHT / "watch"
                        self.sdir.mkdir(exist_ok=True)
                    st = self.read(node)
                    held = nd.setdefault("held", st and {"6064": st["6064"], "6041": st["6041"]})
                    if not st or st["606c"] != 0 or st["1001"] != "00" or st["603f"] != "0000" or \
                            st["6064"] != held["6064"] or st["6041"] != held["6041"]:
                        self.say("!!! ALERT retired node {} changed: {} (held {})".format(node, st, held))
                    else:
                        self.say("  retired node {} unchanged: 6041={} 6064={}".format(node, st["6041"], st["6064"]))
                    self.save()
            for node in active:
                if utc() >= self.end or self.stopfile():
                    break
                if (NIGHT / "PAUSE").exists():
                    self.say("PAUSE file present: soak paused between series")
                    return
                nd = self.s["nodes"][str(node)]
                kind = rotation[nd["rot"] % len(rotation)]
                if kind == "Cv" and nd.get("mode") != "03":
                    kind = "Vq"  # The console velocity needs a released mode 3 drive.
                elif kind == "Cm" and (nd.get("mode") != "01" or not nd.get("stoprec")):
                    kind = "Pa"  # The console move session needs mode 1 and a start record for its enable.
                if kind.startswith("P") and not nd.get("stoprec"):
                    kind = "Vq"  # A velocity run starts from a released drive and leaves a settled stop record.
                elif kind.startswith("V") and nd.get("mode") == "03" and nd.get("stoprec"):
                    kind = "Pa"  # Two velocity series in a row would never return to mode 1.
                if kind.startswith("C") and self.dry:
                    self.say("DRY: console session {} skipped".format(kind))
                    nd["rot"] += 1
                    continue
                if kind.startswith("C"):
                    self.save()
                    self.log.flush()
                    r = subprocess.run([sys.executable, str(SCRATCH / "conmo.py"), self.wt,
                                        "move" if kind == "Cm" else "velocity", str(node), "0"],
                                       env={"PYTHONIOENCODING": "utf-8", **os.environ}, capture_output=True, text=True,
                                       errors="replace")
                    self.s = json.loads(self.statefile.read_text(encoding="utf-8"))
                    nd = self.s["nodes"][str(node)]
                    self.record(kind="console", node=node, session=kind, rc=r.returncode, series=self.s["series"])
                    if r.returncode != 0:
                        nd["anomalies"] = nd.get("anomalies", 0) + 1
                        self.say("!!! node {}: console session {} ended with exit {}".format(node, kind, r.returncode))
                        if nd["anomalies"] >= 6:
                            nd["retired"] = "console session failures ({})".format(utc().isoformat(timespec="seconds"))
                            self.say("!!! node {} retired for the night".format(node))
                    nd["rot"] += 1
                    self.save()
                    continue
                try:
                    if kind.startswith("P"):
                        self.series_position(node, kind[1])
                    else:
                        self.series_velocity(node, kind == "Vh")
                    nd["rot"] += 1
                    self.save()
                except Retire as error:
                    if self.dry:
                        raise
                    self.safe(node, str(error))
                    nd["rot"] += 1
                    self.save()
            if self.dry and self.s["nodes"]["1"]["rot"] >= 8:
                break
        self.say("soak loop ended")


if __name__ == "__main__":
    wt = sys.argv[1]
    end = datetime.datetime.fromisoformat(sys.argv[2]).replace(tzinfo=datetime.timezone.utc)
    only = int(sys.argv[sys.argv.index("--only") + 1]) if "--only" in sys.argv else None
    Night(wt, end, "--dry" in sys.argv).go(only)
