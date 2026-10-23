# MotorControl-RS standalone CLI contract

Arduino and native ESP-IDF compile the same
[console](../examples/probe_cli/ProbeConsole.cpp) and
[application](../examples/probe_cli/ProbeApp.cpp). `help` lists callable commands
once; `help COMMAND` supplies exact syntax and examples. The
[console guide](console.md) explains ordinary use, and the
[API/CLI inventory](ess_api_cli_coverage.md) records coverage and explicit gaps.
These sources replace the earlier speculative `motor ...` and
`profile ess-rs ...` grammar.

Plain commands produce human-readable output. An explicit `@ID` prefix selects
correlated JSONL; command IDs and admitted operation IDs are distinct. Output
format is retained for asynchronous completion and deferred stop replies.
Protocol 3 uses `stats reset` for local counters, `stop fast` for the ESS
emergency-stop request and `probe` for presence. The former `?`, `ver`, `ping`,
`reset`, `health check`, `help advanced` and `profile ess_rs` operation aliases
are unavailable. Automation must use the matching protocol-3 client.

## API and application boundary

Every motor command calls the same installed common/profile preparation and
sequencing API available to firmware. Parsing adds no private register writes,
conversion formulas or motor semantics. Host utilities call application
services; all bus traffic passes through one owner. The installed core depends
on neither the CLI nor its runtime.

The command inventory drives dispatch and help. Parsing, formatting and
application code are shared; adapters supply terminal I/O, UART/capture,
startup and time. Equivalent Arduino/ESP-IDF builds expose the same operations.
Capabilities distinguish unavailable callbacks, unimplemented features,
unresolved semantics and physical qualification.

Input/output use fixed capacities and bounded service work. Reject truncated
input, excess tokens, trailing junk, numeric overflow, unsupported units and
unresolved conversions before writes. Exact numbers retain rational or integer
precision. The input capacity is 128 bytes including NUL, with at most 22
parsed tokens and route-specific argument limits. A settings candidate must
fit and validate in full before admission.

## Commands and effects

This table groups implemented routes. `help COMMAND` and the linked operation
guides define complete arguments and prerequisites.

| Commands | Meaning and traffic |
| --- | --- |
| `help`, `version`, `profile list`, `caps` | Local syntax, identity and coverage |
| `config`, `host`, `useaddr`, `wiring` | Host selection/configuration; no drive-setting writes |
| `status`, `health`, `stats`, `drv`, `memory`, `result` | Passive cached diagnostics and retained results |
| `stats reset` | Clear local counters; preserve faults, observations and uncertainty |
| `probe [address]` | Minimal checked model read; no hidden identity/state sweep |
| `read identity\|config\|state [address]` | Explicit non-changing [typed reads](ess_reads.md) |
| `settings`, `subdivision [INTEGER\|options]` | Read actual settings; an explicit integer requests checked subdivision configuration |
| `moveby`, `moveto`, `speed`, `accel`, `decel`, `motion`, `stepsperturn` | Simple finite positioning and host intent; [console guide](console.md) |
| `axis config`, `axis origin`, `prepare` | Pure host coordinates and [target preview](axis_preparation.md) |
| `move relative\|absolute\|angle` | Detailed [finite moves](ess_position.md) with explicit frame/setup/rounding |
| `velocity` | [Finite velocity](ess_velocity.md) with explicit duration and stop policy |
| `enable`, `motor-release`, `alarm-clear`, `position-clear`, `stop normal\|fast` | Explicit [drive actions](ess_actions.md) |
| `home methods`, `home ...` | Method inventory and qualified [homing](ess_homing.md) |
| `driver`, `io`, `segment`, `control`, `tuning` | Typed reads/whole-candidate updates; [driver](ess_driver_settings.md), [I/O](ess_io.md), [segments](ess_segments.md), [control](ess_control_settings.md), [tuning](ess_tuning.md) |
| `motion-profile read\|inspect\|restore\|forget` | Retained position-parameter snapshot and explicit restoration |
| `communication ...`, `persistence ...` | Explicit [drive communication](ess_communication.md) and [save/restore](ess_persistence.md) workflows |
| `discover ...` | Bounded non-changing [scan](ess_discovery.md) |
| `monitor [off\|INTERVAL_MS COUNT]` | Inspect or enable finite state polling; `off` cancels its local continuation |
| `cancel [ID]`, `release ID`, `recover` | Local cancellation, terminal-result release and explicit host transport recovery |
| `debug [off\|raw\|decoded]`, `capture-read`, `load` | Copied traffic, fixed read-only timing fixture and optional host workload |

`config` reports host settings; `settings` reads motor configuration and position
parameters and shows next-move intent. Neither implicitly enables the drive.
`release ID` frees result storage; `motor-release` sends a drive action. Local
`cancel`, `monitor off`, `stats reset` and `recover` do not stop a motor. There
is no raw-register write command or serial segment-start command.

## Simple positioning and coordinates

`moveby VALUE [steps|deg|turn|mm]` and `moveto ...` use one finite application
session. It reads missing checked configuration/profile data and obtains
stationary state before admitting the ordinary move sequence. Speed/ramp
commands and `motion write|stored` retain intent for the next move. Full staging
before start is the default; stored reuse is explicit.

Boot intent is 60 rpm and 100 ms ramps with an explicitly ASSUMED scale of
1000 command increments per motor turn. A first checked stationary read can
establish a RAM-only boot-session zero under the standalone's documented
ASSUMED feedback-to-command convention. It does not home, clear the device
counter or write NVS. `stepsperturn` changes host interpretation only;
`subdivision INTEGER` is the separate typed device-setting workflow.

