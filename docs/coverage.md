# Requirement coverage after pure CAN/SDO checks

Prompt39 adds the `2300` typed U16 read (192 total read shapes), conditional
`prepareSave`/`prepareFactoryRestore` and explicit `verifyPersistence`. Native
command outcomes remain immutable while independent readback can report survival
of three fields after an externally evidenced power cycle. Application
`captureConfiguration`/`prepareConfigurationRestore` reuses the existing settings
engine for `6081/6083/6084`, retaining partial effects without rollback or save.
See [report39](reports/prompt_39.md) and [commissioning](commissioning.md). Q15
keeps full persistence scope/rearm/restart/effects unqualified; Q20 still gates
the new upload. No hardware or CLI coverage is claimed.

Prompt38 adds all 89 assigned PDO/2203 read rows (191 total read shapes) and one conditional heartbeat setting (14 conditional setting rows). Mandatory exact acquisition grants remain separate from confirmed channel serviceability. [Commissioning](commissioning.md#explicit-communication-commissioning) records the supported workflow and blocked node/route/bitrate/persistence scope. No new hardware qualification is claimed.

Prompt37 adds implemented-profile inventory, checked minimal probe projection,
explicit host selection and bounded passive/active discovery through the same
app/owner. Native and isolated package consumers exercise it; [commissioning](commissioning.md)
states the permanent two-slot limit, unknown read effects, identity ambiguity and
retained quarantine. No physical discovery, unique model map or firmware qualification is added.

Prompt 36 adds conditional method-specific homing, separate mode-6 enable and
interrupt, and checked reference publication. The six assigned read rows are
implemented. [Manual native reconciliation](reports/prompt_36_coverage.md)
separates API/test/documentation coverage from physical qualification and lists
remaining earlier owned omissions. [Homing gates](homing.md) remain closed by
default; no physical homing evidence was acquired.

Prompt 35 adds the conditional [finite velocity lifecycle](velocity.md), typed
raw `60FF` readback and mode3 DEC-8 owner stop integration. Host software and
firmware evidence remain separate: no new startup, reverse, zero-rate, mode-change
or physical-stop fact is established. [Prompt35 report](reports/prompt_35.md)
records executable software and the smallest remaining evidence obligations.


Prompt 34 adds `prepareMoveAngle`, `validateMoveAngle` and
`AxisSession::requestMoveAngle` to the implemented host motion scope. It uses
the same `MoveContext`, exact `preparePosition` and ordinary finite owner path;
`move_angle` checks wraps/ties/polarity, limits/quantization, exact large angles,
epoch/reference invalidation, stops and unknown trigger effects. The installed
`angle_preview` and C1 N5/abstract servo consumers are checked. This adds no
qualified physical angle, automatic scale, CLI route or second production profile.

**As of the 2026-11-02 checkpoint.** The [NMT/TPDO checkpoint](reports/2026-11-02_nmt_pdo_checkpoint.md) adds fixed
TPDO1 configuration and addressed owner NMT to R11/R12/R13/R17/R20. Native
adversarial checks cover per-step failures and interval freshness; one node1
specimen completed the exact24-service experiment. Broader PDO/timers/SYNC,
broader persistence, sample provenance and Q20 remain open; prompt18 is Partial.
A user power cycle supplied both boot-up frames and subsequent26-field readback;
node1 reverted to producer0/map3 in that one cycle. Each node also completed a
30-field read-only readiness inventory; nonzero `607A`/`60FF` reads equal to EDS
defaults and mode0 do not close motion-admission or completion requirements.
The 2026-11-06 [PDO sessions](reports/2026-11-06_pdo_stream.md) later measured
the TPDO1 event timer and NMT transitions with bench tools only; the other open
items above are unchanged.

[Prompt 33](reports/prompt_33.md) adds checked native position-parameter arithmetic and owned target/axis/settings snapshots to R06/R13/R20. Packets 20 and 21 add the gated move lifecycles on one path: `prepareMoveAbsolute`, `prepareMoveRelative`, `nextMove`, `advanceMove` and `AxisSession::requestMove`/`requestMoveRelative`, with interruption tests through the real fake owner. The absolute move needs caller-supplied acceptance and completion evidence and refuses by default; every relative request is refused until a wire base is evidenced. The
application-local `position` experiment measured node 1's acceptance and completion signals for
the absolute move ([packet 19](reports/handoff_19_33/packet_19.md)). With them, the absolute
lifecycle first ran unattended on node 2 through the bench `move` route and succeeded with the same
bits (trial 121, [unattended report](reports/2026-11-03_unattended_bench.md)). It has since run on both nodes: 213 on node 2 and 413 on node 1, each ended by a planned quick stop at 30 rpm, and 705 and 706 on node 1 at 10 and 60 rpm. On 2026-11-04 node 1 measured the relative base `ACTUAL` (+200 from 104 with last target 600 ended at 304) and then ran the library's relative lifecycle to `SUCCEEDED` (411; again at 120 rpm in 708) ([2026-11-04](reports/2026-11-04_bench.md), [2026-11-05](reports/2026-11-05_motion_and_stress.md)). One firmware, judged from drive reports, no qualification; the remaining gates are in the [guide](position_parameters.md).

[Prompt 32](reports/prompt_32.md) adds conditional fault-clear and zero-only
device-position actions to R06/R13/R20. Ordered command terminals, original
errors and new observations remain independent. Counter effects withdraw
coordinates and old targets before possible trigger TX; no new origin/home is
inferred. The [guide](fault_reference_actions.md) keeps other reset routes,
fault classes and physical/internal-counter qualification explicitly open.
Packets 17 and 18 add `AlarmIncident::codeLink`, always `UNRESOLVED`, and
`valueOneMayRemain` (Q13), and the FEEDBACK and `606C=0` admission with
`CounterBaseline` (Q14). Both have an application caller,
`AxisSession::requestClearAlarm` and `requestCounterZero` (backlog S12). The
counter zero ran on node 2 (trial 323); no alarm clear has run.

[Prompt 31](reports/prompt_31.md) adds conditional native halt/quick-stop coverage
to R06/R12/R13/R20: exact single writes, fresh typed not-running observations and
real-owner urgent/interruption tests. It does not qualify other predecessor
states, velocity/homing stopping, target disposal, braking, torque or reference
effects. [Stop scope and gates](stop_actions.md) remain separate from host tests.
Packets 7 and 16 add the DEC-8 predecessors, session history and the best-effort
`000B`, which never succeeds. One HALT from `001F` ran on node 1 during a 2 rpm move: the drive
reported a stop and `6064` stayed 316 for 26 s (trial 113; the attendant was not sure about the shaft,
[attended report](reports/2026-11-03_attended_bench.md)). The bench
`stop` route then sent a stationary `000B` on each node, unattended: from `000F` and from that halted
`010F`, each drive reported low bits `000` and released (`2100` bit 4). A `QUICK_STOP` now
completes on that report and keeps the release, host-tested by `quickStopReleaseIsTheMeasuredTerminal`
([unattended report](reports/2026-11-03_unattended_bench.md)), retested on hardware on 2026-11-04 (trial 208).
That day also ran a stationary `010F` (reported enabled and unchanged, 207), `000B` with no known predecessor on a released and on a
never-enabled drive, and `000B` during a move, which completes on standstill after the
`standstillObserving` fix (trial 413; [bench report](reports/2026-11-04_bench.md)). On 2026-11-05 a HALT at
300 rpm (709) and a quick stop at about 100 rpm while accelerating (713) completed `OBSERVED`
([report](reports/2026-11-05_motion_and_stress.md)). All of these are drive reports on one firmware.

[Prompt 30](reports/prompt_30.md) adds checked documentary state interpretation
to R08/R10/R20: explicit layouts, raw values and acquisition provenance, unknown
bits and disagreement. It does not close action coverage R06/R13 or firmware
qualification. `prepareEnable` implements the documented position-enable sequence behind a closed
gate. It succeeded on both specimens in the attended session, after the bit 10 fix (`e17271a`);
that is one firmware, not a qualification, and its initial/no-pending-motion provenance is a
caller's statement, not a measured rule. No reviewed source defines release's CAN
command/effects. Both lifecycles and their action-specific tests remain pending.

The [connected-board intake](reports/2026-11-02_s2_bench_intake.md) adds actual
USB/design facts to R19 and S2/P4 isolated core compile/link evidence to R04/R20.
That original intake did not establish target communication; the S2 checkpoint
above now does. P4 firmware support and wider physical qualification remain
pending; [platform coverage](firmware_examples.md) keeps these
separate.

[Prompt 29](reports/prompt_29.md) delivers its offline evidence-readiness scope:
one typed application/owner path, retained finite acquisitions, loss/failure
tests and native/firmware builds. The later S2 checkpoint supplies raw acquisition,
while qualification remains pending; the [conflict table](device_conflicts.md)
keeps Q01-Q21 open with exact
measurements and conditional experiment prerequisites. No firmware capability
or canonical read-effect policy changed.

The [independent 27/28 audit](reports/prompts_27_28_audit.md) strengthens R12/R17/R20/R28:
captured ingress cannot overtake SDK handoff, invalid GPIOs fail before SDK calls,
SDK command validation is shared, and failed IDF builds retain diagnostic
evidence. These are host/compile corrections, with no new drive permission or
physical qualification.

The [fresh 25/26 audit](reports/prompts_25_26_audit.md) strengthens passive health
history coverage: qualified expired PDO deliveries retain processing time while
remaining insufficient for READY. Native application, retention and isolated
consumer checks were repeated; no hardware or new profile support is claimed.

The [requirements matrix](requirements_matrix.md) owns the full acceptance criteria.
This ledger tracks the actual milestone evidence and retains every requirement ID
and its current prompt owners. The [source map](source_map.md) and
[prompt 1 report](reports/prompt_1.md) and [prompt 2 report](reports/prompt_2.md)
substantiate the audit and intake entries. [Prompt 3](reports/prompt_3.md) and
the [reviewed dictionary guide](reference/lichuan_review.md) substantiate the
completed documentary policy and remaining feature-specific gates. The
[prompt 4 contrast ledger](reference/drive_contrasts.md) records test-only
N5, EPOS4 and proprietary-frame cases; it adds no installed support.

[Prompt 5](reports/prompt_5.md) adds an actual native CANopenNode fit experiment
and [one selected service boundary](decisions/canopen_stack.md). It establishes
host proof evidence only. [Prompt 6](reports/prompt_6.md) adds concrete contracts,
an actual owner/read/write proof and core-only drive contrasts. Production
controller and vendor drive support remain pending. [Prompt 7](reports/prompt_7.md)
adds the [minimal package](getting_started.md), version freshness and source/install/export
tests. Status, endpoint identity, Classical frame validation and prompt 9's
[exact units/quantization](units_status.md) and prompt 10's [pure axis/reference API](axis_contract.md) and prompt 11's [pure SDO checks](canopen_services.md) are installed. Later, prompt 33 installed the Lichuan profile's gated, unqualified position moves.
[Prompt 8](reports/prompt_8.md) adds real quick/full verification, strict public
documentation and passing Windows quick and Linux GCC/Clang sanitizer CI.

The [fresh prompt-14 audit](reports/prompt_14_audit.md) strengthens R12/R20
with segmented-upload cancellation boundaries and a toggle-wrap counterexample.
It also supplies current independent review evidence for R32. No runtime or
hardware-support claim changed.

The [fresh audit of prompts 15-17](reports/prompts_15_17_audit.md) adds regressions
for initial service-proof age/origin, independent matching-response history and
unknown-capture forwarding. R05/R11/R20/R32 retain host-only scope.

[Prompt 18](reports/prompt_18.md) is **Partial**: the fixed TPDO1 sequence,
exact readback and every-step failure handling are implemented and host-tested;
node1 supplied the narrow live result. Broader remapping/timing, persistence
and sample-provenance qualification retain their separate Q17 scope.
The [fresh Prompt 18 audit](reports/prompt_18.md#fresh-independent-audit)
corrects the standard-versus-device evidence distinction and narrows Q17's
dependencies; it grants no new runtime capability or hardware qualification.

**Pending** means no CAN implementation, corresponding CAN host test, firmware
build or hardware qualification has been delivered for that requirement. Finite raw Lichuan reads are implemented under 19; no operational drive
profile or hardware qualification is claimed. The installed API implements gated, unqualified position moves in the Lichuan profile (prompt 33);
no motion is qualified, and prototype evidence below does not change that support claim. Reference/data verification is not driver testing.

| Requirement | Prompt owners | Evidence now / remaining scope |
| --- | --- | --- |
| R01 | 1-2; recurring all | Prompt 14 refresh records current RS ownership, cancellation and retained-outcome implementations/tests; source map links current proof declarations. Recurring audits remain. |
| R02 | All | Prompt 14 refresh inspects ADS1115/INA228 cancellation/effect/result seams and actual callers/tests; differences from CAN result/release retained. Recurring audits remain. |
| R03 | 5-10, 40-42, 49-50 | Prompt 6 implements prepare/next/advance, typed getIdentity and concrete contexts plus admit/admitUrgent/service/result/release/cancel in the prototype; pure units/axis APIs are now installed under 9-10; 19 adds installed typed raw read contexts/getters; typed identity is host-tested: it works under the explicit `MEASURED_U8` option and fails by default (`DECLARED_U16`) on the measured specimens at `1009`, where raw acquisition succeeds; conditional position/angle, finite velocity and homing operations are host-tested; CLI remains pending. |
| R04 | 5-12, 15, 17, 26-28, 45-46, 49-50 | C++11 api_contract and fixtures build without stack/SDK/Python/siblings; optional owner uses the selected C99 client. Prompt 7 source/install/export consumers pass; the core excludes the optional owner/stack. |
| R05 | 5-6, 13-18, 23-25, 45-46 | [Owner contract](can_owner.md) fixes one table, lifetimes and per-call RX/TX/event budgets including zero-budget behavior. Native tests cover copied payload, single RX, reservations, immutable operation deadlines and retained terminals; urgent capacity now has real application-owner tests in 24. Prompt 13 promotes the selected-stack SDO lifecycle with persistent channel quarantine; 15 adds passive node facts and synchronous invalidation to that table; 23 adds the real ordinary application scheduler, two-channel fairness, copied admission/retention, passive dispatch and actual typed read/loop-setting paths; 24 adds reserved urgent/results, cancellation, local controller epochs and finite retained recovery without remote requalification. |
| R06 | 2-4, 19-22, 29-36, 47-50 | Conditional halt/quick-stop in 31, fault-clear/zero-only reference actions in 32, and the gated absolute and relative moves in 33 have real-owner tests. The moves refuse by default: absolute without caller-supplied acceptance and completion evidence, relative without an evidenced wire base. Fault clear and counter zero have an application caller, `AxisSession` (backlog S12); the counter zero ran on node 2 (trial 323), and no alarm clear has run. Finite velocity (35) and homing (36) now have conditional host lifecycles and owner tests; broader transitions and physical qualification remain pending. |
| R07 | 1-4 | Original bytes verified; every EDS entry and all 53 manual functions reviewed with exact sources. Four Lichuan model scopes and N5/EPOS4 documentary contrasts complete. |
| R08 | 2-4, 19-22, 29-36, 47-48 | All 24 conflicting claim sets retain original alternatives and reviewed dispositions; 21 bounded feature gates remain open. Prompt 29 rechecked original pages/EDS and supplies a [measurement table](device_conflicts.md) separating documentary, host and physical evidence. No firmware winner invented. |
| R09 | 9-10, 21-22, 32-36 | Production exact rational parsing/conversion, independent scales, integer rounding and bounded radians/binary32 quantization are host tested. Pure axis/reference mutation, exact endpoints, freshness/epoch checks and offline preview are host tested under 10; native scaling and drive actions retain later owners. |
| R10 | 9-10, 13-25, 30-32 | Production Status preserves selected call-result semantics/static labels. Production SDO checks and the proof preserve raw U32 aborts separately; acknowledged write retains completion NOT_OBSERVED. Prompt 25 implements passive application communication health and exact counters; real drive/machine-state interpretation remains separate. |
| R11 | 11-18, 23-25 | Real expedited/two-segment SDO proof plus prompt-6 continuous raw event dispatch and boot-up invalidation even when the observation ring is full. Prompt 11 adds installed checked initiation envelopes, legal segmented/abort evidence and real selected-engine vectors. Prompt 13 implements the optional bounded SDO lifecycle and checked 1-32-byte forms; 15 adds pure network codecs, shared node facts and reset/cache invalidation; 16 adds explicit NMT and service eligibility; 17 adds checked actual maps, qualified raw PDO observations and per-route invalidation; 19 adds checked raw SDO/PDO projection with per-field age/skew; 18 now adds the fixed TPDO1 sequence and exact readback, and bench continuation packet 7 the `EVENT_TIMER` `1800:05` raw 0/100 transition with exact readback (host-tested; replayed once on node 2 in both directions, 2026-11-06); broader Q17 scope remains. |
| R12 | 13-14, 21-25, 30-39, 45-46 | Production SDO tests cover copied intent/results, bounded queue/response/overall deadlines, per-route quarantine, abort cleanup and no replay. The [independent prompt-13 audit](reports/prompt_13_audit.md) adds abort-during-staging and reply-before-controller-confirmation regressions. Prompt 14 adds separate capture/processing observations, adversarial correlation, copied-TX eligibility, binding invalidation and permanent quarantine where reopening evidence is missing. Prompt 27 implements the pinned S3 controller retirement boundary with fake and SDK compile checks; physical retirement remains unqualified; [source/evidence gates](reports/prompt_14_sources.md) stay explicit. |
| R13 | 2-4, 19-22, 30-39, 49-50 | Reviewed documentary access/effect/value policy and exact native owners exist. Prompt 12 generates 194 compact descriptors and complete source/semantic coverage with active freshness checks; four conflicting types and four reserved entries are withheld/excluded. 19 implements 30 raw read rows, 20 nine native group rows, and 21 all 23 I/O read rows and 22 all 33 tuning/control rows; 35 adds typed raw `60FF` and 36 adds six homing reads and 38 adds 89 PDO/2203 reads (191 total), with separate read coverage. Remaining reads retain their ledger owners; the [manual reconciliation](reports/prompt_36_coverage.md) lists four earlier software omissions and communication rows under 38. Fourteen conditional setting rows now have separate setting coverage (three position parameters, the 2202 selector, eight DI fields, one loop selector and one exact heartbeat transition); remaining DO/filter/actions stay gated under 21, and the 32 other tuning rows under 22 each name their next evidence in the ledger. Q20 read effects stay unestablished. The owner-accepted DEC-1 list is risk acceptance, not evidence: it admits ordinary typed reads of all 16 listed rows and `prepareProbe` on `1000:00`, and every row outside it stays gated. Packet 11 read all 16 live on two specimens, which records success, not read effects. Descriptors do not complete setters or actions. |
| R14 | 21-25, 29-36, 47-48 | Prompt 21 decodes documentary DI function/polarity separately from raw output registers and logical monitor levels. Wiring/electrical knowledge and method prerequisites remain independent; explicit DI reassignment is implemented with partial-effect/readback/invalidation tests; no output action is implemented. |
| R15 | 21-22, 29, 32-36, 47-48 | Conditional finite position/angle, velocity and homing APIs have bounded owner tests and retained trigger/completion/stop evidence. Physical scope and firmware gates remain separate; homing requires actual method-specific inputs, direction and reference qualification. |
| R16 | 37-39 | 37 implements static profile/capability queries, checked probe, finite explicit configured-target scans, passive network-only observations and zero-I/O host selection. Timeout/result release cannot recycle quarantined channels. 38 adds 89 reviewed-acquisition reads and the conditional 1017 raw 0 to 1000 workflow; remaining communication mutation and persistence stay 38-39; Q01/Q18/Q20 and physical qualification remain separate. |
| R17 | 26-28, 40-42, 49-50 | Prompt 26 implements framework-free offline units and fake native read examples plus bounded shared ProbeApp/ProbePlatform. Prompt 27 adds the controller adapter; 28 builds shared Arduino/IDF firmware and an isolated core component. CLI remains later work; fake/compile results are not physical qualification. |
| R18 | 40-44 | Pending |
| R19 | 29, 43-44, 47-48 | Prompt 29 delivers bounded native/firmware read acquisition through the ordinary application, explicit admission, raw traffic and retained failed evidence; [host checks and limits](reports/prompt_29.md). S2 raw reads, `1017`, addressed NMT TX and fixed TPDO1 are retained for named specimens. Motion experiments, Python campaigns and physical qualification remain pending. |
| R20 | 7-46, 49-50 | The explicitly registered CTests cover core/units/axis, Classical frames, SDO checks/selected-engine vectors, preview/native consumer/proofs, version/checker/verification/doc failures and planning evidence. Quick runs registered non-full tests; full adds documentation and a separately retained package contract. Prompt 28 adds firmware/platform and build-checker regressions; prompt 29 adds exact-byte acquisition and firmware evidence tests, a fake-only example and an isolated harness consumer; current counts are in [its report](reports/prompt_29.md). Generator/object/header and lifecycle tests use the existing registration; typed read tests and preview extend the same registration; current owner integration is in [prompt 23](reports/prompt_23.md), tuning/control is in [prompt 22](reports/prompt_22.md), and preceding settings evidence in the [fresh settings audit](reports/prompts_20_21_audit.md). |
| R21 | 1-2, 5-6, 23-25, 45-46 | Prompt-6 read-only FieldCore refresh plus common identity/configuration generations and exact-intent fixtures. Actual FieldCore-like integration remains 45–46; no framework types imported. |
| R22 | All | Execution branch/base recorded; this prompt uses checked commit/sync delivery; recurring duty. |
| R23 | All; final 49-50 | Current audit, roadmap, backlog, bench and support limits documented; release scope pending. |
| R24 | All; architecture 5-6 | One selected stack and one documented fixed ownership table, no runtime selection or universal operation engine. Host owner with two node/SDO slots, one NMT result and eight PDO routes sizeof 10528 bytes on Windows x86-64, including transient initial service/SDO times; embedded sizing, timing and controller qualification pending. |
| R25 | 7-8, 49-50 | Prompt 7 delivers C++11 core, installed/exported consumers, independent headers, C++17/no-RTTI checks, exact distribution lists and MIT attribution. Prompt 8 adds working verifier/CI/strict docs; final delivery status remains in its report. |
| R26 | 1-10, 45-46, 49-50 | Core-only N5 stepper/BLDC and EPOS4 fixtures pass with optional full-step geometry, independent scales and C11 capability/calibration gates. No additional installed driver or vendor sequence. |
| R27 | 5-6, 9-18, 23-25, 45-46 | Generic proprietary address 500/subaxis 2 and servo-without-CANopen-state review attempts pass. Synthetic CANopen profile/owner reject unsupported protocol before admission. Separate real protocol implementation pending. |
| R28 | 5-6, 11-12, 15, 17, 23-28, 45-46 | Classical eight-byte values preserve extended/RTR shape; FD and Classical BRS/ESI combinations explicitly fail. Installed SDO checks narrow service frames while common source-backed VESC/ODrive shapes remain valid. Prompt 27 adds a pinned Classical S3 adapter, host callback/retirement tests and SDK compilation; FD and physical qualification remain pending. |
| R29 | 2-4, 9-10, 21-22, 30-36, 45-46 | Production tests exercise independent command/feedback/load/linear scales and exact time dimensions without full-step geometry. C10 keeps Lichuan ms distinct from acceleration. Pure axis/reference policy is implemented; native/physical scaling qualification remains pending. |
| R30 | 5-6, 29, 45-50 | Explicit test-only N5/EPOS4/proprietary fixtures and synthetic profile do not install or advertise another driver. Actual additional profile/qualification milestones remain pending. |
| R31 | All; 1-2, 9-10, 23-25, 40-42 | Literal conventions mapped to actual prototype functions, contexts and owner methods, with current RS/I2C source evidence. Prompt 25 implements application health/statistics/diagnostic copies; conditional position/angle, finite velocity and homing operations are host-tested; CLI remains pending; pure units and axis targets are installed. |
| R32 | All | Prompt 6 separated header ownership, source/fixture work and stronger independent boundary review; corrections verified against actual code and independent native probes. The [fresh prompts 10-11 audit](reports/prompts_10_11_audit.md) repeats arithmetic, protocol and package review against current source; no production defect found. Lead owns integration and Git. |
| R33 | All; tooling 7-8, final 49-50 | Five distinct contracts, proof README, source comments and status docs accompany prompt 6. Focused Doxygen on all public proof headers passes without warnings; prompt 8 now enforces strict installed API docs and extraction with real failure fixtures. |
| R34 | All | Same actual upstream engine reused with a direct fixed-table owner and explicit prototype limits; no second engine or runtime selector. Production integration remains pending. |
| R35 | 1-50 | Current owners and delivered subsets are tracked in [PROGRESS](prompts/PROGRESS.md): prompts 1-17, 23-26 and 34 Complete, 18-22, 27-33 and 35-36 Partial, and 37-50 Pending. Prompt 34 closes angle software integration, retaining inherited physical/firmware gates. Prompt 35 supplies conditional finite velocity software while its own device-evidence gates remain open. The [29/30 audit](reports/prompts_29_30_audit.md) preserves readiness/decoder versus dependent firmware actions. |

Later stages replace Pending with links to real APIs, tests and evidence, keeping
documented, implemented, host-tested, firmware-built and hardware-qualified
status distinct. Do not close a semantic feature using a descriptor, raw accessor
or UNSUPPORTED stub. [D01](backlog.md) closes documentary review only; the
remaining entries and Q01-Q21 preserve the actual feature/evidence obligations.

[Prompt 20](reports/prompt_20.md) adds typed native group reads through the same
engine. R09/R13/R20 retain separate write/scale prerequisites and host-only
qualification. The fresh 20/21 audit implements documented native parameter
ranges (including zero) with concrete pre-trigger evidence, write/readback and
partial-effect tests. Packet 12 adds a typed `2202` selector, register-level only, and
real-owner integration; `2200`, `2201`, `607D` and `6060` keep exact gates in the
[driver guide](driver_settings.md). Packet 13 proves the DI group through the real owner and
keeps per-field DO, filter and `60FE` write gates in the [I/O guide](io_settings.md); the `60FE` read is not gated, and it completed on both specimens under the measured count width (trials 608, 609). Remaining source-gated groups retain their original owners.

[Prompt 28](reports/prompt_28.md) adds firmware-built evidence to R04/R17/R20/R25:
Arduino and native-IDF ELF/BIN builds consume the same application/owner sources;
a separate isolated IDF consumer uses only staged portable core files. Host fakes
cover clock failure, pressure, routing and recovery. Installed headers acquire no
SDK dependency, no device command occurs at startup, and hardware qualification
remains absent. Command/schema parity is still assigned to 40-42.
