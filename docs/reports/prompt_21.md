# Prompt 21: digital I/O

**Current correction:** the [fresh independent 20/21 audit](prompts_20_21_audit.md)
implements the confirmed native setting subsets and corrects this checkpoint's
blanket blocker rationale. The original read-only checkpoint and its checks
below remain historical evidence; current status is still Partial for the
specific unresolved groups, not for the now-implemented settings.


## Scope and status

**Partial.** All 23 assigned readable rows are implemented as four typed native
groups, including documentary DI codes/polarity and checked `getIo`. Four
reserved entries remain excluded. Reassignment, filters and explicit output
actions retain the precise evidence gates below; their validation, partial
effects, readback and invalidation tests remain owed to 21. No unsupported
setter/action stub or raw integer-write substitute closes those obligations.

Entry commit `6eb900b09fb8f6d8d63729e4525576eec6d32310`, clean index/tree and
fresh fetch ahead/behind 0/0. Execution identity version 2, original base
`7878f7c`, same branch `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`. Prerequisites 19/20 remain Partial; this
independent chunk consumes their verified ordinary raw-read/service machinery,
not their missing probe/write contracts. No later prompt is executed.

## Frozen scope and implementation

| Group | Exact readable rows | Native interpretation |
| --- | --- | --- |
| `IO_INPUT_CONFIGURATION` | `2400:00..08` | Nine U16 values: count, complete polarity word, DI0..6 raw codes and documentary labels. |
| `IO_OUTPUT_CONFIGURATION` | `2400:0C..0F` | Four U16 exact-address raw words; conflicting DO semantics remain unresolved. |
| `IO_INPUT_FILTERS` | `2401..2407:00` | Seven U16 coefficients; no assumed time units/ranges/defaults. |
| `IO_OUTPUT_COMMAND` | `60FE:00..02` | U8 count, U32 value and mask readback; no electrical or terminal-map claim. |
| Reserved exclusions | `2400:09/:0A/:0B/:10` | Never uploaded or written, including contradictory manual aliases. |

`IoSettings.h` extends the existing `DriverGroup`/`DriverContext` pattern.
`prepareDriverRead`, `nextDriver`, `advanceDriver` and `getIo` reuse the same
read implementation, matching, immutable deadlines, retained raw abort/effects,
generation checks and conservative age/skew. The four fixed plans use 9/4/7/3
of the existing 15 fields; `IoObservation` owns at most nine provenance entries.
No second context engine, node table, receiver or larger owner capacity appears.
The core remains C++11, framework/allocation/I/O/clock free.

Unknown codes, high polarity bits, arbitrary count values and numeric extremes
are preserved. Count values do not drive enumeration. `getDriver` explicitly
rejects I/O kinds before its four-entry provenance copy; `getIo` rejects old
driver/other kinds and incomplete/stale/invalidated sources. Candidate publication
is atomic. Configuration, function, value/mask, logical monitor level and wiring
remain independent. Physical knowledge flags stay false. Reads do not modify
configuration/reference generations or claim an atomic device snapshot.

The canonical ledger adds separate read coverage for these 23 rows. Generation
now publishes 62 read descriptors in total without widening write/access/effect
policy. Validator tests restrict read owner 21 to the exact non-reserved rows.
Original metadata, conflicts, source IDs/hashes and whole-object/write coverage
remain unchanged. All upload effects remain unqualified.

## Source review and remaining mutation gates

Original EDS lines 1343-1558 and 2035-2059 were reopened. Rendered EN86 p18,
ZH86 p19, OP86 p7 and ZH57 p8/p17, plus EN86/ZH86 p8/p17 and OP86 p6,
were compared with the original EDS. All six retained vendor source hashes
match the canonical source records. No original bytes or extracted evidence
were changed. [I/O guidance](../io_settings.md) owns the detailed source table
and mutation prerequisites.

Source-confirmed: exact native widths/access/subindices, DI function codes
0..13 and polarity bits 0..6. Independent review corrected a new comment/example
label from “unused” to the source's **undefined** for code 0. Whether an input
is unused remains an independent application fact.

DI writes are not blocked by the output address conflict: they lack applicable
drive/NMT-state and reassignment/activation behavior, function-combination
constraints and effects on active enable/stop/limit inputs. A checked current
assignment/level prerequisite cannot be replaced by a permission Boolean.

DO address/function conflicts remain Q06: EDS/ZH86 p19 use polarity `:0C` and
functions `:0D/:0E/:0F`; EN86 p8/p18, ZH86 p8 and OP86 p6 use
`:0B/:0C/:0D/:0E`. Codes 2/3 exchange running/drive-state and homing meanings;
only two polarity bits are described. Filter ranges/equation/units remain Q08,
with EDS defaults `50,40,100,4000,10,2,16` versus OP86's 2.

Output actions remain Q07: selected-model terminal mapping and complete
value/mask zero/clear/untouched-bit/sequential-update semantics are missing.
OP86 p7 has a CL57-C/OL57-C header; ZH57 p8 calls bits 16/17/18 DO0/1/2,
while p17 calls them DO1/2/3. Those claims do not establish an 86C mapping,
a fourth output or a writable union. Hardware absence alone is not the blocker;
the specific applicable facts are missing. No confirmed admissible mutation
path is currently established.

