# Lichuan stop commands and reported stopping

The finite stop path uses `prepareStop`, `nextAction` and `advanceAction` in
`MotorControlCAN/profiles/lichuan_c/Actions.h`. The caller owns the context,
continuous sequence reservation, time and ordinary CAN owner. This is a narrow
CL86-C/OL86-C contract, with position-mode transitions and the explicitly gated
mode-3 addition below. It does not implement a safety stop, generic CiA402
stopping or a torque/brake policy.

## Mode-3 addition (prompt 35)

A fresh running mode3 state with low status bits7, alarm/released clear and the
actual latest acknowledged word `000F` admits one `010F` or `000B`. The context
freezes `stopModeRaw=3`; later observations of another mode cannot complete it.
`predecessorDocumented=false`: the arrow is a DEC-8 project decision because the
manual gives it in its position section. Explicit running HALT-to-QUICK
escalation retains that mode. Direct mode3 admission from other words is refused.

The position-mode released/latched-running feedback fallback stays mode 1 only, with one
mode 3 exception for a QUICK_STOP from `000F` that followed a running report (backlog S18,
below). The finite [velocity operation](velocity.md) requires its own stop qualification,
closed by default; no connected specimen has supplied it yet. Controller-reported
completion is not independent shaft standstill. When a velocity write could have
started motion, an older idle cycle cannot establish that no stop is needed.

**Released standstill in mode 3 (backlog S18), from drive reports.** On 2026-11-10 `000B` from a
running mode 3 run, at 30 rpm on node 2 (trial 1513) and at 60 rpm on node 1 (trial 1607), read
`2100=0014` (released, running latched), `6041=C050` and `6061=3` 28 ms later; later reads found
`2100=0015`, `6041=C050`, `606C=0` and `6064` unchanged (1514, 1608). That stop context ended
`OBSERVATION_SCOPE`, because a release was outside the mode 3 scope. A mode 3 QUICK_STOP now gets
`standstillObserving` at preparation when it followed a running report and `maximumAgeUs` covers a
seven-field cycle: `prepareStop`'s mode 3 admission (a fresh running report after `000F`), or
`prepareVelocityStop` for the run's own requested stop when the run's latest cycle reported running.
Every cycle then reads `6064` and `606C`. A released cycle with alarm clear, mode 3 and low bits `000`
stays in scope, running set or clear, and two such cycles in a row with `606C` zero and `6064`
unchanged complete the stop as `OBSERVED`. One released report never completes it. A cycle that is
not released and not running still completes it as before. Not covered: HALT, an escalation from `010F`,
a stop on the run's `000F` alone (S16) or after the run ended, a five-field age, and any other low bits;
there a release still ends the stop outside its scope. With the bench plan (interval 4000 ms, deadline
17000 ms for 12 s) the stop has two cycles, at the command and about 4 s later.

**Quick stop of a halted velocity drive (backlog S18), an attended experiment.** After a settled HALT
both drives stay enabled and halted in mode 3 (`6041=9437`, `2100=0001`: trials 1507, 1613, 1706). The
idle-report rule refused a later QUICK_STOP there (trial 1707), and nothing else leaves that state
without a power cycle. `prepareHaltedVelocityQuickStop` composes one `000B` only after an
`ATTENDED_EXPERIMENT` run that ended `SUCCEEDED` with its own HALT acknowledged and observed, while that
`010F` is still the latest word and a fresh report reads exactly `6041=9437`, `2100=0001`, mode 3.
`AxisSession` takes it only with `StopPolicy::attendedExperiment`. No source or record says what `000B`
does there. The stop reads FEEDBACK from its first cycle and completes only on the released standstill
above; a not-running report alone completes nothing, because the drive was idle before it. Its result is
unqualified and clears no fence. After an MCU restart the session has no controlword, so that path is
closed. With the same flag, no latest word and no write of the session object in the boot other than a
mode selection, a fresh report of exactly that halted state (`validateHaltedVelocityReport`: 6041 9437,
2100 0001, mode 3, and 606C 0 when the read holds FEEDBACK) lets a QUICK_STOP take the DEC-2 best-effort
`000B` where the idle report would refuse; `bestEffortDeadlineUs` must allow it. It never succeeds, may
leave the route unresolved until the next restart, clears no fence, and the session's own write ends
the exception. Host-tested only.

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
`predecessorWord` records the word. Every result below is the drive's own report on one
firmware; no shaft or torque was observed. On 2026-11-03, `010F` from `001F`
stopped a 2 rpm move on node 1 within its 500 ms ramp, and `6064` stayed 316 for 26 s (trial 113;
the attendant was not sure about the shaft); afterwards `6041=0437` and `2100=0000`, so the halted
move reads neither completion signal. `000B` was then sent stationary, unattended, on both
specimens: from `000F` on node 2 (`8050`, `2100=0011`, trial 124) and from that halted `010F` on
node 1 (`0050`, `2100=0010`, trial 126). Both drives reported low bits `000` and **released**
(`2100` bit 4), and neither moved: node 1 stayed at 316 with its target 750 still retained. On
2026-11-04 a stationary `010F` from `000F` left `6041=8437` and `2100=0001` unchanged (trial 207),
and `000B` from it was reported released again (`8050`, `2100=0011`), now completing as `OBSERVED`
(208). `000B` during a 30 rpm move on node 2, about 1,180 pulses into a 2,000-pulse move, was
reported released at once and the position stopped within about 10 pulses, but `2100` then kept
running set (`0x0014`) with `6041=4050`, minutes later too (213): see the standstill rule below;
with that rule a second moving `000B`, on node 1, completed as `OBSERVED` (trial 413). A HALT from
`001F` at 6 rpm stopped node 1 at 104 (406), and clearing it with `000F` did not resume the halted
target within 8 s (trial 408). `000B` with no known predecessor was also sent once each: on a
released drive it changed nothing (trial 307), and on node 2's never-enabled power-up state in
mode 1 it was reported released with no motion reported (trial 503). On 2026-11-05 a HALT from
`001F` at 300 rpm on node 1 (709, `010F` at `6064=3336`, 500 ms ramps) completed `OBSERVED` and
stopped at 4728, 1,392 pulses after the command, and `000F` did not resume it within 2 s (710).
`000B` from `001F` on node 2 at about 100 rpm while still accelerating (713, at `6064=854`) was
reported released with running latched, settled at 896 and completed on standstill
([unattended session](reports/2026-11-03_unattended_bench.md),
[2026-11-04 session](reports/2026-11-04_bench.md),
[2026-11-05 session](reports/2026-11-05_motion_and_stress.md), [packet 16](reports/handoff_19_33/packet_16.md)).

