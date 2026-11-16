#!/bin/bash
# Phase 4b (2026-11-16): console trials as single-port sessions (sched.py), then phase 5 (SYNC). Image 17386514.
WT='C:\Users\Honza\Documents\Projects\MotorControl-CAN\.build\wt-17386514'
D=/c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261116-hil
S=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad
cd /c/Users/Honza/Documents/Projects/MotorControl-CAN/.build/wt-17386514
run() { [ -f $D/STOP ] && exit 0; echo "=== session $*" >> $D/run-console.out; PYTHONIOENCODING=utf-8 timeout 1200 python "$S/conhil2.py" "$WT" "$@" >> $D/run-console.out 2>&1; echo "=== exit $? $*" >> $D/run-console.out; }
run vchg-after 2
run vchg-down 1
run vchg-down 2
run vchg-during 2
run vchg-big 1
run vchg-big 2
run mspeed 2
run mramps 2
run mspeed 1
run mramps 1
run mretarget 2
run hm12
run hm3
run hm5
echo "=== phase 4b done" >> $D/run-console.out
[ -f $D/STOP ] && exit 0
bash $S/hil_p5.sh
