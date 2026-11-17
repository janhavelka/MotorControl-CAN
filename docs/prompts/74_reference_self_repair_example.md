# Prompt 74 Reference self-repair example: presence, retries, reconciliation and recovery

Apply [COMMON.md](COMMON.md). Prerequisites: [55](55_self_repair.md), [57](57_regular_drive_operations.md), [68](68_application_task_structure.md), [73](73_timing_freshness_guards.md).

## Objective and boundaries

The user's instruction, verbatim (2026-11-17), answering the listed restriction "No retry, no automatic fault clear,
and uncertainty lasts for the whole session": "an example can implement all of these as a reference solution - take
rs485 implementation from fieldcore-node library for how rs485 is handled and take the relevant parts and implement
them in here (audit the fieldcore code thoroughly and write new prompts)"; and generally: "In general we are after good
engineering practises and more responsibility on the end user of this firmware with possible optional settable
guards".

The FieldCore-node audit is done ([report](../reports/2026-11-17_fieldcore_rs485_audit.md), HEAD
`c40ea8341edf8d665460b902cafefbe857a7f73f`). Its "Recommended adoption" table is this prompt's design input. Prompt 55
now delivers only the library primitives; this prompt builds the reference application policy on them, in the examples,
as one simple, documented, settable module that any upper firmware can copy or replace. It is the only policy
implementation: it plugs into prompt 68's per-pass call site and extends prompt 68's motion inhibit, and adds no second
task, queue or inhibit.

Deliver, per node, with every policy settable and reported:

1. **Presence:** Unknown → Online → Degraded → Offline, plus Disabled (visible, bus-silent). A miss is one operation
   ending without any response, counted once after its retries; an SDO abort, a wrong reply or an EMCY is "present but
   erroring". Thresholds default to 2 and 5 (FieldCore's), validated degraded > 0 and offline > degraded. One response,
   heartbeat or boot-up sets Online and emits a returned event. Offline nodes fail fast (new non-stop work is refused
   as unavailable) and get one probe per settable interval (one cheap reviewed read or a heartbeat; on a quarantined route that read is prompt 55's marker). Never an NMT reset
   on loss; a boot-up still invalidates that node's bindings and motion evidence.
2. **Timeouts and retries:** the per-node response timeout of prompt 73; a per-operation deadline that no retry
   extends. Retry classes, from prompt 55's class on each operation: R (reads whose `readEffect` is reviewed
   non-changing or accepted under DEC-1, 0..3, default 0; a nonzero count is the application's decision to repeat them
   and leaves `readEffect` and its uncertainty unchanged; settable backoff, only on no response or a malformed reply,
   with an "exhausted" result), W (idempotent absolute settings with readback, off by default), M (motion start,
   enable, homing, save and NMT: never retried; policy validation refuses a nonzero count, as FieldCore's
   `Rs485MotorModule::applyRuntimeConfig` does) and N (the default for everything else, including `UNQUALIFIED` fault
   and history reads: never retried, validated as M). A repeat after a timeout waits until prompt 55's resync has
   reopened the route under the selected reuse policy and is a new admission inside the original deadline. A quick
   stop may be repeated, each repeat a new, separately identified attempt; the HALT word is never repeated.
3. **Uncertainty reconciliation:** an unconfirmed write or a stop inhibits motion under prompt 68's per-axis inhibit
   and generation counter, not a second one. The inhibit lifts on a matching generation, a fresh standstill report
   acquired after the inhibit, a settable age bound and nothing in flight, through the session's existing paths: a
   checked stop clears `AxisSession::moveFenced()`, and `AxisSession::acknowledgeStandstill` takes the application's
   own fresh standstill read (both added on 2026-11-17).
   An opt-in automatic stop → read → reconcile sequence, as FieldCore's `RodCycleCoordinator` runs. Retained results
   keep their history; readiness is never silently requalified.
4. **Controller recovery:** bus-off, RX overflow, a frame without TX-done within its lifetime or a failed install
   (prompt 68) restarts the controller through prompt 55's entry point, at a due time that backs off after each failed
   restart, with a rate limit, no catch-up and no replay; this is the only restart schedule. An explicit operator recovery is single-flight
   with its own result. Recovery invalidates bindings and motion evidence and inhibits motion.
