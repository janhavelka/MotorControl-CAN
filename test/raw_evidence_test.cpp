// SPDX-License-Identifier: MIT
// Synthetic wire traffic through the real owner and selected SDO engine.
#include "RawEvidence.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                       \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <size_t N> void label(char (&out)[N], const char* value) {
    CHECK(std::strlen(value) < N);
    std::strcpy(out, value);
}
L::ReadTarget target() {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = 101;
    t.channel.binding.endpoint.profile = 1;
    t.channel.binding.endpoint.address = 1;
    t.channel.binding.bindingGeneration = t.channel.binding.configurationGeneration = 1;
    t.channel.clientToServerId = 0x601;
    t.channel.serverToClientId = 0x581;
    t.nodeGeneration = 1;
    return t;
}
struct Wire {
    enum class Mode { SUCCESS, SILENT, WRONG_OBJECT, BOOTUP, SERVER_ABORT, OVERSIZED, MALFORMED };
    Mode mode = Mode::SUCCESS;
    uint64_t now = 0;
    E::ReceivedFrame rx[32];
    E::TxReport reports[4];
    M::Frame sent[64];
    unsigned rxHead = 0, rxCount = 0, reportHead = 0, reportCount = 0;
    unsigned transmitted = 0, uploads = 0, receives = 0;
    unsigned failureAt = 0; ///< Zero applies selected fault to every field.
    bool holdCleanupReport = false;
    bool holdReport = false;
    bool proveRetirement = false;
    unsigned retireCalls = 0;
    void push(const M::Frame& frame) {
        CHECK(rxCount < 32);
        auto& out = rx[(rxHead + rxCount++) % 32];
        out.frame = frame;
        out.receivedTimeKnown = true;
        out.receivedUs = now;
        out.controllerEpoch = 1;
    }
    static bool receive(void* ptr, E::ReceivedFrame& out) {
        Wire& w = *static_cast<Wire*>(ptr);
        ++w.receives;
        if (!w.rxCount)
            return false;
        out = w.rx[w.rxHead];
        w.rxHead = (w.rxHead + 1) % 32;
        --w.rxCount;
        return true;
    }
    static bool transmit(void* ptr, const M::Frame& request, C::SdoTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(ptr);
        CHECK(request.id == 0x601 && epoch == 1 && request.payloadLength == 8);
        CHECK(request.data[0] == 0x40 || request.data[0] == 0x80);
        CHECK(w.transmitted < 64 && w.reportCount < 4);
        w.sent[w.transmitted++] = request;
        auto& report = w.reports[(w.reportHead + w.reportCount++) % 4];
        report.id = id;
        report.transmitted = true;
        report.controllerEpoch = epoch;
        if (request.data[0] == 0x80)
            return true;
        ++w.uploads;
        const Mode mode = w.failureAt && w.uploads != w.failureAt ? Mode::SUCCESS : w.mode;
        if (mode == Mode::SILENT)
            return true;
        M::Frame reply;
        reply.id = mode == Mode::BOOTUP ? 0x701 : 0x581;
        reply.dlc = reply.payloadLength = mode == Mode::BOOTUP ? 1 : 8;
        if (mode != Mode::BOOTUP) {
            // The second identity object deliberately responds with one byte.
            const unsigned width = w.uploads == 2 ? 1 : w.uploads % 4 + 1;
            reply.data[0] = uint8_t(0x43u | ((4u - width) << 2));
            reply.data[1] = request.data[1];
            reply.data[2] = request.data[2];
            reply.data[3] = request.data[3];
            for (unsigned i = 0; i < 4; ++i)
                reply.data[4 + i] = uint8_t(0x30 + w.uploads + i);
            if (mode == Mode::WRONG_OBJECT)
                ++reply.data[1];
            if (mode == Mode::SERVER_ABORT)
                reply.data[0] = 0x80;
            if (mode == Mode::OVERSIZED) {
                reply.data[0] = 0x41;
                reply.data[4] = 5;
                reply.data[5] = reply.data[6] = reply.data[7] = 0;
            }
            if (mode == Mode::MALFORMED)
                reply.data[0] = 0x42; // Unindicated size forbidden raw.
        }
        w.push(reply);
        return true;
    }
    static bool report(void* ptr, E::TxReport& out) {
        Wire& w = *static_cast<Wire*>(ptr);
        if (!w.reportCount || w.holdReport ||
            (w.holdCleanupReport && w.uploads && w.sent[w.transmitted - 1].data[0] == 0x80))
            return false;
        out = w.reports[w.reportHead];
        w.reportHead = (w.reportHead + 1) % 4;
        --w.reportCount;
        return true;
    }
    E::Transport callbacks() {
        E::Transport t;
        t.context = this;
        t.receive = receive;
        t.transmit = transmit;
        t.takeTxReport = report;
        t.retire = [](void* ptr, C::SdoTxId, uint32_t) {
            Wire& w = *static_cast<Wire*>(ptr);
            ++w.retireCalls;
            if (!w.proveRetirement)
                return false;
            w.reportCount = 0; // Exact sole test handoff and every future report retired.
            return true;
        };
        return t;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    E::CanBusOwner owner;
    E::RawEvidence collector;
    uint64_t now = 0;
    Rig() : owner(services, wire.callbacks(), 42), collector(owner, services) {
        const L::ReadTarget t = target();
        CHECK(services.configure(0, t.channel, false, true));
        C::NetworkRoute network;
        network.binding = t.channel.binding;
        network.generation = 1;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    }
    E::RawEvidencePlan plan(bool state = false) {
        E::RawEvidencePlan p;
        auto& r = p.review;
        auto& a = r.plan;
        a.id = 100;
        a.target = target();
        a.deadlineUs = 5000000;
        a.options.queueTimeoutUs = a.options.responseTimeoutUs = 250000;
        label(a.fixtureId, "synthetic-only");
        label(a.softwareRevision, "0123456789abcdef0123456789abcdef01234567");
        label(a.firmware, "synthetic-unknown");
        label(a.dialect, "raw-unresolved");
        label(a.authorization, "offline finite experiment");
        label(a.observations, "test record");
        label(r.sourceReview, "source discrepancies unresolved");
        label(r.independentReview, "offline test review");
        label(r.fixtureReview, "no hardware");
        label(r.independentControl, "offline control");
        label(r.failurePlan, "stop; retain; no retry");
        label(r.reconciliation, "inspect raw bytes");
        p.firstAuthorizationId = 1000;
        p.includeState = state;
        return p;
    }
    void turn(uint64_t time, bool collect = true, uint8_t tx = 2) {
        now = wire.now = time;
        E::ServiceBudget b;
        b.rx = 8;
        b.tx = tx;
        b.events = 8;
        b.starts = 2;
        CHECK(owner.service(now, b));
        const unsigned received = wire.receives, sent = wire.transmitted;
        if (collect)
            CHECK(collector.service(now));
        CHECK(wire.receives == received && wire.transmitted == sent);
    }
    void finish() {
        for (unsigned i = 0; i < 100 && !collector.record()->terminal; ++i)
            turn(now + 1);
        CHECK(collector.record()->terminal);
    }
};
void passiveAndSuccess() {
    Rig r;
    CHECK(!r.collector.record() && !r.wire.transmitted && !r.wire.receives);
    r.turn(0);
    CHECK(!r.wire.transmitted);
    E::RawEvidencePlan p = r.plan(true);
    CHECK(r.collector.begin(p, 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(!r.wire.transmitted && r.owner.resources().reservations == 1);
    CHECK(r.collector.begin(p, 0) == E::EvidenceAdmission::BUSY);
    E::SequenceId other;
    CHECK(!r.owner.beginSequence(p.review.plan.target.channel.binding, 1, 0, other));
    p = E::RawEvidencePlan(); // No request metadata is borrowed after begin.
    r.finish();
    const auto* out = r.collector.record();
    CHECK(out->failure == E::RawEvidenceFailure::NONE && out->terminalCount == 20);
    CHECK(out->attemptedCount == 20 && r.wire.uploads == 20);
    CHECK(out->trafficCount == 60 && !out->evidenceLost && !out->tailOverflow);
    CHECK(out->terminals[1].service.size == 1 && out->terminals[1].service.data[0] == 0x32);
    CHECK(out->terminals[1].service.transfer.index == 0x1009);
    CHECK(out->terminals[19].service.transfer.index == 0x6061);
    CHECK(out->terminals[0].service.effectPossible && !out->terminals[0].service.effectUncertain);
    CHECK(out->terminals[0].service.protocolAcknowledged);
    C::SdoTransfer forbidden = out->terminals[0].service.transfer;
    forbidden.direction = C::SdoDirection::DOWNLOAD;
    forbidden.uploadSize = C::SdoUploadSize::EXACT;
    CHECK(!r.services.checkAdmission(0, forbidden, r.now));
    E::RawEvidenceRecord owned = *out;
    CHECK(r.collector.release() && !r.collector.record());
    for (auto& frame : r.wire.rx)
        frame = E::ReceivedFrame();
    CHECK(owned.terminals[1].service.data[0] == 0x32 && owned.traffic[0].frame.data[0] == 0x40);
    p = r.plan();
    p.review.plan.id = 101;
    p.firstAuthorizationId = 1020;
    CHECK(r.collector.begin(p, r.now) == E::EvidenceAdmission::ACCEPTED);
    r.finish();
    CHECK(r.collector.record()->terminalCount == 18 && r.wire.uploads == 38);
    CHECK(r.collector.release());
}
void rejections() {
    for (unsigned i = 0; i < 8; ++i) {
        Rig r;
        E::RawEvidencePlan p = r.plan();
        switch (i) {
        case 0:
            p.firstAuthorizationId = UINT32_MAX;
            break;
        case 1:
            p.review.plan.target.nodeGeneration = 2;
            break;
        case 2:
            p.review.independentControl[0] = 0;
            break;
        case 3:
            p.review.plan.options.responseTimeoutUs = 0;
            break;
        case 4:
            p.review.plan.options.queueTimeoutUs = 249999;
            break;
        case 5:
            p.review.plan.deadlineUs = 0;
            break;
        case 6:
            p.slot = 1;
            break;
        default:
            ++p.review.plan.target.channel.clientToServerId;
            break;
        }
        CHECK(r.collector.begin(p, 0) == E::EvidenceAdmission::INVALID);
        CHECK(!r.collector.record() && !r.wire.transmitted &&
              r.owner.resources().reservations == 0);
    }
}
void failures() {
    const Wire::Mode modes[] = {Wire::Mode::SILENT,    Wire::Mode::WRONG_OBJECT,
                                Wire::Mode::BOOTUP,    Wire::Mode::SERVER_ABORT,
                                Wire::Mode::OVERSIZED, Wire::Mode::MALFORMED};
    for (const auto mode : modes) {
        Rig r;
        r.wire.mode = mode;
        CHECK(r.collector.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
        r.turn(1);
        r.turn(2);
        r.turn(3);
        if (!r.collector.record()->terminal)
            r.turn(250003);
        const auto* out = r.collector.record();
        CHECK(out->terminal && out->failure == E::RawEvidenceFailure::SERVICE);
        CHECK(out->terminalCount == 1 && out->attemptedCount == 1 && r.wire.uploads == 1);
        CHECK(out->terminals[0].service.outcome != C::SdoOutcome::SUCCESS);
        const auto terminal = out->terminals[0];
        const uint64_t completed = out->completedUs;
        r.turn(r.now + 1);
        r.turn(r.now + 1);
        CHECK(r.wire.uploads == 1 && out->completedUs == completed);
        CHECK(!std::memcmp(&terminal, &out->terminals[0], sizeof terminal));
        CHECK(r.collector.release());
    }
}
void replyBeforeFinalTxReport() {
    Rig r;
    r.wire.holdReport = true;
    CHECK(r.collector.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    r.turn(1); // First grant/admission.
    r.turn(2); // First controller enqueue; final report deliberately withheld.
    r.turn(3); // Matching reply settles the service before the TX report.
    const auto* record = r.collector.record();
    CHECK(!record->terminal && record->terminalCount == 1 && record->attemptedCount == 1);
    CHECK(record->terminals[0].service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(record->terminals[0].service.protocolAcknowledged);
    CHECK(r.wire.uploads == 1 && r.owner.stats().queueRejected == 0);
    C::SdoTransfer next = record->terminals[0].service.transfer;
    next.index = 0x1009;
    next.request = next.step = 2;
    CHECK(!r.services.checkAdmission(0, next, r.now)); // No premature next-field grant.
    const E::Completion first = record->terminals[0];
    for (unsigned i = 0; i < 3; ++i) {
        r.turn(r.now + 1);
        CHECK(!record->terminal && record->attemptedCount == 1 && record->terminalCount == 1);
        CHECK(r.wire.uploads == 1 && r.owner.stats().queueRejected == 0);
        CHECK(!r.services.checkAdmission(0, next, r.now));
    }
    r.wire.holdReport = false;
    r.turn(r.now + 1);
    CHECK(!record->terminal && record->attemptedCount == 2 && record->terminalCount == 1);
    CHECK(r.wire.uploads == 1 && r.owner.stats().queueRejected == 0);
    CHECK(r.services.checkAdmission(0, next, r.now)); // Granted exactly once after retirement.
    CHECK(!std::memcmp(&first, &record->terminals[0], sizeof first));
    r.finish();
    CHECK(record->failure == E::RawEvidenceFailure::NONE && record->terminalCount == 18);
    CHECK(record->attemptedCount == 18 && r.wire.uploads == 18);
    CHECK(!record->evidenceLost && r.owner.stats().queueRejected == 0);
    CHECK(r.collector.release());

    Rig deadline;
    auto plan = deadline.plan();
    plan.review.plan.deadlineUs = 10;
    deadline.wire.holdReport = true;
    CHECK(deadline.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
    deadline.turn(1);
    deadline.turn(2);
    deadline.turn(3);
    CHECK(!deadline.collector.record()->terminal);
    deadline.turn(10);
    CHECK(deadline.collector.record()->terminal);
    CHECK(deadline.collector.record()->failure == E::RawEvidenceFailure::DEADLINE);
    CHECK(deadline.collector.record()->attemptedCount == 1);
    CHECK(deadline.collector.record()->terminalCount == 1 && deadline.wire.uploads == 1);
    CHECK(deadline.collector.record()->completedUs == 10 && !deadline.collector.release());
    deadline.wire.holdReport = false;
    deadline.turn(11);
    CHECK(deadline.collector.record()->completedUs == 10 && deadline.wire.uploads == 1);
    CHECK(deadline.collector.release());
}
void cleanupTail() {
    Rig r;
    r.wire.mode = Wire::Mode::MALFORMED;
    CHECK(r.collector.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    r.turn(1);
    r.turn(2);
    r.turn(3, true, 0);
    CHECK(r.collector.record()->terminal &&
          r.collector.record()->terminals[0].service.clientAbortQueued);
    CHECK(!r.owner.localTxRetired(0)); // Native cleanup has not crossed the owner yet.
    CHECK(!r.collector.release());
    const auto original = r.collector.record()->terminals[0];
    r.wire.holdCleanupReport = true;
    r.turn(4);
    CHECK(r.wire.sent[r.wire.transmitted - 1].data[0] == 0x80);
    CHECK(!r.collector.release() && r.collector.record()->tailCount == 1);
    CHECK(!r.owner.localTxRetired(0)); // Accepted SDK cleanup still awaits its final report.
    r.wire.holdCleanupReport = false;
    r.turn(5);
    CHECK(r.collector.record()->tailCount == 2);
    CHECK(r.owner.localTxRetired(0));
    CHECK(!std::memcmp(&original, &r.collector.record()->terminals[0], sizeof original));
    CHECK(r.collector.release());
}
void authoritativeRetirement() {
    Rig lost;
    CHECK(lost.owner.localTxRetired(0));
    CHECK(!lost.owner.localTxRetired(1)); // Unconfigured, even though its storage is empty.
    CHECK(!lost.owner.localTxRetired(C::SdoClient::CHANNELS));
    CHECK(!lost.owner.localTxRetired(UINT8_MAX));
    lost.wire.holdReport = true;
    lost.wire.mode = Wire::Mode::SILENT;
    CHECK(lost.collector.begin(lost.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    lost.turn(1);
    lost.turn(2, false); // An accepted TX's diagnostic is not collected yet.
    CHECK(!lost.owner.localTxRetired(0));
    for (unsigned i = 0; i < 8; ++i) {
        M::Frame f;
        f.id = 0x300;
        lost.wire.push(f);
    }
    lost.turn(3); // The ring overwrites that TX; collector detects loss and cancels.
    CHECK(lost.collector.record()->terminal && lost.collector.record()->evidenceLost);
    CHECK(lost.collector.record()->failure == E::RawEvidenceFailure::EVIDENCE_LOSS);
    CHECK(lost.wire.reportCount == 1 && lost.wire.retireCalls == 1);
    CHECK(!lost.owner.localTxRetired(0) && !lost.collector.release());
    lost.wire.holdReport = false;
    lost.turn(4);
    CHECK(lost.owner.localTxRetired(0) && lost.collector.release());

    Rig retired;
    retired.wire.holdReport = true;
    retired.wire.proveRetirement = true;
    CHECK(retired.collector.begin(retired.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    retired.turn(1);
    retired.turn(2);
    retired.turn(3); // Reply succeeds; callback retires the external handoff and its reports.
    CHECK(retired.wire.retireCalls == 1 && retired.wire.reportCount == 0);
    CHECK(retired.owner.localTxRetired(0));
    CHECK(!retired.collector.record()->terminal && retired.collector.record()->attemptedCount == 2);
    CHECK(retired.owner.stats().queueRejected == 0);
    retired.finish();
    CHECK(retired.collector.record()->failure == E::RawEvidenceFailure::NONE);
    CHECK(retired.wire.uploads == 18 && retired.wire.retireCalls == 18);
    CHECK(retired.collector.release());
}
void prefixAndInterruption() {
    Rig r;
    r.wire.mode = Wire::Mode::SERVER_ABORT;
    r.wire.failureAt = 2;
    CHECK(r.collector.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    r.finish();
    CHECK(r.collector.record()->terminalCount == 2 && r.wire.uploads == 2);
    CHECK(r.collector.record()->terminals[0].service.outcome == C::SdoOutcome::SUCCESS);
    CHECK(r.collector.record()->terminals[1].service.outcome == C::SdoOutcome::SERVER_ABORT);
    CHECK(r.collector.record()->terminals[1].service.effectUncertain);
    const E::RawEvidenceRecord saved = *r.collector.record();
    CHECK(!r.collector.service(r.now - 1));
    CHECK(!std::memcmp(&saved, r.collector.record(), sizeof saved));
    CHECK(r.collector.release());

    Rig lifecycle;
    CHECK(lifecycle.collector.begin(lifecycle.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(lifecycle.services.reboot(1, 0));
    CHECK(lifecycle.collector.service(0));
    CHECK(lifecycle.collector.record()->failure == E::RawEvidenceFailure::INVALIDATED);
    CHECK(!lifecycle.wire.transmitted && !lifecycle.collector.record()->attemptedCount);
    CHECK(lifecycle.collector.release());

    Rig deadline;
    auto p = deadline.plan();
    p.review.plan.deadlineUs = 1;
    CHECK(deadline.collector.begin(p, 0) == E::EvidenceAdmission::ACCEPTED);
    deadline.turn(1);
    CHECK(deadline.collector.record()->failure == E::RawEvidenceFailure::DEADLINE);
    CHECK(!deadline.wire.transmitted && !deadline.collector.record()->attemptedCount);
    CHECK(deadline.collector.release());
}
void admissionAndGrantFailure() {
    Rig r;
    r.turn(0);
    L::ReadTarget second = target();
    second.channel.binding.logicalAxisId = 102;
    second.channel.binding.endpoint.address = 2;
    second.channel.clientToServerId = 0x602;
    second.channel.serverToClientId = 0x582;
    CHECK(r.services.configure(1, second.channel, true, true));
    C::NetworkRoute n;
    n.binding = second.channel.binding;
    n.generation = 1;
    n.heartbeatId = 0x702;
    n.emcyId = 0x82;
    CHECK(r.services.configureNetwork(1, n, C::NetworkObserver(), 0));
    E::SequenceId sequence;
    CHECK(r.owner.beginSequence(second.channel.binding, 3, 0, sequence));
    CHECK(r.owner.beginOperation(sequence, 1, 5000000, 0));
    for (unsigned i = 0; i < E::CanBusOwner::REQUESTS; ++i) {
        E::BusRequest req;
        req.sequence = sequence;
        auto& t = req.transfer;
        t.channel = second.channel;
        t.operation = 1;
        t.request = t.step = i + 1;
        t.index = 0x1000;
        t.size = 4;
        t.deadlineUs = 5000000;
        t.queueTimeoutUs = t.responseTimeoutUs = 250000;
        t.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        E::RequestId id;
        CHECK(r.owner.admit(req, 0, id) == E::BusAdmission::ACCEPTED);
    }
    CHECK(r.collector.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(r.collector.service(0));
    CHECK(r.collector.record()->failure == E::RawEvidenceFailure::ADMISSION);
    CHECK(r.collector.record()->admission == E::BusAdmission::RESULTS_FULL);
    CHECK(r.collector.record()->attemptedCount == 1 && !r.wire.transmitted);
    CHECK(r.collector.release()); // Other producer's queued work remains owned and untouched.
    CHECK(r.owner.resources().queued == E::CanBusOwner::REQUESTS);

    Rig grant;
    auto p = grant.plan();
    CHECK(grant.collector.begin(p, 0) == E::EvidenceAdmission::ACCEPTED);
    grant.finish();
    CHECK(grant.collector.release());
    p.review.plan.id = 101; // Stale review serial must not authorize a new operation.
    CHECK(grant.collector.begin(p, grant.now) == E::EvidenceAdmission::ACCEPTED);
    CHECK(grant.collector.service(grant.now));
    CHECK(grant.collector.record()->failure == E::RawEvidenceFailure::AUTHORIZATION);
    CHECK(grant.collector.record()->attemptedCount == 1 && grant.wire.uploads == 18);
    CHECK(grant.collector.release());
}
void capacityAndNoRetry() {
    Rig r;
    CHECK(r.collector.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    // Copies are bounded even under unrelated traffic; owner RX still consumes it.
    for (unsigned turn = 1; turn <= 15 && !r.collector.record()->terminal; ++turn) {
        for (unsigned i = 0; i < 6; ++i) {
            M::Frame f;
            f.id = 0x300;
            r.wire.push(f);
        }
        r.turn(turn);
    }
    CHECK(r.collector.record()->terminal && r.collector.record()->evidenceLost);
    CHECK(r.collector.record()->failure == E::RawEvidenceFailure::EVIDENCE_LOSS);
    CHECK(r.collector.record()->trafficCount <= E::RAW_EVIDENCE_TRAFFIC);
    const unsigned uploads = r.wire.uploads;
    for (unsigned i = 0; i < 5; ++i)
        r.turn(r.now + 1);
    CHECK(r.wire.uploads == uploads);
    for (unsigned turn = 0; turn < 4; ++turn) {
        for (unsigned i = 0; i < 6; ++i) {
            M::Frame f;
            f.id = 0x300;
            r.wire.push(f);
        }
        r.turn(r.now + 1);
    }
    CHECK(r.collector.record()->tailCount == E::RAW_EVIDENCE_TAIL &&
          r.collector.record()->tailOverflow);
}
} // namespace
int main() {
    passiveAndSuccess();
    rejections();
    failures();
    replyBeforeFinalTxReport();
    cleanupTail();
    authoritativeRetirement();
    prefixAndInterruption();
    admissionAndGrantFailure();
    capacityAndNoRetry();
    std::printf("raw evidence: %u checks passed\n", checks);
    return 0;
}
