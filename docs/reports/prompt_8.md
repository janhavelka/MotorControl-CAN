# Prompt 8 verification, CI and documentation gates

Date: 2026-10-29. Local gates complete; remote delivery verification pending.
Started from `95d7b7ad46c4fe8dbdd4b333de0cee6b6bb3c946`, clean and 0 ahead /
0 behind after fetch, on `promptset/motorcontrol-can` with its existing origin
upstream. Prompt 7 is complete. This stage changes developer gates, not drive
support, firmware or hardware qualification.

## Implemented gates and evidence

The maintained [verification table](../verification.md) owns current versus
planned checks. `scripts/verify.py --mode quick|full` runs structured commands
with bounded timeouts, exact tool/version logging, separate passed/failed/
unavailable outcomes and nonzero failure exits. Every run has a new directory;
`summary.json` points to its commands, output logs, revision, dirty paths and
retained artifacts. Earlier evidence is not deleted. Python optimization,
empty/skipped/disabled tests and failure-bypass CTest properties are rejected.
Both console output and CTest's LastTest.log are checked, since successful
CTest output alone hides skipped Python subtests.

Quick configures/builds the actual core/proofs, validates generated version and
real CTest command identity, then executes all quick native/Python/planning
contracts. Full additionally executes documentation failures, the independent
export/source/relocated-install consumers, strict Doxygen and API extraction.
Package commands and their archive, source/install trees, logs and summary are
retained separately. C++11, C++17/no-RTTI and header-independent compilation
reuse prompt 7's actual consumers. No verifier flags are exported to applications.

`scripts/check_repository.py` checks package/doc inputs, manifest shape/identity/
allowlist, version freshness and explicit registrations. A configured CTest
JSON inventory checks exact native executable and Python interpreter/script/
argument paths, duplicates, disabled/skip/pass-regex overrides and mode labels.
Only package and documentation contracts may carry the full label; relabeling a
quick test cannot hide it. All 14 current CTests are explicitly registered.
The existing planning checker still owns maintained links and original hashes.
It now excludes only declared root development-output directories, so retained
package copies cannot masquerade as maintained repository Markdown; nested
`docs/build` and retained reports stay checked.

`Doxyfile` is tested with exact Doxygen 1.13.2. Warnings for public members,
parameters, enums, incomplete docs and unresolved references fail. All five
installed headers are inputs; XML verification checks their extraction, 38
members and the five current free functions. The strict settings are reused by
real valid/missing-parameter/undocumented-member/unresolved-reference fixtures.
A narrow Markdown filter renders repository navigation as source links because
Doxygen otherwise treats excluded research Markdown as unresolved API refs.
It preserves explicit @ref and C++ input; the ordinary link checker still
validates source links. Generated header comments remain generator-owned.

CI preserves automatic push/PR and manual triggers. Windows 2025 runs quick
GCC; Ubuntu 24.04 runs full GCC 13 and Clang 18 with required ASan/UBSan.
Official action revisions and Doxygen source commit
`26342b775ea25e6fefb53220926b20702c56fcb3` (Release_1_13_2) are pinned.
Source-build version output may append that clean Git identity; newer versions,
other commits and dirty suffixes fail. No future firmware/catalogue job exists.
Artifacts are uploaded even on failure. The installed core remains unchanged.

## Fresh source audit and review

All inspected siblings remained clean at these exact revisions, unchanged from
the previous applicable audit; no dirty-file behavior was treated as HEAD.

| Checkout | Revision | Current evidence read |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | `scripts/verify.py`, `scripts/check_repository.py`, `test/repository_check_test.py`, root CMake, installed consumers and `.github/workflows/verify.yml`: command inventories, logs, package isolation and impact policy |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | `Doxyfile`, `.clang-format`, `.github/workflows/ci.yml`, `tools/check_cli_contract.py`, `check_idf_example_contract.py`, `check_core_timing_guard.py`; no dedicated failure suite was found |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `Doxyfile`, component/native boundary and package use; its C++17/sensor policy is not imposed on this core |
| BME280 | `d538f2e203ee26d6ddbc86f86d2b0cf4c73a6e98` | `Doxyfile`, workflow and `tools/test_checker_contracts.py`, `test_check_package_contents.py`: independent expected rules and deliberate failures |
| MCP45HVX1 | `c6c7feefd79b9fc16ddc7b7786ad187e5185fde6` | `Doxyfile.api`, workflow and `tools/test_contract_tools.py`: strict public API practice and real checker entrypoints |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `config/native_suite_links.json`, `build_profiles.json`, `scripts/check_native_test_registration.py`, PlatformIO profiles and dormant RS consumer: explicit suite execution and dependency closure |

No sibling suites were rerun or files changed. CAN reuses RS names/registration
and independently derived failure fixtures. It adds bounded subprocesses and
unavailable summaries rather than copying older verifier gaps. I2C's broad
exclusions, EXTRACT_ALL settings, firmware jobs and two-space style are not
universal conventions. Four-space LLVM-derived formatting matches the current
core/RS; formatting is an aid, not a falsely passing unavailable tool gate.

Three bounded agents used the inherited model/effort for checker/failure tests,
documentation/source review and independent verifier/package/workflow review.
The lead owns CMake, verifier, workflow, shared docs and Git. Independent review
found missing timeout/retention in the old package harness, optimization/skip
guards, a possible full-label bypass and Doxygen's namespace XML layout.
All were corrected and rechecked. Original source also confirmed that Doxygen's
--version includes a Git suffix when built from its pinned source.

## Local verification and limitations

Tools: Windows x64 GCC/G++ 15.1.0, Python 3.12.10, CMake 4.0.1, Ninja 1.12.1,
Doxygen 1.13.2. Clang and WSL are absent. An actual GCC sanitizer link probe
failed for missing libasan/libubsan; requesting Clang or sanitizers returns
unavailable/nonzero with retained diagnostics, not a passing skip.

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir .build/p8-final-quick` | PASS: actual CTest inventory and 12 quick contracts |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p8-final-full` | PASS: 13 CTests plus separately retained package contract, strict docs and XML API inventory |
| Checker, verifier and documentation regressions | PASS: 15 repository methods, 9 verifier methods and 7 documentation methods, including useful real failure diagnostics |
| Existing version and planning/ledger regressions | PASS: 6 version methods and 74 planning/ledger methods |
| `doxygen Doxyfile` | PASS: zero warnings, complete installed API extraction |
| Required negatives | Stale output, missing registration, invalid/missing package inputs, unavailable commands, timeout, optimization, empty extraction and broken public docs fail nonzero in disposable copies; production files remain correct |
| Planning integrity and Git checks | PASS locally; INDEX before commit and HEAD after commit verify all 134 original artifact hashes; whitespace checks pass |

Early integrated failures were retained: a development-output Markdown scan and
file-only XML count failed until corrected. Final runs use the corrected checks.
Local snapshots record dirty paths because implementation was uncommitted when
tested; delivery CI will verify the actual pushed commit. The exact remote run
and terminal result will be recorded after synchronization, before handoff.

README/index, native/verification guides, contributor/security policy, source
map, backlog, coverage, roadmap, progress and CHANGELOG accompany the tooling.
No verified private reporting channel exists yet; SECURITY records that release
gap without inventing an address. No source API, profile, hardware workflow or
new runtime abstraction was added. Prompts 12 and 27–28 retain their generation
and firmware responsibilities. Prompt 9 is next after this stage's CI closure.
