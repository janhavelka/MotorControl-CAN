# CANopen service contract

Passive heartbeat/boot-up/EMCY handling uses the checked boundary and shared
owner projection described below. The [prompt-15 decision correction](decisions/canopen_stack.md#prompt-15-passive-boundary-correction)
records why native HB/EM consumers do not run alongside that projection.

The authoritative service engine is **CANopenNode**, selected in
[the pinned fit decision](decisions/canopen_stack.md), revision
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`. The installed portable package now
provides a checked scalar SDO boundary in
[`canopen/Sdo.h`](../include/MotorControlCAN/canopen/Sdo.h). It checks selected-stack
requests and decodes initiation replies; CANopenNode owns encoding and transfer
progression. The portable header has no stack dependency, session, transport,
clock, allocation or retry. Portable owned request/result contracts live in
[`SdoTransfer.h`](../include/MotorControlCAN/canopen/SdoTransfer.h). The optional
[`SdoClient`](../adapters/canopennode/SdoClient.h) application adapter implements
the bounded lifecycle around that same selected engine. It performs no
controller I/O and is not an installed-core dependency. CANopenNode retains its
Apache-2.0 notices. No SDK adapter, drive profile or physical support is implied.

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
overlap and unchanged outputs. These remain host evidence. The lifecycle below
now uses these checks; event codecs remain assigned to prompts 15/17.

## Bounded production SDO lifecycle

The optional adapter promotes the existing prompt-6 channel storage and
CANopenNode engine. Its fixed table has two physical channels on one interface,
one active or retained request per channel, one 32-byte native FIFO per client,
one owned upload buffer and one retirable software TX slot per channel. The
retained proof is now a compatibility facade over this implementation. There is
no second protocol engine, runtime driver registry or controller receive loop.

The application owns this stationary, noncopyable `SdoClient` and serializes all
calls. Its single interface owner offers frames through `receive`; a false
return leaves unrelated node/channel/heartbeat/PDO/EMCY traffic for ordinary
owner dispatch. Network-event decoding remains prompt 15. The owner explicitly
reports `reboot(node)` for every relevant node event and `busOff()` for the
interface; these invalidate channels regardless of observation-ring capacity.
The existing proof still demonstrates its four-slot raw event ring and boot-up
handling; the lifecycle does not create a competing event registry.

### Route and admission

`configure(slot, route, externallyConfirmed, freshSessionQualified)` requires
explicit IDs and an external fresh-session/local-TX/RX premise for every route.
Duplicate or crossing IDs reject even if logical axis, subaxis, profile or host
epoch differs. All slots belong to one physical bus. Configuration cannot rebind
or overwrite a quarantined slot. Multiple separate adapters must never own the
same physical SDO route; constructing a new object does not retire old traffic.

The EDS `1200:01/:02` defaults are the literal `$NODEID+0x600` and
`$NODEID+0x580`; they establish source metadata, not live configuration. No IDs
are derived, probed with writes or silently installed. `externallyConfirmed`
means bounded, applicable, reviewed non-changing confirmation already occurred
under the fresh-session premise. If false, only an explicitly reviewed
non-changing upload can be admitted until one completes. Its success confirms
the selected route but cannot establish freshness retrospectively. Current
Lichuan read-effect gates remain unresolved; the synthetic host fixture's
non-changing read is not manufacturer qualification.

`admit` validates the complete candidate and reserves its result before copying
all work. It records full target/binding/configuration generations, operation,
request and step identities; these host fields never appear on ordinary SDO
wire frames. Local rejection publishes neither a handle nor a terminal. Zero
identities, invalid routes, size zero/above 32, unsupported direction, zero
budgets, occupied/quarantined channels and expired/backward time reject before
TX. The result reservation survives all service turns until explicit `release`.
A released handle cannot affect a later admission; generations never wrap.

Object access and effect permission belong to the reviewed profile caller.
This service mechanism supplies no raw Lichuan operation, setter, retry or
escape around unresolved typed policy. The optional adapter supports exact
1-32-byte uploads/downloads. All 202 EDS scalar entries are 1-4 bytes; the larger
bound preserves the retained segmented proof and is not another object
catalogue or advertised device capability. Block transfer is disabled.

### Transfer forms and dispatch bounds

Sized expedited scalar replies must agree with the expected width. For legal
unsized `42` replies, CANopenNode internally copies four bytes; the checked
adapter publishes only the admitted 1-4-byte prefix and retains the entire raw
frame. Every download waits for the matching native acknowledgement. Server
aborts must match index/subindex before entering the upstream callback, and all
32 bits remain unsigned. Native abort evidence is not a malformed frame and
does not roll back earlier writes. After possible TX, a matching server abort
also terminates work while the next segment is still in local staging. That
staged frame is retired before processing the abort; a wrong-object abort
cannot discard it. Before the initial handoff, no abort can belong to the
untransmitted request.

A server may choose segmented upload for a small scalar. Segmented transfers
use the actual selected engine's toggle/last-bit protocol. Declared size and
final total must agree with the explicit request; capacity and command bits are
checked. The adapter drains the FIFO after each progression call, so all sizes
through 32 work without the proof's former 28-byte boundary. No progress loop
runs inside a service call. `service` makes at most one initiation, two engine
progression calls and one FIFO drain per channel. `receive` adds at most one
callback, two progression calls and one drain for its selected channel. `nextTx`
uses the same service bound and hands off at most one frame; zero application
TX budget means the owner omits that entry.

When `allowSegmentedUpload=false`, a valid segmented initiation terminates as
`UNSUPPORTED`, preserving the sent initiation and possible effects. The selected
engine stages a protocol abort. Malformed/size/toggle failures similarly retain
local client-abort evidence. A cleanup abort can be handed off only within the
original remaining queue/overall budget; otherwise it is retired. The immutable
terminal states whether cleanup was staged, never that it reached the server.
Cleanup can survive terminal result release; its later handoff never clears
quarantine. No abort is automatically replayed.

### Time and precedence

All times are supplied nondecreasing, nonwrapping `uint64_t` microseconds;
zero is valid. The absolute exclusive `deadlineUs` is copied at admission and
never renewed. `queueTimeoutUs` and `responseTimeoutUs` are explicit nonzero
host policies, not Lichuan timing guarantees. Each cutoff is computed from the
remaining overall budget before addition, including near `UINT64_MAX`. Native
per-step timers receive zero elapsed time: owner microsecond cutoffs govern all
queue, transmission and response intervals instead of the stack resetting an
overall lease after progress.

| Boundary | Precedence and retained meaning |
| --- | --- |
| Service/RX/TX at overall cutoff | Expiry wins before callbacks or handoff, including equality |
| Staged frame reaches queue cutoff | Retire software bytes and publish timeout; never transmit them later |
| `nextTx` succeeds | Copies one frame and marks possible TX before returning, since controller acceptance may immediately transmit |
| `confirmTx` | Records an actual controller TX/ACK report for the latest live frame token, even after a nonfinal reply; the next handoff supersedes that token. Cannot complete SDO, extend a cutoff or rewrite a terminal |
| Response cutoff after handoff | Clamped to overall deadline; CAN ACK/unrelated traffic/progress cannot extend the total budget |
| Cancel | Expires first, then cancels without pumping unsent work; retires local staging and never means stop |
| Bus-off/reboot/binding withdrawal | Expiry first; quarantine relevant channels and retire local staging; earlier immutable terminals remain unchanged |
| Known capture after processing, or backward processing time | Reject without mutation, including observations; never infer a clock conversion |
| Capture before a cutoff, processing at/after it | Processing expiry wins; retain capture and processing times separately without accepting the reply |
| Capture at/after a cutoff | Cannot be timely confirmation: processing is at least as late as capture |
| Valid response before handoff, while idle or after terminal | Quarantine as unsolicited/duplicate traffic; close live work as `CORRELATION_LOST`, leaving any existing terminal unchanged |
| Known capture before the relevant handoff | Quarantine as provably stale, even when object/command match |
| Cancel/failure processed before a queued earlier RX | First terminal wins; retain the later-dispatched raw observation without rollback |
| Success published before cancel/failure | Keep success and its effects; a later bus-off/reboot/invalidation or duplicate still quarantines the route |
| Events with equal processing timestamps | Cutoff expiry first; otherwise cooperative call order determines the first terminal |

`receive(frame, nowUs)` leaves capture time unknown. The three-argument overload
accepts a source-qualified `receivedUs` in the same expanded monotonic domain as
processing time. Neither form backdates processing or revives a terminal. Clock
wrap must be expanded outside this adapter; raw counter wrap is rejected as
backward time. Equal capture/handoff timestamps cannot establish finer ordering.
The session premise still applies; timestamps and host tokens are not wire tags.
This is deliberately more conservative than RS's captured-history settlement
path; [the source refresh](reports/prompt_14_sources.md) records that difference.

Every frame offered with a configured response identifier has one copied
`SdoRxObservation`: route, raw bytes/flags, optional capture time, processing time
and disposition. Unsupported format or a wrong object is `REJECTED`; a live
response entering validation is `PROCESSED`, including one causing client abort;
old/unsolicited traffic on a quarantined route is `UNRESOLVED`. The latest view
survives result release and new admission, but is overwritten by the next frame
with that identifier. It is not a history queue or attributed to a new local
request. Applications copy it when a longer bounded history is needed. Unmatched
identifiers still belong to the continuous physical owner dispatcher.

An ordinary download acknowledgement echoes index/subindex, not written data.
Changing a value, operation number, step, generation or delay cannot identify an
old `60` response. During a live initial exchange the adapter checks the object;
later segment command/toggle checks belong to the selected engine. Known stale
capture or an unsolicited frame invalidates the premise even if its object is
different. An indistinguishable duplicate arriving only after a new handoff
cannot be detected without external evidence. Every exchange requires exclusive
ownership and no delayed duplicate response from any completed initiation or
segment exchange, including segments within the same active transfer.

The segmented toggle is one bit, not a sequence number: download acknowledgements
repeat `20, 30, 20`. A delayed first `20` can therefore look like the third
segment's acknowledgement. The retained counterexample deliberately violates the
external premise and demonstrates that the unknown-capture path cannot detect
this case. Supplying its original pre-handoff capture time instead quarantines
the route. A duplicate that first arrives locally after the later handoff is
still indistinguishable, even with an accurate local timestamp. Capture time is
not server-generation time. Host tests document these limits; they do not prove
a real session or authorize its configuration.

### Evidence, effects and quarantine

`SdoCompletion` is a non-consuming immutable view until `release`. It separately
retains admission intent, local queueing, possible and confirmed TX, matching
protocol success, raw server/client abort, cause, supplied terminal time, raw
reply and owned upload value. `effectPossible` remains true for a transmitted
download or unqualified upload, even on service success. `effectUncertain`
retains possible changing effects on failure. Only source-reviewed non-changing
uploads can have no changing effect; even their late replies can corrupt
correlation and therefore quarantine the channel on uncertain failure.

Any unsuccessful possibly transmitted request makes the physical channel
unresolved; transport failure, reboot and bus-off also invalidate it. Release,
quiet time, host epochs, a client abort or construction do not requalify it.
`invalidate(slot, nowUs)` withdraws exactly one binding without replacing its
physical route, generations or retained possible effects. Reboot invalidates
every channel of that node; bus-off invalidates the whole adapter. Other
explicitly configured channels/nodes can continue under their own premises.
Same-node alternate channels require actual independent server-channel evidence,
distinct identifiers and a separate fixed slot. Relabeling the same peer channel
with different IDs, logical axes or host generations is rejected. This is not a
claim that CL86-C has a second server channel or an alternate stop route.

All 202 reviewed Lichuan entries have unestablished read effects (Q20); all 194
generated descriptors remain `UNQUALIFIED`. No read is asserted destructive,
but no discovery/reconciliation upload is promoted to non-changing from access
flags. Multi-write callers keep each prior success and later possible effect;
a matching server abort on a later request does not undo an earlier write.

### Unsupported reopening and required evidence

Prompt 14 implements no reopening capability. Negative tests keep admission
blocked for every unsupported path below, rather than supplying a success stub.
The [source report](reports/prompt_14_sources.md) identifies the actual pinned
stack, Lichuan documents and installed controller API inspected.

| Proposed path | Why insufficient / evidence needed before implementation |
| --- | --- |
| Quiet interval, longer timeout or new host epoch | No established maximum server response/retransmission or local delivery delay; need all bounds plus an ordered drain/retirement boundary |
| Client abort queued, handed off or CAN-ACKed | No server acknowledgement of abort or proof previous effects vanished; need applicable remote-session settlement and local queue disposition |
| Heartbeat or boot-up alone | Proves only the observed network event; need matched firmware reset/session semantics and ordering against retained TX/RX and other clients |
| Controller stop/recovery/start | Inspected TWAI queue clearing concerns local driver storage; need application/cache/active-frame retirement and independent remote-server settlement, not just queue acceptance |
| Reconstruct adapter or rebind same server | Local tokens do not tag wire data; need externally established old-session exclusion before constructing any replacement |
| Read to reconcile an uncertain write | Need Q20 effect permission, applicable value semantics and fresh correlation; equal readback does not prove which write happened |

Q18 retains server/timing evidence, Q16 altered-route activation, Q01 selected
model/firmware provenance, and Q20 read effects. These remain precise device
qualification gates, not unfinished software reopening procedures. No reset,
save, stop, retry or discovery write is added to obtain evidence.

### Build and host evidence

The full-checkout build automatically registers the selected adapter and
[independent fake-controller/node tests](../test/sdo_transfer_test.cpp) and
[adversarial settlement tests](../test/sdo_reconciliation_test.cpp). The
[adapter guide](../adapters/canopennode/README.md) gives optional-target commands.
Installed/exported core consumers need no adapter, upstream source, SDK, ledger,
Python or reference material; portable service value types compile independently.
No CAN traffic, live drive acknowledgement, motion or hardware qualification is
claimed by the native harness.

## Passive network events and node lifecycle

Installed `canopen/Network.h` contains pure `validateNetworkRoute` and
`checkNetworkEvent` functions. They borrow inputs only during the call and
publish an owned event atomically. The optional `SdoClient` consumes those
events through the same receive entry as SDO. There is no second controller
receiver or installed node registry. Native NMT and EMCY facts are not common
motion requirements for proprietary protocols.

### Explicit routing and storage

`configureNetwork(slot, route, observer, nowUs)` adds a passive node anchor in
the existing two-slot channel table. It may precede SDO configuration: passive
monitoring needs no SDO fresh-session premise. One anchor owns a physical
bus/node; another configured SDO channel of that node shares `node(slot)`.
All slots belong to one interface. Duplicate anchors and crossing heartbeat,
EMCY or SDO identifiers reject in either configuration order. Routes contain
the full host binding, nonzero event generation and actual standard CAN-IDs;
neither source-node identity nor IDs are guessed from low identifier bits.
An invalidated passive anchor cannot be used to qualify a new SDO alias.

| Route | Required Classical frame shape | Checked payload |
| --- | --- | --- |
| Configured heartbeat ID | Standard data, DLC/length 1 | `00` boot-up; `04` Stopped; `05` Operational; `7F` Pre-operational |
| Configured EMCY ID | Standard data, DLC/length 8 | Little-endian U16 error code, complete register byte and five opaque manufacturer bytes |
| Configured SDO response | Existing selected channel and reviewed transfer expectation | Ordinary SDO validation, independent of events |
| Unknown ID or unsupported event | Copied diagnostic only | No discovery, protocol switch, inferred native state or command result |

Unknown heartbeat states, including `85`, cannot update valid NMT/heartbeat
facts. Unknown/zero EMCY codes and reserved register bit 6 remain raw evidence;
CANopenNode's private byte-3 convention is not imposed on Lichuan. EMCY zero
does not clear an application drive fault. A fresh heartbeat changes no drive
state/mode, SDO failure, position, reference or configuration evidence.

One four-record FIFO retains copied raw event/unknown-frame observations.
`takeNetworkObservation` copies one independently owned entry; overflow drops
the newest history record and saturates `droppedNetworkObservations`. Normal
snapshot updates, EMCY counting and reset invalidation occur even when full.
The latest EMCY and its originating route survive invalidation as history.
Snapshot views are borrowed until the next mutation; copy them for retention.
No consumer borrows callback or controller RX bytes.

### Time, generations and independent facts

Every timestamp uses the supplied nonwrapping monotonic microsecond domain.
Capture zero is valid. `receive(frame, receivedUs, nowUs)` preserves source
capture; the two-argument form leaves capture unknown. `receiveNetwork` also
accepts binding/event-route provenance captured at earliest ingress. It rejects
old binding/epoch qualification while retaining those original tags in history.
A host epoch is not on the wire and cannot identify delayed remote traffic.
Future capture or backward processing time rejects before service or mutation.

Configured node snapshots keep heartbeat, observed NMT and last checked matching
SDO times independently. Malformed SDO frames cannot renew that communication
fact. A valid abort proves matching service communication without success.
Older heartbeat capture cannot replace a newer heartbeat; EMCY has its own
ordering, so an EMCY older than the latest heartbeat remains independent evidence.
Unknown capture cannot replace a known newer observation. After invalidation,
unknown-capture heartbeat/EMCY are diagnostic only; known capture must be
strictly after the processing boundary. An explicitly old route epoch always
fails. This conservatively rejects already queued pre-boundary samples.

Boot-up is a reset observation, not a periodic heartbeat: it cannot start
heartbeat monitoring. Unless capture/provenance proves it predates the current
lifecycle boundary, even a delayed boot-up conservatively invalidates caches.
A later heartbeat does not prove that earlier configuration survived that reset.
When a delayed boot-up predates a newer NMT observation, that newer observation
remains historical with its old generation; qualify NMT only when its timestamp
epoch matches the current route and the node is not exhausted.

`setProducerInterval` records an externally established effective U16 interval
in milliseconds. Zero explicitly means disabled. EDS default zero and packet
cadence do not establish that live fact. `heartbeatFreshness` distinguishes
unknown producer, disabled, never observed, unqualified, fresh and expired;
its nonzero maximum age is explicit application policy. Age uses original
capture and an inclusive limit; processing delay never renews it. Expiration
does not assert a drive watchdog action, motor stop or application health state.

### Invalidation and actual application dependencies

Boot-up, explicit reboot, `invalidateNetwork` and legacy binding withdrawal
revoke the node's SDO channels and advance its event generation. Bus-off and
reported controller recovery revoke all configured nodes. Controller AVAILABLE
restores neither node evidence nor channel admission. Generations never wrap:
at maximum, the node stays exhausted/unqualified while invalidation still runs.
Prior terminal results and possible effects remain immutable; local TX staging
and cached handoff eligibility are retired, and no write is replayed.

Register a bounded synchronous `NetworkObserver` for actual dependent caches.
It runs after all affected channels have been retired and before another TX can
be yielded, independently of history space. The stationary context/callback
are borrowed, must not reenter the owner or perform I/O, and identical pairs
are notified once per node event. Each notification carries the dependent
binding and revoked/new epoch. Callers retaining configuration/PDO maps, scale,
drive state/mode, references or prepared operations must revoke their actual
dependencies here or check the current epoch before every use.

The native fixture connects this callback to production
`invalidateAxisReference(..., COORDINATES)`, revokes readback-dependent command
and encoder scales and invalidates prepared work. Axis invalidation deliberately
does not itself clear scale provenance: the cache owner does that according to
its source dependence. Raw faults/history and interrupted command outcomes are
retained. Future typed settings/PDO modules use this same notification; this
stage installs no placeholder cache or generic health state machine.

The original EDS documents producer `1017` U16/default zero and EMCY route
`$NODEID+0x80`. It contains no `1016`, `100C` or `100D` entries. That absence is
not proof of unsupported firmware behavior, and none of these host observations
establishes a Lichuan host-loss stop guarantee. No implicit NMT, heartbeat,
drive or PDO configuration write is performed.

## Explicit NMT manager requests

Installed `makeNmtRequest` encodes only the five reviewed commands: Start `01`,
Stop `02`, Pre-operational `80`, Reset node `81` and Reset communication `82`.
The standard data frame has ID zero and exactly two bytes: command, target.
Targets 1–127 address a node; target zero is a domain broadcast. NMT Stop is
neither a physical motor stop nor a CiA402 quick stop. This codec is pure.

The optional owner provides `admitNmt`, `nextNmtTx`, `canTransmitNmt`,
`confirmNmtTx`, `cancelNmt`, `nmtResult` and `releaseNmt`. The application must
explicitly act as network manager. One request/result slot bounds manager
work independently of the two SDO slots. Every local SDO binding must have a
network anchor before admitting NMT. An addressed request carries that exact
route/epoch; a broadcast carries the physical bus and snapshots all local
bindings, including passive-only nodes and additional SDO channels. It never
creates an axis at node zero. Unbound remote nodes are affected by the actual
broadcast too. Configure/rebind membership only after releasing the request.

Admission copies intent without traffic. Service invokes the actual pinned
`CO_NMT_sendCommand` producer once. Queue refusal or cancellation/expiry before
handoff leaves prior observations and producer facts unchanged. `nextNmtTx`
marks possible transmission before yielding a copy: do not replay it. As with
SDO, serialize the pure `canTransmitNmt` check and actual enqueue with lifecycle
events. After that conservative boundary, cancellation cannot prove no effect.
`confirmNmtTx` records actual controller TX and publishes **TRANSMITTED**, a
local result. NMT has no remote acknowledgement; neither CAN ACK nor a later
heartbeat can manufacture one. Results remain immutable until release.

Queue cutoff and overall deadline are immutable, exclusive caller microsecond
budgets, including equality and `UINT64_MAX` arithmetic. Stalled staging,
unconfirmed handoffs and pending observations expire without extending those
budgets. Each service turn handles at most one NMT staging call plus two node
wait expirations, in addition to the documented SDO bounds. Deadline expiry
wins over lifecycle reports at the same time. No vendor boot delay is assumed.

### Intent, observations and invalidation

`NodeSnapshot` retains requested command/identity, handoff boundary, original
deadline and independent pending/observed/conflict/expired/invalidated facts.
The old observed state/time and SDO time stay history. After possible NMT TX,
current qualification requires known capture **strictly after** that handoff,
the current lifecycle epoch and the caller's age policy. Unknown capture,
pre-handoff/equal timestamps and elapsed time cannot qualify the requested
state. A conflicting current heartbeat records the actual state while retaining
the conflict; it never gets rewritten to match intent. A match settles the
observation wait independently of the local TX result. Unrelated reset/binding/
controller invalidation retires that wait; later epochs cannot satisfy it.
Only a qualified post-handoff boot-up can observe a pending reset request.

Start and Pre-operational preserve active SDO deadlines. Stop handoff or a
known-current Stopped heartbeat retires affected SDO staging/handoffs and
finishes live work through the existing uncertainty rules. A proven unsent
request does not become an uncertain write; a possibly transmitted write stays
uncertain/quarantined. Nonreset transitions preserve configuration/map contents,
scales and reference facts: `NMT_TRANSITION` callbacks revoke only dependent
service/prepared eligibility, with the command and unchanged epoch. Applications
must enforce that distinction. A requested reset uses `NMT_RESET`, advances
the lifecycle epoch and revokes configuration/map/scale/reference evidence and
all affected SDO sessions, as actual boot-up does. Neither later heartbeat,
Start, host token nor local result release reopens an unresolved SDO channel.
Broadcast applies these rules once per physical node and notifies each distinct
dependent observer; affected binding/previous/new epochs remain in its result.

### Service eligibility

`serviceEligibility` is the maintained CANopen policy boundary. The following
table describes eligibility only; reviewed object access/effect, drive
prerequisites, channel qualification, deadlines and actual PDO map/provenance
are additional requirements. SDO admission **and each frame handoff/enqueue
check** apply it. A finite evidence-age policy can therefore end further
ordinary TX during an admitted transfer; it never renews the overall deadline.

| Qualified current evidence | Ordinary SDO | Reviewed non-changing probe | PDO transmit | Configured TPDO receive validation |
| --- | --- | --- | --- | --- |
| Pre-operational | Allowed | Allowed | Denied | Denied |
| Operational | Allowed | Allowed | Allowed | Allowed |
| Stopped / Initializing | Denied | Denied | Denied | Denied |
| Exact state unknown, current SDO service evidence | Allowed | Allowed | Denied | Allowed to enter validation |
| Exact state and serviceability unknown | Denied | Allowed explicitly | Denied | Allowed to enter validation |
| Exhausted lifecycle provenance | Denied | Denied | Denied | Denied |

`setServiceAge(slot, maximumAgeUs, nowUs)` selects a nonzero inclusive age cap
for exact NMT and SDO service evidence; it does not refresh either fact. The
documented default `UINT64_MAX` selects lifecycle validity without age expiry,
not a guessed device timeout. Applications needing bounded observation age
must choose their finite cap. Capture age, not processing time, determines
freshness. Explicit external service confirmation is recorded separately in
`serviceQualification` with `serviceFromSdo=false`; the last matching SDO time
is not fabricated. Checked matching responses, including a valid rejection,
update service evidence with SDO provenance while preserving channel quarantine.

Effective heartbeat disabled/unknown is independent of this table. With no
current exact state or service fact, explicitly request a bounded, reviewed
non-changing upload on a qualified fresh channel. Once it succeeds, a reviewed
producer-setting SDO write can be admitted without requiring heartbeat first.
It still needs actual object/drive prerequisites and explicit caller intent;
no current Lichuan read is newly declared non-changing here. A current Stopped
observation blocks that probe. An explicit transition that may have transmitted
makes prior Stopped historical, permitting the bounded probe while exact state
remains unknown. No NMT Start or heartbeat write is issued automatically.

Prompt 17 must offer a configured TPDO to its map/binding/capture validation
while exact NMT is unknown; do not gate the first possible observation on
`pdoTransmit`. This stage makes **no Operational inference** from TPDO. If a
later profile chooses one, it must retain TPDO provenance and establish current
route/map identity beyond a reused same-ID/same-length sample. The protocol
rule is pinned `CO_PDO.c:1348–1359` (valid TPDO production requires Operational),
not permission to trust arbitrary traffic. SDO success and NMT TX cannot infer
Operational. Service/NMT state never substitutes for drive or application health.

### Pinned producer boundary and evidence

CANopenNode pin `9b8beed8367241e96ac03f916cd5a500bcb2cf23`,
`301/CO_NMT_Heartbeat.h:40–56,81–87,272–281` defines the states, service
availability, target zero and command bytes. Its C producer at lines 274–288
only accesses `nodeId`, `internalCommand` and two producer pointers. The optional
adapter initializes that restricted field set with a retirable ID-zero/DLC-two
port and invokes only `CO_NMT_sendCommand`. It never calls full NMT init/process
or registers NMT RX: full initialization requires a local OD/EM context, and
processing emits local boot-up even with heartbeat disabled. The inert local
broadcast `internalCommand` assignment is not a remote observation. No second
NMT/EMCY/heartbeat engine is active. The 14-file offline subset retains upstream
licenses and exact hashes; pure installed consumers depend on none of it.

The original EDS lines 26–27 claim `SimpleBootUpMaster=0` and
`SimpleBootUpSlave=1`. They establish neither a measured startup/reset timeline
nor a fixed response deadline. Producer `1017` default zero is factory metadata;
missing consumer/guarding entries establish no host-loss behavior. These are
documented source claims, not measured firmware support or hardware qualification.
