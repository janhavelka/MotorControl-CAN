// SPDX-License-Identifier: MIT
// Bounded fake controller/server evidence exercises the ordinary owner path.
// Wire replies are literal expedited SDO forms, not physical qualification.
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
C::SdoChannel route(unsigned node = 1) {
    C::SdoChannel c;
    c.binding.logicalAxisId = 100 + node;
    c.binding.endpoint.profile = 41;
    c.binding.endpoint.address = node;
    c.binding.bindingGeneration = 3;
    c.binding.configurationGeneration = 7;
    c.clientToServerId = 0x600 + node;
    c.serverToClientId = 0x580 + node;
    return c;
}
L::ReadTarget target(unsigned node = 1) {
    L::ReadTarget t;
    t.channel = route(node);
    t.nodeGeneration = 11;
    return t;
}
M::Frame frame(uint32_t id, uint8_t size = 8) {
    M::Frame f;
    f.id = id;
    f.dlc = f.payloadLength = size;
    return f;
}
M::Frame response(unsigned node, uint16_t index, uint8_t width, uint32_t raw = 0,
                  bool download = false, uint8_t subindex = 0) {
    M::Frame f = frame(0x580 + node);
    f.data[0] = download ? 0x60 : width == 1 ? 0x4f : width == 2 ? 0x4b : 0x43;
    f.data[1] = uint8_t(index);
    f.data[2] = uint8_t(index >> 8);
    f.data[3] = subindex;
    CHECK(download || width == 1 || width == 2 || width == 4);
    if (!download)
        for (unsigned i = 0; i < width && i < 4; ++i)
            f.data[4 + i] = uint8_t(raw >> (i * 8));
    return f;
}
E::ServiceBudget budget(uint8_t rx = 8, uint8_t tx = 2, uint8_t events = 8, uint8_t starts = 2) {
    E::ServiceBudget b;
    b.rx = rx;
    b.tx = tx;
    b.events = events;
    b.starts = starts;
    return b;
}
bool same(E::RequestId a, E::RequestId b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
struct Fake {
    enum { CAPACITY = 64 };
    E::ReceivedFrame incoming[CAPACITY];
    E::TxReport reports[CAPACITY];
    M::Frame sent[CAPACITY];
    C::SdoTxId sentId[CAPACITY];
    unsigned rxRead = 0, rxWrite = 0, reportRead = 0, reportWrite = 0, sentCount = 0;
    unsigned receives = 0, transmits = 0, reportCalls = 0, observations = 0, guards = 0;
    uint64_t now = 0;
    bool autoConfirm = true, accept = true, reenter = false, rejectGuard = false;
    E::CanBusOwner* owner = nullptr;
    C::SdoClient* services = nullptr;
    E::RequestId retained;
    L::DriverSettingsContext* settings = nullptr;
    L::SettingsReservation reservation;
    M::Binding current;
    void recursiveCall() {
        if (!reenter)
            return;
        CHECK(owner && !owner->service(now, E::ServiceBudget()));
        if (retained.owner)
            CHECK(!owner->release(retained));
    }
    static bool receive(void* p, E::ReceivedFrame& out) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.receives;
        f.recursiveCall();
        if (f.rxRead == f.rxWrite)
            return false;
        out = f.incoming[f.rxRead++];
        return true;
    }
    static bool transmit(void* p, const M::Frame& value, C::SdoTxId id) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.transmits;
        f.recursiveCall();
        CHECK(f.sentCount < CAPACITY);
        f.sent[f.sentCount] = value;
        f.sentId[f.sentCount++] = id;
        if (f.accept && f.autoConfirm) {
            CHECK(f.reportWrite < CAPACITY);
            f.reports[f.reportWrite].id = id;
            f.reports[f.reportWrite++].transmitted = true;
        }
        return f.accept;
    }
    static bool report(void* p, E::TxReport& out) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.reportCalls;
        f.recursiveCall();
        if (f.reportRead == f.reportWrite)
            return false;
        out = f.reports[f.reportRead++];
        return true;
    }
    static bool guard(void* p, E::SequenceId id, const C::SdoTransfer& work, uint64_t now) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.guards;
        f.recursiveCall();
        CHECK(id.owner == f.owner && work.direction == C::SdoDirection::DOWNLOAD);
        if (f.rejectGuard)
            return false;
        if (f.settings) {
            CHECK(id.generation == f.reservation.generation);
            CHECK(uint32_t(id.slot + 1) == f.reservation.sequence);
            CHECK(L::noteDriverTransmission(*f.settings, f.reservation, f.current, now));
        }
        return true;
    }
    static void observe(void* p, const E::ReceivedFrame& value, uint64_t) {
        Fake& f = *static_cast<Fake*>(p);
        ++f.observations;
        f.recursiveCall();
        // Observer copies follow authoritative dispatch, including passive traffic.
        if (value.frame.id == 0x701 && value.frame.data[0] == 5)
            CHECK(f.services->node(0)->nmt == C::NmtState::OPERATIONAL);
    }
    E::Transport transport() {
        E::Transport t;
        t.context = this;
        t.receive = receive;
        t.transmit = transmit;
        t.takeTxReport = report;
        t.beforeTransmit = guard;
        t.observe = observe;
        return t;
    }
    void push(const M::Frame& value, uint64_t captured) {
        CHECK(rxWrite < CAPACITY);
        incoming[rxWrite].frame = value;
        incoming[rxWrite].receivedTimeKnown = true;
        incoming[rxWrite++].receivedUs = captured;
    }
};
struct Rig {
    C::SdoClient services;
    Fake fake;
    E::CanBusOwner owner;
    uint64_t now = 0;
    explicit Rig(uint32_t seed = 0, bool sharedPeer = false, bool distinctLogicalAxis = false)
        : owner(services, fake.transport(), 73, seed) {
        fake.owner = &owner;
        fake.services = &services;
        fake.current = route().binding;
        for (uint8_t i = 0; i < 2; ++i) {
            C::SdoChannel selected = route(i + 1);
            if (sharedPeer && i == 1) {
                // A synthetic server's explicitly distinct SDO channel, not a
                // claimed second CL86-C channel. It shares the physical node.
                selected = route();
                selected.binding.endpoint.channel = 1;
                if (distinctLogicalAxis)
                    selected.binding.logicalAxisId = 202;
                selected.clientToServerId = 0x641;
                selected.serverToClientId = 0x5c1;
            }
            CHECK(services.configure(i, selected, true, true));
            if (sharedPeer && i == 1)
                continue;
            C::NetworkRoute n;
            n.binding = route(i + 1).binding;
            n.generation = 11;
            n.heartbeatId = 0x701 + i;
            n.emcyId = 0x81 + i;
            CHECK(services.configureNetwork(i, n, C::NetworkObserver(), 0));
        }
        if (sharedPeer)
            CHECK(services.node(0) == services.node(1));
    }
    void turn(uint64_t time, E::ServiceBudget b = budget()) {
        now = fake.now = time;
        CHECK(owner.service(now, b));
    }
    E::SequenceId sequence(unsigned node = 1, uint8_t producer = 1, uint32_t operation = 1,
                           uint64_t deadline = 1000) {
        E::SequenceId id;
        CHECK(owner.beginSequence(route(node).binding, producer, now, id));
        CHECK(owner.beginOperation(id, operation, deadline, now));
        return id;
    }
    E::BusRequest request(E::SequenceId sequence, uint32_t step = 1, uint16_t index = 0x6041,
                          bool download = false) {
        const E::Sequence* s = owner.sequence(sequence);
        CHECK(s);
        E::BusRequest r;
        r.sequence = sequence;
        r.transfer.channel = route(s->binding.endpoint.address);
        r.transfer.channel.binding = s->binding;
        r.transfer.operation = r.transfer.request = s->operation;
        r.transfer.step = step;
        r.transfer.index = index;
        r.transfer.size = 2;
        r.transfer.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
        r.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
        r.transfer.queueTimeoutUs = r.transfer.responseTimeoutUs = 100;
        r.transfer.deadlineUs = s->deadlineUs;
        r.transfer.data[0] = 0x34;
        r.transfer.data[1] = 0x12;
        return r;
    }
    E::RequestId admit(const E::BusRequest& r) {
        E::RequestId id;
        CHECK(owner.admit(r, now, id) == E::BusAdmission::ACCEPTED);
        CHECK(!owner.result(id) && !owner.release(id));
        return id;
    }
    E::RequestId admit(E::SequenceId sequence, const C::SdoTransfer& work) {
        E::BusRequest r;
        r.sequence = sequence;
        r.transfer = work;
        return admit(r);
    }
    const E::Completion& done(E::RequestId id) {
        const E::Completion* c = owner.result(id);
        CHECK(c && same(c->id, id));
        return *c;
    }
    void respond(E::RequestId id, unsigned node, uint16_t index, uint32_t raw,
                 bool download = false, uint8_t width = 2, uint8_t subindex = 0) {
        fake.push(response(node, index, width, raw, download, subindex), now + 1);
        turn(now + 1, budget(8, 0, 8, 0));
        CHECK(done(id).reason == E::CompletionReason::SERVICE);
        CHECK(done(id).service.outcome == C::SdoOutcome::SUCCESS);
    }
    C::SdoCompletion exchange(E::SequenceId sequence, const C::SdoTransfer& work, uint32_t raw) {
        const unsigned sent = fake.sentCount;
        const E::RequestId id = admit(sequence, work);
        turn(now);
        CHECK(fake.sentCount == sent + 1);
        const M::Frame& tx = fake.sent[sent];
        CHECK(tx.id == work.channel.clientToServerId && tx.data[1] == uint8_t(work.index) &&
              tx.data[2] == uint8_t(work.index >> 8) && tx.data[3] == work.subindex);
        if (work.direction == C::SdoDirection::UPLOAD) {
            CHECK(tx.data[0] == 0x40);
            for (unsigned i = 4; i < 8; ++i)
                CHECK(tx.data[i] == 0);
        } else {
            CHECK(tx.data[0] == 0x2b && tx.data[4] == 2 && tx.data[5] == 0);
        }
        respond(id, work.channel.binding.endpoint.address, work.index, raw,
                work.direction == C::SdoDirection::DOWNLOAD, work.size, work.subindex);
        const C::SdoCompletion copy = done(id).service;
        // The bridge terminal has already been copied/released, without consuming ours.
        CHECK(!services.result(fake.sentId[sent].request));
        CHECK(owner.result(id) == &done(id));
        CHECK(owner.release(id));
        return copy;
    }
};

