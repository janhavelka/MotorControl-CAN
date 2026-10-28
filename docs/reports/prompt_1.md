# Prompt 1 source audit

Status: Complete. Date: 2026-10-28. This executes the user's pasted former
Stage 00 audit assignment against the current version-2 promptset. Its source
audit is current prompt 1; the extra requested original-reference verification
is performed here without claiming prompt 2's full ledger intake is complete.
No CAN library source or hardware operation belongs to this assignment.

## Execution and plan

The starting checkout was clean on `main` at `986a00b`, eight commits behind
its fetched upstream. The lead inspected the incoming merged correction work,
fast-forwarded to `7878f7c`, then created `promptset/motorcontrol-can` for this
new promptset execution. This branch will be retained across later prompts.
The destination and configured upstream are `origin/promptset/motorcontrol-can`.
No sibling checkout was fetched or modified.

1. Read current RS declarations, implementations, consumers, tests and workflow;
   compare the protocol-3 console and the retained verification scope.
2. Audit the specified I2C/codec libraries and FieldCore read-only, retaining
   distinct lifecycle, health, result and generation contracts.
3. Verify originals and rendered conflict pages, recording applicability and
   feature gates without inventing firmware facts.
4. Publish the live source map, executable backlog/roadmap and unknown CAN bench
   facts. Record current tools and requirement owners; check paths, integrity
   and whitespace, independently review, then commit and sync.

The lead owns documentation and Git. Read-only reviewers use `gpt-6.1-sol`
with high reasoning for RS and ecosystem audits, and `gpt-6-astra` with high
reasoning for device-source conflicts. Four runtime slots include the lead.
There are no worker branches, hardware sessions or sibling edits.

## Delivered scope

| Pasted Stage 00 obligation | Current deliverable and boundary |
| --- | --- |
| Live revisions, dirty state, CI/tool availability and target state | [Input record](prompt_1_inputs.json), source delta and verification below; target has no core, build target or CI workflow yet |
| Source map, backlog and roadmap | [Source map](../source_map.md), [backlog](../backlog.md), [roadmap](../roadmap.md); current 1-50 owners replace old stage numbers |
| Actual naming/ownership/CLI/runner/package inventory | Source-map rows include declarations, implementations, real callers, focused tests and planned CAN differences |
| Original hashes, covers and conflict applicability | All 8 main originals and all 17 public-code originals independently checked; full planning checker covers 123 artifacts; page review below |
| No-hardware assumptions and bench facts | [Bench record](../hardware_bench.md); actual controller/transceiver/drive/motor/firmware/wiring/node/bitrate and live authorization unknown |
| Requirements and honest execution ledger | [Coverage](../coverage.md) retains R01-R35 and their owners; [PROGRESS](../prompts/PROGRESS.md) records this assignment only; full ledger 2 remains pending |

README, documentation index, CHANGELOG and requirement navigation now distinguish
the executed audit from pending software support. No C++ API, generated object
policy, CAN firmware, Doxygen output or hardware qualification was added.
Public-source comments are not applicable until declarations exist.

## Live source delta

All 13 requested sibling checkouts exist and were clean at inspection. Full
HEADs, branches and final dirty-state comparison are retained in the input
record. No uncommitted behavior is attributed solely to HEAD. No remote update,
fetch, checkout, configuration change or source edit was performed in a sibling.

- MotorControl-RS remains `bd831c859daa9c8c07b8d27a2dd879bb4e750511`;
  ADS1115 remains `235af3e8c722661a89a44766a4ccd903a654f6fe`;
  INA228 remains `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`.
  These match the 28 October maintenance audit. Current FieldCore
  `5ad9292dc8481cf08be9a61f662495a6474d1da0` likewise matches that audit.
