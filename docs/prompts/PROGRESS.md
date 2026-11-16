# Prompt execution progress

Prompt52 delivers gated changes during finite velocity and position operations,
retained original/replacement results, console routes and bounded experiments.
[Report52](../reports/prompt_52.md) and the [measurement tables](../reports/prompt_52_measurements.md)
record the initial velocity, all eight position cases and the moving stop from
003F. Bare writes that had no observed active-curve effect stay closed. This
does not qualify all firmware, ramp laws or finished operations; prompt54 owns
that qualification. Node1 remains quarantined pending an external server-session
boundary, as recorded in the current bench handoff.
Prompt51 remains Partial for same-cycle sample provenance, RPDO buffering and
sustained two-axis10ms qualification; see [report51](../reports/prompt_51.md).
The [fresh prompts50–51 audit](../reports/audit_50_51.md) corrects SYNC routing
and stale-time handling, with adversarial regressions and a source-provenance
correction. These fixes do not close the remaining physical/firmware gates.
The [independent candidate review](../reports/prompt_50.md) and
[documentation/package audit](../reports/prompt_49.md) retain their earlier scope.
Bounded read/load campaigns remain in [report48](../reports/prompt_48.md).
The earlier finite motion and physical observation limits remain in [report47](../reports/prompt_47.md).
Partial rows below retain their distinct software/source/physical obligations.
No current hardware qualification is inferred from a host test or an old bench run.

The [fresh prompts47–49 audit](../reports/audit_47_49.md) rechecks the actual
implementation, raw captures and package. It corrects soak request/record
correlation; prompt47/48 physical obligations and prompt49's software scope
remain unchanged.

The [independent auditor disposition](../audit/2026-11-15_auditor_disposition.md)
checks every numbered finding in the November 2 audit against current code.
It links resolved defects, corrections and concrete owners for remaining work.
Current state belongs in this table; detailed chronology belongs in the linked reports.

**Execution identity.** Promptset version 2 on branch `promptset/motorcontrol-can`, base `7878f7c` from `origin/main`, upstream `origin/promptset/motorcontrol-can`. The user authorized merging prompt42 into `main` and continuing on `main` until version0 is finished; create no new branches during that period. The original execution identity and base remain historical provenance. The [source map](../source_map.md) and [prompt 1 inputs](../reports/prompt_1_inputs.json) record the starting point. Earlier narrative is in the [progress history](../reports/progress_history.md).

