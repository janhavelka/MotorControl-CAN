# Attended bench session, 2026-11-03

The first attended motion session on this library's own routes: packet 15 P1 to P3 on both specimens, packet 19's position experiment with the rearm, and packet 16's moving stop, all on node 1 first. The user was present at the independent drive-power cutoff throughout, with both shafts free and marked and both motors fixed to the table. **One specimen and one firmware each; nothing here qualifies a firmware or a production capability.**

## Setup and conduct

| Item | Value |
| --- | --- |
| Fixture | ESP-Prog-2 on COM15 (serial `94A990398B40`), ESP32-S2, two CL86-C drives, nodes 1 and 2, 1 Mbit/s |
| Images | `71c8670` (P1, P2, first P3), then `e17271a` after the fix below, each flashed by `scripts/s2_flash.py` and checked by `ping` and `--check-ping` |
| Campaign | [`20261103-campaign.jsonl`](../bench/20261103-campaign.jsonl): 17 sends, **12 motion admissions, the packet 22 cap**. Two were sends the firmware refused (`capacity`) or that never left (`trial_record_held`): the ledger counts every send |
| Operator answers | The host runner asks by `input()`; a relay passed each question to the lead, who asked the attendant in chat and appended the answer. At the user's request, routine confirmations were not asked: `ATTENDED <trial>` was relayed from their standing presence, and shaft answers not observed are marked so. Two lead-entered answers that claimed an observation are corrected by `NOTE.txt` beside them |

## Results

| Trial | Node | Route | Result |
| --- | --- | --- | --- |
| 101 | 1 | P1 reads | Clean. First reads of `1001` and `603F`: both 0, fault LED off before and after. `2100=1`, `6041=8037`, mode 0, `6064=0`, `607A=5000`, `60FF=120`, `6081`/`6083`/`6084` 60/100/100, `1017=0` |
| 102 | 1 | P2 `6060=1` | Acknowledged. Mode 1, and **`6041` bit 10 set** (`8437`); no motion |
| 103 | 1 | P3 enable | Shaft **HOLDING at power-up**, before any word. Word 6: `6041=8031`, `2100=0011` (released), shaft FREE afterwards. Bit 10 cleared, the enable judged it out of scope and **sent nothing more**: the defect fixed in `e17271a` |
| 105 | 1 | P3 enable, from released | Word 6 accepted. **Word 7 energized the motor** (`2100` released clear, `6041=8033`) and `6064` went from −545, after the attendant's hand check, to 0: counter reset or pull-back, not observed. Out of scope, word 15 not sent |
| 108 | 1 | P3 enable, after a power cycle | **Succeeded**: 6 → `8031`/`0011`, 7 → `8033`/`0001`, 15 → `8437`/`0001`, position 0 throughout |
| 111 | 1 | Position 0 → 250, 2 rpm | Accepted within 24 ms, complete between 8 and 9 s, `6064=250`. The attendant saw **about a quarter turn counter-clockwise** from the shaft end |
| 112 | 1 | Rearm `000F` | Bit 12 cleared (`9437` to `8437`), position unchanged |
| 113 | 1 | Position 250 → 750, HALT after 3 cycles | `010F` from `001F` at 2.05 s; the stop was **observed** (`2100` running cleared). Final `6064=316`, unchanged 26 s later: **no creep** |
| 114, 115 | 1 | P1 reads | After the stop, `6064=316` still. `6041=0437`: bits 15 and 12 clear. `2100=0000`: in place and running both clear. The halted move reports neither completion signal, so a move halted short of its target never reads as complete |
| 116 to 118 | 2 | P1, P2, P3 | Same bytes as node 1: `8037`, `8437` in mode 1, shaft HOLDING at power-up, and **the enable succeeded** with identical words |

## What the drives showed

**Power-up.** Both shafts hold torque at power-up, with `6041` low bits 111 and `2100` released clear, before any controlword. So the enable's first word, 6, **drops holding torque**, and word 7 restores it, not word 15.

