// SPDX-License-Identifier: MIT
// Synthetic external evidence tests the owner contract, not hardware qualification.
#include "SdoClient.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    bool unchanged(const T& value) const { return std::memcmp(bytes, &value, sizeof value) == 0; }
};
M::Binding binding(unsigned node = 1) {
    M::Binding out; out.logicalAxisId = 100 + node; out.endpoint.profile = 41;
    out.endpoint.address = node; out.bindingGeneration = 3; out.configurationGeneration = 7;
    return out;
}
C::NetworkRoute network(unsigned node = 1) {
    C::NetworkRoute out; out.binding = binding(node); out.generation = 11;
    out.heartbeatId = 0x700 + node; out.emcyId = 0x80 + node; return out;
}
C::SdoChannel channel(unsigned node = 1) {
    C::SdoChannel out; out.binding = binding(node); out.clientToServerId = 0x600 + node;
    out.serverToClientId = 0x580 + node; return out;
}
M::Frame frame(uint32_t id, uint8_t length = 8) {
    M::Frame out; out.id = id; out.dlc = out.payloadLength = length; return out;
}
M::Frame heartbeat(uint8_t state) {
    M::Frame out = frame(0x701, 1); out.data[0] = state; return out;
}
C::PdoMapEvidence mapping(unsigned number = 2, uint32_t id = 0x281, uint64_t now = 0) {
    C::PdoMap map; CHECK(L::pdoTemplate(C::PdoDirection::TPDO, static_cast<uint16_t>(number), map));
    C::PdoMapEvidence out; out.route.binding = binding(); out.route.generation = 1; out.route.cobId = id;
    CHECK(C::validatePdoMap(map, L::pdoProfile(41), out.route.layout));
    out.nodeGeneration = 11; out.startedUs = out.observedUs = now; out.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
    out.evidenceId = 100; out.communicationKnown = true; out.cobIdParameter = id; out.transmissionType = 255;
    return out;
}
C::PdoQualification proof(const C::PdoMapEvidence& map, uint64_t now = 0) {
    C::PdoQualification out; out.route = map.route; out.nodeGeneration = map.nodeGeneration;
    out.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
    out.establishedUs = now; out.evidenceId = 200; return out;
}
struct Consumer {
    unsigned samples = 0, invalidations = 0;
    bool valid = false;
    C::PdoObservation copy;
    C::PdoRoute revoked;
    static void sample(void* context, const C::PdoObservation& value) {
        Consumer& self = *static_cast<Consumer*>(context); ++self.samples; self.valid = true; self.copy = value;
    }
    static void invalidate(void* context, const C::PdoRoute& value) {
        Consumer& self = *static_cast<Consumer*>(context); ++self.invalidations; self.valid = false; self.revoked = value;
    }
    C::PdoObserver observer() {
        C::PdoObserver out; out.context = this; out.sample = sample; out.invalidate = invalidate; return out;
    }
};
void setup(C::SdoClient& owner) { CHECK(owner.configureNetwork(0, network(), C::NetworkObserver(), 0)); }
void install(C::SdoClient& owner, const C::PdoMapEvidence& map, Consumer& consumer, uint8_t slot = 0, uint64_t now = 0) {
    CHECK(owner.configurePdo(slot, 0, map, L::pdoProfile(41), consumer.observer(), now));
}
void drain(C::SdoClient& owner) { C::NetworkObservation value; while (owner.takeNetworkObservation(value)) {} }
void freshness(const C::PdoRecord& record, uint64_t now, uint64_t age, C::PdoFreshness expected) {
    C::PdoFreshness out = C::PdoFreshness::NO_MAP; CHECK(C::pdoFreshness(record, now, age, out)); CHECK(out == expected);
}

