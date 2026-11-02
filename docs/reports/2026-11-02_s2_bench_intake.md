# ESP-Prog2 and ESP32-S2 board intake, 2026-11-02

## Scope and actual checks

The user authorized board testing and supplied a new fixture: PC, ESP-Prog2,
OPTAK_FGI_2025 PCB, two daisy-chained CL86 drives and free-shaft motors bolted
to the table. Both probe cables are attached. Probe and PCB power selectors are
reported at 5 V. ESP32-S2, S3 and P4 are now explicit requested platform coverage.

This session enumerated Windows devices, checked installed tool versions and
inspected the open EasyEDA project through the
[easyeda-api skill](C:/Users/Honza/.agents/skills/easyeda-api/SKILL.md).
No design was modified or saved. No serial port, USB JTAG target session, CAN
interface or programmer was opened; no firmware, driver, DIP or eFuse was changed.
Unstable target power and ambiguous UART naming remain concrete blockers to the
next target test, not a lack of user authorization.

Branch: `promptset/motorcontrol-can`, upstream of the same name on origin, entry
`accaa3e07e788f7eb368a783a98baace425d3ec8`; fresh fetch 0 ahead / 0 behind. Three
unrelated untracked `.vscode` files were preserved. This supplements prompt 29's
conditional acquisition route; prompts 30-33 are still Pending in this checkout.

## USB identity and port collision

Windows reports USB parent `303A:1002`, serial `94A990398B40`, bus description
`ESP-Prog-2`. Its interfaces have problem code zero:

| Interface | Windows observation | Meaning |
| --- | --- | --- |
| MI_00 CDC | usbser, driver 10.0.26100.9278, COM15 | Bridge UART, not a proven target connection |
| MI_02 JTAG | WinUSB, driver 10.0.26100.9549 | USB JTAG access; no COM port is needed for JTAG |
| MI_03 MSC | USBSTOR | Bridge mass-storage interface; no files written |

A distinct present device `303A:1001`, serial ending `6B:B0`, also advertises
COM15. Registry DOS mapping is `COM15 -> \\Device\\USBSER001`; that alone does not
prove which physical bridge a COM15 open reaches. Resolve the duplicate by
disconnecting the unrelated board temporarily or assigning the identified
ESP-Prog2 an unused port, then re-enumerate. No driver replacement is indicated
by the current healthy JTAG interface. Original FTDI ESP-Prog settings are not
the correct bridge identity for ESP-Prog2.

Raw selected-device inventory is retained locally under
`.build/board-check-20261102/usb_inventory.json`; unrelated Bluetooth identities
were excluded. Enumeration does not measure rail voltage or verify target pins.

## Actual design inspection

EasyEDA window `9c18efbc-770a-47a9-a5e6-0cfd6ee35374` matched the user's named
project `OPTAK_FGI_2025`, UUID `fef938316eb942568e9ccd3313a2ad6f`, board
`FirstPrototype`. Read-only exports of schematic `mainboard_sch` and PCB
`mainboard_pcb` each contain 158 components. The inspected MCU, regulator, CAN
transceiver, connectors, jumpers, choke and termination pin nets match between
the two exports. Pad-net agreement is not copper continuity or physical assembly
verification. The original MCU sheet was restored after viewing the PCB.

Local retained exports are `schematic_netlist.txt` and `pcb_netlist.txt` under
`.build/board-check-20261102/`. They are design evidence, not disposable test cache.
No full copy of the user's schematic was added to the public library package.
Their SHA-256 hashes are respectively
`f0ce7d44ebe0209dead6a71188f6c5de641ab9a24dff1da82cdff0fbda7afaf0`
and `6f66998403a960dbe96e5d9aa963296f675d32fc3188895ffe0543ecd1b81778`.