- Older ecosystem notes name ADS `381236e`, MCP `d3eae4a` and FieldCore `da94ed5`.
  ADS and MCP advances to current revisions are documentation-only retained
  branch audits. FieldCore's actual admission/publication diagnostic hooks were
  reread; the 28 October findings remain supported. Historical reports were
  preserved rather than silently relabelled with current revisions.
- SHT3x, BME280, MB85RC, PCA9555 and RV3032 source revisions remain those
  recorded in the prior ecosystem audit. SHZK-PT, VTN4xx and VibWire-108 were
  inspected live at the exact revisions in the input record. No optional
  checkout is missing.
- The retained RS quick run is older: `5da4ba7f13be457ea7bf9706ffd7d546821669c7`.
  Comparison to current RS shows unchanged core, examples, existing runtime
  tests, verifier/workflow and package metadata. CMake adds one offline
  Lichuan reference regression, alongside research/preparation documentation.
  This justified only the focused new regression and configured inventory
  inspection below, not another broad run of unchanged suites.

## Conventions retained

The source map is the exact path inventory. These decisions retain the
[selected conventions](../prompts/CONVENTIONS.md) without adding synonyms:

- RS `Status`, free `Ok()`, `Status::isOk()`, explicit bool and free
  `errToString()`; status details/messages remain distinct from operation
  outcome, execution uncertainty, native drive state and application health.
- Concrete caller-owned contexts and operation-specific
  `prepare.../next.../advance...`; checked typed read getters preserve output
  on invalid kind/provenance. A valid failure event can be consumed with
  `Ok()` while the retained operation becomes failed.
- Exact units, rational coordinates, explicit origin/reference/configuration
  generations and candidate validation. Velocity duration begins at successful
  preparation, including later queue/setup time. Native RPM widths and ESS
  geometry are not common multi-drive assumptions.
- Owner `admit/admitUrgent/service/result/release/cancel` roles, immutable copied
  input, reserved retained completion and explicit release. Other traffic and
  diagnostics cannot consume frames away from the physical owner.
- Actual protocol 3 CLI inventory, correlated requests/results and Python
  session/evidence ownership. Live inventory and tests agree with the protocol
  contract. `stats reset`, `probe`, `read identity` and `profile list` are wire
  forms; Python internal hyphenated dispatch names are translated internally.
- `test/`, `scripts/`, source/install consumers and C++11 core boundaries.
  Installed users must not need the source ledger, Python, SDK or sibling tree.

## CAN differences and source exceptions

- RS has one installed ESS-RS profile. Leadshine iEM-RS is its documented
  contrast, with different ramp units and a contradictory 50 ms cadence claim
  in original PDF pages 25 versus 36. LC57BK-R and TL57-R material is research.
  CAN retains separate motor kind, feedback, protocol and support maturity;
  N5/EPOS4 are planned contrasts, not implemented drivers.
- RS owner code is in `examples/common/`; ProbeApp/ProbeConsole/StateCache are
  actually in `examples/probe_cli/`. Planned CAN placement of reusable app and
  console in `examples/common/` is deliberate. The final source-path check
  corrected five initially mislocated review references before acceptance.
- RS root guidance still mentions old `reset`/`ping` forms. The real CLI,
  protocol 3 contract and rejection tests take precedence. No sibling file was
  changed and no obsolete alias was imported into CAN.
- RS read freshness is strict at its age boundary; cache/axis freshness can
  include equality. Reference invalidation clears confidence even when a
  generation increment fails. These are explicit exceptions to audit and test,
  not permission to assume all sibling APIs are identical.
- I2C libraries differ in passive binding, start-call return, terminal take,
  callback lifetime, cleanup, health latching and counter overflow. ADS/INA
  generation counters skip zero after wrap, unlike the no-reuse safety rule.
  MB retains staged application buffers; CAN admitted work owns bytes. PCA
  cancellation can owe cleanup, whose success does not erase primary failure.
- CAN health/statistics are an explicit application contract: passive cached
  observations and logical terminal services, separate physical traffic and
  retained errors. Sensor automatic retries, profile replay, latch policy,
  startup writes and lifetime-counter wrap do not become motor policy.
