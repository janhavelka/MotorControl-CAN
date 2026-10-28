# Prompt 28 Arduino and ESP-IDF integration

Apply [COMMON.md](COMMON.md). Prerequisites: [27](27_twai_adapter.md).

## Objective and boundaries

Build the same shared application and CAN owner under Arduino and native ESP-IDF using the reviewed transport adapter. Finish reproducible firmware builds, platform configuration and integration documentation. Console parsing arrives in prompts 40–42; current examples use explicit public calls and bounded platform seams. No temporary command parser, success stub or duplicated SDK-specific motor path is needed. This prompt authorizes compile/fake work, not flashing or physical motor activity.

## Read and audit

Refresh RS Arduino/IDF platform startup, USB output, board configuration, build pins and example consumers. Compare ADS1115/INA228 example setup and current framework-neutral owner conventions; read applicable FieldCore interfaces only for integration boundaries. Inspect actual target workflows, dependency versions and prompt 27's SDK evidence. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), the example/build sections of [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Add the Arduino `probe_cli` platform and native-IDF `probe_idf` startup/build integration around the same `ProbeApp` and owner sources. Keep entry points short: explicit local configuration, bounded platform setup, supplied time, application service and idle. Do not fork application behavior by SDK.
2. Supply exact board configuration only when established. Otherwise require explicit user configuration and identify any build fixture as compile-only. Document controller, external transceiver, pins, bitrate and interface prerequisites without borrowing RS bench defaults. Local controller initialization is distinct from sending a device command; startup must not issue NMT start/reset, parameter changes, enable or movement.
3. Implement bounded platform input/output, clock acquisition/wrap handling, USB/serial diagnostics and idle hooks under the existing seams. Keep ISR/driver storage in SDK-appropriate memory. Record task stack/queue/trace capacities, drop accounting and service budget. Console-pressure behavior can be tested using supplied bytes even though command parsing comes later.
4. Pin PlatformIO/framework and native-IDF dependencies consistently with the adapter's supported SDK versions. Register compile-only example gates using existing verification/CI patterns. Verify native-IDF component consumption separately from building standalone firmware; neither is equivalent to the other.
5. Keep native offline/fake examples buildable without either SDK. If a SocketCAN example is selected, implement it as an isolated optional adapter through the same owner, with an explicit interface/start/request and no Linux headers in core builds. Record a deliberate omission if it adds no current value; optional SocketCAN is not a release blocker.

## Verify and exit

Build Arduino and standalone ESP-IDF examples, the IDF component consumer and the existing native examples through exact recorded commands. Inspect build inputs to prove both firmware systems compile the shared application/owner. Test platform clock wrap, bounded input/output, RX routing, queue exhaustion and recovery through adapter fakes. Verify zero unsolicited device traffic at startup or selection. Document unperformed toolchain gates precisely and finish independent host work; an unavailable SDK does not turn a required build into a pass. Full command/schema parity is deferred explicitly to prompt 42, when the real console exists.

## Documentation and delegation

Each example README includes purpose, exact build commands, pinned prerequisites, configuration, expected demonstration output, traffic behavior and compile-only versus physical evidence. Flash/run instructions may be documented without executing them. Add platform Doxygen and update getting-started/source-map/verification documents and CHANGELOG. Normal engineering agents can own separate Arduino and IDF build checks after common sources are fixed; one lead owns shared application changes. Use stronger review for any SDK mismatch affecting TX uncertainty or recovery. Follow runtime availability and same-branch rules in [DELEGATION.md](DELEGATION.md).

Close this prompt's owned scope and checks; record `docs/reports/prompt_28.md` with actual builds and physical prerequisites. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
