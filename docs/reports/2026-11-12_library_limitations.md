# Library limitations and flexibility options (2026-11-12)

Read-only review of `main` at `4a976a6` (prompts, progress, backlog, the 2026-11-12 reports and the code in `include/`, `src/`, `adapters/`, `examples/`, `test/`).
Line numbers are at that commit; nothing was built, flashed or sent, and no other file was changed.

**Words used.** SDO: one request and one reply, used for reads and writes. PDO: a data frame sent without a
request; TPDO goes from drive to host, RPDO from host to drive. SYNC: a shared timing pulse on the bus. NMT: network
start and stop commands. Owner: the one piece of code that sends and receives every frame on a CAN interface. Gate
and record: a capability stays closed until the application supplies a measured record for that exact drive.

**Kinds.** *Rule*: a deliberate project rule. *Drive*: a limit of the Lichuan drive or its manuals. *Planned*: missing,
with a prompt or backlog owner. *Missing*: missing, no owner yet. *Accidental*: a code limit nobody chose on purpose.
*Changing now*: another worker is changing it.

**Status words.** *Host-tested*: passes tests with fake drives. *Ran on the bench*: ran on two CL86-C drives of one
firmware, judged from the drives' own reports. *Qualified*: nothing is qualified yet.

## 1. Overall architecture and layering

**1.1 The common layer has words, not actions.** The common headers give units, coordinates and result words. Every
real action (enable, move, velocity, home, stop) is a Lichuan function, and the manufacturer and family lists have one
entry each. For example, your code calls `LICHUAN_C::prepareMoveAbsolute`; a Nanotec axis would need other calls, its
own session code and an edit to the library's lists.
- Kind: Rule (static selection, no plugin system) and Missing (no second profile yet).
- Change: when the second real profile (N5) is built, add a small per-profile function table or a thin common front,
  and let applications add list entries. Cost: design work; a front built before a second drive exists can hide real
  differences.
- Evidence: `include/MotorControlCAN/profiles/lichuan_c/Position.h:37`, `:386`; `include/MotorControlCAN/Discovery.h:10-16`; `docs/multidrive_decision.md:50`.

**1.2 The usable axis object and all bus code are example code, not the installed library.** `AxisSession` and
`ProbeMotion` (which tie reads, enable, move and stop to the owner), the owner and the TWAI and CANopenNode adapters are
compiled from the repository. The package exports only `include/` and `src/`. For example, installing the PlatformIO
library gives you units and codecs but no way to send a frame or run a move.
- Kind: Rule (portable core without a CAN stack) and Missing (packaging of the optional parts).
- Change: publish the owner, adapters and a trimmed session as a separate optional package. Cost: they become public
  APIs that need versioning, Doxygen, per-platform build checks and CANopenNode's Apache-2.0 notice.
- Evidence: `library.json:13-15`; `examples/common/AxisSession.h:201`, `:280`; `examples/native_probe/README.md:12`.

**1.3 The owner speaks only CANopen.** `CanBusOwner` is built on the CANopen `SdoClient` and can only send SDO and NMT
frames. For example, an ODrive or VESC axis cannot use it, and because one interface has exactly one receiver, such an
axis cannot share the bus with the Lichuan drives without a new owner.
- Kind: Rule for now (CANopen first, one protocol per interface by default) and Missing.
- Change: split the owner into a protocol-neutral frame scheduler and protocol parts. Cost: a large change to the most
  tested part, with risk to stop priority and the TX bookkeeping.
- Evidence: `adapters/owner/CanBusOwner.h:41`; `adapters/owner/CanBusTypes.h:62-65`; `docs/multidrive_decision.md:87`.

**1.4 One task, fixed storage, nothing implicit.** All owner, session and operation calls must come from one task.
Contexts must stay in static or owner storage, never copied to the stack. Every option defaults to a value that
refuses. For example, a UI task on the second core cannot call `requestMove` directly, a move with
`observationIntervalUs` left at 0 is refused, and `prepareMoveAbsolute` takes 16 inputs.
- Kind: Rule.
- Change: give the examples a small command queue into the owner task and named example presets (never core
  defaults). Cost: small, but a preset can be mistaken for vendor timing.
