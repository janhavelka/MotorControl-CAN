# Speed, ramp and position units

This guide is for application developers who command a Lichuan CL86-C (or a related 86C/57C CANopen drive) through
this library. It explains what the drive's native units are, why whole rpm and millisecond ramps limit what you can
ask for, what the drive was measured to deliver, and how the library's host unit layer handles it today.

Every statement carries its evidence:

- **documented** with the manual page (EN86, ZH86, ZH57 are the 86C English, 86C Chinese and 57C Chinese manuals in
  [reference/vendor](reference/vendor/); pages are PDF pages) or the EDS;
- **measured** with a report link: drive reports from two CL86-C specimens on one firmware, subdivision `2201=1000`,
  nothing qualified;
- **inferred**: arithmetic or reasoning, not a drive fact.

The page-by-page audit behind this guide is the [units re-audit](reports/2026-11-13_units_reaudit.md).

## Short answers

| Question | Answer |
| --- | --- |
| Speed unit | Whole motor rpm, 0..3000, for position, velocity and homing speeds (documented, EN86 p11, p13) |
| Finer speed | None. No object takes 0.1 rpm, rps or pulses/s (documented: no such object or unit on any page or in the EDS) |
| Ramp unit | A time in milliseconds, 0..2000, not an acceleration (documented, EN86 p11, p13) |
| Ramp meaning | Time from rest to the commanded speed (documented as a figure only, EN86 p11; measured fits on four runs) |
| Position unit | Command pulses, ±1,000,000; 1000 pulses per motor turn at `2201=1000` (documented range EN86 p11; scale measured) |
| Unit conversion in the drive | None. The EDS has no CiA402 factor group (`6091`, `6092`, `608F`, `6096`, `6097`, `60A8`, `60A9`) (documented, EDS) |
| Delivered speed | 0.992 × the command, after truncation to whole pulses per second (measured fit, cause unknown) |
| Negative velocity | Refused today; prompt 59 measures it |

## Native units

| Object | What it is | Native unit | Type and range | Evidence |
| --- | --- | --- | --- | --- |
| `607A` | Position target | Command pulses | I32, -1,000,000..+1,000,000 | documented, EN86 p11 (the figure labels the target "number of pulses") |
| `607C` | Home offset | Not stated; pulses inferred from the shared range | I32, -1,000,000..+1,000,000 | documented range, EN86 p13 |
| `6064` | Actual position | Command pulses: moves end exactly at their `607A` target | I32, read only | measured, [2026-11-05](reports/2026-11-05_motion_and_stress.md) |
| `6081` | Position-mode speed | Whole rpm | U32, 0..3000 | documented, EN86 p11 |
| `60FF` | Velocity-mode target | Whole rpm | U32 0..3000 in the mode table, I32 in the dictionary and EDS | documented, EN86 p13, p17; conflict Q03 |
| `6099:01`, `6099:02` | Homing search and creep speeds | Whole rpm | U32, 0..3000 | documented, EN86 p13 (which calls `:02` "creep time (rpm)") |
| `606C` | Actual speed | Signed rpm, read in 2 rpm steps at cruise | I32, read only | measured, [re-audit](reports/2026-11-13_units_reaudit.md) section 3 |
| `6083`, `6084` | Acceleration and deceleration time | Milliseconds | U32, 0..2000 | documented, EN86 p11, p13 |
| `609A` | Homing acceleration and deceleration time (one value) | Milliseconds | U32, 0..2000 | documented, EN86 p13 |
| `2201` | Subdivision: command pulses per motor turn | Pulses per turn (inferred; 1000 measured) | U16, 0..65535 (p11) against 400..51200 (p18) | documented, EN86 p11, p18; conflict Q04 |
| `2501` | Encoder resolution | Not stated; the specimens read 4000 with a 1000-line encoder | U16 | documented, EN86 p19 and p3 |

The drive has no maximum-speed, polarity or quick-stop deceleration object (`6080`, `607F`, `607E`, `6085` are absent
from the EDS). All conversion from millimetres, degrees, load turns or accelerations happens on the host.

## The rpm problem

### Whole rpm only

The drive takes speed as an integer number of motor revolutions per minute. One rpm is the smallest step, and it is a
large relative step at low speed: at 10 rpm one step is 10 %, at 100 rpm 1 %. No other object offers a finer speed
(documented). Speed is commanded per motor turn, whatever the mechanics behind the motor; the drive knows nothing about
gears or screws.

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

Alternating 2 and 3 rpm to fake 2.5 rpm is not offered and not industry practice. Changing the subdivision `2201`
does not refine the speed either: the speed objects are rpm, not pulses (documented), and whether rpm stays motor rpm
at another subdivision is not measured.

### What the drive actually delivers: the 0.992 observation

On the bench the actual position `6064` rose more slowly than the command (measured,
[re-audit](reports/2026-11-13_units_reaudit.md) section 3, night soak of 2026-11-11: 1,026 cruise intervals at
20 speeds from 6 to 300 rpm, position and velocity modes, both specimens). The data fit, within 0.0002 at every speed:

**`6064` rate = floor(rpm × 1000 / 60) × 0.992 counts/s** (at `2201=1000`)

So two things happen, both measured and neither explained:

1. The pulse rate is truncated to whole pulses per second. At `2201=1000` that only matters when the rpm is not a
   multiple of 3 (10 rpm is 166.67 pulses/s, delivered as 166).
