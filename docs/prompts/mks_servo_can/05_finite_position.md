# 05 — Finite position

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 03 and 04. MK-D02, D14, D19, D29 and D30 directly affect this stage; MK-D36 affects
stop interaction. Until 07 lands its limit admission (07 step 6), motion here and in 06 is admitted only when 90H
EndLimit is established off (a 00H 90 read where 03 established it readable, or a caller declaration with its
evidence reference); enabled or unestablished EndLimit refuses before TX.

## Work

1. Map `prepareMoveAbsolute`, `prepareMoveRelative` and `prepareMoveAngle` with `nextMove`/`advanceMove` onto the four
   native commands, all bus-mode only (MK109 p.88-104):

   | Native | Position field | Basis | Common use |
   | --- | --- | --- | --- |
   | F4H | int24 relative encoder counts | Current 31H coordinate | Preferred relative move |
   | F5H | int24 absolute encoder counts | 31H coordinate origin | Preferred absolute and angle move |
   | FDH | uint24 pulses, sign in the direction bit | Current position | Relative move with a known microstep setting |
   | FEH | int24 absolute pulses | 33H pulse origin (MK-D30) | Native only until 33H is qualified as a position |

   Encoder counts are 16384 per motor turn independent of microsteps (MK109 p.17, p.96). Pulses are MStep × 200 per
   turn for the required 1.8° motor (p.17, p.115); a pulse conversion needs the caller's `UnitConfig`, a readback or
   declared microstep value (84H value 256 refused, MK-D29) and no inferred geometry.
2. Check the prepared native value, not the user coordinate: the absolute target and the relative displacement each
   within -8388607..+8388607 (about ±512 turns), FDH magnitude within 0..0xFFFFFF. A 31H coordinate is int48, so an
   absolute target can be unreachable from a far coordinate; refuse before TX instead of truncating. Exact rounding and
   the quantization error are reported through the existing axis preparation.
3. Speed and acc fields use 06's conversion helpers (implement them here first if 05 lands before 06, then reuse):
   speed code per dialect and microstep rule, acc code per the ramp rule. Speed 0 is the stop form and is refused for a
   move. Requests above the declared work mode's limit (OPEN 400, CLOSE 1500, vFOC 3000 rpm, MK109 p.78) are refused:
   the drive clamps silently. Whether the 16/MStep speed rule applies to F4H/F5H is unstated (MK-D14): outside 16, 32
   and 64 microsteps an exact speed mapping is refused and only an explicit native code with unscaled units is allowed.
4. Absolute reference (MK-D19): absolute moves "must" follow a zeroing (MK109 p.92, p.100) while examples say the
   power-on position is zero (p.95, p.103). Admit an absolute common move only with a host reference generation
   established after the last restart by 07's homing or zero set, or by an explicit caller declaration recorded in the
   evidence. Restart, bus-off, rebinding and 92H/91H/3FH invalidate it. The protocol reports no boot-up (review
   section 14 rule 10), so an uncommanded power cycle resets the origin unseen, and a 40H read that agrees with the
   earlier one establishes no continuity ([README](README.md) open question 3). Until that question is answered, an
   absolute common move also needs the application's continuity record for that reference (an evidence reference,
   such as supply monitoring or an operator record, that no power cycle occurred since the reference was
   established), recorded in the result as accepted risk; without it absolute moves refuse before TX. Relative
   moves do not depend on the origin and stay available.
5. Relative moves are relative to the drive's current position (MK109 p.17); `MoveRelation` names that native basis.
   Never convert a relative intent into an absolute target or the reverse.
6. Direction (MK-D02): dir 0 is called CCW in the field table and "forward" or "clockwise" in examples, and 31H text
   and examples disagree (MK109 p.31-32, p.82, p.88, p.91; MK100 p.11-12). Common positive direction needs 03's
   `directionQualified` declaration; without it only native signed requests are accepted, labelled unqualified.
