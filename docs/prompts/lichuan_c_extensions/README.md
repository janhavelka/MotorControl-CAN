# Lichuan C-family extensions promptset (TL-C integrated steppers, CL2 drives, 57C English manual)

Prepared 2026-11-19. Source collection and prompt preparation complete. All implementation stages are NOT
STARTED; TL-C, CL2 and CL57-C/OL57-C hardware is NOT TESTED. Revised the same day after two independent
[audit](audit.md) rounds ([application](audit_application.md)) and prompt 57's identity policy as of `a7656efb`.

This promptset **extends the implemented Lichuan C profile** (`lichuan_c`, `MotorControlCAN::LICHUAN_C`,
CL86-C first). It is not a new profile: no `DriveProfile` value, no copied profile directory, no second
ledger. New models become declared models and model-scoped rows of the existing
[reviewed ledger](../../reference/lichuan_objects.json), and every existing operation refuses a model until a
row admits it. Evidence is the [LX reference pack](../../reference/lichuan_c_extensions/README.md) and its
[delta review](../../reference/lichuan_c_extensions/review.md) with discrepancies LX-D01..LX-D35, written
against [lichuan_review.md](../../reference/lichuan_review.md). Generic CANopen facts come from the
[CANopen principles pack](../../reference/canopen_principles/principles.md). Every stage runs under the
[execution contract](execution_contract.md); dispatch one stage at a time and carry its handoff forward.
Creating this promptset does not execute it. Selection rationale: [family selection](../../audit/2026-11-19_family_selection.md).

## Scope

| Row | Models (order codes) | Sources | Feedback and notes |
| --- | --- | --- | --- |
| TL-C | TLC42-C, TLO42-C (24V-04/-08); TLC57-C, TLO57-C (36V-1/-2/-3); TLC60-C, TLO60-C (36V-3/-3.5/-4); TLC86-C, TLO86-C (48V-4.5/-8.5/-10/-12) | TLCEN, TL42ZH..TL86ZH, TL42OP..TL86OP, TLEDS, PG-TL42..PG-TL86, PG-TL57EN | Integrated 1.8 degree stepper; TLC 1000-line encoder, TLO none (TLCEN p3). Five inputs, two outputs (TLCEN p6) |
| CL2 | CL2-57C (42/57/60 motors), CL2-86C (86 motors) | CL257, CL286, PG-CL257, PG-CL286 | 17-bit or 23-bit absolute encoder, single or multi-turn (CL257 p4, p10). No EDS |
| CL2 ABZ | CL257C-ABZ, CL286C-ABZ | ABZ57, ABZ86, PG-ABZ57, PG-ABZ86 | 1000-line ABZ encoder, Z optional (ABZ57 p4, p6). No EDS |
| 57C English | CL57-C, OL57-C | EN57, PG-CL57EN | Documentary witness only (stage 05); no model value or capability is added |

Firmware rows: no source names a hardware or firmware revision; document dates (TLEDS 2023-05-25, TL user
manuals 2024-07-24, CL2 manuals 2026-03-31, EN57 2022-11-29) identify documents, not firmware. Excluded: TL-P,
TL-R, TL-E and the CL2 pulse and RS485 drives (other products); the DS-C/DS2-C servos (their own
[reference pack](../../reference/lichuan_ds2_canopen/README.md) and promptset); motor matching through the vendor tool over
Type-C (CL257 p10); the vendor debug software, which is never downloaded or run; torque, CSP, CSV, CST and
host-loss reactions, which no extension source documents.

## Stages

| Stage | Deliverable | Dependencies and gates |
| --- | --- | --- |
| 01 | [Model declaration, decision applicability and identity confirmation](01_models_and_identity.md) | canopen_shared 01 refresh; backlog D18; coordinate prompt 57 identity policy; no hardware |
| 02 | [Model-scoped rows on the LICHUAN_C ledger](02_model_ledger.md) | 01 model values and register; TLEDS lossless accounting; LX conflicts under a second hash guard |
| 03a | [TL-C rows: reads, status table and the bitrate hazard](03a_tlc_rows.md) | 02; LX-D03/D04/D05/D06/D11/D12/D13; X gates for TL-C |
| 03b | [TL-C operations](03b_tlc_operations.md) | 03a; LX-D07/D08/D09/D10; prompts 57 and 77 where landed |
| 04a | [CL2 rows: mode-dependent status, encoder and loop rows, absent communication objects](04a_cl2_rows.md) | 02 (independent of 03); LX-D20..D25/D28/D30..D34 |
| 04b | [CL2 operations](04b_cl2_operations.md) | 04a; LX-D23/D25/D26/D27/D29; prompt 77 where landed |
| 05 | [The 57C English manual: C01, C02, C17 and Q02](05_en57_conflicts.md) | 02; ledger-only; LX-D15..D19 |
| 06 | [Console, capabilities and coverage for the model rows](06_console_and_coverage.md) | Implemented subsets of 03a..05; canopen_shared S6 if landed; prompt 64 after it lands |
| 07a | [First contact per model](07a_first_contact.md) | An exact TL-C or CL2 fixture, owner return and authorization; a reviewed fixture electrical record before first power; each question's evidence path: a production route (reads after 03a/04a, admitted operations after 03b/04b) or an assigned isolated experiment, otherwise NOT RUN |
| 07b | [Qualification and release reconciliation](07b_qualification_and_release.md) | Software closeout after 06; physical part needs the 07a fixture |

