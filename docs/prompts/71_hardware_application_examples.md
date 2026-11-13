# Prompt 71 Hardware qualification of the application examples

Apply [COMMON.md](COMMON.md). Prerequisites: [66](66_hardware_regular_operation.md), [69](69_application_io_watchdog.md), [70](70_startup_settings_reboot.md).

## Objective and boundaries

Qualify on real hardware what prompts 68 to 70 delivered: the CAN task structure, the headless job runner, the startup
checks, settings storage and reboot handling on the two-drive S2 bench; and the hardware watchdog, stop line, MCU
inputs and outputs once the user has built and wired that hardware (a physical action by the user). Repeat on the
ESP32-S3 boards once prompt 67 has brought them up. Results stay scoped to the actual board, chip, wiring, drives,
firmware and configuration; nothing transfers between boards. Apply the applicable bench authorization and the
evidence rules of [prompt 47](47_hardware_finite.md) and [prompt 48](48_hardware_stress.md).

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its hardware campaign conventions for this seam;
RS is authoritative. Inspect FieldCore-node read-only. If HEAD, dirty state and seam match the last recorded row, write
"unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and
symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read reports 66 to 70, the
host-loss guide, the hardware bench record and the wiring the user provides. Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md).

## Implement

1. A short existing-feature regression on the image under test, then each of prompts 68 to 70's focused checks on the
   combined image with raw traffic, timing, service gaps and stack margins.
2. A headless job soak (at least 2 hours, attended or under a written waiver for that run) on both axes with stops,
   injected SDO timeouts, an absent node and console traffic, counting every refusal, self-repair and uncertain result.
3. When the hardware exists: every prompt 69 trial on the wired board (each stop-line failure path, watchdog trip,
   frozen pin, button, limit, permit, relay, brake), the attended host-loss trial, and timing from each trigger to the
   drive's reported standstill.
4. Root-cause every anomaly with a native regression where representable; a later passing run is not a fix.

## Verify and exit

Each qualified claim names its board, chip, wiring, drives, firmware, configuration and evidence directory; trials
without hardware stay pending and explicit. Run the appropriate host gates for any fix and CI on the actual final
commit.

## Documentation and delegation

Update coverage, the hardware bench record, the host-loss and firmware guides' qualification notes, backlog and
CHANGELOG. One lead alone controls the port and CAN requests; agents analyze evidence read-only. Record
`docs/reports/prompt_71.md`. Commit and sync on the current branch before handoff.
