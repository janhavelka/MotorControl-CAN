// SPDX-License-Identifier: MIT
// The S2 bench's K and H lines through the real owner and service store over a synthetic two-node
// communication dictionary. Widths are transcribed from the EDS independently of the profile.
// The firmware turn is an ordinary owner service; the transport's handoff guard plays main.cpp's
// download dispatcher, in which only the H record takes part.
#include "CommunicationBench.h"
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
} // namespace
#include "support/OwnerRig.h"
namespace {
L::ReadTarget target(unsigned node) {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = node;
    t.channel.binding.endpoint.profile = 1;
    t.channel.binding.endpoint.address = node;
    t.channel.binding.bindingGeneration = t.channel.binding.configurationGeneration = 1;
    t.channel.clientToServerId = 0x600 + node;
    t.channel.serverToClientId = 0x580 + node;
    t.nodeGeneration = 1;
    return t;
}
// EDS widths (LC_StepDriverCAN_V1.1.eds): 1005/1014 U32, 1017 U16, 1200:00 U8 and :01/:02 U32,
// 2203 U16; PDO communication :00 U8, :01 U32, :02 U8, :03 U16, :04 U8, :05 U16; mapping :00 U8,
// :01..04 U32. Zero means the dictionary has no such entry.
unsigned width(uint16_t index, uint8_t sub) {
    if (index == 0x1000 || index == 0x1005 || index == 0x1014)
        return sub ? 0 : 4;
    if (index == 0x1017 || index == 0x2203)
        return sub ? 0 : 2;
    if (index == 0x1200)
        return sub == 0 ? 1 : sub <= 2 ? 4 : 0;
    if ((index >= 0x1400 && index <= 0x1403) || (index >= 0x1800 && index <= 0x1803)) {
        const unsigned w[6] = {1, 4, 1, 2, 1, 2};
        return sub < 6 ? w[sub] : 0;
    }
    if ((index >= 0x1600 && index <= 0x1603) || (index >= 0x1a00 && index <= 0x1a03))
        return sub == 0 ? 1 : sub <= 4 ? 4 : 0;
    return 0;
}
uint32_t value(unsigned node, uint16_t index, uint8_t sub, uint16_t heartbeat) {
    const unsigned n = (index & 0xff) % 4;
    switch (index & 0xff00) {
    case 0x1000:
        return index == 0x1000   ? 0x00040192
               : index == 0x1005 ? 0x80
               : index == 0x1014 ? 0x80 + node
                                 : heartbeat;
    case 0x1200:
        return sub == 0 ? 2 : sub == 1 ? 0x600 + node : 0x580 + node;
    case 0x2200:
        return 0;
    case 0x1400:
    case 0x1800: {
        const uint32_t base = (index & 0xff00) == 0x1800 ? 0x180 : 0x200;
        const uint32_t cob = (n ? 0x80000000u : 0) + base + 0x100 * n + node;
        const uint32_t v[6] = {5, cob, 255, (index == 0x1800) ? 100u : 0u, 0, 0};
        return v[sub];
    }
    case 0x1600:
        return 0;
    case 0x1a00: {
        const uint32_t v[5] = {3, 0x60410010, 0x60610008, 0x60640020, 0};
        return n == 0 ? v[sub] : 0;
    }
    }
    return 0;
}
struct Wire : OwnerRig::WireBase {
    uint64_t now = 0;
    E::HeartbeatCommission* heartbeat = nullptr;
    uint16_t stored[3] = {0, 0, 0}; // 1017:00 per node.
    uint16_t storeOverride = 0;     // Nonzero: a download stores this instead (readback mismatch).
    // Failure injection on the n-th request frame to `failNode` (1-based), or on its download.
    enum class Fault { NONE, ABORT, SILENT, BOOTUP } fault = Fault::NONE;
    unsigned failNode = 0, failAt = 0, frames[3] = {}, downloads[3] = {}, guardCalls = 0;
    bool silentDownload = false;
    uint8_t stateWithAck = 0; // Nonzero: a heartbeat in this state arrives just before the write's ACK.
    static bool guard(void* context, O::SequenceId sequence, const C::SdoTransfer& t, uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        ++w.guardCalls;
        return w.heartbeat && w.heartbeat->preflight(sequence, t, now);
    }
    static bool transmit(void* context, const M::Frame& f, O::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        CHECK(f.id == 0x601 || f.id == 0x602);
        const unsigned node = f.id - 0x600;
        CHECK(OwnerRig::WireBase::transmit(context, f, id, epoch));
        const uint16_t index = uint16_t(f.data[1] | f.data[2] << 8);
        const uint8_t sub = f.data[3];
        ++w.frames[node];
        if (w.fault != Fault::NONE && node == w.failNode && w.frames[node] == w.failAt) {
            if (w.fault == Fault::SILENT)
                return true;
            if (w.fault == Fault::BOOTUP) {
                M::Frame boot = OwnerRig::frame(0x700 + node, 1);
                w.push(boot, w.now, epoch);
                return true;
            }
        }
        M::Frame reply = OwnerRig::frame(0x580 + node, 8);
        reply.data[1] = f.data[1];
        reply.data[2] = f.data[2];
        reply.data[3] = sub;
        if (f.data[0] == 0x2b) {
            // Only the one reviewed download ever reaches this wire: 1017:00, two bytes.
            CHECK(index == 0x1017 && sub == 0);
            ++w.downloads[node];
            if (w.silentDownload)
                return true;
            w.stored[node] = w.storeOverride ? w.storeOverride : uint16_t(f.data[4] | f.data[5] << 8);
            if (w.stateWithAck) {
                // Producer now running: its first heartbeat is processed in the ACK's owner turn.
                M::Frame beat = OwnerRig::frame(0x700 + node, 1);
                beat.data[0] = w.stateWithAck;
                w.push(beat, w.now, epoch);
            }
            reply.data[0] = 0x60;
            w.push(reply, w.now, epoch);
            return true;
        }
        CHECK(f.data[0] == 0x40); // Every other frame is an expedited upload request.
        const unsigned size = width(index, sub);
        if (!size || (w.fault == Fault::ABORT && node == w.failNode && w.frames[node] == w.failAt)) {
            reply.data[0] = 0x80;
            const uint32_t abort = 0x06090011; // Subindex does not exist.
            for (unsigned i = 0; i < 4; ++i)
                reply.data[4 + i] = uint8_t(abort >> (8 * i));
            w.push(reply, w.now, epoch);
            return true;
        }
        const uint32_t raw = value(node, index, sub, w.stored[node]);
        reply.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
        for (unsigned i = 0; i < 4; ++i)
            reply.data[4 + i] = uint8_t(raw >> (8 * i));
        w.push(reply, w.now, epoch);
        return true;
    }
    O::Transport callbacks() {
        O::Transport t = plumbing();
        t.transmit = transmit;
        t.beforeTransmit = guard;
        return t;
    }
};
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    E::CommunicationReads reads;
    E::HeartbeatCommission commission;
    uint64_t now = 10;
    explicit Rig(unsigned routes = 2)
        : owner(services, wire.callbacks(), 28), reads(owner, services), commission(owner, services) {
        wire.heartbeat = &commission;
        for (unsigned node = 1; node <= routes; ++node) {
            const L::ReadTarget t = target(node);
            CHECK(services.configure(uint8_t(node - 1), t.channel, false, true));
            C::NetworkRoute route;
            route.binding = t.channel.binding;
            route.generation = 1;
            route.heartbeatId = 0x700 + node;
            route.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), route, C::NetworkObserver(), 0));
        }
    }
    void turn(uint64_t step = 100) {
        now += step;
        wire.now = now;
        CHECK(owner.service(now, OwnerRig::budget()));
        CHECK(reads.service(owner.lastTimeUs()));
        CHECK(commission.service(owner.lastTimeUs()));
    }
    template <class T> void finish(const T& record, unsigned limit = 20000) {
        for (unsigned i = 0; i < limit && !(record.record() && record.record()->terminal); ++i)
            turn();
        CHECK(record.record() && record.record()->terminal);
    }
    E::CommunicationBenchRequest readRequest(unsigned node, uint32_t id = 100, uint32_t grant = 10001) const {
        E::CommunicationBenchRequest r;
        r.id = id;
        r.node = uint8_t(node);
        r.firstGrant = grant;
        r.deadlineUs = now + 15000000;
        return r;
    }
    E::HeartbeatBenchRequest heartbeatRequest(unsigned node, uint32_t id = 200, uint32_t grant = 20001) const {
        E::HeartbeatBenchRequest r;
        r.id = id;
        r.node = uint8_t(node);
        r.producerTimeRaw = 1000;
        r.firstGrant = grant;
        r.deadlineUs = now + 5000000;
        return r;
    }
    // One accepted (DEC-1) 1000:00 upload through the owner without a grant, as the D line's probe
    // makes on the bench: the adapter records ordinary service evidence for the node from it.
    void qualify(unsigned node) {
        O::SequenceId sequence;
        CHECK(owner.beginSequence(target(node).channel.binding, 1, now, sequence));
        CHECK(owner.beginOperation(sequence, 50 + node, now + 1000000, now));
        O::BusRequest request;
        request.sequence = sequence;
        C::SdoTransfer& t = request.transfer;
        t.channel = target(node).channel;
        t.operation = 50 + node;
        t.request = t.step = 1;
        t.index = 0x1000;
        t.size = 4;
        t.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        t.deadlineUs = now + 1000000;
        t.queueTimeoutUs = t.responseTimeoutUs = 250000;
        O::RequestId id;
        CHECK(owner.admit(request, now, id) == O::BusAdmission::ACCEPTED);
        for (unsigned i = 0; i < 100 && !owner.result(id); ++i)
            turn();
        CHECK(owner.result(id) && owner.result(id)->service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(owner.release(id));
        for (unsigned i = 0; i < 100 && !owner.localTxRetired(uint8_t(node - 1)); ++i)
            turn();
        CHECK(owner.releaseSequence(sequence));
    }
    void heartbeatFrame(unsigned node, uint8_t state) {
        M::Frame f = OwnerRig::frame(0x700 + node, 1);
        f.data[0] = state;
        wire.push(f, now);
    }
    template <class T> unsigned lines(const T& source, char (*out)[E::COMMUNICATION_LINE], unsigned capacity) {
        unsigned n = 0;
        while (n < capacity && source.formatLine(n, out[n], E::COMMUNICATION_LINE)) {
            CHECK(std::strstr(out[n], "OUTPUT_FORMAT_ERROR") == nullptr);
            CHECK(out[n][std::strlen(out[n]) - 1] == '\n');
            ++n;
        }
        return n;
    }
};
// A refusal word, null-safe: an admitted request (null) is never a refusal.
bool refused(const char* got, const char* word) {
    return got && std::strcmp(got, word) == 0;
}
bool starts(const char* line, const char* prefix) {
    return std::strncmp(line, prefix, std::strlen(prefix)) == 0;
}
uint16_t frameIndex(const M::Frame& f) {
    return uint16_t(f.data[1] | f.data[2] << 8);
}

