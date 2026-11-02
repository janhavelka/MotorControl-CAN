# Prompt 31: conditional stop operations and owner integration

Subsequent [independent audit of prompts 31/32](prompts_31_32_audit.md) rechecked
the actual implementation, source evidence, owner paths and tests. The original
scope and firmware gates below remain in effect.

Date: 2026-11-02. Promptset v2 continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, base `7878f7c`. Entry HEAD was
`9b3088abb4e28b3952d9c6d2548c0e5f278bae89`; fetched upstream was 0 ahead/0 behind.
The index was empty. Three untracked `.vscode` files and the concurrent
`docs/audit/PROMPTSET_AUDIT_34_50_2026-11-02.md` were preserved and excluded.
During work, another session committed and synchronized `41f8969` adding only
`CLAUDE.md` importing AGENTS. That commit is preserved on this same branch; it
changed no implementation or verification input.

## Delivered scope and remaining gates

**Partial.** The source-backed, conditional position-mode stop lifecycle is
implemented through the real owner. It does not complete unsupported native
transitions or qualify firmware/physical stopping. Inherited prompt 24 Q20 and
prompt 30 enable/release obligations remain unchanged. No unsupported stub,
hidden enable, option write, PDO bypass, new service engine or hardware action
was added.

`ActionOperation.h` supplies protocol-neutral action state, execution and
completion vocabulary. Lichuan `Actions.h/.cpp` implements `prepareStop`,
`nextAction`, `noteActionTransmission` and both `advanceAction` forms. The
[stop guide](../stop_actions.md) owns the transition table, eligibility,
observation and handoff rules. It was written before integrated verification;
the implementation header specified transitions before source implementation.

| Original requirement | Delivered behavior / exact remaining boundary |
| --- | --- |
| 1. Transition contract before implementation | Table distinguishes HALT/QUICK_STOP, exact 000F predecessor and fresh mode/monitor evidence, one command, new observations and unknown queue/torque/reference consequences. No 000F preamble. |
| 2. Concrete action lifecycle | Single 010F or000B download, retained protocol acknowledgement, finite fresh typed observation cycles and controller-reported not-running completion. Other initial states/modes and repeated stop from halted/released states reject before work. |
| 3. Real urgent owner path | Tests use public profile work, `admitUrgent`, actual CANopenNode service, the same receive dispatcher and reserved request/result slot. No test-local stop codec or fake installed driver. |
| 4. Stale queue retirement | Explicit cancellation of every old request, retained results, newer operation IDs, definite local retirement versus submitted uncertainty and permanent SDO quarantine. Other-node stop/event progress remains independent. |
| 5. Interrupted identity and later admission | Original request terminals remain unchanged. Old operation IDs reject; the caller consumes prior controlword eligibility at possible stop TX and retains the latest command terminal. A new ID cannot rehabilitate stale prerequisite history. |
| 6. Time, fault and cancellation | Immutable inclusive processing cutoff; explicit local cancellation/fault/invalidation; lost reply and read failures preserve original command evidence and uncertain effects. No replay or terminal rewrite. |

The supported predecessor is deliberately exact. Documented target-trigger
values 001F/003F/005F/007F, halted/quick-stopped/released states, modes 3/6 and
unknown last command do not gain stop transitions by analogy. Obtain an
applicable manufacturer state-transition contract or independently reviewed
finite fixture evidence for those cases. This is still owned prompt-31 work.
`605D` option meanings, braking/latency, target disposal, torque and reference
effects remain Q12; no generic quick-stop object fills them. There is no proven
alternate SDO/PDO route for an unresolved channel. Missing firmware binding and
read-effect evidence remain Q01/Q20. The software's reported-not-running result
does not close any of these physical or firmware obligations.

## Fresh source audit and original evidence

All sibling checkouts below were clean at entry and exit; no fetch, edit or
unchanged suite rerun was performed in them. RS/I2C/FieldCore revisions match
the final 29/30 audit; PCA9555 is the additionally selected cancellation seam.

| Source | Revision | Actual behavior inspected |
| --- | --- | --- |
| MotorControl-RS | `6145bd8a12eda501cd1b3dcbf561b8ce39373a61` | `ActionOperation.h`, ESS `Actions.cpp`, `ProbeApp::startAction`, `RtuBusOwner` cancellation, `bus_scheduling_test`, `actions_test`, `velocity_app_test`: separate execution/completion, urgent pressure, old continuation retirement and independent interrupted results. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | `02_owner_safe_poll/main.cpp`: owner lock/time and matched terminals; no sensor retry imported. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `_cancelJob`, `_invalidateJobHardwareState`, `invalidateHardwareState` and precise-effect cancellation tests: withdraw validity, retain partial effects/first cause. |
| PCA9555 | `498178e4d9e4acb6fab7d8b816ad5021efe8806a` | `_requestTerminal`, `cancelOperation`, `_finishOperation` and native cleanup/partial-result tests: owed pointer cleanup differs from CAN retirement. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `Rs485Task::submitMotorCommand/serviceMotorControl`, `Rs485MotorModule` outcome projection and host motor-runtime urgent tests: dedicated urgent lane and retained original outcomes. Read-only. |

