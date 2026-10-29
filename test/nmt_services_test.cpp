// SPDX-License-Identifier: MIT
// Independent NMT wire vectors and owner evidence tests. All routes, object
// permissions and service premises below are synthetic, not vendor support.
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
M::Binding binding(unsigned node = 1, unsigned channel = 0) {
    M::Binding out;
    out.logicalAxisId = 100 + node + channel * 1000;
    out.endpoint.profile = 41; out.endpoint.address = node;
    out.endpoint.channel = static_cast<uint16_t>(channel);
    out.bindingGeneration = 3; out.configurationGeneration = 7;
    return out;
}
C::NetworkRoute route(unsigned node = 1) {
    C::NetworkRoute out;
    out.binding = binding(node); out.generation = 11;
    out.heartbeatId = 0x700 + node; out.emcyId = 0x80 + node;
    return out;
}
C::SdoChannel channel(unsigned node = 1, unsigned subchannel = 0) {
    C::SdoChannel out;
    out.binding = binding(node, subchannel);
    out.clientToServerId = 0x600 + node + subchannel * 0x20;
    out.serverToClientId = 0x580 + node + subchannel * 0x20;
    return out;
}
M::Frame heartbeat(unsigned node, uint8_t state) {
    M::Frame out;
    out.id = 0x700 + node; out.dlc = out.payloadLength = 1; out.data[0] = state;
    return out;
}
C::NmtTransfer request(C::NmtCommand command = C::NmtCommand::START, bool broadcast = false) {
    C::NmtTransfer out;
    out.command = command; out.broadcast = broadcast; out.target = route();
    out.request = 31; out.deadlineUs = 100; out.queueTimeoutUs = 20;
    return out;
}
C::SdoTransfer transfer(bool download = false, unsigned node = 1, unsigned subchannel = 0) {
    C::SdoTransfer out;
    out.channel = channel(node, subchannel); out.operation = 17; out.request = 19; out.step = 23;
    out.index = 0x6041; out.size = 2;
    out.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
    out.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
    out.deadlineUs = 90; out.queueTimeoutUs = 20; out.responseTimeoutUs = 50;
    out.data[0] = 0x35; out.data[1] = 0xa2;
    return out;
}
M::Frame reply(uint16_t index = 0x6041, bool download = false) {
    M::Frame out;
    out.id = 0x581; out.dlc = out.payloadLength = 8;
    out.data[0] = download ? 0x60 : 0x4b;
    out.data[1] = static_cast<uint8_t>(index); out.data[2] = static_cast<uint8_t>(index >> 8);
    if (!download) { out.data[4] = 0x35; out.data[5] = 0xa2; }
    return out;
}
void setup(C::SdoClient& owner, bool sdo = true, bool confirmed = true) {
    if (sdo) CHECK(owner.configure(0, channel(), confirmed, true));
    CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 0));
}
void expectWire(const M::Frame& frame, uint8_t command, uint8_t node) {
    CHECK(frame.id == 0 && frame.dlc == 2 && frame.payloadLength == 2);
    CHECK(!frame.extended && !frame.rtr && !frame.fd && !frame.brs && !frame.esi);
    CHECK(frame.data[0] == command && frame.data[1] == node);
    for (unsigned i = 2; i < sizeof frame.data; ++i) CHECK(frame.data[i] == 0);
}
C::ServiceEligibility eligibility(const C::SdoClient& owner, uint64_t now, unsigned slot = 0) {
    CHECK(owner.node(static_cast<uint8_t>(slot)));
    C::ServiceEligibility out;
    CHECK(C::serviceEligibility(*owner.node(static_cast<uint8_t>(slot)), now, out));
    return out;
}
C::NmtRequestId admitNmt(C::SdoClient& owner, const C::NmtTransfer& work, uint64_t now) {
    C::NmtRequestId id;
    CHECK(owner.admitNmt(work, now, id)); CHECK(!owner.nmtResult(id));
    return id;
}
C::SdoRequestId admitSdo(C::SdoClient& owner, const C::SdoTransfer& work, uint64_t now, uint8_t slot = 0) {
    C::SdoRequestId id;
    CHECK(owner.admit(slot, work, now, id)); CHECK(!owner.result(id));
    return id;
}
C::SdoTxId handoffSdo(C::SdoClient& owner, uint64_t now, uint8_t slot = 0) {
    M::Frame frame;
    C::SdoTxId id;
    CHECK(owner.nextTx(slot, now, frame, id)); CHECK(frame.payloadLength == 8);
    return id;
}
struct Observer {
    unsigned calls = 0;
    C::NetworkInvalidationEvent last;
    static void invalidate(void* context, const C::NetworkInvalidationEvent& event) {
        Observer& self = *static_cast<Observer*>(context); ++self.calls; self.last = event;
    }
    C::NetworkObserver view() {
        C::NetworkObserver out; out.context = this; out.invalidate = invalidate; return out;
    }
};

