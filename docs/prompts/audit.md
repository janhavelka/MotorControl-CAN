# Promptset audit — 17 November 2026

Audited revision: `b6d5ffa1` on `main`, synchronized with `origin/main` at intake;
working tree and index were clean. This report reviews the **current 76 numbered
prompts**, their shared contracts, execution metadata and active bench supplement.
It is an instruction review, not implementation or new hardware qualification.
The findings below are **open corrections**, not claims that the code was fixed.

The main problems are contradictory new requirements and missing integration
contracts. The reviewed Lichuan object widths and mode tables generally remain
correctly qualified. Do not replace existing explicit device gates with generic
CiA402 assumptions to satisfy a contradictory prompt.

Priority: **P1** can cause unintended device work, false confirmation or loss of
critical control evidence; **P2** blocks correct implementation or leaves an
important behavior undefined; **P3** is a factual or acceptance-test correction.
There are **21 confirmed findings: 5 P1, 12 P2 and 4 P3**.

## Findings and corrections

### A01 — P1: A different-object SDO marker does not establish remote retirement

**Where:** [55](55_self_repair.md), lines 37–41, 77–78 and 91–92.

The revised prompt reopens a timed-out channel after a client abort, local TX
retirement and a different-object upload. It makes explicit risk acceptance
conditional on disabling the marker, although it states Q18 remains unqualified
in both modes. The missing correlation premise also matters with the marker enabled:
`A(x)` times out, marker `B` succeeds, a new `A(x)` starts, then the old reply to
`A(x)` arrives. Ordinary SDO replies contain no local request ID; a download ACK
does not echo its written value.

Current [SdoClient.cpp](../../adapters/canopennode/SdoClient.cpp),
`checkDrain` at lines 324–339, requires explicit sole-client/ordered-server
premises and excludes retained object pairs. The [service contract](../canopen_services.md),
“Settling and reopening a route,” records that Lichuan evidence does not establish
the required remote ordering/duplicate-retirement premise. A marker cannot prove it.

**Fix:** preserve those prerequisites, or explicitly introduce an application
risk-accepted reuse policy. Describe its uncertainty with the marker both on and
off; do not silently set unproved premises true or promise detection of an
indistinguishable reply. Keep local TX retirement and historical effects separate.

**Acceptance:** extend the existing [reconciliation tests](../../test/sdo_reconciliation_test.cpp)
with the `A → timeout → B → new A → old A reply` sequence under each policy.
Report lost correlation assurance honestly; a marker success is not proof of it.

### A02 — P1: `release ID` already means result release, not motor release

**Where:** [57](57_regular_drive_operations.md), implementation item 5, line 99.

The proposed physical `release ID` command collides with the existing command
that frees an operation's retained result. [ProbeConsole.cpp](../../examples/common/ProbeConsole.cpp),
lines 84–85 and the RELEASE dispatch around 1801–1882, implement result release.
[42](42_cli_motion_routes.md), item 2, explicitly reserves `motor-release` for the
physical action. Axis and operation IDs can have the same numeric value.

**Fix:** retain `release OPERATION_ID` unchanged. Use `motor-release AXIS_ID`
through `prepareRelease`; update the command inventory, help, schema and runners
together. Never infer which meaning was intended from which ID happens to exist.

**Acceptance:** when an axis and operation share an ID, ordinary result release
emits no CAN frame; motor release uses the axis and the reviewed action path only.

### A03 — P1: Lost word-15 acknowledgement is not the word-7 release case

**Where:** [72](72_enable_fault_guards.md), implementation item 2 versus Verify.

The implementation permits the word-7 guard only after word 7 was acknowledged
and **before word 15 was sent**. Its test list also demands `6040=0000` after a
lost word-15 reply. In that case `000F` may already have enabled the drive and
the SDO route may be unresolved. The old word-7 report no longer establishes the
current state. The retained [fault-state report](../reports/2026-11-13_enable_fault_state.md)
supports `0000` once from `8033`, after refusal before word 15; it does not support
the expanded transition. The unresolved-route stop exception permits `000B`, not
arbitrary release writes.

