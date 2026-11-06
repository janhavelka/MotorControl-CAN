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
uses these checks; the passive network events section below covers the event
codecs.

## Bounded production SDO lifecycle

The optional adapter promotes the existing prompt-6 channel storage and
CANopenNode engine. Its fixed table has two physical channels on one interface,
one active or retained request per channel, one 32-byte native FIFO per client,
one owned upload buffer and one retirable software TX slot per channel. The
retained proof is now a compatibility facade over this implementation. There is
no second protocol engine, runtime driver registry or controller receive loop.

The application owns this stationary, noncopyable `SdoClient` and serializes all
calls. Its single interface owner offers every frame through `receive`, which
routes configured PDO, heartbeat, boot-up and EMCY identifiers itself before SDO
matching. A false return means live protocol validation did not consume the
frame. The frame may still have been recorded or have quarantined a route; the
owner counts it in `Stats.unmatchedFrames` and does not route it further. A valid
boot-up invalidates that node's channels, and the owner's `reportController`
invalidates all channels on bus-off or recovery, regardless of observation-ring
capacity. `reboot(node)` and `busOff()` remain adapter entries for a caller
holding such evidence outside the frame path. Network observations enter a
four-slot history read through `takeNetworkObservation`; the lifecycle does not
create a competing event registry.

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
means a bounded, applicable upload already completed under the fresh-session
premise (a reviewed or owner-accepted one; see below). If false, ordinary
admission requires such a probe upload until one completes. Its success confirms
the selected route but cannot establish freshness retrospectively. The finite
experimental exception below grants one unknown-effect upload without promoting
that route to ordinary service eligibility.

Read-effect policy has three members. `REVIEWED_NON_CHANGING` needs an actual
source review; no Lichuan row uses it today. `ACCEPTED_UNQUALIFIED` is the
repository owner's risk acceptance of 2026-11-02 (DEC-1) for a fixed object
list. Packet 11 ran those reads live on two specimens with no read-effect
observation. It is not evidence that a read has no
effect: `effectPossible` stays true and the ledger `read_effect` stays
`not_established`. A first accepted upload can confirm an unconfirmed channel;
it cannot establish freshness. Everything else is `UNQUALIFIED` (Q20). The
synthetic host fixture's reviewed read is not manufacturer qualification.

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
escape around unresolved typed policy. `SdoUploadSize::EXACT` is the default:
the optional adapter supports exact 1-32-byte uploads/downloads. The separately
granted raw upload mode below treats size as capacity. All 202 EDS scalar entries
are 1-4 bytes; the larger bound preserves the retained segmented proof and is not another object
catalogue or advertised device capability. Block transfer is disabled.

### One-use experimental upload grants

`authorizeDownloadExperiment` shares the same one-use grant storage and review
serial for an independently reviewed exact scalar download of 1–4 bytes. It
copies and matches the payload as well as the complete request identity, state
prerequisites, node generation and budgets. It permits no implicit write and
does not turn experimental success into service confirmation or production
capability. `clearUploadExperiment` withdraws either unused grant; neither call
clears quarantine, cancels admitted work or permits replay. The application must
retain the exact write procedure, source basis, fixture authorization and
independent control/reconciliation plan.

`authorizeUploadExperiment(slot, exactUpload, nodeGeneration, authorizationId,
nowUs)` copies one explicitly reviewed `UNQUALIFIED` upload. It requires the
configured fresh-session/exclusive-server premise, a current nonzero passive
node epoch, eligible controller/NMT state and no occupied transfer, cleanup,
unresolved route or live handoff. The nonzero authorization serial increases
per channel; operation IDs cannot decrease and steps within the same operation
must increase. Clearing a grant does not allow its step to be authorized again.
The application retains the actual fixture, authorization and finite experiment
review under that serial; the adapter cannot authenticate those external facts.

The grant matches the complete relevant upload intent: route/binding epochs,
operation/request/step, object/subindex, exact width or raw capacity, size policy,
read-effect policy, segmented permission, deadline and queue/response budgets.
An object-only permission or altered candidate is insufficient. Native service
admission consumes the grant once; an accepted transfer retains its permission
through the ordinary owner handoff. `clearUploadExperiment` withdraws only an
unused grant. It performs no I/O and does not cancel work already admitted;
the owner must cancel that work explicitly when ending an experiment.

