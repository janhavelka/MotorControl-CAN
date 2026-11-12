# Prompt 53 Multi-axis operation, configurable capacity and group stop

Apply [COMMON.md](COMMON.md). Prerequisites: [42](42_cli_motion_routes.md), [45](45_fault_integration.md), [51](51_sync_producer.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "i want the lib to be able to do multi-axis, but to be able to stop one
motor at the time also - check all the datasheets and other existing libraries for how they approach this
functionality and mimick it in here"; "i dont want the capacity to be fixed at 2 - make it as large as the underlying
system/mcu allows"; "stop-all rules etc - make a prompt for that or apply it right now if simple"; "i think the
library should allow also psram allocations if feasible (future s3 projects/pcbs) at compile time to allow control of
user-set amount of drives".

Deliver several axes operating concurrently from one application, each stoppable on its own at any time, a group stop
with explicit rules, capacities chosen by the application at compile time up to what the target allows, and storage the
application may place in PSRAM where the target has it. This follows the [multi-axis and SYNC survey](../reports/2026-11-12_multiaxis_sync_survey.md),
the [multi-drive preparation](../reports/2026-11-12_multidrive_preparation.md), the
[capacity configuration study](../reports/2026-11-12_capacity_configuration.md) and the [S2 memory report](../reports/2026-11-12_s2_memory.md).

Group stop rules (backlog M02), to implement and test. No surveyed stack has a CiA402 group stop (their group actions
are NMT stop or reset, which are not motor stops), so the application chooses the axes and the library keeps HALT and
QUICK_STOP distinct (ros2_canopen's "halt" is a quick stop; Lely's default reset after a heartbeat timeout is rejected).
(1) A group stop is one per-axis stop request per axis, each with its own result, never one shared result; (2) never an NMT stop as a motor stop; (3) the group outcome is success
only when every axis is observed stopped by its own settlement rule; any axis unconfirmed keeps the group uncertain;
(4) one axis's refusal, loss or fault never delays or cancels the others' stops; (5) a group stop never re-enables,
replays or clears faults; (6) the group stop has its own reserved capacity and output so ordinary work cannot starve
it; (7) after a group stop, every axis's earlier motion stays as uncertain as its own evidence says.

The user asked (2026-11-12) whether a network-level stop, or a network stop first and then individual stops, would be
better, and whether the library can stop the motors from a signal on the drives' inputs. Recorded answers and work:

- **NMT Stop is not used, before or instead of the per-axis stops.** In the CiA 301 Stopped state a node serves
  neither SDO nor PDO, so stop commands sent after it are not acted on until the application sends Pre-operational or
  Start. What the motor does on NMT Stop is the CiA402 `6007` abort-connection option, which the Lichuan EDS does not
  have; the manuals say nothing. Measured so far: NMT Stop to a released node 2 changed neither its CiA402 state nor its
  position (2026-11-06, backlog D15); an enabled or moving drive was never sent one. Its reaction during motion is a
  measurement (backlog M14, in prompt 54), never a library stop path.
- **One frame to all drives, a shared quick-stop RPDO (backlog M16).** CANopen lets several drives consume one PDO.
  One RPDO with the same COB-ID on every axis, a controlword-only map (after the M09 review), asynchronous type 255 and
  volatile configuration would let one frame carry `000B` to every drive at the same instant. Only the quick-stop word
  is allowed: in the generic CiA402 state machine `000B` (bit 2 clear) never leads to an enabled state, while a shared
  `010F` HALT would enable a drive that is switched on but not enabled, so it is refused. The Lichuan manuals document
  `000B` only from `000F`, so its effect from the other states is part of the measurement. Each axis's result still
  comes from its own observation (rules 1 and 3), and the per-axis SDO stops stay available. Experiment scope until
  measured.
- **Wired stop input (backlog M15).** DI function 5 (stop) or 6 (emergency stop) on each drive (EN86/ZH86 p8),
  wired in parallel to one signal (a button, a relay or an MCU output), stops every drive without CAN or the MCU's
  software. The library already writes these assignments (`prepareIoSettings`, DI0..DI6, codes 0..13, never saved; one
  field measured as lost at a power cycle) and reads the input states (`2101`). The installed core never drives MCU pins (no device I/O); an example
  may own a stop output on its own GPIO. The drive's reaction (ramp, final state, alarm, what re-enabling needs, active
  level) is undocumented and needs a wired fixture, a physical action by the user. Neither the input nor any protocol
  command is safety-rated; a certified emergency stop removes motor power by machine design.

