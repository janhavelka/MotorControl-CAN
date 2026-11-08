// SPDX-License-Identifier: MIT
// Width/access oracle transcribed from LC_StepDriverCAN_V1.1.eds [1400sub0]..[1A03sub4]
// and [2203]. Values and owner timing below are synthetic adversarial fixtures, not measurements.
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
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
constexpr uint64_t DEADLINE = 10000;
using G = L::CommunicationGroup;
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
L::ReadTarget target() {
    L::ReadTarget out;
    out.channel = OwnerRig::route();
    out.nodeGeneration = 11;
    return out;
}
L::ReadOptions options(unsigned pdo = 0) {
    L::ReadOptions out;
    out.queueTimeoutUs = out.responseTimeoutUs = 100;
    out.pdoNumber = uint8_t(pdo);
    return out;
}
bool isPdo(G group) {
    return group >= G::RPDO_COMMUNICATION && group <= G::TPDO_MAPPING;
}
bool isMapping(G group) {
    return group == G::RPDO_MAPPING || group == G::TPDO_MAPPING;
}
struct Field {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t raw;
};
unsigned fields(G group, unsigned pdo, Field (&out)[6]) {
    if (group == G::NETWORK) {
        const Field network[] = {{0x1005, 0, 4, 0x40000080}, {0x1014, 0, 4, 0x85},
                                 {0x1017, 0, 2, 0},          {0x1200, 0, 1, 2},
                                 {0x1200, 1, 4, 0x601},      {0x1200, 2, 4, 0x581}};
        std::memcpy(out, network, sizeof network);
        return 6;
    }
    if (group == G::NODE_SETTING) {
        out[0] = {0x2203, 0, 2, 0xffff};
        return 1;
    }
    const uint16_t base = group == G::RPDO_COMMUNICATION   ? 0x1400
                          : group == G::TPDO_COMMUNICATION ? 0x1800
                          : group == G::RPDO_MAPPING       ? 0x1600
                                                           : 0x1a00;
    const uint16_t index = uint16_t(base + pdo - 1);
    if (isMapping(group)) {
        // Even count zero and invalid count 255 must retain every inactive slot.
        const uint8_t counts[] = {0, 4, 255, 1};
        const uint32_t raw[] = {0, 0x80000000u, 0xffffffffu, 0x60410010u};
        out[0] = {index, 0, 1, counts[pdo - 1]};
        for (unsigned i = 1; i < 5; ++i)
            out[i] = {index, uint8_t(i), 4, raw[i - 1]};
        return 5;
    }
    const uint8_t widths[] = {1, 4, 1, 2, 1, 2};
    const uint32_t raw[] = {255, 0xffffffffu, 253, 0xffff, 0xa5, 0x8000};
    for (unsigned i = 0; i < 6; ++i)
        out[i] = {index, uint8_t(i), widths[i], raw[i]};
    return 6;
}
// Refer to each generated declaration, but determine expected access/type independently above.
#define ROW(i, s) &L::OBJECT_##i##_##s
#define RECORD5(i) ROW(i, 00), ROW(i, 01), ROW(i, 02), ROW(i, 03), ROW(i, 04)
#define RECORD6(i) RECORD5(i), ROW(i, 05)
const L::ObjectDescriptor* const declarations[] = {
    RECORD6(1400), RECORD6(1401), RECORD6(1402), RECORD6(1403), RECORD6(1800), RECORD6(1801),
    RECORD6(1802), RECORD6(1803), RECORD5(1600), RECORD5(1601), RECORD5(1602), RECORD5(1603),
    RECORD5(1A00), RECORD5(1A01), RECORD5(1A02), RECORD5(1A03), ROW(2203, 00)};
