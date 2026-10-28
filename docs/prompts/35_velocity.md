# Prompt 35 Finite velocity operations

Apply [COMMON.md](COMMON.md). Prerequisites: [31](31_stop_actions.md), [33](33_position_moves.md).

## Objective and boundaries

Deliver finite velocity preparation, stepping, service timing and stop settlement through the existing owner. Continuous unbounded velocity, torque/current commands and coordinated cyclic streaming remain outside this prompt. Servo support does not require implementing those modes under another name.

Use [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Retain the RS timing meaning while using reviewed Lichuan native widths, signedness, units and start/stop behavior.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read actual RS `prepareVelocity`, `nextVelocity`, `advanceVelocity`, `serviceVelocity` and timing/stop tests. Compare RV3032 bounded cleanup and INA228 validity. Inspect FieldCore active-motion/admission assumptions read-only. Reopen the conflicting Lichuan English/Chinese target/enable ordering and signedness evidence; use the selected firmware disposition, not a guessed sequence.

### Concrete evidence to recheck

The [velocity evidence](../audit/LICHUAN_MOTION_FACTS.md) fixes the candidate target at `60FF:00`, EDS I32/default 120; the mode table instead says U32 and 0..3000 rpm. ZH86/OP86 prescribe mode 3, observed mode 3, target/ramp writes, then `6,7,15` enabling to start; EN86 enables first and later refers to position trigger bits. Require a justified firmware-specific sequence; never splice both procedures together. Neither an I32 representation nor one positive run proves reverse operation. Distinguish zero velocity, stop and disable, and keep the RS preparation-time duration contract.

## Implement

1. Implement `prepareVelocity`, `nextVelocity`, `advanceVelocity` and `serviceVelocity` in the concrete velocity operation/profile files. Requests specify duration, signed engineering intent, units, native ramp time, stop policy and a fixed service deadline. Validate reverse-direction semantics before exposing reverse motion.
2. Resolve safe startup for the supported firmware. Some manuals start velocity on enable after target setup; another ordering appears elsewhere. Never enable with an unknown stale target. If ordering cannot be established, retain an explicit evidence gate rather than a fabricated implementation.
3. Preserve the timing anchor: successful preparation sets `stopDueUs = nowUs + durationUs` using supplied preparation time. Queue delay and setup consume this interval. Admission, trigger acknowledgement and later stepping do not restart it. Validate overflow and required stop-settlement budget before publishing the context.
4. Keep the supplied `deadlineUs` fixed. A missed service interval records service failure; bounded stop work may still be yielded within the original deadline. After expiry, another stop is a separately admitted urgent action with its own budget, never an extension or rewrite of the velocity result. Local cancellation emits no implicit stop.
5. Integrate real owner admission, urgent stop and retained result evidence. Expired prepared work cannot start fresh motion. Preserve needs-stop-equivalent evidence until a new checked stop observation establishes the fact; recovery or health improvement cannot clear it.
6. Retain generation, effective native target/error, setup/trigger/stop evidence and timing. Service any actually required documented lease without replaying an uncertain discrete command or silently disabling a watchdog.

## Verify and exit

Test native signed/range boundaries, invalid candidate before TX, stale target startup, failed mode/rearm, delayed admission, setup consuming the duration and already expired prepared work. Exercise service just before/after stop due, missed service, deadline expiry, lost stop acknowledgement, stop interruption and late feedback after terminal failure.

Trace original uncertainty through a later separately admitted stop. Use direct APIs through the real fake owner, without CLI. Exit with complete confirmed finite velocity behavior and precise firmware gates; unsupported stubs do not close required capabilities. Run timing/velocity/action/owner regressions and documentation checks.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Publish a short preparation/setup/run/stop timeline and Doxygen for service obligations and missed calls. One implementer owns the lifecycle. Require stronger independent timing/start-order review; delegate boundary/timeline tests separately.

Record scope/checks in `docs/reports/prompt_35.md`, update coverage/examples and commit and sync verified work on the same execution branch. Useful complete checkpoints may be synchronized before the prompt ends.