- Evidence: `adapters/owner/CanBusOwner.h:4-5`; `include/MotorControlCAN/profiles/lichuan_c/Actions.h:67-69`; `Position.h:206-213`, `:386-393`.

## 2. Protocols and buses

**2.1 CANopen only.** `Protocol::PROPRIETARY` is only a label; no non-CANopen code exists. For example, ODrive
CANSimple, VESC or moteus drives cannot be used.
- Kind: Missing (listed as later candidates, no prompt).
- Change: one protocol backend at a time, with its own codecs and uncertainty rules, plus the owner split (1.3). Cost:
  high per protocol.
- Evidence: `include/MotorControlCAN/Endpoint.h:16-19`; `docs/multidrive_decision.md:23-24`.

**2.2 The host cannot send PDOs or SYNC.** The owner sends only SDO and NMT frames. For example, you cannot send a new
target by RPDO, or a SYNC pulse to start two drives together.
- Kind: Planned (prompt 51; M13, F16).
- Change: add RPDO and SYNC send kinds with their own timing and bookkeeping. Cost: medium code, plus bench trials,
  because the Lichuan manuals do not document SYNC.
- Evidence: `adapters/owner/CanBusTypes.h:62-65`; `docs/prompts/51_sync_producer.md`.

**2.3 No CAN FD.** Frames are Classical CAN with at most 8 data bytes; a frame marked FD is refused. The S2 and S3 CAN
controllers are Classical only. For example, a CAN FD servo cannot be connected.
- Kind: Rule (separate milestone) and hardware limit.
- Change: a separate FD frame type and an adapter for an FD-capable controller. Cost: more RAM per frame and a new
  adapter with its own proof.
- Evidence: `include/MotorControlCAN/can/Frame.h:11-14`, `:24`; `docs/adapters/twai_sdk.md:79`; `docs/adapters/twai_esp32s2.md:24`.

**2.4 One CAN interface.** Only one owner on one TWAI controller is built and tested, and the S2 and S3 each have one
CAN controller. For example, splitting a machine across two CAN buses is untested.
- Kind: Missing (the classes are instances, but nothing tests two) and hardware limit.
- Change: host-test two owner stacks side by side; on hardware use a chip with more controllers (P4, pending) or an
  external controller. Cost: medium, with a new adapter for an external controller.
- Evidence: `examples/common/EspProbeMain.cpp:38-41`; `adapters/twai/README.md:31`.

**2.5 A stuck SDO route costs an MCU restart; the host must be the only SDO client.** After an unclear SDO exchange
(timeout, unexpected reply) the channel is blocked, and the only way back (a "drain marker") rests on a drive fact not
yet proven (Q18). For example, on 2026-11-04 four such events each needed an MCU restart. The same rule assumes no
other tool, such as a PC configuration program, talks SDO to the drive at the same time.
- Kind: Rule (never guess which reply belongs to which request) and missing evidence (Q18, D14, H04).
- Change: measure Q18 with an independent listen-only observer to qualify the drain marker, or add an explicit,
  recorded "reset communication" recovery. Cost: bench time; the reset changes the drive's network state.
- Evidence: `adapters/canopennode/SdoClient.h:30-33`, `:41-42`; backlog D14, H04.

**2.6 Small SDO, no extra CANopen services.** One SDO transfer carries at most 32 bytes. There is no block transfer and
no LSS (the service that sets node ID and bitrate over the bus). For example, a long text object cannot be read.
- Kind: Rule (only what the Lichuan needs).
- Change: larger caller-owned buffers, or LSS, when a drive needs them. Cost: RAM, because every kept result copies
  the buffer.
- Evidence: `include/MotorControlCAN/canopen/SdoTransfer.h:47`, `:49`; `docs/architecture_plan.md:112`.

## 3. Drives and profiles

**3.1 Only Lichuan 86C models.** The model list is CL86-C and OL86-C. The 57C models are named in the docs but cannot
be selected. For example, a CL57-C cannot be bound even though its CANopen interface looks the same.
- Kind: Missing (needs 57C evidence; each model keeps its own evidence by rule).
- Change: add CL57-C and OL57-C after a 57C manual and EDS review. Cost: per-model evidence rows and gates.
- Evidence: `include/MotorControlCAN/profiles/lichuan_c/Settings.h:20-24`; `docs/architecture_plan.md:9`.

