# Prompt 7 native build and package foundation

Date: 2026-10-29. Status: Complete. Started from `6271415` on the existing
`promptset/motorcontrol-can` branch, tracking `origin/promptset/motorcontrol-can`.
The worktree was clean and fresh fetch showed 0 ahead / 0 behind. Prompt 6 and
its independent audit were complete. No new execution branch was created.

## Implemented package and migration

The [native guide](../getting_started.md) owns repeatable Windows/Linux source,
install, export and developer commands. The package is `MotorControl-CAN`,
namespace/include/CMake identity `MotorControlCAN`, imported target
`MotorControlCAN::MotorControlCAN`. Development version 0.1.0 comes solely from
`library.json`; no release tag or published artifact is claimed.

Five installed headers expose actual status helpers, copied endpoint/binding
identity, Classical CAN frame validation and generated version constants.
`src/Endpoint.cpp` and `src/can/Frame.cpp` contain the corresponding pure
implementations. The stable declarations moved from the proof; behavior and
numeric detail 1 are retained, with `BindingError` removing a dependency on
prototype `ReadError`. There is no installed synthetic profile, I32-native axis
assumption, arithmetic engine, stack, controller or vendor drive support.
Installed functions have no allocation, clocks, I/O, retry or application state.

Root CMake builds a C++11 static target with private warnings, relocatable build/
install include paths, configured package template and exported targets/version
files. An application receives only include paths and the public C++11 floor.
Native use requires no SDK, Python, generator, network or sibling checkout.
When ESP-IDF supplies `idf_component_register`, it registers the same two pure
sources and includes. This branch was checked with a CMake mock; no actual
ESP-IDF toolchain/board build is claimed. PlatformIO platforms/frameworks are
wildcard. No current distribution requires `library.properties`.

`scripts/generate_version.py sync|check` deterministically derives documented
`Version.h` constants from metadata. Missing/stale headers fail check without
mutation; malformed metadata and numeric-encoding bounds are tested in isolated
copies. Consumers use the checked-in header without running this helper.

Proof entry points moved to `test/api_ownership_test.cpp` and
`test/multidrive_contract_test.cpp`, with their original assertions preserved.
Root CTest registers them, the selected-engine test, core tests and Python
version/package contracts. The legacy experiment CMake entry point still works.
Synthetic operations, capability/units prototypes and the fixed owner stay as
test support in `experiments/api_ownership`; they link the installed common
implementation, so there is no second binding/frame implementation.

`library.json` explicitly exports 14 source/documentation files. The small
standard-library ZIP helper reads that allowlist and copies exact input bytes;
fixed ZIP timestamps/platform/mode and sorted names make it reproducible.
PlatformIO's actual export has the same 14 files and exact bytes. Installation
contains exactly five public headers, one static library, four CMake files and
the software license (11 files in the checked Release install). Vendor material,
audits, experiments, test dependencies, examples and sibling snapshots are
excluded. MIT copyright remains 2026 Jan Havelka; vendor/stack rights remain
separate. Narrow build/install/dist ignores preserve retained evidence and the
tracked generated version header. Original artifacts/attributes are unchanged.

## Recurring source audit and independent review

| Checkout | Exact HEAD / dirty state | Current packaging evidence read |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, clean | `CMakeLists.txt`, `library.json`, `scripts/generate_version.py`, `test/installed_consumer/{CMakeLists.txt,main.cpp,codec.cpp}`, `scripts/verify.py` export/unpack/consumer paths and `.github/workflows/verify.yml` |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean | `CMakeLists.txt`, `library.json`: component-only C++11 CMake, constrained sensor package metadata |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean | `CMakeLists.txt`, `library.json`: component-only C++17 CMake, distinct sensor/toolchain contract |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | `platformio.profiles.generated.ini` pins sensor dependencies; `include/TunnelMonitor/device/Rs485MotorModule.h` imports selected RS profile; `scripts/prepare_rodtester_host_dependencies.py` stages its host dependency |

No revisions/dirty-file behavior changed from prompt 6. All sibling activity was
read-only; unchanged suites were not rerun. RS establishes naming, maintainer,
wildcard metadata, version and isolated-consumer patterns. CAN additionally uses
the required config template and explicit installed file list. I2C component-only
CMake is not a native package template, and INA228's C++17 floor is not imported.
FieldCore owns application dependency selection; it is no dependency of this core.