Simple moves use nearest-command-step quantization with ties to even and at
most half a step of error. Results retain effective native value and rounding
error; zero displacement after rounding yields no motion write. Detailed
requests default to EXACT and expose explicit rounding/approximation choices.
Angles and travel require the corresponding scale, frame and origin.

Host speed intent accepts 0..3000 rpm and ramp intent accepts 0..2000 ms.
Position admission additionally enforces at most 2000 rpm, 200000 command
increments/s and native ramp times 100..2000 ms. A move requires nonzero speed
and actual drive subdivision. `settings` shows the subdivision-dependent speed
ceiling; excess values reject without clamping. Native ramp time is not
physical acceleration. These defaults describe the recorded bench envelope.

Completed moves preserve the fixed origin. Release, counter changes,
unexpected motion and interpretation changes invalidate dependent confidence;
enabled feedback variation alone does not reset the zero. Uncertain execution
blocks new motion until checked stop/reconciliation. The interrupted result
remains historical evidence after that interlock settles.

`subdivision` reads settings; `subdivision options` lists accepted choices.
An explicit integer requests stopped-state checks, the public setter, readback
and the matching standalone assumed scale/zero. An unchanged value produces
no setting write. A changed scale discards the old motion-profile snapshot;
no automatic movement or nonvolatile save follows.

## Selection, observations and settings

`useaddr 1..247` is idle-only host binding, not a device-address write. Rebinding
invalidates selected-target knowledge and resets terminal wiring declarations
to unknown. `wiring` is local and changes no assignment or signal level.
Queries remain passive during faults.

[Host serial changes](host_serial.md) require settled queues, continuations,
capture and DE with monitoring off. Failure retains exclusive configuration
ownership until explicit repair; `recover` cannot clear that block. Historical
results retain their original target, tuple and generations. A routine tuple
change does not itself change device coordinates or motor configuration.

Identity, configuration and each state block have separate valid data and
attempt/success evidence. Probe success refreshes responsiveness only.
`status` and `health` generate no traffic. Age expiry defaults to disabled;
generation checks, absent evidence, transaction deadlines and operation-specific
new-event completion checks still apply.

Typed settings validate the complete candidate before writes and retain ACK,
readback, partial progress and uncertain effects separately. Matching stored
readback does not manufacture a missing acknowledgement or establish physical
activation. Unsupported native pair setters reject. I/O `none` means documented
function zero; unconnected wiring never implicitly disables an input, limit or
stop. Settings do not invent torque/current modes or external triggers.

`motion-profile restore` reserves the axis until checked readback matches the
snapshot. Failed/uncertain restoration retains `restore_unsettled`, blocking
competing writes, rebinding, forget and another restore. Explicit transport
repair, configuration/state reads and `motion-profile read` can reconcile the
stored values. An uncertain write also requires stationary evidence obtained
after delivery. Original write bytes, deadline and uncertainty remain intact.

Communication, persistence and discovery use exclusive application sessions.
Planning/inspection are passive; writes are explicit. Save ACK, live readback
and restart survival are different evidence. Discovery retains findings and
restores host selection before ordinary work resumes; a fault requires explicit
recovery/restoration and does not restart the scan.

## Admission, stop and retained outcomes

One owner serializes UART transactions. Same-axis conflicts reject before
writes; sequences release bus time while waiting without releasing their axis
reservation. Contexts advance through operation-specific `next...`/`advance...`
APIs using supplied events/time. Admission respects operation deadlines and
any required service cadence.

Stop uses reserved urgent admission, invalidates superseded ordinary work and
waits for the in-flight frame/direction boundary to settle. Preserve both the
interrupted operation and stop outcomes. Requested deceleration and device
queue policy must be supported; bus failure cannot guarantee physical stopping.
Stop interrupts simple preparation only after urgent admission succeeds.

Cancellation ends unsent work/local waiting, settles physical TX/DE and retains
uncertain transmitted effects. It does not reverse a write or erase evidence.
No timeout, startup, recovery, discovery or polling path automatically replays
motor commands. ACK is distinct from observed completion; old completion flags
cannot satisfy a new operation.

`result ID` is non-consuming. Explicit correlated/API and failed/uncertain
results remain retained until released. Delivered successful human-command
results can be recycled on the next bus command only after terminal output
leaves both queues. Simple session IDs own their read/move/setting children;
child IDs expose full retained evidence. Historical failure retention alone
does not reserve the motor after checked stop/reconciliation.

Human action/motion terminals are concise; JSON inspection retains diagnostic
detail. `debug raw|decoded` copies owner traffic without consuming protocol
bytes or blocking transactions under output pressure. Dropped observations
are counted. Debug mode uses the same production operation path.

## Verification and qualification

Native console/application tests cover help parity, strict parsing, direct API
equivalence, zero-traffic rejection, provenance, retained results, pressure,
stop/cancel and commissioning isolation. Adapter tests exercise shared code
against SDK fakes; firmware builds check platform boundaries. See
[verification](verification.md) for required checks.

[Named Python scenarios](bench_scenarios.md) use strict correlation and bounded
evidence. The default regression reads only; motion/settings/persistence require
explicit selection. Each result identifies firmware, target, settings and
fixture; it cannot qualify every callable route. The
[release scope](releases/1.0.0.md), [coverage inventory](ess_api_cli_coverage.md)
and original vendor PDFs retain remaining limits.

FieldCore integration uses its own device module and existing owner; it does
not embed this CLI or reuse measurement requests as motor commands. See the
[integration handoff](fieldcore_handoff.md).