**3.2 Other brands exist only as test fixtures.** Nanotec N5 and maxon EPOS4 appear only in a test that checks the
common types fit them. For example, there is no N5 profile that can enable or move a drive.
- Kind: Planned (prompt 46; `docs/prompts/ADD_DRIVE.md`).
- Change: build N5 as the second real profile; it is the best test of the common layer. Cost: a full profile and
  hardware.
- Evidence: `test/multidrive_contract_test.cpp:3`.

## 4. Motion features

**4.1 Only profile position, profile velocity and homing.** The manuals document modes 1, 3 and 6. There is no cyclic
mode (CSP, CSV, IP) and no torque mode, and the library has no cyclic or torque API. For example, you cannot stream a
new position every 1 ms. Other mode numbers were never tried (D17).
- Kind: Drive (documented modes) and Rule (no cyclic or torque API in the first release).
- Change: run the D17 no-motion probe of other `6060` values; add a cyclic API only for a drive that has such a mode.
  Cost: a new timing and watchdog contract.
- Evidence: backlog D17; `docs/multidrive_decision.md:75`.

**4.2 No change to a running move or velocity run.** Immediate update (controlword bit 5) is refused, a velocity run
has one rate, and a move cannot be given a new target. For example, you cannot speed up a running move.
- Kind: Planned (prompt 52; F01 to F16), blocked by facts the manuals do not give.
- Change: first F02 (measure a rate change) and F03 (a velocity change step, closed by default), then F05 to F09 for
  moves. Cost: bench measurements first, then medium code.
- Evidence: `src/profiles/lichuan_c/Position.cpp:364-367`; `Position.h:202-204`.

**4.3 Moves start only from standstill; no queue, no blending.** A move is admitted only from an idle drive and ends
with its own `000F` rearm step. For example, three moves in a row stop between each other; there is no smooth path.
- Kind: Rule (no new edge into a running curve, DEC-8) and Drive (buffered set-points undocumented).
- Change: measure buffered set-points (F14) and, if they work, add a typed queue of one. Cost: hard uncertainty about
  which set-point the drive took.
- Evidence: `Position.h:19-30`, `:156-162`.

**4.4 Velocity runs are finite, and in practice slow and short.** Every run needs a duration and a non-zero rate. The
only gate in use is the attended experiment: at most 60 rpm and 20 s. For example, "run at 200 rpm until I say stop" is
not possible.
- Kind: Rule (finite by design; the caps are project policy) and missing evidence (no measured velocity record yet).
- Change: review bench rounds 7 to 12 and the night soak into a measured record, which lifts the cap to the measured
  range; a renewable run lease for long runs. Cost: review work; long runs depend on host-loss behavior (8.5).
- Evidence: `include/MotorControlCAN/VelocityOperation.h:13`; `include/MotorControlCAN/profiles/lichuan_c/Velocity.h:17-34`; `src/profiles/lichuan_c/Velocity.cpp:127`.

**4.5 No negative velocity.** A negative rate needs a record with `reverseQualified`, and the attended experiment
forbids it. For example, a reverse conveyor run is refused.
- Kind: Rule (your instruction, F15) and Drive (one source says `60FF` is signed, another unsigned, Q03).
- Change: measure reverse once (Q03) and issue a reverse record; the code path already exists. Cost: one bench series.
- Evidence: `Velocity.cpp:123`, `:182`; backlog F02, F15.

**4.6 Native unit limits.** Profile speed is whole rpm (1 to 3000), ramps are a time in ms (1 to 2000, zero refused),
and absolute targets are -1,000,000 to +1,000,000 pulses. For example, 2.5 rpm cannot be sent exactly, and if one turn
is 10,000 pulses the absolute range is +-100 turns.
- Kind: Drive (manual ranges) and Rule (zero ramp refused, Q11).
- Change: nothing for rpm; the existing host origin already lets you shift the position window.
- Evidence: `Position.cpp:12-15`; `Position.h:199-200`.