void copiedAdmissionAndRetainedPressure() {
    Rig r;
    const auto sequence = r.sequence();
    E::RequestId ids[E::CanBusOwner::REQUESTS];
    E::BusRequest first = r.request(sequence, 1, 0x2500, true);
    ids[0] = r.admit(first);
    first.transfer.data[0] = 0xee;
    first.transfer.index = 0x1010;
    first.transfer.deadlineUs = 1;
    for (unsigned i = 1; i < E::CanBusOwner::REQUESTS; ++i)
        ids[i] = r.admit(r.request(sequence, i + 1));
    E::RequestId rejected;
    rejected.generation = 999;
    CHECK(r.owner.admit(r.request(sequence, 7), 0, rejected) == E::BusAdmission::RESULTS_FULL);
    CHECK(rejected.generation == 999 && r.fake.sentCount == 0);
    CHECK(!r.owner.releaseSequence(sequence));
    r.turn(0, budget(0, 1, 0, 1));
    const uint8_t literal[] = {0x2b, 0x00, 0x25, 0, 0x34, 0x12, 0, 0};
    CHECK(r.fake.sentCount == 1 && !std::memcmp(r.fake.sent[0].data, literal, 8));
    r.respond(ids[0], 1, 0x2500, 0, true);
    const E::Completion* retained = r.owner.result(ids[0]);
    CHECK(retained->service.transfer.deadlineUs == 1000);
    CHECK(retained->service.transfer.data[0] == 0x34 && retained->controllerAccepted);
    const Saved<E::Completion> saved(*retained);
    for (unsigned i = 1; i < E::CanBusOwner::REQUESTS; ++i) {
        r.turn(r.now, budget(0, 1, 8, 1));
        CHECK(r.fake.sentCount == i + 1);
        r.respond(ids[i], 1, 0x6041, 0x1200 + i);
    }
    saved.unchanged(*retained);
    CHECK(r.owner.admit(r.request(sequence, 7), r.now, rejected) == E::BusAdmission::RESULTS_FULL);
    CHECK(!r.owner.beginOperation(sequence, 2, 2000, r.now));
    E::RequestId foreign = ids[0];
    Rig other;
    foreign.owner = &other.owner;
    CHECK(!r.owner.result(foreign) && !r.owner.release(foreign));
    E::RequestId wrong = ids[0];
    ++wrong.generation;
    CHECK(!r.owner.result(wrong) && !r.owner.release(wrong));
    CHECK(r.owner.release(ids[0]) && !r.owner.release(ids[0]));
    const E::RequestId reused = r.admit(r.request(sequence, 7));
    CHECK(reused.slot == ids[0].slot && reused.generation != ids[0].generation);
    CHECK(!r.owner.result(ids[0]) && !r.owner.release(ids[0]));
    r.turn(r.now);
    r.respond(reused, 1, 0x6041, 77);
    CHECK(r.owner.release(reused));
    for (unsigned i = 1; i < E::CanBusOwner::REQUESTS; ++i)
        CHECK(r.owner.release(ids[i]));
    CHECK(r.owner.releaseSequence(sequence));
    CHECK(!r.owner.sequence(sequence) && !r.owner.releaseSequence(sequence));
}

