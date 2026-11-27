# Prompt 60 report: user units for speed and acceleration

State: Partial. The software scope is delivered and checked (stages (a) to (d)); the bench exit step and the
unmeasured ramp cases remain, listed under "Remaining" below. Run by the unattended Claude loop of 2026-11-23
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
| Position, start from rest | (Stage (c) re-run, both directions: see below.) D = 17.1 ms + 0.5052 x T at 20 rpm and above (110 runs, rms 1.6 ms, largest error 5 ms); 144 runs on both nodes, 2 to 300 rpm, `6083` 100 to 2000 ms. In 66 runs with `6083` != `6084`, D follows `6083` (rms 1.5 ms), not `6084` (rms 204 ms). The fixed-rate law fits at rms 158 ms and is excluded | MEASURED |
| Velocity, start from rest | D = 465.6 ms + 0.5015 x T at 20 rpm and above (229 runs, rms 7.3 ms); 469.1 ms + 0.4993 x T over all 314 runs on both nodes (6 to 4500 rpm, both signs, `2201` 1000 to 51200, T 200 to 2000 ms; rms 22 ms); the 0.47 s is a start latency after word 15 | MEASURED for the time law; every run had `6083` = `6084`, so which object sets it is not shown |
| Change between two nonzero speeds | Each change takes the full ramp T whatever its size: 30 to 60 and 60 to 30 rpm at T = 500 ms, 6 to 60 rpm at T = 200 ms (7 runs, 6 to 60 rpm, `6083` = `6084`, reaction 16.5 to 18.8 ms, resolution about 10 ms). The guide's formula a = V x 1000 / T with V the commanded speed predicts 0.142 s for 30 to 60 rpm and is contradicted | MEASURED (7 runs); contradicts [speed_units.md](../speed_units.md) |
| Position deceleration | One HALT (trial 709): stop 17 ms + T/2 after the HALT write, T = 500 ms, `6083` = `6084`; arrival at the target only bracketed by 1 s polls | Unresolved |
| Velocity deceleration | Two HALT runs (1507, 1613; 30 and 60 rpm; T = 200 ms): 14 to 15 ms + T/2, `6083` = `6084`. Quick stop (`000B`, about 170 runs) took 5 to 7 ms at 6 to 60 rpm whatever `6084` was | Unresolved; quick stop does not follow `6084` |
| Homing (`609A`) | No retained record writes `609A` or `6099`; every home route was refused before any write | None |

Vendor figures (original PDF pages): EN86 and ZH86 p11 (printed page 9) bracket the acceleration time from zero
velocity to the target plateau; OP86 p9 (CL57-C/OL57-C header) starts from a labelled start velocity. D - T/2 is
constant within a few ms from 20 to 300 rpm, which bounds any start velocity at about 0.2 rpm, so the raw data fits
EN86/ZH86, not OP86. The rate 0.99201 x floor(rpm x 1000 / 60) counts/s (median) is confirmed from 6 to 4500 rpm in both
modes (0.99161 to 0.99237 at `2201=1000`, narrower at higher subdivisions; 2 and 12 rpm in position mode are
unexplained outliers); the ramp lasts about
1.008 to 1.012 x T, which a single slower motion clock (1 / 0.99201 = 1.00806) would explain (hypothesis, worth at
most about 20 ms at 2000 ms).

The [speed units guide](../speed_units.md), the [units re-audit](2026-11-13_units_reaudit.md) and Q11
([device_conflicts.md](../device_conflicts.md)) cite four runs for this law; stage (b) records the larger set and
corrects the guide's formula for speed changes. Slice b1 retained the analysis in
[docs/bench/20261127-p60-ramp-analysis](../bench/20261127-p60-ramp-analysis/README.md); its re-run, after the sign
handling of S0018 and S0023 was fixed, gives the figures in this table (7 speed-change runs, not the first scan's 5;
the velocity fit as stated here, where the first scan gave 465 ms + 0.502 x T at rms 13 ms).