Boundaries: the installed core stays C++11, heap-free and caller-owned: capacity is a compile-time application choice
(template parameter, configuration header or caller-provided storage, as the capacity study recommends), never
allocated by the core; one owner per physical interface; axis identity independent of node IDs; CANopen node IDs
1..127 and bus time remain real limits; the S2 has no PSRAM and keeps internal-RAM storage.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its `BusStorage`, per-axis stop and urgent-class
code; RS is authoritative for common motion names and has no group API, so example-level names must not imply an
RS-backed meaning. Inspect FieldCore-node read-only for ownership and multi-device integration. If HEAD, dirty state and
seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a
behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or
verification rules. Read the owner, adapter, `ProbeApp`, `ProbeConsole`, `ProbeMotion`, `AxisSession` and bench sources
the capacity study inventories, and the ESP-IDF PSRAM placement documentation it cites. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Configurable capacity: the installed core has no axis capacity; the adapters and examples do (`SdoClient::CHANNELS`,
   `SEQUENCES`, `REQUESTS`, `PRODUCERS`, `PDO_ROUTES`, `ProbeApp::axes_`, bench nodes). As the capacity study
   recommends, one adapter capacity header with today's values as defaults, overridable from the application's build
   (CMake, IDF Kconfig, PlatformIO flags), checked by `static_assert` and a link-time tag that makes mixed builds fail
   to link; RS `BusStorage` spans remain the upgrade path for per-interface capacities (record that decision). Fix the
   latent `CanBusOwner.h` `active_{-1, -1}` initialiser, which is wrong for any channel count but two. Build and test
   with N = 1, 2, 8 and 32, measure the owner's turn cost at N = 32, and state the host TX limit: the TWAI adapter sends
   at most one frame per service turn, so stopping N axes needs at least N turns.
   **Delivered early on 2026-11-12 at the user's request** ("Relax the limitation on axis in the can adapter and
   example"): `adapters/canopennode/Capacity.h` with the five macros, CMake cache variables, the shared IDF
   `Kconfig.capacity`, PlatformIO `build_flags`, the `CapacityLinkTag` link check, the `active_` fix, `BENCH_NODES`,
   and the `capacity`, `capacity_n1`, `capacity_n8`, `capacity_n32` and `capacity_mismatch` tests (see the
   [owner guide](../can_owner.md) and CHANGELOG). Decisions: a configuration header, not RS `BusStorage` spans
   (spans stay the upgrade path for per-interface capacities or split placement); the definitions are PUBLIC on
   `MotorControlCANCanopenNode`; discovery targets are at most 28 (nine services per target in an 8-bit budget).
   Still owed here: (a) the owner turn cost grows faster than N (host, every slot queued: 0.5 us at N = 2, 4.7 us at
   8, 183 us at 32); about 95 % is the expiry scan that `CanBusOwner::transmit` repeats for every channel. A
   candidate (return before `expire` when the channel has no staged frame) brings N = 32 to about 15 to 19 us with
   every native test unchanged, but it can delay an expiry or invalidation by one turn, so it needs a review of
   expiry ordering before it is applied, then a board `maxServiceGapUs` measurement; (b) eight existing native tests
   hard-code the default capacity and fail when the whole suite is built with another N (`twai_adapter`,
   `discovery_bench`, `can_bus_owner`, `can_bus_recovery`, `can_bus_health`, `axis_session`, `network_events`,
   `discovery_scan`); decide whether they derive their counts from the constants or stay default-only.
2. Storage placement: the application owns the storage objects, so an S3 application can put cold, bulky storage in
   PSRAM by allocating it once at startup (`heap_caps_aligned_alloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`) with an
   internal-RAM fallback; `EXT_RAM_BSS_ATTR` needs `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY`, makes PSRAM mandatory to
   boot and silently stays internal without that option, so it is not the default way. Receive, transmit, stop paths and
   SDK callback, ISR and driver storage stay in internal RAM. Document the rule and add an S3 PSRAM compile fixture.
   Apply the S2 savings (backlog B01 to B03) where they are example or configuration changes.
3. Concurrent independent motion: two or more axes moving at once through one owner, each with its own session and
   results, and a stop of one axis while the others continue (backlog M01, M03, M06).
4. Group stop with rules 1 to 7 above, its own reserved capacity and per-axis results (backlog M02, M12). Then, as
   explicit experiment scope after the M09 map review, the shared quick-stop RPDO (M16) beside the per-axis stops,
   and an example stop output plus the DI stop-function settings for a wired stop line (M15); host tests for both,
   hardware in prompt 54.
5. Console and bench: independent control of every axis from the console, the user's words (2026-11-12): "i want this
   feature - to be able to stop one axis using the console - pretty much independent control of axes". Each axis has
   its own motion attachment, start, change and stop commands and its own reserved stop slots, so a stop of axis B
   never waits for or disturbs axis A's operation; `stop ID` and `stop all` with per-axis records; paired bench routes
   and runner cap/ledger rules (backlog M01, M03, M11). Every console command goes through the library API (the
   per-axis session of prompt 64 once it exists), so the console adds no motion logic of its own.
6. Synchronized start via prompt 51's synchronous RPDO trigger where its trials succeeded; otherwise a same-turn SDO
   trigger with its measured skew stated as the limit (backlog M07).

## Verify and exit

Host tests cover N-axis owner service and fairness, one-axis stop beside other axes' motion, group stop under loss,
refusal and fault on one axis, capacity exhaustion refusals, and PSRAM-placed storage in a compile fixture. Run the
native (all N variants), package, firmware (S2 and S3, with and without PSRAM), stack and documentation gates and CI on
the actual final commit. Hardware runs belong to prompt 54.

## Documentation and delegation

Update the owner, application, CLI and firmware guides, coverage, backlog M01-M16 and B01-B06, CHANGELOG and Doxygen in
the same stage. Disjoint agents may own the capacity change, the group-stop helper and the console work after the
shared contracts are agreed; a stronger reviewer challenges group-stop and capacity-exhaustion semantics. The lead
integrates and runs combined checks. Record `docs/reports/prompt_53.md`. Commit and sync on the current branch before
handoff.
