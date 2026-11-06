// SPDX-License-Identifier: MIT
// Offline preparation through the installed core: no controller, stack, owner or hardware.
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <cstdio>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;

int main() {
    // Every value is an illustrative application input, never a manufacturer default or measurement.
    M::AxisConfig axis;
    axis.binding.logicalAxisId = 1;
    axis.binding.endpoint.profile = 1;
    axis.binding.endpoint.address = 9;
    axis.binding.bindingGeneration = axis.binding.configurationGeneration = 1;
    axis.units.commandStepsPerMotorTurn = M::UnitScale(1000, 1, M::ScaleSource::ASSUMED);
    axis.units.motorTurnsPerLoadTurn = M::UnitScale(5, 1, M::ScaleSource::ASSUMED);
    axis.units.commandPolarity = 1;
    axis.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    axis.nativeVelocityTime = M::TimeUnit::MINUTE;
    axis.supportedRelativeBases = 1; // Explicit host policy admits the ACTUAL witness below.
    axis.originKnown = true;
    axis.originNative = 100;
    axis.originSource = M::ScaleSource::ASSUMED;
    axis.nativeMinimum = -10000;
    axis.nativeMaximum = 10000;
    axis.softLimitsKnown = true;
    axis.softMinimum = -5000;
    axis.softMaximum = 5000;

    const uint64_t nowUs = 1000;
    M::AxisReference reference;
    reference.binding = axis.binding;
    reference.referenceGeneration = axis.referenceGeneration;
    reference.idle = reference.stationary = reference.nativeKnown = true;
    reference.nativePosition = 100; // Illustrative multi-turn command coordinate at host load zero.
    reference.basis = M::RelativeBasis::ACTUAL;
    reference.source = M::ScaleSource::ASSUMED;
    reference.observedUs = reference.nowUs = nowUs;
    reference.maximumAgeUs = 100000;

    M::MoveRequest move;
    move.position.configurationGeneration = 1;
    move.position.value = M::Rational(10);
    move.position.unit = M::PositionUnit::DEGREES;
    move.position.frame = M::CoordinateFrame::LOAD;
    move.position.wrapped = true;
    move.position.path = M::AnglePath::SHORTEST;
    move.position.tie = M::HalfTurnTie::REJECT;
    move.position.rounding = M::Rounding::NEAREST;
    move.position.maximumQuantizationError = M::Rational(1, 2);
    move.speed.configurationGeneration = 1;
    move.speed.value = M::Rational(120);
    move.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);

    L::DriverSettings settings;
    settings.positionSpeedRpm = 120;
    settings.rampUpTimeMs = 100;
    settings.rampDownTimeMs = 200;
    L::PreparedPositionParameters preview;
    const M::Status arithmetic = L::preparePositionParameters(
        preview, L::SettingsModel::CL86_C, move.position, axis, &reference, settings, nowUs);
    if (!arithmetic) {
        std::printf("Angle preview refused: %s\n", arithmetic.msg);
        return 1;
    }
    const M::IntegerQuantization& error = preview.target.quantization;
    std::printf("ILLUSTRATIVE: shortest path to 10 load degrees, half-turn ties rejected\n"
                "Effective absolute target: %ld increments; error %s%llu/%llu increments\n"
                "Configuration/reference generations: %lu/%lu; no service admitted\n",
                static_cast<long>(preview.targetNative), error.errorNegative ? "-" : "+",
                static_cast<unsigned long long>(error.errorNumerator),
                static_cast<unsigned long long>(error.errorDenominator),
                static_cast<unsigned long>(preview.target.binding.configurationGeneration),
                static_cast<unsigned long>(preview.target.referenceGeneration));

    L::MoveProfile profile;
    profile.rampUpTimeMs = settings.rampUpTimeMs;
    profile.rampDownTimeMs = settings.rampDownTimeMs;
    L::MoveOptions options;
    options.service.queueTimeoutUs = options.service.responseTimeoutUs = 2000;
    options.observationIntervalUs = 1000;
    options.maximumAgeUs = 40000;

    // A live caller supplies its real owner reservation, fresh state and identity, and latest
    // acknowledged 000F here. This offline example has none and leaves them empty. The explicitly
    // CLOSED capability record refuses first, without inventing that provenance or any measurement.
    static L::MoveContext context;
    static L::ReadContext state;
    L::AxisReservation reservation;
    L::ReadTarget current;
    C::SdoCompletion control;
    L::IdentityObservation identity;
    const M::Status gated = L::prepareMoveAngle(context, reservation, current, state, control,
                                                identity, L::MoveEvidence(), move, profile, nullptr,
                                                axis, reference, 1, nowUs, nowUs + 400000, options);
    std::printf("Finite angle move: %s; no service admitted\n", gated.msg);
    return gated.code == M::Err::UNSUPPORTED && context.state == M::ActionState::EMPTY ? 0 : 2;
}
