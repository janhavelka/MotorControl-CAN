# MotorControl-RS source map

Reviewed against the implementation on 12 October 2026. This page locates the
current layers; [architecture](architecture.md) defines their contracts and
[API/CLI coverage](ess_api_cli_coverage.md) records remaining gaps. The core
contains no framework I/O, heap allocation, clocks, scheduler or hidden retry.
The standalone firmware supplies those application responsibilities.
All seven [maintainability stages](archive/prompts/maintainability/README.md)
are complete at their specified seams. Their archived handoffs describe the
baselines and verification; this map describes current code.

## Installed core

| Source | Responsibility |
| --- | --- |
| [MotorControlRS.h](../include/MotorControlRS/MotorControlRS.h) | Common umbrella; profile headers remain explicit |
| [Status.h](../include/MotorControlRS/Status.h) | Pure-call validation result, numeric detail and static diagnostic text |
| [Units.h](../include/MotorControlRS/Units.h), [Units.cpp](../src/Units.cpp) | Checked displacement, velocity and acceleration conversion with explicit scales |
| [Axis.h](../include/MotorControlRS/Axis.h), [Axis.cpp](../src/Axis.cpp) | Exact rational target preparation, origins, reference validity, quantization and limits |
| [ReadOperation.h](../include/MotorControlRS/ReadOperation.h), [ActionOperation.h](../include/MotorControlRS/ActionOperation.h), [MoveOperation.h](../include/MotorControlRS/MoveOperation.h), [VelocityOperation.h](../include/MotorControlRS/VelocityOperation.h) | Shared request/event/evidence value types |
| [Discovery.h](../include/MotorControlRS/Discovery.h), [Discovery.cpp](../src/Discovery.cpp) | Finite profile inventory, probe capabilities and outcomes |
| [Traffic.h](../include/MotorControlRS/Traffic.h), [Traffic.cpp](../src/Traffic.cpp) | Bounded caller-owned passive traffic records and cursors |
| [Codec.h](../include/MotorControlRS/profiles/ess_rs/Codec.h), [Codec.cpp](../src/profiles/ess_rs/Codec.cpp) | ESS builders, checked response parsing, raw exceptions and pair word order |
| [rtu/Frame.h](../src/rtu/Frame.h) | Private framing/CRC/overlap mechanics without transport or device policy |
| [Registers.h](../include/MotorControlRS/profiles/ess_rs/Registers.h), [Types.h](../include/MotorControlRS/profiles/ess_rs/Types.h), [Access.h](../src/profiles/ess_rs/Access.h) | Generated register descriptors/enums and compact codec access policy from one ledger |
| [Defaults.h](../include/MotorControlRS/profiles/ess_rs/Defaults.h) | Explicit bench conversion assumptions; no device configuration or detection |
| [Reads.h](../include/MotorControlRS/profiles/ess_rs/Reads.h), [Discovery.h](../include/MotorControlRS/profiles/ess_rs/Discovery.h) | Typed bounded observations and minimal checked probe preparation |
| [Actions.h](../include/MotorControlRS/profiles/ess_rs/Actions.h), [Position.h](../include/MotorControlRS/profiles/ess_rs/Position.h), [Velocity.h](../include/MotorControlRS/profiles/ess_rs/Velocity.h), [Homing.h](../include/MotorControlRS/profiles/ess_rs/Homing.h) | Concrete action/motion contexts, prerequisites, yielded work and event advancement |
| [ActionEvidence.h](../src/profiles/ess_rs/ActionEvidence.h) | Private mechanical transport-evidence copier; validation, publication and motor outcomes stay in the four sequences |
| [DriverSettings.h](../include/MotorControlRS/profiles/ess_rs/DriverSettings.h), [Segments.h](../include/MotorControlRS/profiles/ess_rs/Segments.h), [ControlSettings.h](../include/MotorControlRS/profiles/ess_rs/ControlSettings.h), [Tuning.h](../include/MotorControlRS/profiles/ess_rs/Tuning.h) | Typed native settings and bounded whole-candidate validation |
| [Communication.h](../include/MotorControlRS/profiles/ess_rs/Communication.h), [Persistence.h](../include/MotorControlRS/profiles/ess_rs/Persistence.h) | Explicit communication/save/restore operations with retained uncertainty |
| [Version.h](../include/MotorControlRS/Version.h) | Generated version constants from `library.json` |

Profile implementations live in [src/profiles/ess_rs](../src/profiles/ess_rs).
Concrete `next...` functions yield a request, wait or result; `advance...`
functions consume supplied evidence and time. There is no generic `Sequence.h`
or `Commands.h` engine to import. Public common forwarding overloads call the
same profile contexts. A raw codec remains independently usable and does not
claim readiness, physical completion or complete native command coverage.

## Standalone application and adapters

