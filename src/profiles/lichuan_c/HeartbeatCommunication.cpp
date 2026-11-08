// SPDX-License-Identifier: MIT
#include "ProfileEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
Status error(ReadError reason, const char* text) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), text);
}
bool reviewed(const CommunicationReview& review, const AxisReservation& r) {
    return sameTarget(review.target, r.target) &&
           (review.scope == CommunicationScope::ISOLATED_EXPERIMENT ||
            review.scope == CommunicationScope::FIXTURE_QUALIFIED) &&
           review.fixtureRecord && review.firmwareRecord && review.procedureRecord &&
           review.authorizationRecord;
}
bool nmtEligible(const AxisReservation& r, const CommunicationReview& review,
                 bool requirePreOperational, const CommunicationNmt& nmt, uint64_t now,
                 uint64_t age) {
    if (!nmt.known)
        return !requirePreOperational && review.scope == CommunicationScope::ISOLATED_EXPERIMENT;
    const CANOPEN::ObservationTime& t = nmt.time;
    if (nmt.state != CANOPEN::NmtState::PRE_OPERATIONAL || !t.present ||
        t.generation != r.target.nodeGeneration || t.receivedTimeKnown == t.receiveBoundsKnown ||
        t.processedUs > now)
        return false;
    const uint64_t first = t.receivedTimeKnown ? t.receivedUs : t.receivedEarliestUs;
    const uint64_t last = t.receivedTimeKnown ? t.receivedUs : t.receivedLatestUs;
    return first >= r.acquiredUs && first <= last && last <= t.processedUs && now - first <= age;
}
Binding expectedBinding(const CommunicationContext& c) {
    Binding b = c.reservation.target.channel.binding;
    if (c.effectsPossible)
        ++b.configurationGeneration;
    return b;
}
bool active(const CommunicationContext& c) {
    return c.state == CommunicationState::WRITING || c.state == CommunicationState::READING_BACK;
}
bool workEligible(const CommunicationContext& c, const AxisReservation& live,
                  const CommunicationNmt& nmt, uint64_t now) {
    return exchangeFits(c.deadlineUs, now, c.options.queueTimeoutUs, c.options.responseTimeoutUs) &&
           nmtEligible(live, c.review, c.requirePreOperational, nmt, now, c.maximumAgeUs) &&
           (c.state != CommunicationState::WRITING ||
            (now >= c.baseline.admittedUs && now - c.baseline.admittedUs <= c.maximumAgeUs));
}
Status current(const CommunicationContext& c, const AxisReservation& live, const Binding& binding,
               uint64_t now) {
    if (!sameReservation(c.reservation, live) || !sameBinding(binding, expectedBinding(c)))
        return error(ReadError::TARGET, "communication reservation or binding changed");
    if (c.state == CommunicationState::EMPTY || c.state > CommunicationState::FAILED ||
        c.request.producerTimeRaw != 1000 || !reviewed(c.review, c.reservation) ||
        c.reservation.model != SettingsModel::CL86_C || !c.operation || !c.maximumAgeUs ||
        !c.options.queueTimeoutUs || !c.options.responseTimeoutUs ||
        c.options.identityWidths != IdentityWidths::DECLARED_U16 ||
        c.options.countWidths != CountWidths::DECLARED_U8 || c.options.pdoNumber ||
        now < c.servicedUs || c.deadlineUs <= c.startedUs ||
        c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX ||
        c.prerequisitesWithdrawn != c.effectsPossible || c.transmissionNoted != c.effectsPossible ||
        ((c.state == CommunicationState::AWAITING_CONFIRMATION ||
          c.state == CommunicationState::READING_BACK ||
          c.state == CommunicationState::SUCCEEDED) &&
         (!c.effectsPossible || !c.writeAcknowledged || c.writeOutcome != ReadOutcome::SUCCESS)))
        return error(ReadError::STATE, "invalid communication workflow state or time");
    return Ok();
}
bool sameCurrentTarget(const CommunicationContext& c, const ReadTarget& t) {
    CANOPEN::SdoChannel expected = c.reservation.target.channel;
    expected.binding = expectedBinding(c);
    return t.nodeGeneration == c.reservation.target.nodeGeneration &&
           CANOPEN::sameSdoChannel(t.channel, expected);
}
CANOPEN::SdoTransfer work(const CommunicationContext& c, bool confirmation) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.index = 0x1017;
    t.size = 2;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.queueTimeoutUs;
    t.responseTimeoutUs = c.options.responseTimeoutUs;
    t.requirePreOperational = c.requirePreOperational;
    if (confirmation) {
        t.step = 2;
        t.channel.binding = expectedBinding(c);
        // The new upload remains Q20-unqualified, even after an acknowledged download.
        t.readEffect = CANOPEN::SdoReadEffect::UNQUALIFIED;
        t.requireUploadReview = true;
    } else {
        t.step = 1;
        t.direction = CANOPEN::SdoDirection::DOWNLOAD;
        t.data[0] = 0xe8;
        t.data[1] = 0x03;
    }
    return t;
}
Status failureStatus(ReadOutcome reason) {
    if (reason == ReadOutcome::PROTOCOL_REJECTED)
        return Status(Err::PROTOCOL_ERROR, 0, "communication native service rejected");
    return error(reason == ReadOutcome::DEADLINE || reason == ReadOutcome::SERVICE_TIMEOUT
                     ? ReadError::TIME
                     : ReadError::STATE,
                 "communication operation failed");
}
void fail(CommunicationContext& c, ReadOutcome reason, uint64_t now) {
    if (c.state == CommunicationState::WRITING) {
        c.writeOutcome = reason;
        c.writeStatus = failureStatus(reason);
    }
    c.state = CommunicationState::FAILED;
    c.outcome = reason;
    c.status = failureStatus(reason);
    c.servicedUs = now;
    c.uncertain |= c.effectsPossible;
}
} // namespace

