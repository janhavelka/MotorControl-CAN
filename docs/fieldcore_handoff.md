# Prospective FieldCore CAN integration

The [standalone consumer](../examples/fieldcore_consumer/README.md) consumes only
installed public APIs with a deterministic fake, without FieldCore, an SDK,
a stack or a controller. Its synthetic reports and prerequisite grants provide
no physical qualification. No sibling repository was changed.

FieldCore-node currently has no CAN owner, board route or request for one, and
no CAN motor health row or CAN drive-settings group. A CAN BusId, routing, settings and
health projection would be new work, separately authorized.

## Current source review

On 2026-11-15, the source/package/documentation review observed MotorControl-RS
at `0e3b559e5bf6d7affb71ca16974747e76f550250`. The checkout was initially
clean; an unrelated stage-report modification appeared during the audit.
The inspected package, public-header, version and documentation seams and their
working-byte/committed-blob hashes are recorded in the
[source audit](reports/prompt_49_sources.json). That unrelated report supplied
no source behavior. The earlier motion review in [prompt 46](reports/prompt_46_audit.md)
read `MoveOperation.h`, `Axis.h` and
`PositionStageApp.h::beginPositionStage/servicePositionStage`. Its adopted
copied intent, caller-owned contexts, yielded work and settlement before
interruption remain the handoff contract. RTU framing, echo policy and device
timing remain separate from CAN assumptions.

FieldCore-node was clean at `c40ea8341edf8d665460b902cafefbe857a7f73f`.
The ownership/result seam is unchanged since prompt 46: its review covered
`include/TunnelMonitor/contracts/MotorControl.h::MotorCommand/MotorResult`,
`src/devices/rs485_motor/Rs485MotorModule.cpp::startMotor/nextStep/acceptObservation`,
`src/rs485/Rs485Task.cpp::submitMotorCommand/completeMotor/serviceMotorControl` and
`include/TunnelMonitor/measurement/rodtester/RodCyclePorts.h::submitMotor/takeMotor`. Adopt copied
submission, prepare/next/advance, exact result take and retained uncertainty.
Existing RS ordinary/stop lanes imply no current CAN integration. The current
bounded review confirms that `contracts/Health.h::BusId` has no CAN entry.
`SystemHealthProjection.h::appendSelectedProfileDeviceHealth` already projects
generic passive device health, and the RS motor module exposes `snapshot` and
`copyStatus`. RodTester also has product motor-ratio settings. These existing
surfaces do not supply a CAN owner, CAN drive settings or CAN health projection.

## Responsibility mapping

| Prospective responsibility | Demonstration / existing boundary |
| --- | --- |
| Module | MotorModule owns concrete move/observation contexts and retained typed terminal; no generic executor. |
| Scheduling | A CAN owner would exclusively receive, dispatch passive events and schedule bounded channel work. The fake supplies one pending exchange and one retained terminal, not another production owner. |
| Admission | Local preparation is distinct from wire TX. Downloads call noteMoveTransmission before handoff; queue acceptance never establishes native acknowledgement. |
| Wire settlement | Adapter retirement evidence or uncertainty survives interruption; local IDs cannot identify an identical late SDO reply. |
| Execution / completion | Production contexts retain setup, trigger acceptance and native completion independently. Native completion is not independent shaft measurement. |
| Result lifetime | takeResult requires the exact operation ID, preserves output on mismatch, then releases the module slot. Service terminals remain until consumed and explicitly released. |
| Interruption | Pending work settles before terminal publication. Local cancellation is not motor stop; an independent stop lane remains prospective integration work. |
| Settings | Existing typed groups would use the same owner and retain confirmed/unknown partial effects and readback. No existing CAN drive-settings group is claimed. |
| Binding / reference | Configuration changes invalidate dependent work under production generations. Rebinding cannot qualify reused PDO bytes or physical position. |
| Health | A prospective cached row would keep liveness, drive faults, operation uncertainty and application state distinct. |

The example supplies explicit synthetic state, identity, predecessor, reservation
and scales. It sends no enable to make prerequisites pass. Fake latency is not
vendor timing. A real integration must provide qualified facts and retirement.

## Independent commands

```text
cmake -S . -B .build/core -G Ninja -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="C:/temp/motor sdk"
cmake --build .build/core
cmake --install .build/core
cmake -S examples/fieldcore_consumer -B .build/consumer -G Ninja -DCMAKE_PREFIX_PATH="C:/temp/motor sdk"
cmake --build .build/consumer
.build/consumer/fieldcore_consumer.exe
```

On Linux use an explicit prefix such as `/tmp/motor-sdk` and executable
`.build/consumer/fieldcore_consumer`. The package test copies only the example
to a temporary location and relocates its installed dependency. No repository,
reference, test, sibling or stack includes are available there.

The library always builds Lichuan. Core-header-only consumption is an include
boundary, not a profile-selection option. Tests also build Lichuan and combined
proof/production headers, including C++11 and C++17 without RTTI. N5, EPOS4 and
other protocol fixtures are not installed or discoverable support. See the
[profile contract](profile_contract.md) for the reviewed extension limits.
