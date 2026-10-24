# Prompt 33 Finite position operations

Apply [COMMON.md](COMMON.md). Prerequisites: [10](10_axis_coordinates.md), [20](20_driver_settings.md), [31](31_stop_actions.md), [32](32_fault_reference_actions.md).

## Objective and boundaries

Deliver complete finite absolute/relative position operations using the exact-axis preparation already implemented, the reviewed Lichuan Profile Position sequence and the real owner/stop path. Wrapped-angle request integration belongs to 34. Do not implement a generic trajectory framework or expand scope to coordinated cyclic motion.

Apply [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Native profile parameter units and completion semantics remain explicit; servo and stepper common intent does not impose identical wire behavior.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `Position`, `Axis`, request/profile snapshot validation and terminal context tests. Compare BME280 partial configuration and I2C admitted-buffer ownership. Inspect actual FieldCore target/operation/step correlation and exact-target snapshots read-only. Recheck selected Lichuan mode, trigger, acknowledgement and completion evidence; unqualified status dialect remains a motion gate.

### Concrete evidence to recheck

Use [position sequences](../audit/LICHUAN_MOTION_FACTS.md): `607A` I32 target, documented -1000000..+1000000 pulses; `6081` U32 profile speed, 0..3000 rpm; `6083/6084` U32 times, 0..2000 ms. Controlword decimal `15 -> 31` starts absolute and `15 -> 95` relative; `15 -> 63/127` sets immediate-update bit5. Bit4 is the new-target rising edge; bit6 selects relative. The documents do not establish a target FIFO, capacity or acknowledgement timing. Completion bit15 must be fresh for this operation and interpreted under the qualified status dialect.

## Implement

1. Implement `prepareMoveAbsolute`, `prepareMoveRelative`, `nextMove` and `advanceMove` using `profiles/lichuan_c/Position.h`, common `MoveOperation.h` and existing owner work/events. The concrete terminal context is the result; do not add `getMoveResult`.
2. Capture immutable intent, effective target, quantization error, scale/origin/limits and binding/configuration/dialect generations. Validate native target/rate/ramp widths and allowed ranges without clamping, truncation or ESS tested limits. Relative actual/commanded/queued bases require explicit evidenced semantics.
3. Implement the reviewed finite phases directly: parameter setup, mode confirmation, trigger/rearm, protocol confirmation, new acceptance evidence and new completion evidence. Successful SDO or an old target-reached bit cannot finish a new move. Keep submitted bytes/objects, readback and timing sufficient to explain partial setup or an unknown trigger.
4. Integrate deadline, cancellation, real stop interruption, fault/reboot and stale/missing feedback with retained outcomes. A request prepared under generation A cannot execute under B. Recovery never resumes an interrupted move.
5. Make configuration/profile snapshot reuse explicit and invalidate it on contradictory observations. Define zero-distance semantics without claiming position/standstill from missing reference evidence. If a finite onboard trajectory requires a documented refresh lease, service that lease without replaying a relative trigger or restarting the trajectory.
6. Keep controller-observed completion separate from independent physical movement/standstill evidence. Missing firmware facts block affected motion capability, not unrelated unit arithmetic or owner tests.

## Verify and exit

Test positive/negative absolute and relative boundaries, native overflow, exact quantization, all supported bases, zero displacement, snapshot reuse/mismatch, mode/setup failure, trigger history, stale acceptance/completion and completion without matching reference. Interrupt at every phase with stop, fault, cancellation, reboot and deadline; test timeout after trigger followed by attempted new motion.

Run through the real fake owner path and compare common intent vectors to RS without copying its device frames. Exit with complete confirmed absolute/relative behavior and native examples independent of CLI. Do not mark unresolved dialect behavior implemented merely because simulated events pass. Run position/axis/action/owner regressions and Doxygen.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Publish request/evidence timelines and Doxygen for bases, quantization, generations and deadlines. One implementer owns the lifecycle. Delegate exact-target test review separately; require stronger review tracing unknown trigger through retained result and later admission.

Record closure in `docs/reports/prompt_33.md`, update coverage/examples and commit and sync on the same execution branch. Complete verified checkpoints may be synchronized during the prompt.
