# Prompt 75 Runtime node table, monitor-only drives and a flexible bench

Apply [COMMON.md](COMMON.md). Prerequisites: [53](53_multi_axis.md), [70](70_startup_settings_reboot.md), [73](73_timing_freshness_guards.md).

## Objective and boundaries

The user's question, verbatim (2026-11-17): "you are saying these are compile time things and that bench tests are
kind of clumsy and not flexible to add another dive on the fly or monitor more drives? Btw whats the possible baud
rate? Thats the MCU baud between the mcu and computer, right? Otherwise the CAN goes much faster with 1mbps, right?
Could the baud be increased?"; and generally: "In general we are after good engineering practises and more
responsibility on the end user of this firmware with possible optional settable guards".

Facts at the time of writing: adapter capacity is compile-time (`adapters/canopennode/Capacity.h`,
`MOTORCONTROLCAN_SDO_CHANNELS`, default 2; `MOTORCONTROLCAN_PDO_ROUTES`); the S2 bench fixes two nodes (`BENCH_NODES`,
`examples/s2_bench/BenchRoute.h`) and holds one bench record at a time in one shared arena
(`examples/s2_bench/main/main.cpp`); every bench T line is refused while the console retains an operation. The
console and bench lines share UART0 at 115200 baud (`examples/common/EspProbePlatform.cpp`, `config.baud_rate`;
the bench banner "UART115200"), the link between the MCU and the PC, while CAN runs at 1 Mbit/s; one route record's
dump took about 2.7 s there ([HIL report](../reports/2026-11-16_hil_campaign.md)). The PC side is the ESP-Prog-2, an
ESP32-S3 running esp-usb-bridge over USB full speed, with no UART rate limit documented
(<https://docs.espressif.com/projects/esp-dev-kits/en/latest/other/esp-prog-2/user_guide.html>,
<https://github.com/espressif/esp-usb-bridge>); the S2 and S3 UARTs allow up to 5 Mbit/s (`SOC_UART_BITRATE_MAX` in
ESP-IDF 5.5.5's `soc_caps.h`). Eleven runner and test scripts pin 115200 (`s2_bench.py`, `s2_bench_console.py`,
`s2_console.py`, `s2_console_account.py`, `s2_flash.py`, `s2_pdo.py`, `s2_soak.py`, `s2_uart_compare.py` and tests).
FieldCore-node keeps a compile-time inventory with runtime per-instance settings
([audit](../reports/2026-11-17_fieldcore_capacity_psram_audit.md)); the user wants more than that.

Deliver:

1. **Runtime node table** up to the compile-time maximum: entries (node ID, compiled-in profile, role `controlled` or
   `monitor-only`, enabled, expected product identity) added, removed or rebound at runtime, each only while that
   entry is idle (no admitted work, no unconfirmed write, no motion uncertainty). Removal or rebinding invalidates that
   node's evidence and queued work; adding sends no NMT, enables nothing and moves nothing. A disabled entry stays
   visible and bus-silent. A request beyond capacity is refused with "requested N, fits M" before anything changes. The
   persisted form is [prompt 70](70_startup_settings_reboot.md)'s axis table; this prompt adds the runtime edits and the
   role.
2. **Monitor-only drives:** passive heartbeat, EMCY and TPDO observation per entry, with a settable observation period
   and a separate stale threshold (FieldCore separates 5000 ms cadence from a 15000 ms stale threshold); SDO polling of
   monitored drives is bounded by host TX turns and bus time and reported as such.
3. **Bench flexibility:** the S2 bench takes its node set from the table instead of `BENCH_NODES`; route records per
   node instead of one global record where the owner and memory allow (the paired arming remains); arming, dump and
   release that never block another node's console work; record dumps that never delay motion or stop replies.
4. **Console transport and bounds:** one shared console rate constant in the firmware and in every runner, with an
   override option. Measure it on the fixture at 921600, 2000000 and 3000000 baud with the platform's input and output
   loss counters and a long dump, and choose the highest clean rate as the default (unless the 2026-11-17 change
   already did). Recorded evidence keeps the rate it was taken at. S3 boards use USB Serial/JTAG instead
   ([prompt 76](76_psram_storage_placement.md)). Adopt bounded publications: a per-response byte budget, whole-record
   discard with a counter, non-blocking chunked writes, one command per poll.
5. **Memory report:** a console `memory` command with internal and 8-bit heap total, free, low-water and largest block,
   each task's configured stack and minimum free, and the size of every per-node block; prompt 76 adds PSRAM rows and
   placement.

Boundaries: capacity stays a compile-time storage bound; the runtime table never allocates in the control path. All
table operations are library or example API calls; the console only calls them. No startup NMT, enable or motion.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its axis registration and capacity seams; RS is
authoritative for common names. Inspect FieldCore-node read-only for its runtime instance configuration, disabled
bindings and device cadence. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt
N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax
vendor evidence, safety, uncertainty or verification rules. Read the capacity study, the S2 memory report, the prompt
53, 70 and 73 reports, `Capacity.h`, `ProbeApp`, `AxisSession`, `BenchRoute`, `BenchConsole`, the bench `main.cpp` and
the bench README. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. The node table API (add, remove, rebind, enable, role) with idle checks, invalidation and capacity refusal; console
   commands that call it.
2. Passive monitoring per entry with period and stale threshold; monitored-drive health in the console.
3. The bench firmware on the table: node set, per-node records, non-blocking arming, dumps and releases; the Python
   runners follow (route protocol bump, tests).
4. The shared rate constant and override, the rate measurement and the bench default; bounded console publications.
5. The `memory` command.

## Verify and exit

Host tests: add, remove and rebind refused while busy or uncertain and accepted at idle; invalidation on removal; the
capacity refusal message; a disabled entry silent; monitor-only staleness from heartbeat and TPDO times; two nodes'
bench records interleaved without blocking; console replies never delayed by a dump. Run the native, firmware, stack
and documentation gates and CI on the actual final commit. Bench: add a third (absent) node ID and remove it; monitor
one drive while the other moves; the UART rate matrix at 921600, 2000000 and 3000000 baud with loss counters and a
long dump; a short regression on both drives.

## Documentation and delegation

Update the multi-axis and owner guides, the bench README and `hardware_bench.md`, the firmware examples guide, the CLI
contract, coverage, backlog, CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the idle and
invalidation rules; normal coding agents own disjoint tests and the runner changes. The lead integrates and runs
combined checks. Record `docs/reports/prompt_75.md`. Commit and sync on the current branch before handoff.
