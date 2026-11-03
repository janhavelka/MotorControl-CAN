// SPDX-License-Identifier: MIT
// Explicit settings through the real CanBusOwner and SdoClient, against a scripted object dictionary
// holding the values both recorded CL86-C specimens reported. Wire bytes are literal; nothing here
// is a device measurement, and the scripted answers prove only what the library does with them.
#include "CanBusOwner.h"
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace O = MotorControlCANOwner;
static unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/OwnerRig.h"
namespace {
using OwnerRig::budget;
using OwnerRig::route;
constexpr uint64_t DEADLINE = 100000;
constexpr uint64_t AGE = 5000;
const uint8_t STATE_MASK = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                   (1u << unsigned(L::StateBlock::STATUS)));
// An input placed inside an output. Only its address is ever used: the call refuses before reading it.
template <class T> const T& at(const void* address) {
    return *static_cast<const T*>(address);
}
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
L::ReadOptions options() {
    L::ReadOptions o;
    o.queueTimeoutUs = o.responseTimeoutUs = 100;
    return o;
}
L::ReadTarget target() {
    L::ReadTarget t;
    t.channel = route(1);
    t.nodeGeneration = 11;
    return t;
}
struct Entry {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t raw;
};
// A scripted dictionary, not a drive model: it stores what is written and answers what it holds.
struct Drive {
    enum Fault { NONE, ABORT, LOST };
    Entry table[74] = {{0x2100, 0, 2, 1},      {0x2101, 0, 2, 0},
                       {0x2102, 0, 2, 0},      {0x6041, 0, 2, 0x8037},
                       {0x6061, 0, 1, 0},      {0x6040, 0, 2, 0},
                       {0x2200, 0, 2, 0},      {0x2201, 0, 2, 1000},
                       {0x2202, 0, 2, 0},      {0x607d, 0, 1, 2},
                       {0x607d, 1, 4, 0x80000000u}, {0x607d, 2, 4, 0x7fffffffu},
                       {0x6081, 0, 4, 60},     {0x6083, 0, 4, 100},
                       {0x6084, 0, 4, 100},    {0x607a, 0, 4, 5000},
                       {0x60fd, 0, 4, 0},      {0x603f, 0, 2, 0},
                       {0x2400, 0, 2, 16},     {0x2400, 1, 2, 0xa500},
                       {0x2400, 2, 2, 1},      {0x2400, 3, 2, 2},
                       {0x2400, 4, 2, 3},      {0x2400, 5, 2, 0},
                       {0x2400, 6, 2, 0},      {0x2400, 7, 2, 0},
                       {0x2400, 8, 2, 0},      {0x2400, 0xc, 2, 0},
                       {0x2400, 0xd, 2, 0},    {0x2400, 0xe, 2, 0},
                       {0x2400, 0xf, 2, 0},    {0x2401, 0, 2, 2},
                       {0x2402, 0, 2, 2},      {0x2403, 0, 2, 2},
                       {0x2404, 0, 2, 2},      {0x2405, 0, 2, 2},
                       {0x2406, 0, 2, 2},      {0x2407, 0, 2, 2},
                       {0x60fe, 0, 1, 2},      {0x60fe, 1, 4, 0},
                       {0x60fe, 2, 4, 0},
                       {0x2500, 0, 2, 2},      {0x2501, 0, 2, 4000},
                       // No retained record reads 2502..2520. These words are arbitrary, and all differ.
                       {0x2502, 0, 2, 0x1002}, {0x2503, 0, 2, 0x1003},
                       {0x2504, 0, 2, 0x1004}, {0x2505, 0, 2, 0x1005},
                       {0x2506, 0, 2, 0x1006}, {0x2507, 0, 2, 0x1007},
                       {0x2508, 0, 2, 0x1008}, {0x2509, 0, 2, 0x1009},
                       {0x250a, 0, 2, 0x100a}, {0x250b, 0, 2, 0x100b},
                       {0x250c, 0, 2, 0x100c}, {0x250d, 0, 2, 0x100d},
                       {0x250e, 0, 2, 0x100e}, {0x250f, 0, 2, 0x100f},
                       {0x2510, 0, 2, 0x1010}, {0x2511, 0, 2, 0x1011},
                       {0x2512, 0, 2, 0x1012}, {0x2513, 0, 2, 0x1013},
                       {0x2514, 0, 2, 0x1014}, {0x2515, 0, 2, 0x1015},
                       {0x2516, 0, 2, 0x1016}, {0x2517, 0, 2, 0x1017},
                       {0x2518, 0, 2, 0x1018}, {0x2519, 0, 2, 0x1019},
                       {0x251a, 0, 2, 0x101a}, {0x251b, 0, 2, 0x101b},
                       {0x251c, 0, 2, 0x101c}, {0x251d, 0, 2, 0x101d},
                       {0x251e, 0, 2, 0x101e}, {0x251f, 0, 2, 0x101f},
                       {0x2520, 0, 2, 0x1020}};
    Entry* find(uint16_t index, uint8_t subindex) {
        for (Entry& e : table)
            if (e.index == index && e.subindex == subindex)
                return &e;
        return nullptr;
    }
    void set(uint16_t index, uint8_t subindex, uint32_t raw) {
        Entry* e = find(index, subindex);
        CHECK(e);
        e->raw = raw;
    }
    uint32_t value(uint16_t index, uint8_t subindex = 0) {
        Entry* e = find(index, subindex);
        CHECK(e);
        return e->raw;
    }
    uint16_t faultIndex = 0;
    uint8_t faultSubindex = 0;
    bool faultDownload = true;
    Fault fault = NONE;
    void inject(Fault kind, uint16_t index, uint8_t subindex, bool download) {
        fault = kind;
        faultIndex = index;
        faultSubindex = subindex;
        faultDownload = download;
    }
    // A readback answer that differs from what is stored.
    uint16_t lieIndex = 0;
    uint8_t lieSubindex = 0;
    uint32_t lieRaw = 0;
    unsigned downloads = 0, uploads = 0;
    bool serve(const M::Frame& request, uint32_t replyId, M::Frame& reply) {
        const uint16_t index = uint16_t(request.data[1] | (uint16_t(request.data[2]) << 8));
        const uint8_t subindex = request.data[3];
        if (request.data[0] == 0x80)
            return false; // Client abort cleanup: a server sends nothing back.
        const bool download = request.data[0] != 0x40;
        Entry* e = find(index, subindex);
        CHECK(e);
        (download ? downloads : uploads)++;
        reply.id = replyId;
        reply.dlc = reply.payloadLength = 8;
        reply.data[1] = request.data[1];
        reply.data[2] = request.data[2];
        reply.data[3] = subindex;
        if (fault != NONE && faultIndex == index && faultSubindex == subindex &&
            faultDownload == download) {
            if (fault == LOST) {
                if (download) // The write reached the dictionary; only its answer was lost.
                    store(*e, request);
                return false;
            }
            reply.data[0] = 0x80;
            reply.data[4] = 0x22;
            reply.data[7] = 0x08;
            return true;
        }
        if (download) {
            store(*e, request);
            reply.data[0] = 0x60;
            return true;
        }
        const uint32_t raw = lieIndex == index && lieSubindex == subindex ? lieRaw : e->raw;
        reply.data[0] = e->width == 1 ? 0x4f : e->width == 2 ? 0x4b : 0x43;
        for (unsigned i = 0; i < e->width && i < 4; ++i)
            reply.data[4 + i] = uint8_t(raw >> (8 * i));
        return true;
    }
    static void store(Entry& e, const M::Frame& request) {
        // An expedited download names its width in its command byte, so a wrong width cannot pass.
        CHECK(request.data[0] == (e.width == 1 ? 0x2f : e.width == 2 ? 0x2b : 0x23));
        uint32_t raw = 0;
        for (unsigned i = 0; i < e.width && i < 4; ++i)
            raw |= uint32_t(request.data[4 + i]) << (8 * i);
        e.raw = raw;
    }
};
// The application side of the owner: notes each settings download, and retires retained motion
// parameters for a move's own write, both from the one pre-transmit hook.
struct Wire : OwnerRig::WireBase {
    L::DriverSettingsContext* settings = nullptr;
    L::AxisReservation live;
    M::Binding current;
    unsigned guards = 0, notices = 0, sentAtNotice = 0;
    bool rejectGuard = false;
    L::DriverContext* retained[2] = {nullptr, nullptr};
    static bool guard(void* context, O::SequenceId, const C::SdoTransfer& work, uint64_t now) {
        Wire& w = static_cast<Wire&>(*static_cast<OwnerRig::WireBase*>(context));
        ++w.guards;
        if (w.rejectGuard)
            return false;
        if (w.settings) {
            CHECK(L::noteDriverTransmission(*w.settings, w.live, w.current, now));
            ++w.notices;
            w.sentAtNotice = w.sentCount;
        }
        for (L::DriverContext* c : w.retained)
            if (c) {
                L::ReadTarget current = w.live.target;
                current.channel.binding = w.current;
                CHECK(L::retireMotionParameters(*c, current, work));
            }
        return true;
    }
    O::Transport transport() {
        O::Transport t = plumbing();
        t.beforeTransmit = guard;
        return t;
    }
};
struct Bench {
    C::SdoClient services;
    Wire wire;
    O::CanBusOwner owner;
    Drive drive;
    uint64_t now = 0;
    O::SequenceId sequence;
    L::AxisReservation reservation;
    uint32_t operation = 10;
    unsigned served = 0;
    explicit Bench(L::SettingsModel model = L::SettingsModel::CL86_C)
        : owner(services, wire.transport(), 73) {
        CHECK(services.configure(0, route(1), true, true));
        C::NetworkRoute n;
        n.binding = route(1).binding;
        n.generation = 11;
        n.heartbeatId = 0x701;
        n.emcyId = 0x81;
        CHECK(services.configureNetwork(0, n, C::NetworkObserver(), 0));
        wire.current = route(1).binding;
        CHECK(owner.beginSequence(route(1).binding, 1, now, sequence));
        const O::Sequence* s = owner.sequence(sequence);
        CHECK(s);
        reservation.target = target();
        reservation.model = model;
        reservation.owner = owner.identity();
        reservation.sequence = uint32_t(sequence.slot + 1);
        reservation.generation = sequence.generation;
        reservation.acquiredUs = s->acquiredUs;
        wire.live = reservation;
    }
    L::ReadTarget current() const {
        L::ReadTarget t = reservation.target;
        t.channel.binding = wire.current;
        return t;
    }
    // A finished transaction keeps its frozen token. The next one takes a new value holding the
    // same owner facts and the current target, as the application constructs it.
    void renew() {
        reservation.target = current();
        wire.live = reservation;
    }
    void turn(uint64_t time, O::ServiceBudget b = budget()) {
        now = time;
        CHECK(owner.service(now, b));
    }
    uint32_t begin(uint64_t deadline = DEADLINE) {
        ++operation;
        CHECK(owner.beginOperation(sequence, operation, deadline, now));
        return operation;
    }
    O::RequestId admit(const C::SdoTransfer& work) {
        O::BusRequest request;
        request.sequence = sequence;
        request.transfer = work;
        O::RequestId id;
        CHECK(owner.admit(request, now, id) == O::BusAdmission::ACCEPTED);
        return id;
    }
    // Answer everything sent since the last call, in order, as the scripted server.
    void deliver() {
        for (; served < wire.sentCount; ++served) {
            M::Frame reply;
            if (wire.sent[served].id == 0x601 &&
                drive.serve(wire.sent[served], 0x581, reply))
                wire.push(reply, now + 1);
        }
    }
    C::SdoCompletion exchange(const C::SdoTransfer& work) {
        const O::RequestId id = admit(work);
        turn(now);
        deliver();
        turn(now + 1, budget(8, 0, 8, 0));
        const O::Completion* c = owner.result(id);
        CHECK(c && c->reason == O::CompletionReason::SERVICE);
        const C::SdoCompletion copy = c->service;
        CHECK(owner.release(id));
        return copy;
    }
    void read(L::DriverContext& out, L::DriverGroup group) {
        const uint32_t op = begin();
        CHECK(L::prepareDriverRead(out, current(), op, now, DEADLINE, options(), group));
        for (;;) {
            L::PreparedRead work;
            CHECK(L::nextDriver(out, now, work));
            if (!work.ready)
                break;
            const C::SdoCompletion terminal = exchange(work.transfer);
            CHECK(L::advanceDriver(out, terminal, now));
        }
        CHECK(out.read.state == M::ReadState::SUCCEEDED);
    }
    void readState(L::ReadContext& out, uint8_t mask = STATE_MASK) {
        const uint32_t op = begin();
        CHECK(L::prepareState(out, current(), op, now, DEADLINE, options(), mask));
        for (;;) {
            L::PreparedRead work;
            CHECK(L::nextRead(out, now, work));
            if (!work.ready)
                break;
            const C::SdoCompletion terminal = exchange(work.transfer);
            CHECK(L::advanceRead(out, terminal, now));
        }
    }
    C::SdoCompletion control(uint16_t word = 0x000f) {
        C::SdoTransfer work;
        work.channel = current().channel;
        work.operation = work.request = begin();
        work.step = 1;
        work.index = 0x6040;
        work.direction = C::SdoDirection::DOWNLOAD;
        work.size = 2;
        work.data[0] = uint8_t(word);
        work.data[1] = uint8_t(word >> 8);
        work.deadlineUs = DEADLINE;
        work.queueTimeoutUs = work.responseTimeoutUs = 100;
        return exchange(work);
    }
    // Drive one prepared context to its terminal through the owner, moving the reservation to the
    // new configuration once the old results are released.
    void run(L::DriverSettingsContext& c) {
        wire.settings = &c;
        bool moved = false;
        while (c.state == L::SettingsState::WRITING || c.state == L::SettingsState::READING_BACK) {
            if (c.state == L::SettingsState::READING_BACK && !moved) {
                CHECK(owner.advanceConfiguration(sequence, wire.current, now));
                moved = true;
            }
            L::PreparedRead work;
            CHECK(L::nextDriver(c, wire.live, wire.current, now, work) && work.ready);
            const C::SdoCompletion terminal = exchange(work.transfer);
            CHECK(L::advanceDriver(c, wire.live, wire.current, terminal, now));
        }
        wire.settings = nullptr;
    }
    // Index of the next frame this bench will send.
    unsigned mark() const { return wire.sentCount; }
    void checkSilentSince(unsigned sent) const { CHECK(wire.sentCount == sent); }
};
bool frameIs(const M::Frame& f, uint8_t command, uint16_t index, uint8_t subindex, uint32_t value) {
    if (f.id != 0x601 || f.dlc != 8 || f.data[0] != command || f.data[1] != uint8_t(index) ||
        f.data[2] != uint8_t(index >> 8) || f.data[3] != subindex)
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (f.data[4 + i] != uint8_t(value >> (8 * i)))
            return false;
    return true;
}
L::DriverSettings motion(uint32_t speed = 120, uint32_t up = 200, uint32_t down = 300) {
    L::DriverSettings s;
    s.positionSpeedRpm = speed;
    s.rampUpTimeMs = up;
    s.rampDownTimeMs = down;
    return s;
}
L::SoftLimitSettings limit(uint16_t code) {
    L::SoftLimitSettings s;
    s.enable = static_cast<L::SoftLimitEnable>(code);
    return s;
}
// Everything prepareDriverSettings wants, acquired in its required order.
struct MotionEvidence {
    C::SdoCompletion control;
    L::ReadContext state;
    L::DriverContext prior;
    explicit MotionEvidence(Bench& b) {
        b.drive.set(0x6061, 0, 1);
        control = b.control();
        b.readState(state);
        b.read(prior, L::DriverGroup::MOTION_PARAMETERS);
    }
};
M::Status prepareMotion(L::DriverSettingsContext& out, Bench& b, const MotionEvidence& e,
                        const L::DriverSettings& s, uint64_t deadline = DEADLINE) {
    const uint32_t op = b.begin(deadline);
    return L::prepareDriverSettings(out, b.wire.live, b.current(), e.prior, e.state, e.control, s, op, b.now,
                                    deadline, options(), AGE);
}
struct SelectorEvidence {
    L::ReadContext state;
    L::DriverContext limits;
    explicit SelectorEvidence(Bench& b) {
        b.readState(state);
        b.read(limits, L::DriverGroup::SOFT_LIMITS);
    }
};
M::Status prepareLimit(L::DriverSettingsContext& out, Bench& b, const SelectorEvidence& e,
                       uint16_t code, uint64_t deadline = DEADLINE) {
    const uint32_t op = b.begin(deadline);
    return L::prepareSoftLimitSettings(out, b.wire.live, b.current(), e.limits, e.state, limit(code), op, b.now,
                                       deadline, options(), AGE);
}
// DI through the real owner. What the fixture holds matches the specimens' reported assignment, with
// unexplained high polarity bits added: origin and both limits on DI0..2, the rest undefined.
L::IoSettings asFound() {
    L::IoSettings s;
    s.invertedInputs = 0;
    s.functions[0] = L::InputFunction::ORIGIN;
    s.functions[1] = L::InputFunction::POSITIVE_LIMIT;
    s.functions[2] = L::InputFunction::NEGATIVE_LIMIT;
    for (unsigned i = 3; i < 7; ++i)
        s.functions[i] = L::InputFunction::UNDEFINED;
    return s;
}
// All eight words differ from the fixture.
L::IoSettings everyField() {
    L::IoSettings s;
    s.invertedInputs = 0x55;
    for (unsigned i = 0; i < 7; ++i)
        s.functions[i] = static_cast<L::InputFunction>(13 - i);
    return s;
}
M::Status prepareIo(L::DriverSettingsContext& out, Bench& b, const L::DriverContext& prior,
                    const L::IoSettings& s, uint64_t deadline = DEADLINE) {
    const uint32_t op = b.begin(deadline);
    return L::prepareIoSettings(out, b.wire.live, b.current(), prior, s, op, b.now, deadline, options(), AGE);
}
M::PreparedTarget preparedTarget(const Bench& b, M::AxisConfig& axis) {
    axis.binding = b.wire.current;
    M::PositionRequest request;
    request.configurationGeneration = axis.binding.configurationGeneration;
    request.value = M::Rational(10);
    M::PreparedTarget prepared;
    CHECK(M::preparePosition(request, axis, nullptr, prepared));
    return prepared;
}

void positionParametersEndToEnd() {
    Bench b;
    MotionEvidence e(b);
    L::DriverSettingsContext c;
    CHECK(prepareMotion(c, b, e, motion(3000, 0, 2000)));
    CHECK(c.state == L::SettingsState::WRITING && c.changed == 7 && !c.effectsPossible);
    CHECK(b.wire.current.configurationGeneration == 7);
    const unsigned first = b.mark();
    b.run(c);
    CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == 7 && !c.uncertain);
    CHECK(b.wire.current.configurationGeneration == 8 && b.wire.notices == 3);
    CHECK(frameIs(b.wire.sent[first], 0x23, 0x6081, 0, 3000));
    CHECK(frameIs(b.wire.sent[first + 1], 0x23, 0x6083, 0, 0));
    CHECK(frameIs(b.wire.sent[first + 2], 0x23, 0x6084, 0, 2000));
    CHECK(frameIs(b.wire.sent[first + 3], 0x40, 0x6081, 0, 0));
    CHECK(frameIs(b.wire.sent[first + 4], 0x40, 0x6083, 0, 0));
    CHECK(frameIs(b.wire.sent[first + 5], 0x40, 0x6084, 0, 0));
    CHECK(b.wire.sentCount == first + 6);
    CHECK(c.readback.read.target.channel.binding.configurationGeneration == 8);
    L::DriverObservation seen;
    CHECK(L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
    CHECK(seen.positionSpeedRpm == 3000 && seen.rampUpTimeMs == 0 && seen.rampDownTimeMs == 2000);
    // The reservation token stays at its original epoch, and no later owner read can use it.
    CHECK(b.reservation.target.channel.binding.configurationGeneration == 7);
    CHECK(!L::getDriver(c, b.wire.live, b.reservation.target, b.now, AGE, seen));
}