void fairProducersAndBusyChannel() {
    Rig r;
    const auto a = r.sequence(1, 1), b = r.sequence(2, 4);
    E::RequestId ids[4];
    ids[0] = r.admit(r.request(a, 1, 0x2500));
    ids[1] = r.admit(r.request(a, 2, 0x2501));
    ids[2] = r.admit(r.request(b, 1, 0x2500));
    ids[3] = r.admit(r.request(b, 2, 0x2501));
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 1 && r.fake.sent[0].id == 0x601);
    // Node 1 remains active while a different producer/channel gets its turn.
    r.turn(1, budget(0, 1, 8, 1));
    CHECK(r.fake.sentCount == 2 && r.fake.sent[1].id == 0x602);
    CHECK(!r.owner.result(ids[0]));
    r.respond(ids[2], 2, 0x2500, 20);
    r.turn(r.now, budget(0, 1, 8, 1));
    CHECK(r.fake.sentCount == 3 && r.fake.sent[2].id == 0x602 && r.fake.sent[2].data[1] == 1 &&
          r.fake.sent[2].data[2] == 0x25);
    r.respond(ids[3], 2, 0x2501, 21);
    CHECK(!r.owner.result(ids[0]) && !r.owner.result(ids[1]));
    r.respond(ids[0], 1, 0x2500, 10);
    r.turn(r.now, budget(0, 1, 8, 1));
    CHECK(r.fake.sentCount == 4 && r.fake.sent[3].id == 0x601 && r.fake.sent[3].data[1] == 1);
    r.respond(ids[1], 1, 0x2501, 11);
    CHECK(r.done(ids[0]).producer == 1 && r.done(ids[2]).producer == 4);
    CHECK(r.done(ids[0]).service.data[0] == 10 && r.done(ids[2]).service.data[0] == 20);
    // Two ready channels dispatch cyclically and retain FIFO within each producer.
    Rig cycle;
    const auto c = cycle.sequence(1, 1), d = cycle.sequence(2, 2);
    const auto c1 = cycle.admit(cycle.request(c, 1));
    const auto c2 = cycle.admit(cycle.request(c, 2));
    const auto d1 = cycle.admit(cycle.request(d, 1));
    const auto d2 = cycle.admit(cycle.request(d, 2));
    cycle.turn(0, budget(0, 1, 0, 1));
    cycle.respond(c1, 1, 0x6041, 1);
    cycle.turn(cycle.now, budget(0, 1, 8, 1));
    cycle.respond(d1, 2, 0x6041, 2);
    cycle.turn(cycle.now, budget(0, 1, 8, 1));
    cycle.respond(c2, 1, 0x6041, 3);
    cycle.turn(cycle.now, budget(0, 1, 8, 1));
    cycle.respond(d2, 2, 0x6041, 4);
    const uint32_t order[] = {0x601, 0x602, 0x601, 0x602};
    CHECK(cycle.fake.sentCount == 4);
    for (unsigned i = 0; i < 4; ++i)
        CHECK(cycle.fake.sent[i].id == order[i]);
}

void passiveDispatchDuringTransferAndBudgets() {
    Rig r;
    C::PdoMap map;
    CHECK(L::pdoTemplate(C::PdoDirection::TPDO, 2, map));
    C::PdoMapEvidence actual;
    actual.route.binding = route().binding;
    actual.route.generation = 1;
    actual.route.cobId = 0x281;
    CHECK(C::validatePdoMap(map, L::pdoProfile(41), actual.route.layout));
    actual.nodeGeneration = 11;
    actual.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
    actual.evidenceId = 501;
    actual.communicationKnown = true;
    actual.cobIdParameter = 0x281;
    actual.transmissionType = 255;
    CHECK(r.services.configurePdo(0, 0, actual, L::pdoProfile(41), C::PdoObserver(), 0));
    C::PdoQualification proof;
    proof.route = actual.route;
    proof.nodeGeneration = 11;
    proof.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
    proof.evidenceId = 502;
    CHECK(r.services.qualifyPdo(0, proof, 0));
    const auto s = r.sequence();
    const auto id = r.admit(r.request(s));
    r.turn(0, E::ServiceBudget());
    CHECK(r.fake.receives == 0 && r.fake.transmits == 0 && r.fake.reportCalls == 0);
    r.turn(0, budget(0, 0, 0, 2));
    CHECK(r.fake.transmits == 0); // A start requires an explicit TX opportunity.
    r.turn(0, budget(0, 1, 0, 1));
    CHECK(r.fake.sentCount == 1);
    M::Frame heartbeat = frame(0x701, 1);
    heartbeat.data[0] = 5;
    M::Frame emcy = frame(0x81);
    const uint8_t fault[] = {0x10, 0x23, 0x80, 1, 2, 3, 4, 5};
    std::memcpy(emcy.data, fault, 8);
    M::Frame pdo = frame(0x281);
    pdo.data[0] = 0xff;
    pdo.data[1] = pdo.data[2] = pdo.data[3] = 0xff;
    pdo.data[7] = 0x80;
    r.fake.push(heartbeat, 1);
    r.fake.push(emcy, 1);
    r.fake.push(pdo, 1);
    r.turn(1, budget(0, 0, 0, 0));
    CHECK(r.fake.rxRead == 0 && r.fake.observations == 0 && r.fake.reportCalls == 0);
    r.turn(1, budget(1, 0, 0, 0));
    CHECK(r.fake.rxRead == 1 && r.services.node(0)->heartbeat.present);
    r.turn(2, budget(2, 0, 1, 0));
    CHECK(r.fake.rxRead == 3 && r.fake.observations == 3);
    CHECK(r.services.node(0)->emcyCount == 1 && r.services.node(0)->emcy.errorCode == 0x2310);
    const C::PdoRecord* record = r.services.pdo(0);
    CHECK(record && record->qualified && record->observation.sample.values[0].native == -1);
    CHECK(record->observation.sample.values[1].native == -2147483647LL - 1);
    CHECK(!r.owner.result(id));
    r.respond(id, 1, 0x6041, 0x1234);
    CHECK(r.done(id).service.transmissionConfirmed && r.fake.sentCount == 1);
    const Saved<E::Completion> immutable(r.done(id));
    r.fake.retained = id;
    r.fake.reenter = true;
    r.fake.push(heartbeat, r.now + 1);
    r.turn(r.now + 1, budget(1, 0, 1, 0));
    immutable.unchanged(r.done(id));
    r.fake.reenter = false;
    const unsigned calls = r.fake.receives;
    r.turn(r.now, budget(255, 0, 0, 0));
    CHECK(r.fake.receives == calls + 1); // Empty stops the bounded drain immediately.
    CHECK(!r.owner.service(r.now - 1, budget()));
}

void invalidAdmissionSequenceAndIdExhaustion() {
    Rig r;
    const auto s = r.sequence();
    E::SequenceId unchanged;
    unchanged.generation = 999;
    CHECK(!r.owner.beginSequence(route().binding, 2, 0, unchanged));
    CHECK(unchanged.generation == 999);
    const Saved<E::Sequence> saved(*r.owner.sequence(s));
    CHECK(!r.owner.beginOperation(s, 1, 2000, 0));
    saved.unchanged(*r.owner.sequence(s));
    for (unsigned fault = 0; fault < 9; ++fault) {
        E::BusRequest bad = r.request(s);
        switch (fault) {
        case 0:
            bad.sequence.owner = nullptr;
            break;
        case 1:
            ++bad.sequence.generation;
            break;
        case 2:
            ++bad.transfer.operation;
            break;
        case 3:
            ++bad.transfer.deadlineUs;
            break;
        case 4:
            bad.transfer.step = 0;
            break;
        case 5:
            bad.transfer.size = 0;
            break;
        case 6:
            ++bad.transfer.channel.binding.bindingGeneration;
            break;
        case 7:
            bad.transfer.queueTimeoutUs = 0;
            break;
        case 8:
            bad.transfer.responseTimeoutUs = 0;
            break;
        }
        E::RequestId out;
        out.generation = 555;
        CHECK(r.owner.admit(bad, 0, out) == E::BusAdmission::INVALID);
        CHECK(out.generation == 555 && r.fake.sentCount == 0);
        saved.unchanged(*r.owner.sequence(s));
    }
    const auto first = r.admit(r.request(s));
    E::RequestId out;
    out.generation = 555;
    CHECK(r.owner.admit(r.request(s), 0, out) == E::BusAdmission::INVALID);
    CHECK(out.generation == 555 && !r.owner.release(first));
    r.turn(1000, E::ServiceBudget());
    CHECK(r.done(first).reason == E::CompletionReason::DEADLINE);
    CHECK(!r.done(first).service.transmissionPossible && r.fake.sentCount == 0);
    CHECK(r.owner.release(first));
    CHECK(r.owner.beginOperation(s, 2, 2000, r.now));
    Rig exhausted(std::numeric_limits<uint32_t>::max() - 2);
    const auto sequence = exhausted.sequence();
    const auto last = exhausted.admit(exhausted.request(sequence));
    E::RequestId refused;
    refused.generation = 123;
    CHECK(exhausted.owner.admit(exhausted.request(sequence, 2), 0, refused) ==
          E::BusAdmission::IDS_EXHAUSTED);
    CHECK(refused.generation == 123);
    exhausted.turn(0);
    exhausted.respond(last, 1, 0x6041, 42);
    CHECK(exhausted.owner.release(last) && exhausted.owner.releaseSequence(sequence));
    E::SequenceId noWrap;
    noWrap.generation = 321;
    CHECK(!exhausted.owner.beginSequence(route().binding, 1, exhausted.now, noWrap));
    CHECK(noWrap.generation == 321);
}

