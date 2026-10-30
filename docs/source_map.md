# Live source and responsibility map

Audited 2026-10-28 for [prompt 1](reports/prompt_1.md). This is navigation to
current declarations, implementation, real callers and focused tests, not a
directory dump. The original sibling audit remains below; production additions
and their actual callers/tests are recorded in the migration tables. Exact revisions and clean-tree
observations are in [the input record](reports/prompt_1_inputs.json).

Source paths below are relative to the sibling `Projects/` directory. They are
written as code so a standalone clone does not require sibling checkouts merely
to validate its documentation. The audit checks their existence locally.
Target paths are relative to MotorControl-CAN and are **planned**, unless the
row links an existing implementation or documentation file. The numbered prompt owns
implementation and tests; no new public header is created by this audit.

Prompt 6 refreshed the relevant live seams in the
[source review](reports/prompt_6_source_review.md), with no sibling revision or
dirty-state delta. Its concrete proof navigation is separate from planned paths:

| Responsibility | Actual declaration/implementation | Actual caller/test | Migration |
| --- | --- | --- | --- |
| Installed status/identity/frame | [Public headers](../include/MotorControlCAN/MotorControlCAN.h), [Endpoint.cpp](../src/Endpoint.cpp), [Frame.cpp](../src/can/Frame.cpp) | [core_test.cpp](../test/core_test.cpp), [installed consumer](../test/installed_consumer/main.cpp) | 7/9/11 complete the current pure status/frame boundary |
| Prototype units and typed profile contexts | [Contracts.h](../experiments/api_ownership/Contracts.h), [Operations.h](../experiments/api_ownership/Operations.h), [Operations.cpp](../experiments/api_ownership/Operations.cpp) | [api_ownership_test.cpp](../test/api_ownership_test.cpp), [multidrive_contract_test.cpp](../test/multidrive_contract_test.cpp) | Synthetic profile stays test-only; 9/10 replaced prototype math with production Units/Axis; 19/33 implement real operations |
| Physical owner/table | [CanBusOwner.h](../experiments/api_ownership/CanBusOwner.h), [CanBusOwner.cpp](../experiments/api_ownership/CanBusOwner.cpp) | Native main above | 13/15/23 extend the same table/client boundary; 27 qualifies controller |
| Build/package | [Root CMake](../CMakeLists.txt), [manifest](../library.json), [version helper](../scripts/generate_version.py) | [Registered tests](../test/CMakeLists.txt), [package contract](../test/package_test.py), [version contract](../test/version_test.py) | 7 complete; [verifier](../scripts/verify.py), [checker](../scripts/check_repository.py), [workflow](../.github/workflows/verify.yml) and [Doxyfile](../Doxyfile) implement prompt 8 gates |

## Typed raw reads (prompt 19)

Prompt 20 adds [DriverSettings.h](../include/MotorControlCAN/profiles/lichuan_c/DriverSettings.h)
and three fixed plans in the same [read implementation](../src/profiles/lichuan_c/Reads.cpp).
[Driver tests](../test/driver_settings_test.cpp), the
[offline caller](../examples/driver_settings_preview/main.cpp) and isolated
installed consumer use the actual public wrappers. Matching, deadlines, epochs
and failed-output rules are shared with prompt 19; no second service engine is
introduced. The [group guide](driver_settings.md) owns units and write gates.

Prompt 21 adds [IoSettings.h](../include/MotorControlCAN/profiles/lichuan_c/IoSettings.h)
and four fixed I/O plans in the same read implementation. The
[23-row suite](../test/io_settings_test.cpp), [offline caller](../examples/io_settings_preview/main.cpp)
and isolated consumer exercise `getIo` through the existing driver lifecycle.
RS `DriverSettings.cpp` and its `io_test.cpp` are the naming/caller evidence;
[I/O guidance](io_settings.md) separates exact raw words, documentary DI labels
and still-gated writes/actions. No new receiver or service engine appears.

| Responsibility | Actual declaration/implementation | Actual caller/test | Boundary |
| --- | --- | --- | --- |
| Typed lifecycle and getters | [Common read phases](../include/MotorControlCAN/ReadOperation.h), [profile API](../include/MotorControlCAN/profiles/lichuan_c/Reads.h), [implementation](../src/profiles/lichuan_c/Reads.cpp) | [Independent fake-owner tests](../test/reads_test.cpp), [compiled preview](../examples/reads_preview/main.cpp), isolated installed consumer | 30 raw rows; unknown model/status/scale/effects preserved. Non-consuming probe remains Q20 gated. |
| Shared PDO evidence | [Installed PdoState.h](../include/MotorControlCAN/canopen/PdoState.h), [route comparison/decoder](../src/canopen/Pdo.cpp) | Existing owner routing and read projection tests | Same bounded owner record forwarded by the adapter; no second node/map table or installed stack dependency. |

