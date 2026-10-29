# Independent audit of prompts 7–9

Date: 2026-10-29. Audited clean commit
`33791450c595c329cfbcb1172d43c1a838e9ea6d` on `promptset/motorcontrol-can`.
Fresh fetch showed 0 ahead / 0 behind its existing
`origin/promptset/motorcontrol-can` upstream. This audits the original prompts
7, 8 and 9 in full; it does not execute the selected IDE prompt 11 or advance
axis, codec, firmware or hardware scope.

Three independent agents inspected package/version, verifier/documentation and
arithmetic responsibilities in parallel using the inherited runtime model/effort.
The lead reread the prompts, checked actual implementation/diffs and integrated
confirmed findings. Workers used disjoint test/output paths and no Git or sibling
mutations. Stage summaries were not substituted for source/test evidence.

## Findings and corrections

| Scope | Confirmed finding | Smallest correction and verification |
| --- | --- | --- |
| 7 package documentation | Guide still described five headers/two sources and omitted the newly exported units guide; code/manifest actually contain six headers/three sources. Its full CTest command also needs Doxygen. | Describe actual portable source/header inventory without fragile counts, list both exported guides and the real full-test prerequisite. Remove obsolete proof-CMake comment denying an installed core. Existing independent package inventory/consumers pass. |
| 8 quick evidence | Quick success had no explicit documentation/package omission record, contrary to DOCUMENTATION's summary requirement. | Add `omitted_gates` with package/docs in quick and an empty list in full; test both retained summaries. No new gate framework. |
| 8 tool failure | Empty/whitespace successful `--version` output raised IndexError and left retained status `running`. | Reject empty version text with `VerificationFailure`, log location, nonzero exit and final failed summary; regression calls the real main path with isolated subprocess results. |
| 8 command table | Direct repository-checker row claimed link checking although the independent planning checker owns links. | Name maintained links under the command that actually checks them. Both still run in quick/full. |
| 9 numeric documentation | Approximate API promised unchanged floating environment; ordinary FP arithmetic can raise exception flags. | Promise unchanged rounding mode and document ordinary exception flags. No new fenv preservation behavior or arithmetic refactor. |

No functional arithmetic, Status, source/install/export or version-generation
defect was found. All selected names, C++11 constexpr rules, static strings,
raw U32 abort separation, exact integer paths, signed extremes, independent
scales, rounding/ties, explicit approximation and atomic failure contracts were
checked against actual declarations, implementations and callers/tests.

## Requirements and scope review

| Original prompt | Current evidence checked |
| --- | --- |
| 7 | CMake identity/imported target, private warnings, C++11 core, C++17/no-RTTI consumers, version single source/check mode, exact export/install inventories, relocation, independent headers, missing dependency failure and optional SDK-free component registration. Package includes software/license and excludes research, fixtures, stack and sibling dependencies. |
| 8 | Actual quick/full commands, registered CTest executable/interpreter inventory, stale/missing/invalid negative fixtures, nonzero failures/timeouts, retained command/tool/status artifacts, strict pinned Doxygen including an independent completely undocumented function failure, current/planned table and honest unavailable tools. Workflow pins/triggers and sanitizer requirements remain intact. |
| 9 | Strict parser capacity/grammar/reduction, native integers around 2^53 and signed extremes, cross-cancellation/overflow, every integer policy/negative tie, independent command/feedback/load scales, binary32 midpoint/tolerance/error bounds, explicit radians approximation and proof migration. Axis/reference policy and native-drive semantics retain their original later owners. |

## Fresh source evidence

All refreshed checkouts were clean and unchanged at these revisions. Actual
files/callers/tests were read; no unchanged sibling suite was run.

| Checkout | Revision | Seam rechecked |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | Root CMake, packaging/consumers, version tooling, verifier/checker/workflow; Status/Units/Axis and UnitFactors, tests, units_preview and console callers |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | CMake/manifest/status/checker and staged typed publication patterns |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | C++17 package boundary, Status, finite/range-checked numeric assignment and callers/tests |
| BME280 | `d538f2e203ee26d6ddbc86f86d2b0cf4c73a6e98` | Checker failure contracts, package checks and Doxygen/workflow practice |
| MCP45HVX1 | `c6c7feefd79b9fc16ddc7b7786ad187e5185fde6` | Contract-tool failure tests, Doxyfile.api and CI |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | Subtract-first range checks, huge-size tests and typed staged reads; reset/unspecified-output counterexamples retained |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Native registration/dependency configuration, exact target/scale identity and read-only RS module consumer |

CAN deliberately retains RS call-result names and exact common intent; I2C
factories, device overflow categories and lifecycle policies are not copied.
No Lichuan object constant, permission, firmware claim or reference byte changed.
Its I32/U32 velocity contradiction and millisecond-ramp distinction remain
visible in the existing reviewed evidence. No hardware activity occurred.

## Verification record

The integrated delivery is checked with the ordinary quick/full verifier, strict
Doxygen, original-byte integrity and whitespace gates. Local tools: Windows
GCC 15.1, Python 3.12.10, CMake 4.0.1, Ninja 1.12.1, Doxygen 1.13.2. Local Clang
and sanitizer runtimes remain unavailable; supported Linux jobs supply those
checks on the exact delivery commit. No unavailable local check is counted as
passing. Final commit/CI URL and terminal result are recorded in the handoff.

| Focused independent check | Result / retained evidence |
| --- | --- |
| Integrated quick verification | PASS: 13 registered CTests; `.build/audit-7-9-quick/summary.json` explicitly records package/docs omissions |
| Integrated full verification | PASS: 14 CTests plus independent retained package consumers and strict Doxygen; `.build/audit-7-9-full/summary.json`, 6 headers / 88 members / 14 distinct free-function names |
| Original-byte integrity and whitespace | PASS: working/index/committed original artifacts are verified with the planning checker; 134 unchanged hashes and `git diff --check` |
| Version contracts | PASS: 6 methods |
| Isolated source/install/export package | PASS, including all headers, C++11/C++17/no RTTI, relocated install, missing Status.h failure and component mock; `.build/audit7/package/summary.json` |
| Repository failure contracts | PASS: 15 methods |
| Verifier failure contracts after fixes | PASS: 11 methods, including omission fields and empty/whitespace tool versions |
| Documentation failure contracts | PASS: 7 methods plus independent completely undocumented free-function failure |
| Independent exact numeric oracle | PASS: 40,042 directed doubles, 40,042 binary32/error bounds, 75,210 integer policies, 15,042 exact float tolerances and 10,000 dimensional conversions (180,336 checks); `.build/audit9` |
| Separate live RS comparison | PASS: 5 parser, 6 atomic malformed-parser and 4 compatible dimensional vectors, strict C++11 executable; `.build/audit9` |

Public runtime declarations/algorithms, package dependencies and API scope remain
unchanged. Only verifier reporting/error handling and matching regressions change
behavior. Documentation sits beside the affected work; old reports/reference
artifacts are preserved. Prompts 7–9 retain Complete status; prompt 10 remains
the next implementation stage.