// ------------------------------------------------------------------- K
void readsAllGroups() {
    Rig rig;
    CHECK(rig.reads.begin(rig.readRequest(1), rig.now) == nullptr);
    CHECK(!rig.wire.sentCount); // Admission alone sends nothing.
    rig.finish(rig.reads);
    const E::CommunicationBenchRecord& r = *rig.reads.record();
    CHECK(r.failure == E::CommunicationBenchFailure::NONE && r.outcome == M::ReadOutcome::SUCCESS);
    CHECK(r.published == 18 && r.group == 18 && r.terminalCount == 95);
    // The exact frames: 95 expedited uploads on node 1's route in the fixed group order, no write.
    CHECK(rig.wire.sentCount == 95 && !rig.wire.downloads[1] && !rig.wire.frames[2]);
    const uint16_t order[18] = {0x1005, 0x2203, 0x1800, 0x1801, 0x1802, 0x1803, 0x1a00, 0x1a01, 0x1a02,
                                0x1a03, 0x1400, 0x1401, 0x1402, 0x1403, 0x1600, 0x1601, 0x1602, 0x1603};
    unsigned frame = 6 + 1; // After NETWORK's six and the node setting.
    CHECK(frameIndex(rig.wire.sent[0]) == 0x1005 && frameIndex(rig.wire.sent[2]) == 0x1017 &&
          frameIndex(rig.wire.sent[5]) == 0x1200 && rig.wire.sent[5].data[3] == 2);
    CHECK(frameIndex(rig.wire.sent[6]) == 0x2203);
    for (unsigned g = 2; g < 18; ++g) {
        const bool communication = (order[g] & 0x0200) == 0;
        const unsigned children = communication ? 6 : 5;
        for (unsigned sub = 0; sub < children; ++sub, ++frame) {
            CHECK(rig.wire.sent[frame].id == 0x601 && rig.wire.sent[frame].data[0] == 0x40);
            CHECK(frameIndex(rig.wire.sent[frame]) == order[g] && rig.wire.sent[frame].data[3] == sub);
        }
    }
    CHECK(frame == 95);
    // Q20: every unqualified upload had its own one-use grant; nothing is left armed.
    CHECK(r.grants >= 89 && r.grants <= 95);
    char out[128][E::COMMUNICATION_LINE];
    const unsigned n = rig.lines(rig.reads, out, 128);
    CHECK(n == 1 + 18 + 95 + 1);
    CHECK(starts(out[0], "COMM id=100 node=1 failure=0 groups=18 published=18 group=18 outcome=1 "
                         "uncertain=0 status=0 detail=0 terminals=95"));
    CHECK(starts(out[1], "COMM_NETWORK node=1 sync=00000080 emcy=00000081 producer=0 sdo=2 "
                         "cs=00000601 sc=00000581 lo="));
    CHECK(starts(out[2], "COMM_NODE node=1 raw=0 lo="));
    CHECK(starts(out[3], "COMM_PDO node=1 dir=t pdo=1 entries=5 cob=00000181 type=255 inhibit=100 "
                         "compat=0 event=0 lo="));
    CHECK(starts(out[4], "COMM_PDO node=1 dir=t pdo=2 entries=5 cob=80000281 "));
    CHECK(starts(out[7], "COMM_MAP node=1 dir=t pdo=1 count=3 w1=60410010 w2=60610008 w3=60640020 "
                         "w4=00000000 lo="));
    CHECK(starts(out[8], "COMM_MAP node=1 dir=t pdo=2 count=0 w1=00000000 "));
    CHECK(starts(out[11], "COMM_PDO node=1 dir=r pdo=1 entries=5 cob=00000201 type=255 inhibit=0 "));
    CHECK(starts(out[18], "COMM_MAP node=1 dir=r pdo=4 count=0 "));
    for (unsigned i = 19; i < 19 + 95; ++i)
        CHECK(starts(out[i], "RESULT ") && std::strstr(out[i], " dir=0 outcome=0 "));
    CHECK(starts(out[114], "END comm terminals=95 groups=18;"));
    // Released after the last TX retired; the binding is free for the next record.
    CHECK(rig.reads.release() && !rig.reads.record());
    CHECK(rig.owner.resources().queued == 0);
    // Node 2 reads on its own route only (a fresh rig: the fake wire's frame log is bounded).
    Rig other;
    CHECK(other.reads.begin(other.readRequest(2), other.now) == nullptr);
    other.finish(other.reads);
    CHECK(other.reads.record()->published == 18 && other.wire.frames[2] == 95 && !other.wire.frames[1]);
}

