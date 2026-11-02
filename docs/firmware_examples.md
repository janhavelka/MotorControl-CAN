# Shared Arduino and ESP-IDF firmware

The requested target set is ESP32-S2, S3 and P4. Current evidence differs by layer:

| Target | Portable core/component | Complete example/adapter | Physical qualification |
| --- | --- | --- | --- |
| S3 | Compiled and linked | Arduino/IDF builds and host tests pass | Pending |
| S2 | IDF compile/link passed | UART/core-0/TWAI port and bounded bench image build and run | 20 raw uploads per node on nodes 1/2 observed; broader qualification pending |
| P4 | Isolated IDF compile/link passed | Target lifecycle/USB routing, silicon selection and build registration pending | Pending |

The [board intake](reports/2026-11-02_s2_bench_intake.md) records actual S2/P4
compiler evidence and precise remaining work. Do not flash the S3 example onto
the connected S2 or treat core compilation as a functioning hardware adapter.

Both firmware systems compile the same [source inventory](../examples/common/firmware_sources.json),
`ProbeApp`, `DeviceEvidence`, `CanBusOwner`, TWAI node driver and platform. S3
uses USB Serial/JTAG; S2 uses exclusive UART0 on GPIO43 TX/GPIO44 RX at 115200
8N1, with the SDK console disabled. The installed core remains SDK-free C++11.
The shared entry points admit no startup work. The separate S2 bench image adds
fixed evidence controls and its own `RawEvidence` collector; production console
parity belongs to prompt 42. [Native examples](native_application.md)
remain independent of either SDK. SocketCAN is deliberately omitted: the current
owner and fake fixtures already exercise the transport boundary without another
adapter or Linux dependency.

## Configuration and startup

The supported build pin is pioarduino 55.03.311 / Arduino 3.3.11 / ESP-IDF 5.5.5,
with the reviewed ESP32-S2/S3 TWAI node API. See [SDK evidence](adapters/twai_sdk.md)
and the [additional original USB/time/task source hashes](adapters/esp_platform_sources.json).
The USB implementation copies complete one-byte writes or returns zero; the
zero-tick read returns immediately. Its ISR silently drops a full RX ring. These
are source-backed SDK facts, not physical loss/timing measurements.
The [S2 UART proof](adapters/esp32s2_uart.md) records its zero-tick one-byte read
and FIFO-only `uart_tx_chars` write, with no TX ring or FIFO-space wait. Its SDK
TX mutex is uncontended only under exclusive same-task UART ownership. Neither
console provides complete overflow accounting or lossless host delivery.
The N8/no-PSRAM/8 MB board is a **compile fixture**, not an installed bench fact.
The default fixture keeps CAN disabled even if someone flashes it. No flashing,
monitoring or physical test is part of these compile checks.

Deployable configuration requires established TX/RX GPIOs, bitrate and copied RX
capacity. S3 GPIO19/20 belong to USB Serial/JTAG; the SDK validates remaining pin
capabilities. An actual board, external CAN transceiver, electrical wiring and
termination must be supplied. No RS UART/DE pins, COM port, node or bitrate default
applies. Define no drive binding or session evidence merely from controller setup.

Both entry points call `startProbeFirmware(firmwareBoardConfig())`. One static
12288-byte internal stack and task record run on core 1 for S3 and core 0 for S2.
SDK-facing context and queues stay internal. Construction has no I/O. Console setup precedes explicit local
controller setup; no drive selection, NMT, parameter, enable or movement request
is made. `firmwareBootStatus()` exposes cached startup failure as well as running
or configuration-required status; RUNNING means local progression only.

Each turn obtains checked signed-64-bit `esp_timer_get_time()` microseconds, runs
`ProbeApp::service` (and its sole receive owner), then TWAI promotion, then
`DeviceEvidence::service`, then yields
one scheduler tick. The platform callback's `idle` is null. Unread CAN ingress
defers TX. Time zero/equality and crossing the 32-bit microsecond boundary are
valid. Negative/backward/failed time latches a fatal local fault: owner evidence
is invalidated using its last accepted time and controller retirement is attempted
once. Nothing automatically restarts, retries or replays. Failed retirement keeps
storage alive and effects uncertain. Actual service gaps and stack high-water
marks require the physical fixture. The [S2 bench example](../examples/s2_bench/README.md)
adds only fixed, explicitly requested evidence controls. Its separate
[physical report](reports/2026-11-02_s2_live_connection.md) records the original
stack-budget failure, corrected bounds and observed headroom. The later raw
collector waits for the preceding accepted TX's final report before granting
another field, within the original deadline. It does not retry a refused grant.

Explicit C++ application work uses the existing `FirmwareProbe::app()` public
read functions and `owner()` retained-result/recovery APIs. Establish actual
binding, profile, current epochs and peer/service premises first. Q20 read
effects are unestablished (the DEC-1 accepted list is owner risk acceptance
only); these examples do not manufacture a fresh session
or issue an automatic discovery read. A successful SDO/host recovery still cannot
qualify a drive model, motion completion or an ambiguous reused route.

