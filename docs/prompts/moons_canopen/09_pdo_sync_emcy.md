# 09 — PDO, SYNC, EMCY and network events

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 03-04. MO-D10, MO-D12, MO-D13, MO-D14 and MO-D26 are owned here; MO-D19 for
EMCY payloads; MO-D11 for the missing heartbeat consumer. Consume prompt 51's SYNC producer, prompt 17/18's
PDO services and prompt 55's evidence classes; do not fork them. Claim or extend seam S13 (item 9) under the
[seam table](../canopen_shared/README.md) rules; S11 and S12 are not needed (item 3).

## Work

1. NMT (UM21 p13-p14): start 0x01, stop 0x02, enter pre-operational 0x80, reset node 0x81, reset communication
   0x82; the node enters Pre-operational on its own; SDO, SYNC, TIME and EMCY work in Pre-operational and
   Operational, PDO only in Operational. Admit the `nmt` route for MOONS' bindings only as explicit commands;
   none is sent on startup, probe or recovery. An NMT stop is not a stop. What a transition to
   Pre-operational does to an active motion is not documented; report it, claim no motion effect.
2. Heartbeat and boot-up: the drive produces heartbeat on 0x700 + node, 0x1017 default 1000 ms (UM21 p48);
   states 0, 4, 5, 0x7F (UM21 p13). Boot-up invalidates every cached fact, qualification and queued work of the
   binding. The drive has no heartbeat consumer: 0x1016 is absent from UM21 p40 and all EDS files, although
   UM21 p49 mentions heartbeat consumer errors (MO-D11). Host heartbeat observation proves no drive reaction.
3. Unsupported services: node guarding (0x100C/0x100D listed only in AMP10 p15, in no EDS; MO-D12), LSS
   (`LSS_Supported=0`), RTR (TPDO types 0xFC/0xFD "not supported", UM21 p53; AMP10 p51-p52 describes RTR for
   the 2010 generation, MO-D13) and TIME (listed on UM21 p14, but no 0x1012 object; MO-D26). The profile never
   sends guarding RTR, LSS or TIME to a MOONS' node; requests for them reject before TX. S11 (host heartbeat
   producer) does not help MOONS': without a 0x1016 consumer the drive cannot supervise a host heartbeat, and
   its only documented host-loss reaction is the 0x2060 watchdog fed by RPDOs or SYNC (UM21 p38, p70; stage
   11). S12 (owner-admitted guarding RTR) is not needed: UM21 and no MOONS' EDS declare 0x100C/0x100D.
   MOONS' claims neither seam.
4. PDO resources: four RPDOs and four TPDOs, eight entries each, granularity 8, no MPDO or dynamic channels
   (EDS DeviceInfo; UM21 p40, p52, p54). Supply them to `PdoProfile` from the ledger per product.
   Default COB-IDs 0x180/0x280/0x380/0x480 + node (TPDO) and 0x200/0x300/0x400/0x500 + node (RPDO)
   (UM21 p50, p53).
5. Default maps are not evidence of the live map: UM21 p52/p54 show one entry per PDO, the EDS files two-entry
   RPDO2/3 and TPDO2/3, AMP10 p52 16- and 8-bit entries, and UM21 p12 a TPDO1 matching no default (MO-D14).
   Decode a PDO only after `pdoMapFromReadback` of that node in this epoch. UM21 p90's example maps 0x700A,
   which the UM21 dictionary lacks; never copy it.
6. Transmission and timing (UM21 p51, p53): TPDO types 0, 1-240, 0xFE, 0xFF; RPDO types 0-240, 0xFE, 0xFF;
   inhibit time in 100 us (TPDO1 default 100), event timer in ms. Change-of-state masks 0x2040-0x2043 sub 1/2
   select which bytes trigger an event TPDO (UM21 p69) and are read, not written, by default.
7. Remapping: invalidate with COB-ID bit 31, sub 0 = 0, entries, sub 0 = n, validate; allowed in
   Pre-operational and "if supported" in Operational (UM21 p52, p54). Admit remapping only in Pre-operational as
   host policy, through prompt 18's partial-effect evidence; never append the "save all parameters" step of
   UM21 p90. A remap of a reused route keeps old-semantics samples unqualified (root AGENTS PDO rules).
