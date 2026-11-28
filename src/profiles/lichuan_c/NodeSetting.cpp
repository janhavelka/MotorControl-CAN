// SPDX-License-Identifier: MIT
// Prompt 61 node setting (decision 4): one 2203 write and its readback on the original route, then at most
// two read-only confirmation slots on exact candidates (MotorControl-RS prepareCommunicationConfirmation).
#include "ProfileEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
constexpr uint16_t NODE_OBJECT = 0x2203;
Status error(ReadError reason, const char* text) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), text);
}
bool listed(const NodeSet& set, unsigned node) {
    return node < 128 && (set.words[node / 32] >> (node % 32) & 1u);
}
unsigned nodeOf(const ReadTarget& t) {
    return t.channel.binding.endpoint.address;
}
Binding expectedBinding(const NodeSettingContext& c) {
    Binding b = c.reservation.target.channel.binding;
    if (c.effectsPossible)
        ++b.configurationGeneration;
    return b;
}
bool writeActive(const NodeSettingContext& c) {
    return c.state == NodeSettingState::WRITING || c.state == NodeSettingState::READING_BACK;
}
// After the write's terminal: acknowledged and stored, or failed after a possible download.
bool writeTerminal(const NodeSettingContext& c) {
    return c.state == NodeSettingState::STORED || (c.state == NodeSettingState::FAILED && c.effectsPossible);
}
// Project policy (no source gives a state for 2203): a clean STATUS and MONITORS read of this boot under the
// reservation, 2100 released with no running, alarm or undocumented bit.
bool releasedState(const ReadContext& s, const AxisReservation& r, uint64_t now, uint64_t age) {
    ReadWindow status, monitors;
    if (s.source != ReadSource::SDO || s.state != ReadState::SUCCEEDED || s.uncertain ||
        !validateStateBlock(s, StateBlock::STATUS, r.target, now, age, status) ||
        !validateStateBlock(s, StateBlock::MONITORS, r.target, now, age, monitors) ||
        status.earliestUs < r.acquiredUs || monitors.earliestUs < r.acquiredUs)
        return false;
    const uint32_t word = s.values[s.blockStart[static_cast<unsigned>(StateBlock::MONITORS)]].raw;
    return (word & MONITOR_RELEASED) &&
           !(word & (MONITOR_RUNNING | MONITOR_ALARM | ~uint32_t(MONITOR_DOCUMENTED_BITS)));
}
CANOPEN::SdoTransfer work(const NodeSettingContext& c, uint8_t step) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.step = step;
    t.index = NODE_OBJECT;
    t.size = 2;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.queueTimeoutUs;
    t.responseTimeoutUs = c.options.responseTimeoutUs;
    if (step == 1) {
        t.direction = CANOPEN::SdoDirection::DOWNLOAD;
        t.data[0] = c.request.node;
        return t;
    }
    // Every 2203 upload stays Q20-unqualified: an exact one-use reviewed acquisition.
    t.readEffect = CANOPEN::SdoReadEffect::UNQUALIFIED;
    t.requireUploadReview = true;
    if (step == 2) {
        t.channel.binding = expectedBinding(c);
    } else {
        const NodeSettingConfirmation& slot = c.confirmations[step - 3];
        t.channel = slot.target.channel;
        t.deadlineUs = slot.deadlineUs;
    }
    return t;
}
uint8_t activeStep(const NodeSettingContext& c) {
    if (c.state == NodeSettingState::WRITING)
        return 1;
    if (c.state == NodeSettingState::READING_BACK)
        return 2;
    return c.confirmationActive ? uint8_t(2 + c.confirmationsUsed) : 0;
}
Status checked(const NodeSettingContext& c, uint64_t now) {
    if (c.state == NodeSettingState::EMPTY || c.state > NodeSettingState::FAILED || !c.operation ||
        !c.request.node || c.request.node > 127 || c.request.node == c.beforeNode || now < c.servicedUs ||
        c.confirmationsUsed > NODE_SETTING_CONFIRMATIONS || (c.confirmationActive && !c.confirmationsUsed) ||
        (c.confirmationActive && writeActive(c)) || c.transmissionNoted != c.effectsPossible)
        return error(ReadError::STATE, "invalid node-setting workflow state or time");
    return Ok();
}
Status failureStatus(ReadOutcome reason) {
    if (reason == ReadOutcome::PROTOCOL_REJECTED)
        return Status(Err::PROTOCOL_ERROR, 0, "node-setting native service rejected");
    return error(reason == ReadOutcome::DEADLINE || reason == ReadOutcome::SERVICE_TIMEOUT ? ReadError::TIME
                                                                                         : ReadError::STATE,
                 "node-setting operation failed");
}
void fail(NodeSettingContext& c, ReadOutcome reason, uint64_t now) {
    if (c.state == NodeSettingState::WRITING)
        c.writeOutcome = reason;
    c.state = NodeSettingState::FAILED;
    c.outcome = reason;
    c.status = failureStatus(reason);
    c.servicedUs = now;
    c.uncertain |= c.effectsPossible;
}
void endSlot(NodeSettingContext& c, ReadOutcome reason, uint64_t now) {
    NodeSettingConfirmation& slot = c.confirmations[c.confirmationsUsed - 1];
    slot.answer = NodeSettingAnswer::NOT_ANSWERED;
    slot.outcome = reason;
    c.confirmationActive = false;
    c.servicedUs = now;
}
} // namespace