void actualMapAndAtomicSamples() {
    C::SdoClient owner; setup(owner); Consumer consumer;
    C::PdoMapEvidence map = mapping();
    C::PdoMapEvidence invalid = map; invalid.source = C::PdoMapSource::TEMPLATE;
    CHECK(!owner.configurePdo(0, 0, invalid, L::pdoProfile(41), consumer.observer(), 0)); CHECK(!owner.pdo(0));
    invalid = map; invalid.communicationKnown = false;
    CHECK(!owner.configurePdo(0, 0, invalid, L::pdoProfile(41), consumer.observer(), 0));
    install(owner, map, consumer);
    CHECK(owner.pdo(0)->confirmed && !owner.pdo(0)->qualified);
    M::Frame payload = frame(map.route.cobId); payload.data[0] = 0xff; payload.data[1] = 0xff;
    payload.data[2] = 0xff; payload.data[3] = 0xff; payload.data[4] = 0x00; payload.data[7] = 0x80;
    CHECK(!owner.receive(payload, 1, 1)); CHECK(consumer.samples == 0);
    freshness(*owner.pdo(0), 1, 1, C::PdoFreshness::UNQUALIFIED);
    C::PdoQualification qualified = proof(map, 2); CHECK(owner.qualifyPdo(0, qualified, 2));
    const Saved<C::NodeSnapshot> independent(*owner.node(0));
    CHECK(owner.receive(payload, 3, 3)); CHECK(independent.unchanged(*owner.node(0)));
    CHECK(consumer.samples == 1 && consumer.valid);
    CHECK(consumer.copy.sample.values[0].native == -1);
    CHECK(consumer.copy.sample.values[1].native == -2147483647LL - 1);
    CHECK(consumer.copy.sample.values[0].object.role == C::PdoRole::ACTUAL);
    CHECK(consumer.copy.sample.values[0].raw == 0xffffffffu);
    CHECK(consumer.copy.time.receivedUs == 3 && consumer.copy.time.processedUs == 3);
    CHECK(!owner.node(0)->nmtTime.present && !owner.node(0)->heartbeat.present && !owner.node(0)->sdo.present);
    const Saved<C::PdoObservation> accepted(owner.pdo(0)->observation);
    payload.data[0] = 12; CHECK(consumer.copy.sample.frame.data[0] == 0xff);
    C::NetworkObservation trace; CHECK(owner.takeNetworkObservation(trace));
    CHECK(trace.pdo && trace.disposition == C::NetworkDisposition::UNQUALIFIED);
    CHECK(owner.takeNetworkObservation(trace)); CHECK(trace.disposition == C::NetworkDisposition::ACCEPTED);
    CHECK(!owner.takeNetworkObservation(trace)); CHECK(consumer.samples == 1);
    trace.frame.data[0] = 9; CHECK(accepted.unchanged(owner.pdo(0)->observation));
    M::Frame bad = payload; bad.dlc = bad.payloadLength = 7;
    CHECK(!owner.receive(bad, 4, 4)); CHECK(accepted.unchanged(owner.pdo(0)->observation));
    bad = payload; bad.rtr = true; bad.payloadLength = 0;
    CHECK(!owner.receive(bad, 5, 5)); CHECK(accepted.unchanged(owner.pdo(0)->observation));
    bad = payload; bad.extended = true; CHECK(!owner.receive(bad, 6, 6));
    CHECK(!owner.receive(payload, 2, 7)); CHECK(!owner.receive(payload, 9, 8));
    CHECK(!owner.receive(payload, 8)); // Unknown capture cannot renew the sample.
    CHECK(accepted.unchanged(owner.pdo(0)->observation)); CHECK(consumer.samples == 1);
    freshness(*owner.pdo(0), 8, 5, C::PdoFreshness::FRESH);
    freshness(*owner.pdo(0), 9, 5, C::PdoFreshness::EXPIRED);
    C::PdoFreshness unchanged = C::PdoFreshness::EXPIRED;
    CHECK(!C::pdoFreshness(*owner.pdo(0), 2, 5, unchanged)); CHECK(unchanged == C::PdoFreshness::EXPIRED);
    CHECK(!C::pdoFreshness(*owner.pdo(0), 9, 0, unchanged));

    C::PdoMapEvidence rawMap = mapping(1, 0x181, 9); Consumer raw;
    install(owner, rawMap, raw, 1, 9); CHECK(owner.qualifyPdo(1, proof(rawMap, 9), 9));
    M::Frame rawPayload = frame(0x181, 7); rawPayload.data[0] = 0x34; rawPayload.data[1] = 0x12;
    rawPayload.data[5] = 0x80; rawPayload.data[6] = 0xff;
    CHECK(owner.receive(rawPayload, 10, 10));
    CHECK(raw.copy.sample.values[0].numericKnown && raw.copy.sample.values[0].native == 0x1234);
    CHECK(!raw.copy.sample.values[2].numericKnown && raw.copy.sample.values[2].raw == 255);
    const Saved<C::PdoObservation> rawAccepted(owner.pdo(1)->observation);
    rawPayload.dlc = rawPayload.payloadLength = 6; CHECK(!owner.receive(rawPayload, 11, 11));
    CHECK(rawAccepted.unchanged(owner.pdo(1)->observation));
}

