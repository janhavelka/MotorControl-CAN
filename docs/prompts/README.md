# MotorControl-CAN implementation promptset

Use [START_HERE.md](START_HERE.md) as a single assignment, or give an agent one numbered prompt at a time. Every stage incorporates [COMMON.md](COMMON.md) and root [AGENTS.md](../../AGENTS.md). **Every stage audits current sibling source again**, including MotorControl-RS, relevant I2C libraries and FieldCore's relevant consumer seam. This is essential because names, health policy, tests and CLI have evolved.

Start each new promptset execution on one new dedicated branch and record it in PROGRESS. Keep it throughout that promptset, including resumed sessions. Commit and sync after every prompt and whenever a coherent tested chunk is ready; do not create a branch per numbered stage.

Every stage now includes concrete implementation contracts, documentation duties and a useful work split, as well as the audit, deliverables and verification gate. Read these shared contracts first:

| Contract | Concrete decisions |
| --- | --- |
| [CONVENTIONS.md](CONVENTIONS.md) | Literal status/outcome/function/owner names; selected application health states, fields, counter/reset rules; folders, CLI and tests |
| [DOCUMENTATION.md](DOCUMENTATION.md) | Stage-owned README/CHANGELOG/general docs; Doxygen units, lifetimes, timing and failure contracts; strict early documentation checks |
| [MAINTAINABILITY.md](MAINTAINABILITY.md) | Direct C++11 code, small responsibilities, explicit state, single ownership, justified abstraction and plain engineering language |
| [DELEGATION.md](DELEGATION.md) | Authorized subagents, task-complexity model choices, disjoint writes, independent review and integrated checks |

The [fresh conventions audit](../audit/CONVENTIONS_REAUDIT.md) distinguishes existing source facts from proposed CAN additions. The instruction to “match the libraries” is resolved into explicit choices; it does not mean forcing their differing I2C lifecycles or health policies onto motor control.

The broadened scope is one library for stepper and servo drives across manufacturers and CAN protocols. Read [the scope decision](../multidrive_decision.md) and mandatory [MULTIDRIVE.md](MULTIDRIVE.md) stage criteria. CANopen/Lichuan remains first; Nanotec N5 and EPOS4 validate the design. Use [ADD_DRIVE.md](ADD_DRIVE.md) for a later concrete profile, without treating research candidates as supported drivers.

The [audits and drive surveys](../audit/), [reference originals](../reference/README.md), [copied baseline](../baseline/README.md), [architecture plan](../architecture_plan.md), [naming map](../api_cli_mapping.md) and [acceptance matrix](../requirements_matrix.md) are inputs. Downloaded manufacturer files are local, with hashes; blocked downloads are listed explicitly. English/Chinese Lichuan manuals and EDS have unresolved conflicts; affected behavior must be reconciled before code can claim it.

## Execution order

| Stage | Prompt | Main dependency and exit |
| --- | --- | --- |
| 00 | [Refresh audits and evidence](00_audit_and_evidence.md) | Establish live baselines and scoped backlog |
| 01 | [Dictionary and family coverage](01_dictionary_and_family.md) | 00; reviewed object/function ledger and conflicts |
| 02 | [Architecture and stack decision](02_architecture_and_stack.md) | 00–01; concrete portable API and service ownership proof |
| 03 | [Build, packaging and CI](03_build_and_ci.md) | 02; offline core/source/install consumer gates |
| 04 | [Statuses, units and axes](04_status_units_axis.md) | 03; exact common semantics and native widths |
| 05 | [Frames and codecs](05_frames_and_codecs.md) | 01–04; checked CAN/SDO/PDO primitives |
| 06 | [SDO service execution](06_sdo_services.md) | 02,05; bounded confirmed service outcomes |
| 07 | [Network, events and PDO](07_network_and_pdo.md) | 05–06; continuous node-event handling |
| 08 | [Typed reads and settings](08_reads_and_settings.md) | 01,04,06–07; native public coverage |
| 09 | [Drive states and actions](09_drive_states_and_actions.md) | 07–08; evidenced enable/release/fault/stop |
| 10 | [Position and angles](10_position_and_angles.md) | 04,08–09; finite positions and completion evidence |
| 11 | [Velocity, homing and native features](11_velocity_homing_native.md) | 09–10; remaining documented motion/native scope |
| 12 | [CAN owner and health](12_can_owner_and_health.md) | 06–11; shared scheduling/ownership and diagnostics |
| 13 | [Discovery and commissioning](13_discovery_commissioning.md) | 08,12; non-changing discovery and explicit changes |
| 14 | [Native and firmware examples](14_examples_and_adapters.md) | 12–13; native plus shared Arduino/IDF builds |
| 15 | [CLI and JSONL](15_cli_and_jsonl.md) | 08–14; strict parser and API parity |
| 16 | [Python runners](16_python_runners.md) | 15; correlated finite campaigns and evidence |
| 17 | [Faults and consumer contracts](17_faults_and_integration.md) | 03–16; adversarial and cross-library regression |
| 18 | [Hardware and family qualification](18_hardware_qualification.md) | Early evidence work after minimal 05–08/adapter; full campaign after 14–17 |
| 19 | [Release and final audit](19_release_and_final_audit.md) | 00–17; 18 required for physical support claims |

Execute sequentially by default. Unit work, documentation or isolated read-only reviews may run in parallel after their contracts are agreed. Stage 12's owner contract is designed in stage 02; earlier sequences can use a fake executor, avoiding a cyclic implementation dependency. Stage 18 has an early evidence phase: after minimal checked services/reads and a focused adapter slice, resolve firmware-dependent facts needed by stages 09–11, then return to implementation. Its full motion campaign still requires stages 14–17. Documentary conflicts can leave individual features gated while unrelated stages proceed. Final release status must preserve those gaps.

## Completion and handoff

Each stage writes `docs/reports/stage_NN.md`, updates [PROGRESS.md](PROGRESS.md), the backlog/roadmap and its relevant coverage. Re-read the previous stage's actual outputs before proceeding. No stage may mark a feature implemented solely because it was added to a table or rejected as unsupported.

Do not ask for routine confirmation between stages after the implementation assignment is authorized. Do not upload, publish, change sibling repositories or move hardware merely because an old RS document describes that workflow. Inspect current Git/remote/toolchain state rather than assuming the initial empty directory is unchanged; state unavailable CI/hardware honestly and continue host work.

Stage 19 can produce an explicitly scoped software release candidate while physical qualification is pending. It cannot call the complete library family-qualified unless all required scope and hardware evidence exist. Hardware results are per model, firmware, motor, configuration and fixture.
