# Prompt 72 Enable-fault guards: pulse-rate admission, word 7 guard and fault-state reporting

Apply [COMMON.md](COMMON.md). Prerequisites: [57](57_regular_drive_operations.md), [59](59_velocity_range_direction.md).

## Objective and boundaries

Close the library gaps found by the enable-fault investigation of 2026-11-13 (backlog S21,
[report](../reports/2026-11-13_enable_fault_state.md)). The user's instruction, verbatim (2026-11-13): "Make super you
fill in the library gaps - put it into promptset or implement it now if simple". None of these was simple, so they
are specified here.

Facts, two CL86-C specimens, one firmware (`1000=00040192`, `1009=000c`, `100A=0020`), all measured unless marked:

- An FF05 raised while running above the command-pulse ceiling at `2201=51200` (about 2.6 to 2.7 MHz; clean at
  2.56 MHz, FF05 at 2.73 MHz) put the drive into a state where every later enable reports `606C` 121 to 256 rpm at
  word 7 with no command, turns the rotor about 0.16 turn and raises FF05 0.50 to 0.51 s after word 7. 3 of 3 such
  FF05s did; FF05s at `2201` 1000, 10000 and 25600 did not. Only a power cycle cleared it; both clears, `2302`, NMT
  Reset Node and Communication, `2201` changes, the `2500` toggle, an open-loop enable and `6040=0x0000` did not.
- In open loop (`2500=1`) the velocity enable refused at word 7 on `606C` nonzero and left the drive switched on
  and energized (`6041=8033`, `2100=0001`, no alarm). The stop refused it (stationary report), the clear refused it
  (no alarm). `6040=0x0000` released it (`8050`, `2100=0011`, trial 5991). Afterwards `606C` read a constant 1711
  while released, so every enable route refused the drive until a power cycle.
- The `6064` step at enable word 7 is a routine counter switch (I: command and encoder counters); it is not the
  cause and must not be used as a fault signature.
- No source documents a pulse-rate limit, this state, the 0.5 s delay or a recovery (D, by absence;
  [source audit](../reports/2026-11-13_enable_pull_sources.md)).

Boundaries: a guard refuses or releases; it never enables, retries, clears an alarm, resets the node or power-cycles
on its own. The pulse ceiling is a measured fact of one firmware with a project margin, never a documented limit.
Detecting the fault state is a report to the application, which owns the decision to stop using the axis and to
ask for a power cycle.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its velocity and position admission, enable
failure handling and fault reporting seams; RS is authoritative for common names. Inspect FieldCore-node read-only
for how its tasks report a device that needs a power cycle. If HEAD, dirty state and seam match the last recorded
row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with
path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read the S21 report,
the source audit, the afternoon section of the
[limits report](../reports/2026-11-13_velocity_limits_and_fault_recovery.md), `Velocity.h`/`.cpp`, `Position.h`/`.cpp`,
`Actions.h`/`.cpp` (enable), `Settings.h` (`SubdivisionSettings`), `DriverSettings.h` (the `2201` read) and
`AxisSession`. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. **Pulse-rate admission.** Velocity runs and position moves take the drive's `2201` as typed evidence from a fresh
   read in the same reservation (the `DRIVER` read prompt 59 added) and refuse, before any write, a rate whose
   `rpm × 2201 / 60` exceeds a named profile limit. The constant states the measured 2.56 MHz clean and 2.73 MHz FF05,
   the firmware it applies to and the margin as project policy (for example 2.5 MHz). The range probe may exceed it
   only as an attended experiment that says so. A changed `2201` (prompt 59's settings epoch) invalidates the
   evidence. Report the computed rate in the refusal detail.
2. **Word 7 guard.** When an enable (velocity, position, regular enable of prompt 57) refuses or fails after word 7
   was acknowledged and before word 15 was sent, the session follows at once with prompt 57's release from the
   switched-on state (`6040=0x0000`, measured once from `8033`), recorded as that operation's own write, with its
   execution evidence and uncertainty. It is not a stop and claims no standstill. On `606C` nonzero at word 7 the
   result carries a distinct detail ("speed reported at enable without a command"), which the application can
   count across attempts.
3. **Fault-state report.** A typed, documented application signal, not a library decision: after an FF05 whose run
   exceeded the pulse limit, or after a word 7 speed report, the axis result says "the drive may need a power cycle;
   verify with a short slow enable". The examples (`AxisSession`, `ProbeMotion`, console) print it and stop
   admitting motion on that axis until the operator acknowledges or the node reboots (a boot-up observed after a
   power cycle). No automatic retry or clear.
4. **Bench routes** for the measurements below, through the ordinary public path.

## Verify and exit

Host tests: the pulse-rate refusal at the boundary and just below, with `2201` 1000, 25600 and 51200; a stale or
missing `2201` refuses; the word 7 guard's release after a `606C` nonzero report and after a lost word 15 reply
(nothing more than the one `0x0000`); the fault-state signal never enables, clears or retries. Run the native,
firmware, stack and documentation gates and CI on the actual final commit.

Bench, under the applicable authorization, each step needing a user power cycle afterwards:

1. Whether `2500` persists without a save: write 1, power-cycle, read. (`2201` was measured volatile on 2026-11-13:
   node 2's unsaved 51200 read 1000 after a power cycle.)
2. In a reproduced fault state (one 3300 rpm run at 51200): a mode 1 enable, and the word 7 guard's `0x0000` within
   100 ms of word 7, recording whether the FF05 and the pull still occur.
3. The discriminator: 3000 rpm with a 500 ms ramp at 51200 (pulse limit versus the `250A` deviation threshold).
4. A short regression on both nodes.

## Documentation and delegation

Update the velocity, position and drive actions guides, the fault guide's enable-fault paragraph, the speed units
guide (pulse-rate arithmetic), coverage, backlog S21, CHANGELOG and Doxygen in the same stage. A stronger reviewer
challenges the guard's release timing and the fault-state signal's ownership; normal coding agents own disjoint tests.
The lead integrates and runs combined checks and owns the bench. Record `docs/reports/prompt_72.md`. Commit and sync on
the current branch before handoff.
