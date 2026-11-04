> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 6: Per-axis urgent work and interrupted-operation retention

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 5.
This is the owner portion of W6/C-01/02/17/18/19, original 24/31. Packet 7 owns
the concrete profile action and application integration. No physical drive test.

Read the moved owner and its caller loops, retained request/recovery results,
`SdoClient` outside-sequence admission, `ProbeApp` session storage, and actual
RS urgent stop/producer invalidation callers/tests. Compare PCA cancellation
and INA validity without importing sensor retry rules.

Implement W6's per-axis urgent capacity (`URGENT_REQUESTS = SEQUENCES` with the
audited channel/axis mapping), independent retained result capacity and identity
checks. One axis cannot take the other axis's urgent slot; an unread stop result
on node 1 must not prevent node 2's stop. Preserve ordinary fair RX/service budgets.

Add the required `beginUrgentOperation` and `findSequence` behavior. Retire locally
unsubmitted old work with explicit nontransmission results, but let an in-flight
poll settle rather than cancelling it and manufacturing quarantine. The stop
has its own operation identity and immutable deadline. Old sequence invalidation
cannot revive a copied target; a retired sequence slot may be reused without
destroying retained old results. Inspect the actual mapping when multiple
channels belong to one axis; do not equate a logical axis with a CAN node by
accident.

Complete the narrow transport route needed by packet 7's explicit best-effort
stop: only scalar U16 download6040:00=000B, no arbitrary urgent raw transfer.
Reuse packet 5's reviewed outside-sequence boundary. Pending external TX,
controller recovery/bus-off, known stopped/initializing network and exhausted
epochs still inhibit unsafe submission. Quarantine remains in force for ordinary
work. Incoming ambiguous replies cannot refresh service qualification or retire
a pending best-effort handoff as if its stop were acknowledged.

Prove identity, deadline, cleanup and retirement behavior with concrete test-only
requests; these fixtures are not a validated motor stop. Add full ordinary/result
pressure, two simultaneous urgent axes, stale producer/configuration epochs,
in-flight settlement, lost poll, queued never-sent stop, late reply and release
cases. Preserve original results and asynchronous heartbeat/EMCY delivery.

One writer owns central owner storage. Delegate pressure/timeline tests after
the API settles and independent review of every outside-sequence bypass. Tests
must assert exact admitted bytes and effects, not only counters. Run owner,
recovery, service/NMT, application and firmware regressions plus full verification.
Document capacity and migration, update original 24/31 and packet 6 reports, and
commit/sync with required CI. Exit with a real bounded path packet 7 can consume,
not a stop-success claim or placeholder action.
