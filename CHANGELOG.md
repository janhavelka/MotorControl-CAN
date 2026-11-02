# Changelog

## Unreleased

### Accepted reads and identity widths (audit W4)

- Add `ReadEffect::ACCEPTED_UNQUALIFIED` and `SdoReadEffect::ACCEPTED_UNQUALIFIED` for the owner's DEC-1 list, driven by the ledger's `read_disposition`. This records risk acceptance, not evidence: `effectPossible` stays true.
- `prepareProbe` on `1000:00` now works. A first accepted upload can confirm a channel without claiming freshness. `DeviceEvidence` admits a plan only when every object is accepted. `603F`, `1001`, `1003` and all unlisted objects keep their gates.
- Add `ReadOptions::identityWidths`. `MEASURED_U8` reads `1009` and `100A` as the one byte two specimens returned; nothing selects it automatically, and the default stays the EDS U16. `IdentityObservation` reports the widths.
- Rename `ServiceEligibility::nonChangingProbe` to `probeUpload`.

### TWAI transport (audit W3)

- `TwaiDriver::submit` returns `TwaiSubmit` instead of `bool`: SENT, DEFERRED, FAILED_UNSENT or FAILED_UNCERTAIN. A receive interrupt during promotion now defers instead of quarantining the route.
- Replace `TwaiAdapter::service` with `pollEvents` before `owner.service` and `promote` after it.
- The adapter holds a queued handoff, so its possible effect and response clock start at actual SDK submission. A frame proved unsent ends without quarantine: `TxReport::provedUnsent`, `SdoClient::holdHandoff`, `noteSubmitted`, `withdrawUnsent`, `withdrawNmtUnsent`.
- Set `fail_retry_cnt=-1` (DEC-4): hardware retransmits, so a no-ACK frame stays pending until an application-started recovery. See [the consequences](docs/adapters/twai_sdk.md).
- Accept a received Classical DLC of 9..15 as eight bytes. `FirmwareProbe::clockFailed()` becomes `progressionFailed()` plus `failure()`, which reports the real cause.

### Owner placement (audit W2)

- Move `CanBusOwner`, its diagnostics and counters from `examples/common/` to `adapters/owner/`, in namespace `MotorControlCANOwner`, with optional target `MotorControlCAN::CanBusOwner`. Runtime behavior is unchanged and the target is still not installed.
- Owner value types and the transport boundary are in `CanBusTypes.h`, included by `CanBusOwner.h`. Tests share plumbing through `test/support/OwnerRig.h`.
- Migration: include path `adapters/owner`; replace `MotorControlCANExample::CanBusOwner` and its owner types with `MotorControlCANOwner::`; link `MotorControlCAN::CanBusOwner` instead of `MotorControlCANExampleOwner`. `ProbeApp`, consoles and board code stay in `examples/` under `MotorControlCANExample`.

### Promptset repair (audit W1)

- Remove prompt 18 from prompt 38's prerequisites, make prompt 43 depend on 40, 44 on 42 and 43, and 47 on 29, 44 and 45, so prompts 49-50 can close as a scoped software candidate. New checker guards keep 18 out of 49's ancestors and 41/42 in the ancestors of 44, 45, 47 and 48, and reject mis-decoded UTF-8.
- Adopt the usable-prerequisite rule and an Inherited gates column. Move progress narrative to `docs/reports/progress_history.md`; prompts 23, 25 and 26 become Complete with their gates listed, and no gate closes.
- Reassign 12 read rows from prompt 38 to the prompts that write them, leaving 89 with 38. `607C` moves to prompt 36 for both read and write ownership. The generated `Objects.h` descriptor changes its owner number only.
- Correct the reading of `607A=5000` and `60FF=120`: they equal the EDS defaults, and no retained record shows they were written, so "retained intent" was unsupported. Device conflicts and coverage now carry the November 2 measurements.
- Replace "exclusive" and "inclusive" deadline wording with `now >= deadlineUs` expires. Behavior is unchanged.

### External coder continuation

- Add 22 paste-ready handoff packets for prompts19–22 and29–33, including the
  required architecture repairs, concrete source/test routes, bounded HIL
  diagnostics and deliberate failure reproduction/retesting. Preserve current
  implementation and firmware gates; this documentation performs no hardware test.