Status prepareNodeSetting(NodeSettingContext& output, const AxisReservation& r, const NodeSettingReview& review,
                          const NodeSettingRequest& request, const ReadContext& state, uint32_t operation,
                          uint64_t now, uint64_t deadline, const ReadOptions& options, uint64_t age) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &review, sizeof review) ||
        overlaps(&output, sizeof output, &request, sizeof request) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "node-setting preparation overlaps input");
    if (!validReservation(r) || !sameTarget(review.target, r.target) || !review.dipAttestationRecord ||
        !review.routeBackRecord || !operation || !age ||
        r.target.channel.binding.configurationGeneration == UINT32_MAX || !options.queueTimeoutUs ||
        !options.responseTimeoutUs || options.identityWidths != IdentityWidths::DECLARED_U16 ||
        options.countWidths != CountWidths::DECLARED_U8 || options.pdoNumber ||
        !exchangeFits(deadline, now, options.queueTimeoutUs, options.responseTimeoutUs))
        return error(ReadError::ARGUMENT, "node setting needs DIP attestation, route back and budgets");
    if (r.model != SettingsModel::CL86_C && r.model != SettingsModel::OL86_C)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ReadError::TARGET),
                      "2203 documented for the 86 C manuals only");
    const unsigned before = nodeOf(r.target);
    if (!request.node || request.node > 127 || before < 1 || before > 127 || request.node == before ||
        listed(review.configured, request.node))
        return error(ReadError::VALUE, "node 1..127 that is neither current nor configured required");
    if (!releasedState(state, r, now, age))
        return error(ReadError::STATE, "fresh clean released state of this boot required");
    NodeSettingContext c;
    c.reservation = r;
    c.review = review;
    c.request = request;
    c.beforeNode = uint8_t(before);
    c.options = options;
    c.maximumAgeUs = age;
    c.startedUs = c.servicedUs = c.stepStartedUs = now;
    c.deadlineUs = deadline;
    c.operation = operation;
    c.state = NodeSettingState::WRITING;
    output = c;
    return Ok();
}

Status prepareNodeSettingConfirmation(NodeSettingContext& c, const ReadTarget& candidate, NodeSettingCandidate which,
                                      uint64_t now, uint64_t deadline) noexcept {
    if (overlaps(&c, sizeof c, &candidate, sizeof candidate))
        return error(ReadError::OVERLAP, "node-setting confirmation overlaps input");
    const Status s = checked(c, now);
    if (!s)
        return s;
    if (!writeTerminal(c) || c.confirmationActive || c.confirmationsUsed >= NODE_SETTING_CONFIRMATIONS)
        return error(ReadError::STATE, "a confirmation follows the terminal write, two slots at most");
    const CANOPEN::SdoChannel& original = c.reservation.target.channel;
    const unsigned node = which == NodeSettingCandidate::BEFORE ? c.beforeNode : c.request.node;
    const bool exact =
        which == NodeSettingCandidate::BEFORE
            ? candidate.channel.clientToServerId == original.clientToServerId &&
                  candidate.channel.serverToClientId == original.serverToClientId
            : which == NodeSettingCandidate::REQUESTED && candidate.channel.clientToServerId == 0x600u + node &&
                  candidate.channel.serverToClientId == 0x580u + node;
    if (!exact || nodeOf(candidate) != node || !candidate.nodeGeneration ||
        candidate.channel.binding.endpoint.bus != original.binding.endpoint.bus)
        return error(ReadError::TARGET, "confirmation reads the exact before or requested route only");
    if (!exchangeFits(deadline, now, c.options.queueTimeoutUs, c.options.responseTimeoutUs))
        return error(ReadError::TIME, "confirmation deadline leaves no exchange");
    NodeSettingConfirmation slot;
    slot.candidate = which;
    slot.target = candidate;
    slot.startedUs = now;
    slot.deadlineUs = deadline;
    c.confirmations[c.confirmationsUsed] = slot;
    ++c.confirmationsUsed;
    c.confirmationActive = true;
    c.servicedUs = now;
    return Ok();
}

