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
namespace O = MotorControlCANOwner;
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
const Field otherState[] = {{0x1001, 0, 1, 0x81},       {0x603f, 0, 2, 0x7500},
                            {0x1003, 0, 1, 1},          {0x1003, 1, 4, 0x12345678},
                            {0x2100, 0, 2, 0x10},       {0x2101, 0, 2, 0x80},
                            {0x2102, 0, 2, 0x08},       {0x6064, 0, 4, 0x100},
                            {0x606c, 0, 4, 0},          {0x60fd, 0, 4, 0x40}};
// Reply values retained on two CL86-C specimens (docs/bench/20261102-nmt-pdo-node1/rx.txt and its
// node 2 record). A labelled fixture that contradicts the EDS: not a default or a capability.
const Field measured[] = {{0x1000, 0, 4, 0x00040192}, {0x1009, 0, 1, 0x0c}, {0x100a, 0, 1, 0x20},
                          {0x1018, 0, 1, 5},          {0x1018, 1, 4, 0x1000}, {0x1018, 2, 4, 1},
                          {0x1018, 3, 4, 0},          {0x2000, 0, 2, 0x0400}, {0x2001, 0, 2, 0x0200}};
uint8_t mask(L::StateBlock block) {
    return uint8_t(1u << unsigned(block));
}
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
    O::ReceivedFrame incoming[32];
    O::TxReport reports[4];
    M::Frame sent[32];
    unsigned rxHead = 0, rxCount = 0, reportHead = 0, reportCount = 0;
    unsigned transmitted = 0, receiveCalls = 0, observed = 0;
    unsigned abortAt = 0;
    const Field* identityFields = identity;
    bool interval = false; // Report a conservative ingress interval instead of an exact capture.
    void push(const M::Frame& value, uint64_t captured) {
        CHECK(rxCount < 32);
        O::ReceivedFrame& item = incoming[(rxHead + rxCount++) % 32];
        item.frame = value;
        item.receivedTimeKnown = !interval;
        item.receivedUs = interval ? 0 : captured;
        item.receiveBoundsKnown = interval;
        item.receivedEarliestUs = interval && captured >= 3 ? captured - 3 : 0;
        item.receivedLatestUs = interval ? captured : 0;
        item.controllerEpoch = 1;
    }
    static bool receive(void* context, O::ReceivedFrame& output) {
        Wire& wire = *static_cast<Wire*>(context);
        ++wire.receiveCalls;
        if (!wire.rxCount)
            return false;
        output = wire.incoming[wire.rxHead];
        wire.rxHead = (wire.rxHead + 1) % 32;
        --wire.rxCount;
        return true;
    }
    static bool transmit(void* context, const M::Frame& request, O::TransportTxId id, uint32_t epoch) {
        Wire& wire = *static_cast<Wire*>(context);
        CHECK(epoch == 1 && request.id == 0x605 && !request.extended && !request.rtr);
        // An upload request, or the client abort a typed width failure queues behind one.
        const bool clientAbort = request.data[0] == 0x80;
        CHECK(request.payloadLength == 8 && (request.data[0] == 0x40 || clientAbort));
        CHECK(wire.transmitted < 32 && wire.reportCount < 4);
        const uint16_t index = uint16_t(request.data[1]) | uint16_t(request.data[2]) << 8;
        const Field* selected = nullptr;
        for (unsigned i = 0; i < 9; ++i) {
            const Field& field = wire.identityFields[i];
            if (field.index == index && field.subindex == request.data[3])
                selected = &field;
        }
        for (const Field& field : configuration)
            if (field.index == index && field.subindex == request.data[3])
                selected = &field;
        for (const Field& field : status)
            if (field.index == index && field.subindex == request.data[3])
                selected = &field;
        for (const Field& field : otherState)
            if (field.index == index && field.subindex == request.data[3])
                selected = &field;
        CHECK(selected);
        const bool abort = wire.mode == Mode::ABORT && wire.transmitted == wire.abortAt;
        wire.sent[wire.transmitted++] = request;
        O::TxReport& report = wire.reports[(wire.reportHead + wire.reportCount++) % 4];
        report.id = id;
        report.controllerEpoch = epoch;
        report.transmitted = true;
        if (wire.mode != Mode::SILENT && !clientAbort) {
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
    static bool report(void* context, O::TxReport& output) {
        Wire& wire = *static_cast<Wire*>(context);
        if (!wire.reportCount)
            return false;
        output = wire.reports[wire.reportHead];
        wire.reportHead = (wire.reportHead + 1) % 4;
        --wire.reportCount;
        return true;
    }
    static void observe(void* context, const O::ReceivedFrame&, uint64_t) {
        ++static_cast<Wire*>(context)->observed;
    }
    O::Transport callbacks() {
        O::Transport result;
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
    O::CanBusOwner owner;
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
    // Identity is accepted under DEC-1 (acceptedPlansAndUnlistedPlans), so CONFIG now pins refusal.
    E::AcquisitionPlan plan = rig.plan();
    CHECK(rig.evidence.request(rig.plan(L::ReadKind::CONFIG), 0) ==
          E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
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
        CHECK(record.intended[i].readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED);
        const uint8_t request[] = {
            0x40, uint8_t(expected.index), uint8_t(expected.index >> 8), expected.subindex, 0, 0, 0,
            0};
        CHECK(!std::memcmp(rig.wire.sent[i].data, request, 8));
        const O::Completion& terminal = record.terminals[i];
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
        if (copy.kind == O::DiagnosticKind::TX) {
            CHECK(!std::memcmp(copy.frame.data, rig.wire.sent[tx].data, 8));
            ++tx;
        } else if (copy.kind == O::DiagnosticKind::RX) {
            CHECK(copy.frame.id == 0x585 && copy.receivedTimeKnown &&
                  copy.receivedUs <= copy.processedUs);
            ++rx;
        } else if (copy.kind == O::DiagnosticKind::TX_REPORT)
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
    O::SequenceId held;
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
    CHECK(timeout.terminalCount == 1 && timeout.read.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
    CHECK(timeout.terminals[0].service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(timeout.terminals[0].service.transfer.readEffect ==
          C::SdoReadEffect::ACCEPTED_UNQUALIFIED);
    CHECK(timeout.read.uncertain && timeout.terminals[0].service.effectUncertain);
    CHECK(timeout.terminals[0].service.effectPossible);
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
        if (old.traffic[i].kind == O::DiagnosticKind::RX) {
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
        if (failed.traffic[i].kind == O::DiagnosticKind::CONTROLLER) {
            retainedCause = true;
            CHECK(failed.traffic[i].controller == C::ControllerState::BUS_OFF);
        }
    CHECK(retainedCause && controller.wire.transmitted == 1);
}
void stateAndScope() {
    Rig rig(E::EvidenceEnvironment::DEVICE);
    E::ReadExperiment review = rig.experiment();
    review.plan = rig.plan(L::ReadKind::STATE);
    // 6041 and 6061 are both accepted, so this block no longer needs an experiment.
    CHECK(rig.evidence.request(review.plan, 0) == E::EvidenceAdmission::ACCEPTED);
    rig.finish();
    CHECK(rig.evidence.record()->intendedCount == 2 && !rig.evidence.record()->experimental);
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
    CHECK(second.evidence.request(second.plan(L::ReadKind::CONFIG), 0) ==
          E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
    // A review may still cover accepted objects, and it is recorded as an experiment.
    CHECK(second.evidence.requestExperiment(distinct, 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(second.evidence.record()->experimental);
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
    // Contract change (C-06): evidence loss no longer cancels the transmitted request. It stays
    // pending until its own terminal, here a response timeout, and the remainder never starts.
    CHECK(!rig.evidence.record()->terminal && rig.evidence.record()->interrupted &&
          rig.evidence.record()->evidenceLost && rig.evidence.record()->droppedTraffic > 0);
    CHECK(rig.evidence.record()->trafficCount == E::EVIDENCE_TRAFFIC && rig.wire.transmitted == 1);
    CHECK(rig.app.pendingRequest().owner && rig.evidence.record()->terminalCount == 0);
    rig.turn(1000);
    CHECK(rig.evidence.record()->terminal && rig.wire.transmitted == 1);
    CHECK(rig.evidence.record()->read.uncertain && rig.evidence.record()->terminalCount == 1);
    CHECK(rig.evidence.record()->terminals[0].service.outcome == C::SdoOutcome::TIMEOUT);
    Rig overwrite;
    overwrite.wire.mode = Wire::Mode::SILENT;
    CHECK(overwrite.evidence.request(overwrite.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    overwrite.turn(0);
    overwrite.turn(1);
    for (unsigned turn = 2; turn < 6; ++turn) {
        for (unsigned i = 0; i < 8; ++i)
            overwrite.wire.push(frame(0x706, 1), turn - 1);
        overwrite.turn(turn, false);
    }
    CHECK(overwrite.owner.stats().traceOverwritten > 0);
    CHECK(overwrite.evidence.service(5));
    CHECK(!overwrite.evidence.record()->terminal && overwrite.evidence.record()->evidenceLost &&
          overwrite.evidence.record()->interrupted);
    overwrite.turn(1000);
    CHECK(overwrite.evidence.record()->terminal && overwrite.evidence.record()->read.uncertain);
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
    for (unsigned turn = 0; turn < 4; ++turn) {
        for (unsigned i = 0; i < 8; ++i)
            counter.wire.push(frame(0x706, 1), 0);
        counter.turn(turn >= 2 ? 1 : turn, false);
    }
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
        if (missing.traffic[i].kind == O::DiagnosticKind::CONTROLLER) {
            overflowCause = true;
            CHECK(missing.traffic[i].controller == C::ControllerState::RECOVERING);
        }
    CHECK(overflowCause);
}
// C-06. The trace holds more than one turn produces, so RX at the budget plus a TX loses nothing.
void fullTurnFitsTheTrace() {
    Rig rig;
    CHECK(rig.evidence.request(rig.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    rig.turn(0); // The application admits its first read after this owner turn.
    CHECK(rig.wire.transmitted == 0);
    for (unsigned i = 0; i < O::CanBusOwner::RX_LIMIT; ++i)
        rig.wire.push(frame(0x706, 1), 0);
    rig.turn(1);
    CHECK(rig.wire.transmitted == 1 && rig.owner.stats().rxFrames == 8);
    CHECK(rig.owner.stats().traceOverwritten == 0);
    const E::AcquisitionRecord& record = *rig.evidence.record();
    CHECK(!record.evidenceLost && !record.interrupted && record.droppedTraffic == 0);
    unsigned rx = 0, tx = 0;
    for (unsigned i = 0; i < record.trafficCount; ++i) {
        rx += record.traffic[i].kind == O::DiagnosticKind::RX ? 1 : 0;
        tx += record.traffic[i].kind == O::DiagnosticKind::TX ? 1 : 0;
    }
    CHECK(rx == 8 && tx == 1);
    rig.finish();
    CHECK(record.read.state == M::ReadState::SUCCEEDED && !record.evidenceLost);
}
// C-06. A forced overflow is real evidence loss, but no reason to abandon a sent frame. Its reply
// settles it, only the unstarted remainder is withdrawn, and the route stays usable.
void traceOverflowLetsTheSentRequestSettle() {
    Rig rig;
    rig.wire.mode = Wire::Mode::SILENT;
    CHECK(rig.evidence.request(rig.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    rig.turn(0);
    rig.turn(1);
    CHECK(rig.wire.transmitted == 1 && rig.app.pendingRequest().owner);
    const O::RequestId sent = rig.app.pendingRequest();
    for (unsigned turn = 2; turn < 6; ++turn) {
        for (unsigned i = 0; i < 8; ++i)
            rig.wire.push(frame(0x706, 1), turn - 1);
        rig.turn(turn, false);
    }
    CHECK(rig.owner.stats().traceOverwritten > 0);
    CHECK(rig.evidence.service(5));
    const E::AcquisitionRecord& record = *rig.evidence.record();
    CHECK(record.evidenceLost && record.interrupted && !record.terminal);
    CHECK(rig.app.pendingRequest().generation == sent.generation && !rig.owner.result(sent));
    CHECK(!rig.services.unresolved(0) && record.terminalCount == 0);
    const M::Frame request = rig.wire.sent[0];
    CHECK(request.data[1] == 0x00 && request.data[2] == 0x10 && request.data[3] == 0);
    M::Frame reply = frame(0x585, 8);
    reply.data[0] = 0x43;
    std::memcpy(reply.data + 1, request.data + 1, 3);
    reply.data[4] = 0x78;
    reply.data[5] = 0x56;
    reply.data[6] = 0x34;
    reply.data[7] = 0x12;
    rig.wire.push(reply, 6);
    rig.turn(6);
    CHECK(record.terminal && record.terminalCount == 1);
    CHECK(record.terminals[0].service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(record.terminals[0].service.size == 4 && record.terminals[0].service.data[0] == 0x78);
    // The acquired field is kept. Local termination of an SDO read records uncertainty by rule.
    CHECK(record.read.outcome == M::ReadOutcome::CANCELLED && record.read.uncertain);
    CHECK(record.read.values[0].valid && record.read.values[0].raw == 0x12345678u &&
          !record.read.values[1].valid);
    CHECK(record.interrupted && record.evidenceLost);
    CHECK(rig.wire.transmitted == 1);
    rig.turn(7);
    CHECK(rig.wire.transmitted == 1 && !rig.services.unresolved(0));
    // Loss cost that record, not the route: the next acquisition runs on the same client.
    CHECK(rig.evidence.release());
    rig.wire.mode = Wire::Mode::SUCCESS;
    CHECK(rig.evidence.request(rig.plan(L::ReadKind::IDENTITY, 2), rig.now) ==
          E::EvidenceAdmission::ACCEPTED);
    rig.finish();
    CHECK(rig.evidence.record()->read.state == M::ReadState::SUCCEEDED);
    CHECK(rig.wire.transmitted == 10 && !rig.evidence.record()->evidenceLost);
}
// C-06. A field admitted but not started has no frame to settle, so loss withdraws it unsent.
void traceOverflowWithdrawsTheUnstartedField() {
    Rig rig;
    CHECK(rig.evidence.request(rig.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    rig.turn(0);
    const O::RequestId admitted = rig.app.pendingRequest();
    CHECK(admitted.owner && !rig.owner.mayHaveTransmitted(admitted) && rig.wire.transmitted == 0);
    O::ServiceBudget receiveOnly; // An owner turn that starts nothing still fills the trace.
    receiveOnly.rx = O::CanBusOwner::RX_LIMIT;
    for (unsigned turn = 1; turn < 5; ++turn) {
        for (unsigned i = 0; i < 8; ++i)
            rig.wire.push(frame(0x706, 1), turn - 1);
        CHECK(rig.owner.service(turn, receiveOnly));
    }
    CHECK(rig.owner.stats().traceOverwritten > 0 && rig.wire.transmitted == 0);
    CHECK(rig.evidence.service(4));
    CHECK(rig.owner.result(admitted));
    rig.turn(5);
    const E::AcquisitionRecord& record = *rig.evidence.record();
    CHECK(record.terminal && record.evidenceLost && record.interrupted && record.terminalCount == 1);
    CHECK(record.terminals[0].reason == O::CompletionReason::CANCELLED);
    CHECK(!record.terminals[0].service.transmissionPossible);
    rig.turn(6);
    CHECK(rig.wire.transmitted == 0 && !rig.services.unresolved(0));
}
void acceptedPlansAndUnlistedPlans() {
    const uint8_t status = mask(L::StateBlock::STATUS), monitors = mask(L::StateBlock::MONITORS),
                  feedback = mask(L::StateBlock::FEEDBACK), errors = mask(L::StateBlock::ERRORS),
                  history = mask(L::StateBlock::ERROR_HISTORY), inputs = mask(L::StateBlock::INPUTS);
    // Every object is on the DEC-1 list, so DEVICE admits these without an experiment.
    struct Accepted {
        L::ReadKind kind;
        uint8_t stateMask;
        unsigned uploads;
    };
    const Accepted admitted[] = {{L::ReadKind::IDENTITY, 0, 9},
                                 {L::ReadKind::STATE, status, 2},
                                 {L::ReadKind::STATE, monitors, 3},
                                 {L::ReadKind::STATE, feedback, 2},
                                 {L::ReadKind::STATE, uint8_t(status | monitors | feedback), 7}};
    for (const Accepted& item : admitted) {
        Rig rig(E::EvidenceEnvironment::DEVICE);
        E::AcquisitionPlan plan = rig.plan(item.kind);
        plan.stateMask = item.stateMask;
        CHECK(rig.evidence.request(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        CHECK(!rig.evidence.record()->experimental && !rig.wire.transmitted); // Not yet sent.
        rig.finish();
        const E::AcquisitionRecord& record = *rig.evidence.record();
        CHECK(record.read.state == M::ReadState::SUCCEEDED && !record.read.uncertain);
        CHECK(record.terminalCount == item.uploads && rig.wire.transmitted == item.uploads);
        for (unsigned i = 0; i < item.uploads; ++i) {
            CHECK(record.intended[i].readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED);
            const C::SdoCompletion& terminal = record.terminals[i].service;
            CHECK(terminal.transfer.readEffect == C::SdoReadEffect::ACCEPTED_UNQUALIFIED);
            CHECK(terminal.effectPossible && !terminal.effectUncertain);
        }
        CHECK(rig.services.nmtPort()->accepted == 0);
    }
    // One unlisted object refuses the whole plan: no partial acquisition of its accepted blocks.
    const uint8_t refused[] = {errors,
                               history,
                               inputs,
                               uint8_t(errors | status),
                               uint8_t(history | feedback),
                               uint8_t(inputs | monitors),
                               0x3f};
    for (const uint8_t selection : refused) {
        Rig rig(E::EvidenceEnvironment::DEVICE);
        E::AcquisitionPlan plan = rig.plan(L::ReadKind::STATE);
        plan.stateMask = selection;
        CHECK(rig.evidence.request(plan, 0) == E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
        CHECK(!rig.evidence.record() && rig.app.readContext().state == M::ReadState::EMPTY);
        rig.turn(1);
        CHECK(!rig.wire.transmitted && rig.owner.resources().reservations == 0);
    }
    Rig config(E::EvidenceEnvironment::DEVICE);
    CHECK(config.evidence.request(config.plan(L::ReadKind::CONFIG), 0) ==
          E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
    config.turn(1);
    CHECK(!config.wire.transmitted);
    // Accepted identity on an unconfirmed route starts without a grant. Unlisted work does not.
    Rig first(E::EvidenceEnvironment::DEVICE, false);
    CHECK(first.evidence.request(first.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    first.finish();
    CHECK(first.evidence.record()->read.state == M::ReadState::SUCCEEDED);
    CHECK(first.wire.transmitted == 9 && !first.services.unresolved(0));
    CHECK(!first.services.node(0)->nmtTime.present); // Confirmed, with no freshness claim.
    Rig unlisted(E::EvidenceEnvironment::HOST_FAKE, false);
    CHECK(unlisted.evidence.request(unlisted.plan(L::ReadKind::CONFIG), 0) ==
          E::EvidenceAdmission::ACCEPTED); // A fake harness admits locally; the owner still refuses.
    unlisted.finish();
    CHECK(!unlisted.wire.transmitted &&
          unlisted.evidence.record()->read.outcome == M::ReadOutcome::INVALIDATED);
}
void faultAndHistoryNeedAReviewedExperiment() {
    const L::StateBlock blocks[] = {L::StateBlock::ERRORS, L::StateBlock::ERROR_HISTORY};
    const unsigned uploads[] = {2, 2}; // 1001 and 603F; 1003:00 reporting one, then 1003:01.
    for (unsigned b = 0; b < 2; ++b) {
        Rig rig(E::EvidenceEnvironment::DEVICE);
        E::ReadExperiment review = rig.experiment();
        review.plan = rig.plan(L::ReadKind::STATE);
        review.plan.stateMask = mask(blocks[b]);
        CHECK(rig.evidence.request(review.plan, 0) ==
              E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
        CHECK(!rig.wire.transmitted);
        CHECK(rig.evidence.requestExperiment(review, 0) == E::EvidenceAdmission::ACCEPTED);
        rig.finish();
        const E::AcquisitionRecord& record = *rig.evidence.record();
        CHECK(record.experimental && record.read.state == M::ReadState::SUCCEEDED);
        CHECK(record.terminalCount == uploads[b] && rig.wire.transmitted == uploads[b]);
        for (unsigned i = 0; i < uploads[b]; ++i) {
            CHECK(record.terminals[i].service.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED);
            CHECK(record.terminals[i].service.effectPossible);
            CHECK(record.intended[i].readEffect == L::ReadEffect::UNQUALIFIED);
        }
        CHECK(rig.evidence.release());
        // A completed experiment grants nothing: repeating that plan still needs its own review,
        // so these objects cannot enter a polling loop through the ordinary path.
        E::AcquisitionPlan again = review.plan;
        again.id = review.plan.id + 1;
        CHECK(rig.evidence.request(again, rig.now) ==
              E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED);
        rig.turn(rig.now + 1);
        CHECK(rig.wire.transmitted == uploads[b] && !rig.evidence.record());
    }
}
void startupAndCachedInspectionEmitNoTraffic() {
    Rig rig(E::EvidenceEnvironment::DEVICE);
    for (uint64_t now = 0; now < 20; ++now)
        rig.turn(now);
    L::IdentityObservation identityValue;
    identityValue.modelCode = 0xa55a;
    const Saved<L::IdentityObservation> before(identityValue);
    CHECK(!rig.app.getIdentity(rig.now, 1000, identityValue));
    before.unchanged(identityValue);
    O::Health health;
    CHECK(rig.app.health(health));
    CHECK(!rig.wire.transmitted && !rig.services.port(0)->accepted);
    CHECK(!rig.services.nmtPort()->accepted && !rig.evidence.record());
    CHECK(rig.evidence.request(rig.plan(), rig.now) == E::EvidenceAdmission::ACCEPTED);
    rig.finish();
    CHECK(rig.wire.transmitted == 9);
    // Reading the cached observation, record and health repeatedly reaches no wire.
    const unsigned receives = rig.wire.receiveCalls;
    L::IdentityObservation observed;
    for (unsigned i = 0; i < 5; ++i) {
        CHECK(rig.app.getIdentity(rig.now, 1000, observed) && rig.evidence.record()->terminal);
        CHECK(rig.app.health(health));
    }
    CHECK(rig.wire.receiveCalls == receives);
    for (unsigned i = 0; i < 5; ++i)
        rig.turn(rig.now + 1);
    CHECK(rig.wire.transmitted == 9 && rig.services.port(0)->handedOff == 9);
    CHECK(!rig.services.nmtPort()->accepted);
    // Expiry is a getter failure, never a hidden refresh.
    const Saved<L::IdentityObservation> cached(observed);
    CHECK(!rig.app.getIdentity(rig.now + 1000, 1, observed));
    cached.unchanged(observed);
    CHECK(rig.wire.transmitted == 9);
    // Boot-up retires the cached evidence without a read or a reset being sent in response.
    rig.wire.push(frame(0x705, 1), rig.now);
    rig.turn(rig.now + 1);
    CHECK(rig.services.node(0)->route.generation != 1);
    CHECK(!rig.app.getIdentity(rig.now, 100000, observed));
    cached.unchanged(observed);
    CHECK(rig.wire.transmitted == 9 && !rig.services.nmtPort()->accepted);
}
void measuredWidthsThroughTheApplication() {
    // Selected explicitly by a plan that has established this fixture. Nothing detects it.
    Rig rig(E::EvidenceEnvironment::DEVICE);
    rig.wire.identityFields = measured;
    E::AcquisitionPlan plan = rig.plan();
    plan.options.identityWidths = L::IdentityWidths::MEASURED_U8;
    CHECK(rig.evidence.request(plan, 0) == E::EvidenceAdmission::ACCEPTED);
    rig.finish();
    const E::AcquisitionRecord& record = *rig.evidence.record();
    CHECK(record.read.state == M::ReadState::SUCCEEDED && rig.wire.transmitted == 9);
    CHECK(record.plan.options.identityWidths == L::IdentityWidths::MEASURED_U8);
    CHECK(record.intended[1].widthBytes == 1 && record.intended[2].widthBytes == 1);
    const uint8_t request1009[8] = {0x40, 0x09, 0x10, 0x00, 0, 0, 0, 0};
    const uint8_t reply1009[8] = {0x4f, 0x09, 0x10, 0x00, 0x0c, 0, 0, 0};
    const uint8_t reply100a[8] = {0x4f, 0x0a, 0x10, 0x00, 0x20, 0, 0, 0};
    const uint8_t reply1018[8] = {0x4f, 0x18, 0x10, 0x00, 0x05, 0, 0, 0};
    CHECK(!std::memcmp(rig.wire.sent[1].data, request1009, 8));
    CHECK(!std::memcmp(record.terminals[1].service.reply.data, reply1009, 8));
    CHECK(!std::memcmp(record.terminals[2].service.reply.data, reply100a, 8));
    CHECK(!std::memcmp(record.terminals[3].service.reply.data, reply1018, 8));
    L::IdentityObservation observed;
    CHECK(rig.app.getIdentity(rig.now, 1000, observed));
    CHECK(observed.deviceType == 0x00040192u);
    CHECK(observed.hardwareVersion == 0x0c && observed.hardwareVersionWidth == 1);
    CHECK(observed.softwareVersion == 0x20 && observed.softwareVersionWidth == 1);
    CHECK(observed.identityEntries == 5 && observed.vendorId == 0x1000 && !observed.modelMatched);
    // The default on the same specimen replies fails at 1009, with the raw reply retained.
    Rig strict(E::EvidenceEnvironment::DEVICE);
    strict.wire.identityFields = measured;
    CHECK(strict.evidence.request(strict.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    strict.finish();
    const E::AcquisitionRecord& failed = *strict.evidence.record();
    CHECK(failed.read.state == M::ReadState::FAILED && failed.terminalCount == 2);
    CHECK(failed.read.outcome == M::ReadOutcome::REPLY_ERROR && failed.read.uncertain);
    const C::SdoCompletion& width = failed.terminals[1].service;
    CHECK(width.outcome == C::SdoOutcome::CLIENT_ABORT && width.abortCode == 0x06070010u);
    CHECK(width.replyValid && !std::memcmp(width.reply.data, reply1009, 8));
    L::IdentityObservation none;
    none.modelCode = 0xa55a;
    const Saved<L::IdentityObservation> unchanged(none);
    CHECK(!strict.app.getIdentity(strict.now, 1000, none));
    unchanged.unchanged(none);
    // An option on any other kind is invalid before a plan could reach the application.
    Rig other(E::EvidenceEnvironment::DEVICE);
    E::AcquisitionPlan state = other.plan(L::ReadKind::STATE);
    state.options.identityWidths = L::IdentityWidths::MEASURED_U8;
    CHECK(other.evidence.request(state, 0) == E::EvidenceAdmission::INVALID);
    E::AcquisitionPlan unknown = other.plan();
    unknown.options.identityWidths = static_cast<L::IdentityWidths>(2);
    CHECK(other.evidence.request(unknown, 0) == E::EvidenceAdmission::INVALID);
    // The array-count width belongs to two group reads, none of which this harness plans.
    const L::ReadKind kinds[] = {L::ReadKind::IDENTITY, L::ReadKind::STATE, L::ReadKind::CONFIG};
    for (L::ReadKind kind : kinds) {
        E::AcquisitionPlan counted = other.plan(kind);
        counted.options.countWidths = L::CountWidths::MEASURED_U16;
        CHECK(other.evidence.request(counted, 0) == E::EvidenceAdmission::INVALID);
    }
    CHECK(!other.evidence.record() && !other.wire.transmitted);
}
void receiveIntervalsStayIntervals() {
    // The ESP32-S2 platform reports a completed-ingress interval, not a capture timestamp
    // (see exact=0 bounds=1 in docs/bench/20261102-nmt-pdo-node1/rx.txt).
    Rig rig;
    rig.wire.interval = true;
    E::AcquisitionPlan plan = rig.plan();
    CHECK(rig.evidence.request(plan, 0) == E::EvidenceAdmission::ACCEPTED);
    rig.turn(10);
    rig.finish();
    const E::AcquisitionRecord& record = *rig.evidence.record();
    CHECK(record.read.state == M::ReadState::SUCCEEDED && record.terminalCount == 9);
    unsigned rx = 0;
    for (unsigned i = 0; i < record.trafficCount; ++i) {
        const E::EvidenceTraffic& copy = record.traffic[i];
        if (copy.kind != O::DiagnosticKind::RX)
            continue;
        ++rx;
        CHECK(copy.accepted && !copy.receivedTimeKnown && copy.receivedUs == 0);
        CHECK(copy.receiveBoundsKnown && copy.receivedLatestUs - copy.receivedEarliestUs == 3);
        CHECK(copy.receivedLatestUs <= copy.processedUs);
    }
    CHECK(rx == 9);
    L::IdentityObservation observed;
    CHECK(rig.app.getIdentity(rig.now, 1000, observed));
    for (unsigned i = 0; i < 9; ++i) {
        const L::ReadProvenance& field = observed.fields[i];
        const C::SdoCompletion& terminal = record.terminals[i].service;
        // Service admission and completion bound the observation. Neither is called its capture.
        CHECK(!field.captureKnown && field.source == L::ReadSource::SDO);
        CHECK(field.evidenceId == 0 && field.mappingGeneration == 0);
        CHECK(field.earliestUs == terminal.admittedUs && field.latestUs == terminal.completedUs);
        CHECK(field.earliestUs < field.latestUs);
    }
    CHECK(observed.window.earliestUs == observed.fields[0].earliestUs);
    CHECK(observed.window.latestUs == observed.fields[8].latestUs);
    // Contract change (bounds qualification): this fake's interval reaches back to its own
    // handoff, so each reply settles its read but records no SDO or service evidence. An
    // interval used to leave a timeless record here. The external premise stays as configured.
    const C::NodeSnapshot& node = *rig.services.node(0);
    CHECK(!node.sdo.present && !node.nmtTime.present && !node.serviceFromSdo);
    CHECK(node.serviceQualification.receivedTimeKnown && node.serviceQualification.receivedUs == 0);
    // An interval wholly before the handoff is old traffic, exactly as an old timestamp would be.
    Rig stale;
    stale.wire.interval = true;
    stale.wire.mode = Wire::Mode::STALE;
    CHECK(stale.evidence.request(stale.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    stale.turn(10);
    stale.turn(11);
    stale.finish();
    const E::AcquisitionRecord& old = *stale.evidence.record();
    CHECK(old.terminals[0].service.outcome == C::SdoOutcome::CORRELATION_LOST);
    CHECK(old.read.state == M::ReadState::FAILED && !old.read.values[0].valid && old.read.uncertain);
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
    fullTurnFitsTheTrace();
    traceOverflowLetsTheSentRequestSettle();
    traceOverflowWithdrawsTheUnstartedField();
    acceptedPlansAndUnlistedPlans();
    faultAndHistoryNeedAReviewedExperiment();
    startupAndCachedInspectionEmitNoTraffic();
    measuredWidthsThroughTheApplication();
    receiveIntervalsStayIntervals();
    std::printf("device_evidence: %u checks passed (synthetic only)\n", checks);
}
