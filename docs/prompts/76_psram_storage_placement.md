# Prompt 76 PSRAM-aware storage placement, stacks and memory reporting

Apply [COMMON.md](COMMON.md). Prerequisites: [53](53_multi_axis.md), [56](56_host_throughput.md), [75](75_runtime_node_table.md).

## Objective and boundaries

The user's instruction, verbatim (2026-11-17), answering that the console's call chain sits at its 6144-byte stack
limit: "increase if the mcu allows for it - also i want to design around using psram when the mcu allows for it (add
promptsets, get inspired with fieldcore-node)"; earlier (2026-11-12): "the library should allow also psram allocations
if feasible (future s3 projects/pcbs) at compile time to allow control of user-set amount of drives".

Placement rule of 2026-11-18, the owner's words, verbatim: "Also always try to use as much PSRAM as possible if the MCU
allows - check diligently sensitive part of the implementation there are some prohibited stuff that should not go to
psram - check that now and edit the instructions in this regard". This replaces the earlier hot/cold split below as
the default: external RAM first for everything the chip allows, internal RAM for the prohibited list and the hot set.

Facts at the time of writing: the S2 bench task stack was 12288 bytes with a 6144-byte `scripts/check_stack.py` chain
limit (`CHAIN_LIMIT`), which the console's deepest chain reached exactly on 2026-11-16; `db4e5f3e` (2026-11-17) raised
`FIRMWARE_STACK_BYTES` to 16384 and `CHAIN_LIMIT` to 8192 (static DRAM had about 95 KB of heap beside it; the
high-water mark on the bench is not yet measured); route records reported
about 61.8 KB internal heap free and a 45 KB largest block during the two-axis trials
([HIL report](../reports/2026-11-16_hil_campaign.md)). `examples/common/StartupColdStorage.h` already allocates one
startup block externally with one internal fallback and reports `ColdStoragePlacement`; `scripts/check_firmware.py`
builds `storage-s2`, `storage-s2-psram`, `storage-s3` and `storage-s3-psram` fixtures. FieldCore-node places cold
buffers in PSRAM per call site, keeps hot owner data, stacks and TCBs internal, and enforces PSRAM with a watchdog reset
loop ([audit](../reports/2026-11-17_fieldcore_capacity_psram_audit.md)). [Prompt 67](67_s3_psram_qualification.md)
qualifies the user's S3 boards; it consumes this prompt's design when its boards arrive.

Deliver:

