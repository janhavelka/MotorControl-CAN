// SPDX-License-Identifier: MIT
// Capacity contracts of one compile-time capacity set (adapters/canopennode/Capacity.h), through the
// real SDO store, owner, TWAI adapter, ProbeApp, DiscoveryScan and AxisSession on fake drives. The
// same source is built once per channel count: `capacity` with the configured set (default two
// channels) and `capacity_n1`, `capacity_n8` and `capacity_n32`, each from its own build of the
// adapters and the shared application (test/CMakeLists.txt). Every node, route, reply, identity and
// motion value comes from the fakes here and in support/MoveRig.h. A passing run shows storage,
// admission and scheduling at N channels; it says nothing about a drive, a bus or the timing of N
// real axes. The turn-cost line it prints is a host measurement for information, not a contract.
#include "DiscoveryScan.h"
#include "ProbeApp.h"
#include "TwaiAdapter.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

static unsigned checks = 0;
static bool asyncTraffic = false; // MoveRig.h's switch; this suite sends no unsolicited traffic.
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s, N=%u]\n", __FILE__, __LINE__, #condition,         \
                         scenario, unsigned(N));                                                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
namespace {
enum { N = MotorControlCAN::CANOPEN::SdoClient::CHANNELS };
}
#include "support/MoveRig.h"

#ifndef CAPACITY_TEST_CHANNELS
#error "test/CMakeLists.txt defines CAPACITY_TEST_CHANNELS for each capacity build"
#endif

