# Prompt 19 Typed identity state and configuration reads

Apply [COMMON.md](COMMON.md). Prerequisites: [10](10_axis_coordinates.md), [12](12_object_generation.md), [14](14_sdo_reconciliation.md), [15](15_network_events.md), [17](17_pdo_observations.md).

## Objective and boundaries

Deliver a complete typed read lifecycle with minimal probe, identity, state and configuration observations. This prompt owns the common read machinery and the ledger's identity/state/configuration rows. Driver, I/O and tuning group reads stay with their settings in prompts 20–22. Discovery scanning is prompt 37. Do not turn this task into implementation of every object in the catalogue.

Use [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) for names and result meanings, and [MULTIDRIVE](MULTIDRIVE.md) for profile-specific observations. Reads expose documented facts; successful transport does not validate an unknown model or interpret a contradictory status bit.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `Reads`, actual getter callers and wrong-context tests; inspect INA228 freshness and destructive-read distinctions and ADS1115 output validity. Review FieldCore observation projection read-only. Inspect the reviewed Lichuan ledger and original pages for each owned block, including any read effects. Record the source delta rather than repeating unrelated audits.

### Concrete evidence to recheck

Inspect [object facts](../audit/LICHUAN_OBJECT_FACTS.md) for `0x1000/0x1001/0x1003`, `0x1009/0x100A`, `0x1018:00..03`, `0x2000/0x2001`, `0x2100..0x2102`, `0x603F/0x6041/0x6061/0x6064`, and I/O observations. No actual-following-error reading is established by the retained sources; alarm/in-position thresholds are settings, not measured following error. The EDS has no `0x1018:04` serial-number entry; do not require it for discovery. Several monitoring fields are RW in EDS but RO in manuals. Preserve raw observations and uncertainty instead of exporting write setters from the access flag.

## Implement

1. Implement `prepareProbe`, `prepareIdentity`, `prepareState`, `prepareConfig`, `nextRead` and `advanceRead` with concrete caller-owned contexts. Probe is a documented minimal non-changing read; it never starts NMT, configures heartbeat or sweeps all state.
2. Provide checked `getIdentity`, `getStateBlock` and `getConfig`. Wrong-kind, incomplete or stale-context access leaves output unchanged. Preserve raw identity/model/firmware fields, match confidence and unknown values. Do not invent a generic `getReadResult`.
3. Publish only matched service or valid PDO evidence with binding, object/map/configuration generation, supplied observation time and provenance. A multi-object read is not automatically an atomic snapshot: expose block validity and relevant skew instead of presenting torn observations as one instant.
4. Reuse existing object codecs, service matching and map decoding. Keep interpretation separate from raw read success. Unknown status dialect can block interpreted state while independently verified raw reads remain available.
5. Assign coverage rows explicitly. Record reviewed error-history and communication-record reads, including `0x1003`, `0x1005/0x1014/0x1017/0x1200` and PDO records, under this prompt or their named group owner in 20–22/38. No row may disappear because its writable counterpart belongs later. Implement only the read rows assigned here; PDO configuration remains prompt 18/38.

## Verify and exit

Test matched and wrong replies, every owned type/access rule, getter before completion, wrong identity/context, timestamp zero, mixed-age blocks, reboot/configuration changes, atomic publication and partial read failure. Assert probe and all reviewed non-changing reads generate no reset, save, enabling, I/O reassignment or hidden NMT request. A destructive read requires explicit ledger-backed semantics and may not enter probe automatically.

Exit only when all confirmed owned read rows and their negative tests are complete. Record unresolved fields separately from implemented reads and from unsupported models; an unsupported stub does not close a confirmed row. Run the focused native suites, relevant consumers and documentation gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document block validity, getter failure behavior, read effects and timestamps; update coverage and read examples. A normal coding agent can implement settled blocks; delegate independent provenance/getter tests after shared context types are stable. One agent owns read lifecycle and canonical ledger edits.

Close owned scope and checks in `docs/reports/prompt_19.md`. Commit and sync on the recorded execution branch before finishing; completed verified chunks may be committed and synced earlier. Leave physical qualification explicitly separate.
