// SPDX-License-Identifier: MIT
// The real service engine and single owner drive a bounded synthetic TPDO dictionary.
#include "PdoExperiment.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
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
template <size_t N> void label(char (&out)[N], const char* value) {
    CHECK(std::strlen(value) < N);
    std::strcpy(out, value);
}
L::ReadTarget target() {
    L::ReadTarget t;
    auto& b = t.channel.binding;
    b.logicalAxisId = 101;
    b.endpoint.profile = 1;
    b.endpoint.address = 1;
    b.bindingGeneration = b.configurationGeneration = 1;
    t.channel.clientToServerId = 0x601;
    t.channel.serverToClientId = 0x581;
    t.nodeGeneration = 1;
    return t;
}
struct Wire {
    enum class Failure { NONE, ABORT, SILENT, BOOTUP, READBACK };
    Failure failure = Failure::NONE;
    unsigned failureAt = 0, attempts = 0, transmitted = 0, receives = 0, downloads = 0;
    uint64_t now = 0;
    uint32_t values[9] = {0x181, 255, 100, 0, 3, 0x60410010, 0x60610008, 0x60640020, 0};
    E::ReceivedFrame rx[16];
    E::TxReport reports[8];
    M::Frame sent[32];
    unsigned rxHead = 0, rxCount = 0, reportHead = 0, reportCount = 0;
    bool holdReports = false;
    void push(const M::Frame& f) {
        CHECK(rxCount < 16);
        auto& out = rx[(rxHead + rxCount++) % 16];
        out.frame = f;
        out.receivedTimeKnown = true;
        out.receivedUs = now;
        out.controllerEpoch = 1;
    }
    void heartbeat(uint8_t state = 0x7f) {
        M::Frame f;
        f.id = 0x701;
        f.dlc = f.payloadLength = 1;
        f.data[0] = state;
        push(f);
    }
    static bool receive(void* ptr, E::ReceivedFrame& out) {
        auto& w = *static_cast<Wire*>(ptr);
        ++w.receives;
        if (!w.rxCount)
            return false;
        out = w.rx[w.rxHead];
        w.rxHead = (w.rxHead + 1) % 16;
        --w.rxCount;
        return true;
    }
    static bool transmit(void* ptr, const M::Frame& f, E::TransportTxId id, uint32_t epoch) {
        auto& w = *static_cast<Wire*>(ptr);
        CHECK(f.id == 0x601 && f.payloadLength == 8 && epoch == 1 && w.transmitted < 32 &&
              w.reportCount < 8);
        w.sent[w.transmitted++] = f;
        auto& report = w.reports[(w.reportHead + w.reportCount++) % 8];
        report.id = id;
        report.controllerEpoch = epoch;
        report.transmitted = true;
        if (f.data[0] == 0x80)
            return true;
        CHECK(f.data[0] == 0x40 || f.data[0] == 0x23 || f.data[0] == 0x2f);
        ++w.attempts;
        const uint16_t index = uint16_t(f.data[1]) | uint16_t(f.data[2]) << 8;
        const uint8_t sub = f.data[3];
        unsigned slot = 99, width = 4;
        if (index == 0x1800) {
            if (sub == 1)
                slot = 0;
            else if (sub == 2) {
                slot = 1;
                width = 1;
            } else if (sub == 3) {
                slot = 2;
                width = 2;
            } else if (sub == 5) {
                slot = 3;
                width = 2;
            }
        } else if (index == 0x1a00 && sub <= 4) {
            slot = 4 + sub;
            if (!sub)
                width = 1;
        }
        CHECK(slot < 9);
        const Failure failure = w.failureAt == w.attempts ? w.failure : Failure::NONE;
        if (failure == Failure::SILENT)
            return true;
        if (failure == Failure::BOOTUP) {
            w.heartbeat(0);
            return true;
        }
        M::Frame response;
        response.id = 0x581;
        response.dlc = response.payloadLength = 8;
        response.data[1] = f.data[1];
        response.data[2] = f.data[2];
        response.data[3] = sub;
        if (failure == Failure::ABORT) {
            response.data[0] = 0x80;
            response.data[4] = 0x30;
            response.data[5] = 0;
            response.data[6] = 9;
            response.data[7] = 6;
        } else if (f.data[0] == 0x40) {
            response.data[0] = uint8_t(0x43u | ((4u - width) << 2));
            uint32_t value = w.values[slot];
            if (failure == Failure::READBACK)
                value ^= 1;
            for (unsigned i = 0; i < width; ++i)
                response.data[4 + i] = uint8_t(value >> (8 * i));
        } else {
            ++w.downloads;
            uint32_t value = 0;
            for (unsigned i = 0; i < width; ++i)
                value |= uint32_t(f.data[4 + i]) << (8 * i);
            // Literal documentary order: map changes require disabled PDO/count0.
            if (index == 0x1a00) {
                CHECK(w.values[0] & 0x80000000u);
                if (sub)
                    CHECK(w.values[4] == 0);
            }
            w.values[slot] = value;
            response.data[0] = 0x60;
        }
        w.push(response);
        return true;
    }
    static bool report(void* ptr, E::TxReport& out) {
        auto& w = *static_cast<Wire*>(ptr);
        if (!w.reportCount || w.holdReports)
            return false;
        out = w.reports[w.reportHead];
        w.reportHead = (w.reportHead + 1) % 8;
        --w.reportCount;
        return true;
    }
    E::Transport callbacks() {
        E::Transport t;
        t.context = this;
        t.receive = receive;
        t.transmit = transmit;
        t.takeTxReport = report;
        return t;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    E::CanBusOwner owner;
    E::PdoExperiment experiment;
    uint64_t now = 0;
    Rig() : owner(services, wire.callbacks(), 42), experiment(owner, services) {
        const auto t = target();
        CHECK(services.configure(0, t.channel, false, true));
        C::NetworkRoute r;
        r.binding = t.channel.binding;
        r.generation = 1;
        r.heartbeatId = 0x701;
        r.emcyId = 0x81;
        CHECK(services.configureNetwork(0, r, C::NetworkObserver(), 0));
        CHECK(services.setServiceAge(0, 1000000, 0));
    }
    E::PdoExperimentPlan plan() {
        E::PdoExperimentPlan p;
        auto& r = p.review;
        auto& a = r.plan;
        a.id = 3000;
        a.target = target();
        a.deadlineUs = 5000000;
        a.options.queueTimeoutUs = a.options.responseTimeoutUs = 250000;
        label(a.fixtureId, "synthetic-only");
        label(a.softwareRevision, "0123456789abcdef0123456789abcdef01234567");
        label(a.firmware, "raw-synthetic");
        label(a.dialect, "raw-unresolved");
        label(a.authorization, "offline finite experiment");
        label(a.observations, "synthetic owner trace");
        label(r.sourceReview, "EN86 p16/EDS plus conditional CiA procedure");
        label(r.independentReview, "synthetic exact vectors");
        label(r.fixtureReview, "no hardware");
        label(r.independentControl, "synthetic independent control");
        label(r.failurePlan, "stop and retain; no replay");
        label(r.reconciliation, "exact readback and retained uncertainty");
        auto& q = p.configurationReview;
        q.target = target();
        q.scope = L::PdoConfigurationScope::ISOLATED_EXPERIMENT;
        q.fixtureRecord = 1;
        q.firmwareRecord = 2;
        q.procedureRecord = 3;
        q.authorizationRecord = 4;
        p.firstAuthorizationId = 3000;
        p.maximumAgeUs = 1000000;
        return p;
    }
    void turn(uint64_t at) {
        now = wire.now = at;
        E::ServiceBudget b;
        b.rx = 8;
        b.tx = 2;
        b.events = 8;
        b.starts = 2;
        CHECK(owner.service(now, b));
        const unsigned tx = wire.transmitted, rx = wire.receives;
        CHECK(experiment.service(now));
        CHECK(wire.transmitted == tx && wire.receives == rx);
    }
    void start() {
        CHECK(experiment.begin(plan(), 0) == E::EvidenceAdmission::ACCEPTED);
        wire.now = 1;
        wire.heartbeat();
        turn(1);
    }
    void finish() {
        for (unsigned i = 0; i < 100 && !experiment.record()->terminal; ++i)
            turn(now + 1);
        CHECK(experiment.record()->terminal);
    }
};
void passiveHappyAndReservation() {
    Rig r;
    CHECK(!r.experiment.record() && !r.wire.transmitted && !r.wire.receives);
    CHECK(r.experiment.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(!r.wire.transmitted && r.owner.resources().reservations == 1);
    CHECK(r.experiment.begin(r.plan(), 0) == E::EvidenceAdmission::BUSY);
    E::SequenceId other;
    CHECK(!r.owner.beginSequence(target().channel.binding, 1, 0, other));
    r.turn(1);
    CHECK(!r.wire.transmitted); // No heartbeat/state manufactured from elapsed time.
    r.wire.now = 2;
    r.wire.heartbeat();
    r.turn(2);
    r.finish();
    const auto* out = r.experiment.record();
    CHECK(out->failure == E::PdoExperimentFailure::NONE && out->terminalCount == 24 &&
          out->attemptedCount == 24);
    CHECK(out->configurationPrepared && out->configurationMigrated &&
          out->configuration.mapConfirmed);
    CHECK(out->configuration.review.scope == L::PdoConfigurationScope::ISOLATED_EXPERIMENT);
    CHECK(out->configuration.acknowledgedMask == 0x3f && !out->configuration.uncertain);
    CHECK(out->current.configurationGeneration == 2 &&
          r.services.channel(0)->binding.configurationGeneration == 2);
    CHECK(r.wire.downloads == 6 && r.wire.values[0] == 0x181 && r.wire.values[4] == 2);
    CHECK(r.wire.values[6] == 0x60640020 && r.wire.values[7] == 0x60640020 &&
          r.wire.values[8] == 0);
    CHECK(!out->evidenceLost && !out->tailOverflow);
    for (unsigned i = 0; i < 24; ++i) {
        const auto& t = out->terminals[i].service.transfer;
        CHECK(t.uploadSize == C::SdoUploadSize::EXACT && t.operation == (i < 9 ? 3000u : 3001u));
        CHECK(t.channel.binding.configurationGeneration == (i < 15 ? 1u : 2u));
        CHECK(t.index == 0x1800 || t.index == 0x1a00); // No motion/NMT/SYNC/save.
    }
    // Exact experimental grants remain required; success does not authorize arbitrary writes.
    C::SdoTransfer forbidden = out->terminals[0].service.transfer;
    forbidden.operation++;
    forbidden.request++;
    forbidden.index = 0x6040;
    forbidden.direction = C::SdoDirection::DOWNLOAD;
    forbidden.size = 2;
    CHECK(!r.services.checkAdmission(0, forbidden, r.now));
    r.turn(r.now + 1);
    CHECK(out->trafficCount + out->tailCount == 73); // Includes the opening observed heartbeat.
    CHECK(r.experiment.release());
    CHECK(!r.experiment.record());
    CHECK(r.experiment.begin(r.plan(), r.now) ==
          E::EvidenceAdmission::INVALID); // No ID reuse/replay.
}
void failureAtEveryStep() {
    for (unsigned at = 1; at <= 24; ++at) {
        Rig r;
        r.wire.failure = Wire::Failure::ABORT;
        r.wire.failureAt = at;
        r.start();
        r.finish();
        const auto* out = r.experiment.record();
        if (out->failure != E::PdoExperimentFailure::SERVICE || out->terminalCount != at)
            std::fprintf(stderr, "abortAt=%u failure=%u terminals=%u core=%u\n", at,
                         unsigned(out->failure), out->terminalCount,
                         unsigned(out->configuration.state));
        CHECK(out->failure == E::PdoExperimentFailure::SERVICE && out->terminalCount == at);
        CHECK(r.wire.attempts == at && !out->configuration.mapConfirmed);
        CHECK(out->terminals[at - 1].service.abortCode == 0x06090030u);
        if (at > 9)
            CHECK(out->configuration.uncertain);
        CHECK(r.experiment.release());
    }
    for (unsigned at = 16; at <= 24; ++at) {
        Rig r;
        r.wire.failure = Wire::Failure::READBACK;
        r.wire.failureAt = at;
        r.start();
        r.finish();
        CHECK(r.experiment.record()->failure == E::PdoExperimentFailure::READBACK);
        CHECK(r.experiment.record()->configuration.uncertain &&
              !r.experiment.record()->configuration.mapConfirmed);
        CHECK(r.wire.attempts == at && r.experiment.release());
    }
}
void lifecycleDeadlineAndRetirement() {
    Rig r;
    r.wire.failure = Wire::Failure::BOOTUP;
    r.wire.failureAt = 11;
    r.start();
    r.finish();
    CHECK(r.experiment.record()->failure == E::PdoExperimentFailure::INVALIDATED);
    CHECK(r.experiment.record()->configuration.uncertain &&
          !r.experiment.record()->configuration.mapConfirmed);
    CHECK(r.experiment.release());
    Rig stalled;
    CHECK(stalled.experiment.begin(stalled.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    stalled.turn(5000000);
    CHECK(stalled.experiment.record()->failure == E::PdoExperimentFailure::DEADLINE);
    CHECK(!stalled.wire.transmitted && stalled.experiment.release());
    Rig held;
    held.start();
    held.wire.holdReports = true;
    held.turn(2);
    held.turn(3);
    CHECK(held.wire.attempts == 1 && held.experiment.record()->terminalCount == 1);
    held.turn(4);
    CHECK(held.wire.attempts == 1); // Matched reply cannot retire outstanding actual TX.
    held.wire.holdReports = false;
    held.turn(5);
    held.finish();
    CHECK(held.experiment.record()->failure == E::PdoExperimentFailure::NONE &&
          held.experiment.release());
    Rig old;
    old.start();
    old.wire.heartbeat(5);
    old.turn(2);
    old.finish();
    CHECK(old.experiment.record()->failure == E::PdoExperimentFailure::INVALIDATED);
    CHECK(old.wire.downloads == 0 && old.experiment.release());
}
void planRejections() {
    for (unsigned i = 0; i < 7; ++i) {
        Rig r;
        auto p = r.plan();
        if (i == 0)
            p.firstAuthorizationId = UINT32_MAX;
        if (i == 1)
            p.review.plan.target.channel.binding.endpoint.address = 2;
        if (i == 2)
            p.configurationReview.scope = L::PdoConfigurationScope::FIXTURE_QUALIFIED;
        if (i == 3)
            p.configurationReview.firmwareRecord = 0;
        if (i == 4)
            p.maximumAgeUs = 0;
        if (i == 5)
            p.review.plan.id = UINT32_MAX;
        if (i == 6)
            p.review.independentControl[0] = 0;
        CHECK(r.experiment.begin(p, 0) == E::EvidenceAdmission::INVALID);
        CHECK(!r.experiment.record() && !r.wire.transmitted &&
              r.owner.resources().reservations == 0);
    }
}
void operationalBetweenAdmissionAndOwnerHandoff() {
    Rig r;
    r.start();
    for (unsigned i = 0; i < 100 && r.experiment.record()->terminalCount < 9; ++i)
        r.turn(r.now + 1);
    CHECK(r.experiment.record()->terminalCount == 9 &&
          r.experiment.record()->configurationPrepared);
    CHECK(r.experiment.record()->attemptedCount == 10 && r.wire.downloads == 0);
    r.wire.now = r.now + 1;
    r.wire.heartbeat(5);
    r.turn(r.now + 1);
    r.finish();
    CHECK(r.wire.downloads == 0 && r.wire.attempts == 9);
    CHECK(r.experiment.record()->failure == E::PdoExperimentFailure::INVALIDATED);
    CHECK(r.experiment.record()->configuration.uncertain);
    CHECK(!r.experiment.record()->configuration.mapConfirmed && r.experiment.release());
}
} // namespace
void ageExpiresBetweenAdmissionAndOwnerHandoff() {
    Rig r;
    CHECK(r.services.setServiceAge(0, 50, 0));
    auto p = r.plan();
    p.maximumAgeUs = 50;
    CHECK(r.experiment.begin(p, 0) == E::EvidenceAdmission::ACCEPTED);
    r.wire.now = 1;
    r.wire.heartbeat();
    r.turn(1);
    for (unsigned i = 0; i < 100 && r.experiment.record()->terminalCount < 9; ++i)
        r.turn(r.now + 1);
    CHECK(r.experiment.record()->terminalCount == 9 && r.experiment.record()->attemptedCount == 10);
    CHECK(r.wire.downloads == 0);
    r.turn(52);
    r.finish();
    CHECK(r.wire.downloads == 0 && r.wire.attempts == 9);
    CHECK(r.experiment.record()->failure == E::PdoExperimentFailure::INVALIDATED);
    CHECK(r.experiment.record()->configuration.uncertain && r.experiment.release());
    Rig broad;
    CHECK(broad.services.setServiceAge(0, 1000001, 0));
    CHECK(broad.experiment.begin(broad.plan(), 0) == E::EvidenceAdmission::INVALID);
    CHECK(!broad.experiment.record() && broad.owner.resources().reservations == 0);
}
int main() {
    passiveHappyAndReservation();
    failureAtEveryStep();
    lifecycleDeadlineAndRetirement();
    planRejections();
    operationalBetweenAdmissionAndOwnerHandoff();
    ageExpiresBetweenAdmissionAndOwnerHandoff();
    std::printf("pdo_experiment_test: %u checks passed\n", checks);
}