## Lead decisions (stage (a))

1. The ramp law is recorded as MEASURED for starts from rest only: the native ramp is the time from rest to the
   effective commanded speed. Position mode: `6083` governs it, measured 2 to 300 rpm and 100 to 2000 ms, both
   specimens, identity `00040192`/`0C`/`20`. Velocity mode: measured 6 to 4500 rpm and 200 to 2000 ms with
   `6083` = `6084`, so a velocity acceleration converts to one ramp written to both, and a separate velocity
   deceleration is refused. The library's own speed, ramp and record limits still apply after conversion; a unit
   request never widens them.
2. Refused in acceleration form until measured (native milliseconds stay available): position deceleration (`6084`),
   velocity deceleration other than equal to the acceleration, speed changes and move replacements (the measured law
   is T = |dV| / a on 7 runs at 6 to 60 rpm; too thin for a regular conversion, recorded for the guide), homing ramps (`609A`), and
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

## Stage (b) commits and checks (2026-11-27)

| Slice | Commit | Content | Checks |
| --- | --- | --- | --- |
| b1 | `e34c7e0d` | Ramp-law analysis retained under [docs/bench/20261127-p60-ramp-analysis](../bench/20261127-p60-ramp-analysis/README.md) (scripts, per-run table, summary; outputs reproduce byte for byte); the speed guide records the start-from-rest law and corrects the speed-change formula; re-audit correction note; Q11 row | `check_promptset.py --check-git-ref INDEX` and `HEAD` (only the held maxon artifacts), `check_repository.py`, `git diff --check` |
| b2 | `5ba893de` | `AccelerationRequest`, `PreparedRampTime`, `prepareRampTime` (`Axis.h`): T = 1000 x \|speed\| / a, exact with checked 128-bit products, the request's rounding, an allowance on the acceleration error, the caller's millisecond range (refused outside, never clamped); `rateConversion` frame rules shared with velocities; `ramp_time_test` | Full verify on a clean worktree 180 of 180, package, ledger, strict Doxygen; CI 45956602049 succeeded |
| b3 | `a593882d` | `LICHUAN_C::prepareAccelerationRamp` (`Position.h`): move starts fill `6083` only (2 to 300 rpm, 100 to 2000 ms), velocity starts fill both ramps with one value (6 to 4500 rpm, 200 to 2000 ms); decelerations, velocity changes, move replacements and homing ramps refused in acceleration form before any work; native validation and record checks unchanged; `acceleration_ramp_test` | Full verify 181 of 181, package, ledger, strict Doxygen; CI 45959858841 succeeded |
| b4 | `19430b30` | Console: speeds in `UNIT/s` and `UNIT/min` for move, speed and home (bare number stays rpm), accelerations in `rpm/s`, `turns/s2` or `UNIT/s2` for move and velocity through `prepareAccelerationRamp` only, `ramp_value` reply record; refusals carry the library's reason; bench routes, replacements and changes stay native; about 330 bytes of RAM per console (host estimate) | Full verify 181 of 181, package, ledger, `s2_bench_test.py`, strict Doxygen; the implementer's `check_firmware.py --target all` and `check_stack.py --strict` |
| b5 | `98d12665` | Units status, axis contract, position, velocity and homing guides, coverage, CHANGELOG; the ledger appends the measured law to `6083`/`6084` and the 7-run change result to Q26 (requirement text kept), generated reference regenerated; prompt 73 points at the measured law | Strict Doxygen (39 headers), `check_promptset.py --check-git-ref HEAD`, `check_repository.py`, `check_lichuan_ledger.py`, `generate_lichuan_objects.py --check`, `check_lichuan_operations.py` |

Lead decision during b3: the acceleration form admits only each mode's measured ramp range (moves 100 to 2000 ms,
velocity 200 to 2000 ms), not the native 1 to 2000 ms; shorter ramps stay available in native milliseconds.

