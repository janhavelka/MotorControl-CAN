# 05 — Units and finite position (Profile Position)

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03, 04 and 04b. NT-D10, NT-D18 and NT-D19 directly affect it.
Follow [prompt 60](../60_user_units.md) for host-side user units and
[prompt 52](../52_motion_changes.md) for later change operations.

## Work

1. Unit relation. The native position unit is set by 60A8h (bits 16-23 unit, 24-31
   power of ten; preset FF410000h = 0.1 degree), with gear 6091h applying to every
   unit except encoder increments B5h, steps ACh and electrical poles C0h, and feed
   6092h for length and dimensionless units (N5 pp61-64, p390). Velocity is 60A9h
   (preset 00B44700h = rpm; trust the field diagram of p64/p391 over the mislabelled
   p65 examples, NT-D10) times 6096h; acceleration is velocity per second times 6097h;
   jerk times 60A2h. From one configuration generation, derive an exact rational
   relation between native units and motor turns, and publish it only when the caller
   qualifies it for that generation. FIR-v1650's 2000 increments per revolution
   (FWUPD p5) never applies to an admitted row.
2. Generation rules. Unit and scaling objects written in Operation enabled take
   effect only after leaving that state (N5 p62; 3203h likewise, p298), so a unit
   write observed while enabled makes the interpretation pending, and motion is
   refused until the state has been left and the objects read again. Any change to
   60A8h, 60A9h, 6091h, 6092h, 6096h, 6097h, 3202h, 3203h, 2030h or 608Fh invalidates
   scale and reference generations.
3. Rate basis gap. `NativeRateBasis` offers `COMMAND_INCREMENTS` and `MOTOR_TURNS`.
   rpm with 6091h = 1:1 and 6096h = 1 is motor turns per minute; with another gear it
   is driven-shaft turns. Support the configurations the existing bases represent
   exactly and refuse the rest before TX. A new basis is a reviewed core change under
   canopen_shared 02, not a profile shortcut.
4. Feedback. 607Ah target and 6064h actual are I32 user units; 6062h is demand;
   6063h is raw increments and invalid when 608Fh = 0 (N5 pp360-361). In open loop
   6064h follows the commanded field unless 3203h selects an encoder (N5 p300), so
   the result reports commanded position, never a measured one. Both motor classes
   use the same preparation; full-step geometry stays optional (MULTIDRIVE case 1).
5. PP sequence (N5 pp68-75). Admission: identity, exclusive control, Operation
   enabled with mode 1 confirmed by 6061h, fresh 6040h bit 4 = 0. Stage 607Ah, 6081h,
   6083h, 6084h and 6086h (0 trapezoid; 3 jerk-limited with 60A4h:01-04), each
   confirmed. Trigger: one write with bit 4 rising, bit 5 = 1 (apply immediately),
   bit 6 per intent, bit 8 = 0. Evidence: set-point acknowledge bit 12 after the
   trigger, then clear bit 4 unless 60F2h bits 4-5 make the drive do it (N5 p407);
   arrival is bit 10 observed after the acknowledgement together with 6064h inside
   6067h for 6068h. Bit 10 is also set when halted at standstill, so a halted move
   never reports arrival. Bit 13 (following error) is valid in closed loop only, and
   3700h = -1 means the drive does not react to it (N5 p337). The first subset admits
   one set-point; buffered and chained set-points (bit 9, overshoot N5 p73) wait for
   the prompt 52 lifecycle.
6. Relative moves. Bit 6 = 1 is relative to the reference selected by 60F2h bits 0-1;
   the preset 1 (ramp generator output) differs from the examples' 0 (N5 pp72-73,
   p407; NT-D19). A common relative move needs the caller to qualify the read 60F2h
   basis; otherwise it returns `UNSUPPORTED_BASIS`. A native relative request keeps
   its raw intent and an acknowledged-only result.
7. Limits. 607Dh:01/02 limits absolute targets, ignores the home offset, and outside
   the range accepts only moves towards it (N5 p67, pp372-373); its 0/0 preset is not
   known to mean unlimited (NT-D18), so host limits are separate and 607Dh is reported
   raw. 607Bh and 60F2h bits 6-7 (rotary options, NT-D19 cites 607Dh by mistake) are
   refused in the first subset. 607Fh and 6080h cap 6081h.

## Deliver

`profiles/nanotec_canopen/Units.h/.cpp` (relation and invalidation) and
`Position.h/.cpp` with `MoveContext`, `prepareMoveAbsolute`, `prepareMoveRelative`,
`prepareMoveAngle`, `nextMove`, `advanceMove`; `test/nanotec_canopen_units_test.cpp`
and `test/nanotec_canopen_position_test.cpp` with stepper and BLDC configurations;
console routes `nanotec moveto`, `moveby` and `move` with parity tests.

## Acceptance

Tests cover exact quantization and range at I32 boundaries, gear and feed rationals,
refused rate bases, pending unit writes in Operation enabled, open-loop commanded
feedback, staging order, a lost trigger reply (UNKNOWN, no replay), a stale bit 10
or bit 12 before the trigger, halt during the move, quick stop during staging, the
60F2h basis rule and no relative-to-absolute conversion. Quick and full
verification, firmware builds and CI on the exact final pushed commit pass.
Hardware NOT RUN; no physical scale or arrival accuracy is claimed.
