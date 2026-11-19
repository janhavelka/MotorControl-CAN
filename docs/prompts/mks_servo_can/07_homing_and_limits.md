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
4. Completion needs the 91H start's status 1 attributed by 02b step 6 (MK109 p.70), then a new status 2 for this
   pending phase that 02b step 6 attributes (`QUALIFIED`, or `QUIET_INTERVAL` reported as that risk), or a fresh 3BH
   transition to success observed after that status 1, the 2 captured, or the 3BH reads handed off, after the 91H
   frame is settled (04 step 4 settlement first). A 3BH transition does not resolve a lost or unattributed
   start reply (lead default, round 5): homing can also start from the menu GoHome (p.14, p.28) or at power-on
   (p.27), and no page ties a 3BH value to one 91H (inference), so it stays an observation and 04 step 4's uncertain
   execution holds.
   A pre-existing 3BH success, F1H 1 or an elapsed deadline is never completion. This completion is the homing's
   terminal evidence under 04 step 4 and leaves the address free for new admission; a failed, stopped or expired
   homing, or one with an unattributed start, leaves a retained obligation for 04's stop and post-stop observation
   under 04 step 4's discharge ordering. Only after qualified completion does
   `getHomeReference` publish a host origin generation, with a fresh 31H read recorded; the manual says the marked
   position becomes coordinate zero, but no source establishes the residual reading, so do not require exact zero.
5. Zero set: 92H sets the current position to zero without motion (MK109 p.69) as the native `SET_DEVICE_POSITION`
   action. Require caller-declared stationary admission (no device interlock is documented), then invalidate and
   re-establish host references. It is distinct from homing and from 9AH's "set 0 point".
6. End limits: enabled by 90H EndLimit; left limit (IN_1, or En) stops leftward motion, right limit (IN_2 on 57D only,
   or Dir) stops rightward motion, bus mode only (p.14, p.75). After first use or any limit-parameter change a homing is
   required (p.14, p.67): record `limitHomingRequired`. Limit admission replaces 05's interim EndLimit gate for 05 and
   06 and fails closed, per binding generation and identity session (03 step 5). Ordinary motion is admitted only with
   EndLimit established off (an attributed 00H 90 read where 03 established it readable, or a caller declaration with
   its evidence reference), or established on with the input assignment and wiring of step 7 and a homing completed
   under step 4 after the last possible limit-parameter change, in this binding generation or by the application's
   commissioning record (evidence reference) that a homing (91H or the menu GoHome, p.14) followed the last change
   with none since. Anything else refuses ordinary motion before TX: no established value, first attachment,
   rebinding, a declaration change, restart or bus-off, 3FH, and `limitHomingRequired`, which a possibly applied 90H,
   94H, 9EH or 9FH write sets, also with its reply lost (08 step 9); a read or declaration requalifies only once that
   write is settled (08 step 9, 04 step 4 discharge ordering). Stops and step 3's homing, the requalifying
   route, stay admitted under their own gates. "Left" is defined only by
   examples (left with dir bit 1, p.77), so mapping to common limit directions needs `directionQualified`. Status 3 is a
   terminal limit stop; limit hits outside a move send no frame. MK106 says homing onto the left switch with EndLimit
   enabled releases the shaft (MK106 p.12, p.29); MK109 is silent (MK-D16): read 3AH after homing instead of assuming
   the hold.
7. Inputs: 34H levels, configured function and physical wiring are three separate facts (MULTIDRIVE row 21). An
   unconnected input does not disable its function. Never disable limits or remap ports to make a test run. On a 42D,
   08's 9FH can turn IN_1 into an output; method 0 and the left limit on IN_1 are admitted only while the ledger and
   the application's record establish IN_1 as an input (08 step 9).
8. Stop during homing: only the F7H forms are documented; a mode stop code is refused. `V1_0_0` has 91H but no F7H
   (04 step 2), so homing there has no documented stop and is not offered. Homing and 91H 01 zeroing are host-ended
   runs under contract 02 "Operation admission", timed runs (its homing sentence), cited, not restated: the caller's
   latest interruption time takes the duration's place, an optional travel bound uses attributed 31H reads no older
   than a caller-supplied age, and the same stop budget, transfer cap and stop path apply; at stop due without step
   4's completion the application composes its separate urgent F7H stop, and `needsStop` is 04 step 4's retained
   obligation. The stop is the dialect's F7H form, admitted without a fresh state report (04 step 1), and it must be
   a stop path under 04 step 2: F7H 01 on `V1_0_9_SEP2026`; plain F7H, the only form on `V1_0_9_NOV2025`, only with
   the application's specimen evidence at the homing's current (12A Q13), because the low-current limit (MK109
   p.81) concerns current, not the method: mechanical homing runs at Hm_ma, "just enough to drive the load" (p.68,
   p.72), and Hm_ma does not apply to the other methods (p.68), whose current no page states. Without that evidence
   a `V1_0_9_NOV2025` homing of any method needs an independent stop the application declares for the run (its own
   enable, STO or power path, named in the result) with its fallback deadline under contract 02 timed runs,
   otherwise it is refused before its first changing TX (round 6, extending round 5's mechanical-only rule). A
   stopped or failed homing invalidates the host reference.
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
limit change; limit admission at first attachment, after a lost 90H or 9EH reply and after a possibly applied 9FH
change, each with zero ordinary motion TX until a read or declaration and a homing or commissioning record requalify
it, also an old-value read taken while the write was held below the owner queue requalifying nothing (08 step 9's
delayed-write ordering for 90H, tested here with its descriptor: a possibly sent 90H held below the owner queue, an
old-value readback before its settlement, the late TX applying it and its final report, then ordinary motion refused
at admission); stop during
homing with F7H only and homing refused on V1_0_0; a 91H status 2 of an ambiguous shape not completing a new
homing; a lost 91H start reply followed by a fresh 3BH running-then-success transition leaving execution uncertain
and the obligation for 04's stop; a qualified homing completion followed by a new motion admitted, and an expired
homing followed by motion refused until 04's stop and post-stop observation; the timed-run cases of contract 02
"Operation admission" for homing (an interruption time that no longer fits with the stop budget, a missing switch
reaching stop due, a poll in flight at stop due, service resuming late), each asserting the composed F7H stop
intent and not only the expired home result; Nov-2025 homing, mechanical and origin-switch, refused before TX
without specimen evidence or the declared independent stop and admitted with either, Sep-2026 homing admitted with
F7H 01; with a declared independent stop, the owner-backed application test asserting its fallback invocation when
the urgent F7H is not admitted, misses its dispatch window or leaves its effect unconfirmed at the fallback deadline,
and late service invoking it at once (contract 02 timed runs); a 91H held below the owner queue while a status 1 and
a status 2 attributed `QUIET_INTERVAL` are captured, then transmitting: no completion, the stop still owed at stop
due; status 3 handling. The full matrix of contract 02 "Registration
acceptance" for this major change and CI on the exact final pushed commit pass. Physical homing needs the actual
switch or mechanical fixture: hardware NOT RUN. Commit and push.
