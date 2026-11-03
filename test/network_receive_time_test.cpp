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
C::SdoChannel channel(uint16_t sub = 0) {
    C::SdoChannel value;
    value.binding = binding(); value.binding.endpoint.channel = sub;
    value.clientToServerId = 0x601u + 0x20u * sub; value.serverToClientId = 0x581u + 0x20u * sub;
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
M::Frame reply(uint16_t sub = 0) {
    M::Frame value;
    value.id = 0x581u + 0x20u * sub; value.dlc = value.payloadLength = 8;
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
    {   // Contract change (bounds qualification): the interval, never exact capture, now
        // reaches the node record when passive routing is attached later.
        C::SdoClient owner; const auto id = startRead(owner);
        CHECK(owner.receive(reply(), 11, 12, 12));
        CHECK(owner.result(id)->outcome == C::SdoOutcome::SUCCESS);
        CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 12));
        const C::NodeSnapshot& node = *owner.node(0);
        CHECK(!node.sdo.receivedTimeKnown && node.sdo.receiveBoundsKnown && node.sdo.receivedUs == 0);
        CHECK(node.sdo.receivedEarliestUs == 11 && node.sdo.receivedLatestUs == 12);
        CHECK(node.sdo.generation == route().generation && node.serviceFromSdo);
        CHECK(node.serviceQualification.receiveBoundsKnown && node.serviceQualification.receivedEarliestUs == 11);
    }
    {
        C::SdoClient owner; const auto id = startRead(owner);
        CHECK(!owner.receive(reply(), 11, 12, 30));
        CHECK(owner.result(id)->outcome == C::SdoOutcome::TIMEOUT);
    }
}