**From a moving `005F`, measured on 2026-11-09.** These are drive reports on one firmware and two
specimens; no shaft angle was measured. A HALT interrupting node 1's relative move (trial 832, 6 rpm,
200 ms ramps, node 2 observed stationary) read `2100=0004` with `6041=4037` in its first cycle: bit
12 already clear, running still set. The second cycle read `2100=0000` with `6041=0437`, and the stop
completed `OBSERVED` there. A HALT from node 2's moving `001F` the same morning (828, 500 ms ramps)
read running in two cycles before `0000` and `0437`, and the `000F` after it did not resume the halted
target (829: `6064` 104 before it and in eight cycles after it). Both drives stayed enabled (low bits
7, `2100` bit 4 clear). In 832, `6064` was last read at 146 with `606C=5`, just before the `010F`. The
HALT's own cycles read no `6064`, and the next read of node 1, in a later MCU boot without a drive
power cycle, found 158 (901). A quick stop interrupting node 1's relative move (1109, 6 rpm, node 2
observed stationary) read released with running latched, `2100=0014` and `6041=4050`, in all three of
its cycles. `6064` read 259 with `606C=0` in the second and third cycles, 3 pulses past the last move
cycle's 256, and the standstill rule below completed it `OBSERVED`. **After that quick stop the drive
reports itself released:** the next read (1111) again found `2100=0014` and `6041=4050` at 259. Torque
was not observed, so treat a load the motor held as no longer held. A quick stop from node 2's
acknowledged, stationary `005F` (833) also released it (`6041=8050`, `2100=0011`). Each interrupted move
was kept as its own record: `CANCELLED` with interruption `STOP`, trajectory uncertain, the session
fenced. `move_relative_test.cpp` tests 32 and 33 replay these values through the real owner and
`AxisSession` ([packet 8](reports/bench_closure_18_33/packet_8.md),
[2026-11-09 round 4](reports/2026-11-09_bench_r4.md)).

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
never enabled. Measured once, on node 2 (trial 503): the drive acknowledged it and afterwards read
`2100=0011`, `6041=8050`, `6064=0`, released with no motion reported, so its power-up holding torque
is gone by that report. The best-effort path still never succeeds and left the route unresolved, and
the shaft was not observed. Rely on the independent cutoff there, not on a CAN stop.