void changedFieldsOnlyAndSourceOrder() {
    Bench b;
    MotionEvidence e(b);
    L::DriverSettingsContext c;
    CHECK(prepareMotion(c, b, e, motion(60, 200, 100)));
    CHECK(c.changed == 2);
    const unsigned first = b.mark();
    b.run(c);
    CHECK(c.state == L::SettingsState::SUCCEEDED && b.wire.notices == 1);
    CHECK(frameIs(b.wire.sent[first], 0x23, 0x6083, 0, 200));
    CHECK(b.wire.sentCount == first + 4 && b.drive.downloads == 2); // The 6040 prerequisite and this.
}

template <class Prepare> void refused(Bench& b, L::DriverSettingsContext& out, Prepare call) {
    const Saved<L::DriverSettingsContext> before(out);
    const Saved<M::Binding> binding(b.wire.current);
    const unsigned sent = b.mark(), guards = b.wire.guards;
    CHECK(!call());
    before.unchanged(out);
    binding.unchanged(b.wire.current);
    b.turn(b.now);
    b.checkSilentSince(sent);
    CHECK(b.wire.guards == guards);
}

void everyDocumentedBoundary() {
    struct Vector {
        uint32_t speed, up, down;
        bool accepted;
    };
    const Vector motionVectors[] = {
        {0, 0, 0, true},          {3000, 2000, 2000, true}, {3000, 0, 2000, true},
        {3001, 0, 0, false},      {0, 2001, 0, false},      {0, 0, 2001, false},
        {UINT32_MAX, 0, 0, false}, {0, UINT32_MAX, 0, false}, {0, 0, UINT32_MAX, false},
    };
    for (const Vector& v : motionVectors) {
        Bench b;
        MotionEvidence e(b);
        L::DriverSettingsContext c;
        c.operation = 0xbeef;
        if (!v.accepted) {
            refused(b, c, [&] { return prepareMotion(c, b, e, motion(v.speed, v.up, v.down)); });
            continue;
        }
        CHECK(prepareMotion(c, b, e, motion(v.speed, v.up, v.down)));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED);
        CHECK(b.drive.value(0x6081) == v.speed && b.drive.value(0x6083) == v.up &&
              b.drive.value(0x6084) == v.down);
    }
    { // The default candidate is invalid in every field.
        Bench b;
        MotionEvidence e(b);
        L::DriverSettingsContext c;
        refused(b, c, [&] { return prepareMotion(c, b, e, L::DriverSettings()); });
    }
    for (uint16_t code : {uint16_t(0), uint16_t(1), uint16_t(2), uint16_t(10), uint16_t(0x8000),
                          uint16_t(0xffff)}) {
        Bench b;
        // Start from the other documented code, so codes 0 and 1 both need a real write.
        b.drive.set(0x2202, 0, code == 0 ? 1 : 0);
        const uint32_t before = b.drive.value(0x2202);
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        if (code > 1) {
            refused(b, c, [&] { return prepareLimit(c, b, e, code); });
            CHECK(b.drive.value(0x2202) == before);
            continue;
        }
        const unsigned first = b.mark();
        CHECK(prepareLimit(c, b, e, code));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == 1);
        CHECK(b.drive.value(0x2202) == code);
        // One literal write, then the group's four literal reads, and nothing else.
        CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2202, 0, code));
        CHECK(frameIs(b.wire.sent[first + 1], 0x40, 0x2202, 0, 0));
        for (uint8_t sub = 0; sub < 3; ++sub)
            CHECK(frameIs(b.wire.sent[first + 2 + sub], 0x40, 0x607d, sub, 0));
        CHECK(b.wire.sentCount == first + 5);
        L::DriverObservation seen;
        CHECK(L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
        CHECK(seen.softLimitSelectorRaw == code && !seen.interpreted);
    }
    { // The DI group: the seven function codes and the documented polarity bits, literal on the wire.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::IoSettings s;
        s.invertedInputs = 0x7f;
        for (L::InputFunction& f : s.functions)
            f = static_cast<L::InputFunction>(13);
        L::DriverSettingsContext c;
        uint32_t op = b.begin();
        CHECK(L::prepareIoSettings(c, b.wire.live, b.current(), prior, s, op, b.now, DEADLINE, options(), AGE));
        const unsigned first = b.mark();
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED);
        // Unknown high polarity bits of the observed word survive the write.
        CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2400, 1, 0xa57f));
        CHECK(b.drive.value(0x2400, 1) == 0xa57f);
        b.renew();
        L::IoSettings reserved = s;
        reserved.invertedInputs = 0x80;
        L::DriverContext fresh;
        b.read(fresh, L::DriverGroup::IO_INPUT_CONFIGURATION);
        op = b.begin();
        L::DriverSettingsContext r;
        refused(b, r, [&] {
            return L::prepareIoSettings(r, b.wire.live, b.current(), fresh, reserved, op, b.now, DEADLINE,
                                        options(), AGE);
        });
        L::IoSettings badCode = s;
        badCode.functions[6] = static_cast<L::InputFunction>(14);
        refused(b, r, [&] {
            return L::prepareIoSettings(r, b.wire.live, b.current(), fresh, badCode, op, b.now, DEADLINE,
                                        options(), AGE);
        });
        // The same read admits a valid candidate, so those two refusals isolate the bad value.
        CHECK(L::prepareIoSettings(r, b.wire.live, b.current(), fresh, s, op, b.now, DEADLINE, options(), AGE));
    }
}

void unknownRawReadsStayRawAndAdmitNothing() {
    { // A drive actually holding the EDS default 10: readable, and never a basis for a transition.
        Bench b;
        b.drive.set(0x2202, 0, 10);
        SelectorEvidence e(b);
        L::DriverObservation seen;
        CHECK(L::getDriver(e.limits, b.current(), b.now, AGE, seen));
        CHECK(seen.softLimitSelectorRaw == 10 && !seen.interpreted);
        L::DriverSettingsContext c;
        refused(b, c, [&] { return prepareLimit(c, b, e, 0); });
        refused(b, c, [&] { return prepareLimit(c, b, e, 1); });
    }
    // 2200 and 2201 are reads only: whatever they hold stays raw, and no call here can write them.
    for (uint32_t raw : {0u, 1u, 7u, 399u, 400u, 1000u, 51200u, 51201u, 65535u}) {
        Bench b;
        b.drive.set(0x2200, 0, raw);
        b.drive.set(0x2201, 0, raw);
        L::DriverContext driver;
        b.read(driver, L::DriverGroup::DRIVER);
        L::DriverObservation seen;
        CHECK(L::getDriver(driver, b.current(), b.now, AGE, seen));
        CHECK(seen.directionRaw == raw && seen.subdivisionRaw == raw);
        CHECK(b.drive.downloads == 0);
    }
    { // A retained odd count is compared, not rewritten, across a 2202 write.
        Bench b;
        b.drive.set(0x607d, 0, 5);
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        CHECK(c.changed == 1);
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.value(0x607d, 0) == 5);
    }
    { // An odd 607D count stays raw, is no discovery bound and changes the read plan not at all.
        Bench b;
        b.drive.set(0x607d, 0, 5);
        const unsigned uploads = b.drive.uploads;
        L::DriverContext limits;
        b.read(limits, L::DriverGroup::SOFT_LIMITS);
        CHECK(b.drive.uploads == uploads + 4);
        L::DriverObservation seen;
        CHECK(L::getDriver(limits, b.current(), b.now, AGE, seen));
        CHECK(seen.softLimitEntries == 5 && seen.minimumNative == INT32_MIN &&
              seen.maximumNative == INT32_MAX);
    }
    // Limit order is checked for arming only, and the bounds themselves are never rewritten.
    struct Limits {
        int32_t minimum, maximum;
        bool arms;
    };
    const Limits limitVectors[] = {{INT32_MIN, INT32_MAX, true}, {-1, 0, true},
                                   {0, 0, false},                {1, 0, false},
                                   {INT32_MAX, INT32_MIN, false}};
    for (const Limits& v : limitVectors) {
        Bench b;
        b.drive.set(0x607d, 1, uint32_t(v.minimum));
        b.drive.set(0x607d, 2, uint32_t(v.maximum));
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        if (v.arms) {
            CHECK(prepareLimit(c, b, e, 1));
            b.run(c);
            CHECK(c.state == L::SettingsState::SUCCEEDED);
        } else {
            refused(b, c, [&] { return prepareLimit(c, b, e, 1); });
        }
        CHECK(b.drive.value(0x607d, 1) == uint32_t(v.minimum) &&
              b.drive.value(0x607d, 2) == uint32_t(v.maximum));
    }
    { // Out-of-range stored speed and ramps are observable; only the candidate is bounded.
        Bench b;
        b.drive.set(0x6081, 0, 70000);
        b.drive.set(0x6083, 0, 2001);
        MotionEvidence e(b);
        L::DriverObservation seen;
        CHECK(L::getDriver(e.prior, b.current(), b.now, AGE, seen));
        CHECK(seen.positionSpeedRpm == 70000 && seen.rampUpTimeMs == 2001);
        L::DriverSettingsContext c;
        refused(b, c, [&] { return prepareMotion(c, b, e, motion(70000, 2001, 100)); });
        CHECK(prepareMotion(c, b, e, motion(3000, 2000, 100)));
        CHECK(c.changed == 3);
    }
}

void completeCandidateRejectedBeforeTransmission() {
    // Each case spoils one prerequisite of a position candidate that is otherwise admissible.
    enum Case {
        NO_MODEL, PRIOR_BEFORE_RESERVATION, EXPIRED_AGE, CONTROL_WRONG_WORD, MODE_ZERO, MODE_VELOCITY, LOW_BITS,
        RUNNING, ALARM, RELEASED, PRIOR_BEFORE_CONTROL, STATE_BEFORE_CONTROL, NO_OPERATION,
        DEADLINE_NOW, NO_QUEUE_BUDGET, NO_RESPONSE_BUDGET, NO_AGE, EXHAUSTED, CASES
    };
    for (unsigned k = 0; k < CASES; ++k) {
        Bench b;
        if (k == MODE_ZERO)
            b.drive.set(0x6061, 0, 0);
        else
            b.drive.set(0x6061, 0, k == MODE_VELOCITY ? 3 : 1);
        if (k == LOW_BITS)
            b.drive.set(0x6041, 0, 0x8036);
        if (k == RUNNING)
            b.drive.set(0x2100, 0, 0x0005);
        if (k == ALARM)
            b.drive.set(0x2100, 0, 0x0009);
        if (k == RELEASED)
            b.drive.set(0x2100, 0, 0x0011);
        L::ReadContext state;
        L::DriverContext prior;
        C::SdoCompletion control;
        if (k == STATE_BEFORE_CONTROL)
            b.readState(state);
        if (k == PRIOR_BEFORE_CONTROL)
            b.read(prior, L::DriverGroup::MOTION_PARAMETERS);
        control = b.control(k == CONTROL_WRONG_WORD ? 0x0007 : 0x000f);
        if (k != STATE_BEFORE_CONTROL)
            b.readState(state);
        if (k != PRIOR_BEFORE_CONTROL)
            b.read(prior, L::DriverGroup::MOTION_PARAMETERS);
        L::AxisReservation live = b.wire.live;
        if (k == NO_MODEL)
            live.model = L::SettingsModel::UNKNOWN;
        if (k == PRIOR_BEFORE_RESERVATION)
            live.acquiredUs = b.now + 1;
        if (k == EXHAUSTED)
            live.target.channel.binding.configurationGeneration = UINT32_MAX;
        uint32_t operation = b.begin();
        if (k == NO_OPERATION)
            operation = 0;
        uint64_t deadline = DEADLINE, age = AGE;
        if (k == DEADLINE_NOW)
            deadline = b.now;
        if (k == NO_AGE)
            age = 0;
        if (k == EXPIRED_AGE)
            b.turn(b.now + AGE + 1);
        L::ReadOptions policy = options();
        if (k == NO_QUEUE_BUDGET)
            policy.queueTimeoutUs = 0;
        if (k == NO_RESPONSE_BUDGET)
            policy.responseTimeoutUs = 0;
        L::DriverSettingsContext c;
        refused(b, c, [&] {
            return L::prepareDriverSettings(c, live, live.target, prior, state, control, motion(), operation, b.now,
                                            deadline, policy, age);
        });
    }
    // The same evidence admits the candidate, so the cases above isolate what they spoil.
    Bench b;
    MotionEvidence e(b);
    L::DriverSettingsContext c;
    CHECK(prepareMotion(c, b, e, motion()));
    // A selector candidate is refused whole for the same reasons.
    enum SelectorCase { S_MODEL, S_WRONG_GROUP, S_NO_OPERATION, S_DEADLINE, S_NO_AGE, S_OTHER_GROUP,
                        S_BEFORE_RESERVATION, S_CASES };
    for (unsigned k = 0; k < S_CASES; ++k) {
        Bench s;
        L::ReadContext state;
        L::DriverContext limits, unrelated;
        s.readState(state);
        s.read(limits, L::DriverGroup::SOFT_LIMITS);
        L::AxisReservation live = s.wire.live;
        const L::DriverContext* priorRead = &limits;
        if (k == S_WRONG_GROUP || k == S_OTHER_GROUP) { // Complete, and its unused fields are 0.
            s.read(unrelated, k == S_WRONG_GROUP ? L::DriverGroup::DRIVER
                                                 : L::DriverGroup::MOTION_PARAMETERS);
            priorRead = &unrelated;
        }
        uint32_t operation = s.begin();
        uint64_t deadline = DEADLINE, age = AGE;
        if (k == S_MODEL)
            live.model = L::SettingsModel::UNKNOWN;
        if (k == S_NO_OPERATION)
            operation = 0;
        if (k == S_DEADLINE)
            deadline = s.now;
        if (k == S_NO_AGE)
            age = 0;
        if (k == S_BEFORE_RESERVATION)
            live.acquiredUs = s.now + 1; // The evidence precedes this reservation.
        L::DriverSettingsContext out;
        refused(s, out, [&] {
            return L::prepareSoftLimitSettings(out, live, live.target, *priorRead, state, limit(1), operation, s.now,
                                               deadline, options(), age);
        });
    }
    // Each prerequisite must follow the reservation on its own: a fresh prior read cannot carry an
    // older state read across it.
    {
        Bench s;
        L::ReadContext state;
        L::DriverContext limits;
        s.readState(state);
        L::AxisReservation live = s.wire.live;
        live.acquiredUs = s.now + 1;
        s.turn(s.now + 1);
        s.read(limits, L::DriverGroup::SOFT_LIMITS);
        const uint32_t operation = s.begin();
        L::DriverSettingsContext out;
        refused(s, out, [&] {
            return L::prepareSoftLimitSettings(out, live, live.target, limits, state, limit(1), operation, s.now,
                                               DEADLINE, options(), AGE);
        });
    }
}

// Project policy on 2100 for the 2202 write. Each case holds alone against the fixture's clean word.
void softLimitStateRules() {
    const auto admitted = [](uint32_t word, uint16_t code, uint32_t stored) {
        Bench b;
        b.drive.set(0x2100, 0, word);
        b.drive.set(0x2202, 0, stored);
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        const unsigned first = b.mark();
        const M::Status status = prepareLimit(c, b, e, code);
        if (status) {
            b.run(c);
            CHECK(c.state == L::SettingsState::SUCCEEDED);
            if (stored != code)
                CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2202, 0, code));
        } else {
            CHECK(b.wire.sentCount == first && b.drive.value(0x2202) == stored);
        }
        return bool(status);
    };
    CHECK(admitted(0x0001, 1, 0)); // Both specimens read 2100=1.
    CHECK(admitted(0x0000, 1, 0));
    // Bit 4 is deliberately unused: reported set or clear, it decides nothing here.
    CHECK(admitted(0x0011, 1, 0));
    CHECK(admitted(0x0011, 0, 1));
    // The soft-limit flags are documented, and are not a refusal on their own.
    CHECK(admitted(0x0021, 1, 0));
    CHECK(admitted(0x0041, 1, 0));
    for (uint16_t code : {uint16_t(0), uint16_t(1)}) {
        const uint32_t stored = code ? 0 : 1;
        CHECK(!admitted(0x0005, code, stored)); // Running.
        CHECK(!admitted(0x0009, code, stored)); // Alarm.
        for (unsigned bit = 7; bit < 16; ++bit) // Undocumented, fail closed one at a time.
            CHECK(!admitted(0x0001u | (1u << bit), code, stored));
    }
    // Homing reported: arming would take effect at once, so it refuses, and so does restating it.
    CHECK(!admitted(0x0003, 1, 0));
    CHECK(!admitted(0x0002, 1, 0));
    CHECK(!admitted(0x0003, 1, 1));
    // Disarming stays possible then, and even with reversed limits, which only arming cares about.
    CHECK(admitted(0x0003, 0, 1));
    {
        Bench b;
        b.drive.set(0x2202, 0, 1);
        b.drive.set(0x607d, 1, 5);
        b.drive.set(0x607d, 2, uint32_t(-5));
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        const unsigned first = b.mark();
        CHECK(prepareLimit(c, b, e, 0));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED);
        CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2202, 0, 0));
        CHECK(b.drive.value(0x607d, 1) == 5 && b.drive.value(0x607d, 2) == uint32_t(-5));
    }
}

// Review finding D1. A frozen token and its old evidence agree with each other, and an unchanged
// candidate from them would report success with no frame while the drive holds another value.
void frozenTokenCannotPrepareAFalseNoOp() {
    const auto noneFrom = [](Bench& b, const M::Status& status, const L::DriverSettingsContext& out,
                             const Saved<L::DriverSettingsContext>& blank, unsigned sent) {
        CHECK(!status && status.detail == int32_t(M::ReadError::TARGET));
        blank.unchanged(out);
        CHECK(b.wire.sentCount == sent);
    };
    { // After a finished transaction: the drive holds 1 and the old evidence says 0.
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.value(0x2202) == 1);
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> blank(out);
        const uint32_t op = b.begin();
        noneFrom(b, L::prepareSoftLimitSettings(out, b.wire.live, b.current(), e.limits, e.state, limit(0),
                                                op, b.now, DEADLINE, options(), AGE),
                 out, blank, b.mark());
    }
    { // The same for DI.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED);
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> blank(out);
        const uint32_t op = b.begin();
        noneFrom(b, L::prepareIoSettings(out, b.wire.live, b.current(), prior, asFound(), op, b.now,
                                         DEADLINE, options(), AGE),
                 out, blank, b.mark());
    }
    { // The same for position parameters.
        Bench b;
        MotionEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareMotion(c, b, e, motion(70, 110, 120)));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED);
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> blank(out);
        const uint32_t op = b.begin();
        noneFrom(b, L::prepareDriverSettings(out, b.wire.live, b.current(), e.prior, e.state, e.control,
                                             motion(60, 100, 100), op, b.now, DEADLINE, options(), AGE),
                 out, blank, b.mark());
    }
    // After a lost reply with possible effect: the dictionary took the write, the reservation is gone
    // and the application still holds the token and the evidence from before it.
    {
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        b.wire.settings = &c;
        b.drive.inject(Drive::LOST, 0x2202, 0, true);
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        b.deliver();
        b.turn(b.now + 100, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.effectUncertain);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.uncertain && b.drive.value(0x2202) == 1);
        CHECK(b.wire.current.configurationGeneration == 8);
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> blank(out);
        noneFrom(b, L::prepareSoftLimitSettings(out, b.wire.live, b.current(), e.limits, e.state, limit(0),
                                                99, b.now, DEADLINE, options(), AGE),
                 out, blank, b.mark());
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A lost DI reply, the same way.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.wire.settings = &c;
        b.drive.inject(Drive::LOST, 0x2400, 1, true);
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        b.deliver();
        b.turn(b.now + 100, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.uncertain);
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> blank(out);
        noneFrom(b, L::prepareIoSettings(out, b.wire.live, b.current(), prior, asFound(), 99, b.now,
                                         DEADLINE, options(), AGE),
                 out, blank, b.mark());
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A new node generation refuses the same way, before the evidence is even looked at.
        Bench b;
        SelectorEvidence e(b);
        L::ReadTarget rebooted = b.current();
        ++rebooted.nodeGeneration;
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> blank(out);
        noneFrom(b, L::prepareSoftLimitSettings(out, b.wire.live, rebooted, e.limits, e.state, limit(0), 99,
                                                b.now, DEADLINE, options(), AGE),
                 out, blank, b.mark());
    }
    { // A new value holding the same owner facts and the current target, with fresh evidence, works.
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        b.run(c);
        b.renew();
        SelectorEvidence fresh(b);
        L::DriverSettingsContext back;
        CHECK(prepareLimit(back, b, fresh, 0));
        CHECK(back.state == L::SettingsState::WRITING && back.changed == 1);
    }
}

