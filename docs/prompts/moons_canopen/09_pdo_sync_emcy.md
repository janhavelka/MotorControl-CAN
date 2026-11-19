# 09 — PDO, SYNC, EMCY and network events

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 03-04. MO-D10, MO-D12, MO-D13, MO-D14 and MO-D26 are owned here; MO-D19 for
EMCY payloads; MO-D11 for the missing heartbeat consumer. Consume prompt 51's SYNC producer, prompt 17/18's
PDO services and prompt 55's evidence classes; do not fork them. Claim or extend seam S13 (item 9) under the
[seam table](../canopen_shared/README.md) rules; S11 and S12 are not needed (item 3).

## Work

1. NMT (UM21 p13-p14): start 0x01, stop 0x02, enter pre-operational 0x80, reset node 0x81, reset communication
   0x82; the node enters Pre-operational on its own; SDO, SYNC, TIME and EMCY work in Pre-operational and
   Operational, PDO only in Operational, and a Stopped node serves only node and error control (UM21 p14
   table). Admit the `nmt` route for MOONS' bindings only as explicit commands;
   none is sent on startup, probe or recovery. An NMT stop is not a stop. What a transition to
   Pre-operational does to an active motion is not documented; report it, claim no motion effect. Every NMT
   command, targeted or to node 0, passes the owner's NMT admission under S13 (canopen_shared 02 S13 bullet;
   item 9): until item 9 lands, the S2-commit default deny refuses node-0 NMT on any interface with a MOONS'
   binding, and admitting a command that can change an evaluated node's NMT state revokes the SYNC approval
   before it is sent. An NMT command to a MOONS' binding is a changing request under prompt 57's identity
   policy, never an exempt stop.
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
   UM21 p90. A remap of a reused route keeps old-semantics samples unqualified (root AGENTS PDO rules). Every
   remap runs the write preflight first: a TPDO remap writes 0x1800-0x1803 sub 1/2 and 0x1A00-0x1A03, RW in
   UM21 p53-p54 and the EDS; an RPDO remap or transmission-type change needs 0x1400-0x1403 sub 1/2, RO on
   UM21 p50 (sub 2 UNSIGNED32) but rw (sub 2 UNSIGNED8) in the EDS (MO-D09), so it is refused with zero
   changing TX until stage 02's write policy resolves them. RPDO data transmission needs seam S14 anyway.
8. SYNC objects: 0x1005 default 0x80; whether the drive can produce SYNC is not stated, and setting bit 30 on a
   non-producer is answered with abort 0x06090030 (UM21 p43). 0x1006/0x1007 are "in ms" on UM21 p44 against
   CiA microseconds (MO-D10): read raw, never write. 0x1019 is 0 or 2-240 (UM21 p48).
