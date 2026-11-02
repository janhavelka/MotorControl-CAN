// SPDX-License-Identifier: MIT
// Synthetic network/SDO timing vectors. No device capability or ISR latency bound.
#include "SdoClient.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
M::Binding binding() {
    M::Binding value;
    value.logicalAxisId = 101; value.endpoint.address = 1; value.endpoint.profile = 41;
    value.bindingGeneration = 3; value.configurationGeneration = 7;
    return value;
}
C::NetworkRoute route() {
    C::NetworkRoute value;
    value.binding = binding(); value.generation = 11;
    value.heartbeatId = 0x701; value.emcyId = 0x81;
    return value;
}
C::SdoChannel channel() {
    C::SdoChannel value;
    value.binding = binding(); value.clientToServerId = 0x601; value.serverToClientId = 0x581;
    return value;
}
M::Frame heartbeat(uint8_t state = 5) {
    M::Frame value;
    value.id = 0x701; value.dlc = value.payloadLength = 1; value.data[0] = state;
    return value;
}
void setup(C::SdoClient& owner, uint64_t now = 0) {
    CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), now));
    CHECK(owner.setProducerInterval(0, route(), 100, now));
    CHECK(owner.setServiceAge(0, 20, now));
}
C::ServiceEligibility eligible(const C::SdoClient& owner, uint64_t now) {
    C::ServiceEligibility value;
    CHECK(C::serviceEligibility(*owner.node(0), now, value));
    return value;
}
void freshness(const C::SdoClient& owner, uint64_t now, uint64_t age, C::HeartbeatFreshness expected) {
    C::HeartbeatFreshness value = C::HeartbeatFreshness::DISABLED;
    CHECK(C::heartbeatFreshness(*owner.node(0), now, age, value));
    CHECK(value == expected);
}
void boundsAreExplicitAndAtomic() {
    C::ObservationTime time;
    uint64_t first = 91, last = 92;
    CHECK(!C::observationTimeBounds(time, first, last) && first == 91 && last == 92);
    time.present = true; time.receivedTimeKnown = true;
    CHECK(C::observationTimeBounds(time, first, last) && first == 0 && last == 0);
    time.receivedTimeKnown = false; time.receiveBoundsKnown = true;
    time.receivedEarliestUs = 10; time.receivedLatestUs = 20; time.processedUs = 20;
    CHECK(C::observationTimeBounds(time, first, last) && first == 10 && last == 20);
    CHECK(!C::observationTimeBounds(time, first, first) && first == 10);
    CHECK(!C::observationTimeBounds(time, time.receivedEarliestUs, last));
    time.receivedTimeKnown = true;
    CHECK(!C::observationTimeBounds(time, first, last));
    time.receivedTimeKnown = false; time.receivedEarliestUs = 21;
    CHECK(!C::observationTimeBounds(time, first, last));
    time.receivedEarliestUs = 10; time.processedUs = 19;
    CHECK(!C::observationTimeBounds(time, first, last));

    C::SdoClient owner; setup(owner);
    const Saved<C::SdoClient> before(owner);
    CHECK(!owner.receive(heartbeat(), 20, 10, 30) && before.unchanged(owner));
    CHECK(!owner.receive(heartbeat(), 10, 31, 30) && before.unchanged(owner));
    CHECK(owner.receive(heartbeat(), 0, 20, 20));
    const auto& observed = owner.node(0)->heartbeat;
    CHECK(observed.receiveBoundsKnown && !observed.receivedTimeKnown && observed.receivedUs == 0);
    CHECK(observed.receivedEarliestUs == 0 && observed.receivedLatestUs == 20);
    CHECK(eligible(owner, 20).exactNmtKnown && eligible(owner, 20).pdoTransmit);
    CHECK(!eligible(owner, 21).exactNmtKnown);
    freshness(owner, 20, 20, C::HeartbeatFreshness::FRESH);
    freshness(owner, 21, 20, C::HeartbeatFreshness::UNQUALIFIED);
    freshness(owner, 40, 20, C::HeartbeatFreshness::UNQUALIFIED);
    freshness(owner, 41, 20, C::HeartbeatFreshness::EXPIRED);
    const Saved<C::SdoClient> after(owner);
    CHECK(!owner.receive(heartbeat(), 1, 2, 19) && after.unchanged(owner));
}
void lifecycleNeedsEntireIntervalAfterBoundary() {
    C::SdoClient owner; setup(owner);
    CHECK(owner.receive(heartbeat(), 10, 20, 20));
    CHECK(owner.invalidateNetwork(0, C::NetworkInvalidation::CONFIGURATION, 30));
    CHECK(!owner.receive(heartbeat(), 20, 40, 40));
    CHECK(!owner.node(0)->heartbeat.present && !eligible(owner, 40).exactNmtKnown);
    CHECK(!owner.receive(heartbeat(), 30, 45, 45));
    CHECK(owner.receive(heartbeat(127), 31, 46, 46));
    CHECK(eligible(owner, 46).exactNmtKnown && eligible(owner, 46).ordinarySdo);
    CHECK(!owner.receive(heartbeat(4), 1, 29, 47));
    CHECK(owner.node(0)->nmt == C::NmtState::PRE_OPERATIONAL);
    CHECK(owner.node(0)->heartbeat.generation == owner.node(0)->route.generation);
    freshness(owner, 47, 30, C::HeartbeatFreshness::PRODUCER_UNKNOWN);
    CHECK(owner.setProducerInterval(0, owner.node(0)->route, 0, 47));
    freshness(owner, 47, 30, C::HeartbeatFreshness::DISABLED);
}
void producerWriteWithdrawsOnlyItsTimingPremise() {
    C::SdoClient owner; setup(owner);
    CHECK(owner.receive(heartbeat(), 10, 12, 12));
    const C::NodeSnapshot before = *owner.node(0);
    CHECK(owner.invalidateProducerInterval(0, route(), 12));
    CHECK(!owner.node(0)->producerKnown && owner.node(0)->producerTimeMs == 100);
    CHECK(owner.node(0)->route.generation == before.route.generation);
    CHECK(owner.node(0)->heartbeat.receivedEarliestUs == before.heartbeat.receivedEarliestUs);
    CHECK(owner.node(0)->heartbeat.receivedLatestUs == before.heartbeat.receivedLatestUs);
    CHECK(eligible(owner, 12).exactNmtKnown);
    freshness(owner, 12, 20, C::HeartbeatFreshness::PRODUCER_UNKNOWN);
    const Saved<C::SdoClient> unchanged(owner);
    auto wrong = route(); ++wrong.generation;
    CHECK(!owner.invalidateProducerInterval(0, wrong, 12) && unchanged.unchanged(owner));
    CHECK(!owner.invalidateProducerInterval(0, route(), 11) && unchanged.unchanged(owner));
    CHECK(!owner.invalidateProducerInterval(2, route(), 12) && unchanged.unchanged(owner));
    CHECK(owner.setProducerInterval(0, route(), 200, 13));
    freshness(owner, 13, 20, C::HeartbeatFreshness::FRESH);
}

