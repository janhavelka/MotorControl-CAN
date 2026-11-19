# Lichuan DS2-C CANopen servo profile promptset

Prepared 2026-11-19. **Source collection and prompt preparation complete. All implementation stages are NOT
STARTED; Lichuan DS2-C hardware is NOT TESTED.**

Add the Lichuan DS2-C low-voltage CANopen servo family to MotorControl-CAN as one concrete profile, `lichuan_ds2`,
beside the implemented Lichuan C stepper profile (`lichuan_c`). It is the library's first Lichuan servo and its first
profile-torque mode. The [execution contract](execution_contract.md) binds every stage. The
[shared integration contract](../canopen_shared/02_profile_integration_contract.md) says how the profile lands in
the code. The [reference pack](../../reference/lichuan_ds2_canopen/README.md) and its
[evidence review](../../reference/lichuan_ds2_canopen/review.md), with discrepancies DS-D01..DS-D32 (stage 01 adds
DS-D33 and DS-D34 as intake corrections), are the evidence. Dispatch one stage at a time and carry its handoff into
the next. Creating this promptset does not execute it. The family was
[selected on 2026-11-19](../../audit/2026-11-19_family_selection.md); selection is not a support claim. The set name
`lichuan_ds2_canopen` names `PROGRESS.md` and the stage reports; the profile token is `lichuan_ds2`.

## Scope

| Model | Manuals | EDS | Model-specific documented facts |
| --- | --- | --- | --- |
| DS2-10C, DS2-20C, DS2-45C | DS2C, DS2OP | DS2EDS | DS2C status table; `607F` torque speed limit; DIP nodes 1..7; BRK+/BRK- terminals |
| DS2-Mini-15C-400, DS2-Mini-20C-750 | MINI | none linked | DS2C status table; `607F`; DIP nodes 1..7; no brake pins (DS-D26) |
| IBL60-C | IBL60 | IBLEDS | IBL60 status table; vendor `2406h` speed limit; DIP nodes 1..31; brake by FunOUT.9 |
| IBL80-C | IBL80 | IBLEDS | DS2C status table (DS-D04); `2406h`; DIP nodes 1..31; brake by FunOUT.9 |
| ISL60-C, ISL80-C | ISL60 or ISL80, ISLMP | ISLEDS | IBL60 status table; `607F`; DIP nodes 1..7; FunOUT.9; touch-probe text (DS-D28) |

- Interface letter C only (CANopen, DS2C p4, MINI p4, IBL60 p5, ISL60 p6). Encoder suffixes C17, R17, C23 and R23
  (17/23-bit incremental or absolute) are declared by the application; IBL manuals claim 17-bit single-turn only
  (DS-D25). Power and voltage codes are fixture facts.
- Firmware rows: none. No source states a hardware or firmware revision or the meaning of `1009`/`100A`; every
  claim has document scope only. Stage 12a records the first specimen's identity.
- Excluded: the P/PR and E interface variants, the older DS-C servo (its EDS is stored only for the identity
  comparison), Modbus and USB debug-port parameter access (DS2C p22-23, DS2OP p5), the vendor debug software,
  every P-parameter without a CANopen index, LSS (`LSS_Supported=0`), CAN FD, interpolated and cyclic modes (not
  documented), and the CL86-C, TL-C and CL2 families (their own promptsets).

## Stages

