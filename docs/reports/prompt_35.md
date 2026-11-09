# Prompt 35: finite velocity operations

## Bench run, 2026-11-09

The bench `mode3` route wrote `6060=3` once on node 2 while it was released and stationary; `6061` read back 3 and nothing moved ([report](2026-11-09_bench_r2.md)): one Q11 data point. No velocity run: not unattended, and its start and stop qualification (Q03, Q12) is not established by any record.

## Scope

The conditional finite velocity software and application owner integration are
implemented. Original prompt35 remains **Partial** because its confirmed-device
scope needs Q03/Q11 startup and mode-change facts, reverse range and Q12 mode3
stop evidence. No retained specimen trial establishes them. A fake test does
not close those gates. Continuous velocity, torque/current, cyclic streaming,
homing, CLI routes and a second service engine are outside this change.

The consumed artifacts are prompt33's exact rate conversion, prompt31's stop
actions and prompt24's owner urgency/retirement path. Their outstanding physical
qualification does not prevent conditional host implementation. Nothing here
enables a gated production operation by default or fabricates a release command.

## Source refresh

Entry revision `865e0a2`, existing `promptset/motorcontrol-can` execution branch,
upstream `origin/promptset/motorcontrol-can`. Fresh fetch: 0 ahead/0 behind.
The three unrelated untracked `.vscode` JSON files were preserved.

| Source | Revision / state | Adopted or rejected behavior |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean | `src/profiles/ess_rs/Velocity.cpp::prepareVelocity` anchors duration at the successful preparation call. `beginStop` checks strictly later than due+interval. `examples/probe_cli/ProbeApp.cpp` consumes the owner terminal before `serviceVelocity`. Adopt timer, first-failure retention and separate later-stop identity; reject ESS native widths, frames and device timing as CAN authority. Timing tests reopened, including delayed service, poll coverage, observation across due and preserved failure. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Identity/admission seam unchanged since prompt34: `Rs485MotorModule::configure` and `RodCycleCoordinator::consumeMotor` separate binding/configuration, operation/step and stop outcomes. No velocity API, sibling change or integration claim. |

Original EN86/ZH86 PDFp13, dictionary/stop pages and EDS `60FF`, `6060`,
`6061`, `6083`, `6084`, `6040` were reopened. Local rendered pages are disposable
audit output; originals and manifest hashes are unchanged. Source-confirmed:
rpm destination, positive documentary 3000rpm ceiling, U32 ramp times up to
2000ms, and nominal10ms controlword spacing. Contradicted/unresolved: enable
order, signedness versus operating range, zero ramp/target meaning and enabled
mode changes. The conditional software chooses ZH86 target-before-enable only,
not a combined sequence. OP86 retains its 57/86 heading limitation. Physical
measurement in this prompt: **none**. DEC-8 mode3 stop is project policy, with
`predecessorDocumented=false`, not vendor evidence.

## Delivery

- `VelocityOperation.h` adds finite intent and interruption/error vocabulary;
  `Axis.h::VelocityRequest` remains pure. The concrete profile owns preparation,
  checked integer-rpm conversion, state progression, observations, fixed timing,
  setup/start/stop terminals and uncertainty.
- `AxisSession` reuses the ordinary owner, two alternating state-read buffers,
  retained settings and urgent stop. Actual controller handover rechecks rate
  provenance and cutoff. Scheduled stop and external interruption are distinct;
  cancellation sends no implicit stop. A later stop cannot rewrite a terminal run.
- Mode3 STOP accepts only the latest acknowledged `000F` with fresh enabled/running
  mode3 evidence; no enabling preamble. Position-mode released/latched-running
  completion is not reused for mode3. Unresolved-route best effort remains one
  `000B`, unknown execution and no qualified reply.
- Typed raw `60FF` readback uses the existing driver-read engine. It stays RAW and
  generally Q20-unqualified; packet15's one-per-enable accepted prerequisite is
  not broadened. Velocity/ramp writes retire retained motion-parameter snapshots
  without advancing configuration generation.
- Direct and real-owner fake tests, installed offline example, package inventory,
  generated read accounting, Doxygen and user guides accompany the change.

