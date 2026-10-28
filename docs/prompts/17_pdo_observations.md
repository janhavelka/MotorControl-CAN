# Prompt 17 Observed PDO maps and decoding

Apply [COMMON.md](COMMON.md). Prerequisites: [12](12_object_generation.md), [15](15_network_events.md), [16](16_nmt_services.md).

## Objective and boundaries

Implement checked PDO mapping representation, explicit mapping evidence acquisition and passive PDO decoding. Use the application-owned storage/dispatch established in 13-16; the complete scheduling owner arrives in 23. This prompt does not silently configure a drive or implement remapping; prompt 18 owns explicit mutation sequences. EDS templates describe possible/default maps and are never proof of current live configuration.

## Read and audit

Refresh RS, ADS1115 and INA228, inspecting generation-tagged observations, native widths and freshness with actual consumers/tests. Inspect FieldCore binding/feedback invalidation read-only. Read selected stack PDO APIs, original Lichuan mapping objects and reviewed ledger eligibility. Use N5's documented larger PDO count only as a contrast. Follow [CONVENTIONS.md](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases), plus prompt 16's completed service-eligibility policy in normal execution order.

### Concrete evidence to recheck

The [EDS PDO table](../audit/LICHUAN_OBJECT_FACTS.md) supplies concrete fixtures: RPDO1 `1600` count 3 maps `60400010,60FF0020,60600008` (7 bytes); RPDO2 `1601` count 2 maps `607A0020,60810020` (8 bytes). TPDO1 `1A00` maps `60410010,60FD0020,60610008` (7 bytes); TPDO2 `1A01` maps `60640020,606C0020` (8 bytes). PDO3/4 counts default to 0 in both directions. Decode mapping words as index/subindex/bit length; all records offer four slots, not four necessarily active values. These are source-derived fake fixtures until live map/route evidence exists.

## Implement

1. Add bounded pure PDO mapping validation and decoding under the existing `canopen/` boundary, such as `Pdo.h`. Validate index/subindex, reviewed datatype/bit width, mapping eligibility, accumulated Classical payload length, PDO direction and profile-specific supported count. Separate SDO access from PDO direction: default RPDO1 contains WO `6040/6060`; mapping-record readback does not require uploading those payload targets. Lichuan's four sets are not a universal library limit; accommodate source-backed N5 contrast counts without stepper/servo branching.
2. Represent reviewed template and observed active map separately. Active map evidence includes binding/profile/configuration generation, actual configured COB-ID/frame expectations, mapping entries, timestamp and provenance. Read back supported mapping parameters through existing bounded SDO services or accept an explicitly established reviewed configuration with clear provenance. Never label an unverified template as observed.
3. Route each matching PDO once through that bounded application storage, then publish decoded observations to interested contexts/caches. A trace/console observer receives a copy and cannot consume the frame. Unknown IDs, unproven maps and malformed lengths remain diagnostic events rather than updates to position or drive state.
4. Decode exact native values with validated bit/byte widths and sign extension; separate received actual/commanded/estimated feedback according to reviewed object meaning. Preserve native words and source timing. Publish a complete candidate observation only on successful validation; one malformed field cannot leave a partially updated sample.
5. Tie observations to mapping and binding generations, invalidating dependent values after reset/rebind or uncertain map changes. Generations are absent on the wire: delayed old bytes with the same COB-ID/DLC can arrive in a new generation. Follow the [sample-provenance contract](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md#pdo-sample-provenance-after-a-route-changes): keep ambiguous samples unqualified until an evidenced transition/retirement boundary, distinct verified route or applicable observable marker resolves them. Confirmed map contents and qualified samples are separate. Keep sample freshness independent from heartbeat, SDO success, EMCY history and health.
6. Accept valid configured TPDO evidence when exact NMT state is unknown under the established service policy. Any inference about Operational state needs the reviewed CANopen/profile basis and provenance. Do not infer valid drive position, mapped scale or command completion from PDO arrival alone. Add only required pure SYNC format support where reviewed; active scheduling and coordinated trajectory guarantees are outside this observation prompt.

## Verify and exit

Construct independent mapping/payload vectors from the reviewed dictionary. Test widths/signs, invalid/zero/overflowing lengths, ineligible/reserved objects, wrong COB-ID/flags/length, map changes, first TPDO with unknown NMT and uncertainty. Preserve default RPDO1 without granting SDO reads of WO targets. Deliver same-ID/same-DLC old-map bytes after the new generation without an old-generation test label; they cannot renew observations without the required provenance. Exercise four/eight-set capacity contrasts without installing another driver.

Interleave PDOs, heartbeat/EMCY and SDO activity; ensure observers cannot steal frames and malformed PDOs leave output unchanged. Verify readback failure does not certify a map. Run native/sanitizer tests where useful, Doxygen/verifier, integrity and diff checks. Exit with tested actual-map observation semantics and no implicit writes. Close scope in `docs/reports/prompt_17.md`, update PROGRESS and coverage, then commit and sync this branch. Useful complete checked map/decoder groups may be checkpointed earlier.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to mapping provenance, validity, signedness, sample time and atomic publication. Update service and owner-capacity guides; apply [MAINTAINABILITY.md](MAINTAINABILITY.md#tests-and-refactoring). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), delegate independent mapping vectors and generation tests while one implementer owns routing/storage integration. Require independent review of actual-map versus template evidence.
