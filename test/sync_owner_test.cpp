// SPDX-License-Identifier: MIT
// Host-only SYNC admission/retirement evidence; no device behavior is inferred.
#include "CanBusOwner.h"
#include <cstdio>
#include <cstdlib>
namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace O = MotorControlCANOwner;
static unsigned checks = 0;
#define CHECK(c) do { ++checks; if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (false)
#include "support/OwnerRig.h"
namespace {
struct Wire : OwnerRig::WireBase {
    O::RecoveryProgress recovery = O::RecoveryProgress::PENDING;
    bool hold = false, reenter = false;
    O::CanBusOwner* owner = nullptr;
    O::Transport transport() {
        auto value = plumbing();
        value.transmit = [](void* p, const M::Frame& f, O::TransportTxId id, uint32_t epoch) {
            auto& wire = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(p));
            if (wire.reenter) {
                CHECK(!wire.owner->disarmSync(0));
                CHECK(!wire.owner->cancelSync(id.sync, 0));
                CHECK(!wire.owner->service(0, O::ServiceBudget()));
            }
            if (wire.hold && id.kind == O::TransportTxId::Kind::SYNC)
                CHECK(wire.owner->holdSyncHandoff(id.sync));
            return OwnerRig::WireBase::transmit(p, f, id, epoch);
        };
        value.beginRecovery = [](void*, uint32_t) { return true; };
        value.pollRecovery = [](void* p, uint32_t) {
            return static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(p))->recovery;
        };
        return value;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    explicit Rig(uint32_t seed = 0, bool collision = false, uint32_t heartbeatId = 0x701)
        : owner(services, wire.transport(), 51, seed) {
        wire.owner = &owner;
        auto route = OwnerRig::route();
        if (collision) route.clientToServerId = 0x80;
        CHECK(services.configure(0, route, true, true));
        C::NetworkRoute network;
        network.binding = OwnerRig::route().binding;
        network.generation = 11;
        network.heartbeatId = heartbeatId;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    }
    C::SyncWork work(uint64_t deadline = 100) {
        C::SyncWork value;
        value.available = true;
        value.generation = 4;
        value.expiresUs = deadline;
        CHECK(C::makeSyncRequest(0x80, value.frame));
        return value;
    }
    O::SyncRequestId admit(uint64_t deadline = 100) {
        CHECK(owner.armSync(0x80, owner.lastTimeUs()));
        O::SyncRequestId id;
        CHECK(owner.admitSync(work(deadline), owner.lastTimeUs(), id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    void turn(uint64_t now, uint8_t tx = 2, uint8_t events = 8) {
        CHECK(owner.service(now, OwnerRig::budget(8, tx, events, tx, 1)));
    }
    O::BusRequest ordinary(O::SequenceId sequence, uint32_t step) {
        O::BusRequest value;
        value.sequence = sequence;
        value.transfer.channel = OwnerRig::route();
        value.transfer.operation = value.transfer.request = 1;
        value.transfer.step = step;
        value.transfer.index = 0x6041;
        value.transfer.size = 2;
        value.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        value.transfer.queueTimeoutUs = value.transfer.responseTimeoutUs = 100;
        value.transfer.deadlineUs = 1000;
        return value;
    }
};
void independentCapacityAndFirstTerminal() {
    Rig r;
    O::SequenceId sequence;
    CHECK(r.owner.beginSequence(OwnerRig::route().binding, 1, 0, sequence));
    CHECK(r.owner.beginOperation(sequence, 1, 1000, 0));
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        O::RequestId id;
        CHECK(r.owner.admit(r.ordinary(sequence, i + 1), 0, id) == O::BusAdmission::ACCEPTED);
    }
    const auto id = r.admit();
    O::SyncRequestId unchanged;
    unchanged.generation = 78;
    CHECK(r.owner.admitSync(r.work(), 0, unchanged) == O::BusAdmission::RESULTS_FULL);
    CHECK(unchanged.generation == 78);
    r.turn(0, 1);
    CHECK(r.wire.sentCount == 1 && r.wire.ids[0].kind == O::TransportTxId::Kind::SYNC);
    CHECK(!r.owner.syncResult(id));
    r.turn(1, 0);
    const auto* result = r.owner.syncResult(id);
    CHECK(result && result->service.outcome == C::SyncOutcome::LOCAL_TX);
    CHECK(result->service.transmissionConfirmed && result->service.transmissionPossible);
    CHECK(!r.owner.cancelSync(id, 2));
    CHECK(r.owner.syncResult(id)->completedUs == 1);
    CHECK(r.owner.stats().txFrames == 1);
    CHECK(r.owner.releaseSync(id));
    CHECK(!r.owner.releaseSync(id));
    CHECK(r.owner.resources().queued == O::CanBusOwner::REQUESTS);
}
void deadlineAndRetirement() {
    Rig r;
    r.wire.autoReport = false;
    r.wire.retirement = false;
    const auto id = r.admit(5);
    r.turn(0);
    CHECK(r.wire.sentCount == 1);
    r.wire.report(0);
    r.turn(5, 0, 0);
    CHECK(r.owner.syncResult(id)->service.outcome == C::SyncOutcome::MISSED_DEADLINE);
    CHECK(r.owner.syncResult(id)->service.transmissionPossible);
    CHECK(!r.owner.syncResult(id)->service.transmissionConfirmed);
    CHECK(!r.owner.syncArmed() && r.owner.resources().syncTxPending);
    CHECK(r.owner.releaseSync(id));
    CHECK(!r.owner.armSync(0x80, 5));
    r.turn(6, 0);
    CHECK(!r.owner.resources().syncTxPending && r.owner.stats().txFrames == 1);
    CHECK(r.owner.armSync(0x80, 6));
    Rig before;
    const auto never = before.admit(5);
    CHECK(!before.owner.cancelSync(never, 5));
    CHECK(before.owner.syncResult(never)->service.outcome == C::SyncOutcome::MISSED_DEADLINE);
    CHECK(!before.owner.syncResult(never)->service.transmissionPossible);
    CHECK(before.wire.sentCount == 0);
}
void conflictBoundsAndResolution() {
    Rig r;
    const auto id = r.admit();
    auto f = OwnerRig::frame(0x80, 1);
    f.data[0] = 7;
    r.wire.push(f, 0);
    auto& ingress = r.wire.incoming[0];
    ingress.receivedTimeKnown = false;
    ingress.receiveBoundsKnown = true;
    ingress.receivedEarliestUs = 0;
    ingress.receivedLatestUs = 2;
    r.turn(2);
    CHECK(r.owner.syncConflict() && !r.owner.syncArmed() && r.wire.sentCount == 0);
    CHECK(r.owner.syncResult(id)->service.outcome == C::SyncOutcome::PRODUCER_CONFLICT);
    CHECK(r.owner.syncObservation().time.receiveBoundsKnown);
    CHECK(!r.owner.syncObservation().time.receivedTimeKnown);
    CHECK(r.owner.syncObservation().time.receivedLatestUs == 2);
    CHECK(!r.owner.resolveSyncConflict(1, 2));
    CHECK(r.owner.releaseSync(id));
    CHECK(!r.owner.armSync(0x80, 2));
    CHECK(!r.owner.resolveSyncConflict(0, 2));
    CHECK(r.owner.resolveSyncConflict(51, 2));
    CHECK(!r.owner.syncArmed());
    CHECK(r.owner.armSync(0x80, 2));
    Rig stale;
    const auto live = stale.admit();
    stale.wire.push(OwnerRig::frame(0x80, 0), 2, 99);
    stale.wire.push(OwnerRig::frame(0x80, 0), 99);
    stale.turn(2);
    CHECK(!stale.owner.syncConflict() && stale.wire.sentCount == 1);
    CHECK(!stale.owner.syncResult(live));
}
void busOffRecoveryDoesNotRearm() {
    Rig r;
    r.wire.autoReport = false;
    r.wire.retirement = false;
    const auto id = r.admit();
    r.turn(0);
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 1));
    CHECK(r.owner.syncResult(id)->service.outcome == C::SyncOutcome::BUS_OFF);
    CHECK(!r.owner.syncArmed());
    CHECK(r.owner.releaseSync(id));
    O::RecoveryId recovery;
    CHECK(r.owner.recover(2, 20, recovery));
    r.wire.recovery = O::RecoveryProgress::RETIRED;
    r.turn(3, 0);
    CHECK(r.owner.recoveryResult(recovery));
    CHECK(!r.owner.syncArmed() && !r.owner.resources().syncTxPending);
    O::SyncRequestId output;
    CHECK(r.owner.admitSync(r.work(), 3, output) == O::BusAdmission::INHIBITED);
    CHECK(r.owner.armSync(0x80, 3));
    CHECK(r.owner.receiveOverflow(4));
    CHECK(!r.owner.syncArmed());
}
void heldUnsentAndFailure() {
    Rig r;
    r.wire.hold = true;
    r.wire.autoReport = false;
    const auto id = r.admit();
    r.turn(0);
    CHECK(!r.owner.holdSyncHandoff(id)); // Only the actual enqueue callback can establish this fact.
    CHECK(r.owner.canTransmitSync(id, 1));
    CHECK(r.owner.noteSyncSubmitted(id, 1));
    CHECK(!r.owner.noteSyncSubmitted(id, 1));
    CHECK(r.owner.cancelSync(id, 2));
    CHECK(r.owner.syncResult(id)->service.transmissionPossible);
    Rig unsent;
    unsent.wire.hold = true;
    unsent.wire.autoReport = false;
    const auto held = unsent.admit(3);
    unsent.turn(0);
    unsent.turn(3, 0);
    CHECK(!unsent.owner.syncResult(held)->service.transmissionPossible);
    CHECK(!unsent.owner.noteSyncSubmitted(held, 3));
    Rig refused;
    refused.wire.accept = false;
    refused.wire.autoReport = false;
    const auto failed = refused.admit();
    refused.turn(0);
    CHECK(refused.owner.syncResult(failed)->service.outcome == C::SyncOutcome::TRANSPORT_ERROR);
    CHECK(refused.owner.syncResult(failed)->service.transmissionPossible);
    CHECK(refused.owner.stats().txRejected == 1);
}
void admissionAndMalformedIngressStayAtomic() {
    Rig r;
    O::SyncRequestId output;
    output.generation = 991;
    CHECK(r.owner.admitSync(r.work(), 0, output) == O::BusAdmission::INHIBITED);
    CHECK(output.generation == 991 && !r.owner.resources().syncPending);
    CHECK(!r.owner.armSync(0x40000080, 0));
    CHECK(r.owner.armSync(0x80, 0));
    auto work = r.work();
    work.frame = OwnerRig::frame(0x80, 1);
    CHECK(r.owner.admitSync(work, 0, output) == O::BusAdmission::INVALID);
    work = r.work();
    work.dueUs = 1;
    CHECK(r.owner.admitSync(work, 0, output) == O::BusAdmission::INVALID);
    work = r.work(1);
    CHECK(r.owner.admitSync(work, 1, output) == O::BusAdmission::EXPIRED);
    CHECK(output.generation == 991 && r.wire.sentCount == 0);
    auto malformed = OwnerRig::frame(0x80, 2);
    r.wire.push(malformed, 0);
    malformed = OwnerRig::frame(0x80, 0);
    malformed.rtr = true;
    r.wire.push(malformed, 0);
    malformed.rtr = false;
    malformed.extended = true;
    r.wire.push(malformed, 0);
    r.turn(0, 0);
    CHECK(!r.owner.syncConflict() && !r.owner.syncObservation().present);
    r.wire.push(OwnerRig::frame(0x80, 0), 0);
    auto& unknown = r.wire.incoming[r.wire.rxWrite - 1];
    unknown.receivedTimeKnown = false;
    r.turn(1, 0);
    CHECK(r.owner.syncConflict() && r.owner.syncObservation().present);
    CHECK(!r.owner.syncObservation().time.receivedTimeKnown &&
          !r.owner.syncObservation().time.receiveBoundsKnown);
    O::RecoveryId recovery;
    CHECK(r.owner.recover(2, 9, recovery));
    r.wire.recovery = O::RecoveryProgress::RETIRED;
    r.turn(3, 0);
    CHECK(r.owner.syncConflict() && !r.owner.syncArmed());
    Rig collision(0, true);
    CHECK(!collision.owner.armSync(0x80, 0));
}
void explicitArmEpochPreservesLifecycleBoundaries() {
    Rig r;
    CHECK(r.owner.syncArmGeneration() == 0 && !r.owner.syncArmed());
    CHECK(r.owner.armSync(0x80, 0));
    CHECK(r.owner.syncArmGeneration() == 1);
    CHECK(r.owner.disarmSync(1));
    CHECK(!r.owner.syncArmed() && r.owner.syncDisarmOutcome() == C::SyncOutcome::CANCELLED);
    CHECK(r.owner.armSync(0x80, 1));
    CHECK(r.owner.syncArmGeneration() == 2); // An older finite helper cannot resume unnoticed.
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, 2));
    CHECK(!r.owner.syncArmed() && r.owner.syncDisarmOutcome() == C::SyncOutcome::BUS_OFF);
    CHECK(!r.owner.armSync(0x80, 2) && r.owner.syncArmGeneration() == 2);
    O::RecoveryId recovery;
    CHECK(r.owner.recover(3, 9, recovery));
    r.wire.recovery = O::RecoveryProgress::RETIRED;
    r.turn(4, 0);
    CHECK(!r.owner.syncArmed() && r.owner.syncArmGeneration() == 2);
    CHECK(r.owner.armSync(0x80, 4) && r.owner.syncArmGeneration() == 3);
}
void urgentPriorityAndValidation() {
    Rig r;
    O::SequenceId sequence;
    CHECK(r.owner.beginSequence(OwnerRig::route().binding, 1, 0, sequence));
    CHECK(r.owner.beginOperation(sequence, 1, 1000, 0));
    O::RequestId urgent;
    CHECK(r.owner.admitUrgent(r.ordinary(sequence, 1), 0, urgent) == O::BusAdmission::ACCEPTED);
    const auto id = r.admit();
    CHECK(!r.owner.syncTxPriorityReady());
    r.turn(0, 1);
    CHECK(r.wire.sentCount == 1 && r.wire.ids[0].kind == O::TransportTxId::Kind::SDO);
    r.turn(1, 1);
    CHECK(r.wire.sentCount == 1 && !r.owner.syncResult(id));
    CHECK(!r.owner.armSync(0x81, 1));
    CHECK(!r.owner.cancelSync(id, 0));
    CHECK(r.owner.disarmSync(2));
    CHECK(r.owner.syncResult(id)->service.outcome == C::SyncOutcome::CANCELLED);
    Rig exhausted(UINT32_MAX);
    CHECK(exhausted.owner.armSync(0x80, 0));
    O::SyncRequestId output;
    CHECK(exhausted.owner.admitSync(exhausted.work(), 0, output) == O::BusAdmission::IDS_EXHAUSTED);
    Rig reentrant;
    reentrant.wire.reenter = true;
    reentrant.admit();
    reentrant.turn(0);
}
void passiveSyncPreservesConfiguredBootupRouting() {
    Rig r(0, false, 0x80);
    O::SequenceId sequence;
    CHECK(r.owner.beginSequence(OwnerRig::route().binding, 1, 0, sequence));
    CHECK(r.owner.beginOperation(sequence, 1, 1000, 0));
    O::RequestId queued;
    CHECK(r.owner.admit(r.ordinary(sequence, 1), 0, queued) == O::BusAdmission::ACCEPTED);
    CHECK(!r.owner.armSync(0x80, 0)); // Existing custom routes preclude local production.
    r.wire.push(OwnerRig::frame(0x80, 1), 1); // Configured boot-up before any handoff.
    r.turn(1);
    CHECK(r.owner.syncConflict() && r.owner.syncObservation().present);
    CHECK(r.services.node(0)->route.generation > 11);
    CHECK(!r.owner.sequence(sequence));
    const auto* terminal = r.owner.result(queued);
    CHECK(terminal && terminal->reason == O::CompletionReason::INVALIDATED);
    CHECK(!terminal->service.transmissionPossible && r.wire.sentCount == 0);
}
void passiveSyncPreservesConfiguredPdoRouting() {
    Rig r;
    // Synthetic one-byte profile: this checks routing, not a Lichuan map claim.
    const C::PdoObject object(0x2000, 0, 1, C::PdoSign::UNSIGNED, C::PdoRole::RAW, false, true);
    C::PdoProfile profile;
    profile.profile = 41;
    profile.sets = profile.slots = 1;
    profile.objects = &object;
    profile.objectCount = 1;
    C::PdoMap map;
    map.number = map.count = 1;
    map.words[0] = 0x20000008;
    C::PdoMapEvidence evidence;
    evidence.route.binding = OwnerRig::route().binding;
    evidence.route.generation = 1;
    evidence.route.cobId = 0x80;
    CHECK(C::validatePdoMap(map, profile, evidence.route.layout));
    evidence.nodeGeneration = 11;
    evidence.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
    evidence.evidenceId = 51;
    evidence.communicationKnown = true;
    evidence.cobIdParameter = 0x80;
    evidence.transmissionType = 255;
    CHECK(r.services.configurePdo(0, 0, evidence, profile, C::PdoObserver(), 0));
    C::PdoQualification qualification;
    qualification.route = evidence.route;
    qualification.nodeGeneration = 11;
    qualification.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
    qualification.evidenceId = 52;
    CHECK(r.services.qualifyPdo(0, qualification, 0));
    CHECK(!r.owner.armSync(0x80, 0));
    auto sample = OwnerRig::frame(0x80, 1);
    sample.data[0] = 42;
    r.wire.push(sample, 1);
    r.turn(1);
    CHECK(r.owner.syncConflict() && !r.owner.syncArmed());
    const auto* record = r.services.pdo(0);
    CHECK(record && record->qualified && record->observation.time.present);
    CHECK(record->observation.sample.values[0].native == 42);
    CHECK(record->observation.time.receivedUs == 1 && record->observation.time.processedUs == 1);
    CHECK(r.owner.stats().rxFrames == 1 && r.owner.stats().unmatchedFrames == 0);
}
void foreignSyncConflictPreservesOrdinaryTraffic() {
    Rig r;
    O::SequenceId sequence;
    CHECK(r.owner.beginSequence(OwnerRig::route().binding, 1, 0, sequence));
    CHECK(r.owner.beginOperation(sequence, 1, 1000, 0));
    O::RequestId ordinary;
    CHECK(r.owner.admit(r.ordinary(sequence, 1), 0, ordinary) == O::BusAdmission::ACCEPTED);
    const auto sync = r.admit();
    auto heartbeat = OwnerRig::frame(0x701, 1);
    heartbeat.data[0] = 5;
    r.wire.push(heartbeat, 1);
    r.wire.push(OwnerRig::frame(0x80, 0), 1);
    r.turn(1);
    CHECK(r.services.node(0)->heartbeat.present && r.services.node(0)->nmt == C::NmtState::OPERATIONAL);
    CHECK(r.owner.syncConflict() && !r.owner.syncArmed());
    CHECK(r.owner.syncResult(sync)->service.outcome == C::SyncOutcome::PRODUCER_CONFLICT);
    CHECK(!r.owner.syncResult(sync)->service.transmissionPossible);
    CHECK(r.wire.sentCount == 1 && r.wire.ids[0].kind == O::TransportTxId::Kind::SDO);
    CHECK(r.owner.sequence(sequence) && !r.owner.result(ordinary));
    CHECK(r.owner.stats().rxFrames == 2 && r.owner.stats().unmatchedFrames == 0);
}
}
int main() {
    independentCapacityAndFirstTerminal();
    deadlineAndRetirement();
    conflictBoundsAndResolution();
    busOffRecoveryDoesNotRearm();
    heldUnsentAndFailure();
    urgentPriorityAndValidation();
    admissionAndMalformedIngressStayAtomic();
    explicitArmEpochPreservesLifecycleBoundaries();
    passiveSyncPreservesConfiguredBootupRouting();
    passiveSyncPreservesConfiguredPdoRouting();
    foreignSyncConflictPreservesOrdinaryTraffic();
    std::printf("sync owner: %u checks\n", checks);
}
