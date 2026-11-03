// SPDX-License-Identifier: MIT
#include "FirmwareProbe.h"

namespace MotorControlCANExample {
FirmwareProbe::FirmwareProbe(MotorControlCAN::CANOPEN::SdoClient& services, TwaiDriver driver,
                             const ProbeCallbacks& callbacks) noexcept
    : transport_(services, driver), adapter_(transport_.transport()),
      owner_(services, ownerTransport(), 28), platform_(callbacks), app_(owner_, platform_),
      evidence_(app_, owner_), callbacksValid_(callbacks.nowUs && !callbacks.idle) {}
MotorControlCANOwner::Transport FirmwareProbe::ownerTransport() noexcept {
    using MotorControlCANOwner::ReceivedFrame;
    using MotorControlCANOwner::TransportTxId;
    MotorControlCANOwner::Transport t;
    t.context = this;
    // A missing adapter callback stays missing: the owner treats absence as a fact.
    if (adapter_.receive)
        t.receive = [](void* p, ReceivedFrame& frame) {
            const auto& a = static_cast<FirmwareProbe*>(p)->adapter_;
            return a.receive(a.context, frame);
        };
    if (adapter_.transmit)
        t.transmit = [](void* p, const MotorControlCAN::Frame& frame, TransportTxId id, uint32_t epoch) {
            const auto& a = static_cast<FirmwareProbe*>(p)->adapter_;
            return a.transmit(a.context, frame, id, epoch);
        };
    if (adapter_.takeTxReport)
        t.takeTxReport = [](void* p, MotorControlCANOwner::TxReport& report) {
            const auto& a = static_cast<FirmwareProbe*>(p)->adapter_;
            return a.takeTxReport(a.context, report);
        };
    if (adapter_.observe)
        t.observe = [](void* p, const ReceivedFrame& frame, uint64_t now) {
            const auto& a = static_cast<FirmwareProbe*>(p)->adapter_;
            a.observe(a.context, frame, now);
        };
    if (adapter_.retire)
        t.retire = [](void* p, TransportTxId id, uint32_t epoch) {
            const auto& a = static_cast<FirmwareProbe*>(p)->adapter_;
            return a.retire(a.context, id, epoch);
        };
    if (adapter_.beginRecovery)
        t.beginRecovery = [](void* p, uint32_t epoch) {
            const auto& a = static_cast<FirmwareProbe*>(p)->adapter_;
            return a.beginRecovery(a.context, epoch);
        };
    if (adapter_.pollRecovery)
        t.pollRecovery = [](void* p, uint32_t epoch) {
            const auto& a = static_cast<FirmwareProbe*>(p)->adapter_;
            return a.pollRecovery(a.context, epoch);
        };
    t.beforeTransmit = [](void* p, MotorControlCANOwner::SequenceId sequence,
                          const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t now) {
        const auto& self = *static_cast<FirmwareProbe*>(p);
        const auto& a = self.adapter_;
        if (a.beforeTransmit && !a.beforeTransmit(a.context, sequence, transfer, now))
            return false;
        return !self.preflight_.check || self.preflight_.check(self.preflight_.context, sequence, transfer, now);
    };
    return t;
}
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