8. SYNC objects: 0x1005 default 0x80; whether the drive can produce SYNC is not stated, and setting bit 30 on a
   non-producer is answered with abort 0x06090030 (UM21 p43). 0x1006/0x1007 are "in ms" on UM21 p44 against
   CiA microseconds (MO-D10): read raw, never write. 0x1019 is 0 or 2-240 (UM21 p48).
9. Seam **S13, SYNC side-effect admission** ([seam table](../canopen_shared/README.md), row added at
   `25af57eb`; implemented by the first family whose devices act on SYNC; it gates prompt 51's producer
   without forking it). Before coding, recheck the table and the Git log. If S13 is unclaimed, the lead records
   the claim (lead, stage 09, date) before any shared edit and lands it in one focused tested commit, recording
   its commit ID, tested interface and consumer gate; if another family has claimed it, extend it with the
   MOONS' entry or wait, never build a parallel gate. A SYNC is a broadcast. The MOONS' reviewed SYNC effects:
   on a node in mode -2 every SYNC starts the selected Q segment (UM21 p36; AMP10 p48-p49); a SYNC feeds the
   watchdog when 0x2060 sub 4 bit 4 is set (UM21 p70); PDOs with transmission types 0-240 act on SYNC (UM21
   p51, p53), known only from this epoch's map readback (item 5). A MOONS' binding is SYNC-safe only when
   0x6061 was read as a value other than -2 after the node's last boot-up and this owner has not written 0x6060
   since; otherwise S13 refuses SYNC production on that interface with a typed reason. 0x6060 is in no save
   list (UM21 p45-p46), but the power-up mode is not stated, so the read after each boot-up is required. Until
   S13 has landed and passed, the MOONS' `sync` route stays Blocked and no MOONS' binding shares an interface
   with a SYNC schedule (execution contract); S13's tests cover both directions. Record the shared-interface
   analysis that canopen_shared 02 requires for a second CANopen family in one domain (node IDs, configured
   COB-IDs, bitrate, NMT broadcasts and SYNC effects on both families), with S13 as its SYNC part. The owner
   arms only SYNC COB-ID 0x80 ([SYNC contract](../../sync.md)), which is also MOONS' 0x1005 default (UM21
   p43), so a host SYNC schedule for Lichuan PDOs reaches every MOONS' node on that interface.
10. EMCY: COB-ID 0x1014 = 0x80 + node, RO (UM21 p47); history 0x1003 with eight entries (UM21 p42). No MOONS'
    document lists EMCY codes or bytes 3-7 (MO-D19). Decode only the CiA layout (code, error register, raw
    bytes 3-7; principles s8) and label the meaning unknown. An EMCY is a node event: presence, no success, no
    miss, no drive-state change; the fault comes from the statusword and 0x603F.
11. SDO: one server channel 0x600/0x580 + node (UM21 p49). Only expedited transfers are documented (UM21 p11);
    record segmented support per firmware as stage 03 observes it (MO-D27). The only documented abort is
    0x06090030; keep every abort raw.

## Deliver

MOONS' `PdoProfile` data and map validation rows; network-event, PDO and SYNC admission integration;
`test/moons_canopen_pdo_test.cpp`, `test/moons_canopen_network_test.cpp` and
`test/moons_canopen_sync_hazard_test.cpp`; console `nmt`, `heartbeat`, `network`, `pdo` and `sync` routes for
MOONS' bindings with parity tests; S13 claimed and landed, or extended with the MOONS' entry, with its commit,
tested interface and consumer gate in the seam table; Doxygen; report.

## Acceptance

Tests cover boot-up invalidation, NMT commands without motion claims, refused guarding/LSS/TIME/RTR, map
readback before decode, remap in Operational refused, partial remap, EMCY raw decoding, and, through S13,
SYNC production refused with a mode -2, unread or post-write MOONS' node and admitted after a fresh non -2
read, both directions with a Lichuan axis on the same interface. Quick and full verify, firmware builds and
exact-commit CI pass. Hardware: NOT RUN.
