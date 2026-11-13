"""Pin node 2's limit at 2201=51200 (already set): ascending quick-stop runs, stop at the first fault."""
import importlib.util, sys
spec = importlib.util.spec_from_file_location("ladder", r"C:\Users\Honza\AppData\Local\Temp\claude\7a9e2316d506a34f\1ad24d78-b127-4f86-8571-4006da894f8f\scratchpad\ladder.py")
m = importlib.util.module_from_spec(spec); argv = sys.argv; sys.argv = ["x"]; spec.loader.exec_module(m); sys.argv = argv
lad = m.Ladder(r"C:\Users\Honza\Documents\Projects\MotorControl-CAN\.build\wt-b350da4", False)
assert lad.s["nodes"]["2"]["subdivision"] == 51200, lad.s["nodes"]["2"]
try:
    good = lad.ladder(2, sys.argv[1:] or ["2600", "2800", "3000", "3200", "3400", "3516"], 2344,
                      stop_first=True)
    lad.say("highest clean step on node 2 at 2201=51200: {}".format(good))
except m.night.Retire as e:
    lad.say("!!! " + str(e)); lad.safe(2, str(e))
lad.say("pin51200 ended")
