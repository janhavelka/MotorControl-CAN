# 02 — Object ledger, identity rows, generated tables and operation map

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 01; host-only work. Follow the canopen_shared 01
[intake rules](../canopen_shared/01_existing_code_and_tooling.md) ("Pack", "CANopen
ledger", "Operation map and tools", "Seam S7") for the schema and tooling; this stage
states what is specific to Nanotec. Its generated headers are the family's first
public headers, so seam S7 lands here.

## Work

1. Build `docs/reference/nanotec_canopen_objects.json`, schema 2, with the top-level
   keys and entry fields of `lichuan_objects.json`. The object universe per scope is
   the stored all-objects EDS for that model and firmware
   (`vendor/od_firmware_package/PLC_<model>_<build>_EDS.eds`). Use the N5 OD XML only
   for the N5 `private` flags; for other models the flag is unknown (NT-D27). Record
   the reduced N5 EDS as a second flavour, not as a second object universe.
2. Scopes: one per model and firmware build. Qualified candidates are N5-1-2, N5-2-2,
   C5-E-1-09, C5-E-2-09, CL4-E-1-12, CL4-E-2-12, PD4-C5918M4204-E-08 and
   PD4-CB59M024035-E-08 at FIR-v2213 B1031134. The FIR-v2538 B1077563 EDS load as
   unqualified scopes so their differences stay visible and feed stage 13's behavior
   gates (1018h:03 = 09EA0000h, new 2007h:02 "Automatic COB-ID", 2017h, renamed 3242h,
   20A0h/3390h and 4010h subentries; NT-D02). CL4-E -5VDI and the other PD4-C steppers
   have no stored EDS, so they have no scope: they stay documented and outside the
   scope until stage 13 stores their EDS (a model-scope question, not a firmware gate). The manual's statement that -5VDI changes only the input level (CL4E p16) is
   recorded for that stage; it does not replace the stored-EDS rule of item 1.
3. Review only documented objects. An entry becomes reviewed after its N5 (or row)
   object page is reopened: type, access, savable category, PDO mapping, preset,
   units and the firmware version line. Record EDS access separately from reviewed
   access (`rww`, `rwr`, `const`). Every entry starts with `write_policy` none and
   `read_effect` unreviewed; set `REVIEWED_NON_CHANGING` only with page evidence.
   Record that 1003h subindices abort with 08000024h while the stack is empty
   (N5 p163) and that strings (1008h, 100Ah, 4040h) need segmented upload or the
   per-character subindex form (N5 p123).
4. Per-model differences are ledger facts, not code branches by name: PD4-C lacks
   2059h and 3390h; CL4-E has 2102h and 1010h:0Bh (Modbus); PD4-C has 2006h, 2700h,
   4013h, 4015h and 4016h; C5-E has 2800h:03; N5 has 2010h-2013h and 1010h:0Ch. EDS
   defaults that differ (3202h 0 versus 1 or 41h, 60FEh:01 1 versus 0, 2030h, 6073h)
   are recorded and never written as initialization values.
5. One admission table, one line per qualified row, with exact model and suffix, the
   nameplate hardware revision range (N5 W007 and later, N5 p1; C5-E W006, C5E p1;
   CL4-E W002, CL4E p1; PD4-C W006, PD4C p1; record the N5-2-2 W007/W007b supply
   split of N5 p18 as a fact, not a gate), the stored EDS scope, the firmware build
   and the identity values (N5 p159, p167, p179; C5E p177; CL4E p202; PD4C
   p146, p166): 1018h:01 = 26Ch, 1018h:02 (N5-1-2 08h, N5-2-2 0Ah, C5-E 10h/11h,
   CL4-E 5Ah/5Bh/5Eh/5Fh), 1018h:03 = 08A50000h, 100Ah build string and 1000h
   (00060192h; PD4-C stepper 00040192h, PD4-CB 00020192h; take the PD4-C motor class
   from 1000h and the data table, PD4C p17 and p146, not the introduction, NT-D15).
   Keep both 100Ah strings
   FIR-v2213-B1029645 (manual) and B1031134 (EDS) visible (NT-D03); qualify a build
   only with its own evidence and state which. PD4-C lists two product codes per variant
   (NT-D14): keep both, mark which hardware reports which as unresolved, and verify
   that no code maps to two variants. Check the table for collisions inside the
   family and against every other implemented profile; a match never selects a
   profile (backlog D18).
6. Known unqualified identities: FIR-v1650 06720000h (N5V2 p149), FIR-v2538 09EA0000h
   and CL4-E FIR-v2315 090B0000h (IDX). Each gets a ledger row with its reason, the
   objects its evidence says are absent and stage 13's behavior gates, so that stage
   03 names it rather than reporting an unknown build. None of them refuses by name
   (execution contract, "No firmware lock").
7. Manual-only semantic functions (`manual_only`): CiA 402 state commands and their
   edge rules, mode activation, Plug & Drive commands (PDI pp5-6), NanoJ control,
   auto setup, save and restore categories, 1020h stamp, reboot 2800h, boot-loader
   boot-up, rotary-switch precedence, cfg.txt precedence, LSS services, Ethernet
   REST. Each with pages, conflict IDs, owner stage and the coverage columns.
8. Conflicts: NT-D01..NT-D31, NT-D32 from 01 and any later number, each with every
   original claim and source reference under a hash guard copied in mechanism (not
   content) from the Lichuan checker.
9. Seam S7, before or in the same commit as the first generated header. Claim S7 in
   the canopen_shared README table (or extend the landed split if another family
   claimed it first, adding no second scoping mechanism). Scope
   `check_lichuan_operations.py` to common and `lichuan_c` headers and
   `scripts/check_nanotec_canopen_operations.py` to
   `include/MotorControlCAN/profiles/nanotec_canopen/`, over
   `docs/reference/nanotec_canopen_operations.json` (schema 2 with Lichuan's keys:
   `families` with `header`, `functions`, `tests`, `console`; `ledger_bindings`;
   `python_plans`). The map references ledger keys and never copies device facts; it
   lists any generated header with public functions, and 02b adds the first API
   family. Test that the checkers' header sets are disjoint, that their union is every
   public header with public functions, and that an unclassified common, Lichuan or
   Nanotec function still fails.
10. Package entries in the commit that adds the generated headers (canopen_shared 01
    intake rule "Seam S7", 02 "Multi-profile consumers and package scope"): an
    `install(FILES ...)` rule for `profiles/nanotec_canopen/`, the headers (and any new
    `src/` file, also in `MOTORCONTROLCAN_SOURCES`) in `test/package_test.py` `HEADERS`
    and the exact archive set, the per-header consumer compile (`headers_11`,
    `headers_17`) and C++11 consumers of core only, Lichuan only, Nanotec only and both.
    `library.json` and `check_repository.py` `EXPORTS` stay unchanged. 02b extends these
    entries with `Reads.h` and its source.

## Deliver

- The ledger; `scripts/check_nanotec_canopen_ledger.py` (lossless EDS accounting per
  scope, reviewed entries cite a page, no promoted gate, conflict hash guard, identity
  table consistency and collision check) and its registered negative test.
- `scripts/generate_nanotec_canopen_objects.py` with `--check` that never writes,
  generating `Types.h`, `Objects.h`, `Pdo.h` and an optional `Catalogue.h` in
  `include/MotorControlCAN/profiles/nanotec_canopen/` and
  `docs/reference/nanotec_canopen_generated.md`; its negative test (altered output,
  missing row, tampered hash). Extend `TYPE_CODES` only from the family EDS.
- LF rules in `.gitattributes` for the generated headers; checker registration in
  `scripts/verify.py`, `scripts/check_repository.py` (`CHECKS`, file lists) and CTest.
- Seam S7: the operation map, `scripts/check_nanotec_canopen_operations.py` and its
  registered negative test (unclassified function, header claimed twice, missing
  test path, promoted gate), the scoped Lichuan checker with its regression, and the
  S7 claim and landed row (commit, tested interface, consumer gate) in the
  canopen_shared README. Tooling may be committed before the ledger and headers only
  where every checker it registers has its inputs and passes at that commit; the
  headers' commit carries item 10's package entries, so no commit ships a header the
  package gates do not name.
- Review of the `SdoReuse` marker objects for Nanotec (1000h:00, 1018h:01-03): their
  read effects from the ledger, handed to 02b for seam S10. Until that review exists
  the Nanotec policy is `NONE`.

## Acceptance

Every EDS object of every admitted scope is accounted for; reviewed entries cite
pages; no entry has a write policy without a stage that owns it. The generator is
deterministic, catalogue strings stay optional, and codec-only consumers link
without them. Identity rows reproduce the documented values exactly, each qualified
line names its hardware revision range and EDS scope, and the unqualified identities
with their gates and the EDS-less models are named. The S7 tests pass, each checker's negative test fails as
intended, and `test/package_test.py`, the standalone-header compile and the C++11
consumers pass at the headers' commit. Run the family checks, `python scripts/check_promptset.py
--check-git-ref INDEX` before the commit and `HEAD` after, quick verification and
CI on the exact final pushed commit. No device API or hardware claim follows.
