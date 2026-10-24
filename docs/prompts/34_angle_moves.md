# Prompt 34 Wrapped-angle motion integration

Apply [COMMON.md](COMMON.md). Prerequisites: [33](33_position_moves.md).

## Objective and boundaries

Complete `prepareMoveAngle` by composing existing exact axis/angle policy with the finite position lifecycle. This prompt owns angle-to-move integration, evidence retention and focused contrasting consumers. It does not create another motion executor, a second angle conversion implementation or production Nanotec/maxon drivers.

Follow [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Common angular intent remains usable by stepper and servo configurations with independent command/feedback scales and without mandatory full-step geometry.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS angle preparation, path/tie/limit tests and actual `prepareMoveAngle` callers; inspect I2C immutable-input/output-on-error conventions. Review FieldCore exact target snapshots read-only. Recheck the existing N5/EPOS4 contrast evidence and prompt-10 coordinate contract; no new vendor sequence may be inferred from a common fixture.

## Implement

1. Implement `prepareMoveAngle` through the established position context and `nextMove`/`advanceMove`. Reuse the common exact conversion functions. Every caller, including the future CLI, uses this same public path.
2. Require explicit angular frame, origin, path and half-turn tie policy, with the fresh multi-turn reference needed by the chosen semantics. Support only the agreed shortest/positive/negative paths. A selected path violating limits rejects; do not silently choose another turn.
3. Preserve requested angle, resolved exact displacement/target, chosen turn/path, quantization/error budget and reference/configuration generations. Native integer preparation remains exact where representable; human decimal display does not force a floating wire conversion.
4. Ensure reference, scale, mode or binding changes invalidate prepared angle work before admission. Apply the existing zero-distance rule without inventing physical standstill. Once prepared, reuse the same setup/trigger/acceptance/completion and uncertainty handling as absolute/relative moves.
5. Add small evidence-backed contrast fixtures exercising production common conversion code: N5 stepper/BLDC configurations without mandatory motor steps, command versus feedback counts and an explicitly checked float-native conversion where supported by the contrast evidence. Fixtures are not installed profiles and do not advertise second-driver support.
6. Add a native example with illustrative application-supplied increments-per-turn, gear ratio, origin, limits and rounding. Display effective target/error before admission. No manufacturer defaults or RS boot scale are guessed.

## Verify and exit

Test positive/negative wraps, multiple turns, exact half-turn ties, chosen-direction limits, coordinate frames, quantization boundaries, unavailable/stale reference and generation changes between preparation and admission. Verify output unchanged on failure and no motion writes from invalid preparation. Include zero-distance and integer precision above ordinary floating exactness.

Run prepared angles through the real finite move/owner fake path with stop and unknown trigger outcomes, proving conversion adds no alternate lifecycle. Check production common code against N5 and servo contrast inputs while keeping claims limited to contract tests. Exit with the complete angle API, examples and evidence semantics; physical motion qualification remains separate. Run focused angle/position/consumer/Doxygen checks.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document path/tie/frame choices and error budgets with Doxygen and examples. A normal coding agent can integrate settled conversion code; use independent stronger review for limit/path and stale-reference cases. Contrast tests can be delegated without changing production profile ownership.

Record results in `docs/reports/prompt_34.md`, update coverage and unit/angle guidance, then commit and sync on the recorded execution branch. Complete verified checkpoints may be synchronized earlier.
