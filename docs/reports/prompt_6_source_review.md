# Prompt 6 live API and ownership source review

2026-10-28. This review supports the common-contract proof on
`promptset/motorcontrol-can`. It changes only this report. The lead owns shared
contracts, integrated checks and all Git mutations. No sibling edit, fetch,
hardware activity or sibling test execution occurred.

## Refreshed sources

Paths below are relative to `C:/Users/Honza/Documents/Projects`. Local HEAD and
`git status --short` were inspected before and after the source review.

| Checkout | Local HEAD | Before / after |
| --- | --- | --- |
| `MotorControl-RS` | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | clean / clean |
| `ADS1115` | `235af3e8c722661a89a44766a4ccd903a654f6fe` | clean / clean |
| `INA228` | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | clean / clean |
| `FieldCore-node` | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | clean / clean |

These match prompt 5. The new inspection covers exact axis/operation declarations,
their implementations and callers, configuration provenance and the owner result
lifetime. Other I2C modules have no newly affected seam in this bounded proof.
The parent/root AGENTS, prompt README/COMMON, prompt 6, CONVENTIONS sections 3-4,
MULTIDRIVE, DOCUMENTATION, MAINTAINABILITY, DELEGATION, multi-drive decision,
corrected contrast ledger and prompt-5 source review were read. Historical reports
were navigation aids; the observations below come from current local source.

## Literal RS API and observed behavior

| Current source | Actual declaration/behavior and checked caller/test seam | CAN consequence |
| --- | --- | --- |
| `MotorControl-RS/include/MotorControlRS/{Units,Axis}.h`, `src/{Units,Axis}.cpp` | `Rational` retains signed I64 numerator/U64 denominator. `PositionRequest` carries unit, coordinate frame, relative basis, wrapping/path/tie, generation, rounding and explicit approximation allowances. `AxisConfig` separates command/full-step/encoder/load scales and origins; optional scales are needed only by relevant conversions. `configureAxis` validates candidate and stationary evidence before publication and generation change. | Preserve exact intent and atomic output publication; full steps remain optional. Do not copy the serial `ReadTarget` into the common axis. Production arithmetic remains prompts 9-10. |
| `MotorControl-RS/include/MotorControlRS/ReadOperation.h`, `profiles/ess_rs/Reads.h`, `src/profiles/ess_rs/Reads.cpp` | `ReadTarget { id, address, generation }` contains a real RS unicast address. `ReadEvent` borrows its frame only for `advanceRead`. `ReadContext` copies target, active serial tuple, wiring and bounded step evidence. `nextRead` repeats the current work; `advanceRead` validates correlation/envelope before mutation and copies raw evidence. Its `Ok()` means consumed event, including terminal failure. `getIdentity`/`getConfig` publish a whole successful operation; `getStateBlock` can publish an earlier successful block after a later failure. Unknown raw codes retain successful read status with interpretation flags. `test/reads_test.cpp` checks malformed/wrong-kind/incomplete publication and unchanged outputs. | Common binding identity is distinct from CANopen node/channel/subaxis. Typed getters remain checked publication, with provenance/freshness obligations explicit. No universal result getter is present. CAN events contain native service evidence rather than serial closure/echo assumptions. |
| `MotorControl-RS/include/MotorControlRS/{ActionOperation,MoveOperation,VelocityOperation}.h`, `profiles/ess_rs/{Actions,Position,Velocity}.h`, corresponding `.cpp` | `ActionContext` retains `state`, `outcome`, `execution`, `completion`, `status`, write/observation/failure evidence. `MoveContext` additionally retains `setupExecution`, `stagingApplied`, `uncertain` and distinct staging/trigger evidence. A setup may have changed parameters while trigger execution is `NOT_TRANSMITTED`; uncertainty is therefore broader than trigger acknowledgement. `VelocityContext` retains `stopDueUs`, original `deadlineUs`, `needsStop` and nested stop context. `serviceVelocity` advances supplied time only after pending evidence is consumed; cancellation retains stop obligation and never yields a stop automatically. | Concrete context evidence must distinguish setup, trigger and observed completion. CAN TX/ACK cannot imply native acknowledgement. Keep a representative proof sequence explicitly synthetic; ESS sequences and qualification masks are not Lichuan CAN capabilities. |
| `MotorControl-RS/examples/probe_cli/ProbeApp.cpp` around `advanceActions`, `advanceOperation`, `moveProgress`, `releaseMove`; `StateCache.h`; `ProbeConsole.cpp` read formatting | The application routes retained owner completions into the original concrete context, then explicitly releases the service slot. It services finite velocity only with no borrowed admitted token. `moveProgress` copies concrete context members into an example view; it is not an installed generic result API. Read console/cache paths call checked typed getters. A later stopped report can reconcile an application interlock while historical failed-operation uncertainty remains unchanged. | Service result retention and caller-owned operation retention are different lifetimes. Releasing an owner slot cannot erase the operation result or uncertain external effects. Avoid promoting application helper names into common core. |