Status nextNodeSetting(const NodeSettingContext& c, const AxisReservation& live, const Binding& binding,
                       uint64_t now, PreparedRead& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) || overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &binding, sizeof binding))
        return error(ReadError::OVERLAP, "node-setting work overlaps input");
    const Status s = checked(c, now);
    if (!s)
        return s;
    const uint8_t step = activeStep(c);
    if (!step) {
        output.ready = false;
        return Ok();
    }
    if (step <= 2 && (!sameReservation(c.reservation, live) || !sameBinding(binding, expectedBinding(c))))
        return error(ReadError::TARGET, "node-setting reservation or binding changed");
    const NodeSettingConfirmation* slot = step > 2 ? &c.confirmations[step - 3] : nullptr;
    if (slot && !sameBinding(binding, slot->target.channel.binding))
        return error(ReadError::TARGET, "confirmation binding is not the candidate route");
    if (!exchangeFits(slot ? slot->deadlineUs : c.deadlineUs, now, c.options.queueTimeoutUs,
                      c.options.responseTimeoutUs))
        return error(ReadError::TIME, "node-setting service budget expired");
    PreparedRead candidate;
    candidate.ready = true;
    candidate.transfer = work(c, step);
    output = candidate;
    return Ok();
}

Status noteNodeSettingTransmission(NodeSettingContext& c, const AxisReservation& live, Binding& binding,
                                   uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) || overlaps(&c, sizeof c, &binding, sizeof binding) ||
        overlaps(&binding, sizeof binding, &live, sizeof live))
        return error(ReadError::OVERLAP, "node-setting notice overlaps mutable inputs");
    const Status s = checked(c, now);
    if (!s)
        return s;
    if (c.state != NodeSettingState::WRITING || c.transmissionNoted || !sameReservation(c.reservation, live) ||
        !sameBinding(binding, expectedBinding(c)) ||
        !exchangeFits(c.deadlineUs, now, c.options.queueTimeoutUs, c.options.responseTimeoutUs))
        return error(ReadError::STATE, "no eligible node-setting download notice");
    ++binding.configurationGeneration;
    c.transmissionNoted = c.effectsPossible = c.prerequisitesWithdrawn = c.uncertain = true;
    c.servicedUs = now;
    return Ok();
}

