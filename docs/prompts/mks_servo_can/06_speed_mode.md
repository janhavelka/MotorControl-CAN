# 06 — Speed mode and finite velocity

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 04 and 05's unit helpers. MK-D02, D14 and D15 gate the unit mapping; this stage does
not broaden physical authorization.

## Work

1. Native F6H speed mode: direction bit, 12-bit speed, acc; the 8-byte form adds a uint24 run time in 10 ms units
   (MK109 p.82; since firmware 1.0.7 per the version notes, so absent from `V1_0_0`; the ledger confirms its MK109A
   page). Status replies 0 fail,
   1 start, 2 complete, 3 limit stop, 5 latched (p.83). Bus mode only.
2. Speed conversion helper, shared with 05 (inferences recorded in 01, confirmed against the pages here):
   - V1.0.9 tables: the code is motor rpm at 16, 32 or 64 microsteps; other settings scale by 16/MStep (MK109 p.78:
     8 gives ×2, 128 gives ÷8). Use `NativeRateBasis::MOTOR_TURNS`. The rule for non-power-of-two microsteps is unstated
     (MK-D14): refuse exact mapping there.
   - `V1_0_0`: rpm = code × 6000/(MStep × 200) (MK100 p.22), so one code unit is 100 command pulses per second; use
     `NativeRateBasis::COMMAND_INCREMENTS` with range 0..1600. MK106 p.55's "0x140 = 600 rpm" is this V1.0.0 formula,
     while MK106 p.40 and MK109 p.82 give 320 rpm for 1.0.1+ (MK-D15); never mix the two.
   - Quantize with the existing exact conversion and report the error; refuse values above the declared mode's limit
     (p.78) rather than relying on the silent clamp.
3. Acceleration helper: for the V1.0.9 tables the speed changes by 1 rpm every (256 − acc) × 50 µs (MK109 p.79), a rate
   of 20000/(256 − acc) rpm/s (inference; acc 236 gives the manual's 1 rpm per ms table), acc 0 meaning no ramp. For
   `V1_0_0` the code changes by acc every 10 ms (MK100 p.23). Express both as `RampKind::ACCELERATION` through
   `convertNativeRamp`, with the nonlinear code table in the profile and explicit rounding; never a ramp time. Whether
   the 1 rpm step scales with microsteps is unstated: outside 16/32/64 refuse exact acceleration mapping.
4. `prepareVelocity` (finite only) with `nextVelocity`/`advanceVelocity`/`serviceVelocity`:
   - when the dialect has the run-time form, the duration becomes F6H run time, quantized to 10 ms with explicit
     rounding and refused above 0xFFFFFF × 10 ms; the drive bounds the run itself, and status 2 is its report;
   - the host keeps its own immutable deadline (run time plus the caller's margin). Reaching it without status 2
     yields a stop (`MODE_RAMPED`, `MODE_IMMEDIATE` or F7H per caller policy) as new intent, never a repeated start;
   - without the run-time form (`V1_0_0`) the host bounds the run: the context yields the stop at expiry and keeps
     `needsStop` with execution unknown when transport or service loss prevents a confirmed stop;
   - zero speed is the stop form and is refused as a run; nonzero finite duration is required.
5. Native continuous F6H (no run time) is exposed only as an explicit native operation whose context retains the stop
   obligation; the common finite API never reaches it. Re-sending F6H while running has no documented live-update
   meaning (only F5H documents one), so `prepareVelocityChange` is unsupported until evidence exists. MK109 p.2 fixed a
   "two commands needed to start speed mode with limits" defect in the Sep-2026 build: on `V1_0_9_NOV2025` record that
   a start with EndLimit enabled may not take effect, and never resend automatically.
6. Direction: dir 0 is CCW in the field table and "forward" in the example (MK109 p.82); 32H is CCW-positive in V1.0.9
   (p.32) and CW-positive in V1.0.0 (MK100 p.11). Common signed velocity needs `directionQualified` (03, MK-D02).
7. Stops: 04's `MODE_RAMPED` and `MODE_IMMEDIATE` with F6H speed 0 (p.84) and the F7H forms. A run-time completion,
   limit stop or stop completion all end the context; the reply code is shared, so MK-D36 rules apply.
8. FFH power-on auto-run stores the last F6H command and runs it at every power-on (MK109 p.85); its enable write also
   stops the current run ("the motor will slow down and stop", p.87). The library never sends FFH; where 00H can read
   it, the state is reported as a startup motion hazard.
9. 32H speed is a firmware report in rpm, not measured shaft speed; F1H states 2-4 (accelerating, decelerating, full
   speed) are firmware reports, not physical ramp evidence.

## Deliver

`Velocity.h/.cpp`, `VelocityContext`, shared unit and ramp helpers, `test/mks_servo_can_velocity_test.cpp`,
owner-backed app and console routes (`mks velocity`, native `mks run`), API/CLI parity, lost-start, lost-stop and
missed-service cases, coverage rows, report and PROGRESS row.

## Acceptance

Exact F6H 5- and 8-byte frames per dialect; speed and acc conversion boundaries with reported quantization; refused
unscaled microsteps, over-limit speed, zero-speed run and missing duration; run-time completion, host deadline stop,
lost start reply, lost stop reply, limit stop and deadline without service; no repeated start; native continuous run
keeps its stop obligation. The full matrix of contract 02 "Registration acceptance" for this major change and CI on
the exact final pushed commit pass. Hardware NOT RUN: no unbounded run or moving link-loss test substitutes for a
finite free-shaft test. Commit and push.
