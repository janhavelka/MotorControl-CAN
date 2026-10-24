# Documentation contract for every implementation stage

Apply with [COMMON.md](COMMON.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and the active numbered prompt. This file specifies future deliverables. The checkout remains a planning promptset until implementation is explicitly assigned; filenames, declarations and commands below are requirements, not claims that those artifacts already exist.

## Keep documentation beside the work

Update the affected user guide, public header comments, examples, coverage ledger and changelog in the same stage as their contract changes. Do not postpone all documentation until stage 19. Stage reports record what was changed and checked; they do not replace the durable user documentation. Preserve original references, historical reports and their provenance in this repository.

Write direct engineering English: what the operation does, what the caller supplies, what it returns, what can fail and what evidence it establishes. Define terms once, use actual identifiers and show a small ordinary use case. Avoid promotional claims, vague adjectives such as “robust” without a measurable contract, invented terminology and restatements of function names. Explain a protocol term when its meaning matters. Prefer a linked authoritative contract over copied paragraphs that will drift.

Keep documented, implemented, host-tested, firmware-built and physically qualified coverage separate. Identify the exact model/interface/firmware/configuration for device claims. A fake-node scenario, compiled board example, successful CAN TX or cached value does not establish physical completion. N5 and EPOS4 design fixtures remain design evidence; surveyed products are not supported drivers.

## File inventory and stage ownership

The paths below are target documentation. Consolidate a small cohesive topic when useful, record the final path in `docs/source_map.md`, and fix its inbound links. Do not create empty placeholder pages merely to satisfy this list. Before a topic exists, the relevant stage report may identify its future owner and prerequisite; once an API or workflow exists, its usable contract must exist too.

| Target | First accountable stage | Required content and later updates |
| --- | --- | --- |
| `README.md` | 00; working usage in 03–04 | Present project status, names, scope, unsupported areas, native install/use, minimal offline example, ownership and evidence limits, documentation/verification/license links. Add hardware instructions only with a real example. |
| `docs/README.md`, `docs/source_map.md` | 00–03 | Reading order and a small responsibility map: installed core, profiles, adapters, applications, tests, scripts, generated files, evidence. Distinguish maintained contracts from historical audit material. |
| `docs/backlog.md`, `docs/roadmap.md`, coverage ledgers, `docs/prompts/PROGRESS.md`, `docs/reports/stage_NN.md` | 00; reports/progress every stage | Concrete gaps, dependent stages/evidence, status and exact verification. Update backlog/roadmap/coverage when their respective facts change, following COMMON. No unchecked item silently becomes implemented. |
| Original-source manifests and object/function ledger | 00–01 | Source URL/identity/hash/page, model/firmware, object type/access/unit/PDO facts, conflicts, resolution and independent implementation/test/qualification columns. Preserve originals. |
| `docs/architecture.md`, `docs/profile_contract.md` | 02; refine with each seam | Actual dependency direction, stack choice, caller storage, profile/capability selection, common axis versus endpoint identity, native extensions and explicit protocol differences. Keep `architecture_plan.md` visibly a plan or explain its relationship to the implemented architecture. |
| `CONTRIBUTING.md` | 03 | Applicable local workflow, style, exact build/check commands, dependency setup, generated-file updates, tests/docs/changelog expectations, focused changes and evidence reporting. Follow this checkout's branch/publication instructions; do not copy another repository's Git authority. |
| `SECURITY.md` | 03; review at 19 | Real supported-version policy or honest pre-release status, verified reporting route if available, input/length/resource validation scope, externally owned transport and application controls. Do not invent maintainer addresses, private channels, response deadlines, certification or safety guarantees. An unavailable reporting route remains an explicit release gap. |
| `LICENSE`, attribution and distribution exclusions | 03; audit at 19 | Correct software license and preserved copied-code notices; distinct licenses for optional stack/SDK code and vendor/standards/reference exclusions. Keep installation/license metadata consistent. |
| `CHANGELOG.md` | 03; every user-visible change | `Unreleased` entries describing actual changes, compatibility/migration and meaningful limitations. Group Added/Changed/Fixed/Removed as needed; include only applicable groups. Dates/tags/release claims reflect actual releases. Do not list planned APIs as added. |
| `Doxyfile`, `.clang-format`, verification/CI integration | 03 | Tested documentation configuration and formatting style; exact local invocation/tool version; warning failures, logs and clean output location. Formatting policy is described in [MAINTAINABILITY.md](MAINTAINABILITY.md). |
| `docs/getting_started.md`, `docs/verification.md` | 03–04; extend in 14–19 | Isolated CMake source/install consumer, C++11 requirements, reproducible host commands, dependency versions, check modes/artifacts and limits. Ordinary native use must not require a sibling checkout or framework. |
| `docs/axis_contract.md`, unit guidance and public headers | 04; extend in 10–11 | Coordinate/reference/scale provenance, exact and approximate representations, units/direction, checked quantization/limits/error bounds and invalidation. Examples include a stepper and servo representation without treating their geometry as identical. |
| `docs/canopen_services.md` and frame/codec API comments | 05–07 | Frame format and payload/DLC/RTR distinctions, service matching, chosen stack boundary, capacity, SDO ambiguity/abort/timeout, asynchronous routing, PDO mapping validity and generation. Do not describe CANopen as universal CAN semantics. |
| Typed reads/settings, action, motion and native-profile API guidance | 08–11 | Full declared native coverage, exact inputs/outputs, read side effects, state/mode prerequisites, multi-step effects, stop variants, uncertain writes, terminal evidence and firmware gates. Link the object ledger and actual tests. |
| `docs/can_owner.md`, `docs/fieldcore_handoff.md` | 02 contract; implementation in 12/17 | Single receiver, caller-owned queues/time/caches, resource budgets, event dispatch, serialization, deadline/epoch/cancellation/recovery and prospective consumer integration. FieldCore remains read-only; no claimed integration. |
| Discovery/commissioning guide | 13 | Explicit non-changing probe and identity limits, known endpoint selection, bounded discovery, write/readback/persistence/restart prerequisites, uncertain outcomes and recovery. No default wiring/node/bitrate guesses. |
| Example `README.md` files and Arduino/IDF integration guide | 14; console integration in 15 | Real target/toolchain, configuration and pins from established evidence, build/flash/use commands, shared app/owner/platform ownership, capacities and platform differences; native fake/offline example independent of hardware. Add actual shared console usage and parity when implemented in 15. |
| `docs/cli_contract.md`, command help and API/CLI mapping | 15 | Complete command grammar/aliases, argument units/ranges, mutation classes, result identity, JSONL schema/version, error/terminal/uncertainty semantics, cached versus refreshing behavior and Arduino/IDF parity. |
| Runner guide, scenario catalogue, `docs/bench_scenarios.md` | 16–17 | Actual runner arguments, parser checks, offline/dry-run versus HIL verdicts, finite campaigns, logs/session identity, missing evidence, repeat/restore limits and no replay of uncertain motor effects. |
| `docs/hardware_bench.md`, qualification summaries | 18; early evidence slice when needed | Established board/transceiver/drive/motor/firmware/wiring/node/bitrate and authorization scope, procedures, raw evidence links, observed behavior and unmeasured facts. RS bench facts do not transfer. |
| `docs/releases/<version>.md`, final coverage/migration | 19 | Actual shipped API/services/model/firmware scope, limitations, checked artifact/revision, dependencies and separate host/firmware/hardware verdicts. No tag/publishing action is authorized by writing release notes. |

Keep the package name `MotorControl-CAN` and C++ namespace/include/CMake identity `MotorControlCAN` consistent. One real version source feeds manifests, headers and documentation; generated version fields use the same deterministic generator/check mode as other metadata. Avoid wall-clock build dates or dirty-state strings in reproducible published artifacts unless explicitly separated as development diagnostics.

## Doxygen contract in the source

Document installed public declarations where users include them. Use the existing ecosystem's `/** ... */` or `///` form consistently. Include `@file` and a brief responsibility statement per public header; retain applicable SPDX/attribution. Public namespaces/modules need a short scope description. Types, fields, enum values, constants and callback signatures need the meaning a caller cannot infer safely from their C++ type. Do not hide undocumented public members or extract private internals to inflate apparent coverage.

For every public fallible function, document the following where applicable:

1. Purpose and preconditions: supported capability/profile/firmware, binding/reference/configuration freshness and legal operation phase. State whether the call only validates, prepares, yields work, consumes an event, inspects retained results or changes local state.
2. Every parameter with `@param[in]`, `@param[out]` or `@param[in,out]`: exact unit/basis/direction, inclusive range, capacity/count meaning, null/empty behavior, aliasing restrictions and ownership. Distinguish bytes from frames, command increments from encoder counts and ramp time from acceleration.
3. Return meaning with `@return` and useful `@retval` cases: local status versus yielded/terminal operation outcome, error-detail interpretation and preserved raw protocol/controller evidence. Document meaningful precedence when multiple inputs can fail. `OK` must say which success it proves.
4. Publication rules: a rejected or invalid call leaves outputs/context unchanged; identify any explicit count-reset exception. An accepted event reporting a transport fault, deadline or drive failure can validly advance a context to a failed terminal state while the advancement call returns `Ok()`. Distinguish call validity from operation outcome. Explain when partial external effects can already exist despite atomic local publication. Typed read getters check kind/phase/identity and never manufacture a default value on failure.
5. Lifetime: call-only borrowed buffers; admitted work and retained data own their bytes. Context reuse, terminal-result retention/consumption, reference/string lifetime and whether returned views are invalidated by the next advance/reset. Do not borrow an input pointer for later execution by accident.
6. Time and progress: caller-supplied monotonic unit/domain, zero validity, wrap/backward-clock rules, deadline inclusivity, work budget and no-progress behavior, bounded storage and work. Distinguish an observation timestamp from a physical wire timestamp and a work budget from an elapsed-time guarantee. Pure functions say they acquire no time and perform no I/O.
7. Concurrency: independent contexts may be used independently when true; the owner serializes mutation of a shared context and its observers. No implied lock, ISR safety or callback reentrancy. Describe callback calling context and pointer/recursion restrictions where a real callback exists.
8. Side effects and evidence: which native writes can occur through yielded work, what cancellation/recovery invalidates, and what remains uncertain after submission without confirmation. TX completion, SDO acceptance, observed drive state, trajectory completion and physical standstill are different observations. Link the contract for the concrete operation and device-source pages instead of repeating a universal disclaimer on every arithmetic helper.

For value types, document default validity, initialization, units, ranges, sentinel values, counters/overflow and whether a field represents requested, effective, observed or estimated data. Document raw bit values separately from interpreted states. Use a shared type/module contract for repeated invariants, with a local link and explicit exceptions; avoid repeating an eight-point template on trivial declarations. Private comments explain a non-obvious invariant, arithmetic boundary, source contradiction or protocol ordering, not what the next line already says.

### Small illustrative planned API comment

This is a documentation example adapted from current RS naming; it is **not an implemented MotorControlCAN declaration or a frozen CAN wire-width requirement**. The eventual function belongs only where a reviewed profile needs signed 32-bit narrowing. Keep common targets wider and quantize native floating formats separately.

```cpp
/** @brief Narrow an exact native position to a signed 32-bit field.
 *
 * Validates representation only. Axis limits, units, binding and drive readiness
 * are checked by the operation that uses this value. Performs no I/O or clock
 * access and creates no command or evidence that motion occurred.
 * @param[in] value Exact native command increments, in INT32_MIN..INT32_MAX.
 * @param[out] output Narrowed value on success; unchanged on failure. The caller
 * owns this object; no pointer or reference is retained after the call.
 * @return Status with Err::OK only when value is representable;
 * Err::ILLEGAL_VALUE otherwise, with a documented range reason in detail.
 * @note Bounded, allocation-free arithmetic. Independent calls are reentrant;
 * concurrent writes to the same output require caller serialization.
 */
Status narrowNativePosition(int64_t value, int32_t& output);
```

Use the selected `Err` spellings in [CONVENTIONS.md](CONVENTIONS.md) and document the concrete detail value if this helper is implemented. Add an ordinary compiled example showing success, rejection and preserved output when it actually exists. Do not add this function solely to reproduce the example.

## Documentation build and verification

Stage 03 creates a real root `Doxyfile` and registers its command with the chosen verifier and a CI docs/static job. The initial headers that stage introduces must already be documented; newly introduced APIs extend the same gate. The full verifier runs documentation; a smaller local mode may omit it only if its summary explicitly says so. A docs-only edit needs appropriate local validation, not a remote dispatch for each wording change.

Require these effective settings with a tested Doxygen version:

```text
PROJECT_NAME          = "MotorControl-CAN"
EXTRACT_ALL           = NO
EXTRACT_PRIVATE       = NO
WARN_IF_UNDOCUMENTED  = YES
WARN_IF_DOC_ERROR     = YES
WARN_NO_PARAMDOC      = YES
WARN_IF_UNDOC_ENUM_VAL = YES
WARN_AS_ERROR         = FAIL_ON_WARNINGS
MARKDOWN_SUPPORT      = YES
GENERATE_LATEX        = NO
HAVE_DOT              = NO
```

Use `WARN_IF_INCOMPLETE_DOC = YES` when supported by the selected version. Check version support rather than copying an unknown tag. Pin and record the tool version in CI/local setup, or pin a verified environment providing it; do not silently rely on a moving `ubuntu-latest` package. A downloaded binary is version/hash checked. A version-compatible equivalent to `FAIL_ON_WARNINGS` is acceptable only if a deliberate missing-doc fixture proves the process fails. Preserve exit status through log capture; a successful `tee` must not hide a failed Doxygen process.

Use explicit `INPUT` paths for public headers and maintained user pages, `RECURSIVE = YES` for the actual include tree, and `USE_MDFILE_AS_MAINPAGE = ./README.md` to disambiguate the docs index. Exclude generated documentation, vendor/standards originals, extracts, copied baseline, audit/reports and build trees. Scope exclusion patterns to those paths; a bare `*/test/*` can accidentally exclude an entire checkout under a directory named `test`. Never fix warnings by setting `EXTRACT_ALL = YES`, suppressing broad public namespaces/macros or narrowing input until an API disappears. Generated public declarations need generated documentation or a precise justified treatment of internal overrides while typed public names remain covered.

Write generated HTML/XML and warning logs to ignored development output. Do not commit generated HTML. Disable HTML timestamps and normalize source/output paths as supported so identical input/tool/version produces stable documentation and diagnostics. Version fields derive from repository metadata, not generation time. Doxygen and any optional renderer must not download content during the build. New API headers are discovered from the actual installed-header inventory; missing input, empty extraction or silently undocumented free functions must fail a repository/API coverage check. Doxygen warnings alone are not a semantic review and may not detect every undocumented declaration; use a small XML/API-inventory check if needed, rather than a brittle comment-count threshold or a new general documentation framework.

The normal local command is `doxygen Doxyfile` once stage 03 installs the actual configuration. Record its exact prerequisite/version and verifier integration in `docs/verification.md`. Run missing-parameter/undocumented-member fixtures in a temporary tree to establish that the gate fails; run those checks again when the docs tooling changes. Do not inject intentional warnings into production headers. No Doxygen installation is required merely to review this planning-only promptset.

The existing repository checker should also check maintained local Markdown links, required topic paths, version/generated freshness and relevant API/CLI coverage. Preserve provenance hashes; do not require working internet URLs for an offline build. Explicitly define its scope for historical snapshots and not-yet-created target paths. Compile small public examples through source and isolated installed consumers; do not present pseudocode or unbuilt snippets as tested usage. Prefer included maintained example code over multiple divergent pasted copies. CLI help/schema examples come from the actual tested command contract; documentation generation is not a substitute for parser/behavior tests.

Every stage report adds a brief documentation record:

```text
Affected docs/API comments/examples/CHANGELOG: paths and reason
N/A decisions: item and concrete reason (for example, no public change)
Checks: exact command, version, result and retained log where useful
Open docs/evidence gaps: missing fact, affected claim and owning stage
```

Stage 19 verifies this inventory against actual shipped files and readers' workflows. “N/A” is acceptable for unaffected topics; it is not a way to defer an existing undocumented API or claim a check passed. Report unavailable checks and complete independent local checks.

## Source evidence for this contract

Live documentation-source inspection on 2026-10-23; no sibling edits, CI dispatch or hardware tests. Recheck revisions/dirty state under COMMON for execution. All paths below are relative to `Projects/`.

| Repository / local HEAD | Inspected documentation and source seams | Observed convention and deliberate CAN choice |
| --- | --- | --- |
| MotorControl-RS / `540e640c4bb7bff989082d765778fcaebfdf0d7e` | `AGENTS.md`, `README.md`, `CHANGELOG.md`, `include/MotorControlRS/{Units,MoveOperation}.h`, `src/Units.cpp`, `examples/probe_cli/ProbeApp.cpp`, `scripts/{verify,check_repository,bench_probe}.py`, `.github/workflows/verify.yml`, `test/units_test.cpp` | Exact units, unchanged outputs, caller-owned operations, concise README/source-install usage, retained coverage limits and offline maintained-link checks. No root Doxyfile/CONTRIBUTING/SECURITY/.clang-format was present: strict docs/style files here are deliberate additions from I2C practice. |
| ADS1115 / `235af3e8c722661a89a44766a4ccd903a654f6fe` | `AGENTS.md`, `README.md`, `CONTRIBUTING.md`, `SECURITY.md`, `Doxyfile`, `.clang-format`, `.github/workflows/ci.yml`, `include/ADS1115/Config.h`, `src/ADS1115.cpp`, `test/test_basic.cpp`, `tools/run_i2c_hil.py`, `examples/02_owner_safe_poll/main.cpp` | Explicit callback lifetime/uncertain effect, parameter comments, strict separate docs job, explicit Doxygen inputs and scoped exclusions. Reuse clarity/ownership, not automatic sensor recovery/profile replay, fixed version/contact details or another checkout's Git workflow. |
| INA228 / `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `AGENTS.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, `Doxyfile`, `.github/workflows/ci.yml`, `include/INA228/INA228.h`, `src/INA228.cpp`, `test/test_basic.cpp`, `tools/test_run_i2c_hil_parser.py` | Units/validity/cached diagnostic side effects in public types, warning-as-error docs with retained logs. CAN retains its own prompt/reference/evidence records despite INA228's narrower document retention policy. |
| SHT3x-main / `f65f15573e3c9b1ef04cce72ad08f307d021e563` | `AGENTS.md`, `Doxyfile`, `tools/sht3x_cli_contract.py` | Explicit simplicity/reuse rules, incomplete-doc warning gate and shared CLI execution classification. No imported sensor lifecycle or health/retry defaults. |
| MB85RC / `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | `AGENTS.md`, `Doxyfile`, `include/MB85RC/MB85RC.h` transfer comments | Enum/parameter warnings and precise staged buffer/partial-effect/result contracts. CAN codec buffers remain call-only borrowed and admitted work owns data; do not copy MB85RC's retained application buffers. |
| MCP45HVX1 / `c6c7feefd79b9fc16ddc7b7786ad187e5185fde6` | `AGENTS.md`, `Doxyfile.api`, `.github/workflows/ci.yml` docs steps | Separate public-API extraction, explicit warnings and Doxygen 1.15.0 binary/hash pin. Reuse a small checked gate; a second Doxyfile is optional when one config suffices. |
| PCA9555 / `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | `AGENTS.md`, `Doxyfile`, `include/PCA9555/Config.h` | One-attempt effect/count evidence and callback restrictions. Its `WARN_NO_PARAMDOC = NO` is not adopted: CAN requires parameter documentation. |
| FieldCore-node / `da94ed5280ed42ee3a55e0d977f9cb6565c56b51` | `AGENTS.md`, `src/devices/rs485_motor/Rs485MotorModule.cpp` | Actual prepare/next ownership, binding generations and write-effect projection. CAN documentation describes an independent prospective integration; no FieldCore CAN implementation or universal native field is inferred. |

All inspected sibling worktrees were clean except MotorControl-RS, which contained untracked TL57 reference material: `docs/pdf-extracted-md/Lichuan_TL-R_Serial_Examples_CN.md`, `Lichuan_TL57-R_Operation_Manual_CN.md`, `Lichuan_TL57-R_User_Manual_CN.md`, `docs/reference/lichuan_tl57_rs485_{models,registers,sources}.json` and `docs/vendor/lichuan_tl57_rs485/`. That material was unrelated to the inspected documentation seam. Historical audit HEADs for ADS1115/MCP45HVX1 differ; the live revisions above are the evidence for this contract, not assertions of upstream synchronization or successful sibling checks.