void codecAndOwnedVectors() {
    const C::NmtCommand commands[] = {C::NmtCommand::START, C::NmtCommand::STOP,
        C::NmtCommand::PRE_OPERATIONAL, C::NmtCommand::RESET_NODE, C::NmtCommand::RESET_COMMUNICATION};
    const uint8_t bytes[] = {0x01, 0x02, 0x80, 0x81, 0x82};
    const uint8_t targets[] = {0, 1, 127};
    for (unsigned i = 0; i < 5; ++i) for (unsigned j = 0; j < 3; ++j) {
        M::Frame encoded; encoded.id = 0x123; encoded.data[7] = 0xa5;
        CHECK(C::makeNmtRequest(commands[i], targets[j], encoded));
        expectWire(encoded, bytes[i], targets[j]);
        C::SdoClient owner;
        Observer observer;
        CHECK(owner.configureNetwork(0, route(targets[j] ? targets[j] : 1), observer.view(), 0));
        C::NmtTransfer work = request(commands[i], targets[j] == 0);
        work.target = route(targets[j] ? targets[j] : 1);
        const auto id = admitNmt(owner, work, 1);
        work.command = C::NmtCommand::STOP; work.request = 99;
        CHECK(owner.nmtPort()->accepted == 0 && !owner.nmtPort()->queueOccupied);
        M::Frame frame;
        CHECK(owner.nextNmtTx(id, 2, frame)); expectWire(frame, bytes[i], targets[j]);
        CHECK(observer.calls == 1 && observer.last.command == commands[i]);
        CHECK(observer.last.cause == (i < 3 ? C::NetworkInvalidation::NMT_TRANSITION : C::NetworkInvalidation::NMT_RESET));
        CHECK(!owner.nmtResult(id)); CHECK(owner.node(0)->requestedId == 31);
        CHECK(owner.node(0)->requestedCommand == commands[i]);
        CHECK(owner.canTransmitNmt(id, 2)); CHECK(owner.confirmNmtTx(id, 3));
        const auto* result = owner.nmtResult(id);
        CHECK(result && result->outcome == C::NmtOutcome::TRANSMITTED);
        CHECK(result->queued && result->transmissionPossible && result->transmissionConfirmed);
        CHECK(result->affectedCount == 1 && result->affected[0].binding.endpoint.address != 0);
        CHECK(!eligibility(owner, 3).exactNmtKnown);
        const Saved<C::NmtCompletion> saved(*result);
        CHECK(owner.service(100)); CHECK(saved.unchanged(*owner.nmtResult(id)));
        CHECK(!owner.confirmNmtTx(id, 100) && !owner.cancelNmt(id, 100));
        CHECK(owner.releaseNmt(id)); CHECK(!owner.nmtResult(id) && !owner.releaseNmt(id));
    }
    M::Frame out; out.id = 0x719; out.data[3] = 0x68;
    const Saved<M::Frame> saved(out);
    CHECK(!C::makeNmtRequest(static_cast<C::NmtCommand>(0), 1, out)); CHECK(saved.unchanged(out));
    CHECK(!C::makeNmtRequest(C::NmtCommand::START, 128, out)); CHECK(saved.unchanged(out));
}

void invalidAdmissionsAndFrozenMembership() {
    for (unsigned fault = 0; fault < 12; ++fault) {
        C::SdoClient owner; setup(owner);
        C::NmtTransfer work = request();
        switch (fault) {
        case 0: work.command = static_cast<C::NmtCommand>(3); break;
        case 1: work.target.binding.endpoint.address = 0; break;
        case 2: work.target.binding.endpoint.address = 128; break;
        case 3: work.target.generation++; break;
        case 4: work.target.binding.bindingGeneration++; break;
        case 5: work.target.heartbeatId++; break;
        case 6: work.bus++; break;
        case 7: work.request = 0; break;
        case 8: work.queueTimeoutUs = 0; break;
        case 9: work.deadlineUs = 1; break;
        case 10: CHECK(owner.reportController(C::ControllerState::BUS_OFF, 0)); break;
        default: CHECK(owner.reportController(C::ControllerState::RECOVERING, 0)); break;
        }
        C::NmtRequestId id; id.owner = &owner; id.generation = 0x8123;
        const Saved<C::NmtRequestId> saved(id);
        const Saved<C::NodeSnapshot> node(*owner.node(0));
        CHECK(!owner.admitNmt(work, 1, id)); CHECK(saved.unchanged(id));
        CHECK(node.unchanged(*owner.node(0))); CHECK(owner.nmtPort()->accepted == 0);
    }
    C::SdoClient owner; setup(owner, false);
    const auto id = admitNmt(owner, request(C::NmtCommand::START, true), 1);
    CHECK(!owner.configure(1, channel(2), true, true));
    CHECK(!owner.configureNetwork(1, route(2), C::NetworkObserver(), 1));
    C::NmtRequestId other; const Saved<C::NmtRequestId> saved(other);
    CHECK(!owner.admitNmt(request(), 1, other)); CHECK(saved.unchanged(other));
    CHECK(!owner.releaseNmt(id)); CHECK(owner.cancelNmt(id, 2));
    CHECK(owner.nmtResult(id)->outcome == C::NmtOutcome::CANCELLED);
    CHECK(!owner.nmtResult(id)->transmissionPossible && !owner.node(0)->intentPresent);
    CHECK(!owner.configureNetwork(1, route(2), C::NetworkObserver(), 2));
    CHECK(owner.releaseNmt(id));
    CHECK(owner.configureNetwork(1, route(2), C::NetworkObserver(), 2));
    C::SdoClient noAnchor; CHECK(noAnchor.configure(0, channel(), true, true));
    CHECK(!noAnchor.admitNmt(request(C::NmtCommand::START, true), 0, other));
}