Experimental success retains SDO observation and unknown-effect evidence. It
does not establish a new channel confirmation or `serviceQualification`, change
`UNQUALIFIED` to `REVIEWED_NON_CHANGING` or `ACCEPTED_UNQUALIFIED`, authorize a
download, or admit another upload. An already established confirmation remains separate. No grant or failed
attempt is automatically replayed. Reboot, binding withdrawal, bus-off, expiry
and uncertain completion retain their ordinary invalidation/quarantine rules.

`SdoUploadSize::SERVER_INDICATED` is available only for an exact experimental
grant, including on an already confirmed channel. `SdoTransfer::size` is its
1-32-byte capacity; successful `SdoCompletion::size` is the actual returned count.
A raw expedited reply must indicate its width: `43/47/4B/4F` are bounded by that
capacity; unsized `42` cannot establish an actual count and is rejected. Sized
or unsized segmented transfers remain bounded by capacity, native declared-size
consistency, toggles and the immutable deadline. Overflow or malformed transfer
publishes no successful value and retains its failure/raw-reply evidence.
The policy establishes no native object type, unit or firmware applicability.

Production typed read completion matching and PDO-map readback require `EXACT`;
they reject raw-capacity evidence even when its returned count happens to equal
the reviewed width. The bench-only [raw collector](../examples/s2_bench/RawEvidence.h)
retains such bytes as observations through this same owner. It neither repairs
the EDS nor changes the production object ledger. The
[service vectors](../test/sdo_transfer_test.cpp),
[owner grant tests](../test/can_bus_owner_test.cpp) and
[collector tests](../test/raw_evidence_test.cpp) exercise this distinction.

### Transfer forms and dispatch bounds

Under the default `EXACT` policy, sized expedited scalar replies must agree with
the expected width. For legal unsized `42` replies, CANopenNode internally copies
four bytes; the checked
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
use the actual selected engine's toggle/last-bit protocol. Under `EXACT`, declared
size and final total must agree with the explicit request; capacity and command bits are
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
retains possible changing effects on failure. Only an upload with an actual
source review (`REVIEWED_NON_CHANGING`) could be treated as having no changing
effect; even its late replies can corrupt correlation and therefore quarantine
the channel on uncertain failure. An `ACCEPTED_UNQUALIFIED` upload keeps
`effectPossible` true.

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

All 202 reviewed Lichuan entries have unestablished read effects (Q20); the
ledger `read_effect` is `not_established` for every one. Thirteen object groups
(`1000`, `1018:00..03`, `1009`, `100A`, `2000`, `2001`, `6041`, `6061`, `6064`,
`606C`, `2100`, `2101`, `2102`) carry `ACCEPTED_UNQUALIFIED` by owner decision
DEC-1; this is risk acceptance, not evidence of no effect. `603F`, `1001` and
`1003` stay `UNQUALIFIED` explicit single reads, never polled, reached only
through `requestExperiment` or an action's prerequisites; the first bench read of
each records LED fault state before and after. Naming a fault code as the most
plausible read-to-clear object is an audit judgment, not a source claim. All
other descriptors stay `UNQUALIFIED` under Q20 (owner prompts 19, 29, 37, 41, 44,
47). No read is asserted destructive, and no upload is promoted from access
flags. Multi-write callers keep each prior success and later possible effect;
a matching server abort on a later request does not undo an earlier write.

### Settling and reopening a route

A matched server abort on a clean exchange, meaning a reply to the one confirmed outstanding frame, has the premise a success has. It reports the rejection, keeps earlier effects uncertain, ends queued steps of its sequence and leaves the route open. An abort with a staged next segment, or with its frame still live in transport, quarantines.

A controller event, bus-off or recovery, quarantines only a route with a frame that may have left: `SdoClient::possibleWork`, sampled before retirement. A route with nothing possible stays open and needs NMT or service evidence for ordinary work. A boot-up or rebinding still quarantines.

**Drain marker (DEC-3).** The only way to reopen. An application explicitly requests one finite marker through `CanBusOwner::admitDrainMarker`:

