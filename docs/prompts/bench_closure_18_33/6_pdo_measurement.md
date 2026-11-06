# Packet 6 Measure the next bounded PDO configuration subset

Apply [COMMON](COMMON.md). Prerequisite [1](1_evidence_contracts.md); use the
existing tested S2 bench without waiting for packets3-5. Originals18 and29,
with16/17's NMT and sample-provenance contracts. This is a finite experiment,
not general remapping support or motion through RPDO.

## Inspect

Read originals18/29 and reports `prompt_18.md`,
`2026-11-02_nmt_pdo_checkpoint.md`. Reopen vendor EDS records1400-1403,
1600-1603,1800-1803,1A00-1A03 and original EN86/ZH86 communication/map tables,
with Q17 in the ledger. Inspect `PdoCommand.*`, `PdoWatch.*`, `RawEvidence.*`,
`PdoExperiment.*` and `main/main.cpp` under `examples/s2_bench/`, plus
`scripts/s2_pdo.py`, `s2_pdo_test.py` and `s2_evidence.py`.
Refresh RS settings prior-state/partial-effect seams and INA228 validity;
FieldCore configuration generation stays read-only.

At authoring HEAD4eb7683 these commands already exist: addressed NMT via
N(node,o/p/s), finite watch P(seconds), record read R(node,t/r,set), and
W(node,set,subindex,baseline,value). Use actual syntax/help. W accepts TPDO
communication subindices1/2/3/5; COB-ID changes only toggle bit31. Parser-accepted
generic transmission-type values are not established Lichuan capabilities.

## Implement and execute

1. Independently review the new path from parser through admission, exact
   baseline read, single write, readback, retention and exit. Fix confirmed bugs
   before hardware. In particular: missing/wrong baseline emits no write; every
   possible transmitted write retains uncertainty; trace overflow/incomplete
   evidence prevents declaring qualification; no cleanup restores or retries.
   In the authoring revision `scripts/s2_pdo.py::run_steps` marks a step answered
   after matching markers without validating the firmware terminal. Add a
   regression where an acknowledged write is followed by failed/mismatching
   readback or a failed RESULT: no subsequent state-changing step may be sent.
   Validate the actual parsed terminal identity/outcome rather than an end marker
   or substring. Preserve both nodes' async traffic.
2. Build a concrete JSON plan using the existing schema and dry-run it. Select
   one node and TPDO1 initially. Capture fresh identity/state, exact communication
   and mapping fields, existing traffic and time bounds. Do not assume the EDS
   map is live: measured TPDO1 differs from documentary defaults. No broadcast,
   reset, RPDO command or save is part of this packet.
3. Start with unchanged-baseline observation and explicit addressed NMT state
   requests only when the required prior state is established. State the final
   intended network state. Start/Pre-operational/Stop are network actions, never
   a motor stop. Require stationary drive evidence for configuration; an NMT
   state alone does not prove that. An interrupted experiment leaves its actual
   state recorded, not silently repaired.
4. Select at most one unresolved timing field for this first assignment, with
   no more than two reviewed nonzero candidate values. Write their literal values,
   source/experiment rationale, NMT/disable prerequisites and finite watch windows
   into the plan before execution. Prefer event timing if current type/baseline
   allows an observable test; otherwise report that prerequisite specifically.
   Concrete initial proposal: TPDO1 event field1800:05, raw values100 and200,
   with a 10-second watch per value. These are selected experiment inputs within
   the raw U16 representation, not established milliseconds or a supported range.
   Use them only after original-source and independent transition review; record
   the actual baseline and each planned enable/state step. If that current type
   cannot produce an interpretable test, retain the specific gate instead of
   silently changing transmission mode or substituting another recipe.
   Do not sweep transmission modes, guess zero semantics, or enable SYNC/RPDO
   motion to force a result. Use a separate explicit action for any restoration.
5. Compare exact before/after readback and bounded arrival intervals. Separate
   value acceptance, observable timing change and confirmed units. If timestamp
   intervals are too broad, retain an interval/result rather than an exact device
   period. Inhibit semantics need a faster evidenced event source; periodic traffic
   alone need not prove them. Replicate a useful finite case only after its effects
   settle, never by replaying an uncertain write.
6. Emit a transition record consumable by packet7: selected specimen/profile,
   exact baseline/new fields, allowable states, required sequence, readback and
   failure behavior, timing meaning supported/not established, and raw capture
   references. A confirmed mapping and a fresh-qualified PDO sample are separate.

## Verify and finish

Use `pdo_command`, `pdo_watch`, `raw_evidence`, `pdo_experiment`,
`pdo_configuration`, `pdo_observations`, `s2_pdo_contract`, owner/NMT tests and
actual S2 build as affected. Existing entrypoints:

```text
python scripts/s2_pdo.py run --port COM15 --serial 94A990398B40 --plan <reviewed-plan.json> --output <new-directory> --dry-run
python scripts/s2_pdo.py analyze <retained-directory>
```

Discover the port anew; remove dry-run only after review/fake checks. Record
packet_6, update Q17 only for established facts, and commit/sync. One lead runs
hardware; a stronger reviewer checks command ordering and old-frame ambiguity.
Even a negative experiment completes its measurement if the finite plan and
raw outcome are retained; it does not complete the blocked production feature.