### Fixed TPDO1, explicit NMT and honest receive bounds

- Add the fixed reviewed TPDO1 configuration lifecycle, retained per-step effects,
  exact complete readback and same-owner experimental caller; no retry/rollback.
- Add tagged NMT transport and conservative S2/S3 receive intervals without
  claiming exact arrival timestamps or weakening SDO/PDO deadlines.
- Retain node1 heartbeat1000/readback, fresh addressed Pre-operational and all
  24 fixed-map services. Final map2 is confirmed; broader firmware and sample
  qualification remain open. See the [checkpoint](docs/reports/2026-11-02_nmt_pdo_checkpoint.md).
- Retain both boot-up frames after a user power cycle and fresh readback of
  node1 producer0/map3. This is one-cycle reversion, not a persistence guarantee.
  Add finite 30-field readiness uploads per node, preserving nonzero retained
  targets, mode0 and configured input functions without motion or reassignment.
- Keep prompt18 Partial and prompt33 unfinished. No motion/start/reset/save or
  restore was performed. Final integration verification is recorded separately.

### Earlier S2 connection and bounded raw evidence

- Add the pinned IDF 5.5.5 S2 UART/core-0/TWAI port and a separate OPTAK bench
  image with silent startup, explicit controller start and one finite upload-only
  experiment per node. No startup device request, retry or automatic recovery.
- Add exact one-use unknown-effect upload grants and bounded server-indicated
  raw counts without promoting service qualification or changing typed widths.
- Retain 20 successful raw uploads on each of nodes 1/2, 40 RX/40 TX in total,
  zero reported CAN errors/drops and 9852 bytes minimum remaining task stack.
  Both devices indicate one byte for `1009=0x0C` and `100A=0x20`, versus EDS U16.
- Correct the acquisition stack budget/preflight scratch and wait for the
  preceding controller TX report before granting the next raw field. Preserve
  failed attempts and the separate bounded post-terminal traffic tail.
- Add S2 compile/input checks and CI coverage. The [connection report](docs/reports/2026-11-02_s2_live_connection.md)
  separates observations from firmware meaning and qualification; no PDO mutation
  or motor action was performed at that earlier checkpoint; Prompt18 subsequently gains the subset above.

### Native position preparation

- Add pure `preparePositionParameters` with owned exact intent/axis/settings, native target limits, explicit quantization and current reference freshness.
- Keep relative displacement limits separate from endpoint limits; preserve exact and conservative approximate error evidence.
- Add standalone native usage and installed consumers. Finite move lifecycle and firmware qualification remain pending; preparation produces no service work.

### Stop and reference-action audit

- Add real-owner regressions for admission after failed or uncertain device
  counter clears, including retained results, coordinate epochs and attempted
  replacement of a quarantined reservation.
- Name the reviewed monitor masks and make the three action completion checks
  explicit. Wire values, public APIs and firmware gates are unchanged.

### Fault clear and device-position zero

- Add conditional `prepareClearAlarm` and zero-only `prepareSetDevicePosition`
  through the existing action lifecycle, with exact ordered native writes and
  fresh report-based completion. No enable, motion or factory-reset preamble.
- Preserve original errors, raw status and each command terminal, including an
  acknowledged trigger followed by a failed return-to-zero write.
- Withdraw host origins, references and prepared targets before possible counter
  trigger transmission; synchronize owner configuration before new-epoch reads.
- Keep controlword reset, other fault classes and physical home/rearm/internal
  resume semantics explicitly outside the implemented conditional scope.

### Conditional Lichuan stops

- Add finite `prepareStop/nextAction/advanceAction` for the documented position-mode
  `000F -> 010F/000B` transitions, with no enabling preamble or automatic replay.
- Retain command acknowledgement separately from fresh not-running reports,
  interrupted request results and uncertainty after cancellation or failure.
- Exercise real urgent owner admission, old-operation fencing, queue retirement,
  capacity, asynchronous events and channel quarantine with native fakes.
- Keep unsupported predecessor states, braking/torque/queue consequences and
  hardware qualification explicit in the stop guide and prompt 31 report.