// Packet 10's note. STATUS and MONITORS complete, a sibling block then fails: the blocks stay readable
// as history, and no settings call admits them. The same rule validateStop applies.
void failedStateReadIsRefused() {
    for (Drive::Fault fault : {Drive::ABORT, Drive::LOST}) {
        Bench b;
        b.drive.set(0x6061, 0, 1);
        const C::SdoCompletion control = b.control();
        L::DriverContext prior, limits;
        b.read(prior, L::DriverGroup::MOTION_PARAMETERS);
        b.read(limits, L::DriverGroup::SOFT_LIMITS);
        // INPUTS follows MONITORS and STATUS in the read plan, so both finish before it fails.
        b.drive.inject(fault, 0x60fd, 0, false);
        const uint8_t mask = uint8_t(STATE_MASK | (1u << unsigned(L::StateBlock::INPUTS)));
        L::ReadContext state;
        uint32_t op = b.begin();
        CHECK(L::prepareState(state, b.current(), op, b.now, DEADLINE, options(), mask));
        for (unsigned i = 0; i < 5; ++i) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, b.now, work) && work.ready);
            const C::SdoCompletion terminal = b.exchange(work.transfer);
            CHECK(L::advanceRead(state, terminal, b.now));
        }
        L::PreparedRead failing;
        CHECK(L::nextRead(state, b.now, failing) && failing.ready);
        CHECK(failing.transfer.index == 0x60fd);
        const O::RequestId id = b.admit(failing.transfer);
        b.turn(b.now);
        b.deliver();
        b.turn(b.now + (fault == Drive::LOST ? 100 : 1), budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(L::advanceRead(state, terminal->service, b.now));
        CHECK(state.state == M::ReadState::FAILED);
        CHECK(state.uncertain || fault == Drive::ABORT);
        L::StateObservation block;
        CHECK(L::getStateBlock(state, L::StateBlock::STATUS, b.current(), b.now, AGE, block));
        CHECK(L::getStateBlock(state, L::StateBlock::MONITORS, b.current(), b.now, AGE, block));
        op = b.operation;
        L::DriverSettingsContext c;
        refused(b, c, [&] {
            return L::prepareDriverSettings(c, b.wire.live, b.current(), prior, state, control, motion(), op, b.now,
                                            DEADLINE, options(), AGE);
        });
        refused(b, c, [&] {
            return L::prepareSoftLimitSettings(c, b.wire.live, b.current(), limits, state, limit(1), op, b.now,
                                               DEADLINE, options(), AGE);
        });
        CHECK(b.owner.release(id));
        if (fault == Drive::LOST)
            continue; // The route is unresolved: no fresh read can follow on it.
        // One fresh read clears it: nothing else about the candidate was wrong.
        b.drive.fault = Drive::NONE;
        L::ReadContext fresh;
        b.readState(fresh);
        CHECK(fresh.state == M::ReadState::SUCCEEDED && !fresh.uncertain);
        op = b.begin();
        // Each half of the rule holds alone, on a read no owner path produces.
        L::ReadContext forged = fresh;
        forged.uncertain = true;
        refused(b, c, [&] {
            return L::prepareDriverSettings(c, b.wire.live, b.current(), prior, forged, control, motion(), op, b.now,
                                            DEADLINE, options(), AGE);
        });
        forged = fresh;
        forged.state = M::ReadState::FAILED;
        refused(b, c, [&] {
            return L::prepareDriverSettings(c, b.wire.live, b.current(), prior, forged, control, motion(), op, b.now,
                                            DEADLINE, options(), AGE);
        });
        CHECK(L::prepareDriverSettings(c, b.wire.live, b.current(), prior, fresh, control, motion(), op, b.now,
                                       DEADLINE, options(), AGE));
    }
}

void noOpIsTrafficFree() {
    { // Position parameters.
        Bench b;
        MotionEvidence e(b);
        L::DriverSettingsContext c;
        const unsigned sent = b.mark(), guards = b.wire.guards;
        CHECK(prepareMotion(c, b, e, motion(60, 100, 100)));
        CHECK(c.state == L::SettingsState::SUCCEEDED && c.outcome == M::ReadOutcome::SUCCESS);
        CHECK(!c.changed && !c.effectsPossible && !c.uncertain);
        L::PreparedRead work;
        work.ready = true;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && !work.ready);
        b.turn(b.now);
        b.checkSilentSince(sent);
        CHECK(b.wire.guards == guards && b.wire.current.configurationGeneration == 7);
        L::DriverObservation seen;
        CHECK(L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
        CHECK(seen.positionSpeedRpm == 60);
    }
    { // The selector, and an earlier prepared target stays valid across it.
        Bench b;
        SelectorEvidence e(b);
        M::AxisConfig axis;
        const M::PreparedTarget prepared = preparedTarget(b, axis);
        L::DriverSettingsContext limits;
        const unsigned sent = b.mark();
        CHECK(prepareLimit(limits, b, e, 0));
        CHECK(limits.state == L::SettingsState::SUCCEEDED && !limits.changed);
        b.turn(b.now);
        b.checkSilentSince(sent);
        axis.binding = b.wire.current;
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
    }
}

void aliasedAndStaleContexts() {
    Bench b;
    SelectorEvidence e(b);
    // Two plans made while the token is current, before another transaction moves the generation.
    L::DriverSettingsContext heldNoOp, heldWrite, c;
    CHECK(prepareLimit(heldNoOp, b, e, 0));
    CHECK(heldNoOp.state == L::SettingsState::SUCCEEDED && !heldNoOp.changed);
    CHECK(prepareLimit(heldWrite, b, e, 1));
    CHECK(prepareLimit(c, b, e, 1));
    const uint32_t op = b.operation;
    const Saved<L::DriverSettingsContext> saved(c);
    const auto callAliased = [&](M::Status status) {
        CHECK(!status && status.detail == int32_t(M::ReadError::OVERLAP));
        saved.unchanged(c);
    };
    L::SoftLimitSettings wantedLimit = limit(1);
    L::ReadOptions policy = options();
    // Every input that can sit inside the output refuses, before any work.
    callAliased(L::prepareSoftLimitSettings(c, c.reservation, b.current(), e.limits, e.state, wantedLimit, op, b.now,
                                            DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, c.reservation.target, e.limits, e.state,
                                            wantedLimit, op, b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, b.current(), c.readback, e.state, wantedLimit, op, b.now,
                                            DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, b.current(), e.limits, c.readback.read, wantedLimit, op,
                                            b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, b.current(), e.limits, e.state,
                                            at<L::SoftLimitSettings>(&c.requested[0]),
                                            op, b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, b.current(), e.limits, e.state, wantedLimit, op, b.now,
                                            DEADLINE, c.readback.read.options, AGE));
    callAliased(L::noteDriverTransmission(c, c.reservation, b.wire.current, b.now));
    callAliased(L::noteDriverTransmission(c, b.wire.live, c.reservation.target.channel.binding, b.now));
    // A hand-edited active context refuses instead of writing an undocumented code or a retained field.
    L::PreparedRead edited;
    const uint32_t code = c.requested[0];
    const uint16_t fields = c.changed;
    c.requested[0] = 2;
    CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, b.now, edited));
    c.requested[0] = code;
    c.changed = 3;
    CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, b.now, edited));
    c.changed = fields;
    saved.unchanged(c);
    // A stale copy of the reservation refuses every step; the live one still works afterwards.
    L::AxisReservation stale = b.wire.live;
    ++stale.sequence;
    L::PreparedRead work;
    const Saved<L::PreparedRead> noWork(work);
    CHECK(!L::nextDriver(c, stale, b.wire.current, b.now, work));
    noWork.unchanged(work);
    M::Binding binding = b.wire.current;
    CHECK(!L::noteDriverTransmission(c, stale, binding, b.now));
    CHECK(binding.configurationGeneration == 7);
    saved.unchanged(c);
    b.run(c);
    CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.value(0x2202) == 1);
    // The two earlier plans are stale against the actual binding: next, note and get still refuse
    // them, including the no-op whose `state` says SUCCEEDED while the drive now holds 1.
    L::DriverObservation seen;
    const Saved<L::DriverObservation> none(seen);
    CHECK(!L::getDriver(heldNoOp, b.wire.live, b.current(), b.now, AGE, seen));
    none.unchanged(seen);
    CHECK(!L::nextDriver(heldNoOp, b.wire.live, b.wire.current, b.now, work));
    CHECK(heldWrite.state == L::SettingsState::WRITING);
    CHECK(!L::nextDriver(heldWrite, b.wire.live, b.wire.current, b.now, work));
    binding = b.wire.current;
    CHECK(!L::noteDriverTransmission(heldWrite, b.wire.live, binding, b.now));
    CHECK(binding.configurationGeneration == b.wire.current.configurationGeneration);
    // With a current token the old evidence is refused outright.
    b.renew();
    L::DriverSettingsContext next;
    refused(b, next, [&] { return prepareLimit(next, b, e, 0); });
    refused(b, next, [&] { return prepareLimit(next, b, e, 1); });
    // A settings context that was never prepared yields nothing.
    L::DriverSettingsContext empty;
    CHECK(!L::nextDriver(empty, b.wire.live, b.wire.current, b.now, work));
    CHECK(!L::getDriver(empty, b.wire.live, b.current(), b.now, AGE, seen));
    none.unchanged(seen);
    // Revocation: the owner invalidates, the application withdraws its token, and old copies stop.
    L::DriverSettingsContext active;
    Bench r;
    SelectorEvidence f(r);
    CHECK(prepareLimit(active, r, f, 1));
    const L::AxisReservation before = r.wire.live;
    CHECK(r.owner.invalidate(r.sequence, r.now) && !r.owner.sequence(r.sequence));
    r.reservation.owner = 0;
    r.wire.live.owner = 0;
    CHECK(!L::nextDriver(active, r.wire.live, r.wire.current, r.now, work));
    O::BusRequest late;
    late.sequence = r.sequence;
    CHECK(L::nextDriver(active, before, r.wire.current, r.now, work) && work.ready);
    late.transfer = work.transfer;
    O::RequestId refusedId;
    CHECK(r.owner.admit(late, r.now, refusedId) != O::BusAdmission::ACCEPTED);
}

void queuedBytesAreImmutable() {
    Bench b;
    MotionEvidence e(b);
    L::DriverSettingsContext c;
    L::DriverSettings wanted = motion(3000, 2000, 1);
    CHECK(prepareMotion(c, b, e, wanted));
    b.wire.settings = &c;
    L::PreparedRead work;
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const O::RequestId id = b.admit(work.transfer);
    // Neither the candidate, the yielded copy nor the context can reach the queued request.
    wanted = motion(1, 1, 1);
    work.transfer.data[0] = 0xee;
    work.transfer.data[1] = 0xee;
    const uint32_t held = c.requested[0];
    c.requested[0] = 1;
    const unsigned first = b.mark();
    b.turn(b.now);
    CHECK(frameIs(b.wire.sent[first], 0x23, 0x6081, 0, 3000));
    c.requested[0] = held;
    b.deliver();
    b.turn(b.now + 1, budget(8, 0, 8, 0));
    const O::Completion* terminal = b.owner.result(id);
    CHECK(terminal && terminal->service.outcome == C::SdoOutcome::SUCCESS);
    // The acknowledgement's transfer is the admitted copy, not the edited one.
    CHECK(terminal->service.transfer.data[0] == uint8_t(3000 & 0xff));
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
    CHECK(b.owner.release(id));
    CHECK(c.confirmed == 1 && c.state == L::SettingsState::WRITING);
    b.wire.settings = nullptr;
    CHECK(b.drive.value(0x6081) == 3000);
}

void partialAcknowledgementsAreRetained() {
    const uint16_t objects[] = {0x6081, 0x6083, 0x6084};
    for (unsigned at = 0; at < 3; ++at) {
        Bench b;
        MotionEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareMotion(c, b, e, motion(3000, 2000, 2000)));
        b.drive.inject(Drive::ABORT, objects[at], 0, true);
        b.run(c);
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
        CHECK(c.confirmed == (1u << at) - 1 && c.effectsPossible && c.uncertain);
        CHECK(c.evidencePresent && c.evidence.abortValid && c.evidence.abortCode == 0x08000022u);
        for (unsigned i = 0; i < 3; ++i) {
            CHECK((c.acknowledgedUs[i] != 0) == (i < at));
            CHECK(c.requested[i] == (i == 0 ? 3000u : 2000u));
        }
        // The prefix stays in the dictionary: failure is not rollback, and no later field went out.
        CHECK(b.drive.value(0x6081) == (at > 0 ? 3000u : 60u));
        CHECK(b.drive.value(0x6083) == (at > 1 ? 2000u : 100u));
        CHECK(b.drive.value(0x6084) == 100);
        CHECK(b.wire.current.configurationGeneration == 8 && b.wire.notices == at + 1);
        L::DriverObservation seen;
        const Saved<L::DriverObservation> none(seen);
        CHECK(!L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
        none.unchanged(seen);
        L::PreparedRead work;
        work.ready = true;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && !work.ready);
        const unsigned sent = b.mark();
        b.turn(b.now + 10);
        b.checkSilentSince(sent); // No retry, rollback or readback was queued.
    }
}

void lostReplyIsUncertainAndNeverReplayed() {
    const uint16_t objects[] = {0x6081, 0x6083, 0x6084};
    for (unsigned at = 0; at < 3; ++at) {
        Bench b;
        MotionEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareMotion(c, b, e, motion(3000, 2000, 2000)));
        b.wire.settings = &c;
        b.drive.inject(Drive::LOST, objects[at], 0, true);
        for (unsigned i = 0; i <= at; ++i) {
            L::PreparedRead work;
            CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
            if (i < at) {
                const C::SdoCompletion ack = b.exchange(work.transfer);
                CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
                continue;
            }
            const O::RequestId id = b.admit(work.transfer);
            b.turn(b.now);
            b.deliver();
            b.turn(b.now + 100, budget(8, 0, 8, 0));
            const O::Completion* terminal = b.owner.result(id);
            CHECK(terminal && terminal->service.outcome == C::SdoOutcome::TIMEOUT);
            CHECK(terminal->service.effectUncertain && terminal->service.transmissionPossible);
            CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
            CHECK(c.state == L::SettingsState::FAILED &&
                  c.outcome == M::ReadOutcome::SERVICE_TIMEOUT && c.uncertain);
            CHECK(c.confirmed == (1u << at) - 1);
            // The reply is late and the route is unresolved: the result does not change.
            const Saved<O::Completion> retained(*terminal);
            M::Frame late;
            late.id = 0x581;
            late.dlc = late.payloadLength = 8;
            late.data[0] = 0x60;
            late.data[1] = uint8_t(objects[at]);
            late.data[2] = uint8_t(objects[at] >> 8);
            b.wire.push(late, b.now + 1);
            b.turn(b.now + 1, budget(8, 0, 8, 0));
            retained.unchanged(*b.owner.result(id));
            CHECK(b.services.unresolved(0) && !b.owner.sequence(b.sequence));
            // New work on the route cannot go out. Nothing replayed the write.
            O::SequenceId another;
            CHECK(!b.owner.beginSequence(route(1).binding, 2, b.now, another));
            CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
            unsigned writes = 0;
            for (unsigned f = 0; f < b.wire.sentCount; ++f)
                if (b.wire.sent[f].id == 0x601 && b.wire.sent[f].data[0] == 0x23 &&
                    b.wire.sent[f].data[1] == uint8_t(objects[at]))
                    ++writes;
            CHECK(writes == 1);
            CHECK(b.owner.release(id));
        }
        b.wire.settings = nullptr;
    }
}

void readbackMismatchPublishesNothing() {
    { // The written field reads back otherwise, at every position of the position group.
        const uint16_t objects[] = {0x6081, 0x6083, 0x6084};
        for (unsigned at = 0; at < 3; ++at) {
            Bench b;
            MotionEvidence e(b);
            L::DriverSettingsContext c;
            CHECK(prepareMotion(c, b, e, motion(3000, 2000, 2000)));
            b.drive.lieIndex = objects[at];
            b.drive.lieRaw = 7;
            b.run(c);
            CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR);
            CHECK(c.confirmed == 7 && c.readback.read.values[at].raw == 7);
            L::DriverObservation seen;
            const Saved<L::DriverObservation> none(seen);
            CHECK(!L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
            none.unchanged(seen);
        }
    }
    // The same for each retained 607D word after a 2202 write.
    for (uint8_t sub = 0; sub < 3; ++sub) {
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        b.drive.lieIndex = 0x607d;
        b.drive.lieSubindex = sub;
        b.drive.lieRaw = 3;
        b.run(c);
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR);
    }
    { // A failed readback upload keeps its prefix and confirms nothing more.
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        b.drive.inject(Drive::ABORT, 0x607d, 1, false);
        b.run(c);
        CHECK(c.state == L::SettingsState::FAILED && c.confirmed == 1);
        CHECK(b.drive.value(0x2202) == 1);
    }
}

void cancellationBeforeAndAfterHandoff() {
    { // Cancelled before any possible transmission: no epoch change and no uncertainty.
        Bench b;
        MotionEvidence e(b);
        M::AxisConfig axis;
        const M::PreparedTarget prepared = preparedTarget(b, axis);
        L::DriverSettingsContext c;
        CHECK(prepareMotion(c, b, e, motion()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        const unsigned sent = b.mark();
        CHECK(b.owner.cancel(id, b.now) == O::Cancel::CANCELLED);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && !terminal->service.transmissionPossible);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::CANCELLED);
        CHECK(!c.effectsPossible && !c.uncertain && !c.confirmed);
        CHECK(b.wire.current.configurationGeneration == 7);
        b.turn(b.now);
        b.checkSilentSince(sent);
        axis.binding = b.wire.current;
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A refused preflight is the same: the notice never ran.
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        b.wire.settings = &c;
        b.wire.rejectGuard = true;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const unsigned sent = b.mark();
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->reason == O::CompletionReason::PREFLIGHT_REJECTED);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(!c.effectsPossible && !c.uncertain && b.wire.current.configurationGeneration == 7);
        b.checkSilentSince(sent);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
        b.wire.rejectGuard = false;
    }
    { // Cancelled after handoff: the effect is uncertain and the epoch stays advanced.
        Bench b;
        MotionEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareMotion(c, b, e, motion()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        CHECK(b.owner.mayHaveTransmitted(id));
        CHECK(b.owner.cancel(id, b.now) == O::Cancel::CANCELLED);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.effectUncertain);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.uncertain && c.effectsPossible);
        CHECK(b.wire.current.configurationGeneration == 8 && b.services.unresolved(0));
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // Local withdrawal during readback: acknowledged writes survive as history. Not a motor stop.
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        CHECK(c.state == L::SettingsState::READING_BACK && c.confirmed == 1);
        CHECK(L::advanceDriver(c, M::ReadOutcome::CANCELLED, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::CANCELLED);
        CHECK(c.confirmed == 1 && b.drive.value(0x2202) == 1);
        const unsigned sent = b.mark();
        b.turn(b.now + 1);
        b.checkSilentSince(sent);
        b.wire.settings = nullptr;
    }
}

