# Finite Lichuan velocity

The native speed, ramp and position units, their whole-rpm and ramp-time limits and the measured delivered rate are
explained in [Speed, ramp and position units](speed_units.md).

`prepareVelocity`, `nextVelocity`, `advanceVelocity` and `serviceVelocity` operate
on caller-owned `VelocityContext`. `AxisSession::requestVelocity` connects them
to the existing owner and urgent stop. No CLI or second SDO engine is required.
The [offline example](../examples/velocity_preview/README.md) shows exact rate
conversion and the closed capability gate. The native fake-owner tests exercise
the complete sequence; they establish no firmware or physical qualification.

## Admission and source boundary

`FiniteVelocityRequest` adds a positive `durationUs` to the unchanged pure
`VelocityRequest`. Lichuan `60FF` requires integer motor turns per minute:
`nativeRateBasis=MOTOR_TURNS`, `nativeVelocityTime=MINUTE`, integer representation.
Conversion happens once through `prepareVelocityTarget`; the context retains
the engineering request, effective signed rpm, quantization error and binding.
Binary32, command-increment rates, a stale generation, zero speed, overflow and
unqualified reverse requests fail before work. Positive native range is bounded
by the caller's evidenced range and the documentary 3000 rpm ceiling. Negative
encoding is not evidence of reverse behavior. Ramp times are positive integer
milliseconds, at most 2000; undocumented zero meaning is rejected.

`VelocityEvidence` is closed by default. Opening it asserts an actual retained
record for the selected model/raw firmware tuple, status layout, startup, chosen
stop and rate range. Mode changes while released and reverse operation require
their separate facts. `modeChangeFromRaw` names the one qualified source mode;
a mode1-to3 record cannot authorize mode0/6-to3. Admission requires a fresh complete MONITORS, STATUS and
FEEDBACK read, released/not enabled, no running or alarm, and raw velocity zero.
Changing mode on an enabled drive is refused, including home-to-move workflows.
Neither power-up nor an earlier position trial establishes these conditions.
The session also requires the admission cycle to follow its latest write: a
recent but contradicted pre-run released sample cannot admit another run after
a halt left the drive enabled.

One exception to "no running" (backlog S18): after a moving quick stop both specimens report
their release with 2100 running still latched, `2100=0015`, `6041=C050`, mode 3, `606C=0` and
`6064` still (reads 1514 and 1608), so without it no second run could start in the same
power-up. Admission takes that latch only in mode 3, only together with the release, 6041 low
bits 000, alarm clear and 606C zero. Word 6's cycle may still show the latch and must then read
the admission's 6064: the second fresh report of one standstill before any enabling word. From
word 7's cycle on the latch must be clear, as word 7 cleared it in mode 1 (trial 308,
`6041=0033`, `2100=0000`); otherwise the prefix fails with the run's stop requested. In mode 3
this was measured once, on node 2 (trial 1742): word 6's cycle still read `2100=0015`, `6041=C031`
at the admission's 8412, and word 7's read `2100=0001`.

### Attended experiment scope

`VelocityGate::ATTENDED_EXPERIMENT` admits one finite run whose mode 3 start and
stop are what the run measures, not facts it claims. It follows the existing
experiment gates (`EnableGate::ATTENDED_EXPERIMENT`, the `ISOLATED_EXPERIMENT`
scopes of the PDO and communication settings). Under it `startQualified`,
`stopQualified`, `modeChangeQualified` and `reverseQualified` must all be false,
so the drive must already report released and stationary in mode 3 and only a
positive rate can be prepared, unless the reverse probe below is set. The rate is at most
`VELOCITY_EXPERIMENT_MAXIMUM_RPM` (60 rpm) and the duration at most
`VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US` (20 s). These two bounds are project
policy for an unmeasured start and stop; they say nothing about the drive.
`record` names the experiment's own retained record. Every other check is the
same as for `MEASURED`: exact identity and dialect, released stationary mode 3
admission, the ZH86 p13 order, duration coverage and stop reserve, the fixed
deadlines, the run's own stop at due, retained uncertainty after a possible
transmission, no retry and the checked fault path.

