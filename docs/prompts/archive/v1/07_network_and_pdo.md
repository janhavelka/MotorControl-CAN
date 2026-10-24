> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 07 Implement network events, PDO and node lifecycle

Apply [COMMON.md](COMMON.md). Prerequisites: stages 05–06.

## Audit

Read RS observation/generation and passive traffic handling, ADS1115 owner ingress, INA228 freshness semantics and FieldCore binding/availability invalidation. Inspect chosen stack heartbeat/NMT/PDO/EMCY configuration and manufacturer mapping tables. Do not use a generic CiA402 map as live configuration.

## Deliver

1. Extend the same application-owned bounded node/session storage from stages 02/06 and continuous event routing for boot-up, NMT heartbeat, EMCY, PDO and SDO; stage 12 integrates this storage. Route by actual configured COB-ID/channel and binding generation; report unknown IDs and drops without stealing traffic from normal processing. Do not create an installed global registry or a second receiver.
2. Keep network state, drive state, controller state, mode, error history, heartbeat liveness and sample freshness distinct. Heartbeat-disabled/never-observed differs from timed-out. Host monitoring does not establish drive-side consumer/watchdog behavior.
3. On boot-up/reset/rebinding/recovery invalidate affected cached config, PDO mappings, operation expectations, scale/reference and prepared work. Preserve interrupted/uncertain results. No automatic NMT start, re-enable or replay.
4. Implement decoding of reviewed PDO maps with exact bit/byte widths and mapping generation. Initially read back or explicitly establish mapping evidence; reject ambiguous data. Keep EDS default-map templates separate from actual state.
5. Implement typed bounded PDO configuration only where documented: preconditions, disable/map-count/reset/map entries/count/enable/readback order, allowed mapping object types and total length, failure at each step, unknown partially applied state. No silent rollback of uncertain writes. Expose supported transmission/event/inhibit settings with actual units.
6. Add explicit NMT commands and optional required SYNC support with typed effects and no hidden sends. NMT commands have no SDO-style acknowledgement: retain TX evidence separately from an observed state transition. Broadcast node 0 affects every addressed binding and invalidates their relevant epochs/work, rather than creating an ordinary node-zero target. Do not claim coordinated trajectory timing from basic PDO/SYNC capability.
7. Separate observed NMT state from current SDO serviceability. CANopen permits SDO in pre-operational and operational, PDO only in operational, and neither service in stopped. A fresh matching SDO response can establish serviceability without proving operational state. Since heartbeat may be disabled, permit caller-requested, reviewed SDO writes when their actual object/drive prerequisites and service evidence are satisfied; do not require a heartbeat merely to enable its producer. Keep discovery non-changing. Decode valid configured TPDOs when NMT is unknown rather than dropping the first possible evidence; any operational inference must cite the reviewed CANopen/profile contract and retain its provenance. Do not infer operational from NMT TX or SDO success. Test pre-operational/operational/stopped/unknown paths and explicit transitions against primary service rules and actual device evidence.

## Concrete implementation contract

Extend the application-owned service/node storage introduced in stage 06. Pure event decoders may live in `canopen/Network.h` and `Pdo.h`; node tables, subscription state, timestamps and counters stay in the application owner. Stage 12 integrates this same code.

Use a routing table that describes the configured identifier, frame kind, expected length, service/PDO mapping and binding generation. Dispatch an incoming frame once, then pass bounded decoded observations to interested operations/caches. A console/debug observer receives a copy and cannot consume the only frame. An unknown identifier is diagnostic traffic, not an automatic node discovery or protocol switch.

Keep these facts independent: last valid heartbeat, effective producer interval, last matching SDO service, PDO sample time, NMT state, drive status, EMCY history and controller state. A valid heartbeat restores heartbeat freshness only; it cannot erase an SDO failure or establish valid position/configuration. A successful SDO does not establish operational NMT state.

For PDO mapping, retain both reviewed template and observed active map with a generation. Validate index/subindex, mapped bit width, accumulated payload size, allowed access and profile-specific PDO count. Lichuan's four PDO sets are not a universal library bound; N5's eight are a contrast test. If remapping fails after changing count or disabling a PDO, mark the affected configuration unknown and expose the exact last confirmed step.

Explicit network transitions return TX evidence and observed transition evidence separately. Broadcast NMT targets the network domain, not a fabricated axis with node ID zero. Capture which bindings/work were invalidated. Keep requested state, last observed state/time and pending transition separate. A known-current stopped state prohibits ordinary SDO/PDO work. Once an explicit transition command may have been transmitted, retain the old stopped observation as history without treating it as indefinitely current; a bounded non-changing SDO probe may reconcile now-unknown serviceability. A rejected or proven-not-transmitted request leaves the prior evidence unchanged. The probe response proves service availability, not the requested exact NMT state. No implicit NMT start, hidden heartbeat configuration or timeout-based promotion to operational is allowed.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Update `docs/canopen_services.md` with routing, NMT eligibility and remap tables; update `docs/can_owner.md` with table capacities/ownership. Doxygen describes timestamps, raw event retention and invalidation precisely.

Use [DELEGATION.md](DELEGATION.md): independent event/PDO vectors and remap-failure tests can be delegated after the shared storage contract is fixed. One agent owns receive dispatch and routing changes.

## Verify and exit

Inject all NMT heartbeat states, known-state service restrictions, unknown-state non-changing discovery, NMT TX without observed transition, broadcast invalidation, boot-up during SDO/move, disabled heartbeat, explicit producer-heartbeat configuration after service reconciliation, the first valid TPDO with unknown NMT state, stopped-to-requested-transition reconciliation, missed deadline/late heartbeat, repeated EMCY, wrong length, wrong mapping, generation change, remap failure at every write, unknown node and queue overflow. Prove event processing continues while a service waits and stale events cannot restore old configuration validity.

Exit with testable independent network/event services and reviewed PDO behavior. Absence of hardware does not prevent software completion; actual device mapping/watchdog behavior remains independently qualified.