Status prepareCommunication(CommunicationContext& output, const AxisReservation& r,
                            const CommunicationReview& review, const CommunicationRequest& request,
                            const CANOPEN::SdoCompletion& baseline, const CommunicationNmt& nmt,
                            uint32_t operation, uint64_t now, uint64_t deadline,
                            const ReadOptions& options, uint64_t age) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &review, sizeof review) ||
        overlaps(&output, sizeof output, &request, sizeof request) ||
        overlaps(&output, sizeof output, &baseline, sizeof baseline) ||
        overlaps(&output, sizeof output, &nmt, sizeof nmt) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "communication preparation overlaps input");
    if (!validReservation(r) || !reviewed(review, r) || !operation || !age ||
        request.producerTimeRaw != 1000 ||
        r.target.channel.binding.configurationGeneration == UINT32_MAX || !options.queueTimeoutUs ||
        !options.responseTimeoutUs || options.identityWidths != IdentityWidths::DECLARED_U16 ||
        options.countWidths != CountWidths::DECLARED_U8 || options.pdoNumber ||
        !exchangeFits(deadline, now, options.queueTimeoutUs, options.responseTimeoutUs) ||
        !nmtEligible(r, review, nmt.known, nmt, now, age))
        return error(ReadError::ARGUMENT, "unreviewed communication candidate, state or budget");
    if (r.model != SettingsModel::CL86_C)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ReadError::TARGET),
                      "heartbeat transition measured on CL86-C only");
    const CANOPEN::SdoTransfer& t = baseline.transfer;
    const uint64_t earliest = nmt.known ? nmt.time.processedUs : r.acquiredUs;
    if (!CANOPEN::sameSdoChannel(t.channel, r.target.channel) ||
        baseline.nodeGeneration != r.target.nodeGeneration || !t.operation || !t.request ||
        !t.step || t.index != 0x1017 || t.subindex || t.size != 2 ||
        t.direction != CANOPEN::SdoDirection::UPLOAD ||
        t.uploadSize != CANOPEN::SdoUploadSize::EXACT ||
        (t.readEffect != CANOPEN::SdoReadEffect::UNQUALIFIED &&
         t.readEffect != CANOPEN::SdoReadEffect::ACCEPTED_UNQUALIFIED) ||
        (nmt.known && !t.requirePreOperational) || !t.queueTimeoutUs || !t.responseTimeoutUs ||
        baseline.admittedUs < earliest || baseline.admittedUs > baseline.completedUs ||
        baseline.completedUs > now || baseline.completedUs >= t.deadlineUs ||
        now - baseline.admittedUs > age || baseline.outcome != CANOPEN::SdoOutcome::SUCCESS ||
        !CANOPEN::checkUploadTerminal(baseline, 2))
        return error(ReadError::CORRELATION,
                     "heartbeat baseline is not fresh exact service evidence");
    if (baseline.data[0] || baseline.data[1])
        return error(ReadError::VALUE, "heartbeat transition requires stored raw zero baseline");
    CommunicationContext c;
    c.reservation = r;
    c.review = review;
    c.request = request;
    c.options = options;
    c.maximumAgeUs = age;
    c.startedUs = c.servicedUs = c.stepStartedUs = now;
    c.deadlineUs = deadline;
    c.operation = operation;
    c.requirePreOperational = nmt.known;
    c.baseline = baseline;
    c.state = CommunicationState::WRITING;
    output = c;
    return Ok();
}