// Contract change (bounds qualification, decided from the 2026-11-03 S2 soaks): a checked reply
// received with bounds records service evidence exactly when its whole interval is after every
// boundary the exact form checks. Its lower bound orders and ages it. The S2 bench shape follows:
// an unconfirmed channel, passive routing with no heartbeat, and interval-only ingress.
void unconfirmed(C::SdoClient& owner, uint64_t age = 20) {
    CHECK(owner.configure(0, channel(), false, true));
    CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 0));
    CHECK(owner.setServiceAge(0, age, 0));
}
C::SdoTransfer accepted(uint16_t sub = 0) {
    C::SdoTransfer transfer;
    transfer.channel = channel(sub); transfer.operation = 17; transfer.request = 19; transfer.step = 23;
    transfer.index = 0x6041; transfer.size = 2;
    transfer.direction = C::SdoDirection::UPLOAD;
    transfer.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    transfer.deadlineUs = 1000; transfer.queueTimeoutUs = 200; transfer.responseTimeoutUs = 200;
    return transfer;
}
C::SdoTransfer write() {
    C::SdoTransfer transfer = accepted();
    transfer.index = 0x6040; transfer.direction = C::SdoDirection::DOWNLOAD;
    transfer.readEffect = C::SdoReadEffect::UNQUALIFIED;
    transfer.data[0] = 0x0b;
    return transfer;
}
C::SdoRequestId sent(C::SdoClient& owner, uint64_t admitted, uint64_t handoff, uint16_t sub = 0) {
    C::SdoRequestId id;
    CHECK(owner.admit(uint8_t(sub), accepted(sub), admitted, id));
    M::Frame request; C::SdoTxId tx;
    CHECK(owner.nextTx(uint8_t(sub), handoff, request, tx) && request.id == 0x601u + 0x20u * sub);
    CHECK(owner.confirmTx(tx, handoff)); // The controller's TX report retires the frame.
    return id;
}
void succeeded(C::SdoClient& owner, C::SdoRequestId id) {
    CHECK(owner.result(id) && owner.result(id)->outcome == C::SdoOutcome::SUCCESS);
    CHECK(owner.release(id));
}
void boundedReplyQualifiesFromItsEarliestBound() {
    C::SdoClient owner; unconfirmed(owner);
    CHECK(!eligible(owner, 0).ordinarySdo && eligible(owner, 0).probeUpload);
    CHECK(!owner.checkAdmission(0, write(), 0));
    const auto id = sent(owner, 1, 2);
    CHECK(owner.receive(reply(), 3, 5, 6));
    succeeded(owner, id);
    const C::NodeSnapshot& node = *owner.node(0);
    const C::ObservationTime& proof = node.serviceQualification;
    CHECK(proof.present && proof.receiveBoundsKnown && !proof.receivedTimeKnown && proof.receivedUs == 0);
    CHECK(proof.receivedEarliestUs == 3 && proof.receivedLatestUs == 5 && proof.processedUs == 6);
    CHECK(proof.generation == route().generation && node.serviceFromSdo);
    CHECK(node.sdo.receivedEarliestUs == 3 && node.sdo.receivedLatestUs == 5 && !node.nmtTime.present);
    const C::ServiceEligibility now = eligible(owner, 6);
    CHECK(now.ordinarySdo && !now.exactNmtKnown && !now.pdoTransmit && now.probeUpload);
    CHECK(owner.checkAdmission(0, write(), 6));
    // Age runs from the earliest bound: 23-3 is within 20, 24-3 is not, although 24-5 would be.
    CHECK(eligible(owner, 23).ordinarySdo && !eligible(owner, 24).ordinarySdo);
    CHECK(owner.checkAdmission(0, write(), 23) && !owner.checkAdmission(0, write(), 24));
    CHECK(owner.checkAdmission(0, accepted(), 24)); // The accepted read that can renew it stays.
}
void intervalMustStartAfterItsHandoff() {
    for (unsigned form = 0; form < 3; ++form) {
        C::SdoClient owner; unconfirmed(owner);
        const auto id = sent(owner, 1, 2);
        const Saved<C::NodeSnapshot> before(*owner.node(0));
        if (form == 0) CHECK(owner.receive(reply(), 2, 5, 6));      // Starts at the handoff.
        else if (form == 1) CHECK(owner.receive(reply(), 1, 5, 6)); // Straddles it.
        else CHECK(owner.receive(reply(), 2, 6));                   // Exact, equal to it.
        succeeded(owner, id); // Matching and settlement are unchanged.
        CHECK(!owner.unresolved(0));
        if (form < 2) {
            CHECK(before.unchanged(*owner.node(0)));
            CHECK(!eligible(owner, 6).ordinarySdo && !owner.checkAdmission(0, write(), 6));
        } else {
            // Unchanged: equal exact timestamps cannot order, so this capture still records.
            CHECK(owner.node(0)->serviceQualification.receivedTimeKnown);
            CHECK(owner.node(0)->serviceQualification.receivedUs == 2 && eligible(owner, 6).ordinarySdo);
        }
    }
}
void nodeBoundaryKeepsItsStrictness() {
    // Attaching passive routing sets an inclusive boundary: an interval may start on it.
    for (unsigned straddle = 0; straddle < 2; ++straddle) {
        C::SdoClient owner;
        CHECK(owner.configure(0, channel(), false, true));
        const auto id = sent(owner, 1, 2);
        CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 10));
        CHECK(owner.setServiceAge(0, 20, 10));
        const Saved<C::NodeSnapshot> before(*owner.node(0));
        CHECK(owner.receive(reply(), straddle ? 9 : 10, 12, 12));
        succeeded(owner, id);
        if (straddle) {
            CHECK(before.unchanged(*owner.node(0)) && !eligible(owner, 12).ordinarySdo);
        } else {
            CHECK(owner.node(0)->serviceQualification.receivedEarliestUs == 10);
            CHECK(eligible(owner, 12).ordinarySdo);
        }
    }
}
void recoveryClearsBoundedQualification() {
    for (unsigned exact = 0; exact < 2; ++exact) {
        C::SdoClient owner; unconfirmed(owner);
        const auto first = sent(owner, 1, 2);
        CHECK(owner.receive(reply(), 3, 5, 6));
        succeeded(owner, first);
        CHECK(eligible(owner, 6).ordinarySdo);
        CHECK(owner.reportController(C::ControllerState::BUS_OFF, 10));
        const C::NodeSnapshot& node = *owner.node(0);
        CHECK(!node.serviceQualification.present && !node.sdo.present && !owner.unresolved(0));
        CHECK(node.strictBoundary && node.boundaryUs == 10);
        CHECK(owner.reportController(C::ControllerState::RECOVERING, 10));
        CHECK(owner.reportController(C::ControllerState::AVAILABLE, 10));
        CHECK(!eligible(owner, 10).ordinarySdo && !owner.checkAdmission(0, write(), 10));
        // After invalidation the boundary is strict: a reply on it is not after it, in either form.
        const auto equal = sent(owner, 10, 10);
        const Saved<C::NodeSnapshot> before(node);
        CHECK(exact ? owner.receive(reply(), 10, 12) : owner.receive(reply(), 10, 12, 12));
        succeeded(owner, equal);
        CHECK(before.unchanged(node) && !eligible(owner, 12).ordinarySdo);
        const auto later = sent(owner, 12, 12);
        CHECK(owner.receive(reply(), 13, 14, 14));
        succeeded(owner, later);
        CHECK(node.serviceQualification.receivedEarliestUs == 13 && eligible(owner, 14).ordinarySdo);
    }
    for (unsigned busOff = 0; busOff < 2; ++busOff) {
        // Interval evidence kept before routing exists is withdrawn by a controller event too.
        C::SdoClient owner;
        CHECK(owner.configure(0, channel(), false, true));
        const auto id = sent(owner, 1, 2);
        CHECK(owner.receive(reply(), 3, 5, 6));
        succeeded(owner, id);
        if (busOff) {
            CHECK(owner.reportController(C::ControllerState::BUS_OFF, 7));
            CHECK(owner.reportController(C::ControllerState::AVAILABLE, 8));
        }
        CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 9));
        CHECK(owner.node(0)->serviceQualification.present == !busOff);
        CHECK(eligible(owner, 9).ordinarySdo == !busOff);
        if (!busOff) CHECK(owner.node(0)->serviceQualification.receivedEarliestUs == 3);
    }
}
void transitionNeedsTheWholeIntervalAfterIt() {
    C::SdoClient owner; unconfirmed(owner);
    const auto first = sent(owner, 1, 2);
    CHECK(owner.receive(reply(), 3, 5, 6));
    succeeded(owner, first);
    const auto inFlight = sent(owner, 6, 7);
    C::NmtTransfer work;
    work.target = route(); work.command = C::NmtCommand::START;
    work.request = 5; work.deadlineUs = 100; work.queueTimeoutUs = 50;
    C::NmtRequestId nmt;
    CHECK(owner.admitNmt(work, 7, nmt));
    M::Frame output;
    CHECK(owner.nextNmtTx(nmt, 8, output) && owner.confirmNmtTx(nmt, 8) && owner.releaseNmt(nmt));
    CHECK(owner.node(0)->intentPresent && owner.node(0)->transitionUs == 8);
    CHECK(!eligible(owner, 8).ordinarySdo); // The earlier proof predates the possible transition.
    // In flight across it: the interval starts on the transition, so it qualifies nothing.
    CHECK(owner.receive(reply(), 8, 10, 10));
    succeeded(owner, inFlight);
    CHECK(!eligible(owner, 10).ordinarySdo);
    const auto after = sent(owner, 10, 10);
    CHECK(owner.receive(reply(), 11, 12, 12));
    succeeded(owner, after);
    CHECK(eligible(owner, 12).ordinarySdo && !eligible(owner, 12).exactNmtKnown);
}
void rebootRevokesBoundedQualification() {
    C::SdoClient owner; unconfirmed(owner);
    const auto first = sent(owner, 1, 2);
    CHECK(owner.receive(reply(), 3, 5, 6));
    succeeded(owner, first);
    const auto inFlight = sent(owner, 6, 7);
    // A boot-up whose own interval starts after the qualifying reply's earliest bound.
    CHECK(owner.receive(heartbeat(0), 7, 8, 8));
    CHECK(owner.result(inFlight)->outcome == C::SdoOutcome::REBOOT && owner.unresolved(0));
    CHECK(!owner.node(0)->serviceQualification.present && !owner.node(0)->sdo.present);
    CHECK(!eligible(owner, 8).ordinarySdo);
    // The in-flight reply, delivered after the boot-up with an interval reaching before it.
    CHECK(!owner.receive(reply(), 7, 9, 9));
    CHECK(!owner.node(0)->serviceQualification.present && !eligible(owner, 9).ordinarySdo);
    CHECK(owner.result(inFlight)->outcome == C::SdoOutcome::REBOOT);
}
void olderBoundedReplyCannotRefreshNewerQualification() {
    // Two channels of one node, both handed off at 2. The newer reply is processed first.
    // Neither can be told apart by processing order alone; only the receive bounds order them.
    for (unsigned wide = 0; wide < 2; ++wide) {
        C::SdoClient owner; unconfirmed(owner);
        CHECK(owner.configure(1, channel(1), false, true));
        const auto late = sent(owner, 1, 2, 0);
        const auto newer = sent(owner, 2, 2, 1);
        CHECK(owner.receive(reply(1), 10, 12, 12));
        succeeded(owner, newer);
        const C::NodeSnapshot& node = *owner.node(0);
        const Saved<C::ObservationTime> proof(node.serviceQualification), sdo(node.sdo);
        // Received earlier and processed later. Its upper bound past 12 does not make it newer.
        CHECK(wide ? owner.receive(reply(0), 9, 15, 20) : owner.receive(reply(0), 3, 5, 20));
        succeeded(owner, late);
        CHECK(proof.unchanged(node.serviceQualification) && sdo.unchanged(node.sdo));
        CHECK(eligible(owner, 30).ordinarySdo && !eligible(owner, 31).ordinarySdo);
    }
}
void invalidReplyBoundsChangeNothing() {
    C::SdoClient owner; unconfirmed(owner);
    const auto id = sent(owner, 1, 2);
    const Saved<C::SdoClient> before(owner);
    CHECK(!owner.receive(reply(), 5, 4, 6) && before.unchanged(owner)); // Inverted.
    CHECK(!owner.receive(reply(), 3, 7, 6) && before.unchanged(owner)); // Upper bound in the future.
    CHECK(!owner.result(id));
    CHECK(owner.receive(reply(), 3, 5, 6));
    succeeded(owner, id);
    CHECK(eligible(owner, 6).ordinarySdo);
}
} // namespace
int main() {
    boundsAreExplicitAndAtomic();
    lifecycleNeedsEntireIntervalAfterBoundary();
    producerWriteWithdrawsOnlyItsTimingPremise();
    contradictoryOverlapWithdrawsState();
    nmtTransitionNeedsLowerBoundAndOriginalDeadline();
    sdoDeadlinesAndCorrelationStayConservative();
    boundedReplyQualifiesFromItsEarliestBound();
    intervalMustStartAfterItsHandoff();
    nodeBoundaryKeepsItsStrictness();
    recoveryClearsBoundedQualification();
    transitionNeedsTheWholeIntervalAfterIt();
    rebootRevokesBoundedQualification();
    olderBoundedReplyCannotRefreshNewerQualification();
    invalidReplyBoundsChangeNothing();
    std::printf("Conservative network receive time: %u checks\n", checks);
}
