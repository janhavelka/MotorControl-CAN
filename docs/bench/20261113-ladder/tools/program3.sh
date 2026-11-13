#!/bin/bash
# Position-mode ladder on both nodes, then the 51200 boundary bisection on node 1 (2026-11-13, same waiver).
WT='C:\Users\Honza\Documents\Projects\MotorControl-CAN\.build\wt-b350da4'
LAD=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad/ladder.py
UNR=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad/unretire.py
OUT=/c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261113-ladder/run-program3.out
cd /c/Users/Honza/Documents/Projects/MotorControl-CAN/.build/wt-b350da4
run() { [ -f /c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261113-ladder/STOP ] && exit 0; python "$UNR" >> "$OUT" 2>&1; echo "=== plan $*" >> "$OUT"; PYTHONIOENCODING=utf-8 python "$LAD" "$WT" "$@" >> "$OUT" 2>&1; }
run "pos2:300,600,1200,1800,2400,3000"
echo "=== program done" >> "$OUT"