| Rule | Detail |
| --- | --- |
| Premises | `SdoDrainPremises`: sole client on these COB-IDs, and a server that answers one request at a time and in order. **No retained Lichuan or CiA source establishes the second.** It rests on memory of CiA 301 and the protocol's shape, and classic SDO carries no transaction tag. It stays open under Q18, so no real fixture sets it and real routes stay blocked. Tests that set it name it as a fake |
| Local TX | Every frame on the route retired, and the marker's own frame retired before reopening |
| Marker | One accepted expedited upload whose object differs from the last completed transfer and from each of up to four retained pairs. Pairs come from unresolved requests and unsolicited frames |
| Refusal | Retained-pair overflow, a frame with no multiplexer, or any unexpected frame after an earlier reopening: that evidence contradicts the premises, so the route refuses markers from then on |
| While pending | A retained pair is ignored, because it may be the old reply still arriving. Any other frame fails the marker and is retained |
| Success | The owner invalidates every reservation on the node first, so an old reservation and its old command history cannot come back. Armed experiment grants are void. `requalifications` counts it. Service qualification is not refreshed |
| Failure | The route stays unresolved. A further attempt is new explicit intent with a different object |

A marker settles the channel only. Earlier effects stay uncertain, and nothing replays or re-enables. **If the ordered-server premise is false, a stale reply naming the same object as a later write can still confirm that write, and no client-side check can tell.** That is why the premise is a gate, not a default.

### Best-effort stop on an unresolved route (DEC-2)

`SdoClient::checkBestEffortStop` and `admitBestEffortStop` pass one exact request, `6040:00` U16 `000B`, on a route that ordinary work may not use. The frame is submitted at most once. A controller TX report ends it `SdoOutcome::UNCONFIRMED_SENT`, with execution unknown. A frame received meanwhile, even a matching `60 40 60 00`, is retained: it retires nothing and qualifies nothing, because it may answer the write before. The route stays unresolved and the marker above is still required. The [owner contract](can_owner.md) gives the admission table. This is not a reopening path and not a stop result.

### Other proposed reopening paths

Negative tests keep admission blocked for every path below, rather than supplying a success stub.
The [source report](reports/prompt_14_sources.md) identifies the actual pinned
stack, Lichuan documents and installed controller API inspected.

| Proposed path | Why insufficient / evidence needed before implementation |
| --- | --- |
| Quiet interval, longer timeout or new host epoch | No established maximum server response/retransmission or local delivery delay; need all bounds plus an ordered drain/retirement boundary |
| Client abort queued, handed off or CAN-ACKed | No server acknowledgement of abort or proof previous effects vanished; need applicable remote-session settlement and local queue disposition |
| Heartbeat or boot-up alone | Proves only the observed network event; need matched firmware reset/session semantics and ordering against retained TX/RX and other clients |
| Controller stop/recovery/start | Inspected TWAI queue clearing concerns local driver storage; need application/cache/active-frame retirement and independent remote-server settlement, not just queue acceptance |
| Reconstruct adapter or rebind same server | Local tokens do not tag wire data; need externally established old-session exclusion before constructing any replacement |
| Read to reconcile an uncertain write | Need read-effect permission (Q20 or the DEC-1 list), applicable value semantics and fresh correlation; equal readback does not prove which write happened |

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

Attaching an anchor preserves initial usable SDO evidence with its original
capture/processing times and external-versus-SDO origin. Attachment time is
not confirmation time. Quarantined channels contribute no initial premise;
their raw channel observations remain available separately. Initial SDO history
and the strongest service proof are selected independently across channels.

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

