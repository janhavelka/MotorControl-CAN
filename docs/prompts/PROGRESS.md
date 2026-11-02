# Prompt execution progress

**Execution identity.** Promptset version 2 on branch `promptset/motorcontrol-can`, base `7878f7c` from `origin/main`, upstream `origin/promptset/motorcontrol-can`. Reuse this branch across every prompt and resumed session. The [source map](../source_map.md) and [prompt 1 inputs](../reports/prompt_1_inputs.json) record the starting point. Earlier narrative is in the [progress history](../reports/progress_history.md).

**Active work.** The [handoff for prompts 19-22 and 29-33](handoff_19_33/README.md) divides the remaining work into 22 packets; its [execution table](../reports/handoff_19_33/README.md) records what each one delivered. The [repair audit](PROMPTSET_AUDIT_34_50_2026-11-02.md) stays active under [COMMON.md](COMMON.md) item 5 until W1 to W8 are done. W1 is delivered by [packet 1](../reports/handoff_19_33/packet_1.md); W2 to W8 are not done, so prompt 33's finite lifecycle and prompts 34-36 stay on hold.

**Rule.** A prerequisite is usable when the specific artifact consumed exists and its checks passed, regardless of that prompt's state. Start when the consumed artifacts are usable and name them in the report. If an artifact is missing, the dependent feature is Blocked. Complete means the prompt's owned scope and checks closed. Record predecessor gates in the Inherited gates column; they do not change the state.

A prompt with an audit-confirmed defect in its owned scope is reopened and Partial until its packet closes it. Allowed states: Pending, In progress, Complete, Partial, Blocked. Feature evidence and physical qualification are separate [coverage](../coverage.md) columns; missing hardware does not become Complete through a fake test. An inherited gate still blocks the feature that depends on it. Q numbers refer to the [device conflict table](../device_conflicts.md).