void readRefusals() {
    Rig rig;
    E::CommunicationBenchRequest bad = rig.readRequest(3);
    CHECK(refused(rig.reads.begin(bad, rig.now), "invalid"));
    bad = rig.readRequest(1, 0);
    CHECK(refused(rig.reads.begin(bad, rig.now), "invalid"));
    bad = rig.readRequest(1);
    bad.firstGrant = 0;
    CHECK(refused(rig.reads.begin(bad, rig.now), "invalid"));
    bad = rig.readRequest(1);
    bad.deadlineUs = rig.now;
    CHECK(refused(rig.reads.begin(bad, rig.now), "invalid"));
    Rig one(1);
    CHECK(refused(one.reads.begin(one.readRequest(2), one.now), "route"));
    // While one record is held the binding is reserved: a second record on it is busy.
    rig.qualify(1);
    CHECK(rig.reads.begin(rig.readRequest(1), rig.now) == nullptr);
    CHECK(refused(rig.reads.begin(rig.readRequest(1, 300), rig.now), "busy"));
    CHECK(refused(rig.commission.begin(rig.heartbeatRequest(1), rig.now), "busy"));
    CHECK(!rig.reads.release());
    CHECK(rig.wire.sentCount == 1); // Only the qualifying probe; the refusals sent nothing.
}

