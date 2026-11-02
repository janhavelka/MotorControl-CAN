# Packet 4: usable typed reads with honest read effects

Execute this packet fully. Apply [COMMON.md](COMMON.md).

Prerequisite: packet 3 (W3), as recorded in this handoff's index. This packet implements W4 and the remaining assigned software scope of [original prompt 19](../19_typed_reads.md). It does not implement discovery, communication writes or every readable catalogue entry.

## Read the actual contract

Read DEC-1 and W4 in [the repair audit](../PROMPTSET_AUDIT_34_50_2026-11-02.md), then `docs/reads.md`, `docs/reports/prompt_19_audit.md`, `docs/device_conflicts.md` and the current NMT/TPDO checkpoint. Inspect:

- `include/MotorControlCAN/profiles/lichuan_c/Reads.h`, `ReadOperation.h`, `src/profiles/lichuan_c/Reads.cpp`: fixed plans, `prepareProbe`, `prepareIdentity`, `prepareState`, `prepareConfig`, `nextRead`, `advanceRead` and checked getters.
- `adapters/canopennode/SdoClient.cpp`, `Nmt.cpp`, `examples/common/DeviceEvidence.cpp`, `examples/s2_bench/RawEvidence.cpp`: bootstrap permission, read effects and existing one-use experimental grants.
- The canonical `docs/reference/lichuan_objects.json`, generator and validator; generated `Types.h` and `Objects.h` are outputs, not editable policy.
- RS `profiles/ess_rs/Reads` and actual getter callers; ADS1115 `consumeTerminalResult` in `02_owner_safe_poll`; INA228 freshness/configuration-generation checks; FieldCore observation projection read-only. Refresh revisions and dirty paths under COMMON.

Reopen original EDS identity/history/communication sections and rendered EN86 p17, ZH86 p17–18. Do not infer a serial number at absent `1018:04`, a human firmware version, or actual following error from threshold settings.

## Implement the precise policy change

Implement the audit's `accepted_not_qualified` ledger disposition and corresponding `ACCEPTED_UNQUALIFIED` read-effect values. The accepted polling list is exactly `1000`, `1018:00..03`, `1009`, `100A`, `2000`, `2001`, `6041`, `6061`, `6064`, `606C`, `2100`, `2101`, `2102`. This is recorded application risk acceptance, not a manufacturer guarantee of non-consuming behavior.

Keep `603F`, `1001` and `1003` as explicit single reads with unknown effects; never put them in an automatic poll loop. All unlisted settings/configuration reads retain their existing disposition. Do not promote all 95 implemented read descriptors or change generic RW/access policy.

Make effect translation explicit end to end. In the current `Reads.cpp::work`, every value other than `UNQUALIFIED` becomes `REVIEWED_NON_CHANGING`; leaving that expression unchanged would falsely clear possible effects. Inspect every comparison in generation, SDO admission/completion, NMT eligibility, read advancement and evidence acquisition. An accepted upload may establish the specified channel usability, while `effectPossible` remains true and freshness requires its own evidence. `prepareProbe` uses only accepted `1000:00`, with no NMT or settings side effect.

Resolve the observed identity-width incompatibility narrowly. Both retained specimens returned one-byte `1009=0x0C`, `100A=0x20` while the EDS declares U16; `1018:00=5` also conflicts with metadata. Inspect retained raw traffic before changing types. Implement a reviewed fixture/model-binding rule that preserves actual width/raw data and contradictory EDS evidence. Do not relax ordinary SDO matching globally, infer an undocumented version string or silently change immutable source files.

Preserve finite ownership using packet 1's corrected ledger, not historical counts. The baseline had30 rows in 19, nine in 20,23 in 21,33 in 22 and 101 readable rows assigned to38. W1/G-07 reassigns12 of those101 to their actual write-feature owners, leaving89 with 38; verify every row and the total after that repair. This packet must neither absorb all communication/PDO reads nor orphan reassigned rows. Existing typed raw reads remain usable under their explicit service policy without being advertised as harmless.

## Prove the result

Extend `test/reads_test.cpp`, `objects_test.cpp`, `device_evidence_test.cpp`, `service_experiment_test.cpp` and generator/ledger tests. Cover the accepted allowlist, unlisted rejection, unknown effects after success, wrong widths, identity variants, wrong-kind/unfinished/stale getters, timestamp zero, mixed-age blocks, partial history, reboot/configuration invalidation and unchanged failed outputs. Test startup and cached inspection emit no traffic. Retain S2 receive intervals rather than fabricating exact arrival time.

Prepare the bounded ordinary typed acquisition and fake-test it here; packet 11 executes its live comparison against retained raw replies after transport/stack repairs and diagnostics. Fault/history reads remain separate one-shot observations with before/after fault indication, not a repetition-based proof of harmlessness. Keep that physical verification explicitly owned by11 rather than falsely claiming it now. Never conceal a mismatch by increasing timeouts.

Delegate descriptor/source accounting separately from independent getter/provenance tests. One implementer owns read lifecycle and effect translation; the lead owns the canonical ledger and generated integration. Run focused read/evidence/service suites, generator/checker tests, full verifier and applicable firmware checks. Update guides, coverage, CHANGELOG and Doxygen; report exact remaining evidence gates in `docs/reports/handoff_19_33/packet_4.md`. Commit and sync the checked result under COMMON.
