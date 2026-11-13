# MotorControl-CAN implementation prompts 1-71

Execute the numbered files below in ascending **numeric order** among ready work. Finish each ready prompt; if blocked, preserve its remaining scope, advance only to independent work within the assignment, and return when evidence arrives under COMMON. Start at [prompt 1](1_source_audit.md). Names use `1_...md` through `71_...md`; filesystem alphabetical order is not the execution order. [START_HERE.md](START_HERE.md) provides copyable instructions for one prompt, a range or the whole sequence.

Every prompt includes its objective/boundaries, exact prerequisites, targeted source audit, concrete implementation work, tests, documentation, delegation and commit/sync closure. [COMMON.md](COMMON.md) applies in full. Read relevant shared contract sections and source deltas, rather than reloading every historical audit. Complete the current owned work before marking it done; a later integration review is not permission to defer component tests or leave stubs.

The prompt42 user instruction authorizes merging into `main` after its gates and
continuing on `main` until version0 is finished, with no new branches. This takes
precedence over the historical execution-branch instructions for that period.

Create one new branch at the start of a new execution and retain it across all 71 prompts and resumed sessions. Commit and sync after every prompt and useful completed checkpoints. Only the lead changes Git state. A maintenance audit does not start implementation. See root [AGENTS.md](../../AGENTS.md).

Prompts 51-54 were added on 2026-11-12 at the user's request (SYNC master, changes during motion,
multi-axis with configurable capacity and group stop, and their hardware qualification). By the
ascending-order rule they run after prompt 50 unless the user reorders them; their dependencies are
real prerequisites, and prompt 53's group stop and capacity work can start once 45 and 51 are done.

Prompts 55-67 were added later on 2026-11-12 at the user's request: communication self-repair and retry policy,
host throughput, regular enable/release/mode select, measured records from the console, full-range and
reverse velocity, user units, node/bitrate/RPDO/save configuration, the wired stop line, other CAN protocols on
the same bus, a common axis interface for several brands, an optional package for the bus code, and their
hardware qualification on the S2 bench and on ESP32-S3 boards. They also run in ascending order unless the
user reorders them; every feature lands as library API, and the examples consume it.

Prompts 68-71 were added on 2026-11-13 at the user's request for examples of everything an upper firmware does:
a dedicated CAN task with bounded queues and a headless job runner, a hardware watchdog with MCU and
drive I/O, startup checks with settings storage and reboot handling, and their hardware qualification.
They are prepared and host-tested before the hardware exists; the trials wait for the user's wiring.

## Shared contracts

The [bench continuation for prompts 18-33](bench_closure_18_33/README.md) contains
eight new, unexecuted packets for concrete remaining host work and bounded tests
on the current S2/two-drive fixture. It distinguishes implementable gaps from
instrument, firmware and operator-observation requirements. Continue the same
execution branch; this supplement does not renumber or replace prompts 1-50.

The [external coder handoff](archive/handoff_19_33/README.md) for prompts 19–22 and
29–33 has run all 22 packets and is archived for traceability; do not execute it again.
Its [execution table](../reports/handoff_19_33/README.md) records what each packet
delivered, and the [backlog](../backlog.md) tracks the physical trials still owed.

- [CONVENTIONS.md](CONVENTIONS.md): authoritative names, Status/outcomes, owner, health/statistics and CLI behavior.
- [DOCUMENTATION.md](DOCUMENTATION.md): current guides, general project files, meaningful public Doxygen and strict tooling.
- [MAINTAINABILITY.md](MAINTAINABILITY.md): direct code, single ownership, explicit state and justified abstractions.
- [DELEGATION.md](DELEGATION.md): bounded subagents, model/effort choices, disjoint ownership and integrated review.
- [MULTIDRIVE.md](MULTIDRIVE.md): each prompt's stepper/servo and protocol-neutral acceptance criterion.
- [DEVICE_EVIDENCE.md](DEVICE_EVIDENCE.md): focused routes to concrete Lichuan object, motion and timing facts, source conflicts and reviewed public implementation examples. Recheck original sources before using any value.

The first implementation is Lichuan C over Classical CANopen. N5/EPOS4 are test-only design contrasts. Other CAN protocols and CAN FD remain separately implemented future capabilities. The [scope decision](../multidrive_decision.md), [architecture](../architecture_plan.md), [API/CLI map](../api_cli_mapping.md), [requirements](../requirements_matrix.md), [audits](../audit/) and [original references](../reference/README.md) remain authoritative inputs.

## Repair audit

The [prompts 34-50 repair audit](PROMPTSET_AUDIT_34_50_2026-11-02.md) closed W1 to W8 in [packet 10](../reports/handoff_19_33/packet_10.md). It stays the source of its decisions and findings for prompts 33-50, and its W10 and W11 order still applies, as [COMMON.md](COMMON.md) item 5 states.

