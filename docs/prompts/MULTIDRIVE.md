# Mandatory stepper and servo criteria

This contract applies through [COMMON.md](COMMON.md). Read the row for the active prompt and the applicable contrast cases below; it is not 50 extra implementation tasks. The [scope decision](../multidrive_decision.md), [CANopen survey](../audit/CANOPEN_DRIVE_SURVEY.md) and [other protocols](../audit/OTHER_CAN_PROTOCOLS.md) provide evidence. Lichuan C over Classical CANopen remains first. N5/EPOS4 are mandatory test-only contrasts, not installed support. Use [ADD_DRIVE.md](ADD_DRIVE.md) for a later real profile.

| Prompt | Concrete requirement and acceptance |
| --- | --- |
| 1 | Audit multi-profile RS and consumer boundaries; keep motor type, protocol and support maturity separate. |
| 2 | Keep original sources and model/firmware provenance distinct from implementation support. |
| 3 | Review per-model Lichuan facts; no cross-vendor object or firmware assumptions. |
| 4 | Finish N5 stepper/BLDC and EPOS4 contrast evidence without installing their drivers. |
| 5 | Keep CANopen stack dependencies below the common motion interface and optional for core consumers. |
| 6 | Prove shared logical-axis intent/ownership with small N5/EPOS4 fixtures and endpoint/subaxis mapping. |
| 7 | Build a consumer that includes only core headers and a Lichuan consumer without forcing optional SDK/stack/catalogue dependencies; Lichuan is always built in the one library. |
| 8 | Register contrast tests outside installation; tooling must not make SDKs mandatory for the core. |
| 9 | Keep common values wider than RS native RPM fields; distinguish command/feedback/effort units and quantization. |
| 10 | Allow servo coordinates without full-step geometry; keep independent load/command/feedback scale evidence. |
| 11 | Preserve 11/29-bit identifiers and RTR DLC versus payload; apply CANopen restrictions only in its service. |
| 12 | Generate profile-keyed policy: equal vendor object numbers never imply equal meanings. |
| 13 | Common outcome vocabulary cannot require every protocol to acknowledge a write through SDO. |
| 14 | Preserve uncertainty for no-ack protocols; CANopen reconciliation is not a universal retry policy. |
| 15 | Keep heartbeat/EMCY/boot-up as optional CANopen observations, separate from common motion health. |
| 16 | NMT is protocol-specific; SDO success does not establish operational state or generic motor readiness. |
| 17 | Use observed maps and profile-specific PDO counts; Lichuan four and N5 eight are distinct evidence. |
| 18 | Remapping belongs to the selected profile/service; partial effects cannot become invented rollback. |
| 19 | Expose typed observations with explicit unavailable fields; do not require all drives to have the same feedback. |
| 20 | Driver subdivision/ramp/scale settings stay native; servo operation does not require stepper geometry. |
| 21 | Keep physical wiring, configured I/O function and observed level distinct; capabilities are profile-specific. |
| 22 | Keep current limits, torque intent and tuning facts distinct; no invented generic effort-control implementation. |
| 23 | Route selected protocol domains through one receiver; reject overlapping routes and unsupported frame capabilities. |
| 24 | Two CANopen vendors share one service backend; recovery never fabricates absent acknowledgement or completion. |
| 25 | Passive application health cannot replace native drive state or impose sensor-style retry/replay. |
| 26 | Native examples use public APIs without framework or CLI dependency; installed support lists only real profiles. |
| 27 | Adapter depends on frame/transport contracts, not the Lichuan dictionary; Classical versus FD is explicit. |
| 28 | Arduino/IDF share application behavior and thin adapters; no board/SDK types in common motion. |
| 29 | Qualification facts remain per model/interface/firmware/motor; a shared protocol does not resolve vendor contradictions. |
| 30 | Enable/release/hold/coast/brake semantics stay profile-specific; no hidden calibration or commutation workflow. |
| 31 | Stop, NMT stop, cancel and release are different; optional brake/feedback evidence remains explicit. |
| 32 | Fault clear, counter clear, host origin and homing references are distinct native effects. |
| 33 | Use the same finite intent with checked native translation; completion does not necessarily prove zero velocity. |
| 34 | Exercise production angle/position preparation through source-backed Lichuan/N5 contrasts without fake vendor engines. |
| 35 | Finite velocity does not introduce generic cyclic/torque control; required refresh cannot retrigger motion. |
| 36 | Homing prerequisites and reference publication are method/profile-specific; capability rejection does not generalize to all servos. |
| 37 | Discovery selects reviewed protocol/profile domains; no global CAN-ID sweep or protocol guesses. |
| 38 | Identity, route and active motor configuration have independent evidence; do not assume universal node ranges. |
| 39 | Save/reset/defaults and persistence effects remain vendor-specific with retained uncertain outcomes. |
| 40 | Help/profile/caps distinguish compiled support, selected protocol and evidence; no future-driver success stubs. |
| 41 | Native settings/network commands apply only to matching profiles; no fabricated NMT for other protocols. |
| 42 | Common command names remain stable; all actual routes share public APIs and exact numeric representations. |
| 43 | Handshake/evidence includes protocol/profile/model/firmware; no universal node/statusword or float-precision assumption. |
| 44 | Select finite scenarios by capabilities; unavailable evidence is an explicit non-pass, not fabricated success. |
| 45 | Exercise lost replies, unobserved completion and differing native states on the CANopen path through small actual-contract fault fixtures; protocols with no acknowledgement or a native done report are prompt 46's contrasts. |
| 46 | Run N5 stepper/BLDC, EPOS4 and proprietary-frame contrasts, including absent acknowledgement and native done reports, on production common code, not installed fake drivers. |
| 47 | Qualify only the actual model/interface/firmware/motor/configuration and independently observed behavior. |
| 48 | Related models, brake/stop/watchdog behavior and load limits need separate evidence; no family-wide extrapolation. |
| 49 | Keep research, fixtures, implemented scope, firmware builds and hardware qualification in separate support columns. |
| 50 | Final candidate claims only verified scope; additional profiles follow ADD_DRIVE with their own evidence and implementation. |
| 51 | SYNC production and synchronous PDO semantics are per-profile facts; other protocols get no invented SYNC, and a profile without synchronous PDO support refuses the trigger. |
| 52 | Change operations are per-profile capabilities with their own evidence; common requests carry exact units and refuse unsupported changes before admission. |
| 53 | Axes are identified independently of node IDs, mix motor kinds and profiles under one owner per interface, and stop individually; capacity is an application choice, not a profile assumption. |
| 54 | Qualify only the actual drives, firmware and configuration; another family's SYNC or multi-axis behavior is never inferred. |
| 55 | Self-repair and retry classes are per-protocol services; other protocols get their own timeouts and never inherit SDO resync or NMT semantics. |
| 56 | Throughput changes keep one owner per interface and the exact TX retirement proof for every protocol on it. |
| 57 | Enable, release and mode select are per-profile operations with their own words and evidence; product identity selects the profile, firmware identity is reported. |
| 58 | Records are keyed by profile and product identity and carry their own evidence; one family's record never qualifies another. |
| 59 | Speed ranges, clamp behavior and direction rules are per-profile measured facts with exact native units. |
| 60 | User units convert host-side to each profile's native units and refuse what a profile cannot represent. |
| 61 | Node, bitrate, PDO and save mechanics are per-profile; protocols without these services refuse them before any work. |
| 62 | Stop-input functions and polarity values are per-profile documented facts; MCU pins stay application-owned. |
| 63 | Protocols share one interface only through a checked identifier plan; no protocol receives another's semantics. |
| 64 | The common interface carries exact units and capability refusals; native words, objects and evidence stay in each profile. |
| 65 | Packages expose profile-neutral components and each profile's API without vendor material. |
| 66 | Qualify only the actual drives, firmware and configuration; another family's behavior is never inferred. |
| 67 | Each MCU and board needs its own adapter proof and qualification; results never transfer between chips. |
| 68 | The CAN task, queues and job runner use the common axis interface; profile-specific words stay in each profile. |
| 69 | Drive I/O functions and polarity values are per-profile documented facts; MCU pins and the watchdog stay board facts. |
| 70 | The axis table records each axis's profile and expected product identity; checks run each profile's own readbacks. |
| 71 | Qualify only the actual board, wiring, drives and firmware; nothing transfers between boards or families. |
| 72 | Pulse-rate limits, enable-fault signatures and the release word are per-profile, per-firmware measured facts; the common axis reports only the typed signal. |

