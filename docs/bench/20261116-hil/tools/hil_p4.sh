#!/bin/bash
# Phase 4 (2026-11-16): console-path HIL sessions, image 17386514, one MCU boot per session.
WT='C:\Users\Honza\Documents\Projects\MotorControl-CAN\.build\wt-17386514'
D=/c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261116-hil
C=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad/conhil.py
cd /c/Users/Honza/Documents/Projects/MotorControl-CAN/.build/wt-17386514
run() { [ -f $D/STOP ] && exit 0; echo "=== session $*" >> $D/run-console.out; PYTHONIOENCODING=utf-8 timeout 1200 python "$C" "$WT" "$@" >> $D/run-console.out 2>&1; echo "=== exit $? $*" >> $D/run-console.out; }
run cmove 2
run cvel 1
run cvel 2
run vchg-after 1
run vchg-down 2
run vchg-during 1
run vchg-during 2
run vchg-after 2
run mspeed 2
run mramps 2
run mspeed 1
run mramps 1
run mretarget 2
run hm12
run hm3
run hm5
echo "=== phase 4 done" >> $D/run-console.out
