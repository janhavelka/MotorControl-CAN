# Prompt 2 Source inventory and ledger intake

Apply [COMMON.md](COMMON.md). Prerequisites: [1 Current source audit](1_source_audit.md).

## Objective and boundaries

Create a lossless, traceable intake of vendor sources and the Lichuan object dictionary. Finish accounting and schema validation here; prompt 3 reviews semantic permissions and sequences. Imported metadata is not an implemented feature, and an unreviewed EDS entry is not permission to write a drive. No generator, production driver or live CAN exchange is required.

## Read and audit

Refresh RS, ADS1115 and INA228 revisions and inspect the dictionary/type/access-policy seams relevant to this work. Inspect FieldCore's affected configuration/identity seam read-only. Read RS's register ledger, generator, checker tests and freshness workflow; use MB85RC variant handling and PCA9555 reserved-field policy where relevant. Follow [CONVENTIONS.md](CONVENTIONS.md#1-public-names-layout-and-dependencies) for names and ownership.

Read the reference manifests, original EN86, ZH86, OP86, ZH57 and OP57 covers and EDS sections. Use the protocol audit to locate conflict pages, checking rendered PDF pages when extracted text loses tables. Read [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases) to keep manufacturer identity and common capability facts separate.

## Implement

1. Verify all original sources, extracts, copied baseline and retained evidence with `python scripts/check_promptset.py --check-git-ref HEAD`. Preserve `.gitattributes` byte-protection rules and immutable originals. Record source ID, URL, acquired file, SHA-256, document title/version, applicable model and the evidence for that applicability. PDF metadata and EDS creation times do not establish firmware identity.
2. Create the reviewed-ledger schema and initial `docs/reference/lichuan_objects.json`. Suggested fields cover key, model/dialect/firmware scope, index/subindex, record/array/scalar kind, datatype/width/signedness, access, limits/default, units/scaling, mapping eligibility, effect, prerequisites, edge behavior, source/page/EDS section, confidence, conflicts, owner prompt and coverage state. Preserve unknown values explicitly; `null` is never a writable default.
3. Import EDS structure without silently filling absent values. Account for all 99 indexes and every subindex; separately account for the 202 scalar entries carrying explicit DataType. These counts are different checks, not counts of supported operations. Keep arrays, records, reserved entries, excluded entries and address gaps distinguishable. A deterministic intake helper is acceptable; it must not replace semantic review or infer control sequences.
4. Inventory manual-only objects and semantic functions, with document/page and an explicit pending-review disposition. Every EDS entry and manual-only item maps to a row or an exclusion with a reason. Record conflicting source values side by side; do not select a winner because a file looks newer or English text is easier to parse.
5. Define documented, implemented, host-tested and hardware-qualified coverage as separate fields. Establish readable source/accounting reports and the handoff into prompt 3. Do not generate public writable declarations from this intake.

## Verify and exit

Validate schema, duplicate keys, index/subindex uniqueness within the applicable model/dialect, numeric formats, widths and source references. Test the validator against malformed rows, duplicates, missing applicability and contradictory claims marked resolved without evidence. Compare representative scalar, array, record, reserved and manual-only rows against original pages/EDS, not solely extracted text.

Run the integrity check and `git diff --check`; for any artifact or attributes change, verify staged Git bytes with `--check-git-ref INDEX` and committed bytes with `HEAD`. Complete this intake with a full accounting report and explicitly pending semantics. Write `docs/reports/prompt_2.md`, update PROGRESS and changed coverage/backlog facts, then commit and sync the same execution branch. Useful complete checkpoints are allowed.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#source-evidence-for-this-contract) and [MAINTAINABILITY.md](MAINTAINABILITY.md#structure-and-naming). Document schema and reproducible validation commands; original vendor material retains separate licensing. Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), delegate EDS accounting and PDF inventory independently. One implementer owns the schema and ledger merge. A reviewer must inspect raw source rows independently of the importer.
