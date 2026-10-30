// SPDX-License-Identifier: MIT
// Literal wire fixtures exercise the public settings path through the real SDO adapter.
#include "SdoClient.h"
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
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
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(std::memcmp(bytes, &value, sizeof value) == 0); }
};

L::ReadTarget target() {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = 17;
    t.channel.binding.endpoint.profile = 41;
    t.channel.binding.endpoint.address = 5;
    t.channel.binding.endpoint.bus = 2;
    t.channel.binding.bindingGeneration = 3;
    t.channel.binding.configurationGeneration = 7;
    t.channel.clientToServerId = 0x625;
    t.channel.serverToClientId = 0x5a5;
    t.nodeGeneration = 11;
    return t;
}
L::ReadOptions options() {
    L::ReadOptions o;
    o.queueTimeoutUs = o.responseTimeoutUs = 100;
    return o;
}
M::Frame reply(const C::SdoTransfer& work, uint32_t raw, bool abort = false) {
    M::Frame out;
    out.id = work.channel.serverToClientId;
    out.dlc = out.payloadLength = 8;
    out.data[0] = abort                                         ? 0x80
                  : work.direction == C::SdoDirection::DOWNLOAD ? 0x60
                  : work.size == 1                              ? 0x4f
                  : work.size == 2                              ? 0x4b
                                                                : 0x43;
    out.data[1] = uint8_t(work.index);
    out.data[2] = uint8_t(work.index >> 8);
    out.data[3] = work.subindex;
    if (abort || work.direction == C::SdoDirection::UPLOAD)
        for (unsigned i = 0; i < 4; ++i)
            out.data[i + 4] = uint8_t(raw >> (8 * i));
    return out;
}

// This application fixture issues the actual live reservation and excludes other
// work. It never reconstructs a token from transaction history after revocation.
struct Owner {
    C::SdoClient service;
    L::SettingsReservation reservation;
    M::Binding current = target().channel.binding;
    uint64_t now = 0;
    unsigned settingsDownloads = 0;
    unsigned settingsUploads = 0;
    bool exclusive = false;

    explicit Owner(M::Generation configuration = 7) {
        reservation.target = target();
        reservation.target.channel.binding.configurationGeneration = configuration;
        current = reservation.target.channel.binding;
        CHECK(service.configure(0, reservation.target.channel, true, true));
        C::NetworkRoute route;
        route.binding = current;
        route.generation = 11;
        route.heartbeatId = 0x705;
        route.emcyId = 0x85;
        CHECK(service.configureNetwork(0, route, C::NetworkObserver(), now));
        exclusive = true;
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = 3;
        reservation.sequence = 5;
        reservation.generation = 9;
        reservation.acquiredUs = now;
    }
    const L::SettingsReservation& live() const { return reservation; }
    void revoke() {
        exclusive = false;
        reservation.owner = 0;
    }
    bool competingWorkPermitted() const { return !exclusive; }
    bool admitCompeting(const C::SdoTransfer& work, C::SdoRequestId& id) {
        return !exclusive && service.admit(0, work, now, id);
    }
    C::SdoCompletion exchange(C::SdoTransfer work, uint32_t raw, bool abort = false,
                              bool transportFailure = false) {
        C::SdoRequestId id;
        CHECK(service.admit(0, work, now, id));
        CHECK(service.service(now));
        M::Frame tx;
        C::SdoTxId txId;
        CHECK(service.nextTx(0, now, tx, txId));
        CHECK(tx.id == 0x625 && tx.data[1] == uint8_t(work.index) &&
              tx.data[2] == uint8_t(work.index >> 8) && tx.data[3] == work.subindex);
        CHECK(service.canTransmit(txId, now));
        CHECK(service.confirmTx(txId, now));
        ++now;
        if (transportFailure)
            CHECK(service.transportFailure(id, now));
        else
            CHECK(service.receive(reply(work, raw, abort), now, now));
        const C::SdoCompletion* terminal = service.result(id);
        CHECK(terminal && terminal->outcome == (transportFailure ? C::SdoOutcome::TRANSPORT_ERROR
                                                : abort          ? C::SdoOutcome::SERVER_ABORT
                                                                 : C::SdoOutcome::SUCCESS));
        C::SdoCompletion result = *terminal;
        CHECK(service.release(id));
        return result;
    }
    L::DriverContext readGroup(L::DriverGroup group, const uint32_t* words, unsigned count) {
        L::DriverContext out;
        CHECK(L::prepareDriverRead(out, reservation.target, 21, now, 5000, options(), group));
        for (unsigned i = 0; i < count; ++i) {
            L::PreparedRead work;
            CHECK(L::nextDriver(out, now, work) && work.ready);
            const C::SdoCompletion terminal = exchange(work.transfer, words[i]);
            CHECK(L::advanceDriver(out, terminal, now));
        }
        CHECK(out.read.state == M::ReadState::SUCCEEDED);
        return out;
    }
    C::SdoCompletion control() {
        C::SdoTransfer work;
        work.channel = reservation.target.channel;
        work.operation = work.request = 31;
        work.step = 1;
        work.index = 0x6040;
        work.direction = C::SdoDirection::DOWNLOAD;
        work.size = 2;
        work.data[0] = 0x0f;
        work.deadlineUs = 5000;
        work.queueTimeoutUs = work.responseTimeoutUs = 100;
        return exchange(work, 0);
    }
    L::ReadContext state(uint32_t monitor = 0, uint32_t statusword = 7, uint32_t mode = 1) {
        L::ReadContext out;
        const uint8_t mask = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                     (1u << unsigned(L::StateBlock::STATUS)));
        CHECK(L::prepareState(out, reservation.target, 32, now, 5000, options(), mask));
        const uint32_t words[] = {monitor, 0, 0, statusword, mode};
        for (unsigned i = 0; i < 5; ++i) {
            L::PreparedRead work;
            CHECK(L::nextRead(out, now, work) && work.ready);
            const C::SdoCompletion terminal = exchange(work.transfer, words[i]);
            CHECK(L::advanceRead(out, terminal, now));
        }
        return out;
    }
    L::ReadTarget currentTarget() const {
        L::ReadTarget out = reservation.target;
        out.channel.binding = current;
        return out;
    }
};

