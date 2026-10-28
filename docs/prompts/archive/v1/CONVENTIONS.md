> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Concrete ecosystem conventions for MotorControl-CAN

This is a **selected implementation contract**, not implemented code. Apply it with
[COMMON.md](COMMON.md), the active stage and the [multi-drive decision](../../../multidrive_decision.md).
The fresh source inventory and differences are recorded in
[CONVENTIONS_REAUDIT.md](../../../audit/CONVENTIONS_REAUDIT.md). Recheck the live sources
at every implementation stage. An older report does not override current code.

The choices below resolve the naming and ownership decisions for the promptset.
If a later source-backed change is necessary, update this contract, the
[API/CLI map](../../../api_cli_mapping.md), affected callers and tests together, and
record why in the stage report. Do not create competing synonyms quietly.

## 1. Public names, layout and dependencies

| Concern | Selected CAN convention | Evidence and boundary |
| --- | --- | --- |
| Package | `MotorControl-CAN` | Agreed project identity |
| Namespace, include directory, CMake identity | `MotorControlCAN`; imported target `MotorControlCAN::MotorControlCAN` | Match the RS distinction between package spelling and C++ identity |
| First profile namespace | `MotorControlCAN::LICHUAN_C` | Proposed; follows RS `MotorControlRS::ESS_RS` style |
| Public code | `include/MotorControlCAN/`, `src/`, profile subdirectories | RS structure; install/export only portable code and required generated headers |
| Types and functions | `PascalCase` types; `camelCase` functions/fields; uppercase enum values | Actual RS public API; do not import FieldCore's PascalCase enum values |
| Tests and tooling | `test/`, `scripts/`, `.github/workflows/verify.yml` | Actual RS names; do not introduce parallel `tests/` and `tools/` trees by copying a sensor template |
| Application and console | `examples/common/`, `examples/probe_cli/`, `examples/probe_idf/` | Shared owner/application/CLI with thin framework adapters; native usage remains independent |
| Language floor | Installed C++11, proven by source and installed consumers | RS CMake explicitly sets C++11 for its standalone test consumers |

Use short files for real responsibilities. The architecture tree is a plan, not
a requirement to create empty headers. Extend an existing operation, owner or
profile before adding another engine, registry, abstraction or lifecycle.
Keep profile catalogue strings optional where codecs do not need them.

The core contains free functions, fixed value types and concrete caller-owned
contexts. It never includes Arduino, ESP-IDF, FreeRTOS, FieldCore or a chosen
CANopen stack. It never opens a device, samples a clock, sleeps, logs, allocates,
starts a thread, retries a command or persists application state. CAN stack and
SDK objects stay in the application adapter. Keep copied MIT attribution;
vendor reference files are not part of the core's software license or package.

## 2. `Status` is a call result

Copy the **RS shape and spellings**, not the I2C static factory convention:

```cpp
// Selected MotorControlCAN surface; not implemented here.
struct Status {
    Err code;
    int32_t detail;
    const char* msg; // Static lifetime.
    constexpr bool isOk() const;
    constexpr explicit operator bool() const;
};
constexpr Status Ok();
const char* errToString(Err code);
```

Preserve default-success and explicit `(Err, int32_t, const char*)` construction.
Use `status.isOk()` or explicit bool consistently. Do not add `ok()`,
`Status::Ok()`, `Status::Error()` or `inProgress()` aliases to imitate sensor
classes. `nextMove()` returning `Ok()` means that call validly produced work,
waiting or a terminal view; it does not mean the motor completed a move.

The selected initial `Err` vocabulary is `OK`, `INVALID_CONFIG`, `ILLEGAL_VALUE`,
`UNSUPPORTED`, `FRAME_ERROR`, plus the proposed CAN addition `PROTOCOL_ERROR`.
The first five preserve matching RS meanings. `PROTOCOL_ERROR` means a checked
native service rejection, such as an SDO abort. It does not mean a malformed
frame, controller bus-off or a drive fault. Do not copy `CRC_ERROR` or RTU
`EXCEPTION`: the CAN controller handles CAN CRC and an SDO abort is not a Modbus
exception. Do not copy I2C error codes, `OFFLINE` or `IN_PROGRESS` into `Err`.

