/** @file Units.h
 * @brief Pure exact quantities, checked dimensional conversion and quantization.
 * Functions perform bounded arithmetic only: no I/O, time, allocation, retained
 * input or shared mutable state. Independent outputs may be used concurrently;
 * callers serialize writes to a shared output. Outputs remain unchanged on any
 * error, including input/output aliasing. IEEE binary64/binary32 are required.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stdint.h>
#include "MotorControlCAN/Status.h"

namespace MotorControlCAN {
/** Exact signed fraction. Zero denominator is invalid. Functions reduce before
 * range checks; reduced numerator must fit int64 and denominator uint64. */
struct Rational {
    int64_t numerator; ///< Signed exact numerator in the selected unit.
    uint64_t denominator; ///< Positive denominator; no implicit scale or geometry.
    /** @param[in] n Signed numerator. @param[in] d Positive denominator. */
    constexpr Rational(int64_t n = 0, uint64_t d = 1) : numerator(n), denominator(d) {}
};
/** Spatial quantities. Angles/travel use the load axis; STEPS are command
 * increments, independent of optional full-step geometry and feedback counts. */
enum class PositionUnit : uint8_t {
    STEPS, ///< Native command increments, following commandPolarity.
    FULL_STEPS, ///< Optional motor full steps; never a servo prerequisite.
    ENCODER_COUNTS, ///< Selected feedback counts, following encoder polarity/basis.
    TURNS, ///< Load turns; no wrapping or origin change.
    DEGREES, ///< Exactly 360 per load turn.
    RADIANS, ///< Requires explicit approximation when converted to another dimension.
    MILLIMETRES ///< Linear travel using explicit millimetresPerLoadTurn.
};
/** Exact time denominator; does not imply a native RPM field or ramp time. */
enum class TimeUnit : uint8_t {
    SECOND, ///< One SI second.
    MINUTE, ///< Exactly 60 seconds.
    MILLISECOND ///< Exactly 1/1000 second.
};
/** Retained scale provenance, not proof that a drive setting is active. */
enum class ScaleSource : uint8_t {
    UNKNOWN, ///< No usable scale: represented only by 0/1.
    ASSUMED, ///< Explicit caller assumption.
    DOCUMENTED, ///< Applicable documentary value.
    READBACK, ///< Checked readback supplied by caller.
    QUALIFIED ///< Additional caller qualification.
};
/** Positive rational scale; missing optional scale is 0/1 with UNKNOWN source. */
struct UnitScale {
    uint32_t numerator; ///< Positive scale numerator, or zero only for UNKNOWN.
    uint32_t denominator; ///< Positive scale denominator.
    ScaleSource source; ///< Retained applicability/evidence class.
    /** @param[in] n Numerator. @param[in] d Denominator. @param[in] s Provenance. */
    constexpr UnitScale(uint32_t n = 0, uint32_t d = 1, ScaleSource s = ScaleSource::UNKNOWN)
        : numerator(n), denominator(d), source(s) {}
};
/** Physical basis of the selected feedback scale, independent of command units. */
enum class EncoderBasis : uint8_t {
    MOTOR_TURN, ///< Counts per motor turn.
    LOAD_TURN, ///< Counts per load turn.
    MILLIMETRE ///< Counts per millimetre of load travel.
};
/** Selected feedback source; no scale is inferred from its technology. */
struct EncoderScale {
    UnitScale countsPerUnit; ///< Explicit counts per selected basis unit.
    EncoderBasis basis = EncoderBasis::MOTOR_TURN; ///< Basis for countsPerUnit.
    uint32_t sourceId = 0; ///< Application identity; nonzero when scale is known.
    int8_t polarity = 1; ///< Exactly +1 or -1.
};
/** Independent scale facts only. No origins, references, defaults, I/O or policy.
 * Malformed optional facts fail validation; missing facts fail only when needed.
 * ASSUMED scales are usable; the caller owns documentary/qualification policy. */
