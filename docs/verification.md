# Verification

The `position_parameters` and `position_preview` native gates check the installed pure Lichuan target candidate. They do not qualify a position command or motor completion.

The working skeleton has reproducible native, package and documentation gates.
They check implemented local contracts and retained test-only proofs; they do
not qualify a drive, firmware adapter or physical CAN setup. Ordinary consumers
use the [package guide](getting_started.md) and need no verifier dependencies.

## Tools and commands

Developer checks require Python 3.10+, CMake 3.19+, Ninja and a native C/C++
compiler. Full mode requires Doxygen **1.13.2**. Confirm `python --version`,
`cmake --version`, `ninja --version`, the selected compiler's version and
`doxygen --version` before running. Install tools explicitly; the verifier does
not download dependencies. CI uses the pinned environment in
[verify.yml](../.github/workflows/verify.yml), including official Doxygen tag
`Release_1_13_2` at commit `26342b775ea25e6fefb53220926b20702c56fcb3`.
A missing required tool or different Doxygen version
fails with an unavailable result and a nonzero exit status.
The official source build may append its clean pinned Git identity to 1.13.2;
the verifier accepts that exact provenance as well as plain release-version text.

From PowerShell or a Linux shell at the checkout:

```text
python scripts/verify.py --mode quick
python scripts/verify.py --mode full
python scripts/verify.py --mode full --compiler gcc --build-dir ".build/gcc review"
python scripts/verify.py --mode full --compiler clang --build-dir ".build/clang review"
python scripts/verify.py --mode full --compiler clang --sanitize --build-dir .build/sanitized
```

The default compiler selection is GCC. Compiler selection is explicit when
comparing GCC and Clang. Run each supported
toolchain separately; availability on one machine does not establish another
compiler's result. CI runs full GCC 13 and Clang 18 jobs with `--sanitize` on
Linux. A local full run tests the selected toolchain and records other capability
availability; it does not claim that an absent toolchain passed. Sanitizers
require a working ASan/UBSan toolchain and runtime;
the requested gate fails when those are unavailable. Neither verification flags
nor sanitizer libraries become exported package requirements.

Use a fresh ignored build directory for a clean run. The latest summary is
`<build-dir>/summary.json`; each invocation also retains its own timestamped
subdirectory with numbered command logs, summary and build/package artifacts.
Quick summaries explicitly list `package` and `documentation` in `omitted_gates`;
full summaries list no mode omissions. Sanitizer selection and tool availability
remain separate fields. A tool returning empty version output fails with its log
location rather than leaving a run marked in progress.
These identify the executed argument list, tool/version,
status, elapsed time and artifact location. A failure retains its diagnostics
and returns nonzero. Save relevant logs before removing disposable build output;
retained audit/bench/reference evidence is never disposable cache. CI uploads
verification artifacts even when a check fails. Exact run URLs and final-commit
status belong in the stage report, not in claims inferred from workflow YAML.

## Current and planned checks

This table is the maintained inventory. Stage reports record individual results.