| Source | Responsibility |
| --- | --- |
| [RtuRunner](../examples/common/RtuRunner.h) | One bounded transaction, physical TX/DE settlement, RX timing/framing, deadlines and explicit recovery |
| [RtuBusOwner](../examples/common/RtuBusOwner.h) | Copied admission, producer scheduling, urgent capacity, sequence reservation and retained checked results |
| [EssRtuValidator](../examples/common/EssRtuValidator.h) | Checked ESS response validation at owner settlement |
| [Esp32S3Uart](../examples/common/Esp32S3Uart.h) | S3 UART2/pin configuration, optional GPTimer capture and atomic TX/release evidence |
| [HostSerial.h](../examples/common/HostSerial.h), [BoardPins.h](../examples/common/BoardPins.h) | Host tuple timing/capability and standalone board configuration |
| [ProbeApp.cpp](../examples/probe_cli/ProbeApp.cpp) and its operation helpers | Application-owned concrete contexts/caches, private record-kind dispatch, independent lifecycle/ownership facts, operation admission, sequence driving and results |
| [ProbeConsole.cpp](../examples/probe_cli/ProbeConsole.cpp), [ConsoleText.cpp](../examples/probe_cli/ConsoleText.cpp) | One command inventory, strict parsing, human/JSON rendering and bounded output |
| [HomeConsoleParser.cpp](../examples/probe_cli/HomeConsoleParser.cpp), [HomeConsoleFormat.cpp](../examples/probe_cli/HomeConsoleFormat.cpp) | Private HOME grammar and bounded formatting; Console retains admission, correlation and emission |
| [DebugSession.h](../examples/probe_cli/DebugSession.h), [DebugSession.cpp](../examples/probe_cli/DebugSession.cpp) | Application-owned, noncopyable capture storage and at most one optional traffic record per service turn |
| [ProbePlatform.h](../examples/probe_cli/ProbePlatform.h), [ArduinoPlatform.cpp](../examples/probe_cli/ArduinoPlatform.cpp), [IdfPlatform.cpp](../examples/probe_idf/main/IdfPlatform.cpp) | Small independent startup/time/USB/idle/diagnostic adapters |
| [Esp32UsbConsole.cpp](../examples/probe_cli/Esp32UsbConsole.cpp) | Shared bounded USB Serial/JTAG output |
| [Esp32Load](../examples/probe_cli/Esp32Load.h) | Optional task/console/load fixture; no independent motor path |
| [probe_idf](../examples/probe_idf), [platformio.ini](../platformio.ini) | Native-IDF application and Arduino example builds |
| [units_preview](../examples/units_preview) | Offline conversion/catalogue example |

The physical UART has one owner. The application executes the core's yielded
work and returns copied/qualified evidence. Neither the console nor the passive
traffic observer consumes bytes directly from normal protocol processing.
Large task-context storage may use PSRAM; ISR/driver-required state and stacks
remain internal under the adapter contract. These choices do not constrain
other core consumers.

## FieldCore boundary

The relevant FieldCore owner/backend/binding sources were inspected read-only
at commit `c591ecddcd28ad3965c89a60b37279aa7f9e9940`. Its existing owner remains
appropriate, but its eight-byte TX capacity, unconditional matching-prefix echo
stripping, fixed/suffix completion, untimed RX batches and measurement-only
command/results contract require explicit adaptation before motor integration.
The [handoff](fieldcore_handoff.md) is the current implementation plan and source
map for those edits; the historical reviews below are supporting evidence.

Keep exact motion types and contexts in a FieldCore-owned device module, with
its existing bus owner/backend executing work. Do not copy the standalone CLI,
replace sensor completion rules with ESS rules, tunnel motion through `Measure`,
or treat administrative `setEnabled`/cancel as drive enable/stop. No FieldCore
source is changed or qualified by this repository's tests.

## Tests, packaging and evidence

The Python host [bench_probe.py](../scripts/bench_probe.py) owns serial sessions,
framing, correlation and lifecycle policy. Its private
[_bench_evidence.py](../scripts/_bench_evidence.py) supplies pure read/motion
evidence checks and shared helpers, exposed through the existing host imports
and Console aliases. Settings and host/commissioning/discovery/debug/profile
checks retain their original session/module boundaries.
Live terminals and retained-result inspection share one closed payload routing
function in `Console`; original-request correlation and stateful confirmation
updates remain with the serial session. In the core, `DriverSettings.cpp` keeps
ordered admission helpers separate from in-place request/context publication
and the unchanged group scheduler.

[Native tests](../test) cover codecs, arithmetic, sequence failures, settings,
owner scheduling, transport timing, SDK fakes, console/API parity and Python
campaign behavior. [Verification](verification.md) defines quick/full checks,
clean source/install/package consumers and required CI. Python is required for
repository verification but not for the exported core package.

The root [CMakeLists.txt](../CMakeLists.txt) supports native build/install and
ESP-IDF component consumption. [library.json](../library.json) supplies package
identity/version. Original vendor/reference files are excluded from core
packages and retain their own licensing. The
[release scope](releases/1.0.0.md) separates build/native coverage from the
recorded motor/firmware/fixture evidence; S2 is compile-only.

Earlier architecture, capture, owner and console development is indexed in the
[development archive](archive/README.md). Its version numbers, suite counts and
then-planned work describe those snapshots, not the current source inventory.
