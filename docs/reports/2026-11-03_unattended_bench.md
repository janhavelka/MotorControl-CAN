# Unattended bench session, 2026-11-03

The second motion campaign, run with **nobody at the bench** on the user's written waiver. It covers what the [attended session](2026-11-03_attended_bench.md) left owed: the library's own move lifecycle (packet 20), a stationary quick stop (packet 15 P4, packet 16 step 2) and a quick stop from a halted state (packet 22 step 5). **One specimen and one firmware each; nothing here qualifies a firmware or a production capability.**

## Campaign record, written before transmitting

| Item | Value |
| --- | --- |
| Waiver | User, 2026-11-03, verbatim: "Do all the tests - i will not be present - i take the reponsibility - nothing can happen, motors bolted, free shafts". It replaces the handoff's attendant and independent drive-power cutoff (COMMON "Physical test authorization") for this campaign only. Nobody observes a shaft or an LED; the runner stores every such answer as `unattended: not observed` (`--unattended-waiver`) |
| Fixture | ESP-Prog-2 on COM15 (serial `94A990398B40`), ESP32-S2, two CL86-C drives, nodes 1 and 2, 1 Mbit/s; motors fixed to the table, shafts free (user) |
| Image | `e17271a`, ELF `024a653a…`, the attended session's boot, **not reflashed**: uptime 17,401 s and `motion=8` at [ping](../bench/20261103T1605Z-s2-ping/rx.txt). A new boot would lose both sessions' latest controlwords, and no enable could then be admitted: both drives have had motion writes since their last power-up, so `NO_MOTION_WRITE_RECORDED` cannot be stated. The counter fix of `0e342b2` is therefore not on the board, and no record of this campaign holds `tec`/`rec` |
| Budget | Firmware, this boot: 4 motion admissions left of 12. Ledger [`20261103b-campaign.jsonl`](../bench/20261103b-campaign.jsonl), new. Planned: 3 admissions, one spare kept for a retest after a defect, no adaptive increase |
| Identity | Both nodes, retained reads: `1000=00040192`, `1009=0C`, `100A=20` (one byte), `1018:01..03` = `1000`, `1`, `0`, `2000=0400`, `2001=0200` |
| Expected start | Node 1: mode 1, `6041=0437`, `2100=0000`, `6064=316`, latest word `010F` (the HALT of trial 113), target `750` retained. Node 2: mode 1, `8437`, `0001`, `6064=0`, latest word `000F` from its enable (trial 118) |
| Scale | Node 1: 1000 command pulses per revolution, positive is counter-clockwise (trial 111, attended). Node 2 is not measured: its move uses the same native target, and its turn is bounded by time and speed, not by that scale |
| Observation | Drive reports only: `6041`, `2100`, `6064`, `606C`, `1001`, `603F` through the routes |

Every case is preceded and followed by `readiness-actions` with `--fault-led unobserved`, and every route is released before the next.

| Order | Trial | Node | Route | Values | Bound |
| --- | --- | --- | --- | --- | --- |
| 1 | 121 | 2 | `move`, packet 20 | 0 → 250 absolute `STEPS`, 2 rpm, ramps 500/500 ms, deadline 20 s, poll 2 s, record 111, acceptance `6041` `0x1000`/`0x1000`, completion `2100` `0x0001`/`0x0001`, `AFTER_ABSENCE`, the identity above, no planned stop | Node 1's scale gives a quarter turn. Whatever node 2's scale, 2 rpm for at most the 20 s deadline is at most 2/3 of a turn before the move ends |
| 2 | 124 | 2 | `stop` quick `000B`, stationary | From the move's own rearm `000F`; deadline 2 s, no best effort, poll 200 ms | The documented arrow `000F→000B`. No motion expected |
| 3 | 126 | 1 | `stop` quick `000B`, from the halt | From `010F`, which `stopPredecessorAllowed` admits for a quick stop only; deadline 2 s, no best effort, poll 200 ms | Not a documented arrow. If the drive resumed its retained target 750 instead, that is 434 pulses, under half a turn at 2 rpm |

**Abort conditions.** Stop the campaign at any alarm, nonzero `1001` or `603F`, EMCY, lost evidence, refused route or unexpected state, and keep the record as it is. No retry, enable, fault clear, mode change, NMT command or cleanup follows. A move left without an observed completion stays fenced.

## Results

Ledger: 9 sends, **3 motion admissions** (`G2`, `S2`, `S1`); the firmware's boot count went from 8 to 11, so one admission of this boot is left unused. Every read had `1001=0`, `603F=0` and no alarm. No EMCY, no lost record, no console loss.

