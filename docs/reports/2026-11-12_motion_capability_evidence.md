# Motion capability evidence: speed range, reverse, units, mode, enable and release, firmware lock, records (2026-11-12)

Read-only research on `main` at `2c5d832`. Nothing was built, flashed or sent. This file is the only file written.
The working tree also held another session's untracked prompt drafts (`62`, `64`, `65`, `67`) and a modified `53`;
they were read, not changed. MotorControl-RS was checked for the velocity sign seam only: HEAD `5938f1d`, dirty files
are its bench scripts and their tests, not this seam.

It answers seven user requests, verbatim in each section, and maps them to the prompt set (section h).

## How to read the evidence

- **Reopened**: I rendered the original PDF page to an image this session and read it.
- **Text**: I extracted the text of the original PDF page this session (PyMuPDF), without reading the image.
- **EDS**: I read the section of `docs/reference/vendor/LC_StepDriverCAN_V1.1.eds` directly.
- **Ledger**: taken from `docs/reference/lichuan_objects.json` (the reviewed ledger), not rechecked.
- **Measured**: retained bench records and reports. All are drive reports from two CL86-C specimens (nodes 1 and 2) of
  one firmware identity: `1000=00040192`, `1009=0C`, `100A=20`, `1018:01..03=1000/1/0`, `2000=0400`, `2001=0200`.
  The shaft was observed only where stated. Nothing is qualified.
- **Inferred**: my arithmetic or reasoning. Never a device fact.

Reopened pages: EN86 PDF p11, p12, p13, p17, p18; ZH86 PDF p13, p17. Text pages: EN86 p3, p16, p19, p20; ZH86 p11,
p18, p20; ZH57 p13, p17; OP86 p11; OP57 p11; the CL86-C product page snapshot. Page numbers are one-based PDF pages.

## Summary

| Request | Short answer |
| --- | --- |
| a) Full speed range | The only documented limit is 0..3000 rpm (rpm objects) and 0..2000 ms (ramps). No pulse frequency limit exists in any retained source. "Too fast" is a documented cause of the position out-of-tolerance alarm. Bench maximum so far: 300 rpm in position mode, 60 rpm in velocity mode. The drive stored `60FF=3001` without complaint. Position moves up to 3000 rpm can run today; velocity needs a new experiment cap. |
| b) Negative speed | The EDS and both 86C dictionary tables say `60FF` is I32. The mode tables say U32 0..3000. `606C` already reports signed rpm. Treat `60FF` as signed rpm after one guarded measurement. Run the above-limit probe of (a) first: it bounds what an unsigned reading of a negative value could do. |
| c) User units | The host unit layer already does what the CiA402 factor group does (gear, lead, scale, polarity, exact rounding). The EDS has no factor group objects. Missing: acceleration in units/s² converted to the drive's millisecond ramp, and unit words for `speed`/`accel` in the console. 2.5 rpm is not physically possible on this drive's 1 rpm objects except through gearing. |
| d) Typed mode switch | Yes, one library function: `prepareModeSelect`, then the existing `nextAction`/`advanceAction`. Released or power-up drive only. Read back `6061`; never write `6061`; never read `6060`. About 130 measured mode writes exist (trial 917 and others), none on an enabled drive in modes 1, 3 or 6. |
| e) Enable and release | 111 enables succeeded on two drives, 2 early ones failed. Word 6 (`0006`) released the drive in every power-up enable and freed node 1's shaft by hand. A `MEASURED` enable gate and a `prepareRelease` that writes `0006` are the smallest change. |
| f) Firmware lock | Enforced in three places by an exact 8-value match. Replace it with a comparison: product fields must match, firmware fields differing is reported as "unqualified firmware" and carried to the terminal result. |
| g) Records from the console | Today only firmware code supplies records (`set...Prerequisites`); the bench has text "arming" lines. Add a pure record codec in the profile and `record load/show/use/save` console routes, volatile by default, NVS only on an explicit save. |
| h) Roadmap | Prompt 30 owns (e), blocked on its gates. Prompt 35 owns parts of (a), (b) and (d). Prompts 9 and 10 own the unit layer of (c), but not acceleration. Nothing owns the above-limit tests, a standalone mode select, (f) or (g). Prompt 52's text conflicts with (b). |

## a) Velocity runs in full range

User: "Velocity runs - do the tests in full range (mind about the drives pulse limit - try all of it and above and
record whats happening)".

### Documented facts

| Object | Claim | Source |
| --- | --- | --- |
| `6081` profile speed | "Target velocity (rpm)", 0 ~ 3000, UNSIGNED32, RW | EN86 p11 (reopened); EDS `[6081]` U32 RW default 60 |
| `60FF` velocity target | "Target velocity (rpm)", 0 ~ 3000, UNSIGNED32, RW | EN86 p13, ZH86 p13 (reopened); ZH57 p13, OP86 p11, OP57 p11 (text) |
| `60FF` in the dictionary | "Target Velocity", I32, RW | EN86 p17, ZH86 p17 (reopened); ZH57 p17 (text); EDS `[60FF]` I32 (0x0004) default 120 |
| `6099:01/:02` homing speeds | 0 ~ 3000 rpm, U32 | EN86 p13 (text) |
| `6083`/`6084` ramps | "Acceleration/Deceleration time (ms)", 0 ~ 2000, U32 | EN86 p11, p13 (reopened); EDS U32 default 100 |
| `2201` subdivision | 0 ~ 65535 (p11) against 400 ~ 51200 (p18) | EN86 p11, p18 (reopened); ZH86 p18 (text); EDS default 0 |
| `2501` encoder resolution | No unit, no range | EN86 p19 (text); EN86 p3: CL86-C has a 1000-line incremental encoder (text) |
| `2500` operation mode | 0 noneffective, 1 open loop, 2 closed loop | EN86 p19 (text) |
| Over-speed | "Position out-of-tolerance alarm", 5 flashes every 5 s, causes include "5. Too fast", resettable | EN86 p20, ZH86 p20 (text: 速度过快) |
| Alarm code | `603F` FF05 "too high position deviation" | EN86 p16 (text) |
| Deviation threshold | `250A` 超差报警阈值, "factory default, no adjustment needed"; both specimens read 4000 (trials 309, 310) | ZH86 p20 (text); [2026-11-04](2026-11-04_bench.md) |