Use concrete `ReadError`, `ActionError` and profile-specific reason enums for
documented `detail` values, as RS does. A sequence's outcome records timeout,
cancellation and transport failure; its call `Status` and retained failing-step
`Status` remain distinct. Do not add a new top-level `Err` merely to duplicate an
existing outcome. Any extra code needs an actual failing call and caller test.

`detail` is signed 32-bit. Keep raw SDO abort in a separate `uint32_t abortCode`
inside CANopen evidence, with an explicit validity flag. Preserve the entire
value, including the high bit. Retain raw controller detail, received bytes,
object index/subindex, statusword, controlword and EMCY fields in their own typed
evidence. `msg` is diagnostic text, never a machine decision key or an owning
string. Unknown enum values stringify to a static `UNKNOWN` label.

Preparation/parsing validate the whole candidate before changing caller output.
Document explicit count-reset exceptions. Retained contexts own admitted input
and result bytes; parser input is borrowed for that call only.

## 3. Motion, typed reads and terminal evidence

Reuse these exact RS operation families where their meanings apply:

| Intent | Selected public functions | Terminal inspection |
| --- | --- | --- |
| Identity/configuration/state reads | `prepareIdentity`, `prepareConfig`, `prepareState`, `nextRead`, `advanceRead` | Checked `getIdentity`, `getConfig`, `getStateBlock` into typed observations |
| Explicit enable/release/fault action/stop | `prepareEnable`, `prepareRelease`, `prepareClearAlarm`, `prepareStop`, `nextAction`, `advanceAction` | Concrete `ActionContext` members |
| Finite position and angle | `prepareMoveAbsolute`, `prepareMoveRelative`, `prepareMoveAngle`, `nextMove`, `advanceMove` | Concrete `MoveContext` members |
| Finite velocity | `prepareVelocity`, `nextVelocity`, `advanceVelocity`, `serviceVelocity` where required by the implemented contract | Concrete `VelocityContext` members |
| Homing | `prepareHome`, `nextHome`, `advanceHome`, `getHomeReference` | Concrete `HomeContext`; checked reference publication |
| Typed native settings | `prepareDriverRead`, `prepareDriverSettings`, `nextDriver`, `advanceDriver`, `getDriver`, `getIo` where the reviewed Lichuan groups fit | Concrete settings context and typed observations; documented native group additions |
| Host units/coordinates | `parseExactNumber`, `convertDisplacement`, `convertVelocity`, `convertAcceleration`, `preparePosition`, `prepareVelocityTarget`, `validateAxisConfig`, `configureAxis`, `setAxisOrigin`, `invalidateAxisReference` | Pure checked outputs; no device I/O |

These names are observed in RS; their CAN implementations and signatures are
planned. Retain `state`, `outcome`, `status`, `execution`, `completion`,
`uncertain` and operation-specific evidence in the concrete contexts where
applicable. Do not invent `getMoveResult`, `getActionResult`, `getResult`,
`advanceOperation` or a universal `OperationContext`. Checked read getters reject
incomplete/wrong-kind/stale provenance without changing output. A snapshot with
an unknown raw value can remain a successful read; validity flags determine
which interpretation is available.

Preserve `ReadState` and `ActionState` values `EMPTY`, `ACTIVE`, `SUCCEEDED`,
`FAILED`. Preserve the meanings of `ActionExecution::{NOT_TRANSMITTED,
ACKNOWLEDGED,REJECTED,UNKNOWN}` and `ActionCompletion::{NOT_OBSERVED,OBSERVED}`.
Keep operation-specific `ReadOutcome` and `ActionOutcome`; use their existing
`REPLY_ERROR`, `TRANSPORT_ERROR`, `CANCELLED`, `DEADLINE` and observation outcomes
where applicable. Add proposed `PROTOCOL_REJECTED` to distinguish a checked SDO
abort from `REPLY_ERROR`. `ACKNOWLEDGED` needs the profile's actual service
confirmation; controller TX success or CAN ACK alone cannot establish it.
`OBSERVED` means the documented controller evidence was observed, not an
independent physical measurement of standstill or electrical safety.

The names share intent across stepper and servo drives. Common axis identity is
logical, separate from protocol endpoint/channel/subaxis. Do not copy RS
`ReadTarget.address`, `ActiveSerialTuple`, `uint16_t speedRpm`, signed native RPM
restrictions, subdivision prerequisites or encoder assumptions into common CAN
types. Retain exact rational targets, explicit coordinate frame, scale provenance,
rounding/error budget and checked native conversion. Full motor steps are
optional stepper metadata. Mechanical RPM, electrical RPM, command increments,
encoder counts, motor/load turns and raw ramp times remain different quantities.
Profile-native integer or floating representation must be explicit.

