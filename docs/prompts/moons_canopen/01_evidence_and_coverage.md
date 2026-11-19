# 01 — Evidence, scope and coverage ledger

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

The pack already exists; do not redownload unchanged originals or treat the review as an implemented profile.
This stage fixes scope, records every conflict and builds the coverage denominator that later stages close.
No device API, firmware or hardware claim comes from it.

## Work

1. Record HEAD, dirty state and the family seam state. Execute the
   [canopen_shared 01 inventory](../canopen_shared/01_existing_code_and_tooling.md) for this family: every row
   confirmed, changed or absent at HEAD, classified (reuse, extend seam, replace, not applicable), and the seams
   MOONS' needs (S1, S2, S3, S6, S7, S10, S13 from 09; S8 only if evidenced; S4/S5/S9 from their prompts; S11
   and S12 not needed: no heartbeat consumer, no node guarding) with each seam's current claim in the
   [seam table](../canopen_shared/README.md). Apply that 01's intake rules to every MOONS' stage that adds
   sources, a ledger, generated output or the first public header.
   Record PROGRESS rows 51, 57, 63, 64, 65, 73, 75 and 77, and the MotorControl-RS row under AGENTS' audit.
2. Verify the pack: `python scripts/check_promptset.py` (the manifest is globbed into `reference_manifests`),
   then reopen the pages behind each scope claim: UM21 p6 (models, firmware 310A/310B/1.02A), STFHW p30 and
   SSDCHW p3 (suffix -C), SSDCHW p35 (no SSDC05-C, MO-D01), TSM34 p16 and TSM24 p2-p3 (C-xCG part numbers),
   PG-STF05FC and PG-SSDC06FC (no UM21 or EDS link), PG-STF03MINI (RS-232 only, MO-D15), PG-MDX and PG-M56S.
   Record MO-D22 (SSDCHW p36 "CoE", p34 ECAT/STO LEDs) as not applicable to -C behavior.
3. Correct the review where it is wrong, with the old claim, the evidence and the reason, in one checked change.
   The promptset author found these pack defects while writing this set and left the pack unedited; this stage
   reopens each cited page or EDS section and applies them to the pack review itself:
   - Review section 5.1 cites "MO-D34" for the enable-input default; no MO-D34 exists. The conflict
     (0x2617/0x220D default 3 in UM21 p75/p87 against EDS 0) belongs to MO-D24.
   - Add **MO-D31**: UM21 p18 says statusword bit 12 = 1 means "previous set-point still in process, set-point
     overwriting shall be accepted", while UM21 p21 says bit 12 = 1 means the two-deep buffer is full and
     another set-point is ignored. Owner: stage 05.
   - Add **MO-D32**: EDS-TSM [7009] velocity actual value is UNSIGNED16 (DataType 0x0006) although velocity is
     signed in every other object; direction cannot be read from it as declared. Owner: stage 13.
   - Add **MO-D33**: EDS-STF declares 0x6078 current actual value, which UM21 p60 says exists on step-servo
     drives only; EDS-STF, EDS-SSDC and UM21 declare no 0x605B, 0x605C or 0x605E, so shutdown, disable
     operation and fault reactions are undocumented. Owner: stage 04 (03 for the 0x6078 read).
   - Extend **MO-D09**: its "0x1010/0x1011 shown RO (UM21 p40) but RW subs (p45, EDS)" holds for 0x1010 only.
     UM21 p47 lists 0x1011 sub 1 (restore all) and sub 2 (restore communication) as RO, while EDS-STF,
     EDS-SSDC and EDS-TSM give both rw. Owner: stage 02 (access review), stage 11 (restore write gate).
   Further discrepancies found by later stages are added the same way with the next free ID.
4. Fix the profile identity: family `moons_canopen`, namespace `MotorControlCAN::MOONS_CANOPEN`, product rows
   `STF_C`, `SSDC_C`, `TSM_TXM_C`, and object layouts `UM21` (STF-C, SSDC-C) and `AMP10` (TSM/TXM-C). Record the
   firmware rows: STF-C 310A and later, SSDC-C 310B and later; any other string is unqualified (MO-D01).
