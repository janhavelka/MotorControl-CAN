# Prompt 60 User units for speed and acceleration

Apply [COMMON.md](COMMON.md). Prerequisites: [10](10_axis_coordinates.md), [59](59_velocity_range_direction.md).

## Objective and boundaries

The user's requirement, verbatim (2026-11-12): "Native units only - extend this (explore the possibilities so its
physically achievable, industry standard - simplest functional - add it into the promptset/s)".

Facts ([motion capability report](../reports/2026-11-12_motion_capability_evidence.md) section c): the Lichuan EDS has
no CiA402 factor group (`6091`, `6092`, `608F`, `6096`, `6097`, `60A8`, `60A9`), so conversion is host-side. The
library already has the host unit layer (`Units.h`, `Axis.h`): gear ratio, lead, scale, polarity, frames and exact
rounding with the error reported, for positions and speeds. What stays native-only: ramps are milliseconds and speeds
are whole rpm, so 2.5 rpm cannot be sent (refuse with EXACT, or NEAREST gives 2 with the error reported). The
[speed units guide](../speed_units.md) and the [units re-audit](../reports/2026-11-13_units_reaudit.md) (2026-11-13)
hold the reviewed facts: no finer speed unit on any page, the p11 figure shows the ramp as the time from rest to the
commanded speed (a figure only; Q11 open), and the delivered rate is 0.99201 x floor(rpm x 1000 / 60) counts/s at
`2201=1000`, cause unknown.

Deliver the missing parts in the same style as the CiA402 factor group, the industry standard: acceleration and
deceleration in user units per second squared (for example mm/s^2, rev/s^2, deg/s^2) converted to ramp milliseconds,
speed requests in user units per second with the quantization rule stated per request, and the effective acceleration
and speed reported back. The conversion uses the ramp law measured in prompt 59 (inferred today: the ramp is the time
from 0 to the target speed; trials 709 and 713 fit it, Q11 open) and stays refused until that law is recorded.

Boundaries: exact checked arithmetic, no floating point in the installed core unless a reviewed decision allows it;
ramps under 1 ms or over 2000 ms are refused, never clamped silently; a unit request never widens a drive or record
limit; native operations stay available. Conversions use the effective whole rpm and report it; the 0.992 shortfall is
reported as an observation, never compensated silently. The common name for ramp times is decided against RS (its ESS
profile uses `accelerationTime`/`decelerationTime`) and the Lichuan names (`rampUpTimeMs`/`rampDownTimeMs`).

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its unit and acceleration seams; RS is
authoritative for common names and meanings. Inspect FieldCore-node read-only for user-unit configuration. If HEAD,
dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row
must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Read `Units.h`, `Axis.h`, the units and axis guides, the prompt 9, 10 and 59
reports, and the N5/EPOS4 factor-group material in `docs/reference/multidrive` as contrast only. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Acceleration and deceleration value types in exact units with the axis scale, and their conversion to native ramp
   time for a given speed change, with the rounding rule and the effective value reported.
2. Speed requests in user units per second (and per minute) with EXACT and NEAREST rules through the existing layer,
   applied to position, velocity and homing speeds.
3. Common requests carry the unit form; the profile converts to native values and refuses what it cannot represent.
4. Console forms of the same requests (for example `move ID absolute 12.5mm speed 20mm/s accel 100mm/s2`) that call
   the API only.

## Verify and exit

Host tests: conversion tables against hand-computed values, boundary refusals (ramp 0, ramp over 2000 ms, speed over
the record limit, unrepresentable EXACT requests), reported quantization errors, and identical native traffic for
requests that map to existing native values. Run the native, package, firmware, stack and documentation gates and CI on
the actual final commit. Bench: one move and one velocity run commanded in user units, compared with the drive's
reports.

## Documentation and delegation

Update the units, axis, position and velocity guides, the CLI contract, coverage, CHANGELOG and Doxygen in the same
stage. A stronger reviewer checks the arithmetic and rounding rules. The lead integrates and runs combined checks.
Record `docs/reports/prompt_60.md`. Commit and sync on the current branch before handoff.