## Execution order

Prerequisites name the earlier results a prompt consumes, under the usable-prerequisite rule below; the prompt also states any device or fixture evidence gate. All numbers refer to this sequence, not historical stages.

| Prompt | Complete assignment | Prerequisites |
| --- | --- | --- |
| 1 | [Current source audit](1_source_audit.md) | None |
| 2 | [Source inventory and ledger intake](2_source_ledger.md) | 1 |
| 3 | [Reviewed Lichuan semantics and family gates](3_lichuan_semantics.md) | 2 |
| 4 | [Stepper and servo contrast ledger](4_drive_contrasts.md) | 2, 3 |
| 5 | [CANopen service fit experiment](5_stack_decision.md) | 3, 4 |
| 6 | [Common API and ownership proof](6_api_ownership.md) | 5 |
| 7 | [Native build and package foundation](7_native_package.md) | 6 |
| 8 | [Verification CI and documentation gates](8_verification_docs.md) | 7 |
| 9 | [Status and exact units](9_status_units.md) | 6, 8 |
| 10 | [Axis coordinates and references](10_axis_coordinates.md) | 9 |
| 11 | [Classical CAN and SDO codecs](11_can_sdo_codecs.md) | 5, 8, 9 |
| 12 | [Reviewed object generation](12_object_generation.md) | 3, 8, 11 |
| 13 | [Bounded SDO transfer lifecycle](13_sdo_lifecycle.md) | 5, 6, 11 |
| 14 | [SDO ambiguity and reconciliation](14_sdo_reconciliation.md) | 13 |
| 15 | [Passive network events and invalidation](15_network_events.md) | 13, 14 |
| 16 | [NMT requests and service eligibility](16_nmt_services.md) | 15 |
| 17 | [Observed PDO maps and decoding](17_pdo_observations.md) | 12, 15, 16 |
| 18 | [Explicit PDO remapping](18_pdo_configuration.md) | 16, 17 |
| 19 | [Typed identity state and configuration reads](19_typed_reads.md) | 10, 12, 14, 15, 17 |
| 20 | [Driver motion and scale settings](20_driver_settings.md) | 19 |
| 21 | [Digital I/O settings and output actions](21_io_settings.md) | 19, 20 |
| 22 | [Tuning and remaining nonpersistent groups](22_tuning_settings.md) | 19, 20 |
| 23 | [CAN owner dispatch and retained results](23_owner_dispatch.md) | 6, 14, 17, 19 |
| 24 | [Owner urgent capacity and transport recovery](24_owner_recovery.md) | 23 |
| 25 | [Passive health and statistics](25_health_stats.md) | 24 |
| 26 | [Native examples and shared application](26_native_examples.md) | 10, 19, 25 |
| 27 | [TWAI adapter and SDK evidence](27_twai_adapter.md) | 24, 26 |
| 28 | [Arduino and ESP-IDF integration](28_firmware_examples.md) | 27 |
| 29 | [Device evidence readiness and conflict gate](29_device_evidence.md) | 3, 19, 28 |
| 30 | [Drive state enable and release](30_drive_actions.md) | 19, 20, 29 |
| 31 | [Stop operations and owner integration](31_stop_actions.md) | 24, 30 |
| 32 | [Fault clear and device-reference actions](32_fault_reference_actions.md) | 21, 30, 31 |
| 33 | [Finite position operations](33_position_moves.md) | 10, 20, 31, 32 |
| 34 | [Wrapped-angle motion integration](34_angle_moves.md) | 33 |
| 35 | [Finite velocity operations](35_velocity.md) | 31, 33 |
| 36 | [Homing and native coverage reconciliation](36_homing.md) | 31, 32, 35 |
| 37 | [Explicit discovery and binding](37_discovery.md) | 19, 25 |
| 38 | [Explicit communication commissioning](38_communication_commissioning.md) | 24, 37 |
| 39 | [Persistence reset and reconciliation](39_persistence.md) | 24, 38 |
| 40 | [Shared CLI parser and local routes](40_cli_parser.md) | 26, 28 |
| 41 | [Typed device and network CLI routes](41_cli_device_routes.md) | 21, 22, 37, 38, 39, 40 |
| 42 | [Motion CLI and platform parity](42_cli_motion_routes.md) | 34, 35, 36, 41 |
| 43 | [Python Console and evidence primitives](43_python_console.md) | 40 |
| 44 | [Finite Python campaigns and sessions](44_python_campaigns.md) | 42, 43 |
| 45 | [Adversarial ownership and motion integration](45_fault_integration.md) | 36, 39, 44 |
| 46 | [Independent consumers and drive contrasts](46_consumer_contrasts.md) | 4, 10, 28, 42, 45 |
| 47 | [Finite hardware qualification](47_hardware_finite.md) | 29, 44, 45 |
| 48 | [Failure load and related-model qualification](48_hardware_stress.md) | 47 |
| 49 | [User documentation and package audit](49_documentation_package_audit.md) | 42, 44, 46 |
| 50 | [Final independent release-candidate review](50_final_review.md) | 49 |
| 51 | [Host SYNC producer and synchronous PDO operation](51_sync_producer.md) | 17, 24, 38, 42 |
| 52 | [Changes of velocity, ramps and targets during motion](52_motion_changes.md) | 29, 33, 35, 42 |
| 53 | [Multi-axis operation, configurable capacity and group stop](53_multi_axis.md) | 42, 45, 51 |
| 54 | [Hardware qualification of SYNC, motion changes and multi-axis operation](54_hardware_sync_multiaxis.md) | 48, 51, 52, 53 |
| 55 | [Communication self-repair, node presence and retry policy](55_self_repair.md) | 24, 42, 45 |
| 56 | [Host throughput: faster service passes, event wake and owner turn cost](56_host_throughput.md) | 27, 42, 53 |
| 57 | [Regular enable, release, mode select and firmware identity policy](57_regular_drive_operations.md) | 30, 42, 45 |
| 58 | [Measured records through the library API and the console](58_measured_records.md) | 42, 57 |
| 59 | [Velocity in full range and in both directions](59_velocity_range_direction.md) | 35, 52, 58 |
| 60 | [User units for speed and acceleration](60_user_units.md) | 10, 59 |
| 61 | [Node setting, bitrate procedure, command-PDO maps and save trials](61_drive_configuration.md) | 38, 39, 41, 42 |
| 62 | [Wired stop line and host-loss watchdog](62_stop_line.md) | 21, 42, 53 |
| 63 | [Other CAN protocols on the same bus as CANopen](63_mixed_protocols.md) | 46, 55 |
| 64 | [Common axis interface for several drive brands](64_common_axis_interface.md) | 46, 53 |
| 65 | [Optional library package for the bus code and the axis API](65_application_package.md) | 49, 64 |
| 66 | [Hardware qualification of self-repair, regular operations and configuration](66_hardware_regular_operation.md) | 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 |
| 67 | [ESP32-S3, PSRAM and further MCU qualification](67_s3_psram_qualification.md) | 53, 56, 65 |
| 68 | [Application task structure and headless operation examples](68_application_task_structure.md) | 55, 56, 57, 64 |
| 69 | [Application I/O and hardware watchdog examples](69_application_io_watchdog.md) | 62, 68 |
| 70 | [Startup checks, application settings storage and reboot handling examples](70_startup_settings_reboot.md) | 55, 57, 61, 68 |
| 71 | [Hardware qualification of the application examples](71_hardware_application_examples.md) | 66, 69, 70 |
| 72 | [Enable-fault guards: pulse-rate admission, word 7 guard and fault-state reporting](72_enable_fault_guards.md) | 57, 59 |

