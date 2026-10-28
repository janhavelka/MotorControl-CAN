# Prompt 46 Independent consumers and drive contrasts

Apply [COMMON.md](COMMON.md). Prerequisites: [4](4_drive_contrasts.md), [10](10_axis_coordinates.md), [28](28_firmware_examples.md), [42](42_cli_motion_routes.md), [45](45_fault_integration.md).

## Objective and boundaries

Prove framework-independent consumption, required stepper/servo/protocol contrasts and complete native/API/CLI coverage against actual implemented files. Deliver a small FieldCore-like consumer and a concrete handoff without modifying FieldCore. Reuse earlier evidence-backed contrast fixtures; do not build fake production drivers for Nanotec, maxon or proprietary protocols. Final user-document/package release review follows in prompt 49.

## Read and audit

Refresh RS multi-profile/common types and consumers, affected ADS1115/INA228 packaging/owner examples and FieldCore's current motor contracts, scheduler, module and settings/health boundaries. Inspect actual installed exports, profile inventory, generated ledger and CLI routes. Read [MULTIDRIVE.md](MULTIDRIVE.md) and original contrast evidence rather than assuming shared CiA402 names mean identical behavior. Apply [CONVENTIONS.md](CONVENTIONS.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Add a small framework-neutral fake consumer exercising exact preparation, caller-owned work, supplied asynchronous observations, admission/wire/execution evidence and typed terminal results. Map how a prospective FieldCore CAN owner/module would handle scheduling, settings, binding/reference invalidation and health. Use ordinary values and public APIs, not copied FieldCore types or a new application framework.
2. Build an isolated installed native consumer with no sibling checkout, SDK, stack, Python, reference or test tree. Check public headers and selected-profile export boundaries. Contract fixtures stay tests and are excluded from installation, production profile selection and `profile list`.
3. Complete required N5 stepper/BLDC and EPOS4 contrasts through real common code: no mandatory full-step geometry, independent motor/feedback/mode facts, command versus feedback scale, wider signed rates, differing ramp representations and multi-axis endpoint identity. Do not promise every unknown future drive fits unchanged; document the reviewed seam and remaining limits.
4. Complete non-CANopen representation/routing fixtures: valid extended ID and RTR semantics, Classical-only FD rejection, native float quantization, absent/unknown acknowledgement, optional state/completion/brake/calibration evidence and no automatic mixed-protocol probing. Use authoritative values where established; label pure abstract invariants honestly. CANopen network terms must not become generic axis prerequisites.
5. Verify profile-keyed object policy keeps vendor meanings separate, including differing uses of index 0x2300. Check capacities against reviewed PDO/resource differences without changing common axis types. Reuse small fixtures; no speculative universal engine or second physical receiver is justified by a test.
6. Reconcile ledger descriptors, codecs, typed reads/settings/actions/workflows, public functions, CLI routes and Python scenarios mechanically and manually. Every confirmed in-scope function needs its implemented public path and tests, plus CLI exposure or a concrete API-only rationale. Unknown documentation and unqualified hardware stay distinct. Do not delete obligations or count a descriptor/raw accessor as typed feature completion.

## Verify and exit

Run native consumer/build/export checks, the documented contrast suite and coverage consistency checks. Build core-only and selected-Lichuan consumers plus the combined test-fixture target; actual two-production-profile packaging is required only when a second real profile exists. Verify no fixture leaks into support inventory. Compile/run the FieldCore-like example without FieldCore. Resolve confirmed ownership, API or packaging defects and rerun affected tests. Record precise remaining source/hardware uncertainty without claiming surveyed products are supported.

## Documentation and delegation

Write `docs/fieldcore_handoff.md`, update the source map/profile contract, coverage and relevant CHANGELOG/Doxygen. Keep integration guidance prospective and independently usable. Independent normal engineering reviewers may own isolated-consumer and coverage checks; use stronger source reasoning for profile/units/acknowledgement contrasts. The lead owns common contracts and integrates fixes under [DELEGATION.md](DELEGATION.md); no sibling edits or concurrent Git actions.

Close this prompt's owned scope and checks; record `docs/reports/prompt_46.md` with consumer evidence and deliberate differences. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
