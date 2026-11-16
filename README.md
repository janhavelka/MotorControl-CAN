# MotorControl-CAN

A C++11 library for exact motion intent and conditional Lichuan C drive operations
over Classical CANopen. The installed core uses caller-owned contexts and supplied
events. It performs no I/O, allocation, clock reads, sleeps or automatic retries.
Applications own the controller, time, scheduling and retained results.

This is a **development software candidate**, version **0.1.0**, not a published
or physically qualified release. Lichuan C is the only implemented profile.
CL86-C specimen observations do not establish support for every firmware or the
related OL86-C/CL57-C/OL57-C drives. N5 and EPOS4 are test-only design contrasts;
proprietary CAN and CAN FD need additional implementations.

| Layer | Available scope | Limit |
| --- | --- | --- |
| Installed core | Exact units, axis/reference conversion, CAN/SDO/PDO values, finite caller-timed SYNC, typed reads/settings, conditional actions, position/angle, finite velocity and gated in-motion changes, homing, discovery and persistence | Per-operation prerequisites and firmware gates apply |
| Optional application layer | One CANopen owner with finite SYNC service, CANopenNode adapter, ESP TWAI adapter, shared CLI and bounded Python runners | Full repository required; not installed with the core |
| Firmware | S3 Arduino/IDF and S2 IDF build paths | Arduino S2 is blocked by the pinned package; P4 has isolated core/component evidence only |
| Hardware | Retained S2/two-CL86-C communication and conditional motion trials | Physical motion/stop, failure/load and related-model qualification remain incomplete |

See the [candidate scope and migration notes](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/releases/0.1.0.md)
and [current feature gates](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/device_conflicts.md).
The [finite SYNC guide](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/sync.md)
covers explicit production and the conditional TPDO1 type 1/255 experiment.
Qualified same-cycle samples, synchronous RPDO starts and sustained two-axis
10 ms operation remain open work.
The [extension qualification report](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/prompt_54.md)
records the bounded diagnostic tools and specimen campaign without promoting
software timing or raw PDO counts to physical synchronization evidence.

[Concurrent axes and group stop](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/multi_axis.md)
use one owner, configurable application capacity and independent per-axis results.
Each axis can be selected, changed or stopped while another axis retains work.
Optional PSRAM storage is for application-owned cold records; the core remains
heap-free and live transport/stop storage stays internal.

## Install and use the native core

Requirements: CMake 3.19+, Ninja and a C++11 compiler. From the repository or
exported core source root, these commands need no Python, SDK, CANopenNode,
sibling checkout or connected board. They work in PowerShell and POSIX shells.

```text
cmake -S . -B .build/core -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "-DCMAKE_INSTALL_PREFIX=./.install/core"
cmake --build .build/core
cmake --install .build/core
```

Create `consumer/main.cpp` with this complete offline example. Its scales are
illustrative host data, not measured CL86-C mechanics. It emits no CAN requests.

```cpp
#include <MotorControlCAN/Axis.h>
#include <cstdio>

int main() {
    namespace M = MotorControlCAN;
    M::AxisConfig axis;
    axis.binding.logicalAxisId = 9;
    axis.binding.endpoint.protocol = M::Protocol::CANOPEN;
    axis.binding.endpoint.profile = 1;
    axis.binding.endpoint.address = 1;
    axis.binding.bindingGeneration = axis.binding.configurationGeneration = 1;
    axis.units.commandStepsPerMotorTurn = M::UnitScale(4096, 1, M::ScaleSource::ASSUMED);
    axis.units.motorTurnsPerLoadTurn = M::UnitScale(3, 2, M::ScaleSource::ASSUMED);
    axis.units.millimetresPerLoadTurn = M::UnitScale(20, 1, M::ScaleSource::ASSUMED);
    axis.originKnown = true;
    axis.originSource = M::ScaleSource::ASSUMED;
    M::PositionRequest request;
    request.value = M::Rational(5);
    request.unit = M::PositionUnit::MILLIMETRES;
    request.frame = M::CoordinateFrame::LOAD;
    request.configurationGeneration = 1;
    M::PreparedTarget target;
    const M::Status status = M::preparePosition(request, axis, nullptr, target);
    if (!status) {
        std::fprintf(stderr, "%s: %s\n", M::errToString(status.code), status.msg);
        return 1;
    }
    std::printf("5 load mm -> %lld command increments\n",
                static_cast<long long>(target.effectiveNative));
    return target.effectiveNative == 1536 ? 0 : 2;
}
```

