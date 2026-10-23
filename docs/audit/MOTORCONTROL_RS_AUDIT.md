# MotorControl-RS source audit for MotorControl-CAN

Audit date: 2026-10-23 (Europe/Prague). This is a source-backed migration and
architecture audit, not CL86-C protocol documentation or hardware qualification.
All source references below are relative to the inspected MotorControl-RS root.
Line numbers identify the snapshot and must be rechecked when implementing.

## 1. Baseline and result

| Item | Observed value |
| --- | --- |
| Source checkout | `C:/Users/Honza/Documents/Projects/MotorControl-RS` |
| HEAD | `5da4ba7f13be457ea7bf9706ffd7d546821669c7` |
| Latest commit | 2026-10-19, `Use remote CI for major changes and plan work against CI` |
| Package / namespace / CMake identity | `MotorControl-RS` / `MotorControlRS` / `MotorControlRS` |
| Package version | `1.0.0` in `library.json` |
| Working tree | Clean before and after inspection and verification |
| Source changes made | None; no fetch, checkout, commit, push, firmware flash or hardware operation |
| Instructions read | Parent `Projects/AGENTS.md`; source `MotorControl-RS/AGENTS.md` |
| Verification | Source's complete **quick** verifier passed; 78 registered tests plus clean source and installed consumers |

The source is an appropriate behavioral and repository reference. Preserve its
pure API, explicit motion vocabulary, caller ownership, retained evidence,
bounded execution, console/API parity, verification and document provenance.
Do not mechanically rename the implementation: the transport, several nominally
common types, examples, discovery and console payloads contain ESS/RTU-specific
assumptions. The source itself explicitly assigns CANopen to a separate future
library, with a common motion vocabulary and independent implementations, and
rejects premature universal transport/framework extraction (`AGENTS.md`,
Current scope and Intended structure sections).

The audit includes the actual public headers, core implementation seams,
transaction owner/runner, application/cache/console seams, Python tools,
tests, CI, packaging, maintained contracts, release scope and source ledgers.
It does not claim a line-by-line formal proof of the entire repository. The
current sibling-library and FieldCore inspection is a separate workstream;
historical FieldCore claims in this repository are not current integration proof.

## 2. Repository anatomy to preserve

Source code inventory at this snapshot, counting `.h`, `.cpp` and `.py` files:

| Area | Files | Lines | Role |
| --- | ---: | ---: | --- |
| `include/` | 28 | 3,152 | Installed public API |
| `src/` | 23 | 6,068 | Pure common and profile implementations |
| `examples/` | 48 | 13,429 | Application, owner, adapters and CLI |
| `scripts/` | 13 | 7,563 | Verification, provenance and finite bench tools |
| `test/` | 100 | 32,239 | Native/Python fixtures and platform/package consumers |

The actual test directory is **`test/`**, not `tests/`. There is no `tools/`
directory and no `library.properties` in this snapshot.

```text
MotorControl-RS/
  AGENTS.md
  README.md  CHANGELOG.md  LICENSE
  library.json  CMakeLists.txt  platformio.ini
  .github/workflows/verify.yml
  boards/
  include/MotorControlRS/
    MotorControlRS.h  Status.h  Version.h
    Units.h  Axis.h  ReadOperation.h  ActionOperation.h
    MoveOperation.h  VelocityOperation.h  Discovery.h  Traffic.h
    profiles/ess_rs/
      Codec.h  Registers.h  Types.h  Defaults.h
      Reads.h  Discovery.h  Actions.h  Position.h  Velocity.h  Homing.h
      DriverSettings.h  Segments.h  ControlSettings.h  Tuning.h
      Communication.h  Persistence.h  Traffic.h
  src/
    Axis.cpp  Units.cpp  UnitFactors.h  Discovery.cpp  Traffic.cpp
    rtu/Frame.h
    profiles/ess_rs/ (matching implementations, private Access.h/ActionEvidence.h)
  examples/
    units_preview/main.cpp
    common/ (runner, owner, validator, board/config and platform transport)
    probe_cli/ (shared application, console and operation helpers)
    probe_idf/ (native ESP-IDF startup, component and Kconfig)
  test/ (native tests, Python tests, fakes, installed/IDF/portability consumers)
  scripts/ (verify, generators, checks, finite bench tools, reference preparation)
  docs/
    architecture and operation contracts; onboarding and source map
    reference/ (source manifests, machine-readable ledgers, generated catalogues)
    vendor/  standards/ (original downloaded files)
    reports/  releases/  archive/
```

Evidence: `docs/source_map.md:12`, `CMakeLists.txt:3`, `library.json:1`,
`platformio.ini:1`, and the filesystem inventory. Follow this *separation of
responsibilities* for MotorControl-CAN; add only files justified by its actual
documented protocol. Do not add empty ESS-named categories to imitate the tree.

## 3. Installed core and ownership

The library has three reusable concerns: common units/intent, profile-specific
wire codecs and typed interpretation, and bounded caller-owned operations.
There is no hardware-owning singleton, scheduler, Arduino dependency, ESP-IDF
dependency, background worker, logger, internal clock or automatic retry in the
core. The common umbrella includes common types; callers explicitly include
profile headers (`include/MotorControlRS/MotorControlRS.h:1`).