struct UnitConfig {
    UnitScale commandStepsPerMotorTurn; ///< Command increments per motor turn.
    UnitScale motorTurnsPerLoadTurn; ///< Gear ratio, motor turns per load turn.
    UnitScale fullStepsPerMotorTurn; ///< Optional full-step geometry.
    UnitScale millimetresPerLoadTurn; ///< Explicit linear travel per load turn.
    EncoderScale encoder; ///< Independent selected feedback scale and polarity.
    int8_t commandPolarity = 1; ///< Exactly +1 or -1.
};
/** Spatial quantity per exact time unit. */
struct VelocityUnit {
    PositionUnit position; ///< Numerator spatial unit.
    TimeUnit time; ///< Time denominator.
    /** @param[in] p Spatial unit. @param[in] t Time denominator. */
    constexpr VelocityUnit(PositionUnit p = PositionUnit::STEPS, TimeUnit t = TimeUnit::SECOND)
        : position(p), time(t) {}
};
/** Distinct time denominators support e.g. turns/minute/second (RPM/s).
 * A native millisecond ramp duration is not an acceleration. */
struct AccelerationUnit {
    PositionUnit position; ///< Spatial numerator.
    TimeUnit velocityTime; ///< First time denominator.
    TimeUnit accelerationTime; ///< Second time denominator.
    /** @param[in] p Spatial unit. @param[in] v First time. @param[in] a Second time. */
    constexpr AccelerationUnit(PositionUnit p = PositionUnit::STEPS,
        TimeUnit v = TimeUnit::SECOND, TimeUnit a = TimeUnit::SECOND)
        : position(p), velocityTime(v), accelerationTime(a) {}
};
/** Function-specific Status::detail, never a native protocol abort or drive fault. */
enum class UnitError : int32_t {
    NONE, ///< No error.
    INVALID_ARGUMENT, ///< Null text, zero denominator or invalid allowance.
    INVALID_TEXT, ///< Not strict integer/decimal/fraction ASCII grammar.
    CAPACITY, ///< Text exceeds 127 characters or an unsigned component exceeds uint64.
    ARITHMETIC_OVERFLOW, ///< Reduced result exceeds fixed exact storage.
    INVALID_SCALE, ///< Malformed scale or provenance.
    MISSING_SCALE, ///< Required explicit scale/source is absent.
    INVALID_POLARITY, ///< Polarity differs from +1/-1.
    INVALID_UNIT, ///< Unrecognized unit, time, basis or rounding value.
    FRACTIONAL_TARGET, ///< EXACT integer policy rejected a fraction.
    QUANTIZATION_ERROR, ///< Exact error exceeds caller tolerance.
    APPROXIMATION_REQUIRED, ///< Nonzero radians conversion requires explicit allowance.
    PRECISION_LIMIT ///< Approximation nonfinite, underflowing or outside its allowance.
};
/** Checked integer rounding; NEAREST resolves half ties to the even integer. */
enum class Rounding : uint8_t {
    EXACT, ///< Reject noninteger values.
    NEAREST, ///< Nearest, ties to even for either sign.
    TOWARD_ZERO, ///< Discard fractional magnitude.
    FLOOR, ///< Toward negative infinity.
    CEIL ///< Toward positive infinity.
};
/** Integer result plus exact signed error (effective minus requested).
 * Fraction fields are reduced; zero error has numerator 0, denominator 1. */
struct IntegerQuantization {
    int64_t value = 0; ///< Effective native integer, without passing through double.
    uint64_t errorNumerator = 0; ///< Unsigned error magnitude numerator.
    uint64_t errorDenominator = 1; ///< Positive error denominator.
    bool errorNegative = false; ///< True means effective is below requested.
};
/** Explicit approximate engineering result. Source scale uncertainty is excluded. */
struct UnitConversion {
    double value = 0; ///< Approximate destination quantity, never an exact native integer.
    double absoluteErrorBound = 0; ///< Conservative arithmetic/pi error in destination units.
};
/** Native IEEE binary32 quantization evidence; the exact request is not replaced. */
struct FloatQuantization {
    float value = 0; ///< Nearest binary32 value, ties to even.
    double absoluteErrorBound = 0; ///< Upward bound on exact absolute error in native units.
};

/** Reduce an exact fraction, canonicalizing zero. @param[in] value Input fraction.
 * @param[out] output Reduced result; unchanged on failure. @return Local status. */
Status reduceRational(const Rational& value, Rational& output);
/** Parse at most 127 ASCII characters: signed integer, decimal or n/d; no spaces,
 * exponent, .5, 1., signed denominator, NaN or infinity. Each unsigned component
 * must fit uint64 before reduction; decimal trailing zeros are removed first.
 * @param[in] text Borrowed NUL-terminated text (128 readable bytes if not terminated).
 * @param[out] output Reduced fraction; unchanged on failure. @return Local status. */
