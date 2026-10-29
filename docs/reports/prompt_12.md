# Prompt 12 - Reviewed object generation

## Scope and delivery state

Implemented deterministic reviewed-ledger generation, compact installed native
declarations/access policy, optional descriptive strings and complete readable
coverage. The real freshness check is active in local quick/full and their
existing CI workflow. No semantic read, setting, action, workflow, raw-access
route, default PDO template or operational drive profile is implemented here.

The stage is **Complete**: local quick/full verification, independent review,
package/header/Doxygen checks and exact implementation-commit Windows/GCC/Clang
sanitizer CI passed. Prompt 13 is next. Device/firmware qualification remains separate.

Started clean on `promptset/motorcontrol-can` at
`1356cffc0e79d17fd78725ff8164078468a022b7`, configured upstream
`origin/promptset/motorcontrol-can`; fresh fetch showed 0 ahead / 0 behind.
Original execution identity/base in [PROGRESS](../prompts/PROGRESS.md) continues.
Root/parent policy, original prompt and applicable shared contracts, actual
workflow/verifier and prerequisite artifacts were inspected before edits.

## Single source and conservative generation

The [ledger](../reference/lichuan_objects.json) retains schema 2 and all original
claims. Each entry now has mandatory `review.generation_type`:

- `resolved`: 194 named compact descriptors with established native storage
  type and conservative policy. This does not qualify its other semantics.
- `withheld`: `6060`, `6061`, `6098`, `60FF` have known byte widths but disputed
  complete signed types. Original EDS/manual alternatives and future owners
  remain visible; no typed declaration or alternate metadata descriptor exists.
- `excluded`: reserved `2400:09/0A/0B/10` retain reasons and raw RW/PDO claims,
  with no generated descriptor or permission.

The full source validator guards each disposition against existing reviewed
policy and source accounting. `607A` retains reviewed I32 normalization of the
manual's malformed I3C spelling; an unrelated conflict does not erase established
representation. Unknown firmware/effect facts remain unknown.

[The generator](../../scripts/generate_lichuan_objects.py) owns these products:

| Product | Responsibility |
| --- | --- |
| [Types.h](../../include/MotorControlCAN/profiles/lichuan_c/Types.h) | Fixed C++11 documentary types, reviewed SDO direction, read effect, write restriction, independent PDO policy, owner and qualification mask/scope |
| [Objects.h](../../include/MotorControlCAN/profiles/lichuan_c/Objects.h) | 194 named constexpr constants in `MotorControlCAN::LICHUAN_C`; no strings, lookup dictionary or admission helpers |
| [Catalogue.h](../../include/MotorControlCAN/profiles/lichuan_c/Catalogue.h) | Optional static-lifetime descriptions, source IDs/pages/EDS spans, original limits/defaults and expressions, units/effects/conflicts/applicability; only resolved descriptor rows |
| [Generated coverage](../reference/lichuan_generated.md) | All 99 indexes, 202 typed entries, 53 manual functions, four withheld/four excluded, 24 conflicts, 27 gaps, A01, source/model/family/firmware policies and support gates; descriptor presence separate from semantic coverage |

Every read remains `UNQUALIFIED`; RO does not establish non-changing polling.
Writes are forbidden or qualification-required, never enabled by a descriptor.
PDO eligibility remains forbidden/unqualified separately from SDO direction.
The original RPDO1 words `60400010`, `60FF0020`, `60600008` are documented from
source metadata without granting SDO reads, active mapping or an action route.
Lichuan `2300` save/reset has no global index-only dispatcher and cannot become
N5 NanoJ control. All semantic implemented/host-tested/hardware-qualified ledger
flags remain false; generator/descriptor tests do not change those flags.

## Determinism, validation and publication

The command uses only Python's standard library and calls full
`validate_ledger` before rendering. This checks original hashes/fields, source
references, schema/type/access/effect/disposition, scope/gates, bounds,
uniqueness and complete accounting. Small synthetic projections additionally
exercise the declaration boundary without replacing CLI source validation.

All output is rendered in memory. Publication stages every changed candidate
before replacing files; a replacement error restores previous bytes or removes
new files. Tests cover both existing/new outputs failing at the second replace.
This is error rollback, not a process-crash or filesystem transaction guarantee
across multiple files. Successful no-op reruns preserve bytes and mtimes.
`--check` compares exact bytes and never writes, including when stale or absent.
Narrow LF attributes cover the three generated headers; original/reference
byte-protection rules are preserved. Vendor material stays excluded from core
distribution and keeps its separate licensing.

Developer commands and ownership are in the [dictionary guide](../reference/lichuan_objects.md)
and [CONTRIBUTING](../../CONTRIBUTING.md). Installed/exported source consumers
need no Python, ledger, original documents, generator, SDK or stack. Headers are
explicitly installed and independently compiled; the common umbrella includes
neither profile header nor optional strings. A consumer remains buildable with
the descriptive catalogue physically removed. Version generation is independent.

## Fresh source review

All inspected sources are clean and unchanged from preceding stage revisions:

