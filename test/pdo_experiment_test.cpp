// SPDX-License-Identifier: MIT
// The real service engine and single owner drive a bounded synthetic TPDO dictionary on two nodes.
#include "PdoExperiment.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
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
L::ReadTarget target(unsigned node = 1) {
    L::ReadTarget t;
    auto& b = t.channel.binding;
    b.logicalAxisId = 100 + node;
    b.endpoint.profile = 1;
    b.endpoint.address = node;
    b.bindingGeneration = b.configurationGeneration = 1;
    t.channel.clientToServerId = 0x600 + node;
    t.channel.serverToClientId = 0x580 + node;
    t.nodeGeneration = 1;
    return t;
}
// Two TPDO1 dictionaries in the measured specimens' state, answering on 0x580 + node.
struct Wire {
    E::PdoExperiment* experiment = nullptr;
    unsigned guardCalls = 0;
    // SILENT: no effect and no reply. LOST: the write takes effect, but its reply never arrives.
    enum class Failure { NONE, ABORT, SILENT, LOST, BOOTUP, READBACK };
    Failure failure = Failure::NONE;
    unsigned failureNode = 1, failureAt = 0, transmitted = 0, receives = 0;
    unsigned attempts[3] = {}, downloads[3] = {};
    uint64_t now = 0;
    uint32_t values[3][9] = {{},
                             {0x181, 255, 100, 0, 3, 0x60410010, 0x60610008, 0x60640020, 0},
                             {0x182, 255, 100, 0, 3, 0x60410010, 0x60610008, 0x60640020, 0}};
    O::ReceivedFrame rx[32];
    O::TxReport reports[8];
    M::Frame sent[96];
    unsigned rxHead = 0, rxCount = 0, reportHead = 0, reportCount = 0;
    bool holdReports = false;
    void push(const M::Frame& f) {
        CHECK(rxCount < 32);
        auto& out = rx[(rxHead + rxCount++) % 32];
        out.frame = f;
        out.receivedTimeKnown = true;
        out.receivedUs = now;
        out.controllerEpoch = 1;
    }
    void heartbeat(uint8_t state = 0x7f, unsigned node = 1) {
        M::Frame f;
        f.id = 0x700 + node;
        f.dlc = f.payloadLength = 1;
        f.data[0] = state;
        push(f);
    }
    // One TPDO1 frame under the count-3 map: 6041 0037, 6061 01, 6064 4728 (the 1314Z payload).
    void tpdo(unsigned node) {
        const uint8_t bytes[7] = {0x37, 0x00, 0x01, 0x78, 0x12, 0x00, 0x00};
        M::Frame f;
        f.id = 0x180 + node;
        f.dlc = f.payloadLength = 7;
        std::memcpy(f.data, bytes, 7);
        push(f);
    }
    void unrelated() {
        M::Frame f;
        f.id = 0x2a0;
        f.dlc = f.payloadLength = 8;
        push(f);
    }
    static bool receive(void* ptr, O::ReceivedFrame& out) {
        auto& w = *static_cast<Wire*>(ptr);
        ++w.receives;
        if (!w.rxCount)
            return false;
        out = w.rx[w.rxHead];
        w.rxHead = (w.rxHead + 1) % 32;
        --w.rxCount;
        return true;
    }
    static bool transmit(void* ptr, const M::Frame& f, O::TransportTxId id, uint32_t epoch) {
        auto& w = *static_cast<Wire*>(ptr);
        const unsigned node = f.id - 0x600;
        CHECK((node == 1 || node == 2) && f.payloadLength == 8 && epoch == 1 &&
              w.transmitted < 96 && w.reportCount < 8);
        w.sent[w.transmitted++] = f;
        auto& report = w.reports[(w.reportHead + w.reportCount++) % 8];
        report.id = id;
        report.controllerEpoch = epoch;
        report.transmitted = true;
        if (f.data[0] == 0x80)
            return true;
        CHECK(f.data[0] == 0x40 || f.data[0] == 0x23 || f.data[0] == 0x2b || f.data[0] == 0x2f);
        ++w.attempts[node];
        const uint16_t index = uint16_t(f.data[1]) | uint16_t(f.data[2]) << 8;
        const uint8_t sub = f.data[3];
        M::Frame response;
        response.id = 0x580 + node;
        response.dlc = response.payloadLength = 8;
        response.data[1] = f.data[1];
        response.data[2] = f.data[2];
        response.data[3] = sub;
        if (index == 0x6041 && !sub && f.data[0] == 0x40) {
            // The other node's reviewed state read used for resource pressure: raw 0x0237.
            response.data[0] = 0x4b;
            response.data[4] = 0x37;
            response.data[5] = 0x02;
            w.push(response);
            return true;
        }
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
        const Failure failure =
            node == w.failureNode && w.failureAt == w.attempts[node] ? w.failure : Failure::NONE;
        if (failure == Failure::SILENT)
            return true;
        if (failure == Failure::BOOTUP) {
            w.heartbeat(0, node);
            return true;
        }
        if (failure == Failure::ABORT) {
            response.data[0] = 0x80;
            response.data[4] = 0x30;
            response.data[5] = 0;
            response.data[6] = 9;
            response.data[7] = 6;
        } else if (f.data[0] == 0x40) {
            response.data[0] = uint8_t(0x43u | ((4u - width) << 2));
            uint32_t value = w.values[node][slot];
            if (failure == Failure::READBACK)
                value ^= 1;
            for (unsigned i = 0; i < width; ++i)
                response.data[4 + i] = uint8_t(value >> (8 * i));
        } else {
            ++w.downloads[node];
            // Expedited download sizes by object width: 4 bytes 0x23, 2 bytes 0x2b, 1 byte 0x2f.
            CHECK(f.data[0] == (width == 4 ? 0x23 : width == 2 ? 0x2b : 0x2f));
            uint32_t value = 0;
            for (unsigned i = 0; i < width; ++i)
                value |= uint32_t(f.data[4 + i]) << (8 * i);
            // Literal documentary order: map changes require disabled PDO/count0.
            if (index == 0x1a00) {
                CHECK(w.values[node][0] & 0x80000000u);
                if (sub)
                    CHECK(w.values[node][4] == 0);
            }
            w.values[node][slot] = value;
            response.data[0] = 0x60;
            if (failure == Failure::LOST)
                return true;
        }
        w.push(response);
        return true;
    }
    static bool report(void* ptr, O::TxReport& out) {
        auto& w = *static_cast<Wire*>(ptr);
        if (!w.reportCount || w.holdReports)
            return false;
        out = w.reports[w.reportHead];
        w.reportHead = (w.reportHead + 1) % 8;
        --w.reportCount;
        return true;
    }
    O::Transport callbacks() {
        O::Transport t;
        t.context = this;
        t.receive = receive;
        t.transmit = transmit;
        t.takeTxReport = report;
        t.beforeTransmit = [](void* context, O::SequenceId sequence,
                              const C::SdoTransfer& transfer, uint64_t now) {
            Wire& w = *static_cast<Wire*>(context);
            ++w.guardCalls;
            CHECK(w.experiment);
            C::SdoTransfer other = transfer;
            other.data[0] ^= 1;
            CHECK(!w.experiment->preflight(sequence, other, now));
            O::SequenceId unrelated = sequence;
            ++unrelated.generation;
            CHECK(!w.experiment->preflight(unrelated, transfer, now));
            CHECK(!w.experiment->preflight(sequence, transfer, transfer.deadlineUs));
            return w.experiment->preflight(sequence, transfer, now);
        };
        return t;
    }
    // Node-2 frames other than uploads: the writes this harness actually put on the wire.
    unsigned writesTo(unsigned node, M::Frame* out, unsigned capacity) const {
        unsigned count = 0;
        for (unsigned i = 0; i < transmitted; ++i)
            if (sent[i].id == 0x600 + node && sent[i].data[0] != 0x40 && count < capacity)
                out[count++] = sent[i];
        return count;
    }
};
struct Consumer {
    unsigned samples = 0, invalidations = 0;
    static void sample(void* context, const C::PdoObservation&) {
        ++static_cast<Consumer*>(context)->samples;
    }
    static void invalidate(void* context, const C::PdoRoute&) {
        ++static_cast<Consumer*>(context)->invalidations;
    }
    C::PdoObserver observer() {
        C::PdoObserver out;
        out.context = this;
        out.sample = sample;
        out.invalidate = invalidate;
        return out;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    E::PdoExperiment experiment;
    uint64_t now = 0;
    // node2Confirmed: an externally established node-2 session premise, which the adapter
    // requires before it accepts replacement map evidence for a newer configuration epoch.
    explicit Rig(bool node2Confirmed = false)
        : owner(services, wire.callbacks(), 42), experiment(owner, services) {
        wire.experiment = &experiment;
        for (unsigned node = 1; node <= 2; ++node) {
            const auto t = target(node);
            CHECK(services.configure(uint8_t(node - 1), t.channel, node == 2 && node2Confirmed,
                                     true));
            C::NetworkRoute r;
            r.binding = t.channel.binding;
            r.generation = 1;
            r.heartbeatId = 0x700 + node;
            r.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), r, C::NetworkObserver(), 0));
            CHECK(services.setServiceAge(uint8_t(node - 1), 1000000, 0));
        }
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
        p.request.transition = L::Tpdo1Transition::MAP3_TO_MAP2;
        p.request.eventTimer = L::Tpdo1EventTimerRaw::RAW_0;
        p.firstAuthorizationId = 3000;
        p.maximumAgeUs = 1000000;
        return p;
    }
    // The supported change on the node's current route and epoch, as the E line builds it.
    E::PdoExperimentPlan timerPlan(unsigned node, L::Tpdo1EventTimerRaw value, uint32_t id = 5000,
                                   uint32_t grant = 5000) {
        E::PdoExperimentPlan p = plan();
        auto& a = p.review.plan;
        a.id = id;
        a.target.channel = *services.channel(uint8_t(node - 1));
        a.target.nodeGeneration = services.node(uint8_t(node - 1))->route.generation;
        a.deadlineUs = now + 5000000;
        p.configurationReview.target = a.target;
        p.configurationReview.scope = L::PdoConfigurationScope::FIXTURE_QUALIFIED;
        p.request.transition = L::Tpdo1Transition::EVENT_TIMER;
        p.request.eventTimer = value;
        p.slot = uint8_t(node - 1);
        p.firstAuthorizationId = grant;
        return p;
    }
    // serveAgain: the owner serves once more at the same time after the experiment, as a
    // cooperative loop may before its clock advances, so new work can start at that instant.
    bool serveAgain = false;
    void turn(uint64_t at) {
        now = wire.now = at;
        O::ServiceBudget b;
        b.rx = 8;
        b.tx = 2;
        b.events = 8;
        b.starts = 2;
        CHECK(owner.service(now, b));
        const unsigned tx = wire.transmitted, rx = wire.receives;
        CHECK(experiment.service(now));
        CHECK(wire.transmitted == tx && wire.receives == rx);
        if (serveAgain)
            CHECK(owner.service(now, b));
    }
    void start() {
        CHECK(experiment.begin(plan(), 0) == E::EvidenceAdmission::ACCEPTED);
        wire.now = 1;
        wire.heartbeat();
        turn(1);
    }
    // Begin a timer plan now, then the node's fresh Pre-operational heartbeat after it.
    void startTimer(unsigned node, L::Tpdo1EventTimerRaw value, uint32_t id = 5000,
                    uint32_t grant = 5000) {
        CHECK(experiment.begin(timerPlan(node, value, id, grant), now) ==
              E::EvidenceAdmission::ACCEPTED);
        wire.now = now + 1;
        wire.heartbeat(0x7f, node);
        turn(now + 1);
    }
    void finish() {
        for (unsigned i = 0; i < 100 && !experiment.record()->terminal; ++i)
            turn(now + 1);
        CHECK(experiment.record()->terminal);
    }
    // Install reviewed actual-map evidence for one node's TPDO1 route, count 3, 7 bytes.
    C::PdoMapEvidence reviewedMap(unsigned node) {
        C::PdoMapEvidence out;
        out.route.binding = services.channel(uint8_t(node - 1))->binding;
        out.route.generation = 1;
        out.route.cobId = 0x180 + node;
        C::PdoMap map;
        map.direction = C::PdoDirection::TPDO;
        map.number = 1;
        map.count = 3;
        map.words[0] = 0x60410010;
        map.words[1] = 0x60610008;
        map.words[2] = 0x60640020;
        CHECK(C::validatePdoMap(map, L::pdoProfile(1), out.route.layout));
        out.nodeGeneration = 1;
        out.startedUs = out.observedUs = now;
        out.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
        out.evidenceId = 100 + node;
        out.communicationKnown = true;
        out.cobIdParameter = 0x180 + node;
        out.transmissionType = 255;
        return out;
    }
};
void passiveHappyAndReservation() {
    Rig r;
    CHECK(!r.experiment.record() && !r.wire.transmitted && !r.wire.receives);
    CHECK(r.experiment.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(!r.wire.transmitted && r.owner.resources().reservations == 1);
    CHECK(r.experiment.begin(r.plan(), 0) == E::EvidenceAdmission::BUSY);
    O::SequenceId other;
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
    CHECK(out->configuration.request.transition == L::Tpdo1Transition::MAP3_TO_MAP2);
    CHECK(out->configuration.acknowledgedMask == 0x3f && !out->configuration.uncertain);
    CHECK(out->configuration.confirmedStep == 15 && out->configuration.firstEffectStep == 1);
    CHECK(out->current.configurationGeneration == 2 &&
          r.services.channel(0)->binding.configurationGeneration == 2);
    CHECK(r.wire.downloads[1] == 6 && r.wire.values[1][0] == 0x181 && r.wire.values[1][4] == 2);
    CHECK(r.wire.guardCalls == 6);
    CHECK(!r.experiment.preflight(out->terminals[9].sequence,
                                 out->terminals[9].service.transfer, r.now));
    CHECK(r.wire.values[1][6] == 0x60640020 && r.wire.values[1][7] == 0x60640020 &&
          r.wire.values[1][8] == 0);
    CHECK(!out->evidenceLost && !out->tailOverflow);
    for (unsigned i = 0; i < 24; ++i) {
        const auto& t = out->terminals[i].service.transfer;
        CHECK(t.uploadSize == C::SdoUploadSize::EXACT && t.operation == (i < 9 ? 3000u : 3001u));
        CHECK(t.channel.binding.configurationGeneration == (i < 15 ? 1u : 2u));
        CHECK(t.index == 0x1800 || t.index == 0x1a00); // No motion/NMT/SYNC/save.
    }
    // Node 2 saw nothing: its route, epoch and dictionary are untouched.
    CHECK(!r.wire.attempts[2] && r.services.channel(1)->binding.configurationGeneration == 1);
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
        CHECK(r.wire.attempts[1] == at && !out->configuration.mapConfirmed);
        CHECK(out->terminals[at - 1].service.abortCode == 0x06090030u);
        if (at > 9)
            CHECK(out->configuration.uncertain && out->configuration.failedStep == at - 9);
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
        CHECK(r.wire.attempts[1] == at && r.experiment.release());
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
    CHECK(held.wire.attempts[1] == 1 && held.experiment.record()->terminalCount == 1);
    held.turn(4);
    CHECK(held.wire.attempts[1] == 1); // Matched reply cannot retire outstanding actual TX.
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
    CHECK(old.wire.downloads[1] == 0 && old.experiment.release());
}
// Diagnostic loss cancels only work with no frame out. A sent frame settles first.
void evidenceLossWithdrawsOnlyUnstartedWork() {
    const auto overflow = [](Rig& r, uint64_t from) {
        O::ServiceBudget receiveOnly; // An owner turn that starts nothing still fills the trace.
        receiveOnly.rx = 8;
        for (unsigned round = 0; round < 4; ++round) {
            for (unsigned i = 0; i < 8; ++i) {
                M::Frame noise;
                noise.id = 0x300;
                noise.dlc = noise.payloadLength = 8;
                r.wire.push(noise);
            }
            CHECK(r.owner.service(from + round, receiveOnly));
        }
        CHECK(r.owner.stats().traceOverwritten > 0);
        CHECK(r.experiment.service(from + 3));
    };
    Rig unstarted;
    unstarted.start();
    CHECK(unstarted.wire.transmitted == 0);
    overflow(unstarted, 2);
    CHECK(unstarted.experiment.record()->terminal);
    CHECK(unstarted.experiment.record()->failure == E::PdoExperimentFailure::EVIDENCE_LOSS);
    CHECK(unstarted.wire.transmitted == 0 && !unstarted.services.unresolved(0));
    Rig sent;
    sent.wire.failure = Wire::Failure::SILENT;
    sent.wire.failureAt = 1;
    sent.start();
    sent.turn(2);
    CHECK(sent.wire.transmitted == 1 && !sent.experiment.record()->terminal);
    overflow(sent, 3);
    CHECK(!sent.experiment.record()->terminal && sent.experiment.record()->evidenceLost);
    CHECK(sent.wire.transmitted == 1);
    sent.turn(sent.plan().review.plan.deadlineUs);
    CHECK(sent.experiment.record()->terminal);
    CHECK(sent.experiment.record()->failure == E::PdoExperimentFailure::EVIDENCE_LOSS);
    CHECK(sent.experiment.record()->terminals[0].service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(sent.wire.transmitted == 1);
}
void planRejections() {
    for (unsigned i = 0; i < 16; ++i) {
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
        if (i == 7)
            p.request = L::Tpdo1ConfigurationRequest(); // No transition named.
        if (i == 8)
            p.request.eventTimer = L::Tpdo1EventTimerRaw::RAW_100; // The map keeps event 0.
        if (i >= 9)
            p = r.timerPlan(2, L::Tpdo1EventTimerRaw::RAW_100);
        if (i == 9)
            p.configurationReview.scope = L::PdoConfigurationScope::ISOLATED_EXPERIMENT;
        if (i == 10)
            p.request.eventTimer = L::Tpdo1EventTimerRaw(200); // Measured, not supported.
        if (i == 11)
            p.request.eventTimer = L::Tpdo1EventTimerRaw(10);
        if (i == 12)
            p.firstAuthorizationId = UINT32_MAX - 17; // Nineteen grants do not fit.
        if (i == 13)
            p.slot = 0; // Node 1's route is not this target.
        if (i == 14) {
            p.review.plan.target.channel.binding.configurationGeneration++; // Stale epoch.
            p.configurationReview.target = p.review.plan.target;
        }
        if (i == 15)
            p.review.plan.target.nodeGeneration++; // A different node lifecycle.
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
    CHECK(r.experiment.record()->attemptedCount == 10 && r.wire.downloads[1] == 0);
    r.wire.now = r.now + 1;
    r.wire.heartbeat(5);
    r.turn(r.now + 1);
    r.finish();
    CHECK(r.wire.downloads[1] == 0 && r.wire.attempts[1] == 9);
    CHECK(r.experiment.record()->failure == E::PdoExperimentFailure::INVALIDATED);
    CHECK(r.experiment.record()->configuration.uncertain);
    CHECK(!r.experiment.record()->configuration.mapConfirmed && r.experiment.release());
}
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
    CHECK(r.wire.downloads[1] == 0);
    r.turn(52);
    r.finish();
    CHECK(r.wire.downloads[1] == 0 && r.wire.attempts[1] == 9);
    CHECK(r.experiment.record()->failure == E::PdoExperimentFailure::INVALIDATED);
    CHECK(r.experiment.record()->configuration.uncertain && r.experiment.release());
    Rig broad;
    CHECK(broad.services.setServiceAge(0, 1000001, 0));
    CHECK(broad.experiment.begin(broad.plan(), 0) == E::EvidenceAdmission::INVALID);
    CHECK(!broad.experiment.record() && broad.owner.resources().reservations == 0);
}

// ---- The supported event-timer change, on node 2 while node 1 streams Operational.
const uint8_t kWrite100[8] = {0x2b, 0x00, 0x18, 0x05, 0x64, 0x00, 0x00, 0x00};
const uint8_t kWrite0[8] = {0x2b, 0x00, 0x18, 0x05, 0x00, 0x00, 0x00, 0x00};
// Every node-2 frame of one run: nineteen, the tenth the one write, the rest exact uploads of the
// TPDO1 snapshot. Nothing went to node 1.
void checkWire(const Wire& w, unsigned from, const uint8_t (&write)[8]) {
    unsigned node2 = 0;
    for (unsigned i = from; i < w.transmitted; ++i) {
        CHECK(w.sent[i].id == 0x602);
        const uint16_t index = uint16_t(w.sent[i].data[1] | w.sent[i].data[2] << 8);
        CHECK(index == 0x1800 || index == 0x1a00);
        if (++node2 == 10)
            CHECK(std::memcmp(w.sent[i].data, write, 8) == 0);
        else
            CHECK(w.sent[i].data[0] == 0x40);
    }
    CHECK(node2 == 19);
}
void timerOnOneNodeWhileAnotherStreams() {
    Rig r(true);
    Consumer one, two;
    // Node 1: Operational, its TPDO1 route confirmed and qualified, so its frames are samples.
    CHECK(r.services.configurePdo(0, 0, r.reviewedMap(1), L::pdoProfile(1), one.observer(), 0));
    CHECK(r.services.configurePdo(1, 1, r.reviewedMap(2), L::pdoProfile(1), two.observer(), 0));
    C::PdoQualification proof;
    proof.route = r.services.pdo(0)->map.route;
    proof.nodeGeneration = 1;
    proof.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
    proof.establishedUs = 0;
    proof.evidenceId = 200;
    CHECK(r.services.qualifyPdo(0, proof, 0));
    r.wire.now = 1;
    r.wire.heartbeat(0x05, 1);
    r.turn(1);
    r.wire.now = 2;
    r.wire.tpdo(1);
    r.turn(2);
    CHECK(one.samples == 1);
    // E2,100: begin, then node 2's fresh Pre-operational heartbeat.
    r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
    unsigned streamed = 0;
    for (unsigned i = 0; i < 120 && !r.experiment.record()->terminal; ++i) {
        r.wire.now = r.now + 1;
        if (i % 4 == 0) {
            r.wire.tpdo(1); // The other node keeps streaming.
            ++streamed;
        }
        if (i % 9 == 0)
            r.wire.tpdo(2); // Node 2's own old map bytes, while it is Pre-operational.
        if (i % 11 == 0)
            r.wire.unrelated();
        r.turn(r.now + 1);
    }
    const auto* out = r.experiment.record();
    CHECK(out->terminal && out->failure == E::PdoExperimentFailure::NONE);
    CHECK(out->terminalCount == 19 && out->attemptedCount == 19 && !out->evidenceLost);
    const auto& c = out->configuration;
    CHECK(c.state == L::PdoConfigurationState::SUCCEEDED && c.mapConfirmed && !c.uncertain);
    CHECK(c.request.transition == L::Tpdo1Transition::EVENT_TIMER);
    CHECK(c.review.scope == L::PdoConfigurationScope::FIXTURE_QUALIFIED);
    CHECK(c.acknowledgedMask == 1 && c.confirmedStep == 10 && c.firstEffectStep == 1 &&
          !c.failedStep);
    CHECK(c.readback[3] == 100 && c.original[0] == 0x182);
    for (unsigned i = 0; i < 19; ++i) {
        const auto& t = out->terminals[i].service.transfer;
        CHECK(t.operation == (i < 9 ? 5000u : 5001u) && t.requirePreOperational);
        CHECK(t.channel.binding.configurationGeneration == (i < 10 ? 1u : 2u));
        CHECK(out->terminals[i].service.outcome == C::SdoOutcome::SUCCESS);
    }
    CHECK(r.wire.downloads[2] == 1 && r.wire.values[2][3] == 100);
    checkWire(r.wire, 0, kWrite100);
    // Node 2's configuration and route withdrawn before the write; node 1 untouched and still
    // delivering samples, never invalidated.
    CHECK(out->current.configurationGeneration == 2 &&
          r.services.channel(1)->binding.configurationGeneration == 2);
    CHECK(r.services.channel(0)->binding.configurationGeneration == 1 && !r.wire.downloads[1]);
    CHECK(!r.services.pdo(1)->confirmed && !r.services.pdo(1)->qualified && two.invalidations);
    CHECK(r.services.pdo(0)->confirmed && r.services.pdo(0)->qualified && !one.invalidations);
    // Every frame node 1 sent during the run was accepted as its sample; none of node 2's was.
    CHECK(streamed >= 5 && one.samples == 1 + streamed && !two.samples);
    // Publication only through the checked getter, then an explicit owner reinstallation.
    C::PdoMapEvidence map;
    L::Tpdo1EventTimerRaw literal = L::Tpdo1EventTimerRaw::RAW_0;
    CHECK(L::getTpdo1Configuration(c, out->reservation, out->current, r.now, 2, 77, map, literal));
    CHECK(literal == L::Tpdo1EventTimerRaw::RAW_100 && map.route.cobId == 0x182);
    CHECK(map.route.layout.payloadLength == 7 && map.route.binding.configurationGeneration == 2);
    // The harness withdrew the route again after the epoch advance and read back strictly after
    // that boundary, so the owner accepts the readback as replacement map evidence.
    CHECK(c.readbackStartedUs > r.services.pdo(1)->boundaryUs);
    C::PdoMapEvidence early = map;
    early.startedUs = r.services.pdo(1)->boundaryUs; // Acquired at the boundary: refused.
    CHECK(!r.services.configurePdo(1, 1, early, L::pdoProfile(1), two.observer(), r.now));
    CHECK(r.services.configurePdo(1, 1, map, L::pdoProfile(1), two.observer(), r.now));
    CHECK(r.services.pdo(1)->confirmed && !r.services.pdo(1)->qualified);
    // Same-ID, same-DLC bytes after the new map epoch stay unqualified: a host generation is no
    // wire marker. A distinct-route claim cannot qualify this reused identifier either.
    r.wire.now = r.now + 1;
    r.wire.heartbeat(0x05, 2);
    r.turn(r.now + 1);
    r.wire.now = r.now + 1;
    r.wire.tpdo(2);
    r.turn(r.now + 1);
    CHECK(!two.samples && !r.services.pdo(1)->samplePresent);
    C::PdoQualification distinct;
    distinct.route = r.services.pdo(1)->map.route;
    distinct.nodeGeneration = 1;
    distinct.basis = C::PdoSampleBasis::DISTINCT_VERIFIED_ROUTE;
    distinct.establishedUs = r.now;
    distinct.evidenceId = 201;
    CHECK(!r.services.qualifyPdo(1, distinct, r.now));
    CHECK(r.experiment.release());
    // A plan on the stale epoch refuses; the reverse change on the current one runs.
    E::PdoExperimentPlan stale = r.timerPlan(2, L::Tpdo1EventTimerRaw::RAW_0, 6000, 6000);
    stale.review.plan.target.channel.binding.configurationGeneration = 1;
    stale.configurationReview.target = stale.review.plan.target;
    CHECK(r.experiment.begin(stale, r.now) == E::EvidenceAdmission::INVALID);
    r.wire.now = r.now + 1;
    r.wire.heartbeat(0x7f, 2); // Back to Pre-operational.
    r.turn(r.now + 1);
    const unsigned from = r.wire.transmitted;
    r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_0, 6000, 6000);
    r.finish();
    out = r.experiment.record();
    CHECK(out->failure == E::PdoExperimentFailure::NONE && out->terminalCount == 19);
    CHECK(out->configuration.original[3] == 100 && out->configuration.readback[3] == 0);
    CHECK(out->current.configurationGeneration == 3 && r.wire.values[2][3] == 0);
    checkWire(r.wire, from, kWrite0);
    CHECK(r.services.pdo(0)->qualified && !r.services.pdo(1)->confirmed);
    CHECK(r.experiment.release());
}
void timerFailureAtEveryStep() {
    for (unsigned at = 1; at <= 19; ++at) {
        Rig r;
        r.wire.failure = Wire::Failure::ABORT;
        r.wire.failureNode = 2;
        r.wire.failureAt = at;
        r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
        r.finish();
        const auto* out = r.experiment.record();
        CHECK(out->failure == E::PdoExperimentFailure::SERVICE && out->terminalCount == at);
        CHECK(r.wire.attempts[2] == at && !out->configuration.mapConfirmed);
        CHECK(out->terminals[at - 1].service.abortCode == 0x06090030u);
        CHECK(out->configuration.uncertain == (at >= 10));
        CHECK(out->configurationPrepared == (at >= 10));
        if (at >= 10)
            CHECK(out->configuration.failedStep == at - 9 &&
                  out->configuration.confirmedStep == at - 10 &&
                  out->configuration.firstEffectStep == 1);
        // A server-refused write and a failed readback leave node 2's epoch withdrawn.
        CHECK(out->current.configurationGeneration == (at >= 10 ? 2u : 1u));
        CHECK(!r.wire.attempts[1] && r.experiment.release());
    }
    for (unsigned at = 11; at <= 19; ++at) {
        Rig r;
        r.wire.failure = Wire::Failure::READBACK;
        r.wire.failureNode = 2;
        r.wire.failureAt = at;
        r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
        r.finish();
        const auto* out = r.experiment.record();
        CHECK(out->failure == E::PdoExperimentFailure::READBACK);
        CHECK(out->configuration.uncertain && !out->configuration.mapConfirmed);
        CHECK(out->configuration.failedStep == at - 9);
        CHECK(r.wire.attempts[2] == at && r.experiment.release());
    }
    // A boot-up during readback ends it as invalidated; the possible write stays uncertain.
    Rig boot;
    boot.wire.failure = Wire::Failure::BOOTUP;
    boot.wire.failureNode = 2;
    boot.wire.failureAt = 12;
    boot.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
    boot.finish();
    CHECK(boot.experiment.record()->failure == E::PdoExperimentFailure::INVALIDATED);
    CHECK(boot.experiment.record()->configuration.uncertain);
    CHECK(boot.experiment.release());
}
void timerUnresolvedRouteIsNotReused() {
    // The write goes out and takes effect, but nothing answers: the response timeout ends the
    // record with an unknown effect and an unresolved route. The next E line sends nothing.
    Rig r;
    r.wire.failure = Wire::Failure::LOST;
    r.wire.failureNode = 2;
    r.wire.failureAt = 10;
    r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
    for (unsigned i = 0; i < 100 && r.wire.attempts[2] < 10; ++i)
        r.turn(r.now + 1);
    CHECK(r.wire.attempts[2] == 10 && !r.experiment.record()->terminal);
    for (unsigned i = 0; i < 4 && !r.experiment.record()->terminal; ++i) {
        r.wire.now = r.now + 260000;
        r.wire.heartbeat(0x7f, 2);
        r.turn(r.now + 260000);
    }
    const auto* out = r.experiment.record();
    CHECK(out->terminal && out->failure == E::PdoExperimentFailure::SERVICE);
    CHECK(out->terminals[9].service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(out->terminals[9].service.effectUncertain && out->configuration.uncertain);
    CHECK(out->configuration.failedStep == 1 && !out->configuration.acknowledgedMask);
    CHECK(r.services.unresolved(1) && r.wire.values[2][3] == 100); // The device did change.
    for (unsigned i = 0; i < 4 && !r.experiment.release(); ++i)
        r.turn(r.now + 1);
    CHECK(!r.experiment.record());
    const unsigned sent = r.wire.transmitted;
    r.wire.failure = Wire::Failure::NONE;
    // The owner refuses to reserve an unresolved route: the restoring line is refused unsent.
    CHECK(r.experiment.begin(r.timerPlan(2, L::Tpdo1EventTimerRaw::RAW_0, 6000, 6000), r.now) ==
          E::EvidenceAdmission::BUSY);
    CHECK(!r.experiment.record() && !r.owner.resources().reservations);
    for (unsigned i = 0; i < 4; ++i)
        r.turn(r.now + 1);
    CHECK(r.wire.transmitted == sent && r.services.unresolved(1)); // No replay, no new traffic.
    CHECK(!r.services.unresolved(0)); // Node 1's independent route stays resolved.
}
void timerStateHeldTxAndPressure() {
    // Operational arrives after the baseline, between the write's owner admission and its
    // handoff: nothing is written, and the conservative withdrawal remains.
    Rig op;
    op.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
    for (unsigned i = 0; i < 100 && op.experiment.record()->terminalCount < 9; ++i)
        op.turn(op.now + 1);
    CHECK(op.experiment.record()->attemptedCount == 10 && !op.wire.downloads[2]);
    op.wire.now = op.now + 1;
    op.wire.heartbeat(5, 2);
    op.turn(op.now + 1);
    op.finish();
    CHECK(!op.wire.downloads[2] && op.wire.attempts[2] == 9);
    CHECK(op.experiment.record()->failure == E::PdoExperimentFailure::INVALIDATED);
    CHECK(op.experiment.record()->configuration.uncertain &&
          !op.experiment.record()->configuration.mapConfirmed);
    CHECK(op.experiment.release());
    // A held TX report keeps the next read from starting until the write's frame retires.
    Rig held;
    held.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
    for (unsigned i = 0; i < 100 && held.wire.attempts[2] < 9; ++i)
        held.turn(held.now + 1);
    for (unsigned i = 0; i < 100 && held.experiment.record()->terminalCount < 9; ++i)
        held.turn(held.now + 1);
    held.wire.holdReports = true;
    for (unsigned i = 0; i < 6; ++i)
        held.turn(held.now + 1);
    CHECK(held.wire.attempts[2] == 10 && held.wire.downloads[2] == 1);
    CHECK(!held.experiment.record()->terminal);
    held.wire.holdReports = false;
    held.finish();
    CHECK(held.experiment.record()->failure == E::PdoExperimentFailure::NONE);
    CHECK(held.wire.attempts[2] == 19 && held.experiment.release());
    // Resource pressure: node 1's own work fills every ordinary owner slot first. The first
    // baseline read refuses at admission; nothing reaches node 2 and no grant stays armed.
    Rig busy;
    O::SequenceId other;
    CHECK(busy.owner.beginSequence(target(1).channel.binding, 1, 0, other));
    CHECK(busy.owner.beginOperation(other, 900, 4000000, 0));
    O::RequestId ids[O::CanBusOwner::REQUESTS];
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        O::BusRequest request;
        request.sequence = other;
        auto& t = request.transfer;
        t.channel = target(1).channel;
        t.operation = t.request = 900;
        t.step = i + 1;
        t.index = 0x6041;
        t.size = 2;
        t.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        t.deadlineUs = 4000000;
        t.queueTimeoutUs = t.responseTimeoutUs = 250000;
        CHECK(busy.owner.admit(request, 0, ids[i]) == O::BusAdmission::ACCEPTED);
    }
    busy.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
    busy.finish();
    const auto* out = busy.experiment.record();
    CHECK(out->failure == E::PdoExperimentFailure::ADMISSION);
    CHECK(out->admission == O::BusAdmission::RESULTS_FULL && !out->terminalCount);
    CHECK(!busy.wire.attempts[2] && !out->configurationPrepared && !out->configuration.uncertain);
    CHECK(busy.services.channel(1)->binding.configurationGeneration == 1);
    C::SdoTransfer first;
    first.channel = target(2).channel;
    first.operation = first.request = 5000;
    first.step = 1;
    first.index = 0x1800;
    first.subindex = 1;
    first.size = 4;
    first.requirePreOperational = true;
    first.deadlineUs = out->plan.review.plan.deadlineUs;
    first.queueTimeoutUs = first.responseTimeoutUs = 250000;
    CHECK(!busy.services.checkAdmission(1, first, busy.now)); // The unused grant was withdrawn.
    CHECK(busy.experiment.release());
}
// The route withdrawal before the write: with node 2's TPDO1 confirmed and qualified, a write
// that the server refuses or never answers leaves no confirmed or qualifiable map, so no later
// Operational frame can become a sample. Node 1's route stays as it was.
void timerUncertainWriteWithdrawsTheRoute() {
    const Wire::Failure failures[] = {Wire::Failure::ABORT, Wire::Failure::LOST};
    for (Wire::Failure failure : failures) {
        Rig r(true);
        Consumer one, two;
        CHECK(r.services.configurePdo(0, 0, r.reviewedMap(1), L::pdoProfile(1), one.observer(), 0));
        CHECK(r.services.configurePdo(1, 1, r.reviewedMap(2), L::pdoProfile(1), two.observer(), 0));
        for (uint8_t slot = 0; slot < 2; ++slot) {
            C::PdoQualification proof;
            proof.route = r.services.pdo(slot)->map.route;
            proof.nodeGeneration = 1;
            proof.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
            proof.evidenceId = 200 + slot;
            CHECK(r.services.qualifyPdo(slot, proof, 0));
        }
        CHECK(r.services.pdo(1)->confirmed && r.services.pdo(1)->qualified);
        r.wire.failure = failure;
        r.wire.failureNode = 2;
        r.wire.failureAt = 10;
        r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
        for (unsigned i = 0; i < 100 && r.wire.attempts[2] < 10; ++i)
            r.turn(r.now + 1);
        // A lost reply waits out the response budget; heartbeats keep Pre-operational current.
        for (unsigned i = 0; i < 8 && !r.experiment.record()->terminal; ++i) {
            r.wire.now = r.now + 260000;
            r.wire.heartbeat(0x7f, 2);
            r.turn(r.now + 260000);
        }
        r.finish();
        const auto* out = r.experiment.record();
        CHECK(out->failure == E::PdoExperimentFailure::SERVICE && out->terminalCount == 10);
        CHECK(out->configuration.uncertain && out->configuration.failedStep == 1);
        CHECK(!out->configurationMigrated);
        CHECK(!r.services.pdo(1)->confirmed && !r.services.pdo(1)->qualified && two.invalidations);
        C::PdoQualification again;
        again.route = r.services.pdo(1)->map.route;
        again.nodeGeneration = 1;
        again.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
        again.establishedUs = r.now;
        again.evidenceId = 210;
        CHECK(!r.services.qualifyPdo(1, again, r.now)); // No map to qualify after the write.
        r.wire.now = r.now + 1;
        r.wire.heartbeat(0x05, 2);
        r.turn(r.now + 1);
        r.wire.now = r.now + 1;
        r.wire.tpdo(2);
        r.turn(r.now + 1);
        CHECK(!two.samples && !r.services.pdo(1)->samplePresent);
        CHECK(r.services.pdo(0)->confirmed && r.services.pdo(0)->qualified && !one.invalidations);
        for (unsigned i = 0; i < 4 && !r.experiment.release(); ++i)
            r.turn(r.now + 1);
        CHECK(!r.experiment.record());
    }
}
// Two changes in a row with no install between: the route keeps the epoch it was installed at,
// and the second readback can still be installed. Node 1's route is not this binding.
void timerTwiceWithoutInstall() {
    Rig r(true);
    Consumer one, two;
    CHECK(r.services.configurePdo(0, 0, r.reviewedMap(1), L::pdoProfile(1), one.observer(), 0));
    CHECK(r.services.configurePdo(1, 1, r.reviewedMap(2), L::pdoProfile(1), two.observer(), 0));
    r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
    r.finish();
    CHECK(r.experiment.record()->failure == E::PdoExperimentFailure::NONE);
    CHECK(r.experiment.release());
    r.wire.now = r.now + 1;
    r.wire.heartbeat(0x7f, 2);
    r.turn(r.now + 1);
    r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_0, 6000, 6000);
    r.finish();
    const auto* out = r.experiment.record();
    CHECK(out->failure == E::PdoExperimentFailure::NONE && out->current.configurationGeneration == 3);
    CHECK(r.services.pdo(1)->map.route.binding.configurationGeneration == 1);
    CHECK(out->configuration.readbackStartedUs > r.services.pdo(1)->boundaryUs);
    C::PdoMapEvidence map;
    L::Tpdo1EventTimerRaw literal = L::Tpdo1EventTimerRaw::RAW_100;
    CHECK(L::getTpdo1Configuration(out->configuration, out->reservation, out->current, r.now, 2, 78,
                                   map, literal));
    CHECK(literal == L::Tpdo1EventTimerRaw::RAW_0);
    CHECK(r.services.configurePdo(1, 1, map, L::pdoProfile(1), two.observer(), r.now));
    CHECK(r.services.pdo(1)->confirmed && !r.services.pdo(1)->qualified);
    CHECK(r.services.pdo(0)->confirmed && !one.invalidations && !r.wire.attempts[1]);
    CHECK(r.experiment.release());
}
// The readback waits for a later time than the renewed withdrawal: here the owner serves again at
// the boundary's own time, which would otherwise start the first read at that instant.
void timerReadbackFollowsTheBoundary() {
    Rig r(true);
    r.serveAgain = true;
    Consumer two;
    CHECK(r.services.configurePdo(1, 1, r.reviewedMap(2), L::pdoProfile(1), two.observer(), 0));
    r.startTimer(2, L::Tpdo1EventTimerRaw::RAW_100);
    r.finish();
    const auto* out = r.experiment.record();
    CHECK(out->failure == E::PdoExperimentFailure::NONE);
    CHECK(out->configuration.readbackStartedUs > r.services.pdo(1)->boundaryUs);
    C::PdoMapEvidence map;
    CHECK(L::getTpdo1Configuration(out->configuration, out->reservation, out->current, r.now, 2, 79,
                                   map));
    CHECK(r.services.configurePdo(1, 1, map, L::pdoProfile(1), two.observer(), r.now));
    CHECK(r.experiment.release());
}
// The application path sets the age policy only for an admitted request: a refusal restores it.
void serviceAgeOnlyForAdmission() {
    Rig r;
    CHECK(r.services.setServiceAge(0, UINT64_MAX, 0) && r.services.setServiceAge(1, UINT64_MAX, 0));
    E::PdoExperimentPlan refused = r.timerPlan(2, L::Tpdo1EventTimerRaw::RAW_100);
    refused.maximumAgeUs = 3000000;
    refused.request.eventTimer = L::Tpdo1EventTimerRaw(200);
    CHECK(r.experiment.beginWithServiceAge(refused, 0) == E::EvidenceAdmission::INVALID);
    CHECK(r.services.node(1)->maximumEvidenceAgeUs == UINT64_MAX && !r.experiment.record());
    refused = r.timerPlan(2, L::Tpdo1EventTimerRaw::RAW_100);
    refused.maximumAgeUs = 0;
    CHECK(r.experiment.beginWithServiceAge(refused, 0) == E::EvidenceAdmission::INVALID);
    CHECK(r.services.node(1)->maximumEvidenceAgeUs == UINT64_MAX);
    E::PdoExperimentPlan admitted = r.timerPlan(2, L::Tpdo1EventTimerRaw::RAW_100);
    admitted.maximumAgeUs = 3000000;
    CHECK(r.experiment.beginWithServiceAge(admitted, 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(r.services.node(1)->maximumEvidenceAgeUs == 3000000 && !r.wire.transmitted);
    // While a record is held, the next request is BUSY and touches no node's policy.
    E::PdoExperimentPlan busy = r.plan();
    busy.maximumAgeUs = 3000000;
    CHECK(r.experiment.beginWithServiceAge(busy, 0) == E::EvidenceAdmission::BUSY);
    CHECK(r.services.node(0)->maximumEvidenceAgeUs == UINT64_MAX);
    r.turn(admitted.review.plan.deadlineUs);
    CHECK(r.experiment.record()->failure == E::PdoExperimentFailure::DEADLINE && r.experiment.release());
}
} // namespace
int main() {
    passiveHappyAndReservation();
    failureAtEveryStep();
    lifecycleDeadlineAndRetirement();
    evidenceLossWithdrawsOnlyUnstartedWork();
    planRejections();
    operationalBetweenAdmissionAndOwnerHandoff();
    ageExpiresBetweenAdmissionAndOwnerHandoff();
    timerOnOneNodeWhileAnotherStreams();
    timerFailureAtEveryStep();
    timerUnresolvedRouteIsNotReused();
    timerStateHeldTxAndPressure();
    timerUncertainWriteWithdrawsTheRoute();
    timerTwiceWithoutInstall();
    timerReadbackFollowsTheBoundary();
    serviceAgeOnlyForAdmission();
    std::printf("pdo_experiment_test: %u checks passed\n", checks);
}
