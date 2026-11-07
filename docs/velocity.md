# Finite Lichuan velocity

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

The selected conditional sequence follows **ZH86 PDF p13**: mode 3, observe mode
3, target/ramp writes, then `6,7,15`. **EN86 PDF p13** enables before parameters
and refers to position trigger bits; the library does not combine those recipes.
EDS `[60FF]` specifies I32/default120, while the mode table says U32/0..3000 rpm.
The signed representation is retained without inferring a negative operating
range. OP86 p11 agrees with ZH86 but has the retained 57/86 heading conflict.
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

The position-mode released/latched-running fallback is **not** reused for mode3.
Successful mode3 settlement needs its own supported fresh state observation;
service ACK alone is insufficient. An unresolved channel can take only the
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

The session alternates two shared state-read buffers so a stop sees the last
complete cycle, not a partial newer cycle. Starting a move or velocity operation
reuses those buffers; retain a copy if an application needs older raw cycles.
The concrete context keeps owned summaries and service terminals.

## Remaining specimen evidence

No retained record establishes mode3 startup, reverse operation or a mode3 stop
on the connected CL86-C specimens. The following conditional prompt29 experiment
can establish a narrow positive subset; it is not an automatic production run:

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
