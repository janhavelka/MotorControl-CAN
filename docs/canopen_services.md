# CANopen service contract

The authoritative service engine is **CANopenNode**, selected in
[the pinned fit decision](decisions/canopen_stack.md), revision
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`. The installed portable package now
provides a checked scalar SDO boundary in
[`canopen/Sdo.h`](../include/MotorControlCAN/canopen/Sdo.h). It checks selected-stack
requests and decodes initiation replies; CANopenNode owns encoding and transfer
progression. The portable header has no stack dependency, session, transport,
clock, allocation or retry. The actual stack bridge remains a private native
proof, with its Apache-2.0 notices. No production controller adapter or device
profile is advertised.

The [owner contract](can_owner.md) owns storage and scheduling. The
[profile contract](profile_contract.md) owns operation interpretation. A selected
service can confirm a native exchange, but cannot declare a motor stopped or an
axis healthy. Generic axis state does not acquire mandatory CANopen fields.

## Production frame and scalar boundary

[`Frame`](../include/MotorControlCAN/can/Frame.h) owns up to eight Classical CAN
bytes. It preserves standard/extended identifiers and data/RTR kind. A standard
ID must fit 11 bits; an extended ID must fit 29. Data length equals DLC, while
RTR has zero payload even when requesting DLC 8. Frame validation never reads
RTR payload storage. Timestamps and controller status belong to separate
adapter evidence. `fd` returns `UNSUPPORTED`, including combinations with other
invalid fields; Classical `brs` or `esi` flags return `FRAME_ERROR`. Adapters must
reject unknown controller flags explicitly instead of dropping them.

`CANOPEN::SdoChannel` supplies a complete `Binding` and two distinct active
standard CAN-IDs in 1..0x7ff. Its server node address must be 1..127. These are
protocol-local restrictions: common `Frame` and logical axis identity retain
their wider domains. The channel may use custom IDs; this code never derives a
route from a node or treats an EDS default as active configuration. The owner
establishes the physical interface, route and current generations before a call.
Host provenance is absent from the wire and cannot identify an indistinguishable
late response. [CiA's SDO protocol overview](https://www.can-cia.org/can-knowledge/sdo-protocol)
describes the two CAN-IDs per channel.

`SdoExpectation` adds exact index/subindex, upload/download direction and reviewed
scalar width 1..4. Zero width is invalid; three-byte scalars are representable
without implying a Lichuan object of that width. The boundary has no object
whitelist, access policy or permission to transmit. Profiles supply those facts
from the reviewed ledger and current device evidence.

| Call | Checked behavior |
| --- | --- |
| `checkRequest` | Checks a CANopenNode outgoing initiation before controller handoff. Upload is exactly command `40`; sized expedited downloads are `2F/2B/27/23` for 1/2/3/4 bytes. Reserved request bytes and unused download bytes are zero, matching the selected stack's canonical output. |
| `checkReply` | Checks standard data kind, eight-byte envelope, explicit reply ID, index/subindex and legal command fields; publishes an owned `SdoReply` only after all checks pass. Valid extended/RTR frames return `UNSUPPORTED` at this service boundary. |
| `readWord` / `readSignedWord` | Read an explicitly selected 1..4-byte little-endian value from a borrowed span. Signed values use checked two's-complement interpretation, including 24-bit and I32 extremes, without narrowing unsigned native aborts. |

The span helpers require sufficient available bytes and leave scalar output
unchanged on invalid width, null/short span or overlap. They access only the
selected prefix. Output may not overlap any part of the supplied span.
`checkReply` similarly rejects either input overlapping any part of its output,
including `output.frame` or `output.expected`. Input storage is borrowed only
during a call; the successful reply owns its frame, expectation and data.

### Command, size and padding policy

For uploads, `43/47/4B/4F` indicate expedited widths 4/3/2/1. `42` is a legal
expedited reply without a size indication: only the explicitly reviewed expected
width determines the scalar prefix. `40/41` are legal segmented initiation,
without/with indicated total size. They produce `SEGMENTED_UPLOAD`, no scalar
value and no completion. Every wire-indicated size must agree with the expected
width; a disagreement returns `ILLEGAL_VALUE` with `SdoError::WIDTH` and preserves
output. A download acknowledgement is exactly `60`; an abort is exactly `80`.

Reserved command bits reject, and unused-byte-count bits must be zero unless
both expedited and size-indicated flags apply. Unused receive bytes remain
opaque: expedited padding, acknowledgement tails and unsized segmented tails
may vary. Their original bytes survive in `SdoReply::frame`. This separates
command validity from meaningless byte values. The primary
[Parker PIO-337 manual](https://www.parker.com/content/dam/Parker-com/Literature/Electromechanical-North-America/SERVICE-USER-GUIDES/CAN_STD_PIO-337.pdf)
88-022351-01A, October 2003, Initiate SDO Upload and SDO Examples (pp. 108-111)
documents these protocol fields and unused-byte examples. This protocol evidence
establishes no PIO-337 or Lichuan support. The
[pinned CANopenNode source](reference/public_code/canopennode/301/CO_SDOclient.c)
corroborates canonical zeroed TX, opaque receive padding and unsized/segmented
forms. The checked boundary rejects malformed command combinations that the
pinned client's broad upload command mask alone can accept.

`checkReply` returning `OK` means a valid representation. For `ABORT`, its
`uint32_t abortCode` preserves all 32 bits, including unknown codes and zero;
the higher service reports protocol rejection separately. No abort passes
through signed `Status::detail`. A malformed frame instead returns a local
validation failure without publishing output. Neither a valid acknowledgement
nor a decoded value establishes drive readiness, motion completion or health.

### Host-only worked bytes

These literal format examples use illustrative node 5 and explicitly selected
IDs `605`/`585`. They are neither captured traffic nor CAN configuration or
authorization. Every row is a standard data frame with DLC/data length 8.

| Intent / reply | CAN-ID | Eight payload bytes | Checked meaning |
| --- | --- | --- | --- |
| Upload `6041:00`, expected U16 | `605` | `40 41 60 00 00 00 00 00` | Width is local metadata, never request command `4B`. |
| Sized U16 upload response | `585` | `4B 41 60 00 34 12 00 00` | Raw value `0x1234`; no qualified interpretation of status bits. |
| Unsized expedited response | `585` | `42 41 60 00 34 12 A5 5A` | Expected width 2 selects `34 12`; remaining bytes stay opaque. |
| Sized segmented initiation | `585` | `41 41 60 00 02 00 00 00` | Known size 2, with data still pending. |
| Download `609A:00`, reviewed U32, value 100 | `605` | `23 9A 60 00 64 00 00 00` | Four bytes even though 100 fits in one byte. |
| Matching download acknowledgement | `585` | `60 9A 60 00 00 00 00 00` | Native acknowledgement, separate from operation completion. |
| Synthetic unknown abort | `585` | `80 9A 60 00 98 BA DC FE` | Valid abort evidence `0xFEDCBA98`, with every bit intact. |

The original EDS identifies `6041:00` as U16 and `609A:00` as U32. EN86/ZH86
physical page 13 (printed 11) likewise specify the latter as millisecond ramp
time; neither width nor access spelling establishes permission to issue a
transfer. Firmware applicability remains unresolved. The pinned
[public example](audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) wrongly uses response
commands for reads and two-byte homing writes. Tests reject its `4B` upload
request and `2B` download when the reviewed `609A` width is four.

[Frame tests](../test/frame_test.cpp),
[scalar codec tests](../test/sdo_codec_test.cpp) and
[selected-stack adapter tests](../test/sdo_adapter_test.cpp) exercise independent
literal vectors, widths/signs, malformed commands, sizes, identities, capacities,
overlap and unchanged outputs. These remain host evidence. Transfer sessions,
bounded segmented draining and retained service settlement belong to prompt 13;
event codecs and object generation retain their separately assigned stages.

## Retained proof request and evidence

[`ServiceWork`](../experiments/api_ownership/Contracts.h) carries a complete copied
binding, operation/step tokens, exclusive absolute deadline and either an SDO
index/subindex/payload or a raw frame. The proof owner admits only CANopen SDO
work on its fixed route. `RAW_FRAME` expresses the future separate protocol
boundary; it is rejected by this adapter before queuing. Uploads have no outgoing
payload, downloads have 1–32 bytes. A read's `mayChange` is an explicit profile
fact, never inferred from an upload or EDS access flag. The synthetic identity
read alone is declared non-consuming.

`ServiceEvidence` retains binding/tokens, owner qualification time, queue and
possible-transmission facts, protocol acknowledgement, copied upload bytes and
raw native failure data. The unsigned abort is independent of signed local
`Status::detail`; `transportDetail` carries the stack return in this adapter.

| Evidence kind | Qualification |
| --- | --- |
| `ACKNOWLEDGED` | Matching service exchange completed; never inferred from queue acceptance or CAN ACK |
| `PROTOCOL_REJECTED` | Checked matching server abort, with all unsigned abort bits |
| `REPLY_ERROR` | Native client detected an invalid response; a local abort is not a server rejection |
| `TRANSPORT_FAILURE` | Adapter failure or loss of qualified route; copied event provides reset evidence when applicable |
| `CANCEL` | Local work ended; physical motion is not stopped |
| `DEADLINE` | Immutable operation deadline reached, including time spent queued |

Any possibly transmitted unsuccessful transfer makes this proof's channel
unresolved, including an upload that may have a delayed reply. A checked write
abort preserves possible partial effects. The operation copies terminal service
evidence into its own context before the owner slot is released.

## Bridge and physical boundary

The private prompt-5 bridge owns one actual SDO client, one fake retirable TX
slot and a 32-byte FIFO. The outer owner is the sole public RX entry and holds
the canonical retained-result table. It delivers only SDO frames to the bridge;
heartbeat/EMCY/PDO frames go directly to the owner's raw observation ring, so
there is no second asynchronous event registry. A bridge terminal is copied and
released synchronously before another transfer starts.

The outer deadline governs queue time, final controller handoff and RX. The
bridge gets a non-earlier 65,535 ms timeout; this is proof host policy, not a
Lichuan timing guarantee. Admission bounds and overflow headroom are in the
owner contract. There is no retry. The upstream close call does not retract TX;
the fake bridge retires its software slot separately. Real TWAI/controller
acceptance can already make transmission possible and must preserve uncertainty
when hardware retirement is unproved.

The fake route begins externally qualified, with no earlier pending/duplicated
responses. On a successfully settled route, reuse assumes the controller/peer
does not later duplicate an old completed exchange. Local tokens/generations
cannot establish this premise or distinguish identical delayed SDO replies.
Failure quarantine is demonstrated; safe real route reuse and recovery remain
prompt 14/24/27 work, with actual queue/peer evidence. No quiet delay or fresh
host epoch is accepted as proof.

Prompt 5 demonstrates expedited and limited segmented exchanges. This bridge
does not drain a full incoming FIFO: 1–28-byte uploads are evidenced; larger
normal segmented uploads are not production-ready. Prompt 13 implements bounded
draining and the required transfer forms. Network state/heartbeat policy,
NMT/SYNC/PDO services, map qualification and bus-off recovery remain prompts
15–18/24. The proof's raw copied events are not a completed network stack port.
