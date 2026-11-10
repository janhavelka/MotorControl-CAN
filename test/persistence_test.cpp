// SPDX-License-Identifier: MIT
#include "CanBusOwner.h"
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/Persistence.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& v) { std::memcpy(bytes, &v, sizeof v); }
    void unchanged(const T& v) const { CHECK(!std::memcmp(bytes, &v, sizeof v)); }
};
} // namespace
#include "support/OwnerRig.h"
namespace {
L::ReadTarget target() {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = 101;
    t.channel.binding.endpoint.profile = 41;
    t.channel.binding.endpoint.address = 1;
    t.channel.binding.bindingGeneration = 3;
    t.channel.binding.configurationGeneration = 7;
    t.channel.clientToServerId = 0x601;
    t.channel.serverToClientId = 0x581;
    t.nodeGeneration = 11;
    return t;
}
L::ReadOptions options() {
    L::ReadOptions o;
    o.queueTimeoutUs = o.responseTimeoutUs = 100;
    return o;
}
C::SdoCompletion terminal(const C::SdoTransfer& t, M::Generation generation, uint64_t at,
                          uint32_t raw = 0, C::SdoOutcome outcome = C::SdoOutcome::SUCCESS,
                          bool possible = true) {
    C::SdoCompletion e;
    e.transfer = t;
    e.nodeGeneration = generation;
    e.admittedUs = at;
    e.completedUs = at + 1;
    e.outcome = outcome;
    e.queued = e.transmissionPossible = e.transmissionConfirmed = e.effectPossible = possible;
    e.protocolAcknowledged = outcome == C::SdoOutcome::SUCCESS;
    e.effectUncertain = possible && !e.protocolAcknowledged;
    if (e.protocolAcknowledged && t.direction == C::SdoDirection::UPLOAD) {
        e.size = t.size;
        for (unsigned i = 0; i < t.size; ++i)
            e.data[i] = uint8_t(raw >> (i * 8));
    }
    if (outcome == C::SdoOutcome::SERVER_ABORT) {
        e.abortValid = true;
        e.abortCode = 0xf1234567;
    }
    return e;
}
void complete(L::ReadContext& read, uint64_t& now, uint32_t speed = 500, uint16_t monitor = 0) {
    while (read.state == M::ReadState::ACTIVE) {
        L::PreparedRead p;
        CHECK(L::nextRead(read, now, p));
        CHECK(p.ready && p.transfer.direction == C::SdoDirection::UPLOAD &&
              p.transfer.index != 0x2300);
        uint32_t raw = 0;
        switch (p.transfer.index) {
        case 0x1000:
            raw = 0x00040192;
            break;
        case 0x1009:
            raw = 0x0c;
            break;
        case 0x100a:
            raw = 0x20;
            break;
        case 0x1018:
            raw = p.transfer.subindex == 0 ? 3 : p.transfer.subindex;
            break;
        case 0x2000:
            raw = 0x400;
            break;
        case 0x2001:
            raw = 0x200;
            break;
        case 0x2100:
            raw = monitor;
            break;
        case 0x6041:
            raw = 0x40;
            break;
        case 0x6061:
            raw = 1;
            break;
        case 0x6081:
            raw = speed;
            break;
        case 0x6083:
            raw = 200;
            break;
        case 0x6084:
            raw = 300;
            break;
        }
        const auto e = terminal(p.transfer, read.target.nodeGeneration, now, raw);
        now += 2;
        CHECK(L::advanceRead(read, e, now));
    }
    CHECK(read.state == M::ReadState::SUCCEEDED);
}
L::PersistencePrerequisites observations(const L::ReadTarget& t, uint64_t& now,
                                         uint32_t speed = 500, uint16_t monitor = 0) {
    L::PersistencePrerequisites p;
    const auto o = options();
    CHECK(L::prepareIdentity(p.identity, t, 2, now, now + 10000, o));
    complete(p.identity, now);
    CHECK(L::prepareState(p.state, t, 3, now, now + 10000, o,
                          uint8_t((1u << unsigned(L::StateBlock::STATUS)) |
                                  (1u << unsigned(L::StateBlock::MONITORS)))));
    complete(p.state, now, speed, monitor);
    CHECK(L::prepareDriverRead(p.configuration, t, 4, now, now + 10000, o,
                               L::DriverGroup::MOTION_PARAMETERS));
    complete(p.configuration.read, now, speed);
    return p;
}
struct Fixture {
    L::AxisReservation reservation;
    L::PersistenceReview review;
    L::PersistencePrerequisites prior;
    L::PersistenceContext context;
    M::Binding binding;
    uint64_t now = 10;
    Fixture() {
        reservation.target = target();
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = 73;
        reservation.sequence = 1;
        reservation.generation = 5;
        binding = reservation.target.channel.binding;
        review.target = reservation.target;
        review.fixtureRecord = 1;
        review.firmwareRecord = 2;
        review.procedureRecord = 3;
        review.authorizationRecord = 4;
        prior = observations(reservation.target, now);
    }
    M::Status prepare(bool factory = false, uint64_t age = 1000) {
        review.kind = factory ? L::PersistenceKind::FACTORY_RESTORE : L::PersistenceKind::SAVE;
        return factory ? L::prepareFactoryRestore(context, reservation, reservation.target, review,
                                                  prior, 9, 7, now, now + 10000, options(), age)
                       : L::prepareSave(context, reservation, reservation.target, review, prior, 9,
                                        7, now, now + 10000, options(), age);
    }
    L::PreparedRead next() {
        L::PreparedRead p;
        CHECK(L::nextPersistence(context, reservation, binding, now, p));
        return p;
    }
    void finish(C::SdoOutcome outcome = C::SdoOutcome::SUCCESS) {
        const auto p = next();
        CHECK(p.ready);
        CHECK(L::notePersistenceTransmission(context, reservation, binding, now));
        CHECK(!next().ready);
        auto e = terminal(p.transfer, reservation.target.nodeGeneration, now, 0, outcome);
        now += 2;
        CHECK(L::advancePersistence(context, reservation, binding, e, now));
    }
    L::PersistenceVerification verification(bool power = false, uint32_t speed = 500) {
        L::PersistenceVerification v;
        v.current = reservation.target;
        v.current.channel.binding = binding;
        v.model = reservation.model;
        v.fixtureRecord = review.fixtureRecord;
        v.firmwareRecord = review.firmwareRecord;
        v.routeRecord = 8;
        v.maximumAgeUs = 1000;
        if (power) {
            v.powerCycleObserved = true;
            v.powerCycleUs = ++now;
            v.powerCycleRecord = 10;
            ++v.current.nodeGeneration;
            v.powerCycleTarget = v.current;
            v.noInterveningWritesRecord = 11;
        }
        ++now;
        v.observations = observations(v.current, now, speed);
        return v;
    }
};
void commandsAndKnowledge() {
    for (unsigned factory = 0; factory < 2; ++factory) {
        Fixture f;
        f.review.backupRecord = 20;
        f.review.routeBackRecord = 21;
        CHECK(f.prepare(factory != 0));
        auto p = f.next();
        CHECK(p.ready && p.transfer.index == 0x2300 && !p.transfer.subindex &&
              p.transfer.size == 2);
        CHECK(p.transfer.direction == C::SdoDirection::DOWNLOAD);
        CHECK(p.transfer.data[0] == (factory ? 1 : 2) && p.transfer.data[1] == 0);
        CHECK(f.context.attempts == 0 && f.context.attemptOrdinal == 7);
        const Saved<L::PersistenceRecord> before(f.context.before);
        f.finish();
        CHECK(f.context.state == L::PersistenceState::ACKNOWLEDGED);
        CHECK(f.context.execution == M::ActionExecution::ACKNOWLEDGED && !f.context.uncertain);
        CHECK(f.context.attempts == 1 && f.binding.configurationGeneration == 8 && !f.next().ready);
        CHECK(!f.context.survivedRestart &&
              f.context.restart == L::PersistenceRequirement::UNKNOWN);
        CHECK(f.context.rearm == L::PersistenceRequirement::UNKNOWN &&
              f.context.completeSavedSet == L::PersistenceRequirement::UNKNOWN);
        auto v = f.verification();
        CHECK(L::verifyPersistence(f.context, v, f.now));
        CHECK(f.context.matchingFields == 7 && !f.context.survivedRestart);
        v = f.verification(true, 501);
        CHECK(L::verifyPersistence(f.context, v, f.now));
        CHECK(f.context.matchingFields == 6 && f.context.survivedRestart == (factory ? 0 : 6));
        CHECK(f.context.outcome == M::ReadOutcome::SUCCESS && !f.next().ready);
        before.unchanged(f.context.before);
    }
}
void lostReplyDoesNotBecomeSuccess() {
    Fixture f;
    CHECK(f.prepare());
    f.finish(C::SdoOutcome::TIMEOUT);
    CHECK(f.context.state == L::PersistenceState::FAILED && f.context.uncertain);
    CHECK(f.context.execution == M::ActionExecution::UNKNOWN && f.context.effectsPossible);
    const Saved<C::SdoCompletion> write(f.context.writeEvidence);
    const auto v = f.verification(true);
    CHECK(L::verifyPersistence(f.context, v, f.now));
    CHECK(f.context.survivedRestart == 7 && f.context.uncertain && !f.next().ready);
    CHECK(f.context.state == L::PersistenceState::FAILED &&
          f.context.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
    write.unchanged(f.context.writeEvidence);
    const Saved<L::PersistenceContext> saved(f.context);
    CHECK(!L::advancePersistence(f.context, M::ReadOutcome::CANCELLED, f.now));
    saved.unchanged(f.context);
    Fixture aborted;
    CHECK(aborted.prepare());
    aborted.finish(C::SdoOutcome::SERVER_ABORT);
    CHECK(aborted.context.execution == M::ActionExecution::REJECTED && aborted.context.uncertain);
    CHECK(aborted.context.writeEvidence.abortValid &&
          aborted.context.writeEvidence.abortCode == 0xf1234567);
}
void rejectedCandidates() {
    for (unsigned scenario = 0; scenario < 12; ++scenario) {
        Fixture f;
        const Saved<L::PersistenceContext> saved(f.context);
        switch (scenario) {
        case 0:
            f.review.procedureRecord = 0;
            break;
        case 1:
            f.review.firmwareRecord = 0;
            break;
        case 2:
            ++f.review.target.nodeGeneration;
            break;
        case 3:
            f.reservation.model = L::SettingsModel::UNKNOWN;
            break;
        case 4:
            f.reservation.acquiredUs = f.now;
            break;
        case 5:
            f.prior.identity.uncertain = true;
            break;
        case 6:
            f.prior.configuration.read.state = M::ReadState::FAILED;
            break;
        case 7:
            f.prior.state.uncertain = true;
            break;
        case 8:
            ++f.prior.configuration.read.target.nodeGeneration;
            break;
        case 9:
            f.prior = observations(f.reservation.target, f.now, 500, 4);
            break;
        case 10:
            f.prior = observations(f.reservation.target, f.now, 500, 0x8000);
            break;
        case 11:
            f.now += 1001;
            break;
        }
        CHECK(!f.prepare());
        saved.unchanged(f.context);
    }
    Fixture factory;
    CHECK(!factory.prepare(true));
    factory.review.backupRecord = 1;
    CHECK(!factory.prepare(true));
    factory.review.routeBackRecord = 2;
    CHECK(factory.prepare(true));
    Fixture overlap;
    CHECK(overlap.prepare());
    const Saved<L::PersistenceContext> saved(overlap.context);
    CHECK(!L::prepareSave(overlap.context, overlap.context.reservation, overlap.reservation.target,
                          overlap.review, overlap.prior, 8, 1, overlap.now, 10000, options(),
                          1000));
    saved.unchanged(overlap.context);
}
void invalidNativeEvidenceAndEpochs() {
    for (unsigned scenario = 0; scenario < 10; ++scenario) {
        Fixture f;
        CHECK(f.prepare());
        const auto p = f.next();
        auto e = terminal(p.transfer, f.reservation.target.nodeGeneration, f.now);
        if (scenario != 0)
            CHECK(L::notePersistenceTransmission(f.context, f.reservation, f.binding, f.now));
        switch (scenario) {
        case 1:
            e.transfer.data[0] = 0;
            break;
        case 2:
            e.transfer.size = 4;
            break;
        case 3:
            ++e.transfer.step;
            break;
        case 4:
            ++e.nodeGeneration;
            break;
        case 5:
            e.protocolAcknowledged = false;
            break;
        case 6:
            e.size = 2;
            break;
        case 7:
            e.effectPossible = false;
            break;
        case 8:
            ++f.reservation.generation;
            break;
        case 9:
            --f.binding.configurationGeneration;
            break;
        }
        const Saved<L::PersistenceContext> saved(f.context);
        CHECK(!L::advancePersistence(f.context, f.reservation, f.binding, e, f.now + 2));
        saved.unchanged(f.context);
    }
    Fixture f;
    CHECK(f.prepare());
    const auto p = f.next();
    CHECK(L::notePersistenceTransmission(f.context, f.reservation, f.binding, f.now));
    CHECK(!L::notePersistenceTransmission(f.context, f.reservation, f.binding, f.now));
    auto e = terminal(p.transfer, f.reservation.target.nodeGeneration, f.now);
    CHECK(L::advancePersistence(f.context, f.reservation, f.binding, e, f.context.deadlineUs));
    CHECK(f.context.state == L::PersistenceState::FAILED &&
          f.context.outcome == M::ReadOutcome::DEADLINE);
    CHECK(f.context.execution == M::ActionExecution::ACKNOWLEDGED);
    Fixture cancelled;
    CHECK(cancelled.prepare());
    CHECK(L::advancePersistence(cancelled.context, M::ReadOutcome::CANCELLED, cancelled.now));
    CHECK(!cancelled.context.effectsPossible && !cancelled.context.attempts);
}
void reconciliationRejectsStaleOrInventedEvidence() {
    for (unsigned scenario = 0; scenario < 11; ++scenario) {
        Fixture f;
        CHECK(f.prepare());
        f.finish();
        auto v = f.verification(true);
        switch (scenario) {
        case 0:
            v.routeRecord = 0;
            break;
        case 1:
            v.powerCycleRecord = 0;
            break;
        case 2:
            v.current.nodeGeneration = f.reservation.target.nodeGeneration;
            break;
        case 3:
            v.firmwareRecord++;
            break;
        case 4:
            v.current.channel.clientToServerId++;
            break;
        case 5:
            v.observations = f.prior;
            break;
        case 6:
            v.maximumAgeUs = 1;
            break;
        case 7:
            v.powerCycleUs = f.now + 1;
            break;
        case 8:
            v.noInterveningWritesRecord = 0;
            break;
        case 9:
            ++v.powerCycleTarget.channel.binding.configurationGeneration;
            break;
        case 10:
            ++v.powerCycleTarget.channel.serverToClientId;
            break;
        }
        const Saved<L::PersistenceContext> saved(f.context);
        CHECK(!L::verifyPersistence(f.context, v, f.now));
        saved.unchanged(f.context);
    }
    Fixture changed;
    CHECK(changed.prepare());
    changed.finish(C::SdoOutcome::TIMEOUT);
    auto v = changed.verification(true);
    ++v.current.channel.binding.bindingGeneration;
    v.current.channel.binding.endpoint.address = 2;
    v.current.channel.clientToServerId = 0x602;
    v.current.channel.serverToClientId = 0x582;
    v.powerCycleTarget = v.current;
    v.observations = observations(v.current, changed.now);
    CHECK(L::verifyPersistence(changed.context, v, changed.now));
    CHECK(changed.context.observed.identity.target.channel.binding.endpoint.address == 2);
    CHECK(changed.context.before.identity.target.channel.binding.endpoint.address == 1);
    CHECK(changed.context.uncertain && changed.context.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
}
void postCycleReapplicationIsNotSurvival() {
    Fixture f;
    CHECK(f.prepare());
    f.finish(C::SdoOutcome::TIMEOUT);
    auto v = f.verification(true, 100);
    L::DriverObservation afterCycle;
    CHECK(L::getDriver(v.observations.configuration, v.current, f.now, 1000, afterCycle));
    CHECK(afterCycle.positionSpeedRpm == 100 &&
          f.context.before.configuration.positionSpeedRpm == 500);
    // A separate host re-application restores 500 and advances the actual configuration epoch.
    // Matching post-write reads cannot be attributed to the earlier power-cycle boundary.
    ++v.current.channel.binding.configurationGeneration;
    v.observations = observations(v.current, f.now, 500);
    const Saved<L::PersistenceContext> history(f.context);
    CHECK(!L::verifyPersistence(f.context, v, f.now));
    history.unchanged(f.context);
    v.powerCycleObserved = false;
    v.powerCycleRecord = 0;
    v.powerCycleUs = 0;
    v.powerCycleTarget = L::ReadTarget();
    v.noInterveningWritesRecord = 0;
    CHECK(L::verifyPersistence(f.context, v, f.now));
    CHECK(f.context.matchingFields == 7 && !f.context.survivedRestart && f.context.uncertain);
}
struct Wire : OwnerRig::WireBase {
    Fixture* fixture = nullptr;
    C::SdoClient* services = nullptr;
    C::NetworkRoute network;
    unsigned notices = 0;
    M::AxisConfig axis;
    M::AxisReference reference;
    static bool guard(void* context, O::SequenceId, const C::SdoTransfer& t, uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        CHECK(t.index == 0x2300 && t.direction == C::SdoDirection::DOWNLOAD && t.size == 2);
        CHECK(L::notePersistenceTransmission(w.fixture->context, w.fixture->reservation,
                                             w.fixture->binding, now));
        CHECK(w.services->invalidateProducerInterval(0, w.network, now));
        CHECK(w.services->invalidatePdo(0, true, now));
        CHECK(M::invalidateAxisReference(w.axis, w.reference, M::ReferenceLoss::COORDINATES));
        CHECK(M::sameBinding(w.axis.binding, w.fixture->binding));
        ++w.notices;
        return true;
    }
    O::Transport transport() {
        O::Transport t = plumbing();
        t.beforeTransmit = guard;
        return t;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    Fixture f;
    O::SequenceId sequence;
    uint64_t now = 0;
    explicit Rig(bool factory = false) : owner(services, wire.transport(), 73) {
        const uint64_t preparedAt = f.now;
        CHECK(services.configure(0, f.reservation.target.channel, true, true));
        wire.services = &services;
        wire.fixture = &f;
        wire.axis.binding = f.binding;
        wire.reference.binding = f.binding;
        wire.reference.referenceGeneration = wire.axis.referenceGeneration;
        wire.reference.nativeKnown = true;
        wire.network.binding = f.binding;
        wire.network.generation = 11;
        wire.network.heartbeatId = 0x701;
        wire.network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, wire.network, C::NetworkObserver(), now));
        CHECK(services.setProducerInterval(0, wire.network, 0, now));
        C::PdoMap map;
        CHECK(L::pdoTemplate(C::PdoDirection::TPDO, 2, map));
        C::PdoMapEvidence actual;
        actual.route.binding = f.binding;
        actual.route.generation = 1;
        actual.route.cobId = 0x281;
        CHECK(C::validatePdoMap(map, L::pdoProfile(41), actual.route.layout));
        actual.nodeGeneration = 11;
        actual.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
        actual.evidenceId = 501;
        actual.communicationKnown = true;
        actual.cobIdParameter = 0x281;
        actual.transmissionType = 255;
        CHECK(services.configurePdo(0, 0, actual, L::pdoProfile(41), C::PdoObserver(), now));
        CHECK(owner.beginSequence(f.binding, 1, now, sequence));
        f.reservation.owner = owner.identity();
        f.reservation.sequence = uint32_t(sequence.slot + 1);
        f.reservation.generation = sequence.generation;
        f.reservation.acquiredUs = owner.sequence(sequence)->acquiredUs;
        M::Frame heartbeat = OwnerRig::frame(0x701, 1);
        heartbeat.data[0] = 0x7f;
        wire.push(heartbeat, 1);
        turn(1);
        f.review.backupRecord = 11;
        f.review.routeBackRecord = 12;
        now = f.now = preparedAt;
        CHECK(f.prepare(factory));
        CHECK(owner.beginOperation(sequence, 9, f.context.deadlineUs, now));
    }
    void turn(uint64_t at) {
        now = f.now = at;
        CHECK(owner.service(now, OwnerRig::budget()));
    }
    O::RequestId admit(C::SdoTransfer& transfer) {
        const auto p = f.next();
        CHECK(p.ready);
        transfer = p.transfer;
        CHECK(services.authorizeDownloadExperiment(0, transfer, 11, 1, now));
        O::BusRequest request;
        request.sequence = sequence;
        request.transfer = transfer;
        O::RequestId id;
        CHECK(owner.admit(request, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    void answer(uint64_t at) {
        M::Frame reply = OwnerRig::frame(0x581, 8);
        reply.data[0] = 0x60;
        reply.data[2] = 0x23;
        wire.push(reply, at);
        turn(at);
    }
};
void realOwnerCommandsAndRetirement() {
    for (unsigned factory = 0; factory < 2; ++factory) {
        Rig r(factory != 0);
        M::PositionRequest position;
        position.configurationGeneration = r.f.binding.configurationGeneration;
        position.value.numerator = 1;
        M::PreparedTarget prepared;
        CHECK(M::preparePosition(position, r.wire.axis, nullptr, prepared));
        CHECK(M::validatePreparedTarget(prepared, r.wire.axis, nullptr, r.now));
        C::SdoTransfer t;
        const auto id = r.admit(t);
        CHECK(!r.f.context.effectsPossible && r.wire.reference.nativeKnown);
        r.wire.autoReport = false;
        r.wire.retirement = false;
        r.turn(r.now + 1);
        CHECK(r.wire.sentCount == 1 && r.wire.notices == 1);
        CHECK(r.wire.sent[0].data[0] == 0x2b && r.wire.sent[0].data[2] == 0x23);
        CHECK(r.wire.sent[0].data[4] == (factory ? 1 : 2) && r.wire.sent[0].data[5] == 0);
        CHECK(!r.services.pdo(0)->confirmed && !r.services.node(0)->producerKnown &&
              !r.wire.reference.nativeKnown);
        CHECK(!M::validatePreparedTarget(prepared, r.wire.axis, nullptr, r.now));
        L::IdentityObservation identity;
        L::ReadTarget changed = r.f.reservation.target;
        changed.channel.binding = r.f.binding;
        CHECK(!L::getIdentity(r.f.prior.identity, changed, r.now, 1000, identity));
        r.answer(r.now + 1);
        CHECK(r.owner.result(id));
        CHECK(r.owner.result(id)->service.transfer.channel.binding.configurationGeneration == 7);
        CHECK(L::advancePersistence(r.f.context, r.f.reservation, r.f.binding,
                                    r.owner.result(id)->service, r.now));
        CHECK(!r.owner.advanceConfiguration(r.sequence, r.f.binding, r.now));
        CHECK(r.owner.release(id));
        CHECK(!r.owner.localTxRetired(0));
        CHECK(!r.owner.advanceConfiguration(r.sequence, r.f.binding, r.now));
        r.wire.report(0);
        r.turn(r.now + 1);
        CHECK(r.owner.localTxRetired(0));
        CHECK(r.owner.advanceConfiguration(r.sequence, r.f.binding, r.now));
        O::BusRequest stale;
        stale.sequence = r.sequence;
        stale.transfer = t;
        O::RequestId rejected;
        CHECK(r.owner.admit(stale, r.now, rejected) != O::BusAdmission::ACCEPTED);
        L::ReadTarget current = r.f.reservation.target;
        current.channel.binding = r.f.binding;
        CHECK(r.owner.matchesChannel(current.channel, 11));
        auto v = r.f.verification();
        CHECK(L::verifyPersistence(r.f.context, v, r.f.now));
        CHECK(r.f.context.matchingFields == 7 && !r.f.context.survivedRestart);
        CHECK(r.f.context.execution == M::ActionExecution::ACKNOWLEDGED);
        CHECK(!r.f.next().ready && r.wire.sentCount == 1);
    }
}
void realOwnerLostReplyAndReset() {
    for (unsigned reset = 0; reset < 2; ++reset) {
        Rig r;
        C::SdoTransfer t;
        const auto id = r.admit(t);
        r.turn(r.now + 1);
        const L::AxisReservation original = r.f.reservation;
        const M::Binding submittedBinding = r.f.binding;
        if (reset) {
            M::Frame boot = OwnerRig::frame(0x701, 1);
            r.wire.push(boot, r.now + 1);
            r.turn(r.now + 1);
            CHECK(!r.owner.sequence(r.sequence));
        } else {
            r.turn(r.now + 101);
        }
        CHECK(r.owner.result(id));
        const auto e = r.owner.result(id)->service;
        CHECK(e.effectUncertain && !e.protocolAcknowledged);
        CHECK(L::advancePersistence(r.f.context, original, submittedBinding, e, r.now));
        CHECK(r.f.context.state == L::PersistenceState::FAILED && r.f.context.uncertain);
        if (reset) {
            CHECK(e.outcome == C::SdoOutcome::REBOOT);
            const L::AxisReservation revoked;
            L::PreparedRead work;
            const Saved<L::PreparedRead> untouched(work);
            CHECK(!L::nextPersistence(r.f.context, revoked, r.f.binding, r.now, work));
            untouched.unchanged(work);
            const Saved<L::PersistenceContext> terminal(r.f.context);
            CHECK(!L::notePersistenceTransmission(r.f.context, revoked, r.f.binding, r.now));
            terminal.unchanged(r.f.context);
        } else {
            CHECK(!r.f.next().ready);
        }
        CHECK(r.wire.notices == 1 && r.wire.sentCount == 1);
        const Saved<L::PersistenceContext> history(r.f.context);
        CHECK(r.owner.release(id));
        r.turn(r.now + 100);
        history.unchanged(r.f.context);
        CHECK(r.wire.sentCount == 1);
    }
}
} // namespace
int main() {
    commandsAndKnowledge();
    lostReplyDoesNotBecomeSuccess();
    rejectedCandidates();
    invalidNativeEvidenceAndEpochs();
    reconciliationRejectsStaleOrInventedEvidence();
    postCycleReapplicationIsNotSurvival();
    realOwnerCommandsAndRetirement();
    realOwnerLostReplyAndReset();
    std::printf("persistence: %u checks\n", checks);
}