void rebootEndsTheTransaction() {
    const auto boot = []() {
        M::Frame f;
        f.id = 0x701;
        f.dlc = f.payloadLength = 1;
        return f;
    };
    { // During a write: uncertain, reservation gone, and the context is withdrawn locally.
        Bench b;
        MotionEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareMotion(c, b, e, motion()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        b.wire.push(boot(), b.now + 1);
        b.turn(b.now + 1, budget(1, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.outcome == C::SdoOutcome::REBOOT);
        CHECK(terminal->service.effectUncertain && !b.owner.sequence(b.sequence));
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::INVALIDATED);
        CHECK(c.uncertain && !c.confirmed);
        // No queued work, no retry, and the route stays closed to a new reservation.
        O::SequenceId another;
        CHECK(!b.owner.beginSequence(route(1).binding, 1, b.now, another));
        CHECK(b.services.unresolved(0));
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // During readback: the acknowledged write stays recorded and nothing is published.
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        b.wire.push(boot(), b.now + 1);
        b.turn(b.now + 1, budget(1, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.outcome == C::SdoOutcome::REBOOT);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.confirmed == 1);
        L::DriverObservation seen;
        const Saved<L::DriverObservation> none(seen);
        CHECK(!L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
        none.unchanged(seen);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
}

void deadlineWins() {
    { // The overall cutoff reached while a write waits: not a timeout, and the effect is uncertain.
        Bench b;
        MotionEvidence e(b);
        const uint64_t deadline = b.now + 50;
        L::DriverSettingsContext c;
        CHECK(prepareMotion(c, b, e, motion(), deadline));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        // Before the cutoff a local deadline claim is refused and changes nothing.
        const Saved<L::DriverSettingsContext> saved(c);
        CHECK(!L::advanceDriver(c, M::ReadOutcome::DEADLINE, b.now));
        saved.unchanged(c);
        b.turn(deadline, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::DEADLINE);
        CHECK(c.uncertain && c.effectsPossible);
        CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) || !work.ready);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // At the cutoff nothing further is yielded, during writes or readback.
        Bench b;
        SelectorEvidence e(b);
        const uint64_t deadline = b.now + 5;
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1, deadline));
        L::PreparedRead work;
        const Saved<L::PreparedRead> none(work);
        CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, deadline, work));
        none.unchanged(work);
        M::Binding binding = b.wire.current;
        CHECK(!L::noteDriverTransmission(c, b.wire.live, binding, deadline));
        CHECK(binding.configurationGeneration == 7);
        CHECK(L::advanceDriver(c, M::ReadOutcome::DEADLINE, deadline));
        CHECK(c.outcome == M::ReadOutcome::DEADLINE && !c.uncertain);
    }
    { // A readback that arrives on or after the cutoff does not publish.
        Bench b;
        SelectorEvidence e(b);
        const uint64_t deadline = b.now + 6;
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1, deadline));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        b.turn(deadline, budget(8, 0, 8, 0));
        CHECK(L::advanceDriver(c, M::ReadOutcome::DEADLINE, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::DEADLINE);
        CHECK(c.confirmed == 1 && c.acknowledgedUs[0] <= b.now);
        L::DriverObservation seen;
        CHECK(!L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
        b.wire.settings = nullptr;
    }
}

// Packet 10's distinction, end to end: explicit settings advance the generation, and a move's own
// writes retire retained parameters without moving it, so they cannot stale their own target.
void generationsAndOldTargetAdmission() {
    { // An explicit call stales an earlier target from its first possible download, and only once.
        Bench b;
        SelectorEvidence e(b);
        M::AxisConfig axis;
        const M::PreparedTarget prepared = preparedTarget(b, axis);
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        // Preparation alone changes no epoch.
        axis.binding = b.wire.current;
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        CHECK(b.wire.current.configurationGeneration == 7);
        b.turn(b.now);
        CHECK(b.wire.current.configurationGeneration == 8);
        axis.binding = b.wire.current;
        CHECK(!M::validatePreparedTarget(prepared, axis, nullptr, b.now));
        // New-epoch work is refused until the old result is released and the reservation moved.
        CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        b.deliver();
        b.turn(b.now + 1, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(b.owner.release(id));
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        CHECK(work.transfer.channel.binding.configurationGeneration == 8);
        O::BusRequest early;
        early.sequence = b.sequence;
        early.transfer = work.transfer;
        O::RequestId rejected;
        CHECK(b.owner.admit(early, b.now, rejected) == O::BusAdmission::INVALID);
        CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        CHECK(b.owner.sequence(b.sequence)->binding.configurationGeneration == 8);
        // Readback runs at the new epoch and publishes only there.
        while (c.state == L::SettingsState::READING_BACK) {
            CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
            const C::SdoCompletion reply = b.exchange(work.transfer);
            CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, reply, b.now));
        }
        CHECK(c.state == L::SettingsState::SUCCEEDED);
        CHECK(b.wire.notices == 1);
        L::DriverObservation seen;
        CHECK(L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
        CHECK(seen.target.channel.binding.configurationGeneration == 8);
        // Earlier group evidence is now refused.
        CHECK(!L::getDriver(e.limits, b.current(), b.now, AGE, seen));
        b.wire.settings = nullptr;
    }
    for (uint16_t index : {uint16_t(0x6081), uint16_t(0x6083), uint16_t(0x6084)}) {
        Bench b;
        MotionEvidence e(b);
        L::DriverContext finished;
        { // A finished settings context's readback is also a retained observation.
            L::DriverSettingsContext first;
            CHECK(prepareMotion(first, b, e, motion(70, 110, 120)));
            b.run(first);
            CHECK(first.state == L::SettingsState::SUCCEEDED);
            finished = first.readback;
            b.renew();
        }
        L::DriverContext prior = finished;
        M::AxisConfig axis;
        const M::PreparedTarget prepared = preparedTarget(b, axis);
        // The move's own write, under its own operation on the real owner.
        C::SdoTransfer own;
        own.channel = b.current().channel;
        own.operation = own.request = b.begin();
        own.step = 1;
        own.index = index;
        own.direction = C::SdoDirection::DOWNLOAD;
        own.size = 4;
        own.data[0] = 77;
        own.deadlineUs = DEADLINE;
        own.queueTimeoutUs = own.responseTimeoutUs = 100;
        b.wire.retained[0] = &prior;
        b.wire.retained[1] = &finished;
        const M::Binding binding = b.wire.current;
        const C::SdoCompletion ack = b.exchange(own);
        CHECK(ack.outcome == C::SdoOutcome::SUCCESS);
        b.wire.retained[0] = b.wire.retained[1] = nullptr;
        // The generation stands still and its own target stays valid.
        CHECK(b.wire.current.configurationGeneration == binding.configurationGeneration);
        CHECK(b.owner.sequence(b.sequence)->binding.configurationGeneration ==
              binding.configurationGeneration);
        axis.binding = b.wire.current;
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
        // Both retained observations are withdrawn, raw history kept, and no no-op can hide.
        CHECK(prior.read.invalidated && finished.read.invalidated);
        CHECK(prior.read.values[0].raw == 70);
        L::DriverObservation seen;
        const Saved<L::DriverObservation> none(seen);
        CHECK(!L::getDriver(prior, b.current(), b.now, AGE, seen));
        none.unchanged(seen);
        C::SdoCompletion control = b.control();
        L::ReadContext state;
        b.readState(state);
        const uint32_t op = b.begin();
        L::DriverSettingsContext blocked;
        refused(b, blocked, [&] {
            return L::prepareDriverSettings(blocked, b.wire.live, b.current(), prior, state, control,
                                            motion(70, 110, 120), op, b.now, DEADLINE, options(), AGE);
        });
        // A fresh read sees the move's value, and the same candidate is real work, not a no-op.
        L::DriverContext fresh;
        b.read(fresh, L::DriverGroup::MOTION_PARAMETERS);
        const uint32_t op2 = b.begin();
        L::DriverSettingsContext settings;
        CHECK(L::prepareDriverSettings(settings, b.wire.live, b.current(), fresh, state, control, motion(70, 110, 120),
                                       op2, b.now, DEADLINE, options(), AGE));
        CHECK(settings.state == L::SettingsState::WRITING &&
              settings.changed == (index == 0x6081 ? 1 : index == 0x6083 ? 2 : 4));
        // That explicit call, unlike the move's write, does stale the target.
        b.run(settings);
        CHECK(settings.state == L::SettingsState::SUCCEEDED);
        axis.binding = b.wire.current;
        CHECK(!M::validatePreparedTarget(prepared, axis, nullptr, b.now));
    }
    { // Queue rejection needs no notice and stales nothing.
        Bench b;
        SelectorEvidence e(b);
        M::AxisConfig axis;
        const M::PreparedTarget prepared = preparedTarget(b, axis);
        L::DriverSettingsContext c;
        CHECK(prepareLimit(c, b, e, 1));
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        O::BusRequest wrong;
        wrong.sequence = b.sequence;
        wrong.transfer = work.transfer;
        ++wrong.transfer.operation;
        O::RequestId rejected;
        CHECK(b.owner.admit(wrong, b.now, rejected) != O::BusAdmission::ACCEPTED);
        CHECK(b.wire.current.configurationGeneration == 7);
        axis.binding = b.wire.current;
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
    }
}

// No controlword, mode, save, reset, clear, DO, filter, output command or NMT frame since `first`:
// only single-word 2400:01..08 writes, their own 2400:00..08 reads, and a client abort cleanup.
void onlyDiTraffic(const Bench& b, unsigned first) {
    for (unsigned i = first; i < b.wire.sentCount; ++i) {
        const M::Frame& f = b.wire.sent[i];
        CHECK(f.id == 0x601);
        if (f.data[0] == 0x80)
            continue;
        CHECK(f.data[1] == 0x00 && f.data[2] == 0x24);
        if (f.data[0] == 0x40)
            CHECK(f.data[3] <= 8);
        else
            CHECK(f.data[0] == 0x2b && f.data[3] >= 1 && f.data[3] <= 8);
    }
}
unsigned writesTo(const Bench& b, uint8_t subindex) {
    unsigned writes = 0;
    for (unsigned i = 0; i < b.wire.sentCount; ++i)
        if (b.wire.sent[i].id == 0x601 && b.wire.sent[i].data[0] == 0x2b &&
            b.wire.sent[i].data[3] == subindex)
            ++writes;
    return writes;
}
void noPublication(const L::DriverSettingsContext& c, Bench& b) {
    L::IoObservation seen;
    const Saved<L::IoObservation> none(seen);
    CHECK(!L::getIo(c, b.wire.live, b.current(), b.now, AGE, seen));
    none.unchanged(seen);
}
M::Frame bootFrame() {
    M::Frame f;
    f.id = 0x701;
    f.dlc = f.payloadLength = 1;
    return f;
}

void diEveryCodeAndMaskOnTheWire() {
    for (unsigned input = 0; input < 7; ++input)
        for (uint16_t code = 0; code <= 13; ++code) {
            Bench b;
            L::DriverContext prior;
            b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
            const uint8_t sub = uint8_t(input + 2);
            const uint32_t held = b.drive.value(0x2400, sub);
            L::IoSettings s = asFound();
            s.functions[input] = static_cast<L::InputFunction>(code);
            L::DriverSettingsContext c;
            const unsigned first = b.mark();
            CHECK(prepareIo(c, b, prior, s));
            if (held == code) { // Unchanged: fresh prior evidence, no traffic and no epoch change.
                CHECK(c.state == L::SettingsState::SUCCEEDED && !c.changed && !c.effectsPossible);
                b.turn(b.now);
                b.checkSilentSince(first);
                CHECK(b.wire.current.configurationGeneration == 7);
            } else {
                CHECK(c.changed == (1u << sub));
                b.run(c);
                CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == c.changed);
                CHECK(!c.uncertain && b.wire.notices == 1);
                CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2400, sub, code));
                CHECK(b.wire.sentCount == first + 10);
                for (uint8_t field = 0; field < 9; ++field)
                    CHECK(frameIs(b.wire.sent[first + 1 + field], 0x40, 0x2400, field, 0));
                onlyDiTraffic(b, first);
                CHECK(b.wire.current.configurationGeneration == 8);
                CHECK(c.readback.read.target.channel.binding.configurationGeneration == 8);
            }
            CHECK(b.drive.value(0x2400, sub) == code);
            L::IoObservation seen;
            CHECK(L::getIo(c, b.wire.live, b.current(), b.now, AGE, seen));
            CHECK(seen.inputFunctionRaw[input] == code &&
                  seen.inputFunctions[input] == static_cast<L::InputFunction>(code));
            CHECK(!seen.wiringKnown && !seen.electricalLevelsKnown && !seen.outputMappingKnown);
        }
    // Polarity at each documented bit and both ends, from words holding unexplained high bits.
    for (uint32_t word : {0x0000u, 0xa500u, 0xff80u, 0x8000u, 0x0080u, 0x007fu, 0xffffu})
        for (uint16_t mask : {uint16_t(0), uint16_t(1), uint16_t(2), uint16_t(4), uint16_t(8),
                              uint16_t(0x10), uint16_t(0x20), uint16_t(0x40), uint16_t(0x3f),
                              uint16_t(0x7f)}) {
            Bench b;
            b.drive.set(0x2400, 1, word);
            L::DriverContext prior;
            b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
            L::IoSettings s = asFound();
            s.invertedInputs = mask;
            const uint32_t expected = (word & 0xff80u) | mask;
            L::DriverSettingsContext c;
            const unsigned first = b.mark();
            CHECK(prepareIo(c, b, prior, s));
            CHECK(c.requested[1] == expected);
            b.run(c);
            CHECK(c.state == L::SettingsState::SUCCEEDED);
            if (expected == word) {
                b.checkSilentSince(first);
            } else {
                CHECK(c.changed == 2);
                CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2400, 1, expected));
            }
            CHECK(b.drive.value(0x2400, 1) == expected);
            L::IoObservation seen;
            CHECK(L::getIo(c, b.wire.live, b.current(), b.now, AGE, seen));
            CHECK(seen.inputPolarityRaw == expected);
            for (unsigned bit = 0; bit < 7; ++bit)
                CHECK(seen.inputInverted[bit] == bool(mask & (1u << bit)));
        }
    // Both documented models send the same words.
    for (L::SettingsModel model : {L::SettingsModel::CL86_C, L::SettingsModel::OL86_C}) {
        Bench b(model);
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        const unsigned first = b.mark();
        CHECK(prepareIo(c, b, prior, everyField()));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == 0x1fe);
        CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2400, 1, 0xa555));
        for (uint8_t i = 0; i < 7; ++i)
            CHECK(frameIs(b.wire.sent[first + 1 + i], 0x2b, 0x2400, uint8_t(i + 2), 13u - i));
        onlyDiTraffic(b, first);
    }
}

void diCandidatesRefusedWhole() {
    Bench b;
    L::DriverContext prior;
    b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
    L::DriverSettingsContext c;
    c.operation = 0xbeef;
    const uint32_t op = b.begin();
    const auto call = [&](const L::IoSettings& s) {
        return L::prepareIoSettings(c, b.wire.live, b.current(), prior, s, op, b.now, DEADLINE, options(), AGE);
    };
    // Bit 7 would name an eighth terminal, and none exists; the others are undocumented.
    for (uint16_t mask : {uint16_t(0x80), uint16_t(0x100), uint16_t(0x8000), uint16_t(0xff),
                          uint16_t(0xff80), uint16_t(0xffff)}) {
        L::IoSettings s = asFound();
        s.invertedInputs = mask;
        refused(b, c, [&] { return call(s); });
    }
    for (unsigned input = 0; input < 7; ++input)
        for (uint16_t code : {uint16_t(14), uint16_t(15), uint16_t(255), uint16_t(0x100),
                              uint16_t(0x7fff), uint16_t(0x8000), uint16_t(0xfffe),
                              uint16_t(0xffff)}) {
            L::IoSettings s = everyField();
            s.functions[input] = static_cast<L::InputFunction>(code);
            refused(b, c, [&] { return call(s); });
        }
    // Defaults refuse, and so does each field left at its default in an otherwise complete candidate.
    refused(b, c, [&] { return call(L::IoSettings()); });
    {
        L::IoSettings s = everyField();
        s.invertedInputs = L::IoSettings().invertedInputs;
        refused(b, c, [&] { return call(s); });
    }
    for (unsigned input = 0; input < 7; ++input) {
        L::IoSettings s = everyField();
        s.functions[input] = L::IoSettings().functions[input];
        refused(b, c, [&] { return call(s); });
    }
    // The same evidence admits a complete candidate, so the refusals above isolate their values.
    CHECK(call(everyField()) && c.changed == 0x1fe);
    { // An undocumented stored code stays raw and is replaced only by an explicit documented one.
        Bench u;
        u.drive.set(0x2400, 5, 99);
        L::DriverContext held;
        u.read(held, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::IoObservation seen;
        CHECK(L::getIo(held, u.current(), u.now, AGE, seen));
        CHECK(seen.inputFunctionRaw[3] == 99 && seen.inputFunctions[3] == L::InputFunction::UNKNOWN);
        L::IoSettings s = asFound();
        s.functions[3] = static_cast<L::InputFunction>(99);
        L::DriverSettingsContext out;
        refused(u, out, [&] { return prepareIo(out, u, held, s); });
        s = asFound();
        const unsigned first = u.mark();
        CHECK(prepareIo(out, u, held, s));
        CHECK(out.changed == (1u << 5));
        u.run(out);
        CHECK(frameIs(u.wire.sent[first], 0x2b, 0x2400, 5, 0) && u.drive.value(0x2400, 5) == 0);
    }
    // No other family reaches this write, whatever value a caller forces into the model.
    for (L::SettingsModel model : {L::SettingsModel::UNKNOWN, static_cast<L::SettingsModel>(3),
                                   static_cast<L::SettingsModel>(255)}) {
        L::AxisReservation live = b.wire.live;
        live.model = model;
        L::DriverSettingsContext out;
        refused(b, out, [&] {
            return L::prepareIoSettings(out, live, live.target, prior, everyField(), op, b.now, DEADLINE, options(),
                                        AGE);
        });
    }
}

// Host order is not atomic: each write can act while the next is unsent. Uniqueness is not a
// documented rule, so the duplicate this passes through, and a requested duplicate, both stand.
void diOrderIsNotAtomicAndDuplicatesStand() {
    Bench b;
    L::DriverContext prior;
    b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
    L::IoSettings s = asFound();
    s.functions[0] = L::InputFunction::NEGATIVE_LIMIT; // DI0 and DI2 exchange 1 and 3.
    s.functions[2] = L::InputFunction::ORIGIN;
    L::DriverSettingsContext c;
    const unsigned first = b.mark();
    CHECK(prepareIo(c, b, prior, s));
    CHECK(c.changed == ((1u << 2) | (1u << 4)));
    b.wire.settings = &c;
    L::PreparedRead work;
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const C::SdoCompletion ack = b.exchange(work.transfer);
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
    CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2400, 2, 3));
    CHECK(b.drive.value(0x2400, 2) == 3 && b.drive.value(0x2400, 4) == 3);
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const C::SdoCompletion second = b.exchange(work.transfer);
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, second, b.now));
    CHECK(frameIs(b.wire.sent[first + 1], 0x2b, 0x2400, 4, 1));
    b.run(c);
    CHECK(c.state == L::SettingsState::SUCCEEDED && writesTo(b, 3) == 0);
    Bench d;
    L::DriverContext held;
    d.read(held, L::DriverGroup::IO_INPUT_CONFIGURATION);
    L::IoSettings duplicate;
    duplicate.invertedInputs = 0;
    for (L::InputFunction& f : duplicate.functions)
        f = L::InputFunction::ORIGIN;
    L::DriverSettingsContext two;
    CHECK(prepareIo(two, d, held, duplicate));
    d.run(two);
    CHECK(two.state == L::SettingsState::SUCCEEDED);
    for (uint8_t sub = 2; sub <= 8; ++sub)
        CHECK(d.drive.value(0x2400, sub) == 1);
}

