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
# Microstepping grid (prompt 59 Part 3): rpm per 2201 value for pulse rates 100k..2M+ pulses/s, capped at 4500 rpm,
# just above the measured top speed at 2201=1000 (4000 clean, 4500 FF05 on node 1).
GRID = {4000: [1500, 3000, 4000, 4500],
        10000: [600, 1200, 1800, 3000, 4000, 4500],
        25600: [235, 469, 703, 1172, 1758, 2344, 3516, 4000, 4500],
        51200: [117, 234, 352, 586, 879, 1172, 1758, 2344, 3516, 4000, 4500]}


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
        sample = re.search(r"TRIAL_MOVE_SAMPLE which=1 .*?position=(-?\d+)", text)
        if sample:
            rec["move_final"] = int(sample.group(1))  # night.move reads the arrival from here
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

    def velocity_run(self, node, rpm, kind="quick", probe=None, ramp=None, reverse=False):
        up, down = (ramp, ramp) if ramp else ramps(rpm)
        probe = rpm > 60 if probe is None else probe
        # The night soak's settled pattern: 12 s at 4 s cycles, 5 s reserve (118 terminals). A quick stop at due needs
        # two still cycles after its first (trial 5112's first read 606C -1), which no plan fits in 128 terminals
        # once the duration is at least 11470 ms, so the ladder uses HALT at due and then the halted quick stop.
        duration, interval, reserve = 12000, 4000, 5000
        t = str(self.s["trial"] + 1)
        args = ["velocity", "--rpm", str(rpm), "--ramp-up-ms", str(up), "--ramp-down-ms", str(down),
                "--duration-ms", str(duration), "--interval-ms", str(interval), "--deadline-ms",
                str(duration + reserve), "--kind", kind, "--record", t, "--layout", "zh86"] + night.IDENT +             (["--range-probe"] if probe else []) + (["--reverse"] if reverse else [])
        if not self.fits(node, args):
            raise night.Retire("plan error: no velocity plan fits at %d rpm" % rpm)
        rc, text, d, rec = self.motion(node, "velocity-{}-{}".format(kind, rpm), args, ok=(0, 3, 4), seconds=90)
        emcy = re.search(r"TRIAL_EMCY data=([0-9a-f]+)", text)
        head = re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=(\d+) why=(\S+)", text)
        vline = re.search(r"TRIAL_VELOCITY own=.*", text)
        result = {"node": node, "rpm": -rpm if reverse else rpm, "stop_kind": kind, "probe": probe, "up": up,
                  "down": down, "reverse": reverse,
                  "subdivision": self.s["nodes"][str(node)]["subdivision"], "rc": rc, "dir": d,
                  "failure": head.group(1) if head else None, "why": head.group(2) if head else None,
                  "velocity": vline.group(0)[:300] if vline else None, "emcy": emcy.group(1) if emcy else None}
        c = cruise(text, up, kind == "quick")
        if c:
            result.update(c)
            if "cps" in c:
                result["rpm_measured"] = c["cps"] * 60.0 / result["subdivision"]
                # The ratio compares like signs: a reverse run must measure a negative rate.
                result["ratio"] = result["rpm_measured"] / (-rpm if reverse else rpm)
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
        """Recover a resettable alarm (measured 2026-11-13, node 1): the typed clear, alternating its two methods so
        both are measured, then, if the drive reads 6041 8010 (low bits 000, not released), one 6040 = 0x0080 word,
        which moved 8010 to the released 8050/0011 (trial 5405; 0000, 000B and 0006 did nothing, trials 5402-5404).
        Never an enable. Returns the state read after the recovery."""
        nd = self.s["nodes"][str(node)]
        method = "controlword"  # The best method, chosen 2026-11-13 (11 of 11 direct releases).
        nd["clears"] = nd.get("clears", 0) + 1
        self.save()
        rc, text, d, _ = self.motion(node, "clear-alarm-" + method,
                                     ["clear-alarm", "--deadline-ms", "3000", "--interval-ms", "500", "--method",
                                      method], ok=(0, 3), seconds=60)
        st = self.read(node)
        self.note(kind="clear-alarm", node=node, method=method, rc=rc, dir=d, state=st)
        self.say("  after the {} clear: {}".format(method, st))
        if st and not self.alarm_state(st) and not self.released(st) and not self.enabled(st) and                 (int(st["6041"], 16) & 0x07) == 0:
            rc2, text2, d2, _ = self.motion(node, "controlword-0080", ["controlword", "--word", "0x0080", "--cycles",
                                                                       "3", "--interval-ms", "500"], ok=(0, 3),
                                            seconds=60)
            st = self.read(node)
            self.note(kind="controlword-0080", node=node, rc=rc2, dir=d2, state=st)
            self.say("  after 6040=0x0080: {}".format(st))
        return st

    # ---- series
    def begin(self, node, kind):
        try:
            return super().begin(node, kind)
        except night.Retire as error:
            if not str(error).startswith("fault reported"):
                raise
            st = self.read(node)
            self.say("  fault at series start: typed alarm clear first ({})".format(st))
            st = self.clear_alarm(node)
            if not st or self.alarm_state(st) or not self.released(st):
                raise night.Retire("alarm not recovered to a release: {}".format(st))
            # A fresh boot: the clear's 6040 writes are this boot's controlword history, after which the runner
            # refuses a mode write.
            return super().begin(node, kind + "c")

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
        if st and not self.released(st) and not self.enabled(st) and not self.alarm_state(st) and \
                (int(st["6041"], 16) & 0x7f) == 0x10:
            # After the 2301 clear in mode 3 (trial 5323) the drive read 6041 8010, 2100 0001: alarm gone, low bits
            # 000, not the released 8050. The best-effort 000B released the similar power-up state (trials 503/506).
            self.say("  post-clear state {}: mode 1, best-effort quick stop, then a fresh series".format(st["6041"]))
            if st["6061"] != "01":
                # The stop route refused 8010 in mode 3 (trial 5327); from mode 1 the power-up state took 000B.
                self.motion(node, "mode1", ["mode1", "--cycles", "3", "--interval-ms", "500"], ok=(0, 3), seconds=60)
            self.emergency(node)
            st = self.begin(node, label + "c")
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

    def ladder(self, node, steps, last_good=None, sweep=False, stop_first=False, reverse=False):
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
                rc, text, d, res = self.velocity_run(node, rpm, kind=kind, ramp=opts[0], reverse=reverse)
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
                if bad and stop_first:
                    self.say("  {} rpm failed: stop at the first fault (each fault risks the enable-fault state)".format(rpm))
                    queue[:] = []
                if not bad:
                    last_good = rpm if last_good is None or rpm > last_good else last_good
                elif stop_first:
                    pass
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
                    if nd["alarms"] > 40:
                        raise night.Retire("more than forty alarms on this node: {}".format(code))
                    if code[1].lower() not in ("ff05", "0000"):
                        raise night.Retire("alarm {} not documented as resettable".format(code))
                    st = self.clear_alarm(node)
                    if not st or self.alarm_state(st) or not self.released(st):
                        raise night.Retire("alarm not recovered to a release: {}".format(st))
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

    def zero_step(self, node, st, label):
        """Counter zero in mode 1 (needs 2100 running clear), then a fresh boot."""
        if st["6061"] != "01":
            self.motion(node, "mode1", ["mode1", "--cycles", "3", "--interval-ms", "500"], seconds=60)
        self.motion(node, "counter-zero", ["counter-zero", "--deadline-ms", "3000", "--interval-ms", "500"],
                    ok=(0, 3), seconds=60)
        st = self.begin(node, label + "z")
        self.say("  after counter zero: 6064={}".format(st["6064"] if st else None))
        if not st or abs(st["6064"]) > 200000 or not self.released(st):
            raise night.Retire("counter zero did not bring 6064 near zero: {}".format(st))
        return st

    def position_series(self, node, label):
        """A series ready for position moves (night soak pattern; each mode or controlword step in its own MCU boot,
        because the runner refuses a mode write after this boot's controlword history). Steps by state, at most six:
        counter zero when 6064 is large and the running latch is clear (mode 1 needed; node 1 faulted at an enable with
        6064 near 183 million after the 51200 runs); a settled stop record from a 60 rpm HALT run and the halted quick
        stop when none is valid; when 6064 is large and latched, an enable in mode 1 (its word 7 clears the latch,
        trial 308) and a stationary quick stop. Then mode 1 and the released-by-recorded-stop enable."""
        nd = self.s["nodes"][str(node)]
        st = self.begin(node, label)
        latched = lambda x: bool(int(x["2100"], 16) & 0x0004)
        big = lambda x: abs(x["6064"]) > 200000
        for step in range(6):
            if not st or not self.released(st):
                raise night.Retire("not released at position series start: {}".format(st))
            valid = nd.get("stoprec") and self.good_stop(nd["stoprec"], node)
            if big(st) and not latched(st):
                if st["6061"] != "01":
                    self.motion(node, "mode1", ["mode1", "--cycles", "3", "--interval-ms", "500"], seconds=60)
                self.motion(node, "counter-zero", ["counter-zero", "--deadline-ms", "3000", "--interval-ms", "500"],
                            ok=(0, 3), seconds=60)
                st = self.begin(node, label + "z")
                self.say("  after counter zero: 6064={}".format(st["6064"] if st else None))
                if st and big(st):
                    raise night.Retire("counter zero did not bring 6064 near zero: {}".format(st))
            elif not valid:
                if st["6061"] != "03":
                    self.motion(node, "mode3", ["mode3", "--cycles", "2", "--interval-ms", "500"], seconds=60)
                    st = self.read(node)
                # HALT only with 2100 bit 0 set: the 0014 latch (bit 0 clear) left node 2 halted with no accepted
                # stop (S20); the 0015 latch is what the night soak's HALT series started from.
                if not (int(st["2100"], 16) & 0x0001):
                    raise night.Retire("no HALT start for a stop record from 2100={}".format(st["2100"]))
                self.velocity_run(node, 60, kind="halt", probe=False)
                st = self.read(node)
                if st and self.alarm_state(st):
                    raise night.Retire("alarm during the stop-record run: {}".format(st))
                text, d = self.quick(node, "stop-quick-halted")
                if not self.good_stop(d, node):
                    raise night.Retire("halted quick stop did not settle; no start record")
                nd["stoprec"] = d
                self.save()
                st = self.begin(node, label + "r")
            elif big(st) and latched(st):
                if st["6061"] != "01":
                    self.motion(node, "mode1", ["mode1", "--cycles", "3", "--interval-ms", "500"], seconds=60)
                    st = self.read(node)
                self.enable(node, st)
                text, d = self.quick(node, "stop-quick-stationary")
                if not self.good_stop(d, node):
                    raise night.Retire("stationary quick stop did not settle")
                nd["stoprec"] = d
                self.save()
                st = self.begin(node, label + "u")
                self.say("  after enable and stationary quick stop: 2100={} 6041={}".format(st["2100"], st["6041"]))
            else:
                break
        else:
            raise night.Retire("position series start did not converge: {}".format(st))
        if st["6061"] != "01":
            self.motion(node, "mode1", ["mode1", "--cycles", "3", "--interval-ms", "500"], seconds=60)
            st = self.read(node)
            if not st or st["6061"] != "01" or not self.released(st):
                raise night.Retire("mode 1 not reported released: {}".format(st))
        return self.enable(node, st)

    def position_ladder(self, node, rpms):
        """Position-mode speed ladder: finite moves to completion at each rpm (ramps 1000 ms below 1200 rpm, 2000 ms
        from 1200), about 2 s of travel at speed, alternating toward zero; three moves per series, then a stationary
        quick stop whose record starts the next series."""
        night.LIMIT = 900000
        queue = list(rpms)
        while queue:
            pos = self.position_series(node, "P{}".format(queue[0]))
            for _ in range(3):
                if not queue:
                    break
                if self.stopfile():
                    raise night.Retire("STOP file present")
                rpm = queue.pop(0)
                up, down = ramps(rpm)
                try:
                    # Travel long enough to cruise: half of each ramp plus 1.2 s at speed (prompt 59 plan).
                    cruise_s = (up + down) / 2000.0 + 1.2
                    final, text, d = self.move(node, pos, rpm, up, down, cruise_s, cruise_s + 0.2)
                except night.Retire as error:
                    self.note(kind="position", node=node, rpm=rpm, ok=False, reason=str(error))
                    self.say("  POSITION n{} {} rpm failed: {}".format(node, rpm, error))
                    st = self.read(node)
                    if self.alarm_state(st):
                        st = self.clear_alarm(node)
                    if st and self.enabled(st):
                        self.quick(node, "stop-quick-safe")
                    nd = self.s["nodes"][str(node)]
                    nd["stoprec"] = None
                    self.save()
                    break
                pts = [(int(m.group(2)), int.from_bytes(bytes.fromhex(m.group(1)), "little", signed=True)) for m in
                       re.finditer(r"object=6064:00 .*?outcome=0 .*?size=4 data=([0-9a-f]{8}) .*?completed=(\d+)", text)]
                speeds = [int.from_bytes(bytes.fromhex(m.group(1)), "little", signed=True) for m in
                          re.finditer(r"object=606c:00 .*?outcome=0 .*?size=4 data=([0-9a-f]{8})", text)]
                slopes = [abs(p2 - p1) * 1e6 / (t2 - t1) for (t1, p1), (t2, p2) in zip(pts, pts[1:]) if t2 > t1]
                peak = max(slopes) * 60.0 / self.s["nodes"][str(node)].get("subdivision", 1000) if slopes else None
                self.note(kind="position", node=node, rpm=rpm, ok=True, final=final, dir=d, peak_rpm=peak,
                          max_606c=max((abs(v) for v in speeds), default=None))
                self.say("  POSITION n{} {} rpm -> final {} peak slope {} rpm, max |606C| {}".format(
                    node, rpm, final, round(peak) if peak else None, max((abs(v) for v in speeds), default=None)))
                pos = final
            text, d = self.quick(node, "stop-quick")
            nd = self.s["nodes"][str(node)]
            nd["stoprec"] = d if self.good_stop(d, node) else None
            self.save()

    def grid(self, node, only=None):
        nd = self.s["nodes"][str(node)]
        for value, rpms in GRID.items():
            if only and value not in only:
                continue
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
    if (LADDER / "HOLD").exists() and plan.startswith(tuple((LADDER / "HOLD").read_text(encoding="utf-8").split())):
        print("plan {} held by HOLD".format(plan))
        sys.exit(0)
    lad = Ladder(wt, "--dry" in sys.argv)
    node = int(re.match(r"[a-z]+(\d)", plan).group(1))
    try:
        if plan.startswith("rev"):
            # rev1:6,30,60,... reverse velocity sweep (each step once) on the probe
            good = lad.ladder(node, plan.split(":")[1].split(","), None, sweep=True, reverse=True)
            lad.say("highest clean reverse step on node {}: {}".format(node, good))
        elif plan.startswith("sub"):
            # sub1:51200:3516[:2344][:sweep] - set 2201, run the steps (continuation rule from the given last clean
            # step, or a sweep), then restore 2201=1000 below.
            parts = plan.split(":")
            value, steps = int(parts[1]), parts[2].split(",")
            last = int(parts[3]) if len(parts) > 3 and parts[3].isdigit() else None
            lad.to_released_clear(node)
            ok, _ = lad.subdivision(node, value)
            if not ok:
                raise night.Retire("subdivision {} not confirmed".format(value))
            good = lad.ladder(node, steps, last, sweep="sweep" in parts, stop_first="first" in parts)
            lad.say("highest clean step on node {} at 2201={}: {}".format(node, value, good))
        elif plan.startswith("pos"):
            # pos1:300,600,... position-mode ladder on node 1
            lad.position_ladder(node, [int(x) for x in plan.split(":")[1].split(",")])
        elif plan.startswith("grid"):
            # grid1 or grid1:51200,25600 (only those 2201 values)
            lad.grid(node, [int(v) for v in plan.split(":")[1].split(",")] if ":" in plan else None)
        else:
            steps = plan.split(":")[1].split(",") if ":" in plan else PART1 + PART2
            good = lad.ladder(node, steps, int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3].isdigit() else None,
                              sweep=plan.startswith("s") and not plan.startswith("sub"))
            lad.say("highest clean step on node {}: {}".format(node, good))
    except night.Retire as error:
        lad.say("!!! " + str(error))
        lad.safe(node, str(error))
    if lad.s["nodes"][str(node)].get("subdivision", 1000) != 1000:
        # The grid ended early: put 2201 back to the measured 1000 (volatile, no save) if the drive allows it.
        try:
            lad.to_released_clear(node)
            ok, _ = lad.subdivision(node, 1000)
            lad.say("  restore 2201=1000 after an early end: {}".format(ok))
        except night.Retire as error:
            lad.say("!!! restore of 2201 failed: {} (2201 stays {} until a power cycle)".format(
                error, lad.s["nodes"][str(node)].get("subdivision")))
    lad.say("ladder {} ended".format(plan))
