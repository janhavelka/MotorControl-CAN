# Prompt 7 Native build and package foundation

Apply [COMMON.md](COMMON.md). Prerequisites: [6 Common API and ownership proof](6_api_ownership.md).

## Objective and boundaries

Create the real portable library build and prove source, installed and exported consumption. Migrate the retained small proof into registered tests. This prompt owns CMake/package/version correctness; prompt 8 adds the central verifier, CI and strict documentation command. Public headers introduced here already require Doxygen comments. Do not add nonexistent firmware jobs, catalogue generators or success stubs.

## Read and audit

Refresh RS, ADS1115 and INA228, then inspect current RS CMake targets, install/export configuration, package consumers, `library.json` and version generation. Compare relevant I2C packaging and FieldCore's dependency/consumer boundary read-only. Read actual source/install consumer tests and toolchain constraints. Follow [CONVENTIONS.md](CONVENTIONS.md#1-public-names-layout-and-dependencies) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

## Implement

1. Establish package name `MotorControl-CAN`, public namespace/include/CMake identity `MotorControlCAN` and imported target `MotorControlCAN::MotorControlCAN`. Create `CMakeLists.txt`, `cmake/MotorControlCANConfig.cmake.in`, `library.json`, installed minimal implemented headers and source layout. Use C++11 without forcing project warning/RTTI flags onto consumers. Keep Jan Havelka maintainer identity and MIT attribution consistent with reused source; do not invent a published version tag or remote.
2. Implement one-source version metadata and deterministic `include/MotorControlCAN/Version.h` generation/check mode. Check in generated output. Consumer builds must not need Python or generation; developer checks may use Python. Defer object catalogue generation/freshness to prompt 12, when real reviewed output exists.
3. Support native source builds, `find_package(MotorControlCAN CONFIG REQUIRED)` installation and the intended optional ESP-IDF component boundary without requiring any SDK for native use. Keep controller/stack adapters and examples optional. Core PlatformIO metadata uses appropriate wildcard platforms/frameworks and precise exported files; add `library.properties` only if an actual distribution requirement justifies it.
4. Register the retained proof and initial `*_test.cpp`/`*_test.py` contracts under `test/`, with `test/installed_consumer/` for the installed target. Compile meaningful public headers independently with only declared dependencies. Keep N5/EPOS4 fixtures test-only and compile selected profile dependencies without pulling unrelated stack/catalogue resources into minimal consumers.
5. Define explicit package inclusion/exclusion rules: software license and required source/headers are included; vendor files, audits, examples, test dependencies and sibling snapshots do not become core requirements. Add narrow development-output ignores while preserving retained evidence. Generated version files and original reference artifacts retain their intended Git treatment.
6. Provide direct repeatable build/install/export commands for Windows paths and Linux. Add tests demonstrating stale version detection and package failure when an intended public dependency is missing. Do not rely on a future verifier to prove this prompt's package works.

## Verify and exit

Build and run a minimal native source consumer and isolated installed/exported consumers with strict warnings. Check C++11 library compilation, C++17 consumption and no-RTTI consumption. Use a temporary consumer location that cannot find repository-relative includes, siblings, SDKs, Python, reference documents or CANopenNode accidentally. Verify exported source is buildable without network access.

Run existing planning integrity and diff checks. Inspect the installed file list and ensure no prototype or research fixture is presented as production profile support. Exit with working package commands and attributable minimal code, not just build scripts. Write `docs/reports/prompt_7.md`, update PROGRESS, then commit and sync this branch after all owned checks. Coherent complete packaging checkpoints may be synchronized earlier.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#file-inventory-and-stage-ownership): update README, CHANGELOG, LICENSE and build/installation instructions honestly. Add [Doxygen contracts](DOCUMENTATION.md#doxygen-contract-in-the-source) now; prompt 8 makes their tool gate strict. Apply [MAINTAINABILITY.md](MAINTAINABILITY.md#structure-and-naming). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), delegate isolated consumer review and version-check tests, but one lead owns CMake and package export configuration. Validate the integrated tree before committing.