void queueFailureAndDeadlines() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        C::SdoClient owner; setup(owner);
        CHECK(owner.receiveNetwork(heartbeat(1, 4), route(), 1, 1));
        auto work = request(); work.deadlineUs = 40; work.queueTimeoutUs = 5;
        const auto id = admitNmt(owner, work, 2);
        if (scenario == 0) const_cast<CO_CANmodule_t*>(owner.nmtPort())->refuseSend = true;
        if (scenario == 1) CHECK(owner.service(2));
        M::Frame frame; frame.id = 0x123; const Saved<M::Frame> saved(frame);
        if (scenario == 2 || scenario == 3) {
            CHECK(owner.nextNmtTx(id, 3, frame));
            CHECK(!eligibility(owner, 3).exactNmtKnown);
            CHECK(owner.node(0)->transitionPending);
            if (scenario == 3) CHECK(owner.cancelNmt(id, 4));
            else CHECK(owner.service(40));
        } else {
            CHECK(owner.service(scenario ? 7 : 3));
            CHECK(!owner.nextNmtTx(id, scenario ? 7 : 3, frame)); CHECK(saved.unchanged(frame));
            CHECK(eligibility(owner, scenario ? 7 : 3).exactNmtKnown);
            CHECK(owner.node(0)->nmt == C::NmtState::STOPPED && !owner.node(0)->intentPresent);
        }
        const auto* result = owner.nmtResult(id); CHECK(result);
        CHECK(result->outcome == (scenario == 0 ? C::NmtOutcome::QUEUE_REJECTED :
            scenario == 3 ? C::NmtOutcome::CANCELLED : C::NmtOutcome::TIMEOUT));
        CHECK(result->transmissionPossible == (scenario >= 2));
        CHECK(!result->transmissionConfirmed);
        CHECK(!owner.confirmNmtTx(id, 40));
        if (scenario >= 2) {
            CHECK(owner.service(40)); CHECK(owner.node(0)->transitionExpired);
            CHECK(!owner.node(0)->transitionPending && !eligibility(owner, 40).exactNmtKnown);
            CHECK(owner.node(0)->nmt == C::NmtState::STOPPED);
        }
    }
}

void transitionObservations() {
    C::SdoClient owner; setup(owner);
    CHECK(owner.receiveNetwork(heartbeat(1, 4), route(), 1, 1));
    auto work = request(); work.deadlineUs = 30;
    const auto id = admitNmt(owner, work, 2);
    M::Frame frame; CHECK(owner.nextNmtTx(id, 10, frame));
    CHECK(owner.confirmNmtTx(id, 11));
    CHECK(owner.node(0)->nmt == C::NmtState::STOPPED);
    CHECK(owner.node(0)->transitionPending && !eligibility(owner, 11).ordinarySdo);
    CHECK(eligibility(owner, 11).nonChangingProbe && eligibility(owner, 11).tpdoReceive);
    // A timestamp no later than handoff cannot establish the requested state.
    CHECK(owner.receiveNetwork(heartbeat(1, 5), route(), 9, 12));
    CHECK(owner.node(0)->transitionPending && !eligibility(owner, 12).exactNmtKnown);
    CHECK(owner.receiveNetwork(heartbeat(1, 5), route(), 10, 13));
    CHECK(owner.node(0)->transitionPending && !eligibility(owner, 13).exactNmtKnown);
    CHECK(owner.receiveNetwork(heartbeat(1, 127), route(), 14, 14));
    CHECK(owner.node(0)->transitionConflict && owner.node(0)->transitionPending);
    auto e = eligibility(owner, 14);
    CHECK(e.exactNmtKnown && e.state == C::NmtState::PRE_OPERATIONAL && e.ordinarySdo && !e.pdoTransmit);
    const Saved<C::NmtCompletion> terminal(*owner.nmtResult(id));
    CHECK(owner.receiveNetwork(heartbeat(1, 5), route(), 15, 15));
    CHECK(!owner.node(0)->transitionPending && owner.node(0)->transitionConflict);
    CHECK(eligibility(owner, 15).pdoTransmit);
    CHECK(terminal.unchanged(*owner.nmtResult(id)));
    CHECK(owner.service(30)); CHECK(!owner.node(0)->transitionExpired);
    CHECK(owner.nmtPort()->accepted == 1); // Observation never sends another request.
}

void serviceStatesAndAges() {
    const uint8_t states[] = {127, 5, 4};
    for (unsigned i = 0; i < 3; ++i) {
        C::SdoClient owner; setup(owner);
        CHECK(owner.receiveNetwork(heartbeat(1, states[i]), route(), 1, 1));
        const auto e = eligibility(owner, 1);
        CHECK(e.exactNmtKnown && static_cast<uint8_t>(e.state) == states[i]);
        CHECK(e.ordinarySdo == (i != 2)); CHECK(e.nonChangingProbe == (i != 2));
        CHECK(e.pdoTransmit == (i == 1) && e.tpdoReceive == (i == 1));
        C::SdoRequestId id;
        CHECK(owner.admit(0, transfer(true), 2, id) == (i != 2));
        if (i != 2) CHECK(owner.cancel(id, 2));
    }
    C::SdoClient owner; setup(owner, true, false);
    auto e = eligibility(owner, 0);
    CHECK(!e.exactNmtKnown && !e.ordinarySdo && e.nonChangingProbe && !e.pdoTransmit && e.tpdoReceive);
    CHECK(owner.setProducerInterval(0, route(), 0, 0));
    CHECK(owner.nmtPort()->accepted == 0 && owner.port(0)->accepted == 0);
    CHECK(owner.setServiceAge(0, 5, 0));
    CHECK(owner.receiveNetwork(heartbeat(1, 5), route(), 1, 1));
    CHECK(eligibility(owner, 6).exactNmtKnown && eligibility(owner, 6).ordinarySdo);
    e = eligibility(owner, 7);
    CHECK(!e.exactNmtKnown && !e.ordinarySdo && e.nonChangingProbe && e.tpdoReceive);
    C::ServiceEligibility output; output.ordinarySdo = true;
    const Saved<C::ServiceEligibility> saved(output);
    CHECK(!C::serviceEligibility(*owner.node(0), 0, output)); CHECK(saved.unchanged(output));
    CHECK(!owner.setServiceAge(0, 0, 7));
    C::NodeSnapshot invalid = *owner.node(0); invalid.maximumEvidenceAgeUs = 0;
    CHECK(!C::serviceEligibility(invalid, 7, output)); CHECK(saved.unchanged(output));
}

