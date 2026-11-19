# 10 — Communication settings and persistence

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03, 04, 08 and 09. NT-D04, D05, D06, D09, D21, D23 and D28 are
central. Follow the [commissioning guide](../../commissioning.md) and
[prompt 61](../61_drive_configuration.md); every operation here is explicit,
`MOTION_LIKE` or `NOT_REPEATABLE`, and never run at startup, probe or recovery.

## Work

1. Node ID and bitrate are not plain object values. N5: two hex rotary switches
   (S1 high, S2 low); 0 gives 2009h at fixed 1 Mbit/s, 1-127 that node at 1 Mbit/s,
   128 gives 2009h and 2005h, 129-255 give value - 128 with 2005h; position 1 forces
   node 1 at 1000 kbit/s (N5 p37, rendered). Load order: default, stored value,
   configuration file, switch (N5 p37; the configuration file is undescribed for N5
   and PD4-C, NT-D23). C5-E uses S2/S3 (C5E pp34-35); PD4-C one 16-position switch and
   the offset 2006h (PD4C pp28-29, p238); CL4-E has no switch, objects only, and a
   default pad (CL4E p28, pp34-35). Switch positions are caller declarations; the
   profile reports which source governs and refuses a setter that the declared
   switch position overrides. The default node is 7Fh by preset but node 1 by text
   (NT-D06): never assume either.
2. 2005h codes (N5 pp250-251, rendered; p38; NT-D04): 81h-86h are 10-500 kbit/s in
   every source; 88h is 1000 kbit/s on the object page, the CL4-E table, the FIR-v1650
   table and every EDS; the 3.3.0 commissioning tables (N5 p38, C5E p35, PD4C p29) give
   78h = 100 and 87h = 1000. The object page reads any unlisted value as 1000. The
   setter admits 81h-86h and 88h only; 78h and 87h are refused with NT-D04 until a
   fixture or vendor answer resolves them.
3. A node or bitrate change: write in RAM, store with 1010h:0Ah = 65766173h, then take
   effect after a power cycle or NMT Reset Communication (N5 p37). Require the old and
   new tuple, endpoint reservation, the activation plan, immutable deadlines and a
   post-change identity read at the new tuple. A lost reply leaves activation unknown;
   never replay or scan guessed settings. Owner absence forbids these changes on any
   bench. 2007h:01 (boot-loader boot-up) and 1F80h (automatic Operational) are explicit
   setters of the same kind. CL4-E 2102h enables fieldbuses (preset 0019000Bh, CL4E
   p294) and may disable CANopen itself: refused. FIR-v2538 2007h:02 stays refused.
4. LSS (CiA 305) is documented in full (N5 pp134-146; bit timing indexes 0, 2, 3, 4, 6,
   7, 8), named as a commissioning path (C5E p35, CL4E p35, PD4C p29), and works only at
   switch positions that read the objects, yet every EDS has `LSS_Supported=0` (NT-D05).
   The frame and example errors of NT-D21 stand. The library has no LSS service:
   keep every LSS service a manual-only row, refused, owned by a separate shared
   assignment that needs a fixture's answer first.
5. Persistence (N5 pp110-116). Category saves only: 1010h:02 communication, :03
   application, :04 customer, :05 drive, :06 tuning, :0Ah CANopen, :0Bh Modbus (CL4-E),
   :0Ch Ethernet (N5). 1010h:01 saves all categories except CANopen and Ethernet. The
   value reads back 1 when the save finished; the SDO reply alone is not completion.
   Saving needs standstill and must not overlap a start (N5 p114), may disturb the
   fieldbus and take "a few seconds", and power must not fail meanwhile (N5 p115).
   Host policy: refuse saves in Operation enabled; one attempt; poll the subindex
   within a caller deadline; a timeout or lost reply leaves the stored state unknown.
   After a save, read 1001h and 1003h: errors 14, 16 and 21 mean full or damaged
   memory, and 16 resets all saved objects to defaults (N5 p163).
6. The application category stores command objects: 6040h, 6060h, 607Ah, 60FFh, 6071h,
   6042h, 60FEh, 2290h, 2300h, 6007h, 3700h, 3701h and 2800h (N5 pp112-113, rendered).
   A save of 1010h:03 or :01 therefore persists whatever those hold. Before such a save,
   read and present them, require 6040h with bit 4 = 0, 2300h bit 0 as the caller
   intends, 60FEh outputs and brake bit as intended, and explicit caller acceptance of
   the persisted values. The manuals do not say what a restored 6040h does at boot.
7. PDO configuration belongs to communication 1010h:02 by the category list, while the
   remapping procedure saves with 1010h:03 (NT-D09). Keep the PDO save gated until a
   restart test on a fixture shows which category retained the map.
8. Restore and restart. 1011h:nn = 64616F6Ch discards saved data, effective after a
   restart; 1011h:01 keeps tuning, CANopen and Ethernet (N5 pp115-116). Restart with
   2800h:01 = 746F6F62h, optional delay 2800h:02 (N5 p115, p297), or NMT Reset Node;
   both invalidate every generation, and neither is assumed equal to the other.
   1020h is a configuration stamp reset to 0 by any later write of a savable object
   (N5 p116, pp180-181): read it to report "saved configuration unchanged" or "changed
   since save", never as proof of the restored values. On C5-E and CL4-E a USB cfg.txt
   is applied at every boot and wins over saved values (C5E p108; CL4E p106): the
   caller declares whether one exists. Restoration records never replay a motion
   trigger, enable, homing or NanoJ start. Never repeat saves for test coverage.

## Deliver

`profiles/nanotec_canopen/Communication.h/.cpp` (`prepareCommunication`,
`prepareCommunicationConfirmation`, `nextCommunication`, `advanceCommunication`) and
`Persistence.h/.cpp` (`prepareSave` per category, `prepareFactoryRestore`,
`nextPersistence`, `advancePersistence`, `verifyPersistence`, restart);
`test/nanotec_canopen_communication_test.cpp` and
`test/nanotec_canopen_persistence_test.cpp`; console routes `nanotec communication`,
`nanotec persistence` and `nanotec restart` with parity tests; coverage rows for LSS.

## Acceptance

Tests cover switch-governed refusals, every 2005h code, old/new tuple routing, lost
replies with unknown activation, the application-category snapshot gate, completion
by readback of 1, timeouts during a save, post-save error checks, restore without
restart, restart invalidation, 1020h interpretation and no automatic retry or save.
Quick and full verification and CI on the exact final pushed commit pass. Restart and
persistence evidence needs restart control on a Nanotec fixture; NOT RUN otherwise.
