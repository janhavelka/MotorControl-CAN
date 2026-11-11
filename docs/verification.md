# Verification

The registered `probe_app_local`, `probe_console` and `console_protocol` contracts
exercise direct pure APIs, damaged-line/pressure/correlation cases, actual JSONL
and the real native entry. The isolated package consumer compiles the console
against installed public headers and runs a native batch. Firmware inventory
checks require both ProbeConsole.cpp and ProbeAppLocal.cpp; later device route
and full platform parity remain prompts 41/42.

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
| Gated enable | `enable_action`, `action_preview`, `axis_session`, `stop_action` | Quick/full; literal bytes through the real owner, target review before any word, each step's scope, word and observation failures, interval, deadline and cancellation precedence, boot and bus-off, session history, a stop after each step, atomic refusals, and the measured node 1 bytes (`zeroSpeedBitFollowsTheEnable`). These tests use a fake drive; the attended runs on two specimens are bench evidence, not these tests, and no torque, retained-target or firmware claim follows |
| Gated absolute move | `move_absolute`, `position_preview`, `axis_session` | Quick/full; literal bytes through the real owner and `AxisSession`, with and without heartbeat traffic: phase order, gates before TX, parameter retirement, trigger fencing, caller-supplied acceptance and completion masks, and stops from the held `001F`. These tests use a fake drive and fake masks; the one specimen run, through the bench `move` route on node 2 ([unattended report](reports/2026-11-03_unattended_bench.md)), is bench evidence, not these tests, and qualifies nothing (packet 20) |
| Fault-clear and counter-reference actions | `reference_action`, `alarm_clear`, `counter_clear`, `stop_action`, `axis`, owner/service suites | Quick/full; ordered bytes, retained prefix/fault evidence, unresolved fault identity and `valueOneMayRemain` (packet 17), the counter baseline and each write failing at each stage (packet 18), first-possible-trigger coordinate withdrawal, stale targets, deadline/lifecycle races; no home or firmware qualification |
| Shared evidence checks and adversarial timelines | `sdo_evidence`, `evidence_corruption`, `evidence_timeline` | Quick/full; private terminal, work and overlap checks against literal oracles, every field corruption of each consumed terminal refused byte-identically, and budget timeouts around the overall cutoff (packet 10). Synthetic traffic only |
| S2 flash, smoke runner, offline parser and read-only soak | `s2_flash_contract`, `s2_smoke_contract`, `s2_evidence_contract`, `soak`, `soak_transcript`, `s2_soak_contract`, `s2_soak_transcript` | Quick/full; the flash tool and the shared-image smoke runner (`scripts/s2_smoke_test.py`) against fakes with no port or esptool, retained captures and adversarial fixtures through the parser, and the soak through the real firmware assembly against a synthetic driver that refuses anything but an accepted upload. The parser must read every record the real firmware prints (packet 11). No port is opened |
| Attended bench routes | `bench_route`, `bench_route_transcript`, `s2_bench_contract`, `s2_trial_transcript` | Quick/full; the `BenchRoute` routes through the real owner and `AxisSession` against synthetic drives, and the runner against a fake board, clock and operator. The parser reads every trial record the real `BenchRoute` prints, and the runner's `--unattended-waiver` refuses `enable` and asks no attendant question. Manufactured values, except `replayRouteBootR2c` and `replayRouteBootR4b`, which answer every upload of the 2026-11-09 home and mode3 records with the captured reply and compare the printed lines with the captured ones; `communication_bench`, `discovery_bench` and `pdo_watch` replay that day's `K`, `H`, `D` and `P` captures the same way. A replay pins the interpretation of those exact values; it is no new device evidence. The routes' runs on two specimens are bench evidence in the [attended](reports/2026-11-03_attended_bench.md), [unattended](reports/2026-11-03_unattended_bench.md) and [2026-11-09](reports/2026-11-09_bench_r4.md) reports, not these tests |
| Stack frames and call chains | `python scripts/check_stack.py --strict` in the `idf` CI job; `stack_check_contract` | Explicit SDK gate; per-frame and per-chain limits on S2 and S3 at `-Os` and `-Og` with the pinned compiler, unknown edges reported, and context sizes in the report's `context_sizes`. Compile-time lower bounds, not a measured task high-water mark (packet 8) |
| Explicit settings through the real owner | `settings_owner`, existing `settings_write`, `driver_settings` and `can_bus_owner` | Quick/full; scripted dictionary, literal bytes, partial effects, epochs and old-target admission. No device evidence |
| Conditional native halt/quick-stop lifecycle | `stop_action` plus `can_bus_owner`, `can_bus_recovery`, `sdo_transfer` and `sdo_reconciliation` | Quick/full; exact command bytes, fresh reports, interruption, pressure, ambiguous channels, time and uncertain effects, and the measured `000B` release replayed (`quickStopReleaseIsTheMeasuredTerminal`: a quick stop completes on it, a `HALT` does not). The fixed quick stop has not been retested on hardware; no physical stop qualification |
| Urgent quotas, cancellation, controller queue retirement and recovery | `can_bus_recovery`, existing `can_bus_owner` and `owner_preview` | Quick/full; fake controller queues, stale epochs, first-terminal retention, two-node receive and finite recovery; no validated drive stop or physical retirement claim |
| Passive communication health, exact saturating counters/reset and bounded diagnostics | `can_bus_health`, existing owner/recovery suites and native preview | Quick/full; logical-service counting, independent TX evidence, current PDO provenance, cached getters, heartbeat policy and full trace under ordinary/urgent work; no retry or drive-state policy |
| Shared native application and bounded platform I/O | `probe_app`, `native_probe --fake`, existing `units_preview`; isolated copied applications in `package_contract` | Quick/full for native paths; full adds relocated installed-core builds without checkout includes, siblings, SDKs, Python or network. Fake wire values are not hardware qualification. |
| Pinned TWAI policy and actual SDK shim with host fakes | `twai_adapter`, `twai_driver` | Quick/full; one RX owner, queued boot-up, exact TX evidence, deadlines, retirement, loss and SDK failure paths; fake only |
| TWAI SDK interface / target ABI | `python scripts/check_twai_sdk.py --idf-path "path to esp-idf" --idf-python "path to IDF Python"` | Explicit SDK gate; exact IDF 5.5.5 source hashes, S3 C++11 static archive and retained logs; no firmware linked |
| Shared firmware startup, S3 USB and S2 UART/GPIO platform contracts and build-checker failures | `firmware_probe`, `esp_probe_platform`, `esp_s2_probe_platform`, `firmware_build_contract` | Quick/full; clock boundaries/faults, zero startup traffic, target pin policy, silent-input ordering and failures, owner invalidation on every closing path, the smoke hook, bounded pressure, routing and retirement using host fakes. The S2 test drives the real UART0 platform and `espSilentPin()` against SDK fakes |
| Arduino firmware compile | `python scripts/check_firmware.py --target arduino` | Explicit SDK gate and Arduino CI job; PlatformIO 6.1.19, pioarduino 55.03.311 / Arduino 3.3.11 / IDF 5.5.5; S3 fixture ELF/BIN, `xtensa-esp32s3-elf-` compiler and the exact checkout compiler inputs (inventory, core, `examples/probe_cli/main.cpp`) |
| Arduino S2 firmware compile | `python scripts/check_firmware.py --target arduino-s2` | **Blocked**, not part of `all` or CI: the pinned `framework-arduinoespressif32-libs` esp32s2 `sdkconfig` enables the UART0 console (`CONFIG_ESP_CONSOLE_UART`), which the S2 platform's exclusive-UART0 guard refuses. Reports status `blocked` on that guard and fails otherwise ([firmware guide](firmware_examples.md)) |
| Native-IDF standalone firmware, S3 | `python scripts/check_firmware.py --target idf` | Explicit SDK gate and IDF CI job; original source hashes, exact IDF 5.5.5, S3 ELF/BIN, target, compiler, resolved `sdkconfig` (8 MB flash, console none, no PSRAM component) and exact checkout compiler inputs |
| Native-IDF standalone firmware, S2 fixture | `python scripts/check_firmware.py --target idf-s2` | Explicit SDK gate and IDF CI job; the same shared application for esp32s2: 4 MB DIO, UART0 platform, no smoke or bench source |
| Shared S2 OPTAK image with the smoke hook | `python scripts/check_firmware.py --target idf-s2-optak` | Explicit SDK gate and IDF CI job; every board and smoke value of `sdkconfig.optak_s2*`, `FirmwareSmoke.cpp` compiled, no bench source. A linked image is not a smoke result |
| Special S2 bench image | `python scripts/check_firmware.py --target s2` | Explicit SDK gate and IDF CI job; esp32s2 target, compiler and resolved `sdkconfig`, bench entry and raw collector, exact checkout inputs outside `examples/s2_bench` |
| Isolated portable IDF component | `python scripts/check_firmware.py --target component` | Separate explicit SDK/CI gate; exactly the staged core and consumer outside the checkout, no checkout file, esp32s3 target and compiler, C++ exceptions/RTTI off, real link, no owner/stack/reference dependencies |
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
TWAI archive build cannot replace any of the real compile/link gates
(`arduino`, `idf`, `idf-s2`, `idf-s2-optak`, `s2`, `component`; `all` runs every
one of them, `arduino-s2` is reported blocked). The checker inspects actual
compilation databases: every gate requires its shared sources and refuses any
other checkout file it did not expect, and each SDK gate checks the target, the
cross compiler and reviewed resolved `sdkconfig` values, where an option required
to be disabled must appear as `is not set` (only a reviewed absence, such as
`CONFIG_SPIRAM` in S3 builds, may be missing). No gate flashes, monitors or opens
a controller/serial port.

Both SDK commands share source/version/virtualenv validation. The TWAI-only gate
checks its own source manifest; firmware additionally checks the USB/time/task
manifest. The selected virtualenv executable keeps its lexical path, and SDK
constraint checks cannot be disabled. `firmware_build_contract` exercises both
command boundaries and failed-build evidence retention without installing an SDK.