### Documentary drive-state interpretation

- Strengthen checked-publication tests for output/current overlap and real
  boot-up invalidation while preserving historical observations.

- Add checked `getStateInterpretation` over existing typed state reads, preserving
  raw values, per-field provenance, unknown bits and status/monitor disagreements.
- Keep EN86, ZH86, ZH57 and OP86 interpretations explicitly separate. Selecting
  a source table does not qualify firmware or establish readiness/completion.
- Record the documented position-enable sequence and its missing admission
  provenance separately from the absent release command. No action stub, hidden
  enable or generic CiA402 fallback is installed.

### Connected S2 board intake

- Record ESP-Prog2 USB identity, duplicate COM15 assignments, inspected board
  power/debug/CAN net mappings and the user's reported DIP settings.
- Verify isolated portable-core builds for S2 and P4; retain the separate
  firmware-adapter, electrical and hardware qualification obligations.
- Resolve the COM15 mapping and verify UART/automatic boot entry with the actual
  S2FH4 ROM. Retain the failed manual-entry attempt; no flash or CAN test performed.

### Finite device evidence readiness

- Add a bounded acquisition collector around the ordinary typed application and
  CAN owner, shared by native and firmware examples. Record exact plans,
  raw traffic, all consumed terminals, controller events, timing and resources.
- Keep ordinary DEVICE acquisition behind Q20. Explicit reviewed unknown-effect
  read experiments retain their exact admission references without granting
  production permission or retrying an uncertain request.
- Interrupt on trace loss or reported RX overflow, distinguish missing ingress
  from captured rejected frames, and preserve prior successful fields on cancel.
- Add offline success/failure demonstrations, adversarial harness and firmware
  regressions, isolated consumers and strict documentation/build registration.
- Recheck original manuals/EDS, retain all conflicts and record user-reported
  fixture facts with precise conditional measurement gates. No live qualification.

### Independent TWAI and firmware audit

- Keep the final captured-RX/fault guard and SDK submission in one critical
  section; a captured boot-up cannot enter between validation and handoff.
- Invalidate backward adapter time using the owner's last accepted timestamp,
  retaining uncertain active effects.
- Bound GPIO numbers before the SDK's shifting macros and match host fixtures
  to the pinned ESP32-S3 pin mask.
- Share SDK virtualenv/source checks between compile gates, reject disabled
  dependency checks, and retain partial IDF diagnostics on failure without
  masking the original error. Add independent interleaving and failure regressions.

### Shared Arduino and ESP-IDF examples

- Add thin ESP32-S3 Arduino and native-IDF startup around the existing application,
  owner and pinned TWAI adapter. No selection or device request occurs at startup.
- Require explicit controller facts; default compile fixtures leave CAN disabled.
  Add a pinned internal owner task, bounded USB byte I/O and signed 64-bit time.
- Latch clock faults, invalidate using authoritative owner time and attempt local
  retirement once. Preserve uncertainty and retained results on failure.
- Add actual firmware and isolated portable-component build gates, compiler-input
  checks, host pressure/fault tests and CI. Core consumers remain SDK-free.
  Compilation and fake evidence do not qualify a physical board or drive.

### Pinned TWAI transport

- Add an application-only ESP32-S3 adapter for ESP-IDF 5.5.5's node API,
  with two owned TX slots, configurable bounded RX storage and exact TX callbacks.
- Retire the old controller before explicit recovery creates a fresh node;
  cancellation and restart never replay queued work or settle unknown effects.
- Keep queued RX ahead of TX promotion, preserve raw error flags and inhibit
  on observable RX loss. Record SDK overflow/timing limitations and source hashes.
- Register host policy and actual-shim callback tests plus a separate pinned SDK
  compile command. No firmware startup, flashing or physical qualification is added.

### Corrected passive PDO delivery history

- Retain checked delivery time for a qualified PDO whose capture is already
  expired. Its stale capture cannot establish READY or change service counters;
  revoked mapping samples and malformed input remain excluded.

### Native application examples

- Shared example-only ProbeApp/ProbePlatform now connect explicit typed reads,
  bounded input/output and retained owner completions. Native fake mode exercises
  the ordinary receive path with interleaved passive traffic and supplied time.