void diWrongEvidenceRefused() {
    // A complete read of any other group is not the input configuration.
    for (L::DriverGroup group :
         {L::DriverGroup::IO_OUTPUT_CONFIGURATION, L::DriverGroup::IO_INPUT_FILTERS,
          L::DriverGroup::IO_OUTPUT_COMMAND, L::DriverGroup::DRIVER, L::DriverGroup::SOFT_LIMITS,
          L::DriverGroup::MOTION_PARAMETERS}) {
        Bench b;
        L::DriverContext wrong;
        b.read(wrong, group);
        L::DriverSettingsContext c;
        refused(b, c, [&] { return prepareIo(c, b, wrong, everyField()); });
    }
    { // The empty context, and an incomplete or failed read.
        Bench b;
        L::DriverContext empty;
        L::DriverSettingsContext c;
        refused(b, c, [&] { return prepareIo(c, b, empty, everyField()); });
        L::DriverContext partial;
        const uint32_t op = b.begin();
        CHECK(L::prepareDriverRead(partial, b.current(), op, b.now, DEADLINE, options(),
                                   L::DriverGroup::IO_INPUT_CONFIGURATION));
        for (unsigned i = 0; i < 5; ++i) {
            L::PreparedRead work;
            CHECK(L::nextDriver(partial, b.now, work) && work.ready);
            const C::SdoCompletion terminal = b.exchange(work.transfer);
            CHECK(L::advanceDriver(partial, terminal, b.now));
        }
        refused(b, c, [&] {
            return L::prepareIoSettings(c, b.wire.live, b.current(), partial, everyField(), op, b.now,
                                        DEADLINE, options(), AGE);
        });
        b.drive.inject(Drive::ABORT, 0x2400, 5, false);
        L::PreparedRead failing;
        CHECK(L::nextDriver(partial, b.now, failing) && failing.ready);
        const C::SdoCompletion aborted = b.exchange(failing.transfer);
        CHECK(L::advanceDriver(partial, aborted, b.now));
        CHECK(partial.read.state == M::ReadState::FAILED);
        refused(b, c, [&] {
            return L::prepareIoSettings(c, b.wire.live, b.current(), partial, everyField(), op, b.now,
                                        DEADLINE, options(), AGE);
        });
    }
    enum Case { NO_OWNER, NO_SEQUENCE, NO_GENERATION, BEFORE_RESERVATION, OTHER_NODE, INVALIDATED,
                EXPIRED, NO_OPERATION, DEADLINE_NOW, NO_QUEUE, NO_RESPONSE, NO_AGE, EXHAUSTED, CASES };
    for (unsigned k = 0; k < CASES; ++k) {
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::AxisReservation live = b.wire.live;
        uint32_t op = b.begin();
        uint64_t deadline = DEADLINE, age = AGE;
        L::ReadOptions policy = options();
        if (k == NO_OWNER)
            live.owner = 0;
        if (k == NO_SEQUENCE)
            live.sequence = 0;
        if (k == NO_GENERATION)
            live.generation = 0;
        if (k == BEFORE_RESERVATION)
            live.acquiredUs = prior.read.startedUs + 1;
        if (k == OTHER_NODE)
            ++live.target.nodeGeneration;
        if (k == INVALIDATED)
            prior.read.invalidated = true;
        if (k == EXPIRED)
            b.turn(b.now + AGE + 1);
        if (k == NO_OPERATION)
            op = 0;
        if (k == DEADLINE_NOW)
            deadline = b.now;
        if (k == NO_QUEUE)
            policy.queueTimeoutUs = 0;
        if (k == NO_RESPONSE)
            policy.responseTimeoutUs = 0;
        if (k == NO_AGE)
            age = 0;
        if (k == EXHAUSTED)
            live.target.channel.binding.configurationGeneration = UINT32_MAX;
        L::DriverSettingsContext c;
        refused(b, c, [&] {
            return L::prepareIoSettings(c, live, live.target, prior, everyField(), op, b.now, deadline, policy, age);
        });
    }
    { // The exact age limit is inclusive, one microsecond later is not.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        b.turn(prior.read.values[0].provenance.earliestUs + AGE);
        CHECK(prepareIo(c, b, prior, everyField()));
        b.turn(b.now + 1);
        L::DriverSettingsContext late;
        refused(b, late, [&] { return prepareIo(late, b, prior, everyField()); });
    }
}

void diAliasedAndEditedContexts() {
    Bench b;
    L::DriverContext prior;
    b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
    L::DriverSettingsContext c;
    L::IoSettings s = everyField();
    CHECK(prepareIo(c, b, prior, s));
    const uint32_t op = b.operation;
    const Saved<L::DriverSettingsContext> saved(c);
    L::ReadOptions policy = options();
    const auto aliased = [&](M::Status status) {
        CHECK(!status && status.detail == int32_t(M::ReadError::OVERLAP));
        saved.unchanged(c);
    };
    aliased(L::prepareIoSettings(c, c.reservation, b.current(), prior, s, op, b.now, DEADLINE, policy, AGE));
    aliased(L::prepareIoSettings(c, b.wire.live, c.reservation.target, prior, s, op, b.now, DEADLINE,
                                 policy, AGE));
    aliased(L::prepareIoSettings(c, b.wire.live, b.current(), c.readback, s, op, b.now, DEADLINE, policy, AGE));
    // `readback` is larger than the candidate, so the reference stays inside its object.
    aliased(L::prepareIoSettings(c, b.wire.live, b.current(), prior, at<L::IoSettings>(&c.readback), op, b.now,
                                 DEADLINE, policy, AGE));
    aliased(L::prepareIoSettings(c, b.wire.live, b.current(), prior, s, op, b.now, DEADLINE,
                                 c.readback.read.options, AGE));
    // A hand-edited context refuses rather than send an undocumented code or a wider word.
    L::PreparedRead edited;
    const Saved<L::PreparedRead> blank(edited);
    for (unsigned field = 2; field < 9; ++field) {
        const uint32_t held = c.requested[field];
        for (uint32_t bad : {14u, 0x8000u, 0x1000du}) {
            c.requested[field] = bad;
            CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, b.now, edited));
        }
        c.requested[field] = held;
    }
    for (unsigned field = 0; field < 2; ++field) {
        const uint32_t held = c.requested[field];
        c.requested[field] = 0x10000u | held;
        CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, b.now, edited));
        c.requested[field] = held;
    }
    // The count, and subindices beyond the DI group, are never selectable.
    const uint16_t changed = c.changed;
    for (uint16_t extra : {uint16_t(1), uint16_t(1u << 9), uint16_t(1u << 12)}) {
        c.changed = uint16_t(changed | extra);
        CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, b.now, edited));
    }
    c.changed = changed;
    // The output groups have no write path: a context retargeted at one refuses.
    const L::ReadKind kind = c.readback.read.kind;
    for (L::ReadKind other : {L::ReadKind::IO_OUTPUT_CONFIGURATION, L::ReadKind::IO_INPUT_FILTERS,
                              L::ReadKind::IO_OUTPUT_COMMAND}) {
        c.readback.read.kind = other;
        CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, b.now, edited));
    }
    c.readback.read.kind = kind;
    blank.unchanged(edited);
    saved.unchanged(c);
    // The candidate cannot reach the context once prepared, nor the context a queued request.
    s = asFound();
    b.wire.settings = &c;
    L::PreparedRead work;
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const O::RequestId id = b.admit(work.transfer);
    work.transfer.data[0] = 0xee;
    const uint32_t held = c.requested[1];
    c.requested[1] = 1;
    const unsigned first = b.mark();
    b.turn(b.now);
    CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2400, 1, 0xa555));
    c.requested[1] = held;
    b.deliver();
    b.turn(b.now + 1, budget(8, 0, 8, 0));
    const O::Completion* terminal = b.owner.result(id);
    CHECK(terminal);
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
    CHECK(b.owner.release(id));
    CHECK(b.drive.value(0x2400, 1) == 0xa555 && c.confirmed == 2);
    b.wire.settings = nullptr;
}

void diPartialEffectsSurvive() {
    for (unsigned position = 0; position < 8; ++position) {
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        const uint32_t before[9] = {16, 0xa500, 1, 2, 3, 0, 0, 0, 0};
        L::DriverSettingsContext c;
        const unsigned first = b.mark();
        CHECK(prepareIo(c, b, prior, everyField()));
        const uint8_t sub = uint8_t(position + 1);
        b.drive.inject(Drive::ABORT, 0x2400, sub, true);
        b.run(c);
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
        CHECK(c.effectsPossible && c.uncertain);
        CHECK(c.confirmed == uint16_t(((1u << sub) - 1u) & 0x1fe));
        CHECK(c.evidencePresent && c.evidence.abortValid && c.evidence.abortCode == 0x08000022u);
        CHECK(c.requested[1] == 0xa555);
        for (uint8_t i = 1; i <= 8; ++i) {
            // Written fields stay written, with no rollback. Nothing after the failure went out.
            const uint32_t wanted = i == 1 ? 0xa555u : 13u - (i - 2u);
            CHECK(b.drive.value(0x2400, i) == (i < sub ? wanted : before[i]));
            CHECK((c.acknowledgedUs[i] != 0) == (i < sub));
            CHECK(writesTo(b, i) == (i <= sub ? 1u : 0u));
        }
        onlyDiTraffic(b, first);
        CHECK(b.wire.current.configurationGeneration == 8 && b.wire.notices == position + 1u);
        noPublication(c, b);
        L::PreparedRead work;
        work.ready = true;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && !work.ready);
        const unsigned sent = b.mark();
        b.turn(b.now + 10);
        b.checkSilentSince(sent);
        // Old evidence is stale, and a new explicit operation on a fresh read sends only the rest.
        L::IoObservation stale;
        CHECK(!L::getIo(prior, b.current(), b.now, AGE, stale));
        CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        b.renew();
        b.drive.fault = Drive::NONE;
        L::DriverContext fresh;
        b.read(fresh, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext rest;
        CHECK(prepareIo(rest, b, fresh, everyField()));
        CHECK(rest.changed == uint16_t(0x1fe & ~((1u << sub) - 1u)));
        b.run(rest);
        CHECK(rest.state == L::SettingsState::SUCCEEDED);
        for (uint8_t i = 1; i < sub; ++i)
            CHECK(writesTo(b, i) == 1);
    }
}

void diLostReplyIsNeverReplayed() {
    for (unsigned position = 0; position < 8; ++position) {
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        const uint8_t sub = uint8_t(position + 1);
        b.wire.settings = &c;
        b.drive.inject(Drive::LOST, 0x2400, sub, true);
        for (unsigned i = 0; i <= position; ++i) {
            L::PreparedRead work;
            CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
            if (i < position) {
                const C::SdoCompletion ack = b.exchange(work.transfer);
                CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
                continue;
            }
            const O::RequestId id = b.admit(work.transfer);
            b.turn(b.now);
            b.deliver();
            b.turn(b.now + 100, budget(8, 0, 8, 0));
            const O::Completion* terminal = b.owner.result(id);
            CHECK(terminal && terminal->service.outcome == C::SdoOutcome::TIMEOUT);
            CHECK(terminal->service.effectUncertain);
            CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
            CHECK(c.state == L::SettingsState::FAILED && c.uncertain);
            CHECK(c.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
            CHECK(c.confirmed == uint16_t(((1u << sub) - 1u) & 0x1fe));
            // The route is unresolved: the write reached the dictionary, and nothing resends it.
            CHECK(b.services.unresolved(0) && !b.owner.sequence(b.sequence));
            O::SequenceId another;
            CHECK(!b.owner.beginSequence(route(1).binding, 2, b.now, another));
            CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
            b.turn(b.now + 500);
            CHECK(writesTo(b, sub) == 1);
            for (uint8_t later = uint8_t(sub + 1); later <= 8; ++later)
                CHECK(writesTo(b, later) == 0);
            noPublication(c, b);
            CHECK(b.owner.release(id));
        }
        b.wire.settings = nullptr;
    }
}

void diReadbackMismatchAndFailure() {
    // Every field of the group, including the count and the unwritten words of a sparse candidate.
    for (uint8_t sub = 0; sub <= 8; ++sub) {
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::IoSettings s = asFound();
        s.functions[6] = L::InputFunction::USER6; // Writes only 2400:08.
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, s));
        b.drive.lieIndex = 0x2400;
        b.drive.lieSubindex = sub;
        b.drive.lieRaw = c.requested[sub] ^ 1u;
        b.run(c);
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR);
        CHECK(c.confirmed == (1u << 8) && c.readback.read.values[sub].raw == (c.requested[sub] ^ 1u));
        noPublication(c, b);
        CHECK(writesTo(b, 8) == 1); // No write repeats to repair it.
    }
    for (uint8_t sub = 0; sub <= 8; ++sub) {
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.drive.inject(Drive::ABORT, 0x2400, sub, false);
        b.run(c);
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
        CHECK(c.confirmed == 0x1fe && b.drive.value(0x2400, 1) == 0xa555);
        noPublication(c, b);
    }
}

void diCancellationAndPreflight() {
    { // Cancelled before any possible transmission: no epoch change and no uncertainty.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        M::AxisConfig axis;
        const M::PreparedTarget prepared = preparedTarget(b, axis);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const unsigned sent = b.mark();
        const O::RequestId id = b.admit(work.transfer);
        CHECK(b.owner.cancel(id, b.now) == O::Cancel::CANCELLED);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && !terminal->service.transmissionPossible);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::CANCELLED);
        CHECK(!c.effectsPossible && !c.uncertain && !c.confirmed);
        CHECK(b.wire.current.configurationGeneration == 7);
        b.turn(b.now);
        b.checkSilentSince(sent);
        axis.binding = b.wire.current;
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A preflight refusal on the second download: the prefix stays, and nothing new is uncertain.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        b.wire.rejectGuard = true;
        const unsigned sent = b.mark();
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->reason == O::CompletionReason::PREFLIGHT_REJECTED);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.confirmed == 2 && !c.uncertain);
        CHECK(c.effectsPossible && b.wire.current.configurationGeneration == 8);
        b.checkSilentSince(sent);
        CHECK(writesTo(b, 2) == 0);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
        b.wire.rejectGuard = false;
    }
    { // The controller refuses the frame after the notice: it stays uncertain, and nothing resends.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.wire.accept = false;
        b.turn(b.now);
        CHECK(b.wire.notices == 1 && b.wire.current.configurationGeneration == 8);
        b.turn(b.now + 1, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.uncertain && c.effectsPossible && !c.confirmed);
        noPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // Cancelled after handoff: uncertain, and the epoch stays advanced.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        CHECK(b.owner.mayHaveTransmitted(id));
        CHECK(b.owner.cancel(id, b.now) == O::Cancel::CANCELLED);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.effectUncertain);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.uncertain && c.effectsPossible);
        CHECK(b.wire.current.configurationGeneration == 8 && b.services.unresolved(0));
        noPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A local withdrawal during readback: not a rollback, and the writes stay as history.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::IoSettings s = asFound();
        s.functions[6] = L::InputFunction::USER6;
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, s));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        CHECK(c.state == L::SettingsState::READING_BACK);
        CHECK(L::advanceDriver(c, M::ReadOutcome::CANCELLED, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.confirmed == (1u << 8));
        CHECK(b.drive.value(0x2400, 8) == 13);
        const unsigned sent = b.mark();
        b.turn(b.now + 1);
        b.checkSilentSince(sent);
        noPublication(c, b);
        b.wire.settings = nullptr;
    }
}

void diDeadlines() {
    { // The cutoff reached while a write waits: not a timeout, and the effect stays uncertain.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        const uint64_t deadline = b.now + 50;
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField(), deadline));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        b.turn(deadline, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::DEADLINE);
        CHECK(c.uncertain && c.effectsPossible && !c.confirmed);
        noPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // At the cutoff nothing more is yielded or noted.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        const uint64_t deadline = b.now + 5;
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField(), deadline));
        L::PreparedRead work;
        const Saved<L::PreparedRead> none(work);
        CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, deadline, work));
        none.unchanged(work);
        M::Binding binding = b.wire.current;
        CHECK(!L::noteDriverTransmission(c, b.wire.live, binding, deadline));
        CHECK(binding.configurationGeneration == 7);
        CHECK(L::advanceDriver(c, M::ReadOutcome::DEADLINE, deadline));
        CHECK(c.outcome == M::ReadOutcome::DEADLINE && !c.uncertain && !c.effectsPossible);
    }
    { // A deadline during readback publishes nothing.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::IoSettings s = asFound();
        s.functions[6] = L::InputFunction::USER6;
        const uint64_t deadline = b.now + 6;
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, s, deadline));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        b.turn(deadline, budget(8, 0, 8, 0));
        CHECK(L::advanceDriver(c, M::ReadOutcome::DEADLINE, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.confirmed == (1u << 8));
        noPublication(c, b);
        b.wire.settings = nullptr;
    }
}

void diFaultRebootAndBusOff() {
    { // A boot-up during a write: uncertain, reservation gone, and the route closed to new work.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        const unsigned first = b.mark();
        b.wire.push(bootFrame(), b.now + 1);
        b.turn(b.now + 1, budget(1, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.outcome == C::SdoOutcome::REBOOT);
        CHECK(terminal->service.effectUncertain && !b.owner.sequence(b.sequence));
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::INVALIDATED);
        CHECK(c.uncertain && c.confirmed == 2);
        noPublication(c, b);
        O::SequenceId another;
        CHECK(!b.owner.beginSequence(route(1).binding, 1, b.now, another));
        CHECK(b.owner.release(id));
        b.turn(b.now + 1);
        b.checkSilentSince(first); // No restore, no replay and no NMT followed the boot-up.
        b.wire.settings = nullptr;
    }
    { // A boot-up during readback: the acknowledged write is recorded, and nothing is published.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::IoSettings s = asFound();
        s.functions[6] = L::InputFunction::USER6;
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, s));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        b.wire.push(bootFrame(), b.now + 1);
        b.turn(b.now + 1, budget(1, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.outcome == C::SdoOutcome::REBOOT);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.confirmed == (1u << 8));
        noPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // Bus-off during a write: the work is invalidated and the effect stays uncertain.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        CHECK(prepareIo(c, b, prior, everyField()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const O::RequestId id = b.admit(work.transfer);
        b.turn(b.now);
        b.now += 1;
        CHECK(b.owner.reportController(C::ControllerState::BUS_OFF, b.now));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.effectUncertain);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.uncertain && !c.confirmed);
        noPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A drive fault event mid-transaction is observed, never cleared, and adds no frame.
        Bench b;
        L::DriverContext prior;
        b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
        L::DriverSettingsContext c;
        const unsigned first = b.mark();
        CHECK(prepareIo(c, b, prior, everyField()));
        b.wire.settings = &c;
        L::PreparedRead work;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
        M::Frame fault;
        fault.id = 0x81;
        fault.dlc = fault.payloadLength = 8;
        fault.data[0] = 0x01; // The bytes of FF01, an example; no meaning is asserted here.
        fault.data[1] = 0xff;
        b.wire.push(fault, b.now + 1);
        b.turn(b.now + 1, budget(8, 0, 8, 0));
        CHECK(b.services.node(0)->emcyCount == 1);
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == 0x1fe);
        onlyDiTraffic(b, first);
        b.wire.settings = nullptr;
    }
}