void sameWireNewMapRequiresRetirement() {
    C::SdoClient owner; setup(owner); Consumer changed, stable;
    C::PdoMapEvidence map = mapping(), other = mapping(1, 0x181);
    install(owner, map, changed); install(owner, other, stable, 1);
    CHECK(owner.qualifyPdo(0, proof(map), 0)); CHECK(owner.qualifyPdo(1, proof(other), 0));
    M::Frame bytes = frame(0x281); bytes.data[0] = 1; bytes.data[4] = 2;
    CHECK(owner.receive(bytes, 1, 1));
    C::PdoMap swapped = map.route.layout.mapping; swapped.words[0] = 0x606c0020; swapped.words[1] = 0x60640020;
    CHECK(C::validatePdoMap(swapped, L::pdoProfile(41), map.route.layout));
    map.route.generation = 2; map.startedUs = map.observedUs = 2;
    install(owner, map, changed, 0, 2); CHECK(changed.invalidations == 1 && !changed.valid);
    CHECK(owner.pdo(1)->confirmed && owner.pdo(1)->qualified);
    CHECK(!owner.receive(bytes, 3, 3)); // New ingress time, no synthetic old-generation label.
    CHECK(owner.pdo(0)->observation.sample.route.generation == 1);
    CHECK(!owner.receive(bytes, 1000, 1000)); // Quiet time and repeated good bytes prove nothing.
    C::PdoQualification evidence = proof(map, 1001);
    evidence.basis = C::PdoSampleBasis::UNQUALIFIED; CHECK(!owner.qualifyPdo(0, evidence, 1001));
    evidence.basis = C::PdoSampleBasis::DISTINCT_VERIFIED_ROUTE; CHECK(!owner.qualifyPdo(0, evidence, 1001));
    evidence = proof(map, 1001); ++evidence.route.binding.configurationGeneration;
    CHECK(!owner.qualifyPdo(0, evidence, 1001));
    evidence = proof(map, 1001); ++evidence.nodeGeneration; CHECK(!owner.qualifyPdo(0, evidence, 1001));
    evidence = proof(map, 1001); ++evidence.route.generation; CHECK(!owner.qualifyPdo(0, evidence, 1001));
    evidence = proof(map, 1001); evidence.evidenceId = 0; CHECK(!owner.qualifyPdo(0, evidence, 1001));
    evidence = proof(map, 2); CHECK(!owner.qualifyPdo(0, evidence, 1001));
    evidence = proof(map, 1002); CHECK(!owner.qualifyPdo(0, evidence, 1001));
    evidence = proof(map, 1001); CHECK(owner.qualifyPdo(0, evidence, 1001));
    freshness(*owner.pdo(0), 1001, 100, C::PdoFreshness::NEVER_OBSERVED);
    CHECK(!owner.receive(bytes, 1001, 1002)); CHECK(changed.samples == 1);
    CHECK(owner.receive(bytes, 1003, 1003)); CHECK(changed.samples == 2);
    CHECK(changed.copy.sample.values[0].object.index == 0x606c && changed.copy.sample.values[0].native == 1);
    M::Frame stableBytes = frame(0x181, 7); CHECK(owner.receive(stableBytes, 1004, 1004));
    CHECK(stable.samples == 1 && stable.invalidations == 0);
    CHECK(owner.invalidatePdo(0, false, 1005)); CHECK(owner.pdo(0)->confirmed && !owner.pdo(0)->qualified);
    evidence = proof(map, 1006); evidence.basis = C::PdoSampleBasis::DISTINCT_VERIFIED_ROUTE;
    CHECK(!owner.qualifyPdo(0, evidence, 1006));
    evidence.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
    CHECK(owner.qualifyPdo(0, evidence, 1006)); CHECK(!owner.receive(bytes, 1004, 1007));
    CHECK(owner.receive(bytes, 1008, 1008)); CHECK(stable.invalidations == 0);
}

