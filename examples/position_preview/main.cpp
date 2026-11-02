// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <cstdio>

int main() {
    namespace M = MotorControlCAN;
    namespace L = MotorControlCAN::LICHUAN_C;
    M::AxisConfig axis;
    axis.binding.logicalAxisId = 1;
    axis.binding.endpoint.profile = 1;
    axis.binding.endpoint.address = 9;
    axis.binding.bindingGeneration = 1;
    axis.binding.configurationGeneration = 1;
    M::PositionRequest request;
    request.configurationGeneration = 1;
    request.value = M::Rational(250);
    L::DriverSettings settings;
    settings.positionSpeedRpm = 120;
    settings.rampUpTimeMs = 100;
    settings.rampDownTimeMs = 200;
    L::PreparedPositionParameters prepared;
    if (!L::preparePositionParameters(prepared, L::SettingsModel::CL86_C, request, axis, nullptr,
                                      settings, 0))
        return 1;
    std::printf("Prepared absolute target: %ld pulses, %lu rpm, ramps %lu/%lu ms\n",
                static_cast<long>(prepared.targetNative),
                static_cast<unsigned long>(prepared.settings.positionSpeedRpm),
                static_cast<unsigned long>(prepared.settings.rampUpTimeMs),
                static_cast<unsigned long>(prepared.settings.rampDownTimeMs));
    std::puts("Pure parameter arithmetic: no CAN traffic, acceptance or motion completion.");
    return 0;
}
