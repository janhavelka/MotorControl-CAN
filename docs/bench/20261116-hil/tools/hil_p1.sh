#!/bin/bash
# Phase 1 (2026-11-16): short existing-feature regression and prompt 59's remaining runs, image 17386514.
WT='C:\Users\Honza\Documents\Projects\MotorControl-CAN\.build\wt-17386514'
D=/c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261116-hil
HIL=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad/hil.py
cd /c/Users/Honza/Documents/Projects/MotorControl-CAN/.build/wt-17386514
run() { [ -f $D/STOP ] && exit 0; echo "=== plan $*" >> $D/run-p1.out; PYTHONIOENCODING=utf-8 python "$HIL" "$WT" "$@" >> $D/run-p1.out 2>&1; }
run "s1:60"
run "s2:60"
run "pos1:300,600,1200,1800,2400,3000"
run "rev2:60,300,1000,2000,3000"
run "pos2:300"
echo "=== phase 1 done" >> $D/run-p1.out
