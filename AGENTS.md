# MotorControl-CAN engineering guidance

## CI and local verification

- Inspect the actual workflows and verifier before implementation. Reuse their commands, dependency pins, generated-file checks, test registration and package consumers.
- Run focused local checks for each coherent change. While this checkout is planning-only, run `python scripts/check_promptset.py` and `git diff --check`; do not claim this compiles a CAN library. Once implemented, use the real native, Python, package, example and Doxygen gates appropriate to the change.
- Reserve remote CI dispatch and waiting for major milestones: substantial behavior/API changes, cross-module integration, persistence, build/toolchain/dependency/workflow changes, and release work. Judge impact, not diff size.
- Small documentation, comment, formatting and low-risk isolated changes need appropriate local verification, not a manual CI run after every commit or push. Frequent synchronization does not mean repeatedly dispatching unchanged suites. Existing automatic triggers and required checks still apply.
- For a major milestone, verify required CI on the exact final commit. Fix confirmed failures without weakening tests, bypassing hooks or changing workflow triggers to avoid checks. Report failed, unavailable and unperformed checks honestly; an older green run does not verify a new commit.

## Git workflow: one branch for the promptset

- Before editing each prompt, inspect the branch, upstream, recent commits, staged changes and working tree. Fetch the configured remote and inspect incoming changes; fast-forward only when safe. Inspect local state even if the network is unavailable. Do not assume an old audit describes the current checkout.
- When starting execution of a **new promptset**, create one new dedicated branch from the inspected, up-to-date agreed base. Choose a clear name such as `promptset/motorcontrol-can`; if that name already exists, establish whether it belongs to the same execution before reusing it. Record the promptset identity, branch and base commit in its progress/handoff. Creating this branch and its matching branch on the configured remote is authorized.
- Keep that branch for the **entire execution of that promptset**, across numbered prompts, sessions and agents. A new chat, resumed stage or completed chunk is not a new promptset. Resume the recorded branch; do not create a branch/worktree per prompt or subagent, switch back to the base after each prompt, or merge the execution into another branch without authorization. A separate later promptset starts its own new branch. Promptset maintenance/audit work is distinct from starting library implementation.
- Assume concurrent work. Preserve unrelated files and hunks, including staged changes. Never stash, discard, overwrite or commit someone else's work to make the tree clean. A dirty worktree alone is not a blocker. Review the staged diff before every commit and stage explicit task-owned paths/hunks rather than using blanket `git add .` or `git commit -a`.
- **For every prompt, commit and sync its meaningful completed changes before finishing or moving to the next prompt**, after the relevant checks pass. Also commit and sync during a prompt whenever a coherent, reviewable part is complete and useful on its own. Examples include a checked codec with its tests/docs, a complete typed settings group, or a verified documentation update. Do not wait for the full promptset to finish.
- A checkpoint includes the code, relevant tests, documentation and generated updates that belong together. Do not commit broken intermediate steps, unverified behavior, success stubs or arbitrary half-files merely to increase commit frequency. A partially blocked prompt can still deliver tested independent chunks; keep the remaining scope explicitly pending. No-op/read-only prompts do not need empty commits.
- Routine **commit and push are authorized without another permission question**. Use clear commit messages describing the completed change. Sync means push to this branch's configured upstream and verify the remote branch contains the intended commits; a local commit or stale remote-tracking reference alone is not synchronization. On the new branch's first push, set its upstream to the same branch name on the established remote (for example `git push -u origin <branch>` when origin is the confirmed destination). Do not invent a remote or push the execution directly to the base branch.
- Fetch/recheck around synchronization. If the remote advanced, inspect and integrate safe changes on the same branch, rerun affected checks, then push normally. Never force-push, rewrite shared history, reset away work or bypass branch protections to resolve divergence. If a conflict, permission, network or remote problem prevents safe sync, preserve completed local work and report the exact blocker and unpushed commits; continue independent authorized work where possible.
- The lead owns shared Git mutations, branch integration, commits, pushes and required CI verification. Subagents use disjoint paths or read-only reviews on the same branch and return changes for integration; they do not race Git operations.
- Before handoff, verify branch/upstream and synchronization with a fresh fetch and ahead/behind comparison where available. Report actual commit IDs, sync result, relevant checks and any remaining task-owned changes. Do not claim a clean tree when unrelated work remains. Routine pushes do not authorize release tags, package publication or changing sibling repositories.

## Current assignment and scope

This checkout is currently an audited implementation promptset, not a working driver. Do not report planned APIs, tests or hardware behavior as implemented. The library package name is `MotorControl-CAN`; the proposed C++ namespace, include directory and CMake identity are `MotorControlCAN`. Implement only when instructed to execute the promptset. Preserve the original references and their provenance.

