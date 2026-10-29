// SPDX-License-Identifier: MIT
#include <MotorControlCAN/MotorControlCAN.h>
#include <cstring>

int main() {
    using namespace MotorControlCAN;
    Binding binding;
    binding.logicalAxisId = 9;
    binding.endpoint.protocol = Protocol::PROPRIETARY;
    binding.endpoint.profile = 1;
    binding.endpoint.address = 500;
    binding.endpoint.subaxis = 2;
    binding.bindingGeneration = binding.configurationGeneration = 1;
    Frame frame;
    frame.id = 0x117;
    frame.extended = true;
    frame.dlc = frame.payloadLength = 4;
    if (!validateBinding(binding) || !sameBinding(binding, binding) || !validateFrame(frame)) return 1;
    frame.fd = true;
    if (validateFrame(frame).code != Err::UNSUPPORTED) return 2;
    if (std::strcmp(errToString(Err::OK), "OK") != 0 || VERSION_CODE == 0) return 3;
    Rational exact;
    if (!parseExactNumber("9007199254740993", exact) || exact.numerator != INT64_C(9007199254740993)) return 4;
    UnitConfig units;
    Rational converted;
    if (!convertVelocity(Rational(-70000), VelocityUnit(PositionUnit::TURNS, TimeUnit::MINUTE),
        VelocityUnit(PositionUnit::DEGREES, TimeUnit::SECOND), units, converted) ||
        converted.numerator != -420000 || converted.denominator != 1) return 5;
    FloatQuantization native;
    if (!quantizeFloat(Rational(16777217), Rational(1), native) || native.absoluteErrorBound != 1) return 6;
    AxisConfig config; config.binding = binding;
    config.units.commandStepsPerMotorTurn = UnitScale(4096, 1, ScaleSource::ASSUMED);
    config.units.motorTurnsPerLoadTurn = UnitScale(3, 2, ScaleSource::ASSUMED);
    config.units.millimetresPerLoadTurn = UnitScale(20, 1, ScaleSource::ASSUMED);
    config.originKnown = true; config.originSource = ScaleSource::ASSUMED;
    PositionRequest request; request.configurationGeneration = 1;
    request.value = Rational(5); request.unit = PositionUnit::MILLIMETRES; request.frame = CoordinateFrame::LOAD;
    PreparedTarget target;
    if (!preparePosition(request, config, nullptr, target) || target.endpointNative != 1536) return 7;
    if (!validatePreparedTarget(target, config, nullptr, 0)) return 8;
    return 0;
}
