# Offline I/O configuration read

This C++11 example calls the installed `prepareDriverRead`, `nextDriver`,
`advanceDriver` and checked `getIo` for `IO_INPUT_CONFIGURATION`. The portable
core is its only dependency. It reads the exact fixed `2400:00..08` group:
raw count, polarity and seven documentary input functions.

Route, epochs, timestamps and successful `SdoCompletion` records are synthetic
fixture premises. No controller, stack, SDK, clock or hardware is accessed.
Production callers obtain retained terminals from the sole service owner;
a copied local token cannot distinguish an ambiguous late wire response.

DI0 has code 0 (undefined), DI1 code 1 (origin) and DI2 unlisted code 99.
The raw code remains 99 while its documentary interpretation is `UNKNOWN`.
Polarity `0x8001` retains its unexplained high bit and decodes only the documented
DI0..6 inversion bits. Raw count 65535 does not add any read addresses.
The acquisition interval is `[0, 83]` microseconds, with 83 microseconds of skew;
these separate reads are not an atomic device snapshot.

No function assignment establishes wiring, electrical level or homing/stop
prerequisites. `wiringKnown`, `electricalLevelsKnown` and `outputMappingKnown`
remain false. Upload effects remain unqualified even with synthetic successful
terminals. The example prepares an explicit DI2 function candidate, inspects its one changed
download, then withdraws it before admission. Its model/reservation are clearly
synthetic premises. No controller handoff or output/motion action occurs; no
configuration epoch changes. The real fake-controller transaction and readback
are exercised by `test/settings_write_test.cpp`.

## Build from the source tree

```text
cmake -S . -B .build/io-settings-preview -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON
cmake --build .build/io-settings-preview --target io_settings_preview
```

Run `.build/io-settings-preview/examples/io_settings_preview/io_settings_preview`
on Linux or `& '.build/io-settings-preview/examples/io_settings_preview/io_settings_preview.exe'`
in PowerShell.

## Build against an installed package

```text
cmake -S examples/io_settings_preview -B .build/installed-io-settings-preview -G Ninja -DCMAKE_PREFIX_PATH=/absolute/install/prefix
cmake --build .build/installed-io-settings-preview
.build/installed-io-settings-preview/io_settings_preview
```

Quote the complete prefix argument when a Windows path contains spaces. Use
`& '.build/installed-io-settings-preview/io_settings_preview.exe'` in PowerShell.
No Python, sibling checkout or optional stack is needed.
