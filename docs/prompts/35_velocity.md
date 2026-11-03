# Prompt 35 Finite velocity operations

Apply [COMMON.md](COMMON.md). Prerequisites: [31](31_stop_actions.md), [33](33_position_moves.md).

## Objective and boundaries

Deliver finite velocity preparation, stepping, service timing and stop settlement through the existing owner. Continuous unbounded velocity, torque/current commands and coordinated cyclic streaming remain outside this prompt. Servo support does not require implementing those modes under another name.

Use [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Retain the RS timing meaning while using reviewed Lichuan native widths, signedness, units and start/stop behavior.

## Read and audit

Refresh MotorControl-RS HEAD and dirty state and read the RS code for this seam; RS is authoritative. Read actual RS `prepareVelocity`, `nextVelocity`, `advanceVelocity`, `serviceVelocity` and timing/stop tests. Trust the RS code over its comment: "admission" in the RS stop-timer comment (`Velocity.h`, `docs/ess_velocity.md`) means the preparation call. Inspect current FieldCore-node read-only for ownership or integration changes; FieldCore has no angle or velocity operation, so compare identity and admission only. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread; a sibling row must name a behavior adopted or rejected, with path and symbol. Reopen the conflicting Lichuan English/Chinese target/enable ordering and signedness evidence; use the selected firmware disposition, not a guessed sequence.

### Concrete evidence to recheck

The [velocity evidence](../audit/LICHUAN_MOTION_FACTS.md) fixes the candidate target at `60FF:00`, EDS I32/default 120; the mode table instead says U32 and 0..3000 rpm. ZH86/OP86 prescribe mode 3, observed mode 3, target/ramp writes, then `6,7,15` enabling to start; EN86 enables first and later refers to position trigger bits. Require a justified firmware-specific sequence; never splice both procedures together. Neither an I32 representation nor one positive run proves reverse operation. Distinguish zero velocity, stop and disable, and keep the RS preparation-time duration contract. `60FF`=0 is undocumented: reject zero speed and do not treat it as stop. Every Lichuan rate (`60FF`, `6081`, `6099:01/:02`) is rpm, so a Lichuan `AxisConfig` selects `nativeRateBasis` `MOTOR_TURNS` with `nativeVelocityTime` `MINUTE`; the default `COMMAND_INCREMENTS` is the wrong unit for `60FF` (A-01, [axis contract](../axis_contract.md#signed-velocity-and-the-native-rate-destination)). Never write `effectiveNative` to an rpm object unless the prepared `rateBasis`, `rateTime` and integer representation are that destination, with a negative test (under `BINARY32`, `effectiveNative` stays 0 and reads like a stop). Compare a prepared rate's generation yourself: `validatePreparedTarget` covers positions only; `prepareVelocityTarget` quantizes once and reports error in rpm. Prompts 33, 35 and 36 share that one rate path and add unsigned and width checks on top.

Evidence route: prompt 29 conditional experiment or manufacturer clarification; do not wait for prompt 47. Processing-time cutoff follows [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence): `now >= deadlineUs` expires, even if the frame was received earlier.

Read row owned here: `60FF` (typed readback, assigned by the reviewed ledger).

## Implement

1. Implement `prepareVelocity`, `nextVelocity`, `advanceVelocity` and `serviceVelocity` in the concrete velocity operation/profile files. Keep `Axis.h` `VelocityRequest` pure. Add a distinct finite-operation request or prerequisites type with `durationUs`, signed engineering intent and units, stop policy and verified native ramps; pass the fixed service `deadlineUs` as an argument. Zero speed rejects; a duration above `maxPolls * pollIntervalUs` rejects and must be below `deadline - now`; `serviceVelocity` runs only with no admitted transaction; a service is missed when later than `stopDueUs + pollIntervalUs`. Validate reverse-direction semantics before exposing reverse motion. Own the mode-specific parameter writes `60FF`, `6083`, `6084` in the operation as phases of the velocity run; do not route them through `prepareDriverSettings`, and they do not advance the configuration generation. They retire retained `MOTION_PARAMETERS` observations, and writing `6083/6084` in either mode invalidates the other mode's snapshot. A new header or source needs `CMakeLists.txt`, `test/package_test.py`, `scripts/check_repository.py` and `test/repository_test.py` updates.
2. State the full transition. Enabling in mode 3 (documented only from a non-enabled start; the mode 3 order conflicts, Q03) and changing `6060` while enabled (no documented sequence, Q11) are separate gated actions. Reject before TX if the observed mode differs and the drive is enabled. The ordinary workflow "home, then move" has no documented path; do not write `6060` silently on an energized drive. Resolve safe startup for the supported firmware. Some manuals start velocity on enable after target setup; another ordering appears elsewhere. Never enable with an unknown stale target. If ordering cannot be established, retain an explicit evidence gate rather than a fabricated implementation.
3. Preserve the timing anchor: successful preparation sets `stopDueUs = nowUs + durationUs` using supplied preparation time. Queue delay and setup consume this interval. Admission, trigger acknowledgement and later stepping do not restart it. Validate overflow and required stop-settlement budget before publishing the context.
4. Keep the supplied `deadlineUs` fixed. A missed service interval records service failure; bounded stop work may still be yielded within the original deadline. After expiry, another stop is a separately admitted urgent action with its own budget, never an extension or rewrite of the velocity result. Local cancellation emits no implicit stop.
5. Integrate real owner admission, urgent stop and retained result evidence. DEC-8 mode 3 stop row, added here with the operation: latest acknowledged `6040` `000F` while running, stop word `010F` or `000B`. Arrows appear only in the position section, so carry `predecessorDocumented=false` in the evidence; allowing it is a project decision, not vendor evidence. Under DEC-2, an explicitly requested urgent `000B` may be transmitted on an unresolved route; its reply never qualifies, `execution` stays UNKNOWN, one request is one transmission with no resend, and a CAN stop is best effort and not a safety function. Expired prepared work cannot start fresh motion. Preserve needs-stop-equivalent evidence until a new checked stop observation establishes the fact; recovery or health improvement cannot clear it.
6. Retain generation, effective native target/error, setup/trigger/stop evidence and timing. Service any actually required documented lease without replaying an uncertain discrete command or silently disabling a watchdog.

Read rows here are outside the DEC-1 accepted list unless named there; they stay Q20 gated and use an explicit reviewed acquisition.

## Verify and exit

Test native signed/range boundaries, invalid candidate before TX, stale target startup, failed mode/rearm, delayed admission, setup consuming the duration and already expired prepared work. Exercise service just before/after stop due, missed service, deadline expiry, lost stop acknowledgement, stop interruption and late feedback after terminal failure.

Trace original uncertainty through a later separately admitted stop. Use direct APIs through the real fake owner, without CLI. Exit with complete confirmed finite velocity behavior and precise firmware gates; unsupported stubs do not close required capabilities. Run timing/velocity/action/owner regressions and documentation checks.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Publish a short preparation/setup/run/stop timeline and Doxygen for service obligations and missed calls. One implementer owns the lifecycle. Require stronger independent timing/start-order review; delegate boundary/timeline tests separately.

Record scope/checks in `docs/reports/prompt_35.md`, update coverage/examples and commit and sync verified work on the same execution branch. Useful complete checkpoints may be synchronized before the prompt ends.