| Responsibility | Owner and evidence | MotorControl-CAN consequence |
| --- | --- | --- |
| Protocol frame construction and interpretation | Profile free functions; `profiles/ess_rs/Codec.h:27` | Pure CAN frame/SDO/PDO/NMT/heartbeat/EMCY interpretation belongs in reviewed protocol/profile modules; no driver calls |
| Units, coordinate origin and target arithmetic | `Axis.h:27`, `Units.h:63`; caller supplies configurations/evidence | Preserve exact quantities, explicit scale provenance, target/config generations and coordinate policies |
| Sequence state | Fixed `ReadContext`, `ActionContext`, `MoveContext`, etc.; `Reads.h:33`, `Actions.h:33`, `Position.h:84` | Caller owns concrete contexts; operations yield work and consume supplied time/events |
| Time, bus arbitration and execution | Application owner; `examples/common/RtuBusOwner.h:129` | Application owns a CAN controller/router and scheduling; keep platform adapters outside installed core |
| Buffers | Caller supplied or bounded context arrays; codec retains no pointer; `Codec.h:69` | Validate capacities and lifetimes; copy required asynchronous evidence before borrowed receive memory expires |
| Caches and freshness | `examples/probe_cli/StateCache.h:7` | Keep identity, communications, configuration, drive state and last attempts distinct |
| Health/recovery policy | Application; `ProbeConsole.cpp:2018` | No core device lifecycle or generic sensor retry policy masquerading as CAN motion health |
| Console and Python tools | Consumers of the same public operations | No hidden register/object behavior in parsing, scripts or debug paths |
| MCU and memory placement | Examples/adapters | No PSRAM, FreeRTOS or ESP32 requirement for consuming the core |

In-place placement `new` occurs in `DriverSettings.cpp:211` and
`Persistence.cpp:104` to reconstruct caller storage. This is not heap
allocation; a simplistic banned-token check for `new` would report a false
violation. Check actual ownership and allocation behavior.

## 4. Exact API vocabulary and error semantics

### 4.1 Status

`include/MotorControlRS/Status.h:12` defines `enum class Err : uint8_t` with
`OK`, `INVALID_CONFIG`, `ILLEGAL_VALUE`, `UNSUPPORTED`, `CRC_ERROR`,
`FRAME_ERROR`, `EXCEPTION`. `Status` is `{ Err code; int32_t detail;
const char* msg; }`, with `isOk()`, explicit boolean conversion, namespace
`Ok()`, and `errToString()`. Messages have static lifetime.

Copy this shape and naming. Do not cast sibling error enums numerically.
Select CAN-specific error meaning deliberately: a controller error, malformed
CANopen message, SDO abort and decoded drive alarm are different facts. Preserve
raw unknown protocol error values in typed evidence; an SDO abort field must
not accidentally lose unsigned bits through `int32_t detail`. Do not promise
software CRC calculation or use Modbus exception labels merely to match RS.

The distinction between validation result and operation outcome is essential:
`advanceRead(...)` returning `Ok()` means the correlated event was consumed,
even when it terminalized a timeout or failure. Inspect context state/outcome/
status to determine the operation result (`Reads.h:193`).

### 4.2 Pure functions and unchanged outputs

| RS symbols | Exact contract/evidence | Recommended treatment |
| --- | --- | --- |
| `buildReadRegisters`, `buildWriteSingleRegister`, `buildWriteMultipleRegisters` | Bytes written; zero on rejection; output unchanged; `Codec.h:69` | Preserve `build...` vocabulary and atomic publication, but use object/SDO/PDO names where those are the real operation |
| `validateReadRegistersRequest`, `validateWrite...Request` | Pure admission checks; `Codec.h:34` | Keep validation callable separately from construction |
| `validateReadResponseExpected`, `parseRegisters`, `parseRegister` | Mandatory request expectations; exact lengths; unchanged payload; count resets zero; `Codec.h:90` | Validate route/node, command, object/subindex, width, transfer state and all other available correlation evidence |
| `parseWriteSingleRegister`, `parseWriteMultipleRegisters` | Checked acknowledgement is distinct from effect; `Codec.h:119` | Explicitly separate transfer acknowledgement, drive-state acceptance and motion completion |
| `buildProbe`, `parseProbe` | Minimal non-changing query, responder-only success; `Codec.h:128` | Verify a minimal CL86-C query and honest identity confidence; no universal guessed probe |
| `encodeUint32`, `decodeUint32`, `encodeInt32`, `decodeInt32` | Checked width/order; no retained pointers; `Codec.h:141` | Reuse validation style, not ESS register-pair layout or caller-selectable word order if CAN objects specify a fixed layout |
| `readWord`, `checkReply`, `writeHeader` | Small private helper vocabulary; `src/profiles/ess_rs/Codec.cpp:44`, `src/rtu/Frame.h` | Prefer short concrete helpers over generic frameworks or synonym-heavy aliases |

Do not preserve a function name whose protocol meaning changes silently. Record
necessary deviations in a public API mapping. Retain shared high-level names
where semantics truly match.

