# 02 — Object ledger, checkers and generated tables

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 01. All work is host-side. This is a tooling and registration change: it follows the
major-change CI rule of the contract.

## Work

1. Build `docs/reference/lichuan_ds2_objects.json`, schema 2, with the keys and entry fields named in
   [canopen_shared 01](../canopen_shared/01_existing_code_and_tooling.md) "Intake rules", CANopen ledger. Scopes, one
   per document group: DS2 (DS2C, DS2OP, DS2EDS), MINI (manual only), IBL60 and IBL80 (own manuals, shared IBLEDS),
   ISL60 and ISL80 (own manuals, shared ISLEDS). Each entry lists the scopes whose sources contain it; an object absent
   from a scope's sources is absent there, not inferred from a sibling.
2. Account losslessly for every section of DS2EDS, IBLEDS and ISLEDS (60 objects each per the pack; confirm counts)
   and every row of the six manuals' object tables (DS2C p24-29 and the matching pages of MINI, IBL60, IBL80, ISL60,
   ISL80) and mode tables (DS2C p10-13 and equivalents). Mode-table rows and object-table rows are separate claims of
   the same object; keep both.
3. Record each conflict as a `conflicts` row holding every original claim under the hash guard: DS-D01 and DS-D05
   (`1018`), DS-D06 and DS-D07 (PDO, `1014`, `1200`), DS-D08 (`6040`/`6060` RW versus WO, `6060` I8 versus U8, `6098`
   I08 versus U8 and default 17 versus 0), DS-D09 (`607A`), DS-D10 (`60FF`), DS-D11 (`607F` widths and ranges,
   `2406h`), DS-D12 (`6087`), DS-D13 (`6072`), DS-D14 (`6077`), DS-D15 (`607D`), DS-D16 (`6083`, `6084`, `6099`,
   `609A` units and defaults), DS-D17 (`605D`), DS-D18 (`6098` range), DS-D19 (`1003`) and DS-D30 (both parts:
   the `60FD` default and the `P02-34` monitor designation in DS2C p22 versus `P0B-34` in MINI p24, a `manual_only`
   claim), plus
   stage 01's intake corrections: DS-D33 (`6081` default 10000 pul/s, DS2C p28, versus "posmode speed" 60, DS2EDS
   l.1398-1405) and DS-D34 (`6091:02` type not printed, DS2C p28, versus I32, DS2EDS l.1464-1470). The pack review
   stays unedited; the conflict rows cite stage 01's report.
4. Choose conservative reviewed policy, never the convenient claim: `6077` and `6041`, `6061`, `6064`, `606C`, `60FD`,
   `603F`, `1001`, `1003` read-only; `6040` and `6060` written by typed operations only, with no readback required;
   `6061` never written. Four-byte width for `607A` and `60FF` with the unqualified range 0..2147483647 (where the
   signed and unsigned claims mean the same wire value) and negative encodings rejected under a `DS-G` gate;
   `6072` nonnegative only; `6087` bounded at 65535 until its range conflict closes. `607F` download width per scope:
   withheld in DS2 and MINI (U16 in the mode table versus U32 in the object table and DS2EDS, DS-D11), four bytes in
   ISL only after this stage confirms that both ISL manuals' tables and ISLEDS agree. IBL `2406h` writes stay
   withheld until model- and firmware-scoped evidence of meaning and write effect is recorded (06b). An RW or PDO flag
   grants no write; `write_policy` names the owning stage's typed operation, withholds the write with its gate (as
   Lichuan's `qualification_required` does), or is `none`.
5. Put every function without a CANopen index into `manual_only` with disposition `no_canopen_index`: each P-parameter
   group P00..P0D, with individual rows for node and bitrate (`P0C-01`, `P0C-08`), control mode (`P02-00`, DS-D29),
   absolute encoder and direction (`P02-01`, `P02-02`), brake delays (`P02-09`, `P02-10`), save and restore
   (`P02-31`), encoder lines (`P00-31`/`P00-32`), motor and drive codes (`P00-00`, `P01-02`), DI and DO functions
   (`P03`, `P04`), gains and auto-tuning (`P08`, `P09`), protections (`P0A`) and every side-effect row in the review's
   "Vendor objects with side effects" table. IBL `2406h` (IBL60 p13, IBL80 p13; absent from IBLEDS) and ISL `60B8h`
   (ISL60 p12; absent everywhere) are `manual_only_object` rows with their own gates.
