> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 03 Establish builds, packages and verification

Apply [COMMON.md](COMMON.md). Prerequisite: stage 02.

## Audit

Read current MotorControl-RS CMake/install/export consumers, `library.json`, version generator, `scripts/verify.py`, checker and `.github/workflows/verify.yml`. Compare ADS1115/BME280/MCP45HVX1 checker tests and manifests. Inspect exact native test registration and framework pins; do not transplant arbitrary SDK versions from a different sibling.

## Deliver

1. Create the real core skeleton with only implemented minimal public types, package identity `MotorControl-CAN`, namespace/include/CMake `MotorControlCAN`, target `MotorControlCAN::MotorControlCAN`, C++11 and MIT attribution for reused source. Use Jan Havelka maintainer identity consistently with the source repository; do not invent a published remote or release tag.
2. Make CMake support native source/install consumers and the intended ESP-IDF component boundary without requiring an SDK for native use. Keep examples, stack adapters and dependencies optional. Add PlatformIO metadata with wildcard core frameworks/platforms and precise export inclusion.
3. Generate Version.h from one metadata source and check freshness. Buildable exported code must not require Python, network, PDFs, examples, tests or the sibling checkout. Stage 05 adds the real catalogue generator, checked-in output and its freshness gate; do not require nonexistent catalogue output in this stage.
4. Establish `verify.py --mode quick` and `--mode full`, repository/checker tests, native test registration and clean source/install/export consumer checks. Keep Windows paths and Linux CI both workable; use structured subprocess calls, timeouts, retained logs and nonzero failures.
5. Define CI with native GCC/Clang, C++11 plus C++17/no-RTTI compatibility, generated-file checks, package consumers and later Arduino/IDF jobs. Add actual firmware jobs only once their runnable examples exist; never create always-green no-op jobs. Document the planned remaining gates.
6. Add license exclusions for vendor/reference snapshots, development artifact ignores, source map and verification guide. Preserve evidence artifacts; do not erase reports as build caches.

## Concrete implementation contract

Create and wire the actual files, rather than describing future checks:

- `CMakeLists.txt`, `cmake/MotorControlCANConfig.cmake.in`, `library.json`, generated `include/MotorControlCAN/Version.h` and its single-source generator.
- `test/` with `*_test.cpp` and `*_test.py`; `test/installed_consumer/` using `find_package(MotorControlCAN CONFIG REQUIRED)` and `MotorControlCAN::MotorControlCAN`.
- `scripts/verify.py`, `scripts/check_repository.py`, the working version freshness command and `.github/workflows/verify.yml`. Register catalogue freshness with the actual generator in stage 05; do not add a success stub now. Keep existing `check_promptset.py` as a planning/reference integrity tool, including byte-preserving `.gitattributes` rules for references/evidence.
- `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `LICENSE`, `Doxyfile` and a source-backed formatting configuration as specified in [DOCUMENTATION.md](DOCUMENTATION.md).

Use C++11 as the required library language. Verify strict-warning native builds, C++17 consumption and no-RTTI consumption without forcing flags onto downstream consumers. Build each public header with only its declared dependencies. Do not add `library.properties` solely because unrelated Arduino libraries have one; document a real distribution need first.

The quick verifier covers repository structure, checked-in generated freshness, native contracts and Python tests. Full adds supported compiler/sanitizer/package/example/documentation gates. List their exact commands and tool versions. Missing tools and unimplemented examples are explicit unavailable gates; never convert required failures into successful skips. Future gates may be listed as planned until their source exists.

Add a negative packaging check: a consumer in a temporary directory can use the installed/exported core without `../MotorControl-RS`, repository-relative includes, an SDK, Python, reference downloads or CANopenNode. Optional examples/adapter dependencies do not become transitive core requirements.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Set up Doxygen now and document the tested command/version; do not postpone in-source contracts to stage 19. README distinguishes working skeleton from implemented device support. CHANGELOG describes only changes that exist.

Use [DELEGATION.md](DELEGATION.md): packaging/consumer review and documentation configuration can be separate workstreams, but one agent owns CMake, verifier registration and CI changes. Integrate before running the complete affected build path.

## Verify and exit

Build a minimal native consumer from the source tree and an isolated install/export. Compile public headers independently with strict warnings; check no accidental SDK or sibling dependency. Exercise checker failure cases so stale generated files/missing registered tests fail meaningfully.

Exit with passing host/package gates and reproducible commands. No hardware or remote creation is required; CI configuration is not a claimed CI execution.
