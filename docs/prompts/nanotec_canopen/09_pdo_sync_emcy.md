# 09 — PDO, SYNC, EMCY and boot-up

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03. NT-D02, D07, D09, D16 and D30 apply. Reuse `Pdo.h`, `PdoState.h`,
`Network.h` and `Sync.h` with a Nanotec `PdoProfile`; fork nothing. See
[sync.md](../../sync.md) and [prompt 51](../51_sync_producer.md).

## Work

1. `PdoProfile` for Nanotec: 8 RPDO and 8 TPDO sets (N5 p127; EDS `NrOfRXPDO=8`,
   `NrOfTXPDO=8` for every member), 8 mapping entries each (EDS [1600] SubNumber=9;
   N5 p209) although the text describes entries 01h-0Fh (N5 p128, p130; NT-D30), byte
   granularity (EDS Granularity=8) and at most 8 data bytes. Reviewed mappable
   objects come from the ledger's PDO columns, not from a CiA 402 list. Size owner
   routes (`MOTORCONTROLCAN_PDO_ROUTES`, per interface) for the routes an application
   binds, not for 16 sets per node.
2. Communication rules (N5 pp127-131): COB-IDs of PDO 1-4 are fixed to the CiA
   defaults and only bit 31 changes; PDO 5-8 COB-IDs are free, with defaults invalid
   (RPDO5 80000000h, TPDO5 C0000000h); a COB-ID change takes effect only after a
   restart or Reset Communication. Mapping and communication objects change only in
   Pre-operational or Stopped, otherwise abort 08000022h (N5 p127). RPDO transmission
   types 00h-F0h apply at the next SYNC, FEh/FFh at once; TPDO types 0, 1-240, FCh,
   FDh, FEh/FFh; inhibit in 100 µs units, preset 0064h = 10 ms; event timer in ms;
   SYNC start value active only when 1019h > 1 (N5 p130, pp210-211, p180; CP-D20).
   Dummy mapping 0002h-0007h is allowed (N5 p129).
3. Default maps are facts to verify by readback, never assumed: RPDO1 6040h + 6060h,
   RPDO2 607Ah + 6081h, RPDO3 6042h, RPDO4 60FEh:01; TPDO1 6041h + 6061h, TPDO2 6064h,
   TPDO3 6044h, TPDO4 60FDh (N5 p131). Note that RPDO1 maps the controlword: in NMT
   Operational any frame on 200h + node writes 6040h, which 04b treats as a control
   path.
4. Remapping uses the documented order (N5 pp131-132): valid bit 1, count 0, entries,
   count, valid bit 0, each step confirmed, in Pre-operational, with retained partial
   effects and no invented rollback. Saving is not part of remapping: the procedure
   says 1010h:03 while the category list puts PDO objects in communication 1010h:02
   (N5 pp111-112, p132; NT-D09); until it is resolved, stage 10's PDO-persistence
   guard (on by default) refuses every category save that could persist a PDO change
   made by this library, and another writer's change is excluded only by 04b item 1's
   single-master declaration (10 items 5 and 7).
   FIR-v2538 adds 2007h:02 "Automatic COB-ID", default 1, undocumented (NT-D02): on
   that row COB-ID writes refuse through stage 13's `FIR2538_AUTOMATIC_COB_ID` gate,
   and maps and COB-IDs are always taken from readback.
   A COB-ID change (the CAN-ID bits of a PDO 5-8 COB-ID; the valid-bit steps of the
   remapping order above are not one) consumes the interface's identifier plan before
   its first write: 11 item 4's binding check, rerun at the write's admission against
   every identifier reserved on the interface (other nodes' services, every configured,
   pending or active PDO COB-ID, S11's producer and any S14 route); a collision refuses
   with zero TX, and the owner then reserves the candidate beside the old ID. That
   check and its atomic reservation land with 11 item 4; until they have landed with
   their tests the COB-ID change is BLOCKED (refused before TX with that reason), while
   the valid-bit steps, remapping and typed readback stay available, and 11 item 4's
   landing commit unblocks it and runs the COB-ID acceptance cases below. The
   binding keeps per PDO a configured value (this library's confirmed write or a
   readback), an active value (the readback taken after the last observed boot-up,
   before any COB-ID write since) and a pending flag while the two differ. A readback
   before activation is the configured value, never proof of the wire identifier: the
   change takes effect only at a restart (N5 p127, p129), saved values reload at
   switch-on or reset (N5 p110) and Reset Communication resets the CANopen settings
   (N5 p119), so whether a RAM-only change survives its own activation is
   undocumented, and the save that would keep it is gated (NT-D09, 10 item 7). While
   pending, after a partial write or a lost reply, both IDs stay reserved, every route
   on either is unqualified and the node's S13 approval stays revoked. A fresh
   readback after a later observed boot-up sets the active value and releases only the
   ID it no longer shows, but only with an activation boundary independent of that
   boot-up's ambiguity: item 8 reporting READY for that boot-up (so the readback comes
   from the firmware session that uses it), or, for a TPDO and that route only, a
   frame observed after that boot-up on the read-back identifier with the confirmed
   map's length. Without either, the application may declare the new identifier
   active after a power cycle or Reset Communication that it records (lead decision
   2026-11-19 under the owner's direction of 2026-11-17: user responsibility,
   settable); the route then becomes active, and every result that uses it names the
   declaration. Otherwise
   the change stays pending with both IDs reserved, its routes unqualified and the
   reason named; pending refuses no other work. A delayed sample on either ID stays
   unqualified until item 5's boundary.