A claim inside the experiment (either qualification flag, a mode change or a
reverse range) is refused as `ARGUMENT`; a rate over the bound likewise, and a
longer duration as `TIME`. A `MEASURED` record without both start and stop
facts stays `UNSUPPORTED`/`CAPABILITY`, and `CLOSED` stays the default. The
context keeps the gate it was prepared with: a run under the experiment never
reads as qualified, and a successful terminal qualifies nothing by itself. The
attendant's independent power cutoff is the control the library cannot see.

**Range probe (prompt 59).** `VelocityEvidence::rangeProbe`, admitted only under
`ATTENDED_EXPERIMENT`, raises the experiment's rate bound from 60 rpm to
`VELOCITY_RANGE_PROBE_MAXIMUM_RPM` (6000 rpm, twice the documented 3000) and
refuses ramps under `VELOCITY_RANGE_PROBE_MINIMUM_RAMP_MS` (200 ms). Both are
project policy for the attended range ladder and say nothing about what the drive
or motor reaches; `60FF=3001` was stored unchecked (trial 2001), so the drive's own
behavior above 3000 is what the probe measures. Any other gate with `rangeProbe` is
refused as `ARGUMENT`, and a probe run qualifies nothing.

**Reverse probe (prompt 59 item 4).** `VelocityEvidence::reverseProbe`, admitted only
under `ATTENDED_EXPERIMENT`, is the run that measures what a negative `60FF` does. No
source states a direction rule: the EDS and the p17 dictionaries declare `60FF` I32,
the EN86/ZH86 p13 mode tables give it as UNSIGNED32 0~3000 ([Q03](device_conflicts.md),
open), and `606C` already reads signed rpm (-60 during a 60 rpm move in the negative
direction, trial 706). With the probe, `maximumReverseRpm` states the experiment's
reverse range, 1 to the experiment's cap (60 rpm, or 6000 with `rangeProbe`), and a
rate down to its negation is admitted and written as its two's complement I32 (-6 is
`23 FF 60 00 FA FF FF FF`); positive rates are unchanged and zero stays refused. From the enable report on, before running is
observed too, a cycle whose `606C` opposes the command by more than
`VELOCITY_REVERSE_PROBE_SIGN_MARGIN_RPM` (2 rpm, one cruise step) ends the run as
`OBSERVATION_SCOPE` (`"reported velocity opposes the command"`) with its stop requested;
without the probe a wrong-sign report before running is only no running evidence. The
margin is project policy, and `606C` is the drive's report, not an independent direction
observation. `reverseQualified` stays a `MEASURED` claim that only a reviewed reverse
record (prompt 58) sets; the probe beside it, or under any other gate, is refused as
`ARGUMENT`, and a probe run qualifies nothing. A sign change during a run stays with
prompt 52's measurements (F15).

**Over-speed stop (every gate).** An enable-report or running cycle whose `606C`
magnitude exceeds the command's by more than a fifth plus
`VELOCITY_OVERSPEED_MARGIN_RPM` (10 rpm) ends the run as `OBSERVATION_SCOPE` with
its stop requested. The bound is project policy; `606C` is the drive's own report
(2 rpm steps at cruise on both specimens), not an independent speed observation.

The selected conditional sequence follows **ZH86 PDF p13**: mode 3, observe mode
3, target/ramp writes, then `6,7,15`. **EN86 PDF p13** enables before parameters
and refers to position trigger bits; the library does not combine those recipes.
EDS `[60FF]` specifies I32/default120, while the mode table says U32/0..3000 rpm.
The signed representation is retained without inferring a negative operating
range; only the reverse probe above sends a negative value, as an experiment.
OP86 p11 agrees with ZH86 but has the retained 57/86 heading conflict.
See [Q03/Q11](device_conflicts.md) and the [original facts](audit/LICHUAN_MOTION_FACTS.md).

