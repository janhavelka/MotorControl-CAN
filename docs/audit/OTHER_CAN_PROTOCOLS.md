# Non-CANopen motor protocols: architecture contrasts

Research date: **2026-10-23**, Europe/Prague. Primary sources only. This report
defines expansion requirements for MotorControl-CAN; it does not claim that
any profile is implemented or hardware-qualified. Downloaded originals and
hashes are in [other_can/SOURCES.json](../reference/other_can/SOURCES.json).

## Decision

The library can serve multiple stepper and servo families if **CAN is the
transport boundary and CANopen is one protocol family**. Common motion intent,
units, ownership, observation provenance and outcome vocabulary can remain
portable. Wire codecs, discovery, device state machines, trajectories, stopping,
watchdogs and native settings must be protocol/profile-specific. This is a
design conclusion from the contrasts below, not a vendor compatibility claim.

Use CL86-C as the first real device profile, an ODrive CANSimple profile as the
first concrete non-CANopen extension, and moteus/VESC as mandatory architecture
test cases. Bring their implementations into the release only with explicit
firmware/model scope, complete declared feature slices and their own tests.
Do not describe a broad list of planned profiles as supported products.

## Source identity and scope

| Family examined | Exact primary baseline | Applicability limit |
| --- | --- | --- |
| ODrive CANSimple | Official **0.6.12** protocol documentation, versioned URL, retrieved bytes hashed | Pro/S1/Micro features described by this documentation; older firmware/legacy boards need their own dialect |
| mjbots moteus register protocol | Official repository commit `8f747f4ac448d117c1390b77ca2d80c27e751a56`; protocol/register/control/timeout docs plus official client schema/transport | Protocol source baseline, not an assertion that every controller board runs that firmware/register map |
| VESC native CAN mode | Official `vedderb/bldc` commit `abc9217fb2b7ee3821f1742bc32b269173643319`; version macros declare **7.01** | Source snapshot, not a verified release tag or every VESC-compatible hardware/firmware fork; other CAN modes are separate protocols |

