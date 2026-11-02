# Application CAN owner contract

The [shared native application](native_application.md) now layers explicit typed
reads and bounded platform I/O over this same owner. It leaves terminal service
results available for inspection until explicit advancement/release and provides
no additional CAN receive path.

The working application owner is
[`MotorControlCANOwner::CanBusOwner`](../adapters/owner/CanBusOwner.h),
implemented in [CanBusOwner.cpp](../adapters/owner/CanBusOwner.cpp).
It schedules copied CANopen SDO work from existing typed reads/settings, routes
all ingress once and retains immutable results. It provides reserved urgent
admission, explicit cancellation/invalidation and bounded host/controller
recovery. It is application code, excluded from the installed/exported core.
Cached communication health, statistics and bounded diagnostic copies are
implemented in [CanBusDiagnostics.cpp](../adapters/owner/CanBusDiagnostics.cpp).
Actual drive stop operations remain prompt 31 work; no motion context is imported.

The [offline consumer](../examples/owner_preview/README.md) needs no controller
or SDK. The [independent tests](../test/can_bus_owner_test.cpp) exercise two nodes,
multiple producers, passive events, public typed reads and an actual confirmed
loop-setting sequence. The [recovery suite](../test/can_bus_recovery_test.cpp)
adds external pending queues, missing adapter hooks, cancellation timelines and
two abstract vendor-profile bindings through that same backend. Host fixtures
do not qualify a drive. Prompt 23's owned
software is delivered; its status remains Partial while Q20 read-effect evidence
remains unresolved as a device fact (the DEC-1 accepted list is a project risk
acceptance only).

## One storage table

An application resource container owns one stationary `SdoClient`, controller
and `CanBusOwner`. The owner borrows that same idle adapter and copied `Transport`
callback bundle for its lifetime. All admission, receive, TX and terminal
progression passes through this owner. Do not independently drain controller RX
or use the adapter's admission/TX methods as a parallel path. Configure native
routes, map evidence and observers before scheduling, or in the same cooperative
context while idle. There is no global registry, allocation or task.

The adapter's existing storage remains authoritative for every channel, node,
NMT/liveness fact, EMCY record and PDO map/sample. Owner reservations copy the
identity they exclude; they are not another current node table. `collect` copies
the entire native terminal into the already reserved owner slot and immediately
releases native result storage. An older unread owner completion therefore
cannot monopolize a service channel.