### 4.3 Common quantities and coordinate API

`Units.h:13` defines position/time units, scale provenance (`UNKNOWN`,
`ASSUMED`, `DOCUMENTED`, `READBACK`, `QUALIFIED`), encoder basis, unit config
and conversions. Public helpers include:

- `validateUnitConfig`, `convertDisplacement`, `convertVelocity`,
  `convertAcceleration`, `validateNativePosition`, `narrowNativePosition`
  (`Units.h:107`).
- `parseExactNumber`, `rationalToDouble`, `validateAxisConfig`, `configureAxis`,
  `setAxisOrigin`, `invalidateAxisReference`, `preparePosition` (`Axis.h:111`).

`Rational` retains signed 64-bit numerator/unsigned denominator; native
integers do not pass through floating point. `CoordinateFrame` is
`NATIVE/MOTOR/LOAD`; `RelativeBasis` is `ACTUAL/COMMANDED/QUEUED`;
`AnglePath` is `POSITIVE/NEGATIVE/SHORTEST`; half-turn ties and rounding are
explicit. The exact path checks overflow, scale/origin availability,
configuration generation, reference age and limits. Radians require explicit
approximation/error allowance (`Axis.h:14`, `Axis.h:72`, `Axis.h:131`).

Preserve distinctions between command increments, encoder counts, motor/load
coordinates, given/actual position and host/device zero. Do not infer an origin
from a successful conversion. Do not silently clamp or switch the requested
wrapped-angle path to fit a limit.

### 4.4 Typed operations and naming

| Function family | Context and behavior | Evidence |
| --- | --- | --- |
| `prepareIdentity`, `prepareConfig`, `prepareState`; `nextRead`, `advanceRead`; `getIdentity`, `getConfig`, `getStateBlock` | Identity/config publish only whole success; state publishes successful blocks independently | `Reads.h:176` |
| `prepareEnable`, `prepareRelease`, `prepareClearAlarm`, `prepareSetDevicePosition`, `prepareStop`, `prepareNormalStop`, `prepareEmergencyStop`; `nextAction`, `advanceAction` | Explicit effects and stop policy; zero-only position clear in ESS | `Actions.h:71` |
| `prepareMoveRelative`, `prepareMoveAbsolute`, `prepareMoveAngle`; `nextMove`, `advanceMove` | Full setup/readback/trigger/observation phases with finite deadline | `Position.h:167` |
| `prepareVelocityTarget`; `prepareVelocity`, `serviceVelocity`, `nextVelocity`, `advanceVelocity` | Finite host policy and separate service obligation | `VelocityOperation.h:49`, `profiles/ess_rs/Velocity.h:66` |
| `prepareHome`, `nextHome`, `advanceHome`, `getHomeReference` | Method-specific prerequisites, new completion evidence and reference publication | `Homing.h:112` |
| `prepareDriverRead`, `prepareDriverSettings`, `nextDriver`, `advanceDriver`, `getDriver`, `getIo` | Full candidate checked before mutation; typed native access | `DriverSettings.h:224` |
| `prepareCommunication`, `prepareCommunicationConfirmation`, `nextCommunication`, `advanceCommunication` | Explicit commissioning, original/requested endpoint evidence | `Communication.h:95` |
| `prepareSave`, `prepareFactoryRestore`, `nextPersistence`, `advancePersistence` | Save/restore acknowledged, read back and restart durability recorded separately | `Persistence.h:104` |

Common forwarding overloads use the same concrete profile contexts
(`Actions.h:107`, `Position.h:203`). There is **no generic `Sequence.h` or
`Commands.h` engine** to import. Repeated `next...` yields the same work/token
until an event is advanced; the caller prevents duplicate admission.

There are also **no installed `getReadResult`, `getActionResult`,
`getMoveResult`, `getVelocityResult` or `getHomeResult` functions**. Read typed
observations use the getters named above; action/motion callers inspect their
concrete context's `state`, `outcome`, `status`, `execution`, `completion` and
related evidence members. Do not invent getter names and label them exact RS
parity. `BusOwner::result` and example `moveProgress` are different, application
level inspection surfaces.

Not every RS operation maps to a CAN drive. An unsupported/unresolved behavior
must fail before yielding traffic or remain an explicit coverage gap. Never
invent a position-clear primitive, instant/emergency stop behavior, homing
method, mode or live-speed update solely to make the CLI look identical.

## 5. Evidence, operation lifecycle and uncertainty

`ReadTarget` carries application identity, wire address and binding generation
(`ReadOperation.h:24`). Operations also carry a nonzero operation ID and step.
`ReadEvent` carries frame memory borrowed only for advancement, qualified
observation bounds, transport detail, accepted TX prefix and execution
uncertainty (`ReadOperation.h:58`). `ActionEvent` adds response-confirmed and
physical-TX-complete evidence (`ActionOperation.h:45`).

`ActionExecution` distinguishes `NOT_TRANSMITTED`, `ACKNOWLEDGED`, `REJECTED`,
`UNKNOWN`; `ActionCompletion` distinguishes `NOT_OBSERVED/OBSERVED`;
`ActionOutcome` separately records terminal reason (`ActionOperation.h:52`).
`ActionWork` is `TRANSACTION/WAIT/DONE` (`profiles/ess_rs/Actions.h:13`). These
are independent dimensions, not several spellings of one success boolean.