- The existing offline units preview demonstrates a rejected stale target and
  unchanged output. Isolated installed-core consumers build both examples;
  no SDK, sibling checkout or hardware is required. No motion or CLI is added.

### Passive owner health and statistics

- Add cached binding health with explicit timestamp validity, service totals,
  retained prior errors and passive communication-state precedence.
- Count actual controller TX separately from queue acceptance; saturate interface
  counters and preserve live evidence/interlocks/history when clearing totals.
- Add eight bounded copied diagnostics and resource snapshots; slow trace output
  cannot consume normal RX. Keep map/sample provenance and heartbeat policy explicit.
- Extend the offline native consumer and independent health/counter/trace tests.

### Owner cancellation and recovery

- Added reserved urgent admission/results, per-request cancellation, reservation/producer
  invalidation and independently retained finite controller recovery in the example owner.
- Track each external frame through final TX reports or explicit retirement, including
  cleanup frames and uncertain enqueue failures. Controller epochs reject old ingress;
  unknown retirement inhibits reuse without discarding operation results.
- Allow the next operation under the same axis reservation while old terminal results
  remain retained. Live prior work still requires explicit cancellation; no stop or replay
  is implied. The transport enqueue callback now receives the controller epoch.

### Fixed

- Added real application-owner loop-setting failure checks for uncertainty,
  retained acknowledgements, readback failures and unrelated-node progress.
  Removed an unused duplicate generation field from each owner request slot.

### Ordinary application CAN owner

- Add copied admission, per-axis/physical-peer reservations, fair bounded
  two-channel scheduling and immutable results with explicit release.
- Route all ingress through existing passive/SDO storage before observer copies;
  distinguish controller queue acceptance, actual TX and protocol confirmation.
- Preserve initial queue age across dispatch and retire active/queued work on
  peer invalidation; retain uncertainty and permanent SDO quarantine.
- Integrate public reads and confirmed loop settings, including pre-TX
  configuration withdrawal and checked new-epoch readback, plus a native example.
- Keep the owner outside the core package; urgent/cancel/recovery remain 24.

### Control and tuning groups

- Add five finite typed groups covering all 33 U16 tuning/control reads, with
  raw unknowns, per-field provenance, checked atomic getters and bounded work.
- Add explicit model-bound loop-selector settings using the existing reserved
  ACK/readback lifecycle; reject OL86-C closed loop and invalid candidates.
- Harden settings descriptor/mask/cursor validation before yielding work.
- Record exact evidence gates for other tuning writes, independent history/
  communication ownership and 95 generated read descriptors. No new motion API.

### Typed settings audit corrections

- Implement explicit 86C DI function/polarity and position speed/ramp settings;
  correct blanket blockers that confused absent software with missing device facts.
- Validate full candidates, actual control/state evidence and continuous owner
  reservation before work; retain per-write acknowledgements, unknown effects,
  exact readback and unchanged getter outputs on failure.
- Withdraw the actual configuration epoch before possible transmission; add a
  checked idle owner epoch update and explicit PDO-map restoration with sample
  qualification kept independent. No quarantine reopening or automatic commands.
- Add independent real fake-controller write/failure tests, standalone consumers
  and documentation. DO/filter/action and scale/limit/mode gates remain pending.


### Digital I/O reads

- Add four finite I/O groups through `prepareDriverRead/nextDriver/advanceDriver`
  and checked `getIo`, with all 23 raw read rows and documentary DI codes.
- Preserve reserved entries, unknown bits/codes, conflicting output addresses,
  terminal numbering and filter metadata; no electrical-level claim is inferred.
- Add offline usage, 7,230 native checks, installed/exported consumer coverage and
  explicit generated read ownership. Prompt 21 remains Partial: DO/filter writes,
  output actions and their partial-effect/invalidation contracts stay gated by
  the specific source facts in the I/O guide.


This repository contains planning/reference material and native service/API ownership proofs. No CAN library, firmware or hardware qualification is released.

### Added

