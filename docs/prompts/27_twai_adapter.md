# Prompt 27 TWAI adapter and SDK evidence

Apply [COMMON.md](COMMON.md). Prerequisites: [24](24_owner_recovery.md), [26](26_native_examples.md).

## Objective and boundaries

Implement one thin ESP32 TWAI adapter against a selected, pinned SDK interface. Complete its event/ownership contract and host-testable failure behavior. This prompt concerns transport evidence, not motor behavior or board qualification. Firmware startup/build integration follows in prompt 28; the shared application and owner already exist. Do not introduce a second CAN stack, controller receiver or framework dependency into installed core headers.

## Read and audit

Inspect RS platform/transport boundaries, ADS1115 and INA228 adapter examples and current FieldCore ownership assumptions. Then read the exact pinned ESP-IDF/TWAI API, implementation and MCU requirements; distinguish those facts from latest online examples or a different release. Inspect the selected Arduino dependency's underlying SDK version before deciding whether it can share this adapter unchanged. Apply frame/owner rules in [CONVENTIONS.md](CONVENTIONS.md) and [MULTIDRIVE.md](MULTIDRIVE.md), plus [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Implement the adapter over the established frame/transport contract, with explicit pins, bitrate, queue capacities and controller configuration supplied by the application. Describe the required external CAN transceiver. Do not reuse RS UART/DE wiring, infer a board's pins or assume an RJ45 connector means Ethernet.
2. Write a reviewed SDK evidence table: software enqueue acceptance, identifiable hardware transmission if available, errors, bus-off, receive metadata/timestamp meaning, controller retransmission and pending-TX disposal. Map only evidence the API actually provides. If completion cannot identify one particular frame, preserve that uncertainty instead of translating queue success into wire success.
3. Integrate controller shutdown/recovery with the owner's retirement contract. Pre-recovery queued motion must never become fresh intent after restart. Define what can be cancelled, drained, invalidated or left uncertain, including failures during controller reconfiguration. Recovery remains an explicit host operation and sends no device enable, NMT start or replay.
4. Implement one bounded RX/event path. ISR or driver callbacks copy only allowed bounded metadata; the cooperative owner performs routing. Verify actual SDK requirements for ISR storage and memory placement. Keep task stacks, internal RAM and optional trace/PSRAM choices application-local; no S3-only or PSRAM requirement belongs in the portable API.
5. Handle Classical CAN standard/extended and data/RTR frames according to the controller's real capabilities. Preserve requested RTR DLC separately from payload length. Reject unsupported capabilities, including FD when absent, before submission. Configure filters without dropping required asynchronous heartbeat/PDO/EMCY traffic.
6. Add host adapter fakes and measured/configured capacity documentation. Keep any SDK-specific compatibility shim small and justified by the selected versions, rather than building a general HAL framework.

## Verify and exit

Test queue rejection, delayed or unavailable completion evidence, TX error, bus-off, recovery failure, pending-TX retirement and RX/event overflow. Exercise callbacks and service under bounded work/output pressure. Verify frame metadata and rejection of unsupported formats. Establish clock timestamp meaning and test any adapter-owned 32-to-64-bit wrap extension; the core receives explicit monotonic microseconds. Compile against the pinned SDK where possible. Missing toolchains are explicit unperformed gates, not a passing adapter build. Do not flash or send traffic merely to prove compilation.

## Documentation and delegation

Update adapter/source-map documentation, configuration requirements, Doxygen and CHANGELOG together. Retain SDK source/version references and the evidence table so future upgrades can recheck assumptions. A normal engineering agent may implement fake-driver tests after the interface is fixed; a stronger reasoning reviewer independently checks stale TX retirement and completion ambiguity. The lead owns the adapter contract and integrates tests under [DELEGATION.md](DELEGATION.md); workers perform no hardware or Git operations.

Close this prompt's owned scope and checks; record `docs/reports/prompt_27.md`, verified SDK limits and remaining physical prerequisites. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
