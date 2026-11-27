# Speed, ramp and position units

This guide describes the Lichuan C profile's documented native units, host conversion and retained CL86-C
observations. Related 86C/57C models need their own applicability and qualification evidence. It explains whole-rpm
and millisecond-ramp limits without treating drive-reported counts as independently measured shaft motion.

Every statement carries its evidence:

- **documented** with the manual page (EN86, ZH86, ZH57 are the 86C English, 86C Chinese and 57C Chinese manuals in
  [reference/vendor](reference/vendor/); pages are PDF pages) or the EDS;
- **measured** with a report link: drive reports from two CL86-C specimens on one firmware, with the subdivision
  stated for each campaign; physical observations are identified separately, and nothing is qualified;
- **inferred**: arithmetic or reasoning, not a drive fact.

The page-by-page audit behind this guide is the [units re-audit](reports/2026-11-13_units_reaudit.md).

## Short answers

| Question | Answer |
| --- | --- |
| Speed unit | Whole rpm, 0..3000, for position, velocity and homing speeds (documented, EN86 p11, p13); the motor-shaft basis is the profile's inferred interpretation, not an explicit scale formula in those pages |
| Finer speed | No finer-speed object or unit is documented in the retained manuals/EDS; the implemented profile uses whole rpm |
| Ramp unit | A time in milliseconds, 0..2000, not an acceleration (documented, EN86 p11, p13) |
| Ramp meaning | Time from rest to the commanded speed: documented as a figure only (EN86 p11) and measured for starts from rest (144 position, 314 velocity runs); a change between two speeds also takes the whole ramp time (7 runs); deceleration and homing ramps remain unresolved |
| Position unit | Command pulses, ±1,000,000 (documented, EN86 p11); a visual quarter-turn for 250 pulses on one specimen at `2201=1000` supports an approximate scale, not a qualified precise counts/turn relation |
| Unit conversion in the drive | No CiA402 factor objects at `6091`, `6092`, `608F`, `6096`, `6097`, `60A8` or `60A9` appear in the EDS; no configurable native conversion is established |
| Reported position rate | `6064` rate fits 0.992 × the nominal whole-pulse rate at `2201=1000`; physical shaft speed and cause remain unmeasured |
| Negative velocity | Ordinary admission needs `reverseQualified`, which no retained record states; the separate attended reverse probe has recorded negative feedback without qualifying physical direction |

## Native units

| Object | What it is | Native unit | Type and range | Evidence |
| --- | --- | --- | --- | --- |
| `607A` | Position target | Command pulses | I32, -1,000,000..+1,000,000 | documented, EN86 p11 (the figure labels the target "number of pulses") |
| `607C` | Home offset | Not stated; pulses inferred from the shared range | I32, -1,000,000..+1,000,000 | documented range, EN86 p13 |
| `6064` | Actual position | Native observed counts; matching `607A` endpoints support a command-coordinate relation on the measured fixture | I32, read only | measured, [2026-11-05](reports/2026-11-05_motion_and_stress.md); physical scale remains separate |
| `6081` | Position-mode speed | Whole rpm | U32, 0..3000 | documented, EN86 p11 |
| `60FF` | Velocity-mode target | Whole rpm | U32 0..3000 in the mode table, I32 in the dictionary and EDS | documented, EN86 p13, p17; conflict Q03 |
| `6099:01`, `6099:02` | Homing search and creep speeds | Whole rpm | U32, 0..3000 | documented, EN86 p13 (which calls `:02` "creep time (rpm)") |
| `606C` | Actual speed | Signed rpm, read in 2 rpm steps at cruise | I32, read only | measured, [re-audit](reports/2026-11-13_units_reaudit.md) section 3 |
| `6083`, `6084` | Acceleration and deceleration time | Milliseconds | U32, 0..2000 | documented, EN86 p11, p13 |
| `609A` | Homing acceleration and deceleration time (one value) | Milliseconds | U32, 0..2000 | documented, EN86 p13 |
| `2201` | Subdivision | Pulses per turn (inferred; approximate visual scale at 1000) | U16, 0..65535 (p11) against 400..51200 (p18) | documented range, EN86 p11, p18; conflict Q04 |
| `2501` | Encoder resolution | Not stated; the specimens read 4000 with a 1000-line encoder | U16 | documented, EN86 p19 and p3 |

