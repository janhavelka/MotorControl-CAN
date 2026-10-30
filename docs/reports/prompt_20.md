# Prompt 20: native driver groups

**Current correction:** the [fresh independent 20/21 audit](prompts_20_21_audit.md)
implements the confirmed native setting subsets and corrects this checkpoint's
blanket blocker rationale. The original read-only checkpoint and its checks
below remain historical evidence; current status is still Partial for the
specific unresolved groups, not for the now-implemented settings.


## Scope and status

**Partial.** All nine independently established native read rows are implemented,
documented and host tested. Typed writes, their partial effects/readback and
configuration/reference invalidation remain open on the precise prerequisites
below. No unsupported setter stub, generic integer-write escape, success stub or
test-only workflow is presented as a production setting.

Entry commit `151c5a14b408adbc8ac0cac8104897beb021e2ba`; clean index/tree,
fresh fetch ahead/behind 0/0. Execution identity version 2, original base
`7878f7c`, same branch `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`. Prompt 19 is Partial on Q20; this stage
consumes its already verified ordinary raw-read/service machinery, not its
unimplemented non-consuming probe. No later prompt is executed here.

Rows and direct group names were frozen before implementation:

| Group / owner 20 | Rows and native read shape | Implemented API / evidence |
| --- | --- | --- |
| DRIVER | `2200/2201` U16 direction selector/subdivision | `prepareDriverRead(..., DRIVER)`, `nextDriver`, `advanceDriver`, checked `getDriver` |
| SOFT_LIMITS | `2202` U16, `607D:00` U8, `:01/:02` I32 | Same lifecycle with `SOFT_LIMITS`; raw coordinate basis remains unknown |
| MOTION_PARAMETERS | `6081/6083/6084` U32 rpm/ms/ms | Same lifecycle with `MOTION_PARAMETERS`; no common acceleration-rate substitution |
| Mode request | `6060` write-only under reviewed policy | Remains explicitly owned and blocked, never uploaded |

Current/holding `2502..2507` stay solely with owner/read-owner 22 alongside
loop/motor-rating prerequisites. Their peak mA, percent and ms facts are not
reimplemented or assigned invented percentage bounds here.

## Implementation and maintainability

Installed `DriverSettings.h` introduces a concrete `DriverContext` and typed
`DriverObservation`, backed by the existing `ReadContext`. Three fixed plans
and small wrappers in `Reads.cpp` reuse its matching, deadlines, retained aborts,
unknown effects, epochs, conservative age/skew and atomic getters. No second
SDO engine, owner/node table or configuration framework was added. Storage is
bounded by the existing 15-field read context; each driver group uses 2/4/3
fields and at most four provenance entries. The installed core remains C++11,
allocation/framework/I/O/clock free.

Generated read descriptors extend from 30 to 39 through the canonical ledger.
The validator restricts owner 20 to its exact nine reviewed read rows and keeps
read coverage separate from whole-object/write support. All read effects remain
unqualified; source and write gates are unchanged. Raw subdivision zero, selector
10, unexpected count, reversed signed limits and numeric extremes are retained,
not normalized to accepted writes or inferred geometry. No guessed subentry is
uploaded from a count value. Current target epochs invalidate getters; reads do
not mutate configuration/reference generations.

The offline example and isolated source/install/export consumers exercise the
actual public API. Precise header/package lists, CTest/checker registration and
strict documentation include the new API and guide. Catalogue strings, ledger,
vendor sources, Python and the optional stack remain unnecessary for consumers.

## Fresh source audit and write boundary

Original EDS lines 1274–1300, 1859–1867 and 1918–1969; EN86/ZH86 PDF pages
11–13, 16–18 were rechecked, rendering tables where needed. The current/holding
comparison also reopened EDS 1575–1622 and EN86 p19/ZH86 p19–20. Source hashes
and scope remain those in the canonical ledger; no original bytes were changed.

Source-confirmed: native widths/access, `607D` signed fields, `6081` 0–3000 rpm,
`6083/6084` 0–2000 ms. Contradicted/unresolved: `2201` default/range, `2202`
default 10 versus 0/1, physical direction, native soft-limit coordinate/range,
activation/enforcement and mode transition/retained-target behavior.

EN86/ZH86 p12 gives positive native position-parameter writes a documented
place between position-mode enabling and a separate bit-4 trigger. Positive
speed 1–3000 rpm and ramps 1–2000 ms are **not** blocked by absent numeric ranges,
velocity start-order conflicts, or unknown zero-ramp physics. What is missing is
concrete current compatible pre-trigger control/state provenance. Existing raw
state observations, `6041` low bits, `6061=1` and generic SDO acknowledgements
do not establish it. The installed code has no retained controlword-history
contract that supplies it. Neither a caller permission Boolean nor enabling as
a settings preamble is an acceptable replacement.

