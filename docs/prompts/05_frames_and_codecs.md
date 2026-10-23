# Stage 05 Implement checked CAN frame and object codecs

Apply [COMMON.md](COMMON.md). Prerequisites: stages 01–04.

## Audit

Read RS checked codec/output guarantees, stateless SHZK-PT/VTN4xx/VibWire-108 codecs and their malformed-response tests. Compare INA228 signed/width handling and PCA9555 reserved-bit policy. Inspect the chosen stack's raw frame boundary and manufacturer EDS/PDF exact data widths.

## Deliver

1. Implement bounded protocol-neutral Classical CAN frame values and validation: 11/29-bit identifier, standard/extended, RTR/data, FD capability rejection, actual data length/DLC and up to eight payload bytes. RTR contains no payload even with nonzero requested-response DLC. CANopen services reject inappropriate flags at their own boundary, not globally. Test documented extended/RTR shapes from the contrast survey without installing another protocol driver. Do not expose a UART byte-stream API or add Modbus CRC.
2. Implement the needed pure SDO request encoding/reply checking or adapter-side checked decoding according to stage 02. For expedited transfers validate expected command, node/channel COB-ID, index/subindex, expedited/size bits and exact payload width; preserve little-endian signed values and complete abort codes.
3. Add bounded pure NMT/heartbeat/boot-up/EMCY and selected PDO/SYNC codecs only for required services. Distinguish TX wire encoding from controller metadata. Unknown events remain diagnosable without corrupting active operation output.
4. Generate compact `Objects.h`/Types/access policy from the reviewed ledger. Descriptive catalogue strings should be optional to link. Generation must not promote unresolved fields to writable operations. Preserve source citations in generated metadata/docs and a deterministic check mode.
5. Provide short direct helper names (`readWord`, `checkReply`, etc.) where meaningful. Builders/parsers take explicit expectations and capacities, borrow caller buffers only during the call, reject invalid overlap and publish payload atomically.

## Concrete implementation contract

Keep public frame representation in `include/MotorControlCAN/can/Frame.h`, CANopen services in `canopen/`, and generated/native Lichuan objects under `profiles/lichuan_c/`. A Classical frame needs enough information to distinguish these cases:

| Input | Core frame validation | CANopen SDO handling |
| --- | --- | --- |
| Standard data, DLC 8, eight bytes | Valid | Validate expected service and object |
| Extended data within 29-bit range | Valid | Reject for a standard-only SDO channel |
| RTR with DLC 8 and no bytes | Valid remote request | Reject as an SDO response |
| Standard ID beyond 0x7FF | Invalid | No decoding |
| FD request through initial Classical-only adapter | Capability rejection | No TX |

Do not read payload bytes from an RTR frame. Do not silently truncate an extended identifier or coerce FD to Classical. Define reserved flag handling and adapter timestamp/controller metadata separately.

For an ordinary default-channel expedited SDO, useful protocol vectors include upload request `40 41 60 00 00 00 00 00` for object `0x6041:00`, two-byte upload reply command `0x4B`, two-byte download command `0x2B`, download acknowledgement `0x60`, and abort command `0x80`. Verify these against the chosen CANopen source before using them. They illustrate protocol format; they do not prove CL86 permissions, values, writable state or configured COB-IDs.

Each decoder accepts explicit expected address/type/length and commits output only on success. Preserve protocol rejection as typed evidence rather than malformed frame. Document if a low-level decoder returns a valid decoded abort while the higher service returns a rejected outcome. Test that distinction directly.

Generate only facts reviewed in stage 01. Each generated object descriptor records its width/access/units and support gate; runtime public APIs cannot write unresolved/reserved fields through a generic descriptor escape path.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Document frame invariants, endianness, borrowing/overlap rules and output-on-failure behavior in headers. Add checked worked vectors to the service guide and a deterministic catalogue generation command.

Under [DELEGATION.md](DELEGATION.md), vector construction and generator/schema checks can run independently of implementation. A reviewer checks expected bytes from sources, not by calling the encoder being tested.

## Verify and exit

Test byte-exact vectors from original documents and separately constructed expected frames, every supported width/sign, valid device abort, wrong COB-ID/node/channel/index/subindex/command/length/flags, malformed size bits, insufficient capacity, overlaps and output sentinels. Vary unused bytes without reading beyond declared length. Run sanitizer builds where available.

Exit with pure independently usable codecs and catalogue policy. A valid frame is not evidence that an operation is permitted or completed. If the selected stack builds frames, tests exercise the adapter boundary without creating a duplicate transport engine.