const uint32_t oldMotion[] = {100, 200, 300};
// Unknown high polarity bits belong to the observed word and must survive writes.
const uint32_t oldIo[] = {16, 0xa500, 0, 1, 2, 3, 4, 5, 6};
L::DriverSettings motionCandidate() {
    L::DriverSettings out;
    out.positionSpeedRpm = 101;
    out.rampUpTimeMs = 201;
    out.rampDownTimeMs = 301;
    return out;
}
L::IoSettings ioCandidate(unsigned code = 13) {
    L::IoSettings out;
    out.invertedInputs = 0x55;
    for (unsigned i = 0; i < 7; ++i)
        out.functions[i] = static_cast<L::InputFunction>(code);
    return out;
}
struct Fixture {
    Owner owner;
    C::SdoCompletion control;
    L::ReadContext state;
    L::DriverContext prior;
    bool io;
    explicit Fixture(bool input, M::Generation configuration = 7)
        : owner(configuration), io(input) {
        if (!io) {
            control = owner.control();
            state = owner.state();
        }
        prior = owner.readGroup(io ? L::DriverGroup::IO_INPUT_CONFIGURATION
                                   : L::DriverGroup::MOTION_PARAMETERS,
                                io ? oldIo : oldMotion, io ? 9 : 3);
    }
    M::Status prepare(L::DriverSettingsContext& out, uint64_t deadline = 1000) {
        return io ? L::prepareIoSettings(out, owner.live(), prior, ioCandidate(), 41, owner.now,
                                         deadline, options(), 100)
                  : L::prepareDriverSettings(out, owner.live(), prior, state, control,
                                             motionCandidate(), 41, owner.now, deadline, options(),
                                             100);
    }
};

