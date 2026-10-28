> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 11 Implement velocity, homing and remaining native operations

Apply [COMMON.md](COMMON.md). Prerequisites: stages 09–10.

## Audit

Read RS Velocity/Homing, `serviceVelocity`, `getHomeReference`, remaining native coverage gaps and tests. Compare RV3032 finite substep cleanup and MB85RC partial effects. Inspect current FieldCore consumer assumptions about active motion and administrative enable. Re-read all applicable Lichuan mode tables and their contradictions.

## Deliver

1. Implement finite `prepareVelocity`, `nextVelocity`, `advanceVelocity`, `serviceVelocity` with explicit duration, signed engineering intent, units, ramp time, stop behavior and service deadline. Validate wire signedness and reverse-direction semantics before exposing reverse motion.
2. Resolve the conflicting velocity start sequences: Chinese manuals can start on enable after target setup; English describes another ordering. Never enable with an unknown stale target. A profile unable to establish the correct sequence remains explicitly unsupported for this operation pending evidence.
3. Implement `prepareHome`, `nextHome`, `advanceHome`, `getHomeReference` for documented methods with explicit method IDs, direction, origin/limit/signal prerequisites, search/creep speed, offset and ramp time. Validate methods 17/18/24/29 against the actual source/model; do not copy ESS methods or assume every method needs every switch.
4. Distinguish successful homing/reference publication from merely accepted mode/start commands. Handle fault, stop, timeout, impossible signal state, restart, counter overflow and uncertain completion. Publish a new reference only with qualified evidence and the correct generation.
5. Complete reviewed native functions outside common motion: supported I/O actions, device-position changes, tuning/control groups and other documented capabilities. Keep dangerous persistent/communication workflows for stage 13. Do not add invented serial segments, torque/current motion or cyclic modes merely to fill a generic API.
6. Audit native coverage against every ledger row. Record known but unimplemented fields separately from source conflicts and genuinely unsupported model capabilities. Complete all confirmed rows assigned through stage 11; communication/persistence rows remain owned by stage 13. No catch-all raw accessor counts as typed completion.

## Concrete implementation contract

Use `profiles/lichuan_c/Velocity.h` and `Homing.h`, keeping `prepareVelocity/nextVelocity/advanceVelocity/serviceVelocity` and `prepareHome/nextHome/advanceHome/getHomeReference`. Do not substitute a blocking run-for-duration helper.

Preserve the audited RS timing distinction: successful `prepareVelocity` sets `stopDueUs = nowUs + durationUs` using the caller-supplied preparation time. Queue delay and setup consume that interval; owner admission, step admission and trigger acknowledgement do not restart it. The separately supplied fixed `deadlineUs` must leave the required bounded stop-settlement budget. Validate this arithmetic before publishing the context. Missing the required service interval records a service failure, while stop work may still be yielded within the original deadline. Once that deadline expires, any further stop is a separately admitted urgent action with its own budget. It cannot extend the expired velocity operation or clear its original uncertainty. Local cancellation yields no implicit stop.

The exact available CAN error/outcome fields may differ, but retain that behavior. Include tests with delayed admission after preparation, setup consuming the intended run interval, service arriving just before/after the stop due time, lost stop acknowledgement, and new feedback after expiry. Expired prepared work cannot start fresh motion on admission. Preserve `needsStop`-equivalent evidence until a new checked stop observation establishes the relevant fact.

For each documented homing method, record required inputs, prohibited assumptions, direction/search/creep values, offset, mode/trigger, expected acknowledgement/completion and reference publication. Validate only the signals that method actually requires. An application origin, device counter clear and successful home reference are different operations.

Complete remaining native-operation rows assigned to 11; communication/persistence remains stage 13. A servo contrast fixture may reject unsupported homing through capabilities without claiming every servo lacks homing. Do not expose torque/current/impedance commands under the name velocity.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Document finite service timing with a short timeline, homing prerequisites by method, and updated native coverage. Doxygen specifies when `serviceVelocity` must run and what missing a call does.

Use [DELEGATION.md](DELEGATION.md) for independent timing/state review and disjoint homing-method source verification. The lead reviews all side-effecting native operations and their coverage before integration.

## Verify and exit

Run finite-duration/deadline/stop tests, missing service tick, signedness/range boundary, startup with stale velocity target, failed rearm, each homing method/prerequisite, interrupted home, stale home-complete bit, reference invalidation and native setting constraints. Test with no CLI and with adversarial fake network events.

Exit with all confirmed documented motion/native rows assigned through stage 11 implemented and the remaining uncertainty list narrowed to exact device facts. Communication/persistence rows remain explicitly pending for stage 13. Continuous unbounded velocity and coordinated cyclic control remain outside the agreed initial trajectory scope.
