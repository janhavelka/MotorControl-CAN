// SPDX-License-Identifier: MIT
#include "DiscoveryScan.h"
#include <new>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace C = MotorControlCAN::CANOPEN;
namespace O = MotorControlCANOwner;
namespace {
bool sameInterface(const DiscoveryInterface& a, const DiscoveryInterface& b) {
    return a.protocol == b.protocol && a.bus == b.bus && a.bitrate == b.bitrate;
}
bool sameRoute(const C::SdoChannel& a, const C::SdoChannel& b) {
    return M::sameBinding(a.binding, b.binding) && a.clientToServerId == b.clientToServerId &&
           a.serverToClientId == b.serverToClientId;
}
bool recent(const C::ObservationTime& t, uint64_t now, uint64_t age) {
    uint64_t first = 0, last = 0;
    return C::observationTimeBounds(t, first, last) && last <= now && now - first <= age;
}
} // namespace
DiscoveryScan::DiscoveryScan(ProbeApp& app, CanBusOwner& owner, C::SdoClient& services) noexcept
    : app_(app), owner_(owner), services_(services) {}

bool DiscoveryScan::begin(const DiscoveryPlan& p, const DiscoveryInterface& actual, uint64_t now,
                          uint64_t deadline) noexcept {
    M::DiscoveryCapabilities capabilities;
    if (occupied_ || app_.readContext().state != M::ReadState::EMPTY || now < lastUs_ ||
        now < owner_.lastTimeUs() || deadline <= now || deadline > uint64_t(INT64_MAX) ||
        !p.count || p.count > DiscoveryPlan::CAPACITY || p.mode > DiscoveryMode::IDENTITY ||
        !p.maximumAgeUs || p.interface.protocol != M::Protocol::CANOPEN || !p.interface.bitrate ||
        !sameInterface(p.interface, actual) ||
        !M::getDiscoveryCapabilities(p.profile, capabilities))
        return false;
    const uint8_t needed = p.mode == DiscoveryMode::IDENTITY ? uint8_t(p.count * 9)
                           : p.mode == DiscoveryMode::PROBE  ? p.count
                                                             : 0;
    if (p.maximumServices < needed)
        return false;
    uint8_t slots[DiscoveryPlan::CAPACITY] = {};
    uint32_t sessions[DiscoveryPlan::CAPACITY] = {};
    for (uint8_t i = 0; i < p.count; ++i) {
        const auto& wanted = p.targets[i];
        const auto& route = wanted.target.channel;
        if (route.binding.endpoint.bus != actual.bus ||
            route.binding.endpoint.protocol != actual.protocol ||
            !M::validateBinding(route.binding) ||
            (wanted.expected.firmwareKnown && !wanted.expected.identityKnown))
            return false;
        bool found = false;
        for (uint8_t s = 0; s < C::SdoClient::CHANNELS; ++s) {
            const C::SdoChannel* configured = services_.channel(s);
            const C::NodeSnapshot* node = services_.node(s);
            const bool matched =
                p.mode == DiscoveryMode::PASSIVE
                    ? node && !node->exhausted &&
                          M::sameBinding(node->route.binding, route.binding) &&
                          node->route.generation == wanted.target.nodeGeneration
                    : configured && sameRoute(*configured, route) &&
                          owner_.matchesChannel(route, wanted.target.nodeGeneration);
            if (matched) {
                slots[i] = s;
                sessions[i] = node->serverSessions;
                found = true;
            }
        }
        if (!found || (p.mode != DiscoveryMode::PASSIVE && services_.unresolved(slots[i])))
            return false;
        for (uint8_t j = 0; j < i; ++j)
            if (slots[j] == slots[i])
                return false;
        if (p.mode != DiscoveryMode::PASSIVE &&
            !L::validateRead(p.mode == DiscoveryMode::PROBE ? L::ReadKind::PROBE
                                                            : L::ReadKind::IDENTITY,
                             wanted.target, 1, now, deadline, p.options))
            return false;
    }
    plan_ = p;
    summary_ = DiscoverySummary();
    summary_.targets = p.count;
    for (uint8_t s = 0; s < C::SdoClient::CHANNELS; ++s)
        if (services_.channel(s) || services_.node(s))
            ++summary_.occupiedSlotsAtStart;
    summary_.freeSlotsAtStart = uint8_t(C::SdoClient::CHANNELS - summary_.occupiedSlotsAtStart);
    for (uint8_t i = 0; i < p.count; ++i) {
        new (static_cast<void*>(&findings_[i])) DiscoveryFinding();
        findings_[i].requested = p.targets[i];
        findings_[i].slot = slots[i];
        serverSessions_[i] = sessions[i];
    }
    startedUs_ = lastUs_ = now;
    deadlineUs_ = deadline;
    cursor_ = 0;
    countedRequest_ = O::RequestId();
    controllerEpoch_ = owner_.controllerEpoch();
    historyDropsAtStart_ = services_.droppedNetworkObservations();
    occupied_ = active_ = true;
    startedRead_ = stopping_ = false;
    return true;
}