void diGenerationsAndExclusion() {
    Bench b;
    L::DriverContext prior;
    b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
    L::ReadContext state;
    b.readState(state);
    M::AxisConfig axis;
    const M::PreparedTarget prepared = preparedTarget(b, axis);
    L::DriverSettingsContext same;
    { // A no-op neither moves the generation nor stales anything.
        const unsigned sent = b.mark();
        CHECK(prepareIo(same, b, prior, asFound()));
        CHECK(same.state == L::SettingsState::SUCCEEDED && !same.changed);
        b.turn(b.now);
        b.checkSilentSince(sent);
        axis.binding = b.wire.current;
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
        L::IoObservation same2;
        CHECK(L::getIo(same, b.wire.live, b.current(), b.now, AGE, same2));
        CHECK(same2.inputPolarityRaw == 0xa500);
    }
    L::DriverSettingsContext c;
    CHECK(prepareIo(c, b, prior, everyField()));
    axis.binding = b.wire.current;
    CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now)); // Preparation alone moves nothing.
    // The route has one reservation: nothing else can begin on it while this one holds.
    O::SequenceId other;
    CHECK(!b.owner.beginSequence(route(1).binding, 2, b.now, other));
    b.wire.settings = &c;
    L::PreparedRead work;
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const O::RequestId id = b.admit(work.transfer);
    CHECK(b.wire.current.configurationGeneration == 7); // Admission alone moves nothing either.
    b.turn(b.now);
    CHECK(b.wire.current.configurationGeneration == 8 && b.wire.notices == 1);
    axis.binding = b.wire.current;
    CHECK(!M::validatePreparedTarget(prepared, axis, nullptr, b.now));
    // The earlier group and state evidence is now refused at the actual target.
    L::IoObservation seen;
    const Saved<L::IoObservation> none(seen);
    CHECK(!L::getIo(prior, b.current(), b.now, AGE, seen));
    none.unchanged(seen);
    L::StateObservation block;
    CHECK(!L::getStateBlock(state, L::StateBlock::STATUS, b.current(), b.now, AGE, block));
    // New-epoch work waits until the old result is consumed, released and the reservation moved.
    CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
    b.deliver();
    b.turn(b.now + 1, budget(8, 0, 8, 0));
    const O::Completion* terminal = b.owner.result(id);
    CHECK(terminal);
    CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
    CHECK(b.owner.release(id));
    // The remaining old-epoch writes keep the frozen epoch; the generation moved only the once.
    while (c.state == L::SettingsState::WRITING) {
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        CHECK(work.transfer.channel.binding.configurationGeneration == 7);
        const C::SdoCompletion ack = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
    }
    CHECK(b.wire.current.configurationGeneration == 8 && b.wire.notices == 8);
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    CHECK(work.transfer.channel.binding.configurationGeneration == 8);
    O::BusRequest early;
    early.sequence = b.sequence;
    early.transfer = work.transfer;
    O::RequestId rejected;
    CHECK(b.owner.admit(early, b.now, rejected) == O::BusAdmission::INVALID);
    CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
    while (c.state == L::SettingsState::READING_BACK) {
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
        const C::SdoCompletion reply = b.exchange(work.transfer);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, reply, b.now));
    }
    b.wire.settings = nullptr;
    CHECK(c.state == L::SettingsState::SUCCEEDED);
    CHECK(L::getIo(c, b.wire.live, b.current(), b.now, AGE, seen));
    CHECK(seen.target.channel.binding.configurationGeneration == 8);
    // Publication refuses the frozen token's target and a stale copy of the reservation.
    const Saved<L::IoObservation> published(seen);
    CHECK(!L::getIo(c, b.wire.live, b.reservation.target, b.now, AGE, seen));
    L::AxisReservation stale = b.wire.live;
    ++stale.sequence;
    CHECK(!L::getIo(c, stale, b.current(), b.now, AGE, seen));
    published.unchanged(seen);
    // A no-op prepared while current says SUCCEEDED, and publishes nothing now the epoch has moved.
    noPublication(same, b);
    CHECK(!L::nextDriver(same, b.wire.live, b.wire.current, b.now, work));
    // The frozen token cannot prepare one any more.
    L::DriverSettingsContext stalePlan;
    refused(b, stalePlan, [&] { return prepareIo(stalePlan, b, prior, asFound()); });
    // With a current token the same evidence is refused outright.
    b.renew();
    L::DriverSettingsContext next;
    refused(b, next, [&] { return prepareIo(next, b, prior, asFound()); });
    // Revocation: old copies of the reservation stop at the owner.
    Bench r;
    L::DriverContext held;
    r.read(held, L::DriverGroup::IO_INPUT_CONFIGURATION);
    L::DriverSettingsContext active;
    CHECK(prepareIo(active, r, held, everyField()));
    const L::AxisReservation before = r.wire.live;
    CHECK(r.owner.invalidate(r.sequence, r.now) && !r.owner.sequence(r.sequence));
    r.wire.live.owner = 0;
    CHECK(!L::nextDriver(active, r.wire.live, r.wire.current, r.now, work));
    O::BusRequest late;
    late.sequence = r.sequence;
    CHECK(L::nextDriver(active, before, r.wire.current, r.now, work) && work.ready);
    late.transfer = work.transfer;
    O::RequestId refusedId;
    CHECK(r.owner.admit(late, r.now, refusedId) != O::BusAdmission::ACCEPTED);
}

// Fixtures only, and no signal here qualifies hardware. The specimens' assignment with all logical
// levels zero and unconnected terminals is representable, and writing an unrelated input keeps it.
bool fixtureMethodPermits(const L::IoObservation& io, bool needsOrigin, bool signalKnown, bool active) {
    if (!needsOrigin)
        return true;
    for (unsigned i = 0; i < 7; ++i)
        if (io.inputFunctions[i] == L::InputFunction::ORIGIN)
            return signalKnown && active;
    return false;
}
void diUnusedAndMethodFixtures() {
    Bench b;
    b.drive.set(0x2400, 1, 0);
    L::DriverContext prior;
    b.read(prior, L::DriverGroup::IO_INPUT_CONFIGURATION);
    L::IoObservation before;
    CHECK(L::getIo(prior, b.current(), b.now, AGE, before));
    // A method that needs no origin is not held back by an unwired one. One that does needs its own
    // known active signal, whatever the register holds.
    CHECK(fixtureMethodPermits(before, false, false, false));
    CHECK(!fixtureMethodPermits(before, true, false, false));
    CHECK(!fixtureMethodPermits(before, true, true, false));
    CHECK(fixtureMethodPermits(before, true, true, true));
    CHECK(!before.wiringKnown && !before.electricalLevelsKnown);
    L::IoSettings s = asFound();
    s.functions[3] = L::InputFunction::USER0;
    L::DriverSettingsContext c;
    const unsigned first = b.mark();
    CHECK(prepareIo(c, b, prior, s));
    b.run(c);
    CHECK(c.state == L::SettingsState::SUCCEEDED && c.changed == (1u << 5));
    // The configured origin and limits stayed: nothing reassigned them on the way past.
    CHECK(writesTo(b, 2) == 0 && writesTo(b, 3) == 0 && writesTo(b, 4) == 0);
    CHECK(b.drive.value(0x2400, 2) == 1 && b.drive.value(0x2400, 3) == 2 &&
          b.drive.value(0x2400, 4) == 3);
    onlyDiTraffic(b, first);
    // Logical level is a separate read, and this scripted dictionary does not link the two.
    L::ReadContext state;
    b.renew();
    b.readState(state, uint8_t(1u << unsigned(L::StateBlock::MONITORS)));
    L::StateObservation monitors;
    CHECK(L::getStateBlock(state, L::StateBlock::MONITORS, b.current(), b.now, AGE, monitors));
    CHECK(monitors.logicalInputs == 0 && !monitors.interpreted);
    // A protective assignment changes only when a candidate asks for it.
    b.renew();
    L::DriverContext fresh;
    b.read(fresh, L::DriverGroup::IO_INPUT_CONFIGURATION);
    L::IoSettings cleared = s;
    cleared.functions[0] = L::InputFunction::UNDEFINED;
    L::DriverSettingsContext explicitClear;
    CHECK(prepareIo(explicitClear, b, fresh, cleared));
    CHECK(explicitClear.changed == (1u << 2));
    b.run(explicitClear);
    L::IoObservation after;
    CHECK(L::getIo(explicitClear, b.wire.live, b.current(), b.now, AGE, after));
    CHECK(after.inputFunctions[0] == L::InputFunction::UNDEFINED);
    CHECK(!fixtureMethodPermits(after, true, true, true));
}
// The 2500 selector through the real owner. 2500=2 and 2501=4000 are what both specimens report, and
// neither that nor anything scripted here qualifies a selector, an encoder convention or an activation.
L::ControlSettings loop(L::LoopMode mode) {
    L::ControlSettings s;
    s.loopMode = mode;
    return s;
}
L::ControlSettings loop(uint16_t code) {
    return loop(static_cast<L::LoopMode>(code));
}
// Drive a read to its terminal; a server abort leaves it FAILED.
void readControl(Bench& b, L::DriverContext& out) {
    const uint32_t op = b.begin();
    CHECK(L::prepareControlRead(out, b.current(), op, b.now, DEADLINE, options()));
    while (out.read.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        CHECK(L::nextDriver(out, b.now, work) && work.ready);
        const C::SdoCompletion terminal = b.exchange(work.transfer);
        CHECK(L::advanceDriver(out, terminal, b.now));
    }
}
struct LoopEvidence {
    L::ReadContext state;
    L::DriverContext prior;
    explicit LoopEvidence(Bench& b) {
        b.readState(state);
        readControl(b, prior);
        CHECK(prior.read.state == M::ReadState::SUCCEEDED);
    }
};
M::Status prepareLoop(L::DriverSettingsContext& out, Bench& b, const LoopEvidence& e,
                      const L::ControlSettings& s, uint64_t deadline = DEADLINE) {
    const uint32_t op = b.begin(deadline);
    return L::prepareControlSettings(out, b.wire.live, b.current(), e.prior, e.state, s, op, b.now,
                                     deadline, options(), AGE);
}
M::Status prepareLoop(L::DriverSettingsContext& out, Bench& b, const LoopEvidence& e, L::LoopMode mode,
                      uint64_t deadline = DEADLINE) {
    return prepareLoop(out, b, e, loop(mode), deadline);
}
// No controlword, mode, save, reset, clear, NMT or other object since `first`: only single 2500
// writes, 2500 and 2501 reads, and a client abort cleanup.
void onlyLoopTraffic(const Bench& b, unsigned first) {
    for (unsigned i = first; i < b.wire.sentCount; ++i) {
        const M::Frame& f = b.wire.sent[i];
        CHECK(f.id == 0x601);
        if (f.data[0] == 0x80)
            continue;
        CHECK(f.data[2] == 0x25 && f.data[3] == 0);
        if (f.data[0] == 0x40)
            CHECK(f.data[1] <= 1);
        else
            CHECK(f.data[0] == 0x2b && f.data[1] == 0 && f.data[4] <= 2 && !f.data[5] && !f.data[6] &&
                  !f.data[7]);
    }
}
unsigned loopWrites(const Bench& b) {
    unsigned writes = 0;
    for (unsigned i = 0; i < b.wire.sentCount; ++i)
        if (b.wire.sent[i].id == 0x601 && b.wire.sent[i].data[0] != 0x40 && b.wire.sent[i].data[0] != 0x80 &&
            b.wire.sent[i].data[2] == 0x25)
            ++writes;
    return writes;
}
void noLoopPublication(const L::DriverSettingsContext& c, Bench& b) {
    L::ControlObservation seen;
    const Saved<L::ControlObservation> none(seen);
    CHECK(!L::getControl(c, b.wire.live, b.current(), b.now, AGE, seen));
    none.unchanged(seen);
}
// Yield and admit the next step of an owned transaction, leaving it for the scenario to settle.
O::RequestId startLoopStep(Bench& b, L::DriverSettingsContext& c) {
    b.wire.settings = &c;
    L::PreparedRead work;
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    return b.admit(work.transfer);
}
// Run the write and its acknowledgement, and move the reservation to the new epoch for readback.
void acknowledgeLoopWrite(Bench& b, L::DriverSettingsContext& c) {
    b.wire.settings = &c;
    L::PreparedRead work;
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const C::SdoCompletion ack = b.exchange(work.transfer);
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, ack, b.now));
    CHECK(c.state == L::SettingsState::READING_BACK && c.confirmed == 1);
    CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
}

void loopEndToEnd() {
    Bench b;
    LoopEvidence e(b);
    L::ControlObservation found;
    CHECK(L::getControl(e.prior, b.current(), b.now, AGE, found));
    CHECK(found.loopMode == L::LoopMode::CLOSED_LOOP && found.loopModeRaw == 2 &&
          found.encoderResolutionRaw == 4000);
    L::DriverContext motionHeld;
    b.read(motionHeld, L::DriverGroup::MOTION_PARAMETERS);
    M::AxisConfig axis;
    const M::PreparedTarget prepared = preparedTarget(b, axis);
    L::DriverSettingsContext c;
    CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
    CHECK(c.state == L::SettingsState::WRITING && c.changed == 1 && !c.effectsPossible && !c.confirmed);
    // Preparation moves no epoch and invalidates nothing.
    CHECK(b.wire.current.configurationGeneration == 7);
    axis.binding = b.wire.current;
    CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
    const unsigned first = b.mark();
    b.run(c);
    CHECK(c.state == L::SettingsState::SUCCEEDED && c.outcome == M::ReadOutcome::SUCCESS);
    CHECK(c.changed == 1 && c.confirmed == 1 && !c.uncertain && c.effectsPossible);
    CHECK(b.wire.current.configurationGeneration == 8 && b.wire.notices == 1);
    // One write, then the whole control group at the new epoch, and nothing else.
    CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2500, 0, 1));
    CHECK(frameIs(b.wire.sent[first + 1], 0x40, 0x2500, 0, 0));
    CHECK(frameIs(b.wire.sent[first + 2], 0x40, 0x2501, 0, 0));
    CHECK(b.wire.sentCount == first + 3);
    CHECK(b.drive.value(0x2500) == 1 && b.drive.value(0x2501) == 4000);
    CHECK(c.readback.read.target.channel.binding.configurationGeneration == 8);
    L::ControlObservation seen;
    CHECK(L::getControl(c, b.wire.live, b.current(), b.now, AGE, seen));
    CHECK(seen.loopMode == L::LoopMode::OPEN_LOOP && seen.loopModeRaw == 1);
    CHECK(seen.encoderResolutionRaw == 4000);
    CHECK(seen.target.channel.binding.configurationGeneration == 8);
    CHECK(seen.fields[0].index == 0x2500 && seen.fields[1].index == 0x2501);
    // The frozen token keeps its epoch and publishes nothing there.
    CHECK(b.reservation.target.channel.binding.configurationGeneration == 7);
    const Saved<L::ControlObservation> published(seen);
    CHECK(!L::getControl(c, b.wire.live, b.reservation.target, b.now, AGE, seen));
    published.unchanged(seen);
    // Every earlier observation and target on this configuration is now refused.
    CHECK(!L::getControl(e.prior, b.current(), b.now, AGE, seen));
    published.unchanged(seen);
    L::StateObservation block;
    CHECK(!L::getStateBlock(e.state, L::StateBlock::STATUS, b.current(), b.now, AGE, block));
    L::DriverObservation motionSeen;
    CHECK(!L::getDriver(motionHeld, b.current(), b.now, AGE, motionSeen));
    axis.binding = b.wire.current;
    CHECK(!M::validatePreparedTarget(prepared, axis, nullptr, b.now));
    // The other settings getters refuse this transaction's kind.
    CHECK(!L::getDriver(c, b.wire.live, b.current(), b.now, AGE, motionSeen));
    L::IoObservation ioSeen;
    CHECK(!L::getIo(c, b.wire.live, b.current(), b.now, AGE, ioSeen));
}

void loopEveryTransitionOnTheWire() {
    const uint32_t storedWords[] = {0, 1, 2, 3, 0x1234, 0xffff};
    for (L::SettingsModel model : {L::SettingsModel::CL86_C, L::SettingsModel::OL86_C})
        for (uint32_t stored : storedWords)
            for (uint16_t code = 0; code < 3; ++code) {
                Bench b(model);
                b.drive.set(0x2500, 0, stored);
                LoopEvidence e(b);
                // An undocumented stored word stays raw, undecoded and repairable.
                L::ControlObservation found;
                CHECK(L::getControl(e.prior, b.current(), b.now, AGE, found));
                CHECK(found.loopModeRaw == stored && found.encoderResolutionRaw == 4000);
                CHECK((found.loopMode == L::LoopMode::UNKNOWN) == (stored > 2));
                L::DriverSettingsContext c;
                if (model == L::SettingsModel::OL86_C && code == 2) { // No documented encoder, EN86 p3.
                    refused(b, c, [&] { return prepareLoop(c, b, e, loop(code)); });
                    CHECK(b.drive.value(0x2500) == stored);
                    continue;
                }
                const unsigned first = b.mark(), guards = b.wire.guards;
                CHECK(prepareLoop(c, b, e, loop(code)));
                if (stored == code) { // The no-op: its fresh read stands, and no frame or epoch moves.
                    CHECK(c.state == L::SettingsState::SUCCEEDED && !c.changed && !c.effectsPossible);
                    CHECK(!c.confirmed && !c.uncertain && c.outcome == M::ReadOutcome::SUCCESS);
                    L::PreparedRead work;
                    work.ready = true;
                    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && !work.ready);
                    b.turn(b.now);
                    b.checkSilentSince(first);
                    CHECK(b.wire.guards == guards && b.wire.notices == 0);
                    CHECK(b.wire.current.configurationGeneration == 7);
                    L::ControlObservation same;
                    CHECK(L::getControl(c, b.wire.live, b.current(), b.now, AGE, same));
                    CHECK(same.loopModeRaw == code && same.encoderResolutionRaw == 4000);
                    continue;
                }
                b.run(c);
                CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == 1 && !c.uncertain);
                CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2500, 0, code));
                CHECK(b.wire.sentCount == first + 3);
                onlyLoopTraffic(b, first);
                CHECK(b.drive.value(0x2500) == code && b.drive.value(0x2501) == 4000);
                CHECK(b.wire.notices == 1 && b.wire.current.configurationGeneration == 8);
            }
    { // Selector 0 is a register value like the others: no controlword goes with it.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        const unsigned first = b.mark();
        CHECK(prepareLoop(c, b, e, L::LoopMode::INEFFECTIVE));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.value(0x2500) == 0);
        onlyLoopTraffic(b, first);
        CHECK(b.drive.downloads == 1);
    }
}

