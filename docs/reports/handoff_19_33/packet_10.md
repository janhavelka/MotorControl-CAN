# Packet 10: shared evidence, settings generations and state interpretation

Completes audit W8 with A-02, A-03, A-04, A-06, A-07, A-08, A-10 and A-12, and reopens prompts 17, 19, 20, 30 and 31. Host and fake evidence only. **Closing W1 to W8 qualifies no release command, moving stop, relative wire base or physical scale.**

## Delivered

See the [changelog](../../../CHANGELOG.md) for the migration.

- **A-04.** `AxisReservation`, as its own mechanical commit `1e0e379`. One `Detail::acknowledgedControlWrite`. The predicates stay with their callers: settings needs `000F`, an age bound and a nonrunning report; a stop accepts DEC-8 predecessors with no age limit and may be running.
- **A-10.** Private `src/canopen/SdoEvidence.h`, replacing all eight overlap copies in `src/`.
- **A-03.** `interpretStatusword`, which `getStateInterpretation` calls, with the reviewed `2100` masks beside it. A decoder is not admission or completion evidence.
- **A-08.** `SERVICE_TIMEOUT` for an early queue or response timeout. `DEADLINE` now means only the overall cutoff. Possible transmission stays independent of both.
- **A-07.** `CORRELATION_LOST` withdraws earlier typed blocks and keeps historical terminal bytes.
- **A-06.** The generator's null status message is fixed and its output regenerated.
- **A-12.** One read plan table and a getter entry helper. `Reads.cpp` falls from 1,069 to 656 lines, with new `DriverReads.cpp` and `Tuning.cpp`.
- **A-02.** Explicit settings calls advance configuration confidence. An operation's own writes do not, and retire retained `MOTION_PARAMETERS` observations through `retireMotionParameters`.

## The hook packet 20 calls

`retireMotionParameters(retained, current, work)`, once per retained context including a finished settings context's `readback`, per own download, after queue admission and before possible TX. Do not transmit if it refuses. It takes every operation-owned write at its EDS width: 6081, 6083, 6084, 6098, 6099:01, 6099:02, 609A and 607C retire, and 607A and 60FF retire nothing. It never moves the generation.

Prompt 36 still owes its own ruling on 607C: retiring on it is conservative, because its effect on the group is unestablished.

## Closure audit of W1 to W8

An independent reviewer checked each step and finding against code and a real test. W1 to W8 are done for host scope, except:

| Item | Status |
| --- | --- |
| DEC-3 marker | Partial by design: no real route can reopen until Q18 |
| C-10 | History and stop only; other operations join in their packets |
| C-15 | Live stop stack moves to packet 16 |
| W7 step 4 | "No task-local contexts" is vacuous: no firmware holds a session yet |
| G-04 | Moved to W9 |
| C-09 remainder | Moved to prompt 38, and now stated there |
| A-15, A-16, A-18, A-19 | Not in W1 to W8. No owner was found, so they stay as open audit findings |

The reviewer's findings, fixed here: DEC-2's "independent physical stop stays mandatory" was missing from the guide, owner contract and Doxygen; the hook refused the writes prompts 35 and 36 own; no test decoded a status-only or TPDO sample.

**The audit tells packet 9 to report A-05 and DEC-6 to the RS owner.** The handoff forbids contacting maintainers, so the deltas are recorded in the [axis contract](../../axis_contract.md) and [packet 9](packet_9.md) only.

## CI

Clang failed `stop_action` on every commit from packet 7 on. CI named the line once a diagnostic was added: `byte 301 of 4896 changed` at a compare of two separate stack objects, which are padding bytes on the host layout. So this was a test defect rather than a rejected call writing its output. Packet 8's "no rejected call changed a byte" had GCC evidence only until then.

## Override

The [audit's](../../prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md) hold lifts only when this commit's full verifier and CI pass. See the execution table for the outcome.

**These stay open after closure:** Q02, Q04, Q05, Q10, Q11, Q12, Q18 and the unlisted Q20 rows; release command, moving stop, relative wire base, physical scale; whether `000B` stops the drive or could enable it; every DEC-8 row; the live stack during a stop; packet 3's seven TWAI questions. No firmware holds an `AxisSession`, so no firmware can issue a stop yet.

## Adversarial tests

Two new suites run the real code: `evidence_corruption` (52,138 checks, about 65 field corruptions of each terminal, every rejection byte-identical) and `evidence_timeline` (over 3.5 million checks, including every status word in all five layouts and an index sweep of the hook). 121 mutants: 118 killed and 3 reasoned unreachable. Their findings, fixed here:

| Finding | Fix |
| --- | --- |
| `advanceBestEffortStop` hand-checked its terminal, and accepted a wrong step, request, budget, node epoch or `queued` | It compares the whole work with `sameSdoWork`, and the node epoch, effect and admission time |
| `validateEscalation` accepted a `HALT` whose later read had been withdrawn, reachable through `AxisSession` | Refused; the best-effort path stays available |
| `terminalConsistent` allowed an abort code on a timeout, and a staged client abort on an answered service | Refused |

`prepareDriverSettings` accepts a failed, uncertain state read whose STATUS and MONITORS blocks did complete, where `validateStop` refuses the same read. Left as is: complete blocks stay usable under settings' stricter predicate. Packet 12 should weigh it.

## Open

- `retireMotionParameters` has no production caller until packet 20. A context not passed to it is unreachable, so keep one retained context per axis.

## Sibling refresh

| Sibling | Revision | Dirty | Symbol | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `e59c5ca` | Clean | `ActionEvidence.h` `Detail::classifyWrite` | Adopted: a private profile header with no outcome policy |
| MotorControl-RS | | | `Reads.cpp:219-229` | Adopted: the overall cutoff wins |
| MotorControl-RS | | | `ReadOutcome`, `ActionOutcome` | Differ: RS has no `SERVICE_TIMEOUT`, because it has only a deadline event |
| MotorControl-RS | | | `DriverSettings.cpp:320-357` | Rejected: it matches a generation on observations, and A-02 forbids advancing it for a move's own writes |
| INA228 | `73f9548` | Clean | `invalidateHardwareState` | Adopted: no I/O, less trusted and never more, evidence retained. Rejected: a driver-object flag, as the core has no driver object |
| ADS1115 | `d613a13` | Clean | `hardwareConfigDirty` | Adopted: a dirty observation needs a fresh acquisition. Rejected: compatibility aliases |
| FieldCore-node | `5ad9292` | Clean | No such names | No integration impact; read-only |

## Checks

See the [execution table](README.md).
