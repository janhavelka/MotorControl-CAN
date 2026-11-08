// SPDX-License-Identifier: MIT
// Literal 1017 vectors, adverse native terminals and the real sole-owner path. No hardware claims.
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

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
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
} // namespace
#include "support/OwnerRig.h"
namespace {
C::SdoCompletion terminal(const C::SdoTransfer& t, uint64_t at, uint16_t raw = 0,
                          C::SdoOutcome result = C::SdoOutcome::SUCCESS, bool possible = true) {
    C::SdoCompletion e;
    e.transfer = t;
    e.nodeGeneration = 11;
    e.admittedUs = at;
    e.completedUs = at + 1;
    e.outcome = result;
    e.queued = e.transmissionPossible = e.transmissionConfirmed = possible;
    e.effectPossible = possible;
    e.protocolAcknowledged = result == C::SdoOutcome::SUCCESS;
    e.effectUncertain = possible && !e.protocolAcknowledged;
    if (e.protocolAcknowledged && t.direction == C::SdoDirection::UPLOAD) {
        e.size = 2;
        e.data[0] = uint8_t(raw);
        e.data[1] = uint8_t(raw >> 8);
    }
    if (result == C::SdoOutcome::SERVER_ABORT) {
        e.abortValid = true;
        e.abortCode = 0xf1234567;
    }
    return e;
}
struct Fixture {
    L::AxisReservation reservation;
    L::CommunicationReview review;
    L::CommunicationRequest request;
    L::CommunicationNmt nmt;
    L::ReadOptions options;
    L::CommunicationContext context;
    C::SdoCompletion baseline;
    M::Binding binding;
    uint64_t now = 100;
    Fixture() {
        reservation.target.channel = OwnerRig::route();
        reservation.target.nodeGeneration = 11;
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = 73;
        reservation.sequence = 1;
        reservation.generation = 3;
        reservation.acquiredUs = 0;
        binding = reservation.target.channel.binding;
        review.target = reservation.target;
        review.scope = L::CommunicationScope::ISOLATED_EXPERIMENT;
        review.fixtureRecord = 1;
        review.firmwareRecord = 2;
        review.procedureRecord = 3;
        review.authorizationRecord = 4;
        request.producerTimeRaw = 1000;
        options.queueTimeoutUs = options.responseTimeoutUs = 100;
        C::SdoTransfer t;
        t.channel = reservation.target.channel;
        t.operation = t.request = t.step = 1;
        t.index = 0x1017;
        t.size = 2;
        t.deadlineUs = 1000;
        t.queueTimeoutUs = t.responseTimeoutUs = 100;
        baseline = terminal(t, 40);
    }
    void preOp() {
        nmt.known = true;
        nmt.state = C::NmtState::PRE_OPERATIONAL;
        nmt.time.present = nmt.time.receiveBoundsKnown = true;
        nmt.time.receivedEarliestUs = 10;
        nmt.time.receivedLatestUs = 11;
        nmt.time.processedUs = 12;
        nmt.time.generation = 11;
        baseline.transfer.requirePreOperational = true;
    }
    M::Status prepare(uint64_t deadline = 5000, uint64_t age = 10000) {
        return L::prepareCommunication(context, reservation, review, request, baseline, nmt, 9, now,
                                       deadline, options, age);
    }
    L::ReadTarget target() const {
        L::ReadTarget t = reservation.target;
        t.channel.binding = binding;
        return t;
    }
    L::PreparedRead next() {
        L::PreparedRead p;
        CHECK(L::nextCommunication(context, reservation, binding, nmt, now, p));
        return p;
    }
    void ack(C::SdoOutcome outcome = C::SdoOutcome::SUCCESS) {
        const auto p = next();
        CHECK(p.ready);
        CHECK(L::noteCommunicationTransmission(context, reservation, binding, nmt, now));
        const auto e = terminal(p.transfer, now, 0, outcome);
        now += 2;
        CHECK(L::advanceCommunication(context, reservation, binding, e, now));
    }
    void confirm(uint16_t raw = 1000) {
        CHECK(L::prepareCommunicationConfirmation(context, reservation, target(), nmt, now));
        const auto p = next();
        CHECK(p.ready && p.transfer.direction == C::SdoDirection::UPLOAD);
        const auto e = terminal(p.transfer, now, raw);
        now += 2;
        CHECK(L::advanceCommunication(context, reservation, binding, e, now));
    }
};
void candidatesAndEvidence() {
    Fixture good;
    CHECK(good.prepare());
    CHECK(!good.context.requirePreOperational && !good.nmt.known);
    for (unsigned scenario = 0; scenario < 24; ++scenario) {
        Fixture f;
        const Saved<L::CommunicationContext> saved(f.context);
        switch (scenario) {
        case 0:
            f.request.producerTimeRaw = 0;
            break;
        case 1:
            f.request.producerTimeRaw = 999;
            break;
        case 2:
            f.request.producerTimeRaw = UINT16_MAX;
            break;
        case 3:
            f.review.scope = L::CommunicationScope::UNQUALIFIED;
            break;
        case 4:
            f.review.fixtureRecord = 0;
            break;
        case 5:
            f.review.firmwareRecord = 0;
            break;
        case 6:
            f.review.procedureRecord = 0;
            break;
        case 7:
            f.review.authorizationRecord = 0;
            break;
        case 8:
            ++f.review.target.nodeGeneration;
            break;
        case 9:
            f.reservation.model = L::SettingsModel::OL86_C;
            break;
        case 10:
            f.baseline.transfer.size = 1;
            break;
        case 11:
            f.baseline.size = 1;
            break;
        case 12:
            f.baseline.data[0] = 1;
            break;
        case 13:
            ++f.baseline.transfer.channel.serverToClientId;
            break;
        case 14:
            f.baseline.admittedUs = 0;
            f.reservation.acquiredUs = 1;
            break;
        case 15:
            f.baseline.nodeGeneration = 12;
            break;
        case 16:
            f.baseline.effectPossible = false;
            break;
        case 17:
            f.binding.configurationGeneration = UINT32_MAX;
            f.reservation.target.channel.binding = f.binding;
            f.review.target = f.reservation.target;
            break;
        case 18:
            f.nmt.known = true;
            f.nmt.state = C::NmtState::STOPPED;
            break;
        case 19:
            f.review.scope = L::CommunicationScope::FIXTURE_QUALIFIED;
            break;
        case 20:
            f.options.pdoNumber = 1;
            break;
        case 21:
            f.options.countWidths = L::CountWidths::MEASURED_U16;
            break;
        case 22:
            f.options.responseTimeoutUs = 0;
            break;
        case 23:
            f.baseline.transfer.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
            break;
        }
        CHECK(!f.prepare());
        saved.unchanged(f.context);
    }
    Fixture f;
    f.preOp();
    f.review.scope = L::CommunicationScope::FIXTURE_QUALIFIED;
    CHECK(f.prepare() && f.context.requirePreOperational);
    f.nmt.known = false;
    L::PreparedRead p;
    const Saved<L::PreparedRead> output(p);
    CHECK(!L::nextCommunication(f.context, f.reservation, f.binding, f.nmt, f.now, p));
    output.unchanged(p);
    Fixture shortBudget;
    CHECK(!shortBudget.prepare(299));
    CHECK(shortBudget.prepare(300));
    Fixture stale;
    CHECK(!stale.prepare(5000, 59));
    CHECK(stale.prepare(5000, 60));
    L::PreparedRead expired;
    const Saved<L::PreparedRead> expiredSaved(expired);
    CHECK(!L::nextCommunication(stale.context, stale.reservation, stale.binding, stale.nmt, 101,
                                expired));
    expiredSaved.unchanged(expired);
    const Saved<L::CommunicationContext> staleSaved(stale.context);
    CHECK(!L::noteCommunicationTransmission(stale.context, stale.reservation, stale.binding,
                                            stale.nmt, 101));
    staleSaved.unchanged(stale.context);
}
void exactWriteAndExplicitConfirmation() {
    Fixture f;
    CHECK(f.prepare());
    const auto p = f.next();
    CHECK(p.ready && p.transfer.index == 0x1017 && p.transfer.subindex == 0);
    CHECK(p.transfer.size == 2 && p.transfer.direction == C::SdoDirection::DOWNLOAD);
    CHECK(p.transfer.step == 1 && p.transfer.data[0] == 0xe8 && p.transfer.data[1] == 3);
    CHECK(p.transfer.channel.binding.configurationGeneration == 7);
    f.ack();
    CHECK(f.context.state == L::CommunicationState::AWAITING_CONFIRMATION);
    CHECK(f.context.writeAcknowledged && f.context.writeEvidencePresent && f.context.uncertain);
    CHECK(f.binding.configurationGeneration == 8 && !f.next().ready);
    CHECK(f.context.prerequisitesWithdrawn);
    const Saved<C::SdoCompletion> ack(f.context.writeEvidence);
    const Saved<L::CommunicationContext> awaiting(f.context);
    CHECK(!L::noteCommunicationTransmission(f.context, f.reservation, f.binding, f.nmt, f.now));
    awaiting.unchanged(f.context);
    auto wrong = f.target();
    wrong.channel.serverToClientId++;
    CHECK(!L::prepareCommunicationConfirmation(f.context, f.reservation, wrong, f.nmt, f.now));
    awaiting.unchanged(f.context);
    f.confirm();
    ack.unchanged(f.context.writeEvidence);
    CHECK(f.context.state == L::CommunicationState::SUCCEEDED && !f.context.uncertain);
    CHECK(f.context.writeOutcome == M::ReadOutcome::SUCCESS);
    CHECK(f.context.confirmationEvidence.transfer.step == 2);
    CHECK(f.context.confirmationEvidence.transfer.channel.binding.configurationGeneration == 8);
    CHECK(f.context.confirmationEvidence.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED);
    CHECK(f.context.restart == L::CommunicationRequirement::UNKNOWN);
    CHECK(f.context.persistence == L::CommunicationRequirement::UNKNOWN);
    L::CommunicationResult result;
    CHECK(L::getCommunication(f.context, f.reservation, f.target(), f.now, result));
    CHECK(result.producerTimeRaw == 1000 &&
          result.scope == L::CommunicationScope::ISOLATED_EXPERIMENT);
    const Saved<L::CommunicationResult> saved(result);
    wrong = f.target();
    wrong.nodeGeneration++;
    CHECK(!L::getCommunication(f.context, f.reservation, wrong, f.now, result));
    saved.unchanged(result);
    CHECK(!L::getCommunication(f.context, f.reservation, f.target(), f.now + 10001, result));
    saved.unchanged(result);
    CHECK(!f.next().ready);
    const Saved<L::CommunicationContext> completed(f.context);
    CHECK(!L::advanceCommunication(f.context, M::ReadOutcome::INVALIDATED, f.now));
    completed.unchanged(f.context);
}
void permanentFailureAndRetention() {
    for (auto cause : {C::SdoOutcome::TIMEOUT, C::SdoOutcome::SERVER_ABORT, C::SdoOutcome::REBOOT,
                       C::SdoOutcome::BINDING_INVALIDATED}) {
        Fixture f;
        CHECK(f.prepare());
        f.ack(cause);
        CHECK(f.context.state == L::CommunicationState::FAILED && f.context.uncertain);
        CHECK(f.context.effectsPossible && !f.context.writeAcknowledged);
        CHECK(f.context.writeEvidence.outcome == cause && !f.next().ready);
        if (cause == C::SdoOutcome::SERVER_ABORT)
            CHECK(f.context.writeEvidence.abortValid &&
                  f.context.writeEvidence.abortCode == 0xf1234567);
        const Saved<L::CommunicationContext> saved(f.context);
        CHECK(!L::prepareCommunicationConfirmation(f.context, f.reservation, f.target(), f.nmt,
                                                   f.now));
        saved.unchanged(f.context);
    }
    Fixture f;
    CHECK(f.prepare());
    f.ack();
    const Saved<C::SdoCompletion> ack(f.context.writeEvidence);
    f.confirm(999);
    CHECK(f.context.state == L::CommunicationState::FAILED && f.context.readbackKnown);
    CHECK(f.context.readbackRaw == 999 && !f.context.readbackMatched && f.context.uncertain);
    CHECK(f.context.writeAcknowledged && f.context.writeOutcome == M::ReadOutcome::SUCCESS);
    ack.unchanged(f.context.writeEvidence);
    Fixture late;
    CHECK(late.prepare(400));
    late.ack();
    const Saved<L::CommunicationContext> unchanged(late.context);
    CHECK(!L::prepareCommunicationConfirmation(late.context, late.reservation, late.target(),
                                               late.nmt, 201));
    unchanged.unchanged(late.context);
    CHECK(L::advanceCommunication(late.context, M::ReadOutcome::DEADLINE, 400));
    CHECK(late.context.writeAcknowledged && late.context.writeOutcome == M::ReadOutcome::SUCCESS);
    Fixture readTimeout;
    CHECK(readTimeout.prepare());
    readTimeout.ack();
    const Saved<C::SdoCompletion> originalWrite(readTimeout.context.writeEvidence);
    CHECK(L::prepareCommunicationConfirmation(readTimeout.context, readTimeout.reservation,
                                              readTimeout.target(), readTimeout.nmt,
                                              readTimeout.now));
    auto timed = terminal(readTimeout.next().transfer, readTimeout.now, 0, C::SdoOutcome::TIMEOUT);
    readTimeout.now += 2;
    CHECK(L::advanceCommunication(readTimeout.context, readTimeout.reservation, readTimeout.binding,
                                  timed, readTimeout.now));
    CHECK(readTimeout.context.state == L::CommunicationState::FAILED &&
          readTimeout.context.uncertain);
    CHECK(readTimeout.context.writeOutcome == M::ReadOutcome::SUCCESS &&
          readTimeout.context.writeAcknowledged);
    originalWrite.unchanged(readTimeout.context.writeEvidence);
}
void successfulPublicationChecksOriginalEvidence() {
    for (unsigned scenario = 0; scenario < 7; ++scenario) {
        Fixture f;
        CHECK(f.prepare());
        f.ack();
        f.confirm();
        switch (scenario) {
        case 0:
            f.context.writeEvidence.nodeGeneration++;
            break;
        case 1:
            f.context.writeEvidence.admittedUs = f.context.startedUs - 1;
            break;
        case 2:
            f.context.writeEvidence.admittedUs = f.context.writeEvidence.completedUs + 1;
            break;
        case 3:
            f.context.writeEvidence.completedUs = f.context.stepStartedUs + 1;
            break;
        case 4:
            f.context.writeEvidence.completedUs = f.context.deadlineUs;
            break;
        case 5:
            f.context.confirmationEvidence.data[0] ^= 1;
            break;
        case 6:
            f.context.confirmationEvidence.admittedUs = f.context.stepStartedUs - 1;
            break;
        }
        L::CommunicationResult result;
        const Saved<L::CommunicationResult> saved(result);
        CHECK(!L::getCommunication(f.context, f.reservation, f.target(), f.now, result));
        saved.unchanged(result);
    }
}
void nativeEnvelopeRejections() {
    for (unsigned scenario = 0; scenario < 11; ++scenario) {
        Fixture f;
        CHECK(f.prepare());
        const auto p = f.next();
        CHECK(L::noteCommunicationTransmission(f.context, f.reservation, f.binding, f.nmt, f.now));
        auto e = terminal(p.transfer, f.now);
        switch (scenario) {
        case 0:
            e.queued = false;
            break;
        case 1:
            e.effectPossible = false;
            break;
        case 2:
            e.effectUncertain = true;
            break;
        case 3:
            e.protocolAcknowledged = false;
            break;
        case 4:
            e.abortValid = true;
            break;
        case 5:
            e.clientAbortQueued = true;
            break;
        case 6:
            e.size = 1;
            break;
        case 7:
            e.transfer.data[1] ^= 1;
            break;
        case 8:
            e.transfer.step++;
            break;
        case 9:
            e.nodeGeneration++;
            break;
        case 10:
            e.admittedUs = f.now - 1;
            break;
        }
        const Saved<L::CommunicationContext> saved(f.context);
        CHECK(!L::advanceCommunication(f.context, f.reservation, f.binding, e, f.now + 2));
        saved.unchanged(f.context);
    }
}
void immutableRejectionsAndLocalWithdrawal() {
    Fixture f;
    CHECK(f.prepare());
    auto p = f.next();
    auto e = terminal(p.transfer, f.now);
    const Saved<L::CommunicationContext> before(f.context);
    CHECK(!L::advanceCommunication(f.context, f.reservation, f.binding, e, f.now + 2));
    before.unchanged(f.context); // Possible TX without the mandatory prior notice.
    CHECK(L::noteCommunicationTransmission(f.context, f.reservation, f.binding, f.nmt, f.now));
    const Saved<L::CommunicationContext> noted(f.context);
    auto changed = f.reservation;
    changed.generation++;
    CHECK(!L::advanceCommunication(f.context, changed, f.binding, e, f.now + 2));
    noted.unchanged(f.context);
    e.transfer.channel.serverToClientId++;
    CHECK(!L::advanceCommunication(f.context, f.reservation, f.binding, e, f.now + 2));
    noted.unchanged(f.context);
    CHECK(L::advanceCommunication(f.context, M::ReadOutcome::INVALIDATED, f.now));
    CHECK(f.context.effectsPossible && f.context.uncertain && !f.context.writeEvidencePresent);
    Fixture noTx;
    CHECK(noTx.prepare());
    CHECK(L::advanceCommunication(noTx.context, M::ReadOutcome::CANCELLED, noTx.now));
    CHECK(!noTx.context.effectsPossible && !noTx.context.uncertain &&
          !noTx.context.writeEvidencePresent);
    Fixture overlap;
    CHECK(overlap.prepare());
    const Saved<L::CommunicationContext> initial(overlap.context);
    CHECK(!L::prepareCommunication(overlap.context, overlap.context.reservation, overlap.review,
                                   overlap.request, overlap.baseline, overlap.nmt, 19, overlap.now,
                                   5000, overlap.options, 1000));
    initial.unchanged(overlap.context);
}

// Ordinary adapter and owner traffic, including a real pre-handoff withdrawal callback.
struct Wire : OwnerRig::WireBase {
    L::CommunicationContext* context = nullptr;
    L::AxisReservation* reservation = nullptr;
    M::Binding* current = nullptr;
    C::SdoClient* services = nullptr;
    C::NetworkRoute network;
    L::CommunicationNmt nmt;
    unsigned notices = 0;
    bool callbackWithdrawn = false;
    static bool guard(void* opaque, O::SequenceId, const C::SdoTransfer& transfer, uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(opaque));
        if (!w.context || transfer.direction != C::SdoDirection::DOWNLOAD)
            return false;
        CHECK(w.services->invalidateProducerInterval(0, w.network, now));
        for (uint8_t i = 0; i < C::SdoClient::PDO_ROUTES; ++i) {
            const C::PdoRecord* record = w.services->pdo(i);
            if (record &&
                record->map.route.binding.endpoint.bus == w.network.binding.endpoint.bus &&
                record->map.route.binding.endpoint.address == w.network.binding.endpoint.address)
                CHECK(w.services->invalidatePdo(i, true, now));
        }
        CHECK(L::noteCommunicationTransmission(*w.context, *w.reservation, *w.current, w.nmt, now));
        w.callbackWithdrawn = true;
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
    uint32_t grant = 0;
    explicit Rig(bool preOperational = false) : owner(services, wire.transport(), 73) {
        CHECK(services.configure(0, OwnerRig::route(), true, true));
        wire.services = &services;
        wire.network.binding = OwnerRig::route().binding;
        wire.network.generation = 11;
        wire.network.heartbeatId = 0x701;
        wire.network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, wire.network, C::NetworkObserver(), now));
        CHECK(services.setProducerInterval(0, wire.network, 0, now));
        C::PdoMap map;
        CHECK(L::pdoTemplate(C::PdoDirection::TPDO, 2, map));
        C::PdoMapEvidence actual;
        actual.route.binding = OwnerRig::route().binding;
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
        if (preOperational) {
            M::Frame heartbeat = OwnerRig::frame(0x701, 1);
            heartbeat.data[0] = 0x7f;
            wire.push(heartbeat, 1);
            turn(1);
            f.nmt.known = true;
            f.nmt.state = C::NmtState::PRE_OPERATIONAL;
            f.nmt.time = services.node(0)->nmtTime;
        }
        CHECK(owner.beginSequence(OwnerRig::route().binding, 1, now, sequence));
        f.reservation.owner = owner.identity();
        f.reservation.sequence = uint32_t(sequence.slot + 1);
        f.reservation.generation = sequence.generation;
        f.reservation.acquiredUs = owner.sequence(sequence)->acquiredUs;
        CHECK(owner.beginOperation(sequence, 1, 5000, now));
        auto t = f.baseline.transfer;
        t.deadlineUs = 5000;
        t.requirePreOperational = preOperational;
        t.readEffect = C::SdoReadEffect::UNQUALIFIED;
        const auto id = admit(t);
        turn(1);
        answer(t, 0, 2);
        CHECK(owner.result(id));
        f.baseline = owner.result(id)->service;
        CHECK(owner.release(id));
        f.now = now;
        CHECK(owner.beginOperation(sequence, 9, 5000, now));
        CHECK(f.prepare());
        wire.context = &f.context;
        wire.reservation = &f.reservation;
        wire.current = &f.binding;
        wire.nmt = f.nmt;
    }
    void turn(uint64_t at) {
        now = at;
        CHECK(owner.service(now, OwnerRig::budget()));
    }
    O::RequestId admit(const C::SdoTransfer& t) {
        if (t.direction == C::SdoDirection::UPLOAD)
            CHECK(services.authorizeUploadExperiment(0, t, 11, ++grant, now));
        else
            CHECK(services.authorizeDownloadExperiment(0, t, 11, ++grant, now));
        O::BusRequest request;
        request.sequence = sequence;
        request.transfer = t;
        O::RequestId id;
        CHECK(owner.admit(request, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    void answer(const C::SdoTransfer& t, uint16_t raw, uint64_t at, bool wrongRoute = false) {
        M::Frame reply = OwnerRig::frame(t.channel.serverToClientId + (wrongRoute ? 1u : 0u), 8);
        reply.data[0] = t.direction == C::SdoDirection::DOWNLOAD ? 0x60 : 0x4b;
        reply.data[1] = 0x17;
        reply.data[2] = 0x10;
        if (t.direction == C::SdoDirection::UPLOAD) {
            reply.data[4] = uint8_t(raw);
            reply.data[5] = uint8_t(raw >> 8);
        }
        wire.push(reply, at);
        turn(at);
    }
    L::PreparedRead work() {
        f.now = now;
        return f.next();
    }
};
void realOwnerAckReadbackAndWithdrawal() {
    Rig r;
    CHECK(r.services.node(0)->producerKnown && r.services.pdo(0)->confirmed);
    auto p = r.work();
    const auto id = r.admit(p.transfer);
    CHECK(!r.wire.callbackWithdrawn && !r.f.context.effectsPossible);
    r.turn(3);
    CHECK(r.wire.callbackWithdrawn && r.wire.notices == 1);
    CHECK(!r.services.node(0)->producerKnown && !r.services.pdo(0)->confirmed);
    CHECK(r.wire.sent[1].data[0] == 0x2b && r.wire.sent[1].data[4] == 0xe8);
    r.answer(p.transfer, 0, 4);
    CHECK(r.owner.result(id));
    CHECK(L::advanceCommunication(r.f.context, r.f.reservation, r.f.binding,
                                  r.owner.result(id)->service, r.now));
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.f.binding, r.now));
    CHECK(r.owner.release(id));
    CHECK(r.owner.localTxRetired(0));
    CHECK(r.owner.advanceConfiguration(r.sequence, r.f.binding, r.now));
    CHECK(r.owner.matchesChannel(r.f.target().channel, 11));
    CHECK(L::prepareCommunicationConfirmation(r.f.context, r.f.reservation, r.f.target(), r.f.nmt,
                                              r.now));
    p = r.work();
    CHECK(p.transfer.requireUploadReview);
    O::BusRequest unreviewed;
    unreviewed.sequence = r.sequence;
    unreviewed.transfer = p.transfer;
    O::RequestId refused;
    const Saved<O::RequestId> refusedSaved(refused);
    CHECK(r.owner.admit(unreviewed, r.now, refused) == O::BusAdmission::INVALID);
    refusedSaved.unchanged(refused);
    CHECK(r.wire.sentCount == 2);
    const auto read = r.admit(p.transfer);
    r.turn(5);
    r.answer(p.transfer, 1000, 6);
    CHECK(r.owner.result(read));
    CHECK(L::advanceCommunication(r.f.context, r.f.reservation, r.f.binding,
                                  r.owner.result(read)->service, r.now));
    CHECK(r.owner.release(read));
    L::CommunicationResult result;
    CHECK(L::getCommunication(r.f.context, r.f.reservation, r.f.target(), r.now, result));
    CHECK(result.producerTimeRaw == 1000 && r.wire.sentCount == 3);
    CHECK(!r.services.node(0)->producerKnown && !r.services.pdo(0)->confirmed);
    CHECK(!r.work().ready && r.wire.notices == 1);
}
void realOwnerRequiresRetiredHandoff() {
    Rig r;
    r.wire.autoReport = false;
    r.wire.retirement = false;
    const auto p = r.work();
    const auto id = r.admit(p.transfer);
    r.turn(3);
    r.answer(p.transfer, 0, 4);
    CHECK(r.owner.result(id));
    CHECK(r.owner.result(id)->service.protocolAcknowledged);
    CHECK(L::advanceCommunication(r.f.context, r.f.reservation, r.f.binding,
                                  r.owner.result(id)->service, r.now));
    CHECK(r.owner.release(id));
    CHECK(!r.owner.localTxRetired(0));
    CHECK(!r.owner.advanceConfiguration(r.sequence, r.f.binding, r.now));
    r.wire.report(1);
    r.turn(5);
    CHECK(r.owner.localTxRetired(0));
    CHECK(r.owner.advanceConfiguration(r.sequence, r.f.binding, r.now));
    CHECK(r.wire.sentCount == 2 && r.f.context.writeAcknowledged);
}
void realOwnerLostReplyWrongRouteAndBoot() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        Rig r;
        const auto p = r.work();
        const auto id = r.admit(p.transfer);
        r.turn(3);
        if (scenario == 1)
            r.answer(p.transfer, 0, 4, true);
        if (scenario == 2) {
            M::Frame boot = OwnerRig::frame(0x701, 1);
            r.wire.push(boot, 4);
            r.turn(4);
            CHECK(!r.owner.sequence(r.sequence));
        } else if (scenario == 3) {
            CHECK(r.services.invalidateNetwork(0, C::NetworkInvalidation::BINDING, 4));
            r.turn(4);
            CHECK(!r.owner.sequence(r.sequence));
        } else {
            r.turn(104);
        }
        CHECK(r.owner.result(id));
        const auto e = r.owner.result(id)->service;
        CHECK(e.outcome != C::SdoOutcome::SUCCESS && e.effectUncertain);
        CHECK(L::advanceCommunication(r.f.context, r.f.reservation, r.f.binding, e, r.now));
        CHECK(r.f.context.state == L::CommunicationState::FAILED && r.f.context.uncertain);
        CHECK(!r.services.node(0)->producerKnown && !r.services.pdo(0)->confirmed);
        CHECK(r.wire.sentCount == 2 && r.wire.notices == 1);
        CHECK(!L::prepareCommunicationConfirmation(r.f.context, r.f.reservation, r.f.target(),
                                                   r.f.nmt, r.now));
        r.turn(r.now + 20);
        CHECK(r.wire.sentCount == 2);
    }
}
void queuedPreOperationalWorkRejectsChangedState() {
    for (auto state : {C::NmtState::OPERATIONAL, C::NmtState::STOPPED, C::NmtState::INITIALIZING}) {
        Rig r(true);
        CHECK(r.f.context.requirePreOperational && r.f.nmt.known);
        const auto p = r.work();
        CHECK(p.transfer.requirePreOperational);
        const auto id = r.admit(p.transfer);
        CHECK(r.owner.service(r.now, OwnerRig::budget(0, 0, 0, 0)));
        CHECK(r.wire.sentCount == 1 && !r.wire.callbackWithdrawn);
        M::Frame changed = OwnerRig::frame(0x701, 1);
        changed.data[0] = uint8_t(state);
        r.wire.push(changed, 3);
        r.now = 3;
        CHECK(r.owner.service(r.now, OwnerRig::budget(8, 0, 8, 0)));
        r.turn(4);
        CHECK(!r.wire.callbackWithdrawn && r.wire.notices == 0 && r.wire.sentCount == 1);
        // Ineligible queued work may wait for the immutable operation cutoff; it never transmits.
        if (!r.owner.result(id))
            r.turn(5000);
        CHECK(r.owner.result(id));
        CHECK(r.owner.result(id)->service.outcome != C::SdoOutcome::SUCCESS);
        CHECK(!r.owner.result(id)->service.transmissionPossible);
        CHECK(!r.wire.callbackWithdrawn && r.wire.notices == 0 && r.wire.sentCount == 1);
        CHECK(!r.f.context.effectsPossible && !r.f.context.prerequisitesWithdrawn);
    }
}
} // namespace
int main() {
    candidatesAndEvidence();
    exactWriteAndExplicitConfirmation();
    permanentFailureAndRetention();
    successfulPublicationChecksOriginalEvidence();
    nativeEnvelopeRejections();
    immutableRejectionsAndLocalWithdrawal();
    realOwnerAckReadbackAndWithdrawal();
    realOwnerRequiresRetiredHandoff();
    realOwnerLostReplyWrongRouteAndBoot();
    queuedPreOperationalWorkRejectsChangedState();
    std::printf("heartbeat communication: %u checks passed\n", checks);
}
