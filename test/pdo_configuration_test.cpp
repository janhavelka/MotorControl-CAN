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
    // Default: the legacy map3-to-map2 overload. makeTimer() selects the request overload.
    L::Tpdo1ConfigurationRequest request;
    bool legacy = true;
    uint32_t expected[9];
    uint64_t now = 100;
    Fixture() {
        std::memcpy(expected, finalValues, sizeof expected);
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
        if (legacy)
            return L::prepareTpdo1Configuration(context, reservation, review, baseline, 9,
                                                C::NmtState::PRE_OPERATIONAL, preOp, 9, now,
                                                deadline, options, age);
        return L::prepareTpdo1Configuration(context, reservation, review, request, baseline, 9,
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
        C::SdoCompletion e = terminal(p.transfer, now, write ? 0 : expected[context.cursor]);
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
    CHECK(f.context.request.transition == L::Tpdo1Transition::MAP3_TO_MAP2);
    CHECK(f.context.confirmedStep == 15 && f.context.firstEffectStep == 1 && !f.context.failedStep);
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
            // The step that ended it, the confirmed prefix and the first possible write stay.
            CHECK(f.context.failedStep == at + 1 && f.context.confirmedStep == at);
            CHECK(f.context.firstEffectStep == 1);
            CHECK(f.context.outcome ==
                  (why == C::SdoOutcome::SERVER_ABORT   ? M::ReadOutcome::PROTOCOL_REJECTED
                   : why == C::SdoOutcome::TIMEOUT      ? M::ReadOutcome::SERVICE_TIMEOUT
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
// ---- EVENT_TIMER: one 1800:05 write between the two measured raw literals.
void setValue(C::SdoCompletion& e, uint32_t value) {
    for (unsigned i = 0; i < e.size; ++i)
        e.data[i] = uint8_t(value >> (8 * i));
}
uint32_t other(L::Tpdo1EventTimerRaw target) {
    return target == L::Tpdo1EventTimerRaw::RAW_0 ? 100 : 0;
}
// The request overload, with the baseline holding the literal other than the target.
void makeTimer(Fixture& f, L::Tpdo1EventTimerRaw target) {
    f.legacy = false;
    f.request.transition = L::Tpdo1Transition::EVENT_TIMER;
    f.request.eventTimer = target;
    setValue(f.baseline[3], other(target));
    std::memcpy(f.expected, baselineValues, sizeof f.expected);
    f.expected[3] = uint32_t(target);
}
Fixture timer(L::Tpdo1EventTimerRaw target = L::Tpdo1EventTimerRaw::RAW_100) {
    Fixture f;
    makeTimer(f, target);
    return f;
}
bool refusedAs(const M::Status& s, M::ReadError detail) {
    return !s && s.code == M::Err::INVALID_CONFIG && s.detail == int32_t(detail);
}
void timerSequenceAndPublication() {
    const L::Tpdo1EventTimerRaw targets[] = {L::Tpdo1EventTimerRaw::RAW_100,
                                             L::Tpdo1EventTimerRaw::RAW_0};
    for (L::Tpdo1EventTimerRaw target : targets) {
        Fixture f = timer(target);
        CHECK(f.prepare());
        CHECK(f.context.request.transition == L::Tpdo1Transition::EVENT_TIMER);
        CHECK(f.context.request.eventTimer == target && f.context.original[3] == other(target));
        CHECK(f.context.state == L::PdoConfigurationState::WRITING && !f.context.effectsPossible);
        L::PreparedRead p;
        CHECK(f.next(p) && p.ready);
        CHECK(p.transfer.step == 1 && p.transfer.direction == C::SdoDirection::DOWNLOAD);
        CHECK(p.transfer.index == 0x1800 && p.transfer.subindex == 5 && p.transfer.size == 2);
        CHECK(p.transfer.requirePreOperational && p.transfer.operation == 9);
        CHECK(p.transfer.channel.binding.configurationGeneration == 7 &&
              p.transfer.deadlineUs == 2000);
        // Literal expedited two-byte download, as the bench sent it (1316Z, 1317Z).
        const uint8_t lo = target == L::Tpdo1EventTimerRaw::RAW_100 ? 0x64 : 0x00;
        const uint8_t bytes[8] = {0x2b, 0x00, 0x18, 0x05, lo, 0x00, 0x00, 0x00};
        M::Frame expected;
        expected.id = 0x601;
        expected.dlc = expected.payloadLength = 8;
        std::memcpy(expected.data, bytes, 8);
        C::SdoExpectation e;
        e.channel = p.transfer.channel;
        e.index = 0x1800;
        e.subindex = 5;
        e.width = 2;
        e.direction = C::SdoDirection::DOWNLOAD;
        CHECK(C::checkRequest(expected, e));
        CHECK(std::memcmp(expected.data + 4, p.transfer.data, 2) == 0);
        f.success();
        CHECK(f.binding.configurationGeneration == 8 && f.context.firstEffectStep == 1);
        CHECK(f.context.acknowledgedMask == 1 && f.context.confirmedStep == 1);
        CHECK(f.context.state == L::PdoConfigurationState::READING_BACK && f.context.uncertain);
        for (unsigned i = 0; i < 9; ++i) {
            CHECK(f.next(p) && p.ready);
            CHECK(p.transfer.step == 2 + i && p.transfer.direction == C::SdoDirection::UPLOAD);
            CHECK(p.transfer.index == readIndex[i] && p.transfer.subindex == readSub[i]);
            CHECK(p.transfer.size == readWidth[i] && p.transfer.requirePreOperational);
            CHECK(p.transfer.channel.binding.configurationGeneration == 8);
            C::PdoMapEvidence early;
            CHECK(!L::getTpdo1Configuration(f.context, f.reservation, f.binding, f.now, 12, 42,
                                            early)); // Never before the ninth matched read.
            f.success();
            CHECK(f.context.confirmedStep == 2 + i && f.context.mapConfirmed == (i == 8));
        }
        CHECK(f.context.state == L::PdoConfigurationState::SUCCEEDED && !f.context.uncertain);
        CHECK(!f.context.failedStep && f.context.readback[3] == uint32_t(target));
        C::PdoMapEvidence map;
        L::Tpdo1EventTimerRaw confirmed = target == L::Tpdo1EventTimerRaw::RAW_0
                                              ? L::Tpdo1EventTimerRaw::RAW_100
                                              : L::Tpdo1EventTimerRaw::RAW_0;
        CHECK(L::getTpdo1Configuration(f.context, f.reservation, f.binding, f.now, 12, 42, map,
                                       confirmed));
        CHECK(confirmed == target);
        CHECK(map.source == C::PdoMapSource::SDO_READBACK && map.route.generation == 12);
        CHECK(map.route.binding.configurationGeneration == 8 && map.evidenceId == 42);
        CHECK(map.route.cobId == 0x181 && map.cobIdParameter == 0x181 &&
              map.transmissionType == 255);
        CHECK(map.route.layout.mapping.count == 3 && map.route.layout.payloadLength == 7);
        CHECK(map.route.layout.mapping.words[0] == 0x60410010 &&
              map.route.layout.mapping.words[1] == 0x60610008 &&
              map.route.layout.mapping.words[2] == 0x60640020);
        // The published map decodes a seven-byte frame. That is layout only: an old frame on the
        // same identifier decodes the same way, and nothing here qualifies it as a fresh sample.
        M::Frame old;
        old.id = 0x181;
        old.dlc = old.payloadLength = 7;
        C::PdoSample sample;
        CHECK(C::decodePdo(old, map.route, sample));
        old.dlc = old.payloadLength = 6;
        CHECK(!C::decodePdo(old, map.route, sample));
        // Publication fails unchanged on a zero epoch, a stale readback or an aliased literal.
        const Saved<C::PdoMapEvidence> kept(map);
        L::Tpdo1EventTimerRaw unchanged = L::Tpdo1EventTimerRaw::RAW_0;
        CHECK(!L::getTpdo1Configuration(f.context, f.reservation, f.binding, f.now, 0, 42, map,
                                        unchanged));
        CHECK(!L::getTpdo1Configuration(f.context, f.reservation, f.binding, 20000, 12, 42, map,
                                        unchanged));
        L::Tpdo1EventTimerRaw* inside = reinterpret_cast<L::Tpdo1EventTimerRaw*>(&map);
        CHECK(refusedAs(L::getTpdo1Configuration(f.context, f.reservation, f.binding, f.now, 12,
                                                 42, map, *inside),
                        M::ReadError::OVERLAP));
        kept.unchanged(map);
        CHECK(unchanged == L::Tpdo1EventTimerRaw::RAW_0);
        CHECK(f.next(p) && !p.ready); // A terminal yields nothing: no second write, no rollback.
    }
    // The bench caller's scope, FIXTURE_QUALIFIED, is retained, never derived from success.
    Fixture f = timer();
    f.review.scope = L::PdoConfigurationScope::FIXTURE_QUALIFIED;
    CHECK(f.prepare() && f.context.review.scope == L::PdoConfigurationScope::FIXTURE_QUALIFIED);
}
void timerRequestAndBaselineRejections() {
    // Only the two literals, and only the change from one to the other.
    const uint16_t literals[] = {1, 10, 99, 101, 200, 1000, 65535};
    for (uint16_t value : literals) {
        Fixture f = timer();
        f.request.eventTimer = L::Tpdo1EventTimerRaw(value);
        const Saved<L::Tpdo1ConfigurationContext> saved(f.context);
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
        saved.unchanged(f.context);
    }
    {
        Fixture f = timer();
        f.request.transition = L::Tpdo1Transition::NONE;
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
        f = Fixture();
        f.legacy = false; // A default request names no transition.
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
        f.request.transition = L::Tpdo1Transition::MAP3_TO_MAP2;
        f.request.eventTimer = L::Tpdo1EventTimerRaw::RAW_100; // The map change keeps event 0.
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
        f.request.eventTimer = L::Tpdo1EventTimerRaw::RAW_0;
        CHECK(f.prepare()); // The request form of the existing map change.
        f = Fixture();
        f.legacy = false;
        f.request.transition = L::Tpdo1Transition(3);
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
    }
    // Baseline values: each case alone leaves the context unchanged with VALUE.
    struct Case {
        unsigned slot;
        uint32_t value;
    };
    const Case cases[] = {
        {3, 100},        // Already the target: no measured transition, so no write.
        {3, 200},        // Measured in packet 6 but not supported.
        {3, 10},         // Likewise.
        {0, 0x182},      // Another node's identifier on this node: changed nothing.
        {0, 0x80000181}, // PDO not valid.
        {0, 0x40000181}, // Bit 30 differs from the measured specimens.
        {0, 0x20000181}, // Extended-frame flag.
        {0, 0x201},      // Not the TPDO1 default identifier of node 1.
        {1, 254},        // Transmission type.
        {1, 1},          // A SYNC type.
        {2, 0},          // Inhibit.
        {4, 2},          // Count two, as after the map change.
        {4, 4},          // Count four.
        {4, 0},          // Disabled map.
        {5, 0x60410008}, // Width of an active word.
        {6, 0x60640020}, // Another active word.
        {7, 0x606c0020}, // Another active word.
        {8, 0x60640020}, // Inactive slot four not zero.
    };
    for (const Case& c : cases) {
        Fixture f = timer(L::Tpdo1EventTimerRaw::RAW_100);
        setValue(f.baseline[c.slot], c.value);
        const Saved<L::Tpdo1ConfigurationContext> saved(f.context);
        CHECK(refusedAs(f.prepare(), M::ReadError::VALUE));
        saved.unchanged(f.context);
    }
    // The reverse direction refuses a baseline already at 0 and one at 200.
    const uint32_t reverse[] = {0, 200};
    for (uint32_t value : reverse) {
        Fixture f = timer(L::Tpdo1EventTimerRaw::RAW_0);
        setValue(f.baseline[3], value);
        CHECK(refusedAs(f.prepare(), M::ReadError::VALUE));
    }
    // The map change keeps accepting bit 30, which the timer change does not.
    {
        Fixture f;
        setValue(f.baseline[0], 0x40000181);
        CHECK(f.prepare());
    }
    // Node 2's measured identifier is 0x182, checked against the reservation's own node.
    {
        Fixture f = timer();
        auto& b = f.reservation.target.channel.binding;
        b.endpoint.address = 2;
        f.reservation.target.channel.clientToServerId = 0x602;
        f.reservation.target.channel.serverToClientId = 0x582;
        f.review.target = f.reservation.target;
        for (auto& e : f.baseline)
            e.transfer.channel = f.reservation.target.channel;
        setValue(f.baseline[0], 0x182);
        f.binding = b;
        CHECK(f.prepare());
        setValue(f.baseline[0], 0x181); // Node 1's identifier, a collision if written here.
        CHECK(refusedAs(f.prepare(), M::ReadError::VALUE));
    }
    // A width or format other than the EDS's is not this provenance, before any value check.
    for (unsigned slot = 0; slot < 9; ++slot) {
        Fixture f = timer();
        f.baseline[slot].transfer.size = uint8_t(readWidth[slot] == 4 ? 2 : 4);
        f.baseline[slot].size = f.baseline[slot].transfer.size;
        CHECK(refusedAs(f.prepare(), M::ReadError::CORRELATION));
        f = timer();
        f.baseline[slot].transfer.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
        CHECK(refusedAs(f.prepare(), M::ReadError::CORRELATION));
        f = timer();
        f.baseline[slot].transfer.channel.binding.configurationGeneration++;
        CHECK(refusedAs(f.prepare(), M::ReadError::CORRELATION));
        f = timer();
        f.baseline[slot].nodeGeneration++;
        CHECK(refusedAs(f.prepare(), M::ReadError::CORRELATION));
        f = timer();
        f.baseline[slot].admittedUs = 19; // Before the Pre-operational observation.
        CHECK(refusedAs(f.prepare(), M::ReadError::CORRELATION));
    }
    // Reservation, NMT, deadline, budgets and aliasing reject before the baseline is examined.
    {
        Fixture f = timer();
        f.reservation.model = L::SettingsModel::UNKNOWN;
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
        // Every event-timer measurement is CL86-C: OL86-C refuses before the baseline is examined,
        // even a baseline that would also fail its values. The map change keeps its model scope.
        for (unsigned bad = 0; bad < 2; ++bad) {
            f = timer();
            f.reservation.model = L::SettingsModel::OL86_C;
            if (bad)
                setValue(f.baseline[2], 0);
            const Saved<L::Tpdo1ConfigurationContext> kept(f.context);
            const M::Status model = f.prepare();
            CHECK(!model && model.code == M::Err::UNSUPPORTED &&
                  model.detail == int32_t(M::ReadError::TARGET));
            kept.unchanged(f.context);
        }
        f = Fixture();
        f.reservation.model = L::SettingsModel::OL86_C;
        CHECK(f.prepare());
        f = timer();
        CHECK(refusedAs(f.prepare(f.now), M::ReadError::ARGUMENT));
        f = timer();
        f.options.responseTimeoutUs = 0;
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
        f = timer();
        f.preOp.generation++;
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
        f = timer();
        CHECK(refusedAs(f.prepare(2000, 89), M::ReadError::ARGUMENT)); // Stale Pre-operational.
        f = timer();
        f.review.authorizationRecord = 0;
        CHECK(refusedAs(f.prepare(), M::ReadError::ARGUMENT));
        f = timer();
        CHECK(refusedAs(L::prepareTpdo1Configuration(f.context, f.reservation, f.review, f.request,
                                                     f.baseline, 9, C::NmtState::OPERATIONAL,
                                                     f.preOp, 9, f.now, 2000, f.options, 10000),
                        M::ReadError::ARGUMENT));
        f = timer();
        CHECK(refusedAs(L::prepareTpdo1Configuration(f.context, f.reservation, f.review, f.request,
                                                     f.baseline, 8, C::NmtState::PRE_OPERATIONAL,
                                                     f.preOp, 9, f.now, 2000, f.options, 10000),
                        M::ReadError::ARGUMENT));
        f = timer();
        const L::Tpdo1ConfigurationRequest* inside =
            reinterpret_cast<const L::Tpdo1ConfigurationRequest*>(&f.context);
        CHECK(refusedAs(L::prepareTpdo1Configuration(f.context, f.reservation, f.review, *inside,
                                                     f.baseline, 9, C::NmtState::PRE_OPERATIONAL,
                                                     f.preOp, 9, f.now, 2000, f.options, 10000),
                        M::ReadError::OVERLAP));
        // A binding whose profile identity cannot represent the published map.
        f = timer();
        f.reservation.target.channel.binding.endpoint.profile = 0;
        f.review.target = f.reservation.target;
        for (auto& e : f.baseline)
            e.transfer.channel = f.reservation.target.channel;
        const Saved<L::Tpdo1ConfigurationContext> saved(f.context);
        CHECK(!f.prepare());
        saved.unchanged(f.context);
    }
}
void timerFailureAtEveryStep() {
    const C::SdoOutcome failures[] = {C::SdoOutcome::SERVER_ABORT,    C::SdoOutcome::TIMEOUT,
                                      C::SdoOutcome::CANCELLED,       C::SdoOutcome::REBOOT,
                                      C::SdoOutcome::BUS_OFF,         C::SdoOutcome::TRANSPORT_ERROR,
                                      C::SdoOutcome::CLIENT_ABORT,
                                      C::SdoOutcome::BINDING_INVALIDATED};
    for (unsigned at = 0; at < 10; ++at) {
        for (C::SdoOutcome why : failures) {
            Fixture f = timer();
            CHECK(f.prepare());
            f.prefix(at);
            L::PreparedRead p;
            CHECK(f.next(p) && p.ready);
            if (at == 0)
                CHECK(f.notice());
            C::SdoCompletion e = terminal(p.transfer, f.now, 0, why);
            f.now += 2;
            CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
            CHECK(f.context.state == L::PdoConfigurationState::FAILED && !f.context.mapConfirmed);
            CHECK(f.context.failedStep == at + 1 && f.context.confirmedStep == at);
            CHECK(f.context.firstEffectStep == 1 && f.context.uncertain);
            CHECK(f.context.acknowledgedMask == (at ? 1u : 0u));
            CHECK(f.context.evidencePresent && f.context.evidence.outcome == why);
            CHECK(f.context.evidence.transfer.step == at + 1);
            if (why == C::SdoOutcome::SERVER_ABORT)
                CHECK(f.context.evidence.abortValid && f.context.evidence.abortCode == 0xf1234567u &&
                      f.context.status.code == M::Err::PROTOCOL_ERROR);
            CHECK(f.next(p) && !p.ready); // No retry, rollback or second write.
            C::PdoMapEvidence map;
            const Saved<C::PdoMapEvidence> blank(map);
            CHECK(!L::getTpdo1Configuration(f.context, f.reservation, f.binding, f.now, 12, 42,
                                            map));
            blank.unchanged(map);
        }
        if (at) {
            // Readback contradicting the write or a preserved field: REPLY_ERROR, never success.
            Fixture f = timer();
            CHECK(f.prepare());
            f.prefix(at);
            L::PreparedRead p;
            CHECK(f.next(p));
            C::SdoCompletion e = terminal(p.transfer, f.now, f.expected[at - 1] ^ 0x40u);
            f.now += 2;
            CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
            CHECK(f.context.outcome == M::ReadOutcome::REPLY_ERROR && f.context.uncertain);
            CHECK(f.context.failedStep == at + 1 && f.context.readbackCount == at);
            CHECK(!f.context.mapConfirmed && f.next(p) && !p.ready);
        }
    }
    // A write refused before any possible TX changes no configuration epoch.
    Fixture f = timer();
    CHECK(f.prepare());
    L::PreparedRead p;
    CHECK(f.next(p));
    C::SdoCompletion e = terminal(p.transfer, f.now, 0, C::SdoOutcome::TRANSPORT_ERROR, false);
    f.now += 2;
    CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    CHECK(!f.context.effectsPossible && !f.context.uncertain && !f.context.firstEffectStep);
    CHECK(f.context.failedStep == 1 && f.binding.configurationGeneration == 7);
    // A terminal claiming possible TX that was never noted does not correlate.
    f = timer();
    CHECK(f.prepare());
    CHECK(f.next(p));
    e = terminal(p.transfer, f.now, 0);
    f.now += 2;
    Saved<L::Tpdo1ConfigurationContext> saved(f.context);
    CHECK(refusedAs(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now),
                    M::ReadError::CORRELATION));
    saved.unchanged(f.context);
}
void timerCancellationDeadlineAndGenerations() {
    for (unsigned at = 0; at <= 10; ++at) {
        Fixture f = timer();
        CHECK(f.prepare());
        f.prefix(at);
        if (at == 10) {
            CHECK(!L::advancePdoConfiguration(f.context, M::ReadOutcome::CANCELLED, f.now));
            continue;
        }
        CHECK(L::advancePdoConfiguration(f.context, M::ReadOutcome::CANCELLED, f.now));
        CHECK(f.context.outcome == M::ReadOutcome::CANCELLED && !f.context.mapConfirmed);
        CHECK(f.context.uncertain == (at != 0) && f.context.failedStep == at + 1);
        CHECK(f.context.confirmedStep == at && f.context.evidencePresent == (at != 0));
    }
    // Cancelled after the possible TX of the write but before its terminal: effect unknown.
    Fixture f = timer();
    CHECK(f.prepare());
    L::PreparedRead p;
    CHECK(f.next(p) && f.notice());
    Saved<L::Tpdo1ConfigurationContext> saved(f.context);
    CHECK(!f.notice()); // One notice per step.
    saved.unchanged(f.context);
    CHECK(L::advancePdoConfiguration(f.context, M::ReadOutcome::INVALIDATED, f.now));
    CHECK(f.context.uncertain && f.context.failedStep == 1 && f.context.firstEffectStep == 1);
    CHECK(!f.context.acknowledgedMask && !f.context.evidencePresent);
    CHECK(f.binding.configurationGeneration == 8); // The withdrawal at possible TX stays.
    // Deadline: an acknowledgement completing before the cutoff, consumed at it, is retained but
    // confirms nothing.
    f = timer();
    CHECK(f.prepare(102));
    CHECK(f.next(p) && f.notice());
    C::SdoCompletion e = terminal(p.transfer, 100, 0);
    f.now = 102;
    CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    CHECK(f.context.outcome == M::ReadOutcome::DEADLINE && f.context.acknowledgedMask == 1);
    CHECK(f.context.confirmedStep == 0 && f.context.failedStep == 1 && f.context.uncertain);
    // A local deadline withdrawal before the cutoff is refused; at it, it ends the sequence.
    f = timer();
    CHECK(f.prepare(200));
    CHECK(!L::advancePdoConfiguration(f.context, M::ReadOutcome::DEADLINE, 199));
    CHECK(L::advancePdoConfiguration(f.context, M::ReadOutcome::CANCELLED, 200));
    CHECK(f.context.outcome == M::ReadOutcome::DEADLINE);
    // Wrong generations, reservation, binding and NMT reject without change.
    f = timer();
    CHECK(f.prepare());
    f.prefix(1);
    CHECK(f.next(p));
    e = terminal(p.transfer, f.now, f.expected[0]);
    f.now += 2;
    saved = Saved<L::Tpdo1ConfigurationContext>(f.context);
    C::SdoCompletion wrong = e;
    wrong.transfer.channel.binding.configurationGeneration = 7; // Old epoch after the write.
    CHECK(refusedAs(L::advancePdoConfiguration(f.context, f.reservation, f.binding, wrong, f.now),
                    M::ReadError::CORRELATION));
    wrong = e;
    wrong.nodeGeneration++; // A reply from after a reboot.
    CHECK(!L::advancePdoConfiguration(f.context, f.reservation, f.binding, wrong, f.now));
    wrong = e;
    wrong.transfer.operation++; // Another operation's terminal.
    CHECK(!L::advancePdoConfiguration(f.context, f.reservation, f.binding, wrong, f.now));
    wrong = e;
    wrong.transfer.step = 1; // The write's step again: a late duplicate is not this read.
    CHECK(!L::advancePdoConfiguration(f.context, f.reservation, f.binding, wrong, f.now));
    M::Binding rebound = f.binding;
    rebound.bindingGeneration++;
    CHECK(refusedAs(L::advancePdoConfiguration(f.context, f.reservation, rebound, e, f.now),
                    M::ReadError::TARGET));
    L::AxisReservation other = f.reservation;
    other.generation++;
    CHECK(refusedAs(L::advancePdoConfiguration(f.context, other, f.binding, e, f.now),
                    M::ReadError::TARGET));
    saved.unchanged(f.context);
    L::PreparedRead out;
    const Saved<L::PreparedRead> blank(out);
    CHECK(refusedAs(L::nextPdoConfiguration(f.context, f.reservation, f.binding,
                                            C::NmtState::OPERATIONAL, f.preOp, f.now, out),
                    M::ReadError::TIME));
    CHECK(refusedAs(L::nextPdoConfiguration(f.context, f.reservation, rebound,
                                            C::NmtState::PRE_OPERATIONAL, f.preOp, f.now, out),
                    M::ReadError::TARGET));
    blank.unchanged(out);
    CHECK(L::advancePdoConfiguration(f.context, f.reservation, f.binding, e, f.now));
    // An edited frozen request is no longer this context.
    f.context.request.eventTimer = L::Tpdo1EventTimerRaw(200);
    CHECK(refusedAs(f.next(out), M::ReadError::STATE));
}
} // namespace
int main() {
    exactSequenceAndPublication();
    baselineProvenanceAndNmt();
    everyServiceFailureAndReadbackMismatch();
    cancellationCorrelationEpochAndDeadline();
    timerSequenceAndPublication();
    timerRequestAndBaselineRejections();
    timerFailureAtEveryStep();
    timerCancellationDeadlineAndGenerations();
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