void disabledProducerReconciliation() {
    C::SdoClient owner; setup(owner, true, false);
    CHECK(owner.setProducerInterval(0, route(), 0, 0));
    C::SdoRequestId refused;
    const Saved<C::SdoRequestId> saved(refused);
    auto write = transfer(true); write.index = 0x1017; write.data[0] = 100; write.data[1] = 0;
    CHECK(!owner.admit(0, write, 1, refused)); CHECK(saved.unchanged(refused));
    auto unreviewed = transfer(); unreviewed.readEffect = C::SdoReadEffect::UNQUALIFIED;
    CHECK(!owner.admit(0, unreviewed, 1, refused)); CHECK(saved.unchanged(refused));
    const auto probe = admitSdo(owner, transfer(), 1);
    handoffSdo(owner, 2);
    CHECK(owner.receive(reply(), 3, 3));
    CHECK(owner.result(probe) && owner.result(probe)->outcome == C::SdoOutcome::SUCCESS);
    auto e = eligibility(owner, 3);
    CHECK(!e.exactNmtKnown && e.ordinarySdo && !e.pdoTransmit && e.tpdoReceive);
    CHECK(owner.node(0)->serviceFromSdo && owner.node(0)->producerTimeMs == 0);
    CHECK(owner.release(probe));
    const auto setting = admitSdo(owner, write, 4);
    M::Frame frame; C::SdoTxId tx;
    CHECK(owner.nextTx(0, 5, frame, tx));
    CHECK(frame.id == 0x601 && frame.data[0] == 0x2b && frame.data[1] == 0x17 && frame.data[2] == 0x10);
    CHECK(frame.data[4] == 100 && frame.data[5] == 0);
    CHECK(owner.receive(reply(0x1017, true), 6, 6));
    CHECK(owner.result(setting)->outcome == C::SdoOutcome::SUCCESS);
    CHECK(!eligibility(owner, 6).exactNmtKnown && !owner.node(0)->heartbeat.present);
    CHECK(owner.node(0)->producerTimeMs == 0); // Acknowledgement alone does not install effective metadata.
    CHECK(owner.setProducerInterval(0, route(), 100, 6));
    CHECK(owner.node(0)->producerTimeMs == 100 && !owner.node(0)->heartbeat.present);
    CHECK(owner.port(0)->accepted == 2 && owner.nmtPort()->accepted == 0);
}

void broadcastEffectsAndFrozenResults() {
    for (unsigned shared = 0; shared < 2; ++shared) {
        C::SdoClient owner; setup(owner);
        Observer a, b; CHECK(owner.setNetworkObserver(0, a.view()));
        if (shared) {
            CHECK(owner.configure(1, channel(1, 1), true, true));
            CHECK(owner.setNetworkObserver(1, b.view()));
            CHECK(owner.node(1) == owner.node(0));
        } else {
            CHECK(owner.configureNetwork(1, route(2), b.view(), 0));
        }
        auto write = transfer(true); const auto pending = admitSdo(owner, write, 1);
        handoffSdo(owner, 2);
        const auto id = admitNmt(owner, request(C::NmtCommand::RESET_COMMUNICATION, true), 3);
        M::Frame frame; CHECK(owner.nextNmtTx(id, 4, frame)); expectWire(frame, 0x82, 0);
        CHECK(owner.unresolved(0)); if (shared) CHECK(owner.unresolved(1));
        CHECK(a.calls == 1 && b.calls == 1);
        CHECK(a.last.cause == C::NetworkInvalidation::NMT_RESET && b.last.cause == C::NetworkInvalidation::NMT_RESET);
        CHECK(owner.node(0)->route.generation == 12 && owner.node(1)->route.generation == 12);
        CHECK(owner.node(0)->requestedId == 31 && owner.node(1)->requestedId == 31);
        CHECK(owner.result(pending) && owner.result(pending)->effectUncertain);
        const Saved<C::SdoCompletion> retained(*owner.result(pending));
        CHECK(owner.confirmNmtTx(id, 5));
        const auto* result = owner.nmtResult(id); CHECK(result && result->affectedCount == 2);
        for (unsigned i = 0; i < 2; ++i) {
            CHECK(result->affected[i].previousGeneration == 11 && result->affected[i].generation == 12);
            CHECK(result->affected[i].binding.endpoint.address == (shared ? 1u : i + 1u));
        }
        CHECK(!M::sameBinding(result->affected[0].binding, result->affected[1].binding));
        CHECK(owner.service(100)); CHECK(retained.unchanged(*owner.result(pending)));
        CHECK(owner.releaseNmt(id)); CHECK(owner.release(pending)); CHECK(owner.unresolved(0));
    }
    // Addressed nonreset commands affect only that node and preserve unrelated evidence.
    C::SdoClient owner; setup(owner, false);
    Observer a, b; CHECK(owner.setNetworkObserver(0, a.view()));
    CHECK(owner.configureNetwork(1, route(2), b.view(), 0));
    CHECK(owner.receiveNetwork(heartbeat(2, 5), route(2), 1, 1));
    const auto id = admitNmt(owner, request(C::NmtCommand::STOP), 2);
    M::Frame frame; CHECK(owner.nextNmtTx(id, 3, frame));
    CHECK(a.calls == 1 && b.calls == 0 && eligibility(owner, 3, 1).pdoTransmit);
    CHECK(owner.node(0)->route.generation == 11 && owner.node(1)->route.generation == 11);
}