9. Seam **S13, SYNC and NMT-broadcast effect admission** ([seam table](../canopen_shared/README.md), row added at
   `25af57eb`; implemented by the first family whose devices act on SYNC; it gates prompt 51's producer
   without forking it). Before coding, recheck the table and the Git log. If S13 is unclaimed, the lead records
   the claim (lead, stage 09, date) before any shared edit and lands it in one focused tested commit, recording
   its commit ID, tested interface and consumer gate; if another family has claimed it, extend it with the
   MOONS' entry or wait, never build a parallel gate. Consume the S13 bullet of canopen_shared 02
   ("CANopen-only rules") as written: approval over the whole declared bus population, refusal of unknown,
   stale, harmful or unauthorized effects, revocation during a schedule, zero further SYNC TX, node-0 NMT at
   the owner's NMT admission under the same rule, revocation before an NMT command that can change an
   evaluated node's state, and the default deny landed with S2 (stage 02b, if MOONS' lands S2). The MOONS' entry
   supplies each declared MOONS' node's SYNC and NMT effects and the reads that decide them, taken for the current binding,
   server session and configuration generation (an unread item refuses):
   - Mode: 0x6061 read after the node's last boot-up and after this owner's last 0x6060 write. -2 is a program
     start on every SYNC (UM21 p36; AMP10 p48-p49): always refused, since stage 10 is refusal-only. 0x6060 is
     in no save list (UM21 p45-p46) and the power-up mode is not stated, so the read after each boot-up is
     required.
   - Watchdog: 0x2060 sub 1 and sub 4. Enabled with bit 4 set, the schedule feeds it (UM21 p70): refused
     unless stage 11's armed-watchdog record names this schedule as its feed.
   - PDOs: live NMT state and this epoch's communication and map readback of 0x1400-0x1403, 0x1600-0x1603,
     0x1800-0x1803 and 0x1A00-0x1A03 (item 5). In Operational a valid RPDO of type 0-240 applies whatever was
     received at the next SYNC (UM21 p51): refused unless its producer is admitted with this schedule through
     S14, which has not landed. A valid TPDO of type 0-240 transmits on SYNC (UM21 p53): admitted once its map
     is read back, and counted in the load analysis. The mode and watchdog effects are evaluated in every NMT
     state: UM21 p14 lists SYNC in Pre-operational and Operational, and does not say that a Stopped node
     ignores it for SYNC Q or the watchdog.
   - NMT effects per command, for the owner's NMT admission of node 0 (UM21 p13-p14; an effect not stated
     here is unknown and refuses node-0 NMT on the interface):
     - Start 0x01: the node goes Operational and PDO exchange starts, so a valid RPDO applies received data
       (types 0-240 at the next SYNC, UM21 p51) and a valid TPDO transmits per its type (UM21 p53); no drive
       state or motion effect is stated. Known harmless only when this epoch's readback shows every RPDO of
       the node invalid (COB-ID bit 31 set); a valid RPDO refuses it while S14 has not landed.
     - Stop 0x02, Enter Pre-operational 0x80, Reset node 0x81 and Reset communication 0x82: the state change
       is stated, its effect on running motion, a running Q program and an armed watchdog is not (a Stopped
       node no longer receives the RPDO or SYNC feed, UM21 p14, p70): unknown, refused for node 0.
     A targeted command to a MOONS' binding stays item 1's explicit command and revokes the node's approval
     first, since each of the five can change its NMT state from some state.
   - Revocation: the node's approval is withdrawn before this owner admits a write to 0x6060, 0x2060, 0x1005,
     0x1400-0x1403, 0x1600-0x1603, 0x1800-0x1803 or 0x1A00-0x1A03 or an NMT command that can change the
     node's state (not after its reply), on the shared invalidators, on a heartbeat or boot-up reporting an
     NMT state other than the one evaluated (another NMT master) and on a report that an RS-232 tool was
     attached; only fresh reads, with the NMT state the node then reports, requalify it.
   Until S13 has landed and passed, the MOONS' `sync` route stays Blocked and no MOONS' binding shares an
   interface with a SYNC schedule (execution contract). Record the shared-interface
   analysis that canopen_shared 02 requires for a second CANopen family in one domain (node IDs, configured
   COB-IDs, bitrate, NMT broadcasts and SYNC effects on both families), with S13 as its SYNC and node-0 NMT
   part. The owner
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
readback before decode, remap in Operational refused, the RPDO remap refused by the preflight with zero
changing TX, partial remap, EMCY raw decoding, and, through the real producer, S13's acceptance classes with
MOONS' nodes: admitted only when every declared node's effects are known harmless; refused on the target and
on a node other than the target for mode -2, an unread mode, a non -2 node with an unread or unknown map, a
valid synchronous RPDO without an admitted producer and an enabled watchdog with the SYNC trigger not owned by
the schedule; a 0x6060 write, remap, rebind and NMT reset during a running schedule followed by no SYNC TX
until fresh reads; a targeted and a broadcast Start while a Pre-operational MOONS' node holds a valid
synchronous RPDO (approval revoked before the Start, no later SYNC TX); a heartbeat reporting an unevaluated
NMT state during a schedule (no later SYNC TX); node-0 Stop, Enter Pre-operational and both resets on an
interface with a MOONS' binding (zero NMT TX), also before item 9 lands (the S2-commit default deny); both
directions with a Lichuan axis on the same interface. Quick and full verify, firmware builds and
exact-commit CI pass. Hardware: NOT RUN.