**Fix:** split definitely-not-submitted word 15 from possibly-transmitted word 15.
Only the former can use the specified switched-on release under its reviewed
conditions. The latter retains UNKNOWN and uses a separately qualified action
for the actual state and route, or remains unresolved. Do not bypass admission.

**Acceptance:** test cancellation before handoff, controller acceptance, physical
TX and lost ACK separately. Old word-7 evidence must not certify release after a
possibly executed word 15; retain both the enable and follow-up outcomes.

### A04 — P1: SP706R WDO is not a latch cleared only by a new WDI edge

**Where:** [69](69_application_io_watchdog.md), item 1; [host-loss guide](../host_loss.md),
SP706 selection and output-hold description.

The prompt names SP706REN-L/TR as holding STOP until WDI toggles again. The
[MaxLinear SP706R/S/T datasheet, Rev. 3.0.2](https://www.maxlinear.com/ds/sp706r_sp706s_sp706t_sp708s_sp708t.pdf),
PDF pages 10 and 14 (printed pages 5 and 9), says the timer also clears on RESET
or high-impedance WDI; floating WDI disables it. WDO can return high after supply
recovery without a new feed edge. A fake implementing the prompt's prose would
therefore prove the wrong circuit behavior.

**Fix:** correct the chip description. Require the combined circuit and application
RUN permission to hold STOP through reset, watchdog disable and brownout until
deliberate rearming. An independent RUN gate or latch may suffice; replacing the
chip is not automatically necessary.

**Acceptance:** model and later measure WDI high impedance, MCU reset, supply
recovery and any WDO-to-MR connection. WDO alone must not reauthorize RUN.

### A05 — P1: Entire axis sessions are not cold PSRAM data

**Where:** [76](76_psram_storage_placement.md), delivery item 1, lines 26–28.

The prompt keeps urgent/stop storage internal, then allows “sessions, retained
results” in PSRAM. Actual [AxisSession.h](../../examples/common/AxisSession.h),
around lines 788–808, embeds pending and urgent work, two stop contexts,
best-effort stop state and live move/replacement contexts.
[StartupColdStorage.h](../../examples/common/StartupColdStorage.h), lines 30–32,
explicitly excludes active axis contexts and reserved stop storage.

**Fix:** keep complete existing live sessions, active contexts and reserved
stop/urgent state internal. Ordinary completed results may move externally once
detached from hot owner/session paths, with stable lifetimes and reviewed placement.
Detached history copies, diagnostic rings and suitable scratch are also candidates.
If a later split is useful, name the exact members and their lifetimes before
moving them; an object is not cold merely because some fields retain history.

**Acceptance:** an allocation/placement inventory proves all stop-critical
objects remain internal. External-memory refusal leaves ordinary and urgent
operation available at the explicitly accepted capacity.

### A06 — P2: Retry classes have no truthful default for unqualified reads

**Where:** [55](55_self_repair.md), delivery item 4 and implementation item 5;
[74](74_reference_self_repair_example.md), retry policy.

Every operation is to be R (non-consuming read), W (idempotent setting) or M
(listed motion-like commands). However, the [reviewed ledger](../reference/lichuan_objects.json)
and [service contract](../canopen_services.md) retain unestablished read effects
for all 202 Lichuan entries. DEC-1 allows selected reads as accepted risk; it
does not prove they are non-consuming. Fault/history reads have narrower scope.

**Fix:** default to a non-retryable/unqualified class. Classify retryable work
only with the required evidence or an explicit application repeat-permission
decision. Preserve `readEffect` independently; accepting a repeat must not
relabel an unknown effect as harmless. Reconciliation still precedes any retry.

**Acceptance:** distinguish an accepted-unqualified state read, fault/history
read and a known non-consuming fake read. Only the selected repeat policy may
retry, inside the original deadline and without changing effect provenance.

### A07 — P2: TPDO-fed state needs a multi-frame observation contract

**Where:** [73](73_timing_freshness_guards.md), design item 4 and implementation item 5.

The listed state fields `6041`, `2100`, `6061`, `6064`, `606C` occupy
**2 + 2 + 1 + 4 + 4 = 13 bytes** in the original
[EDS](../reference/vendor/LC_StepDriverCAN_V1.1.eds). They cannot fit one Classical
CAN PDO (8 bytes), and five entries exceed the retained EDS's four-entry mapping
limit. Multiple PDOs are possible, but arbitrary latest values do not establish
the single coherent cycle required by the same prompt. An event-driven stream
has no inherent shared cycle tag; Q27 also leaves same-SYNC identity unqualified.

**Fix:** define the minimum field set for each operation. Prefer one reviewed PDO
where that set fits. Otherwise define the multi-PDO acquisition window,
per-field causality/provenance, completeness and fallback behavior. Do not mark
a cache assembled from unrelated arrivals as one acquired report.

**Acceptance:** missing, reordered and duplicate components, one component before
the command, stale velocity beside fresh status, and old-map same-ID bytes must
not qualify a fabricated complete report. Use the established SDO fallback.

### A08 — P2: No motion deadline must not mean an unlimited service attempt

**Where:** [73](73_timing_freshness_guards.md), design items 1–2 and implementation items 2–3.

Removing the queue budget while charging queue time only to an optional operation
deadline leaves queued work without a finite cutoff when that deadline is off.
Current [CanBusOwner.cpp](../../adapters/owner/CanBusOwner.cpp) admission/expiry and
[SdoClient.cpp](../../adapters/canopennode/SdoClient.cpp) transfer handling require
finite queue/service cutoffs. Homing also has a separate `maxPolls` termination
bound, so removing only `deadlineUs` does not implement the stated behavior.

**Fix:** distinguish unbounded motion duration from bounded service attempts.
Each yielded SDO still gets a finite queue/TX/response lifetime. A single user
timeout may derive these limits; a second exposed knob is not required. Define
the remaining poll/token exhaustion behavior and update the shared deadline
contract alongside the API. Do not propagate `UINT64_MAX` as every inner timeout.

**Acceptance:** deadline-none moves/homing with a stalled queue, pending TX or
lost response end that service attempt predictably, preserve effects and leave
another node serviceable. Test poll/identity exhaustion explicitly.

### A09 — P2: Optional stop guards cannot remove physical or storage prerequisites

**Where:** [73](73_timing_freshness_guards.md), design item 5: a stop is never refused
for a guard or cap.

This is too broad for fixed urgent slots, retained results, invalid bindings,
unretired TX and HALT's state prerequisites. Current
`CanBusOwner::admitBestEffortStop` legitimately refuses unavailable transport or
route conditions. Repeating `010F` from an unsuitable state can enable; the
promptset already distinguishes it from an explicitly requested `000B` fallback.

**Fix:** say optional application policy guards cannot suppress an **otherwise
admissible** stop. Preserve explicit capacity/transport failures and retained
outcomes. Do not silently substitute a different stop kind, evict results or
bypass TX retirement to meet an unconditional acceptance promise.

**Acceptance:** full urgent/result capacity, unavailable transport, invalid
binding and unsuitable HALT predecessor produce honest outcomes; unrelated
ordinary pressure still cannot consume reserved stop capacity.

### A10 — P2: The derived move deadline has no rule when distance cannot be known

**Where:** [73](73_timing_freshness_guards.md), design item 2 and Verify.

The proposed default is distance divided by speed plus ramps and margin. Current
native requests can specify command increments and RPM without a qualified
increments-per-turn conversion. An absolute endpoint is also not displacement
without a matching current position/reference. [Axis.cpp](../../src/Axis.cpp),
`conversionConfig` and `preparePosition`, preserve those distinctions; native
increments do not imply mechanical scale. The existing ramp-law uncertainty also
prevents treating a duration estimate as a vendor guarantee.

**Fix:** derive an estimate only when the required position, scale, coordinate
basis and applicable ramp model are available. Mark it as host policy. Otherwise
require an explicit deadline or use the explicitly selected `none` policy; do
not invent a subdivision, assume the current position is zero or reject all
otherwise valid native motion merely because an estimate is unavailable.

**Acceptance:** unscaled native moves, absolute targets without a current
coordinate, zero displacement, reverse travel and arithmetic overflow have
documented outcomes. Estimated and explicit deadlines remain distinguishable.

### A11 — P2: Lossy output must not include accepted-operation results

**Where:** [75](75_runtime_node_table.md), delivery item 4 (“whole-record discard”);
[68](68_application_task_structure.md), implementation item 3 (results in event queue).

P68's own earlier contract says terminal results stay in reserved slots and the
lossy event queue contains notifications only. The implementation list then puts
results in that queue. P75's per-response discard instruction has no class
restriction. The current [CLI contract](../cli_contract.md), lines 45–54, reserves
output before mutation, retains terminal suffixes and requires delivery before
release. A dropped terminal can destroy correlation or strand result storage.

**Fix:** name three classes: retained admission/terminal/control results, lossy
diagnostic notifications, and deferred bulk dumps. The event queue carries a
result-available notification, not the only copy of a result. Apply dropping only
to permitted diagnostics or unadmitted refusals under the existing contract.

**Acceptance:** saturate dumps/events while admitting both axes and urgent stops.
Every accepted operation retains one terminal until explicit release; diagnostic
loss is counted and cannot make an operation appear complete or disappear.

### A12 — P2: Moving policy from prompt 55 to 74 left earlier consumers unchanged

**Where:** [55](55_self_repair.md), revised scope; [66](66_hardware_regular_operation.md),
objective and soak; [68](68_application_task_structure.md), boundaries;
[71](71_hardware_application_examples.md); [74](74_reference_self_repair_example.md).

P55 now supplies primitives only. Presence, retry, reconciliation and optional
fault-clear policy belong to P74, which depends on P68 and P73. P66 still promises
to qualify those policies as output of prompts 55–63. P68 still refers to P55's
configured policies, and P71 counts self-repair in its earlier campaign.

**Fix:** give P68 hooks, observations and explicit recovery without a second
supervisor. Split P66/P71 acceptance into available primitive/application checks
and policy qualification resumed after P74. Record that evidence gate in the
index and progress metadata. Do not add P74 as a dependency of P68: that creates
a cycle. Keep one policy implementation in P74.

**Acceptance:** the dependency/ownership tables identify a real producer for
every required artifact. Earlier campaigns cannot claim policy coverage before
the supervisor exists, and P74 does not duplicate an earlier policy engine.

### A13 — P2: Startup cannot require a boot-up/heartbeat from a warm silent drive

**Where:** [70](70_startup_settings_reboot.md), implementation item 2.

It waits for boot-up or heartbeat before identity/configuration checks. The
original EDS defaults `1017` to zero, and the retained
[November 16 campaign](../reports/2026-11-16_hil_campaign.md) observed zero after
power cycling both drives. An MCU starting later can miss the one boot-up frame
and then receive no heartbeat from an otherwise healthy drive.

**Fix:** after a bounded passive window, perform a reviewed addressed
identity/state probe through the owner. Keep silent, absent, mismatched and
unresolved states distinct. Do not reset the node, start it or change heartbeat
configuration merely to manufacture the awaited event.

**Acceptance:** MCU cold start with warm drives and disabled heartbeat can reach
checked readiness through reads. An absent node stays unavailable; a timed-out
probe preserves channel uncertainty and causes no implicit mutation.

### A14 — P2: Prompt 57's implementation omits its required switched-on release

**Where:** [57](57_regular_drive_operations.md), release objective versus implementation item 3.

The objective requires the measured `8033 → 0000` release and explicitly says
word 6 from `8033` is unmeasured. The implementation list says word 6 and refusal
elsewhere. Following it literally omits the release consumed by prompt 72 or
encourages replacing the measured word with an unmeasured one.

**Fix:** use one predecessor/command table in the objective, implementation and
tests. Scope `0000` to its reviewed state/mode/loop/identity evidence, and qualify
word 6 separately for its supported predecessors. The `8010 → 0080` reset
observation is not a generic motor-release implementation.

**Acceptance:** assert exact words for each supported predecessor and rejection
of unmeasured combinations before TX. P72 consumes the same reviewed table.

### A15 — P2: MCU-mediated switch handling has no one-round-trip latency guarantee

**Where:** [69](69_application_io_watchdog.md), implementation item 2.

It describes an MCU limit/home input mapped to a CAN stop as one CAN round trip
slower than a drive input. The actual path includes debounce, task scheduling,
in-flight service settlement, queue/arbitration and drive reaction.
[AxisSession.cpp](../../examples/common/AxisSession.cpp), `requestStop`, preserves
these phases. Communication failure can prevent delivery entirely.

**Fix:** describe the additional application/CAN path without a fixed latency
claim. Define measurement boundaries and a response budget; preserve separate
command-delivery and observed-stop evidence.

**Acceptance:** cover debounce, occupied channels, delayed TX and failed
delivery. Report measured latency under its load conditions, not a universal
round-trip constant.

### A16 — P2: Current pasteable entry points contain obsolete execution instructions

**Where:** [README](README.md), bench supplement description;
[supplement README](bench_closure_18_33/README.md), lines 3–5;
[supplement COMMON](bench_closure_18_33/COMMON.md), line 15;
[START_HERE](START_HERE.md), “Execute the whole sequence or a range.”

The supplement is advertised as eight unexecuted packets on
`promptset/motorcontrol-can`. Its [execution index](../reports/bench_closure_18_33/README.md)
records all eight complete with commits and hardware results. Current branch
policy requires `main`. The main pasteable whole-sequence instruction also says
the sequence ends at 72 although the index and SEQUENCE now contain 76 prompts.

**Fix:** mark the supplement executed/historical and link its execution index;
point residual work to the live backlog/original Partial rows. Defer branch choice
to current root guidance. Make the pasteable range match the 76-entry sequence
or reference SEQUENCE without duplicating its maximum.

**Acceptance:** no current entry point asks a coder to repeat the completed
campaign, resume the obsolete branch or omit prompts 73–76. Preserve historical
reports and their original commit/branch facts.

### A17 — P2: Factory-restore backup policy is mandatory and optional in different prompts

**Where:** [57](57_regular_drive_operations.md), save/restore scope and item 6;
[61](61_drive_configuration.md), boundaries and Verify.

The November 17 policy makes a complete backup an optional factory-restore guard.
P61 still requires a complete backup first and a missing-backup refusal. P39 also
correctly records that the complete persistent set and factory values are
unknown. A dump of currently readable objects cannot establish a complete
restorable device image.

**Fix:** propagate the selected optional-guard policy into P61. Name the recorded
supported configuration subset and unknown fields rather than promising a
complete backup. Test guard-on refusal and explicit guard-off admission, while
retaining separate factory-restore intent, uncertain effects and no automatic
replay. Optionality is the user's policy choice, not a datasheet claim.

**Acceptance:** both workflows use one policy and one backup-scope description;
no successful read dump is labelled proof of complete rollback capability.

### A18 — P3: Lazy diagnostic allocation conflicts with the no-allocation test

**Where:** [76](76_psram_storage_placement.md), delivery item 2 versus Verify.

The prompt permits lazy console/diagnostic scratch but requires no PSRAM
allocation after startup. A permitted first-use allocation therefore fails its
own acceptance condition.

**Fix:** choose one rule. Preallocating all PSRAM blocks before control startup is
the simplest. If bounded one-time lazy diagnostics remain useful, explicitly
exempt them, account for their timing and retain the control-path prohibition.

**Acceptance:** allocator spies cover first diagnostic/console use during service
and enforce the chosen rule without hiding an exception.

### A19 — P3: CANopen's default identifier set is not fixed

**Where:** [63](63_mixed_protocols.md), facts, lines 11–15.

The prompt calls the per-node ranges fixed and `0x680–0x6DF` clean. The original
EDS has writable PDO COB-IDs, and the pinned
[CANopenNode PDO header](../reference/public_code/canopennode/301/CO_PDO.h)
distinguishes the predefined connection set from configurable identifiers.
P63's later requirement to inspect actual configured IDs is correct, but its
opening claim can encourage hardcoded allocation.

**Fix:** call these default/predefined ranges. The proposed free block is only a
candidate after checking every device's actual identifiers, broadcasts and frame
format behavior.

**Acceptance:** a synthetic configured PDO at `0x680` makes the proposed mixed
identifier plan fail validation. Default-range separation alone must not pass.

### A20 — P3: The moving-NMT-Stop premise is out of date

**Where:** [53](53_multi_axis.md), NMT Stop discussion, lines 35–37.

It says an enabled/moving drive has never received NMT Stop. The
[November 16 M14 trials](../reports/2026-11-16_hil_campaign.md) record four runs
on both specimens at 6/30 RPM: NMT Stop then Pre-operational left velocity running;
the run's SDO quick stop settled it.

**Fix:** append the dated, specimen-scoped result and evidence link. Keep NMT
Stop distinct from a motor stop. Repeating a trial may qualify a changed image;
do not describe it as first discovery or extrapolate to all firmware.

**Acceptance:** current instructions and backlog agree on what was observed and
what still needs qualification.

### A21 — P3: The SYNC fallback asks for the wrong period metric

**Where:** [54](54_hardware_sync_multiaxis.md), implementation item 2: “longest
period that passed.” [56](56_host_throughput.md) already uses the useful opposite.

The target is a fast usable update rate near 10 ms. An arbitrarily slow passing
period would satisfy the literal P54 wording without locating a useful limit.

**Fix:** report the **shortest tested passing period** at the same required
30-minute duration, with tested periods, load, failures and measurement limits.
The retained 20 ms/30-minute pass is a tested point, not proof of the exact
threshold. A shorter-duration pass is not interchangeable.

**Acceptance:** the qualification summary separates period, duration, node count,
load and pass/failure; it does not compare a 60-second trial with 30 minutes as
equivalent evidence.

## Facts checked that should not be “fixed”

- The former two-axis arming barrier is no longer a current missing feature.
  Paired arming was implemented in `b7033c12`, and later two-axis/group-stop
  evidence exists, including `b6d5ffa1`. Preserve that work; do not create a
  second paired-motion engine from the older failure report.
- EN86 PDF page 11 gives `607A` I32, ±1,000,000 pulses; `6081` U32, 0–3000 RPM;
  `6083/6084` U32, 0–2000 ms. The prompts preserve these and distinguish the
  narrower admitted ranges from the documented ranges.
- EN86 PDF page 13 and EDS disagree on `60FF` signedness; velocity start order
  also conflicts across manuals. These are real conflicts, not permission to
  merge procedures. Existing prompts correctly retain firmware-specific evidence.
- EN86 PDF pages 13–15 and the EDS support one-byte `6098`, four-byte `6099:01/:02`
  and `609A`, and signed `607C`. Homing bit 4 falling interrupts; method 29 is not
  merely method 24 with its direction reversed. Those warnings remain useful.
- `6061` is observation/readback. The English procedure's wording about setting
  it does not justify a write. Current prompts correctly reject that reading.
- EN86 PDF page 8 distinguishes DI stop function 5 from emergency-stop function
  6. It does not establish a certified stop, latency or electrical truth table.
  The DO conflicts remain open; do not choose terminal identity by convenience.
- `2300=2` save and `2300=1` factory restore are distinct from `2301` alarm clear
  and the `2302` zero-clear edge. Generic `1010/1011` signatures do not belong in
  this family's dictionary.
- P60's nearest rounding of 2.5 RPM to 2 is correct: the existing unit API uses
  ties to even. [Units.h](../../include/MotorControlCAN/Units.h) and
  [unit tests](../../test/units_test.cpp) explicitly cover it.
- Optional freshness/deadline/review guards, relaxed firmware matching and a
  reference recovery policy are deliberate user choices. The findings concern
  implementable contracts and truthful evidence, not rejection of those choices.
- P67 explicitly says its placement qualification consumes P76 despite numbering.
  That is a visible later evidence gate, not an undisclosed dependency cycle.

## Suggested correction order for the next coder

1. Resolve A01–A05 before implementing affected recovery, release, watchdog or
   memory paths. Correct each prompt and its authoritative contract together.
2. Settle P73's service-lifetime, observation-assembly and estimation contracts
   (A07–A10). Reuse the existing owner, qualified observations and operation
   contexts; do not add a parallel executor or general workflow framework.
3. Reconcile retry/output/policy ownership and startup (A06, A11–A15, A17), then
   the execution entry points and smaller wording/tests (A16, A18–A21).
4. Give each correction the focused regression named above. Keep hardware
   qualification separate; a corrected fake must not create a measured capability.
   Run the actual build/firmware/package/Doxygen gates when code changes.

## Review scope and verification

Three independent read-only reviewers covered disjoint primary areas, with the
lead checking the findings against actual declarations, implementations and
retained sources:

| Area | Numbered prompts reviewed in full |
| --- | --- |
| Protocol, transport and timing | 11–18, 23–28, 37–41, 51, 55–56, 61, 63, 73 |
| Device/motion and I/O | 19–22, 29–36, 52, 57–60, 62, 69, 72, 74 |
| Architecture, integration, qualification and execution | 1–10, 42–50, 53–54, 64–68, 70–71, 75–76 |

All eight active bench-continuation packets were also read. Archived v1/handoff
material was used for traceability, not treated as another current assignment.
This is not a claim that every implementation line or historical traffic frame
was independently re-audited.

Original Lichuan PDF/EDS facts were checked against the retained originals, with
rendered table/diagram checks where needed (including EN86 PDF pages 8, 11, 13,
15 and 18, and the ZH86 homing diagrams). PDF page numbers here are one-based
physical pages, not the printed manual footer. The SP706 manufacturer PDF was
checked separately online and rendered locally; it is a newly inspected source,
not an addition to the repository's immutable reference manifest.

Read-only sibling refresh: MotorControl-RS
`afe3588258228440125129988b877ebb453e7b72` and FieldCore-node
`c40ea8341edf8d665460b902cafefbe857a7f73f`, both clean. RS's application-owned
deadline/retry and retained uncertainty contracts were checked; no Modbus
settlement rule was adopted as CANopen proof. Neither sibling was modified.

Baseline `python scripts/check_promptset.py` passed: 76 stages, 202 ledger
entries, 53 manual obligations, 24 recorded conflicts and 140 hashed artifacts.
That checker validates links, inventories and provenance; these findings show
why a passing integrity check does not prove semantic consistency.

Final report checks passed: `python scripts/check_promptset.py` (76 stages,
438 Markdown files, 4,701 local links and 140 hashed artifacts),
`git diff --check` and `git diff --cached --check`.
This change adds only this audit report. No library code, original references,
numbered prompt instructions or hardware state were changed. Native/HIL tests
and manual CI dispatch are not needed to validate this documentation-only change;
none are claimed as performed by this audit.