2. Everything is then 0.8 % slow.

| Command (5 mm lead) | Nominal | Delivered (model) | Difference |
| --- | --- | --- | --- |
| 12 rpm (1 mm/s) | 200 pulses/s | 198.4 counts/s = 0.992 mm/s | -0.8 % |
| 10 rpm (0.833 mm/s) | 166.67 pulses/s | 164.7 counts/s = 0.823 mm/s | -1.2 % |
| 300 rpm (25 mm/s) | 5000 pulses/s | 4960 counts/s = 24.8 mm/s | -0.8 % |

What this means for an application:

- Positions are not affected: moves end exactly at their target count (measured). A move's cruise phase takes about
  0.8 % longer than computed; a 10 s cruise lasts about 10.08 s (inferred). Do not size deadlines without margin.
- A velocity run covers about 0.8 % less distance than rpm × time predicts (inferred).
- At `2201=1000`, choose rpm that are multiples of 3 when the extra truncation matters (inferred from the measured
  model).
- The drive's clock is not the cause as far as can be seen: its heartbeat and PDO timers run within 0.04 % of the host
  clock (measured). A separate motion time base, or a difference between `2201` and the pulses the drive really uses
  per shaft turn, are not excluded. Counting shaft turns over a +100,000 pulse move would separate the two.
- `606C` is too coarse to see this: at cruise it reads in 2 rpm steps (296, 298 or 300 at 300 rpm; 5 or 7 at 6 rpm).
  Judge speed from the `6064` slope (measured).

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
one firmware, velocity mode): a `2201` write takes effect at the next enable without a power cycle, and `6081`/`60FF`
stay motor rpm at every subdivision while `6064` counts in the new pulses. Runs were clean at command pulse rates up
to 2 MHz (2344 rpm at `2201=51200`), so there is no 200 kHz or 500 kHz limit on this drive. The limit is the motor's
speed: FF05 from 4250 rpm on node 1 and from 4625 rpm on node 2 at `2201=1000` with 2000 ms ramps; 4000 rpm with a
200 ms ramp faults on both. Only at `2201=51200` did it fall, to between 2344 and 3516 rpm (a pulse-rate ceiling or
the deviation threshold `250A`, which in pulses is about 28° there; not established).

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
labelled "start velocity" above zero, for which no object exists. Four bench runs fit "time from rest to the commanded
speed" and exclude a fixed reference such as "time per 1000 rpm" (measured, [re-audit](reports/2026-11-13_units_reaudit.md)
section 2: trials 713 and 709 in position mode, round 8's two HALT runs in velocity mode).

Under that reading (inferred, and still open as [Q11](device_conflicts.md)):

a [turns/s²] = (rpm / 60) / (ms / 1000), or a [rpm/s] = rpm × 1000 / ms

where rpm is the commanded speed of that move. The same ramp time gives a higher acceleration at a higher speed: a
200 ms ramp is 25 turns/s² to 300 rpm and 250 turns/s² to 3000 rpm.

### What can be reached

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

Zero ramp times have no documented meaning, and the library refuses them. Nothing documents a ramp between two
nonzero speeds, the velocity- and homing-mode ramps (no figure), the shape of the curve, or whether the 0.8 % shortfall
also applies to the ramp slope. A HALT took about 20 to 30 ms before the deceleration began (measured, inferred from
stop distances).

## Positions

`607A` is in command pulses (documented). At `2201=1000`, 1000 pulses are one motor turn (measured on node 1 as a
quarter turn for 250 pulses), so the ±1,000,000 range is ±1000 motor turns; at `2201=51200` it would be about
±19.5 turns (inferred). With the 5 mm screw one pulse is 0.005 mm at `2201=1000`. The library converts steps, full
steps, encoder counts, turns, degrees, radians and millimetres to pulses exactly, with a stated rounding
([units and status](units_status.md), [axis contract](axis_contract.md)). Positions do not suffer the speed
truncation: a move ends exactly at its pulse target (measured).

## Sign of velocity

`60FF` is "0 ~ 3000, UNSIGNED32" in the velocity-mode tables but I32 in the dictionaries and the EDS (documented,
EN86 p13 and p17; conflict Q03). No page mentions a negative speed. `606C` already reports negative rpm when the motor
turns backwards (measured, -60 during a -60 rpm position move), and a positive `60FF` turns the shaft
counter-clockwise seen from the shaft end with `2200=0` (measured, [round 7](reports/2026-11-10_bench_r7.md)). Today
the library refuses a negative velocity unless a record states `reverseQualified`, which no record does
([velocity](velocity.md)). Position moves take a positive speed; the direction comes from the target. Prompt 59
measures a negative `60FF` and, if it runs in reverse at the commanded magnitude, opens signed velocity.

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
4. The Lichuan move, velocity and homing operations refuse anything that is not integer motor rpm in 1..3000
   (`checkSpeed` in `src/profiles/lichuan_c/Position.cpp`, the velocity and homing equivalents in `Velocity.cpp` and
   `Homing.cpp`), and velocity adds the sign and record gates.

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
acceleration to a ramp time: by rule no millisecond ramp is turned into an acceleration while the ramp law is
unmeasured ([units and status](units_status.md)).

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
refused, never clamped, and the acceleration conversion stays refused until prompt 59 records the ramp law.

Neither prompt will hide the 0.992 or the truncation: the library keeps reporting what it sent, and the measured
behavior is documented beside it.
