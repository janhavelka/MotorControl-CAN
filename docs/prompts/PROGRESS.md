# Prompt execution progress

Prompt 32 implements [conditional fault clearing and device zero](../fault_reference_actions.md)
through the existing action/owner path. Original errors, command prefixes and
coordinate invalidation remain independent of reported completion. It stays
**Partial** for the separate controlword reset route, broader fault/state and
firmware evidence, plus inherited 21/30/31 obligations. See the
[report](../reports/prompt_32.md) for exact implementation and checks.

Prompt 31 adds the [conditional halt/quick-stop lifecycle](../stop_actions.md)
and real-owner fake integration. Single native commands, fresh not-running
reports and original interrupted results remain separate. It is **Partial**:
broader predecessor states, mode/option consequences and firmware qualification
remain gated, alongside inherited 24/30 obligations. See its
[report](../reports/prompt_31.md) for exact scope and verification.

A [fresh independent audit of prompts 29/30](../reports/prompts_29_30_audit.md)
found no runtime or source-transcription defect in the delivered scope. It adds
checked state-publication regressions for overlapping output/current storage and
an actual owner-dispatched boot-up after successful acquisition, and corrects
the stale R35 coverage summary. Both stages retain their precise Partial status;
no action or firmware qualification is supplied by these tests.

Prompt 30 adds [checked documentary state interpretation](../drive_actions.md),
with explicit source layouts, retained raw/provenance and disagreement reporting.
It remains **Partial**: the documented position-enable sequence needs producible
initial/retained-motion provenance, and no source establishes the release command.
The [report](../reports/prompt_30.md) keeps real action APIs and their acceptance
tests pending; this decoder grants no firmware qualification or motion permission.

The [2026-11-02 connected-board intake](../reports/2026-11-02_s2_bench_intake.md)
records the user's board-test authorization, EasyEDA power/debug/CAN net review,
resolved ESP-Prog2 COM15 conflict, successful UART/automatic-boot ROM identity and
reported drive DIP/termination settings. Portable-core S2/P4
compile/link experiments pass; complete S2/P4 adapter support and physical tests
remain pending. This advances prompt 29's fixture intake without closing Q20 or
changing motion-stage status.

Prompt 29 delivers [finite device evidence readiness](../reports/prompt_29.md):
typed native/firmware acquisition through the same owner, exact retained plans,
raw traffic/failures and a complete conflict-to-measurement table. Local full,
package, Doxygen, Arduino and IDF checks pass. Its owned software scope is done;
inherited 19/28 Q20 and conditional acquisition keep the stage Partial. No
physical measurement, state-changing experiment or production gate closure is
claimed. Continue with independent ready work; resume the precise evidence
gates when fixture, review and applicable authorization exist.

A [fresh independent prompts 27/28 audit](../reports/prompts_27_28_audit.md)
corrects a captured-RX handoff race, oversized GPIO validation, stale clock-fault
invalidation and SDK checker/
failure-evidence gaps. The shared firmware/owner boundary remains unchanged;
both stages retain their inherited Q20 Partial status.

Prompt 28 delivers [shared Arduino/IDF firmware](../reports/prompt_28.md),
bounded USB/time, actual firmware and isolated-component builds and compile CI.
Its owned software and local/Windows/GCC/Clang-sanitizer/Arduino/IDF CI checks
pass; inherited 27/24/26/19 Q20 keeps it Partial.
Prompt 29 now supplies the offline evidence harness; physical acquisition still
needs a fixture and applicable authorization.

A [fresh independent prompts 25/26 audit](../reports/prompts_25_26_audit.md)
corrects lost delivery history for qualified expired PDO samples. Capture age
still gates READY; service counters, uncertainty and stage blockers are unchanged.
Application/retention/package review found no additional confirmed defect.

Prompt 24 implements its [urgent/cancel/recovery owner mechanisms](../reports/prompt_24.md)
over the existing single service store. Its status remains Partial only because
23 inherits 19/Q20's non-consuming probe gate. That gate does not defer any
Prompt 24 owner mechanism or turn controller recovery into remote requalification.

A [fresh independent prompt-19 audit](../reports/prompt_19_audit.md) requires
explicit ownership for every readable ledger row, preventing silent loss of a
read obligation. Lifecycle/source/package review found no runtime defect.
Prompt 19 remains Partial on Q20's non-consuming probe evidence.

