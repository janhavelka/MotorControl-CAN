# 10 — Q program modes -1 and -2

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 04 and 09. Mode -2 stays refused, and its deliverable Blocked, until seam S13
(SYNC side-effect admission, claimed by stage 09 or another family) has landed and its consumer gate passed;
this stage consumes S13 and never builds its own SYNC gate. The user-register mapping gap of MO-D20 and the
segment range of MO-D18 are owned here.

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
   -1 and confirm through 0x6061; write 0x2007 and read it back; bit 4 low, then 0x001F starts the segment,
   which "runs to completion" (AMP10 p48). 0x011F halts it immediately; the next start runs it from the
   beginning (AMP10 p48). A later 0-to-1 edge of bit 4 re-executes the program: never repeat a start,
   including after a lost reply.
4. Observation: in Q mode statusword bits 12 and 13 are reserved (UM21 p37). Use 0x200B bit 14 "Q Program is
   running", bit 7 "Waiting for an input" and bit 11 "executing a WT command" (UM21 p67), and 0x603F bit 14
   "Blank Q Segment" (UM21 p56). Completion is `OBSERVED` only when bit 14 was seen set after the trigger and
   then clear; a program that ends between polls gives acknowledged execution with completion `NOT_OBSERVED`.
   A blank-segment alarm ends the operation failed with the raw alarm.
5. Results: a Q run publishes no position, velocity or target-reached claim and no common motion completion.
   Position reads afterwards are ordinary reads. Host origin and reference generations are invalidated after
   every Q run, because the program may move the axis or rewrite positions.
6. Mode -2, SYNC Q (UM21 p36, p90; AMP10 p48-p49): after selecting the segment, every received SYNC starts it;
   halt stops it, and the next SYNC after halt is cleared restarts it from the beginning. Refuse mode -2 in the
   first implementation. Once S13 has landed and passed, consume it: mode -2 may be implemented only as an
   explicit operation that owns the interface's SYNC schedule for its lifetime, admits the 0x6060 = -2 write
   only with that ownership, treats clearing halt in mode -2 as a motion start, and leaves the node marked
   SYNC-unsafe in S13 until 0x6061 reads a value other than -2. UM21 p36 writes 0x1005 = 0x80 as part of the
   setup; that is a communication change outside this stage: admit mode -2 only when 0x1005 already reads the
   schedule's COB-ID, never write it here.
7. User registers 0x2100 sub 1-23 (UM21 p71; INT32 in the EDS, UNSIGNED32 in UM21, MO-D09) are volatile and
   shared with Q programs. UM21 gives no mapping to Q register names (AMP10 p18 does for the 2010 objects
   0x4000-0x4016; stage 13). Offer typed native reads and writes of the raw 32-bit words; a write while 0x200B
   bit 14 is set changes a running program and needs an explicit caller policy. No register is written on
   startup or by any other operation.
8. Watchdog actions 2 and above of 0x2060 sub 5 run Q segments on communication loss (UM21 p38, p70). They
   stay refused in stage 11 unless the qualification of item 1 covers the named segment.

## Deliver

Profile `QProgram.h/.cpp` with the program qualification, a Q run context and the user-register descriptors;
`test/moons_canopen_q_program_test.cpp`; MOONS' family console routes for Q start, halt and registers with
parity tests; coverage rows for Q start, SYNC Q (refused; Blocked on S13) and registers; Doxygen; report.

## Acceptance

Tests cover refusal without qualification, wrong product or layout, segment outside the qualification, mode
accept timeout, lost start reply without repeat, halt during a run, bit 14 observed and missed, blank segment
alarm, origin invalidation, register writes while running refused, and mode -2 refused before any TX in every
path, including a stale console request. No Q run is reported as a common move. Quick and full verify,
firmware builds and exact-commit CI pass. Hardware: NOT RUN; a physical Q test needs a fixture with a program
whose content the owner supplies.
