// SPDX-License-Identifier: MIT
// Independent passive-wire vectors through the real optional service owner.
// The synthetic routes and read effects establish no vendor capability.
#include "SdoClient.h"
#include <MotorControlCAN/Axis.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;

namespace {
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)

template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};

M::Binding binding(uint8_t node = 1, uint16_t channel = 0) {
    M::Binding out;
    out.logicalAxisId = 100u + node + 1000u * channel;
    out.endpoint.profile = 41;
    out.endpoint.address = node;
    out.endpoint.channel = channel;
    out.bindingGeneration = 3;
    out.configurationGeneration = 7;
    return out;
}
C::NetworkRoute network(uint8_t node = 1) {
    C::NetworkRoute out;
    out.binding = binding(node);
    out.generation = 11;
    out.heartbeatId = 0x700u + node;
    out.emcyId = 0x80u + node;
    return out;
}
C::SdoChannel sdo(uint8_t node = 1, uint16_t channel = 0) {
    C::SdoChannel out;
    out.binding = binding(node, channel);
    out.clientToServerId = 0x600u + node + 0x20u * channel;
    out.serverToClientId = 0x580u + node + 0x20u * channel;
    return out;
}
M::Frame heartbeat(uint32_t id, uint8_t state = 5) {
    M::Frame out;
    out.id = id;
    out.dlc = out.payloadLength = 1;
    out.data[0] = state;
    return out;
}
M::Frame emergency(uint32_t id, uint16_t code = 0x8123, uint8_t reg = 0xc1) {
    M::Frame out;
    out.id = id;
    out.dlc = out.payloadLength = 8;
    out.data[0] = static_cast<uint8_t>(code);
    out.data[1] = static_cast<uint8_t>(code >> 8);
    out.data[2] = reg;
    const uint8_t manufacturer[] = {0xf1, 0x02, 0x83, 0x74, 0xa5};
    std::memcpy(out.data + 3, manufacturer, sizeof manufacturer);
    return out;
}
M::Frame uploadReply(uint8_t node = 1) {
    M::Frame out;
    out.id = 0x580u + node;
    out.dlc = out.payloadLength = 8;
    const uint8_t bytes[] = {0x4b, 0x41, 0x60, 0x00, 0x35, 0xa2, 0, 0};
    std::memcpy(out.data, bytes, sizeof bytes);
    return out;
}
C::SdoTransfer transfer(uint8_t node = 1, bool download = false, uint16_t channel = 0) {
    C::SdoTransfer out;
    out.channel = sdo(node, channel);
    out.operation = 17;
    out.request = 19;
    out.step = 23;
    out.index = 0x6041;
    out.size = 2;
    out.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
    out.data[0] = 0xa6; out.data[1] = 0x39;
    out.deadlineUs = 100000;
    out.queueTimeoutUs = 1000;
    out.responseTimeoutUs = 1000;
    return out;
}
bool sameFrame(const M::Frame& a, const M::Frame& b) {
    return a.id == b.id && a.dlc == b.dlc && a.payloadLength == b.payloadLength &&
        a.extended == b.extended && a.rtr == b.rtr && a.fd == b.fd && a.brs == b.brs &&
        a.esi == b.esi && std::memcmp(a.data, b.data, sizeof a.data) == 0;
}
C::NetworkObservation observation(C::SdoClient& owner, C::NetworkDisposition expected) {
    C::NetworkObservation out;
    CHECK(owner.takeNetworkObservation(out));
    CHECK(out.disposition == expected);
    return out;
}
void drain(C::SdoClient& owner) {
    C::NetworkObservation out;
    while (owner.takeNetworkObservation(out)) {}
}
void fresh(const C::NodeSnapshot& node, uint64_t now, uint64_t age, C::HeartbeatFreshness expected) {
    C::HeartbeatFreshness out = C::HeartbeatFreshness::EXPIRED;
    CHECK(C::heartbeatFreshness(node, now, age, out));
    CHECK(out == expected);
}
C::SdoRequestId admit(C::SdoClient& owner, const C::SdoTransfer& work, uint8_t slot, uint64_t now) {
    C::SdoRequestId out;
    CHECK(owner.admit(slot, work, now, out));
    CHECK(!owner.result(out));
    return out;
}
C::SdoTxId handoff(C::SdoClient& owner, uint8_t slot, uint64_t now, uint32_t expectedId = 0x601) {
    M::Frame frame;
    C::SdoTxId out;
    CHECK(owner.nextTx(slot, now, frame, out));
    CHECK(frame.id == expectedId);
    CHECK(frame.payloadLength == 8 && !frame.fd && !frame.rtr);
    CHECK(owner.canTransmit(out, now));
    return out;
}

// The application keeps its interpretation/position cache here. The callback
// uses the ordinary installed coordinate invalidator and owns a copied event.
struct DependentCache {
    M::AxisConfig axis;
    M::AxisReference reference;
    M::PreparedTarget prepared;
    bool configurationValid = true, mapValid = true, driveValid = true, modeValid = true;
    bool commandScaleValid = true, encoderScaleValid = true;
    unsigned calls = 0;
    uint32_t faults = 2;
    uint16_t rawFault = 0x8123;
    M::Generation mapGeneration = 7;
    C::NetworkInvalidationEvent notification;
    M::Status invalidation;

