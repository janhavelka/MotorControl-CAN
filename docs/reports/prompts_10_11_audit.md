# Independent audit of prompts 10 and 11

## Scope and result

Re-read both original prompts in full and audited actual source, tests, callers
and changes from `1c5de5d` through `082af369b47301e8a4c9da85533d3a7e61258d02`.
The initial tree/index were clean. Fresh fetch confirmed that commit on
`promptset/motorcontrol-can` and its configured upstream, with 0 ahead/behind.
The original promptset branch/base remain unchanged.

**No production correctness defect or unfulfilled feature requirement was
confirmed.** Both prompts retain Complete status. Two documentation corrections
were verified against implementation and made:

1. `experiments/api_ownership/Operations.h` still restricted the retained proof
   to exact native I32 or turns/degrees. Its actual `convertTarget` delegates
   absolute unwrapped INTEGER requests to production `preparePosition`, including
   its unit/rounding/error policy, then checks the synthetic I32 wire range.
   The comment now describes that actual boundary.
2. Prompt 11's ADS1115 source row named `readADC`. The current implementation is
   `ADS1115::readLatestRaw`; it publishes the conversion only after a successful
   `readRegister16`. The source citation is corrected.

Executable production code, public declarations, immutable references, package
contents and workflow configuration are unchanged. No new abstraction, test
framework, dependency, driver or hardware claim was added.

## Independent review and actual evidence

Three bounded parallel read-only tasks reviewed the actual code/diff. Axis
arithmetic/reference review used fresh-context `gpt-6-astra`, high effort;
protocol review and requirements/package review inherited the available
model/effort. Lead independently checked their findings, source citations,
public contracts, mutators and test/build integration, and owns fixes and Git.
Workers used disjoint disposable probe directories, with no tracked, Git,
sibling or hardware mutations.

| Requirement group | Inspected implementation and observable evidence |
| --- | --- |
| 10: complete pure API and configuration | `Axis.h` / all `Axis.cpp`: validation, candidate publication, independent command/feedback/gear/travel/origins, optional geometry and generic endpoint identity. Native/frame restrictions do not import ESS RPM or CANopen node assumptions. |
| 10: exact targets and limits | Production Units mixed arithmetic and Axis endpoint checks; `axis_test.cpp` covers sign/extreme/overflow, adjacent values beyond 2^53, requested/effective inclusive limits, origin-before-absolute-rounding, relative displacement rounding and explicit float tolerance. |
| 10: reference/generation/time | Config/reference epochs, exhaustion latch, POSITION versus COORDINATES loss, optional/required witnesses and exact basis; frozen observation ages and admission revalidation. Existing tests include stale/future/backward time, changed evidence and unchanged failed outputs. |
| 10: path/contrasts/ordinary caller | Actual wrapped-angle direction/tie primitives, servo without full-step geometry, independent scales/subaxis and binary32; compiled offline preview, installed/native consumer and proof using production arithmetic. No finite motion success stub. |
| 11: common frames | `Frame.h/.cpp`, exhaustive field/flag tests: 11/29-bit IDs, data/RTR, requested DLC versus zero RTR payload, invalid identifiers/lengths/BRS/ESI and explicit FD capability failure. Source-backed VESC/ODrive shapes remain valid common frames and fail a mismatched SDO route. |
| 11: checked selected-engine boundary | `Sdo.h/.cpp`, pinned raw CANopenNode and actual engine request tests: explicit channel/node/COB-IDs/object/direction/width, legal expedited/unsized/segmented forms, canonical TX versus opaque RX tails, full unsigned abort evidence. No second encoder/session engine. |
| 11: atomic words and correlation | Literal and exhaustive command tests, all selected widths/signs, native abort extremes, wrong object/phase/route/length/flags, short spans, prefix/full-span overlap and output sentinels. Borrowing is call-only; results own their bytes. |
| Both: delivery and scope | Actual CMake/install/export lists, test registration/checker/verifier/workflow and eight independent headers; C++11, C++17/no-RTTI, source/exported/relocated consumers, missing-dependency negative, strict docs and original-byte integrity. Research fixtures/stack/SDK/catalogue remain outside installed dependencies. |

Independent disposable probes are additional checks, not alternate production
implementations or installed support:

- `.build/p10-11-audit-axis/`: 30,000 seeded wrapped targets compared against
  Python Fraction, including rational periods, polarities, all directions/ties
  and large origins/bases; all matched, 29,344 accepted and the remainder rejected
  endpoint overflow. Another 30,000 radians cases used 90-digit Decimal: 17,026
  accepted results had valid reported approximation/total error bounds. The
  latter checks accepted-result bounds, not maximal acceptance of approximations.