1. **Placement rule, PSRAM first.** Every block, ring, table, history, archive, console and trace buffer and
   application store is placed in external RAM when the board has it. Internal RAM is only for (a) the prohibited
   list below, each row with its reason and source, and (b) the hot set. Prompt 53's earlier choice (only cold archival
   bytes external) is superseded as the default. The hot set is what the service and stop paths touch each turn: the
   owner with its request/result slots, SDO channels, stop and urgent reservations, every live `AxisSession` (it embeds
   pending and urgent work, both stop contexts, best-effort stop state and the live move, replacement, enable and
   motion contexts) and every other active operation context, and the TWAI adapter object, its callbacks and `user_ctx`.
   The hot set stays internal until a measurement on that board promotes a member (item 8); then a stop depends on
   PSRAM presence and cache-miss timing only as far as that board's recorded verdict allows. Internal RAM bounds the
   session count until a promotion; the placement table reports each session's internal bytes and the count that fits.
   Moving part of a session to PSRAM to raise that count is a separate reviewed split that names the exact members and
   their lifetimes first, and it keeps all stop and urgent state internal until promoted.
   **Prohibited in external RAM** (ESP-IDF 5.5.5, the revision `verify.yml` pins; sources read 2026-11-18, see the list
   after the table):

   | Item | S3 | S2 | Reason |
   | --- | --- | --- | --- |
   | Code, callbacks, context and data of an `ESP_INTR_FLAG_IRAM` interrupt, and tick hooks | internal | internal | With the cache disabled (flash write) external RAM raises an illegal cache access exception; only IRAM-safe handlers run. TWAI with `CONFIG_TWAI_ISR_CACHE_SAFE` needs callbacks and `user_ctx` internal. This adapter requires that option off (`IdfTwaiDriver.cpp` `#error`), so its callbacks run only with the cache on; their data is hot (b), and internal if the option is ever enabled |
   | Stack of a task that calls a flash API (NVS, partition, `esp_flash`) or runs with the cache disabled | internal | internal | `CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM` help: only for tasks whose stack is never accessed with the cache disabled. A flash call runs on the caller's stack (read from `esp_flash_api.c`, not tested) |
   | Stack of any other task (owner, console) | allowed | allowed | `xTaskCreate` always allocates internal. External needs `ALLOW_EXT_MEM` (default y with PSRAM except on the original ESP32), `xTaskCreateStaticPinnedToCore` with a `heap_caps` buffer and the TCB internal. The owner stays hot (b) |
   | DMA descriptors | internal | internal | external-ram.rst Restrictions, both chips |
   | DMA buffers | `MALLOC_CAP_SPIRAM \| MALLOC_CAP_DMA` possible, bandwidth "very limited" | not supported in ESP-IDF: `MALLOC_CAP_DMA \| MALLOC_CAP_INTERNAL` | external-ram.rst; `mem_alloc.rst` says EDMA allows external DMA buffers on every chip but the ESP32; for the S2 the chip's own Restrictions entry governs. TWAI is not a DMA peripheral in this adapter; the UART and S3 USB Serial/JTAG buffers belong to the SDK drivers, and any buffer the application passes to a DMA peripheral is internal |
   | FreeRTOS TCBs, queues, semaphores, mutexes, event groups, timers, `portMUX` spinlocks | internal | internal | `heap_idf.c`: FreeRTOS dynamic memory is `MALLOC_CAP_INTERNAL \| MALLOC_CAP_8BIT` because objects must be reachable with the cache disabled. Static creation accepts any memory; keep it internal (inference). A ring buffer from `xRingbufferCreateStatic` may be external only when no interrupt uses it |
   | Atomics and lock words shared between cores or with an interrupt | internal | internal | S3: the native `S32C1I` cannot address external RAM; the SDK works around it in `esp_cpu_compare_and_set` (`cpu.c`) and, for GCC only, in `std::atomic` (`STDATOMIC_S32C1I_SPIRAM_WORKAROUND`, `components/newlib/Kconfig:210`, off with clang). S2: the core has no `S32C1I` (`core-isa.h` 0; S3 is 1) and how the SDK serves atomics in external RAM was not read. Keep internal on both |
   | The record kept across reset (boot log, `__NOINIT_ATTR`) | internal or RTC until measured | same | external noinit is "not initialized or modified even during startup or restart" (external-ram.rst); retention across a watchdog reset is not established for either chip |

   Not prohibited: the source buffer of a flash write (the SDK copies a buffer outside DRAM through a stack buffer,
   `esp_flash_api.c` 1128-1138) and a flash read into external RAM (a temporary internal buffer or
   `ESP_ERR_NO_MEM`, lines 957-1012). Read from source at 5.5.5, not tested on the S2 or S3; the old rule that every
   buffer a flash write reads is internal is dropped.
   Read at v5.5.5 on 2026-11-18: `external-ram.rst`, `Kconfig.spiram.common`, the S2 data cache choice,
   `freertos/Kconfig`, `heap_idf.c`, `cpu.c`, `newlib/Kconfig`, `esp_flash_api.c`, both `core-isa.h`, `mem_alloc.rst`,
   `spi_flash_concurrency.rst`, `memory-types.rst`, `freertos_additions.rst`, `twai.rst` and
   `esp_driver_twai/Kconfig`. Not read at v5.5.5: the chip errata and the S2 and S3 technical reference manuals (the
   S3 cache-hit errata is bypassed by ESP-IDF 5.0.4 and later per the errata page read at its "latest" revision; it
   gives no placement rule).
   Chip facts: external address space S3 32 MB shared with flash instructions and rodata, S2 10.5 MB. PSRAM shares
   the cache with flash: frequently used data runs near internal speed, a scan above 32 KB runs at PSRAM speed and can
   push cached flash code out, so a cold scan (trace dump, archive, console) can add control-path jitter; measure it
   (item 8). Octal PSRAM at 120 MHz is an S3 option with restrictions. `CONFIG_SPIRAM_XIP_FROM_PSRAM` is not adopted.
   `malloc()` with `SPIRAM_USE_MALLOC` sends requests up to `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` (16384) to internal
   RAM first; the explicit `heap_caps_aligned_alloc(..., MALLOC_CAP_SPIRAM)` form ignores it and is the only form
   used for a block that must be external.
