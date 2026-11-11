# Shared Arduino and ESP-IDF firmware

Default Arduino/IDF entries now service the same [local console](cli_contract.md)
after the application/owner turn, including while CAN remains inactive.
`ProbeConsole` and `ProbeAppLocal` are in the checked shared build inventory.
Explicit smoke hooks bypass console input because they own their test protocol;
the separate S2 bench parser is not a production route. Device commands and full
platform parity remain prompts 41/42. No local command implicitly starts CAN.

Ordinary firmware downloads require the explicit pending-work dispatcher described
in [commissioning](commissioning.md#explicit-communication-commissioning). A null
or unrelated preflight refuses before enqueue. Raw acquisition, fixed PDO trials
and motion share that one owner; no helper constructor installs a hidden guard.

The requested target set is ESP32-S2, S3 and P4. Current evidence differs by layer:

| Target | Portable core/component | Shared application image | Special bench image | Physical qualification |
| --- | --- | --- | --- | --- |
| S3 | Compiled and linked | Arduino and IDF compile fixtures link ELF/BIN; host tests pass | None | Pending |
| S2 | IDF compile/link passed | IDF compile fixture and OPTAK board image link ELF/BIN. **Arduino S2 is blocked** by the pinned package (below) | UART/core-0/TWAI bench image builds and runs | Bench image: 20 raw uploads per node on nodes 1/2. Shared image: smoke test pending |
| P4 | Isolated IDF compile/link passed | Target lifecycle/USB routing, silicon selection and build registration pending | None | Pending |

The [board intake](reports/2026-11-02_s2_bench_intake.md) records actual S2/P4
compiler evidence and precise remaining work. Do not flash the S3 example onto
the connected S2 or treat core compilation as a functioning hardware adapter.
The S2 shared image and the S2 bench image are different programs; a result of
one is not evidence for the other. Arduino/IDF parity is **not** claimed for S2.

Every firmware build compiles the same [source inventory](../examples/common/firmware_sources.json),
`ProbeApp`, `DeviceEvidence`, `CanBusOwner`, TWAI node driver and platform. S3
uses USB Serial/JTAG; S2 uses exclusive UART0 on GPIO43 TX/GPIO44 RX at 115200
8N1, with the SDK console disabled. The installed core remains SDK-free C++11.
The shared entry points admit no startup work. The separate S2 bench image adds
fixed evidence controls and its own `RawEvidence` collector; production console
parity belongs to prompt 42. [Native examples](native_application.md)
remain independent of either SDK. SocketCAN is deliberately omitted: the current
owner and fake fixtures already exercise the transport boundary without another
adapter or Linux dependency.

## Configuration and target policy

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

`FirmwareConfig::target` selects the pin policy through two functions.
`firmwareConsolePin`: S3 reserves GPIO19/20 (USB Serial/JTAG), S2 reserves
GPIO43/44 (UART0). Neither target reserves the other's console pins, so GPIO19/20
are ordinary pins on S2. `firmwareSystemPin` covers pins the chip drives itself
before and outside this firmware: GPIO26..32 on both targets, the MSPI flash/PSRAM
bus (`soc/spi_pins.h`) that the ROM, bootloader and cache use from reset, and on S3
GPIO43 (U0TXD), where the ROM prints its boot messages unless eFuse
`UART_PRINT_CONTROL` disables them (`esp_efuse_table.csv`; the default enables them).
On such a pin the chip's own traffic would reach the CAN TX line or the silent input,
or contend with the transceiver's RXD output, before any check runs, so every role is
refused. Strapping pins (GPIO0, GPIO45, GPIO46, and GPIO3 on S3) are not refused:
whether a transceiver there changes the boot depends on the board's reset levels.
`FirmwareTarget::UNSPECIFIED` applies no reservation; the shared entries always set
the compiled target, and the S2 bench application, which fixes TX17/RX16 itself,
still leaves it unspecified.
`checkFirmwareConfig` reports the first refused board fact: `NOT_CONFIGURED`,
`MISSING_PIN`, `CONSOLE_PIN`, `SYSTEM_PIN`, `PIN_CONFLICT` (TX equals RX, or the
silent pin equals either) or `SILENT_INCOMPLETE` (pin without level, level without
pin, or no GPIO callback). A given silent pin faces the console, system and conflict
checks even without its level. A refused board drives no GPIO and starts no
controller. `FirmwareProbe::lastCheck` keeps the reason; `INVALID_PLATFORM` marks
platform callbacks without a clock or with an idle hook. Pin capability is the SDK's
fact: `FirmwareBoard.h` checks the build's pins at compile time against the pinned
`soc_caps.h` masks for the current target (`SOC_GPIO_VALID_GPIO_MASK`,
`SOC_GPIO_VALID_OUTPUT_GPIO_MASK`), refuses console and system pins, and checks the
two functions' literals against the SDK constants (`U0TXD_GPIO_NUM`,
`U0RXD_GPIO_NUM`, `USB_INT_PHY0_DM_GPIO_NUM`/`DP`, `MSPI_IOMUX_PIN_NUM_*`) in every
real build. `IdfTwaiDriver` and the GPIO drive repeat `GPIO_IS_VALID_*` at run time.
`checkFirmwareConfig` itself does not check capability, so a caller that passes its
own pins at run time sees an SDK refusal of an absent or input-only GPIO later, as
`TRANSCEIVER_FAILED` or `CONTROLLER_FAILED` rather than `CONFIGURATION_REJECTED`; the
shared entries cannot, because their pins are checked at compile time.
`FirmwareBoard.h` refuses any target other than S2/S3: the P4 driver guard is
unchanged and no P4 board policy exists.