void contradictoryOverlapWithdrawsState() {
    C::SdoClient owner;
    CHECK(owner.configure(0, channel(), true, true));
    setup(owner);
    CHECK(owner.receive(heartbeat(), 20, 20)); // Exact timestamp remains supported.
    CHECK(eligible(owner, 20).pdoTransmit);
    CHECK(owner.receive(heartbeat(4), 15, 25, 25));
    CHECK(owner.node(0)->nmtOrderingUncertain);
    CHECK(!eligible(owner, 25).exactNmtKnown && !eligible(owner, 25).ordinarySdo);
    CHECK(!eligible(owner, 25).pdoTransmit);
    CHECK(owner.node(0)->nmtTime.receivedEarliestUs == 15 &&
          owner.node(0)->nmtTime.receivedLatestUs == 25);
    CHECK(owner.receive(heartbeat(), 24, 30, 30));
    CHECK(owner.node(0)->nmtOrderingUncertain && owner.node(0)->nmtTime.receivedLatestUs == 30);
    CHECK(!eligible(owner, 30).exactNmtKnown);
    CHECK(owner.receive(heartbeat(127), 31, 32, 32));
    CHECK(!owner.node(0)->nmtOrderingUncertain && eligible(owner, 32).exactNmtKnown);
    CHECK(eligible(owner, 32).ordinarySdo && !eligible(owner, 32).pdoTransmit);
    CHECK(!owner.receive(heartbeat(4), 1, 10, 33));
    CHECK(owner.node(0)->nmt == C::NmtState::PRE_OPERATIONAL);
}
void nmtTransitionNeedsLowerBoundAndOriginalDeadline() {
    for (unsigned late = 0; late < 2; ++late) {
        C::SdoClient owner; setup(owner);
        C::NmtTransfer work;
        work.target = route(); work.command = C::NmtCommand::START;
        work.request = 5; work.deadlineUs = 40; work.queueTimeoutUs = 20;
        C::NmtRequestId id;
        CHECK(owner.admitNmt(work, 1, id));
        M::Frame output;
        CHECK(owner.nextNmtTx(id, 10, output));
        CHECK(owner.confirmNmtTx(id, 11));
        CHECK(owner.receive(heartbeat(), 5, 15, 15));
        CHECK(!owner.node(0)->transitionObserved && !eligible(owner, 15).exactNmtKnown);
        CHECK(owner.receive(heartbeat(), 16, 20, late ? 40 : 20));
        CHECK(owner.node(0)->transitionObserved == !late);
        CHECK(owner.node(0)->transitionExpired == bool(late));
    }
}
C::SdoRequestId startRead(C::SdoClient& owner) {
    CHECK(owner.configure(0, channel(), true, true));
    C::SdoTransfer transfer;
    transfer.channel = channel(); transfer.operation = 17; transfer.request = 19; transfer.step = 23;
    transfer.index = 0x6041; transfer.size = 2;
    transfer.direction = C::SdoDirection::UPLOAD;
    transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
    transfer.deadlineUs = 30; transfer.queueTimeoutUs = 20; transfer.responseTimeoutUs = 20;
    C::SdoRequestId id;
    CHECK(owner.admit(0, transfer, 1, id));
    M::Frame request; C::SdoTxId tx;
    CHECK(owner.nextTx(0, 10, request, tx));
    return id;
}
M::Frame reply() {
    M::Frame value;
    value.id = 0x581; value.dlc = value.payloadLength = 8;
    value.data[0] = 0x4b; value.data[1] = 0x41; value.data[2] = 0x60;
    value.data[4] = 0x35; value.data[5] = 0xa2;
    return value;
}
void sdoDeadlinesAndCorrelationStayConservative() {
    {
        C::SdoClient owner; const auto id = startRead(owner);
        CHECK(!owner.receive(reply(), 1, 9, 11));
        CHECK(owner.result(id)->outcome == C::SdoOutcome::CORRELATION_LOST);
        CHECK(owner.unresolved(0));
    }
    {
        C::SdoClient owner; const auto id = startRead(owner);
        CHECK(owner.receive(reply(), 11, 12, 12));
        CHECK(owner.result(id)->outcome == C::SdoOutcome::SUCCESS);
        CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 12));
        CHECK(!owner.node(0)->sdo.receivedTimeKnown && !owner.node(0)->sdo.receiveBoundsKnown);
    }
    {
        C::SdoClient owner; const auto id = startRead(owner);
        CHECK(!owner.receive(reply(), 11, 12, 30));
        CHECK(owner.result(id)->outcome == C::SdoOutcome::TIMEOUT);
    }
}
} // namespace
int main() {
    boundsAreExplicitAndAtomic();
    lifecycleNeedsEntireIntervalAfterBoundary();
    producerWriteWithdrawsOnlyItsTimingPremise();
    contradictoryOverlapWithdrawsState();
    nmtTransitionNeedsLowerBoundAndOriginalDeadline();
    sdoDeadlinesAndCorrelationStayConservative();
    std::printf("Conservative network receive time: %u checks\n", checks);
}
