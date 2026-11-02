// SPDX-License-Identifier: MIT
#include "FirmwareProbe.h"

namespace MotorControlCANExample {
FirmwareProbe::FirmwareProbe(MotorControlCAN::CANOPEN::SdoClient& services, TwaiDriver driver,
                             const ProbeCallbacks& callbacks) noexcept
    : transport_(services, driver), owner_(services, transport_.transport(), 28),
      platform_(callbacks), app_(owner_, platform_), evidence_(app_, owner_),
      callbacksValid_(callbacks.nowUs && !callbacks.idle) {}
bool FirmwareProbe::begin(const FirmwareConfig& config) noexcept {
    if (attempted_)
        return false;
    attempted_ = true;
    if (!callbacksValid_ || !config.configured || config.controller.txPin == 19 ||
        config.controller.txPin == 20 || config.controller.rxPin == 19 ||
        config.controller.rxPin == 20)
        return false;
    running_ = transport_.start(config.controller, owner_.controllerEpoch());
    return running_;
}
bool FirmwareProbe::service() noexcept {
    if (!running_) {
        platform_.service();
        return false;
    }
    uint64_t now = 0;
    FirmwareFailure failure = FirmwareFailure::NONE;
    if (!platform_.nowUs(now) || now < lastUs_) {
        failure = FirmwareFailure::CLOCK;
    } else {
        transport_.pollEvents(owner_, now);
        if (!app_.service(now)) {
            failure = FirmwareFailure::APPLICATION;
        } else {
            lastUs_ = now;
            transport_.promote(owner_, now);
            if (!evidence_.service(now))
                failure = FirmwareFailure::EVIDENCE;
        }
    }
    if (failure != FirmwareFailure::NONE) {
        running_ = false;
        failure_ = failure;
        const bool invalidated = owner_.reportController(
            MotorControlCAN::CANOPEN::ControllerState::RECOVERING, owner_.lastTimeUs());
        evidence_.interrupt(owner_.lastTimeUs());
        // Public app/owner calls can advance time between firmware turns. Never
        // report this failure at an older timestamp or invent a newer one.
        const bool retired = transport_.shutdown();
        faultRetired_ = invalidated && retired;
        return false;
    }
    return true;
}
} // namespace MotorControlCANExample
