# Prompt 9 Status and exact units

Apply [COMMON.md](COMMON.md). Prerequisites: [6 Common API and ownership proof](6_api_ownership.md), [8 Verification CI and documentation gates](8_verification_docs.md).

## Objective and boundaries

Implement production Status and exact numeric/unit primitives, replacing the corresponding proof code. Finish arithmetic semantics and boundary tests here. Axis configuration/reference policy belongs to prompt 10; drive-specific native conversion and motion sequences remain later work. These functions perform no I/O or device lifecycle actions.

## Read and audit

Refresh RS, ADS1115 and INA228. Read current RS `Status.h`, `Units.h/.cpp`, and the reusable exact helpers in `Axis.h/.cpp`, including `parseExactNumber` and `rationalToDouble`; inspect their actual tests and callers. Own reusable arithmetic/parsing here; keep origin/frame/angle-path policy in prompt 10 rather than duplicating conversion helpers. Compare I2C status differences and MB85RC overflow/output policy without importing competing helper names. Inspect FieldCore exact target handling read-only. Use prompt 4's contrasts, [CONVENTIONS.md](CONVENTIONS.md#2-status-is-a-call-result) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

### Concrete evidence to recheck

Use [motion facts](../audit/LICHUAN_MOTION_FACTS.md) for native-unit contrasts: `0x6083/0x6084/0x609A` carry milliseconds, `0x6081/0x6099` RPM, and position scaling remains model/firmware-dependent. `0x60FF` is I32 in EDS while EN86 lists U32 and nonnegative limits; retain that conflict instead of making every velocity unsigned. Native width and unit must remain distinct from the exact common unit representation.

## Implement

1. Implement the selected RS-style `Err`, `Status`, free `Ok()`, `Status::isOk()`, explicit boolean conversion and free `errToString()`. `Status` carries `Err code`, documented `int32_t detail` and a static-lifetime `const char* msg`; apply valid C++11 constexpr rules. Use the chosen CAN error policy, including protocol rejection, without copying RTU-only error semantics or adding `ok()`/`success()`/`Status::Ok()` aliases.
2. Keep call validation, native service outcomes, drive faults and application health separate. Preserve full raw CANopen abort codes in a distinct `uint32_t` rather than narrowing through signed detail. Include unknown enum behavior and stable labels without heap allocation or logging.
3. Port/adapt attributable exact rational/integer primitives and compatible public names: `parseExactNumber`, `rationalToDouble`, `convertDisplacement`, `convertVelocity` and `convertAcceleration`. Check every multiplication, addition, reduction, sign change and denominator. Exact native integer values must not pass through binary floating point. Document when conversion to double is explicitly requested and cannot promise exact representation.
4. Support the common dimensional vocabulary required by the axis contract: steps/command increments, encoder counts, turns, degrees, explicitly approximated radians and configured linear travel. This layer consumes explicit scale facts; it does not invent geometry, zero-denominator meanings or defaults such as 1000 counts/turn. Keep native RPM widths and Lichuan ramp times out of dimensional acceleration types.
5. Implement selected rounding and quantization policy as checked operations, with documented ties and signed behavior. Provide enough explicit error information for a profile to reject excessive float-native target error. Do not pretend all CAN protocols have integer targets or that an inexact float conversion preserves a common exact request.
6. Replace duplicate prototype arithmetic in the retained proof and preserve small N5/EPOS4/common invariant tests through the production functions. Avoid introducing a general dimensional-analysis framework where direct value types and conversion functions express current requirements.

## Verify and exit

Test all sign/extreme boundaries, zero/invalid denominators, rational reduction, parser capacity/malformed input, adjacent integers around `2^53`, overflowing scale products, every selected rounding policy and negative half-unit ties. Test zero and extreme values without invoking undefined signed overflow. Verify native floating quantization is measured and rejected when outside declared tolerance.

Use independent expected vectors rather than encoding and decoding through the same helper. Verify output sentinels remain unchanged on failure; document any explicit exception. Compare compatible semantic vectors to current RS in separate namespaces/executables. Run focused tests, strict headers, affected verification/Doxygen, prompt integrity and diff checks.

Exit with production pure units/status APIs and documented intentional differences. Close scope in `docs/reports/prompt_9.md`, update PROGRESS and affected coverage, then commit and sync this branch. A tested arithmetic group with its docs may be checkpointed earlier; do not leave production arithmetic half migrated.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source): document units, sign/range, rounding, approximation, static string lifetime and output-on-failure. Update the units/status guide and CHANGELOG. Apply [MAINTAINABILITY.md](MAINTAINABILITY.md#tests-and-refactoring). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), delegate independent arithmetic vectors and stronger overflow/quantization review. One implementer owns public numeric types and policies.
