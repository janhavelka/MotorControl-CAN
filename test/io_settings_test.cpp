// SPDX-License-Identifier: MIT
// Independent literal rows and synthetic traffic exercise acquisition only.
#include "SdoClient.h"
#include <MotorControlCAN/profiles/lichuan_c/IoSettings.h>
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
// Independent literal addresses: no guessed subentries from count values.
const Field inputs[] = {{0x2400, 0, 2, 0xffff}, {0x2400, 1, 2, 0xffa5}, {0x2400, 2, 2, 0},
                        {0x2400, 3, 2, 1},      {0x2400, 4, 2, 2},      {0x2400, 5, 2, 3},
                        {0x2400, 6, 2, 4},      {0x2400, 7, 2, 13},     {0x2400, 8, 2, 14}};
const Field outputs[] = {{0x2400, 0x0c, 2, 0xffff},
                         {0x2400, 0x0d, 2, 2},
                         {0x2400, 0x0e, 2, 3},
                         {0x2400, 0x0f, 2, 0x8000}};
const Field filters[] = {{0x2401, 0, 2, 50},   {0x2402, 0, 2, 40}, {0x2403, 0, 2, 100},
                         {0x2404, 0, 2, 4000}, {0x2405, 0, 2, 10}, {0x2406, 0, 2, 2},
                         {0x2407, 0, 2, 16}};
const Field command[] = {
    {0x60fe, 0, 1, 255}, {0x60fe, 1, 4, 0xffffffff}, {0x60fe, 2, 4, 0x80000000}};
