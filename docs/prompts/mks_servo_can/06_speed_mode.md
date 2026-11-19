# 06 — Speed mode and finite velocity

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 04 and 05's unit helpers. MK-D02, D14 and D15 gate the unit mapping; this stage does
not broaden physical authorization. 05's interim EndLimit gate (05 status) applies here until 07 lands.

## Work

1. Native F6H speed mode: direction bit, 12-bit speed, acc; the 8-byte form adds a uint24 run time in 10 ms units
   (MK109 p.82; since firmware 1.0.7 per the version notes, so absent from `V1_0_0`; the ledger confirms its MK109A
   page). Status replies 0 fail,
   1 start, 2 complete, 3 limit stop, 5 latched (p.83). Bus mode only.
2. Speed conversion helper, shared with 05 (inferences recorded in 01, confirmed against the pages here):
   - V1.0.9 tables: the code is motor rpm at 16, 32 or 64 microsteps (MK109 p.78). Use
     `NativeRateBasis::MOTOR_TURNS`. The page's 8 (×2) and 128 (÷8) are documented examples of a 16/MStep rule, other
     powers of two an inferred extension, non-powers of two unstated (MK-D14); keep the three apart in the ledger.
     Exact common-unit mapping is admitted only at 16, 32 and 64 (review section 16) until 01's ledger records a scoped
     resolution; at any other setting, 8 and 128 included, only an explicit native code with unscaled units is offered
     (as 05 step 3).
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
     rounding and refused above 0xFFFFFF × 10 ms; the drive bounds the run itself, and status 2 is its report. The
     page gives the range 0..0xFFFFFF and no meaning for 0 (MK109 p.82), so a duration that quantizes to 0 is refused
     rather than sent as a possibly unbounded run;
   - the host side is contract 02 "Operation admission", timed runs, which reuses Lichuan's finite-velocity design
     (preparation-anchored duration, caller stop budget fitting before the operation deadline, transfers bounded by
     stop due, `needsStop`). At stop due without step 7's run-time completion (the run-time form's status 2 after the
     run's attributed status 1, captured after the start is settled, no same-code stop handed off since; an
     attributed 2 alone, or one captured while the start may still transmit, is not it, 04 step 4 settlement first)
     the application composes its separate urgent stop from 04 (`MODE_RAMPED`, `MODE_IMMEDIATE` or an F7H form the
     dialect has, per caller policy), never a repeated start and never barred by the run's deadline; the drive's own
     run time does not replace it. The selected stop must be a stop path under 04 step 2 (usable from every
     reachable state of the run; plain F7H only with specimen evidence). A run whose dialect and mode have no
     reviewed stop row (04 step 2), or whose selected stop is no stop path and which has no declared independent
     stop, is refused before its first changing TX;
   - without the run-time form (`V1_0_0`) that host stop is the run's only bound;
   - zero speed is the stop form and is refused as a run; nonzero finite duration is required.
5. Native continuous F6H (no run time) is exposed only as an explicit native operation whose context retains the stop
   obligation; the common finite API never reaches it. Re-sending F6H while running has no documented live-update
   meaning (only F5H documents one), so `prepareVelocityChange` is unsupported until evidence exists. MK109 p.2 fixed a
   "two commands needed to start speed mode with limits" defect in the Sep-2026 build: on `V1_0_9_NOV2025` record that
   a start with EndLimit enabled may not take effect, and never resend automatically.
6. Direction: dir 0 is CCW in the field table and "forward" in the example (MK109 p.82); 32H is CCW-positive in V1.0.9
   (p.32) and CW-positive in V1.0.0 (MK100 p.11). Common signed velocity needs `directionQualified` (03, MK-D02).
7. Stops: 04's `MODE_RAMPED` and `MODE_IMMEDIATE` with F6H speed 0 (p.84) and the F7H forms the dialect has. A
   run-time completion, limit stop or stop all end the context; the reply code is shared, so MK-D36 rules apply: a
   `possiblyLate` status 2 ends nothing, and one attributed `QUIET_INTERVAL` ends the run with that correlation (05
   step 7); otherwise the host deadline and its stop decide. Only the run-time form's status 2, after the run's
   attributed status 1, captured after the start is settled and before any same-code stop is handed off, is
   run-time completion and discharges the run's obligation (04 step 4 terminal evidence, settlement first). After a same-code stop a status 2 is 04 step 3's observation, and a
   host-bounded or continuous run has no terminal evidence of its own: each is discharged only by a 04 stop that meets
   04 step 4's discharge ordering (one already handed off counts if it does) and its post-stop observation. An
   attributed status 3 is a drive-side limit stop: it ends the run, and it discharges the obligation through the
   post-stop observation alone, with no host stop frame, only when its capture follows the settlement of every
   earlier changing frame to the address (the same ordering). Until then the address admits no other changing work.
8. FFH power-on auto-run stores the last F6H command and runs it at every power-on (MK109 p.85); its enable write also
   stops the current run ("the motor will slow down and stop", p.87). The library never sends FFH; where 00H can read
   it, the state is reported as a startup motion hazard.
9. 32H speed is a firmware report in rpm, not measured shaft speed; F1H states 2-4 (accelerating, decelerating, full
   speed) are firmware reports, not physical ramp evidence.

## Deliver

`Velocity.h/.cpp`, `VelocityContext`, shared unit and ramp helpers, `test/mks_servo_can_velocity_test.cpp`, owner-backed
app and console routes (`mks velocity`, native `mks run`), API/CLI parity (console routes and parity once S4 lands),
lost-start, lost-stop and missed-service cases, coverage rows, report and PROGRESS row.

## Acceptance

Exact F6H 5- and 8-byte frames per dialect; speed and acc conversion boundaries with reported quantization; exact
mapping refused at 8 microsteps (a documented example outside the qualified set) and at a non-power of two, native
code still offered; refused over-limit speed, zero-speed run, missing duration and a duration that quantizes to run
time 0; an old F6H status 2 during a new run not ending it before the quiet interval; run-time completion followed
by a new run admitted; a status 2 after a same-code host stop, attributed `QUIET_INTERVAL` with the stop's reply
withheld, leaving the obligation until 04's post-stop observation and a new run refused before it; host
stop at stop due, the timed-run tests of contract 02 "Operation admission" (a duration that no longer fits with the
stop budget, an observation in flight near stop due, service resuming after stop due, a queue-pressured stop), a
lost start reply, lost stop reply, limit stop and deadline without service; with the start reply withheld, an
attributed run-time status 2 not suppressing the urgent stop at stop due; a start held below the owner queue while
a status 1 and a run-time status 2 attributed `QUIET_INTERVAL` are captured, then transmitting late: the urgent stop
still composed at stop due and the obligation retained; stop path (04 step 2): a run selecting plain F7H on either
V1.0.9 table refused before TX without specimen evidence, a mode stop or F7H 01, or a declared independent stop, and
admitted with each; with a declared independent stop, the owner-backed application test asserting its fallback
invocation when the urgent native stop is not admitted, misses its dispatch window or leaves its effect unconfirmed
at the fallback deadline, a fallback deadline already elapsed at dispatch of the start refusing it, and service
resuming after that deadline invoking the declared stop at once with the missed deadline and unknown motion in the
result (contract 02 timed runs); a limit status 3 captured before a held
earlier frame's final TX report, that frame then transmitting, discharging nothing; no repeated start; native
continuous run keeps its stop obligation. The full matrix of contract 02 "Registration acceptance" for this major change and CI on
the exact final pushed commit pass. Hardware NOT RUN: no unbounded run or moving link-loss test substitutes for a
finite free-shaft test. Commit and push.