// A native abort in the middle: finite, no retry, earlier groups published, the terminal held.
void readAbortMidSequence() {
    Rig rig;
    rig.wire.fault = Wire::Fault::ABORT;
    rig.wire.failNode = 1;
    rig.wire.failAt = 7 + 24 + 20 + 6 + 3; // The third upload of RPDO2 communication, :02.
    CHECK(rig.reads.begin(rig.readRequest(1), rig.now) == nullptr);
    rig.finish(rig.reads);
    const E::CommunicationBenchRecord& r = *rig.reads.record();
    CHECK(r.failure == E::CommunicationBenchFailure::SERVICE &&
          r.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
    CHECK(r.published == 11 && r.group == 11 && r.terminalCount == rig.wire.failAt);
    CHECK(rig.wire.sentCount == rig.wire.failAt); // Nothing after the failed field, no retry.
    for (unsigned i = 0; i < 200; ++i)
        rig.turn();
    CHECK(rig.wire.sentCount == rig.wire.failAt);
    char out[128][E::COMMUNICATION_LINE];
    const unsigned n = rig.lines(rig.reads, out, 128);
    CHECK(n == 1 + 11 + rig.wire.failAt + 1);
    // SERVICE, PROTOCOL_REJECTED; an aborted unqualified upload keeps its possible effects.
    CHECK(starts(out[0], "COMM id=100 node=1 failure=3 groups=18 published=11 group=11 outcome=3 "
                         "uncertain=1 "));
    CHECK(std::strstr(out[11 + rig.wire.failAt], " object=1401:02 dir=0 outcome=1 ") &&
          std::strstr(out[11 + rig.wire.failAt], " abort=06090011 "));
    CHECK(rig.reads.release());
}

// A lost reply: the owner's own timeout ends it, with possible effects kept and the route left
// unresolved; nothing more is sent and the record still releases after TX retirement.
void readLostReplyKeepsUncertainty() {
    Rig rig;
    rig.wire.fault = Wire::Fault::SILENT;
    rig.wire.failNode = 1;
    rig.wire.failAt = 4;
    CHECK(rig.reads.begin(rig.readRequest(1), rig.now) == nullptr);
    rig.finish(rig.reads, 40000);
    const E::CommunicationBenchRecord& r = *rig.reads.record();
    CHECK(r.failure == E::CommunicationBenchFailure::SERVICE && r.uncertain);
    CHECK(r.outcome == M::ReadOutcome::SERVICE_TIMEOUT && r.published == 0 && r.terminalCount == 4);
    CHECK(rig.wire.sentCount == 4 && rig.services.unresolved(0));
    CHECK(rig.reads.release());
    CHECK(refused(rig.reads.begin(rig.readRequest(1, 300), rig.now), "unresolved"));
}

// A boot-up during the reads invalidates the node epoch: the record ends without another upload.
void readRebootInvalidates() {
    Rig rig;
    rig.wire.fault = Wire::Fault::BOOTUP;
    rig.wire.failNode = 1;
    rig.wire.failAt = 10;
    CHECK(rig.reads.begin(rig.readRequest(1), rig.now) == nullptr);
    rig.finish(rig.reads, 40000);
    const E::CommunicationBenchRecord& r = *rig.reads.record();
    CHECK(r.failure == E::CommunicationBenchFailure::INVALIDATED ||
          r.failure == E::CommunicationBenchFailure::SERVICE);
    CHECK(r.published == 2 && rig.wire.sentCount == 10);
}

// A one-use grant that cannot be issued (a later grant already used on the slot) sends nothing.
void readGrantRefused() {
    Rig rig;
    CHECK(rig.reads.begin(rig.readRequest(1, 100, 5), rig.now) == nullptr);
    C::SdoTransfer earlier;
    earlier.channel = target(1).channel;
    earlier.operation = earlier.request = earlier.step = 99;
    earlier.index = 0x1000;
    earlier.size = 4;
    earlier.deadlineUs = rig.now + 1000000;
    earlier.queueTimeoutUs = earlier.responseTimeoutUs = 1000;
    // A grant numbered above the record's own on this slot: the record's first grant is then refused.
    CHECK(rig.services.authorizeUploadExperiment(0, earlier, 1, 50, rig.now));
    rig.services.clearUploadExperiment(0);
    rig.finish(rig.reads);
    const E::CommunicationBenchRecord& r = *rig.reads.record();
    CHECK(r.failure == E::CommunicationBenchFailure::AUTHORIZATION && !rig.wire.sentCount);
    CHECK(rig.reads.release());
}

// ------------------------------------------------------------------- H
// Frame 0 of each H test is the accepted probe that qualifies the route (as the D line's probe
// does on the bench); the record's own frames follow it.
void heartbeatCommissions() {
    Rig rig;
    rig.qualify(1);
    CHECK(rig.services.setProducerInterval(0, rig.services.node(0)->route, 0, rig.now));
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    CHECK(rig.wire.sentCount == 1);
    rig.finish(rig.commission);
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::NONE && r.published && r.prepared && r.migrated);
    CHECK(r.context.state == L::CommunicationState::SUCCEEDED && r.context.writeAcknowledged &&
          r.context.readbackMatched && r.context.readbackRaw == 1000 && !r.context.uncertain);
    CHECK(r.result.producerTimeRaw == 1000 && r.result.scope == L::CommunicationScope::ISOLATED_EXPERIMENT);
    CHECK(r.result.restart == L::CommunicationRequirement::UNKNOWN &&
          r.result.persistence == L::CommunicationRequirement::UNKNOWN);
    // The exact frames: baseline upload, the one download, the readback, in that order.
    CHECK(rig.wire.sentCount == 4 && rig.wire.downloads[1] == 1 && rig.wire.stored[1] == 1000);
    const uint8_t baseline[8] = {0x40, 0x17, 0x10, 0, 0, 0, 0, 0};
    const uint8_t write[8] = {0x2b, 0x17, 0x10, 0, 0xe8, 0x03, 0, 0};
    CHECK(!std::memcmp(rig.wire.sent[1].data, baseline, 8) && !std::memcmp(rig.wire.sent[2].data, write, 8) &&
          !std::memcmp(rig.wire.sent[3].data, baseline, 8));
    CHECK(rig.wire.guardCalls == 1); // Only the download crossed the dispatcher.
    // Next configuration epoch on the owner's route, node epoch unchanged; producer stays withdrawn.
    CHECK(r.current.configurationGeneration == 2 &&
          rig.services.channel(0)->binding.configurationGeneration == 2);
    CHECK(!rig.services.node(0)->producerKnown && rig.services.node(0)->route.generation == 1);
    char out[96][E::COMMUNICATION_LINE];
    const unsigned n = rig.lines(rig.commission, out, 96);
    CHECK(n == 2u + 3u + r.trafficCount + r.tailCount + 1u);
    CHECK(starts(out[0], "HB id=200 node=1 failure=0 lost=0 state=4 outcome=1 write=1 ack=1 effects=1 "
                         "uncertain=0 known=1 readback=1000 matched=1 published=1 raw=1000 terminals=3 "));
    CHECK(starts(out[1], "HB_PREP node=1 nmt=0 nmtState=0 prepared=1 migrated=1 cfg=2 scope=1 restart=0 "
                         "persistence=0 status=0 detail=0"));
    CHECK(std::strstr(out[2], " object=1017:00 dir=0 outcome=0 ") && std::strstr(out[2], " data=0000"));
    CHECK(std::strstr(out[3], " object=1017:00 dir=1 outcome=0 "));
    CHECK(std::strstr(out[4], " object=1017:00 dir=0 outcome=0 ") && std::strstr(out[4], " data=e803"));
    unsigned downloads = 0;
    for (unsigned i = 5; i < n - 1; ++i)
        if (std::strstr(out[i], "TRAFFIC k=1 ") && std::strstr(out[i], " id=601 ") &&
            !std::strstr(out[i], " data=40"))
            downloads += std::strstr(out[i], " data=2b171000e8030000 ") ? 1 : 100;
    CHECK(downloads == 1 && starts(out[n - 1], "END heartbeat terminals=3 "));
    CHECK(rig.commission.release() && !rig.commission.record());
    // The same node again reads raw 1000 and refuses before any write: there is no other value.
    CHECK(rig.commission.begin(rig.heartbeatRequest(1, 300, 30001), rig.now) == nullptr);
    rig.finish(rig.commission);
    CHECK(rig.commission.record()->failure == E::HeartbeatBenchFailure::BASELINE);
    CHECK(rig.wire.sentCount == 5 && rig.wire.downloads[1] == 1);
}

