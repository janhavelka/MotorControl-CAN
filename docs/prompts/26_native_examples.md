# Prompt 26 Native examples and shared application

Apply [COMMON.md](COMMON.md). Prerequisites: [10](10_axis_coordinates.md), [19](19_typed_reads.md), [25](25_health_stats.md).

## Objective and boundaries

Deliver native examples and the small shared application used by later firmware and console prompts. A native C++ caller must be able to prepare exact coordinates, submit an explicit typed read through the owner, supply fake events and inspect a retained result without a framework, physical controller or CLI. Motion operations are added in prompts 30–36; do not invent temporary motion APIs here. TWAI, firmware builds and command parsing have their own following prompts.

## Read and audit

Refresh the current RS `ProbeApp`, `ProbePlatform`, owner examples and native package consumers. Read ADS1115/INA228 owner examples for supplied time, passive health and caller-owned storage; inspect FieldCore's ownership seam read-only. Record relevant source changes and current revisions under COMMON. Apply [CONVENTIONS.md](CONVENTIONS.md) for public/owner names, [MULTIDRIVE.md](MULTIDRIVE.md) for protocol-neutral application boundaries and the actual contracts produced by prompts 23–25. Read the example and source-comment sections of [DOCUMENTATION.md](DOCUMENTATION.md), with [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Reuse and extend prompt 10's `units_preview`: an offline example using explicit axis configuration, exact targets and public checked conversions. Show successful preparation, a meaningful rejection and unchanged output on failure. Register the existing example rather than creating a competing copy. Keep command increments distinct from encoder feedback; no CAN controller, framework or serial dependency is allowed.
2. Create `native_probe` fake mode with a small deterministic fake transport and the ordinary `CanBusOwner`. Demonstrate a typed identity/configuration read, supplied event/time progression, explicit result inspection and release. Include interleaved passive network observations. The fake is an example/test facility, not an installed driver or a substitute for hardware evidence.
3. Establish shared `ProbeApp.*` and `ProbePlatform.*` responsibilities around the existing owner. The application owns selection, application state and explicit requests. Platform seams provide time, bounded input/output and idle/diagnostic facilities. The owner remains the only CAN receiver. Keep firmware startup and future command parsing outside application behavior.
4. Bound input/output buffers and per-service work. Define what happens on exhausted output capacity, including drop counters where needed. No callback may invoke a second receive path, perform an unbounded wait or retain a borrowed codec buffer. Platform clock acquisition and later wrap extension stay outside the installed core.
5. Register native examples and focused fake integration tests in the existing build/verifier. Use the current installed public headers rather than repository-private helpers. Keep SDK-specific types out of their public includes and out of the exported package.

## Verify and exit

Build and run the offline example and fake native demonstration through real documented commands. Test read success, protocol failure, timeout, unrelated node traffic and retained-result lifetime. Verify that construction, selection, cached status/health and example startup produce no device command. Exercise bounded input/output under backpressure and prove owner service continues. Compile an isolated native consumer without sibling repositories, SDK headers or CAN hardware. Record measured example capacities and any unperformed check; do not label a fake run physically qualified.

## Documentation and delegation

Each example README states its purpose, dependencies, build/run command, required supplied configuration, expected output and fake/offline status. Add Doxygen for application/platform ownership and buffer lifetimes; update the source map and affected CHANGELOG entries. A normal engineering agent may own the offline example or fake test file after the lead fixes shared application contracts. Use a stronger ownership review if event routing or retained storage changes. Keep worker paths disjoint and all Git operations with the lead.

Close this prompt's owned scope and checks; record `docs/reports/prompt_26.md`, coverage and the next executable work. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
