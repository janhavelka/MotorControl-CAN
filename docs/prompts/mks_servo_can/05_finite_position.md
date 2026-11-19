# 05 — Finite position

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 03 and 04. MK-D02, D14, D19, D29 and D30 directly affect this stage; MK-D36 affects
stop interaction.

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
   evidence. Restart, bus-off, rebinding and 92H/91H/3FH invalidate it.
5. Relative moves are relative to the drive's current position (MK109 p.17); `MoveRelation` names that native basis.
   Never convert a relative intent into an absolute target or the reverse.
6. Direction (MK-D02): dir 0 is called CCW in the field table and "forward" or "clockwise" in examples, and 31H text
   and examples disagree (MK109 p.31-32, p.82, p.88, p.91; MK100 p.11-12). Common positive direction needs 03's
   `directionQualified` declaration; without it only native signed requests are accepted, labelled unqualified.
7. Completion phases (MK109 p.47, p.89):
   - immediate 1 is `ACKNOWLEDGED` (start), 0 `REJECTED`, 5 latched (09 owns synchronization), no reply `UNKNOWN`;
   - an unsolicited 2 is native completion only when the declared reply policy has the active byte set, the pending
     phase belongs to this motion and the code's completion shape is outside 02b's ambiguous set (correlation
     `QUALIFIED`), or after the application's quiet interval with correlation `QUIET_INTERVAL` reported as such; it
     reflects the 95H threshold and "may not" arrive (p.50, p.89). Otherwise the 2 is published `possiblyLate` and
     does not end the phase: after a cancelled, timed-out, stopped or superseded same-code move A, A's delayed 2
     arriving during this move B is never B's completion, and B completes only through the endpoint check below,
     read after B's start, or ends at its deadline;
   - 3 is a terminal limit stop, never completion; a status 2 after a same-code stop is "ended, cause
     indistinguishable" (04);
   - endpoint evidence for encoder moves is a fresh 31H within a caller tolerance of this move's target; for pulse
     moves 33H is not an endpoint proof until MK-D30 is resolved; standstill is a separate F1H/32H observation; both
     reads carry their own correlation (02b step 6) into the result.
   A common completion contract that the binding cannot satisfy (respond-off, OPEN mode with 95H disabled, unknown
   calibration) is refused before start; the explicit native acknowledged-only path states its weaker outcome.
8. F5H live update: a new F5H while running changes speed and target (MK109 p.100, p.104), since firmware 1.0.5
   (MK109 p.1). Offer it only in the V1.0.9 tables, never on `V1_0_0` (MK100 documents none), and only as an explicit
   change operation on the admitted move (`MoveRelation`, prompt 52 vocabulary): after it, any 2 is attributed to
   "latest or superseded target", F5H's shapes enter 02b's ambiguous set, and completion requires the endpoint check
   against the latest target. Do not count completions. Other codes have no documented live update; a second command
   while moving is refused.
9. Open loop: SR_OPEN runs without encoder control (MK109 p.23); completion and endpoint semantics there are unqualified
   and the manual's open-loop workaround for magnetless motors disables 95H (p.80, p.88). Report this per binding.
10. Interruption and failure: 04's stops; lost immediate reply leaves execution `UNKNOWN` with a stop obligation; never
    resend a move. A stall trip or host-silence stop can end the move silently (MK-D17, MK-D18).

## Deliver

`Position.h/.cpp`, `MoveContext`, native move records, `test/mks_servo_can_position_test.cpp`, owner-backed app and
console routes (`mks moveby`, `mks moveto`, native `mks move CODE ...`), API/CLI parity, coverage rows, report and
PROGRESS row. Use the existing axis and move tests as structural examples; extract no generic engine.

## Acceptance

Exact frames for each command and dialect; range boundaries on native values; unit and rounding cases for counts and
pulses; refused speed 0, over-limit speed and unscaled microsteps; absolute without reference refused; restart
invalidation; immediate, completion, limit and latched phases; completion withheld, late, after a stop and after an
F5H update; F5H update refused on V1_0_0; an old same-code status 2 arriving during a new move B (after A's cancel,
timeout, same-code stop or supersession) not completing B, before and after the chosen quiet interval (`possiblyLate`,
then `QUIET_INTERVAL`, never `QUALIFIED`), with B completing by endpoint check; lost start reply with no resend;
endpoint check. The full matrix of contract 02 "Registration acceptance"
for this major change and CI on the exact final pushed commit pass. Hardware NOT RUN. Commit and push with the API and
coverage updates.