void loopCandidatesRefusedWhole() {
    // Candidate words and a default candidate: nothing outside 0..2 goes anywhere.
    for (uint16_t code : {uint16_t(3), uint16_t(4), uint16_t(0x8000), uint16_t(0xfffe), uint16_t(0xffff)}) {
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        refused(b, c, [&] { return prepareLoop(c, b, e, loop(code)); });
        CHECK(b.drive.value(0x2500) == 2);
    }
    {
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        refused(b, c, [&] { return prepareLoop(c, b, e, L::ControlSettings()); });
    }
    // Each case spoils one prerequisite of a candidate that is otherwise admissible.
    enum Case {
        NO_MODEL, BAD_MODEL, NO_OWNER, NO_SEQUENCE, NO_GENERATION, PRIOR_BEFORE_RESERVATION,
        NO_OPERATION, DEADLINE_NOW, DEADLINE_PAST, NO_QUEUE_BUDGET, NO_RESPONSE_BUDGET, NO_AGE,
        EXPIRED, EXHAUSTED, CASES
    };
    for (unsigned k = 0; k < CASES; ++k) {
        Bench b;
        LoopEvidence e(b);
        L::AxisReservation live = b.wire.live;
        if (k == NO_MODEL)
            live.model = L::SettingsModel::UNKNOWN;
        if (k == BAD_MODEL)
            live.model = static_cast<L::SettingsModel>(3);
        if (k == NO_OWNER)
            live.owner = 0;
        if (k == NO_SEQUENCE)
            live.sequence = 0;
        if (k == NO_GENERATION)
            live.generation = 0;
        if (k == PRIOR_BEFORE_RESERVATION)
            live.acquiredUs = b.now + 1;
        if (k == EXHAUSTED)
            live.target.channel.binding.configurationGeneration = UINT32_MAX;
        uint32_t operation = b.begin();
        uint64_t deadline = DEADLINE, age = AGE;
        if (k == NO_OPERATION)
            operation = 0;
        if (k == DEADLINE_NOW)
            deadline = b.now;
        if (k == DEADLINE_PAST) {
            b.turn(b.now + 1);
            deadline = b.now - 1;
        }
        if (k == NO_AGE)
            age = 0;
        if (k == EXPIRED)
            b.turn(b.now + AGE + 1);
        L::ReadOptions policy = options();
        if (k == NO_QUEUE_BUDGET)
            policy.queueTimeoutUs = 0;
        if (k == NO_RESPONSE_BUDGET)
            policy.responseTimeoutUs = 0;
        L::DriverSettingsContext c;
        refused(b, c, [&] {
            return L::prepareControlSettings(c, live, live.target, e.prior, e.state, loop(uint16_t(1)), operation,
                                             b.now, deadline, policy, age);
        });
    }
    // The same evidence admits with its oldest field exactly AGE old, so the cases above isolate what
    // they spoil, and one microsecond more refuses.
    for (uint64_t late = 0; late < 2; ++late) {
        Bench b;
        LoopEvidence e(b);
        L::StateObservation status, monitors;
        L::ControlObservation control;
        CHECK(L::getStateBlock(e.state, L::StateBlock::STATUS, b.current(), b.now, AGE, status));
        CHECK(L::getStateBlock(e.state, L::StateBlock::MONITORS, b.current(), b.now, AGE, monitors));
        CHECK(L::getControl(e.prior, b.current(), b.now, AGE, control));
        uint64_t oldest = status.window.earliestUs;
        if (monitors.window.earliestUs < oldest)
            oldest = monitors.window.earliestUs;
        if (control.window.earliestUs < oldest)
            oldest = control.window.earliestUs;
        b.turn(oldest + AGE + late);
        L::DriverSettingsContext c;
        if (late) {
            refused(b, c, [&] { return prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP); });
            continue;
        }
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        CHECK(c.state == L::SettingsState::WRITING);
    }
    { // A prior of another kind is complete and still not this one.
        Bench b;
        L::ReadContext state;
        b.readState(state);
        const L::DriverGroup others[] = {L::DriverGroup::DRIVER, L::DriverGroup::SOFT_LIMITS,
                                         L::DriverGroup::MOTION_PARAMETERS,
                                         L::DriverGroup::IO_INPUT_CONFIGURATION,
                                         L::DriverGroup::TUNING_CURRENT_HOLDING};
        for (L::DriverGroup group : others) {
            L::DriverContext wrong;
            b.read(wrong, group);
            const uint32_t op = b.begin();
            L::DriverSettingsContext c;
            refused(b, c, [&] {
                return L::prepareControlSettings(c, b.wire.live, b.current(), wrong, state, loop(uint16_t(1)), op,
                                                 b.now, DEADLINE, options(), AGE);
            });
        }
    }
    // A failed prior read, at either word.
    for (uint16_t index : {uint16_t(0x2500), uint16_t(0x2501)}) {
        Bench b;
        L::ReadContext state;
        b.readState(state);
        b.drive.inject(Drive::ABORT, index, 0, false);
        L::DriverContext prior;
        readControl(b, prior);
        CHECK(prior.read.state == M::ReadState::FAILED);
        const uint32_t op = b.begin();
        L::DriverSettingsContext c;
        refused(b, c, [&] {
            return L::prepareControlSettings(c, b.wire.live, b.current(), prior, state, loop(uint16_t(1)), op,
                                             b.now, DEADLINE, options(), AGE);
        });
    }
    { // A prior or state read older than this reservation, each on its own.
        Bench b;
        L::ReadContext oldState;
        b.readState(oldState);
        L::DriverContext oldPrior;
        readControl(b, oldPrior);
        b.turn(b.now + 1);
        L::AxisReservation live = b.wire.live;
        live.acquiredUs = b.now;
        L::ReadContext state;
        b.readState(state);
        L::DriverContext prior;
        readControl(b, prior);
        const uint32_t op = b.begin();
        L::DriverSettingsContext c;
        refused(b, c, [&] {
            return L::prepareControlSettings(c, live, live.target, oldPrior, state, loop(uint16_t(1)), op, b.now,
                                             DEADLINE, options(), AGE);
        });
        refused(b, c, [&] {
            return L::prepareControlSettings(c, live, live.target, prior, oldState, loop(uint16_t(1)), op, b.now,
                                             DEADLINE, options(), AGE);
        });
        CHECK(L::prepareControlSettings(c, live, live.target, prior, state, loop(uint16_t(1)), op, b.now, DEADLINE,
                                        options(), AGE));
    }
    { // A new node generation refuses before the evidence is looked at.
        Bench b;
        LoopEvidence e(b);
        L::ReadTarget rebooted = b.current();
        ++rebooted.nodeGeneration;
        L::DriverSettingsContext c;
        const M::Status status = L::prepareControlSettings(c, b.wire.live, rebooted, e.prior, e.state,
                                                           loop(uint16_t(1)), 99, b.now, DEADLINE, options(), AGE);
        CHECK(!status && status.detail == int32_t(M::ReadError::TARGET));
    }
}

// Project policy, as for 2202. Each word holds alone against the fixture's clean one.
void loopStateRules() {
    const auto admitted = [](uint32_t word, uint16_t code) {
        Bench b;
        b.drive.set(0x2100, 0, word);
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        const uint32_t before = b.drive.value(0x2500);
        const unsigned first = b.mark();
        const M::Status status = prepareLoop(c, b, e, loop(code));
        if (status) {
            b.run(c);
            CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.value(0x2500) == code);
        } else {
            CHECK(status.detail == int32_t(M::ReadError::STATE));
            CHECK(b.wire.sentCount == first && b.drive.value(0x2500) == before);
        }
        return bool(status);
    };
    for (uint16_t code : {uint16_t(0), uint16_t(1)}) {
        CHECK(admitted(0x0001, code)); // Both specimens read 2100=1.
        CHECK(admitted(0x0000, code));
        CHECK(admitted(0x0003, code)); // Homing complete is not a refusal here.
        CHECK(admitted(0x0011, code)); // Bit 4 decides nothing, reported set or clear.
        CHECK(admitted(0x0021, code)); // The soft-limit flags are documented reports.
        CHECK(admitted(0x0041, code));
        CHECK(!admitted(0x0005, code)); // Running.
        // Alarm. Open loop cannot be used to get past one either, even from a lost encoder.
        CHECK(!admitted(0x0009, code));
        for (unsigned bit = 7; bit < 16; ++bit)
            CHECK(!admitted(0x0001u | (1u << bit), code));
    }
    // A failed or partial state read refuses even though the blocks this call reads completed.
    for (Drive::Fault fault : {Drive::ABORT, Drive::LOST}) {
        Bench b;
        L::DriverContext prior;
        readControl(b, prior);
        b.drive.inject(fault, 0x60fd, 0, false);
        L::ReadContext state;
        const uint32_t op = b.begin();
        CHECK(L::prepareState(state, b.current(), op, b.now, DEADLINE, options(),
                              uint8_t(STATE_MASK | (1u << unsigned(L::StateBlock::INPUTS)))));
        for (unsigned i = 0; i < 5; ++i) {
            L::PreparedRead work;
            CHECK(L::nextRead(state, b.now, work) && work.ready);
            const C::SdoCompletion terminal = b.exchange(work.transfer);
            CHECK(L::advanceRead(state, terminal, b.now));
        }
        L::PreparedRead failing;
        CHECK(L::nextRead(state, b.now, failing) && failing.ready);
        const O::RequestId id = b.admit(failing.transfer);
        b.turn(b.now);
        b.deliver();
        b.turn(b.now + (fault == Drive::LOST ? 100 : 1), budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(L::advanceRead(state, terminal->service, b.now));
        CHECK(state.state == M::ReadState::FAILED);
        L::DriverSettingsContext c;
        refused(b, c, [&] {
            return L::prepareControlSettings(c, b.wire.live, b.current(), prior, state, loop(uint16_t(1)),
                                             b.operation, b.now, DEADLINE, options(), AGE);
        });
        CHECK(b.owner.release(id));
    }
    // Each required block must be there: a read of one alone is not enough.
    for (L::StateBlock only : {L::StateBlock::STATUS, L::StateBlock::MONITORS}) {
        Bench b;
        L::ReadContext state;
        b.readState(state, uint8_t(1u << unsigned(only)));
        L::DriverContext prior;
        readControl(b, prior);
        const uint32_t op = b.begin();
        L::DriverSettingsContext c;
        refused(b, c, [&] {
            return L::prepareControlSettings(c, b.wire.live, b.current(), prior, state, loop(uint16_t(1)), op,
                                             b.now, DEADLINE, options(), AGE);
        });
    }
    // Closed loop on a stored 2501 of zero is refused; the rest of the domain stays open, so the same
    // read can still leave closed loop. Nonzero words are not a domain: each only passes this check.
    for (uint32_t encoder : {0u, 1u, 4000u, 65535u})
        for (uint32_t stored : {0u, 1u, 2u, 3u})
            for (uint16_t code = 0; code < 3; ++code) {
                Bench b;
                b.drive.set(0x2500, 0, stored);
                b.drive.set(0x2501, 0, encoder);
                LoopEvidence e(b);
                L::DriverSettingsContext c;
                if (code == 2 && !encoder) {
                    const Saved<L::DriverSettingsContext> blank(c);
                    const M::Status status = prepareLoop(c, b, e, loop(code));
                    CHECK(!status && status.detail == int32_t(M::ReadError::VALUE));
                    blank.unchanged(c);
                    continue;
                }
                CHECK(prepareLoop(c, b, e, loop(code)));
                b.run(c);
                CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.value(0x2501) == encoder);
            }
}

void loopFrozenTokenAndAliases() {
    { // A finished transaction leaves a token and evidence that agree with each other and not the drive.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.value(0x2500) == 1);
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> blank(out);
        const unsigned sent = b.mark();
        const uint32_t op = b.begin();
        const M::Status status = L::prepareControlSettings(out, b.wire.live, b.current(), e.prior, e.state,
                                                           loop(uint16_t(2)), op, b.now, DEADLINE, options(), AGE);
        CHECK(!status && status.detail == int32_t(M::ReadError::TARGET));
        blank.unchanged(out);
        b.checkSilentSince(sent);
        // A current token refuses the old evidence outright. Fresh evidence admits the next step.
        b.renew();
        refused(b, out, [&] { return prepareLoop(out, b, e, L::LoopMode::CLOSED_LOOP); });
        LoopEvidence fresh(b);
        L::DriverSettingsContext back;
        const unsigned first = b.mark();
        CHECK(prepareLoop(back, b, fresh, L::LoopMode::CLOSED_LOOP));
        b.run(back);
        CHECK(back.state == L::SettingsState::SUCCEEDED && b.wire.current.configurationGeneration == 9);
        CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2500, 0, 2));
        CHECK(b.wire.notices == 2);
    }
    { // A plan made while current cannot act once another transaction has moved the generation.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext heldNoOp, heldWrite, c;
        CHECK(prepareLoop(heldNoOp, b, e, L::LoopMode::CLOSED_LOOP));
        CHECK(heldNoOp.state == L::SettingsState::SUCCEEDED && !heldNoOp.changed);
        CHECK(prepareLoop(heldWrite, b, e, L::LoopMode::INEFFECTIVE));
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        b.run(c);
        noLoopPublication(heldNoOp, b);
        L::PreparedRead work;
        CHECK(!L::nextDriver(heldNoOp, b.wire.live, b.wire.current, b.now, work));
        CHECK(!L::nextDriver(heldWrite, b.wire.live, b.wire.current, b.now, work));
        M::Binding binding = b.wire.current;
        const Saved<M::Binding> kept(binding);
        CHECK(!L::noteDriverTransmission(heldWrite, b.wire.live, binding, b.now));
        kept.unchanged(binding);
    }
    { // Every input that can sit inside the output refuses, before any work.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        const uint32_t op = b.operation;
        const Saved<L::DriverSettingsContext> saved(c);
        const auto aliased = [&](M::Status status) {
            CHECK(!status && status.detail == int32_t(M::ReadError::OVERLAP));
            saved.unchanged(c);
        };
        const L::ControlSettings wanted = loop(uint16_t(1));
        const L::ReadOptions policy = options();
        aliased(L::prepareControlSettings(c, c.reservation, b.current(), e.prior, e.state, wanted, op, b.now,
                                          DEADLINE, policy, AGE));
        aliased(L::prepareControlSettings(c, b.wire.live, c.reservation.target, e.prior, e.state, wanted, op,
                                          b.now, DEADLINE, policy, AGE));
        aliased(L::prepareControlSettings(c, b.wire.live, b.current(), c.readback, e.state, wanted, op, b.now,
                                          DEADLINE, policy, AGE));
        aliased(L::prepareControlSettings(c, b.wire.live, b.current(), e.prior, c.readback.read, wanted, op,
                                          b.now, DEADLINE, policy, AGE));
        aliased(L::prepareControlSettings(c, b.wire.live, b.current(), e.prior, e.state,
                                          at<L::ControlSettings>(&c.requested[0]), op, b.now, DEADLINE, policy,
                                          AGE));
        aliased(L::prepareControlSettings(c, b.wire.live, b.current(), e.prior, e.state, wanted, op, b.now,
                                          DEADLINE, c.readback.read.options, AGE));
    }
    { // A stale copy of the reservation refuses every step, and revocation stops old copies at the owner.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        // The route has one reservation: nothing else can begin on it while this one holds.
        O::SequenceId other;
        CHECK(!b.owner.beginSequence(route(1).binding, 2, b.now, other));
        L::AxisReservation stale = b.wire.live;
        ++stale.sequence;
        L::PreparedRead work;
        const Saved<L::PreparedRead> noWork(work);
        CHECK(!L::nextDriver(c, stale, b.wire.current, b.now, work));
        noWork.unchanged(work);
        M::Binding binding = b.wire.current;
        CHECK(!L::noteDriverTransmission(c, stale, binding, b.now));
        CHECK(binding.configurationGeneration == 7);
        const L::AxisReservation before = b.wire.live;
        CHECK(b.owner.invalidate(b.sequence, b.now) && !b.owner.sequence(b.sequence));
        b.wire.live.owner = 0;
        CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, b.now, work));
        O::BusRequest late;
        late.sequence = b.sequence;
        CHECK(L::nextDriver(c, before, b.wire.current, b.now, work) && work.ready);
        late.transfer = work.transfer;
        O::RequestId refusedId;
        CHECK(b.owner.admit(late, b.now, refusedId) != O::BusAdmission::ACCEPTED);
    }
}

void loopCandidateIsImmutable() {
    Bench b;
    LoopEvidence e(b);
    L::ControlSettings wanted = loop(L::LoopMode::OPEN_LOOP);
    L::DriverSettingsContext c;
    CHECK(prepareLoop(c, b, e, wanted));
    wanted.loopMode = L::LoopMode::INEFFECTIVE;
    b.wire.settings = &c;
    L::PreparedRead work;
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const O::RequestId id = b.admit(work.transfer);
    // Neither the candidate, the yielded copy nor the context can reach the queued request.
    work.transfer.data[0] = 0xee;
    const uint32_t held = c.requested[0];
    c.requested[0] = 0;
    const unsigned first = b.mark();
    b.turn(b.now);
    CHECK(frameIs(b.wire.sent[first], 0x2b, 0x2500, 0, 1));
    c.requested[0] = held;
    b.deliver();
    b.turn(b.now + 1, budget(8, 0, 8, 0));
    const O::Completion* terminal = b.owner.result(id);
    CHECK(terminal && terminal->service.transfer.data[0] == 1);
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
    CHECK(b.owner.release(id));
    CHECK(c.confirmed == 1 && b.drive.value(0x2500) == 1);
    b.wire.settings = nullptr;
    // A hand-edited active context refuses instead of writing 2501 or an undocumented code.
    Bench r(L::SettingsModel::OL86_C);
    r.drive.set(0x2500, 0, 0);
    LoopEvidence f(r);
    L::DriverSettingsContext d;
    CHECK(prepareLoop(d, r, f, L::LoopMode::OPEN_LOOP));
    const Saved<L::DriverSettingsContext> saved(d);
    const auto refusedEdit = [&]() {
        L::PreparedRead edited;
        const Saved<L::PreparedRead> none(edited);
        CHECK(!L::nextDriver(d, r.wire.live, r.wire.current, r.now, edited));
        none.unchanged(edited);
    };
    const uint32_t code = d.requested[0];
    const uint16_t fields = d.changed;
    d.requested[0] = 3;
    refusedEdit();
    d.requested[0] = 2; // Closed loop for OL86-C.
    refusedEdit();
    d.requested[0] = code;
    d.changed = 2; // 2501.
    refusedEdit();
    d.changed = 3;
    refusedEdit();
    d.changed = fields;
    d.cursor = 1;
    refusedEdit();
    d.cursor = 0;
    saved.unchanged(d);
    const unsigned sent = r.mark();
    r.run(d);
    CHECK(d.state == L::SettingsState::SUCCEEDED && r.drive.value(0x2500) == 1);
    CHECK(frameIs(r.wire.sent[sent], 0x2b, 0x2500, 0, 1));
    onlyLoopTraffic(r, sent);
}