| Stage | Deliverable | Dependencies and gates |
| --- | --- | --- |
| 01 | [Evidence, inventory and operation coverage](01_evidence_and_coverage.md) | Pack and contracts; canopen_shared 01 inventory at current HEAD; documentation only |
| 02 | [Object ledger, checkers and generated tables](02_object_ledger.md) | 01; seam S7 with the first headers' package entries; tooling change under the CI policy; no hardware |
| 02b | [Profile registration, probe and `ds2` console family](02b_profile_integration.md) | 02; seams S1, S2 with the S3 and S13 default denies, S6, S10; the probe's read-effect row; real owner, `SdoClient` and fake server |
| 03 | [Typed reads, identity, status dialects and capabilities](03_reads_identity.md) | 02b; prompt 57's identity outcomes; prepares `DS-DEC-1`; DS-D01..D05, D14, D19, D30, D33, D34 gate interpretations only |
| 04 | [Mode select, enable, release and stops](04_state_and_stops.md) | 03; source-state table first; controlword history and trigger predecessor; `DS-DEC-2` stop rows; seam S3 opt-in after its every-state review, seam S8 review; DS-D17, D21, D29 |
| 05 | [Units and finite profile-position moves](05_finite_position.md) | 04; DS-D09, D16, D33, D34; negative encodings and ramp units stay declared or gated |
| 06 | [Finite profile velocity](06_profile_velocity.md) | 04, 05 units; DS-D10, D16, D32; enable is the start; a `DS-DEC-2` mode-3 stop row |
| 06b | [Finite profile torque](06b_profile_torque.md) | 04, 06 run pattern; DS-D11..D14; `607F` width per scope; `2406h` write withheld; a `DS-DEC-2` mode-4 stop row |
| 07 | [Homing and limits](07_homing_and_limits.md) | 04, 05; `DS-DEC-1` for completion; DS-D08, D15, D16, D18; prompt 77 vocabulary when landed |
| 08 | [I/O, brake, encoder and drive settings](08_io_and_settings.md) | 03, 04 `released` predicate, 05; DS-D24..D28, D30, D31; most vendor functions unreachable |
| 09 | [PDO, SYNC, EMCY and heartbeat observation](09_pdo_sync_emcy.md) | 03; DS-D06, D07, D19, D32; no remap, no RPDO |
| 10 | [Communication settings and persistence](10_communication_persistence.md) | 03, 08 settings context, 09; DS-D20, D22, D23 |
| 11 | [Console and mixed-profile integration](11_console_integration.md) | Implemented subsets of 02b..10; prompts 64 and 65 when landed; shared-interface positive tests after S13 (SYNC and node-0 NMT) |
| 12a | [First-contact questions](12a_first_contact.md) | Optional when a DS2 fixture and authorization exist; each question after its route |
| 12 | [Qualification and release reconciliation](12_qualification_release.md) | Software closeout after 11; physical work needs a DS2 fixture |

Dependencies are semantic prerequisites, not a demand to guess missing behavior or leave independent work idle. A
stage may finish a usable subset while naming its gated operations. Every documented native function stays in the
coverage denominator. Shape decisions: torque is its own stage (06b) because it has its own evidence, discrepancies,
vendor object and free-shaft risk; homing and limits are one stage because `607D` and the limit inputs serve homing
first; 01 has no checker because the operation checker needs the first public header (02); there is no segment,
tuning or commissioning-motion stage because the family documents no stored segments and its gains, node and
bitrate settings are P-parameters without a CANopen index (08 and 10 record them as unsupported with reasons).

## Discrepancy ownership

Each DS-D stays open until its owning stage closes it with cited new evidence or records its gate.

| IDs | Topic | Owning stage |
| --- | --- | --- |
| DS-D01, DS-D02, DS-D03, DS-D05 | `1018` claims, identity collisions (backlog D18), `1000` motor bits, serial sub-index | 01 decision, 03 reads, 12a live values |
| DS-D04 | Two status-word layouts | 03 decoder; 04, 05, 07 consumers; 12a |
| DS-D06, DS-D07 | PDO defaults, `1014` and `1200` claims | 09, 10 |
| DS-D08 | `6040`/`6060` access and `6098` type | 02 ledger, 04 mode select, 07 |
| DS-D09 | `607A` U32 versus I32 | 05 |
| DS-D10 | `60FF` type and range | 06 |
| DS-D11, DS-D12, DS-D13, DS-D14 | Torque speed limit, slope, maximum torque, actual torque access | 06b (D14 also 03) |
| DS-D15, DS-D18 | `607D` sub-index order; homing method 35, 31/32 | 07 |
| DS-D16 | Ramp and homing-speed units | 05, 06, 07; 12a |
| DS-D17, DS-D21, DS-D29 | `605D` halt, fault reset, `P02-00` control mode | 04 (D29 also 12a) |
| DS-D19 | `1003` count and Er-code to EMCY mapping | 03 reads, 09 events |
| DS-D20, DS-D22, DS-D23 | Save and restore, node ID, bitrate | 10 |
| DS-D24..DS-D28, DS-D30, DS-D31 | Ratings, product pages, brake wiring, fault texts, touch probe, `60FD` text, motor codes | 01 model matrix, 08 |
| DS-D32 | No host-loss, NMT or boot behavior | 06, 06b, 09, 10; 12a |
| DS-D33, DS-D34 | `6081` default and unit; `6091:02` type (stage 01 intake corrections; the pack review is not edited) | 02 ledger; 05 (D33 also 12a, D34 also 08) |

