# Stage 06 Implement bounded SDO execution and uncertain outcomes

Apply [COMMON.md](COMMON.md). Prerequisites: stages 02 and 05.

## Audit

Read RS runner/owner retained evidence, cancellation, late reply and no-replay tests; compare MB85RC transport-effect results and RV3032 uncertain persistent writes. Inspect actual chosen stack SDO client callbacks/session storage/error paths. Recheck every selected native object size before deciding expedited-only scope.

## Deliver

1. Implement the selected application-owned service adapter or caller-owned bounded local session. One active transfer per actual SDO channel; allow other channels/nodes and all asynchronous events to progress. Keep controller I/O outside the portable core.
2. Support required upload/download transfer forms. The supplied Lichuan EDS has 202 scalar entries with explicit DataType, all one to four bytes; width alone does not require segmentation. It also does not prove that every server upload will be expedited. A server may select segmented upload even for a small object. If supported objects or observed required server behavior need segmentation, use bounded toggle, last-segment, declared-size and capacity checks, preferably through the selected stack. Reject known unsupported downloads before TX. An unsupported upload form may become known only after its initiation reply: return an explicit unsupported outcome, send a bounded protocol abort where required, and apply the channel-settlement/effect rules. Do not mislabel a valid reply as malformed or claim that no request was sent. Do not implement block transfer without a demonstrated need.
3. Represent admission, queued TX, actual TX evidence, valid SDO success, abort, timeout, cancellation, bus-off, node reboot and unknown side effects independently. Record original target/channel/request/operation/step, copied payload, observed time and epochs.
4. Define timeout/late-response policy: no automatic motion write retry; identical delayed replies are not distinguishable with a host token alone. Retire/quarantine/reconcile channels and expose ambiguity, including same-object new requests. A client abort attempt is not proof that the drive did nothing.
5. Make polling/advancement bounded and time-supplied. Retain an immutable overall deadline established at admission; clamp each queue/transfer/response/step cutoff to it. Actual TX evidence can establish a shorter response deadline, never renew the operation budget. Polling, partial progress and unrelated events cannot extend it. Document timestamp limits and overflow/wrap handling. Stalled TX queues cannot hang a service forever. Terminal results remain stable until explicit release.
6. Supply a fake controller/node service harness with injected events, no real CAN traffic. Document stack-specific allocation/setup and callback lifetime separately from core guarantees.

## Concrete implementation contract

Implement the service boundary chosen in stage 02, using its existing caller-owned channel storage. Record the lifecycle explicitly:

| Event | Service result | Effect evidence |
| --- | --- | --- |
| Admission fails before any TX | Rejected locally | Not transmitted |
| Adapter accepts a queue entry only | Still pending | Transmission not established |
| Frame may have left the controller, no valid reply | Pending, then timeout/cancel as applicable | Download effect unknown |
| Matched download acknowledgement | Service succeeded | Protocol acknowledgement; no motion-completion claim |
| Matched SDO abort | Protocol rejected | Preserve full abort; do not assert partial multi-write rollback |
| Node reboot or controller fault | Interrupted/failed | Preserve possible earlier effects and invalidate dependent evidence |

Define the request payload's ownership at admission and terminal result's lifetime until `release`. Supply monotonic time from the caller, use explicit units, and reject invalid time/deadline arithmetic. A time advance cannot renew an expired operation.

The conservative initial rule for an unconfirmed transmitted request is to mark that SDO channel unresolved and reject new ordinary work that could accept an indistinguishable late reply. A quiet interval or host-only generation change does not prove reuse is safe. Before reopening, require a documented reconciliation procedure that addresses the remote SDO server and local pending TX/RX, or explicit externally established reset/session evidence under the selected device/stack contract. Do not hide an NMT reset in recovery. If a useful bounded-drain assumption is used, document and test its actual controller/server timing premises; without those premises keep the channel unresolved.

Only ledger-reviewed non-changing reads may be treated as having no drive-changing effect. An SDO upload or EDS read-only access flag alone does not prove a read is non-consuming or cannot clear a latch. Classify any documented read effects explicitly; discovery and reconciliation use reviewed non-changing reads. No specific destructive CL86 read is established by this warning. Even a non-changing read's late reply can poison a new request's correlation. A valid abort ends the matching service; bus ACK does not. Stop over another proven route/channel remains possible under its own reviewed contract; do not silently overwrite the unresolved channel.

Test same-object downloads with different values as well as identical requests: the acknowledgement does not echo the written value. Define receive-time versus processing-time and cancellation/deadline precedence so a delayed service turn cannot turn late evidence into timely confirmation. Preserve terminal outcomes while allowing separately timestamped observations to update.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Document service outcome/effect tables, channel reuse rules, timestamp origin, per-transfer storage, segmented-transfer limits and how an application releases results. Public service/context declarations need Doxygen for output mutation and uncertainty.

Use [DELEGATION.md](DELEGATION.md): assign an independent high-reasoning review of correlation and deadline logic. A separate fake-node test writer can inject adversarial replies after the service contract is fixed. Keep stack/channel ownership changes under one implementer.

## Verify and exit

Test queue rejection before TX, delayed TX, successful upload/download, expedited uploads with and without size indication, a valid unsupported segmented initiation and its settlement, valid 32-bit abort, wrong response, stale/duplicate reply, reply after cancel/deadline, same-object retry ambiguity, reboot during transfer, bus-off, storage exhaustion and all segmented boundaries if supported. Interleave another node's heartbeat/PDO throughout. Prove failed writes are not replayed automatically.

Exit with predictable service outcomes that later motion code can trust without owning a bus. No sequence may treat CAN ACK or queue acceptance as SDO confirmation.
