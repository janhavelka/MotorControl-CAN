# Native package use

The installed [typed read API](reads.md) needs only the core target. The
[reads preview](../examples/reads_preview/README.md) compiles a finite identity
read with synthetic evidence, without a controller. Read effects and model
applicability remain unqualified; the non-consuming probe is evidence gated.

The development package `MotorControl-CAN` currently implements local call
status, exact numeric/unit conversion and quantization, pure axis targets, copied logical endpoint/
binding identity, Classical CAN frame validation and pure checked CANopen SDO envelopes. The C++ namespace, include
directory, CMake package and target are
`MotorControlCAN`, with imported target `MotorControlCAN::MotorControlCAN`.
There is no installed drive profile, CAN controller, motion operation or
hardware qualification. Version `0.1.0` describes this development source and
does not identify a published release or tag.

## Requirements and ordinary use

Native consumers need CMake 3.19+, a C++11 compiler and a build tool. The numeric
API requires IEEE binary32/binary64. Python, ESP-IDF, Arduino, CANopenNode,
sibling checkouts and reference documents are not package dependencies.
The maintained consumer in
`test/installed_consumer/main.cpp` exercises the public functions under C++11
and C++17, with RTTI disabled for the C++17 target.

```cmake
find_package(MotorControlCAN CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE MotorControlCAN::MotorControlCAN)
```

Include `<MotorControlCAN/MotorControlCAN.h>` for the small complete interface,
or `<MotorControlCAN/Axis.h>`, `<MotorControlCAN/Units.h>`, `<MotorControlCAN/Endpoint.h>` and
`<MotorControlCAN/can/Frame.h>` individually. The [units/status guide](units_status.md)
documents exact inputs, scale dependencies, rounding and explicit approximation.
The [axis guide](axis_contract.md) covers configuration, origins, references and
checked position/velocity targets; these prepare values without transmitting motion.
`validateBinding` checks common nonzero identities/generations, without probing
a drive or imposing CANopen addressing. `validateFrame` checks Classical CAN
shape and rejects an FD requirement. Their successful `Status` proves local
validation only. Include `<MotorControlCAN/canopen/Sdo.h>` for explicit
channel/binding/width checks, owned reply evidence and signed little-endian
words; see the [service guide](canopen_services.md). These functions neither
encode an active transfer nor open a controller. Public headers document
validity, units, ownership and limits.

For source consumption use `add_subdirectory("path/to/MotorControl-CAN" core)`
and link the same target; the library does not enable repository tests or change
the parent's `BUILD_TESTING`. Private warnings are not exported to consumers.

## Build and install

From PowerShell at the checkout (paths with spaces remain quoted):

```powershell
cmake -S . -B ".build/native" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="C:/Temp/MotorControl CAN"
cmake --build ".build/native"
cmake --install ".build/native"
cmake -S test/installed_consumer -B ".build/installed consumer" -G Ninja "-DCMAKE_PREFIX_PATH=C:/Temp/MotorControl CAN" "-DMOTORCONTROLCAN_HEADERS_DIR=C:/Temp/MotorControl CAN/include"
cmake --build ".build/installed consumer"
ctest --test-dir ".build/installed consumer" --output-on-failure
```

The equivalent Linux commands:

```sh
cmake -S . -B .build/native -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$PWD/.install/core"
cmake --build .build/native
cmake --install .build/native
cmake -S test/installed_consumer -B .build/consumer -G Ninja -DCMAKE_PREFIX_PATH="$PWD/.install/core" -DMOTORCONTROLCAN_HEADERS_DIR="$PWD/.install/core/include"
cmake --build .build/consumer
ctest --test-dir .build/consumer --output-on-failure
```

Install locations follow `GNUInstallDirs`, including a custom
`CMAKE_INSTALL_INCLUDEDIR`. The prefix is relocatable. Installation contains
the public headers, static library, CMake config/target/version files and
the software license. The consumer may request an exact metadata version with
`find_package(MotorControlCAN 0.1.0 EXACT CONFIG REQUIRED)`.

## Offline coordinate preview

