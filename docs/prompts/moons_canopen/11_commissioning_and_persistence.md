# 11 — Commissioning, communication, watchdog and persistence

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 03-04, 08's settings context, 09 and 10. MO-D11, MO-D16, MO-D17, MO-D18 and
MO-D23 are owned here; MO-D09 applies to 0x1010, 0x1011, 0x2020 and 0x2060.

Every operation here is explicit, uses the `prepareCommunication`/`nextCommunication`/`advanceCommunication`
and `prepareSave`/`prepareFactoryRestore`/`nextPersistence`/`advancePersistence`/`verifyPersistence`
families, and retains unknown activation after a lost reply. None runs on startup, after settings or motion.
While COMMON records the owner as away, no communication change or persistent write runs on any bench. The
MOONS' objects this covers are listed in the execution contract's "Hardware" section; COMMON's rule 2 numbers
are the CL86-C fixture's.

## Work

1. Node ID: a 16-position switch sets the low four bits and software sets the upper three (UM21 p9; STFHW p13;
   SSDCHW p12); 0x2020 values 0-7 select 0x00-0x0F ... 0x70-0x7F (UM21 p68), although its text says "high 4
   bit". Activation is stated four ways: power cycle or network reset (UM21 p9), next power cycle after storing
   with 0x1010 (UM21 p68, sub-index not stated), power cycle only (STFHW p13; SSDCHW p12) (MO-D16). Which of
   switch and stored value wins is not stated. Offer an explicit upper-bits change with old/new tuple, endpoint
   reservation, confirmed write and readback, ending "written, activation pending". Storing it and confirming
   the new node ID stay gated until the store sub-index and activation are evidenced; never scan guessed node
   IDs as recovery.
2. Bitrate: switch codes 0-7 select 1000, 800, 500, 250, 125, 50, 20 and 12.5 kbit/s; 8-9 reserved (UM21 p9;
   STFHW p13; SSDCHW p12). 0x2021's value coding is not given (UM21 p68; EDS-TSM 0x2102 defaults to 250, a
   different coding) and HCR p52 says CB code 1 = 800 kbit/s but its example "CB1" sets 1 Mbps (MO-D17). All
   EDS files claim 10 kbit/s and omit 12.5 (MO-D17). No bitrate setter exists until the coding is evidenced;
   resolve the 10/12.5 kbit/s question by vendor clarification or a recorded fixture before 02b's admission
   gains either rate. LSS is unsupported, so no LSS commissioning path exists.
3. Error behavior 0x1029: UM21 p49 gives every sub-index default 1 (no change) and access RO; the EDS files give
   rw with sub 1 = 0 (go to Pre-operational on bus-off or heartbeat-consumer error) and sub 3 = 1 (MO-D11).
   Read and report it; the write stays unsupported until the access conflict is resolved. Its effect on motion
   is undocumented.
4. Heartbeat producer 0x1017 (UINT16 ms, 0 = off, default 1000; UM21 p48): explicit setter with readback. It is
   in the communication store group (UM21 p46), so it persists only after an explicit store.
5. Communication watchdog 0x2060 (UM21 p38, p70): sub 1 enable (default 0), sub 2 status (0 normal, 1 triggered,
   2 armed), sub 3 timeout in ms (default 500), sub 4 trigger messages (bits 0-3 RPDO1-4, bit 4 SYNC; default
   0x0F), sub 5 action (0 stop and disable, 1 stop and stay enabled, 2 and above run Q segments). On timeout the
   motor decelerates with 0x6085 (UM21 p70); afterwards sub 1 reads 0 and sub 2 reads 1, and re-arming writes
   sub 2 and sub 1 (UM21 p38). It is described as "a safety feature in Profile Velocity mode" that "can only be
   set with operation mode" (MO-D18). SDO traffic does not feed it.
6. Implement an explicit `prepareWatchdog` arm/disarm with the whole candidate: timeout, triggers, action 0 or
   1 (2 and above only with a stage 10 qualification for that segment). Refuse arming when no trigger names a
   message the application declares it produces at a period shorter than the timeout. Report armed, triggered
   and disarmed states; a triggered watchdog is a drive event that invalidates motion evidence. Re-arming is a
   new explicit operation, never automatic. Never disable the watchdog implicitly. Its behavior outside
   profile velocity and the meaning of "set with operation mode" stay open for 14a.
7. Store (UM21 p45-p46): 0x1010 sub 1 config parameters and sub 2 communication parameters, signature "save"
   (0x65766173). UM21 p45-p46 lists the groups: stepper config objects (0x2601-0x2617 with gaps), step-servo
   config objects (0x2201-0x2205, 0x2208-0x2215, 0x221A, 0x2220-0x2222, 0x2224-0x2227, 0x2252) and the
   communication objects. Modes, targets, motion parameters and 0x605A are in no group; after restart they are
   re-read, never assumed. Completion evidence: the SDO response, then 0x200B bit 8 "Saving" and
   0x2217 bad-flash bits (UM21 p67, p79) as drive-reported facts; durability needs `verifyPersistence` after a
   real restart. Write endurance is not stated: no repeated store for test coverage.
8. Restore (UM21 p47): 0x1011 sub 1 all, sub 2 communication, signature "load" (0x64616F6C). UM21 p47 lists
   both sub-indexes RO, the EDS files rw (MO-D09 as extended by stage 01); the write stays gated until that
   is resolved.
   Whether a reset is needed and what exactly is restored are not stated (MO-D23). After a restore re-read
   everything; never re-enable, select a mode, start a Q program or replay settings. A restore can move
   node-ID upper bits, bitrate, PDO maps and heartbeat: treat the binding as invalid until confirmed.
9. Configuration written over RS-232 (UM21 p9; STFHW p17-p19) bypasses CAN; a store/restore record states which
   fields were last observed over CAN in this epoch. A node held in Initialization by an attached tool
   answers nothing; post-change checks report `NO_ANSWER`, not a failed change.

## Deliver

Profile `Communication.h/.cpp` (node-ID upper bits, heartbeat, watchdog, 0x1029 read) and
`Persistence.h/.cpp`; `test/moons_canopen_communication_test.cpp`, `test/moons_canopen_watchdog_test.cpp` and
`test/moons_canopen_persistence_test.cpp`; console routes with parity tests; `docs/commissioning.md` section
for MOONS' (switches, tool hold, activation unknowns); coverage and ledger updates; Doxygen; report.

## Acceptance

Tests cover old/new tuple routing, conflicting owners, lost write reply, readback mismatch, refused store of
0x2020, refused bitrate setter, watchdog candidates (no feeding source, action 2+ without qualification,
re-arm explicit only, triggered event invalidation), store and restore signatures, lost replies, restore
invalidation and no automatic retry or save. Quick and full verify, firmware builds and exact-commit CI pass.
Restart-dependent checks need restart control on an identified MOONS' fixture. Hardware: NOT RUN.
