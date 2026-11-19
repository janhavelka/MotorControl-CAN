# 06 — Profile velocity and profile torque

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 04-05. MO-D04 is owned here; MO-D06, MO-D09 and MO-D18 constrain it.

Profile velocity (mode 3) and profile torque (mode 4) share the halt start/stop pattern in UM21 (p32, p34), so
they form one stage. Torque is a MOONS'-native typed operation for SSDC-C only, not a common effort API
([multi-drive decision](../../multidrive_decision.md), "Changes required for servo and multiple vendors").

## Work

### Profile velocity

1. Objects: 0x60FF target velocity, INT32 in counts/s (UM21 p55, p64 and the EDS; UM21 p31 says UINT32,
   MO-D09), 0x6083 and 0x6084 (UM21 p31). Units and common conversion follow stage 05. The 0x60FF write
   policy is stage 02's signedness rule: 0 to 2^31-1 is admitted, a negative target is refused by the write
   preflight until MO-D09 is resolved for 0x60FF (under UINT32 it would be a large forward value), and even
   then means reverse only after 0x607E and the direction settings were read.
2. Sequence (UM21 p32): mode 3 confirmed through 0x6061; enable with halt set (0x010F); write 0x60FF, 0x6083,
   0x6084, each confirmed; start by clearing halt (0x000F); stop by setting halt (0x010F). Statusword bit 10
   target reached, bit 12 speed (1 = speed is 0) and bit 13 maximum slippage error (UM21 p31, rendered page).
   Bit 12 is the drive's zero-speed indication, not a measured standstill.
3. Halt ramp: UM21 says the axis stops "according to the halt option code (0x605D)", which does not exist
   (MO-D06). AMP10 p26 says halt and a zero target decelerate with the profile deceleration and keep torque
   applied, for the 2010 generation. Treat the UM21 halt ramp as unknown, claim "stop commanded" rather than
   "decelerated with 0x6084", and keep the AMP10 statement for stage 13.
4. Finite run: `prepareVelocity` requires a nonzero finite `durationUs`; zero speed is a setting, not a run. At
   expiry yield the halt stop, never success while running. A host deadline cannot stop the drive: on a lost
   stop reply or service loss retain `needsStop` and `UNKNOWN` execution. Use distinct dispatch and wire
   deadlines, each bounded by the one operation deadline (canopen_shared 02 "Operation admission"), and the
   shared never-transmitted-stop policy; never repeat a start or an already-sent stop.
5. Host loss: the communication watchdog 0x2060 is disabled by default, is fed only by RPDO1-4 or SYNC
   reception (UM21 p70), and is described as "a safety feature in Profile Velocity mode" (MO-D18). There is no
   heartbeat consumer (MO-D11), and no document states a default reaction to host loss (review section 7).
   Publish in the velocity and torque capabilities "host-loss reaction: none established; motion may
   continue", and with an armed watchdog its configuration as stage 11 reports it, never a stop as an
   established outcome. Arming belongs to stage 11. Do not arm it here.
6. Changes during motion (prompt 52): AMP10 p26 shows target changes while running; UM21 documents none. Keep
   velocity change unsupported for UM21 until 14a or vendor evidence; refuse it before admission.

### Profile torque (SSDC-C only)

7. Admission: refuse mode 4 on STF-C before TX (UM21 p58 "only supported for StepSERVO drive"; 0x6502 bit 3
   step-servo only, UM21 p64). Require the declared SSDC-C product and a confirmed UM21 layout.
8. Objects and the unit conflict (MO-D04): 0x6071 target torque INT16 is "per thousand of rated torque" and
   "mN·m" in one paragraph (UM21 p59); 0x6087 torque slope is "per thousand of rated torque per second" and
   "Nm/s" (UM21 p62), INT8 WO on UM21 p33 but UINT32 RW on p55/p62 and in EDS-SSDC; the worked example uses
   mN·m and mN·m/s (UM21 p34). 0x2216 torque constant is RO (UM21 p72, p78; EDS-SSDC) yet written in the p34
   example. Implement native-unit requests only, with the raw value and both documented interpretations in
   the result; physical torque units reject until MO-D04 is resolved. Never write 0x2216.
9. Sequence (UM21 p34): mode 4 confirmed; enable with halt set (0x010F); write 0x6071 and 0x6087; start by
   clearing halt; stop by setting halt. A finite run follows item 4. Report 0x6074 torque demand (mN·m) and
   0x6078 current actual (0.01 A) as drive-reported values (UM21 p60). The sequence needs the 0x6087 write,
   whose width and access conflict (INT8 WO on UM21 p33, UINT32 RW on p55/p62 and in EDS-SSDC; MO-D04): the
   write preflight refuses the whole torque operation, mode select and enable included, with nothing sent,
   until vendor clarification or recorded fixture evidence names one encoding for SSDC-C. Choosing native
   units resolves no width, and omitting the slope write is not the documented sequence. Deliver the torque
   context with this refusal and its tests; the admitted sequence lands only with the resolving evidence.
10. Stop meaning: AMP10 p45 says halt and a zero target ramp the torque down by the slope and then leave the
    shaft free while the drive stays Operation enabled (2010 generation). For UM21 claim only "torque command
    halted"; a torque halt is never reported as a holding stop. Quick stop follows stage 04.
11. Current check: UM21 p34 tells the user to compare torque divided by the torque constant with the drive's
    continuous and peak ratings. The only drive-side limit object is 0x6073 (0.01 A, UM21 p60; MO-D25). The
    profile does not invent a limit; it documents the check for the caller.

## Deliver

Profile `Velocity.h/.cpp` with `VelocityContext`; profile `Torque.h/.cpp` with a MOONS'-native torque context;
`test/moons_canopen_velocity_test.cpp` and `test/moons_canopen_torque_test.cpp`; console `velocity` route and
a MOONS' family torque route with parity tests; capability and coverage rows (no established host-loss
reaction, unsupported changes, torque refused on MO-D04); Doxygen; report.

## Acceptance

Tests cover direction and range, a negative 0x60FF refused by the preflight, mode refusal on STF, halt
start/stop frames, partial setup, lost start, lost poll and lost stop, expiry yielding the halt, cumulative
delay reaching the operation deadline before the start (no start TX), missed service, urgent stop, unknown
terminal motion, the host-loss capability text, native torque values at INT16 limits, the withheld 0x6087
write refusing the torque operation with zero changing TX, and the refused physical torque unit. No test
claims a deceleration ramp, standstill, holding torque or a host-loss stop. Quick and full verify, firmware
builds and exact-commit CI pass. Hardware: NOT RUN.