**4.7 No typed mode switch.** Enable writes no mode, and only a velocity record that allows a mode change writes
`6060=3`. The example and bench code send other `6060` writes as raw work. For example, going back from velocity to
position needs your own raw `6060=1` write.
- Kind: Accidental (a gap between operations).
- Change: one small typed mode-selection operation, released drive only. Cost: low code; it needs the mode-change
  measurement reviewed (one exists, trial 917).
- Evidence: `Actions.h:236`; `Velocity.cpp:296`; `examples/common/AxisSession.cpp:42-45`.

**4.8 Homing: four switch methods, never run.** Only methods 17, 18, 24 and 29 exist, the gate is closed, and no
homing ran on the bench. For example, homing on the current position or an index pulse is not offered.
- Kind: Drive (documented methods) and missing evidence.
- Change: run a homing qualification with wired switches; add methods only with evidence. Cost: wiring and bench time.
- Evidence: `include/MotorControlCAN/profiles/lichuan_c/Homing.h:19-24`; `docs/prompts/PROGRESS.md:95`.

## 5. Multi-axis and stopping

**5.1 Axis count fixed at 2 (being changed now).** SDO channels, owner reservations and the example app hold two axes,
and the owner's `active_` initializer is written for exactly two. For example, a third drive is refused. The installed
core has no axis limit.
- Kind: Accidental (adapters and examples). Changing now (M04, prompt 53).
- Change: the compile-time capacity header in the capacity report. Cost: low; the 8-bit slot index caps it near 125
  axes.
- Evidence: `adapters/canopennode/SdoClient.h:50`; `adapters/owner/CanBusOwner.h:24`, `:375`; `examples/common/ProbeApp.h:359`
  (lines at `4a976a6`; these files are being edited in the working tree now).

**5.2 No group stop.** Each axis has its own stop and its own urgent slot, but there is no "stop all" with a combined
result. For example, after a fault on axis A, your code must stop B, C and D itself and track each result.
- Kind: Planned (M02, M12, prompt 53).
- Change: a group stop made of per-axis stops, successful only when every axis is seen stopped. Cost: low to medium;
  it reuses the per-channel urgent slots.
- Evidence: `CanBusOwner.h:22`; backlog M02.

**5.3 No synchronized start.** Starts leave one frame at a time, so the second drive starts later than the first, and
there is no SYNC. For example, two axes cannot start a coordinated move at the same instant.
- Kind: Planned (M07, M13, prompt 51).
- Change: measure the skew (M07), then try SYNC as a common start trigger (M13 step c). Cost: medium; the result is
  unknown because Lichuan SYNC is undocumented.
- Evidence: `adapters/twai/TwaiAdapter.cpp:153`; backlog M07, M13.

**5.4 Example app and console: one motion axis, shared fairness.** The console attaches motion to one axis at a time,
and every session uses producer 1 for fair scheduling. For example, you cannot stop axis B from the console while axis
A's move is attached.
- Kind: Accidental example limits (M03, M06).
- Change: per-axis console attachments and stop slots, one producer per session. Cost: low, about 2.2 KB per extra stop
  pair (capacity report estimate).
- Evidence: `examples/common/ProbeConsole.h:198`; `examples/common/ProbeApp.cpp:159`.

## 6. Memory, capacity and platforms

**6.1 S2 RAM is the first wall.** The S2 bench image keeps about 210 KB of worst-case storage at once. Each
`AxisSession` holds separate move, velocity, home, enable and clear contexts (about 35.8 KB per axis), and each bench
record has its own buffer. For example, the S2 runs 2 axes on the bench and about 4 in a lean app (estimates).
- Kind: Accidental layout and platform limit (no PSRAM).
- Change: B01 (IDF settings, about +31 KB), B03 (one operation slot per session, about -14.7 KB per axis), B02 (shared
  bench arena). Cost: low, host tests. B05 (fewer evidence copies) is a contract change.
- Evidence: `examples/common/AxisSession.h:697-746`; backlog B01 to B06 (sizes from the S2 memory report, not re-measured).

**6.2 PSRAM is not used and untested.** Both firmware configs turn PSRAM off, and the TWAI driver refuses storage
outside internal RAM. The S3 placement plan needs no core change, but there is no S3 board. For example, a 20-axis S3
build exists only on paper.
- Kind: Planned (prompt 53) and missing hardware.
- Change: place per-axis objects in PSRAM at startup; keep owner, driver and stack internal; add a compile-only
  S3-PSRAM build, then a board. Cost: medium; cache misses can lengthen service gaps.
