// SPDX-License-Identifier: MIT
#include "CanBusOwner.h"
#include "OwnerCounters.h"

namespace MotorControlCANOwner {
namespace C = MotorControlCAN::CANOPEN;
namespace {
using detail::overlap;
bool sameIdentity(const Binding& a, const Binding& b) {
    Binding candidate = a;
    candidate.configurationGeneration = b.configurationGeneration;
    return MotorControlCAN::sameBinding(candidate, b);
}
bool sameNode(const Binding& a, const Binding& b) {
    return a.endpoint.protocol == b.endpoint.protocol && a.endpoint.bus == b.endpoint.bus &&
           a.endpoint.address == b.endpoint.address;
}
MotorControlCAN::Status failure(C::SdoOutcome outcome) {
    using MotorControlCAN::Err;
    const Err code = outcome == C::SdoOutcome::SERVER_ABORT   ? Err::PROTOCOL_ERROR
                     : outcome == C::SdoOutcome::CLIENT_ABORT ? Err::FRAME_ERROR
                                                              : Err::INVALID_CONFIG;
    return MotorControlCAN::Status(code, static_cast<int32_t>(outcome), "CANopen service failed");
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
    // A bus-level fault names no node (rule C2): the rows keep their last projection, marked.
    const bool offline = busFault();
    if (offline && !interfaceOffline_)
        offlineSinceUs_ = now;
    interfaceOffline_ = offline;
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
        if (offline) {
            if (!health.notObserved) {
                health.notObserved = true;
                health.notObservedSinceUs = offlineSinceUs_;
            }
        } else {
            health.notObserved = false;
            health.state = expired                     ? DriverState::OFFLINE
                           : health.consecutiveFailures ? DriverState::DEGRADED
                           : evidence                   ? DriverState::READY
                                                        : DriverState::UNINIT;
        }
        health.reusePolicy = resync_[i].reuse.policy;
        health.reuseStopped = route && services_.reuseRefused(uint8_t(i));
        health.resyncPending = route && resync_[i].pending;
        health.acceptedCorrelation = route && markActive(i);
        health.noListener = noListener_;
    }
}
// Observations only: the owner changes no state, admission or policy from them.
void CanBusOwner::observePresence(const ReceivedFrame& input, bool accepted, uint64_t now) noexcept {
    const Frame& f = input.frame;
    if (f.extended || f.rtr || f.fd)
        return;
    for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c) {
        const auto* route = services_.channel(uint8_t(c));
        const auto* node = services_.node(uint8_t(c));
        const bool response = route && f.id == route->serverToClientId;
        // The adapter accepted it as a checked boot-up (state byte 0) or heartbeat.
        const bool network = !response && node && accepted && f.id == node->route.heartbeatId &&
                             f.payloadLength == 1;
        if (!response && !network)
            continue;
        const Binding& origin = route ? route->binding : node->route.binding;
        for (HealthRecord& record : health_) {
            if (!record.bound || !sameNode(record.binding, origin))
                continue;
            Health& health = record.value;
            if (response) {
                health.lastResponseUs = now;
                health.lastResponseKnown = true;
                health.consecutiveNoResponse = 0;
            } else if (f.data[0] == 0) {
                detail::add(health.bootUps);
                health.lastBootUpUs = now;
                health.lastBootUpKnown = true;
            } else {
                // A heartbeat is an answer (rule C3): it ends the miss streak, counts no success.
                health.lastHeartbeatUs = now;
                health.lastHeartbeatKnown = true;
                health.consecutiveNoResponse = 0;
            }
        }
        if (response && f.payloadLength == 8 && f.data[0] == 0x80) {
            detail::add(stats_.abortsReceived);
            stats_.lastAbortReceivedUs = now;
            countNode(c, &Health::abortsReceived, &Health::lastAbortReceivedUs, now);
        }
        return; // Identifiers are unique on this interface, and peers share one node anchor.
    }
}
// One class per terminal (rule C3). Only the server's own frames make it answered, and only a
// confirmed request frame that waited in vain makes it a miss: no ACK is bus evidence (C2).
EvidenceClass CanBusOwner::classify(const Slot& slot) const noexcept {
    const Completion& completion = slot.completion;
    if (completion.reason != CompletionReason::SERVICE || slot.bestEffortStop || busFault())
        return EvidenceClass::LOCAL_END;
    const C::SdoAnswer answer = services_.answer(slot.native);
    switch (completion.service.outcome) {
    case C::SdoOutcome::SUCCESS:
        return EvidenceClass::ANSWERED;
    case C::SdoOutcome::SERVER_ABORT:
    case C::SdoOutcome::CLIENT_ABORT:
    case C::SdoOutcome::CORRELATION_LOST:
        return answer == C::SdoAnswer::FRAME ? EvidenceClass::ANSWERED_WITH_ERROR
                                             : EvidenceClass::LOCAL_END;
    case C::SdoOutcome::TIMEOUT:
        return answer == C::SdoAnswer::FRAME      ? EvidenceClass::ANSWERED_WITH_ERROR
               : answer == C::SdoAnswer::NO_FRAME ? EvidenceClass::NO_ANSWER
                                                  : EvidenceClass::LOCAL_END;
    default: // Cancelled, unsupported, boot-up, invalidated, bus-off, transport, unconfirmed stop.
        return EvidenceClass::LOCAL_END;
    }
}
void CanBusOwner::countNode(unsigned c, uint32_t Health::*count, uint64_t Health::*last,
                            uint64_t now) noexcept {
    const auto* route = services_.channel(uint8_t(c));
    const auto* node = services_.node(uint8_t(c));
    if (!route && !node)
        return;
    const Binding& origin = route ? route->binding : node->route.binding;
    for (HealthRecord& record : health_)
        if (record.bound && sameNode(record.binding, origin)) {
            detail::add(record.value.*count);
            record.value.*last = now;
        }
}
void CanBusOwner::recordTerminal(Slot& slot) noexcept {
    Completion& completion = slot.completion;
    if (slot.localCancellation)
        detail::add(stats_.cancelled);
    if (completion.service.outcome == C::SdoOutcome::TIMEOUT)
        detail::add(stats_.timeouts);
    completion.evidence = classify(slot);
    refreshHealth(lastUs_);
    const int index = healthSlot(completion.service.transfer.channel.binding);
    if (index >= 0 && completion.evidence != EvidenceClass::LOCAL_END) {
        Health& health = health_[index].value;
        if (completion.evidence == EvidenceClass::ANSWERED) {
            detail::add(health.totalSuccess);
            health.consecutiveFailures = 0;
            health.lastOkUs = completion.service.completedUs;
            health.lastOkKnown = true;
        } else {
            detail::add(completion.evidence == EvidenceClass::NO_ANSWER ? health.totalMisses
                                                                        : health.totalErrors);
            detail::add(health.totalFailures);
            detail::add(health.consecutiveFailures);
            health.lastErrorUs = completion.service.completedUs;
            health.lastErrorKnown = true;
            health.lastError = failure(completion.service.outcome);
        }
        // The miss streak is the node's: peers on the same node share it.
        for (HealthRecord& record : health_)
            if (record.bound &&
                sameNode(record.binding, completion.service.transfer.channel.binding)) {
                if (completion.evidence == EvidenceClass::NO_ANSWER)
                    detail::add(record.value.consecutiveNoResponse);
                else
                    record.value.consecutiveNoResponse = 0;
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
        Health& health = record.value;
        health.totalSuccess = health.totalErrors = health.totalMisses = health.totalFailures = 0;
        health.abortsSent = health.abortsReceived = health.resyncs = 0;
        health.lastAbortSentUs = health.lastAbortReceivedUs = health.lastResyncUs = 0;
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
    value.syncArmed = syncArmed_;
    value.syncConflict = syncConflict_;
    value.syncPending = syncOccupied_ && !syncTerminal_;
    value.syncRetained = syncOccupied_ && syncTerminal_;
    value.syncTxPending = syncExternal_.pending || syncExternal_.blocked;
    value.nmtRetained = nmtOccupied_ && services_.nmtResult(nmtId_);
    value.nmtPending = nmtOccupied_ && !value.nmtRetained;
    value.nmtTxPending = nmtExternal_.pending || nmtExternal_.blocked;
    value.noListener = noListener_;
    value.receiveLost = receiveLost_;
    value.interfaceState = busFault() ? InterfaceState::OFFLINE : InterfaceState::AVAILABLE;
    value.offlineSinceUs = !busFault() ? 0 : interfaceOffline_ ? offlineSinceUs_ : lastUs_;
    for (const ExternalTx& tx : external_)
        value.txExpired = uint8_t(value.txExpired + (tx.pending && tx.expired ? 1 : 0));
    value.txExpired = uint8_t(value.txExpired + (nmtExternal_.pending && nmtExternal_.expired ? 1 : 0) +
                              (syncExternal_.pending && syncExternal_.expired ? 1 : 0));
    return value;
}
} // namespace MotorControlCANOwner