void lifecycleAndFullHistory() {
    const C::NetworkInvalidation causes[] = {C::NetworkInvalidation::CONFIGURATION, C::NetworkInvalidation::BINDING,
        C::NetworkInvalidation::DIALECT, C::NetworkInvalidation::RECOVERY};
    for (unsigned n = 0; n < sizeof causes / sizeof causes[0]; ++n) {
        C::SdoClient owner; setup(owner); Consumer consumer; C::PdoMapEvidence map = mapping();
        install(owner, map, consumer); CHECK(owner.qualifyPdo(0, proof(map), 0));
        M::Frame bytes = frame(0x281);
        for (unsigned i = 1; i <= C::SdoClient::NETWORK_HISTORY + 2; ++i) CHECK(owner.receive(bytes, i, i));
        CHECK(owner.droppedNetworkObservations() == 2);
        const Saved<C::PdoObservation> historical(owner.pdo(0)->observation);
        CHECK(owner.invalidateNetwork(0, causes[n], 100));
        CHECK(!owner.pdo(0)->confirmed && !owner.pdo(0)->qualified && !consumer.valid);
        CHECK(consumer.invalidations == 1); CHECK(historical.unchanged(owner.pdo(0)->observation));
        CHECK(!owner.receive(bytes, 101, 101)); freshness(*owner.pdo(0), 101, 1, C::PdoFreshness::NO_MAP);
    }
    C::SdoClient owner; setup(owner); Consumer consumer; C::PdoMapEvidence map = mapping();
    install(owner, map, consumer); CHECK(owner.qualifyPdo(0, proof(map), 0));
    CHECK(owner.receive(frame(0x281), 1, 1));
    for (unsigned i = 0; i < C::SdoClient::NETWORK_HISTORY; ++i) owner.receive(frame(0x123), 2, 2);
    CHECK(owner.receive(heartbeat(0), 3, 3));
    CHECK(!owner.pdo(0)->confirmed && consumer.invalidations == 1);
    CHECK(owner.node(0)->route.generation == 12);
    CHECK(!owner.qualifyPdo(0, proof(map, 4), 4));
    map.nodeGeneration = 12; map.route.generation = 2; map.startedUs = map.observedUs = 4;
    install(owner, map, consumer, 0, 4); CHECK(owner.qualifyPdo(0, proof(map, 5), 5));
    CHECK(!owner.receive(frame(0x281), 6, 6)); // Known Initializing blocks publication.
    CHECK(owner.receive(heartbeat(5), 7, 7)); CHECK(owner.receive(frame(0x281), 8, 8));
    CHECK(owner.receive(heartbeat(127), 9, 9)); CHECK(owner.pdo(0)->confirmed && !owner.pdo(0)->qualified);
    CHECK(!owner.receive(frame(0x281), 10, 10));
    CHECK(owner.receive(heartbeat(5), 11, 11)); CHECK(!owner.receive(frame(0x281), 12, 12));
    CHECK(owner.qualifyPdo(0, proof(map, 13), 13)); CHECK(owner.receive(frame(0x281), 14, 14));
}

void controllerCannotBeBypassedByNewEvidence() {
    const C::ControllerState states[] = {C::ControllerState::BUS_OFF, C::ControllerState::RECOVERING};
    for (unsigned i = 0; i < 2; ++i) {
        C::SdoClient owner; setup(owner); Consumer consumer; C::PdoMapEvidence map = mapping();
        install(owner, map, consumer); CHECK(owner.qualifyPdo(0, proof(map), 0));
        CHECK(owner.receive(frame(0x281), 1, 1)); CHECK(owner.reportController(states[i], 2));
        CHECK(!owner.pdo(0)->confirmed && !owner.pdo(0)->qualified);
        map.nodeGeneration = owner.node(0)->route.generation; map.route.generation = 2; map.startedUs = map.observedUs = 3;
        install(owner, map, consumer, 0, 3); CHECK(!owner.qualifyPdo(0, proof(map, 4), 4));
        CHECK(!owner.receive(frame(0x281), 5, 5)); CHECK(consumer.samples == 1);
        CHECK(owner.reportController(C::ControllerState::AVAILABLE, 6));
        CHECK(owner.qualifyPdo(0, proof(map, 6), 6));
        CHECK(owner.receive(frame(0x281), 7, 7)); CHECK(consumer.samples == 2);
    }
}

