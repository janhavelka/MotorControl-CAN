// SPDX-License-Identifier: MIT
#include "SdoClient.h"
#include <cstring>

namespace MotorControlCAN {
namespace CANOPEN {
namespace {
bool sameNode(const Binding& a, const Binding& b) {
    return a.endpoint.bus == b.endpoint.bus && a.endpoint.address == b.endpoint.address;
}
bool sameRoute(const NetworkRoute& a, const NetworkRoute& b) {
    return sameBinding(a.binding, b.binding) && a.generation == b.generation &&
           a.heartbeatId == b.heartbeatId && a.emcyId == b.emcyId;
}
bool validObserver(NetworkObserver observer) {
    return (observer.context == nullptr) == (observer.invalidate == nullptr);
}
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
ObservationTime timeOf(bool known, uint64_t received, uint64_t now) {
    ObservationTime time;
    time.present = true; time.receivedTimeKnown = known;
    time.receivedUs = received; time.processedUs = now;
    return time;
}
bool older(const ObservationTime& previous, bool known, uint64_t received) {
    return previous.present && previous.receivedTimeKnown && (!known || received < previous.receivedUs);
}
enum class Order { FOLLOWING, STALE, OVERLAPPING };
Order order(const ObservationTime& previous, const ObservationTime& next) {
    if (!previous.present) return Order::FOLLOWING;
    uint64_t previousFirst = 0, previousLast = 0, nextFirst = 0, nextLast = 0;
    const bool previousKnown = observationTimeBounds(previous, previousFirst, previousLast);
    const bool nextKnown = observationTimeBounds(next, nextFirst, nextLast);
    if (!previousKnown) return Order::FOLLOWING;
    if (!nextKnown) return Order::STALE; // Unknown time cannot replace retained qualified evidence.
    if (previous.receivedTimeKnown && next.receivedTimeKnown)
        return nextFirst < previousLast ? Order::STALE : Order::FOLLOWING;
    if (nextFirst > previousLast) return Order::FOLLOWING;
    if (nextLast < previousFirst) return Order::STALE;
    return Order::OVERLAPPING;
}
} // namespace

bool observationTimeBounds(const ObservationTime& time, uint64_t& earliest, uint64_t& latest) {
    if (!time.present || time.receivedTimeKnown == time.receiveBoundsKnown ||
        &earliest == &latest || overlap(&time, sizeof time, &earliest, sizeof earliest) ||
        overlap(&time, sizeof time, &latest, sizeof latest)) return false;
    const uint64_t first = time.receivedTimeKnown ? time.receivedUs : time.receivedEarliestUs;
    const uint64_t last = time.receivedTimeKnown ? time.receivedUs : time.receivedLatestUs;
    if (first > last || last > time.processedUs) return false;
    earliest = first; latest = last;
    return true;
}

bool heartbeatFreshness(const NodeSnapshot& node, uint64_t now, uint64_t age, HeartbeatFreshness& output) {
    uint64_t earliest = 0, latest = 0;
    const bool timed = observationTimeBounds(node.heartbeat, earliest, latest);
    if (!age || now < node.boundaryUs ||
        (node.heartbeat.present && (now < node.heartbeat.processedUs ||
         (node.heartbeat.receivedTimeKnown && now < node.heartbeat.receivedUs) ||
         (node.heartbeat.receiveBoundsKnown && (!timed || now < latest)))) ||
        overlap(&node, sizeof node, &output, sizeof output)) return false;
    HeartbeatFreshness result;
    if (node.exhausted) result = HeartbeatFreshness::UNQUALIFIED;
    else if (!node.producerKnown) result = HeartbeatFreshness::PRODUCER_UNKNOWN;
    else if (!node.producerTimeMs) result = HeartbeatFreshness::DISABLED;
    else if (!node.heartbeat.present) result = HeartbeatFreshness::NEVER_OBSERVED;
    else if (!timed || node.heartbeat.generation != node.route.generation)
        result = HeartbeatFreshness::UNQUALIFIED;
    else if (now - earliest <= age) result = HeartbeatFreshness::FRESH;
    else if (now - latest > age) result = HeartbeatFreshness::EXPIRED;
    else result = HeartbeatFreshness::UNQUALIFIED;
    output = result;
    return true;
}

int SdoClient::networkAnchor(uint8_t slot) const {
    if (slot >= CHANNELS) return -1;
    if (channels_[slot].networkConfigured) return slot;
    if (!channels_[slot].configured) return -1;
    for (unsigned i = 0; i < CHANNELS; ++i)
        if (channels_[i].networkConfigured && sameNode(channels_[i].network.route.binding, channels_[slot].route.binding))
            return static_cast<int>(i);
    return -1;
}

bool SdoClient::configureNetwork(uint8_t slot, const NetworkRoute& route, NetworkObserver observer, uint64_t now) {
    if (nmtOccupied_ || slot >= CHANNELS || channels_[slot].networkConfigured || !validateNetworkRoute(route) ||
        pdoIdentifier(route.heartbeatId, false) || pdoIdentifier(route.emcyId, false) ||
        !validObserver(observer) || now < lastUs_ ||
        (channels_[slot].configured && !sameBinding(channels_[slot].route.binding, route.binding))) return false;
    for (const Channel& c : channels_) {
        if (c.configured && (c.route.binding.endpoint.bus != route.binding.endpoint.bus ||
            c.route.clientToServerId == route.heartbeatId || c.route.clientToServerId == route.emcyId ||
            c.route.serverToClientId == route.heartbeatId || c.route.serverToClientId == route.emcyId)) return false;
        if (c.networkConfigured) {
            const NetworkRoute& other = c.network.route;
            if (other.binding.endpoint.bus != route.binding.endpoint.bus || sameNode(other.binding, route.binding) ||
                other.heartbeatId == route.heartbeatId || other.heartbeatId == route.emcyId ||
                other.emcyId == route.heartbeatId || other.emcyId == route.emcyId) return false;
        }
    }
    Channel& c = channels_[slot];
    c.network.route = route;
    c.network.boundaryUs = now;
    c.networkConfigured = true;
    c.observer = observer;
    for (Channel& other : channels_) {
        if (!other.configured || !sameNode(other.route.binding, route.binding)) continue;
        ObservationTime evidence = other.unanchoredService;
        evidence.generation = route.generation;
        if (!other.unresolved && evidence.present &&
            !older(c.network.serviceQualification, evidence.receivedTimeKnown, evidence.receivedUs)) {
            c.network.serviceQualification = evidence;
            c.network.serviceFromSdo = other.unanchoredServiceFromSdo;
        }
        ObservationTime sdo = other.unanchoredSdo;
        sdo.generation = route.generation;
        if (!other.unresolved && sdo.present && !older(c.network.sdo, sdo.receivedTimeKnown, sdo.receivedUs))
            c.network.sdo = sdo;
        other.unanchoredService = ObservationTime();
        other.unanchoredSdo = ObservationTime();
    }
    lastUs_ = now;
    return true;
}
bool SdoClient::setNetworkObserver(uint8_t slot, NetworkObserver observer) {
    if (slot >= CHANNELS || (!channels_[slot].configured && !channels_[slot].networkConfigured) ||
        !validObserver(observer)) return false;
    channels_[slot].observer = observer;
    return true;
}
const NodeSnapshot* SdoClient::node(uint8_t slot) const {
    const int anchor = networkAnchor(slot);
    return anchor < 0 ? nullptr : &channels_[anchor].network;
}
bool SdoClient::setProducerInterval(uint8_t slot, const NetworkRoute& source, uint16_t interval, uint64_t now) {
    const int anchor = networkAnchor(slot);
    if (anchor < 0 || now < lastUs_ || channels_[anchor].network.exhausted ||
        !sameRoute(source, channels_[anchor].network.route)) return false;
    channels_[anchor].network.producerKnown = true;
    channels_[anchor].network.producerTimeMs = interval;
    lastUs_ = now;
    return true;
}
bool SdoClient::invalidateProducerInterval(uint8_t slot, const NetworkRoute& source, uint64_t now) {
    const int anchor = networkAnchor(slot);
    if (anchor < 0 || now < lastUs_ || channels_[anchor].network.exhausted ||
        !sameRoute(source, channels_[anchor].network.route)) return false;
    channels_[anchor].network.producerKnown = false;
    lastUs_ = now;
    return true;
}

void SdoClient::recordNetwork(const NetworkObservation& observation) {
    if (networkCount_ == NETWORK_HISTORY) {
        if (networkDropped_ != UINT32_MAX) ++networkDropped_;
        return;
    }
    networkHistory_[(networkHead_ + networkCount_) % NETWORK_HISTORY] = observation;
    ++networkCount_;
}
bool SdoClient::takeNetworkObservation(NetworkObservation& output) {
    if (!networkCount_ || overlap(&output, sizeof output, this, sizeof *this)) return false;
    output = networkHistory_[networkHead_];
    networkHead_ = static_cast<uint8_t>((networkHead_ + 1) % NETWORK_HISTORY);
    --networkCount_;
    return true;
}

void SdoClient::invalidateNode(unsigned anchor, NetworkInvalidation cause, uint64_t now, bool resetObserved) {
    expireNmt(now);
    NodeSnapshot& state = channels_[anchor].network;
    if (cause != NetworkInvalidation::NMT_RESET && state.transitionPending) {
        const bool expectedReset = resetObserved && cause == NetworkInvalidation::BOOTUP &&
            (state.requestedCommand == NmtCommand::RESET_NODE || state.requestedCommand == NmtCommand::RESET_COMMUNICATION);
        state.transitionPending = false;
        state.transitionObserved = expectedReset;
        state.transitionInvalidated = !expectedReset;
    }
    if (cause != NetworkInvalidation::NMT_RESET && nmtOccupied_ && !nmtTerminal_ &&
        (nmtCompletion_.transfer.broadcast || sameNode(state.route.binding, nmtCompletion_.transfer.target.binding)))
        finishNmt(NmtOutcome::INVALIDATED, now);
    NetworkInvalidationEvent notification;
    notification.previous = state.route;
    notification.cause = cause; notification.processedUs = now;
    if (cause == NetworkInvalidation::NMT_RESET) notification.command = state.requestedCommand;
    if (state.route.generation == UINT32_MAX) state.exhausted = true;
    else ++state.route.generation;
    notification.generation = state.route.generation;
    if (cause != NetworkInvalidation::RECOVERY && cause != NetworkInvalidation::BUS_OFF &&
        state.serverSessions != UINT32_MAX) ++state.serverSessions;
    state.boundaryUs = now; state.strictBoundary = true;
    state.heartbeat = state.nmtTime = state.sdo = ObservationTime();
    state.nmtOrderingUncertain = false;
    state.serviceQualification = ObservationTime();
    revokeNodePdos(anchor, true, now);
    state.producerKnown = false; // Old readback/defaults cannot establish a new effective interval.
    const SdoOutcome outcome = cause == NetworkInvalidation::BOOTUP ? SdoOutcome::REBOOT :
        cause == NetworkInvalidation::BUS_OFF ? SdoOutcome::BUS_OFF : SdoOutcome::BINDING_INVALIDATED;
    for (Channel& c : channels_) {
        if (!c.configured || !sameNode(c.route.binding, notification.previous.binding)) continue;
        expire(c, now);
        // A boot or rebinding changes the server's session. A controller event only
        // ends local work, so it quarantines what may have left and nothing else.
        const bool local = cause == NetworkInvalidation::RECOVERY || cause == NetworkInvalidation::BUS_OFF;
        const bool ambiguous = !local || possibleWork(c);
        retire(c);
        if (ambiguous) c.unresolved = true;
        finish(c, outcome, now);
    }
    // Every dependent cache is revoked synchronously, even when diagnostics are full.
    for (unsigned i = 0; i < CHANNELS; ++i) {
        Channel& c = channels_[i];
        if ((!c.configured && !c.networkConfigured) || !c.observer.invalidate) continue;
        const Binding& binding = c.configured ? c.route.binding : c.network.route.binding;
        if (!sameNode(binding, notification.previous.binding)) continue;
        bool duplicate = false;
        for (unsigned j = 0; j < i; ++j) {
            const Channel& earlier = channels_[j];
            if (earlier.observer.context == c.observer.context && earlier.observer.invalidate == c.observer.invalidate &&
                ((earlier.configured && sameNode(earlier.route.binding, binding)) ||
                 (earlier.networkConfigured && sameNode(earlier.network.route.binding, binding)))) duplicate = true;
        }
        if (!duplicate) {
            NetworkInvalidationEvent dependent = notification;
            dependent.previous.binding = binding;
            c.observer.invalidate(c.observer.context, dependent);
        }
    }
}
bool SdoClient::invalidateNetwork(uint8_t slot, NetworkInvalidation cause, uint64_t now) {
    const int anchor = networkAnchor(slot);
    if (anchor < 0 || cause > NetworkInvalidation::BUS_OFF || now < lastUs_) return false;
    lastUs_ = now;
    invalidateNode(static_cast<unsigned>(anchor), cause, now);
    return true;
}
// Sample before retire, which clears cleanup. A held handoff never reached the SDK, and
// finish() already quarantines a transfer with possible effects.
bool SdoClient::possibleWork(const Channel& c) {
    return c.txLive || c.cleanup;
}
bool SdoClient::reportController(ControllerState state, uint64_t now) {
    if (state > ControllerState::RECOVERING || now < lastUs_) return false;
    lastUs_ = now; controllerState_ = state;
    if (state != ControllerState::BUS_OFF && state != ControllerState::RECOVERING) return true;
    expireNmt(now);
    finishNmt(state == ControllerState::BUS_OFF ? NmtOutcome::BUS_OFF : NmtOutcome::INVALIDATED, now);
    const NetworkInvalidation cause = state == ControllerState::BUS_OFF ? NetworkInvalidation::BUS_OFF : NetworkInvalidation::RECOVERY;
    for (unsigned i = 0; i < CHANNELS; ++i)
        if (channels_[i].networkConfigured) invalidateNode(i, cause, now);
    for (Channel& c : channels_) {
        if (!c.configured) continue;
        expire(c, now);
        const bool ambiguous = possibleWork(c);
        retire(c);
        if (ambiguous) c.unresolved = true;
        // Service proof from before a controller event says nothing about after it.
        c.unanchoredService = ObservationTime();
        c.unanchoredSdo = ObservationTime();
        finish(c, state == ControllerState::BUS_OFF ? SdoOutcome::BUS_OFF : SdoOutcome::BINDING_INVALIDATED, now);
    }
    return true;
}

bool SdoClient::receiveNetwork(const Frame& input, const NetworkRoute& source, uint64_t received, uint64_t now) {
    if (now < lastUs_ || received > now) return false;
    const Frame frame = input;
    const NetworkRoute provenance = source;
    service(now);
    return dispatchNetwork(frame, &provenance, true, received, now);
}

bool SdoClient::dispatchNetwork(const Frame& frame, const NetworkRoute* source, bool known, uint64_t received, uint64_t now,
                               bool boundsKnown, uint64_t earliest, uint64_t latest) {
    NetworkObservation observation;
    observation.frame = frame; observation.time = timeOf(known, received, now);
    observation.time.receiveBoundsKnown = boundsKnown;
    observation.time.receivedEarliestUs = earliest; observation.time.receivedLatestUs = latest;
    uint64_t first = 0, last = 0;
    const bool timed = observationTimeBounds(observation.time, first, last);
    for (unsigned i = 0; i < CHANNELS; ++i) {
        Channel& c = channels_[i];
        if (!c.networkConfigured || (frame.id != c.network.route.heartbeatId && frame.id != c.network.route.emcyId)) continue;
        NodeSnapshot& state = c.network;
        observation.route = source ? *source : state.route;
        observation.time.generation = observation.route.generation;
        if ((source && !sameRoute(*source, state.route)) || state.exhausted ||
            (timed && (last < state.boundaryUs || (state.strictBoundary && last == state.boundaryUs)))) {
            observation.disposition = NetworkDisposition::STALE;
            recordNetwork(observation); return false;
        }
        const Status checked = checkNetworkEvent(frame, state.route, state.route.binding, state.route.generation, observation.event);
        if (!checked) {
            observation.disposition = NetworkDisposition::REJECTED;
            recordNetwork(observation); return false;
        }
        observation.decoded = true;
        observation.disposition = NetworkDisposition::ACCEPTED;
        const NetworkEvent& event = observation.event;
        if (event.kind == NetworkEventKind::BOOTUP) {
            // A newer heartbeat cannot prove that older configuration survived this reset.
            const ObservationTime previous = state.nmtTime;
            const NmtState previousState = state.nmt;
            invalidateNode(i, NetworkInvalidation::BOOTUP, now, timed && first > state.transitionUs);
            if (order(previous, observation.time) != Order::FOLLOWING) {
                state.nmt = previousState; state.nmtTime = previous;
            } else {
                state.nmt = NmtState::INITIALIZING; state.nmtTime = observation.time;
                state.nmtTime.generation = state.route.generation;
            }
            observeNmt(state, observation.time, NmtState::INITIALIZING);
        } else if ((!timed && state.strictBoundary) ||
                   (timed && (first < state.boundaryUs || (state.strictBoundary && first == state.boundaryUs)))) {
            observation.disposition = NetworkDisposition::UNQUALIFIED;
            recordNetwork(observation); return false;
        } else if (event.kind == NetworkEventKind::HEARTBEAT) {
            const Order heartbeatOrder = order(state.heartbeat, observation.time);
            const Order nmtOrder = order(state.nmtTime, observation.time);
            if (heartbeatOrder == Order::STALE) {
                observation.disposition = NetworkDisposition::STALE;
                recordNetwork(observation); return false;
            }
            if (heartbeatOrder == Order::FOLLOWING) state.heartbeat = observation.time;
            if (nmtOrder == Order::FOLLOWING) {
                state.nmt = event.state; state.nmtTime = observation.time;
                state.nmtOrderingUncertain = false;
                observeNmt(state, observation.time, event.state);
                ServiceEligibility eligibility;
                if (serviceEligibility(state, now, eligibility) && eligibility.exactNmtKnown &&
                    eligibility.state == NmtState::STOPPED) stopSdo(i, now);
                if (eligibility.exactNmtKnown && eligibility.state != NmtState::OPERATIONAL)
                    revokeNodePdos(i, false, now);
            } else if (nmtOrder == Order::OVERLAPPING &&
                       (state.nmtOrderingUncertain || state.nmt != event.state)) {
                // Neither state is demonstrably latest. Preserve the raw prior state,
                // but fence later qualification beyond both possible receive windows.
                uint64_t previousFirst = 0, previousLast = 0;
                observationTimeBounds(state.nmtTime, previousFirst, previousLast);
                ObservationTime fence = observation.time;
                fence.receivedTimeKnown = false; fence.receivedUs = 0;
                fence.receiveBoundsKnown = true;
                fence.receivedEarliestUs = previousFirst < first ? previousFirst : first;
                fence.receivedLatestUs = previousLast > last ? previousLast : last;
                state.nmtTime = fence;
                state.nmtOrderingUncertain = true;
                state.serviceQualification = ObservationTime();
                stopSdo(i, now);
                revokeNodePdos(i, false, now);
            }
            if (heartbeatOrder == Order::OVERLAPPING || nmtOrder == Order::OVERLAPPING)
                observation.disposition = NetworkDisposition::UNQUALIFIED;
        } else {
            if (state.emcyCount != UINT32_MAX) ++state.emcyCount;
            if (order(state.emcyTime, observation.time) == Order::FOLLOWING) {
                state.emcy = event; state.emcyTime = observation.time;
            }
        }
        recordNetwork(observation);
        return true;
    }
    // SDO observations already have their own retained normal route consumer.
    for (const Channel& c : channels_)
        if (c.configured && frame.id == c.route.serverToClientId) return false;
    recordNetwork(observation);
    return false;
}

void SdoClient::recordSdo(Channel& channel, bool known, uint64_t received, uint64_t now) {
    const unsigned slot = static_cast<unsigned>(&channel - channels_);
    const int anchor = networkAnchor(static_cast<uint8_t>(slot));
    if (anchor < 0) {
        if (!older(channel.unanchoredSdo, known, received))
            channel.unanchoredSdo = timeOf(known, received, now);
        if (!channel.experimentActive && !older(channel.unanchoredService, known, received)) {
            channel.unanchoredService = timeOf(known, received, now);
            channel.unanchoredServiceFromSdo = true;
        }
        return;
    }
    NodeSnapshot& state = channels_[anchor].network;
    if (state.exhausted || (known && (received < state.boundaryUs ||
        (state.strictBoundary && received == state.boundaryUs))) || (!known && state.strictBoundary) ||
        older(state.sdo, known, received)) return;
    state.sdo = timeOf(known, received, now);
    state.sdo.generation = state.route.generation;
    if (!channel.experimentActive && !older(state.serviceQualification, known, received)) {
        state.serviceQualification = state.sdo;
        state.serviceFromSdo = true;
    }
}
} // namespace CANOPEN
} // namespace MotorControlCAN