Not present anywhere:

- No maximum-speed object. The EDS object list has no `6080` (max motor speed), `607F` (max profile velocity), `607E`
  (polarity), `6085` or `605A` (EDS read directly). `2500`..`2520` are loop and tuning words (ledger).
- No pulse or step frequency limit. I searched the text of all five PDFs and the product page for kHz, Hz, 频率, 最高,
  转速 and r/min. The only hits are the vibration rating "10~60 Hz" and the rpm columns. The CL86-C is a bus drive;
  the only pulse wording is `2200`'s labels "Pulse Sign / Pulse /Sign" (EN86 p18, reopened).

Inferred arithmetic (not a drive fact): command pulses per second = rpm × `2201` / 60. With the measured
`2201=1000`, 3000 rpm is 50,000 pulses/s. At `2201=51200` it would be 2,560,000 pulses/s. If the drive has an internal
step-rate ceiling, it would show only at a high subdivision. If `2501=4000` counts quadrature edges (unestablished),
the encoder sees 200,000 counts/s at 3000 rpm.

### Conflicts

- `2201` range: 0..65535 against 400..51200 (C12, Q04). No typed write exists, so the subdivision cannot be varied.
- `60FF` type: I32 against U32 (C11, Q03); see (b).
- Supply range: three different claims (Q19). The top speed of a stepper depends on supply voltage, and the bench
  supply is not recorded.

### Current caps in code

| Cap | Where |
| --- | --- |
| Position speed 1..3000 integer rpm | `src/profiles/lichuan_c/Position.cpp:14` (`MAXIMUM_POSITION_SPEED_RPM`), `:248-273` (`checkSpeed`); `src/profiles/lichuan_c/Settings.cpp:219-220` |
| Ramps 1..2000 ms, zero refused | `include/MotorControlCAN/profiles/lichuan_c/Position.h:199-200`; `src/profiles/lichuan_c/Velocity.cpp:208-211` |
| Velocity `MEASURED` range 1..3000 rpm | `src/profiles/lichuan_c/Velocity.cpp:172-178` |
| Velocity experiment 60 rpm and 20 s | `include/MotorControlCAN/profiles/lichuan_c/Velocity.h:31`, `:34`; `Velocity.cpp:179-184` |
| Homing speeds 1..3000 rpm | `src/profiles/lichuan_c/Homing.cpp:136-147` |
| Bench route rpm 1..3000; `60FF` probe -3000..3001 | `examples/s2_bench/BenchRoute.cpp:173`, `:179` |
| Runner: velocity 60 rpm, 20000 ms | `scripts/s2_bench.py:17-21`, `:219-222` |

So a position-mode ladder up to 3000 rpm needs no code change: the bench `position` and `move` routes take 1..3000
rpm under the measured move record. A velocity ladder above 60 rpm needs a new experiment cap.

### Measured on the bench

- Position mode: highest speed 300 rpm. Trial 709 read `606C=296` during a 300 rpm move; the night soak ran 33 moves at
  300 rpm and a ladder of 6..300 rpm ([2026-11-05](2026-11-05_motion_and_stress.md), [night soak](2026-11-12_night_soak.md)).
- Velocity mode: 6, 30 and 60 rpm only (rounds 7 to 9; 163 night-soak runs at 6..60 rpm).
- `6064` rose 0.992 times the commanded rate at 6, 30 and 60 rpm (99.2, 496.0, 992.0 counts/s at 1000 counts per
  turn). The cause is not established ([round 8](2026-11-10_bench_r8.md)).
- `60FF=3001` was acknowledged and read back as 3001 (trial 2001, [round 9](2026-11-10_bench_r9.md)). The drive does
  not range-check `60FF` at the documented bound. Nothing above 3000 has run.
- No alarm, EMCY or `603F` code appeared in any motion record.
- Ramp meaning (inferred from mode 1 records): in trial 713 (200 rpm, 2000 ms ramp) the quick stop came at `606C=101`,
  `6064=854`. A linear 0-to-200 rpm ramp over 2000 ms gives 101 rpm at 1.01 s and 850 counts. In trial 709/710 a 300 rpm
  HALT with `6084=500` took 1392 counts; a linear 500 ms ramp from 5000 counts/s gives 1250, plus about 30 ms before the
  drive reacted. Both fit "ramp time is the time from zero to the target speed". In mode 3 the HALT looked like about
  230 ms for a 200 ms ramp (round 8, inferred). No manual states it (Q11).

### Recommendation and test plan

Run the ladder in two parts: the documented range in both modes, then an above-limit probe in velocity mode only. The
probe must run before any negative-speed test (b), because it shows what the drive does with a target far above 3000.

**Before the first run**

1. Record the supply voltage and current limit from the power supply, and the motor nameplate (LC60H2127; ratings not
   recorded). The reachable top speed depends on them.
2. Remove the paper mask from the shaft, or tape it flat. At 3000 rpm the shaft turns 50 times a second.
3. One node at a time, node 1 first. Cutoff in hand. Motors stay bolted, shafts unloaded.
4. Record `2200`, `2201`, `2500`, `2501`, `250A` once (all readable today). Keep `2201=1000`.
5. Plan the position counter. Velocity runs are positive only, so `6064` grows (3000 rpm for 5 s is 250,000 counts).
   Position moves need targets within ±1,000,000. Bring the counter back with negative position moves between velocity
   steps, or use the typed position clear (`2302`, ran once on node 2).

**Part 1, documented range**