| Design connection | Confirmed net mapping |
| --- | --- |
| ESP_UART2 | 1 EN; 2 supply via UART_VDD_JMP2 to 5V; 3 target TXD0 / S2 GPIO43; 4 GND; 5 target RXD0 / GPIO44; 6 IO0 |
| ESP-JTAG2 | 1 supply via JTAG_VDD_JMP2 to 5V; 2 TMS / GPIO42; 4 TCK / GPIO39; 6 TDO / GPIO40; 8 TDI / GPIO41; 3/5/7/9 GND; 10 unused |
| U14 AMS1117-3.3 | Pin3 input 5V, pins2/4 output 3V, pin1 GND. U13 ESP32-S2-MINI-2-N4 pin3 takes 3V, not 5V |
| U3 TJA1051TK/3/1J | Pin3 VCC=5V; pin5 VIO=3V; TXD GPIO17; RXD GPIO16; S GPIO18; GND and exposed pad grounded |
| R3 | 10 kohm CAN_SILENT to GND: reset/default transceiver mode is normal, not silent |
| CN2 | Pin1 CAN_L_CON; pin2 CAN_H_CON; no ground pin |
| R1/R2/JP1 | Two 60.4 ohm resistors and a three-pad open solder jumper form optional split termination; actual solder bridges are unknown |

UART signal names on the official probe connector are target-relative: pin3
connects to target TX and pin5 to target RX. Do not swap these merely because
generic UART adapter conventions differ. Actual Tag-Connect cables and contact
orientation still need continuity checks. The selected 5 V supply is consistent
with this design, provided the actual assembly matches it.

Both programmer supply paths join the board's 5V rail, as do buck U1 output and
USB switch U10 output. Confirm all attached power sources and their intended
isolation before combining them. The design contains green 3VLED and
5VLED_BUCK1 power indicators with 5.1 kohm and 10 kohm series resistors, but no
LED or net called VPROG. A dim light alone is not a rail measurement.