Status prepareCommunicationConfirmation(CommunicationContext& c, const AxisReservation& live,
                                        const ReadTarget& target, const CommunicationNmt& nmt,
                                        uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &target, sizeof target) || overlaps(&c, sizeof c, &nmt, sizeof nmt))
        return error(ReadError::OVERLAP, "communication confirmation overlaps input");
    const Status checked = current(c, live, target.channel.binding, now);
    if (!checked)
        return checked;
    if (c.state != CommunicationState::AWAITING_CONFIRMATION || !sameCurrentTarget(c, target) ||
        !exchangeFits(c.deadlineUs, now, c.options.queueTimeoutUs, c.options.responseTimeoutUs) ||
        !nmtEligible(live, c.review, c.requirePreOperational, nmt, now, c.maximumAgeUs))
        return error(ReadError::STATE, "no eligible acknowledged communication confirmation");
    c.state = CommunicationState::READING_BACK;
    c.stepStartedUs = c.servicedUs = now;
    return Ok();
}

Status nextCommunication(const CommunicationContext& c, const AxisReservation& live,
                         const Binding& binding, const CommunicationNmt& nmt, uint64_t now,
                         PreparedRead& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &binding, sizeof binding) ||
        overlaps(&output, sizeof output, &nmt, sizeof nmt))
        return error(ReadError::OVERLAP, "communication work overlaps input");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    if (!active(c)) {
        output.ready = false;
        return Ok();
    }
    if (!workEligible(c, live, nmt, now))
        return error(ReadError::TIME, "communication service budget or state evidence expired");
    PreparedRead candidate;
    candidate.ready = true;
    candidate.transfer = work(c, c.state == CommunicationState::READING_BACK);
    output = candidate;
    return Ok();
}

Status noteCommunicationTransmission(CommunicationContext& c, const AxisReservation& live,
                                     Binding& binding, const CommunicationNmt& nmt,
                                     uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &binding, sizeof binding) ||
        overlaps(&c, sizeof c, &nmt, sizeof nmt) ||
        overlaps(&binding, sizeof binding, &live, sizeof live) ||
        overlaps(&binding, sizeof binding, &nmt, sizeof nmt))
        return error(ReadError::OVERLAP, "communication notice overlaps mutable inputs");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    if (c.state != CommunicationState::WRITING || c.transmissionNoted ||
        !workEligible(c, live, nmt, now))
        return error(ReadError::STATE, "no eligible communication download notice");
    ++binding.configurationGeneration;
    c.transmissionNoted = c.effectsPossible = c.prerequisitesWithdrawn = c.uncertain = true;
    c.servicedUs = now;
    return Ok();
}

Status advanceCommunication(CommunicationContext& c, const AxisReservation& live,
                            const Binding& binding, const CANOPEN::SdoCompletion& e,
                            uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &binding, sizeof binding) || overlaps(&c, sizeof c, &e, sizeof e))
        return error(ReadError::OVERLAP, "communication terminal overlaps input");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    if (!active(c))
        return error(ReadError::STATE, "communication has no active native step");
    const bool writing = c.state == CommunicationState::WRITING;
    const CANOPEN::SdoTransfer expected = work(c, !writing);
    if (!CANOPEN::sameSdoWork(e.transfer, expected) ||
        e.nodeGeneration != c.reservation.target.nodeGeneration || e.admittedUs < c.stepStartedUs ||
        e.admittedUs > e.completedUs || e.admittedUs >= c.deadlineUs ||
        e.completedUs < c.servicedUs || e.completedUs > now ||
        (writing ? !CANOPEN::checkDownloadTerminal(e, c.transmissionNoted)
                 : !CANOPEN::checkUploadTerminal(e, 2)))
        return error(ReadError::CORRELATION, "communication terminal does not match exact step");
    c.servicedUs = now;
    c.uncertain |= e.effectUncertain;
    if (writing) {
        c.writeEvidence = e;
        c.writeEvidencePresent = true;
        c.writeAcknowledged = e.outcome == CANOPEN::SdoOutcome::SUCCESS;
    } else {
        c.confirmationEvidence = e;
        c.confirmationPresent = true;
    }
    if (now >= c.deadlineUs || e.completedUs >= c.deadlineUs) {
        fail(c, ReadOutcome::DEADLINE, now);
    } else if (e.outcome != CANOPEN::SdoOutcome::SUCCESS) {
        fail(c, readOutcome(e.outcome), now);
    } else if (writing) {
        c.writeOutcome = ReadOutcome::SUCCESS;
        c.writeStatus = Ok();
        c.state = CommunicationState::AWAITING_CONFIRMATION;
    } else {
        c.readbackRaw = uint16_t(e.data[0] | (uint16_t(e.data[1]) << 8));
        c.readbackKnown = true;
        if (c.readbackRaw != 1000) {
            fail(c, ReadOutcome::REPLY_ERROR, now);
            c.status =
                error(ReadError::VALUE, "heartbeat readback differs from requested raw 1000");
        } else {
            c.readbackMatched = true;
            c.state = CommunicationState::SUCCEEDED;
            c.outcome = ReadOutcome::SUCCESS;
            c.status = Ok();
            c.uncertain = false;
        }
    }
    return Ok();
}

