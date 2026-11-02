// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
const uint16_t kReadIndex[9] = {0x1800, 0x1800, 0x1800, 0x1800, 0x1a00,
                                0x1a00, 0x1a00, 0x1a00, 0x1a00};
const uint8_t kReadSub[9] = {1, 2, 3, 5, 0, 1, 2, 3, 4};
const uint8_t kReadWidth[9] = {4, 1, 2, 2, 1, 4, 4, 4, 4};
Status error(ReadError reason, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}
bool overlaps(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool sameChannel(const CANOPEN::SdoChannel& a, const CANOPEN::SdoChannel& b) {
    return sameBinding(a.binding, b.binding) && a.clientToServerId == b.clientToServerId &&
           a.serverToClientId == b.serverToClientId;
}
bool sameTarget(const ReadTarget& a, const ReadTarget& b) {
    return sameChannel(a.channel, b.channel) && a.nodeGeneration == b.nodeGeneration;
}
bool validReservation(const SettingsReservation& r) {
    return r.owner && r.sequence && r.generation && r.target.nodeGeneration &&
           (r.model == SettingsModel::CL86_C || r.model == SettingsModel::OL86_C) &&
           CANOPEN::validateSdoChannel(r.target.channel);
}
bool sameReservation(const SettingsReservation& a, const SettingsReservation& b) {
    return validReservation(a) && validReservation(b) && sameTarget(a.target, b.target) &&
           a.owner == b.owner && a.sequence == b.sequence && a.generation == b.generation &&
           a.model == b.model && a.acquiredUs == b.acquiredUs;
}
bool active(const Tpdo1ConfigurationContext& c) {
    return c.state == PdoConfigurationState::WRITING ||
           c.state == PdoConfigurationState::READING_BACK;
}
bool preOperational(const SettingsReservation& r, CANOPEN::NmtState nmt,
                    const CANOPEN::ObservationTime& t, uint64_t now, uint64_t age) {
    if (nmt != CANOPEN::NmtState::PRE_OPERATIONAL || !t.present || !age ||
        t.generation != r.target.nodeGeneration || t.receivedTimeKnown == t.receiveBoundsKnown ||
        t.processedUs > now)
        return false;
    const uint64_t first = t.receivedTimeKnown ? t.receivedUs : t.receivedEarliestUs;
    const uint64_t last = t.receivedTimeKnown ? t.receivedUs : t.receivedLatestUs;
    return first <= last && last <= t.processedUs && first >= r.acquiredUs && now - first <= age;
}
Binding expectedBinding(const Tpdo1ConfigurationContext& c) {
    Binding b = c.reservation.target.channel.binding;
    if (c.effectsPossible)
        ++b.configurationGeneration;
    return b;
}
Status current(const Tpdo1ConfigurationContext& c, const SettingsReservation& live,
               const Binding& binding, uint64_t now) {
    if (!sameReservation(c.reservation, live) || !sameBinding(binding, expectedBinding(c)))
        return error(ReadError::TARGET, "PDO configuration reservation or binding changed");
    if (c.state == PdoConfigurationState::EMPTY || c.state > PdoConfigurationState::FAILED ||
        !c.operation || !c.maximumAgeUs || !c.options.queueTimeoutUs ||
        !c.options.responseTimeoutUs || now < c.servicedUs || c.deadlineUs <= c.startedUs ||
        c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX ||
        (c.state == PdoConfigurationState::WRITING && c.cursor >= 6) ||
        (c.state == PdoConfigurationState::READING_BACK && c.cursor >= 9))
        return error(ReadError::STATE, "invalid PDO configuration state or time");
    return Ok();
}
uint32_t raw(const CANOPEN::SdoCompletion& e) {
    uint32_t v = 0;
    for (unsigned i = 0; i < e.size; ++i)
        v |= uint32_t(e.data[i]) << (8 * i);
    return v;
}
bool consistent(const CANOPEN::SdoCompletion& e) {
    const bool success = e.outcome == CANOPEN::SdoOutcome::SUCCESS;
    const bool upload = e.transfer.direction == CANOPEN::SdoDirection::UPLOAD;
    const bool effecting = !upload || e.transfer.readEffect != CANOPEN::SdoReadEffect::REVIEWED_NON_CHANGING;
    return e.outcome <= CANOPEN::SdoOutcome::UNSUPPORTED && (!e.transmissionPossible || e.queued) &&
           (!e.transmissionConfirmed || e.transmissionPossible) &&
           e.effectPossible == (effecting && e.transmissionPossible) &&
           e.effectUncertain == (effecting && e.transmissionPossible && !success) &&
           e.protocolAcknowledged == success &&
           (!success || (e.transmissionPossible && !e.abortValid && !e.clientAbortQueued)) &&
           (e.outcome != CANOPEN::SdoOutcome::SERVER_ABORT ||
            (e.abortValid && e.transmissionPossible)) &&
           e.size == (success && upload ? e.transfer.size : 0);
}
bool validBaseline(const CANOPEN::SdoCompletion& e, const SettingsReservation& r, unsigned slot,
                   uint64_t earliest, uint64_t now, uint64_t age) {
    const CANOPEN::SdoTransfer& t = e.transfer;
    return sameChannel(t.channel, r.target.channel) &&
           e.nodeGeneration == r.target.nodeGeneration && t.operation && t.request && t.step &&
           t.index == kReadIndex[slot] && t.subindex == kReadSub[slot] &&
           t.size == kReadWidth[slot] && t.direction == CANOPEN::SdoDirection::UPLOAD &&
           t.requirePreOperational && t.uploadSize == CANOPEN::SdoUploadSize::EXACT &&
           t.readEffect <= CANOPEN::SdoReadEffect::ACCEPTED_UNQUALIFIED && t.queueTimeoutUs &&
           t.responseTimeoutUs && e.admittedUs >= earliest && e.admittedUs <= e.completedUs &&
           e.completedUs <= now && e.completedUs < t.deadlineUs && now - e.admittedUs <= age &&
           e.outcome == CANOPEN::SdoOutcome::SUCCESS && consistent(e);
}
uint32_t expectedValue(const Tpdo1ConfigurationContext& c, unsigned slot) {
    return slot == 4 ? 2 : slot == 6 ? 0x60640020u : c.original[slot];
}
CANOPEN::SdoTransfer work(const Tpdo1ConfigurationContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.requirePreOperational = true;
    t.operation = t.request = c.operation;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.queueTimeoutUs;
    t.responseTimeoutUs = c.options.responseTimeoutUs;
    if (c.state == PdoConfigurationState::READING_BACK) {
        t.channel.binding = expectedBinding(c);
        t.step = 7 + c.cursor;
        t.index = kReadIndex[c.cursor];
        t.subindex = kReadSub[c.cursor];
        t.size = kReadWidth[c.cursor];
    } else {
        const uint16_t index[6] = {0x1800, 0x1a00, 0x1a00, 0x1a00, 0x1a00, 0x1800};
        const uint8_t sub[6] = {1, 0, 1, 2, 0, 1};
        const uint8_t width[6] = {4, 1, 4, 4, 1, 4};
        const uint32_t value[6] = {
            c.original[0] | 0x80000000u, 0, 0x60410010u, 0x60640020u, 2, c.original[0]};
        t.step = 1 + c.cursor;
        t.index = index[c.cursor];
        t.subindex = sub[c.cursor];
        t.size = width[c.cursor];
        t.direction = CANOPEN::SdoDirection::DOWNLOAD;
        for (unsigned i = 0; i < t.size; ++i)
            t.data[i] = uint8_t(value[c.cursor] >> (8 * i));
    }
    return t;
}
bool sameWork(const CANOPEN::SdoTransfer& a, const CANOPEN::SdoTransfer& b) {
    if (!sameChannel(a.channel, b.channel) || a.operation != b.operation ||
        a.request != b.request || a.step != b.step || a.index != b.index ||
        a.subindex != b.subindex || a.direction != b.direction || a.size != b.size ||
        a.uploadSize != b.uploadSize || a.readEffect != b.readEffect ||
        a.allowSegmentedUpload != b.allowSegmentedUpload ||
        a.requirePreOperational != b.requirePreOperational || a.deadlineUs != b.deadlineUs ||
        a.queueTimeoutUs != b.queueTimeoutUs || a.responseTimeoutUs != b.responseTimeoutUs)
        return false;
    for (unsigned i = 0; i < a.size && a.direction == CANOPEN::SdoDirection::DOWNLOAD; ++i)
        if (a.data[i] != b.data[i])
            return false;
    return true;
}
ReadOutcome outcome(CANOPEN::SdoOutcome o) {
    switch (o) {
    case CANOPEN::SdoOutcome::SERVER_ABORT:
        return ReadOutcome::PROTOCOL_REJECTED;
    case CANOPEN::SdoOutcome::TIMEOUT:
        return ReadOutcome::DEADLINE;
    case CANOPEN::SdoOutcome::CANCELLED:
        return ReadOutcome::CANCELLED;
    case CANOPEN::SdoOutcome::REBOOT:
    case CANOPEN::SdoOutcome::BINDING_INVALIDATED:
        return ReadOutcome::INVALIDATED;
    case CANOPEN::SdoOutcome::CLIENT_ABORT:
    case CANOPEN::SdoOutcome::UNSUPPORTED:
        return ReadOutcome::REPLY_ERROR;
    default:
        return ReadOutcome::TRANSPORT_ERROR;
    }
}
void fail(Tpdo1ConfigurationContext& c, ReadOutcome reason, uint64_t now, const char* message) {
    c.state = PdoConfigurationState::FAILED;
    c.outcome = reason;
    c.servicedUs = now;
    c.mapConfirmed = false;
    c.uncertain |= c.effectsPossible;
    c.status =
        reason == ReadOutcome::PROTOCOL_REJECTED
            ? Status(Err::PROTOCOL_ERROR, 0, message)
            : error(reason == ReadOutcome::DEADLINE ? ReadError::TIME : ReadError::STATE, message);
}
} // namespace

