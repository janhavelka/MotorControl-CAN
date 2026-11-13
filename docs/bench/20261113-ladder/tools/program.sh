#!/bin/bash
# Sequential edge exploration, 2026-11-13: one plan after another; a STOP file in the ladder dir ends a plan.
WT='C:\Users\Honza\Documents\Projects\MotorControl-CAN\.build\wt-b350da4'
LAD=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad/ladder.py
UNR=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad/unretire.py
OUT=/c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261113-ladder/run-program.out
cd /c/Users/Honza/Documents/Projects/MotorControl-CAN/.build/wt-b350da4
run() { [ -f /c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261113-ladder/STOP ] && exit 0; python "$UNR" >> "$OUT" 2>&1; echo "=== plan $*" >> "$OUT"; PYTHONIOENCODING=utf-8 python "$LAD" "$WT" "$@" >> "$OUT" 2>&1; }
run "grid1:51200"
run "n2:5000,5500,6000" 4500
echo "=== program done" >> "$OUT"