N5 stepper/BLDC and EPOS4 remain source-backed test contrasts, not installed
profiles. CANopen NMT/SDO/PDO/statusword semantics stay below common motion.
Future proprietary protocols have their own services and evidence; do not give
them fictional CANopen state. No placeholder torque/cyclic APIs are required for
the first position/velocity servo contract.

## 4. One application CAN owner

Select `CanBusOwner` as the example application owner type in
`MotorControlCANExample`. This name is proposed. Its selected method vocabulary
follows the actual RS `BusOwner`: `admit`, `admitUrgent`, `service`, `result`,
`release`, `cancel`, `recover`, `recoveryResult`, `releaseRecovery`,
`beginSequence`, `invalidate` and `invalidateProducer` where those contracts
are needed. Do not rename these to `submit`/`poll` just because an I2C driver
uses a poll method. Do not copy unrelated commissioning methods without a caller.

Select `RequestId`, `SequenceId`, `BusAdmission` and `Completion` for equivalent
example-owner concepts. Their CAN fields are separately designed; RTU address,
byte-gap, echo and DE/RE fields do not apply. A request ID is local ownership
identity, never an SDO wire transaction ID. A `Completion` is a retained service
result, not a motion context or motion completion. `result(id)` is a
non-consuming view; `release(id)` explicitly releases a matching terminal.
Admission rejection leaves ID output and queued work unchanged. A result view's
lifetime ends on release or owner destruction. Foreign/stale IDs fail.

One cooperative application context owns the controller, receive dispatch,
chosen stack, queues, caches and service turns. All passive heartbeat/PDO/EMCY
events continue through that owner during SDO work. Diagnostics take copies;
they never drain controller RX. Per-axis reservations can span waits without
reserving the entire bus. Fixed ordinary/urgent queue and result quotas protect
explicit stop admission; fairness and bounds are tested. Service calls have
declared RX/TX/event budgets, including zero-budget behavior.

Controller queue acceptance, actual transmission, CAN ACK, SDO confirmation,
drive acceptance and observed motion completion are separate evidence. Queued
SDK/controller frames participate in generation invalidation. A late identical
SDO reply may be indistinguishable from a fresh reply: local generation does not
solve that wire ambiguity. Retire/reconcile channels explicitly. Recovery never
replays work, enables a drive or clears its fault. Local cancel is not stop.

## 5. Selected application health and metrics

**All names in this section are proposed CAN example/application names.** RS
has no installed `Health`/`DriverState` lifecycle. Sensor diagnostics are useful
precedents, not a shared binary interface or universal admission policy.

Use one cached `Health` per logical binding and `DriverState : uint8_t` with
`UNINIT`, `READY`, `DEGRADED`, `OFFLINE` in example code. Keep the state member
named `state`. Expose cached `health(binding)` and `driverState(binding)` on
`CanBusOwner`; reject an unknown binding explicitly rather than returning a
fabricated healthy record. These accessors perform no I/O or clock reads.
Do not add a second `state()` alias: the owner already has multiple kinds of
controller, protocol and operation state.

Use this fixed field vocabulary in `Health`:

| Field | Meaning and owner |
| --- | --- |
| `totalSuccess`, `totalFailures` (`uint32_t`) | Completed attempted logical services for this binding; saturate at `UINT32_MAX` |
| `consecutiveFailures` (`uint8_t`) | Consecutive failed attempted services; saturates at `UINT8_MAX`; a successful logical service clears it |
| `lastOkUs`, `lastErrorUs` (`uint64_t`) | Supplied monotonic microseconds of last successful/failed logical service; paired `lastOkKnown`/`lastErrorKnown` flags make timestamp zero valid |
| `lastError` (`Status`) | Most recent failed service status; retained after later success; full raw failure evidence remains in retained completions/bounded diagnostic history |
| `lastSeenUs`, `lastSeenKnown` | Most recent checked evidence from this endpoint, including valid service rejection or valid passive event; liveness only |
| `state` (`DriverState`) | Communication projection described below; no assertion of enable, readiness, standstill or absence of drive fault |