- Evidence: `examples/probe_idf/sdkconfig.defaults:11`; `adapters/twai/IdfTwaiDriver.cpp:57-58`.

**6.3 ESP32-S2 and S3 only, with fixed SDK settings; Arduino on S2 blocked.** The TWAI adapter refuses to build for
other chips and needs power management, cache-safe ISR and SMP off, with an SDK TX queue of one. Arduino on S2 is
blocked by the pinned package; Arduino on S3 is compile-only. For example, classic ESP32, C3, C6 or P4 do not build,
and light sleep cannot be used.
- Kind: Missing (other adapters) and Rule (the TX bookkeeping proof covers only these settings).
- Change: one adapter per chip with its own proof. Cost: medium per chip.
- Evidence: `adapters/twai/IdfTwaiDriver.cpp:14-19`; `examples/probe_cli/README.md:8-11`, `:18`.

**6.4 No PC CAN adapter.** The native build runs only against a fake transport. For example, you cannot drive the
motors from a Linux PC with SocketCAN or a USB-CAN adapter.
- Kind: Missing.
- Change: a SocketCAN adapter that reuses the owner. Cost: medium; it needs its own model of when a frame has left
  the kernel queues.
- Evidence: `examples/native_probe/README.md:3-7`.

## 7. Timing and throughput

**7.1 One frame per service turn.** The TWAI adapter keeps at most one frame in the controller, and per turn the owner
starts at most 2 new services. The capacity report estimates about 1000 frames/s at a 1 ms turn; polling 7 fields
every 50 ms fills that at about 7 axes (estimate, not measured). Owner work per turn grows roughly with the square of
the slot count.
- Kind: Rule (the code always knows exactly which frame left).
- Change: a TX window of 2 or 3 frames with per-frame bookkeeping, and caching each slot's channel. Cost: high: the
  proof after bus-off and missing ACK must be redone.
- Evidence: `TwaiAdapter.cpp:148-155`; `CanBusOwner.h:26-29`; `adapters/owner/CanBusOwner.cpp:443-496`.

**7.2 Motion is watched by SDO polling only.** Each observation cycle is 7 SDO reads one after another, not one
snapshot, and motion never uses PDO samples. A move shorter than one cycle never shows "complete" and ends at its
deadline as "not observed". For example, a 5-pulse jog that finishes in 10 ms, with a 50 ms cycle, reports not
observed.
- Kind: Rule (PDO sample freshness not proven, D11) and Planned (M08).
- Change: TPDO-based observation once a sample freshness rule is measured (D11, Q17). Cost: medium, with evidence work.
- Evidence: `Position.h:109-111`, `:135-140`.

**7.3 One bad bus event stops all sending.** A frame nobody acknowledges is retried by the hardware forever and blocks
all sending until your code starts an owner recovery. One dropped received frame (16-entry buffer) also stops sending.
For example, if the cable is unplugged during a read, every later frame, stops included, waits for that recovery.
- Kind: Rule (no hidden drop or resend) and controller behavior.
- Change: an application watchdog that starts owner recovery, and a deeper receive buffer where RAM allows. Cost:
  recovery invalidates routes and evidence.
- Evidence: `adapters/twai/TwaiAdapter.h:80-82`; `TwaiAdapter.cpp:123-126`; `adapters/twai/IdfTwaiDriver.h:24-26`.

## 8. Safety and recovery rules (refused on purpose)

**8.1 Motion gates start closed and lock to exact firmware.** Moves, velocity and homing refuse until the application
supplies a measured record, and the record names an exact 8-value drive identity. The model must be stated, not
detected. For example, a replacement drive with different firmware stops moving until it is measured again.
- Kind: Rule.
- Change: let a record list several reviewed identities, and add a tool that writes records from bench evidence.
  Cost: each firmware still needs measuring; risk of a false "same firmware" claim.
- Evidence: `Position.h:113-118`, `:146-155`, `:179`; `Settings.h:19`.

