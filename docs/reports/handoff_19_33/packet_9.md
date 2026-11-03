# Packet 9: native rate basis and coordinate corrections

Executes audit W8's A-01, A-05, A-09, A-13 and DEC-6, and reopens prompt 10. Arithmetic only: no velocity, homing, wrapped motion or finite lifecycle here, and no drive evidence of any rate.

## Delivered

See the [axis contract](../../axis_contract.md) and the [changelog](../../../CHANGELOG.md).

- **A-13**, committed separately as `7f76dcd`: `mechanicalUnits` and `finishPrepared`. A differential over 400,000 vectors matched HEAD in status, detail and outputs; only one message's wording merged.
- **A-01.** `NativeRateBasis`. With 3,200 increments per turn and gear 5, 1 load turn/min gives 5 under `MOTOR_TURNS` and `MINUTE`, and 16,000 under `COMMAND_INCREMENTS`. 3,200 steps/min gives 1 rpm at 1:1 gear. Quantized once, error in the destination unit.
- **A-05.** Wrapped requests follow the relative-basis policy.
- **DEC-6.** Inward soft-limit recovery with a valid witness.

## Decisions

- **Native-frame `STEPS` under `MOTOR_TURNS` rejects**, so `NATIVE` always means the destination itself. The packet lists native motor turns and steps separately, A-01 allows native `TURNS`, and RS also rejects it. `VelocityRequest::frame` defaults to `NATIVE`, so a steps request needs `MOTOR` or `LOAD`.
- **Absolute unwrapped requests with a witness do not consult the relative-basis policy.**

## Independent review

A reviewer who had not read the implementer's tests derived expectations with Python `Fraction` and fuzzed the real code. **It found no arithmetic, limit, policy, undefined-behavior or atomicity defect**, over 1.2M rate and 1.8M position cases with a separate 300k `MOTOR_TURNS` position run, built with UBSan in trap mode, and output bytes unchanged on every rejection. CAN `MOTOR_TURNS`/`MINUTE` matched compiled RS in 100k cases, with no RS-only accept: CAN-only accepts are zero or above int16.

It did find defects in the words, all fixed here:

| Finding | Fix |
| --- | --- |
| "Until the basis is rpm" in prompts 33, 36 and 42 allows a rate wrong by 60: no value means rpm, and `nativeVelocityTime` defaults to `SECOND` | They now require `MOTOR_TURNS`, `MINUTE` and integer representation, plus a negative test |
| The contract said a witness adds no limit to an absolute request, but a witness outside native limits rejects | Sentence corrected; the code follows DEC-6 |
| `Axis.h` comments on `NATIVE` and `LIMIT` were stale | Corrected |
| Prompt 34 did not say wrapped moves need the policy set | Added |

## Open

- **No runtime guard.** Nothing in the library stops a Lichuan caller writing a `COMMAND_INCREMENTS` value to an rpm object: 267 is inside `0..3000`, so a range check will not catch it. Prompts 33, 35, 36 and 42 carry the rule and the negative test; no such caller exists yet.
- `validatePreparedTarget` covers positions only, so prompt 35 must compare a prepared rate's generation itself. Noted there.
- `preparePositionParameters` takes raw `positionSpeedRpm`, so `6081` does not pass through `prepareVelocityTarget`. Packet 12 and prompt 33 should weigh that.
- A nonzero request can round to zero, where RS rejects it. Prompts 35 and 36 require the rejection.

## RS deltas, recorded locally

No message to maintainers. All checked against RS `e59c5ca`, and listed in the [axis contract](../../axis_contract.md).

| Point | RS | CAN |
| --- | --- | --- |
| A-05 | `src/Axis.cpp:502` applies the policy to relative only | Also wrapped |
| DEC-6 | `:508` rejects every witness outside soft limits | Inward recovery |
| Native rate | `:609-611` fixes motor rpm | Selects and records the destination |
| Range, zero | `:647-655` int16, rejects zero | Wide, zero is arithmetic |
| Allowances | binary64 | `Rational` |
| Witness | `:505` drops an unknown one | Always checks a supplied one |

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `e59c5ca` | Clean | Units, Axis, `test/velocity_test.cpp` | Common intent vectors mirrored; deltas above |
| FieldCore-node | `5ad9292` | Clean | `Rs485MotorModule.cpp:164-170` exact targets | Unchanged; read-only. Keeps position increments separate from native rpm, as here |
| ADS1115, INA228 | Unchanged since packet 5 | Clean | None for this seam | No behavior adopted |

## Checks

See the [execution table](README.md).
