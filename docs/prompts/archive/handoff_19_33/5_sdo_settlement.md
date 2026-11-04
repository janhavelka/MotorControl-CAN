> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 5: Explicit SDO settlement and the drain-marker premise

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 4.
Read original 13/14/24, W5, DEC-3 and C-05/06/N-03/N-04 in the active audit.
Read current `SdoClient.cpp`, `Network.cpp`, owner recovery, `DeviceEvidence.cpp`,
`sdo_transfer_test.cpp`, `sdo_reconciliation_test.cpp`, `can_bus_recovery_test.cpp`
and `reference_action_test.cpp`. Keep historical partial effects separate from
channel usability.

## Implement the bounded correction

Reproduce current matched-abort and bus-off behavior. A correctly matched server
abort on a clean correlated transaction settles that transaction without itself
requiring permanent quarantine. It still reports rejection and preserves earlier
multi-write effects. For recovery/bus-off, sample possible work, live handoff and
cleanup before retirement clears them; never label untouched/proved-unsent work
possibly transmitted. Boot/rebind evidence and ordinary NMT eligibility remain
separate invalidation rules.

Implement DEC-3's explicit drain request through the existing owner/service, not
a second SDO client. Before reopening, establish sole-client ownership, local
TX retirement and the stated ordered-server premise. The audit labels remote
ordering as unconfirmed; reopen primary protocol/source evidence and record a
fixture/profile qualification gate. A host fake validates logic, not that remote
premise. Unsupported premises must leave actual routes blocked while independent
settlement fixes finish.

Retain the bounded four object-pair history, including unresolved requests and
stray frames. A marker uses an accepted upload with a distinct index/subindex;
overflow, unidentifiable frames, an outstanding external handoff, stale epochs
or an ineligible controller/NMT state refuse it. Known old replies cannot settle
the marker; unexpected traffic fails it. No elapsed quiet interval, generic
heartbeat or same-object readback substitutes for the required boundary.

Use the audit's `admitDrainMarker`/outside-sequence path only for its narrow
purpose. Invalidate every dependent node reservation before successful reopening,
retain the requalification counter and original uncertain outcomes, and consume
old controlword history. Ordinary admission remains unchanged. Each attempt is
new explicit intent with a finite deadline; no retry or replay follows it.

Fix diagnostic overflow handling: finish/settle the already submitted evidence
request, then withdraw the unstarted remainder. Size trace storage against actual
per-turn production. Remove tests that reconstruct SdoClient in place to pretend
recovery; test the supported path and preserve its history instead.

## Verification and delegation

One implementer owns channel/session/queue mutations. Delegate a failing-case
matrix and a strong independent review of remote ordering and late identical
replies. Test absent-object abort, untouched second node after recovery, marker
wrong/same pair, history overflow, malformed/stray frame, late old replies before
and after marker, failed marker, current reservations invalidated and earlier
effects still unknown after successful marker. Keep
`uncertainCounterTriggerCannotReopenThroughEpochSync` and quiet-time/reboot/token
negative cases meaningful. Positives requiring an ordered server must name that
fake premise explicitly. Packet 11 owns any physical premise experiment.

Run full verification, owner/service/evidence suites and affected firmware gates.
Update `docs/canopen_services.md`, owner guidance and original 13/14/24 reports.
Finish packet 5 report, independent review, exact-commit CI and sync. Do not mark
remote-server qualification complete from the software settlement result.
