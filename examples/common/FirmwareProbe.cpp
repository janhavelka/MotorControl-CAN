// SPDX-License-Identifier: MIT
#include "FirmwareProbe.h"

namespace MotorControlCANExample {
FirmwareProbe::FirmwareProbe(MotorControlCAN::CANOPEN::SdoClient& services, TwaiDriver driver,
                             const ProbeCallbacks& callbacks) noexcept
    : transport_(services, driver), owner_(services, transport_.transport(), 28),
      platform_(callbacks), app_(owner_, platform_),
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
    if (!platform_.nowUs(now) || now < lastUs_ || !app_.service(now)) {
        running_ = false;
        clockFailed_ = true;
        const bool invalidated = owner_.reportController(
            MotorControlCAN::CANOPEN::ControllerState::RECOVERING, owner_.lastTimeUs());
        // Public app/owner calls can advance time between firmware turns. Never
        // report a clock failure at an older timestamp or invent a newer one.
        const bool retired = transport_.shutdown();
        faultRetired_ = invalidated && retired;
        return false;
    }
    lastUs_ = now;
    transport_.service(owner_, now);
    return true;
}
} // namespace MotorControlCANExample