`FirmwareProbe::evidence()` exposes the same bounded collector with fixed DEVICE
provenance. Startup and service do not admit acquisition. An explicit exact plan
is admitted as an ordinary device plan only when every object is on the DEC-1
accepted list, else `READ_EFFECT_UNQUALIFIED`; the separately reviewed experiment path
permits only finite unqualified typed reads and retains their possible effects.
Once admitted, firmware service collects terminals automatically; callers must
not also advance the app or drain owner diagnostics. A clock fault interrupts
the acquisition at the owner's last accepted time, preserving failed terminals
and uncertainty. [Assembly tests](../test/firmware_probe_test.cpp) exercise this
using synthetic fixtures. The [native evidence procedure](../examples/device_evidence/README.md)
documents the offline demonstration; [bench requirements](hardware_bench.md)
own actual fixture and physical admission facts.

## Bounded I/O and resources

| Resource | Current bound / evidence |
| --- | --- |
| Owner task | One static pinned task; 12288-byte internal stack, priority 1; core 1 S3 / core 0 S2 |
| Shared application I/O | 64 input bytes / 256 output bytes; 8 reads / 16 writes per turn |
| USB SDK rings | RX 256 / TX 1024 bytes, plus SDK internal allocations |
| USB callbacks | One zero-tick single-byte read/write attempt; no flush or host wait |
| S2 UART | RX 256 bytes / TX FIFO only; one-byte callbacks, exclusive UART0; no SDK console or event queue |
| CAN ingress/TX | Application-selected RX depth 1..16; two total copied TX slots, one active SDK frame |
| Owner turn | RX 8, TX 2, reports 8, starts 2, recovery poll 1 maximum |
| Diagnostics | Existing eight copied owner records; no second authoritative receive path |
| Acquisition history | One retained record: up to 15 full terminals / 64 traffic copies, initial/final owner counters/resources and observed service gaps; loss interrupts acquisition |
| Separate S2 raw history | 20 terminals / 96 traffic copies / 16 post-terminal tail copies; fixed inventory, same owner, never concurrent with typed acquisition |

Full output preserves queued suffixes and increments SDK-byte refusal accounting.
Full input drops newest consumed bytes with `ProbeIoStats::inputDropped`; it
does not parse or execute them. SDK USB RX ring overflow is not publicly counted,
so this counter is not total physical USB loss. The platform retains original
install/uninstall errors. SDK lifecycle allocation/internal locks and its CAN RX
ISR have no established elapsed-time bound. Capacity counts are not timing or
physical qualification guarantees.

The [two-node S2 capture](reports/2026-11-02_s2_live_connection.md) completed
20 raw uploads per node, with 40 RX/40 TX, zero reported CAN errors/drops and
9852 bytes minimum remaining stack. Those are short-run observations of this
image and fixture. Both `1009` and `100A` indicated one byte rather than EDS
U16; the raw collector retains `0x0C` and `0x20` without inferring version,
operation mode or standstill. The typed identity read defaults to the EDS U16
widths and fails there; `IdentityWidths::MEASURED_U8` is an explicit option. No PDO write was performed.

## Build and inspect

Use the [Arduino instructions](../examples/probe_cli/README.md) or
[native-IDF instructions](../examples/probe_idf/README.md). The separate
[IDF component consumer](../test/idf_consumer/README.md) copies only the portable
core into an isolated directory; it is a different gate from standalone firmware.

```text
python scripts/check_firmware.py --target arduino --pio "path to pio" --build-dir ".build/Arduino firmware"
python scripts/check_firmware.py --target idf --idf-path "path to esp-idf" --idf-python "path to IDF Python" --build-dir ".build/IDF firmware"
python scripts/check_firmware.py --target s2 --idf-path "path to esp-idf" --idf-python "path to IDF Python" --temporary-root "existing short temporary path" --build-dir ".build/S2 firmware"
python scripts/check_firmware.py --target component --idf-path "path to esp-idf" --idf-python "path to IDF Python" --build-dir ".build/IDF consumer"
```

Run in the exported pinned IDF environment for SDK builds. `--target all` performs
all four gates. The checker records commands, versions, artifacts and actual
compilation inputs. ELF/BIN outputs and matching shared input files are required;
missing tools and failed builds return nonzero. SDK compilation uses short
external temporary roots to avoid the Windows compiler's dependency-file path
limit; `--temporary-root` selects an existing short parent path explicitly.
Images, maps, SDK logs and compiler databases are retained in the requested
run directory before temporary sources/build caches are released. Failed SDK
builds also retain resolved configuration, SDK logs and available CMake configure
diagnostics without requiring an ELF. An evidence-copy failure cannot replace
the original build failure; if the build otherwise succeeded, the copy failure
fails the gate. Dependencies are provisioned
explicitly, and no build command opens a serial/CAN interface. Retained log paths
and actual measured build sizes belong in the stage report. SDK-free native
quick/full gates remain in [the verification table](verification.md).
