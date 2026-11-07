# Packet 8 bench procedure: finite relative moves and stops from `005F`

Runnable plan for [packet 8](../../prompts/bench_closure_18_33/8_motion_stop_campaign.md) items 2 to 4, written on 2026-11-07 against `3353ed2` (bench image `elf=8bce2707...`, [start ping](../../bench/20261107T212519Z-s2-ping/rx.txt)). **Nothing here has been run.** Every command line below passed `python scripts/s2_bench.py ... --dry-run` with the values shown; the dry runs touched no port, prompt or file. Report: [packet_8](packet_8.md).

One lead holds the port. The user is the attendant: every enable needs the attendant's own felt shaft observation, and every motion line needs the typed `ATTENDED <trial>` with the independent drive-power cutoff in hand.

## Why the drives are power-cycled first

The [readiness reads of 2026-11-07](../../bench/20261107T212536Z-p8-readiness1/rx.txt) show:

| Node | `6041` | `2100` | `6061` | `6064` | `607A` | Since |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `0037` (enabled) | `0000` | 1 | 4728 | 12000 | The HALT of trial 709 and the `000F` of 710, 2026-11-05 |
| 2 | `4050` (released) | `0014` (running latched, released) | 1 | 821 | 10000 | The quick stop of trial 713, then a hand turn ([readiness2](../../bench/20261107T212546Z-p8-readiness2/rx.txt)) |

Both read `1001=00`, `603F=0000`, `606C=0` and heartbeat `7F`. Neither state admits an enable: the library needs `2100` in place with no running bit (node 1 reads `0000`, node 2 `0014`), and the start record `NO_MOTION_WRITE_RECORDED` is false for both, which were written after their last power-up. Node 1 has no host history in this S2 boot and there is no reviewed release transition, so no route may write to it as it is. A power cycle of both drives returns them to the power-up state every earlier session read: `6041=8037`, `2100=0001`, `6061=0`, `6064=0`, `607A=5000`, `60FF=120` (trials 701, 702).

## Fixed values

- Port and serial: `--port COM15 --serial 94A990398B40`. Rediscover the ESP-Prog-2 (USB `303A:1002`, serial `94A990398B40`) and use its port if it is no longer COM15.
- Ledger: `docs\bench\20261108-p8-campaign.jsonl`, created by `--new-campaign` on the first motion line (803) only. Replace `20261108` by the session's UTC date in the ledger and in every output directory, and `0900` by each step's UTC time. Nothing else in a line changes except the run-time fields below.
- Trials 801 to 813, rising. The highest trial in any existing ledger is 715.
- Release after every route, before the next line: `python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command release --output docs\bench\20261108T0900Z-p8-release-<trial>`; expected `RELEASED record; channel uncertainty unchanged`. A route sent while a record is held is refused and still counted.
- Node 1's move evidence is record 409 (relative base `ACTUAL`, packet 21), acceptance `6041` mask and value `0x1000`, completion `2100` mask and value `1`, `after-absence`, and the raw identity read on both nodes by the [smoke of 2026-11-07](../../bench/20261107T212409Z-s2-smoke/rx.txt): `00040192`, `000c`, `0020`, `00001000`, `1`, `0`, `0400`, `0200`. These are a node 1 measurement only; node 2 gets no production relative move in this boot.

## Run-time fields

| Field | Line | Fill from | Expected now |
| --- | --- | --- | --- |
| `--status` | 804, 807 | `6041` in the `TRIAL_LAST` of that node's mode 1 record (803, 806) | `0x8437` |
| `--position-target`, `--velocity-target` | 804, 807 | `RESULT object=607a:00` and `object=60ff:00` data of that node's readiness record (801, 802), little-endian | 5000, 120 |
| `--start-position` | 805 | `6064` in 804's `TRIAL_LAST` | 0 |
| `--start-position` | 808 | `6064` in 807's `TRIAL_LAST` | 0 |
| `--start-position` | 810 | `6064` in 809's `TRIAL_LAST`, the halted position | about 100 (node 1 halted at 104 with the same values, trial 406) |
| `--start-position` | 812 | `6064` in 811's `TRIAL_LAST` | -100, or one pulse off |
| Identity fields | 805, 812 | The smoke of 2026-11-07 above; the drives' identity does not change with a power cycle, and the move route reads it again before any write | As above. A difference refuses the move after it was counted, and is a stop condition |

A wrong `--start-position` is refused by the route's own check reads, after the firmware counted the line. There is no spare admission for a repeat.

