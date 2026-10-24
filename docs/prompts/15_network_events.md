# Prompt 15 Passive network events and invalidation

Apply [COMMON.md](COMMON.md). Prerequisites: [13 Bounded SDO transfer lifecycle](13_sdo_lifecycle.md), [14 SDO ambiguity and reconciliation](14_sdo_reconciliation.md).

## Objective and boundaries

Add continuous passive CANopen event decoding and node lifecycle evidence to the existing service storage. Complete heartbeat, boot-up and EMCY handling with precise invalidation. Explicit NMT commands/service admission policy belong to prompt 16; PDO maps and configuration belong to prompts 17–18. No implicit network, heartbeat or drive configuration writes are permitted.

## Read and audit

Refresh RS, ADS1115 and INA228. Read RS observation/generation and passive-traffic behavior, ADS1115 owner ingress and INA228 freshness semantics with relevant callers/tests. Inspect current FieldCore binding/availability invalidation read-only. Read selected stack NMT/heartbeat/EMCY configuration and original Lichuan evidence; do not assume drive-side watchdog behavior from host monitoring. Apply [CONVENTIONS.md](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

## Implement

1. Add required pure boot-up, NMT heartbeat and EMCY codecs under the existing `canopen/` boundary, such as `Network.h`, without duplicating stack decoding responsibilities. Validate actual configured COB-ID, frame kind, length, source node/binding and generation. Preserve raw EMCY/error evidence alongside decoded known fields; unknown events remain diagnosable without corrupting active operation output.
2. Extend prompt 6/13's application-owned bounded node/channel storage and receive dispatch. One frame enters normal routing once; observers receive copies. No second CAN receiver, global installed node registry or parallel node table may appear. Keep capacities and overflow/drop behavior explicit and bounded.
3. Record independent facts: controller state, last valid heartbeat, effective producer interval, last matching SDO time, observed NMT state/time, drive state/mode evidence and EMCY history. Heartbeat disabled, never observed and expired are different states. A successful SDO does not establish Operational NMT; a valid heartbeat cannot erase an SDO failure or recreate valid position/configuration.
4. On boot-up/reset, binding/dialect/configuration change and recovery, invalidate dependent cached configuration, active PDO-map evidence, scale/reference, prepared operations and expected replies according to their actual generation dependencies. Preserve interrupted results and uncertain previous effects. Invalidation never causes automatic NMT start, re-enable, fault clear or replay.
5. Keep liveness timestamps, raw event history and application health separate. These event facts feed later passive health policy; this prompt does not create a new generic health state machine. Other protocols need independent observations and may not have heartbeat, NMT, EMCY or statusword concepts.
6. Define a routing description for configured identifiers/frame kinds/expected lengths/service bindings and generation. Unknown identifiers are diagnostics, not an instruction to discover nodes or switch protocols. Make async event delivery continue while SDO waits, cancels or remains unresolved. Document callback data lifetime and copy any retained event information.

## Verify and exit

Inject supported NMT heartbeat values, boot-up during SDO, disabled/never-seen/late heartbeat, repeated EMCY, malformed lengths/flags, wrong binding/generation, unknown identifiers and capacity exhaustion. Verify receive-versus-service timestamps and stale/future evidence policy. Interleave multiple node events with an active SDO and prove each retained observation reaches its normal consumer.

Check that a late event cannot restore old configuration/reference validity and that heartbeat freshness alone cannot overwrite drive faults or command outcomes. No fake event establishes real drive watchdog behavior. Run focused native tests, affected Doxygen/verifier, integrity and diff checks.

Exit with complete passive event/lifecycle behavior and one shared storage owner. Close scope/checks in `docs/reports/prompt_15.md`, update PROGRESS and affected coverage, then commit and sync this branch. A tested codec and its complete event consumer may form a useful earlier checkpoint; do not leave decoding disconnected from invalidation.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to timestamps, event retention, generations and borrowing. Update service routing and owner-capacity guides; follow [MAINTAINABILITY.md](MAINTAINABILITY.md#make-control-flow-and-state-reviewable). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), delegate independent event vectors and invalidation tests after storage contracts are fixed. One agent owns routing mutations; require independent review of reset/late-event behavior.
