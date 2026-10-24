# Changelog

## Unreleased

This repository currently contains planning and reference material. No CAN library, firmware or hardware qualification is released.

### Added

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

- Exposed conflicting DO addresses/functions, three status layouts, mode access, velocity sequence/sign, subdivision/soft-limit defaults and undocumented timing instead of treating generic CANopen or example code as firmware truth.
- Documented and rejected public-example SDO command/width errors, shared status, hidden persistence and arbitrary timing assumptions; kept absent following-error/automatic-tuning capabilities out of the new evidence requirements.
- Corrected velocity duration to start at successful preparation, matching RS, with queue/setup time included.
- Corrected SDO upload size/transfer assumptions and scoped non-changing read claims to reviewed objects.
- Clarified service eligibility with disabled heartbeat, first TPDO observations and NMT transition evidence.
- Moved catalogue freshness and console parity gates to the stages that implement their sources; narrowed multi-profile acceptance to the implemented scope and small shared contrast fixtures.
- Removed unnecessary repeated report/audit work while keeping current source inspection and documentation mandatory.
- Preserved original reference/baseline/evidence bytes in Git and added optional HEAD/index artifact checks; line-ending conversion previously broke 52 stored artifact hashes.

The [24 October prompt audit](docs/audit/PROMPTSET_REVIEW_2026-10-24.md) records the findings, fixes, source revisions and validation limits.

The multi-drive architecture and test contrasts are plans. Nanotec, maxon and other researched products are not implemented or qualified drivers in this checkout.