void queueAgeAndServiceEvidence() {
    Rig r;
    const auto s = r.sequence();
    E::BusRequest a = r.request(s, 1);
    a.transfer.responseTimeoutUs = 500;
    const auto active = r.admit(a);
    r.turn(0);
    E::BusRequest b = r.request(s, 2);
    b.transfer.queueTimeoutUs = 10;
    const auto queued = r.admit(b);
    r.turn(9, E::ServiceBudget());
    CHECK(!r.owner.result(queued));
    r.turn(10, E::ServiceBudget());
    CHECK(r.done(queued).reason == E::CompletionReason::QUEUE_EXPIRED);
    CHECK(r.done(queued).admittedUs == 0 && !r.done(queued).adapterAdmitted &&
          !r.done(queued).controllerAccepted && !r.done(queued).service.transmissionPossible);
    CHECK(r.fake.sentCount == 1 && !r.owner.result(active));
    r.respond(active, 1, 0x6041, 1);
    // First handoff at the original queue cutoff cannot restart its budget.
    Rig equality;
    const auto seq = equality.sequence();
    auto w = equality.request(seq);
    w.transfer.queueTimeoutUs = 10;
    const auto expired = equality.admit(w);
    equality.turn(10);
    CHECK(equality.done(expired).reason == E::CompletionReason::QUEUE_EXPIRED &&
          equality.fake.sentCount == 0);
    Rig lastMoment;
    const auto atLimit = lastMoment.sequence();
    auto limited = lastMoment.request(atLimit);
    limited.transfer.queueTimeoutUs = 10;
    const auto timely = lastMoment.admit(limited);
    lastMoment.turn(9);
    lastMoment.respond(timely, 1, 0x6041, 99);
    CHECK(lastMoment.done(timely).admittedUs == 0 &&
          lastMoment.done(timely).service.admittedUs == 9 &&
          lastMoment.done(timely).service.transfer.queueTimeoutUs == 10);
    Rig late;
    const auto q = late.sequence();
    const auto timeout = late.admit(late.request(q, 1, 0x2500, true));
    const auto nextWrite = late.request(q, 2, 0x2500, true);
    late.fake.autoConfirm = false;
    late.turn(0);
    CHECK(late.fake.sentCount == 1 && !late.owner.result(timeout));
    late.turn(100, E::ServiceBudget());
    const auto* terminal = late.owner.result(timeout);
    CHECK(terminal && terminal->controllerAccepted && terminal->service.transmissionPossible &&
          !terminal->service.transmissionConfirmed && !terminal->service.protocolAcknowledged);
    CHECK(terminal->service.outcome == C::SdoOutcome::TIMEOUT && terminal->service.effectUncertain);
    const Saved<E::Completion> saved(*terminal);
    late.fake.push(response(1, 0x2500, 2, 0, true), 101);
    late.turn(101, budget(1, 0, 0, 0));
    saved.unchanged(*terminal);
    CHECK(late.services.unresolved(0));
    CHECK(late.owner.release(timeout));
    E::RequestId different;
    different.generation = 777;
    CHECK(late.owner.admit(nextWrite, late.now, different) == E::BusAdmission::INVALID);
    CHECK(different.generation == 777 && late.fake.sentCount == 1);
}

void checkedAbortTxReportsAndGuard() {
    Rig r;
    const auto s = r.sequence();
    const auto id = r.admit(r.request(s));
    r.turn(0);
    M::Frame abort = response(1, 0x6041, 2);
    const uint8_t bytes[] = {0x80, 0x41, 0x60, 0, 0x67, 0x45, 0x23, 0xf1};
    std::memcpy(abort.data, bytes, 8);
    r.fake.push(abort, 1);
    r.turn(1, budget(1, 0, 8, 0));
    const auto& c = r.done(id);
    CHECK(c.service.outcome == C::SdoOutcome::SERVER_ABORT && c.service.abortValid &&
          c.service.abortCode == 0xf1234567u && !c.service.protocolAcknowledged);
    const Saved<E::Completion> retained(c);
    // A stale controller report cannot revise the first service terminal.
    r.fake.reports[r.fake.reportWrite].id = r.fake.sentId[0];
    r.fake.reports[r.fake.reportWrite++].transmitted = false;
    r.turn(2, budget(0, 0, 1, 0));
    retained.unchanged(r.done(id));
    Rig noReport;
    noReport.fake.autoConfirm = false;
    const auto q = noReport.sequence();
    const auto success = noReport.admit(noReport.request(q));
    noReport.turn(0);
    noReport.respond(success, 1, 0x6041, 0xabcd);
    CHECK(noReport.done(success).controllerAccepted &&
          !noReport.done(success).service.transmissionConfirmed &&
          noReport.done(success).service.protocolAcknowledged);
    Rig guard;
    const auto g = guard.sequence();
    const auto withdrawn = guard.admit(guard.request(g, 1, 0x2500, true));
    guard.fake.rejectGuard = true;
    guard.fake.reenter = true;
    guard.turn(0);
    CHECK(guard.done(withdrawn).reason == E::CompletionReason::PREFLIGHT_REJECTED &&
          !guard.done(withdrawn).controllerAccepted && guard.fake.sentCount == 0);
    CHECK(guard.fake.guards == 1 && !guard.done(withdrawn).service.transmissionPossible);
}