## Sequence and timing

| Phase | Work and evidence | Failure consequence |
| --- | --- | --- |
| Prepare | Copy intent, axis/configuration, identity/dialect/range record and fixed budgets | Atomic refusal; no admitted work |
| Optional mode | `6060=3` only from qualified released state; acquire fresh `6061=3` and state | No alternate sequence or silent release |
| Parameters | `60FF` I32, `6083` U32, `6084` U32, each once with its terminal retained | Partial effects remain; no retries |
| Enable | `6040=6`, then7, then15, with fresh observations and at least nominal10 ms between acknowledged words | Unknown or contradictory evidence ends the run; possible effects stay |
| Run | Bounded state/feedback cycles, matching mode/dialect and direction | No inferred physical movement from a successful write |
| Stop due | A separate urgent stop operation through the same owner, original deadline | Occupied/unresolved channel policy remains authoritative |
| Terminal | First run failure, setup/start/stop evidence and stop observation retained separately | Later health/recovery or separate stop cannot rewrite this result |

Preparation at `t0` fixes `stopDueUs=t0+durationUs`. Queue pressure, mode setup,
parameters and enable consume this interval. Neither admission nor an ACK
restarts it. The supplied `deadlineUs` remains fixed and must leave the checked
caller stop reserve after due. Duration must be covered by `maxPolls *
pollIntervalUs`, and all time arithmetic is checked within `0..INT64_MAX`.
`maxPolls` bounds running observation cycles. The three enable confirmation
cycles, the optional mode confirmation and any final-enable window cycles (below)
have their own fixed bound and do not consume that allowance. `observations` retains the total cycle count;
`runningPolls` records the running subset. An accepted duration at the exact
coverage limit must not stop early merely because setup required observations.

Call `serviceVelocity` only after consuming and releasing any admitted service
terminal. At due it requests stop settlement; strictly later than
`stopDueUs+pollIntervalUs` records `SERVICE_TIMEOUT`. Bounded stop work may still
run within the original deadline, while preserving that first failure. At
`nowUs >= deadlineUs`, processing expires even if the response arrived earlier.
Each start/setup download has its own cutoff at stop due and must fit its
queue/response budget before admission and handover. The owner accepts a shorter
service cutoff without changing the operation deadline; expiration retires
queued work or retains uncertainty for work the controller cannot retire.
A later stop is a new explicit action with its own budget, never an extension.
Local cancellation emits no stop.

The application rechecks frozen rate/configuration and time at admission.
Its transport's `beforeTransmit` must call `AxisSession::preflight` for the
same checks at controller handover: a queued write cannot start after due or
after configuration invalidation. One receiver continues to dispatch other
nodes, heartbeat, PDO and EMCY. Fault events must reach the session's checked
fault path as for position operations.

## Stop and retained uncertainty

The mode-3 DEC-8 row accepts the actual latest acknowledged `6040=000F`, fresh
enabled/running mode3, then one `010F` or `000B`. It sets
`predecessorDocumented=false`: the manuals place those arrows in their position
section. This is project policy, not a manufacturer statement or a safety stop.
No enabling preamble is sent. Direct mode3 stop from other words is refused;
explicit running HALT-to-QUICK escalation uses the existing action path.

The position-mode release fallback is reused for mode 3 only for a QUICK_STOP that followed a
running report, and only as its standstill rule (backlog S18, below). Successful mode3
settlement needs its own supported fresh state observation; service ACK alone is insufficient. An unresolved channel can take only the
explicit best-effort `000B` route under DEC-2, only when QUICK_STOP was selected:
one transmission, execution
`UNKNOWN`, no correlated reply, no automatic resend. A HALT request grants no
permission to substitute `000B`; a later explicitly requested QUICK_STOP is
independent. See [stop policy](stop_actions.md).

