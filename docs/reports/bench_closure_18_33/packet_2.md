# Packet 2: finite-position admission and regression gaps

[Packet 2](../../prompts/bench_closure_18_33/2_position_regressions.md), host work only, on 2026-11-06. No hardware was used and no physical evidence changed. Back to the [execution index](README.md).

## Start state and sibling audit

Branch `promptset/motorcontrol-can` at `e8b449d` when committed; the work began at `311b682`, and none of the commits since touches its files. Siblings, HEAD and dirty state checked on 2026-11-06, all clean and unchanged since the authoring rows: MotorControl-RS `cd27b12`, BME280 `48b8214`, FieldCore-node `5ad9292d`.

- **MotorControl-RS** `src/Axis.cpp` `preparePosition`, the authority for common intent: adopted as the producer of the retained vectors (item 5), compiled from that revision's blobs. Its relative-basis and witness admission was read for [packet 21](../handoff_19_33/packet_21.md) and is unchanged at this HEAD; the Lichuan `RelativeBase` evidence rule stays.
- **BME280** partial-setting evidence: HEAD unchanged since the authoring row, not reread; nothing imported.
- **FieldCore-node** reference withdrawal and history: HEAD unchanged since the authoring row, not reread; read-only, no integration.

## Requirements

| Item | Status | Where |
| --- | --- | --- |
| 1 Urgent stop on one node beside the other node's queued, adapter-held or handed-off parameter or trigger, both node orders | Met: exact transmitted writes, asynchronous RX unaffected, isolation, both retained outcomes, no replay | `move_relative_test.cpp` `urgentBesideOtherWork`, `urgentBesideOtherNode` |
| 2 Fault through a relative operation's real observation cycle: before acceptance, after acceptance, same cycle, unfinished cycle; copied EMCY through the application during queued and handed-off rearm | Met. The review found a defect, below. Observation precedence, the admission fence and no unqualified trailing `000F` are asserted; no rearm read or second receiver was added | `faultInCycle`, `cycleFaults`, `emcyBeforeHandover`, `emcyDuringMove`; `bench_route_test.cpp` `testMoveEmcyBeforeTrigger` |
| 3 COMMANDED basis beyond the golden path: stale and wrong-basis witnesses, generation change, endpoint and displacement bounds, quantization to zero, borrowed witness mutation before handoff, contradictory observation after acceptance | Met as synthetic contract tests, direct and through `AxisSession`, with and without asynchronous traffic. COMMANDED stays unenabled for the specimens, whose evidence names `ACTUAL` | `move_commanded_test.cpp` (new) |
| 4 One nonmutating admission check | Met: `validateMoveAdmission(context, axis, reference, nowUs)` in `Position.h`. `noteMoveTransmission` and `AxisSession::preflight` both call it, with the same status and detail. Mutation stays in `noteMoveTransmission`; `AxisSession::recheckMove` is gone | `Position.h`, `Position.cpp`, `AxisSession.cpp` |
| 5 Reproducible RS common-intent vectors | Met: exact inputs and outputs in `test/vectors/rs_common_intent.inc`, with RS revision `cd27b12` and the Git blob IDs of the compiled source closure. The optional `scripts/rs_move_vectors.py --rs <checkout>` reads them with `git archive`, compiles `scripts/rs_move_vectors.cpp` and reports any difference as an audit difference. Normal builds, tests and package consumers need neither RS nor network | `move_common_test.cpp`, `rs_move_vectors_contract` |

Also added: `trial706` in `move_absolute_test.cpp` replays the retained trial 706 (node 1, 2026-11-05) under record 111's measured signals. The move keeps the observed `6064 = -999`, never the target.

## Review findings and fixes

An independent review traced a cancelled or unknown trigger through the next admission and challenged the new tests.

- **EMCY in the same turn (defect).** One owner turn receives before it transmits, so an EMCY received in a turn could not stop a download queued before it from leaving in that turn. The guard against it is in the application's transmit hook. `AxisSession::preflight` now takes a `driveFault` fact. When the fact is set, the session's queued enable, move (parameter, trigger or rearm) or clear download is refused unsent and its action withdrawn with `DRIVE_FAULT`, using local records only. The S2 bench's `BenchRoute` passes it per node: a nonzero EMCY code since the route's per-node baseline, or several EMCYs unless exactly one arrived and its code is zero. `MoveRig` does the same, and `faultHook = false` reproduces the defect as a contrast. The withdrawal must happen inside `preflight`, because the S2 services the sessions before `BenchRoute` handles the EMCY; a plain refusal would end the move `INVALIDATED` and leave nothing for the route's `cancelMove(DRIVE_FAULT)`.
- **Withdrawn enable word (defect).** An enable word still queued after `cancelEnable` could leave; it is now refused at handover, as moves and clears already were.
- **Fault fence (contract decision).** A move or enable withdrawn for a drive fault sets `AxisSession::faultBlocked()`. For that session object's life, across release, close and open, `requestMove`, `requestMoveRelative`, `requestEnable` and `requestCounterZero` are refused (`INVALID_CONFIG`, `STATE`). A stop and `requestClearAlarm` stay admissible, and a clear's success does not lift the fence.
- **Tests.** The EMCY test asserts literal codes and details, and the interruption matrix's comment and expectations now state the fence and the post-cancel admission exactly. The fake's `000B` from idle gives the captured `8050`/`0050` (trials 124, 126). The fake freezes position at the word, while trial 713 moved on after its `000B`, so no rig test exercises the motion after a stop.

Each fix's regression fails without it, shown with built and restored mutants: the rig guard never passing the fact, the BenchRoute hook never passing it, the fence never set, and moves ignoring the fence.

## Known limits

- A fault reported only after the move or enable has ended withdraws nothing and sets no fence; for example, an EMCY in the same turn as the rearm's acknowledgement. The next move's fresh admission read must then show the drive's state. Closing this gap would need a new session call. None was added without a use.
- A clear withdrawn or held back for a fault does not set the fence.
- Relative moves of node 2, negative relative moves and stops from `005F` remain device facts for [packet 8](../../prompts/bench_closure_18_33/8_motion_stop_campaign.md).

## Checks

| Check | Result |
| --- | --- |
| Focused CTest: `move_relative`, `move_absolute`, `move_common`, `move_commanded`, `axis_session`, `enable_action`, `stop_action`, `alarm_clear`, `counter_clear`, owner, service, `bench_route`, `firmware_probe` | 21/21 passed; `move_relative` 171,792 checks, `move_absolute` 41,911, `move_commanded` 36,914, `axis_session` 103,884, `bench_route` 66,564 |
| Staged tree alone, in a detached worktree without the other packets' uncommitted work: `verify.py --mode full --compiler gcc` | Passed: repository, objects, build, inventory, registration, every CTest, package. Its documentation step could not run there, because the Doxyfile excludes `*/MotorControl-CAN/.build/*` and the worktree lives under `.build`. Strict Doxygen 1.13.2 rerun with only that exclusion moved: 0 warnings, 28 headers, 2447 members, 101 free functions; `check_documentation` passed |
| S2 bench firmware (`check_firmware.py --target s2`) and strict S2 stack check | Passed; stack: 0 breaches, empty baseline. The S3 stack database was left to CI |
| Remote CI | On the commit, recorded in the [execution index](README.md) |