| Storage | Capacity | Authority/lifetime |
| --- | --- | --- |
| Service channels / physical-node anchors | Existing adapter 2 slots | One active native request per actual channel; explicit routes and shared node anchors |
| PDO wire routes / raw network history | Existing adapter 8 / 4 | Actual map/sample qualification and FIFO/drop policy remain [service contracts](canopen_services.md#one-owner-independent-sample-provenance) |
| Logical-axis reservations | 2 | Span operations and service gaps; exclude the same logical axis or physical node across all its SDO channels |
| Ordinary request/results | 6 total | Queued, active and unread terminals share ordinary slots; admission reserves result storage before TX |
| Urgent request/result | 1 separate slot per channel | Ordinary work cannot use it, and one axis's unread result cannot block another's. Urgent work uses an existing reservation; a best-effort stop or drain marker needs none |
| Recovery result | 1 separate slot | Pending or unread recovery blocks another recovery independently of request/result pressure |
| External TX tracking | 1 frame per service channel plus 1 NMT frame | Tagged exact handoff/epoch retained until final controller report, proven retirement or successful whole-controller recovery; includes cleanup aborts |
| NMT request/result | 1 separate retained slot | Explicit manager intent; TX outcome and observed network transition remain independent |
| Producer identities | 4, numbered 1..4 | Cyclic eligible-producer selection, independent of CAN node/subaxis or motor kind |
| Active associations | 2 slot indexes/native handles | Refer to existing channels and canonical result slots; no second client or node state |
| Binding health | At most 2 channel-associated records | Cached logical-service totals, failure streak, timestamps and communication projection; protocol state stays in the adapter |
| Owner diagnostic history | 8 complete copied records | Oldest-first pull; overflow replaces the oldest copy and increments `traceOverwritten`; independent of authoritative RX and retained results |

Two channels for one physical peer are synthetic ownership contrasts, not a
claim that Lichuan provides a second server. The retained
[prompt-6 proof](../experiments/api_ownership/README.md) stays test-only; its
one-channel bridge, three slots and urgent prototype are not this owner.

## Admission and lifetime

`beginSequence(binding, producer, nowUs, id)` reserves actual configured identity.
It rejects aliases of the same logical axis or physical peer, stale node epochs
and any quarantined peer channel. It never configures a drive or discovers nodes.
The reservation survives waits, released requests and operation boundaries.

`beginOperation` selects a strictly increasing nonzero operation and fixed
exclusive deadline. It requires no nonterminal request in that reservation;
older retained terminals remain inspectable and unchanged. Continuous exclusion
spans prerequisite reads and later settings while each operation can restart
step numbering. `admit` validates exact route,
epochs, current service eligibility, operation, deadline, size/direction and
increasing step before copying all input. Rejection changes neither output ID
nor queued work/reservation/time.

`RequestId` and `SequenceId` contain the stationary owner, fixed slot and a
nonwrapping serial. `RecoveryId` contains the owner and its nonwrapping serial.
One counter issues all three; exhaustion rejects new issuance.
Constructor `lastIssued` permits an explicit application serial seed, not wire
freshness or reconstruction of old handles. Never use a handle after its owner
is destroyed/reconstructed. Operation IDs remain caller supplied.

`result(id)` is non-consuming and null while pending or for foreign/stale IDs.
The first terminal is immutable until `release(id)`. `releaseSequence` separately
refuses while any associated request/result remains occupied. No unread result,
including uncertainty, is evicted to admit another request.

`Completion.admittedUs` is owner admission. `Completion.service.admittedUs`
remains actual native admission. Full transfer bytes, native node epoch,
unsigned abort, reply and effect evidence survive copying. Queued local failures
produce consistent native failure evidence without claiming transmission.

### Finite unknown-effect upload experiments

An explicitly reviewed scalar write uses `authorizeDownloadExperiment` with one
exact 1–4-byte payload. Upload and download grants share the existing one-use
slot and increasing review serial; each copied intent includes the direction,
payload, route, operation/step, lifecycle epoch and immutable time budgets.
The grant is not a production profile capability or service confirmation.
`clearUploadExperiment` clears either unused grant; admitted work still needs
explicit owner cancellation. The current bench uses fixed heartbeat and TPDO1
procedures, with retained partial effects and no replay or automatic rollback.

The application may install one exact
[`authorizeUploadExperiment`](canopen_services.md#one-use-experimental-upload-grants)
grant on the borrowed adapter in this same serialized context. It then submits
the copied `BusRequest` through ordinary `admit`; the grant creates no competing
receiver, sender or service loop. Owner reservation, current binding/node epoch,
increasing operation/step, deadlines, result capacity and controller eligibility
still apply. An altered object, route, size policy, effect policy or budget
cannot use the grant. Native admission consumes it once. An unused grant can be
withdrawn independently; ending the experiment also requires explicit owner
cancellation of any pending request and retention of its terminal.

Experimental permission remains `UNQUALIFIED`. A successful exchange records
the raw service observation and possible effects without creating channel
confirmation or service qualification. It cannot authorize another upload,
download, implicit recovery or automatic retry. Existing qualifications remain
independent. Unknown effects on successful uploads and execution uncertainty
after a submitted failed/interrupted upload survive the owner completion copy.

`SdoUploadSize::EXACT` remains the production default. Typed read and map-readback
consumers reject `SERVER_INDICATED` completions. That experimental policy gives
an upload a fixed capacity and retains the actual successful byte count; it
does not substitute an observed count for a reviewed object width or native
meaning. Neither an EDS width disagreement nor a raw successful read silently
changes profile metadata or read-effect policy.

The bench-only [`RawEvidence`](../examples/s2_bench/RawEvidence.h) collector
reserves one operation and producer through this owner, grants/adopts at most
one next fixed-inventory upload per collector turn, and copies ordinary owner
diagnostics and immutable completions. Service the owner first and collector
second; do not drain diagnostics, clear statistics or run `DeviceEvidence`
concurrently. A preceding accepted TX awaiting its final controller report
delays the next grant within the original deadline. Failure or evidence loss
stops further field admission without replay. A bounded post-terminal cleanup
tail remains separate from frozen terminal facts, and release waits for local
handoff/cleanup retirement. This application collector supplies no typed
interpretation, drive mutation or qualification shortcut.

`DeviceEvidence::requestExperiment` retains external review references for its
typed finite plan. Those references alone do not install an adapter grant or
establish the configured fresh-session premise. Both collectors remain subject
to the actual service admission rules and separate fixture authorization.

## Scheduling and time

The [counter-zero handoff](fault_reference_actions.md#coordinate-withdrawal-and-owner-synchronization)
withdraws application coordinates at possible trigger TX. Its ordered writes
retain the old wire epoch; after their results and external handoffs retire,
`advanceConfiguration` synchronizes the already advanced application epoch for
readback. The configuration invalidation callback must not increment the axis
again. Failed/uncertain prefixes remain retained, with no automatic final-zero
cleanup or channel reopening.

The [Lichuan stop handoff](stop_actions.md#owner-handoff-and-retained-interruption)
uses these same methods and reserved slots. `beginUrgentOperation` retires obsolete
unsubmitted work and leaves in-flight work to settle; `admitUrgent` alone preserves
FIFO and does not withdraw later queued work. Preserve the original terminals and
consume the application's prior controlword eligibility when the stop may transmit.
A new operation ID cannot make old `000F` evidence current again.

All time is supplied nonwrapping, nondecreasing monotonic microseconds; zero is
valid. No clock is acquired. Overall deadlines are immutable. Owner queue delay
counts against the first-frame queue budget: dispatch passes its original cutoff
to the adapter without modifying the copied transfer. Per-frame response/segment
budgets stay clamped to the same overall deadline. No polling or unrelated
traffic renews it.

| `ServiceBudget` | Per-turn maximum | Zero behavior |
| --- | --- | --- |
| `rx` | 8 receive callback calls, stop on empty | No controller receive calls |
| `events` | 8 actual TX-report callback calls, stop on empty; nonzero also permits at most one retirement attempt per channel | No report or retirement callbacks from `service` |
| `tx` | 2 controller enqueue attempts, none replayed | No enqueue and no new dispatch |
| `starts` | 2 new native admissions, also needs TX budget | No new native admission |
| `recovery` | 1 recovery poll, regardless of a larger nonzero value | No recovery poll; recovery deadline still expires |

Larger requests clamp to these bounds. With every budget zero, bounded native
progression, deadline expiry, invalidation and result collection still execute.
Passive events continue during active or unresolved SDO work whenever the
application supplies RX budget. Diagnostics never acquire another receiver.

Each turn settles deadlines, progresses an explicitly requested recovery within
its budget, takes controller reports, routes RX, retires invalid external
handoffs, then services staged frames and eligible new requests. Producer
selection cycles 1..4, skipping
busy/ineligible channels; earlier nonterminal steps in the same sequence retain
FIFO order. Reservations do not block other nodes or passive delivery. Sequence
order is not a success dependency: dependent work is admitted only after its
caller has checked/advanced the prior result.

All scans and callbacks have fixed bounds: seven request slots, two reservations
and four producers per selection. Selected-stack progression retains its
existing bounded per-channel calls and 32-byte FIFO drain. These are source work
bounds, not measured board service gaps.

## Copied TX and receive-time responsibility

`Transport` callbacks borrow a stationary application/controller context and run
synchronously without reentrancy. `receive` and `takeTxReport` publish complete
copies. A valid ingress frame enters normal PDO/network/SDO routing once.
Only afterward may `observe` inspect a call-only copy; it cannot consume a frame
or release/admit owner work. Unknown identifiers stay diagnostic traffic.
Rejected malformed/future-capture frames also reach this raw diagnostic callback;
its copies are not qualified evidence and cannot publish typed cache observations.

Capture time is present only when externally qualified. Unknown capture remains
unknown; processing time is never substituted. `ReceivedFrame.controllerEpoch`
must be stamped when the adapter captures ingress, before any local queue. It
must never be replaced with the current epoch on dequeue. Old-epoch traffic
reaches only the optional raw observer. Source-time, binding/map/epoch
validation and same-ID/same-DLC PDO ambiguity rules remain in the
[service contract](canopen_services.md#passive-network-events-and-node-lifecycle).

`ReceivedFrame` may instead carry `receiveBoundsKnown` with inclusive
`receivedEarliestUs`/`receivedLatestUs`. Exact and interval flags are mutually
exclusive; reversed/future intervals are rejected before normal dispatch and
remain raw diagnostics. Network freshness uses the oldest possible ingress,
while definite expiry uses the newest. Bounds do not establish SDO correlation
or PDO route retirement. The pinned TWAI shim defers future-upper-bound entries
until the adapter's accepted service cutoff reaches them; it never clamps or
relabels their timestamps. See the [source proof](adapters/twai_receive_time.md).

For a staged first download, optional `beforeTransmit` validates application
prerequisites and notifies the actual settings or stop-action context before possible TX.
It runs once per download request, never for cleanup aborts. It performs no
operation stepping, I/O or reentrant owner call. `nextTx`, exact-token
`canTransmit` and one-shot `transmit` then run in the same cooperative turn
without receive interleaving. Failed enqueue is not retried.

The transmit callback now has signature
`bool transmit(void*, const Frame&, TransportTxId, uint32_t controllerEpoch)`.
`TransportTxId::Kind` distinguishes an exact SDO request/frame ID from an NMT
request ID. `sameTransportTxId` compares the active identity only; reports for
another kind, generation or controller epoch cannot settle pending work.
The final argument identifies the local enqueue epoch. True means enqueue
acceptance only; false conservatively permits uncertain acceptance, closes native
work as a transport failure and is never an instruction to retry the same bytes.

Controller acceptance is separate from actual transmission. `takeTxReport`
supplies the exact native request/frame token and its original controller epoch.
Both positive and negative reports must prove this exact frame cannot transmit
later. A positive report additionally proves actual TX/ACK and calls `confirmTx`;
a stale report cannot affect a newer segment. One external frame per channel,
including a cleanup abort, must settle before another handoff on that channel.
A protocol reply may settle service before its controller report; later reports
never rewrite the retained first terminal. Queue acceptance, CAN ACK and SDO
confirmation are not drive acceptance or motion completion.

When an external handoff becomes ineligible, `retire(context, id, epoch)` may make
one bounded attempt to prove the frame cannot leave any local SDK/controller
queue later. A successful retirement preserves possible earlier effects. A
missing or unsuccessful retirement leaves the affected channel blocked; other
channels and ordinary receive dispatch remain available. There is no hidden
retirement retry. An exact final report or explicit successful whole-controller
recovery supplies the other settlement paths. These are application adapter
obligations, not a guarantee about an unimplemented SDK or controller port.

The same owner exposes `admitNmt`, `cancelNmt`, `nmtResult` and `releaseNmt`.
Service hands off at most one explicit NMT frame within the ordinary TX budget,
using the same transport, RX dispatcher, retirement and controller-report path.
A pending or unretired NMT handoff blocks dependent SDO work for its addressed
node, or all local nodes for a broadcast; unrelated addressed-node traffic
continues. Releasing a terminal does not retire a potentially transmitting frame.
Late reports can retire that frame but cannot rewrite its terminal outcome.
NMT results are manager-service facts, never a motor-stop or enable result.

## Cancellation, observations and channel uncertainty

`admitUrgent` validates and copies the same native work as `admit`, using the
urgent slot of its channel: `TOTAL_REQUESTS = REQUESTS + URGENT_REQUESTS`, and
`URGENT_REQUESTS` is `SdoClient::CHANNELS`. The slot belongs to the channel, not to
a node number or to a reservation index. One channel cannot take another's slot,
and an unread urgent result on one channel leaves the others free. The next urgent
admission on the same channel returns `RESULTS_FULL` until that result is released.
It reuses one of the existing two reservations; there is no extra urgent axis
reservation. Ordinary request/result pressure cannot consume these slots, but route
validity, result lifetime and the same-sequence FIFO still apply. Eligible urgent
work gets the next safe dispatch opportunity. It neither preempts an active request
nor cancels earlier intent. An occupied or quarantined channel does not become free
through urgency.

`beginUrgentOperation(id, operation, deadlineUs, nowUs)` starts a new operation on a
live reservation. Queued work that has not reached the transport ends with an
explicit no-TX result under its own identity, while work in flight is left to settle:
cancelling it could quarantine the route, and its reply is still evidence. New work
waits behind it on the FIFO. The operation number must increase, and the deadline is
immutable and independent of the old work. Old results stay readable by `RequestId`.
A quarantined route refuses the call. `findSequence(binding, output)` gives a
producer that does not hold the reservation its identity, and fails on an unknown
binding, an invalidated reservation or a quarantined route. A later `beginSequence`
takes a free slot first, then reuses the slot of an invalidated reservation with
nothing live. Its old `SequenceId` stays invalid and retained results stay readable.
`releaseSequence` on that old identity returns true and changes nothing, because it
no longer holds anything, so its holder can finish. Retired urgent work keeps its
channel's urgent slot until released, and a server abort of old in-flight work ends
queued steps of its own operation only.

### Best-effort stop route (DEC-2)

`admitBestEffortStop(transfer, nowUs, output)` is the one transfer allowed outside
any reservation and on an unresolved route. It takes only the `6040:00` U16 download
of `000B`, and packet 7 owns the action that calls it. The route exists so that a
stop can still be tried after a lost reply, and it claims nothing about the stop:

| Case | Result |
| --- | --- |
| Other object, word, width, direction, `requirePreOperational`, zero identity or timeout, or an expired deadline | `INVALID`, no slot used, whatever the channel is doing |
| Bus-off, recovery, known `STOPPED` or `INITIALIZING`, exhausted node, or a blocked external TX | `INHIBITED` |
| That channel's urgent slot holds other work or an unread result | `RESULTS_FULL`. The stop never displaces anything. Read and release that result, or cancel and release queued work, then admit again |
| Work in an ordinary slot live on the channel or a peer channel of the node | Admitted, waits for it to settle; every rule rechecks at dispatch. No new peer work starts meanwhile |
| Its queue budget ends while waiting | `QUEUE_EXPIRED`, no TX. Size it for the settling time |
| Controller reports the TX | `SdoOutcome::UNCONFIRMED_SENT`, execution unknown |
| No report by the deadline | `TIMEOUT`, effect still possible |
| Never submitted by the deadline | Terminal, no effect possible |
| Boot-up before submission | Invalidated, no TX |

It is submitted at most once. It skips the `beforeTransmit` guard, because that guard
consumes a sequence's controlword eligibility and this request has no sequence. A
frame arriving during it, including an identical `60 40 60 00`, is retained. It cannot
retire the handoff, qualify the route or refresh service evidence: classic SDO has no
transaction tag, so that frame may answer the write before it. The route ends
unresolved, and ordinary work stays blocked until a drain marker settles it.
`UNCONFIRMED_SENT` is not a stop result. It says one frame left the controller.

`cancel(id, nowUs)` closes only that request. Unlike RS owner sequence
cancellation, it does not revoke the reservation or cancel its other queued
requests. It performs no receive or unsent-work progression. `Cancel::CANCELLED`
means cancellation won locally; external retirement and possible drive effects
remain independent. `ALREADY_TERMINAL` preserves earlier completion/expiry;
`INVALID` rejects a foreign/stale handle, backward time or reentrant call.
Use `invalidate(sequence, nowUs)` to revoke a reservation and cancel its pending
group, or `invalidateProducer(producer, nowUs)` for all that producer's
reservations/work. Both retain earlier terminals and perform no drive reset.
Cancellation, invalidation and controller-state reports may also attempt bounded
external retirement, at most once per channel. Local cancellation is not a motor
stop.

Processing-time expiry wins at the inclusive cutoff before receive or cancel.
An earlier qualified capture timestamp delivered after closure may update the
separate raw observation, but cannot rewrite a timeout, cancellation or another
terminal. A service already confirmed before cancellation stays confirmed.
This preserves the native adapter's existing precedence; no retroactive success
is inferred from buffered evidence.

Timeout after possible transmission preserves unknown effects and channel
quarantine. Owner release, quiet time, new host IDs or a new sequence cannot
reopen it; only an explicit drain marker on established premises can, and no
real fixture has those premises yet (Q18). A matched server rejection on a clean
exchange leaves the route open and ends queued steps of its sequence.
Same-peer quarantine revokes reservation usability; queued
work fails and active work is invalidated before later RX/TX can restore success
or emit a segment. Already retained terminals stay unchanged; unrelated nodes
continue normally. Boot-up uses existing synchronous node/cache invalidation,
preserving interrupted outcomes and previous uncertain effects.

## Explicit host/controller recovery

`reportController(state, nowUs)` reports actual controller state through the
serialized owner. A non-`AVAILABLE` report invalidates reservations and dependent
evidence, closes pending work and inhibits TX. `receiveOverflow(nowUs)` reports
lost authoritative receive evidence through the same `RECOVERING` path. A later
direct `AVAILABLE` report cannot bypass the owner's interlock.

`recover(nowUs, deadlineUs, recoveryId)` reserves its own result, invalidates all
reservations and node freshness, closes interrupted work without deleting its
results, and advances the nonwrapping controller epoch. It invokes at most one
`beginRecovery(context, newEpoch)` callback. Rejection leaves output/work
unchanged. Missing callbacks or a failed begin produce a retained `FAILED`
result; accepted recovery is not success.

With nonzero `ServiceBudget.recovery`, `pollRecovery(context, epoch)` runs at
most once per turn. `PENDING` continues within the original deadline. `RETIRED`
must prove all older local TX, RX, reports and asynchronous callbacks retired,
including other local producers, and establish controller readiness. It clears
external handoff blocks and the host TX interlock only. `FAILED` or the exclusive
deadline produces an immutable failure/`TIMEOUT` result and leaves the interlock
set. Zero recovery budget still enforces the deadline. Before another recovery,
the adapter must stop any older attempt; late completion cannot rewrite a result.

`recoveryResult(id)` is non-consuming until `releaseRecovery(id)`. An unread
result blocks another recovery, even when all ordinary/urgent storage is full.
Releasing a failed result does not clear the interlock. Exhausted owner serials
or controller epochs reject fresh recovery rather than wrapping.

Recovery sends no CAN command, replays no write and performs no enabling, fault
clearing or physical stop. Host/controller retirement cannot establish a fresh
remote SDO session or reopen unresolved channels. It cannot qualify PDO samples
on a reused route: local epochs do not identify delayed same-ID/same-length
remote frames or retire another producer's pending remote TX. Reconciliation
still needs the independent evidence below. No real SDK queue retirement,
firmware behavior or hardware service-gap guarantee is claimed by host fixtures.

The [reconciliation evidence contract](canopen_services.md#unsupported-reopening-and-required-evidence)
still requires actual retirement/freshness evidence; local tokens cannot resolve
identical delayed SDO replies. Q20 leaves Lichuan read effects unestablished.
The DEC-1 accepted list is owner risk acceptance, not evidence; reads are not
automatically harmless.

## Native settings exclusion and configuration epochs

The [confirmed settings](driver_settings.md#explicit-position-parameter-settings)
and [loop selection](tuning.md#explicit-loop-mutation-and-retained-effects)
use ordinary copied work and retained completions. Acquire the reservation
before prerequisite reads. Construct `SettingsReservation` from that actual
owner identity/sequence generation/acquisition time and explicit model/target
facts; consult `sequence(id)` for current exclusion validity. Do not refresh a
revoked reservation from an obsolete copy.

The application's `beforeTransmit` calls `noteDriverTransmission` with the actual
settings context, live reservation and actual axis/configuration binding.
Each changed download is notified; the configuration epoch increases only for
the first possible effect of the transaction. Only that exclusive transaction
finishes its original-epoch downloads. The fixed settings token retains its
original target; it is not rewritten for readback.

After writes and service terminals are released, call the owner's checked
`advanceConfiguration(sequence, actualBinding, nowUs)`. It delegates the native
healthy-idle check, preserves exclusion/node history, and advances the reservation
to the actual new configuration. Pending or unretired external frames on the
same node reject this advance even after their owner results are released.
New-epoch reads then use the same reservation
and operation. Map/sample qualification, physical activation and motion completion
stay independent; no hidden save, reset, enable or retry occurs.

Typed contexts stay outside the owner. Native tests trace a public state read
and actual loop-setting ACK/readback through this path, keeping the original
settings token unchanged. They also retain an unconfirmed write and an
acknowledged write followed by failed readback, without replay or loss of the
confirmed prefix. Unrelated nodes continue while these results remain unread.
Future confirmed groups in 20–22 use this same boundary;
prompt 45 checks combined integration again.

## Passive communication health

`health(binding, output)` and `driverState(binding, output)` copy cached evidence
for an exact configured binding, including its current configuration generation.
Unknown or stale bindings return false without changing output. A known binding
can be `UNINIT`. The accessors, `stats()` and `resources()` perform no I/O,
callbacks or clock reads. `stats()` borrows a view until the next owner mutation;
copy it to retain a snapshot. `resources()` returns a fixed-scan value containing
queued, active and retained request counts, reservation count, trace occupancy,
TX inhibition and pending recovery.

Health ageing occurs when an owner method refreshes it using supplied monotonic
time, particularly `service(nowUs, budget)` and
`setHeartbeatMaximumAge(binding, maximumAgeUs, nowUs)`. Repeated cached getters
never advance time. Applications must keep scheduling service turns, including
zero-budget turns when only local deadline/health progression is needed.

The projection applies this precedence:

| State | Cached condition |
| --- | --- |
| `OFFLINE` | Global owner TX interlock/controller unavailability, reported bus-off/recovery, or an expired configured and evidenced heartbeat deadline |
| `DEGRADED` | A current failed attempted-service streak remains |
| `READY` | Checked current-generation endpoint evidence meets the selected evidence-age policy |
| `UNINIT` | No qualifying current endpoint evidence |

`READY` describes communication. Controller state, NMT state, heartbeat
freshness, statusword, mode, EMCY, drive fault, operation outcome and uncertain
effects remain separate facts. A diagnostic state does not independently refuse
an otherwise serviceable stop, read or reconciliation. Existing transport,
channel, binding, capacity and operation prerequisites continue to apply.

`setHeartbeatMaximumAge` changes host observation policy only; it never writes
`1017`. Its age cap is inclusive: age equal to the cap is fresh; age greater
than the cap expires. Zero disables this host monitor. Expiry requires a known
effective nonzero producer interval and a qualified observed heartbeat in the
current node generation. Unknown or disabled producer timing, unknown capture
time and a never-observed heartbeat are inconclusive; there is no invented
startup timer, heartbeat default or interval multiplier. The independent node
evidence-age policy determines whether current checked evidence supports READY.
An evidenced heartbeat expiry has priority even if other traffic is present.

`Health` holds saturating `uint32_t totalSuccess` and `totalFailures`, saturating
`uint8_t consecutiveFailures`, supplied `uint64_t lastOkUs`, `lastErrorUs` and
`lastSeenUs`, their known flags, retained `Status lastError`, and `state`.
Timestamp zero is valid when its known flag is true. Success/failure timestamps
are terminal processing times. `lastSeenUs` is the latest checked endpoint
delivery/processing time; it is not a substitute for qualified wire capture
time. The adapter retains capture-time/generation facts separately. A checked,
currently qualified PDO delivered after its capture-age limit still updates
`lastSeenUs`; it cannot establish READY. Unqualified, revoked-map, malformed and
unknown-capture PDO samples cannot update that delivery history.

One completed attempted SDO upload/download counts once, including every
segment. A multi-object read or motion operation adds no extra owner total.
Owner collection of the native terminal is the authoritative counting boundary.
An attempted service requires possible controller transmission. Successful
services increment `totalSuccess`, clear the current failure streak and set
`lastOkUs`; they preserve `lastError` and `lastErrorUs`. Failed attempted
services increment `totalFailures` and the streak, and retain their failed
status/time. `lastError.detail` contains `SdoOutcome`; the unsigned abort is never
narrowed into that field. Server abort maps to PROTOCOL_ERROR, malformed client
abort to FRAME_ERROR, and other failed services to INVALID_CONFIG with their
native cause in detail. A valid server abort both fails that service and proves endpoint
communication; its complete unsigned abort code stays in the retained terminal
and copied terminal history. Checked malformed responses and attempted-service
timeouts fail their service. Unrelated frames cannot create a service failure
or extend its deadline.

Local validation, unsupported capability, queue refusal, waiting, local
cancellation and pre-handoff withdrawal do not change logical-service totals
or the streak. Fire-and-forget TX is not endpoint confirmation. Explicit probes
using a reviewed confirmed service count normally. Passive heartbeat, EMCY and
qualified PDO observations update endpoint liveness without changing service
totals, the streak or the prior error. A successful independent reconciliation
service may clear the current streak; it cannot rewrite an earlier failed
operation or settle its uncertain effects. Cleanup traffic never replaces the
original failed service result.

Controller recovery withdraws old node freshness. New qualified evidence can
restore communication health while failed-operation evidence, drive faults and
uncertainty remain. Changing logical binding identity or binding generation
resets that binding's entire current Health and its host heartbeat-age setting.
A configuration-only generation advance preserves communication history;
accessors require the updated exact binding. Interface statistics survive
rebinding. Historical diagnostic copies and old operation results retain the
binding/generation recorded at their creation.

The unresolved Lichuan statusword dialect remains unresolved here. EN86 PDF p12
assigns alarm to bit 7; ZH86 PDF p12 assigns it to bit 3; OP86 PDF p10 combines
bit 7 alarm with a different running-bit description. A fresh `6041` sample does
not choose between those claims. Preserve raw/unknown interpretation until
applicable model/firmware evidence establishes the dialect. The EDS `1017`
default zero is factory metadata, not live heartbeat readback or proof of a
drive-side host-loss stop. See the retained
[motion conflicts](audit/LICHUAN_MOTION_FACTS.md#three-incompatible-status-descriptions) and
[communication facts](audit/LICHUAN_OBJECT_FACTS.md).

The cited OP86 page header actually names CL57-C/OL57-C despite its retained 86
filename. Its bits do not establish an additional 86C dialect or model claim.

## Statistics boundaries and reset lifetime

All named counters saturate at `UINT32_MAX`; `maxServiceGapUs` is a `uint64_t`
observed maximum. There are no duplicate binding-service totals in `Stats`.

| Field | Single authoritative increment/update |
| --- | --- |
| `txFrames` | Exact pending frame's matching current-epoch final controller report says `transmitted=true`; includes cleanup frames and reports arriving after service closure. Enqueue acceptance does not increment it. |
| `rxFrames` | One frame is returned by the owner receive callback, including ingress later rejected by owner or protocol validation. Empty receive calls do not count. |
| `txRejected` | One enqueue callback refuses a frame, or that exact frame's matching final report is negative. A refusal followed by a negative report counts once; a later positive report can independently prove actual TX. |
| `rxDropped` | Owner rejects invalid, old-controller-epoch or future-capture ingress; also adds the nonzero reported lost-frame minimum accepted by `receiveOverflow(nowUs, droppedFrames)`. These lost frames need not have reached the receive callback. |
| `rxLost` | Subset of `rxDropped` added only by accepted `receiveOverflow` reports. No raw frame exists for these reported losses; retained rejected/unmatched frames do not increment it. |
| `unmatchedFrames` | Structurally valid current-epoch ingress is not accepted by normal PDO/network/SDO dispatch. This includes unrelated frames and protocol-route rejects; use retained native dispositions to distinguish them. |
| `queueRejected` | Each non-reentrant rejected `admit` or `admitUrgent` call, including local invalid input, expired input, inhibition and exhausted IDs. Reentrant rejection leaves all storage unchanged. Sequence/operation/recovery refusal is not a request admission. |
| `resultsRejected` | Subset of `queueRejected` where admission returns `RESULTS_FULL`, including occupied urgent capacity. |
| `cancelled` | Each newly terminal request whose winning cause is local withdrawal: explicit cancel, sequence/producer invalidation, recovery interruption or preflight guard. Earlier eligible terminals win; repeat cancellation does not count again. |
| `timeouts` | Each request TIMEOUT terminal, including untransmitted owner queue/deadline expiry, and each recovery TIMEOUT terminal. This interface count does not imply an attempted-service health failure. |
| `busOffEvents` | Accepted `reportController(BUS_OFF, nowUs)` changes the reported controller state into BUS_OFF. Repeated BUS_OFF reports do not increment it. |
| `recoveries` | An explicit host/controller recovery first closes `RETIRED`; attempts, refusals, failed recovery and timeout do not count. |
| `traceOverwritten` | A new owner diagnostic replaces the oldest copy when all eight history slots are occupied. Reading copies does not increment it. |
| `maxServiceGapUs` | Maximum difference between consecutive accepted `service` timestamps; first accepted turn establishes the baseline. Other owner calls do not create service turns. |

`clearStats()` returns false during callback reentrancy. Otherwise it zeros all
interface totals/high-water marks and each binding's `totalSuccess` and
`totalFailures`. It preserves the live failure streak, current state, timestamps
and known flags, last error, heartbeat-age policy, configuration/binding/node
generations, controller interlocks, drive faults, uncertain effects, active and
queued work, retained completions and diagnostic history. It also preserves the
previous accepted service timestamp: the next measured service gap still spans
the actual interval. Resetting statistics does not perform recovery or clear
operational evidence.

Positive per-frame `Transport.retire` evidence now includes retiring every queued
or future final report for that exact ID/epoch, alongside preventing later TX.
If an adapter cannot establish report retirement, it returns false and waits for
the ordinary matching report or explicit whole-controller recovery. This avoids
unbounded retired-ID history and diagnostic-driven TX backpressure. `txFrames`
counts positive reports delivered to this owner before explicit retirement;
it is not an estimate of earlier unconfirmed physical transmissions. Retirement
alone never upgrades a result or increments actual TX. Unrelated frame reports
must survive exact-frame retirement.

## Bounded diagnostic copies and output scheduling

`takeDiagnostic(output)` copies and consumes one oldest owner-history record;
it never receives from the controller or consumes normal dispatch input. An
empty/reentrant/overlapping-output call returns false without changing output.
`DiagnosticKind` selects raw RX, enqueue-attempt TX, matching final TX report,
complete copied terminal, controller report or recovery closure. Records own
their frame/terminal bytes, processing time, RX exact capture or conservative interval,
exact local TX identity, controller epoch and applicable acceptance/controller/
recovery facts. A TX trace records enqueue acceptance; a TX_REPORT copy records
the distinct actual transmission report. Native retained histories
remain available independently through the adapter's documented bounded stores.

The owner stores eight complete records without heap allocation. Full history
overwrites the oldest copy and leaves ordinary/urgent work and receive dispatch
running. Taking a diagnostic does not release the canonical terminal. Clearing
statistics preserves buffered records; consuming records is the explicit owner
history retirement policy. Exported copies are application-owned. Larger traces
and console/output queues belong in application storage with their own bounds.

The application [DeviceEvidence collector](../examples/common/DeviceEvidence.h)
uses this existing drain while an explicit typed acquisition is active. It
retains terminals before advancing the app and keeps up to 64 wire/controller/
recovery copies. Trace overwrite, increased `rxLost`, observed decrease or
saturation of either monitored loss counter, or exhausted collector capacity
marks evidence loss and interrupts further acquisition. A retained rejected frame
alone is diagnostic evidence, not an unrecorded RX loss. Initial/final cumulative
statistics and resource snapshots preserve their owner meanings; a late TX report
can arrive after the acquisition terminal and need not appear in its frozen
snapshot. Do not reset statistics or independently drain diagnostics during this
collector's ownership. See [the application flow](native_application.md#finite-acquisition-collector)
for admission and release; the owner remains the only physical receive path.

One service turn still uses the RX/TX/event/start/recovery budgets above.
Diagnostic publication adds fixed copied records and fixed scans over two
channel-associated health records and up to eight PDO routes. Slow consumers
lose copied history instead of blocking dispatch. Drain only a chosen number of
copies per application output turn and schedule ordinary owner service and urgent
admission independently. The synchronous legacy `Transport.observe` callback
must remain bounded and perform no console/output I/O; queue a copy there if an
application uses it. No arbitrary callback can provide a nonblocking guarantee
if its implementation waits or prints.

The native preview demonstrates cached health/statistics/resource access,
bounded diagnostic pulls and a statistics reset without extra transport calls.
Host type sizes and measured service gaps are separate observations; fixed work
bounds do not claim board memory, SDK queue placement, PSRAM suitability or a
hardware scheduling guarantee.

## Pinned controller boundary

`localTxRetired(slot)` is a cached, read-only check of native staged frames and
the owner's external pending/blocked handoff. Invalid or unconfigured slots
return false. A controller-completed frame still blocks until its final report
is consumed or exact retirement is established. This check never calls the
transport, retires work itself, settles remote effects or qualifies a channel.
Collectors use it for progression/release; lossy diagnostic copies cannot own
that decision.

The optional [TWAI adapter](../adapters/twai/README.md) supplies concrete S2/S3/IDF
5.5.5 enqueue, transmission and retirement evidence. Service the owner before
adapter TX promotion with the same supplied time; unread RX defers promotion.
SDK callbacks copy into bounded internal-RAM storage and sample receive-time
upper bounds. Explicit recovery
destroys the old node before a fresh instance can accept work. See the
[SDK evidence](adapters/twai_sdk.md) for bus-off deletion proof, unavailable
hardware-overrun evidence and ISR latency limits. This does not reopen an
ambiguous SDO session or qualify a reused PDO route.