The counted unit is one admitted service request with a defined endpoint reply
or confirmation, such as one SDO upload/download. Count it once at its terminal
outcome, including all segments. A multi-object read or move can contain several
counted services; do not also count its overall operation in this owner. An
independently admitted read or stop service counts normally, even when requested
after a failure. Its success can clear the current service-failure streak but
cannot rewrite the original operation, erase `lastError` or settle that
operation's unknown effect. Fire-and-forget network commands without endpoint
confirmation do not count as communication successes merely because TX succeeds.

Explicit probes through a selected reviewed service count here; this deliberately
differs from the health-neutral probes in several sensor libraries. Local invalid
arguments, unsupported capability, queue refusal, waiting and locally cancelled
services do not change success/failure totals or the failure streak. Count local
cancellation in `Stats.cancelled`, whether transmitted or not, unless an earlier
eligible terminal event wins under the documented precedence rule. Preserve
possible external effects and motion interlocks independently.

A service that times out or terminates on a checked malformed response fails.
Unrelated or unmatched frames update diagnostics, not failed-service counters;
they do not themselves terminate the pending request or renew its deadline.
A checked SDO abort fails the service and also proves endpoint communication;
retain both facts. Passive events update `lastSeenUs`, not service counters or
`lastError`.

The selected projection is passive. Apply these rules in order: invalid binding
is `UNINIT`; unavailable controller/bus-off or expired configured and evidenced
endpoint-liveness deadline is `OFFLINE`; a current failed-service condition is
`DEGRADED`; checked current-generation endpoint evidence is `READY`; otherwise
`UNINIT`. `READY` describes communication only. Missing heartbeat is inconclusive
when producer/consumer timing is zero, unknown or inapplicable; do not invent a
failure-count threshold or heartbeat default.

A fresh valid heartbeat can clear an evidenced liveness expiry and return to
`READY` or `DEGRADED` according to the retained service-failure condition. It does
not clear that condition. Controller recovery invalidates old endpoint freshness;
new checked evidence is needed. A binding-generation change resets that binding's
entire current `Health`, including counters, timestamps and `lastError`, while
retained operation results and bounded historical diagnostics keep their original
binding/generation. Interface `Stats` survives rebinding. Application enable,
motion inhibition and machine health remain separate from this projection.

Do not introduce `HealthPolicy` or `LATCH_OFFLINE` in CAN to suppress requested
work. Explicit transport, generation, channel, capacity, profile and operation
prerequisites determine admission. An unavailable controller can prevent TX;
a projected diagnostic label alone does not block a serviceable explicit stop,
read or reconciliation path. Health recovery never clears a drive fault,
re-enables or resolves an uncertain write. A successful read never rewrites an
old failed operation as successful.

Use one owner `Stats` for interface/queue metrics, with cached `stats()` and
`clearStats()` as in the RS runner. Select `txFrames`, `rxFrames`, `txRejected`,
`rxDropped`, `unmatchedFrames`, `queueRejected`, `resultsRejected`,
`cancelled`, `timeouts`, `busOffEvents`, `recoveries`, `traceOverwritten` and
`maxServiceGapUs` for the implemented counters/high-water mark. All counters
saturate; the gap is an observed maximum, not a scheduling guarantee. Define
each increment at its actual owner boundary in `docs/can_owner.md`. In particular,
`txFrames` means confirmed controller transmission if that evidence exists;
queue acceptance must not be labelled transmission. Do not duplicate per-binding
logical-service totals inside `Stats`; aggregate them when formatting a report.

`clearStats()` and CLI `stats reset` clear interface counters/high-water marks
and per-binding total counters only. They preserve `state`, `consecutiveFailures`
(live health evidence), timestamps, `lastError`, binding/configuration generations,
controller recovery interlocks, faults, uncertainty, retained results and current
work. Keep operational history in its bounded evidence store; its explicit
clear/export policy is different from statistics. Document every reset lifetime.

Keep `ControllerState`, CANopen `NmtState`, `Cia402State`, operation mode,
heartbeat freshness, statusword/controlword, EMCY, service outcomes and `Health`
independently inspectable. Protocol-specific snapshots are present only for that
protocol. FieldCore maps these facts into its own `HealthState`; the core never
includes or imitates that application's enum.

## 6. CLI, Python and platform parity