| Mode | Steps (rpm) | Ramps | Length | Stop |
| --- | --- | --- | --- | --- |
| Position (`6081`) | 300, 600, 1200, 1800, 2400, 3000 | 1000/1000 ms below 1200 rpm, 2000/2000 ms from 1200 | At least 1 s at speed: distance = rpm/60 × (up/2 + down/2 + 1 s) × 1000 counts, for example 150,000 counts at 3000 rpm with 2000 ms ramps. Alternate direction so `6064` returns. | Completion, plus one HALT at 1800 and one quick stop at 3000 |
| Velocity (`60FF`) | 60, 120, 240, 480, 720, 960, 1200, 1500, 1800, 2100, 2400, 2700, 3000 | As above | Ramp + 3 s at speed + setup, within 20 s (the night soak's plan minimum was 11.47 s) | The run's own HALT at due, then the measured halted-drive quick stop back to a released standstill |

Also run 1 rpm once in each mode (the lowest nonzero value). Start a HALT run only from a release without the running
latch: a HALT run from the latch left node 2 enabled and halted with no accepted stop (S20).

**Part 2, above the documented limit** (velocity mode, only after 3000 passed cleanly): 3001, 3300, 3600, 4000, 4500,
5000, 6000 rpm, ramps 2000/2000 ms, 3 s at speed. Stop climbing at the first of: a plateau (two steps in a row where
`606C` and the `6064` slope stay at or below the 3000 rpm values, meaning the drive clamps), an alarm, a stall, or
abnormal noise. 6000 rpm (twice the documented value) is the hard ceiling, as project policy. Then a write and
readback only (no motion) of `6081=3001` and `6081=4000`.

Optional last point, only if Part 2 showed a clamp: one 2 s run at `60FF=100000` with a 2000 ms ramp. It is the closest
safe stand-in for an unsigned reading of a small negative value (b). Expected: the same clamp, or an FF05 alarm.

**Record for every step**

- Commanded value, its readback (`60FF` or `6081` upload) and the ramps.
- Every cycle: `6041`, `2100`, `6061`, `6064`, `606C`. After each run, single explicit reads of `1001` and `603F`.
  Passive EMCY frames, TEC/REC, service gaps.
- Derived: speed at cruise from the `6064` slope (counts/s × 60 / 1000) and from `606C` (mean, min, max); ratio to the
  command; time to reach 95 % of speed (the ramp law, Q11 and F10); HALT stop distance against `6084`; quick-stop coast.
- Attendant: noise, vibration bands (resonance), heat or smell, and the alarm LED flash count. An optical tachometer is
  the independent speed observation if one is available.

**Stop rules**

1. Any alarm: `2100` bit 3, the layout's alarm bit, `603F` or `1001` nonzero, or an EMCY. Stop the run, end the ladder,
   and power-cycle before more motion (over/undervoltage and overcurrent need a restart; FF05 is "resettable", which is
   the first chance to run the typed alarm clear on a natural alarm, backlog H03).
2. `606C` sign opposite to the command, or `|606C|` above 1.2 × the command: quick stop at once.
3. In the documented range, cruise speed below 90 % of the command for two cycles: treat it as the limit, stop, and do
   not go higher.
4. Attendant: abnormal noise, vibration, heat, smell or a loose part: cutoff.
5. Lost reply, uncertain write or console loss: no retry; best-effort stop; end.
6. `6064` past ±900,000: bring it back before the next step.
7. Each step starts only from a released or halted standstill shown by two fresh reads.
8. No above-limit run without the attendant.

### Prompt steps

1. Library: add a range-probe option to the velocity experiment scope (for example `rangeProbe` with
   `VELOCITY_RANGE_PROBE_MAXIMUM_RPM = 6000`, project policy), admitted only with ramps of at least a stated minimum and
   only under the attended experiment. `MEASURED` stays 1..3000 until a reviewed record says otherwise. Add an
   over-speed check to the running cycle: `|606C|` above the command plus a stated tolerance ends the run with its stop
   requested (today only the sign is checked, `Velocity.cpp:512-520`).
2. Host tests: 60/61, 3000/3001 and 6000/6001 boundaries under each gate; a range-probe run never reads as qualified;
   the over-speed check; ramp minimum.
3. Bench: raise the runner and route caps for the probe scope only; add a `6081` write/readback probe next to
   `velocity-target`; a campaign script that applies the stop rules above and writes one summary row per step.
4. Bench trials: Part 1 in both modes on node 1, then node 2; Part 2 on node 1 only.
5. Records: update the ledger value policy of `60FF` and `6081` with the measured range and clamp behavior, the
   device conflict table (Q11 ramp law), coverage and backlog.
6. Later, only if `2201` gets a reviewed write (Q04): repeat Part 1 at a high subdivision to look for a step-rate
   ceiling.

## b) Negative speed in velocity mode

User: "No negative speed - i see that for velocity mode - this should probably be allowed - check what the datasheet
says and what it allows and how it should be treated properly - if unclear - test it".

### Documented facts

- `60FF` is I32 in the EDS (`[60FF]` DataType 0x0004, default 120) and in the dictionary tables: EN86 p17 and ZH86
  p17 (reopened), ZH57 p17 (text).
- `60FF` is "0 ~ 3000, UNSIGNED32" in the velocity mode tables: EN86 p13 and ZH86 p13 (reopened), ZH57 p13, OP86 p11
  and OP57 p11 (text). The same tables list `6081` the same way (EN86 p11, reopened).
- No page mentions a negative speed, a direction bit or reverse operation for velocity mode. No `607E` polarity object
  exists (EDS).
- `2200` "Default direction 0: Pulse Sign; 1: Pulse /Sign" (EN86 p18 reopened; ZH86 p18 text, 默认方向). No page says
  what it does under CAN. Both specimens read 0.
- The drive does have directions: homing methods 17/18/24/29 run "in the negative direction" and "positive direction"
  (EN86 p13-14, ZH86 p13-14, text), and `607A` is I32 ±1,000,000 (EN86 p11, reopened).
- `606C` "actual speed" is I32 RO (EDS; EN86/ZH86 p17).
- Velocity start order: ZH86 p13 (reopened) and OP86 p11 (text) set mode 3, observe `6061=3`, write `60FF`, `6083`,
  `6084`, then 6, 7, 15 "about 10 ms" apart: "motor enabled, motor starts running". EN86 p13 (reopened) enables first,
  then writes the parameters, then refers to bits 4-6.

### Conflicts

- Q03/C11: I32 against U32 0..3000. The C17 start-order conflict is now settled by measurement for positive rates
  only: the ZH86 order starts the motor (rounds 7 to 12).

### Current code

- `VelocityEvidence::reverseQualified` and `maximumReverseRpm`:
  `include/MotorControlCAN/profiles/lichuan_c/Velocity.h:67-69`.
- The lower bound is 0 unless `reverseQualified`: `src/profiles/lichuan_c/Velocity.cpp:119-147` (line 123).
- The experiment refuses `reverseQualified`: `Velocity.cpp:179-182`. So no negative run can be prepared today.
- The written value would be two's complement (`uint32_t(int32_t(effectiveNative))`): `Velocity.cpp:300-303`.
- Running cycles already require `606C` to have the command's sign once running was seen: `Velocity.cpp:512-520`.
  Before that, a wrong-sign sample does not end the run.
- Bench probe route: `velocity-target` writes one `60FF` value in -3000..3001 without enabling
  (`examples/s2_bench/BenchRoute.cpp:179`, `scripts/s2_bench.py:82-87`).

### Measured on the bench

- `606C` reports signed rpm: -60 during a -60 rpm position move (trial 706), -30 in trials 2106 and 2227.
- `60FF=3001` was stored (trial 2001). `60FF=-6` was deliberately not sent: an unsigned reading is 4,294,967,290 rpm,
  and the drive does not range-check (round 9).
- A positive `60FF` turns the shaft counter-clockwise, seen facing the shaft end, with `2200=0` (round 7, attendant).
  A positive position target does too (node 1, 2026-11-03).

### Treatment

CiA 402 defines `60FF` as a signed 32-bit target whose sign is the direction. Both the EDS and the dictionary tables of
the 86C manuals say I32. The mode tables' "0 ~ 3000 UNSIGNED32" most likely gives the magnitude, copied from `6081`
(inference). RS takes the same path for its Modbus drive: a signed range -3000..3000 with a separate
`negativeTwosComplementVerified` fact, opened only after a bench check
(`MotorControl-RS/src/profiles/ess_rs/Velocity.cpp:113-132`, `include/MotorControlRS/profiles/ess_rs/Velocity.h:14-17`).

So: treat `60FF` as signed I32 rpm with magnitude at most 3000, after one measurement shows that a negative value runs
in reverse at the commanded magnitude. Keep zero refused (undocumented). Keep F15: no sign change or zero during a run.
The host `commandPolarity` (`include/MotorControlCAN/Units.h:78`) stays the way to flip the user's positive direction.
F15 is about changes during a run; a reverse start is gated by Q03 only. Prompt 52's sentence "Negative velocity
stays refused (Q03 and the user's instruction)" conflicts with this new request and needs updating.

### Measurement that settles it

1. First (a) Part 2. It shows whether the drive clamps a target above 3000. That bounds the worst case of an unsigned
   reading.
2. Write only: `60FF=-6` to a released mode 3 standstill (`velocity-target` route). Expect the readback `FA FF FF FF`
   and no motion. This checks the encoding only; readback cannot show the sign.
3. One attended run at -6 rpm, ramps 2000/2000 ms (a long ramp limits the speed reached early if the drive misreads
   it), about 12 s, cutoff in hand. Rule: from the first cycle after word 15, `606C` must read -8..-4 once running and
   `6064` must fall. Any positive `606C`, or `|606C|` above 10, ends the run with a quick stop at once. The attendant
   notes the direction (expected clockwise facing the shaft end).
4. If it passes: -30 and -60 rpm on both nodes, HALT and quick stop in reverse, then the (a) ladder in reverse.
5. If the drive reads the value as unsigned (runs positive, races, or alarms): record it and keep reverse refused.
   The other direction mechanisms are position moves (signed `607A`) or `2200` (no typed write, activation unknown,
   Q04), which is not a run-time reverse.

### Prompt steps

1. Library: a reverse-probe option in the attended experiment scope, magnitude at most 60 rpm (or the range-probe cap
   after (a)), ramps at least a stated minimum. While it is set, any wrong-sign or over-speed sample ends the run with
   its stop requested, also before running was seen.
2. Host tests: -1, -60, -61; the wire bytes `FA FF FF FF` for -6; wrong-sign and over-speed samples before and after
   running was seen; the probe never reads as qualified; `MEASURED` with `reverseQualified` admits the reviewed
   magnitude only.
3. Bench: steps 2 to 4 above; then a `MEASURED` velocity record with `reverseQualified=true` and the measured
   `maximumReverseRpm`.
4. Docs: Q03 row, ledger `60FF` value policy, `docs/velocity.md`, backlog F15 wording, prompt 52 text.

## c) Units beyond native

User: "Native units only - extend this (explore the possibilities so its physically achievable, industry standard -
simplest functional)".

### Documented native units

| Object | Native unit | Source |
| --- | --- | --- |
| `607A` target, `607C` home offset | I32 command pulses, ±1,000,000 | EN86 p11, p13 (p11 reopened; the p11 figure labels the target "number of pulses") |
| `6081`, `60FF`, `6099:01/:02` | rpm, 0..3000, whole numbers only (U32/I32 integers) | EN86 p11, p13 (reopened) |
| `6083`, `6084`, `609A` | Ramp time in ms, 0..2000 | EN86 p11, p13 (reopened) |
| `6064` actual position | I32; measured: command pulses, 1000 per turn at `2201=1000` | EDS; Q04 row of `docs/device_conflicts.md` |
| `606C` actual speed | I32; measured: signed rpm, lagging and averaging in ramps | EDS; 2026-11-05 report |
| `2201` | pulses per motor turn; range conflict | EN86 p11, p18 (reopened) |

The EDS has none of the CiA402 factor group: no `6091` gear ratio, `6092` feed constant, `608F` position encoder
resolution, `6096` velocity factor, `6097` acceleration factor, `60A8`/`60A9` SI units (EDS object list read
directly). The drive cannot convert units. Any user unit must be host-side.

Contrast (design evidence only, `docs/reference/drive_contrasts.md`): maxon EPOS4 `6083` is rpm/s; Nanotec N5 uses
configurable user units through `60A8`/`60A9`. Lichuan's `6083` is a time.

### What the library already has

The host layer is already the factor group, exact and checked:

- Exact fractions and strict decimal parsing (`Rational`, `parseExactNumber`): `include/MotorControlCAN/Units.h:16-21`, `:191`.
- Units: steps, full steps, encoder counts, turns, degrees, radians, millimetres (`Units.h:24-32`); rates per second
  or minute and accelerations with two time units (`Units.h:81-98`).
- Factors: command steps per motor turn (like `608F` for the command side), motor turns per load turn (like `6091`),
  millimetres per load turn (like `6092`), encoder scale, command polarity (like `607E`) (`Units.h:72-79`).
- Frames NATIVE, MOTOR, LOAD and a native rate basis (`include/MotorControlCAN/Axis.h:15-20`, `:81-84`, `:89-112`).
- One-time quantization with a chosen rounding (EXACT, NEAREST ties to even, TOWARD_ZERO, FLOOR, CEIL) and an exact
  error limit; the effective value and exact error are reported (`Units.h:116-130`, `:236-241`;
  `Axis.h:180-205`, `:268`).
- Console: positions in steps/fullsteps/counts/turn/deg/rad/mm, rates in rpm, turns/s and UNIT/s, frames and rounding
  (`examples/common/ProbeConsoleMotion.cpp:25-50`); gear, lead, polarity and scales through `axis`
  (`examples/common/ProbeConsole.cpp:439-441`).

What is native-only today:

- Ramps are milliseconds only: `MoveProfile`/`VelocityProfile` and the console `accel`/`decel` commands. By rule no
  millisecond ramp becomes an acceleration (`docs/units_status.md:78`).
- The simple-move `speed` command takes integer motor rpm only (`ProbeConsole.cpp`, `speed` entry).

### Physically achievable

- Speed resolution is 1 motor rpm (whole-number objects). With `2201=1000` that is 16.7 pulses/s. The subdivision
  does not change it (unmeasured whether `6081` rpm scales with `2201`; it was never changed).
- 2.5 rpm at the motor cannot be sent. No other object offers finer speed: the EDS has no `606B`, `6080` or similar.
  Options, simplest first:
  1. Refuse (the default, `Rounding::EXACT` gives `FRACTIONAL_TARGET`).
  2. Round with the error reported. NEAREST with an allowance of at least 1/2 rpm sends 2 (ties go to even) and reports
     an error of -1/2 rpm. The caller chooses the rounding and the allowance.
  3. Gearing. 2.5 rpm at a 10:1 load is exactly 25 motor rpm, so the LOAD frame works exactly.
  Alternating 2 and 3 rpm to fake an average is not industry practice and is not proposed.
- Measured: the `6064` rate is 0.992 of the command (a). Report it as an unexplained observation, not a correction.
- Acceleration: if the ramp is the time from zero to the target speed (inferred, a), then
  a = (rpm / 60) / (ms / 1000) turns/s². The reachable range depends on the speed. At 3000 rpm: 25 turns/s² (2000 ms)
  to 50,000 turns/s² (1 ms). At 60 rpm: 0.5 to 1000 turns/s². So "3000 rpm at 1 turn/s²" needs a 50 s ramp and must be
  refused. The ramp quantum is 1 ms, so the effective acceleration differs from the request and must be reported.

### Recommendation

Keep the existing exact layer. Add one thing to it: acceleration and deceleration in user units, converted per request
to the Lichuan millisecond ramps.

- `t_ms = target_speed / acceleration`, both in the same units, times 1000, then quantized once with the caller's
  rounding and allowance. The target speed is the prepared integer rpm, so the error of both conversions is known.
- Refuse: zero or negative acceleration; a ramp below 1 ms or above 2000 ms after rounding; fractions under EXACT;
  overflow; radians without an explicit approximation allowance; and any acceleration conversion while the ramp law is
  unmeasured for that mode (a closed gate, as for moves).
- Report the effective acceleration exactly: `rpm × 1000 / (60 × ms)` turns/s².
- Console grammar: `accel VALUE UNIT` and `decel VALUE UNIT` with UNIT ms (as today), rpm/s, turn/s2, deg/s2, mm/s2;
  `speed VALUE UNIT` with rpm, turn/s, deg/s, mm/s, using the axis frame. Show effective values and errors.

### Prompt steps

1. Measure the ramp law in both modes during the (a) ladder: time to 95 % speed and stop distance at several
   (rpm, ms) pairs, including the 0.992 ratio at high speed.
2. Library (Lichuan profile): a pure `prepareRampTimes` (or an optional acceleration field in the move and velocity
   preparations) with a `RampLaw` evidence field, `UNMEASURED` by default and refused.
3. Host tests: 2.5 rpm under EXACT (refused) and NEAREST (2, error -1/2); 2.5 load rpm at gear 10 (25, exact); 10 mm/s
   with a 5 mm lead (120 rpm); a ramp above 2000 ms refused; 1 ms boundary; effective acceleration exact; overflow.
4. Console and docs: the new unit words, `docs/units_status.md`, `docs/cli_contract.md`.

## d) Typed mode switch

User: "No typed mode switch - do it ... (literally a function in the lib to call it, right?)". Yes: one library
function, used like the other actions.

### Rules for writing `6060`

- Codes: 0 undefined, 1 position, 3 velocity, 6 homing (EN86 p11, reopened). Mode 0 is not an operating mode.
- Type and access: manual INTEGER8 RW (EN86 p11, p13, reopened); EDS `[6060]` U8 WO. `6061` is RO in EN86 p11-13
  (reopened), while ZH86 p17 lists it RW (reopened) (C08, C09, C10).
- Order: every procedure sets the mode, then observes `6061`, then enables (EN86 p12, p13; ZH86 p13; reopened). EN86's
  "set the monitoring operating mode (6061h-00)" is a mistranslation; the Chinese says monitor (ledger, motion facts).
  So: never write `6061`, and never read `6060` back (D04: WO commands, RO display).
- Undocumented: changing `6060` while enabled (Q11, D-02), codes other than 0/1/3/6 (D17), and how soon `6061` follows.

### Measured on the bench

- Trial 917 (node 2): released (`2100=0011`, `6041=8050`), mode 1, standstill at -49 on two reads; `6060=3`
  (`2F 60 60 00 03 00 00 00`) acknowledged; `6061=3` on the next reads, still released at -49.
- Mode 0 to 1 at power-up (the P2 step, 23 retained records from 102 to 1722, all succeeded): the drive reports the
  enabled pattern (`8037`, `2100=0001`) and `6061` then reads 1 (`8437`). This is a mode write on a drive that holds
  torque, from mode 0.
- 1 to 3 released: 10 more standalone `mode3` records (1008 to 1725), each from `8050`/`0011` in mode 1, and 48
  night-soak runs; 4 night-soak runs were refused by the bench check before the write ("running").
- 3 to 1 released: records 2101 (from `8050`/`0011`) and 2221 (from the latch `C050`/`0015`), and 47 night-soak runs.
- No mode write while enabled in mode 1 or 3, and no write of 6 or of an undocumented code, has run.

### Shape of the operation

Follow the existing action pattern (`prepareStop`, `prepareEnable` in
`include/MotorControlCAN/profiles/lichuan_c/Actions.h`):

```cpp
Status validateModeSelect(const ActionContext& output, const AxisReservation& reservation,
                          const ReadTarget& current, const ReadContext& state, const ModeEvidence& evidence,
                          uint8_t modeRaw, uint32_t operation, uint64_t nowUs, uint64_t deadlineUs,
                          const ActionOptions& options) noexcept;
Status prepareModeSelect(ActionContext& output, /* same arguments */) noexcept;
// then the existing nextAction, noteActionTransmission and advanceAction
```

- New `ActionKind::SELECT_MODE` (CANopen-specific name).
- `ModeEvidence`: gate (`CLOSED`, `MEASURED`, attended experiment for the D17 probe), record, model, and the measured
  source-to-target transitions as a small list (0 to 1 at the power-up report; 1 to 3, 3 to 1 released; others closed).
- Admission: a fresh complete ERRORS, MONITORS, STATUS, FEEDBACK read under the reservation; no alarm, `1001` and
  `603F` zero, `606C` zero, `2100` running clear (or the measured latch of a moving quick stop with `6064` unchanged
  over two reads, as the bench `MODE_THREE` check admits); released (`2100` bit 4 set), or the power-up report in mode 0
  with no motion write recorded (the P2 case). Target code 1, 3 or 6 only, different from the current one. Refuse an
  enabled drive in modes 1, 3, 6.
- Work: one download `2F 60 60 00 0M 00 00 00`, then fresh cycles. Success: `6061` equals the code, release and
  `6064` unchanged, `606C` zero. Other `6061`: `OBSERVATION_SCOPE`. Abort: `REJECTED`. Lost reply: `UNKNOWN`,
  uncertain, never resent.
- The session records an acknowledged mode select as the one write the enable admits before its words (the enable
  already accepts "one acknowledged 6060 selecting this mode", `Actions.h:173-222`). It replaces the raw write path
  (`examples/common/AxisSession.cpp:42-45`; bench `MODE_ONE`/`MODE_THREE`, `examples/s2_bench/BenchRoute.cpp:2040-2048`,
  `:2129-2340`). The velocity operation's own mode phase (`src/profiles/lichuan_c/Velocity.cpp:219-226`, `:295-298`)
  can stay for `MEASURED` records or be dropped in favor of the typed select.

### Prompt steps

1. Library: `ActionKind::SELECT_MODE`, `ModeEvidence`, `validateModeSelect`/`prepareModeSelect`, advance handling;
   `AxisSession::requestModeSelect`; console `mode 1|3|6`.
2. Host tests: each refusal (enabled, running, alarm, `606C` nonzero, unknown code, same code), the exact wire bytes,
   abort, lost reply, `6061` mismatch, latch admission, and replays of 917, 2101, 2221 and one P2 record value for value.
3. Bench: the typed select for 0 to 1 at power-up, 1 to 3 and 3 to 1 released (short regression), then 1 to 6 released
   before any homing. A mode change while enabled stays a separate gated question.
4. The D17 probe (`6060` = 2, 4, 7, 8, 9, 10, drive not enabled, write back a documented mode) through the same
   operation under the experiment gate; an accepted code opens nothing.

## e) Enable and release as regular commands

User: "Enable is experiment-only, and there is no 'free the shaft' command - add these to be regular commands".

### Documented facts

- Enable: words 6, 7, 15 "at an interval of about 10ms. After writing, the motor can be enabled" (EN86 p12, reopened;
  ZH86 p12 and OP86 p9 per ledger).
- Status pairs: 6040=0 gives 6041 low bits 000; 6 gives 001; 7 gives 011; 15 gives 111 (EN86 p12, reopened).
- `2100` bit 4: "Motor enabling bit, 0: Enable, 1: Release" (EN86 p17, text; ZH86 p18, text: 0 使能, 1 释放).
- No page says which word frees the shaft. `6040=0` appears only as the status pair above. DI function 4 is labeled
  motor enabling with no behavior (D-28). Quick stop `15->11` is "Emergency stop" (EN86 p12, reopened).

### Current code

- `EnableGate` has only `CLOSED` and `ATTENDED_EXPERIMENT`: `include/MotorControlCAN/profiles/lichuan_c/Actions.h:166-171`.
- `EnableEvidence` needs a one-use trial identity, the exact reservation and operation, an expiry, a start record,
  a power-up shaft observation when the start reads enabled, and exact reviewed `6041`, `607A`, `60FF`
  (`Actions.h:235-258`; checks in `src/profiles/lichuan_c/Actions.cpp:740-816`).
- "Release has no API" (`Actions.h:57`); the console `motor-release` is a gated stub
  (`examples/common/ProbeConsole.cpp:168-171`). The common `ActionKind` has no RELEASE
  (`include/MotorControlCAN/ActionOperation.h:10-15`); RS has `ActionKind::RELEASE` and `prepareRelease`
  (`MotorControl-RS/include/MotorControlRS/ActionOperation.h:10`, `profiles/ess_rs/Actions.h:80`).

### Measured on the bench

- Enables: 30 retained records, 28 succeeded; 103 and 105 (2026-11-03, node 1) failed before the bit 10 fix and after
  a hand displacement. The night soak added 83 successful enables. So 111 successful enables, two specimens.
- Reports, power-up start (21 records): `8437`/`0001` in mode 1, then word 6 `8031`/`0011` (released), word 7
  `8033`/`0001`, word 15 `8437`/`0001`.
- Reports, start after a quick stop: from `8050`/`0011`, `4050`/`0014` or `C050`/`0015`; word 6 `x031` with the same
  monitor; word 7 `8033`/`0001` or `0033`/`0000`; word 15 `8437`/`0001` or `0037`/`0000`.
- Word 6 frees the shaft: both shafts held torque at power-up (user, by hand), word 6 set `2100` released on both, and
  node 1's shaft then turned freely (2026-11-03, `docs/drive_actions.md`). Every later power-up enable shows the same
  report at word 6.
- A quick stop (`000B`) also releases: low bits 000 and `2100` released; the user turned node 2's released shaft by
  hand on 2026-11-05.
- Word 7 energizes and pulls a displaced shaft back to the drive's internal command (trial 308: 1187 to 1194 to 1180;
  trial 105: about half a turn). No mode 1 enable ever started motion from a retained `607A`.