// The bench premise route is unconfirmed: without ordinary service evidence the owner could not
// migrate after the write, so the record refuses before anything is sent. Granted reads (K) give
// none; an accepted upload does.
void heartbeatRequiresServiceEvidence() {
    Rig rig;
    CHECK(refused(rig.commission.begin(rig.heartbeatRequest(1), rig.now), "service"));
    CHECK(rig.reads.begin(rig.readRequest(1), rig.now) == nullptr);
    rig.finish(rig.reads);
    CHECK(rig.reads.record()->failure == E::CommunicationBenchFailure::NONE && rig.reads.release());
    CHECK(rig.wire.sentCount == 95 && !rig.wire.downloads[1]);
    CHECK(refused(rig.commission.begin(rig.heartbeatRequest(1), rig.now), "service"));
    CHECK(rig.wire.sentCount == 95 && !rig.commission.record());
    // Known current Stopped is no ordinary service either, even after an accepted upload.
    Rig stopped;
    stopped.qualify(1);
    stopped.heartbeatFrame(1, 0x04);
    stopped.turn();
    CHECK(refused(stopped.commission.begin(stopped.heartbeatRequest(1), stopped.now), "service"));
    CHECK(stopped.wire.sentCount == 1);
}

// A baseline that is not raw 0 refuses before any write, with dependent facts untouched.
void heartbeatBaselineMismatch() {
    Rig rig;
    rig.qualify(2);
    rig.wire.stored[2] = 5;
    CHECK(rig.services.setProducerInterval(1, rig.services.node(1)->route, 7, rig.now));
    CHECK(rig.commission.begin(rig.heartbeatRequest(2), rig.now) == nullptr);
    rig.finish(rig.commission);
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::BASELINE && !r.prepared && r.terminalCount == 1);
    CHECK(r.status.code == M::Err::INVALID_CONFIG && r.status.detail == int32_t(M::ReadError::VALUE));
    CHECK(rig.wire.sentCount == 2 && rig.wire.sent[1].data[0] == 0x40 && !rig.wire.downloads[2]);
    CHECK(!rig.wire.guardCalls && !r.context.effectsPossible);
    CHECK(rig.services.node(1)->producerKnown && rig.services.node(1)->producerTimeMs == 7);
    CHECK(rig.services.channel(1)->binding.configurationGeneration == 1);
    char out[96][E::COMMUNICATION_LINE];
    rig.lines(rig.commission, out, 96);
    CHECK(starts(out[0], "HB id=200 node=2 failure=9 lost=0 state=0 "));
    CHECK(starts(out[1], "HB_PREP node=2 nmt=0 nmtState=0 prepared=0 migrated=0 cfg=1 ") &&
          std::strstr(out[1], " status=1 detail=6"));
    CHECK(rig.commission.release());
}