7. Completion phases (MK109 p.47, p.89):
   - immediate 1 is `ACKNOWLEDGED` (start), 0 `REJECTED`, 5 latched (09 step 7 owns synchronization and a latched
     move's completion, which has no status 1), no reply `UNKNOWN`;
   - an unsolicited 2 ends this motion's pending phase only when the declared reply policy has the active byte set,
     the phase belongs to this motion and 02b step 6 attributes the frame: `QUALIFIED` (shape outside the ambiguous
     set) or `QUIET_INTERVAL` (the application's accepted interval has passed; reported as that risk, never as
     `QUALIFIED`). It reflects the 95H threshold and "may not" arrive (p.50, p.89). Otherwise the 2 is `possiblyLate`
     and ends nothing. So a delayed 2 of an earlier same-code move A, whether A was cancelled, timed out, stopped,
     superseded or had completed, ends this move B only after the interval and only with `QUIET_INTERVAL`
     correlation: that is the risk the interval accepts;
   - 3 is a terminal limit stop, never completion; a status 2 after a same-code stop is "ended, cause
     indistinguishable" (04);
   - three outcomes stay separate. `endpointObserved`: a 31H reply to a read sent after B's latest trigger was
     acknowledged, attributed `QUALIFIED` or `QUIET_INTERVAL` (never `possiblyLate`; 31H's stream state `OFF`, 03 step
     8) and within the caller's tolerance of B's latest target. `completed`: the latest trigger's status 1 attributed
     the same way (B's start, or after an F5H update the update's), then B's attributed native 2 with no same-code
     stop handed off since that trigger (04 step 3), or a standstill
     window: F1H 1 with 32H 0, then a 31H read giving `endpointObserved`, then F1H 1 with 32H 0 again, each read
     handed off after the previous reply, all attributed the same way, with no event between them that may start
     motion or invalidate evidence (a motion, stop or multi-target frame reaching the address, restart, bus-off,
     rebinding, a recorded power cycle). A 31H value sampled before the window's first standstill read completes
     nothing, because the shaft may have crossed the tolerance window and stopped elsewhere, after a stall trip or a
     host-silence stop too. `stopObligationDischarged`: only by `completed` or by 04's stop with its post-stop
     observation (04 step 4). `endpointObserved` alone discharges nothing: after a lost start reply, with a target already
     within tolerance, or while the shaft crosses the tolerance window, execution stays `UNKNOWN` and the stop
     obligation and 04's stops apply. For pulse moves 33H is not an endpoint proof until MK-D30 is resolved. Each
     outcome carries the weakest correlation of its evidence.
   A common completion contract that the binding cannot satisfy (respond-off, OPEN mode with 95H disabled, unknown
   calibration) is refused before start; the explicit native acknowledged-only path states its weaker outcome.
8. F5H live update: a new F5H while running changes speed and target (MK109 p.100, p.104), since firmware 1.0.5
   (MK109 p.1). Offer it only in the V1.0.9 tables, never on `V1_0_0` (MK100 documents none), and only as an explicit
   change operation on the admitted move (`MoveRelation`, prompt 52 vocabulary), admitted as that move's same-intent
   transition under 04 step 4: the move still live, its latest trigger's status 1 attributed, no frame to the
   address open TX, no uncertain execution or retained obligation and no request outstanding; an update whose
   predecessor (the start or an earlier update) may still transmit refuses before TX. After it, any 2 is attributed to
   "latest or superseded target", F5H's shapes enter 02b's ambiguous set, and completion follows step 7's outcomes
   against the latest target, starting from the update's own acknowledgement. Do not count completions. Other codes
   have no documented live update; a second command
   while moving is refused.
9. Open loop: SR_OPEN runs without encoder control (MK109 p.23); completion and endpoint semantics there are unqualified
   and the manual's open-loop workaround for magnetless motors disables 95H (p.80, p.88). Report this per binding.
10. Interruption and failure: 04's stops; lost immediate reply leaves execution `UNKNOWN` with a stop obligation; never
    resend a move. A stall trip or host-silence stop can end the move silently (MK-D17, MK-D18).

## Deliver

`Position.h/.cpp`, `MoveContext`, native move records, `test/mks_servo_can_position_test.cpp`, owner-backed app and
console routes (`mks moveby`, `mks moveto`, native `mks move CODE ...`), API/CLI parity (console routes and parity once
S4 lands), coverage rows, report and PROGRESS row. Use the existing axis and move tests as structural examples; extract
no generic engine.

## Acceptance

Exact frames for each command and dialect; range boundaries on native values; unit and rounding cases for counts and
pulses; refused speed 0, over-limit speed and unscaled microsteps; absolute without reference refused; a simulated
drive reset with a changed origin and an unchanged 40H, then an absolute move refused without the continuity record;
motion refused with EndLimit enabled or unestablished before 07 lands; restart
invalidation; immediate, completion, limit and latched phases; completion withheld, late, after a stop and after an
F5H update; F5H update refused on V1_0_0; an F5H update admitted on a live move whose start is acknowledged and
proved transmitted, and the same update refused with zero TX while the start is held below the owner queue without a
final report, after a lost start reply and after a stop; a `completed` move followed by a new move admitted, and a
move ended at its deadline followed by a new move refused until 04's stop and post-stop observation; a status 2 after
a same-code stop leaving `completed` unset; an old same-code status 2 arriving during a new move B (after A's cancel,
timeout, same-code stop, supersession or completion): before the chosen quiet interval `possiblyLate` with B not
ended, after it B ended with `QUIET_INTERVAL` correlation, never `QUALIFIED`; lost start reply with no resend;
endpoint outcomes, where a target already within tolerance with a lost start reply, an in-motion crossing of the
tolerance window, a delayed (`possiblyLate`) 31H value and an F5H update without its own acknowledgement each leave
`completed` and `stopObligationDischarged` unset; an acknowledged start, a 31H read within tolerance while the shaft
crosses the window and fresh F1H 1 and 32H 0 reports after it stopped outside the window (`completed` unset, stop
obligation retained), and a standstill window interrupted by a stop or a restart; a 31H reachability read that meets
its own timeout while the
operation deadline arrives before the trigger (no trigger TX). The full matrix of contract 02 "Registration acceptance"
for this major change and CI on the exact final pushed commit pass. Hardware NOT RUN. Commit and push with the API and
coverage updates.