A [fresh independent 22/23 audit](../reports/prompts_22_23_audit.md) closes the
real-owner tuning failure/retention test gap and removes unused duplicate request
storage. The finite native selector contract and existing evidence gates remain
unchanged; 22 and 23 retain their recorded Partial status.

This tracks **library implementation assignments**, including their source-audit foundation. Execution started 2026-10-28 with the user's pasted former Stage 00, recorded as current prompt 1 plus its requested original-reference checks. Prompts 2Ã¢â‚¬â€œ4 completed intake, Lichuan documentary semantics and test-only drive contrasts. Prompt 5 selected CANopenNode through a native experiment. Prompt 6 completed concrete common API/ownership contracts and a checked native proof. Prompt 7 added the minimal native package, version metadata and registered proofs. Prompt 8 completed quick/full verification, strict Doxygen, Windows quick CI and Linux GCC/Clang sanitizer CI; prompt 9 completed production exact units/quantization and proof migration with local and sanitizer CI checks; prompt 10 completed pure axis/reference targets, offline preview and local/CI verification; prompt 11 completed pure frame/SDO checks, independent vectors and local/Windows/GCC/Clang sanitizer verification; prompt 12 completed reviewed object generation, active freshness and local/Windows/GCC/Clang sanitizer verification; prompt 13 completed the bounded selected-stack SDO lifecycle with local/Windows/GCC/Clang sanitizer verification; prompt 14 completed ambiguity settlement and timestamped observations with local/Windows/GCC/Clang sanitizer verification; prompt 15 completed passive network events and node/cache invalidation; prompt 16 completes explicit NMT and service eligibility; prompt 17 completes checked PDO maps, acquisition and qualified passive observations; prompt 18 is Blocked on the Lichuan PDO mutation contract; prompt 19 is Partial (raw reads complete; Q20 probe evidence missing); prompt 20 is Partial (nine native reads and confirmed parameter settings complete; direction/scale/limits/mode gates remain); prompt 21 is Partial (23 I/O reads and confirmed DI settings complete; DO/filter/action gates remain); prompt 22 is Partial (all 33 native reads and loop selection complete; remaining admissible-domain/scale/effect facts missing); prompt 23 delivers its ordinary-owner software (Partial only on inherited 19/Q20); prompt 24 implements urgent/cancellation/recovery mechanisms (Partial only on inherited 23/19/Q20); prompt 25 implements passive health, exact counters and bounded copied diagnostics (Partial only on inherited 24/23/19/Q20); prompt 26 delivers the shared native application and fake demonstration (Partial only on inherited 19/25/Q20); prompt 27 implements the pinned TWAI adapter (Partial only on inherited 24/26/19/Q20); prompt 28 delivers shared Arduino/IDF integration (Partial only on inherited 27/24/26/19/Q20); prompt 29 delivers finite evidence readiness (Partial on inherited Q20 and conditional acquisition); prompt 30 implements checked documentary state interpretation (Partial on enable/release action evidence); prompt 31 implements conditional native stops and owner integration (Partial on broader transition/firmware gates); prompt 32 adds conditional fault clear and counter zero (Partial on the separate reset/firmware gates); prompts 33-50 remain Pending. No old status was transferred.

Execution identity: promptset version 2; branch `promptset/motorcontrol-can`; inspected base `7878f7c` from updated `origin/main`; destination `origin/promptset/motorcontrol-can` (set on first push). Source-map inputs and exact base are in [prompt 1 inputs](../reports/prompt_1_inputs.json), and the current map is [source_map.md](../source_map.md). Reuse this branch across every prompt and resumed session. Reports use `docs/reports/prompt_N.md`; commit and sync each prompt and useful complete checkpoint.