21 retains whole-candidate model/state/range/reserved-bit validation before TX,
immutable admitted settings, confirmed partial writes, uncertain effects,
readback/mismatch behavior, configuration/prerequisite invalidation and later
real owner integration. No reset/save/fault clear/enable/protective reassignment
is generated by any implemented path. The source review, raw reads and fake
prerequisite fixture do not close those mutation or hardware criteria.

## Live sibling audit and deliberate differences

All six checkouts below were clean and unchanged at their named revisions.
Implementations, callers/tests and relevant verifier/workflow registration were
reopened; unchanged sibling suites were not rerun. FieldCore remained read-only.

| Source / HEAD | Actual seam and caller/test |
| --- | --- |
| RS `f812d7c1d364a03be86f02ae8f648dc8fca23d42` | `profiles/ess_rs/DriverSettings.h:18/275`, implementation `:435/759/776`; `test/io_test.cpp:112-162/169-210/252-278` and installed consumer `:239-244` exercise `getIo`, unused/method-dependent inputs, whole-candidate and partial/uncertain effects. ProbeApp uses `getDriver`, not `getIo`. Names/context reuse matches RS; native codes/units/permission do not transfer. |
| ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe` | Owner-safe-poll example `:246-278` matches terminal kind/configuration generation. Sensor auto-recovery is not motor policy. |
| INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Public header `:168-270`, implementation `:870/1320-1330`: state, outcome and sample validity remain independent. No new generic health state is copied. |
| PCA9555 `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | Public header `:73-94/333`, implementation `:294-316/1018/1046/1077`, test `:1382`: intent/readback/uncertain masks and fenced RMW. Output latch readback is not measured pin voltage. |
| MCP45HVX1 `c6c7feefd79b9fc16ddc7b7786ad187e5185fde6` | Implementation `:510/552/591/611`, public header `:498`, test `:1080-1128` and firmware callers: explicit terminal/shutdown controls, register acknowledgement separate from external electrical effect. |
| FieldCore `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `include/TunnelMonitor/contracts/AuxInputs.h:12-29`, `src/input/AuxInputs.cpp:74-96/110`, native AuxInputs tests `:43/89/118/147`: enabled/known/active, cached/fresh and channel validity independent. No FieldCore type is imported. |

Independent source and final boundary review used `gpt-6-astra` high. Disjoint
sibling/test/example work used `gpt-6.1-sol` high. Workers did not mutate Git or
shared types/ledger/registration. The lead owned contracts, read implementation,
ledger/generation, CMake/package/consumer integration, docs and final checks.
Independent review found no runtime defect and the code-0 wording correction
was applied. New code follows the repository formatter without broad cleanup.

## Verification and documentation

The focused strict Release GCC 15.1 build and two new CTests passed; the native
suite reports **7,230 checks**. All 23 literal rows pass through the real fake
`SdoClient`; assertions cover every partial read failure position, service
outcome, wrong correlation/width, immutable admission, reserved exclusions,
raw limits/masks/all DI codes, unknowns, wrong-kind/stale getters, sentinel
preservation, zero/extreme time, age/skew and deadline/invalidation. Assignment,
output command and logical observation are tested separately. Test-only method
prerequisites use explicit external known/active premises and unused-input intent,
without claiming a production homing API or physical qualification.

| Integrated command | Actual result and retained evidence |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir .build/p21-quick` | PASS: 36 registered contracts, repository/registration and real version/object freshness. `20261030T120553454324Z-42024-ec3n8uxh/summary.json`. |
| `python scripts/verify.py --mode full --build-dir .build/p21-full` | PASS: 37 registered contracts, independent package contract, source/relocated install/export consumers, C++11/C++17/no-RTTI and strict Doxygen/API extraction of all 20 public headers. `20261030T120553454324Z-89108-rj3bu5lh/summary.json`. |
| `python scripts/check_lichuan_ledger_test.py` / `python scripts/generate_lichuan_objects_test.py` | PASS: 68 / 38 cases. After adding the precise pending mutation prerequisites to canonical rows and regenerating documentation, both suites and exact generated freshness passed again. |
| `python scripts/check_promptset.py --check-git-ref HEAD` | PASS: 99 indexes, 202 entries, 53 manual items, 137 original/committed artifact hashes. INDEX and post-commit HEAD checks accompany delivery. |
| `git diff --check`; changed handwritten C++ formatter check | PASS; generated files were regenerated rather than manually formatted. |

Local tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, GCC 15.1.0,
Doxygen 1.13.2. Lead formatter 14.0.0; the worker used 23.1.3 on its owned
test/example and the lead checks final formatting. Required unavailable local
Linux sanitizer toolchains remain distinct from final-commit remote CI. There
is no physical test or settings-write qualification in this stage.

Public Doxygen, [I/O guide](../io_settings.md), compiled offline example,
isolated consumer, README/CHANGELOG, generated accounting, API/source maps,
PROGRESS, roadmap/backlog and requirement coverage describe actual support and
pending obligations. CLI routes remain assigned to 40-42. Commit/sync and
required exact-commit CI accompany final handoff on the recorded branch.
