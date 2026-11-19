# 01 — Evidence and coverage

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. The reference pack exists; do not redownload unchanged
sources or treat transcription as an implemented profile. This stage changes
documentation only (canopen_shared 01: no source change). No hardware is needed.

## Work

1. Run [canopen_shared 01](../canopen_shared/01_existing_code_and_tooling.md) for this
   family: record HEAD, dirty state and PROGRESS rows 57, 63, 64, 65, 73, 75 and 77;
   open every listed symbol with its callers and mark it confirmed, changed or absent.
   Classify each row for Nanotec. Expected needs, to confirm against the current
   seam table claims and Git log (this family may land them first): S7 with the first
   header's package entries in 02; S1, S2 (with S3's default deny in the same commit),
   S6 and S10 in 02b; the Nanotec S3 shape and S8 in 04; S13 in 09; S11 in 09b; S14
   only if 06c is assigned. S12 is not needed (a
   heartbeat consumer is documented; guarding stays BLOCKED, 09b). S4 and S5 are
   consumed from prompts 64 and 65. S9 is not needed (same CANopen domain as
   Lichuan; a shared interface needs stage 11's same-domain analysis). Record the
   protocol choice from family evidence: CiA 301 4.2.0 and CiA 402 3.0, 11-bit identifiers only,
   little-endian data (N5 p117; C5E p115; CL4E p113; PD4C p104).
2. Check the pack: `python scripts/check_promptset.py` validates the 45 source
   entries and 9 derived artifacts by hash. Confirm that the N5 and C5-E manuals
   resolve to the multidrive pack and that `remote_only` items (zip, C5-E 2.0.1,
   PD4-C 2.0.2, PD6-E 1.2.0, newer N5 manual, firmware notes) carry no path or hash.
3. Reopen the original pages behind NT-D01..NT-D31 (render tables and bit diagrams;
   the review names the pages) and record per item: confirmed, narrowed or
   corrected, its model/firmware scope and the stage that must resolve it. Open
   items map as follows: NT-D01/D02/D03/D14/D27/D29 to 02 and 03; D13/D26 to 04;
   D19/D18 to 05 and 07; D20 to 06; D11 to 07; D12/D22/D25 to 08; D07/D09/D16/D30 to
   09; D08/D17/D24 to 09b; D04/D05/D06/D21/D23/D28 to 10; D10 to 05; D15 to 02;
   D31 to 13; D32 (item 4) to 04b and 10, and to 13 for FIR-v1650. A disposition
   that changes a claim is evidence work, not an edit of the review.
4. Record **NT-D32**, found during preparation and confirmed on the original pages
   (2026-11-19): the 2300h object page says "When switching on, a check is performed
   to determine whether a NanoJ program is present. If present, '1' is entered in 2300
   and the NanoJ program is started" (N5 p283; the same sentence on C5E p274, CL4E
   p300 and PD4C p265), without saying whether "switching on" means power-on or
   setting bit 0. FWUPD p6 (changes from FIR-v1650 to FIR-v2039) says that controllers
   with a USB interface no longer start the program automatically at each restart
   and need the cfg.txt line `2300:00=1`. 2300h is also savable (application, N5 p283).
   Enter both claims in the ledger `conflicts` (02) and a dated note in this report;
   the review is not rewritten. Record which admitted models count as "with USB
   interface" and leave autostart unknown per model until device or vendor evidence.
5. Decide and record, against the current checkout: the profile token `<family>`
   (proposed `nanotec_canopen`, the same as the set name; canopen_shared 02 "Names and
   layout"), the namespace and enum spellings in the contract, scope IDs
   for model-by-firmware rows (for example `n5_fir2213_b1031134`), and the first
   implementable subset (identity and reads, state machine and stops, PP finite
   position, PV finite velocity, homing method subset). Everything else stays in
   the denominator with its gate.
6. Seam S7 does not land here, because this stage adds no public header. It lands
   with the first one, stage 02's generated headers (canopen_shared 01 intake rule
   "Seam S7"). Record its current claim state for 02.

## Deliver

- `docs/nanotec_canopen_coverage.md`: one row per common operation (reads, enable,
  release, mode select, absolute, relative and angle move, finite velocity, homing,
  each stop kind, fault reset, save, restore) and one row per documented native
  function (Plug & Drive control, NanoJ control, auto setup, VL, PT, CSP, CSV, CST,
  IP, clock-direction, LSS services, reboot 2800h, category saves and restores,
  heartbeat consumer, node guarding, PDO configuration, SYNC, EMCY, I/O, brake,
  limits) with documented, implemented, host-tested and hardware-qualified columns,
  the owning stage and the gate or reason. Link it from `docs/coverage.md`.
- `docs/prompts/nanotec_canopen/PROGRESS.md` with every stage and block (01, 02,
  02b, 03, 04, 04b, 05, 06a-06c, 07, 08a-08d, 09, 09b, 10, 11, 12A, 12B, 13):
  software status (NOT STARTED, IN PROGRESS, SOFTWARE COMPLETE, PARTIAL or BLOCKED),
  hardware evidence column, inherited gates and report link.
- `docs/reports/nanotec_canopen_01.md` with the inventory record, RS and FieldCore
  rows, NT-D dispositions (NT-D32 included) and the decisions of item 5.

## Acceptance

Every documented native function has a coverage row with page evidence and an
owner; unresolved rows are not writable because the EDS says `rw`. NT-D32 has a
recorded scope or an explicit unknown per model. `python scripts/check_promptset.py`,
`python scripts/check_repository.py` and `git diff --check` pass; the stage changes
documentation only, so no CI run is dispatched and the report says so, while
required automatic checks still apply.
No profile API, firmware support or physical evidence is claimed. Update backlog
and roadmap entries for the Nanotec profile, commit and push the owned changes,
and write the handoff.