    explicit DependentCache(M::Binding target = binding()) {
        axis.binding = target;
        axis.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::READBACK);
        axis.units.encoder.countsPerUnit = M::UnitScale(4000, 1, M::ScaleSource::READBACK);
        axis.units.encoder.sourceId = 6;
        axis.supportedRelativeBases = 1;
        axis.originKnown = axis.encoderOriginKnown = axis.softLimitsKnown = true;
        axis.originSource = axis.encoderOriginSource = M::ScaleSource::READBACK;
        axis.softMinimum = -10000; axis.softMaximum = 10000;
        reference.binding = target;
        reference.referenceGeneration = axis.referenceGeneration;
        reference.nativeKnown = reference.idle = reference.stationary = true;
        reference.source = M::ScaleSource::QUALIFIED;
        reference.observedUs = 0; reference.nowUs = 1; reference.maximumAgeUs = 1000;
        M::PositionRequest request;
        request.configurationGeneration = target.configurationGeneration;
        request.frame = M::CoordinateFrame::MOTOR;
        request.unit = M::PositionUnit::DEGREES;
        request.value = M::Rational(90);
        request.referencePolicy = M::ReferencePolicy::REQUIRED;
        CHECK(M::preparePosition(request, axis, &reference, prepared));
        CHECK(prepared.endpointNative == 800);
        CHECK(M::validatePreparedTarget(prepared, axis, &reference, 1));
    }
    static void invalidate(void* context, const C::NetworkInvalidationEvent& event) {
        DependentCache& cache = *static_cast<DependentCache*>(context);
        ++cache.calls;
        cache.notification = event;
        cache.invalidation = M::invalidateAxisReference(cache.axis, cache.reference, M::ReferenceLoss::COORDINATES);
        cache.axis.units.commandStepsPerMotorTurn = M::UnitScale();
        cache.axis.units.encoder = M::EncoderScale();
        cache.configurationValid = cache.mapValid = cache.driveValid = cache.modeValid = false;
        cache.commandScaleValid = cache.encoderScaleValid = false;
        cache.mapGeneration = 0;
    }
    C::NetworkObserver observer() {
        C::NetworkObserver out;
        out.context = this; out.invalidate = invalidate;
        return out;
    }
    void revoked(uint64_t now) const {
        CHECK(calls != 0);
        CHECK(!configurationValid && !mapValid && !driveValid && !modeValid);
        CHECK(!commandScaleValid && !encoderScaleValid && mapGeneration == 0);
        CHECK(axis.units.commandStepsPerMotorTurn.source == M::ScaleSource::UNKNOWN);
        CHECK(axis.units.encoder.countsPerUnit.source == M::ScaleSource::UNKNOWN);
        CHECK(!axis.originKnown && !axis.encoderOriginKnown && !axis.softLimitsKnown);
        CHECK(!reference.nativeKnown);
        CHECK(!M::validatePreparedTarget(prepared, axis, &reference, now));
        CHECK(faults == 2 && rawFault == 0x8123);
    }
};

void heartbeatStatesAndExplicitProducerPolicy() {
    C::SdoClient owner;
    C::NetworkRoute route = network();
    CHECK(owner.configureNetwork(0, route, C::NetworkObserver(), 0));
    CHECK(owner.node(0) && !owner.node(1));
    fresh(*owner.node(0), 0, 1000, C::HeartbeatFreshness::PRODUCER_UNKNOWN);
    CHECK(owner.setProducerInterval(0, route, 0, 0));
    fresh(*owner.node(0), 0, 1000, C::HeartbeatFreshness::DISABLED);
    CHECK(owner.setProducerInterval(0, route, 100, 0));
    fresh(*owner.node(0), 0, 1000, C::HeartbeatFreshness::NEVER_OBSERVED);
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId), route, 0, 0));
    CHECK(owner.node(0)->heartbeat.present && owner.node(0)->heartbeat.receivedTimeKnown);
    CHECK(owner.node(0)->heartbeat.receivedUs == 0);
    fresh(*owner.node(0), 1000, 1000, C::HeartbeatFreshness::FRESH);
    fresh(*owner.node(0), 1001, 1000, C::HeartbeatFreshness::EXPIRED);
    C::HeartbeatFreshness output = C::HeartbeatFreshness::DISABLED;
    CHECK(!C::heartbeatFreshness(*owner.node(0), 1, 0, output));
    CHECK(output == C::HeartbeatFreshness::DISABLED);
    CHECK(owner.setProducerInterval(0, route, 200, 1));
    CHECK(owner.node(0)->heartbeat.receivedUs == 0);
    const uint8_t states[] = {4, 5, 127};
    const C::NmtState expected[] = {C::NmtState::STOPPED, C::NmtState::OPERATIONAL, C::NmtState::PRE_OPERATIONAL};
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId, states[i]), route, 10 + i, 20 + i));
        CHECK(owner.node(0)->nmt == expected[i]);
        CHECK(owner.node(0)->nmtTime.receivedUs == 10 + i);
        CHECK(owner.node(0)->heartbeat.processedUs == 20 + i);
    }
    drain(owner);
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId, 0), route, 30, 40));
    CHECK(owner.node(0)->route.generation == route.generation + 1);
    CHECK(!owner.node(0)->heartbeat.present && !owner.node(0)->producerKnown);
    CHECK(owner.node(0)->nmt == C::NmtState::INITIALIZING && owner.node(0)->nmtTime.present);
    const C::NetworkObservation boot = observation(owner, C::NetworkDisposition::ACCEPTED);
    CHECK(boot.event.kind == C::NetworkEventKind::BOOTUP && boot.event.state == C::NmtState::INITIALIZING);
    CHECK(boot.route.generation == route.generation && boot.time.receivedUs == 30 && boot.time.processedUs == 40);
    route = owner.node(0)->route;
    CHECK(owner.setProducerInterval(0, route, 100, 41));
    fresh(*owner.node(0), 41, 1000, C::HeartbeatFreshness::NEVER_OBSERVED);
    CHECK(!owner.receive(heartbeat(route.heartbeatId), 42));
    CHECK(!owner.node(0)->heartbeat.present);
    observation(owner, C::NetworkDisposition::UNQUALIFIED);
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId), route, 43, 44));
    fresh(*owner.node(0), 44, 1, C::HeartbeatFreshness::FRESH);

    C::SdoClient unknownCapture;
    CHECK(unknownCapture.configureNetwork(0, network(), C::NetworkObserver(), 0));
    CHECK(unknownCapture.setProducerInterval(0, network(), 100, 0));
    CHECK(unknownCapture.receive(heartbeat(network().heartbeatId), 10));
    CHECK(!unknownCapture.node(0)->heartbeat.receivedTimeKnown);
    fresh(*unknownCapture.node(0), 10, 1000, C::HeartbeatFreshness::UNQUALIFIED);
}

