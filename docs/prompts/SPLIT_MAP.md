# Scope preservation and sizing map

Version 2 replaces the former 20 assignments with 50 complete numbered prompts. This is a workload split, not a feature reduction. Each prompt owns implementation, focused tests, affected docs/Doxygen and commit/sync for its bounded result. Shared contracts remain canonical. The original files below are historical references, not required extra reading during execution.

## Original scope to new owners

| Original stage | Current prompts | Reason for split |
| --- | --- | --- |
| [00](archive/v1/00_audit_and_evidence.md) | [1](1_source_audit.md), [2](2_source_ledger.md) | Source conventions and reference/ledger intake are distinct evidence deliverables. |
| [01](archive/v1/01_dictionary_and_family.md) | [2](2_source_ledger.md), [3](3_lichuan_semantics.md), [4](4_drive_contrasts.md) | Separate inventory, reviewed Lichuan semantics and alternate-drive contrasts. |
| [02](archive/v1/02_architecture_and_stack.md) | [5](5_stack_decision.md), [6](6_api_ownership.md) | Stack experiment precedes the API/ownership proof and freeze. |
| [03](archive/v1/03_build_and_ci.md) | [7](7_native_package.md), [8](8_verification_docs.md) | Usable native packaging precedes verifier/CI and strict documentation tooling. |
| [04](archive/v1/04_status_units_axis.md) | [9](9_status_units.md), [10](10_axis_coordinates.md) | Exact arithmetic/status types precede axis/reference composition. |
| [05](archive/v1/05_frames_and_codecs.md) | [11](11_can_sdo_codecs.md), [12](12_object_generation.md), [15](15_network_events.md), [17](17_pdo_observations.md) | Frame/SDO codecs, object generation, network event codecs and PDO decoding have different inputs and tests. |
| [06](archive/v1/06_sdo_services.md) | [13](13_sdo_lifecycle.md), [14](14_sdo_reconciliation.md) | A safe bounded transfer lifecycle precedes additional reconciliation and adversarial proof. |
| [07](archive/v1/07_network_and_pdo.md) | [15](15_network_events.md), [16](16_nmt_services.md), [17](17_pdo_observations.md), [18](18_pdo_configuration.md) | Passive events, explicit NMT, map observations and remapping are separate service contracts. |
| [08](archive/v1/08_reads_and_settings.md) | [19](19_typed_reads.md), [20](20_driver_settings.md), [21](21_io_settings.md), [22](22_tuning_settings.md) | Typed read infrastructure and three finite native setting groups close distinct ledger rows. |
| [09](archive/v1/09_drive_states_and_actions.md) | [30](30_drive_actions.md), [31](31_stop_actions.md), [32](32_fault_reference_actions.md) | Separate drive-state enable/release, stop, and fault/reference effects. |
| [10](archive/v1/10_position_and_angles.md) | [33](33_position_moves.md), [34](34_angle_moves.md) | Position lifecycle precedes wrapped-angle composition and contrast cases. |
| [11](archive/v1/11_velocity_homing_native.md) | [21](21_io_settings.md), [22](22_tuning_settings.md), [32](32_fault_reference_actions.md), [35](35_velocity.md), [36](36_homing.md) | Move native setting/action rows to their named groups; velocity and homing each have their own deadline/reference contract. |
| [12](archive/v1/12_can_owner_and_health.md) | [23](23_owner_dispatch.md), [24](24_owner_recovery.md), [25](25_health_stats.md) | Ordinary owner dispatch, urgent/recovery handling and passive health are separate deliverables before motion. |
| [13](archive/v1/13_discovery_commissioning.md) | [37](37_discovery.md), [38](38_communication_commissioning.md), [39](39_persistence.md) | Separate non-changing discovery, explicit route changes and persistent/reset effects. |
| [14](archive/v1/14_examples_and_adapters.md) | [26](26_native_examples.md), [27](27_twai_adapter.md), [28](28_firmware_examples.md) | Native/shared app, controller adapter and firmware integrations have independent verification. |
| [15](archive/v1/15_cli_and_jsonl.md) | [40](40_cli_parser.md), [41](41_cli_device_routes.md), [42](42_cli_motion_routes.md) | Parser/local routes precede device routes, then motion routes and complete parity. |
| [16](archive/v1/16_python_runners.md) | [43](43_python_console.md), [44](44_python_campaigns.md) | Console/evidence primitives precede complete finite campaign/session composition. |
| [17](archive/v1/17_faults_and_integration.md) | [45](45_fault_integration.md), [46](46_consumer_contrasts.md) | Separate adversarial ownership/motion integration from consumer/contrast/coverage validation. |
| [18](archive/v1/18_hardware_qualification.md) | [29](29_device_evidence.md), [47](47_hardware_finite.md), [48](48_hardware_stress.md) | Place host evidence readiness before motion; split finite qualification from failure/load/family campaigns. |
| [19](archive/v1/19_release_and_final_audit.md) | [49](49_documentation_package_audit.md), [50](50_final_review.md) | User/package documentation audit precedes independent final candidate verification. |

## Completion boundaries

- Prompts 2-3 give each dictionary/function row an explicit owner. Prompts 19-22 close named read/settings groups; 30-39 close actions/motion/commissioning. The reconciliation in 36 and audit in 46 locate gaps; they are not catch-all implementation assignments.
- Prompt 13 is already correct for its supported service subset: immutable deadlines, explicit uncertainty, retained results and unresolved-channel protection. Prompt 14 adds justified reconciliation and adversarial proof, not delayed basic correctness.
- Prompts 23-24 implement owner dispatch and urgent/recovery mechanics with real service contracts and test-only work fixtures. Prompt 31 integrates the actual stop operation. No fake installed motion driver is needed to compile earlier work.
- Prompt 29 closes host evidence readiness only. Unresolved firmware facts still gate affected behavior in 30-36. Prompts 47-48 close physical qualification only with real evidence; software candidate work is independent and reports gaps.
- Each CLI stage publishes only implemented commands. JSONL/parser and native tests precede final shared-platform parity. Python Console/evidence precede campaign orchestration; all six established runner modules retain their responsibilities.
- Prompt 8 activates strict docs/version/repository gates; prompt 12 activates catalogue freshness with the real generator. Every new public declaration is documented when introduced, including the initial package in 7.

## Reading and task size

Size is judged by independent state machines, ownership domains, platform work and acceptance criteria, not prose length alone. The old individual files were 649-988 words, yet some covered entire subsystems; six shared files added 11,254 words before root guidance and source material. The new files target one reviewable outcome with a short reading route. No artificial word cap permits deleting required behavior or documentation.

Read root/COMMON and the relevant contract sections, prerequisite reports and live source delta. Do not load this historical map or all old prompts for each implementation task. A fresh session reloads the necessary contracts; all shared requirements still apply.