The [official probe schematic](https://dl.espressif.com/schematics/SCH_ESP32-PROG-2_20250403.pdf)
uses a direct red 3.3 V power LED and GPIO-controlled activity LEDs. The
[reference guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/other/esp-prog-2/reference/reference-doc.html)
describes green UART and blue JTAG activity. Installed bridge firmware is unknown;
current upstream firmware also shares the blue indication with UART TX. Identify
the actual LED and measure connector VDD, U14 input, 3V_SENSE and EN before
attributing the flicker to load, power cycling, wiring or serial activity.

The [NXP datasheet](https://www.nxp.com/docs/en/data-sheet/TJA1051.pdf), pp2/4-6,
confirms U3 requires 4.5-5.5 V VCC and separate logic VIO; S high suppresses TX
while retaining RX. A 3.3 V MCU supply does not make its VCC a 3.3 V rail.

## CAN wiring and reported DIP settings

Retained EN86 original PDF pages 9 and 10 were reopened, extracted and rendered.
SW2 ON only selects node2; SW1 ON only selects node1. SW6/SW7 both OFF select
1 Mbit/s. Both reported SW8 OFF settings disable the drives' internal 120 ohm
terminators. These are source-backed interpretations of user-reported switches,
not observed on-wire identities or a firmware qualification.

For this board-to-drive cable, connect CN2 pin1 L to RJ45 pin2, and CN2 pin2 H
to RJ45 pin1. Drive RJ45 pin3 is CAN reference GND; the two-pin CN2 needs an
appropriate separate reference arrangement. Do not infer it from cable shielding
or USB ground. A pin1-to-pin1 cable reverses H/L.

For a PCB-to-drive-A-to-drive-B line with no external terminators, use a verified
termination at the PCB end and SW8 ON at the far drive only; the middle drive
stays OFF. Confirm JP1's actual bridges before assuming PCB termination exists.
With all nodes unpowered, two roughly 120 ohm endpoints give approximately
60 ohms between H and L. This is a wiring check, not a substitute for scope/CAN
error observations. Cable lengths, reference wiring and actual drive labels
remain to be recorded.

## Software and debugging readiness

Installed `esptool.py` reports 4.12.0. Installed OpenOCD reports
`v0.11.0-esp32-20220706`; only version commands were executed. Its
`board/esp32s2-bridge.cfg` selects `interface/esp_usb_bridge.cfg`, chip ID2 and
`target/esp32s2.cfg`; the bridge interface uses 303A:1002. This establishes an
available configuration, not successful JTAG access. An actual debug session
must use the identified bridge and avoid resetting/running unknown firmware as
an unintended motor action. Do not burn eFuses to diagnose a connection.

The current production example cannot simply be flashed to S2:

- `IdfTwaiDriver.cpp` has an S3-only retirement proof/guard.
- `EspProbePlatform.cpp` uses S3 USB Serial/JTAG; S2 offers USB OTG or the
  external probe's UART path instead.
- `EspProbeMain` requires two cores and pins its owner to core1; S2 has one core.
- Firmware configuration has no GPIO18 silent control or explicit listen-only
  mode. Current normal CAN mode may acknowledge frames even without requests.
- Current compile fixtures assume S3 and 8 MB flash; the design lists an N4 S2
  module. Read the actual assembled flash identity before selecting its image.

One shared owner/service engine remains the intended solution for S2/S3/P4.
Pinned SDK5.5.5 declares one Classical TWAI controller on S2/S3 and three on P4;
P4 does not imply CAN FD. Extend target-specific LL/clock/interrupt retirement
evidence, bounded UART/USB platform callbacks, core selection, transceiver
control and compile fixtures before advertising those firmware targets. S2
Xtensa and P4 RISC-V toolchains are installed. P4 physical silicon revision and
USB routing need their own board facts; the inspected old OpenOCD lacks a P4
target script. No second protocol engine or framework dependency in the core
is needed.

### Isolated portable-core build checks

Actual manual compile/link experiments used the existing isolated consumer and
the normal portable-source allowlist at `accaa3e`, with explicit `IDF_TARGET`.
They did not remove target guards or compile the S3 firmware as another chip.

| Target | Verified compiler/link target | Application BIN | Result |
| --- | --- | --- | --- |
| S2 | `xtensa-esp32s2-elf-*`, Xtensa ELF | 154,960 bytes | Passed |
| P4 | `riscv32-esp-elf-*`, RISC-V ELF | 198,096 bytes | Passed |

Both used IDF5.5.5 and GCC14.2.0 `esp-20260121`. Compilation databases contain all
nine portable core sources plus the isolated consumer. Real ELF/BIN artifacts
linked; no runtime assertion or firmware entry was executed on a board. Sizes
describe the whole SDK fixture, not isolated library cost. P4's size utility
warned that `sram_seg` could not be assigned to a memory type, so its RAM
classification is not treated as a complete memory measurement.

The local script `.build/board-check-20261102/compile_portable.py` and retained
run directories contain exact commands, source staging, compiler databases,
sdkconfig, ELF/BIN/map and logs:

- S2: `s2-core/20261102T103837592012Z-32096-8uj3whrx`.
- P4: `p4-core/20261102T103837592012Z-31044-1i5s25hb`.

Initial missing-constraint-path failures were retained, then corrected by using
the documented installed `IDF_TOOLS_PATH`; no dependency check was disabled.
All 30 existing SDK hash entries match installed source. Their S3 scope still
does not establish the missing S2/P4 TWAI lifecycle proof. No tracked build
configuration or automatic CI gate was changed by these manual experiments.

## Source refresh and remaining work

Local `python scripts/check_promptset.py`, `git diff --check` and strict
`doxygen Doxyfile` passed after the documentation updates. The two new component
builds above passed independently. No runtime or hardware suite was claimed or
rerun for these documentation-only changes; existing automatic CI triggers remain.

Read-only sibling checks found MotorControl-RS
`2d646f775566ab7e5d31b3abc10c0fc46fa08625` clean, ADS1115
`235af3e8c722661a89a44766a4ccd903a654f6fe` clean, INA228
`b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` clean, and FieldCore-node
`5ad9292dc8481cf08be9a61f662495a6474d1da0` clean. Current owner/result/bench,
S2 example and consuming-read boundaries were inspected; no sibling changed or
suite rerun. The RS delta adds consumer/header coverage, not transferable bench
authorization. Separate agents reviewed official probe data, actual board netlist
and target SDK capabilities. The lead alone owns eventual physical operations.

Next: establish rail stability and cable continuity, resolve COM15, then perform
bounded target identification. Compile reviewed S2 startup with CAN disabled,
check reset/console behavior, then add finite passive CAN capture with explicit
listen-only/silent behavior and known bitrate. A silent bus does not prove bad
wiring because drive heartbeat may be disabled. Subsequent active typed-read
acquisition retains Q20/conditional review; motion, stop and completion semantics
remain their separate gates. No uncertain action will be replayed.
