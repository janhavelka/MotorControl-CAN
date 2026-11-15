# Device, RS, sibling and contrast audit dispositions

2026-11-15. Independent review of every numbered finding in sections 8-11 of
[the November 2 audit](../prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md): 28 D rows
(there is no D-18), 14 R rows, 12 S rows and 12 X rows. The historical audit is
preserved. This report distinguishes correction of a bad instruction from
implementation and physical qualification. **Resolved** closes the particular
instruction/contract defect, not its listed hardware gates. **Partial** means
some required implementation/evidence remains. **Overstated** identifies a
claim the cited evidence does not establish.

This was source inspection, not a hardware session or a fresh native test run.
Named tests below are inspected regression locations; their existence is not a
claim that this review executed them. The lead owns integrated verification and
Git synchronization. No sibling or reference artifact was edited.

The rows describe baseline `efba718` before this maintenance pass. D-09 and X-11's
small corrections, the stale introduction and prompt-46 wording were subsequently
applied in the same change; [the lead report](2026-11-15_auditor_disposition.md)
records the integrated result. The open implementation and physical gates remain.

## Original evidence reopened

EN86, ZH86 and OP86 mean the original PDFs in `docs/reference/vendor`, as in the
historical audit. Reopened EN86 pp3-4, 7-8, 10-13, 15-20; ZH86 pp12, 15-16, 19;
OP86 pp5, 7, 9; and the original EDS sections identified below. EN86 p4 was
rendered and viewed: the panel says VAC20-80 and VDC30-110. Rendering OP86 and
OP57 pp5-14 at 60 dpi produced equal pixel digests for all ten corresponding
pages, independently reproducing the duplication finding. MuPDF emitted
structure-tree warnings but produced the page rasters.

