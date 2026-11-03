// SPDX-License-Identifier: MIT
// Explicit settings through the real CanBusOwner and SdoClient, against a scripted object dictionary
// holding the values both recorded CL86-C specimens reported. Wire bytes are literal; nothing here
// is a device measurement, and the scripted answers prove only what the library does with them.
#include "CanBusOwner.h"
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>
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
    Entry table[27] = {{0x2100, 0, 2, 1},      {0x2101, 0, 2, 0},
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
                       {0x2400, 8, 2, 0}};
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
    unsigned guards = 0, notices = 0;
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
L::DirectionSettings direction(uint16_t code) {
    L::DirectionSettings s;
    s.direction = static_cast<L::DefaultDirection>(code);
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
    return L::prepareDriverSettings(out, b.wire.live, e.prior, e.state, e.control, s, op, b.now,
                                    deadline, options(), AGE);
}
struct SelectorEvidence {
    L::ReadContext state;
    L::DriverContext driver, limits;
    explicit SelectorEvidence(Bench& b) {
        b.readState(state);
        b.read(limits, L::DriverGroup::SOFT_LIMITS);
        b.read(driver, L::DriverGroup::DRIVER);
    }
};
M::Status prepareDirection(L::DriverSettingsContext& out, Bench& b, const SelectorEvidence& e,
                           uint16_t code, uint64_t deadline = DEADLINE) {
    const uint32_t op = b.begin(deadline);
    return L::prepareDirectionSettings(out, b.wire.live, e.driver, e.limits, e.state,
                                       direction(code), op, b.now, deadline, options(), AGE);
}
M::Status prepareLimit(L::DriverSettingsContext& out, Bench& b, const SelectorEvidence& e,
                       uint16_t code, uint64_t deadline = DEADLINE) {
    const uint32_t op = b.begin(deadline);
    return L::prepareSoftLimitSettings(out, b.wire.live, e.limits, e.state, limit(code), op, b.now,
                                       deadline, options(), AGE);
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
        for (bool isDirection : {true, false}) {
            Bench b;
            // Start from the other documented code, so codes 0 and 1 both need a real write.
            if (isDirection)
                b.drive.set(0x2200, 0, code == 0 ? 1 : 0);
            else
                b.drive.set(0x2202, 0, code == 0 ? 1 : 0);
            const uint32_t before = isDirection ? b.drive.value(0x2200) : b.drive.value(0x2202);
            SelectorEvidence e(b);
            L::DriverSettingsContext c;
            const auto prepare = [&]() {
                return isDirection ? prepareDirection(c, b, e, code) : prepareLimit(c, b, e, code);
            };
            if (code > 1) {
                refused(b, c, prepare);
                CHECK((isDirection ? b.drive.value(0x2200) : b.drive.value(0x2202)) == before);
                continue;
            }
            CHECK(prepare());
            b.run(c);
            CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == 1);
            CHECK((isDirection ? b.drive.value(0x2200) : b.drive.value(0x2202)) == code);
            L::DriverObservation seen;
            CHECK(L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
            CHECK((isDirection ? seen.directionRaw : seen.softLimitSelectorRaw) == code);
            CHECK(!seen.interpreted);
        }
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
        CHECK(L::prepareIoSettings(c, b.wire.live, prior, s, op, b.now, DEADLINE, options(), AGE));
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
            return L::prepareIoSettings(r, b.wire.live, fresh, reserved, op, b.now, DEADLINE,
                                        options(), AGE);
        });
        L::IoSettings badCode = s;
        badCode.functions[6] = static_cast<L::InputFunction>(14);
        refused(b, r, [&] {
            return L::prepareIoSettings(r, b.wire.live, fresh, badCode, op, b.now, DEADLINE,
                                        options(), AGE);
        });
        // The same read admits a valid candidate, so those two refusals isolate the bad value.
        CHECK(L::prepareIoSettings(r, b.wire.live, fresh, s, op, b.now, DEADLINE, options(), AGE));
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
        refused(b, c, [&] { return prepareDirection(c, b, e, 1); }); // Not known to be off.
    }
    { // 2200 outside its documented codes.
        Bench b;
        b.drive.set(0x2200, 0, 7);
        SelectorEvidence e(b);
        L::DriverObservation seen;
        CHECK(L::getDriver(e.driver, b.current(), b.now, AGE, seen));
        CHECK(seen.directionRaw == 7 && seen.subdivisionRaw == 1000);
        L::DriverSettingsContext c;
        refused(b, c, [&] { return prepareDirection(c, b, e, 0); });
        refused(b, c, [&] { return prepareDirection(c, b, e, 1); });
    }
    // The subdivision word is observed, retained and rechecked, never written, whatever it holds:
    // the EDS default 0, the specimens' 1000, both ends of each documented range, and beyond.
    for (uint32_t raw : {0u, 399u, 400u, 1000u, 51200u, 51201u, 65535u}) {
        Bench b;
        b.drive.set(0x2201, 0, raw);
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareDirection(c, b, e, 1));
        const unsigned downloads = b.drive.downloads;
        b.run(c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.downloads == downloads + 1);
        CHECK(b.drive.value(0x2201) == raw);
        L::DriverObservation seen;
        CHECK(L::getDriver(c, b.wire.live, b.current(), b.now, AGE, seen));
        CHECK(seen.subdivisionRaw == raw);
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
            return L::prepareDriverSettings(c, live, prior, state, control, motion(), operation, b.now,
                                            deadline, policy, age);
        });
    }
    // The same evidence admits the candidate, so the cases above isolate what they spoil.
    Bench b;
    MotionEvidence e(b);
    L::DriverSettingsContext c;
    CHECK(prepareMotion(c, b, e, motion()));
    // A selector candidate is refused whole for the same reasons.
    for (unsigned k = 0; k < 11; ++k) {
        Bench s;
        if (k == 0)
            s.drive.set(0x2100, 0, 0x0005); // Reports running.
        if (k == 1)
            s.drive.set(0x2100, 0, 0x0009); // Reports an alarm.
        if (k == 2)
            s.drive.set(0x2202, 0, 1); // Limits on: direction is refused until set off explicitly.
        L::ReadContext state;
        L::DriverContext limits, driver;
        s.readState(state);
        s.read(limits, L::DriverGroup::SOFT_LIMITS);
        s.read(driver, L::DriverGroup::DRIVER);
        L::AxisReservation live = s.wire.live;
        const L::DriverContext* priorRead = &driver;
        const L::DriverContext* limitsRead = &limits;
        L::DriverContext unrelated;
        uint32_t operation = s.begin();
        uint64_t deadline = DEADLINE, age = AGE;
        if (k == 3)
            live.model = L::SettingsModel::UNKNOWN;
        if (k == 4)
            priorRead = &limits; // Wrong group for 2200.
        if (k == 5)
            limitsRead = &driver; // Wrong group for 2202.
        if (k == 6)
            operation = 0;
        if (k == 7)
            deadline = s.now;
        if (k == 8)
            age = 0;
        if (k == 9) {
            s.read(unrelated, L::DriverGroup::MOTION_PARAMETERS); // Complete, but the wrong group.
            priorRead = &unrelated;
            operation = s.begin();
        }
        if (k == 10)
            live.acquiredUs = s.now + 1; // The evidence precedes this reservation.
        L::DriverSettingsContext out;
        refused(s, out, [&] {
            return L::prepareDirectionSettings(out, live, *priorRead, *limitsRead, state, direction(1),
                                               operation, s.now, deadline, options(), age);
        });
    }
    for (unsigned k = 0; k < 2; ++k) {
        Bench s;
        s.drive.set(0x2100, 0, k ? 0x0009 : 0x0005);
        SelectorEvidence e(s);
        L::DriverSettingsContext out;
        refused(s, out, [&] { return prepareLimit(out, s, e, 1); });
    }
    { // A complete read of the wrong group is not a soft-limit prior, though its unused fields are 0.
        Bench s;
        SelectorEvidence e(s);
        const uint32_t operation = s.begin();
        L::DriverSettingsContext out;
        refused(s, out, [&] {
            return L::prepareSoftLimitSettings(out, s.wire.live, e.driver, e.state, limit(0), operation,
                                               s.now, DEADLINE, options(), AGE);
        });
    }
    // Each prerequisite must follow the reservation on its own: a fresh prior read cannot carry an
    // older state or soft-limit read across it.
    for (unsigned stale = 0; stale < 2; ++stale) {
        Bench s;
        L::ReadContext state;
        L::DriverContext limits, driver;
        if (stale)
            s.read(limits, L::DriverGroup::SOFT_LIMITS);
        else
            s.readState(state);
        L::AxisReservation live = s.wire.live;
        live.acquiredUs = s.now + 1;
        s.turn(s.now + 1);
        if (stale)
            s.readState(state);
        else
            s.read(limits, L::DriverGroup::SOFT_LIMITS);
        s.read(driver, L::DriverGroup::DRIVER);
        const uint32_t operation = s.begin();
        L::DriverSettingsContext out;
        refused(s, out, [&] {
            return L::prepareDirectionSettings(out, live, driver, limits, state, direction(1), operation,
                                               s.now, DEADLINE, options(), AGE);
        });
        if (!stale) {
            refused(s, out, [&] {
                return L::prepareSoftLimitSettings(out, live, limits, state, limit(1), operation, s.now,
                                                   DEADLINE, options(), AGE);
            });
        }
    }
}