void sdoDeadlinesAndStopUncertainty() {
    for (unsigned stop = 0; stop < 2; ++stop) {
        C::SdoClient owner; setup(owner);
        CHECK(owner.receiveNetwork(heartbeat(1, 5), route(), 1, 1));
        auto write = transfer(true); write.deadlineUs = 20; write.responseTimeoutUs = 15;
        const auto sdo = admitSdo(owner, write, 2); handoffSdo(owner, 3);
        const auto nmt = admitNmt(owner, request(stop ? C::NmtCommand::STOP : C::NmtCommand::START), 4);
        M::Frame frame; CHECK(owner.nextNmtTx(nmt, 5, frame));
        if (stop) {
            CHECK(owner.result(sdo) && owner.result(sdo)->outcome == C::SdoOutcome::BINDING_INVALIDATED);
            CHECK(owner.result(sdo)->effectUncertain && owner.result(sdo)->transmissionPossible);
        } else {
            CHECK(!owner.result(sdo));
            CHECK(owner.receiveNetwork(heartbeat(1, 127), route(), 6, 6));
            CHECK(owner.node(0)->transitionConflict && !owner.result(sdo));
            CHECK(owner.service(18)); // Original response cutoff: handoff 3 + 15.
            CHECK(owner.result(sdo) && owner.result(sdo)->outcome == C::SdoOutcome::TIMEOUT);
            CHECK(owner.result(sdo)->transfer.deadlineUs == 20 && owner.result(sdo)->effectUncertain);
        }
        const Saved<C::SdoCompletion> retained(*owner.result(sdo));
        CHECK(owner.confirmNmtTx(nmt, 19)); CHECK(owner.service(100));
        CHECK(retained.unchanged(*owner.result(sdo)));
        CHECK(owner.nmtPort()->accepted == 1 && owner.port(0)->accepted == 1);
    }
}

void eligibilityRecheckedAtHandoff() {
    for (unsigned aged = 0; aged < 2; ++aged) for (unsigned alreadyHanded = 0; alreadyHanded < 2; ++alreadyHanded) {
        C::SdoClient owner; setup(owner);
        CHECK(owner.setServiceAge(0, 5, 0));
        CHECK(owner.receiveNetwork(heartbeat(1, 5), route(), 1, 1));
        const auto id = admitSdo(owner, transfer(true), 2);
        C::SdoTxId tx;
        if (alreadyHanded) tx = handoffSdo(owner, 3);
        else CHECK(owner.service(3)); // Stage while eligibility is still valid.
        const uint64_t now = aged ? 7 : 4;
        if (!aged) CHECK(owner.receiveNetwork(heartbeat(1, 4), route(), now, now));
        if (alreadyHanded) {
            CHECK(!owner.canTransmit(tx, now));
            if (aged) {
                CHECK(!owner.result(id)); // Age alone cannot prove that a handoff was unsent.
                CHECK(owner.cancel(id, now));
            } else {
                CHECK(owner.result(id) && owner.result(id)->outcome == C::SdoOutcome::BINDING_INVALIDATED);
                CHECK(!owner.cancel(id, now));
            }
            CHECK(owner.result(id)->transmissionPossible && owner.result(id)->effectUncertain);
        } else {
            M::Frame frame; frame.id = 0x199;
            const Saved<M::Frame> saved(frame);
            CHECK(!owner.nextTx(0, now, frame, tx)); CHECK(saved.unchanged(frame));
            CHECK(owner.result(id) && !owner.result(id)->transmissionPossible);
            CHECK(!owner.result(id)->effectPossible && !owner.result(id)->effectUncertain);
            CHECK(!owner.port(0)->queueOccupied);
        }
    }
}

void nmtDeadlineWinsLifecycleEquality() {
    for (unsigned reboot = 0; reboot < 2; ++reboot) for (unsigned handed = 0; handed < 2; ++handed) {
        C::SdoClient owner; setup(owner);
        auto work = request(); work.deadlineUs = 10; work.queueTimeoutUs = 20;
        const auto id = admitNmt(owner, work, 1);
        if (handed) { M::Frame frame; CHECK(owner.nextNmtTx(id, 2, frame)); }
        else CHECK(owner.service(2));
        if (reboot) CHECK(owner.reboot(1, 10));
        else CHECK(owner.busOff(10));
        const auto* result = owner.nmtResult(id);
        CHECK(result && result->outcome == C::NmtOutcome::TIMEOUT && result->completedUs == 10);
        CHECK(result->transmissionPossible == (handed != 0));
        const Saved<C::NmtCompletion> retained(*result);
        CHECK(owner.service(11)); CHECK(retained.unchanged(*owner.nmtResult(id)));
    }
}

