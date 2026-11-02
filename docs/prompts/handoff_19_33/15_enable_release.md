Execute this packet fully. Apply [COMMON.md](COMMON.md).

# Packet 15: Establish mode and enable behavior; implement the supported actions

Prerequisites: packets 10 (shared API repairs), 11 (HIL diagnostics) and 12 (driver settings). Consume their actual checked artifacts. This resumes [prompt 30](../30_drive_actions.md) through [prompt 29's experimental route](../29_device_evidence.md); it does not require a completed position lifecycle.

## Read the actual starting point

Read [State.h](../../../include/MotorControlCAN/profiles/lichuan_c/State.h), [State.cpp](../../../src/profiles/lichuan_c/State.cpp), [Actions.h](../../../include/MotorControlCAN/profiles/lichuan_c/Actions.h), [Actions.cpp](../../../src/profiles/lichuan_c/Actions.cpp), [ProbeApp.h](../../../examples/common/ProbeApp.h), [drive action guidance](../../drive_actions.md) and [prompt 30's report](../../reports/prompt_30.md). Discover the owner path after the earlier relocation; do not recreate its old location. At handoff baseline, the state getter is passive and `prepareEnable`/`prepareRelease` do not exist. Check what preceding packets changed.

Refresh MotorControl-RS `profiles/ess_rs/Actions.h/.cpp`, its action tests and actual ProbeApp callers. Compare ADS1115 owner-loop completion and INA228 hardware-validity withdrawal. Inspect FieldCore action admission and retained outcomes read-only. Record current revisions, dirty paths and concrete differences; do not copy ESS command values.

Render the original EN86/ZH86 PDF pages 11–13, 17–18 and OP86 pages 9–10; inspect EDS `[6040]`, `[6041]`, `[6060]`, `[6061]`, `[2100]`. Use [the conflict table](../../device_conflicts.md) and [the current motion plan](../../reports/2026-11-02_pdo_motion_plan.md) as navigation. The position enable sequence `6,7,15`, approximately 10 ms apart, is documented. `6061` is observation, not a write target. No reviewed source establishes a motor-release command/effect pair.

## Produce and execute the bounded evidence plan

1. Refresh the packet-11 fixture and image evidence. Historical measurements on both specimens were `607A=5000`, `60FF=120`, `6041=0x8037`, `6061=0`, `2100=1`. They are stale readback, not current intent or an enable prerequisite. Read the complete current readiness record, preserve the original values, and observe the shaft independently.
2. Write the transition table before implementing writes: starting state and provenance, mode/target history, exact command, service result, required fresh observation, deadline, failure handling and torque/reference consequences. Keep documented claims, project policy and measured firmware behavior separate. Shared status bits cannot resolve disputed alarm/running bits by union.
3. Use the existing checked experiment/owner path for one isolated, reviewed `6060:00` U8=1 transition if the fixture and independent cutoff bound its possible retained-target effects. Acquire fresh mode/status/monitor/error/feedback evidence and an independent shaft observation. Do not append enable or trigger as automatic cleanup. If a required bound cannot be established, identify that exact missing fact and finish independent host work.
4. After establishing a usable initial-state/retained-motion contract, implement `prepareEnable`, `nextAction` and `advanceAction` through the existing concrete lifecycle. Use scheduled, nonblocking intervals and finite deadlines; an ACK or nominal 10 ms interval does not establish readiness. Retain every prefix command and uncertain effect. No implicit target zeroing, replay, fault clear or mode change on selection/recovery.
5. Implement `prepareRelease` only when a specific applicable command, admissible states and new observations establish its effect. `6040=0`, status low bits zero and DI function 4 are not established release semantics. Keep this exact capability pending if unresolved; release is not a circular prerequisite to enable/stop qualification.

## Verify and close

Use `drive_state`, `stop_action`, `reference_action`, owner/service and packet-11 harness tests. Cover every supported transition, unknown bits, sequential-sample disagreement, wrong mode, retained nonzero velocity, fresh versus copied control history, fault at each step, delayed response, cancellation/deadline precedence, reboot, stale generation and atomic rejection. Add direct public-API/real-fake-owner examples independent of a console. Measure action stack usage on the actual target after the prior stack repairs.

One implementer owns action/session mutations. A separate strong reviewer traces retained-target effects and uncertainty; another agent checks original diagrams and exact test bytes. Only the lead operates hardware. Apply COMMON's diagnose–fix–retest procedure to failures; a passing repeat alone does not establish a cause.

Update Doxygen, action/bench/conflict guides, ledger/coverage and `docs/reports/handoff_19_33/packet_15.md`; update prompt 30's report with implemented and remaining scope separately. Run the focused and required integrated gates, then commit and sync the same branch. Do not mark all of prompt 30 complete while release or another owned behavior remains unsupported.