`needsStop` begins at the first possibly enabling write and survives uncertain
failure or cancellation. `AxisSession::velocityNeedsStop` fences later motion,
enable and counter changes until a new checked stop observation settles the
application fact. The old concrete velocity result stays unchanged. Reopening,
controller recovery, ordinary health and terminal-slot release cannot clear it.
Controller-proved non-transmission of the first enabling word refines its
provisional needs-stop notice; earlier possibly transmitted words still count.
Parameter writes retire retained `MOTION_PARAMETERS`, including the other mode's
shared `6083/6084` snapshot; they do not advance configuration generation.

### The stop after the run's acknowledged 000F

Once the run's final `000F` is acknowledged, every failure that asks for the
run's stop sends it, within the run's deadline, whatever the drive reports:
`010F` for HALT, `000B` for QUICK_STOP, each exactly once. `AxisSession` first
tries the ordinary mode 3 admission above. When that refuses, and the run's
acknowledged `000F` is still the session's latest controlword, it prepares the
same stop with `prepareVelocityStop`, which needs only that word: a report after
the `000F` need not show its effect yet (see trial 1009 below). Neither stop sets
an enabling bit the `000F` did not already request, so this is the run's own stop
after its own word, not an enable and not a replay. The same path serves a later
explicit `requestStop` while `velocityNeedsStop` holds and that `000F` is still
the latest word, for example after a local cancel.

Such a stop is unqualified. The stop's own context records what it saw, but its
not-running report settles nothing unless the run's latest cycle before the stop
request reported the drive enabled and running (6041 low bits 111, 2100 running
set, no alarm or release, mode 3), the report an ordinary mode 3 admission needs.
Otherwise the drive was not reported running before the stop either.
`advanceVelocityStop` then keeps `needsStop`, `completion` `NOT_OBSERVED` and the
first failure, and the session keeps `velocityNeedsStop`; a stop admitted on the
`000F` alone never clears that fence.

Before the run's final `000F` is acknowledged no such stop exists: from words 6
or 7, `010F` would be the first enable-operation word, and HALT allows no `000B`
substitute. A QUICK_STOP run can still take the DEC-2 `000B` path when the stop
policy allows it. When the requested stop cannot be composed at all, the session
records that with `advanceVelocityStop(context, reason, now)`: the run ends
`FAILED` with its first failure, `phase` `STOPPING`, `stopRequested` set,
`stopOperation` zero, `stopOutcome` `INVALIDATED` (or `DEADLINE` at or after the
deadline) and `stopExecution` `NOT_TRANSMITTED`, and `velocityNeedsStop` stays.
Before this change the same case ended with `interruption` `INVALIDATED`, which
named a lifecycle change that had not happened.

`velocityNeedsStop` is the application's signal and must not be ignored: the
drive may start or keep running. When `velocityAction().stopExecution` is
`NOT_TRANSMITTED`, no stop of the run left (local cancel, withdrawal, deadline,
or a requested stop that could not be composed), and the application must stop
the axis itself: `requestStop`, or the independent cutoff where that refuses.
While the fence stands an idle report no longer refuses either stop path, so an
explicitly chosen QUICK_STOP can take the DEC-2 `000B` after such a run even when
a fresh read reports word 7's state. Local cancellation still sends nothing.

The session alternates two shared state-read buffers so a stop sees the last
complete cycle, not a partial newer cycle. Starting a move or velocity operation
reuses those buffers; retain a copy if an application needs older raw cycles.
The concrete context keeps owned summaries and service terminals.

### Quick stop in mode 3 and the next run (backlog S18)

Measured on 2026-11-10 ([rounds 8 and 9](reports/2026-11-10_bench_r8.md)), drive reports on both
specimens and one firmware: `000B` from a running run at 30 rpm (trial 1513, node 2) and 60 rpm
(trial 1607, node 1) read `2100=0014` (released, running latched), `6041=C050`, `6061=3` 28 ms
later, and the later reads 1514 and 1608 found `2100=0015`, `6041=C050`, `606C=0` with `6064`
unchanged. Those runs ended `OBSERVATION_SCOPE`, uncertain and fenced, because the mode 3 stop
scope refused a release.