Reopened original EN86/ZH86 p12, OP86 p10, EN86 p16-17, ZH86 p17-18 and EDS
`6040/6041/605D` sections. Source-confirmed: U16 command shape, stop arrows,
consistent monitor bits 2/3/4 and mode 1. Contradicted/limited: EN86 omits halt;
6041 alarm/running layouts differ; OP86 internal headings identify 57C despite
its 86C cover/name. Unknown: 605D options and absent 605A/6085 semantics,
broader state transitions and physical consequences. Nothing was physically
measured. No original bytes or contradictory claims were replaced.

The canonical ledger's two stop-function entries now link their conditional
implementation scope. Whole-function implemented/host-tested/qualified flags
remain false because broader obligations remain. Generated documentation was
refreshed through the actual generator, without relaxing write policy or gates.

CAN intentionally retains its inclusive processing-time cutoff and SDO
ambiguity policy rather than copying RS receive-history settlement. RS common
names are reused; native HALT/QUICK_STOP names avoid falsely claiming RS
configured-deceleration/direct effects. One fixed context reuses existing typed
reads; no duplicate observation/service engine or generic trajectory layer.

## Review and verification

`p31_ecosystem` (`gpt-6.1-sol`, high) audited siblings and exclusively implemented
the shared action declarations/lifecycle. `p31_owner_review` (`gpt-6-astra`, high)
independently reviewed owner retirement and owned the adversarial real-owner
suite. `p31_stop_semantics` (`gpt-6-astra`, high) independently reopened sources
and reviewed lifecycle/uncertainty. Lead owned contract integration, source
checks, packaging, documentation, combined verification and Git. No worker
performed Git mutations or hardware activity.

Independent review required fresh monitor observations rather than declaring
all observation semantics blocked by 605D, explicit treatment of released
reports as outside supported scope, retained unknown read effects and consumed
old-control eligibility. The final step identity may complete an observed stop;
only a need for another identity fails exhaustion. These distinctions have
focused native regressions.

| Check | Result |
| --- | --- |
| Focused real-owner stop suite | PASS: 7,824 checks, strict C++11 Release; both commands, all five observation-field cancellation/abort positions, WAITING cancellation, actual EMCY, boot-up, unknown enqueue, full pressure, late replies, aliases and time/identity boundaries |
| Combined native full verifier | PASS: 54/54 registered CTests on GCC 15.1.0 / CMake 4.0.1 / Python 3.12.10 |
| Package isolation | PASS: source/exported and relocated C++11/C++17/no-RTTI consumers, 25 installed headers; new stop call linked without stack/SDK/network dependency |
| Strict Doxygen | PASS: 1.13.2, including new public declarations and stop guide |
| Ledger / generated freshness / promptset | PASS: 99 indexes, 202 entries, 53 manual items; 137 original hashes preserved |
| Formatting / diff | PASS: clang-format 14 on new core/test files; whitespace check |
| Physical drive / S2 live port | NOT RUN; selected transitions and fixture prerequisites remain unqualified |

Combined evidence: `.build/p31-full/summary.json`, run
`20261102T120237582438Z-80032-8u17vtl2`. The focused test build is
`.build/p31-stop-review`. Local Linux sanitizers are unavailable; exact-commit
Linux CI supplies that check. Native x64 GCC `sizeof` measurements are 3,848
bytes for `ActionContext`, 1,944 for `ReadContext`, 520 for `StateObservation`
and 216 for `SdoCompletion`. These are ABI storage sizes, not target stack
high-water measurements; actual firmware task integration still needs its
existing resource qualification. No heap allocation was added to the core.

The initial integration build encountered the test author's unfinished `main` registration
and failed unused-function warnings; that intermediate run is retained under
`.build/p31-quick/20261102T115114754448Z-35432-bnama6lv`. It did not indicate a
device or production-runtime failure, and no check was weakened.
The first full run then exposed an omitted independent repository-test fixture
inventory for the new files. Both file and actual-command fixture inventories
were updated; all 15 repository-checker tests and the complete verifier pass.
That failed run remains in `.build/p31-full/20261102T120002743031Z-25416-fn9_xp34`.

The final commit and exact-commit CI/sync result are reported in the handoff.
No previous green CI run verifies this new behavior. No firmware was uploaded,
no serial/CAN port was opened and no motor command was sent during this prompt.
