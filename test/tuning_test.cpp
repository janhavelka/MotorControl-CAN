// SPDX-License-Identifier: MIT
// Independent literal objects and wire words exercise the ordinary public path.
#include "SdoClient.h"
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
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
const uint16_t indices[][12] = {{0x2500, 0x2501},
                                {0x2502, 0x2503, 0x2504, 0x2505, 0x2506, 0x2507},
                                {0x2508, 0x2509, 0x250a, 0x250b, 0x250c, 0x250d},
                                {0x250e, 0x250f, 0x2510, 0x2511, 0x2512, 0x2513, 0x2514, 0x2515,
                                 0x2516, 0x2517, 0x2518, 0x2519},
                                {0x251a, 0x251b, 0x251c, 0x251d, 0x251e, 0x251f, 0x2520}};
const unsigned counts[] = {2, 6, 6, 12, 7};
const uint16_t words[][12] = {{0x1234, 0xffff},
                              {0xffff, 151, 251, 65534, 101, 0},
                              {0x1234, 0xffff, 0, 65534, 0x4321, 513},
                              {0, 1, 0xffff, 32768, 111, 222, 900, 333, 444, 100, 555, 666},
                              {0, 0xffff, 0x8000, 0x7fff, 0x1234, 0x4321, 17}};
const L::TuningGroup groups[] = {L::TuningGroup::CURRENT_HOLDING, L::TuningGroup::FILTER_POSITION,
                                 L::TuningGroup::LOOP_GAINS, L::TuningGroup::EDS_GAINS};
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
M::Status prepare(L::DriverContext& c, unsigned group, const L::ReadTarget& t, uint32_t operation,
                  uint64_t now, uint64_t deadline, const L::ReadOptions& o) {
    return group ? L::prepareTuningRead(c, t, operation, now, deadline, o, groups[group - 1])
                 : L::prepareControlRead(c, t, operation, now, deadline, o);
}
L::DriverContext prepared(unsigned group) {
    L::DriverContext c;
    CHECK(prepare(c, group, target(), 19, 0, 5000, options()));
    return c;
}
L::PreparedRead next(const L::DriverContext& c, uint64_t now) {
    L::PreparedRead p;
    CHECK(L::nextDriver(c, now, p) && p.ready);
    return p;
}
C::SdoCompletion success(const L::DriverContext& c, uint16_t raw, uint64_t start, uint64_t end) {
    C::SdoCompletion e;
    e.transfer = next(c, start).transfer;
    e.nodeGeneration = 11;
    e.admittedUs = start;
    e.completedUs = end;
    e.outcome = C::SdoOutcome::SUCCESS;
    e.queued = e.transmissionPossible = e.protocolAcknowledged = e.effectPossible = true;
    e.size = 2;
    e.data[0] = uint8_t(raw);
    e.data[1] = uint8_t(raw >> 8);
    return e;
}
void accept(L::DriverContext& c, uint16_t raw, uint64_t now) {
    const auto e = success(c, raw, now, now);
    CHECK(L::advanceDriver(c, e, now));
}
void checkMembers(const L::TuningObservation& o, unsigned group) {
    CHECK(o.group == groups[group - 1] && o.fieldCount == counts[group]);
    if (group == 1) {
        CHECK(o.maximumPeakCurrentMa == 0xffff && o.closedLoopMaximumPercent == 151);
        CHECK(o.closedLoopBasePercent == 251 && o.openLoopMaximumPercent == 65534);
        CHECK(o.holdingCurrentPercent == 101 && o.holdingTimeMs == 0);
    } else if (group == 2) {
        CHECK(o.lowPassEnableRaw == 0x1234 && o.lowPassCoefficientRaw == 0xffff);
        CHECK(o.positionAlarmThresholdRaw == 0 && o.inPositionThresholdRaw == 65534);
        CHECK(o.inPositionTimeRaw == 0x4321 && o.averagingFilterRaw == 513);
    } else if (group == 3) {
        CHECK(o.currentProportionalFactorRaw == 0 && o.currentKpRaw == 1);
        CHECK(o.currentKiRaw == 0xffff && o.currentKcRaw == 32768);
        CHECK(o.velocityKp1Raw == 111 && o.velocityKv1Raw == 222 && o.velocityNode1Raw == 900);
        CHECK(o.velocityKp2Raw == 333 && o.velocityKv2Raw == 444 && o.velocityNode2Raw == 100);
        CHECK(o.velocityFeedforwardRaw == 555 && o.positionKiRaw == 666);
    } else {
        const uint16_t expected[] = {0, 0xffff, 0x8000, 0x7fff, 0x1234, 0x4321, 17};
        for (unsigned i = 0; i < 7; ++i)
            CHECK(o.edsGainsRaw[i] == expected[i]);
    }
}
struct Owner {
    C::SdoClient service;
    L::SettingsReservation reservation;
    M::Binding current;
    uint64_t now = 0;
    explicit Owner(M::Generation configuration = 7) {
        reservation.target = target();
        reservation.target.channel.binding.configurationGeneration = configuration;
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = 3;
        reservation.sequence = 5;
        reservation.generation = 9;
        current = reservation.target.channel.binding;
        CHECK(service.configure(0, reservation.target.channel, true, true));
        C::NetworkRoute route;
        route.binding = current;
        route.generation = 11;
        route.heartbeatId = 0x705;
        route.emcyId = 0x85;
        CHECK(service.configureNetwork(0, route, C::NetworkObserver(), now));
    }
    L::ReadTarget currentTarget() const {
        auto t = reservation.target;
        t.channel.binding = current;
        return t;
    }
    C::SdoRequestId transmit(const C::SdoTransfer& work,
                             L::DriverSettingsContext* settings = nullptr) {
        CHECK(work.subindex == 0 && work.size == 2);
        C::SdoRequestId id;
        CHECK(service.admit(0, work, now, id));
        CHECK(service.service(now));
        if (settings)
            CHECK(L::noteDriverTransmission(*settings, reservation, current, now));
        M::Frame tx;
        C::SdoTxId txId;
        CHECK(service.nextTx(0, now, tx, txId));
        CHECK(tx.id == 0x625 && tx.data[1] == uint8_t(work.index) &&
              tx.data[2] == uint8_t(work.index >> 8) && tx.data[3] == 0);
        if (work.direction == C::SdoDirection::UPLOAD) {
            CHECK(tx.data[0] == 0x40 && work.readEffect == C::SdoReadEffect::UNQUALIFIED);
            for (unsigned i = 4; i < 8; ++i)
                CHECK(tx.data[i] == 0);
        } else {
            CHECK(tx.data[0] == 0x2b && work.index == 0x2500);
            CHECK(tx.data[4] == work.data[0] && tx.data[5] == work.data[1]);
        }
        CHECK(service.canTransmit(txId, now));
        CHECK(service.confirmTx(txId, now));
        return id;
    }
    C::SdoCompletion respond(C::SdoRequestId id, const C::SdoTransfer& work, uint16_t raw,
                             unsigned fault = 0) {
        M::Frame f;
        f.id = 0x5a5;
        f.dlc = f.payloadLength = 8;
        f.data[0] = fault == 1                                  ? 0x43
                    : fault == 2                                ? 0x80
                    : work.direction == C::SdoDirection::UPLOAD ? 0x4b
                                                                : 0x60;
        f.data[1] = uint8_t(work.index);
        f.data[2] = uint8_t(work.index >> 8);
        if (fault == 2) {
            f.data[4] = 1;
            f.data[6] = 1;
            f.data[7] = 6;
        } else if (work.direction == C::SdoDirection::UPLOAD) {
            f.data[4] = uint8_t(raw);
            f.data[5] = uint8_t(raw >> 8);
        }
        ++now;
        CHECK(service.receive(f, now, now));
        const auto* e = service.result(id);
        CHECK(e);
        CHECK(e->outcome == (fault == 1   ? C::SdoOutcome::CLIENT_ABORT
                             : fault == 2 ? C::SdoOutcome::SERVER_ABORT
                                          : C::SdoOutcome::SUCCESS));
        auto result = *e;
        CHECK(service.release(id));
        return result;
    }
    L::DriverContext controlPrior(uint16_t mode = 0) {
        L::DriverContext c;
        CHECK(L::prepareControlRead(c, reservation.target, 19, now, 5000, options()));
        for (unsigned i = 0; i < 2; ++i) {
            const auto work = next(c, now);
            const auto id = transmit(work.transfer);
            const auto e = respond(id, work.transfer, i ? 0xffff : mode);
            CHECK(L::advanceDriver(c, e, now));
        }
        return c;
    }
};
void literalReadsAndOwnedEvidence() {
    unsigned seen = 0;
    for (unsigned g = 0; g < 5; ++g) {
        Owner owner;
        auto c = prepared(g);
        for (unsigned i = 0; i < counts[g]; ++i) {
            const auto p = next(c, owner.now);
            CHECK(p.transfer.index == indices[g][i] && p.transfer.subindex == 0 &&
                  p.transfer.size == 2);
            CHECK(p.transfer.direction == C::SdoDirection::UPLOAD && p.transfer.operation == 19 &&
                  p.transfer.step == i + 1);
            auto copy = p;
            copy.transfer.index = 0x1010;
            copy.transfer.data[0] = 0xaa;
            CHECK(copy.transfer.index != p.transfer.index);
            CHECK(next(c, owner.now).transfer.index == indices[g][i]);
            const auto id = owner.transmit(p.transfer);
            auto e = owner.respond(id, p.transfer, words[g][i]);
            CHECK(L::advanceDriver(c, e, owner.now));
            e.data[0] = e.data[1] = 0;
            CHECK(c.read.values[i].raw == words[g][i]);
            ++owner.now;
            ++seen;
        }
        CHECK(c.read.state == M::ReadState::SUCCEEDED && !c.read.uncertain);
        L::ControlObservation control;
        L::TuningObservation tuning;
        CHECK(g ? bool(L::getTuning(c, target(), owner.now, 100, tuning))
                : bool(L::getControl(c, target(), owner.now, 100, control)));
        if (g)
            checkMembers(tuning, g);
        else
            CHECK(control.loopModeRaw == 0x1234 && control.loopMode == L::LoopMode::UNKNOWN &&
                  control.encoderResolutionRaw == 0xffff);
        const auto& window = g ? tuning.window : control.window;
        CHECK(window.earliestUs == 0 && window.latestUs == counts[g] * 2 - 1 &&
              window.skewUs == counts[g] * 2 - 1);
        for (unsigned i = 0; i < counts[g]; ++i) {
            const auto& field = g ? tuning.fields[i] : control.fields[i];
            CHECK(field.index == indices[g][i] && field.subindex == 0);
            CHECK(field.earliestUs == i * 2 && field.latestUs == i * 2 + 1 &&
                  field.source == L::ReadSource::SDO && !field.captureKnown);
        }
        L::PreparedRead terminal;
        terminal.ready = true;
        terminal.transfer.index = 0xbeef;
        CHECK(L::nextDriver(c, owner.now, terminal) && !terminal.ready &&
              terminal.transfer.index == 0xbeef);
    }
    CHECK(seen == 33);
    for (unsigned mode = 0; mode < 3; ++mode) {
        auto c = prepared(0);
        accept(c, uint16_t(mode), 0);
        accept(c, 0, 0);
        L::ControlObservation o;
        CHECK(L::getControl(c, target(), 0, 1, o));
        CHECK(o.loopMode == static_cast<L::LoopMode>(mode) && o.encoderResolutionRaw == 0);
    }
}
void atomicPreparationAndGetters() {
    for (unsigned g = 0; g < 5; ++g) {
        auto c = prepared(g);
        const Saved<L::DriverContext> initial(c);
        for (unsigned bad = 0; bad < 7; ++bad) {
            auto t = target();
            auto o = options();
            uint32_t id = 19;
            uint64_t deadline = 5000;
            if (bad == 0)
                t.nodeGeneration = 0;
            if (bad == 1)
                t.channel.binding.configurationGeneration = 0;
            if (bad == 2)
                t.channel.clientToServerId = t.channel.serverToClientId;
            if (bad == 3)
                id = 0;
            if (bad == 4)
                deadline = 0;
            if (bad == 5)
                o.queueTimeoutUs = 0;
            if (bad == 6)
                o.responseTimeoutUs = 0;
            CHECK(!prepare(c, g, t, id, 0, deadline, o));
            initial.unchanged(c);
        }
        CHECK(!prepare(c, g, c.read.target, 19, 0, 5000, options()));
        initial.unchanged(c);
        CHECK(!prepare(c, g, target(), 19, 0, 5000, c.read.options));
        initial.unchanged(c);
        if (g) {
            CHECK(!L::prepareTuningRead(c, target(), 19, 0, 5000, options(),
                                        static_cast<L::TuningGroup>(255)));
            initial.unchanged(c);
        }
        L::ControlObservation control;
        control.loopModeRaw = 0xa55a;
        L::TuningObservation tuning;
        tuning.inPositionTimeRaw = 0xa55a;
        Saved<L::ControlObservation> oldControl(control);
        Saved<L::TuningObservation> oldTuning(tuning);
        for (unsigned i = 0; i < counts[g]; ++i) {
            CHECK(!L::getControl(c, target(), i * 2, 100, control));
            oldControl.unchanged(control);
            CHECK(!L::getTuning(c, target(), i * 2, 100, tuning));
            oldTuning.unchanged(tuning);
            accept(c, words[g][i], i * 2);
        }
        const uint64_t now = counts[g] * 2;
        CHECK(g ? bool(L::getTuning(c, target(), now, now, tuning))
                : bool(L::getControl(c, target(), now, now, control)));
        const Saved<L::ControlObservation> completeControl(control);
        const Saved<L::TuningObservation> completeTuning(tuning);
        for (unsigned bad = 0; bad < 9; ++bad) {
            auto t = target();
            uint64_t at = now, age = now;
            if (bad == 0)
                ++t.nodeGeneration;
            if (bad == 1)
                ++t.channel.binding.bindingGeneration;
            if (bad == 2)
                ++t.channel.binding.configurationGeneration;
            if (bad == 3)
                ++t.channel.binding.endpoint.address;
            if (bad == 4)
                ++t.channel.serverToClientId;
            if (bad == 5)
                age = 0;
            if (bad == 6)
                --age; // Newest fields are fresh; the oldest field is expired.
            if (bad == 7)
                at = c.read.servicedUs - 1;
            if (bad == 8)
                ++t.channel.binding.logicalAxisId;
            CHECK(!(g ? bool(L::getTuning(c, t, at, age, tuning))
                      : bool(L::getControl(c, t, at, age, control))));
            completeControl.unchanged(control);
            completeTuning.unchanged(tuning);
        }
        CHECK(g ? !bool(L::getControl(c, target(), now, 100, control))
                : !bool(L::getTuning(c, target(), now, 100, tuning)));
        completeControl.unchanged(control);
        completeTuning.unchanged(tuning);
        if (g) {
            CHECK(!L::getTuning(c, tuning.target, now, 100, tuning));
            completeTuning.unchanged(tuning);
        } else {
            CHECK(!L::getControl(c, control.target, now, 100, control));
            completeControl.unchanged(control);
        }
    }
    auto c = prepared(0);
    CHECK(L::prepareIdentity(c.read, target(), 19, 0, 5000, options()));
    L::PreparedRead p;
    const Saved<L::PreparedRead> old(p);
    CHECK(!L::nextDriver(c, 0, p));
    old.unchanged(p);
}
void everyRowRejectsWrongWidthAndAccess() {
    unsigned seen = 0;
    for (unsigned g = 0; g < 5; ++g)
        for (unsigned fail = 0; fail < counts[g]; ++fail) {
            for (unsigned fault = 1; fault <= 2; ++fault) {
                Owner owner;
                auto c = prepared(g);
                for (unsigned i = 0; i < fail; ++i)
                    accept(c, words[g][i], 0);
                const auto p = next(c, 0);
                CHECK(p.transfer.index == indices[g][fail]);
                const auto id = owner.transmit(p.transfer);
                const auto e = owner.respond(id, p.transfer, words[g][fail], fault);
                CHECK(L::advanceDriver(c, e, owner.now));
                CHECK(c.read.state == M::ReadState::FAILED && c.read.cursor == fail &&
                      c.read.evidencePresent);
                if (fault == 2)
                    CHECK(c.read.outcome == M::ReadOutcome::PROTOCOL_REJECTED &&
                          c.read.evidence.abortCode == 0x06010001);
                for (unsigned i = 0; i < fail; ++i)
                    CHECK(c.read.values[i].valid && c.read.values[i].raw == words[g][i]);
                L::ControlObservation co;
                const Saved<L::ControlObservation> oldCo(co);
                L::TuningObservation tu;
                const Saved<L::TuningObservation> oldTu(tu);
                CHECK(!L::getControl(c, target(), owner.now, 100, co));
                oldCo.unchanged(co);
                CHECK(!L::getTuning(c, target(), owner.now, 100, tu));
                oldTu.unchanged(tu);
            }
            ++seen;
        }
    CHECK(seen == 33);
}
void correlationAndPartialFailures() {
    for (unsigned g = 0; g < 5; ++g) {
        auto c = prepared(g);
        const auto good = success(c, words[g][0], 0, 1);
        for (unsigned bad = 0; bad < 14; ++bad) {
            auto e = good;
            if (bad == 0)
                ++e.transfer.operation;
            if (bad == 1)
                ++e.transfer.request;
            if (bad == 2)
                ++e.transfer.step;
            if (bad == 3)
                ++e.transfer.index;
            if (bad == 4)
                ++e.transfer.subindex;
            if (bad == 5)
                ++e.transfer.channel.binding.bindingGeneration;
            if (bad == 6)
                ++e.transfer.channel.binding.configurationGeneration;
            if (bad == 7)
                ++e.nodeGeneration;
            if (bad == 8)
                e.transfer.size = 4;
            if (bad == 9)
                e.size = 4;
            if (bad == 10)
                e.transfer.direction = C::SdoDirection::DOWNLOAD;
            if (bad == 11)
                e.protocolAcknowledged = false;
            if (bad == 12)
                e.completedUs = 3;
            if (bad == 13)
                ++e.transfer.deadlineUs;
            const Saved<L::DriverContext> old(c);
            CHECK(!L::advanceDriver(c, e, 2));
            old.unchanged(c);
        }
        CHECK(L::advanceDriver(c, good, 1));
        const Saved<L::DriverContext> old(c);
        CHECK(!L::advanceDriver(c, good, 2));
        old.unchanged(c);
        for (unsigned fail = 0; fail < counts[g]; ++fail)
            for (unsigned kind = 0; kind < 3; ++kind) {
                c = prepared(g);
                for (unsigned i = 0; i < fail; ++i)
                    accept(c, words[g][i], 0);
                auto e = success(c, 0, 0, 1);
                e.outcome = kind == 0   ? C::SdoOutcome::TIMEOUT
                            : kind == 1 ? C::SdoOutcome::REBOOT
                                        : C::SdoOutcome::TRANSPORT_ERROR;
                e.size = 0;
                e.protocolAcknowledged = false;
                e.effectUncertain = true;
                CHECK(L::advanceDriver(c, e, 1));
                CHECK(c.read.state == M::ReadState::FAILED && c.read.cursor == fail &&
                      c.read.uncertain);
                for (unsigned i = 0; i < fail; ++i)
                    CHECK(c.read.values[i].raw == words[g][i]);
            }
    }
}
void loopCandidatesAndRealWrite() {
    for (unsigned model = 0; model < 3; ++model)
        for (unsigned value = 0; value < 5; ++value) {
            Owner owner;
            const auto prior = owner.controlPrior();
            owner.reservation.model = model == 0   ? L::SettingsModel::UNKNOWN
                                      : model == 1 ? L::SettingsModel::CL86_C
                                                   : L::SettingsModel::OL86_C;
            L::ControlSettings candidate;
            candidate.loopMode =
                value == 4 ? L::LoopMode::UNKNOWN : static_cast<L::LoopMode>(value);
            L::DriverSettingsContext c;
            const Saved<L::DriverSettingsContext> old(c);
            const bool expected = model && value <= (model == 1 ? 2u : 1u);
            CHECK(bool(L::prepareControlSettings(c, owner.reservation, prior, candidate, 41,
                                                 owner.now, 5000, options(), 100)) == expected);
            if (!expected)
                old.unchanged(c);
            else {
                CHECK(c.state ==
                      (value == 0 ? L::SettingsState::SUCCEEDED : L::SettingsState::WRITING));
                if (value == 0) {
                    CHECK(!c.changed && !c.effectsPossible && !c.uncertain &&
                          owner.current.configurationGeneration == 7);
                    L::PreparedRead done;
                    done.ready = true;
                    CHECK(L::nextDriver(c, owner.reservation, owner.current, owner.now, done) &&
                          !done.ready);
                }
            }
        }
    for (unsigned fault = 0; fault < 5; ++fault) {
        Owner owner;
        const auto prior = owner.controlPrior(fault == 3 ? 0xffff : 0);
        L::ControlSettings candidate;
        candidate.loopMode = L::LoopMode::CLOSED_LOOP;
        L::DriverSettingsContext c;
        CHECK(L::prepareControlSettings(c, owner.reservation, prior, candidate, 41, owner.now, 5000,
                                        options(), 100));
        candidate.loopMode = L::LoopMode::OPEN_LOOP;
        L::PreparedRead p;
        CHECK(L::nextDriver(c, owner.reservation, owner.current, owner.now, p) && p.ready);
        CHECK(p.transfer.index == 0x2500 && p.transfer.direction == C::SdoDirection::DOWNLOAD &&
              p.transfer.data[0] == 2 && p.transfer.data[1] == 0);
        const auto id = owner.transmit(p.transfer, &c);
        CHECK(owner.current.configurationGeneration == 8 && c.effectsPossible);
        L::ControlObservation obsolete;
        const Saved<L::ControlObservation> oldObsolete(obsolete);
        CHECK(!L::getControl(prior, owner.currentTarget(), owner.now, 100, obsolete));
        oldObsolete.unchanged(obsolete);
        if (fault == 2) {
            CHECK(owner.service.cancel(id, ++owner.now));
            const auto* e = owner.service.result(id);
            CHECK(e);
            CHECK(L::advanceDriver(c, owner.reservation, owner.current, *e, owner.now));
            CHECK(owner.service.release(id));
            CHECK(c.state == L::SettingsState::FAILED && c.uncertain && c.effectsPossible &&
                  !c.confirmed);
            continue;
        }
        const auto e = owner.respond(id, p.transfer, 0);
        CHECK(L::advanceDriver(c, owner.reservation, owner.current, e, owner.now));
        CHECK(c.state == L::SettingsState::READING_BACK && c.confirmed == 1);
        CHECK(owner.service.advanceConfiguration(0, target().channel.binding, owner.current,
                                                 owner.now));
        for (unsigned i = 0; i < 2; ++i) {
            CHECK(L::nextDriver(c, owner.reservation, owner.current, owner.now, p) && p.ready);
            CHECK(p.transfer.index == (i ? 0x2501 : 0x2500) &&
                  p.transfer.direction == C::SdoDirection::UPLOAD &&
                  p.transfer.channel.binding.configurationGeneration == 8);
            const auto readId = owner.transmit(p.transfer);
            const auto read = owner.respond(readId, p.transfer,
                                            i            ? fault == 4 ? 1234 : 0xffff
                                            : fault == 1 ? 1
                                                         : 2);
            CHECK(L::advanceDriver(c, owner.reservation, owner.current, read, owner.now));
            if (c.state == L::SettingsState::FAILED)
                break;
        }
        L::ControlObservation out;
        const Saved<L::ControlObservation> old(out);
        if (fault == 1 || fault == 4) {
            CHECK(c.state == L::SettingsState::FAILED && c.confirmed == 1 && c.effectsPossible);
            CHECK(!L::getControl(c, owner.reservation, owner.currentTarget(), owner.now, 100, out));
            old.unchanged(out);
        } else {
            CHECK(c.state == L::SettingsState::SUCCEEDED && c.confirmed == 1 && !c.uncertain);
            CHECK(L::getControl(c, owner.reservation, owner.currentTarget(), owner.now, 100, out));
            CHECK(out.loopMode == L::LoopMode::CLOSED_LOOP && out.encoderResolutionRaw == 0xffff);
            const Saved<L::ControlObservation> complete(out);
            CHECK(!L::getControl(c, owner.reservation, target(), owner.now, 100, out));
            complete.unchanged(out);
            if (fault == 3) {
                owner.reservation.owner = 0;
                CHECK(!L::getControl(c, owner.reservation, owner.currentTarget(), owner.now, 100,
                                     out));
                complete.unchanged(out);
            }
        }
    }
}
void settingsAdmissionAndShape() {
    Owner owner;
    const auto prior = owner.controlPrior();
    L::ControlSettings candidate;
    candidate.loopMode = L::LoopMode::OPEN_LOOP;
    L::DriverSettingsContext valid;
    CHECK(L::prepareControlSettings(valid, owner.reservation, prior, candidate, 41, owner.now, 5000,
                                    options(), 100));
    for (unsigned bad = 0; bad < 10; ++bad) {
        auto r = owner.reservation;
        auto p = prior;
        auto opt = options();
        uint32_t operation = 41;
        uint64_t age = 100, deadline = 5000;
        if (bad == 0)
            r.owner = 0;
        if (bad == 1)
            ++r.acquiredUs;
        if (bad == 2)
            ++r.target.channel.binding.configurationGeneration;
        if (bad == 3)
            p.read.state = M::ReadState::ACTIVE;
        if (bad == 4)
            operation = 0;
        if (bad == 5)
            age = 0;
        if (bad == 6)
            age = 1;
        if (bad == 7)
            deadline = owner.now;
        if (bad == 8)
            opt.queueTimeoutUs = 0;
        if (bad == 9)
            opt.responseTimeoutUs = 0;
        auto c = valid;
        const Saved<L::DriverSettingsContext> old(c);
        CHECK(!L::prepareControlSettings(c, r, p, candidate, operation, owner.now, deadline, opt,
                                         age));
        old.unchanged(c);
    }
    for (unsigned bad = 0; bad < 10; ++bad) {
        auto c = valid;
        auto live = owner.reservation;
        if (bad == 0)
            c.readback.read.values[0].object.index = 0x2501;
        if (bad == 1)
            c.readback.read.values[0].object.index = 0x251a;
        if (bad == 2)
            c.readback.read.values[0].object.subindex = 1;
        if (bad == 3)
            c.readback.read.values[0].object.widthBytes = 4;
        if (bad == 4)
            c.changed = 2;
        if (bad == 5)
            c.cursor = 1;
        if (bad == 6)
            c.confirmed = 2;
        if (bad == 7)
            c.requested[0] = 3;
        if (bad == 8) {
            c.reservation.model = live.model = L::SettingsModel::OL86_C;
            c.requested[0] = 2;
        }
        if (bad == 9)
            c.readback.read.count = 3;
        const Saved<L::DriverSettingsContext> old(c);
        L::PreparedRead p;
        const Saved<L::PreparedRead> work(p);
        CHECK(!L::nextDriver(c, live, owner.current, owner.now, p));
        work.unchanged(p);
        old.unchanged(c);
        auto binding = owner.current;
        const Saved<M::Binding> oldBinding(binding);
        CHECK(!L::noteDriverTransmission(c, live, binding, owner.now));
        old.unchanged(c);
        oldBinding.unchanged(binding);
    }
}
void loopFailureRetentionAndPublication() {
    for (unsigned field = 0; field < 2; ++field)
        for (unsigned failure = 0; failure < 3; ++failure) {
            Owner owner;
            const auto prior = owner.controlPrior();
            L::ControlSettings candidate;
            candidate.loopMode = L::LoopMode::CLOSED_LOOP;
            L::DriverSettingsContext c;
            CHECK(L::prepareControlSettings(c, owner.reservation, prior, candidate, 41, owner.now,
                                            5000, options(), 100));
            L::ControlObservation out;
            out.encoderResolutionRaw = 0xa55a;
            const Saved<L::ControlObservation> old(out);
            CHECK(!L::getControl(c, owner.reservation, owner.currentTarget(), owner.now, 100, out));
            old.unchanged(out);
            L::PreparedRead p;
            CHECK(L::nextDriver(c, owner.reservation, owner.current, owner.now, p) && p.ready);
            const auto writeId = owner.transmit(p.transfer, &c);
            const auto acknowledged = owner.respond(writeId, p.transfer, 0);
            CHECK(L::advanceDriver(c, owner.reservation, owner.current, acknowledged, owner.now));
            CHECK(c.confirmed == 1 && c.acknowledgedUs[0] == acknowledged.completedUs);
            const uint64_t acknowledgedUs = c.acknowledgedUs[0];
            CHECK(owner.service.advanceConfiguration(0, target().channel.binding, owner.current,
                                                     owner.now));
            CHECK(!L::getControl(c, owner.reservation, owner.currentTarget(), owner.now, 100, out));
            old.unchanged(out);
            if (field == 1) {
                CHECK(L::nextDriver(c, owner.reservation, owner.current, owner.now, p) && p.ready);
                CHECK(p.transfer.index == 0x2500);
                const auto id = owner.transmit(p.transfer);
                const auto first = owner.respond(id, p.transfer, 2);
                CHECK(L::advanceDriver(c, owner.reservation, owner.current, first, owner.now));
                CHECK(c.readback.read.values[0].valid && c.readback.read.values[0].raw == 2);
            }
            CHECK(L::nextDriver(c, owner.reservation, owner.current, owner.now, p) && p.ready);
            CHECK(p.transfer.index == (field ? 0x2501 : 0x2500));
            const auto id = owner.transmit(p.transfer);
            if (failure == 0) {
                const auto aborted = owner.respond(id, p.transfer, 0, 2);
                CHECK(L::advanceDriver(c, owner.reservation, owner.current, aborted, owner.now));
                CHECK(c.outcome == M::ReadOutcome::PROTOCOL_REJECTED && c.evidence.abortValid &&
                      c.evidence.abortCode == 0x06010001);
            } else if (failure == 1) {
                CHECK(owner.service.cancel(id, ++owner.now));
                const auto* terminal = owner.service.result(id);
                CHECK(terminal && terminal->transmissionPossible && terminal->effectUncertain);
                CHECK(owner.service.release(id));
                CHECK(L::advanceDriver(c, M::ReadOutcome::CANCELLED, owner.now));
                CHECK(c.outcome == M::ReadOutcome::CANCELLED);
            } else {
                owner.now += 100;
                CHECK(owner.service.service(owner.now));
                const auto* terminal = owner.service.result(id);
                CHECK(terminal && terminal->outcome == C::SdoOutcome::TIMEOUT &&
                      terminal->effectUncertain && terminal->transmissionPossible);
                const auto timedOut = *terminal;
                CHECK(owner.service.release(id));
                CHECK(L::advanceDriver(c, owner.reservation, owner.current, timedOut, owner.now));
                CHECK(c.outcome == M::ReadOutcome::DEADLINE);
            }
            CHECK(c.state == L::SettingsState::FAILED && c.effectsPossible && c.uncertain &&
                  c.confirmed == 1 && c.acknowledgedUs[0] == acknowledgedUs);
            CHECK(owner.current.configurationGeneration == 8);
            if (field)
                CHECK(c.readback.read.values[0].valid && c.readback.read.values[0].raw == 2);
            CHECK(
                !L::getControl(c, owner.reservation, owner.currentTarget(), owner.now, 1000, out));
            old.unchanged(out);
            const Saved<L::DriverSettingsContext> retained(c);
            CHECK(!L::advanceDriver(c, M::ReadOutcome::CANCELLED, owner.now));
            retained.unchanged(c);
        }
    Owner owner;
    const auto prior = owner.controlPrior();
    L::ControlSettings candidate;
    candidate.loopMode = L::LoopMode::OPEN_LOOP;
    L::DriverSettingsContext c;
    CHECK(L::prepareControlSettings(c, owner.reservation, prior, candidate, 41, owner.now, 5000,
                                    options(), 100));
    L::PreparedRead p;
    CHECK(L::nextDriver(c, owner.reservation, owner.current, owner.now, p) && p.ready);
    C::SdoRequestId id;
    CHECK(owner.service.admit(0, p.transfer, owner.now, id));
    CHECK(owner.service.service(owner.now));
    CHECK(owner.service.cancel(id, owner.now));
    const auto* terminal = owner.service.result(id);
    CHECK(terminal && !terminal->transmissionPossible && !terminal->effectUncertain);
    CHECK(L::advanceDriver(c, owner.reservation, owner.current, *terminal, owner.now));
    CHECK(owner.service.release(id));
    CHECK(c.state == L::SettingsState::FAILED && c.outcome == M::ReadOutcome::CANCELLED &&
          !c.effectsPossible && !c.uncertain && !c.confirmed &&
          owner.current.configurationGeneration == 7);

    // A genuine successful DI transaction is still the wrong settings kind.
    L::DriverContext ioPrior;
    CHECK(L::prepareDriverRead(ioPrior, owner.reservation.target, 23, 0, 5000, options(),
                               L::DriverGroup::IO_INPUT_CONFIGURATION));
    for (unsigned i = 0; i < 9; ++i)
        accept(ioPrior, i ? 0 : 16, 0);
    L::IoSettings io;
    io.invertedInputs = 0;
    for (unsigned i = 0; i < 7; ++i)
        io.functions[i] = L::InputFunction::UNDEFINED;
    L::DriverSettingsContext wrong;
    CHECK(L::prepareIoSettings(wrong, owner.reservation, ioPrior, io, 42, owner.now, 5000,
                               options(), 100));
    CHECK(wrong.state == L::SettingsState::SUCCEEDED && !wrong.changed);
    L::ControlObservation out;
    out.loopModeRaw = 0xa55a;
    const Saved<L::ControlObservation> old(out);
    CHECK(!L::getControl(wrong, owner.reservation, owner.currentTarget(), owner.now, 100, out));
    old.unchanged(out);

    Owner exhausted(UINT32_MAX);
    const auto maximumPrior = exhausted.controlPrior();
    const Saved<L::DriverSettingsContext> untouched(c);
    CHECK(!L::prepareControlSettings(c, exhausted.reservation, maximumPrior, candidate, 43,
                                     exhausted.now, 5000, options(), 100));
    untouched.unchanged(c);
    CHECK(exhausted.current.configurationGeneration == UINT32_MAX);
}
} // namespace
int main() {
    literalReadsAndOwnedEvidence();
    atomicPreparationAndGetters();
    everyRowRejectsWrongWidthAndAccess();
    correlationAndPartialFailures();
    loopCandidatesAndRealWrite();
    settingsAdmissionAndShape();
    loopFailureRetentionAndPublication();
    std::printf("tuning: %u checks passed\n", checks);
}