- Three typed native Lichuan read groups covering nine direction/subdivision,
  software-limit and speed/ramp rows, with checked `getDriver`, per-field age,
  unknown raw values and an offline caller. They reuse the existing finite read
  engine. Prompt 20 remains Partial: no write-state or scale permission is inferred.

- Installed finite raw Lichuan identity, state and communication reads for 30
  explicitly assigned rows, with checked getters, per-field SDO/PDO provenance,
  conservative age/skew, retained aborts and an offline compiled preview.
  Read effects, model matching and disputed state interpretation remain unknown;
  Q20 still blocks a non-consuming probe, so prompt 19 is Partial.
- Independent fake-owner read tests and isolated installed/exported consumers
  cover every owned width/access rule, failure publication and invalidation.

- Added pure checked PDO layouts/decoding, complete SDO map-readback evidence and eight passive routes in the existing owner. Actual map contents, wire-sample provenance and capture freshness remain independent; reused routes require explicit producer/ingress retirement evidence.
- Generated the eight unverified EDS PDO templates and ten reviewed passive layout facts from the single ledger, retaining raw unresolved mode/velocity signedness and all existing SDO/write gates. No PDO configuration write, NMT inference, scaled position or completion is added.
- SDO terminals now retain supplied admission time and node lifecycle epoch so mixed stale readback results cannot certify a newer map.

- Added explicit typed CANopen NMT manager requests through the pinned native producer, with one retained local result, bounded deadlines and addressed/domain-broadcast invalidation. Queue/TX evidence remains separate from observed remote state; nothing starts or resets automatically.
- Added NMT versus SDO service eligibility, heartbeat-disabled reviewed probing/configuration, caller evidence-age policy and separate configured-TPDO receive admission. Known Stopped retires ordinary SDO work while retaining uncertainty and terminals; NMT Stop is not a physical motor stop.

- Added pure configured-route heartbeat/boot-up/EMCY codecs and a single passive projection in the existing optional owner's channel table. Independent capture/processing times, producer interval facts, NMT/SDO/error observations and four copied diagnostic records remain separate from drive state, outcomes and health.
- Added synchronous cache/axis invalidation on reset, binding/dialect/configuration withdrawal and controller recovery, including full-history and generation-exhaustion behavior. No heartbeat configuration, NMT command, drive action or channel reopening is implicit. The passive HB/EM implementation decision now uses checked owner projection with pinned native-consumer parity.

- Added SDO capture/processing precedence, separate retained raw observations, stale/unsolicited-response quarantine, explicit binding withdrawal and copied-TX eligibility checks. Unsupported reopening remains blocked without remote-session and local queue evidence; no reset/retry or device read permission is inferred.
- Added independent adversarial reconciliation tests for duplicate replies, different-value writes, every segmented TX cancellation boundary, delayed processing, clock limits, controller caches, alternate qualified channels and partial effects. Updated retained proofs to fail closed on pre-handoff traffic.

- Added bounded CANopenNode SDO lifecycle with two unique physical channel slots, copied requests, immutable overall/queue/response cutoffs, explicit TX versus protocol evidence and retained results. Unconfirmed failures quarantine channels across release; no automatic retry or controller I/O.
- Added expedited/segmented transfers through 32 bytes, checked unsized scalar normalization, full unsigned abort evidence and bounded unsupported-upload cleanup. Promoted the retained bridge into one optional application adapter; portable service values remain stack-free and independently packaged.

- Added deterministic reviewed Lichuan object generation, 194 compact profile-scoped descriptors, optional descriptive metadata and complete generated source/semantic coverage. Four disputed signed types and four reserved entries remain withheld or excluded; all read effects and write qualifications retain their gates.
- Activated exact object freshness in quick/full verification and CI, with malformed-ledger, deterministic, stale-byte and atomic-publication regressions. Added independent-header, installed/exported and catalogue-free consumers; generated metadata implements no typed operation or device support.

- Added pure checked CANopen SDO request/reply envelopes, explicit channel/binding/width correlation, unsigned and signed little-endian reads, and owned reply evidence. Valid aborts retain every bit; valid segmented initiation remains distinct from malformed input. CANopenNode remains the sole selected encoder/session engine.
- Added Classical BRS/ESI rejection, exhaustive frame/command checks, raw-buffer overlap/atomic-output regressions and actual CANopenNode-generated request tests. Isolated installed consumers and strict API documentation cover the new declarations; no device permission or live exchange is implied.

