// SPDX-License-Identifier: MIT
#include <MotorControlCAN/Axis.h>
#include <cinttypes>
#include <cstdio>

using namespace MotorControlCAN;

namespace {
bool checked(Status status, const char* operation) {
    if (status) return true;
    std::fprintf(stderr, "%s failed: %s (%" PRId32 "): %s\n", operation,
        errToString(status.code), status.detail, status.msg);
    return false;
}

AxisConfig illustrativeAxis() {
    AxisConfig axis;
    axis.binding.logicalAxisId = 9;
    axis.binding.endpoint.protocol = Protocol::PROPRIETARY;
    axis.binding.endpoint.profile = 1;
    axis.binding.endpoint.address = 500;
    axis.binding.endpoint.subaxis = 256;
    axis.binding.bindingGeneration = axis.binding.configurationGeneration = 1;
    axis.units.commandStepsPerMotorTurn = UnitScale(4096, 1, ScaleSource::ASSUMED);
    axis.units.motorTurnsPerLoadTurn = UnitScale(3, 2, ScaleSource::ASSUMED);
    axis.units.encoder.countsPerUnit = UnitScale(2048, 1, ScaleSource::ASSUMED);
    axis.units.encoder.sourceId = 7;
    axis.units.millimetresPerLoadTurn = UnitScale(20, 1, ScaleSource::ASSUMED);
    axis.originKnown = true;
    axis.originSource = ScaleSource::ASSUMED;
    return axis;
}
} // namespace

int main() {
    const AxisConfig axis = illustrativeAxis();
    if (!checked(validateAxisConfig(axis), "illustrative configuration")) return 1;
    std::puts("Offline illustrative arithmetic; no drive or controller is opened.");
    std::printf("Logical axis %" PRIu32 ": illustrative endpoint %" PRIu32 ", subaxis %u\n",
        axis.binding.logicalAxisId, axis.binding.endpoint.address,
        static_cast<unsigned>(axis.binding.endpoint.subaxis));

    PositionRequest position;
    position.value = Rational(5);
    position.unit = PositionUnit::MILLIMETRES;
    position.frame = CoordinateFrame::LOAD;
    position.configurationGeneration = axis.binding.configurationGeneration;
    PreparedTarget target;
    if (!checked(preparePosition(position, axis, nullptr, target), "load position")) return 1;
    std::printf("Load 5 mm -> exact command endpoint %" PRId64 " + %" PRIu64 "/%" PRIu64
                "; effective integer %" PRId64 "\n",
        target.requestedNative.integral, target.requestedNative.numerator,
        target.requestedNative.denominator, target.effectiveNative);
    if (target.effectiveNative != 1536) return 2;

    Rational feedback;
    if (!checked(convertDisplacement(Rational(5), PositionUnit::MILLIMETRES,
            PositionUnit::ENCODER_COUNTS, axis.units, feedback), "independent feedback scale")) return 1;
    std::printf("Same illustrative displacement -> %" PRId64 "/%" PRIu64 " encoder counts\n",
        feedback.numerator, feedback.denominator);
    if (feedback.numerator != 768 || feedback.denominator != 1) return 2;

    AxisConfig floatAxis = illustrativeAxis();
    floatAxis.positionRepresentation = NativeRepresentation::BINARY32;
    position.value = Rational(1, 10);
    position.unit = PositionUnit::STEPS;
    position.frame = CoordinateFrame::NATIVE;
    position.maximumQuantizationError = Rational(1, 100000000);
    if (!checked(preparePosition(position, floatAxis, nullptr, target), "binary32 position")) return 1;
    std::printf("Native exact 1/10 -> binary32 %.9g; absolute error bound %.17g"
                " (allowance 1/100000000)\n",
        static_cast<double>(target.effectiveFloat), target.absoluteErrorBound);
    position.maximumQuantizationError = Rational(0);
    const Status rejected = preparePosition(position, floatAxis, nullptr, target);
    if (rejected) return 2;
    std::printf("Zero binary32 error allowance rejects 1/10: %s (%" PRId32 ")\n",
        errToString(rejected.code), rejected.detail);

    VelocityRequest velocity;
    velocity.value = Rational(-70000);
    velocity.configurationGeneration = axis.binding.configurationGeneration;
    PreparedVelocityTarget rate;
    if (!checked(prepareVelocityTarget(velocity, axis, rate), "wide signed velocity")) return 1;
    std::printf("Native velocity %" PRId64 " command increments/second\n", rate.effectiveNative);
    return rate.effectiveNative == -70000 ? 0 : 2;
}