void preparation() {
    {
        Fixture f(true);
        L::DriverSettingsContext output;
        const Saved<L::DriverSettingsContext> saved(output);
        L::IoSettings unset;
        CHECK(!L::prepareIoSettings(output, f.owner.live(), f.prior, unset, 41, f.owner.now, 1000,
                                    options(), 100));
        saved.unchanged(output);
        unset.invertedInputs = 0;
        CHECK(!L::prepareIoSettings(output, f.owner.live(), f.prior, unset, 41, f.owner.now, 1000,
                                    options(), 100));
        saved.unchanged(output);
        CHECK(f.owner.settingsDownloads == 0);
    }
    for (bool io : {false, true}) {
        Fixture f(io);
        L::DriverSettingsContext out;
        out.operation = 0xbeef;
        const Saved<L::DriverSettingsContext> initial(out);
        for (unsigned bad = 0; bad < 12; ++bad) {
            L::SettingsReservation live = f.owner.live();
            L::DriverContext prior = f.prior;
            L::DriverSettings motion = motionCandidate();
            L::IoSettings inputs = ioCandidate();
            uint32_t operation = 41;
            uint64_t deadline = 1000, age = 100;
            L::ReadOptions policy = options();
            switch (bad) {
            case 0:
                live.model = L::SettingsModel::UNKNOWN;
                break;
            case 1:
                live.owner = 0;
                break;
            case 2:
                live.sequence = 0;
                break;
            case 3:
                live.generation = 0;
                break;
            case 4:
                ++live.target.nodeGeneration;
                break;
            case 5:
                live.acquiredUs = f.owner.now + 1;
                break;
            case 6:
                operation = 0;
                break;
            case 7:
                deadline = f.owner.now;
                break;
            case 8:
                age = 0;
                break;
            case 9:
                policy.queueTimeoutUs = 0;
                break;
            case 10:
                policy.responseTimeoutUs = 0;
                break;
            default:
                ++prior.read.target.channel.binding.configurationGeneration;
                break;
            }
            CHECK(!(io ? L::prepareIoSettings(out, live, prior, inputs, operation, f.owner.now,
                                              deadline, policy, age)
                       : L::prepareDriverSettings(out, live, prior, f.state, f.control, motion,
                                                  operation, f.owner.now, deadline, policy, age)));
            initial.unchanged(out);
        }
        CHECK(!f.owner.competingWorkPermitted());
        CHECK(f.owner.settingsDownloads == 0);
        CHECK(f.prepare(out));
        CHECK(out.state == L::SettingsState::WRITING && out.confirmed == 0 &&
              !out.effectsPossible && !out.uncertain);
        CHECK(out.changed == (io ? 0x1fe : 7));
        CHECK(out.requested[io ? 1 : 0] == (io ? 0xa555u : 101u));
        const Saved<L::DriverSettingsContext> admitted(out);
        L::PreparedRead work;
        CHECK(L::nextDriver(out, f.owner.live(), f.owner.current, f.owner.now, work));
        CHECK(work.ready && work.transfer.direction == C::SdoDirection::DOWNLOAD &&
              work.transfer.index == (io ? 0x2400 : 0x6081));
        C::SdoRequestId blocked;
        const Saved<C::SdoRequestId> oldId(blocked);
        CHECK(!f.owner.admitCompeting(work.transfer, blocked));
        oldId.unchanged(blocked);
        admitted.unchanged(out);
        L::DriverObservation driver;
        L::IoObservation inputs;
        const Saved<L::DriverObservation> oldDriver(driver);
        const Saved<L::IoObservation> oldInputs(inputs);
        CHECK(!L::getDriver(out, f.owner.live(), target(), f.owner.now, 100, driver));
        CHECK(!L::getIo(out, f.owner.live(), target(), f.owner.now, 100, inputs));
        oldDriver.unchanged(driver);
        oldInputs.unchanged(inputs);
    }
    Fixture f(false);
    for (unsigned field = 0; field < 3; ++field)
        for (uint32_t value : {field == 0 ? 3001u : 2001u, UINT32_MAX}) {
            L::DriverSettings candidate = motionCandidate();
            if (field == 0)
                candidate.positionSpeedRpm = value;
            if (field == 1)
                candidate.rampUpTimeMs = value;
            if (field == 2)
                candidate.rampDownTimeMs = value;
            L::DriverSettingsContext out;
            const Saved<L::DriverSettingsContext> before(out);
            CHECK(!L::prepareDriverSettings(out, f.owner.live(), f.prior, f.state, f.control,
                                            candidate, 41, f.owner.now, 1000, options(), 100));
            before.unchanged(out);
        }
    for (unsigned initialized = 0; initialized < 3; ++initialized) {
        L::DriverSettings unset;
        if (initialized >= 1)
            unset.positionSpeedRpm = 0;
        if (initialized >= 2)
            unset.rampUpTimeMs = 0;
        L::DriverSettingsContext out;
        const Saved<L::DriverSettingsContext> before(out);
        CHECK(!L::prepareDriverSettings(out, f.owner.live(), f.prior, f.state, f.control, unset, 41,
                                        f.owner.now, 1000, options(), 100));
        before.unchanged(out);
    }
    for (uint32_t value : {1u, 2000u}) {
        L::DriverSettings candidate;
        candidate.positionSpeedRpm = value;
        candidate.rampUpTimeMs = candidate.rampDownTimeMs = value;
        L::DriverSettingsContext out;
        CHECK(L::prepareDriverSettings(out, f.owner.live(), f.prior, f.state, f.control, candidate,
                                       41, f.owner.now, 1000, options(), 100));
    }
    L::DriverSettings maximum = motionCandidate();
    maximum.positionSpeedRpm = 3000;
    L::DriverSettingsContext out;
    CHECK(L::prepareDriverSettings(out, f.owner.live(), f.prior, f.state, f.control, maximum, 41,
                                   f.owner.now, 1000, options(), 100));
    for (unsigned field = 0; field < 3; ++field) {
        C::SdoCompletion bad = f.control;
        if (field == 0)
            bad.transfer.data[0] = 7;
        if (field == 1)
            bad.transfer.index = 0x6060;
        if (field == 2)
            bad.protocolAcknowledged = false;
        const Saved<L::DriverSettingsContext> before(out);
        CHECK(!L::prepareDriverSettings(out, f.owner.live(), f.prior, f.state, bad,
                                        motionCandidate(), 41, f.owner.now, 1000, options(), 100));
        before.unchanged(out);
    }
    Fixture inputs(true);
    for (uint16_t mask : {uint16_t(0), uint16_t(1), uint16_t(0x40), uint16_t(0x7f)}) {
        L::IoSettings candidate = ioCandidate();
        candidate.invertedInputs = mask;
        CHECK(L::prepareIoSettings(out, inputs.owner.live(), inputs.prior, candidate, 41,
                                   inputs.owner.now, 1000, options(), 100));
        CHECK(out.requested[1] == uint16_t(0xa500 | mask));
    }
    for (unsigned code = 0; code <= 13; ++code) {
        // Repeated functions are accepted; uniqueness is not a documented constraint.
        CHECK(L::prepareIoSettings(out, inputs.owner.live(), inputs.prior, ioCandidate(code), 41,
                                   inputs.owner.now, 1000, options(), 100));
        CHECK(out.requested[1] == 0xa555);
    }
    for (unsigned bad = 0; bad < 3; ++bad) {
        L::IoSettings candidate = ioCandidate();
        if (bad == 0)
            candidate.invertedInputs = 0x80;
        if (bad == 1)
            candidate.functions[6] = static_cast<L::InputFunction>(14);
        if (bad == 2)
            candidate.functions[0] = L::InputFunction::UNKNOWN;
        const Saved<L::DriverSettingsContext> before(out);
        CHECK(!L::prepareIoSettings(out, inputs.owner.live(), inputs.prior, candidate, 41,
                                    inputs.owner.now, 1000, options(), 100));
        before.unchanged(out);
    }
    for (unsigned bad = 0; bad < 3; ++bad) {
        const L::ReadContext state =
            f.owner.state(bad == 0 ? 0xffff : 0, bad == 1 ? 6 : 7, bad == 2 ? 3 : 1);
        const Saved<L::DriverSettingsContext> before(out);
        CHECK(!L::prepareDriverSettings(out, f.owner.live(), f.prior, state, f.control,
                                        motionCandidate(), 41, f.owner.now, 1000, options(), 100));
        before.unchanged(out);
    }
    for (bool io : {false, true}) {
        Fixture exhausted(io, UINT32_MAX);
        const Saved<L::DriverSettingsContext> before(out);
        CHECK(!exhausted.prepare(out));
        before.unchanged(out);
        CHECK(exhausted.owner.settingsDownloads == 0);
    }
}

