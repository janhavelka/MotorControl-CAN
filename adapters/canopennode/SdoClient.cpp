// SPDX-License-Identifier: MIT
#define OD_DEFINITION
#include "SdoClient.h"
#include <cstring>

namespace MotorControlCAN {
namespace CANOPEN {
namespace {
// Server responses that name their object. Segment frames do not.
bool frameMultiplexer(const Frame& f, uint16_t& index, uint8_t& subindex) {
    if (f.payloadLength != 8) return false;
    const uint8_t command = f.data[0];
    if ((command & 0xe0u) != 0x40u && command != 0x60u && command != 0x80u) return false;
    index = static_cast<uint16_t>(f.data[1] | (uint16_t(f.data[2]) << 8));
    subindex = f.data[3];
    return true;
}
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
uint64_t cutoff(uint64_t now, uint64_t deadline, uint32_t interval) {
    const uint64_t remaining = deadline - now;
    return now + (remaining < interval ? remaining : interval);
}
Frame wire(const StackFrame& native) {
    Frame f;
    f.id = native.ident;
    f.dlc = f.payloadLength = native.DLC;
    f.extended = native.extended;
    f.rtr = native.rtr;
    if (!f.rtr && f.payloadLength <= 8) std::memcpy(f.data, native.data, f.payloadLength);
    return f;
}
StackFrame native(const Frame& f) {
    StackFrame n = {};
    n.ident = f.id;
    n.DLC = f.payloadLength;
    std::memcpy(n.data, f.data, f.payloadLength);
    return n;
}
uint32_t word(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 |
           uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}
SdoExpectation expectation(const SdoTransfer& work) {
    SdoExpectation out;
    out.channel = work.channel;
    out.index = work.index;
    out.subindex = work.subindex;
    out.width = work.size;
    out.direction = work.direction;
    return out;
}
bool sameExperiment(const SdoTransfer& a, const SdoTransfer& b) {
    return sameBinding(a.channel.binding, b.channel.binding) &&
        a.channel.clientToServerId == b.channel.clientToServerId &&
        a.channel.serverToClientId == b.channel.serverToClientId &&
        a.operation == b.operation && a.request == b.request && a.step == b.step &&
        a.index == b.index && a.subindex == b.subindex && a.size == b.size &&
        a.uploadSize == b.uploadSize &&
        a.direction == b.direction && a.readEffect == b.readEffect &&
        a.allowSegmentedUpload == b.allowSegmentedUpload &&
        a.requirePreOperational == b.requirePreOperational && a.deadlineUs == b.deadlineUs &&
        a.queueTimeoutUs == b.queueTimeoutUs && a.responseTimeoutUs == b.responseTimeoutUs &&
        (a.direction != SdoDirection::DOWNLOAD || std::memcmp(a.data, b.data, a.size) == 0);
}
} // namespace

bool SdoClient::configure(uint8_t slot, const SdoChannel& route, bool confirmed, bool fresh) {
    if (nmtOccupied_ || slot >= CHANNELS || channels_[slot].configured || !fresh || !validateSdoChannel(route) ||
        pdoIdentifier(route.clientToServerId, false) || pdoIdentifier(route.serverToClientId, false))
        return false;
    if (channels_[slot].networkConfigured &&
        (!sameBinding(channels_[slot].network.route.binding, route.binding) ||
         channels_[slot].network.strictBoundary || channels_[slot].network.exhausted)) return false;
    for (const Channel& other : channels_) {
        if (other.networkConfigured) {
            const NetworkRoute& network = other.network.route;
            if (network.binding.endpoint.bus == route.binding.endpoint.bus &&
                network.binding.endpoint.address == route.binding.endpoint.address &&
                (other.network.strictBoundary || other.network.exhausted)) return false;
            if (network.binding.endpoint.bus != route.binding.endpoint.bus ||
                route.clientToServerId == network.heartbeatId || route.clientToServerId == network.emcyId ||
                route.serverToClientId == network.heartbeatId || route.serverToClientId == network.emcyId) return false;
        }
        if (!other.configured) continue;
        if (other.route.binding.endpoint.bus != route.binding.endpoint.bus ||
            (other.route.binding.endpoint.address == route.binding.endpoint.address &&
             other.route.binding.endpoint.channel == route.binding.endpoint.channel) ||
            other.route.clientToServerId == route.clientToServerId ||
            other.route.serverToClientId == route.serverToClientId ||
            other.route.clientToServerId == route.serverToClientId ||
            other.route.serverToClientId == route.clientToServerId) return false;
    }
    Channel& c = channels_[slot];
    uint8_t count = 3, node = static_cast<uint8_t>(route.binding.endpoint.address);
    uint32_t tx = route.clientToServerId, rx = route.serverToClientId;
    OD_obj_record_t fields[] = {{&count, 0, ODA_SDO_R, 1}, {&tx, 1, ODA_SDO_R, 4},
                               {&rx, 2, ODA_SDO_R, 4}, {&node, 3, ODA_SDO_R, 1}};
    OD_entry_t entry = {0x1280, 4, ODT_REC, fields, nullptr};
    // Dynamic/local OD and callback wakeups are disabled; no temporary OD pointer survives init.
    if (CO_SDOclient_init(&c.engine, nullptr, &entry, 0, &c.port, 0, &c.port, 0, nullptr) != CO_ERROR_NO) {
        c.engine = CO_SDOclient_t();
        c.port = CO_CANmodule_t(); // Preserve any passive route and borrowed cache observer.
        return false;
    }
    c.route = route;
    c.configured = true;
    c.confirmed = confirmed;
    const int anchor = networkAnchor(slot);
    if (confirmed) {
        ObservationTime evidence;
        evidence.present = evidence.receivedTimeKnown = true;
        evidence.receivedUs = evidence.processedUs = lastUs_;
        if (anchor >= 0) {
            NodeSnapshot& state = channels_[anchor].network;
            evidence.generation = state.route.generation;
            state.serviceQualification = evidence;
            state.serviceFromSdo = false;
        } else {
            c.unanchoredService = evidence;
        }
    }
    return true;
}

bool SdoClient::valid(SdoRequestId id) const {
    return id.owner == this && id.channel < CHANNELS && channels_[id.channel].occupied &&
           id.generation != 0 && channels_[id.channel].generation == id.generation;
}

bool SdoClient::advanceConfiguration(uint8_t slot, const Binding& previous,
                                      const Binding& current, uint64_t now) {
    if (slot >= CHANNELS || now < lastUs_ || nmtOccupied_ ||
        controllerState_ == ControllerState::BUS_OFF ||
        controllerState_ == ControllerState::RECOVERING ||
        overlap(&previous, sizeof previous, this, sizeof *this) ||
        overlap(&current, sizeof current, this, sizeof *this) ||
        !validateBinding(previous) || !validateBinding(current) ||
        previous.configurationGeneration == UINT32_MAX)
        return false;
    Binding expected = previous;
    ++expected.configurationGeneration;
    if (!sameBinding(expected, current) || !channels_[slot].configured ||
        !sameBinding(channels_[slot].route.binding, previous))
        return false;
    const int anchor = networkAnchor(slot);
    if (anchor < 0) return false;
    NodeSnapshot& state = channels_[anchor].network;
    ServiceEligibility eligibility;
    if (!sameBinding(state.route.binding, previous) || state.transitionPending ||
        !serviceEligibility(state, now, eligibility) || !eligibility.ordinarySdo)
        return false;
    for (const Channel& channel : channels_) {
        if (!channel.configured ||
            channel.route.binding.endpoint.bus != previous.endpoint.bus ||
            channel.route.binding.endpoint.address != previous.endpoint.address)
            continue;
        // A shared physical node with another logical/channel binding needs an
        // explicit effect-scope policy; do not guess which interpretation changed.
        if (!sameBinding(channel.route.binding, previous) ||
            channel.unresolved || channel.occupied || channel.cleanup ||
            channel.awaiting || channel.handoffLive || channel.port.queueOccupied ||
            channel.port.tx.bufferFull || channel.port.sendFault)
            return false;
    }

    NetworkInvalidationEvent event;
    event.previous = state.route;
    event.generation = state.route.generation;
    event.cause = NetworkInvalidation::CONFIGURATION;
    event.processedUs = now;
    lastUs_ = now;
    revokeNodePdos(static_cast<unsigned>(anchor), false, now);
    for (unsigned i = 0; i < CHANNELS; ++i) {
        Channel& channel = channels_[i];
        if ((!channel.configured && !channel.networkConfigured) || !channel.observer.invalidate)
            continue;
        const Binding& binding = channel.configured ? channel.route.binding : channel.network.route.binding;
        if (!sameBinding(binding, previous)) continue;
        bool duplicate = false;
        for (unsigned j = 0; j < i; ++j) {
            const Channel& earlier = channels_[j];
            if (earlier.observer.context == channel.observer.context &&
                earlier.observer.invalidate == channel.observer.invalidate &&
                ((earlier.configured && sameBinding(earlier.route.binding, previous)) ||
                 (earlier.networkConfigured && sameBinding(earlier.network.route.binding, previous))))
                duplicate = true;
        }
        if (!duplicate) channel.observer.invalidate(channel.observer.context, event);
    }
    for (Channel& channel : channels_)
        if (channel.configured && sameBinding(channel.route.binding, previous))
            channel.route.binding = current;
    state.route.binding = current;
    return true;
}

const SdoChannel* SdoClient::channel(uint8_t slot) const {
    return slot < CHANNELS && channels_[slot].configured ? &channels_[slot].route : nullptr;
}
bool SdoClient::authorizeUploadExperiment(uint8_t slot, const SdoTransfer& input,
                                         Generation nodeGeneration, uint32_t authorizationId,
                                         uint64_t now) {
    return input.direction == SdoDirection::UPLOAD &&
        authorizeExperiment(slot, input, nodeGeneration, authorizationId, now);
}
bool SdoClient::authorizeDownloadExperiment(uint8_t slot, const SdoTransfer& input,
                                           Generation nodeGeneration, uint32_t authorizationId,
                                           uint64_t now) {
    return input.direction == SdoDirection::DOWNLOAD && input.size >= 1 && input.size <= 4 &&
        input.uploadSize == SdoUploadSize::EXACT &&
        authorizeExperiment(slot, input, nodeGeneration, authorizationId, now);
}
bool SdoClient::authorizeExperiment(uint8_t slot, const SdoTransfer& input,
                                   Generation nodeGeneration, uint32_t authorizationId,
                                   uint64_t now) {
    if (slot >= CHANNELS || now < lastUs_ ||
        overlap(&input, sizeof input, this, sizeof *this)) return false;
    Channel& c = channels_[slot];
    const int anchor = networkAnchor(slot);
    ServiceEligibility eligibility;
    if (!c.configured || c.occupied || c.cleanup || c.unresolved || c.handoffLive ||
        !authorizationId || authorizationId <= c.experimentAuthorization ||
        (c.experimentAuthorization && (input.operation < c.experimentUpload.operation ||
         (input.operation == c.experimentUpload.operation && input.step <= c.experimentUpload.step))) ||
        !nodeGeneration || anchor < 0 || channels_[anchor].network.route.generation != nodeGeneration ||
        controllerState_ == ControllerState::BUS_OFF || controllerState_ == ControllerState::RECOVERING ||
        !serviceEligibility(channels_[anchor].network, now, eligibility) || !eligibility.probeUpload ||
        !validateSdoChannel(input.channel) || !sameBinding(input.channel.binding, c.route.binding) ||
        input.channel.clientToServerId != c.route.clientToServerId ||
        input.channel.serverToClientId != c.route.serverToClientId ||
        !input.operation || !input.request || !input.step || !input.size || input.size > 32 ||
        (input.direction != SdoDirection::UPLOAD && input.direction != SdoDirection::DOWNLOAD) ||
        input.readEffect != SdoReadEffect::UNQUALIFIED ||
        (input.uploadSize != SdoUploadSize::EXACT && input.uploadSize != SdoUploadSize::SERVER_INDICATED) ||
        input.deadlineUs <= now || !input.queueTimeoutUs || !input.responseTimeoutUs)
        return false;
    c.experimentUpload = input;
    c.experimentNodeGeneration = nodeGeneration;
    c.experimentAuthorization = authorizationId;
    c.experimentArmed = true;
    lastUs_ = now;
    return true;
}
void SdoClient::clearUploadExperiment(uint8_t slot) {
    if (slot < CHANNELS) channels_[slot].experimentArmed = false;
}
bool SdoClient::experimentMatches(uint8_t slot, const SdoTransfer& work, uint64_t now,
                                  bool admitted) const {
    if (slot >= CHANNELS) return false;
    const Channel& c = channels_[slot];
    const int anchor = networkAnchor(slot);
    return c.configured && !c.unresolved && anchor >= 0 &&
        !channels_[anchor].network.exhausted &&
        c.experimentNodeGeneration == channels_[anchor].network.route.generation &&
        now < c.experimentUpload.deadlineUs &&
        (admitted ? c.experimentActive && c.occupied && !c.terminal : c.experimentArmed) &&
        sameExperiment(work, c.experimentUpload);
}
void SdoClient::retainPair(Channel& c, uint16_t index, uint8_t subindex) {
    for (uint8_t i = 0; i < c.retainedCount; ++i)
        if (c.retained[i].index == index && c.retained[i].subindex == subindex) return;
    if (c.retainedCount == Channel::RETAINED_PAIRS) { c.drainRefused = true; return; }
    c.retained[c.retainedCount].index = index;
    c.retained[c.retainedCount].subindex = subindex;
    ++c.retainedCount;
}
void SdoClient::retainFrame(Channel& c, const Frame& f) {
    uint16_t index = 0; uint8_t subindex = 0;
    if (!frameMultiplexer(f, index, subindex)) { c.drainRefused = true; return; }
    for (uint8_t i = 0; i < c.retainedCount; ++i)
        if (c.retained[i].index == index && c.retained[i].subindex == subindex) return;
    // A retained pair names a request made since the last reopening, so its late reply is
    // expected. Any other frame after a marker reopened this route disproves that marker's premise.
    if (c.requalifications) c.drainRefused = true;
    retainPair(c, index, subindex);
}
bool SdoClient::checkAdmission(uint8_t slot, const SdoTransfer& work, uint64_t now) const {
    return slot < CHANNELS && !channels_[slot].unresolved && checkWork(slot, work, now);
}
bool SdoClient::checkDrain(uint8_t slot, const SdoTransfer& work, const SdoDrainPremises& premises,
                           uint64_t now) const {
    if (slot >= CHANNELS || !premises.soleClient || !premises.orderedServer ||
        !checkWork(slot, work, now)) return false;
    const Channel& c = channels_[slot];
    if (!c.unresolved || c.drainRefused || c.drainSuccess || c.occupied || c.cleanup || c.handoffLive ||
        c.port.queueOccupied || work.direction != SdoDirection::UPLOAD ||
        work.readEffect != SdoReadEffect::ACCEPTED_UNQUALIFIED ||
        work.uploadSize != SdoUploadSize::EXACT || work.allowSegmentedUpload || work.size > 4 ||
        c.requalifications == UINT32_MAX) return false;
    if (c.lastCompletedValid && c.lastCompleted.index == work.index &&
        c.lastCompleted.subindex == work.subindex) return false;
    for (uint8_t i = 0; i < c.retainedCount; ++i)
        if (c.retained[i].index == work.index && c.retained[i].subindex == work.subindex) return false;
    const int anchor = networkAnchor(slot);
    return anchor < 0 || !channels_[anchor].network.exhausted;
}
bool SdoClient::admitDrain(uint8_t slot, const SdoTransfer& work, const SdoDrainPremises& premises,
                           uint64_t now, SdoRequestId& id, uint64_t initialQueueDeadline) {
    if (!checkDrain(slot, work, premises, now) || initialQueueDeadline <= now ||
        overlap(&id, sizeof id, this, sizeof *this) || overlap(&id, sizeof id, &work, sizeof work))
        return false;
    if (!start(slot, work, now, id, initialQueueDeadline)) return false;
    channels_[slot].drainActive = true;
    return true;
}
bool SdoClient::drainSettled(uint8_t slot) const {
    return slot < CHANNELS && channels_[slot].drainSuccess;
}
bool SdoClient::reopenAfterDrain(uint8_t slot) {
    if (!drainSettled(slot)) return false;
    Channel& c = channels_[slot];
    c.drainSuccess = c.unresolved = false;
    c.retainedCount = 0;
    c.experimentArmed = c.experimentActive = false; // A grant from before the loss stays void.
    ++c.requalifications;
    return true;
}
uint32_t SdoClient::requalifications(uint8_t slot) const {
    return slot < CHANNELS ? channels_[slot].requalifications : 0;
}
bool SdoClient::checkWork(uint8_t slot, const SdoTransfer& work, uint64_t now) const {
    if (slot >= CHANNELS || now < lastUs_ || !eligibleSdo(slot, work, now))
        return false;
    const Channel& c = channels_[slot];
    return checkShape(c, work, now) &&
        (c.confirmed || experimentMatches(slot, work, now, false) ||
         (work.direction == SdoDirection::UPLOAD && work.readEffect != SdoReadEffect::UNQUALIFIED));
}
bool SdoClient::workEligible(uint8_t slot, const Channel& c, uint64_t now) const {
    return c.bestEffort ? bestEffortEligible(slot, now) : eligibleSdo(slot, c.work, now);
}
bool SdoClient::bestEffortEligible(uint8_t slot, uint64_t now) const {
    if (slot >= CHANNELS || now < lastUs_) return false;
    if (controllerState_ == ControllerState::BUS_OFF || controllerState_ == ControllerState::RECOVERING)
        return false;
    const int anchor = networkAnchor(slot);
    if (anchor < 0) return true;
    ServiceEligibility eligibility;
    // probeUpload is false exactly for an exhausted node or known STOPPED/INITIALIZING.
    return serviceEligibility(channels_[anchor].network, now, eligibility) && eligibility.probeUpload;
}
bool SdoClient::validBestEffortStop(uint8_t slot, const SdoTransfer& work, uint64_t now) const {
    return slot < CHANNELS && now >= lastUs_ && checkShape(channels_[slot], work, now) &&
        work.direction == SdoDirection::DOWNLOAD && work.index == 0x6040 && work.subindex == 0 &&
        work.size == 2 && work.data[0] == 0x0b && work.data[1] == 0 && !work.requirePreOperational;
}
bool SdoClient::checkBestEffortStop(uint8_t slot, const SdoTransfer& work, uint64_t now) const {
    if (!validBestEffortStop(slot, work, now) || !bestEffortEligible(slot, now)) return false;
    const Channel& c = channels_[slot];
    return !c.occupied && !c.cleanup && !c.handoffLive && !c.port.queueOccupied && !c.drainSuccess;
}
bool SdoClient::admitBestEffortStop(uint8_t slot, const SdoTransfer& work, uint64_t now,
                                    SdoRequestId& id, uint64_t initialQueueDeadline) {
    if (!checkBestEffortStop(slot, work, now) || initialQueueDeadline <= now ||
        overlap(&id, sizeof id, this, sizeof *this) || overlap(&id, sizeof id, &work, sizeof work))
        return false;
    if (!start(slot, work, now, id, initialQueueDeadline)) return false;
    channels_[slot].bestEffort = true;
    return true;
}
bool SdoClient::checkShape(const Channel& c, const SdoTransfer& work, uint64_t now) const {
    const uint8_t slot = static_cast<uint8_t>(&c - channels_);
    if (!c.configured || c.generation == UINT32_MAX ||
        !sameBinding(work.channel.binding, c.route.binding) ||
        work.channel.clientToServerId != c.route.clientToServerId ||
        work.channel.serverToClientId != c.route.serverToClientId || work.operation == 0 ||
        work.request == 0 || work.step == 0 || work.size == 0 || work.size > 32 ||
        work.deadlineUs <= now || work.queueTimeoutUs == 0 || work.responseTimeoutUs == 0 ||
        (work.direction != SdoDirection::UPLOAD && work.direction != SdoDirection::DOWNLOAD) ||
        (work.uploadSize != SdoUploadSize::EXACT && work.uploadSize != SdoUploadSize::SERVER_INDICATED) ||
        (work.uploadSize == SdoUploadSize::SERVER_INDICATED &&
         !experimentMatches(slot, work, now, false)) ||
        work.readEffect > SdoReadEffect::ACCEPTED_UNQUALIFIED)
        return false;
    return true;
}
bool SdoClient::admit(uint8_t slot, const SdoTransfer& work, uint64_t now, SdoRequestId& id,
                      uint64_t initialQueueDeadline) {
    if (!checkAdmission(slot, work, now) || initialQueueDeadline <= now ||
        overlap(&id, sizeof id, this, sizeof *this) || overlap(&id, sizeof id, &work, sizeof work))
        return false;
    return start(slot, work, now, id, initialQueueDeadline);
}
bool SdoClient::start(uint8_t slot, const SdoTransfer& work, uint64_t now, SdoRequestId& id,
                      uint64_t initialQueueDeadline) {
    Channel& c = channels_[slot];
    if (c.occupied || c.cleanup)
        return false;
    c.experimentActive = experimentMatches(slot, work, now, false);
    if (c.experimentActive) c.experimentArmed = false;
    c.work = work;
    c.admittedUs = now;
    const int anchor = networkAnchor(slot);
    c.nodeGeneration = anchor < 0 ? 0 : channels_[anchor].network.route.generation;
    ++c.generation;
    c.occupied = true;
    c.terminal = c.started = c.awaiting = c.queued = c.possible = c.txConfirmed = false;
    c.unsizedExpedited = c.replyValid = c.handoffLive = false;
    c.held = c.withdrawn = c.possibleBeforeHandoff = c.txLive = c.abortClean = false;
    c.bestEffort = false;
    c.uploaded = 0;
    c.frame = 0;
    c.queueCutoff = cutoff(now, work.deadlineUs, work.queueTimeoutUs);
    if (initialQueueDeadline < c.queueCutoff)
        c.queueCutoff = initialQueueDeadline;
    c.responseCutoff = work.deadlineUs;
    c.completion = SdoCompletion();
    c.port.sendFault = false;
    c.port.refuseSend = false;
    lastUs_ = now;
    id.owner = this;
    id.channel = slot;
    id.generation = c.generation;
    return true;
}

void SdoClient::retire(Channel& c) {
    c.port.queueOccupied = false;
    c.port.tx.bufferFull = false;
    c.cleanup = false;
    c.handoffLive = false;
    c.held = false;
    c.drainSuccess = false; // Any later local boundary outdates a settled marker.
}

void SdoClient::finish(Channel& c, SdoOutcome outcome, uint64_t now, int32_t returned,
                       bool abortValid, uint32_t abortCode) {
    if (!c.occupied || c.terminal) return;
    SdoCompletion out;
    out.transfer = c.work;
    out.outcome = outcome;
    out.completedUs = now;
    out.admittedUs = c.admittedUs;
    out.nodeGeneration = c.nodeGeneration;
    out.queued = c.queued;
    out.transmissionPossible = c.possible;
    out.transmissionConfirmed = c.txConfirmed;
    out.protocolAcknowledged = outcome == SdoOutcome::SUCCESS;
    out.effectPossible = c.possible &&
        (c.work.direction == SdoDirection::DOWNLOAD ||
         c.work.readEffect != SdoReadEffect::REVIEWED_NON_CHANGING);
    out.effectUncertain = out.effectPossible && outcome != SdoOutcome::SUCCESS;
    out.stackReturn = returned;
    out.abortValid = abortValid;
    out.abortCode = abortCode;
    out.reply = c.lastReply;
    out.replyValid = c.replyValid;
    if (outcome == SdoOutcome::SUCCESS && c.work.direction == SdoDirection::UPLOAD) {
        out.size = c.work.uploadSize == SdoUploadSize::SERVER_INDICATED ? c.uploaded : c.work.size;
        std::memcpy(out.data, c.upload, out.size);
        // Experimental permission establishes neither harmless read effects nor
        // production admission. Preserve an already established confirmation only.
        if (!c.experimentActive) c.confirmed = true;
    }
    c.cleanup = c.port.queueOccupied && c.port.queued.data[0] == 0x80 && now < c.work.deadlineUs;
    out.clientAbortQueued = c.cleanup;
    c.handoffLive = false;
    c.held = false;
    if (!c.cleanup) retire(c);
    // A matched server abort has the same correlation premise as a success, so it leaves
    // the channel open. Controller events decide from sampled evidence in invalidateNode.
    const bool settled = outcome == SdoOutcome::SUCCESS ||
                         (outcome == SdoOutcome::SERVER_ABORT && c.abortClean);
    if ((c.possible && !settled) || (outcome == SdoOutcome::TRANSPORT_ERROR && !c.withdrawn) ||
        outcome == SdoOutcome::REBOOT) c.unresolved = true;
    c.withdrawn = c.abortClean = false;
    c.drainSuccess = c.drainActive && outcome == SdoOutcome::SUCCESS;
    if (c.possible && !settled) retainPair(c, c.work.index, c.work.subindex);
    if (c.possible && settled) {
        // A late error can make a server answer twice, so a marker must not reuse this pair.
        c.lastCompleted.index = c.work.index;
        c.lastCompleted.subindex = c.work.subindex;
        c.lastCompletedValid = true;
    }
    c.drainActive = false;
    CO_SDOclientClose(&c.engine);
    c.awaiting = false;
    c.completion = out;
    c.terminal = true;
}

void SdoClient::expire(Channel& c, uint64_t now) {
    if (!c.configured) return;
    if (c.cleanup && (now >= c.work.deadlineUs || now >= c.queueCutoff)) retire(c);
    if (!c.occupied || c.terminal) return;
    if (now >= c.work.deadlineUs ||
        ((!c.started || c.port.queueOccupied || c.held) && now >= c.queueCutoff) ||
        (c.awaiting && now >= c.responseCutoff)) {
        retire(c);
        finish(c, SdoOutcome::TIMEOUT, now);
    }
}

void SdoClient::abort(Channel& c, SdoOutcome outcome, uint32_t code, uint64_t now) {
    // Only the selected engine encodes aborts. Abort staging cannot qualify effects or reopen a route.
    retire(c);
    CO_SDO_abortCode_t nativeCode = CO_SDO_AB_NONE;
    static_assert(sizeof nativeCode == sizeof code, "SDO abort must preserve 32 bits");
    std::memcpy(&nativeCode, &code, sizeof code);
    const CO_SDO_return_t returned = c.work.direction == SdoDirection::DOWNLOAD
        ? CO_SDOclientDownload(&c.engine, 0, true, false, &nativeCode, nullptr, nullptr)
        : CO_SDOclientUpload(&c.engine, 0, true, &nativeCode, nullptr, nullptr, nullptr);
    c.queueCutoff = cutoff(now, c.work.deadlineUs, c.work.queueTimeoutUs);
    finish(c, outcome, now, static_cast<int32_t>(returned), true, code);
}

void SdoClient::pump(Channel& c, uint64_t now) {
    if (!c.occupied || c.terminal) return;
    if (!c.started) {
        const CO_SDO_return_t initiated = c.work.direction == SdoDirection::DOWNLOAD
            ? CO_SDOclientDownloadInitiate(&c.engine, c.work.index, c.work.subindex, c.work.size, UINT16_MAX, false)
            : CO_SDOclientUploadInitiate(&c.engine, c.work.index, c.work.subindex, UINT16_MAX, false);
        if (initiated != CO_SDO_RT_ok_communicationEnd ||
            (c.work.direction == SdoDirection::DOWNLOAD &&
             CO_SDOclientDownloadBufWrite(&c.engine, c.work.data, c.work.size) != c.work.size)) {
            finish(c, SdoOutcome::TRANSPORT_ERROR, now, static_cast<int32_t>(initiated));
            return;
        }
        c.started = true;
    }
    const bool wasQueued = c.port.queueOccupied;
    CO_SDO_abortCode_t nativeCode = CO_SDO_AB_NONE;
    // Native per-step timers are intentionally not advanced. Owner microsecond cutoffs
    // cover admission, staging and response and cannot be renewed by native progress.
    const CO_SDO_return_t returned = c.work.direction == SdoDirection::DOWNLOAD
        ? CO_SDOclientDownload(&c.engine, 0, false, false, &nativeCode, nullptr, nullptr)
        : CO_SDOclientUpload(&c.engine, 0, false, &nativeCode, nullptr, nullptr, nullptr);
    uint32_t code = 0;
    std::memcpy(&code, &nativeCode, sizeof code); // Arbitrary vendor abort bits are not a C++ enum load.
    if (!wasQueued && c.port.queueOccupied) {
        // The first frame shares the admission queue cutoff: delaying the first
        // service turn must not grant it another queue interval.
        if (c.queued) c.queueCutoff = cutoff(now, c.work.deadlineUs, c.work.queueTimeoutUs);
        if (c.port.queued.data[0] != 0x80) c.queued = true;
    }
    if (c.work.direction == SdoDirection::UPLOAD) {
        uint8_t bytes[32];
        const size_t count = CO_SDOclientUploadBufRead(&c.engine, bytes, sizeof bytes);
        const size_t usable = c.unsizedExpedited ? c.work.size : count;
        if (usable > count || usable > size_t(c.work.size - c.uploaded)) {
            abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_DATA_LONG, now);
            return;
        }
        std::memcpy(c.upload + c.uploaded, bytes, usable);
        c.uploaded = static_cast<uint8_t>(c.uploaded + usable);
        c.unsizedExpedited = false;
    }
    if (c.port.sendFault) {
        finish(c, SdoOutcome::TRANSPORT_ERROR, now, static_cast<int32_t>(returned));
    } else if (returned == CO_SDO_RT_ok_communicationEnd) {
        if (c.work.direction == SdoDirection::UPLOAD &&
            c.work.uploadSize == SdoUploadSize::EXACT && c.uploaded != c.work.size)
            abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_DATA_SHORT, now);
        else finish(c, SdoOutcome::SUCCESS, now, static_cast<int32_t>(returned));
    } else if (returned < 0) {
        finish(c, returned == CO_SDO_RT_endedWithServerAbort ? SdoOutcome::SERVER_ABORT : SdoOutcome::CLIENT_ABORT,
               now, static_cast<int32_t>(returned), true, code);
    }
}

