# Packet 3: shared S2 firmware

[Packet 3](../../prompts/bench_closure_18_33/3_s2_shared_firmware.md), 2026-11-06. Software builds and host tests are recorded here separately from S2 observations. The S2 smoke check of the shared image passed on 2026-11-07, below. Back to the [execution index](README.md).

## Start state and sibling audit

Branch `promptset/motorcontrol-can`. Siblings unchanged since the authoring rows (MotorControl-RS `cd27b12`, FieldCore-node `5ad9292d`, both clean); this packet changes no motion contract and reads no sibling seam. The S3 USB console pins and the S2 bench application's behavior are preserved.

## What changed

| Item | Delivered |
| --- | --- |
| 1 Target-specific board policy | `FirmwareConfig::target` (`FirmwareTarget` UNSPECIFIED, ESP32S2, ESP32S3) selects the pins refused for CAN TX, RX and silent. Console pins: S3 GPIO19/20 (USB), S2 GPIO43/44 (UART0); S3's 19/20 are not reserved on the S2. System pins on both targets: the MSPI flash bus GPIO26..32 and S3 GPIO43, which the ROM drives with boot messages by default (pinned `esp_efuse_table.csv` `UART_PRINT_CONTROL`). `FirmwareBoard.h` static_asserts these numbers against the pinned SDK constants (`U0TXD_GPIO_NUM`, `U0RXD_GPIO_NUM`, `MSPI_IOMUX_PIN_NUM_*`, `USB_INT_PHY0_DM/DP_GPIO_NUM`), and the compile-time capability checks use the real SDK masks. OPTAK's RX16, TX17 and silent GPIO18 at level 1 live only in `examples/probe_idf/sdkconfig.optak_s2` and the bench application |
| 2 Application-owned silent control | `TransceiverConfig`, `SilentLevel` and a `SilentPin` callback (ESP-IDF: `espSilentPin()`, which latches the level before enabling the output). `prepare` checks the board, then requests the silent level; `start` enables the controller, then requests the normal level; `stop` requests silence, then shuts the controller down. A configured shared image now boots `INACTIVE`, never starting the controller. Every path that leaves the controller closed (refused prepare, refused start or normal level, any stop, progression failure) goes through one retirement: the owner is invalidated once and refuses new work, a confirmed shutdown is never repeated. `CONTROLLER_UNRETIRED` reports a refused start whose shutdown was also refused. `TransceiverState` reports the requested level, never a measured bus |
| 3 S2 builds | `check_firmware.py --target idf-s2` (shared S2 compile fixture) and `idf-s2-optak` (the OPTAK board values plus the test-only smoke hook) build the shared `examples/probe_idf` entry. The Arduino S2 build is **blocked**: the pinned pioarduino `55.03.311` esp32s2 libraries are built with `CONFIG_ESP_CONSOLE_UART=y` on UART0 (package `esp32s2/sdkconfig` lines 1455-1462), and `EspProbePlatform` requires exclusive UART0 (`#error` at `EspProbePlatform.cpp:20`). `--target arduino-s2` reports `blocked` with that error and is not part of `all`; no parity is claimed. The guard stays |
| 4 Build checker | Each gate checks its target three ways (project description, `CONFIG_IDF_TARGET`, compiler prefix), its reviewed resolved `sdkconfig` values (a disabled option must appear as `is not set`), and an exact set of checkout-compiled inputs, so an extra parser or a renamed bench copy fails. `idf` asserts 8 MB flash; `s2` and `component` gained compiler and `sdkconfig` checks. Pins unchanged: IDF 5.5.5, PlatformIO 6.1.19, pioarduino 55.03.311 |
| 5 Tests | `firmware_probe` (pin policy tables and a pin sweep, silent-input order and failures, owner invalidation on every closing path, unretired starts, smoke failures and console pressure), `esp_s2_probe_platform` (`espSilentPin`), `firmware_build_contract`, `s2_flash_contract`, `s2_smoke_contract` |
| Flash and smoke tooling | `s2_flash.py --app probe-idf-optak-smoke` builds the shared OPTAK smoke image and checks project, defaults, target, chip, layout, compiler inputs (no `examples/s2_bench` source) and HEAD before any write; the bench image's commands are unchanged. `scripts/s2_smoke.py` requires the passive boot line, then `SMOKE ready elf=… idf=… project=…` matching the upload record, then 0.5 s of quiet, and only then sends one `G`. Otherwise nothing is sent (exit 2) |
| Test-only admission hook | `FirmwareSmoke` (`CONFIG_MOTORCONTROLCAN_SMOKE_HOOK`, only in the OPTAK smoke defaults): an owner-task turn hook through `startProbeFirmware(config, hook)`. One `G` starts the controller and runs one typed identity read on each of nodes 1 and 2, and nothing more. It is not a parser, never auto-reads and has no path back to waiting. After `SMOKE done` the controller stays started at the normal level until reset |

## Reviews

