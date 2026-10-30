# Offline native settings read

This C++11 example calls `prepareDriverRead`, `nextDriver`, `advanceDriver` and
checked `getDriver` for `MOTION_PARAMETERS`: `6081` U32 position speed in rpm,
and `6083/6084` U32 ramp times in milliseconds. Ramp times do not establish a
physical acceleration rate. The installed portable core is its only dependency.

Values, route, generations, timestamps and successful `SdoCompletion` records
are synthetic fixture premises. No controller, CANopen stack, SDK, clock or
hardware is accessed. A production application obtains retained completions
from its sole qualified service owner; copied local tokens cannot qualify an
indistinguishable late wire response.

The example reports speed 1234 rpm, ramp up 250 ms and ramp down 2001 ms.
The last value deliberately exceeds the documented mode-table write range and
remains a valid raw observation. No value is normalized or turned into a write
candidate. The acquisition interval is `[0, 23]` microseconds with 23 microseconds
of skew; this is not an atomic device snapshot. `interpreted` stays false.

Upload effects remain unqualified: the synthetic successful terminals retain
`effectPossible=true`, `effectUncertain=false`. They do not prove harmless
uploads, model applicability, activation, persistence, coordinate scale or
compatible drive state. The example emits no setting writes or motion commands.

## Build from the source tree

```text
cmake -S . -B .build/driver-settings-preview -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON
cmake --build .build/driver-settings-preview --target driver_settings_preview
```

On Linux run
`.build/driver-settings-preview/examples/driver_settings_preview/driver_settings_preview`.
In PowerShell run
`& '.build/driver-settings-preview/examples/driver_settings_preview/driver_settings_preview.exe'`.

## Build against an installed package

After installing to an absolute prefix:

```text
cmake -S examples/driver_settings_preview -B .build/installed-driver-settings-preview -G Ninja -DCMAKE_PREFIX_PATH=/absolute/install/prefix
cmake --build .build/installed-driver-settings-preview
.build/installed-driver-settings-preview/driver_settings_preview
```

Quote the complete prefix argument when a Windows path contains spaces. Run
`& '.build/installed-driver-settings-preview/driver_settings_preview.exe'` in
PowerShell. No Python, sibling checkout or optional stack is needed.
