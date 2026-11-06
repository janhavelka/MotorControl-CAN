# Packet 8 Close remaining finite position and stop cases

Apply [COMMON](COMMON.md). Prerequisites [1](1_evidence_contracts.md) and
[2](2_position_regressions.md). Originals29/31-33. Current bench tooling can run
this independently of packets3-7. No velocity/homing implementation or general
re-enable/release command is introduced.

## Inspect

Read originals29/31/32/33, `docs/position_parameters.md`, `docs/stop_actions.md`,
`docs/fault_reference_actions.md`, latest Nov4/Nov5 reports and raw trial records,
`examples/s2_bench/BenchRoute.*`, `examples/common/AxisSession.*`,
`scripts/s2_bench.py` and its tests. Refresh RS move/stop/reference terminal
contexts and FieldCore separate histories; INA228 validity only if changed.
Check newer commits first: some planned cases may already have evidence.

Current evidence includes node1 ACTUAL relative base (trial409), a production
relative move (411 and later708), and moving stops from absolute001F. Do not
repeat those simply to claim implementation. Node2's hand turn after release
does not by itself calibrate command-pulses-per-turn or a relative wire base.

## Implement and execute

1. Add only missing bounded route support through the ordinary public operations
   and AxisSession. Plan/dry-run exact values, required baseline, duration and
   budgets using the existing CRC-protected command protocol/campaign ledger. Reuse
   current 2/6 rpm low-speed cases and nonzero documented ramp values after
   confirming their current source/fixture applicability; never inherit a stale
   position or assume the drive was reset when the MCU restarted.
2. Run one negative relative displacement on node1 from a fresh stationary
   ACTUAL witness. Initial bounded proposal: -100 native command pulses,
   6 rpm, ramps200/200 ms and 30-second operation deadline, only after actual
   axis limits/scale and the current reviewed parameter checks admit it. These
   are test-policy values, not device defaults or response-time guarantees.
   Verify acceptance, completion, retained effective target and
   raw endpoint separately. Do not make success require exact endpoint equality
   when the measured in-place indication can precede final settling by one pulse.
3. On node2, establish ACTUAL-versus-COMMANDED discrimination with a finite halted
   move whose retained commanded endpoint differs from its observed current
   position. Use the existing experiment route, not a production request falsely
   prequalified with node1's result. After evidenced halt rearm and a stationary
   witness, one finite relative trigger distinguishes the candidate bases.
   If usable evidence results, run one production relative request using that
   specimen's explicit binding; otherwise keep its default UNMEASURED.
4. Exercise HALT and quick stop from a real acknowledged005F predecessor, each as
   its own finite request. No000F enabling/rearm preamble merely to permit the
   stop. Retain the interrupted relative move and stop independently. Include
   a case with the other node receiving passive observations. If a route/state
   needed for a case is unavailable, implement the checked route or name the
   exact evidence gate; do not reset/enable implicitly to force the test.
5. Validate unknown-trigger/stop/counter-reference consequences with packet2's
   deterministic fake timelines, not by deliberately losing live motion replies.
   For counter zero, reuse the already observed public path; a new physical case
   is useful only with an explicitly justified nonzero baseline/state/specimen.
   Preserve 2302 edge history and axis/reference invalidation, with no fabricated
   home or automatic rebase. Do not clear a healthy drive's nonexistent alarm.
6. For any confirmed defect, retain the failing capture, fix the smallest code
   path, add a meaningful native regression, and repeat the exact finite case
   after a sound session/state prerequisite is restored. Respect total admission
   caps across reboots. Repetition is deliberate new intent, never an automatic
   replay of an uncertain write. Continue unrelated software if a physical power
   cycle or shaft observation must wait for the operator.

## Verify and finish

Run move/action/axis-session/owner tests, runner/firmware transcript tests,
affected real S2 build, Doxygen and required CI for behavior changes. Run a short
read baseline before/after each changed image and retain exact build/ELF hashes,
TEC/REC, service gaps, frames and terminal records. Software-reported standstill
does not establish physical torque/scale. Ask for a shaft mark observation only
where needed; never set an observed flag from a readback.

One lead controls the bench. Delegate offline parser/regression work and a strong
review of unknown trigger through later admission. Report packet_8 and update
the continuation execution index, original29/31-33 reports and current gates.
Keep release, naturally occurring alarm, instrument-dependent settings, physical
scale and untested firmware explicitly separate. Provide the next smallest
measurement for each remaining gap; commit and sync. Do not mark the entire
original prompt range complete from this finite campaign.