void captureOrderAndIndependentEmergencyTimes() {
    C::SdoClient owner;
    const C::NetworkRoute route = network();
    CHECK(owner.configureNetwork(0, route, C::NetworkObserver(), 100));
    CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId), route, 99, 110));
    observation(owner, C::NetworkDisposition::STALE);
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId, 4), route, 100, 120));
    observation(owner, C::NetworkDisposition::ACCEPTED);
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId, 5), route, 300, 400));
    observation(owner, C::NetworkDisposition::ACCEPTED);
    const Saved<C::NodeSnapshot> latest(*owner.node(0));
    CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId, 127), route, 250, 410));
    observation(owner, C::NetworkDisposition::STALE);
    CHECK(latest.unchanged(*owner.node(0)));
    CHECK(!owner.receive(heartbeat(route.heartbeatId, 127), 411));
    observation(owner, C::NetworkDisposition::STALE);
    CHECK(latest.unchanged(*owner.node(0)));
    CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId), route, 500, 499));
    CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId), route, 200, 399));
    CHECK(latest.unchanged(*owner.node(0)));
    C::NetworkObservation untouched;
    untouched.frame.id = 0x321;
    const Saved<C::NetworkObservation> noRecord(untouched);
    CHECK(!owner.takeNetworkObservation(untouched));
    CHECK(noRecord.unchanged(untouched));
    C::HeartbeatFreshness result = C::HeartbeatFreshness::DISABLED;
    CHECK(!C::heartbeatFreshness(*owner.node(0), 399, 1000, result));
    CHECK(result == C::HeartbeatFreshness::DISABLED);

    // Event classes have independent capture order. A newer heartbeat cannot
    // discard valid older fault evidence delivered by the same owner later.
    M::Frame emcy = emergency(route.emcyId);
    CHECK(owner.receiveNetwork(emcy, route, 200, 420));
    CHECK(owner.node(0)->heartbeat.receivedUs == 300);
    CHECK(owner.node(0)->emcyTime.receivedUs == 200 && owner.node(0)->emcyTime.processedUs == 420);
    CHECK(owner.node(0)->emcy.errorCode == 0x8123 && owner.node(0)->emcy.errorRegister == 0xc1);
    CHECK(owner.node(0)->emcy.manufacturer[0] == 0xf1 && owner.node(0)->emcy.manufacturer[4] == 0xa5);
    const C::NetworkObservation first = observation(owner, C::NetworkDisposition::ACCEPTED);
    emcy.data[0] = 0; emcy.data[7] = 0;
    CHECK(first.frame.data[0] == 0x23 && first.event.manufacturer[4] == 0xa5);
    CHECK(owner.node(0)->emcy.frame.data[0] == 0x23 && owner.node(0)->emcy.manufacturer[4] == 0xa5);
    CHECK(owner.receiveNetwork(emergency(route.emcyId, 0, 0), route, 201, 421));
    observation(owner, C::NetworkDisposition::ACCEPTED);
    CHECK(owner.node(0)->emcyCount == 2 && owner.node(0)->emcy.errorCode == 0);
    CHECK(owner.receiveNetwork(emergency(route.emcyId, 0xffff, 0xff), route, 150, 422));
    observation(owner, C::NetworkDisposition::ACCEPTED);
    CHECK(owner.node(0)->emcyCount == 3 && owner.node(0)->emcy.errorCode == 0);
    CHECK(owner.node(0)->emcyTime.receivedUs == 201);
    const Saved<C::NetworkEvent> fault(owner.node(0)->emcy);
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId), route, 423, 423));
    CHECK(fault.unchanged(owner.node(0)->emcy) && owner.node(0)->emcyCount == 3);
}

void rejectedRepresentationsAndCopiedUnknowns() {
    const C::NetworkRoute route = network();
    for (unsigned fault = 0; fault < 14; ++fault) {
        C::SdoClient owner;
        CHECK(owner.configureNetwork(0, route, C::NetworkObserver(), 0));
        M::Frame frame = fault < 9 ? heartbeat(route.heartbeatId) : emergency(route.emcyId);
        switch (fault) {
        case 0: frame.data[0] = 1; break;
        case 1: frame.data[0] = 0x80; break;
        case 2: frame.data[0] = 0xff; break;
        case 3: frame.extended = true; break;
        case 4: frame.rtr = true; frame.payloadLength = 0; break;
        case 5: frame.fd = true; break;
        case 6: frame.dlc = frame.payloadLength = 0; break;
        case 7: frame.dlc = frame.payloadLength = 2; break;
        case 8: frame.brs = true; break;
        case 9: frame.dlc = frame.payloadLength = 7; break;
        case 10: frame.extended = true; break;
        case 11: frame.rtr = true; frame.payloadLength = 0; break;
        case 12: frame.esi = true; break;
        default: frame.payloadLength = 6; break;
        }
        const Saved<C::NodeSnapshot> before(*owner.node(0));
        CHECK(!owner.receiveNetwork(frame, route, 1, 2));
        CHECK(before.unchanged(*owner.node(0)));
        const C::NetworkObservation rejected = observation(owner, C::NetworkDisposition::REJECTED);
        CHECK(sameFrame(rejected.frame, frame) && !rejected.decoded);
    }
    for (unsigned field = 0; field < 12; ++field) {
        C::SdoClient owner;
        CHECK(owner.configureNetwork(0, route, C::NetworkObserver(), 0));
        C::NetworkRoute source = route;
        switch (field) {
        case 0: ++source.generation; break;
        case 1: source.generation = 0; break;
        case 2: ++source.binding.logicalAxisId; break;
        case 3: ++source.binding.endpoint.bus; break;
        case 4: ++source.binding.endpoint.profile; break;
        case 5: ++source.binding.endpoint.address; break;
        case 6: ++source.binding.endpoint.channel; break;
        case 7: ++source.binding.endpoint.subaxis; break;
        case 8: ++source.binding.bindingGeneration; break;
        case 9: ++source.binding.configurationGeneration; break;
        case 10: ++source.heartbeatId; break;
        default: ++source.emcyId; break;
        }
        CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId), source, 1, 2));
        CHECK(!owner.node(0)->heartbeat.present);
        const C::NetworkObservation stale = observation(owner, C::NetworkDisposition::STALE);
        CHECK(M::sameBinding(stale.route.binding, source.binding));
        CHECK(stale.route.generation == source.generation && stale.time.generation == source.generation);
        CHECK(stale.route.heartbeatId == source.heartbeatId && stale.route.emcyId == source.emcyId);
    }
    C::SdoClient owner;
    CHECK(owner.configureNetwork(0, route, C::NetworkObserver(), 0));
    M::Frame unknown = heartbeat(0x321, 0);
    CHECK(!owner.receive(unknown, 1, 2));
    const C::NetworkObservation copied = observation(owner, C::NetworkDisposition::UNKNOWN_ROUTE);
    CHECK(sameFrame(copied.frame, unknown) && !copied.decoded);
    CHECK(!M::validateBinding(copied.route.binding));
    unknown.data[0] = 0x7f;
    CHECK(copied.frame.data[0] == 0 && !owner.node(0)->nmtTime.present);
    CHECK(!owner.node(1));
}