void qualificationReplacementAndDistinctRoute() {
    C::SdoClient owner; setup(owner); Consumer oldConsumer, newConsumer;
    C::PdoMapEvidence map = mapping(); install(owner, map, oldConsumer);
    CHECK(owner.qualifyPdo(0, proof(map), 0)); CHECK(owner.receive(frame(0x281), 1, 1));
    const Saved<C::PdoRecord> before(*owner.pdo(0));
    C::PdoQualification replacement = proof(map, 2); replacement.evidenceId = 201;
    CHECK(!owner.qualifyPdo(0, replacement, 2));
    CHECK(before.unchanged(*owner.pdo(0)) && oldConsumer.invalidations == 0 && oldConsumer.valid);
    CHECK(owner.invalidatePdo(0, false, 3)); CHECK(oldConsumer.invalidations == 1 && !oldConsumer.valid);
    replacement.establishedUs = 4; CHECK(owner.qualifyPdo(0, replacement, 4));
    freshness(*owner.pdo(0), 4, 100, C::PdoFreshness::NEVER_OBSERVED);
    CHECK(!owner.receive(frame(0x281), 2, 5)); CHECK(!oldConsumer.valid && oldConsumer.samples == 1);
    CHECK(owner.receive(frame(0x281), 6, 6)); CHECK(oldConsumer.valid && oldConsumer.samples == 2);
    C::PdoMapEvidence distinct = mapping(2, 0x381, 7);
    CHECK(!owner.configurePdo(1, 0, distinct, L::pdoProfile(41), newConsumer.observer(), 7));
    CHECK(owner.invalidatePdo(0, true, 8));
    CHECK(!owner.configurePdo(1, 0, distinct, L::pdoProfile(41), newConsumer.observer(), 9));
    distinct.startedUs = distinct.observedUs = 10;
    install(owner, distinct, newConsumer, 1, 10);
    C::PdoQualification distinctProof = proof(distinct, 10);
    distinctProof.basis = C::PdoSampleBasis::DISTINCT_VERIFIED_ROUTE;
    CHECK(owner.qualifyPdo(1, distinctProof, 10)); CHECK(owner.receive(frame(0x381), 11, 11));
    CHECK(!owner.receive(frame(0x281), 12, 12)); CHECK(newConsumer.samples == 1 && oldConsumer.samples == 2);
    // Retired identifiers stay reserved; moving the same ID to a fresh slot cannot erase history.
    map.route.generation = 2; map.startedUs = map.observedUs = 13;
    CHECK(!owner.configurePdo(2, 0, map, L::pdoProfile(41), oldConsumer.observer(), 13));
}

