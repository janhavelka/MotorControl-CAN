// SPDX-License-Identifier: MIT
// Explicit record/re-application through the real CanBusOwner with scripted transport traffic.
#include "ConfigurationRestore.h"
#include "CanBusOwner.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace A = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                     \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(std::memcmp(bytes, &value, sizeof value) == 0); }
};
} // namespace
#include "support/OwnerRig.h"
namespace {
L::ReadOptions options() {
    L::ReadOptions o;
    o.queueTimeoutUs = o.responseTimeoutUs = 100;
    return o;
}
struct Wire : OwnerRig::WireBase {
    L::DriverSettingsContext* settings = nullptr;
    const L::AxisReservation* reservation = nullptr;
    M::Binding* current = nullptr;
    unsigned notices = 0;
    static bool guard(void* context, O::SequenceId, const C::SdoTransfer& work, uint64_t now) {
        Wire& wire = static_cast<Wire&>(*static_cast<OwnerRig::WireBase*>(context));
        if (wire.settings && work.direction == C::SdoDirection::DOWNLOAD) {
            CHECK(work.index == 0x6081 || work.index == 0x6083 || work.index == 0x6084);
            CHECK(work.size == 4 && work.subindex == 0);
            CHECK(L::noteDriverTransmission(*wire.settings, *wire.reservation, *wire.current, now));
            ++wire.notices;
        }
        return true;
    }
    O::Transport transport() {
        O::Transport t = plumbing();
        t.beforeTransmit = guard;
        return t;
    }
};
struct Owner {
    C::SdoClient service;
    Wire wire;
    O::CanBusOwner owner;
    O::SequenceId sequence;
    L::AxisReservation reservation;
    M::Binding current;
    uint64_t now;
    uint32_t operation = 0;
    unsigned restoredWrites = 0;
    explicit Owner(unsigned node = 5, uint64_t start = 10)
        : owner(service, wire.transport(), 3), now(start) {
        L::ReadTarget& t = reservation.target;
        t.channel.binding.logicalAxisId = 17;
        t.channel.binding.endpoint.profile = 41;
        t.channel.binding.endpoint.address = node;
        t.channel.binding.endpoint.bus = 2;
        t.channel.binding.bindingGeneration = node;
        t.channel.binding.configurationGeneration = 7;
        t.channel.clientToServerId = 0x600 + node;
        t.channel.serverToClientId = 0x580 + node;
        t.nodeGeneration = 11;
        current = t.channel.binding;
        reservation.model = L::SettingsModel::CL86_C;
        CHECK(service.configure(0, t.channel, true, true));
        C::NetworkRoute route;
        route.binding = current;
        route.generation = 11;
        route.heartbeatId = 0x700 + node;
        route.emcyId = 0x80 + node;
        CHECK(service.configureNetwork(0, route, C::NetworkObserver(), now));
        CHECK(owner.beginSequence(current, 1, now, sequence));
        const O::Sequence* live = owner.sequence(sequence);
        CHECK(live);
        reservation.owner = owner.identity();
        reservation.sequence = uint32_t(sequence.slot + 1);
        reservation.generation = sequence.generation;
        reservation.acquiredUs = live->acquiredUs;
        wire.reservation = &reservation;
        wire.current = &current;
    }
    uint32_t begin() {
        CHECK(owner.beginOperation(sequence, ++operation, 5000, now));
        return operation;
    }
    void turn(uint64_t time) {
        now = time;
        CHECK(owner.service(now, OwnerRig::budget()));
    }
    C::SdoCompletion exchange(const C::SdoTransfer& work, uint32_t raw,
                              L::DriverSettingsContext* settings = nullptr, unsigned failure = 0) {
        O::BusRequest request;
        request.sequence = sequence;
        request.transfer = work;
        O::RequestId id;
        CHECK(owner.admit(request, now, id) == O::BusAdmission::ACCEPTED);
        wire.settings = settings;
        const unsigned sent = wire.sentCount;
        const unsigned notices = wire.notices;
        turn(now);
        wire.settings = nullptr;
        CHECK(wire.sentCount == sent + 1);
        const M::Frame& tx = wire.sent[sent];
        CHECK(tx.data[1] == uint8_t(work.index) && tx.data[2] == uint8_t(work.index >> 8));
        if (settings && work.direction == C::SdoDirection::DOWNLOAD) {
            CHECK(wire.notices == notices + 1);
            ++restoredWrites;
        }
        if (failure == 2) {
            // The write reached the fake wire; withhold its reply and let the ordinary owner expire it.
            turn(now + 1);
            turn(now + work.responseTimeoutUs);
        } else {
            M::Frame reply;
            reply.id = work.channel.serverToClientId;
            reply.dlc = reply.payloadLength = 8;
            reply.data[0] = failure == 1 ? 0x80 : work.direction == C::SdoDirection::DOWNLOAD ? 0x60
                                               : work.size == 1 ? 0x4f : work.size == 2 ? 0x4b : 0x43;
            reply.data[1] = uint8_t(work.index);
            reply.data[2] = uint8_t(work.index >> 8);
            reply.data[3] = work.subindex;
            if (failure == 1)
                raw = 0x80000001;
            if (failure || work.direction == C::SdoDirection::UPLOAD)
                for (unsigned i = 0; i < 4; ++i)
                    reply.data[4 + i] = uint8_t(raw >> (8 * i));
            wire.push(reply, now + 1);
            turn(now + 1);
        }
        const O::Completion* terminal = owner.result(id);
        CHECK(terminal && terminal->reason == O::CompletionReason::SERVICE);
        const C::SdoCompletion copy = terminal->service;
        CHECK(owner.release(id));
        return copy;
    }
    void fill(L::ReadContext& read, const uint32_t* words, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            L::PreparedRead p;
            CHECK(L::nextRead(read, now, p) && p.ready);
            const C::SdoCompletion e = exchange(p.transfer, words[i]);
            CHECK(L::advanceRead(read, e, now));
        }
    }
    L::ReadContext identity(uint32_t firmware = 0x0200) {
        L::ReadContext c;
        CHECK(L::prepareIdentity(c, reservation.target, begin(), now, 5000, options()));
        const uint32_t words[] = {0x00040192, 0x0c, 0x20, 5, 0x1000, 1, 0, 0x400, firmware};
        fill(c, words, 9);
        return c;
    }
    L::DriverContext motion(bool original) {
        L::DriverContext c;
        CHECK(L::prepareDriverRead(c, reservation.target, begin(), now, 5000, options(),
                                  L::DriverGroup::MOTION_PARAMETERS));
        const uint32_t words[] = {original ? 100u : 102u, original ? 200u : 202u,
                                   original ? 300u : 302u};
        fill(c.read, words, 3);
        return c;
    }
    C::SdoCompletion control() {
        C::SdoTransfer work;
        work.channel = reservation.target.channel;
        work.operation = work.request = begin();
        work.step = 1;
        work.index = 0x6040;
        work.direction = C::SdoDirection::DOWNLOAD;
        work.size = 2;
        work.data[0] = 0x0f;
        work.deadlineUs = 5000;
        work.queueTimeoutUs = work.responseTimeoutUs = 100;
        return exchange(work, 0);
    }
    L::ReadContext state() {
        L::ReadContext c;
        CHECK(L::prepareState(c, reservation.target, begin(), now, 5000, options(),
            uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                    (1u << unsigned(L::StateBlock::STATUS)))));
        const uint32_t words[] = {0, 0, 0, 7, 1};
        fill(c, words, 5);
        return c;
    }
};
A::ConfigurationRecord record() {
    Owner owner;
    const L::ReadContext id = owner.identity();
    const L::DriverContext motion = owner.motion(true);
    A::ConfigurationRecord out;
    CHECK(A::captureConfiguration(out, owner.reservation, owner.reservation.target, id, motion,
                                  10, 20, 30, owner.now, 100));
    CHECK(out.configuration.positionSpeedRpm == 100 && out.configuration.rampUpTimeMs == 200 &&
          out.configuration.rampDownTimeMs == 300 && out.configuration.fieldCount == 3);
    CHECK(out.identity.fields[0].earliestUs == 10 && out.capturedUs == owner.now);
    return out;
}
struct Fixture {
    A::ConfigurationRecord original = record();
    Owner owner;
    L::ReadContext identity;
    C::SdoCompletion control;
    L::DriverContext prior;
    L::ReadContext state;
    A::ConfigurationRestoreReview review;
    Fixture(unsigned node = 5, uint32_t firmware = 0x0200) : owner(node, 100) {
        identity = owner.identity(firmware);
        control = owner.control();
        prior = owner.motion(false);
        state = owner.state();
        review.target = owner.reservation.target;
        review.fixtureRecord = 20;
        review.firmwareRecord = 30;
        review.routeRecord = 40;
    }
    M::Status prepare(A::ConfigurationRestoreContext& c) {
        const uint32_t operation = owner.begin();
        return A::prepareConfigurationRestore(c, original, review, owner.reservation,
            owner.reservation.target, identity, prior, state, control, operation, owner.now, 5000, options(), 100);
    }
};
void captureValidation() {
    Owner owner;
    L::ReadContext identity = owner.identity();
    L::DriverContext motion = owner.motion(true);
    A::ConfigurationRecord out;
    const Saved<A::ConfigurationRecord> before(out);
    for (unsigned bad = 0; bad < 6; ++bad) {
        L::AxisReservation r = owner.reservation;
        L::ReadContext id = identity;
        L::DriverContext config = motion;
        if (bad == 0)
            r.owner = 0;
        if (bad == 1)
            r.model = L::SettingsModel::UNKNOWN;
        if (bad == 2)
            ++r.target.nodeGeneration;
        if (bad == 3)
            r.acquiredUs = owner.now;
        if (bad == 4)
            id.uncertain = true;
        if (bad == 5)
            config.read.invalidated = true;
        CHECK(!A::captureConfiguration(out, r, owner.reservation.target, id, config,
                                      10, 20, 30, owner.now, 100));
        before.unchanged(out);
    }
    CHECK(!A::captureConfiguration(out, owner.reservation, owner.reservation.target, identity,
                                  motion, 0, 20, 30, owner.now, 100));
    before.unchanged(out);
    CHECK(!A::captureConfiguration(out, owner.reservation, owner.reservation.target, identity,
                                  motion, 10, 20, 30, owner.now + 101, 100));
    before.unchanged(out);
}
void preparationValidation() {
    for (unsigned bad = 0; bad < 10; ++bad) {
        Fixture f;
        if (bad == 0)
            f.review.fixtureRecord = 21;
        if (bad == 1)
            f.review.firmwareRecord = 31;
        if (bad == 2)
            f.review.routeRecord = 0;
        if (bad == 3)
            f.original.model = L::SettingsModel::OL86_C;
        if (bad == 4)
            f.original.configuration.positionSpeedRpm = 3001;
        if (bad == 5)
            f.control.transfer.data[0] = 6;
        if (bad == 6)
            f.state.uncertain = true;
        if (bad == 7)
            f.identity.invalidated = true;
        if (bad == 8)
            f.owner.reservation.owner = 0;
        if (bad == 9)
            ++f.review.target.nodeGeneration;
        A::ConfigurationRestoreContext c;
        const Saved<A::ConfigurationRestoreContext> before(c);
        CHECK(!f.prepare(c));
        before.unchanged(c);
        CHECK(f.owner.restoredWrites == 0);
    }
    Fixture changed(6, 0x0201);
    A::ConfigurationRestoreContext c;
    const Saved<A::ConfigurationRestoreContext> before(c);
    CHECK(!changed.prepare(c));
    before.unchanged(c);
    Fixture valid;
    CHECK(valid.prepare(c));
    const Saved<A::ConfigurationRestoreContext> retained(c);
    CHECK(!A::prepareConfigurationRestore(c, c.original, valid.review, valid.owner.reservation,
        valid.owner.reservation.target, valid.identity, valid.prior, valid.state, valid.control,
        10, valid.owner.now, 5000, options(), 100));
    retained.unchanged(c);
}
void restoredAndPartial() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        Fixture f(scenario == 0 ? 6 : 5);
        // All three fields differ so later failure preserves a meaningful acknowledged prefix.
        A::ConfigurationRestoreContext c;
        CHECK(f.prepare(c));
        const Saved<A::ConfigurationRecord> preserved(c.original);
        CHECK(c.settings.changed == 7 && c.settings.confirmed == 0);
        unsigned steps = 0;
        while (c.settings.state == L::SettingsState::WRITING ||
               c.settings.state == L::SettingsState::READING_BACK) {
            CHECK(++steps <= 6);
            if (c.settings.state == L::SettingsState::READING_BACK &&
                !M::sameBinding(f.owner.service.channel(0)->binding, f.owner.current))
                CHECK(f.owner.owner.advanceConfiguration(f.owner.sequence, f.owner.current,
                                                          f.owner.now));
            L::PreparedRead p;
            CHECK(L::nextDriver(c.settings, f.owner.reservation, f.owner.current, f.owner.now, p));
            CHECK(p.ready);
            const unsigned failure = scenario == 1 && steps == 2 ? 1
                                     : scenario == 2 && steps == 1 ? 2 : 0;
            const unsigned field = p.transfer.index == 0x6081 ? 0 : p.transfer.index == 0x6083 ? 1 : 2;
            uint32_t raw = c.settings.requested[field];
            if (scenario == 3 && steps == 6)
                ++raw;
            const C::SdoCompletion done = f.owner.exchange(p.transfer, raw, &c.settings, failure);
            CHECK(L::advanceDriver(c.settings, f.owner.reservation, f.owner.current, done, f.owner.now));
            preserved.unchanged(c.original);
        }
        if (scenario == 0) {
            CHECK(c.settings.state == L::SettingsState::SUCCEEDED && c.settings.confirmed == 7);
            CHECK(f.owner.restoredWrites == 3 && c.original.identity.target.channel.binding.endpoint.address == 5 &&
                  c.identity.target.channel.binding.endpoint.address == 6);
        } else {
            CHECK(c.settings.state == L::SettingsState::FAILED);
            CHECK(scenario == 1 ? c.settings.confirmed == 1 && f.owner.restoredWrites == 2
                  : scenario == 2 ? c.settings.confirmed == 0 && c.settings.uncertain
                                  : c.settings.confirmed == 7 && c.settings.outcome == M::ReadOutcome::REPLY_ERROR);
        }
        L::PreparedRead done;
        done.ready = true;
        CHECK(L::nextDriver(c.settings, f.owner.reservation, f.owner.current, f.owner.now, done));
        CHECK(!done.ready);
    }
    Fixture unchanged;
    unchanged.prior = unchanged.owner.motion(true);
    A::ConfigurationRestoreContext same;
    CHECK(unchanged.prepare(same));
    CHECK(same.settings.state == L::SettingsState::SUCCEEDED && same.settings.changed == 0 &&
          !same.settings.effectsPossible && unchanged.owner.restoredWrites == 0);
    L::PreparedRead noWork;
    noWork.ready = true;
    CHECK(L::nextDriver(same.settings, unchanged.owner.reservation, unchanged.owner.current,
                        unchanged.owner.now, noWork) && !noWork.ready);
}
} // namespace
int main() {
    captureValidation();
    preparationValidation();
    restoredAndPartial();
    std::printf("configuration restore: %u checks passed\n", checks);
}