Fresh independent audit of prompts 4-5 on 2026-10-28 corrected two source
interpretations and completed the missing ramp/capability fixture groups in
[prompt 4](../reports/prompt_4.md#fresh-independent-audit-after-prompt-5).
[Prompt 5's code and evidence audit](../reports/prompt_5.md#fresh-independent-audit)
found no owned-scope runtime defect. Both remain Complete. The
[prompt-6 proof and independent review](../reports/prompt_6.md) now close the
common semantic boundary; production package, gates, units and axis policy now
are implemented under prompts 7-10.

A [fresh independent prompt-6 audit](../reports/prompt_6.md#fresh-independent-audit)
rechecked the actual code and original acceptance criteria. It clarified fixed
owner work budgets; native, documentation and integrity checks passed. Prompt 6
remains Complete, with no newly discovered runtime defect or scope blocker.

A [fresh independent audit of prompts 7-9](../reports/prompts_7_9_audit.md)
corrected verifier omission/empty-version reporting and package/numeric
documentation. Arithmetic and isolated package reviews found no functional
defect. Prompts 7-9 retain Complete status. Prompt 10 completed production pure
axis/reference APIs, offline preview and
local plus Windows/GCC/Clang sanitizer CI; [its report](../reports/prompt_10.md)
records sources, corrections, checks and synchronization. Prompt 11 is complete; [its report](../reports/prompt_11.md) records pure checks, source review, independent vectors and local/CI evidence. Prompt 13 subsequently completed the bounded selected-stack lifecycle; [its report](../reports/prompt_13.md) records source review, independent vectors and local/CI evidence. Prompt 14 is complete; [its report](../reports/prompt_14.md) records precedence, adversarial tests, unsupported reopening gates and local/CI evidence. Prompt 15 adds passive codecs, shared node evidence and synchronous invalidation; [its report](../reports/prompt_15.md) records the scoped passive-boundary decision and actual checks. Prompt 16 adds explicit NMT intent, native producer progression, broadcast invalidation and service eligibility; [its report](../reports/prompt_16.md) records checks. Prompt 17 adds pure layouts, reviewed generated templates, actual-map evidence and independent sample provenance; [its report](../reports/prompt_17.md) records checks. Prompt 18 completed its evidence review but remains Blocked; [its report](../reports/prompt_18.md) records the missing Q17 contract. Prompt 19 implements its 30 raw read rows; the non-consuming probe remains Q20 gated. See [its report](../reports/prompt_19.md).

Allowed states: Pending, In progress, Complete, Partial, Blocked. Complete means the prompt's entire owned scope and required checks passed. Feature evidence and physical qualification are separate coverage columns; missing hardware does not become Complete through a fake test. See [COMMON.md](COMMON.md) and [execution order](README.md).

The [fresh Prompt 18 audit](../reports/prompt_18.md#fresh-independent-audit)
corrected its blocker rationale: standard remapping is documented by CiA;
selected-model applicability is unresolved. Q17 now separates mutation, timing
changes and sample qualification. No runtime defect or additional supported
Lichuan subset was found; Prompt 18 remains Blocked.

A [fresh independent audit of prompts 10-11](../reports/prompts_10_11_audit.md)
found no production defect or unfulfilled feature requirement. It corrected
a retained-proof comment and an ADS1115 source citation; independent numeric,
protocol and package probes plus integrated full/Doxygen checks passed. Both
stages remain Complete; prompt 12 subsequently completed.

A [fresh independent prompt-12 audit](../reports/prompt_12_audit.md) corrected
catalogue Unicode escaping and two generated coverage omissions. Original
source facts, declaration permissions and semantic coverage remain unchanged.

A [fresh independent prompt-13 audit](../reports/prompt_13_audit.md) corrected
server-abort handling during segment staging and delayed controller TX reports.
Independent vectors verify retirement, quarantine, fixed cutoffs and retained
terminals. The original stage remains Complete with the checked corrections.

A [fresh independent prompt-14 audit](../reports/prompt_14_audit.md) clarified
freshness for individual segment exchanges and added upload cancellation and
toggle-wrap ambiguity regressions. No runtime defect was reproduced; Prompt 14
remains Complete with the strengthened coverage and unchanged device gates.

A [fresh independent audit of prompts 15-17](../reports/prompts_15_17_audit.md)
corrects initial SDO proof timestamps, independent history/proof ordering and
processing-only receive provenance. Passive event, NMT and PDO scope remains
Complete; no device permission or hardware qualification changed.

Use numerical order among ready work within the assignment. Record the blocker and completed artifacts before progressing to independent work; return when evidence arrives. Prompts 18/20-22 may stay Partial while the independent owner/adapter path reaches the evidence harness in 29. Their typed features and owner integration remain owed to their original prompts. A partial predecessor or dependent chunk is never marked Complete merely to continue.

| Prompt | Work | State | Report |
| --- | --- | --- | --- |
| 1 | [Current source audit](1_source_audit.md) | Complete | [Report](../reports/prompt_1.md) |
| 2 | [Source inventory and ledger intake](2_source_ledger.md) | Complete | [Report](../reports/prompt_2.md) |
| 3 | [Reviewed Lichuan semantics and family gates](3_lichuan_semantics.md) | Complete | [Report](../reports/prompt_3.md) |
| 4 | [Stepper and servo contrast ledger](4_drive_contrasts.md) | Complete | [Report](../reports/prompt_4.md) |
| 5 | [CANopen service fit experiment](5_stack_decision.md) | Complete | [Report](../reports/prompt_5.md) |
| 6 | [Common API and ownership proof](6_api_ownership.md) | Complete | [Report](../reports/prompt_6.md) |
| 7 | [Native build and package foundation](7_native_package.md) | Complete | [Report](../reports/prompt_7.md) |
| 8 | [Verification CI and documentation gates](8_verification_docs.md) | Complete | [Report](../reports/prompt_8.md) |
| 9 | [Status and exact units](9_status_units.md) | Complete | [Report](../reports/prompt_9.md) |
| 10 | [Axis coordinates and references](10_axis_coordinates.md) | Complete | [Report](../reports/prompt_10.md) |
| 11 | [Classical CAN and SDO codecs](11_can_sdo_codecs.md) | Complete | [Report](../reports/prompt_11.md) |
| 12 | [Reviewed object generation](12_object_generation.md) | Complete | [Report](../reports/prompt_12.md) |
| 13 | [Bounded SDO transfer lifecycle](13_sdo_lifecycle.md) | Complete | [Report](../reports/prompt_13.md) |
| 14 | [SDO ambiguity and reconciliation](14_sdo_reconciliation.md) | Complete | [Report](../reports/prompt_14.md) |
| 15 | [Passive network events and invalidation](15_network_events.md) | Complete | [Report](../reports/prompt_15.md) |
| 16 | [NMT requests and service eligibility](16_nmt_services.md) | Complete | [Report](../reports/prompt_16.md) |
| 17 | [Observed PDO maps and decoding](17_pdo_observations.md) | Complete | [Report](../reports/prompt_17.md) |
| 18 | [Explicit PDO remapping](18_pdo_configuration.md) | Blocked | [Evidence and remaining implementation](../reports/prompt_18.md) |
| 19 | [Typed identity state and configuration reads](19_typed_reads.md) | Partial | [Raw reads and Q20 probe gate](../reports/prompt_19.md) |
| 20 | [Driver motion and scale settings](20_driver_settings.md) | Partial | [Native reads/settings and remaining gates](../reports/prompts_20_21_audit.md) |
| 21 | [Digital I/O settings and output actions](21_io_settings.md) | Partial | [Typed I/O reads/DI settings and remaining gates](../reports/prompts_20_21_audit.md) |
| 22 | [Tuning and remaining nonpersistent groups](22_tuning_settings.md) | Partial | [33 typed reads, explicit loop selection and precise write gates](../reports/prompt_22.md) |
| 23 | [CAN owner dispatch and retained results](23_owner_dispatch.md) | Partial | [Owned software complete; inherited 19/Q20 gate](../reports/prompt_23.md) |
| 24 | [Owner urgent capacity and transport recovery](24_owner_recovery.md) | Partial | [Owned mechanisms complete; inherited 23/19/Q20 gate](../reports/prompt_24.md) |
| 25 | [Passive health and statistics](25_health_stats.md) | Partial | [Owned software complete; inherited 24/23/19/Q20 gate](../reports/prompt_25.md) |
| 26 | [Native examples and shared application](26_native_examples.md) | Partial | [Report](../reports/prompt_26.md) |
| 27 | [TWAI adapter and SDK evidence](27_twai_adapter.md) | Partial | [Owned software and SDK compile; inherited 24/26/Q20 gate](../reports/prompt_27.md) |
| 28 | [Arduino and ESP-IDF integration](28_firmware_examples.md) | Partial | [Owned firmware/platform software and builds; inherited 27/Q20 gate](../reports/prompt_28.md) |
| 29 | [Device evidence readiness and conflict gate](29_device_evidence.md) | Partial | [Owned software readiness complete; inherited Q20 and conditional acquisition pending](../reports/prompt_29.md) |
| 30 | [Drive state enable and release](30_drive_actions.md) | Partial | [Checked documentary decoder; enable provenance and release command remain gated](../reports/prompt_30.md) |
| 31 | [Stop operations and owner integration](31_stop_actions.md) | Partial | [Conditional native stop lifecycle, owner integration and remaining gates](../reports/prompt_31.md) |
| 32 | [Fault clear and device-reference actions](32_fault_reference_actions.md) | Partial | [Conditional clear/zero operations, retained evidence and coordinate withdrawal](../reports/prompt_32.md) |
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
