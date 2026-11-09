// SPDX-License-Identifier: MIT
// The S2 bench's K and H lines through the real owner and service store over a synthetic two-node
// communication dictionary. Widths are transcribed from the EDS independently of the profile.
// The firmware turn is an ordinary owner service, or with firmwareTurn the firmware's own ProbeApp
// turn, which the D line's DiscoveryBench needs; the transport's handoff guard plays main.cpp's
// download dispatcher, in which only the H record takes part.
#include "CommunicationBench.h"
#include "DiscoveryBench.h"
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
// One upload reply of a retained bench capture as its RESULT line prints it: object=, size= and the
// little-endian data= bytes as a raw value.
struct Captured {
    uint16_t index;
    uint8_t sub;
    uint8_t size;
    uint32_t raw;
};
struct Wire : OwnerRig::WireBase {
    uint64_t now = 0;
    E::HeartbeatCommission* heartbeat = nullptr;
    uint16_t stored[3] = {0, 0, 0}; // 1017:00 per node.
    uint16_t storeOverride = 0;     // Nonzero: a download stores this instead (readback mismatch).
    // While set, each upload to that node must be the table's next object, in the captured order,
    // and takes its captured reply. 1017:00 keeps the fake's stored value, which a download changes.
    const Captured* script[3] = {};
    unsigned scriptLength[3] = {}, scriptAt[3] = {};
    const Captured* next(unsigned node, uint16_t index, uint8_t sub) {
        if (!script[node])
            return nullptr;
        CHECK(scriptAt[node] < scriptLength[node]);
        const Captured& c = script[node][scriptAt[node]++];
        CHECK(c.index == index && c.sub == sub);
        return &c;
    }
    // A received frame with the S2 driver's receive bounds, as a capture's PDOW_F or EVENT line
    // prints them (lo, hi).
    void pushBounded(const M::Frame& value, uint64_t lo, uint64_t hi) {
        CHECK(rxWrite < CAPACITY && lo <= hi);
        O::ReceivedFrame& input = incoming[rxWrite++];
        input = O::ReceivedFrame();
        input.frame = value;
        input.receiveBoundsKnown = true;
        input.receivedEarliestUs = lo;
        input.receivedLatestUs = hi;
        input.controllerEpoch = 1;
    }
    // The base queues never wrap. With nothing queued or in flight, a replay of a whole MCU boot
    // starts them again; the frame log then begins at the next record. The base push sets only
    // the exact time, so every reused slot starts from a default frame.
    void compact() {
        CHECK(rxRead == rxWrite && reportRead == reportWrite);
        for (unsigned i = 0; i < sentCount; ++i)
            CHECK(!pending[i]);
        for (unsigned i = 0; i < CAPACITY; ++i)
            incoming[i] = O::ReceivedFrame();
        rxRead = rxWrite = reportRead = reportWrite = sentCount = 0;
    }
    // Failure injection on the n-th request frame to `failNode` (1-based), or on its download.
    enum class Fault { NONE, ABORT, SILENT, BOOTUP } fault = Fault::NONE;
    unsigned failNode = 0, failAt = 0, frames[3] = {}, downloads[3] = {}, guardCalls = 0, nmtFrames = 0;
    bool silentDownload = false;
    uint8_t stateWithAck = 0; // Nonzero: a heartbeat in this state arrives just before the write's ACK.
    static bool guard(void* context, O::SequenceId sequence, const C::SdoTransfer& t, uint64_t now) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        ++w.guardCalls;
        return w.heartbeat && w.heartbeat->preflight(sequence, t, now);
    }
    static bool transmit(void* context, const M::Frame& f, O::TransportTxId id, uint32_t epoch) {
        Wire& w = *static_cast<Wire*>(static_cast<OwnerRig::WireBase*>(context));
        if (f.id == 0) {
            // An addressed NMT command (the N line). A node with producer time 0 sends no heartbeat,
            // so nothing on this wire observes its transition.
            CHECK(f.payloadLength == 2 && (f.data[1] == 1 || f.data[1] == 2));
            CHECK(OwnerRig::WireBase::transmit(context, f, id, epoch));
            ++w.nmtFrames;
            return true;
        }
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
        unsigned size = width(index, sub);
        uint32_t raw = value(node, index, sub, w.stored[node]);
        if (const Captured* c = w.next(node, index, sub)) {
            size = c->size;
            if (index != 0x1017)
                raw = c->raw;
        }
        if (!size || (w.fault == Fault::ABORT && node == w.failNode && w.frames[node] == w.failAt)) {
            reply.data[0] = 0x80;
            const uint32_t abort = 0x06090011; // Subindex does not exist.
            for (unsigned i = 0; i < 4; ++i)
                reply.data[4 + i] = uint8_t(abort >> (8 * i));
            w.push(reply, w.now, epoch);
            return true;
        }
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
    E::ProbePlatform platform;
    E::ProbeApp app;
    E::DiscoveryBench discovery;
    uint64_t now = 10;
    bool firmwareTurn = false; // True: ProbeApp::service is the turn, as FirmwareProbe runs it.
    explicit Rig(unsigned routes = 2)
        : owner(services, wire.callbacks(), 28), reads(owner, services), commission(owner, services),
          platform(E::ProbeCallbacks()), app(owner, platform), discovery(app, owner, services) {
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
        if (firmwareTurn)
            CHECK(app.service(now));
        else
            CHECK(owner.service(now, OwnerRig::budget()));
        // main.cpp's order after the firmware turn: D, K, then H.
        CHECK(discovery.service(owner.lastTimeUs()));
        CHECK(reads.service(owner.lastTimeUs()));
        CHECK(commission.service(owner.lastTimeUs()));
    }
    // Firmware turns of `step` until `t`, the last one landing on it: a capture's own instants.
    void until(uint64_t t, uint64_t step = 1000) {
        CHECK(t >= now);
        while (t - now > step)
            turn(step);
        if (t > now)
            turn(t - now);
    }
    // A 7F heartbeat of `node` with a capture's receive bounds, processed by the next turn.
    void beat(unsigned node, uint64_t lo, uint64_t hi) {
        until(hi);
        M::Frame f = OwnerRig::frame(0x700 + node, 1);
        f.data[0] = 0x7f;
        wire.pushBounded(f, lo, hi);
        turn();
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
    // The N line (main.cpp admitNmt): one addressed command with the bench's 250 ms queue budget and
    // 5 s wait window, its TX report, then r releases the result. Returns the admission time.
    uint64_t nmt(unsigned node, C::NmtCommand command, uint32_t id) {
        C::NmtTransfer work;
        work.target = services.node(uint8_t(node - 1))->route;
        work.command = command;
        work.bus = work.target.binding.endpoint.bus;
        work.request = id;
        work.queueTimeoutUs = 250000;
        work.deadlineUs = now + 5000000;
        C::NmtRequestId request;
        const uint64_t admitted = now;
        CHECK(owner.admitNmt(work, now, request));
        for (unsigned i = 0; i < 100 && !owner.nmtResult(request); ++i)
            turn();
        CHECK(owner.nmtResult(request) && owner.nmtResult(request)->transmissionConfirmed);
        CHECK(owner.releaseNmt(request));
        return admitted;
    }
    // The D line in probe mode over both nodes, selecting `select`, then r.
    void probe(uint8_t select, uint32_t id) {
        E::DiscoveryBenchRequest r;
        r.id = id;
        r.mode = E::DiscoveryMode::PROBE;
        r.seconds = 3;
        r.select = select;
        r.bitrate = 1000000;
        CHECK(discovery.begin(r, now) == nullptr);
        for (unsigned i = 0; i < 40000 && discovery.active(); ++i)
            turn();
        CHECK(discovery.record() && discovery.record()->terminal);
        CHECK(discovery.scan().result(0) && discovery.scan().result(0)->bindingCurrent);
        CHECK(discovery.release());
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
    // While one record is held it is busy, and the other record finds the binding reserved.
    rig.qualify(1);
    CHECK(rig.reads.begin(rig.readRequest(1), rig.now) == nullptr);
    CHECK(refused(rig.reads.begin(rig.readRequest(1, 300), rig.now), "busy"));
    CHECK(refused(rig.commission.begin(rig.heartbeatRequest(1), rig.now), "reserved"));
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
// The 2026-11-09 S2 sequence on both nodes (docs/bench/20261109T1121Z-r2b-p38_heartbeat_node1),
// through the firmware's own ProbeApp turn: N1,p, the D probe, K1, then H1,1000. The probe gave
// ordinary service, which was all the old precondition checked; but the N line's transition stayed
// pending, as no heartbeat at producer time 0 could observe it, and the adapter refuses the epoch
// migration while it is. The write was then applied and acknowledged and could not be confirmed.
// Now H refuses before anything is sent, and once the N line's 5 s window has ended it commissions.
void heartbeatWaitsForPendingTransition() {
    Rig rig;
    rig.firmwareTurn = true;
    const uint64_t nmtAt = rig.nmt(1, C::NmtCommand::PRE_OPERATIONAL, 37);
    rig.probe(1, 38);
    CHECK(rig.reads.begin(rig.readRequest(1, 39), rig.now) == nullptr);
    rig.finish(rig.reads);
    CHECK(rig.reads.record()->failure == E::CommunicationBenchFailure::NONE && rig.reads.release());
    const C::NodeSnapshot& node = *rig.services.node(0);
    C::ServiceEligibility e;
    CHECK(C::serviceEligibility(node, rig.now, e) && e.ordinarySdo && !e.exactNmtKnown);
    CHECK(node.transitionPending && rig.now < node.transitionDeadlineUs);
    CHECK(!rig.services.configurationEligible(0, rig.services.channel(0)->binding, rig.now));
    const unsigned sent = rig.wire.sentCount;
    CHECK(refused(rig.commission.begin(rig.heartbeatRequest(1, 57, 20001), rig.now), "transition"));
    CHECK(!rig.commission.record() && rig.wire.sentCount == sent);
    CHECK(!rig.wire.downloads[1] && !rig.wire.stored[1] && !rig.wire.guardCalls);
    // Nothing can observe the transition here; only the N line's own window ends it.
    while (rig.now < nmtAt + 5000000)
        rig.turn(100000);
    CHECK(!node.transitionPending && node.transitionExpired);
    // Expiry promotes no NMT state: the experiment still runs with the state unknown.
    CHECK(C::serviceEligibility(node, rig.now, e) && e.ordinarySdo && !e.exactNmtKnown);
    CHECK(rig.commission.begin(rig.heartbeatRequest(1, 57, 20001), rig.now) == nullptr);
    rig.finish(rig.commission);
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::NONE && r.prepared && r.migrated && r.published);
    CHECK(!r.nmt.known && r.context.readbackMatched && !r.context.uncertain);
    CHECK(rig.wire.downloads[1] == 1 && rig.wire.stored[1] == 1000 && rig.wire.nmtFrames == 1);
    CHECK(rig.wire.sentCount == sent + 3);
    CHECK(rig.services.channel(0)->binding.configurationGeneration == 2);
    CHECK(rig.commission.release());
}

// Node 2's run with the probe moved last (20261109T1123Z-r2b-hb-node2-probe-last): N2,p, K2, the D
// probe, H2. The order of K and the probe does not matter; the pending transition refuses alike.
// A probe before the N line gives no ordinary service at all: its evidence precedes the intent.
void heartbeatProbeOrderAroundNmt() {
    Rig rig;
    rig.firmwareTurn = true;
    rig.nmt(2, C::NmtCommand::PRE_OPERATIONAL, 77);
    CHECK(rig.reads.begin(rig.readRequest(2, 78), rig.now) == nullptr);
    rig.finish(rig.reads);
    CHECK(rig.reads.release());
    rig.probe(2, 96);
    const unsigned sent = rig.wire.sentCount;
    CHECK(refused(rig.commission.begin(rig.heartbeatRequest(2, 97, 20001), rig.now), "transition"));
    CHECK(rig.wire.sentCount == sent && !rig.wire.downloads[2]);
    Rig early;
    early.firmwareTurn = true;
    early.probe(1, 1);
    early.nmt(1, C::NmtCommand::PRE_OPERATIONAL, 2);
    CHECK(refused(early.commission.begin(early.heartbeatRequest(1, 3, 20001), early.now), "service"));
    // After the window the earlier probe still counts for nothing: it preceded the intent.
    for (unsigned i = 0; i < 60; ++i)
        early.turn(100000);
    CHECK(!early.services.node(0)->transitionPending);
    CHECK(refused(early.commission.begin(early.heartbeatRequest(1, 3, 20001), early.now), "service"));
    early.probe(1, 4);
    CHECK(early.commission.begin(early.heartbeatRequest(1, 5, 20001), early.now) == nullptr);
}

// The premises are checked once more just before the write is admitted. An NMT intent between the
// baseline and the write ends the record with only the baseline upload sent, nothing written.
void heartbeatRechecksBeforeWrite() {
    Rig rig;
    rig.qualify(1);
    CHECK(rig.commission.begin(rig.heartbeatRequest(1), rig.now) == nullptr);
    for (unsigned i = 0; i < 100 && rig.wire.sentCount < 2; ++i)
        rig.turn();
    CHECK(rig.wire.sentCount == 2 && !rig.commission.record()->terminal); // The baseline is out.
    C::NmtTransfer work;
    work.target = rig.services.node(0)->route;
    work.command = C::NmtCommand::PRE_OPERATIONAL;
    work.bus = work.target.binding.endpoint.bus;
    work.request = 90;
    work.queueTimeoutUs = 250000;
    work.deadlineUs = rig.now + 5000000;
    C::NmtRequestId nmt;
    CHECK(rig.owner.admitNmt(work, rig.now, nmt));
    rig.finish(rig.commission);
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::ELIGIBILITY && r.terminalCount == 1);
    CHECK(!r.context.effectsPossible && !r.context.writeAcknowledged && !r.migrated);
    CHECK(!rig.wire.downloads[1] && !rig.wire.guardCalls && rig.wire.nmtFrames == 1);
    CHECK(rig.services.channel(0)->binding.configurationGeneration == 1);
    CHECK(rig.commission.release());
}
// An open route AxisSession keeps its axis's owner reservation (2026-11-09: after routes 901 and
// 902, K and H were refused for the rest of the boot). They now say so, send nothing, and leave
// the other node alone. Closing the idle session releases it and keeps its object-life records.
void routeSessionKeepsReservation() {
    Rig rig;
    rig.firmwareTurn = true;
    rig.qualify(1);
    rig.qualify(2);
    static E::AxisSession session(rig.owner); // Large; as on the bench, stationary storage.
    CHECK(rig.app.openAxis(session, target(1), L::SettingsModel::CL86_C, rig.now) == &session);
    CHECK(session.isOpen() && rig.owner.sequence(session.sequence()));
    const unsigned sent = rig.wire.sentCount;
    CHECK(refused(rig.reads.begin(rig.readRequest(1), rig.now), "reserved"));
    CHECK(refused(rig.commission.begin(rig.heartbeatRequest(1), rig.now), "reserved"));
    CHECK(rig.wire.sentCount == sent && !rig.reads.record() && !rig.commission.record());
    CHECK(rig.commission.begin(rig.heartbeatRequest(2), rig.now) == nullptr);
    rig.finish(rig.commission);
    CHECK(rig.commission.record()->failure == E::HeartbeatBenchFailure::NONE && rig.commission.release());
    CHECK(rig.wire.sentCount == sent + 3 && rig.wire.frames[1] == 1 && rig.wire.downloads[2] == 1);
    CHECK(!session.busy() && !session.motionHistoryWritten() && session.close() && !session.isOpen());
    CHECK(rig.reads.begin(rig.readRequest(1), rig.now) == nullptr);
    rig.finish(rig.reads);
    CHECK(rig.reads.record()->failure == E::CommunicationBenchFailure::NONE && rig.reads.release());
}

// ------------------------------------------------------- 2026-11-09 captures, replayed literally
// A produced line against a retained captured line, token by token. A token whose key is listed in
// `skip` is compared by key only: S2 microseconds (admitted, completed, p, lo, hi, skew, gap,
// hbp), boot-wide serials (oreq, txr, tx0, tx1) and the driver's receive-time quality (exact, t,
// bounds), which this fake's exact time stamps and fresh counters do not reproduce. Every other
// token, every value included, must be identical, and so must the token count.
bool listed(const char* list, const char* key, size_t size) {
    for (const char* at = list; *at;) {
        while (*at == ' ')
            ++at;
        const char* end = at;
        while (*end && *end != ' ')
            ++end;
        if (size_t(end - at) == size && !std::strncmp(at, key, size))
            return true;
        at = end;
    }
    return false;
}
bool sameLine(const char* got, const char* captured, const char* skip) {
    const char* a = got;
    const char* b = captured;
    for (;;) {
        while (*a == ' ')
            ++a;
        while (*b == ' ')
            ++b;
        const bool doneA = !*a || *a == '\n', doneB = !*b || *b == '\n';
        if (doneA || doneB) {
            if (doneA && doneB)
                return true;
            break;
        }
        const char* endA = a;
        while (*endA && *endA != ' ' && *endA != '\n')
            ++endA;
        const char* endB = b;
        while (*endB && *endB != ' ' && *endB != '\n')
            ++endB;
        const char* keyA = a;
        while (keyA < endA && *keyA != '=')
            ++keyA;
        const char* keyB = b;
        while (keyB < endB && *keyB != '=')
            ++keyB;
        const size_t key = size_t(keyA - a);
        if (key != size_t(keyB - b) || std::strncmp(a, b, key))
            break;
        if (!listed(skip, a, key) &&
            (endA - a != endB - b || std::strncmp(a, b, size_t(endA - a))))
            break;
        a = endA;
        b = endB;
    }
    std::fprintf(stderr, "produced: %s\ncaptured: %s\n", got, captured);
    return false;
}
const char* const RESULT_SKIP = "oreq admitted completed";
const char* const TRAFFIC_SKIP = "p exact t bounds lo hi txr";

// Every K upload reply of both nodes, in the K order, from the RESULT lines of
// docs/bench/20261109T1121Z-r2b-p38_reads/rx.txt: node 1 lines 22-116, node 2 lines 139-233
// (image 757f1cd, drives not power-cycled since 2026-11-08). The K reads after the next power-up,
// 20261109T1245Z-r4a-hb-node1 lines 36-130 and 20261109T1246Z-r4a-hb-node2 lines 36-130, returned
// the same 95 values per node, and so did 20261109T1121Z-r2b-p38_heartbeat_node1 lines 36-130.
// Every one of those RESULT lines has dir=0 outcome=0 ack=1 possible=1 uncertain=0 abort=00000000
// cleanup=0 ne=1 reason=0 st=3f.
const Captured K_NODE1[95] = {
    {0x1005, 0x00, 4, 0x00000080}, {0x1014, 0x00, 4, 0x00000081}, {0x1017, 0x00, 2, 0x00000000},
    {0x1200, 0x00, 1, 0x00000002}, {0x1200, 0x01, 4, 0x00000601}, {0x1200, 0x02, 4, 0x00000581},
    {0x2203, 0x00, 2, 0x00000000}, {0x1800, 0x00, 1, 0x00000005}, {0x1800, 0x01, 4, 0x00000181},
    {0x1800, 0x02, 1, 0x000000ff}, {0x1800, 0x03, 2, 0x00000064}, {0x1800, 0x04, 1, 0x00000000},
    {0x1800, 0x05, 2, 0x00000000}, {0x1801, 0x00, 1, 0x00000005}, {0x1801, 0x01, 4, 0x00000281},
    {0x1801, 0x02, 1, 0x000000ff}, {0x1801, 0x03, 2, 0x00000064}, {0x1801, 0x04, 1, 0x00000000},
    {0x1801, 0x05, 2, 0x00000000}, {0x1802, 0x00, 1, 0x00000005}, {0x1802, 0x01, 4, 0x00000381},
    {0x1802, 0x02, 1, 0x000000ff}, {0x1802, 0x03, 2, 0x00000064}, {0x1802, 0x04, 1, 0x00000000},
    {0x1802, 0x05, 2, 0x00000000}, {0x1803, 0x00, 1, 0x00000005}, {0x1803, 0x01, 4, 0x00000481},
    {0x1803, 0x02, 1, 0x000000ff}, {0x1803, 0x03, 2, 0x00000064}, {0x1803, 0x04, 1, 0x00000000},
    {0x1803, 0x05, 2, 0x00000000}, {0x1a00, 0x00, 1, 0x00000003}, {0x1a00, 0x01, 4, 0x60410010},
    {0x1a00, 0x02, 4, 0x60610008}, {0x1a00, 0x03, 4, 0x60640020}, {0x1a00, 0x04, 4, 0x00000000},
    {0x1a01, 0x00, 1, 0x00000000}, {0x1a01, 0x01, 4, 0x00000000}, {0x1a01, 0x02, 4, 0x00000000},
    {0x1a01, 0x03, 4, 0x00000000}, {0x1a01, 0x04, 4, 0x00000000}, {0x1a02, 0x00, 1, 0x00000000},
    {0x1a02, 0x01, 4, 0x00000000}, {0x1a02, 0x02, 4, 0x00000000}, {0x1a02, 0x03, 4, 0x00000000},
    {0x1a02, 0x04, 4, 0x00000000}, {0x1a03, 0x00, 1, 0x00000000}, {0x1a03, 0x01, 4, 0x00000000},
    {0x1a03, 0x02, 4, 0x00000000}, {0x1a03, 0x03, 4, 0x00000000}, {0x1a03, 0x04, 4, 0x00000000},
    {0x1400, 0x00, 1, 0x00000005}, {0x1400, 0x01, 4, 0x00000201}, {0x1400, 0x02, 1, 0x000000ff},
    {0x1400, 0x03, 2, 0x00000000}, {0x1400, 0x04, 1, 0x00000000}, {0x1400, 0x05, 2, 0x000003e8},
    {0x1401, 0x00, 1, 0x00000005}, {0x1401, 0x01, 4, 0x00000301}, {0x1401, 0x02, 1, 0x000000ff},
    {0x1401, 0x03, 2, 0x00000000}, {0x1401, 0x04, 1, 0x00000000}, {0x1401, 0x05, 2, 0x000003e8},
    {0x1402, 0x00, 1, 0x00000005}, {0x1402, 0x01, 4, 0x00000401}, {0x1402, 0x02, 1, 0x000000ff},
    {0x1402, 0x03, 2, 0x00000000}, {0x1402, 0x04, 1, 0x00000000}, {0x1402, 0x05, 2, 0x000003e8},
    {0x1403, 0x00, 1, 0x00000005}, {0x1403, 0x01, 4, 0x00000501}, {0x1403, 0x02, 1, 0x000000ff},
    {0x1403, 0x03, 2, 0x00000000}, {0x1403, 0x04, 1, 0x00000000}, {0x1403, 0x05, 2, 0x000003e8},
    {0x1600, 0x00, 1, 0x00000004}, {0x1600, 0x01, 4, 0x60400010}, {0x1600, 0x02, 4, 0x60600008},
    {0x1600, 0x03, 4, 0x60980008}, {0x1600, 0x04, 4, 0x607a0020}, {0x1601, 0x00, 1, 0x00000000},
    {0x1601, 0x01, 4, 0x00000000}, {0x1601, 0x02, 4, 0x00000000}, {0x1601, 0x03, 4, 0x00000000},
    {0x1601, 0x04, 4, 0x00000000}, {0x1602, 0x00, 1, 0x00000000}, {0x1602, 0x01, 4, 0x00000000},
    {0x1602, 0x02, 4, 0x00000000}, {0x1602, 0x03, 4, 0x00000000}, {0x1602, 0x04, 4, 0x00000000},
    {0x1603, 0x00, 1, 0x00000000}, {0x1603, 0x01, 4, 0x00000000}, {0x1603, 0x02, 4, 0x00000000},
    {0x1603, 0x03, 4, 0x00000000}, {0x1603, 0x04, 4, 0x00000000},
};
const Captured K_NODE2[95] = {
    {0x1005, 0x00, 4, 0x00000080}, {0x1014, 0x00, 4, 0x00000082}, {0x1017, 0x00, 2, 0x00000000},
    {0x1200, 0x00, 1, 0x00000002}, {0x1200, 0x01, 4, 0x00000602}, {0x1200, 0x02, 4, 0x00000582},
    {0x2203, 0x00, 2, 0x00000000}, {0x1800, 0x00, 1, 0x00000005}, {0x1800, 0x01, 4, 0x00000182},
    {0x1800, 0x02, 1, 0x000000ff}, {0x1800, 0x03, 2, 0x00000064}, {0x1800, 0x04, 1, 0x00000000},
    {0x1800, 0x05, 2, 0x00000000}, {0x1801, 0x00, 1, 0x00000005}, {0x1801, 0x01, 4, 0x00000282},
    {0x1801, 0x02, 1, 0x000000ff}, {0x1801, 0x03, 2, 0x00000064}, {0x1801, 0x04, 1, 0x00000000},
    {0x1801, 0x05, 2, 0x00000000}, {0x1802, 0x00, 1, 0x00000005}, {0x1802, 0x01, 4, 0x00000382},
    {0x1802, 0x02, 1, 0x000000ff}, {0x1802, 0x03, 2, 0x00000064}, {0x1802, 0x04, 1, 0x00000000},
    {0x1802, 0x05, 2, 0x00000000}, {0x1803, 0x00, 1, 0x00000005}, {0x1803, 0x01, 4, 0x00000482},
    {0x1803, 0x02, 1, 0x000000ff}, {0x1803, 0x03, 2, 0x00000064}, {0x1803, 0x04, 1, 0x00000000},
    {0x1803, 0x05, 2, 0x00000000}, {0x1a00, 0x00, 1, 0x00000003}, {0x1a00, 0x01, 4, 0x60410010},
    {0x1a00, 0x02, 4, 0x60610008}, {0x1a00, 0x03, 4, 0x60640020}, {0x1a00, 0x04, 4, 0x00000000},
    {0x1a01, 0x00, 1, 0x00000000}, {0x1a01, 0x01, 4, 0x00000000}, {0x1a01, 0x02, 4, 0x00000000},
    {0x1a01, 0x03, 4, 0x00000000}, {0x1a01, 0x04, 4, 0x00000000}, {0x1a02, 0x00, 1, 0x00000000},
    {0x1a02, 0x01, 4, 0x00000000}, {0x1a02, 0x02, 4, 0x00000000}, {0x1a02, 0x03, 4, 0x00000000},
    {0x1a02, 0x04, 4, 0x00000000}, {0x1a03, 0x00, 1, 0x00000000}, {0x1a03, 0x01, 4, 0x00000000},
    {0x1a03, 0x02, 4, 0x00000000}, {0x1a03, 0x03, 4, 0x00000000}, {0x1a03, 0x04, 4, 0x00000000},
    {0x1400, 0x00, 1, 0x00000005}, {0x1400, 0x01, 4, 0x00000202}, {0x1400, 0x02, 1, 0x000000ff},
    {0x1400, 0x03, 2, 0x00000000}, {0x1400, 0x04, 1, 0x00000000}, {0x1400, 0x05, 2, 0x000003e8},
    {0x1401, 0x00, 1, 0x00000005}, {0x1401, 0x01, 4, 0x00000302}, {0x1401, 0x02, 1, 0x000000ff},
    {0x1401, 0x03, 2, 0x00000000}, {0x1401, 0x04, 1, 0x00000000}, {0x1401, 0x05, 2, 0x000003e8},
    {0x1402, 0x00, 1, 0x00000005}, {0x1402, 0x01, 4, 0x00000402}, {0x1402, 0x02, 1, 0x000000ff},
    {0x1402, 0x03, 2, 0x00000000}, {0x1402, 0x04, 1, 0x00000000}, {0x1402, 0x05, 2, 0x000003e8},
    {0x1403, 0x00, 1, 0x00000005}, {0x1403, 0x01, 4, 0x00000502}, {0x1403, 0x02, 1, 0x000000ff},
    {0x1403, 0x03, 2, 0x00000000}, {0x1403, 0x04, 1, 0x00000000}, {0x1403, 0x05, 2, 0x000003e8},
    {0x1600, 0x00, 1, 0x00000004}, {0x1600, 0x01, 4, 0x60400010}, {0x1600, 0x02, 4, 0x60600008},
    {0x1600, 0x03, 4, 0x60980008}, {0x1600, 0x04, 4, 0x607a0020}, {0x1601, 0x00, 1, 0x00000000},
    {0x1601, 0x01, 4, 0x00000000}, {0x1601, 0x02, 4, 0x00000000}, {0x1601, 0x03, 4, 0x00000000},
    {0x1601, 0x04, 4, 0x00000000}, {0x1602, 0x00, 1, 0x00000000}, {0x1602, 0x01, 4, 0x00000000},
    {0x1602, 0x02, 4, 0x00000000}, {0x1602, 0x03, 4, 0x00000000}, {0x1602, 0x04, 4, 0x00000000},
    {0x1603, 0x00, 1, 0x00000000}, {0x1603, 0x01, 4, 0x00000000}, {0x1603, 0x02, 4, 0x00000000},
    {0x1603, 0x03, 4, 0x00000000}, {0x1603, 0x04, 4, 0x00000000},
};
// The typed lines of the same records, lines 4-21 and 121-138.
const char* const K_TYPED_NODE1[18] = {
    // line 4
    "COMM_NETWORK node=1 sync=00000080 emcy=00000081 producer=0 sdo=2 cs=00000601 sc=00000581 "
    "lo=21369983 hi=21402979 skew=32996",
    // line 5
    "COMM_NODE node=1 raw=0 lo=21406983 hi=21409981 skew=2998",
    // line 6
    "COMM_PDO node=1 dir=t pdo=1 entries=5 cob=00000181 type=255 inhibit=100 compat=0 event=0 "
    "lo=21413983 hi=21446979 skew=32996",
    // line 7
    "COMM_PDO node=1 dir=t pdo=2 entries=5 cob=00000281 type=255 inhibit=100 compat=0 event=0 "
    "lo=21450983 hi=21483979 skew=32996",
    // line 8
    "COMM_PDO node=1 dir=t pdo=3 entries=5 cob=00000381 type=255 inhibit=100 compat=0 event=0 "
    "lo=21487983 hi=21520979 skew=32996",
    // line 9
    "COMM_PDO node=1 dir=t pdo=4 entries=5 cob=00000481 type=255 inhibit=100 compat=0 event=0 "
    "lo=21524983 hi=21557979 skew=32996",
    // line 10
    "COMM_MAP node=1 dir=t pdo=1 count=3 w1=60410010 w2=60610008 w3=60640020 w4=00000000 "
    "lo=21561983 hi=21588979 skew=26996",
    // line 11
    "COMM_MAP node=1 dir=t pdo=2 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=21592983 hi=21619979 skew=26996",
    // line 12
    "COMM_MAP node=1 dir=t pdo=3 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=21623983 hi=21650979 skew=26996",
    // line 13
    "COMM_MAP node=1 dir=t pdo=4 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=21654983 hi=21681979 skew=26996",
    // line 14
    "COMM_PDO node=1 dir=r pdo=1 entries=5 cob=00000201 type=255 inhibit=0 compat=0 event=1000 "
    "lo=21685983 hi=21718979 skew=32996",
    // line 15
    "COMM_PDO node=1 dir=r pdo=2 entries=5 cob=00000301 type=255 inhibit=0 compat=0 event=1000 "
    "lo=21721984 hi=21754979 skew=32995",
    // line 16
    "COMM_PDO node=1 dir=r pdo=3 entries=5 cob=00000401 type=255 inhibit=0 compat=0 event=1000 "
    "lo=21757982 hi=21790979 skew=32997",
    // line 17
    "COMM_PDO node=1 dir=r pdo=4 entries=5 cob=00000501 type=255 inhibit=0 compat=0 event=1000 "
    "lo=21794983 hi=21827979 skew=32996",
    // line 18
    "COMM_MAP node=1 dir=r pdo=1 count=4 w1=60400010 w2=60600008 w3=60980008 w4=607a0020 "
    "lo=21831983 hi=21858979 skew=26996",
    // line 19
    "COMM_MAP node=1 dir=r pdo=2 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=21862983 hi=21889979 skew=26996",
    // line 20
    "COMM_MAP node=1 dir=r pdo=3 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=21893983 hi=21920979 skew=26996",
    // line 21
    "COMM_MAP node=1 dir=r pdo=4 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=21924983 hi=21951979 skew=26996",
};
const char* const K_TYPED_NODE2[18] = {
    // line 121
    "COMM_NETWORK node=2 sync=00000080 emcy=00000082 producer=0 sdo=2 cs=00000602 sc=00000582 "
    "lo=24589983 hi=24622979 skew=32996",
    // line 122
    "COMM_NODE node=2 raw=0 lo=24626983 hi=24629979 skew=2996",
    // line 123
    "COMM_PDO node=2 dir=t pdo=1 entries=5 cob=00000182 type=255 inhibit=100 compat=0 event=0 "
    "lo=24633983 hi=24666979 skew=32996",
    // line 124
    "COMM_PDO node=2 dir=t pdo=2 entries=5 cob=00000282 type=255 inhibit=100 compat=0 event=0 "
    "lo=24670983 hi=24703979 skew=32996",
    // line 125
    "COMM_PDO node=2 dir=t pdo=3 entries=5 cob=00000382 type=255 inhibit=100 compat=0 event=0 "
    "lo=24707983 hi=24740979 skew=32996",
    // line 126
    "COMM_PDO node=2 dir=t pdo=4 entries=5 cob=00000482 type=255 inhibit=100 compat=0 event=0 "
    "lo=24744983 hi=24777979 skew=32996",
    // line 127
    "COMM_MAP node=2 dir=t pdo=1 count=3 w1=60410010 w2=60610008 w3=60640020 w4=00000000 "
    "lo=24781983 hi=24808979 skew=26996",
    // line 128
    "COMM_MAP node=2 dir=t pdo=2 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=24812983 hi=24839979 skew=26996",
    // line 129
    "COMM_MAP node=2 dir=t pdo=3 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=24843983 hi=24870979 skew=26996",
    // line 130
    "COMM_MAP node=2 dir=t pdo=4 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=24874983 hi=24901979 skew=26996",
    // line 131
    "COMM_PDO node=2 dir=r pdo=1 entries=5 cob=00000202 type=255 inhibit=0 compat=0 event=1000 "
    "lo=24905983 hi=24938979 skew=32996",
    // line 132
    "COMM_PDO node=2 dir=r pdo=2 entries=5 cob=00000302 type=255 inhibit=0 compat=0 event=1000 "
    "lo=24942001 hi=24974979 skew=32978",
    // line 133
    "COMM_PDO node=2 dir=r pdo=3 entries=5 cob=00000402 type=255 inhibit=0 compat=0 event=1000 "
    "lo=24978983 hi=25011979 skew=32996",
    // line 134
    "COMM_PDO node=2 dir=r pdo=4 entries=5 cob=00000502 type=255 inhibit=0 compat=0 event=1000 "
    "lo=25015983 hi=25048979 skew=32996",
    // line 135
    "COMM_MAP node=2 dir=r pdo=1 count=4 w1=60400010 w2=60600008 w3=60980008 w4=607a0020 "
    "lo=25052983 hi=25079979 skew=26996",
    // line 136
    "COMM_MAP node=2 dir=r pdo=2 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=25083983 hi=25110979 skew=26996",
    // line 137
    "COMM_MAP node=2 dir=r pdo=3 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=25114983 hi=25141979 skew=26996",
    // line 138
    "COMM_MAP node=2 dir=r pdo=4 count=0 w1=00000000 w2=00000000 w3=00000000 w4=00000000 "
    "lo=25145983 hi=25172979 skew=26996",
};
// The fixed K group sizes: NETWORK six, the node setting one, then 1800..1803 (six children),
// 1A00..1A03 (five), 1400..1403 (six) and 1600..1603 (five).
unsigned groupFields(unsigned group) {
    return group == 0 ? 6 : group == 1 ? 1 : (group - 2) / 4 % 2 == 0 ? 6 : 5;
}
// One K record replayed from a captured table: the header, the 18 typed lines and every RESULT as
// the S2 printed them, in that order, and no download. `network` replaces the typed NETWORK line
// and `producer` the 1017:00 value once a write has changed the fake's stored value.
void replayRead(Rig& rig, unsigned node, uint32_t id, uint32_t grant, const Captured* table,
                const char* const* typed, const char* header, const char* network = nullptr,
                uint16_t producer = 0) {
    rig.wire.script[node] = table;
    rig.wire.scriptLength[node] = 95;
    rig.wire.scriptAt[node] = 0;
    CHECK(rig.wire.stored[node] == producer);
    const unsigned downloads = rig.wire.downloads[node];
    CHECK(rig.reads.begin(rig.readRequest(node, id, grant), rig.now) == nullptr);
    rig.finish(rig.reads);
    rig.wire.script[node] = nullptr;
    CHECK(rig.wire.scriptAt[node] == 95 && rig.wire.downloads[node] == downloads);
    static char out[128][E::COMMUNICATION_LINE];
    const unsigned n = rig.lines(rig.reads, out, 128);
    CHECK(n == 1 + 18 + 95 + 1);
    CHECK(sameLine(out[0], header, "gap"));
    for (unsigned i = 0; i < 18; ++i)
        CHECK(sameLine(out[1 + i], i == 0 && network ? network : typed[i], "lo hi skew"));
    unsigned at = 0;
    for (unsigned group = 0; group < 18; ++group)
        for (unsigned step = 1; step <= groupFields(group); ++step, ++at) {
            const Captured& c = table[at];
            const uint32_t raw = c.index == 0x1017 ? producer : c.raw;
            char expected[E::COMMUNICATION_LINE];
            std::snprintf(expected, sizeof expected,
                          "RESULT op=%lu req=%lu oreq=0 step=%u object=%04x:%02x dir=0 outcome=0 "
                          "ack=1 possible=1 uncertain=0 abort=00000000 cleanup=0 size=%u "
                          "data=%02x%02x%02x%02x admitted=0 completed=0 ne=1 reason=0 st=3f",
                          static_cast<unsigned long>(id + group),
                          static_cast<unsigned long>(id + group), step, unsigned(c.index),
                          unsigned(c.sub), unsigned(c.size), unsigned(raw & 0xff),
                          unsigned(raw >> 8 & 0xff), unsigned(raw >> 16 & 0xff),
                          unsigned(raw >> 24));
            CHECK(sameLine(out[19 + at], expected, RESULT_SKIP));
        }
    CHECK(at == 95);
    CHECK(sameLine(out[114], "END comm terminals=95 groups=18; typed uploads only, nothing "
                             "written; r=release", ""));
    CHECK(rig.reads.release());
}

// K1 and K2 of 2026-11-09 11:21Z in one MCU boot (docs/bench/20261109T1121Z-r2b-p38_reads/rx.txt:
// COMM id=1 line 3, COMM id=19 line 120). The stored words as the drives reported them: all four
// TPDO COB-IDs valid at 0x180/0x280/0x380/0x480 + node, type 255 and inhibit raw 100; the four
// RPDOs valid with event raw 1000; TPDO1 map count 3, RPDO1 count 4 (6040, 6060, 6098, 607A) and
// the other maps empty; 1017 = 0, 2203 = 0. Read effects stay unknown (Q20), and none of these
// stored words is an active route, map or timing.
void replayReads20261109() {
    Rig rig;
    replayRead(rig, 1, 1, 10001, K_NODE1, K_TYPED_NODE1,
               "COMM id=1 node=1 failure=0 groups=18 published=18 group=18 outcome=1 uncertain=0 "
               "status=0 detail=0 terminals=95 grants=95 admission=0 gap=4004");
    rig.wire.compact();
    replayRead(rig, 2, 19, 10001, K_NODE2, K_TYPED_NODE2,
               "COMM id=19 node=2 failure=0 groups=18 published=18 group=18 outcome=1 uncertain=0 "
               "status=0 detail=0 terminals=95 grants=95 admission=0 gap=4004");
    CHECK(rig.wire.sentCount == 95 && rig.wire.frames[1] == 95 && rig.wire.frames[2] == 95);
}

// Boot A of 2026-11-09 (image 627bb42), the drives freshly power-cycled by the user: H on each
// node through the S14-fixed path. Node 1: docs/bench/20261109T1245Z-r4a-hb-node1/rx.txt; node 2:
// docs/bench/20261109T1246Z-r4a-hb-node2/rx.txt. Each plan: N<node>,p, 6 s, a D probe, K, a P
// watch (its heartbeat statistics are pdo_watch_test's replay), H, 5 s, n, K. The node's own 7F
// heartbeats after the write carry the watch's PDOW_F bounds; node 1's heartbeats during node 2's
// run, from its own commissioning a minute earlier, act on no node 2 fact and are left out.
struct Beat {
    uint64_t lo, hi;
};
struct Commissioning {
    unsigned node;
    uint32_t nmtId;
    uint64_t nmtAt; // The NMT line's admitted=.
    uint32_t probeId;
    uint64_t probeAt; // The D probe's first RESULT admitted=.
    uint32_t readId;
    uint64_t readAt; // K's first RESULT admitted=.
    const char* readHeader;
    uint32_t heartbeatId;
    uint64_t heartbeatAt;   // The H baseline's RESULT admitted=.
    const char* record[15]; // HB, HB_PREP, three RESULT, nine TRAFFIC, END.
    Beat beats[5];          // The node's heartbeats before n; the last is n's NETWORK lo/hi.
    uint32_t afterId;
    uint64_t afterAt;
    const char* afterHeader;
    const char* afterNetwork;
};
const Commissioning COMMISSIONING[2] = {
    {1, 3, 50626710, 4, 56671998, 5, 56828987,
     // line 17
     "COMM id=5 node=1 failure=0 groups=18 published=18 group=18 outcome=1 uncertain=0 status=0 "
     "detail=0 terminals=95 grants=95 admission=0 gap=4000",
     23, 60079987,
     {
         // line 136
         "HB id=23 node=1 failure=0 lost=0 state=4 outcome=1 write=1 ack=1 effects=1 uncertain=0 "
         "known=1 readback=1000 matched=1 published=1 raw=1000 terminals=3 gap=3017",
         // line 137
         "HB_PREP node=1 nmt=0 nmtState=0 prepared=1 migrated=1 cfg=2 scope=1 restart=0 "
         "persistence=0 status=0 detail=0",
         // line 138
         "RESULT op=23 req=1 oreq=164 step=1 object=1017:00 dir=0 outcome=0 ack=1 possible=1 "
         "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=60079987 "
         "completed=60082985 ne=1 reason=0 st=3f",
         // line 139
         "RESULT op=24 req=24 oreq=165 step=1 object=1017:00 dir=1 outcome=0 ack=1 possible=1 "
         "uncertain=0 abort=00000000 cleanup=0 size=0 data=00000000 admitted=60085987 "
         "completed=60088985 ne=1 reason=0 st=3f",
         // line 140
         "RESULT op=24 req=24 oreq=166 step=2 object=1017:00 dir=0 outcome=0 ack=1 possible=1 "
         "uncertain=0 abort=00000000 cleanup=0 size=2 data=e8030000 admitted=60092002 "
         "completed=60094985 ne=1 reason=0 st=3f",
         // line 141
         "TRAFFIC k=1 p=60079987 id=601 ext=0 rtr=0 dlc=8 data=4017100000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=127 txf=1",
         // line 142
         "TRAFFIC k=2 p=60082985 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=127 txf=1",
         // line 143
         "TRAFFIC k=0 p=60082985 id=581 ext=0 rtr=0 dlc=8 data=4b17100000000000 ok=1 exact=0 t=0 "
         "bounds=1 lo=60080274 hi=60082169 epoch=1",
         // line 144
         "TRAFFIC k=1 p=60085987 id=601 ext=0 rtr=0 dlc=8 data=2b171000e8030000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=128 txf=1",
         // line 145
         "TRAFFIC k=2 p=60088985 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=128 txf=1",
         // line 146
         "TRAFFIC k=0 p=60088985 id=581 ext=0 rtr=0 dlc=8 data=6017100000000000 ok=1 exact=0 t=0 "
         "bounds=1 lo=60086281 hi=60088539 epoch=1",
         // line 147
         "TRAFFIC k=1 p=60092002 id=601 ext=0 rtr=0 dlc=8 data=4017100000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=129 txf=1",
         // line 148
         "TRAFFIC k=2 p=60094985 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=129 txf=1",
         // line 149
         "TRAFFIC k=0 p=60094985 id=581 ext=0 rtr=0 dlc=8 data=4b171000e8030000 ok=1 exact=0 t=0 "
         "bounds=1 lo=60092288 hi=60094131 epoch=1",
         // line 150
         "END heartbeat terminals=3 traffic=9 tail=0 tailLost=0 lost=0; no save, reset or "
         "restore; r=release",
     },
     // PDOW_F lines 153-161; the last is also n's NETWORK line 162.
     {{61088163, 61089040}, {62089162, 62089852}, {63090162, 63090634}, {64091162, 64091440},
      {65092016, 65092232}},
     25, 65508987,
     // lines 172-173
     "COMM id=25 node=1 failure=0 groups=18 published=18 group=18 outcome=1 uncertain=0 status=0 "
     "detail=0 terminals=95 grants=95 admission=0 gap=4002",
     "COMM_NETWORK node=1 sync=00000080 emcy=00000081 producer=1000 sdo=2 cs=00000601 "
     "sc=00000581 lo=65508987 hi=65541985 skew=32998"},
    {2, 43, 78715722, 44, 84757995, 45, 84908987,
     // line 17
     "COMM id=45 node=2 failure=0 groups=18 published=18 group=18 outcome=1 uncertain=0 status=0 "
     "detail=0 terminals=95 grants=95 admission=0 gap=4000",
     63, 88169987,
     {
         // line 136
         "HB id=63 node=2 failure=0 lost=0 state=4 outcome=1 write=1 ack=1 effects=1 uncertain=0 "
         "known=1 readback=1000 matched=1 published=1 raw=1000 terminals=3 gap=3002",
         // line 137
         "HB_PREP node=2 nmt=0 nmtState=0 prepared=1 migrated=1 cfg=2 scope=1 restart=0 "
         "persistence=0 status=0 detail=0",
         // line 138
         "RESULT op=63 req=1 oreq=364 step=1 object=1017:00 dir=0 outcome=0 ack=1 possible=1 "
         "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=88169987 "
         "completed=88172985 ne=1 reason=0 st=3f",
         // line 139
         "RESULT op=64 req=64 oreq=365 step=1 object=1017:00 dir=1 outcome=0 ack=1 possible=1 "
         "uncertain=0 abort=00000000 cleanup=0 size=0 data=00000000 admitted=88175987 "
         "completed=88178985 ne=1 reason=0 st=3f",
         // line 140
         "RESULT op=64 req=64 oreq=366 step=2 object=1017:00 dir=0 outcome=0 ack=1 possible=1 "
         "uncertain=0 abort=00000000 cleanup=0 size=2 data=e8030000 admitted=88181987 "
         "completed=88184985 ne=1 reason=0 st=3f",
         // line 141
         "TRAFFIC k=1 p=88169987 id=602 ext=0 rtr=0 dlc=8 data=4017100000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=128 txf=1",
         // line 142
         "TRAFFIC k=2 p=88172985 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=128 txf=1",
         // line 143
         "TRAFFIC k=0 p=88172985 id=582 ext=0 rtr=0 dlc=8 data=4b17100000000000 ok=1 exact=0 t=0 "
         "bounds=1 lo=88170273 hi=88172178 epoch=1",
         // line 144
         "TRAFFIC k=1 p=88175987 id=602 ext=0 rtr=0 dlc=8 data=2b171000e8030000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=129 txf=1",
         // line 145
         "TRAFFIC k=2 p=88178985 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=129 txf=1",
         // line 146
         "TRAFFIC k=0 p=88178985 id=582 ext=0 rtr=0 dlc=8 data=6017100000000000 ok=1 exact=0 t=0 "
         "bounds=1 lo=88176282 hi=88178531 epoch=1",
         // line 147
         "TRAFFIC k=1 p=88181987 id=602 ext=0 rtr=0 dlc=8 data=4017100000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=130 txf=1",
         // line 148
         "TRAFFIC k=2 p=88184985 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 "
         "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=130 txf=1",
         // line 149
         "TRAFFIC k=0 p=88184985 id=582 ext=0 rtr=0 dlc=8 data=4b171000e8030000 ok=1 exact=0 t=0 "
         "bounds=1 lo=88182273 hi=88184188 epoch=1",
         // line 150
         "END heartbeat terminals=3 traffic=9 tail=0 tailLost=0 lost=0; no save, reset or "
         "restore; r=release",
     },
     // PDOW_F lines 154-166 for 702; the last is also n's NETWORK line 168.
     {{89178162, 89178561}, {90179159, 90179335}, {91180014, 91180149}, {92180159, 92180925},
      {93181159, 93181730}},
     65, 93599987,
     // lines 178-179
     "COMM id=65 node=2 failure=0 groups=18 published=18 group=18 outcome=1 uncertain=0 status=0 "
     "detail=0 terminals=95 grants=95 admission=0 gap=4002",
     "COMM_NETWORK node=2 sync=00000080 emcy=00000082 producer=1000 sdo=2 cs=00000602 "
     "sc=00000582 lo=93599987 hi=93632985 skew=32998"},
};
// The measured commissioning, per node: N, then the D probe more than 5 s later, when the N line's
// window had ended unobserved (no heartbeat at producer time 0) and only the probe's ordinary
// service remained; the K reads; H: baseline 1017:00 = 0, the one download 2B 17 10 00 E8 03 00 00
// acknowledged, the owner's migration to configuration epoch 2, the readback raw 1000 published,
// with no uncertainty; then the node's own heartbeats, Pre-operational, its producer interval
// still withdrawn (a stored readback is no active timing), and a K that reads producer 1000.
void replayCommissioning20261109() {
    for (const Commissioning& c : COMMISSIONING) {
        Rig rig;
        rig.firmwareTurn = true;
        const uint8_t slot = uint8_t(c.node - 1);
        const Captured* table = c.node == 1 ? K_NODE1 : K_NODE2;
        const char* const* typed = c.node == 1 ? K_TYPED_NODE1 : K_TYPED_NODE2;
        rig.until(c.nmtAt);
        CHECK(rig.nmt(c.node, C::NmtCommand::PRE_OPERATIONAL, c.nmtId) == c.nmtAt);
        const C::NodeSnapshot& node = *rig.services.node(slot);
        CHECK(node.transitionPending && node.transitionDeadlineUs == c.nmtAt + 5000000);
        rig.until(c.probeAt - 2000);
        // The 6 s wait: the window ended with nothing observed; the state stays unknown.
        CHECK(!node.transitionPending && node.transitionExpired && !node.transitionObserved);
        rig.probe(uint8_t(c.node), c.probeId);
        rig.until(c.readAt - 1000);
        replayRead(rig, c.node, c.readId, 10001, table, typed, c.readHeader);
        rig.wire.compact();
        rig.until(c.heartbeatAt - 1000);
        CHECK(rig.services.configurationEligible(slot, rig.services.channel(slot)->binding,
                                                 rig.now));
        CHECK(rig.commission.begin(rig.heartbeatRequest(c.node, c.heartbeatId, 20001), rig.now) ==
              nullptr);
        rig.finish(rig.commission);
        static char out[32][E::COMMUNICATION_LINE];
        CHECK(rig.lines(rig.commission, out, 32) == 15);
        CHECK(sameLine(out[0], c.record[0], "gap"));
        CHECK(sameLine(out[1], c.record[1], ""));
        for (unsigned i = 2; i < 5; ++i)
            CHECK(sameLine(out[i], c.record[i], RESULT_SKIP));
        for (unsigned i = 5; i < 14; ++i)
            CHECK(sameLine(out[i], c.record[i], TRAFFIC_SKIP));
        CHECK(sameLine(out[14], c.record[14], ""));
        const E::HeartbeatBenchRecord& r = *rig.commission.record();
        CHECK(r.failure == E::HeartbeatBenchFailure::NONE && r.migrated && r.published &&
              !r.context.uncertain && r.result.producerTimeRaw == 1000);
        CHECK(rig.wire.downloads[c.node] == 1 && rig.wire.stored[c.node] == 1000);
        CHECK(rig.commission.release());
        for (const Beat& b : c.beats)
            rig.beat(c.node, b.lo, b.hi);
        // n's NETWORK line: epoch=1 state=127 qualified=1 producerKnown=0 ms=0 pending=0
        // observed=0 expired=1 exact=0 bounds=1 and the last heartbeat's lo and hi.
        C::ServiceEligibility e;
        CHECK(C::serviceEligibility(node, rig.now, e) && e.exactNmtKnown &&
              e.state == C::NmtState::PRE_OPERATIONAL);
        CHECK(node.route.generation == 1 && node.nmt == C::NmtState::PRE_OPERATIONAL);
        CHECK(!node.producerKnown && node.producerTimeMs == 0 && !node.transitionPending &&
              !node.transitionObserved && node.transitionExpired);
        CHECK(!node.nmtTime.receivedTimeKnown && node.nmtTime.receiveBoundsKnown &&
              node.nmtTime.receivedEarliestUs == c.beats[4].lo &&
              node.nmtTime.receivedLatestUs == c.beats[4].hi);
        rig.wire.compact();
        rig.until(c.afterAt - 1000);
        replayRead(rig, c.node, c.afterId, 30001, table, typed, c.afterHeader, c.afterNetwork,
                   1000);
        CHECK(!node.producerKnown && rig.wire.downloads[c.node] == 1);
    }
}

// The 2026-11-09 failure on the old image (757f1cd), at its captured instants. Node 1
// (docs/bench/20261109T1121Z-r2b-p38_heartbeat_node1/rx.txt): N1,p admitted 45846649 (line 3), the
// probe's upload 45886983 (line 12), K1 from 46039983 (line 18), the H baseline admitted 49299981
// and the write acknowledged at 49308981 (lines 32-33). Node 2
// (20261109T1123Z-r2b-hb-node2-probe-last/rx.txt): N2,p 125956669 (line 3), K2 from 125995983
// (line 25), the probe 129280982 (line 130), the baseline 129446981 and the acknowledgement at
// 129455981 (lines 138-139). On that image H was admitted, and after the acknowledged write the
// owner refused the epoch migration (HB failure=10 migrated=0, lines 30-31 and 136-137): each N
// line's window, its admission + 5 s, still held, 1537668 us and 1500688 us past the
// acknowledgements. At the same instants the adapter refuses the migration premises now, and H
// refuses at its captured admission with nothing sent; once the window ends they hold.
void replayPendingTransition20261109() {
    struct Run {
        unsigned node;
        uint32_t nmtId;
        uint64_t nmtAt;
        uint32_t probeId;
        uint64_t probeAt;
        uint32_t readId;
        uint64_t readAt, baselineAt, acknowledgedAt, margin;
        const char* readHeader;
    };
    const Run runs[2] = {
        {1, 37, 45846649, 38, 45886983, 39, 46039983, 49299981, 49308981, 1537668,
         // line 17
         "COMM id=39 node=1 failure=0 groups=18 published=18 group=18 outcome=1 uncertain=0 "
         "status=0 detail=0 terminals=95 grants=95 admission=0 gap=4005"},
        {2, 77, 125956669, 96, 129280982, 78, 125995983, 129446981, 129455981, 1500688,
         // line 6
         "COMM id=78 node=2 failure=0 groups=18 published=18 group=18 outcome=1 uncertain=0 "
         "status=0 detail=0 terminals=95 grants=95 admission=0 gap=4004"}};
    for (const Run& run : runs) {
        Rig rig;
        rig.firmwareTurn = true;
        const uint8_t slot = uint8_t(run.node - 1);
        rig.until(run.nmtAt);
        CHECK(rig.nmt(run.node, C::NmtCommand::PRE_OPERATIONAL, run.nmtId) == run.nmtAt);
        const C::NodeSnapshot& node = *rig.services.node(slot);
        const bool probeFirst = run.probeAt < run.readAt;
        if (probeFirst) {
            rig.until(run.probeAt - 2000);
            rig.probe(uint8_t(run.node), run.probeId);
        }
        rig.until(run.readAt - 1000);
        replayRead(rig, run.node, run.readId, 10001, run.node == 1 ? K_NODE1 : K_NODE2,
                   run.node == 1 ? K_TYPED_NODE1 : K_TYPED_NODE2, run.readHeader);
        rig.wire.compact();
        if (!probeFirst) {
            rig.until(run.probeAt - 10000);
            rig.probe(uint8_t(run.node), run.probeId);
        }
        // The premises the old image did not check: service alone held, and the window was open.
        C::ServiceEligibility e;
        CHECK(C::serviceEligibility(node, rig.now, e) && e.ordinarySdo && !e.exactNmtKnown);
        CHECK(node.transitionPending && node.transitionDeadlineUs == run.nmtAt + 5000000);
        CHECK(node.transitionDeadlineUs - run.acknowledgedAt == run.margin);
        rig.until(run.baselineAt - 1000);
        const unsigned sent = rig.wire.sentCount;
        CHECK(refused(rig.commission.begin(rig.heartbeatRequest(run.node, 57, 20001), rig.now),
                      "transition"));
        CHECK(!rig.commission.record() && rig.wire.sentCount == sent &&
              !rig.wire.downloads[run.node]);
        const M::Binding& binding = rig.services.channel(slot)->binding;
        rig.until(run.acknowledgedAt);
        CHECK(!rig.services.configurationEligible(slot, binding, rig.now));
        rig.until(node.transitionDeadlineUs - 1);
        CHECK(node.transitionPending &&
              !rig.services.configurationEligible(slot, binding, rig.now));
        rig.until(node.transitionDeadlineUs + 1000);
        CHECK(!node.transitionPending && node.transitionExpired &&
              rig.services.configurationEligible(slot, binding, rig.now));
        CHECK(rig.wire.sentCount == sent && !rig.wire.stored[run.node]);
    }
}

// The fix on hardware, 2026-11-09 12:06Z (image 627bb42), one MCU boot, node 1 still holding
// 1017:00 = raw 1000 from the 11:21Z write and heartbeating 7F about every 1.0008 s.
// Transition (docs/bench/20261109T1206Z-r3-hb-transition-node1b/rx.txt): node 1's last heartbeat
// before N was processed at 51860981 (DISC_T hbp, line 7); N1,p admitted 52314711 (line 3); the
// probe's upload 52356007 (line 12); H refused reason=transition (line 16), all inside one
// heartbeat period. The S2 printed no receive bounds for that heartbeat; any interval before the
// N line acts the same, and this replay gives it [51859981, 51860881].
// Baseline (20261109T1206Z-r3-hb-baseline-node1b/rx.txt): the heartbeats 52861151..52861571 and
// 53862134..53862364 (EVENT lines 8 and 10), N1,p admitted 54315734 (line 3), the heartbeat
// 55863138..55863948 that n listed (line 5: Pre-operational, observed, not pending), then H, its
// baseline admitted at 56960989. It read raw 1000 and ended failure=9, HB_PREP nmt=1 nmtState=127
// prepared=0 status=1 detail=2 (lines 13-19). Detail 2 is ReadError::ARGUMENT from
// prepareCommunication's NMT premise: a known Pre-operational state must come from a heartbeat
// received at or after the reservation, and every heartbeat had been received before H was
// admitted; none came in the record's 3 ms (its TRAFFIC, lines 16-18, has none). The plan's
// predicted detail=6, VALUE, is the baseline check after it: reached only if a heartbeat is
// received between the H admission and the baseline, as the last part shows. Nothing was written.
void replayHeartbeatRefusals20261109() {
    Rig rig;
    rig.firmwareTurn = true;
    rig.wire.stored[1] = 1000;
    // Transition.
    rig.until(51860881);
    M::Frame beat = OwnerRig::frame(0x701, 1);
    beat.data[0] = 0x7f;
    rig.wire.pushBounded(beat, 51859981, 51860881);
    rig.turn(100);
    const C::NodeSnapshot& node = *rig.services.node(0);
    CHECK(node.nmtTime.processedUs == 51860981 && node.nmt == C::NmtState::PRE_OPERATIONAL);
    rig.until(52314711);
    CHECK(rig.nmt(1, C::NmtCommand::PRE_OPERATIONAL, 2) == 52314711);
    rig.until(52354007);
    rig.probe(1, 3);
    CHECK(node.transitionPending && !node.transitionObserved);
    CHECK(refused(rig.commission.begin(rig.heartbeatRequest(1, 5, 20001), rig.now), "transition"));
    CHECK(!rig.commission.record() && rig.wire.sentCount == 1 + 2 && !rig.wire.downloads[1]);
    // Baseline: the next heartbeat observes the first transition; then a second N line.
    rig.beat(1, 52861151, 52861571);
    CHECK(!node.transitionPending && node.transitionObserved);
    rig.beat(1, 53862134, 53862364);
    rig.until(54315734);
    CHECK(rig.nmt(1, C::NmtCommand::PRE_OPERATIONAL, 4) == 54315734);
    CHECK(node.transitionPending);
    rig.beat(1, 55863138, 55863948);
    // n (line 5): state=127 qualified=1 pending=0 observed=1 expired=0 bounds=1 and these bounds.
    C::ServiceEligibility e;
    CHECK(C::serviceEligibility(node, rig.now, e) && e.exactNmtKnown &&
          e.state == C::NmtState::PRE_OPERATIONAL && e.ordinarySdo);
    CHECK(!node.transitionPending && node.transitionObserved && !node.transitionExpired);
    CHECK(node.nmtTime.receivedEarliestUs == 55863138 && node.nmtTime.receivedLatestUs == 55863948);
    rig.wire.compact();
    rig.until(56959989);
    CHECK(rig.commission.begin(rig.heartbeatRequest(1, 5, 30001), rig.now) == nullptr);
    rig.finish(rig.commission);
    const char* const captured[7] = {
        // line 13
        "HB id=5 node=1 failure=9 lost=0 state=0 outcome=0 write=0 ack=0 effects=0 uncertain=0 "
        "known=0 readback=0 matched=0 published=0 raw=0 terminals=1 gap=2998",
        // line 14
        "HB_PREP node=1 nmt=1 nmtState=127 prepared=0 migrated=0 cfg=1 scope=1 restart=0 "
        "persistence=0 status=1 detail=2",
        // line 15
        "RESULT op=5 req=1 oreq=10 step=1 object=1017:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=e8030000 admitted=56960989 "
        "completed=56963987 ne=1 reason=0 st=3f",
        // line 16
        "TRAFFIC k=1 p=56960989 id=601 ext=0 rtr=0 dlc=8 data=4017100000000000 ok=1 exact=0 t=0 "
        "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=3 txf=1",
        // line 17
        "TRAFFIC k=2 p=56963987 id=0 ext=0 rtr=0 dlc=0 data=0000000000000000 ok=1 exact=0 t=0 "
        "bounds=0 lo=0 hi=0 epoch=1 txk=0 txr=3 txf=1",
        // line 18
        "TRAFFIC k=0 p=56963987 id=581 ext=0 rtr=0 dlc=8 data=4b171000e8030000 ok=1 exact=0 t=0 "
        "bounds=1 lo=56961273 hi=56963141 epoch=1",
        // line 19
        "END heartbeat terminals=1 traffic=3 tail=0 tailLost=0 lost=0; no save, reset or restore; "
        "r=release",
    };
    static char out[16][E::COMMUNICATION_LINE];
    CHECK(rig.lines(rig.commission, out, 16) == 7);
    CHECK(sameLine(out[0], captured[0], "gap"));
    CHECK(sameLine(out[1], captured[1], ""));
    CHECK(sameLine(out[2], captured[2], RESULT_SKIP));
    for (unsigned i = 3; i < 6; ++i)
        CHECK(sameLine(out[i], captured[i], TRAFFIC_SKIP));
    CHECK(sameLine(out[6], captured[6], ""));
    const E::HeartbeatBenchRecord& r = *rig.commission.record();
    CHECK(r.failure == E::HeartbeatBenchFailure::BASELINE && !r.prepared && r.nmt.known &&
          r.status.detail == int32_t(M::ReadError::ARGUMENT));
    CHECK(r.nmt.time.receivedEarliestUs == 55863138 &&
          r.nmt.time.receivedEarliestUs < r.reservation.acquiredUs);
    CHECK(!rig.wire.downloads[1] && !r.context.effectsPossible && !rig.wire.guardCalls);
    CHECK(rig.commission.release());
    // Counterfactual: a heartbeat received after the H admission and processed before the baseline
    // admission gives the NMT premise; the raw 1000 baseline then refuses as VALUE.
    rig.until(57959989);
    CHECK(rig.commission.begin(rig.heartbeatRequest(1, 7, 40001), rig.now) == nullptr);
    rig.wire.pushBounded(beat, rig.now + 10, rig.now + 50);
    rig.finish(rig.commission);
    const E::HeartbeatBenchRecord& late = *rig.commission.record();
    CHECK(late.failure == E::HeartbeatBenchFailure::BASELINE && late.nmt.known &&
          late.status.detail == int32_t(M::ReadError::VALUE) && !rig.wire.downloads[1]);
    CHECK(rig.commission.release());
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
    heartbeatWaitsForPendingTransition();
    heartbeatProbeOrderAroundNmt();
    heartbeatRechecksBeforeWrite();
    routeSessionKeepsReservation();
    replayReads20261109();
    replayCommissioning20261109();
    replayPendingTransition20261109();
    replayHeartbeatRefusals20261109();
    std::printf("communication bench: %u checks passed\n", checks);
}