#undef RECORD6
#undef RECORD5
#undef ROW
void descriptorOracle() {
    unsigned position = 0;
    for (G group : {G::RPDO_COMMUNICATION, G::TPDO_COMMUNICATION, G::RPDO_MAPPING, G::TPDO_MAPPING,
                    G::NODE_SETTING}) {
        for (unsigned pdo = isPdo(group) ? 1 : 0; pdo <= (isPdo(group) ? 4u : 0u); ++pdo) {
            Field expected[6];
            const unsigned count = fields(group, pdo, expected);
            for (unsigned i = 0; i < count; ++i) {
                const Field& f = expected[i];
                CHECK(position < sizeof declarations / sizeof declarations[0]);
                const L::ObjectDescriptor& d = *declarations[position++];
                CHECK(d.index == f.index && d.subindex == f.subindex && d.widthBytes == f.width);
                CHECK(d.type == (f.width == 1   ? L::ScalarType::U8
                                 : f.width == 2 ? L::ScalarType::U16
                                                : L::ScalarType::U32));
                CHECK(!d.signedValue);
                // EDS labels compatibility RW; reviewed policy forbids a setter without semantics.
                CHECK(d.sdoAccess == (!isMapping(group) && isPdo(group) && (i == 0 || i == 4)
                                          ? L::SdoAccess::READ_ONLY
                                          : L::SdoAccess::READ_WRITE));
                CHECK(d.readEffect == L::ReadEffect::UNQUALIFIED);
                CHECK(d.pdoPolicy == (group == G::NODE_SETTING ? L::PdoPolicy::UNQUALIFIED
                                                               : L::PdoPolicy::FORBIDDEN));
                CHECK(d.scope == L::SourceScope::EDS_FAMILY_UNBOUND);
                L::ReadDescriptor read = {};
                CHECK(L::readDescriptor(f.index, f.subindex, read));
                CHECK(read.widthBytes == f.width &&
                      read.representation == L::ReadRepresentation::UNSIGNED);
                CHECK(read.readEffect == L::ReadEffect::UNQUALIFIED && read.scope == d.scope);
            }
        }
    }
    CHECK(position == 89);
    L::ReadDescriptor d = {};
    d.index = 0xbeef;
    const Saved<L::ReadDescriptor> saved(d);
    for (const Field& f : {Field{0x1404, 0, 1, 0}, Field{0x1800, 6, 1, 0}, Field{0x1600, 5, 4, 0},
                           Field{0x1a04, 0, 1, 0}, Field{0x2203, 1, 2, 0}}) {
        CHECK(!L::readDescriptor(f.index, f.subindex, d));
        saved.unchanged(d);
    }
}
struct Bench {
    C::SdoClient services;
    OwnerRig::WireBase wire;
    O::CanBusOwner owner;
    O::SequenceId sequence;
    uint64_t now = 0;
    uint32_t operation = 0, authorization = 0;
    explicit Bench(bool confirmed = true) : owner(services, wire.plumbing(), 73) {
        CHECK(services.configure(0, target().channel, confirmed, true));
        C::NetworkRoute network;
        network.binding = target().channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        CHECK(owner.beginSequence(target().channel.binding, 1, now, sequence));
    }
    L::CommunicationReadContext prepare(G group, unsigned pdo = 0) {
        L::CommunicationReadContext out;
        CHECK(owner.beginOperation(sequence, ++operation, DEADLINE, now));
        CHECK(L::prepareCommunicationRead(out, target(), operation, now, DEADLINE, options(pdo),
                                          group));
        return out;
    }
    void turn(uint64_t when) {
        now = when;
        CHECK(owner.service(now, OwnerRig::budget()));
    }
    L::PreparedRead next(const L::CommunicationReadContext& c) {
        L::PreparedRead out;
        CHECK(L::nextCommunication(c, now, out) && out.ready);
        return out;
    }
    O::RequestId admit(const C::SdoTransfer& transfer, bool grant = true) {
        CHECK(transfer.direction == C::SdoDirection::UPLOAD);
        CHECK(transfer.readEffect == C::SdoReadEffect::UNQUALIFIED);
        if (grant)
            CHECK(services.authorizeUploadExperiment(0, transfer, 11, ++authorization, now));
        O::BusRequest request;
        request.sequence = sequence;
        request.transfer = transfer;
        O::RequestId out;
        CHECK(owner.admit(request, now, out) == O::BusAdmission::ACCEPTED);
        return out;
    }
    M::Frame reply(const Field& f, bool abort = false) {
        CHECK(f.width == 1 || f.width == 2 || f.width == 4);
        M::Frame out = OwnerRig::frame(0x581, 8);
        out.data[0] = abort ? 0x80 : f.width == 1 ? 0x4f : f.width == 2 ? 0x4b : 0x43;
        out.data[1] = uint8_t(f.index);
        out.data[2] = uint8_t(f.index >> 8);
        out.data[3] = f.subindex;
        const uint32_t raw = abort ? 0x80000042u : f.raw;
        for (unsigned i = 0; i < 4 && i < (abort ? 4u : f.width); ++i)
            out.data[4 + i] = uint8_t(raw >> (8 * i));
        return out;
    }
    C::SdoCompletion terminal(O::RequestId id) {
        const O::Completion* done = owner.result(id);
        CHECK(done);
        const C::SdoCompletion out = done->service;
        CHECK(owner.release(id));
        return out;
    }
    C::SdoCompletion exchange(L::CommunicationReadContext& c, const Field& f, bool abort = false) {
        const L::PreparedRead work = next(c);
        CHECK(work.transfer.index == f.index && work.transfer.subindex == f.subindex &&
              work.transfer.size == f.width);
        // The new 89 rows need their own review even on an already confirmed service route.
        const bool reviewedAcquisition =
            f.index == 0x2203 || (f.index >= 0x1400 && f.index <= 0x1403) ||
            (f.index >= 0x1600 && f.index <= 0x1603) || (f.index >= 0x1800 && f.index <= 0x1803) ||
            (f.index >= 0x1a00 && f.index <= 0x1a03);
        CHECK(work.transfer.requireUploadReview == reviewedAcquisition);
        if (reviewedAcquisition) {
            CHECK(!services.checkAdmission(0, work.transfer, now));
            O::BusRequest unreviewed;
            unreviewed.sequence = sequence;
            unreviewed.transfer = work.transfer;
            O::RequestId rejected;
            CHECK(owner.admit(unreviewed, now, rejected) == O::BusAdmission::INVALID);
        }
        const O::RequestId id = admit(work.transfer);
        const unsigned sent = wire.sentCount;
        turn(now);
        CHECK(wire.sentCount == sent + 1);
        const M::Frame& frame = wire.sent[sent];
        CHECK(frame.id == 0x601 && frame.data[0] == 0x40 && frame.dlc == 8);
        CHECK(frame.data[1] == uint8_t(f.index) && frame.data[2] == uint8_t(f.index >> 8) &&
              frame.data[3] == f.subindex);
        for (unsigned i = 4; i < 8; ++i)
            CHECK(frame.data[i] == 0);
        wire.push(reply(f, abort), now + 1);
        turn(now + 1);
        return terminal(id);
    }
    void run(L::CommunicationReadContext& c, G group, unsigned pdo = 0) {
        Field expected[6];
        const unsigned count = fields(group, pdo, expected);
        for (unsigned i = 0; i < count; ++i) {
            const C::SdoCompletion done = exchange(c, expected[i]);
            CHECK(done.outcome == C::SdoOutcome::SUCCESS && done.effectPossible &&
                  !done.effectUncertain);
            CHECK(L::advanceCommunication(c, done, now));
            CHECK(c.read.values[i].valid && c.read.values[i].raw == expected[i].raw);
        }
        CHECK(c.read.state == M::ReadState::SUCCEEDED && c.read.outcome == M::ReadOutcome::SUCCESS);
    }
};
void checkPublication(Bench& b, const L::CommunicationReadContext& c, G group, unsigned pdo) {
    Field expected[6];
    const unsigned count = fields(group, pdo, expected);
    L::CommunicationObservation out;
    CHECK(L::getCommunication(c, target(), b.now, 1000, out));
    CHECK(out.group == group && out.pdoNumber == pdo && out.fieldCount == count);
    CHECK(out.window.earliestUs == c.read.startedUs && out.window.latestUs == b.now);
    CHECK(out.window.skewUs == b.now - c.read.startedUs);
    for (unsigned i = 0; i < count; ++i) {
        CHECK(out.fields[i].index == expected[i].index &&
              out.fields[i].subindex == expected[i].subindex);
        CHECK(out.fields[i].earliestUs == c.read.startedUs + i &&
              out.fields[i].latestUs == c.read.startedUs + i + 1);
        CHECK(out.fields[i].nodeGeneration == 11 && out.fields[i].source == L::ReadSource::SDO);
        CHECK(!out.fields[i].captureKnown && !out.fields[i].mappingGeneration &&
              !out.fields[i].evidenceId);
        CHECK(M::sameBinding(out.fields[i].binding, target().channel.binding));
    }
    if (group == G::NODE_SETTING)
        CHECK(out.nodeSettingRaw == 0xffff && out.target.channel.binding.endpoint.address == 1);
    else if (group == G::NETWORK) {
        CHECK(out.network.syncCobId == expected[0].raw && out.network.emcyCobId == 0x85);
        CHECK(out.network.producerTimeMs == 0 && out.network.sdoEntries == 2);
        CHECK(out.network.clientToServer == 0x601 && out.network.serverToClient == 0x581);
    } else if (isMapping(group)) {
        CHECK(out.mapping.countRaw == expected[0].raw);
        for (unsigned i = 0; i < 4; ++i)
            CHECK(out.mapping.words[i] == expected[i + 1].raw);
    } else {
        CHECK(out.communication.entriesRaw == 255 && out.communication.cobIdRaw == 0xffffffffu);
        CHECK(out.communication.transmissionTypeRaw == 253 &&
              out.communication.inhibitTimeRaw == 0xffff);
        CHECK(out.communication.compatibilityRaw == 0xa5 &&
              out.communication.eventTimerRaw == 0x8000);
    }
    const L::ReadProvenance original = out.fields[0];
    CHECK(L::getCommunication(c, target(), b.now + 1, 1000, out));
    CHECK(out.fields[0].earliestUs == original.earliestUs &&
          out.fields[0].latestUs == original.latestUs &&
          out.fields[0].deliveredUs == original.deliveredUs);
}
void allRowsThroughOwner() {
    Bench b;
    for (G group : {G::NETWORK, G::NODE_SETTING, G::RPDO_COMMUNICATION, G::TPDO_COMMUNICATION,
                    G::RPDO_MAPPING, G::TPDO_MAPPING})
        for (unsigned pdo = isPdo(group) ? 1 : 0; pdo <= (isPdo(group) ? 4u : 0u); ++pdo) {
            L::CommunicationReadContext c = b.prepare(group, pdo);
            b.run(c, group, pdo);
            checkPublication(b, c, group, pdo);
            L::PreparedRead work;
            work.ready = true;
            work.transfer.index = 0xbeef;
            CHECK(L::nextCommunication(c, b.now, work) && !work.ready &&
                  work.transfer.index == 0xbeef);
        }
    CHECK(b.wire.sentCount == 95);
    CHECK(!b.services.pdo(0)); // Raw records never install/qualify a map.
    C::ServiceEligibility eligibility;
    CHECK(C::serviceEligibility(*b.services.node(0), b.now, eligibility));
    CHECK(eligibility.ordinarySdo && !eligibility.exactNmtKnown && !eligibility.pdoTransmit);
}
void preparationAndGetters() {
    Bench b;
    L::CommunicationReadContext c = b.prepare(G::RPDO_MAPPING, 1);
    const Saved<L::CommunicationReadContext> before(c);
    for (unsigned variation = 0; variation < 17; ++variation) {
        L::ReadTarget t = target();
        L::ReadOptions o = options(1);
        G group = G::RPDO_MAPPING;
        uint32_t operation = 5;
        uint64_t deadline = DEADLINE;
        switch (variation) {
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
            t.channel.binding.endpoint.address = 0;
            break;
        case 4:
            t.channel.binding.endpoint.address = 128;
            break;
        case 5:
            t.channel.binding.endpoint.protocol = M::Protocol::PROPRIETARY;
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
        case 11:
            o.pdoNumber = 0;
            break;
        case 12:
            o.pdoNumber = 5;
            break;
        case 13:
            o.pdoNumber = 255;
            break;
        case 14:
            group = static_cast<G>(255);
            break;
        case 15:
            o.identityWidths = L::IdentityWidths::MEASURED_U8;
            break;
        case 16:
            o.countWidths = L::CountWidths::MEASURED_U16;
            break;
        }
        CHECK(!L::prepareCommunicationRead(c, t, operation, 0, deadline, o, group));
        before.unchanged(c);
    }
    for (G group : {G::NETWORK, G::NODE_SETTING}) {
        CHECK(!L::prepareCommunicationRead(c, target(), 5, 0, DEADLINE, options(1), group));
        before.unchanged(c);
    }
    CHECK(!L::prepareCommunicationRead(c, c.read.target, 5, 0, DEADLINE, options(1),
                                       G::RPDO_MAPPING));
    before.unchanged(c);
    L::CommunicationObservation out;
    out.nodeSettingRaw = 0xa55a;
    const Saved<L::CommunicationObservation> untouched(out);
    CHECK(!L::getCommunication(c, target(), 0, 1000, out));
    untouched.unchanged(out);
    b.run(c, G::RPDO_MAPPING, 1);
    for (unsigned variation = 0; variation < 13; ++variation) {
        L::ReadTarget t = target();
        switch (variation) {
        case 0:
            ++t.nodeGeneration;
            break;
        case 1:
            ++t.channel.binding.bindingGeneration;
            break;
        case 2:
            ++t.channel.binding.configurationGeneration;
            break;
        case 3:
            ++t.channel.binding.logicalAxisId;
            break;
        case 4:
            ++t.channel.binding.endpoint.address;
            break;
        case 5:
            ++t.channel.binding.endpoint.bus;
            break;
        case 6:
            ++t.channel.binding.endpoint.channel;
            break;
        case 7:
            ++t.channel.binding.endpoint.subaxis;
            break;
        case 8:
            ++t.channel.binding.endpoint.profile;
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
        CHECK(!L::getCommunication(c, t, b.now, 1000, out));
        untouched.unchanged(out);
    }
    CHECK(!L::getCommunication(c, target(), b.now, 0, out));
    untouched.unchanged(out);
    CHECK(!L::getCommunication(c, target(), b.now, b.now - 1, out));
    untouched.unchanged(out);
    CHECK(!L::getCommunication(c, target(), b.now - 1, 1000, out));
    untouched.unchanged(out);
    L::CommunicationReadContext wrong;
    CHECK(L::prepareProbe(wrong.read, target(), 9, 0, DEADLINE, options()));
    CHECK(!L::getCommunication(wrong, target(), b.now, 1000, out));
    untouched.unchanged(out);
    L::PreparedRead work;
    work.transfer.index = 0xbeef;
    const Saved<L::PreparedRead> noWork(work);
    CHECK(!L::nextCommunication(wrong, b.now, work));
    noWork.unchanged(work);
    L::CommunicationReadContext invalidated = c;
    invalidated.read.invalidated = true;
    CHECK(!L::getCommunication(invalidated, target(), b.now, 1000, out));
    untouched.unchanged(out);
}
void correlationAndBorrowedReplies() {
    Bench b;
    L::CommunicationReadContext c = b.prepare(G::TPDO_COMMUNICATION, 4);
    Field expected[6];
    fields(G::TPDO_COMMUNICATION, 4, expected);
    const C::SdoCompletion good = b.exchange(c, expected[0]);
    const Saved<L::CommunicationReadContext> saved(c);
    for (unsigned variation = 0; variation < 18; ++variation) {
        C::SdoCompletion bad = good;
        switch (variation) {
        case 0:
            ++bad.transfer.operation;
            break;
        case 1:
            ++bad.transfer.request;
            break;
        case 2:
            ++bad.transfer.step;
            break;
        case 3:
            ++bad.transfer.index;
            break;
        case 4:
            ++bad.transfer.subindex;
            break;
        case 5:
            ++bad.transfer.size;
            break;
        case 6:
            ++bad.transfer.channel.clientToServerId;
            break;
        case 7:
            ++bad.transfer.channel.serverToClientId;
            break;
        case 8:
            ++bad.transfer.channel.binding.bindingGeneration;
            break;
        case 9:
            ++bad.transfer.channel.binding.configurationGeneration;
            break;
        case 10:
            ++bad.nodeGeneration;
            break;
        case 11:
            --bad.size;
            break;
        case 12:
            bad.protocolAcknowledged = false;
            break;
        case 13:
            bad.transfer.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
            break;
        case 14:
            bad.completedUs = b.now + 1;
            break;
        case 15:
            ++bad.transfer.deadlineUs;
            break;
        case 16:
            bad.transfer.direction = C::SdoDirection::DOWNLOAD;
            break;
        case 17:
            bad.transfer.requireUploadReview = false;
            break;
        }
        CHECK(!L::advanceCommunication(c, bad, b.now));
        saved.unchanged(c);
    }
    C::SdoCompletion borrowed = good;
    CHECK(L::advanceCommunication(c, borrowed, b.now));
    borrowed.data[0] = 0;
    CHECK(c.read.values[0].raw == 255 && c.read.evidence.data[0] == 255);
    const Saved<L::CommunicationReadContext> progressed(c);
    CHECK(!L::advanceCommunication(c, good, b.now));
    progressed.unchanged(c);
}
void confirmedAndUnconfirmedReviewGrants() {
    for (bool confirmed : {false, true}) {
        for (unsigned mismatch = 0; mismatch < 2; ++mismatch) {
            // Neither another object nor a grant with the mandatory review flag removed matches.
            Bench b(confirmed);
            L::CommunicationReadContext c = b.prepare(G::RPDO_MAPPING, 1);
            const L::PreparedRead work = b.next(c);
            CHECK(work.transfer.requireUploadReview);
            C::SdoTransfer different = work.transfer;
            if (mismatch == 0)
                ++different.subindex;
            else
                different.requireUploadReview = false;
            CHECK(b.services.authorizeUploadExperiment(0, different, 11, ++b.authorization, b.now));
            CHECK(!b.services.checkAdmission(0, work.transfer, b.now));
            O::BusRequest request;
            request.sequence = b.sequence;
            request.transfer = work.transfer;
            O::RequestId rejected;
            CHECK(b.owner.admit(request, b.now, rejected) == O::BusAdmission::INVALID);
            b.turn(0);
            CHECK(b.wire.sentCount == 0);
        }
        { // This flag cannot be repurposed as a permission for a download.
            Bench b(confirmed);
            L::CommunicationReadContext c = b.prepare(G::NODE_SETTING);
            C::SdoTransfer download = b.next(c).transfer;
            CHECK(download.requireUploadReview);
            download.direction = C::SdoDirection::DOWNLOAD;
            CHECK(!b.services.checkAdmission(0, download, b.now));
            CHECK(
                !b.services.authorizeDownloadExperiment(0, download, 11, ++b.authorization, b.now));
            O::BusRequest request;
            request.sequence = b.sequence;
            request.transfer = download;
            O::RequestId rejected;
            CHECK(b.owner.admit(request, b.now, rejected) == O::BusAdmission::INVALID);
            b.turn(0);
            CHECK(b.wire.sentCount == 0);
        }
        { // Consumption cannot leave a permission behind on a confirmed channel.
            Bench b(confirmed);
            L::CommunicationReadContext c = b.prepare(G::NODE_SETTING);
            const L::PreparedRead work = b.next(c);
            CHECK(work.transfer.requireUploadReview);
            Field expected[6];
            fields(G::NODE_SETTING, 0, expected);
            const C::SdoCompletion done = b.exchange(c, expected[0]);
            CHECK(L::advanceCommunication(c, done, b.now));
            CHECK(!b.services.checkAdmission(0, work.transfer, b.now));
            CHECK(!b.services.authorizeUploadExperiment(0, work.transfer, 11, ++b.authorization,
                                                        b.now));
            O::BusRequest replay;
            replay.sequence = b.sequence;
            replay.transfer = work.transfer;
            O::RequestId rejected;
            CHECK(b.owner.admit(replay, b.now, rejected) != O::BusAdmission::ACCEPTED);
            b.turn(b.now + 1);
            CHECK(b.wire.sentCount == 1);
        }
    }
}
void grantAndStateRules() {
    Bench b(false);
    L::CommunicationReadContext c = b.prepare(G::NODE_SETTING);
    const L::PreparedRead work = b.next(c);
    O::BusRequest request;
    request.sequence = b.sequence;
    request.transfer = work.transfer;
    O::RequestId rejected;
    CHECK(b.owner.admit(request, b.now, rejected) == O::BusAdmission::INVALID);
    CHECK(b.wire.sentCount == 0 && !b.services.node(0)->nmtTime.present);
    CHECK(b.services.authorizeUploadExperiment(0, work.transfer, 11, ++b.authorization, b.now));
    ++request.transfer.subindex;
    CHECK(b.owner.admit(request, b.now, rejected) == O::BusAdmission::INVALID);
    request.transfer = work.transfer;
    const O::RequestId id = b.admit(work.transfer, false);
    b.turn(0);
    // A reply at a guessed new node is not evidence of this configured old route.
    Field expected[6];
    fields(G::NODE_SETTING, 0, expected);
    M::Frame wrong = b.reply(expected[0]);
    wrong.id = 0x582;
    b.wire.push(wrong, 1);
    b.turn(1);
    CHECK(!b.owner.result(id));
    wrong = b.reply(expected[0]);
    ++wrong.data[1];
    b.wire.push(wrong, 2);
    b.turn(2);
    CHECK(!b.owner.result(id));
    wrong = b.reply(expected[0]);
    ++wrong.data[3];
    b.wire.push(wrong, 3);
    b.turn(3);
    CHECK(!b.owner.result(id));
    b.wire.push(b.reply(expected[0]), 4);
    b.turn(4);
    const C::SdoCompletion done = b.terminal(id);
    CHECK(L::advanceCommunication(c, done, b.now));
    CHECK(!b.services.authorizeUploadExperiment(0, work.transfer, 11, ++b.authorization, b.now));
    L::CommunicationObservation out;
    CHECK(L::getCommunication(c, target(), 4, 100, out));
    CHECK(out.nodeSettingRaw == 65535 && out.target.channel.serverToClientId == 0x581);
    CHECK(b.wire.sentCount == 1);
    for (uint8_t state : {uint8_t(4), uint8_t(0)}) {
        Bench stopped;
        L::CommunicationReadContext pending = stopped.prepare(G::RPDO_MAPPING, 1);
        M::Frame heartbeat = OwnerRig::frame(0x701, 1);
        heartbeat.data[0] = state;
        stopped.wire.push(heartbeat, 1);
        stopped.turn(1);
        const L::PreparedRead next = stopped.next(pending);
        CHECK(!stopped.services.authorizeUploadExperiment(0, next.transfer, 11, 1, 1));
        CHECK(stopped.wire.sentCount == 0);
    }
}
void exactWidthsAndStoredNodeValues() {
    for (uint32_t raw : {0u, 127u, 128u, 65535u}) {
        Bench b;
        L::CommunicationReadContext c = b.prepare(G::NODE_SETTING);
        const Field value = {0x2203, 0, 2, raw};
        const C::SdoCompletion done = b.exchange(c, value);
        CHECK(L::advanceCommunication(c, done, b.now));
        L::CommunicationObservation out;
        CHECK(L::getCommunication(c, target(), b.now, 100, out));
        CHECK(out.nodeSettingRaw == raw && out.target.channel.binding.endpoint.address == 1);
        CHECK(b.wire.sentCount == 1 && !b.services.pdo(0));
    }
    // A size-indicated U16 answer cannot satisfy an EDS U8 count. Do not retry another width.
    Bench b;
    L::CommunicationReadContext c = b.prepare(G::TPDO_MAPPING, 1);
    const O::RequestId id = b.admit(b.next(c).transfer);
    b.turn(0);
    const Field wrong = {0x1a00, 0, 2, 3};
    b.wire.push(b.reply(wrong), 1);
    b.turn(1);
    const C::SdoCompletion done = b.terminal(id);
    CHECK(done.outcome == C::SdoOutcome::CLIENT_ABORT && done.effectUncertain);
    CHECK(L::advanceCommunication(c, done, b.now));
    CHECK(c.read.outcome == M::ReadOutcome::REPLY_ERROR && c.read.uncertain);
    const unsigned sent = b.wire.sentCount;
    b.turn(2);
    for (unsigned i = sent; i < b.wire.sentCount; ++i)
        CHECK(b.wire.sent[i].data[0] == 0x80); // Bounded protocol cleanup, never another upload.
}
void partialAndUnknownEffects() {
    for (unsigned failure = 0; failure < 4; ++failure) {
        Bench b;
        L::CommunicationReadContext c = b.prepare(G::RPDO_COMMUNICATION, 1);
        Field expected[6];
        fields(G::RPDO_COMMUNICATION, 1, expected);
        for (unsigned i = 0; i < 2; ++i) {
            const C::SdoCompletion done = b.exchange(c, expected[i]);
            CHECK(L::advanceCommunication(c, done, b.now));
        }
        C::SdoCompletion done;
        if (failure == 0)
            done = b.exchange(c, expected[2], true);
        else {
            const O::RequestId id = b.admit(b.next(c).transfer);
            b.turn(b.now);
            if (failure == 1)
                b.turn(b.now + 100); // Lost reply/route silence does not become a guessed retry.
            else if (failure == 2) {
                const M::Frame boot = OwnerRig::frame(0x701, 1);
                b.wire.push(boot, b.now + 1);
                b.turn(b.now + 1);
            } else
                CHECK(b.owner.cancel(id, b.now) == O::Cancel::CANCELLED);
            done = b.terminal(id);
        }
        CHECK(L::advanceCommunication(c, done, b.now));
        CHECK(c.read.state == M::ReadState::FAILED && c.read.values[0].valid &&
              c.read.values[1].valid);
        CHECK(c.read.values[0].raw == 255 && c.read.values[1].raw == 0xffffffffu &&
              !c.read.values[2].valid);
        CHECK(c.read.evidencePresent && c.read.evidence.transfer.subindex == 2);
        if (failure == 0)
            CHECK(c.read.evidence.abortCode == 0x80000042u &&
                  c.read.outcome == M::ReadOutcome::PROTOCOL_REJECTED);
        else
            CHECK(c.read.uncertain && c.read.evidence.effectUncertain && b.services.unresolved(0));
        L::CommunicationObservation out;
        out.nodeSettingRaw = 0xbeef;
        const Saved<L::CommunicationObservation> saved(out);
        CHECK(!L::getCommunication(c, target(), b.now, 1000, out));
        saved.unchanged(out);
        L::PreparedRead next;
        CHECK(L::nextCommunication(c, b.now, next) && !next.ready);
        const unsigned sent = b.wire.sentCount;
        b.turn(b.now + 1);
        CHECK(b.wire.sentCount == sent);
        for (unsigned i = 0; i < sent; ++i)
            CHECK(b.wire.sent[i].id == 0x601 &&
                  (b.wire.sent[i].data[0] == 0x40 || b.wire.sent[i].data[0] == 0x80));
    }
    Bench b;
    L::CommunicationReadContext c = b.prepare(G::TPDO_MAPPING, 1);
    CHECK(L::advanceCommunication(c, M::ReadOutcome::CANCELLED, 0));
    // A local reason supplies no owner retirement evidence, so the read keeps uncertainty.
    CHECK(c.read.outcome == M::ReadOutcome::CANCELLED && c.read.uncertain && b.wire.sentCount == 0);
}
} // namespace
int main() {
    descriptorOracle();
    allRowsThroughOwner();
    preparationAndGetters();
    correlationAndBorrowedReplies();
    grantAndStateRules();
    confirmedAndUnconfirmedReviewGrants();
    exactWidthsAndStoredNodeValues();
    partialAndUnknownEffects();
    std::printf("communication: %u checks\n", checks);
    return 0;
}