Why this split: TL-C and CL2 each get two stages. The rows (ledger, reads, status decoding, hazards) are
usable alone and gate the operations; the operations touch enable, stop, motion,
homing, I/O, tuning and persistence and need their own tests. For CL2 the split matters most: its statusword needs a
mode-aware decoder that every completion path consumes. First contact and qualification are split as in the
MotorControl-RS model (12A/12B): first contact can run per model as soon as a route exists.

Dependencies are semantic prerequisites, not a demand to guess. A stage may finish an independently usable
subset while named operations stay gated. Software state and hardware evidence are separate columns in
`PROGRESS.md`, which stage 01 creates in this directory.

## Existing decisions that do not transfer

Stage 01 records each in a machine-checked applicability register; the owning stage extends or refuses it.

| Existing decision (symbol or ledger id) | Why it does not transfer | Owner |
| --- | --- | --- |
| `StatusLayout` EN86/ZH86/ZH57/OP86, `interpretStatusword` | Each table belongs to its document; TL-C has its own table, CL2 a mode-dependent one | 03a, 04a |
| `STATUS_EN86_ZERO_SPEED`, the enable gate's bit 10 mask | CL2 bit 10 is a completion flag (LX-D23) | 04a, 04b |
| `MONITOR_*` (2100), `RELEASED_LATCH_STATUS_BIT` | CL2 bit4 polarity unstated, bits 5/6 are limit inputs (LX-D28); latch is a CL86-C bench state | 03b, 04a |
| `StopKind`, `stopPredecessorAllowed` (DEC-8), best-effort `000B` premise (DEC-2) | Predecessors and effects measured on CL86-C specimens only | 03b, 04b |
| `AlarmClearMethod` (`OBJECT_2301`, default `CONTROLWORD_BIT7`) | 2301 is EDS-only for TL-C (LX-D11); the default is a CL86-C bench decision | 03b, 04b |
| `IdentityWidths::MEASURED_U8`, `CountWidths::MEASURED_U16` | Both are CL86-C specimen replies; TLEDS (l.212, l.220) and CL257 p14 declare `1009`/`100A` U16 | 03a, 04a |
| DEC-1 read acceptance and the `SdoReuse` default markers | Owner risk acceptance for CL86-C rows; CL2 has no documented 1018 | 03a, 04a |
| `EnableGate`, `EnableStart`, `ENABLE_SERVICE_STEPS`, `MoveEvidence`, `VelocityEvidence` | Attended CL86-C trials and measured CL86-C firmware | 03b, 04b |
| `HomingMethod` 17/18/24/29, mode-6 completion on bit 8 | CL2 gives the same numbers other trajectories and uses bit 10 (LX-D23, LX-D26) | 03b, 04b |
| `InputFunction` 0..13, `prepareIoSettings` (2400:01..08) | TL-C has five inputs (LX-D04); CL2 code 4 is release, 7..13 absent (LX-D25) | 03b, 04b |
| `LoopMode` 1/2 and the OL86-C closed-loop refusal | CL2 closed loop is 3 (LX-D22); TLO has no encoder | 03b, 04a |
| C16/Q08 raw filter policy for `2401..2407` | On TL-C these indices hold a bitrate selector and reserved entries (LX-D03) | 03a |
| Q16 DIP decoding and "no software bitrate object" | TL-C uses SW1..SW4/SW5/SW6 and documents a bitrate object; CL2 drops all-OFF | 03a, 04a |
| Heartbeat `1017` 0 to 1000 and TPDO1 transitions (`CL86_C` scopes) | Measured on CL86-C; CL2 documents no communication records | 03a, 04a |
| `prepareSave`/`prepareFactoryRestore` (2300) | On TL-C a save also activates a stored bitrate after restart | 03b |
| S21 pulse-rate limit, velocity ladder, bench states S20/S21 | CL86-C fixture observations | 03b, 04b |
| `MoveFirmware`, `VelocityFirmware`, `HomeFirmware` (evidence bound to the exact firmware tuple) | Prompt 57 removes the firmware lock: new models report a firmware difference and never refuse on it; a missing fact refuses with its own gate | 03b, 04b |

## Prepared artifacts