2. **Allocation order:** every block, console and diagnostic scratch included, allocated once at startup in a
   documented order, before the TWAI driver and owner start, with attempt-once flags; no cold-block allocation,
   external or internal fallback, after startup, so first console or diagnostic use allocates nothing. Every external request is made per block with an
   `OnShortfall` setting, item 4. Mutually exclusive
   diagnostic tools (soak, silent capture, PDO watch) share one startup-reserved arena sized to the largest, claimed
   and returned at the tool's start and end, so preallocation fits the S2. Measure that budget first: the
   [S2 memory report](../reports/2026-11-12_s2_memory.md) found about 20,644 bytes free after the console, before
   the 4096-byte stack increase.
3. **Capacity:** the node count is set at build time as the storage bound and at runtime by prompt 75's table; a request
   that does not fit is refused "requested N, fits M" before work is admitted, never shrunk silently. A descending
   capacity ladder is allowed only for cold history depth, and reports what it obtained.
4. **Missing PSRAM, fallback per block.** Each block's `OnShortfall` is `REFUSE` (startup ends with "requested N,
   fits M") or `INTERNAL` (one internal attempt, then the same refusal); one attempt each, no retry loop. A missing or
   smaller PSRAM is reported, and the image runs internally at the count that fits; a PSRAM requirement and any
   watchdog decision belong to the application example (prompt 69). The ESP-IDF image keeps
   `CONFIG_SPIRAM_IGNORE_NOTFOUND` on and uses no static external placement (`EXT_RAM_BSS_ATTR`, external noinit): the
   ignore option is unavailable with them (`Kconfig.spiram.common`, v5.5.5), so the image could not run without
   PSRAM. The S2 bench PCB (ESP32-S2-MINI-2-N4, ROM reports S2FH4 with no PSRAM, [bench record](../hardware_bench.md))
   has none, so on it every block falls to internal RAM and the PSRAM-first rule changes nothing there; its
   `ESP32S2_DATA_CACHE_0KB` setting depends on `!SPIRAM` (S2 cache Kconfig, v5.5.5), so an S2 image with PSRAM needs
   an 8 KB or 16 KB data cache and gives back that heap; the `storage-s2-psram` fixture is compile-only. No S3 board
   is recorded yet (prompt 67): its PSRAM type and size are recorded there, never assumed.
5. **Stacks:** high-water reporting and a numeric acceptance margin (for example at least 4096 bytes free at the
   measured worst case). Stacks are static (`xTaskCreateStaticPinnedToCore`; today static arrays). The owner task
   stack stays internal until promoted. A task that never calls a flash API, such as the console on the S2 or S3, may
   take a static stack from external RAM (`ALLOW_EXT_MEM`, a `heap_caps` buffer allocated at startup, TCB internal);
   this may relieve the console's 8192-byte `CHAIN_LIMIT` pressure, and the high-water mark is measured there too. A
   task that calls NVS or any flash API (prompt 70) keeps an internal stack. Measure the 16384-byte S2 stack of
   `db4e5f3e` on the bench and raise it further only with a measured reason, and size S3 stacks from their own
   measurement.
