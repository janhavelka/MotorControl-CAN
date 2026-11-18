# Prompt 67 ESP32-S3, PSRAM and further MCU qualification

Apply [COMMON.md](COMMON.md). Prerequisites: [53](53_multi_axis.md), [56](56_host_throughput.md), [65](65_application_package.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "PSRAM is untested, and there is no S3 board - make it todo for esp32s3
and similar MCUs i will have shortly"; "Only ESP32-S2/S3 - will that be a problem in the future to include more? If
not, no problem in that"; and earlier "the library should allow also psram allocations if feasible (future s3
projects/pcbs) at compile time to allow control of user-set amount of drives".

Placement rule of 2026-11-18, the owner's words, verbatim: "Also always try to use as much PSRAM as possible if the MCU
allows - check diligently sensitive part of the implementation there are some prohibited stuff that should not go to
psram - check that now and edit the instructions in this regard". Prompt 76 turns this into PSRAM-first placement with a
prohibited list and a hot set; this prompt measures it on the boards.

Qualify the library on the user's ESP32-S3 boards when they arrive: internal-RAM and PSRAM placement of the
application's storage (prompt 53 item 2), the user-set axis count, service gaps and throughput (prompt 56), and a short
regression of every feature on the same drives. Then add further MCUs one at a time: each new chip needs its own TWAI
(or other controller) adapter proof, because the exact TX retirement proof is chip and SDK specific. Adding a chip is
an adapter task, not an architecture change. [Prompt 76](76_psram_storage_placement.md) (added 2026-11-17) defines the
placement rule, allocation order, stack margin and memory report this qualification measures; although numbered later,
its result is an evidence gate for items 1 to 3 here.

Boundaries: board facts are recorded, never assumed: module, PSRAM size and type (quad or octal), flash, CAN
transceiver, pins, termination, supply. Prompt 76's prohibited list stays internal on every board (ISR-safe
code and data, stacks of tasks that call flash APIs, DMA descriptors and buffers, kernel objects, atomics and lock
words); the hot set (owner, live sessions, SDO channels, stop and urgent storage, TWAI adapter, owner stack) stays
internal until a run here promotes a member; everything else goes to PSRAM first, allocated once at startup with a
per-block `REFUSE` or `INTERNAL` fallback, and every block's placement is reported. The bench S2 PCB
(ESP32-S2-MINI-2-N4, no PSRAM) runs all blocks internally; an S2 with PSRAM is qualified only if the user has one, and
needs a nonzero data cache (`ESP32S2_DATA_CACHE_0KB` requires no PSRAM). No result from one board, chip, PSRAM mode or
ESP-IDF version is extended to another without its own run.

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

1. Record each S3 board's facts (including PSRAM mode, size and speed: quad or octal, 40, 80 or 120 MHz) and a board
   configuration in the examples; flash the shared image and the bench image with the reviewed sdkconfig, one variant
   without and one with PSRAM (`SPIRAM_IGNORE_NOTFOUND` on, no static external segments).
2. Boot placement report (internal or external per block, free internal and PSRAM heap), then a short regression:
   reads, enable, moves, velocity, stops, console, on the same drives as the S2 record.
3. Capacity runs at the chosen axis counts with storage all internal and PSRAM-first: `maxServiceGapUs`, the route
   `gap=` figure, owner turn time, stop handoff time for N axes; state the largest count that passes.
4. Measurements of the PSRAM-first rule, each with its budget stated before the run and its figures recorded:
   a. Hot-set promotion trials, one member at a time (owner request/result slots, `AxisSession`, SDO channels, stop and
      urgent reservations, the TWAI adapter object and callbacks): internal against external, with the item 3 figures
      and the stop handoff time. A member is promoted only if the budget holds; the verdict is per board, chip
      revision, PSRAM mode and speed, ESP-IDF version and configuration.
   b. Cold-scan interference: the same figures while a scan above 32 KB (trace dump, archive read, console output)
      runs concurrently, since PSRAM shares the cache with flash code.
   c. Stacks: the console task's stack in PSRAM (static task, internal TCB, `ALLOW_EXT_MEM`) against internal:
      high-water mark and service gaps; the owner stack stays internal unless 4a promotes it.
   d. Flash operation with PSRAM-first placement (prompt 70's NVS write, no axis enabled, no SYNC): no illegal cache
      access exception, the TWAI service gap across the write (the adapter runs with `TWAI_ISR_CACHE_SAFE` off), and
      the result after the write.
   e. Missing PSRAM: the PSRAM image on a module without PSRAM, if one is on hand, boots and runs internally at the
      reported count; otherwise the host refusal tests stand alone and the run is listed as pending.
   f. Reset retention of the boot log in internal noinit or RTC against external noinit after a watchdog reset, to
      establish whether external noinit survives; until then the record stays internal.
   g. The toolchain (GCC or clang) is recorded, because the atomics workaround for external RAM exists for GCC only;
      atomics and lock words stay internal either way.
5. Further chips (for example classic ESP32, C3, C6, P4) only when the user has the board: the adapter's retirement
   proof review for that chip and SDK, the compile fixture, the chip's own PSRAM and DMA restrictions read from its
   ESP-IDF documentation, then the same short regression.

## Verify and exit

Each claim names the board, chip revision, PSRAM configuration (mode, speed, size), ESP-IDF version, firmware commit and
evidence directory; unavailable
boards stay explicitly pending. Run the host gates for any fix and CI on the actual final commit.

## Documentation and delegation

Update the firmware guide (its memory placement section, with prompt 76), the TWAI adapter guide, the capacity sections of the owner guide, the hardware bench
record, coverage, backlog and CHANGELOG. One lead controls each port; agents analyze evidence read-only. Record
`docs/reports/prompt_67.md`. Commit and sync on the current branch before handoff.
