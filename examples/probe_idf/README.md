# Native ESP-IDF shared application

This standalone firmware builds the same `ProbeApp`, owner, USB platform and
TWAI adapter as [Arduino](../probe_cli/README.md). It has no CLI parser, automatic
target selection or startup request. See the [shared contract](../../docs/firmware_examples.md)
for capacities, failures and physical prerequisites.

Use ESP-IDF **5.5.5** at `b774170ff46c393eeb5e495ea37936038d3f4f4f` and its
exported Python/tool environment. The configured ESP32-S3/8 MB/no-PSRAM target
is compile-only. Its default `CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE=y` disables
CAN, including if the image is flashed. The expected diagnostic is:

```text
MotorControl-CAN: compile fixture; CAN disabled, configuration required.
```

From PowerShell or a Linux shell after exporting the pinned SDK:

```text
idf.py -C examples/probe_idf -B ".build/probe IDF" -DSDKCONFIG="absolute path to .build/probe IDF/sdkconfig" -DIDF_TARGET=esp32s3 build
python scripts/check_firmware.py --target idf --idf-path "path to esp-idf" --idf-python "path to IDF Python"
```

The checker verifies original SDK hashes, real linked ELF/BIN outputs and actual
compiler inputs for every shared source and core file. It retains logs under
`.build/firmware/`. `sdkconfig` and generated dependencies/build outputs belong
in the specified ignored build directory. No port, monitor, flash or physical
motor activity is required or performed by these commands.

For an actual application, disable the compile-fixture setting through
`menuconfig` and supply the verified `CONFIG_MOTORCONTROLCAN_CAN_TX_PIN`,
`CAN_RX_PIN`, `CAN_BITRATE` and `CAN_RX_DEPTH`. Missing pins/bitrate fail compilation.
GPIO19/20 stay reserved for USB; no RS bench wiring or node defaults are supplied.
Verify flash layout, target board, transceiver and bus prerequisites separately.
Local controller setup issues no device command. Explicit public C++ requests
require established profile/binding/session evidence; Q20 is not bypassed.

This standalone build includes the application and optional stack. The isolated
[portable component consumer](../../test/idf_consumer/README.md) is built and
checked separately, without those resources. A successful compile/link is not
physical qualification, measured stack usage, service latency or USB delivery.
