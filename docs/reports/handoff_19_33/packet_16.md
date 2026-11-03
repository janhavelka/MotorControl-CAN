# Packet 16: qualify and finish the stop path on the actual fixture

**Unattended session, 2026-11-03.** On the user's written waiver, with nobody at the bench, the `stop` route ran twice: a stationary `000B` from `000F` on node 2 (trial 124, step 3's quick stop, from a library move's rearm) and from the halted `010F` on node 1 (trial 126). Within one cycle both drives left operation (low bits `000`) and released the motor: `6041=8050`, `2100=0011` and `0050`, `0010`. Neither moved; node 1 stayed at 316 with its target 750 retained. The library ended both `OBSERVATION_SCOPE`; a `QUICK_STOP` now completes on that report and keeps the release (`quickStopReleaseIsTheMeasuredTerminal`), **not yet retested on hardware**. Step 6: the least free stack stayed at the move's 7,024 of 12,288 bytes, largest service gap 5 ms. Still owed: step 3's stationary `010F`; step 4, `000B` with no known predecessor on a released state (the library refuses an idle released drive on both paths, so the released state `000B` leaves does not open it); step 5's moving `000B`; and the retest. See the [unattended bench report](../2026-11-03_unattended_bench.md).

**Attended session, 2026-11-03.** A moving HALT stop (`010F` from `001F`) ran on node 1 and was observed: the 2 rpm move stopped within its ramp and stayed stopped, with no creep over 26 s. The stationary `010F` and `000B`, and `000B` on a released state, did not run: the campaign cap was reached. Step 6 for the moving stop: least stack free 8,492 of 12,288 bytes, largest service gap 5 ms. Afterwards `6041=0437` and `2100=0000`: the halted move reads neither completion signal. See the [attended bench report](../2026-11-03_attended_bench.md).

**Before that session the physical scope had not run.** Qualifying a stop needs an enabled drive, then a moving one, and those need an attendant with the independent cutoff in hand. The bench ran an unattended communication soak overnight. No drive has been enabled, moved or stopped by this library. Prompt 31 stays Partial.

## What is ready

- **Software contract**, verified rather than assumed:
  - Packet 7's lifecycle, rechecked by the closing audit against code and tests: DEC-8 predecessors with `predecessorDocumented`, session history, best-effort `000B` that never succeeds, hold across one recovery, upward-only escalation, withdrawal winning over a late observation.
  - Packet 15's enable path ends with the acknowledged `000F` that a sequenced stop needs. Packet 20's move holds `001F` and keeps its last cycle, so a mid-move stop can be sequenced as `010F` from `001F` with `predecessorDocumented` false.
- **The packet's adversarial list through the real fake owner** exists, in `stop_action`, `axis_session`, `can_bus_recovery`, `enable_action` and `move_absolute`: in-flight poll then stop, lost trigger reply with an indistinguishable delayed ACK, lost poll while a stop waits, two nodes with one unread stop result, full capacity, stale queued targets, uncertain controller retirement, escalation, cancel, fault, reboot, exact deadline, fresh against expired observations, and a copied old controlword. A never-transmitted stop held across recovery stays distinct from a possible one.
- **Bench routes** in `examples/s2_bench/BenchRoute.cpp` (in the attended session `mode1`, `enable` and `position` with a named HALT ran; `stop` ran in the unattended session): `stop` (explicit kind and policy, path and every terminal recorded, task stack high-water during the action), plus `mode1`, `enable` and `position` to reach the states a stop needs. Each needs an attended trial identity, used once, with motion caps.
- **Receive bounds now qualify SDO service** on the S2 (see the [receive-time guide](../../adapters/twai_receive_time.md)), so a sequenced stop no longer needs a heartbeat first.

## Sources reopened on 2026-11-03

Rendered from the retained PDFs with PyMuPDF (one-based PDF pages):

