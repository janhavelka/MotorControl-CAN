# Prompt 52 Changes of velocity, ramps and targets during motion

Apply [COMMON.md](COMMON.md). Prerequisites: [29](29_device_evidence.md), [33](33_position_moves.md), [35](35_velocity.md), [42](42_cli_motion_routes.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "In future i would like to have and test the functionality of changing
velocity, acceleration, targets and other on the fly"; "i would test the working with motion already in place to see
what happens. What about not just updating the motion parameters with controlword bit 5 but updating the whole target?";
"i want generally the library to allow everything such drives can offer as openly as possible".

Deliver measured and then typed operations for changes during motion: a velocity rate change during a finite velocity
run, a profile-position speed or ramp change of a running move, and a whole-target replacement of a running move (new
`607A` with the bit 4 set-point handshake, with bit 5 change-immediately and with bit 5 = 0 buffered), each with its
console route. Openness means every capability the drive can offer is reachable: documented behavior through typed
operations, undocumented behavior through an explicit, recorded experiment scope that measures it, never by hiding it
and never by claiming it as qualified.

Lichuan facts ([on-the-fly study](../reports/2026-11-12_on_the_fly_changes.md)): EN86/ZH86 p12 document bit 5 (0:
update after the current curve, 1: update immediately) with arrows only from `000F` (`15->63`, `15->127`); not
documented are bit 9, a set-point buffer, what clearing bit 4 does during a curve, and whether a `60FF` change while
running takes effect. The EDS allows `6040`, `607A`, `6081`, `6083`, `6084` and `60FF` in PDOs. The library refuses
immediate update today because a second set-point needs bit 4 to fall during the curve. Negative velocity stays
refused (Q03 and the user's instruction); a relative change mid-curve, reversal and a `60FF` sign change or zero during
a run stay refused until measured (backlog F15).

Boundaries: a change never extends the run's deadline or stop reserve, never enables, never replays an unconfirmed
write and never removes the run's own stop; a change admitted after a stop request is refused; uncertainty after an
unconfirmed change keeps the motion uncertain.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its position interruption seam
(`PositionInterruption::INTERRUPT_CURRENT`, `StoredPositionQualification`, `MoveRelation`) and speed handling; RS is
authoritative for common names. Inspect FieldCore-node read-only for motion integration. If HEAD, dirty state and seam
match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior
adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification
rules. Reopen EN86/ZH86 p12-13 and the EDS for every bit and object used, the [multi-axis and SYNC survey](../reports/2026-11-12_multiaxis_sync_survey.md)
for other libraries' set-point handling, and N5's set-point handshake only as contrast. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md). Record gates Q22 to Q26 (backlog F01)
before any implementation depends on them.

## Implement

1. Measure first, through prompt 29 experiment routes with motion in place (backlog F02, F05, F06, F07, F10, F11, F14):
   a mid-run `60FF` change; bare `6081`/`6083`/`6084` writes during a curve; the bit 4 falling edge during a curve;
   an immediate same-endpoint speed change (`003F`); a whole-target replacement ahead of the position with bit 5 = 1 and
   with bit 5 = 0; stops from `003F` and after a change; what ramp times mean under a change. Each trial records raw
   traffic, `6041` bits 10 and 12, `6064` and `606C` per cycle, and is reported as works, ignored or refused. Also
   the no-motion mode probe (backlog D17): with the drive not enabled, which undocumented `6060` values (2, 4, 7, 8,
   9, 10) the firmware accepts and shows in `6061`; an accepted value opens nothing by itself.
2. A velocity change step in `VelocityContext` (prepare/next/advance), closed by default, opened by the measured
   evidence or an explicit experiment scope; the run's own stop and deadline are unchanged.
3. A profile-position replacement lifecycle (speed change, ramp change, whole new target) with an original/replacement
   relation after RS, the set-point handshake the measurements establish, and an interlock with stops. Keep a
   per-profile set-point record with evidence per field and at most one outstanding set-point while the buffer depth
   is unknown (vendors differ: 0, 1, 2 or unstated); the generic bit 12 handshake is not copied, since the Lichuan
   latches bit 12 until `000F`.
4. Console routes (for example `velocity change ID RATE ...`, `move speed ID RPM`, `move retarget ID absolute ...`) with
   reserved stop capacity unchanged, and bench routes for the trials.
5. Keep refused, with tests: everything in F15, and any change on a route whose earlier write is unresolved.

## Verify and exit

Host tests drive every change path through the real owner and fake drives: change during a pending stop, change after
an unconfirmed write, budget and deadline boundaries, stop priority, refusal before admission. Run the native,
firmware, stack and documentation gates appropriate to the change, and CI on the actual final commit. Hardware
qualification of the finished operations belongs to prompt 54. A measured "ignored" or "refused" keeps that operation
closed and documented.

## Documentation and delegation

Update the position and velocity guides, the CLI contract, coverage, backlog F01-F16, CHANGELOG and Doxygen in the same
stage. A stronger reviewer challenges the handshake, stop interlock and uncertainty rules; normal coding agents own
disjoint test files. The lead integrates and runs combined checks. Record `docs/reports/prompt_52.md`. Commit and sync on
the current branch before handoff.
