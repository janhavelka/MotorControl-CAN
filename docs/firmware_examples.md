# Shared Arduino and ESP-IDF firmware

Both firmware systems compile the same [source inventory](../examples/common/firmware_sources.json),
`ProbeApp`, `CanBusOwner`, TWAI node driver and USB platform. The installed core
remains SDK-free C++11. These examples have no command parser or motion operations;
console parity belongs to prompt 42. [Native examples](native_application.md)
remain independent of either SDK. SocketCAN is deliberately omitted: the current
owner and fake fixtures already exercise the transport boundary without another
adapter or Linux dependency.

## Configuration and startup

The supported build pin is pioarduino 55.03.311 / Arduino 3.3.11 / ESP-IDF 5.5.5,
with the reviewed ESP32-S3 TWAI node API. See [SDK evidence](adapters/twai_sdk.md)
and the [additional original USB/time/task source hashes](adapters/esp_platform_sources.json).
The USB implementation copies complete one-byte writes or returns zero; the
zero-tick read returns immediately. Its ISR silently drops a full RX ring. These
are source-backed SDK facts, not physical loss/timing measurements.
The N8/no-PSRAM/8 MB board is a **compile fixture**, not an installed bench fact.
The default fixture keeps CAN disabled even if someone flashes it. No flashing,
monitoring or physical test is part of these compile checks.

Deployable configuration requires established TX/RX GPIOs, bitrate and copied RX
capacity. GPIO19/20 belong to USB Serial/JTAG; the SDK validates remaining pin
capabilities. An actual board, external CAN transceiver, electrical wiring and
termination must be supplied. No RS UART/DE pins, COM port, node or bitrate default
applies. Define no drive binding or session evidence merely from controller setup.

Both entry points call `startProbeFirmware(firmwareBoardConfig())`. One static
8192-byte internal stack and task record run on core 1. SDK-facing context and
queues stay internal. Construction has no I/O. USB setup precedes explicit local
controller setup; no drive selection, NMT, parameter, enable or movement request
is made. `firmwareBootStatus()` exposes cached startup failure as well as running
or configuration-required status; RUNNING means local progression only.

Each turn obtains checked signed-64-bit `esp_timer_get_time()` microseconds, runs
`ProbeApp::service` (and its sole receive owner), then TWAI promotion, then yields
one scheduler tick. The platform callback's `idle` is null. Unread CAN ingress
defers TX. Time zero/equality and crossing the 32-bit microsecond boundary are
valid. Negative/backward/failed time latches a fatal local fault: owner evidence
is invalidated using its last accepted time and controller retirement is attempted
once. Nothing automatically restarts, retries or replays. Failed retirement keeps
storage alive and effects uncertain. Actual service gaps and stack high-water
marks require the physical fixture.

Explicit C++ application work uses the existing `FirmwareProbe::app()` public
read functions and `owner()` retained-result/recovery APIs. Establish actual
binding, profile, current epochs and peer/service premises first. The Q20 probe
read-effect gate is unresolved; these examples do not manufacture a fresh session
or issue an automatic discovery read. A successful SDO/host recovery still cannot
qualify a drive model, motion completion or an ambiguous reused route.

## Bounded I/O and resources

| Resource | Current bound / evidence |
| --- | --- |
| Owner task | One static pinned task; 8192-byte internal stack, priority 1, core 1 |
| Shared application I/O | 64 input bytes / 256 output bytes; 8 reads / 16 writes per turn |
| USB SDK rings | RX 256 / TX 1024 bytes, plus SDK internal allocations |
| USB callbacks | One zero-tick single-byte read/write attempt; no flush or host wait |
| CAN ingress/TX | Application-selected RX depth 1..16; two total copied TX slots, one active SDK frame |
| Owner turn | RX 8, TX 2, reports 8, starts 2, recovery poll 1 maximum |
| Diagnostics | Existing eight copied owner records; no second authoritative receive path |

Full output preserves queued suffixes and increments SDK-byte refusal accounting.
Full input drops newest consumed bytes with `ProbeIoStats::inputDropped`; it
does not parse or execute them. SDK USB RX ring overflow is not publicly counted,
so this counter is not total physical USB loss. The platform retains original
install/uninstall errors. SDK lifecycle allocation/internal locks and its CAN RX
ISR have no established elapsed-time bound. Capacity counts are not timing or
physical qualification guarantees.

## Build and inspect

Use the [Arduino instructions](../examples/probe_cli/README.md) or
[native-IDF instructions](../examples/probe_idf/README.md). The separate
[IDF component consumer](../test/idf_consumer/README.md) copies only the portable
core into an isolated directory; it is a different gate from standalone firmware.

```text
python scripts/check_firmware.py --target arduino --pio "path to pio" --build-dir ".build/Arduino firmware"
python scripts/check_firmware.py --target idf --idf-path "path to esp-idf" --idf-python "path to IDF Python" --build-dir ".build/IDF firmware"
python scripts/check_firmware.py --target component --idf-path "path to esp-idf" --idf-python "path to IDF Python" --build-dir ".build/IDF consumer"
```

Run in the exported pinned IDF environment for SDK builds. `--target all` performs
all three gates. The checker records commands, versions, artifacts and actual
compilation inputs. ELF/BIN outputs and matching shared input files are required;
missing tools and failed builds return nonzero. SDK compilation uses short
external temporary roots to avoid the Windows compiler's dependency-file path
limit. Images, maps, SDK logs and compiler databases are retained in the requested
run directory before temporary sources/build caches are released. Failed SDK
builds also retain resolved configuration, SDK logs and available CMake configure
diagnostics without requiring an ELF. An evidence-copy failure cannot replace
the original build failure; if the build otherwise succeeded, the copy failure
fails the gate. Dependencies are provisioned
explicitly, and no build command opens a serial/CAN interface. Retained log paths
and actual measured build sizes belong in the stage report. SDK-free native
quick/full gates remain in [the verification table](verification.md).
