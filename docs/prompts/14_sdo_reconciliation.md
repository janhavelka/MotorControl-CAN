# Prompt 14 SDO ambiguity and reconciliation

Apply [COMMON.md](COMMON.md). Prerequisites: [13 Bounded SDO transfer lifecycle](13_sdo_lifecycle.md).

## Objective and boundaries

Finish adversarial correlation and any evidence-backed channel reopening procedure on top of the safe transfer lifecycle. Keep permanently unresolved channels when no sound reopening evidence exists. The result is a verified settlement policy, not a promise to recover every uncertain transfer. Do not add hidden NMT resets, retries, motor stops or a second service engine.

## Read and audit

Refresh RS, ADS1115 and INA228 and inspect late-reply, cancellation, time and retained-outcome implementations/tests. Inspect FieldCore recovery/binding invalidation read-only and the selected controller/stack's queued TX retirement, RX timestamp and server-session assumptions. Use source-backed service timing evidence rather than analogies with Modbus. Apply [CONVENTIONS.md](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

## Implement

1. Write an explicit precedence table for receive time, processing time, operation deadline, cancellation, bus-off/reboot and terminal publication. A frame received after the deadline cannot become timely confirmation because the owner processes it later; also handle evidence received before a cutoff but processed afterward according to the documented contract. Terminal outcomes remain stable while separately timestamped later observations can still be retained.
2. Cover identical requests and same-object downloads with different values. Their acknowledgements do not echo written data, and a local operation/step token is absent from the ordinary SDO wire exchange. Validate target/channel/object/command without pretending those checks eliminate indistinguishable late replies.
3. Implement only reviewed reopening paths that address both the remote SDO server and local queued TX/RX. Examples must be justified by the actual selected device/stack contract, explicit externally established reset/session evidence or a bounded-drain premise with demonstrated controller/server bounds. An elapsed quiet interval, host generation change, successful client-abort enqueue or generic heartbeat alone is insufficient.
4. Ensure retirement/recovery cannot let an old queued write leave later as new intent. Keep binding/session generations and cancellation state tied to accepted work. Rebinding or node reset invalidates dependent evidence but does not erase the possibility of earlier side effects. Do not silently clear an unresolved channel to restore apparent availability.
5. Classify read effects from the ledger. Discovery and reconciliation use reviewed non-changing reads; upload/access flags alone do not prove non-consuming behavior. No particular destructive CL86 read is asserted without evidence. Even a harmless read's delayed reply can corrupt correlation. Preserve partial multi-write effects across a matching abort or later failure.
6. Allow explicit work over a different proven route/channel only under its own capacity, ordering and device contract; do not overwrite the unresolved channel to fit a stop request. Retain SDO-specific settlement below generic motion outcomes. Document unsupported recovery paths as unsupported capabilities, with precise evidence needed to add them.

## Verify and exit

Extend the fake harness with duplicate/stale replies, same-object differing payloads, late reply before/after release, cancellation at every TX boundary, delayed processing, timestamp wrap/overflow limits, reset/rebind, queued frames surviving controller recovery and concurrent unrelated events. Test each supported reopening precondition negatively and positively. For paths lacking justified premises, prove admission remains blocked rather than fabricating successful recovery.

Run independent stronger review of the lifecycle/precedence table, then focused tests and appropriate verifier/Doxygen/integrity/diff checks. Exit when every correlation branch is explained and tested; do not broaden the prompt into motion or machine recovery. Close scope in `docs/reports/prompt_14.md`, update PROGRESS and precise remaining evidence gates, then commit and sync this branch. A complete tested settlement path can be checkpointed earlier.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to channel reuse, time origin, release, late evidence and uncertain effects. Update `docs/canopen_services.md` and `docs/can_owner.md` without copying the same narrative. Apply [MAINTAINABILITY.md](MAINTAINABILITY.md#tests-and-refactoring). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), separate adversarial test design from implementation and assign stronger reasoning review to correlation premises. One implementer owns session/queue mutations and the lead integrates all checks.
