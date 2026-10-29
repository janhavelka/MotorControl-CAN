// SPDX-License-Identifier: MIT
#include "SdoClient.h"
#include <cstring>

namespace MotorControlCAN {
namespace CANOPEN {
namespace {
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool sameNode(const Binding& a, const Binding& b) {
    return a.endpoint.bus == b.endpoint.bus && a.endpoint.address == b.endpoint.address;
}
bool reset(NmtCommand command) {
    return command == NmtCommand::RESET_NODE || command == NmtCommand::RESET_COMMUNICATION;
}
bool current(const NodeSnapshot& node, const ObservationTime& time, uint64_t now) {
    return !node.exhausted && time.present && time.receivedTimeKnown &&
        time.generation == node.route.generation && time.receivedUs <= now &&
        now - time.receivedUs <= node.maximumEvidenceAgeUs &&
        (!node.intentPresent || time.receivedUs > node.transitionUs);
}
bool future(const ObservationTime& time, uint64_t now) {
    return time.present && (time.processedUs > now || (time.receivedTimeKnown && time.receivedUs > now));
}
} // namespace

bool serviceEligibility(const NodeSnapshot& node, uint64_t now, ServiceEligibility& output) {
    if (!node.maximumEvidenceAgeUs || now < node.boundaryUs ||
        (node.intentPresent && now < node.transitionUs) || future(node.nmtTime, now) ||
        future(node.serviceQualification, now) || overlap(&node, sizeof node, &output, sizeof output)) return false;
    ServiceEligibility result;
    result.exactNmtKnown = current(node, node.nmtTime, now);
    result.state = node.nmt;
    const bool inhibited = node.exhausted || (result.exactNmtKnown &&
        (node.nmt == NmtState::STOPPED || node.nmt == NmtState::INITIALIZING));
    if (!inhibited) {
        result.ordinarySdo = (result.exactNmtKnown &&
            (node.nmt == NmtState::PRE_OPERATIONAL || node.nmt == NmtState::OPERATIONAL)) ||
            current(node, node.serviceQualification, now);
        result.nonChangingProbe = true;
        result.pdoTransmit = result.exactNmtKnown && node.nmt == NmtState::OPERATIONAL;
        result.tpdoReceive = !result.exactNmtKnown || node.nmt == NmtState::OPERATIONAL;
    }
    output = result;
    return true;
}

bool SdoClient::setServiceAge(uint8_t slot, uint64_t age, uint64_t now) {
    const int anchor = networkAnchor(slot);
    if (anchor < 0 || !age || now < lastUs_) return false;
    channels_[anchor].network.maximumEvidenceAgeUs = age;
    lastUs_ = now;
    return true;
}

bool SdoClient::eligibleSdo(uint8_t slot, const SdoTransfer& work, uint64_t now) const {
    if (controllerState_ == ControllerState::BUS_OFF || controllerState_ == ControllerState::RECOVERING) return false;
    const int anchor = networkAnchor(slot);
    if (anchor < 0) return true; // Explicit external service premise for SDO-only consumers.
    ServiceEligibility eligibility;
    return serviceEligibility(channels_[anchor].network, now, eligibility) &&
        (eligibility.ordinarySdo || (eligibility.nonChangingProbe && work.direction == SdoDirection::UPLOAD &&
                                    work.readEffect == SdoReadEffect::REVIEWED_NON_CHANGING));
}

bool SdoClient::validNmt(NmtRequestId id) const {
    return id.owner == this && id.generation && nmtOccupied_ && id.generation == nmtGeneration_;
}
bool SdoClient::admitNmt(const NmtTransfer& input, uint64_t now, NmtRequestId& id) {
    if (nmtOccupied_ || nmtGeneration_ == UINT32_MAX || now < lastUs_ ||
        controllerState_ == ControllerState::BUS_OFF || controllerState_ == ControllerState::RECOVERING ||
        overlap(&id, sizeof id, this, sizeof *this) || overlap(&id, sizeof id, &input, sizeof input)) return false;
    const NmtTransfer work = input;
    Frame checked;
    if (!makeNmtRequest(work.command, work.broadcast ? 0 :
        static_cast<uint8_t>(work.target.binding.endpoint.address), checked) ||
        !work.request || !work.queueTimeoutUs || work.deadlineUs <= now ||
        (!work.broadcast && (!validateNetworkRoute(work.target) || work.target.binding.endpoint.bus != work.bus))) return false;
    NmtCompletion candidate;
    candidate.transfer = work;
    bool found = false;
    for (unsigned i = 0; i < CHANNELS; ++i) {
        const Channel& c = channels_[i];
        if (!c.configured && !c.networkConfigured) continue;
        const int anchor = networkAnchor(static_cast<uint8_t>(i));
        if (anchor < 0) return false; // All local bindings must participate in domain effects.
        const NodeSnapshot& node = channels_[anchor].network;
        if (node.route.binding.endpoint.bus != work.bus) return false;
        if (!work.broadcast && !sameNode(node.route.binding, work.target.binding)) continue;
        if ((!work.broadcast && (!sameBinding(work.target.binding, node.route.binding) ||
             work.target.generation != node.route.generation || work.target.heartbeatId != node.route.heartbeatId ||
             work.target.emcyId != node.route.emcyId)) || node.exhausted ||
            (node.transitionPending && now < node.transitionDeadlineUs)) return false;
        NmtAffected& affected = candidate.affected[candidate.affectedCount++];
        affected.binding = c.configured ? c.route.binding : node.route.binding;
        affected.previousGeneration = affected.generation = node.route.generation;
        found = true;
    }
    if (!found) return false;
    nmtCompletion_ = candidate;
    nmtOccupied_ = true; nmtTerminal_ = false; ++nmtGeneration_;
    nmtQueueCutoff_ = now + ((work.deadlineUs - now < work.queueTimeoutUs) ?
                           work.deadlineUs - now : work.queueTimeoutUs);
    // Pin-specific producer-only initialization: sendCommand accesses just these
    // two pointers and nodeId/internalCommand. Never initialize/process a local node.
    nmtEngine_ = CO_NMT_t();
    nmtPort_ = CO_CANmodule_t();
    nmtEngine_.NMT_CANdevTx = &nmtPort_;
    nmtEngine_.NMT_TXbuff = CO_CANtxBufferInit(&nmtPort_, 0, 0, false, 2, false);
    lastUs_ = now;
    id.owner = this; id.generation = nmtGeneration_;
    return true;
}
void SdoClient::finishNmt(NmtOutcome outcome, uint64_t now) {
    if (!nmtOccupied_ || nmtTerminal_) return;
    nmtCompletion_.outcome = outcome;
    nmtCompletion_.completedUs = now;
    nmtTerminal_ = true;
    nmtPort_.queueOccupied = false; nmtPort_.tx.bufferFull = false;
}
void SdoClient::expireNmt(uint64_t now) {
    for (Channel& c : channels_) {
        if (c.networkConfigured && c.network.transitionPending && now >= c.network.transitionDeadlineUs) {
            c.network.transitionPending = false; c.network.transitionExpired = true;
        }
    }
    if (!nmtOccupied_ || nmtTerminal_) return;
    if (now >= nmtCompletion_.transfer.deadlineUs ||
        (!nmtCompletion_.transmissionPossible && now >= nmtQueueCutoff_)) {
        finishNmt(NmtOutcome::TIMEOUT, now); return;
    }
}
void SdoClient::serviceNmt(uint64_t now) {
    expireNmt(now);
    if (!nmtOccupied_ || nmtTerminal_) return;
    if (nmtCompletion_.queued) return;
    const NmtTransfer& work = nmtCompletion_.transfer;
    const CO_ReturnError_t result = CO_NMT_sendCommand(&nmtEngine_, static_cast<CO_NMT_command_t>(work.command),
        work.broadcast ? 0 : static_cast<uint8_t>(work.target.binding.endpoint.address));
    if (result != CO_ERROR_NO) finishNmt(NmtOutcome::QUEUE_REJECTED, now);
    else nmtCompletion_.queued = true;
}

void SdoClient::applyNmt(uint64_t now) {
    const NmtTransfer& work = nmtCompletion_.transfer;
    for (unsigned i = 0; i < CHANNELS; ++i) {
        Channel& anchor = channels_[i];
        if (!anchor.networkConfigured || (!work.broadcast &&
            !sameNode(anchor.network.route.binding, work.target.binding))) continue;
        NodeSnapshot& state = anchor.network;
        state.intentPresent = true; state.requestedCommand = work.command; state.requestedId = work.request;
        state.transitionUs = now; state.transitionDeadlineUs = work.deadlineUs;
        state.transitionPending = true; state.transitionExpired = state.transitionConflict = false;
        state.transitionObserved = state.transitionInvalidated = false;
        if (reset(work.command)) {
            const ObservationTime previous = state.nmtTime, previousSdo = state.sdo;
            const NmtState previousState = state.nmt;
            invalidateNode(i, NetworkInvalidation::NMT_RESET, now);
            state.nmtTime = previous; state.nmt = previousState; state.sdo = previousSdo;
        } else {
            if (work.command == NmtCommand::STOP) stopSdo(i, now);
            NetworkInvalidationEvent event;
            event.previous = state.route; event.generation = state.route.generation;
            event.cause = NetworkInvalidation::NMT_TRANSITION; event.command = work.command; event.processedUs = now;
            for (unsigned j = 0; j < CHANNELS; ++j) {
                Channel& c = channels_[j];
                const int source = networkAnchor(static_cast<uint8_t>(j));
                if (source != static_cast<int>(i)) continue;
                if (!c.observer.invalidate) continue;
                bool duplicate = false;
                for (unsigned k = 0; k < j; ++k)
                    if (networkAnchor(static_cast<uint8_t>(k)) == source &&
                        channels_[k].observer.context == c.observer.context &&
                        channels_[k].observer.invalidate == c.observer.invalidate) duplicate = true;
                if (!duplicate) {
                    event.previous.binding = c.configured ? c.route.binding : state.route.binding;
                    c.observer.invalidate(c.observer.context, event);
                }
            }
        }
        for (unsigned j = 0; j < nmtCompletion_.affectedCount; ++j)
            if (sameNode(nmtCompletion_.affected[j].binding, state.route.binding))
                nmtCompletion_.affected[j].generation = state.route.generation;
    }
}
void SdoClient::stopSdo(unsigned anchor, uint64_t now) {
    for (unsigned i = 0; i < CHANNELS; ++i) {
        Channel& channel = channels_[i];
        if (!channel.configured || networkAnchor(static_cast<uint8_t>(i)) != static_cast<int>(anchor)) continue;
        expire(channel, now); retire(channel); finish(channel, SdoOutcome::BINDING_INVALIDATED, now);
    }
}
bool SdoClient::nextNmtTx(NmtRequestId id, uint64_t now, Frame& frame) {
    if (!validNmt(id) || now < lastUs_ || overlap(&frame, sizeof frame, this, sizeof *this) ||
        overlap(&frame, sizeof frame, &id, sizeof id)) return false;
    service(now);
    if (nmtTerminal_ || nmtCompletion_.transmissionPossible || !nmtPort_.queueOccupied) return false;
    Frame candidate;
    candidate.id = nmtPort_.queued.ident; candidate.dlc = candidate.payloadLength = nmtPort_.queued.DLC;
    std::memcpy(candidate.data, nmtPort_.queued.data, candidate.payloadLength);
    nmtPort_.queueOccupied = false; nmtPort_.tx.bufferFull = false;
    nmtCompletion_.transmissionPossible = true;
    applyNmt(now);
    frame = candidate;
    return true;
}
bool SdoClient::canTransmitNmt(NmtRequestId id, uint64_t now) const {
    return validNmt(id) && !nmtTerminal_ && nmtCompletion_.transmissionPossible && now >= lastUs_ &&
        now < nmtCompletion_.transfer.deadlineUs;
}
bool SdoClient::confirmNmtTx(NmtRequestId id, uint64_t now) {
    if (!validNmt(id) || now < lastUs_) return false;
    service(now);
    if (nmtTerminal_ || !nmtCompletion_.transmissionPossible) return false;
    nmtCompletion_.transmissionConfirmed = true;
    finishNmt(NmtOutcome::TRANSMITTED, now);
    return true;
}
bool SdoClient::cancelNmt(NmtRequestId id, uint64_t now) {
    if (!validNmt(id) || now < lastUs_) return false;
    service(now);
    if (nmtTerminal_) return false;
    finishNmt(NmtOutcome::CANCELLED, now);
    return true;
}
const NmtCompletion* SdoClient::nmtResult(NmtRequestId id) const {
    return validNmt(id) && nmtTerminal_ ? &nmtCompletion_ : nullptr;
}
bool SdoClient::releaseNmt(NmtRequestId id) {
    if (!validNmt(id) || !nmtTerminal_) return false;
    nmtOccupied_ = false;
    return true;
}
void SdoClient::observeNmt(NodeSnapshot& state, const ObservationTime& time, NmtState observed) {
    if (!state.intentPresent || !time.receivedTimeKnown || time.receivedUs <= state.transitionUs ||
        !state.transitionPending) return;
    const bool matches = reset(state.requestedCommand) ? observed == NmtState::INITIALIZING :
        state.requestedCommand == NmtCommand::START ? observed == NmtState::OPERATIONAL :
        state.requestedCommand == NmtCommand::STOP ? observed == NmtState::STOPPED : observed == NmtState::PRE_OPERATIONAL;
    if (matches) { state.transitionPending = false; state.transitionObserved = true; }
    else state.transitionConflict = true;
}
} // namespace CANOPEN
} // namespace MotorControlCAN