## Caps

The runner's ledger and the firmware's per-boot counters both allow 12 motion admissions, 3 per route and node, and 3 stops per node (a planned stop counts once more and also against `stop`). Every sent line counts, admitted or refused. This plan uses exactly 12; the simulated ledger refused a thirteenth (below). Read routes (801, 802, 811) count nothing. If the S2 was reset or any route ran since its boot, check `motion=0` in the ping before 801: the firmware's counters restart only with the MCU, and a session's history only with a new session.

| Line | 803 | 804 | 805 | 806 | 807 | 808 | 809 | 810 | 812 | 813 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Case | M1 | E1 | G1 | M2 | E2 | P2 + S2 | R2 | L2 | G1 + S1 | S2 |
| Total after | 1 | 2 | 3 | 4 | 5 | 7 | 8 | 9 | 11 | 12 |

## Procedure

### 0. Physical preparation (user)

The attendant stands at the drives with the independent drive-power cutoff in hand, both motors table-mounted with free shafts and a visible mark on each. Then power-cycle both drives (their supply, not the S2). Nothing is sent meanwhile.

### 1. Link, no writes

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command ping --output docs\bench\20261108T0900Z-p8-ping
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command stats --output docs\bench\20261108T0900Z-p8-stats
```

Expected: `PONG ... base=3353ed2... elf=8bce2707... routes=5 soak=0 trial=0 motion=0 active=1 running=1`. If `active=0`, send `--command start` once and ping again. Stop if the image differs, `motion` is not 0 or the controller reports bus-off. The single-byte identity commands are not used: they are one-use experimental uploads per boot whose earlier capture (2026-11-02) ended in an abort on `1009`, and the move route reads the identity itself.

### 2. Power-up reads (no campaign yet)

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-actions --node 1 --trial 801 --fault-led prompt --seconds 30 --output docs\bench\20261108T0900Z-p8-p1-node1-801
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-actions --node 2 --trial 802 --fault-led prompt --seconds 30 --output docs\bench\20261108T0900Z-p8-p1-node2-802
```

Questions: the fault LED of that node before and after. Expected on both: `TRIAL ... failure=0 why=none`, `TRIAL_LAST passed=1 ... 1001=00 603f=0000 2100=0001 6041=8037 6061=00 6064=0 606c=0`, `607A` 5000, `60FF` 120, verdict `complete`. Stop on anything else: the drive did not return to its power-up state.