void passiveConfigurationAndIdentifierExclusion() {
    C::SdoClient owner;
    CHECK(owner.configureNetwork(0, network(), C::NetworkObserver(), 0));
    CHECK(!owner.port(0));
    CHECK(owner.receiveNetwork(heartbeat(network().heartbeatId), network(), 0, 0));
    CHECK(owner.configure(0, sdo(), true, true));
    CHECK(owner.node(0)->heartbeat.present);
    CHECK(owner.configureNetwork(1, network(2), C::NetworkObserver(), 0));
    CHECK(owner.configure(1, sdo(2), true, true));
    CHECK(!owner.configureNetwork(2, network(3), C::NetworkObserver(), 0));
    CHECK(!owner.node(2));
    for (unsigned cross = 0; cross < 4; ++cross) {
        C::SdoClient sdoFirst;
        CHECK(sdoFirst.configure(0, sdo(), true, true));
        C::NetworkRoute candidate = network(2);
        const uint32_t occupied = cross & 1 ? sdo().serverToClientId : sdo().clientToServerId;
        if (cross & 2) candidate.emcyId = occupied;
        else candidate.heartbeatId = occupied;
        CHECK(!sdoFirst.configureNetwork(1, candidate, C::NetworkObserver(), 0));
        CHECK(!sdoFirst.node(1));

        C::SdoClient networkFirst;
        CHECK(networkFirst.configureNetwork(0, network(), C::NetworkObserver(), 0));
        C::SdoChannel service = sdo(2);
        const uint32_t eventId = cross & 1 ? network().heartbeatId : network().emcyId;
        if (cross & 2) service.serverToClientId = eventId;
        else service.clientToServerId = eventId;
        CHECK(!networkFirst.configure(1, service, true, true));
        CHECK(!networkFirst.port(1));

        candidate = network(2);
        if (cross & 2) candidate.emcyId = eventId;
        else candidate.heartbeatId = eventId;
        CHECK(!networkFirst.configureNetwork(1, candidate, C::NetworkObserver(), 0));
    }
    C::NetworkRoute bad = network(); bad.binding = binding(2);
    CHECK(!owner.configureNetwork(0, bad, C::NetworkObserver(), 1));
    C::SdoClient observerCheck;
    C::NetworkObserver partial;
    DependentCache cache;
    partial.context = &cache;
    CHECK(!observerCheck.configureNetwork(0, network(), partial, 0));
    partial.context = nullptr; partial.invalidate = DependentCache::invalidate;
    CHECK(!observerCheck.configureNetwork(0, network(), partial, 0));
    CHECK(observerCheck.configureNetwork(0, network(), cache.observer(), 0));
    CHECK(!observerCheck.setNetworkObserver(0, partial));
    CHECK(cache.calls == 0);
}

void eventsInterleaveWithTwoNormalSdoConsumers() {
    C::SdoClient owner;
    const C::NetworkRoute a = network(), b = network(2);
    CHECK(owner.configure(0, sdo(), true, true));
    CHECK(owner.configure(1, sdo(2), true, true));
    CHECK(owner.configureNetwork(0, a, C::NetworkObserver(), 0));
    CHECK(owner.configureNetwork(1, b, C::NetworkObserver(), 0));
    const C::SdoRequestId first = admit(owner, transfer(), 0, 1);
    const C::SdoRequestId second = admit(owner, transfer(2), 1, 1);
    handoff(owner, 0, 2); handoff(owner, 1, 2, 0x602);
    // Pre-operational preserves ordinary SDO service. Current Stopped now
    // retires SDO work under the prompt-16 eligibility contract.
    CHECK(owner.receiveNetwork(heartbeat(b.heartbeatId, 127), b, 3, 4));
    CHECK(owner.receiveNetwork(emergency(a.emcyId), a, 3, 5));
    CHECK(!owner.result(first) && !owner.result(second));
    CHECK(owner.receive(uploadReply(), 6, 7));
    CHECK(owner.result(first) && owner.result(first)->outcome == C::SdoOutcome::SUCCESS);
    CHECK(owner.node(0)->sdo.present && owner.node(0)->sdo.receivedUs == 6 && owner.node(0)->sdo.processedUs == 7);
    CHECK(!owner.node(0)->nmtTime.present && !owner.node(0)->heartbeat.present);
    const Saved<C::SdoCompletion> success(*owner.result(first));
    CHECK(owner.receiveNetwork(heartbeat(a.heartbeatId), a, 8, 8));
    CHECK(owner.receiveNetwork(emergency(b.emcyId, 0x7301, 0x20), b, 8, 9));
    CHECK(owner.receive(uploadReply(2), 10, 10));
    CHECK(owner.result(second) && owner.result(second)->data[0] == 0x35 && owner.result(second)->data[1] == 0xa2);
    CHECK(success.unchanged(*owner.result(first)));
    CHECK(owner.node(0) != owner.node(1));
    CHECK(owner.node(0)->emcy.errorCode == 0x8123 && owner.node(1)->emcy.errorCode == 0x7301);
    CHECK(owner.node(0)->nmt == C::NmtState::OPERATIONAL && owner.node(1)->nmt == C::NmtState::PRE_OPERATIONAL);
    CHECK(owner.node(1)->sdo.present && owner.node(1)->sdo.receivedUs == 10);
    CHECK(owner.droppedNetworkObservations() == 0);
    const uint32_t identifiers[] = {b.heartbeatId, a.emcyId, a.heartbeatId, b.emcyId};
    for (unsigned i = 0; i < 4; ++i) {
        const C::NetworkObservation event = observation(owner, C::NetworkDisposition::ACCEPTED);
        CHECK(event.frame.id == identifiers[i]);
    }
    C::NetworkObservation empty;
    CHECK(!owner.takeNetworkObservation(empty));
    CHECK(owner.release(first) && owner.release(second));
    CHECK(owner.node(0)->sdo.present && owner.node(1)->sdo.present);
}