void boundedCallbacksAndOneShotTransportFailure() {
    Rig bounded;
    const auto firstSequence = bounded.sequence(1, 1);
    const auto secondSequence = bounded.sequence(2, 2);
    const auto first = bounded.admit(bounded.request(firstSequence));
    const auto second = bounded.admit(bounded.request(secondSequence));
    bounded.fake.reenter = true;
    bounded.turn(0, budget(0, 255, 0, 255));
    CHECK(bounded.fake.transmits == E::CanBusOwner::TX_LIMIT && bounded.fake.sentCount == 2);
    CHECK(!bounded.owner.result(first) && !bounded.owner.result(second));
    // More ingress/report records than one turn can consume remain queued.
    M::Frame unknown = frame(0x321, 1);
    for (unsigned i = 0; i < 10; ++i) {
        bounded.fake.push(unknown, 1);
        CHECK(bounded.fake.reportWrite < Fake::CAPACITY);
        E::TxReport& report = bounded.fake.reports[bounded.fake.reportWrite++];
        report.id.request.owner = &bounded.services;
        report.id.request.channel = 0;
        report.id.request.generation = 999;
        report.id.frame = 999;
        report.transmitted = true;
    }
    bounded.turn(1, budget(255, 0, 255, 0));
    CHECK(bounded.fake.rxRead == E::CanBusOwner::RX_LIMIT &&
          bounded.fake.reportRead == E::CanBusOwner::EVENT_LIMIT);
    CHECK(bounded.fake.observations == E::CanBusOwner::RX_LIMIT);
    bounded.fake.reenter = false;
    bounded.turn(1, budget(255, 0, 255, 0));
    CHECK(bounded.fake.rxRead == 10 && bounded.fake.reportRead == 12);
    bounded.respond(first, 1, 0x6041, 1);
    bounded.respond(second, 2, 0x6041, 2);
    for (unsigned failure = 0; failure < 2; ++failure) {
        Rig r;
        r.fake.autoConfirm = false;
        r.fake.accept = failure != 0;
        const auto s = r.sequence();
        const auto id = r.admit(r.request(s, 1, 0x2500, true));
        r.turn(0);
        if (failure) {
            CHECK(!r.owner.result(id));
            r.fake.reports[0].id = r.fake.sentId[0];
            r.fake.reports[0].transmitted = false;
            r.fake.reportWrite = 1;
            r.turn(1, budget(0, 0, 1, 0));
        }
        const auto& result = r.done(id);
        CHECK(result.service.outcome == C::SdoOutcome::TRANSPORT_ERROR);
        CHECK(result.controllerAccepted == (failure != 0));
        CHECK(result.service.transmissionPossible && !result.service.transmissionConfirmed &&
              result.service.effectUncertain && !result.service.protocolAcknowledged);
        const Saved<E::Completion> original(result);
        r.turn(2, E::ServiceBudget());
        original.unchanged(r.done(id));
        CHECK(r.fake.transmits == 1); // A refused or failed handoff is never replayed.
    }
}

void nodeInvalidationKeepsOtherChannelAndTerminals() {
    Rig r;
    const auto a = r.sequence(1, 1), b = r.sequence(2, 2);
    const auto active = r.admit(r.request(a, 1, 0x2500, true));
    const auto queued = r.admit(r.request(a, 2));
    const auto independent = r.admit(r.request(b));
    r.turn(0);
    CHECK(r.fake.sentCount == 2);
    M::Frame boot = frame(0x701, 1);
    boot.data[0] = 0;
    r.fake.push(boot, 1);
    r.turn(1, budget(1, 0, 8, 0));
    CHECK(r.done(active).service.outcome == C::SdoOutcome::REBOOT &&
          r.done(active).service.effectUncertain);
    CHECK(r.done(queued).reason == E::CompletionReason::INVALIDATED &&
          !r.done(queued).adapterAdmitted && !r.done(queued).service.transmissionPossible);
    CHECK(!r.owner.sequence(a) && r.owner.sequence(b) && !r.owner.result(independent));
    CHECK(r.services.unresolved(0) && !r.services.unresolved(1));
    CHECK(!r.owner.releaseSequence(a));
    const Saved<E::Completion> original(r.done(active));
    r.respond(independent, 2, 0x6041, 0x2345);
    original.unchanged(r.done(active));
    CHECK(r.fake.sentCount == 2);
    CHECK(r.owner.release(active) && r.owner.release(queued));
    CHECK(r.owner.releaseSequence(a));
    E::SequenceId rejected;
    rejected.generation = 789;
    CHECK(!r.owner.beginSequence(route().binding, 1, r.now, rejected));
    CHECK(rejected.generation == 789); // New host reservation cannot reopen the wire route.
    CHECK(r.owner.release(independent) && r.owner.releaseSequence(b));
}

void sharedPeerQuarantineRejectsNewSequenceAtomically() {
    Rig r(0, true);
    M::Frame unsolicited = response(1, 0x2500, 2, 0, true);
    unsolicited.id = 0x5c1;
    r.fake.push(unsolicited, 1);
    r.turn(1, budget(1, 0, 0, 0));
    CHECK(!r.services.unresolved(0) && r.services.unresolved(1));
    E::SequenceId id;
    id.owner = &r.owner;
    id.slot = 1;
    id.generation = 456;
    const Saved<E::SequenceId> unchanged(id);
    CHECK(!r.owner.beginSequence(route().binding, 1, r.now, id));
    unchanged.unchanged(id);
    CHECK(r.fake.sentCount == 0 && r.services.node(0) == r.services.node(1));
}

void sharedPeerQuarantineRetiresActiveBeforeLaterRxOrTx() {
    // Expedited ACK after quarantine, segmented initiation ACK after quarantine,
    // and a staged segment followed by quarantine in the same receive turn.
    for (unsigned phase = 0; phase < 3; ++phase) {
        Rig r(0, true);
        const auto s = r.sequence();
        auto write = r.request(s, 1, 0x2500, true);
        if (phase) {
            write.transfer.size = 10;
            for (unsigned i = 0; i < 10; ++i)
                write.transfer.data[i] = uint8_t(i + 1);
        }
        const auto active = r.admit(write);
        const auto queued = r.admit(r.request(s, 2));
        r.turn(0);
        CHECK(r.fake.sentCount == 1 && r.fake.sent[0].data[0] == (phase ? 0x21 : 0x2b));
        M::Frame unsolicited = response(1, 0x2500, 2, 0, true);
        unsolicited.id = 0x5c1;
        const M::Frame ack = response(1, 0x2500, 2, 0, true);
        r.fake.push(phase == 2 ? ack : unsolicited, 1);
        r.fake.push(phase == 2 ? unsolicited : ack, 1);
        r.turn(1, budget(2, 2, 8, 2));
        const E::Completion& terminal = r.done(active);
        CHECK(terminal.service.outcome == C::SdoOutcome::BINDING_INVALIDATED &&
              terminal.service.effectUncertain && terminal.service.transmissionPossible &&
              !terminal.service.protocolAcknowledged);
        CHECK(r.done(queued).reason == E::CompletionReason::INVALIDATED &&
              !r.done(queued).adapterAdmitted && !r.done(queued).service.transmissionPossible);
        CHECK(!r.owner.sequence(s) && r.services.unresolved(0) && r.services.unresolved(1));
        CHECK(r.fake.sentCount == 1 && !r.services.port(0)->queueOccupied);
        const Saved<E::Completion> immutable(terminal);
        r.fake.push(ack, 2);
        r.turn(2, budget(1, 2, 8, 2));
        immutable.unchanged(r.done(active));
        CHECK(r.fake.sentCount == 1 && r.fake.observations == 3);
        CHECK(r.owner.release(active) && r.owner.release(queued) && r.owner.releaseSequence(s));
    }
}