- `6040=0` has never been sent.
- S20: node 2 stayed enabled and halted (`1437`/`0000`) after a HALT run from a latched release; no stop path accepted
  that report, and a power cycle was needed. A typed release is the natural exit, but word 6 from a halted state is
  unmeasured.

### Recommendation

- Enable: add `EnableGate::MEASURED` with a reviewed record built from the 111 enables. Under it: no one-use trial
  identity or expiry (keep the operation identity); the start record stays the application's statement; the power-up
  shaft observation is not required when the record states the power-up report holds torque on this model; `607A` and
  `60FF` are read and reported, not required to equal reviewed values, in mode 1 only (no mode 1 enable started
  motion). Keep every observation rule and the word 7 position rule. Mode 3 enable stays inside the velocity run.
- Release: `prepareRelease` writes `0006` once (RS name). Admit it from a stationary enabled report: mode 1 idle
  (`8437`/`0001` or `0037`/`0000`) as measured, and the halted mode 3 report after measurement. Refuse a moving drive
  (use a stop) and an already released one. Success: two fresh cycles with low bits 001, `2100` released, `606C` zero.
  Effects: the axis reference is invalidated (`ReferenceLoss::POSITION`): the shaft may be turned, and the next word 7
  pulls back. Release never clears a stop fence or `needsStop`. The next enable needs a start rule for the `0x31`
  pattern word 6 leaves (today's released start expects `0x50`, the quick stop's).