void fullHistoryCannotSuppressResetAndCacheInvalidation() {
    C::SdoClient owner;
    const C::NetworkRoute route = network();
    DependentCache cache;
    CHECK(owner.configure(0, sdo(), true, true));
    CHECK(owner.configureNetwork(0, route, cache.observer(), 0));
    CHECK(owner.setProducerInterval(0, route, 100, 0));
    const Saved<M::PreparedTarget> frozen(cache.prepared);
    for (unsigned i = 0; i < 4; ++i)
        CHECK(owner.receiveNetwork(emergency(route.emcyId), route, i + 1, i + 1));
    const C::SdoRequestId request = admit(owner, transfer(1, true), 0, 5);
    const C::SdoTxId tx = handoff(owner, 0, 6);
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId, 0), route, 7, 8));
    CHECK(owner.droppedNetworkObservations() == 1);
    CHECK(cache.calls == 1 && cache.notification.cause == C::NetworkInvalidation::BOOTUP);
    CHECK(cache.notification.generation == route.generation + 1 && cache.notification.processedUs == 8);
    CHECK(M::sameBinding(cache.notification.previous.binding, route.binding));
    cache.revoked(8);
    CHECK(frozen.unchanged(cache.prepared));
    const C::SdoCompletion* result = owner.result(request);
    CHECK(result && result->outcome == C::SdoOutcome::REBOOT && result->transmissionPossible);
    CHECK(result->effectPossible && result->effectUncertain && !result->protocolAcknowledged);
    const Saved<C::SdoCompletion> terminal(*result);
    CHECK(owner.unresolved(0) && !owner.canTransmit(tx, 8));
    CHECK(!owner.confirmTx(tx, 8));
    CHECK(owner.node(0)->emcyCount == 4 && owner.node(0)->emcy.errorCode == 0x8123);
    for (unsigned i = 0; i < 4; ++i) {
        const C::NetworkObservation retained = observation(owner, C::NetworkDisposition::ACCEPTED);
        CHECK(retained.event.kind == C::NetworkEventKind::EMCY && retained.time.receivedUs == i + 1);
    }
    CHECK(owner.service(9));
    CHECK(terminal.unchanged(*owner.result(request)));
    CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId), route, 9, 9));
    observation(owner, C::NetworkDisposition::STALE);
    C::NetworkRoute current = owner.node(0)->route;
    CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId), current, 8, 10));
    observation(owner, C::NetworkDisposition::STALE);
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId), current, 11, 12));
    observation(owner, C::NetworkDisposition::ACCEPTED);
    CHECK(owner.node(0)->emcyCount == 4 && owner.node(0)->emcy.errorCode == 0x8123);
    CHECK(!owner.node(0)->producerKnown && owner.unresolved(0));
    cache.revoked(12);
    CHECK(cache.calls == 1 && terminal.unchanged(*owner.result(request)));
    M::Frame next;
    C::SdoTxId noTx;
    CHECK(!owner.nextTx(0, 12, next, noTx));
    CHECK(owner.release(request));
    C::SdoRequestId noReplay;
    CHECK(!owner.admit(0, transfer(1, true), 13, noReplay));
}

void allInvalidationCausesAndTerminalRetention() {
    const C::NetworkInvalidation causes[] = {C::NetworkInvalidation::BOOTUP, C::NetworkInvalidation::BINDING,
        C::NetworkInvalidation::DIALECT, C::NetworkInvalidation::CONFIGURATION,
        C::NetworkInvalidation::RECOVERY, C::NetworkInvalidation::BUS_OFF};
    for (unsigned i = 0; i < sizeof causes / sizeof causes[0]; ++i) {
        C::SdoClient owner;
        DependentCache cache;
        const C::NetworkRoute route = network();
        CHECK(owner.configure(0, sdo(), true, true));
        CHECK(owner.configureNetwork(0, route, cache.observer(), 0));
        CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId), route, 1, 1));
        CHECK(owner.receiveNetwork(emergency(route.emcyId), route, 2, 2));
        const C::SdoRequestId request = admit(owner, transfer(1, true), 0, 3);
        const C::SdoTxId tx = handoff(owner, 0, 4);
        CHECK(owner.invalidateNetwork(0, causes[i], 5));
        CHECK(cache.calls == 1 && cache.notification.cause == causes[i]);
        cache.revoked(5);
        CHECK(!owner.node(0)->heartbeat.present && !owner.node(0)->nmtTime.present && !owner.node(0)->sdo.present);
        CHECK(owner.node(0)->emcyCount == 1 && owner.node(0)->emcy.errorCode == 0x8123);
        CHECK(owner.node(0)->route.generation == route.generation + 1 && !owner.node(0)->exhausted);
        const C::SdoOutcome expected = causes[i] == C::NetworkInvalidation::BOOTUP ? C::SdoOutcome::REBOOT :
            causes[i] == C::NetworkInvalidation::BUS_OFF ? C::SdoOutcome::BUS_OFF : C::SdoOutcome::BINDING_INVALIDATED;
        CHECK(owner.result(request) && owner.result(request)->outcome == expected);
        CHECK(owner.result(request)->effectUncertain && owner.unresolved(0));
        CHECK(!owner.canTransmit(tx, 5));
        const Saved<C::SdoCompletion> terminal(*owner.result(request));
        CHECK(owner.reportController(C::ControllerState::AVAILABLE, 6));
        CHECK(owner.controllerState() == C::ControllerState::AVAILABLE && owner.unresolved(0));
        CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId), owner.node(0)->route, 7, 7));
        CHECK(terminal.unchanged(*owner.result(request)) && cache.calls == 1);
        cache.revoked(7);
    }

    // A completed command remains its original result after a later lifecycle
    // event. The node's last SDO time is independently invalidated.
    C::SdoClient owner;
    DependentCache cache;
    CHECK(owner.configure(0, sdo(), true, true));
    CHECK(owner.configureNetwork(0, network(), cache.observer(), 0));
    const C::SdoRequestId done = admit(owner, transfer(), 0, 1);
    handoff(owner, 0, 2);
    CHECK(owner.receive(uploadReply(), 3, 3));
    const Saved<C::SdoCompletion> success(*owner.result(done));
    CHECK(owner.reboot(1, 4));
    CHECK(success.unchanged(*owner.result(done)) && !owner.node(0)->sdo.present);
    CHECK(owner.busOff(5));
    CHECK(success.unchanged(*owner.result(done)) && cache.calls == 2);
    CHECK(owner.reportController(C::ControllerState::RECOVERING, 6));
    CHECK(success.unchanged(*owner.result(done)) && cache.calls == 3);
    CHECK(owner.reportController(C::ControllerState::AVAILABLE, 7));
    CHECK(owner.unresolved(0) && cache.calls == 3);
}