## Stage (c) review (2026-11-27)

Two new reviewer agents, read-only, on `1d1e2b6d` (MotorControl-RS `403eed0`, clean; the units seam unchanged since
stage (a)):

- Arithmetic, rounding and an independent re-fit (Fable, high): no critical or high finding. Thirteen hand-computed
  cases (rounding ties, allowance edges, range edges 99/100/2000/2001 ms, gear and lead, deg/s^2, reversed polarity,
  radians refused) and every expected value in the three test files match; a fuzz of about 180k core and 120k box cases
  against an exact-fraction model found no mismatch. Its own fit of the raw captures (1273 loose captures and 536
  archives, read in memory) agrees with all three claims: position starts D = 17.02 ms + 0.5039 x T over 337 runs at
  30 to 300 rpm in both directions (rms 0.9 ms; `6084` coefficient 0.0000), velocity starts D = 461.8 ms + 0.5035 x T
  (83 runs, rms 1.1 ms), and the 7 speed changes take the full T (plus seven position-mode re-triggered changes not
  in the retained set). Findings: medium, the box was the bounding rectangle of measured points, not the measured
  set (4500 rpm completed once; position ramps above 1000 ms rest on one run, trial 52020, counted twice); medium,
  moves too short to reach their speed are unmeasured; low, refusal precedence (storage overflow before LIMIT), no
  binding of `effectiveMs` to a later speed, missing edge-tie tests. The retained `ramp.py` compared the sign of
  `606C` with unsigned `6081` and so dropped every reverse position start.
- API, integration, console and documents (Opus, xhigh): no critical or high finding; the boxes, refusals before any
  frame and identical native traffic confirmed. Findings: medium, `docs/speed_units.md` still said nothing converts;
  medium, the acceleration-form ramp was not tied to the operation it was made for (a caller could place it in a
  replacement, a homing request or with another speed); medium, a change or replacement after an acceleration-form
  start kept the start's milliseconds and so silently changed the acceleration (for example 120 to 3000 rpm: about 24
  times the request); low, console paths relying only on the library's refusal, test gaps, CLI contract drifts.

Lead dispositions (all fixed in `05564001` unless stated):

- Boxes narrowed: moves 2 to 300 rpm with ramps 100 to 1000 ms; velocity 6 to 3000 rpm with ramps 200 to 2000 ms (4500
  rpm completed once on node 2, and FF05 occurred at 4000 rpm with 200 ms ramps on 2026-11-13). Doxygen and the guides
  call each the bounding box of measured points and list its unmeasured corners, short moves included.
- Checked placement: `MoveProfile::rampSource` and `VelocityProfile::rampSource` (`RampSource`, `Settings.h`) carry the
  use, binding, effective native speed and milliseconds of an acceleration-form ramp; the preparations refuse a ramp
  made for another use, binding, speed or value (`RAMP_SOURCE`), and a velocity change or move replacement on a run
  whose ramps came from the acceleration form (`RAMP_CHANGE`), before any work. Native-millisecond runs are unchanged.
- Console: the homing, replacement and deceleration acceleration paths refuse in the console itself; the missing tests
  and the CLI contract wording are added.
- Analysis re-run with both directions and without the duplicate: position 348 runs (143 forward, 205 reverse; three
  reverse runs at 600 rpm lie outside the box), D = 17.25 ms + 0.5042 x T over 278 runs (rms 1.4 ms), 191 runs with
  unequal ramps following `6083` (rms 1.4 ms against 172 ms for `6084`), ramp duration 1.0083 x T; velocity unchanged.
- `docs/speed_units.md` rewritten for the implemented conversion; the Doxygen of `prepareRampTime` states the refusal
  precedence as implemented.
- Backlog, not fixed: refusing a move shorter than its ramps' distance in acceleration form (documented as inference
  instead).

## Stage (d) acceptance (2026-11-27)

The final code is `05564001`; `e8857479` and later commits change only this report, the backlog and `PROGRESS.md`.
Checks:

