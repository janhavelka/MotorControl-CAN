# Prompt 23 CAN owner dispatch and retained results

Apply [COMMON.md](COMMON.md). Prerequisites: [6](6_api_ownership.md), [14](14_sdo_reconciliation.md), [18](18_pdo_configuration.md), [19](19_typed_reads.md), [20](20_driver_settings.md), [21](21_io_settings.md), [22](22_tuning_settings.md).

## Objective and boundaries

Deliver the bounded application CAN owner for ordinary service/read/settings work, receive dispatch and retained results. Reuse the service/node/event storage already implemented; do not create a second receiver or node table. Urgent capacity, full cancellation and controller recovery are prompt 24. Real motion contexts do not exist yet and must not be imported, stubbed or declared implemented here.

Follow [CONVENTIONS](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE](MULTIDRIVE.md). The owner lives in examples/application code; installed core code remains C++11, framework-free and without I/O, clocks, allocation or scheduling.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `RtuBusOwner` copied admission/result lifetimes and `ProbeApp` dispatch; inspect ADS1115 owner-loop patterns and MB85RC retained-result behavior. Review current FieldCore bus ownership and dormant motor module read-only. Recheck the selected CANopen stack/adapter API and the prompt-6 storage/ownership proof.

## Implement

1. Implement `examples/common/CanBusOwner.h/.cpp` and its narrow existing transport/service interface. One cooperative application context owns controller/stack state, receive dispatch, bounded queues, bindings and caches. Passive heartbeat/PDO/EMCY processing continues during SDO transfers.
2. Use actual RS role names `admit`, `service`, `result` and `release`, with selected `RequestId`, `SequenceId`, `BusAdmission` and `Completion` meanings. Freeze CAN-specific signatures without UART parameters or synonymous submit/poll APIs. Admission rejection leaves the caller's ID and queued work unchanged.
3. Copy admitted input and reserve its terminal result slot before TX. Define non-consuming result lifetime, explicit release, foreign/stale ID rejection and generation/ID exhaustion. Never evict unread uncertainty to admit another request.
4. Implement bounded ordinary scheduling, producer fairness, per-channel SDO serialization and per-axis sequence reservation where needed. Reservation cannot block unrelated nodes or passive events. Declare queue/channel/binding/result capacities and per-turn RX/TX/event budgets, including zero-budget behavior.
5. Connect existing read and settings operations through the common yielded-work/event boundary. Use service completion callbacks/events to keep operation stepping outside the owner where the architecture places it. Do not introduce a universal operation executor or future motion-header dependencies.
6. Preserve controller queue acceptance, actual transmission, protocol confirmation and unknown effects separately. Already established SDO quarantine/reconciliation remains effective. This prompt does not claim complete urgent-stop or production firmware readiness before prompt 24.

## Verify and exit

Use two-node, multi-producer native fake tests for fair service, asynchronous receive during a transfer, full queues, retained-result exhaustion, wrong/stale IDs, release lifetime, zero budgets and caller-buffer mutation after admission. Verify identical late SDO ambiguity cannot be solved by local IDs. Trace one public read and one settings sequence through admission, service and retained completion.

No receiver may consume frames away from ordinary dispatch; observer copies cannot become the authoritative input path. Run focused owner/service suites and the native example consumer. Exit with complete ordinary-owner behavior and explicit, separately assigned prompt-24 work, not success stubs for urgent APIs.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document API lifetimes, capacities and budget tables with Doxygen on owner interfaces. One implementer owns receive dispatch/storage; delegate fairness and retention tests after signatures settle. A stronger independent reviewer traces ownership and service-result correlation.

Record closure in `docs/reports/prompt_23.md`, update owner/architecture docs and checks, then commit and sync on the existing execution branch. Synchronize coherent verified checkpoints when complete.