## RS naming, motion and application ownership

| Responsibility | Current declaration | Implementation | Real caller | Focused test | Planned target / owner / deliberate CAN difference |
| --- | --- | --- | --- | --- | --- |
| Call status | `MotorControl-RS/include/MotorControlRS/Status.h` | Same header: `Status`, free `Ok()`, `errToString()`, `isOk()` and explicit bool | `MotorControl-RS/examples/units_preview/main.cpp` | `MotorControl-RS/test/units_test.cpp` | `include/MotorControlCAN/Status.h`; 9. Keep names, static message lifetime and signed detail; omit RTU CRC/exception, retain raw unsigned SDO abort separately |
| Exact units and axis/reference/configuration | `MotorControl-RS/include/MotorControlRS/Units.h`; `MotorControl-RS/include/MotorControlRS/Axis.h` | `MotorControl-RS/src/Units.cpp`; `MotorControl-RS/src/Axis.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp`; units preview above | `MotorControl-RS/test/axis_test.cpp`; units test above | Common units/axis headers and source; 9-10. Logical identity, exact rational arithmetic and generation checks; no mandatory step geometry or native RPM width |
| Typed reads | `MotorControl-RS/include/MotorControlRS/ReadOperation.h`; `MotorControl-RS/include/MotorControlRS/profiles/ess_rs/Reads.h` | `MotorControl-RS/src/profiles/ess_rs/Reads.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` | `MotorControl-RS/test/reads_test.cpp` | `profiles/lichuan_c/Reads.*`; 19. `prepareIdentity/Config/State`, `nextRead/advanceRead`, context `state/outcome/status` and checked `getIdentity/Config/StateBlock`; replace register/serial facts with reviewed objects/services |
| Actions and independent execution/completion evidence | `MotorControl-RS/include/MotorControlRS/ActionOperation.h`; `MotorControl-RS/include/MotorControlRS/profiles/ess_rs/Actions.h` | `MotorControl-RS/src/profiles/ess_rs/Actions.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` | `MotorControl-RS/test/actions_test.cpp` | Concrete action contexts; 30-32. Keep `prepareEnable/Release/ClearAlarm/Stop`, `nextAction/advanceAction` and context `state/outcome/execution/completion/status`; require Lichuan transition evidence |
| Position/angle | `MotorControl-RS/include/MotorControlRS/profiles/ess_rs/Position.h` | `MotorControl-RS/src/profiles/ess_rs/Position.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` | `MotorControl-RS/test/position_test.cpp`; `MotorControl-RS/test/position_command_test.cpp`; `MotorControl-RS/test/move_app_test.cpp` | Profile position and common angle integration; 33-34. `prepareMoveAbsolute/Relative/Angle`, `nextMove/advanceMove`; explicit new completion evidence, no copied ESS masks/widths |
| Finite velocity | `MotorControl-RS/include/MotorControlRS/profiles/ess_rs/Velocity.h` | `MotorControl-RS/src/profiles/ess_rs/Velocity.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` | `MotorControl-RS/test/velocity_test.cpp` | Profile velocity; 35. `prepareVelocity/nextVelocity/advanceVelocity/serviceVelocity` where needed; duration starts at successful preparation, retains stop obligation and primary failure |
| Homing and checked reference | `MotorControl-RS/include/MotorControlRS/profiles/ess_rs/Homing.h` | `MotorControl-RS/src/profiles/ess_rs/Homing.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` | `MotorControl-RS/test/homing_test.cpp` | Profile homing; 36. `prepareHome/nextHome/advanceHome/getHomeReference`; methods/zero/inputs remain model-specific |
| Typed configuration groups | `MotorControl-RS/include/MotorControlRS/profiles/ess_rs/DriverSettings.h` | `MotorControl-RS/src/profiles/ess_rs/DriverSettings.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` | `MotorControl-RS/test/driver_settings_test.cpp` | Reviewed driver/I/O/tuning groups; 20-22. Whole candidate validation, real native units and partial effects; no integer-writer substitute |
| Physical owner and retained results | `MotorControl-RS/examples/common/RtuBusOwner.h` | `MotorControl-RS/examples/common/RtuBusOwner.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp`; `MotorControl-RS/examples/probe_cli/MotionProfileApp.h` | `MotorControl-RS/test/bus_owner_test.cpp`; `MotorControl-RS/test/bus_scheduling_test.cpp` | `examples/common/CanBusOwner.*`; 23-25. `admit/admitUrgent/service/result/release/cancel`, explicit recovery, copied admission and reserved terminal slot; one continuous CAN receiver, no UART gap/DE/echo |
| Transport timing and TX/RX evidence | `MotorControl-RS/examples/common/RtuRunner.h` | `MotorControl-RS/examples/common/RtuRunner.cpp` | `MotorControl-RS/examples/common/RtuBusOwner.cpp` calls `Runner::poll` from owner service; `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` services the owner | `MotorControl-RS/test/runner_test.cpp` physical TX and receive-interval uncertainty cases | Example adapter/owner boundary; 23-24/27. Preserve observed physical TX versus queue acceptance and bounded receive evidence; UART character gaps, DE control and echo do not transfer to CAN. CAN controller enqueue, transmission, ACK and service confirmation need separate evidence |
| Passive state versus health | `MotorControl-RS/examples/probe_cli/StateCache.h` | Same header; `MotorControl-RS/examples/probe_cli/ProbeConsole.cpp` formats cached evidence | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` | `MotorControl-RS/test/probe_app_monitor_control_test.cpp`; `MotorControl-RS/test/probe_console_test.cpp` | Example-owned `Health`, `Stats`, `DriverState`; 25. These are selected CAN additions, not installed RS types; separate heartbeat/NMT/CiA402/fault/outcome |
| One CLI inventory and protocol correlation | `MotorControl-RS/examples/probe_cli/ProbeConsole.h`; `MotorControl-RS/docs/cli_contract.md` | `MotorControl-RS/examples/probe_cli/ProbeConsole.cpp` | `MotorControl-RS/examples/probe_cli/ProbeApp.cpp`; shared Arduino/IDF entry points | `MotorControl-RS/test/human_console_test.cpp`; `MotorControl-RS/test/probe_console_test.cpp` | `examples/common/ProbeConsole.*`, own CAN schema; 40-42. Canonical inventory, `@ID`, asynchronous command/operation correlation and cached/refresh distinction |
| Python sessions and retained evidence | `MotorControl-RS/scripts/bench_probe.py` (`Console`, `Evidence`); `MotorControl-RS/scripts/bench_session.py` | Same files; `MotorControl-RS/scripts/_bench_evidence.py` | `MotorControl-RS/scripts/bench_scenarios.py`; `MotorControl-RS/scripts/bench_motion.py`; `MotorControl-RS/scripts/bench_repeat.py` | `MotorControl-RS/test/bench_probe_test.py`; `MotorControl-RS/test/bench_scenarios_test.py`; `MotorControl-RS/test/motion_campaign_test.py` | `scripts/` and Python tests; 43-44. Explicit CAN identity and native evidence; no hidden retry or implicit serial connection |
| C++11 package and actual verification registration | `MotorControl-RS/CMakeLists.txt`; `MotorControl-RS/library.json`; `MotorControl-RS/platformio.ini` | `MotorControl-RS/scripts/verify.py`; `MotorControl-RS/.github/workflows/verify.yml` | `MotorControl-RS/test/installed_consumer/CMakeLists.txt` | Source/install consumer and codec-symbol gates in verifier | Root CMake/manifests, `test/`, `scripts/`; 7-8. Portable core only; SDK, owner, console and vendor evidence excluded from install; strict Doxygen added from sensor precedent |

The live installed RS profile is ESS-RS only (`MotorControl-RS/src/Discovery.cpp`).
`MotorControl-RS/docs/profile_contract.md` contains the actual Leadshine iEM-RS
design contrast. LC57BK-R/TL57-R references and prompts are research, not installed
profiles. CAN's N5 stepper/BLDC and EPOS4 contrasts likewise do not establish
support. Common motion must stay independent of manufacturer, motor kind and
CANopen state/endpoint representation.

[Prompt 4's contrast ledger](reference/drive_contrasts.md) now records N5
stepper/BLDC, EPOS4 servo and non-CANopen test-only cases for the planned common
units, endpoint, frame and owner paths above. It does not add a source module,
installed profile or qualified device.

The inspected RS owner lives in `examples/common/`, while its ProbeApp,
ProbeConsole and StateCache live in `examples/probe_cli/`. The selected CAN
plan places reusable application/console code in `examples/common/` with thin
framework entry points. That target placement is a deliberate decision, not a
claim that the current RS files already occupy it.

Terminal state is read from the concrete context, not a generic result getter.
`ProbeApp.cpp` checks read/action context state and action execution/completion;
the fields are declared in `Reads.h` and `Actions.h`. RS `getIdentity` and
`getConfig` publish only after whole-operation success, while `getStateBlock`
can publish a successful block even if a later block fails. CAN's checked
getters must specify their own equivalent publication boundary.

## I2C and stateless-codec counterexamples

Each row records a distinct behavior; do not combine these into a fictional
universal sensor lifecycle. Sensor callback, retry and hardware-specific cleanup
semantics are not CAN defaults.

| Seam | Declaration | Implementation | Real caller | Focused test | CAN use / owner |
| --- | --- | --- | --- | --- | --- |
| ADS1115 passive binding and verified profile generation | `ADS1115/include/ADS1115/ADS1115.h`; `ADS1115/include/ADS1115/Status.h` | `ADS1115/src/ADS1115.cpp` | `ADS1115/examples/02_owner_safe_poll/main.cpp`; `FieldCore-node/src/devices/i2c/Ads1115Module.cpp` | `ADS1115/test/test_basic.cpp` owner-safe readback/generation and atomic sample tests | 19-25: verified configuration and retained result; RS free status helpers remain authoritative; no sensor replay |
| INA228 hardware synchronization versus health | `INA228/include/INA228/INA228.h` | `INA228/src/INA228.cpp` | `FieldCore-node/src/devices/i2c/Ina228Module.cpp` | `INA228/test/test_basic.cpp` passive health/hardware validity tests | 19/25: validity independent from communication health; `pollJob/takeJobResult` is not the core motion API |
| SHT3x logical versus physical transport counts | `SHT3x-main/include/SHT3x/SHT3x.h`; `SHT3x-main/include/SHT3x/Config.h` | `SHT3x-main/src/SHT3x.cpp` | `SHT3x-main/examples/common/Sht3xCli.cpp`; `FieldCore-node/src/devices/i2c/Sht3xModule.cpp` | `SHT3x-main/test/test_basic.cpp` CRC/logical outcome/provenance tests | 25: count logical terminal services separately from physical traffic. Poll/cancel returns terminal exactly once; no `takeJobResult` |
| BME280 desired/applied state and sample retention | `BME280/include/BME280/BME280.h` | `BME280/src/BME280.cpp` | `FieldCore-node/src/devices/i2c/Bme280Module.cpp` | `BME280/test/test_basic.cpp` staged apply and uncertain-write tests | 20-22: partial writes keep desired settings with resync required and last good sample; native CAN effects need their own contract |
| MCP45HVX1 explicit output mutation | `MCP45HVX1/include/MCP45HVX1/MCP45HVX1.h` | `MCP45HVX1/src/MCP45HVX1.cpp` | `MCP45HVX1/examples/01_basic_bringup_cli/main.cpp` synchronous path; chunked caller in native test | `MCP45HVX1/test/test_basic.cpp` chunked output/uncertainty tests | 21/30-32: start admission can return `Ok()`; begin can write and clears state before validation, so do not copy as passive bind |
| MB85RC retained borrowed buffers/results | `MB85RC/include/MB85RC/MB85RC.h` | `MB85RC/src/MB85RC.cpp` | `FieldCore-node/src/devices/i2c/Mb85rcModule.cpp` | `MB85RC/test/test_basic.cpp` cancellation, pending result and reconciliation tests | 13-14/23: CAN admitted work owns data; MB intentionally borrows staged buffers. Reconciliation is not write replay |
| PCA9555 effect-aware cancellation cleanup | `PCA9555/include/PCA9555/Config.h`; `PCA9555/include/PCA9555/PCA9555.h` | `PCA9555/src/PCA9555.cpp` | `PCA9555/examples/01_basic_bringup_cli/main.cpp` | `PCA9555/test/test_basic.cpp` pointer-park and retained primary-failure tests | 24/25: cancellation can leave cleanup obligations; successful cleanup does not erase primary failure. No copied I2C pointer work |
| RV3032 persistence ambiguity | `RV3032-C7/include/RV3032/RV3032.h` | `RV3032-C7/src/RV3032.cpp` | `FieldCore-node/src/devices/i2c/Rv3032Module.cpp` | `RV3032-C7/test/test_native/test_datetime.cpp` native persistent-effect tests | 39: retain partial/unknown effects; RTC-specific cleanup/startup exceptions do not permit CAN save/reset |
| SHZK checked stateless parser | `SHZK-PT/include/SHZK_PT/SHZK_PT.h` | `SHZK-PT/src/SHZK_PT.cpp` | `SHZK-PT/examples/01_basic_bringup_cli/main.cpp` | `SHZK-PT/test/test_parsers.cpp` wrong-shape and failure-output tests | 11: borrowed call-only inputs; its checked single-register parser resets the value to zero on failure, and its multi-register parser resets `outCount` while preserving the value array. CAN parsers need their own documented atomic-output contract; no claim to solve identical late response ambiguity |
| VTN count-reset exception and staged publication | `VTN4xx/include/VTN4XX/VTN4XX.h` | `VTN4xx/src/VTN4XX.cpp` | `VTN4xx/examples/01_basic_bringup_cli/src/main.cpp` | `VTN4xx/test/test_codec.cpp` exact-channel/failed-parse tests | 11: document intentional outCount reset; fixed candidate buffer; do not copy its status factory spelling |
| VibWire exact shape/raw uncertainty | `VibWire-108/include/VibWire/VibWire.h` | `VibWire-108/src/VibWire.cpp` | `VibWire-108/examples/espidf_basic/main/main.cpp` | `VibWire-108/test/test_codec.cpp` exact register-count/unchanged-output tests | 11/19: checked exact response; keep unresolved signedness raw and count-reset exceptions explicit |

## FieldCore consumer boundary

| Responsibility | Declaration | Implementation and caller | Focused evidence | CAN target / difference |
| --- | --- | --- | --- | --- |
| Motor identity, intent and retained outcome | `FieldCore-node/include/TunnelMonitor/contracts/MotorControl.h` | `FieldCore-node/src/devices/rs485_motor/Rs485MotorModule.cpp`; `FieldCore-node/src/rs485/Rs485Task.cpp` | `FieldCore-node/test/native/test_owner_rs485/test_rs485_task_fake.cpp`; `FieldCore-node/test/native/test_owner_rs485/suite_main.cpp` | 5-6/23-25/46: framework-neutral IDs/configuration and effect evidence; no FieldCore types or native RPM/ramp widths in installed core |
| Owner/backend settlement | `FieldCore-node/include/TunnelMonitor/rs485/Rs485OwnerTransaction.h`; `FieldCore-node/include/TunnelMonitor/rs485/Rs485Backend.h` | `FieldCore-node/src/rs485/Rs485Task.cpp`; `FieldCore-node/src/rs485/ArduinoRs485Backend.cpp`; motor module above | Same real native owner suite | 23-24/27/45: distinguish acceptance/TX/protocol/effect. UART drain/gap/DE and dequeue timestamps are not CAN transmission proof |
| Passive health and non-consuming diagnostics | `FieldCore-node/include/TunnelMonitor/contracts/Health.h`; `FieldCore-node/include/TunnelMonitor/i2c/I2cDiagnostics.h` | `FieldCore-node/src/system/SystemHealthProjection.h`; `FieldCore-node/src/i2c/I2cDiagnostics.cpp` | `FieldCore-node/test/host/rodtester/production_runtime.cpp` observes actual admission/publication hooks | 25/40-44/46: copy diagnostics at real owner boundaries without consuming results or reentering runtime |

FieldCore's RS motor component is selected through
`FieldCore-node/config/build_profiles.json` and explicit rodtester/calibration
services; it is not universally activated or a CAN integration. Its motor
module rejects automatic retries, freezes operation identity, and refuses
generation reuse on exhaustion. Reconciliation requires qualified matching
evidence; its 500 ms age policy is RS application policy, not a CAN default.

## Decisions and explicit exceptions

- Keep [CONVENTIONS](prompts/CONVENTIONS.md) as the single selected CAN naming
  contract. `Status` reports call validity; `ReadOutcome`/`ActionOutcome`,
  `ActionExecution` and `ActionCompletion` report separate operation facts.
  `advanceRead` may validly return `Ok()` after consuming a failure event.
- Current RS command inventory agrees with protocol 3. Use wire `probe`,
  `stats reset`, `read identity`, `profile list` and `stop fast`. RS root
  guidance still mentions old `reset`/`ping` aliases; actual inventory/tests
  take precedence. Python internal `read-identity`/`profile-list` are dispatch
  names translated to spaced wire commands, not additional wire aliases.
- RS read freshness uses a strict age inequality; cache/axis checks have
  different inclusive boundaries. Specify and test each CAN boundary; do not
  invent a universal sibling rule. RS reference invalidation clears confidence
  even on generation exhaustion, an explicit exception to unchanged output.
- ADS has diagnostic OFFLINE after five passive failures. INA defaults PASSIVE;
  SHT defaults LATCH_OFFLINE while FieldCore selects OBSERVE_ONLY; PCA only has
  UNINIT/READY/DEGRADED. CAN's selected passive `DriverState` and saturating
  Health/Stats are deliberate application contracts. Some MB/RTC totals wrap.
- ADS/INA configuration-generation counters skip zero when wrapping; this is not never-reused safety
  identity. Preserve the stricter CAN exhaustion policy. Do not copy automatic
  sensor retries, profile reapplication or lifecycle writes into motor recovery.
- CAN-specific additions are reviewed object access, SDO abort/ambiguity,
  asynchronous heartbeat/PDO/EMCY dispatch and binding/map provenance. A host
  generation cannot identify delayed same-ID/same-DLC remote PDO bytes.

Original applicability and feature gates are in [the execution report](reports/prompt_1.md),
[backlog](backlog.md) and [bench facts](hardware_bench.md). The
[reviewed dictionary](reference/lichuan_objects.md) and
[decision guide](reference/lichuan_review.md) now own source policy, per-model
scope and feature gates. [Prompt 3's live sibling audit](reports/prompt_3_source_review.md)
refreshes native access/effect/configuration provenance and their real callers.
This map supplies naming/ownership evidence, not firmware qualification.

## Prompt 5 service boundary

The [refreshed source/controller audit](reports/prompt_5_source_review.md) checks
RS owner/operation storage, ADS callback budgets, INA explicit owner expiry and
the FieldCore settlement/effect contract against current implementations,
callers and tests. These are unchanged clean source revisions, not copied API
assumptions. The [stack decision](decisions/canopen_stack.md) is now authoritative
for the service choice; the proposed production tree is still owned by prompt 6.

| Responsibility | Current implementation and focused evidence | Production owner / difference |
| --- | --- | --- |
| Selected SDO engine | [Pinned source inventory](../experiments/canopen_service_fit/UPSTREAM.json), actual client/OD/FIFO C files | 13/14: one CANopenNode application adapter; pure core does not include it |
| Owner/result lifetime and fake retirement | [Proof declarations](../experiments/canopen_service_fit/fit_proof.h), [implementation](../experiments/canopen_service_fit/fit_proof.cpp), [caller/tests](../experiments/canopen_service_fit/main.cpp) | 6 freezes contract; 23/24 integrate existing service storage. Local proof types are not installed ABI |
| Controller port | [Application staging port](../adapters/canopennode/Driver.cpp), [target types](../adapters/canopennode/CO_driver_target.h); retained proof delegates here | 13 promotes bounded software staging; 27 must prove actual SDK retirement and acceptance |
| Build/provenance | [Standalone CMake](../experiments/canopen_service_fit/CMakeLists.txt), [commands and limits](../experiments/canopen_service_fit/README.md) | 7 registers proof and core source/install consumers; 8 establishes repository verification/docs CI |

## Production numeric migration (prompt 9)

| Responsibility | Declaration / implementation | Real caller / focused test | Deliberate difference and next owner |
| --- | --- | --- | --- |
| Call status, static labels and unknown values | [Status.h](../include/MotorControlCAN/Status.h), header-only | [Core tests](../test/core_test.cpp), retained proof operations/abort tests | RS shape retained; CAN service rejection separate from raw U32 abort, faults and health |
| Exact parsing, reduction and directed double | [Units.h](../include/MotorControlCAN/Units.h) / [Units.cpp](../src/Units.cpp) | [Isolated consumer](../test/installed_consumer/main.cpp), [numeric vectors](../test/units_test.cpp) | Reusable arithmetic leaves RS Axis; directed conversion corrects both neighboring directions |
| Dimensional conversion and quantization | Same Units API/implementation | [Proof target conversion](../experiments/api_ownership/Operations.cpp), units tests and N5/EPOS4 contrast tests | Ordinary exact rational overloads; explicit radians/float error budgets, no full-step prerequisite or native RPM width |
| Axis configuration, independent origins and reference policy | [Axis.h](../include/MotorControlCAN/Axis.h) / [Axis.cpp](../src/Axis.cpp) | [Offline preview](../examples/units_preview/main.cpp), [installed consumer](../test/installed_consumer/main.cpp), [axis vectors](../test/axis_test.cpp) and retained proof | Prompt 10 complete: I64/mixed exact endpoints, explicit binary32, required relative basis, independent epochs and preserved observation age. CAN velocity stays wide and permits arithmetic zero; RS narrows to int16 RPM. Finite operations remain 33-36 |

## Production CAN/SDO boundary (prompt 11)

| Responsibility | Declaration / implementation | Real caller / focused test | Deliberate difference and next owner |
| --- | --- | --- | --- |
| Classical identifier, DLC, data/RTR and FD flag validity | [Frame.h](../include/MotorControlCAN/can/Frame.h) / [Frame.cpp](../src/can/Frame.cpp) | [Frame vectors](../test/frame_test.cpp), [installed consumer](../test/installed_consumer/main.cpp) | Extended/RTR remain valid common frames; FD is an unsupported capability, BRS/ESI invalid for Classical; timestamps/controller state are external |
| Explicit SDO channel/binding, request and reply envelopes | [Sdo.h](../include/MotorControlCAN/canopen/Sdo.h) / [Sdo.cpp](../src/canopen/Sdo.cpp) | [Installed consumer](../test/installed_consumer/main.cpp), [independent literal vectors](../test/sdo_codec_test.cpp) | Reviewed width and two explicit COB-IDs, no derived default; no RTU CRC, stream or device permission |
| Authoritative request encoder / checked adapter boundary | Pinned [CO_SDOclient.c](../experiments/canopen_service_fit/vendor/CANopenNode/301/CO_SDOclient.c), installed pure checks above | [Real engine vectors](../test/sdo_adapter_test.cpp) | One optional application-side CANopenNode engine; the production lifecycle below now integrates this boundary; reconciliation remains 14 |
| Little-endian width/sign and protocol evidence | Same Sdo API / implementation | Word/alias/sentinel cases in codec tests; real engine abort tests | Full U32 abort distinct from local Status; valid segmented initiation is evidence, not malformed or completed. Borrow spans only during call, publish owned output atomically |
| Package/header/documentation boundary | [CMake](../CMakeLists.txt), [manifest](../library.json), [service guide](canopen_services.md) | [Isolated package checks](../test/package_test.py), independent header compilation | Installed core needs no stack, SDK, Python, vendor source or research fixture; no driver/profile added |

## Reviewed object generation (prompt 12)

| Responsibility | Declaration / implementation | Caller / focused evidence | Boundary |
| --- | --- | --- | --- |
| Single source and reviewed declaration disposition | [Ledger](reference/lichuan_objects.json), [validator](../scripts/check_lichuan_ledger.py), [guide](reference/lichuan_objects.md) | Full original-source validation before generation; existing ledger and new synthetic generator tests | All 99 indexes / 202 typed entries / 53 manual items retained; semantic coverage stays independent from descriptor presence |
| Compact profile-scoped types and policy | Generated [Types.h](../include/MotorControlCAN/profiles/lichuan_c/Types.h), [Objects.h](../include/MotorControlCAN/profiles/lichuan_c/Objects.h) | [Native source vectors](../test/objects_test.cpp), [installed consumer](../test/installed_consumer/main.cpp) | 194 named constants, no lookup/transaction route; four disputed signed types and four reserved entries have no declaration |
| Optional strings and readable accounting | Generated [Catalogue.h](../include/MotorControlCAN/profiles/lichuan_c/Catalogue.h), [coverage](reference/lichuan_generated.md) | Independent headers/package consumer with Catalogue.h removed; source-row review | Source IDs/pages, raw claims/unknowns, family/model/gates/owners retained; no dictionary dependency in numeric headers |
| Deterministic publication and freshness | [Generator](../scripts/generate_lichuan_objects.py), [negative tests](../scripts/generate_lichuan_objects_test.py) | Quick/full verifier objects gate and explicit CTests | Full validation, in-memory candidates, staged files/rollback; exact bytes in nonmutating check; no consumer Python/ledger/SDK dependency |

## Production SDO lifecycle migration (prompt 13)

| Responsibility | Declaration / implementation | Actual caller / focused evidence | Difference / next owner |
| --- | --- | --- | --- |
| Portable owned intent and terminal | [SdoTransfer.h](../include/MotorControlCAN/canopen/SdoTransfer.h) | [Installed consumer](../test/installed_consumer/main.cpp), independent package headers | No stack, SDK, controller I/O or universal protocol acknowledgement |
| Selected engine, fixed channel/result storage | [SdoClient.h](../adapters/canopennode/SdoClient.h), [implementation](../adapters/canopennode/SdoClient.cpp) | [Fake-node harness](../test/sdo_transfer_test.cpp), retained bridge and outer owner | Two unique routes, one active/retained transfer each; permanent ambiguity quarantine, immutable cutoffs, no replay |
| Controller submission/retirement boundary | [Driver.cpp](../adapters/canopennode/Driver.cpp), [port layout](../adapters/canopennode/CO_driver_target.h) | nextTx/confirmTx and queue/cancel/deadline tests | Software staging only; hardware acceptance may already transmit. SDK evidence remains 27 |
| Original owner/proof migration | [Compatibility facade](../experiments/canopen_service_fit/fit_proof.cpp) | [Retained tests](../experiments/canopen_service_fit/main.cpp), [outer owner](../experiments/api_ownership/CanBusOwner.cpp) | Same selected engine/storage; no duplicate active protocol implementation or second RX dispatcher |
| Optional target and minimal package | [Adapter CMake](../adapters/canopennode/CMakeLists.txt), [commands](../adapters/canopennode/README.md) | Registered native contract and isolated package/header consumers | Core exports only portable values; adapter/stack stay optional and outside installation |

## Passive network evidence (prompt 15)

| Responsibility | Declaration / implementation | Actual caller / focused evidence | Deliberate boundary |
| --- | --- | --- | --- |
| Configured heartbeat/boot-up/EMCY envelopes | [Network.h](../include/MotorControlCAN/canopen/Network.h) / [Network.cpp](../src/canopen/Network.cpp) | [Codec vectors](../test/network_codec_test.cpp), isolated installed consumer and owner projection | Native supported states only; complete raw EMCY and host binding/epoch; no producer, I/O or health state machine |
| Shared node facts, history and routing | [NetworkState.h](../adapters/canopennode/NetworkState.h), [SdoClient](../adapters/canopennode/SdoClient.h) / [projection](../adapters/canopennode/Network.cpp) | [Passive owner vectors](../test/network_events_test.cpp), retained fit/outer owner | Existing Channel[2], one anchor per physical node, four copied history entries; receive time independent of processing and SDO results |
| Synchronous dependent invalidation | Same optional owner API and production `invalidateAxisReference` | Actual axis/reference/scale/prepared fixture, full history and interrupted SDO vectors | Application owns actual configuration/maps/drive/mode caches. Callback revokes their dependencies without clearing raw faults or outcomes; no placeholder cache or channel reopening |
| Passive backend choice and primary parity | [Decision correction](decisions/canopen_stack.md#prompt-15-passive-boundary-correction), [source/results report](reports/prompt_15.md) | Actual pinned HB/EM consumer parity proof | One checked projection replaces planned native passive consumers; active selected SDO/NMT/PDO engine boundaries remain separate |

## Explicit NMT and service eligibility (prompt 16)

| Responsibility | Declaration / implementation | Actual caller / focused evidence | Deliberate boundary |
| --- | --- | --- | --- |
| Pure typed NMT envelope | [Network.h](../include/MotorControlCAN/canopen/Network.h) / [Network.cpp](../src/canopen/Network.cpp) | [Independent literal vectors](../test/nmt_codec_test.cpp), isolated installed consumer | ID-zero domain broadcast is not an ordinary axis; five reviewed generic commands, no device behavior claim |
| One bounded manager request/result | [Nmt.h](../adapters/canopennode/Nmt.h), [SdoClient.h](../adapters/canopennode/SdoClient.h) / [Nmt.cpp](../adapters/canopennode/Nmt.cpp) | [Native producer/owner cases](../test/nmt_services_test.cpp) | Actual selected producer, no local slave process, no synthetic acknowledgement or second receiver; one result independent of SDO capacity |
| Current state versus serviceability | Same optional types and `serviceEligibility`, integrated SDO admission/handoff/enqueue checks | Disabled-heartbeat probe/write, Stopped, finite-age, transition/SDO and TPDO receive-invariant tests | Checked SDO is service evidence only; unknown NMT allows configured TPDO validation without Operational inference |
| Broadcast and lifecycle dependencies | `applyNmt`, `stopSdo`, `invalidateNode`, copied affected binding/epoch records | Shared-node, passive-node, reset/late-event, retained-terminal and uncertainty regressions | Nonreset intent preserves configuration facts; reset revokes them, never reopens SDO or replays a write |

## PDO layouts and qualified observations (prompt 17)

| Responsibility | Declaration / implementation | Actual caller / focused evidence | Deliberate boundary |
| --- | --- | --- | --- |
| Pure layout and exact native decode | [Pdo.h](../include/MotorControlCAN/canopen/Pdo.h) / [Pdo.cpp](../src/canopen/Pdo.cpp) | [Independent vectors](../test/pdo_codec_test.cpp), isolated installed consumer | Standard Classical data, complete byte fields, checked profile counts; RAW conflicts never become guessed signed values |
| Reviewed template/layout facts | [Ledger](reference/lichuan_objects.json), [generator](../scripts/generate_lichuan_objects.py) / [generated profile Pdo.h](../include/MotorControlCAN/profiles/lichuan_c/Pdo.h) | Generator/checker negatives and generated-profile owner tests | Four Lichuan sets/slots are source facts; eight-set contrast is test-only; no live or write permission follows |
| Actual map acquisition | `pdoMapFromReadback`, owned `PdoMapEvidence` and SDO completion admission epoch/time | [Normal bounded SDO acquisition and failure](../test/pdo_observations_test.cpp) | Communication/map records only, including WO payload targets without uploads; all results must share current node provenance and coherent configuration premise |
| Shared routing and sample qualification | [PdoState.h](../adapters/canopennode/PdoState.h), [owner](../adapters/canopennode/SdoClient.h) / [projection](../adapters/canopennode/Pdo.cpp) | Same-ID/same-DLC swapped-map, source timing, reset/collision/capacity and copy-observer regressions | Eight permanently bound routes reference the existing node table; independent map/sample facts, no second receiver, NMT inference or implicit mutation |
