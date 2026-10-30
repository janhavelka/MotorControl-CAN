// SPDX-License-Identifier: MIT
// Literal native fields come from the retained EDS; host times/provenance are
// synthetic fixtures. These exchanges qualify software, never a physical drive.
#include "SdoClient.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    o.queueTimeoutUs = 100;
    o.responseTimeoutUs = 100;
    return o;
}
uint8_t mask(L::StateBlock b) {
    return uint8_t(1u << static_cast<unsigned>(b));
}
C::NetworkRoute network() {
    C::NetworkRoute n;
    n.binding = target().channel.binding;
    n.generation = target().nodeGeneration;
    n.heartbeatId = 0x705;
    n.emcyId = 0x85;
    return n;
}
M::Frame frame(uint32_t id, uint8_t size) {
    M::Frame f;
    f.id = id;
    f.dlc = f.payloadLength = size;
    return f;
}
void configure(C::SdoClient& owner) {
    CHECK(owner.configure(0, target().channel, true, true));
    CHECK(owner.configureNetwork(0, network(), C::NetworkObserver(), 0));
}
L::PreparedRead next(const L::ReadContext& c, uint64_t now) {
    L::PreparedRead p;
    CHECK(L::nextRead(c, now, p));
    CHECK(p.ready);
    return p;
}
C::SdoCompletion success(const L::ReadContext& c, uint32_t raw, uint64_t admitted,
                         uint64_t completed) {
    C::SdoCompletion e;
    e.transfer = next(c, admitted).transfer;
    e.nodeGeneration = c.target.nodeGeneration;
    e.admittedUs = admitted;
    e.completedUs = completed;
    e.outcome = C::SdoOutcome::SUCCESS;
    e.queued = e.transmissionPossible = e.protocolAcknowledged = true;
    e.effectPossible = true;
    e.size = e.transfer.size;
    for (unsigned i = 0; i < e.size; ++i)
        e.data[i] = uint8_t(raw >> (8u * i));
    return e;
}
void accept(L::ReadContext& c, uint32_t raw, uint64_t when) {
    const C::SdoCompletion e = success(c, raw, when, when);
    CHECK(L::advanceRead(c, e, when));
}
struct Field {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t raw;
};
// This fixture independently specifies the operation's entire wire selection.
const Field identity[] = {
    {0x1000, 0, 4, 0x12340192}, {0x1009, 0, 2, 0x2608},     {0x100a, 0, 2, 0x8001},
    {0x1018, 0, 1, 4},          {0x1018, 1, 4, 0x80001000}, {0x1018, 2, 4, 0xffffffff},
    {0x1018, 3, 4, 0x76543210}, {0x2000, 0, 2, 1},          {0x2001, 0, 2, 0xabcd}};
const Field config[] = {{0x1005, 0, 4, 0x40000080}, {0x1014, 0, 4, 0x85},  {0x1017, 0, 2, 0},
                        {0x1200, 0, 1, 2},          {0x1200, 1, 4, 0x625}, {0x1200, 2, 4, 0x5a5}};
const Field states[] = {
    {0x1001, 0, 1, 0x80},       {0x603f, 0, 2, 0xffff},     {0x1003, 0, 1, 4},
    {0x1003, 1, 4, 0xffffffff}, {0x1003, 2, 4, 0x80000000}, {0x1003, 3, 4, 0x12345678},
    {0x1003, 4, 4, 0},          {0x2100, 0, 2, 0xffff},     {0x2101, 0, 2, 0x8080},
    {0x2102, 0, 2, 0xf008},     {0x6041, 0, 2, 0xffff},     {0x6061, 0, 1, 0xff},
    {0x6064, 0, 4, 0x80000000}, {0x606c, 0, 4, 0xffffffff}, {0x60fd, 0, 4, 0x87654321}};