// A known current Operational state appears after admission: refused before the baseline.
void heartbeatKnownIneligibleState() {
    Rig rig;
    rig.qualify(1);
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    rig.heartbeatFrame(1, 0x05);
    rig.finish(rig.commission);
    CHECK(rig.commission.record()->failure == E::HeartbeatBenchFailure::NMT);
    CHECK(rig.commission.record()->nmt.known && rig.wire.sentCount == 1);
}

// The write's reply is lost: the owner's timeout ends it, write effects stay uncertain, no readback
// and no second write; the route is left unresolved and the application epoch stays advanced.
void heartbeatLostWriteKeepsUncertainty() {
    Rig rig;
    rig.qualify(1);
    rig.wire.silentDownload = true;
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    rig.finish(rig.commission, 40000);
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::SERVICE && r.prepared && !r.migrated);
    CHECK(r.context.state == L::CommunicationState::FAILED && r.context.uncertain &&
          r.context.effectsPossible && !r.context.writeAcknowledged && r.context.writeEvidencePresent);
    CHECK(rig.wire.sentCount == 3 && rig.wire.downloads[1] == 1);
    for (unsigned i = 0; i < 200; ++i)
        rig.turn();
    CHECK(rig.wire.sentCount == 3 && rig.services.unresolved(0));
    CHECK(!rig.services.node(0)->producerKnown && r.current.configurationGeneration == 2);
    CHECK(rig.commission.release());
}

