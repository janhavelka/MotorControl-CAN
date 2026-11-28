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
    L::DriverContext group(L::DriverGroup which, const uint32_t* words, unsigned count) {
        L::DriverContext c;
        CHECK(L::prepareDriverRead(c, reservation.target, begin(), now, 5000, options(), which));
        fill(c.read, words, count);
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
// Prompt 61, decision 3: the backup record before a factory restore lists what it did not read and is never
// complete; after a restore the comparison re-applies only fields with a typed write that can name the
// recorded value, reports every other change as changed and every field not read again as unknown.
void backupRecordAndCompare() {
    Owner owner;
    const L::ReadTarget& target = owner.reservation.target;
    const uint32_t driver[] = {0, 1000}, limits[] = {0, 2, uint32_t(-1000), 1000}, motion[] = {100, 200, 300},
                   io[] = {0x0f, 0x0101, 1, 2, 3, 7, 8, 9, 10}, control[] = {2, 4000};
    A::PersistenceBackup backup;
    CHECK(!backup.recordId && !backup.complete && backup.unknown == A::BACKUP_UNKNOWN_ALL &&
          backup.groupsUnknown == A::BACKUP_GROUPS_ALL);
    const L::DriverContext d = owner.group(L::DriverGroup::DRIVER, driver, 2);
    {
        const Saved<A::PersistenceBackup> saved(backup);
        CHECK(!A::recordBackupGroup(backup, 0, L::DriverGroup::DRIVER, d, target, owner.now, 5000));
        CHECK(!A::recordBackupGroup(backup, 7, L::DriverGroup::HOMING, d, target, owner.now, 5000));
        CHECK(!A::recordBackupGroup(backup, 7, L::DriverGroup::SOFT_LIMITS, d, target, owner.now, 5000));
        saved.unchanged(backup);
    }
    CHECK(A::recordBackupGroup(backup, 7, L::DriverGroup::DRIVER, d, target, owner.now, 5000));
    CHECK(backup.recordId == 7 && backup.fieldCount == 2 && !backup.complete);
    {
        const Saved<A::PersistenceBackup> saved(backup);
        CHECK(!A::recordBackupGroup(backup, 7, L::DriverGroup::DRIVER, d, target, owner.now, 5000)); // Once.
        CHECK(!A::recordBackupGroup(backup, 8, L::DriverGroup::DRIVER, d, target, owner.now, 5000)); // One identity.
        saved.unchanged(backup);
    }
    // The read completes first: its publication time is the owner's clock after its last reply.
    const auto rec = [&](A::PersistenceBackup& b, uint32_t id, L::DriverGroup g, const uint32_t* w, unsigned n) {
        const L::DriverContext read = owner.group(g, w, n);
        CHECK(A::recordBackupGroup(b, id, g, read, target, owner.now, 5000));
    };
    const auto all = [&](A::PersistenceBackup& b, uint32_t id, const uint32_t* l, const uint32_t* m, const uint32_t* i,
                         bool withControl) {
        rec(b, id, L::DriverGroup::SOFT_LIMITS, l, 4);
        rec(b, id, L::DriverGroup::MOTION_PARAMETERS, m, 3);
        rec(b, id, L::DriverGroup::IO_INPUT_CONFIGURATION, i, 9);
        if (withControl)
            rec(b, id, L::DriverGroup::CONTROL, control, 2);
    };
    all(backup, 7, limits, motion, io, true);
    // Every supported group read, and still: the other groups, the complete persistent set, the factory values
    // and every other object are listed unknown, and the record is never complete.
    CHECK(backup.fieldCount == A::BACKUP_FIELDS && backup.groupsRead == A::BACKUP_GROUPS_SUPPORTED);
    CHECK(backup.groupsUnknown == (A::BACKUP_GROUPS_ALL & ~A::BACKUP_GROUPS_SUPPORTED) && backup.groupsUnknown);
    CHECK(!backup.complete && backup.unknown == A::BACKUP_UNKNOWN_ALL);
    CHECK(backup.fields[1].index == 0x2201 && backup.fields[1].write == A::BackupWrite::SUBDIVISION);
    CHECK(backup.fields[0].write == A::BackupWrite::NONE && backup.fields[3].write == A::BackupWrite::NONE); // 2200, 607D.
    // After a restore: 2200 changed (no typed write), 2202, 6083 and DI3 changed (typed), CONTROL not read again.
    A::PersistenceBackup after;
    const uint32_t driverAfter[] = {1, 1000}, limitsAfter[] = {10, 2, uint32_t(-1000), 1000},
                   motionAfter[] = {100, 250, 300}, ioAfter[] = {0x0f, 0x0101, 1, 2, 3, 0, 8, 9, 10};
    rec(after, 9, L::DriverGroup::DRIVER, driverAfter, 2);
    all(after, 9, limitsAfter, motionAfter, ioAfter, false);
    A::BackupComparison comparison;
    {
        const Saved<A::BackupComparison> saved(comparison);
        CHECK(!A::compareBackup(comparison, backup, backup)); // Two distinct records.
        saved.unchanged(comparison);
    }
    CHECK(A::compareBackup(comparison, backup, after));
    CHECK(comparison.fieldCount == A::BACKUP_FIELDS && comparison.changed == 1 && comparison.reapply == 3 &&
          comparison.notReread == 2 && comparison.unchanged == 14);
    CHECK(comparison.change[0] == A::BackupChange::CHANGED && comparison.change[2] == A::BackupChange::REAPPLY &&
          comparison.change[7] == A::BackupChange::REAPPLY && comparison.change[14] == A::BackupChange::REAPPLY &&
          comparison.change[18] == A::BackupChange::NOT_REREAD && comparison.change[19] == A::BackupChange::NOT_REREAD);
    CHECK(!comparison.complete && comparison.unknown == A::BACKUP_UNKNOWN_ALL &&
          comparison.groupsUnknown == backup.groupsUnknown);
    A::BackupReapply reapply;
    CHECK(A::prepareBackupReapply(reapply, backup, comparison));
    CHECK(reapply.writes == (1u << unsigned(A::BackupWrite::MOTION) | 1u << unsigned(A::BackupWrite::SOFT_LIMIT) |
                             1u << unsigned(A::BackupWrite::IO)));
    CHECK(reapply.motion.positionSpeedRpm == 100 && reapply.motion.rampUpTimeMs == 200 &&
          reapply.motion.rampDownTimeMs == 300 && reapply.softLimit.enable == L::SoftLimitEnable::LIMITS_OFF);
    CHECK(reapply.io.invertedInputs == 0x0001 && reapply.io.functions[3] == L::InputFunction::USER0 &&
          reapply.io.functions[0] == L::InputFunction::ORIGIN);
    CHECK(reapply.subdivision.subdivisionRaw == 0 && reapply.control.loopMode == L::LoopMode::UNKNOWN); // None.
    // A recorded undocumented DI code: no whole IO candidate exists, so a changed DI is CHANGED, never REAPPLY.
    const uint32_t undocumented[] = {0x0f, 0x0101, 1, 2, 3, 0x0105, 8, 9, 10},
                   changedDi0[] = {0x0f, 0x0101, 2, 2, 3, 0x0105, 8, 9, 10};
    A::PersistenceBackup odd, oddAfter;
    all(odd, 11, limits, motion, undocumented, false);
    all(oddAfter, 12, limits, motion, changedDi0, false);
    CHECK(A::compareBackup(comparison, odd, oddAfter) && comparison.changed == 1 && comparison.reapply == 0);
    CHECK(A::prepareBackupReapply(reapply, odd, comparison) && reapply.writes == 0);
    CHECK(!A::prepareBackupReapply(reapply, backup, comparison)); // Another backup's comparison.
}
int main() {
    captureValidation();
    preparationValidation();
    restoredAndPartial();
    backupRecordAndCompare();
    std::printf("configuration restore: %u checks passed\n", checks);
}