6. Define `semantic_review.gates` `DS-G01`.. so that each open DS-D maps to at least one gate with its required
   evidence and owning stage, and `read_effect` per entry. Every read effect starts unknown. A bounded polling list
   needs a DS2 decision (`DS-DEC-1`, prepared by 03) with `selected_by`; DEC-1 does not transfer. Until it exists,
   results keep `effectPossible` and only the explicit single reads a sequence names are allowed.
7. Write `scripts/generate_lichuan_ds2_objects.py` (`--check` never writes). Outputs:
   `include/MotorControlCAN/profiles/lichuan_ds2/Types.h`, `Objects.h`, optional `Catalogue.h`, `Pdo.h` with the
   per-scope record and slot counts only (DS2EDS l.32 and l.554; no default map is generated as a fact, DS-D06), and
   `docs/reference/lichuan_ds2_generated.md`. Extend `TYPE_CODES` only from the family EDS files. Add LF rules for the
   generated headers to `.gitattributes`. The commit that adds these first DS2 headers also adds, under S7 (canopen_shared
   01 intake rules): an `install(FILES ...)` rule for `profiles/lichuan_ds2/`, their `test/package_test.py` `HEADERS`
   and exact archive entries, and the per-header C++11 and C++17 consumer compiles; it passes `test/package_test.py`.
   `library.json` and `check_repository.py` `EXPORTS` stay unchanged; 02b extends the lists only for its own files.
8. Write `scripts/check_lichuan_ds2_ledger.py` and `scripts/check_lichuan_ds2_operations.py` (operation map
   `docs/reference/lichuan_ds2_operations.json`, schema 2, with an empty `families` list until 02b). Claim seam S7
   in the canopen_shared table, then land or extend it: scope `check_lichuan_operations.py` to common and `lichuan_c`
   headers and `check_lichuan_ds2_operations.py` to `profiles/lichuan_ds2/`, with regressions that an unclassified
   common, Lichuan C or DS2 function still fails and that the checkers' header sets are disjoint and together claim
   every public header with public functions once.
9. Register each checker in `scripts/verify.py`, `scripts/check_repository.py` (`CHECKS`, file lists) and CTest.
   `check_promptset.py` already discovers `docs/reference/*/SOURCES.json` by glob; confirm the pack is counted and
   do not edit `reference_manifests`.

## Deliver

- The ledger, operation map, generator, two checkers and generated outputs above.
- Negative tests: `scripts/check_lichuan_ds2_ledger_test.py` (missing row, tampered conflict hash, promoted gate,
  vendor object without model scope, EDS default used as initialization, MINI row citing an EDS) and
  `scripts/generate_lichuan_ds2_objects_test.py` (altered output, nondeterminism) and
  `scripts/check_lichuan_ds2_operations_test.py` (unclassified DS2 function, a header claimed by two checkers or by
  none), each registered; `test/lichuan_ds2_objects_test.cpp` compiles the generated headers standalone in C++11 and
  links without catalogue strings.
- The install rule, package-test entries and per-header consumer compiles for the generated headers, in their commit.
- Stage report with the accounting totals per source and the conflict and gate list.

## Acceptance

Every EDS section and manual table row is accounted for, with page or line. Every DS-D with an object claim is a
conflict row; none is resolved by choosing one claim as fact. Generated files match `--check`; checker negative
tests fail as intended; a codec-only consumer links without catalogue strings; no commit ships a DS2 header that the
package gates do not name; Lichuan C checks still pass unchanged.
`python scripts/verify.py --mode quick` and `--mode full` pass, and required CI passes on the exact final pushed
commit. No device API, firmware support or hardware evidence is claimed. Update PROGRESS, coverage ledger keys,
the seam table and `docs/reports/lichuan_ds2_canopen_02.md`; commit and push.