// Acknowledged write, but the stored value reads back differently: READBACK, no rollback or retry.
void heartbeatReadbackMismatch() {
    Rig rig;
    rig.qualify(1);
    rig.wire.storeOverride = 500;
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    rig.finish(rig.commission);
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::READBACK && r.context.writeAcknowledged);
    CHECK(r.context.readbackKnown && r.context.readbackRaw == 500 && !r.context.readbackMatched &&
          !r.published && r.context.state == L::CommunicationState::FAILED);
    CHECK(rig.wire.sentCount == 4 && rig.wire.downloads[1] == 1);
}

// The node is seen Operational when the write's ACK is consumed: the confirmation is refused,
// the acknowledged write stays recorded and uncertain, and no readback is sent.
void heartbeatOperationalAfterWrite() {
    Rig rig;
    rig.qualify(1);
    rig.wire.stateWithAck = 0x05;
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    rig.finish(rig.commission);
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::CONFIGURATION && !r.published && r.migrated);
    CHECK(r.context.writeAcknowledged && r.context.state == L::CommunicationState::FAILED &&
          r.context.uncertain && !r.context.readbackKnown);
    CHECK(rig.wire.sentCount == 3);
}

// A fresh Pre-operational heartbeat with the ACK: the reviewed known-state confirmation path.
void heartbeatPreOperationalAfterWrite() {
    Rig rig;
    rig.qualify(1);
    rig.wire.stateWithAck = 0x7f;
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    rig.finish(rig.commission);
    CHECK(rig.commission.record()->failure == E::HeartbeatBenchFailure::NONE);
    CHECK(rig.wire.sentCount == 4);
}

