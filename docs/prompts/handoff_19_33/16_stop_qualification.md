Execute this packet fully. Apply [COMMON.md](COMMON.md).

# Packet 16: Qualify and finish the stop path on the actual fixture

Prerequisites: packet 7's repaired stop/urgent mechanism, packet 11's HIL diagnostics and packet 15's usable enable/state evidence. This resumes [prompt 31](../31_stop_actions.md). A completed prompt-33 motion engine is not required: an isolated packet-29 experiment may supply one bounded trigger to investigate stopping.

## Read and challenge the current implementation

Read [Actions.h](../../../include/MotorControlCAN/profiles/lichuan_c/Actions.h), [Actions.cpp](../../../src/profiles/lichuan_c/Actions.cpp), [stop guidance](../../stop_actions.md), [the prompt report](../../reports/prompt_31.md), [stop tests](../../../test/stop_action_test.cpp) and the actual owner/adapter queue and retirement code after earlier packets. Read the active audit's DEC-2, DEC-8 and W6 dispositions in [the repair audit](../PROMPTSET_AUDIT_34_50_2026-11-02.md). Verify that packet 7 implemented its contract rather than assuming a report proves it.

Refresh RS action interruption, urgent admission and retained-result callers/tests, PCA9555 cancellation and INA228 validity; inspect FieldCore stop admission read-only. In CAN, local cancellation, release, NMT stop and a drive stop remain distinct.

Render ZH86 PDF 12, EN86 PDF 12 and OP86 PDF 10, and inspect EDS `[6040]`, `[605D]`. Documentary arrows are `000F→010F` for normal stop in ZH86/OP86 and `000F→000B` for emergency stop. They do not prove latency, braking, retained torque, queue disposition or a certified emergency-stop function. `605A/6085` are absent; `605D=1` supplies no usable option semantics. Broader predecessors are explicit project decisions requiring specimen evidence, not additional vendor arrows.

## Implement and physically exercise the supported contract

1. Complete a stop-transition/evidence table for each actual starting state. Confirm the repaired session holds latest controlword history and withdraws it after conflicting commands, target/mode/configuration writes, reset, rebind or uncertainty. Never send `000F` or `6,7,15` merely to prepare a stop.
2. Exercise an already independently established enabled/stationary `000F` state before a moving case. Submit each exact stop as new urgent intent with its own immutable deadline; retain command ACK, controller-reported not-running and physical observation separately. Repeated stops, halted states and released states need their own supported disposition.
3. Before relying on the best-effort path, establish `000B` behavior on an independently evidenced released state when that state is available. Do not invent a release command to create it. If this prerequisite is unavailable, retain that narrow physical gate; continue confirmed-route stop work. A best-effort result never becomes qualified merely because a matching-looking SDO reply arrives.
4. For a moving test, jointly execute packet 19's isolated packet-29 experiment after stationary/software artifacts are ready. Review known mode, positive speed/ramps, coordinate/scale facts, maximum travel/time, visible shaft observation and independent cutoff. If scale is the measured unknown, use packet 19's independently bounded exposure/containment rather than pretending the unknown scale gives a software angular bound; retain the gate if that envelope cannot be established. The offline preview is review input, not execution permission. Use the same public codec/service/owner and one target node; keep the other observable. Test the selected post-trigger `000B` or `010F` explicitly. Do not first write `000F`, assume it stops, or require completed33 or completed moving-stop qualification before this isolated experiment. Packet 19 records the shared evidence back into this packet's report.
5. Fix every confirmed software gap exposed by the trace. Preserve interrupted operation identity and uncertainty. Quarantined-channel stop transmission must follow packet 7's separate bounded policy, without reopening the channel or promoting a delayed reply. No automatic repeat, enable, fault clear or restoration follows failure.

## Adversarial verification and handoff

Through the real fake owner, cover: in-flight poll followed by stop; lost trigger reply and indistinguishable delayed ACK; lost poll while stop waits; two nodes with one unread stop result; full ordinary/result capacity; stale queued targets before/after stop; uncertain controller retirement; fast escalation over normal stop; cancel/fault/reboot; exact deadline; fresh versus expired observations; and a copied old controlword after contradictory work. A never-transmitted stop held across bounded recovery must remain distinguishable from possible transmission. Retain existing rejection tests where the revised contract still requires refusal.

Use `stop_action`, `can_bus_owner`, `can_bus_recovery`, `sdo_transfer`, `sdo_reconciliation`, `twai_adapter` and action regressions. Measure worst observed service gaps and target task stack headroom during stop, not only during reads. Apply COMMON's finite reproduction and causal-diagnosis procedure; do not inject electrical faults or disconnect a moving motor to generate a failure.

One writer owns stop/session mutations; independent reviewers trace occupied-channel ambiguity and test pressure/timelines. Only the lead controls the bench. Update Doxygen, stop/owner guidance, evidence gates, coverage, original prompt-31 report and `docs/reports/handoff_19_33/packet_16.md`. Separate software conformance, the observed specimen scope and unsupported physical guarantees. Run required gates, commit and sync this branch.
