# Status and exact units

The installed [Units.h](../include/MotorControlCAN/Units.h) and
[implementation](../src/Units.cpp) provide pure C++11 arithmetic. They perform no
I/O, allocation, clock access, configuration mutation or motion. Calls borrow
inputs only for the call, and publish complete outputs only on success. Output
may alias an input; failures preserve every output. Independent calls are
reentrant; callers serialize writes to shared outputs.

## Call results

`Status` retains the RS shape: `Err code`, signed `int32_t detail`, and borrowed
static-lifetime `const char* msg`. Default construction and free `Ok()` mean
local call success. `isOk()` and explicit boolean conversion inspect that result.
`errToString()` returns stable static labels, including `UNKNOWN` for unrecognized
enum values. Diagnostic strings are never decision keys. There are no sensor
factory aliases, RTU CRC/exception codes or application health states.

`PROTOCOL_ERROR` means a checked native service rejection. A full CANopen abort
stays in separate `uint32_t abortCode` evidence with its validity flag, including
the high bit; it never passes through signed `detail`. The retained ownership
proof tests upload/download vendor aborts separately from call status, drive
faults, service completion and physical execution uncertainty.

Units functions use `UnitError` in `detail`. Invalid configuration/scales return
`INVALID_CONFIG`; invalid numeric input, storage overflow and quantization
failure return `ILLEGAL_VALUE`; an exact nonzero radians conversion needing
approximation returns `UNSUPPORTED`. If several arguments are invalid, validation
stops at the first documented boundary in the function: scale configuration and
spatial units, then time units, then numeric conversion. Quantizers validate
denominators and allowance before rounding. A successful call proves only the
specified calculation.

## Exact quantities and scales

`Rational` stores an I64 numerator and positive U64 denominator. Its constructor
preserves supplied components; `reduceRational` normalizes them. Zero becomes
`0/1`. Zero denominator is invalid. Signed magnitude arithmetic handles
`INT64_MIN` without signed overflow. Every dimensional numerator/denominator
factor cancels before checked multiplication. The final reduced numerator must
fit I64 and denominator U64; otherwise the call fails explicitly. There is no
fallback through floating point or hidden geometry.

`parseExactNumber` accepts strict signed ASCII integer, decimal or fraction text
of at most 127 characters. It rejects whitespace, exponent notation, `.5`, `1.`,
signed denominators, NaN and infinity. Decimal trailing zeros are removed before
accumulation. Each unsigned component must fit U64 before reduction; reduction
then determines signed representability. Thus `18446744073709551615/18446744073709551615`
is exactly one. Parser capacity is an explicit limit, not silent truncation.

`rationalToDouble` is an **explicit approximation**, rounded toward zero to the
closest binary64 value. It is useful for display and conservative policy
allowances. Exact integers, including adjacent values around `2^53`, remain in
`Rational` throughout exact conversion and integer quantization. They are never
encoded by converting them to `double`.

| Quantity | Interpretation and required facts |
| --- | --- |
| `STEPS` | Command increments, following `commandPolarity`; not necessarily full motor steps |
| `FULL_STEPS` | Optional stepper geometry from `fullStepsPerMotorTurn` |
| `ENCODER_COUNTS` | Independent selected feedback scale, basis, nonzero source identity and polarity |
| `TURNS`, `DEGREES` | Load-axis angles; exactly 360 degrees per turn, with no wrapping/origin |
| `RADIANS` | Load-axis angle; nonzero conversion across units requires explicit approximation |
| `MILLIMETRES` | Configured travel per load turn |

Known scales have positive components and explicit `ScaleSource`. Missing
optional scales are `0/1, UNKNOWN`; malformed optional facts are rejected even
when unrelated. Only needed scales must be known. Command-to-feedback conversion
on the same motor basis does not need gearing. Angle-to-command conversion
needs command scale and motor/load ratio. No full-step scale is required for a
servo. Feedback may be per motor turn, load turn or millimetre, independently of
command representation. Provenance describes supplied evidence, not active drive
configuration. `ASSUMED` is usable only because the caller supplied that fact.

Time units are exact seconds, minutes and milliseconds. Acceleration has two
separate time denominators: `60 turns/minute/second` and
`3600 turns/minute/minute` both equal `1 turn/second/second`.
No Lichuan millisecond ramp duration becomes dimensional acceleration.

## Quantization and approximation

`quantizeInteger` selects `EXACT`, `NEAREST`, `TOWARD_ZERO`, `FLOOR` or `CEIL`.
Nearest uses ties to even for either sign: `-1/2` becomes zero, `-3/2` becomes
minus two, and `-5/2` becomes minus two. Comparisons never double a potentially
overflowing remainder. The output retains the effective I64 and exact reduced
signed error magnitude, where error means effective minus requested. Every
policy enforces a nonnegative exact native-unit tolerance; EXACT rejects any
fraction regardless of tolerance.

`quantizeFloat` explicitly targets IEEE binary32, nearest with ties to even.
Exact midpoint comparisons avoid double rounding. The exact error is compared
to the supplied rational tolerance using fixed stack storage; a reported
binary64 absolute-error bound rounds upward. Zero tolerance accepts only an
exact native float. For example, `16777217` maps to `16777216` with error one,
and `INT64_MAX` maps to `2^63` with error one. Even the tiny error from
`1/UINT64_MAX` mapping near `2^-64` stays nonzero. No profile may treat these as
unchanged exact requests. A later profile owns its native format and tolerance.

The explicit approximate conversion overloads consume a positive finite
destination-unit error allowance and return `UnitConversion` with a conservative
arithmetic/pi error bound. They require the default IEEE round-to-nearest
environment, do not change that environment, and do not promise exact
representation. Nonfinite/underflowing results or exceeded allowances reject.
Bounds exclude uncertainty in caller scales. Approximation never supplies an
origin, coordinate reference or angle-path choice; prompt 10 owns that policy.

## Reuse, differences and verification

The arithmetic adapts Jan Havelka's MIT MotorControl-RS `Units.cpp`,
`UnitFactors.h` and reusable `Axis.cpp` helpers at revision
`bd831c859daa9c8c07b8d27a2dd879bb4e750511`; attribution remains in source and
the software license. RS is not a dependency. CAN keeps exact overloads as its
ordinary dimensional API instead of RS's engineering-double path. Unit settings,
origin/reference and frame policy remain outside this module. CAN also corrects
RS's rare extra-ULP underestimate in `rationalToDouble`: bounded comparisons
check both neighboring directions to establish the closest directed value.

The [retained proof](../experiments/api_ownership/README.md) uses production
types/conversion/quantization. Its old million-unit input and `10^18` product
limits are removed. Proof-only I32 bounds, EXACT-only preparation and origin/frame
rules remain until prompt 10. Its numeric errors use
`MoveError::UNIT_ERROR_BASE + UnitError` without changing the underlying Err/msg.
The old proof spellings `NATIVE_INCREMENTS`, `commandIncrementsPerMotorTurn` and
`feedbackCountsPerMotorTurn` are replaced by `STEPS`,
`commandStepsPerMotorTurn` and selected `encoder.countsPerUnit` respectively.
No competing aliases are retained.

Independent vectors in [units_test.cpp](../test/units_test.cpp) cover signs,
extremes, cancellation, malformed text, failure sentinels, scale dependencies,
all rounding policies and floating midpoint/tolerance cases. The compiled
[isolated consumer](../test/installed_consumer/main.cpp) uses the same public
path under C++11 and C++17/no RTTI. Run the registered `units` test or the
[normal verification modes](verification.md); full mode also proves package
inclusion, standalone headers and strict Doxygen.