5. **Optional fault clear:** off by default; only through `prepareClearAlarm` while released or idle, limited per window
   and per boot; it never enables, never retries motion and never escalates to reset or save.
6. **Health and diagnostics:** a per-node status (presence, counts of success, error, timeout, miss and stale, last
   attempt/success/error times, last error); bus metrics with depths, high-water marks, overflow and drop counts; a
   bounded trace ring with sequence numbers; rate-limited state-change events; application age thresholds with boot
   grace and Required/Optional roles.
7. **Configuration:** caller-owned node slots and a `NodePolicy` (response timeout, prompt 55's SDO reuse policy, read
   retries, retry backoff, degraded and offline thresholds, probe interval, quick-stop repeat, automatic reconcile,
   automatic fault clear, fault-clear limit), `validateNodePolicy()`, staging applied at an idle boundary, and console
   commands that call the API only. The reference uses prompt 55's default reuse policy, accepted risk with the marker,
   named in health and carried by the accepted-correlation mark on results. A change that affects interpretation invalidates the node's
   evidence.

Boundaries: half-duplex RS485 mechanics (DE/RE, guards, gaps, drain settlement, echo stripping, baud switching, one
global transaction) are not transferred. The portable core keeps no hidden retry or clock; the policy lives in the
example module and the application owns it. Stops keep priority over every repair step.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its retry classification and recovery seams; RS
is authoritative for common names. Inspect FieldCore-node read-only at its RS485 owner, sensor and motor modules,
`HealthCoordinator` and `RodCycleCoordinator`, starting from the audit's key paths. If HEAD, dirty state and seam match
the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or
rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read
the prompt 55, 57, 68 and 73 reports, `AxisSession`, `ProbeMotion`, `GroupStop`, `CanBusOwner` recovery and the
[self-repair report](../reports/2026-11-12_self_repair_and_mixed_protocols.md). Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. The example module (`examples/common/NodeSupervisor.h/.cpp` or a reviewed name) with `NodePolicy`, presence, retry
   classes and their validation; a decision table in the report mapping each FieldCore mechanism to adopted, adapted or
   rejected.
2. Reconciliation on the session's checked stop and `acknowledgeStandstill`, the generation counter and the opt-in
   automatic sequence.
3. Controller recovery wiring to prompt 55's restart entry point, with its schedule, rate limit and result.
4. The optional fault clear with its limits.
5. Health, metrics, trace and events; console commands; the module plugged into prompt 68's CAN task call site and
   used by the S2 bench image.

## Verify and exit

Host tests: presence transitions with thresholds 2 and 5 and custom values; one success restores; abort and EMCY do not
count as misses; Offline fail-fast and the probe cadence; read retry with backoff inside the deadline and the exhausted
result; an accepted-unqualified state read repeated only under a nonzero count and still `ACCEPTED_UNQUALIFIED`, a fault
or history read (N) never repeated, and a fake reviewed non-changing read repeated under the same count, each inside
the original deadline; no repeat before the route reopens; a nonzero M or N retry refused at validation unless prompt 55's recorded per-object permission makes that object R; a quick-stop
repeat as a new operation and never a HALT repeat; reconciliation refused on an old, other-generation, moving or
in-flight case and accepted on a fresh standstill; recovery rate limit and no replay; fault clear off by default and
limited when on; policy staging at idle only. Run the native, firmware, stack and documentation gates and CI on the
actual final commit. Bench: one node unplugged by the user and returned (Degraded, Offline, probe, Online, the other
node unaffected), a forced read timeout with a read retry, and a stop then reconcile then move, then a short
regression. This bench is focused; the sustained qualification of these policies is the resumed policy part of
prompts 66 and 71.

## Documentation and delegation

Update the owner and multi-axis guides, the firmware examples guide, the CLI contract, coverage, backlog (D14, H04,
Q18 where they change), CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the retry classes and
the reconciliation rule; normal coding agents own disjoint tests. The lead integrates and runs combined checks. Record
`docs/reports/prompt_74.md`. Commit and sync on the current branch before handoff.
