# Prompt execution progress

This tracks future **library implementation**, not promptset maintenance. Execution has not started; all 50 prompts remain Pending. The old 00-19 labels were superseded before implementation, so no completion status was transferred.

At execution start, record promptset version 2, its new dedicated branch, inspected base commit, upstream and source-map revision here. Reuse that branch for all prompts and resumed sessions. Reports use `docs/reports/prompt_N.md`. Commit and sync after each prompt and useful completed checkpoints.

Allowed states: Pending, In progress, Complete, Partial, Blocked. Complete means the prompt's entire owned scope and required checks passed. Feature evidence and physical qualification are separate coverage columns; missing hardware does not become Complete through a fake test. See [COMMON.md](COMMON.md) and [execution order](README.md).

| Prompt | Work | State | Report |
| --- | --- | --- | --- |
| 1 | [Current source audit](1_source_audit.md) | Pending | - |
| 2 | [Source inventory and ledger intake](2_source_ledger.md) | Pending | - |
| 3 | [Reviewed Lichuan semantics and family gates](3_lichuan_semantics.md) | Pending | - |
| 4 | [Stepper and servo contrast ledger](4_drive_contrasts.md) | Pending | - |
| 5 | [CANopen service fit experiment](5_stack_decision.md) | Pending | - |
| 6 | [Common API and ownership proof](6_api_ownership.md) | Pending | - |
| 7 | [Native build and package foundation](7_native_package.md) | Pending | - |
| 8 | [Verification CI and documentation gates](8_verification_docs.md) | Pending | - |
| 9 | [Status and exact units](9_status_units.md) | Pending | - |
| 10 | [Axis coordinates and references](10_axis_coordinates.md) | Pending | - |
| 11 | [Classical CAN and SDO codecs](11_can_sdo_codecs.md) | Pending | - |
| 12 | [Reviewed object generation](12_object_generation.md) | Pending | - |
| 13 | [Bounded SDO transfer lifecycle](13_sdo_lifecycle.md) | Pending | - |
| 14 | [SDO ambiguity and reconciliation](14_sdo_reconciliation.md) | Pending | - |
| 15 | [Passive network events and invalidation](15_network_events.md) | Pending | - |
| 16 | [NMT requests and service eligibility](16_nmt_services.md) | Pending | - |
| 17 | [Observed PDO maps and decoding](17_pdo_observations.md) | Pending | - |
| 18 | [Explicit PDO remapping](18_pdo_configuration.md) | Pending | - |
| 19 | [Typed identity state and configuration reads](19_typed_reads.md) | Pending | - |
| 20 | [Driver motion and scale settings](20_driver_settings.md) | Pending | - |
| 21 | [Digital I/O settings and output actions](21_io_settings.md) | Pending | - |
| 22 | [Tuning and remaining nonpersistent groups](22_tuning_settings.md) | Pending | - |
| 23 | [CAN owner dispatch and retained results](23_owner_dispatch.md) | Pending | - |
| 24 | [Owner urgent capacity and transport recovery](24_owner_recovery.md) | Pending | - |
| 25 | [Passive health and statistics](25_health_stats.md) | Pending | - |
| 26 | [Native examples and shared application](26_native_examples.md) | Pending | - |
| 27 | [TWAI adapter and SDK evidence](27_twai_adapter.md) | Pending | - |
| 28 | [Arduino and ESP-IDF integration](28_firmware_examples.md) | Pending | - |
| 29 | [Device evidence readiness and conflict gate](29_device_evidence.md) | Pending | - |
| 30 | [Drive state enable and release](30_drive_actions.md) | Pending | - |
| 31 | [Stop operations and owner integration](31_stop_actions.md) | Pending | - |
| 32 | [Fault clear and device-reference actions](32_fault_reference_actions.md) | Pending | - |
| 33 | [Finite position operations](33_position_moves.md) | Pending | - |
| 34 | [Wrapped-angle motion integration](34_angle_moves.md) | Pending | - |
| 35 | [Finite velocity operations](35_velocity.md) | Pending | - |
| 36 | [Homing and native coverage reconciliation](36_homing.md) | Pending | - |
| 37 | [Non-changing discovery and binding](37_discovery.md) | Pending | - |
| 38 | [Explicit communication commissioning](38_communication_commissioning.md) | Pending | - |
| 39 | [Persistence reset and reconciliation](39_persistence.md) | Pending | - |
| 40 | [Shared CLI parser and local routes](40_cli_parser.md) | Pending | - |
| 41 | [Typed device and network CLI routes](41_cli_device_routes.md) | Pending | - |
| 42 | [Motion CLI and platform parity](42_cli_motion_routes.md) | Pending | - |
| 43 | [Python Console and evidence primitives](43_python_console.md) | Pending | - |
| 44 | [Finite Python campaigns and sessions](44_python_campaigns.md) | Pending | - |
| 45 | [Adversarial ownership and motion integration](45_fault_integration.md) | Pending | - |
| 46 | [Independent consumers and drive contrasts](46_consumer_contrasts.md) | Pending | - |
| 47 | [Finite hardware qualification](47_hardware_finite.md) | Pending | - |
| 48 | [Failure load and related-model qualification](48_hardware_stress.md) | Pending | - |
| 49 | [User documentation and package audit](49_documentation_package_audit.md) | Pending | - |
| 50 | [Final independent release-candidate review](50_final_review.md) | Pending | - |