bool SdoClient::service(uint64_t now) {
    if (now < lastUs_) return false;
    lastUs_ = now;
    serviceNmt(now);
    for (Channel& c : channels_) {
        expire(c, now);
        pump(c, now);
    }
    return true;
}

bool SdoClient::receive(const Frame& f, uint64_t now) {
    return receiveFrame(f, false, 0, now);
}

bool SdoClient::receive(const Frame& f, uint64_t received, uint64_t now) {
    return receiveFrame(f, true, received, now);
}

bool SdoClient::receive(const Frame& f, uint64_t earliest, uint64_t latest, uint64_t now) {
    return receiveFrame(f, false, 0, now, true, earliest, latest);
}

bool SdoClient::receiveFrame(const Frame& input, bool timeKnown, uint64_t received, uint64_t now,
                           bool boundsKnown, uint64_t earliest, uint64_t latest) {
    if (now < lastUs_ || (timeKnown && received > now) ||
        (boundsKnown && (timeKnown || earliest > latest || latest > now))) return false;
    // A caller may return our borrowed observation frame; copy before replacing it.
    const Frame f = input;
    service(now);
    bool pdoMatched = false;
    const bool pdoAccepted = dispatchPdo(f, timeKnown, received, now, pdoMatched);
    if (pdoMatched) return pdoAccepted;
    bool networkIdentifier = false;
    for (const Channel& c : channels_)
        if (c.networkConfigured && (f.id == c.network.route.heartbeatId || f.id == c.network.route.emcyId))
            networkIdentifier = true;
    const bool networkAccepted = dispatchNetwork(f, nullptr, timeKnown, received, now,
                                                 boundsKnown, earliest, latest);
    if (networkIdentifier) return networkAccepted;
    for (Channel& c : channels_) {
        if (!c.configured || f.id != c.route.serverToClientId) continue;
        c.observation.channel = c.route;
        c.observation.frame = f;
        c.observation.receivedTimeKnown = timeKnown;
        c.observation.receivedUs = received;
        c.observation.processedUs = now;
        c.observation.disposition = SdoRxDisposition::REJECTED;
        c.observationValid = true;
        if (!validateFrame(f) || f.extended || f.rtr || f.fd) return false;
        if (c.bestEffort && c.occupied && !c.terminal) {
            // A reply is indistinguishable from a late reply to earlier work, so it can
            // neither confirm this stop nor withdraw its handoff. It still disproves the
            // external reuse premise, even if this stop then proves never sent.
            retainFrame(c, f);
            c.unresolved = true;
            c.observation.disposition = SdoRxDisposition::UNRESOLVED;
            return false;
        }
        if (c.drainActive && c.occupied && !c.terminal) {
            // A retained pair may be the old reply still arriving, so it cannot settle
            // the marker. Anything else on this route contradicts the sole-client premise.
            uint16_t index = 0; uint8_t subindex = 0;
            const bool known = frameMultiplexer(f, index, subindex);
            bool retainedPair = false;
            for (uint8_t i = 0; known && i < c.retainedCount; ++i)
                retainedPair |= c.retained[i].index == index && c.retained[i].subindex == subindex;
            if (retainedPair) return false;
            if (!known || index != c.work.index || subindex != c.work.subindex) {
                retainFrame(c, f);
                retire(c);
                c.observation.disposition = SdoRxDisposition::UNRESOLVED;
                finish(c, SdoOutcome::CORRELATION_LOST, now);
                return false;
            }
        }
        const bool serverAbort = c.possible && f.payloadLength == 8 && f.data[0] == 0x80;
        // A reply to the one confirmed outstanding frame, the premise a success has.
        c.abortClean = serverAbort && c.awaiting && !c.port.queueOccupied && !c.handoffLive;
        if ((c.unresolved && !c.drainActive) || !c.occupied || c.terminal ||
            (!serverAbort && (!c.awaiting || c.port.queueOccupied)) ||
            (timeKnown && received < c.handoffUs) || (boundsKnown && latest < c.handoffUs)) {
            // Observed old/unsolicited traffic disproves the external reuse premise.
            // Host tokens cannot classify a later indistinguishable response safely.
            retainFrame(c, f);
            retire(c);
            c.unresolved = true;
            c.observation.disposition = SdoRxDisposition::UNRESOLVED;
            finish(c, SdoOutcome::CORRELATION_LOST, now);
            return false;
        }
        if (f.payloadLength != 8) {
            c.observation.disposition = SdoRxDisposition::PROCESSED;
            c.lastReply = f; c.replyValid = true;
            abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_CMD, now);
            return true;
        }
        const bool initial = c.engine.state == CO_SDO_ST_UPLOAD_INITIATE_RSP ||
                             c.engine.state == CO_SDO_ST_DOWNLOAD_INITIATE_RSP;
        if (initial || f.data[0] == 0x80) {
            if ((uint16_t(f.data[1]) | uint16_t(f.data[2]) << 8) != c.work.index ||
                f.data[3] != c.work.subindex) {
                c.abortClean = false;
                retainFrame(c, f);
                return false;
            }
        }
        c.lastReply = f; c.replyValid = true;
        c.observation.disposition = SdoRxDisposition::PROCESSED;
        if (initial && f.data[0] != 0x80) {
            const bool raw = c.work.uploadSize == SdoUploadSize::SERVER_INDICATED;
            if (raw && f.data[0] != 0x40 && f.data[0] != 0x41) {
                // The selected engine derives expedited count from e/s/n bits.
                // Without s, a raw capacity cannot establish the actual count.
                if ((f.data[0] & 0xf3u) != 0x43) {
                    abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_CMD, now);
                    return true;
                }
                const uint8_t count = static_cast<uint8_t>(4 - ((f.data[0] >> 2) & 3));
                if (count > c.work.size) {
                    abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_DATA_LONG, now);
                    return true;
                }
                SdoExpectation expected = expectation(c.work);
                expected.width = count;
                SdoReply checked;
                if (!checkReply(f, expected, checked)) {
                    abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_CMD, now);
                    return true;
                }
            } else if (!raw && c.work.size <= 4) {
                SdoReply checked;
                const Status status = checkReply(f, expectation(c.work), checked);
                if (!status) {
                    const uint32_t code = status.detail == static_cast<int32_t>(SdoError::WIDTH)
                        ? CO_SDO_AB_TYPE_MISMATCH : CO_SDO_AB_CMD;
                    abort(c, SdoOutcome::CLIENT_ABORT, code, now);
                    return true;
                }
                c.unsizedExpedited = checked.kind == SdoReplyKind::EXPEDITED_UPLOAD && !checked.sizeIndicated;
            } else if (!raw && ((c.work.direction == SdoDirection::DOWNLOAD && f.data[0] != 0x60) ||
                       (c.work.direction == SdoDirection::UPLOAD && f.data[0] != 0x40 && f.data[0] != 0x41))) {
                abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_CMD, now);
                return true;
            }
            if (c.work.direction == SdoDirection::UPLOAD && (f.data[0] == 0x40 || f.data[0] == 0x41)) {
                if (!c.work.allowSegmentedUpload) {
                    if (!c.unresolved) recordSdo(c, timeKnown, received, now);
                    abort(c, SdoOutcome::UNSUPPORTED, CO_SDO_AB_UNSUPPORTED_ACCESS, now);
                    return true;
                }
                if (f.data[0] == 0x41 && (raw ? word(f.data + 4) > c.work.size :
                                               word(f.data + 4) != c.work.size)) {
                    abort(c, SdoOutcome::CLIENT_ABORT,
                          raw ? CO_SDO_AB_DATA_LONG : CO_SDO_AB_TYPE_MISMATCH, now);
                    return true;
                }
            }
        } else if (c.engine.state == CO_SDO_ST_UPLOAD_SEGMENT_RSP && f.data[0] != 0x80 &&
                   (f.data[0] & 0xE0) == 0 && (f.data[0] & 0x10) == c.engine.toggle) {
            const uint8_t count = static_cast<uint8_t>(7 - ((f.data[0] >> 1) & 7));
            if (count > c.work.size - c.uploaded ||
                ((f.data[0] & 1) && c.work.uploadSize == SdoUploadSize::EXACT &&
                 count != c.work.size - c.uploaded)) {
                abort(c, SdoOutcome::CLIENT_ABORT,
                      count > c.work.size - c.uploaded ? CO_SDO_AB_DATA_LONG : CO_SDO_AB_DATA_SHORT, now);
                return true;
            }
        }
        // A peer abort can arrive while its next segment is still in our staging
        // slot. After object correlation, retire that slot before closing the engine.
        if (serverAbort) retire(c);
        c.handoffLive = false;
        c.awaiting = false;
        StackFrame incoming = native(f);
        c.port.rx.CANrx_callback(c.port.rx.object, &incoming);
        pump(c, now); // Drain the single native mailbox before accepting another frame.
        if (!c.terminal || c.completion.outcome == SdoOutcome::SUCCESS || c.completion.outcome == SdoOutcome::SERVER_ABORT)
            if (!c.unresolved) recordSdo(c, timeKnown, received, now);
        return true;
    }
    return false;
}