Now the run's own QUICK_STOP, when the run's latest cycle before the request was a running report
(the `runningReport` rule above) and the stop's `maximumAgeUs` covers seven fields, reads 6064 and
606C from its first cycle (`ActionContext::standstillObserving`). Two released cycles in a row with
alarm clear, mode 3, low bits 000, 606C zero and 6064 unchanged complete it as `OBSERVED`, running
latched or not, and the run then ends `SUCCEEDED` with `needsStop` clear. One released report never
completes it, and a cycle that moved, read 606C nonzero, an alarm, another mode or other low bits
leaves the run fenced as before. An explicit QUICK_STOP that `prepareStop` admits on a fresh running
report after the run's `000F` follows the same rule and, once observed, clears `velocityNeedsStop`.
A QUICK_STOP on the run's `000F` alone (S16, no running report before it, or after the run ended)
keeps the old scope and never clears the fence, and HALT is unchanged.

**After the release the drive is released:** torque was not observed, so treat a load the motor held
as no longer held. With the bench plan (12 s, interval 4000 ms, deadline 17000 ms) the stop gets two
cycles, at the command and about 4 s later; the route's terminal bound already counts seven fields per
stop cycle (125 of 128), and its record and runner need no change. A plan with one stop cycle (such as
interval 5000 ms with deadline 15500 ms) cannot settle a release and ends at its deadline.

A run may then start from that latched release (admission above). After a settled HALT the drive
stays enabled and halted in mode 3 (`6041=9437`, `2100=0001`), which no velocity admission accepts
and the idle-report rule refuses a QUICK_STOP from (trial 1707). For the attended experiment only,
`prepareHaltedVelocityQuickStop` sends one unqualified `000B` from exactly that report while the
run's own `010F` is still the latest word ([stop actions](stop_actions.md)); it completes only on the
released standstill and clears no fence. After an MCU restart that `010F` is gone, so with the same
flag, no controlword and no write of this session object in the boot, exactly that report lets a
QUICK_STOP take the best-effort `000B` instead; it never succeeds and may leave the route unresolved
until the next restart. Host-tested only: no record shows what `000B` does there.

## Trial 1009 (2026-11-09)

What happened, from the [record](bench/20261109T1446Z-p3536-velocity-node1-1009/)
on image `4247e59`: the attended bench `velocity` route ran node 1 (CL86-C) under
the attended experiment scope, 6 rpm, ramps 200 ms, 12 s, HALT, ZH86 layout, from
mode 3, released and stationary (`6041=8050`, `2100=0011`). `60FF=6`, `6083=200`
and `6084=200` were acknowledged, then word 6 (next cycle `8031`/`0011`), word 7
(`8033`/`0001`) and word 15, acknowledged. The cycle after word 15, read about 4 to
50 ms after its acknowledgement, still reported `8033`/`0001` with `6064=0` and
`606C=0`. The run failed there (`OBSERVATION_SCOPE`, final enable not confirmed)
and asked for its HALT. No stop was sent: the run ended `FAILED` with
`interruption` `INVALIDATED`. A read-only route about 37 s later found node 1
running (`6041=C437`, `2100=0004`, `606C=5`, `6064` about 3600 and rising) until
the attendant opened the drive-power cutoff.

Cause, reproduced on the host by `velocity_owner`'s literal replay: three gates
in `AxisSession::requestStop` each refused that HALT. `stateRefuses` refused both
paths because a fresh report after the velocity word showed an idle drive in mode
3; `validateStop` refused the sequenced path because the report did not show low
bits 111 with 2100 running; and a HALT policy has no best-effort branch. The session
then ended the run with `INVALIDATED`, and the bench route, seeing no active run or
stop, finished the record without acting on `velocityNeedsStop`. The rules behind
the first two gates assume that a report after an acknowledged word shows its
effect. This record contradicts that assumption for word 15.