Therefore no confirmed admissible write path can be completed from current
evidence. All typed write obligations stay in 20: whole-candidate/state/range/
frame validation before TX, immutable admission, confirmed per-step effects,
uncertain final writes, matching readback, contradiction withdrawal and
scale/reference/prepared-target invalidation. They must pass real owner
integration when resumed; tests of raw reads do not close those write criteria.
Prompt 22 retains all current/holding duties. No hardware work is authorized or
performed by this stage.

| Live checkout / clean HEAD | Relevant implementation, actual caller/test and deliberate difference |
| --- | --- |
| RS `f812d7c1d364a03be86f02ae8f648dc8fca23d42` | `DriverSettings.h:232`, `DriverSettings.cpp:315`, ProbeApp `:1399/1436`, driver tests `:112/235`: copied request/prerequisites, checked prior snapshot and retained partial effects. Control tests `:157` reject an unsafe intermediate current write; `:161` permit base percent above maximum, disproving an invented universal relationship. MotionProfileApp `:4/58` retains snapshots and explicit restoration policy. Names are reused; native units/preconditions are not copied. |
| ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe` | Owner-safe-poll caller `:246` consumes matching terminals/configuration generations. Its sensor auto-recovery is not motor settings policy. |
| INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `src/INA228.cpp:2346/3110/3205`, calibration test `:2554`, bringup `:2129`: whole-plan validation and generation/freshness/uncertainty. Rollback at `:2314` is not copied to motor writes. |
| BME280 `d538f2e203ee26d6ddbc86f86d2b0cf4c73a6e98` | `src/BME280.cpp:1204/1621/933`, test `:6489`, bringup `:1638`: staged candidate/readback and changed generation without refreshing old sample. |
| FieldCore `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `Rs485MotorModule.cpp:50/655/674`: config replacement rejects active/retained work, invalidates dependent readiness/reference/cache, handles exhaustion and defers invalidation until terminal consumption. Read-only reference, not an integration dependency. |

All five worktrees were clean. Revisions are unchanged from prior relevant
audits; actual implementations/callers/tests and workflow registration were
reopened. Unchanged sibling suites were not rerun. Independent source/strong
boundary review used `gpt-6-astra` high; sibling/example/test work used
`gpt-6.1-sol` high, with disjoint ownership and no worker Git mutations. The lead
owned ledger, public types/core, packaging, docs, integrated checks and Git.

## Verification and handoff

The independent registered native suite passes **2,108 checks**: literal nine-row
vectors through the actual fake `SdoClient`, wrong replies/widths, every partial
failure position, raw extremes/unknowns, malformed terminal correlation, stale
epochs, incomplete/wrong-kind getter access, sentinel preservation, time zero,
age/skew, deadline/invalidation precedence and no hidden download/NMT/save/reset.
The offline preview passes. The initial strict Release build caught a missing
four-byte bound in the fixture's reply loop; it was corrected without weakening
warnings or changing production interpretation. All runtime code compiled.

| Integrated command | Result / retained evidence |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir .build/p20-quick` | PASS, 34 registered contracts plus repository and real generated freshness; `20261030T113925599886Z-27736-4luy1f2o/summary.json`. |
| `python scripts/verify.py --mode full --build-dir .build/p20-full` | PASS, 35 registered contracts, isolated export/install consumers, C++11/C++17/no-RTTI, 19 independent public headers and strict Doxygen/API extraction; `20261030T113925599886Z-24756-gu0zm142/summary.json`. |
| Ledger / generator contracts | PASS, 67 / 38 cases respectively, including exact new read scope and unchanged write/effect gates. |
| `python scripts/check_promptset.py --check-git-ref HEAD` | PASS, 99 indexes / 202 entries / 53 manual items and original/committed provenance. INDEX and post-commit HEAD checks accompany delivery. |
| `git diff --check` and changed handwritten C++ `clang-format --dry-run --Werror` | PASS. |

After formatting the new test/preview, their strict rebuild and the native
consumer passed again (3/3 focused CTests). Local versions: Python 3.12.10,
CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0, Doxygen 1.13.2; formatter 14.0.0.
Required unavailable local Linux sanitizer toolchains remain separate from
exact-commit remote results at handoff. No physical test,
settings-write test or motion qualification is claimed.

Public Doxygen, [settings guide](../driver_settings.md), README, CHANGELOG,
schema/generated coverage, API map, source map, PROGRESS, roadmap/backlog and
coverage describe actual read support and pending writes. The guide contains
each row's source, native field, blocker and intended later CLI ownership.
After checks, commit and sync on the recorded branch; the final handoff records
the actual commit, synchronization and required CI results.