void DiscoveryScan::passive(uint64_t now) noexcept {
    // This queue contains observer copies AFTER normal routing. Consuming it cannot steal an SDO,
    // heartbeat, PDO or lifecycle event from its normal consumer. Retain boot-up even when a later
    // heartbeat in the same owner turn replaced the node's latest NMT snapshot.
    C::NetworkObservation event;
    for (unsigned count = 0;
         count < C::SdoClient::NETWORK_HISTORY && services_.takeNetworkObservation(event);
         ++count) {
        if (!event.decoded || event.disposition != C::NetworkDisposition::ACCEPTED ||
            !recent(event.time, now, plan_.maximumAgeUs))
            continue;
        for (uint8_t i = 0; i < plan_.count; ++i) {
            DiscoveryFinding& f = findings_[i];
            if (!M::sameBinding(event.route.binding, f.requested.target.channel.binding))
                continue;
            if (event.event.kind == C::NetworkEventKind::BOOTUP &&
                event.time.processedUs >= startedUs_)
                f.bootup = event.time;
        }
    }
    summary_.passiveHistoryDrops = services_.droppedNetworkObservations() - historyDropsAtStart_;
    for (uint8_t i = 0; i < plan_.count; ++i) {
        DiscoveryFinding& f = findings_[i];
        const C::NodeSnapshot* n = services_.node(f.slot);
        if (!n || !M::sameBinding(n->route.binding, f.requested.target.channel.binding))
            continue;
        // These remain copied observations. Boot-up may advance the node epoch; it never makes an
        // active scan's old target current again. The normal owner already handled invalidation.
        if (n->heartbeat.generation == n->route.generation &&
            recent(n->heartbeat, now, plan_.maximumAgeUs))
            f.heartbeat = n->heartbeat;
        if (n->serverSessions > serverSessions_[i] && n->nmt == C::NmtState::INITIALIZING &&
            recent(n->nmtTime, now, plan_.maximumAgeUs))
            f.bootup = n->nmtTime;
        f.suspectedDuplicate |=
            n->nmtOrderingUncertain && recent(n->nmtTime, now, plan_.maximumAgeUs);
        for (uint8_t s = 0; s < C::SdoClient::PDO_ROUTES; ++s) {
            const C::PdoRecord* record = services_.pdo(s);
            C::PdoFreshness freshness;
            if (record && record->map.nodeGeneration == n->route.generation &&
                M::sameBinding(record->map.route.binding, f.requested.target.channel.binding) &&
                C::pdoFreshness(*record, now, plan_.maximumAgeUs, freshness) &&
                freshness == C::PdoFreshness::FRESH) {
                f.pdo = record->observation.time;
                f.mappingGeneration = record->map.route.generation;
            }
        }
    }
}