- `6040=0` stays an unmeasured alternative.

### Prompt steps

1. Library: `EnableGate::MEASURED`; `ActionKind::RELEASE`; `validateRelease`/`prepareRelease`; an enable start for a
   release this application recorded; `AxisSession::requestRelease`; console `enable` and `motor-release` made real.
2. Host tests: release admission matrix, wire bytes `2B 40 60 00 06 00 00 00`, completion, lost reply, refusal while
   moving or released, reference invalidation, enable after release, fences kept; `MEASURED` enable without trial
   identity; replays of 206, 2102 and 2201.
3. Bench: release from mode 1 idle after a completed move (drive report; one hand check if the user allows); release
   from the halted mode 3 report (`9437`/`0001`) and, if it recurs, from `1437`/`0000` (S20); re-enable after release
   with `6064` before and after word 7.
4. Docs: `docs/drive_actions.md` release section, D-17, prompt 30 report, coverage.

## f) Motion locked to the exact measured firmware

User: "Motion is locked to the exact measured firmware - we dont need this rule".

### Where the lock is

| Operation | Record field | Exact 8-value comparison |
| --- | --- | --- |
| Move | `MoveFirmware`, `include/MotorControlCAN/profiles/lichuan_c/Position.h:146-155`, `:179` | `src/profiles/lichuan_c/Position.cpp:179-184`, refusal `:381-385` |
| Velocity | `VelocityFirmware`, `Velocity.h:40-49`, `:57` | `src/profiles/lichuan_c/Velocity.cpp:54-59`, refusal `:203-207` |
| Homing | `HomeFirmware`, `Homing.h:31-40`, `:50` | `src/profiles/lichuan_c/Homing.cpp:61-66`, refusal `:193-197` |
| Enable | No firmware field; locked to a one-use trial, reservation, operation and expiry instead | `Actions.cpp:740-766` |