**Handoff.** All 22 packets of the [handoff for prompts 19-22 and 29-33](archive/handoff_19_33/README.md) have run, and the handoff is archived. Its [execution table](../reports/handoff_19_33/README.md) records what each one delivered; backlog rows H03 and H04 track the physical trials still owed, under the archived [handoff COMMON](archive/handoff_19_33/COMMON.md)'s physical-test rules. The [repair audit](PROMPTSET_AUDIT_34_50_2026-11-02.md)'s W1 to W8 are closed by packets 1 to 10; see [packet 10](../reports/handoff_19_33/packet_10.md). The audit file remains the source of its decisions. Prompt 33's lifecycle and prompts 34-36 may proceed only for consumed qualified contracts: this qualifies no release command, moving stop, relative wire base or physical scale.

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
| 10 | [Axis coordinates and references](10_axis_coordinates.md) | Complete | None | [Report](../reports/prompt_10.md) |
| 11 | [Classical CAN and SDO codecs](11_can_sdo_codecs.md) | Complete | None | [Report](../reports/prompt_11.md) |
| 12 | [Reviewed object generation](12_object_generation.md) | Complete | None | [Report](../reports/prompt_12.md) |
| 13 | [Bounded SDO transfer lifecycle](13_sdo_lifecycle.md) | Complete | Q18 before a drain marker can reopen a real route. Packet 11 left Q18 unresolved: it needs a slow write and a read seen by a verified listen-only observer | [Report](../reports/prompt_13.md) |
| 14 | [SDO ambiguity and reconciliation](14_sdo_reconciliation.md) | Complete | Q18 before a drain marker can reopen a real route. Packet 11 left Q18 unresolved: it needs a slow write and a read seen by a verified listen-only observer | [Report](../reports/prompt_14.md) |
| 15 | [Passive network events and invalidation](15_network_events.md) | Complete | None | [Report](../reports/prompt_15.md) |
| 16 | [NMT requests and service eligibility](16_nmt_services.md) | Complete | None | [Report](../reports/prompt_16.md) |
| 17 | [Observed PDO maps and decoding](17_pdo_observations.md) | Complete | None | [Report](../reports/prompt_17.md) |
| 18 | [Explicit PDO remapping](18_pdo_configuration.md) | Partial | Own Q17 gates; no dependents | [Fixed TPDO1 implemented and specimen-observed; remaining Q17 gates](../reports/prompt_18.md); bench continuation [packet 7](../reports/bench_closure_18_33/packet_7.md): typed `EVENT_TIMER` `1800:05` raw 0/100 transition with native and bench callers |
| 19 | [Typed identity state and configuration reads](19_typed_reads.md) | Partial | None inherited. Own Q20 gate (accepted list done by packet 4; other rows stay gated) | [Raw reads and Q20 probe gate](../reports/prompt_19.md) |
| 20 | [Driver motion and scale settings](20_driver_settings.md) | Partial | Q20 through 19 (accepted list done by packet 4; other rows stay gated). Packet 12 typed `2202` and kept the `2200`, `2201`, `607D` and `6060` gates; both specimens accepted `2202` codes 1 and 0 with readback on 2026-11-04, after a measured count width for `607D:00` ([backlog D16](../backlog.md)); enforcement is not shown | [Native reads/settings and remaining gates](../reports/prompts_20_21_audit.md); [packet 12](../reports/handoff_19_33/packet_12.md) |
| 21 | [Digital I/O settings and output actions](21_io_settings.md) | Partial | Q20 through 19 (accepted list done by packet 4; other rows stay gated). Packet 13 verified DI writes through the real owner and kept per-field DO, filter and `60FE` write gates. On 2026-11-04 both specimens' I/O configuration was read and node 1 accepted DI3 function and polarity writes; `60FE:00` answered in two bytes ([backlog D16](../backlog.md)), and the `60FE:00..02` readback then completed on both nodes under `CountWidths::MEASURED_U16` (trials 608, 609). P4 needs an operator with meters | [Typed I/O reads/DI settings and remaining gates](../reports/prompts_20_21_audit.md); [packet 13](../reports/handoff_19_33/packet_13.md) |
| 22 | [Tuning and remaining nonpersistent groups](22_tuning_settings.md) | Partial | Q20 through 19 (accepted list done by packet 4; other rows stay gated). Packet 14 verified the `2500` write through the real owner and gated the other 32 rows with their next evidence. Both specimens' 2502..2520 were read on 2026-11-04. Later `2500` 2 to 1 to 2 writes and an open-loop enable experiment are recorded in the [November13 investigation](../reports/2026-11-13_enable_fault_state.md); they do not qualify tuning, and T1 still needs a phase-current probe | [33 typed reads, explicit loop selection and precise write gates](../reports/prompt_22.md); [packet 14](../reports/handoff_19_33/packet_14.md) |
| 23 | [CAN owner dispatch and retained results](23_owner_dispatch.md) | Complete | Q20 through 19 (accepted list done by packet 4; other rows stay gated) | [Owned software complete](../reports/prompt_23.md) |
| 24 | [Owner urgent capacity and transport recovery](24_owner_recovery.md) | Complete | Q20 through 19 (accepted list done by packet 4; other rows stay gated). W5 and W6 applied by packets 5 to 7; C-09 remainder goes to 38 | [Owned mechanisms delivered; reopened by W5 and W6](../reports/prompt_24.md) |
| 25 | [Passive health and statistics](25_health_stats.md) | Complete | Q20 through 19 (accepted list done by packet 4; other rows stay gated) | [Owned software complete](../reports/prompt_25.md) |
| 26 | [Native examples and shared application](26_native_examples.md) | Complete | Q20 through 19 (accepted list done by packet 4; other rows stay gated) | [Owned software complete](../reports/prompt_26.md) |
| 27 | [TWAI adapter and SDK evidence](27_twai_adapter.md) | Partial | Q20 through 19 (accepted list done by packet 4; other rows stay gated). W3 delivered by packet 3; G-04/W9 remainder in backlog T04. Packet 11 left packet 3's questions 1, 4 and 6 unanswered; they wait for prompt 47's listen-only capture. 2 was measured on the 2026-11-03 no-ACK; 3, 5 and 7 are seen only if they occur | [Owned software delivered; reopened by W3 and W9](../reports/prompt_27.md) |
| 28 | [Arduino and ESP-IDF integration](28_firmware_examples.md) | Partial | Q20 through 19 (accepted list done by packet 4; other rows stay gated). G-04 reopened it for the S2 board; see W9 | [Owned S3 firmware delivered; reopened by G-04/W9 for the S2 board](../reports/prompt_28.md); bench continuation [packet 3](../reports/bench_closure_18_33/packet_3.md): shared S2 IDF builds, target pin policy, silent control, configured images boot `INACTIVE`; Arduino S2 blocked by the pinned console |
| 29 | [Device evidence readiness and conflict gate](29_device_evidence.md) | Partial | Q20 through 19 (accepted list done by packet 4; other rows stay gated). Packet 11 ran the live baseline and the read-only soak; state-changing acquisition stays conditional. Packet 19's bench `position` route ran on node 1 in the attended session | [Owned software readiness complete; inherited Q20 and conditional acquisition pending](../reports/prompt_29.md); packets [11](../reports/handoff_19_33/packet_11.md) and [19](../reports/handoff_19_33/packet_19.md) |
| 30 | [Drive state enable and release](30_drive_actions.md) | Partial | Q02, Q03 and release evidence. Packet 15 added a gated enable, closed by default; it succeeded on both specimens in the attended session after the bit 10 fix. Release stays absent | [Checked documentary decoder; enable provenance and release command remain gated](../reports/prompt_30.md); [packet 15](../reports/handoff_19_33/packet_15.md) |
| 31 | [Stop operations and owner integration](31_stop_actions.md) | Partial | Conditional normal/quick-stop, occupied-channel best effort and retained outcomes are implemented. Q02/Q12 firmware, option/queue/reference effects, unmeasured predecessors and the standalone605D read remain. Bench evidence includes moving stops from 001F and 005F on two specimens; it is not a universal stop guarantee. | [Report](../reports/prompt_31.md); [stop contract](../stop_actions.md); [bench record](../hardware_bench.md) |
| 32 | [Fault clear and device-reference actions](32_fault_reference_actions.md) | Partial | Q13, Q14 and standalone2301/2302 reads. Packet 17 made the alarm's fault identity explicitly unresolved (D-26, D-27) and removed the "documented resettable FF05" text; packet 18 added the counter baseline. The counter zero ran once on node 2 on 2026-11-04. The [raw November13 review](../reports/prompt_50_fault_evidence.json) retains initial failed trial5323, nine later explicit `2301` attempts (eight later clear reports, one recurrence), and eleven controlword attempts with released reports in later separate acquisitions. These mode3/FF05 observations qualify no other firmware/fault or physical behavior | [Conditional clear/zero operations, retained evidence and coordinate withdrawal](../reports/prompt_32.md); packets [17](../reports/handoff_19_33/packet_17.md) and [18](../reports/handoff_19_33/packet_18.md) |
| 33 | [Finite position operations](33_position_moves.md) | Partial | Absolute/relative lifecycles and exact target preparation are implemented. Standalone607A typed read remains pending independently of action prerequisite use. Q02/Q04/Q05/Q10/Q11/Q12 acceptance, scale, relative-base, completion and stop facts remain scoped to the selected firmware/evidence. Bench success is distinct from independent physical qualification. | [Report](../reports/prompt_33.md); [position contract](../position_parameters.md); [bench record](../hardware_bench.md) |
| 34 | [Wrapped-angle motion integration](34_angle_moves.md) | Complete | 33; Q04, Q14 and applicable motion evidence; no physical angle qualification | [Exact conversion, existing lifecycle, offline example and checks](../reports/prompt_34.md) |
| 35 | [Finite velocity operations](35_velocity.md) | Partial | Conditional finite lifecycle, preparation-time duration and explicit stop settlement are implemented. Q03/Q11 startup, sign/mode changes, Q12 stops and Q20 general read effects remain. Attended specimen runs and later timeout/stop fixes are retained; wider rates/reverse and final-image qualification remain assigned to 59/66. | [Report](../reports/prompt_35.md); [34/35 audit](../reports/prompts_34_35_audit.md); [bench record](../hardware_bench.md) |
| 36 | [Homing and native coverage reconciliation](36_homing.md) | Partial | Q02/Q04/Q10/Q11/Q14/Q20/Q21: exact firmware, inputs, direction, offset, enable/completion and reference qualification. Admission ran on both nodes on 2026-11-09 with zero downloads: refused `code=1 detail=3` without enable history (913, 915, [2026-11-09 round 2](../reports/2026-11-09_bench_r2.md)), and at the CLOSED gate `code=3 detail=7` after an enable (1107, 1108, [round 4](../reports/2026-11-09_bench_r4.md)). Both specimens read DI0..2 functions 1/2/3, polarity 0 | [Conditional software and remaining evidence](../reports/prompt_36.md); [native reconciliation](../reports/prompt_36_coverage.md) |
| 37 | [Explicit discovery and binding](37_discovery.md) | Complete | Q01/Q20 identity/effect meaning; Q18 before real route reopening. Explicit scans are bounded to the existing two configured slots; arbitrary-node slot recycling is unavailable. Passive, probe, identity and host-selection scans ran on both nodes on 2026-11-09; both identities match the retained record ([2026-11-09 round 2](../reports/2026-11-09_bench_r2.md)) | [Inventory, slot design, finite scans, host selection and checks](../reports/prompt_37.md) |
| 38 | [Explicit communication commissioning](38_communication_commissioning.md) | Partial | Q16 activation/reply route and unknown restart/persistence; Q17 broader PDO timers/maps, Q18 reopening, Q20 effects stay scoped. On 2026-11-09 the 95 reads ran on both nodes, and heartbeat commissioning `1017` 0 to 1000 succeeded on both after the S14 fix (`627bb42`): migration, readback and publication ([round 4](../reports/2026-11-09_bench_r4.md)). Persistence across a power cycle and the drives' host-loss reaction are unestablished | [89 reads, conditional heartbeat, firmware guard and checks](../reports/prompt_38.md) |
| 39 | [Persistence reset and reconciliation](39_persistence.md) | Partial | Q15 full saved set, rearm, restart/route effects and physical qualification; Q20 raw read acquisition | [Conditional workflows and remaining evidence](../reports/prompt_39.md) |
| 40 | [Shared CLI parser and local routes](40_cli_parser.md) | Complete | No device gates consumed; inherited S2 Arduino package limitation remains P28 | [Report](../reports/prompt_40.md) |
| 41 | [Typed device and network CLI routes](41_cli_device_routes.md) | Complete | Exact native/source/firmware prerequisites, Q20 ordinary read policy and Q15 persistence uncertainty; no grant/review created by CLI | [Report](../reports/prompt_41.md) |
| 42 | [Motion CLI and platform parity](42_cli_motion_routes.md) | Complete | Exact native firmware/scale/reference/input qualification; D-17 release, stored-only setup and qualification-install routes remain caps-only gates. Arduino S2 SDK console conflict remains; no hardware qualification claimed | [Report](../reports/prompt_42.md) |
| 43 | [Python Console and evidence primitives](43_python_console.md) | Complete | Existing P41 read qualifications/Q20; no firmware boot identity in schema1; no live qualification | [Report](../reports/prompt_43.md) |
| 44 | [Finite Python campaigns and sessions](44_python_campaigns.md) | Complete | Existing42 native qualification and Q20 gates; hardware campaigns remain47-48 | [Report](../reports/prompt_44.md) |
| 45 | [Adversarial ownership and motion integration](45_fault_integration.md) | Complete software audit | Existing firmware/remapping/physical gates remain with their owners | [Report](../reports/prompt_45.md) |
| 46 | [Independent consumers and drive contrasts](46_consumer_contrasts.md) | Complete software scope | Surveyed profiles remain fixtures; physical/source gates stay with their owners | [Report](../reports/prompt_46.md); [fresh audit](../reports/prompt_46_audit.md) |
| 47 | [Finite hardware qualification](47_hardware_finite.md) | Partial | Authorized unattended S2/two-node acquisition; physical shaft/stop/direction, electrical and method-specific homing evidence remain unavailable | [Campaign, fixes and exact remaining measurements](../reports/prompt_47.md) |
| 48 | [Failure load and related-model qualification](48_hardware_stress.md) | Partial | Physical fault/link/restart observation and OL86-C/CL57-C/OL57-C fixtures unavailable; no save/restore campaign executed | [Finite read/load work and exact pending campaigns](../reports/prompt_48.md) |
| 49 | [User documentation and package audit](49_documentation_package_audit.md) | Complete | Prompt18/Q17 and all retained feature/physical gates; no release publication | [Report](../reports/prompt_49.md) |
| 50 | [Final independent release-candidate review](50_final_review.md) | Complete software review | Earlier Partial software/source/physical gates remain inherited; no whole-library completion or publication | [Independent review and final checks](../reports/prompt_50.md) |
| 51 | [Host SYNC producer and synchronous PDO operation](51_sync_producer.md) | Partial | Consumes 17/24/38/42 artifacts; Q27 sample provenance, Q28 RPDO buffering/withdrawal and M09 map remain open; prompt56 throughput precedes the sustained two-axis 10 ms trial | [Finite producer, typed TPDO1 type experiment and remaining trials](../reports/prompt_51.md) |
| 52 | [Changes of velocity, ramps and targets during motion](52_motion_changes.md) | Complete | Gated velocity/replacement lifecycle, owner/CLI and initial m/p/v measurements delivered; final checks recorded in report52; Q22-Q26 qualification remains open | [Report52](../reports/prompt_52.md) |
| 53 | [Multi-axis operation, configurable capacity and group stop](53_multi_axis.md) | Pending | 42, 45, 51; capacity study, group-stop rules M02, S2 memory B01-B03 | - |
| 54 | [Hardware qualification of SYNC, motion changes and multi-axis operation](54_hardware_sync_multiaxis.md) | Pending | 48, 51, 52, 53; bench authorization and fixture facts | - |
| 55 | [Communication self-repair, node presence and retry policy](55_self_repair.md) | Pending | 24, 42, 45; DEC-3a SDO premise then Q18; one unplug action by the user | - |
| 56 | [Host throughput: faster service passes, event wake and owner turn cost](56_host_throughput.md) | Pending | 27, 42, 53; pass timing measured before changes | - |
| 57 | [Regular enable, release, mode select and firmware identity policy](57_regular_drive_operations.md) | Pending | 30, 42, 45; release trials from mode 1 idle and halted mode 3 | - |
| 58 | [Measured records through the library API and the console](58_measured_records.md) | Pending | 42, 57; records reviewed against raw evidence | - |
| 59 | [Velocity in full range and in both directions](59_velocity_range_direction.md) | Partial | 35, 52, 58; range probe, `2201` write, speed, edge-acceleration, subdivision and node 2 position runs done early on 2026-11-13 ([report](../reports/2026-11-13_velocity_limits_and_fault_recovery.md)); reverse run on node 1 to −3000 rpm, clean (shaft direction unobserved); enable-fault state (S21) investigated, cleared only by a power cycle ([report](../reports/2026-11-13_enable_fault_state.md)), node 2 in it again; node 1 position ladder and records owed | - |
| 60 | [User units for speed and acceleration](60_user_units.md) | Pending | 10, 59; ramp law Q11 from prompt 59 | - |
| 61 | [Node setting, bitrate procedure, command-PDO maps and save trials](61_drive_configuration.md) | Pending | 38, 39, 41, 42; save proof waits for a natural power cycle; not after 18 | - |
| 62 | [Wired stop line and host-loss watchdog](62_stop_line.md) | Pending | 21, 42, 53; trials wait for the user's optocoupler wiring | - |
| 63 | [Other CAN protocols on the same bus as CANopen](63_mixed_protocols.md) | Pending | 46, 55; Lichuan 29-bit behavior measured first | - |
| 64 | [Common axis interface for several drive brands](64_common_axis_interface.md) | Pending | 46, 53; test-only second profile, no placeholder production profile | - |
| 65 | [Optional library package for the bus code and the axis API](65_application_package.md) | Pending | 49, 64; vendor material excluded from packages | - |
| 66 | [Hardware qualification of self-repair, regular operations and configuration](66_hardware_regular_operation.md) | Pending | 54-63; written waiver for an unattended night | - |
| 67 | [ESP32-S3, PSRAM and further MCU qualification](67_s3_psram_qualification.md) | Pending | 53, 56, 65; waits for the user's S3 boards | - |
| 68 | [Application task structure and headless operation examples](68_application_task_structure.md) | Pending | 55, 56, 57, 64; library gaps closed in the API, not in the example | - |
| 69 | [Application I/O and hardware watchdog examples](69_application_io_watchdog.md) | Pending | 62, 68; trials wait for the user's watchdog, optocoupler and switch wiring | - |
| 70 | [Startup checks, application settings storage and reboot handling examples](70_startup_settings_reboot.md) | Pending | 55, 57, 61, 68; startup verifies only unless the table opts in | - |
| 71 | [Hardware qualification of the application examples](71_hardware_application_examples.md) | Pending | 66, 69, 70; hardware trials wait for the user's wiring; S3 after 67 | - |
| 72 | [Enable-fault guards: pulse-rate admission, word 7 guard and fault-state reporting](72_enable_fault_guards.md) | Pending | 57, 59; backlog S21 gaps; its bench steps each need a user power cycle | - |

## Next ready

1. Prompt51 retains its Q27/Q28 and M09 evidence work. The finite producer and
   conditional TPDO1 type setting are usable artifacts; they do not qualify
   synchronous samples or RPDO starts. Prompt56 throughput is required before
   the sustained two-axis 10 ms trial. Continue independent assigned work under
   [COMMON](COMMON.md) while those precise obligations remain visible.
2. Resume earlier Partial rows only for their named unfulfilled scope. Use
   [device conflicts](../device_conflicts.md), the [backlog](../backlog.md) and the
   [bench record](../hardware_bench.md) to select the next required observation.
   Old trial history is not a statement of the currently powered fixture.
3. Prompts47-48 retain physical qualification and prompt48's serial-loss cause.
   Prompts53-72 remain explicit later assignments; 53 owns concurrent application
   axes and 55 owns reviewed communication recovery/retry policy. A corrected
   document does not implement their remaining work.
