# Prompt 13 Bounded SDO transfer lifecycle

Apply [COMMON.md](COMMON.md). Prerequisites: [5 CANopen service fit experiment](5_stack_decision.md), [6 Common API and ownership proof](6_api_ownership.md), [11 Classical CAN and SDO codecs](11_can_sdo_codecs.md).

## Objective and boundaries

Implement a complete bounded SDO transfer lifecycle with conservative uncertainty handling from the first usable version. Use the selected stack adapter or caller-owned local session, never both. Prompt 14 adds reviewed channel reconciliation and deeper adversarial proof; this prompt must already quarantine ambiguous channels safely. No hardware traffic, motion sequence or automatic retry belongs here.

## Read and audit

Refresh RS, ADS1115 and INA228. Inspect RS owner/runner cancellation, retained results and no-replay behavior with callers/tests; compare MB85RC effect results and RV3032-C7 uncertain writes. Inspect FieldCore service ownership read-only and the chosen stack's actual session/callback/error paths. Recheck ledger widths and [CONVENTIONS.md](CONVENTIONS.md#4-one-application-can-owner). Apply [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases): SDO is a CANopen service, not a universal native acknowledgement requirement.

### Concrete evidence to recheck

The EDS [communication records](../audit/LICHUAN_OBJECT_FACTS.md) give default client/server COB-IDs `$NODEID+0x600` and `$NODEID+0x580` in `0x1200:01/:02`; use an explicitly configured/source-backed initial channel and bounded non-changing confirmation; do not probe guessed alternatives with writes. The [public example](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) uses sleeps instead of acknowledgement and has no sufficient transaction correlation. Every download step must await matching protocol evidence. A caller-configured SDO deadline is not the vendor's approximate 10 ms enable-step interval.

## Implement

1. Implement the selected boundary using prompt 6's existing channel storage. One active transfer per actual SDO channel; other channels/nodes and unsolicited traffic can progress through the same dispatch boundary. Keep controller I/O in the application adapter and time supplied by its caller. Admission copies retained request data and records target, operation/request/step identities and relevant epochs.
2. Represent local admission rejection, queued TX, possible/confirmed TX, valid SDO success, full abort, timeout, cancellation, bus-off and reboot independently. Queue acceptance or CAN ACK is not SDO confirmation. A matched download acknowledgement does not establish motion completion; an abort cannot roll back earlier writes in a larger workflow.
3. Support the transfer forms actually required by the reviewed objects/server contract. All 202 supplied EDS scalar datatype entries are one to four bytes, but a server may choose segmented upload even for a small object. Handle expedited replies with and without size indication correctly. Reject known unsupported downloads before TX. A valid unsupported segmented upload discovered after initiation produces an explicit unsupported result, bounded protocol abort where required, and safe channel/effect settlement; never claim no request was sent or call it malformed.
4. If segmentation is required, implement bounded toggle, last-segment, declared-size and capacity checks through the chosen stack or selected local subset. Do not add block transfer for hypothetical future need. Document supported forms and size limits explicitly.
5. Fix an immutable overall deadline at admission. Clamp queue, transmission, response and step cutoffs to it; actual TX may start a shorter response interval but cannot renew the total budget. Polling, unrelated frames and partial progress cannot extend time. Reject invalid time arithmetic and bound each service turn. Stalled TX queues must terminate predictably.
6. Retain terminal results until explicit `release`. Once an unconfirmed request may have been transmitted, keep effect uncertainty and mark its channel unresolved; reject ordinary reuse that could accept a late reply. Cancellation is not a motor stop. No quiet interval, host token/generation increment or client abort alone proves the server did nothing or the channel reusable. Safe reconciliation is added in prompt 14.

## Verify and exit

Build a fake controller/node harness with no real CAN. Test admission/queue rejection, delayed/stalled TX, upload/download success, full 32-bit abort, unsupported upload settlement, wrong response, cancel/deadline, reboot, bus-off, storage exhaustion and segmentation boundaries if supported. Interleave another channel/node's traffic without stealing it. Mutate caller buffers after admission to verify owned payloads; terminal results remain stable until release.

Test that transmitted uncertain writes are never replayed and cannot be followed by unsafe same-channel work. Only ledger-reviewed non-changing reads carry no drive-changing effect; uploads are not inherently harmless. Run focused tests, Doxygen, relevant verification, integrity and diff checks. Close scope in `docs/reports/prompt_13.md`, update PROGRESS, then commit and sync this branch. No deferred uncertainty fix is acceptable at this exit.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to payload/result lifetimes, supplied time, state/outcome/effect and allocation boundaries. Update service guide and CHANGELOG; use [MAINTAINABILITY.md](MAINTAINABILITY.md#make-control-flow-and-state-reviewable). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), one implementer owns session storage; delegate fake-node vectors and stronger independent deadline/cancellation review. Useful complete checked chunks may be synchronized earlier.
