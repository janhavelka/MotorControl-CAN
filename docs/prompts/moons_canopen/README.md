# MOONS' / Applied Motion CANopen profile promptset (STF-C, SSDC-C, TSM/TXM-C)

Prepared 2026-11-19; revised the same day after an independent [audit](audit.md)
([application](audit_application.md)) and the shared contract's round 2 (`1043608b`). Source collection and
prompt preparation complete. All implementation stages are NOT STARTED; MOONS' CANopen hardware is NOT TESTED.

Add the Applied Motion Products / MOONS' CANopen drives as one independent concrete profile family,
`moons_canopen`, inside MotorControl-CAN. It reuses the CANopen services, owner and common motion vocabulary
and plugs in through the [shared integration contract](../canopen_shared/02_profile_integration_contract.md).
Every numbered stage runs under the [execution contract](execution_contract.md). The evidence is the
[MOONS' reference pack](../../reference/moons_canopen/README.md) and its [review](../../reference/moons_canopen/review.md)
with discrepancies MO-D01..MO-D30 (stage 01 corrects the review and adds MO-D31..MO-D33); generic CANopen facts come from the
[CANopen principles pack](../../reference/canopen_principles/principles.md). Dispatch one stage at a time and
carry its handoff into the next. Creating this promptset does not execute it.

## Scope

| Row | Models and firmware | Object layout and evidence | Role |
| --- | --- | --- | --- |
| STF-C | STF05-C and STF10-C first; STF03-C and STF06-C as rows of the same manual; firmware 310A and later (UM21 p6) | 2021 layout: UM21 with EDS-STF (V310A, 06-02-2020) | Primary target. Open-loop two-phase stepper (STFHW p3, p23); modes 1, 3, 6, -1, -2 |
| SSDC-C | SSDC03-C, SSDC06-C, SSDC10-C; firmware 310B and later (UM21 p6). SSDC05-C is named in UM21 p6 but not in SSDCHW p35 (MO-D01) | 2021 layout: UM21 with EDS-SSDC (V310B, 06-01-2020) | A row of the STF layout. Closed-loop StepSERVO stepper with encoder; adds profile torque (mode 4) |
| TSM/TXM-C | TSM17C, TSM23C, TSM24C, TSM34C, TXM24C, TXM34C (C-xCG part numbers) | 2010 layout: EDS-TSM (05-29-2020) and AMP10 Rev F; no current manual (MO-D30) | A separate, identity-gated layout table (stage 13). Vendor objects 0x7000/0x4000/0x5000, units 1/240 rev/s and 1/6 rev/s² (inferred, stage 13), 16-bit effective values, index 0x2100 with another meaning |

Excluded until their own evidence exists: the -FC (UL type) variants, whose pages link neither UM21 nor an EDS
(PG-STF05FC, PG-SSDC06FC); STF03-C-mini (MO-D15); the -R, -D, -IP, -ECX and -PN interfaces; MDX+, M56S and
SV200, which have separate manuals; the 2010-generation ST5/10-C, STM-C, STAC6-C and SV7-C drives that AMP10
describes. SCL over CANopen is not documented by any source (UM21, AMP10, HCR) and is not offered. RS-232
configuration tools (Stepper Suite, STF Configurator, Quick Tuner, Q Programmer) are never executed.
The two layouts are never merged: equal object numbers do not mean equal objects (review section 12).

## Stages

| Stage | Deliverable | Dependencies and gates |
| --- | --- | --- |
| 01 | [Evidence, scope and coverage ledger](01_evidence_and_coverage.md) | Preserved pack; canopen_shared 01 inventory; no hardware |
| 02 | [Object ledger and generated tables](02_object_ledger.md) | 01; two layout scopes, reviewed access policy, seam S7; MO-D02/D08/D09 |
| 02b | [Profile registration, probe and console family](02b_profile_registration.md) | 02; seams S1, S2, S3, S6, S10; console Blocked until S4 (prompt 64) |
| 03 | [Identity, layout selection, typed reads and capabilities](03_identity_and_reads.md) | 02b; no universal probe; MO-D01/D27 |
| 04 | [State machine, enable, release, fault reset and stops](04_state_machine_and_stops.md) | 02b-03; MO-D05/D06/D19/D24/D28/D33; prompt 57 for common enable/release names |
| 05 | [Units and finite profile position](05_units_and_position.md) | 03-04; MO-D03/D20/D31; two-deep set-point buffer |
| 06 | [Profile velocity and profile torque](06_velocity_and_torque.md) | 04-05; torque SSDC only, refused by the 0x6087 write preflight until MO-D04 resolves |
| 07 | [Homing, coordinate changes and limits](07_homing_and_limits.md) | 03-05; MO-D07; prompt 77 for common limit facts |
| 08 | [I/O and drive settings, blocks 08a..08c](08_io_and_settings.md) | 03; 08b needs 07 limits; MO-D21/D24/D25 |
| 09 | [PDO, SYNC, EMCY and network events](09_pdo_sync_emcy.md) | 03-04; MO-D10/D12/D13/D14/D19/D26; seam S13 claimed and landed here unless another family holds it |
| 10 | [Q program modes -1 and -2](10_q_programs.md) | 04, 08's settings context, 09; mode -2 refused in this set; MO-D18 segments, MO-D20 register mapping |
| 11 | [Commissioning, communication, watchdog and persistence](11_commissioning_and_persistence.md) | 03-04, 08, 09, 10; MO-D11/D16/D17/D18/D23 |
| 12 | [Console and mixed-profile integration](12_console_and_integration.md) | Implemented subsets of 02b-11; prompts 64 and 65 when landed |
| 13 | [TSM/TXM-C 2010 object layout](13_tsm_txm_layout.md) | 02-12 contexts; MO-D02/D29/D30/D32; Part B needs a current TSM/TXM manual |
| 14a | [First-contact questions](14a_first_contact.md) | An identified MOONS' fixture and authorization; owner present |
| 14 | [Physical qualification and release reconciliation](14_qualification_and_release.md) | Software closeout any time; hardware only with 14a fixture |

The list follows the canopen_shared shape with three changes. Profile torque joins velocity in 06 because UM21
documents both with the same halt start/stop pattern (UM21 p32, p34). Q program modes get their own stage 10:
starting a stored program is opaque vendor motion, and mode -2 turns every SYNC on the bus into a motion trigger
(UM21 p36; AMP10 p48-p49), so stage 10 refuses mode -2 and stage 09's S13 entry refuses SYNC while any MOONS'
node may be in it. A positive SYNC Q start would need its own shared-contract change. The 2010 layout gets
stage 13 because its
objects, units and widths differ and its current manual is missing (MO-D02, MO-D30); it must not delay 12.
Dependencies are semantic prerequisites, not a demand to guess missing behavior. A stage may finish an
independently usable subset while named operations stay gated.

## Prepared artifacts

- The reference pack: 10 manufacturer PDFs, 3 EDS files and 10 product-page snapshots with SHA-256 in
  [SOURCES.json](../../reference/moons_canopen/SOURCES.json), ten page-marked text extracts, and the review with
  MO-D01..MO-D30. Blocked or unstored sources (AMP10 Rev K, applied-motion.com listings) are `remote_only`.
- The [CANopen principles pack](../../reference/canopen_principles/README.md) (committed at `e02ab04f`; its
  Kollmorgen AKD manual is remote-only) for CiA 301/402 facts and the points where vendors differ
  (CP-D01..CP-D28). It is secondary evidence and never establishes a MOONS' object, capability or sequence.
- No ledger, generated header, profile code, console route or test exists for this family yet.

These are research inputs, not an implementation allowlist. Do not reuse Lichuan objects, decisions (DEC-n),
gates (Qn), bench states or console verbs ([canopen_shared 01](../canopen_shared/01_existing_code_and_tooling.md)).

## Preparation verification

`python scripts/check_promptset.py` checks the links and table shapes of this set and the pack's manifest
(`reference_manifests` already globs `docs/reference/*/SOURCES.json`). `git diff --check` checks whitespace.
Neither compiles anything. No product source, firmware, CI run or hardware step belongs to this preparation.

## Relation to the main sequence and the shared seams

The family is independent of the main-sequence prompts except where a seam is owned there. Following the
[claim, land, consume rules](../canopen_shared/README.md), MOONS' may claim and land any first-family seam that
no other family has claimed: S1 inventory entry, S2 profile resolution on a binding, S3 per-binding
best-effort stop shape (MOONS' starts with none), S6 console family and JSONL profile, S7 per-family checkers
(claimed before stage 01's checker registration, completed with the first public header in stage 02), S10
reviewed SDO reuse policy (MOONS' starts with `NONE`) and S13
SYNC side-effect admission (stage 09). A claimed seam is extended or waited for, never rebuilt. S8 (shared
CiA 402 helpers) is extracted only when MOONS' and the other caller need identical evidenced semantics. S4
(prompt 64 common axis table), S5 (prompt 65 package components) and S9 (prompt 63 second protocol) are
consumed only from their prompts. Until S4 lands, every MOONS' console entry, common-verb dispatch and API/CLI
parity test is Blocked; the API-level routes, owner tests and checkers proceed. S11 (host heartbeat producer)
does not help MOONS': no MOONS' document declares a heartbeat consumer 0x1016 (MO-D11), so a host heartbeat
gives the drive nothing to supervise; its only documented host-loss reaction is the 0x2060 watchdog fed by
RPDOs or SYNC (UM21 p38, p70; stage 11). S12 (RTR node guarding) is not needed: UM21 and all three EDS files
declare no 0x100C/0x100D (listed only in AMP10 p15, MO-D12), and UM21 p53 marks RTR-only PDO types unsupported.

Main-sequence contracts this family consumes: prompt 51 SYNC producer (stage 09), 57 regular enable, release,
mode select and firmware identity (04), 73 timeouts and freshness guards (04-06), 75 runtime node table (12),
77 switch-ready limits and homing (07). The declared binding selects the profile and product row; product
identity only confirms it (MULTIDRIVE row 57, backlog D18). Seam S13 is the shared form of the SYNC hazard this
family raised: a SYNC is a broadcast, and a MOONS' node in mode -2 starts a Q program on each one, so before
the owner produces SYNC on an interface every bound device's reviewed SYNC effects must be known. S13 gates
prompt 51's producer without forking it and is the SYNC part of canopen_shared 02's shared-interface analysis.
Stage 09 claims it in the seam table if it is still unclaimed, or extends or waits for the claiming family;
stage 10 keeps mode -2 refused and relies on S13's revocation for its 0x6060 writes.
