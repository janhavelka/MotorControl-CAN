"""Prompt 59 velocity ladder on powered drives (2026-11-13), under the user's written waiver for this campaign.

Usage: python ladder.py WORKTREE PLAN [--dry]
  PLAN: "n1", "n2" (Part 1 + Part 2 on that node) or "grid1" (Part 3 subdivision grid on node 1).

Built on the night soak's driver (docs/bench/20261111-night/tools/night.py): one series = one MCU boot (EN reset)
= one node = one campaign ledger (12 motion admissions, 3 per route and node). Each series: read; a stationary
quick stop if the drive is in its power-up state; mode 3 if needed; then at most three velocity runs (each run
asks for its own quick stop at due, which counts against the stop route). Continuation rule (prompt 59): a
failing step (route failure, alarm, or cruise below 90 % of the command) is repeated once; if it fails again,
the boundary is bisected to max(100 rpm, 5 %), then the climb continues through the remaining steps. A resettable
alarm is cleared with the typed clear-alarm route as an explicit, logged step (never an enable). The same alarm
three times in a row, a non-resettable alarm or any runner alert retires the node. Never replays an uncertain
write. A file named STOP in the ladder directory ends after the current step.
"""
import json
import os
import pathlib
import re
import sys

REPO = pathlib.Path(r"C:\Users\Honza\Documents\Projects\MotorControl-CAN")
LADDER = REPO / "docs" / "bench" / "20261113-ladder"
os.environ["NIGHT_DIR"] = str(LADDER)
sys.path.insert(0, str(REPO / "docs" / "bench" / "20261111-night" / "tools"))
import night  # noqa: E402

WAIVER = ("Unattended, I accept it - Waiver for this ladder campaign only: run the velocity ladder, above-limit probe "
          "and resolution grid without me at the bench. I take responsibility; motors bolted, free shafts. "
          "(user's answer, 2026-11-13, to: How should I run it?)")
night.UNATT = ["--unattended-waiver", WAIVER, "--waiver-covers-motion"]
night.KEYS = night.KEYS + ("TRIAL_SUBDIVISION", "TRIAL_DRIVER", "TRIAL_SETTINGS", "TRIAL_PLAN")

PART1 = [60, 120, 240, 480, 720, 960, 1200, 1500, 1800, 2100, 2400, 2700, 3000]
PART2 = [3001, 3300, 3600, 4000, 4500, 5000, 6000]
GRID = {4000: [1500, 3000, 4500, 6000],
        10000: [600, 1200, 1800, 3000, 4500, 6000],
        25600: [235, 469, 703, 1172, 1758, 2344, 3516, 4688, 6000],
        51200: [117, 234, 352, 586, 879, 1172, 1758, 2344, 3516, 4688, 6000]}


def ramps(rpm):
    return (1000, 1000) if rpm < 1200 else (2000, 2000)


def le32(hexdata):
    return int.from_bytes(bytes.fromhex(hexdata), "little", signed=True)


def cruise(text, up_ms, through_stop=False):
    """Cruise rate from the record's own 6064/606C uploads: counts/s from the first sample taken at least the ramp
    plus 300 ms after word 15's acknowledgement to the first 6064 upload after the stop was asked for (the quick stop
    reached standstill within about 7 ms at 60 rpm, trial 5112), or to the last running sample if that is later."""
    ack = re.search(r"TRIAL_VELOCITY_WINDOW .*?ack=(\d+)", text)
    timing = re.search(r"TRIAL_VELOCITY_TIME .*?stopAt=(\d+)", text)
    pos = [(int(m.group(2)), le32(m.group(1))) for m in
           re.finditer(r"object=6064:00 .*?outcome=0 .*?size=4 data=([0-9a-f]{8}) .*?completed=(\d+)", text)]
    vel = [(int(m.group(2)), le32(m.group(1))) for m in
           re.finditer(r"object=606c:00 .*?outcome=0 .*?size=4 data=([0-9a-f]{8}) .*?completed=(\d+)", text)]
    if not timing:
        return None
    start = (int(ack.group(1)) if ack else (pos[0][0] if pos else 0)) + (up_ms + 300) * 1000
    stop = int(timing.group(1))
    window = [p for p in pos if start <= p[0] <= stop]
    after = [p for p in pos if p[0] > stop]
    speeds = [v for t, v in vel if start <= t <= stop]
    stop_speeds = [v for t, v in vel if t > stop]
    out = {"samples": len(window), "606c": speeds, "606c_after_stop": stop_speeds}
    if after:
        out["stop_travel"] = after[-1][1] - after[0][1]
    points = window + (after[:1] if through_stop else [])
    if len(points) < 2:
        return out
    (t1, p1), (t2, p2) = points[0], points[-1]
    out.update({"cps": (p2 - p1) * 1e6 / (t2 - t1), "span_s": (t2 - t1) / 1e6, "p_last": points[-1][1],
                "to_stop": bool(after)})
    return out