const Field* const rows[] = {inputs, outputs, filters, command};
const unsigned counts[] = {9, 4, 7, 3};
const L::DriverGroup groups[] = {
    L::DriverGroup::IO_INPUT_CONFIGURATION, L::DriverGroup::IO_OUTPUT_CONFIGURATION,
    L::DriverGroup::IO_INPUT_FILTERS, L::DriverGroup::IO_OUTPUT_COMMAND};
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
    for (unsigned i = 0; i < 4 && i < e.size; ++i)
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
    if (p.transfer.index == 0x2400)
        CHECK(p.transfer.subindex != 9 && p.transfer.subindex != 10 && p.transfer.subindex != 11 &&
              p.transfer.subindex < 16);
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
    for (unsigned g = 0; g < 4; ++g) {
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
        L::IoObservation o;
        CHECK(L::getIo(c, target(), 20, 30, o));
        CHECK(o.group == groups[g] && o.fieldCount == counts[g] && !o.outputMappingKnown &&
              !o.wiringKnown && !o.electricalLevelsKnown);
        CHECK(o.window.earliestUs == 0 && o.window.latestUs == counts[g] * 2 - 1 &&
              o.window.skewUs == counts[g] * 2 - 1);
        for (unsigned i = 0; i < counts[g]; ++i) {
            CHECK(o.fields[i].index == rows[g][i].index &&
                  o.fields[i].subindex == rows[g][i].subindex);
            CHECK(o.fields[i].earliestUs == i * 2 && o.fields[i].latestUs == i * 2 + 1 &&
                  o.fields[i].source == L::ReadSource::SDO && !o.fields[i].captureKnown);
        }
        if (g == 0) {
            CHECK(o.ioEntries == 0xffff && o.inputPolarityRaw == 0xffa5);
            for (unsigned i = 0; i < 7; ++i) {
                CHECK(o.inputFunctionRaw[i] == inputs[i + 2].raw);
                CHECK(o.inputInverted[i] == bool(0xa5 & (1u << i)));
            }
            CHECK(o.inputFunctions[0] == L::InputFunction::UNDEFINED &&
                  o.inputFunctions[1] == L::InputFunction::ORIGIN &&
                  o.inputFunctions[6] == L::InputFunction::UNKNOWN);
        } else if (g == 1) {
            CHECK(o.outputConfig0CRaw == 0xffff && o.outputConfig0DRaw == 2 &&
                  o.outputConfig0ERaw == 3 && o.outputConfig0FRaw == 0x8000);
        } else if (g == 2) {
            for (unsigned i = 0; i < 7; ++i)
                CHECK(o.inputFilterRaw[i] == filters[i].raw);
        } else {
            CHECK(o.outputEntries == 255 && o.outputValueRaw == UINT32_MAX &&
                  o.outputMaskRaw == 0x80000000);
        }
        // The old four-field publication type must reject nine-field IO contexts.
        L::DriverObservation wrong;
        wrong.directionRaw = 0x5aa5;
        const Saved<L::DriverObservation> old(wrong);
        CHECK(!L::getDriver(c, target(), 20, 100, wrong));
        old.unchanged(wrong);
        L::PreparedRead done;
        done.ready = true;
        done.transfer.index = 0xbeef;
        CHECK(L::nextDriver(c, 20, done) && !done.ready && done.transfer.index == 0xbeef);
    }
    CHECK(seen == 23);
}
void admissionAndPublication() {
    for (unsigned g = 0; g < 4; ++g) {
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
        L::IoObservation o;
        o.inputPolarityRaw = 0xa55a;
        const Saved<L::IoObservation> prior(o);
        for (unsigned i = 0; i < counts[g]; ++i) {
            CHECK(!L::getIo(c, target(), i * 2, 100, o));
            prior.unchanged(o);
            accept(c, rows[g][i].raw, i * 2);
        }
        CHECK(L::getIo(c, target(), 20, 20, o));
        const Saved<L::IoObservation> complete(o);
        CHECK(!L::getIo(c, target(), 20, 19, o));
        complete.unchanged(o);
        CHECK(!L::getIo(c, target(), 20, 0, o));
        complete.unchanged(o);
        o.target = target();
        const Saved<L::IoObservation> alias(o);
        CHECK(!L::getIo(c, o.target, 20, 100, o));
        alias.unchanged(o);
        CHECK(!L::getIo(c, target(), c.read.servicedUs - 1, 100, o));
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
            CHECK(!L::getIo(c, t, 20, 100, o));
            complete.unchanged(o);
        }
        const Saved<L::DriverContext> retained(c);
        CHECK(!L::advanceDriver(c, M::ReadOutcome::CANCELLED, 20));
        retained.unchanged(c);
    }
    L::DriverContext wrong;
    CHECK(L::prepareIdentity(wrong.read, target(), 19, 0, 1000, options()));
    L::PreparedRead p;
    p.ready = true;
    const Saved<L::PreparedRead> old(p);
    CHECK(!L::nextDriver(wrong, 0, p));
    old.unchanged(p);
    L::IoObservation o;
    const Saved<L::IoObservation> prior(o);
    CHECK(!L::getIo(wrong, target(), 0, 100, o));
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
    CHECK(!L::getIo(wrong, target(), 0, 100, o));
    prior.unchanged(o);
}
void terminalCorrelation() {
    for (unsigned g = 0; g < 4; ++g) {
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
    for (unsigned g = 0; g < 4; ++g)
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
                L::IoObservation o;
                o.ioEntries = 0xa55a;
                const Saved<L::IoObservation> prior(o);
                CHECK(!L::getIo(c, target(), 20, 100, o));
                prior.unchanged(o);
            }
    for (unsigned g = 0; g < 4; ++g)
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
    for (uint32_t value : {0u, 1u, 13u, 14u, 255u, 0x8000u, 0xffffu}) {
        for (unsigned g = 0; g < 4; ++g) {
            L::DriverContext c = prepared(g);
            for (unsigned i = 0; i < counts[g]; ++i)
                accept(c, value, 0);
            L::IoObservation o;
            CHECK(L::getIo(c, target(), 0, 1, o));
            CHECK(o.fieldCount == counts[g] && o.window.skewUs == 0);
            if (g == 0) {
                CHECK(o.ioEntries == value && o.inputPolarityRaw == value);
                for (unsigned i = 0; i < 7; ++i) {
                    CHECK(o.inputFunctionRaw[i] == value);
                    CHECK(o.inputFunctions[i] == (value <= 13 ? static_cast<L::InputFunction>(value)
                                                              : L::InputFunction::UNKNOWN));
                    CHECK(o.inputInverted[i] == bool(value & (1u << i)));
                }
            } else if (g == 1) {
                CHECK(o.outputConfig0CRaw == value && o.outputConfig0FRaw == value);
            } else if (g == 2) {
                for (unsigned i = 0; i < 7; ++i)
                    CHECK(o.inputFilterRaw[i] == value);
            } else {
                CHECK(o.outputEntries == (value & 255u) && o.outputValueRaw == value &&
                      o.outputMaskRaw == value);
            }
        }
    }
    const L::InputFunction decoded[] = {
        L::InputFunction::UNDEFINED,      L::InputFunction::ORIGIN,
        L::InputFunction::POSITIVE_LIMIT, L::InputFunction::NEGATIVE_LIMIT,
        L::InputFunction::ENABLE_MF,      L::InputFunction::STOP,
        L::InputFunction::EMERGENCY_STOP, L::InputFunction::USER0,
        L::InputFunction::USER1,          L::InputFunction::USER2,
        L::InputFunction::USER3,          L::InputFunction::USER4,
        L::InputFunction::USER5,          L::InputFunction::USER6};
    for (uint32_t code = 0; code <= 13; ++code) {
        L::DriverContext c = prepared(0);
        accept(c, 0, 0);
        accept(c, 0, 0);
        for (unsigned i = 0; i < 7; ++i)
            accept(c, code, 0);
        L::IoObservation o;
        CHECK(L::getIo(c, target(), 0, 1, o));
        for (unsigned i = 0; i < 7; ++i)
            CHECK(o.inputFunctions[i] == decoded[code]);
    }
    for (uint32_t mask : {0u, 1u, 0x40u, 0x7fu, 0x80u, 0xff80u, 0xffffu}) {
        L::DriverContext c = prepared(0);
        accept(c, 0, 0);
        accept(c, mask, 0);
        for (unsigned i = 0; i < 7; ++i)
            accept(c, 0, 0);
        L::IoObservation o;
        CHECK(L::getIo(c, target(), 0, 1, o) && o.inputPolarityRaw == mask);
        for (unsigned i = 0; i < 7; ++i)
            CHECK(o.inputInverted[i] == bool(mask & (1u << i)));
    }
    for (uint32_t value : {0u, 0x80000000u, 0xffffffffu}) {
        L::DriverContext c = prepared(3);
        accept(c, 0, 0);
        accept(c, value, 0);
        accept(c, value, 0);
        L::IoObservation o;
        CHECK(L::getIo(c, target(), 0, 1, o));
        CHECK(o.outputValueRaw == value && o.outputMaskRaw == value);
    }
    for (unsigned g = 0; g < 4; ++g)
        for (unsigned late = 0; late < 2; ++late) {
            L::DriverContext c = prepared(g, 0, 100);
            for (unsigned i = 0; i + 1 < counts[g]; ++i)
                accept(c, rows[g][i].raw, i * 2);
            C::SdoCompletion e = success(c, rows[g][counts[g] - 1].raw, 20, late ? 100 : 99);
            CHECK(L::advanceDriver(c, e, 100));
            CHECK(c.read.outcome == M::ReadOutcome::DEADLINE && c.read.evidencePresent &&
                  !c.read.values[counts[g] - 1].valid && c.read.values[0].valid);
        }
    for (M::ReadOutcome cause :
         {M::ReadOutcome::CANCELLED, M::ReadOutcome::INVALIDATED, M::ReadOutcome::DEADLINE}) {
        L::DriverContext c = prepared(0, 0, 100);
        accept(c, 7, 0);
        CHECK(L::advanceDriver(c, cause, 100));
        CHECK(c.read.outcome == M::ReadOutcome::DEADLINE && c.read.uncertain &&
              c.read.values[0].raw == 7);
        CHECK(c.read.invalidated == (cause == M::ReadOutcome::INVALIDATED));
        L::IoObservation o;
        const Saved<L::IoObservation> prior(o);
        CHECK(!L::getIo(c, target(), 100, 1000, o));
        prior.unchanged(o);
    }
    L::DriverContext latest = prepared(0, UINT64_MAX - 1, UINT64_MAX);
    for (unsigned i = 0; i < 9; ++i)
        accept(latest, 0, UINT64_MAX - 1);
    L::IoObservation o;
    CHECK(L::getIo(latest, target(), UINT64_MAX, 1, o));
    L::DriverContext wrong;
    CHECK(L::prepareDriverRead(wrong, target(), 19, 0, 1000, options(), L::DriverGroup::DRIVER));
    accept(wrong, 0, 0);
    accept(wrong, 400, 0);
    const Saved<L::IoObservation> prior(o);
    CHECK(!L::getIo(wrong, target(), 0, 100, o));
    prior.unchanged(o);
}