The EDS has no maximum-speed, polarity or quick-stop deceleration object at `6080`, `607F`, `607E` or `6085`.
This absence establishes no undocumented firmware capability. Implemented unit conversions happen on the host;
no acceleration-to-ramp conversion is implemented yet (see "Ramps" below).

## The rpm problem

### Whole rpm only

The profile interprets the integer rpm objects as motor revolutions per minute. One rpm is its smallest command step,
which is a large relative step at low speed: at 10 rpm one step is 10 %, at 100 rpm 1 %. No finer speed or gearing/feed
conversion is documented in the reviewed objects. Applications supply the motor/load relation explicitly.

### Worked example: 5 mm lead screw, direct drive

One motor turn moves the nut 5 mm, so 1 rpm = 5 mm/min = 1/12 mm/s ≈ 0.0833 mm/s. Only whole multiples of
0.0833 mm/s can be sent (inferred arithmetic).

| Requested | Exact motor speed | With `Rounding::EXACT` | With `Rounding::NEAREST`, allowance 1/2 rpm | Speed sent |
| --- | --- | --- | --- | --- |
| 1 mm/s | 12 rpm | 12 rpm | 12 rpm, error 0 | 1 mm/s |
| 1.5 mm/s | 18 rpm | 18 rpm | 18 rpm, error 0 | 1.5 mm/s |
| 1.3 mm/s | 15.6 rpm | refused | 16 rpm, error +2/5 rpm | 1.333 mm/s (+2.6 %) |
| 0.1 mm/s | 1.2 rpm | refused | 1 rpm, error -1/5 rpm | 0.0833 mm/s (-16.7 %) |
| 0.03 mm/s | 0.36 rpm | refused | 0 rpm, error -9/25 rpm, then refused by the profile (below 1 rpm) | none |
| 250 mm/s | 3000 rpm | 3000 rpm | 3000 rpm | 250 mm/s, the documented top |
| 260 mm/s | 3120 rpm | 3120 rpm, then refused by the profile (above 3000) | the same | none |

The common layer (`prepareVelocityTarget`) produced every rpm value and error in this table, and the 10:1 values
below, in a scratch program compiled against `src/Axis.cpp` and `src/Units.cpp`
([re-audit](reports/2026-11-13_units_reaudit.md) section 5). The profile's 1..3000 rpm check is `checkSpeed` in
`src/profiles/lichuan_c/Position.cpp`.

### Gearing is the exact way out

Put a reduction between motor and load and each motor rpm becomes a smaller load speed. With a 10:1 gearbox in front
of the same 5 mm screw, 1 motor rpm = 0.00833 mm/s, so 0.1 mm/s is exactly 12 motor rpm and 1.3 mm/s exactly 156 rpm.
The price is top speed: 3000 motor rpm is then 25 mm/s. A finer lead works the same way (a 2 mm lead gives
0.0333 mm/s per rpm). In general the load speed step is lead / (60 × gear ratio) mm/s (inferred arithmetic).

Alternating 2 and 3 rpm to approximate 2.5 rpm is not offered. Changing `2201` does not add fractional bits to the
integer rpm objects. The November 13 grid below measured how reported count rates changed with subdivision;
it did not independently measure shaft rpm at each setting.

### Reported count rate: the 0.992 observation

On the bench the actual position `6064` rose more slowly than the command (measured,
[re-audit](reports/2026-11-13_units_reaudit.md) section 3, night soak of 2026-11-11: 1,026 cruise intervals at
20 speeds from 6 to 300 rpm, position and velocity modes, both specimens). The data fit, within 0.0002 at every speed:

**`6064` rate = floor(rpm × 1000 / 60) × 0.992 counts/s** (at `2201=1000`)