bool SdoClient::nextTx(uint8_t slot, uint64_t now, Frame& f, SdoTxId& id) {
    if (slot >= CHANNELS || now < lastUs_ || overlap(&f, sizeof f, this, sizeof *this) ||
        overlap(&id, sizeof id, this, sizeof *this) || overlap(&f, sizeof f, &id, sizeof id)) return false;
    service(now);
    Channel& c = channels_[slot];
    if (!c.configured || !c.port.queueOccupied || c.frame == UINT32_MAX) return false;
    const bool cleanup = c.cleanup;
    const uint64_t cleanupCutoff = c.queueCutoff;
    if ((!c.occupied || c.terminal) && !cleanup) return false;
    if (!cleanup && !workEligible(slot, c, now)) {
        retire(c); finish(c, SdoOutcome::BINDING_INVALIDATED, now); return false;
    }
    const Frame candidate = wire(c.port.queued);
    if (!validateFrame(candidate) ||
        (c.frame == 0 && c.work.size <= 4 && !checkRequest(candidate, expectation(c.work)))) {
        retire(c); finish(c, SdoOutcome::TRANSPORT_ERROR, now); return false;
    }
    ++c.frame;
    id.request.owner = this; id.request.channel = slot; id.request.generation = c.generation;
    id.frame = c.frame;
    f = candidate;
    c.port.lastHandoff = c.port.queued;
    if (c.port.handedOff != UINT32_MAX) ++c.port.handedOff;
    retire(c);
    c.handoffLive = true;
    c.txLive = true;
    c.handoffUs = now;
    c.handoffCutoff = cleanupCutoff;
    if (!cleanup) {
        c.possibleBeforeHandoff = c.possible;
        c.possible = true;
        c.awaiting = true;
        c.responseCutoff = cutoff(now, c.work.deadlineUs, c.work.responseTimeoutUs);
        c.handoffCutoff = c.responseCutoff;
        // The drive's ordinary reply must not count as an unexpected frame on this route.
        if (c.bestEffort) retainPair(c, c.work.index, c.work.subindex);
    }
    return true;
}