Use [the API/CLI mapping](../../../api_cli_mapping.md) as the selected inventory.
Follow RS canonical `help`, `version`, `config`, `status`, `health`, `stats`,
`drv`, `probe`, `read identity|config|state`, `axis`, `prepare`, `moveby`,
`moveto`, `move`, `velocity`, `home`, `enable`, `motor-release`, `alarm-clear`,
`stop`, `result`, `cancel`, `release ID`, `recover` where implemented.
`stats reset` is local. `status`, `health`, `stats`, `drv` and `result` use
cached observations/results. Explicit reads alone refresh device evidence.
`release ID` frees a result; `motor-release` changes drive state.

The exact RS console removed `?`, `ver`, `ping`, bare `reset`, `health check`
and `profile ess_rs`; sensor examples still having such aliases do not authorize
reintroducing them. Keep CANopen `nmt`/`pdo`/`heartbeat`/`network` groups scoped
to CANopen. Distinguish host selection/configuration from explicit drive
node/bitrate/persistence changes. Discovery never broadcasts mutating commands
to identify a drive. Default one protocol domain per physical interface.

One command inventory drives help, dispatch, mutation classification, both
firmware adapters, native console tests and Python parser contracts. Retain the
RS `@ID` correlated JSONL model and plain human output, with an explicitly
different CAN schema/library identity. RS protocol version 3 does not establish
CAN schema compatibility. Bound input/output, queue capacity, duration and
result retention. Do not emit truncated JSON as a valid result or round an exact
native integer through floating point. Unknown evidence appears explicitly,
not as zero-valued success. Field names identify units and observation scope.

Arduino and native ESP-IDF compile the same application, owner and console;
platform sources provide clock/transport/startup only. Runners import serial
support only for live mode, correlate command and operation IDs, preserve raw
terminal evidence, reject duplicate/missing/malformed responses and never
replay an uncertain command. Dry-run/parser success is host evidence only.
Motion and persistent/network mutations require their explicit application
workflow; an I2C stress/recovery policy is not a motor command policy.

## 7. Documentation, Doxygen and meaningful gates

Document public structs/enums/functions in the same change as implementation.
Use Doxygen `@file`, `@brief`, `@param`, `@return`/`@retval` and `@note` where
they clarify the actual contract. Every fallible operation documents units,
ranges, validity, mutation, borrowed/owned lifetimes, output-on-error, context
reuse, terminal retention, time domain/deadline, capacity and uncertainty where
applicable. Avoid boilerplate claims that do not match the code. Each native
object/function has original document/EDS/model/firmware evidence in its ledger.

Choose a strict Doxygen build for the new library. RS supplies Doxygen-style
comments but has no root Doxyfile in the inspected checkout; strict generation
is an intentional addition based on the I2C and FieldCore documentation gates.
Stage 03 establishes the configuration and local/CI command; later stages keep
warnings and broken links at zero. Do not report this future gate as present.
Keep architecture/user guides, API/CLI coverage and the stage report current,
with documented/implemented/host-tested/hardware-qualified columns distinct.

Use the RS CMake/CTest pattern: explicitly register `test/*_test.cpp` suites,
keep assertions enabled in Release, compile source and installed consumers in
C++11, and run actual Python tests through the host verifier. Preserve focused
fault fixtures; do not test only helper internals. Stage 03 adapts current
`python scripts/verify.py --mode quick --build-dir build/verification` and
full Arduino/IDF modes into the new repository, with CAN's real gates and pinned
SDKs. Those commands are RS precedents, not working CAN commands today.

At minimum cover atomic failure output; wrong target/generation/request/step;
typed getters before completion; retained result pressure; zero budgets; exact
deadline boundaries; late identical SDO ambiguity; continuous passive events
during a service; controller queue invalidation; checked abort versus malformed
reply; abort high-bit preservation; unconfirmed writes; stop capacity under
pressure; health counter scope/saturation/reset; known timestamp zero; disabled
heartbeat; no replay after bus-off; and source/install/profile-subset consumers.
Use N5 stepper/BLDC, EPOS4 and non-CANopen contrast fixtures to catch native
width/unit/identity assumptions. A fixture is not hardware qualification.

The stage report identifies exact inspected sibling paths/HEAD/dirty state,
intentional differences, exact local commands/results, unavailable checks and
remaining evidence. Documentation-only edits use local integrity checks. Major
implementation milestones follow the parent final-commit CI policy when a
remote exists. Never claim old RS CI or a sensor dry-run verifies a CAN commit.