6. **Reporting:** prompt 75's `memory` command gains PSRAM total, free, low-water and largest block, the expected
   PSRAM from a per-board facts header compared with `heap_caps_get_total_size`, and for every block its class (fixed
   internal, hot, default), reason, placement and promotion state; per-phase boot heap checkpoints; a bench-only
   failed-allocation ring.
7. **Console on S3:** USB Serial/JTAG with bounded publications; the S2 keeps its UART (or a measured USB OTG CDC).
8. **Hot-set promotion by measurement:** a hot member moves to external RAM only after a run on that board shows the
   external placement meets the stated service-gap and stop-handoff budget under load, including a concurrent cold scan
   above 32 KB (trace dump, archive read, console output); the verdict is per board, chip revision, PSRAM mode and
   speed, ESP-IDF version and configuration, and records the budget, figures and evidence directory. Atomics and
   lock words, interrupt-touched data, DMA storage and kernel objects are never promoted (table).
9. **Startup block class:** `StartupColdStorage` becomes the general startup block (a rename is allowed with the
   contract kept): each block declares its class (fixed internal, hot or default), its reason and its `OnShortfall`.
   A request for external memory by a fixed-internal or unpromoted hot block is refused at plan time.

Boundaries: PSRAM-first applies only where the board has PSRAM; board facts are recorded, never assumed; no result transfers between chips or boards; the portable core
stays allocation-free, and placement lives in the examples and the optional package.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its S3 and PSRAM placement seams; RS is
authoritative for ecosystem conventions. Inspect FieldCore-node read-only at `PsramAllocator.h`, `SystemRuntime.cpp`,
`StorageTask.cpp`, `StorageDiagnostics.cpp`, `SerialCli.cpp` and `WatchdogRuntime.cpp`, starting from the audit. If
HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling
row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Read the capacity study, the S2 memory report, the prompt 53, 56 and 75 reports,
`StartupColdStorage.h`, `check_stack.py`, `check_firmware.py`, the bench `main.cpp` and the ESP-IDF 5.5.5 heap and
PSRAM documentation and sources named in item 1. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. The placement table (block, size per node, class, reason, placement rule) in the report and the firmware guide;
   every block through `StartupColdStorage` with a native failure seam per site, external first except the prohibited
   list and the hot set.
2. The startup allocation order with attempt-once flags, and the capacity refusal.
3. The stack high-water measurement of the 16384-byte stack and the margin rule in the stack check.
4. The PSRAM rows, placement listing and boot checkpoints in the `memory` command; the per-board facts header.
5. The S3 console transport in the shared S3 image.

## Verify and exit

Host tests: each block's `REFUSE` and `INTERNAL` fallback, an external request by a fixed-internal or unpromoted
hot block refused, zero PSRAM, an injected failure per site, the capacity refusal text, placement
reported per block, a placement inventory showing the owner, every live session, all stop and urgent storage, the TWAI adapter,
kernel objects, atomics, lock words and ISR-touched data internal and every other block external when available, ordinary and urgent work still available at the reported capacity with external memory refused, and allocator
spies proving no block allocation after startup, including first console and diagnostic use during service. Run
the native, firmware (all storage variants), stack and documentation gates and CI on the actual final commit. Bench on
the S2 (no PSRAM, all internal): the memory report, stack high-water after the console's deepest commands, and a short regression; the S3
board trials, PSRAM timing and promotions stay with prompt 67.

## Documentation and delegation

Update the firmware examples guide, the owner guide's capacity sections, the TWAI adapter guide, the bench README,
coverage, backlog, CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the prohibited list and every promotion; normal
coding agents own the fixtures and tests. The lead integrates and runs combined checks. Record
`docs/reports/prompt_76.md`. Commit and sync on the current branch before handoff.