// All settings downloads are literal reviewed rows. A returned frame for a
// controlword, mode request, save/reset or output action fails these assertions.
void checkWrite(const L::PreparedRead& work, bool io, unsigned field, uint32_t expected) {
    CHECK(work.ready && work.transfer.direction == C::SdoDirection::DOWNLOAD);
    CHECK(work.transfer.operation == 41 && work.transfer.request == 41 &&
          work.transfer.step == field + 1);
    const uint16_t motionIndices[] = {0x6081, 0x6083, 0x6084};
    CHECK(work.transfer.index == (io ? 0x2400 : motionIndices[field]));
    CHECK(work.transfer.subindex == (io ? field : 0));
    CHECK(work.transfer.size == (io ? 2 : 4));
    for (unsigned i = 0; i < work.transfer.size; ++i)
        CHECK(work.transfer.data[i] == uint8_t(expected >> (i * 8)));
    CHECK(work.transfer.channel.binding.configurationGeneration == 7);
}
enum class Failure { NONE, ABORT, TRANSPORT, QUEUED_CANCEL };
C::SdoCompletion write(Fixture& f, L::DriverSettingsContext& c, unsigned field,
                       Failure failure = Failure::NONE, bool consumeAtDeadline = false) {
    Owner& owner = f.owner;
    L::PreparedRead work;
    CHECK(L::nextDriver(c, owner.live(), owner.current, owner.now, work));
    checkWrite(work, f.io, field, c.requested[field]);
    C::SdoRequestId id;
    CHECK(owner.service.admit(0, work.transfer, owner.now, id));
    CHECK(owner.service.service(owner.now));
    M::Binding future = target().channel.binding;
    ++future.configurationGeneration;
    CHECK(!owner.service.advanceConfiguration(0, target().channel.binding, future, owner.now));
    const Saved<L::DriverSettingsContext> admitted(c);
    if (failure == Failure::QUEUED_CANCEL) {
        C::SdoRequestId rejected;
        CHECK(!owner.service.admit(0, work.transfer, owner.now, rejected));
        admitted.unchanged(c);
        CHECK(owner.service.cancel(id, owner.now));
    } else {
        const M::Generation before = owner.current.configurationGeneration;
        CHECK(L::noteDriverTransmission(c, owner.live(), owner.current, owner.now));
        CHECK(owner.current.configurationGeneration == (c.confirmed == 0 ? before + 1 : before));
        CHECK(c.effectsPossible && c.transmissionNoted);
        const Saved<L::DriverSettingsContext> noted(c);
        const Saved<M::Binding> binding(owner.current);
        CHECK(!L::noteDriverTransmission(c, owner.live(), owner.current, owner.now));
        noted.unchanged(c);
        binding.unchanged(owner.current);
        M::Frame tx;
        C::SdoTxId txId;
        CHECK(owner.service.nextTx(0, owner.now, tx, txId));
        ++owner.settingsDownloads;
        CHECK(tx.id == 0x625 && tx.data[1] == uint8_t(work.transfer.index) &&
              tx.data[2] == uint8_t(work.transfer.index >> 8) &&
              tx.data[3] == work.transfer.subindex);
        CHECK(tx.data[0] == (f.io ? 0x2b : 0x23));
        CHECK(owner.service.canTransmit(txId, owner.now));
        CHECK(owner.service.confirmTx(txId, owner.now));
        ++owner.now;
        if (failure == Failure::TRANSPORT)
            CHECK(owner.service.transportFailure(id, owner.now));
        else
            CHECK(owner.service.receive(reply(work.transfer,
                                              failure == Failure::ABORT ? 0x80000001u : 0,
                                              failure == Failure::ABORT),
                                        owner.now, owner.now));
    }
    const C::SdoCompletion* terminal = owner.service.result(id);
    CHECK(terminal);
    C::SdoCompletion result = *terminal;
    if (consumeAtDeadline)
        owner.now = c.deadlineUs;
    CHECK(L::advanceDriver(c, owner.live(), owner.current, result, owner.now));
    CHECK(!owner.service.advanceConfiguration(0, target().channel.binding, future, owner.now));
    CHECK(owner.service.release(id));
    CHECK(c.evidencePresent && c.evidence.transfer.step == field + 1 &&
          c.evidence.outcome == result.outcome && c.evidence.abortCode == result.abortCode);
    return result;
}
void readback(Fixture& f, L::DriverSettingsContext& c, int mismatch = -1) {
    Owner& owner = f.owner;
    CHECK(c.state == L::SettingsState::READING_BACK);
    CHECK(
        owner.service.advanceConfiguration(0, target().channel.binding, owner.current, owner.now));
    CHECK(owner.current.configurationGeneration == 8);
    const unsigned count = f.io ? 9 : 3;
    for (unsigned field = 0; field < count; ++field) {
        L::PreparedRead work;
        CHECK(L::nextDriver(c, owner.live(), owner.current, owner.now, work) && work.ready);
        CHECK(work.transfer.direction == C::SdoDirection::UPLOAD &&
              work.transfer.step == field + 10 &&
              work.transfer.channel.binding.configurationGeneration == 8);
        const uint16_t motionIndices[] = {0x6081, 0x6083, 0x6084};
        CHECK(work.transfer.index == (f.io ? 0x2400 : motionIndices[field]) &&
              work.transfer.subindex == (f.io ? field : 0));
        ++owner.settingsUploads;
        const C::SdoCompletion terminal =
            owner.exchange(work.transfer, c.requested[field] ^ (int(field) == mismatch ? 1u : 0u));
        CHECK(L::advanceDriver(c, owner.live(), owner.current, terminal, owner.now));
        if (c.state == L::SettingsState::FAILED)
            break;
    }
}
void successfulTransactions() {
    for (bool io : {false, true}) {
        Fixture f(io);
        L::DriverSettingsContext c;
        CHECK(f.prepare(c));
        const uint16_t changed = c.changed;
        const unsigned first = io ? 1 : 0, count = io ? 8 : 3;
        for (unsigned n = 0; n < count; ++n) {
            const unsigned field = first + n;
            const C::SdoCompletion terminal = write(f, c, field);
            CHECK(terminal.outcome == C::SdoOutcome::SUCCESS);
            CHECK(c.confirmed == uint16_t(((1u << (field + 1)) - 1u) & changed));
            CHECK(c.acknowledgedUs[field] == f.owner.now && !c.uncertain);
            CHECK(f.owner.current.configurationGeneration == 8);
        }
        CHECK(c.confirmed == changed && c.effectsPossible);
        L::DriverObservation obsolete;
        const Saved<L::DriverObservation> before(obsolete);
        if (!io) {
            CHECK(!L::getDriver(f.prior, f.owner.currentTarget(), f.owner.now, 100, obsolete));
            before.unchanged(obsolete);
        }
        readback(f, c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && c.outcome == M::ReadOutcome::SUCCESS &&
              !c.uncertain && c.confirmed == changed);
        CHECK(f.owner.settingsDownloads == count && f.owner.settingsUploads == (io ? 9u : 3u));
        L::DriverObservation driver;
        L::IoObservation inputs;
        const Saved<L::DriverObservation> savedDriver(driver);
        const Saved<L::IoObservation> savedInputs(inputs);
        if (io) {
            CHECK(L::getIo(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, inputs));
            CHECK(inputs.inputPolarityRaw == 0xa555 && inputs.ioEntries == 16);
            for (unsigned i = 0; i < 7; ++i)
                CHECK(inputs.inputFunctionRaw[i] == 13);
            CHECK(!L::getDriver(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100,
                                driver));
            savedDriver.unchanged(driver);
        } else {
            CHECK(
                L::getDriver(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, driver));
            CHECK(driver.positionSpeedRpm == 101 && driver.rampUpTimeMs == 201 &&
                  driver.rampDownTimeMs == 301);
            CHECK(!L::getIo(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, inputs));
            savedInputs.unchanged(inputs);
        }
        const Saved<L::DriverObservation> observedDriver(driver);
        const Saved<L::IoObservation> observedInputs(inputs);
        CHECK(!L::getDriver(c, f.owner.live(), target(), f.owner.now, 100, driver));
        CHECK(!L::getIo(c, f.owner.live(), target(), f.owner.now, 100, inputs));
        observedDriver.unchanged(driver);
        observedInputs.unchanged(inputs);
        CHECK(!L::getDriver(c, f.owner.live(), f.owner.currentTarget(), f.owner.now + 1000, 1,
                            driver));
        CHECK(!L::getIo(c, f.owner.live(), f.owner.currentTarget(), f.owner.now + 1000, 1, inputs));
        observedDriver.unchanged(driver);
        observedInputs.unchanged(inputs);
        L::PreparedRead done;
        done.ready = true;
        done.transfer.index = 0xbeef;
        CHECK(L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, done) && !done.ready &&
              done.transfer.index == 0xbeef);
        f.owner.revoke();
        CHECK(f.owner.competingWorkPermitted());
        CHECK(!L::getDriver(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, driver));
        CHECK(!L::getIo(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, inputs));
        observedDriver.unchanged(driver);
        observedInputs.unchanged(inputs);
        C::SdoTransfer competing;
        competing.channel = f.owner.currentTarget().channel;
        competing.operation = competing.request = 99;
        competing.step = 1;
        competing.index = 0x1001;
        competing.size = 1;
        competing.deadlineUs = 1000;
        competing.queueTimeoutUs = competing.responseTimeoutUs = 100;
        C::SdoRequestId admitted;
        CHECK(f.owner.admitCompeting(competing, admitted));
        CHECK(f.owner.service.cancel(admitted, f.owner.now));
        CHECK(f.owner.service.release(admitted));
    }
}
void unchangedAndSparseCandidates() {
    for (bool io : {false, true}) {
        Fixture f(io);
        L::DriverSettings motion;
        motion.positionSpeedRpm = oldMotion[0];
        motion.rampUpTimeMs = oldMotion[1];
        motion.rampDownTimeMs = oldMotion[2];
        L::IoSettings inputs;
        inputs.invertedInputs = 0;
        for (unsigned i = 0; i < 7; ++i)
            inputs.functions[i] = static_cast<L::InputFunction>(oldIo[i + 2]);
        L::DriverSettingsContext c;
        CHECK(io ? L::prepareIoSettings(c, f.owner.live(), f.prior, inputs, 41, f.owner.now, 1000,
                                        options(), 100)
                 : L::prepareDriverSettings(c, f.owner.live(), f.prior, f.state, f.control, motion,
                                            41, f.owner.now, 1000, options(), 100));
        CHECK(c.state == L::SettingsState::SUCCEEDED && !c.changed && !c.confirmed &&
              !c.effectsPossible && !c.uncertain);
        L::PreparedRead work;
        work.ready = true;
        CHECK(L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, work) && !work.ready);
        CHECK(f.owner.current.configurationGeneration == 7 && !f.owner.settingsDownloads &&
              !f.owner.settingsUploads);
        L::DriverObservation driver;
        L::IoObservation observed;
        CHECK(io ? L::getIo(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, observed)
                 : L::getDriver(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100,
                                driver));

        // A changed final field must not rewrite any unchanged earlier field.
        const unsigned field = io ? 8 : 2;
        if (io)
            inputs.functions[6] = L::InputFunction::USER6;
        else
            motion.rampDownTimeMs = 400;
        CHECK(io ? L::prepareIoSettings(c, f.owner.live(), f.prior, inputs, 41, f.owner.now, 1000,
                                        options(), 100)
                 : L::prepareDriverSettings(c, f.owner.live(), f.prior, f.state, f.control, motion,
                                            41, f.owner.now, 1000, options(), 100));
        CHECK(c.changed == (1u << field));
        const uint32_t retained = c.requested[field];
        inputs.functions[6] = L::InputFunction::UNDEFINED;
        motion.rampDownTimeMs = 999;
        f.prior.read.values[field].raw = 0xffff;
        CHECK(c.requested[field] == retained);
        write(f, c, field);
        readback(f, c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && f.owner.settingsDownloads == 1);
    }
}
void zeroParameters() {
    // Zero is a documented native register word. These transactions establish
    // storage/readback only, with the same retained pre-trigger prerequisites.
    for (unsigned selected = 0; selected < 4; ++selected) {
        Fixture f(false);
        L::DriverSettings requested;
        requested.positionSpeedRpm = selected == 0 || selected == 3 ? 0 : oldMotion[0];
        requested.rampUpTimeMs = selected == 1 || selected == 3 ? 0 : oldMotion[1];
        requested.rampDownTimeMs = selected == 2 || selected == 3 ? 0 : oldMotion[2];
        L::DriverSettingsContext c;
        CHECK(L::prepareDriverSettings(c, f.owner.live(), f.prior, f.state, f.control, requested,
                                       41, f.owner.now, 1000, options(), 100));
        CHECK(c.changed == (selected == 3 ? 7u : 1u << selected));
        for (unsigned field = 0; field < 3; ++field)
            if (c.changed & (1u << field)) {
                CHECK(c.requested[field] == 0);
                const C::SdoCompletion terminal = write(f, c, field);
                CHECK(terminal.outcome == C::SdoOutcome::SUCCESS && terminal.protocolAcknowledged);
            }
        readback(f, c);
        CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == c.changed && !c.uncertain);
        L::DriverObservation observed;
        CHECK(L::getDriver(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, observed));
        CHECK(observed.positionSpeedRpm == requested.positionSpeedRpm &&
              observed.rampUpTimeMs == requested.rampUpTimeMs &&
              observed.rampDownTimeMs == requested.rampDownTimeMs);
        CHECK(f.owner.settingsDownloads == (selected == 3 ? 3u : 1u) &&
              f.owner.settingsUploads == 3);
    }

    Fixture f(false);
    const uint32_t zeroWords[] = {0, 0, 0};
    f.prior = f.owner.readGroup(L::DriverGroup::MOTION_PARAMETERS, zeroWords, 3);
    L::DriverSettings requested;
    requested.positionSpeedRpm = requested.rampUpTimeMs = requested.rampDownTimeMs = 0;
    L::DriverSettingsContext c;
    CHECK(L::prepareDriverSettings(c, f.owner.live(), f.prior, f.state, f.control, requested, 41,
                                   f.owner.now, 1000, options(), 100));
    CHECK(c.state == L::SettingsState::SUCCEEDED && !c.changed && !c.confirmed &&
          !c.effectsPossible && !c.uncertain);
    L::PreparedRead work;
    work.ready = true;
    CHECK(L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, work) && !work.ready);
    CHECK(f.owner.current.configurationGeneration == 7 && !f.owner.settingsDownloads &&
          !f.owner.settingsUploads);
    L::DriverObservation observed;
    CHECK(L::getDriver(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, observed));
    CHECK(observed.positionSpeedRpm == 0 && observed.rampUpTimeMs == 0 &&
          observed.rampDownTimeMs == 0);
    CHECK(observed.fields[0].earliestUs == f.prior.read.values[0].provenance.earliestUs);
}
void mismatchAndInvalidation() {
    for (bool io : {false, true}) {
        const unsigned first = io ? 1 : 0, count = io ? 8 : 3;
        for (unsigned mismatch = first; mismatch < (io ? 9u : 3u); ++mismatch) {
            Fixture f(io);
            L::DriverSettingsContext c;
            CHECK(f.prepare(c));
            for (unsigned i = 0; i < count; ++i)
                write(f, c, first + i);
            const uint16_t confirmed = c.confirmed;
            readback(f, c, int(mismatch));
            CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR &&
                  c.confirmed == confirmed && c.effectsPossible && c.evidencePresent);
            L::DriverObservation driver;
            L::IoObservation inputs;
            const Saved<L::DriverObservation> previousDriver(driver);
            const Saved<L::IoObservation> previousInputs(inputs);
            CHECK(!L::getDriver(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100,
                                driver));
            CHECK(!L::getIo(c, f.owner.live(), f.owner.currentTarget(), f.owner.now, 100, inputs));
            previousDriver.unchanged(driver);
            previousInputs.unchanged(inputs);
        }
        Fixture f(io);
        L::DriverSettingsContext c;
        CHECK(f.prepare(c));
        // Host prepared motion uses the actual binding which the TX notice withdraws.
        M::AxisConfig axis;
        axis.binding = f.owner.current;
        M::PositionRequest request;
        request.configurationGeneration = axis.binding.configurationGeneration;
        request.value = M::Rational(10);
        M::PreparedTarget prepared;
        CHECK(M::preparePosition(request, axis, nullptr, prepared));
        CHECK(M::validatePreparedTarget(prepared, axis, nullptr, f.owner.now));
        write(f, c, first);
        axis.binding = f.owner.current;
        CHECK(!M::validatePreparedTarget(prepared, axis, nullptr, f.owner.now));
        const Saved<L::DriverSettingsContext> retained(c);
        f.owner.revoke();
        L::PreparedRead work;
        const Saved<L::PreparedRead> old(work);
        CHECK(!L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, work));
        CHECK(!L::noteDriverTransmission(c, f.owner.live(), f.owner.current, f.owner.now));
        retained.unchanged(c);
        old.unchanged(work);
        CHECK(L::advanceDriver(c, M::ReadOutcome::INVALIDATED, f.owner.now));
        CHECK(c.state == L::SettingsState::FAILED && c.effectsPossible &&
              c.confirmed == (1u << first));
    }
}
void correlationAndLocalDeadlines() {
    for (bool io : {false, true}) {
        Fixture f(io);
        L::DriverSettingsContext c;
        CHECK(f.prepare(c));
        L::PreparedRead work;
        CHECK(L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, work));
        C::SdoCompletion good;
        good.transfer = work.transfer;
        good.nodeGeneration = 11;
        good.admittedUs = good.completedUs = f.owner.now;
        good.outcome = C::SdoOutcome::SUCCESS;
        good.queued = good.transmissionPossible = good.protocolAcknowledged = good.effectPossible =
            true;
        // Fabricated possible-TX success cannot bypass the actual withdrawal boundary.
        const Saved<L::DriverSettingsContext> original(c);
        CHECK(!L::advanceDriver(c, f.owner.live(), f.owner.current, good, f.owner.now));
        original.unchanged(c);
        CHECK(L::noteDriverTransmission(c, f.owner.live(), f.owner.current, f.owner.now));
        for (unsigned bad = 0; bad < 10; ++bad) {
            C::SdoCompletion terminal = good;
            if (bad == 0)
                ++terminal.transfer.step;
            if (bad == 1)
                ++terminal.transfer.request;
            if (bad == 2)
                ++terminal.transfer.operation;
            if (bad == 3)
                ++terminal.transfer.index;
            if (bad == 4)
                ++terminal.nodeGeneration;
            if (bad == 5)
                ++terminal.transfer.data[0];
            if (bad == 6)
                terminal.effectPossible = false;
            if (bad == 7)
                terminal.effectUncertain = true;
            if (bad == 8)
                terminal.protocolAcknowledged = false;
            if (bad == 9)
                terminal.completedUs = f.owner.now + 1;
            const Saved<L::DriverSettingsContext> before(c);
            CHECK(!L::advanceDriver(c, f.owner.live(), f.owner.current, terminal, f.owner.now));
            before.unchanged(c);
        }
        CHECK(L::advanceDriver(c, M::ReadOutcome::DEADLINE, 1000));
        CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::DEADLINE &&
              c.effectsPossible && c.uncertain && !c.confirmed);

        Fixture waiting(io);
        L::DriverSettingsContext pending;
        CHECK(waiting.prepare(pending));
        const Saved<L::DriverSettingsContext> prior(pending);
        const Saved<L::PreparedRead> old(work);
        CHECK(!L::nextDriver(pending, waiting.owner.live(), waiting.owner.current, 1000, work));
        CHECK(
            !L::noteDriverTransmission(pending, waiting.owner.live(), waiting.owner.current, 1000));
        prior.unchanged(pending);
        old.unchanged(work);
        CHECK(L::advanceDriver(pending, M::ReadOutcome::CANCELLED, 1000));
        CHECK(pending.outcome == M::ReadOutcome::DEADLINE && !pending.effectsPossible &&
              !pending.uncertain);

        Fixture late(io);
        L::DriverSettingsContext delayed;
        CHECK(late.prepare(delayed));
        late.owner.now = 998;
        const unsigned field = io ? 1 : 0;
        const C::SdoCompletion acknowledged = write(late, delayed, field, Failure::NONE, true);
        CHECK(acknowledged.outcome == C::SdoOutcome::SUCCESS && acknowledged.completedUs == 999);
        CHECK(delayed.state == L::SettingsState::FAILED &&
              delayed.outcome == M::ReadOutcome::DEADLINE && delayed.confirmed == (1u << field) &&
              delayed.acknowledgedUs[field] == 999 && delayed.evidence.protocolAcknowledged &&
              !delayed.uncertain);
    }
}
void failedReadback() {
    for (bool io : {false, true})
        for (bool transport : {false, true}) {
            const unsigned first = io ? 1 : 0, writes = io ? 8 : 3, reads = io ? 9 : 3;
            for (unsigned failed = 0; failed < reads; ++failed) {
                Fixture f(io);
                L::DriverSettingsContext c;
                CHECK(f.prepare(c));
                for (unsigned i = 0; i < writes; ++i)
                    write(f, c, first + i);
                CHECK(f.owner.service.advanceConfiguration(0, target().channel.binding,
                                                           f.owner.current, f.owner.now));
                const uint16_t confirmed = c.confirmed;
                for (unsigned i = 0; i <= failed; ++i) {
                    L::PreparedRead work;
                    CHECK(L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, work));
                    CHECK(work.ready && work.transfer.step == i + 10);
                    const bool failing = i == failed;
                    const C::SdoCompletion terminal =
                        f.owner.exchange(work.transfer, failing ? 0x80000002u : c.requested[i],
                                         failing && !transport, failing && transport);
                    CHECK(L::advanceDriver(c, f.owner.live(), f.owner.current, terminal,
                                           f.owner.now));
                }
                CHECK(c.state == L::SettingsState::FAILED && c.uncertain && c.effectsPossible &&
                      c.confirmed == confirmed && c.evidencePresent &&
                      c.evidence.transfer.step == failed + 10 &&
                      c.readback.read.evidence.transfer.step == failed + 10);
                if (!transport)
                    CHECK(c.evidence.abortValid && c.evidence.abortCode == 0x80000002u &&
                          c.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
                for (unsigned i = 0; i < failed; ++i)
                    CHECK(c.readback.read.values[i].valid &&
                          c.readback.read.values[i].raw == c.requested[i]);
            }
        }
}
void localReadbackWithdrawal() {
    for (bool io : {false, true})
        for (M::ReadOutcome reason :
             {M::ReadOutcome::CANCELLED, M::ReadOutcome::INVALIDATED, M::ReadOutcome::DEADLINE})
            for (unsigned boundary = 0; boundary < 3; ++boundary) {
                Fixture f(io);
                L::DriverSettingsContext c;
                CHECK(f.prepare(c));
                const unsigned first = io ? 1 : 0, count = io ? 8 : 3;
                for (unsigned i = 0; i < count; ++i)
                    write(f, c, first + i);
                CHECK(f.owner.service.advanceConfiguration(0, target().channel.binding,
                                                           f.owner.current, f.owner.now));
                CHECK(c.state == L::SettingsState::READING_BACK);
                if (boundary == 1) {
                    L::PreparedRead work;
                    CHECK(L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, work));
                    const C::SdoCompletion terminal =
                        f.owner.exchange(work.transfer, c.requested[0]);
                    CHECK(L::advanceDriver(c, f.owner.live(), f.owner.current, terminal,
                                           f.owner.now));
                    CHECK(c.readback.read.values[0].valid);
                }
                if (boundary == 2) {
                    L::PreparedRead work;
                    CHECK(L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, work));
                    C::SdoRequestId id;
                    CHECK(f.owner.service.admit(0, work.transfer, f.owner.now, id));
                    CHECK(f.owner.service.service(f.owner.now));
                    M::Frame frame;
                    C::SdoTxId txId;
                    CHECK(f.owner.service.nextTx(0, f.owner.now, frame, txId));
                    CHECK(frame.data[0] == 0x40);
                    CHECK(f.owner.service.confirmTx(txId, f.owner.now));
                    ++f.owner.now;
                    CHECK(f.owner.service.cancel(id, f.owner.now));
                    const C::SdoCompletion* terminal = f.owner.service.result(id);
                    CHECK(terminal && terminal->effectUncertain && terminal->transmissionPossible);
                    // The owner retires work; local withdrawal has no copied terminal
                    // proving the upload harmless, so unknown read effects remain.
                    CHECK(f.owner.service.release(id));
                }
                const uint16_t confirmed = c.confirmed;
                const uint64_t now = reason == M::ReadOutcome::DEADLINE ? 1000 : f.owner.now;
                CHECK(L::advanceDriver(c, reason, now));
                CHECK(c.state == L::SettingsState::FAILED && c.outcome == reason &&
                      c.effectsPossible && c.uncertain && c.confirmed == confirmed);
                CHECK(c.readback.read.state == M::ReadState::FAILED && c.readback.read.uncertain);
                if (boundary == 1)
                    CHECK(c.readback.read.values[0].valid);
            }
}
void partialFailure() {
    for (bool io : {false, true})
        for (Failure failure : {Failure::ABORT, Failure::TRANSPORT}) {
            const unsigned first = io ? 1 : 0, count = io ? 8 : 3;
            for (unsigned position = 0; position < count; ++position) {
                Fixture f(io);
                L::DriverSettingsContext c;
                CHECK(f.prepare(c));
                for (unsigned n = 0; n < position; ++n)
                    write(f, c, first + n);
                const uint16_t confirmed = c.confirmed;
                const C::SdoCompletion terminal = write(f, c, first + position, failure);
                CHECK(c.state == L::SettingsState::FAILED && c.confirmed == confirmed &&
                      c.effectsPossible && c.uncertain && c.evidencePresent);
                CHECK(terminal.effectUncertain && c.evidence.effectUncertain);
                if (failure == Failure::ABORT)
                    CHECK(c.evidence.abortValid && c.evidence.abortCode == 0x80000001u &&
                          c.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
                L::PreparedRead terminalWork;
                terminalWork.ready = true;
                CHECK(
                    L::nextDriver(c, f.owner.live(), f.owner.current, f.owner.now, terminalWork) &&
                    !terminalWork.ready);
                const Saved<L::DriverSettingsContext> retained(c);
                CHECK(!L::advanceDriver(c, f.owner.live(), f.owner.current, terminal, f.owner.now));
                retained.unchanged(c);
            }
        }
    for (bool io : {false, true}) {
        Fixture f(io);
        L::DriverSettingsContext c;
        CHECK(f.prepare(c));
        const C::SdoCompletion terminal = write(f, c, io ? 1 : 0, Failure::QUEUED_CANCEL);
        CHECK(!terminal.transmissionPossible && !terminal.effectUncertain);
        CHECK(c.state == L::SettingsState::FAILED && !c.effectsPossible && !c.uncertain &&
              !c.confirmed && f.owner.current.configurationGeneration == 7);
    }
}

} // namespace
int main() {
    preparation();
    successfulTransactions();
    partialFailure();
    unchangedAndSparseCandidates();
    zeroParameters();
    mismatchAndInvalidation();
    correlationAndLocalDeadlines();
    failedReadback();
    localReadbackWithdrawal();
    std::printf("settings writes: %u checks passed\n", checks);
}
