// SPDX-License-Identifier: MIT
#include "ProfileEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
// The nine-entry snapshot read before and after either transition, in step order.
const uint16_t kReadIndex[9] = {0x1800, 0x1800, 0x1800, 0x1800, 0x1a00,
                                0x1a00, 0x1a00, 0x1a00, 0x1a00};
const uint8_t kReadSub[9] = {1, 2, 3, 5, 0, 1, 2, 3, 4};
const uint8_t kReadWidth[9] = {4, 1, 2, 2, 1, 4, 4, 4, 4};
// Snapshot slots that name fields: 1800:01 COB-ID and 1800:05 event timer.
const unsigned kCobSlot = 0, kEventSlot = 3;
Status error(ReadError reason, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}
bool literal(Tpdo1EventTimerRaw v) {
    return v == Tpdo1EventTimerRaw::RAW_0 || v == Tpdo1EventTimerRaw::RAW_100;
}
// The map change preserves event 0, so its request names RAW_0: one meaning for the field.
bool validRequest(const Tpdo1ConfigurationRequest& r) {
    return (r.transition == Tpdo1Transition::MAP3_TO_MAP2 &&
            r.eventTimer == Tpdo1EventTimerRaw::RAW_0) ||
           (r.transition == Tpdo1Transition::EVENT_TIMER && literal(r.eventTimer));
}
bool timer(const Tpdo1ConfigurationContext& c) {
    return c.request.transition == Tpdo1Transition::EVENT_TIMER;
}
unsigned writes(const Tpdo1ConfigurationContext& c) { return timer(c) ? 1 : 6; }
// Writes are steps 1..W, reads W+1..W+9; meaningful in WRITING and READING_BACK only.
uint8_t step(const Tpdo1ConfigurationContext& c) {
    return uint8_t(c.state == PdoConfigurationState::READING_BACK ? writes(c) + 1 + c.cursor
                                                                   : 1 + c.cursor);
}
bool active(const Tpdo1ConfigurationContext& c) {
    return c.state == PdoConfigurationState::WRITING ||
           c.state == PdoConfigurationState::READING_BACK;
}
bool preOperational(const AxisReservation& r, CANOPEN::NmtState nmt,
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
Status current(const Tpdo1ConfigurationContext& c, const AxisReservation& live,
               const Binding& binding, uint64_t now) {
    if (!sameReservation(c.reservation, live) || !sameBinding(binding, expectedBinding(c)))
        return error(ReadError::TARGET, "PDO configuration reservation or binding changed");
    if (c.state == PdoConfigurationState::EMPTY || c.state > PdoConfigurationState::FAILED ||
        !validRequest(c.request) || !c.operation || !c.maximumAgeUs ||
        !c.options.queueTimeoutUs || !c.options.responseTimeoutUs || now < c.servicedUs ||
        c.deadlineUs <= c.startedUs ||
        c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX ||
        (c.state == PdoConfigurationState::WRITING && c.cursor >= writes(c)) ||
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
// Stricter than the shared terminal checks: a read with a reviewed effect claims none, and this
// exact sequence also refuses any queued client abort.
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
bool validBaseline(const CANOPEN::SdoCompletion& e, const AxisReservation& r, unsigned slot,
                   uint64_t earliest, uint64_t now, uint64_t age) {
    const CANOPEN::SdoTransfer& t = e.transfer;
    return CANOPEN::sameSdoChannel(t.channel, r.target.channel) &&
           e.nodeGeneration == r.target.nodeGeneration && t.operation && t.request && t.step &&
           t.index == kReadIndex[slot] && t.subindex == kReadSub[slot] &&
           t.size == kReadWidth[slot] && t.direction == CANOPEN::SdoDirection::UPLOAD &&
           t.requirePreOperational && t.uploadSize == CANOPEN::SdoUploadSize::EXACT &&
           t.readEffect <= CANOPEN::SdoReadEffect::ACCEPTED_UNQUALIFIED && t.queueTimeoutUs &&
           t.responseTimeoutUs && e.admittedUs >= earliest && e.admittedUs <= e.completedUs &&
           e.completedUs <= now && e.completedUs < t.deadlineUs && now - e.admittedUs <= age &&
           e.outcome == CANOPEN::SdoOutcome::SUCCESS && consistent(e);
}
// The exact baseline each transition accepts. The shared fields are the measured specimens'
// TPDO1 (packet 6 transition record); nothing is defaulted from the EDS.
bool validBaselineValues(const uint32_t (&v)[9], const Tpdo1ConfigurationRequest& request,
                         const Binding& binding) {
    if (v[1] != 255 || v[2] != 100 || v[4] != 3 || v[5] != 0x60410010u ||
        v[6] != 0x60610008u || v[7] != 0x60640020u || v[8] != 0)
        return false;
    if (request.transition == Tpdo1Transition::MAP3_TO_MAP2) {
        const uint32_t allowedCob = 0x400007ffu;
        return !(v[kCobSlot] & ~allowedCob) && (v[kCobSlot] & 0x7ffu) && v[kEventSlot] == 0;
    }
    // The EDS rule [1800sub1] DefaultValue=$NODEID+0x180 (EDS line 810), valid, standard and with
    // bit 30 clear, as read on nodes 1 and 2. Any other value is another configuration, possibly
    // another node's identifier, and is not changed.
    const uint32_t node = binding.endpoint.address;
    const uint32_t other = request.eventTimer == Tpdo1EventTimerRaw::RAW_0 ? 100 : 0;
    return node >= 1 && node <= 127 && v[kCobSlot] == 0x180 + node && v[kEventSlot] == other;
}
uint32_t expectedValue(const Tpdo1ConfigurationContext& c, unsigned slot) {
    if (timer(c))
        return slot == kEventSlot ? uint32_t(c.request.eventTimer) : c.original[slot];
    return slot == 4 ? 2 : slot == 6 ? 0x60640020u : c.original[slot];
}
// The map the final readback describes; prepare checks it before any write, get publishes it.
Status publishedLayout(const Tpdo1ConfigurationRequest& request, const Binding& binding,
                       CANOPEN::PdoLayout& output) {
    CANOPEN::PdoMap map;
    map.direction = CANOPEN::PdoDirection::TPDO;
    map.number = 1;
    map.words[0] = 0x60410010u;
    if (request.transition == Tpdo1Transition::EVENT_TIMER) {
        map.count = 3;
        map.words[1] = 0x60610008u;
        map.words[2] = 0x60640020u;
    } else {
        map.count = 2;
        map.words[1] = 0x60640020u;
    }
    return CANOPEN::validatePdoMap(map, pdoProfile(binding.endpoint.profile), output);
}
CANOPEN::SdoTransfer work(const Tpdo1ConfigurationContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.requirePreOperational = true;
    t.operation = t.request = c.operation;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.queueTimeoutUs;
    t.responseTimeoutUs = c.options.responseTimeoutUs;
    t.step = step(c);
    if (c.state == PdoConfigurationState::READING_BACK) {
        t.channel.binding = expectedBinding(c);
        t.index = kReadIndex[c.cursor];
        t.subindex = kReadSub[c.cursor];
        t.size = kReadWidth[c.cursor];
        return t;
    }
    uint32_t value = 0;
    if (timer(c)) {
        t.index = 0x1800;
        t.subindex = 5;
        t.size = 2;
        value = uint32_t(c.request.eventTimer);
    } else {
        // Documentary order: disable, count 0, slot 1, slot 2, count 2, enable.
        const uint16_t index[6] = {0x1800, 0x1a00, 0x1a00, 0x1a00, 0x1a00, 0x1800};
        const uint8_t sub[6] = {1, 0, 1, 2, 0, 1};
        const uint8_t width[6] = {4, 1, 4, 4, 1, 4};
        const uint32_t values[6] = {c.original[kCobSlot] | 0x80000000u, 0, 0x60410010u,
                                    0x60640020u, 2, c.original[kCobSlot]};
        t.index = index[c.cursor];
        t.subindex = sub[c.cursor];
        t.size = width[c.cursor];
        value = values[c.cursor];
    }
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    for (unsigned i = 0; i < t.size; ++i)
        t.data[i] = uint8_t(value >> (8 * i));
    return t;
}
// Called only while active, so step() still names the step that ended the sequence.
void fail(Tpdo1ConfigurationContext& c, ReadOutcome reason, uint64_t now, const char* message) {
    c.failedStep = step(c);
    c.state = PdoConfigurationState::FAILED;
    c.outcome = reason;
    c.servicedUs = now;
    c.mapConfirmed = false;
    c.uncertain |= c.effectsPossible;
    c.status =
        reason == ReadOutcome::PROTOCOL_REJECTED
            ? Status(Err::PROTOCOL_ERROR, 0, message)
            : error(reason == ReadOutcome::DEADLINE || reason == ReadOutcome::SERVICE_TIMEOUT
                        ? ReadError::TIME
                        : ReadError::STATE,
                    message);
}
} // namespace

Status prepareTpdo1Configuration(Tpdo1ConfigurationContext& output, const AxisReservation& r,
                                 const Tpdo1ConfigurationReview& review,
                                 const CANOPEN::SdoCompletion* baseline, size_t count,
                                 CANOPEN::NmtState nmt, const CANOPEN::ObservationTime& preOp,
                                 uint32_t operation, uint64_t now, uint64_t deadline,
                                 const ReadOptions& options, uint64_t age) noexcept {
    Tpdo1ConfigurationRequest request;
    request.transition = Tpdo1Transition::MAP3_TO_MAP2;
    request.eventTimer = Tpdo1EventTimerRaw::RAW_0;
    return prepareTpdo1Configuration(output, r, review, request, baseline, count, nmt, preOp,
                                     operation, now, deadline, options, age);
}
Status prepareTpdo1Configuration(Tpdo1ConfigurationContext& output, const AxisReservation& r,
                                 const Tpdo1ConfigurationReview& review,
                                 const Tpdo1ConfigurationRequest& request,
                                 const CANOPEN::SdoCompletion* baseline, size_t count,
                                 CANOPEN::NmtState nmt, const CANOPEN::ObservationTime& preOp,
                                 uint32_t operation, uint64_t now, uint64_t deadline,
                                 const ReadOptions& options, uint64_t age) noexcept {
    if (!baseline || count != 9)
        return error(ReadError::ARGUMENT, "PDO baseline requires nine exact readbacks");
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &review, sizeof review) ||
        overlaps(&output, sizeof output, &request, sizeof request) ||
        overlaps(&output, sizeof output, baseline, count * sizeof *baseline) ||
        overlaps(&output, sizeof output, &preOp, sizeof preOp) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "PDO preparation overlaps input");
    if (!validRequest(request) || !validReservation(r) ||
        r.target.channel.binding.configurationGeneration == UINT32_MAX || !operation ||
        deadline <= now || !options.queueTimeoutUs || !options.responseTimeoutUs ||
        !sameTarget(review.target, r.target) ||
        (review.scope != PdoConfigurationScope::ISOLATED_EXPERIMENT &&
         review.scope != PdoConfigurationScope::FIXTURE_QUALIFIED) ||
        !review.fixtureRecord || !review.firmwareRecord || !review.procedureRecord ||
        !review.authorizationRecord || !preOperational(r, nmt, preOp, now, age))
        return error(ReadError::ARGUMENT,
                     "unqualified PDO request, scope, reservation, NMT or budget");
    // The 1800/1A00 ledger entries bind no model, and every event-timer measurement is CL86-C:
    // the OL86-C scope of the 86C manuals does not carry this transition.
    if (request.transition == Tpdo1Transition::EVENT_TIMER && r.model != SettingsModel::CL86_C)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ReadError::TARGET),
                      "TPDO1 event-timer change measured on CL86-C only");
    uint64_t earliest = preOp.processedUs;
    uint32_t values[9];
    for (unsigned i = 0; i < 9; ++i) {
        if (!validBaseline(baseline[i], r, i, earliest, now, age))
            return error(ReadError::CORRELATION,
                         "PDO baseline is not fresh exact sequential evidence");
        earliest = baseline[i].completedUs;
        values[i] = raw(baseline[i]);
    }
    CANOPEN::PdoLayout layout;
    if (!validBaselineValues(values, request, r.target.channel.binding) ||
        !publishedLayout(request, r.target.channel.binding, layout))
        return error(ReadError::VALUE, "PDO baseline outside the reviewed TPDO1 transition");
    Tpdo1ConfigurationContext c;
    c.reservation = r;
    c.review = review;
    c.request = request;
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
Status nextPdoConfiguration(const Tpdo1ConfigurationContext& c, const AxisReservation& live,
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
                                        const AxisReservation& live, Binding& binding,
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
    if (!c.effectsPossible) {
        ++binding.configurationGeneration;
        c.firstEffectStep = step(c);
    }
    c.effectsPossible = c.transmissionNoted = c.uncertain = true;
    c.mapConfirmed = false;
    c.servicedUs = now;
    return Ok();
}
Status advancePdoConfiguration(Tpdo1ConfigurationContext& c, const AxisReservation& live,
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
    if (!CANOPEN::sameSdoWork(e.transfer, expected) ||
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
        fail(c, readOutcome(e.outcome), now, "PDO configuration service failed");
    } else if (c.state == PdoConfigurationState::WRITING) {
        c.confirmedStep = step(c);
        c.transmissionNoted = false;
        c.stepStartedUs = now;
        if (++c.cursor == writes(c)) {
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
            c.confirmedStep = step(c);
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
Status getTpdo1Configuration(const Tpdo1ConfigurationContext& c, const AxisReservation& live,
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
        c.acknowledgedMask != (1u << writes(c)) - 1 || !generation || !evidenceId ||
        c.readbackStartedUs > c.readbackCompletedUs || c.readbackCompletedUs > now ||
        now - c.readbackStartedUs > c.maximumAgeUs)
        return error(ReadError::STATE, "no complete fresh PDO configuration readback");
    CANOPEN::PdoMapEvidence candidate;
    candidate.route.binding = binding;
    candidate.route.generation = generation;
    candidate.route.cobId = c.original[kCobSlot] & 0x7ffu;
    const Status layout = publishedLayout(c.request, binding, candidate.route.layout);
    if (!layout)
        return layout;
    candidate.nodeGeneration = c.reservation.target.nodeGeneration;
    candidate.startedUs = c.readbackStartedUs;
    candidate.observedUs = c.readbackCompletedUs;
    candidate.source = CANOPEN::PdoMapSource::SDO_READBACK;
    candidate.evidenceId = evidenceId;
    candidate.communicationKnown = true;
    candidate.cobIdParameter = c.original[kCobSlot];
    candidate.transmissionType = 255;
    output = candidate;
    return Ok();
}
Status getTpdo1Configuration(const Tpdo1ConfigurationContext& c, const AxisReservation& live,
                             const Binding& binding, uint64_t now, Generation generation,
                             uint32_t evidenceId, CANOPEN::PdoMapEvidence& output,
                             Tpdo1EventTimerRaw& eventTimer) noexcept {
    if (overlaps(&eventTimer, sizeof eventTimer, &output, sizeof output) ||
        overlaps(&eventTimer, sizeof eventTimer, &c, sizeof c) ||
        overlaps(&eventTimer, sizeof eventTimer, &live, sizeof live) ||
        overlaps(&eventTimer, sizeof eventTimer, &binding, sizeof binding) ||
        overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &binding, sizeof binding))
        return error(ReadError::OVERLAP, "PDO result overlaps input");
    CANOPEN::PdoMapEvidence candidate;
    const Status published =
        getTpdo1Configuration(c, live, binding, now, generation, evidenceId, candidate);
    if (!published)
        return published;
    // A successful context matched its readback against the requested literal, so the read
    // value is one of the two literals by construction; recheck rather than cast blindly.
    const Tpdo1EventTimerRaw read = Tpdo1EventTimerRaw(uint16_t(c.readback[kEventSlot]));
    if (c.readback[kEventSlot] > UINT16_MAX || !literal(read) || read != c.request.eventTimer)
        return error(ReadError::STATE, "PDO event timer readback outside the request");
    output = candidate;
    eventTimer = read;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
