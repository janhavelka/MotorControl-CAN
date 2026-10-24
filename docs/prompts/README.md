# MotorControl-CAN implementation prompts 1-50

Execute the numbered files below in ascending **numeric order**, one complete prompt at a time. Start at [prompt 1](1_source_audit.md). Names use `1_...md` through `50_...md`; filesystem alphabetical order is not the execution order. [START_HERE.md](START_HERE.md) provides copyable instructions for one prompt, a range or the whole sequence.

Every prompt includes its objective/boundaries, exact prerequisites, targeted source audit, concrete implementation work, tests, documentation, delegation and commit/sync closure. [COMMON.md](COMMON.md) applies in full. Read relevant shared contract sections and source deltas, rather than reloading every historical audit. Complete the current owned work before marking it done; a later integration review is not permission to defer component tests or leave stubs.

Create one new branch at the start of a new execution and retain it across all 50 prompts and resumed sessions. Commit and sync after every prompt and useful completed checkpoints. Only the lead changes Git state. A maintenance audit does not start implementation. See root [AGENTS.md](../../AGENTS.md).

## Shared contracts

- [CONVENTIONS.md](CONVENTIONS.md): authoritative names, Status/outcomes, owner, health/statistics and CLI behavior.
- [DOCUMENTATION.md](DOCUMENTATION.md): current guides, general project files, meaningful public Doxygen and strict tooling.
- [MAINTAINABILITY.md](MAINTAINABILITY.md): direct code, single ownership, explicit state and justified abstractions.
- [DELEGATION.md](DELEGATION.md): bounded subagents, model/effort choices, disjoint ownership and integrated review.
- [MULTIDRIVE.md](MULTIDRIVE.md): each prompt's stepper/servo and protocol-neutral acceptance criterion.

The first implementation is Lichuan C over Classical CANopen. N5/EPOS4 are test-only design contrasts. Other CAN protocols and CAN FD remain separately implemented future capabilities. The [scope decision](../multidrive_decision.md), [architecture](../architecture_plan.md), [API/CLI map](../api_cli_mapping.md), [requirements](../requirements_matrix.md), [audits](../audit/) and [original references](../reference/README.md) remain authoritative inputs.

## Execution order

Prerequisites name completed earlier software/design results; the prompt also states any device or fixture evidence gate. All numbers refer to this sequence, not historical stages.

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
| 23 | [CAN owner dispatch and retained results](23_owner_dispatch.md) | 6, 14, 18, 19, 20, 21, 22 |
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
| 37 | [Non-changing discovery and binding](37_discovery.md) | 19, 25 |
| 38 | [Explicit communication commissioning](38_communication_commissioning.md) | 18, 24, 37 |
| 39 | [Persistence reset and reconciliation](39_persistence.md) | 24, 38 |
| 40 | [Shared CLI parser and local routes](40_cli_parser.md) | 26, 28 |
| 41 | [Typed device and network CLI routes](41_cli_device_routes.md) | 21, 22, 37, 38, 39, 40 |
| 42 | [Motion CLI and platform parity](42_cli_motion_routes.md) | 34, 35, 36, 41 |
| 43 | [Python Console and evidence primitives](43_python_console.md) | 42 |
| 44 | [Finite Python campaigns and sessions](44_python_campaigns.md) | 43 |
| 45 | [Adversarial ownership and motion integration](45_fault_integration.md) | 36, 39, 44 |
| 46 | [Independent consumers and drive contrasts](46_consumer_contrasts.md) | 4, 10, 28, 42, 45 |
| 47 | [Finite hardware qualification](47_hardware_finite.md) | 29, 44, 45, 46 |
| 48 | [Failure load and related-model qualification](48_hardware_stress.md) | 47 |
| 49 | [User documentation and package audit](49_documentation_package_audit.md) | 42, 44, 46 |
| 50 | [Final independent release-candidate review](50_final_review.md) | 49 |

## Completion and external blockers

Update [PROGRESS.md](PROGRESS.md) and `docs/reports/prompt_N.md` after each prompt. A Complete verdict requires every owned deliverable and required check. Partial/Blocked preserves exactly what remains; it is not successful completion. Continue an independent later prompt only when its prerequisites are satisfied.

Owner/adapters in 23-28 precede device-evidence readiness in 29 and motion in 30-36. Prompt 29 completes the host harness and evidence assessment; it cannot invent missing firmware facts. Affected motion behavior remains gated until resolved. Physical qualification in 47-48 requires actual fixture/authorization and remains pending or blocked when unavailable. Prompts 49-50 can still produce an honestly scoped software candidate; they cannot close missing hardware qualification or silently drop blocked native features.

[SEQUENCE.json](SEQUENCE.json) is the machine-checked inventory of IDs, files, dependencies and original-stage ownership. [SPLIT_MAP.md](SPLIT_MAP.md) explains how all previous obligations were divided. The [historical v1 index](archive/v1/README.md) preserves the original 00-19 assignments for traceability; do not execute both versions. [ADD_DRIVE.md](ADD_DRIVE.md) is a later extension playbook, not an extra current prompt.