void DiscoveryScan::finish(uint8_t i, DiscoveryOutcome outcome, uint64_t now) noexcept {
    DiscoveryFinding& f = findings_[i];
    if (f.outcome != DiscoveryOutcome::PENDING)
        return;
    f.outcome = outcome;
    f.completedUs = now;
    f.unresolved = services_.unresolved(f.slot);
    if (f.unresolved)
        ++summary_.unresolvedSlots;
    ++summary_.completed;
}
void DiscoveryScan::classify(DiscoveryFinding& f, uint64_t now) noexcept {
    const L::ReadContext& r = f.read;
    if (r.state == M::ReadState::SUCCEEDED) {
        if (!owner_.matchesChannel(f.requested.target.channel, f.requested.target.nodeGeneration) ||
            owner_.controllerEpoch() != controllerEpoch_) {
            finish(cursor_, DiscoveryOutcome::INVALIDATED, now);
            return;
        }
        if (plan_.mode == DiscoveryMode::PROBE) {
            L::ProbeObservation probe;
            if (!L::checkProbe(r, f.requested.target, now, plan_.maximumAgeUs, probe)) {
                finish(cursor_, DiscoveryOutcome::INVALIDATED, now);
                return;
            }
            f.confidence = DiscoveryConfidence::RESPONDER_ONLY;
            finish(cursor_, DiscoveryOutcome::PRESENCE, now);
        } else {
            L::IdentityObservation identity;
            if (!L::getIdentity(r, f.requested.target, now, plan_.maximumAgeUs, identity)) {
                finish(cursor_, DiscoveryOutcome::INVALIDATED, now);
                return;
            }
            f.confidence = DiscoveryConfidence::RAW_IDENTITY;
            const DiscoveryExpectation& e = f.requested.expected;
            if (e.identityKnown &&
                (identity.vendorId != e.vendorId || identity.productCode != e.productCode ||
                 identity.modelCode != e.modelCode))
                finish(cursor_, DiscoveryOutcome::CONTRADICTORY_IDENTITY, now);
            else if (e.firmwareKnown &&
                     (identity.revision != e.revision || identity.firmwareCode != e.firmwareCode))
                finish(cursor_, DiscoveryOutcome::UNSUPPORTED_FIRMWARE, now);
            else {
                if (e.identityKnown)
                    f.confidence = DiscoveryConfidence::EXPECTATION_MATCH;
                finish(cursor_, DiscoveryOutcome::GENERIC_IDENTITY, now);
            }
        }
    } else {
        DiscoveryOutcome outcome = DiscoveryOutcome::SERVICE_ERROR;
        switch (r.outcome) {
        case M::ReadOutcome::SERVICE_TIMEOUT:
            outcome = DiscoveryOutcome::NO_RESPONSE;
            break;
        case M::ReadOutcome::DEADLINE:
            outcome = DiscoveryOutcome::DEADLINE;
            break;
        case M::ReadOutcome::CANCELLED:
            outcome = DiscoveryOutcome::CANCELLED;
            break;
        case M::ReadOutcome::INVALIDATED:
            outcome = DiscoveryOutcome::INVALIDATED;
            break;
        case M::ReadOutcome::PROTOCOL_REJECTED:
            outcome = DiscoveryOutcome::PROTOCOL_REJECTED;
            f.confidence = DiscoveryConfidence::RESPONDER_ONLY;
            break;
        default:
            break;
        }
        finish(cursor_, outcome, now);
    }
}
void DiscoveryScan::stop(DiscoveryOutcome reason, uint64_t now) noexcept {
    stopping_ = true;
    stopReason_ = reason;
    if (startedRead_ && app_.readContext().state == M::ReadState::ACTIVE)
        app_.cancelRead(now);
}
bool DiscoveryScan::cancel(uint64_t now) noexcept {
    if (!active_ || now < lastUs_ || now < owner_.lastTimeUs() || now > uint64_t(INT64_MAX))
        return false;
    summary_.cancelled = true;
    stop(now >= deadlineUs_ ? DiscoveryOutcome::DEADLINE : DiscoveryOutcome::CANCELLED, now);
    lastUs_ = now;
    return true;
}

bool DiscoveryScan::service(const DiscoveryInterface& actual, uint64_t now) noexcept {
    if (now < lastUs_ || now < owner_.lastTimeUs() || now > uint64_t(INT64_MAX))
        return false;
    if (!active_)
        return app_.service(now);
    if (!sameInterface(plan_.interface, actual) || owner_.controllerEpoch() != controllerEpoch_) {
        summary_.interfaceChanged = true;
        stop(DiscoveryOutcome::INVALIDATED, now);
    }
    if (now >= deadlineUs_) {
        summary_.deadlineExpired = true;
        stop(DiscoveryOutcome::DEADLINE, now);
    }
    if (!app_.service(now, plan_.serviceBudget))
        return false;
    return advance(actual, now);
}