- The [LX pack](../../reference/lichuan_c_extensions/README.md): 26 stored originals (14 PDFs, one EDS, 11
  HTML pages) with URL, Referer, size and SHA-256 in [SOURCES.json](../../reference/lichuan_c_extensions/SOURCES.json),
  15 PyMuPDF extracts and a derived TLEDS against C-EDS delta. Rights: "ALL RIGHTS RESERVED"; outside the
  library license and excluded from packages.
- The delta review with LX-D01..LX-D35, an object-by-object delta, a non-transfer table and "what a profile
  must not assume". These are research inputs, not implemented tables or firmware facts.

## Preparation verification

`python scripts/check_promptset.py` (links, table shapes, manifests and hashes) and `git diff --check`. They
compile nothing. On 2026-11-19 the claims turned into instructions were rechecked on the rendered original
pages and EDS lines named in each stage. No product source, firmware or hardware behavior changed.

## Relationship to the main sequence and the canopen_shared seams

- [Prompt 57](../57_regular_drive_operations.md) (Pending) refuses motion on a differing product identity
  (`1000`, `1018:01/02`, `2000`) and now records D18. CL86-C, TL-C and DS-C declare the same `1000` and
  `1018:01/02` (LX-D01, LX-D35) and the same `2000` default 1 (C-EDS l.1229, TLEDS l.1324, DSCEDS l.1212;
  declared defaults, not live values: both CL86-C specimens read `2000` = 0x0400, Q01), so a match does not
  prove a CL86-C; the declared binding selects the
  model and the Lichuan family sets add model-scoped checks on that one policy. Its outcomes (CONFIRMED means
  unique among the supplied references, not physical proof), its "Admission by outcome" precedence (only a
  declared-model match admits), its vendor-object and motion rules, its stop exemption and its firmware
  rule (reported, never refusing) are defined once in prompt 57's Identity policy bullet; the execution
  contract's
  [identity outcomes](execution_contract.md#identity-outcomes-prompt-57s-policy) list what this set supplies,
  as the DS2 set's do. Prompt 57, stage 01 and DS2 stage 03 land one comparison; whichever lands later
  extends it.
- Prompt 64 (common axis table), 65 (package layout), 72 (enable-fault guards) and 77 (input facts) are
  consumed when landed, never replaced by family-local copies.
- [canopen_shared](../canopen_shared/README.md) seams. This set claims none and may land before any family:
  - S1 and S2 are not needed (no new profile); the model binding is profile-internal. Once S2 lands, a
    new-model binding carries `LICHUAN_C`'s `DriveProfile` value like every Lichuan binding.
  - S3: TLCEN p11 and CL257 p12 document `000B` only from predecessor 15, while the best-effort word goes out
    with the predecessor unknown, so TL-C and CL2 keep S3's default deny until a reviewed model decision
    covers that case (X15). Before S3 lands, `prepareBestEffortStop` refuses these models; after it, the
    application configures no shape for their bindings (03b item 3, 04b item 3). CL86-C keeps DEC-2.
  - S10 keeps Lichuan's marker default for every `lichuan_c` binding, but the markers read `1018`, which CL2
    does not document and DEC-1 does not cover for any new model. New-model bindings are set to
    `SdoReusePolicy::NONE` through the existing `setSdoReuse` (03a, 04a), and new-model preparations
    require the application's attestation of it in the `ModelBinding` (stage 01 item 5); this is per model,
    not S10.
  - S6: model fields in JSONL extend the Lichuan envelope; `profile` stays `lichuan_c` on every record S6 gives
    a target (and `null` where S6 says so), and S6's work stays with the family that claims it.
  - S7: a new header under `profiles/lichuan_c/`, or a common header such as stage 01's profile-neutral
    identity comparison, is mapped in `lichuan_operations.json` in the same commit and is not S7's trigger;
    S7's checker partition stays with the first new family.
  - S4: new-model routes ride the Lichuan-typed `ProbeApp`, so unlike a new family they are not Blocked before
    prompt 64; prompt 64 later absorbs them. S5, S8 and S9 are not applicable.
  - S11 (host heartbeat producer) and S12 (RTR node guarding) are not needed: TLEDS declares no `1016`, `100C`
    or `100D` and the CL2 manuals document neither, so no extension documents host-loss supervision.
  - S13 (SYNC and NMT-broadcast effect admission) is not claimed: no TL-C or CL2 source documents what a
    SYNC does. No model row of this set admits `sync` (03a item 6, 04a item 1), but a per-model refusal does
    not stop the interface's producer or a node-0 NMT command, and S13's default deny keys on non-Lichuan
    bindings, which these `LICHUAN_C` bindings are not. From 03a and 04a the application therefore refuses
    SYNC and node-0 NMT on every interface holding a TL-C or CL2 binding (03a item 6) until S13's full gate
    lands; then this set supplies their effects as unknown and its population-wide guard (canopen_shared 02,
    CANopen-only rules, S13) refuses on their interface.