// Application-only prerequisite example. External evidence is a fixture premise,
// not inferred from assignment, raw levels or a successful protocol read.
bool fixtureOriginPermits(const L::IoObservation& io, bool methodNeedsOrigin,
                          bool externalSignalKnown, bool externalSignalActive) {
    if (!methodNeedsOrigin)
        return true;
    return io.group == L::DriverGroup::IO_INPUT_CONFIGURATION &&
           io.inputFunctions[0] == L::InputFunction::ORIGIN && externalSignalKnown &&
           externalSignalActive;
}
void separateAssignmentValueAndLevel() {
    L::DriverContext configuration = prepared(0);
    for (unsigned i = 0; i < 9; ++i)
        accept(configuration, 0, 0);
    L::IoObservation unused;
    CHECK(L::getIo(configuration, target(), 0, 100, unused));
    CHECK(unused.inputFunctions[0] == L::InputFunction::UNDEFINED);
    CHECK(fixtureOriginPermits(unused, false, false, false));
    CHECK(!fixtureOriginPermits(unused, true, true, true));
    configuration = prepared(0);
    for (unsigned i = 0; i < 9; ++i)
        accept(configuration, i == 2 ? 1 : 0, 0);
    L::IoObservation origin;
    CHECK(L::getIo(configuration, target(), 0, 100, origin));
    CHECK(!fixtureOriginPermits(origin, true, false, true));
    CHECK(!fixtureOriginPermits(origin, true, true, false));
    CHECK(fixtureOriginPermits(origin, true, true, true));
    CHECK(!origin.wiringKnown && !origin.electricalLevelsKnown);

    L::DriverContext outputConfiguration = prepared(1), outputCommand = prepared(3);
    for (unsigned i = 0; i < 4; ++i)
        accept(outputConfiguration, 3, 0);
    accept(outputCommand, 255, 0);
    accept(outputCommand, 0x10000, 0);
    accept(outputCommand, 0x70000, 0);
    L::IoObservation assigned, commanded;
    CHECK(L::getIo(outputConfiguration, target(), 0, 100, assigned));
    CHECK(L::getIo(outputCommand, target(), 0, 100, commanded));
    L::DriverContext monitors;
    CHECK(L::prepareState(monitors.read, target(), 19, 0, 1000, options(),
                          uint8_t(1u << unsigned(L::StateBlock::MONITORS))));
    for (unsigned i = 0; i < 3; ++i) {
        L::PreparedRead work;
        CHECK(L::nextRead(monitors.read, 0, work));
        C::SdoCompletion terminal;
        terminal.transfer = work.transfer;
        terminal.nodeGeneration = 11;
        terminal.outcome = C::SdoOutcome::SUCCESS;
        terminal.queued = terminal.transmissionPossible = terminal.protocolAcknowledged = true;
        terminal.effectPossible = true;
        terminal.size = work.transfer.size;
        // Logical output monitor zero deliberately differs from nonzero command.
        CHECK(L::advanceRead(monitors.read, terminal, 0));
    }
    L::StateObservation logical;
    CHECK(L::getStateBlock(monitors.read, L::StateBlock::MONITORS, target(), 0, 100, logical));
    CHECK(assigned.outputConfig0DRaw == 3 && commanded.outputValueRaw == 0x10000 &&
          logical.logicalOutputs == 0);
    CHECK(!assigned.outputMappingKnown && !commanded.electricalLevelsKnown && !logical.interpreted);
}

} // namespace
int main() {
    realOwnerGroups();
    admissionAndPublication();
    terminalCorrelation();
    partialFailures();
    rawDomainsAndTime();
    separateAssignmentValueAndLevel();
    std::printf("io settings: %u checks passed\n", checks);
}
