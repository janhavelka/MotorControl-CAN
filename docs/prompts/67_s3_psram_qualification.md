# Prompt 67 ESP32-S3, PSRAM and further MCU qualification

Apply [COMMON.md](COMMON.md). Prerequisites: [53](53_multi_axis.md), [56](56_host_throughput.md), [65](65_application_package.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "PSRAM is untested, and there is no S3 board - make it todo for esp32s3
and similar MCUs i will have shortly"; "Only ESP32-S2/S3 - will that be a problem in the future to include more? If
not, no problem in that"; and earlier "the library should allow also psram allocations if feasible (future s3
projects/pcbs) at compile time to allow control of user-set amount of drives".

Qualify the library on the user's ESP32-S3 boards when they arrive: internal-RAM and PSRAM placement of the
application's storage (prompt 53 item 2), the user-set axis count, service gaps and throughput (prompt 56), and a short
regression of every feature on the same drives. Then add further MCUs one at a time: each new chip needs its own TWAI
(or other controller) adapter proof, because the exact TX retirement proof is chip and SDK specific. Adding a chip is
an adapter task, not an architecture change.

Boundaries: board facts are recorded, never assumed: module, PSRAM size and type (quad or octal), flash, CAN
transceiver, pins, termination, supply. TWAI driver, ISR and callback storage, the owner task stack and TCB stay in
internal RAM; only cold per-axis storage may go to PSRAM, allocated once at startup with an internal fallback, and
every block's placement is reported. No result from one board or chip is extended to another without its own run.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its S3 and PSRAM placement code; RS is
authoritative for ecosystem conventions. Inspect FieldCore-node read-only for board configuration ownership. If HEAD,
dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row
must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Read the [capacity study](../reports/2026-11-12_capacity_configuration.md), the
[S2 memory report](../reports/2026-11-12_s2_memory.md), the prompt 53, 56 and 65 reports, `adapters/twai/README.md`,
`docs/firmware_examples.md` and the ESP-IDF 5.5.5 PSRAM and TWAI documentation for the chip. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Record each S3 board's facts and a board configuration in the examples; flash the shared image and the bench image
   with the reviewed sdkconfig, one variant without and one with PSRAM.
2. Boot placement report (internal or external per block, free internal and PSRAM heap), then a short regression:
   reads, enable, moves, velocity, stops, console, on the same drives as the S2 record.
3. Capacity runs at the chosen axis counts with storage in internal RAM and in PSRAM: `maxServiceGapUs`, the route
   `gap=` figure, owner turn time, stop handoff time for N axes; state the largest count that passes.
4. Further chips (for example classic ESP32, C3, C6, P4) only when the user has the board: the adapter's retirement
   proof review for that chip and SDK, the compile fixture, then the same short regression.

## Verify and exit

Each claim names the board, chip revision, PSRAM configuration, firmware commit and evidence directory; unavailable
boards stay explicitly pending. Run the host gates for any fix and CI on the actual final commit.

## Documentation and delegation

Update the firmware guide, the TWAI adapter guide, the capacity sections of the owner guide, the hardware bench
record, coverage, backlog and CHANGELOG. One lead controls each port; agents analyze evidence read-only. Record
`docs/reports/prompt_67.md`. Commit and sync on the current branch before handoff.
