> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 9: Native rate basis and coordinate corrections

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 8.
This is W8's A-01/A-05/A-09/A-13 and DEC-6 arithmetic scope. It does not implement
velocity, homing, wrapped motion or a finite position lifecycle.

Read actual `include/MotorControlCAN/{Units,Axis}.h`, `src/{Units,Axis}.cpp`,
`test/{units,axis,position_parameters}_test.cpp`, `profiles/lichuan_c/Position`
and `docs/axis_contract.md`. Compare RS Units/Axis, position/velocity callers and
tests; inspect FieldCore exact target snapshots read-only. Retain the N5/EPOS4
and non-CANopen contrast fixtures. Do not impose full-step geometry on servos.

1. Reproduce the native rate error: with 3,200 command increments/motor turn and
   gear 5, one load turn/minute requires5 motor rpm, not16,000 command increments
   written to a Lichuan rpm object. Add the reviewed native-rate basis to AxisConfig
   and make the conversion destination explicit. Test mechanical turns, native
   motor turns, steps, polarity, gear, approximate radians and integer/float
   representation. A3,200 steps/min request must become1 rpm at 1:1 gear. Quantize
   once and report error in the actual destination unit. Position NATIVE semantics
   must remain unchanged. Define units in public comments; no magic1/1 scale
   patch that makes one source type work and another wrong.
2. Apply the relative-basis policy to wrapped requests as well as relative ones.
   A queued witness must reject under an ACTUAL-only policy. Keep exact wrapped
   path/rounding behavior and existing approximation bounds. Do not add A-14's
   speculative field here; its later approximate-angle need belongs to34.
3. Apply DEC-6 inward soft-limit recovery only with a valid native-known witness:
   an outside witness may reach an inside endpoint toward the permitted range.
   Both requested and quantized endpoints remain checked. Native out-of-range,
   stale/unknown references, still-outside endpoint or outward travel reject.
   Test absolute/relative and polarity boundaries, not only 1005→995 in 0..1000.
4. Extract the repeated mechanical-unit and prepared-endpoint tail logic only
   where A-13 identifies it. Separate mechanical refactoring from behavior fixes
   in reviewed commits. Retain exact arithmetic and output atomicity; no second
   conversion engine or generic trajectory abstraction.

Delegate independent exact-value vectors and source/caller review; one writer
owns Units/Axis. Include overflow, nonfinite/zero-denominator, signed extremes,
rounding crossing a boundary, fresh-reference and config-generation tests.
Run units/axis/native-parameter and contrast tests, installed C++11 consumers,
full verification and strict Doxygen. Compare RS common intent vectors without
copying ESS frames or native limits. Record deliberate RS differences locally;
do not send messages to maintainers or edit that repository. Update original 10,
33's prerequisite contract and packet 9 report, then required CI/commit/sync.