void sameNodeChannelsShareFactsAndObserverDeduplication() {
    for (bool sharedObserver : {false, true}) {
        C::SdoClient owner;
        DependentCache first;
        DependentCache second(binding(1, 1));
        CHECK(owner.configure(0, sdo(), true, true));
        CHECK(owner.configure(1, sdo(1, 1), true, true));
        CHECK(owner.configureNetwork(0, network(), first.observer(), 0));
        CHECK(owner.setNetworkObserver(1, sharedObserver ? first.observer() : second.observer()));
        CHECK(owner.node(0) == owner.node(1));
        C::NetworkRoute duplicate = network(); duplicate.binding = binding(1, 1);
        CHECK(!owner.configureNetwork(1, duplicate, C::NetworkObserver(), 0));
        const C::SdoRequestId a = admit(owner, transfer(), 0, 1);
        const C::SdoRequestId b = admit(owner, transfer(1, true, 1), 1, 1);
        const C::SdoTxId oldTx = handoff(owner, 0, 2);
        CHECK(owner.service(2)); // The other channel still has only local staging.
        CHECK(owner.receiveNetwork(heartbeat(network().heartbeatId, 0), network(), 3, 3));
        CHECK(first.calls == 1 && second.calls == (sharedObserver ? 0u : 1u));
        CHECK(owner.unresolved(0) && owner.unresolved(1) && !owner.canTransmit(oldTx, 3));
        CHECK(owner.result(a)->outcome == C::SdoOutcome::REBOOT && owner.result(a)->effectUncertain);
        CHECK(owner.result(b)->outcome == C::SdoOutcome::REBOOT && !owner.result(b)->transmissionPossible);
        if (!sharedObserver) {
            CHECK(M::sameBinding(second.notification.previous.binding, binding(1, 1)));
            second.revoked(3);
        }
        first.revoked(3);
    }
}

void epochExhaustionAndDelayedResetCannotRestoreCoordinates() {
    C::SdoClient owner;
    C::NetworkRoute route = network();
    route.generation = UINT32_MAX;
    DependentCache cache;
    cache.axis.binding.configurationGeneration = UINT32_MAX;
    cache.axis.referenceGeneration = UINT32_MAX;
    CHECK(owner.configure(0, sdo(), true, true));
    CHECK(owner.configureNetwork(0, route, cache.observer(), 0));
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId), route, 1, 1));
    CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId, 0), route, 2, 2));
    CHECK(owner.node(0)->exhausted && owner.node(0)->route.generation == UINT32_MAX);
    CHECK(cache.calls == 1 && cache.notification.generation == UINT32_MAX);
    CHECK(!cache.invalidation && cache.axis.exhausted);
    cache.revoked(2);
    CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId), route, 3, 3));
    CHECK(!owner.setProducerInterval(0, route, 100, 3));
    fresh(*owner.node(0), 3, 100, C::HeartbeatFreshness::UNQUALIFIED);
    CHECK(owner.invalidateNetwork(0, C::NetworkInvalidation::RECOVERY, 4));
    CHECK(cache.calls == 2 && owner.node(0)->route.generation == UINT32_MAX);

    C::SdoClient delayed;
    DependentCache dependent;
    const C::NetworkRoute initial = network();
    CHECK(delayed.configureNetwork(0, initial, dependent.observer(), 0));
    CHECK(delayed.receiveNetwork(heartbeat(initial.heartbeatId), initial, 100, 100));
    CHECK(delayed.receiveNetwork(heartbeat(initial.heartbeatId, 0), initial, 50, 110));
    CHECK(dependent.calls == 1 && delayed.node(0)->route.generation == initial.generation + 1);
    CHECK(delayed.node(0)->nmt == C::NmtState::OPERATIONAL);
    CHECK(!delayed.node(0)->heartbeat.present);
    CHECK(delayed.node(0)->nmtTime.generation != delayed.node(0)->route.generation);
    dependent.revoked(110);
    const C::NetworkRoute current = delayed.node(0)->route;
    CHECK(!delayed.receiveNetwork(heartbeat(current.heartbeatId), current, 101, 111));
    CHECK(delayed.receiveNetwork(heartbeat(current.heartbeatId), current, 112, 112));
    dependent.revoked(112);
}

void checkedSdoTimeRejectsMalformedTrafficAndFaultsRemainIndependent() {
    for (unsigned fault = 0; fault < 6; ++fault) {
        C::SdoClient owner;
        CHECK(owner.configure(0, sdo(), true, true));
        CHECK(owner.configureNetwork(0, network(), C::NetworkObserver(), 0));
        const C::SdoRequestId request = admit(owner, transfer(), 0, 1);
        handoff(owner, 0, 2);
        M::Frame frame = uploadReply();
        switch (fault) {
        case 0: frame.data[1] = 0x40; break; // Different object.
        case 1: frame.data[3] = 1; break; // Different subindex.
        case 2: frame.extended = true; break;
        case 3: frame.dlc = frame.payloadLength = 7; break;
        case 4: frame.data[0] = 0x43; break; // Four-byte upload cannot satisfy two bytes.
        default: frame.data[0] = 0x61; break; // Invalid command envelope.
        }
        (void)owner.receive(frame, 3, 3);
        CHECK(!owner.node(0)->sdo.present);
        CHECK(!owner.node(0)->nmtTime.present && !owner.node(0)->heartbeat.present);
        if (const C::SdoCompletion* result = owner.result(request))
            CHECK(!result->protocolAcknowledged);
    }
    C::SdoClient owner;
    DependentCache cache;
    CHECK(owner.configure(0, sdo(), true, true));
    CHECK(owner.configureNetwork(0, network(), cache.observer(), 0));
    const C::SdoRequestId request = admit(owner, transfer(1, true), 0, 1);
    handoff(owner, 0, 2);
    M::Frame abort = uploadReply();
    const uint8_t aborted[] = {0x80, 0x41, 0x60, 0, 0, 0, 2, 6};
    std::memcpy(abort.data, aborted, sizeof aborted);
    CHECK(owner.receive(abort, 3, 4));
    CHECK(owner.result(request)->outcome == C::SdoOutcome::SERVER_ABORT);
    CHECK(owner.result(request)->abortValid && owner.result(request)->abortCode == 0x06020000);
    CHECK(owner.node(0)->sdo.present && owner.node(0)->sdo.receivedUs == 3);
    const Saved<C::SdoCompletion> failed(*owner.result(request));
    CHECK(owner.receiveNetwork(emergency(network().emcyId), network(), 5, 5));
    CHECK(owner.receiveNetwork(heartbeat(network().heartbeatId), network(), 6, 6));
    CHECK(failed.unchanged(*owner.result(request)));
    CHECK(cache.calls == 0 && cache.faults == 2 && cache.rawFault == 0x8123);
    CHECK(owner.node(0)->emcy.errorCode == 0x8123 && owner.node(0)->emcyCount == 1);
    CHECK(owner.node(0)->sdo.receivedUs == 3 && owner.node(0)->nmt == C::NmtState::OPERATIONAL);
}

