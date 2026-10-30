# Application CAN owner contract

The working ordinary application owner is
[`MotorControlCANExample::CanBusOwner`](../examples/common/CanBusOwner.h),
implemented in [CanBusOwner.cpp](../examples/common/CanBusOwner.cpp).
It schedules copied CANopen SDO work from existing typed reads/settings, routes
all ingress once and retains immutable results. It is application code, excluded
from the installed/exported core. Prompt 24 owns urgent capacity, full owner
cancellation and controller recovery; no motion context is imported here.

The [offline consumer](../examples/owner_preview/README.md) needs no controller
or SDK. The [independent tests](../test/can_bus_owner_test.cpp) exercise two nodes,
multiple producers, passive events, public typed reads and an actual confirmed
loop-setting sequence. Host fixtures do not qualify a drive. Prompt 23's owned
software is delivered; its status remains Partial solely while prerequisite
19's non-consuming probe evidence Q20 remains unresolved.

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
| Ordinary request/results | 6 total | Queued, active and unread terminals share slots; admission reserves result storage before TX |
| Producer identities | 4, numbered 1..4 | Cyclic eligible-producer selection, independent of CAN node/subaxis or motor kind |
| Active associations | 2 slot indexes/native handles | Refer to existing channels and canonical result slots; no second client or node state |

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
exclusive deadline. It requires all earlier requests/results in that reservation
to be released. Continuous exclusion spans prerequisite reads and later settings
while each operation can restart step numbering. `admit` validates exact route,
epochs, current service eligibility, operation, deadline, size/direction and
increasing step before copying all input. Rejection changes neither output ID
nor queued work/reservation/time.

`RequestId` and `SequenceId` contain the stationary owner, fixed slot and a
nonwrapping serial. One counter issues both; exhaustion rejects new issuance.
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

## Scheduling and time

All time is supplied nonwrapping, nondecreasing monotonic microseconds; zero is
valid. No clock is acquired. Overall deadlines are immutable. Owner queue delay
counts against the first-frame queue budget: dispatch passes its original cutoff
to the adapter without modifying the copied transfer. Per-frame response/segment
budgets stay clamped to the same overall deadline. No polling or unrelated
traffic renews it.

| `ServiceBudget` | Per-turn maximum | Zero behavior |
| --- | --- | --- |
| `rx` | 8 receive callback calls, stop on empty | No controller receive calls |
| `events` | 8 actual TX-report callback calls, stop on empty | No controller report calls |
| `tx` | 2 one-shot controller enqueue attempts | No enqueue and no new dispatch |
| `starts` | 2 new native admissions, also needs TX budget | No new native admission |

Larger requests clamp to these bounds. With every budget zero, bounded native
progression, deadline expiry, invalidation and result collection still execute.
Passive events continue during active or unresolved SDO work whenever the
application supplies RX budget. Diagnostics never acquire another receiver.

Each turn settles deadlines, takes controller reports, routes RX, then services
staged frames and eligible new requests. Producer selection cycles 1..4, skipping
busy/ineligible channels; earlier nonterminal steps in the same sequence retain
FIFO order. Reservations do not block other nodes or passive delivery. Sequence
order is not a success dependency: dependent work is admitted only after its
caller has checked/advanced the prior result.

All scans and callbacks have fixed bounds: six request slots, two reservations
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
unknown; processing time is never substituted. Source-time, binding/map/epoch
validation and same-ID/same-DLC PDO ambiguity rules remain in the
[service contract](canopen_services.md#passive-network-events-and-node-lifecycle).

For a staged first download, optional `beforeTransmit` validates application
prerequisites and notifies the actual settings context before possible TX.
It runs once per download request, never for cleanup aborts. It performs no
operation stepping, I/O or reentrant owner call. `nextTx`, exact-token
`canTransmit` and one-shot `transmit` then run in the same cooperative turn
without receive interleaving. Failed enqueue is not retried.

Controller acceptance is separate from actual transmission. `takeTxReport`
supplies the exact native request/frame token; only an actual positive report
calls `confirmTx`. A stale negative report cannot fail a newer segment. A
protocol reply may settle service before a controller report; that late report
never rewrites the retained first terminal. Queue acceptance, CAN ACK and SDO
confirmation are not drive acceptance or motion completion. Actual SDK queue
retirement/recovery remains prompt 24/27 work.

## Cancellation, observations and channel uncertainty

There is no ordinary-owner cancel/recovery/urgent success stub. Existing native
cancellation/quarantine remains conservative; full owner cancellation and
controller recovery belong to prompt 24. Local cancellation is not stop.

Timeout after possible transmission preserves unknown effects and permanent
channel quarantine. Owner release, quiet time, new host IDs or a new sequence
cannot reopen it. Same-peer quarantine revokes reservation usability; queued
work fails and active work is invalidated before later RX/TX can restore success
or emit a segment. Already retained terminals stay unchanged; unrelated nodes
continue normally. Boot-up uses existing synchronous node/cache invalidation,
preserving interrupted outcomes and previous uncertain effects.

The [reconciliation evidence contract](canopen_services.md#unsupported-reopening-and-required-evidence)
still requires actual retirement/freshness evidence; local tokens cannot resolve
identical delayed SDO replies. Q20 leaves current Lichuan uploads unqualified,
including the standalone preview; reads are not automatically harmless.

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
to the actual new configuration. New-epoch reads then use the same reservation
and operation. Map/sample qualification, physical activation and motion completion
stay independent; no hidden save, reset, enable or retry occurs.

Typed contexts stay outside the owner. Native tests trace a public state read
and actual loop-setting ACK/readback through this path, keeping the original
settings token unchanged. Future confirmed groups in 20–22 use this same boundary;
prompt 45 checks combined integration again.