bool SdoClient::canTransmit(SdoTxId id, uint64_t now) const {
    if (id.request.owner != this || id.request.channel >= CHANNELS || now < lastUs_)
        return false;
    const Channel& c = channels_[id.request.channel];
    return c.handoffLive && id.request.generation != 0 &&
           id.request.generation == c.generation && id.frame != 0 && id.frame == c.frame &&
           now < c.handoffCutoff && now < c.work.deadlineUs &&
           ((!c.occupied || c.terminal) || workEligible(id.request.channel, c, now));
}

bool SdoClient::holdHandoff(SdoTxId id) {
    if (!valid(id.request)) return false;
    Channel& c = channels_[id.request.channel];
    if (!c.handoffLive || id.frame == 0 || id.frame != c.frame) return false;
    c.held = true;
    c.txLive = false;
    if (c.occupied && !c.terminal) {
        c.possible = c.possibleBeforeHandoff;
        c.awaiting = false;
        c.handoffCutoff = c.queueCutoff < c.work.deadlineUs ? c.queueCutoff : c.work.deadlineUs;
    }
    return true;
}

bool SdoClient::noteSubmitted(SdoTxId id, uint64_t now) {
    if (!canTransmit(id, now)) return false;
    lastUs_ = now;
    Channel& c = channels_[id.request.channel];
    if (!c.held) return false;
    c.held = false;
    c.txLive = true;
    if (c.occupied && !c.terminal) {
        c.possible = true;
        c.awaiting = true;
        c.handoffUs = now;
        c.responseCutoff = cutoff(now, c.work.deadlineUs, c.work.responseTimeoutUs);
        c.handoffCutoff = c.responseCutoff;
    }
    return true;
}