Create `consumer/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.19)
project(Quickstart LANGUAGES CXX)
find_package(MotorControlCAN CONFIG REQUIRED)
add_executable(quickstart main.cpp)
target_link_libraries(quickstart PRIVATE MotorControlCAN::MotorControlCAN)
set_target_properties(quickstart PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
```

From the same source root, build and run it (append `.exe` on Windows):

```text
cmake -S consumer -B .build/consumer -G Ninja "-DCMAKE_PREFIX_PATH=../.install/core"
cmake --build .build/consumer
.build/consumer/quickstart
```

The prefix above is relative to the consumer source directory. For another
directory layout, pass the absolute installation prefix. The
[native package guide](docs/getting_started.md) covers custom/relocated installs.

## Read, move and stop with supplied evidence

From a **full checkout**, after the install above:

```text
cmake -S examples/position_preview -B .build/position -G Ninja "-DCMAKE_PREFIX_PATH=../../.install/core"
cmake --build .build/position
.build/position/position_preview
```

This maintained example uses only the installed core: exact preparation,
`prepareIdentity`/`nextRead`/`advanceRead`,
`prepareMoveAbsolute`/`nextMove`/`advanceMove`, and
`prepareStop`/`nextAction`/`advanceAction`. It checks missing-evidence rejection,
then runs a simulated move and stop using supplied service results. Its
[complete source](https://github.com/janhavelka/MotorControl-CAN/blob/main/examples/position_preview/main.cpp)
labels the invented observations and evidence records. They authorize no device
traffic. Real work goes through one application owner. Retain the original
operation result even when a later stop succeeds; an acknowledgement is distinct
from physical movement or standstill. Local cancellation is not a motor stop.

The [read guide](docs/reads.md), [axis contract](docs/axis_contract.md),
[position parameters](docs/position_parameters.md) and
[stop contract](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/stop_actions.md)
define units, gates, lifetimes and failures. Repository examples are excluded
from the core archive; the README consumer above is self-contained.

## Firmware and Python

The full checkout supplies a shared application/CLI with thin Arduino and IDF
entry points. Default compile fixtures leave CAN inactive. Follow the
[firmware guide](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/firmware_examples.md)
for pinned Arduino 3.3.11 / ESP-IDF 5.5.5 builds, actual board pins and explicit
controller start. Building does not flash or qualify hardware. The separate S2
bench image has its own evidence controls and is not the default console.

Python 3.10+ offline commands, from the full checkout:

```text
python scripts/bench_probe.py --plan-only --scenario identity
python scripts/bench_probe.py --self-test
python scripts/bench_scenarios.py --list
```

Offline use opens no port and needs no serial package. Live use also needs
pyserial 3.5, a configured console, established node/bitrate/profile and explicit
finite intent. The [runner guide](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/runners.md)
gives the full invocation, immutable deadlines and retained failure logs.
Startup and recovery never implicitly enable, save or move a drive.

## Documentation, checks and license

- [Documentation index](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/README.md): user contracts, examples and source map.
- [Progress and inherited gates](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/prompts/PROGRESS.md): implemented, Partial and future work. Prompt18/Q17 remapping and physical qualification remain open.
- [Verification](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/verification.md): `python scripts/verify.py --mode full` runs native contracts, exported/installed consumers, generated checks and strict Doxygen 1.13.2.
- [Contributing](https://github.com/janhavelka/MotorControl-CAN/blob/main/CONTRIBUTING.md) and [security reporting](https://github.com/janhavelka/MotorControl-CAN/blob/main/SECURITY.md).
- [CHANGELOG](CHANGELOG.md): public changes and descriptor migration. `library.json` is the version authority.

The core is [MIT licensed](LICENSE); adapted RS code retains its attribution.
Optional CANopenNode is Apache-2.0 with retained notices; SDKs have separate
licenses. Vendor manuals/EDS/full extracts, copied reference code, tests/tools
and bench evidence are excluded from the portable core distribution.
Repository links describe current main and may be newer than an exported
archive; select the archive's commit when comparing historical behavior.

Prompt52 adds explicit positive velocity changes and absolute position replacements, with retained original/replacement identities and unchanged finite stop budgets. Defaults remain closed; [change evidence and current bench limits](https://github.com/janhavelka/MotorControl-CAN/blob/main/docs/reports/prompt_52.md) distinguish experiment support from qualified drive behavior.
