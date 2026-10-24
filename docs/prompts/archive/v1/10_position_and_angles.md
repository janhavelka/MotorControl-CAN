> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 10 Implement finite position and wrapped-angle moves

Apply [COMMON.md](COMMON.md). Prerequisites: stages 04, 08–09.

## Audit

Read RS Position/Axis context APIs, request validation, motion-profile snapshots and completion tests; inspect `nextMove`/`advanceMove` actual terminal fields. Compare I2C caller-buffer ownership and BME280 partial configuration handling. Read FieldCore Rs485MotorModule's target/operation/step correlation and exact-target snapshots.

## Deliver

1. Implement `prepareMoveAbsolute`, `prepareMoveRelative`, `prepareMoveAngle`, `nextMove` and `advanceMove` using the same common intent and exact target preparation. Keep all scale/origin/rounding/limits in the public API, not the console.
2. Define request target/node/dialect/configuration generations, explicit mode/profile setup policy, native position/rate/ramp-time widths, allowed range, stop policy and required observations. Do not silently clamp, truncate signed positions or apply ESS tested RPM limits to Lichuan.
3. Implement the verified Profile Position handshake: required mode confirmation, parameter staging, controlword edge/rearm, target acceptance and subsequent completion. Distinguish configuration write, trigger, drive acceptance and controller-observed completion; independent physical movement/standstill evidence belongs to stage 18. A successful SDO or old completion bit cannot finish a new move.
4. Separate relative actual/commanded/queued bases and reject unproved semantics. Wrapped angles require explicit path, tie policy, frame/origin and fresh multi-turn reference. A selected path that violates a limit fails; do not silently choose another turn.
5. Preserve effective target, quantization/approximation error, submitted bytes/objects, readback, timing and final evidence. Handle zero-displacement without motion writes when the contract permits. Partial setup and unknown trigger results remain distinct.
6. Make cancellation, deadlines, stop interruption, fault/reboot and stale/missing feedback produce explicit retained outcomes. Recovery never resumes an interrupted move. Configuration/profile snapshot reuse is explicit and invalidated by any contradictory change.

## Concrete implementation contract

Use `profiles/lichuan_c/Position.h`, common `MoveOperation.h`, `Axis.h` and the shared owner work boundary. All callers, including CLI, use `prepareMoveAbsolute`, `prepareMoveRelative`, `prepareMoveAngle`, `nextMove` and `advanceMove`. The terminal context is the result; a generic `getMoveResult` is not part of RS parity.

Separate these stages in code and tests: exact target preparation, profile parameter setup, mode confirmation, trigger/rearm, protocol acknowledgement, new acceptance evidence, and new completion evidence. Do not require a separate generic executor just to express those few states. The context retains enough immutable intent/configuration evidence to explain partial setup and an unknown trigger.

A move prepared for binding/configuration generation A must not run under B. The native target can be an exact integer even when a human display uses decimal. Conversion to a native float in a contrast fixture must produce a checked error bound; it cannot weaken Lichuan's integer path.

Use illustrative configured values in examples rather than manufacturer defaults: an application explicitly supplies increments-per-turn, gear ratio, origin, limits and rounding. Show the resulting effective target and error before motion admission. Wrapped-angle policy includes shortest/positive/negative path and a specified half-turn tie; a limit violation rejects the chosen path.

For a zero-distance request, explicitly specify whether no motion is needed or the selected semantics still require a device action. Do not use a local no-op optimization to claim verified position/standstill when reference evidence is missing. A finite onboard trajectory that requires a documented watchdog refresh must retain that lease without replaying a relative trigger or restarting the trajectory.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Add native C++ preparation/advancement examples, unit/angle examples and evidence timelines. Doxygen states relative basis, generations, quantization, deadline and completion meaning.

Under [DELEGATION.md](DELEGATION.md), the exact-target and lifecycle reviews can run separately. Require a reviewer to trace timeout after trigger and stale completion from a previous move all the way to the retained result and later admission policy.

## Verify and exit

Test absolute/relative positive/negative boundary targets, all unit/frame paths, zero displacement, profile reuse and mismatch, mode change failures, trigger edge history, stale target-reached, target accepted without completion, completion without matching reference, stop at each stage, timeout after trigger and new operation after uncertain execution. Compare common behavior vectors to RS without copying device-specific expected frames.

Exit with a complete finite position API for resolved profiles, independently callable by native applications. Any unresolved status dialect/scale blocks affected physical motion claims, even if simulated handshakes pass.
