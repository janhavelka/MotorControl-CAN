# Prompt 8 Verification CI and documentation gates

Apply [COMMON.md](COMMON.md). Prerequisites: [7 Native build and package foundation](7_native_package.md).

## Objective and boundaries

Turn the existing build/package checks into reproducible local and CI gates, with strict Doxygen from this point onward. This prompt delivers executable checks and contributor documentation. It does not manufacture firmware examples or catalogue output to satisfy a future gate. Required unavailable tools are reported honestly, never represented as passing no-op checks.

## Read and audit

Refresh RS, ADS1115 and INA228. Inspect current RS `scripts/verify.py`, repository checker, test registration, workflow and retained-log behavior. Compare ADS1115/BME280/MCP45HVX1 checker failure tests and I2C Doxyfile/CI practice. Inspect FieldCore's relevant native consumer seam read-only. Follow [CONVENTIONS.md](CONVENTIONS.md#7-documentation-doxygen-and-meaningful-gates) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases). Read the actual available CI environment and parent/root CI policy before choosing commands and pins.

## Implement

1. Implement `scripts/verify.py --mode quick` and `--mode full`, `scripts/check_repository.py`, meaningful checker tests and `.github/workflows/verify.yml`. Reuse prompt 7's real build/install/version commands. Use structured subprocess arguments, bounded timeouts, nonzero failures, retained logs and paths that work on Windows and Linux. Keep source and installed consumer checks independent enough to detect packaging omissions.
2. Make quick cover repository structure, implemented generated-file freshness, registered native contracts and Python tests. Full adds supported compilers, sanitizers, package consumers and documentation. Include native GCC/Clang, C++11, C++17 consumption and no-RTTI compatibility where toolchains exist. Do not force verification flags into exported consumer requirements.
3. Register every current test explicitly according to the audited project pattern. Add negative checker cases for stale version output, missing test registration, invalid manifests and missing required package files. Future catalogue freshness becomes active in prompt 12 with its actual generator. Future Arduino/IDF jobs become active when prompts 27–28 provide buildable adapters/examples.
4. Add `Doxyfile` and the real strict documentation command/version, with public API warnings and unresolved references treated as failures as specified in DOCUMENTATION. Cover existing public headers now. Keep generated files documented through their source/generator rather than manual fixes. A dependency missing from the machine is an unavailable gate requiring an honest result, not an always-green substitute.
5. Complete `CONTRIBUTING.md`, `SECURITY.md`, verification guidance, documentation index and a formatting configuration justified by source conventions. Explain clean builds, retained evidence versus disposable artifacts, generated-file ownership, package consumption and how new tests/docs enter verification. README/CHANGELOG describe the working skeleton, not planned device support.
6. Define current versus planned checks in one maintained table. Existing automatic CI triggers remain respected; manual remote runs follow milestone policy. This build/workflow integration is a milestone: verify required CI on the final commit when the configured remote/service is available, and report unavailable execution accurately.

## Verify and exit

Run quick, supported full gates and Doxygen against the integrated tree. Demonstrate that intentionally stale generated output, a missing registered test and a public documentation error cause useful failures, then restore the correct files. Do not weaken checks or convert failures into successful skips. Confirm retained logs identify command, tool/version, status and artifact location.

Run planning integrity and diff checks. Exit with actual local results and exact remote CI status for the final commit, not a claim that adding YAML executed CI. Close scope in `docs/reports/prompt_8.md`, update PROGRESS, then commit and sync this branch; a final CI result may be recorded in a subsequent checked documentation checkpoint.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#documentation-build-and-verification) and [MAINTAINABILITY.md](MAINTAINABILITY.md#tests-and-refactoring). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), checker failure tests and documentation review can be independent tasks. One lead owns verifier registration, CMake integration and workflow changes. Integrate all changes before running complete affected paths; worker success is not integrated verification.