The [velocity guide](../velocity.md) owns the API/timing contract and exact next
conditional acquisition obligations. No table/sequence is duplicated into a
generic profile framework. Parallel work separated original evidence/core,
typed reads/direct boundaries, and stop/owner adversarial tests; the lead owns
integration and Git. Independent review checks actual code and failures.

## Verification and review

Focused tests and independent review found and corrected a noted-work handover
rejection, an unjustified release requirement after enable word7, stale released
state reused after a halt, and an overly broad source-mode qualification.
Controller-proved first-word retirement now refines the tentative stop obligation;
prior possible effects remain. Start-work deadlines and admission budget checks
are bounded by stop due, independently of the original stop-settlement deadline.
HALT-only requests cannot silently authorize a best-effort QUICK_STOP; the
selected kind is checked in both the session and direct stop-settlement API.
Tests were not changed merely to agree with the implementation.

| Check | Result and retained evidence |
| --- | --- |
| Full GCC verifier | Passed:104 registered native/Python contracts, generated/version/inventory checks, isolated headers, export/relocated-install consumers and strict Doxygen. `.build/p35-final/summary.json`; full run used GCC15.1, Python3.12.10, CMake4.0.1 and Doxygen1.13.2. |
| Final integrated native regression | Passed all104 after the final stop-kind and deadline corrections: `ctest --test-dir .build/prompt35/native --output-on-failure -E '^package_contract$'`. Direct velocity1881 checks; real-owner velocity13341 checks. Tests cover stale admission, pending/accepted TX, bounds, cancellation, faults, boot, pressure, deadline races, stop-kind policy and retained outcomes. |
| Package/API documentation | Full verifier passed source/export/install consumers including `velocity_preview`; final `doxygen Doxyfile` also passed. Extracted30 public headers and113 documented free-function names. |
| S2 compile/link | Passed current source through `check_firmware.py --target s2`, retained `.build/p35-s2-final/summary.json`. Uses the existing reviewed IDF5.5.5 environment; no flash or port open. |
| S2 stack | Passed `check_stack.py --strict --target esp32s2` against that compile database, `-Os` and `-Og`; `.build/p35-stack-final/report.json`. No frame over2048 or known chain over6144 bytes. `VelocityContext=3056`, `AxisSession=31880` bytes. Chains are compiler-derived lower bounds with reported unknown/indirect calls, not measured task high-water usage. |
| Evidence/integrity | Generator45 tests and ledger82 tests passed; generated output current, promptset integrity and diff checks passed.140 immutable reference artifacts unchanged; staged and committed blob verification accompanies synchronization. |

The final native rerun includes the corrections found during the first full
build, rather than treating an older green executable as verification. The first
full attempt failed an intermediate owner-test expectation about boot outcome;
the final test distinguishes native `INVALIDATED` from a local interruption.
The shorter service cutoff changed the owner contract explicitly: tests retain
both later-cutoff rejection and earlier-cutoff positive expiry, including urgent
work. No test, hook or workflow was bypassed or weakened.

RS and FieldCore revisions and clean state were rechecked at exit and remained
unchanged. The implementation stays on the existing execution branch; unrelated
`.vscode` files are excluded. Exact commit/upstream and final automatic CI results
are reported at handoff. Local Windows cannot run Linux ASan/UBSan or the Clang
matrix; the existing CI verifies those plus Arduino/IDF/S3 and component builds
on the final commit. No new manual workflow dispatch is needed for the automatic
push workflow.

## Remaining evidence

Q03/Q11: obtain one independently bounded target-before-enable positive mode3
trial from an actually released stationary state. Capture each6/7/15 transition,
fresh mode/status/monitor/feedback and independent shaft observation. An enabled
specimen has no assumed safe6060 transition or fabricated release path. Qualify
the selected stop separately with an independent cutoff; do not use the disputed
stop as the only experimental bound. Negative rates, zero, all rates, other
firmware tuples and the other stop kind require their own evidence. General
60FF acquisitions retain Q20. The prompt29 conditional route may resume these
measurements without waiting for prompt47; software tests do not substitute for
them. No serial port, controller or drive was opened by this implementation.

## Independent follow-up audit

The [fresh prompts 34/35 audit](prompts_34_35_audit.md) rechecks the original
requirements against code and tests, records the velocity poll-budget correction
and final verification, and keeps firmware/physical gates separate.