The eight values are `1000`, `1009`, `100A`, `1018:01..03`, `2000`, `2001` (`IdentityObservation`,
`include/MotorControlCAN/profiles/lichuan_c/Reads.h:173-189`). The bench arming lines carry them too
(`examples/s2_bench/BenchRoute.cpp:262-343`).

### Recommendation

Key records by model; report firmware; flag a mismatch.

- One helper, for example `compareIdentity(record, identity) -> IdentityMatch { SAME, FIRMWARE_DIFFERS, PRODUCT_DIFFERS }`.
- Product fields must match, or the preparation refuses: device type `1000`, vendor `1018:01`, product `1018:02`, and
  model code `2000` against the selected model. A different product is not a firmware update.
- Firmware fields may differ: `1009`, `100A`, `1018:03`, `2001`. The operation proceeds, the context keeps
  `FIRMWARE_DIFFERS` and the observed identity, and the terminal result and console output say "unqualified firmware".
- What still protects a different firmware: every fresh-state admission, the acceptance-absent rule, the completion
  rule, deadlines, the run's own stop and the uncertainty rules. Risk to state plainly: a firmware with another status
  dialect can make the record's signals wrong; a move then ends at its deadline with completion not observed, and a
  velocity run's running check fails. Both end with a stop request, not silent motion.