- Added production pure axis configuration, independent mechanical/encoder origins, exact position/rate preparation, wrapped-angle path primitives and reference-aware admission revalidation. Relative targets require their selected basis; observation age survives unchanged reconfiguration, and exhausted generations fail closed.
- Added offline units preview, installed/native axis consumers and independent coordinate, time, overflow, limit and binary32 regressions. Replaced proof coordinate arithmetic with the production API; synthetic I32 capabilities remain test-only. No device command or motion operation is implemented.

- Added production exact rational parsing/conversion, independent dimensional scales, all integer rounding policies and checked binary32 quantization with exact tolerance tests. Added explicit bounded radians approximation and installed C++11/C++17/no-RTTI consumer coverage.
- Migrated retained proof arithmetic and numeric types to production Units, preserving test-only N5/EPOS4 contrasts and profile-specific coordinate policy.
- Added quick/full verification with bounded commands, retained logs/artifacts, generated-version and actual CTest registration checks. Added Windows GCC quick and Ubuntu GCC/Clang sanitizer full CI with pinned actions and Doxygen source.
- Added strict Doxygen 1.13.2 and installed API extraction checks, deliberate checker/documentation failure tests, contributor/security guidance, formatting policy and a maintained current/planned verification table.

- Added the native C++11 `MotorControlCAN::MotorControlCAN` package with documented status, endpoint identity and Classical CAN validation; relocatable CMake installation, optional IDF registration and explicit source export. Development version 0.1.0 comes only from `library.json`, with checked-in deterministic `Version.h`.
- Registered the retained API/owner and multi-drive proofs under `test/`, plus independent-header, C++11/C++17/no-RTTI source/install consumers, stale-version and missing-public-dependency checks. Synthetic profiles and stack dependencies remain outside installation.
- Completed prompt 6 with concrete C++11 value/operation contracts and one fixed-capacity application owner around the selected CANopenNode client. Added copied read/target-write flows, retained results, deadline/retirement checks, urgent fairness and boot-up invalidation.
- Added core-only N5 stepper/BLDC, EPOS4 and proprietary-binding contrasts, independent scales and capability/ramp-dimension checks. Published five distinct architecture contracts and Doxygen for every public proof declaration; no installed profile or physical completion claim.
- Completed prompt 5 with a pinned CANopenNode C/C++11 fake-controller exchange, retained payload/results, asynchronous events, segmented transfers and failure/retirement checks. Selected one application-side engine while keeping the future installed core independent of stack/SDK dependencies.
- Recorded source-backed cancellation, deadline, callback and TWAI queue limits; preserved uncertain execution after controller handoff. Fixed an expired-frame handoff gap found during independent review.
- Added offline vendor inventory/hash/license validation, CMake hash guards and seven integrity regressions for the retained experiment. Recorded measured host object sizes separately from unmeasured embedded memory/timing.
- Completed prompt 4's source-backed N5 stepper/BLDC and EPOS4 servo contrast ledger, with test-only cases for exact coordinates, native units, frame formats, protocol routing and evidence limits. No additional driver or hardware support is claimed.
- Completed prompt 3 documentary review of every Lichuan entry/function, with per-field policy, four separate model records, source-backed decision tables and 21 bounded qualification gates. No CAN implementation or hardware support is claimed.
- Reviewed-schema guards and regressions retain exact feature owners, manual obligations and original conflict alternatives; reject unconditional writes, false non-consuming reads, erased gates and malformed review data.
- Completed prompt 2 source intake: a traceable Lichuan ledger for all 99 EDS indexes, 202 typed entries, 53 manual semantic claims, 24 unresolved conflicts and the `1018` count anomaly, with independent coverage fields and a source-byte validator.
- Added adversarial ledger-validator tests and integrated ledger accounting into the planning-package check.
- Started promptset execution with a live source/consumer audit, exact responsibility map, requirement coverage, executable backlog/roadmap and an explicit unknown CAN bench record. Original reference verification is separate from future ledger implementation and hardware qualification.
- Complete Lichuan EDS intake: 99 indexes, 202 typed entries, raw fields and original line spans, with a source-comparison integrity check.
- Detailed official-source motion, status, settings, I/O, PDO, commissioning and timing tables; pinned public implementations and 17 synthetic protocol fixtures with provenance and licenses.
- Focused concrete evidence in 40 numbered prompts, a reading map, and an explicit source-check/correction loop in every prompt through COMMON and AGENTS.
- Audits of MotorControl-RS, sibling I2C libraries and the FieldCore consumer boundary, with source revisions and retained verification evidence.
- Manufacturer manuals, EDS and multi-drive protocol research with local provenance and integrity manifests.
- A 20-stage implementation promptset for a portable C++11 stepper/servo library, starting with Lichuan Classical CANopen.
- Concrete API/status/owner/health conventions, continuous documentation and Doxygen requirements, maintainability guidance and task-based subagent/model selection.
- An offline planning validator for prompt structure, requirements, links and reference hashes.