## Completion and external blockers

Update [PROGRESS.md](PROGRESS.md) and `docs/reports/prompt_N.md` after each prompt. A Complete verdict requires every owned deliverable and required check. Partial/Blocked preserves exactly what remains; it is not successful completion. A prerequisite is usable when the specific artifact consumed exists and its checks passed, regardless of that prompt's state. Start when the consumed artifacts are usable and name them in the report. If an artifact is missing, the dependent feature is Blocked. Complete means the prompt's owned scope and checks closed. Record predecessor gates in an Inherited gates column; they do not change the state.

Owner/adapters in 23-28 precede device-evidence readiness in 29 and motion in 30-36. Prompt 29 completes the host harness and evidence assessment; it cannot invent missing firmware facts. Affected motion behavior remains gated until resolved. Physical qualification in 47-48 requires actual fixture/authorization and remains pending or blocked when unavailable. Prompts 49-50 close as a scoped software candidate listing all inherited gates; they cannot close missing hardware qualification or silently drop blocked native features. Prompt 18 has no dependents: PDO reads belong to 38 and PDO writes stay with 18, integrated through 38's workflow when 18 resumes.

[SEQUENCE.json](SEQUENCE.json) is the machine-checked inventory of IDs, files, dependencies and original-stage ownership. [SPLIT_MAP.md](SPLIT_MAP.md) explains how all previous obligations were divided. The [historical v1 index](archive/v1/README.md) preserves the original 00-19 assignments for traceability; do not execute both versions. [ADD_DRIVE.md](ADD_DRIVE.md) is a later extension playbook, not an extra current prompt.
