// SPDX-License-Identifier: MIT
#include "TwaiAdapter.h"

namespace MotorControlCANExample {
using MotorControlCANOwner::sameTransportTxId;
namespace {
uint32_t add(uint32_t a, uint32_t b) {
    return b > UINT32_MAX - a ? UINT32_MAX : a + b;
}
} // namespace
TwaiAdapter::TwaiAdapter(MotorControlCAN::CANOPEN::SdoClient& services, TwaiDriver driver) noexcept
    : services_(services), driver_(driver) {}
bool TwaiAdapter::start(const TwaiConfig& config, uint32_t epoch) noexcept {
    if (attempted_ || epoch == 0 || config.txPin < 0 || config.rxPin < 0 ||
        config.txPin == config.rxPin || !config.bitrate || config.samplePointPermill >= 1000 ||
        config.sdkTxQueueDepth != 1 || config.rxQueueDepth == 0 || config.rxQueueDepth > 16 ||
        !driver_.start || !driver_.stop || !driver_.submit || !driver_.events || !driver_.receive)
        return false;
    config_ = config;
    attempted_ = true;
    epoch_ = epoch;
    initialized_ = driver_.start(driver_.context, config_, epoch_);
    diagnostics_.inhibited = !initialized_;
    return initialized_;
}
bool TwaiAdapter::shutdown() noexcept {
    diagnostics_.inhibited = true;
    if (attempted_ && !driver_.stop(driver_.context))
        return false;
    for (auto& slot : slots_)
        slot = Slot();
    initialized_ = false;
    recovery_ = Recovery::IDLE;
    return true;
}
Transport TwaiAdapter::transport() noexcept {
    Transport result;
    result.context = this;
    result.receive = [](void* p, ReceivedFrame& frame) {
        auto& self = *static_cast<TwaiAdapter*>(p);
        return self.initialized_ && !self.diagnostics_.inhibited &&
               self.driver_.receive(self.driver_.context, frame, self.now_);
    };
    result.transmit = [](void* p, const MotorControlCAN::Frame& frame,
                         TransportTxId id, uint32_t epoch) {
        return static_cast<TwaiAdapter*>(p)->enqueue(frame, id, epoch);
    };
    result.takeTxReport = [](void* p, TxReport& report) {
        auto& self = *static_cast<TwaiAdapter*>(p);
        for (auto& slot : self.slots_)
            if (slot.state == State::REPORT) {
                report = slot.report;
                slot = Slot();
                return true;
            }
        return false;
    };
    result.retire = [](void* p, TransportTxId id, uint32_t epoch) {
        return static_cast<TwaiAdapter*>(p)->retire(id, epoch);
    };
    result.beginRecovery = [](void* p, uint32_t epoch) {
        return static_cast<TwaiAdapter*>(p)->beginRecovery(epoch);
    };
    result.pollRecovery = [](void* p, uint32_t epoch) {
        return static_cast<TwaiAdapter*>(p)->pollRecovery(epoch);
    };
    return result;
}
bool TwaiAdapter::enqueue(const MotorControlCAN::Frame& frame, TransportTxId id,
                          uint32_t epoch) noexcept {
    if (!initialized_ || diagnostics_.inhibited || epoch != epoch_ || serial_ == UINT32_MAX ||
        !MotorControlCAN::validateFrame(frame))
        return false;
    for (const auto& slot : slots_)
        if (slot.state != State::FREE && sameTransportTxId(slot.report.id, id))
            return false;
    for (auto& slot : slots_)
        if (slot.state == State::FREE) {
            slot.frame = frame;
            slot.report.id = id;
            slot.report.controllerEpoch = epoch;
            slot.serial = ++serial_;
            slot.state = State::QUEUED;
            // Queued here means no SDK contact yet: no possible effect or response clock.
            if (id.kind == TransportTxId::Kind::NMT)
                services_.holdNmtHandoff(id.nmt);
            else if (id.kind == TransportTxId::Kind::SDO)
                services_.holdHandoff(id.sdo);
            return true;
        }
    return false;
}
bool TwaiAdapter::retire(TransportTxId id, uint32_t epoch) noexcept {
    for (auto& slot : slots_)
        if (slot.state != State::FREE && sameTransportTxId(slot.report.id, id) &&
            slot.report.controllerEpoch == epoch) {
            if (slot.state == State::ACTIVE)
                return false;
            slot = Slot();
        }
    return true;
}
bool TwaiAdapter::acceptTime(CanBusOwner& owner, uint64_t now) noexcept {
    if (haveTime_ && now < now_) {
        diagnostics_.inhibited = true;
        // Explicit owner calls can advance time between adapter turns. A clock
        // fault must invalidate at that authority, never a rejected older time.
        owner.reportController(MotorControlCAN::CANOPEN::ControllerState::RECOVERING,
                               owner.lastTimeUs());
        return false;
    }
    now_ = now;
    haveTime_ = true;
    return initialized_ && recovery_ == Recovery::IDLE;
}
void TwaiAdapter::pollEvents(CanBusOwner& owner, uint64_t now) noexcept {
    if (!acceptTime(owner, now))
        return;
    const TwaiEvents events = driver_.events(driver_.context);
    diagnostics_.rxDropped = add(diagnostics_.rxDropped, events.rxDropped);
    diagnostics_.busErrors = add(diagnostics_.busErrors, events.busErrors);
    diagnostics_.errorFlags |= events.errorFlags;
    if (events.rxDropped) {
        diagnostics_.inhibited = true;
        owner.receiveOverflow(now, events.rxDropped);
    }
    bool fault = events.fault;
    if (events.txDone) {
        Slot* active = nullptr;
        for (auto& slot : slots_)
            if (slot.state == State::ACTIVE)
                active = &slot;
        if (!active || active->serial != events.serial)
            fault = true;
        else {
            active->report.transmitted = events.transmitted;
            active->state = State::REPORT;
        }
    }
    if (events.busOff || fault) {
        diagnostics_.inhibited = true;
        owner.reportController(events.busOff
                                   ? MotorControlCAN::CANOPEN::ControllerState::BUS_OFF
                                   : MotorControlCAN::CANOPEN::ControllerState::RECOVERING,
                               now);
    }
}
void TwaiAdapter::promote(CanBusOwner& owner, uint64_t now) noexcept {
    if (!acceptTime(owner, now) || diagnostics_.inhibited)
        return;
    Slot* next = nullptr;
    for (auto& slot : slots_) {
        if (slot.state == State::ACTIVE)
            return;
        if (slot.state == State::QUEUED && (!next || slot.serial < next->serial))
            next = &slot;
    }
    if (!next)
        return;
    const TransportTxId id = next->report.id;
    const bool eligible = id.kind == TransportTxId::Kind::NMT
        ? services_.canTransmitNmt(id.nmt, now)
        : id.kind == TransportTxId::Kind::SDO && services_.canTransmit(id.sdo, now);
    if (!eligible) {
        next->report.provedUnsent = true;
        next->state = State::REPORT;
        return;
    }
    // Mark active before submit: the SDK can interrupt with completion immediately.
    next->state = State::ACTIVE;
    const TwaiSubmit result = driver_.submit(driver_.context, next->frame, next->serial);
    if (result == TwaiSubmit::DEFERRED) {
        next->state = State::QUEUED;
        return;
    }
    if (result == TwaiSubmit::FAILED_UNSENT) {
        next->report.provedUnsent = true;
        next->state = State::REPORT;
        return;
    }
    if (id.kind == TransportTxId::Kind::NMT)
        services_.noteNmtSubmitted(id.nmt, now);
    else if (id.kind == TransportTxId::Kind::SDO)
        services_.noteSubmitted(id.sdo, now);
    if (result == TwaiSubmit::FAILED_UNCERTAIN)
        next->state = State::REPORT;
}
bool TwaiAdapter::beginRecovery(uint32_t epoch) noexcept {
    if (!attempted_ || epoch <= epoch_)
        return false;
    diagnostics_.inhibited = true;
    recoveryEpoch_ = epoch;
    recovery_ = Recovery::STOP;
    return true;
}
RecoveryProgress TwaiAdapter::pollRecovery(uint32_t epoch) noexcept {
    if (epoch != recoveryEpoch_)
        return RecoveryProgress::FAILED;
    if (recovery_ == Recovery::STOP) {
        if (!driver_.stop(driver_.context)) {
            recovery_ = Recovery::FAILED;
            return RecoveryProgress::FAILED;
        }
        initialized_ = false;
        for (auto& slot : slots_)
            slot = Slot();
        recovery_ = Recovery::START;
        return RecoveryProgress::PENDING;
    }
    if (recovery_ == Recovery::START) {
        if (!driver_.start(driver_.context, config_, epoch)) {
            recovery_ = Recovery::FAILED;
            return RecoveryProgress::FAILED;
        }
        epoch_ = epoch;
        initialized_ = true;
        diagnostics_.inhibited = false;
        recovery_ = Recovery::IDLE;
        return RecoveryProgress::RETIRED;
    }
    return RecoveryProgress::FAILED;
}
} // namespace MotorControlCANExample
