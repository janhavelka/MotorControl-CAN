# Source re-audit for additional CAN stepper and servo controllers

Date: 2026-10-23. Scope: a fresh read-only inspection of the local source
boundaries relevant to the user's expanded multi-controller intent. This report
changes no sibling repository and does not claim a servo or second vendor is
implemented, documented or hardware-qualified. Recommendations below are design
inferences from inspected source, not device capability facts or web research.

## Revisions and concurrent changes

| Repository | Inspected revision/state |
| --- | --- |
| MotorControl-RS | Started at `5da4ba7f13be457ea7bf9706ffd7d546821669c7`; another session committed documentation while this audit ran, advancing HEAD to `a1f70d0bf91c7c33fd83909d7b9d21b4a777c381` |
| ADS1115 | `381236ebfed813cd7a9aca34520ce031d8a0ff6b`, clean |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean |
| FieldCore-node | `da94ed5280ed42ee3a55e0d977f9cb6565c56b51`, clean |

MotorControl-RS initially had modified AGENTS/README and architecture, backlog,
profile, source-map, runner-platform, multi-vendor and related documentation;
`docs/prompts/standalone/` and `scripts/prepare_lichuan_references.py` were
untracked. At the end, only the latter script remained untracked. A
`git diff --name-only` between the two HEADs showed documentation/agent files
only, including new standalone prompts; the inspected installed headers,
implementation, tests and example owner did not change across that commit.
No changes were discarded or incorporated into this repository by this audit.

Root and shared project guidance were refreshed. Actual source, tests and
consumer contracts were read; prior audit prose was not used as the sole basis.
Paths and line numbers below are relative to the sibling repository named in
each section. No native, firmware, remote CI or hardware tests were run for
this documentation-only audit.

## Main finding

The RS **ownership and operation vocabulary** are reusable. Several fields in
its nominally common headers still encode ESS/RS specifics and should not
become universal MotorControl-CAN contracts. Preserve `preparePosition`,
`prepareMoveAbsolute`, `prepareMoveRelative`, `prepareMoveAngle`, `prepareVelocity`,
`next*`, `advance*`, typed observations, exact arithmetic and retained effects.
Preserve meanings rather than copying every parameter type or struct member.

The minimum useful extension is a clear separation between common motion
intent, CAN transport ownership, protocol services and concrete drive profiles.
CANopen is the first protocol and Lichuan C is the first concrete family.
Neither should define the only possible public endpoint, state machine, rate
encoding, mode list, motor technology or completion evidence in the library.

## MotorControl-RS source findings

