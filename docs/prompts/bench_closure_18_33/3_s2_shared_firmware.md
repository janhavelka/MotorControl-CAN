# Packet 3 Complete shared S2 firmware build and startup

Apply [COMMON](COMMON.md). Prerequisite [1](1_evidence_contracts.md). Originals
27-28. Host implementation/build is ready now; the current S2 supports a finite
smoke test. Full shared CLI commands belong to 40-42, not this packet.

## Inspect

Read originals27/28 and `docs/firmware_examples.md`, `docs/adapters/esp32s2_uart.md`,
`examples/common/FirmwareBoard.h`, `EspProbeMain.*`, `EspProbePlatform.*`,
`FirmwareProbe.*`, `firmware_sources.json`, `examples/probe_cli/main.cpp`,
`examples/probe_idf/main/`, `platformio.ini` and `scripts/check_firmware.py`.
Refresh RS platform/runner seams, ADS1115 owner-loop and INA228 validity seams;
FieldCore only for changed integration responsibilities.

The shared loop already chooses S2 core0/UART0 and S3 USB. The missing parts
include S3-specific board validation, S2 Arduino configuration and a standalone
shared S2 IDF build. Existing `--target s2` builds the special bench application.

## Implement

1. Introduce explicit target-specific board validation/configuration using the
   current board type. `FirmwareProbe::begin` also has unconditional GPIO19/20
   rejection and S3-specific documentation: update both checks or centralize
   the actual target policy. Preserve S3 USB pins; do not unconditionally reserve
   S3 GPIO19/20 on every chip. Check actual SDK pin capabilities. Keep OPTAK
   RX16/TX17/silent18 in its application configuration, not the portable core.
2. Make transceiver silent control application-owned with explicit polarity and
   startup/failure behavior. Inactive/unconfigured startup transmits nothing.
   GPIO/configuration/driver-start failure must leave transmission inhibited.
   Distinguish a requested pin level from a physically verified silent bus.
3. Add S2 Arduino compile-only and explicit board environments, and a shared S2
   standalone IDF build. Reuse ProbeApp/FirmwareProbe/owner/platform sources;
   do not copy the special bench main or introduce a second parser/service loop.
   First resolve a concrete build conflict: the installed pinned Arduino S2
   SDK enables its UART0 console, while EspProbePlatform requires
   CONFIG_ESP_CONSOLE_NONE. Inspect packaged sdkconfig and Arduino startup/UART
   ownership. Implement source-backed exclusive ownership, or a reviewed
   Arduino-as-IDF-component configuration with the same pins. Never undefine
   SDK macros, remove the guard or mask console traffic just to pass compilation.
   If the required pinned package cannot express the contract, record that exact
   Arduino build blocker while completing standalone S2 IDF; do not claim parity.
   Do not remove the P4 driver guard or claim P4 support from a core-only build.
4. Extend the real build checker, source inventory, registered build-contract
   tests and CI matrix to verify selected MCU/framework, actual compiler inputs,
   ELF/BIN and source parity. Preserve pins IDF5.5.5, PlatformIO6.1.19 and
   pioarduino55.03.311 unless a separately justified issue requires change.
5. Test missing/unsupported/conflicting pins, S2 UART/core0, S3 USB unchanged,
   silent-output failure, explicit start/stop and no unsolicited CAN writes.
   Use existing `firmware_build_test.py`, `esp_s2_probe_platform_test.cpp`,
   `esp_probe_platform_test.cpp`, `firmware_probe_test.cpp` and TWAI fakes.

## Verify and finish

Run affected registered host gates and actual S2 Arduino/IDF plus existing S3
builds. Update checker help and documentation with the exact new selectors;
do not present a mocked compile as a linked image. Check the isolated core
consumer still has no board/SDK dependencies.

Use existing flash tooling only if it selects and verifies this exact image;
otherwise extend its artifact selection with offline tests before uploading.
Do not accidentally rebuild/flash the old special bench target while claiming
shared-image qualification. Verify image identity, passive startup, explicit
start and one finite existing typed-read baseline on both nodes. Do not require
the not-yet-implemented full CLI to exercise shared startup. The current shared
bootstrap owns its FirmwareProbe internally: use an existing adequate seam or
add a small test-only explicit admission hook on the owner task for this finite
check. Do not add another command parser, auto-read on startup, or call hardware
directly from another task. The tested image must actually use the shared entry.

Delegate build-matrix verification separately from the platform implementation.
A stronger reviewer checks startup transmission and pin/error paths. Record
software builds versus S2 observations, keep P4 and wire-retirement gates open,
write packet_3, update27/28 and commit/sync with exact-commit CI.
