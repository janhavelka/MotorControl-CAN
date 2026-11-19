# Nanotec CANopen profile promptset (FIR-v2213 generation)

Prepared 2026-11-19. Source collection and prompt preparation complete; revised the
same day after five rounds of independent [audit](audit.md) ([application](audit_application.md)).
All implementation stages are NOT STARTED; Nanotec hardware is NOT TESTED.

Add the Nanotec CANopen family as an independent concrete profile of
MotorControl-CAN, beside the implemented Lichuan C profile, without changing
Lichuan behavior. This is the [decided next profile](../../multidrive_decision.md)
(`docs/audit/2026-11-19_family_selection.md`). Every stage runs under the
[execution contract](execution_contract.md) and the binding
[profile integration contract](../canopen_shared/02_profile_integration_contract.md).
Dispatch one stage at a time and carry its handoff into the next. Creating this
promptset does not execute it, and no stage authorizes hardware work by itself.

## Scope

| Row | Models and suffixes | Firmware | Status in this promptset |
| --- | --- | --- | --- |
| Primary | N5-1-2, N5-2-2 (CANopen, hardware W007 and later) | FIR-v2213, manual 3.3.0; EDS build B1031134 | Implementation target |
| Same profile | C5-E-1-09, C5-E-2-09 (CANopen/USB, W006+) | FIR-v2213, manual 3.3.0 | Admitted where its own pages and EDS match |
| Same profile | CL4-E-1-12, CL4-E-2-12 (CANopen/USB/Modbus RTU, W002+); -5VDI suffixes | FIR-v2213, manual 1.5.0 | Admitted where evidence matches; -5VDI refused until [stage 13](13_firmware_and_model_rows.md) stores its EDS |
| Same profile | PD4-C5918M4204-E-08 (stepper), PD4-CB59M024035-E-08 (BLDC); other PD4-C ...-E-08 steppers | FIR-v2213, manual 3.3.0 | Admitted where evidence matches; dual product codes (NT-D14) gated |
| Unqualified, behavior-gated | N5, C5-E, PD4-C (CL4-E history starts at FIR-v1825) | FIR-v1650 (stock firmware per the product pages; N5 2.0.3 manual: no 1016h, 1F80h, 3701h, 6007h) | Runs reported as firmware unqualified; [stage 13](13_firmware_and_model_rows.md) gates refuse only what it lacks (host-loss setters, limit reaction, mode change when enabled and others) |
| Unqualified, behavior-gated | Every model above | FIR-v2538 (packaged EDS only, no manual) | Runs unqualified; stage 13 gates refuse COB-ID writes (2007h:02) and the changed EDS entries |

Both motor classes are configurations of the same controllers, not separate
APIs: open-loop stepper, closed-loop stepper with encoder, BLDC with Hall sensors
and BLDC with encoder on N5, C5-E and CL4-E; fixed stepper or BLDC on PD4-C. Motor
kind, feedback and supported modes are caller-declared capabilities checked
against readback, never detected.

Excluded: N5-x-1 (EtherCAT), N5 -3/-4/-5, C5-E-x-09A (no manual), PD4-C ...-E-01
(USB product), PD6-E (later row), the CL4-E FIR-v2315 build, Modbus RTU and
Ethernet REST as control paths, Plug & Drive interface commands (2291h), NanoJ
program authoring or upload, clock-direction mode (6060h = -1), PD4-C special
drive modes (4015h), LSS and SDO block transfer.

## Stages

