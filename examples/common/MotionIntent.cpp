// SPDX-License-Identifier: MIT
#include "MotionIntent.h"
#include <cmath>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
M::Status invalid(const char* message) {
    return M::Status(M::Err::INVALID_CONFIG, 0, message);
}
M::Status range(const char* message) {
    return M::Status(M::Err::ILLEGAL_VALUE, 0, message);
}
M::Status rate(const M::VelocityRequest& request, const M::AxisConfig& axis, bool signedRate,
               bool allowZero, M::PreparedVelocityTarget& output) {
    // Check the destination before conversion: default increments/second can give plausible rpm.
    if (axis.velocityRepresentation != M::NativeRepresentation::INTEGER ||
        axis.nativeRateBasis != M::NativeRateBasis::MOTOR_TURNS ||
        axis.nativeVelocityTime != M::TimeUnit::MINUTE)
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::MoveError::RATE_BASIS),
                         "native rate destination must be integer motor turns per minute");
    M::PreparedVelocityTarget candidate;
    const M::Status converted = M::prepareVelocityTarget(request, axis, candidate);
    if (!converted)
        return converted;
    const int64_t low = signedRate ? -3000 : 0;
    const int64_t value = candidate.effectiveNative;
    if (value < low || value > 3000 || (!allowZero && !value))
        return range("effective speed is outside admitted motor rpm range");
    if (candidate.exactArithmetic) {
        const M::NativeQuantity& q = candidate.requestedNative;
        if (q.integral < low || q.integral > 3000 ||
            (q.integral == low && q.numerator && q.negative) ||
            (q.integral == 3000 && q.numerator && !q.negative) ||
            (!allowZero && !q.integral && !q.numerator) ||
            (!allowZero &&
             ((q.integral < 0 || (!q.integral && q.numerator && q.negative)) != (value < 0))))
            return range("requested speed is outside admitted motor rpm range");
    } else {
        const double error = candidate.absoluteErrorBound;
        const double distance = value < 0 ? -double(value) : double(value);
        if (!std::isfinite(error) || error < 0 || error > double(value - low) ||
            error > double(3000 - value) || (!allowZero && error >= distance))
            return range("approximate speed crosses admitted motor rpm boundary");
    }
    output = candidate;
    return M::Ok();
}
bool validSetup(L::MoveSetup setup) {
    return setup == L::MoveSetup::WRITE_ALL || setup == L::MoveSetup::REUSE_STORED;
}
} // namespace

