// SPDX-License-Identifier: MIT
// Literal baseline/command vectors and adversarial owner terminals exercise the public core.
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
unsigned checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(std::memcmp(bytes, &value, sizeof value) == 0); }
};
const uint32_t baselineValues[9] = {0x181, 255, 100, 0, 3, 0x60410010, 0x60610008, 0x60640020, 0};
const uint32_t finalValues[9] = {0x181, 255, 100, 0, 2, 0x60410010, 0x60640020, 0x60640020, 0};
const uint16_t readIndex[9] = {0x1800, 0x1800, 0x1800, 0x1800, 0x1a00,
                               0x1a00, 0x1a00, 0x1a00, 0x1a00};
const uint8_t readSub[9] = {1, 2, 3, 5, 0, 1, 2, 3, 4};
const uint8_t readWidth[9] = {4, 1, 2, 2, 1, 4, 4, 4, 4};
const uint8_t writes[6][8] = {{0x23, 0x00, 0x18, 0x01, 0x81, 0x01, 0x00, 0x80},
                              {0x2f, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00},
                              {0x23, 0x00, 0x1a, 0x01, 0x10, 0x00, 0x41, 0x60},
                              {0x23, 0x00, 0x1a, 0x02, 0x20, 0x00, 0x64, 0x60},
                              {0x2f, 0x00, 0x1a, 0x00, 0x02, 0x00, 0x00, 0x00},
                              {0x23, 0x00, 0x18, 0x01, 0x81, 0x01, 0x00, 0x00}};