void stoppedTransitionProbeDoesNotInventExactState() {
    C::SdoClient owner; setup(owner);
    CHECK(owner.receiveNetwork(heartbeat(1, 4), route(), 1, 1));
    C::SdoRequestId refused;
    CHECK(!owner.admit(0, transfer(), 2, refused));
    const auto nmt = admitNmt(owner, request(), 2);
    M::Frame frame; CHECK(owner.nextNmtTx(nmt, 3, frame));
    CHECK(owner.confirmNmtTx(nmt, 4));
    CHECK(owner.node(0)->nmt == C::NmtState::STOPPED && !eligibility(owner, 4).exactNmtKnown);
    CHECK(!owner.admit(0, transfer(true), 4, refused));
    const auto probe = admitSdo(owner, transfer(), 5); handoffSdo(owner, 6);
    CHECK(owner.receive(reply(), 7, 7)); CHECK(owner.result(probe)->outcome == C::SdoOutcome::SUCCESS);
    const auto e = eligibility(owner, 7);
    CHECK(e.ordinarySdo && !e.exactNmtKnown && !e.pdoTransmit && e.tpdoReceive);
    CHECK(owner.node(0)->transitionPending && owner.node(0)->nmt == C::NmtState::STOPPED);
    CHECK(owner.nmtPort()->accepted == 1 && owner.port(0)->accepted == 1);
}

void preexistingTerminalsSurviveReset() {
    C::SdoClient owner; setup(owner);
    const auto sdo = admitSdo(owner, transfer(), 1); handoffSdo(owner, 2);
    CHECK(owner.receive(reply(), 3, 3));
    CHECK(owner.result(sdo)->outcome == C::SdoOutcome::SUCCESS);
    const Saved<C::SdoCompletion> retained(*owner.result(sdo));
    const auto nmt = admitNmt(owner, request(C::NmtCommand::RESET_NODE), 4);
    M::Frame frame; CHECK(owner.nextNmtTx(nmt, 5, frame));
    CHECK(owner.unresolved(0) && retained.unchanged(*owner.result(sdo)));
    CHECK(owner.confirmNmtTx(nmt, 6)); CHECK(owner.release(sdo)); CHECK(owner.unresolved(0));
    C::SdoRequestId rejected; CHECK(!owner.admit(0, transfer(), 7, rejected));
}

void invalidatedStartIntentCannotCompleteInNewEpoch() {
    for (unsigned cause = 0; cause < 3; ++cause) {
        C::SdoClient owner; setup(owner);
        const auto nmt = admitNmt(owner, request(), 1);
        M::Frame frame; CHECK(owner.nextNmtTx(nmt, 2, frame));
        CHECK(owner.confirmNmtTx(nmt, 3)); CHECK(owner.releaseNmt(nmt));
        CHECK(owner.node(0)->transitionPending && !owner.node(0)->transitionObserved);
        if (cause == 0) CHECK(owner.receiveNetwork(heartbeat(1, 0), route(), 4, 4));
        else if (cause == 1) CHECK(owner.invalidateNetwork(0, C::NetworkInvalidation::BINDING, 4));
        else CHECK(owner.reportController(C::ControllerState::BUS_OFF, 4));
        CHECK(!owner.node(0)->transitionPending && owner.node(0)->transitionInvalidated);
        CHECK(!owner.node(0)->transitionObserved && !owner.node(0)->transitionExpired);
        CHECK(owner.node(0)->requestedId == 31 && owner.node(0)->requestedCommand == C::NmtCommand::START);
        const C::NetworkRoute renewed = owner.node(0)->route;
        CHECK(renewed.generation == route().generation + 1);
        if (cause == 2) CHECK(owner.reportController(C::ControllerState::AVAILABLE, 5));
        CHECK(owner.receiveNetwork(heartbeat(1, 5), renewed, 6, 6));
        CHECK(eligibility(owner, 6).exactNmtKnown && eligibility(owner, 6).state == C::NmtState::OPERATIONAL);
        CHECK(owner.node(0)->transitionInvalidated && !owner.node(0)->transitionObserved);
        CHECK(!owner.node(0)->transitionPending && owner.unresolved(0));
        auto next = request(C::NmtCommand::PRE_OPERATIONAL); next.target = renewed; next.request = 32;
        const auto replacement = admitNmt(owner, next, 7);
        CHECK(owner.cancelNmt(replacement, 7)); // Dead old intent cannot reserve the node until time 100.
        CHECK(owner.node(0)->requestedId == 31 && owner.node(0)->transitionInvalidated);
    }
}

