# Prompt 1 Current source audit

Apply [COMMON.md](COMMON.md). Prerequisites: none; this begins a new implementation promptset execution, not another promptset rewrite.

## Objective and boundaries

Establish a current source map, execution branch and executable backlog before designing the CAN library. The completed result is a checked navigation and decision record. No target motor code or hardware activity belongs to this prompt. CL86-C is a finished Lichuan drive with an external CANopen protocol; its internal IC is unknown.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty states, then inspect their actual declarations, implementations, callers and tests at the naming, ownership and health seams. Read relevant FieldCore-node motor contracts, dormant RS module, owner/backend and native tests without changing that repository. Use the supplied audits to locate code, then verify it. Inspect SHT3x-main, BME280, MCP45HVX1, MB85RC, PCA9555 and RV3032-C7 for the lifecycle/configuration behaviors recorded in the ecosystem audit; inspect SHZK-PT, VTN4xx and VibWire-108 only for relevant stateless codec patterns.

Read RS guidance, public operation headers, ESS sequences, `examples/common/RtuBusOwner.*`, ProbeConsole and Python runners, test registration, packaging and current verifier/workflow. Compare the live command inventory against its protocol-3 documentation. Apply [CONVENTIONS.md](CONVENTIONS.md#1-public-names-layout-and-dependencies) and its status, operation and owner sections. Apply [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases) when inspecting multiple profiles; an ESS-only audit is insufficient.

### Concrete evidence to recheck

Use [DEVICE_EVIDENCE.md](DEVICE_EVIDENCE.md) to locate the new concrete intake. The public Lichuan example is a source to audit, not a naming or implementation baseline: it contains SDO-command, homing-width and ownership defects. Keep RS/I2C naming authority separate from device protocol authority, and record which source claims require corrections.

## Implement

1. Inspect this checkout, upstream, recent commits and existing changes. Create the single new execution branch according to COMMON; record promptset identity, base commit and upstream in `PROGRESS.md`. Preserve unrelated changes and original supplied material.
2. Create `docs/source_map.md` as a navigation table: responsibility, current declaration, implementation, real caller, focused test, intended target location and deliberate CAN difference. Include Status helpers, motion preparation/advancement/terminal context access, exact units, configuration generations, owner `admit/admitUrgent/service/result/release/cancel`, passive health, CLI correlation, runner evidence and package boundaries.
3. Create `docs/backlog.md` and `docs/roadmap.md` from the requirements matrix and ordered prompt manifest. Separate software tasks, documentary uncertainty, integration tasks and physical qualification. Allocate work to its numbered prompt; do not turn every pending feature into an obligation for the next coder.
4. Record tool availability and exact existing verification commands. Compare supplied audit revisions against current source. Mark behavior read from dirty files explicitly; a HEAD hash alone does not identify uncommitted behavior. Preserve differing I2C lifecycles instead of inventing a universal one.
5. Create `docs/hardware_bench.md` containing only known CAN facts and missing board, transceiver, drive, motor, firmware, wiring, node and bitrate details. RS COM-port defaults and free-shaft authorization are not CAN facts. Prompt 2 performs reference-byte and ledger intake work.

## Verify and exit

Check source-map paths against actual code and verify every selected naming/ownership claim with implementation and a caller or test. Run `python scripts/check_promptset.py` and `git diff --check`. Reuse a pinned passing sibling verifier result only with its revision and scope; rerun focused checks when drift or an unresolved question requires them. Do not run unchanged sibling suites merely to fill a report.

Exit when the live delta, source map, backlog, roadmap and bench inventory are usable. An unavailable optional checkout is an explicit evidence limitation, not a reason to abandon other audits. Close all owned scope and checks in `docs/reports/prompt_1.md`, update PROGRESS, then commit and sync completed changes on this execution branch. Useful verified checkpoints may be committed and synced earlier.

## Documentation and delegation

Follow [DOCUMENTATION.md](DOCUMENTATION.md#keep-documentation-beside-the-work) and [MAINTAINABILITY.md](MAINTAINABILITY.md#keep-the-smallest-design-that-expresses-the-real-contract); README must distinguish planning from implemented support. No new C++ declarations require Doxygen yet. Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), source, I2C/FieldCore and workflow audits can be independent read-only tasks. The lead owns naming decisions, the shared source map and Git. Require exact declarations and counterexamples from reviewers.
