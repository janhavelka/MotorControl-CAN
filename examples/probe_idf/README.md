# Native ESP-IDF shared application

This standalone firmware builds the same `ProbeApp`, owner, console platform and
TWAI adapter for ESP32-S3 (USB Serial/JTAG console) and ESP32-S2 (exclusive UART0
console on GPIO43/44). It has no CLI parser, automatic target selection or startup
request, and startup never starts the controller. See the
[shared contract](../../docs/firmware_examples.md) for target policy, the
transceiver silent input, capacities, failures and physical prerequisites. The
special [S2 bench image](../s2_bench/README.md) is a different program.

Use ESP-IDF **5.5.5** at `b774170ff46c393eeb5e495ea37936038d3f4f4f` and its
exported Python/tool environment. Select the target with `-DIDF_TARGET`; the
shared `sdkconfig.defaults` has no target, and `sdkconfig.defaults.<target>` adds
the flash layout (S3: 8 MB; S2: 4 MB DIO). The default build is a compile fixture:
`CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE=y` disables CAN, including if the image is
flashed. The expected diagnostic is:

```text
MotorControl-CAN: compile fixture; CAN disabled, configuration required.
```

From PowerShell or a Linux shell after exporting the pinned SDK:

```text
idf.py -C examples/probe_idf -B ".build/probe IDF" -DSDKCONFIG="absolute path to .build/probe IDF/sdkconfig" -DIDF_TARGET=esp32s3 build
idf.py -C examples/probe_idf -B ".build/probe IDF S2" -DSDKCONFIG="absolute path to .build/probe IDF S2/sdkconfig" -DIDF_TARGET=esp32s2 build
python scripts/check_firmware.py --target idf --idf-path "path to esp-idf" --idf-python "path to IDF Python"
python scripts/check_firmware.py --target idf-s2 --idf-path "path to esp-idf" --idf-python "path to IDF Python" --temporary-root "short path"
```

The checker verifies original SDK hashes, real linked ELF/BIN outputs, the target
in `project_description.json`, the cross compiler of every compiled file, the
resolved `sdkconfig` values (S3 8 MB, S2 4 MB DIO, console none, disabled options
present as `is not set`) and the exact set of checkout files compiled: every shared
source, core file and `main.cpp`, nothing else. It retains logs under the requested build directory. `sdkconfig`
and generated dependencies/build outputs belong in the specified ignored build
directory. No port, monitor, flash or physical motor activity is required or
performed by these commands.

## Board configuration

For an actual board, disable the compile-fixture setting and supply the verified
`CONFIG_MOTORCONTROLCAN_CAN_TX_PIN`, `CAN_RX_PIN`, `CAN_BITRATE` and `CAN_RX_DEPTH`,
and, if the transceiver has an application-controlled silent input,
`CONFIG_MOTORCONTROLCAN_CAN_SILENT_PIN` with its `CAN_SILENT_LEVEL` (0 or 1, from
the datasheet and schematic; -1 with a pin fails compilation). `FirmwareBoard.h`
checks each pin against the pinned SDK capability masks of the selected target and
refuses that target's console pins (S3 GPIO19/20, S2 GPIO43/44) and the pins the chip
drives itself: the flash bus GPIO26..32 on both, and on S3 GPIO43, where the ROM prints
its boot messages. It checks those numbers against the SDK's own pin constants. Missing
pins or bitrate fail compilation. No RS bench wiring or node defaults are supplied. Verify
flash layout, target board, transceiver and bus prerequisites separately. Local
controller setup issues no device command. Explicit public C++ requests require
established profile/binding/session evidence; Q20 is not bypassed.

The OPTAK_FGI_2025 S2 board configuration is
[`sdkconfig.optak_s2`](sdkconfig.optak_s2): TX17, RX16, TJA1051 silent input
GPIO18 at level 1, 1 Mbit/s, RX depth 16. A configured image boots `INACTIVE`:
silence requested, controller closed, until an explicit `FirmwareProbe::start`.

## Shared-image smoke check (test-only)

[`sdkconfig.optak_s2_smoke`](sdkconfig.optak_s2_smoke) adds the test-only
`FirmwareSmoke` hook for nodes 1 and 2. Build that exact image with:

```text
python scripts/check_firmware.py --target idf-s2-optak --idf-path "path to esp-idf" --idf-python "path to IDF Python" --temporary-root "short path" --build-dir ".build/IDF S2 OPTAK firmware"
```

or directly:

```text
idf.py -C examples/probe_idf -B "<build>" -DSDKCONFIG="<build>/sdkconfig" -DIDF_TARGET=esp32s2 "-DSDKCONFIG_DEFAULTS=<repo>/examples/probe_idf/sdkconfig.defaults;<repo>/examples/probe_idf/sdkconfig.optak_s2;<repo>/examples/probe_idf/sdkconfig.optak_s2_smoke" build
```

A manually reused build directory keeps the values of its existing `sdkconfig`:
`SDKCONFIG_DEFAULTS` applies only to options that file does not yet hold, so a
directory once built with the smoke defaults keeps `CONFIG_MOTORCONTROLCAN_SMOKE_HOOK=y`
(and the OPTAK pins) when it is later built without them, and the reverse. Use a new
directory, or delete its `sdkconfig`, whenever the defaults list changes. The checker
always builds in a new temporary directory.

The image is passive until the console receives the single byte `G`. While passive
it prints, once per boot after the startup line, its identity:

```text
SMOKE ready elf=<application ELF SHA-256> idf=v5.5.5 project=motorcontrol_can_probe
```

After `G` it prints the identity again with the start result, starts the controller,
runs one typed identity read on node 1 and node 2, prints the raw fields or the
failure and admits no further work. It does **not** stop the controller: after
`SMOKE done` the controller stays started with the transceiver's normal level
requested until reset, so the node acknowledges every frame on the bus and its error
counters follow bus traffic. The [shared contract](../../docs/firmware_examples.md)
gives the exact lines. Flash layout matches the bench: bootloader `0x1000`,
partition table `0x8000`, application `0x10000`, DIO 80 MHz 4 MB.

Flash this exact image with `python scripts/s2_flash.py --flash --app
probe-idf-optak-smoke`, which builds it with the `idf-s2-optak` defaults and refuses
any other image before writing, and run the check with
`python scripts/s2_smoke.py --artifact <upload record directory>`: it requires the
passive startup line, sends one `G` and captures to the final line (exit 0 passed,
1 a check failed after `G`, 2 nothing was sent). The runner must accept the
`SMOKE ready` line between the startup line and its quiet interval, and can then
compare that ELF hash with the upload record before it sends anything; a runner that
does not know the line stops with exit 2 and sends nothing. The
[firmware guide](../../docs/firmware_examples.md) gives the full commands. A
compile/link of this image is not a smoke-test result.

This standalone build includes the application and optional stack. The isolated
[portable component consumer](../../test/idf_consumer/README.md) is built and
checked separately, without those resources. A successful compile/link is not
physical qualification, measured stack usage, service latency or console delivery.