void resetObservationRequiresQualifiedWireEvidence() {
    for (unsigned command = 0; command < 2; ++command) for (unsigned evidence = 0; evidence < 5; ++evidence) {
        C::SdoClient owner; setup(owner);
        const auto nmt = admitNmt(owner, request(command ? C::NmtCommand::RESET_COMMUNICATION :
                                                         C::NmtCommand::RESET_NODE), 1);
        M::Frame frame; CHECK(owner.nextNmtTx(nmt, 2, frame));
        CHECK(owner.confirmNmtTx(nmt, 3));
        const Saved<C::NmtCompletion> retained(*owner.nmtResult(nmt));
        const C::NetworkRoute resetRoute = owner.node(0)->route;
        CHECK(owner.node(0)->transitionPending && !owner.node(0)->transitionObserved);
        if (evidence == 0) CHECK(owner.receiveNetwork(heartbeat(1, 0), resetRoute, 4, 4));
        else if (evidence == 1) CHECK(owner.receive(heartbeat(1, 0), 4)); // Unknown capture is not reset completion.
        else if (evidence == 2) CHECK(owner.reboot(1, 4)); // Application report is not observed wire timing.
        else if (evidence == 3) CHECK(!owner.receiveNetwork(heartbeat(1, 0), resetRoute, 2, 4));
        else CHECK(owner.receiveNetwork(heartbeat(1, 0), resetRoute, 99, 100)); // Processing cutoff wins.
        CHECK(owner.node(0)->transitionObserved == (evidence == 0));
        CHECK(owner.node(0)->transitionInvalidated == (evidence == 1 || evidence == 2));
        CHECK(owner.node(0)->transitionPending == (evidence == 3));
        CHECK(owner.node(0)->transitionExpired == (evidence == 4));
        CHECK(retained.unchanged(*owner.nmtResult(nmt)) && owner.unresolved(0));
        CHECK(!owner.node(0)->heartbeat.present);
        if (evidence != 3) {
            const C::NetworkRoute current = owner.node(0)->route;
            const uint64_t now = evidence == 4 ? 101 : 5;
            CHECK(owner.receiveNetwork(heartbeat(1, 5), current, now, now));
            CHECK(owner.node(0)->transitionObserved == (evidence == 0));
            CHECK(!owner.node(0)->transitionPending && owner.unresolved(0));
            CHECK(retained.unchanged(*owner.nmtResult(nmt)));
        }
    }
}

void explicitStopRetiresQueuedAndPossibleSdo() {
    for (unsigned handed = 0; handed < 2; ++handed) for (unsigned changing = 0; changing < 2; ++changing) {
        C::SdoClient owner; setup(owner);
        const auto sdo = admitSdo(owner, transfer(changing != 0), 1);
        C::SdoTxId old;
        if (handed) old = handoffSdo(owner, 2);
        else CHECK(owner.service(2));
        const auto nmt = admitNmt(owner, request(C::NmtCommand::STOP), 3);
        M::Frame frame; CHECK(owner.nextNmtTx(nmt, 4, frame));
        const auto* result = owner.result(sdo);
        CHECK(result && result->outcome == C::SdoOutcome::BINDING_INVALIDATED);
        CHECK(result->transmissionPossible == (handed != 0));
        CHECK(result->effectUncertain == (handed && changing));
        CHECK(owner.unresolved(0) == (handed != 0));
        CHECK(!owner.port(0)->queueOccupied && !owner.canTransmit(old, 4));
        const Saved<C::SdoCompletion> retained(*result);
        CHECK(owner.confirmNmtTx(nmt, 5));
        CHECK(owner.receiveNetwork(heartbeat(1, 4), route(), 6, 6));
        CHECK(owner.node(0)->transitionObserved && !owner.node(0)->transitionPending);
        CHECK(retained.unchanged(*owner.result(sdo)));
        CHECK(owner.release(sdo));
        C::SdoRequestId rejected; CHECK(!owner.admit(0, transfer(), 7, rejected));
        CHECK(owner.port(0)->accepted == 1 && owner.nmtPort()->accepted == 1);
    }
}

void nmtCutoffsDoNotWrap() {
    for (unsigned handed = 0; handed < 2; ++handed) {
        C::SdoClient owner; setup(owner, false);
        auto work = request(); work.deadlineUs = UINT64_MAX; work.queueTimeoutUs = handed ? 10 : 3;
        const auto nmt = admitNmt(owner, work, UINT64_MAX - 5);
        M::Frame frame; frame.id = 0x199;
        if (handed) CHECK(owner.nextNmtTx(nmt, UINT64_MAX - 1, frame));
        const uint64_t cutoff = handed ? UINT64_MAX : UINT64_MAX - 2;
        const Saved<M::Frame> unchanged(frame);
        CHECK(!owner.nextNmtTx(nmt, cutoff, frame)); CHECK(unchanged.unchanged(frame));
        CHECK(!owner.confirmNmtTx(nmt, cutoff));
        const auto* result = owner.nmtResult(nmt);
        CHECK(result && result->outcome == C::NmtOutcome::TIMEOUT && result->completedUs == cutoff);
        CHECK(result->transmissionPossible == (handed != 0));
        CHECK(owner.node(0)->transitionExpired == (handed != 0));
    }
}

void networkAttachmentPreservesServiceProof() {
    // An initial external premise works in either configure order; installing
    // passive routing later must not renew its original evidence timestamp.
    for (unsigned networkFirst = 0; networkFirst < 2; ++networkFirst) {
        C::SdoClient owner;
        if (networkFirst) CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 0));
        CHECK(owner.configure(0, channel(), true, true));
        if (!networkFirst) CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 0));
        CHECK(owner.setServiceAge(0, 5, 0));
        CHECK(eligibility(owner, 0).ordinarySdo);
        CHECK(owner.node(0)->serviceQualification.receivedUs == 0);
        CHECK(!owner.node(0)->serviceFromSdo && !owner.node(0)->sdo.present);
    }
    {
        C::SdoClient owner;
        CHECK(owner.configure(0, channel(), true, true));
        CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 100));
        CHECK(owner.setServiceAge(0, 5, 100));
        CHECK(!eligibility(owner, 100).ordinarySdo);
        CHECK(owner.node(0)->serviceQualification.receivedUs == 0);
        CHECK(!owner.node(0)->serviceFromSdo);
    }
    for (unsigned known = 0; known < 2; ++known) for (unsigned delayed = 0; delayed < 2; ++delayed) {
        C::SdoClient owner;
        CHECK(owner.configure(0, channel(), false, true));
        const auto probe = admitSdo(owner, transfer(), 1);
        handoffSdo(owner, 2);
        CHECK(known ? owner.receive(reply(), 3, 3) : owner.receive(reply(), 3));
        CHECK(owner.result(probe)->outcome == C::SdoOutcome::SUCCESS);
        CHECK(owner.release(probe));
        const uint64_t now = delayed ? 100 : 4;
        CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), now));
        CHECK(owner.setServiceAge(0, 5, now));
        const auto e = eligibility(owner, now);
        CHECK(e.ordinarySdo == (known && !delayed));
        CHECK(!e.exactNmtKnown && !e.pdoTransmit && e.nonChangingProbe);
        CHECK(owner.node(0)->serviceFromSdo);
        CHECK(owner.node(0)->serviceQualification.receivedTimeKnown == (known != 0));
        CHECK(owner.node(0)->serviceQualification.processedUs == 3);
        if (known) CHECK(owner.node(0)->serviceQualification.receivedUs == 3);
        auto write = transfer(true); write.deadlineUs = 200;
        C::SdoRequestId admitted;
        CHECK(owner.admit(0, write, now, admitted) == (known && !delayed));
    }
}

