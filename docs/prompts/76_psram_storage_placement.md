# Prompt 76 PSRAM-aware storage placement, stacks and memory reporting

Apply [COMMON.md](COMMON.md). Prerequisites: [53](53_multi_axis.md), [56](56_host_throughput.md), [75](75_runtime_node_table.md).

## Objective and boundaries

The user's instruction, verbatim (2026-11-17), answering that the console's call chain sits at its 6144-byte stack
limit: "increase if the mcu allows for it - also i want to design around using psram when the mcu allows for it (add
promptsets, get inspired with fieldcore-node)"; earlier (2026-11-12): "the library should allow also psram allocations
if feasible (future s3 projects/pcbs) at compile time to allow control of user-set amount of drives".

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

1. **Placement rule:** hot data (the owner, SDO channels, TWAI ISR and callback storage, stop and urgent reservations,
   stacks, TCBs) is always internal. Cold per-node blocks (sessions, retained results, record and trace rings, console
   scratch) may go to PSRAM through `StartupColdStorage`, each with its internal fallback and a reported placement.
2. **Allocation order:** every block allocated once at startup in a documented order, before the TWAI driver and owner
   start, with attempt-once flags; lazy allocation only for console or diagnostic scratch; no PSRAM allocation inside a
   control path.
3. **Capacity:** the node count is set at build time as the storage bound and at runtime by prompt 75's table; a request
   that does not fit is refused "requested N, fits M" before work is admitted, never shrunk silently. A descending
   capacity ladder is allowed only for cold history depth, and reports what it obtained.
4. **No PSRAM requirement loop:** a missing or smaller PSRAM is reported, and the image runs internally at the count
   that fits; any watchdog decision belongs to the application example (prompt 69).
5. **Stacks:** static internal stacks with high-water reporting and a numeric acceptance margin (for example at least
   4096 bytes free at the measured worst case). Measure the 16384-byte S2 stack of `db4e5f3e` on the bench and raise
   it further only with a measured reason, and size S3 stacks from
   their own measurement.
6. **Reporting:** prompt 75's `memory` command gains PSRAM total, free, low-water and largest block, the expected PSRAM
   from a per-board facts header compared with `heap_caps_get_total_size`, and the placement of every block; per-phase
   boot heap checkpoints; a bench-only failed-allocation ring.
7. **Console on S3:** USB Serial/JTAG with bounded publications; the S2 keeps its UART (or a measured USB OTG CDC).

Boundaries: board facts are recorded, never assumed; no result transfers between chips or boards; the portable core
stays allocation-free, and placement lives in the examples and the optional package.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its S3 and PSRAM placement seams; RS is
authoritative for ecosystem conventions. Inspect FieldCore-node read-only at `PsramAllocator.h`, `SystemRuntime.cpp`,
`StorageTask.cpp`, `StorageDiagnostics.cpp`, `SerialCli.cpp` and `WatchdogRuntime.cpp`, starting from the audit. If
HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling
row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Read the capacity study, the S2 memory report, the prompt 53, 56 and 75 reports,
`StartupColdStorage.h`, `check_stack.py`, `check_firmware.py`, the bench `main.cpp` and the ESP-IDF 5.5.5 heap and
PSRAM documentation. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. The placement table (block, size per node, hot or cold, placement rule) in the report and the firmware guide; every
   cold block through `StartupColdStorage` with a native failure seam per site.
2. The startup allocation order with attempt-once flags, and the capacity refusal.
3. The stack high-water measurement of the 16384-byte stack and the margin rule in the stack check.
4. The PSRAM rows, placement listing and boot checkpoints in the `memory` command; the per-board facts header.
5. The S3 console transport in the shared S3 image.

## Verify and exit

Host tests: each cold block's internal fallback, an injected failure per site, the capacity refusal text, placement
reported per block, and no PSRAM allocation after startup. Run the native, firmware (all storage variants), stack and
documentation gates and CI on the actual final commit. Bench on the S2: the memory report, stack high-water after the
console's deepest commands, and a short regression; the S3 board trials stay with prompt 67.

## Documentation and delegation

Update the firmware examples guide, the owner guide's capacity sections, the TWAI adapter guide, the bench README,
coverage, backlog, CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the hot/cold split; normal
coding agents own the fixtures and tests. The lead integrates and runs combined checks. Record
`docs/reports/prompt_76.md`. Commit and sync on the current branch before handoff.
