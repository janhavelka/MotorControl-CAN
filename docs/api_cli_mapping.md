# API and CLI naming map

Prompt 38 implements `LICHUAN_C::prepareCommunicationRead`, `nextCommunication`,
`advanceCommunication` and `getCommunication` for finite raw records, and
`prepareCommunication`, `noteCommunicationTransmission` and explicit
`prepareCommunicationConfirmation` for the reviewed heartbeat transition.
These are native APIs with owner tests; production console routes remain prompt41.
See [commissioning](commissioning.md#explicit-communication-commissioning) for
the exact read, write and acquisition scopes.

Prompt37 implements `getDiscoveryProfile`, `getDiscoveryCapabilities`, profile
`prepareProbe/checkProbe`, `ProbeApp::select` and the optional `DiscoveryScan`.
They are native APIs today; discovery CLI fields/routes remain prompt41. Read
[commissioning](commissioning.md) for bounded slots and identity/effect limitations.

| New public API | Scope | CLI status |
| --- | --- | --- |
| `LICHUAN_C::preparePositionParameters` | Pure native pulse/rpm/ms candidate with exact owned axis/intent; no move/service work | No implemented route; future motion routes must first satisfy prompt 33 gates |

This mapping preserves current MotorControl-RS names where their meanings apply. Exact signatures must be audited against live headers in each implementation stage. The stage notes identify implemented APIs; remaining inventory entries are planned contracts.

The [S2 bench controls](../examples/s2_bench/README.md) are explicit experimental
acquisition commands, not the future production CLI. `RawEvidence` uses the same
owner and an exact one-use upload grant; `SERVER_INDICATED` retains actual wire
length without replacing typed getters or EDS metadata. Owner
`localTxRetired(slot)` inspects local queue/report retirement only; it does not
qualify an SDO session or prove absence of earlier remote effects.

Prompt 6 now demonstrates `prepareIdentity/nextRead/advanceRead/getIdentity`,
`prepareMoveAbsolute/nextMove/advanceMove` and the owner admission/result lifecycle
in an [experimental synthetic profile](profile_contract.md). That page maps
proof declarations to route intent. Prompt 19 now installs the finite
[Lichuan raw read lifecycle](reads.md), including checked identity/state/config
getters and SDO/PDO evidence. No CLI route exists yet. The non-consuming probe
remains Q20 gated; the inventory below retains later operation/CLI owners.

[Prompt 18](reports/prompt_18.md) is Partial: the fixed TPDO1 map subset in
`PdoConfiguration.h` is implemented and specimen-observed, and the `EVENT_TIMER`
transition (`1800:05` raw 0 or 100, CL86-C, Pre-operational) is implemented, host-tested and
replayed once on node 2 in both directions,
with its native caller in `examples/pdo_configuration_preview`. Broader remapping, other
timer values and sample qualification stay gated by Q17, and no CLI route is implemented.
`configurePdo` installs host map evidence only; raw SDO access and EDS metadata
do not substitute for those missing operations.

The selected detailed vocabulary is centralized in [CONVENTIONS.md](prompts/CONVENTIONS.md), backed by the [fresh source audit](audit/CONVENTIONS_REAUDIT.md). Use that contract for exact status, context, owner and health names; this file maps them to user-facing intent and CLI routes.

The [multi-drive decision](multidrive_decision.md) makes these common motion names apply to both stepper and servo drives. CANopen and Lichuan are the first concrete backend/profile, not mandatory public axis semantics. Future families have their own namespaces and typed contexts; test-only N5/EPOS4/proprietary contrasts must not appear as installed drivers.

## Style and status

Prompt36 implements `prepareHome`, `nextHome`, `advanceHome` and checked
`getHomeReference`, with the [homing contract](homing.md) and six typed native
reads. Concrete terminal contexts retain outcomes; no `getHomeResult` is added.
CLI42 routes remain planned and are not implemented by these APIs.

Prompt35 implements `prepareVelocity`, `nextVelocity`, `advanceVelocity`,
`serviceVelocity`, concrete `VelocityContext`, and `AxisSession::requestVelocity`.
`DriverGroup::VELOCITY_TARGET` uses the existing checked driver read/getter path.
The [velocity guide](velocity.md) records timing, exact rpm, firmware gates and
separate stop evidence. No `getVelocityResult` or velocity CLI route is added;
prompt42 owns a future route calling these APIs. Physical mode3 qualification
and general 60FF Q20 acquisition remain pending.

Wrapped angles now have a public core/owner path: `prepareMoveAngle`,
`validateMoveAngle`, shared `nextMove` / `advanceMove` and
`AxisSession::requestMoveAngle`. [Angle policy](position_parameters.md#wrapped-angle-moves)
and the [offline consumer](../examples/angle_preview/README.md) are implemented;
the `move angle` CLI route remains owned by prompt 42.

Prompt 32 adds conditional `prepareClearAlarm` and zero-only
`prepareSetDevicePosition`, using the installed `nextAction/advanceAction`.
The [fault/reference guide](fault_reference_actions.md) defines literal writes,
retained trigger/rearm evidence and the axis-aware `noteActionTransmission`
overload. No corresponding CLI route is installed; the controlword reset route
and unsupported device-position values do not gain fallback implementations.

Prompt 30 installs profile `getStateInterpretation` in `State.h`, consuming checked
STATUS and MONITORS reads for explicit documentary comparison.
Packet 10 adds `interpretStatusword` for one raw word, with the same meanings. Unknown bits and
disagreement remain inspectable; no firmware is selected automatically. There is
no CLI route for an enable. `prepareEnable` exists and is gated: it ran attended on both specimens,
on one firmware, through a bench route ([attended report](reports/2026-11-03_attended_bench.md)),
which qualifies nothing, and `prepareRelease` does not exist. The [transition guide](drive_actions.md) records both.
Prompt 31 installs `prepareStop/nextAction/advanceAction` and the concrete
`ActionContext` for the [conditional native stop path](stop_actions.md), including
`noteActionTransmission` at the owner's external handoff. No stop CLI is installed.

Prompt 29 extends example `ProbeApp` with explicit `requestState`, checked
`getStateBlock` and local `cancelRead`. The application-only
[`DeviceEvidence`](../examples/common/DeviceEvidence.h) provides `request`,
`requestExperiment`, `service`, `interrupt`, cached `record` and terminal
`release`. Its exact finite plan selects existing typed identity/configuration/
state reads; it exposes no arbitrary object command or CLI route.
`FirmwareProbe::evidence()` has fixed DEVICE provenance and is serviced by the
ordinary firmware turn. Ordinary DEVICE acquisition remains Q20 rejected;
explicit reviewed unqualified-read experiments retain possible/uncertain effects.
The [offline procedure](../examples/device_evidence/README.md) demonstrates the
path with synthetic records. State-changing experimental sequences remain
unimplemented and capability-gated; fixture and review strings alone grant no
physical authorization.

Prompt 26 adds example `ProbeApp` selection, explicit `requestIdentity` and
`requestConfig`, `service`, `advanceRead`, checked `getIdentity/getConfig`,
concrete `readContext` inspection and explicit `releaseRead`. It uses the
existing owner `result/release` lifecycle. [The shared application](native_application.md)
contains no command parser or live startup procedure; future CLI routes must
invoke these public paths instead of adding a second receiver.

Prompt 25 implements application `Health`, `DriverState`, `Stats`, checked
`health/driverState`, cached `stats/resources`, local `clearStats`, explicit
host heartbeat-age policy and bounded `takeDiagnostic` copies. Their rules are
in the [owner guide](can_owner.md#passive-communication-health). The future CLI
must use those cached APIs; no health/stats CLI is implemented here.

Prompt 23 implements application `RequestId`, `SequenceId`, `BusAdmission`,
`Completion` and `admit/service/result/release` in examples/common.
`beginSequence/beginOperation` preserve exclusion across prerequisite and setting
operations. [The owner contract](can_owner.md) defines the controller/event
boundary; operation stepping stays outside the scheduler. Prompt 24 adds
`admitUrgent`, per-request `cancel`, `invalidate`, `invalidateProducer`, `recover`,
`recoveryResult` and `releaseRecovery`, with independent urgent/recovery storage
and explicit transport retirement. These are application APIs, not CLI routes
or validated drive stops. The next operation may retain previous terminal results.

Prompt 22 adds `prepareControlRead`, `prepareTuningRead`, `getControl` and
`getTuning`, plus explicit `prepareControlSettings` through the existing
`nextDriver/advanceDriver` lifecycle; packet 14 adds its clean state read.
[The tuning guide](tuning.md) lists exact rows, the conditional 2500 write and
each withheld row's next evidence. Planned CLI owner is 41; no route or broad
tuning-write capability is implemented.

The [fresh 20/21 audit](reports/prompts_20_21_audit.md) adds real native
`prepareDriverSettings`, `prepareSoftLimitSettings`
and `prepareIoSettings`, overloaded
`nextDriver/advanceDriver`, `noteDriverTransmission` and checked typed readback
getters. Confirmed position parameters, the 2202 selector and DI settings
are explicit bounded register transactions; no CLI, enable, physical direction or
output-action route is implied.


Prompt 21 adds four [I/O read groups](io_settings.md) through the same
`prepareDriverRead/nextDriver/advanceDriver` lifecycle with checked `getIo`.
Output actions and unresolved DO reassignment remain explicitly gated; future CLI routes do
not supply missing firmware/terminal semantics.

Prompt 20 implements `prepareDriverRead/nextDriver/advanceDriver/getDriver` for
the [native read groups](driver_settings.md); prompt 35 adds the fourth group, raw `VELOCITY_TARGET` (`60FF`), with its Q20 acquisition gate. Confirmed settings progression is implemented; future CLI routes and unresolved
setting groups retain their assigned owners.

Use free `camelCase` functions/fields, `PascalCase` types, `CAPS_CASE` constants and enum values, namespace `MotorControlCAN`, and profile namespace `MotorControlCAN::LICHUAN_C`. Keep `Status { Err code; int32_t detail; const char* msg; }`, free `Ok()`, `Status::isOk()`, explicit bool and free `errToString()` returning static-lifetime labels. Preserve `OK`, `INVALID_CONFIG`, `ILLEGAL_VALUE`, `UNSUPPORTED` and `FRAME_ERROR`; select `PROTOCOL_ERROR` for a checked native service rejection. Do not retain `CRC_ERROR` or RTU `EXCEPTION`. A raw SDO abort needs a separate `uint32_t abortCode` with validity in protocol evidence; never truncate or silently sign-convert it into generic detail.

I2C siblings differ in lifecycle, status helpers and health enum members. Carry over their single-owner, bounded job and cached diagnostics conventions. Do not invent a shared `begin()/update()/health()` interface. The stateless core has no lifecycle service. The example `CanBusOwner` uses actual RS `admit/admitUrgent/service/result/release/cancel` vocabulary, with adapter-specific signatures documented.

## Common operation parity

| Existing name | CAN requirement | Deliberate distinction |
| --- | --- | --- |
| `parseExactNumber`, `rationalToDouble` | Same strict exact-number/allowance semantics | No float round trip for native position |
| `convertDisplacement`, `convertVelocity`, `convertAcceleration` | Explicit dimensional scales and overflow checks | Native ramp time remains a different quantity |
| `validateAxisConfig`, `configureAxis` | Host-only config, stationary evidence, generation changes | No CAN write implicit in configuration |
| `setAxisOrigin`, `invalidateAxisReference` | Same host reference semantics | No silent device-counter reset |
| `preparePosition`, `prepareVelocityTarget` | Pure exact target/rate preparation | Remove ESS-only native RPM width from common types |
| `getDiscoveryProfile`, `getDiscoveryCapabilities` | Finite explicit profile inventory | Model/firmware/dialect scope; no universal CiA402 compatibility |
| `prepareProbe` | Typed upload of `1000:00` under DEC-1, with no write intent; effect recorded as unknown | Reviewed SDO/passive evidence; absent heartbeat may be inconclusive |
| `prepareIdentity`, `prepareState`, `prepareConfig` | Typed bounded observations | Logical axis/binding generation is common; native node/channel/subaxis route is protocol-specific |
| `prepareMoveAbsolute`, `prepareMoveRelative`, `prepareMoveAngle` | Same intent, coordinate, quantization and failure semantics | Device handshake and actual scaling are Lichuan-specific |
| `prepareVelocity` | Finite signed velocity with explicit duration/stop policy | Wire units, signedness and start sequence require profile evidence |
| `prepareHome` | Explicit documented method and prerequisites | Do not transfer ESS home method numbers |
| `prepareSetDevicePosition` | Explicit supported device-coordinate change only | Reject if no documented equivalent; origin remains available |
| `prepareEnable`, `prepareRelease` | Explicit drive state change | Confirm state transition; NMT operational alone is insufficient. `prepareEnable` is gated, unrun on any specimen and has no console route. `prepareRelease` stays absent, with no source-backed command |
| `prepareClearAlarm` | Explicit fault action preserving original evidence | Lichuan fault-reset behavior and controlword edge |
| `prepareStop` | Explicit stop behavior and pending-command policy | Occupied SDO/PDO channel and option-code rules |
| `nextRead`, `advanceRead`, `getIdentity`, `getConfig`, `getStateBlock` | Concrete bounded read state and checked typed getters | Service events, not serial byte replies |
| `nextAction`, `advanceAction` | Concrete action context retains terminal evidence | Preserve network/drive/transport distinctions |
| `nextMove`, `advanceMove` | Same finite work/context outcome pattern | Mode/trigger/target-ack/completion evidence |
| `nextVelocity`, `advanceVelocity`, `serviceVelocity` | Same finite lifecycle and explicit deadline servicing | Stop due time and fixed settlement deadline differ; cancellation is local, later stop is a separate action after expiry |
| `nextHome`, `advanceHome`, `getHomeReference` | Same finite lifecycle and checked reference publication | Completion dialect and reference invalidation |

Terminal operation results are inspected through concrete context members such as `state`, `outcome`, `status`, `execution`, `completion` and `uncertain`; the RS installed core has no `getMoveResult`/`getActionResult` family. Typed settings use getters such as `getDriver` and `getIo`. The example owner exposes `result(RequestId)`/`release(RequestId)`; example `moveProgress`/`releaseMove` helpers are not installed library APIs. Audit exact members/signatures before reuse. Do not add a generic `OperationContext` or `advanceOperation` just to conceal distinct bounds. Common request data and typed family operations remain separate; vendor-only settings need descriptive native APIs.

Before copying `MoveRequest`, `MoveRamp`, `PreparedVelocityTarget` or `ReadTarget`, inspect their RS-specific fields and arithmetic, including the int16-RPM restriction inside `Axis.cpp`. Preserve intent with generic signed rate quantities, profile-specific native representation/quantization and logical axis identity. `steps` means configured command increments when retained for vocabulary parity; full motor steps are an optional stepper-specific scale, and encoder feedback is independently interpreted. Servo operation cannot require subdivision or a full-step count. Source copying must retain MIT attribution and targeted regression tests. Do not modify MotorControl-RS or create a shared dependency package during this assignment merely to remove duplicate files; document concrete reuse and coordinate a later extraction if warranted.

## CLI parity

Current RS console protocol is **3**, with plain human output and `@ID` correlated JSONL. Reuse its correlation/framing model, bounded output and naming; include explicit library/profile identity and a documented CAN schema version so a Python client cannot mistake one protocol for the other. RS protocol number alone does not prove wire/schema compatibility.

Two inventory states apply to every route. **Implemented**: in help and dispatch, backed by a tested public path. **Gated**: listed by `caps` with an exact reason, absent from dispatch and working help, and not counted as implemented coverage. The owner column names the prompt that delivers the route; `P40` to `P42` below mean prompts 40 to 42. Prompt 40 ships a compiled-profile inventory for `profile list` and `caps` without prompt 37's `getDiscovery*`; prompt 41 adds discovery-backed fields.

| Command group | Owner | Preserve/adapt |
| --- | --- | --- |
| `help`, `version`, `profile list`, `caps` | P40 (compiled inventory); P41 adds discovery fields | One command inventory; report compiled profile/protocol, motor/feedback config, supported modes and qualification separately; research fixtures are excluded |
| `config`, `wiring` | P40 | Host-only interface/node/bitrate selection; no silent device writes |
| `host` | P41 | Host-only target selection; no silent device writes |
| `useaddr` | P41 | Retain selection grammar where suitable; validate the selected protocol's endpoint/range, not a universal CAN node-ID limit; multi-axis bindings identify subaxis separately |
| `status`, `health`, `stats`, `drv`, `memory`, `result` | P40 | Cached diagnostics and retained results, separate network/drive fields |
| `stats reset` | P40 | Local counters only; preserve uncertainty, fault and evidence |
| `probe`, `read identity\|config\|state`, `discover` | P41 | Explicit typed owner-scheduled uploads with no write intent, effect recorded as unknown; bounded scan; objects outside the accepted read list return the Q20 gate error |
| `settings`, `subdivision [INTEGER\|options]` | P41 | Common settings inspect selected typed groups; subdivision is a Lichuan/native capability and cannot be forced onto servo profiles; host `stepsperturn` stays separate |
| `axis config`, `axis origin`, `prepare` | P40 | Public pure API; exact values, no writes |
| `moveby`, `moveto`, `move relative\|absolute\|angle` | P42 | Same public finite motion API; no assumed RS scale or speed/ramp limits. RS `moveby VALUE [unit]` has no speed or ramp argument; an unset speed, ramp or scale rejects, and `moveto` needs an explicit `axis origin` |
| `speed`, `accel`, `decel`, `motion`, `stepsperturn` | P42 | Explicit host intent where meaningful; display actual units, native ramp-time distinction and verified scale; this is the simple-move intent that replaces RS boot defaults, which CAN forbids |
| `velocity`, `home methods`, `home ...` | P42 | Finite velocity and reviewed homing; velocity `native` means the configured [native-rate dimension](axis_contract.md#signed-velocity-and-the-native-rate-destination), so `rpm native` needs `MOTOR_TURNS` and `MINUTE`; `effectiveNative` reaches an rpm object only with that recorded destination |
| `enable`, `alarm-clear`, `position-clear`, `stop normal\|fast` | P42 | Common explicit actions where supported; define quick-stop versus halt without implying equivalence; stop follows the DEC-2/DEC-8 contract in the RS rows below |
| `motor-release` | P42 (visibly gated) | Gated: no manual documents a bus release command, so it is listed by `caps` with that reason and is not dispatched (D-17) |
| `driver`, `io`, `control`, `tuning` | P41 | Typed family settings; document any native group differences |
| `motion-profile read\|inspect` | P41 | Retained profile snapshot; read effects explicit |
| `motion-profile restore\|forget` | P41 route and application code | Explicit restore effects and generation validity; no automatic restoration after uncertainty. This is `re-apply recorded configuration` in `examples/common` over the typed settings API, as RS does it in its application, with no new core API. Prompt 39 owns `restore factory settings` only |
| `communication`, `persistence` | P41 | Explicit network/device setting workflows with uncertain outcomes; `save parameters` (`2300=2`), `restore factory settings` (`2300=1`) and `re-apply recorded configuration` stay separate |
| `monitor` | P41 | Finite monitoring |
| `cancel`, `release ID`, `recover` | P40 | Local cancellation/result release/host recovery; not motor stop; `cancel` of sequenced work or a typed read lets in-flight frames settle |
| `debug [off\|raw\|decoded]`, `load` | P41 | Debug receives passive traffic copies; load is an optional bounded application workload, never another motor path |
| `capture-read` | P41 | Active bounded diagnostic read through the normal owner/public API using a reviewed CAN object set; capture does not directly drain controller RX |
| CANopen `nmt`, `pdo`, `heartbeat`, `network` groups | P41 | Native operations/state only on matching implemented profiles; other CAN protocols expose their own liveness/state and never fake CANopen support |

### MotorControl-RS additions after the audited revision

The RS audit baseline predates these additions; see the [dated delta](audit/MOTORCONTROL_RS_AUDIT.md#delta-2026-11-02-against-motorcontrol-rs-776facd). Each row is adopt, or not applicable with the reason. Adopt means the intent is mapped to the owner shown; it does not mean implemented, and no row below is a CAN route today.

| RS addition | Disposition | CAN mapping and reason |
| --- | --- | --- |
| `velocity read`, `prepareVelocityParameters` | Adopt | Typed read of the reviewed velocity rows (the ledger owns the exact set; `60FF` is owned by prompt 35). Read API in prompt 35, route in P42 |
| `home read`, `prepareHomingParameters` | Adopt | Typed read of the reviewed homing rows (`607C`, `6098`, `6099:00..02`, `609A`). Read API implemented in prompt 36, route in P42 |
| `home auxiliary` | Not applicable | ESS auxiliary configuration 0..11 is an ESS function number set; ESS function numbers do not transfer and no CL86-C equivalent is documented |
| `home qualification` install and revoke, `checkHomeQualification`, `revokeHome` | Adopt (shape) | Operator states each commissioning fact by name, live checks decide admission, and no all-true switch exists. The facts are Lichuan-specific and owned by prompt 36; revoke invalidates the reference. Route in P42 |
| `velocity qualification install` | Adopt (shape) | Same named-fact install for velocity prerequisites, owned by prompt 35. The facts themselves are not copied from RS |
| `position-clear qualification install` | Not applicable now | Prompt 32 gates `prepareSetDevicePosition` by ledger evidence (Q14), not operator-stated facts. Revisit if bench evidence produces a commissioning fact |
| `motion-profile start-speed`, start-speed relation route | Not applicable | The reviewed Lichuan ledger has no start-speed object; the RS relation maps an ESS register word to a minimum rpm. Do not invent one |
| `position interrupt`, `PositionInterruption` | Not applicable now | RS uses an ESS register bit for interrupt-versus-ignore. No equivalent is documented for CL86-C; mode-1 bit 4 behavior is Q12-gated and DEC-8 holds bit 4 until the move is terminal |
| Stop held across one bounded recovery and sent once; second fault ends it not transmitted; lost reply stays UNKNOWN, never resent | Adopt | Prompt 42 application dispatch; the owner path is W6. CAN sends the held stop as `000B` (re-issued, not the requested policy), which differs from RS on purpose |
| `stop fast` escalates `stop normal` | Adopt, with a difference | CAN escalates upward only. RS has no policy check on supersede, so a later `stop normal` could replace a running `stop fast` |
| `cancel` of sequenced work or a typed read, and `monitor off`, let in-flight frames settle | Adopt | A cancel must not leave a route unresolved by itself; prompt 42 |
| Move reference check (`MoveContext::positionMismatch`; idle, stationary, ACTUAL reference required; native bounds on relative displacement) | Compare in prompt 33 | Not a CLI row; recorded so prompt 33 decides adopt or reject |
| Bench runner helpers `cleanup_stop_attempts`, `write_rejected`, `home_inputs_permit` | Compare in prompts 43 and 44 | Runner-side; not a console route |
| ESS target magnitude limit and exception 05 mapping | Not applicable | Serial ESS facts with no CAN counterpart |

Do not copy removed RS aliases (`ping`, bare `reset`, `health check`, `profile ess_rs`, `?`, `ver`) from old documentation; RS tests also reject `help advanced`, `profile ess_rs enable` and `stop direct`. `motor ...` is never-implemented grammar, not a removed alias; do not invent it in parallel with the actual established command family. `release ID` releases result storage; `motor-release` changes drive state.

Gated commands/capabilities must be inspectable through `caps` and explain why they are unavailable; they are absent from dispatch and working help, so no unimplemented command is presented as callable. Native features such as internal segments are added only if documented for the target; an ESS segment command is not automatically a CAN feature.

## Health and result separation

Retain independent facts for application administrative enable, transport/controller state, native drive/mode/liveness, fault latch, observation freshness, admission, wire outcome, device acceptance, execution and motion inhibition. NMT/CiA402/heartbeat/statusword/SDO details belong to an optional CANopen observation, not mandatory fields for every protocol. Explicitly document which event updates each field and which transitions invalidate axis/configuration evidence. A recent heartbeat may support node liveness while position evidence remains stale; a successful SDO may coexist with a drive fault. A protocol without a write acknowledgement or completion signal cannot synthesize those facts.

Select example-only `DriverState { UNINIT, READY, DEGRADED, OFFLINE }`, per-binding `Health`, owner `Stats`, cached `health(binding)`/`driverState(binding)`/`stats()` and `clearStats()` as defined in CONVENTIONS. Health owns saturating logical-service totals/failure streak and timestamped evidence; Stats owns frame/controller/queue counters. The projection is passive and has no sensor-style offline admission latch. Resetting statistics preserves live failure state, uncertainty, faults and retained results. Keep errors/history bounded and useful to FieldCore-like applications without importing FieldCore enums.