The 2026-11-27 [ramp analysis](bench/20261127-p60-ramp-analysis/README.md) finds the same factor in its cruise fits:
median 0.99201 of floor(rpm × `2201` / 60) from 20 to 4500 rpm in both modes, and 0.99197 to 0.99210 in the 41 runs
at subdivisions 4000 to 51200 (the subdivision inferred from the rate; measured observation, cause unknown).

The empirical fit has two parts; neither identifies an internal firmware mechanism:

1. Rounding the nominal pulse rate down to whole pulses per second fits the observations. At `2201=1000`,
   10 rpm gives 166.67 nominal pulses/s and a fitted integer factor of 166.
2. Multiplying that factor by 0.992 fits the observed `6064` rate on these specimens.

The mechanical rates below are inferred from the illustrative 5 mm lead and assumed count/shaft relation;
they are not independent travel measurements.

| Command (5 mm lead) | Nominal | Inferred from count-rate model | Difference |
| --- | --- | --- | --- |
| 12 rpm (1 mm/s) | 200 pulses/s | 198.4 counts/s = 0.992 mm/s | -0.8 % |
| 10 rpm (0.833 mm/s) | 166.67 pulses/s | 164.7 counts/s = 0.823 mm/s | -1.2 % |
| 300 rpm (25 mm/s) | 5000 pulses/s | 4960 counts/s = 24.8 mm/s | -0.8 % |

What this means for an application:

- The retained moves' final reported counts matched their targets. This does not prove physical positioning
  accuracy. The count-rate model predicts about 0.8 % more cruise time for the same count displacement;
  a nominal 10 s cruise becomes about 10.08 s (inferred). It is not a deadline guarantee.
- A velocity run accumulates about 0.8 % fewer reported counts than nominal rpm × scale × time predicts
  at whole nominal pulse rates. Physical distance additionally requires an independently established scale.
- At `2201=1000`, choose rpm that are multiples of 3 when the extra truncation matters (inferred from the measured
  model).
- The drive's clock is not the cause as far as can be seen: its heartbeat and PDO timers run within 0.04 % of the host
  clock (measured). A separate motion time base, or a difference between `2201` and the pulses the drive really uses
  per shaft turn, are not excluded. Counting shaft turns over a +100,000 pulse move would separate the two.
- `606C` is too coarse to see this: at cruise it reads in 2 rpm steps (296, 298 or 300 at 300 rpm; 5 or 7 at 6 rpm).
  The `6064` slope resolves reported count rate more finely; it still does not independently measure shaft speed.

The library reports the speed it sent, not a corrected one, and never compensates silently: the cause is unknown and
may differ on another firmware.

### Pulse rate and pulse limits

No retained Lichuan document states a pulse or step frequency limit for this drive (documented absence: all five
manuals, the EDS and the product page were searched). The CL86-C has no pulse input; the 200 kHz or 500 kHz limits
typical of pulse-input drives describe their input stage and are not in any retained source for this model. The
nominal internal pulse rate is rpm × `2201` / 60: 50,000 pulses/s at 3000 rpm with `2201=1000`, but 2.56 million at
`2201=51200` (inferred arithmetic). Whether the drive has an internal ceiling is what prompt 59's subdivision grid
measures. The only documented speed-related failure is the position out-of-tolerance alarm (`603F=FF05`), whose listed
causes include "Too fast" (documented, EN86 p20).

