# Offline units preview

This native console example calls the public axis and units functions. It opens
no controller, performs no device lifecycle, and transmits no motion. All scale,
origin, identity and route values are illustrative caller assumptions; the
proprietary endpoint is a common-contract example, not an implemented profile.

The supplied configuration has 4096 command increments per motor turn, 2048
independent encoder counts per motor turn, gearing of 3/2 motor turns per load
turn, 20 mm of travel per load turn, and host origin zero. Full-step geometry is
unknown and unused. A 5 mm load target is exactly 1536 command increments; its
illustrative feedback displacement is 768 encoder counts. The route uses address
500 and subaxis 256 independently of logical axis 9.

The preview also shows the exact native request `1/10` quantized to binary32 with
an explicit error allowance, rejects zero allowance for that inexact value, and
prepares native signed velocity `-70000` command increments/second. These are
arithmetic results, not drive permission or completion evidence.

## Build from the source tree

Run from the repository root with CMake, Ninja and a C++11 compiler:

```text
cmake -S . -B .build/preview -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON
cmake --build .build/preview --target units_preview
```

On Linux run `.build/preview/examples/units_preview/units_preview`. In PowerShell
run `& '.build/preview/examples/units_preview/units_preview.exe'`.

## Build against an installed package

After installing the library to an absolute prefix, build this example as an
independent consumer:

```text
cmake -S examples/units_preview -B .build/installed-preview -G Ninja -DCMAKE_PREFIX_PATH=/absolute/install/prefix
cmake --build .build/installed-preview
.build/installed-preview/units_preview
```

For a Windows path containing spaces, quote the entire CMake argument:
`'-DCMAKE_PREFIX_PATH=C:/path with spaces/install'`. Run the resulting executable
with `& '.build/installed-preview/units_preview.exe'` in PowerShell. The installed
consumer needs no Python, sibling checkout, SDK or CANopen stack.
