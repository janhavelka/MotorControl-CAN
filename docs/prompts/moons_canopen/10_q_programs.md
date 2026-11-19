# 10 — Q program modes -1 and -2

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 04, 08's settings context (user-register writes) and 09. Mode -2 is refused in
this set (item 6); this stage builds no SYNC gate, and where seam S13 has landed its 0x6060 writes revoke the
node's S13 approval as stage 09's entry defines. The user-register mapping gap of MO-D20 and the segment range
of MO-D18 are owned here.

Q programs are vendor sequences outside CiA 402 (UM21 p35). They are written and loaded with Q Programmer over
RS-232, during which the CAN node is held in Initialization (UM21 p9, p35; AMP10 p48). A program may contain
arbitrary SCL motion and I/O commands (HCR), so starting one is opaque vendor motion: the library cannot know
its travel, speed, duration, outputs or end state. SCL itself has no documented CANopen object (UM21, AMP10;
review s5.6) and is not offered.

## Work

1. Scope: the library never loads, reads back or verifies program content, never runs a configuration tool,
   and offers no SCL transport. A Q start requires a caller-supplied, generation-bound program qualification
   that names the binding, the segment number and the caller's evidence of what that segment does. Without it
   every Q operation rejects before TX. The qualification is invalidated by boot-up, rebinding, layout or
   product change and any report that an RS-232 tool was attached. It also states the segment's ending:
   **self-ending**, when the caller's evidence shows the segment ends by itself within a stated duration and
   travel bound, waiting for no input and not looping; otherwise **host-ended**, including a segment that
   waits for an input (0x200B bit 7, UM21 p67) or whose end the caller cannot bound. Item 9 admits each kind.
2. Segment number: 0x2007 Q segment, UINT8 RW (UM21 p66). UM21 gives no range; AMP10 p48 says 1-12 for the
   2010 object 0x7007, and UM21 p38 ("0x2-0x14: Q program with 1-12 segment") and p70 ("0x2-0x0E") disagree
   on the watchdog codes that start segments (MO-D18). Admit only segments named by the qualification; keep
   the UM21 range unresolved until fixture or vendor evidence.
3. Normal Q, mode -1 (UM21 p35, p89): enable with halt set (0x0006, 0x0007, 0x010F, stage 04); write 0x6060 =
   -1 and confirm through 0x6061; write 0x2007 and read it back; bit 4 low, then 0x001F starts the segment
   (UM21 p35, p89). Halt admission, from fresh 0x6041 and 0x6061 reads before the first changing TX, is
   stage 06 item 2's: a drive already in Operation enabled must be stage 04 item 8's known-halted predecessor,
   whatever the enable result, and the 0x6060 = -1 write (stage 04 item 3), the 0x2007 write and the start are
   sent only while the record is known halted; halt clear in any candidate or an unknown record refuses before
   TX, also when the drive already reads mode -1. Stage 04's halt in mode -1 is 0x011F after an acknowledged
   0x001F and 0x010F before any start, both only with stage 04 item 9's local-transition declaration. Without
   it, or after a lost start reply, or with the record unknown, the halt is refused and the result names the
   quick stop 0x010B as the eligible stop row (stage 04 items 8 and 9), dispatched only as item 9 below states.
   UM21 shows 0x011F as the halt word and states nothing more; "runs to completion", "halts
   immediately" and "the next start runs it from the beginning" are AMP10 p48 statements for the 2010 layout.
   For UM21 publish "halt commanded" and the observed 0x200B bits; the stopping behavior and the restart point
   stay unknown until fixture evidence (14a Q11). A later 0-to-1 edge of bit 4 may re-execute the program
   (AMP10 p48): never repeat a start, including after a lost reply.
4. Observation: in Q mode statusword bits 12 and 13 are reserved (UM21 p37). Use 0x200B bit 14 "Q Program is
   running", bit 7 "Waiting for an input" and bit 11 "executing a WT command" (UM21 p67), and 0x603F bit 14
   "Blank Q Segment" (UM21 p56). Completion is `OBSERVED` only when bit 14 was seen set after the trigger and
   then clear; a program that ends between polls gives acknowledged execution with completion `NOT_OBSERVED`.
   A blank-segment alarm ends the operation failed with the raw alarm.
5. Results: a Q run publishes no position, velocity or target-reached claim and no common motion completion.
   Position reads afterwards are ordinary reads. Because the program may move the axis or rewrite positions,
   the binding's host origin and reference generations, prepared or queued position work built on them and
   host soft limits bound to them are retired before the start word is handed to the owner (the rule of stage
   07 item 12); a lost start reply, cancellation, halt or unobserved end never restores them, and no Q result
   supplies a replacement reference.