void collisionsCapacityAndContrasts() {
    C::SdoClient owner; setup(owner); Consumer consumer;
    CHECK(owner.configure(0, channel(), false, true));
    const uint32_t collisions[] = {0x701, 0x81, 0x601, 0x581};
    for (unsigned i = 0; i < 4; ++i) {
        C::PdoMapEvidence bad = mapping(2, collisions[i]);
        CHECK(!owner.configurePdo(0, 0, bad, L::pdoProfile(41), consumer.observer(), 0));
    }
    C::PdoMapEvidence map = mapping(); install(owner, map, consumer);
    CHECK(!owner.configurePdo(1, 0, map, L::pdoProfile(41), consumer.observer(), 0));
    C::NetworkRoute networkCollision = network(2); networkCollision.emcyId = 0x281;
    CHECK(!owner.configureNetwork(1, networkCollision, C::NetworkObserver(), 0));
    networkCollision = network(2); networkCollision.heartbeatId = 0x281;
    CHECK(!owner.configureNetwork(1, networkCollision, C::NetworkObserver(), 0));
    C::SdoChannel sdoCollision = channel(2); sdoCollision.clientToServerId = 0x281;
    CHECK(!owner.configure(1, sdoCollision, false, true));
    sdoCollision = channel(2); sdoCollision.serverToClientId = 0x281;
    CHECK(!owner.configure(1, sdoCollision, false, true));
    C::PdoObserver invalidObserver = consumer.observer(); invalidObserver.invalidate = nullptr;
    CHECK(!owner.configurePdo(1, 0, mapping(1, 0x181), L::pdoProfile(41), invalidObserver, 0));

    C::SdoClient capacity; setup(capacity);
    C::PdoProfile contrast = L::pdoProfile(41); contrast.sets = 8;
    for (uint8_t i = 0; i < C::SdoClient::PDO_ROUTES; ++i) {
        const uint64_t now = uint64_t(i) * 3;
        C::PdoMapEvidence item = mapping(2, 0x300u + i, now);
        C::PdoMap candidate = item.route.layout.mapping; candidate.number = static_cast<uint16_t>(i + 1);
        CHECK(C::validatePdoMap(candidate, contrast, item.route.layout));
        CHECK(capacity.configurePdo(i, 0, item, contrast, C::PdoObserver(), now));
        C::PdoQualification distinct = proof(item, now); distinct.basis = C::PdoSampleBasis::DISTINCT_VERIFIED_ROUTE;
        CHECK(capacity.qualifyPdo(i, distinct, now)); CHECK(capacity.receive(frame(0x300u + i), now + 1, now + 1));
    }
    CHECK(C::SdoClient::PDO_ROUTES == 8);
    CHECK(!capacity.configurePdo(C::SdoClient::PDO_ROUTES, 0, mapping(), contrast, C::PdoObserver(), 100));
    CHECK(!capacity.pdo(C::SdoClient::PDO_ROUTES));
    C::PdoMap fifth = mapping().route.layout.mapping; fifth.number = 5;
    C::PdoLayout output; CHECK(!C::validatePdoMap(fifth, L::pdoProfile(41), output));
    CHECK(C::validatePdoMap(fifth, contrast, output));
    C::PdoMap rpdo; CHECK(L::pdoTemplate(C::PdoDirection::RPDO, 1, rpdo));
    CHECK(C::validatePdoMap(rpdo, L::pdoProfile(41), output)); CHECK(output.payloadLength == 7);
    CHECK(output.fields[0].index == 0x6040 && output.fields[2].index == 0x6060);
}

void readbackFailureCannotCertify() {
    const C::PdoMapEvidence map = mapping(); C::SdoCompletion results[5];
    const uint32_t values[] = {0x281, 255, 2, 0x60640020, 0x606c0020};
    for (unsigned i = 0; i < 5; ++i) {
        C::SdoCompletion& result = results[i]; result.transfer.channel = channel();
        result.transfer.index = i < 2 ? 0x1801 : 0x1a01;
        result.transfer.subindex = static_cast<uint8_t>(i < 2 ? i + 1 : i - 2);
        result.transfer.direction = C::SdoDirection::UPLOAD;
        result.transfer.size = static_cast<uint8_t>(i == 1 || i == 2 ? 1 : 4);
        result.size = result.transfer.size; result.outcome = C::SdoOutcome::SUCCESS;
        result.protocolAcknowledged = true; result.admittedUs = i; result.completedUs = i + 1; result.nodeGeneration = 11;
        for (unsigned b = 0; b < result.size; ++b) result.data[b] = static_cast<uint8_t>(values[i] >> (8 * b));
    }
    C::PdoMapEvidence actual; CHECK(C::pdoMapFromReadback(results, 5, map.route, 11, 301, actual));
    CHECK(actual.source == C::PdoMapSource::SDO_READBACK && actual.observedUs == 5);
    C::SdoClient owner; setup(owner); Consumer consumer; install(owner, actual, consumer, 0, 5);
    CHECK(!owner.pdo(0)->qualified); CHECK(!owner.receive(frame(0x281), 6, 6));
    const Saved<C::PdoMapEvidence> before(actual);
    results[4].outcome = C::SdoOutcome::SERVER_ABORT; results[4].abortValid = true; results[4].abortCode = 0x06020000;
    CHECK(!C::pdoMapFromReadback(results, 5, map.route, 11, 302, actual)); CHECK(before.unchanged(actual));
    CHECK(!C::pdoMapFromReadback(results, 4, map.route, 11, 302, actual)); CHECK(before.unchanged(actual));
    CHECK(owner.pdo(0)->confirmed && !owner.pdo(0)->qualified);
    results[4].outcome = C::SdoOutcome::SUCCESS; results[4].abortValid = false;
    results[4].nodeGeneration = 12;
    CHECK(!C::pdoMapFromReadback(results, 5, map.route, 11, 302, actual)); CHECK(before.unchanged(actual));
    results[4].nodeGeneration = 0;
    CHECK(!C::pdoMapFromReadback(results, 5, map.route, 11, 302, actual)); CHECK(before.unchanged(actual));
    CHECK(owner.invalidateNetwork(0, C::NetworkInvalidation::CONFIGURATION, 7));
    C::PdoRoute replacement = map.route; replacement.generation = 2;
    for (unsigned i = 0; i < 5; ++i) {
        results[i].nodeGeneration = 12; results[i].completedUs = 8 + i;
    }
    // A late last completion cannot hide acquisition that began before invalidation.
    CHECK(C::pdoMapFromReadback(results, 5, replacement, 12, 303, actual));
    CHECK(actual.startedUs == 0 && actual.observedUs == 12);
    CHECK(!owner.configurePdo(0, 0, actual, L::pdoProfile(41), consumer.observer(), 12));
    CHECK(!owner.pdo(0)->confirmed);
    for (unsigned i = 0; i < 5; ++i) {
        results[i].admittedUs = 8 + i; results[i].completedUs = 9 + i;
    }
    CHECK(C::pdoMapFromReadback(results, 5, replacement, 12, 304, actual));
    install(owner, actual, consumer, 0, 13); CHECK(owner.pdo(0)->confirmed && !owner.pdo(0)->qualified);
}