| Actual source | Reusable behavior | Assumption to remove or keep profile-specific |
| --- | --- | --- |
| `include/MotorControlRS/Units.h:13`, `:35`, `:76` | Separate command increments, full motor steps, encoder counts, turns/travel; explicit gearing, encoder basis/source/polarity and scale provenance | `STEPS` and `commandStepsPerMotorTurn` must be documented as command increments if retained. Full-step geometry is optional and cannot be required for a servo or linear axis |
| `include/MotorControlRS/Axis.h:34`, `:55`, `:68` | Exact target/reference, per-axis config generation, independent command and encoder origins, limits, fresh actual/commanded/queued basis | A common axis must identify a logical axis, not assume one motor per CAN node. Common math should not depend on an SDO node or protocol header |
| `include/MotorControlRS/ReadOperation.h:24` | Nonzero application ID plus immutable binding generation | `uint8_t address` is a wire endpoint embedded in common correlation. An arbitrary CAN protocol may need explicit frame IDs and axis selection; a CANopen node ID is not a universal CAN address |
| `include/MotorControlRS/MoveOperation.h:14` | Explicit finite position intent and setup policy | `uint16_t speedRpm` is a native ESS representation in a common request. Use an explicit rate quantity/unit/frame and profile conversion; do not force all devices into positive integer motor RPM |
| `include/MotorControlRS/VelocityOperation.h:14`, `:31` | Exact signed rate intent, finite duration and explicit stop policy | RPM-specific quantization allowances, `requestedRpm` and `int16_t nativeRpm` constrain every consumer. Native quantization belongs to the selected profile's rate representation |
| `src/Axis.cpp:597`, `:647`, `:656`; `test/velocity_test.cpp:351` | Checked conversion, atomic output and tested narrowing | `prepareVelocityTarget` currently enforces signed 16-bit RPM even though it lives in common `Axis.cpp`. Changing only the struct type would leave the implementation and tests restricted |
| `include/MotorControlRS/ActionOperation.h:10` | Explicit enable/release/fault-clear/stop and separate acceptance/completion | `StopBehavior::DIRECT` and release semantics are not universal stop, brake, torque-off or functional-safety semantics. `ActionEvent` carries RS frame/accepted-prefix evidence that must not be relabeled as CAN evidence |
| `include/MotorControlRS/Discovery.h:11`, `:24` | Static reviewed inventory and capabilities independent of live identity proof | Manufacturer/profile defaults are STEPPERONLINE/ESS; address range and register probe metadata are serial-profile assumptions. Inventory can remain fixed without making these fields universal |
| `include/MotorControlRS/profiles/ess_rs/Position.h:55`; `src/profiles/ess_rs/Position.cpp:199` | Explicit readiness, scale, sign, relative-basis and feedback qualification | ESS RPM limits, start-speed/ramp words, FC10 staging, serial-input prerequisites and feedback interpretation stay in the family profile |
| `include/MotorControlRS/profiles/ess_rs/Velocity.h:11`; `src/profiles/ess_rs/Velocity.cpp:98`, `:117` | Evidence-gated finite motion and stop servicing | Native RPM and negative two's-complement qualification are profile requirements, not requirements for every future encoder or velocity representation |
| `include/MotorControlRS/profiles/ess_rs/ControlSettings.h:2` | Distinguishes settings from motion operations | Its control/current settings explicitly are not commanded current or torque mode. Do not count existing current-limit APIs as servo torque control |

Additional important distinctions:

- `src/Axis.cpp:60` validates nonzero identity and generation; it does not impose
  a serial range on pure math. Preserve that separation. Endpoint range checks
  belong to protocol/profile bindings.
- `src/Units.cpp:97` validates optional scales and `:111` requires an identified
  encoder source when that scale is present. `test/axis_test.cpp:114` exercises
  a linear encoder basis and separate encoder origin. This is useful existing
  support, not a reason to require motor-turn geometry for every operation.
- Command-coordinate position, controller-reported actual position and raw
  encoder position remain distinct even when two happen to use the same
  numeric scaling. Preserve per-source origin, timestamp, generation and
  interpretation evidence; do not infer one from the other's name.
- `ReadOperation.h:13` supplies time and evidence age; operation contexts use
  explicit monotonic microseconds. `VelocityOperation.h:28` uses a finite
  `uint32_t durationUs`. Keep deliberate duration bounds or widen with checked
  arithmetic if scope requires; do not turn finite velocity into an implicit
  cyclic servo scheduler.
- `examples/common/RtuBusOwner.h:48`, `:76`, `:112` demonstrates immutable
  admission deadlines and reserved urgent result capacity. Reuse those ownership
  contracts without importing global RTU serialization or accepted-byte-prefix
  semantics into CAN frame submission.

## I2C conventions refreshed against actual source

ADS1115 `include/ADS1115/Status.h:39` and INA228
`include/INA228/Status.h:86` use the same status field shape but methods such as
`ok()`/`Status::Ok()` differ from RS's `isOk()`/free `Ok()`. Keep RS naming for
the new motion library rather than mixing incompatible helper vocabularies.

ADS1115 `include/ADS1115/ADS1115.h:225` specifies owner-safe
`bind/start*/poll/takeResult/unbind`; its example at
`examples/02_owner_safe_poll/main.cpp:328` grants one transfer per owner pass.
INA228's `include/INA228/INA228.h:305`, `:367`, `:386` separates binding,
budgeted work and exact result take. Its `HardwareState` at `:168` and passive
default `Config.h:193` separate synchronized device state from health policy.

