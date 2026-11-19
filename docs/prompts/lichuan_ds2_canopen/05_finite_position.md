# 05 — Units and finite profile-position moves

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 04. DS-D09, DS-D16 and DS-D33 belong here; DS-D34 applies to the scale and DS-D04
decides completion evidence per dialect. MULTIDRIVE rows 9, 10, 33, 34, 52 and 60 apply.

## Work

1. Native units. Position objects are "pul" (`6064`, `6065`, `6067`, `607A`, `607C`; DS2C p27-28): command increments,
   with feedback `6064` claimed in the same unit. `6091:01/02` electronic gear (default 131072/10000; DS2C p28;
   DS2EDS names them MotorRevolutions/ShaftRevolutions; `:02` type unprinted versus I32, DS-D34) suggests 10000 pul
   per turn of a 17-bit encoder; that relation is an inference and fails for a 23-bit motor. Common-unit conversion uses `UnitConfig` with a caller-supplied,
   generation-bound scale; never a default from `6091` defaults. A `6091` readback that differs from the scale's
   recorded value invalidates the scale and origin generations.
2. Rate unit. `6081` is pul/s with default 10000 (DS2C p28), while DS2EDS names it "posmode speed" with default 60
   and the DS2C p11 figure uses rpm and ms (DS-D16, DS-D33; stage 01 recorded the `6081` default pair as an intake
   correction). `NativeRateBasis::COMMAND_INCREMENTS` per second therefore needs a caller declaration of the
   rate unit for the binding; without it common speed preparation rejects before TX and only the native-value path
   remains.
3. Ramp unit (DS-D16). `6083`/`6084` are pul/s² in DS2C p10/p28 and "accel/decel time" with default 100 in DS2EDS
   l.1407/l.1416 and DS2OP p6-7. Offer two ramp policies: `VERIFIED_CONFIGURED` (read the current words, write none,
   record their raw values with "unit unresolved") and `WRITE_NATIVE` with a caller-declared `RampKind` (`TIME_MS` or
   `ACCELERATION` in pul/s²) bound to the binding. `convertNativeRamp` never crosses dimensions. Without a declaration
   common acceleration requests reject before TX.
4. Target encoding (DS-D09). `607A` is U32 default 50000 in the object table (DS2C p27), I32 in the mode table (DS2C
   p10) and DS2EDS l.1333, and its printed range does not fit I32. Check the prepared native wire value: a negative
   `607A` encoding rejects before TX unless a caller declaration states signed targets are qualified for this
   binding. Absolute moves to smaller nonnegative targets and nonnegative relative displacements need no declaration.
   Record that an unqualified U32 reading of a negative word is a move of about 4.29e9 pul.
5. Sequence (DS2C p11). Preconditions: fresh 111 in mode 1 from 04, no other operation on the axis. Write `607A`,
   `6081` and the ramp words of the chosen policy, each confirmed; then the trigger word 31 (bit 4 rising, bit 5 0,
   bit 6 0) for absolute or 95 (bit 6 1) for relative, from a confirmed 15. Words 63 and 127 (bit 5, change set
   immediately) are a change operation and stay UNSUPPORTED here; [prompt 52](../52_motion_changes.md)'s per-profile
   change vocabulary owns them later. Never raise bit 4 again for a lost trigger.
6. Acknowledgement and rearm. In `DS2C_TABLE` bit 12 "target position effective" is documented, unqualified
   acknowledgement evidence; a stale high bit 12 is not new evidence. `IBL60_TABLE` has no bit 12, so the trigger's
   SDO confirmation is the only acknowledgement. Rearm by writing 15 after acknowledgement or a terminal, as a
   recorded `DS-DEC-5`; DEC-8's "hold bit 4 until terminal" does not transfer.
7. Completion. `DS2C_TABLE`: bit 15 "positioning complete" must be low after the trigger and then rise; bit 10 means
   speed zero, not arrival. `IBL60_TABLE`: bit 10 in mode 1 means positioning complete, with the same fall-then-rise
   rule. A bit already high never completes a move; a short move whose fall the polls missed ends with completion
   `NOT_OBSERVED`, never a failure claim or a re-trigger. Common target-arrival completion also compares fresh `6064`
   with the endpoint; `6067`/`6068` windows (500 pul, 100 ms defaults) are drive settings read live, not host
   tolerances.
8. Relative basis. Whether bit 6 adds to the previous target or the actual position is undocumented. Common relative
   moves need a caller declaration of the verified basis; otherwise `MoveError` rejects before TX and the native
   relative intent stays available as acknowledged-only. Angle moves compose `preparePosition` through
   `prepareMoveAngle` once the scale is declared.
9. Limits. Do not rely on `607D` or `607F` to bound a move until 07 qualifies them; host limits come from `AxisConfig`.

## Deliver

`Position.h`/`Position.cpp` with `MoveContext` and the DS2 native move record; `test/lichuan_ds2_position_test.cpp`
and `test/lichuan_ds2_units_test.cpp`; owner-backed `ds2 move abs|rel` routes with API/CLI parity; `DS-DEC-5`;
coverage, operation map, `docs/position_parameters.md` and `docs/speed_units.md` DS2 sections, Doxygen, CHANGELOG.

## Acceptance

Tests cover exact quantization at range edges, negative native targets rejected without declaration, smaller
nonnegative absolute targets accepted, each ramp policy and an undeclared ramp request rejected, scale invalidation
on a changed `6091`, the write and trigger order, a lost trigger with no re-trigger, stale bits 12 and 15, both
dialects' completion rules, a stop during the move and a retained terminal. No hidden restoration or replay can
restart motion. Quick and full verification and required CI on the exact final pushed commit pass. Hardware NOT
TESTED. Commit, push and hand off with the declarations listed.
