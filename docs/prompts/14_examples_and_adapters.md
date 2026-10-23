# Stage 14 Build native and shared firmware examples

Apply [COMMON.md](COMMON.md). Prerequisites: stages 12–13.

## Audit

Read RS shared ProbeApp/ProbePlatform/ArduinoPlatform/IdfPlatform, explicit board configuration, USB output and build pins. Compare ADS1115/INA228 owner examples and SHT3x neutral command processor. Inspect the selected ESP-IDF/TWAI API and hardware requirements for the exact SDK/MCU; do not assume current documentation matches a different pinned release.

## Deliver

1. Provide an offline native example, a fake-node integration example, and an optional native SocketCAN example if its platform dependency is isolated. All call the same public motion/read APIs; portable core builds must work without CAN hardware or Linux headers.
2. Implement a thin ESP32 TWAI adapter taking explicit bus pins/configuration. A suitable external CAN transceiver is required; no RS UART/DE pin or board's presumed wiring may be reused. Supply a concrete board configuration only when known, with compile-only versus physical evidence labeled.
3. Build the same application, owner and eventual console sources under Arduino and native ESP-IDF, with thin platform startup/clock/USB/idle/diagnostic adapters. Do not fork motion behavior by framework. Explicit local CAN controller configuration/start is allowed; startup must not send unsolicited NMT/device-setting/motion commands or automatically enable a drive.
4. Verify SDK queue admission, TX-completion/error semantics, controller retransmission, RX metadata/time, bus-off and recovery, filter configuration, ISR/callback storage and pending-TX retirement. Use actual pinned source/docs plus fakes. Recovery cannot release stale queued motion into a restarted controller.
5. Keep task stack/internal RAM, optional PSRAM trace buffers and ISR memory requirements application-local. No PSRAM or S3-only assumption in installed headers. Bound service work under console/load pressure and account for dropped frames/events.
6. Update PlatformIO/native-IDF build files, dependency pins, compile-only CI and setup instructions. Core-only package must remain offline and framework independent. Native-IDF component consumption and standalone firmware are different checks.

## Concrete implementation contract

Create a small example inventory with build/run purpose:

| Example | Required role | Traffic behavior |
| --- | --- | --- |
| `units_preview` | Exact preparation with explicitly supplied configuration | Offline; no controller |
| `native_probe` fake mode | Owner/service/read demonstration and repeatable event injection | In-memory fake only |
| Optional SocketCAN adapter | Native integration through the shared owner | Explicit interface/start/request |
| `probe_cli` Arduino | Thin platform around shared ProbeApp/ProbeConsole | No device command until explicit request |
| `probe_idf` | Same application/owner/console under native ESP-IDF | Same command and side-effect contract |

The native example is first-class usage documentation. It must not include Arduino types, need a firmware board, or depend on CLI strings to use the public API. Keep example board pins outside installed headers; if no board is selected, show explicit required configuration and a labeled compile-only fixture.

Define the adapter's evidence table against the actual pinned SDK: accepted into software queue, confirmed hardware transmission if available, error, bus-off, receive timestamp meaning and pending-TX disposal. If the API cannot identify a precise frame as transmitted, report that limit and retain uncertainty; a successful driver enqueue is not sufficient.

Use `CanBusOwner::service` from one cooperative owner context. ISR/driver callbacks copy bounded metadata into the chosen adapter path; they do not invoke the CLI, allocate unbounded objects or compete for RX. Clock acquisition and 32-to-64-bit wrap extension belong to the platform adapter. All core deadlines receive explicit monotonic microseconds.

Build the same command/application sources under both firmware systems. No duplicated private enable/move/stop path per SDK. Demonstrate fake read then ordinary prepared work/result inspection, with firmware-specific startup kept short. Build pins and dependencies come from the selected release's source/docs, not merely latest online examples.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Each example documents purpose, exact build/run commands, required configuration, expected output and whether it touches hardware. Annotate example owner/transport/platform contracts with Doxygen and explain SDK limitations.

Use [DELEGATION.md](DELEGATION.md) for disjoint platform adapter review/builds after the common transport contract is fixed. The lead integrates shared code and checks that both builds actually compile the same app/console.

## Verify and exit

Build native/no-framework, Arduino and ESP-IDF examples with the same operation coverage. Run SDK fakes for TX rejection, completion timeout, bus-off recovery and stale queue retirement. Test clock wrap extension and nonblocking console behavior. Do not flash merely to prove a compile.

Exit with reproducible builds and explicit hardware prerequisites. Unavailable SDK/hardware is reported precisely; host gates still run. A successful fake/compile is not bench qualification.
