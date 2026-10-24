> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Prompt execution progress

This file tracks **future library implementation**, not completion of the planning audit. All implementation stages are initially pending. Update it after each stage; detailed evidence belongs in `docs/reports/stage_NN.md`.

Execution has not started. When it starts, record the promptset identity, newly created execution branch, inspected base commit and upstream here. Reuse that branch for all stages and resumed sessions, committing and syncing after each prompt and at useful completed checkpoints. A maintenance/audit branch is not an implementation-stage completion or the automatically selected future execution branch.

The user-expanded scope and [multi-drive criteria](MULTIDRIVE.md) apply to all stages: one stepper/servo library, Lichuan CANopen first, documented N5/EPOS4 contrasts, independent future CAN protocol profiles. Research and test-fixture planning do not mark another driver implemented.

The expanded [conventions](CONVENTIONS.md), [documentation](DOCUMENTATION.md), [maintainability](MAINTAINABILITY.md) and [delegation](DELEGATION.md) contracts apply to every stage. Completing these planning documents does not change any implementation stage below from Pending.

| Stage | Work | State | Report |
| --- | --- | --- | --- |
| 00 | Refresh audits and establish evidence | Pending | — |
| 01 | Resolve dictionary and family coverage | Pending | — |
| 02 | Freeze architecture and stack boundary | Pending | — |
| 03 | Build/package/CI foundation | Pending | — |
| 04 | Status, units and exact axis vocabulary | Pending | — |
| 05 | CAN frames and checked codecs | Pending | — |
| 06 | SDO services and uncertainty | Pending | — |
| 07 | Network events, PDO and node lifecycle | Pending | — |
| 08 | Typed reads, settings and native catalogue | Pending | — |
| 09 | CiA402 state and actions | Pending | — |
| 10 | Finite position and angle moves | Pending | — |
| 11 | Velocity, homing and remaining native operations | Pending | — |
| 12 | Shared CAN owner and health | Pending | — |
| 13 | Discovery, commissioning and persistence | Pending | — |
| 14 | Native and ESP32 application adapters | Pending | — |
| 15 | Shared CLI and JSONL | Pending | — |
| 16 | Python runners and evidence | Pending | — |
| 17 | Fault injection and cross-library contracts | Pending | — |
| 18 | Hardware qualification and family validation | Pending | — |
| 19 | Release, documentation and final audit | Pending | — |