**8.2 Enable is experiment-only; there is no release.** The enable gate has only CLOSED and ATTENDED_EXPERIMENT. There
is no release (free shaft) command, because no manual pairs a command with that effect. For example, you cannot
de-energize the motor to turn it by hand; only a quick stop happens to release it.
- Kind: Rule and Drive (documentation gap D-17).
- Change: a measured enable gate after review of the seven bench enables, and a typed release from the measured word 6
  behavior. Cost: low code; review and bench work.
- Evidence: `Actions.h:50-61`, `:166-171`; `examples/common/ProbeConsole.cpp:168-171`.

**8.3 No automatic retry, replay, re-enable or fault clear.** An unconfirmed write stays "uncertain" and is never sent
again; after a reboot nothing resumes. For example, if a move trigger gets no reply, the move ends uncertain and your
code decides what to do.
- Kind: Rule.
- Change: keep this in the core; applications may add explicit, recorded policies.
- Evidence: `AGENTS.md:75`; `Position.h:216-217`.

**8.4 Stops follow strict report rules.** A stop is admitted and settled only on drive reports that match measured
patterns. On 2026-11-11 one drive reached a report no stop path accepted and stayed enabled (S20); some velocity quick
stops stay unsettled (S19). The best-effort `000B` quick stop needs no state read.
- Kind: Rule, with known gaps.
- Change: review a rule for the S20 report, and keep the best-effort quick stop reachable from every console state.
  Cost: evidence and review.
- Evidence: backlog S19, S20; `Actions.h:673-675`.

**8.5 Nothing known stops the drive if the host dies.** The drive has no heartbeat consumer (`1016`) and no
communication-loss action (`6007`). For example, if the MCU crashes during a velocity run, nothing documented makes
the motor stop, so expect it to keep running (not measured).
- Kind: Drive.
- Change: nothing in the library; wire a DI stop input (backlog M15) to a watchdog or a stop button. Cost: wiring.
- Evidence: backlog D10; `docs/reference/vendor/LC_StepDriverCAN_V1.1.eds` has no `[1016]` or `[6007]` section.

**8.6 Reads are gated because their side effects are unknown.** Only reviewed read groups run on the normal path;
other uploads need a one-use grant. For example, you cannot casually read any object you like from the console.
- Kind: Rule (Q20).
- Change: widen the accepted list as reads are shown harmless. Cost: evidence work.
- Evidence: `adapters/canopennode/SdoClient.h:214`; `include/MotorControlCAN/profiles/lichuan_c/Communication.h:16`.

## 9. Configuration and persistence

**9.1 Save and factory restore never ran on a drive.** Both are one-attempt operations and host-tested; no live save or
restore was done, and an ACK does not prove a value survives power-off. For example, you cannot rely on a saved ramp
after a power cycle.
- Kind: Missing evidence (Q15).
- Change: run the Q15 power-cycle trials. Cost: bench time and flash wear.
- Evidence: `include/MotorControlCAN/profiles/lichuan_c/Persistence.h:139`, `:158`; `docs/prompts/PROGRESS.md:98`.

**9.2 Communication setup is minimal.** PDO setup allows only two TPDO1 changes (one map change, and event timer raw 0
or 100). Heartbeat allows only 0 to 1000 ms. There is no node-ID, bitrate or RPDO setup. For example, you cannot map
velocity into TPDO2 or set node 3 from software.
- Kind: Rule and missing evidence (Q16, Q17; prompt 18 Partial).
- Change: general reviewed remapping with readback, and the RPDO map review (M09). Cost: Q17 evidence and PDO sample
  rules.
- Evidence: `include/MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h:36-53`; `Communication.h:113-115`.

**9.3 Most drive settings are read-only.** Writable: motion parameters, soft limits, DI functions and polarity,
direction (`2202`), loop select (`2500`). Not writable: tuning gains and currents, encoder settings (`2501`), free
subdivision; outputs are gated. For example, you cannot change the holding current.
- Kind: Missing evidence (prompts 20 to 22 Partial).
- Change: open rows one by one with their bench check (T1 needs a phase-current probe). Cost: instruments and bench
  time.
- Evidence: `examples/common/ProbeConsole.cpp:192`; `include/MotorControlCAN/profiles/lichuan_c/Tuning.h:153`; `docs/prompts/PROGRESS.md:79-81`.

## 10. Console/CLI and examples