C::SdoCompletion terminal(const C::SdoTransfer& t, uint64_t at, uint32_t value,
                          C::SdoOutcome outcome = C::SdoOutcome::SUCCESS, bool possible = true) {
    C::SdoCompletion e;
    e.transfer = t;
    e.outcome = outcome;
    e.nodeGeneration = 11;
    e.admittedUs = at;
    e.completedUs = at + 1;
    e.queued = e.transmissionPossible = e.transmissionConfirmed = possible;
    e.protocolAcknowledged = outcome == C::SdoOutcome::SUCCESS;
    e.effectPossible = possible;
    e.effectUncertain = possible && outcome != C::SdoOutcome::SUCCESS;
    if (outcome == C::SdoOutcome::SUCCESS && t.direction == C::SdoDirection::UPLOAD) {
        e.size = t.size;
        for (unsigned i = 0; i < e.size; ++i)
            e.data[i] = uint8_t(value >> (8 * i));
    }
    if (outcome == C::SdoOutcome::SERVER_ABORT) {
        e.abortValid = true;
        e.abortCode = 0xf1234567u;
    }
    return e;
}
struct Fixture {
    L::AxisReservation reservation;
    L::Tpdo1ConfigurationReview review;
    C::ObservationTime preOp;
    C::SdoCompletion baseline[9];
    L::ReadOptions options;
    M::Binding binding;
    L::Tpdo1ConfigurationContext context;
    uint64_t now = 100;
    Fixture() {
        auto& t = reservation.target;
        auto& b = t.channel.binding;
        b.logicalAxisId = 17;
        b.endpoint.bus = 2;
        b.endpoint.profile = 41;
        b.endpoint.address = 1;
        b.bindingGeneration = 3;
        b.configurationGeneration = 7;
        t.channel.clientToServerId = 0x601;
        t.channel.serverToClientId = 0x581;
        t.nodeGeneration = 11;
        reservation.model = L::SettingsModel::CL86_C;
        reservation.owner = 3;
        reservation.sequence = 5;
        reservation.generation = 9;
        review.target = t;
        review.scope = L::PdoConfigurationScope::ISOLATED_EXPERIMENT;
        review.fixtureRecord = 1;
        review.firmwareRecord = 2;
        review.procedureRecord = 3;
        review.authorizationRecord = 4;
        preOp.present = true;
        preOp.receiveBoundsKnown = true;
        preOp.receivedEarliestUs = 10;
        preOp.receivedLatestUs = 15;
        preOp.processedUs = 20;
        preOp.generation = 11;
        options.queueTimeoutUs = options.responseTimeoutUs = 1000;
        binding = b;
        for (unsigned i = 0; i < 9; ++i) {
            C::SdoTransfer w;
            w.channel = t.channel;
            w.operation = w.request = 1;
            w.requirePreOperational = true;
            w.step = i + 1;
            w.index = readIndex[i];
            w.subindex = readSub[i];
            w.size = readWidth[i];
            w.deadlineUs = 1000;
            w.queueTimeoutUs = w.responseTimeoutUs = 100;
            baseline[i] = terminal(w, 30 + i * 2, baselineValues[i]);
        }
    }
    M::Status prepare(uint64_t deadline = 2000, uint64_t age = 10000) {
        return L::prepareTpdo1Configuration(context, reservation, review, baseline, 9,
                                            C::NmtState::PRE_OPERATIONAL, preOp, 9, now, deadline,
                                            options, age);
    }
    M::Status next(L::PreparedRead& p) {
        return L::nextPdoConfiguration(context, reservation, binding, C::NmtState::PRE_OPERATIONAL,
                                       preOp, now, p);
    }
    M::Status notice() {
        return L::notePdoConfigurationTransmission(context, reservation, binding,
                                                   C::NmtState::PRE_OPERATIONAL, preOp, now);
    }
    C::SdoCompletion success() {
        L::PreparedRead p;
        CHECK(next(p));
        CHECK(p.ready);
        const bool write = p.transfer.direction == C::SdoDirection::DOWNLOAD;
        if (write)
            CHECK(notice());
        C::SdoCompletion e = terminal(p.transfer, now, write ? 0 : finalValues[context.cursor]);
        now += 2;
        CHECK(L::advancePdoConfiguration(context, reservation, binding, e, now));
        return e;
    }
    void prefix(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            success();
    }
};
void exactSequenceAndPublication() {
    Fixture f;
    CHECK(f.prepare());
    CHECK(f.context.review.scope == L::PdoConfigurationScope::ISOLATED_EXPERIMENT);
    for (unsigned i = 0; i < 6; ++i) {
        L::PreparedRead p;
        CHECK(f.next(p));
        CHECK(p.ready);
        CHECK(p.transfer.step == i + 1 && p.transfer.channel.binding.configurationGeneration == 7);
        CHECK(p.transfer.requirePreOperational);
        M::Frame expected;
        expected.id = 0x601;
        expected.dlc = expected.payloadLength = 8;
        std::memcpy(expected.data, writes[i], 8);
        C::SdoExpectation e;
        e.channel = p.transfer.channel;
        e.index = p.transfer.index;
        e.subindex = p.transfer.subindex;
        e.width = p.transfer.size;
        e.direction = C::SdoDirection::DOWNLOAD;
        CHECK(C::checkRequest(expected, e));
        CHECK(std::memcmp(expected.data + 4, p.transfer.data, p.transfer.size) == 0);
        f.success();
        CHECK(f.binding.configurationGeneration == 8);
        CHECK(f.context.acknowledgedMask == (1u << (i + 1)) - 1);
        CHECK(!f.context.mapConfirmed && f.context.uncertain);
    }
    CHECK(f.context.state == L::PdoConfigurationState::READING_BACK);
    for (unsigned i = 0; i < 9; ++i) {
        L::PreparedRead p;
        CHECK(f.next(p));
        CHECK(p.transfer.step == 7 + i && p.transfer.index == readIndex[i]);
        CHECK(p.transfer.subindex == readSub[i] && p.transfer.size == readWidth[i]);
        CHECK(p.transfer.channel.binding.configurationGeneration == 8);
        f.success();
        CHECK(f.context.mapConfirmed == (i == 8));
    }
    CHECK(f.context.state == L::PdoConfigurationState::SUCCEEDED && !f.context.uncertain);
    CHECK(f.context.review.scope == L::PdoConfigurationScope::ISOLATED_EXPERIMENT);
    C::PdoMapEvidence map;
    CHECK(L::getTpdo1Configuration(f.context, f.reservation, f.binding, f.now, 12, 42, map));
    CHECK(map.source == C::PdoMapSource::SDO_READBACK && map.route.layout.payloadLength == 6);
    CHECK(map.cobIdParameter == 0x181 && map.transmissionType == 255);
    CHECK(map.route.layout.mapping.words[0] == 0x60410010);
    CHECK(map.route.layout.mapping.words[1] == 0x60640020);
    M::Frame old;
    old.id = 0x181;
    old.dlc = old.payloadLength = 7;
    C::PdoSample sample;
    CHECK(!C::decodePdo(old, map.route, sample));
    // A six-byte envelope can decode; this API produces no sample-retirement proof.
    old.dlc = old.payloadLength = 6;
    CHECK(C::decodePdo(old, map.route, sample));
    L::PreparedRead p;
    CHECK(f.next(p) && !p.ready);
    Saved<L::Tpdo1ConfigurationContext> saved(f.context);
    CHECK(!L::advancePdoConfiguration(f.context, M::ReadOutcome::CANCELLED, f.now));
    saved.unchanged(f.context);
    Saved<C::PdoMapEvidence> oldMap(map);
    CHECK(!L::getTpdo1Configuration(f.context, f.reservation, f.binding, f.now, 0, 42, map));
    oldMap.unchanged(map);
    CHECK(!L::getTpdo1Configuration(f.context, f.reservation, f.binding, 20000, 12, 42, map));
    oldMap.unchanged(map);
}
void baselineProvenanceAndNmt() {
    for (unsigned slot = 0; slot < 9; ++slot) {
        Fixture f;
        f.baseline[slot].transfer.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
        Saved<L::Tpdo1ConfigurationContext> saved(f.context);
        CHECK(!f.prepare());
        saved.unchanged(f.context);
        f = Fixture();
        f.baseline[slot].admittedUs = 19;
        CHECK(!f.prepare());
        f = Fixture();
        f.baseline[slot].transfer.channel.binding.configurationGeneration++;
        CHECK(!f.prepare());
        f = Fixture();
        f.baseline[slot].nodeGeneration++;
        CHECK(!f.prepare());
        f = Fixture();
        f.baseline[slot].size = 0;
        CHECK(!f.prepare());
        f = Fixture();
        f.baseline[slot].transfer.requirePreOperational = false;
        CHECK(!f.prepare());
    }
    Fixture f;
    f.review.scope = L::PdoConfigurationScope::UNQUALIFIED;
    CHECK(!f.prepare());
    f = Fixture();
    f.review.firmwareRecord = 0;
    CHECK(!f.prepare());
    f = Fixture();
    f.review.target.nodeGeneration++;
    CHECK(!f.prepare());
    f = Fixture();
    f.preOp.receivedTimeKnown = true;
    CHECK(!f.prepare());
    f = Fixture();
    f.preOp.receivedLatestUs = 21;
    CHECK(!f.prepare());
    f = Fixture();
    f.preOp.receivedEarliestUs = 16;
    CHECK(!f.prepare());
    f = Fixture();
    f.preOp.generation++;
    CHECK(!f.prepare());
    f = Fixture();
    CHECK(!f.prepare(2000, 89));
    CHECK(f.prepare(2000, 90));
    f = Fixture();
    f.preOp.receiveBoundsKnown = false;
    f.preOp.receivedTimeKnown = true;
    f.preOp.receivedUs = 15;
    CHECK(f.prepare());
    f = Fixture();
    f.reservation.target.channel.binding.configurationGeneration = UINT32_MAX;
    f.review.target = f.reservation.target;
    CHECK(!f.prepare());
    f = Fixture();
    CHECK(f.prepare());
    L::PreparedRead p;
    Saved<L::PreparedRead> saved(p);
    CHECK(!L::nextPdoConfiguration(f.context, f.reservation, f.binding, C::NmtState::OPERATIONAL,
                                   f.preOp, f.now, p));
    saved.unchanged(p);
    f.reservation.generation++;
    CHECK(!f.next(p));
    saved.unchanged(p);
}
void everyServiceFailureAndReadbackMismatch() {
    const C::SdoOutcome failures[] = {C::SdoOutcome::SERVER_ABORT, C::SdoOutcome::TIMEOUT,
                                      C::SdoOutcome::CANCELLED,    C::SdoOutcome::REBOOT,
                                      C::SdoOutcome::BUS_OFF,      C::SdoOutcome::TRANSPORT_ERROR,
                                      C::SdoOutcome::CLIENT_ABORT};
    for (unsigned at = 0; at < 15; ++at) {
        for (C::SdoOutcome why : failures) {
            Fixture f;
            CHECK(f.prepare());
            f.prefix(at);
            L::PreparedRead p;
            CHECK(f.next(p));
            if (at < 6)
                CHECK(f.notice());
            C::SdoCompletion e = terminal(p.transfer, f.now, 0, why);
            f.now += 2;
            CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
            CHECK(f.context.state == L::PdoConfigurationState::FAILED && !f.context.mapConfirmed);
            CHECK(f.context.uncertain && f.context.evidencePresent);
            CHECK(f.context.evidence.outcome == why && f.context.evidence.abortCode == e.abortCode);
            CHECK(f.context.acknowledgedMask == (at < 6 ? (1u << at) - 1 : 0x3fu));
            CHECK(f.context.outcome ==
                  (why == C::SdoOutcome::SERVER_ABORT   ? M::ReadOutcome::PROTOCOL_REJECTED
                   : why == C::SdoOutcome::TIMEOUT      ? M::ReadOutcome::DEADLINE
                   : why == C::SdoOutcome::CANCELLED    ? M::ReadOutcome::CANCELLED
                   : why == C::SdoOutcome::REBOOT       ? M::ReadOutcome::INVALIDATED
                   : why == C::SdoOutcome::CLIENT_ABORT ? M::ReadOutcome::REPLY_ERROR
                                                        : M::ReadOutcome::TRANSPORT_ERROR));
            CHECK(f.next(p) && !p.ready); // No rollback, retry or second write.
        }
        if (at >= 6) {
            Fixture f;
            CHECK(f.prepare());
            f.prefix(at);
            L::PreparedRead p;
            CHECK(f.next(p));
            C::SdoCompletion e = terminal(p.transfer, f.now, finalValues[at - 6] ^ 1u);
            f.now += 2;
            CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
            CHECK(f.context.outcome == M::ReadOutcome::REPLY_ERROR && f.context.uncertain);
        }
    }
}
void cancellationCorrelationEpochAndDeadline() {
    for (unsigned at = 0; at < 15; ++at) {
        Fixture f;
        CHECK(f.prepare());
        f.prefix(at);
        CHECK(L::advancePdoConfiguration(f.context, M::ReadOutcome::CANCELLED, f.now));
        CHECK(f.context.uncertain == (at != 0));
        CHECK(!f.context.mapConfirmed);
    }
    Fixture f;
    CHECK(f.prepare());
    L::PreparedRead p;
    CHECK(f.next(p));
    C::SdoCompletion e = terminal(p.transfer, f.now, 0, C::SdoOutcome::TRANSPORT_ERROR, false);
    f.now += 2;
    CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    CHECK(!f.context.effectsPossible && !f.context.uncertain &&
          f.binding.configurationGeneration == 7);
    f = Fixture();
    CHECK(f.prepare());
    CHECK(f.next(p));
    CHECK(f.notice());
    Saved<L::Tpdo1ConfigurationContext> saved(f.context);
    CHECK(!f.notice());
    saved.unchanged(f.context);
    e = terminal(p.transfer, f.now, 0);
    e.transfer.step++;
    f.now += 2;
    CHECK(!L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    saved.unchanged(f.context);
    e.transfer.step--;
    e.protocolAcknowledged = false;
    CHECK(!L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    saved.unchanged(f.context);
    e.protocolAcknowledged = true;
    e.nodeGeneration++;
    CHECK(!L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    saved.unchanged(f.context);
    e.nodeGeneration--;
    CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    f.prefix(5);
    CHECK(f.next(p));
    e = terminal(p.transfer, f.now, finalValues[0]);
    e.transfer.channel.binding.configurationGeneration = 7;
    f.now += 2;
    saved = Saved<L::Tpdo1ConfigurationContext>(f.context);
    CHECK(!L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    saved.unchanged(f.context);
    f = Fixture();
    CHECK(f.prepare(102));
    CHECK(f.next(p));
    CHECK(f.notice());
    e = terminal(p.transfer, 100, 0);
    f.now = 102;
    CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    CHECK(f.context.outcome == M::ReadOutcome::DEADLINE && f.context.acknowledgedMask == 1);
    CHECK(f.context.uncertain && !f.context.mapConfirmed);
    f = Fixture();
    CHECK(f.prepare());
    CHECK(f.notice());
    CHECK(L::advancePdoConfiguration(f.context, M::ReadOutcome::INVALIDATED, f.now));
    CHECK(f.context.uncertain && f.context.acknowledgedMask == 0);
}
} // namespace
int main() {
    exactSequenceAndPublication();
    baselineProvenanceAndNmt();
    everyServiceFailureAndReadbackMismatch();
    cancellationCorrelationEpochAndDeadline();
    // Every active baseline field except COB-ID must exactly match this reviewed subset.
    for (unsigned i = 1; i < 9; ++i) {
        Fixture f;
        f.baseline[i].data[0] ^= 1;
        CHECK(!f.prepare());
    }
    Fixture f;
    f.baseline[0].data[3] = 0x80;
    CHECK(!f.prepare());
    std::printf("pdo_configuration_test: %u checks passed\n", checks);
}
