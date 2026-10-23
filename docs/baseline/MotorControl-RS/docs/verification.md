# Repeatable verification and core packages

Use the commands below to verify the current source and produce a core package.
The package version comes from `library.json`; generated headers and installed
CMake metadata must agree. Software checks do not establish motor or electrical
qualification. Older candidate and hardware records remain under
[reports](archive/reports/ess_release_30_2026-10-05.md) with their original versions,
commits and hashes.

Run from the repository root with Python 3.10+, CMake/CTest 3.26+, Ninja and
GCC or Clang with GNU/LLVM `nm` available to CMake. The verifier is qualified
on Windows MinGW GCC and hosted Linux GCC/Clang; MSVC verification remains
unqualified. Tests require unoptimized Python: do not set `PYTHONOPTIMIZE`.
Install the offline PDF checker dependencies once:

```sh
python -m pip install -r scripts/requirements-verification.txt
python scripts/verify.py --mode quick
python scripts/verify.py --mode full
```

The core build still supports CMake 3.16. The verifier uses CTest's
[fail-on-zero-tests option](https://cmake.org/cmake/help/latest/manual/ctest.1.html#cmdoption-ctest-no-tests),
which requires 3.26.

Quick runs all registered Release native/Python suites, generated register and
version checks, operation inventory, offline reference hashes and local Markdown
links. It then exports the reviewed core-only package, checks every ZIP member
and its bytes, extracts that archive, builds/installs it, and
builds/runs independent source and exact-version `find_package` consumers.
Every public header compiles alone in strict C++11 and C++17; C++17 consumers
disable RTTI. Both codec-only consumers must omit descriptive catalogue strings
and symbols. The static CMake target exports the configured install include path.
No examples, Python, vendor files or sibling repository are package dependencies.

Full additionally builds the four pinned Arduino environments and native IDF
S3 firmware, core-only S3/S2 consumers and the portable S2 application fixture.
Export the pinned ESP-IDF 5.5.5 SDK/tool environment first. `--idf-path PATH`
and `--idf-python PATH` select an existing SDK and its Python environment.
PlatformIO must already be installed; Windows falls back to the repository's
managed-Core wrapper. No SDK or tool is installed silently.

CI separates the same full checks with `--firmware arduino` and `--firmware idf`;
each explicitly selected run also executes quick checks. Hosted Ubuntu GCC and
Clang jobs cover compiler differences. Firmware jobs use the pinned PlatformIO
platform and IDF container. They compile firmware and do not access COM13,
flash hardware, run motion or assert hardware qualification. Local Windows GCC
verification is recorded separately from hosted CI execution.

Every run uses a new `build/verify/<timestamp>` directory, or a new directory
chosen with `--build-dir PATH`. Existing output is rejected. Logs and
`summary.json` retain exact commands, durations, failure status and package
SHA-256. The ZIP uses stable entry order, timestamps and permissions. A missing
tool/input, unregistered suite, empty/skipped test or failing command is a
failure. Registration checks require the actual interpreter, script, native
executable and check arguments; per-test Python optimization overrides fail.
CTest properties that turn a failing exit into PASS are also rejected.
Reference checks are offline; original vendor bytes remain separately
licensed. Local links are checked in maintained documents, active/archived
prompts and report/archive indexes. Dated report bodies and PDF extracts keep
their historical link-source exemption; links to those files must still resolve.
External URLs, anchors and explicitly named sibling reference repositories are
not availability claims.

For a clean check, use a fresh checkout or a staging copy of tracked source
plus the reviewed changes, with no `.pio` or build outputs. Tool download caches
may remain external. A verifier run never deletes existing builds, evidence or
flash backups.

The exported ZIP contains only `include`, `src`, root CMake and metadata,
license, README and changelog. It can be unpacked anywhere and consumed without
repository-private helpers:

```sh
cmake -S MotorControlRS -B core-build -G Ninja -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/absolute/core-install
cmake --build core-build
cmake --install core-build
cmake -S consumer -B consumer-build -G Ninja -DCMAKE_PREFIX_PATH=/absolute/core-install
cmake --build consumer-build
```

An independent consumer needs only its own CMake project/main and
`find_package(MotorControlRS 1.0.0 EXACT CONFIG REQUIRED)` followed by linking
`MotorControlRS::MotorControlRS`. The checked consumer fixture also supports
`MOTORCONTROLRS_SOURCE_DIR` and `MOTORCONTROLRS_HEADERS_DIR` for direct source
and individual-header checks. Hardware and endurance dispositions remain in
their owning reports; a green verifier is software/build evidence.

## First release tag

The existing annotated `v1.0.0` tag points to
`4125a092d91b4b6423d1664ee51fff209110006f` (local and remote verified on
11 October). Earlier `0.x` numbers were development snapshots, not published
releases. Do not recreate the tag or change versions recorded in old evidence.

For future releases, create an annotated tag only after the release commit is
pushed and all four CI jobs pass. Push the tag, then check its new CI run. A tag
does not publish a GitHub Release; publication is a separate GitHub action.
The active remote was verified on 11 October as `janhavelka/MotorControl-RS`;
the [repository rename](archive/reports/2026-10-11_repository_rename.md) has completed for this checkout.

GitHub's automatically generated source archives contain the repository,
including separately licensed vendor references. The verifier's
`MotorControl-RS-1.0.0.zip` is the smaller core-only distribution; attach that
ZIP when a consumer needs only the library. Its SHA-256 is recorded in the
verifier's `summary.json`.

Earlier measurements are preserved in [early verification history](archive/reports/early_verification_history.md).