**Status bits during a move** (node 1, trial 111; the same bits as packet 20's evidence list):

| Phase | `6041` | `2100` |
| --- | --- | --- |
| Idle, after the enable | `8437`: 15 done, 10 zero speed, low 111 | `0001`: in place |
| From 24 ms after `001F` | `5037`: 14 running, 12 set, 15 clear, 10 clear | `0004`: running |
| Complete | `9437`: 15 done, 12 still set, 10 set | `0001` |
| After the rearm `000F` | `8437`: 12 cleared | `0001` |

The specimen sets EN86's bits 10, 12 and 15 and ZH86's bit 14 at once, so neither manual's table alone describes it. For packet 20's `MoveEvidence`, the measured candidates are:
- **Acceptance:** `6041` bit 12. It is absent at idle and after the rearm, and present from the first cycle after `001F`, latched until the rearm.
- **Completion:** `2100` bit 0, which was already set before the trigger, so a bare "present" test would pass at once. In the first cycle after `001F`, bit 12 was set and bit 0 clear (`5037`, `0004`); bit 0 returned in cycle 10. So both rules fit this record: `AFTER_ABSENCE`, which needs that observed absence, and `AFTER_ACCEPTANCE`, which needs acceptance to withdraw the earlier report. `AFTER_ABSENCE` is the conservative choice: a move shorter than one cycle then ends `NOT_OBSERVED` at its deadline, never complete. `6041` bit 15 behaves the same way.

  *Correction, 2026-11-03.* This bullet first said "only the after-acceptance rule fits". The trial 111 bytes (`20261103T1028Z-position-node1`, operations 24 and 33) show the absence that `AFTER_ABSENCE` needs, so that claim was wrong.

**Scale.** `2201` reads 1000 and a target of 250 turned about a quarter turn, so **1000 command pulses per revolution** on this configuration. `6064` reported 250 at the end, the command's own units, not `2501`'s 4000. A positive target turns **counter-clockwise** seen from the shaft end. At 2 rpm, `6064` advanced about 33 pulses a second, consistent with rpm being motor turns per minute. `606C` read 1 while moving at 2 rpm, so its unit is not established.

**Rearm.** `000F` after completion only cleared bit 12. Nothing moved. This answers packet 19's falling-edge question for this firmware, absolute mode.

**HALT from `001F`.** It stopped a 2 rpm move within the 500 ms ramp, and the shaft stayed put. This is the DEC-8 row with `predecessorDocumented=0`, now with one specimen measurement, at 2 rpm and 1000 pulses/rev. MotorControl-RS HW-14's overshoot appeared on another drive at high command rates, untested here.

## Defect found and fixed

| Defect | Cause | Fix | Regression | Retest |
| --- | --- | --- | --- | --- |
| No enable could complete: the enable ended at word 6 with `OBSERVATION_SCOPE` | The rule compared every `6041` bit above 0..2 across the sequence, and bit 10 follows the enable state itself | `e17271a`: bit 10 (`STATUS_EN86_ZERO_SPEED`) is not compared; `606C` still decides velocity, and every other bit still ends it | `zeroSpeedBitFollowsTheEnable`, replaying the measured bytes; fails without the fix | Trials 108 and 118: both enables succeeded |

The position rule (`6064` unchanged during an enable) stopped trial 105 correctly: a position change during an enable may be motion. Do not turn a released shaft by hand before an enable.

## Resources during the actions

From each route's `TRIAL_RES`, on the 12,288-byte owner task:

| Measure | Value |
| --- | --- |
| Least stack ever free | 8,492 bytes, reached during the moving HALT (trial 113), so at most 3,796 bytes used. The enable reached 8,700 (trial 108) and reads alone 9,056. These are high-water marks since boot, one image |
| Largest service gap | 5 ms in every enable and in the stop; 3 to 4 ms in reads, the move and the rearm |
| Record loss | `lost=0`, `dropped=0` and `tailLost=0` on every route; no console loss in any capture |
| Internal heap | 73,640 bytes free through node 1's routes. Node 2's first route allocated its `AxisSession`: free fell once to 50,060 and the largest block from 49,152 to 25,600, then stayed constant. The drop, 23,580 bytes, is the 23,576-byte session (`check_stack.py` size table) plus the allocator's header: one allocation per node, never freed, by design, not a leak |
| Controller counters | **Not retained**: the route records had no TEC or REC. Added after the session (`TRIAL_RES tec0 rec0 tec1 rec1`); the next campaign records them. Each route's traffic already copies controller reports and TX reports: no controller report occurred, and all 543 TX reports succeeded |

## Smaller findings

- A route sent while a record is held is refused before sending, but the host still counts it. Release after every route.
- The `position` route parses up to 16 cycles, but more than 15 can never fit the 128-terminal record, and the firmware refuses it as `capacity` after the host has counted the send. With a planned stop the limit is lower: 8 cycles with a 2 s stop polled every 250 ms.
- A ping sent while the S2 was still booting after a reflash got no answer, and the capture began with one `0xB8` byte of UART reset noise. The parser now accepts up to two such bytes before a first boot line, keeping the raw text.

## Still owed

- **Stationary stops:** `010F` and, on a new power-up, `000B`, both from idle `000F` (packet 15 P4, packet 16 step 3). No admissions were left.
- **`000B` on a released state** (packet 16 step 4), before the best-effort path is relied on.
- **A move on node 2,** with node 1's retained outcome checked unchanged (packet 22 step 6). Two-node work with an urgent stop on the other node has no route: one route runs at a time.
- **Packet 20's `move` route** through `AxisSession::requestMove`, with acceptance `6041` mask and value `0x1000`, completion `2100` mask and value `0x0001`, and rule `AFTER_ABSENCE`, reviewed first. The library's own move lifecycle has not run on a specimen; trial 111 was the application-local `position` experiment.
- **Relative moves:** what `005F` adds to (packet 21). Every relative request stays refused.
- **Error history:** `1003` has never been read; its read effect needs the fault LED observed before and after (backlog H03). The `1001` and `603F` reads of trial 101 had the LED off before and after.
- **Physical questions:** whether word 7 pulls the shaft back to its commanded position after a manual displacement; `606C`'s unit.
- **Next campaign:** these need a new ledger and record, and a reflash: the counter fix above is not on the board.

**Drive state at the end:** node 1 energized at `6064=316` with latest word `010F`, node 2 enabled at 0 with `000F`. Both hold torque until powered off.