The accepted scope is one library for multiple manufacturers' stepper and servo drives. Common motion is motor- and protocol-neutral; CANopen/CiA402 is the first protocol backend and Lichuan C the first implemented family. Nanotec N5 (stepper and BLDC configurations) and maxon EPOS4 are mandatory design contrasts, not automatically supported products. Proprietary CAN and CAN FD require their own protocol/adapter milestones. Read `docs/multidrive_decision.md`; use `docs/prompts/ADD_DRIVE.md` when a concrete additional profile is selected. Do not split public APIs solely by motor technology or infer that every CAN controller speaks CANopen.

Read `docs/prompts/README.md`, `docs/prompts/COMMON.md`, the active numbered prompt, the applicable reports in `docs/audit/`, and the applicable parent `AGENTS.md` before work. The parent Projects CI policy applies. Local Markdown in this checkout is repository documentation; keep it here.

The concrete shared contracts are [CONVENTIONS.md](docs/prompts/CONVENTIONS.md), [DOCUMENTATION.md](docs/prompts/DOCUMENTATION.md), [MAINTAINABILITY.md](docs/prompts/MAINTAINABILITY.md) and [DELEGATION.md](docs/prompts/DELEGATION.md). Apply them in every stage. Preserve exact source-backed names; distinguish planned CAN additions from existing sibling behavior. Write plain engineering English in code comments, docs, progress and handoffs.

## Documentation and delegation

Update affected README/user docs, CHANGELOG, examples and Doxygen in the same stage as a public contract or behavior change. Establish the strict Doxygen gate in stage 03; do not postpone source documentation to release. Public comments explain units, validity, ownership, time, output-on-failure and uncertainty where relevant. Private comments explain non-obvious reasons. Prefer small direct functions and explicit states; do not create a generic framework or placeholder profiles to fill a planned tree.

The user authorizes subagents and model choices according to task complexity. Inspect the runtime's available models/efforts and limits; follow DELEGATION for bounded tasks, disjoint file ownership and independent review. Use normal coding models for settled implementation and stronger reasoning for conflicting protocol/state/ownership decisions. Fall back to the available model when overrides are unavailable. Include relevant checks and the single-branch policy in delegated tasks. The lead integrates shared contracts and final checks; worker success is not evidence that the combined tree passes. Routine commits/pushes follow the Git workflow above; this does not authorize hardware activity, release publication or sibling changes.

## Recurring source audit

For **every implementation, review or audit prompt**, inspect current `../MotorControl-RS` and the relevant sibling I2C implementations, callers, examples, tests, runners and CI. At minimum refresh MotorControl-RS, ADS1115 and INA228; select SHT3x-main, BME280, MB85RC, MCP45HVX1, PCA9555, RV3032-C7 and the stateless codec siblings according to the affected seam. Inspect current FieldCore-node read-only for ownership/integration changes. Record paths, revisions, dirty state, relevant behavior and intentional differences in the stage report. Do not rely solely on these historical audit reports or copy names from an old prompt.

MotorControl-RS is authoritative for common motion names/meaning; I2C libraries establish ecosystem style and owner integration, not a single universal lifecycle. Native CANopen names describe genuinely different services. Keep `Status`, protocol errors, drive state, command outcome and application health distinct. Never import sensor retry policy into motor commands.

## Ownership and portability

- Keep the installed core C++11, deterministic and bounded, with free functions, fixed value types and caller-owned storage/contexts. No Arduino/ESP-IDF/FreeRTOS/FieldCore includes, device I/O, clocks, sleeps, logging, heap allocation, background tasks, automatic retries or application persistence.
- Applications own CAN controller/stack instances, time, scheduling, queues, node bindings, caches, health policy, retries and physical machine workflows. One local owner receives and dispatches every frame on a physical interface; asynchronous heartbeat/PDO/EMCY handling continues during SDO transactions.
- Keep common axis identity independent of wire node IDs/channels/subaxes. Motor kind, feedback and supported modes are independent capability facts. CANopen network/drive services remain below common motion; other protocols do not receive invented NMT/SDO/statusword semantics. Default to one protocol domain per physical interface; sharing a bus needs explicit identifier/broadcast/format compatibility analysis.
- CAN framing preserves 11/29-bit identifiers and data/RTR distinctions; service-specific rejection is not a global transport limit. FD is an explicit future adapter/profile capability. Exact common targets convert to native integer/float representation with checked quantization. Do not assume every drive uses RPM, subdivision or encoder-count command units.
- Reuse exact unit semantics and operation-specific `prepare...`, `next...`, `advance...` vocabulary, checked typed read getters and terminal evidence in concrete contexts. Avoid invented generic result getters, speculative universal engines and runtime plugin systems. Evaluate a proven CANopen stack before choosing service implementation; keep stack/SDK dependencies outside the installed portable core.
- Keep board configuration/adapters and CLI in examples. Arduino and native ESP-IDF must use the same application and command semantics. Provide native usage independent of either framework. FieldCore remains read-only unless separately authorized for integration.
- Preparation and parsing publish outputs atomically on success. Document any explicit count-reset exception. Codec input buffers are borrowed during calls only; admitted work and retained results own their data.
- Keep exact units, coordinate frames, scale/origin, rounding and angle-path policy in the public API. A CLI or Python runner must not be the only place that implements a motor feature or conversion. Unsupported capabilities fail before work is admitted; a raw object accessor does not replace typed native coverage.
- Runtime diagnostics exercise or observe the ordinary public path. Raw/decoded CAN tracing copies owner traffic without consuming frames from normal dispatch or blocking service under console pressure. Larger traces are bounded and application-local; ISR/driver storage follows the actual SDK requirements. Measure memory and service gaps instead of assuming PSRAM or a particular board.

