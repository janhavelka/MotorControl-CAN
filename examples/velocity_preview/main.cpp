// SPDX-License-Identifier: MIT
// Offline exact rate and closed capability gate. No controller or manufactured drive observations.
#include <MotorControlCAN/profiles/lichuan_c/Velocity.h>
#include <cstdio>

namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
int main() {
    M::AxisConfig axis;
    axis.binding.logicalAxisId = 1;
    axis.binding.endpoint.profile = 1;
    axis.binding.endpoint.address = 1;
    axis.binding.bindingGeneration = axis.binding.configurationGeneration = 1;
    axis.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    axis.nativeVelocityTime = M::TimeUnit::MINUTE;
    axis.units.motorTurnsPerLoadTurn = M::UnitScale(5, 1, M::ScaleSource::ASSUMED);
    M::FiniteVelocityRequest request;
    request.velocity.value = M::Rational(12);
    request.velocity.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    request.velocity.frame = M::CoordinateFrame::LOAD;
    request.velocity.configurationGeneration = 1;
    request.durationUs = 1000000;
    M::PreparedVelocityTarget rate;
    const M::Status converted = M::prepareVelocityTarget(request.velocity, axis, rate);
    if (!converted)
        return 1;
    std::printf("Illustrative 5:1 ratio: 12 load rpm -> %lld motor rpm.\n"
                "Finite duration starts at preparation; setup consumes its 1000000 us.\n",
                static_cast<long long>(rate.effectiveNative));
    static L::VelocityContext run;
    static L::ReadContext state;
    L::VelocityProfile ramps;
    ramps.rampUpTimeMs = ramps.rampDownTimeMs = 100;
    L::VelocityOptions options;
    options.service.queueTimeoutUs = options.service.responseTimeoutUs = 2000;
    options.pollIntervalUs = 10000;
    options.maxPolls = 200;
    options.maximumAgeUs = 40000;
    options.requiredStopBudgetUs = 200000;
    options.commandIntervalUs = 10000;
    // Real operation also needs the owner's reservation, fresh identity/state and an actual
    // firmware qualification record. Keeping this CLOSED is deliberate; arithmetic proves none.
    const M::Status gated = L::prepareVelocity(run, L::AxisReservation(), L::ReadTarget(), state,
                                               L::IdentityObservation(), L::VelocityEvidence(),
                                               request, ramps, axis, 1, 0, 2000000, options);
    std::printf("Velocity capability: %s; no work admitted.\n", gated.msg);
    return rate.effectiveNative == 60 && gated.code == M::Err::UNSUPPORTED &&
                   run.state == M::ActionState::EMPTY
               ? 0
               : 2;
}