### Changed

- Moved pure PDO evidence/time value types into installed headers, forwarded by
  the existing adapter; shared bounded route comparison avoids duplicate policy.
- Assigned all readable ledger rows separate read owners and read coverage;
  corrected the `2000/2001` ZH86 source citation from PDF page 18 to page 17.
  Whole-object/write coverage and unqualified read effects are unchanged.

- Corrected the PDO blocker rationale: CiA documents the conditional remapping procedure. Q17 now separates Lichuan applicability, changed timing settings and sample qualification so an established narrow subset need not wait for unrelated facts.

- Recorded the Prompt 18 PDO configuration blocker and expanded the reviewed Q17 evidence requirement for edit order, allowed states, readback, timing and persistence. Existing passive PDO support is unchanged; no remap/settings API is claimed.

- Corrected initial network attachment to preserve original SDO service-proof
  time and origin, with independent matching-response history across channels.
  Older or unknown-capture responses no longer erase a valid newer service proof.
- The retained proof facade now forwards processing-only receive timestamps as
  unknown capture; queued passive traffic cannot qualify after a reset through
  a fabricated capture time.

- Clarified that SDO freshness premises cover individual segments during a transfer. Added toggle-wrap ambiguity counterexamples and all segmented-upload cancellation boundaries; corrected a test's caller-time order. No runtime state or device permission changed.
- Split the original 20 implementation stages into 50 complete prompts numbered 1–50, with smaller scopes, explicit prerequisites and focused completion checks. Preserved original obligations in a scope map and historical archive.
- Moved owner/adapters before motion evidence, retained the early conditional experiment path, and separated CLI, runner and qualification work into bounded assignments.
- Added targeted shared-contract reading, single-prompt/range execution instructions, a checked sequence manifest and regression checks for numbering, prerequisites, scope ownership and commit/sync closure.
- Expanded every stage with concrete file/API contracts, failure cases, documentation duties and work splits.
- Refreshed source-backed naming, including RS owner admission/service methods and the distinction between operation results and application health.
- Clarified late SDO reply ambiguity, fixed operation deadlines, explicit stop outcomes and staged native-feature coverage.
- Adapted RS/I2C agent guidance for CAN: one branch for the full promptset, verified commit-and-sync checkpoints during and at the end of each prompt, lead-owned Git operations, proportionate CI, failure investigation and reference maintenance.
- Clarified branch creation: each new promptset execution starts a new dedicated branch, then reuses it across all stages and resumed sessions; maintenance audits do not start implementation.

### Fixed

- Reject missing/null ownership for readable ledger entries, preventing history
  and deferred communication/PDO read obligations from disappearing silently.
  Independent prompt-19 review found no runtime defect; its Q20 probe gate remains.

- Fixed matching server aborts being ignored while the next SDO segment was staged; pending TX is now retired before processing the abort. Delayed controller confirmation for the latest live handoff now remains valid after a nonfinal reply, until the next handoff supersedes its token. Independent upload/download regressions preserve quarantine, deadlines and immutable terminals.