void readbackThroughNormalServices() {
    for (unsigned failLast = 0; failLast < 2; ++failLast) {
        C::SdoClient owner; CHECK(owner.configure(0, channel(), true, true)); setup(owner);
        C::SdoCompletion results[5];
        const uint32_t words[] = {0x281, 255, 2, 0x60640020, 0x606c0020};
        for (unsigned i = 0; i < 5; ++i) {
            const uint64_t now = 1 + i * 3;
            C::SdoTransfer work; work.channel = channel(); work.operation = 77; work.request = work.step = i + 1;
            work.index = i < 2 ? 0x1801 : 0x1a01; work.subindex = static_cast<uint8_t>(i < 2 ? i + 1 : i - 2);
            work.size = static_cast<uint8_t>(i == 1 || i == 2 ? 1 : 4);
            work.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING; // Synthetic reviewed read premise.
            work.deadlineUs = 200; work.queueTimeoutUs = work.responseTimeoutUs = 20;
            C::SdoRequestId id; CHECK(owner.admit(0, work, now, id));
            M::Frame request; C::SdoTxId tx; CHECK(owner.nextTx(0, now + 1, request, tx));
            CHECK(request.data[0] == 0x40 && work.index != 0x6040 && work.index != 0x6060);
            M::Frame response = frame(0x581);
            response.data[0] = work.size == 1 ? 0x4f : 0x43;
            response.data[1] = static_cast<uint8_t>(work.index); response.data[2] = static_cast<uint8_t>(work.index >> 8);
            response.data[3] = work.subindex;
            for (unsigned b = 0; b < 4; ++b) response.data[4 + b] = static_cast<uint8_t>(words[i] >> (8 * b));
            if (failLast && i == 4) { response.data[0] = 0x80; response.data[4] = 0; response.data[5] = 0; response.data[6] = 2; response.data[7] = 6; }
            CHECK(owner.receive(response, now + 2, now + 2)); CHECK(owner.result(id));
            results[i] = *owner.result(id);
            CHECK(results[i].admittedUs == now && results[i].completedUs == now + 2 && results[i].nodeGeneration == 11);
            CHECK(owner.release(id));
        }
        const C::PdoMapEvidence expected = mapping(); C::PdoMapEvidence actual;
        const Saved<C::PdoMapEvidence> before(actual);
        const M::Status checked = C::pdoMapFromReadback(results, 5, expected.route, 11, 909, actual);
        CHECK(bool(checked) == !failLast);
        CHECK(owner.port(0)->accepted == 5 && owner.nmtPort()->accepted == 0);
        if (failLast) { CHECK(before.unchanged(actual)); CHECK(!owner.pdo(0) && owner.unresolved(0)); }
        else {
            Consumer consumer; install(owner, actual, consumer, 0, 20);
            CHECK(actual.startedUs == 1 && actual.observedUs == 15 && !owner.pdo(0)->qualified);
            CHECK(owner.qualifyPdo(0, proof(actual, 21), 21)); CHECK(owner.receive(frame(0x281), 22, 22));
            CHECK(consumer.samples == 1 && !owner.node(0)->nmtTime.present);
        }
    }
}