The retained N5 v3.3.0/FIR-v2213 PDF was reopened at pp38, 64-65, 84, 283,
297-298, 349, 360-361, 370 and 412. The official
[EPOS4 Firmware Specification](https://www.maxongroup.com/medias/sys_master/root/9579215585310/EPOS4-Firmware-Specification-En.pdf)
was fetched into memory on this review date: 310 pages, rel13740, SHA-256
`66fe32f15ab0bcbf9a008d0d0da8ffe395064c6716edcb3f3b28907f7160e64f`.
Pages 221, 232, 234, 236 and 256 were read. It remains remote-only; no PDF or
invented manifest artifact was added. This refresh confirms the scoped X-02,
X-03, X-04, X-06 and X-11 documentary claims, not device support.

## Device findings

Paths beginning `L/` below mean `src/profiles/lichuan_c/`; `P33`, etc. mean the
corresponding numbered prompt. Q numbers refer to `docs/device_conflicts.md`.

| ID | Verdict | Current evidence and disposition | Remaining action or gate |
| --- | --- | --- | --- |
| D-01 | Resolved | P33 Implement3 now requires observed mode1 and explicit enable before setup. `L/Position.cpp::checkState/checkMove` enforce that; `prepareMoveAbsolute` uses that check. EN86/ZH86 p12 and OP86 p9 confirm mode, enable, parameters, trigger order. | Keep the separate mode/enable qualification; no extra reorder is needed. |
| D-02 | Partial | P33/P35/P36 explicitly distinguish mode changes, mode6 enable and homing. `L/Homing.cpp::check` rejects anything but enabled stationary mode6; `L/Position.cpp::checkState` rejects wrong mode. EN86 pp12-15 do not specify an energized home-to-position transition. | Q11/Q21 remain; P36 owns explicit mode6 enable, P47 the home-then-move measurement; later P57 owns regular mode/release work. A conditional home engine is not a proven transition. |
| D-03 | Resolved contract; physical scope partial | P33 holds bit4 through motion and makes rearm visible; P36 holds `001F` until terminal/interrupt. `test/homing_test.cpp::methodsAndReference` checks no position-style rearm. `test/velocity_test.cpp::runStopFollowsTheAcknowledgedFinalWord` preserves non-documentary predecessors. EN86/ZH86 p15 explicitly assign falling bit4 to interrupt. | Q11/Q12/Q21 retain unmeasured predecessors and homing stop behavior. Do not relabel derived words as manual arrows. |
| D-04 | Resolved | P47 step2 now starts listen-only, names DEC-1 reads and leaves other objects Q20-gated. `examples/common/DeviceEvidence.cpp`, `L/Reads.cpp` and communication review grants retain explicit acquisition instead of blanket read permission; `test/communication_test.cpp::confirmedAndUnconfirmedReviewGrants` covers the boundary. | General read effects remain Q20; one accepted list/record does not qualify every EDS row. |
| D-05 | Resolved documentary separation | P38 explicitly labels the communication rows EDS-only, unknown restart/persistence and general CiA301 claims. EN86 pp15-16 list the narrow manual table; EDS `[1017]`, `[1200sub1/2]` supply names/defaults without the missing behavior. `Communication.h` retains UNKNOWN requirements. | Q17/Q18 still own service, timer and retirement evidence. Existing measured NMT observations in Q18 are specimen evidence, not a newly discovered manual guarantee. |
| D-06 | Resolved with wording caveat | P38 excludes software bitrate changes; `Communication.h::NODE_SETTING` describes `2203` as stored, not active node/DIP. `test/communication_test.cpp::exactWidthsAndStoredNodeValues` checks that distinction. EN86 pp9-10/18 and EDS `[2203]` provide no software bitrate object. | Q16 activation/precedence remains. Read the prompt's response-route sentence as evidence of the observed SDO endpoint, not proof that no other passive node evidence can exist. |
| D-07 | Resolved conflict recording | Q19, P39/P47 and `LICHUAN_OBJECT_FACTS.md` retain all three ranges. EN86 p4 panel visually confirms30-110VDC/20-80VAC; p3 spec is36-110/18-80; p7 wiring is24-50. | Nameplate/applicable manufacturer evidence controls each fixture. The unretained listing must not silently become an independently verified source. |
| D-08 | Resolved provenance correction | Q19/C18 and P41 retain the duplicated57C-body caveat. OP86/OP57 pp5-14 pixel equality reproduced; OP86 p7 assigns bits16-18 to user-defined DOs. This does not independently corroborate an86C CANopen-manual behavior. | Q06 and prompt21/47 DO qualification remain; no trial-write choice between conflicting addresses. |
| D-09 | Overstated contradiction; safety concern valid | OP86 p5 says shaft-locked at power-up, and p9 describes enable writes. Holding torque and operation-enabled are different facts, so these statements alone do not contradict. Q11 already records attended power-up holding torque on both bench specimens on2026-11-03. | Correct P39/P47/P48's `points the other way` wording; keep duplicated-body applicability and independent boot/restart observation. Do not generalize the two-specimen observation. |
| D-10 | Resolved | P39 names save parameters, restore factory settings and re-apply recorded configuration separately. `Persistence.h::PersistenceKind` and the separate preparation functions have no vendor restart API. EN86 p18/EDS `[2300]` support values 2/1, not a restart object. | Q15/Q18 distinguish power cycle and independently gated NMT effects. |
| D-11 | Resolved software contract; device evidence partial | `Persistence.h` retains UNKNOWN restart/rearm, and documents no generated zero write. `test/persistence_test.cpp::commandsAndKnowledge`, `lostReplyDoesNotBecomeSuccess`, `postCycleReapplicationIsNotSurvival` distinguish ACK/survival/reapplication. EN86/ZH86 p15 give non-exhaustive saved examples and no save rearm. | Q15 complete saved set, durability, restart and rearm remain; P48 counts/caps save actions. |
| D-12 | Resolved admission distinction | `L/Position.cpp::checkMove` rejects zero ramps; `L/Homing.cpp::rateRange/check` reject zero rates/ramps; finite velocity rejects zero. `test/homing_test.cpp::validation`, `test/velocity_test.cpp::preparationBoundaries` exercise these cases. Pure parameter conversion may still represent zero without authorizing motion. | EN86 pp11/13 ranges include zero but do not explain it; retain Q11 until measured. |
| D-13 | Resolved | P35/P36 assign their own setup writes; `L/Velocity.cpp::noteVelocityTransmission` retires motion parameter snapshots, as does `L/Position.cpp::noteMoveTransmission`; `L/Settings.cpp::retireMotionParameters` centralizes invalidation. | EN86 pp11/13 share6083/6084; keep cross-mode invalidation and do not route operation setup through mode1-only driver settings. |
| D-14 | Resolved contract; direction evidence partial | P34/P36/P47 now name2200. `L/Homing.cpp::check` compares `DriverObservation::directionRaw` against the admitted evidence; `test/homing_test.cpp::validation` rejects a mismatch. EDS `[2200]`/EN86 p18 establish the setting, not its CAN/homing effect. | Q04 direction/scale/reference invalidation remains scoped to actual evidence; no method direction inferred from the object's name. |
| D-15 | Resolved conditional implementation | `L/Homing.cpp::inputsReady/check` validates method-specific input configuration and state; `test/homing_test.cpp::validation/observationPolicy` covers admission and changed observations. P36 explicitly calls the1/2/3 method mapping host policy and refuses60FD as a substitute. EN86 p8 lists the function names; p17 gives no60FD map. | Q19/Q21 physical switch, polarity, already-active switch and limit-during-origin behavior remain unqualified. |
| D-16 | Resolved distinction | P33 correctly separates bit15 position completion, EN86-only bit12 acceptance, and EN86 bit10 zero speed. `L/Position.cpp::finishCycle` requires current evidence, not an old done bit. EN86/ZH86 p12 agree on bit15 while differing on other bits. | Q02/Q11 and250B/250C dependence/soft-limit behavior remain; mode3/6 bit15 is not fabricated. |
| D-17 | Resolved prerequisite correction | P47 step4 no longer makes release a prerequisite for every move; P42 keeps `motor-release` visibly gated. EN86 p12's6040=0 row is status correspondence, not a release procedure; p8 DI4 names no behavior. | Regular release remains a distinct later P57 assignment. A measured quick-stop release is not a universal release command. |
| D-19 | Resolved | P34 explicitly says2201 is a candidate requiring Q04, not proved pulses/turn; bounds remain native. `test/move_angle_test.cpp` exercises production conversion/admission and `position_parameters_test.cpp::nativeRangesAndQuantization` rejects unrepresentable targets. | EDS `[2201]` default0 and EN86 p11 subdivision do not establish scale. Physical angles/scale remain Q04. |
| D-20 | Resolved | P36 names the retained SRT source and5000ms violation. `test/homing_test.cpp::validation` rejects5000; `failuresAndCorrelation` rejects malformed two-byte work. EN86 p13 fixes U8 method and U32 speed/ramp. | The SRT example is a negative regression source, not a firmware procedure. |
| D-21 | Partial | P36 and `docs/homing.md` keep607C order/units/sign/reference effect qualified per evidence. `L/Homing.cpp::check` requires the exact offset scope; `test/homing_test.cpp::validation` checks out-of-range and explicitly qualified offsets. EN86 p13 includes607C; p15 omits it from the sequence. | Q14/Q21 offset and resulting reference require source/physical resolution before general home admission. |
| D-22 | Resolved | P45 and `docs/audit/PROMPT_45_SOURCES.md` separate SRT ramp scaling/63-or127/timeout defects from manual6061 mistranslation and generic quick-stop objects; the retained SRT source has not been altered. | Keep origins distinct. Prompt45's host regressions do not qualify vendor behaviors. |
| D-23 | Resolved investigation instruction | P47 step1 calls for reviewed reads2400:00/:0F and comparing:0C/:0D, not competing trial writes. ZH86 p19 and original EDS `[2400subC..F]` support0C..0F; EN86 p8/18 differ. | Q06 remains: register arithmetic is corroboration, not resolution; default comparisons cannot establish live firmware numbering alone. |
| D-24 | Resolved source separation; timing partial | P38 explicitly separates generic1ms heartbeat,100us inhibit and event units from vendor confirmation. `Communication.h` exposes a single measured heartbeat transition, not universal timing. Original `[1017]` has no unit. | Q17/Q18 timing measurement remains; the retained event-period observations already caution against blindly substituting stack defaults. |
| D-25 | Resolved policy | P38 makes1200 read-only in typed workflows; `Communication.h` exposes NETWORK reads, no SDO-route setter. Original `[1200sub1/2]` are RW but their flags do not justify a live route change. | P38/Q16 must establish route-write/recovery mechanics before any setter. No new setter is needed for this audit. |
| D-26 | Resolved inference labeling; fault scope partial | `L/Actions.cpp` now explicitly calls the FF05-to-LED-class link an inference; `docs/fault_reference_actions.md` keeps `incident.codeLink=UNRESOLVED` and the staleFF05 case. EN86 pp16/20 link only by description, not a coded reset table. | Q13 keeps other fault classes and stale-code identification unresolved despite scoped measured clears. |
| D-27 | Resolved uncertainty handling | `docs/fault_reference_actions.md` names missing arming zero and possible held1; `L/Actions.cpp::oneMayRemain`/terminal projection preserve `valueOneMayRemain`. EN86 p18/EDS `[2301]` define values without an edge/rearm promise. | Q13 future-trigger edge behavior stays unknown; no speculative arming write should be added. |
| D-28 | Resolved conflict recording | Q19 now distinguishes EN86 p7 wiring labels from p8 defaults and lists DI4-6 behavior as unqualified. Original pages confirm DI3/default0 and DO1/2/default0 despite illustrative diagram labels. | P62/69 and fixture work must qualify actual stop/release electrical functions; do not disable protection to satisfy tests. |
| D-29 | Resolved editorial notes | Q19 records negative/reverse limit wording, both manuals'603F RW versus EDS RO, and creep-time(rpm) wording. Reopened ZH86 pp16/19 and EN86 p13 confirm the notes. | No type/unit/value change is justified by these editorial differences. |

## Current sibling source closure

HEAD and dirty state were refreshed locally, with no sibling fetch or write.
All ten inspected repositories were clean at intake. MotorControl-RS had
advanced beyond prompt45's recorded revision, so its affected seams were read
again. FieldCore's ownership seam is unchanged since prompt45 at the same clean
revision; its concrete contracts and reference/runtime-config branches were
also spot-checked to evaluate the old section10 claims.

| Repository | HEAD | Relevant inspected behavior adopted or rejected |
| --- | --- | --- |
| MotorControl-RS | `19a1a4b48cb2665d0373b750d3d47730344f22fd` | `VelocityOperation.h`, `ESS_RS::prepareVelocity`, position admission, `Persistence.h`, discovery inventory and `scripts/bench_repeat.py`: adopt finite timing, explicit prerequisites, narrow inventory and bounded comparison; reject native RPM widths/defaults and RTU route rules as universal CAN contracts. |
| FieldCore-node | `c40ea8341edf8d665460b902cafefbe857a7f73f` | `MotorOperation`, `Rs485MotorModule::startMotor/applyRuntimeConfig` and reference publication: adopt identity/generation/owned terminal boundaries; reject claims of CAN, homing, velocity or persistence integration. |
| INA228 | `4de84f2564eb6382835ca9a72d8688377657dcb4` | README output-on-failure rule and `tools/run_i2c_hil.py` dry-run/version checks: adopt explicit verification/identity separation, not sensor retry or framing as motor policy. |
| MB85RC | `3ace168eb01d687ab60e9fe88cc72322e5cae260` | `MB85RC.h::readOnce/requestRead/requestWrite`: retained borrowed buffers and partial failed chunks differ deliberately from CAN's copied admitted work. |
| PCA9555 | `8b461b2afe9910b5b15fe32cc049d52e3b498970` | `readObservedState` publishes valid partial pairs; `tools/run_i2c_hil.py` branches before serial import for dry-run. Adopt the latter, not a universal output rule. |
| RV3032-C7 | `15d7a2782f7d6e136d29ff7339c1bee27bc5b8aa` | `RV3032::startWriteUserRamJob` copies input; `ConfigurationJobReport` separates failures. Reject sensor cleanup/restoration as motor stop policy. |
| ADS1115 | `e07622a99fe9f13c5d6a5a2497a8112a14185b9d` | `tools/run_i2c_hil.py::contract_verdict/evidence_verdict` separate protocol evidence from physical evidence; adopt that distinction. |
| BME280 | `2fed8fff5452bff43e4da50fbe309930e4a8f1ea` | `src/BME280.cpp::_markHardwareConfigDirty` and `mayHaveReachedDeviceAfterDiagnosticWrite` retain possible configuration effects; adopt that distinction, not sensor retry or startup writes. |
| MCP45HVX1 | `abfd749fbc641b9fe29e1eb2d6f6428ec4e9a4e2` | `tools/run_hil_mcp45hvx1.py::verdict` preserves `SKIPPED_UNSAFE` and uncertain restoration; adopt explicit non-pass classification, not automatic restoration. |
| SHT3x-main | `7d04cd1a94a01eb9c144fd95d4ec95cff3d22ec3` | `tools/run_sht3x_hil.py::main` requires exact version/commit for live mode; no I2C framing contract is imported. |

## MotorControl-RS findings

| ID | Verdict | Current evidence and disposition | Remaining action or gate |
| --- | --- | --- | --- |
| R-01 | Resolved | P35 now separates pure `Axis.h::VelocityRequest`, finite request/prerequisites and deadline argument. `L/Velocity.cpp::prepareVelocity/serviceVelocity` and `test/velocity_test.cpp::preparationBoundaries/timingAndRetainedFailure/exactPollCoverageIncludesTheWholeDuration` cover timing. Current RS `prepareVelocity` still anchors `stopDueUs=now+durationUs`, rejects zero and insufficient poll/deadline budgets. | Keep native sign/start-order evidence separate from common timing. |
| R-02 | Resolved | `scripts/bench_repeat.py::build_scenario` accepts1..20 write/verify move pairs, explicit bounds and one planned stop; no arbitrary scenario/save/restore loop. Current RS `bench_repeat.py::campaign` remains a fixed setup comparison with one cleanup. | CAN's intentionally narrower setup policies and no RS restoration sequence are documented; P44 plan/adversarial tests own this. |
| R-03 | Resolved | `bench_session.py::run_recorded` owns Console/session; `bench_scenarios.py::Scenario.run` owns cleanup; only `bench_probe.py` imports `_bench_evidence`. | No ownership rewrite is needed; preserve primary error when cleanup succeeds/fails. |
| R-04 | Resolved | Shared console exists in `examples/common/ProbeConsole.*`; native and platform source inventories consume it. P40 explicitly uses that location. | Keep package/firmware source registration checked when adding files; do not move it into the Arduino entry. |
| R-05 | Resolved | CONVENTIONS section3 and P39 name all five persistence functions, including `verifyPersistence`; current RS header confirms exact names. CAN retains separate save/factory restore and verification semantics. | No vendor reset API follows from the RS names. |
| R-06 | Resolved mapping defect | `docs/api_cli_mapping.md` dated RS additions table has separate adopt/reject rows for reads, auxiliary, qualifications, start-speed and interruption. `MOTORCONTROL_RS_AUDIT.md` has the dated delta; P35/P36/P42 own the mapped implementations/gates. | The old audit is historical, not a current HEAD assertion. This review records the new HEAD separately. |
| R-07 | Resolved | `include/MotorControlCAN/Discovery.h::discoveryProfileCount` and current discovery/scan tests distinguish static inventory, one probe and example scanning; P37 bounds scans to configured slots. Current RS `Discovery.h` contains `ProbeOutcome/ProbeConfidence`, not a core scanner. | Preserve Q01/Q20 response/model/effect distinction and explicit slot bounds. |
| R-08 | Resolved | `L/Position.cpp::checkMove` rejects zero effective displacement; `validateMoveAdmission` checks the prepared target; `test/move_angle_test.cpp` checks ZERO_DISPLACEMENT. Current RS common arithmetic versus operation rejection still differ as described. | An application already-at-target message is not physical standstill. |
| R-09 | Resolved instruction; retained regression discipline | P33/P45 require explicit comparison fields; `test/position_parameters_test.cpp::commonIntentVectorsAndOriginParity` pins intent. Current RS/CAN types still differ in relative/unit/error-budget defaults. | Future vectors must explicitly set all semantically relevant fields; do not normalize public defaults merely for test equality. |
| R-10 | Resolved | `bench_motion.py`/`bench_session.py` default to quick; `bench_scenarios.py` owns `--list`, session arguments own `--plan-only`; no CAN COM/address default was copied. `test/bench_campaign_plans_test.py` and adversarial companion cover offline plans. | Offline plans and parser checks are not live evidence. |
| R-11 | Resolved | P41 step5 and shared console name `debug off/raw/decoded` and `load`; trace describes behavior, not a fabricated route. | None beyond normal inventory/dispatch consistency. |
| R-12 | Resolved | P40 reject list includes `help advanced`, `profile ess_rs enable`, `stop direct`; `motor ...` is explicitly never-implemented grammar. `test/probe_console_test.cpp` is the parser regression seam. | Do not introduce compatibility aliases without a contract change. |
| R-13 | Resolved | P35 explicitly warns that the RS comment's admission means preparation time; current RS `prepareVelocity` assigns the timer from its `now` argument. | CAN must keep setup/queue time inside the supplied duration; see R-01. |
| R-14 | Resolved adopt/reject record; scope remains deliberate | API/CLI mapping now records held-stop recovery, upward-only escalation, settle-on-cancel and the RS comparisons. `docs/stop_actions.md` keeps CAN's `000B` reissue difference. P33/P35/P36 separately name reference, duration and method-input comparisons. | RS stop application now lives under `examples/probe_cli/ProbeApp.cpp`, so old `examples/common` line references are historical. No serial recovery mechanism transfers into CAN without its own retirement evidence. |

## Sibling and FieldCore findings

| ID | Verdict | Current evidence and disposition | Remaining action or gate |
| --- | --- | --- | --- |
| S-01 | Resolved | P34 names RS output-on-failure authority and says I2C libraries differ. Current INA228 README, MB85RC `readOnce` and PCA9555 `readObservedState` independently confirm that difference. | Keep atomic CAN preparation as its own contract. |
| S-02 | Resolved | P35 no longer uses RV3032 cleanup as a stop model; P36 limits the analogy to separate first-operation/stop failures in `ConfigurationJobReport`. | No sensor safe-state or EEPROM restoration writes may become implicit motor cleanup. |
| S-03 | Resolved | P36 now states FieldCore has no homing and names reference generation only. `MotorOperation` is None/Move/ReadState/UrgentStop; `Rs485MotorModule`'s readiness renewal is opt-in and no source/include assignment enables it. | Do not cite the contradictory old prose as homing implementation. |
| S-04 | Resolved | P33 explicitly rejects MB85RC's borrowed-buffer lifetime; current `requestRead/requestWrite` require retained caller storage while `RV3032::startWriteUserRamJob` copies bytes. CAN position contexts copy admitted data. | Use retained-context/borrowed-reply tests for CAN, not sensor API imitation. |
| S-05 | Partial, correctly bounded | P46 now targets `Rs485MotorModule`, motor task lanes and `RodCyclePorts`, expressly says no CAN owner/route/request and no motor health row/settings group. | Actual independent consumer and prospective handoff remain P46 work; wording alone does not deliver them. |
| S-06 | Resolved | P39 states there is no FieldCore drive persistence precedent; `applyRuntimeConfig` rejects motor address/baud changes and retries. | Only the ownership boundary transfers, not a save/restore procedure. |
| S-07 | Resolved attribution | P43 now attributes ADS1115 verdict split, PCA9555 dry-run guard and INA228/SHT3x version flags separately; original tools confirm those seams. P47/P48 keep evidence/non-pass limits. | CAN uses its RS-derived Console protocol; no I2C runner is an authority for motor framing or replay. |
| S-08 | Resolved instruction | P37 now distinguishes MB85RC AUTO/fixed identity and FieldCore bounded RS discovery from an I2C ACK scan. CAN `test/discovery_scan_test.cpp` exercises explicit configured slots. | ACK/responding address is not a CAN profile/model identity; arbitrary-node scanning remains absent. |
| S-09 | Resolved | P35/P36 omit irrelevant MB85RC/INA228 analogies; P38 keeps only desired-versus-verified hardware state and binding invalidation. | Do not expand this into a broad sensor lifecycle audit. |
| S-10 | Resolved | P40 owns diagnostics/classification review; P41/P42 explicitly reuse it for new routes. | Refresh a changed seam, not every historical sibling audit. |
| S-11 | Resolved | P34/P35 explicitly say FieldCore has no angle/velocity operation; `Rs485MotorModule::startMotor` selects STEPS/NATIVE and `MotorOperation` has neither operation. | Compare identity/admission only. |
| S-12 | Resolved | `ECOSYSTEM_CONVENTIONS.md` and `CONVENTIONS_REAUDIT.md` have historical/superseded headers; ownership anchor is54-56. AGENTS/COMMON now use scoped current-source auditing. | Preserve historical revision provenance; do not rewrite old snapshots as current evidence. |

## Drive-contrast findings

| ID | Verdict | Current evidence and disposition | Remaining action or gate |
| --- | --- | --- | --- |
| X-01 | Open implementation, corrected assignment | `experiments/api_ownership/Contracts.h` still owns capability/ramp/service proof types and duplicate `ActionExecution/ActionCompletion`; `test/multidrive_contract_test.cpp` includes the proof operations. P46 explicitly names the gap. | P46 must complete the required production seams and remove namespace collisions, without a speculative framework. Prototype-only labeling cannot satisfy production acceptance; tighten that alternative in the prompt. |
| X-02 | Resolved provenance | `docs/reference/drive_contrasts.md` and P46 now mark EPOS4 remote-only and require a fresh hash. Today's in-memory fetch matched the recorded rel13740 hash and310pages. | Repeat source refresh when P46 executes; no manifest hash for a nonexistent local artifact. |
| X-03 | Partial | Ledger/P46 now correctly compare Lichuan time with N5/EPOS4 acceleration. N5 p65 and EP p236 confirm dimensions. `rampDimensionsAndCapabilityGuards` still compares prototype enums/values only. | P46 must exercise production `convertAcceleration` and a typed refusal of time/acceleration substitution; no extra vendor operation engine. |
| X-04 | Partial | Ledger/P46 correctly identify N5 `6042h` I16 versus all three `60FF` I32. N5 pp349/412 and EP p256 reconfirm. `independentScalesAndWideIntent` still lacks the production I16 narrowing rejection. | P46 adds only the checked width case; a wide request is not a claimed device speed capability. |
| X-05 | Resolved | P34 removed the unsupported N5/EP float claim; `test/move_angle_test.cpp` and `position_parameters_test.cpp::representationAndApproximation` reject BINARY32 at the I32 finite-move boundary. | Proprietary float representation remains its own P46 contrast. |
| X-06 | Resolved documentary correction | P36 and the contrast ledger now include the profile-local homing row. Reopened EP p232 lists17/18 and omits24/29; N5 p84 confirms negative block-method convention/current-position35. | EP rejection is explicitly an inference from its list; no common numeric method enum or installed EP profile. |
| X-07 | Resolved instruction; production contrast partial | P39/P46 and C9 distinguish Lichuan2300 from N5 NanoJ and EP1010/1011. Reopened N5 p283 and EDS `[2300]` confirm the dangerous index collision. | P46 must prove profile-keyed dispatch/capacity through production contracts; documentary comparison is not second-profile implementation. |
| X-08 | Resolved corruption; explanatory detail partial | The test no longer stores0x08/0x40 in `Endpoint.profile`; ledger identifies3202 as a bitfield. N5 pp297-298 confirm bit0/3/6 and that CL/OL toggling is forbidden while operation-enabled; current reduction is open-loop-specific. | P46 should verify existing correction rather than repeat `stop writing` as current fact; preserve relevant bitfield constraints, no production N5 configuration engine. |
| X-09 | Resolved separation; remaining production coverage P46 | Ledger C5/C6/C7 and P46 separate VESC extended data, ODrive standard RTR and moteus FD rejection. `Frame` cannot hold16payload bytes. The historical audit itself correctly retracts its stronger extended+RTR accusation. | P46 checks routing/no double dispatch and ODrive empty-data alternative against retained protocol originals. A Frame flag rejection is not an FD adapter. |
| X-10 | Partial | `motorKindsAndOptionalGeometry` now uses production `preparePosition/validatePreparedTarget` for wrapped N5/abstract servo vectors with explicit1:1 scope. N5 p64 confirms3600/turn only with1:1 gearing. C1b still comments1536feedback counts without asserting that conversion. | P46 must assert the existing C1b conversion through production code or remove the unproved claim; no duplicate fixture family. |
| X-11 | Partial, small documentation correction | EP pp221/234 use the same increments-only position unit; the current ledger C1b still cites EP alongside N5 as evidence that representations differ. The servo test correctly labels its scale abstract. | C1b should cite N5 pp360-361/370 for distinct command/feedback units; keep EP only as a no-step-geometry contrast. |
| X-12 | Resolved terminology | Ledger C11 and P46 explicitly distinguish electrical braking from holding brakes and label the capability combinations abstract. | Capability/calibration production proof remains part of X-01/P46, not proof of any particular brake hardware. |

## Small corrections versus remaining implementation

The concrete small corrections found in this pass are D-09's false contradiction,
X-11's remaining EP citation, the contrast ledger introduction's obsolete claim
that Lichuan has no driver/matched firmware, and P46's prototype-only escape
clause/current-tense X-08 instruction. They do not justify changes to motion
code, original manuals, EDS or qualification flags.

The substantive open contrast work belongs to existing P46, especially X-01,
X-03, X-04 and X-10. Home/mode transitions, offset/input qualification, broader
communication/persistence and physical validation remain with P36/P38/P39 and
P47/P48 or their explicitly added follow-on prompts. A repaired prompt is not
evidence those obligations were implemented or measured.