### 3. Node 1: mode 1, enable, item 2

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command mode1 --node 1 --trial 803 --cycles 5 --interval-ms 500 --campaign docs\bench\20261108-p8-campaign.jsonl --new-campaign --seconds 60 --output docs\bench\20261108T0900Z-p8-p2-node1-803
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command enable --node 1 --trial 804 --status 0x8437 --position-target 5000 --velocity-target 120 --deadline-ms 10000 --interval-ms 800 --no-motion-write-recorded --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 60 --output docs\bench\20261108T0900Z-p8-p3-node1-804
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command move --node 1 --trial 805 --start-position 0 --target -100 --rpm 6 --ramp-up-ms 200 --ramp-down-ms 200 --deadline-ms 30000 --interval-ms 3000 --record 409 --accept-source statusword --accept-mask 0x1000 --accept-value 0x1000 --complete-source monitor --complete-mask 1 --complete-value 1 --rule after-absence --device-type 262546 --hardware-version 12 --software-version 32 --vendor-id 4096 --product-code 1 --revision 0 --model-code 1024 --firmware-code 512 --move-base actual --stop-kind none --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 90 --output docs\bench\20261108T0900Z-p8-moverel-neg-node1-805
```

Questions: `ATTENDED <trial>` before each; for 804 also "turn the node 1 shaft by hand now: FREE or HOLDING as felt, or UNOBSERVED" (UNOBSERVED refuses the enable, unsent and uncounted) and afterwards FREE or HOLDING again; after each a free-text shaft description against the mark.

- 803: `TRIAL_LAST ... 6041=8437 6061=01`, `2100=0001`, `6064=0`.
- 804: `TRIAL_ENABLE own=1 state=2 outcome=1 exec=1 completion=1 uncertain=0`; words 6, 7, 15 each acknowledged (earlier power-ups: `8031`/`0011`, `8033`/`0001`, `8437`/`0001`); `TRIAL_LAST ... 6064=0`.
- 805, item 2: `TRIAL_FIRST` and `TRIAL_CHECK passed=1 ... 6064=0`; `TRIAL_BASE ... basis=1 witness=0`; `TRIAL_EVIDENCE record=409 ... rb=1`; `TRIAL_MOVE own=1 state=2 ... outcome=1 ... setup=1 exec=1 completion=1 rearm=1 uncertain=0 ... target=-100 ... fenced=0`. Check separately: acceptance in `TRIAL_MOVE_SAMPLE which=2` (`6041` bit 12 set), completion in `which=4` (`2100` bit 0 set), the retained effective target `target=-100`, and the raw endpoint in `which=4` and `TRIAL_LAST`. The endpoint may read -99 or -101: completion has been reported one pulse before final settling (trials 121, 706). Expected `6064` change is negative; the attendant reports the direction and the approximate rotation (about a tenth of a turn if `2201=1000` meant pulses per turn, which is not established).

At 6 rpm the move takes about 1.2 s, so the first cycle after the trigger should report it running and the second, 3 s later, completed. With interval 2500 ms the plan would need 131 terminals; the runner refuses it.

### 4. Node 2: mode 1, enable, item 3

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command mode1 --node 2 --trial 806 --cycles 5 --interval-ms 500 --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 60 --output docs\bench\20261108T0900Z-p8-p2-node2-806
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command enable --node 2 --trial 807 --status 0x8437 --position-target 5000 --velocity-target 120 --deadline-ms 10000 --interval-ms 800 --no-motion-write-recorded --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 60 --output docs\bench\20261108T0900Z-p8-p3-node2-807
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command position --node 2 --trial 808 --start-position 0 --target 600 --rpm 6 --ramp-up-ms 500 --ramp-down-ms 500 --cycles 4 --interval-ms 1000 --stop-kind halt --stop-after 2 --stop-deadline-ms 2000 --stop-best-effort-ms 0 --stop-interval-ms 250 --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 60 --output docs\bench\20261108T0900Z-p8-position-halt-node2-808
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command rearm --node 2 --trial 809 --predecessor position-halt --cycles 8 --interval-ms 1000 --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 60 --output docs\bench\20261108T0900Z-p8-rearm-halt-node2-809
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command relative --node 2 --trial 810 --start-position 104 --target 200 --rpm 6 --ramp-up-ms 500 --ramp-down-ms 500 --cycles 10 --interval-ms 1000 --stop-kind none --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 60 --output docs\bench\20261108T0900Z-p8-relative-node2-810
```

`104` in 810 is a placeholder: use 809's `TRIAL_LAST` `6064`. These are node 1's trials 404 to 409 repeated on node 2 with the same values, through the experiment routes, never a production request with node 1's record.

- 806, 807: as 803 and 804, for node 2.
- 808: `TRIAL_STOP kind=1 path=1 final=4 own=1 state=2 outcome=1 ... pred=001f documented=0 basis=3`; `6064` stops between 0 and 600 with `2100` running clear. Stop item 3 if the move completes at 600 or the drive reports release: actual and commanded positions would not differ.
- 809: `TRIAL_REARM pword=010f ... latest=1 word=000f`, and `6064` the same in `TRIAL_CHECK` and `TRIAL_LAST`. If `6064` moves, the `000F` resumed the halted target (node 1 did not, trials 408 and 710): record it and do not send 810, since actual and commanded positions would no longer differ.
- 810: `TRIAL_CHECK passed=1 ... 6064=<H>`; `TRIAL_BASE read=1 target=600 known=1 prev=600 prevTrial=808 prevRelative=0`; the endpoint in `TRIAL_LAST` is `H + 200` for an `ACTUAL` base or 800 for a `COMMANDED` base, one pulse either way. About 500 pulses separate the two. Any other endpoint supports neither and is recorded as such. Only the endpoint and the drive's acceptance and completion bits after `005F` become node 2's record; the attendant's shaft description is the only physical observation.