void interleavedServiceTerminals() {
    C::SdoClient owner; CHECK(owner.configure(0, channel(), true, true)); setup(owner);
    Consumer consumer; const C::PdoMapEvidence map = mapping(); install(owner, map, consumer);
    CHECK(owner.qualifyPdo(0, proof(map), 0));
    C::SdoTransfer work; work.channel = channel(); work.operation = 1; work.request = 2; work.step = 3;
    work.index = 0x6040; work.size = 2; work.direction = C::SdoDirection::DOWNLOAD;
    work.deadlineUs = 100; work.queueTimeoutUs = 20; work.responseTimeoutUs = 50;
    C::SdoRequestId request; CHECK(owner.admit(0, work, 1, request));
    M::Frame transmitted; C::SdoTxId tx; CHECK(owner.nextTx(0, 2, transmitted, tx));
    CHECK(owner.receive(frame(0x281), 3, 3)); CHECK(!owner.result(request));
    CHECK(owner.receive(heartbeat(5), 4, 4));
    M::Frame emergency = frame(0x81); emergency.data[0] = 0x10; emergency.data[1] = 0x81;
    CHECK(owner.receive(emergency, 5, 5)); CHECK(owner.node(0)->emcyCount == 1);
    CHECK(!owner.result(request)); CHECK(owner.cancel(request, 6));
    CHECK(owner.result(request) && owner.result(request)->effectUncertain);
    CHECK(owner.result(request)->admittedUs == 1 && owner.result(request)->nodeGeneration == 11);
    const Saved<C::SdoCompletion> retained(*owner.result(request));
    CHECK(owner.receive(frame(0x281), 7, 7)); CHECK(retained.unchanged(*owner.result(request)));
    const Saved<C::ObservationTime> heartbeatTime(owner.node(0)->heartbeat);
    const Saved<C::ObservationTime> sdoTime(owner.node(0)->sdo);
    CHECK(owner.receive(frame(0x281), 8, 8));
    CHECK(heartbeatTime.unchanged(owner.node(0)->heartbeat) && sdoTime.unchanged(owner.node(0)->sdo));
    C::NmtTransfer intent; intent.command = C::NmtCommand::STOP; intent.target = owner.node(0)->route;
    intent.request = 8; intent.deadlineUs = 100; intent.queueTimeoutUs = 20;
    C::NmtRequestId nmt; CHECK(owner.admitNmt(intent, 9, nmt));
    CHECK(owner.receive(frame(0x281), 10, 10)); // Queuing intent alone is not a transition.
    CHECK(owner.nextNmtTx(nmt, 11, transmitted));
    CHECK(owner.pdo(0)->confirmed && !owner.pdo(0)->qualified);
    CHECK(!owner.receive(frame(0x281), 12, 12)); CHECK(owner.confirmNmtTx(nmt, 13));
    CHECK(owner.nmtResult(nmt)); const Saved<C::NmtCompletion> nmtTerminal(*owner.nmtResult(nmt));
    CHECK(owner.receive(heartbeat(4), 14, 14)); CHECK(!owner.receive(frame(0x281), 15, 15));
    CHECK(retained.unchanged(*owner.result(request))); CHECK(nmtTerminal.unchanged(*owner.nmtResult(nmt)));
    drain(owner); CHECK(consumer.samples == 4);
}
} // namespace
int main() {
    actualMapAndAtomicSamples(); sameWireNewMapRequiresRetirement(); lifecycleAndFullHistory();
    controllerCannotBeBypassedByNewEvidence(); qualificationReplacementAndDistinctRoute();
    collisionsCapacityAndContrasts(); readbackFailureCannotCertify();
    readbackThroughNormalServices(); interleavedServiceTerminals();
    std::printf("PDO owner observations: %u checks\n", checks); return 0;
}