These conventions support a multi-drive library through bounded work, external
serialization, typed failure, exact result retention and separate validity.
They do not justify a universal driver lifecycle, one common alarm enumeration,
or an automatic recovery policy for all motor technologies. Relevant existing
tests include INA228 `test/test_basic.cpp:5707`, `:5723`, `:5746` for bus-silent
binding, exact result identity and passive health. They were inspected, not run.

## FieldCore is a consumer example, not the common API ceiling

The current `include/TunnelMonitor/contracts/MotorControl.h:16` exposes Move,
ReadState and UrgentStop only. `MotorMove` at `:38` contains native `uint16_t`
RPM/ramp words; `MotorTarget` at `:51` carries a 1..247 serial address. Its
axis snapshot uses signed 32-bit native limits and one commissioned scale.
Those choices describe this consumer's selected contract, not every servo or
future CAN controller.

The useful cross-application boundary is retained in the same file: original
deadline and complete request identity (`:68`), independent admission/wire/
execution and effect fields, fresh position evidence and motion inhibition.
`src/devices/rs485_motor/Rs485MotorModule.cpp:171` copies native RPM into RS
requests; `:254` yields prepared work; `:640` takes retained results and `:684`
copies passive status. `include/TunnelMonitor/device/Rs485MotorModule.h:35`
makes the application owner responsible for physical transport settlement.

The module remains dormant: `docs/guidelines/rs485_devices.md:7` and `:19`
state that neither current product selects it. Its pinned host dependency is
MotorControl-RS v1.0.0, resolved to
`4125a092d91b4b6423d1664ee51fff209110006f` in
`scripts/prepare_rodtester_host_dependencies.py:12`. That differs from the live
RS HEAD audited above. Future adapters must audit their actual pinned API,
not assume the local newest headers are the consumed release.

No FieldCore changes are needed to prepare the new library for broader use.
Document that a future consumer adding servo/torque/cyclic operations must
extend its own command contract; do not constrain MotorControl-CAN to the
existing dormant three-operation RS application contract.

## Minimal recommended boundary

The following is a proposed responsibility split, not a requirement to create
empty directories or abstract base classes immediately:

```text
Common C++11 motion values / units / axis / status / observations
    no vendor, CANopen, platform, node-ID, statusword or native RPM dependency

CAN frame and application-owner transport boundary
    one receiver, copied frames, explicit standard/extended ID and length,
    actual TX/clock/error evidence, bounded queues and admission deadlines

Protocol services (CANopen first)
    CANopen endpoint/channel binding, SDO, NMT, PDO, heartbeat, EMCY;
    CiA402-specific modes/status projections only for profiles using them

Concrete drive profile (Lichuan C first)
    documented units/widths/object dictionary, state quirks, limits,
    concrete prepare*/next*/advance* contexts, supported native operations

Application composition / CLI / adapters
    fixed selected profiles, endpoint-to-axis mapping, scheduling,
    caches/health, platform CAN backend, machine policy and brake wiring
```

Recommended decisions before freezing the public API:

1. Separate logical axis identity and binding generation from a protocol route.
   Bind each logical axis to its reviewed immutable route in the application or
   concrete profile. CANopen node/channel/axis selection stays typed there.
   A future proprietary profile can use a different explicit route without
   changing pure axis arithmetic. Include physical interface identity in routing
   and correlation; identical frame IDs on separate buses are not one endpoint.
2. Express common speed intent as a quantity plus rate unit/frame. Retain
   `speedRpm` only as an explicitly named convenience/profile input where it is
   truly RPM. Native target bounds, signedness, scale and rounding are profile
   properties. Preserve integer/rational precision and reject values outside
   representable common or native ranges; do not promise arbitrary numeric
   formats merely by using `int64_t`.
3. Keep `FULL_STEPS` optional. Do not infer command counts from encoder
   resolution or vice versa. A native increment can mean a profile-defined
   command coordinate without physical stepper geometry. Retain source identity
   and confidence separately from motor technology labels.
