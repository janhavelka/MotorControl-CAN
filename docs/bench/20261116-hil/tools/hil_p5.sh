#!/bin/bash
# Phase 5 (2026-11-16): SYNC production with both TPDO1 at type 1 (set in 13-tpdo1-type1), image 17386514.
D=/c/Users/Honza/Documents/Projects/MotorControl-CAN/docs/bench/20261116-hil
S=/c/Users/Honza/AppData/Local/Temp/claude/7a9e2316d506a34f/1ad24d78-b127-4f86-8571-4006da894f8f/scratchpad
ELF=20cc8ea2527993d8aa2acaa31be3508cd8b15bde598cb8e736ed7779439f0d54
cd /c/Users/Honza/Documents/Projects/MotorControl-CAN
[ -f $D/STOP ] && exit 0
python $S/en_reset.py .build/20261116-en-reset-p5 "fresh boot before the SYNC phase" > /dev/null 2>&1
python scripts/s2_pdo.py run --port COM15 --serial 94A990398B40 --plan $D/plans/operational.json --output $D/15-operational-sync --tail-s 1 >> $D/run-sync.out 2>&1
grep "^NETWORK" $D/15-operational-sync/rx.txt >> $D/run-sync.out
python scripts/sync_watch.py --port COM15 --expected-elf $ELF --duration-s 5 --period-ms 10 --output $D/16-sync-10ms-short >> $D/run-sync.out 2>&1
echo "=== short exit $?" >> $D/run-sync.out
[ -f $D/STOP ] && exit 0
python scripts/sync_watch.py --port COM15 --expected-elf $ELF --duration-s 1800 --period-ms 20 --output $D/17-sync-20ms-30min >> $D/run-sync.out 2>&1
echo "=== 20ms exit $?" >> $D/run-sync.out
echo "=== phase 5 done" >> $D/run-sync.out
