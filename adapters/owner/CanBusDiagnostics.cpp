// SPDX-License-Identifier: MIT
#include "CanBusOwner.h"
#include "OwnerCounters.h"

namespace MotorControlCANOwner {
namespace C = MotorControlCAN::CANOPEN;
namespace {
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool sameIdentity(const Binding& a, const Binding& b) {
    Binding candidate = a;
    candidate.configurationGeneration = b.configurationGeneration;
    return MotorControlCAN::sameBinding(candidate, b);
}
MotorControlCAN::Status failure(C::SdoOutcome outcome) {
    using MotorControlCAN::Err;
    const Err code = outcome == C::SdoOutcome::SERVER_ABORT   ? Err::PROTOCOL_ERROR
                     : outcome == C::SdoOutcome::CLIENT_ABORT ? Err::FRAME_ERROR
                                                              : Err::INVALID_CONFIG;
    return MotorControlCAN::Status(code, static_cast<int32_t>(outcome), "CANopen service failed");
}
bool attempted(const Completion& completion) {
    if (completion.reason != CompletionReason::SERVICE || !completion.service.transmissionPossible)
        return false;
    switch (completion.service.outcome) {
    case C::SdoOutcome::CANCELLED:
    case C::SdoOutcome::UNSUPPORTED:
    case C::SdoOutcome::BINDING_INVALIDATED:
        return false;
    default:
        return true;
    }
}
bool current(const C::ObservationTime& time, const C::NodeSnapshot& node, uint64_t now) {
    uint64_t earliest = 0, latest = 0;
    return !node.exhausted && time.present && time.generation == node.route.generation &&
           time.processedUs <= now && C::observationTimeBounds(time, earliest, latest) &&
           latest <= now && now - earliest <= node.maximumEvidenceAgeUs;
}
} // namespace

int CanBusOwner::healthSlot(const Binding& binding) const noexcept {
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        const auto* node = services_.node(uint8_t(i));
        const Binding* configured = route ? &route->binding : node ? &node->route.binding : nullptr;
        if (configured && MotorControlCAN::sameBinding(*configured, binding))
            return int(i);
    }
    return -1;
}
void CanBusOwner::refreshHealth(uint64_t now) noexcept {
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        const auto* node = services_.node(uint8_t(i));
        const Binding* binding = route ? &route->binding : node ? &node->route.binding : nullptr;
        HealthRecord& record = health_[i];
        if (!binding) {
            record = HealthRecord();
            continue;
        }
        if (!record.bound || !sameIdentity(record.binding, *binding)) {
            record = HealthRecord();
            record.bound = true;
        }
        record.binding = *binding; // Configuration-only changes preserve communication history.
        Health& health = record.value;
        bool evidence = false;
        auto observe = [&](const C::ObservationTime& time) {
            uint64_t earliest = 0, latest = 0;
            if (!node || node->exhausted || !time.present ||
                time.generation != node->route.generation || time.processedUs > now ||
                (time.receivedTimeKnown && time.receivedUs > now) ||
                (time.receiveBoundsKnown &&
                 (!C::observationTimeBounds(time, earliest, latest) || latest > now)))
                return;
            if (!health.lastSeenKnown || time.processedUs > health.lastSeenUs) {
                health.lastSeenUs = time.processedUs;
                health.lastSeenKnown = true;
            }
            if (current(time, *node, now))
                evidence = true;
        };
        if (node) {
            observe(node->sdo);
            observe(node->heartbeat);
            observe(node->nmtTime);
            observe(node->emcyTime);
            for (unsigned p = 0; p < C::SdoClient::PDO_ROUTES; ++p) {
                const auto* value = services_.pdo(uint8_t(p));
                if (!value || !value->confirmed || !value->qualified || !value->samplePresent ||
                    value->map.nodeGeneration != node->route.generation ||
                    !MotorControlCAN::sameBinding(value->map.route.binding, *binding))
                    continue;
                C::PdoFreshness freshness;
                if (!C::pdoFreshness(*value, now, node->maximumEvidenceAgeUs, freshness) ||
                    (freshness != C::PdoFreshness::FRESH && freshness != C::PdoFreshness::EXPIRED))
                    continue;
                // A qualified late delivery is history; observe() checks capture age
                // separately before it can establish current communication evidence.
                C::ObservationTime time = value->observation.time;
                // PDO time's generation is its mapping epoch, already checked by the store.
                time.generation = node->route.generation;
                observe(time);
            }
        }
        bool expired = false;
        if (node && record.heartbeatMaximumAgeUs) {
            C::HeartbeatFreshness freshness;
            expired = C::heartbeatFreshness(*node, now, record.heartbeatMaximumAgeUs, freshness) &&
                      freshness == C::HeartbeatFreshness::EXPIRED;
        }
        const auto controller = services_.controllerState();
        health.state = inhibited_ || controller == C::ControllerState::BUS_OFF ||
                               controller == C::ControllerState::RECOVERING || expired
                           ? DriverState::OFFLINE
                       : health.consecutiveFailures ? DriverState::DEGRADED
                       : evidence                   ? DriverState::READY
                                                    : DriverState::UNINIT;
    }
}
void CanBusOwner::recordTerminal(const Slot& slot) noexcept {
    const Completion& completion = slot.completion;
    if (slot.localCancellation)
        detail::add(stats_.cancelled);
    if (completion.service.outcome == C::SdoOutcome::TIMEOUT)
        detail::add(stats_.timeouts);
    refreshHealth(lastUs_);
    const int index = healthSlot(completion.service.transfer.channel.binding);
    if (index >= 0 && attempted(completion)) {
        Health& health = health_[index].value;
        if (completion.service.outcome == C::SdoOutcome::SUCCESS) {
            detail::add(health.totalSuccess);
            health.consecutiveFailures = 0;
            health.lastOkUs = completion.service.completedUs;
            health.lastOkKnown = true;
        } else {
            detail::add(health.totalFailures);
            detail::add(health.consecutiveFailures);
            health.lastErrorUs = completion.service.completedUs;
            health.lastErrorKnown = true;
            health.lastError = failure(completion.service.outcome);
        }
    }
    refreshHealth(lastUs_);
    Diagnostic record;
    record.kind = DiagnosticKind::TERMINAL;
    record.processedUs = completion.service.completedUs;
    record.controllerEpoch = controllerEpoch_;
    record.completion = completion;
    trace(record);
}
bool CanBusOwner::health(const Binding& binding, Health& output) const noexcept {
    if (overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &binding, sizeof binding))
        return false;
    const int index = healthSlot(binding);
    if (index < 0)
        return false;
    output = health_[index].bound && sameIdentity(health_[index].binding, binding)
                 ? health_[index].value
                 : Health();
    return true;
}
bool CanBusOwner::driverState(const Binding& binding, DriverState& output) const noexcept {
    if (overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &binding, sizeof binding))
        return false;
    Health value;
    if (!health(binding, value))
        return false;
    output = value.state;
    return true;
}
bool CanBusOwner::clearStats() noexcept {
    if (entered_)
        return false;
    stats_ = Stats();
    for (HealthRecord& record : health_) {
        record.value.totalSuccess = 0;
        record.value.totalFailures = 0;
    }
    return true;
}
bool CanBusOwner::setHeartbeatMaximumAge(const Binding& binding, uint64_t maximumAge,
                                         uint64_t now) noexcept {
    const int index = healthSlot(binding);
    if (entered_ || index < 0 || now < lastUs_)
        return false;
    refreshHealth(now);
    health_[index].heartbeatMaximumAgeUs = maximumAge;
    lastUs_ = now;
    refreshHealth(now);
    return true;
}
void CanBusOwner::trace(const Diagnostic& record) noexcept {
    if (traceCount_ == TRACE_CAPACITY) {
        traceHead_ = uint8_t((traceHead_ + 1) % TRACE_CAPACITY);
        --traceCount_;
        detail::add(stats_.traceOverwritten);
    }
    trace_[(traceHead_ + traceCount_) % TRACE_CAPACITY] = record;
    ++traceCount_;
}
bool CanBusOwner::takeDiagnostic(Diagnostic& output) noexcept {
    if (entered_ || !traceCount_ || overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_))
        return false;
    output = trace_[traceHead_];
    traceHead_ = uint8_t((traceHead_ + 1) % TRACE_CAPACITY);
    --traceCount_;
    return true;
}
OwnerResources CanBusOwner::resources() const noexcept {
    OwnerResources value;
    for (const Slot& slot : slots_)
        if (slot.occupied) {
            if (slot.terminal)
                ++value.retained;
            else if (slot.submitted)
                ++value.active;
            else
                ++value.queued;
        }
    for (const Reservation& sequence : sequences_)
        if (sequence.occupied)
            ++value.reservations;
    value.traceUsed = traceCount_;
    value.inhibited = inhibited_;
    value.recoveryPending = recoveryOccupied_ && !recoveryTerminal_;
    value.nmtRetained = nmtOccupied_ && services_.nmtResult(nmtId_);
    value.nmtPending = nmtOccupied_ && !value.nmtRetained;
    value.nmtTxPending = nmtExternal_.pending || nmtExternal_.blocked;
    return value;
}
} // namespace MotorControlCANOwner