The inspected implementation enforces those distinctions. `Position.cpp:44`
retains uncertainty after staging/trigger transmission; `Position.cpp:59`
classifies execution from qualified response evidence. `Codec.cpp:44`
validates exact reply form before publishing; `Reads.cpp` builds a local typed
observation then assigns the caller output after successful parsing.

Preserve these invariants in CAN:

1. Accepted by a host TX queue, physical controller transmission, a protocol
   acknowledgement, drive state transition and completed motion are separate.
2. Repeated frames or cached target-reached bits cannot establish completion of
   a newly submitted operation without a documented operation-specific rule.
3. A timeout after a possible write leaves execution unknown. Recovery,
   reconnection, reset, rebind, startup and script cleanup must not replay it.
4. Cancellation cancels local work/waiting; it is not a physical stop. An
   explicit stop has its own transmission, settlement and observation outcome.
5. A failed/uncertain setup must not yield a start trigger. Retain partial
   configuration and original target/generation provenance.
6. Invalid event envelopes/correlation leave contexts unchanged. Valid failure
   events do update contexts and retain their failure, rather than pretending
   no call occurred.
7. Operation deadlines do not renew on polling or retry. Timing observations
   must preserve their real bounds when delivered later.
8. Drive boot-up, NMT state, heartbeat, controller state, EMCY, configuration
   generation and identity need separate validity/invalidation rules based on
   the reviewed CANopen profile, not an RS blanket cache copy.

These are design obligations for the new implementation; exact CANopen wire
semantics must be established from its own acquired primary references.

## 6. Transport owner and adapter design

The RS example provides a useful ownership model, not a reusable CAN transport.

`RtuRunner.h:12` defines a one-transaction phase machine
`IDLE/WAIT_BUS/SETUP/DRAIN/HOLD/RECEIVE/DONE/FAULT`. It consumes bounded
callbacks, physical TX completion, receive intervals/watermarks, direction
control, RTU gaps and explicit echo policy. It is deliberately outside the
installed package. Its 256-byte frames, byte-stream collection, 1.5/3.5-character
gaps, DE/RE transitions, local FC06 echo ambiguity and reply lengths are RTU
facts and must not be transplanted to CAN.

`RtuBusOwner.h:147` demonstrates the useful reusable *contract*:

- Caller-owned `PendingSlot`, `ResultSlot`, `ProducerSlot` arrays with explicit
  capacities (`RtuBusOwner.h:96`). Request admission copies request bytes and
  expectations and reserves result capacity before work can run.
- Generation-protected owner-scoped request IDs and sequence IDs; stale or
  foreign IDs cannot release or cancel current work (`RtuBusOwner.h:14`).
- One cooperative owner, no reentrant callbacks, bounded service work,
  per-producer FIFO, eligible-head fairness and waits that release bus time.
- Urgent request/result capacity reserved independently of ordinary pressure;
  `admitUrgent` runs at the next safe settlement boundary (`:156`).
- `beginSequence`, `invalidate`, `invalidateProducer`, `cancel`, `cancelUnsent`,
  `service`, non-consuming `result`, explicit `release`, and explicit recovery
  result lifecycle (`:157`).
- Separate idle host-configuration and commissioning ownership
  (`beginConfiguration`, `beginCommissioning`; `:203`, `:213`).
- One immutable terminal `Completion`, published once and retained until
  release (`:71`). Application-level axis reservation is separate from
  transaction queue ownership and retained historical failure evidence.

For CAN, design a controller/router contract that can process asynchronous
incoming traffic while an SDO operation waits, maintain route/session
correlation, and dispatch PDO/heartbeat/EMCY/NMT events to the correct consumer.
Do not force unsolicited CAN traffic through a single RTU request/reply slot.
Define exact concurrency restrictions from the implemented SDO channels and
drive capabilities. An application can serialize more conservatively, but
that must not prevent required asynchronous observations or service deadlines.

Urgency in a software queue is not a guaranteed physical CAN stop latency.
The design must account for controller queue backlog, bus arbitration, bus-off,
stale queued frames and failed transmissions. Determine and document the
controller's automatic retransmission and cancellation behavior; host
"no replay" policy alone does not describe controller-level behavior.
Recovery must not unexpectedly release stale motion frames still queued in
hardware/software. These are required audit questions, not asserted properties
of an as-yet-selected CAN adapter.

The platform boundary in `ProbePlatform.h:8` is intentionally small: console
I/O, idle and startup failure; Arduino and ESP-IDF use separate startup files
and share command/motor semantics. Preserve that split. Implement the example
CAN adapter for the actually selected controller/transceiver and audited SDK;
the installed library stays controller-independent.

## 7. Cached status, health and diagnostics

There is no universal installed `Health` enum in MotorControl-RS.
`StateCache.h:10` keeps, per observation block: checked value and validity;
last attempted target/operation/time/status; last-success time; observation
bounds; delivered time; and invalidation time. A failed attempt does not erase
last-known-good values. A late/older result cannot replace newer observation
evidence (`StateCache.h:48`). `current`, `ageUs`, and `fresh` are passive.

