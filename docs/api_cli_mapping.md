# API and CLI naming map

This mapping preserves current MotorControl-RS names where their meanings apply. Exact signatures must be audited against live headers in each implementation stage. The API names below are proposed for MotorControlCAN, not implemented promises.

Prompt 6 now demonstrates `prepareIdentity/nextRead/advanceRead/getIdentity`,
`prepareMoveAbsolute/nextMove/advanceMove` and the owner admission/result lifecycle
in an [experimental synthetic profile](profile_contract.md). That page maps
proof declarations to route intent. Prompt 19 now installs the finite
[Lichuan raw read lifecycle](reads.md), including checked identity/state/config
getters and SDO/PDO evidence. No CLI route exists yet. The non-consuming probe
remains Q20 gated; the inventory below retains later operation/CLI owners.

[Prompt 18](reports/prompt_18.md) is blocked on the Lichuan PDO mutation contract
(Q17). No typed remap/timing API or corresponding CLI route is implemented.
`configurePdo` installs host map evidence only; raw SDO access and EDS metadata
do not substitute for those missing operations.

The selected detailed vocabulary is centralized in [CONVENTIONS.md](prompts/CONVENTIONS.md), backed by the [fresh source audit](audit/CONVENTIONS_REAUDIT.md). Use that contract for exact status, context, owner and health names; this file maps them to user-facing intent and CLI routes.

The [multi-drive decision](multidrive_decision.md) makes these common motion names apply to both stepper and servo drives. CANopen and Lichuan are the first concrete backend/profile, not mandatory public axis semantics. Future families have their own namespaces and typed contexts; test-only N5/EPOS4/proprietary contrasts must not appear as installed drivers.

## Style and status

Prompt 20 implements `prepareDriverRead/nextDriver/advanceDriver/getDriver` for
the [three native read groups](driver_settings.md). Settings write progression
and its future typed CLI route remain unimplemented under their evidence gates.

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
| `prepareProbe` | Smallest verified non-changing presence request | Reviewed SDO/passive evidence; absent heartbeat may be inconclusive |
| `prepareIdentity`, `prepareState`, `prepareConfig` | Typed bounded observations | Logical axis/binding generation is common; native node/channel/subaxis route is protocol-specific |
| `prepareMoveAbsolute`, `prepareMoveRelative`, `prepareMoveAngle` | Same intent, coordinate, quantization and failure semantics | Device handshake and actual scaling are Lichuan-specific |
| `prepareVelocity` | Finite signed velocity with explicit duration/stop policy | Wire units, signedness and start sequence require profile evidence |
| `prepareHome` | Explicit documented method and prerequisites | Do not transfer ESS home method numbers |
| `prepareSetDevicePosition` | Explicit supported device-coordinate change only | Reject if no documented equivalent; origin remains available |
| `prepareEnable`, `prepareRelease` | Explicit drive state change | Confirm state transition; NMT operational alone is insufficient |
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

| Command group | Preserve/adapt |
| --- | --- |
| `help`, `version`, `profile list`, `caps` | One command inventory; report compiled profile/protocol, motor/feedback config, supported modes and qualification separately; research fixtures are excluded |
| `config`, `host`, `wiring` | Host-only interface/node/bitrate selection; no silent device writes |
| `useaddr` | Retain selection grammar where suitable; validate the selected protocol's endpoint/range, not a universal CAN node-ID limit; multi-axis bindings identify subaxis separately |
| `status`, `health`, `stats`, `drv`, `memory`, `result` | Cached diagnostics and retained results, separate network/drive fields |
| `stats reset` | Local counters only; preserve uncertainty, fault and evidence |
| `probe`, `read identity\|config\|state`, `discover` | Explicit non-changing owner-scheduled reads; bounded scan |
| `settings`, `subdivision [INTEGER\|options]` | Common settings inspect selected typed groups; subdivision is a Lichuan/native capability and cannot be forced onto servo profiles; host `stepsperturn` stays separate |
| `axis config`, `axis origin`, `prepare` | Public pure API; exact values, no writes |
| `moveby`, `moveto`, `move relative\|absolute\|angle` | Same public finite motion API; no assumed RS scale or speed/ramp limits |
| `speed`, `accel`, `decel`, `motion`, `stepsperturn` | Explicit host intent where meaningful; display actual units, native ramp-time distinction and verified scale |
| `velocity`, `home methods`, `home ...` | Finite velocity and reviewed homing |
| `enable`, `motor-release`, `alarm-clear`, `position-clear`, `stop normal\|fast` | Common explicit actions where supported; define quick-stop versus halt without implying equivalence |
| `driver`, `io`, `control`, `tuning` | Typed family settings; document any native group differences |
| `motion-profile read\|inspect\|restore\|forget` | Retained profile snapshot with explicit read/restore effects and generation validity; no automatic restoration after uncertainty |
| `communication`, `persistence` | Explicit network/device setting workflows with uncertain outcomes |
| `monitor`, `cancel`, `release ID`, `recover` | Finite monitoring, local cancellation/result release/host recovery; not motor stop |
| `debug off\|raw\|decoded`, `load` | Debug receives passive traffic copies; load is an optional bounded application workload, never another motor path |
| `capture-read` | Active bounded diagnostic read through the normal owner/public API using a reviewed CAN object set; capture does not directly drain controller RX |
| CANopen `nmt`, `pdo`, `heartbeat`, `network` groups | Native operations/state only on matching implemented profiles; other CAN protocols expose their own liveness/state and never fake CANopen support |

Do not copy removed RS aliases (`ping`, bare `reset`, `health check`, `profile ess_rs`, `?`, `ver`) from old documentation. Do not invent a `motor ...` prefix in parallel with the actual established command family. `release ID` releases result storage; `motor-release` changes drive state.

Unsupported commands/capabilities must be inspectable and explain why they are unavailable. Avoid unimplemented commands presented as callable. Native features such as internal segments are added only if documented for the target; an ESS segment command is not automatically a CAN feature.

## Health and result separation

Retain independent facts for application administrative enable, transport/controller state, native drive/mode/liveness, fault latch, observation freshness, admission, wire outcome, device acceptance, execution and motion inhibition. NMT/CiA402/heartbeat/statusword/SDO details belong to an optional CANopen observation, not mandatory fields for every protocol. Explicitly document which event updates each field and which transitions invalidate axis/configuration evidence. A recent heartbeat may support node liveness while position evidence remains stale; a successful SDO may coexist with a drive fault. A protocol without a write acknowledgement or completion signal cannot synthesize those facts.

Select example-only `DriverState { UNINIT, READY, DEGRADED, OFFLINE }`, per-binding `Health`, owner `Stats`, cached `health(binding)`/`driverState(binding)`/`stats()` and `clearStats()` as defined in CONVENTIONS. Health owns saturating logical-service totals/failure streak and timestamped evidence; Stats owns frame/controller/queue counters. The projection is passive and has no sensor-style offline admission latch. Resetting statistics preserves live failure state, uncertainty, faults and retained results. Keep errors/history bounded and useful to FieldCore-like applications without importing FieldCore enums.
