// SPDX-License-Identifier: MIT
// Exact synthetic grants test service ownership, not a documented drive capability.
#include "CanBusOwner.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (false)
C::SdoChannel route() {
    C::SdoChannel out;
    out.binding.logicalAxisId = 17;
    out.binding.endpoint.profile = 41;
    out.binding.endpoint.address = 1;
    out.binding.bindingGeneration = 3;
    out.binding.configurationGeneration = 7;
    out.clientToServerId = 0x601;
    out.serverToClientId = 0x581;
    return out;
}
C::SdoTransfer transfer(C::SdoDirection direction = C::SdoDirection::DOWNLOAD,
                         uint32_t operation = 1, uint32_t step = 1) {
    C::SdoTransfer out;
    out.channel = route();
    out.direction = direction;
    out.operation = out.request = operation;
    out.step = step;
    out.index = 0x1017;
    out.size = 2;
    out.data[0] = 0xfa;
    out.data[1] = 0x00;
    out.deadlineUs = 1000;
    out.queueTimeoutUs = out.responseTimeoutUs = 100;
    return out;
}
struct Wire {
    E::ReceivedFrame rx;
    E::TxReport report;
    M::Frame sent[4];
    uint64_t now = 0;
    unsigned count = 0;
    bool rxReady = false, reportReady = false;
    bool respond = true, accept = true;
    static bool transmit(void* p, const M::Frame& frame, E::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(p);
        CHECK(id.kind == E::TransportTxId::Kind::SDO && w.count < 4);
        w.sent[w.count++] = frame;
        if (!w.accept) return false;
        CHECK(!w.reportReady && !w.rxReady);
        w.report.id = id;
        w.report.controllerEpoch = epoch;
        w.report.transmitted = true;
        w.reportReady = true;
        if (w.respond) {
            w.rx.frame = frame;
            w.rx.frame.id = 0x581;
            w.rx.frame.data[0] = frame.data[0] == 0x40 ? 0x4b : 0x60;
            w.rx.frame.data[4] = 0xfa;
            w.rx.frame.data[5] = 0;
            w.rx.receivedTimeKnown = true;
            w.rx.receivedUs = w.now;
            w.rx.controllerEpoch = epoch;
            w.rxReady = true;
        }
        return true;
    }
    E::Transport transport() {
        E::Transport out;
        out.context = this;
        out.transmit = transmit;
        out.receive = [](void* p, E::ReceivedFrame& value) {
            Wire& w = *static_cast<Wire*>(p);
            if (!w.rxReady) return false;
            value = w.rx;
            w.rxReady = false;
            return true;
        };
        out.takeTxReport = [](void* p, E::TxReport& value) {
            Wire& w = *static_cast<Wire*>(p);
            if (!w.reportReady) return false;
            value = w.report;
            w.reportReady = false;
            return true;
        };
        out.retire = [](void* p, E::TransportTxId, uint32_t) {
            static_cast<Wire*>(p)->reportReady = false;
            return true; // No future local callback; earlier wire effects remain possible.
        };
        return out;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    E::CanBusOwner owner;
    E::SequenceId sequence;
    Rig() : owner(services, wire.transport(), 127) {
        CHECK(services.configure(0, route(), false, true));
        C::NetworkRoute network;
        network.binding = route().binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        CHECK(owner.beginSequence(route().binding, 1, 0, sequence));
        CHECK(owner.beginOperation(sequence, 1, 1000, 0));
    }
    bool grant(const C::SdoTransfer& work, uint32_t authorization = 1, uint64_t now = 0) {
        return work.direction == C::SdoDirection::DOWNLOAD
            ? services.authorizeDownloadExperiment(0, work, 11, authorization, now)
            : services.authorizeUploadExperiment(0, work, 11, authorization, now);
    }
    E::RequestId admit(const C::SdoTransfer& work, uint64_t now = 0) {
        E::BusRequest request;
        request.sequence = sequence;
        request.transfer = work;
        E::RequestId id;
        CHECK(owner.admit(request, now, id) == E::BusAdmission::ACCEPTED);
        return id;
    }
    void turn(uint64_t now) {
        wire.now = now;
        E::ServiceBudget budget;
        budget.rx = budget.events = 8;
        budget.tx = budget.starts = 2;
        CHECK(owner.service(now, budget));
    }
};
void exactCopiedGrantAndAtomicMismatch() {
    for (unsigned variant = 0; variant < 18; ++variant) {
        Rig r;
        const C::SdoTransfer exact = transfer();
        CHECK(r.grant(exact));
        C::SdoTransfer changed = exact;
        switch (variant) {
        case 0: ++changed.data[0]; break;
        case 1: ++changed.data[1]; break;
        case 2: ++changed.index; break;
        case 3: ++changed.subindex; break;
        case 4: ++changed.size; break;
        case 5: ++changed.operation; break;
        case 6: ++changed.request; break;
        case 7: ++changed.step; break;
        case 8: ++changed.deadlineUs; break;
        case 9: ++changed.queueTimeoutUs; break;
        case 10: ++changed.responseTimeoutUs; break;
        case 11: ++changed.channel.binding.configurationGeneration; break;
        case 12: ++changed.channel.binding.bindingGeneration; break;
        case 13: ++changed.channel.clientToServerId; break;
        case 14: changed.uploadSize = C::SdoUploadSize::SERVER_INDICATED; break;
        case 15: changed.allowSegmentedUpload = false; break;
        case 16: changed.direction = C::SdoDirection::UPLOAD; break;
        default: changed.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING; break;
        }
        CHECK(!r.services.checkAdmission(0, changed, 0));
        CHECK(r.services.checkAdmission(0, exact, 0)); // Invalid candidate did not consume grant.
    }
    Rig r;
    C::SdoTransfer input = transfer();
    CHECK(r.grant(input));
    const C::SdoTransfer exact = input;
    input.data[0] = 1;
    CHECK(!r.services.checkAdmission(0, input, 0) && r.services.checkAdmission(0, exact, 0));
    const auto id = r.admit(exact);
    r.turn(1);
    CHECK(r.wire.count == 1 && r.wire.sent[0].data[4] == 0xfa && r.wire.sent[0].data[5] == 0);
    r.turn(2);
    CHECK(r.owner.result(id)->service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(!r.services.checkAdmission(0, exact, 2));
    CHECK(!r.grant(exact, 2, 2)); // A new serial cannot reauthorize the same operation step.
    CHECK(r.owner.release(id));
    r.turn(3);
    CHECK(r.wire.count == 1);
}
void invalidScalarAndStoppedState() {
    Rig r;
    const C::SdoTransfer exact = transfer();
    CHECK(r.grant(exact));
    for (unsigned size : {0u, 5u, 33u}) {
        C::SdoTransfer changed = exact;
        changed.size = uint8_t(size);
        CHECK(!r.services.authorizeDownloadExperiment(0, changed, 11, 2, 0));
    }
    C::SdoTransfer changed = exact;
    changed.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
    CHECK(!r.services.authorizeDownloadExperiment(0, changed, 11, 2, 0));
    CHECK(!r.services.authorizeDownloadExperiment(0, exact, 12, 2, 0));
    CHECK(!r.services.authorizeDownloadExperiment(1, exact, 11, 2, 0));
    CHECK(r.services.checkAdmission(0, exact, 0));
    for (uint8_t state : {uint8_t(0), uint8_t(4)}) {
        Rig stopped;
        M::Frame heartbeat;
        heartbeat.id = 0x701;
        heartbeat.dlc = heartbeat.payloadLength = 1;
        heartbeat.data[0] = state;
        CHECK(stopped.services.receive(heartbeat, 1, 1));
        CHECK(!stopped.grant(exact, 1, 1));
    }
}
void cancellationConsumesNativeGrantWithoutReplay() {
    {
        Rig r;
        const auto exact = transfer();
        CHECK(r.grant(exact));
        C::SdoRequestId id;
        CHECK(r.services.admit(0, exact, 0, id));
        CHECK(r.services.cancel(id, 1)); // No handoff, no possible external effect.
        CHECK(!r.services.result(id)->effectPossible && r.services.release(id));
        CHECK(!r.services.checkAdmission(0, exact, 1) && !r.grant(exact, 2, 1));
        CHECK(r.wire.count == 0);
    }
    {
        Rig r;
        const auto exact = transfer();
        CHECK(r.grant(exact));
        const auto id = r.admit(exact);
        CHECK(r.owner.cancel(id, 1) == E::Cancel::CANCELLED);
        r.services.clearUploadExperiment(0); // Explicitly end an unused native grant.
        CHECK(r.owner.release(id));
        CHECK(!r.services.checkAdmission(0, exact, 1) && !r.grant(exact, 2, 1));
        r.turn(2);
        CHECK(r.wire.count == 0);
    }
    for (unsigned reject = 0; reject < 2; ++reject) {
        Rig r;
        r.wire.respond = false;
        r.wire.accept = reject == 0;
        const auto exact = transfer();
        CHECK(r.grant(exact));
        const auto id = r.admit(exact);
        r.turn(1);
        if (!reject) CHECK(r.owner.cancel(id, 2) == E::Cancel::CANCELLED);
        const auto* completion = r.owner.result(id);
        CHECK(completion && completion->service.effectUncertain && completion->service.transmissionPossible);
        const auto outcome = completion->service.outcome;
        CHECK(r.services.unresolved(0));
        CHECK(r.owner.release(id));
        r.turn(3);
        CHECK(r.wire.count == 1 && !r.services.checkAdmission(0, exact, 3));
        C::SdoTransfer next = transfer(C::SdoDirection::DOWNLOAD, 2);
        CHECK(!r.grant(next, 2, 3)); // Quarantine survives a new review identity.
        CHECK(outcome == (reject ? C::SdoOutcome::TRANSPORT_ERROR : C::SdoOutcome::CANCELLED));
    }
}
void readbackAndConfigurationNeverPromoteWritePermission() {
    Rig r;
    C::SdoTransfer write = transfer();
    CHECK(r.grant(write));
    const auto first = r.admit(write);
    r.turn(1);
    r.turn(2);
    CHECK(r.owner.result(first)->service.outcome == C::SdoOutcome::SUCCESS && r.owner.release(first));
    CHECK(r.owner.beginOperation(r.sequence, 2, 1000, 3));
    const auto read = transfer(C::SdoDirection::UPLOAD, 2);
    CHECK(r.grant(read, 2, 3));
    const auto second = r.admit(read, 3);
    r.turn(4);
    r.turn(5);
    CHECK(r.owner.result(second)->service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(r.owner.result(second)->service.data[0] == 0xfa);
    CHECK(!r.services.node(0)->serviceQualification.present && r.services.node(0)->sdo.present);
    C::ServiceEligibility eligibility;
    CHECK(C::serviceEligibility(*r.services.node(0), 5, eligibility) && !eligibility.ordinarySdo);
    CHECK(r.owner.release(second));
    C::SdoTransfer ordinary = transfer(C::SdoDirection::DOWNLOAD, 3);
    CHECK(!r.services.checkAdmission(0, ordinary, 5));
    M::Frame heartbeat;
    heartbeat.id = 0x701;
    heartbeat.dlc = heartbeat.payloadLength = 1;
    heartbeat.data[0] = 0x7f;
    CHECK(r.services.receive(heartbeat, 6, 6));
    CHECK(C::serviceEligibility(*r.services.node(0), 6, eligibility) && eligibility.ordinarySdo);
    CHECK(!r.services.checkAdmission(0, ordinary, 6)); // No channel confirmation from experiments.
    CHECK(r.grant(ordinary, 3, 6)); // An armed old-configuration grant must not migrate.
    M::Binding next = route().binding;
    ++next.configurationGeneration;
    CHECK(r.owner.advanceConfiguration(r.sequence, next, 7));
    CHECK(r.services.node(0)->route.generation == 11 && r.services.node(0)->nmtTime.receivedUs == 6);
    CHECK(!r.services.checkAdmission(0, ordinary, 7));
    ordinary.channel.binding = next;
    CHECK(!r.services.checkAdmission(0, ordinary, 7));
    r.services.clearUploadExperiment(0);
    CHECK(!r.services.checkAdmission(0, ordinary, 7));
}
void preOperationalPrerequisiteCannotBeStrippedOrAssumed() {
    {
        C::SdoClient unanchored;
        CHECK(unanchored.configure(0, route(), true, true));
        C::SdoTransfer exact = transfer();
        CHECK(unanchored.checkAdmission(0, exact, 0));
        exact.requirePreOperational = true;
        CHECK(!unanchored.checkAdmission(0, exact, 0));
    }
    {
        Rig r;
        M::Frame heartbeat;
        heartbeat.id = 0x701;
        heartbeat.dlc = heartbeat.payloadLength = 1;
        heartbeat.data[0] = 0x7f;
        CHECK(r.services.receive(heartbeat, 0, 0));
        C::SdoTransfer exact = transfer();
        exact.requirePreOperational = true;
        CHECK(r.grant(exact));
        C::SdoTransfer stripped = exact;
        stripped.requirePreOperational = false;
        CHECK(!r.services.checkAdmission(0, stripped, 0));
        CHECK(r.services.checkAdmission(0, exact, 0));
        C::SdoRequestId id;
        CHECK(r.services.admit(0, exact, 0, id));
        CHECK(r.services.service(1));
        heartbeat.data[0] = 5;
        CHECK(r.services.receive(heartbeat, 2, 2));
        M::Frame noTx;
        C::SdoTxId noId;
        CHECK(!r.services.nextTx(0, 3, noTx, noId));
        CHECK(r.services.result(id) && !r.services.result(id)->transmissionPossible);
        CHECK(r.services.result(id)->outcome == C::SdoOutcome::BINDING_INVALIDATED);
    }
    {
        Rig r;
        C::SdoTransfer exact = transfer();
        exact.requirePreOperational = true;
        CHECK(!r.services.checkAdmission(0, exact, 0)); // Unknown is insufficient.
        M::Frame heartbeat;
        heartbeat.id = 0x701;
        heartbeat.dlc = heartbeat.payloadLength = 1;
        heartbeat.data[0] = 0x7f;
        CHECK(r.services.receive(heartbeat, 0, 0));
        CHECK(r.grant(exact));
        C::SdoRequestId id;
        CHECK(r.services.admit(0, exact, 0, id));
        M::Frame copied;
        C::SdoTxId handedOff;
        CHECK(r.services.nextTx(0, 1, copied, handedOff));
        CHECK(r.services.canTransmit(handedOff, 1));
        heartbeat.data[0] = 5;
        CHECK(r.services.receive(heartbeat, 2, 2));
        CHECK(!r.services.canTransmit(handedOff, 2)); // Exact copied TX remains subject to premise.
    }
}
void lifecycleRevokesUnusedGrant() {
    Rig r;
    const auto exact = transfer();
    CHECK(r.grant(exact));
    CHECK(r.services.reboot(1, 1));
    CHECK(r.services.node(0)->route.generation == 12 && r.services.unresolved(0));
    CHECK(!r.services.checkAdmission(0, exact, 1));
    CHECK(!r.services.authorizeDownloadExperiment(0, exact, 12, 2, 1));
    r.turn(2);
    CHECK(r.wire.count == 0);
}
} // namespace
int main() {
    exactCopiedGrantAndAtomicMismatch();
    invalidScalarAndStoppedState();
    cancellationConsumesNativeGrantWithoutReplay();
    readbackAndConfigurationNeverPromoteWritePermission();
    lifecycleRevokesUnusedGrant();
    preOperationalPrerequisiteCannotBeStrippedOrAssumed();
    std::printf("Service experiments: %u checks passed\n", checks);
}
