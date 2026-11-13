# Prompt 59 Velocity in full range and in both directions

Apply [COMMON.md](COMMON.md). Prerequisites: [35](35_velocity.md), [52](52_motion_changes.md), [58](58_measured_records.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "Velocity runs - do the tests in full range (mind about the drives
pulse limit - try all of it and above and record whats happening)"; "No negative speed - i see that for velocity mode
- this should probably be allowed - check what the datasheet says and what it allows and how it should be treated
properly - if unclear - test it". This supersedes, for velocity mode, the earlier instruction to refuse negative speeds;
position moves keep positive speeds with signed targets. Added on 2026-11-13: "its usual for such drives to have mostly
200k and some 500k pulses/sec as their own limit - check those boundaries with practical tests - no need for stopping
after first signs of problems - explore the boundaries well".

Facts ([motion capability report](../reports/2026-11-12_motion_capability_evidence.md) sections a and b): the only
documented limits are 0..3000 rpm for `6081`, `60FF` and `6099` and 0..2000 ms for ramps (EN86 p11/p13); no pulse or
step-frequency limit appears in any retained source, and the EDS has no `6080`, `607F` or `607E`; "too fast" is a
listed cause of the FF05 out-of-tolerance alarm (EN86/ZH86 p20). `60FF` is I32 in the EDS and the p17 dictionaries
but U32 0~3000 in the p13 tables (Q03); no page names a direction rule; `606C` already reads signed rpm; the drive
stored `60FF=3001` without a range check (trial 2001). Bench maxima so far: 300 rpm in position mode, 60 rpm in
velocity mode. The `6064` rate is 0.99201 x floor(rpm x 1000 / 60) counts/s at `2201=1000` (night soak, 20 speeds 6 to
300 rpm, both nodes); it is not a drive-clock error, since the drives' heartbeat and TPDO timers keep within 0.04 % of
the S2's clock; the cause is unknown ([units re-audit](../reports/2026-11-13_units_reaudit.md) section 3).

Pulse rate (inferred arithmetic, not a drive fact): command pulses per second = rpm x `2201` / 60. At the bench's
`2201=1000`, 3000 rpm is only 50,000 pulses/s, so the usual 200 kHz or 500 kHz limits of pulse-input drives would need
12,000 or 30,000 rpm and cannot be reached at this subdivision. They can be reached at motor speeds the ladder already
covers only with a higher subdivision: at `2201=10000`, 200 kHz is 1200 rpm and 500 kHz is 3000 rpm; at `2201=51200`,
200 kHz is about 234 rpm, 500 kHz about 586 rpm and 1 MHz about 1172 rpm. Whether this bus drive has a step-rate
ceiling at all, or computes its trajectory in encoder counts, is unknown; the subdivision grid (Part 3) shows it.
The units re-audit and the [speed units guide](../speed_units.md) hold the reviewed unit facts.

Deliver: a full-range ladder in both modes with an above-limit probe, a pulse-rate grid across subdivisions, the
measured range and clamp, stall or alarm behavior in the records, signed velocity treated as I32 rpm once measured,
and a measured reverse record.

Boundaries: unloaded, bolted motors, one node at a time; the above-limit and grid parts are attended (the one physical
presence this prompt needs, for the cutoff, not for shaft reports) and have a hard ceiling of 6000 motor rpm (twice
the documented value, project policy); a range probe never reads as qualified; no retry after a lost reply;
best-effort stop on any doubt. Exploring the boundary is the goal: a plateau, a stall or a resettable alarm ends that
run, not the ladder (see the continuation rule in item 2). Only damage signs (smell, smoke, heat, a loose part,
grinding) or a non-resettable alarm end the ladder on that node; a non-resettable alarm leaves the node for its next
natural power cycle, which is not requested. On 2026-11-13 the user waived the attendant for this ladder campaign only
("Unattended, I accept it": the velocity ladder, above-limit probe and resolution grid without them at the bench,
motors bolted, free shafts); the runner records it with `--unattended-waiver` and `--waiver-covers-motion`.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its velocity sign seam
(`negativeTwosComplementVerified`); RS is authoritative for common names. Inspect FieldCore-node read-only for speed
limits in motor modules. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N"
and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax
vendor evidence, safety, uncertainty or verification rules. Reopen EN86/ZH86 p11, p13, p17, p18 and p20 and the EDS
rows, `Velocity.h`/`Velocity.cpp`, `Position.cpp`, the bench reports, the units re-audit and the capability report's
ladder and stop rules. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. A range-probe option in the velocity experiment scope (up to 6000 rpm, minimum ramps, attended only) and an
   over-speed check in the running cycle (`|606C|` above the command plus a stated tolerance ends the run with its
   stop); `MEASURED` stays 1..3000 until a reviewed record says otherwise.