bool SdoClient::withdrawUnsent(SdoTxId id, uint64_t now) {
    if (!valid(id.request) || now < lastUs_) return false;
    lastUs_ = now;
    Channel& c = channels_[id.request.channel];
    expire(c, now);
    if (c.terminal || !c.held || id.frame == 0 || id.frame != c.frame) return false;
    retire(c);
    c.withdrawn = true;
    finish(c, SdoOutcome::TRANSPORT_ERROR, now);
    return true;
}

bool SdoClient::noteTxRetired(SdoTxId id) {
    if (id.request.owner != this || id.request.channel >= CHANNELS) return false;
    Channel& c = channels_[id.request.channel];
    if (id.request.generation == 0 || id.request.generation != c.generation || id.frame == 0 ||
        id.frame != c.frame) return false;
    c.txLive = false;
    // Without this a reported cleanup abort would hold the route's handoff live forever.
    if (!c.occupied || c.terminal) c.handoffLive = false;
    return true;
}
bool SdoClient::transmissionPossible(SdoRequestId id) const {
    return valid(id) && (channels_[id.channel].possible || channels_[id.channel].txLive);
}
bool SdoClient::confirmTx(SdoTxId id, uint64_t now) {
    if (now < lastUs_) return false;
    noteTxRetired(id);
    if (!valid(id.request)) return false;
    lastUs_ = now;
    Channel& c = channels_[id.request.channel];
    expire(c, now);
    if (c.terminal || id.frame == 0 || id.frame != c.frame || !c.possible) return false;
    c.txConfirmed = true;
    c.handoffLive = false;
    if (c.bestEffort) finish(c, SdoOutcome::UNCONFIRMED_SENT, now);
    return true;
}