The default build of each example is a **compile fixture**: CAN stays disabled
even if someone flashes it. Deployable configuration requires established TX/RX
GPIOs, bitrate and copied RX capacity, and, if the board has one, the transceiver
silent input with its level. An actual board, external CAN transceiver, electrical
wiring and termination must be supplied. No RS UART/DE pins, COM port, node or
bitrate default applies. Define no drive binding or session evidence merely from
controller setup.

The OPTAK_FGI_2025 S2 board configuration lives only in the IDF example's
application files, [`sdkconfig.optak_s2`](../examples/probe_idf/sdkconfig.optak_s2):
TX GPIO17, RX GPIO16, silent GPIO18 with level 1, 1 Mbit/s, RX depth 16. These are
the user-reported board facts recorded in [the bench record](hardware_bench.md);
the transceiver's actual silent behaviour on this board is physically unverified.

## Transceiver silent input

The silent input is application-owned. `FirmwareProbe` receives a `SilentPin`
callback (the ESP-IDF one is `espSilentPin()` in `EspTransceiverPin.cpp`); the
core, `TwaiAdapter` and `IdfTwaiDriver` never drive it. Polarity is explicit board
data: `SilentLevel::HIGH` for a TJA1051 S input (high = transmitter off, receiver
kept; [NXP TJA1051 datasheet](https://www.nxp.com/docs/en/data-sheet/TJA1051.pdf)),
`LOW` for the inverse. The ESP drive bounds the pin, checks the SDK output
capability, latches the level with `gpio_set_level` and only then enables the
output with `gpio_set_direction`, so it never drives the reset level first.

| Step | Silent input | Controller |
| --- | --- | --- |
| `prepare` (startup, before the console) | Silent level requested | Not started |
| Refused board | Untouched | Never started; owner invalidated |
| Silent level refused | `FAILED`: level unknown | Never started; owner invalidated |
| `start` (explicit) | Normal level requested after the controller started | Started |
| Controller start refused | Silent level kept | Owner invalidated, shut down once at once |
| Normal level refused | Silence requested again, then | Owner invalidated, shut down once |
| Progression failure | Silence requested before shutdown | Owner invalidated, shut down once |
| `stop` (explicit), also before `start` | Silence requested before shutdown | Owner invalidated unless already, shutdown only if unconfirmed |

Every path that leaves the controller closed for the rest of the boot goes through
one retirement: the owner is invalidated once, at its own last time, and from then
on refuses new work and settles the requests it holds, instead of admitting a read
that no service turn will ever run. A read that `ProbeApp` already holds when its
controller start is refused was never handed to the controller; the application
ends it with `ProbeApp::cancelRead`. A confirmed shutdown is never repeated; a
refused one is repeated only by an explicit `stop`. `FirmwareProbe::retired()`
reports whether any controller resources may remain.

`TransceiverState` reports the last **requested** level (`NOT_CONTROLLED`,
`SILENT_REQUESTED`, `NORMAL_REQUESTED`, `FAILED`). It means the SDK accepted the
GPIO calls. After `FAILED` the controller is not running, but it is retired only
when its shutdown also succeeded (`retired()`); a refused shutdown leaves both
inhibitors unknown. It never proves a silent bus, the transceiver's mode or the pin's
electrical level; that needs a measurement on the board. Without a silent input,
the closed controller is the only inhibitor. The board's own default matters
before firmware runs: in the inspected OPTAK design R3 pulls S low (normal mode),
so the transceiver is not silent during reset and boot, only after `prepare`.
Before `start`, no firmware routes the controller to the CAN TX pin.

## Startup

Both entry points call `startProbeFirmware(firmwareBoardConfig())`. One static
12288-byte internal stack and task record run on core 1 for S3 and core 0 for S2;
`EspProbeMain.cpp` checks that rule in each target build. SDK-facing context and
queues stay internal. Construction has no I/O.

The owner task runs `FirmwareProbe::prepare`, then the console, then the service
loop. **Startup never starts the controller**, on any target: a configured image
stays `INACTIVE` (silence requested where configured) until an explicit
`FirmwareProbe::start`. Until the console arrives in prompts 40-42 the only
caller of `start` is an optional `FirmwareTurnHook`, which runs on the owner task
after each service turn; no production entry passes one. Nothing issues drive
selection, NMT, parameter, enable or movement requests at startup. A hook that
started the controller and then stops admitting work does not thereby stop it: the
smoke hook leaves the controller started with the normal level requested after its
final line, so the node acknowledges bus frames until reset.

`firmwareBootStatus()` reports `PLATFORM_FAILED`, `CONFIGURATION_REQUIRED`
(compile fixture), `CONFIGURATION_REJECTED`, `TRANSCEIVER_FAILED`, `INACTIVE`, then
after explicit calls `RUNNING`, `CONTROLLER_FAILED` (start refused, shutdown
confirmed), `CONTROLLER_UNRETIRED` (start refused and its shutdown refused too: an
SDK node may remain, and only the adapter and any requested silence inhibit TX),
`STOPPED` (stop confirmed) or `STOP_FAILED` (silence, invalidation or shutdown
unconfirmed), and `PROGRESSION_FAILED`. The owner task recomputes it after every
turn with the pure `firmwareStatus` projection, in which explicit outcomes win over
startup facts and a refused silent level wins over a failed console, because its
pin level is unknown. `PROGRESSION_FAILED` stays the status after a later stop;
`faultRetired()` keeps whether the failure path itself confirmed retirement, while
`retired()` and `stopConfirmed()` show what the stop achieved. `RUNNING` means
local progression only.

Each turn obtains checked signed-64-bit `esp_timer_get_time()` microseconds, runs
`ProbeApp::service` (and its sole receive owner), then TWAI promotion, then
`DeviceEvidence::service`, then the optional hook, then yields one scheduler
tick. The platform callback's `idle` is null. Unread CAN ingress
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

## Shared-image smoke check (test-only)

`FirmwareSmoke` is a finite check of the shared image, compiled only when
`CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y` (the `idf-s2-optak` image). It is not a
console: one byte `G` arms one fixed sequence once per boot, and every other byte
is counted and ignored. Before that byte the image is passive: once per boot, after
the startup line and before it examines any input, it prints `SMOKE ready` with the
image identity (application ELF SHA-256 from `esp_app_get_description`, IDF
version, project), or `SMOKE ready identity=unconfigured` without one. The sequence
prints that identity again, calls `FirmwareProbe::start`, then for each configured node
configures the predefined-connection-set route (`0x600`/`0x580`/`0x700`/`0x080` +
node) as an explicit fixture premise and runs the existing typed identity read
(`ProbeApp::requestIdentity`, measured one-byte `1009`/`100A`). Budgets are 250 ms
queue, 250 ms response and a 5 s overall deadline per node, copied from the bench
identity plan; they are caller policy, not vendor timing. Lines (`idf=` is
`esp_app_desc_t::idf_ver`, which the bench image's pings report as `5.5.5` on this SDK):

```text
MotorControl-CAN: board accepted; CAN inactive until explicit start.
SMOKE ready elf=<sha256> idf=5.5.5 project=motorcontrol_can_probe
SMOKE image elf=<sha256> idf=5.5.5 project=motorcontrol_can_probe start=ok running=1 transceiver=normal-requested check=0
SMOKE node=1 identity=ok device=... hw=..../1 sw=..../1 entries=... vendor=... product=... revision=... model=... firmware=... skew=...
SMOKE node=2 identity=failed state=3 outcome=... uncertain=...
SMOKE done; no further requests; ignored=<n>
```

It never writes an object, retries, resets or reads without the trigger, and a
second `G` does nothing. A failed or uncertain read is printed and ends that node.
Host tests drive the real `FirmwareSmoke`, `FirmwareProbe`, owner and SDO stack
against controller, GPIO and console fakes.

Flash and check this image only with the reviewed tools, from PowerShell or cmd,
on the port where the recorded ESP-Prog-2 serial enumerates:

```text
python scripts/s2_flash.py --dry-run --app probe-idf-optak-smoke --port COM15 --serial 94A990398B40
python scripts/s2_flash.py --flash --app probe-idf-optak-smoke --port COM15 --serial 94A990398B40
python scripts/s2_smoke.py --port COM15 --serial 94A990398B40 --artifact docs/bench/<UTC>-s2-smoke-upload --output NEW_SMOKE_DIRECTORY
```

[`s2_flash.py`](../scripts/s2_flash.py) `--app probe-idf-optak-smoke` builds
`examples/probe_idf` in `.build/s2-smoke-live` with the `idf-s2-optak` gate's own
`-DSDKCONFIG_DEFAULTS` argument, then `reconfigure build`. Before any write it
refuses outputs that are not that image: another project name or path, other
applied defaults (`project_description.json` `config_defaults`), another target or
chip ID, a second ELF in the build directory, an image without its ELF hash, any
layout other than bootloader `0x1000`, partition table `0x8000` and
`motorcontrol_can_probe.bin` at `0x10000` with dio/80m/4MB in both `flash_args` and
`flasher_args.json`, `sdkconfig` drift from the four defaults files or a missing
board or smoke value of the gate, compiler inputs without `FirmwareSmoke.cpp` or
with any `examples/s2_bench` source, another cross compiler, and a project version
(`git describe` at configure time) that does not name `HEAD`. A directory that
another project configured is refused before building. The USB identity, target
MAC, single write, no erase, no eFuse and no-retry rules of the bench upload apply
unchanged. The record, by default `docs/bench/<UTC>-s2-smoke-upload/build.json`,
names the selection, its ELF SHA-256 and every check. Without `--app` the tool
builds and flashes `examples/s2_bench` exactly as before, and `--check-ping`
accepts only bench records.

[`s2_smoke.py`](../scripts/s2_smoke.py) first reads that record: a flashed smoke
record made through the same USB serial, with the ELF SHA-256, project, SDK
version and a retained `sdkconfig` matching its hash and naming the two nodes. It
then requires the recorded ESP-Prog-2 uniquely on the port and opens it with DTR
and RTS deasserted, so opening drives neither EN nor BOOT. It requires the passive
line `MotorControl-CAN: board accepted; CAN inactive until explicit start.`, then
within 2 s, as the very next line, the `SMOKE ready` line whose `elf=`, `idf=` and
`project=` equal the record, then 0.5 s with no further byte. Only then does it send
one `G`. It captures until `SMOKE done` or another terminal line within `--seconds`
(default 30) and writes `rx.bin`, `rx.txt` and `request.json` to the new directory.
It passes only when the image line after the start names the recorded image again,
the start reports `start=ok running=1 transceiver=normal-requested`, both nodes
report `identity=ok` and the image counted no other byte. Exit status 2 means
nothing was sent, 1 that `G` was sent and a check failed or the capture ended early.
Nothing is sent when the passive line is missed or another first line appears (the
bench image's `S2_BENCH ready`, a fixture or failure banner, or a `SMOKE` line,
including a ready line before the passive line). The same holds for a missing,
malformed or `identity=unconfigured` ready line, a ready line naming another ELF,
IDF version or project, and any byte during the quiet interval. The image prints
both lines once per boot. A ping after an upload captured no boot banner
(2026-11-06), so lines printed after the upload's reset do not reach a port opened
later; such a run records `passive_state_missed`. `--reset-en` resets the board once
after the port is open: RTS asserted pulls EN low for 0.1 s while DTR stays deasserted,
so IO0 stays high and the chip boots its application, the hard reset esptool performs
after an upload. Without it the runner never moves DTR or RTS, and the board must be
reset by hand (EN button or power) while it waits. Offline tests (`s2_flash_test.py`, `s2_smoke_test.py`)
use fakes only; they are not a smoke result.

## Arduino S2: blocked by the pinned package

`pio run -e probe_cli_s2_compile` (board `esp32-s2-saola-1`, DIO, USB mode 0)
fails with exactly one error, the S2 platform guard
`S2 UART0 platform requires the SDK console disabled for exclusive ownership`.
The guard is correct for this package and stays:

- The pinned `framework-arduinoespressif32-libs` 5.5.5+sha.b774170ff46 `esp32s2`
  libraries are prebuilt with `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`,
  `CONFIG_ESP_CONSOLE_UART=y`, `CONFIG_ESP_CONSOLE_UART_NUM=0` and
  `# CONFIG_ESP_CONSOLE_NONE is not set` (`esp32s2/sdkconfig` lines 1455-1462;
  the compiled `dio_qspi/include/sdkconfig.h` lines 620-624), with
  `CONFIG_LOG_DEFAULT_LEVEL_ERROR=y` and `CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL_ERROR=y`.
- ESP-IDF 5.5.5 `esp_vfs_console/vfs_console.c` registers that UART as stdout at
  startup (`init_vfs_console`). `esp_driver_uart/src/uart_vfs.c` `uart_tx_char`
  writes UART0's FIFO directly with an unbounded busy wait for space, outside the
  UART driver and its TX mutex. Any precompiled SDK error log therefore writes
  into the same FIFO as the platform's protocol bytes, and can wait without bound.
  Exclusive ownership and the bounded-call argument both fail.
- Arduino `cores/esp32/main.cpp` `loopTask` calls `Serial0.setPins` before
  `setup`; with no driver installed this only stores pins
  (`esp32-hal-uart.c` `uartSetPins`). Arduino itself is not the conflict; the
  prebuilt SDK console is.
- The package's only ways to change that configuration are not usable here.
  `custom_sdkconfig` rebuilds the shared global `framework-arduinoespressif32-libs`
  package in place, and deletes and reinstalls both framework packages from the
  network when the stored configuration differs (platform
  `builder/frameworks/arduino.py`, `check_reinstall_frwrk`, `safe_framework_cleanup`
  and `call_compile_libs`). `framework = arduino, espidf` (Arduino as an IDF
  component) resolves more than fifteen managed components, most with range
  versions, through the component manager (`framework-arduinoespressif32/idf_component.yml`).
  Both change the pinned dependency set; neither was run.

Undefining SDK macros, removing the guard or silencing the console's output would
hide this conflict rather than resolve it. The shared S2 image is therefore built
with ESP-IDF only. `python scripts/check_firmware.py --target arduino-s2` reproduces
the blocker and reports status `blocked`; it is not part of `--target all`.
Arduino S2 needs either a pinned Arduino S2 library build with
`CONFIG_ESP_CONSOLE_NONE`, or a reviewed, pinned Arduino-as-component
configuration with the same UART0 pins.

## Bounded I/O and resources

| Resource | Current bound / evidence |
| --- | --- |
| Owner task | One static pinned task; 12288-byte internal stack, priority 1; core 1 S3 / core 0 S2 |
| Shared application I/O | 64 input bytes / 512 output bytes; 8 reads / 16 writes per turn |
| USB SDK rings | RX 256 / TX 1024 bytes, plus SDK internal allocations |
| USB callbacks | One zero-tick single-byte read/write attempt; no flush or host wait |
| S2 UART | RX 256 bytes / TX FIFO only; one-byte callbacks, exclusive UART0; no SDK console or event queue |
| Silent input | One application GPIO, two SDK calls per request; requested level only |
| CAN ingress/TX | Application-selected RX depth 1..16; two total copied TX slots, one active SDK frame |
| Owner turn | RX 8, TX 2, reports 8, starts 2, recovery poll 1 maximum |
| Diagnostics | Existing eight copied owner records; no second authoritative receive path |
| Acquisition history | One retained record: up to 15 full terminals / 64 traffic copies, initial/final owner counters/resources and observed service gaps; loss interrupts acquisition |
| Smoke check | One 240-byte pending line; one read in flight; nine identity fields per node |
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
9852 bytes minimum remaining stack. Those are short-run observations of the bench
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
python scripts/check_firmware.py --target idf-s2 --idf-path "path to esp-idf" --idf-python "path to IDF Python" --temporary-root "existing short temporary path" --build-dir ".build/IDF S2 firmware"
python scripts/check_firmware.py --target idf-s2-optak --idf-path "path to esp-idf" --idf-python "path to IDF Python" --temporary-root "existing short temporary path" --build-dir ".build/IDF S2 OPTAK firmware"
python scripts/check_firmware.py --target s2 --idf-path "path to esp-idf" --idf-python "path to IDF Python" --temporary-root "existing short temporary path" --build-dir ".build/S2 firmware"
python scripts/check_firmware.py --target component --idf-path "path to esp-idf" --idf-python "path to IDF Python" --build-dir ".build/IDF consumer"
python scripts/check_firmware.py --target arduino-s2 --pio "path to pio" --build-dir ".build/Arduino S2 blocker"
```

| Selector | Project and configuration | Checked beyond ELF/BIN and shared inputs |
| --- | --- | --- |
| `arduino` | PlatformIO `probe_cli_compile`, S3 fixture | `xtensa-esp32s3-elf-` compiler, Arduino core compiled, exact checkout inputs with `examples/probe_cli/main.cpp`, no bench or smoke source |
| `arduino-s2` | PlatformIO `probe_cli_s2_compile` | Reports `blocked` on the console guard; never a pass today |
| `idf` | `examples/probe_idf`, S3 fixture | Target, compiler, resolved sdkconfig (8 MB flash; `CONFIG_SPIRAM` absent because no PSRAM component is built), exact checkout inputs, no bench or smoke source |
| `idf-s2` | `examples/probe_idf`, S2 fixture (4 MB DIO) | Same, for esp32s2, with `CONFIG_SPIRAM` `is not set` |
| `idf-s2-optak` | `examples/probe_idf` + `sdkconfig.optak_s2` + `sdkconfig.optak_s2_smoke` | Same, every board and smoke value, `FirmwareSmoke.cpp` compiled, no bench source |
| `s2` | `examples/s2_bench` (special bench image) | Target, compiler, resolved sdkconfig, bench entry and raw collector, exact checkout inputs outside the bench project |
| `component` | Isolated portable core consumer | Target, compiler, exceptions/RTTI off, exactly the staged core and consumer, no checkout file |

Run in the exported pinned IDF environment for SDK builds. `--target all` performs
every gate except `arduino-s2`. The checker records commands, versions, artifacts,
the shared inventory and actual compilation inputs. ELF/BIN outputs and the exact
expected checkout input files are required: a missing one, and any other checkout
file compiled (a second parser, a renamed bench copy), fail the gate; SDK, toolchain
and build-directory files are not checkout inputs. A required disabled option must
appear as `is not set`; only a reviewed absence passes as absent. Missing tools and
failed builds return nonzero.
SDK compilation uses short external temporary roots to avoid the Windows
compiler's dependency-file path limit; `--temporary-root` selects an existing
short parent path explicitly.
Images, maps, SDK logs and compiler databases are retained in the requested
run directory before temporary sources/build caches are released. Failed SDK
builds also retain resolved configuration, SDK logs and available CMake configure
diagnostics without requiring an ELF. An evidence-copy failure cannot replace
the original build failure; if the build otherwise succeeded, the copy failure
fails the gate. Dependencies are provisioned
explicitly, and no build command opens a serial/CAN interface. Retained log paths
and actual measured build sizes belong in the stage report. SDK-free native
quick/full gates remain in [the verification table](verification.md).
