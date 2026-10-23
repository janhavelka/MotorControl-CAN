# MotorControl-CAN engineering guidance

## Current assignment and scope

This checkout is currently an audited implementation promptset, not a working driver. Do not report planned APIs, tests or hardware behavior as implemented. The library package name is `MotorControl-CAN`; the proposed C++ namespace, include directory and CMake identity are `MotorControlCAN`. Implement only when instructed to execute the promptset. Preserve the original references and their provenance.

Read `docs/prompts/README.md`, `docs/prompts/COMMON.md`, the active numbered prompt, the three reports in `docs/audit/`, and the applicable parent `AGENTS.md` before work. The parent Projects CI policy applies. Local Markdown in this checkout is repository documentation; keep it here.

## Recurring source audit

For **every implementation, review or audit prompt**, inspect current `../MotorControl-RS` and the relevant sibling I2C implementations, callers, examples, tests, runners and CI. At minimum refresh MotorControl-RS, ADS1115 and INA228; select SHT3x-main, BME280, MB85RC, MCP45HVX1, PCA9555, RV3032-C7 and the stateless codec siblings according to the affected seam. Inspect current FieldCore-node read-only for ownership/integration changes. Record paths, revisions, dirty state, relevant behavior and intentional differences in the stage report. Do not rely solely on these historical audit reports or copy names from an old prompt.

MotorControl-RS is authoritative for common motion names/meaning; I2C libraries establish ecosystem style and owner integration, not a single universal lifecycle. Native CANopen names describe genuinely different services. Keep `Status`, protocol errors, drive state, command outcome and application health distinct. Never import sensor retry policy into motor commands.

## Ownership and portability

- Keep the installed core C++11, deterministic and bounded, with free functions, fixed value types and caller-owned storage/contexts. No Arduino/ESP-IDF/FreeRTOS/FieldCore includes, device I/O, clocks, sleeps, logging, heap allocation, background tasks, automatic retries or application persistence.
- Applications own CAN controller/stack instances, time, scheduling, queues, node bindings, caches, health policy, retries and physical machine workflows. One local owner receives and dispatches every frame on a physical interface; asynchronous heartbeat/PDO/EMCY handling continues during SDO transactions.
- Reuse exact unit semantics and operation-specific `prepare...`, `next...`, `advance...`, `get...Result` vocabulary. Avoid speculative universal engines and runtime plugin systems. Evaluate a proven CANopen stack before choosing service implementation; keep stack/SDK dependencies outside the installed portable core.
- Keep board configuration/adapters and CLI in examples. Arduino and native ESP-IDF must use the same application and command semantics. Provide native usage independent of either framework. FieldCore remains read-only unless separately authorized for integration.
- Preparation and parsing publish outputs atomically on success. Document any explicit count-reset exception. Codec input buffers are borrowed during calls only; admitted work and retained results own their data.

## Evidence and motor behavior

Use original vendor PDF pages and EDS together. Extracted tables can be wrong. Record object index/subindex, type, access, units, PDO capability, model/firmware scope, page and contradictions before implementation. Never turn a generic CiA402 definition or another drive's manual into a CL86-C capability. Keep documented, implemented, host-tested and hardware-qualified coverage separate.

No implicit enabling, movement, homing, fault clearing, NMT reset/start, parameter save, node/bitrate change or PDO reconfiguration on startup, selection, probe, recovery or cached reads. Local cancel/recovery is not a motor stop. CAN TX success/ACK is not SDO success, drive acceptance or motion completion. Preserve execution uncertainty after an unconfirmed write and never automatically replay motion or re-enable after reboot/recovery.

Distinguish controller state, CANopen NMT state, CiA402 drive state, mode, heartbeat freshness, fault events and application health. Boot-up/reset/bus-off/rebinding/configuration changes invalidate dependent evidence and queued work. No health recovery clears a drive fault implicitly. An NMT stop is not a CiA402 quick stop. Host heartbeat observation does not establish drive action on host loss.

The RS bench authorization and wiring are not CAN bench facts. Host-only work can proceed without hardware. Before live CAN/motion work establish the actual board, transceiver, drive/motor/firmware, wiring, node/bitrate and applicable user authorization. Do not guess defaults from the RS setup. Missing hardware blocks qualification, not independent implementation and testing.

## Work and verification

Keep changes scoped, inspect actual CI before implementation, and use its commands. Establish host gates early and maintain generated-file freshness, packaging and example parity. Tests must exercise observable contracts and adversarial evidence, not merely mirror helpers. Never weaken tests to make a stage green.

Keep the promptset's working branch when one exists; do not create per-prompt branches or worktrees without need/authorization. Preserve concurrent/unrelated work. The lead owns shared Git mutations and records exact revisions. This planning checkout initially has no Git repository or remote: do not fabricate a remote, upstream sync, CI result or release. Publishing and repository creation follow the user's applicable instructions, not assumptions copied from another repository.

Maintain a stage report, object/API/CLI coverage, backlog and roadmap as work proceeds. Mark software checks, device evidence and remaining gaps independently. For major implementation milestones verify required CI on the actual final commit when a remote exists; report unavailable checks honestly. Documentation-only promptset changes need local validation, not remote CI.