Fix: the run's own stop after its acknowledged `000F`, and the explicit record of
an uncomposed stop, as described above. The replay now sends `2B 40 60 00 0F 01 00 00`
at once after the failure; the run ends `FAILED` with its first failure, the HALT
acknowledged, `completion` `NOT_OBSERVED` and the fence kept.

The lag, as observed: in all 13 retained records in which word 15 was sent on this
firmware (12 mode 1 enables on both nodes, 2026-11-03 to 2026-11-09, and 1009 in
mode 3), the first cycle after its acknowledgement, with 6041 read about 23 to 28 ms
after it, still reported `6041=8033`. In each mode 1 enable the next cycle, about
850 to 880 ms after the acknowledgement, reported `8437`; the enable action waits
and re-reads, so it passed. In mode 3 nothing was read between about 50 ms and 37 s,
when the drive was running. So in mode 1 the drive reported operation enabled more
than 28 ms and at most about 880 ms after the acknowledgement; in mode 3 the time is
not measured. The velocity sequence accepts only the first cycle after word 15, so
on this firmware a run is expected to fail there, as 1009 did, and now ends with its
own stop. No wait is assumed by default; the explicit window below exists only for
the attended experiment that measures that time.

What is documented, measured and qualified stays separate. Documented: ZH86 p13's
order and the position-section arrows from `000F` to `010F` and `000B`. Measured, on
one specimen and one firmware: the report lag above, and that node 1 ran at about
the commanded rate after 1009's acknowledged `000F` with no stop sent. Not measured:
what `010F` or `000B` does in mode 3, during or after that lag, and whether the drive
honours a HALT sent before it reports operation enabled. Nothing here is qualified.

## Final-enable report window (backlog S17)

`VelocityOptions::enableWindowUs` and `enableSpacingUs` (both default 0) let an
attended experiment wait a bounded time for the enable report after word 15
instead of failing at the first cycle. The window is the application's bounded
wait, not a device timing: no record establishes when this firmware reports the
enable in mode 3, so preparation refuses a nonzero window under any gate but
`ATTENDED_EXPERIMENT`. Outside the experiment the window would claim that the report
arrives within it.

| Field | Rule |
| --- | --- |
| `enableWindowUs` | 0 keeps the one-cycle rule exactly. Otherwise at most `VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US`, measured from word 15's acknowledgement |
| `enableSpacingUs` | 0 without a window; with one, 1 to `enableWindowUs`, and `enableWindowUs / enableSpacingUs` at most `VELOCITY_ENABLE_WINDOW_MAXIMUM_CYCLES` (16) |

Inside the window, a final-enable cycle that still shows word 7's report exactly
(mode 3, clean, `6041` low bits 011, `2100` released and running clear, `606C` 0,
`6064` word 7's cycle's) schedules another cycle `enableSpacingUs` later. The reference was the
admission sample until 2026-11-10: on node 2 word 7 itself moved 6064, from 8412 and from 8419 to
8424 (trials 1742 and 1762; word 6's cycles still read the admission's value), and word 15's first
cycle read 8033 at 8424 with `606C` 0, so both runs failed there and sent their own `000B`. Word 7
re-energizes the drive, which in mode 1 also returned the position report to its last commanded value
(trial 308). No bound on that pull is claimed; any change after word 7's cycle still fails. Word 7's
6064 is in the record's RESULT lines (the run's 6064 read after the second `6040` download) and, once a
window cycle passed, in `pendingFirst` (`TRIAL_VELOCITY_SAMPLE which=2`). It
does so only when that cycle becomes eligible at most `enableWindowUs` after the
acknowledgement and before stop due. Any other report fails as without a window,
with the run's own stop requested (the S16 path), and so does the last such report
the window admits ("final enable not reported within its window"). A cycle that
reports low bits 111 with `2100` released clear continues exactly as before:
running evidence, `maxPolls` and the stop at due. `needsStop`, the session fence and
the run's own stop after its acknowledged `000F` are unchanged.