**10.1 The console cannot create evidence records.** Moves, velocity, homing and enable need records that firmware code
supplies through `set...Prerequisites`; typing cannot create one. For example, a new user with only the console cannot
move a motor.
- Kind: Rule.
- Change: an example that loads reviewed records from flash with their drive identity. Cost: a storage format and
  review rules.
- Evidence: `ProbeConsole.cpp:255-268`; `examples/common/ProbeMotion.h:143-176`.

**10.2 The bench is a two-node fixture with per-boot ordering limits.** Bench routes accept only nodes 1 and 2, and
some raw, PDO and evidence experiments are still refused after a route in the same boot (S15). For example, a third
drive on the bench cannot be addressed.
- Kind: Rule (fixture) and Accidental (S15 remainder).
- Change: one `BENCH_NODES` constant checked against capacity; finish S15. Cost: low.
- Evidence: `examples/s2_bench/BenchRoute.h:800-801`; backlog S15.

**10.3 Python campaigns are pending.** The Python client does one typed probe or read; finite campaigns and sessions are
prompt 44. For example, a scripted multi-step test still uses the bench scripts.
- Kind: Planned (prompt 44).
- Evidence: `docs/prompts/PROGRESS.md:103`.

**10.4 Some docs lag the code.** The README says no mode 3 run is retained, though rounds 7 to 12 ran velocity; the
TWAI README says two TX slots, the code has three; the Arduino README says there is no command parser, but the entry
starts the shared console.
- Kind: Accidental.
- Change: a doc pass (prompt 49). Cost: low.
- Evidence: `README.md:30`; `adapters/twai/README.md:43` against `TwaiAdapter.h:90`; `examples/probe_cli/README.md:5`.

## 11. Testing, evidence and qualification gaps

**11.1 Nothing is hardware-qualified.** Bench runs used two drives of one firmware and judged motion from the drives'
own reports, without measuring the shaft. Finite, stress and SYNC/multi-axis qualification (prompts 47, 48, 54) have
not run. There is no S3 or P4 board and no sustained-load or electrical test.
- Kind: Planned (47, 48, 54; H02, T04).
- Evidence: `README.md:41`; `docs/prompts/PROGRESS.md:106-113`.

**11.2 Never run on a drive at all.** Homing, alarm clear, digital outputs, the `2500` loop-select write, save and
factory restore.
- Kind: Missing evidence (H03).
- Evidence: `docs/prompts/PROGRESS.md:80-98` (rows 21, 22, 32, 36, 39); backlog H03.

**11.3 No independent bus observer.** Without a verified listen-only observer, Q18 (when an SDO route may reopen) and
three TWAI questions stay open.
- Kind: Planned (first step of prompt 47; H04).
- Evidence: backlog H04.

## Top 8 changes that would most increase flexibility (value against effort)

1. **Finish the user-set axis count, then B03** (one operation slot per `AxisSession`). Low effort; more axes and about
   40 % less RAM per axis. Already in progress (5.1, 6.1).
2. **Turn existing bench data into measured records where it really establishes the facts, and add typed mode
   selection.** Low code effort; lifts the 60 rpm / 20 s velocity cap and the experiment-only enable, and removes raw
   `6060` writes (4.4, 4.7, 8.2).
3. **Group stop from per-axis stops** (M02). Low to medium effort; the main safety need for several axes (5.2).
4. **RPDO and SYNC sending in the owner** (prompt 51). Medium effort; opens PDO commands, a common start trigger and the
   SYNC trial (2.2, 5.3).
5. **Velocity change during a run, then move retarget** (F02, F03, F04, then F05 to F09). Bench measurement first, then
   medium code (4.2).
6. **Ship the owner, adapters and a trimmed axis session as an optional installed package.** Medium effort; a new
   project no longer copies example code (1.2).
7. **A SocketCAN adapter for a PC.** Medium effort; desktop control and testing against real drives without an ESP32
   (6.4).
8. **TPDO-based motion observation** (M08). Medium to high effort with evidence work (D11, Q17); faster completion
   detection and much less SDO traffic, which also eases the one-frame-per-turn limit (7.1, 7.2).

Later, when the second real profile (N5) is built: a common per-profile interface (1.1) and an owner split for other
protocols (1.3).
