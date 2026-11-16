"""Enable-fault investigation steps on one node (2026-11-13). Usage: python stuck.py WT NODE STEP...
Steps: provoke:RPM (velocity at RPM, expect FF05, then controlword clear), check (fresh boot + 60 rpm enable check),
clear (fresh boot + controlword clear if an alarm is present), state (fresh boot + read)."""
import importlib.util, re, sys
spec = importlib.util.spec_from_file_location("hil", r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad\hil.py")
m = importlib.util.module_from_spec(spec); argv = sys.argv; sys.argv = ["x"]; spec.loader.exec_module(m); sys.argv = argv
lad = m.Ladder(argv[1], False)
node = int(argv[2])
night = m.night

def boot(label):
    return lad.begin(node, label)   # a fault at series start takes the controlword clear and a fresh boot

for step in argv[3:]:
    try:
        if step.startswith("provoke:"):
            rpm = int(step.split(":")[1])
            st = boot("provoke{}".format(rpm))
            if st["6061"] != "03":
                lad.motion(node, "mode3", ["mode3", "--cycles", "2", "--interval-ms", "500"], seconds=60)
            rc, text, d, res = lad.velocity_run(node, rpm, kind="quick")
            st = lad.read(node)
            lad.say("  INVEST provoke {}: emcy={} state={}".format(rpm, res.get("emcy"), st))
            if st and lad.alarm_state(st):
                st = lad.clear_alarm(node)
                lad.say("  INVEST after clear: {}".format(st))
        elif step == "check":
            st = boot("check")
            if not lad.released(st):
                lad.say("  INVEST check: not released {}".format(st)); continue
            word = int(st["2100"], 16)
            kind = "halt" if (word & 0x0001) and not (word & 0x0004) else "quick"
            rc, text, d, res = lad.velocity_run(node, 60, kind=kind, probe=False)
            v7 = re.search(r"TRIAL_VELOCITY_SAMPLE which=1 .*?status=(\w+) .*?velocity=(-?\d+)", text)
            st = lad.read(node)
            stuck = bool(st and lad.alarm_state(st))
            lad.say("  INVEST check: failure={} why={} stuck={} sample={} state={}".format(
                res.get("failure"), res.get("why"), stuck, v7.groups() if v7 else None, st))
            lad.note(kind="invest-check", node=node, stuck=stuck, failure=res.get("failure"), dir=d, state=st)
            if st and lad.enabled(st):
                lad.quick(node, "stop-quick-halted")
        elif step == "clear":
            st = boot("clear")
            lad.say("  INVEST clear: {}".format(st))
        elif step.startswith("loop:"):
            # One typed 2500 write (route loop, protocol 13) in a fresh boot, released and alarm-free.
            value = step.split(":")[1]
            st = lad.begin_series(node, "loop{}".format(value))  # releases an enabled or power-up report first
            t = lad.next()
            d = lad.out("loop-{}-n{}-{}".format(value, node, t))
            rc, text = lad.run("{} loop {} n{}".format(t, value, node),
                               ["--command", "loop", "--node", str(node), "--trial", t, "--value", value] +
                               night.UNATT + lad.camp() + ["--seconds", "40", "--output", d])
            lad.release(t)
            grp = re.findall(r"TRIAL_CONTROL_GROUP which=(\d) present=1 2500=(\d+) 2501=(\d+)", text)
            line = re.search(r"TRIAL_LOOP .*", text)
            head = re.search(r"TRIAL id=\d+ node=\d route=\S+ failure=(\d+) why=(\S+)", text)
            st = lad.read(node)
            lad.say("  INVEST loop {}: rc={} head={} groups={} {} state={}".format(
                value, rc, head.groups() if head else None, grp, line.group(0) if line else None, st))
            lad.note(kind="invest-loop", node=node, value=value, rc=rc, groups=grp, dir=d, state=st)
        elif step == "release":
            # Fresh boot, then the best-effort 000B (night emergency) when the drive is not released.
            st = boot("release")
            if st and not lad.released(st):
                lad.emergency(node)
            st = boot("release-check")
            lad.say("  INVEST release: {}".format(st))
        elif step.startswith("disable"):
            # One 6040 0x0000 (disable voltage) on a switched-on stationary drive (route T, 0db0c1f), fresh boot.
            st = boot("disable")
            rc, text, d, res = lad.motion(node, "controlword-0000", ["controlword", "--word", "0x0000", "--cycles",
                                                                   "5", "--interval-ms", "200"], ok=(0, 3, 4))
            cyc = re.findall(r"TRIAL_CONTROLWORD_CYCLE which=(\d+) .*? 2100=(\w+) 6041=(\w+) 6061=\w+ 6064=(-?\d+) "
                             r"606c=(-?\d+)", text)
            line = re.search(r"TRIAL_CONTROLWORD .*", text)
            st = lad.read(node)
            lad.say("  INVEST disable: rc={} cycles={} {} state={}".format(rc, cyc, line.group(0) if line else None, st))
            lad.note(kind="invest-disable", node=node, rc=rc, cycles=cyc, dir=d, state=st)
        elif step.startswith("target:"):
            # One 60FF write and readback (route velocity-target, no enable) on a released drive, fresh boot.
            value = step.split(":")[1]
            st = boot("target{}".format(value))
            t = lad.next()
            d = lad.out("target-{}-n{}-{}".format(value, node, t))
            rc, text = lad.run("{} target {} n{}".format(t, value, node),
                               ["--command", "velocity-target", "--node", str(node), "--trial", t, "--value", value] +
                               night.UNATT + lad.camp() + ["--seconds", "40", "--output", d])
            lad.release(t)
            line = re.search(r"TRIAL_VELOCITY_TARGET .*", text)
            st = lad.read(node)
            lad.say("  INVEST target {}: rc={} {} state={}".format(value, rc, line.group(0)[:200] if line else None, st))
            lad.note(kind="invest-target", node=node, value=value, rc=rc, dir=d, state=st)
        elif step.startswith("sub:"):
            # One 2201 write with the record's before, readback and after reads (route subdivision), fresh boot.
            value = int(step.split(":")[1])
            st = lad.begin_series(node, "sub{}".format(value))  # releases a power-up state first
            ok, text = lad.subdivision(node, value)
            drv = re.findall(r"TRIAL_DRIVER which=(\d) present=(\d) 2200=([0-9a-f]{4}) 2201=(\d+)", text)
            st = lad.read(node)
            lad.say("  INVEST sub {}: ok={} driver={} state={}".format(value, ok, drv, st))
        elif step.startswith("nmt:"):
            # M14 (prompt 54 item 5): route velocity-nmt, a 6..30 rpm quick-stop run with one addressed NMT Stop and
            # then Pre-operational inside a running gap; the run's own stop follows. Released mode 3 start.
            parts = step.split(":")
            rpm = parts[1]
            dur, dl = (parts[2], parts[3]) if len(parts) > 3 else ("13000", "24000")
            st = lad.begin_series(node, "nmt{}".format(rpm))
            t = str(lad.s["trial"] + 1)
            args = ["velocity-nmt", "--rpm", rpm, "--ramp-up-ms", "200", "--ramp-down-ms", "200", "--duration-ms",
                    dur, "--interval-ms", "5000", "--deadline-ms", dl, "--kind", "quick", "--record", t,
                    "--layout", "zh86"] + night.IDENT
            rc, text, d, rec = lad.motion(node, "velocity-nmt-{}".format(rpm), args, ok=(0, 3), seconds=90)
            nmt = re.findall(r"TRIAL_NMT[A-Z_]* .*", text)
            st = lad.read(node)
            lad.say("  INVEST nmt {}: rc={} head={} {} state={}".format(rpm, rc, (rec.get("failure"), rec.get("why")),
                                                                    [x[:200] for x in nmt[:4]], st))
            lad.note(kind="invest-nmt", node=node, rpm=rpm, rc=rc, nmt=nmt, dir=d, state=st)
            if st and lad.enabled(st):
                lad.quick(node, "stop-quick-halted")
        elif step.startswith("vchange:"):
            # Prompt 52/54 velocity change (route velocity-change, Tv): RPM, NEW rpm, change after AFTER ms, ramps
            # RAMP ms, duration DUR ms, interval IV ms. Released mode 3 start; HALT only from 2100 bit 0 set.
            _, rpm, new, after, ramp, dur, iv = step.split(":")
            st = lad.begin_series(node, "vc{}to{}".format(rpm, new))
            word = int(st["2100"], 16)
            kind = "halt" if (word & 0x0001) and not (word & 0x0004) else "quick"
            t = str(lad.s["trial"] + 1)
            args = ["velocity-change", "--rpm", rpm, "--ramp-up-ms", ramp, "--ramp-down-ms", ramp, "--duration-ms", dur,
                    "--interval-ms", iv, "--deadline-ms", str(int(dur) + 8000), "--kind", kind, "--record", t,
                    "--layout", "zh86"] + night.IDENT + ["--change-rpm", new, "--change-after-ms", after]
            rc, text, d, rec = lad.motion(node, "velocity-change-{}-{}".format(rpm, new), args, ok=(0, 3), seconds=90)
            lines = re.findall(r"TRIAL_VELOCITY(?:_CHANGE)?[A-Z_]* .*", text)
            samples = re.findall(r"TRIAL_VELOCITY_SAMPLE which=(\d+) .*?velocity=(-?\d+)", text)
            st = lad.read(node)
            lad.say("  INVEST vchange {}->{} after {} ramp {} kind {}: rc={} head={} samples={} state={}".format(
                rpm, new, after, ramp, kind, rc, (rec.get("failure"), rec.get("why")), samples, st))
            lad.note(kind="invest-vchange", node=node, rpm=rpm, new=new, after=after, ramp=ramp, stop_kind=kind, rc=rc,
                     lines=[x[:400] for x in lines], samples=samples, dir=d, state=st)
            if st and lad.enabled(st):
                lad.quick(node, "stop-quick-halted")
        elif step.startswith("pchange:"):
            # Prompt 52/54 position change (route position-change, Tp), after mode 1 and the enable in this boot:
            # CASE, RPM, RAMP ms, change VALUE, travel DIST counts, replacement REPL counts beyond the target (cases
            # 6..8, else 0), change after cycle AC, DUR ms, CYCLES, interval IV ms, STOPKIND.
            _, case, rpm, ramp, value, dist, repl, ac, dur, cyc, iv, sk = step.split(":")
            pos = lad.position_series(node, "pc{}".format(case))
            target = pos + int(dist)
            replacement = target + int(repl) if int(repl) else 0
            t = str(lad.s["trial"] + 1)
            args = ["position-change", "--start-position", str(pos), "--target", str(target), "--rpm", rpm,
                    "--ramp-up-ms", ramp, "--ramp-down-ms", ramp, "--change-case", case, "--change-after-cycle", ac,
                    "--change-value", value, "--replacement-target", str(replacement), "--duration-ms", dur,
                    "--cycles", cyc, "--interval-ms", iv, "--stop-kind", sk, "--stop-deadline-ms", "1500",
                    "--stop-best-effort-ms", "1500", "--stop-interval-ms", "500"]
            rc, text, d, rec = lad.motion(node, "position-change-{}".format(case), args, ok=(0, 3), seconds=90)
            lines = re.findall(r"TRIAL_(?:POSITION_CHANGE|CHANGE|MOVE)[A-Z_]* .*", text)
            st = lad.read(node)
            lad.say("  INVEST pchange case {} {} rpm ramp {} value {} dist {} repl {}: rc={} head={} state={}".format(
                case, rpm, ramp, value, dist, repl, rc, (rec.get("failure"), rec.get("why")), st))
            lad.note(kind="invest-pchange", node=node, case=case, rpm=rpm, ramp=ramp, value=value, start=pos,
                     target=target, replacement=replacement, rc=rc, lines=[x[:400] for x in lines], dir=d, state=st)
            if st and lad.enabled(st):
                lad.quick(node, "stop-quick-stationary")
        elif step == "vq":
            # A 6 rpm velocity run with its own quick stop on the 17 s plan that settled in every M14 trial: a fresh
            # settled stop record, and usually the 0015 latch (in place set), from which the HALT path can clear it.
            st = lad.begin_series(node, "vq")
            t = str(lad.s["trial"] + 1)
            args = ["velocity", "--rpm", "6", "--ramp-up-ms", "200", "--ramp-down-ms", "200", "--duration-ms", "17000",
                    "--interval-ms", "5000", "--deadline-ms", "24000", "--kind", "quick", "--record", t,
                    "--layout", "zh86"] + night.IDENT
            rc, text, d, rec = lad.motion(node, "velocity-quick-6-long", args, ok=(0, 3), seconds=90)
            if lad.good_stop(d, node):
                lad.s["nodes"][str(node)]["stoprec"] = d
                lad.save()
            st = lad.read(node)
            lad.say("  INVEST vq: rc={} head={} stoprec={} state={}".format(rc, (rec.get("failure"), rec.get("why")),
                                                                        lad.good_stop(d, node), st))
        elif step == "unlatch":
            # Clear the 2100 running latch (0014) the console fast stops left: the mode 1 enable's word 7 clears it
            # (trials 308, 1742), then a stationary quick stop releases the drive with a fresh stop record.
            pos = lad.position_series(node, "unlatch")
            text, d = lad.quick(node, "stop-quick-stationary")
            if lad.good_stop(d, node):
                lad.s["nodes"][str(node)]["stoprec"] = d
                lad.save()
            st = boot("unlatch-check")
            lad.say("  INVEST unlatch: {}".format(st))
        elif step == "state":
            st = boot("state")
            lad.say("  INVEST state: {}".format(st))
    except night.Retire as e:
        lad.say("  INVEST step {} retired: {}".format(step, e))
lad.save()
lad.say("stuck.py ended")
