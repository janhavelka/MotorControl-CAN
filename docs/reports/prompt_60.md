# Prompt 60 report: user units for speed and acceleration

State: In progress; stage (a) complete, stage (b) next. Run by the unattended Claude loop of 2026-11-23
([COMMON "Owner absence", Schedule](../prompts/COMMON.md#owner-absence-20-november-to-about-4-december-2026)).

## Starting point

- Starting revision: `b3f6f364` on `main` (equal to `origin/main`).
- Prompt: [60_user_units.md](../prompts/60_user_units.md), read in full.
- Consumed artifacts: prompt 10 (Complete): `Units.h`, `Axis.h` and `prepareVelocityTarget`; prompt 59 (Partial, no
  report of its own): the bench records it ran early (2026-11-13 ladder, 2026-11-16 campaign). Prompt 59 did not
  record the ramp law, which prompt 60 consumes; stage (a) therefore checked the retained raw records for it (below).

## Source audit (stage (a), 2026-11-27)

| Sibling | HEAD and state | Behavior adopted or rejected (path, symbol) |
| --- | --- | --- |
| MotorControl-RS | `831f3e7` (2026-11-27), dirty in `examples/probe_cli/InputStartApp.h` and `test/input_start_app_test.cpp`, outside the units seam | Adopted: the meaning in `docs/axis_contract.md` "Velocity, ramps and effort": a ramp request states acceleration and deceleration as nonnegative magnitudes, `rpm/s` is load rpm per second, the profile translates native time forms, reports the effective value and refuses what it cannot represent; the console unit grammar `rpm/s` and `<unit>/s2` (`examples/probe_cli/ProbeConsole.cpp:334`, `accelerationUnit`). Rejected: `double acceleration, deceleration` in `VelocityOperation.h:24-25` `VelocityRequest` (MCC keeps exact `Rational`), and `accelerationTime`/`decelerationTime` as common names: in `profiles/ess_rs/Position.h:36, 101, 238` and `Velocity.h:16` they are ESS words 0..2000, "not physical acceleration". RS never converts an acceleration into a time (`src/profiles/ess_rs/Velocity.cpp:112` refuses `VelocityRamp::ACCELERATION`) |
| FieldCore-node | `453db51f` (2026-11-21), clean, unchanged since prompt 58 | Configures no user units: `src/devices/rs485_motor/Rs485MotorModule.cpp:165` sends native steps, `speedRpm` and `MoveRamp::VERIFIED_CONFIGURED`; `include/TunnelMonitor/contracts/MotorControl.h:41-43` holds native ramp words. Nothing would consume prompt 60 today (inferred) |

Contrast only, never a CL86-C capability: Nanotec N5 pp63-65 (`6091` gear, `6092` feed constant, `608F`, `60A8`/`60A9`
unit codes, `6096`/`6097` factors, "acceleration unit = speed unit per second"); EPOS4 (`60A8` increments, `60A9` rpm,
`60AA` rpm/s only). The existing `UnitConfig` gear and lead scales cover `6091`/`6092`, and `AccelerationUnit` covers
`6097`.

## Baseline

- Units: `Rational`, `PositionUnit`, `TimeUnit` (with `MINUTE`), `UnitConfig`, `VelocityUnit`, `AccelerationUnit`,
  `UnitError`, `Rounding` (EXACT, NEAREST ties-to-even, TOWARD_ZERO, FLOOR, CEIL), `IntegerQuantization`, exact
  `convertVelocity` and `convertAcceleration`, `quantizeInteger`, `toNativeQuantity` (`Units.h:16-247`).
- Axis: `VelocityRequest` with rounding and a `Rational` allowance (`Axis.h:180`), `PreparedVelocityTarget` with
  `effectiveNative` and `quantization` (`:194`), `prepareVelocityTarget` (`:268`, `src/Axis.cpp:515`, motor-turn
  handling in `rateConversion`, `:103`). `convertNativeRamp` refuses time to acceleration by design
  (`src/Native.cpp:22-23`).
- Requests: `MoveRequest` (`MoveOperation.h:17`), `FiniteVelocityRequest` (`VelocityOperation.h:11`) and `HomeRequest`
  (`lichuan_c/Homing.h:84-90`) carry speeds as `VelocityRequest`; no request carries an acceleration. Native ramps:
  `MoveProfile::rampUpTimeMs`/`rampDownTimeMs` (`Position.h:203-204`), `VelocityProfile` (`Velocity.h:133-136`), homing
  `rampTimeMs` equal to the qualified value (`Homing.cpp:202`). Speed checks 1..3000 integer rpm (`Position.cpp:242`),
  velocity `rateRange` (`Velocity.cpp:114`), records `RECORD_MAXIMUM_RPM`/`RECORD_MAXIMUM_RAMP_MS` (`Records.h:77, 80`).
- Console (`examples/common/ProbeConsoleMotion.cpp`): `move` takes its speed as bare rpm (`:152-154`), `speed` rpm only
  (`:496-503`), `accel`/`decel` milliseconds only (`:435, :449-456`), `velocity` already takes rate units (`:596`),
  `home` rpm and ms (`:619`); no `/min` form other than rpm. The bench routes stay native (`BenchRoute.cpp:265-403`).
- Floating point: the core uses it only for the explicit radians approximation and BINARY32; exact paths never pass
  through it. The ramp conversion needs none.

## Ramp law evidence (stage (a) scan of the retained raw records)

Every loose `rx.txt` and every member of the three packed archives was parsed (1503 records with motion writes). For
each start from rest, a line through the cruise `6064` samples gives the time D from the start write to the line's
crossing of the start position: a linear ramp from rest predicts D = reaction + T/2; a fixed rate per 1000 rpm predicts
D = reaction + T x V / 2000. Counts are exact and each SDO sample is timed to 1 to 1.5 ms, so each run resolves D to
about 2 ms.

| Case | Result | Basis |
| --- | --- | --- |
| Position, start from rest | D = 17.1 ms + 0.5052 x T at 20 rpm and above (110 runs, rms 1.6 ms, largest error 5 ms); 144 runs on both nodes, 2 to 300 rpm, `6083` 100 to 2000 ms. In 66 runs with `6083` != `6084`, D follows `6083` (rms 1.5 ms), not `6084` (rms 204 ms). The fixed-rate law fits at rms 158 ms and is excluded | MEASURED |
| Velocity, start from rest | D = 465 ms + 0.502 x T (314 runs on both nodes, 6 to 4500 rpm, both signs, `2201` 1000 to 51200, T 200 to 2000 ms; rms 13 ms, 1 ms at 3001 rpm and above); the 0.465 s is a start latency after word 15 | MEASURED for the time law; every run had `6083` = `6084`, so which object sets it is not shown |
| Change between two nonzero speeds | Each change takes the full ramp T whatever its size: 30 to 60 and 60 to 30 rpm at T = 500 ms, 6 to 60 rpm at T = 200 ms (5 runs, 6 to 60 rpm, `6083` = `6084`, resolution about 10 ms). The guide's formula a = V x 1000 / T with V the commanded speed predicts 0.142 s for 30 to 60 rpm and is contradicted | MEASURED (5 runs); contradicts [speed_units.md](../speed_units.md) |
| Position deceleration | One HALT (trial 709): stop 17 ms + T/2 after the HALT write, T = 500 ms, `6083` = `6084`; arrival at the target only bracketed by 1 s polls | Unresolved |
| Velocity deceleration | Two HALT runs (1507, 1613; 30 and 60 rpm; T = 200 ms): 14 to 15 ms + T/2, `6083` = `6084`. Quick stop (`000B`, about 170 runs) took 5 to 7 ms at 6 to 60 rpm whatever `6084` was | Unresolved; quick stop does not follow `6084` |
| Homing (`609A`) | No retained record writes `609A` or `6099`; every home route was refused before any write | None |

Vendor figures (original PDF pages): EN86 and ZH86 p11 (printed page 9) bracket the acceleration time from zero
velocity to the target plateau; OP86 p9 (CL57-C/OL57-C header) starts from a labelled start velocity. D - T/2 is
constant within a few ms from 20 to 300 rpm, which bounds any start velocity at about 0.2 rpm, so the raw data fits
EN86/ZH86, not OP86. The rate 0.99201 x floor(rpm x 1000 / 60) counts/s is confirmed from 6 to 4500 rpm in both
modes (0.99197 to 0.99210 above 20 rpm; 2 and 12 rpm in position mode are unexplained outliers); the ramp lasts about
1.008 to 1.012 x T, which a single slower motion clock (1 / 0.99201 = 1.00806) would explain (hypothesis, worth at
most about 20 ms at 2000 ms).

The [speed units guide](../speed_units.md), the [units re-audit](2026-11-13_units_reaudit.md) and Q11
([device_conflicts.md](../device_conflicts.md)) cite four runs for this law; stage (b) records the larger set and
corrects the guide's formula for speed changes. The analysis was made in the session scratchpad; slice b1 retains it.

## Lead decisions (stage (a))

1. The ramp law is recorded as MEASURED for starts from rest only: the native ramp is the time from rest to the
   effective commanded speed. Position mode: `6083` governs it, measured 2 to 300 rpm and 100 to 2000 ms, both
   specimens, identity `00040192`/`0C`/`20`. Velocity mode: measured 6 to 4500 rpm and 200 to 2000 ms with
   `6083` = `6084`, so a velocity acceleration converts to one ramp written to both, and a separate velocity
   deceleration is refused. The library's own speed, ramp and record limits still apply after conversion; a unit
   request never widens them.
2. Refused in acceleration form until measured (native milliseconds stay available): position deceleration (`6084`),
   velocity deceleration other than equal to the acceleration, speed changes and move replacements (the measured law
   is T = |dV| / a on 5 runs; too thin for a regular conversion, recorded for the guide), homing ramps (`609A`), and
   position starts above 300 rpm (velocity-mode evidence is not position-mode evidence).
3. Conversion: T [ms] = V_eff [motor rpm] x 1000 / (60 x a [motor turns/s^2]), in exact `Rational` arithmetic with a
   checked reciprocal, where V_eff is the effective whole rpm after the speed's own quantization. The acceleration
   request carries its own `Rounding` for the millisecond result and an allowance in the request's acceleration unit;
   the result reports the exact and effective ramp, the effective acceleration in the request's unit and the error.
   Zero or negative acceleration, overflow, the approximate radians path and a ramp under 1 ms or over 2000 ms after
   rounding are refused, never clamped. The 0.992 rate shortfall and the 1% ramp stretch are reported as observations
   in the guides and never compensated.
4. Names: the common request uses RS's physical meaning, `acceleration` and `deceleration` with `AccelerationUnit`
   (not RS's ESS words `accelerationTime`/`decelerationTime`); the Lichuan native fields keep `rampUpTimeMs` and
   `rampDownTimeMs`.
5. Console: the existing grammar is kept (selected axis, explicit frame); unit forms are added: `speed VALUE UNIT/s`
   or `UNIT/min` (a bare number stays rpm), `accel VALUE UNIT/s2` or `VALUE ms`, and `move ... speed 20 mm/s accel
   100 mm/s2`; replies report the effective rpm, ramp, acceleration and errors. The prompt's
   `move ID absolute 12.5mm ...` example is read as an example, not a new grammar. `decel UNIT/s2` is refused with the
   reason. The bench routes stay native.
6. The bench exit step (one move and one velocity run in user units compared with the drive's reports) changes nothing
   persistent; it is decided in stage (d) against the owner-absence rules and prompt 66.

## Implementer brief for stage (b)

One implementer (Opus, xhigh) owns all slices; the lead verifies each slice on a short clean worktree (full verify,
package, Doxygen outside `.build`, firmware gates for console changes) and commits it.

- b1 Evidence: retain the ramp-law analysis (parser, fits, result table) under `docs/bench/20261127-p60-ramp-analysis/`
  with a README stating inputs, method and outputs; record the measured law and its limits in
  [speed_units.md](../speed_units.md), correct its speed-change formula (old claim, evidence, replacement), add a dated
  correction note to the units re-audit and update the Q11 row. Raw evidence is not edited.
- b2 Core: a protocol-neutral acceleration request (exact value, `AccelerationUnit`, frame, rounding, allowance) and
  the start-from-rest conversion of decision 3 with its result type; checked reciprocal; refusals; host tables against
  hand-computed values (for example 5 mm lead: 600 rpm at 500 mm/s^2 is 100 ms; at 300 mm/s^2, 166.67 ms rounds by the
  request's rule; 10 mm/s^2 needs 5000 ms and is refused).
- b3 Lichuan: position starts fill `6083` from an acceleration within decision 1's box; velocity starts fill both
  ramps; every case of decision 2 refused before any work; requests that map to existing native values produce
  identical native traffic (test); never widening a record or library limit (test).
- b4 Console forms of decision 5 calling the API only, JSON replies with the effective values; Python parser contracts,
  CLI contract, help rows.
- b5 Documents: [units_status.md](../units_status.md), [axis_contract.md](../axis_contract.md), position, velocity
  and homing guides, coverage, CHANGELOG, Doxygen.

Stage (c): a stronger reviewer (Fable, high) checks the arithmetic and rounding rules and independently re-fits the
ramp law from the raw records; a second reviewer checks the API and console.