Status advanceNodeSetting(NodeSettingContext& c, const AxisReservation& original, const CANOPEN::SdoCompletion& e,
                          uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &original, sizeof original) || overlaps(&c, sizeof c, &e, sizeof e))
        return error(ReadError::OVERLAP, "node-setting terminal overlaps input");
    const Status s = checked(c, now);
    if (!s)
        return s;
    const uint8_t step = activeStep(c);
    if (!step || !sameReservation(c.reservation, original))
        return error(ReadError::STATE, "node setting has no active native step");
    NodeSettingConfirmation* slot = step > 2 ? &c.confirmations[step - 3] : nullptr;
    const uint64_t started = slot ? slot->startedUs : c.stepStartedUs;
    const uint64_t cutoff = slot ? slot->deadlineUs : c.deadlineUs;
    const Generation node = slot ? slot->target.nodeGeneration : c.reservation.target.nodeGeneration;
    if (!CANOPEN::sameSdoWork(e.transfer, work(c, step)) || e.nodeGeneration != node || e.admittedUs < started ||
        e.admittedUs > e.completedUs || e.admittedUs >= cutoff || e.completedUs < c.servicedUs ||
        e.completedUs > now ||
        (step == 1 ? !CANOPEN::checkDownloadTerminal(e, c.transmissionNoted) : !CANOPEN::checkUploadTerminal(e, 2)))
        return error(ReadError::CORRELATION, "node-setting terminal does not match the exact step");
    const bool late = now >= cutoff || e.completedUs >= cutoff;
    const uint16_t raw = uint16_t(e.data[0] | (uint16_t(e.data[1]) << 8));
    if (slot) {
        slot->evidence = e;
        slot->evidencePresent = true;
        if (late || e.outcome != CANOPEN::SdoOutcome::SUCCESS) {
            endSlot(c, late ? ReadOutcome::DEADLINE : readOutcome(e.outcome), now);
        } else {
            slot->answer = NodeSettingAnswer::ANSWERED;
            slot->outcome = ReadOutcome::SUCCESS;
            slot->raw = raw;
            c.confirmationActive = false;
            c.servicedUs = now;
        }
        return Ok();
    }
    c.servicedUs = now;
    c.uncertain |= e.effectUncertain;
    if (step == 1) {
        c.writeEvidence = e;
        c.writeEvidencePresent = true;
        c.writeAcknowledged = e.outcome == CANOPEN::SdoOutcome::SUCCESS;
    }
    if (late) {
        fail(c, ReadOutcome::DEADLINE, now);
    } else if (e.outcome != CANOPEN::SdoOutcome::SUCCESS) {
        fail(c, readOutcome(e.outcome), now);
    } else if (step == 1) {
        // Acknowledged on the old route: not activation. The readback follows at the next epoch.
        c.writeOutcome = ReadOutcome::SUCCESS;
        c.state = NodeSettingState::READING_BACK;
        c.stepStartedUs = now;
    } else {
        c.readbackRaw = raw;
        c.readbackKnown = true;
        if (raw != c.request.node) {
            fail(c, ReadOutcome::REPLY_ERROR, now);
            c.status = error(ReadError::VALUE, "2203 readback differs from the requested node");
        } else {
            c.state = NodeSettingState::STORED;
            c.outcome = ReadOutcome::SUCCESS;
            c.status = Ok();
            c.uncertain = false; // The stored register only; activation stays unknown.
        }
    }
    return Ok();
}

Status advanceNodeSetting(NodeSettingContext& c, ReadOutcome reason, uint64_t now) noexcept {
    if ((reason != ReadOutcome::CANCELLED && reason != ReadOutcome::DEADLINE && reason != ReadOutcome::INVALIDATED) ||
        !checked(c, now) || !activeStep(c))
        return error(ReadError::STATE, "invalid node-setting local withdrawal");
    const uint64_t cutoff = c.confirmationActive ? c.confirmations[c.confirmationsUsed - 1].deadlineUs : c.deadlineUs;
    if (reason == ReadOutcome::DEADLINE && now < cutoff)
        return error(ReadError::STATE, "node-setting deadline not reached");
    const ReadOutcome ended = now >= cutoff ? ReadOutcome::DEADLINE : reason;
    if (c.confirmationActive)
        endSlot(c, ended, now);
    else
        fail(c, ended, now);
    return Ok();
}

Status getNodeSetting(const NodeSettingContext& c, NodeSettingResult& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c))
        return error(ReadError::OVERLAP, "node-setting result overlaps input");
    if (!checked(c, c.servicedUs) || writeActive(c) || c.confirmationActive)
        return error(ReadError::STATE, "node-setting result needs a terminal write and no active slot");
    NodeSettingResult r;
    r.beforeNode = c.beforeNode;
    r.requestedNode = c.request.node;
    r.state = c.state;
    r.writeAcknowledged = c.writeAcknowledged;
    r.stored = c.state == NodeSettingState::STORED;
    r.uncertain = c.uncertain;
    r.confirmationsUsed = c.confirmationsUsed;
    for (uint8_t i = 0; i < c.confirmationsUsed; ++i) {
        const NodeSettingConfirmation& slot = c.confirmations[i];
        (slot.candidate == NodeSettingCandidate::BEFORE ? r.before : r.requested) = slot.answer;
        if (slot.candidate == NodeSettingCandidate::REQUESTED && slot.answer == NodeSettingAnswer::ANSWERED &&
            slot.raw == c.request.node)
            r.answeredOnRequested = true;
    }
    output = r;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