- **Build matrix**, independent of the implementation: every gate built (`all`, then each selector) and the assertions were shown to fail on synthetic wrong-target, wrong-compiler, forbidden-input and wrong-value databases. Its gaps were fixed: exact checkout inputs (B-D2), S3 flash size (B-D3), absent-key checks (B-D4), and `s2` and `component` checks (B-D6).
- **Startup and pin paths**, a stronger reviewer: no path sends CAN traffic before an explicit `start` on any configuration. In the pinned IDF the TX pad is routed to the controller only inside `twai_new_node_onchip`, called only from `start`. Fixed: the owner was not invalidated after a refused start, a refused normal level or a stop before start (R-D1, reproduced: a read was admitted and hung); an unconfirmed start-path shutdown was reported like a clean refusal (R-D2); pins the chip drives before firmware runs were accepted (R-D3); stale docs (R-D4). Each fix's regression fails on its mutant (19 mutants, all killed).

## S2 smoke check, 2026-11-07 21:24Z

Image: the shared `examples/probe_idf` entry with `sdkconfig.optak_s2` and the smoke defaults at `3353ed2`, built and checked by `s2_flash.py --app probe-idf-optak-smoke` ([upload](../../bench/20261107T212300Z-s2-smoke-upload/), ELF `807fd468…`; no `examples/s2_bench` source). The user was absent and asked for the reset through the ESP-Prog-2, so `s2_smoke.py --reset-en` pulsed EN once through RTS after opening the port, with DTR deasserted ([session](../../bench/20261107T212409Z-s2-smoke/)). The ROM reported `rst:0x1 (POWERON)`; a chip-enable reset is reported that way, so the RTS pulse is the recorded cause.

| Line | Observed |
| --- | --- |
| Passive | `MotorControl-CAN: board accepted; CAN inactive until explicit start.` |
| Ready | `SMOKE ready elf=807fd468… idf=5.5.5 project=motorcontrol_can_probe`, equal to the upload record; then 0.5 s of quiet |
| After the one `G` | `SMOKE image ... start=ok running=1 transceiver=normal-requested check=0` |
| Node 1, node 2 | `identity=ok device=00040192 hw=000c/1 sw=0020/1 entries=5 vendor=00001000 product=00000001 revision=00000000 model=0400 firmware=0200`, the identity both specimens reported before |
| End | `SMOKE done; no further requests; ignored=0` |

This shows the shared image's identity, passive startup (no CAN start before the trigger), explicit start through the test-only hook and one typed read on each node. It does not show the GPIO18 or TXD levels before the start (below). The bench image was then reflashed ([upload](../../bench/20261107T212422Z-s2-upload/), ELF `8bce2707…`, ping matched).

## Not measured on hardware

GPIO18's level and the TJA1051 mode while `INACTIVE`; TXD (GPIO17) and the bus across reset and boot before `prepare` (R3 holds S low then, so the transceiver is in normal mode while the TX pad is unrouted); whether TX is suppressed while S is high; ACK and error-counter behavior after the smoke check; the S2 chip's `UART_PRINT_CHANNEL` and `UART_PRINT_CONTROL` eFuses. With `UART_PRINT_CHANNEL=1`, ROM output would go to U1TXD, GPIO17, the OPTAK CAN TX. The refused-start and refused-shutdown SDK paths are fake-only. S3 has no hardware here. The P4 driver guard and the wire-retirement gates stay open.

## Checks

| Check | Result |
| --- | --- |
| Focused CTest (TWAI adapter and driver, `firmware_probe`, both ESP platform fakes, `native_probe`, `probe_app`, `firmware_build_contract`) | Passed; `firmware_probe` 3018 checks |
| `test/firmware_build_test.py`, `s2_flash_test.py`, `s2_smoke_test.py` | 47, 70 and 32 tests passed |
| Firmware gates `all` (arduino, idf, idf-s2, idf-s2-optak, s2, component) and `arduino-s2` | `all` passed; `arduino-s2` blocked by the console guard, exactly as above |
| `check_stack.py --strict --require-toolchain`, with the CI-style S3 and S2 databases | Passed, 0 breaches |
| Strict Doxygen with `FirmwareSmoke.h` and `EspTransceiverPin.h` added | 0 warnings |
| Staged tree alone, in a detached worktree outside `.build` without the other packets' uncommitted work | `verify.py --mode full --compiler gcc`: repository, build, every CTest, package passed; its documentation step needs a `.build/doxygen` directory a fresh worktree lacks, so Doxygen was rerun there: 0 warnings, 28 headers, 2522 members, `check_documentation` passed. Firmware `all` passed (arduino, idf, idf-s2, idf-s2-optak, s2, component); `arduino-s2` blocked as above. CI-style strict stack check with that run's S3 and S2 databases: 0 breaches. `check_promptset.py` passed |
| Remote CI | The IDF job failed on `4df97bc` (run 42249057257) and `f2c90ab`: CI installs ESP-IDF at `.build/esp-idf`, inside the checkout, and the new exact-input check counted the SDK's sources. Locally the SDK lies outside the checkout, so no local gate could show it. Fixed by excluding the build environment's SDK and tool roots by location, from the exactness check (`980982a`) and, after the component gate failed the same way (run 42252710418), from the forbidden-input check too, with regression tests; CI on the fix commit is recorded in the [execution index](README.md) |
| S2 smoke check of the shared OPTAK image | Passed 2026-11-07 on `3353ed2`, below |