## Prepared artifacts

- The [reference pack](../../reference/lichuan_ds2_canopen/README.md): 25 originals (seven manuals, three family
  EDS files and the DS-C EDS, nine product pages, four download-index pages), eight PyMuPDF extracts, the review and
  [SOURCES.json](../../reference/lichuan_ds2_canopen/SOURCES.json) with URL, Referer, size and SHA-256.
- The [CANopen principles pack](../../reference/canopen_principles/principles.md), committed at `e02ab04f`.
- No object ledger, generated header, profile code, test, console route or `PROGRESS.md` exists. Stage 01 creates
  `PROGRESS.md` in this directory.

## Preparation verification

`python scripts/check_promptset.py` passed with no errors after these files were written; it checks links and table
shapes and compiles nothing. No build, firmware gate, remote CI or hardware step ran, and no product file changed.
Two independent audit rounds on 2026-11-19 ([audit](audit.md)) were verified and applied the same day
([application](audit_application.md)); the same checks passed again for this directory.

## Relationship to the main sequence and shared seams

| Seam ([table](../canopen_shared/README.md)) | This family |
| --- | --- |
| S1 inventory, S2 binding profile and foreign refusal, S6 console and JSONL profile | 02b lands or extends them; S6's console part waits for S4 |
| S7 per-family ledger, generator and operation checker, with the first header's package entries | 02 lands or extends it, package entries in the commit of the first DS2 headers |
| S3 per-binding best-effort stop shape | Default deny in the S2 commit (02b), before any DS2 binding is admitted; 04 opts in DS2's shape under `DS-DEC-3` only with its every-state review |
| S8 shared CiA 402 helpers | 04 reviews; only an identical, evidenced helper is extracted |
| S4 axis table and session, S5 package components | Consumed only after [prompt 64](../64_common_axis_interface.md) and [prompt 65](../65_application_package.md) |
| S9 second protocol on one interface | Not needed: DS2 is CANopen; [prompt 63](../63_mixed_protocols.md) stays its owner |
| S10 reviewed SDO reuse per binding | 02b lands or extends it; DS2 bindings keep `NONE` until a reviewed marker decision, separate from `DS-DEC-1` |
| S11 host heartbeat producer, S12 RTR node guarding | Not needed: DS2 documents no `1016` consumer and no node guarding (DS-D32) |
| S13 SYNC and NMT-broadcast side-effect admission | Default deny in the S2 commit (02b): no SYNC and no node-0 NMT on any interface with a DS2 binding. The full gate is not claimed: DS2 documents no SYNC or NMT effect, so its bindings enter it as unknown and keep both refused (09, 11); until it lands DS2 shares no interface with another family |

A family that reaches a seam first claims it in the canopen_shared table and implements it once; a later one extends
it. Main-sequence prompts this family consumes or must stay compatible with: [57](../57_regular_drive_operations.md)
(identity policy, which defines the identity outcomes and the vendor-object and motion rules both Lichuan family sets
use, beside the [Lichuan C extensions contract](../lichuan_c_extensions/execution_contract.md); enable, release and
mode select per profile), [60](../60_user_units.md) (user
units), [62](../62_stop_line.md) (wired stop and host loss), [70](../70_startup_settings_reboot.md) and
[75](../75_runtime_node_table.md) (expected product identity per node), [73](../73_timing_freshness_guards.md)
(optional guards) and [77](../77_switch_ready_homing_limits.md) (limit and homing vocabulary). While [COMMON.md](../COMMON.md) records the owner as away, no family bench step runs.
