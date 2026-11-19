# 04 — CiA 402 state machine, enable, release, stops and fault reset

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03. NT-D13 and NT-D26 apply; seams S3 and S8 land or are extended
here. Follow [prompt 57](../57_regular_drive_operations.md) for the common enable,
release and mode-select policy; the words and evidence below are Nanotec's own.

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
2. `prepareEnable`: 0006h, 0007h, 000Fh, each confirmed by its SDO reply and followed
   by a fresh statusword showing Ready to switch on, Switched on and Operation enabled
   (masks of N5 p58). Admission before the first word: admitted identity (03),
   exclusive control (04b), no fault, known 6061h, reported supervision state (09b)
   and these documented side effects declared and accepted by the caller:
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
   transitions 7, 9, 10, 12, ends Switch on disabled, motor free). Read the option
   code first; completion is the observed state, not standstill. Automatic brake
   timing on leaving Operation enabled is 2038h:01/02. Release is never a stop.
4. `prepareStop` with native kinds: `HALT` (bit 8 set on the current mode word,
   605Dh 1 or 2, stays in Operation enabled) and `QUICK_STOP` (000Bh, transition 11,
   605Ah). Read 605Ah first: 0 coasts; 1 and 2 ramp then Switch on disabled; 5 and 6
   ramp and stay in Quick stop active, energised (N5 pp59-61, p356). N5 documents the
   quick-stop word only from Operation enabled (CP-D10); from other states its effect
   is undocumented and the operation says so. A halt or quick stop ends ACKNOWLEDGED
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
   loop). Controlword writes by SDO are valid in Pre-operational (N5 p118); PDO
   control needs Operational (09).
7. Seam S8: compare Lichuan's statusword decoding and command words with these
   pages. Extract a shared CiA 402 helper only for an identical evidenced subset
   (likely the 004Fh/006Fh state decode and the plain command words); Nanotec bits
   9, 15 and mode bits and Lichuan manual dialects stay profile-local. Lichuan bytes
   and outcomes stay identical. If nothing is identical, record why and keep both local.
8. Seam S3 (claim if first), opt-in per binding (canopen_shared 02 "CANopen-only
   rules"): a Nanotec binding has no best-effort stop until its reviewed exact shape
   (object, size, data, ledger review ID) is set for it. The candidate is an expedited
   two-byte 6040h:00 download of 000Bh from Nanotec's own pages (N5 p57, p347); that
   the value equals Lichuan's DEC-2 word is not reuse. Its review records that N5
   documents the quick-stop word only from Operation enabled (CP-D10) and that another
   control path (04b) may overwrite 6040h. `admitBestEffortStop` checks the transfer
   against the shape of its exact current binding and channel generation; another
   family's shape, no shape and a stale target return `INVALID` with no TX and no
   fallback word. One transmission at most, never `SUCCEEDED`, and the route stays
   unresolved; Lichuan's DEC-2 behavior and tests stay unchanged.

## Deliver

`profiles/nanotec_canopen/Actions.h/.cpp` with `ActionContext`, `StopKind`,
`ReleaseKind`, mode select and fault reset; the S3 and S8 changes with their tests;
`test/nanotec_canopen_actions_test.cpp`; console routes `nanotec enable`, `release`,
`stop`, `alarm-clear` and `mode` with parity tests; coverage rows updated.

## Acceptance

Tests use literal frames through the real owner and fake server: every step's
confirmed, aborted, malformed, lost and late reply; stop while queued, in flight and
waiting; stop during enable; bit 2 and bit 7 edges against a drive-modified 6040h;
each 605Ah and 605Dh value; refused mode changes; foreign-binding stop shapes; no
repeated trigger and no enable after reset or stop. A pre-existing state never
manufactures completion. Quick and full verification, firmware builds and CI on the
exact final pushed commit pass. Hardware NOT RUN; no physical stop is qualified.
