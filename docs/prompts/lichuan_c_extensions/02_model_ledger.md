# 02 — Model-scoped rows on the LICHUAN_C ledger

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

The extensions become rows of the one reviewed ledger, [lichuan_objects.json](../../reference/lichuan_objects.json)
(schema 2), and of [lichuan_operations.json](../../reference/lichuan_operations.json). Nothing is copied: a
TLEDS section equal to the C-EDS section is recorded as a second declaration of the existing row, and only a
difference becomes a model-scoped row. CL2 has no EDS, so all its rows come from its manuals. The existing
C-family claims, conflicts C01..C24, gates Q01..Q28 and generated descriptor values stay unchanged.

## Work

1. Sources. `check_promptset.py` already discovers the LX pack manifest by glob and hashes its originals; do
   not edit `reference_manifests`. The ledger's own source check (`check_lichuan_ledger.py`, which requires
   `source_manifest` = `docs/reference/SOURCES.json`) admits the LX manifest as a second ledger manifest (for
   example an `extension_manifests` key), verifies each LX source used (EN57, TLCEN, TLEDS, TL42ZH..TL86ZH,
   TL42OP..TL86OP, CL257, CL286, ABZ57, ABZ86, PG-* where cited) against
   [LX SOURCES.json](../../reference/lichuan_c_extensions/SOURCES.json) hashes, and keeps the six existing
   source rows and their checks byte-for-byte.
2. Scopes. Append `eds_tl_c` (TLEDS; TLC/TLO 42..86-C; binding unverified), `manual_tl_c` (TLCEN, TLxxZH,
   TLxxOP), `manual_cl2` (CL257, CL286), `manual_cl2_abz` (ABZ57, ABZ86) and `manual_57_en` (EN57; CL57-C,
   OL57-C). Append matching `SourceScope` values in generated `Types.h`; `EDS_FAMILY_UNBOUND` keeps value 0.
3. TLEDS accounting, lossless. Parse TLEDS and classify every section and key: equal to the C-EDS section after
   the normalization the [delta](../../reference/lichuan_c_extensions/extracted/TL_CAN_V1.0_vs_LC_StepDriverCAN_V1.1.delta.txt)
   uses (case, number format), recorded as `also_declared_by` with the TLEDS line on the existing row; or a
   TL-C row. Known differences: `1018` count 4 with sub04 (TLEDS l.75, l.107), `2402` `Obj.BaudSet` default 40
   (l.1569), `2403..2407` `Obj.RSVD` (l.1577..l.1609), `2301` present (l.1404), `[DeviceInfo] CompactPDO=31`
   (l.30), no `[DynamicChannels]`, `ProductName=TL_CAN` (l.15). The checker fails on any unaccounted section,
   key or line, and accounting counts are kept per EDS.
4. CL2 rows. Record the CL2 dictionary from CL257 p14..19, with the P-parameter and Modbus-address tables of
   p8..10, as model-scoped `manual_only` rows: index, subindex, P-parameter, type, access, printed range,
   default where printed, source page. Record CL286, ABZ57 and ABZ86 differences found by the
   page comparison (`2502` bounds; encoder faults Err7..Err10 absent in ABZ p20; motor-matching section absent
   in ABZ) with their pages. Absent objects stay absent: no CL2 row is inferred from C-EDS.
5. Model applicability. Every object, entry and manual row gets `models`: the documentary designations it
   documents (`CL86-C`, `TLC57-C`, `CL2-57C`, `CL57-C`, `OL57-C`, ...), each with an optional mapping to a
   runtime `SettingsModel` value, and a per-designation `write_policy`. A designation without a runtime value
   (CL57-C and OL57-C, stage 05) keeps its documentary coverage and admits nothing. A model not listed for a
   row gets NOT_DOCUMENTED and refuses every write; a C-EDS row never becomes a TL-C or CL2 row by the absence
   of contrary text. `coverage` keeps `documented`, `implemented`, `host_tested` and `hardware_qualified`
   independently per designation.
6. Conflicts. Add LX conflicts with the pack ID as `id`, each original claim with its `source_ref`: LX-D03,
   D04, D07, D08, D10, D11, D12, D13, D21, D22, D23, D24, D25, D26, D27, D28, D34. Discrepancies that are
   identity facts, absences, contrasts, typos or ratings (LX-D01, D02, D05, D06, D09, D14, D20, D29..D33, D35)
   become `source_anomalies` or row remarks with their ID. Keep `ORIGINAL_CONFLICT_CLAIMS_SHA256` and C01..C24
   untouched; add a second constant hashing the LX conflict claims. The EN57 discrepancies LX-D15..D19 belong
   to stage 05, which links them to C01/C02/C17 and Q02 the same way.
7. Model gates. Add an X series to `semantic_review.gates`, each with feature, required evidence, models, owning
   stage and status `open`. Do not append the new models to Q01..Q28: their recorded evidence is CL86-C
   specimen evidence. Proposed set (renumber only before landing):