Links: [ODrive protocol 0.6.12](https://docs.odriverobotics.com/v/0.6.12/manual/can-protocol.html),
[moteus pinned repository](https://github.com/mjbots/moteus/tree/8f747f4ac448d117c1390b77ca2d80c27e751a56),
[VESC pinned version declarations](https://github.com/vedderb/bldc/blob/abc9217fb2b7ee3821f1742bc32b269173643319/conf_general.h).

No purchased ODrive, moteus or VESC board, motor, settings, encoder, pole count,
gear ratio or flashed image was inspected. CubeMars/MyActuator and Trinamic
variants remain additional candidates; this report makes no compatibility or
protocol assertions for them without a separately pinned manual and model.

## Wire and command comparison

| Concern | ODrive CANSimple | moteus | VESC native CAN |
| --- | --- | --- | --- |
| Identifier | Standard 11-bit command/node encoding | Source/destination/reply-request addressing; extended IDs used when required | Extended 29-bit; command in upper portion, destination/controller ID in low byte |
| Frame family | Classical CAN; Pro/S1/Micro also documented FD support with firmware gates, still at most 8 payload bytes | CAN-FD multiplex frames; larger payloads and subframes | Reviewed native implementation uses Classical CAN data frames, up to 8 bytes |
| Polling | RTR supported in Classical CAN; empty data request alternative for FD | Explicit read subframes/reply-request bit; no RTR assumption | Periodic configured status packets and protocol-specific ping/forwarded commands |
| Byte representation | Little-endian, IEEE binary32 and scaled integers | Little-endian; typed integer/binary32 register subframes and bounded varuint | Big-endian signed/scaled integers in simple commands |
| State model | ODrive axis/controller/input modes | moteus mode/fault/reference model | VESC motor configuration/control modes and app timeout |
| CANopen SDO/NMT/CiA402 | Not those services; even `RxSdo`/`TxSdo` are CANSimple endpoint messages | Not CANopen objects or SDO | Not CANopen; UAVCAN/bridge modes are additionally distinct |

Evidence: [ODrive protocol](https://docs.odriverobotics.com/v/0.6.12/manual/can-protocol.html),
[moteus CAN format](https://github.com/mjbots/moteus/blob/8f747f4ac448d117c1390b77ca2d80c27e751a56/docs/protocol/can.md),
[moteus frame construction](https://github.com/mjbots/moteus/blob/8f747f4ac448d117c1390b77ca2d80c27e751a56/lib/python/moteus/transport.py#L330),
[VESC wire documentation](https://github.com/vedderb/bldc/blob/abc9217fb2b7ee3821f1742bc32b269173643319/documentation/comm_can.md),
[VESC transmit implementation](https://github.com/vedderb/bldc/blob/abc9217fb2b7ee3821f1742bc32b269173643319/comm/comm_can.c#L300).

### ODrive: suitable first non-CANopen extension

Position/velocity/torque commands use revolutions, revolutions/second and N·m;
position-command feedforward integer scales are configurable. Heartbeat carries
axis state, error/procedure information and trajectory-done. Enabled watchdog
feeding is tied to designated setpoint commands and entry into closed-loop
control. CANSimple FD arrived in 0.6.10; the documentation recommends at least
0.6.12 for FD operation. FD has no RTR; requests use the documented data-frame
alternative. [ODrive 0.6.12 protocol sections](https://docs.odriverobotics.com/v/0.6.12/manual/can-protocol.html)

Implementation implications:

- Pin firmware/protocol/hardware identity and relevant input-mode configuration.
  Do not apply a generic CiA402 controlword or Lichuan mode number.
- Provide explicit pure profile builders and typed observations using the RS
  `prepare.../next.../advance...` pattern. A setpoint submission need not have a
  write acknowledgement; represent that fact instead of inventing SDO success.
- Establish the move's trajectory mode, target, relevant fresh feedback and
  post-submission completion evidence. A retained `trajectory_done` value is
  insufficient to attribute completion to a new request.
- Preserve feedforward scale provenance and float-conversion error; the exact
  common target does not become exactly representable merely because it was
  encoded as binary32.
- Keep watchdog service separate from host deadlines and node liveness. A
  periodic heartbeat from the drive does not mean the host is feeding its
  command watchdog.
- Review ID allocation before combining CANSimple and CANopen on one bus;
  separate protocols can occupy conflicting CAN identifiers. FD capability also
  depends on every bus participant, not just this library's frame type.

These are design/verification requirements inferred from the documented
protocol, not claims of observed drive behavior.

### moteus: exercises FD, rich setpoints and completion semantics

The register protocol combines multiple typed read/write/reply subframes in a
CAN-FD payload. Integer encodings have quantity-specific scales and special
sentinels; floating encodings use binary32. The protocol documentation describes
1 Mbit/s nominal and 5 Mbit/s data timing; actual application timing remains
explicit configuration. [moteus pinned CAN specification](https://github.com/mjbots/moteus/blob/8f747f4ac448d117c1390b77ca2d80c27e751a56/docs/protocol/can.md)

**Source conflict:** that specification says an ID above `0x7fff` requires an
extended frame. The same revision's official client actually sets extended-ID
format above **`0x7ff`**, and sets FD frames. The client condition matches the
11-bit standard identifier range. Preserve the original document and record the
prose discrepancy; do not implement the erroneous threshold.
[Official frame construction](https://github.com/mjbots/moteus/blob/8f747f4ac448d117c1390b77ca2d80c27e751a56/lib/python/moteus/transport.py#L330)

Position/velocity/torque observations refer to the configured output coordinate;
units include revolutions, revolutions/second and N·m. `trajectory_complete`
means the limited trajectory ended and the controller follows the final
velocity, which can be nonzero. Register-map identity and model/firmware reads
are available in the register surface. [Register reference](https://github.com/mjbots/moteus/blob/8f747f4ac448d117c1390b77ca2d80c27e751a56/docs/protocol/registers.md)

The main control mode combines target position/velocity, gains, torque limit
and feedforward torque; finite-position completion therefore requires an
explicit final-velocity policy. Velocity and torque behavior are expressed
through this configured control mechanism rather than a CiA402 mode transition.
[Control modes](https://github.com/mjbots/moteus/blob/8f747f4ac448d117c1390b77ca2d80c27e751a56/docs/guides/control-modes.md)

When command watchdog service expires, timeout mode inhibits ordinary resumed
commands until a stop command clears that state. Diagnostic commands can use
different timeout defaults from efficient register commands. A prototype using
the diagnostic console is therefore not evidence of production watchdog
behavior. [Timeout-mode guide](https://github.com/mjbots/moteus/blob/8f747f4ac448d117c1390b77ca2d80c27e751a56/docs/troubleshooting/timeout-mode-11.md)

Implementation implications:

- Require an FD-capable adapter for this profile. Do not fragment an arbitrary
  moteus FD command into Classical CAN frames and claim equivalent behavior.
- Model frame flags/length, subframe bounds, padding, varuint limits, response
  addressing and unknown register/error handling explicitly.
- Keep intentional sentinel values typed. A generic API should reject ordinary
  NaN quantities but still allow a named, documented native option that encodes
  a special sentinel; arbitrary NaN acceptance is not the solution.
- Distinguish a trajectory's internal completion, final velocity, measured
  position tolerance and confirmed stationary state. Public `stop` must have
  its own mode/feedback contract.
- Preserve configured output/reference provenance and active torque/gain/limit
  state. Do not silently reuse motor-shaft conversions from another profile.
- Use the upstream schema as protocol evidence, not an unreviewed dependency
  on its asynchronous transport/runtime. Keep the library's fixed-storage and
  caller-owned execution boundary.

### VESC: exercises extended IDs, electrical speed and current control

The native protocol's simple commands cover duty, motor/braking current, speed
and PID position. Position is documented as an angle in the 0–360° domain,
not an unlimited signed multi-turn destination. Six configured status packet
groups carry different telemetry, including ERPM, current and PID position.
Commands outside configured limits may be clamped by firmware.
[VESC protocol documentation](https://github.com/vedderb/bldc/blob/abc9217fb2b7ee3821f1742bc32b269173643319/documentation/comm_can.md)

The actual command handler feeds timeout on recognized motor setpoint commands,
accepts a broadcast destination and dispatches native motor setters. Its
transmitter explicitly uses extended data frames. The basic setter path does
not publish a correlated motion-complete response.
[Pinned command implementation](https://github.com/vedderb/bldc/blob/abc9217fb2b7ee3821f1742bc32b269173643319/comm/comm_can.c#L1607)

Speed feedback is electrical RPM; the motor interface converts it using the
configured motor pole count when calculating mechanical quantities. Current
commands are amperes, not torque. Mechanical RPM/torque APIs require explicit
configuration and conversion evidence rather than a renamed native value.
[VESC motor interface](https://github.com/vedderb/bldc/blob/abc9217fb2b7ee3821f1742bc32b269173643319/motor/mc_interface.c#L1620)

The timeout implementation applies configured braking current and maintains
its own timeout state. The broad documentation phrase about no CAN traffic
must not be read as proof that unrelated status traffic feeds the motor
watchdog. Inspect the effective application configuration and recognized
command paths; do not hard-code a universal timeout from a prose default.
[Timeout implementation](https://github.com/vedderb/bldc/blob/abc9217fb2b7ee3821f1742bc32b269173643319/timeout.c#L240)

Implementation implications:

- Begin with a small explicit native mode: checked current/velocity setters,
  raw identity/status where documented, and bounded host service/stop policy.
  Declare that implemented subset separately from the full firmware protocol.
- Do not emulate arbitrary multi-turn absolute/relative moves with repeated
  angle setpoints without a separately specified trajectory/feedback algorithm.
  Reject unsupported common motion mappings before TX.
- Preserve requested versus configured/observed effective values. Where the
  firmware clamps and no correlated acceptance value exists, the library
  cannot report exact applied output merely from its own transmitted bytes.
- Classify periodic setpoint refresh separately from discrete command replay.
  Publishing a timeout result must not leave a background setpoint refresh alive.
- Treat ping, configured cyclic telemetry, multi-frame command forwarding,
  application modes and native firmware updates as separate documented
  capabilities. Do not run write-based discovery or broadcast motion.
- VESC source files carry GPL notices. They are retained for protocol research,
  not copied into a new MIT implementation. Preserve notices and audit any
  proposed dependency/code reuse separately.

## Required architecture consequences

These requirements are this audit's synthesis. They deliberately do not force
every protocol into one identical engine or claim every device has every
operation.

### Common frame and owner boundary

The portable CAN description must retain the arbitration ID, standard/extended
format, Classical/FD format, RTR where legal, FD bitrate-switch state, payload
length and bytes. Support bounded storage up to the selected frame format's
capacity; do not confuse FD DLC encoding with byte count. Adapter capabilities
must reject unsupported formats before TX. A Classical-only example remains a
valid consumer but cannot qualify an FD profile.

One application owner receives every frame and routes it to the selected
protocol instance/profile by explicit routing metadata. A CANopen stack owns
CANopen service state only; other protocols do not have to instantiate it.
Keep all vendor traffic flowing through that single controller owner. Mixed
protocol buses require a documented identifier-allocation/conflict policy and
compatible physical bitrate/frame capabilities.

Bind an endpoint using protocol/profile, interface/bus, node/axis identity,
firmware/model selection and generation. A bare integer node address or a
CiA402 stateword is insufficient common identity. Discovery is protocol-specific
and defaults to explicit reviewed candidates; arbitrary frame guessing is not
a universal harmless scan.

### Portable intent without false equivalence

Keep RS exact numbers, coordinate frames, provenance, rounding, host origin,
deadline and retained outcome vocabulary. Add real torque/current/control
capabilities only when implemented; a torque request in N·m, motor current in
A, duty ratio, electrical RPM and mechanical velocity are distinct types.
Torque conversion needs appropriate motor configuration, calibration and frame
semantics; do not infer it from the presence of a current setter.

Public capability queries should distinguish:

- Finite onboard trajectory versus continuously refreshed setpoint versus
  an application-generated trajectory.
- Absolute multi-turn coordinates, wrapped angle, relative command and
  velocity-only control.
- Position, velocity, torque, current, duty and native gain/feedforward modes.
- Setpoint reception, acknowledged command, internal trajectory completion,
  measured arrival and stationary evidence.
- Drive watchdog support, feed mechanism, expiry reaction, and rearm policy.
- Hardware/profile maximum frame type, service period and required feedback.

Profile contexts retain native state/error fields; common health projections
report only facts actually established. NMT, SDO abort, EMCY, PDO map and
CiA402 mode/status fields remain CANopen-specific extensions. Other profiles
have their own raw/native evidence, not fabricated CANopen values.

### Deadline and refresh policy

"Never automatically replay uncertain motion" remains mandatory for discrete
side effects. It must not accidentally prohibit a documented continuous
setpoint mode that requires refreshing under caller authorization. Represent
the difference explicitly: refresh period/deadline, authorized setpoint,
maximum duration, expiry stop/rearm policy and callback/resource bounds.
The application reserves service capacity and executes yielded work; the core
does not create a background task.

After cancellation, failure, target rebind, reboot or bus recovery, retire all
corresponding queued and future refresh work. A new session requires explicit
intent and renewed prerequisites. Controller retransmission and queued-frame
settlement remain separate from application replay. A watchdog is neither a
guaranteed physical stop nor proof of standstill.

### Tests and naming

Preserve `Status`, `Err`, pure `build.../parse.../validate...`, concrete
`prepare.../next.../advance...`, typed observation getters and context outcome
members. Do not make all codecs accept an object-index/subindex or all
operations pass through `advanceSdo`.

Use a small real codec/fixture from each selected contrast before freezing the
common API. Tests should expose standard/extended ID selection, FD flag/length
validation, RTR restrictions, byte-order differences, float/sentinel handling,
electrical/mechanical scale rejection, watchdog starvation, nonzero final
velocity, stale completion bits, unsolicited telemetry, routing collisions,
unknown command effect and no post-cancel refresh. Each profile's CLI and Python
automation must call that same production API.

## Inclusion order and acceptance

| Stage | Inclusion recommendation | Completion gate |
| --- | --- | --- |
| Foundation | Protocol-neutral CAN frames/owner hooks, units/intent/evidence; compare all three protocols now | No mandatory CANopen types in common surface; bounded tests demonstrate every claimed format |
| First device | CL86-C and documented related Lichuan variants | Existing primary manual/EDS gates, full declared typed surface and device-specific qualification |
| First independent extension | ODrive CANSimple on an explicitly selected hardware/firmware dialect | Real builders/parsers, observations, configured trajectory/watchdog policy, CLI/runner parity; never CANopen emulation |
| FD extension | moteus on an explicitly matched model/firmware/register map | FD-capable adapter and whole-profile service/timeout/units/trajectory contract; discrepancy resolved from source |
| Other native CAN | VESC native CAN mode, starting with a clearly declared subset | Electrical/mechanical units and current semantics explicit; bounded stream lifecycle; no unsupported multi-turn claims |
| Further families | CubeMars/MyActuator, Trinamic native/CANopen variants or another requested controller | Acquire and audit exact primary model/firmware documents first; distinguish dialects and firmware modes |

This order is a recommendation for implementation milestones, not a reason to
encode CANopen restrictions in the foundation or silently remove the user's
general multi-controller objective. Native host implementation and tests can
proceed without hardware; qualification remains independently pending.

## Recurring local-source audit and verification

Read current MotorControl-CAN `AGENTS.md` and refreshed the affected common
intent/status/owner conventions. Recorded live repositories:

| Checkout | HEAD | Working state observed |
| --- | --- | --- |
| MotorControl-RS | `5da4ba7f13be457ea7bf9706ffd7d546821669c7` | **Dirty**: documentation/agent guidance edits and new standalone prompts/reference script; preserved untouched |
| ADS1115 | `381236ebfed813cd7a9aca34520ce031d8a0ff6b` | Clean |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Clean |
| FieldCore-node | `da94ed5280ed42ee3a55e0d977f9cb6565c56b51` | Clean |

Inspected `MotorControlRS/ActionOperation.h`, current CAN naming plan,
ADS1115/INA228 `Status.h` and `lastError` accessors, their CLI check/test runners
and CI sources, plus FieldCore `contracts/MotorControl.h` and
`scripts/check_motor_runtime.py`. FieldCore separates admission, wire settlement,
execution, stopped evidence and write effect; its current RS RPM/native-ramp
types are consumer-specific and must not constrain non-RS protocols. I2C status
helpers also differ from the RS API; preserve ownership and naming conventions
without casting their enums or importing sensor retry behavior.

This live dirty RS state is later than the earlier clean baseline audit. It
does not invalidate that earlier recorded check, and that earlier check does
not verify the current dirty documents. During this research, independent work
advanced the RS checkout to `a1f70d0bf91c7c33fd83909d7b9d21b4a777c381`
(`Audit standalone ESS backlog and add completion promptset`); the final status
also contained new untracked Lichuan RS485 reference files. The table records
the initial observed state rather than silently substituting this later
revision. No sibling files were edited or committed by this research task.

Validation for this documentation task: HTTP success and original-byte
SHA-256/length manifests for retained sources; inspected the actual code paths
behind ambiguous prose; local document links, manifest paths and source
integrity checked. No third-party downloaded code was executed. No library
implementation, native firmware test, hardware action, purchase, vendor contact
or remote CI was performed or claimed.