bool SdoClient::cancel(SdoRequestId id, uint64_t now) {
    if (now < lastUs_ || !valid(id)) return false;
    lastUs_ = now;
    Channel& c = channels_[id.channel];
    expire(c, now);
    if (c.terminal) return false;
    retire(c);
    finish(c, SdoOutcome::CANCELLED, now);
    return true;
}

bool SdoClient::busOff(uint64_t now) {
    return reportController(ControllerState::BUS_OFF, now);
}

bool SdoClient::reboot(uint8_t node, uint64_t now) {
    if (node == 0 || node > 127 || now < lastUs_) return false;
    lastUs_ = now;
    for (unsigned i = 0; i < CHANNELS; ++i)
        if (channels_[i].networkConfigured && channels_[i].network.route.binding.endpoint.address == node)
            invalidateNode(i, NetworkInvalidation::BOOTUP, now);
    for (Channel& c : channels_) {
        expire(c, now);
        if (!c.configured || c.route.binding.endpoint.address != node) continue;
        retire(c);
        c.unresolved = true;
        finish(c, SdoOutcome::REBOOT, now);
    }
    return true;
}

bool SdoClient::invalidate(uint8_t slot, uint64_t now) {
    if (slot >= CHANNELS || !channels_[slot].configured || now < lastUs_) return false;
    if (networkAnchor(slot) >= 0) return invalidateNetwork(slot, NetworkInvalidation::BINDING, now);
    lastUs_ = now;
    Channel& c = channels_[slot];
    expire(c, now);
    retire(c);
    c.unresolved = true;
    finish(c, SdoOutcome::BINDING_INVALIDATED, now);
    return true;
}