The console reports `communication=unavailable/failed/unknown/stale/current`,
alarms separately, and `readiness=unknown` rather than converting one probe to
a readiness guarantee (`ProbeConsole.cpp:2018`). `status` and `health` issue
no traffic. `read state` or another explicit query refreshes evidence.

For CAN, use analogous passive snapshots, but add independent controller bus
state, NMT state, heartbeat availability/age, emergency history, drive state,
enabled/mode/position observations and operation uncertainty where available.
An alive heartbeat is not proof that a drive is enabled, fault-free, in the
requested mode, safe to move, or physically stationary. Never mark health
healthy merely because an arbitrary CAN frame was received.

`Traffic.h:19` and `TrafficCapture` provide bounded caller-owned passive copied
traffic with cursors and drop accounting. `DebugSession` does not consume
normal receive bytes. Preserve passive copied diagnostics; CAN capture should
retain frame ID/flags/DLC/data and source timing/context, not fictional UART
byte/gap events. Dropped diagnostic events must remain separate from lost
protocol events.

## 8. Current CLI contract and programmatic example API

The actual grammar is protocol **3**, not the speculative/historical grammar
in archived prompts. `ProbeConsole.cpp:1847` reports product, protocol, version
and outstanding capacity. A plain command emits human output; `@ID` selects
correlated JSONL. Command IDs differ from admitted operation IDs. Completion
preserves its original output mode (`docs/cli_contract.md:11`).

| Preserve command vocabulary when semantically supported | Meaning |
| --- | --- |
| `help`, `help COMMAND`, `version`, `profile list`, `caps` | Passive discovery of local syntax and capabilities |
| `config`, `host`, `useaddr`, `wiring` | Host context; device changes are explicit separate commands |
| `status`, `health`, `stats`, `drv`, `memory`, `result ID` | Passive state/diagnostics/evidence |
| `stats reset` | Local counters only; retains faults/uncertainty |
| `probe`, `read identity`, `read config`, `read state` | Explicit non-changing reads |
| `moveby`, `moveto`, `speed`, `accel`, `decel`, `motion`, `stepsperturn` | Simple positioning and next-move intent |
| `axis config`, `axis origin`, `prepare`; `move relative/absolute/angle` | Exact coordinate/configuration/advanced positioning interface |
| `enable`, `motor-release`, `alarm-clear`, `position-clear`, `stop normal/fast` | Explicit drive effects, only where reviewed mapping exists |
| `velocity`, `home`, native operation groups | Public typed operation parity |
| `communication`, `persistence`, `discover` | Explicit bounded commissioning/persistence/discovery sessions |
| `monitor`, `cancel`, `release`, `recover` | Finite observations, local lifecycle, result storage, explicit transport repair |
| `debug off/raw/decoded`, `capture-read`, `load` | Production-path diagnostics and optional bounded load fixture |

Evidence: command inventory `ProbeConsole.cpp:53`, full current contract
`docs/cli_contract.md:41`. Removed aliases include `?`, `ver`, `ping`, `reset`,
`health check`, `help advanced`, and `profile ess_rs` operations. Do not
resurrect them solely because an older sibling or archived prompt used them.
Document intentional CAN naming changes, such as host bitrate versus serial
tuple. `release ID` frees a result; `motor-release` is a drive action.

Input is 128 bytes including NUL, token array is 22, and output buffers are
8,192 bytes (`ProbeConsole.h:24`, `ProbeConsole.cpp:899`). These are example
resource budgets, not mandatory limits for a new library. Derive and test CAN
command/report capacities from its actual schema, especially multi-frame
transfer/PDO/object-map evidence. Reject truncation, excess/trailing tokens and
numeric overflow before admission.

The example exposes firmware convenience calls `moveBy`, `moveTo`,
`submitMove`, `positionConfiguration`, `moveProgress`, `releaseMove`
(`examples/probe_cli/ProbeApp.h:34`). These use the ordinary application owner
and operation path. They are **not installed core APIs**. Keep that distinction
clear; use the installed `prepare.../next.../advance...` API in independently
owned firmware. Core behavior must never depend on a console callback.

Successful human terminals may recycle results only after output delivery;
correlated/programmatic and failed/uncertain results stay retained until explicit
release (`docs/cli_contract.md:172`). Backpressure must not block bus service,
drop a required result or falsely acknowledge an operation.

## 9. Python runner design to match

| Source | Responsibility to preserve |
| --- | --- |
| `scripts/bench_probe.py` | One console session owner, strict JSONL framing/correlation/deadlines, firmware protocol handshake, typed operations |
| `scripts/_bench_evidence.py` | Pure evidence validation shared through the existing public host imports |
| `scripts/bench_session.py` | Session/artifact lifecycle shared by campaigns |
| `scripts/bench_scenarios.py` | Named finite scenarios, read-only default, `--list` and `--plan-only` with no port access |
| `scripts/bench_motion.py` | Explicit finite motion/action/stop scenarios with retained evidence and planned cleanup |
| `scripts/bench_repeat.py` | Bounded comparison of setup strategies, never unlimited generic write stress |