namespace {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;

static_assert(int(N) == CAPACITY_TEST_CHANNELS, "This build sees the capacity set its target names");
static_assert(int(O::CanBusOwner::SEQUENCES) == int(N) && int(O::CanBusOwner::URGENT_REQUESTS) == int(N),
              "One reservation and one urgent slot per channel");
static_assert(int(E::TwaiAdapter::TX_CAPACITY) == int(N) + 1, "One TX slot per channel plus the NMT frame");
#ifndef MOTORCONTROLCAN_OWNER_REQUESTS
static_assert(int(O::CanBusOwner::REQUESTS) == (int(N) + 4 > 6 ? int(N) + 4 : 6), "Derived ordinary slots");
#endif
#ifndef MOTORCONTROLCAN_OWNER_PRODUCERS
static_assert(int(O::CanBusOwner::PRODUCERS) == (int(N) + 2 > 4 ? int(N) + 2 : 4), "Derived producers");
#endif
#ifndef MOTORCONTROLCAN_DISCOVERY_TARGETS
static_assert(int(E::DiscoveryPlan::CAPACITY) == (int(N) < 2 ? int(N) : 2), "Derived discovery targets");
#endif
static_assert(int(O::CanBusOwner::PRODUCERS) >= int(N), "This suite gives every binding its own producer");
static_assert(int(O::CanBusOwner::REQUESTS) >= int(N), "This suite queues one ordinary request per binding");

enum { REQUESTS = O::CanBusOwner::REQUESTS, TOTAL = O::CanBusOwner::TOTAL_REQUESTS };
// Generous budgets: N axes share two frames per turn, so each waits about N/2 turns for its next one.
const uint64_t TICK = 500;
const uint32_t QUEUE = 100000, RESPONSE = 100000;
const uint64_t LEAD = 30000000;

L::ReadOptions reads() {
    L::ReadOptions value;
    value.queueTimeoutUs = QUEUE;
    value.responseTimeoutUs = RESPONSE;
    return value;
}
L::MoveOptions moveOptions() {
    L::MoveOptions value;
    value.service = reads();
    value.observationIntervalUs = 1000;
    value.maximumAgeUs = 5000000;
    return value;
}
C::SdoChannel route(unsigned node) { return MoveRig::target(node).channel; }
M::Binding binding(unsigned node) { return route(node).binding; }
uint16_t indexOf(const M::Frame& f) { return uint16_t(f.data[1] | (uint16_t(f.data[2]) << 8)); }
// The node an SDO request frame addresses, or zero for any other frame.
unsigned nodeOf(const M::Frame& f) {
    return f.id > 0x600 && f.id <= 0x600u + unsigned(N) && f.payloadLength == 8 ? f.id - 0x600 : 0;
}
bool isWord(const M::Frame& f, uint16_t word) {
    return nodeOf(f) && f.data[0] == 0x2b && indexOf(f) == 0x6040 && f.data[3] == 0 &&
           (f.data[4] | (uint16_t(f.data[5]) << 8)) == word;
}

// Fixed storage for objects that borrow the owner and cannot be copied, built in place once.
template <class T, unsigned Count> struct Placed {
    alignas(T) unsigned char bytes[Count][sizeof(T)];
    unsigned built = 0;
    Placed() = default;
    Placed(const Placed&) = delete;
    Placed& operator=(const Placed&) = delete;
    template <class Argument> void build(Argument& argument) {
        for (; built < Count; ++built)
            new (static_cast<void*>(bytes[built])) T(argument);
    }
    T& operator[](unsigned i) { return *reinterpret_cast<T*>(bytes[i]); }
    const T& operator[](unsigned i) const { return *reinterpret_cast<const T*>(bytes[i]); }
    ~Placed() {
        while (built)
            reinterpret_cast<T*>(bytes[--built])->~T();
    }
};

// The fake controller: every handed-off frame is logged and reported sent at once. Frames reach
// their fake drive at the start of the next turn, whose replies the owner then receives.
struct Wire {
    struct Sent {
        M::Frame frame;
        O::TransportTxId id;
        uint32_t epoch = 0;
    };
    std::vector<Sent> sent;
    std::vector<O::ReceivedFrame> rx;
    std::vector<O::TxReport> reports;
    size_t rxRead = 0, reportRead = 0, delivered = 0;
    MoveRig::FakeDrive* drives = nullptr; // Node n is drives[n - 1].
    bool answer = true;
    static Wire& of(void* context) { return *static_cast<Wire*>(context); }
    static bool receive(void* context, O::ReceivedFrame& output) {
        Wire& w = of(context);
        if (w.rxRead == w.rx.size())
            return false;
        output = w.rx[w.rxRead++];
        return true;
    }
    static bool transmit(void* context, const M::Frame& value, O::TransportTxId id, uint32_t epoch) {
        Wire& w = of(context);
        Sent s;
        s.frame = value;
        s.id = id;
        s.epoch = epoch;
        w.sent.push_back(s);
        O::TxReport report;
        report.id = id;
        report.controllerEpoch = epoch;
        report.transmitted = true;
        w.reports.push_back(report);
        return true;
    }
    static bool takeReport(void* context, O::TxReport& output) {
        Wire& w = of(context);
        if (w.reportRead == w.reports.size())
            return false;
        output = w.reports[w.reportRead++];
        return true;
    }
    static bool retire(void* context, O::TransportTxId id, uint32_t epoch) {
        Wire& w = of(context);
        size_t kept = w.reportRead;
        for (size_t i = w.reportRead; i < w.reports.size(); ++i)
            if (!O::sameTransportTxId(w.reports[i].id, id) || w.reports[i].controllerEpoch != epoch)
                w.reports[kept++] = w.reports[i];
        w.reports.resize(kept);
        return true;
    }
    O::Transport transport() {
        O::Transport value;
        value.context = this;
        value.receive = receive;
        value.transmit = transmit;
        value.takeTxReport = takeReport;
        value.retire = retire;
        return value;
    }
    void push(const M::Frame& value, uint64_t now) {
        O::ReceivedFrame input;
        input.frame = value;
        input.receivedTimeKnown = true;
        input.receivedUs = now;
        input.controllerEpoch = 1;
        rx.push_back(input);
    }
    void deliver(uint64_t now) {
        for (; delivered < sent.size(); ++delivered) {
            const M::Frame& f = sent[delivered].frame;
            const unsigned node = nodeOf(f);
            M::Frame reply;
            if (answer && node && drives[node - 1].answer && MoveRig::replyTo(f, drives[node - 1], reply) &&
                !drives[node - 1].mute)
                push(reply, now);
        }
    }
    // 6040 downloads of `word` to `node` handed off from frame `from` on.
    unsigned words(unsigned node, size_t from, uint16_t word) const {
        unsigned count = 0;
        for (size_t i = from; i < sent.size(); ++i)
            count += nodeOf(sent[i].frame) == node && isWord(sent[i].frame, word);
        return count;
    }
};

// The first N frames handed off are one 000B per node; index 0 counts frames of no node.
void stopsFirst(const Wire& wire) {
    CHECK(wire.sent.size() >= unsigned(N));
    unsigned perNode[N + 1] = {};
    for (unsigned i = 0; i < unsigned(N); ++i) {
        const M::Frame& f = wire.sent[i].frame;
        CHECK(isWord(f, 0x000b));
        ++perNode[nodeOf(f)];
    }
    CHECK(!perNode[0]);
    for (unsigned node = 1; node <= unsigned(N); ++node)
        CHECK(perNode[node] == 1);
}
void configureNode(C::SdoClient& services, unsigned node) {
    CHECK(services.configure(uint8_t(node - 1), route(node), true, true));
    C::NetworkRoute network;
    network.binding = binding(node);
    network.generation = 11;
    network.heartbeatId = 0x700 + node;
    network.emcyId = 0x80 + node;
    CHECK(services.configureNetwork(uint8_t(node - 1), network, C::NetworkObserver(), 0));
}

// One axis: its session, latest reads, and the coordinates and witness its move borrows.
struct Axis {
    E::AxisSession a;
    L::ReadContext state;
    L::IdentityObservation identity;
    M::AxisConfig coordinates;
    M::AxisReference witness;
    explicit Axis(O::CanBusOwner& owner) : a(owner) {}
};

// N configured channels, nodes 1..N, on the real store and owner. Allocated once on the heap: at
// N = 32 the sessions alone exceed a thread stack.
struct Rig {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    MoveRig::FakeDrive drives[N];
    Placed<Axis, N> axes;
    uint64_t now = 0;
    Rig() : owner(services, wire.transport(), 73) {
        wire.drives = drives;
        for (unsigned node = 1; node <= unsigned(N); ++node)
            configureNode(services, node);
        axes.build(owner);
        for (unsigned node = 1; node <= unsigned(N); ++node)
            axis(node).coordinates = MoveRig::axisConfig(node);
    }
    Rig(const Rig&) = delete;
    Rig& operator=(const Rig&) = delete;
    Axis& axis(unsigned node) { return axes[node - 1]; }
    MoveRig::FakeDrive& drive(unsigned node) { return drives[node - 1]; }
    L::ReadTarget current(unsigned node) const {
        L::ReadTarget value = MoveRig::target(node);
        value.nodeGeneration = services.node(uint8_t(node - 1))->route.generation;
        return value;
    }
    void ownerTurn(O::ServiceBudget b = OwnerRig::budget(8, 2, 8, 2, 1)) {
        now += TICK;
        wire.deliver(now);
        CHECK(owner.service(now, b));
    }
    void turn() {
        ownerTurn();
        for (unsigned i = 0; i < unsigned(N); ++i)
            axes[i].a.service(now);
    }
    // The session's one terminal, after the turns it needs; one axis at a time needs two.
    C::SdoCompletion settled(unsigned node) {
        C::SdoCompletion output;
        bool done = false;
        for (unsigned i = 0; i < 20 && !done; ++i) {
            turn();
            done = axis(node).a.settle(output);
        }
        CHECK(done);
        return output;
    }
    void readState(unsigned node) {
        E::AxisSession& s = axis(node).a;
        L::ReadContext& into = axis(node).state;
        const uint32_t operation = s.beginOperation(now + LEAD, now);
        CHECK(operation);
        CHECK(L::prepareState(into, current(node), operation, now, now + LEAD, reads(), MoveRig::STATE_MASK));
        while (into.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            CHECK(s.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            const C::SdoCompletion terminal = settled(node); // Advances now: never in one expression.
            CHECK(L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    void readIdentity(unsigned node) {
        static L::ReadContext read;
        E::AxisSession& s = axis(node).a;
        const uint32_t operation = s.beginOperation(now + LEAD, now);
        CHECK(operation);
        CHECK(L::prepareIdentity(read, current(node), operation, now, now + LEAD, reads()));
        while (read.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(read, now, work) && work.ready);
            CHECK(s.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            const C::SdoCompletion terminal = settled(node);
            CHECK(L::advanceRead(read, terminal, now));
        }
        CHECK(read.state == M::ReadState::SUCCEEDED);
        CHECK(L::getIdentity(read, current(node), now, LEAD, axis(node).identity));
    }
    // An explicit acknowledged 000F stands in for the enable, as in support/MoveRig.h.
    void writeControl(unsigned node, uint16_t word) {
        E::AxisSession& s = axis(node).a;
        const uint32_t operation = s.beginOperation(now + LEAD, now);
        CHECK(operation);
        C::SdoTransfer t;
        t.channel = current(node).channel;
        t.operation = t.request = operation;
        t.step = 1;
        t.index = 0x6040;
        t.size = 2;
        t.direction = C::SdoDirection::DOWNLOAD;
        t.data[0] = uint8_t(word);
        t.data[1] = uint8_t(word >> 8);
        t.deadlineUs = now + LEAD;
        t.queueTimeoutUs = QUEUE;
        t.responseTimeoutUs = RESPONSE;
        CHECK(s.admitPrepared(t, now) == O::BusAdmission::ACCEPTED);
        CHECK(settled(node).outcome == C::SdoOutcome::SUCCESS);
    }
    void ready(unsigned node) {
        CHECK(axis(node).a.open(current(node), L::SettingsModel::CL86_C, uint8_t(node), now));
        readIdentity(node);
        writeControl(node, 0x000f);
        readState(node);
    }
    M::Status relative(unsigned node, int64_t displacement) {
        Axis& x = axis(node);
        x.witness = M::AxisReference();
        x.witness.binding = x.coordinates.binding;
        x.witness.referenceGeneration = x.coordinates.referenceGeneration;
        x.witness.idle = x.witness.stationary = x.witness.nativeKnown = true;
        x.witness.nativePosition = drive(node).position;
        x.witness.basis = M::RelativeBasis::ACTUAL;
        x.witness.source = M::ScaleSource::QUALIFIED;
        x.witness.observedUs = x.witness.nowUs = now;
        x.witness.maximumAgeUs = 5000000;
        L::MoveEvidence evidence = MoveRig::evidence();
        evidence.relativeBase = L::RelativeBase::ACTUAL;
        M::MoveRequest request = MoveRig::moveRequest(displacement);
        request.position.relative = true;
        request.position.basis = M::RelativeBasis::ACTUAL;
        return x.a.requestMoveRelative(x.state, x.identity, evidence, request, MoveRig::profile(), nullptr,
                                       x.coordinates, x.witness, moveOptions(), now + LEAD, now);
    }
};

O::BusRequest upload(O::SequenceId sequence, unsigned node, uint32_t operation, uint32_t step,
                     uint64_t deadline, uint32_t queue = QUEUE, uint32_t response = RESPONSE) {
    O::BusRequest work;
    work.sequence = sequence;
    work.transfer.channel = route(node);
    work.transfer.operation = operation;
    work.transfer.request = work.transfer.step = step;
    work.transfer.index = 0x6041;
    work.transfer.size = 2;
    work.transfer.direction = C::SdoDirection::UPLOAD;
    work.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
    work.transfer.queueTimeoutUs = queue;
    work.transfer.responseTimeoutUs = response;
    work.transfer.deadlineUs = deadline;
    return work;
}
// The 000B bytes as an owner request: an owner fixture, not a validated motor stop.
C::SdoTransfer stopTransfer(unsigned node, uint32_t operation, uint32_t step, uint64_t deadline) {
    C::SdoTransfer t;
    t.channel = route(node);
    t.operation = operation;
    t.request = t.step = step;
    t.index = 0x6040;
    t.size = 2;
    t.direction = C::SdoDirection::DOWNLOAD;
    t.data[0] = 0x0b;
    t.queueTimeoutUs = QUEUE;
    t.responseTimeoutUs = RESPONSE;
    t.deadlineUs = deadline;
    return t;
}
void reserveAll(Rig& r, O::SequenceId (&ids)[N], uint64_t deadline) {
    for (unsigned node = 1; node <= unsigned(N); ++node) {
        CHECK(r.owner.beginSequence(binding(node), uint8_t(node), r.now, ids[node - 1]));
        CHECK(r.owner.beginOperation(ids[node - 1], 1, deadline, r.now));
    }
}
// Every ordinary slot taken, in rotation over the bindings; steps continues each binding's count.
void fillOrdinary(Rig& r, const O::SequenceId (&ids)[N], uint32_t (&steps)[N], uint64_t deadline,
                  uint32_t queue = QUEUE, uint32_t response = RESPONSE) {
    for (unsigned i = 0; i < unsigned(REQUESTS); ++i) {
        const unsigned c = i % unsigned(N);
        O::RequestId id;
        CHECK(r.owner.admit(upload(ids[c], c + 1, 1, ++steps[c], deadline, queue, response), r.now, id) ==
              O::BusAdmission::ACCEPTED);
        CHECK(id.slot < unsigned(REQUESTS));
    }
}

// 1. N routes configure and slot N does not exist; one reservation per binding, N in all.
void routesAndReservations() {
    scenario = "routes and reservations";
    std::unique_ptr<Rig> r(new Rig);
    for (unsigned c = 0; c < unsigned(N); ++c) {
        CHECK(r->services.channel(uint8_t(c)) && r->services.node(uint8_t(c)));
        CHECK(r->services.channel(uint8_t(c))->binding.endpoint.address == c + 1);
        CHECK(!r->services.unresolved(uint8_t(c)) && r->services.port(uint8_t(c)));
    }
    // The slot after the last is refused for SDO and network routes alike, and nothing changes.
    CHECK(!r->services.configure(uint8_t(N), route(N + 1), true, true));
    C::NetworkRoute network;
    network.binding = binding(N + 1);
    network.generation = 11;
    network.heartbeatId = 0x700 + N + 1;
    network.emcyId = 0x80 + N + 1;
    CHECK(!r->services.configureNetwork(uint8_t(N), network, C::NetworkObserver(), 0));
    CHECK(!r->services.channel(uint8_t(N)) && !r->services.node(uint8_t(N)) && !r->services.port(uint8_t(N)));
    CHECK(r->services.unresolved(uint8_t(N))); // An invalid slot never reads as usable.
    // A channel never rebinds: the existing slots refuse a second route.
    CHECK(!r->services.configure(0, route(N + 1), true, true));
    O::SequenceId ids[N];
    for (unsigned node = 1; node <= unsigned(N); ++node) {
        CHECK(r->owner.beginSequence(binding(node), uint8_t(node), r->now, ids[node - 1]));
        CHECK(ids[node - 1].slot == node - 1);
        CHECK(r->owner.sequence(ids[node - 1])->producer == node);
    }
    CHECK(r->owner.resources().reservations == unsigned(N));
    // A second reservation of any binding is refused, and so is a binding no channel carries.
    for (unsigned node = 1; node <= unsigned(N); ++node) {
        O::SequenceId again;
        CHECK(!r->owner.beginSequence(binding(node), 1, r->now, again));
    }
    O::SequenceId unknown;
    CHECK(!r->owner.beginSequence(binding(N + 1), 1, r->now, unknown));
    CHECK(r->owner.resources().reservations == unsigned(N));
    // Releasing the last reservation frees that binding alone.
    CHECK(r->owner.releaseSequence(ids[N - 1]));
    CHECK(r->owner.resources().reservations == unsigned(N) - 1);
    O::SequenceId again;
    if (N > 1)
        CHECK(!r->owner.beginSequence(binding(1), 1, r->now, again));
    CHECK(r->owner.beginSequence(binding(N), uint8_t(N), r->now, again) && again.slot == N - 1);
}

// 2. With every ordinary slot full, each channel's urgent slot still takes its stop, and the stops
// leave before any queued read. A best-effort stop takes the same per-channel slot.
void urgentBesideFullOrdinarySlots() {
    scenario = "urgent stops with full ordinary slots";
    std::unique_ptr<Rig> r(new Rig);
    O::SequenceId ids[N];
    uint32_t steps[N] = {};
    reserveAll(*r, ids, r->now + LEAD);
    fillOrdinary(*r, ids, steps, r->now + LEAD);
    for (unsigned c = 0; c < unsigned(N); ++c) {
        O::RequestId refused;
        CHECK(r->owner.admit(upload(ids[c], c + 1, 1, steps[c] + 1, r->now + LEAD), r->now, refused) ==
              O::BusAdmission::RESULTS_FULL);
    }
    CHECK(r->owner.stats().resultsRejected == unsigned(N));
    // Each axis starts its stop operation, which retires its own unsent reads, and admits the stop.
    O::RequestId urgent[N];
    for (unsigned c = 0; c < unsigned(N); ++c) {
        CHECK(r->owner.beginUrgentOperation(ids[c], 2, r->now + LEAD, r->now));
        O::BusRequest stop;
        stop.sequence = ids[c];
        stop.transfer = stopTransfer(c + 1, 2, 1, r->now + LEAD);
        CHECK(r->owner.admitUrgent(stop, r->now, urgent[c]) == O::BusAdmission::ACCEPTED);
        CHECK(urgent[c].slot == unsigned(REQUESTS) + c);
    }
    // Retired reads keep their slots until read, so ordinary work is still refused.
    O::RequestId refused;
    CHECK(r->owner.admit(upload(ids[0], 1, 2, 2, r->now + LEAD), r->now, refused) ==
          O::BusAdmission::RESULTS_FULL);
    // One urgent slot per channel: a second urgent request or a best-effort stop must wait.
    O::BusRequest second;
    second.sequence = ids[0];
    second.transfer = stopTransfer(1, 2, 2, r->now + LEAD);
    CHECK(r->owner.admitUrgent(second, r->now, refused) == O::BusAdmission::RESULTS_FULL);
    CHECK(r->owner.admitBestEffortStop(stopTransfer(1, 901, 901, r->now + LEAD), r->now, refused) ==
          O::BusAdmission::RESULTS_FULL);
    const O::OwnerResources held = r->owner.resources();
    CHECK(held.queued == unsigned(N) && held.retained == unsigned(REQUESTS) && held.reservations == unsigned(N));
    for (unsigned i = 0; i < 4 * unsigned(N) + 8 && r->wire.sent.size() < unsigned(N); ++i)
        r->ownerTurn();
    // Exactly one stop per node and nothing else: the retired reads never reach the wire.
    CHECK(r->wire.sent.size() == unsigned(N));
    stopsFirst(r->wire);
    for (unsigned i = 0; i < 8; ++i)
        r->ownerTurn();
    CHECK(r->wire.sent.size() == unsigned(N));
    for (unsigned c = 0; c < unsigned(N); ++c) {
        const O::Completion* done = r->owner.result(urgent[c]);
        CHECK(done && done->service.outcome == C::SdoOutcome::SUCCESS);
        CHECK(r->owner.release(urgent[c]));
    }
}

void bestEffortPerChannel() {
    scenario = "best-effort stop per channel";
    std::unique_ptr<Rig> r(new Rig);
    O::SequenceId ids[N];
    uint32_t steps[N] = {};
    reserveAll(*r, ids, r->now + LEAD);
    fillOrdinary(*r, ids, steps, r->now + LEAD);
    O::RequestId stops[N];
    for (unsigned c = 0; c < unsigned(N); ++c) {
        CHECK(r->owner.admitBestEffortStop(stopTransfer(c + 1, 901, 901, r->now + LEAD), r->now, stops[c]) ==
              O::BusAdmission::ACCEPTED);
        CHECK(stops[c].slot == unsigned(REQUESTS) + c);
        O::RequestId again;
        CHECK(r->owner.admitBestEffortStop(stopTransfer(c + 1, 902, 902, r->now + LEAD), r->now, again) ==
              O::BusAdmission::RESULTS_FULL);
    }
    for (unsigned i = 0; i < 4 * unsigned(N) + 8 && r->wire.sent.size() < unsigned(N); ++i)
        r->ownerTurn();
    // The stops go first, one per node; no reply can qualify one, so every route ends unresolved.
    CHECK(r->wire.sent.size() >= unsigned(N));
    stopsFirst(r->wire);
    for (unsigned i = 0; i < 8; ++i)
        r->ownerTurn();
    for (unsigned c = 0; c < unsigned(N); ++c) {
        CHECK(r->owner.result(stops[c]) && r->services.unresolved(uint8_t(c)));
        CHECK(r->wire.words(c + 1, 0, 0x000b) == 1);
    }
}

// 3. Producers rotate across every binding: one binding's backlog never delays the others.
void producersRotate() {
    scenario = "producer rotation";
    std::unique_ptr<Rig> r(new Rig);
    // Producer identities are 1..PRODUCERS: zero and PRODUCERS + 1 are refused, the last is taken.
    O::SequenceId refused;
    CHECK(!r->owner.beginSequence(binding(1), 0, r->now, refused));
    CHECK(!r->owner.beginSequence(binding(1), uint8_t(O::CanBusOwner::PRODUCERS + 1), r->now, refused));
    CHECK(!r->owner.resources().reservations);
    CHECK(r->owner.beginSequence(binding(1), uint8_t(O::CanBusOwner::PRODUCERS), r->now, refused));
    CHECK(r->owner.sequence(refused)->producer == unsigned(O::CanBusOwner::PRODUCERS));
    CHECK(r->owner.releaseSequence(refused));
    O::SequenceId ids[N];
    reserveAll(*r, ids, r->now + LEAD);
    // Binding 1 queues every slot the others leave; each other binding queues one read.
    const unsigned backlog = unsigned(REQUESTS) - (unsigned(N) - 1);
    O::RequestId id;
    for (unsigned k = 1; k <= backlog; ++k)
        CHECK(r->owner.admit(upload(ids[0], 1, 1, k, r->now + LEAD), r->now, id) == O::BusAdmission::ACCEPTED);
    for (unsigned node = 2; node <= unsigned(N); ++node)
        CHECK(r->owner.admit(upload(ids[node - 1], node, 1, 1, r->now + LEAD), r->now, id) ==
              O::BusAdmission::ACCEPTED);
    for (unsigned i = 0; i < 8 * unsigned(REQUESTS) && r->wire.sent.size() < unsigned(REQUESTS); ++i)
        r->ownerTurn();
    CHECK(r->wire.sent.size() == unsigned(REQUESTS));
    // Producer order 1..N first, then binding 1's remaining reads.
    for (unsigned i = 0; i < unsigned(REQUESTS); ++i)
        CHECK(nodeOf(r->wire.sent[i].frame) == (i < unsigned(N) ? i + 1 : 1u));
}

// 4. The TWAI adapter holds one frame per channel plus the NMT frame, and refuses one more.
struct IdleSdk {
    bool live = false;
    static bool start(void* p, const E::TwaiConfig&, uint32_t) {
        static_cast<IdleSdk*>(p)->live = true;
        return true;
    }
    static bool stop(void* p) {
        static_cast<IdleSdk*>(p)->live = false;
        return true;
    }
    static E::TwaiSubmit submit(void*, const M::Frame&, uint32_t) { return E::TwaiSubmit::DEFERRED; }
    static E::TwaiEvents events(void*) { return E::TwaiEvents(); }
    static bool receive(void*, O::ReceivedFrame&, uint64_t) { return false; }
    E::TwaiDriver driver() {
        E::TwaiDriver value;
        value.context = this;
        value.start = start;
        value.stop = stop;
        value.submit = submit;
        value.events = events;
        value.receive = receive;
        return value;
    }
};
void twaiSlots() {
    scenario = "TWAI TX slots";
    std::unique_ptr<C::SdoClient> services(new C::SdoClient);
    IdleSdk sdk;
    E::TwaiAdapter adapter(*services, sdk.driver());
    E::TwaiConfig config;
    config.txPin = 4;
    config.rxPin = 5;
    config.bitrate = 250000;
    config.sdkTxQueueDepth = 1;
    config.rxQueueDepth = 16;
    CHECK(adapter.start(config));
    const O::Transport t = adapter.transport();
    M::Frame frame = OwnerRig::frame(0x123, 2);
    for (unsigned i = 1; i <= unsigned(E::TwaiAdapter::TX_CAPACITY) + 1; ++i) {
        C::SdoTxId id;
        id.request.owner = services.get();
        id.request.generation = i;
        id.frame = 1;
        CHECK(t.transmit(t.context, frame, O::TransportTxId::fromSdo(id), 1) ==
              (i <= unsigned(E::TwaiAdapter::TX_CAPACITY)));
    }
    CHECK(adapter.shutdown());
}

// 5. ProbeApp opens one session per channel and refuses another; a closed session's entry is reused.
void probeAppSessions() {
    scenario = "ProbeApp sessions";
    std::unique_ptr<Rig> r(new Rig);
    E::ProbePlatform platform((E::ProbeCallbacks()));
    std::unique_ptr<E::ProbeApp> app(new E::ProbeApp(r->owner, platform));
    for (unsigned node = 1; node <= unsigned(N); ++node) {
        E::AxisSession& s = r->axis(node).a;
        CHECK(app->openAxis(s, r->current(node), L::SettingsModel::CL86_C, r->now) == &s);
        CHECK(s.isOpen() && app->axis(binding(node)) == &s);
    }
    CHECK(r->owner.resources().reservations == unsigned(N));
    // Every channel carries an open session: one more session object finds no binding it may take.
    std::unique_ptr<E::AxisSession> extra(new E::AxisSession(r->owner));
    for (unsigned node = 1; node <= unsigned(N); ++node)
        CHECK(!app->openAxis(*extra, r->current(node), L::SettingsModel::CL86_C, r->now) && !extra->isOpen());
    CHECK(r->owner.resources().reservations == unsigned(N));
    // Closing the last session frees its entry and reservation; the extra session then takes both.
    CHECK(r->axis(N).a.close());
    CHECK(!app->axis(binding(N)));
    CHECK(app->openAxis(*extra, r->current(N), L::SettingsModel::CL86_C, r->now) == extra.get());
    CHECK(app->axis(binding(N)) == extra.get());
    if (N > 1)
        CHECK(app->axis(binding(1)) == &r->axis(1).a);
    CHECK(extra->close());
}

// 6. Discovery of every channel runs as several plans of at most DiscoveryPlan::CAPACITY targets.
void discoveryBatches() {
    scenario = "discovery batches";
    std::unique_ptr<Rig> r(new Rig);
    E::ProbePlatform platform((E::ProbeCallbacks()));
    std::unique_ptr<E::ProbeApp> app(new E::ProbeApp(r->owner, platform));
    std::unique_ptr<E::DiscoveryScan> scan(new E::DiscoveryScan(*app, r->owner, r->services));
    E::DiscoveryInterface actual;
    actual.bitrate = 500000;
    E::DiscoveryPlan plan;
    plan.interface = actual;
    plan.mode = E::DiscoveryMode::PROBE;
    plan.options = reads();
    plan.maximumAgeUs = 1000000;
    plan.serviceBudget = OwnerRig::budget();
    // A plan larger than its storage is refused before anything starts.
    plan.count = uint8_t(E::DiscoveryPlan::CAPACITY + 1);
    plan.maximumServices = plan.count;
    for (unsigned i = 0; i < unsigned(E::DiscoveryPlan::CAPACITY); ++i)
        plan.targets[i].target = r->current(i + 1);
    CHECK(!scan->begin(plan, actual, r->now, r->now + LEAD) && r->wire.sent.empty());
    unsigned batches = 0;
    for (unsigned first = 1; first <= unsigned(N); first += unsigned(E::DiscoveryPlan::CAPACITY)) {
        const unsigned count = unsigned(N) + 1 - first < unsigned(E::DiscoveryPlan::CAPACITY)
                                   ? unsigned(N) + 1 - first
                                   : unsigned(E::DiscoveryPlan::CAPACITY);
        plan.count = uint8_t(count);
        plan.maximumServices = uint8_t(count);
        for (unsigned i = 0; i < count; ++i)
            plan.targets[i].target = r->current(first + i);
        CHECK(scan->begin(plan, actual, r->now, r->now + LEAD));
        for (unsigned k = 0; k < 400 && scan->active(); ++k) {
            r->now += TICK;
            r->wire.deliver(r->now);
            CHECK(scan->service(actual, r->now));
        }
        CHECK(scan->complete() && scan->summary().completed == count);
        CHECK(scan->summary().occupiedSlotsAtStart == unsigned(N) && !scan->summary().freeSlotsAtStart);
        for (unsigned i = 0; i < count; ++i) {
            const E::DiscoveryFinding* f = scan->result(uint8_t(i));
            CHECK(f && f->slot == first + i - 1 && f->outcome == E::DiscoveryOutcome::PRESENCE);
        }
        CHECK(scan->release());
        ++batches;
    }
    CHECK(batches == (unsigned(N) + unsigned(E::DiscoveryPlan::CAPACITY) - 1) / unsigned(E::DiscoveryPlan::CAPACITY));
    // One probe per node, each in its own batch, and only reads.
    CHECK(r->wire.sent.size() == unsigned(N));
    for (unsigned i = 0; i < unsigned(N); ++i)
        CHECK(nodeOf(r->wire.sent[i].frame) == i + 1 && r->wire.sent[i].frame.data[0] == 0x40);
}

// 7. N relative moves at once through one owner, and a HALT of one axis while the others move. The
// halted axis stops on its own stop evidence; every other move keeps its words and completes.
void concurrentMovesWithOneHalt() {
    scenario = "concurrent moves, one halt";
    std::unique_ptr<Rig> r(new Rig);
    const unsigned halted = (unsigned(N) + 1) / 2;
    for (unsigned node = 1; node <= unsigned(N); ++node)
        r->drive(node).travelReads = node == halted ? 1000000 : 6;
    for (unsigned node = 1; node <= unsigned(N); ++node)
        r->ready(node);
    // Fresh admission reads: the first axes' reads aged while the later ones were made ready.
    for (unsigned node = 1; node <= unsigned(N); ++node)
        r->readState(node);
    const size_t start = r->wire.sent.size();
    for (unsigned node = 1; node <= unsigned(N); ++node)
        CHECK(r->relative(node, node % 2 ? 4000 : -4000).isOk());
    for (unsigned k = 0; k < 400 * unsigned(N) && r->axis(halted).a.moveAction().observations < 1; ++k)
        r->turn();
    CHECK(r->axis(halted).a.moveAction().observations >= 1);
    // Every move is still under way when the stop is requested.
    for (unsigned node = 1; node <= unsigned(N); ++node)
        CHECK(r->axis(node).a.moveActive());
    const size_t at = r->wire.sent.size();
    E::StopPolicy policy;
    policy.options.service = reads();
    policy.options.observationIntervalUs = 1000;
    policy.options.maximumAgeUs = 5000000;
    policy.deadlineUs = r->now + LEAD;
    policy.bestEffortDeadlineUs = r->now + LEAD + 1000000;
    E::AxisSession& stopping = r->axis(halted).a;
    CHECK(stopping.requestStop(L::StopKind::HALT, stopping.moveObservation(), policy, r->now) ==
          E::StopPath::CONFIRMED);
    bool busy = true;
    for (unsigned k = 0; k < 2000 * unsigned(N) && busy; ++k) {
        r->turn();
        busy = false;
        for (unsigned node = 1; node <= unsigned(N); ++node)
            busy |= r->axis(node).a.busy() || r->axis(node).a.moveActive() || r->axis(node).a.stopActive();
    }
    CHECK(!busy);
    const L::ActionContext& stop = stopping.stopAction(L::StopKind::HALT);
    CHECK(stop.state == M::ActionState::SUCCEEDED && stop.operation > stopping.moveAction().operation);
    CHECK(stopping.moveAction().interruption == M::MoveInterruption::STOP && stopping.moveFenced());
    CHECK(r->wire.words(halted, at, 0x010f) == 1 && !r->wire.words(halted, start, 0x000b));
    CHECK(r->wire.words(halted, start, 0x005f) == 1 && r->drive(halted).edges == 1);
    for (unsigned node = 1; node <= unsigned(N); ++node) {
        if (node == halted)
            continue;
        const L::MoveContext& c = r->axis(node).a.moveAction();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
        CHECK(c.interruption == M::MoveInterruption::NONE && !c.uncertain && !r->axis(node).a.moveFenced());
        CHECK(c.completed.position == (node % 2 ? 4000 : -4000));
        CHECK(r->drive(node).edges == 1 && r->wire.words(node, start, 0x005f) == 1);
        CHECK(!r->wire.words(node, start, 0x010f) && !r->wire.words(node, start, 0x000b));
        CHECK(!r->wire.words(node, start, 0x001f));
    }
}

// 8. Host time of one owner service() turn with every ordinary and urgent slot queued and every
// channel holding one unanswered exchange: the most the owner's scans can find. Printed only.
void turnCost() {
    scenario = "turn cost";
    std::unique_ptr<Rig> r(new Rig);
    r->wire.answer = false;
    const uint32_t forever = 3000000000u; // No queue or response cutoff falls inside the run.
    O::SequenceId ids[N];
    uint32_t steps[N] = {};
    reserveAll(*r, ids, r->now + 4000000000ull);
    fillOrdinary(*r, ids, steps, r->now + 4000000000ull, forever, forever);
    for (unsigned c = 0; c < unsigned(N); ++c) {
        O::BusRequest urgent = upload(ids[c], c + 1, 1, ++steps[c], r->now + 4000000000ull, forever, forever);
        O::RequestId id;
        CHECK(r->owner.admitUrgent(urgent, r->now, id) == O::BusAdmission::ACCEPTED);
    }
    CHECK(r->owner.resources().queued == unsigned(TOTAL));
    typedef std::chrono::steady_clock Clock;
    const unsigned TURNS = 4000;
    double total = 0, steady = 0, worst = 0;
    unsigned steadyTurns = 0;
    for (unsigned turn = 0; turn < TURNS; ++turn) {
        r->now += TICK;
        const Clock::time_point begin = Clock::now();
        CHECK(r->owner.service(r->now, OwnerRig::budget(8, 2, 8, 2, 1)));
        const double us = std::chrono::duration<double, std::micro>(Clock::now() - begin).count();
        total += us;
        worst = us > worst ? us : worst;
        if (r->owner.resources().active == unsigned(N)) {
            steady += us;
            ++steadyTurns;
        }
    }
    // Two starts per turn until every channel holds one exchange, then nothing more can start.
    const O::OwnerResources end = r->owner.resources();
    CHECK(end.active == unsigned(N) && end.queued == unsigned(TOTAL) - unsigned(N) && !end.retained);
    CHECK(r->wire.sent.size() == unsigned(N) && steadyTurns > 0);
    std::printf("capacity N=%u: owner service() with %u slots queued and %u exchanges in flight: "
                "mean %.2f us, steady mean %.2f us, max %.2f us over %u host turns (informational)\n",
                unsigned(N), unsigned(TOTAL), unsigned(N), total / TURNS, steady / steadyTurns, worst, TURNS);
}
} // namespace

int main() {
    routesAndReservations();
    urgentBesideFullOrdinarySlots();
    bestEffortPerChannel();
    producersRotate();
    twaiSlots();
    probeAppSessions();
    discoveryBatches();
    concurrentMovesWithOneHalt();
    turnCost();
    std::printf("capacity N=%u: %u checks\n", unsigned(N), checks);
    return 0;
}