Representative exact signatures, in `MotorControlRS::ESS_RS`, are:

```cpp
Status nextRead(const ReadContext&, uint64_t nowUs, PreparedRead& output) noexcept;
Status advanceRead(ReadContext&, const ReadEvent&, uint64_t nowUs) noexcept;
Status getIdentity(const ReadContext&, IdentityObservation& output) noexcept;
Status getConfig(const ReadContext&, ConfigObservation& output) noexcept;
Status getStateBlock(const ReadContext&, uint8_t block, StateObservation& output) noexcept;
Status nextAction(const ActionContext&, uint64_t nowUs, PreparedAction&) noexcept;
Status advanceAction(ActionContext&, const ActionEvent&, uint64_t nowUs) noexcept;
Status nextMove(const MoveContext&, uint64_t nowUs, PreparedMove&) noexcept;
Status advanceMove(MoveContext&, const ActionEvent&, uint64_t nowUs) noexcept;
Status serviceVelocity(VelocityContext&, uint64_t nowUs) noexcept;
Status nextVelocity(const VelocityContext&, uint64_t nowUs, PreparedVelocity&) noexcept;
Status advanceVelocity(VelocityContext&, const ActionEvent&, uint64_t nowUs) noexcept;
```

Preparation families are `prepareIdentity/Config/State`,
`prepareEnable/Release/ClearAlarm/Stop`,
`prepareMoveRelative/Absolute/Angle` and `prepareVelocity`. Their concrete profile
parameters differ; there is no common universal operation context. RS
`MoveRequest::speedRpm` is U16 and `PreparedVelocityTarget::nativeRpm` is I16;
`Axis.cpp` contains the corresponding native RPM narrowing. CAN must preserve
signed rates wider than these RS fields and perform profile-owned narrowing.

## Application owner and sensor contrasts

`MotorControl-RS/examples/common/RtuBusOwner.{h,cpp}` implements exactly
`admit(const BusRequest&, uint64_t, RequestId&)`, `admitUrgent` with the same
shape, `service(uint64_t, bool recoveryReady = true)`,
`const Completion* result(const RequestId&) const`,
`bool release(const RequestId&)` and `Cancel cancel(const RequestId&, uint64_t)`.
All are `noexcept`. Admission validates before publishing the ID, copies request
bytes and expectations and reserves result capacity. `result` is non-consuming,
returns null for pending/foreign/stale IDs, and remains valid until matching
terminal release or owner destruction. `RequestId` includes owner/slot/generation;
generation does not wrap. `test/bus_owner_test.cpp::testCapacitiesAndIds` and
`testTransientAndReadShapes` inspect held results, stale IDs, unchanged rejection
output and caller-buffer mutation. `test/position_command_test.cpp` checks setup
uncertainty without start/replay; `test/actions_test.cpp` and
`test/velocity_test.cpp` separately inspect UNKNOWN execution and missing stop
evidence. These tests were read, not run here.

`ADS1115/include/ADS1115/{Config,ADS1115,Status}.h` and `src/ADS1115.cpp` separate
copied `DriverConfig`, desired/candidate/applied profile and configuration state.
`bind` performs no I/O and refuses outstanding work/results; `getAppliedProfile`
returns generation/state rather than declaring desired settings verified.
Synchronous transport callbacks borrow buffers and must honor timeout including
external serialization. `takeResult(OperationToken, OperationResult&)` consumes
only a matching retained terminal. `examples/02_owner_safe_poll/main.cpp` binds
the application context and calls `poll(nowMs, 1)` before consuming the active
token. Focused `test/test_basic.cpp` cases retain owner results and reject
unrelated calls without I/O.

