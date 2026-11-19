# 07 — Homing, zero set and limits

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 03-05 and after 08 has landed its `SettingsContext`. MK-D16, D19, D25 and D37 are
central. Use [prompt 77](../77_switch_ready_homing_limits.md)'s protocol-neutral limit, origin and outcome vocabulary
where it has landed;
the MKS input functions and reactions stay in this profile. Review the original homing pages (MK109 p.14-15, p.27-28,
p.65-77), not only the extracts.

## Work

1. Typed homing configuration as a descriptor block added to 08's settings context, owned and tested here with its
   exact frames and its invalidation (step 6's `limitHomingRequired` and the home reference); all treated as
   persistent:
   - 90H: homeTrig (0 low, 1 high when closed), homeDir (0 CW, 1 CCW), homeSpeed uint16 rpm 0..3000, EndLimit,
     hm_mode 0 origin switch, 1 mechanical stop (since 1.0.5), 2 single-turn (since 1.0.9) (MK109 p.67; MK106 p.28);
   - 94H: Hm_offset four bytes, default 0x2000 counts (180°), signedness undocumented so bit 31 set is refused; Hm_ma
     mA, used only by mechanical homing (p.68); defaults differ between editions (MK-D25), so read, never assume;
   - 9EH: limit remap to En/Dir in bus mode, making IN_1/IN_2 invalid and requiring COM at high level (p.14, p.66).
   homeSpeed follows the rpm field rule; whether 16/MStep scaling applies is unstated (MK-D14), so outside 16/32/64
   microsteps only the native value is offered.
2. Method table, each with prerequisites, motion and terminal meaning:

   | hm_mode | Method | Prerequisites | Behavior | Source |
   | --- | --- | --- | --- | --- |
   | 0 | Origin switch | Switch on IN_1, or En with 9EH | Search in homeDir, stop on the rising edge, mark coordinate zero; starting on the switch backs off first | MK109 p.28, p.65 |
   | 1 | Mechanical stop | Hm_ma set low enough for the load | Run at Hm_ma until stalled, move back Hm_offset, mark zero | MK109 p.28, p.65, p.68, p.72 |
   | 2 | Single-turn | 9AH zero point configured | Move within one turn to the stored zero, reset to zero | MK109 p.65, p.69, p.73 |

   Single-turn homing needs the 9AH zero point, and the menu permits setting it only with 0_Mode not Disable, which
   homes automatically at every power-on (MK109 p.27, p.69; MK-D37). The library never writes 9AH, so method 2 is
   admitted only when the application configured 9AH outside the library; the capability output states that this
   arms power-on homing.
3. `prepareHome`/`nextHome`/`advanceHome` with 91H 00 (origin homing per hm_mode; the bare 91H form is equivalent,
   MK109 p.70) and 91H 01 coordinate zeroing (move to the stored zero; motor enabled first, p.66, p.74; V1.0.9 tables
   only). Replies 0 fail, 1 start, 2 complete; 3BH status1 (single-turn) and status2 (other homing) with 0 running,
   1 success, 2 fail, one byte before 1.0.7 (p.35; MK106 p.19); F1H 5 while homing (p.80).
4. Completion: a new status 2 for this pending phase that 02b step 6 attributes (`QUALIFIED`, or `QUIET_INTERVAL`
   reported as that risk), or a fresh 3BH transition to success observed after the trigger.
   A pre-existing 3BH success, F1H 1 or an elapsed deadline is never completion. Only after qualified completion does
   `getHomeReference` publish a host origin generation, with a fresh 31H read recorded; the manual says the marked
   position becomes coordinate zero, but no source establishes the residual reading, so do not require exact zero.
5. Zero set: 92H sets the current position to zero without motion (MK109 p.69) as the native `SET_DEVICE_POSITION`
   action. Require caller-declared stationary admission (no device interlock is documented), then invalidate and
   re-establish host references. It is distinct from homing and from 9AH's "set 0 point".
6. End limits: enabled by 90H EndLimit; left limit (IN_1, or En) stops leftward motion, right limit (IN_2 on 57D only,
   or Dir) stops rightward motion, bus mode only (p.14, p.75). After first use or any limit-parameter change a homing is
   required (p.14, p.67): record `limitHomingRequired` and refuse motion with EndLimit enabled until a homing completes
   after the change. This admission replaces 05's interim EndLimit gate for 05 and 06. "Left" is defined only by
   examples (left with dir bit 1, p.77), so mapping to common limit directions needs `directionQualified`. Status 3 is a
   terminal limit stop; limit hits outside a move send no frame. MK106 says homing onto the left switch with EndLimit
   enabled releases the shaft (MK106 p.12, p.29); MK109 is silent (MK-D16): read 3AH after homing instead of assuming
   the hold.
7. Inputs: 34H levels, configured function and physical wiring are three separate facts (MULTIDRIVE row 21). An
   unconnected input does not disable its function. Never disable limits or remap ports to make a test run. On a 42D,
   08's 9FH can turn IN_1 into an output; method 0 and the left limit on IN_1 are admitted only while the ledger and
   the application's record establish IN_1 as an input (08 step 9).
8. Stop during homing: only the F7H forms are documented; a mode stop code is refused. `V1_0_0` has 91H but no F7H
   (04 step 2), so homing there has no documented stop and is not offered. A stopped or failed homing invalidates the
   host reference.
9. Power-on homing (9AH mode 1 or 2) and the stored zero are read and reported where 00H permits, as a startup motion
   hazard and as a reason restart (10) can move the shaft.

## Deliver

`Homing.h/.cpp`, `HomeContext`, method table, `test/mks_servo_can_homing_test.cpp`, owner-backed app and console routes
(`mks home METHOD`, `mks zero-set`, `mks limits`), API/CLI parity (console routes and parity once S4 lands), coverage
rows, report and PROGRESS row.

## Acceptance

Method admission per hm_mode, firmware and dialect; exact 90H, 94H, 9EH and 91H frames; refused 9AH writes and
single-turn homing without external configuration; completion only from new evidence; stale 3BH success rejected;
reference publication and invalidation; zero set with and without stationary admission; `limitHomingRequired` after a
limit change; stop during homing with F7H only and homing refused on V1_0_0; a 91H status 2 of an ambiguous shape not
completing a new homing; status 3 handling. The full matrix of contract 02 "Registration
acceptance" for this major change and CI on the exact final pushed commit pass. Physical homing needs the actual
switch or mechanical fixture: hardware NOT RUN. Commit and push.