The four-argument `receive(frame, earliestUs, latestUs, nowUs)` retains a separate
conservative completed-frame receive interval. Exact capture remains unknown.
Invalid, reversed or future bounds reject before mutation. Network age uses the
oldest possible receive time to prove freshness and the newest possible time to
prove expiry; a policy-straddling interval is unqualified. Lifecycle and NMT
qualification require the whole interval strictly after the relevant boundary.
Contradictory overlapping NMT observations withdraw state qualification until a
provably later observation arrives. See the [TWAI receive-time proof](adapters/twai_receive_time.md).
SDO deadlines and correlation remain independent; a wholly pre-handoff interval
quarantines stale traffic. A checked reply records SDO service evidence from its
interval under the exact-capture rule, with the lower bound as its receive time:
it must start strictly after the request's handoff and after the node boundary
(strictly after an invalidation), it orders and ages by that bound, and it is
current only when that bound follows a possible NMT transition. An interval
reaching back to its handoff or straddling a boundary still settles the request
but records nothing ([the rule and its evidence](adapters/twai_receive_time.md#sdo-service-qualification)).
Bounds never promote SDO/PDO observations to exact capture or qualify reused PDO
routes.

`invalidateProducerInterval` explicitly withdraws effective heartbeat producer
timing before a requested change can transmit. It preserves raw old timing,
receive history, epochs and channel state. Only subsequent effective evidence
passed to `setProducerInterval` restores that timing premise; successful TX alone
does not establish it.

The protocol millisecond unit for `1017h` is independently shown in CiA's
[official CANopen poster, page 1 heartbeat diagram](https://member.can-cia.org/files/canopen_poster_2024.pdf).
The original PDF was reopened and visually checked on 2026-11-02 (SHA-256
`f7a498e61dbd45641748f6c1cd809cceb2e796504783afe36358f2cd77635ac0`).
CiA also describes `1017h` as the configurable heartbeat cycle and the heartbeat
payload as the producer's NMT state in its
[error-control overview](https://www.can-cia.org/can-knowledge/error-control-protocols).
The retained Lichuan EDS `[1017]`, lines 261–269, supplies U16/RW/default-zero
metadata but no unit or firmware activation guarantee. The protocol source does
not resolve that separate device qualification gap or establish a host-loss stop.

Configured node snapshots keep heartbeat, observed NMT and last checked matching
SDO times independently. Malformed SDO frames cannot renew that communication
fact. A valid abort proves matching service communication without success.
Older heartbeat capture cannot replace a newer heartbeat; EMCY has its own
ordering, so an EMCY older than the latest heartbeat remains independent evidence.
Unknown capture cannot replace a known newer observation. After invalidation,
heartbeat/EMCY without qualified receive bounds are diagnostic only; known
capture or the entire receive interval must be strictly after the processing boundary. An explicitly old route epoch always
fails. This conservatively rejects already queued pre-boundary samples.
The retained proof facade supplies processing time only and forwards unknown
capture; it cannot refresh passive evidence after a reset by stamping delivery
time as capture time.

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

The application `CanBusOwner` now connects this service through tagged
`TransportTxId` identities on its ordinary transport. NMT uses one retained
request/result and one external handoff slot, distinct from SDO request/frame
tokens. Exact report identity and controller epoch are required; stale reports,
cancelled work and controller recovery cannot cause replay. See the
[owner contract](can_owner.md#copied-tx-and-receive-time-responsibility).

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
current qualification requires known capture or an entire receive interval
**strictly after** that handoff, the current lifecycle epoch and the caller's age policy. Unknown time,
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

| Qualified current evidence | Ordinary SDO | Probe upload (reviewed or accepted) | PDO transmit | Configured TPDO receive validation |
| --- | --- | --- | --- | --- |
| Pre-operational | Allowed | Allowed | Denied | Denied |
| Operational | Allowed | Allowed | Allowed | Allowed |
| Stopped / Initializing | Denied | Denied | Denied | Denied |
| Exact state unknown, current SDO service evidence | Allowed | Allowed | Denied | Allowed to enter validation |
| Exact state and serviceability unknown | Denied | Allowed explicitly | Denied | Allowed to enter validation |
| Exhausted lifecycle provenance | Denied | Denied | Denied | Denied |

The probe column also bounds the state eligibility of a specifically granted
unknown-effect upload. The grant does not reclassify that upload as reviewed or
accepted, and its result cannot promote ordinary service eligibility. The column
is `ServiceEligibility::probeUpload`: an upload whose read-effect policy is
`REVIEWED_NON_CHANGING` or `ACCEPTED_UNQUALIFIED`.

`setServiceAge(slot, maximumAgeUs, nowUs)` selects a nonzero inclusive age cap
for exact NMT and SDO service evidence; it does not refresh either fact. The
documented default `UINT64_MAX` selects lifecycle validity without age expiry,
not a guessed device timeout. Applications needing bounded observation age
must choose their finite cap. Capture age, or conservative oldest possible receive age, determines
freshness. Explicit external service confirmation is recorded separately in
`serviceQualification` with `serviceFromSdo=false`; the last matching SDO time
is not fabricated. Checked matching responses, including a valid rejection,
update service evidence with SDO provenance while preserving channel quarantine.
An experimental upload updates the separate raw SDO observation only and leaves
`serviceQualification` unchanged.
The same rule applies to experimental downloads; their success cannot qualify
later ordinary services.
An older or unknown-capture response may update separate raw SDO history but
cannot replace a newer known service proof. Receive intervals are ordered by their
lower bound, so an interval's upper bound never makes it newer. Since 2026-11-03 a
qualifying interval counts as known service proof; before, only exact capture or
an external confirmation did, and S2 ordinary work waited for a heartbeat.
Configuring an externally confirmed
SDO route uses the owner's last supplied processing time as a conservative
confirmation bound; attaching passive routing later cannot renew that bound.

Effective heartbeat disabled/unknown is independent of this table. With no
current exact state or service fact, explicitly request a bounded, reviewed
probe upload (reviewed or accepted, for example `1000:00` through `prepareProbe`)
on a qualified fresh channel. Once it succeeds, a reviewed
producer-setting SDO write can be admitted without requiring heartbeat first.
It still needs actual object/drive prerequisites and explicit caller intent;
`prepareProbe` has no NMT or settings effect, and no Lichuan read is declared
non-changing here. The separate
one-use experiment can collect raw observations while retaining unknown effects;
it cannot establish this write-enabling service qualification. A current Stopped
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

## Observed PDO maps and passive samples

`canopen/Pdo.h` provides bounded `validatePdoMap`, `validatePdoRoute`,
`decodePdo` and `pdoMapFromReadback`. They perform no I/O or configuration.
PDO direction is relative to the drive and independent of SDO access. Mapping
words contain index/subindex/bit length; the selected subset accepts complete
1–4-byte fields, at most eight entries/eight payload bytes, exact standard
Classical data envelopes and a source-backed profile set/slot limit. Partial
bits, zero active widths, duplicate/ineligible/reserved/unknown objects and
overflow reject atomically. Disabled count-zero templates remain representable;
they cannot establish an active receiving route. Common Frame still accepts
valid extended/RTR shapes and rejects FD explicitly; this PDO subset accepts
only standard Classical data.

### Source templates versus actual-map evidence

Generated `profiles/lichuan_c/Pdo.h` exposes `LICHUAN_C::pdoProfile(profileId)`
and `pdoTemplate(direction, number, output)`. These come from the single reviewed
ledger, without linking catalogue strings. Lichuan source facts describe four
sets per direction with four mapping slots each. PDO3/4 templates have count
zero; their unused slots are not active values. Pure profile validation also
accepts an explicit eight-set contrast without installing N5 support. The
owner's eight route slots are an application resource, not a global protocol
or manufacturer limit.

The default source-derived fixtures are RPDO1 `60400010,60FF0020,60600008`
(seven bytes), RPDO2 `607A0020,60810020` (eight), TPDO1
`60410010,60FD0020,60610008` (seven), and TPDO2 `60640020,606C0020` (eight).
RPDO1's WO `6040/6060` targets need no SDO upload to read mapping records.
Three unresolved signedness claims remain raw-only: `6060/6061` bytes and
`60FF` word. `numericKnown=false` preserves their bits without a numeric guess.
Resolved signed values use checked widening and sign extension. `PdoRole`
separates dictionary actual, commanded, estimated and uninterpreted fields;
it does not establish physical feedback scale, drive acceptance or completion.
No statusword or input-bit interpretation is inferred from raw PDO values.

`PdoMapEvidence` contains the complete binding/profile/configuration identity,
actual identifier and frame expectation, map epoch/layout, full communication
COB-ID parameter/transmission type, acquisition times and provenance record.
`TEMPLATE` provenance cannot install a confirmed map. Callers may supply an
explicitly established reviewed actual configuration, or collect matched SDO
completions through the existing bounded owner and call `pdoMapFromReadback`.
That helper checks communication `:01/:02`, mapping `:00` and each active slot,
in order, with `SdoUploadSize::EXACT`, exact sizes, matching channel/full binding, successful upload,
no abort and the specified node epoch. It never uploads mapped payload targets.
All completions retain admission time/node epoch from the actual service owner;
earliest admission and latest completion remain separate. Stale constituent
reads cannot hide behind one newer timestamp. Concurrent external configuration
still requires a coherent acquisition/exclusive configuration premise; matching
values alone cannot prove it. No read is newly declared non-changing here.

### One owner, independent sample provenance

`configurePdo(routeSlot, nodeSlot, evidence, profile, observer, nowUs)` installs
validated actual map contents in the existing owner. Every route attaches to
its existing node anchor; there is no second node table or CAN receiver.
Identifiers cannot cross SDO, heartbeat, EMCY, NMT or another PDO route, in
either configuration order. A bound wire identifier is never recycled during
that owner's lifetime. A replacement map needs a strictly newer epoch and
post-boundary acquisition; at most one map for a physical node/direction/set
can be confirmed. To move a set to a distinct verified wire route, explicitly
withdraw the previous map first. Eight fixed slots include those reserved old
routes; exhaustion rejects without stealing another route.

Confirmed map contents do **not** qualify any sample. `qualifyPdo` requires
exact current route/layout/binding/map/node scope, a nonzero external evidence
record and either an evidenced producer plus controller/application-ingress
retirement boundary, or a genuinely distinct verified non-reused route. A free
host slot, quiet time, newer timestamp, heartbeat, SDO success or host generation
increment proves neither premise. No generic counter/timestamp marker is offered
without reviewed producer semantics. After reuse or lifecycle/NMT withdrawal,
this fixed route requires producer-and-ingress retirement; a distinct-route
claim cannot reopen it. Qualification while already qualified rejects: explicitly
`invalidatePdo(slot, false, nowUs)` before replacing proof, so dependent consumers
are revoked too. Proof never retroactively promotes old diagnostics or samples.

One matching frame enters `receive` once. It must have known source capture
strictly after the qualification and invalidation boundaries, pass the existing
service-eligibility policy and decode completely before the latest observation
or any callback changes. Unknown exact NMT allows the first configured qualified
TPDO; arrival sets no NMT state. Known Stopped/Pre-operational or unavailable
controller prevents publication. No PDO updates heartbeat, SDO timestamps,
producer configuration, EMCY history, faults or operation results. PDO admission
and reception create no NMT or configuration traffic.

Malformed/unproven/stale frames remain copied diagnostics. The existing four-
record FIFO gains a PDO flag and map epoch; draining it cannot steal reception.
Overflow drops newest diagnostic copies while normal publication/invalidation
continues. Each of eight route records retains one complete sample, current-map
and qualification facts and historical provenance independently. Its bounded
synchronous observer receives call-only complete copies and invalidation notices;
retained consumers must copy them and may not reenter or perform I/O.

Boot-up/reset, binding/dialect/configuration withdrawal and controller failure
revoke affected maps and sample qualification before consumer callbacks, even
with full history. Nonreset NMT transitions and known non-Operational heartbeat
withdraw sample qualification while preserving unchanged map contents. Local
`invalidatePdo` affects only the selected route; unrelated stable traffic stays
qualified. Old observations remain historical and cannot renew in a new map
epoch merely because identical bytes arrive with a later capture time. The
same-ID/eight-byte position/velocity swap regression exercises precisely that
case without an old-generation test label.

`pdoFreshness(record, nowUs, maximumAgeUs, output)` uses only the current
qualified PDO capture and an explicit nonzero inclusive age policy. It
distinguishes absent map, unresolved provenance, never observed, fresh and
expired. Evidence installation refreshes no sample time. Snapshot copies
describe validity at their copy time; inspect the current owner record for
later invalidation. Generations never wrap; maximum map epoch permits no
replacement. No active SYNC scheduling or codec is needed by the reviewed
asynchronous fixtures, and no trajectory guarantee is added here. The explicit
TPDO1 mutation sequences and their partial effects are in the next section.

### Lichuan TPDO1 configuration subset

`profiles/lichuan_c/PdoConfiguration.h` implements two TPDO1 transitions and no
other PDO edit, through one lifecycle: `prepareTpdo1Configuration`,
`nextPdoConfiguration`, `notePdoConfigurationTransmission`,
`advancePdoConfiguration` and `getTpdo1Configuration`. A
`Tpdo1ConfigurationRequest` names the transition; its default names none and
refuses. `configurePdo` installs host evidence; it does not configure a drive.
The [offline caller](../examples/pdo_configuration_preview/README.md) runs the
event-timer change with retained bench replies and shows two refusals.

| Transition | Writes, in order | Baseline it requires | Evidence and scope |
| --- | --- | --- | --- |
| `MAP3_TO_MAP2` | `1800:01` valid bit set, `1A00:00=0`, `1A00:01=60410010`, `1A00:02=60640020`, `1A00:00=2`, `1800:01` restored | Enabled standard COB-ID, type 255, inhibit raw 100, event raw 0, count 3, `60410010 60610008 60640020 0` | CiA's variable-mapping order and one node-1 run (2026-11-02); `m` keeps it an isolated experiment |
| `EVENT_TIMER` | `1800:05` only, two bytes: raw 100 to raw 0 or raw 0 to raw 100 | CL86-C only; COB-ID exactly `0x180+node`, the EDS `$NODEID+0x180` rule (valid, standard, bit 30 clear), type 255, inhibit raw 100, count 3, `60410010 60610008 60640020 0`, event the other literal | Exact read, one acknowledged write and exact readback in Pre-operational on both CL86-C specimens ([packet 6](reports/bench_closure_18_33/packet_6.md#transition-record-for-packet-7)); the supported production subset, host-tested and awaiting its first bench replay through the S2 `E` line |

Both run in qualified Pre-operational under a continuous owner reservation and
take nine exact baseline uploads (`1800:01/02/03/05`, `1A00:00..04`) after the
reservation and its Pre-operational observation, then the writes, then the same
nine uploads. Steps are numbered writes first (1..6 or 1), reads after. Every
check, request, scope records, reservation, NMT evidence, deadline, budgets,
provenance, widths, values and the profile's ability to represent the published
map, precedes the first write; output is unchanged on refusal.
`Tpdo1EventTimerRaw` has only `RAW_0` and `RAW_100`. They are raw U16 values:
on the specimens raw 100 gave periods of about 101 ms on the S2's clock while
Operational and raw 0 sent no periodic frame while stationary. These two
directions are the only transitions measured on both specimens; 200 to 0 and
10 to 0 were measured on node 2 only and are not supported. `EVENT_TIMER` on an
OL86-C reservation refuses with `UNSUPPORTED`: the 1800/1A00 ledger entries bind
no model and every measurement is CL86-C. No unit, zero meaning, other value,
Operational write or persistence is claimed.

The context freezes the request, payload, deadline, operation and binding.
`notePdoConfigurationTransmission` advances the binding's configuration epoch at
the first possible write and records `firstEffectStep`; the caller withdraws
dependent maps then. `acknowledgedMask` and `confirmedStep` keep the confirmed
prefix; a terminal failure records `failedStep`, its retained terminal (with an
unsigned abort code) and `uncertain` when a write may have taken effect. There
is no rollback, retry or replay; a restoration is new explicit intent after a
known outcome. A route left unresolved by an unconfirmed write is not reused:
the owner refuses to reserve it.

`getTpdo1Configuration` publishes `SDO_READBACK` map evidence only after the
ninth matched read, and its overload adds the confirmed `1800:05` literal. Map
confirmation qualifies no sample: delayed same-ID, same-length frames decode
under the new map but stay unqualified, and a reused route cannot claim a
distinct-route proof. `SdoClient::advanceConfiguration` revokes the node's routes
again, which ends their permission to take replacement evidence; a caller that
will install the readback withdraws the route once more after the advance and
reads back strictly after that boundary, as the S2 harness does. The harness
matches the node's own route at its installed or an earlier configuration epoch,
so two changes without an install between still leave the second readback
installable; another node's route is never touched.

Missing broader evidence stays in Q17: other maps, timers and types, inhibit,
the COB-ID valid bit as a separate change, SYNC, RPDO, Operational edits,
persistence and post-change sample qualification. Device-sequence acceptance is
specimen evidence, not a firmware qualification.

## Local settings epoch transition

`SdoClient::advanceConfiguration` accepts only the next configuration generation
on an otherwise identical, idle, confirmed and resolved node/channel binding.
It requires released terminals, retired staging/cleanup, no pending NMT and
existing usable service evidence. It cannot reconcile an ambiguous channel or
rebind identifiers. It preserves node generation, liveness/NMT/service times and
historical diagnostics without renewing them. Cache observers run and PDO samples
are revoked before publishing the new binding. See the
[owner contract](can_owner.md#native-settings-exclusion-and-configuration-epochs).
Explicit new-epoch map evidence requires withdrawal/reinstallation; delayed
same-ID samples still need an independent qualification boundary. This is a
local bookkeeping seam for confirmed settings, not PDO reconfiguration or a
controller recovery operation.