## Evidence and motor behavior

Use original vendor PDF pages and EDS together. Extracted tables can be wrong. Record object index/subindex, type, access, units, PDO capability, model/firmware scope, page and contradictions before implementation. Never turn a generic CiA402 definition or another drive's manual into a CL86-C capability. Keep documented, implemented, host-tested and hardware-qualified coverage separate.

No implicit enabling, movement, homing, fault clearing, NMT reset/start, parameter save, node/bitrate change or PDO reconfiguration on startup, selection, probe, recovery or cached reads. Local cancel/recovery is not a motor stop. CAN TX success/ACK is not SDO success, drive acceptance or motion completion. Preserve execution uncertainty after an unconfirmed write and never automatically replay motion or re-enable after reboot/recovery.

Distinguish controller state, CANopen NMT state, CiA402 drive state, mode, heartbeat freshness, fault events and application health. Boot-up/reset/bus-off/rebinding/configuration changes invalidate dependent evidence and queued work. No health recovery clears a drive fault implicitly. An NMT stop is not a CiA402 quick stop. Host heartbeat observation does not establish drive action on host loss.

The RS bench authorization and wiring are not CAN bench facts. Host-only work can proceed without hardware. Before live CAN/motion work establish the actual board, transceiver, drive/motor/firmware, wiring, node/bitrate and applicable user authorization. Do not guess defaults from the RS setup. Missing hardware blocks qualification, not independent implementation and testing.

External I/O may be unused. An unconnected terminal does not mean its configured function is disabled. Require physical signals only for operations that need them; never silently disable limits, stops or input assignments to make a motion test run. Use only this profile's documented function values and actual fixture evidence.

## Failure investigation and hardware evidence

When a host or hardware test fails, retain the exact revision, fixture/configuration, intent, raw traffic, operation/step identities, timing/service gaps, fault/uncertainty and resource evidence needed to distinguish causes. Separate observations from hypotheses. Keep diagnostics bounded and account for their timing/load effects.

Reproduce deliberately when the setup and authorization allow it. A later passing run, guessed explanation, longer timeout or hidden retry is not a fix. Establish the cause, apply the simplest correct fix or focused refactor, add a meaningful native regression when representable, then rerun the failing case and an appropriate short regression. Deliberate reproduction is never permission to replay an uncertain motor write automatically.

When new behavior reaches an authorized CAN bench, run a short existing-feature regression plus the focused new checks. Short tests do not replace sustained-load, electrical or per-family qualification. If evidence or hardware is missing, keep that part unresolved with the precise next measurement; finish independent software work and report what was not tested.

## Reference and generated-file maintenance

Preserve original manuals, EDS, source snapshots and hash manifests. Check original PDF pages when text extraction loses tables, figures or notation. Review any changed upstream document before replacing an existing snapshot, retain its provenance, and keep conflicts visible in the reviewed ledger. Never execute downloaded vendor tools as a side effect of preparing documentation.

Use one reviewed object ledger for generated declarations/access policy and coverage. Run the actual generator/check mode once implemented; do not hand-edit generated files or fill undocumented gaps with invented objects. Keep descriptive catalogue strings optional for minimal consumers. Vendor material is not covered by the library's software license and is excluded from distributable core packages. Separate disposable build output from retained audit/bench evidence; never delete evidence as cache cleanup.

## Work and verification

Keep changes scoped and use the CI/Git policies above throughout each prompt. Establish host gates early and maintain generated-file freshness, packaging and example parity. Tests must exercise observable contracts and adversarial evidence, not merely mirror helpers. Never weaken tests to make a stage green.

Maintain a current handoff rather than a transcript: completed behavior, actual verification, source/firmware uncertainty, next executable work, branch/commits and sync status. Continue through already-authorized stages without routine confirmation. When one feature is blocked, keep its obligation visible and proceed with independent work; do not call a partial stage complete because its current subset builds.

Maintain a stage report, object/API/CLI coverage, backlog and roadmap as work proceeds. Mark software checks, device evidence and remaining gaps independently. For major implementation milestones verify required CI on the actual final commit when a remote exists; report unavailable checks honestly. Documentation-only promptset changes need local validation, not remote CI.
