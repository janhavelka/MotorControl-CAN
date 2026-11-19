# 06 — Finite profile velocity

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 04 and the unit work of 05. DS-D10 and DS-D32 belong here; DS-D16 applies to the ramp
words. MULTIDRIVE rows 35, 52, 59 and 73 apply. This stage does not broaden any physical authorization.

## Work

1. Documented sequence (DS2C p12; IBL60 p13; IBL80 p13; each manual's velocity section): write `6060`=3 and confirm
   `6061`=3; write `60FF`, `6083`, `6084`; then 0x0006, 0x0007, 0x000F, after which "the motor is enabled and starts
   running". The enable words are the start. `prepareVelocity` therefore admits only a drive that 04's `released`
   predicate accepts on a fresh report, in mode 3 or a mode it selects first through 04's mode select, after write
   preflight of every word it may send (canopen_shared 02). It owns the targets, the enable words, the run and the
   stop as one operation under the one operation deadline.
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
5. Run and stop. Require a positive finite `durationUs`; zero speed is a setting, not a run. The run counts from the
   owner's admission of the start word (word 15), the earliest moment motion can begin. At preparation, and again
   before the start word, the context checks with overflow-checked arithmetic that `now + durationUs` plus the
   caller's stop dispatch budget is at most the operation deadline; otherwise it sends no start word and reports the
   confirmed and uncertain effects of the setup writes. At run expiry the context yields the stop step of 04
   (`QUICK_STOP` under `DS-DEC-2`: no velocity-mode stop is documented, the only stop arrow is in the position-mode
   table, DS2C p11). If service resumes after the operation deadline, nothing further is sent: the result keeps the
   stop obligation, says whether a stop was never transmitted or possibly transmitted, and never extends the deadline
   or reports success. Changing `60FF` while enabled is undocumented: a zero target is neither a
   stop nor offered here, and speed changes stay UNSUPPORTED until [prompt 52](../52_motion_changes.md)'s vocabulary
   and DS2 evidence exist. Keep separate dispatch and wire deadlines and the shared never-transmitted-stop rule; never
   report success while the drive may still run.
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

Tests cover refusal from every report 04's `released` predicate rejects, the target-then-enable order, the range
edges 2147483647 and 2147483648 and negative targets rejected without a declaration (zero changing TX), undeclared
units and ramps, a lost enable word with retained stop obligation, run expiry yielding exactly one stop, cumulative
setup delay leaving less than `durationUs` plus the stop budget (no start word), equality at that cutoff, service
resumed after the operation deadline (nothing sent, stop obligation kept), a lost stop, a priority stop from another
producer, deadline cutoff `now >= deadlineUs`, cancel that is not a stop and no repeat of any start. Tests drive the production sequence through the owner and fake server. Quick and
full verification and required CI on the exact final pushed commit pass. Hardware NOT TESTED; never substitute an
unbounded run for a finite test. Commit, push and hand off.
