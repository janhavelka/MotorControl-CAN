# Lichuan stop commands and reported stopping

The finite stop path uses `prepareStop`, `nextAction` and `advanceAction` in
`MotorControlCAN/profiles/lichuan_c/Actions.h`. The caller owns the context,
continuous sequence reservation, time and ordinary CAN owner. This is a narrow
CL86-C/OL86-C position-mode contract. It does not implement a safety stop,
generic CiA402 stopping, velocity-mode stopping or a torque/brake policy.

## Reviewed transitions

The manuals give whole-word stop arrows only from word 15. ZH86 PDF p12 places both
arrows in the position-mode procedure. EN86 p12 has the quick-stop arrow and omits
halt. OP86 p10 agrees with ZH86, with the retained 57C/86C internal-heading
applicability conflict. No manual defines bit 2 or bit 8, or says what a falling bit
4 does in mode 1. EDS `[6040]` establishes a U16 write-only object, so a controlword
read-modify-write is not required.

So every row below the first is a **project decision, not vendor evidence** (DEC-8):
a running move with no stop is worse, and clearing bit 4 while stopping cannot be the
wrong direction. `ActionContext::predecessorDocumented` is false for those rows, and
`predecessorWord` records the word. They stay unmeasured until packet 16.

| Latest acknowledged `6040`, mode 1 | Stop | `predecessorDocumented` | Basis |
| --- | --- | --- | --- |
| `000F` | `000B` (`QUICK_STOP`) | True | EN86 and ZH86 p12, OP86 p10 |
| `000F` | `010F` (`HALT`) | True | ZH86 p12 and OP86 p10 only |
| `001F`, `005F`, `003F`, `007F` | `010F` or `000B` | False | DEC-8, no documented arrow |
| `010F` | `000B` only | False | DEC-8, escalation of a running normal stop |
| `000B`, `010F` for `HALT`, any other word | Rejected, output unchanged | - | No rearm, enabling preamble or inferred idempotence |
| Unknown or withdrawn word, or stale state | `000B` on the best-effort path below only | - | DEC-2 |

The sequenced path needs the frozen CL86-C/OL86-C model, and fresh STATUS reporting mode
1 and low bits 7, and fresh MONITORS reporting alarm and released clear. Fresh reports of an
**idle** drive that is alarmed, released, in another mode or not enabled refuse the request
on both paths, with no traffic, because nothing establishes what `000B` does in those
states. A running report keeps the stop available on the best-effort path, as this project's
decision that a running motor should get the attempt. Modes 3 and 6 get their own
sequenced stops in prompts 35 and 36.

The word must be the application's actual latest acknowledged controlword, under a
reservation acquired before that command. Fresh low bits 7 cannot reconstruct it. An
acknowledgement does not expire by age, because a drive idle at `000F` must not lose
its stop. Read observations still expire.

`AxisSession` in `examples/common/` keeps this history per axis, and every command the
axis sends passes through its `admitPrepared`. It withdraws the word at once on any new
download through that path, because that write may reach the drive before any report
says so. That covers `6040`, target, mode, RPDO and configuration writes alike. It also
withdraws on a reset, rebind, recovery, bus-off, boot, route quarantine or a stop's own
admission. Reopening the session does not restore it, and a write settling after a stop
was admitted is older intent and does not restore it. A copied evidence struct is not a
new observation. A stop never writes `000F` or the `6,7,15` enable sequence. The separately gated
[enable](drive_actions.md) does, through this same path, so its acknowledged `000F` is such a word. A
stop request ends an active enable first, even when the stop itself is refused, and even with a
time that moved backward. `AxisSession` refuses after a known word with no arrow, such as enable
word 6 or 7, only on a fresh report that the drive is not running. A fresh running report, or no
fresh report, takes the best-effort `000B`, exactly as when that word is still in flight. This
changes the earlier rule, under which any such word refused the request.

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

Time is nonwrapping monotonic microseconds. Processing-time expiry at
`now >= deadlineUs` wins even if an earlier captured reply is processed after the deadline. Later
observations cannot rewrite a terminal. Acknowledged command evidence survives
observation failure, fault, cancellation and timeout. Unknown execution remains
unknown after a missing reply; no command is retried. Poll-step and timestamp
arithmetic must not wrap.

## Owner handoff and retained interruption

The [owner contract](can_owner.md) owns capacity, transmission retirement, receive
dispatch and channel quarantine. A stop is an explicitly admitted new operation, and
`AxisSession::requestStop` runs it:

