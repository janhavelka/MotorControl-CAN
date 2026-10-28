# Application CAN owner contract

[`MotorControlCANExample::CanBusOwner`](../experiments/api_ownership/CanBusOwner.h)
is the sole application receiver for the fake physical interface. It uses the
[selected service bridge](canopen_services.md) and accepts copied work from
[concrete operations](profile_contract.md). It is stationary, noncopyable,
cooperative and nonreentrant. Call it from one owner loop, never directly from
concurrent ISR/tasks. Core code has no pointer back to this owner.

## One storage table

`CanBusOwner::OwnerStorage` owns every row below. These are proof capacities,
not measured requirements for an embedded product. Prompts 13/15/23 extend this
same table rather than adding an independent request/channel/result registry.

| Storage | Capacity | Lifetime and authority |
| --- | --- | --- |
| Node/channel | One qualified fake node 1 / SDO channel 0 on bus 0 | One embedded prompt-5 bridge contains actual client, callbacks, 32-byte FIFO and retirable TX; fixed 0x601/0x581 route |
| Active transfer | One slot index plus bridge handle | Only this channel can advance that transfer; ordinary/urgent requests cannot overlap on it |
| Axis reservations | Two | Copied binding, operation, fixed deadline, nonwrapping reservation generation and last admitted step |
| Ordinary work | Two slots | Complete copied `ServiceWork`; admission reserves the corresponding terminal result |
| Urgent work | One distinct slot | Same lifetime; capacity is available independently of ordinary slots |
| Retained results | One per occupied request slot, three total | First terminal immutable until explicit `release`; no separate dynamic result queue |
| Asynchronous observations | Four copied raw frames/time values | FIFO consumed by `takeObservation`; overflow drops newest and saturates loss count |

The bridge's temporary terminal belongs to the active transfer. `collect` copies
it into the already reserved owner slot and immediately releases bridge storage.
This permits a later request to use the channel while older owner results remain
readable. No caller borrows the bridge result or callback buffer. Native NMT,
EMCY and PDO bytes stay observations rather than mandatory generic-axis fields.
No unbounded node cache or opaque heap storage is introduced.

## Admission and lifetime

`beginSequence` reserves one logical axis/endpoint with copied binding and
operation identity. Duplicate logical axes or identical physical endpoints are
refused. A reservation can outlive a request, its result and the wait between
operation steps. `releaseSequence` refuses while any associated request/result
is occupied; it is separate from `release(request)`.

`admit` and `admitUrgent` validate the full reservation/binding/operation and
strictly increasing step before copying work. Refusal leaves output IDs, clock,
reservation and slots unchanged. Foreign/stale handles fail. Each slot generation
is nonwrapping; exhaustion makes that slot unavailable. The owner address is part
of each host handle: handles must not outlive/refer to a reconstructed owner.
The application owns nonzero operation/binding/configuration generation issuance.

The proof adapter accepts synthetic profile identity 1, bus 0, CANopen node 1,
channel 0. Subaxis remains an identity field; the real profile must resolve its
index/subindex semantics before yielding work. This is not a vendor subaxis
addressing rule. Common bindings can describe other protocols/addresses; this
owner rejects their service work.

`result` is a non-consuming const view, null before terminal and after release.
Repeated service does not overwrite it. The caller advances its original typed
context using that evidence, then explicitly releases the request slot. Context
results survive slot reuse. Observation consumption has a separate copied-output
API and does not consume SDO results.

## Scheduling and time

`service` makes bounded scans of the fixed table and starts at most one eligible
transfer per call. One channel serializes all SDO traffic. Ordinary starts rotate
between eligible reserved axes. Urgent traffic wins at a settled boundary, with
at most one urgent start before eligible ordinary work gets its turn. Earlier
nonterminal steps of the same reservation stay ahead of later steps. Urgency
does not preempt a transfer or imply a motor-stop sequence.

The proof uses fixed per-call budgets, rather than runtime budget arguments:

| Entry | Supplied RX frames | Observation copies | New transfers started | Fake TX handoffs |
| --- | --- | --- | --- | --- |
| `service` | 0 | 0 | At most 1 | 0 |
| `receive` | At most 1 | At most 1 | At most 1 through service | 0 |
| `handoff` | 0 | 0 | At most 1 through service | At most 1 |

`service` scans the fixed three request slots and can progress the active
transfer before starting another. Conservative bounds on calls to the private
bridge's `service` are two from `service`, four from `receive` and three from
`handoff`. These are source-derived work bounds, not elapsed-time measurements.
`takeObservation` consumes at most one already copied event.

A caller with zero RX or TX budget omits `receive` or `handoff` for that turn;
zero event-consumption budget omits `takeObservation`. Calling `service` can
still expire work or stage a frame in the single retirable fake TX slot, without
transmitting it. Observation copying accompanies an accepted event frame and
has no independent budget argument. This describes the fake interface boundary,
not a production receive-starvation policy: deadlines keep advancing and the
application must schedule subsequent receive service.

Reservations enforce exclusion/order, **not success dependencies**. Callers must
not preadmit a dependent next step before advancing the previous result. An
independently authorized urgent stop may share a reservation without pretending
that a failed earlier command succeeded. The proof tests scheduling with
independent synthetic reads; its profile operations each yield one step.

All time is supplied nondecreasing monotonic microseconds, including zero.
Preparation fixes an exclusive deadline. First admission records that deadline
in the reservation; later steps must use exactly the same value. Admission requires a future deadline
no more than 65,535,000 us away and no later than `UINT64_MAX - 65,535,000` so the
coarse bridge timer cannot overflow even after queued delay. Invalid input is
rejected atomically. `service`, RX and final handoff expire work before accepting
evidence at the deadline. No queue/transfer progress extends it. These bounds are
prototype policy, not a drive timeout guarantee or the production clock design.

## Cancellation, observations and channel uncertainty

Successful queue admission does not prove transmission. In this fake controller,
`handoff` is the first point at which external TX becomes possible; there is no
CAN ACK simulation. `cancel` retires unsubmitted fake work and ends locally. It
never sends a physical stop or automatically retries. A failed/cancelled/expired
possibly transmitted request interlocks the channel even if it was a read.
Queued unsent work then ends with transport failure; terminal slots stay intact.
Releasing a result or reservation never resolves a channel.

RX remains active without an SDO and while results are retained. The proof
accepts node-1 heartbeat (0x701, one byte), EMCY (0x81, eight bytes) and raw TPDO1
(0x181, 1–8 bytes). It does not infer a map, sample freshness, drive state or
health from these frames. Boot-up heartbeat value zero invalidates route
qualification, cancels active work, retires local TX and fails queued work, even
if the observation ring is full. No reset/rebind/recovery shortcut is supplied.
Previously retained results remain historical evidence with their old binding.

Native source timing, real controller retirement and late identical-response
ambiguity are bounded premises in [canopen_services.md](canopen_services.md).
The fake proof is not evidence that real hardware can retract an accepted frame.
Health, caches, traces and persistence belong to the later application; they
must not become implicit recovery/actuation policies in this owner.
