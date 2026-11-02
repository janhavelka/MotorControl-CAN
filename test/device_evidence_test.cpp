// SPDX-License-Identifier: MIT
// Synthetic traffic exercises the ordinary application and its sole receive owner.
#include "DeviceEvidence.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
template <size_t N> void label(char (&output)[N], const char* value) {
    CHECK(std::strlen(value) < N);
    std::strcpy(output, value);
}
struct Field {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t raw;
};
// Invented identities and values, not vendor defaults or measured firmware facts.
const Field identity[] = {
    {0x1000, 0, 4, 0x12345678}, {0x1009, 0, 2, 0x1122},     {0x100a, 0, 2, 0x3344},
    {0x1018, 0, 1, 3},          {0x1018, 1, 4, 0xa1b2c3d4}, {0x1018, 2, 4, 0x55667788},
    {0x1018, 3, 4, 0x99aabbcc}, {0x2000, 0, 2, 0x4567},     {0x2001, 0, 2, 0x6789}};
const Field configuration[] = {{0x1005, 0, 4, 0x80}, {0x1014, 0, 4, 0x85},  {0x1017, 0, 2, 250},
                               {0x1200, 0, 1, 2},    {0x1200, 1, 4, 0x605}, {0x1200, 2, 4, 0x585}};
const Field status[] = {{0x6041, 0, 2, 0x8123}, {0x6061, 0, 1, 0xff}};
M::Frame frame(uint32_t id, uint8_t count) {
    M::Frame result;
    result.id = id;
    result.dlc = result.payloadLength = count;
    return result;
}
L::ReadTarget target() {
    L::ReadTarget result;
    result.channel.binding.logicalAxisId = 9;
    result.channel.binding.endpoint.profile = 1;
    result.channel.binding.endpoint.address = 5;
    result.channel.binding.bindingGeneration = 1;
    result.channel.binding.configurationGeneration = 1;
    result.channel.clientToServerId = 0x605;
    result.channel.serverToClientId = 0x585;
    result.nodeGeneration = 1;
    return result;
}
struct Wire {
    enum class Mode { SUCCESS, ABORT, SILENT, STALE };
    Mode mode = Mode::SUCCESS;
    uint64_t now = 0;
    E::ReceivedFrame incoming[32];
    E::TxReport reports[4];
    M::Frame sent[32];
    unsigned rxHead = 0, rxCount = 0, reportHead = 0, reportCount = 0;
    unsigned transmitted = 0, receiveCalls = 0, observed = 0;
    unsigned abortAt = 0;
    void push(const M::Frame& value, uint64_t captured) {
        CHECK(rxCount < 32);
        E::ReceivedFrame& item = incoming[(rxHead + rxCount++) % 32];
        item.frame = value;
        item.receivedTimeKnown = true;
        item.receivedUs = captured;
        item.controllerEpoch = 1;
    }
    static bool receive(void* context, E::ReceivedFrame& output) {
        Wire& wire = *static_cast<Wire*>(context);
        ++wire.receiveCalls;
        if (!wire.rxCount)
            return false;
        output = wire.incoming[wire.rxHead];
        wire.rxHead = (wire.rxHead + 1) % 32;
        --wire.rxCount;
        return true;
    }
    static bool transmit(void* context, const M::Frame& request, E::TransportTxId id, uint32_t epoch) {
        Wire& wire = *static_cast<Wire*>(context);
        CHECK(epoch == 1 && request.id == 0x605 && !request.extended && !request.rtr);
        CHECK(request.payloadLength == 8 && request.data[0] == 0x40);
        CHECK(wire.transmitted < 32 && wire.reportCount < 4);
        const uint16_t index = uint16_t(request.data[1]) | uint16_t(request.data[2]) << 8;
        const Field* selected = nullptr;
        for (const Field& field : identity)
            if (field.index == index && field.subindex == request.data[3])
                selected = &field;
        for (const Field& field : configuration)
            if (field.index == index && field.subindex == request.data[3])
                selected = &field;
        for (const Field& field : status)
            if (field.index == index && field.subindex == request.data[3])
                selected = &field;
        CHECK(selected);
        const bool abort = wire.mode == Mode::ABORT && wire.transmitted == wire.abortAt;
        wire.sent[wire.transmitted++] = request;
        E::TxReport& report = wire.reports[(wire.reportHead + wire.reportCount++) % 4];
        report.id = id;
        report.controllerEpoch = epoch;
        report.transmitted = true;
        if (wire.mode != Mode::SILENT) {
            M::Frame reply = frame(0x585, 8);
            reply.data[0] = abort                  ? 0x80
                            : selected->width == 1 ? 0x4f
                            : selected->width == 2 ? 0x4b
                                                   : 0x43;
            reply.data[1] = request.data[1];
            reply.data[2] = request.data[2];
            reply.data[3] = request.data[3];
            const uint32_t raw = abort ? 0xfedcba98u : selected->raw;
            for (unsigned i = 0; i < 4; ++i)
                reply.data[4 + i] = static_cast<uint8_t>(raw >> (8 * i));
            wire.push(reply, wire.mode == Mode::STALE ? 0 : wire.now);
        }
        return true;
    }
    static bool report(void* context, E::TxReport& output) {
        Wire& wire = *static_cast<Wire*>(context);
        if (!wire.reportCount)
            return false;
        output = wire.reports[wire.reportHead];
        wire.reportHead = (wire.reportHead + 1) % 4;
        --wire.reportCount;
        return true;
    }
    static void observe(void* context, const E::ReceivedFrame&, uint64_t) {
        ++static_cast<Wire*>(context)->observed;
    }
    E::Transport callbacks() {
        E::Transport result;
        result.context = this;
        result.receive = receive;
        result.transmit = transmit;
        result.takeTxReport = report;
        result.observe = observe;
        return result;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    E::ProbePlatform platform;
    E::CanBusOwner owner;
    E::ProbeApp app;
    E::DeviceEvidence evidence;
    uint64_t now = 0;
    explicit Rig(E::EvidenceEnvironment environment = E::EvidenceEnvironment::HOST_FAKE,
                 bool qualified = true, L::ReadTarget selected = target())
        : platform(E::ProbeCallbacks()), owner(services, wire.callbacks(), 129),
          app(owner, platform), evidence(app, owner, environment) {
        CHECK(services.configure(0, selected.channel, qualified, true));
        C::NetworkRoute network;
        network.binding = selected.channel.binding;
        network.generation = selected.nodeGeneration;
        network.heartbeatId = 0x705;
        network.emcyId = 0x85;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        CHECK(app.select(selected));
    }
    E::AcquisitionPlan plan(L::ReadKind kind = L::ReadKind::IDENTITY, uint32_t id = 1) const {
        E::AcquisitionPlan result;
        result.id = id;
        label(result.fixtureId, "synthetic-fixture-A");
        label(result.softwareRevision, "0123456789abcdef0123456789abcdef01234567");
        label(result.firmware, "synthetic-firmware-A");
        label(result.dialect, "raw-unresolved");
        label(result.authorization, "offline finite test only");
        label(result.observations, "retained test record; no physical observation");
        result.target = *app.selected();
        result.kind = kind;
        result.stateMask = kind == L::ReadKind::STATE ? 1u << unsigned(L::StateBlock::STATUS) : 0;
        result.deadlineUs = 1000;
        result.options.queueTimeoutUs = 50;
        result.options.responseTimeoutUs = 20;
        return result;
    }
    E::ReadExperiment experiment() const {
        E::ReadExperiment result;
        result.plan = plan();
        label(result.sourceReview, "synthetic unknown-effect procedure; not a source resolution");
        label(result.independentReview, "offline reviewer / procedure test-1");
        label(result.fixtureReview, "synthetic controller; no device attached");
        label(result.independentControl, "synthetic finite fixture control");
        label(result.failurePlan, "retain failure and prohibit replay");
        label(result.reconciliation, "inspect retained raw record before a different request");
        return result;
    }
    void turn(uint64_t time, bool collect = true) {
        wire.now = now = time;
        CHECK(app.service(time));
        const unsigned receives = wire.receiveCalls, sent = wire.transmitted;
        if (collect) {
            CHECK(evidence.service(time));
            CHECK(wire.receiveCalls == receives && wire.transmitted == sent);
        }
    }
    void finish() {
        for (unsigned i = 0; i < 100 && !evidence.record()->terminal; ++i)
            turn(now + 1);
        CHECK(evidence.record()->terminal);
    }
};
void passiveAndAdmission() {
    Rig rig(E::EvidenceEnvironment::DEVICE);
    CHECK(!rig.evidence.record() && !rig.wire.receiveCalls && !rig.wire.transmitted);
    rig.turn(0);
    CHECK(!rig.wire.transmitted);
    E::AcquisitionPlan plan = rig.plan();
    CHECK(rig.evidence.request(plan, 0) == E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
    CHECK(!rig.evidence.record() && rig.app.readContext().state == M::ReadState::EMPTY);
    CHECK(!rig.wire.transmitted && !rig.evidence.release());
    for (unsigned i = 0; i < 12; ++i) {
        E::AcquisitionPlan bad = plan;
        switch (i) {
        case 0:
            bad.id = 0;
            break;
        case 1:
            bad.fixtureId[0] = 0;
            break;
        case 2:
            std::memset(bad.authorization, 'x', sizeof bad.authorization);
            break;
        case 3:
            bad.softwareRevision[39] = 'g';
            break;
        case 4:
            bad.softwareRevision[40] = 'a';
            break;
        case 5:
            bad.observations[0] = 0;
            break;
        case 6:
            bad.target.nodeGeneration++;
            break;
        case 7:
            bad.target.channel.serverToClientId++;
            break;
        case 8:
            bad.deadlineUs = 0;
            break;
        case 9:
            bad.options.responseTimeoutUs = 0;
            break;
        case 10:
            bad.kind = L::ReadKind::PROBE;
            break;
        case 11:
            bad.stateMask = 1;
            break;
        }
        CHECK(rig.evidence.request(bad, 0) == E::EvidenceAdmission::INVALID);
        CHECK(!rig.evidence.record() && !rig.wire.transmitted);
    }
    E::ReadExperiment review = rig.experiment();
    for (unsigned i = 0; i < 6; ++i) {
        E::ReadExperiment bad = review;
        char* required[] = {bad.sourceReview,       bad.independentReview, bad.fixtureReview,
                            bad.independentControl, bad.failurePlan,       bad.reconciliation};
        required[i][0] = 0;
        CHECK(rig.evidence.requestExperiment(bad, 0) == E::EvidenceAdmission::INVALID);
        CHECK(!rig.evidence.record() && !rig.wire.transmitted);
    }
    Rig badMode(static_cast<E::EvidenceEnvironment>(0xff));
    CHECK(badMode.evidence.request(badMode.plan(), 0) == E::EvidenceAdmission::INVALID);
    Rig bootstrap(E::EvidenceEnvironment::HOST_FAKE, false);
    CHECK(bootstrap.evidence.request(bootstrap.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    bootstrap.finish();
    CHECK(!bootstrap.wire.transmitted && bootstrap.evidence.record()->terminal &&
          bootstrap.evidence.record()->read.outcome == M::ReadOutcome::INVALIDATED);
}
void exactSuccessAndRetention() {
    Rig rig;
    E::AcquisitionPlan plan = rig.plan();
    CHECK(rig.evidence.request(plan, 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(!rig.wire.transmitted && !rig.evidence.release());
    label(plan.fixtureId, "changed-after-admission");
    CHECK(std::strcmp(rig.evidence.record()->plan.fixtureId, "synthetic-fixture-A") == 0);
    CHECK(rig.evidence.request(plan, 0) == E::EvidenceAdmission::BUSY);
    rig.finish();
    const E::AcquisitionRecord& record = *rig.evidence.record();
    CHECK(record.environment == E::EvidenceEnvironment::HOST_FAKE && !record.experimental);
    CHECK(record.read.state == M::ReadState::SUCCEEDED && record.intendedCount == 9);
    CHECK(record.terminalCount == 9 && rig.wire.transmitted == 9 && !record.evidenceLost);
    CHECK(record.initialStats.txFrames == 0 && record.initialStats.rxFrames == 0 &&
          record.finalStats.txFrames == 9 && record.finalStats.rxFrames == 9 &&
          record.finalStats.rxLost == 0 && record.maxServiceGapUs == 1);
    CHECK(record.initialResources.reservations == 1 && record.initialResources.queued == 0 &&
          record.finalResources.reservations == 1 && record.finalResources.retained == 0 &&
          record.finalResources.active == 0);
    CHECK(record.trafficCount == 27 && record.startedUs == 0 && record.completedUs == rig.now);
    for (unsigned i = 0; i < 9; ++i) {
        const Field& expected = identity[i];
        CHECK(record.intended[i].index == expected.index &&
              record.intended[i].subindex == expected.subindex &&
              record.intended[i].widthBytes == expected.width);
        CHECK(record.intended[i].readEffect == L::ReadEffect::UNQUALIFIED);
        const uint8_t request[] = {
            0x40, uint8_t(expected.index), uint8_t(expected.index >> 8), expected.subindex, 0, 0, 0,
            0};
        CHECK(!std::memcmp(rig.wire.sent[i].data, request, 8));
        const E::Completion& terminal = record.terminals[i];
        CHECK(terminal.service.outcome == C::SdoOutcome::SUCCESS && terminal.service.replyValid);
        CHECK(terminal.service.transfer.index == expected.index &&
              terminal.service.transfer.subindex == expected.subindex &&
              terminal.service.transfer.size == expected.width);
        CHECK(terminal.service.effectPossible && !terminal.service.effectUncertain);
        CHECK(terminal.service.transfer.direction == C::SdoDirection::UPLOAD);
        CHECK(terminal.service.reply.id == 0x585 && terminal.service.reply.data[1] == request[1] &&
              terminal.service.reply.data[2] == request[2] &&
              terminal.service.reply.data[3] == request[3]);
        CHECK(terminal.service.reply.data[0] == (expected.width == 1   ? 0x4f
                                                 : expected.width == 2 ? 0x4b
                                                                       : 0x43));
        for (unsigned b = 0; b < expected.width; ++b)
            CHECK(terminal.service.reply.data[4 + b] == uint8_t(expected.raw >> (8 * b)));
        CHECK(record.read.values[i].valid && record.read.values[i].raw == expected.raw);
    }
    unsigned tx = 0, rx = 0, reports = 0;
    for (unsigned i = 0; i < record.trafficCount; ++i) {
        const E::EvidenceTraffic& copy = record.traffic[i];
        CHECK(copy.controllerEpoch == 1 && copy.accepted);
        if (copy.kind == E::DiagnosticKind::TX) {
            CHECK(!std::memcmp(copy.frame.data, rig.wire.sent[tx].data, 8));
            ++tx;
        } else if (copy.kind == E::DiagnosticKind::RX) {
            CHECK(copy.frame.id == 0x585 && copy.receivedTimeKnown &&
                  copy.receivedUs <= copy.processedUs);
            ++rx;
        } else if (copy.kind == E::DiagnosticKind::TX_REPORT)
            ++reports;
        else
            CHECK(false);
    }
    CHECK(tx == 9 && rx == 9 && reports == 9 && rig.wire.observed == 9);
    L::IdentityObservation observation;
    CHECK(rig.app.getIdentity(rig.now, 1000, observation) && !observation.modelMatched);
    const Saved<E::AcquisitionRecord> kept(record);
    const unsigned sent = rig.wire.transmitted, receives = rig.wire.receiveCalls;
    CHECK(rig.evidence.service(rig.now + 1));
    CHECK(!rig.evidence.interrupt(rig.now + 1));
    CHECK(rig.wire.transmitted == sent && rig.wire.receiveCalls == receives);
    kept.unchanged(*rig.evidence.record());
    CHECK(rig.evidence.release() && !rig.evidence.record());
    CHECK(rig.evidence.request(rig.plan(), rig.now) == E::EvidenceAdmission::INVALID);
    // Preflight may reuse the unoccupied record's storage, but failed app
    // admission must expose neither the old record nor the rejected candidate.
    E::SequenceId held;
    CHECK(rig.owner.beginSequence(rig.app.selected()->channel.binding, 2, rig.now, held));
    const Saved<L::ReadContext> empty(rig.app.readContext());
    const unsigned beforeRejected = rig.wire.transmitted;
    CHECK(rig.evidence.request(rig.plan(L::ReadKind::CONFIG, 2), rig.now) ==
          E::EvidenceAdmission::INVALID);
    CHECK(!rig.evidence.record() && !rig.evidence.release());
    CHECK(rig.wire.transmitted == beforeRejected && rig.owner.resources().reservations == 1);
    empty.unchanged(rig.app.readContext());
    CHECK(rig.owner.sequence(held) && rig.owner.releaseSequence(held));
    // Refusal consumed no acquisition identity; the same exact plan can start
    // once the other producer's reservation is released, without device I/O.
    CHECK(rig.evidence.request(rig.plan(L::ReadKind::CONFIG, 2), rig.now) ==
          E::EvidenceAdmission::ACCEPTED);
    rig.finish();
    L::ConfigObservation config;
    CHECK(rig.app.getConfig(rig.now, 1000, config) && config.producerTimeMs == 250);
    CHECK(rig.evidence.record()->intendedCount == 6 && rig.evidence.record()->terminalCount == 6);
    CHECK(rig.wire.transmitted == 15 && rig.evidence.release());
}
void failedEvidenceAndNoReplay() {
    Rig rejected;
    rejected.wire.mode = Wire::Mode::ABORT;
    rejected.wire.abortAt = 2;
    CHECK(rejected.evidence.request(rejected.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    rejected.finish();
    const E::AcquisitionRecord& abort = *rejected.evidence.record();
    CHECK(abort.read.state == M::ReadState::FAILED && abort.terminalCount == 3);
    CHECK(abort.read.values[0].valid && abort.read.values[1].valid && !abort.read.values[2].valid);
    const C::SdoCompletion& failure = abort.terminals[2].service;
    CHECK(failure.outcome == C::SdoOutcome::SERVER_ABORT && failure.abortValid &&
          failure.abortCode == 0xfedcba98u && failure.replyValid);
    CHECK(failure.reply.data[4] == 0x98 && failure.reply.data[7] == 0xfe);
    const Saved<E::AcquisitionRecord> retained(abort);
    for (unsigned i = 0; i < 3; ++i)
        rejected.turn(rejected.now + 1);
    retained.unchanged(*rejected.evidence.record());
    CHECK(rejected.wire.transmitted == 3 && rejected.evidence.release());
    Rig silent;
    silent.wire.mode = Wire::Mode::SILENT;
    CHECK(silent.evidence.request(silent.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    silent.finish();
    const E::AcquisitionRecord& timeout = *silent.evidence.record();
    CHECK(timeout.terminalCount == 1 && timeout.read.outcome == M::ReadOutcome::DEADLINE);
    CHECK(timeout.terminals[0].service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(timeout.read.uncertain && timeout.terminals[0].service.effectUncertain);
    CHECK(!timeout.terminals[0].service.replyValid && silent.wire.transmitted == 1);
    CHECK(silent.evidence.release());
    CHECK(silent.evidence.request(silent.plan(L::ReadKind::CONFIG, 2), silent.now) ==
          E::EvidenceAdmission::INVALID);
    CHECK(silent.wire.transmitted == 1); // Quarantine cannot be repaired by a new acquisition ID.
    Rig stale;
    stale.wire.mode = Wire::Mode::STALE;
    CHECK(stale.evidence.request(stale.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    stale.turn(1); // queued
    stale.turn(2); // crossed TX; reply capture zero predates the handoff
    stale.finish();
    const E::AcquisitionRecord& old = *stale.evidence.record();
    CHECK(old.read.state == M::ReadState::FAILED && !old.read.values[0].valid);
    CHECK(old.terminals[0].service.outcome == C::SdoOutcome::CORRELATION_LOST);
    CHECK(old.read.uncertain && stale.wire.transmitted == 1);
    CHECK(!old.evidenceLost && old.finalStats.rxLost == 0);
    bool rawStale = false;
    for (unsigned i = 0; i < old.trafficCount; ++i)
        if (old.traffic[i].kind == E::DiagnosticKind::RX) {
            rawStale = true;
            CHECK(old.traffic[i].receivedTimeKnown && old.traffic[i].receivedUs == 0 &&
                  !old.traffic[i].accepted && old.traffic[i].frame.id == 0x585);
        }
    CHECK(rawStale && stale.evidence.release());
}
void experimentsInterruptAndReset() {
    Rig rig(E::EvidenceEnvironment::DEVICE);
    rig.wire.mode = Wire::Mode::SILENT;
    const E::ReadExperiment review = rig.experiment();
    CHECK(rig.evidence.requestExperiment(review, 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(rig.evidence.record()->experimental && !rig.wire.transmitted);
    rig.turn(0);
    rig.turn(1);
    CHECK(rig.wire.transmitted == 1 && !rig.evidence.record()->terminal);
    const unsigned receives = rig.wire.receiveCalls;
    CHECK(rig.evidence.interrupt(2));
    const E::AcquisitionRecord& stopped = *rig.evidence.record();
    CHECK(stopped.terminal && stopped.interrupted && stopped.experimental);
    CHECK(stopped.environment == E::EvidenceEnvironment::DEVICE && stopped.read.uncertain);
    CHECK(stopped.terminals[0].service.outcome == C::SdoOutcome::CANCELLED);
    CHECK(stopped.terminals[0].service.transmissionPossible &&
          stopped.terminals[0].service.effectUncertain);
    CHECK(!std::memcmp(&stopped.review, &review, sizeof review));
    CHECK(rig.wire.receiveCalls == receives && rig.wire.transmitted == 1);
    const Saved<E::AcquisitionRecord> saved(stopped);
    rig.turn(3);
    saved.unchanged(*rig.evidence.record());
    CHECK(rig.evidence.release());
    CHECK(rig.evidence.request(rig.plan(L::ReadKind::CONFIG, 2), 3) ==
          E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
    Rig queued;
    CHECK(queued.evidence.request(queued.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    queued.turn(0);
    CHECK(queued.evidence.interrupt(1));
    CHECK(queued.evidence.record()->terminal && !queued.evidence.record()->read.uncertain &&
          !queued.wire.transmitted);
    Rig reboot;
    reboot.wire.mode = Wire::Mode::SILENT;
    CHECK(reboot.evidence.request(reboot.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    reboot.turn(0);
    reboot.turn(1);
    reboot.wire.push(frame(0x705, 1), 1); // boot-up byte zero, not an NMT command
    reboot.turn(2);
    CHECK(reboot.evidence.record()->terminal && reboot.evidence.record()->read.uncertain);
    CHECK(reboot.evidence.record()->terminals[0].service.outcome == C::SdoOutcome::REBOOT);
    CHECK(reboot.services.node(0)->route.generation != 1 && reboot.wire.transmitted == 1);
    CHECK(reboot.evidence.release());
    CHECK(reboot.evidence.request(reboot.plan(L::ReadKind::CONFIG, 2), 2) ==
          E::EvidenceAdmission::INVALID);
}
void interruptAfterReplyAndControllerLoss() {
    Rig prefix;
    CHECK(prefix.evidence.request(prefix.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    for (unsigned i = 0; i < 5 && !prefix.owner.result(prefix.app.pendingRequest()); ++i)
        prefix.turn(prefix.now + 1, false);
    CHECK(prefix.owner.result(prefix.app.pendingRequest()));
    CHECK(prefix.evidence.interrupt(prefix.now));
    const E::AcquisitionRecord& interrupted = *prefix.evidence.record();
    CHECK(interrupted.terminal && interrupted.interrupted && interrupted.terminalCount == 1);
    CHECK(interrupted.terminals[0].service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(interrupted.read.values[0].valid && !interrupted.read.values[1].valid &&
          interrupted.read.outcome == M::ReadOutcome::CANCELLED);
    prefix.turn(prefix.now + 1);
    CHECK(prefix.wire.transmitted ==
          1); // A won reply cannot admit the next field after interruption.
    Rig final;
    CHECK(final.evidence.request(final.plan(L::ReadKind::STATE), 0) ==
          E::EvidenceAdmission::ACCEPTED);
    for (unsigned i = 0; i < 10 && !final.evidence.record()->terminalCount; ++i)
        final.turn(final.now + 1);
    CHECK(final.evidence.record()->terminalCount == 1);
    for (unsigned i = 0; i < 5 && !final.owner.result(final.app.pendingRequest()); ++i)
        final.turn(final.now + 1, false);
    CHECK(final.owner.result(final.app.pendingRequest()));
    CHECK(final.evidence.interrupt(final.now));
    CHECK(final.evidence.record()->terminal && final.evidence.record()->interrupted &&
          final.evidence.record()->read.state == M::ReadState::SUCCEEDED &&
          final.evidence.record()->terminalCount == 2 && final.wire.transmitted == 2);
    Rig controller;
    CHECK(controller.evidence.request(controller.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    for (unsigned i = 0; i < 10 && !controller.evidence.record()->terminalCount; ++i)
        controller.turn(controller.now + 1);
    CHECK(controller.evidence.record()->terminalCount == 1);
    CHECK(controller.owner.reportController(C::ControllerState::BUS_OFF, controller.now));
    controller.turn(controller.now + 1);
    const E::AcquisitionRecord& failed = *controller.evidence.record();
    CHECK(failed.terminal && failed.read.outcome == M::ReadOutcome::INVALIDATED &&
          failed.terminalCount == 1 && failed.read.values[0].valid);
    bool retainedCause = false;
    for (unsigned i = 0; i < failed.trafficCount; ++i)
        if (failed.traffic[i].kind == E::DiagnosticKind::CONTROLLER) {
            retainedCause = true;
            CHECK(failed.traffic[i].controller == C::ControllerState::BUS_OFF);
        }
    CHECK(retainedCause && controller.wire.transmitted == 1);
}
void stateAndScope() {
    Rig rig(E::EvidenceEnvironment::DEVICE);
    E::ReadExperiment review = rig.experiment();
    review.plan = rig.plan(L::ReadKind::STATE);
    CHECK(rig.evidence.request(review.plan, 0) == E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
    CHECK(rig.evidence.requestExperiment(review, 0) == E::EvidenceAdmission::ACCEPTED);
    rig.finish();
    CHECK(rig.evidence.record()->intendedCount == 2 && rig.evidence.record()->experimental);
    CHECK(rig.evidence.record()->intended[0].index == 0x6041 &&
          rig.evidence.record()->intended[1].index == 0x6061);
    L::StateObservation observation;
    CHECK(rig.app.getStateBlock(L::StateBlock::STATUS, rig.now, 1000, observation));
    CHECK(!observation.interpreted);
    CHECK(rig.evidence.record()->read.values[0].raw == 0x8123 &&
          rig.evidence.record()->read.values[1].raw == 0xff);
    const Saved<L::StateObservation> saved(observation);
    CHECK(!rig.app.getStateBlock(L::StateBlock::FEEDBACK, rig.now, 1000, observation));
    saved.unchanged(observation);
    CHECK(rig.evidence.release());
    E::ReadExperiment configReview = rig.experiment();
    configReview.plan = rig.plan(L::ReadKind::CONFIG, 2);
    CHECK(rig.evidence.requestExperiment(configReview, rig.now) == E::EvidenceAdmission::ACCEPTED);
    rig.finish();
    CHECK(rig.evidence.record()->experimental && rig.evidence.record()->intendedCount == 6 &&
          rig.evidence.record()->terminalCount == 6);
    for (unsigned i = 0; i < 6; ++i) {
        const Field& field = configuration[i];
        CHECK(rig.evidence.record()->intended[i].index == field.index &&
              rig.evidence.record()->intended[i].subindex == field.subindex &&
              rig.evidence.record()->intended[i].widthBytes == field.width &&
              rig.evidence.record()->read.values[i].raw == field.raw);
        CHECK(rig.evidence.record()->terminals[i].service.effectPossible &&
              rig.evidence.record()->intended[i].readEffect == L::ReadEffect::UNQUALIFIED);
    }
    CHECK(rig.evidence.release());
    CHECK(rig.evidence.request(rig.plan(L::ReadKind::CONFIG, 3), rig.now) ==
          E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
    L::ReadTarget other = target();
    other.channel.binding.endpoint.profile = 2;
    other.channel.binding.bindingGeneration = 2;
    Rig second(E::EvidenceEnvironment::DEVICE, true, other);
    E::AcquisitionPlan wrong = second.plan();
    wrong.target = review.plan.target;
    CHECK(second.evidence.request(wrong, 0) == E::EvidenceAdmission::INVALID);
    CHECK(second.evidence.requestExperiment(review, 0) == E::EvidenceAdmission::INVALID);
    E::ReadExperiment distinct = second.experiment();
    label(distinct.plan.fixtureId, "synthetic-fixture-B");
    label(distinct.plan.firmware, "synthetic-firmware-B");
    CHECK(second.evidence.request(distinct.plan, 0) ==
          E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
    CHECK(second.evidence.requestExperiment(distinct, 0) == E::EvidenceAdmission::ACCEPTED);
    second.finish();
    CHECK(second.evidence.record()->plan.target.channel.binding.endpoint.profile == 2 &&
          std::strcmp(second.evidence.record()->plan.firmware, "synthetic-firmware-B") == 0);
    L::IdentityObservation identityValue;
    CHECK(second.app.getIdentity(second.now, 1000, identityValue) && !identityValue.modelMatched);
}
void boundedLossAndTime() {
    Rig rig;
    rig.wire.mode = Wire::Mode::SILENT;
    E::AcquisitionPlan plan = rig.plan();
    plan.options.responseTimeoutUs = 900;
    CHECK(rig.evidence.request(plan, 0) == E::EvidenceAdmission::ACCEPTED);
    rig.turn(0);
    rig.turn(1);
    for (unsigned turn = 2; turn < 20 && !rig.evidence.record()->terminal; ++turn) {
        for (unsigned i = 0; i < 7; ++i)
            rig.wire.push(frame(0x706, 1), rig.now);
        rig.turn(turn);
    }
    CHECK(rig.evidence.record()->terminal && rig.evidence.record()->interrupted &&
          rig.evidence.record()->evidenceLost && rig.evidence.record()->droppedTraffic > 0);
    CHECK(rig.evidence.record()->trafficCount == E::EVIDENCE_TRAFFIC && rig.wire.transmitted == 1);
    CHECK(rig.evidence.record()->read.uncertain && rig.evidence.record()->terminalCount == 1);
    Rig overwrite;
    overwrite.wire.mode = Wire::Mode::SILENT;
    CHECK(overwrite.evidence.request(overwrite.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    overwrite.turn(0);
    overwrite.turn(1);
    for (unsigned i = 0; i < 8; ++i)
        overwrite.wire.push(frame(0x706, 1), 1);
    overwrite.turn(2, false);
    for (unsigned i = 0; i < 8; ++i)
        overwrite.wire.push(frame(0x706, 1), 2);
    overwrite.turn(3, false);
    CHECK(overwrite.owner.stats().traceOverwritten > 0);
    CHECK(overwrite.evidence.service(3));
    CHECK(overwrite.evidence.record()->terminal && overwrite.evidence.record()->evidenceLost &&
          overwrite.evidence.record()->interrupted);
    const Saved<E::AcquisitionRecord> retained(*overwrite.evidence.record());
    const unsigned receives = overwrite.wire.receiveCalls;
    CHECK(!overwrite.evidence.service(2) && !overwrite.evidence.interrupt(2));
    retained.unchanged(*overwrite.evidence.record());
    CHECK(overwrite.wire.receiveCalls == receives && overwrite.wire.transmitted == 1);
    Rig deadline;
    E::AcquisitionPlan finite = deadline.plan();
    finite.deadlineUs = 1;
    CHECK(deadline.evidence.request(finite, 0) == E::EvidenceAdmission::ACCEPTED);
    deadline.turn(0);
    deadline.turn(1); // Overall cutoff wins before a queued frame can leave.
    CHECK(deadline.evidence.record()->terminal &&
          deadline.evidence.record()->read.outcome == M::ReadOutcome::DEADLINE &&
          !deadline.evidence.record()->read.uncertain && !deadline.wire.transmitted);
    Rig counter;
    for (unsigned i = 0; i < 8; ++i)
        counter.wire.push(frame(0x706, 1), 0);
    counter.turn(0, false);
    for (unsigned i = 0; i < 8; ++i)
        counter.wire.push(frame(0x706, 1), 0);
    counter.turn(1, false);
    CHECK(counter.owner.stats().traceOverwritten > 0);
    CHECK(counter.evidence.request(counter.plan(), 1) == E::EvidenceAdmission::ACCEPTED);
    CHECK(counter.owner.clearStats());
    CHECK(counter.evidence.service(1));
    CHECK(counter.evidence.record()->terminal && counter.evidence.record()->evidenceLost &&
          counter.evidence.record()->droppedTraffic > 0 && !counter.wire.transmitted);
    CHECK(counter.evidence.record()->initialStats.traceOverwritten > 0 &&
          counter.evidence.record()->finalStats.traceOverwritten == 0);
    Rig overflow;
    overflow.wire.mode = Wire::Mode::SILENT;
    CHECK(overflow.evidence.request(overflow.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    overflow.turn(0);
    overflow.turn(1);
    CHECK(overflow.owner.receiveOverflow(7, 3));
    overflow.turn(7);
    const E::AcquisitionRecord& missing = *overflow.evidence.record();
    CHECK(missing.terminal && missing.evidenceLost && missing.interrupted &&
          missing.droppedTraffic >= 3 && missing.read.uncertain);
    CHECK(missing.initialStats.rxLost == 0 && missing.finalStats.rxLost == 3 &&
          missing.finalStats.rxDropped == 3 && missing.maxServiceGapUs == 6);
    CHECK(missing.finalResources.inhibited && missing.terminalCount == 1 &&
          overflow.wire.transmitted == 1);
    bool overflowCause = false;
    for (unsigned i = 0; i < missing.trafficCount; ++i)
        if (missing.traffic[i].kind == E::DiagnosticKind::CONTROLLER) {
            overflowCause = true;
            CHECK(missing.traffic[i].controller == C::ControllerState::RECOVERING);
        }
    CHECK(overflowCause);
}
} // namespace
int main() {
    passiveAndAdmission();
    exactSuccessAndRetention();
    failedEvidenceAndNoReplay();
    experimentsInterruptAndReset();
    interruptAfterReplyAndControllerLoss();
    stateAndScope();
    boundedLossAndTime();
    std::printf("device_evidence: %u checks passed (synthetic only)\n", checks);
}
