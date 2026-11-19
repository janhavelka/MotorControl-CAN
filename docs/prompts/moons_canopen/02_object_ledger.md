# 02 — Object ledger and generated tables

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 01's ledger evidence parts and checkers. All work is host-only.

One reviewed ledger holds both object layouts; generation emits separate, layout-typed tables. The ledger is
the single source for declarations, access policy and coverage. An EDS row is evidence of a declared object,
not of its units, range, effect or a safe write.

## Work

1. Parse EDS-STF, EDS-SSDC and EDS-TSM losslessly into `objects`: every section and sub-index with raw fields,
   EDS line range and the source hash recorded by 01. Each entry belongs to exactly one layout: UM21 entries
   carry the product scopes they apply to (`um21_stf_c_310a`, `um21_ssdc_c_310b`) with each product's raw EDS
   fields side by side where they differ; AMP10 entries carry `amp10_tsm_txm_c` only (MO-D02). Keys are
   layout-prefixed (`um21:0x6064:00`, `amp10:0x6064:00`) so the same number never resolves to one entry for
   both layouts.
2. Add the UM21 object tables (UM21 p40-p87) as a second source per entry: name, type, access, PDO and COS
   columns, unit text and page. Objects listed in UM21 but absent from an EDS, or present in an EDS but not in
   UM21, are recorded as such per product: 0x2008 homing inner offset (EDS only), 0x2032/0x2033 (EDS-SSDC
   only), 0x2206 (UM21 p72, absent from EDS-SSDC), 0x6065 (absent from EDS-STF) (MO-D08).
3. Record referenced-but-absent objects in `index_gaps` so no code can address them: 0x605D halt option code
   (referenced UM21 p22, p31, p33; MO-D06), 0x1012 TIME COB-ID (MO-D26), 0x1016 heartbeat consumer (MO-D11),
   0x100C/0x100D node guarding (listed AMP10 p15 only; MO-D12), 0x607D software position limits (review s7),
   0x608F-0x6097 factor group.
4. Review access per entry (`access_reviewed`, `review.read_effect`, `review.write_policy`, `review.gate_ids`).
   Keep every MO-D09 conflict side by side: 0x1010/0x1011 RO on p40, 0x1010 RW sub-indexes on p45, 0x1011 RO
   sub-indexes on p47 (stage 01's extension) and rw sub-indexes of both in the EDS; 0x1400 sub 1/2; 0x60FD RW/RO/rw; 0x200F RO (p65), WO (p67), ro (EDS); 0x607E RO/RW/rww; 0x200B and
   0x2070 UINT32 versus UINT16; 0x2100 UNSIGNED32 versus INT32; 0x2020 UINT16 versus UINT8; 0x2060 sub widths,
   sub 0 = 4 versus five sub-indexes, sub 2 RW versus ro; 0x60FF UINT32 (p31) versus INT32. Where widths
   conflict, a read accepts exactly the documented candidate sizes and retains the observed size as evidence;
   the write stays withheld until a fixture or vendor clarification names one. Conflicting access is withheld
   the same way. Where only signedness differs at one width (0x60FF; 0x609A, MO-D07), the write policy admits
   only values whose bytes and meaning agree under every candidate (0 to 2^31-1 for INT32 against UINT32) and
   withholds the rest; 0x2100's raw-word access (stage 10) writes the same four bytes under both candidates and
   publishes no signed value. Record each policy per product scope: at this revision 0x2020 and the 0x1400
   sub 1/2 writes are withheld on both products, the 0x2060 sub 1, 2 and 4 writes on STF-C (UINT8 in UM21 p38
   and p70, UINT16 in EDS-STF) and the sub 2 and sub 5 writes on SSDC-C (sub 2 ro and sub 5 UINT8 in
   EDS-SSDC), and 0x6087 on SSDC-C (INT8 WO on UM21 p33). The operations that need these writes consume the
   policy through the execution contract's write preflight; a read of one candidate width or a fake server
   resolves nothing.
5. Record EDS defects in `source_anomalies` without repairing the original: EDS-SSDC [1403sub2] ObjectType 0x9
   on an UNSIGNED8 entry (MO-D08). Extend `TYPE_CODES` only from these EDS files (STRING 0x0009 for 0x2050,
   0x2031, 0x100A; OCTET_STRING 0x000A for EDS-TSM 0x2100).
6. Mark side-effect objects (review section 11) with `effect` and a withheld write policy until their owning
   stage reviews them: 0x6060 (mode -1/-2 start Q programs), 0x2007, 0x2060, 0x200C, 0x2006, 0x2217 (write 0
   clears flags), 0x1003 sub 0 (write 0 clears history), 0x1010, 0x1011, 0x2020, 0x2021, 0x2604, 0x260D,
   0x2612, current objects, direction objects, limit definitions, enable/alarm-reset input functions, brake
   and output functions, homing offset mode and the step-servo gains.
7. Write `scripts/generate_moons_canopen_objects.py` (`--check` never writes). It emits `Types.h`, `Objects.h`
   with one table per layout and a distinct key type per layout, optional `Catalogue.h` with names and units,
   `Pdo.h` with the PDO-mappable entries per product, and `docs/reference/moons_canopen_generated.md`. Generate
   only the UM21 tables now; AMP10 entries stay ledgered with generation withheld until stage 13. Product
   differences (for example 0x6071 SSDC only, 0x2604 STF, 0x2207/0x2218 SSDC) are per-product applicability
   bits, never a merged row.
8. Seam S7, in or before the commit that adds the first generated header (canopen_shared 01 intake rules):
   extend the S7 claim stage 01 recorded before its checker registration (or another family's landed S7), then
   scope
   `scripts/check_lichuan_operations.py` to common and `lichuan_c` headers and the MOONS' checker to
   `profiles/moons_canopen/`, and test that the header sets are disjoint, their union is every public header
   with public functions, and an unclassified common, Lichuan or MOONS' function still fails. Add the generated
   headers to the `install(FILES ...)` rule and `test/package_test.py` (`HEADERS`, exact archive set);
   `library.json` and `check_repository.py` `EXPORTS` stay unchanged. Vendor files never enter `include/` or
   `src/`.

## Deliver

- The filled ledger (`objects`, `index_gaps`, `source_anomalies`, `semantic_review`, `accounting` covering every
  EDS section of all three files) and its checker extended to validate them.
- The generator, its generated outputs under `include/MotorControlCAN/profiles/moons_canopen/` and the generated
  Markdown, with `scripts/generate_moons_canopen_objects_test.py` covering missing row, altered output,
  entry in two layouts, promoted gate and a write policy on a withheld object.
- `test/moons_canopen_objects_test.cpp`: constants checked against values derived independently from the
  documented pages (for example 0x1018:01 = 0x000002D9 from UM21 p48, 0x6060 INT8 WO from UM21 p58), not
  against the generator's own output; a C++11 consumer that includes only the generated headers.
- The S7 claim, scoping and partition tests; install and package-test entries; `docs/reports/moons_canopen_02.md`.

## Acceptance

Accounting is lossless for all three EDS files; every UM21-listed object is matched or recorded as absent.
No entry spans both layouts and the AMP10 table is not generated. Every conflicting access or width keeps both
claims and a withheld write; a signedness-only conflict keeps both claims and admits only the common range.
`generate_moons_canopen_objects.py --check`, the ledger and operation checkers and
their negative tests, `python scripts/check_promptset.py --check-git-ref INDEX` (then `HEAD`),
`python scripts/verify.py --mode quick` and `--mode full` pass; CI passes on the exact pushed commit (new
public headers and checker scope). No device behavior is claimed. Hardware: NOT RUN.