void sdoHistoryCannotDowngradeNewerServiceProof() {
    for (bool captureKnown : {false, true}) {
        C::SdoClient owner;
        CHECK(owner.configure(0, sdo(), true, true));
        CHECK(owner.configureNetwork(0, network(), C::NetworkObserver(), 0));
        const C::SdoRequestId request = admit(owner, transfer(), 0, 1);
        handoff(owner, 0, 2);
        CHECK(owner.service(100));
        // A separate confirmed channel establishes a newer service premise for
        // the same physical node while this channel's response is buffered.
        CHECK(owner.configure(1, sdo(1, 1), true, true));
        CHECK(owner.setServiceAge(0, 5, 100));
        CHECK(owner.node(0)->serviceQualification.receivedUs == 100);
        if (captureKnown) CHECK(owner.receive(uploadReply(), 3, 101));
        else CHECK(owner.receive(uploadReply(), 101));
        CHECK(owner.result(request) && owner.result(request)->outcome == C::SdoOutcome::SUCCESS);
        CHECK(owner.node(0)->sdo.present && owner.node(0)->sdo.receivedTimeKnown == captureKnown);
        CHECK(owner.node(0)->sdo.processedUs == 101);
        if (captureKnown) CHECK(owner.node(0)->sdo.receivedUs == 3);
        CHECK(owner.node(0)->serviceQualification.receivedTimeKnown);
        CHECK(owner.node(0)->serviceQualification.receivedUs == 100 && !owner.node(0)->serviceFromSdo);
        C::ServiceEligibility service;
        CHECK(C::serviceEligibility(*owner.node(0), 101, service) && service.ordinarySdo);
        CHECK(owner.release(request));
        const C::SdoRequestId next = admit(owner, transfer(1, true), 0, 102);
        CHECK(owner.cancel(next, 102));
        CHECK(C::serviceEligibility(*owner.node(0), 106, service) && !service.ordinarySdo);
    }
}

void networkAttachmentPreservesIndependentSdoHistory() {
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        C::SdoClient owner;
        const uint8_t activeSlot = scenario == 1 ? 1 : 0;
        const bool captureKnown = scenario != 2;
        const uint64_t admittedUs = captureKnown ? 1 : 100;
        const uint64_t handoffUs = captureKnown ? 2 : 100;
        const uint64_t processedUs = captureKnown ? 3 : 101;
        if (!captureKnown) CHECK(owner.service(100));
        CHECK(owner.configure(activeSlot, sdo(1, activeSlot), true, true));
        const C::SdoRequestId request = admit(owner, transfer(1, false, activeSlot), activeSlot, admittedUs);
        handoff(owner, activeSlot, handoffUs, 0x601u + 0x20u * activeSlot);
        M::Frame response = uploadReply(); response.id += 0x20u * activeSlot;
        if (captureKnown) CHECK(owner.receive(response, 3, processedUs));
        else CHECK(owner.receive(response, processedUs));
        CHECK(owner.result(request) && owner.result(request)->outcome == C::SdoOutcome::SUCCESS);
        if (captureKnown) {
            CHECK(owner.service(100));
            const uint8_t confirmedSlot = static_cast<uint8_t>(1 - activeSlot);
            CHECK(owner.configure(confirmedSlot, sdo(1, confirmedSlot), true, true));
        }
        CHECK(owner.configureNetwork(0, network(), C::NetworkObserver(), 102));
        const C::NodeSnapshot& node = *owner.node(0);
        // Slot iteration must not couple the newest external service premise
        // to the independent history of the last matching SDO response.
        CHECK(node.serviceQualification.receivedTimeKnown && node.serviceQualification.receivedUs == 100);
        CHECK(!node.serviceFromSdo);
        CHECK(node.sdo.present && node.sdo.receivedTimeKnown == captureKnown);
        CHECK(node.sdo.processedUs == processedUs);
        if (captureKnown) CHECK(node.sdo.receivedUs == 3);
    }
}

