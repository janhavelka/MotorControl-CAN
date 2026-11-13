"""Enable-fault investigation steps on one node (2026-11-13). Usage: python stuck.py WT NODE STEP...
Steps: provoke:RPM (velocity at RPM, expect FF05, then controlword clear), check (fresh boot + 60 rpm enable check),
clear (fresh boot + controlword clear if an alarm is present), state (fresh boot + read)."""
import importlib.util, re, sys
spec = importlib.util.spec_from_file_location("ladder", r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad\ladder.py")
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
            st = boot("loop{}".format(value))
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
        elif step == "state":
            st = boot("state")
            lad.say("  INVEST state: {}".format(st))
    except night.Retire as e:
        lad.say("  INVEST step {} retired: {}".format(step, e))
lad.save()
lad.say("stuck.py ended")
