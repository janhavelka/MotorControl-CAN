# Arduino shared application build

This ESP32-S3 example builds the same application, CAN owner, TWAI adapter and
USB platform as the [native ESP-IDF example](../probe_idf/README.md).
It has no command parser yet; command/schema parity belongs to prompt 42.
The thin Arduino entry creates the shared static owner task and idles.

**ESP32-S2 under Arduino is blocked** with the pinned package: its prebuilt
`esp32s2` SDK libraries enable the UART0 console, which conflicts with the shared
platform's exclusive UART0 ownership. `pio run -e probe_cli_s2_compile` reproduces
the single guard error; it is not a passing build and no S2 parity is claimed.
The [shared contract](../../docs/firmware_examples.md) records the evidence and
what would resolve it. Build the shared S2 image with ESP-IDF instead.

Use PlatformIO Core **6.1.19**, pioarduino **55.03.311**, Arduino **3.3.11** and
its ESP-IDF **5.5.5** libraries. The extra script rejects different resolved
framework versions and verifies the retained CANopenNode source hashes.
The board `esp32-s3-devkitc-1` is the N8 8 MB QD, no-PSRAM **compile fixture**;
it does not identify an available CAN bench.

From the repository root:

```text
pio run -e probe_cli_compile
pio run -e probe_cli_compile -t compiledb
```

Build artifacts and the compilation database go under
`.build/platformio/probe_cli_compile/`. Set `PLATFORMIO_BUILD_DIR` to choose a
different ignored output directory. The maintained
[source inventory](../common/firmware_sources.json) supplies every shared source
and include directory; the portable core is consumed through `lib_deps`.
An ordinary native/offline build needs neither PlatformIO nor an ESP SDK.

The local compile check used the existing isolated installation without installing
another Core or changing global settings:

```powershell
$env:PLATFORMIO_CORE_DIR = 'C:/p55s'
$env:PLATFORMIO_BUILD_DIR = '.build/p28-pio'
& C:/p55s/penv/Scripts/pio.exe run -e probe_cli_compile
& C:/p55s/penv/Scripts/pio.exe run -e probe_cli_compile -t compiledb
```

Both commands passed on 2026-11-01 with Xtensa GCC 14.2.0
`esp-14.2.0_20260121`. The linked compile fixture used 50,696 bytes of static RAM
and 342,861 bytes of application flash; SDK runtime allocations and task stack
high-water marks were not measured. Its compilation database includes all 18
shared inventory sources and the portable core. Logs were retained under
`.build/p28-source/`; firmware and database under `.build/p28-pio/`.
Building `probe_cli` without controller definitions failed as required.
These are compilation results; no firmware was flashed or run.

`probe_cli_compile` sets `MOTORCONTROLCAN_COMPILE_FIXTURE=1`. CAN stays disabled,
including if this fixture binary is flashed. USB initialization is local and
does not issue CAN/NMT, enable, movement, parameter or recovery requests.
Expected local startup status is `CONFIGURATION_REQUIRED`; diagnostics are
bounded best-effort USB bytes, without a host-delivery guarantee.
A configured build boots `INACTIVE`: startup never starts the controller.

For an actual application use `probe_cli` with explicit build definitions
`MOTORCONTROLCAN_CAN_TX_PIN`, `MOTORCONTROLCAN_CAN_RX_PIN`,
`MOTORCONTROLCAN_CAN_BITRATE` and `MOTORCONTROLCAN_CAN_RX_DEPTH` (1..16), alongside
`MOTORCONTROLCAN_COMPILE_FIXTURE=0`. A transceiver silent input adds
`MOTORCONTROLCAN_CAN_SILENT_PIN` and its `MOTORCONTROLCAN_CAN_SILENT_LEVEL` (0 or 1).
Supply them in that environment's `build_flags`, retaining `${env.build_flags}`.
No working pin/bitrate defaults exist. Pins are checked against the SDK capability
masks of the target. GPIO19/20 belong to the S3 USB Serial/JTAG console; automatic
Arduino CDC startup is disabled. GPIO43 (the ROM's boot-message output, U0TXD) and
GPIO26..32 (the flash bus) are refused as well, at compile time. A valid local
configuration boots `INACTIVE`: startup checks the board and requests a configured
transceiver silent level, and the controller opens only on an explicit
`FirmwareProbe::start`. Nothing selects or commands a drive.

Before physical use establish the actual board/flash layout, external CAN
transceiver and its enable/standby requirements, TX/RX wiring, bus termination,
bitrate, drive/motor/firmware, node identity and applicable authorization. RS bench
pins, ports and authorization do not establish these facts. After that review,
PlatformIO upload/monitor commands require explicitly selected ports; this stage
only compiles and does not flash or open a port.

See the [shared platform and capacity guide](../../docs/firmware_examples.md)
for the static owner stack (`FIRMWARE_STACK_BYTES` in `examples/common/EspProbeMain.h`), queues, service budgets, clock semantics,
USB backpressure and separate host versus physical evidence.