Bounds: the window adds at most `enableWindowUs / enableSpacingUs` cycles. Cycle `j`
of the window is eligible no earlier than `j` spacings after the acknowledgement. The
context shape check counts them in the setup observation bound, and
`enablePending` is at most one more. The window's last cycle starts at most
`enableWindowUs` after the acknowledgement and has the first enable cycle's
budget, so preparation adds `enableWindowUs` to the setup time the duration must
cover: 27 services (35 with a mode change), plus the window, plus two command
intervals.

Record: `enablePending` counts the cycles that still showed word 7's report;
`pendingFirst` and `pendingLast` keep the first and last of them, and `enableReport`
the cycle that first reported the enable, with or without a window. Together with
word 15's acknowledgement (`commandHistory[2].completedUs`) they bracket the time to
the enable report. The bench velocity route uses a 2000 ms window with cycles
300 ms apart, the experiment's chosen budget ([hardware bench](hardware_bench.md)).

## Remaining specimen evidence

No retained record establishes mode3 startup, reverse operation or a mode3 stop
on the connected CL86-C specimens; trial 1009 above measured a start that the
library failed and that was stopped only by the attendant's cutoff. One related
fact is measured: on 2026-11-09 the
bench `mode3` route found node 2 released (`2100=0011`, `6041=8050`), mode 1 and
stationary at `6064=-49` on two fresh reads, wrote `6060=3` once
(`2F 60 60 00 03 00 00 00`, acknowledged), and then read `6061=3` twice with the
drive still released and at -49 ([record](bench/20261109T1125Z-r2c-mode3-node2-917/),
[report](reports/2026-11-09_bench_r2.md)). One measurement, one specimen, from a
released state, with no enable and no motion; `bench_route` replays it value for value
(`replayRouteBootR2c`). It qualifies no mode change while enabled, no startup or stop
and no other source mode, and Q11 stays open. The following conditional prompt29 experiment
can establish a narrow positive subset; it is not an automatic production run.
The bench `velocity` route runs it under the attended experiment scope above
([procedure](reports/bench_closure_18_33/velocity_homing_procedure.md)); only a
reviewed record of that run, not the run itself, could later back a `MEASURED`
gate for that firmware tuple:

1. Retain fixture/software/raw identity, actual non-enabled stationary state,
   current mode, target/ramp values, I/O state, independent cutoff and shaft
   observation provisions. An enabled position-mode specimen is not an admissible
   mode-change experiment; no release command is inferred to obtain that state.
2. Independently review one exact positive target-before-enable sequence, its
   nonzero ramps, duration/total deadline and stop word. Set literal experimental
   values and bounds from the actual fixture, rather than treating EDS defaults
   or a host timeout as drive timing evidence. Use the same public codec/service
   and owner through the prompt29 acquisition route.
3. Record every request/response and fresh state before and after mode3 and each
   of6/7/15. Observe the first motion, direction and approximate rate independently;
   test the one selected stop with independent cutoff available. Do not depend on
   the disputed stop/status interpretation for the only termination provision.
4. A failed/unknown command terminates that trial without replay. Establish a new
   externally checked session/state before another explicitly bounded trial.
   Update the record and focused regression if a firmware fact contradicts the
   candidate sequence. A positive trial qualifies neither reverse nor zero,
   all rates, both stop kinds, another specimen or a different firmware tuple.

`60FF` typed readback is `DriverGroup::VELOCITY_TARGET` and
`DriverObservation::velocityTargetRpm`. It retains the I32 bit interpretation;
general acquisition remains Q20 `UNQUALIFIED`. The earlier one-per-enable
accepted prerequisite read does not authorize continuous velocity polling.
