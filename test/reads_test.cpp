// SPDX-License-Identifier: MIT
// Literal native fields come from the retained EDS; host times/provenance are
// synthetic fixtures. These exchanges qualify software, never a physical drive.
#include "SdoClient.h"
#include <MotorControlCAN/profiles/lichuan_c/DriverSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/IoSettings.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
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
// DEC-1 accepted polling list, transcribed from the repair audit rather than read from the ledger
// or generated header, so a mistake there cannot also change this oracle. It is the owner's risk
// acceptance. Nothing here is evidence that an upload has no effect.
struct Address {
    uint16_t index;
    uint8_t subindex;
};
const Address acceptedReads[] = {{0x1000, 0}, {0x1009, 0}, {0x100a, 0}, {0x1018, 0},
                                 {0x1018, 1}, {0x1018, 2}, {0x1018, 3}, {0x2000, 0},
                                 {0x2001, 0}, {0x2100, 0}, {0x2101, 0}, {0x2102, 0},
                                 {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}};
bool accepted(uint16_t index, uint8_t subindex) {
    for (const Address& address : acceptedReads)
        if (address.index == index && address.subindex == subindex)
            return true;
    return false;
}
void expected(const C::SdoTransfer& w, const Field& f) {
    CHECK(w.index == f.index && w.subindex == f.subindex && w.size == f.width);
    CHECK(w.direction == C::SdoDirection::UPLOAD);
    CHECK(w.readEffect == (accepted(f.index, f.subindex) ? C::SdoReadEffect::ACCEPTED_UNQUALIFIED
                                                         : C::SdoReadEffect::UNQUALIFIED));
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
            CHECK(d.representation == rep);
            CHECK(d.readEffect == (accepted(f.index, f.subindex) ? L::ReadEffect::ACCEPTED_UNQUALIFIED
                                                                 : L::ReadEffect::UNQUALIFIED));
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
    // Probe permission and atomic failure: see probeIsOneAcceptedUpload for the wire behavior.
    L::ReadContext probe;
    CHECK(L::prepareProbe(probe, target(), 8, 0, 10000, options()));
    CHECK(probe.kind == L::ReadKind::PROBE && probe.count == 1);
    CHECK(probe.values[0].object.index == 0x1000 &&
          probe.values[0].object.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED);
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
    for (unsigned i = 0; i < 41; ++i) {
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
        case 37:
            // Matching bytes/count do not turn raw experiment evidence into a typed read.
            e.transfer.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
            break;
        case 38:
            // 1000 is accepted: an unlisted effect is a different request, not the same evidence.
            e.transfer.readEffect = C::SdoReadEffect::UNQUALIFIED;
            break;
        case 39:
            e.transfer.readEffect = static_cast<C::SdoReadEffect>(255);
            break;
        case 40:
            // An accepted read keeps its unknown effect. It cannot report the effect as absent.
            e.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
            e.effectPossible = false;
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
                                     C::SdoOutcome::BUS_OFF, C::SdoOutcome::CORRELATION_LOST};
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
// An early budget timeout is its own outcome, and whether a frame left stays separate evidence.
void serviceTimeoutIsNotTheDeadline() {
    for (bool possible : {false, true}) {
        L::ReadContext c;
        CHECK(L::prepareState(
            c, target(), 1, 0, 100, options(),
            uint8_t(mask(L::StateBlock::ERRORS) | mask(L::StateBlock::STATUS))));
        accept(c, 0x80, 0);
        accept(c, 0x1234, 1);
        C::SdoCompletion e = success(c, 0, 2, 10);
        e.outcome = C::SdoOutcome::TIMEOUT;
        e.protocolAcknowledged = false;
        e.size = 0;
        e.queued = e.transmissionPossible = e.effectPossible = e.effectUncertain = possible;
        e.transmissionConfirmed = false;
        CHECK(L::advanceRead(c, e, 10));
        CHECK(c.state == M::ReadState::FAILED && c.outcome == M::ReadOutcome::SERVICE_TIMEOUT);
        CHECK(c.status.detail == int32_t(M::ReadError::TIME) && c.status.msg && *c.status.msg);
        CHECK(c.evidence.outcome == C::SdoOutcome::TIMEOUT && c.uncertain == possible);
        // A timeout says nothing against earlier replies, so that block stays published.
        L::StateObservation s;
        CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), 10, 1000, s));
        CHECK(s.errorRegister == 0x80 && s.errorCode == 0x1234);
    }
    // The same terminal at the cutoff is the deadline.
    L::ReadContext late;
    CHECK(L::prepareIdentity(late, target(), 1, 0, 100, options()));
    C::SdoCompletion e = success(late, 0, 2, 100);
    e.outcome = C::SdoOutcome::TIMEOUT;
    e.protocolAcknowledged = false;
    e.size = 0;
    e.effectUncertain = true;
    CHECK(L::advanceRead(late, e, 100));
    CHECK(late.outcome == M::ReadOutcome::DEADLINE);
}
// Historical bytes survive the withdrawal, and a later terminal cannot clear it.
void lostCorrelationWithdrawsButKeepsHistory() {
    L::ReadContext c;
    CHECK(L::prepareState(c, target(), 1, 0, 10000, options(),
                          uint8_t(mask(L::StateBlock::ERRORS) | mask(L::StateBlock::STATUS))));
    accept(c, 0x80, 0);
    accept(c, 0x1234, 1);
    L::StateObservation s;
    CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), 2, 100, s));
    const Saved<L::StateObservation> published(s);
    C::SdoCompletion e = success(c, 0, 2, 3);
    e.outcome = C::SdoOutcome::CORRELATION_LOST;
    e.protocolAcknowledged = false;
    e.size = 0;
    e.effectUncertain = true;
    CHECK(L::advanceRead(c, e, 3));
    CHECK(c.state == M::ReadState::FAILED && c.outcome == M::ReadOutcome::TRANSPORT_ERROR &&
          c.invalidated);
    CHECK(c.evidencePresent && c.evidence.outcome == C::SdoOutcome::CORRELATION_LOST);
    CHECK(c.values[0].valid && c.values[0].raw == 0x80 && c.values[1].raw == 0x1234);
    CHECK(c.values[0].provenance.index == 0x1001 && c.values[1].provenance.index == 0x603f);
    const M::Status refused = L::getStateBlock(c, L::StateBlock::ERRORS, target(), 3, 100, s);
    CHECK(!refused && refused.detail == int32_t(M::ReadError::TARGET));
    published.unchanged(s);
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
                time.delivered >= 100              ? M::ReadOutcome::DEADLINE
                : reason == C::SdoOutcome::TIMEOUT ? M::ReadOutcome::SERVICE_TIMEOUT
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
// Every readable descriptor is swept, so an extra accepted row cannot go unnoticed. The generator
// test pins the table at 95 rows; finding all 95 proves no row lies outside this finite range.
void acceptedListIsExact() {
    unsigned readable = 0, effective = 0;
    for (uint32_t index = 0x1000; index <= 0x6fff; ++index)
        for (uint32_t subindex = 0; subindex <= 0x1f; ++subindex) {
            L::ReadDescriptor d = {};
            if (!L::readDescriptor(uint16_t(index), uint8_t(subindex), d))
                continue;
            ++readable;
            const bool listed = accepted(uint16_t(index), uint8_t(subindex));
            CHECK(d.readEffect == (listed ? L::ReadEffect::ACCEPTED_UNQUALIFIED
                                          : L::ReadEffect::UNQUALIFIED));
            effective += d.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED;
        }
    CHECK(readable == 95 && effective == 16);
    for (const Address& address : acceptedReads) {
        L::ReadDescriptor d = {};
        CHECK(L::readDescriptor(address.index, address.subindex, d));
        CHECK(d.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED);
    }
    // Named because they are explicit single reads (DEC-1), or sit in a fixed plan beside accepted
    // rows, or are ordinary settings that keep their existing disposition.
    const Address never[] = {{0x603f, 0}, {0x1001, 0}, {0x1003, 0}, {0x1003, 1}, {0x1003, 2},
                             {0x1003, 3}, {0x1003, 4}, {0x60fd, 0}, {0x1005, 0}, {0x1017, 0},
                             {0x2200, 0}, {0x2500, 0}};
    for (const Address& address : never) {
        L::ReadDescriptor d = {};
        CHECK(L::readDescriptor(address.index, address.subindex, d));
        CHECK(d.readEffect == L::ReadEffect::UNQUALIFIED);
    }
}
// Measured replies, retained on two CL86-C specimens (docs/bench/20261102-nmt-pdo-node1/rx.txt and
// its node 2 record). They contradict the EDS, and they are a labelled fixture: not a default, not
// a claim about another drive or firmware, and not a reason to relax typed width checks.
const uint8_t measuredIdentity[9][8] = {
    {0x43, 0x00, 0x10, 0x00, 0x92, 0x01, 0x04, 0x00}, {0x4f, 0x09, 0x10, 0x00, 0x0c, 0, 0, 0},
    {0x4f, 0x0a, 0x10, 0x00, 0x20, 0, 0, 0},          {0x4f, 0x18, 0x10, 0x00, 0x05, 0, 0, 0},
    {0x43, 0x18, 0x10, 0x01, 0x00, 0x10, 0, 0},       {0x43, 0x18, 0x10, 0x02, 0x01, 0, 0, 0},
    {0x43, 0x18, 0x10, 0x03, 0, 0, 0, 0},             {0x4b, 0x00, 0x20, 0x00, 0x00, 0x04, 0, 0},
    {0x4b, 0x01, 0x20, 0x00, 0x00, 0x02, 0, 0}};
