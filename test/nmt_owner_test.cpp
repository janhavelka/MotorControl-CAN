// SPDX-License-Identifier: MIT
// Host-only owner/transport evidence. Fixture routes/times are not device defaults.
#include "CanBusOwner.h"
#include <cstdio>
#include <cstdlib>
namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (false)
C::SdoChannel route(unsigned node) {
    C::SdoChannel out;
    out.binding.logicalAxisId = 100 + node;
    out.binding.endpoint.profile = 41;
    out.binding.endpoint.address = node;
    out.binding.bindingGeneration = 3;
    out.binding.configurationGeneration = 7;
    out.clientToServerId = 0x600 + node;
    out.serverToClientId = 0x580 + node;
    return out;
}
M::Frame frame(uint32_t id, uint8_t count) {
    M::Frame out;
    out.id = id;
    out.dlc = out.payloadLength = count;
    return out;
}
O::ServiceBudget budget(uint8_t tx = 2) {
    O::ServiceBudget out;
    out.rx = out.events = 8;
    out.starts = out.tx = tx;
    out.recovery = 1;
    return out;
}
struct Wire {
    enum { CAPACITY = 16 };
    M::Frame sent[CAPACITY];
    O::TransportTxId ids[CAPACITY];
    uint32_t epochs[CAPACITY] = {};
    bool retired[CAPACITY] = {};
    O::TxReport reports[CAPACITY];
    O::ReceivedFrame rx[CAPACITY];
    unsigned sentCount = 0, reportRead = 0, reportWrite = 0, rxRead = 0, rxWrite = 0;
    unsigned retireCalls = 0, beginCalls = 0, pollCalls = 0;
    bool accept = true, retireOk = false;
    O::RecoveryProgress recovery = O::RecoveryProgress::PENDING;
    static bool transmit(void* p, const M::Frame& value, O::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(p);
        CHECK(w.sentCount < CAPACITY);
        w.sent[w.sentCount] = value;
        w.ids[w.sentCount] = id;
        w.epochs[w.sentCount++] = epoch;
        return w.accept;
    }
    static bool report(void* p, O::TxReport& out) {
        Wire& w = *static_cast<Wire*>(p);
        if (w.reportRead == w.reportWrite) return false;
        out = w.reports[w.reportRead++];
        return true;
    }
    static bool receive(void* p, O::ReceivedFrame& out) {
        Wire& w = *static_cast<Wire*>(p);
        if (w.rxRead == w.rxWrite) return false;
        out = w.rx[w.rxRead++];
        return true;
    }
    static bool retire(void* p, O::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(p);
        ++w.retireCalls;
        if (!w.retireOk) return false;
        for (unsigned i = 0; i < w.sentCount; ++i)
            if (w.epochs[i] == epoch && O::sameTransportTxId(w.ids[i], id)) w.retired[i] = true;
        unsigned write = w.reportRead;
        for (unsigned i = w.reportRead; i < w.reportWrite; ++i)
            if (w.reports[i].controllerEpoch != epoch || !O::sameTransportTxId(w.reports[i].id, id))
                w.reports[write++] = w.reports[i];
        w.reportWrite = write;
        return true;
    }
    O::Transport transport() {
        O::Transport out;
        out.context = this;
        out.transmit = transmit;
        out.takeTxReport = report;
        out.receive = receive;
        out.retire = retire;
        out.beginRecovery = [](void* p, uint32_t) { ++static_cast<Wire*>(p)->beginCalls; return true; };
        out.pollRecovery = [](void* p, uint32_t) {
            Wire& w = *static_cast<Wire*>(p);
            ++w.pollCalls;
            return w.recovery;
        };
        return out;
    }
    void final(unsigned index, bool transmitted, uint32_t epoch = 0) {
        CHECK(index < sentCount && !retired[index] && reportWrite < CAPACITY);
        O::TxReport& out = reports[reportWrite++];
        out.id = ids[index];
        out.transmitted = transmitted;
        out.controllerEpoch = epoch ? epoch : epochs[index];
    }
    void push(const M::Frame& value, uint64_t time, uint32_t epoch = 1) {
        CHECK(rxWrite < CAPACITY);
        auto& out = rx[rxWrite++];
        out.frame = value;
        out.receivedTimeKnown = true;
        out.receivedUs = time;
        out.controllerEpoch = epoch;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    Rig() : owner(services, wire.transport(), 127) {
        for (uint8_t i = 0; i < 2; ++i) {
            CHECK(services.configure(i, route(i + 1), true, true));
            C::NetworkRoute network;
            network.binding = route(i + 1).binding;
            network.generation = 11;
            network.heartbeatId = 0x701 + i;
            network.emcyId = 0x81 + i;
            CHECK(services.configureNetwork(i, network, C::NetworkObserver(), 0));
        }
    }
    C::NmtTransfer work(C::NmtCommand command = C::NmtCommand::START, unsigned node = 1,
                        uint64_t deadline = 100) {
        C::NmtTransfer out;
        out.command = command;
        out.broadcast = node == 0;
        out.bus = 0;
        out.target = services.node(node ? uint8_t(node - 1) : 0)->route;
        out.request = 123;
        out.deadlineUs = deadline;
        out.queueTimeoutUs = 20;
        return out;
    }
    C::NmtRequestId admit(C::NmtCommand command = C::NmtCommand::START, unsigned node = 1,
                          uint64_t now = 0, uint64_t deadline = 100) {
        C::NmtRequestId id;
        CHECK(owner.admitNmt(work(command, node, deadline), now, id));
        return id;
    }
    void heartbeat(unsigned node, uint8_t state, uint64_t now) {
        M::Frame out = frame(0x700 + node, 1);
        out.data[0] = state;
        wire.push(out, now);
    }
    O::RequestId request(unsigned node, uint64_t now, bool download = false) {
        O::SequenceId sequence;
        CHECK(owner.beginSequence(route(node).binding, uint8_t(node), now, sequence));
        CHECK(owner.beginOperation(sequence, 1, 100, now));
        O::BusRequest out;
        out.sequence = sequence;
        out.transfer.channel = route(node);
        out.transfer.operation = out.transfer.step = out.transfer.request = 1;
        out.transfer.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
        out.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        out.transfer.index = download ? 0x6040 : 0x6041;
        out.transfer.size = 2;
        out.transfer.queueTimeoutUs = 20;
        out.transfer.responseTimeoutUs = 50;
        out.transfer.deadlineUs = 100;
        O::RequestId id;
        CHECK(owner.admit(out, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
};
void copiedExplicitIntentAndReports() {
    Rig r;
    CHECK(r.owner.service(0, budget()) && r.wire.sentCount == 0);
    r.heartbeat(1, 0x7f, 1);
    CHECK(r.owner.service(1, budget()));
    C::NmtTransfer input = r.work();
    C::NmtRequestId id;
    CHECK(r.owner.admitNmt(input, 2, id));
    input.command = C::NmtCommand::RESET_NODE;
    input.target.binding.endpoint.address = 2;
    CHECK(r.owner.service(3, O::ServiceBudget()) && r.wire.sentCount == 0);
    CHECK(r.owner.service(4, budget(1)) && r.wire.sentCount == 1);
    CHECK(r.wire.sent[0].id == 0 && r.wire.sent[0].payloadLength == 2 &&
          r.wire.sent[0].data[0] == uint8_t(C::NmtCommand::START) && r.wire.sent[0].data[1] == 1);
    CHECK(r.wire.ids[0].kind == O::TransportTxId::Kind::NMT &&
          r.wire.ids[0].nmt.generation == id.generation && !r.owner.nmtResult(id));
    O::TxReport wrong;
    wrong.id = O::TransportTxId::fromSdo(C::SdoTxId());
    wrong.id.sdo.request.owner = &r.services;
    wrong.id.sdo.request.generation = id.generation;
    wrong.id.sdo.frame = 1;
    wrong.transmitted = true;
    r.wire.reports[r.wire.reportWrite++] = wrong;
    r.wire.final(0, true, 2); // Wrong controller epoch cannot settle the frame.
    CHECK(r.owner.service(5, budget()) && !r.owner.nmtResult(id) && r.owner.stats().txFrames == 0);
    r.wire.final(0, true);
    CHECK(r.owner.service(6, budget()));
    const C::NmtCompletion* result = r.owner.nmtResult(id);
    CHECK(result && result->outcome == C::NmtOutcome::TRANSMITTED && result->transmissionConfirmed);
    C::ServiceEligibility eligibility;
    CHECK(C::serviceEligibility(*r.services.node(0), 6, eligibility) && !eligibility.exactNmtKnown);
    r.heartbeat(1, 4, 7);
    CHECK(r.owner.service(7, budget()));
    CHECK(r.services.node(0)->transitionConflict && !r.services.node(0)->transitionObserved);
    r.heartbeat(1, 5, 8);
    CHECK(r.owner.service(8, budget()));
    CHECK(r.services.node(0)->transitionObserved && r.owner.nmtResult(id) == result);
    CHECK(!r.owner.cancelNmt(id, 9) && r.owner.nmtResult(id)->outcome == C::NmtOutcome::TRANSMITTED);
    CHECK(r.owner.releaseNmt(id) && !r.owner.nmtResult(id));
    CHECK(r.wire.sentCount == 1 && r.owner.stats().txFrames == 1);
    bool found = false;
    O::Diagnostic diagnostic;
    while (r.owner.takeDiagnostic(diagnostic))
        if (diagnostic.kind == O::DiagnosticKind::TX_REPORT) {
            CHECK(diagnostic.txId.kind == O::TransportTxId::Kind::NMT && diagnostic.txId.nmt.generation == id.generation);
            found = true;
        }
    CHECK(found);
}
void cancellationAndUncertainEnqueue() {
    {
        Rig r;
        const auto id = r.admit();
        CHECK(r.owner.cancelNmt(id, 1));
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::CANCELLED &&
              !r.owner.nmtResult(id)->transmissionPossible);
        CHECK(r.owner.service(2, budget()) && r.wire.sentCount == 0);
        CHECK(r.owner.releaseNmt(id));
    }
    {
        Rig r;
        r.wire.accept = false;
        const auto id = r.admit();
        CHECK(r.owner.service(1, budget()));
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::TRANSPORT_ERROR &&
              r.owner.nmtResult(id)->transmissionPossible && !r.owner.nmtResult(id)->transmissionConfirmed);
        CHECK(r.wire.retireCalls == 1 && r.owner.releaseNmt(id));
        C::NmtRequestId refused;
        CHECK(!r.owner.admitNmt(r.work(), 2, refused));
        r.wire.accept = true;
        const auto other = r.request(2, 2);
        CHECK(r.owner.service(3, budget()) && r.wire.sentCount == 2);
        CHECK(!r.owner.result(other));
        CHECK(r.wire.ids[1].kind == O::TransportTxId::Kind::SDO &&
              r.wire.ids[1].sdo.request.channel == 1);
        r.wire.final(0, true); // Late proof retires old handoff, never rewrites released result.
        CHECK(r.owner.service(4, budget()));
        CHECK(r.owner.admitNmt(r.work(C::NmtCommand::START, 2), 5, refused));
        CHECK(r.wire.sentCount == 2); // No automatic replay of the first request.
    }
    {
        Rig r;
        const auto id = r.admit();
        CHECK(r.owner.service(1, budget()));
        CHECK(r.owner.cancelNmt(id, 2) && r.wire.retireCalls == 1);
        const C::NmtCompletion snapshot = *r.owner.nmtResult(id);
        r.wire.final(0, true);
        CHECK(r.owner.service(3, budget()));
        CHECK(r.owner.nmtResult(id)->outcome == snapshot.outcome &&
              !r.owner.nmtResult(id)->transmissionConfirmed && r.wire.sentCount == 1);
        CHECK(r.owner.releaseNmt(id));
    }
}
void finiteDeadlineAndRecovery() {
    {
        Rig r;
        const auto id = r.admit(C::NmtCommand::START, 1, 0, 10);
        CHECK(r.owner.service(10, O::ServiceBudget()));
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::TIMEOUT &&
              !r.owner.nmtResult(id)->transmissionPossible && r.wire.sentCount == 0);
    }
    {
        Rig r;
        const auto id = r.admit(C::NmtCommand::RESET_NODE, 1, 0, 10);
        CHECK(r.owner.service(1, budget()));
        r.wire.final(0, true);
        CHECK(r.owner.service(10, budget()));
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::TIMEOUT &&
              r.owner.nmtResult(id)->transmissionPossible && !r.owner.nmtResult(id)->transmissionConfirmed);
        CHECK(r.wire.sentCount == 1);
    }
    {
        Rig r;
        const auto id = r.admit(C::NmtCommand::RESET_COMMUNICATION);
        CHECK(r.owner.service(1, budget()));
        O::RecoveryId recovery;
        CHECK(r.owner.recover(2, 20, recovery));
        CHECK(r.owner.nmtResult(id)->outcome == C::NmtOutcome::INVALIDATED &&
              r.owner.nmtResult(id)->transmissionPossible);
        const auto preserved = *r.owner.nmtResult(id);
        r.wire.final(0, true); // Old epoch report cannot confirm or release the new epoch.
        r.wire.recovery = O::RecoveryProgress::RETIRED;
        CHECK(r.owner.service(3, budget()));
        CHECK(r.owner.recoveryResult(recovery)->outcome == O::RecoveryOutcome::RETIRED);
        CHECK(r.owner.nmtResult(id)->outcome == preserved.outcome && r.owner.stats().txFrames == 0);
        CHECK(r.owner.releaseNmt(id) && r.owner.releaseRecovery(recovery));
        CHECK(r.owner.service(4, budget()) && r.wire.sentCount == 1 && r.wire.beginCalls == 1);
    }
}
void ownerPreservesConservativeReceiveBounds() {
    Rig r;
    O::ReceivedFrame bounded;
    bounded.frame = frame(0x701, 1);
    bounded.frame.data[0] = 0x7f;
    bounded.receiveBoundsKnown = true;
    bounded.receivedEarliestUs = 1;
    bounded.receivedLatestUs = 2;
    r.wire.rx[r.wire.rxWrite++] = bounded;
    CHECK(r.owner.service(3, budget()));
    CHECK(r.services.node(0)->nmtTime.receiveBoundsKnown &&
          !r.services.node(0)->nmtTime.receivedTimeKnown &&
          r.services.node(0)->nmtTime.receivedEarliestUs == 1 &&
          r.services.node(0)->nmtTime.receivedLatestUs == 2);
    C::ServiceEligibility eligibility;
    CHECK(C::serviceEligibility(*r.services.node(0), 3, eligibility) && eligibility.exactNmtKnown);
    O::Diagnostic diagnostic;
    CHECK(r.owner.takeDiagnostic(diagnostic) && diagnostic.accepted && diagnostic.receiveBoundsKnown &&
          !diagnostic.receivedTimeKnown && diagnostic.receivedEarliestUs == 1 && diagnostic.receivedLatestUs == 2);
    // Conflicting representations must not silently choose the exact timestamp.
    bounded.receivedTimeKnown = true;
    bounded.receivedUs = 4;
    bounded.frame.data[0] = 4;
    bounded.receivedEarliestUs = 4;
    bounded.receivedLatestUs = 5;
    r.wire.rx[r.wire.rxWrite++] = bounded;
    CHECK(r.owner.service(6, budget()));
    CHECK(r.owner.stats().rxDropped == 1 && r.services.node(0)->nmt == C::NmtState::PRE_OPERATIONAL);
    CHECK(r.services.node(0)->nmtTime.receivedEarliestUs == 1);
    CHECK(r.owner.takeDiagnostic(diagnostic) && !diagnostic.accepted && diagnostic.receivedTimeKnown &&
          diagnostic.receiveBoundsKnown && diagnostic.receivedUs == 4 && diagnostic.receivedLatestUs == 5);
    for (unsigned invalid = 0; invalid < 2; ++invalid) {
        bounded.receivedTimeKnown = false;
        bounded.receivedEarliestUs = invalid ? 9 : 8;
        bounded.receivedLatestUs = invalid ? 10 : 7;
        r.wire.rx[r.wire.rxWrite++] = bounded;
        CHECK(r.owner.service(8, budget()));
    }
    CHECK(r.owner.stats().rxDropped == 3 && r.services.node(0)->nmt == C::NmtState::PRE_OPERATIONAL);
}
void twoNodePendingSdoAndBroadcast() {
    for (unsigned broadcast = 0; broadcast < 2; ++broadcast) {
        Rig r;
        r.heartbeat(1, 5, 0);
        r.heartbeat(2, 5, 0);
        CHECK(r.owner.service(0, budget()));
        const auto first = r.request(1, 1, true);
        const auto second = r.request(2, 1);
        CHECK(r.owner.service(2, budget()) && r.wire.sentCount == 2);
        const auto nmt = r.admit(C::NmtCommand::RESET_NODE, broadcast ? 0 : 1, 3);
        CHECK(r.owner.service(4, budget()) && r.wire.sentCount == 3);
        CHECK(r.wire.ids[2].kind == O::TransportTxId::Kind::NMT);
        CHECK(r.owner.result(first) && r.owner.result(first)->service.effectUncertain);
        CHECK(r.services.node(0)->route.generation == 12);
        CHECK(r.services.node(1)->route.generation == (broadcast ? 12u : 11u));
        if (broadcast) {
            CHECK(r.owner.result(second));
        } else {
            CHECK(!r.owner.result(second));
            M::Frame reply = frame(0x582, 8);
            reply.data[0] = 0x4b;
            reply.data[1] = 0x41;
            reply.data[2] = 0x60;
            reply.data[4] = 0x23;
            r.wire.push(reply, 5);
            CHECK(r.owner.service(5, budget()));
            CHECK(r.owner.result(second) && r.owner.result(second)->service.outcome == C::SdoOutcome::SUCCESS);
        }
        r.wire.final(2, true);
        CHECK(r.owner.service(6, budget()));
        const auto* result = r.owner.nmtResult(nmt);
        CHECK(result && result->affectedCount == (broadcast ? 2 : 1));
        CHECK(result->affected[0].binding.endpoint.address == 1 && result->affected[0].generation == 12);
        if (broadcast) CHECK(result->affected[1].binding.endpoint.address == 2);
        CHECK(r.wire.sentCount == 3);
    }
}
} // namespace
int main() {
    copiedExplicitIntentAndReports();
    cancellationAndUncertainEnqueue();
    finiteDeadlineAndRecovery();
    twoNodePendingSdoAndBroadcast();
    ownerPreservesConservativeReceiveBounds();
    std::printf("NMT owner: %u checks passed\n", checks);
}