- Fixed object catalogue generation for supplementary Unicode and C++11 trigraph/escape boundaries; invalid UTF-8 surrogates fail before publication. Generated coverage now retains each documentary disposition and all three RPDO1 source mapping words.

- Fresh independent prompts 10-11 audit corrected a stale retained-proof unit/rounding comment and an ADS1115 source citation. Axis/reference arithmetic, CAN/SDO checks and isolated package consumption passed new independent probes and integrated verification; no executable change was needed.

- Independent prompts 7–9 audit made quick-mode omissions explicit in verifier summaries, added a useful failure for empty tool-version output, and corrected stale package inventory and floating-environment documentation. Arithmetic and package consumption passed independent review.
- Corrected the adapted directed binary64 conversion to check both neighboring directions; rare rounded-operand underestimates now return the closest toward-zero value, with independent regression vectors.
- Allocated retained verifier directories atomically so equal Windows clock readings cannot collide; the retention regression now fixes the clock deliberately.
- Preserved arbitrary 32-bit SDO abort words at the experimental C/C++ bridge without an invalid enum load, found by the new GCC/Clang UBSan jobs. Added upload/download high-bit regressions.

- Fresh independent prompt-6 audit clarified the fake owner's fixed per-call RX/TX/event budgets and zero-budget behavior. Additional core/owner probes and all maintained checks passed; no runtime correction was required.
- Fresh independent audit of prompts 4-5 corrected EPOS4 unit configurability, retained N5's already-set-bit4 profile-position entry trigger, and added missing ramp-dimension and optional brake/calibration/missing-mode fixture specifications. The native stack experiment passed fresh ownership/failure/boundary review without a runtime change.
- Corrected fault-reset edge wording, native mode/action ownership and per-entry manual citations. Kept unknown limit/offset/feedback units, peak-current meaning, model-specific output conflicts and raw identity acquisition distinct from qualified behavior.
- Rechecked prompts 1-2 independently: completed the 126-path source-map check record, retained ZH86's same-manual DO contradictions and the separate 57C statusword contrast, and hardened ledger/planning validation for false applicability, missing conflicts and malformed rows.
- Removed the evidence-harness dependency on firmware-gated remapping/settings, preserving unfinished feature and owner-integration obligations; aligned numeric execution order with independent progress after a blocker.
- Clarified that stop/reset controlword arrows never require enabling first; added explicit repeated-stop and fault-reset trace requirements.
- Preserved the 57C manual's conflicting DO0/1/2 versus DO1/2/3 labels, separated SDO access from PDO eligibility, and required qualified provenance for delayed PDOs on reused routes.
- Added planning regressions for transitive evidence deadlocks and README prerequisite drift. Rechecked the five official PDFs and EDS without replacing their identical original bytes.
- Exposed conflicting DO addresses/functions, three status layouts, mode access, velocity sequence/sign, subdivision/soft-limit defaults and undocumented timing instead of treating generic CANopen or example code as firmware truth.
- Documented and rejected public-example SDO command/width errors, shared status, hidden persistence and arbitrary timing assumptions; kept absent following-error/automatic-tuning capabilities out of the new evidence requirements.
- Corrected velocity duration to start at successful preparation, matching RS, with queue/setup time included.
- Corrected SDO upload size/transfer assumptions and scoped non-changing read claims to reviewed objects.
- Clarified service eligibility with disabled heartbeat, first TPDO observations and NMT transition evidence.
- Moved catalogue freshness and console parity gates to the stages that implement their sources; narrowed multi-profile acceptance to the implemented scope and small shared contrast fixtures.
- Removed unnecessary repeated report/audit work while keeping current source inspection and documentation mandatory.
- Preserved original reference/baseline/evidence bytes in Git and added optional HEAD/index artifact checks; line-ending conversion previously broke 52 stored artifact hashes.

The [24 October prompt audit](docs/audit/PROMPTSET_REVIEW_2026-10-24.md) records the findings, fixes, source revisions and validation limits.

The [28 October correction audit](docs/audit/PROMPTSET_REVIEW_2026-10-28.md) records the new source-backed findings, live sibling delta and current prompt measurements.

The multi-drive architecture and test contrasts are plans. Nanotec, maxon and other researched products are not implemented or qualified drivers in this checkout.
