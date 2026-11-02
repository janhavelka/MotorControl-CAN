# Lichuan stop commands and reported stopping

The finite stop path uses `prepareStop`, `nextAction` and `advanceAction` in
`MotorControlCAN/profiles/lichuan_c/Actions.h`. The caller owns the context,
continuous sequence reservation, time and ordinary CAN owner. This is a narrow
CL86-C/OL86-C position-mode contract. It does not implement a safety stop,
generic CiA402 stopping, velocity-mode stopping or a torque/brake policy.

## Reviewed transitions

This table was specified before the lifecycle implementation. The original
ZH86 PDF p12 places both arrows in the position-mode procedure. EN86 p12
contains the quick-stop arrow but omits halt. OP86 p10 agrees, with the retained
57C/86C internal-heading applicability conflict. EDS `[6040]` establishes a
U16 write-only object; a controlword read-modify-write is not required.

| Kind / starting evidence | Exact command | New evidence | Remaining consequences |
| --- | --- | --- | --- |
| `HALT`: last acknowledged command `6040:00=000F`, continuous exclusion, fresh mode 1 / low status bits 7 / monitor alarm and released clear | One `6040:00=010F` U16 download | Matching SDO acknowledgement, then new STATUS/MONITORS reads | Normal-stop label only; deceleration, queue disposition, torque and reference effects unknown |
| `QUICK_STOP`: same prerequisites | One `6040:00=000B` U16 download | Same acquisition contract | Vendor's emergency-labelled command; no certified safety function or latency claim |
| Prior `010F`/`000B`, unknown predecessor, released, other mode or revoked history | Reject preparation | None; output unchanged | No automatic rearm, enabling preamble or inferred idempotence |
| Prior target trigger such as `001F`/`005F` | Outside this implemented predecessor scope | Requires applicable transition evidence | This remains an owned stop capability gate, not a completed motion-interruption implementation |

The preceding `000F` must be the application's actual latest acknowledged
controlword under a reservation acquired before that command. Fresh low bits 7
cannot reconstruct it. Any intervening target, controlword, mode, RPDO,
configuration change, reset/rebind/recovery or uncertain submission withdraws
that history. Reacquiring a reservation does not restore it. These facts must
come from the sole application owner; copying an old evidence struct is not a
new observation. This API never writes `000F` or the `6,7,15` enable sequence.

Initial running may be either value. A stop does not require an already stopped
drive. Native kind names deliberately avoid promising RS configured deceleration
or direct torque removal. No queue-preserve/discard, custom ramp or option-code
setting is offered. `605D` is I16 RW/default 1 without usable option semantics;
`605A` and `6085` are absent. None supplies a braking guarantee or a setup write.

## Evidence and finite progression

Preparation copies validated inputs atomically and produces no traffic. Yielded
work owns its bytes. The application admits each step through `admitUrgent` on
its existing reservation and reports the possible-transmission boundary through
`noteActionTransmission` before external handoff. A rejected admission is not
transmission. The owner must retire outstanding work before a local action
withdrawal can conclude that it will never transmit.

The context retains command evidence separately from subsequent observations.
`ActionExecution::ACKNOWLEDGED` means a matching SDO download completed.
`ActionCompletion::OBSERVED` means a newly acquired monitor reported not running
during its observation window. EN86 p17 and ZH86 p18 agree on `2100` bit 2
(running), bit 3 (alarm) and bit 4 (released). No disputed `6041` completion or
alarm layout is selected for this decision. Raw status, mode and per-field
provenance remain available.

Every observation cycle begins after the command acknowledgement. A cached
stopped/completion bit cannot satisfy it. The existing typed reader acquires
MONITORS before STATUS; the five fields are not an atomic snapshot. New mode 1,
alarm clear and released clear are the supported observation policy. A released
report fails that policy; it does not prove that stopping physically failed or
that the command should have retained torque. An alarm/fault terminates the
operation with its command evidence preserved. The application delivers routed
fault/lifecycle invalidation before advancing a later successful read result.

If running remains set, the caller-specified observation interval schedules
another finite acquisition. The interval, service budgets and overall deadline
are host policies, not Lichuan timing guarantees. All uploads retain their
reviewed `UNQUALIFIED` read-effect classification. This path neither proves
non-consuming reads nor reopens an unresolved SDO session.

Time is nonwrapping monotonic microseconds. Inclusive processing-time expiry
wins even if an earlier captured reply is processed after the deadline. Later
observations cannot rewrite a terminal. Acknowledged command evidence survives
observation failure, fault, cancellation and timeout. Unknown execution remains
unknown after a missing reply; no command is retried. Poll-step and timestamp
arithmetic must not wrap.

## Owner handoff and retained interruption

The [owner contract](can_owner.md) owns capacity, transmission retirement,
receive dispatch and channel quarantine. Stop is an explicitly admitted new
operation. Urgent priority alone does not cancel old work.

1. Validate the proposed stop and retain the original operation/context. A failed
   preparation leaves earlier work unchanged.
2. Explicitly withdraw the old producer's continuation and cancel every pending
   request in its sequence. Copy or retain each terminal by its original ID.
3. With no pending old request, use `beginOperation` on the same reservation and
   a strictly larger operation ID. Retained old results do not block it.
4. Admit the stop's yielded work through reserved urgent capacity, service the
   same owner, consume its matching completion, then explicitly release that
   request slot. Repeat only the observation steps the context yields.
5. Keep the stop and interrupted operation results independent. Reject any later
   admission of old-operation work. A later new operation requires fresh,
   applicable prerequisites; stop does not restore the old controlword history.

Cancellation and stop admission are distinct outcomes. Capacity refusal after
explicit interruption does not undo cancellation or claim a stop was sent.
Do not invalidate/reacquire the sequence merely to fit the stop: that loses
continuous exclusion and cannot bypass retained results or channel quarantine.
Do not service another turn between deciding to retire old queued work and
performing that retirement.

Local queued work can be definitely retired. Once handed off, exact controller
retirement/final-report evidence and remote SDO ambiguity remain separate. If
the route becomes unresolved, this stop cannot use it. Other nodes and passive
events remain serviceable; no NMT, guessed PDO or second receiver substitutes
for the unavailable stop. Controller recovery cannot make the earlier write
not have happened, retire a remote producer or replay the stop.

## Support and qualification

Host tests cover the implemented conditional path through the real owner and
selected SDO engine. They do not establish firmware applicability, motor
standstill, stop latency, braking curve, torque, device-target retirement or
reference validity. Broader predecessor states, repeated stop from halted or
released state, velocity/homing modes and a quarantined-channel alternative need
their own reviewed transitions and fixture evidence. See the
[prompt 31 report](reports/prompt_31.md) for remaining obligations and checks,
and the [evidence harness](device_conflicts.md) for acquisition prerequisites.
