// SPDX-License-Identifier: MIT
#include "SdoClient.h"

namespace MotorControlCAN {
namespace CANOPEN {
namespace {
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool sameObject(const PdoObject& a, const PdoObject& b) {
    return a.index == b.index && a.subindex == b.subindex && a.widthBytes == b.widthBytes &&
        a.sign == b.sign && a.role == b.role && a.rpdo == b.rpdo && a.tpdo == b.tpdo;
}
bool sameLayout(const PdoLayout& a, const PdoLayout& b) {
    if (a.profile != b.profile || a.payloadLength != b.payloadLength ||
        a.mapping.direction != b.mapping.direction || a.mapping.number != b.mapping.number ||
        a.mapping.count != b.mapping.count) return false;
    for (unsigned i = 0; i < PDO_ENTRIES; ++i)
        if (a.mapping.words[i] != b.mapping.words[i] ||
            (i < a.mapping.count && !sameObject(a.fields[i], b.fields[i]))) return false;
    return true;
}
bool sameRoute(const PdoRoute& a, const PdoRoute& b) {
    return sameBinding(a.binding, b.binding) && a.generation == b.generation &&
        a.cobId == b.cobId && a.extended == b.extended && sameLayout(a.layout, b.layout);
}
bool validObserver(PdoObserver observer) {
    return observer.context ? observer.sample && observer.invalidate : !observer.sample && !observer.invalidate;
}
bool validCommunication(const PdoMapEvidence& evidence) {
    return evidence.communicationKnown && (evidence.cobIdParameter & 0xbffff800u) == 0 &&
        (evidence.cobIdParameter & 0x7ffu) == evidence.route.cobId &&
        (evidence.transmissionType <= 240 || evidence.transmissionType >= 254);
}
} // namespace

bool pdoFreshness(const PdoRecord& record, uint64_t now, uint64_t age, PdoFreshness& output) {
    if (!age || now < record.boundaryUs || (record.confirmed && now < record.map.observedUs) ||
        (record.qualified && now < record.qualification.establishedUs) ||
        (record.samplePresent && (now < record.observation.time.processedUs ||
         (record.observation.time.receivedTimeKnown && now < record.observation.time.receivedUs))) ||
        overlap(&record, sizeof record, &output, sizeof output)) return false;
    PdoFreshness result;
    if (!record.confirmed) result = PdoFreshness::NO_MAP;
    else if (!record.qualified) result = PdoFreshness::UNQUALIFIED;
    else if (!record.samplePresent || !record.observation.time.receivedTimeKnown ||
        !sameRoute(record.observation.sample.route, record.map.route) ||
        record.observation.evidenceId != record.qualification.evidenceId ||
        record.observation.time.receivedUs <= record.qualification.establishedUs) result = PdoFreshness::NEVER_OBSERVED;
    else result = now - record.observation.time.receivedUs <= age ? PdoFreshness::FRESH : PdoFreshness::EXPIRED;
    output = result;
    return true;
}
bool SdoClient::pdoIdentifier(uint32_t id, bool extended) const {
    for (const PdoSlot& slot : pdos_)
        if (slot.bound && slot.record.map.route.cobId == id && slot.record.map.route.extended == extended) return true;
    return false;
}
const PdoRecord* SdoClient::pdo(uint8_t slot) const {
    return slot < PDO_ROUTES && pdos_[slot].bound ? &pdos_[slot].record : nullptr;
}
void SdoClient::revokePdo(unsigned slot, bool mapUncertain, uint64_t now) {
    PdoSlot& target = pdos_[slot];
    if (!target.bound) return;
    target.record.qualified = false;
    if (mapUncertain) target.record.confirmed = false;
    target.record.boundaryUs = now;
    target.reused = true;
    if (target.observer.invalidate) {
        const PdoRoute previous = target.record.map.route;
        target.observer.invalidate(target.observer.context, previous);
    }
}
void SdoClient::revokeNodePdos(unsigned anchor, bool mapUncertain, uint64_t now) {
    for (unsigned i = 0; i < PDO_ROUTES; ++i)
        if (pdos_[i].bound && pdos_[i].anchor == anchor) revokePdo(i, mapUncertain, now);
}
bool SdoClient::configurePdo(uint8_t slot, uint8_t nodeSlot, const PdoMapEvidence& input,
                             const PdoProfile& profile, PdoObserver observer, uint64_t now) {
    const int anchor = networkAnchor(nodeSlot);
    if (slot >= PDO_ROUTES || anchor < 0 || nmtOccupied_ || now < lastUs_ || !validObserver(observer)) return false;
    const PdoMapEvidence evidence = input;
    const NodeSnapshot& node = channels_[anchor].network;
    const Binding& binding = channels_[nodeSlot].configured ? channels_[nodeSlot].route.binding : node.route.binding;
    PdoLayout checked;
    if (!validatePdoRoute(evidence.route) || !validatePdoMap(evidence.route.layout.mapping, profile, checked) ||
        !sameLayout(checked, evidence.route.layout) || !sameBinding(binding, evidence.route.binding) || node.exhausted ||
        evidence.nodeGeneration != node.route.generation || !validCommunication(evidence) || !evidence.evidenceId ||
        (evidence.source != PdoMapSource::REVIEWED_CONFIGURATION && evidence.source != PdoMapSource::SDO_READBACK) ||
        evidence.observedUs > now || evidence.startedUs > evidence.observedUs || evidence.startedUs < node.boundaryUs ||
        (node.strictBoundary && evidence.startedUs == node.boundaryUs)) return false;
    PdoSlot& target = pdos_[slot];
    if (target.bound && (target.anchor != anchor || target.record.map.route.cobId != evidence.route.cobId ||
        target.record.map.route.extended != evidence.route.extended ||
        !sameBinding(target.record.map.route.binding, evidence.route.binding) ||
        evidence.route.generation <= target.record.map.route.generation ||
        evidence.startedUs <= target.record.boundaryUs)) return false;
    for (unsigned i = 0; i < PDO_ROUTES; ++i) {
        if (i == slot || !pdos_[i].bound) continue;
        const PdoSlot& other = pdos_[i];
        if (other.record.map.route.cobId == evidence.route.cobId &&
            other.record.map.route.extended == evidence.route.extended) return false;
        if (other.anchor == anchor && other.record.map.route.layout.mapping.direction == evidence.route.layout.mapping.direction &&
            other.record.map.route.layout.mapping.number == evidence.route.layout.mapping.number &&
            (other.record.confirmed || evidence.startedUs <= other.record.boundaryUs)) return false;
    }
    for (const Channel& channel : channels_) {
        if (channel.configured && (channel.route.clientToServerId == evidence.route.cobId ||
                                  channel.route.serverToClientId == evidence.route.cobId)) return false;
        if (channel.networkConfigured && (channel.network.route.heartbeatId == evidence.route.cobId ||
                                           channel.network.route.emcyId == evidence.route.cobId)) return false;
    }
    // Copy candidates before callbacks: callers may supply one of our borrowed records.
    if (target.bound) revokePdo(slot, true, now);
    target.bound = true; target.anchor = static_cast<uint8_t>(anchor);
    target.record.map = evidence; target.record.confirmed = true; target.record.qualified = false;
    target.record.boundaryUs = now; target.observer = observer;
    lastUs_ = now;
    return true;
}
bool SdoClient::qualifyPdo(uint8_t slot, const PdoQualification& input, uint64_t now) {
    if (slot >= PDO_ROUTES || !pdos_[slot].bound || now < lastUs_ ||
        controllerState_ == ControllerState::BUS_OFF || controllerState_ == ControllerState::RECOVERING) return false;
    const PdoQualification evidence = input;
    PdoSlot& target = pdos_[slot];
    const NodeSnapshot& node = channels_[target.anchor].network;
    if (!target.record.confirmed || target.record.qualified || node.exhausted || !evidence.evidenceId ||
        !sameRoute(evidence.route, target.record.map.route) || evidence.nodeGeneration != node.route.generation ||
        evidence.nodeGeneration != target.record.map.nodeGeneration ||
        (evidence.basis != PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED && evidence.basis != PdoSampleBasis::DISTINCT_VERIFIED_ROUTE) ||
        (target.reused && evidence.basis == PdoSampleBasis::DISTINCT_VERIFIED_ROUTE) ||
        evidence.establishedUs > now || evidence.establishedUs < target.record.boundaryUs ||
        (target.reused && evidence.establishedUs == target.record.boundaryUs)) return false;
    target.record.qualification = evidence;
    target.record.qualified = true;
    lastUs_ = now;
    return true;
}
bool SdoClient::invalidatePdo(uint8_t slot, bool mapUncertain, uint64_t now) {
    if (slot >= PDO_ROUTES || !pdos_[slot].bound || now < lastUs_) return false;
    lastUs_ = now;
    revokePdo(slot, mapUncertain, now);
    return true;
}
bool SdoClient::dispatchPdo(const Frame& frame, bool known, uint64_t received, uint64_t now, bool& matched) {
    matched = false;
    for (PdoSlot& target : pdos_) {
        if (!target.bound || target.record.map.route.cobId != frame.id || target.record.map.route.extended != frame.extended) continue;
        matched = true;
        PdoRecord& record = target.record;
        const NodeSnapshot& node = channels_[target.anchor].network;
        NetworkObservation diagnostic;
        diagnostic.route = node.route; diagnostic.route.binding = record.map.route.binding;
        diagnostic.frame = frame; diagnostic.pdo = true; diagnostic.mappingGeneration = record.map.route.generation;
        diagnostic.time.present = true; diagnostic.time.receivedTimeKnown = known;
        diagnostic.time.receivedUs = received; diagnostic.time.processedUs = now;
        diagnostic.time.generation = node.route.generation;
        PdoSample candidate;
        ServiceEligibility service;
        if (!decodePdo(frame, record.map.route, candidate)) diagnostic.disposition = NetworkDisposition::REJECTED;
        else if (!record.confirmed || !record.qualified || node.exhausted ||
            controllerState_ == ControllerState::BUS_OFF || controllerState_ == ControllerState::RECOVERING ||
            record.map.nodeGeneration != node.route.generation || !known ||
            received <= record.qualification.establishedUs || received <= record.boundaryUs ||
            !serviceEligibility(node, now, service) || !service.tpdoReceive)
            diagnostic.disposition = NetworkDisposition::UNQUALIFIED;
        else if (record.samplePresent && record.observation.sample.route.generation == record.map.route.generation &&
                 received < record.observation.time.receivedUs) diagnostic.disposition = NetworkDisposition::STALE;
        else {
            PdoObservation observation;
            observation.sample = candidate;
            observation.time = diagnostic.time; observation.time.generation = record.map.route.generation;
            observation.evidenceId = record.qualification.evidenceId;
            record.observation = observation; record.samplePresent = true;
            diagnostic.disposition = NetworkDisposition::ACCEPTED;
            if (target.observer.sample) target.observer.sample(target.observer.context, observation);
        }
        recordNetwork(diagnostic);
        return diagnostic.disposition == NetworkDisposition::ACCEPTED;
    }
    return false;
}
} // namespace CANOPEN
} // namespace MotorControlCAN