| Stage | Deliverable | Dependencies and gates |
| --- | --- | --- |
| 01 | [Evidence and coverage](01_evidence_and_coverage.md) | canopen_shared 01 inventory; NT-D dispositions including NT-D32; documentation only, no hardware |
| 02 | [Object ledger, identity rows, generated tables and operation map](02_object_ledger.md) | 01; seam S7 and the package entries with the first generated header; per-model and per-firmware scopes; RW grants no write |
| 02b | [Profile registration, probe and console family](02b_profile_integration.md) | 02; seams S1, S2 (with S3's and S13's default deny, including the owner-level deny of targeted NMT until 09), S6, S10; no identity outcome, so identity-dependent routes unavailable; real owner, `SdoClient` and fake server; console part Blocked until S4 |
| 03 | [Typed reads, identity gate and capabilities](03_reads_identity_capabilities.md) | 02b; prompt 57 identity outcomes and admission (stops exempt); firmware and hardware reported unqualified with stage 13 gates (no firmware lock); read-only `Supervision` snapshot; no detection of motor class |
| 04 | [CiA 402 state machine, enable, release, stops and fault reset](04_state_machine_and_stops.md) | 03, 04b item 1; source-state table with bit-8 rows (a set bit is a requested halt, never an attained one) and two stop branches that never refuse for missing evidence (unknown halt sets bit 8; 010Bh without a fresh report); host mode table, enabled VL/PV entry only with a fresh zero target (accepted through the public route in 06a); halt state from a fresh 6040h readback (N5 p346) under the ownership premise; Nanotec S3 shape (010Bh) and S8; edge rules on bits 2 and 7; the positive two-family stop test |
| 04b | [Vendor command paths: Plug & Drive interface, NanoJ, auto setup](04b_vendor_command_paths.md) | Item 1 (exclusive-control admission for every motion stage) after 03 and before 04's enable; items 2-5 after 04; auto setup has its own state sequence |
| 05 | [Units and finite position (PP)](05_finite_position.md) | 03, 04, 04b, 07 item 8 (limit admission); caller-qualified unit relation; NativeRateBasis gap |
| 06 | [Velocity, profile torque and cyclic modes](06_velocity_torque_cyclic.md) | 05; timed runs need a stop for an unresolved route (opted-in S3 shape or a declared independent stop with its fallback deadline); 06a PV/VL with start rows naming the start-capable write, 06b PT with active closed loop, 04's bit-4 and halt checks, a fresh zero 6071h and its own start rows, 06c after 09 and 09b and only with seams S13 and S14 |
| 07 | [Homing, limit switches and software limits](07_homing_and_limits.md) | Item 8 (limit admission, prompt 77 item 2) after 03 and before 05's move; items 1-7 after 05; moving homing is a host-ended run (interruption time, stop budget, transfer cap, stop path before the first changing TX, declared stop with its fallback deadline, `prepareHomeInterrupt`); method 35 with its own source-state rows, never enabling; NT-D11, NT-D18; prompt 77 vocabulary |
| 08 | [I/O, brake, motor and protection settings](08_io_brake_settings.md) | 03; four blocks 08a..08d; NT-D12, NT-D25 |
| 09 | [PDO, SYNC, EMCY and boot-up](09_pdo_sync_emcy.md) | 03; seam S13 with Nanotec SYNC and node-0 NMT effects, the same rows for targeted NMT, rechecked at hand-off; declared ownership exceptions make touched evidence unqualified; replaces 02b's targeted NMT deny; targeted Reset Node with 10's restart admission; COB-ID changes BLOCKED until 11 item 4's identifier check lands, then with configured, pending and active values (a TPDO frame activates only with an evidenced boundary); firmware readiness reported UNKNOWN unless 2007h:01 = 1 was in force at the boot-up; 8 RPDO and 8 TPDO; NT-D07, NT-D09, NT-D16, NT-D30 |
| 09b | [Host-loss supervision and reactions](09b_host_loss_supervision.md) | 09; off by default; NT-D08, NT-D24; the nonzero 1016h setter needs seam S11 with its delivery bound and handoff recheck, and a live matching producer |
| 10 | [Communication settings and persistence](10_communication_and_persistence.md) | 03, 04, 08, 09; save reservation also refuses restart, restore and NMT; the 2800h restart and a targeted Reset Node with one admission (no torque, NT-D32 acceptance, pending until boot-up); row-scoped save snapshot; NT-D04, NT-D05, NT-D06, NT-D09, NT-D28 |
| 11 | [Console and mixed-profile integration](11_console_and_integration.md) | Implemented subsets of 02b-10; item 4's identifier check lands first and unblocks 09's COB-ID change; prompts 64, 75 when landed |
| 12A | [First-contact questions](12a_first_contact.md) | Optional; needs an identified Nanotec fixture and authorization; one evidence path per question, never a production bypass |
| 12B | [Qualification and release reconciliation](12_qualification_and_release.md) | Software closeout any time; HIL needs the exact fixture |
| 13 | [Further firmware and model rows](13_firmware_and_model_rows.md) | Behavior-gate table now; qualifying a row needs new source evidence, never inferred from FIR-v2213 |

The numbering follows the common family layout (01, 02, 02b, 03 to 12) so that
stage N means the same topic in every family set. Nanotec adds 04b because the
drive has command paths besides CiA 402 that every motion stage must check, 09b
because host-loss supervision is off by default and needs its own evidence and the
shared host heartbeat producer (seam S11), and 13 because two firmware generations are known to
exist but are not documented for these models. Stage 06 holds three blocks
(06a..06c) and stage 08 four (08a..08d), each delivered and committed alone.

Dependencies are semantic prerequisites, not a demand to guess missing behavior
or leave independent work idle. A stage may finish a usable subset while
named operations stay gated. Every documented native function stays in the
coverage denominator whether or not it is implemented.

## Prepared artifacts