- FieldCore rejects motor retries and freezes command/configuration identity;
  generation exhaustion prevents reuse. Its components are build-profile and
  service selected, not a universal active CAN module. Its RS 500 ms
  reconciliation rule and UART timing/DE/gap evidence remain application- and
  transport-specific. CAN integration is still future work.
- CAN adds object/service evidence, complete unsigned SDO aborts and continuous
  heartbeat/PDO/EMCY routing. Local correlation cannot identify an identical
  late SDO response or delayed same-ID/same-DLC PDO after route reuse. Confirmed
  map contents and qualified sample provenance remain separate.

## Original references and unresolved facts

The device reviewer checked all five rendered covers and conflict pages, plus
original EDS sections and the pinned public example. All 8 main originals match
[SOURCES.json](../reference/SOURCES.json) lengths and SHA256 values; all 17
public-code originals match their manifest. The full checker also verifies
derived/reference/baseline evidence. This turn did not refresh upstream vendor
pages; retained HTML hashes prove retained bytes, not current webpage content.

| Original | Documentary applicability | Observed limit |
| --- | --- | --- |
| EN86: [English 86C manual](../reference/vendor/86C_CANopen_user_manual_en.pdf), 20 pages | Cover CL86-C/OL86-C | No established installed-firmware binding |
| ZH86: [Chinese 86C manual](../reference/vendor/86C_CANopen_user_manual_zh.pdf), 20 pages | Cover CL86-C/OL86-C | Disagrees with EN86 status definitions |
| OP86: [86C operating manual](../reference/vendor/CL86C_OL86C_operating_manual_zh.pdf), 14 pages | Cover CL86-C/OL86-C | Internal pp3/10 say57C; p3 labels CAN pin table RS485 |
| ZH57: [Chinese 57C manual](../reference/vendor/57C_CANopen_user_manual_zh.pdf), 20 pages | Cover CL57-C/OL57-C | Separate model evidence; internally conflicting output labels |
| OP57: [57C operating manual](../reference/vendor/CL57C_OL57C_operating_manual_zh.pdf), 14 pages | Cover CL57-C/OL57-C | No matched firmware; not automatically 86C evidence |
| [EDS](../reference/vendor/LC_StepDriverCAN_V1.1.eds) | Vendor `XLC`, product `LC_SterDriverCAN`, product 1/revision 0 | Generic metadata, not automatic model/firmware detection |

All page references are one-based PDF pages. Operating-manual renderers emitted
structure-tree warnings, but the inspected pages rendered successfully. EDS
independent accounting found 99 indexes and 202 scalar entries; EDSVersion 4.0,
file version/revision `0x01/0x01`, creation 2020-12-23 and modification 2022-11-11.
Dates do not identify firmware. `1009/100A` are U16, and `1018` has only 00-03
despite count HighLimit 4; no serial subindex 04 is declared.