void unknownCaptureCannotReplacePendingServiceProof() {
    C::SdoClient owner;
    CHECK(owner.configure(0, channel(), true, true));
    const auto probe = admitSdo(owner, transfer(), 1);
    handoffSdo(owner, 2);
    CHECK(owner.receive(reply(), 3));
    CHECK(owner.result(probe)->outcome == C::SdoOutcome::SUCCESS);
    CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 4));
    CHECK(owner.setServiceAge(0, 5, 4));
    CHECK(eligibility(owner, 4).ordinarySdo);
    CHECK(owner.node(0)->serviceQualification.receivedTimeKnown);
    CHECK(owner.node(0)->serviceQualification.receivedUs == 0);
    CHECK(!owner.node(0)->serviceFromSdo);
    CHECK(!eligibility(owner, 6).ordinarySdo);
}

void checkedAbortQualifiesServiceWithoutReopeningChannel() {
    C::SdoClient owner; setup(owner, true, false);
    const auto probe = admitSdo(owner, transfer(), 1);
    handoffSdo(owner, 2);
    M::Frame abort = reply();
    abort.data[0] = 0x80;
    abort.data[4] = 0x00; abort.data[5] = 0x00; abort.data[6] = 0x02; abort.data[7] = 0x06;
    CHECK(owner.receive(abort, 3, 3));
    CHECK(owner.result(probe)->outcome == C::SdoOutcome::SERVER_ABORT);
    CHECK(owner.unresolved(0));
    CHECK(eligibility(owner, 3).ordinarySdo && !eligibility(owner, 3).exactNmtKnown);
    CHECK(owner.node(0)->serviceFromSdo && owner.node(0)->serviceQualification.receivedUs == 3);
    CHECK(owner.release(probe));
    C::SdoRequestId rejected;
    CHECK(!owner.admit(0, transfer(true), 4, rejected));
}

void networkAttachmentSelectsUsableChannelProof() {
    for (unsigned quarantine = 0; quarantine < 2; ++quarantine) {
        C::SdoClient owner;
        CHECK(owner.configure(0, channel(), true, true));
        CHECK(owner.service(10));
        CHECK(owner.configure(1, channel(1, 1), true, true));
        if (quarantine) CHECK(owner.invalidate(1, 11));
        CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 12));
        CHECK(owner.setServiceAge(0, 5, 12));
        CHECK(owner.node(0) == owner.node(1));
        CHECK(owner.node(0)->serviceQualification.receivedUs == (quarantine ? 0u : 10u));
        CHECK(eligibility(owner, 12).ordinarySdo == (quarantine == 0));
        CHECK(!owner.node(0)->serviceFromSdo);
    }
    for (unsigned reset = 0; reset < 2; ++reset) {
        C::SdoClient owner;
        CHECK(owner.configure(0, channel(), true, true));
        if (reset) CHECK(owner.reboot(1, 1));
        else CHECK(owner.busOff(1));
        CHECK(owner.reportController(C::ControllerState::AVAILABLE, 2));
        CHECK(owner.configureNetwork(0, route(), C::NetworkObserver(), 3));
        CHECK(!owner.node(0)->serviceQualification.present);
        CHECK(!eligibility(owner, 3).ordinarySdo && eligibility(owner, 3).nonChangingProbe);
        CHECK(owner.unresolved(0));
    }
}
} // namespace
int main() {
    codecAndOwnedVectors(); invalidAdmissionsAndFrozenMembership(); queueFailureAndDeadlines();
    transitionObservations(); serviceStatesAndAges(); disabledProducerReconciliation();
    broadcastEffectsAndFrozenResults(); sdoDeadlinesAndStopUncertainty();
    eligibilityRecheckedAtHandoff(); nmtDeadlineWinsLifecycleEquality();
    stoppedTransitionProbeDoesNotInventExactState(); preexistingTerminalsSurviveReset();
    invalidatedStartIntentCannotCompleteInNewEpoch(); resetObservationRequiresQualifiedWireEvidence();
    explicitStopRetiresQueuedAndPossibleSdo(); nmtCutoffsDoNotWrap();
    networkAttachmentPreservesServiceProof(); unknownCaptureCannotReplacePendingServiceProof();
    checkedAbortQualifiesServiceWithoutReopeningChannel();
    networkAttachmentSelectsUsableChannelProof();
    std::printf("nmt_services: %u checks\n", checks);
}
