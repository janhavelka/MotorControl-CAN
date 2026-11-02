Execute this packet fully. Apply [COMMON.md](COMMON.md).

# Packet 17: Finish explicit alarm clearing and retained fault evidence

Prerequisites: packets 15 and 16's usable action/session contracts and packet 11's diagnostic path. Resume the alarm-clear portion of [prompt 32](../32_fault_reference_actions.md). Device counter clear is packet 18; persistence, factory restore and homing are outside this packet.

## Read the implementation and exact fault claims

Read [Actions.h](../../../include/MotorControlCAN/profiles/lichuan_c/Actions.h), [Actions.cpp](../../../src/profiles/lichuan_c/Actions.cpp), [fault/reference guidance](../../fault_reference_actions.md), [reference action tests](../../../test/reference_action_test.cpp), [prompt 32's report](../../reports/prompt_32.md) and [the independent audit](../../reports/prompts_31_32_audit.md). Inspect `prepareClearAlarm`, observation-plan construction, command history, terminal publication, the current application incident/session record and owner EMCY dispatch. Follow any earlier shared-helper/path changes.

Refresh RS `prepareClearAlarm` and actual caller/tests, INA228 retained dirty-cause/validity handling, PCA9555 explicit actions and FieldCore fault consumers read-only. Preserve common intent names without importing their native commands or fault-code interpretation.

Render EN86 PDF 15,16,18,20, ZH86 PDF 12,15,18 and OP86 PDF 10,13; inspect EDS `[2301]`, `[603F]`, `[6040]` and `[1003]`. Recheck Q13 and D-26/D-27 in [the repair audit](../PROMPTSET_AUDIT_34_50_2026-11-02.md). `2301` is U16; EN86 describes 1 clear and 0 ineffective. The current `1,0` sequence is explicit library policy, not proof of an edge or rearm requirement. The link between FF05 position deviation and the separately named resettable LED fault is an inference; the LED table has no numeric mapping or route assignment.

## Complete the action and investigation

1. Write the supported transition table before changing behavior: exact fault provenance, mode/state, native route, command values, rearm policy, ACK, required new alarm observation, raw error history and final uncertainty. Preserve the separate controlword-bit7/full-word 128 question. No enabling preamble or subsequent enable/resume is permitted.
2. Audit the existing FF05 admission. A retained `603F=FF05` plus a later alarm does not establish that the later cause is the same resettable event. Correct overclaiming text and represent unresolved fault identity explicitly. Do not invent FF02–FF04 from LED counts. Keep source inference and physically established behavior separately scoped.
3. Integrate fresh alarm observations through the ordinary action/owner path. Honor the earlier accepted-read policy: do not repeatedly poll potentially consuming error/history objects as an ordinary completion loop. Preserve original raw status, error/history, EMCY, failed-operation evidence and the new observations. Acknowledged clear is neither proof that the cause disappeared nor permission to clear the incident record.
4. When a naturally present, identified fault and applicable fixture provide a valid experiment, execute one exact reviewed clear attempt through the public action path. Record the visible fault indication, original raw fields, exact bytes, TX/ACK, new monitor state and independent observation. If the condition persists, retain that result. Do not electrically fault the board, obstruct the shaft, disconnect live motor/encoder wiring or induce overcurrent to obtain a test case.
5. Determine whether the final ineffective zero was definitely transmitted and acknowledged. If it failed or remained uncertain, retain the possible level/rearm consequence; never automatically resend 1 or claim that a new operation ID creates a new edge. Further trials follow COMMON's finite new-intent and reconciliation rules, with the cause established first.

## Test and close this bounded scope

Add direct public API and real-fake-owner tests for each supported fault/state; stale FF05 associated with a later incident; persistent alarm after ACK; error value remaining latched; controlword route rejection; prefix failure at every command; cancellation/deadline races; delayed ACK; reboot; stale reservation/configuration; and retained historical evidence after success or failure. Assert exact writes and absence of enable, motion, save, restore or automatic retry. Exercise two-node fault isolation and asynchronous EMCY during clearing.

Run `reference_action`, `drive_state`, `stop_action`, relevant read/evidence suites and owner/service regressions. Reuse packet-11 diagnostics to distinguish command rejection, transport uncertainty, stale data and an uncleared physical cause. A later successful trial alone is not a repair or causal explanation. If no suitable actual fault exists, finish the software scope and document the exact physical case still required; do not manufacture a qualification result.

One implementer owns alarm state and incident publication. An independent reviewer challenges stale-code admission, inferred resettable scope and rearm uncertainty; another may add disjoint tests. Only the lead operates hardware. Update Doxygen, Q13/coverage, guides, original prompt-32 report and `docs/reports/handoff_19_33/packet_17.md`, with source/host/physical evidence separated. Run required gates, commit and sync this branch.