Status parseExactNumber(const char* text, Rational& output);
/** Explicit approximate conversion, magnitude rounded toward zero like RS.
 * Binary64 may not represent the exact request; use only for display/allowances.
 * @param[in] value Exact input. @param[out] output Approximation, unchanged on failure.
 * @return Local status. */
Status rationalToDouble(const Rational& value, double& output);
/** Validate optional scale representations, source identity and +/-1 polarities.
 * @param[in] config Explicit facts. @return Local validation status; no I/O. */
Status validateUnitConfig(const UnitConfig& config);
/** Exact displacement conversion, without wrapping/origin. All factors cancel
 * before checked multiplication; no binary floating point. Radians across units
 * require the explicit approximate overload (except exact zero).
 * @param[in] value Input. @param[in] from Input unit. @param[in] to Output unit.
 * @param[in] config Explicit facts. @param[out] output Reduced result, unchanged
 * on failure. @return Local status, including storage overflow. */
Status convertDisplacement(const Rational& value, PositionUnit from, PositionUnit to,
    const UnitConfig& config, Rational& output);
/** Exact rate conversion. @param[in] value Input. @param[in] from Input units.
 * @param[in] to Output units. @param[in] config Explicit facts.
 * @param[out] output Reduced result, unchanged on failure. @return Local status. */
Status convertVelocity(const Rational& value, VelocityUnit from, VelocityUnit to,
    const UnitConfig& config, Rational& output);
/** Exact acceleration conversion. @param[in] value Input. @param[in] from Input units.
 * @param[in] to Output units. @param[in] config Explicit facts.
 * @param[out] output Reduced result, unchanged on failure. @return Local status. */
Status convertAcceleration(const Rational& value, AccelerationUnit from, AccelerationUnit to,
    const UnitConfig& config, Rational& output);
/** Explicit approximate displacement, including radians with a conservative pi
 * bound. Assumes IEEE binary64 round-to-nearest; no change to floating environment.
 * @param[in] value Exact input. @param[in] from Input unit. @param[in] to Output unit.
 * @param[in] config Facts. @param[in] maximumError Positive finite absolute error allowance.
 * @param[out] output Approximation/error bound, unchanged on failure. @return Local status. */
Status convertDisplacement(const Rational& value, PositionUnit from, PositionUnit to,
    const UnitConfig& config, double maximumError, UnitConversion& output);
/** Explicit approximate rate conversion. @param[in] value Input. @param[in] from Input units.
 * @param[in] to Output units. @param[in] config Facts. @param[in] maximumError Positive finite allowance.
 * @param[out] output Value/error bound, unchanged on failure. @return Local status. */
Status convertVelocity(const Rational& value, VelocityUnit from, VelocityUnit to,
    const UnitConfig& config, double maximumError, UnitConversion& output);
/** Explicit approximate acceleration. @param[in] value Input. @param[in] from Input units.
 * @param[in] to Output units. @param[in] config Facts. @param[in] maximumError Positive finite allowance.
 * @param[out] output Value/error bound, unchanged on failure. @return Local status. */
Status convertAcceleration(const Rational& value, AccelerationUnit from, AccelerationUnit to,
    const UnitConfig& config, double maximumError, UnitConversion& output);
/** Quantize an exact request to int64; compare the exact error to a nonnegative
 * exact allowance. No floating conversion. @param[in] value Exact native quantity.
 * @param[in] rounding Rounding policy. @param[in] maximumError Native-unit allowance.
 * @param[out] output Effective value/exact error, unchanged on failure. @return Local status. */
Status quantizeInteger(const Rational& value, Rounding rounding, const Rational& maximumError,
    IntegerQuantization& output);
/** Quantize to IEEE binary32 with nearest-even ties, avoiding double rounding.
 * Error is compared exactly with the nonnegative rational allowance; the reported
 * double bound rounds upward. Zero allowance succeeds only for exact binary32.
 * @param[in] value Exact native quantity. @param[in] maximumError Native-unit allowance.
 * @param[out] output Float and error bound, unchanged on failure. @return Local status. */
Status quantizeFloat(const Rational& value, const Rational& maximumError, FloatQuantization& output);
} // namespace MotorControlCAN
