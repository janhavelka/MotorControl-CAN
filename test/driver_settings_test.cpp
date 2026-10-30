// SPDX-License-Identifier: MIT
// Independent literal rows and synthetic traffic exercise acquisition only.
#include "SdoClient.h"
#include <MotorControlCAN/profiles/lichuan_c/DriverSettings.h>
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
struct Field {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t raw;
};
// These deliberately include unknown selectors and values outside write ranges.
const Field driver[] = {{0x2200, 0, 2, 0xffff}, {0x2201, 0, 2, 0}};
const Field limits[] = {{0x2202, 0, 2, 10},
                        {0x607d, 0, 1, 0xff},
                        {0x607d, 1, 4, 0x7fffffff},
                        {0x607d, 2, 4, 0x80000000}};
const Field motion[] = {
    {0x6081, 0, 4, 0xffffffff}, {0x6083, 0, 4, 2001}, {0x6084, 0, 4, 0xffffffff}};
const Field* const rows[] = {driver, limits, motion};
const unsigned counts[] = {2, 4, 3};
const L::DriverGroup groups[] = {L::DriverGroup::DRIVER, L::DriverGroup::SOFT_LIMITS,
                                 L::DriverGroup::MOTION_PARAMETERS};
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
L::DriverContext prepared(unsigned group, uint64_t now = 0, uint64_t deadline = 1000) {
    L::DriverContext c;
    CHECK(L::prepareDriverRead(c, target(), 19, now, deadline, options(), groups[group]));
    return c;
}
L::PreparedRead next(const L::DriverContext& c, uint64_t now) {
    L::PreparedRead p;
    CHECK(L::nextDriver(c, now, p));
    CHECK(p.ready);
    return p;
}
C::SdoCompletion success(const L::DriverContext& c, uint32_t raw, uint64_t start, uint64_t end) {
    C::SdoCompletion e;
    e.transfer = next(c, start).transfer;
    e.nodeGeneration = 11;
    e.admittedUs = start;
    e.completedUs = end;
    e.outcome = C::SdoOutcome::SUCCESS;
    e.queued = e.transmissionPossible = e.protocolAcknowledged = e.effectPossible = true;
    e.size = e.transfer.size;
    for (unsigned i = 0; i < e.size; ++i)
        e.data[i] = uint8_t(raw >> (8 * i));
    return e;
}
void accept(L::DriverContext& c, uint32_t raw, uint64_t now) {
    const C::SdoCompletion e = success(c, raw, now, now);
    CHECK(L::advanceDriver(c, e, now));
}
void configure(C::SdoClient& owner) {
    CHECK(owner.configure(0, target().channel, true, true));
    C::NetworkRoute route;
    route.binding = target().channel.binding;
    route.generation = 11;
    route.heartbeatId = 0x705;
    route.emcyId = 0x85;
    CHECK(owner.configureNetwork(0, route, C::NetworkObserver(), 0));
}
M::Frame response(const Field& f) {
    M::Frame reply;
    reply.id = 0x5a5;
    reply.dlc = reply.payloadLength = 8;
    reply.data[0] = f.width == 1 ? 0x4f : f.width == 2 ? 0x4b : 0x43;
    reply.data[1] = uint8_t(f.index);
    reply.data[2] = uint8_t(f.index >> 8);
    reply.data[3] = f.subindex;
    for (unsigned i = 0; i < 4 && i < f.width; ++i)
        reply.data[4 + i] = uint8_t(f.raw >> (8 * i));
    return reply;
}
C::SdoRequestId transmit(C::SdoClient& owner, const L::DriverContext& c, const Field& f,
                         uint64_t now) {
    const L::PreparedRead p = next(c, now);
    CHECK(p.transfer.index == f.index && p.transfer.subindex == f.subindex &&
          p.transfer.size == f.width && p.transfer.direction == C::SdoDirection::UPLOAD);
    CHECK(p.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED);
    CHECK(p.transfer.operation == 19 && p.transfer.request == 19 &&
          p.transfer.step == c.read.cursor + 1u);
    const L::PreparedRead repeated = next(c, now);
    CHECK(repeated.transfer.step == p.transfer.step);
    C::SdoRequestId id;
    CHECK(owner.admit(0, p.transfer, now, id));
    CHECK(owner.service(now));
    M::Frame tx;
    C::SdoTxId txId;
    CHECK(owner.nextTx(0, now, tx, txId));
    CHECK(tx.id == 0x625 && tx.data[0] == 0x40 && tx.data[1] == uint8_t(f.index) &&
          tx.data[2] == uint8_t(f.index >> 8) && tx.data[3] == f.subindex);
    for (unsigned i = 4; i < 8; ++i)
        CHECK(tx.data[i] == 0);
    CHECK(owner.canTransmit(txId, now));
    CHECK(owner.confirmTx(txId, now));
    return id;
}
void realOwnerGroups() {
    unsigned seen = 0;
    for (unsigned g = 0; g < 3; ++g) {
        C::SdoClient owner;
        configure(owner);
        L::DriverContext c = prepared(g);
        for (unsigned i = 0; i < counts[g]; ++i) {
            const Field& f = rows[g][i];
            const C::SdoRequestId id = transmit(owner, c, f, i * 2);
            M::Frame reply = response(f);
            M::Frame wrong = reply;
            ++wrong.id;
            CHECK(!owner.receive(wrong, i * 2 + 1, i * 2 + 1));
            CHECK(!owner.result(id));
            wrong = reply;
            ++wrong.data[1];
            CHECK(!owner.receive(wrong, i * 2 + 1, i * 2 + 1));
            CHECK(!owner.result(id));
            wrong = reply;
            ++wrong.data[3];
            CHECK(!owner.receive(wrong, i * 2 + 1, i * 2 + 1));
            CHECK(!owner.result(id));
            CHECK(owner.receive(reply, i * 2 + 1, i * 2 + 1));
            const C::SdoCompletion* e = owner.result(id);
            CHECK(e && e->outcome == C::SdoOutcome::SUCCESS && e->effectPossible &&
                  !e->effectUncertain && e->nodeGeneration == 11);
            CHECK(L::advanceDriver(c, *e, i * 2 + 1));
            CHECK(owner.release(id));
            reply.data[4] = 0;
            CHECK(c.read.values[i].raw == f.raw);
            ++seen;
        }
        CHECK(c.read.state == M::ReadState::SUCCEEDED && !c.read.uncertain);
        L::DriverObservation o;
        CHECK(L::getDriver(c, target(), 10, 20, o));
        CHECK(o.group == groups[g] && o.fieldCount == counts[g] && !o.interpreted);
        CHECK(o.window.earliestUs == 0 && o.window.latestUs == counts[g] * 2 - 1 &&
              o.window.skewUs == counts[g] * 2 - 1);
        for (unsigned i = 0; i < counts[g]; ++i) {
            CHECK(o.fields[i].index == rows[g][i].index &&
                  o.fields[i].subindex == rows[g][i].subindex);
            CHECK(o.fields[i].earliestUs == i * 2 && o.fields[i].latestUs == i * 2 + 1 &&
                  o.fields[i].source == L::ReadSource::SDO && !o.fields[i].captureKnown);
        }
        if (g == 0)
            CHECK(o.directionRaw == 0xffff && o.subdivisionRaw == 0);
        else if (g == 1)
            CHECK(o.softLimitSelectorRaw == 10 && o.softLimitEntries == 255 &&
                  o.minimumNative == INT32_MAX && o.maximumNative == INT32_MIN);
        else
            CHECK(o.positionSpeedRpm == UINT32_MAX && o.rampUpTimeMs == 2001 &&
                  o.rampDownTimeMs == UINT32_MAX);
        L::PreparedRead done;
        done.ready = true;
        done.transfer.index = 0xbeef;
        CHECK(L::nextDriver(c, 10, done) && !done.ready && done.transfer.index == 0xbeef);
    }
    CHECK(seen == 9);
}
void admissionAndPublication() {
    for (unsigned g = 0; g < 3; ++g) {
        L::DriverContext c = prepared(g);
        const Saved<L::DriverContext> initial(c);
        for (unsigned bad = 0; bad < 8; ++bad) {
            L::ReadTarget t = target();
            L::ReadOptions o = options();
            uint32_t operation = 19;
            uint64_t deadline = 1000;
            L::DriverGroup group = groups[g];
            switch (bad) {
            case 0:
                t.nodeGeneration = 0;
                break;
            case 1:
                t.channel.binding.configurationGeneration = 0;
                break;
            case 2:
                t.channel.clientToServerId = t.channel.serverToClientId;
                break;
            case 3:
                operation = 0;
                break;
            case 4:
                deadline = 0;
                break;
            case 5:
                o.queueTimeoutUs = 0;
                break;
            case 6:
                o.responseTimeoutUs = 0;
                break;
            default:
                group = static_cast<L::DriverGroup>(255);
                break;
            }
            CHECK(!L::prepareDriverRead(c, t, operation, 0, deadline, o, group));
            initial.unchanged(c);
        }
        CHECK(!L::prepareDriverRead(c, c.read.target, 19, 0, 1000, options(), groups[g]));
        initial.unchanged(c);
        CHECK(!L::prepareDriverRead(c, target(), 19, 0, 1000, c.read.options, groups[g]));
        initial.unchanged(c);
        L::ReadTarget t = target();
        L::ReadOptions opt = options();
        CHECK(L::prepareDriverRead(c, t, 19, 0, 1000, opt, groups[g]));
        ++t.nodeGeneration;
        ++opt.responseTimeoutUs;
        CHECK(c.read.target.nodeGeneration == 11 && c.read.options.responseTimeoutUs == 100);
        L::DriverObservation o;
        o.directionRaw = 0xa55a;
        const Saved<L::DriverObservation> prior(o);
        for (unsigned i = 0; i < counts[g]; ++i) {
            CHECK(!L::getDriver(c, target(), i * 2, 100, o));
            prior.unchanged(o);
            accept(c, rows[g][i].raw, i * 2);
        }
        CHECK(L::getDriver(c, target(), 10, 10, o));
        const Saved<L::DriverObservation> complete(o);
        CHECK(!L::getDriver(c, target(), 10, 9, o));
        complete.unchanged(o);
        CHECK(!L::getDriver(c, target(), 10, 0, o));
        complete.unchanged(o);
        o.target = target();
        const Saved<L::DriverObservation> alias(o);
        CHECK(!L::getDriver(c, o.target, 10, 100, o));
        alias.unchanged(o);
        CHECK(!L::getDriver(c, target(), c.read.servicedUs - 1, 100, o));
        complete.unchanged(o);
        for (unsigned stale = 0; stale < 5; ++stale) {
            t = target();
            if (stale == 0)
                ++t.nodeGeneration;
            if (stale == 1)
                ++t.channel.binding.bindingGeneration;
            if (stale == 2)
                ++t.channel.binding.configurationGeneration;
            if (stale == 3)
                ++t.channel.binding.endpoint.address;
            if (stale == 4)
                ++t.channel.serverToClientId;
            CHECK(!L::getDriver(c, t, 10, 100, o));
            complete.unchanged(o);
        }
        const Saved<L::DriverContext> retained(c);
        CHECK(!L::advanceDriver(c, M::ReadOutcome::CANCELLED, 10));
        retained.unchanged(c);
    }
    L::DriverContext wrong;
    CHECK(L::prepareIdentity(wrong.read, target(), 19, 0, 1000, options()));
    L::PreparedRead p;
    p.ready = true;
    const Saved<L::PreparedRead> old(p);
    CHECK(!L::nextDriver(wrong, 0, p));
    old.unchanged(p);
    L::DriverObservation o;
    const Saved<L::DriverObservation> prior(o);
    CHECK(!L::getDriver(wrong, target(), 0, 100, o));
    prior.unchanged(o);
    const Saved<L::DriverContext> original(wrong);
    CHECK(!L::advanceDriver(wrong, M::ReadOutcome::CANCELLED, 0));
    original.unchanged(wrong);
    L::PreparedRead identityWork;
    CHECK(L::nextRead(wrong.read, 0, identityWork));
    C::SdoCompletion identityTerminal;
    identityTerminal.transfer = identityWork.transfer;
    CHECK(!L::advanceDriver(wrong, identityTerminal, 0));
    original.unchanged(wrong);
    for (unsigned i = 0; i < 9; ++i) {
        CHECK(L::nextRead(wrong.read, 0, identityWork));
        identityTerminal.transfer = identityWork.transfer;
        identityTerminal.nodeGeneration = 11;
        identityTerminal.outcome = C::SdoOutcome::SUCCESS;
        identityTerminal.queued = identityTerminal.transmissionPossible = true;
        identityTerminal.protocolAcknowledged = identityTerminal.effectPossible = true;
        identityTerminal.size = identityWork.transfer.size;
        CHECK(L::advanceRead(wrong.read, identityTerminal, 0));
    }
    CHECK(wrong.read.state == M::ReadState::SUCCEEDED);
    CHECK(!L::getDriver(wrong, target(), 0, 100, o));
    prior.unchanged(o);
}
void terminalCorrelation() {
    for (unsigned g = 0; g < 3; ++g) {
        L::DriverContext c = prepared(g);
        const C::SdoCompletion good = success(c, rows[g][0].raw, 0, 1);
        for (unsigned bad = 0; bad < 18; ++bad) {
            C::SdoCompletion e = good;
            switch (bad) {
            case 0:
                ++e.transfer.operation;
                break;
            case 1:
                ++e.transfer.request;
                break;
            case 2:
                ++e.transfer.step;
                break;
            case 3:
                ++e.transfer.index;
                break;
            case 4:
                ++e.transfer.subindex;
                break;
            case 5:
                ++e.transfer.channel.binding.bindingGeneration;
                break;
            case 6:
                ++e.transfer.channel.binding.configurationGeneration;
                break;
            case 7:
                ++e.transfer.channel.clientToServerId;
                break;
            case 8:
                ++e.nodeGeneration;
                break;
            case 9:
                --e.transfer.size;
                break;
            case 10:
                --e.size;
                break;
            case 11:
                e.transfer.direction = C::SdoDirection::DOWNLOAD;
                break;
            case 12:
                e.protocolAcknowledged = false;
                break;
            case 13:
                e.effectPossible = false;
                break;
            case 14:
                e.effectUncertain = true;
                break;
            case 15:
                e.abortValid = true;
                break;
            case 16:
                e.completedUs = 3;
                break;
            default:
                ++e.transfer.deadlineUs;
                break;
            }
            const Saved<L::DriverContext> before(c);
            CHECK(!L::advanceDriver(c, e, 2));
            before.unchanged(c);
        }
        CHECK(L::advanceDriver(c, good, 1));
        const Saved<L::DriverContext> advanced(c);
        CHECK(!L::advanceDriver(c, good, 2));
        advanced.unchanged(c);
        L::PreparedRead p;
        const Saved<L::PreparedRead> before(p);
        CHECK(!L::nextDriver(c, 0, p));
        before.unchanged(p);
        CHECK(!L::nextDriver(c, 1000, p));
        before.unchanged(p);
    }
}
void partialFailures() {
    const C::SdoOutcome failures[] = {
        C::SdoOutcome::SERVER_ABORT, C::SdoOutcome::TIMEOUT, C::SdoOutcome::TRANSPORT_ERROR,
        C::SdoOutcome::REBOOT,       C::SdoOutcome::BUS_OFF, C::SdoOutcome::BINDING_INVALIDATED,
        C::SdoOutcome::CANCELLED};
    for (unsigned g = 0; g < 3; ++g)
        for (unsigned fail = 0; fail < counts[g]; ++fail)
            for (C::SdoOutcome failure : failures) {
                L::DriverContext c = prepared(g);
                for (unsigned i = 0; i < fail; ++i)
                    accept(c, rows[g][i].raw, i * 2);
                C::SdoCompletion e = success(c, 0, fail * 2, fail * 2 + 1);
                e.outcome = failure;
                e.size = 0;
                e.protocolAcknowledged = false;
                e.effectUncertain = true;
                e.abortValid = failure == C::SdoOutcome::SERVER_ABORT;
                e.abortCode = 0x80000000u + fail;
                CHECK(L::advanceDriver(c, e, fail * 2 + 1));
                CHECK(c.read.state == M::ReadState::FAILED && c.read.cursor == fail &&
                      c.read.uncertain && c.read.evidencePresent);
                CHECK(c.read.evidence.outcome == failure &&
                      c.read.evidence.abortCode == 0x80000000u + fail);
                if (e.abortValid)
                    CHECK(c.read.status.code == M::Err::PROTOCOL_ERROR &&
                          c.read.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
                if (failure == C::SdoOutcome::REBOOT || failure == C::SdoOutcome::BUS_OFF ||
                    failure == C::SdoOutcome::BINDING_INVALIDATED)
                    CHECK(c.read.invalidated);
                for (unsigned i = 0; i < fail; ++i)
                    CHECK(c.read.values[i].valid && c.read.values[i].raw == rows[g][i].raw);
                L::DriverObservation o;
                o.subdivisionRaw = 0xa55a;
                const Saved<L::DriverObservation> prior(o);
                CHECK(!L::getDriver(c, target(), 20, 100, o));
                prior.unchanged(o);
            }
    for (unsigned g = 0; g < 3; ++g)
        for (unsigned fail = 0; fail < counts[g]; ++fail) {
            C::SdoClient owner;
            configure(owner);
            L::DriverContext c = prepared(g);
            for (unsigned i = 0; i < fail; ++i)
                accept(c, rows[g][i].raw, i * 2);
            const C::SdoRequestId id = transmit(owner, c, rows[g][fail], fail * 2);
            M::Frame wrongWidth = response(rows[g][fail]);
            wrongWidth.data[0] = rows[g][fail].width == 4 ? 0x4b : 0x43;
            CHECK(owner.receive(wrongWidth, fail * 2 + 1, fail * 2 + 1));
            const C::SdoCompletion* e = owner.result(id);
            CHECK(e && e->outcome == C::SdoOutcome::CLIENT_ABORT && e->effectUncertain);
            CHECK(L::advanceDriver(c, *e, fail * 2 + 1));
            CHECK(c.read.outcome == M::ReadOutcome::REPLY_ERROR && c.read.uncertain);
            CHECK(owner.release(id));
        }
}
void rawDomainsAndTime() {
    const uint32_t raw[] = {0, 1, 2, 255, 3000, 3001, 2000, 2001, 0x80000000, 0xffffffff};
    for (uint32_t value : raw) {
        L::DriverContext c = prepared(2);
        for (unsigned i = 0; i < 3; ++i)
            accept(c, value, 0);
        L::DriverObservation o;
        CHECK(L::getDriver(c, target(), 0, 1, o));
        CHECK(o.positionSpeedRpm == value && o.rampUpTimeMs == value && o.rampDownTimeMs == value &&
              o.window.skewUs == 0 && !o.interpreted);
    }
    for (uint32_t value : {0u, 400u, 51200u, 65535u}) {
        L::DriverContext c = prepared(0);
        accept(c, value, 0);
        accept(c, value, 0);
        L::DriverObservation o;
        CHECK(L::getDriver(c, target(), 0, 1, o));
        CHECK(o.directionRaw == value && o.subdivisionRaw == value);
    }
    for (uint32_t count : {0u, 1u, 2u, 255u}) {
        L::DriverContext c = prepared(1);
        accept(c, 10, 0);
        accept(c, count, 0);
        CHECK(next(c, 0).transfer.subindex == 1);
        accept(c, 0xffffffff, 0);
        CHECK(next(c, 0).transfer.subindex == 2);
        accept(c, 0x80000000, 0);
        L::DriverObservation o;
        CHECK(L::getDriver(c, target(), 0, 1, o));
        CHECK(o.softLimitEntries == count && o.fieldCount == 4 && o.minimumNative == -1 &&
              o.maximumNative == INT32_MIN);
    }
    for (unsigned g = 0; g < 3; ++g)
        for (unsigned late = 0; late < 2; ++late) {
            L::DriverContext c = prepared(g, 0, 100);
            for (unsigned i = 0; i + 1 < counts[g]; ++i)
                accept(c, rows[g][i].raw, i * 2);
            C::SdoCompletion e = success(c, rows[g][counts[g] - 1].raw, 10, late ? 100 : 99);
            CHECK(L::advanceDriver(c, e, 100));
            CHECK(c.read.outcome == M::ReadOutcome::DEADLINE && c.read.evidencePresent &&
                  !c.read.values[counts[g] - 1].valid);
            CHECK(c.read.values[0].valid);
        }
    for (C::SdoOutcome cause :
         {C::SdoOutcome::REBOOT, C::SdoOutcome::BUS_OFF, C::SdoOutcome::BINDING_INVALIDATED}) {
        L::DriverContext c = prepared(0, 0, 100);
        accept(c, 7, 0);
        C::SdoCompletion e = success(c, 0, 10, 100);
        e.outcome = cause;
        e.protocolAcknowledged = false;
        e.size = 0;
        e.effectUncertain = true;
        CHECK(L::advanceDriver(c, e, 100));
        CHECK(c.read.outcome == M::ReadOutcome::DEADLINE && c.read.invalidated &&
              c.read.uncertain && c.read.values[0].valid && c.read.values[0].raw == 7);
        L::DriverObservation o;
        const Saved<L::DriverObservation> prior(o);
        CHECK(!L::getDriver(c, target(), 100, 1000, o));
        prior.unchanged(o);
    }
    for (M::ReadOutcome cause :
         {M::ReadOutcome::CANCELLED, M::ReadOutcome::INVALIDATED, M::ReadOutcome::DEADLINE}) {
        L::DriverContext c = prepared(0, 0, 100);
        accept(c, 7, 0);
        CHECK(L::advanceDriver(c, cause, 100));
        CHECK(c.read.outcome == M::ReadOutcome::DEADLINE && c.read.uncertain &&
              c.read.values[0].raw == 7);
        CHECK(c.read.invalidated == (cause == M::ReadOutcome::INVALIDATED));
    }
    L::DriverContext latest = prepared(0, UINT64_MAX - 1, UINT64_MAX);
    accept(latest, 0, UINT64_MAX - 1);
    accept(latest, 400, UINT64_MAX - 1);
    L::DriverObservation o;
    CHECK(L::getDriver(latest, target(), UINT64_MAX, 1, o));
}
} // namespace
int main() {
    realOwnerGroups();
    admissionAndPublication();
    terminalCorrelation();
    partialFailures();
    rawDomainsAndTime();
    std::printf("driver settings: %u checks passed\n", checks);
}
