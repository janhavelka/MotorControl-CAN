# Finite bench scenarios

The Python campaigns orchestrate the ordinary shared console described in the
[CLI contract](cli_contract.md). They use the same `Console` and checked evidence
helpers as [the runner guide](runners.md). Plans, host fake results, live console
observations and physical qualification remain separate. Prompts 47–48 own live
campaign qualification; implementing a plan does not qualify its motor behavior.

`bench_scenarios.py` lists and runs six named scenarios. `bench_motion.py` exposes
the same finite action plans with a read-only `quick` default. `bench_repeat.py`
compares only ordinary position moves with `setup write` and `setup verify`.
There is no arbitrary-command or arbitrary-scenario option on the repeat tool.
`--list` exists only on `bench_scenarios.py`; all three tools accept `--plan-only`.
Imports, help, listing, plans and host tests require no serial package or hardware.

| Scenario | Required explicit campaign inputs | Public commands and expected evidence |
| --- | --- | --- |
| `quick` (default) | None beyond live session identity when executing | One `read identity TIMEOUT_MS` after the session's initial `probe`; checked upload terminal. No motor writes. |
| `position` | `--target`, `--kind relative\|absolute`, `--rpm`, `--ramp-up-ms`, `--ramp-down-ms`, `--setup write\|verify`, `--stop-policy normal\|fast` | Two host ramp intents, one detailed native-coordinate move, one preplanned independent stop. Each native result remains separate. |
| `stop` | `--stop-policy normal\|fast` | One standalone stop. This does not demonstrate stopping a moving motor. |
| `velocity` | `--rpm`, `--duration-ms`, `--stop-policy normal\|fast` | One finite motor-RPM velocity request with its native stop. One separate cleanup stop is preplanned only if the run fails. |
| `homing` | `--method`, `--search-rpm`, `--return-rpm`, `--home-ramp-ms`, `--stop-policy normal\|fast` | One exact method-specific zero-offset home, followed by one preplanned independent stop. No implicit enable or input reassignment. |
| `settings` | `--driver-settings RPM UP_MS DOWN_MS` | One complete `driver set` candidate. Native terminal retains confirmed/changed masks and possible effects. No snapshot upload, restoration, save or durability claim. |

The session's first active upload is `probe`, on the DEC-1 accepted list. Ordinary
configuration, homing-parameter and I/O uploads remain Q20-gated; no campaign uses
them to manufacture missing prerequisites. Settings require an externally prepared
fresh native settings session and handoff guard. Its absence is a refusal with
the gate retained, never a pass. No campaign enables a drive to satisfy admission.

Position targets are exact signed 64-bit decimal integers in **native command
increments**, without a float conversion. Relative moves explicitly use the
actual-position basis. Native preparation checks its representation, documented
profile range, geometry, limits and reference; accepting a host integer does not
promise that it is a legal drive target. Absolute moves require an existing
qualified origin/reference. A relative displacement of zero is rejected as a
motion experiment. The runners do not convert encoder counts to command pulses.

Motion RPM and ramps are positive campaign choices, with RPM at most 3000 and
ramp times at most 2000 milliseconds. The complete settings candidate also
accepts zero where the existing typed CLI accepts it. These ceilings follow the
existing CLI and the retained EN86 manual PDF pages 11 and 13; EDS sections
`6081`, `6083`, `6084`, `6098`, `6099sub1`, `6099sub2`, `609A` were rechecked.
Campaign positivity is host policy, not a new vendor minimum. Velocity is positive
only; conflicting negative `60FF` claims are not resolved by this runner.
Homing methods are exactly 17, 18, 24 and 29; the creep input is RPM despite the
manual's conflicting “creep time” label. Native firmware qualification remains
required. See [motion evidence](audit/LICHUAN_MOTION_FACTS.md).