M::Status makeSimplePosition(const M::Rational& value, M::PositionUnit unit, bool relative,
                             M::Generation generation, M::PositionRequest& output) noexcept {
    if (!value.denominator || !generation ||
        (unit != M::PositionUnit::STEPS && unit != M::PositionUnit::DEGREES &&
         unit != M::PositionUnit::TURNS && unit != M::PositionUnit::MILLIMETRES))
        return invalid("simple moves require steps, deg, turn or mm and a current epoch");
    M::PositionRequest request;
    request.value = value;
    request.unit = unit;
    request.relative = relative;
    request.frame = unit == M::PositionUnit::STEPS && relative ? M::CoordinateFrame::NATIVE
        : unit == M::PositionUnit::MILLIMETRES ? M::CoordinateFrame::LOAD : M::CoordinateFrame::MOTOR;
    request.configurationGeneration = generation;
    request.rounding = M::Rounding::NEAREST;
    request.maximumQuantizationError = M::Rational(1, 2);
    output = request;
    return M::Ok();
}
M::Status setSimpleSpeed(SimpleMoveIntent& intent, const M::VelocityRequest& speed,
                         const M::AxisConfig& axis) noexcept {
    M::PreparedVelocityTarget checked;
    const M::Status status = rate(speed, axis, false, true, checked);
    if (!status)
        return status;
    intent.speed = speed;
    intent.speedKnown = true;
    return M::Ok();
}
M::Status setSimpleRamp(SimpleMoveIntent& intent, bool acceleration,
                        const M::Rational& milliseconds) noexcept {
    M::Rational reduced;
    const M::Status status = M::reduceRational(milliseconds, reduced);
    if (!status)
        return status;
    if (reduced.denominator != 1 || reduced.numerator < 0 || reduced.numerator > 2000)
        return range("ramp must be an integer in 0..2000 milliseconds");
    if (acceleration) {
        intent.rampUpMs = static_cast<uint32_t>(reduced.numerator);
        intent.rampUpKnown = true;
    } else {
        intent.rampDownMs = static_cast<uint32_t>(reduced.numerator);
        intent.rampDownKnown = true;
    }
    return M::Ok();
}
M::Status setSimpleSetup(SimpleMoveIntent& intent, L::MoveSetup setup) noexcept {
    if (!validSetup(setup))
        return invalid("unsupported native move setup policy");
    intent.setup = setup;
    return M::Ok();
}
M::Status makeCommandScale(const M::Rational& stepsPerTurn, M::UnitScale& output) noexcept {
    M::Rational reduced;
    const M::Status status = M::reduceRational(stepsPerTurn, reduced);
    if (!status)
        return status;
    if (reduced.numerator <= 0 || uint64_t(reduced.numerator) > UINT32_MAX ||
        reduced.denominator > UINT32_MAX)
        return range("command steps per motor turn must fit positive uint32 scale fields");
    output = M::UnitScale(static_cast<uint32_t>(reduced.numerator),
                          static_cast<uint32_t>(reduced.denominator), M::ScaleSource::ASSUMED);
    return M::Ok();
}
M::Status buildSimpleMove(const SimpleMoveIntent& intent, const M::PositionRequest& position,
                          const M::AxisConfig& axis, bool simpleAbsolute, M::MoveRequest& request,
                          L::MoveProfile& profile) noexcept {
    const M::Status configured = M::validateAxisConfig(axis);
    if (!configured)
        return configured;
    if (!intent.speedKnown || !intent.rampUpKnown || !intent.rampDownKnown)
        return invalid("set speed, accel and decel before a simple move");
    if (axis.units.commandStepsPerMotorTurn.source == M::ScaleSource::UNKNOWN)
        return invalid("declare command steps per motor turn before a simple move");
    if (position.wrapped || position.relative == simpleAbsolute || !validSetup(intent.setup))
        return invalid("simple move requires its explicit relative or absolute intent");
    if (position.configurationGeneration != axis.binding.configurationGeneration)
        return invalid("position configuration generation is stale");
    if (simpleAbsolute && !axis.originKnown)
        return invalid("moveto requires an explicit axis origin");
    const unsigned basis = static_cast<unsigned>(position.basis);
    if (!simpleAbsolute && (basis > 2 || !(axis.supportedRelativeBases & (1u << basis))))
        return invalid("selected relative basis is not supported; no fallback is permitted");
    if (!intent.rampUpMs || intent.rampUpMs > 2000 || !intent.rampDownMs ||
        intent.rampDownMs > 2000)
        return range("a move requires qualified ramp times in 1..2000 milliseconds");
    M::PreparedVelocityTarget checked;
    const M::Status speed = rate(intent.speed, axis, false, false, checked);
    if (!speed)
        return speed;
    M::MoveRequest candidate;
    candidate.position = position;
    candidate.speed = intent.speed;
    L::MoveProfile native;
    native.rampUpTimeMs = intent.rampUpMs;
    native.rampDownTimeMs = intent.rampDownMs;
    native.setup = intent.setup;
    request = candidate;
    profile = native;
    return M::Ok();
}
M::Status makeFiniteVelocity(const M::VelocityRequest& velocity, uint64_t durationMs,
                             const M::AxisConfig& axis, M::FiniteVelocityRequest& request,
                             M::PreparedVelocityTarget* preview) noexcept {
    if (!durationMs || durationMs > uint64_t(INT64_MAX) / 1000u)
        return range("finite velocity duration is outside the nonwrapping microsecond domain");
    M::PreparedVelocityTarget checked;
    const M::Status status = rate(velocity, axis, true, false, checked);
    if (!status)
        return status;
    M::FiniteVelocityRequest candidate;
    candidate.velocity = velocity;
    candidate.durationUs = durationMs * 1000u;
    request = candidate;
    if (preview)
        *preview = checked;
    return M::Ok();
}
} // namespace MotorControlCANExample