`bench_probe.py:498` owns the serial session and invalidates unsafe sessions;
`begin`, `wait`, `command`, `identify` are at `:2254`, `:2418`, `:2459`,
`:2470`. `identify` requires protocol 3. Do not make a CAN client silently
accept incompatible RS payloads because it has similar command names. Give
the CAN firmware product/profile/schema an explicit verified identity and keep
host/firmware parity tests.

`docs/bench_scenarios.md:9` specifies read-only default, new evidence prefix,
bounded count/duration, no port access for planning, one serial owner, fixed
memory evidence summaries, streaming JSONL and retention of failed runs.
Raw capture and interpreted result checks are independent. Disconnect/reboot,
wrong product/protocol, malformed response, stale IDs, output exhaustion and
evidence-write failure must end/interlock the affected session.

RS defaults such as COM13, address 1, finite 100-increment moves, 60 rpm,
recorded firmware words and its existing bench authorization do not establish
a CAN bench setup. Select an explicit test fixture and capabilities from actual
CAN evidence. Preserve deliberate single writes and uncertainty; a failed run
must not cause a script to retry motion or invent compensating changes.

The host tools talk to the normal firmware CLI. They are not a second
implementation of drive operations. A later direct host-CAN adapter is a
separate consumer and must use the same reviewed semantics; do not grow an
independent Python motor protocol while claiming firmware/API parity.

## 10. Tests, CI and package integrity

`CMakeLists.txt:24` supports native ESP-IDF component registration without
Arduino. Native CMake provides static `MotorControlRS` and namespaced alias
`MotorControlRS::MotorControlRS`, C++11, warning flags, install/export and
`SameMinorVersion` compatibility (`:40`, `:239`). `library.json` is version
authority, advertises all frameworks/platforms, and exports only core sources,
headers, build metadata, license, README and changelog. `generate_version.py`
maintains `Version.h`. Exported core does not require Python or vendor PDFs.

Tests are standalone assert-based executables. Assertions remain enabled even
in Release (`CMakeLists.txt:67`). Native/Python test registration is explicit;
the verifier rejects missing tests, skipped/empty tests, disabled assertions,
unregistered Python tests, wrong fixtures and masked exit codes
(`scripts/verify.py:30`, `:85`, `:173`).

Important inspected testing patterns:

- Literal independent vendor frame vectors plus a different CRC fixture
  algorithm (`test/codec_test.cpp:15`); not only encoder/decoder round trips.
- Invalid arguments, count/capacity/overlap, exact framing, wrong address and
  function, raw unknown exceptions, immutable payloads and signed boundaries.
- Pure target overflow, exact rational precision, rounding, scales, wrap/tie,
  generation/reference and unsupported capability cases.
- Concrete sequence tests for no start after uncertain setup, wrong event
  correlation, expired evidence/deadlines, local cancellation, new completion
  observation and retained failure (`test/position_test.cpp`,
  `test/ActionEvidenceChecks.h`).
- Owner queue/result pressure, urgent quotas, fairness, per-producer ordering,
  stale IDs, cancelled in-flight work, recovery and commissioning isolation.
- SDK fakes for adapter behavior, native Arduino/IDF startup parity and
  console/API/Python parity tests.
- Package ZIP round-trip, clean source and installed consumers, C++11/C++17
  isolated public headers, and codec-only linkage without descriptive catalogue
  strings (`scripts/verify.py:92`, `:203`, `:222`).
- Negative tests for the verification/inventory tools themselves; do not allow
  metadata coverage or missing tools to become an accidental green check.

For CAN, replace wire-specific tests with independently sourced CANopen
vectors, malformed identifiers/flags/DLC/payloads, object/subindex/width
correlation, transfer progress/abort/timeouts, unsolicited routing, heartbeat
and boot-up invalidation, drive-state transitions, PDO map/configuration
validation, controller completion uncertainty and queue/recovery behavior.
Add only modes/transfers justified by the acquired protocol scope; test every
advertised one, and make deferred capability explicit.

Current hosted CI (`.github/workflows/verify.yml:1`):

| Job | Checked configuration |
| --- | --- |
| Native | Ubuntu 24.04, GCC and Clang matrix, Python 3.12, quick verifier |
| Arduino | Pinned PlatformIO 6.1.19, full Arduino subset |
| Native ESP-IDF | `espressif/idf:v5.5.5`, full IDF subset |

Workflow triggers are push, pull request and manual dispatch; read-only content
permissions; logs/summaries uploaded on success or failure. Arduino examples
pin `pioarduino` platform 55.03.311 (`platformio.ini:9`). Full verification
builds four Arduino environments and native IDF application/core-S3/core-S2/
portable-S2 fixtures (`scripts/verify.py:238`). S2 claims are compile-only and
do not imply an implemented or tested physical S2 adapter.

For MotorControl-CAN reproduce the check classes and workflow shape, then audit
the actually selected SDK/toolchain and CAN driver APIs. Do not inherit old
pin values without review or add nominal platform coverage with empty stubs.

## 11. Reference and coverage management

