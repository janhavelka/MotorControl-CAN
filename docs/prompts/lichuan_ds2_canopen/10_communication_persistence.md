# 10 — Communication settings and persistence

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03, 08's settings context and 09. DS-D20, DS-D22 and DS-D23 belong here; DS-D06,
DS-D07 and DS-D32 constrain it. MULTIDRIVE rows 38, 39 and 61 apply. Most of this stage delivers evidence-based
UNSUPPORTED results; the one write path is `1017`.

## Work

1. Node ID (DS-D22). The node comes from DIP SW1..SW3 (1..7) on DS2, DS2-Mini and ISL (DS2C p6-7, MINI p8-9, ISL60
   p9) or SW1..SW5 (1..31) on IBL (IBL60 p7 lists the switches in reverse order against IBL80 p7); all OFF is
   "custom", and no source says it selects `P0C-01` (1..255, default 1, after a power cycle; DS2C p22). `P0C-01` has no
   CANopen index and LSS is absent (`LSS_Supported=0`). The binding takes a caller-declared node 1..127;
   `prepareCommunication` for a node change returns UNSUPPORTED with the reason.
2. Bitrate (DS-D23). `P0C-08` selects 1 Mbit/s, 500 k (default), 250 k or 125 k after a power cycle (DS2C p23); it has
   no CANopen index and no DIP selection exists. DS2EDS also advertises 50 k. The binding takes a caller-declared
   bitrate from the four documented values intersected with the host adapter's support; 50 k stays unsupported until
   measured. A bitrate change is UNSUPPORTED.
3. COB-IDs. Writes to `1005`, `1014`, `1200` and `1400`-`1403:01`, `1800`-`1803:01` are refused: the manual marks PDO
   COB-IDs RO and `1014`/`1200` claims conflict (DS-D06, DS-D07). Routes use values read live in 09.
4. Heartbeat producer. `prepareCommunication` for `1017` (U16 ms, RW/S, default 0; DS2C p24) reads the prior value,
   writes the requested period, reads it back and leaves a restore record. "S" suggests the value may be stored;
   whether it survives a power cycle is unknown, so the result never claims a volatile change. Zero disables the
   producer. The accepted range beyond U16 is undocumented: send any requested 0..65535 value, keep a checked abort
   as `PROTOCOL_REJECTED` with its raw code, and never retry another value. A heartbeat change invalidates the node's
   freshness evidence ([prompt 73](../73_timing_freshness_guards.md)).
5. Save and restore (DS-D20). No `1010` or `1011` exists in any manual table or EDS. `P02-31` (1 restore factory
   values except P00/P01, 2 clear fault records, 3 save the 6000-group parameters; DS2C p16) has no CANopen index and no
   completion signal, and the "save" and "clear position" sections are broken references (DS2C p3).
   `prepareSave` and `prepareFactoryRestore` return UNSUPPORTED with the reason; never write `1010`/`1011` or `2300`
   to a DS2 binding, and never probe for them.
6. Volatility. `P02-31 = 3` implies 6000-group writes are not stored by themselves; that is an inference. `P0A-03`
   power-loss save (DS2C p21) and "RW/S" columns leave scope unknown; `P0C-13` concerns Modbus writes only (DS2C p23).
   Every write in this profile keeps "persistence unknown" until 12a's restart test for that object and firmware.
7. Resets. NMT reset node and reset communication are explicit 09 operations whose DS2 effects are undocumented
   (DS-D32); results invalidate all dependent evidence, routes and queued work. `P0D-00` software reset is
   unreachable. DS2OP p9 asks for at least 10 s between power-off and power-on (Er.410); record it as a fixture
   procedure, not a library delay.
8. Vendor tool. Node, bitrate, control mode, motor codes and saves are documented through the USB debug software
   (DS2OP p5-7, ISLMP). The library never runs, wraps or emulates it.
9. Restore records. Settings written by 06b, 08 and this stage leave restore records in the shape of
   [prompt 57](../57_regular_drive_operations.md)'s configuration restore, keyed by profile and node. A restore is its
   own intent with uncertain effects and never replays a motion or enable word. The node, bitrate, PDO-map and save
   mechanics that [prompt 61](../61_drive_configuration.md) adds for Lichuan C are per-profile; DS2 refuses each of
   them before any work until DS2 evidence exists (MULTIDRIVE row 61).

## Deliver

`Communication.h`/`Communication.cpp` with `CommunicationContext` for `1017` and explicit UNSUPPORTED preparations
for node, bitrate, COB-ID, save and restore; `test/lichuan_ds2_communication_test.cpp` and
`test/lichuan_ds2_persistence_test.cpp`; owner-backed `ds2 communication heartbeat <ms>` and refused routes with
reasons; `docs/commissioning.md` DS2 section, coverage, operation map, Doxygen and CHANGELOG.

## Acceptance

Tests cover the `1017` prior read, write, readback and restore record, a lost write keeping execution unknown with no
retry, freshness invalidation, and UNSUPPORTED results without TX for node, bitrate, COB-ID, save and restore. No DS2
test or route writes `1010`, `1011` or any `2xxx` object; 06b's IBL `2406h` write stays withheld until its evidence exists. Quick and full verification and required CI on the exact
final pushed commit pass. Restart-dependent facts need a DS2 fixture with restart control; hardware NOT TESTED.
Commit, push and hand off.