class Ladder(night.Night):
    def __init__(self, wt, dry):
        super().__init__(wt, None, dry)
        if self.s["trial"] == 3100:  # A fresh state file: this campaign's own numbering and nodes.
            self.s = {"trial": 5100, "series": 0, "rng": 20261113,
                      "nodes": {str(n): {"stoprec": None, "retired": None, "rot": 0, "subdivision": 1000,
                                         "alarms": 0} for n in (1, 2)}}
            self.save()
        self.results_path = LADDER / "ladder.jsonl"

    def note(self, **values):
        values["utc"] = night.utc().isoformat(timespec="seconds")
        with open(self.results_path, "a", encoding="utf-8") as f:
            f.write(json.dumps(values) + "\n")

    # ---- routes
    def motion(self, node, label, args, ok=(0,), seconds=90):
        """night.motion, except that a runner alert after a drive fault (why=emcy) whose drive then reads released
        (not enabled) with 606C 0 is returned for the ladder to record and clear, after the emergency stop."""
        if self.stopfile():
            raise night.Retire("STOP file present")
        t = self.next()
        directory = self.out("{}-n{}-{}".format(label, node, t))
        self.last_admitted = True
        rc, text = self.run("{} {} n{}".format(t, label, node),
                            ["--command", args[0], "--node", str(node), "--trial", t] + args[1:] + night.UNATT +
                            self.camp() + ["--seconds", str(seconds), "--output", directory])
        self.last_admitted = rc != 2
        self.release(t)
        head = re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=(\d+) why=(\S+)", text)
        rec = {"series": self.s["series"], "node": node, "trial": int(t), "route": label, "args": args, "rc": rc,
               "failure": head.group(1) if head else None, "why": head.group(2) if head else None, "dir": directory}
        self.record(**rec)
        if rc == 4:
            self.say("!!! ALERT on node {}: release and quick stop now".format(node))
            self.emergency(node)
            st = self.read(node)
            if head and head.group(2) == "emcy" and st and not self.enabled(st) and st["606c"] == 0:
                self.say("  drive fault left node {} released and still: recorded as a boundary".format(node))
                self.note(kind="fault-stop", node=node, trial=int(t), state=st, dir=directory)
                return rc, text, directory, rec
            raise night.Retire("runner alert after {}".format(label))
        if rc not in ok:
            raise night.Retire("unexpected exit {} after {} {}".format(rc, t, label))
        return rc, text, directory, rec

    def velocity_run(self, node, rpm, kind="quick", probe=None, ramp=None):
        up, down = (ramp, ramp) if ramp else ramps(rpm)
        probe = rpm > 60 if probe is None else probe
        # The night soak's settled pattern: 12 s at 4 s cycles, 5 s reserve (118 terminals). A quick stop at due needs
        # two still cycles after its first (trial 5112's first read 606C -1), which no plan fits in 128 terminals
        # once the duration is at least 11470 ms, so the ladder uses HALT at due and then the halted quick stop.
        duration, interval, reserve = 12000, 4000, 5000
        t = str(self.s["trial"] + 1)
        args = ["velocity", "--rpm", str(rpm), "--ramp-up-ms", str(up), "--ramp-down-ms", str(down),
                "--duration-ms", str(duration), "--interval-ms", str(interval), "--deadline-ms",
                str(duration + reserve), "--kind", kind, "--record", t, "--layout", "zh86"] + night.IDENT +             (["--range-probe"] if probe else [])
        if not self.fits(node, args):
            raise night.Retire("plan error: no velocity plan fits at %d rpm" % rpm)
        rc, text, d, rec = self.motion(node, "velocity-{}-{}".format(kind, rpm), args, ok=(0, 3, 4), seconds=90)
        emcy = re.search(r"TRIAL_EMCY data=([0-9a-f]+)", text)
        head = re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=(\d+) why=(\S+)", text)
        vline = re.search(r"TRIAL_VELOCITY own=.*", text)
        result = {"node": node, "rpm": rpm, "stop_kind": kind, "probe": probe, "up": up, "down": down,
                  "subdivision": self.s["nodes"][str(node)]["subdivision"], "rc": rc, "dir": d,
                  "failure": head.group(1) if head else None, "why": head.group(2) if head else None,
                  "velocity": vline.group(0)[:300] if vline else None, "emcy": emcy.group(1) if emcy else None}
        c = cruise(text, up, kind == "quick")
        if c:
            result.update(c)
            if "cps" in c:
                result["rpm_measured"] = c["cps"] * 60.0 / result["subdivision"]
                result["ratio"] = result["rpm_measured"] / rpm
                result["pulse_rate"] = rpm * result["subdivision"] / 60.0
        self.note(kind="velocity", **result)
        self.say("  RESULT n{} {} rpm s={} -> failure={} why={} measured={} ratio={} 606c={}".format(
            node, rpm, result["subdivision"], result["failure"], result["why"],
            round(result.get("rpm_measured", -1), 1), round(result.get("ratio", -1), 4), c.get("606c") if c else None))
        return rc, text, d, result

    def subdivision(self, node, value):
        t = self.next()
        directory = self.out("subdivision-{}-n{}-{}".format(value, node, t))
        rc, text = self.run("{} subdivision {} n{}".format(t, value, node),
                            ["--command", "subdivision", "--node", str(node), "--trial", t, "--value", str(value)] +
                            night.UNATT + self.camp() + ["--seconds", "40", "--output", directory])
        self.release(t)
        drv = re.findall(r"TRIAL_DRIVER which=(\d) present=1 2200=([0-9a-f]{4}) 2201=(\d+)", text)
        sub = re.search(r"TRIAL_SUBDIVISION .*", text)
        ok = rc == 0 and any(w == "2" and int(v) == value for w, _, v in drv)
        self.note(kind="subdivision", node=node, value=value, rc=rc, driver=drv, line=sub.group(0) if sub else None,
                  ok=ok, dir=directory)
        if ok:
            self.s["nodes"][str(node)]["subdivision"] = value
            self.save()
        return ok, text

    def clear_alarm(self, node):
        rc, text, d, _ = self.motion(node, "clear-alarm", ["clear-alarm", "--deadline-ms", "3000", "--interval-ms",
                                                           "500"], ok=(0, 3), seconds=60)
        self.note(kind="clear-alarm", node=node, rc=rc, dir=d)
        return rc

    # ---- series
    def begin(self, node, kind):
        try:
            return super().begin(node, kind)
        except night.Retire as error:
            if not str(error).startswith("fault reported"):
                raise
            st = self.read(node)
            self.say("  fault at series start: typed alarm clear first ({})".format(st))
            self.clear_alarm(node)
            st = self.read(node)
            if not st or self.alarm_state(st):
                raise night.Retire("alarm did not clear: {}".format(st))
            return st

    def begin_series(self, node, label):
        st = self.begin(node, label)
        self.stops = 0
        if self.enabled(st) and st["2100"] == "0001" and st["6061"] in ("00", "01"):
            # Trials 502/503/506 (2026-11-04): from the power-up state, mode 1, then 000B on the best-effort path
            # (no acknowledged controlword in the session); after an MCU reset the drive read 2100 0011, 6041 8050.
            self.say("  power-up state: mode 1, best-effort quick stop, then a fresh series")
            if st["6061"] == "00":
                self.motion(node, "mode1", ["mode1", "--cycles", "5", "--interval-ms", "500"], seconds=60)
            self.emergency(node)
            st = self.begin(node, label + "b")
            if self.enabled(st):
                raise night.Retire("power-up state not released by the best-effort quick stop: {}".format(st))
        if not st or not self.released(st):
            raise night.Retire("not released at series start: {}".format(st))
        if st["6061"] != "03":
            self.motion(node, "mode3", ["mode3", "--cycles", "2", "--interval-ms", "500"], seconds=60)
            st = self.read(node)
            if not st or st["6061"] != "03" or not self.released(st):
                raise night.Retire("mode 3 not reported released")
        return st

    def alarm_state(self, st):
        return st and (st["1001"] != "00" or st["603f"] != "0000" or (int(st["2100"], 16) & 0x0008))

    def ladder(self, node, steps, last_good=None, sweep=False):
        """Run steps (rpm list) with the continuation rule, three velocity runs per series."""
        nd = self.s["nodes"][str(node)]
        # A step is RPM, RPMxRAMP_MS (both ramps) or either with a trailing q (quick stop at due, even from a plain
        # release). Ramps below 200 ms are refused by the library's range probe above 60 rpm.
        queue = []
        for item in steps:
            text = str(item)
            quick = text.endswith("q")
            rpm, _, ramp = text.rstrip("q").partition("x")
            queue.append((int(rpm), "step", (int(ramp) if ramp else None, quick)))
        self.boundary_done = sweep  # A sweep records each step once: no repeat, no bisection.
        alarms_in_row, last_alarm = 0, None
        while queue:
            st = self.begin_series(node, "L{}".format(queue[0][0]))
            runs = 0
            while queue and runs < 1:
                if self.stopfile():
                    raise night.Retire("STOP file present")
                rpm, why, opts = queue.pop(0)
                st = self.read(node)
                if not st or not self.released(st):
                    raise night.Retire("not released before {} rpm".format(rpm))
                # HALT only from a release with 2100 bit 0 (in place) and without the running latch (S0071, S20);
                # otherwise a quick stop at due, whose settlement may run out of reserve (DEADLINE) after a clean run.
                word = int(st["2100"], 16)
                kind = "halt" if (word & 0x0001) and not (word & 0x0004) and not opts[1] else "quick"
                rc, text, d, res = self.velocity_run(node, rpm, kind=kind, ramp=opts[0])
                runs += 1
                st = self.read(node)
                if st and self.enabled(st):
                    self.quick(node, "stop-quick-halted")
                    st = self.read(node)
                vm = re.search(r"first=(\d+) .*?running=(\d)", res.get("velocity") or "")
                first, running = (int(vm.group(1)), vm.group(2) == "1") if vm else (None, False)
                res["first"], res["running"] = first, running
                # A clean run: running observed, no run failure other than an unconfirmed stop (DEADLINE, 6), and
                # the cruise at least 90 % of the command. OBSERVATION_SCOPE (stall, over-speed, contradicted
                # running), DRIVE_FAULT and the rest are boundary findings.
                bad = not running or first not in (0, 6) or res.get("ratio", 0) < 0.9
                self.note(kind="classified", node=node, rpm=rpm, first=first, running=running, bad=bad,
                          ratio=res.get("ratio"))
                if not bad:
                    last_good = rpm if last_good is None or rpm > last_good else last_good
                elif rpm == 1:
                    self.say("  1 rpm once: not repeated")
                elif why == "step" and self.boundary_done:
                    self.say("  step {} rpm failed above the bisected boundary: recorded, no repeat".format(rpm))
                elif why == "step":
                    self.say("  step {} rpm failed: repeat once".format(rpm))
                    queue.insert(0, (rpm, "repeat", opts))
                elif why == "repeat" and last_good is not None and rpm - last_good > max(100, rpm * 0.05):
                    mid = (last_good + rpm) // 2
                    self.say("  {} rpm failed twice: bisect at {} (last good {})".format(rpm, mid, last_good))
                    queue.insert(0, (mid, "bisect-{}".format(rpm), opts))
                elif why.startswith("bisect-"):
                    # This midpoint failed: it is the new upper bound.
                    if last_good is not None and rpm - last_good > max(100, rpm * 0.05):
                        queue.insert(0, ((last_good + rpm) // 2, "bisect-{}".format(rpm), opts))
                if not bad and why.startswith("bisect-"):
                    upper = int(why.split("-")[1])
                    if upper - rpm > max(100, upper * 0.05):
                        queue.insert(0, ((rpm + upper) // 2, why, opts))
                if why != "step" and not any(w != "step" for _, w, _o in queue):
                    self.boundary_done = True
                if self.alarm_state(st):
                    code = (st["1001"], st["603f"])
                    alarms_in_row = alarms_in_row + 1 if code == last_alarm else 1
                    last_alarm = code
                    self.say("  ALARM n{} at {} rpm: 1001={} 603f={} 2100={}".format(node, rpm, *code, st["2100"]))
                    self.note(kind="alarm", node=node, rpm=rpm, state=st)
                    nd["alarms"] = nd.get("alarms", 0) + 1
                    self.save()
                    if nd["alarms"] > 10:
                        raise night.Retire("more than ten alarms on this node: {}".format(code))
                    if code[1].lower() not in ("ff05", "0000"):
                        raise night.Retire("alarm {} not documented as resettable".format(code))
                    self.clear_alarm(node)
                    st = self.read(node)
                    if self.alarm_state(st):
                        raise night.Retire("alarm did not clear")
                    break  # A fresh series after the clear.
                else:
                    alarms_in_row, last_alarm = 0, None
                if st and self.enabled(st):
                    self.quick(node, "stop-quick-safe")
                    raise night.Retire("left enabled after {} rpm".format(rpm))
            nd["rot"] += 1
            self.save()
        return last_good

    def to_released_clear(self, node):
        """2201 needs 2100 released without the running latch: after a moving quick stop (0015), a HALT run at
        60 rpm and the halted-state quick stop leave it (night soak Vh pattern)."""
        st = self.begin_series(node, "S")
        if int(st["2100"], 16) & 0x0004:
            if not (int(st["2100"], 16) & 0x0001):
                raise night.Retire("running latch with in-place clear: no HALT run from it (S0071)")
            self.velocity_run(node, 60, kind="halt", probe=False)
            self.quick(node, "stop-quick-halted")
            st = self.read(node)
        return st

    def grid(self, node):
        nd = self.s["nodes"][str(node)]
        for value, rpms in GRID.items():
            st = self.to_released_clear(node)
            ok, _ = self.subdivision(node, value)
            if not ok:
                raise night.Retire("subdivision {} not confirmed".format(value))
            # Activation: the same 60 rpm with the new scale; counts/s equal value if it applies at the enable.
            st = self.begin_series(node, "A{}".format(value))
            rc, text, d, res = self.velocity_run(node, 60, probe=False)
            active = res.get("cps") and abs(res["cps"] / value - 0.992) < 0.1
            self.note(kind="activation", node=node, value=value, cps=res.get("cps"), active=bool(active))
            self.say("  ACTIVATION 2201={}: {} counts/s at 60 rpm -> {}".format(value, res.get("cps"),
                                                                              "active" if active else "NOT active"))
            if not active:
                self.say("  2201 did not take effect at the enable; grid stops here (needs a power cycle?)")
                break
            self.ladder(node, rpms)
        st = self.to_released_clear(node)
        ok, _ = self.subdivision(node, 1000)
        self.say("  restored 2201=1000: {}".format(ok))


if __name__ == "__main__":
    wt, plan = sys.argv[1], sys.argv[2]
    lad = Ladder(wt, "--dry" in sys.argv)
    node = int(plan[1] if plan[0] in "ns" else plan[-1])
    try:
        if plan.startswith("grid"):
            lad.grid(node)
        else:
            steps = plan.split(":")[1].split(",") if ":" in plan else PART1 + PART2
            good = lad.ladder(node, steps, int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3].isdigit() else None,
                              sweep=plan.startswith("s"))
            lad.say("highest clean step on node {}: {}".format(node, good))
    except night.Retire as error:
        lad.say("!!! " + str(error))
        lad.safe(node, str(error))
    lad.say("ladder {} ended".format(plan))