The source keeps original PDFs/software/CAD separate from extracts and
interpretation. `scripts/prepare_references.py` validates formats, hashes and
PDF text; differing existing bytes are not overwritten. Additional contrasting
manuals have a separate manifest/checker. `check_repository.py:18` checks
exact expected files, bytes, hashes, signatures, safe paths and links.

The register ledger `docs/reference/ess_rs_registers.json` contains scope,
sources, qualification, registers, indexed groups, enums, issues, gaps,
bitfields and reviewed multi-write windows. It generates public descriptors,
private compact access tables and documentation; basic codec linking does not
pull catalogue strings. The operation ledger
`docs/reference/ess_rs_operations.json` links typed operations, shared records,
public surface and explicit remaining gaps. `check_ess_operations.py` checks
coverage independently from catalogue presence.

Apply the same principle to CAN with an object-dictionary/profile ledger:
index/subindex or range, scalar/array/record type, width/signedness, unit/scaling,
access, valid value/bit masks, read side effects, required NMT/drive state,
PDO mapping eligibility, persistence/reset applicability, exact source page and
firmware/model scope. Keep object presence separate from callable typed
read/write/action coverage and separate again from hardware qualification.

`docs/profile_contract.md:62` defines four independent coverage dimensions:
documentation resolution, device availability, implementation and qualification.
Keep unresolved/unimplemented features in the denominator. A generic SDO
accessor is not complete typed family support. Account for diagrams/prose and
stateful workflows, not just object-table rows. A tool-generated EDS catalogue
is not evidence that every advertised object behaves identically on the
purchased model/firmware.

Original third-party files retain their licenses. Core MIT coverage is
`LICENSE:1`, copyright Jan Havelka; reused MIT source requires its notice.
Do not relicense vendor PDFs/software or execute downloaded commissioning
software as part of document preparation. Core distribution must exclude
references, bench evidence, compiled firmware and private artifacts.

## 12. Findings and choices not to copy

These are concrete migration hazards or documented limitations. They are not
claims that the RS implementation failed its own tests.

| Finding | Evidence | Required CAN treatment |
| --- | --- | --- |
| RTU protocol permeates runner/evidence/traffic | `RtuRunner.h:12`, `ReadOperation.h:58`, `Traffic.h:10` | Implement a CAN-specific frame/router/adapter contract; no DE/gap/CRC/FC06 copy |
| Some common types retain ESS representations | `MoveOperation.h:16` has `uint16_t speedRpm`; `VelocityOperation.h:34` has `int16_t nativeRpm`; `ReadOperation.h:30` has serial tuple | Preserve semantic names but design reviewed CAN quantity widths, units and route context; avoid a false source-compatible shim |
| Stop semantics are profile-specific | `ActionOperation.h:10`, `Actions.h:89` | Map normal/fast/enable/release/reset only from actual drive-state/options behavior; NMT operations are not assumed equivalent |
| Example scale is an assumption | `ProbeApp.h:10`, `profiles/ess_rs/Defaults.h`, `docs/cli_contract.md:83` | No copied 1000-steps/turn or boot zero; require explicit source/provenance for CAN scale/origin |
| Example depends on PSRAM | `ProbeApp.h:17` explicitly requires it and has no internal-RAM fallback | Core and portable examples remain usable without PSRAM; memory-placement choices belong to a particular optional board adapter |
| Default observation age expiry disabled | `ProbeApp.h:6`, `ReadOperation.h:11` | Keep policy explicit; CAN heartbeat-consumer deadlines and drive state freshness must not be disabled implicitly by copied settings |
| Freshness boundaries differ intentionally by path | `ReadOperation.h:15` uses `< maximumAgeUs`; `StateCache.h:39` uses `<=`; `src/Axis.cpp:78` rejects only `>` | Specify each CAN boundary and test exact equality; do not claim one universal freshness rule or silently unify behavior |
| Protocol-3 grammar differs from archive | `docs/cli_contract.md:11`, `ProbeConsole.cpp:1847` | Audit current source; preserve meaningful current command names, not obsolete aliases |
| Raw codec is weaker than typed operation admission | `Codec.cpp:116` intentionally ignores raw value meaning; `Position.cpp:89` enforces typed operating limits | Raw SDO support must not imply readiness, safe value semantics or supported motion workflow |
| Large shared application/parser remain | `ProbeApp.cpp` 2633 lines; `ProbeConsole.cpp` 2711; `bench_probe.py` 3462 | Copy tested module seams and single ownership, not the accumulated monolith or a speculative abstraction framework |
| Device-family completeness remains open | `docs/releases/1.0.0.md:47`, operation gap ledger | Do not describe RS as a finished universal motor family library or copy its implementation gaps as CAN device limitations |
| Motion evidence is fixture-specific | `docs/releases/1.0.0.md:23` | No inherited CAN hardware qualification, shaft/thermal/electrical performance or release maturity claims |
| Package release scope and branch counts differ | release note has 77 historical tests; current quick verifier has 78 | Record current commit/count; historical evidence belongs to its recorded snapshot |
| Historical FieldCore review is not current code | `docs/source_map.md`, `docs/reference/02_ecosystem_review.md:3` | Re-audit live sibling/FieldCore source for each affected prompt; do not import old findings as current fact |
| RS benchmark permissions and ports are unrelated | source `AGENTS.md` bench section, `docs/hardware_bench.md` | Do not transfer motor/COM13/free-shaft authorization to an unverified CAN installation |