void staleNegativeReportDoesNotFailNewSegment() {
    Rig r;
    r.fake.autoConfirm = false;
    const auto s = r.sequence();
    auto write = r.request(s, 1, 0x2500, true);
    write.transfer.size = 10;
    for (unsigned i = 0; i < 10; ++i)
        write.transfer.data[i] = uint8_t(i + 1);
    const auto id = r.admit(write);
    r.turn(0);
    CHECK(r.fake.sentCount == 1 && r.fake.sent[0].data[0] == 0x21);
    const C::SdoTxId initiation = r.fake.sentId[0];
    r.fake.push(response(1, 0x2500, 2, 0, true), 1);
    r.turn(1, budget(1, 1, 0, 0));
    CHECK(r.fake.sentCount == 2 && r.fake.sent[1].data[0] == 0x00);
    const uint8_t segment[] = {0, 1, 2, 3, 4, 5, 6, 7};
    CHECK(!std::memcmp(r.fake.sent[1].data, segment, sizeof segment));
    const C::SdoTxId current = r.fake.sentId[1];
    CHECK(initiation.request.generation == current.request.generation && initiation.frame == 1 &&
          current.frame == 2);
    CHECK(r.services.canTransmit(current, r.now));
    r.fake.reports[0].id = initiation;
    r.fake.reports[0].transmitted = false;
    r.fake.reportWrite = 1;
    r.turn(2, budget(0, 0, 1, 0));
    CHECK(!r.owner.result(id) && !r.services.unresolved(0));
    CHECK(r.services.canTransmit(current, r.now) && r.fake.sentCount == 2);
    // The request generation matches both frames. Only the exact current
    // per-frame handoff can fail this segment; a delayed report cannot do so.
    r.fake.reports[1].id = current;
    r.fake.reports[1].transmitted = false;
    r.fake.reportWrite = 2;
    r.turn(3, budget(0, 0, 1, 0));
    const auto& terminal = r.done(id);
    CHECK(terminal.service.outcome == C::SdoOutcome::TRANSPORT_ERROR &&
          terminal.controllerAccepted && terminal.service.effectUncertain &&
          !terminal.service.protocolAcknowledged);
    CHECK(!r.services.canTransmit(current, r.now) && r.fake.sentCount == 2);
    const Saved<E::Completion> retained(terminal);
    r.turn(4, E::ServiceBudget());
    retained.unchanged(r.done(id));
}

void alternateChannelUsesConfiguredBindingAndSharesExclusion() {
    // Two distinct logical bindings on a synthetic physical peer share one
    // network anchor; service routing remains the selected channel's fact.
    Rig r(0, true, true);
    const C::SdoChannel alternate = *r.services.channel(1);
    CHECK(alternate.binding.logicalAxisId == 202 && alternate.binding.endpoint.channel == 1);
    CHECK(!M::sameBinding(alternate.binding, r.services.node(1)->route.binding));
    E::SequenceId selected;
    CHECK(r.owner.beginSequence(alternate.binding, 3, 0, selected));
    const E::Sequence* live = r.owner.sequence(selected);
    CHECK(live && M::sameBinding(live->binding, alternate.binding) && live->nodeGeneration == 11);
    CHECK(r.owner.beginOperation(selected, 1, 1000, 0));
    E::SequenceId competing;
    competing.generation = 987;
    const Saved<E::SequenceId> unchanged(competing);
    CHECK(!r.owner.beginSequence(route().binding, 2, 0, competing));
    unchanged.unchanged(competing);
    auto request = r.request(selected);
    request.transfer.channel = alternate;
    const auto id = r.admit(request);
    r.turn(0);
    CHECK(r.fake.sentCount == 1 && r.fake.sent[0].id == 0x641);
    M::Frame reply = response(1, 0x6041, 2, 0x1234);
    reply.id = 0x5c1;
    r.fake.push(reply, 1);
    r.turn(1, budget(1, 0, 8, 0));
    CHECK(r.done(id).service.outcome == C::SdoOutcome::SUCCESS &&
          r.done(id).service.nodeGeneration == 11 && r.done(id).service.data[0] == 0x34);
    CHECK(M::sameBinding(r.done(id).service.transfer.channel.binding, alternate.binding));
    CHECK(r.owner.sequence(selected) && r.services.node(0) == r.services.node(1));
    CHECK(r.owner.release(id) && r.owner.releaseSequence(selected));
}

L::ReadOptions options() {
    L::ReadOptions o;
    o.queueTimeoutUs = o.responseTimeoutUs = 100;
    return o;
}
void rejectedIngressRemainsDiagnosticOnly() {
    Rig r;
    M::Frame invalid = frame(0x123, 1);
    invalid.fd = true;
    r.fake.push(invalid, 0);
    r.fake.push(frame(0x345, 1), 2); // Future capture must not enter normal routing at time zero.
    r.turn(0, budget(8, 0, 0, 0));
    CHECK(r.fake.observations == 2 && r.fake.transmits == 0);
    CHECK(!r.services.node(0)->heartbeat.present && !r.services.node(1)->heartbeat.present);
    C::NetworkObservation diagnostic;
    CHECK(!r.services.takeNetworkObservation(diagnostic));
    // Protocol-format fixture: valid Classical extended RTR, no payload to decode.
    M::Frame remote;
    remote.id = 0x1234567;
    remote.extended = remote.rtr = true;
    remote.dlc = 8;
    r.fake.push(remote, 1);
    r.turn(1, budget(8, 0, 0, 0));
    CHECK(r.fake.observations == 3 && !r.services.node(0)->heartbeat.present);
}
void publicReadAndConfirmedControlSetting() {
    Rig r;
    const auto s = r.sequence(1, 1, 11);
    L::ReadTarget current = target();
    L::ReadContext state;
    const uint8_t statusMask = uint8_t(1u << static_cast<unsigned>(L::StateBlock::STATUS));
    CHECK(L::prepareState(state, current, 11, 0, 1000, options(), statusMask));
    for (unsigned i = 0; i < 2; ++i) {
        L::PreparedRead p;
        CHECK(L::nextRead(state, r.now, p) && p.ready);
        CHECK(p.transfer.index == (i ? 0x6061 : 0x6041));
        const C::SdoCompletion e = r.exchange(s, p.transfer, i ? 0xff : 0x1234);
        CHECK(L::advanceRead(state, e, r.now));
    }
    L::StateObservation observation;
    CHECK(L::getStateBlock(state, L::StateBlock::STATUS, current, r.now, 100, observation));
    CHECK(state.state == M::ReadState::SUCCEEDED);
    CHECK(r.owner.beginOperation(s, 12, 1000, r.now));
    L::DriverContext prior;
    CHECK(L::prepareControlRead(prior, current, 12, r.now, 1000, options()));
    for (unsigned i = 0; i < 2; ++i) {
        L::PreparedRead p;
        CHECK(L::nextDriver(prior, r.now, p) && p.ready);
        CHECK(p.transfer.index == (i ? 0x2501 : 0x2500));
        const auto e = r.exchange(s, p.transfer, i ? 0xffff : 0);
        CHECK(L::advanceDriver(prior, e, r.now));
    }
    CHECK(prior.read.state == M::ReadState::SUCCEEDED);
    const E::Sequence* live = r.owner.sequence(s);
    CHECK(live && live->acquiredUs == 0);
    r.fake.reservation.target = current;
    r.fake.reservation.model = L::SettingsModel::CL86_C;
    r.fake.reservation.owner = r.owner.identity();
    r.fake.reservation.sequence = uint32_t(s.slot + 1);
    r.fake.reservation.generation = s.generation;
    r.fake.reservation.acquiredUs = live->acquiredUs;
    const Saved<L::SettingsReservation> frozen(r.fake.reservation);
    CHECK(r.owner.beginOperation(s, 13, 1000, r.now));
    L::ControlSettings requested;
    requested.loopMode = L::LoopMode::CLOSED_LOOP;
    L::DriverSettingsContext settings;
    CHECK(L::prepareControlSettings(settings, r.fake.reservation, prior, requested, 13, r.now, 1000,
                                    options(), 100));
    requested.loopMode = L::LoopMode::OPEN_LOOP;
    r.fake.settings = &settings;
    L::PreparedRead p;
    CHECK(L::nextDriver(settings, r.fake.reservation, r.fake.current, r.now, p) && p.ready);
    CHECK(p.transfer.index == 0x2500 && p.transfer.direction == C::SdoDirection::DOWNLOAD &&
          p.transfer.data[0] == 2);
    const auto ack = r.exchange(s, p.transfer, 0);
    CHECK(r.fake.current.configurationGeneration == 8 && settings.effectsPossible);
    CHECK(ack.protocolAcknowledged && ack.transmissionConfirmed);
    CHECK(L::advanceDriver(settings, r.fake.reservation, r.fake.current, ack, r.now));
    CHECK(settings.state == L::SettingsState::READING_BACK && settings.confirmed == 1);
    CHECK(r.owner.advanceConfiguration(s, r.fake.current, r.now));
    CHECK(r.owner.sequence(s)->binding.configurationGeneration == 8);
    r.fake.settings = nullptr;
    current.channel.binding = r.fake.current;
    for (unsigned i = 0; i < 2; ++i) {
        CHECK(L::nextDriver(settings, r.fake.reservation, r.fake.current, r.now, p) && p.ready);
        CHECK(p.transfer.index == (i ? 0x2501 : 0x2500) &&
              p.transfer.channel.binding.configurationGeneration == 8);
        const auto e = r.exchange(s, p.transfer, i ? 0xffff : 2);
        CHECK(L::advanceDriver(settings, r.fake.reservation, r.fake.current, e, r.now));
    }
    L::ControlObservation configured;
    CHECK(L::getControl(settings, r.fake.reservation, current, r.now, 100, configured));
    CHECK(configured.loopMode == L::LoopMode::CLOSED_LOOP &&
          configured.encoderResolutionRaw == 0xffff &&
          settings.state == L::SettingsState::SUCCEEDED);
    frozen.unchanged(r.fake.reservation);
    CHECK(r.fake.reservation.target.channel.binding.configurationGeneration == 7);
    CHECK(r.fake.guards == 1 && r.fake.sentCount == 7);
    CHECK(r.owner.releaseSequence(s));
}