## Required contrasting test cases

Keep the fixtures small and evidence-based. They must exercise production common code and endpoint/service contracts, not reproduce the implementation in tests. Where a target's exact behavior has no accessible authoritative evidence, use an explicitly abstract invariant test and leave that device detail unresolved.

1. N5 configured for a stepper and a BLDC motor can use common exact position preparation without mandatory full steps/revolution. Motor kind does not select a different CAN transport.
2. Lichuan millisecond ramp parameters and a servo's documented acceleration representation cannot be substituted without a profile conversion. Commands and sensor feedback need independent scale evidence.
3. A servo native value wider than RS's int16/uint16 RPM fields does not overflow a shared request. A float-native protocol reports explicit quantization instead of falsely promising an exact wire integer.
4. A logical axis maps to a protocol endpoint/subaxis without changing generic 1..127 address assumptions, because there are no such generic assumptions.
5. A proprietary frame with a valid extended ID or RTR polling requirement is admitted by the framing boundary, rejected by a mismatched CANopen route, and never decoded simultaneously as two protocols. FD-required work rejects on Classical-only adapters.
6. No-ack writes, reported trajectory completion with nonzero terminal velocity, optional brake/calibration and missing position/torque modes preserve explicit capability/outcome semantics.
7. Profile-keyed object policy keeps N5 NanoJ control and Lichuan save/reset at index 0x2300 separate; resource capacities can accommodate documented PDO-count differences without changing common axis types. Unknown firmware/state terminology never becomes an invented CANopen standard state.

Document which case uses vendor-backed values, which is a protocol framing fixture, and which is a pure common-contract invariant. No fixture belongs in `profile list` as a real installed drive.
