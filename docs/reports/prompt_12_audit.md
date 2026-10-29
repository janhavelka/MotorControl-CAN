# Prompt 12 independent audit

## Scope and starting state

Re-read [the complete original assignment](../prompts/12_object_generation.md)
and applicable shared contracts, then inspected the implementation diff
`1356cff..67e7222`, current generator/validator, generated products, tests,
package consumers, verifier and actual workflow. Started on the continuing
`promptset/motorcontrol-can` branch at
`67e72226f3860c4a6ad8edbc7383ce6166ab08b1`, clean and synchronized with
`origin/promptset/motorcontrol-can` after a fresh fetch (0 ahead / 0 behind).

Three independent read-only reviewers inspected generator/schema/edge cases,
original-source policy/accounting, and integration/package/requirements. They
inherited the available model and effort; no model override was requested.
The lead reproduced findings and owns all edits and Git operations.

## Confirmed findings and corrections

1. JSON string escaping was used as C++ literal escaping. A schema-valid
   supplementary Unicode character produced surrogate universal-character
   names that GCC rejected in `Catalogue.h`. The generator now encodes UTF-8
   bytes with fixed three-digit octal escapes, escaping quotes, backslashes and
   question marks. This preserves byte boundaries and prevents C++11 trigraph
   translation. Invalid isolated surrogates fail before publication. New tests
   cover exact independent literal expectations and unchanged outputs on this
   failure. Current ledger header bytes remain unchanged.
2. Generated entry evidence omitted `disposition`, although it retained
   `review.generation_type`. It now shows documentary and declaration
   dispositions separately for every row: a resolved native type can remain
   pending device qualification. A regression checks all 202 rows.
3. The RPDO1 explanation listed only the first two source mapping words while
   discussing WO `6040/6060`. Original EDS `1600sub1..3` contains
   `0x60400010`, `0x60ff0020`, `0x60600008`. The generator now lists all three,
   with an independently specified regression. This remains source metadata,
   without an approved PDO template or SDO read permission.

No access-widening, source-accounting, package dependency or publication defect
was found. All 194 descriptors and optional catalogue rows match independent
parsing of raw EDS and reviewed policy. All 99 indexes, 202 typed entries,
53 manual items and six original source hashes remain accounted for. Reserved
and disputed signed types remain excluded/withheld. Complete documentary
review and pending device qualification are distinct states; the generator
continues to reject unreviewed rows.

## Fresh source audit

All sibling worktrees are clean and unchanged from the implementation report:

| Checkout | Current revision | Inspected implementation and caller/test seam |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | Register ledger, generator validation/render/check, generated Types/Registers/private Access, DriverSettings callers, generator tests and freshness registration |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Register/enum checks, runtime profile validation and basic contract tests |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Atomic identity parser, manufacturer/die/revision separation and read/job callers |
| PCA9555 | `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | Reserved-register validation before read/write callbacks and invalid-register tests |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | Variant selection, identity-read caller and capability-dependent cache invalidation |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Read-only binding/configuration generations, exact axis snapshot, startMotor caller and host motor contracts |

CAN retains independent source/access/effect/PDO gates instead of RS raw access
masks or sensor lifecycle policy. RS uses the same JSON literal helper pattern;
the confirmed C++ escaping correction is intentionally local to CAN.

Original EDS sections and rendered EN86 physical PDF pages 13, 15-18 and ZH86
page 19 were rechecked independently. Versions `1009/100A` remain U16;
`2400:00` retains its distinct U16 count; `1018` has no fabricated subindex 04;
reserved aliases, action effects, mode/homing/velocity signedness and output
address conflicts retain their existing dispositions. No manufacturer claim,
model/firmware applicability or ledger permission needed correction. Source
review probes are in `.build/p12-audit-evidence/`.

## Verification

Fresh focused results:

- `python scripts/generate_lichuan_objects_test.py`: PASS, 28 tests.
- `python scripts/check_lichuan_ledger_test.py`: PASS, 55 tests.
- Repository checker and its 15 negative/registration tests: PASS.
- Independent isolated package contract: PASS, exported source, relocated
  install, all 11 headers, C++11/C++17/no-RTTI, catalogue-free consumer and
  missing-public-dependency rejection. Evidence:
  `.build/p12-audit-integration/package/summary.json`.
- The formerly failing full-ledger Unicode fixture now compiles with strict
  GCC C++11 and matches an independently hardcoded runtime byte vector,
  including supplementary Unicode, control/NUL, trigraph text, following
  digits, quotes and backslash. Evidence:
  `.build/p12-audit-generator/unicode/`.

`python scripts/verify.py --mode full --compiler gcc --build-dir .build/p12-audit-full`
passed 23/23 non-package CTests, the separate package contract and strict
Doxygen/API extraction. Retained run:
`20261029T145913534340Z-3988-xmdm5woc`. An earlier attempt failed only because
the progress link to this report was added before the report existed; the
missing file was created and the full run repeated without weakening checks.
That failed attempt is retained as `20261029T145801173464Z-85744-r6807tnd`.

`python scripts/verify.py --mode quick --compiler gcc --build-dir .build/p12-audit-quick`
passed 22/22 CTests, retained as `20261029T150119004154Z-86180-nzunpkpf`.
Planning integrity passed with all 134 retained artifact hashes and HEAD Git
blobs checked; `git diff --check` passed. Staged bytes are checked again before
commit, and committed bytes/freshness after commit.

The actual quick verifier was deliberately run with stale `Objects.h` bytes:
it failed nonzero at `objects`, retaining unchanged stale bytes. The real
generator restored canonical output. Evidence:
`.build/p12-audit-stale/summary.json` and `negative-command.log`.

Tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, MinGW GCC 15.1.0,
Doxygen 1.13.2. Local Clang/Linux sanitizer runtimes are unavailable; the
existing CI supplies GCC 13/Clang 18 ASan/UBSan.

## Maintainability and scope

The correction replaces one unsuitable literal helper and restores two generated
coverage details; it introduces no new module, registry, runtime table, I/O,
dependency or semantic operation. The ledger and generated numeric headers
retain identical facts and bytes. Generator ownership remains authoritative;
generated coverage was regenerated, never hand-edited. The dictionary guide,
CHANGELOG, progress and original report link are updated with this audit.
No backlog, roadmap or feature-coverage state changed. All hardware and future
semantic implementation gates retain their assigned owners.

## Closure

All confirmed findings are fixed within Prompt 12. Final quick, Git-byte
integrity, exact-commit CI and synchronization are verified before handoff;
the resulting commit and actual CI status are reported there. No hardware or
sibling test suite was run. Firmware applicability, read effects and semantic
operations remain explicitly unqualified rather than newly claimed support.