Velocity duration is explicitly 1–30000 milliseconds, a bounded campaign policy.
Its host `--timeout` must exceed that duration plus the firmware application's
10-second action budget, and may not exceed 60 seconds. Native stop deadlines are
separate supplied policies. Velocity consumes the application's externally
prepared velocity profile: position `accel`/`decel` host intent does not alter it.
The runner therefore rejects position ramp flags on a velocity plan; the supplied
fixture configuration must retain the actual velocity ramps and scale.

Every device campaign requires a supplied fixture record for live execution.
The session validates protocol/profile/model/firmware/dialect and relevant
capabilities and provenance before opening the port. Position, velocity and home
require scale facts; position and home require reference facts. Stop facts name
the exact planned `normal` or `fast` method. Home additionally requires
`io.home_inputs_permit:true` and an integer `io.method` equal to the requested
method. Sections carry their evidence references. The record is caller-supplied
provenance; neither it nor cached console capabilities prove observed firmware
identity or arm native application qualification. A missing or mismatched fact
rejects execution. Plans without a fixture remain explicit unexecuted intent.
The [runner guide](runners.md) specifies the shared fixture and artifact schema.

Examples below print plans only and create no files:

```text
python scripts/bench_scenarios.py --list
python scripts/bench_scenarios.py --plan-only
python scripts/bench_motion.py --plan-only --scenario position --target 100 --kind relative --rpm 60 --ramp-up-ms 100 --ramp-down-ms 200 --setup verify --stop-policy fast
python scripts/bench_motion.py --plan-only --scenario velocity --rpm 60 --duration-ms 1000 --stop-policy fast --timeout 15
python scripts/bench_motion.py --plan-only --scenario homing --method 17 --search-rpm 60 --return-rpm 30 --home-ramp-ms 100 --stop-policy fast
python scripts/bench_scenarios.py --plan-only --scenario settings --driver-settings 60 100 200
python scripts/bench_repeat.py --plan-only --count 3 --target 100 --rpm 60 --ramp-up-ms 100 --ramp-down-ms 200 --stop-policy fast
```

For live execution remove `--plan-only` and supply the explicit shared `--port`,
`--baud`, `--profile`, `--node`, `--can-bitrate`, `--expected-library-version`,
`--out` and, for device campaigns, `--fixture`. No serial port, CAN endpoint,
bitrate, speed, scale or calibration default is guessed. The node range belongs
to this selected CANopen profile; it is not a universal axis identity rule.

Repeat accepts `--count 1..20` write/verify pairs (default one), at most 40 moves.
Each move uses the same explicit relative displacement and speed. Its plan
reports the exact aggregate requested displacement so the operator can assess
the complete travel before execution. Each terminal is checked/released before
the next move, and the summary retains each policy, target, speed and terminal.
One stop is planned for the entire comparison, including early failure. `verify`
means native `REUSE_STORED`: it can write differences. CAN has no working
stored-only setup policy. The comparison retains console evidence; it makes no
physical latency or unsupported setup-write-count claim.

Cleanup belongs to the selected scenario and occurs on its existing session.
It is a distinct, finite operation, attempted at most once when a motion handoff
is possible and framing/evidence remain usable. A refusal before any handoff
does not trigger a stop. A poisoned/disconnected stream sends no cleanup command,
and its missing stop evidence remains unknown. Successful cleanup cannot rewrite
a failed or uncertain primary command. Failed cleanup also makes an otherwise
successful campaign fail. There is no reconnection, automatic command replay,
implicit fault clearing or automatic profile restoration.

Exit status is 0 for a distinct `planned` result or successful requested console
work, 1 for execution/cleanup/artifact failure, and 2 for invalid input. A plan's
zero exit never means hardware qualification. Exclusive streamed JSONL and bounded
summary artifacts are owned by `bench_session`; their paths, limits and collision
rules are documented in [the runner guide](runners.md). Native status, outcome,
execution, observed completion and uncertainty are retained separately. A stop
terminal does not establish physical standstill, electrical timing or restoration.

If artifact closure fails, invalidating an independently writable report ZIP does
not depend on repairing the failed summary file. The original failure and streamed
evidence remain retained; a storage error never becomes a passing campaign.