| Gate | Feature | Models | Discrepancies | Owner |
| --- | --- | --- | --- | --- |
| X01 | Identity record per model; distinguishing fields (stage 01 intake correction for LX-D02) | All new | LX-D01, D02, D35 | 01, 07a |
| X02 | TL-C bitrate selector object, codes and activation | TL-C | LX-D03, D13 | 03a, 07a |
| X03 | TL-C node selection: DIP custom against `2203` | TL-C | LX-D12 | 03a, 07a |
| X04 | TL-C status table and freshness | TL-C | LX-D06 | 03a, 07a |
| X05 | TL-C enable, start words and velocity start order | TL-C | LX-D07, D08 | 03b, 07a |
| X06 | TL-C terminal layout: inputs, outputs, `2400`, `60FE` | TL-C | LX-D04, D10 | 03a, 03b |
| X07 | TL-C alarm reset route (`2301` EDS-only) | TL-C | LX-D11 | 03b |
| X08 | CL2 statusword bit 10 per mode; absent bits | CL2 | LX-D23 | 04a, 07a |
| X09 | CL2 communication objects, identity, PDO and heartbeat | CL2 | LX-D20 | 04a, 07a |
| X10 | CL2 loop mode and encoder resolution | CL2 | LX-D21, D22, D29, D34 | 04a |
| X11 | CL2 homing method table and Z source | CL2 | LX-D26, D27 | 04b, 07a |
| X12 | CL2 I/O codes, `2100` and `2102` meanings | CL2 | LX-D24, D25, D28 | 04a, 04b |
| X13 | CL2 faults and reset classes | CL2 | LX-D30 | 04a |
| X14 | Ratings and current bounds of the fixture model | All new | LX-D09, D31 | 03b, 04b, 07a |
| X15 | Enable, release and stop source states: predecessors other than 15, stop effect and completion | All new | None; DEC-2 and DEC-8 do not transfer | 03b, 04b, 07b |
| X16 | Move and velocity start, acceptance and completion signals, relative basis and command scale | All new | LX-D23 (CL2 bit 10) | 03b, 04b, 07b |
| X17 | TL-C homing per method: trajectory, signals and bit 8 completion | TL-C | None; C-family `HomingMethod` evidence does not transfer | 03b, 07b |
| X18 | Save and factory restore: persistent set and route after restart | All new | LX-D03, D12, D13, D32 | 03b, 04b, 07b |

8. Generation. `generate_lichuan_objects.py` emits a per-model applicability table (a generated header, for
   example `Models.h`, or rows in `Objects.h`) giving, per runtime model and entry, documented or not, write
   policy and X-gate bits; only rows mapped to a runtime model contribute. Existing `ObjectDescriptor` layout, `gateMask` bit meanings (Q01 bit 0 through Q21 bit 20) and
   values stay unchanged. Extend `lichuan_generated.md` with model sections and `.gitattributes` LF rules for
   a new generated header. Catalogue strings stay optional; codec-only consumers link no descriptions.
9. Operation map. Add model columns to `lichuan_operations.json` bindings so the operation checker proves that
   each public function's admitted models equal its ledger rows. Add a disposition map, total over the common
   operations of canopen_shared 02's coverage list and the runtime models, before any operation stage: each
   cell is ADMITTED (its row), GATED (an X or Q gate and its evidence owner), NOT_DOCUMENTED,
   NOT_IMPLEMENTED (with the missing dependency, such as prompt 57) or MISSING_PREREQUISITE (the caller record
   it needs); proposed names. This stage lands every new-model cell as something other than ADMITTED;
   03b and 04b change their cells. Refusal details, console text (06) and coverage come from this one map,
   and a documented entry alone never makes a cell ADMITTED.

## Deliver

Updated ledger, operation map, `check_lichuan_ledger.py`, `generate_lichuan_objects.py` (`--check` never
writes) and `check_lichuan_operations.py`; regenerated `Types.h`, `Objects.h`, `Catalogue.h`, `Pdo.h`, the
model table and `lichuan_generated.md`; negative tests for a missing TLEDS section, a tampered LX hash, a model
row without a source, a promoted X gate, a TL-C row inheriting a C-EDS default, an edited original C claim,
a CL2 row citing an EDS line, a model admitted by code but not by the ledger, a disposition map missing a
cell, and an EN57 row naming CL57-C and OL57-C that counts as documentary coverage while generating no
runtime selector and no write admission. Report and `PROGRESS.md`.

## Acceptance

Every TLEDS section and key is accounted; every new-model row cites a page or line; no new-model write is
admitted by generation. Original C claims and descriptors are byte-identical; `check_promptset.py` (with
`--check-git-ref INDEX`, then `HEAD`), the three Lichuan checks, quick and full verification and exact-commit CI
pass (generated-table change). No implementation or hardware claim follows; all X gates are open.