| Trial | Node | Route | Result |
| --- | --- | --- | --- |
| [119](../bench/20261103T1610Z-p1-node2-119/rx.txt), [120](../bench/20261103T1610Z-p1-node1-120/rx.txt) | 2, 1 | Reads | Both as expected. Node 2: `8437`, `0001`, mode 1, `6064=0`, `607A=5000`. Node 1: `0437`, `0000`, `6064=316`, `607A=750`, unchanged after 5.5 h |
| [121](../bench/20261103T1611Z-move-node2-121/rx.txt) | 2 | `move` | **The library's move lifecycle ran to completion**, `SUCCEEDED` with completion observed and its own rearm. `607A`, `6081`, `6083`, `6084` and `001F` acknowledged in order. Cycle 1, 4 ms after the trigger's acknowledgement: `5037`/`0004`, acceptance present and completion absent, as on node 1. Cycle 5, 8.2 s after the trigger: `9437`/`0001` at **`6064=251`**. Then `000F`, acknowledged |
| [122](../bench/20261103T1611Z-p1-node2-122/rx.txt), [123](../bench/20261103T1611Z-p1-node1-123/rx.txt) | 2, 1 | Reads | Node 2: `8437`, `0001`, **`6064=250`** 23 s later; `6081=2`, `6083=6084=500`, `607A=250`. Node 1 unchanged by node 2's move |
| [124](../bench/20261103T1612Z-stop-node2-124/rx.txt) | 2 | `stop` quick, from the move's `000F` | `000B` acknowledged. The first cycle, 6 to 24 ms later: `2100=0011` (in place, **released**), `6041=8050`, mode 1. The library ended it `OBSERVATION_SCOPE`, as its contract then required for any release |
| [125](../bench/20261103T1613Z-p1-node2-125/rx.txt) | 2 | Reads | `8050`, `0011`, `6064=250`: the same state a minute later |
| [126](../bench/20261103T1613Z-stop-node1-126/rx.txt) | 1 | `stop` quick, from `010F` | `000B` acknowledged. First cycle: `2100=0010` (released, in place clear), `6041=0050`, mode 1. `OBSERVATION_SCOPE` |
| [127](../bench/20261103T1613Z-p1-node1-127/rx.txt) | 1 | Reads | `0050`, `0010`, **`6064=316`**, `607A=750`: no motion toward the retained target |

## What the drives showed

**The move on node 2** reproduces node 1's bits exactly: `6041` bit 12 from the first cycle, `2100` bit 0 absent while running and back at completion. So packet 19's evidence, measured on node 1, held on a second specimen of the same identity. The difference is the final count: completion was reported at 251, one pulse past the target, and the drive read 250 when next asked. Whether it overshoots and returns, or the counter dithers inside the in-place window, is not established. `AFTER_ABSENCE` judged completion from `2100` alone, so the move succeeded either way.

**`000B`, stationary.** On both drives, from `000F` and from the halted `010F`, `000B` took the drive out of operation within one cycle: low bits `000`, bit 6 set (`0x50` low byte, read but not interpreted by the library) and **`2100` released, so the motor holds no torque**. There is no quick-stop-active state that keeps torque: this firmware disables the drive. Bit 15 stayed as it was (set on node 2, clear on node 1 after the halt). Node 1 did not resume its retained target. The drives stay released until an enable, which needs a power cycle first, because both have had motion writes since power-up.

## Defect found and fixed

| Defect | Cause | Fix | Regression | Retest |
| --- | --- | --- | --- | --- |
| A quick stop could never complete on these drives: both ended `OBSERVATION_SCOPE` | The stop rule treated every released report as outside its scope, a provisional policy written before `000B` was measured | `QUICK_STOP` only: a not-running cycle with `2100` released, low bits `000`, mode 1 and no alarm is `OBSERVED`; `monitorObservation` keeps the release. A `HALT` still refuses a release | `quickStopReleaseIsTheMeasuredTerminal`, replaying trials 124 and 126 and five neighbors; fails without the fix | **Not retested on hardware.** Both drives are now released and no enable can be admitted until they are power-cycled |

## Resources during the actions

| Measure | Value |
| --- | --- |
| Least stack ever free | **7,024 bytes**, reached during the move (trial 121): at most 5,264 of 12,288 used. The stops added nothing below it |
| Largest service gap | 5 ms in the move and both stops, 4 ms in reads |
| Record loss | `lost=0`, `dropped=0`, `tailLost=0` on every route |
| Internal heap | 50,060 bytes free and largest block 25,600 throughout: no change since the attended session |
| Controller counters | Not on this image (see the campaign record). Every TX report in the copied traffic succeeded and no controller report occurred |

## Still owed

- **Retest the quick stop** with the fixed library: needs a power cycle of both drives, then a reflashed board, the enable (an attendant must observe the power-up shaft), and a stationary `000B`.
- **A stationary `010F`** from `000F`: no drive had that predecessor left on this boot.
- **`000B` during a move**, and `000B` with no known predecessor (packet 16 step 3; the library refuses an idle released drive on both paths, so the released state reached here does not open it).
- **Relative moves** (packet 21) and the remaining settings, alarm-clear and counter-zero trials, as listed in the [attended session](2026-11-03_attended_bench.md).
- **Word 7 pull-back** after a manual displacement, and the 251/250 completion count, need an attendant or a dedicated measurement.