| Unresolved claim | Original evidence reread | Smallest affected scope / next evidence |
| --- | --- | --- |
| Status dialect | EN86 p12 alarm7/zero-speed10/target-effective12; ZH86 p12 alarm3/running14; OP86 p10 alarm7/running14 | 3 retains alternatives; 29 matched-firmware evidence before dependent19/30-36 interpretation; reviewed raw transport may proceed |
| DO addresses/functions | EN86 pp8/18 puts polarity 2400:0B and DO0-2 at 0C-0E; ZH86 p19/EDS puts polarity 0C and DO at 0D-0F, 0B reserved; function 2/3 also disagree | 21 typed output assignment gated; never probe both write addresses |
| 57C physical output identity | ZH57 p8 section2.2.2 maps60FE bits 16-18 to DO0-2; p17 section3.6.2 labels both subentries DO1-3 | Profile-specific terminal evidence; no silent renumbering, fourth output or 86C inference |
| Mode/control access | EN86 p11/EDS 6061 RO versus EN86 p12 translated set instruction; 6040/6060 WO in EDS versus manual RW | 2/3 policy; no write to mode display or assumed controlword read-modify-write |
| Start/target order | EN86 p13 versus ZH86 p13/OP86 p11 velocity sequence | 29 then30/35; no enable merely to prepare stop/reset |
| Scale/default/range | 2201 default 0 versus detailed 400-51200 range; 2202 default 10 versus 0/1 choices | 20/29/33/34: observed scale/reference and valid values; no startup defaults |
| Ramp and command widths | EN86 pp11/13 ramps 6083/6084/609A U32 ms 0-2000; 6099 speeds U32 and 6098 one byte | Confirmed documentary widths; zero behavior/response bounds still unqualified |
| Host loss/stop/persistence/tuning | Missing consumer/quick-stop objects, unresolved 605D options and incomplete 2500-2520 units/effects | Explicit [backlog D07-D12](../backlog.md); no borrowed generic capability or guessed timing |

No contradiction was resolved by this audit. The 28 October corrections were
independently corroborated. The public Lichuan example was rechecked as a
counterexample: response command bytes used for SDO requests, wrong homing
widths, shared caches, missing-reply-as-stopped behavior, implicit enable/save
and unchecked scaling. It establishes no successful CL86-C firmware behavior.

## Tools and exact verification scope

Current PATH provides Python 3.12.10, Git 2.52.0.windows.1, CMake/CTest 4.0.1,
Ninja 1.12.1, MinGW GCC 15.1.0, Doxygen 1.13.2 and PlatformIO 6.1.19.
`clang++`, `cl`, `idf.py`, `pdftotext` and `pdftoppm` are not on PATH;
`IDF_PATH` is unset. Python `fitz`, `pypdf` and `serial` are discoverable.
This is tool availability, not proof a CAN firmware/docs target builds.

Current RS workflow commands, observed rather than newly executed:

```text
python scripts/verify.py --mode quick --build-dir build/verification
python scripts/verify.py --mode full --firmware arduino --build-dir build/verification
python scripts/verify.py --mode full --firmware idf --build-dir build/verification
```

RS CI uses Ubuntu 24.04 GCC/Clang, Python 3.12, PlatformIO 6.1.19,
pioarduino 55.03.311 and ESP-IDF 5.5.5. Verification requirements pin
PyMuPDF 1.27.2.2 and requests 2.32.5. The verifier requires Python >=3.10 and
CTest >=3.26, active Release assertions, actual registration, isolated
C++11/C++17 consumers and package/symbol checks.

Sensor workflows differ: ADS has C++11 support and IDF 5.3.5 pinned by digest;
several others require C++17. MCP's API docs pin Doxygen 1.15.0 while the local
tool is 1.13.2. ADS uses `tools/run_i2c_hil.py --parser-test`; INA uses
`--parser-self-test`; RV uses dependency-light `python -S` and a different
runner. No universal flag, language floor or SDK is inferred. Prompt 8 must
select and verify the CAN commands/pins, including strict parameter docs.

| Check | Actual result and limit |
| --- | --- |
| `python scripts/check_promptset.py --check-git-ref HEAD` at inspected base | PASS: 50 prompts, 35 requirements, 4 packs, 123 artifact hashes and committed copies |
| `python scripts/check_promptset_test.py` after documentation integration | PASS: 10 planning-checker regression tests; no CAN driver tests implied |
| Original main/public-code hash review and PDF covers/conflict pages | PASS for 25 original artifacts; independent source reading, not firmware qualification |
| `python ../MotorControl-RS/test/lichuan_reference_test.py` with process environment `PYTHONDONTWRITEBYTECODE=1` | 14 tests passed, 0.083s; only new offline regression beyond pinned suite |
| `cmake -S ../MotorControl-RS -B <external-temp> -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON` | Configure/generate passed with GCC 15.1.0; no native build performed |
| `ctest --test-dir <external-temp> --show-only=json-v1` | 79 configured tests listed, including `lichuan_reference_failures`; listing is not test execution |
| `verify.check_inventory(root, ctest_json, build)` on configure-only output | Rejected `missing or wrong native test command: actions`; executables were intentionally unbuilt. Expected precondition, neither a gate pass nor an RS defect |
| Final source paths, requirement ownership, sibling state, planning integrity and whitespace | Results retained in [prompt 1 validation](prompt_1_validation.json) and [source checks](prompt_1_source_checks.json) after combined review |

