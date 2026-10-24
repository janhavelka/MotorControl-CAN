# Prompt 25 Passive health and statistics

Apply [COMMON.md](COMMON.md). Prerequisites: [24](24_owner_recovery.md).

## Objective and boundaries

Add the selected application health projection, exact statistics semantics and bounded passive diagnostics to the completed owner. These are cached application observations, not an installed core lifecycle or drive-state machine. This prompt does not add retry policy, recovery automation or motion behavior.

Use [CONVENTIONS](CONVENTIONS.md#5-selected-application-health-and-metrics) as the authoritative field/counter/reset contract and [MULTIDRIVE](MULTIDRIVE.md) for protocol optionality. Preserve distinct controller, NMT, drive, mode, freshness, operation and application facts.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS owner counters, passive traffic and cached accessors; inspect ADS1115 owner diagnostics, INA228 failure reporting and SHT3x health policy. Review MB85RC saturation and FieldCore health projection read-only. Record intentional CAN differences: sensor retry thresholds and latching policies do not become motor admission policy.

## Implement

1. Implement example-only `DriverState { UNINIT, READY, DEGRADED, OFFLINE }`, `Health` and `Stats`, with `health(binding)`, `driverState(binding)`, `stats()` and `clearStats()`. Reject unknown bindings rather than returning healthy defaults. All accessors use cached evidence and supplied time; none performs I/O or reads a clock.
2. Use exact Health field names and types from CONVENTIONS, including known flags so timestamp zero is valid. Count one completed logical service once, including all SDO segments. A multi-object operation is not counted again. Valid aborts prove endpoint liveness while failing that service.
3. Exclude local validation, unsupported capability, queue refusal, waiting and local cancellation from service-success/failure totals. Count cancellation in interface statistics. Passive events update liveness, not successful-service counts or the prior error. Cleanup success cannot overwrite the original failed result.
4. Apply the documented passive state precedence. Disabled/unknown heartbeat alone is inconclusive; controller recovery invalidates prior freshness. New evidence may restore communication health without resolving motion uncertainty or clearing a drive fault. No `LATCH_OFFLINE` or diagnostic label independently blocks a serviceable stop/read/reconciliation path.
5. Implement each named interface counter at one authoritative boundary, with saturation and clear reset lifetime. `txFrames` means actual confirmed controller transmission when available, never queue acceptance. `clearStats` clears only documented totals/high-water marks while preserving live failure streak, timestamps, state, generations, interlocks, faults, retained results and required history.
6. Expose bounded owner/traffic/resource diagnostic copies with drop/overwrite accounting and fair servicing. Trace consumers cannot drain normal RX or block service under slow output. State storage and service-gap bounds explicitly; larger traces remain application-local.

## Verify and exit

Test every state transition, service-versus-operation counting, segmented service counted once, abort/liveness distinction, timestamp zero, disabled heartbeat, counter saturation, binding reset and statistics reset. Include a successful independent reconciliation service after failure without rewriting the failed operation.

Exercise slow observer/output and full trace storage while ordinary and urgent owner work continues. Verify exact counter ownership and no cached getter traffic. Exit with the complete selected contract, documented capacities and no hidden thresholds/retries. Run focused owner/health/diagnostic suites and the native consumer.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document state projection, every counter increment/reset and diagnostic overhead; keep Doxygen synchronized. Ordinary coding agents can split health and counter tests after interfaces settle. An independent reviewer checks no double counting, no erased uncertainty and no diagnostic-driven motion policy.

Record results in `docs/reports/prompt_25.md`, update owner docs and coverage, then commit and sync on the recorded execution branch. Useful verified diagnostic or health checkpoints may be synchronized earlier.