void loopFailedDownload() {
    { // A server abort: nothing confirmed, no readback, no retry, and the failure stays.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        const unsigned first = b.mark();
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        b.drive.inject(Drive::ABORT, 0x2500, 0, true);
        b.run(c);
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
        CHECK(!c.confirmed && c.effectsPossible && c.uncertain && !c.acknowledgedUs[0]);
        CHECK(c.evidencePresent && c.evidence.abortValid && c.evidence.abortCode == 0x08000022u);
        CHECK(b.wire.current.configurationGeneration == 8 && b.wire.notices == 1);
        CHECK(b.drive.value(0x2500) == 2);
        noLoopPublication(c, b);
        L::PreparedRead work;
        work.ready = true;
        CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && !work.ready);
        const unsigned sent = b.mark();
        b.turn(b.now + 10);
        b.checkSilentSince(sent);
        CHECK(loopWrites(b) == 1);
        onlyLoopTraffic(b, first);
    }
    { // A lost reply: the write may have landed, is not confirmed, is never replayed.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        b.drive.inject(Drive::LOST, 0x2500, 0, true);
        const O::RequestId id = startLoopStep(b, c);
        b.turn(b.now);
        b.deliver();
        b.turn(b.now + 100, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.outcome == C::SdoOutcome::TIMEOUT);
        CHECK(terminal->service.effectUncertain && terminal->service.transmissionPossible);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
        CHECK(c.uncertain && !c.confirmed && b.drive.value(0x2500) == 1);
        // The late reply changes no result, the route stays unresolved, and no write repeats.
        const Saved<O::Completion> retained(*terminal);
        M::Frame late;
        late.id = 0x581;
        late.dlc = late.payloadLength = 8;
        late.data[0] = 0x60;
        late.data[2] = 0x25;
        b.wire.push(late, b.now + 1);
        b.turn(b.now + 1, budget(8, 0, 8, 0));
        retained.unchanged(*b.owner.result(id));
        CHECK(b.services.unresolved(0) && !b.owner.sequence(b.sequence));
        O::SequenceId another;
        CHECK(!b.owner.beginSequence(route(1).binding, 2, b.now, another));
        CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
        CHECK(loopWrites(b) == 1);
        // The token and evidence from before it cannot report a no-op, whatever the drive holds.
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> blank(out);
        const M::Status status = L::prepareControlSettings(out, b.wire.live, b.current(), e.prior, e.state,
                                                           loop(uint16_t(2)), 99, b.now, DEADLINE, options(), AGE);
        CHECK(!status && status.detail == int32_t(M::ReadError::TARGET));
        blank.unchanged(out);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
}

void loopFailedReadback() {
    for (uint16_t index : {uint16_t(0x2500), uint16_t(0x2501)}) {
        { // An upload abort after the acknowledgement: register accepted, group unconfirmed.
            Bench b;
            LoopEvidence e(b);
            L::DriverSettingsContext c;
            CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
            b.drive.inject(Drive::ABORT, index, 0, false);
            b.run(c);
            CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
            CHECK(c.confirmed == 1 && c.effectsPossible);
            CHECK(b.drive.value(0x2500) == 1 && loopWrites(b) == 1);
            noLoopPublication(c, b);
        }
        { // A readback word that contradicts the candidate. 2501 was never written, and still decides.
            Bench b;
            LoopEvidence e(b);
            L::DriverSettingsContext c;
            CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
            const uint32_t wrong = index == 0x2500 ? 2 : 4001;
            b.drive.lieIndex = index;
            b.drive.lieRaw = wrong;
            b.run(c);
            CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR);
            CHECK(c.confirmed == 1 && !c.uncertain);
            CHECK(c.readback.read.values[index & 1].raw == wrong);
            noLoopPublication(c, b);
            CHECK(loopWrites(b) == 1);
        }
        { // A lost upload: uncertain route, the acknowledgement stays, and nothing is published.
            Bench b;
            LoopEvidence e(b);
            L::DriverSettingsContext c;
            CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
            b.drive.inject(Drive::LOST, index, 0, false);
            acknowledgeLoopWrite(b, c);
            const uint64_t acknowledged = c.acknowledgedUs[0];
            if (index == 0x2501) {
                L::PreparedRead work;
                CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
                const C::SdoCompletion first = b.exchange(work.transfer);
                CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, first, b.now));
            }
            const O::RequestId id = startLoopStep(b, c);
            b.turn(b.now);
            b.deliver();
            b.turn(b.now + 100, budget(8, 0, 8, 0));
            const O::Completion* terminal = b.owner.result(id);
            CHECK(terminal && terminal->service.outcome == C::SdoOutcome::TIMEOUT);
            CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
            CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
            CHECK(c.confirmed == 1 && c.acknowledgedUs[0] == acknowledged && c.uncertain);
            noLoopPublication(c, b);
            CHECK(b.owner.release(id));
            b.wire.settings = nullptr;
        }
    }
}

void loopCancellationAndPreflight() {
    { // Cancelled before any possible transmission: no epoch change and no uncertainty.
        Bench b;
        LoopEvidence e(b);
        M::AxisConfig axis;
        const M::PreparedTarget prepared = preparedTarget(b, axis);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        const unsigned sent = b.mark();
        const O::RequestId id = startLoopStep(b, c);
        CHECK(b.owner.cancel(id, b.now) == O::Cancel::CANCELLED);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && !terminal->service.transmissionPossible);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::CANCELLED);
        CHECK(!c.effectsPossible && !c.uncertain && !c.confirmed);
        CHECK(b.wire.current.configurationGeneration == 7 && b.drive.value(0x2500) == 2);
        b.turn(b.now);
        b.checkSilentSince(sent);
        axis.binding = b.wire.current;
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, b.now));
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A refused preflight is the same: the notice never ran.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        b.wire.rejectGuard = true;
        const unsigned sent = b.mark();
        const O::RequestId id = startLoopStep(b, c);
        b.turn(b.now);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->reason == O::CompletionReason::PREFLIGHT_REJECTED);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(!c.effectsPossible && !c.uncertain && b.wire.current.configurationGeneration == 7);
        CHECK(b.wire.notices == 0);
        b.checkSilentSince(sent);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // The controller refuses the frame after the notice: uncertain, and nothing resends.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        const O::RequestId id = startLoopStep(b, c);
        b.wire.accept = false;
        b.turn(b.now);
        CHECK(b.wire.notices == 1 && b.wire.current.configurationGeneration == 8);
        b.turn(b.now + 1, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.uncertain && c.effectsPossible && !c.confirmed);
        noLoopPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // Cancelled after handoff: uncertain, and the epoch stays advanced.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        const O::RequestId id = startLoopStep(b, c);
        b.turn(b.now);
        CHECK(b.owner.mayHaveTransmitted(id));
        CHECK(b.owner.cancel(id, b.now) == O::Cancel::CANCELLED);
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.effectUncertain);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.uncertain && c.effectsPossible);
        CHECK(b.wire.current.configurationGeneration == 8 && b.services.unresolved(0));
        noLoopPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A local withdrawal during readback: not a rollback and not a motor stop.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        const unsigned first = b.mark();
        acknowledgeLoopWrite(b, c);
        CHECK(L::advanceDriver(c, M::ReadOutcome::CANCELLED, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::CANCELLED);
        CHECK(c.confirmed == 1 && b.drive.value(0x2500) == 1);
        const unsigned sent = b.mark();
        b.turn(b.now + 1);
        b.checkSilentSince(sent);
        noLoopPublication(c, b);
        onlyLoopTraffic(b, first);
        b.wire.settings = nullptr;
    }
}

void loopDeadlines() {
    { // The cutoff reached while the write waits: not a timeout, and the effect stays uncertain.
        Bench b;
        LoopEvidence e(b);
        const uint64_t deadline = b.now + 50;
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP, deadline));
        const O::RequestId id = startLoopStep(b, c);
        b.turn(b.now);
        const Saved<L::DriverSettingsContext> saved(c);
        CHECK(!L::advanceDriver(c, M::ReadOutcome::DEADLINE, b.now));
        saved.unchanged(c);
        b.turn(deadline, budget(8, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::DEADLINE);
        CHECK(c.uncertain && c.effectsPossible && !c.confirmed);
        noLoopPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // At the cutoff nothing more is yielded or noted.
        Bench b;
        LoopEvidence e(b);
        const uint64_t deadline = b.now + 5;
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP, deadline));
        L::PreparedRead work;
        const Saved<L::PreparedRead> none(work);
        CHECK(!L::nextDriver(c, b.wire.live, b.wire.current, deadline, work));
        none.unchanged(work);
        M::Binding binding = b.wire.current;
        CHECK(!L::noteDriverTransmission(c, b.wire.live, binding, deadline));
        CHECK(binding.configurationGeneration == 7);
        CHECK(L::advanceDriver(c, M::ReadOutcome::DEADLINE, deadline));
        CHECK(c.outcome == M::ReadOutcome::DEADLINE && !c.uncertain && !c.effectsPossible);
    }
    { // A deadline during readback publishes nothing, and the acknowledgement stays.
        Bench b;
        LoopEvidence e(b);
        const uint64_t deadline = b.now + 6;
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP, deadline));
        acknowledgeLoopWrite(b, c);
        b.turn(deadline, budget(8, 0, 8, 0));
        CHECK(L::advanceDriver(c, M::ReadOutcome::DEADLINE, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::DEADLINE);
        CHECK(c.confirmed == 1);
        noLoopPublication(c, b);
        b.wire.settings = nullptr;
    }
}

void loopFaultRebootAndBusOff() {
    { // A boot-up during the write: uncertain, reservation gone, route closed, nothing replayed.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        const O::RequestId id = startLoopStep(b, c);
        b.turn(b.now);
        const unsigned first = b.mark();
        b.wire.push(bootFrame(), b.now + 1);
        b.turn(b.now + 1, budget(1, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.outcome == C::SdoOutcome::REBOOT);
        CHECK(terminal->service.effectUncertain && !b.owner.sequence(b.sequence));
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::INVALIDATED);
        CHECK(c.uncertain && !c.confirmed);
        noLoopPublication(c, b);
        O::SequenceId another;
        CHECK(!b.owner.beginSequence(route(1).binding, 1, b.now, another));
        CHECK(b.owner.release(id));
        b.turn(b.now + 1);
        b.checkSilentSince(first); // No restore, replay, enable or NMT followed the boot-up.
        b.wire.settings = nullptr;
    }
    { // A boot-up during readback: the acknowledged write is history, and nothing is published.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        acknowledgeLoopWrite(b, c);
        const O::RequestId id = startLoopStep(b, c);
        b.turn(b.now);
        b.wire.push(bootFrame(), b.now + 1);
        b.turn(b.now + 1, budget(1, 0, 8, 0));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.outcome == C::SdoOutcome::REBOOT);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.confirmed == 1);
        noLoopPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // Bus-off during the write: invalidated, and the effect stays uncertain.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        const O::RequestId id = startLoopStep(b, c);
        b.turn(b.now);
        b.now += 1;
        CHECK(b.owner.reportController(C::ControllerState::BUS_OFF, b.now));
        const O::Completion* terminal = b.owner.result(id);
        CHECK(terminal && terminal->service.effectUncertain);
        CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
        CHECK(c.state == L::SettingsState::FAILED && c.uncertain && !c.confirmed);
        noLoopPublication(c, b);
        CHECK(b.owner.release(id));
        b.wire.settings = nullptr;
    }
    { // A drive fault event mid-transaction is observed, never cleared, and adds no frame.
        Bench b;
        LoopEvidence e(b);
        L::DriverSettingsContext c;
        const unsigned first = b.mark();
        CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
        acknowledgeLoopWrite(b, c);
        M::Frame fault;
        fault.id = 0x81;
        fault.dlc = fault.payloadLength = 8;
        fault.data[0] = 0x05; // The bytes of FF05, an example; no meaning is asserted here.
        fault.data[1] = 0xff;
        b.wire.push(fault, b.now + 1);
        b.turn(b.now + 1, budget(8, 0, 8, 0));
        CHECK(b.services.node(0)->emcyCount == 1);
        while (c.state == L::SettingsState::READING_BACK) {
            L::PreparedRead work;
            CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
            const C::SdoCompletion reply = b.exchange(work.transfer);
            CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, reply, b.now));
        }
        CHECK(c.state == L::SettingsState::SUCCEEDED);
        onlyLoopTraffic(b, first);
        b.wire.settings = nullptr;
    }
}

// Notice before possible TX, and retirement of old results before the reservation moves.
void loopGenerationsAndRetirement() {
    Bench b;
    LoopEvidence e(b);
    M::AxisConfig axis;
    const M::PreparedTarget prepared = preparedTarget(b, axis);
    L::DriverSettingsContext c;
    CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
    L::PreparedRead work;
    b.wire.settings = &c;
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const unsigned writeFrame = b.mark();
    const O::RequestId id = b.admit(work.transfer);
    CHECK(b.wire.current.configurationGeneration == 7 && !b.wire.notices); // Admission alone moves nothing.
    b.turn(b.now);
    // The notice ran with the write still unsent, and the epoch moved at that first possible download.
    CHECK(b.wire.notices == 1 && b.wire.sentAtNotice == writeFrame && b.wire.sentCount == writeFrame + 1);
    CHECK(frameIs(b.wire.sent[writeFrame], 0x2b, 0x2500, 0, 1));
    CHECK(b.wire.current.configurationGeneration == 8);
    axis.binding = b.wire.current;
    CHECK(!M::validatePreparedTarget(prepared, axis, nullptr, b.now));
    // New-epoch work waits until the old result is consumed, released and the reservation moved.
    CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
    b.deliver();
    b.turn(b.now + 1, budget(8, 0, 8, 0));
    const O::Completion* terminal = b.owner.result(id);
    CHECK(terminal);
    CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, terminal->service, b.now));
    CHECK(!b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
    CHECK(b.owner.release(id));
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    CHECK(work.transfer.index == 0x2500 && work.transfer.direction == C::SdoDirection::UPLOAD);
    CHECK(work.transfer.channel.binding.configurationGeneration == 8);
    O::BusRequest early;
    early.sequence = b.sequence;
    early.transfer = work.transfer;
    O::RequestId rejected;
    CHECK(b.owner.admit(early, b.now, rejected) == O::BusAdmission::INVALID);
    CHECK(b.owner.advanceConfiguration(b.sequence, b.wire.current, b.now));
    CHECK(b.owner.sequence(b.sequence)->binding.configurationGeneration == 8);
    // Publication waits for the whole readback, and the first field alone publishes nothing.
    const C::SdoCompletion first = b.exchange(work.transfer);
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, first, b.now));
    CHECK(c.state == L::SettingsState::READING_BACK);
    noLoopPublication(c, b);
    CHECK(L::nextDriver(c, b.wire.live, b.wire.current, b.now, work) && work.ready);
    const C::SdoCompletion second = b.exchange(work.transfer);
    CHECK(L::advanceDriver(c, b.wire.live, b.wire.current, second, b.now));
    CHECK(c.state == L::SettingsState::SUCCEEDED && b.wire.notices == 1);
    L::ControlObservation seen;
    CHECK(L::getControl(c, b.wire.live, b.current(), b.now, AGE, seen));
    CHECK(seen.target.channel.binding.configurationGeneration == 8);
    b.wire.settings = nullptr;
    { // A download that crossed the boundary with no notice cannot settle a transaction.
        Bench r;
        LoopEvidence f(r);
        L::DriverSettingsContext d;
        CHECK(prepareLoop(d, r, f, L::LoopMode::OPEN_LOOP));
        L::PreparedRead step;
        CHECK(L::nextDriver(d, r.wire.live, r.wire.current, r.now, step) && step.ready);
        const C::SdoCompletion unnoticed = r.exchange(step.transfer);
        CHECK(unnoticed.outcome == C::SdoOutcome::SUCCESS);
        const Saved<L::DriverSettingsContext> saved(d);
        const M::Status status = L::advanceDriver(d, r.wire.live, r.wire.current, unnoticed, r.now);
        CHECK(!status && status.detail == int32_t(M::ReadError::CORRELATION));
        saved.unchanged(d);
    }
    { // A second notice for one download refuses, and the binding stays where the first left it.
        Bench r;
        LoopEvidence f(r);
        L::DriverSettingsContext d;
        CHECK(prepareLoop(d, r, f, L::LoopMode::OPEN_LOOP));
        CHECK(L::noteDriverTransmission(d, r.wire.live, r.wire.current, r.now));
        M::Binding binding = r.wire.current;
        CHECK(binding.configurationGeneration == 8);
        CHECK(!L::noteDriverTransmission(d, r.wire.live, binding, r.now));
        CHECK(binding.configurationGeneration == 8);
    }
}

// The 31 tuning words read through the owner stay raw, and a selector write leaves everything else alone.
void loopTouchesNothingElse() {
    Bench b;
    const unsigned first = b.mark();
    const unsigned downloads = b.drive.downloads;
    L::TuningObservation seen;
    const L::TuningGroup groups[] = {L::TuningGroup::CURRENT_HOLDING, L::TuningGroup::FILTER_POSITION,
                                     L::TuningGroup::LOOP_GAINS, L::TuningGroup::EDS_GAINS};
    unsigned words = 0;
    for (L::TuningGroup group : groups) {
        L::DriverContext context;
        const uint32_t op = b.begin();
        CHECK(L::prepareTuningRead(context, b.current(), op, b.now, DEADLINE, options(), group));
        while (context.read.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextDriver(context, b.now, work) && work.ready);
            CHECK(work.transfer.direction == C::SdoDirection::UPLOAD);
            CHECK(work.transfer.index >= 0x2502 && work.transfer.index <= 0x2520 && !work.transfer.subindex);
            const C::SdoCompletion terminal = b.exchange(work.transfer);
            CHECK(L::advanceDriver(context, terminal, b.now));
            ++words;
        }
        CHECK(L::getTuning(context, b.current(), b.now, AGE, seen) && seen.group == group);
    }
    CHECK(words == 31 && seen.edsGainsRaw[6] == 0x1020);
    CHECK(b.drive.downloads == downloads && b.wire.sentCount == first + 31);
    uint32_t before[74];
    for (unsigned i = 0; i < 74; ++i)
        before[i] = b.drive.table[i].raw;
    LoopEvidence e(b);
    L::DriverSettingsContext c;
    CHECK(prepareLoop(c, b, e, L::LoopMode::OPEN_LOOP));
    b.run(c);
    CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.downloads == downloads + 1);
    for (unsigned i = 0; i < 74; ++i)
        if (b.drive.table[i].index != 0x2500)
            CHECK(b.drive.table[i].raw == before[i]);
}
} // namespace

int main() {
    positionParametersEndToEnd();
    changedFieldsOnlyAndSourceOrder();
    everyDocumentedBoundary();
    unknownRawReadsStayRawAndAdmitNothing();
    completeCandidateRejectedBeforeTransmission();
    failedStateReadIsRefused();
    noOpIsTrafficFree();
    aliasedAndStaleContexts();
    queuedBytesAreImmutable();
    partialAcknowledgementsAreRetained();
    lostReplyIsUncertainAndNeverReplayed();
    readbackMismatchPublishesNothing();
    cancellationBeforeAndAfterHandoff();
    rebootEndsTheTransaction();
    deadlineWins();
    generationsAndOldTargetAdmission();
    softLimitStateRules();
    frozenTokenCannotPrepareAFalseNoOp();
    diEveryCodeAndMaskOnTheWire();
    diCandidatesRefusedWhole();
    diOrderIsNotAtomicAndDuplicatesStand();
    diWrongEvidenceRefused();
    diAliasedAndEditedContexts();
    diPartialEffectsSurvive();
    diLostReplyIsNeverReplayed();
    diReadbackMismatchAndFailure();
    diCancellationAndPreflight();
    diDeadlines();
    diFaultRebootAndBusOff();
    diGenerationsAndExclusion();
    diUnusedAndMethodFixtures();
    loopEndToEnd();
    loopEveryTransitionOnTheWire();
    loopCandidatesRefusedWhole();
    loopStateRules();
    loopFrozenTokenAndAliases();
    loopCandidateIsImmutable();
    loopFailedDownload();
    loopFailedReadback();
    loopCancellationAndPreflight();
    loopDeadlines();
    loopFaultRebootAndBusOff();
    loopGenerationsAndRetirement();
    loopTouchesNothingElse();
    std::printf("settings_owner: %u checks\n", checks);
    return 0;
}