6. Mode -2, SYNC Q (UM21 p36, p90; AMP10 p48-p49): after selecting the segment, every received SYNC starts it;
   by AMP10 p49 halt stops it and the next SYNC after halt is cleared restarts it from the beginning. This set
   is refusal-only (canopen_shared [round 2](../canopen_shared/audit_application.md#round-2-family-audits-19-november-2026)):
   no path writes 0x6060 = -2, and a node that reads -2 keeps S13 refusing SYNC on its interface (stage 09).
   Owning a schedule is no exception to S13. A positive SYNC-triggered program start needs its own
   shared-contract change for an intentional, exclusive, qualified trigger (activation, interruption, a lost
   mode write, teardown) and is not planned here; the ordinary S13 gate stays unchanged.
7. User registers 0x2100 sub 1-23 (UM21 p71; INT32 in the EDS, UNSIGNED32 in UM21, MO-D09) are volatile and
   shared with Q programs. UM21 gives no mapping to Q register names (AMP10 p18 does for the 2010 objects
   0x4000-0x4016; stage 13). Offer typed native reads and writes of the raw 32-bit words; a write while 0x200B
   bit 14 is set changes a running program and needs an explicit caller policy. No register is written on
   startup or by any other operation.
8. Watchdog actions 2 and above of 0x2060 sub 5 run Q segments on communication loss (UM21 p38, p70), but
   which action value starts which segment is unknown: p38 gives 0x2-0x14 for 12 segments and p70 0x2-0x0E,
   and neither range has 12 values (MO-D18). Knowing what segment N does says nothing about which action byte
   starts N, so stage 11 refuses every action of 2 and above until vendor or fixture evidence resolves the
   action-to-segment mapping for the firmware **and** item 1's qualification covers that segment.
9. Interruption, by item 1's ending. A self-ending segment runs as a finite operation, like stage 05's move:
   its qualified duration bound, with the caller's stop budget, must fit before the operation deadline or
   preparation refuses; if 0x200B bit 14 has not been seen clear when that bound passes, the result says the
   qualification was not met and keeps `needsStop` for the application's urgent stop. A host-ended segment
   is a host-ended run under canopen_shared 02 "Timed runs" (as stage 07 item 6 applies it to homing): a
   caller-supplied latest interruption time in place of the duration, fitting with the stop budget before
   the operation deadline or refused at preparation; every later transfer capped at that time, after which
   the run issues no further work and keeps `needsStop`; and the bullet's stop path. A MOONS' binding has no
   S3 shape and its route stays unresolved after a lost start reply (`SdoReuse` `NONE`, stage 04 item 10),
   so no reviewed stop can leave then: preparation refuses a host-ended run before its first changing TX
   (the enable, the 0x6060 = -1 and 0x2007 writes included) unless the application declares an independent
   stop for it with the bullet's caller-supplied fallback deadline, named in every result. Every Q result
   keeps stage 04 item 10's two facts apart: the eligible stop row (item 3's halt where admitted, otherwise
   the quick stop 0x010B) and whether the owner can dispatch it; after a start-reply timeout it cannot, and
   the result says "no software stop on the unresolved route" and names the declared stop (host-ended) or
   stage 04 item 9's application-owned fallback (self-ending).

## Deliver

Profile `QProgram.h/.cpp` with the program qualification and its ending, a Q run context and the user-register descriptors;
`test/moons_canopen_q_program_test.cpp`; MOONS' family console routes for Q start, halt and registers with
parity tests; coverage rows for Q start, SYNC Q (refused in this set) and registers; Doxygen; report.

## Acceptance

Tests cover refusal without qualification, wrong product or layout, segment outside the qualification, item
3's halt admission on an already enabled binding with the record halt set, clear and unknown, through the
mode -1 select and with the drive already in mode -1 (zero changing TX unless halt set), mode
accept timeout, item 9 by ending (a host-ended segment without a declared independent stop refused with zero
changing TX, one whose interruption time no longer fits with the stop budget refused at preparation, no
transfer after the interruption time; a self-ending segment whose bit 14 is still set at its bound keeping
`needsStop`), a possibly executed start whose reply is lost (no repeat; the route timeout under `NONE`; a
halt then refused with zero TX; the quick stop 0x010B named as the eligible row with no dispatch and zero
stop frames; `UNKNOWN` and `needsStop` kept; the declared stop named for a host-ended segment and the
application-owned fallback for a self-ending one), halt during a run publishing no stop or restart claim, bit 14
observed and missed, blank segment alarm, origin retirement before the start TX (references unusable also
after a lost start reply), register writes while running refused, and
mode -2 refused before any TX in every path, including a stale console request. No Q run is reported as a
common move. Quick and full verify,
firmware builds and exact-commit CI pass. Hardware: NOT RUN; a physical Q test needs a fixture with a program
whose content the owner supplies.