### 5. Item 4: HALT from node 1's held `005F` with node 2 observing, then a quick stop from node 2's `005F`

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-actions --node 1 --trial 811 --fault-led prompt --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 30 --output docs\bench\20261108T0900Z-p8-p1-node1-811
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command move --node 1 --trial 812 --start-position -100 --target 600 --rpm 6 --ramp-up-ms 200 --ramp-down-ms 200 --deadline-ms 5000 --interval-ms 2500 --record 409 --accept-source statusword --accept-mask 0x1000 --accept-value 0x1000 --complete-source monitor --complete-mask 1 --complete-value 1 --rule after-absence --device-type 262546 --hardware-version 12 --software-version 32 --vendor-id 4096 --product-code 1 --revision 0 --model-code 1024 --firmware-code 512 --move-base actual --observer 2 --stop-kind halt --stop-after 2 --stop-deadline-ms 1500 --stop-best-effort-ms 0 --stop-interval-ms 750 --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 60 --output docs\bench\20261108T0900Z-p8-moverel-halt-observed-node1-812
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command stop --node 2 --trial 813 --kind quick --deadline-ms 2000 --best-effort-ms 0 --interval-ms 250 --campaign docs\bench\20261108-p8-campaign.jsonl --seconds 30 --output docs\bench\20261108T0900Z-p8-stop-quick-node2-813
```

- 811: a fresh node 1 read for 812's `--start-position`, because a completion can be reported one pulse before the drive settles. Expected `2100=0001`, `6041` low bits 111, `6064` -100 or one pulse off.
- 812: the production relative move from 811's `6064`, +600 at 6 rpm (about 6 s of travel), interrupted by its planned HALT after the second cycle, 2.5 s in. No `000F` comes before the stop. Node 2 must first read stationary (running and alarm clear, `606C` 0) and is then only read, through its own session. Expected: `TRIAL_OBSERVER node=2 checked=1 passed=1 ... moved=0`, no download to node 2; `TRIAL_MOVE ... outcome=5 int=2 ... exec=1 completion=0 rearm=0 uncertain=1 ... fenced=1`, the interrupted move kept as its own record; `TRIAL_STOP kind=1 path=1 final=4 own=1 state=2 outcome=1 ... pred=005f documented=0 basis=3`; `6064` stops between the start and the start + 600. Node 1 then stays enabled and halted, fenced for this boot: the `010F` of a move is no rearm's predecessor. The shaft question names node 1; the attendant also says whether node 2 moved.
- 813: one `000B` from node 2's acknowledged `005F` of 810, which is still its latest word; node 2 is stationary, so this stop interrupts no move. Expected `TRIAL_STOP kind=2 path=1 final=4 own=1 state=2 outcome=1 ... pred=005f documented=0 basis=3`, and as after every `000B` so far a release report: `2100` bit 4 set and `6041` low bits 000 (from idle `000F`: `8050`, `2100=0011`, trial 124). A drive that does not report release ends the stop at its deadline: a finding, never a reason to repeat.

Both stops are each one finite request. Neither drive is touched again in this boot. Node 1 is left enabled and halted (holding as reported), node 2 released (as reported); the attendant checks both by hand at the end.

## Stop conditions

Stop the campaign, send nothing further and keep every record, when any of these appears. If a drive may still be moving, the attendant uses the cutoff; the S2 sends nothing because of a stop condition.

- The runner exits 1 (damaged or unfinished capture) or 2 (refused) on a line that was meant to run, or a ledger or lock refusal.
- A record with `failure` other than 0, other than the documented outcomes above; `lost=1`; `lossCancel=1`; `unsettled=1`.
- Any EMCY, any `2100` alarm bit, any `603F` or `1001` other than 0, or a mode other than 1 after 803 or 806.
- A move or stop reported `uncertain` where the expectation says otherwise, a lost reply, a controller error state or rising TEC/REC in `TRIAL_RES`.
- The attendant reports motion that the record does not explain, motion of the observed or idle node, a direction opposite to the record, a noise, heat or smell, or cannot observe.
- Any value marked expected that differs: for example the power-up state, the identity in 805's record, the enable words, 808 completing, or 809 moving.

Nothing is retried. A repeat is new reviewed intent, after the cause is known, under its own trial number, and only within the caps; this boot has no spare admission.

## Dry runs

`scripts/s2_bench.py --dry-run` for every line above, with the values shown: exit status 0 for all 13 trial lines and the single-byte lines (`ping`, `stats`, `release`). Key lines:

| Line | Plan bound | Line sent once |
| --- | --- | --- |
| 801 | 22 terminals | `TA1,801,K5cb7c842` |
| 802 | 22 terminals | `TA2,802,K821ee328` |
| 803 | 53 terminals, 2500 ms; campaign `motion_total 0`, would be appended | `TM1,803,1,5,500,K87fe695f` |
| 804 | 119 terminals, 10000 ms; the attendant's FREE or HOLDING replaces field 5 | `TE1,804,1,1,0,33847,5000,120,10000,800,K8a42df73` |
| 805 | 117 terminals, 30000 ms | `TG1,805,1,0,-100,6,200,200,30000,3000,409,1,4096,4096,2,1,1,1,262546,12,32,4096,1,0,1024,512,1,0,0,0,0,0,0,0,K2342f7ff` |
| 806 | 53 terminals, 2500 ms | `TM2,806,1,5,500,K70a340de` |
| 807 | 119 terminals, 10000 ms | `TE2,807,1,1,0,33847,5000,120,10000,800,K27bdac32` |
| 808 | 112 terminals, 17000 ms | `TP2,808,1,0,600,6,500,500,4,1000,1,2,2000,0,250,K194d16f1` |
| 809 | 66 terminals, 8000 ms | `TR2,809,1,2,8,1000,K12b4ad38` |
| 810 | 88 terminals, 10000 ms | `TL2,810,1,104,200,6,500,500,10,1000,0,0,0,0,0,K8b272738` |
| 811 | 22 terminals | `TA1,811,K45acf903` |
| 812 | 122 terminals, 6500 ms | `TG1,812,1,-100,600,6,200,200,5000,2500,409,1,4096,4096,2,1,1,1,262546,12,32,4096,1,0,1024,512,1,-100,2,1,2,1500,0,750,K43350a4f` |
| 813 | 71 terminals, 2000 ms | `TS2,813,1,2,2000,0,250,Kf62f19d4` |

Each motion line printed `require the operator to type: ATTENDED <trial>`. Against the absent ledger, 803 printed `campaign ...: motion_total 0 ...; this line would be appended before it is sent`, and every later line `would refuse: campaign ledger ... does not exist`, as expected before 803 creates it. Run in order against a scratch ledger that recorded each motion line as sent, the summaries rose `0, 1, 2, 3, 4, 5, 7, 8, 9, 9, 11` before 803 to 813, ended at `motion_total 12` with cases `E1 1, E2 1, G1 2, L2 1, M1 1, M2 1, P2 1, R2 1, S1 1, S2 2`, and a thirteenth motion line was refused: `the campaign holds 12 motion admissions; this line would make 13, over 12`. A run-time line whose values change must be dry-run again.

The same sequence, with this fake's identity and signals, runs through the real `BenchRoute` in `test/bench_route_test.cpp` `testPacket8Plan`: every line admitted in order, twelve admissions, and further motion refused as `budget`. With 812's deadline at 3000 ms the library refused the move after the firmware counted it; `scripts/s2_bench.py` now refuses a move deadline of 4550 ms or less before sending (`would refuse before sending: move deadline: 3000 ms does not cover the library's 13 services at 100 + 250 ms; it needs over 4550 ms`).

