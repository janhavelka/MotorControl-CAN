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
4 does in mode 1. EDS `[6040]` gives a U16 write-only object while every manual page prints it RW
(conflict C08), so this library never reads it back and needs no read-modify-write.

So every row below the first is a **project decision, not vendor evidence** (DEC-8):
a running move with no stop is worse, and clearing bit 4 while stopping cannot be the
wrong direction. `ActionContext::predecessorDocumented` is false for those rows, and
`predecessorWord` records the word. One has been measured: on 2026-11-03, `010F` from `001F`
stopped a 2 rpm move on node 1 within its 500 ms ramp, and the shaft held for 26 s; afterwards
`6041=0437` and `2100=0000`, so the halted move reads neither completion signal. That is one
specimen, firmware and speed. `000B` was then measured stationary, unattended, on both
specimens: from `000F` on node 2 (`8050`, `2100=0011`) and from that halted `010F` on node 1
(`0050`, `2100=0010`). Both drives left operation for low bits `000` and **released the motor**,
and neither moved: node 1 stayed at 316 with its target 750 still retained. The other rows,
`000B` during a move, and `000B` before this library's enable stay unmeasured
([unattended session](reports/2026-11-03_unattended_bench.md), [packet 16](reports/handoff_19_33/packet_16.md)).

| Latest acknowledged `6040`, mode 1 | Stop | `predecessorDocumented` | Basis |
| --- | --- | --- | --- |
| `000F` | `000B` (`QUICK_STOP`) | True | EN86 and ZH86 p12, OP86 p10 |
| `000F` | `010F` (`HALT`) | True | ZH86 p12 and OP86 p10 only |
| `001F`, `005F`, `003F`, `007F` | `010F` or `000B` | False | DEC-8, no documented arrow |
| `010F` | `000B` only | False | DEC-8, escalation of a running normal stop |
| `000B`, `010F` for `HALT`, any other word | Rejected, output unchanged | - | No rearm, enabling preamble or inferred idempotence |
| Unknown or withdrawn word, or stale state | `000B` on the best-effort path below only | - | DEC-2 |
| None yet, after `6060=1` only: fresh mode 1, low bits 7, alarm and released clear | `000B` on the best-effort path only | - | DEC-2. The specimens' observed power-up state once in mode 1 |

**The last row is the one to avoid.** The specimens read low bits 7 and released clear at
power-up, before any enable from this library (packet 11), and the attended session found both
shafts already holding torque then. After packet 15 P2 sets mode 1, a
stop requested before P3's acknowledged `000F` therefore sends `000B` to a drive this library
never enabled, and what `000B` does in that state is the unmeasured question of packet 16.
Rely on the independent cutoff there, not on a CAN stop.

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
was admitted is older intent and does not restore it. A download whose terminal shows that
no frame of it could have left, because it was cancelled, expired or refused by the transmit
guard before the owner handed it over, changed nothing on the drive: the word before it is the
latest again, unless a stop was requested meanwhile. A copied evidence struct is not a
new observation. A stop never writes `000F` or the `6,7,15` enable sequence. The separately gated
[enable](drive_actions.md) does, through this same path, so its acknowledged `000F` is such a word. A
stop request ends an active enable or move first, even when the stop itself is refused, and even
with a time behind the session's or the owner's last time. It ends them at the later of those
times, at which the owner can still cancel a queued move download. `AxisSession` refuses after a known word with no arrow, such as enable
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
that the command should have retained torque. **One exception, measured:** a
`QUICK_STOP` whose cycle reads released with `6041` low bits `000`, not running and
no alarm, is `OBSERVED`, because that is what `000B` did on both specimens.
`monitorObservation` keeps the released bit: **after a quick stop this drive holds no
torque**, so a load the motor held is no longer held. A `HALT` held torque when
measured, so a release still ends it outside the scope, as does a release with any low
bit set. An alarm/fault terminates the
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
current read, so a caller polling through it must copy the last complete one. During or after
a move, `AxisSession::moveObservation()` (packet 20) is that copy: the last complete cycle,
never the one in progress.

## Best-effort stop

`BestEffortStop` is the separate DEC-2 path. It yields only `6040:00=000B`, needs no
state evidence, and is keyed to a target because its route may have no usable
reservation. It uses `CanBusOwner::admitBestEffortStop`, and `000B` is never replaced by
`010F` on this path, whatever kind was requested.

**An independent physical stop stays mandatory.** Nothing here is a safety function, and a best-effort stop may not reach or affect the drive.

**It is not a confirmed stop and it never reaches `SUCCEEDED`.** The sequenced `000B`
from `000F` and `010F` released both specimens without enabling them, but this path sends it
with no known predecessor, and nothing measured covers that
([packet 16](reports/handoff_19_33/packet_16.md)).

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
Two rows have one measurement each on one firmware (`010F` from `001F`, `000B` from `000F`
and from `010F`). The other DEC-8 rows, `000B` with no known predecessor, and velocity and
homing modes need their own fixture evidence. See the
[prompt 31 report](reports/prompt_31.md) for remaining obligations and checks,
and the [evidence harness](device_conflicts.md) for acquisition prerequisites.