`INA228/include/INA228/{Config,INA228,Status}.h` and `src/INA228.cpp` retain desired
configuration separately from hardware verification. `bind` copies configuration
without I/O; callback context remains application-owned. `pollJob(nowMs,
maxTransfers)` and `takeJobResult(expectedOperationId, JobResult&)` use a different
lifecycle from ADS1115; the latter consumes a checked terminal exactly once.
`examples/esp_idf/basic/main/main.cpp::pollOwnerJob` checks active identity before
taking the terminal. `test/test_basic.cpp` around the cooperative sample test
checks retained-result pressure and second-take rejection. Both sensors use
`Status::Ok()`/`.ok()` rather than RS free `Ok()`/`.isOk()`. Their profile replay,
conversion waits, health thresholds and consuming result APIs do not become motor
recovery, retry or CAN result policy.

## FieldCore configuration and endpoint seam

`FieldCore-node/include/TunnelMonitor/contracts/MotorControl.h` freezes
`MotorTarget`, `MotorAxisSnapshot`, original command deadline and submission token.
The snapshot carries exact command/gear ratios and configuration/reference
generations. Results keep admission, wire settlement, setup/trigger effects,
historical uncertainty, completion and stopped evidence independently.
`include/TunnelMonitor/device/Rs485MotorModule.h` and
`src/devices/rs485_motor/Rs485MotorModule.cpp::configure/startMotor` show that
configuration is copied, reconfiguration requires a newer target generation, and
every move compares the full frozen axis snapshot. Invalid move scales/readiness
do not obstruct an explicit stop/read. `test/host/rodtester/motor_module.cpp`
checks this separation and ACK without stopped observation. Its 1..247 address,
positive U16 RPM, qualified finite-age policy and native ramp words are actual
RS consumer constraints, not common CAN constraints. No CAN FieldCore integration
was added or claimed.

## Contrast and coverage intent

The corrected [contrast ledger](../reference/drive_contrasts.md) owns source
values and applicability. Prompt 6 should exercise C1/C4 and the independent-scale
invariant through its real proof types: N5 stepper/BLDC and an EPOS4 servo fit
without full-step geometry; command/feedback scales remain independent; logical
axis and endpoint/subaxis differ; a non-CANopen endpoint needs no CANopen state.
N5 PP entry with bit 4 already set can initiate travel, and EPOS4 acceleration
representation is separately documented from velocity scaling. Those corrected
facts must not become a fabricated native sequence in a fixture. No new vendor
constant or device-facing behavior was selected in this review; no primary
manufacturer or physically measured claim was changed.

[API/CLI mapping](../api_cli_mapping.md) remains the inventory of planned routes.
Proof declarations and native contrast tests may be recorded separately as
contract evidence. They do not make `move`, `read`, N5/EPOS4 discovery or any CLI
route implemented. Preserve typed read getters and concrete motion context
inspection, and distinguish owner `release ID` from `motor-release`.

## Verification boundary

Inspected current RS `CMakeLists.txt`, `scripts/verify.py` and
`.github/workflows/verify.yml`: C++11 CTest registration includes units, axis,
reads/actions, position/velocity, owner and installed-consumer gates. The runner
checks test inventory rather than silently skipping missing registrations; CI
uses Python 3.12, GCC/Clang and IDF 5.5.5. ADS1115/INA228 actual CI files use
PlatformIO 6.1.19 native and framework consumers; their lifecycle suites need not
be rerun for this unchanged-source audit.

This CAN checkout has no `.github/workflows` yet; prompt 8 owns that gate.
`experiments/canopen_service_fit/CMakeLists.txt` pins upstream source hashes,
builds C99 CANopenNode and C++11 proof, and registers its CTest executable.
`scripts/check_promptset.py` verifies the pinned subset against its source pack.
The lead should compile/register the new proof and contrast tests in the real
experiment CMake path, run them, run `python scripts/check_promptset.py` and
`git diff --check`, and distinguish each claim's no-heap/no-I/O layer. Installed
package and strict Doxygen gates remain prompts 7-8. Remote CI availability must
be reported honestly for this substantive contract milestone.

Available local tools were queried: Python 3.12.10, CMake 4.0.1, Ninja 1.12.1,
MinGW g++ 15.1.0. `clang++` and `cl` were not on PATH. No unchanged sibling suite,
firmware build or hardware qualification ran. Report-only local validation is
recorded after the final sibling refresh; integrated proof results belong in the
lead's `docs/reports/prompt_6.md` stage report.

`python scripts/check_promptset.py` passed after this report's local link was
corrected; `git diff --check -- docs/reports/prompt_6_source_review.md` passed.
These are report/planning integrity checks, not a native proof build.