void expected(const C::SdoTransfer& w, const Field& f) {
    CHECK(w.index == f.index && w.subindex == f.subindex && w.size == f.width);
    CHECK(w.direction == C::SdoDirection::UPLOAD && w.readEffect == C::SdoReadEffect::UNQUALIFIED);
    CHECK(w.deadlineUs == 10000 && w.queueTimeoutUs == 100 && w.responseTimeoutUs == 100);
    CHECK(M::sameBinding(w.channel.binding, target().channel.binding));
    for (unsigned i = 0; i < sizeof w.data; ++i)
        CHECK(w.data[i] == 0);
}
void runOwner(C::SdoClient& owner, L::ReadContext& c, const Field* fields, unsigned count,
              uint64_t& now) {
    for (unsigned i = 0; i < count; ++i) {
        L::PreparedRead p = next(c, now);
        expected(p.transfer, fields[i]);
        const L::PreparedRead repeat = next(c, now);
        CHECK(repeat.transfer.operation == p.transfer.operation &&
              repeat.transfer.step == p.transfer.step);
        C::SdoRequestId id;
        CHECK(owner.admit(0, p.transfer, now, id));
        CHECK(owner.service(now));
        M::Frame tx;
        C::SdoTxId txId;
        CHECK(owner.nextTx(0, now, tx, txId));
        CHECK(tx.id == 0x625 && tx.data[0] == 0x40 && tx.data[1] == uint8_t(fields[i].index));
        CHECK(tx.data[2] == uint8_t(fields[i].index >> 8) && tx.data[3] == fields[i].subindex);
        for (unsigned j = 4; j < 8; ++j)
            CHECK(tx.data[j] == 0);
        CHECK(owner.canTransmit(txId, now));
        CHECK(owner.confirmTx(txId, now));
        M::Frame reply = frame(0x5a5, 8);
        reply.data[0] = fields[i].width == 1 ? 0x4f : fields[i].width == 2 ? 0x4b : 0x43;
        reply.data[1] = uint8_t(fields[i].index);
        reply.data[2] = uint8_t(fields[i].index >> 8);
        reply.data[3] = fields[i].subindex;
        for (unsigned j = 0; j < 4 && j < fields[i].width; ++j)
            reply.data[4 + j] = uint8_t(fields[i].raw >> (8 * j));
        ++now;
        if (i == 0) {
            const Saved<L::ReadContext> pending(c);
            M::Frame wrong = reply;
            wrong.data[1] ^= 1;
            CHECK(!owner.receive(wrong, now, now));
            CHECK(!owner.result(id));
            pending.unchanged(c);
            wrong = reply;
            ++wrong.data[3];
            CHECK(!owner.receive(wrong, now, now));
            CHECK(!owner.result(id));
            pending.unchanged(c);
            wrong = reply;
            ++wrong.id;
            CHECK(!owner.receive(wrong, now, now));
            CHECK(!owner.result(id));
            pending.unchanged(c);
        }
        CHECK(owner.receive(reply, now, now));
        const C::SdoCompletion* e = owner.result(id);
        CHECK(e && e->outcome == C::SdoOutcome::SUCCESS);
        CHECK(e->effectPossible && !e->effectUncertain && e->nodeGeneration == 11);
        C::SdoCompletion truncated = *e;
        --truncated.size;
        const Saved<L::ReadContext> before(c);
        CHECK(!L::advanceRead(c, truncated, now));
        before.unchanged(c);
        CHECK(L::advanceRead(c, *e, now));
        CHECK(owner.release(id));
        reply.data[4] =
            0; // Context retains bytes even after the actual owner releases its terminal.
        CHECK(c.evidence.data[0] == uint8_t(fields[i].raw));
        ++now;
    }
    CHECK(c.state == M::ReadState::SUCCEEDED && c.outcome == M::ReadOutcome::SUCCESS);
    L::PreparedRead terminal;
    terminal.transfer.index = 0xbeef;
    CHECK(L::nextRead(c, now, terminal) && !terminal.ready && terminal.transfer.index == 0xbeef);
}
void allRowsAndRealOwner() {
    const Field* groups[] = {identity, config, states};
    const unsigned counts[] = {9, 6, 15};
    unsigned seen = 0;
    for (unsigned group = 0; group < 3; ++group)
        for (unsigned i = 0; i < counts[group]; ++i) {
            const Field& f = groups[group][i];
            L::ReadDescriptor d = {};
            CHECK(L::readDescriptor(f.index, f.subindex, d));
            CHECK(d.widthBytes == f.width);
            const L::ReadRepresentation rep = f.index == 0x6061 ? L::ReadRepresentation::RAW
                                              : f.index == 0x6064 || f.index == 0x606c
                                                  ? L::ReadRepresentation::SIGNED
                                                  : L::ReadRepresentation::UNSIGNED;
            CHECK(d.representation == rep && d.readEffect == L::ReadEffect::UNQUALIFIED);
            CHECK(d.scope == L::SourceScope::EDS_FAMILY_UNBOUND);
            ++seen;
        }
    CHECK(seen == 30);
    const Field excluded[] = {{0x6040, 0, 2, 0}, {0x6060, 0, 1, 0}, {0x1018, 4, 4, 0},
                              {0x1003, 5, 4, 0}, {0x2300, 0, 2, 0}, {0x1400, 0, 1, 0},
                              {0x1600, 0, 1, 0}, {0x2203, 0, 2, 0}, {0x2521, 0, 2, 0},
                              {0x60f4, 0, 4, 0}};
    L::ReadDescriptor d = {};
    d.index = 0xa55a;
    const Saved<L::ReadDescriptor> saved(d);
    for (const Field& f : excluded) {
        CHECK(!L::readDescriptor(f.index, f.subindex, d));
        saved.unchanged(d);
    }
    C::SdoClient owner;
    configure(owner);
    uint64_t now = 0;
    L::ReadContext c;
    CHECK(L::prepareIdentity(c, target(), 1, now, 10000, options()));
    runOwner(owner, c, identity, 9, now);
    L::IdentityObservation id;
    CHECK(L::getIdentity(c, target(), now, 1000, id));
    CHECK(id.deviceType == 0x12340192 && id.hardwareVersion == 0x2608 &&
          id.softwareVersion == 0x8001);
    CHECK(id.identityEntries == 4 && id.vendorId == 0x80001000 && id.productCode == 0xffffffff);
    CHECK(id.revision == 0x76543210 && id.modelCode == 1 && id.firmwareCode == 0xabcd &&
          !id.modelMatched);
    CHECK(id.fields[0].earliestUs == 0 && !id.fields[0].captureKnown &&
          id.fields[0].mappingGeneration == 0);
    CHECK(id.window.earliestUs == 0 && id.window.latestUs == 17 && id.window.skewUs == 17);
    CHECK(L::prepareConfig(c, target(), 2, now, 10000, options()));
    runOwner(owner, c, config, 6, now);
    L::ConfigObservation cfg;
    CHECK(L::getConfig(c, target(), now, 1000, cfg));
    CHECK(cfg.syncCobId == 0x40000080 && cfg.emcyCobId == 0x85 && cfg.producerTimeMs == 0);
    CHECK(cfg.sdoEntries == 2 && cfg.clientToServer == 0x625 && cfg.serverToClient == 0x5a5);
    CHECK(L::prepareState(c, target(), 3, now, 10000, options(), 0x3f));
    runOwner(owner, c, states, 15, now);
    L::StateObservation s;
    CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), now, 1000, s));
    CHECK(s.errorRegister == 0x80 && s.errorCode == 0xffff);
    CHECK(L::getStateBlock(c, L::StateBlock::ERROR_HISTORY, target(), now, 1000, s));
    CHECK(s.errorCount == 4 && s.errorHistory[0] == 0xffffffff && s.errorHistory[1] == 0x80000000 &&
          s.fieldCount == 5);
    CHECK(L::getStateBlock(c, L::StateBlock::MONITORS, target(), now, 1000, s));
    CHECK(s.monitor == 0xffff && s.logicalInputs == 0x8080 && s.logicalOutputs == 0xf008 &&
          !s.interpreted);
    CHECK(L::getStateBlock(c, L::StateBlock::STATUS, target(), now, 1000, s));
    CHECK(s.statusword == 0xffff && s.modeRaw == 0xff && !s.interpreted);
    CHECK(L::getStateBlock(c, L::StateBlock::FEEDBACK, target(), now, 1000, s));
    CHECK(s.actualPosition == INT32_MIN && s.actualVelocity == -1 && !s.interpreted);
    CHECK(L::getStateBlock(c, L::StateBlock::INPUTS, target(), now, 1000, s));
    CHECK(s.digitalInputs == 0x87654321);
}
void preparationAndGetters() {
    L::ReadContext c;
    CHECK(L::prepareIdentity(c, target(), 8, 0, 10000, options()));
    const Saved<L::ReadContext> before(c);
    for (unsigned i = 0; i < 11; ++i) {
        L::ReadTarget t = target();
        L::ReadOptions o = options();
        uint32_t operation = 8;
        uint64_t deadline = 10000;
        switch (i) {
        case 0:
            t.nodeGeneration = 0;
            break;
        case 1:
            t.channel.binding.bindingGeneration = 0;
            break;
        case 2:
            t.channel.binding.configurationGeneration = 0;
            break;
        case 3:
            t.channel.binding.endpoint.protocol = M::Protocol::PROPRIETARY;
            break;
        case 4:
            t.channel.binding.endpoint.address = 0;
            break;
        case 5:
            t.channel.binding.endpoint.address = 128;
            break;
        case 6:
            t.channel.clientToServerId = t.channel.serverToClientId;
            break;
        case 7:
            operation = 0;
            break;
        case 8:
            deadline = 0;
            break;
        case 9:
            o.queueTimeoutUs = 0;
            break;
        case 10:
            o.responseTimeoutUs = 0;
            break;
        }
        CHECK(!L::prepareIdentity(c, t, operation, 0, deadline, o));
        before.unchanged(c);
        CHECK(!L::prepareConfig(c, t, operation, 0, deadline, o));
        before.unchanged(c);
        CHECK(!L::prepareState(c, t, operation, 0, deadline, o, 1));
        before.unchanged(c);
    }
    CHECK(!L::prepareState(c, target(), 8, 0, 10000, options(), 0));
    before.unchanged(c);
    CHECK(!L::prepareState(c, target(), 8, 0, 10000, options(), 0x80));
    before.unchanged(c);
    const M::Status probe = L::prepareProbe(c, target(), 8, 0, 10000, options());
    CHECK(probe.code == M::Err::UNSUPPORTED &&
          probe.detail == static_cast<int32_t>(M::ReadError::READ_EFFECT));
    before.unchanged(c);
    L::IdentityObservation id;
    id.modelCode = 0xa55a;
    const Saved<L::IdentityObservation> prior(id);
    CHECK(!L::getIdentity(c, target(), 0, 100, id));
    prior.unchanged(id);
    for (unsigned i = 0; i < 9; ++i)
        accept(c, identity[i].raw, i == 0 ? 0 : 100 + i);
    CHECK(L::getIdentity(c, target(), 108, 108, id));
    CHECK(id.window.skewUs == 108);
    const Saved<L::IdentityObservation> current(id);
    CHECK(!L::getIdentity(c, target(), 108, 107, id));
    current.unchanged(id);
    CHECK(!L::getIdentity(c, target(), 107, 1000, id));
    current.unchanged(id);
    CHECK(!L::getIdentity(c, target(), 108, 0, id));
    current.unchanged(id);
    for (unsigned i = 0; i < 13; ++i) {
        L::ReadTarget t = target();
        switch (i) {
        case 0:
            ++t.channel.binding.logicalAxisId;
            break;
        case 1:
            ++t.channel.binding.endpoint.bus;
            break;
        case 2:
            ++t.channel.binding.endpoint.profile;
            break;
        case 3:
            ++t.channel.binding.endpoint.address;
            break;
        case 4:
            ++t.channel.binding.endpoint.channel;
            break;
        case 5:
            ++t.channel.binding.endpoint.subaxis;
            break;
        case 6:
            ++t.channel.binding.bindingGeneration;
            break;
        case 7:
            ++t.channel.binding.configurationGeneration;
            break;
        case 8:
            ++t.nodeGeneration;
            break;
        case 9:
            ++t.channel.clientToServerId;
            break;
        case 10:
            ++t.channel.serverToClientId;
            break;
        case 11:
            t.channel.binding.endpoint.protocol = M::Protocol::PROPRIETARY;
            break;
        case 12:
            t.nodeGeneration = 0;
            break;
        }
        CHECK(!L::getIdentity(c, t, 108, 1000, id));
        current.unchanged(id);
    }
    L::ConfigObservation cfg;
    cfg.syncCobId = 0xa55a;
    const Saved<L::ConfigObservation> oldCfg(cfg);
    CHECK(!L::getConfig(c, target(), 108, 1000, cfg));
    oldCfg.unchanged(cfg);
    L::StateObservation s;
    s.monitor = 0xa55a;
    const Saved<L::StateObservation> oldState(s);
    CHECK(!L::getStateBlock(c, L::StateBlock::MONITORS, target(), 108, 1000, s));
    oldState.unchanged(s);
    L::ReadContext empty;
    L::PreparedRead p;
    p.ready = true;
    const Saved<L::PreparedRead> oldWork(p);
    CHECK(!L::nextRead(empty, 0, p));
    oldWork.unchanged(p);
    CHECK(!L::prepareIdentity(c, c.target, 9, 200, 10000, options())); // No aliased preparation.
    const Saved<L::ReadContext> terminal(c);
    CHECK(!L::advanceRead(c, M::ReadOutcome::CANCELLED, 200));
    terminal.unchanged(c);
}
void wrongServiceAndTime() {
    L::ReadContext c;
    CHECK(L::prepareIdentity(c, target(), 9, 10, 10000, options()));
    C::SdoCompletion good = success(c, 0x12340192, 11, 12);
    for (unsigned i = 0; i < 37; ++i) {
        C::SdoCompletion e = good;
        switch (i) {
        case 0:
            ++e.transfer.channel.binding.logicalAxisId;
            break;
        case 1:
            ++e.transfer.channel.binding.endpoint.bus;
            break;
        case 2:
            ++e.transfer.channel.binding.endpoint.profile;
            break;
        case 3:
            ++e.transfer.channel.binding.endpoint.address;
            break;
        case 4:
            ++e.transfer.channel.binding.endpoint.channel;
            break;
        case 5:
            ++e.transfer.channel.binding.endpoint.subaxis;
            break;
        case 6:
            ++e.transfer.channel.binding.bindingGeneration;
            break;
        case 7:
            ++e.transfer.channel.binding.configurationGeneration;
            break;
        case 8:
            ++e.transfer.channel.clientToServerId;
            break;
        case 9:
            ++e.transfer.channel.serverToClientId;
            break;
        case 10:
            ++e.transfer.operation;
            break;
        case 11:
            ++e.transfer.request;
            break;
        case 12:
            ++e.transfer.step;
            break;
        case 13:
            ++e.transfer.index;
            break;
        case 14:
            ++e.transfer.subindex;
            break;
        case 15:
            e.transfer.direction = C::SdoDirection::DOWNLOAD;
            break;
        case 16:
            --e.transfer.size;
            break;
        case 17:
            e.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
            break;
        case 18:
            e.transfer.allowSegmentedUpload = false;
            break;
        case 19:
            ++e.transfer.deadlineUs;
            break;
        case 20:
            ++e.transfer.queueTimeoutUs;
            break;
        case 21:
            ++e.transfer.responseTimeoutUs;
            break;
        case 22:
            ++e.nodeGeneration;
            break;
        case 23:
            e.admittedUs = 9;
            break;
        case 24:
            e.admittedUs = 13;
            break;
        case 25:
            e.completedUs = 14;
            break;
        case 26:
            e.protocolAcknowledged = false;
            break;
        case 27:
            e.queued = false;
            break;
        case 28:
            e.transmissionPossible = false;
            break;
        case 29:
            e.effectPossible = false;
            break;
        case 30:
            e.effectUncertain = true;
            break;
        case 31:
            e.abortValid = true;
            break;
        case 32:
            --e.size;
            break;
        case 33:
            e.outcome = static_cast<C::SdoOutcome>(255);
            break;
        case 34:
            e.outcome = C::SdoOutcome::SERVER_ABORT;
            break;
        case 35:
            e.outcome = C::SdoOutcome::TIMEOUT;
            break;
        case 36:
            e.nodeGeneration = 0;
            break;
        }
        const Saved<L::ReadContext> before(c);
        CHECK(!L::advanceRead(c, e, 13));
        before.unchanged(c);
    }
    const Saved<L::ReadContext> before(c);
    CHECK(!L::advanceRead(c, good, 9));
    before.unchanged(c);
    for (unsigned i = 0; i < 2; ++i) {
        C::SdoCompletion impossible = good;
        impossible.outcome = i ? C::SdoOutcome::SERVER_ABORT : C::SdoOutcome::TIMEOUT;
        impossible.size = 0;
        impossible.protocolAcknowledged = false;
        impossible.queued = impossible.transmissionPossible = impossible.effectPossible = false;
        impossible.transmissionConfirmed = !i;
        impossible.abortValid = i != 0;
        CHECK(!L::advanceRead(c, impossible, 13));
        before.unchanged(c);
    }
    CHECK(L::advanceRead(c, good, 13));
    good.data[0] = 0;
    CHECK(c.values[0].raw == 0x12340192);
    // Same-length previous step cannot settle the next operation slot.
    CHECK(!L::advanceRead(c, good, 14));
    L::PreparedRead p;
    const Saved<L::PreparedRead> old(p);
    CHECK(!L::nextRead(c, 12, p));
    old.unchanged(p);
    CHECK(!L::nextRead(c, 10000, p));
    old.unchanged(p);
    for (unsigned late = 0; late < 2; ++late) {
        L::ReadContext d;
        CHECK(L::prepareIdentity(d, target(), 10, 0, 100, options()));
        C::SdoCompletion e = success(d, 1, 0, late ? 100 : 99);
        CHECK(L::advanceRead(d, e, 100));
        CHECK(d.outcome == M::ReadOutcome::DEADLINE && !d.values[0].valid);
    }
    L::ReadContext zero;
    CHECK(L::prepareState(zero, target(), 1, 0, 1, options(), mask(L::StateBlock::INPUTS)));
    accept(zero, 0xffffffff, 0);
    L::StateObservation s;
    CHECK(L::getStateBlock(zero, L::StateBlock::INPUTS, target(), 0, 1, s));
    CHECK(s.fields[0].earliestUs == 0);
    CHECK(L::getStateBlock(zero, L::StateBlock::INPUTS, target(), UINT64_MAX, UINT64_MAX, s));
    L::ReadContext end;
    CHECK(L::prepareState(end, target(), 1, UINT64_MAX - 1, UINT64_MAX, options(),
                          mask(L::StateBlock::INPUTS)));
    accept(end, 1, UINT64_MAX - 1);
    CHECK(L::getStateBlock(end, L::StateBlock::INPUTS, target(), UINT64_MAX, 1, s));
}
void partialFailuresAndHistory() {
    const uint32_t aborts[] = {0, 0x80000000, 0xffffffff};
    for (uint32_t abort : aborts) {
        L::ReadContext c;
        CHECK(L::prepareState(c, target(), 1, 0, 10000, options(),
                              uint8_t(mask(L::StateBlock::ERRORS) | mask(L::StateBlock::STATUS))));
        accept(c, 0x80, 0);
        accept(c, 0x1234, 1);
        C::SdoCompletion e = success(c, 0, 2, 3);
        e.outcome = C::SdoOutcome::SERVER_ABORT;
        e.protocolAcknowledged = false;
        e.size = 0;
        e.abortValid = true;
        e.abortCode = abort;
        e.effectUncertain = true;
        CHECK(L::advanceRead(c, e, 3));
        CHECK(c.outcome == M::ReadOutcome::PROTOCOL_REJECTED && c.uncertain);
        CHECK(c.evidence.abortValid && c.evidence.abortCode == abort && c.status.detail == 0);
        L::StateObservation s;
        CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), 3, 100, s));
        CHECK(s.errorRegister == 0x80 && s.errorCode == 0x1234 && s.window.skewUs == 1);
        const Saved<L::StateObservation> good(s);
        CHECK(!L::getStateBlock(c, L::StateBlock::STATUS, target(), 3, 100, s));
        good.unchanged(s);
    }
    for (unsigned count = 0; count <= 5; ++count) {
        L::ReadContext c;
        CHECK(L::prepareState(c, target(), 1, 0, 10000, options(),
                              mask(L::StateBlock::ERROR_HISTORY)));
        accept(c, count, 0);
        for (unsigned i = 1; i <= count && i <= 4; ++i) {
            if (count > 4)
                break;
            CHECK(next(c, i).transfer.subindex == i);
            accept(c, 0x80000000u + i, i);
        }
        L::StateObservation s;
        s.errorCount = 0x7f;
        const Saved<L::StateObservation> old(s);
        if (count > 4) {
            CHECK(c.state == M::ReadState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR);
            CHECK(!L::getStateBlock(c, L::StateBlock::ERROR_HISTORY, target(), 5, 100, s));
            old.unchanged(s);
        } else {
            CHECK(c.state == M::ReadState::SUCCEEDED);
            CHECK(L::getStateBlock(c, L::StateBlock::ERROR_HISTORY, target(), 5, 100, s));
            CHECK(s.errorCount == count && s.fieldCount == count + 1);
            for (unsigned i = 0; i < count; ++i)
                CHECK(s.errorHistory[i] == 0x80000001u + i);
        }
    }
    // A failure at every config step must leave an earlier caller snapshot intact.
    for (unsigned fail = 0; fail < 6; ++fail) {
        L::ReadContext c;
        CHECK(L::prepareConfig(c, target(), 1, 0, 10000, options()));
        for (unsigned i = 0; i < fail; ++i)
            accept(c, config[i].raw, i);
        C::SdoCompletion e = success(c, 0, fail, fail);
        e.outcome = C::SdoOutcome::TRANSPORT_ERROR;
        e.protocolAcknowledged = false;
        e.size = 0;
        e.effectUncertain = true;
        CHECK(L::advanceRead(c, e, fail));
        L::ConfigObservation cfg;
        cfg.syncCobId = 0xabcdef01;
        const Saved<L::ConfigObservation> old(cfg);
        CHECK(!L::getConfig(c, target(), 6, 100, cfg));
        old.unchanged(cfg);
        CHECK(c.cursor == fail && c.outcome == M::ReadOutcome::TRANSPORT_ERROR && c.uncertain);
    }
    const M::ReadOutcome local[] = {M::ReadOutcome::CANCELLED, M::ReadOutcome::INVALIDATED,
                                    M::ReadOutcome::DEADLINE};
    for (M::ReadOutcome reason : local) {
        L::ReadContext c;
        CHECK(L::prepareIdentity(c, target(), 1, 0, 100, options()));
        CHECK(L::advanceRead(c, reason, reason == M::ReadOutcome::DEADLINE ? 100 : 0));
        CHECK(c.state == M::ReadState::FAILED && c.outcome == reason && c.uncertain);
    }
    // A completed block remains historical, but known lifecycle withdrawal must
    // reject current publication even when the caller supplies its old target.
    const C::SdoOutcome revoked[] = {C::SdoOutcome::REBOOT, C::SdoOutcome::BINDING_INVALIDATED,
                                     C::SdoOutcome::BUS_OFF};
    for (C::SdoOutcome reason : revoked) {
        L::ReadContext c;
        CHECK(L::prepareState(c, target(), 1, 0, 10000, options(),
                              uint8_t(mask(L::StateBlock::ERRORS) | mask(L::StateBlock::STATUS))));
        accept(c, 0x80, 0);
        accept(c, 0x1234, 1);
        L::StateObservation s;
        CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), 1, 100, s));
        const Saved<L::StateObservation> prior(s);
        C::SdoCompletion e = success(c, 0, 2, 3);
        e.outcome = reason;
        e.protocolAcknowledged = false;
        e.size = 0;
        e.effectUncertain = true;
        CHECK(L::advanceRead(c, e, 3));
        CHECK(c.values[0].valid && c.values[0].raw == 0x80);
        CHECK(!L::getStateBlock(c, L::StateBlock::ERRORS, target(), 3, 100, s));
        prior.unchanged(s);
    }
    L::ReadContext withdrawn;
    CHECK(L::prepareState(withdrawn, target(), 1, 0, 10000, options(),
                          uint8_t(mask(L::StateBlock::ERRORS) | mask(L::StateBlock::STATUS))));
    accept(withdrawn, 1, 0);
    accept(withdrawn, 2, 1);
    CHECK(L::advanceRead(withdrawn, M::ReadOutcome::INVALIDATED, 2));
    L::StateObservation s;
    const Saved<L::StateObservation> prior(s);
    CHECK(!L::getStateBlock(withdrawn, L::StateBlock::ERRORS, target(), 2, 100, s));
    prior.unchanged(s);
}
void cutoffDoesNotEraseInvalidation() {
    const C::SdoOutcome native[] = {C::SdoOutcome::REBOOT, C::SdoOutcome::BINDING_INVALIDATED,
                                    C::SdoOutcome::BUS_OFF, C::SdoOutcome::TIMEOUT};
    struct Timing {
        uint64_t completed, delivered;
    };
    // Also cover an on-time terminal that reaches this context at/after cutoff.
    const Timing times[] = {{99, 99}, {100, 100}, {101, 101}, {99, 100}, {99, 101}};
    for (C::SdoOutcome reason : native)
        for (const Timing& time : times) {
            L::ReadContext c;
            CHECK(L::prepareState(
                c, target(), 1, 0, 100, options(),
                uint8_t(mask(L::StateBlock::ERRORS) | mask(L::StateBlock::STATUS))));
            accept(c, 0x80, 0);
            accept(c, 0x1234, 1);
            L::StateObservation s;
            CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), 1, 1000, s));
            const Saved<L::StateObservation> prior(s);
            C::SdoCompletion e = success(c, 0, 2, time.completed);
            e.outcome = reason;
            e.protocolAcknowledged = false;
            e.size = 0;
            e.effectUncertain = true;
            CHECK(L::advanceRead(c, e, time.delivered));
            const M::ReadOutcome expected =
                time.delivered >= 100 || reason == C::SdoOutcome::TIMEOUT ? M::ReadOutcome::DEADLINE
                : reason == C::SdoOutcome::BUS_OFF ? M::ReadOutcome::TRANSPORT_ERROR
                                                   : M::ReadOutcome::INVALIDATED;
            CHECK(c.state == M::ReadState::FAILED && c.outcome == expected);
            CHECK(c.evidencePresent && c.evidence.outcome == reason && c.uncertain);
            CHECK(c.values[0].valid && c.values[0].raw == 0x80 && c.values[1].raw == 0x1234);
            const bool available =
                bool(L::getStateBlock(c, L::StateBlock::ERRORS, target(), time.delivered, 1000, s));
            CHECK(available == (reason == C::SdoOutcome::TIMEOUT));
            if (!available)
                prior.unchanged(s);
            else
                CHECK(s.errorRegister == 0x80 && s.errorCode == 0x1234 && s.window.latestUs == 1);
        }
    const M::ReadOutcome local[] = {M::ReadOutcome::INVALIDATED, M::ReadOutcome::DEADLINE,
                                    M::ReadOutcome::CANCELLED};
    for (M::ReadOutcome reason : local)
        for (uint64_t now = 99; now <= 101; ++now) {
            L::ReadContext c;
            CHECK(L::prepareState(
                c, target(), 1, 0, 100, options(),
                uint8_t(mask(L::StateBlock::ERRORS) | mask(L::StateBlock::STATUS))));
            accept(c, 0x80, 0);
            accept(c, 0x1234, 1);
            L::StateObservation s;
            CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), 1, 1000, s));
            const Saved<L::StateObservation> prior(s);
            if (reason == M::ReadOutcome::DEADLINE && now < 100) {
                const Saved<L::ReadContext> active(c);
                CHECK(!L::advanceRead(c, reason, now));
                active.unchanged(c);
                CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), now, 1000, s));
                continue;
            }
            CHECK(L::advanceRead(c, reason, now));
            CHECK(c.state == M::ReadState::FAILED &&
                  c.outcome == (now >= 100 ? M::ReadOutcome::DEADLINE : reason));
            CHECK(c.values[0].valid && c.values[0].raw == 0x80 && c.values[1].raw == 0x1234);
            const bool available =
                bool(L::getStateBlock(c, L::StateBlock::ERRORS, target(), now, 1000, s));
            CHECK(available == (reason != M::ReadOutcome::INVALIDATED));
            if (!available)
                prior.unchanged(s);
            else
                CHECK(s.errorRegister == 0x80 && s.errorCode == 0x1234 && s.window.latestUs == 1);
        }
}
C::PdoMapEvidence mapEvidence() {
    C::PdoMapEvidence e;
    e.route.binding = target().channel.binding;
    e.route.generation = 2;
    e.route.cobId = 0x285;
    C::PdoMap map;
    CHECK(L::pdoTemplate(C::PdoDirection::TPDO, 2, map));
    CHECK(C::validatePdoMap(map, L::pdoProfile(41), e.route.layout));
    e.nodeGeneration = target().nodeGeneration;
    e.source = C::PdoMapSource::REVIEWED_CONFIGURATION;
    e.communicationKnown = true;
    e.cobIdParameter = 0x285;
    e.transmissionType = 255;
    e.evidenceId = 22;
    return e;
}
void passiveProvenance() {
    C::SdoClient owner;
    configure(owner);
    C::PdoMapEvidence map = mapEvidence();
    CHECK(owner.configurePdo(0, 0, map, L::pdoProfile(41), C::PdoObserver(), 0));
    L::ReadContext c;
    CHECK(L::prepareState(c, target(), 1, 0, 1000, options(), mask(L::StateBlock::FEEDBACK),
                          &map.route));
    L::PreparedRead p;
    p.transfer.index = 0xbeef;
    CHECK(L::nextRead(c, 0, p) && !p.ready && p.transfer.index == 0xbeef);
    const Saved<L::ReadContext> empty(c);
    CHECK(!L::advanceRead(c, *owner.pdo(0), 0));
    empty.unchanged(c);
    C::PdoQualification q;
    q.route = map.route;
    q.nodeGeneration = 11;
    q.evidenceId = 33;
    q.basis = C::PdoSampleBasis::PRODUCER_AND_INGRESS_RETIRED;
    q.establishedUs = 1;
    CHECK(owner.qualifyPdo(0, q, 1));
    M::Frame bytes = frame(0x285, 8);
    bytes.data[3] = 0x80;
    bytes.data[4] = bytes.data[5] = bytes.data[6] = bytes.data[7] = 0xff;
    CHECK(owner.receive(bytes, 2, 3));
    C::PdoRecord valid = *owner.pdo(0);
    for (unsigned i = 0; i < 30; ++i) {
        C::PdoRecord bad = valid;
        switch (i) {
        case 0:
            bad.confirmed = false;
            break;
        case 1:
            bad.qualified = false;
            break;
        case 2:
            bad.samplePresent = false;
            break;
        case 3:
            bad.map.communicationKnown = false;
            break;
        case 4:
            bad.map.source = C::PdoMapSource::TEMPLATE;
            break;
        case 5:
            bad.map.evidenceId = 0;
            break;
        case 6:
            ++bad.map.nodeGeneration;
            break;
        case 7:
            ++bad.qualification.nodeGeneration;
            break;
        case 8:
            bad.qualification.basis = C::PdoSampleBasis::UNQUALIFIED;
            break;
        case 9:
            bad.qualification.evidenceId = 0;
            break;
        case 10:
            ++bad.map.route.generation;
            break;
        case 11:
            ++bad.qualification.route.binding.configurationGeneration;
            break;
        case 12:
            ++bad.observation.sample.route.binding.bindingGeneration;
            break;
        case 13:
            bad.observation.time.present = false;
            break;
        case 14:
            bad.observation.time.receivedTimeKnown = false;
            break;
        case 15:
            ++bad.observation.time.generation;
            break;
        case 16:
            bad.observation.time.receivedUs = 1;
            break;
        case 17:
            bad.observation.time.receivedUs = 4;
            break;
        case 18:
            bad.observation.time.processedUs = 4;
            break;
        case 19:
            ++bad.observation.evidenceId;
            break;
        case 20:
            bad.boundaryUs = 2;
            break;
        case 21:
            bad.observation.sample.frame.id++;
            break;
        case 22:
            bad.observation.sample.frame.dlc = bad.observation.sample.frame.payloadLength = 7;
            break;
        case 23:
            bad.map.source = static_cast<C::PdoMapSource>(255);
            break;
        case 24:
            bad.map.observedUs = 3;
            break; // Recent readback cannot relabel an older sample.
        case 25:
            bad.map.startedUs = 1;
            break;
        case 26:
            bad.boundaryUs = 1;
            bad.qualification.establishedUs = 0;
            break;
        case 27:
            ++bad.map.cobIdParameter;
            break;
        case 28:
            bad.map.transmissionType = 241;
            break;
        case 29:
            bad.map.cobIdParameter |= 0x80000000;
            break; // Disabled route cannot qualify active bytes.
        }
        CHECK(!L::advanceRead(c, bad, 3));
        empty.unchanged(c);
    }
    CHECK(L::advanceRead(c, valid, 3));
    L::StateObservation s;
    CHECK(L::getStateBlock(c, L::StateBlock::FEEDBACK, target(), 3, 1, s, &valid));
    CHECK(s.actualPosition == INT32_MIN && s.actualVelocity == -1 && s.fieldCount == 2);
    CHECK(s.fields[0].source == L::ReadSource::TPDO && s.fields[0].mappingGeneration == 2 &&
          s.fields[0].nodeGeneration == 11);
    CHECK(s.fields[0].captureKnown && s.fields[0].earliestUs == 2 && s.fields[0].deliveredUs == 3 &&
          s.window.skewUs == 0);
    const Saved<L::StateObservation> good(s);
    CHECK(!L::getStateBlock(c, L::StateBlock::FEEDBACK, target(), 4, 1, s, &valid));
    good.unchanged(s);
    CHECK(!L::getStateBlock(c, L::StateBlock::FEEDBACK, target(), 3, 100, s));
    good.unchanged(s);
    C::PdoRecord withdrawn = valid;
    withdrawn.qualified = false;
    CHECK(!L::getStateBlock(c, L::StateBlock::FEEDBACK, target(), 3, 100, s, &withdrawn));
    good.unchanged(s);
    withdrawn = valid;
    ++withdrawn.qualification.evidenceId;
    CHECK(!L::getStateBlock(c, L::StateBlock::FEEDBACK, target(), 3, 100, s, &withdrawn));
    good.unchanged(s);
    M::Frame boot = frame(0x705, 1);
    CHECK(owner.receive(boot, 4, 4));
    CHECK(!L::getStateBlock(c, L::StateBlock::FEEDBACK, target(), 4, 100, s, owner.pdo(0)));
    good.unchanged(s);
    L::ReadTarget reset = target();
    reset.nodeGeneration = owner.node(0)->route.generation;
    CHECK(!L::getStateBlock(c, L::StateBlock::FEEDBACK, reset, 4, 100, s, &valid));
    good.unchanged(s);
    // Selecting an SDO-only monitor block from a feedback PDO fails before admission.
    const Saved<L::ReadContext> retained(c);
    CHECK(!L::prepareState(c, target(), 2, 4, 1000, options(), mask(L::StateBlock::MONITORS),
                           &map.route));
    retained.unchanged(c);
}
} // namespace
int main() {
    allRowsAndRealOwner();
    preparationAndGetters();
    wrongServiceAndTime();
    partialFailuresAndHistory();
    cutoffDoesNotEraseInvalidation();
    passiveProvenance();
    std::printf("reads: %u checks passed\n", checks);
    return 0;
}