bool SdoClient::transportFailure(SdoRequestId id, uint64_t now) {
    if (!valid(id) || now < lastUs_) return false;
    lastUs_ = now;
    Channel& c = channels_[id.channel];
    expire(c, now);
    if (c.terminal) return false;
    retire(c);
    c.txLive = false;
    c.port.sendFault = true;
    finish(c, SdoOutcome::TRANSPORT_ERROR, now);
    return true;
}

const SdoCompletion* SdoClient::result(SdoRequestId id) const {
    return valid(id) && channels_[id.channel].terminal ? &channels_[id.channel].completion : nullptr;
}
const SdoRxObservation* SdoClient::observation(uint8_t slot) const {
    return slot < CHANNELS && channels_[slot].observationValid ? &channels_[slot].observation : nullptr;
}
bool SdoClient::release(SdoRequestId id) {
    if (!result(id)) return false;
    channels_[id.channel].occupied = false;
    return true;
}
bool SdoClient::unresolved(uint8_t slot) const {
    return slot >= CHANNELS || !channels_[slot].configured || channels_[slot].unresolved;
}
const CO_CANmodule_t* SdoClient::port(uint8_t slot) const {
    return slot < CHANNELS && channels_[slot].configured ? &channels_[slot].port : nullptr;
}
} // namespace CANOPEN
} // namespace MotorControlCAN