void directWithdrawalAndControllerScope() {
    C::SdoClient owner;
    DependentCache first, second(binding(2));
    CHECK(owner.controllerState() == C::ControllerState::UNKNOWN);
    CHECK(owner.configure(0, sdo(), true, true));
    CHECK(owner.configure(1, sdo(2), true, true));
    CHECK(owner.configureNetwork(0, network(), first.observer(), 0));
    CHECK(owner.configureNetwork(1, network(2), second.observer(), 0));
    CHECK(owner.setProducerInterval(1, network(2), 100, 0));
    const C::SdoRequestId interrupted = admit(owner, transfer(1, true), 0, 1);
    const C::SdoRequestId independent = admit(owner, transfer(2), 1, 1);
    const C::SdoTxId old = handoff(owner, 0, 2);
    handoff(owner, 1, 2, 0x602);
    CHECK(owner.receiveNetwork(heartbeat(network(2).heartbeatId), network(2), 3, 3));
    const Saved<C::NodeSnapshot> unaffected(*owner.node(1));
    CHECK(owner.invalidate(0, 4));
    CHECK(first.calls == 1 && second.calls == 0);
    CHECK(first.notification.cause == C::NetworkInvalidation::BINDING);
    first.revoked(4);
    CHECK(unaffected.unchanged(*owner.node(1)) && !owner.unresolved(1));
    CHECK(owner.result(interrupted) && owner.result(interrupted)->effectUncertain);
    CHECK(!owner.canTransmit(old, 4) && !owner.result(independent));
    CHECK(owner.receive(uploadReply(2), 5, 5));
    CHECK(owner.result(independent) && owner.result(independent)->outcome == C::SdoOutcome::SUCCESS);
    const Saved<C::SdoCompletion> resultA(*owner.result(interrupted)), resultB(*owner.result(independent));
    CHECK(owner.reportController(C::ControllerState::BUS_OFF, 6));
    CHECK(owner.controllerState() == C::ControllerState::BUS_OFF);
    CHECK(first.calls == 2 && second.calls == 1 && owner.unresolved(0) && owner.unresolved(1));
    CHECK(first.notification.cause == C::NetworkInvalidation::BUS_OFF);
    CHECK(second.notification.cause == C::NetworkInvalidation::BUS_OFF);
    first.revoked(6); second.revoked(6);
    CHECK(!owner.node(1)->producerKnown && !owner.node(1)->heartbeat.present && !owner.node(1)->sdo.present);
    CHECK(owner.reportController(C::ControllerState::RECOVERING, 7));
    CHECK(first.calls == 3 && second.calls == 2);
    CHECK(first.notification.cause == C::NetworkInvalidation::RECOVERY);
    const Saved<C::NetworkInvalidationEvent> callbackCopy(second.notification);
    CHECK(owner.reportController(C::ControllerState::AVAILABLE, 8));
    CHECK(first.calls == 3 && second.calls == 2);
    CHECK(owner.receiveNetwork(heartbeat(network(2).heartbeatId), owner.node(1)->route, 9, 9));
    CHECK(callbackCopy.unchanged(second.notification));
    CHECK(resultA.unchanged(*owner.result(interrupted)) && resultB.unchanged(*owner.result(independent)));
    first.revoked(9); second.revoked(9);
    const Saved<C::NodeSnapshot> nodeA(*owner.node(0)), nodeB(*owner.node(1));
    CHECK(!owner.reportController(static_cast<C::ControllerState>(255), 10));
    CHECK(!owner.reportController(C::ControllerState::BUS_OFF, 8));
    CHECK(!owner.invalidateNetwork(0, static_cast<C::NetworkInvalidation>(255), 10));
    CHECK(!owner.invalidateNetwork(0, C::NetworkInvalidation::BINDING, 8));
    CHECK(!owner.reboot(0, 10) && !owner.reboot(128, 10));
    CHECK(nodeA.unchanged(*owner.node(0)) && nodeB.unchanged(*owner.node(1)));
    CHECK(first.calls == 3 && second.calls == 2 && owner.controllerState() == C::ControllerState::AVAILABLE);
}

void passiveInvalidationCannotAdmitFirstSdoOrReuseOldEpoch() {
    for (unsigned boundary = 0; boundary < 4; ++boundary) {
        C::SdoClient owner;
        C::NetworkRoute route = network();
        if (boundary == 3) route.generation = UINT32_MAX;
        CHECK(owner.configureNetwork(0, route, C::NetworkObserver(), 0));
        if (boundary == 0 || boundary == 3)
            CHECK(owner.receiveNetwork(heartbeat(route.heartbeatId, 0), route, 1, 1));
        else if (boundary == 1) CHECK(owner.reportController(C::ControllerState::BUS_OFF, 1));
        else CHECK(owner.invalidateNetwork(0, C::NetworkInvalidation::DIALECT, 1));
        CHECK(!owner.configure(0, sdo(), true, true));
        CHECK(!owner.configure(1, sdo(1, 1), true, true));
        CHECK(!owner.port(0) && !owner.port(1));
        const Saved<C::NodeSnapshot> revoked(*owner.node(0));
        CHECK(!owner.setProducerInterval(0, route, 100, 2));
        CHECK(revoked.unchanged(*owner.node(0)));
        CHECK(!owner.receiveNetwork(heartbeat(route.heartbeatId), route, 2, 2));
        C::SdoRequestId noWork;
        CHECK(!owner.admit(0, transfer(1, true), 3, noWork));
        CHECK(!noWork.owner && noWork.generation == 0);
    }

    C::SdoClient limits;
    const C::NetworkRoute route = network();
    CHECK(limits.configureNetwork(0, route, C::NetworkObserver(), 0));
    CHECK(limits.setProducerInterval(0, route, 65535, 0));
    const uint64_t end = std::numeric_limits<uint64_t>::max();
    CHECK(limits.receiveNetwork(heartbeat(route.heartbeatId), route, end, end));
    fresh(*limits.node(0), end, 1, C::HeartbeatFreshness::FRESH);
    CHECK(limits.invalidateNetwork(0, C::NetworkInvalidation::RECOVERY, end));
    CHECK(!limits.receiveNetwork(heartbeat(route.heartbeatId), limits.node(0)->route, end, end));
    CHECK(!limits.node(0)->heartbeat.present);
}
} // namespace

int main() {
    heartbeatStatesAndExplicitProducerPolicy();
    captureOrderAndIndependentEmergencyTimes();
    rejectedRepresentationsAndCopiedUnknowns();
    passiveConfigurationAndIdentifierExclusion();
    eventsInterleaveWithTwoNormalSdoConsumers();
    fullHistoryCannotSuppressResetAndCacheInvalidation();
    allInvalidationCausesAndTerminalRetention();
    sameNodeChannelsShareFactsAndObserverDeduplication();
    epochExhaustionAndDelayedResetCannotRestoreCoordinates();
    checkedSdoTimeRejectsMalformedTrafficAndFaultsRemainIndependent();
    sdoHistoryCannotDowngradeNewerServiceProof();
    networkAttachmentPreservesIndependentSdoHistory();
    directWithdrawalAndControllerScope();
    passiveInvalidationCannotAdmitFirstSdoOrReuseOldEpoch();
    std::printf("network_events: %u checks passed\n", checks);
}
