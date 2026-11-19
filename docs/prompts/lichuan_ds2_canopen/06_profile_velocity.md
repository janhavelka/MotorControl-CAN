# 06 — Finite profile velocity

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 04 and the unit work of 05. DS-D10 and DS-D32 belong here; DS-D16 applies to the ramp
words. MULTIDRIVE rows 35, 52, 59 and 73 apply. This stage does not broaden any physical authorization.

## Work

1. Documented sequence (DS2C p12; IBL60 p13; IBL80 p13; each manual's velocity section): write `6060`=3 and confirm
   `6061`=3; write `60FF`, `6083`, `6084`; then 0x0006, 0x0007, 0x000F, after which "the motor is enabled and starts
   running". The enable words are the start. `prepareVelocity` therefore admits only a drive that 04's `released`
   predicate accepts on a fresh report, in mode 3 or a mode it selects first through 04's mode select, after write
   preflight of every word it may send (canopen_shared 02). It owns the targets, the enable words and the run under
   the one operation deadline; the run's stop is the application's separate urgent stop (item 5).
2. Target encoding (DS-D10). `60FF` is I32 "maximum speed in velocity mode" default 0 (DS2C p29), U32 0..2147483648 in
   the mode table (DS2C p12), a signed range with type U32 in DS2OP p6, IBL60 p13 and ISL80 p14, and I32 default 120
   in DS2EDS. As for `607A` (05), the unqualified range is 0..2147483647, checked on the exact quantized value before
   narrowing; 2147483648 rejects. A negative `60FF` encoding rejects before TX unless a caller declaration states
   signed velocity, in a range it names, is qualified for the binding; `607E` polarity has no documented bit meaning
   (DS2C p28) and `P02-02` direction is unreachable, so neither provides reverse motion.
3. Units. `60FF` is pul/s in the manual; DS2EDS default 120 and the p11 figure leave it unqualified (DS-D16). Use 05's
   rate-unit declaration; without it only the native-value path exists. `VelocityError::RATE_BASIS` is documented as
   "not integer motor turns per minute" from Lichuan C; a pul/s destination needs that common comment and its tests
   reviewed with [prompt 60](../60_user_units.md)'s owner, as a core change, not a profile shortcut.
4. Ramps use 05's policies (`VERIFIED_CONFIGURED` or a declared `WRITE_NATIVE`); a requested physical acceleration
   without a declaration rejects before TX.
5. Run and stop. Require a positive finite `durationUs`; zero speed is a setting, not a run. The run follows
   canopen_shared 02 "Operation admission", timed-runs bullet, which reuses Lichuan C's finite-velocity design
   (`VelocityOperation.h` `durationUs`; `profiles/lichuan_c/Velocity.h` `requiredStopBudgetUs`, `stopDueUs`,
   `needsStop`, phase `STOPPING`): the duration runs from successful preparation and includes setup and queue
   delay; preparation refuses, with overflow-checked arithmetic, unless the duration plus the caller's stop budget
   fits before the operation deadline; every later transfer of the run, enable words and observations included, has
   `deadlineUs` at most the stop due time, and no enable word is yielded at or after it. So nothing of the run holds
   the channel at stop due, and 04's "an in-flight request settles first" waits for none of it. At stop due the
   context issues no further work and the application composes 04's `QUICK_STOP` as its separate urgent stop with
   its own deadline, which the run's deadline, identity outcome and a missing acknowledgement never bar. DS2 adds:
   no velocity-mode stop is documented (the only stop arrow is in the position-mode table, DS2C p11), so the run is
   refused before its first changing TX, mode select included, unless `DS-DEC-2` (04 item 6) has a mode-3 row for
   every state the run may leave (an acknowledged 0x000F and each uncertain enable word); neither a valid encoding
   nor a fake server accepting 0x000B clears that gate. On an unresolved route the best-effort word exists only
   after 04's S3 opt-in (item 7 there); before it `admitBestEffortStop` returns `INVALID` with no TX. `needsStop`
   survives cancellation, failure, recovery, a lost start reply and acknowledgement-only completion while any enable
   word may have left; the result says whether a stop was never or possibly transmitted and never reports success
   while the drive may still run. Changing `60FF` while enabled is undocumented: a zero target is neither a stop nor
   offered here, and speed changes stay UNSUPPORTED until [prompt 52](../52_motion_changes.md)'s vocabulary and DS2
   evidence exist.
6. Uncertainty. A lost enable word leaves start execution `UNKNOWN` and a stop obligation; a lost stop leaves
   `needsStop` with execution unknown. Never repeat the enable or a transmitted stop automatically. Local cancel does
   not stop the motor.
7. Host loss (DS-D32). No consumer heartbeat, guarding, `6007` or `1029` exists, so `durationUs` is host intent only;
   the API, coverage and the console help say so. A wired stop (FunIN.34, DS2C p9, assigned by the vendor tool) is the
   application's, per [prompt 62](../62_stop_line.md); the library cannot verify that assignment.
8. Speed limits. `607F` maximum speed limit (U32 pul/s, default 2147483648; DS2C p28) may or may not clamp mode 3;
   read it, never rely on it, and never write it as an incidental setup step. The motor tables' 3000 rpm rated and
   6000 rpm maximum (`P00-14`/`P00-15`, DS2C p14-15) are motor ranges, not drive limits; `P0A-08` overspeed (Er.500)
   is unreachable. Host maxima come from `AxisConfig`; a drive's accepted range, clamp behavior and direction rule are
   measured facts per model and firmware (MULTIDRIVE row 59), recorded by 12a, never assumed from Lichuan C's ladder.
9. Observation. Fresh `606C` (pul/s) and the dialect's documented bits are drive reports. `DS2C_TABLE` bit 10 "speed
   zero" after the stop is the documented post-stop evidence; `IBL60_TABLE` has none. No completion is `OBSERVED`
   without the 04 rule, and a speed report is not a measured shaft speed. Observation cadence is the caller's (SDO
   reads now, TPDOs only after 09's live maps and an explicit NMT start); the context never samples time itself.

## Deliver

`Velocity.h`/`Velocity.cpp` with `VelocityContext`; `test/lichuan_ds2_velocity_test.cpp`; owner-backed `ds2 velocity`
with API/CLI parity and lost-start, lost-poll and lost-stop cases; `docs/velocity.md` DS2 section, coverage, operation
map, Doxygen, CHANGELOG; the `RATE_BASIS` core review recorded in the report (landed change or explicit blocker).

## Acceptance

Tests cover refusal from every report 04's `released` predicate rejects, the target-then-enable order, the range edges
2147483647 and 2147483648 and negative targets rejected without a declaration (zero changing TX), undeclared units and
ramps, a lost enable word with retained stop obligation, and the timed-run cases of canopen_shared 02 through the real
owner: a duration that no longer fits with the stop budget (refused at preparation), setup delay reaching stop due (no
enable word), an observation in flight near stop due (its deadline ends there), `now` equal to stop due (no run
transfer, the urgent stop composed), a lost start reply (`needsStop` kept), service resuming after stop due (no run
work, the separate stop still composed) and a queue-pressured stop. Also a run refused with zero TX while `DS-DEC-2`
has no mode-3 row, an uncertain start on an unresolved route (no best-effort TX before 04's opt-in, `needsStop` kept),
a lost stop, a priority stop from another producer, deadline cutoff `now >= deadlineUs`, cancel that is not a stop and
no repeat of any start. Tests drive the production sequence through the owner and fake server. Quick and full
verification and required CI on the exact final pushed commit pass. Hardware NOT TESTED; never substitute an unbounded
run for a finite test. Commit, push and hand off.