| Prompt | Work | State | Inherited gates | Report |
| --- | --- | --- | --- | --- |
| 1 | [Current source audit](1_source_audit.md) | Complete | None | [Report](../reports/prompt_1.md) |
| 2 | [Source inventory and ledger intake](2_source_ledger.md) | Complete | None | [Report](../reports/prompt_2.md) |
| 3 | [Reviewed Lichuan semantics and family gates](3_lichuan_semantics.md) | Complete | None | [Report](../reports/prompt_3.md) |
| 4 | [Stepper and servo contrast ledger](4_drive_contrasts.md) | Complete | None | [Report](../reports/prompt_4.md) |
| 5 | [CANopen service fit experiment](5_stack_decision.md) | Complete | None | [Report](../reports/prompt_5.md) |
| 6 | [Common API and ownership proof](6_api_ownership.md) | Complete | None | [Report](../reports/prompt_6.md) |
| 7 | [Native build and package foundation](7_native_package.md) | Complete | None | [Report](../reports/prompt_7.md) |
| 8 | [Verification CI and documentation gates](8_verification_docs.md) | Complete | None | [Report](../reports/prompt_8.md) |
| 9 | [Status and exact units](9_status_units.md) | Complete | None | [Report](../reports/prompt_9.md) |
| 10 | [Axis coordinates and references](10_axis_coordinates.md) | Partial | Reopened by W8 (packet 9) | [Report](../reports/prompt_10.md) |
| 11 | [Classical CAN and SDO codecs](11_can_sdo_codecs.md) | Complete | None | [Report](../reports/prompt_11.md) |
| 12 | [Reviewed object generation](12_object_generation.md) | Complete | None | [Report](../reports/prompt_12.md) |
| 13 | [Bounded SDO transfer lifecycle](13_sdo_lifecycle.md) | Partial | Reopened by W5 (packet 5) | [Report](../reports/prompt_13.md) |
| 14 | [SDO ambiguity and reconciliation](14_sdo_reconciliation.md) | Partial | Reopened by W5 (packet 5) | [Report](../reports/prompt_14.md) |
| 15 | [Passive network events and invalidation](15_network_events.md) | Complete | None | [Report](../reports/prompt_15.md) |
| 16 | [NMT requests and service eligibility](16_nmt_services.md) | Complete | None | [Report](../reports/prompt_16.md) |
| 17 | [Observed PDO maps and decoding](17_pdo_observations.md) | Partial | Reopened by W8 (packet 10) | [Report](../reports/prompt_17.md) |
| 18 | [Explicit PDO remapping](18_pdo_configuration.md) | Partial | Own Q17 gates; no dependents | [Fixed TPDO1 implemented and specimen-observed; remaining Q17 gates](../reports/prompt_18.md) |
| 19 | [Typed identity state and configuration reads](19_typed_reads.md) | Partial | None inherited. Own Q20 gate (W4, packet 4); W8 reopens it (packet 10) | [Raw reads and Q20 probe gate](../reports/prompt_19.md) |
| 20 | [Driver motion and scale settings](20_driver_settings.md) | Partial | Q20 through 19; W8 reopens it (packet 10) | [Native reads/settings and remaining gates](../reports/prompts_20_21_audit.md) |
| 21 | [Digital I/O settings and output actions](21_io_settings.md) | Partial | Q20 through 19 (W4, packet 4) | [Typed I/O reads/DI settings and remaining gates](../reports/prompts_20_21_audit.md) |
| 22 | [Tuning and remaining nonpersistent groups](22_tuning_settings.md) | Partial | Q20 through 19 (W4, packet 4) | [33 typed reads, explicit loop selection and precise write gates](../reports/prompt_22.md) |
| 23 | [CAN owner dispatch and retained results](23_owner_dispatch.md) | Complete | Q20 through 19 (W4, packet 4) | [Owned software complete](../reports/prompt_23.md) |
| 24 | [Owner urgent capacity and transport recovery](24_owner_recovery.md) | Partial | Q20 through 19 (W4, packet 4). W5 and W6 reopen it (packets 5 to 7) | [Owned mechanisms delivered; reopened by W5 and W6](../reports/prompt_24.md) |
| 25 | [Passive health and statistics](25_health_stats.md) | Complete | Q20 through 19 (W4, packet 4) | [Owned software complete](../reports/prompt_25.md) |
| 26 | [Native examples and shared application](26_native_examples.md) | Complete | Q20 through 19 (W4, packet 4). C-10 applies in W6 (packet 7) | [Owned software complete](../reports/prompt_26.md) |
| 27 | [TWAI adapter and SDK evidence](27_twai_adapter.md) | Partial | Q20 through 19 (W4, packet 4). Reopened by W3 (packet 3) and G-04/W9 | [Owned software delivered; reopened by W3 and W9](../reports/prompt_27.md) |
| 28 | [Arduino and ESP-IDF integration](28_firmware_examples.md) | Partial | Q20 through 19 (W4, packet 4). G-04 reopened it for the S2 board; see W9 | [Owned S3 firmware delivered; reopened by G-04/W9 for the S2 board](../reports/prompt_28.md) |
| 29 | [Device evidence readiness and conflict gate](29_device_evidence.md) | Partial | Q20 through 19 (W4, packet 4); W4, W5, W7 and packet 11 before fresh acquisition | [Owned software readiness complete; inherited Q20 and conditional acquisition pending](../reports/prompt_29.md) |
| 30 | [Drive state enable and release](30_drive_actions.md) | Partial | W8 reopens it (packet 10); Q02, Q03 and release evidence | [Checked documentary decoder; enable provenance and release command remain gated](../reports/prompt_30.md) |
| 31 | [Stop operations and owner integration](31_stop_actions.md) | Partial | New: W6, DEC-2, DEC-3, DEC-8 (packets 6, 7); W8 (packet 10); Q02, Q12 | [Conditional native stop lifecycle, owner integration and remaining gates](../reports/prompt_31.md) |
| 32 | [Fault clear and device-reference actions](32_fault_reference_actions.md) | Partial | New: D-26, D-27 guide and Q13 notes, CLEAR_ALARM reads ERRORS once (W6); Q13, Q14 | [Conditional clear/zero operations, retained evidence and coordinate withdrawal](../reports/prompt_32.md) |
| 33 | [Finite position operations](33_position_moves.md) | Partial | W1 to W8 before the finite lifecycle; Q02, Q04, Q05, Q10, Q11, Q12 | [Pure native parameters implemented; finite lifecycle and firmware gates remain](../reports/prompt_33.md) |
| 34 | [Wrapped-angle motion integration](34_angle_moves.md) | Pending | 33; Q04, Q14 | - |
| 35 | [Finite velocity operations](35_velocity.md) | Pending | 33; Q03, Q11. Preparation and timing only until then | - |
| 36 | [Homing and native coverage reconciliation](36_homing.md) | Pending | 33, 35; Q21, Q02, Q04, Q05, Q10, Q11, Q14 | - |
| 37 | [Explicit discovery and binding](37_discovery.md) | Pending | W4 and W5 for `prepareProbe`; Q01 | - |
| 38 | [Explicit communication commissioning](38_communication_commissioning.md) | Pending | Node and heartbeat read rows first; Q16, Q17, Q18 for writes and PDO rows | - |
| 39 | [Persistence reset and reconciliation](39_persistence.md) | Pending | Q15 | - |
| 40 | [Shared CLI parser and local routes](40_cli_parser.md) | Pending | None for local routes. Best next prompt under W10 | - |
| 41 | [Typed device and network CLI routes](41_cli_device_routes.md) | Pending | Follows the APIs each route calls; 40 | - |
| 42 | [Motion CLI and platform parity](42_cli_motion_routes.md) | Pending | Motion APIs for motion routes; 41 | - |
| 43 | [Python Console and evidence primitives](43_python_console.md) | Pending | None for the offline scope; 41 for probe and read routes | - |
| 44 | [Finite Python campaigns and sessions](44_python_campaigns.md) | Pending | Motion routes through 42 | - |
| 45 | [Adversarial ownership and motion integration](45_fault_integration.md) | Pending | Motion APIs for the remaining traces | - |
| 46 | [Independent consumers and drive contrasts](46_consumer_contrasts.md) | Pending | X-01 | - |
| 47 | [Finite hardware qualification](47_hardware_finite.md) | Pending | W9, fixture facts, applicable bench authorization | - |
| 48 | [Failure load and related-model qualification](48_hardware_stress.md) | Pending | 47 | - |
| 49 | [User documentation and package audit](49_documentation_package_audit.md) | Pending | G-09; lists all inherited gates as a scoped software candidate | - |
| 50 | [Final independent release-candidate review](50_final_review.md) | Pending | 49; lists all inherited gates as a scoped software candidate | - |

## Next ready

1. Handoff packets 2 to 10 in numeric order: W2 to W8, then the remaining shared seams.
2. Prompts 40 and 43's offline scope may run in parallel once W1 is done, as audit W10 states. Neither is assigned by the handoff.
3. Physical packets 11 and later follow their own prerequisites and the [bench conditions](../hardware_bench.md).