**Measured on 2026-11-13** ([bench report](reports/2026-11-13_velocity_limits_and_fault_recovery.md), two specimens,
one firmware, velocity mode): after a `2201` write and the next enable, the reported `6064` rates scaled with
the selected subdivision without a power cycle. This supports the profile's rpm interpretation but does not
measure physical shaft speed. Runs met the report's clean-feedback criteria at nominal command pulse rates above
2 MHz (2344 rpm at `2201=51200`); these are calculated rates, not measured internal pulses. FF05 occurred from
4250 rpm on node 1 and from 4625 rpm on node 2 at `2201=1000` with 2000 ms ramps; 4000 rpm with a
200 ms ramp faults on both. Only at `2201=51200` did it fall: node 2 was clean to 3000 rpm (2.56 MHz) and faulted
at 3200 rpm (2.73 MHz); node 1's wider interval was between 2344 and 3516 rpm. A pulse-rate ceiling and an effect of
the deviation threshold remain competing hypotheses, not established causes. On 2026-11-16, 3000 rpm with 500 ms
ramps (four times the acceleration of the clean 2000 ms run) was also clean on node 1 at 51200
([HIL report](reports/2026-11-16_hil_campaign.md#s21-discriminator-and-2500-persistence)), which favours the pulse-rate ceiling; one run. After an
FF05 at 51200 both drives faulted at every later enable until a power cycle
([enable-fault report](reports/2026-11-13_enable_fault_state.md)). These trials establish no safe pulse-rate ceiling
or causal motor-speed limit. In one unsaved-setting observation, node 2, written to 51200 at 16:23Z, read 1000 after
the power cycle at 17:38Z; that single observation is not a general persistence guarantee.

## The ramp-time problem

### A time, not an acceleration

`6083`, `6084` and `609A` are milliseconds, 0..2000 (documented, EN86 p11, p13). The dictionary on EN86 p17 calls them
"Profile acceleration" and "Profile deceleration", the CiA402 names, but the unit is the time in the mode tables; the
EDS calls them "accel time" and "decel time". Other CiA402 drives use the same object numbers for real accelerations
(maxon EPOS4 in rpm/s), so a value cannot be copied between brands ([drive contrasts](reference/drive_contrasts.md)).

### What the time means

The position-mode motion curve on EN86, ZH86 and ZH57 p11 brackets the "acceleration time" from rest to the moment the
curve reaches the target velocity, and the "deceleration time" from the target velocity back to rest (documented as a
figure; no sentence or formula states it). The operating manuals' version of the figure (OP86/OP57 p9) starts from a
labelled "start velocity" above zero, for which no object exists.

**Measured** (2026-11-27, [ramp analysis](bench/20261127-p60-ramp-analysis/README.md) of every retained record that
commands motion): drive reports (`6064` counts, `606C`) from the two CL86-C specimens, nodes 1 and 2, one firmware
identity (`1000=00040192`, `1009=0C`, `100A=20`). Each run gives one intercept: the time D from the start write to the
moment the fitted cruise line crosses the start position. A linear ramp from rest lasting T after a reaction L gives
D = L + T/2. Nothing here is qualified, and the results hold only inside the stated box.

| Case | Box | Result |
| --- | --- | --- |
| Position start from rest | 2 to 300 rpm, `6083` 100 to 2000 ms, 144 runs | D = 17 ms + 0.505 × `6083` after the bit 4 write (110 runs at 20 rpm and above, rms 1.6 ms): the ramp is the time from rest to the commanded speed. In 66 runs with `6083` ≠ `6084`, `6083` sets it (rms 1.5 ms; `6084` fits at 204 ms). A fixed rate per 1000 rpm fits at 158 ms and is excluded. The fitted start velocity is -0.2 ± 0.03 rpm, so the data fit the EN86/ZH86 figure from rest, not OP86's start velocity |
| Velocity start from rest | 6 to 4500 rpm, both signs, 200 to 2000 ms, `6083` = `6084` in every run, 314 runs | D = 466 ms + 0.502 × T after word `000F` (229 runs at 20 rpm and above, rms 7 ms; rms 0.6 ms at 3001 rpm and above): the same ramp from rest, after a start latency of about 0.465 s. Which of `6083` and `6084` sets it is not shown |
| Change between two speeds | Velocity mode, 30 to 60 and 60 to 30 rpm at 500 ms, 6 to 60 rpm at 200 ms, `6083` = `6084`, 7 runs | Each change takes the whole ramp time, whatever its size, after a 16 to 19 ms reaction |
| HALT | Trial 709 (position, 300 rpm, `6084` = 500 ms); trials 1507 and 1613 (velocity, 30 and 60 rpm, 200 ms); `6083` = `6084` | The stop distances fit a ramp-down lasting `6084` after 14 to 18 ms. Three runs only: unresolved |
| Quick stop (`000B`) | 170 runs, 6 to 60 rpm, `6084` 200, 500 and 1000 ms | The stop distance equals 4 to 7 ms of cruise travel (quartiles) whatever `6084` is: the quick stop does not follow `6084` |

Two observations go with the law and are neither modelled nor compensated by the library. The reported cruise rate is
0.99201 × floor(rpm × `2201` / 60) counts/s (median over the fitted runs, 20 to 4500 rpm, both modes, subdivisions
1000 to 51200 inferred from the rate). The ramp lasts about 0.3 to 1 % longer than T (twice the fitted slope: 1.0105
in position mode, 1.003 to 1.008 in velocity mode); a single slower motion clock (1 / 0.99201 = 1.008) would explain
both, which is only a hypothesis. At 2000 ms the stretch is at most about 20 ms.

### Acceleration of a start from rest

From rest to the commanded speed in the ramp time (measured, inside the box above):

a [turns/s²] = (rpm / 60) / (ms / 1000), or a [rpm/s] = rpm × 1000 / ms

where rpm is the commanded whole-rpm speed and ms the ramp time (`6083` in position mode; one value written to both
`6083` and `6084` in velocity mode). The same ramp time gives a higher acceleration at
a higher speed: a 200 ms ramp is 25 turns/s² to 300 rpm and 250 turns/s² to 3000 rpm (inferred arithmetic). Position
starts above 300 rpm are not measured; velocity-mode evidence is not position-mode evidence.

**Correction (2026-11-27).** Old claim (this guide and the [units re-audit](reports/2026-11-13_units_reaudit.md)
section 2, 2026-11-13): a = rpm × 1000 / ms with rpm "the commanded speed of that move". Applied to a change from 30
to 60 rpm with a 500 ms ramp, that predicts a change lasting 250 ms. Evidence: seven velocity-mode speed
changes on both specimens, same firmware ([ramp analysis](bench/20261127-p60-ramp-analysis/README.md), "Results"):
every change took the whole ramp time; the crossing of the two cruise lines fits a 16 to 19 ms reaction plus T/2 in
each run (resolution 8 to 15 ms), where the old formula would need a 144 ms reaction for 30 to 60 rpm. Replacement: a
change between two nonzero speeds lasts T, so a = |V2 - V1| / T; measured on 7 runs at 6 to 60 rpm, velocity mode,
`6083` = `6084`. Position-mode changes, other speeds and unequal ramps stay unmeasured. The formula above applies to
starts from rest only.

### What can be reached

From rest (inferred arithmetic from the formula above; rows and ramps outside the measured box are arithmetic only):

| Speed | Longest ramp (2000 ms) | Shortest ramp (1 ms) |
| --- | --- | --- |
| 6 rpm | 0.05 turns/s² | 100 turns/s² |
| 60 rpm | 0.5 turns/s² | 1,000 turns/s² |
| 600 rpm | 5 turns/s² | 10,000 turns/s² |
| 3000 rpm | 25 turns/s² | 50,000 turns/s² |

(inferred arithmetic). A gentle ramp to a high speed is impossible: 3000 rpm at 1 turn/s² would need a 50 s ramp.

### Worked example: 5 mm lead, 600 rpm (50 mm/s)

| Requested acceleration | Exact ramp | Whole milliseconds (nearest) | Effective acceleration |
| --- | --- | --- | --- |
| 500 mm/s² | 100 ms | 100 ms | 500 mm/s² |
| 300 mm/s² | 166.67 ms | 167 ms | 299.4 mm/s² |
| 30,000 mm/s² | 1.667 ms | 2 ms | 25,000 mm/s² (-17 %) |
| 10 mm/s² | 5000 ms | above 2000 ms: refuse | none |

(inferred arithmetic). The ramp quantum is 1 ms, so short ramps are coarse. At 50 mm/s the reachable range is
25 mm/s² to 50,000 mm/s².

### What stays open

Zero ramp times have no documented meaning, and the library refuses them. No document describes a ramp between two
nonzero speeds, the velocity- and homing-mode ramps (no figure) or the shape of the curve. Still unmeasured or
unresolved ([Q11](device_conflicts.md)): deceleration (three HALT runs fit a ramp-down lasting `6084`, and position
arrivals are bracketed only by polls about 1 s apart), which object sets the velocity-mode ramp, speed changes in
position mode or outside 6 to 60 rpm, position starts above 300 rpm, the homing ramp `609A` (no retained record writes
it), the curve between the samples, and the cause of the rate shortfall and the ramp stretch.

## Positions

`607A` is in command pulses (documented). At `2201=1000`, node 1's visually observed quarter turn for 250 pulses
supports approximately 1000 pulses/turn; it cannot resolve the 0.8 % scale question. Under that assumed relation,
the ±1,000,000 range is ±1000 motor turns; at `2201=51200` it would be about ±19.5 turns (inferred).
With the illustrative 5 mm screw one pulse would be 0.005 mm at `2201=1000`. The library converts steps, full
steps, encoder counts, turns, degrees, radians and millimetres to pulses exactly, with a stated rounding
([units and status](units_status.md), [axis contract](axis_contract.md)). The retained moves ended at their reported
pulse targets; physical positioning accuracy remains independently unqualified.

## Sign of velocity

`60FF` is "0 ~ 3000, UNSIGNED32" in the velocity-mode tables but I32 in the dictionaries and the EDS (documented,
EN86 p13 and p17; conflict Q03). No page mentions a negative speed. `606C` already reports negative rpm when the motor
turns backwards (measured, -60 during a -60 rpm position move), and a positive `60FF` turns the shaft
counter-clockwise seen from the shaft end with `2200=0` (measured, [round 7](reports/2026-11-10_bench_r7.md)). Today
the library refuses a negative velocity unless a record states `reverseQualified`, which no record does
([velocity](velocity.md)). Position moves take a positive speed; the direction comes from the target. The
[November 13 reverse trials](reports/2026-11-13_velocity_limits_and_fault_recovery.md) recorded negative `606C`
feedback, but shaft direction was not observed and `reverseQualified` remains false. The attended experiment's
reverse probe (`VelocityEvidence::reverseProbe`, bench `--reverse`) supplies the separate experimental path: the
rate goes out as the two's complement I32 (-6 is `FA FF FF FF`), and a `606C` against the command by more than
2 rpm ends the run with its stop; the probe qualifies nothing ([velocity](velocity.md)).

## How the library handles units today

### Speeds

Speeds go through the common host layer, then the profile checks the native object.

1. Describe the mechanics once in `AxisConfig::units` (`include/MotorControlCAN/Units.h`): `motorTurnsPerLoadTurn`
   (gear), `millimetresPerLoadTurn` (lead), `commandStepsPerMotorTurn`, encoder scale and `commandPolarity`. Set
   `nativeRateBasis = NativeRateBasis::MOTOR_TURNS` and `nativeVelocityTime = TimeUnit::MINUTE`: that pair means rpm
   (`include/MotorControlCAN/Axis.h`).
2. Build a `VelocityRequest` with an exact `Rational` value, a `VelocityUnit`, a `CoordinateFrame`, a `Rounding` and
   `maximumQuantizationError` in the destination unit, here rpm.
3. `prepareVelocityTarget` converts exactly and quantizes once. `PreparedVelocityTarget::effectiveNative` is the
   integer rpm that will be sent and `quantization` the exact error (effective minus requested).
4. Ordinary Lichuan move, velocity and homing operations require integer motor-rpm magnitudes in 1..3000
   (`checkSpeed` in `src/profiles/lichuan_c/Position.cpp`, the velocity and homing equivalents in `Velocity.cpp` and
   `Homing.cpp`), and velocity adds the sign and record gates. The separate attended velocity range probe can
   raise its experimental cap to 6000 rpm; the reverse probe permits explicitly reviewed negative experiments.
   Neither expands qualified support or changes ordinary admission.

```cpp
using namespace MotorControlCAN;
AxisConfig axis;                       // binding filled from the selected drive
axis.units.motorTurnsPerLoadTurn = UnitScale(1, 1, ScaleSource::ASSUMED);
axis.units.millimetresPerLoadTurn = UnitScale(5, 1, ScaleSource::ASSUMED);   // 5 mm lead
axis.nativeRateBasis = NativeRateBasis::MOTOR_TURNS;
axis.nativeVelocityTime = TimeUnit::MINUTE;                                   // rpm

VelocityRequest speed;
speed.value = Rational(13, 10);                                               // 1.3
speed.unit = VelocityUnit(PositionUnit::MILLIMETRES, TimeUnit::SECOND);      // mm/s
speed.frame = CoordinateFrame::LOAD;
speed.configurationGeneration = axis.binding.configurationGeneration;
speed.rounding = Rounding::NEAREST;
speed.maximumQuantizationError = Rational(1, 2);                              // rpm
PreparedVelocityTarget prepared;
Status s = prepareVelocityTarget(speed, axis, prepared);
// s.isOk(); prepared.effectiveNative == 16 (rpm); error +2/5 rpm.
// With Rounding::EXACT the call fails: detail 109 (FRACTIONAL_TARGET).
```

The rounding choices are `EXACT` (refuse any fraction, the default), `NEAREST` (ties to even), `TOWARD_ZERO`, `FLOOR`
and `CEIL`. The allowance is always checked, so a request is never rounded further than the caller allowed.

### Ramps

Ramps are native milliseconds only: `MoveProfile::rampUpTimeMs`/`rampDownTimeMs`
(`include/MotorControlCAN/profiles/lichuan_c/Position.h`), `VelocityProfile` (`Velocity.h`) and the homing ramp, each
1..2000, zero refused. `AccelerationUnit` and `convertAcceleration` exist in `Units.h`, but nothing converts an
acceleration to a ramp time yet ([units and status](units_status.md)). The start-from-rest law above is now measured;
prompt 60 stage (b) is adding its conversion, with the unmeasured cases refused (planned, not implemented).

### Console

The example console mirrors the API ([CLI contract](cli_contract.md)):

- `velocity VALUE RATE FRAME DURATION_MS configured normal|fast [round POLICY ERROR]` takes user rate units (`rpm`,
  `turns/s`, `mm/s`, `deg/s` and the other `UNIT/s` forms) and a rounding policy, for example
  `velocity 1.3 mm/s load 5000 configured fast round nearest 1/2`.
- `speed VALUE [rpm]` for the simple moves, and the speed of `move ... RPM configured`, take exact motor rpm only.
- `accel VALUE [ms]` and `decel VALUE [ms]` take milliseconds only.
- `home METHOD SEARCH_RPM RETURN_RPM RAMP_MS zero` takes rpm and milliseconds.

## What prompts 59 and 60 add

[Prompt 59](prompts/59_velocity_range_direction.md) measures the drive: a full-range ladder in both modes up to
3000 rpm and an attended probe above it, a pulse-rate grid at `2201` = 1000, 4000, 10000, 25600 and 51200 (so whether
rpm stays motor rpm and where any pulse ceiling lies), the ramp law (time to speed and stop distance at several rpm and
ramp pairs), the delivered-to-commanded speed ratio at every step (so the 0.992 at high speed and at other
subdivisions), and negative `60FF`. Its results update this guide.

[Prompt 60](prompts/60_user_units.md) adds the missing host conversions, in the style of the CiA402 factor group:
acceleration and deceleration in user units per second squared (mm/s², turns/s², deg/s²) converted per request to
whole milliseconds using the integer rpm actually sent, with the rounding stated and the effective acceleration
reported; speeds in user units for position, velocity and homing; and console forms such as
`move ID absolute 12.5mm speed 20mm/s accel 100mm/s2` that call the API only. Ramps under 1 ms or over 2000 ms are
refused, never clamped. Prompt 59 did not record the ramp law; prompt 60 measured it from the retained records
([ramp analysis](bench/20261127-p60-ramp-analysis/README.md)); its planned conversion covers only the measured box
(starts from rest), and the [prompt 60 report](reports/prompt_60.md) lists the cases that stay refused.

Neither prompt will hide the 0.992 or the truncation: the library keeps reporting what it sent, and the measured
behavior is documented beside it.