5. PDO observations: decode TPDOs by the observed map and bind each sample to the map
   generation; a host generation is not a wire tag. After boot-up, remap or reset, a
   delayed same-ID same-length sample stays unqualified until an evidenced boundary.
6. SYNC: consumption only. 1005h preset 80h; bit 30 would make the drive a SYNC
   producer (N5 p165) and is never set. Seam S13 (claim if first, or extend), consumed
   as canopen_shared 02 "CANopen-only rules" S13 states it: one approval over every
   declared node on the interface, from live NMT state and RPDO/TPDO communication and
   map reads of the current binding, session and configuration generation, gating
   prompt 51's producer (`armSync` and each `admitSync`) without forking it; prompt
   51's producer stays the only producer. This family's entry classifies each Nanotec
   node's effects (N5 p130): harmless, a synchronous TPDO (types 0-240) or a
   synchronous RPDO mapping no command object (04b item 1's list); harmful, a valid
   synchronous RPDO (types 00h-F0h) mapping a command object, because data it has
   received would apply at the next SYNC and the library cannot see whether any is
   pending, unless the requesting schedule is itself authorized for that RPDO (only an
   assigned 06c with S14); unknown, an unread or stale map or transmission type. The
   NMT state recorded with the approval never turns a harmful RPDO harmless (another
   master can make a node Operational). Harmful or unknown on any node, the target or
   another, refuses with zero SYNC TX; remap, rebind, NMT reset or boot-up, a
   configuration generation change and owner invalidation revoke the approval, also
   during a running schedule. 1006h, 1007h (synchronous window, late PDOs
   rejected with EMCY, N5 p166) and 1019h are read and reported. The manual's SYNC
   figure shows CAN-ID 80h + node (N5 p119) against 1005h = 80h (NT-D07): use 80h.

   The NMT part of the same S13 rule replaces 02b's default deny for Nanotec nodes: a
   node-0 NMT command on an interface with a Nanotec binding is admitted at the
   owner's NMT admission only when every declared node's effect is known and admitted;
   an unlisted effect or unread or stale evidence refuses with zero NMT TX. This
   family's per-command effects, from fresh reads of the current binding, session and
   configuration generation:

   | Command (N5 p119) | Effect on a Nanotec node | Admitted on that node only when |
   | --- | --- | --- |
   | 01h Start | PDO becomes active (N5 p118): valid RPDOs apply received data, and RPDO1 maps 6040h and 6060h by default (N5 p131) | Fresh maps show no valid RPDO mapping a command object (04b item 1's list), or 04b item 1's RPDO row admits for that node |
   | 80h Enter Pre-operational | PDO stops; SDO, SYNC and EMCY stay active (N5 p118); no page gives the drive's reaction to the lost RPDO data | Fresh maps show no valid RPDO mapping a command object |
   | 02h Stop | Only NMT stays active (N5 p118): no SDO stop or EMCY can reach the node, and no page gives a motion reaction (an NMT Stop is not a quick stop) | A fresh CiA 402 state without torque at the motor (not Operation enabled, Quick stop active or Fault reaction active, N5 p57) |
   | 82h Reset Communication | CANopen settings reset and communication restarts (N5 p119); boot-up and item 8's invalidation follow | A fresh state without torque, as for Stop |
   | 81h Reset Node | Complete restart (N5 p119): saved values reload (N5 p110), a saved 6040h or 6060h included (N5 pp112-113); 1F80h = 8 enters Operational (N5 p250); whether a stored NanoJ program then starts is unknown (NT-D32) | Never while NT-D32 is open: the effect is unknown |

   Admitting any NMT command that can change an evaluated node's NMT state revokes the
   SYNC approval before the command is sent (canopen_shared 02 S13). A targeted NMT
   command to a Nanotec binding remains its own changing request (prompt 57 "Identity
   policy": never a stop; `MOTION_LIKE`). The owner's targeted NMT route admits it
   only under prompt 57 "Admission by outcome" and this table's row for that command
   evaluated on the addressed node alone, from fresh reads of its binding: a targeted
   Stop or Reset Communication needs a fresh state without torque, a targeted Start or
   Enter Pre-operational the row's map condition, Reset Node refuses while NT-D32 is
   open, and a command without a row refuses, each with zero NMT TX. Both admissions,
   targeted and node 0, are rechecked when the command is handed to the controller
   (canopen_shared 02 S13): a binding change, stale evidence or a conflicting
   operation on an evaluated node (10 item 5's save reservation included) retires a
   not-yet-sent command, and one already handed off is reported possibly sent. The
   one exception is prompt 57's bootstrap command, applied by reference: a targeted Enter
   Pre-operational to a Nanotec node whose fresh heartbeat or node-guarding state
   reports Stopped is admitted under every outcome except CONTRADICTED, a current
   outcome absent included, without the map condition (no SDO answers in Stopped, and
   leaving Stopped for Pre-operational activates no PDO, N5 p118), and its result
   records the reason. N5 does not show
   heartbeat or guarding activity in Stopped (N5 p118, NT-D17), so such a report may
   never arrive; without it the node is reported stranded and its recovery is the
   application's (prompt 57). Nothing sends this command implicitly.
7. EMCY (N5 pp120-121, pp161-165): 80h + node; error code, 1001h, manufacturer error
   number, padding; an error-cleared EMCY "00 00 E-REG E-Number" when no error
   remains. Decode with the N5 error-number table (watchdog reset 0, fieldbus 4,
   node guarding 6, following error 13, PDO timeout 17, PDO length 19 and 20, rated
   current must be set 22, unexpected sync length 27, slippage 30). 1014h bit 31 would
   disable EMCY and is never written. 1003h holds error number, class and code, up to
   eight entries although the text speaks of seven shifting (NT-D16); subindex reads
   abort 08000024h while empty (N5 p163).
8. Boot-up: the boot loader sends its own boot-up before the firmware's unless
   2007h:01 (BL Config, savable in category CANopen, preset 0) = 1 (N5 p132, pp251-252),
   and 2084h delays boot-up (preset 0, N5 p275). Each boot-up invalidates dependent
   evidence, queued work and declarations (04b). Counting boot-ups is no readiness
   rule: with 2007h:01 = 1 only one arrives, and one can be missed. Three facts stay
   separate: a responding server (any SDO reply), the product identity outcome (03,
   prompt 57) and firmware readiness. Whether the boot loader answers SDO is not
   documented: N5 p132 and p252 describe only its boot-up message, and neither 1F50h,
   1F51h and 1F57h (pp246-249) nor 4042h (p344) documents a value that tells it from
   the firmware. A fresh identity read answered after the last observed boot-up
   therefore re-establishes the identity outcome, not readiness. Readiness is READY
   only when that boot-up cannot be the boot loader's, which needs 2007h:01 = 1 in
   force at that boot-up (only the firmware then sends a boot-up, N5 p252): a read of
   2007h:01 = 1 on the binding taken after the preceding observed boot-up, with no
   2007h write, CANopen-category save or 1011h restore by this library from that
   boot-up to the boot-up in question, and 04b item 1's single-master declaration in
   force throughout, so that
   the value read is the saved one that reloads at the next boot (N5 p110). A RAM-only
   1 written by this library or another writer is not that evidence, and with the
   single-master guard off the read never is. A single unanswered identity request
   between two boot-ups is fixture evidence only (12A Q2): it cannot tell a boot loader
   without SDO from timing or a lost frame, and never makes READY. Only a positive,
   reviewed observation that distinguishes the firmware session on that model and
   firmware row, recorded in the ledger, could add a path; none is known. Otherwise it is UNKNOWN and every result after
   that boot-up reports it with the reason. UNKNOWN refuses nothing by itself (no
   firmware lock; an application may require READY as its own policy, as 09b item 4
   does for supervision); reads, reviewed diagnostics and prompt 57's stop rules stay
   available either way. No delay, retry, boot-up count or firmware-build list makes it
   READY; each later boot-up resets it. The manuals give no boot duration (review
   section 11), and the spacing is 12A Q2's measurement. 1F80h = 8
   enters Operational automatically (default 0, N5 p250). NMT commands follow N5 p119; the "000 | 01 2A
   completely restarted" example mislabels Start Remote Node (NT-D07).

## Deliver

`profiles/nanotec_canopen/Pdo.h` content from the generator plus `PdoConfig.h/.cpp`
for typed map readback and explicit remapping; EMCY and boot-up decoding in the
profile's event module; `test/nanotec_canopen_pdo_test.cpp` and
`test/nanotec_canopen_events_test.cpp`; console routes `nanotec pdo`, `nanotec emcy`;
`nmt`, `heartbeat`, `network` and `sync` admitted for a Nanotec binding where this
evidence supports them, with unchanged meaning.

## Acceptance

Tests cover 8+8 sets, 8 entries, fixed versus free COB-IDs, the Operational abort,
partial remap, delayed samples across a remap, every transmission type, inhibit and
event-timer units, EMCY decoding including the cleared form and unknown numbers, the
empty-1003h abort, one and two boot-ups in sequence with invalidation on each, and
readiness: an identity reply after one observed boot-up with no later boot-up (a
possible boot-loader reply) gives the identity outcome with readiness UNKNOWN, while
reads and both stop entry points stay available; READY only with 2007h:01 = 1 read
after the preceding boot-up under the single-master declaration and no 2007h write,
CANopen-category save or restore by this library around it; a RAM-only 2007h:01 = 1
written by this library, then a reboot with the boot loader's boot-up, gives UNKNOWN,
and so do the single-master guard off and a single unanswered identity read between
two boot-ups (12A Q2's record); no delay makes it READY.
COB-ID changes: before 11 item 4's check has landed, refused as BLOCKED with zero TX;
in the commit that lands it (11 item 4), a candidate already reserved on the
interface refused with zero TX; a
readback before activation reported as configured and pending, not active; both IDs
reserved and their routes unqualified through a partial write, a lost reply and
pending activation; a readback after a boot-up with readiness UNKNOWN keeping the
change pending, a TPDO frame on the new identifier after the boot-up activating
that TPDO's ID only, and a caller declaration after a recorded power cycle or Reset
Communication activating an RPDO's new ID with the declaration named in every result
using the route (READY and the TPDO frame still activating without one); after a reset, a delayed sample under the old map not qualified.
The owner's targeted NMT route on a Nanotec binding: the bootstrap Enter
Pre-operational admitted on a fresh Stopped report with no current outcome; refused
under CONTRADICTED and with stale or missing state evidence (the node reported
stranded); never sent by recovery or probe; a targeted Stop and Reset Communication
refused with a fresh torque state (Operation enabled, Quick stop active, Fault
reaction active) and with unread or stale state, a targeted Reset Node refused, a
targeted Start refused with a valid RPDO mapping a command object that 04b item 1
does not admit, each with zero NMT TX; a targeted and a node-0 command whose evidence
goes stale or whose binding changes between admission and hand-off retired unsent. S13 through the real producer with several
Nanotec nodes (canopen_shared 02 S13 cases): known harmless admitted; unknown or stale
refused; a valid synchronous RPDO mapping a command object refused on the target and
on a node other than the target; remap, rebind and reset during a running schedule
with no later SYNC TX; a targeted and a broadcast Start while a Pre-operational node
holds a synchronous RPDO (approval revoked before the Start, no later SYNC TX). Node-0
NMT through the real owner's NMT admission with several Nanotec nodes: each command
admitted only under its row's condition on every node; an unlisted command, unread or
stale evidence, Reset Node, and a harmful effect on a node other than the target each
refused with zero NMT TX. The landing or extension is recorded with its tested
interface and these cases in the canopen_shared README table. Quick and full
verification and CI on the exact final pushed commit pass. Hardware NOT RUN.