Status prepareTpdo1Configuration(Tpdo1ConfigurationContext& output, const SettingsReservation& r,
                                 const Tpdo1ConfigurationReview& review,
                                 const CANOPEN::SdoCompletion* baseline, size_t count,
                                 CANOPEN::NmtState nmt, const CANOPEN::ObservationTime& preOp,
                                 uint32_t operation, uint64_t now, uint64_t deadline,
                                 const ReadOptions& options, uint64_t age) noexcept {
    if (!baseline || count != 9)
        return error(ReadError::ARGUMENT, "PDO baseline requires nine exact readbacks");
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &review, sizeof review) ||
        overlaps(&output, sizeof output, baseline, count * sizeof *baseline) ||
        overlaps(&output, sizeof output, &preOp, sizeof preOp) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "PDO preparation overlaps input");
    if (!validReservation(r) || r.target.channel.binding.configurationGeneration == UINT32_MAX ||
        !operation || deadline <= now || !options.queueTimeoutUs || !options.responseTimeoutUs ||
        !sameTarget(review.target, r.target) ||
        (review.scope != PdoConfigurationScope::ISOLATED_EXPERIMENT &&
         review.scope != PdoConfigurationScope::FIXTURE_QUALIFIED) ||
        !review.fixtureRecord || !review.firmwareRecord || !review.procedureRecord ||
        !review.authorizationRecord || !preOperational(r, nmt, preOp, now, age))
        return error(ReadError::ARGUMENT, "unqualified PDO scope, reservation, NMT or budget");
    uint64_t earliest = preOp.processedUs;
    uint32_t values[9];
    for (unsigned i = 0; i < 9; ++i) {
        if (!validBaseline(baseline[i], r, i, earliest, now, age))
            return error(ReadError::CORRELATION,
                         "PDO baseline is not fresh exact sequential evidence");
        earliest = baseline[i].completedUs;
        values[i] = raw(baseline[i]);
    }
    const uint32_t allowedCob = 0x400007ffu;
    if ((values[0] & ~allowedCob) || !(values[0] & 0x7ffu) || values[1] != 255 ||
        values[2] != 100 || values[3] != 0 || values[4] != 3 || values[5] != 0x60410010u ||
        values[6] != 0x60610008u || values[7] != 0x60640020u || values[8] != 0)
        return error(ReadError::VALUE, "PDO baseline outside reviewed TPDO1 subset");
    Tpdo1ConfigurationContext c;
    c.reservation = r;
    c.review = review;
    c.state = PdoConfigurationState::WRITING;
    c.options = options;
    c.maximumAgeUs = age;
    c.startedUs = c.servicedUs = c.stepStartedUs = now;
    c.deadlineUs = deadline;
    c.operation = operation;
    for (unsigned i = 0; i < 9; ++i)
        c.original[i] = values[i];
    output = c;
    return Ok();
}
Status nextPdoConfiguration(const Tpdo1ConfigurationContext& c, const SettingsReservation& live,
                            const Binding& binding, CANOPEN::NmtState nmt,
                            const CANOPEN::ObservationTime& preOp, uint64_t now,
                            PreparedRead& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &binding, sizeof binding) ||
        overlaps(&output, sizeof output, &preOp, sizeof preOp))
        return error(ReadError::OVERLAP, "PDO work overlaps input");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    PreparedRead candidate;
    if (active(c)) {
        if (now >= c.deadlineUs || !preOperational(live, nmt, preOp, now, c.maximumAgeUs))
            return error(ReadError::TIME,
                         "PDO configuration deadline or Pre-operational evidence expired");
        candidate.ready = true;
        candidate.transfer = work(c);
    }
    output = candidate;
    return Ok();
}
Status notePdoConfigurationTransmission(Tpdo1ConfigurationContext& c,
                                        const SettingsReservation& live, Binding& binding,
                                        CANOPEN::NmtState nmt,
                                        const CANOPEN::ObservationTime& preOp,
                                        uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &binding, sizeof binding) ||
        overlaps(&c, sizeof c, &preOp, sizeof preOp) ||
        overlaps(&binding, sizeof binding, &live, sizeof live) ||
        overlaps(&binding, sizeof binding, &preOp, sizeof preOp))
        return error(ReadError::OVERLAP, "PDO TX notice overlaps input");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    if (c.state != PdoConfigurationState::WRITING || c.transmissionNoted || now >= c.deadlineUs ||
        !preOperational(live, nmt, preOp, now, c.maximumAgeUs))
        return error(ReadError::STATE, "no eligible PDO download transmission");
    if (!c.effectsPossible)
        ++binding.configurationGeneration;
    c.effectsPossible = c.transmissionNoted = c.uncertain = true;
    c.mapConfirmed = false;
    c.servicedUs = now;
    return Ok();
}
Status advancePdoConfiguration(Tpdo1ConfigurationContext& c, const SettingsReservation& live,
                               const Binding& binding, const CANOPEN::SdoCompletion& e,
                               uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &binding, sizeof binding) || overlaps(&c, sizeof c, &e, sizeof e))
        return error(ReadError::OVERLAP, "PDO terminal overlaps input");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    if (!active(c))
        return error(ReadError::STATE, "PDO configuration already terminal");
    const CANOPEN::SdoTransfer expected = work(c);
    if (!sameWork(e.transfer, expected) ||
        e.nodeGeneration != c.reservation.target.nodeGeneration || e.admittedUs < c.stepStartedUs ||
        e.admittedUs > e.completedUs || e.admittedUs >= c.deadlineUs ||
        e.completedUs < c.servicedUs || e.completedUs > now || !consistent(e) ||
        (c.state == PdoConfigurationState::WRITING && e.transmissionPossible &&
         !c.transmissionNoted))
        return error(ReadError::CORRELATION, "PDO terminal does not match the current exact step");
    // All rejection checks precede mutation. The remaining finite updates cannot fail.
    c.evidence = e;
    c.evidencePresent = true;
    c.servicedUs = now;
    const bool success = e.outcome == CANOPEN::SdoOutcome::SUCCESS;
    if (c.state == PdoConfigurationState::WRITING && success)
        c.acknowledgedMask |= uint8_t(1u << c.cursor);
    c.uncertain |= e.effectUncertain;
    if (now >= c.deadlineUs || e.completedUs >= c.deadlineUs) {
        fail(c, ReadOutcome::DEADLINE, now, "PDO configuration overall deadline");
    } else if (!success) {
        fail(c, outcome(e.outcome), now, "PDO configuration service failed");
    } else if (c.state == PdoConfigurationState::WRITING) {
        c.transmissionNoted = false;
        c.stepStartedUs = now;
        if (++c.cursor == 6) {
            c.cursor = 0;
            c.state = PdoConfigurationState::READING_BACK;
        }
    } else {
        const uint32_t value = raw(e);
        c.readback[c.cursor] = value;
        c.readbackCount = uint8_t(c.cursor + 1);
        if (c.cursor == 0)
            c.readbackStartedUs = e.admittedUs;
        c.readbackCompletedUs = e.completedUs;
        if (value != expectedValue(c, c.cursor)) {
            fail(c, ReadOutcome::REPLY_ERROR, now,
                 "PDO readback contradicts requested configuration");
        } else {
            c.stepStartedUs = now;
            if (++c.cursor == 9) {
                c.state = PdoConfigurationState::SUCCEEDED;
                c.outcome = ReadOutcome::SUCCESS;
                c.mapConfirmed = true;
                c.uncertain = false;
                c.status = Ok();
            }
        }
    }
    return Ok();
}
Status advancePdoConfiguration(Tpdo1ConfigurationContext& c, ReadOutcome reason,
                               uint64_t now) noexcept {
    if (!active(c) || now < c.servicedUs ||
        (reason != ReadOutcome::CANCELLED && reason != ReadOutcome::DEADLINE &&
         reason != ReadOutcome::INVALIDATED) ||
        (reason == ReadOutcome::DEADLINE && now < c.deadlineUs))
        return error(ReadError::STATE, "invalid local PDO configuration withdrawal");
    if (now >= c.deadlineUs)
        reason = ReadOutcome::DEADLINE;
    fail(c, reason, now, "PDO configuration locally withdrawn; no rollback");
    return Ok();
}
Status getTpdo1Configuration(const Tpdo1ConfigurationContext& c, const SettingsReservation& live,
                             const Binding& binding, uint64_t now, Generation generation,
                             uint32_t evidenceId, CANOPEN::PdoMapEvidence& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &binding, sizeof binding))
        return error(ReadError::OVERLAP, "PDO result overlaps input");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    if (c.state != PdoConfigurationState::SUCCEEDED || !c.mapConfirmed || c.readbackCount != 9 ||
        c.acknowledgedMask != 0x3f || !generation || !evidenceId ||
        c.readbackStartedUs > c.readbackCompletedUs || c.readbackCompletedUs > now ||
        now - c.readbackStartedUs > c.maximumAgeUs)
        return error(ReadError::STATE, "no complete fresh PDO configuration readback");
    CANOPEN::PdoMapEvidence candidate;
    candidate.route.binding = binding;
    candidate.route.generation = generation;
    candidate.route.cobId = c.original[0] & 0x7ffu;
    CANOPEN::PdoMap map;
    map.direction = CANOPEN::PdoDirection::TPDO;
    map.number = 1;
    map.count = 2;
    map.words[0] = 0x60410010u;
    map.words[1] = 0x60640020u;
    const Status layout =
        CANOPEN::validatePdoMap(map, pdoProfile(binding.endpoint.profile), candidate.route.layout);
    if (!layout)
        return layout;
    candidate.nodeGeneration = c.reservation.target.nodeGeneration;
    candidate.startedUs = c.readbackStartedUs;
    candidate.observedUs = c.readbackCompletedUs;
    candidate.source = CANOPEN::PdoMapSource::SDO_READBACK;
    candidate.evidenceId = evidenceId;
    candidate.communicationKnown = true;
    candidate.cobIdParameter = c.original[0];
    candidate.transmissionType = 255;
    output = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