5. Create `docs/reference/moons_canopen_objects.json` (schema 2, canopen_shared 02 "Object and operation
   ledger") with its evidence parts now: `sources` bound to the pack manifest hashes, `scopes`
   (`um21_stf_c_310a`, `um21_ssdc_c_310b`, `amp10_tsm_txm_c`), `conflicts` for MO-D01..MO-D33 with every original
   claim and its page or EDS line under a hash guard, and `manual_only` for prose-only functions. Stage 02
   adds the `objects` entries. Prose-only functions include at least: two-deep set-point buffering (UM21 p21),
   halt per mode (UM21 p21, p31; AMP10 p26, p45), RS-232 power-up hold in Initialization (UM21 p9), Q program
   start and SYNC Q start (UM21 p35-p36), watchdog arming and re-arming (UM21 p38), node-ID composition from
   switch and 0x2020 (UM21 p9, p68), bitrate switch codes (UM21 p9), homing offset modes (UM21 p66),
   edge-triggered output mask (UM21 p63) and the store/restore groups (UM21 p45-p47).
6. Create `docs/reference/moons_canopen_operations.json` (schema 2, Lichuan keys, `object_ledger` pointing at
   the file above) with no function families yet, and `docs/moons_canopen_coverage.md` with one row per common
   operation of canopen_shared 02 (reads, enable, release, mode select, absolute, relative and angle move,
   finite velocity, homing, each stop kind, fault clear, save, restore) plus the vendor rows: profile torque,
   Q segment start, SYNC Q, zero position (0x200C), DSP clear alarm (0x2006), watchdog. Each row names its
   owning stage, the ledger keys it will use and its MO-D gates; none is marked implemented.
7. Write `scripts/check_moons_canopen_ledger.py` (sources, scopes, conflicts, manual items; every entry in exactly
   one layout) and `scripts/check_moons_canopen_operations.py` (family header scan of
   `include/MotorControlCAN/profiles/moons_canopen/`, bindings to ledger keys, tests, console rows), each with a
   registered negative test: missing row, tampered hash, entry in two layouts, unclassified function, promoted
   gate. Register both in `scripts/verify.py`, `scripts/check_repository.py` (`CHECKS`, file lists) and CTest.
8. Decide the first implementable subset from the evidence: STF-C UM21 reads, enable/release, profile position
   with native units, profile velocity, quick stop with read-back option code, and the probe. Everything that
   depends on an open MO-D fact is named with its gate. Missing hardware blocks no software here.

## Deliver

- The corrected review entries (MO-D24 reference, MO-D09 0x1011 extension, MO-D31, MO-D32, MO-D33) with their
  evidence, applied in the pack under the intake rules (working tree, INDEX and HEAD checks).
- `docs/reference/moons_canopen_objects.json` (evidence parts), `docs/reference/moons_canopen_operations.json`,
  `docs/moons_canopen_coverage.md` linked from `docs/coverage.md` and `docs/profile_contract.md`.
- The two checkers with negative tests, registered once; `.gitattributes` LF rules for future generated headers.
- `docs/prompts/moons_canopen/PROGRESS.md` with every stage and block (01, 02, 02b, 03-07, 08a-08c, 09-13, 14a,
  14), software state and a separate hardware column; `docs/reports/moons_canopen_01.md`.

## Acceptance

Every documented native function in the review sections 3-11 has a row with page source, owning stage and
gate; unresolved rows never become writable. All 33 MO-D items are in `conflicts` with an owner stage. The
checkers fail their negative tests as intended and pass on the real files. `python scripts/check_promptset.py`,
`--check-git-ref INDEX` before the commit and `HEAD` after, `git diff --check` and `python scripts/verify.py
--mode quick` pass. The checker registration is a build/check change: verify CI on the exact pushed commit.
Hardware: NOT RUN (no fixture). Update backlog and roadmap; the lead commits and pushes.