// Two-byte forms of the EDS U16 declaration. Synthetic: no retained specimen replied this way.
const uint8_t declared1009[8] = {0x4b, 0x09, 0x10, 0x00, 0x08, 0x26, 0, 0};
const uint8_t declared100a[8] = {0x4b, 0x0a, 0x10, 0x00, 0x01, 0x00, 0, 0};
// One exchange through the real service client, checking the exact request bytes. The caller
// supplies literal reply bytes, so a wrong width reaches the client's own reply checks.
C::SdoCompletion exchange(C::SdoClient& owner, L::ReadContext& c, const uint8_t (&bytes)[8],
                          uint64_t& now) {
    const L::PreparedRead p = next(c, now);
    C::SdoRequestId id;
    CHECK(owner.admit(0, p.transfer, now, id));
    CHECK(owner.service(now));
    M::Frame tx;
    C::SdoTxId txId;
    CHECK(owner.nextTx(0, now, tx, txId));
    const uint8_t request[8] = {0x40, uint8_t(p.transfer.index), uint8_t(p.transfer.index >> 8),
                                p.transfer.subindex, 0, 0, 0, 0};
    CHECK(tx.id == 0x625 && tx.payloadLength == 8 && std::memcmp(tx.data, request, 8) == 0);
    CHECK(owner.canTransmit(txId, now) && owner.confirmTx(txId, now));
    M::Frame reply = frame(0x5a5, 8);
    std::memcpy(reply.data, bytes, 8);
    ++now;
    CHECK(owner.receive(reply, now, now));
    const C::SdoCompletion* done = owner.result(id);
    CHECK(done != nullptr);
    const C::SdoCompletion copy = *done;
    CHECK(copy.replyValid && std::memcmp(copy.reply.data, bytes, 8) == 0);
    CHECK(L::advanceRead(c, copy, now));
    CHECK(owner.release(id));
    return copy;
}
void noFrameLeft(C::SdoClient& owner, uint64_t now) {
    M::Frame tx;
    C::SdoTxId id;
    CHECK(!owner.nextTx(0, now, tx, id));
}
void probeIsOneAcceptedUpload() {
    for (unsigned confirmed = 0; confirmed < 2; ++confirmed) {
        C::SdoClient owner;
        CHECK(owner.configure(0, target().channel, confirmed != 0, true));
        CHECK(owner.configureNetwork(0, network(), C::NetworkObserver(), 0));
        L::ReadContext c;
        CHECK(L::prepareProbe(c, target(), 1, 0, 10000, options()));
        // Preparation is pure: nothing is queued, and repeating nextRead stages nothing.
        CHECK(owner.port(0)->handedOff == 0 && owner.nmtPort()->accepted == 0);
        L::PreparedRead p = next(c, 0);
        CHECK(p.transfer.readEffect == C::SdoReadEffect::ACCEPTED_UNQUALIFIED);
        CHECK(p.transfer.direction == C::SdoDirection::UPLOAD && p.transfer.size == 4 &&
              !p.transfer.requirePreOperational);
        noFrameLeft(owner, 0);
        uint64_t now = 0;
        const C::SdoCompletion done = exchange(owner, c, measuredIdentity[0], now);
        CHECK(done.outcome == C::SdoOutcome::SUCCESS && done.effectPossible &&
              !done.effectUncertain);
        CHECK(c.state == M::ReadState::SUCCEEDED && c.outcome == M::ReadOutcome::SUCCESS);
        CHECK(c.count == 1 && c.values[0].valid && c.values[0].raw == 0x00040192u);
        // Exactly one frame on the SDO route, and no NMT command. A probe sets no setting.
        CHECK(owner.port(0)->handedOff == 1);
        CHECK(owner.nmtPort()->accepted == 0 && owner.nmtPort()->handedOff == 0);
        CHECK(!owner.node(0)->intentPresent && !owner.node(0)->transitionPending);
        noFrameLeft(owner, now);
        L::PreparedRead terminal;
        CHECK(L::nextRead(c, now, terminal) && !terminal.ready);
        // A probe is its own intent. No typed getter publishes it as identity or configuration.
        L::IdentityObservation id;
        id.modelCode = 0xa55a;
        const Saved<L::IdentityObservation> oldId(id);
        CHECK(!L::getIdentity(c, target(), now, 1000, id));
        oldId.unchanged(id);
        L::ConfigObservation cfg;
        cfg.syncCobId = 0xa55a;
        const Saved<L::ConfigObservation> oldCfg(cfg);
        CHECK(!L::getConfig(c, target(), now, 1000, cfg));
        oldCfg.unchanged(cfg);
    }
    // There is no selection through which another object could arrive, and failure is atomic.
    L::ReadContext c;
    c.operation = 0xa55a;
    const Saved<L::ReadContext> before(c);
    L::ReadTarget bad = target();
    bad.nodeGeneration = 0;
    CHECK(!L::prepareProbe(c, bad, 1, 0, 10000, options()));
    before.unchanged(c);
    CHECK(!L::prepareProbe(c, target(), 0, 0, 10000, options()));
    before.unchanged(c);
}
void acceptedEffectsStayUnknown() {
    // A successful accepted read keeps a possible effect, recorded in the retained evidence.
    {
        C::SdoClient owner;
        configure(owner);
        uint64_t now = 0;
        L::ReadContext c;
        CHECK(L::prepareState(c, target(), 1, 0, 10000, options(), mask(L::StateBlock::STATUS)));
        const uint8_t status[8] = {0x4b, 0x41, 0x60, 0x00, 0x37, 0x80, 0, 0};
        const C::SdoCompletion done = exchange(owner, c, status, now);
        CHECK(done.transfer.readEffect == C::SdoReadEffect::ACCEPTED_UNQUALIFIED);
        CHECK(done.effectPossible && !done.effectUncertain);
        CHECK(c.evidencePresent && c.evidence.effectPossible && !c.uncertain);
    }
    // A lost reply leaves the effect uncertain and quarantines the route: no silent replay.
    {
        C::SdoClient owner;
        configure(owner);
        L::ReadContext c;
        CHECK(L::prepareState(c, target(), 1, 0, 100, options(), mask(L::StateBlock::STATUS)));
        const L::PreparedRead p = next(c, 0);
        C::SdoRequestId id;
        CHECK(owner.admit(0, p.transfer, 0, id));
        CHECK(owner.service(0));
        M::Frame tx;
        C::SdoTxId txId;
        CHECK(owner.nextTx(0, 0, tx, txId) && owner.confirmTx(txId, 0));
        CHECK(owner.service(100));
        const C::SdoCompletion* lost = owner.result(id);
        CHECK(lost && lost->outcome == C::SdoOutcome::TIMEOUT);
        CHECK(lost->transfer.readEffect == C::SdoReadEffect::ACCEPTED_UNQUALIFIED);
        CHECK(lost->effectPossible && lost->effectUncertain && !lost->protocolAcknowledged);
        CHECK(L::advanceRead(c, *lost, 100));
        CHECK(c.state == M::ReadState::FAILED && c.outcome == M::ReadOutcome::DEADLINE);
        CHECK(c.uncertain && !c.values[0].valid);
        CHECK(owner.release(id) && owner.unresolved(0));
        L::ReadContext again;
        CHECK(L::prepareState(again, target(), 2, 100, 200, options(),
                              mask(L::StateBlock::STATUS)));
        C::SdoRequestId refused;
        CHECK(!owner.admit(0, next(again, 100).transfer, 100, refused));
    }
    // A local withdrawal of accepted work conservatively keeps uncertainty too.
    {
        L::ReadContext c;
        CHECK(L::prepareProbe(c, target(), 1, 0, 100, options()));
        CHECK(L::advanceRead(c, M::ReadOutcome::CANCELLED, 1));
        CHECK(c.state == M::ReadState::FAILED && c.uncertain);
    }
}
void identityWidthVariants() {
    CHECK(L::ReadOptions().identityWidths == L::IdentityWidths::DECLARED_U16);
    L::IdentityObservation untouched;
    untouched.modelCode = 0xa55a;
    untouched.hardwareVersionWidth = 0x7f;
    const Saved<L::IdentityObservation> old(untouched);
    // DECLARED_U16 with the measured one-byte reply: a width failure, and nothing is published.
    {
        C::SdoClient owner;
        configure(owner);
        uint64_t now = 0;
        L::ReadContext c;
        CHECK(L::prepareIdentity(c, target(), 1, 0, 10000, options()));
        CHECK(exchange(owner, c, measuredIdentity[0], now).outcome == C::SdoOutcome::SUCCESS);
        CHECK(next(c, now).transfer.size == 2);
        const C::SdoCompletion failed = exchange(owner, c, measuredIdentity[1], now);
        CHECK(failed.outcome == C::SdoOutcome::CLIENT_ABORT && failed.abortValid);
        CHECK(failed.abortCode == 0x06070010u && failed.size == 0);
        CHECK(failed.effectPossible && failed.effectUncertain && failed.clientAbortQueued);
        CHECK(c.state == M::ReadState::FAILED && c.outcome == M::ReadOutcome::REPLY_ERROR);
        CHECK(c.uncertain && c.values[0].valid && !c.values[1].valid && c.values[1].raw == 0);
        CHECK(!L::getIdentity(c, target(), now, 1000, untouched));
        old.unchanged(untouched);
        M::Frame abort;
        C::SdoTxId abortId;
        CHECK(owner.nextTx(0, now, abort, abortId));
        const uint8_t literal[8] = {0x80, 0x09, 0x10, 0x00, 0x10, 0x00, 0x07, 0x06};
        CHECK(std::memcmp(abort.data, literal, 8) == 0 && owner.unresolved(0));
        // Nothing learned the width from that reply. The next default read still declares U16.
        L::ReadContext again;
        CHECK(L::prepareIdentity(again, target(), 2, now, 10000, options()));
        CHECK(again.values[1].object.widthBytes == 2 && again.values[2].object.widthBytes == 2);
    }
    L::ReadOptions measuredOptions = options();
    measuredOptions.identityWidths = L::IdentityWidths::MEASURED_U8;
    // MEASURED_U8 with the measured replies publishes raw values and actual widths.
    {
        C::SdoClient owner;
        configure(owner);
        uint64_t now = 0;
        L::ReadContext c;
        CHECK(L::prepareIdentity(c, target(), 1, 0, 10000, measuredOptions));
        const uint8_t widths[9] = {4, 1, 1, 1, 4, 4, 4, 2, 2};
        for (unsigned i = 0; i < 9; ++i) {
            CHECK(c.values[i].object.widthBytes == widths[i]);
            const C::SdoCompletion done = exchange(owner, c, measuredIdentity[i], now);
            CHECK(done.outcome == C::SdoOutcome::SUCCESS && done.size == widths[i]);
            CHECK(done.transfer.size == widths[i] && done.effectPossible && !done.effectUncertain);
            CHECK(done.transfer.readEffect == C::SdoReadEffect::ACCEPTED_UNQUALIFIED);
        }
        CHECK(c.state == M::ReadState::SUCCEEDED);
        L::IdentityObservation id = untouched;
        CHECK(L::getIdentity(c, target(), now, 1000, id));
        CHECK(id.deviceType == 0x00040192u);
        CHECK(id.hardwareVersion == 0x0c && id.hardwareVersionWidth == 1);
        CHECK(id.softwareVersion == 0x20 && id.softwareVersionWidth == 1);
        // 1018:00=5 exceeds the EDS limit of 4 and stays raw. It adds no 1018:04 upload.
        CHECK(id.identityEntries == 5 && owner.port(0)->handedOff == 9);
        CHECK(id.vendorId == 0x1000 && id.productCode == 1 && id.revision == 0);
        CHECK(id.modelCode == 0x0400 && id.firmwareCode == 0x0200 && !id.modelMatched);
        CHECK(id.fields[1].index == 0x1009 && id.fields[2].index == 0x100a);
        CHECK(!id.fields[1].captureKnown && id.fields[0].earliestUs == 0);
        // The descriptor table is untouched: the option narrows this one context only.
        L::ReadDescriptor table = {};
        CHECK(L::readDescriptor(0x1009, 0, table) && table.widthBytes == 2);
        // A public context cannot swap its width or option after preparation.
        for (unsigned variant = 0; variant < 3; ++variant) {
            L::ReadContext forged;
            CHECK(L::prepareIdentity(forged, target(), 9, now, 10000, measuredOptions));
            if (variant == 0)
                forged.options.identityWidths = L::IdentityWidths::DECLARED_U16;
            else if (variant == 1)
                forged.values[1].object.widthBytes = 2;
            else
                forged.values[3].object.widthBytes = 2; // 1018:00 is outside the option.
            L::PreparedRead p;
            CHECK(!L::nextRead(forged, now, p) && !p.ready);
        }
        L::ReadContext forged;
        CHECK(L::prepareIdentity(forged, target(), 9, now, 10000, options()));
        forged.values[1].object.widthBytes = 1;
        L::PreparedRead p;
        CHECK(!L::nextRead(forged, now, p) && !p.ready);
    }
    // MEASURED_U8 with a two-byte reply fails, at either object.
    for (unsigned failing = 1; failing <= 2; ++failing) {
        C::SdoClient owner;
        configure(owner);
        uint64_t now = 0;
        L::ReadContext c;
        CHECK(L::prepareIdentity(c, target(), 1, 0, 10000, measuredOptions));
        for (unsigned i = 0; i < failing; ++i)
            CHECK(exchange(owner, c, measuredIdentity[i], now).outcome == C::SdoOutcome::SUCCESS);
        const C::SdoCompletion failed =
            exchange(owner, c, failing == 1 ? declared1009 : declared100a, now);
        CHECK(failed.outcome == C::SdoOutcome::CLIENT_ABORT && failed.abortCode == 0x06070010u);
        CHECK(c.outcome == M::ReadOutcome::REPLY_ERROR && !c.values[failing].valid);
        CHECK(!L::getIdentity(c, target(), now, 1000, untouched));
        old.unchanged(untouched);
    }
    // MEASURED_U8 evidence claiming two bytes cannot bypass the width check.
    {
        L::ReadContext c;
        CHECK(L::prepareIdentity(c, target(), 1, 0, 10000, measuredOptions));
        accept(c, 0x00040192u, 0);
        C::SdoCompletion e = success(c, 0x0c, 1, 1);
        e.size = 2;
        const Saved<L::ReadContext> before(c);
        CHECK(!L::advanceRead(c, e, 1));
        before.unchanged(c);
    }
    // DECLARED_U16 with two-byte replies publishes width 2.
    {
        C::SdoClient owner;
        configure(owner);
        uint64_t now = 0;
        L::ReadContext c;
        CHECK(L::prepareIdentity(c, target(), 1, 0, 10000, options()));
        for (unsigned i = 0; i < 9; ++i) {
            const uint8_t (&reply)[8] =
                i == 1 ? declared1009 : i == 2 ? declared100a : measuredIdentity[i];
            CHECK(exchange(owner, c, reply, now).outcome == C::SdoOutcome::SUCCESS);
        }
        L::IdentityObservation id;
        CHECK(L::getIdentity(c, target(), now, 1000, id));
        CHECK(id.hardwareVersion == 0x2608 && id.hardwareVersionWidth == 2);
        CHECK(id.softwareVersion == 0x0001 && id.softwareVersionWidth == 2);
    }
}
void checkArgument(const M::Status& status) {
    CHECK(status.code == M::Err::INVALID_CONFIG &&
          status.detail == static_cast<int32_t>(M::ReadError::ARGUMENT));
}
void identityWidthOptionRejectsAtomically() {
    const C::PdoMapEvidence map = mapEvidence();
    const L::IdentityWidths values[] = {L::IdentityWidths::MEASURED_U8,
                                        static_cast<L::IdentityWidths>(2),
                                        static_cast<L::IdentityWidths>(255)};
    L::ReadContext c;
    CHECK(L::prepareConfig(c, target(), 1, 0, 10000, options()));
    const Saved<L::ReadContext> before(c);
    L::DriverContext driver;
    CHECK(L::prepareDriverRead(driver, target(), 1, 0, 10000, options(), L::DriverGroup::DRIVER));
    const Saved<L::DriverContext> beforeDriver(driver);
    for (L::IdentityWidths value : values) {
        L::ReadOptions o = options();
        o.identityWidths = value;
        checkArgument(L::prepareProbe(c, target(), 2, 0, 10000, o));
        before.unchanged(c);
        checkArgument(L::prepareConfig(c, target(), 2, 0, 10000, o));
        before.unchanged(c);
        checkArgument(L::prepareState(c, target(), 2, 0, 10000, o, mask(L::StateBlock::STATUS)));
        before.unchanged(c);
        checkArgument(L::prepareState(c, target(), 2, 0, 10000, o, mask(L::StateBlock::FEEDBACK),
                                      &map.route));
        before.unchanged(c);
        checkArgument(L::prepareDriverRead(driver, target(), 2, 0, 10000, o,
                                           L::DriverGroup::SOFT_LIMITS));
        beforeDriver.unchanged(driver);
        checkArgument(L::prepareControlRead(driver, target(), 2, 0, 10000, o));
        beforeDriver.unchanged(driver);
        checkArgument(L::prepareTuningRead(driver, target(), 2, 0, 10000, o,
                                           L::TuningGroup::EDS_GAINS));
        beforeDriver.unchanged(driver);
        if (value != L::IdentityWidths::MEASURED_U8) {
            checkArgument(L::prepareIdentity(c, target(), 2, 0, 10000, o));
            before.unchanged(c);
        }
    }
}
// Measured replies to the two array counts on two CL86-C specimens, 2026-11-04: two bytes of 0
// against the EDS U8 (docs/bench/20261104T1405Z-settings2202-node1-313 and -node2-315,
// 20261104T1309Z-io-node1-303 and -node2-304). 2202 is those records' own U16 0, and 607D:01/:02
// the signed extremes the 2026-11-02 raw uploads read (20261102-final-node1). A labelled fixture,
// not a default and not a claim about another drive or firmware.
const uint8_t measured2202[8] = {0x4b, 0x02, 0x22, 0x00, 0, 0, 0, 0};
const uint8_t measured607d00[8] = {0x4b, 0x7d, 0x60, 0x00, 0, 0, 0, 0};
const uint8_t measured607d01[8] = {0x43, 0x7d, 0x60, 0x01, 0, 0, 0, 0x80};
const uint8_t measured607d02[8] = {0x43, 0x7d, 0x60, 0x02, 0xff, 0xff, 0xff, 0x7f};
const uint8_t measured60fe00[8] = {0x4b, 0xfe, 0x60, 0x00, 0, 0, 0, 0};
// Synthetic: the one-byte forms of the EDS U8 with its default 2, a two-byte count whose high byte
// is set, and 60FE:01/:02 words, which no retained record reached.
const uint8_t declared607d00[8] = {0x4f, 0x7d, 0x60, 0x00, 0x02, 0, 0, 0};
const uint8_t declared60fe00[8] = {0x4f, 0xfe, 0x60, 0x00, 0x02, 0, 0, 0};
const uint8_t wide607d00[8] = {0x4b, 0x7d, 0x60, 0x00, 0x02, 0x01, 0, 0};
const uint8_t synthetic60fe01[8] = {0x43, 0xfe, 0x60, 0x01, 0x00, 0x00, 0x01, 0x80};
const uint8_t synthetic60fe02[8] = {0x43, 0xfe, 0x60, 0x02, 0x00, 0x00, 0x07, 0x00};
// One group read through the real client with literal replies; returns the last terminal.
typedef const uint8_t Reply[8];
C::SdoCompletion groupRead(L::DriverContext& c, L::DriverGroup group, const L::ReadOptions& o,
                           Reply* const* replies, unsigned count) {
    C::SdoClient owner;
    configure(owner);
    uint64_t now = 0;
    CHECK(L::prepareDriverRead(c, target(), 1, 0, 10000, o, group));
    C::SdoCompletion last;
    for (unsigned i = 0; i < count; ++i)
        last = exchange(owner, c.read, *replies[i], now);
    return last;
}
void countWidthVariants() {
    CHECK(L::ReadOptions().countWidths == L::CountWidths::DECLARED_U8);
    L::DriverObservation untouched;
    untouched.softLimitEntries = 0xa55a;
    untouched.softLimitEntriesWidth = 0x7f;
    const Saved<L::DriverObservation> old(untouched);
    L::IoObservation untouchedIo;
    untouchedIo.outputEntries = 0xa55a;
    untouchedIo.outputEntriesWidth = 0x7f;
    const Saved<L::IoObservation> oldIo(untouchedIo);
    L::ReadOptions measured = options();
    measured.countWidths = L::CountWidths::MEASURED_U16;
    // DECLARED_U8 with the measured two-byte replies: a width failure with the very cleanup frame
    // both specimens' records hold, and nothing is published.
    for (unsigned io = 0; io < 2; ++io) {
        Reply* const limits[] = {&measured2202, &measured607d00};
        Reply* const outputs[] = {&measured60fe00};
        L::DriverContext c;
        const C::SdoCompletion failed =
            io ? groupRead(c, L::DriverGroup::IO_OUTPUT_COMMAND, options(), outputs, 1)
               : groupRead(c, L::DriverGroup::SOFT_LIMITS, options(), limits, 2);
        CHECK(c.read.values[io ? 0 : 1].object.widthBytes == 1);
        CHECK(failed.outcome == C::SdoOutcome::CLIENT_ABORT && failed.abortValid);
        CHECK(failed.abortCode == 0x06070010u && failed.size == 0 && failed.clientAbortQueued);
        CHECK(failed.effectPossible && failed.effectUncertain);
        CHECK(c.read.state == M::ReadState::FAILED && c.read.outcome == M::ReadOutcome::REPLY_ERROR);
        CHECK(c.read.uncertain && !c.read.values[io ? 0 : 1].valid);
        CHECK(!L::getDriver(c, target(), 10, 1000, untouched) && !L::getIo(c, target(), 10, 1000, untouchedIo));
        old.unchanged(untouched);
        oldIo.unchanged(untouchedIo);
        // Nothing learned the width from that reply. The next default read still declares U8.
        L::DriverContext again;
        CHECK(L::prepareDriverRead(again, target(), 2, 10, 10000, options(),
                                   io ? L::DriverGroup::IO_OUTPUT_COMMAND : L::DriverGroup::SOFT_LIMITS));
        CHECK(again.read.values[io ? 0 : 1].object.widthBytes == 1);
    }
    {
        // The literal cleanup frames, through the client that sent them on the bench.
        C::SdoClient owner;
        configure(owner);
        uint64_t now = 0;
        L::DriverContext c;
        CHECK(L::prepareDriverRead(c, target(), 1, 0, 10000, options(), L::DriverGroup::IO_OUTPUT_COMMAND));
        exchange(owner, c.read, measured60fe00, now);
        M::Frame abort;
        C::SdoTxId abortId;
        CHECK(owner.nextTx(0, now, abort, abortId));
        const uint8_t literal[8] = {0x80, 0xfe, 0x60, 0x00, 0x10, 0x00, 0x07, 0x06};
        CHECK(std::memcmp(abort.data, literal, 8) == 0 && owner.unresolved(0));
    }
    // MEASURED_U16 with the measured replies publishes the raw counts and their actual widths.
    {
        Reply* const replies[] = {&measured2202, &measured607d00, &measured607d01, &measured607d02};
        L::DriverContext c;
        groupRead(c, L::DriverGroup::SOFT_LIMITS, measured, replies, 4);
        const uint8_t widths[4] = {2, 2, 4, 4};
        for (unsigned i = 0; i < 4; ++i)
            CHECK(c.read.values[i].object.widthBytes == widths[i]);
        CHECK(c.read.state == M::ReadState::SUCCEEDED && !c.read.uncertain);
        L::DriverObservation o = untouched;
        CHECK(L::getDriver(c, target(), 10, 1000, o));
        CHECK(o.group == L::DriverGroup::SOFT_LIMITS && o.softLimitSelectorRaw == 0);
        CHECK(o.softLimitEntries == 0 && o.softLimitEntriesWidth == 2 && o.fieldCount == 4);
        CHECK(o.minimumNative == INT32_MIN && o.maximumNative == INT32_MAX && !o.interpreted);
        CHECK(o.fields[1].index == 0x607d && o.fields[1].subindex == 0);
        // The descriptor table is untouched: the option narrows this one context only.
        L::ReadDescriptor table = {};
        CHECK(L::readDescriptor(0x607d, 0, table) && table.widthBytes == 1);
        CHECK(L::readDescriptor(0x60fe, 0, table) && table.widthBytes == 1);
    }
    {
        Reply* const replies[] = {&measured60fe00, &synthetic60fe01, &synthetic60fe02};
        L::DriverContext c;
        groupRead(c, L::DriverGroup::IO_OUTPUT_COMMAND, measured, replies, 3);
        CHECK(c.read.values[0].object.widthBytes == 2 && c.read.values[1].object.widthBytes == 4 &&
              c.read.values[2].object.widthBytes == 4);
        L::IoObservation o = untouchedIo;
        CHECK(L::getIo(c, target(), 10, 1000, o));
        CHECK(o.group == L::DriverGroup::IO_OUTPUT_COMMAND && o.fieldCount == 3);
        CHECK(o.outputEntries == 0 && o.outputEntriesWidth == 2);
        CHECK(o.outputValueRaw == 0x80010000u && o.outputMaskRaw == 0x00070000u);
        CHECK(!o.outputMappingKnown && !o.electricalLevelsKnown);
    }
    // Both bytes of a two-byte count are kept, and the count bounds nothing: the plan stays four.
    {
        Reply* const replies[] = {&measured2202, &wide607d00, &measured607d01, &measured607d02};
        L::DriverContext c;
        groupRead(c, L::DriverGroup::SOFT_LIMITS, measured, replies, 4);
        L::DriverObservation o;
        CHECK(L::getDriver(c, target(), 10, 1000, o) && o.softLimitEntries == 0x0102 && c.read.count == 4);
    }
    // MEASURED_U16 with a one-byte reply fails, at either count.
    for (unsigned io = 0; io < 2; ++io) {
        Reply* const limits[] = {&measured2202, &declared607d00};
        Reply* const outputs[] = {&declared60fe00};
        L::DriverContext c;
        const C::SdoCompletion failed =
            io ? groupRead(c, L::DriverGroup::IO_OUTPUT_COMMAND, measured, outputs, 1)
               : groupRead(c, L::DriverGroup::SOFT_LIMITS, measured, limits, 2);
        CHECK(failed.outcome == C::SdoOutcome::CLIENT_ABORT && failed.abortCode == 0x06070010u);
        CHECK(c.read.outcome == M::ReadOutcome::REPLY_ERROR && !c.read.values[io ? 0 : 1].valid);
        CHECK(!L::getDriver(c, target(), 10, 1000, untouched) && !L::getIo(c, target(), 10, 1000, untouchedIo));
        old.unchanged(untouched);
        oldIo.unchanged(untouchedIo);
    }
    // MEASURED_U16 evidence claiming one byte cannot bypass the width check.
    {
        L::DriverContext c;
        CHECK(L::prepareDriverRead(c, target(), 1, 0, 10000, measured, L::DriverGroup::SOFT_LIMITS));
        accept(c.read, 0, 0);
        C::SdoCompletion e = success(c.read, 2, 1, 1);
        CHECK(e.transfer.size == 2);
        e.size = 1;
        const Saved<L::DriverContext> before(c);
        CHECK(!L::advanceDriver(c, e, 1));
        before.unchanged(c);
    }
    // A public context cannot swap its width or option after preparation.
    for (unsigned variant = 0; variant < 4; ++variant) {
        L::DriverContext forged;
        CHECK(L::prepareDriverRead(forged, target(), 9, 0, 10000, variant == 3 ? options() : measured,
                                   L::DriverGroup::SOFT_LIMITS));
        if (variant == 0)
            forged.read.options.countWidths = L::CountWidths::DECLARED_U8;
        else if (variant == 1)
            forged.read.values[1].object.widthBytes = 1;
        else if (variant == 2)
            forged.read.values[2].object.widthBytes = 2; // 607D:01 is outside the option.
        else
            forged.read.values[1].object.widthBytes = 2; // The declared plan widened by hand.
        L::PreparedRead p;
        CHECK(!L::nextDriver(forged, 0, p) && !p.ready);
    }
    // DECLARED_U8 with one-byte replies is today's read: width 1 and the raw count.
    {
        Reply* const replies[] = {&measured2202, &declared607d00, &measured607d01, &measured607d02};
        L::DriverContext c;
        groupRead(c, L::DriverGroup::SOFT_LIMITS, options(), replies, 4);
        L::DriverObservation o;
        CHECK(L::getDriver(c, target(), 10, 1000, o) && o.softLimitEntries == 2 && o.softLimitEntriesWidth == 1);
        Reply* const outputs[] = {&declared60fe00, &synthetic60fe01, &synthetic60fe02};
        L::DriverContext d;
        groupRead(d, L::DriverGroup::IO_OUTPUT_COMMAND, options(), outputs, 3);
        L::IoObservation io;
        CHECK(L::getIo(d, target(), 10, 1000, io) && io.outputEntries == 2 && io.outputEntriesWidth == 1);
    }
}
// MEASURED_U16 is refused on every kind but its two groups, and an unknown value on all of them.
void countWidthOptionRejectsAtomically() {
    const C::PdoMapEvidence map = mapEvidence();
    const L::CountWidths values[] = {L::CountWidths::MEASURED_U16, static_cast<L::CountWidths>(2),
                                     static_cast<L::CountWidths>(255)};
    L::ReadContext c;
    CHECK(L::prepareConfig(c, target(), 1, 0, 10000, options()));
    const Saved<L::ReadContext> before(c);
    L::DriverContext driver;
    CHECK(L::prepareDriverRead(driver, target(), 1, 0, 10000, options(), L::DriverGroup::DRIVER));
    const Saved<L::DriverContext> beforeDriver(driver);
    const L::DriverGroup others[] = {L::DriverGroup::DRIVER, L::DriverGroup::MOTION_PARAMETERS,
                                     L::DriverGroup::IO_INPUT_CONFIGURATION,
                                     L::DriverGroup::IO_OUTPUT_CONFIGURATION, L::DriverGroup::IO_INPUT_FILTERS,
                                     L::DriverGroup::CONTROL, L::DriverGroup::TUNING_CURRENT_HOLDING};
    for (L::CountWidths value : values) {
        L::ReadOptions o = options();
        o.countWidths = value;
        checkArgument(L::prepareProbe(c, target(), 2, 0, 10000, o));
        before.unchanged(c);
        checkArgument(L::prepareIdentity(c, target(), 2, 0, 10000, o));
        before.unchanged(c);
        checkArgument(L::prepareConfig(c, target(), 2, 0, 10000, o));
        before.unchanged(c);
        checkArgument(L::prepareState(c, target(), 2, 0, 10000, o, mask(L::StateBlock::STATUS)));
        before.unchanged(c);
        checkArgument(L::prepareState(c, target(), 2, 0, 10000, o, mask(L::StateBlock::FEEDBACK), &map.route));
        before.unchanged(c);
        for (L::DriverGroup group : others) {
            checkArgument(L::prepareDriverRead(driver, target(), 2, 0, 10000, o, group));
            beforeDriver.unchanged(driver);
        }
        checkArgument(L::prepareControlRead(driver, target(), 2, 0, 10000, o));
        beforeDriver.unchanged(driver);
        checkArgument(L::prepareTuningRead(driver, target(), 2, 0, 10000, o, L::TuningGroup::EDS_GAINS));
        beforeDriver.unchanged(driver);
        const L::ReadKind kinds[] = {L::ReadKind::DRIVER_SOFT_LIMITS, L::ReadKind::IO_OUTPUT_COMMAND};
        for (L::ReadKind kind : kinds) {
            const M::Status checked = L::validateRead(kind, target(), 2, 0, 10000, o);
            CHECK(bool(checked) == (value == L::CountWidths::MEASURED_U16));
        }
        if (value != L::CountWidths::MEASURED_U16) {
            checkArgument(L::prepareDriverRead(driver, target(), 2, 0, 10000, o, L::DriverGroup::SOFT_LIMITS));
            beforeDriver.unchanged(driver);
            checkArgument(L::prepareDriverRead(driver, target(), 2, 0, 10000, o, L::DriverGroup::IO_OUTPUT_COMMAND));
            beforeDriver.unchanged(driver);
        }
        checkArgument(L::validateRead(L::ReadKind::DRIVER_MOTION_PARAMETERS, target(), 2, 0, 10000, o));
    }
    // Both options at once are still each on its own kind: neither group admits identity widths.
    L::ReadOptions both = options();
    both.countWidths = L::CountWidths::MEASURED_U16;
    both.identityWidths = L::IdentityWidths::MEASURED_U8;
    checkArgument(L::prepareDriverRead(driver, target(), 2, 0, 10000, both, L::DriverGroup::SOFT_LIMITS));
    checkArgument(L::prepareIdentity(c, target(), 2, 0, 10000, both));
    beforeDriver.unchanged(driver);
    before.unchanged(c);
}
// Existing coverage kept as is: wrong-kind and unfinished getters, stale targets and age limits
// (preparationAndGetters), timestamp zero (wrongServiceAndTime), full history counts and synthetic
// reboot/binding/bus-off evidence (partialFailuresAndHistory, cutoffDoesNotEraseInvalidation).
// The functions below add what those do not reach.
void mixedAgeBlocks() {
    L::ReadContext c;
    CHECK(L::prepareState(c, target(), 1, 0, 10000, options(),
                          uint8_t(mask(L::StateBlock::ERRORS) | mask(L::StateBlock::STATUS))));
    accept(c, 0x80, 0);
    accept(c, 0x1234, 400); // Mixed ages inside the ERRORS block itself.
    accept(c, 0x8037, 500);
    accept(c, 0, 510);
    L::StateObservation s;
    s.monitor = 0xa55a;
    const Saved<L::StateObservation> old(s);
    // The older block expires on its oldest field, while the newer block stays current.
    CHECK(!L::getStateBlock(c, L::StateBlock::ERRORS, target(), 1000, 999, s));
    old.unchanged(s);
    CHECK(L::getStateBlock(c, L::StateBlock::STATUS, target(), 1000, 500, s));
    CHECK(s.statusword == 0x8037 && s.modeRaw == 0 && s.fieldCount == 2 && !s.interpreted);
    CHECK(s.window.earliestUs == 500 && s.window.latestUs == 510 && s.window.skewUs == 10);
    const Saved<L::StateObservation> status(s);
    CHECK(!L::getStateBlock(c, L::StateBlock::STATUS, target(), 1000, 499, s));
    status.unchanged(s);
    // Age is inclusive, and a block reports its own window rather than the whole operation's.
    CHECK(L::getStateBlock(c, L::StateBlock::ERRORS, target(), 1000, 1000, s));
    CHECK(s.errorRegister == 0x80 && s.errorCode == 0x1234);
    CHECK(s.window.earliestUs == 0 && s.window.latestUs == 400 && s.window.skewUs == 400);
}
void partialHistory() {
    // A count of three, then a lost second entry: nothing is published, and the prefix stays
    // readable only as history.
    L::ReadContext c;
    CHECK(L::prepareState(c, target(), 1, 0, 10000, options(),
                          mask(L::StateBlock::ERROR_HISTORY)));
    accept(c, 3, 0);
    accept(c, 0x80000001u, 1);
    C::SdoCompletion e = success(c, 0, 2, 3);
    CHECK(e.transfer.subindex == 2);
    e.outcome = C::SdoOutcome::TRANSPORT_ERROR;
    e.protocolAcknowledged = false;
    e.size = 0;
    e.effectUncertain = true;
    CHECK(L::advanceRead(c, e, 3));
    CHECK(c.state == M::ReadState::FAILED && c.uncertain);
    CHECK(c.values[0].raw == 3 && c.values[1].raw == 0x80000001u && !c.values[2].valid);
    L::StateObservation s;
    s.errorCount = 0x7f;
    const Saved<L::StateObservation> old(s);
    CHECK(!L::getStateBlock(c, L::StateBlock::ERROR_HISTORY, target(), 3, 100, s));
    old.unchanged(s);
    // Entries past the reported count are never requested, and read as zero rather than stale.
    L::ReadContext two;
    CHECK(L::prepareState(two, target(), 2, 0, 10000, options(),
                          mask(L::StateBlock::ERROR_HISTORY)));
    accept(two, 2, 0);
    accept(two, 0xa, 1);
    accept(two, 0xb, 2);
    CHECK(two.state == M::ReadState::SUCCEEDED && !two.values[3].valid && !two.values[4].valid);
    L::PreparedRead done;
    CHECK(L::nextRead(two, 2, done) && !done.ready);
    CHECK(L::getStateBlock(two, L::StateBlock::ERROR_HISTORY, target(), 2, 100, s));
    CHECK(s.errorCount == 2 && s.fieldCount == 3 && s.errorHistory[1] == 0xb);
    CHECK(s.errorHistory[2] == 0 && s.errorHistory[3] == 0);
}
void realLifecycleInvalidation() {
    // A completed read through the real client, then a real boot-up or configuration advance.
    for (unsigned cause = 0; cause < 2; ++cause) {
        C::SdoClient owner;
        configure(owner);
        uint64_t now = 0;
        L::ReadContext c;
        CHECK(L::prepareIdentity(c, target(), 1, 0, 10000, options()));
        runOwner(owner, c, identity, 9, now);
        L::IdentityObservation id;
        CHECK(L::getIdentity(c, target(), now, 1000, id));
        const Saved<L::IdentityObservation> good(id);
        L::ReadTarget current = target();
        if (cause == 0) {
            CHECK(owner.receive(frame(0x705, 1), now, now));
            CHECK(owner.node(0)->route.generation != current.nodeGeneration);
            current.nodeGeneration = owner.node(0)->route.generation;
        } else {
            M::Binding advanced = current.channel.binding;
            ++advanced.configurationGeneration;
            CHECK(owner.advanceConfiguration(0, current.channel.binding, advanced, now));
            current.channel.binding = advanced;
        }
        CHECK(!L::getIdentity(c, current, now, 1000, id));
        good.unchanged(id);
        CHECK(c.values[0].valid && c.values[0].raw == identity[0].raw); // History survives.
    }
    // Boot-up while an accepted upload is in flight: the terminal withdraws the earlier block too.
    C::SdoClient owner;
    configure(owner);
    uint64_t now = 0;
    L::ReadContext c;
    CHECK(L::prepareState(c, target(), 1, 0, 10000, options(),
                          uint8_t(mask(L::StateBlock::MONITORS) | mask(L::StateBlock::STATUS))));
    const uint8_t monitors[3][8] = {{0x4b, 0x00, 0x21, 0, 0x10, 0, 0, 0},
                                    {0x4b, 0x01, 0x21, 0, 0x80, 0xa5, 0, 0},
                                    {0x4b, 0x02, 0x21, 0, 0x08, 0xf0, 0, 0}};
    for (const auto& reply : monitors)
        CHECK(exchange(owner, c, reply, now).outcome == C::SdoOutcome::SUCCESS);
    L::StateObservation s;
    CHECK(L::getStateBlock(c, L::StateBlock::MONITORS, target(), now, 1000, s));
    const Saved<L::StateObservation> prior(s);
    C::SdoRequestId id;
    CHECK(owner.admit(0, next(c, now).transfer, now, id));
    CHECK(owner.service(now));
    M::Frame tx;
    C::SdoTxId txId;
    CHECK(owner.nextTx(0, now, tx, txId) && owner.confirmTx(txId, now));
    ++now;
    CHECK(owner.receive(frame(0x705, 1), now, now));
    const C::SdoCompletion* lost = owner.result(id);
    CHECK(lost && lost->outcome == C::SdoOutcome::REBOOT && lost->effectUncertain);
    CHECK(L::advanceRead(c, *lost, now));
    CHECK(c.state == M::ReadState::FAILED && c.outcome == M::ReadOutcome::INVALIDATED);
    CHECK(c.invalidated && c.uncertain && c.values[0].valid);
    CHECK(!L::getStateBlock(c, L::StateBlock::MONITORS, target(), now, 1000, s));
    prior.unchanged(s);
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
    {
        // Values are written in place, so a forged route missing the second selected field must
        // refuse before the first is written. Every copy of the route agrees, to get that far.
        L::ReadContext forged = c;
        C::PdoRecord record = valid;
        C::PdoRoute route = c.pdoRoute;
        for (unsigned j = 0; j < route.layout.mapping.count; ++j)
            if (route.layout.fields[j].index == 0x606c) {
                route.layout.fields[j].index = 0x6041;
                route.layout.mapping.words[j] = (uint32_t(0x6041) << 16) | (route.layout.mapping.words[j] & 0xffffu);
            }
        forged.pdoRoute = route;
        record.map.route = route;
        record.qualification.route = route;
        record.observation.sample.route = route;
        const Saved<L::ReadContext> kept(forged);
        const M::Status status = L::advanceRead(forged, record, 3);
        CHECK(!status && !std::strcmp(status.msg, "missing selected PDO field"));
        kept.unchanged(forged);
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
// A count the read's own checks would have failed still cannot publish a block.
void forgedHistoryCountRefusesAndPublishesNothing() {
    L::ReadContext c;
    CHECK(L::prepareState(c, target(), 1, 0, 10000, options(), mask(L::StateBlock::ERROR_HISTORY)));
    accept(c, 4, 0);
    for (unsigned i = 1; i <= 4; ++i)
        accept(c, 0x80000000u + i, i);
    CHECK(c.state == M::ReadState::SUCCEEDED);
    c.values[0].raw = 5;
    L::StateObservation s;
    s.errorCount = 0x7f;
    const Saved<L::StateObservation> old(s);
    L::ReadWindow window;
    window.skewUs = 123;
    const M::Status status = L::getStateBlock(c, L::StateBlock::ERROR_HISTORY, target(), 5, 100, s);
    CHECK(!status && status.detail == int32_t(M::ReadError::VALUE));
    old.unchanged(s);
    CHECK(!L::validateStateBlock(c, L::StateBlock::ERROR_HISTORY, target(), 5, 100, window));
    CHECK(window.skewUs == 123);
    c.values[0].raw = 4;
    CHECK(L::validateStateBlock(c, L::StateBlock::ERROR_HISTORY, target(), 5, 100, window));
    CHECK(window.earliestUs == 0 && window.latestUs == 4 && window.skewUs == 4);
    CHECK(L::getStateBlock(c, L::StateBlock::ERROR_HISTORY, target(), 5, 100, s) && s.errorCount == 4);
    CHECK(s.window.earliestUs == window.earliestUs && s.window.latestUs == window.latestUs);
}
// validateRead gives prepare's status without a context, and resetRead returns one to EMPTY.
void validateReadMatchesPreparation() {
    L::ReadContext c;
    const Saved<L::ReadContext> empty(c);
    L::ReadOptions measured = options();
    measured.identityWidths = L::IdentityWidths::MEASURED_U8;
    CHECK(L::validateRead(L::ReadKind::IDENTITY, target(), 1, 0, 100, options()));
    CHECK(L::validateRead(L::ReadKind::IDENTITY, target(), 1, 0, 100, measured));
    CHECK(L::validateRead(L::ReadKind::STATE, target(), 1, 0, 100, options(), mask(L::StateBlock::STATUS)));
    const M::Status width = L::validateRead(L::ReadKind::STATE, target(), 1, 0, 100, measured,
                                            mask(L::StateBlock::STATUS));
    const M::Status prepared = L::prepareState(c, target(), 1, 0, 100, measured, mask(L::StateBlock::STATUS));
    CHECK(!width && !prepared && width.detail == prepared.detail && width.code == prepared.code);
    empty.unchanged(c);
    CHECK(!L::validateRead(L::ReadKind::STATE, target(), 1, 0, 100, options()));
    CHECK(!L::validateRead(L::ReadKind::STATE, target(), 1, 0, 100, options(), 0x40));
    CHECK(!L::validateRead(L::ReadKind::CONFIG, target(), 0, 0, 100, options()));
    CHECK(!L::validateRead(L::ReadKind::CONFIG, target(), 1, 100, 100, options()));
    CHECK(!L::validateRead(L::ReadKind::CONFIG, L::ReadTarget(), 1, 0, 100, options()));
    CHECK(L::prepareConfig(c, target(), 1, 0, 100, options()));
    L::resetRead(c);
    CHECK(c.state == M::ReadState::EMPTY && !c.count && !c.operation);
}
} // namespace
int main() {
    allRowsAndRealOwner();
    preparationAndGetters();
    wrongServiceAndTime();
    partialFailuresAndHistory();
    cutoffDoesNotEraseInvalidation();
    serviceTimeoutIsNotTheDeadline();
    lostCorrelationWithdrawsButKeepsHistory();
    passiveProvenance();
    acceptedListIsExact();
    probeIsOneAcceptedUpload();
    acceptedEffectsStayUnknown();
    identityWidthVariants();
    identityWidthOptionRejectsAtomically();
    countWidthVariants();
    countWidthOptionRejectsAtomically();
    mixedAgeBlocks();
    partialHistory();
    realLifecycleInvalidation();
    forgedHistoryCountRefusesAndPublishesNothing();
    validateReadMatchesPreparation();
    std::printf("reads: %u checks passed\n", checks);
    return 0;
}
