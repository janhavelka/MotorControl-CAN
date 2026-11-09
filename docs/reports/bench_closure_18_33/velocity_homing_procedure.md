# Prompts 35 and 36 bench procedure: finite velocity and homing admission

**Run on 2026-11-09 from 14:44Z on `4247e59` and stopped at line 1009** ([report](../2026-11-09_bench_r5.md)): node 1's velocity start ended unconfirmed and no stop was sent, so the motor ran until the user opened the cutoff (backlog S16). Do not send a velocity line again until S16 is fixed. The text below is the plan as written before that run.

Runnable plan for the attended routes `mode3`, `velocity` and `home` (line protocol 7, [hardware_bench](../../hardware_bench.md#attended-bench-routes)), written on 2026-11-09 against `8293b3e` plus the uncommitted route change that adds them, and revised the same day for protocol 7: the velocity lines run as the library's attended experiment and carry no start or stop flag. **Nothing here has been run.** Every command line below passed `python scripts/s2_bench.py ... --dry-run` with exit status 0 and the values shown; the dry runs touched no port, prompt or file. The same sequence, with the fake's own identity and reports, runs through the real `BenchRoute` in `test/bench_route_test.cpp` `testVelocityHomePlan`.

One lead holds the port. The user is the attendant: every motion line and every `home` line needs the typed `ATTENDED <trial>` with the independent drive-power cutoff in hand, and each enable needs the attendant's own felt shaft observation.

## Before the first line

1. The image must speak protocol 7. Build and flash it from the commit that holds the attended experiment scope (`VelocityGate::ATTENDED_EXPERIMENT`, PONG `routes=7`, the V line without start and stop fields) and the `scripts/s2_evidence.py` grammar rows for `TRIAL_HOME`, `TRIAL_VELOCITY_EVIDENCE` (with `gate`), `TRIAL_VELOCITY`, `TRIAL_VELOCITY_TIME`, `TRIAL_VELOCITY_SAMPLE` and the `TRIAL_PLAN` fields. The runner refuses any image that does not report `routes=7`, the protocol 6 images included; without the grammar rows its verdict on these records is not `complete`.
2. The image must allocate every block this plan needs: `BenchRoute` (42688 bytes on the S2), both `AxisSession`s (35592 each) and `RouteWork` (8432, on the first `home`). On 2026-11-09 the `8293b3e` image refused node 2's session (`reason=memory`; largest free block 34816 after `BenchRoute` and one session), and the allocation order in `main.cpp` is being changed. Until an image passes lines 1001 and 1002 with both sessions allocated, run node 1's part only and send nothing to node 2.
3. Power-cycle both drives (their supply, not the S2) with the attendant present, so that both read the power-up state every earlier session read: `6041=8037`, `2100=0001`, `6061=0`, `6064=0`, `607A=5000`, `60FF=120` (trials 701, 702, 801, 802). Nothing is sent meanwhile.
4. Reset the S2 or confirm `motion=0` in the first ping: the firmware's caps and the sessions' histories restart only with the MCU.

## What this can and cannot establish

**Velocity (prompt 35).** No record of this fixture establishes the mode 3 start (Q03 start order) or stop (Q12), so a `MEASURED` `VelocityEvidence` cannot be honest here. The route runs each line under `VelocityGate::ATTENDED_EXPERIMENT` ([velocity guide](../../velocity.md#attended-experiment-scope)): the run measures its start and stop and claims neither, `startQualified` and `stopQualified` stay false, no mode change or reverse range is claimed, the rate is at most 60 rpm and the duration at most 20000 ms, and every other library check stays. `--record <their own trial>` names this experiment's own record. The session's context keeps the experiment gate, so a successful record qualifies nothing by itself; only a later review of it could back a `MEASURED` record for this firmware tuple. The runner refuses `--start-qualified`, `--stop-qualified` and `--unattended-waiver` on a velocity line. **The lead and the user must accept the experiment before lines 1009 and 1016 are sent**, with the attendant's independent cutoff in hand. Without that acceptance, stop after line 1008 (or 1015): everything before it stands on its own.

The route claims no mode change and no reverse range, so the drive must already report mode 3, released and stationary. That needs a released drive first: the specimens' power-up state reports enabled (`2100` bit 4 clear, low bits 111), and no release command is reviewed. A quick stop from the enable's own `000F` has reported release on both specimens (trials 124, 208: `6041=8050`, `2100=0011`), so each node runs mode 1, the enable, that quick stop, then `mode3` (one `6060=3` on the released drive, Q11's released mode change), then `velocity`. The run follows ZH86 PDF p13: `60FF`, `6083`, `6084`, then words 6, 7, 15, each after a fresh cycle that reports it, then running cycles, then its own HALT at the preparation-anchored due time.

A positive run can establish, for this firmware and these two specimens only: whether `6060=3` is accepted and reported while released (Q11), whether words 6/7/15 in mode 3 follow the low-bit sequence and start the motor at 15 rather than earlier (Q03), the reported running bits in mode 3, and what a HALT `010F` from the acknowledged `000F` reports in mode 3 (Q12). It qualifies no other rate, no reverse or zero target, no quick stop in mode 3, no enabled mode change and no other firmware. Physical direction and rate come only from the attendant.

**Homing (prompt 36).** No retained record establishes a homing method, switch wiring, logical polarity, direction under `2200`, a mode 6 enable or `607C`'s effect ([homing guide](../../homing.md#remaining-specimen-evidence)), so the route's `HomeEvidence` is CLOSED and no line field opens it. Each `home` line reads what `requestHome` consumes (identity, `2400:00..08`, `2101`, `2200`/`2201`) and the library refuses before any write. That refusal, with no download in the record, is the bench-checkable result.

Correction to the request that asked for this route: the refusal is **not** "the method's required input function is absent". Both specimens assign functions 1, 2 and 3 to DI0, DI1 and DI2 (`2400:02/:03/:04 = 1/2/3`, 2026-11-04, [I/O guide](../../io_settings.md#specimen-readings-2026-11-04)), so every method's function is present in the mapping; what is missing is wiring evidence and every qualification. The library checks its capability gate before anything else, and checks the input mapping only after an acknowledged `000F` and a fresh enabled, stationary mode 6 report. An `INPUT` refusal would need a measured `HomeEvidence` and a drive enabled in mode 6; neither is admissible here, and no route enables in mode 6 or changes `6060` while enabled. A session with no latest controlword refuses earlier still, which is why the `home` lines follow each node's quick stop.

## Fixed values

- Port and serial: `--port COM15 --serial 94A990398B40`. Rediscover the ESP-Prog-2 (USB `303A:1002`, serial `94A990398B40`) and use its port if it is no longer COM15.
- Ledger: `docs\bench\YYYYMMDD-p3536-campaign.jsonl`, created by `--new-campaign` on the first counted line (1003) only. Replace `YYYYMMDD` by the session's UTC date in the ledger and in every output directory, and `HHMM` by each step's UTC time. Nothing else in a line changes except the run-time fields below.
- Trials 1001 to 1018, rising. Check that no ledger of this boot already holds a trial at or above 1001; packet 8's run of 2026-11-09 reached 833 (`b35849d`). That run left node 1 enabled and halted and node 2 released, which is why the drives are power-cycled first.
- Release after every route, before the next line: `python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command release --output docs\bench\YYYYMMDDTHHMMZ-p3536-release-TRIAL`, `TRIAL` the trial just recorded; expected `RELEASED record; channel uncertainty unchanged`. A route sent while a record is held is refused and still counted.
- Raw identity of both nodes, from the [smoke of 2026-11-07](../../bench/20261107T212409Z-s2-smoke/rx.txt): `1000=00040192` (262546), `1009=0c` (12), `100A=20` (32), `1018:01=00001000` (4096), `1018:02=1`, `1018:03=0`, `2000=0400` (1024), `2001=0200` (512). The velocity route reads it again and the library compares it exactly; a difference refuses the run after it was counted, and is a stop condition.
- Velocity values: 6 rpm (`606C` reads motor rpm on this configuration, 2026-11-04), ramps 200 ms, duration 12000 ms from the request, running cycles every 2000 ms, deadline 18000 ms, HALT, layout ZH86. ZH86 because the run follows ZH86 p13; in mode 1 moves both specimens reported `6041` bit 14 set while `2100` reported running (`5037`, trial 121) and clear at rest (`8437`), consistent with ZH86's running bit, and the alarm bits of both dialects read 0. Mode 3's bits are unmeasured; a contradiction ends the run with its evidence. At 6 rpm for about 12 s the motor shaft would turn about 1.2 revolutions if `606C` and `60FF` share the motor rpm unit, which is not established for `60FF`.
- Home values: search 60 rpm, creep 6 rpm, ramp 200 ms, offset 0. They are a well-formed request, never reached by the library's checks while the gate is closed, and nothing is written.

## Run-time fields

| Field | Line | Fill from | Expected now |
| --- | --- | --- | --- |
| `--status` | 1004, 1011 | `6041` in the `TRIAL_LAST` of that node's mode 1 record (1003, 1010) | `0x8437` |
| `--position-target`, `--velocity-target` | 1004, 1011 | `RESULT object=607a:00` and `object=60ff:00` data of that node's readiness record (1001, 1002), little-endian | 5000, 120 |
| Identity fields | 1009, 1016 | The smoke of 2026-11-07 above | As above; a difference is a stop condition |

A run-time line whose values change must be dry-run again before it is sent.

## Caps

The runner's ledger and the firmware's per-boot counters both allow 12 motion admissions, 3 per route and node, and 3 stops per node (a velocity's own stop counts once more, and against `stop`). `home` has no motion weight and its own cap of 3 per node. Every sent line counts, admitted or refused. This plan uses exactly 12 motion admissions; the simulated ledger refused a thirteenth (below).

| Line | 1003 | 1004 | 1005 | 1006, 1007 | 1008 | 1009 | 1010 | 1011 | 1012 | 1013, 1014 | 1015 | 1016 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Case | M1 | E1 | S1 | O1 ×2 | N1 | V1 + S1 | M2 | E2 | S2 | O2 ×2 | N2 | V2 + S2 |
| Motion total after | 1 | 2 | 3 | 3 | 4 | 6 | 7 | 8 | 9 | 9 | 10 | 12 |

## Procedure

### 0. Physical preparation (user)

The attendant stands at the drives with the independent drive-power cutoff in hand, both motors table-mounted with free shafts and a visible mark on each, and confirms by eye which DI terminals of each drive are wired (the `home` lines ask). Then power-cycle both drives. Do not turn a released shaft by hand at any time in this boot: on both specimens word 7 pulled a displaced shaft back to its last commanded position.

### 1. Link, no writes

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command ping --output docs\bench\YYYYMMDDTHHMMZ-p3536-ping
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command stats --output docs\bench\YYYYMMDDTHHMMZ-p3536-stats
```

Expected: `PONG ... routes=7 soak=0 trial=0 motion=0 active=1 running=1` and the base revision of the flashed commit. If `active=0`, send `--command start` once and ping again. Stop if `routes` is not 7, `motion` is not 0 or the controller reports bus-off.

### 2. Power-up reads, both sessions allocated (no campaign yet)

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-actions --node 1 --trial 1001 --fault-led prompt --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-p1-node1-1001
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-actions --node 2 --trial 1002 --fault-led prompt --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-p1-node2-1002
```

Questions: the fault LED of that node before and after. Expected on both: `TRIAL ... failure=0 why=none`, `TRIAL_LAST passed=1 ... 1001=00 603f=0000 2100=0001 6041=8037 6061=00 6064=0 606c=0`, `607A` 5000, `60FF` 120. These two lines allocate `BenchRoute` and both sessions first, the largest blocks; `TRIAL_RES largest` in 1002 shows what is left for `RouteWork` (8432 bytes on the S2), which the first `home` line allocates and every `velocity` line needs. A `TRIAL_REFUSED reason=memory` on 1002 means node 2 has no session: continue with node 1 only. Stop on anything else, and on any `reason=memory` refusal later in the boot.

### 3. Node 1: mode 1, enable, quick stop

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command mode1 --node 1 --trial 1003 --cycles 5 --interval-ms 500 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --new-campaign --seconds 60 --output docs\bench\YYYYMMDDTHHMMZ-p3536-p2-node1-1003
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command enable --node 1 --trial 1004 --status 0x8437 --position-target 5000 --velocity-target 120 --deadline-ms 10000 --interval-ms 800 --no-motion-write-recorded --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 60 --output docs\bench\YYYYMMDDTHHMMZ-p3536-p3-node1-1004
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command stop --node 1 --trial 1005 --kind quick --deadline-ms 2000 --best-effort-ms 0 --interval-ms 250 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-stop-quick-node1-1005
```

Questions: `ATTENDED <trial>` before each; for 1004 also "turn the node 1 shaft by hand now: FREE or HOLDING as felt, or UNOBSERVED" (UNOBSERVED refuses the enable, unsent and uncounted) and afterwards FREE or HOLDING again; after each a free-text shaft description against the mark.

- 1003: `TRIAL_LAST ... 2100=0001 6041=8437 6061=01 6064=0`.
- 1004: `TRIAL_ENABLE own=1 state=2 outcome=1 exec=1 completion=1 uncertain=0`; words 6, 7, 15 each acknowledged (earlier power-ups: `8031`/`0011`, `8033`/`0001`, `8437`/`0001`).
- 1005: one `000B` from the enable's acknowledged `000F`, on a stationary drive. Expected `TRIAL_STOP kind=2 path=1 final=4 own=1 state=2 outcome=1 ... pred=000f documented=0`, and the release report every earlier `000B` from `000F` gave: `6041` low bits 000 and `2100` bit 4 set (`8050`, `2100=0011`, trial 124). A drive that does not report release ends the stop at its deadline; then stop the node's part here: `mode3` refuses a drive that reports enabled.

### 4. Node 1: homing admission, refused before any write

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command home --node 1 --trial 1006 --method 24 --search-rpm 60 --creep-rpm 6 --ramp-ms 200 --offset 0 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-home24-node1-1006
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command home --node 1 --trial 1007 --method 29 --search-rpm 60 --creep-rpm 6 --ramp-ms 200 --offset 0 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-home29-node1-1007
```

Questions: `ATTENDED <trial>`, then "which DI terminals of node 1 are wired to a switch, as seen? Type NONE, or the terminals and switches". The answer is recorded in `request.json` only; nothing typed opens the gate. No shaft question follows: nothing is written.

Expected for each, runner exit status 3 (a consistent record of a failure): `TRIAL ... route=home failure=11 why=home ... terminals=29`, 29 `RESULT` lines all with `dir=0` (identity, `2400:00..08`, the full read, `2200`, `2201`) and none with `dir=1`; `TRIAL_STATUS code=3 detail=7; homing needs exact selected-firmware qualification`; `TRIAL_IO which=0 present=1 entries=16 polarity=0000 di0=0001 di1=0002 di2=0003 di3=0000 di4=0000 di5=0000 di6=0000`; `TRIAL_HOME method=24 gate=0 identity=1 io=1 state=1 driver=1 2101=0000 2200=0000 2201=03e8 admitted=0` (29 for 1007), the values both nodes read before (`2400` on 2026-11-04, `2101`, `2200`, `2201` in [motion-readiness](../../bench/20261102-motion-readiness-node1/rx.txt) and trial 821). Any download, a `TRIAL_STATUS` other than this one, or `admitted=1` is a stop condition. A different `2400` or `2101` is recorded, not a stop condition: it is the evidence this route exists to collect.

### 5. Node 1: mode 3, then the velocity run

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command mode3 --node 1 --trial 1008 --cycles 2 --interval-ms 500 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-mode3-node1-1008
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command velocity --node 1 --trial 1009 --rpm 6 --ramp-up-ms 200 --ramp-down-ms 200 --duration-ms 12000 --interval-ms 2000 --deadline-ms 18000 --kind halt --record 1009 --layout zh86 --device-type 262546 --hardware-version 12 --software-version 32 --vendor-id 4096 --product-code 1 --revision 0 --model-code 1024 --firmware-code 512 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 60 --output docs\bench\YYYYMMDDTHHMMZ-p3536-velocity-node1-1009
```

Questions: `ATTENDED <trial>` before each; afterwards a free-text shaft description: whether it turned, which way against the mark, about how far, and when it stopped.

- 1008: `TRIAL_FIRST` and `TRIAL_CHECK passed=1 ... 2100=0011 6041=8050 6061=01 6064=0 606c=0`; one `2F 60 60 00 03 00 00 00`, acknowledged; `TRIAL_LAST ... 6061=03`. Whether `2100` and `6041` keep reporting release in mode 3 is the observation; nothing should turn. Stop the node's part if the mode does not read 3, if release is no longer reported, or on any motion.
- 1009: the attendant expects the shaft to start turning within a fraction of a second of the line and to stop about 12 s later, within the 200 ms ramp. Expected if the ZH86 sequence holds, none of it established: `TRIAL_CHECK passed=1 ... 2100=0011 6061=03 606c=0`; `TRIAL_VELOCITY_EVIDENCE record=1009 layout=2 gate=2 start=0 stop=0 kind=1 max=6 reverse=0 modeChange=0 dt=00040192 hw=000c sw=0020 vid=00001000 pc=00000001 rev=00000000 mc=0400 fc=0200`; downloads in order `23 FF 60 00 06 00 00 00`, `23 83 60 00 C8 00 00 00`, `23 84 60 00 C8 00 00 00`, `2B 40 60 00 06 00 00 00`, `...07...`, `...0F...`, then after the due time `2B 40 60 00 0F 01 00 00`; `TRIAL_VELOCITY own=1 state=2 ... outcome=1 first=0 ... completion=1 stop=1 sexec=1 uncertain=0 ... needs=0 fenced=0 running=1 ... rpm=6`; `TRIAL_VELOCITY_TIME` with `due` 12000000 µs after `started` and `stopAt` at or after `due`, at most one interval later; `TRIAL_STOP kind=1 path=0 final=4 own=1 state=2 outcome=1 ... pred=000f documented=0`; `TRIAL_VELOCITY_SAMPLE which=1` with `6041` bit 14 set, `2100` bit 2 set and `606c` 6 while running, if mode 3 reports as mode 1 did. The record ends about 12.5 s after the line. Record whatever differs; it is the measurement.

### 6. Node 2: the same, with the limit methods

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command mode1 --node 2 --trial 1010 --cycles 5 --interval-ms 500 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 60 --output docs\bench\YYYYMMDDTHHMMZ-p3536-p2-node2-1010
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command enable --node 2 --trial 1011 --status 0x8437 --position-target 5000 --velocity-target 120 --deadline-ms 10000 --interval-ms 800 --no-motion-write-recorded --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 60 --output docs\bench\YYYYMMDDTHHMMZ-p3536-p3-node2-1011
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command stop --node 2 --trial 1012 --kind quick --deadline-ms 2000 --best-effort-ms 0 --interval-ms 250 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-stop-quick-node2-1012
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command home --node 2 --trial 1013 --method 17 --search-rpm 60 --creep-rpm 6 --ramp-ms 200 --offset 0 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-home17-node2-1013
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command home --node 2 --trial 1014 --method 18 --search-rpm 60 --creep-rpm 6 --ramp-ms 200 --offset 0 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-home18-node2-1014
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command mode3 --node 2 --trial 1015 --cycles 2 --interval-ms 500 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-mode3-node2-1015
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command velocity --node 2 --trial 1016 --rpm 6 --ramp-up-ms 200 --ramp-down-ms 200 --duration-ms 12000 --interval-ms 2000 --deadline-ms 18000 --kind halt --record 1016 --layout zh86 --device-type 262546 --hardware-version 12 --software-version 32 --vendor-id 4096 --product-code 1 --revision 0 --model-code 1024 --firmware-code 512 --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 60 --output docs\bench\YYYYMMDDTHHMMZ-p3536-velocity-node2-1016
```

Run node 2 only if node 1's records matched their expectations or the differences were reviewed. Questions and expectations as for node 1, with node 2 and methods 17 and 18 in `TRIAL_HOME`. A second specimen with the same values is replication, not a new qualification.

### 7. Final reads

```text
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-actions --node 1 --trial 1017 --fault-led prompt --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-p1-node1-1017
python scripts\s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-actions --node 2 --trial 1018 --fault-led prompt --campaign docs\bench\YYYYMMDD-p3536-campaign.jsonl --seconds 30 --output docs\bench\YYYYMMDDTHHMMZ-p3536-p1-node2-1018
```

The end state as each drive reports it after its HALT: expected `6061=03`, low bits 111, `2100` running and released clear, `606C` 0, `60FF` 6, `6083` and `6084` 200. Nothing more is written to either drive in this boot: no reviewed transition leaves a halted mode 3 drive, and a later `velocity` would be refused by its own check (`unreleased`). The attendant checks both shafts by hand, then power-cycles both drives.

## Stop conditions

Stop the campaign, send nothing further and keep every record, when any of these appears. If a drive may still be moving, the attendant opens the cutoff; the S2 sends nothing because of a stop condition. **During a velocity run Ctrl-C does not withdraw it**: the firmware refuses the abort (`TRIAL_ABORT refused reason=velocity`) so that the run's own stop still leaves; the cutoff is the immediate control.

- The runner exits 1 (damaged or unfinished capture) or 2 (refused) on a line that was meant to run, a ledger or lock refusal, or `reason=memory`.
- A record with `failure` other than 0, other than the documented `failure=11 why=home`; `lost=1`; `lossCancel=1`; `unsettled=1`.
- Any EMCY, any `2100` alarm bit, any `603F` or `1001` other than 0. An EMCY during a velocity run withdraws it with `DRIVE_FAULT` and **no stop follows**: open the cutoff.
- Motion before word 15's report in a velocity record, motion during `mode3` or a `home` line, motion of the other node, a direction or speed that the attendant cannot reconcile with 6 rpm, or a shaft still turning 2 s after the due time: open the cutoff.
- A velocity record whose stop did not leave or did not report settlement (`TRIAL_VELOCITY ... needs=1` or `fenced=1`, `TRIAL_STOP` absent or `state=3`): the drive may still be enabled in mode 3 with `60FF=6`; open the cutoff. If running was never reported (`running=0`, first failure 9 `SERVICE_TIMEOUT`), the stop left only if `2100` reported running; otherwise nothing was sent and the drive stays enabled.
- A `home` record with a download, `admitted=1`, or any refusal other than the expected one.
- The attendant reports a noise, heat or smell, or cannot observe.

Nothing is retried. A repeat is new reviewed intent, after the cause is known, under its own trial number and within the caps; this boot has no spare motion admission.

## Dry runs

`scripts/s2_bench.py --dry-run` for every line above, with the values shown: exit status 0 for all 18 trial lines and the single-byte lines (`ping`, `stats`, `release`). Each motion and `home` line printed `require the operator to type: ATTENDED <trial>`; each `home` line also `ask the attendant which DI terminals are wired (recorded only); the route's HomeEvidence is CLOSED, so the library refuses before any write`; each velocity line `VelocityEvidence gate ATTENDED_EXPERIMENT, startQualified 0, stopQualified 0: the run measures its start and stop and claims neither; from this line: record <trial>, layout 2, stop kind 1 (motion weight 2 with its own stop)`; and each line `send p; refuse unless PONG has routes=7 soak=0 trial=0 active=1 running=1`. The protocol 6 form of lines 1009 and 1016, with `--start-qualified 1 --stop-qualified 1`, now exits 2 before anything: `--start-qualified and --stop-qualified are refused`.

| Line | Plan bound | Line sent once |
| --- | --- | --- |
| 1001 | 22 terminals | `TA1,1001,K7f410608` |
| 1002 | 22 terminals | `TA2,1002,K60dc251c` |
| 1003 | 53 terminals, 2500 ms | `TM1,1003,1,5,500,K892bc4b5` |
| 1004 | 119 terminals, 10000 ms; the attendant's FREE or HOLDING replaces field 5 | `TE1,1004,1,1,0,33847,5000,120,10000,800,K2cc433ac` |
| 1005 | 71 terminals, 2000 ms | `TS1,1005,1,2,2000,0,250,Kd447a257` |
| 1006 | 29 terminals | `TO1,1006,1,24,60,6,200,0,Ka14c3cfb` |
| 1007 | 29 terminals | `TO1,1007,1,29,60,6,200,0,Kb845f656` |
| 1008 | 35 terminals, 1000 ms | `TN1,1008,1,2,500,Ka823ec01` |
| 1009 | 118 terminals, 18000 ms | `TV1,1009,1,6,200,200,12000,2000,18000,1,1009,2,262546,12,32,4096,1,0,1024,512,K07a11bb6` |
| 1010 | 53 terminals, 2500 ms | `TM2,1010,1,5,500,K2305b695` |
| 1011 | 119 terminals, 10000 ms | `TE2,1011,1,1,0,33847,5000,120,10000,800,Kc00783bf` |
| 1012 | 71 terminals, 2000 ms | `TS2,1012,1,2,2000,0,250,Kd5061382` |
| 1013 | 29 terminals | `TO2,1013,1,17,60,6,200,0,K499c75e8` |
| 1014 | 29 terminals | `TO2,1014,1,18,60,6,200,0,K1b9a96d2` |
| 1015 | 35 terminals, 1000 ms | `TN2,1015,1,2,500,Kcacf45b3` |
| 1016 | 118 terminals, 18000 ms | `TV2,1016,1,6,200,200,12000,2000,18000,1,1016,2,262546,12,32,4096,1,0,1024,512,K3e99c2c8` |
| 1017 | 22 terminals | `TA1,1017,K8f39927c` |
| 1018 | 22 terminals | `TA2,1018,K9912fd43` |

The velocity's 118 terminals are 9 identity, 9 full read, 3 parameters, 3 words with a seven-field cycle each, 6 running cycles of 7 and its stop over the remaining 6000 ms at 2000 ms (31), within the record's 128; a 100 ms interval would not fit and the runner refuses it. The library's own floors, which the runner also checks before sending: a duration of at least 9470 ms (27 setup services at the route budgets and two 10 ms enable intervals) and more than 2800 ms between the duration and the deadline (8 services).

Against the absent ledger, 1003 printed `campaign ...: motion_total 0 ...; this line would be appended before it is sent`, and every later counted line `would refuse: campaign ledger ... does not exist`, as expected before 1003 creates it. Run in order against a scratch ledger that recorded each counted line as sent, the summaries rose `0, 1, 2, 3, 3, 3, 4, 6, 7, 8, 9, 9, 9, 10, 12, 12` before 1003 to 1018, ended at `motion_total 12` with cases `E1 1, E2 1, M1 1, M2 1, N1 1, N2 1, O1 2, O2 2, S1 2, S2 2, V1 1, V2 1`, and a thirteenth motion line was refused: `the campaign holds 12 motion admissions; this line would make 13, over 12`.

## Not in this boot

- A quick stop in mode 3 (`--kind quick`): the library's mode 3 stop scope refuses a reported release as its settlement, as it should until measured, so a `000B` that releases would end that run's stop `OBSERVATION_SCOPE`. It needs its own boot and review after this one.
- Reverse, zero and other rates, a mode change by the library itself (`modeChangeQualified`), an enabled mode change, and any other firmware.
- Homing beyond admission: switch wiring, polarity, a reviewed mode 6 enable, direction under `2200`, `607C` effects and completion need their own evidence and a separate route. The `home` route never opens its gate.
