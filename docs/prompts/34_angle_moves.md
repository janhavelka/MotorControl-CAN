# Prompt 34 Wrapped-angle motion integration

A wrapped request now follows the relative-basis policy (packet 9, A-05): the default `supportedRelativeBases = 0` rejects it, so set the policy explicitly.

Apply [COMMON.md](COMMON.md). Prerequisites: [33](33_position_moves.md).

## Objective and boundaries

Complete `prepareMoveAngle` by composing existing exact axis/angle policy with the finite position lifecycle. This prompt owns angle-to-move integration, evidence retention and focused contrasting consumers. It does not create another motion executor, a second angle conversion implementation or production Nanotec/maxon drivers.

Follow [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Common angular intent remains usable by stepper and servo configurations with independent command/feedback scales and without mandatory full-step geometry.

## Read and audit

Refresh MotorControl-RS HEAD and dirty state and read the RS code for this seam; RS is authoritative. Read RS angle preparation, path/tie/limit tests and actual `prepareMoveAngle` callers. Output-on-failure authority is RS `Units.h` and the `prepareMoveAngle` tests; the I2C libraries differ and set no CAN rule. Inspect current FieldCore-node read-only for ownership or integration changes; FieldCore has no angle or velocity operation, so compare identity and admission only. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread; a sibling row must name a behavior adopted or rejected, with path and symbol. Recheck the existing N5/EPOS4 contrast evidence and prompt-10 coordinate contract; no new vendor sequence may be inferred from a common fixture.

Evidence route: prompt 29 conditional experiment or manufacturer clarification; do not wait for prompt 47. Processing-time cutoff follows [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence): `now >= deadlineUs` expires, even if the frame was received earlier.

### Concrete evidence to recheck

Use [position/scale evidence](../audit/LICHUAN_MOTION_FACTS.md). Keep angle conversion dependent on a confirmed command-unit scale and reference generation; do not substitute 4000 encoder counts/revolution for a confirmed command scale because the product page mentions a 1000-line encoder; `2201` is a candidate scale and needs Q04 evidence, since no page states pulses per revolution equals `2201` and the EDS default is 0. Capture raw `2200` (the only direction-named object) in the configuration generation. Its effect on CANopen direction is undocumented; direction sign relative to `2200` and the shaft is measured evidence (Q04), and a change invalidates prepared work and the reference. About ±1,000,000 pulses is roughly ±19.5 revolutions at 51200 per turn, so multi-turn absolute targets can be unrepresentable and must reject. A software coordinate transform cannot resolve an unestablished actual-position scale. Retain native rounding/error and limit checks without adding a Lichuan-only assumption to common angle APIs.

## Implement

1. Implement `prepareMoveAngle` through the established position context and `nextMove`/`advanceMove`. Reuse the common exact conversion functions. Every caller, including the future CLI, uses this same public path.
2. Require explicit angular frame, origin, path and half-turn tie policy, with the fresh multi-turn reference needed by the chosen semantics. Support only the agreed shortest/positive/negative paths. A selected path violating limits rejects; do not silently choose another turn.
3. Preserve requested angle, resolved exact displacement/target, chosen turn/path, quantization/error budget and reference/configuration generations. Exact paths are recoverable, so no field is added for them; if approximate radians need the native direction, add `nativeDirectionPositive` (native sign, meaningless at zero distance) instead of reusing `pathPositive`, which misleads under polarity -1. Native integer preparation remains exact where representable; human decimal display does not force a floating wire conversion.
4. Ensure reference, scale, mode or binding changes invalidate prepared angle work before admission. Move preparation rejects zero effective displacement with no writes; pure arithmetic may report it, and an "already at target" report is application policy. Do not invent physical standstill. Use `validatePreparedTarget` at admission. The move's own parameter writes (`607A`, `6081`, `6083`, `6084`) are phases of the move: they do not go through `prepareDriverSettings` and do not advance the configuration generation, which would otherwise make the prepared angle stale. They do retire retained `MOTION_PARAMETERS` observations through `retireMotionParameters`, as prompt 33 states. Once prepared, reuse the same setup/trigger/acceptance/completion and uncertainty handling as absolute/relative moves.
5. Extend `test/multidrive_contract_test.cpp` case C1 with one wrapped-angle vector on the N5-documented scale (3600 per turn at 1:1 only, because N5 degrees include gear ratio `6091h`) and an abstract non-step servo scale, using production `preparePosition`. No new fixture family; fixtures are not installed profiles and do not advertise second-driver support. A prepared `BINARY32` angle target is rejected by the I32 finite move path before admission.
6. Add a native example with illustrative application-supplied increments-per-turn, gear ratio, origin, limits and rounding. Display effective target/error before admission. No manufacturer defaults or RS boot scale are guessed.

## Verify and exit

Test positive/negative wraps, multiple turns, exact half-turn ties, chosen-direction limits, coordinate frames, quantization boundaries, unavailable/stale reference and generation changes between preparation and admission. Verify output unchanged on failure and no motion writes from invalid preparation. Include zero-distance and integer precision above ordinary floating exactness.

A new header or source needs `CMakeLists.txt`, `test/package_test.py`, `scripts/check_repository.py` and `test/repository_test.py` updates. Run prepared angles through the real finite move/owner fake path with stop and unknown trigger outcomes, proving conversion adds no alternate lifecycle. Check production common code against N5 and servo contrast inputs while keeping claims limited to contract tests. Exit with the complete angle API, examples and evidence semantics; physical motion qualification remains separate. Run focused angle/position/consumer/Doxygen checks.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document path/tie/frame choices and error budgets with Doxygen and examples. A normal coding agent can integrate settled conversion code; use independent stronger review for limit/path and stale-reference cases. Contrast tests can be delegated without changing production profile ownership.

Record results in `docs/reports/prompt_34.md`, update coverage and unit/angle guidance, then commit and sync on the recorded execution branch. Complete verified checkpoints may be synchronized earlier.