The sequenced path needs the frozen CL86-C/OL86-C model, and fresh STATUS reporting mode
1 and low bits 7, and fresh MONITORS reporting alarm and released clear. Fresh reports of an
**idle** drive that is alarmed, released, in another mode or not enabled refuse the request
on both paths, with no traffic, because nothing establishes what `000B` does in those
states. A running report keeps the stop available on the best-effort path, as this project's
decision that a running motor should get the attempt. Prompt 35 adds the mode-3 row below; mode6 is the separately gated prompt36 row described below.
After a finite velocity run, while `velocityNeedsStop()` holds, no idle report refuses either path: a report after a velocity word need not show it yet (bench trial 1009 read `6041=8033` after its acknowledged `000F` and was found running 37 s later). When the run's acknowledged `000F` is still the latest word, the sequenced path takes its `010F` or `000B` (`prepareVelocityStop`); its observation clears no fence, except that the run's own requested stop settles the run when the run reported running before it (and, for QUICK_STOP, on the mode 3 released standstill above).

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
that the command should have retained torque. **One exception, from drive reports:** a
`QUICK_STOP` whose cycle reads released with `6041` low bits `000`, not running and
no alarm, is `OBSERVED`, because that is what both specimens reported after `000B`.
`monitorObservation` keeps the released bit: **after a quick stop this drive reports itself
released**. Torque was not observed, so treat a load the motor held as no longer held. Every
`HALT` tried left the drive reporting enabled, not released (207, 406, 709, 828, 832), so a release
still ends it outside the scope, as does a release with any low bit set.

**Standstill after a moving quick stop, from drive reports.** After `000B` during a move the drive
reported release but kept `2100` bit 2 set while its target stayed unreached, so "not running" never came.
When a `QUICK_STOP` cycle reads released, low bits `000` and running, and
`options.maximumAgeUs` covers a seven-field cycle (seven times queue plus response), the stop sets
`standstillObserving` and every later cycle also reads `6064` and `606C`. Two such cycles in a row
with `606C` zero and `6064` unchanged then complete it as `OBSERVED`. With a five-field age budget
the rule is not offered and the stop ends at its deadline as before. The latched bit is the drive's
report, not motion. Retested on node 1 (trial 413): `000B` at `6064=1558` during a 30 rpm move, then
`6064=1569` twice with `606C=0`, `OBSERVED` about 0.6 s after the command. The latch appeared on
both specimens. The rule has completed three times on one firmware: from `001F` at about 30 rpm (413,
node 1) and at about 100 rpm while accelerating (713, node 2), and from a held `005F` at 6 rpm (1109,
node 1).

**No observation at the deadline.** An action yields no new observation read once one exchange's
queue and response budgets no longer fit before its deadline. On 2026-11-04 a stop's last read
began 6 ms before the deadline, was cut there with its request possibly sent, and left the route
unresolved until an MCU restart (Q18). The action now ends at its deadline with nothing in flight,
and the route stays usable; in trial 413 the stop's reads ended about 0.4 s before its deadline.
Commands are not held back. An alarm/fault terminates the
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

**It is not a confirmed stop and it never reaches `SUCCEEDED`.** After the sequenced `000B`
from `000F` and `010F` both specimens reported release, without being enabled first. This path sends it with no
known predecessor, measured twice on one firmware: from node 2's never-enabled power-up state it
released the drive (trial 503, through this path), and on an already released node 1 the same word,
sent by an application-local route, changed nothing (trial 307). Neither is a guarantee for another
state ([packet 16](reports/handoff_19_33/packet_16.md)).

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
On one firmware, judged from drive reports: `000B` from `000F` (124) and from `010F` (126, 208);
`010F` from `000F` (207); `010F` from `001F` three times on node 1 (113 at 2 rpm, 406 at 6 rpm,
709 at 300 rpm) and once on node 2 (828 at 6 rpm); `000B` from `001F` three times (213 on node 2 and
413 on node 1 at 30 rpm, 713 on node 2 at about 100 rpm); `010F` from a moving `005F` (832, node 1);
`000B` from a moving `005F` (1109, node 1) and from a stationary acknowledged one (833, node 2); and
`000B` with no known predecessor on two states (307, 503); in mode 3, `010F` from a running `000F`
(1407, 1413, 1507, 1613) and `000B` from a running `000F` (1513 on node 2, 1607 on node 1). These are
measurements, not a qualification: the `005F` and mode 3 rows stay project decisions (DEC-8) with no
documented arrow. Stops from `003F` and `007F`, any other state, a moving `010F` to `000B` escalation,
`000B` from a halted mode 3 drive, other rates and directions in mode 3, and homing need their own
fixture evidence. See the
[prompt 31 report](reports/prompt_31.md) for remaining obligations and checks,
and the [evidence harness](device_conflicts.md) for acquisition prerequisites.

## Mode-6 interruption (prompt 36)

`prepareStop(HALT)` accepts mode 6 only from the actual acknowledged `001F`,
under the same uninterrupted reservation and fresh state. It emits one `000F`:
the documented bit-4 falling edge, with a derived whole word and
`predecessorDocumented=false`. It adds feedback to each observation cycle and
requires monitor running clear plus zero `606C`. This is controller evidence,
not a physical standstill/reference guarantee. No enabling preamble is sent.
A confirmed mode-6 quick-stop/escalation has no reviewed transition; the existing
explicit DEC-2 unknown-state path remains unconfirmed. See [homing](homing.md).
