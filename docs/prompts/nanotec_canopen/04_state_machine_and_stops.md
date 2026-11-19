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
   bit 1 = 1). Bits 4-6, 8 and 9 are mode specific and are 0 in every state command.
   Bit 7 is 0 except in fault reset (CP-D08). Two transitions need a rising edge:
   transition 16 (Quick stop active to Operation enabled, bit 2) and fault reset (15,
   bit 7, N5 p58). Because 60F2h bits 4-5 let the drive rewrite 6040h bit 4 itself
   (N5 p407), an edge-dependent step reads 6040h first instead of trusting the last
   written word. Never read-modify-write a controlword.
2. `prepareEnable`: the words of the source-state table below from the observed
   starting state (from Switch on disabled 0006h, 0007h, 000Fh), each confirmed by its
   SDO reply and followed by a fresh statusword showing the next state (masks of
   N5 p58). Admission before the first word: admitted identity (03), exclusive control
   (04b item 1), no fault, known 6061h, the source-state table, and these documented
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
   states the table admits for it. Read the option
   code first; completion is the observed state, not standstill. Automatic brake
   timing on leaving Operation enabled is 2038h:01/02. Release is never a stop.
4. `prepareStop` with native kinds: `HALT` (the whole word 010Fh, enable operation
   with bit 8 and bits 4-6 = 0, never the current word modified; 605Dh 1 or 2, stays
   in Operation enabled) only where N5 p347 lists halt and this set has a mode
   operation: PP, VL, PV and PT (6061h 1-4). IP's halt belongs to 06c; homing stops by
   clearing bit 4 (07); CSP, CSV, CST and the vendor modes refuse `HALT` with the
   reason. Clearing bit 8 resumes motion (N5 p76), so no stop or recovery clears it;
   the next motion start writes its own word. `QUICK_STOP` is 000Bh, transition 11,
   605Ah. Read 605Ah first: 0 coasts; 1 and 2 ramp then Switch on disabled; 5 and 6
   ramp and stay in Quick stop active, energised (N5 pp59-61, p356). N5 documents the
   quick-stop word only from Operation enabled (CP-D10), so the table sends it only
   there. Stops need no exclusive control; their result names any active or unknown
   path of 04b item 1 that can overwrite 6040h (with PD4-C special modes on, 6040h
   state requests have no effect, PD4C p33). A halt or quick stop ends ACKNOWLEDGED
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
   require a fresh 6040h with bit 4 = 0 (FWUPD p6). Refuse -1 and -2 (04b), values
   absent from 6502h and modes the capabilities exclude (PT and CST without closed
   loop). A request for the mode 6061h already shows sends nothing. Controlword writes
   by SDO are valid in Pre-operational (N5 p118); PDO control needs Operational (09).
7. Seam S8: compare Lichuan's statusword decoding and command words with these
   pages. Extract a shared CiA 402 helper only for an identical evidenced subset
   (likely the 004Fh/006Fh state decode and the plain command words); Nanotec bits
   9, 15 and mode bits and Lichuan manual dialects stay profile-local. Lichuan bytes
   and outcomes stay identical. If nothing is identical, record why and keep both local.
8. Seam S3 shape, opt-in per binding (canopen_shared 02 "CANopen-only rules"); the
   default deny landed with S2 in 02b, so until this item a Nanotec binding has no
   best-effort stop. Its reviewed exact shape (object, size, data, ledger review ID) is
   set per binding here. The candidate is an expedited
   two-byte 6040h:00 download of 000Bh from Nanotec's own pages (N5 p57, p347); that
   the value equals Lichuan's DEC-2 word is not reuse. Its review records that N5
   documents the quick-stop word only from Operation enabled (CP-D10) and that another
   control path (04b) may overwrite 6040h. `admitBestEffortStop` checks the transfer
   against the shape of its exact current binding and channel generation; another
   family's shape, no shape and a stale target return `INVALID` with no TX and no
   fallback word. One transmission at most, never `SUCCEEDED`, and the route stays
   unresolved; Lichuan's DEC-2 behavior and tests stay unchanged.

## Source-state and intent table

The table canopen_shared 02 "Operation admission" requires, from N5 pp57-58 (rendered:
transitions 2-16, state masks; the figure places torque at the motor only in Operation
enabled, Quick stop active and Fault reaction active). A row is chosen by a fresh 6041h
(with 6061h, 603Fh and, for an edge, 6040h), never by the last written word. The first
listed word is the first TX; "nothing sent" reports the observed state as the result.
An undecodable or failed read refuses every column (an unresolved route is the
best-effort stop's case, item 8).

| Fresh state | Enable | Release (`DISABLE_OPERATION`, `SHUTDOWN`, `DISABLE_VOLTAGE`) | `HALT` | `QUICK_STOP` | Fault reset | Mode select |
| --- | --- | --- | --- | --- | --- | --- |
| Not ready to switch on | Refused | Refused | Nothing sent (no torque) | Nothing sent (no torque) | Refused | Refused |
| Switch on disabled | 0006h (2), 0007h (3), 000Fh (4) | Already released: nothing sent, every kind | Nothing sent (no torque) | Nothing sent (no torque) | Refused | 6060h |
| Ready to switch on | 0007h (3), 000Fh (4) | Nothing sent for the first two kinds; 0000h (7) | Nothing sent (no torque) | Nothing sent (no torque) | Refused | 6060h |
| Switched on | 000Fh (4) | Nothing sent; 0006h (6); 0000h (10) | Nothing sent (no torque) | Nothing sent (no torque) | Refused | 6060h |
| Operation enabled | Already enabled: nothing sent, no shutdown, power-stage or brake cycle | 0007h (5); 0006h (8); 0000h (9) | 010Fh in 6061h 1-4, otherwise refused | 000Bh (11) | Refused (only in Fault, item 5) | 6060h with a fresh 6040h bit 4 = 0 |
| Quick stop active | Refused; the separate enable-after-quick-stop (16) needs a fresh 6040h with bit 2 = 0, then 000Fh | Refused; refused; 0000h (12) | Refused | Already reached: nothing sent | Refused | Refused |
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
(already enabled sends nothing; `HALT` sends 010Fh only in 6061h 1-4); every step's
confirmed, aborted, malformed, lost and late reply; stop while queued, in flight and
waiting; stop during enable; bit 2 and bit 7 edges against a drive-modified 6040h;
each 605Ah and 605Dh value; refused mode changes; enable refused before TX when 04b
item 1's predicate refuses; the Nanotec S3 shape admitted only on its own binding and
channel generation, and Lichuan's shape, no shape or a stale target refused with no
TX; no repeated trigger and no enable after reset or stop. A pre-existing state never
manufactures completion. Quick and full verification, firmware builds and CI on the
exact final pushed commit pass. Hardware NOT RUN; no physical stop is qualified.
