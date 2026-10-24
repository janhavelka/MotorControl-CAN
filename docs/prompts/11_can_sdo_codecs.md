# Prompt 11 Classical CAN and SDO codecs

Apply [COMMON.md](COMMON.md). Prerequisites: [5 CANopen service fit experiment](5_stack_decision.md), [8 Verification CI and documentation gates](8_verification_docs.md), [9 Status and exact units](9_status_units.md).

## Objective and boundaries

Implement protocol-neutral Classical CAN frame validation and the checked SDO codec boundary selected by prompt 5. Complete pure representation/decoding behavior; transfer sessions, event codecs and generated objects remain prompts 13, 15/17 and 12. If the selected stack owns encoding, test its checked adapter boundary instead of installing a duplicate active engine.

## Read and audit

Refresh RS, ADS1115 and INA228. Read RS Protocol/access checks, output-atomic parsing tests and actual callers; compare INA228 signed-width handling and PCA9555 reserved-bit policy. Inspect FieldCore's relevant frame/transport integration read-only. Inspect primary CANopen protocol/selected stack sources for command/size/reserved-bit rules and the reviewed ledger for object widths. Apply [CONVENTIONS.md](CONVENTIONS.md#1-public-names-layout-and-dependencies) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

### Concrete evidence to recheck

Use the [pinned protocol review and vectors](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md). Upload initiation is `0x40` regardless of the expected value width; `0x4F/0x4B/0x43` are size-indicated expedited server responses for 1/2/4 bytes. Downloads use `0x2F/0x2B/0x23`; acknowledgement is `0x60`. Test a synthetic `0x6041:00` read and U32 `0x609A:00` transfer, including the public example's wrong read command and two-byte homing encoding as negative cases. These are host-only vectors, not authorization to issue either transfer.

## Implement

1. Implement `include/MotorControlCAN/can/Frame.h` with explicit 11/29-bit identifiers, standard/extended and data/RTR kinds, Classical DLC/data length and up to eight payload bytes. RTR may request a nonzero DLC but carries no payload. Do not access RTR payload bytes, truncate identifiers or silently convert FD to Classical. Describe unsupported FD as a capability failure at the initial adapter boundary.
2. Keep controller timestamps/status separate from wire data. Define invalid/reserved flag combinations and borrowed-buffer lifetime. Standard identifiers beyond `0x7FF` and extended identifiers beyond their range fail. Valid extended/RTR frames remain valid at this common boundary even when a specific CANopen service rejects them.
3. Implement required SDO request/reply encoding or checked adapter decoding under `canopen/`. Require explicit expected COB-ID/channel, node binding, index/subindex, command and reviewed object width. Validate legal expedited/size flags. Size-indicated replies must agree with reviewed width; non-size-indicated expedited replies use an explicitly known expected width, never guessed absent size bits. Check reserved/padding fields according to the selected source.
4. Preserve little-endian signed values and all 32 abort bits. Decode a valid native abort as protocol evidence, distinct from a malformed frame; higher service code later reports rejection. Likewise a valid segmented-upload initiation is not malformed merely because initial support is expedited-only. Expose enough checked information for prompt 13 to settle unsupported forms honestly.
5. Use direct helper names such as `readWord` and `checkReply` where meaningful. Inputs have explicit expectations/capacities; builders/parsers reject prohibited overlap and commit outputs atomically. Borrow codec buffers only during the call. Do not add UART streams or Modbus CRC to CAN framing.
6. Establish independently derived vectors. Standard examples include upload request `40 41 60 00 00 00 00 00` for `0x6041:00`, reply command `0x4B`, two-byte download `0x2B`, acknowledgement `0x60` and abort `0x80`; verify against primary sources before adopting. Format vectors do not establish Lichuan permissions, COB-ID configuration or motion semantics.

## Verify and exit

Test every supported width/sign, expedited replies with/without size indication, segmented initiation and valid abort. Reject wrong COB-ID, index/subindex, command, length and flags; exercise malformed size bits, capacity, overlap and output sentinels. Vary unused bytes without reading beyond declared data. Include RTR with requested DLC 8 and zero payload, extended frames, invalid standard identifiers and explicit FD rejection. Use source-backed non-CANopen frame shapes only as framing fixtures.

Run focused native/sanitizer checks where available, Doxygen, package/header checks affected by new declarations, integrity and diff checks. Close scope in `docs/reports/prompt_11.md`, update PROGRESS, then commit and sync this branch. A valid frame remains neither operation permission nor completion evidence.

## Documentation and delegation

Follow [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) for flags, endianness, width, borrowing, overlap and output guarantees; add worked vectors to the service guide. Apply [MAINTAINABILITY.md](MAINTAINABILITY.md#tests-and-refactoring). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), delegate expected-vector construction independently from the encoder; a stronger reviewer checks ambiguous command/size interpretation. Useful complete codec checkpoints may be synchronized earlier.