- On a clean worktree of `05564001`: `verify.py --mode full` (configure, C++11 gcc build, registration, 181 of 181
  tests, package), `test/package_test.py`, `check_promptset.py --check-git-ref HEAD`, `check_lichuan_operations.py`,
  `check_lichuan_ledger.py`, `generate_lichuan_objects.py --check`, `s2_bench_test.py`; strict Doxygen in a worktree
  outside `.build` (39 headers, 4916 members). The implementer ran `check_firmware.py --target all` and
  `check_stack.py --strict` on the fix round.
- CI `verify.yml` run 45981612252 succeeded on `e8857479` in every job (Windows quick, full gcc and clang with ASan
  and UBSan, ESP-IDF firmware with the strict stack check and the isolated component, Arduino). Earlier: 45956602049
  (`5ba893de`), 45959858841 (`a593882d`), 45964998121 failed on `98d12665` in the package step (packaged documents
  linked unpackaged guides), fixed by `1d1e2b6d` with CI 45967128160.

Acceptance against "Verify and exit":

| Item | Evidence |
| --- | --- |
| Conversion tables against hand-computed values | `ramp_time_test` and `acceleration_ramp_test` (frames, gear and lead, rpm/s, turns/s^2, mm/s^2, deg/s^2, every rounding rule, ties); the stage (c) reviewer's 13 independent cases and a 300k-case fuzz against an exact-fraction model |
| Boundary refusals: ramp 0, ramp over 2000 ms, speed over the record limit, unrepresentable EXACT | `ramp_time_test` (zero and negative acceleration, 0 and 2001 ms after rounding, EXACT with a fractional time, overflow, radians); `acceleration_ramp_test` (each box edge, 99/1001/199/2001 ms, speeds outside the boxes, refused uses, `recordsNeverWidened`: a bound record's speed and ramp limits still refuse after conversion; `RAMP_SOURCE`, `RAMP_CHANGE`) |
| Reported quantization errors | `PreparedRampTime::quantization` and `accelerationError` checked in both test files; the console's `ramp_value` record (`motion_console_test`) |
| Identical native traffic for requests that map to existing native values | `motion_console_test` `accelerationFormsGiveTheNativeTraffic` (the unit and millisecond forms send the same frames) and `accelerationFormRefusalsBeforeTraffic` (no frame on any refusal) |
| Native, package, firmware, stack and documentation gates; CI on the final commit | Above |
| Bench: one move and one velocity run in user units compared with the drive's reports | NOT RUN (Remaining) |
| Units, axis, position and velocity guides, CLI contract, coverage, CHANGELOG, Doxygen | b5 and the stage (c) round; [speed_units.md](../speed_units.md) rewritten in stage (c) |
| A stronger reviewer checks the arithmetic and rounding rules | Stage (c), Fable high, with an independent re-fit of the ramp law |

## Remaining

- Bench exit step, NOT RUN in this host-only prompt: one move (for example 60 rpm from rest, `accel` in mm/s^2 or
  rpm/s giving a ramp inside 100 to 1000 ms) and one velocity run (for example 120 rpm, ramp inside 200 to 2000 ms)
  commanded in user units, comparing the drive's `6083`/`6084` readback, `606C` and the `6064` trajectory with the
  reported effective values. Nothing persistent or communication-changing, so COMMON's owner-absence rules allow it
  unattended; prompt 66 (hardware qualification) owns it.
- Unmeasured, kept in native milliseconds until measured: position deceleration (`6084`) and arrival, velocity
  deceleration other than equal to the acceleration, speed changes (measured on 7 runs only, 6 to 60 rpm), homing
  ramps (`609A`, no evidence). The measurement list is in "Ramp law evidence" above; prompt 66 or a later velocity
  prompt runs it.
- Backlog S23: moves too short to reach their speed.
- RAM of the console additions (about 330 bytes per console) is a host estimate; prompt 76 measures storage placement.