2. Bench routes and a campaign script for the ladder, one summary row per step with the report's recorded quantities:
   - Part 1, both modes: position 300 to 3000 rpm, velocity 60 to 3000 rpm, plus 1 rpm once. During the attended
     part, one count of shaft turns over a +100,000-pulse position move (a shaft mark or an optical tachometer)
     separates "a different number of pulses per turn" from "the rate is short" for the 0.992 finding.
   - Part 2, velocity mode above 3000: 3001, 3300, 3600, 4000, 4500, 5000, 6000 rpm, then `6081` write/readback probes.
   - Continuation rule: a plateau, stall or resettable alarm (FF05) ends the run with its stop; the script records it,
     clears a resettable alarm with the typed alarm clear as an explicit, logged test step (never enabling), waits for
     a released standstill shown by two fresh reads, repeats that step once, then bisects between the last clean and
     the first failing step to about 5 % or 100 rpm, and then continues upward in the remaining steps to the ceiling to
     see whether the behavior changes (clamp, stall, alarm). The same alarm three times in a row on one node ends that
     node's climb. Every stop, clear and repeat is in the summary.
3. Part 3, pulse-rate grid: a volatile typed `2201` write with readback, only from a released standstill, only at
   values inside both documented ranges (400..51200 and 0..65535, Q04): 1000, 4000, 10000, 25600 and 51200. This is a
   five-point grid, not a range sweep; 0..399 and 51201..65535 stay untested. The write invalidates position-scale
   evidence through the existing configuration generation. Its activation (at once, at the next enable or only after
   a power cycle) is the first measurement; if it needs a power cycle, Part 3 waits for a natural one. At each
   subdivision, velocity runs whose pulse rates cross 50k, 100k, 200k, 300k, 500k, 750k and 1M pulses/s and beyond,
   up to the motor ceiling, with the item 2 continuation rule; a position move of whole turns after each step checks
   that `6064` returns to its start without an FF05, and the measured rate is compared with the floor x 0.99201 model
   to record the truncation at each subdivision. The starting `2201` is restored at the end without a save and read
   back.
4. Reverse: a write of `60FF=-6` without motion and readback, then one attended -6 rpm run with an immediate quick stop
   on a wrong sign or over-speed, then -30 and -60, then a reviewed reverse record (prompt 58) and the signed
   velocity request in the library; a sign change during a run stays with prompt 52's measurements (F15).
5. Records: ledger value policy of `60FF`, `6081` and `2201` with the measured range, pulse-rate ceiling (or its
   absence) and clamp, stall or alarm behavior, the ramp law (Q11, F10) from the ladder, device conflicts (Q03, Q04),
   the re-audit's proposed ledger notes (`6064` rate model, `606C` 2 rpm steps, `6083`/`6084`/`609A` p11 figure and
   fits), coverage and backlog.

## Verify and exit

Host tests: 60/61, 3000/3001 and 6000/6001 boundaries under each gate, the over-speed check, signed rates refused
without the reverse record and accepted with it, a probe run never qualified, `2201` values 399/400 and 51200/51201
and the grid points, scale invalidation after a `2201` write, and the continuation rule's clear step never sending an
enable word. Run the native, firmware, stack and documentation gates and CI on the actual final commit. Report every
step's outcome, including the runs that stopped and the points where the drive clamped, stalled or alarmed.

## Documentation and delegation

Update the velocity and position guides, the speed units guide, the CLI contract, coverage, backlog (Q03, Q04, Q11,
F02, F15), CHANGELOG and Doxygen in the same stage. One lead controls the port; agents analyze evidence read-only.
Record `docs/reports/prompt_59.md`. Commit and sync on the current branch before handoff.
