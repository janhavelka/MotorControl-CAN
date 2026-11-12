# Prompt 59 Velocity in full range and in both directions

Apply [COMMON.md](COMMON.md). Prerequisites: [35](35_velocity.md), [52](52_motion_changes.md), [58](58_measured_records.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "Velocity runs - do the tests in full range (mind about the drives
pulse limit - try all of it and above and record whats happening)"; "No negative speed - i see that for velocity mode
- this should probably be allowed - check what the datasheet says and what it allows and how it should be treated
properly - if unclear - test it". This supersedes, for velocity mode, the earlier instruction to refuse negative speeds;
position moves keep positive speeds with signed targets.

Facts ([motion capability report](../reports/2026-11-12_motion_capability_evidence.md) sections a and b): the only
documented limits are 0..3000 rpm for `6081`, `60FF` and `6099` and 0..2000 ms for ramps (EN86 p11/p13); no pulse or
step-frequency limit appears in any retained source, and the EDS has no `6080`, `607F` or `607E`; "too fast" is a
listed cause of the FF05 out-of-tolerance alarm (EN86/ZH86 p20). `60FF` is I32 in the EDS and the p17 dictionaries
but U32 0~3000 in the p13 tables (Q03); no page names a direction rule; `606C` already reads signed rpm; the drive
stored `60FF=3001` without a range check (trial 2001). Bench maxima so far: 300 rpm in position mode, 60 rpm in
velocity mode; `6064` runs at 0.992 times the command (cause unknown).

Deliver: a full-range ladder in both modes with an above-limit probe, the measured range and clamp or alarm behavior
in the records, signed velocity treated as I32 rpm once measured, and a measured reverse record.

Boundaries: unloaded, bolted motors, one node at a time; the above-limit part is attended (the one physical presence
this prompt needs) and has a hard ceiling of 6000 rpm (twice the documented value, project policy); a range probe
never reads as qualified; after an alarm the ladder ends and further motion waits for the user's next power cycle,
which is not requested; no retry after a lost reply; best-effort stop on any doubt.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its velocity sign seam
(`negativeTwosComplementVerified`); RS is authoritative for common names. Inspect FieldCore-node read-only for speed
limits in motor modules. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N"
and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax
vendor evidence, safety, uncertainty or verification rules. Reopen EN86/ZH86 p11, p13, p17 and p20 and the EDS rows,
`Velocity.h`/`Velocity.cpp`, `Position.cpp`, the bench reports and the report's ladder and stop rules. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. A range-probe option in the velocity experiment scope (up to 6000 rpm, minimum ramps, attended only) and an
   over-speed check in the running cycle (`|606C|` above the command plus a stated tolerance ends the run with its
   stop); `MEASURED` stays 1..3000 until a reviewed record says otherwise.
2. Bench routes and a campaign script for the ladder: Part 1 in both modes (position 300 to 3000 rpm, velocity 60 to
   3000 rpm, plus 1 rpm once), Part 2 in velocity mode above 3000 to the first clamp, alarm, stall or abnormal noise,
   then `6081` write/readback probes; one summary row per step with the report's recorded quantities and stop rules.
3. Reverse: a write of `60FF=-6` without motion and readback, then one attended -6 rpm run with an immediate quick stop
   on a wrong sign or over-speed, then -30 and -60, then a reviewed reverse record (prompt 58) and the signed
   velocity request in the library; a sign change during a run stays with prompt 52's measurements (F15).
4. Records: ledger value policy of `60FF` and `6081` with the measured range and clamp or alarm behavior, the ramp law
   (Q11, F10) from the ladder, device conflicts, coverage and backlog.

## Verify and exit

Host tests: 60/61, 3000/3001 and 6000/6001 boundaries under each gate, the over-speed check, signed rates refused
without the reverse record and accepted with it, a probe run never qualified. Run the native, firmware, stack and
documentation gates and CI on the actual final commit. Report every step's outcome, including the ones that stopped
the ladder.

## Documentation and delegation

Update the velocity and position guides, the CLI contract, coverage, backlog (Q03, Q11, F02, F15), CHANGELOG and
Doxygen in the same stage. One lead controls the port; agents analyze evidence read-only. Record
`docs/reports/prompt_59.md`. Commit and sync on the current branch before handoff.
