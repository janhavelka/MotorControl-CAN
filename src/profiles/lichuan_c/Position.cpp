// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <cmath>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
constexpr int64_t TARGET_MINIMUM = -1000000;
constexpr int64_t TARGET_MAXIMUM = 1000000;
constexpr uint32_t MAXIMUM_POSITION_SPEED_RPM = 3000;
constexpr uint32_t MAXIMUM_RAMP_TIME_MS = 2000;

Status error(PositionParameterError detail, const char* message, Err code = Err::INVALID_CONFIG) {
    return Status(code, static_cast<int32_t>(detail), message);
}

Status axisStatus(Status status) {
    if (!status)
        status.detail += static_cast<int32_t>(PositionParameterError::AXIS_ERROR_BASE);
    return status;
}

bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}

bool inRange(const NativeQuantity& value) {
    return value.integral >= TARGET_MINIMUM && value.integral <= TARGET_MAXIMUM &&
           !(value.integral == TARGET_MINIMUM && value.numerator && value.negative) &&
           !(value.integral == TARGET_MAXIMUM && value.numerator && !value.negative);
}

Status requestedRange(const PreparedTarget& target) {
    if (!target.exactArithmetic) {
        const int64_t lowerDistance = target.effectiveNative - TARGET_MINIMUM;
        const int64_t upperDistance = TARGET_MAXIMUM - target.effectiveNative;
        const int64_t distance = lowerDistance < upperDistance ? lowerDistance : upperDistance;
        // Comparing the bound with exact small distances avoids rounding a tiny
        // outward uncertainty away when the effective integer is at a limit.
        if (!std::isfinite(target.absoluteErrorBound) || target.absoluteErrorBound < 0 ||
            target.absoluteErrorBound > static_cast<double>(distance))
            return error(PositionParameterError::RANGE,
                         "approximate requested position may exceed the native target range",
                         Err::ILLEGAL_VALUE);
        return Ok();
    }

    // Quantization records effective-requested for the encoded value, before
    // adding a relative basis. Reconstructing from it avoids subtracting a
    // possibly INT64_MIN reference or duplicating coordinate conversion.
    NativeQuantity requested;
    requested.numerator = target.quantization.errorNumerator;
    requested.denominator = target.quantization.errorDenominator;
    requested.negative = requested.numerator && !target.quantization.errorNegative;
    Status status = addNativeInteger(requested, target.effectiveNative, requested);
    if (!status) {
        status.detail += static_cast<int32_t>(AxisError::UNIT_ERROR_BASE);
        return axisStatus(status);
    }
    if (!inRange(requested))
        return error(PositionParameterError::RANGE,
                     "requested position exceeds the native target range", Err::ILLEGAL_VALUE);
    return Ok();
}
} // namespace

Status preparePositionParameters(PreparedPositionParameters& output, SettingsModel model,
                                 const PositionRequest& request, const AxisConfig& axis,
                                 const AxisReference* reference, const DriverSettings& settings,
                                 uint64_t nowUs) noexcept {
    if (overlap(&output, sizeof output, &request, sizeof request) ||
        overlap(&output, sizeof output, &axis, sizeof axis) ||
        overlap(&output, sizeof output, &settings, sizeof settings) ||
        (reference && overlap(&output, sizeof output, reference, sizeof *reference)))
        return error(PositionParameterError::OVERLAP, "position parameters overlap input");
    if (model != SettingsModel::CL86_C && model != SettingsModel::OL86_C)
        return error(PositionParameterError::MODEL, "explicit documented 86C model required");
    if (request.wrapped || nowUs > uint64_t(INT64_MAX))
        return error(PositionParameterError::ARGUMENT,
                     "position parameters require unwrapped intent and a valid current time");
    if (axis.positionRepresentation != NativeRepresentation::INTEGER)
        return error(PositionParameterError::REPRESENTATION,
                     "86C position target requires integer native representation",
                     Err::UNSUPPORTED);
    if (settings.positionSpeedRpm > MAXIMUM_POSITION_SPEED_RPM ||
        settings.rampUpTimeMs > MAXIMUM_RAMP_TIME_MS ||
        settings.rampDownTimeMs > MAXIMUM_RAMP_TIME_MS)
        return error(PositionParameterError::RANGE, "position profile exceeds native rpm/ms ranges",
                     Err::ILLEGAL_VALUE);

    AxisReference currentReference;
    if (reference) {
        if (reference->nowUs > nowUs)
            return axisStatus(Status(Err::INVALID_CONFIG,
                                     static_cast<int32_t>(AxisError::CLOCK_ERROR),
                                     "position preparation time precedes reference time"));
        currentReference = *reference;
        currentReference.nowUs = nowUs;
        reference = &currentReference;
    }

    PreparedPositionParameters candidate;
    Status status = preparePosition(request, axis, reference, candidate.target);
    if (!status)
        return axisStatus(status);
    if (candidate.target.effectiveNative < TARGET_MINIMUM ||
        candidate.target.effectiveNative > TARGET_MAXIMUM)
        return error(PositionParameterError::RANGE,
                     "effective position exceeds the native target range", Err::ILLEGAL_VALUE);
    status = requestedRange(candidate.target);
    if (!status)
        return status;

    candidate.model = model;
    candidate.axis = axis;
    candidate.settings = settings;
    candidate.targetNative = static_cast<int32_t>(candidate.target.effectiveNative);
    candidate.preparedUs = nowUs;
    output = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