4. Keep ordinary motion names shared, with capability-specific concrete
   contexts and arguments. Capabilities need operation/mode support, supported
   units and limits, prerequisites, stop outcomes and evidence level; a single
   `isServo` or `isStepper` boolean cannot supply those contracts.
5. Keep common health observational and profile/protocol state optional or
   explicitly tagged. A non-CANopen profile has no implied NMT, SDO abort,
   statusword, heartbeat or CiA402 state. Preserve its raw evidence alongside
   justified common projections; do not invent healthy defaults for absent
   fields.
6. Separate controlled stop, halt, drive disable, torque release and any brake
   command/feedback. Never equate release or zero velocity with a safe vertical
   axis or applied brake. Machine brake sequencing/wiring/interlocks stay
   application-owned unless a concrete reviewed profile exposes a typed brake
   function. Functional-safety claims require their own evidence and are not
   inferred from a software stop API.
7. Do not prematurely implement torque, current, cyclic synchronous control,
   interpolation or synchronized multi-axis scheduling. Leave common math,
   endpoint and owner contracts able to accept a later concrete operation
   module. Such support needs an actual controller source, unit/limit/mode
   contract, bounded owner service policy and tests. Do not add always-unsupported
   production methods solely to advertise future servo support.
8. For the present Classical CAN implementation keep standard and extended
   frame identity representable at the transport layer; the Lichuan profile can
   reject unreviewed formats. CAN FD support remains an explicit future
   transport/profile decision, not an accidental promise of a generic library
   name. Avoid forcing every future profile into CANopen's node range or frame
   allocation rules.
9. Keep static composition and a fixed inventory. A concrete new profile adds
   its own types, source ledger, codecs/contexts, tests, capabilities and CLI
   bindings. Do not add dynamic registration, universal operation bags,
   virtual-driver hierarchies, a second receive owner or a protocol switch in
   every common math function.

## Concrete portability proof to require in the promptset

Use test-only fixtures, not fake supported products in production:

- A servo-like coordinate/rate fixture with no full-step geometry, distinct
  encoder and command scale, signed native rates beyond the RS int16 RPM range,
  non-RPM native rate units and exact large positions. It must reuse common
  preparation without including Lichuan or CANopen headers.
- A non-CANopen routing fixture with an extended CAN identifier and two logical
  axes sharing one transport endpoint. It must exercise the common owner/frame
  boundary without NMT, SDO or a 1..127 node-ID prerequisite. This is a contract
  fixture, not proof of a vendor's real protocol.
- Two interfaces with the same frame IDs and logical nodes, proving route,
  generation, result and observation isolation; independent per-axis reservations
  with shared physical reception and explicit channel/resource conflicts.
- Optional capability tests: absence of homing, feedback, torque mode or a
  brake interface must be explicit. It must not fabricate successful status,
  silently weaken evidence requirements or block independent valid operations.
- Pure installed-header/consumer builds that include common values and CAN
  transport contracts without a Lichuan, CANopen stack, SDK or FieldCore
  dependency. Source and installed-package tests must both enforce this.
- Profile-native tests continue to use real reviewed Lichuan facts; the generic
  contract fixtures do not resolve the existing manual/EDS contradictions.

Reuse existing verifier registration patterns from MotorControl-RS
`CMakeLists.txt:61` and its isolated codec check at `:76`. Keep exact source
parity vectors, but rewrite RPM-width and serial-target vectors deliberately for
the new separation rather than weakening the old library's tests.

## Scope statement to carry forward

MotorControl-CAN is intended for extensible, framework-independent CAN motor
control across reviewed stepper and servo profiles. The first implementation
remains Lichuan C over CANopen. Architecture and test-only extension proofs may
be completed before a second real controller is selected; claiming supported
servo hardware, proprietary CAN, torque modes or additional manufacturers
requires their own sources, implementation and validation. This distinction
prepares the library without replacing concrete delivery with a speculative
framework.
