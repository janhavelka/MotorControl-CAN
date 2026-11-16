#!/bin/bash
# Phases 2-3 (2026-11-16): prompt 52 changes during motion through the Tv/Tp routes, then M14, image 17386514.
WT='C:\Users\Honza\Documents\Projects\MotorControl-CAN\.build\wt-17386514'
D=/c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261116-hil
STEP=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad/hilstep.py
cd /c/Users/Honza/Documents/Projects/MotorControl-CAN
run() { [ -f $D/STOP ] && exit 0; echo "=== step $*" >> $D/run-p2.out; PYTHONIOENCODING=utf-8 timeout 900 python "$STEP" "$WT" "$@" >> $D/run-p2.out 2>&1; }
for n in 1 2; do
  # Velocity change: after (change at the 5 s cycle, short ramp) and during (change at the 10 s cycle, stop at 12 s).
  run $n vchange:30:60:5000:500:12000:5000
  run $n vchange:60:30:5000:500:12000:5000
  run $n vchange:6:60:5000:200:12000:5000
  run $n vchange:60:6:9000:2000:12000:5000
  run $n vchange:12:30:9000:1000:12000:5000
done
for n in 2 1; do
  # Position change, mode 1 after an enable in the same boot: case:rpm:ramp:value:dist:repl:aftercycle:dur:cycles:iv:stop
  run $n pchange:5:12:1000:30:4000:0:2:6000:8:500:quick
  run $n pchange:5:60:200:12:4000:0:2:6000:8:500:quick
  run $n pchange:5:30:500:60:6000:0:2:6000:8:500:halt
  run $n pchange:6:30:500:0:800:600:2:6000:8:500:quick
  run $n pchange:6:60:200:0:2000:1500:2:6000:8:500:quick
  run $n pchange:7:30:500:0:800:600:2:6000:8:500:quick
  run $n pchange:7:12:1000:0:400:300:2:6000:8:500:quick
done
for n in 1 2; do
  run $n nmt:6
done
run 1 nmt:30
echo "=== phases 2-3 done" >> $D/run-p2.out
