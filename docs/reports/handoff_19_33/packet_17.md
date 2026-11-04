# Packet 17: explicit alarm clearing and retained fault evidence

Resumes the alarm-clear part of prompt 32. Host and fake evidence only: **no retained read has reported an alarm on either specimen, so no clear has run.** A physical trial needs a naturally present, identified fault. Since `e7f28ac` the application caller and the `clear-alarm` bench route exist; through the [2026-11-04 session](../2026-11-04_bench.md), every route read of `2100`, `1001` and `603F` on both nodes still showed no alarm.

## Delivered

See the [fault and reference guide](../../fault_reference_actions.md) and the [changelog](../../../CHANGELOG.md).

- **Transition table**, written first, in the guide.
- **Unresolved fault identity is explicit.** `AlarmIncident::codeLink` is always `UNRESOLVED`: a retained `603F=FF05` plus a later alarm does not show the same resettable event, and the p16 to p20 link is an inference (D-26). The overclaiming refusal text, "documented resettable FF05", is gone. No `FF02..FF04` from LED counts.
- **`valueOneMayRemain`** is set when `2301=1` may have landed and the final `0` has no positive acknowledgement. An SDO abort counts as a possible effect. Nothing clears it or resends.
- `test/alarm_clear_test.cpp`, 9,774 checks through the real owner, with exact frames and no enable, motion, save, restore, NMT or retry.

The independent review covered packets 17 and 18 together; its findings and fixes are in [packet 18](packet_18.md#independent-review).

## Decisions

| Rule | Detail |
| --- | --- |
| Admission | Unchanged: mode 1, `2100` running clear and alarm set, raw `603F=FF05`, ERRORS and ERROR_HISTORY fresh. `FF05` only narrows the attempt |
| Writes | `2301` U16 `1` then `0`, as library policy. No arming `0` first: D-27 says no code change without evidence |
| Completion | `2100` alarm clear with mode 1 and running clear, then one ERRORS read. `603F` need not read zero; RS's rule requiring it is rejected |
| Persisting alarm | MONITORS and STATUS re-polled until the deadline, with no rewrite. Consuming objects are never polled |
| Controlword reset route | Still absent: bit 7 and word 128 are a separate question |
| EMCY during clearing | The application withdraws through `noteActionWithdrawal`. An EMCY `0000` is kept raw and is neither a fault nor recovery evidence |

## Physical plan, not run

Only for a naturally present, identified alarm, one drive, cutoff in hand. Record the LED count, raw `603F`, `1001`, `1003`, `2100`, `6041`, `6061` and an EMCY copy, then one `prepareClearAlarm`: `2B 01 23 00 01 00 00 00`, then `2B 01 23 00 00 00 00 00`, recording TX, ACK, later cycles and the single ERRORS read. Abort with no further write on a running report, mode change, uncertain write, lost evidence or bus fault. A persisting alarm is the result. One attempt per incident. **Never induce a fault to get one.**

## Open

- **Application caller, delivered in `e7f28ac`** (backlog S12): `AxisSession::requestClearAlarm`, with `clearBlocked()` set for the session object's life when a clear ends with `valueOneMayRemain`, surviving result release, as RS `actionConflicts` does. The session has no EMCY view: the application copies the incident's EMCY before the request and routes a later one to `cancelClear`. See the [guide](../../fault_reference_actions.md#application-caller-axissession).
- **Bench route:** `clear-alarm` (`TC`) in `examples/s2_bench`, refused unless `2100` reports the alarm. It has not run: no alarm has occurred.
- **Still owed:** the physical plan above, on a naturally present alarm, and Q13's physical qualification.

## Sibling refresh

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `6499571` | Clean | Adopted: no replay; separate write, observation and failure evidence (`ess_rs/Actions.h:52`). Rejected: completion requiring the alarm word to read 0 (`Actions.cpp:64-65`) |
| ADS1115 | `d613a13` | Clean | Adopted: an uncertain effect ends in its own terminal that keeps the uncertainty (`INDETERMINATE`, `hardwareStateUncertain`, `include/ADS1115/ADS1115.h:51,134`). Rejected: `OperationKind::RECOVER`, which replays the desired profile; a clear is never replayed |
| INA228 | `73f9548` | Clean | Adopted: first dirty cause, less trusted and never more |
| PCA9555 | `414e0f6` | Clean | Adopted: only a matching full readback revalidates |
| FieldCore-node | `5ad9292` | Clean | Rejected: reusing `RodCycleCoordinator` auto-reconcile for faults |

## Checks

The tests passed the local gates recorded for `01190cf` in the [execution table](README.md). CI failed at `01190cf` on a test registered twice, which is unrelated to this packet. The full verifier then ran on the exact commit `f1116b1` in a clean detached worktree, which includes every test named here, and CI passed at `f1116b1`.