Status advanceCommunication(CommunicationContext& c, ReadOutcome reason, uint64_t now) noexcept {
    if ((reason != ReadOutcome::CANCELLED && reason != ReadOutcome::DEADLINE &&
         reason != ReadOutcome::INVALIDATED) ||
        (!active(c) && c.state != CommunicationState::AWAITING_CONFIRMATION) ||
        now < c.servicedUs || (reason == ReadOutcome::DEADLINE && now < c.deadlineUs))
        return error(ReadError::STATE, "invalid communication local withdrawal");
    fail(c, now >= c.deadlineUs ? ReadOutcome::DEADLINE : reason, now);
    return Ok();
}

Status getCommunication(const CommunicationContext& c, const AxisReservation& live,
                        const ReadTarget& target, uint64_t now,
                        CommunicationResult& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &target, sizeof target))
        return error(ReadError::OVERLAP, "communication result overlaps input");
    const Status checked = current(c, live, target.channel.binding, now);
    if (!checked)
        return checked;
    if (!sameCurrentTarget(c, target) || c.state != CommunicationState::SUCCEEDED ||
        c.outcome != ReadOutcome::SUCCESS || !c.writeAcknowledged || !c.readbackKnown ||
        !c.readbackMatched || c.readbackRaw != 1000 || !c.confirmationPresent || c.uncertain ||
        !c.writeEvidencePresent || c.writeEvidence.outcome != CANOPEN::SdoOutcome::SUCCESS ||
        !CANOPEN::sameSdoWork(c.writeEvidence.transfer, work(c, false)) ||
        !CANOPEN::checkDownloadTerminal(c.writeEvidence, c.transmissionNoted) ||
        c.writeEvidence.nodeGeneration != c.reservation.target.nodeGeneration ||
        c.writeEvidence.admittedUs < c.startedUs ||
        c.writeEvidence.admittedUs > c.writeEvidence.completedUs ||
        c.writeEvidence.completedUs > c.stepStartedUs ||
        c.writeEvidence.completedUs >= c.deadlineUs ||
        c.confirmationEvidence.outcome != CANOPEN::SdoOutcome::SUCCESS ||
        !CANOPEN::sameSdoWork(c.confirmationEvidence.transfer, work(c, true)) ||
        !CANOPEN::checkUploadTerminal(c.confirmationEvidence, 2) ||
        c.confirmationEvidence.nodeGeneration != c.reservation.target.nodeGeneration ||
        c.confirmationEvidence.admittedUs < c.stepStartedUs ||
        c.confirmationEvidence.admittedUs > c.confirmationEvidence.completedUs ||
        c.confirmationEvidence.completedUs > c.servicedUs ||
        c.confirmationEvidence.completedUs >= c.deadlineUs ||
        c.confirmationEvidence.data[0] != 0xe8 || c.confirmationEvidence.data[1] != 3 ||
        c.confirmationEvidence.admittedUs > now ||
        now - c.confirmationEvidence.admittedUs > c.maximumAgeUs)
        return error(ReadError::STATE, "communication result incomplete, stale or invalidated");
    CommunicationResult candidate;
    candidate.target = target;
    candidate.producerTimeRaw = c.readbackRaw;
    candidate.scope = c.review.scope;
    candidate.window.earliestUs = c.confirmationEvidence.admittedUs;
    candidate.window.latestUs = c.confirmationEvidence.completedUs;
    candidate.window.skewUs = candidate.window.latestUs - candidate.window.earliestUs;
    output = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