| Checkout | Revision | Actual seam, caller/test and intentional difference |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | `docs/reference/ess_rs_registers.json`, `scripts/generate_ess_registers.py`, generated Types/Registers/private Access, `test/generator_test.py`/registers tests and Codec/DriverSettings users. Reuse deterministic source generation and compact-versus-string separation. CAN does not copy RS raw-read/write admission masks; all effect gates remain explicit. Actual verify.py registers source freshness. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Register-mask/enum static checks, runtime enum validators, basic tests and diagnostic callers. Source type/access does not substitute for supported values or operation effects. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `parseDeviceIdentity`, job/readDeviceIdentity callers and identity/parser/reset tests distinguish manufacturer, die and revision; no inferred common firmware binding or sensor retry policy. |
| PCA9555 | `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | CommandTable/validRegister, `_readRegs`/`_writeRegs` and bus-silent invalid-register tests reject reserved fields without masking. |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | Variant table/`_selectVariant`, identity-read caller and capability/no-ID tests separate model identification from usable support. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `MotorControl.h`, RS485 module start/axis snapshot and host motor tests bind exact configuration/reference identity. No framework dependency, RTU address/RPM assumption or FieldCore change introduced. |

Independent reviewers reopened raw EDS and rendered EN86 pages 13/15/16/18 and
ZH86 pages 18/19. `1009/100A` are U16 codes; `2400:00` is U16 while other count
rows are U8; mapping counts can be RW with qualification. `1018` has only
children `00..03` despite count HighLimit 4/default 3. No `1018:04` was invented.
Reserved output aliases remain excluded despite conflicting manual mappings.
Save/clear raw PDO flags grant no action. EN86 `60FF` U32 versus EDS I32 and
the one-byte signedness disagreements remain unresolved. All six original source
hashes match. N5 v3.3.0/FIR-v2213 p283 identifies its separate `2300` NanoJ Control
U32 meaning. No primary claim needed correction or physical measurement.

## Delegation, independent review and simplicity

One implementer owned generator, ledger/validator and generated-file writes;
lead owns public decisions, build/checker/verifier integration and Git. Source
row/accounting review inherited the available model/effort. A fresh-context
`gpt-6-astra`, high-effort reviewer checked schema/permissions/publication.
Workers did not change Git, siblings, hardware or shared integration paths.

An independent parser compared every one of the 194 emitted descriptors to raw
EDS and reviewed ledger: native width/sign/access, read/write/PDO policy, owner,
every gate bit and unbound scope agree. Both optional/compact surfaces omit the
eight withheld/excluded entries. All index/manual/conflict/anomaly coverage is
retained. Stronger review found one omission: generated gate model lists and
family/binding policy were missing. The generator now preserves them explicitly,
with a regression for the 86C Q06 versus 57C Q07 distinction.

The implementation is one direct Python generator and three fixed-value
headers, with no runtime registry, dispatch engine or duplicate numeric codec.
Descriptive text is opt-in. All new abstractions serve current declaration,
policy or source-traceability requirements; no deferred cleanup remains.

## Verification

Tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, MinGW GCC/G++ 15.1.0,
Doxygen 1.13.2. Local Clang and Linux sanitizer runtimes remain unavailable;
the configured milestone CI supplies GCC 13/Clang 18 ASan/UBSan.

| Check | Actual result / retained evidence |
| --- | --- |
| `python scripts/check_lichuan_ledger_test.py` | PASS 55 ledger tests |
| `python scripts/generate_lichuan_objects_test.py` | PASS 24 dedicated tests: synthetic invalid rows, duplicates/bounds/evidence/applicability, bool widths/signs, reserved/pending policy, source validation, deterministic reruns, exact/CRLF freshness, untouched failures, replacement rollback and model-scope preservation |
| `cmake -S . -B .build/p12 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++` / build / focused CTest | PASS strict C++11; 5/5 objects/freshness/generator/repository/native-consumer tests |
| Deliberately stale Objects.h through the actual quick verifier | Expected nonzero at `objects`, with unchanged stale bytes; real generator restored canonical bytes. `.build/p12-stale-verifier/summary.json` and `negative-command.log` retain the failing command/status/log |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p12-full` | PASS 23/23 non-package CTests, separate package contract, strict Doxygen/API extraction. Run `20261029T143008207250Z-69052-owrtkyz2` retains command/tool/status logs |
| Full package contract | PASS exported source, 11 independent headers, C++11/C++17/no-RTTI, relocated install, missing public dependency failure, catalogue-free consumer and IDF registration mock; no firmware build claimed |

Final quick PASS (22/22), retained under
.build/p12-quick/20261029T143320627384Z-54356-ru9anmdw/.
Original-byte HEAD integrity PASS (134 retained artifacts, all source/ledger
accounting and 1629 links); working diff check PASS. Staged and committed original-byte integrity passed with INDEX and HEAD.
All four staged/committed generated blobs were also independently compared to
canonical generator bytes and matched. Exact CI/sync results follow below.
Required unavailable tools are not counted as
passing local sanitizer gates. No sibling suites or physical CAN tests ran.

## Handoff

A subsequent [independent audit](prompt_12_audit.md) records fresh source,
implementation and package review, confirmed generator/coverage corrections
and their verification. The original implementation results below retain
their exact revision scope.

Generated declarations are not implemented semantic functions or qualified
support. Remaining model/firmware/read-effect/write/PDO gates stay visible in
the ledger/generated coverage. Prompt 13 owns production SDO sessions; typed
reads/settings/actions and physical qualification keep their assigned owners.

## Commit, CI and synchronization

Implementation checkpoint: 6e982ae9599af2028451db7b577982740093b335.
Push and fresh fetch confirmed matching upstream HEAD, 0 ahead / 0 behind,
with a clean worktree; committed-byte integrity/freshness passed afterward.

[Required implementation CI run 40892982772](https://github.com/janhavelka/MotorControl-CAN/actions/runs/40892982772)
completed successfully on that full SHA: Quick GCC on Windows, Full GCC with
ASan/UBSan and Full Clang with ASan/UBSan all passed. The existing workflow
runs the active object freshness command and contracts through verify.py. Actual
run/head SHA/job results were read via the GitHub CLI because the connected
tool cannot inspect these push runs. No manual dispatch or trigger change.

This closure changes reporting/status only; generator, ledger, generated
products and executable source remain unchanged. Its exact final-commit CI and
fresh upstream comparison are verified before handoff and reported with the
resulting commit ID there.
