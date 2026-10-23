# Audit findings and implementation implications

Audited 23 October 2026. The requested planning deliverable is complete: source audits, local vendor evidence, copied baseline and staged implementation prompts. The future library remains unimplemented.

The user's subsequent scope expansion has been researched and incorporated: **one library for stepper and servo CAN drives from multiple manufacturers**, with common motion above separate protocol/device profiles. CANopen/Lichuan remains first; N5 and EPOS4 are mandatory design contrasts. Read [the decision](../multidrive_decision.md), [CANopen market survey](CANOPEN_DRIVE_SURVEY.md), [other protocol survey](OTHER_CAN_PROTOCOLS.md) and [fresh source audit](MULTIDRIVE_SOURCE_REAUDIT.md). The original baseline evidence below remains dated to its captured state.

## The baseline is suitable, with specific boundaries

MotorControl-RS at `5da4ba7f13be457ea7bf9706ffd7d546821669c7` already separates pure C++11 units/axis/profile operations from application transport, health, CLI and platform adapters. Its maintained architecture explicitly anticipates an independent CANopen library with the same motion vocabulary. Preserve the concrete `prepare*`/`next*`/`advance*` contexts, checked read getters and retained outcome fields. There is no generic installed `getMoveResult` family.

Its current quick verifier passed **78 registered tests**, **3 source-consumer tests** and **3 installed-consumer tests**, with all 15 verifier steps retained in [the evidence pack](evidence/motorcontrol_rs_quick/README.md). No source changes, firmware builds, remote CI or hardware tests were performed. Details and exact source anchors are in [the RS audit](MOTORCONTROL_RS_AUDIT.md).

The source was clean during that audit and snapshot. Later in the same work session, concurrent documentation edits appeared in MotorControl-RS while its HEAD remained unchanged. The immutable copied source still verifies; [the final drift note](FINAL_REVIEW.md) records why a live byte comparison now differs. The passing tests are evidence for the original audited state, not a claim to have verified concurrent work.

A mechanical rename would copy incorrect assumptions: serial route metadata, RTU CRC/echo/silence handling, ESS native RPM widths/ramp policy, sample scale/origin assumptions, PSRAM example requirements and tested speed limits. The CAN design replaces those responsibilities while preserving upper-level meaning.

## Ecosystem consistency is semantic

The eight audited I2C libraries share explicit ownership, bounded work, status/error reporting, testable adapters and diagnostic tooling. They do not share one exact lifecycle or health policy. The [ecosystem audit](ECOSYSTEM_CONVENTIONS.md) records the differences rather than choosing arbitrary aliases.

Current FieldCore-node contains a real but dormant RS motor integration with independent admission, wire and execution outcomes. It supplies a concrete consumer example; it is neither deployed CAN support nor a core dependency. Future CAN integration needs an application CAN owner/module boundary and remains separate from this repository's implementation.

## Manufacturer evidence changes the plan

The [reference pack](../reference/README.md) contains five official manufacturer-linked PDFs, the EDS and two source-page snapshots. Earlier RS feasibility work could not retrieve them; this audit obtained the current originals. The manuals include the hardware information needed for this drive-interface library; no internal IC was identified that would justify a chip datasheet claim.

The [protocol audit](DEVICE_PROTOCOL_RESEARCH.md) identifies consequential conflicts in status bits, velocity start order, access/signedness, subdivision, I/O encodings and stop details. Ramp parameters use milliseconds. The EDS has 99 object indexes and generic identity/default values; it is not live device configuration or a safe initialization script. Firmware-matched state/trigger/completion behavior must be established before affected capabilities can be enabled.

CL86-C is the first target. Grouped manuals also cover OL86-C and CL57-C/OL57-C, with distinct encoder/model facts. Shared documents or CiA402 marketing do not establish interchangeable behavior, host-loss stopping or support for cyclic/torque modes. Model/firmware qualification is explicit.

## Implementation handoff

The [20 numbered prompts](../prompts/README.md) build from evidence and stack selection through core APIs, CANopen services, motion, owner/health, shared firmware/CLI, Python runners, adversarial tests, hardware qualification and release. Every stage requires a fresh relevant sibling-code audit and records actual coverage. [The architecture](../architecture_plan.md) and [naming map](../api_cli_mapping.md) make the intended structure concrete.

All confirmed documented native features must receive typed access and tests; unresolved documentary behavior remains a named gap. This distinction lets host development proceed without falsely declaring the whole family qualified. The planning checker verifies artifact hashes, links and stage completeness; it does not validate a future CAN implementation.