The repository example uses illustrative independent command/feedback scales,
gearing and travel with no full-step geometry. It prints exact targets and
binary32 quantization evidence, opens no controller, and is excluded from core
source distributions. From PowerShell or Linux at the checkout:

```text
cmake -S . -B .build/preview -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON
cmake --build .build/preview
.build/preview/examples/units_preview/units_preview
```

On Windows the executable has the `.exe` suffix. Its
[README](../examples/units_preview/README.md) also documents an installed-package build.
Repository tests run the same example and a small native axis consumer.

## Repository tests and source export

Developer tests need Python 3.10+, Ninja and C/C++ compilers. They register the
retained synthetic operations/owner and N5/EPOS4 fixtures under `test/`; these
tests additionally build the pinned local CANopenNode experiment. Running the
complete CTest inventory also requires Doxygen 1.13.2; the quick verifier omits
documentation and package checks as described in the [verification guide](verification.md).
The installed library does not link that experiment. Checks stay active in Release builds.

```text
python scripts/generate_version.py check
cmake -S . -B .build/tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .build/tests
ctest --test-dir .build/tests --output-on-failure
python scripts/check_promptset.py
git diff --check
```

`package_contract` builds fresh source and installed consumers in an external
temporary directory with spaces. It compiles every header independently,
checks C++11 and C++17/no-RTTI consumption, relocates the install prefix and
proves a missing `Status.h` dependency fails in a fresh build. A small CMake
mock checks component source registration; this is not an ESP-IDF firmware build.

Export a source archive using the explicit `library.json` export allowlist:

```text
python scripts/export_package.py dist/MotorControl-CAN.zip
```

Extract it with a ZIP tool, then use the native build/install commands above.
The exported build needs neither Python nor network access. The archive contains
only `include/`, `src/`, `cmake/`, root CMake/metadata/license/readme/changelog
and the package, units/status, axis and CANopen service guides. Vendor material, audits, test/experiment
sources and dependencies, examples and sibling snapshots are excluded. PlatformIO 6.1.19's equivalent
developer packaging command is `pio pkg pack --output dist/MotorControl-CAN.tar.gz`;
its platforms/frameworks are wildcard because the core has no SDK dependency.
No `library.properties` is needed for the current CMake/PlatformIO distribution.

`library.json` is the only version source. After intentionally changing it,
run `python scripts/generate_version.py sync`, commit the generated `Version.h`
and rerun `check`. Ordinary builds consume that checked-in header directly.
Reviewed object headers are checked in. Include
`<MotorControlCAN/profiles/lichuan_c/Objects.h>` explicitly for compact metadata;
`Catalogue.h` adds optional static descriptive strings. The root umbrella remains
common-only, and neither header establishes an operational drive profile.
`<MotorControlCAN/profiles/lichuan_c/Pdo.h>` separately provides reviewed passive
layout facts and unverified EDS templates. Pure `<MotorControlCAN/canopen/Pdo.h>`
validates/decodes supplied native layouts; the optional owner separately requires
actual-map and sample-provenance evidence. No catalogue or stack is needed by
these core headers, and no PDO configuration is written by including them.
Developer freshness uses `python scripts/generate_lichuan_objects.py --check`;
installed consumers need no Python, ledger or original vendor material. The [verification guide](verification.md)
documents the working quick/full verifier, CI and strict Doxygen gate. These
developer tools are outside the core distribution.

## Optional CANopenNode application adapter

The separate [CANopenNode application adapter](../adapters/canopennode/README.md)
can be built explicitly in a full checkout. Its selected stack/port is omitted
from the exported/installed core. `<MotorControlCAN/canopen/SdoTransfer.h>`
contains portable service values without either dependency. Native fake-node
tests do not establish an SDK/controller port or firmware build.

## Optional ESP-IDF component boundary

When ESP-IDF supplies `idf_component_register`, root CMake registers the same
portable sources and public include directory with a C++11 floor. The application
owns the SDK, controller, bus configuration and all I/O. Native builds never
load ESP-IDF. Actual board adapters and firmware examples are later milestones;
the current component contract does not imply a tested firmware target.