| Check | Current command or registration | Mode/status |
| --- | --- | --- |
| Structure, manifest, package files, explicit test registration | `python scripts/check_repository.py` | Quick and full |
| Checked-in version freshness | `python scripts/generate_version.py check` | Quick and full |
| Maintained links and retained reference/ledger integrity | `python scripts/check_promptset.py` | Quick and full |
| Native core, exact units/axis targets, Classical frames, checked SDO/network envelopes, selected-engine vectors, passive node/axis-cache lifecycle, offline previews, native consumers, ordinary application owner, retained ownership proof and multidrive contracts | Explicit CMake/CTest registration in `test/CMakeLists.txt` | Quick and full; C++11, strict private warnings |
| Python version/checker/planning regressions | Explicit CMake/CTest registration in `test/CMakeLists.txt` | Quick and full |
| Source, relocated install and exported-source consumers; missing dependency rejection | `test/package_test.py` / `package_contract` | Full; isolated paths, C++11 and C++17/no RTTI |
| Doxygen output, API inventory and deliberate documentation failures | `doxygen Doxyfile`, verifier API check, `test/documentation_test.py` | Full; exact Doxygen 1.13.2 |
| ASan/UBSan | `scripts/verify.py --mode full --sanitize` | Explicit full option; required in supported CI job |
| GCC/Clang comparison | Full runs with `--compiler gcc` and `--compiler clang` | Supported toolchains checked separately |
| Whitespace | `git diff --check` | Required local handoff check |
| Reviewed object freshness and generator failure contracts | `python scripts/generate_lichuan_objects.py --check` plus `objects_generated` / `objects_generator_contract` | Quick and full; exact bytes, original-source/schema/policy validation, deterministic generation and failure atomicity |
| Bounded SDO lifecycle, ambiguity settlement and fake-node vectors | `sdo_transfer`, `sdo_reconciliation` plus retained adapter/owner proofs | Quick and full; actual optional CANopenNode engine, separate capture/processing evidence, permanent unresolved-channel quarantine; no physical CAN or SDK |
| Fault-clear and counter-reference actions | `reference_action`, `stop_action`, `axis`, owner/service suites | Quick/full; ordered bytes, retained prefix/fault evidence, first-possible-trigger coordinate withdrawal, stale targets, deadline/lifecycle races; no home or firmware qualification |
| Explicit settings through the real owner | `settings_owner`, existing `settings_write`, `driver_settings` and `can_bus_owner` | Quick/full; scripted dictionary, literal bytes, partial effects, epochs and old-target admission. No device evidence |
| Conditional native halt/quick-stop lifecycle | `stop_action` plus `can_bus_owner`, `can_bus_recovery`, `sdo_transfer` and `sdo_reconciliation` | Quick/full; exact command bytes, fresh reports, interruption, pressure, ambiguous channels, time and uncertain effects; no physical stop qualification |
| Urgent quotas, cancellation, controller queue retirement and recovery | `can_bus_recovery`, existing `can_bus_owner` and `owner_preview` | Quick/full; fake controller queues, stale epochs, first-terminal retention, two-node receive and finite recovery; no validated drive stop or physical retirement claim |
| Passive communication health, exact saturating counters/reset and bounded diagnostics | `can_bus_health`, existing owner/recovery suites and native preview | Quick/full; logical-service counting, independent TX evidence, current PDO provenance, cached getters, heartbeat policy and full trace under ordinary/urgent work; no retry or drive-state policy |
| Shared native application and bounded platform I/O | `probe_app`, `native_probe --fake`, existing `units_preview`; isolated copied applications in `package_contract` | Quick/full for native paths; full adds relocated installed-core builds without checkout includes, siblings, SDKs, Python or network. Fake wire values are not hardware qualification. |
| Pinned TWAI policy and actual SDK shim with host fakes | `twai_adapter`, `twai_driver` | Quick/full; one RX owner, queued boot-up, exact TX evidence, deadlines, retirement, loss and SDK failure paths; fake only |
| TWAI SDK interface / target ABI | `python scripts/check_twai_sdk.py --idf-path "path to esp-idf" --idf-python "path to IDF Python"` | Explicit SDK gate; exact IDF 5.5.5 source hashes, S3 C++11 static archive and retained logs; no firmware linked |
| Shared firmware startup / USB platform contracts and build-checker failures | `firmware_probe`, `esp_probe_platform`, `firmware_build_contract` | Quick/full; clock boundaries/faults, zero startup traffic, bounded pressure, routing and retirement using host fakes |
| Arduino firmware compile | `python scripts/check_firmware.py --target arduino` | Explicit SDK gate and Arduino CI job; PlatformIO 6.1.19, pioarduino 55.03.311 / Arduino 3.3.11 / IDF 5.5.5; ELF/BIN and shared compiler-input proof |
| Native-IDF standalone firmware | `python scripts/check_firmware.py --target idf` | Explicit SDK gate and IDF CI job; original source hashes, exact IDF 5.5.5, S3 ELF/BIN and shared compiler-input proof |
| Isolated portable IDF component | `python scripts/check_firmware.py --target component` | Separate explicit SDK/CI gate; only staged core and consumer outside checkout, real link, no owner/stack/reference dependencies |
| Physical CAN/motion qualification | Applicable fixture, firmware and authorized bench procedure | Planned hardware prompts; no current physical result |

## Documentation gate

The direct command is `doxygen Doxyfile` with Doxygen 1.13.2, from the checkout
root. HTML goes to `.build/doxygen/html/` and XML to `.build/doxygen/xml/`.
No network, Graphviz or LaTeX dependency is used. Public input is the complete
`include/` tree, README and the maintained package/verification guides. Research
snapshots and synthetic profiles stay outside public API extraction. There is
no build timestamp or copied version number to maintain in the Doxyfile.
`scripts/doxygen_filter.py` renders relative Markdown navigation as links to
this execution branch on GitHub: Doxygen otherwise treats excluded research
pages as unresolved API references. The repository checker validates the
original local links offline. The filter leaves C++ input and explicit Doxygen
references unchanged, so broken public `@ref` links still fail.

Warnings are fatal, including missing parameter/member comments and unresolved
references. `EXTRACT_ALL` stays disabled; the verifier checks extracted API
inventory so an empty successful documentation build cannot pass. The
`documentation_contract` test runs documented and deliberately broken headers
under the effective strict settings in temporary paths, including paths with
spaces and a `test` directory. It proves nonzero results and useful diagnostics
without mutating production headers:

```text
python test/documentation_test.py
```

Fix generated header comments in `scripts/generate_version.py`, regenerate and
check the result. New API headers must be installed, independently compilable,
documented and visible in the API inventory. Follow
[CONTRIBUTING](../CONTRIBUTING.md) for test registration and focused formatting.
Quick mode omits package and documentation gates; its success is scoped to the
quick checks listed above. Full success still establishes host evidence only.

## Firmware compile gates

[The firmware guide](firmware_examples.md) documents provisioning and commands.
`scripts/check_firmware.py` reuses the verifier's retained-log runner, structured
arguments, bounded subprocesses and nonzero unavailable/failure results. Native
quick/full remain SDK-free; the explicit firmware gates and their two CI jobs
are independent and required for firmware changes. A successful host fake or
TWAI archive build cannot replace any of the three real compile/link gates.
The checker inspects actual compilation databases to prove shared sources were
compiled. No gate flashes, monitors or opens a controller/serial port.

Both SDK commands share source/version/virtualenv validation. The TWAI-only gate
checks its own source manifest; firmware additionally checks the USB/time/task
manifest. The selected virtualenv executable keeps its lexical path, and SDK
constraint checks cannot be disabled. `firmware_build_contract` exercises both
command boundaries and failed-build evidence retention without installing an SDK.
