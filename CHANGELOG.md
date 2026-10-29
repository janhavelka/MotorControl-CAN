# Changelog

## Unreleased

This repository contains planning/reference material and native service/API ownership proofs. No CAN library, firmware or hardware qualification is released.

### Added

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

- Split the original 20 implementation stages into 50 complete prompts numbered 1–50, with smaller scopes, explicit prerequisites and focused completion checks. Preserved original obligations in a scope map and historical archive.
- Moved owner/adapters before motion evidence, retained the early conditional experiment path, and separated CLI, runner and qualification work into bounded assignments.
- Added targeted shared-contract reading, single-prompt/range execution instructions, a checked sequence manifest and regression checks for numbering, prerequisites, scope ownership and commit/sync closure.
- Expanded every stage with concrete file/API contracts, failure cases, documentation duties and work splits.
- Refreshed source-backed naming, including RS owner admission/service methods and the distinction between operation results and application health.
- Clarified late SDO reply ambiguity, fixed operation deadlines, explicit stop outcomes and staged native-feature coverage.
- Adapted RS/I2C agent guidance for CAN: one branch for the full promptset, verified commit-and-sync checkpoints during and at the end of each prompt, lead-owned Git operations, proportionate CI, failure investigation and reference maintenance.
- Clarified branch creation: each new promptset execution starts a new dedicated branch, then reuses it across all stages and resumed sessions; maintenance audits do not start implementation.

### Fixed

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