E::SequenceId prepareOwnedControlSetting(Rig& r, L::DriverSettingsContext& settings) {
    const auto sequence = r.sequence(1, 1, 11);
    L::DriverContext prior;
    CHECK(L::prepareControlRead(prior, target(), 11, r.now, 1000, options()));
    for (unsigned i = 0; i < 2; ++i) {
        L::PreparedRead work;
        CHECK(L::nextDriver(prior, r.now, work) && work.ready);
        const auto event = r.exchange(sequence, work.transfer, i ? 0xffff : 0);
        CHECK(L::advanceDriver(prior, event, r.now));
    }
    const E::Sequence* live = r.owner.sequence(sequence);
    CHECK(live);
    r.fake.reservation.target = target();
    r.fake.reservation.model = L::SettingsModel::CL86_C;
    r.fake.reservation.owner = r.owner.identity();
    r.fake.reservation.sequence = uint32_t(sequence.slot + 1);
    r.fake.reservation.generation = sequence.generation;
    r.fake.reservation.acquiredUs = live->acquiredUs;
    CHECK(r.owner.beginOperation(sequence, 12, 1000, r.now));
    L::ControlSettings candidate;
    candidate.loopMode = L::LoopMode::CLOSED_LOOP;
    CHECK(L::prepareControlSettings(settings, r.fake.reservation, prior, candidate, 12, r.now, 1000,
                                    options(), 100));
    r.fake.settings = &settings;
    return sequence;
}

void checkFailedControlRetention(Rig& r, E::SequenceId sequence, E::RequestId failed,
                                 L::DriverSettingsContext& settings, unsigned uploads) {
    L::ReadTarget current = target();
    current.channel.binding = r.fake.current;
    L::ControlObservation output;
    output.loopModeRaw = 0xa55a;
    output.encoderResolutionRaw = 0x5aa5;
    const Saved<L::ControlObservation> oldOutput(output);
    CHECK(!L::getControl(settings, r.fake.reservation, current, r.now, 1000, output));
    oldOutput.unchanged(output);
    const Saved<L::DriverSettingsContext> oldSettings(settings);
    const Saved<E::Completion> retained(r.done(failed));
    CHECK(!r.owner.releaseSequence(sequence));
    // A retained failure and quarantined node cannot starve a different node.
    const auto other = r.sequence(2, 2, 21);
    const auto read = r.admit(r.request(other));
    r.turn(r.now);
    r.respond(read, 2, 0x6041, 0x1234);
    CHECK(r.owner.release(read) && r.owner.releaseSequence(other));
    r.turn(r.now + 1);
    retained.unchanged(r.done(failed));
    oldSettings.unchanged(settings);
    CHECK(!L::getControl(settings, r.fake.reservation, current, r.now, 1000, output));
    oldOutput.unchanged(output);
    CHECK(r.fake.current.configurationGeneration == 8 && r.fake.guards == 1);
    unsigned downloads = 0, reads = 0;
    for (unsigned i = 0; i < r.fake.sentCount; ++i) {
        const M::Frame& sent = r.fake.sent[i];
        if (sent.id != 0x601)
            continue;
        if (sent.data[0] == 0x80)
            continue; // Native abort cleanup is neither a setting nor a replay.
        CHECK(sent.data[2] == 0x25 && sent.data[1] <= 1 && sent.data[3] == 0);
        if (sent.data[0] == 0x40)
            ++reads;
        else {
            CHECK(sent.data[0] == 0x2b && sent.data[1] == 0 && sent.data[4] == 2 &&
                  sent.data[5] == 0);
            ++downloads;
        }
    }
    CHECK(downloads == 1 && reads == uploads);
    CHECK(r.owner.release(failed) && !r.owner.result(failed));
    CHECK(r.owner.releaseSequence(sequence));
}