// Packet 10's note. STATUS and MONITORS complete, a sibling block then fails: the blocks stay readable
// as history, and no settings call admits them. The same rule validateStop applies.
void failedStateReadIsRefused() {
    for (Drive::Fault fault : {Drive::ABORT, Drive::LOST}) {
        Bench b;
        b.drive.set(0x6061, 0, 1);
        const C::SdoCompletion control = b.control();
        L::DriverContext prior, driver, limits;
        b.read(prior, L::DriverGroup::MOTION_PARAMETERS);
        b.read(driver, L::DriverGroup::DRIVER);
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
            return L::prepareDriverSettings(c, b.wire.live, prior, state, control, motion(), op, b.now,
                                            DEADLINE, options(), AGE);
        });
        refused(b, c, [&] {
            return L::prepareDirectionSettings(c, b.wire.live, driver, limits, state, direction(1), op,
                                               b.now, DEADLINE, options(), AGE);
        });
        refused(b, c, [&] {
            return L::prepareSoftLimitSettings(c, b.wire.live, limits, state, limit(1), op, b.now,
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
            return L::prepareDriverSettings(c, b.wire.live, prior, forged, control, motion(), op, b.now,
                                            DEADLINE, options(), AGE);
        });
        forged = fresh;
        forged.state = M::ReadState::FAILED;
        refused(b, c, [&] {
            return L::prepareDriverSettings(c, b.wire.live, prior, forged, control, motion(), op, b.now,
                                            DEADLINE, options(), AGE);
        });
        CHECK(L::prepareDriverSettings(c, b.wire.live, prior, fresh, control, motion(), op, b.now,
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
    { // Selectors, and an earlier prepared target stays valid across them.
        Bench b;
        SelectorEvidence e(b);
        M::AxisConfig axis;
        const M::PreparedTarget prepared = preparedTarget(b, axis);
        L::DriverSettingsContext direct, limits;
        const unsigned sent = b.mark();
        CHECK(prepareDirection(direct, b, e, 0));
        CHECK(prepareLimit(limits, b, e, 0));
        CHECK(direct.state == L::SettingsState::SUCCEEDED && !direct.changed);
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
    L::DriverSettingsContext c;
    CHECK(prepareDirection(c, b, e, 1));
    const uint32_t op = b.operation;
    const Saved<L::DriverSettingsContext> saved(c);
    const auto callAliased = [&](M::Status status) {
        CHECK(!status && status.detail == int32_t(M::ReadError::OVERLAP));
        saved.unchanged(c);
    };
    L::DirectionSettings wanted = direction(1);
    L::SoftLimitSettings wantedLimit = limit(1);
    L::ReadOptions policy = options();
    // Every input that can sit inside the output refuses, before any work.
    callAliased(L::prepareDirectionSettings(c, c.reservation, e.driver, e.limits, e.state, wanted, op,
                                            b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareDirectionSettings(c, b.wire.live, c.readback, e.limits, e.state, wanted, op,
                                            b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareDirectionSettings(c, b.wire.live, e.driver, c.readback, e.state, wanted, op,
                                            b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareDirectionSettings(c, b.wire.live, e.driver, e.limits, c.readback.read, wanted,
                                            op, b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareDirectionSettings(
        c, b.wire.live, e.driver, e.limits, e.state,
        at<L::DirectionSettings>(&c.requested[0]), op, b.now, DEADLINE, policy,
        AGE));
    callAliased(L::prepareDirectionSettings(c, b.wire.live, e.driver, e.limits, e.state, wanted, op,
                                            b.now, DEADLINE, c.readback.read.options, AGE));
    callAliased(L::prepareSoftLimitSettings(c, c.reservation, e.limits, e.state, wantedLimit, op, b.now,
                                            DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, c.readback, e.state, wantedLimit, op, b.now,
                                            DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, e.limits, c.readback.read, wantedLimit, op,
                                            b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, e.limits, e.state,
                                            at<L::SoftLimitSettings>(&c.requested[0]),
                                            op, b.now, DEADLINE, policy, AGE));
    callAliased(L::prepareSoftLimitSettings(c, b.wire.live, e.limits, e.state, wantedLimit, op, b.now,
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
    CHECK(c.state == L::SettingsState::SUCCEEDED && b.drive.value(0x2200) == 1);
    // A preparation checks its evidence against the token's own target, so a frozen token and its
    // old evidence still agree. Nothing can come of it against the actual binding: the no-op below
    // would be wrong, since the drive now holds 1, and it publishes nothing.
    L::DriverSettingsContext stalePlan;
    L::DriverObservation seen;
    const Saved<L::DriverObservation> none(seen);
    CHECK(prepareDirection(stalePlan, b, e, 0));
    CHECK(stalePlan.state == L::SettingsState::SUCCEEDED && !stalePlan.changed);
    CHECK(!L::getDriver(stalePlan, b.wire.live, b.current(), b.now, AGE, seen));
    none.unchanged(seen);
    CHECK(!L::nextDriver(stalePlan, b.wire.live, b.wire.current, b.now, work));
    CHECK(prepareLimit(stalePlan, b, e, 1));
    CHECK(stalePlan.state == L::SettingsState::WRITING);
    CHECK(!L::nextDriver(stalePlan, b.wire.live, b.wire.current, b.now, work));
    binding = b.wire.current;
    CHECK(!L::noteDriverTransmission(stalePlan, b.wire.live, binding, b.now));
    CHECK(binding.configurationGeneration == b.wire.current.configurationGeneration);
    // With a current token the same old evidence is refused outright.
    b.renew();
    L::DriverSettingsContext next;
    refused(b, next, [&] { return prepareDirection(next, b, e, 0); });
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
    CHECK(prepareDirection(active, r, f, 1));
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
    { // A write that disturbs its retained neighbour is caught: 2201 moved by a 2200 write.
        Bench b;
        SelectorEvidence e(b);
        L::DriverSettingsContext c;
        CHECK(prepareDirection(c, b, e, 1));
        b.drive.lieIndex = 0x2201;
        b.drive.lieRaw = 2000;
        b.run(c);
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR);
        CHECK(c.confirmed == 1 && c.readback.read.values[1].raw == 2000);
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
        CHECK(prepareDirection(c, b, e, 1));
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
        CHECK(prepareDirection(c, b, e, 1));
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
        CHECK(prepareDirection(c, b, e, 1, deadline));
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
        CHECK(prepareDirection(c, b, e, 1));
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
        CHECK(!L::getDriver(e.driver, b.current(), b.now, AGE, seen));
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
            return L::prepareDriverSettings(blocked, b.wire.live, prior, state, control,
                                            motion(70, 110, 120), op, b.now, DEADLINE, options(), AGE);
        });
        // A fresh read sees the move's value, and the same candidate is real work, not a no-op.
        L::DriverContext fresh;
        b.read(fresh, L::DriverGroup::MOTION_PARAMETERS);
        const uint32_t op2 = b.begin();
        L::DriverSettingsContext settings;
        CHECK(L::prepareDriverSettings(settings, b.wire.live, fresh, state, control, motion(70, 110, 120),
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
        CHECK(prepareDirection(c, b, e, 1));
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
    std::printf("settings_owner: %u checks\n", checks);
    return 0;
}
