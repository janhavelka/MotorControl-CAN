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
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
// Independent EDS width/address vectors; manual access disagreements remain raw.
const uint16_t readinessIndexes[30] = {
    0x1001, 0x603f, 0x6041, 0x6061, 0x2100, 0x2101, 0x6064, 0x606c, 0x607a, 0x6081,
    0x6083, 0x6084, 0x605d, 0x60ff, 0x2200, 0x2201, 0x2202, 0x607d, 0x607d, 0x2500,
    0x2501, 0x2400, 0x2400, 0x2400, 0x2400, 0x2400, 0x2400, 0x2400, 0x2400, 0x60fd};
const uint8_t readinessSubindexes[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                         0, 0, 1, 2, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0};
const uint8_t readinessWidths[30] = {1, 2, 2, 1, 2, 2, 4, 4, 4, 4, 4, 4, 2, 4, 2,
                                     2, 2, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4};
// CiA 301 "not transferable in the present device state"; only its exact retention matters.
const uint32_t presentStateAbort = 0x08000022u;
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
    E::RawEvidence* collector = nullptr;
    unsigned guardCalls = 0;
    enum class Mode { SUCCESS, SILENT, WRONG_OBJECT, BOOTUP, SERVER_ABORT, OVERSIZED, MALFORMED };
    Mode mode = Mode::SUCCESS;
    uint64_t now = 0;
    O::ReceivedFrame rx[32];
    O::TxReport reports[4];
    M::Frame sent[64];
    unsigned rxHead = 0, rxCount = 0, reportHead = 0, reportCount = 0;
    unsigned transmitted = 0, uploads = 0, receives = 0;
    unsigned failureAt = 0; ///< Zero applies selected fault to every field.
    bool holdCleanupReport = false;
    bool holdReport = false;
    bool proveRetirement = false;
    unsigned retireCalls = 0;
    bool heartbeat = false;
    bool motionReadiness = false;
    uint16_t heartbeatMs = 0;
    bool pdoEntry = false; ///< Emulate one PDO communication entry for exact reads and one write.
    uint16_t pdoIndex = 0;
    uint8_t pdoSubindex = 0, pdoWidth = 0;
    uint32_t pdoValue = 0;       ///< Entry bytes; only an acknowledged download changes them.
    bool pdoIgnoreWrite = false; ///< Acknowledge the download but keep the old bytes.
    void push(const M::Frame& frame) {
        CHECK(rxCount < 32);
        auto& out = rx[(rxHead + rxCount++) % 32];
        out.frame = frame;
        out.receivedTimeKnown = true;
        out.receivedUs = now;
        out.controllerEpoch = 1;
    }
    static bool receive(void* ptr, O::ReceivedFrame& out) {
        Wire& w = *static_cast<Wire*>(ptr);
        ++w.receives;
        if (!w.rxCount)
            return false;
        out = w.rx[w.rxHead];
        w.rxHead = (w.rxHead + 1) % 32;
        --w.rxCount;
        return true;
    }
    static bool transmit(void* ptr, const M::Frame& request, O::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(ptr);
        CHECK(request.id == 0x601 && epoch == 1 && request.payloadLength == 8);
        CHECK(request.data[0] == 0x40 || request.data[0] == 0x80 ||
              (w.heartbeat && request.data[0] == 0x2b) ||
              (w.pdoEntry && request.data[0] == uint8_t(0x23u | ((4u - w.pdoWidth) << 2))));
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
            unsigned width = w.uploads == 2 ? 1 : w.uploads % 4 + 1;
            if (w.motionReadiness) {
                CHECK(w.uploads <= 30 && request.data[0] == 0x40);
                const unsigned field = w.uploads - 1;
                CHECK((unsigned(request.data[1]) | unsigned(request.data[2]) << 8) ==
                      readinessIndexes[field]);
                CHECK(request.data[3] == readinessSubindexes[field]);
                width = readinessWidths[field];
            }
            reply.data[0] = uint8_t(0x43u | ((4u - width) << 2));
            reply.data[1] = request.data[1];
            reply.data[2] = request.data[2];
            reply.data[3] = request.data[3];
            for (unsigned i = 0; i < 4; ++i)
                reply.data[4 + i] = uint8_t(0x30 + w.uploads + i);
            if (w.motionReadiness) {
                const unsigned field = w.uploads - 1;
                const uint32_t value = field == 12   ? 0xffffu
                                       : field == 8  ? 0xfffffff0u
                                       : field == 15 ? 1000u
                                                     : uint32_t(field);
                for (unsigned i = 0; i < 4; ++i)
                    reply.data[4 + i] = uint8_t(value >> (8 * i));
            }
            if (w.heartbeat) {
                CHECK(request.data[1] == 0x17 && request.data[2] == 0x10 && request.data[3] == 0);
                if (request.data[0] == 0x2b) {
                    CHECK(request.data[4] == 0xe8 && request.data[5] == 3);
                    w.heartbeatMs = 1000;
                    reply.data[0] = 0x60;
                } else {
                    reply.data[0] = 0x4b;
                    reply.data[4] = uint8_t(w.heartbeatMs);
                    reply.data[5] = uint8_t(w.heartbeatMs >> 8);
                }
                reply.data[6] = reply.data[7] = 0;
            }
            if (w.pdoEntry) {
                CHECK((unsigned(request.data[1]) | unsigned(request.data[2]) << 8) == w.pdoIndex);
                CHECK(request.data[3] == w.pdoSubindex);
                if (request.data[0] == 0x40) {
                    reply.data[0] = uint8_t(0x43u | ((4u - w.pdoWidth) << 2));
                    for (unsigned i = 0; i < 4; ++i)
                        reply.data[4 + i] = i < w.pdoWidth ? uint8_t(w.pdoValue >> (8 * i)) : 0;
                } else {
                    uint32_t written = 0;
                    for (unsigned i = 0; i < w.pdoWidth; ++i)
                        written |= uint32_t(request.data[4 + i]) << (8 * i);
                    if (mode != Mode::SERVER_ABORT && !w.pdoIgnoreWrite)
                        w.pdoValue = written;
                    reply.data[0] = 0x60;
                    for (unsigned i = 0; i < 4; ++i)
                        reply.data[4 + i] = 0;
                }
                if (mode == Mode::SERVER_ABORT)
                    for (unsigned i = 0; i < 4; ++i)
                        reply.data[4 + i] = uint8_t(presentStateAbort >> (8 * i));
            }
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
    static bool report(void* ptr, O::TxReport& out) {
        Wire& w = *static_cast<Wire*>(ptr);
        if (!w.reportCount || w.holdReport ||
            (w.holdCleanupReport && w.uploads && w.sent[w.transmitted - 1].data[0] == 0x80))
            return false;
        out = w.reports[w.reportHead];
        w.reportHead = (w.reportHead + 1) % 4;
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
            CHECK(w.collector);
            C::SdoTransfer other = transfer;
            other.data[0] ^= 1;
            CHECK(!w.collector->preflight(sequence, other, now));
            O::SequenceId unrelated = sequence;
            ++unrelated.generation;
            CHECK(!w.collector->preflight(unrelated, transfer, now));
            CHECK(!w.collector->preflight(sequence, transfer, transfer.deadlineUs));
            return w.collector->preflight(sequence, transfer, now);
        };
        t.retire = [](void* ptr, O::TransportTxId, uint32_t) {
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
    O::CanBusOwner owner;
    E::RawEvidence collector;
    uint64_t now = 0;
    explicit Rig(M::Generation configuration = 1, M::Generation nodeGeneration = 1)
        : owner(services, wire.callbacks(), 42), collector(owner, services) {
        wire.collector = &collector;
        L::ReadTarget t = target();
        t.channel.binding.configurationGeneration = configuration;
        t.nodeGeneration = nodeGeneration;
        CHECK(services.configure(0, t.channel, false, true));
        C::NetworkRoute network;
        network.binding = t.channel.binding;
        network.generation = nodeGeneration;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    }
    E::RawEvidencePlan plan(bool state = false) {
        E::RawEvidencePlan p;
        auto& r = p.review;
        auto& a = r.plan;
        a.id = 100;
        a.target.channel = *services.channel(0);
        a.target.nodeGeneration = services.node(0)->route.generation;
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
    /// PDO_COMM_WRITE of 1800+set:subindex; the fake entry starts at baseline with the EDS width.
    E::RawEvidencePlan pdoWrite(uint8_t set, uint8_t subindex, uint8_t width, uint32_t baseline,
                                uint32_t value) {
        wire.pdoEntry = true;
        wire.pdoIndex = uint16_t(0x1800 + set);
        wire.pdoSubindex = subindex;
        wire.pdoWidth = width;
        wire.pdoValue = baseline;
        E::RawEvidencePlan p = plan();
        p.inventory = E::RawEvidenceInventory::PDO_COMM_WRITE;
        p.firstAuthorizationId = 6000;
        p.pdo.set = set;
        p.pdo.subindex = subindex;
        p.pdo.baseline = baseline;
        p.pdo.value = value;
        return p;
    }
    void turn(uint64_t time, bool collect = true, uint8_t tx = 2) {
        now = wire.now = time;
        O::ServiceBudget b;
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
    O::SequenceId other;
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
        frame = O::ReceivedFrame();
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
    const O::Completion first = record->terminals[0];
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
    // Contract change (C-06): one full RX turn no longer overflows TRACE_CAPACITY, so this
    // withholds collection for four. The ring overwrites that TX and the collector detects loss.
    for (unsigned round = 0; round < 4; ++round) {
        for (unsigned i = 0; i < 8; ++i) {
            M::Frame f;
            f.id = 0x300;
            lost.wire.push(f);
        }
        lost.turn(3 + round, round == 3);
    }
    CHECK(lost.owner.stats().traceOverwritten > 0);
    // Contract change (C-06): the frame already sent is not cancelled. It settles by itself,
    // here at its response timeout, and only then does the record close.
    CHECK(!lost.collector.record()->terminal && lost.collector.record()->evidenceLost);
    CHECK(lost.wire.reportCount == 1 && lost.wire.retireCalls == 0);
    lost.turn(250002);
    CHECK(lost.collector.record()->terminal && lost.collector.record()->evidenceLost);
    CHECK(lost.collector.record()->failure == E::RawEvidenceFailure::EVIDENCE_LOSS);
    CHECK(lost.collector.record()->terminalCount == 1 &&
          lost.collector.record()->terminals[0].service.outcome == C::SdoOutcome::TIMEOUT);
    CHECK(lost.wire.reportCount == 1 && lost.wire.retireCalls == 1 && lost.wire.transmitted == 1);
    CHECK(!lost.owner.localTxRetired(0) && !lost.collector.release());
    lost.wire.holdReport = false;
    lost.turn(250003);
    CHECK(lost.owner.localTxRetired(0) && lost.collector.release());

    // A field admitted but not started has no frame to settle, so loss withdraws it unsent.
    Rig unstarted;
    CHECK(unstarted.collector.begin(unstarted.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    unstarted.turn(1);
    CHECK(unstarted.wire.transmitted == 0);
    O::ServiceBudget receiveOnly; // An owner turn that starts nothing still fills the trace.
    receiveOnly.rx = 8;
    for (unsigned round = 0; round < 4; ++round) {
        for (unsigned i = 0; i < 8; ++i) {
            M::Frame f;
            f.id = 0x300;
            unstarted.wire.push(f);
        }
        CHECK(unstarted.owner.service(2 + round, receiveOnly));
    }
    CHECK(unstarted.owner.stats().traceOverwritten > 0 && unstarted.wire.transmitted == 0);
    CHECK(unstarted.collector.service(5));
    CHECK(unstarted.collector.record()->terminal &&
          unstarted.collector.record()->failure == E::RawEvidenceFailure::EVIDENCE_LOSS);
    CHECK(unstarted.collector.record()->terminals[0].reason == O::CompletionReason::CANCELLED);
    unstarted.turn(6);
    CHECK(unstarted.wire.transmitted == 0 && !unstarted.services.unresolved(0));
    CHECK(unstarted.collector.release());

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
    O::SequenceId sequence;
    CHECK(r.owner.beginSequence(second.channel.binding, 3, 0, sequence));
    CHECK(r.owner.beginOperation(sequence, 1, 5000000, 0));
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        O::BusRequest req;
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
        O::RequestId id;
        CHECK(r.owner.admit(req, 0, id) == O::BusAdmission::ACCEPTED);
    }
    CHECK(r.collector.begin(r.plan(), 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(r.collector.service(0));
    CHECK(r.collector.record()->failure == E::RawEvidenceFailure::ADMISSION);
    CHECK(r.collector.record()->admission == O::BusAdmission::RESULTS_FULL);
    CHECK(r.collector.record()->attemptedCount == 1 && !r.wire.transmitted);
    CHECK(r.collector.release()); // Other producer's queued work remains owned and untouched.
    CHECK(r.owner.resources().queued == O::CanBusOwner::REQUESTS);

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
    // Contract change (C-06): each turn now fits the trace, so only the record's bound trips.
    for (unsigned turn = 1; turn <= 25 && !r.collector.record()->terminal; ++turn) {
        for (unsigned i = 0; i < 7; ++i) {
            M::Frame f;
            f.id = 0x300;
            r.wire.push(f);
        }
        r.turn(turn);
    }
    CHECK(r.collector.record()->terminal && r.collector.record()->evidenceLost);
    CHECK(r.collector.record()->failure == E::RawEvidenceFailure::EVIDENCE_LOSS);
    CHECK(r.collector.record()->trafficCount == E::RAW_EVIDENCE_TRAFFIC);
    CHECK(r.owner.stats().traceOverwritten == 0);
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
void networkAndHeartbeat() {
    {
        Rig r;
        auto plan = r.plan(true);
        plan.includeNetwork = true;
        CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        r.finish();
        const auto& record = *r.collector.record();
        CHECK(record.failure == E::RawEvidenceFailure::NONE && record.terminalCount == 26);
        const uint16_t extra[] = {0x1017, 0x2100, 0x603f, 0x6064, 0x2201, 0x606c};
        for (unsigned i = 0; i < 6; ++i)
            CHECK(record.terminals[20 + i].service.transfer.index == extra[i]);
    }
    for (unsigned initial = 0; initial < 2; ++initial) {
        Rig r;
        r.wire.heartbeat = true;
        r.wire.heartbeatMs = initial ? 10 : 0;
        auto plan = r.plan();
        plan.enableHeartbeat = true;
        CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        r.finish();
        const auto& record = *r.collector.record();
        CHECK(record.failure ==
              (initial ? E::RawEvidenceFailure::READBACK : E::RawEvidenceFailure::NONE));
        CHECK(r.wire.transmitted == (initial ? 1u : 3u));
        if (!initial) {
            CHECK(r.wire.guardCalls == 1);
            CHECK(record.terminals[1].service.transfer.direction == C::SdoDirection::DOWNLOAD);
            CHECK(!r.collector.preflight(record.terminals[1].sequence,
                                        record.terminals[1].service.transfer, r.now));
            CHECK(record.terminals[2].service.size == 2 && r.wire.heartbeatMs == 1000);
            // Only this experiment republishes producer timing, from its exact readback.
            CHECK(r.services.node(0)->producerKnown && r.services.node(0)->producerTimeMs == 1000);
        }
        const unsigned sent = r.wire.transmitted;
        r.turn(r.now + 10000000);
        CHECK(r.wire.transmitted == sent); // No repeated write or restoration.
    }
    for (unsigned step = 1; step <= 3; ++step) {
        Rig r;
        r.wire.heartbeat = true;
        r.wire.mode = Wire::Mode::SILENT;
        r.wire.failureAt = step;
        auto plan = r.plan();
        plan.enableHeartbeat = true;
        CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        for (unsigned i = 0; i < 30 && !r.collector.record()->terminal; ++i)
            r.turn(r.now + 20000);
        CHECK(r.collector.record()->terminal);
        CHECK(r.collector.record()->failure == E::RawEvidenceFailure::SERVICE);
        CHECK(r.collector.record()->terminalCount == step);
        const unsigned sent = r.wire.transmitted;
        r.turn(r.now + 1000000);
        CHECK(r.wire.transmitted == sent);
    }
}
void motionReadinessUploadsOnly() {
    Rig r(9, 7); // Current route/epochs, including a previous configuration change.
    r.wire.motionReadiness = true;
    auto plan = r.plan();
    plan.inventory = E::RawEvidenceInventory::MOTION_READINESS;
    plan.firstAuthorizationId = 4000;
    CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
    CHECK(!r.wire.transmitted);
    r.finish();
    const auto* record = r.collector.record();
    CHECK(record->failure == E::RawEvidenceFailure::NONE && record->intendedCount == 30);
    CHECK(record->terminalCount == 30 && record->attemptedCount == 30 && r.wire.uploads == 30);
    for (unsigned i = 0; i < 30; ++i) {
        E::RawEvidenceField documentary;
        CHECK(E::motionReadinessField(i, documentary));
        CHECK(documentary.index == readinessIndexes[i] &&
              documentary.subindex == readinessSubindexes[i]);
        CHECK(documentary.documentedWidth == readinessWidths[i]);
        const auto& result = record->terminals[i].service;
        const auto& work = result.transfer;
        CHECK(work.index == readinessIndexes[i] && work.subindex == readinessSubindexes[i]);
        CHECK(work.direction == C::SdoDirection::UPLOAD && work.size == 4);
        CHECK(work.uploadSize == C::SdoUploadSize::SERVER_INDICATED &&
              result.size == readinessWidths[i]);
        CHECK(work.readEffect == C::SdoReadEffect::UNQUALIFIED && result.effectPossible);
        CHECK(work.channel.binding.configurationGeneration == 9 && result.nodeGeneration == 7);
        CHECK(work.index != 0x6040 && work.index != 0x6060 && work.index != 0x2300);
        CHECK(r.wire.sent[i].data[0] == 0x40);
    }
    CHECK(record->terminals[8].service.data[3] == 0xff); // Negative I32 bits remain raw.
    CHECK(record->terminals[12].service.size == 2 && record->terminals[12].service.data[1] == 0xff);
    CHECK(record->terminals[15].service.data[0] == 0xe8 &&
          record->terminals[15].service.data[1] == 3);
    CHECK(!record->evidenceLost && !record->tailOverflow);
    r.turn(r.now + 1);
    CHECK(record->trafficCount + record->tailCount == 90);
    CHECK(r.collector.release());
    E::RawEvidenceField sentinel(0x1234, 5, 3);
    CHECK(!E::motionReadinessField(30, sentinel));
    CHECK(sentinel.index == 0x1234 && sentinel.subindex == 5 && sentinel.documentedWidth == 3);
    CHECK(r.collector.begin(plan, r.now) ==
          E::EvidenceAdmission::INVALID); // Same operation never replayed.
    for (unsigned flag = 0; flag < 4; ++flag) {
        Rig rejected;
        auto candidate = rejected.plan();
        candidate.inventory = E::RawEvidenceInventory::MOTION_READINESS;
        candidate.includeState = flag == 0;
        candidate.includeNetwork = flag == 1;
        candidate.enableHeartbeat = flag == 2;
        if (flag == 3)
            candidate.inventory = static_cast<E::RawEvidenceInventory>(255);
        CHECK(rejected.collector.begin(candidate, 0) == E::EvidenceAdmission::INVALID);
        CHECK(!rejected.collector.record() && !rejected.wire.transmitted);
    }
    for (unsigned at = 1; at <= 30; ++at) {
        Rig failed;
        failed.wire.motionReadiness = true;
        failed.wire.mode = Wire::Mode::SERVER_ABORT;
        failed.wire.failureAt = at;
        auto candidate = failed.plan();
        candidate.inventory = E::RawEvidenceInventory::MOTION_READINESS;
        candidate.firstAuthorizationId = 4000;
        CHECK(failed.collector.begin(candidate, 0) == E::EvidenceAdmission::ACCEPTED);
        failed.finish();
        CHECK(failed.collector.record()->failure == E::RawEvidenceFailure::SERVICE);
        CHECK(failed.collector.record()->terminalCount == at && failed.wire.uploads == at);
        failed.turn(failed.now + 1);
        CHECK(failed.wire.uploads == at && failed.collector.release());
    }
}
void pdoRecordUploads() {
    for (unsigned transmit = 0; transmit < 2; ++transmit) {
        Rig r;
        auto plan = r.plan();
        plan.inventory = E::RawEvidenceInventory::PDO_RECORD;
        plan.pdo.transmit = transmit != 0;
        plan.pdo.set = transmit ? 3 : 1;
        plan.pdo.subindex = 2; // Ignored: a record read never writes.
        plan.pdo.baseline = 255;
        plan.pdo.value = 1;
        plan.firstAuthorizationId = transmit ? UINT32_MAX - 8 : 5000; // Nine grants fit exactly.
        CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        CHECK(!r.wire.transmitted && r.collector.record()->intendedCount == 9);
        r.finish();
        const auto* record = r.collector.record();
        CHECK(record->failure == E::RawEvidenceFailure::NONE && record->terminalCount == 9);
        CHECK(record->attemptedCount == 9 && r.wire.uploads == 9 && r.wire.transmitted == 9);
        const uint16_t communication = transmit ? 0x1803 : 0x1401;
        const uint16_t mapping = transmit ? 0x1a03 : 0x1601;
        const uint8_t subindexes[9] = {1, 2, 3, 5, 0, 1, 2, 3, 4};
        for (unsigned i = 0; i < 9; ++i) {
            const uint16_t index = i < 4 ? communication : mapping;
            const auto& result = record->terminals[i].service;
            const auto& work = result.transfer;
            CHECK(work.index == index && work.subindex == subindexes[i]);
            CHECK(work.direction == C::SdoDirection::UPLOAD && work.size == 4);
            CHECK(work.uploadSize == C::SdoUploadSize::SERVER_INDICATED &&
                  work.readEffect == C::SdoReadEffect::UNQUALIFIED);
            CHECK(result.outcome == C::SdoOutcome::SUCCESS && result.effectPossible);
            const M::Frame& sent = r.wire.sent[i];
            CHECK(sent.data[0] == 0x40 &&
                  (unsigned(sent.data[1]) | unsigned(sent.data[2]) << 8) == index);
            CHECK(sent.data[3] == subindexes[i]);
        }
        r.turn(r.now + 10000000);
        CHECK(r.wire.transmitted == 9 && r.collector.release());
    }
}
void pdoCommunicationWrite() {
    struct Case {
        uint8_t set, subindex, width;
        uint32_t baseline, value;
    };
    // Widths are the EDS 1800..1803 types; values are raw entry bytes, no timer unit.
    const Case cases[] = {{0, 1, 4, 0x40000181u, 0xc0000181u},
                          {1, 2, 1, 255, 1},
                          {2, 3, 2, 0, 100},
                          {3, 5, 2, 100, 0x01f4}};
    for (const auto& c : cases) {
        Rig r;
        auto plan = r.pdoWrite(c.set, c.subindex, c.width, c.baseline, c.value);
        if (c.subindex == 1)
            plan.firstAuthorizationId = UINT32_MAX - 2; // Three grants fit exactly.
        // A known producer fact the PDO write must leave untouched.
        CHECK(r.services.setProducerInterval(0, r.services.node(0)->route, 100, 0));
        CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        CHECK(r.collector.record()->intendedCount == 3);
        r.finish();
        const auto* record = r.collector.record();
        CHECK(record->failure == E::RawEvidenceFailure::NONE && record->terminalCount == 3);
        CHECK(record->attemptedCount == 3 && r.wire.transmitted == 3 && r.wire.pdoValue == c.value);
        for (unsigned i = 0; i < 3; ++i) {
            const auto& result = record->terminals[i].service;
            CHECK(result.outcome == C::SdoOutcome::SUCCESS);
            CHECK(result.transfer.index == 0x1800 + c.set);
            CHECK(result.transfer.subindex == c.subindex);
            CHECK(result.transfer.size == c.width &&
                  result.transfer.uploadSize == C::SdoUploadSize::EXACT);
            CHECK(result.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED);
            CHECK(result.transfer.direction ==
                  (i == 1 ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD));
        }
        const auto& before = record->terminals[0].service;
        const auto& after = record->terminals[2].service;
        CHECK(before.size == c.width && after.size == c.width);
        const M::Frame& download = r.wire.sent[1];
        CHECK(download.data[0] == uint8_t(0x23u | ((4u - c.width) << 2)));
        CHECK((unsigned(download.data[1]) | unsigned(download.data[2]) << 8) == 0x1800u + c.set);
        CHECK(download.data[3] == c.subindex);
        for (unsigned i = 0; i < c.width; ++i) {
            CHECK(download.data[4 + i] == uint8_t(c.value >> (8 * i)));
            CHECK(record->terminals[1].service.transfer.data[i] == download.data[4 + i]);
            CHECK(before.data[i] == uint8_t(c.baseline >> (8 * i)));
            CHECK(after.data[i] == download.data[4 + i]);
        }
        if (c.subindex == 1) // Every COB-ID byte crosses the wire; only bit 31 changed.
            CHECK(download.data[4] == 0x81 && download.data[5] == 0x01 && download.data[6] == 0 &&
                  download.data[7] == 0xc0);
        CHECK(r.services.node(0)->producerKnown && r.services.node(0)->producerTimeMs == 100);
        r.turn(r.now + 10000000);
        CHECK(r.wire.transmitted == 3); // No repeated write, rollback or restoration.
        CHECK(r.collector.release());
    }
    // Generic CiA 301 transmission-type boundaries admitted locally; the drive decides.
    const uint32_t types[][2] = {{255, 0}, {1, 240}, {255, 254}, {254, 255}};
    for (const auto& type : types) {
        Rig r;
        CHECK(r.collector.begin(r.pdoWrite(0, 2, 1, type[0], type[1]), 0) ==
              E::EvidenceAdmission::ACCEPTED);
        r.finish();
        CHECK(r.collector.record()->failure == E::RawEvidenceFailure::NONE);
        CHECK(r.wire.pdoValue == type[1] && r.collector.release());
    }
}
void pdoWriteReadbackAndAbort() {
    {
        Rig r;
        auto plan = r.pdoWrite(0, 2, 1, 255, 1);
        r.wire.pdoValue = 254; // The entry differs from the reviewed baseline.
        CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        r.finish();
        const auto* record = r.collector.record();
        CHECK(record->failure == E::RawEvidenceFailure::READBACK);
        CHECK(record->terminalCount == 1 && record->attemptedCount == 1);
        CHECK(record->terminals[0].service.outcome == C::SdoOutcome::SUCCESS &&
              record->terminals[0].service.data[0] == 254);
        CHECK(r.wire.transmitted == 1 && r.wire.sent[0].data[0] == 0x40 && r.wire.pdoValue == 254);
        r.turn(r.now + 10000000);
        CHECK(r.wire.transmitted == 1 && r.collector.release()); // No download was admitted.
    }
    {
        Rig r;
        auto plan = r.pdoWrite(1, 3, 2, 0, 100);
        r.wire.pdoIgnoreWrite = true; // Acknowledged download, unchanged entry bytes.
        CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        r.finish();
        const auto* record = r.collector.record();
        CHECK(record->failure == E::RawEvidenceFailure::READBACK && record->terminalCount == 3);
        CHECK(record->terminals[1].service.outcome == C::SdoOutcome::SUCCESS &&
              record->terminals[1].service.protocolAcknowledged);
        const auto& readback = record->terminals[2].service;
        CHECK(readback.size == 2 && readback.data[0] == 0 && readback.data[1] == 0);
        r.turn(r.now + 10000000);
        CHECK(r.wire.transmitted == 3 && r.collector.release()); // No retry or rollback.
    }
    {
        Rig r;
        auto plan = r.pdoWrite(0, 3, 2, 0, 100);
        r.wire.mode = Wire::Mode::SERVER_ABORT;
        r.wire.failureAt = 2; // The drive rejects only the download.
        CHECK(r.services.setProducerInterval(0, r.services.node(0)->route, 100, 0));
        CHECK(r.collector.begin(plan, 0) == E::EvidenceAdmission::ACCEPTED);
        r.finish();
        const auto* record = r.collector.record();
        CHECK(record->failure == E::RawEvidenceFailure::SERVICE);
        CHECK(record->terminalCount == 2 && record->attemptedCount == 2 && r.wire.transmitted == 2);
        const auto& rejected = record->terminals[1].service;
        CHECK(rejected.transfer.direction == C::SdoDirection::DOWNLOAD);
        CHECK(rejected.outcome == C::SdoOutcome::SERVER_ABORT && !rejected.protocolAcknowledged);
        CHECK(rejected.abortValid && rejected.abortCode == presentStateAbort);
        CHECK(rejected.effectPossible && rejected.effectUncertain && r.wire.pdoValue == 0);
        CHECK(r.services.node(0)->producerKnown && r.services.node(0)->producerTimeMs == 100);
        r.turn(r.now + 10000000);
        CHECK(r.wire.transmitted == 2 && r.collector.release()); // No readback or retry.
    }
}
void pdoPlanRejections() {
    const auto RECORD = E::RawEvidenceInventory::PDO_RECORD;
    const auto WRITE = E::RawEvidenceInventory::PDO_COMM_WRITE;
    enum : unsigned { STATE = 1, NETWORK = 2, HEARTBEAT = 4 };
    struct Case {
        E::RawEvidenceInventory inventory;
        bool transmit;
        uint8_t set, subindex;
        uint32_t baseline, value;
        unsigned flags;
        uint32_t firstAuthorizationId;
    };
    const Case cases[] = {
        {RECORD, true, 4, 0, 0, 0, 0, 1000},  // No fifth set.
        {RECORD, false, 4, 0, 0, 0, 0, 1000},
        {WRITE, true, 4, 2, 255, 1, 0, 1000},
        {RECORD, true, 0, 0, 0, 0, STATE, 1000},
        {RECORD, true, 0, 0, 0, 0, NETWORK, 1000},
        {RECORD, true, 0, 0, 0, 0, STATE | NETWORK, 1000},
        {RECORD, true, 0, 0, 0, 0, HEARTBEAT, 1000},
        {WRITE, true, 0, 2, 255, 1, STATE, 1000},
        {WRITE, true, 0, 2, 255, 1, NETWORK, 1000},
        {WRITE, true, 0, 2, 255, 1, HEARTBEAT, 1000},
        {WRITE, false, 0, 2, 255, 1, 0, 1000}, // Receive PDO entries are never written.
        {WRITE, true, 0, 0, 2, 1, 0, 1000},    // Subindex outside 1, 2, 3, 5.
        {WRITE, true, 0, 4, 0, 1, 0, 1000},
        {WRITE, true, 0, 6, 0, 1, 0, 1000},
        {WRITE, true, 0, 2, 256, 1, 0, 1000}, // Wider than the U8 entry.
        {WRITE, true, 0, 2, 255, 0x101, 0, 1000},
        {WRITE, true, 0, 3, 0, 0x10000, 0, 1000}, // Wider than the U16 entries.
        {WRITE, true, 0, 5, 0x10000, 100, 0, 1000},
        {WRITE, true, 0, 3, 100, 100, 0, 1000},                // No change.
        {WRITE, true, 0, 1, 0x181, 0x182, 0, 1000},             // Identifier.
        {WRITE, true, 0, 1, 0x40000181u, 0x00000181u, 0, 1000}, // Bit 30 alone.
        {WRITE, true, 0, 1, 0x40000181u, 0x80000181u, 0, 1000}, // Bits 31 and 30.
        {WRITE, true, 0, 1, 0x00000181u, 0xa0000181u, 0, 1000}, // Bit 31 and frame kind.
        {WRITE, true, 0, 1, 0x00000181u, 0x20000181u, 0, 1000}, // Frame kind alone.
        {WRITE, true, 0, 2, 255, 241, 0, 1000}, // Reserved transmission types.
        {WRITE, true, 0, 2, 255, 251, 0, 1000},
        {WRITE, true, 0, 2, 255, 252, 0, 1000}, // RTR-only transmission types.
        {WRITE, true, 0, 2, 255, 253, 0, 1000},
        {static_cast<E::RawEvidenceInventory>(4), true, 0, 2, 255, 1, 0, 1000},
        {RECORD, true, 0, 0, 0, 0, 0, UINT32_MAX - 7}, // Nine grants do not fit.
        {WRITE, true, 0, 2, 255, 1, 0, UINT32_MAX - 1}, // Three grants do not fit.
    };
    for (const auto& c : cases) {
        Rig r;
        E::RawEvidencePlan p = r.plan();
        p.inventory = c.inventory;
        p.pdo.transmit = c.transmit;
        p.pdo.set = c.set;
        p.pdo.subindex = c.subindex;
        p.pdo.baseline = c.baseline;
        p.pdo.value = c.value;
        p.includeState = (c.flags & STATE) != 0;
        p.includeNetwork = (c.flags & NETWORK) != 0;
        p.enableHeartbeat = (c.flags & HEARTBEAT) != 0;
        p.firstAuthorizationId = c.firstAuthorizationId;
        CHECK(r.collector.begin(p, 0) == E::EvidenceAdmission::INVALID);
        CHECK(!r.collector.record() && r.owner.resources().reservations == 0);
        CHECK(r.collector.service(1) && !r.wire.transmitted);
        // The plan's first grant identity is still unused: begin() granted nothing.
        C::SdoTransfer probe;
        probe.channel = *r.services.channel(0);
        probe.operation = probe.request = probe.step = 1;
        probe.index = 0x1800;
        probe.subindex = 1;
        probe.size = 4;
        probe.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
        probe.deadlineUs = 5000000;
        probe.queueTimeoutUs = probe.responseTimeoutUs = 250000;
        CHECK(r.services.authorizeUploadExperiment(0, probe, r.services.node(0)->route.generation,
                                                   c.firstAuthorizationId, 1));
        r.services.clearUploadExperiment(0);
    }
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
    networkAndHeartbeat();
    motionReadinessUploadsOnly();
    pdoRecordUploads();
    pdoCommunicationWrite();
    pdoWriteReadbackAndAbort();
    pdoPlanRejections();
    std::printf("raw evidence: %u checks passed\n", checks);
    return 0;
}