The external inventory directory was
`C:/Users/Honza/AppData/Local/Temp/motorcontrol-rs-prompt1-inventory-6f0655c0741f46c1873358a0013a0fee`.
The retained [RS quick result](../audit/evidence/motorcontrol_rs_quick/README.md)
is 78/78 at its recorded `5da4ba7` revision, plus its source/install/package
checks. It is not a fresh 79-test pass at current RS. No broad sibling suite,
CAN compilation, firmware build, remote CI or hardware check was performed.
This documentation/evidence milestone has no target CI workflow to dispatch.

## Acceptance review on 2026-10-28

A read-only review against unchanged, clean sibling revisions found three
source-map issues and corrected them in the same branch:

- The map now names RS `ReadContext` and `ActionContext` terminal fields. Live
  `Reads.h`, `Actions.h`, `Reads.cpp` and `ProbeApp.cpp` show direct context
  inspection and the distinction between whole-read publication and a
  successful state block retained before a later failure.
- The map now includes `RtuRunner.h/.cpp`, its owner caller and `runner_test.cpp`.
  Its physical TX and receive-interval evidence is separate from CAN controller
  enqueue, transmission, ACK and service confirmation.
- The old SHZK map row implied atomic output on every parse failure. Live
  `SHZK_PT.cpp` and `test_parsers.cpp` show that a checked scalar value resets to
  zero, while the multi-register count resets and its value array is preserved.
  The row now records both exceptions instead of importing them into CAN.

The planning checker passed for all 50 prompts, 35 requirements, 1,220 local
links and 123 artifact hashes. Its 10 regression tests passed; `git diff --check`
passed. Newly named sibling paths were checked locally. No sibling suite, CAN
build, remote CI or hardware test was run for this documentation correction.

## Maintainability and handoff

Reused the existing prompt contracts, requirement owners, original source
manifests and pinned verification. Added only the requested maintained audit
documents and small retained input/check records; no runtime abstraction,
placeholder API, new test framework or competing ledger schema was introduced.
Source paths and live behavior were reviewed independently; the lead checked
the integrated result. All installed/core/framework decisions remain owned by
later prompts, with unresolved device facts kept explicit.

Next executable assignment: [prompt 2](../prompts/2_source_ledger.md), full
source/object-ledger intake. Resume the same execution branch. Raw-byte checks
and representative page review here do not satisfy its complete schema,
manual-only accounting and per-row dispositions. Current prompt 1 can close
without a fixture; physical evidence acquisition remains 29 and qualification
47-48. Stop after this assigned audit; do not execute later prompts unasked.

Delivery is the commit containing this report on `promptset/motorcontrol-can`,
with the same branch on `origin`. The lead verifies committed artifact bytes,
pushes normally and reports the actual commit and fresh upstream comparison in
the final handoff. This avoids a self-referential commit hash in the report.

## Independent follow-up audit

A later read-only review of prompts 1 and 2 found that the retained
[path-check record](prompt_1_source_checks.json) still listed 123 source-map
paths after the Runner declaration, implementation and test were added to the
map. The record now lists all 126 distinct sibling paths; each exists at the
current clean sibling revisions. The original prompt-1 source claims and
support boundary remain unchanged. This corrects audit traceability, not a
CAN implementation or a rerun of unchanged sibling suites.