## Not in this boot

Twelve admissions cannot hold every case: a second stop from a moving `005F` on either node needs a fresh power-up, because after a HALT from `005F` no route rearms (only a POSITION record's planned HALT is a rearm predecessor), a quick stop releases the drive, and a move interrupted by a stop fences its session. The next boot, with its own ledger after this boot's review, can hold:

- A quick stop from node 1's held `005F` with node 2 observing (dry-run checked, 122 terminals, 6500 ms): readiness, `mode1` and `enable` of node 1, then
  `python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command move --node 1 --trial 905 --start-position 0 --target 600 --rpm 6 --ramp-up-ms 200 --ramp-down-ms 200 --deadline-ms 5000 --interval-ms 2600 --record 409 --accept-source statusword --accept-mask 0x1000 --accept-value 0x1000 --complete-source monitor --complete-mask 1 --complete-value 1 --rule after-absence --device-type 262546 --hardware-version 12 --software-version 32 --vendor-id 4096 --product-code 1 --revision 0 --model-code 1024 --firmware-code 512 --move-base actual --observer 2 --stop-kind quick --stop-after 2 --stop-deadline-ms 1500 --stop-best-effort-ms 0 --stop-interval-ms 500 --campaign docs\bench\YYYYMMDD-p8b-campaign.jsonl --seconds 60 --output docs\bench\YYYYMMDDTHHMMZ-p8b-moverel-quick-observed-node1-905`
  (dry-run with `--new-campaign` added, since that ledger does not exist yet; on the bench `--new-campaign` belongs on that boot's first motion line, its `mode1`). Its stop deadline holds three cycles at 500 ms, which the moving quick stop's standstill rule needs.
- If 810 gives a usable base, one production relative move on node 2 with record 810 and that base, from a fresh stationary witness, before any stop on node 2.