1. Decide first. Invalid budgets, a known unsupported predecessor, or the fresh idle
   reports above refuse the request with nothing changed but a stale word withdrawn. Refusal
   does not stop the drive.
2. `beginUrgentOperation` retires this reservation's queued work with explicit results.
   In-flight work settles, because cancelling it could quarantine the route: a stop never
   cancels a poll. The stop gets its own operation and an immutable deadline.
3. The sequenced stop goes through the channel's urgent slot behind that work, and
   its observation cycles use the same slot. The session consumes each terminal.
4. Old results stay with their own request identities, and later admission of
   old-operation work is refused. A later operation needs fresh prerequisites, and a stop
   does not restore the old controlword history.

If the settling work leaves the route unresolved before any stop frame could leave, the
stop ends `NOT_TRANSMITTED` and the request takes the best-effort branch.

**Escalation is upward only** (R-14). `QUICK_STOP` may replace a running `HALT`. A
`010F` in flight settles first and its terminal stays in its own context, then `000B`
follows. No separate read can run during a stop, so `prepareEscalation` takes the running
`HALT`'s own acknowledgement and latest complete cycle, when fresh. Otherwise, such as
before that first cycle completes, it takes the best-effort path, whose route ends
unresolved. `HALT` never replaces a running stop, and a second `QUICK_STOP` is refused. RS
has no such check, so this differs on purpose.

Keep the last complete state read for `requestStop`: a read still in progress counts as
missing, and the request then takes the best-effort path. `ProbeApp` holds only its one
current read, so a caller polling through it must copy the last complete one. Packet 7 does
not add that storage, because prompt 33's move context owns its own polling.

## Best-effort stop

`BestEffortStop` is the separate DEC-2 path. It yields only `6040:00=000B`, needs no
state evidence, and is keyed to a target because its route may have no usable
reservation. It uses `CanBusOwner::admitBestEffortStop`, and `000B` is never replaced by
`010F` on this path, whatever kind was requested.

**An independent physical stop stays mandatory.** Nothing here is a safety function, and a best-effort stop may not reach or affect the drive.

**It is not a confirmed stop and it never reaches `SUCCEEDED`.** No claim is made that
`000B` stops or does not enable this drive: packet 16 owns that measurement.

| Owner terminal | `outcome` | `execution` |
| --- | --- | --- |
| Controller reported the TX | `STOP_SENT_UNCONFIRMED` | `UNKNOWN` |
| Possible transmission, no report by the overall deadline | `DEADLINE` | `UNKNOWN` |
| Possible transmission, queue or response budget spent earlier | `SERVICE_TIMEOUT` | `UNKNOWN` |
| Nothing possible | The cause, such as `SERVICE_TIMEOUT`, `DEADLINE` or `INVALIDATED` | `NOT_TRANSMITTED` |

A matching-looking `60 40 60 00` cannot make this path succeed, reopen its route or
settle the interrupted operation: classic SDO has no transaction tag, so it may answer
the write before. The route ends unresolved. A session with no usable reservation still
opens, so this branch stays available on such a route.

**When it runs.** For missing history, stale state or an unresolved route, only when
`StopPolicy::bestEffortDeadlineUs` is nonzero. Zero refuses the branch. That deadline is
fixed at the request, at or after the sequenced stop's own, and nothing extends it (W6
step 9).

**Across one recovery.** A stop proved never sent (`transmissionPossible` false, nothing
weaker) by a controller fault waits in `StopPath::HELD`, under the deadline it already had
(W6 step 10): `deadlineUs` for a sequenced stop, its own for a best-effort one. The
application runs the explicit recovery: the session does not start one, and recovery is not
a stop. Then the stop goes out once as `000B`. A second recovery, a boot or rebinding, or
the deadline ends it `NOT_TRANSMITTED`, and `firstBestEffort` keeps an earlier attempt.
**A write that may have left is never sent again**: it keeps `UNKNOWN`, and only a new
explicit request can try again. This adopts RS (R-14), except that RS keeps the requested
policy across recovery where this always re-issues `000B`.

## Support and qualification

Host tests cover these paths through the real owner and selected SDO engine, with words
manufactured by a fake. They do not establish firmware applicability, motor standstill,
stop latency, braking curve, torque, device-target retirement or reference validity.
The DEC-8 rows, whether `000B` is non-enabling, and velocity and homing modes need their
own fixture evidence. See the
[prompt 31 report](reports/prompt_31.md) for remaining obligations and checks,
and the [evidence harness](device_conflicts.md) for acquisition prerequisites.