bool DiscoveryScan::advance(const DiscoveryInterface& actual, uint64_t now) noexcept {
    if (now < lastUs_ || now != owner_.lastTimeUs() || now > uint64_t(INT64_MAX))
        return false;
    if (!active_)
        return true;
    if (!sameInterface(plan_.interface, actual) || owner_.controllerEpoch() != controllerEpoch_) {
        summary_.interfaceChanged = true;
        stop(DiscoveryOutcome::INVALIDATED, now);
    }
    if (now >= deadlineUs_) {
        summary_.deadlineExpired = true;
        stop(DiscoveryOutcome::DEADLINE, now);
    }
    lastUs_ = now;
    const O::RequestId pending = app_.pendingRequest();
    if (pending.owner &&
        (pending.owner != countedRequest_.owner || pending.slot != countedRequest_.slot ||
         pending.generation != countedRequest_.generation)) {
        ++summary_.servicesAdmitted;
        countedRequest_ = pending;
    }
    // Owner dispatch continues after cancellation/deadline, but this scan's observation window is
    // already closed. Earlier capture does not rescue evidence processed at the cutoff.
    if (!stopping_)
        passive(now);
    if (plan_.mode == DiscoveryMode::PASSIVE && !stopping_)
        return true;
    // Consume at most one owner result. It is copied by advanceRead before its slot is released.
    if (app_.pendingRequest().owner)
        app_.advanceRead(now);
    const M::ReadState state = app_.readContext().state;
    if (startedRead_ && state != M::ReadState::ACTIVE) {
        findings_[cursor_].read = app_.readContext();
        if (stopping_)
            finish(cursor_, stopReason_, now);
        else
            classify(findings_[cursor_], now);
        if (!app_.releaseRead())
            return false;
        startedRead_ = false;
        ++cursor_;
    }
    if (stopping_) {
        if (startedRead_)
            return true;
        for (; cursor_ < plan_.count; ++cursor_) {
            DiscoveryOutcome outcome = stopReason_;
            if (plan_.mode == DiscoveryMode::PASSIVE && !summary_.cancelled &&
                !summary_.interfaceChanged) {
                const auto& f = findings_[cursor_];
                outcome = f.heartbeat.present || f.bootup.present || f.pdo.present
                              ? DiscoveryOutcome::PRESENCE
                              : DiscoveryOutcome::NO_RESPONSE;
                if (outcome == DiscoveryOutcome::PRESENCE)
                    findings_[cursor_].confidence = DiscoveryConfidence::RESPONDER_ONLY;
                ++summary_.attempted;
            }
            finish(cursor_, outcome, now);
        }
    } else if (cursor_ < plan_.count && !startedRead_) {
        const auto& t = plan_.targets[cursor_].target;
        const bool prepared =
            app_.select(t) && (plan_.mode == DiscoveryMode::PROBE
                                   ? app_.requestProbe(now, deadlineUs_, plan_.options)
                                   : app_.requestIdentity(now, deadlineUs_, plan_.options));
        ++summary_.attempted;
        if (!prepared) {
            finish(cursor_, DiscoveryOutcome::LOCAL_REJECTION, now);
            ++cursor_;
        } else
            startedRead_ = true;
    }
    if (cursor_ == plan_.count && !startedRead_) {
        summary_.unresolvedSlots = 0;
        for (uint8_t i = 0; i < plan_.count; ++i) {
            DiscoveryFinding& f = findings_[i];
            f.unresolved = services_.unresolved(f.slot);
            const C::NodeSnapshot* node = services_.node(f.slot);
            f.bindingCurrent =
                owner_.controllerEpoch() == controllerEpoch_ &&
                (plan_.mode == DiscoveryMode::PASSIVE
                     ? node && !node->exhausted &&
                           M::sameBinding(node->route.binding,
                                          f.requested.target.channel.binding) &&
                           node->route.generation == f.requested.target.nodeGeneration
                     : owner_.matchesChannel(f.requested.target.channel,
                                             f.requested.target.nodeGeneration));
            if (f.unresolved)
                ++summary_.unresolvedSlots;
            // A completed first target can reboot while another is scanned. Preserve its raw read
            // but do not publish that historical success as a current selection at batch completion.
            if (plan_.mode != DiscoveryMode::PASSIVE &&
                (f.outcome == DiscoveryOutcome::PRESENCE ||
                 f.outcome == DiscoveryOutcome::GENERIC_IDENTITY) &&
                !f.bindingCurrent) {
                f.outcome = DiscoveryOutcome::INVALIDATED;
                f.confidence = DiscoveryConfidence::NONE;
            }
        }
        active_ = false;
    }
    return true;
}
const DiscoveryFinding* DiscoveryScan::result(uint8_t index) const noexcept {
    return complete() && index < plan_.count ? &findings_[index] : nullptr;
}
bool DiscoveryScan::release() noexcept {
    if (!complete())
        return false;
    occupied_ = false;
    return true;
}
} // namespace MotorControlCANExample