- Record the user's decision of 2026-11-12 in a decision file, because it changes the "per-firmware qualification"
  rule the docs repeat.

### Prompt steps

1. Library: the helper; an `identityMatch` field in move, velocity and home contexts; replace the three refusals.
2. Host tests: each of the eight fields changed alone (four refuse, four flag); the flag reaches the terminal result;
   a flagged success never reads as qualified.
3. Console and docs: a `firmware_match` field in motion results; coverage, `docs/position_parameters.md`,
   `docs/velocity.md`, `docs/homing.md`, Q01 note.
4. Bench: nothing new on this fixture (one firmware). Run the short regression when a second firmware arrives.

## g) Loading measured records from the console

User: "The console can't load the measured records motion needs - add this functionality".

### Today

- Firmware code builds `MoveEvidence`, `VelocityEvidence`, `HomeEvidence` and `EnableEvidence` and calls
  `ProbeMotion::setMovePrerequisites`, `setVelocityPrerequisites`, `setHomePrerequisites`, `setEnablePrerequisites`
  (`examples/common/ProbeMotion.h:136-184`).
- The shared console refuses on purpose: "velocity qualification" and "home qualification" are gated entries, "No
  console-created qualification" (`examples/common/ProbeConsole.cpp:258-268`; prompt 42 report: install/revoke shapes
  stay caps-only).