- `.build/p10-11-audit-codecs/`: 4,096 request/reply command/direction/width
  combinations and all 65,536 U16/I16 words passed independent expected values;
  failed outputs stayed unchanged. Pinned command/TX/RX source was reopened.
- `.build/p10-11-audit-coverage/`: independent isolated package check and strict
  live-RS comparison passed. Lead also rebuilt the compatible RS vectors under
  `.build/p10-11-audit-rs/`: exact large positions, motor/load/geared/linear
  conversions, extreme-origin half ties and negative-polarity wrapped paths.
  RS's native -70000 RPM rejection remains an intentional CAN difference.

## Fresh source and evidence audit

All checkouts below are clean and unchanged from the preceding stage records:

| Source | Revision | Reopened implementation/caller/test seam |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | `Axis.h/.cpp`, `Units.h/.cpp`, finite ESS Position/Velocity callers and axis boundary tests; protocol Codec/Access implementation and actual read callers/tests. CAN deliberately requires a relative witness, preserves independent origins and keeps native rate wider than int16 RPM. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | `src/ADS1115.cpp::readLatestRaw`, actual owner/basic tests and packaging; atomic conversion publication, separate bind/apply lifecycle. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `src/INA228.cpp` staged calibration/sample publication and `_signExtend20/_signExtend40` with width tests/callers; no sensor retry/error lifecycle imported. |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | `_fitsRange`, transfer caller and range tests: compare remaining capacity without overflowing endpoint addition. |
| PCA9555 | `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | CommandTable/validRegister, `_readRegs`, invalid-register/partial-output tests: reject reserved command bits rather than masking. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | MotorControl contract, RS485 motor module and host motor tests: qualified exact target, independent readiness/position observation ages, uncertain movement invalidation and reference exhaustion; read-only, no CAN integration claimed. |

Reopened original EN86 pages 3/11/15/18 and original EDS; rendered EN86 pages
11/15 and the protocol review's page 13 were inspected. The 1000-line description,
2201 subdivision, 607A command, 6064 observation and 2302 counter-clear edge do
not establish a shared scale or arbitrary host origin write. Existing contradictory
subdivision ranges and unknown firmware applicability remain intact. Original
6041 U16 and 6099/609A U32 widths agree with the host-only vectors. No device
claim or ledger correction was needed; no physical measurements occurred.

Primary generic mechanics were checked against the pinned CANopenNode files,
including `CO_SDOserver.h:109-159` and actual `CO_SDOclient.c`. A fresh external
Parker manual page fetch timed out and direct download returned HTTP 403;
its earlier rendered-page review is recorded in the prior report, but
this audit does not claim a fresh successful external page read. The available
raw selected-stack source establishes the inspected layouts/TX/RX behavior.
No dependent feature was guessed or introduced because of that retrieval limit.

## Integrated verification and handoff

Tools: Python 3.12.10, GCC/G++ 15.1.0, CMake/CTest 4.0.1, Ninja 1.12.1 and
Doxygen 1.13.2. Local Clang/Linux sanitizer support remains unavailable.

| Command/check | Fresh result |
| --- | --- |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/prompts-10-11-audit-full` | PASS: 20/20 non-package CTests, separate package contract and strict installed documentation/API extraction. Run `20261029T125356499787Z-50704-j7nxmuf5`; retained command/tool/status logs and summaries under that directory. |
| `doxygen experiments/api_ownership/Doxyfile` | PASS: zero warnings, including corrected proof comment. |
| `python test/package_test.py --compiler g++ --work-dir .build/p10-11-audit-coverage/package` | PASS independently: exact 25 exported files, eight headers, source/install/export, C++11/C++17/no-RTTI, missing dependency and IDF registration mock. |
| Strict live-RS vectors, independently compiled probes, axis test and offline preview | PASS, with the independent scopes above; no unchanged sibling suites rerun. |

Planning/original Git-byte integrity passed with HEAD and INDEX (134 retained
artifacts, all ledger accounting and 1603 local links); staged/working diff
checks and both strict Doxygen commands passed. HEAD is rechecked after commit
before handoff. This audit
changes documentation only; no remote dispatch or repeated milestone wait is
required by the root/parent impact policy. The already-completed implementation
[CI run for 082af36](https://github.com/janhavelka/MotorControl-CAN/actions/runs/40875424022)
passed Windows quick plus GCC/Clang ASan/UBSan full gates on the unchanged
executable code. That result does not claim to verify a later documentation
commit; automatic push triggers remain in force. Final commit/upstream state
and any newer observed CI state are reported at handoff.

No owned functional gap remains and no cleanup is deferred. Prompt 12 remains
next; production sessions, ambiguity and event codecs keep their assigned later
owners. Device permissions, matched firmware and physical qualification remain
separate limitations. Original references and retained evidence are preserved.
