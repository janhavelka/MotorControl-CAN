> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 04 Implement status, units and exact axis semantics

Apply [COMMON.md](COMMON.md). Prerequisite: stage 03.

## Audit

Read current RS Status.h, Units.h/cpp, Axis.h/cpp, ReadOperation/MoveOperation/VelocityOperation and their boundary tests. Compare ADS1115/INA228 status conventions, MB85RC overflow/result policy and FieldCore exact motor target/config generation handling. The baseline snapshots are reference code, not files to compile into this library unchanged.

## Deliver

1. Implement shared Status/Err/Ok/isOk/errToString style with static messages, documented numeric detail and separate lossless raw CANopen error evidence. Keep codec validity, drive fault, operation outcome and application health separate.
2. Port or adapt proven exact arithmetic with attribution: integer/rational parsing, displacement/rate/acceleration conversions, steps, encoder counts, turns, degrees, radians with explicit approximation, configured linear travel, motor/load coordinates, origin and gearing. No hidden default 1000 counts/turn.
3. Implement `validateAxisConfig`, `configureAxis`, `setAxisOrigin`, `invalidateAxisReference`, `preparePosition` and `prepareVelocityTarget` with explicit generation/reference/freshness policy. Exact native integers never pass through binary floating point. Quantization and inclusive soft/native limits are explicit; overflow/unknown scale/unsupported relative basis fail before work is yielded.
4. Preserve `prepareMoveAbsolute/Relative/Angle` intent vocabulary for subsequent stages, while keeping wire-specific widths out of common math. Do not copy ESS int16 native RPM, speed bounds or ramp assumptions. Separate Lichuan millisecond ramp-time parameters from dimensional acceleration requests.
5. Define evidence invalidation for node boot-up, binding/dialect/mapping/scale changes, uncertain movement, counter reset and release. Age expiration is an explicit policy and independent from mandatory new-event completion checks.
6. Add offline `units_preview` and a consumer showing exact targets with supplied configuration. It sends nothing and creates no device lifecycle.
7. Pass the multi-drive contrast cases: generic signed rate instead of RPM-width leakage, servo config without full-step geometry, independent command/encoder/load scales, logical subaxis binding, and checked conversion to a documented float-native representation. Do not equate effort/current limits with torque modes or encode mandatory CANopen fields in common axis types.

## Concrete implementation contract

The public result form follows RS, not the differing I2C helpers:

```cpp
namespace MotorControlCAN {
struct Status {
    Err code;
    int32_t detail;
    const char* msg;
    constexpr bool isOk() const;
    constexpr explicit operator bool() const;
};
constexpr Status Ok();
const char* errToString(Err code);
}
```

This lists required names, not a complete header. Use the exact selected enum/error/evidence policy in [CONVENTIONS.md](CONVENTIONS.md); add constructors and definitions with correct C++11 constexpr rules. Do not add parallel `ok()`, `Status::Ok()`, `success()` aliases or a generic error enum member for every owner outcome. `msg` has static lifetime. Preserve a raw SDO abort in a separate `uint32_t`, including values whose high bit is set.

Keep `parseExactNumber`, `rationalToDouble`, `convertDisplacement`, `convertVelocity`, `convertAcceleration` and the existing axis function names where their semantics match. Review both declarations and `Axis.cpp`: removing `nativeRpm` from a type is insufficient if preparation still checks an ESS signed-16-bit RPM range.

Document each numeric field's unit, coordinate frame, signedness, valid range and source. A servo axis may have command increments and feedback counts but no full-step/subdivision data. Unknown geometry is not zero. A common velocity target is a signed dimensional rate; each profile converts it to its native representation and reports quantization.

Concrete boundary vectors include: adjacent integer positions around 2^53 remain distinct; negative half-unit rounding follows the chosen policy; an overflowing scale product fails without output mutation; motor and load origins cannot be confused; a stale generation cannot prepare a referenced move; a native float cannot silently hide an excessive target error. Unsupported conversions fail before work is emitted.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Update the status table and axis/units guide and add Doxygen on every public type, field and function. Explain approximation and rounding with one worked conversion using explicitly illustrative scale values.

Under [DELEGATION.md](DELEGATION.md), arithmetic review and independent boundary-vector design are suitable separate tasks. The lead owns public types and integrates tests after semantics are settled; do not let two agents invent different rounding or origin rules.

## Verify and exit

Run overflow/sign extremes, rational reduction, precision above 2^53, exact/fractional native targets, all rounding policies/ties, angle wrap/half-turn ambiguity, motor/load scale, origin/generation exhaustion, optional versus required reference, stale/future evidence and unchanged-on-failure output tests. Compare common semantic vectors to RS with separate namespaces/executables where needed.

Exit with tested pure math/status API and documented deliberate signature differences. No typed motion API may claim device operation yet.