- [Nanotec reference pack](../../reference/nanotec_canopen/README.md): the
  [engineering review](../../reference/nanotec_canopen/review.md) with discrepancies
  NT-D01..NT-D31 (NT-D32 is recorded by [stage 01](01_evidence_and_coverage.md) item 4,
  not in the review), the [source manifest](../../reference/nanotec_canopen/SOURCES.json),
  the CL4-E 1.5.0, PD4-C 3.3.0 and N5 2.0.3 (FIR-v1650) manuals, the FIR-v2039 update
  instructions, the Plug & Drive interface description, the all-objects EDS for
  eight models at FIR-v2213 and FIR-v2538, four N5 OD XML files and product pages.
- The [N5 3.3.0](../../reference/multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf)
  and [C5-E 3.3.0](../../reference/multidrive/vendor/nanotec_c5e_canopen_v3.3.0.pdf)
  manuals, kept with their hashes in the multidrive pack.
- The [CANopen principles pack](../../reference/canopen_principles/README.md)
  (`e02ab04f`; its Kollmorgen AKD manual is remote-only) for CiA 301/402 facts and the
  CP-D discrepancies where vendors differ. It is not device evidence.

These are research inputs, not implemented tables or proof that a delivered
unit runs FIR-v2213. Nothing in the pack was observed on hardware.

## Preparation verification

Run `python scripts/check_promptset.py` and `git diff --check` for this
documentation set. They check links and table shapes; they compile nothing.
The checker discovers the pack's manifest by itself (canopen_shared 01 intake rule
"Pack"); nothing in the checker is edited for it. No library source, firmware, CI run or
hardware traffic belongs to this preparation. Implementation stages follow the
impact-based CI policy in the execution contract.

## Relationship to the main sequence and the shared seams

This family most likely reaches the [canopen_shared](../canopen_shared/README.md)
seams first, so its stages plan their implementation: S7 (per-family checkers and the
first header's package entries) in 02 with the first generated header; S1 (inventory),
S2 (`Endpoint.profile` resolved to `DriveProfile`) with S3's and S13's default deny in the same
commit, S6 (console family and per-record JSONL `profile`) and S10 (SDO reuse policy
per binding) in 02b; the Nanotec S3 shape (opt-in best-effort stop per binding) and
S8 (shared CiA 402 helpers, only for identical evidenced semantics) in 04; S13 (SYNC
and node-0 NMT effect admission) in 09; S11 (host heartbeat producer) in 09b. S14 (owner-admitted
RPDO data) is needed only if 06c is assigned. Each is claimed
in the seam table before the shared edit, following that README's claim, land and
consume rules. If another family claimed or landed a seam first, the Nanotec stage
waits for it or extends it and builds no copy. S12 (owner-admitted RTR) is not
claimed: Nanotec documents a heartbeat consumer, and node guarding stays BLOCKED
(09b). S4 (common axis table) belongs to [prompt 64](../64_common_axis_interface.md),
S5 (optional packages) to [prompt 65](../65_application_package.md) and S9 (second
protocol on one interface) to [prompt 63](../63_mixed_protocols.md); Nanotec consumes
S4 and S5 when they land. It does not need S9, because Nanotec and Lichuan share the
CANopen domain; a shared interface instead needs the recorded same-domain analysis
of canopen_shared 02 "Multi-profile consumers and package scope" (stage 11), and
until then each profile runs on its own interface. Main-sequence prompts [57](../57_regular_drive_operations.md),
[73](../73_timing_freshness_guards.md), [75](../75_runtime_node_table.md) and
[77](../77_switch_ready_homing_limits.md) define common policy this family follows
(enable/release/mode select, timeouts and freshness, node table entries, limit
admission); a Nanotec stage never substitutes a family-local version of them.

## Open questions

These conflicts remain after reconciliation with canopen_shared; the stages keep
the dependent deliverable Blocked or narrowed until the named owner decides.

1. **Console before S4.** Every console SDO read goes through the Lichuan-typed
   `ProbeApp`, so each stage's console route and API/CLI parity test is Blocked until
   S4 lands. Whether a minimal family read may extend `ProbeApp` before prompt 64
   replaces it is open for the prompt 64 lead or the user
   ([audit application](../canopen_shared/audit_application.md), "Unresolved
   questions" item 2).
2. **Shared-interface gate.** Stage 11's Lichuan-plus-Nanotec interface rests on a
   recorded same-domain analysis instead of prompt 63. The user may still require
   prompt 63 for every shared interface (audit application, "Remaining open items",
   third item; contract 02 "Multi-profile consumers and package scope").
3. **NanoJ autostart (NT-D32).** Whether a stored program starts at power-on on each
   admitted model decides what 04b and 10 may assume after a restart. Stage 01 records
   it, and only device or vendor evidence closes it.

Hardware: no Nanotec fixture exists, and the owner is away until about
2026-12-04 under the [owner-absence rules](../COMMON.md). Never send Nanotec frames
to the CL86-C bench (COM15, nodes 1 and 2).