| Page | What it shows |
| --- | --- |
| EN86 p12 | `6->7->15` enable, `15->31`, `15->95`, `15->63`, `15->127`, and `15->11` "Emergency stop". **No `271` row.** Status bit 7 is alarm |
| ZH86 p12 | The same, plus `15->271` stop, `15->11` emergency stop and `15->128` alarm reset. Status bit 3 is alarm, bit 14 running |
| OP86 p10 | The ZH86 rows, under a CL57-C/OL57-C running header (printed page 8). That applicability caveat is already recorded in the [drive actions guide](../../drive_actions.md) |

EDS `[6040]`: U16, WO, PDO-mappable, default 0. `[605D]`: I16, RW, default 1, with no option meanings. `[605A]` and `[6085]` are absent. This matches the [stop guide](../../stop_actions.md) table, so no stop row changes.

## Warning for the attended sequence

Both specimens read `6041` low bits 7 and `2100` released clear at power-up, before any enable from this library. After step 1 below (mode 1), a stop requested **before** step 2's acknowledged `000F` has no session word. It therefore sends best-effort `000B` to a drive this library never enabled, and what `000B` does there is step 4's open question. Do not use a CAN stop in that window; rely on the cutoff.

## The attended sequence still owed

In order, one drive, node 1 first, cutoff in hand:

1. Packet 15 P2: `6060=1`, with fresh mode, status, monitor, error and feedback, and a shaft mark.
2. Packet 15 P3: enable, with holding torque checked by hand before and after each word.
3. Stationary stops from idle `000F`: `010F`, then on a new power-up `000B`, each with its acknowledgement, the controller's not-running report and a physical observation separately recorded.
4. **Before relying on the best-effort path:** `000B` on an independently evidenced released state, to establish whether it enables. Nothing here creates a released state; if none is available, that gate stays.
5. A moving stop jointly with packet 19's isolated move, post-trigger `000B` or `010F` from `001F`. No `000F` preamble.
6. The live task stack high-water mark and the worst service gap during each stop. The `stop` route records both (`RouteRecord::maxServiceGapUs` and the before and after resource samples).

Each best-effort stop leaves its route unresolved, and no real route can reopen until Q18 closes, so each costs that axis's route until an MCU restart. Plan trials accordingly.

## Unsupported physical guarantees

Stop latency, braking, retained torque, queue disposition, and any safety function. **An independent physical stop stays mandatory.**

## Sibling refresh, stop seam

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `a13f767` (`src`, `include` unchanged since `6499571`) | Clean | HW-14 in `docs/reports/2026-11-02_standalone_promptset_audit.md`: on the ESS drive, a normal stop near the end of a fast move can overshoot and creep until a fast stop. **Rejected as CL86-C evidence**: it is ESS firmware. **Adopted as a test case**: the attended moving stop must observe whether `HALT` leaves the shaft moving. Escalation stays manual and upward only (`AxisSession::requestStop`, `QUICK_STOP` over a running `HALT`), never automatic |
| ADS1115 | `d613a13` | Clean | Adopted: an idle request is complete only after a verifying read (`OperationKind::SHUTDOWN`, `include/ADS1115/ADS1115.h:39`), as a stop's acknowledgement is not its completion |
| INA228 | `73f9548` | Clean | Adopted: a failed side-effecting transfer counts as possibly applied (`INA228::_failJob`, `failedSideEffectingTransfer`, `src/INA228.cpp:777`), as a possibly-sent stop stays unresolved |
| PCA9555 | `414e0f6` | Clean | Adopted: ambiguous-write evidence is kept per pair (`uncertainPairs`, `include/PCA9555/PCA9555.h:186`), as stop uncertainty is kept per axis |
| FieldCore-node | `5ad9292` | Clean | Adopted: a stop is explicit urgent intent with its own lane and history (`RodCycleCoordinator::requestStop`, `stopHistory`; `Rs485RuntimeIngress::copyPendingStop`), as the owner's per-channel urgent slot |

## Checks

Packet 16 changed no library code: the `stop` route was committed with packets 17, 18 and 20 in `01190cf`. Its fake-board tests (`bench_route_test`) passed with the local gates recorded for that commit in the [execution table](README.md). CI failed there on test registration, which `f1116b1` fixed, and CI passed at `f1116b1`. No separate review ran for packet 16. The packet 7 lifecycle was rechecked against code and tests by the closing audit on 2026-11-03.
