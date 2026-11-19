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
   product change and any report that an RS-232 tool was attached.
2. Segment number: 0x2007 Q segment, UINT8 RW (UM21 p66). UM21 gives no range; AMP10 p48 says 1-12 for the
   2010 object 0x7007, and UM21 p38 ("0x2-0x14: Q program with 1-12 segment") and p70 ("0x2-0x0E") disagree
   on the watchdog codes that start segments (MO-D18). Admit only segments named by the qualification; keep
   the UM21 range unresolved until fixture or vendor evidence.
3. Normal Q, mode -1 (UM21 p35, p89): enable with halt set (0x0006, 0x0007, 0x010F, stage 04); write 0x6060 =
   -1 and confirm through 0x6061; write 0x2007 and read it back; bit 4 low, then 0x001F starts the segment
   (UM21 p35, p89). UM21 shows 0x011F as the halt word and states nothing more; "runs to completion", "halts
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

## Deliver

Profile `QProgram.h/.cpp` with the program qualification, a Q run context and the user-register descriptors;
`test/moons_canopen_q_program_test.cpp`; MOONS' family console routes for Q start, halt and registers with
parity tests; coverage rows for Q start, SYNC Q (refused in this set) and registers; Doxygen; report.

## Acceptance

Tests cover refusal without qualification, wrong product or layout, segment outside the qualification, mode
accept timeout, lost start reply without repeat, halt during a run publishing no stop or restart claim, bit 14
observed and missed, blank segment alarm, origin retirement before the start TX (references unusable also
after a lost start reply), register writes while running refused, and
mode -2 refused before any TX in every path, including a stale console request. No Q run is reported as a
common move. Quick and full verify,
firmware builds and exact-commit CI pass. Hardware: NOT RUN; a physical Q test needs a fixture with a program
whose content the owner supplies.