Three bounded agents used the inherited runtime model/effort: one owned the
version helper/tests, one migrated stable core/proof files, and one reviewed
sibling packages then independently challenged isolated consumption. The lead
owned CMake, export, package tests, documentation, integration and Git. Independent
review reran the package contract and found a host-dependent ZIP platform field;
it now explicitly uses the Unix encoding on every host. The review also refined
the isolation claim: Python package discovery is disabled on all tested consumer
configurations; Windows PATH also physically excludes Python, while Linux tools
may share `/usr/bin` with Python. No unsupported absence claim is made for Linux.
An initial harness failure from case-sensitive copied `SystemRoot` lookup was
fixed using the Windows environment mapping; the full isolated test then passed.

## Verification and handoff

Tools: CMake 4.0.1, Ninja 1.12.1, GCC/G++ 15.1.0 (MinGW x64), Python 3.12.10,
PlatformIO 6.1.19 and Doxygen 1.13.2. Native results are Windows-host results;
Linux commands are documented but were not run in a Linux toolchain here.

| Exact command / check | Result |
| --- | --- |
| `cmake -S . -B .build/p7 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-Werror` then `cmake --build .build/p7` | PASS: installed library compiled C++11; authored tests use strict warnings, checks active under NDEBUG |
| `ctest --test-dir .build/p7 --output-on-failure` | PASS 7/7: core, api_ownership, multidrive_contract, version_generated, version_contract, package_contract, canopen_service_fit |
| `python test/version_test.py` | PASS 6 tests: deterministic generation, stale/missing output, malformed/missing metadata and numeric limits |
| `python test/package_test.py` | PASS: 14-file export with exact bytes, external directory with spaces, source consumer, relocated install/custom include directory, all headers independently compiled, strict C++11/C++17/no-RTTI consumers, failed fresh build for missing Status.h, IDF registration mock |
| Direct repository source consumer via `test/installed_consumer`, `MOTORCONTROLCAN_SOURCE_DIR` and `MOTORCONTROLCAN_HEADERS_DIR` in `.build/p7_source` | PASS 2/2: strict C++11 and C++17/no-RTTI, independent headers; add_subdirectory enables no tests, Python or stack |
| Native minimal package build | PASS with only C++ compiler/CMake/Ninja tools on Windows PATH, Python discovery disabled and no SDK/stack/test/reference files in extracted package; no download or fetch operation |
| Legacy `cmake -S experiments/api_ownership -B .build/p7_legacy -G Ninja -DCMAKE_BUILD_TYPE=Release`, build and CTest | PASS 3/3 retained tests after migration |
| `pio pkg pack --output .build/MotorControl-CAN.tar.gz` plus archive-name/byte comparison | PASS: same 14 allowlisted files; no publication/network dependency installation |
| `doxygen .build/p7_doxygen.conf` | PASS zero warnings over all five installed headers with undocumented/enum/parameter/doc warnings fatal; temporary config uses recursive include input and XML only. Central reproducible gate belongs to prompt 8. |
| `python -m unittest discover -s scripts -p '*_test.py'` | PASS 73 planning/ledger regressions |
| `python scripts/check_promptset.py` and `--check-git-ref INDEX` / `HEAD` | PASS: prompt/requirement/link/ledger checks and all 134 original artifact hashes; staged/committed byte checks repeated during delivery |
| `git diff --check`, staged/whole-change whitespace checks | PASS |

Documentation updated together: README, docs index, native guide, source map,
architecture, backlog, coverage, roadmap, progress, CHANGELOG, LICENSE and proof
README. Public declarations already have Doxygen contracts. No central verifier,
workflow, board example or catalogue generator was added; each retains its owner.
The same small core functions and concrete proof contexts were reused without
new runtime abstractions. No firmware build or physical CAN activity occurred.

Prompt 8 is next. Its central verifier/CI/Doxygen gate remains pending, as do
production units, services, profiles, controllers and all physical qualification.
There is no repository workflow to dispatch for this packaging milestone;
remote CI is unavailable rather than passed. The commit containing this report
is committed/pushed on the existing execution branch; its exact ID and fresh
remote ahead/behind comparison are recorded in the final handoff.

## Fresh independent audit of prompts 7-9

The [independent audit](prompts_7_9_audit.md) reread this original prompt and
checked the actual current code, diff, callers and failure tests. It records
the confirmed fixes, source revisions and integrated delivery verification.
This stage retains Complete status and its original scope.
