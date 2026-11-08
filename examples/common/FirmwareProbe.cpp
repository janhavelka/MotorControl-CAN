// SPDX-License-Identifier: MIT
#include "FirmwareProbe.h"

namespace MotorControlCANExample {
FirmwareConfigCheck checkFirmwareConfig(const FirmwareConfig& config, bool silentDriver) noexcept {
    const TwaiConfig& c = config.controller;
    const TransceiverConfig& t = config.transceiver;
    const bool silent = t.silentLevel != SilentLevel::NONE, pin = t.silentPin >= 0;
    if (!config.configured)
        return FirmwareConfigCheck::NOT_CONFIGURED;
    if (c.txPin < 0 || c.rxPin < 0)
        return FirmwareConfigCheck::MISSING_PIN;
    if (firmwareConsolePin(config.target, c.txPin) || firmwareConsolePin(config.target, c.rxPin) ||
        firmwareConsolePin(config.target, t.silentPin))
        return FirmwareConfigCheck::CONSOLE_PIN;
    if (firmwareSystemPin(config.target, c.txPin) || firmwareSystemPin(config.target, c.rxPin) ||
        firmwareSystemPin(config.target, t.silentPin))
        return FirmwareConfigCheck::SYSTEM_PIN;
    if (c.txPin == c.rxPin || (pin && (t.silentPin == c.txPin || t.silentPin == c.rxPin)))
        return FirmwareConfigCheck::PIN_CONFLICT;
    if (silent != pin || (silent && !silentDriver))
        return FirmwareConfigCheck::SILENT_INCOMPLETE;
    return FirmwareConfigCheck::OK;
}
FirmwareBootStatus firmwareStatus(const FirmwareProbe& firmware, bool platformReady) noexcept {
    if (firmware.progressionFailed())
        return FirmwareBootStatus::PROGRESSION_FAILED;
    if (firmware.stopped())
        return firmware.stopConfirmed() ? FirmwareBootStatus::STOPPED : FirmwareBootStatus::STOP_FAILED;
    if (firmware.running())
        return FirmwareBootStatus::RUNNING;
    if (firmware.started())
        return firmware.retired() ? FirmwareBootStatus::CONTROLLER_FAILED
                                  : FirmwareBootStatus::CONTROLLER_UNRETIRED;
    // Before start, FAILED comes only from prepare's silent request: the level is unknown,
    // which matters more than a console that cannot report it.
    if (firmware.transceiver() == TransceiverState::FAILED)
        return FirmwareBootStatus::TRANSCEIVER_FAILED;
    if (!platformReady || firmware.lastCheck() == FirmwareConfigCheck::INVALID_PLATFORM)
        return FirmwareBootStatus::PLATFORM_FAILED;
    if (firmware.prepared())
        return FirmwareBootStatus::INACTIVE;
    return firmware.lastCheck() == FirmwareConfigCheck::NOT_CONFIGURED
               ? FirmwareBootStatus::CONFIGURATION_REQUIRED
               : FirmwareBootStatus::CONFIGURATION_REJECTED;
}
FirmwareProbe::FirmwareProbe(MotorControlCAN::CANOPEN::SdoClient& services, TwaiDriver driver,
                             const ProbeCallbacks& callbacks, const SilentPin& silent) noexcept
    : services_(services), transport_(services, driver), adapter_(transport_.transport()),
      silent_(silent), owner_(services, ownerTransport(), 28), platform_(callbacks),
      app_(owner_, platform_), evidence_(app_, owner_),
      callbacksValid_(callbacks.nowUs && !callbacks.idle) {}
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
        return self.preflight_.check && self.preflight_.check(self.preflight_.context, sequence, transfer, now);
    };
    return t;
}
bool FirmwareProbe::requestSilent(bool silent) noexcept {
    const bool high = (config_.transceiver.silentLevel == SilentLevel::HIGH) == silent;
    const bool accepted = silent_.drive(silent_.context, config_.transceiver.silentPin, high);
    transceiver_ = !accepted ? TransceiverState::FAILED
                   : silent  ? TransceiverState::SILENT_REQUESTED
                             : TransceiverState::NORMAL_REQUESTED;
    return accepted;
}
bool FirmwareProbe::prepare(const FirmwareConfig& config) noexcept {
    if (attempted_)
        return false;
    attempted_ = true;
    // A refused board drives no GPIO: a conflicting silent pin may be the console itself.
    check_ = callbacksValid_ ? checkFirmwareConfig(config, silent_.drive != nullptr)
                             : FirmwareConfigCheck::INVALID_PLATFORM;
    if (check_ == FirmwareConfigCheck::OK) {
        config_ = config;
        // Silence first, so no later step can make the transceiver drive the bus before the
        // explicit start. Without a silent input the stopped controller is the only inhibitor.
        prepared_ = !controlled() || requestSilent(true);
    }
    // Nothing can start this controller now, so work the owner admitted would never run.
    if (!prepared_)
        invalidate();
    return prepared_;
}
bool FirmwareProbe::start() noexcept {
    if (!prepared_ || started_ || stopped_)
        return false;
    started_ = true;
    retired_ = false; // Even a refused SDK start can leave a partial node to delete.
    running_ = transport_.start(config_.controller, owner_.controllerEpoch());
    if (running_ && (!controlled() || requestSilent(false)))
        return true;
    // A refused start kept the silent level requested. A refused normal level leaves the
    // pin state unknown: ask for silence first, as every failure path does. Then retire as
    // stop does rather than leave a node or admitted work behind.
    if (running_)
        requestSilent(true);
    retire();
    return false;
}
bool FirmwareProbe::invalidate() noexcept {
    // Once per lifetime. Public app/owner calls can advance time between firmware turns:
    // never report this at an older timestamp or invent a newer one.
    if (!invalidated_) {
        const uint64_t now = owner_.lastTimeUs();
        invalidated_ =
            owner_.reportController(MotorControlCAN::CANOPEN::ControllerState::RECOVERING, now);
        evidence_.interrupt(now);
    }
    return invalidated_;
}
bool FirmwareProbe::retire() noexcept {
    // The single retirement of every path: refused start, progression failure and stop.
    running_ = false;
    const bool invalidated = invalidate();
    // Repeat only a shutdown that is still unconfirmed; one that succeeded is final.
    if (!retired_)
        retired_ = transport_.shutdown();
    return invalidated && retired_;
}
bool FirmwareProbe::stop() noexcept {
    if (!prepared_ || stopped_)
        return false;
    stopped_ = true;
    const bool silenced = !controlled() || requestSilent(true);
    stopConfirmed_ = retire() && silenced;
    return stopConfirmed_;
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
        failure_ = failure;
        if (controlled())
            requestSilent(true); // Before shutdown: inhibit at the transceiver first.
        faultRetired_ = retire();
        return false;
    }
    return true;
}
} // namespace MotorControlCANExample
