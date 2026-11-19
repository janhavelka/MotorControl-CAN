# 04 — CiA 402 state machine, enable, release, stops and fault reset

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03 and after 04b item 1 (the pure exclusive-control
predicate, landed and tested before this stage exposes enable). NT-D13 and NT-D26
apply; the Nanotec S3 shape (on the default deny landed in 02b) and seam S8 land or are
extended here. Follow [prompt 57](../57_regular_drive_operations.md) for the common
enable, release and mode-select policy and canopen_shared 02 "Operation admission"
(write preflight, one deadline, state transitions); the words, table and evidence
below are Nanotec's own.

## Work

1. Command words. Whole-word constants from N5 p57 (rendered) and p347, bits 7/3/2/1/0:
   shutdown 0006h; switch on 0007h (bit 3 must be 0, CP-D07); enable operation
   000Fh; disable operation 0007h; disable voltage 0000h; quick stop 000Bh (bit 2 = 0,
   bit 1 = 1). Bits 4-6, 8 and 9 are mode specific and are 0 in every state command,
   except that a word sent while the fresh 6040h has bit 8 = 1 keeps bit 8 (the
   table's bit-set Operation enabled row, its Quick stop active release and item 4).
   A set bit 8 is a requested halt in PP, VL, PV, PT and IP only; it is never reported
   as an attained halt. The p57 commands are defined by
   bits 7, 3, 2, 1 and 0 only, so 0107h, 0106h, 0100h and 010Bh are the same commands.
   Bit 7 is 0 except in fault reset (CP-D08). Two transitions need a rising edge:
   transition 16 (Quick stop active to Operation enabled, bit 2) and fault reset (15,
   bit 7, N5 p58). Because 60F2h bits 4-5 let the drive rewrite 6040h bit 4 itself
   (N5 p407), an edge-dependent step reads 6040h first instead of trusting the last
   written word. Never read-modify-write a controlword.
2. `prepareEnable`: the words of the source-state table below from the observed
   starting state (from Switch on disabled 0006h, 0007h, 000Fh), each confirmed by its
   SDO reply and followed by a fresh statusword showing the next state (masks of
   N5 p58). Admission before the first word: admitted identity (03), exclusive control
   (04b item 1), no fault, a fresh 6061h that item 6's host mode table admits for
   enable, the source-state table, and these documented
   side effects declared and accepted by the caller; the result reports 03's
   `Supervision` snapshot (a report, not a gate; 09b item 4):
   - 3202h bit 4 set: the first enable after each restart moves the rotor to a pole
     and needs brake control off (N5 p53, p298).
   - Encoder commutation without auto alignment: open loop until the index passes,
     up to one motor revolution; an encoder without index always aligns (N5 p53,
     p298, 33A0h).
   - Automatic brake (3202h bit 2): current on, 2038h:03, release, 2038h:04 (N5
     pp107-108); CL4-E and PD4-C have no automatic brake chapter.
   - Profile Position with 6040h bit 4 already set starts a move on entering
     Operation enabled (N5 p68; FWUPD p6). The enable words clear bit 4, so this is
     reachable only through another writer, which 04b excludes.
3. `prepareRelease` with explicit native kinds, each mapped to its documented reaction:
   `DISABLE_OPERATION` (0007h, 605Ch, ends Switched on; torque-free per FWUPD p6),
   `SHUTDOWN` (0006h, 605Bh, ends Ready to switch on) and `DISABLE_VOLTAGE` (0000h,
   transitions 7, 9, 10, 12, ends Switch on disabled, motor free), each only from the
   states the table admits for it and with bit 8 kept where the fresh 6040h has it
   set (0107h, 0106h, 0100h; from Quick stop active 0100h). Read the option
   code first; completion is the observed state, not standstill. Automatic brake
   timing on leaving Operation enabled is 2038h:01/02. Release is never a stop.
4. `prepareStop` with native kinds: `HALT` (the whole word 010Fh, enable operation
   with bit 8 and bits 4-6 = 0, never the current word modified; 605Dh 1 or 2, stays
   in Operation enabled) only where N5 p347 lists halt and this set has a mode
   operation: PP, VL, PV and PT (6061h 1-4). IP's halt belongs to 06c; homing stops by
   clearing bit 4 (07); CSP, CSV, CST and the vendor modes refuse `HALT` with the
   reason. Clearing bit 8 resumes motion toward the target in PP, VL, PV, PT and IP
   (N5 p68, p76, p77, p80, p89), so no stop, release or recovery clears it; the next
   motion start writes its own word. `QUICK_STOP` is transition 11, 605Ah: 000Bh, or
   010Bh when the fresh 6040h has bit 8 = 1. N5 gives no precedence between a halt
   release and transition 11 requested in one word, so a quick stop from a halted
   drive keeps the halt rather than combine a documented restart with the stop
   (canopen_shared 02 "State transitions": a stop word never clears a set halt bit
   unless the family's evidence shows the quick stop takes precedence).
   Read 605Ah first: 0 coasts; 1 and 2 ramp then Switch on disabled; 5 and 6
   ramp and stay in Quick stop active, energised (N5 pp59-61, p356). N5 documents the
   quick-stop word only from Operation enabled (CP-D10), so the table sends it only
   there. Stops need no exclusive control; their result names any active or unknown
   path of 04b item 1 that can overwrite 6040h (with PD4-C special modes on, 6040h
   state requests have no effect, PD4C p33). Under a refusing identity outcome
   (prompt 57 "Admission by outcome") `HALT` and `QUICK_STOP` keep prompt 57's stop
   exemption with their own checks: the state reads their table row needs (6041h,
   6061h, 6040h, 603Fh, which prompt 57 keeps admitted) and the stop's own
   non-changing read of the 605Ah or 605Dh reaction it reports, this table, route and
   binding generation; a stop prepared or queued before the outcome
   changed is not withdrawn, while every other pending changing step is retired. When
   the table itself refuses (an undecodable or failed read), the result says that no
   CAN stop was admitted and names the application's independent stop path as the
   remaining means. A halt or quick stop ends ACKNOWLEDGED
   or with the observed state; standstill needs separate evidence. Leaving Quick stop
   active (5, 6) is an explicit enable-after-quick-stop with the bit 2 edge, never
   part of the stop.
5. `prepareClearAlarm` (fault reset): only in Fault. Read 6041h, 603Fh, 1001h and the
   1003h count; if 6040h bit 7 reads 1, write 0000h first; then 0080h; observe Switch
   on disabled and 603Fh = 0 (N5 p346-347). Overtemperature needs cooling first
   (N5 p19), so a persisting fault is a retained result, not a retry. Stored limit
   positions are not cleared (07 owns 3701h = -2). Never enable after a reset.
6. Mode select (6060h, then 6061h confirms). Host policy from the state diagram
   (N5 p57, rendered): admit in Switch on disabled, Ready to switch on, Switched on
   and Operation enabled; refuse in Quick stop active, Fault reaction active and
   Fault, although the text says "at any time" (N5 p58, NT-D13). In Operation enabled
   require a fresh 6040h with bit 4 = 0 (FWUPD p6). Entering VL, PV or PT while in
   Operation enabled also needs the destination target to read zero: VL and PV run
   toward 6042h or 60FFh in Operation enabled (N5 pp76-79), these targets are savable
   (10 item 6), and N5 p58 and FWUPD p6 document no other disposition of a retained
   target on an enabled mode change. The order is: the fresh state reads of the
   table row, a fresh read of the destination target (6042h, 60FFh or 6071h) under
   04b item 1's predicate, then the 6060h write as the only changing step, then 6061h.
   A nonzero or unread target refuses with nothing sent, whatever bit 8 shows (a set
   halt is released by the next start, item 4); the library writes no target of
   another mode to make the change pass. A lost 6060h reply leaves the mode UNKNOWN:
   no replay, and work that depends on the mode is refused until a fresh 6061h. The
   result claims no standstill and no transient behavior. A 6502h bit says what the drive
   supports, never what this host can run: mode select and enable with a mode already
   active both consume the host mode table below, and a mode whose owning stage has
   not delivered its admission is unavailable (`UNSUPPORTED`, nothing sent). Values
   absent from 6502h and modes the capabilities exclude (03 item 5: PT and CST without
   closed loop; IP, CSP and CSV in open loop) are refused as well. A request for the
   mode 6061h already shows sends nothing. Controlword writes
   by SDO are valid in Pre-operational (N5 p118); PDO control needs Operational (09).

   | Mode (N5 p359) | Mode select | Enable with this mode active | Owner |
   | --- | --- | --- | --- |
   | 0 no mode assigned | Refused (no host operation) | Admitted | 04 |
   | 1 PP | Once 05 has landed | Once 05 has landed (bit 4 rule of item 2) | 05 |
   | 2 VL, 3 PV | Once 06a has landed; from Operation enabled only with a fresh zero 6042h or 60FFh (the order above) | Once 06a has landed, with a fresh zero 6042h or 60FFh: host policy, because these modes run toward their target whenever Operation enabled is reached without halt (N5 p76, p77) | 06a |
   | 4 PT | Only inside 06b's operation, from Operation enabled, immediately after its fresh bit 15 = 1 check and a fresh zero 6071h (06 item 6) | Refused: bit 15 is meaningful only in Operation enabled (N5 p348), so PT is entered from another enabled mode | 06b |
   | 6 homing | Once 07 has landed | Once 07 has landed (bit 4 starts homing, N5 p82) | 07 |
   | 7 IP, 8 CSP, 9 CSV, 10 CST | Only inside an assigned 06c operation with its gates passed | Same | 06c |
   | -1, -2 | Refused (04b items 1 and 5); -2 only inside 04b's auto setup | Refused (04b item 1) | 04b |
   | 5 and any other value | Refused | Refused | None |
7. Seam S8: compare Lichuan's statusword decoding and command words with these
   pages. Extract a shared CiA 402 helper only for an identical evidenced subset
   (likely the 004Fh/006Fh state decode and the plain command words); Nanotec bits
   9, 15 and mode bits and Lichuan manual dialects stay profile-local. Lichuan bytes
   and outcomes stay identical. If nothing is identical, record why and keep both local.
8. Seam S3 shape, opt-in per binding, consuming canopen_shared 02 "CANopen-only
   rules" S3 as it stands: the shape establishes no drive state or predecessor, and a
   family opts in only with a review of the word's reaction from every state reachable
   after an uncertain write, a set halt bit included; otherwise the default deny
   landed with S2 in 02b stays. Until this item a Nanotec binding has no best-effort
   stop. Its reviewed exact shape (object, size, data, ledger review ID) is set per
   binding here. The candidate is an expedited two-byte 6040h:00 download of 010Bh;
   its ledger review records from Nanotec's pages: bits 7, 2 and 1 = 0, 0, 1 match only
   the quick stop command, transition 11 from Operation enabled (N5 p57), so from no
   state does the word request an enabling transition (bit 2 = 0 is no rising edge for
   16) or a fault reset (bit 7 = 0, N5 p58), and from Ready to switch on and Switched
   on N5 lists no transition for it (CiA 402 lists 7 and 10, CP-D10); bit 8 = 1 holds
   or triggers a halt with 605Dh in PP, VL, PV, PT and IP (N5 p60, p347) and has no
   special function in homing, CSP, CSV, CST or auto setup (N5 p82, pp91-93, p96), and
   which of the halt and quick-stop reactions applies first is undocumented, so the
   result claims neither; bits 4-6 = 0 start nothing (PP, homing and auto setup start
   on a rising bit 4, N5 p68, p82, p96) and end a running homing or IP interpolation
   (p82, p89). 000Bh is rejected as the shape: from a halted drive it clears bit 8,
   which resumes motion in those five modes (item 4) with no documented precedence
   over transition 11 (canopen_shared 02 "State transitions"). The review also records that another control path (04b) may
   overwrite 6040h. `admitBestEffortStop` checks the transfer
   against the shape of its exact current binding and channel generation; another
   family's shape (Lichuan's DEC-2 `6040:00=000B` included), no shape and a stale
   target return `INVALID` with no TX and no fallback word. One transmission at most,
   never `SUCCEEDED`, and the route stays unresolved; Lichuan's DEC-2 behavior and
   tests stay unchanged.

## Source-state and intent table

The table canopen_shared 02 "Operation admission" requires, from N5 pp57-58 (rendered:
transitions 2-16, state masks; the figure places torque at the motor only in Operation
enabled, Quick stop active and Fault reaction active). A row is chosen by a fresh 6041h
(with 6061h, 603Fh and, for an edge or the halt bit, 6040h), never by the last written word. The first
listed word is the first TX; "nothing sent" reports the observed state as the result.
An undecodable or failed read refuses every column (an unresolved route is the
best-effort stop's case, item 8).

| Fresh state | Enable | Release (`DISABLE_OPERATION`, `SHUTDOWN`, `DISABLE_VOLTAGE`) | `HALT` | `QUICK_STOP` | Fault reset | Mode select |
| --- | --- | --- | --- | --- | --- | --- |
| Not ready to switch on | Refused | Refused | Nothing sent (no torque) | Nothing sent (no torque) | Refused | Refused |
| Switch on disabled | 0006h (2), 0007h (3), 000Fh (4) | Already released: nothing sent, every kind | Nothing sent (no torque) | Nothing sent (no torque) | Refused | 6060h |
| Ready to switch on | 0007h (3), 000Fh (4) | Nothing sent for the first two kinds; 0000h (7) | Nothing sent (no torque) | Nothing sent (no torque) | Refused | 6060h |
| Switched on | 000Fh (4) | Nothing sent; 0006h (6); 0000h (10) | Nothing sent (no torque) | Nothing sent (no torque) | Refused | 6060h |
| Operation enabled, fresh 6040h bit 8 = 0 | Already enabled: nothing sent, no shutdown, power-stage or brake cycle | 0007h (5); 0006h (8); 0000h (9) | 010Fh in 6061h 1-4, otherwise refused | 000Bh (11) | Refused (only in Fault, item 5) | 6060h by item 6's table, with a fresh 6040h bit 4 = 0 |
| Operation enabled, fresh 6040h bit 8 = 1 (bit set) | Already enabled: nothing sent; the bit stays | 0107h (5); 0106h (8); 0100h (9): bit 8 kept (item 4) | In 6061h 1-4: halt already requested, nothing sent; the result reports bit 10 by the mode's table and claims neither an attained halt nor standstill. Other modes: refused (bit 8 has no halt function in homing, CSP, CSV, CST or auto setup, N5 p82, pp91-93, p96; IP's halt belongs to 06c, item 4) | 010Bh (11): bit 8 kept (item 4; canopen_shared 02 "State transitions") | Refused (only in Fault, item 5) | 6060h by item 6's table, with a fresh 6040h bit 4 = 0; the bit stays |
| Quick stop active | Refused; the separate enable-after-quick-stop (16) needs a fresh 6040h with bit 2 = 0 and item 6's enable column for the active mode, then 000Fh, or 010Fh when that 6040h has bit 8 = 1 | Refused; refused; 0000h (12) when the fresh 6040h has bit 8 = 0, 0100h (12, the same command by bits 7, 3, 2, 1, 0; bit 8 kept) when it has bit 8 = 1 | Refused | Already reached: nothing sent | Refused | Refused |
| Fault reaction active | Refused | Refused | Nothing sent (fault reaction runs) | Nothing sent (fault reaction runs) | Refused until Fault | Refused |
| Fault | Refused (reset first, never automatic) | Refused (fault reset is the route) | Nothing sent (no torque) | Nothing sent (no torque) | 0080h, after 0000h when a fresh 6040h has bit 7 = 1 (15) | Refused |

## Deliver

`profiles/nanotec_canopen/Actions.h/.cpp` with `ActionContext`, `StopKind`,
`ReleaseKind`, mode select and fault reset; the source-state table as code and in
the coverage rows; the S3 shape and S8 changes with their tests;
`test/nanotec_canopen_actions_test.cpp`; console routes `nanotec enable`, `release`,
`stop`, `alarm-clear` and `mode` with parity tests; coverage rows updated.

## Acceptance

Tests use literal frames through the real owner and fake server: the first emitted
frame, or none, for every cell of the source-state table and each mode in it
(already enabled sends nothing; `HALT` sends 010Fh only in 6061h 1-4; from Operation
enabled with bit 8 set `QUICK_STOP` sends 010Bh, each release keeps bit 8, and `HALT`
sends nothing with a "requested, not attained" result in 6061h 1-4 and is refused in
every other mode; from Quick stop active `DISABLE_VOLTAGE` sends
0000h or, with bit 8 set, 0100h), each asserting the result as well as the frame;
mode select into VL and PV from Operation enabled, halted or not, with a nonzero or
unread destination target (zero TX) and with a zero one (6060h the only changing
frame; PT's entry is tested in 06b),
and a lost 6060h reply during that entry (mode UNKNOWN, no replay); every step's
confirmed, aborted, malformed, lost and late reply; stop while queued, in flight and
waiting; stop during enable; bit 2 and bit 7 edges against a drive-modified 6040h;
each 605Ah and 605Dh value; refused mode changes; the generic mode select and enable
routes against item 6's table (each mode whose owning stage has not landed refused,
PT outside 06b's operation and enable with 6061h = 4 refused, IP and the cyclic modes
refused without an assigned 06c, enable in VL or PV refused with a nonzero target),
each with zero TX; enable refused before TX when 04b
item 1's predicate refuses; under each refusing identity outcome `HALT`,
`QUICK_STOP` and the opted-in S3 shape admitted with their own checks, a stop queued
before the outcome changed still sent, while enable, release, fault reset and mode
select send nothing; the Nanotec S3 shape (010Bh) admitted only on its own binding and
channel generation, and Lichuan's shape, no shape or a stale target refused with no
TX; no repeated trigger and no enable after reset or stop. A pre-existing state never
manufactures completion. Quick and full verification, firmware builds and CI on the
exact final pushed commit pass. Hardware NOT RUN; no physical stop is qualified.