void heartbeatRefusalsAndPreflight() {
    Rig rig;
    rig.qualify(1);
    E::HeartbeatBenchRequest bad = rig.heartbeatRequest(1);
    bad.producerTimeRaw = 0; // The reverse transition is not reviewed.
    CHECK(refused(rig.commission.begin(bad, rig.now), "value"));
    bad.producerTimeRaw = 500;
    CHECK(refused(rig.commission.begin(bad, rig.now), "value"));
    bad = rig.heartbeatRequest(3);
    CHECK(refused(rig.commission.begin(bad, rig.now), "invalid"));
    bad = rig.heartbeatRequest(1, 0);
    CHECK(refused(rig.commission.begin(bad, rig.now), "invalid"));
    Rig one(1);
    CHECK(refused(one.commission.begin(one.heartbeatRequest(2), one.now), "route"));
    CHECK(rig.wire.sentCount == 1);
    // Nothing held: the dispatcher refuses any download.
    C::SdoTransfer download;
    download.channel = target(1).channel;
    download.operation = download.request = download.step = 1;
    download.index = 0x1017;
    download.size = 2;
    download.direction = C::SdoDirection::DOWNLOAD;
    download.data[0] = 0xe8;
    download.data[1] = 0x03;
    O::SequenceId none;
    CHECK(!rig.commission.preflight(none, download, rig.now));
    // While the baseline upload is pending no download is matched either.
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    rig.turn();
    CHECK(!rig.commission.preflight(none, download, rig.now));
}

// Without the dispatcher's match, the owner closes the download provably unsent.
void heartbeatWithoutDispatcherIsUnsent() {
    Rig rig;
    rig.qualify(1);
    rig.wire.heartbeat = nullptr; // A dispatcher that does not know the H record.
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    rig.finish(rig.commission);
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::SERVICE && r.terminalCount == 2);
    CHECK(rig.wire.sentCount == 2 && !rig.wire.downloads[1] && rig.wire.guardCalls == 1);
    CHECK(!r.terminals[1].possible && r.context.state == L::CommunicationState::FAILED);
}
} // namespace
int main() {
    readsAllGroups();
    readRefusals();
    readAbortMidSequence();
    readLostReplyKeepsUncertainty();
    readRebootInvalidates();
    readGrantRefused();
    heartbeatCommissions();
    heartbeatRequiresServiceEvidence();
    heartbeatBaselineMismatch();
    heartbeatKnownIneligibleState();
    heartbeatLostWriteKeepsUncertainty();
    heartbeatReadbackMismatch();
    heartbeatOperationalAfterWrite();
    heartbeatPreOperationalAfterWrite();
    heartbeatRefusalsAndPreflight();
    heartbeatWithoutDispatcherIsUnsent();
    std::printf("communication bench: %u checks passed\n", checks);
}