The release names nine unresolved native-family areas and eighteen guarded
paired setters; velocity/homing/negative encoding/external I/O/restart durability
retain unqualified cases. Exact lists and provenance remain in
`docs/releases/1.0.0.md:47` and the machine-readable ledger. This honest partial
coverage model is worth copying; the ESS gaps themselves are not CAN features.

## 13. Recommended migration blueprint

Use package `MotorControl-CAN`, namespace/include/CMake identity
`MotorControlCAN`, and an explicit initial `profiles/lichuan_c/` with namespace
`MotorControlCAN::LICHUAN_C` and separately tracked CL86-C/model capabilities.
These are proposed names for the new
project, not names observed in the RS source. Keep reviewed CANopen mechanics
separate from CL86-C object dictionaries, units, state/mode restrictions and
vendor extensions. Scope family expansion through explicit profile descriptors,
not a promise that every similarly branded drive is compatible.

Preserve common motion and error vocabulary, context ownership, evidence rules,
directory roles, example shape, CLI effects, Python session ownership and
verification. Maintain a per-symbol mapping showing unchanged meaning versus
deliberate protocol-specific changes. Copy reusable arithmetic source only
after its types/dependencies/tests and license are reviewed; initially keep
independent implementations rather than creating a speculative shared package.

Each implementation prompt must begin by inspecting current MotorControl-RS,
the relevant current I2C peers and FieldCore read-only. Record their HEAD/dirty
state and source symbols; inspect actual callers and tests. Prefer established
names where compatible and record why a deviation is required. Do not infer
framework ownership or I2C recovery/retry semantics from naming familiarity.
Do not edit reference repositories as part of implementing this library.

Recommended stage ordering:

1. Re-audit current ecosystem and primary protocol documents; establish exact
   target/model/firmware applicability and a source-backed object ledger.
2. Write the namespace/API/evidence/ownership/CLI/coverage contracts and package
   scaffold, including a minimal verified C++11 consumer and CI wiring.
3. Implement checked pure CAN/CANopen codecs and profile access policy with
   independent vectors and immutable-output tests.
4. Implement typed read/probe/state observations and bounded transfer contexts;
   keep discovery confidence, boot-up and async state routing explicit.
5. Implement caller-owned owner/adapter/retained results and fault/recovery
   semantics, then standalone read-only console and Python parity.
6. Implement documented drive-state/actions, exact unit/position behavior,
   profile-position/velocity/homing and native configuration in bounded typed
   slices; each slice includes CLI, tests and ledger updates.
7. Implement explicit PDO/SYNC/configuration/commissioning/persistence support
   only for documented capability; retain mode/object-specific restrictions.
8. Verify complete documented scope, clean packages, both framework consumers,
   resource budgets and independently recorded hardware qualification; deliver
   a FieldCore integration contract without embedding that framework.

At each stage a feature is complete only when it has the applicable public API,
real implementation, explicit unsupported/gap status where needed, CLI access,
native tests, Python coverage/evidence validation, maintained documentation and
honest qualification record. Do not manufacture placeholder APIs to close a
coverage row.

## 14. Verification performed for this audit

Ran the source repository's own complete quick workflow without altering it:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
python scripts/verify.py --mode quick --build-dir C:/Users/Honza/AppData/Local/Temp/motorcontrol-rs-audit-20261023
```

Environment: Windows, Python 3.12.10, MinGW GCC 15.1.0, CMake/CTest 4.0.1,
Ninja. Build and generated artifacts reside outside the audited source tree.

| Check | Result |
| --- | --- |
| Offline reference/contrast integrity | Passed |
| Native Release configure/build | Passed, 407 build steps |
| Registered native/Python tests | **78/78 passed**, 13.27 s CTest runtime |
| Generated version/register/coverage/repository checks | Passed as registered tests |
| Exported core ZIP validation and clean package build/install | Passed |
| Clean source consumer and isolated headers | **3/3 tests passed** |
| Clean installed consumer and isolated headers | **3/3 tests passed** |
| Codec-only symbol/catalogue exclusion | Passed for both consumers |
| Quick verifier aggregate | **PASS**, all 15 recorded steps exit zero |
| Firmware builds, hosted CI and hardware | Not run for this read-only audit |
| Final source git status | Clean |

Local evidence:
`C:/Users/Honza/AppData/Local/Temp/motorcontrol-rs-audit-20261023/summary.json`
and numbered logs. Locally generated core ZIP SHA-256:

```text
dadc797f55a4053958e690d7eaea9c91de9363338db400dc2fe010c47240be20
```

This is the audit build of the recorded branch commit, not a re-attestation of
the original v1.0.0 release artifact or of any hardware image. No newly observed
test failure was found in the executed checks. Passing software checks does
not resolve the documented hardware/firmware uncertainties or certify a CAN
implementation that does not yet exist.