- The bench image already loads records from text: the `Y` (ARM_MOVE) and `B` (ARM_VELOCITY) lines carry the record
  number, signals, completion rule, base, layout, rpm, ramps and the eight identity values
  (`examples/s2_bench/BenchRoute.cpp:292-343`, `:376-378`). `BenchConsole::arm` turns them into evidence and calls the
  setters (`examples/s2_bench/BenchConsole.cpp:335-470`). This is the precedent.

### Recommendation

Keep the library API primary. Add a pure record codec to the Lichuan profile and thin console routes in the example.

- Library (pure C++11, bounded, no I/O): a `MotionRecord` value type (record id, model, the identity it was measured on,
  and per-capability sections: move signals and rule and base; velocity layout, start/stop facts, stop kind, forward and
  reverse range; enable facts; mode transitions; ramp law), plus `parseMotionRecord`, `formatMotionRecord`,
  `validateMotionRecord` and converters to the existing evidence structs.
- Text format: one ASCII line per capability, `key=value`, at most 255 characters, a CRC-32 at the end against
  transcription errors (not a security check). For example:
  `rec 3 move model=cl86c id=00040192/0C/20/00001000/1/0/0400/0200 accept=sw:1000/1000 done=mon:0001/0001 rule=absence base=actual crc=…`
- Validation on load: grammar, every range (rpm 1..3000, ramps 1..2000, nonzero masks, known enums), known model,
  nonzero and unique id, CRC, a fixed capacity per axis. Loading never moves anything.
- Use: `record use ID` acquires a fresh identity and state read, compares the identity as in (f), and calls the
  existing setters, as `BenchConsole::arm` does.
- Storage: volatile by default. `record save` and `record restore` only on explicit command, through an application
  storage hook: NVS on ESP-IDF, Preferences on Arduino, a file on native. No automatic restore at boot. The core never
  persists anything.
- Reviewed records live in the repository (for example `docs/records/`), produced by a script from retained bench
  evidence and reviewed like the ledger.

### Prompt steps

1. Library: `MotionRecord`, codec, validation, converters; Doxygen.
2. Example: `record load|show|list|forget|use|save|restore`; the storage hook with NVS, Preferences and file versions;
   the same semantics on Arduino, IDF and native.
3. Host tests: round trip; every refusal; CRC mismatch; oversize line; duplicate id; load then use through the fake
   owner; a fake storage hook; identity mismatch flagged.
4. Script: build a record from bench records (move from 121/213/413/411/708; velocity from rounds 7 to 12 and the night
   soak), with review notes.
5. Bench: load a record from the console and run a short move and velocity regression on both nodes.

## h) Roadmap coverage

From `docs/prompts/PROGRESS.md` (prompts 1 to 54). The untracked drafts 62, 64, 65 and 67 were present in the working
tree during this review; 64 lists enable, release and mode select among its common operations, and 67 references a
prompt 56 that is not present.

| Request | Owner prompts and status | Coverage |
| --- | --- | --- |
| a) Full speed range | 35 (Partial; owns the velocity range, item 1); 33 (Partial; position speed); 29 (Partial; conditional experiments); 47, 48 (Pending; qualification and stress); 52 (Pending; F10 ramp meaning) | Partly. The documented range is owned by 35/33 and qualification by 47. Nothing owns above-limit tests, the step-rate question or the probe scope. |
| b) Negative speed | 35 item 1 "validate reverse-direction semantics" (Partial); Q03 via 29; 52 (Pending) says negative stays refused; F15 | Partly, with a conflict: prompt 52's text keeps it refused. |
| c) User units | 9 and 10 (Complete) own the exact unit layer; 42 (Complete) owns the console grammar; 52 owns F10 | Partly. Acceleration to ramp time is not owned (Q11 keeps ms only). |
| d) Typed mode switch | 35 item 2 (velocity's own mode phase); 36 (mode 6); 52 (D17 probe); 30 (no mode op); draft 64 (common "mode select") | Not owned as a standalone operation. |
| e) Enable and release | 30 (Partial) owns `prepareEnable` and `prepareRelease` (item 2); 42 (Complete) keeps `motor-release` gated | Fully owned by 30, blocked on its gates. A `MEASURED` gate and the word 6 release are the missing pieces. |
| f) Firmware lock | Rule built into 33, 35, 36 and 29; 47 records identity | Not owned. No prompt removes or relaxes the lock. |
| g) Record loading | 42 (Complete) keeps install/revoke caps-only; 44 (Pending) runs campaigns; draft 65 (application package) | Not owned. |

### Proposed prompt steps, in order

1. **Records and identity (f, g).** Identity comparison helper with the flag; `MotionRecord` codec and validation;
   console `record` routes with volatile storage and explicit NVS save; host tests; a record script and one reviewed
   record per capability from existing bench evidence; a short bench regression.
2. **Mode select and release (d, e).** `prepareModeSelect` and `prepareRelease` with `ActionKind::SELECT_MODE` and
   `RELEASE`; `EnableGate::MEASURED`; console `mode`, `enable`, `motor-release`; host tests and replays; bench trials:
   typed mode regression, release from mode 1 idle and from the halted mode 3 report, re-enable after release.
3. **Full-range ladder (a).** Range-probe experiment option and over-speed check; runner and route caps for the probe
   only; `6081` write probe; the Part 1 ladder in both modes, then Part 2 on node 1; record the ramp law and the
   0.992 ratio at high speed; ledger and conflict updates.
4. **Reverse (b).** Reverse-probe option with the wrong-sign rule; the -6 rpm measurement after step 3; then -30, -60
   and the reverse ladder; a `MEASURED` record with `reverseQualified`; update prompt 52 and F15 wording.
5. **Acceleration units (c).** `RampLaw` evidence from step 3; `prepareRampTimes`; console unit words for `speed`,
   `accel` and `decel`; host tests for the quantization cases above.

Each step keeps the existing rules: explicit operations only, no retry or replay, uncertainty kept, the run's own
stop, attended runs for anything above the measured range, and documented, measured and inferred facts kept separate.