void actualControlUncertainDownloadAndFailedReadback() {
    {
        Rig r;
        L::DriverSettingsContext settings;
        const auto sequence = prepareOwnedControlSetting(r, settings);
        L::PreparedRead work;
        CHECK(L::nextDriver(settings, r.fake.reservation, r.fake.current, r.now, work) &&
              work.ready);
        const auto id = r.admit(sequence, work.transfer);
        r.fake.rejectGuard = true;
        r.turn(r.now);
        const auto& terminal = r.done(id);
        CHECK(terminal.reason == E::CompletionReason::PREFLIGHT_REJECTED &&
              terminal.service.outcome == C::SdoOutcome::CANCELLED &&
              !terminal.controllerAccepted && !terminal.service.transmissionPossible &&
              !terminal.service.effectPossible && !terminal.service.effectUncertain);
        CHECK(L::advanceDriver(settings, r.fake.reservation, r.fake.current, terminal.service,
                               r.now));
        CHECK(settings.state == L::SettingsState::FAILED &&
              settings.outcome == M::ReadOutcome::CANCELLED && !settings.effectsPossible &&
              !settings.uncertain && !settings.confirmed && !settings.acknowledgedUs[0]);
        CHECK(r.fake.current.configurationGeneration == 7 && r.fake.sentCount == 2 &&
              r.fake.guards == 1 && r.owner.sequence(sequence));
        const Saved<E::Completion> retained(terminal);
        const Saved<L::DriverSettingsContext> oldSettings(settings);
        r.fake.settings = nullptr;
        const auto other = r.sequence(2, 2, 21);
        const auto read = r.admit(r.request(other));
        r.turn(r.now);
        r.respond(read, 2, 0x6041, 0x1234);
        retained.unchanged(r.done(id));
        oldSettings.unchanged(settings);
        CHECK(r.fake.current.configurationGeneration == 7 && r.fake.guards == 1 &&
              r.fake.sentCount == 3 && r.fake.sent[2].id == 0x602);
        CHECK(r.owner.release(read) && r.owner.releaseSequence(other));
        CHECK(r.owner.release(id) && r.owner.releaseSequence(sequence));
    }
    {
        Rig r;
        L::DriverSettingsContext settings;
        const auto sequence = prepareOwnedControlSetting(r, settings);
        L::PreparedRead work;
        CHECK(L::nextDriver(settings, r.fake.reservation, r.fake.current, r.now, work) &&
              work.ready);
        const auto id = r.admit(sequence, work.transfer);
        r.fake.autoConfirm = false;
        r.turn(r.now);
        CHECK(settings.effectsPossible && r.fake.current.configurationGeneration == 8);
        r.turn(r.now + 100, E::ServiceBudget());
        const auto& terminal = r.done(id);
        CHECK(terminal.controllerAccepted && terminal.service.outcome == C::SdoOutcome::TIMEOUT &&
              terminal.service.transmissionPossible && !terminal.service.transmissionConfirmed &&
              !terminal.service.protocolAcknowledged && terminal.service.effectUncertain);
        CHECK(L::advanceDriver(settings, r.fake.reservation, r.fake.current, terminal.service,
                               r.now));
        CHECK(settings.state == L::SettingsState::FAILED && settings.uncertain &&
              settings.effectsPossible && !settings.confirmed && !settings.acknowledgedUs[0]);
        r.fake.settings = nullptr;
        r.fake.autoConfirm = true;
        const Saved<E::Completion> retained(terminal);
        r.fake.push(response(1, 0x2500, 2, 0, true), r.now + 1);
        r.turn(r.now + 1, budget(1, 0, 0, 0));
        retained.unchanged(r.done(id));
        CHECK(r.services.unresolved(0) && !r.owner.sequence(sequence));
        checkFailedControlRetention(r, sequence, id, settings, 2);
    }
    for (unsigned field = 0; field < 2; ++field)
        for (unsigned failure = 0; failure < 3; ++failure) {
            Rig r;
            L::DriverSettingsContext settings;
            const auto sequence = prepareOwnedControlSetting(r, settings);
            L::PreparedRead work;
            CHECK(L::nextDriver(settings, r.fake.reservation, r.fake.current, r.now, work) &&
                  work.ready);
            const auto write = r.admit(sequence, work.transfer);
            r.turn(r.now);
            r.respond(write, 1, 0x2500, 0, true);
            const auto acknowledged = r.done(write).service;
            CHECK(acknowledged.protocolAcknowledged);
            CHECK(L::advanceDriver(settings, r.fake.reservation, r.fake.current, acknowledged,
                                   r.now));
            CHECK(!r.owner.advanceConfiguration(sequence, r.fake.current, r.now));
            CHECK(r.owner.release(write));
            CHECK(r.owner.advanceConfiguration(sequence, r.fake.current, r.now));
            r.fake.settings = nullptr;
            if (field) {
                CHECK(L::nextDriver(settings, r.fake.reservation, r.fake.current, r.now, work) &&
                      work.ready);
                const auto first = r.exchange(sequence, work.transfer, 2);
                CHECK(L::advanceDriver(settings, r.fake.reservation, r.fake.current, first, r.now));
            }
            CHECK(L::nextDriver(settings, r.fake.reservation, r.fake.current, r.now, work) &&
                  work.ready && work.transfer.index == (field ? 0x2501 : 0x2500));
            const auto id = r.admit(sequence, work.transfer);
            r.turn(r.now);
            if (failure == 0) {
                M::Frame abort = response(1, work.transfer.index, 2);
                abort.data[0] = 0x80;
                abort.data[4] = 1;
                abort.data[5] = 0;
                abort.data[6] = 1;
                abort.data[7] = 6; // Literal 0x06010001, independently supplied server abort.
                r.fake.push(abort, r.now + 1);
                r.turn(r.now + 1, budget(1, 0, 8, 0));
            } else if (failure == 1)
                r.turn(r.now + 100, E::ServiceBudget());
            else
                r.respond(id, 1, work.transfer.index, field ? 1234 : 1);
            const auto& terminal = r.done(id);
            CHECK(L::advanceDriver(settings, r.fake.reservation, r.fake.current, terminal.service,
                                   r.now));
            CHECK(settings.state == L::SettingsState::FAILED && settings.effectsPossible &&
                  settings.confirmed == 1 &&
                  settings.acknowledgedUs[0] == acknowledged.completedUs);
            CHECK(settings.outcome == (failure == 0   ? M::ReadOutcome::PROTOCOL_REJECTED
                                       : failure == 1 ? M::ReadOutcome::DEADLINE
                                                      : M::ReadOutcome::REPLY_ERROR));
            if (failure == 0)
                CHECK(settings.evidence.abortValid && settings.evidence.abortCode == 0x06010001);
            if (failure == 1)
                CHECK(settings.uncertain && terminal.service.effectUncertain);
            if (field)
                CHECK(settings.readback.read.values[0].valid &&
                      settings.readback.read.values[0].raw == 2);
            checkFailedControlRetention(r, sequence, id, settings, 3 + field);
        }
}
} // namespace
int main() {
    copiedAdmissionAndRetainedPressure();
    fairProducersAndBusyChannel();
    passiveDispatchDuringTransferAndBudgets();
    invalidAdmissionSequenceAndIdExhaustion();
    queueAgeAndServiceEvidence();
    checkedAbortTxReportsAndGuard();
    boundedCallbacksAndOneShotTransportFailure();
    nodeInvalidationKeepsOtherChannelAndTerminals();
    sharedPeerQuarantineRejectsNewSequenceAtomically();
    sharedPeerQuarantineRetiresActiveBeforeLaterRxOrTx();
    staleNegativeReportDoesNotFailNewSegment();
    alternateChannelUsesConfiguredBindingAndSharesExclusion();
    rejectedIngressRemainsDiagnosticOnly();
    publicReadAndConfirmedControlSetting();
    actualControlUncertainDownloadAndFailedReadback();
    std::printf("can_bus_owner_test: %u checks; owner=%zu, completion=%zu bytes\n", checks,
                sizeof(E::CanBusOwner), sizeof(E::Completion));
    return 0;
}
