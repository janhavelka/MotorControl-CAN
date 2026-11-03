# Packet 18: device-zero behavior and reference invalidation

Resumes the device-reference part of prompt 32. Host and fake evidence only: **no specimen has run a counter zero.** Both specimens read `6064=0`, so a first physical run could show no change.

## Delivered

See the [fault and reference guide](../../fault_reference_actions.md) and the [changelog](../../../CHANGELOG.md).

- **Migration:** `prepareSetDevicePosition` now needs a fresh FEEDBACK block, and `606C` must read raw 0, as project policy. A nonzero request still returns `UNSUPPORTED` right after the overlap check.
- **`CounterBaseline`** keeps the admission `6064` and `606C`. An already-zero baseline means a later raw zero shows no change.
- `test/counter_clear_test.cpp`, 8,422 checks: each of `0, 1, 0` failing at 7 stages, followed into later reads, prepared-target snapshots and owner admission. The shared rig moved to `test/support/ReferenceRig.h`.

## Decisions

- Coordinates are withdrawn exactly once, at the first possible `2302=1`. Configured limits, units and protective inputs are preserved: this withdraws validity, it disables nothing.
- A trigger refused by the guard or cancelled in the queue leaves everything intact, distinguishable from a possible effect.
- All three writes stay on the original binding. Owner epoch sync happens only after results are released.
- An uncertain route blocks new work. No epoch advance, release or new reservation reopens it.
- **Host finding:** a reply processed exactly at the cutoff loses. The owner ends the request `TIMEOUT` with the frame out, and the route is quarantined. That is G-13's rule, applied.
- A raw zero never publishes home or a host origin.

## Independent review

A read-only reviewer of packets 17 and 18 found six defects, all fixed. The main one: a late reply quarantined the route before the later-admission checks ran, so the intact and resolved-withdrawn branches were never tested. The others were overclaims left in prompt 32's report, a wrong page citation, wording, a guide row condition and an untested invariant. It confirmed `oneMayRemain` on every termination path and every vendor page and EDS section cited.

## Physical plan, not run

Stationary, alarm-free, mode 1, `606C=0`, a shaft mark, one drive and the cutoff in hand. Record `607A`, `6064`, `606C`, the monitor words and reference metadata, then `2302` `0, 1, 0`, sync, read the new epoch and check the shaft. Abort on motion, alarm, uncertain write or bus fault. A nonzero baseline needs a separately qualified move, never an arbitrary counter write.

## Open

- **Closed by packet 20:** move admission checks `configurationGeneration`, so a counter zero invalidates prepared moves (`counterZeroInvalidates` in `test/move_absolute_test.cpp` and `test/move_relative_test.cpp`).
- **No application caller.** `AxisSession` has no counter-zero member, and its configuration callback must not withdraw a second time on counter sync. Tracked as backlog S12.
- `s2_bench` has no route (S12). Step 4's finite experiment exists only as the plan above and in the [guide](../../fault_reference_actions.md).
- Q14's physical qualification.

## Sibling refresh

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `6499571` | Clean | Adopted: zero-only `UNSUPPORTED` (`Actions.cpp:31-37`); raw kept beside a known flag. Rejected: withdrawal only after `txAccepted` (`ProbeApp.cpp:2272-2279`), where CAN withdraws earlier; the `positionClearQualified` operator flag |
| FieldCore-node | `5ad9292` | Clean | Adopted: reset or origin does not establish a reference (`contracts/MotorControl.h:13-15,103`) |
| ADS1115 | `d613a13` | Clean | Adopted: `INDETERMINATE` with `hardwareStateUncertain` (`include/ADS1115/ADS1115.h:51,134`), as a possible `2302=1` stays uncertain. Rejected: `OperationKind::RECOVER` replay |
| INA228 | `73f9548` | Clean | Adopted, the closest seam: a destructive accumulator reset withdraws accumulated validity as soon as the write returns (`_accumulationReady = false` in `JobPhase::ACCUMULATOR_WRITE`, `src/INA228.cpp:1368`), and a failed side-effecting write counts as possibly applied (`INA228::_failJob`, `:777`). Counter zero withdraws coordinates at the first possible trigger, earlier still |
| PCA9555 | `414e0f6` | Clean | Adopted: ambiguous writes stay recorded (`uncertainPairs`, `include/PCA9555/PCA9555.h:186`) until a matching full readback (`VERIFY_IMAGE`) |

## Checks

The tests passed the local gates recorded for `01190cf` in the [execution table](README.md). CI failed at `01190cf` on a test registered twice, which is unrelated to this packet. The full verifier then ran on the exact commit `f1116b1` in a clean detached worktree, which includes every test named here, and CI passed at `f1116b1`.
