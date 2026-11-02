// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
Status error(ActionError reason, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool sameTarget(const ReadTarget& a, const ReadTarget& b) {
    return sameBinding(a.channel.binding, b.channel.binding) &&
           a.nodeGeneration == b.nodeGeneration &&
           a.channel.clientToServerId == b.channel.clientToServerId &&
           a.channel.serverToClientId == b.channel.serverToClientId;
}
bool valid(const SettingsReservation& r) {
    return r.owner && r.sequence && r.generation && r.target.nodeGeneration &&
           (r.model == SettingsModel::CL86_C || r.model == SettingsModel::OL86_C) &&
           CANOPEN::validateSdoChannel(r.target.channel);
}
bool sameReservation(const SettingsReservation& a, const SettingsReservation& b) {
    return valid(a) && valid(b) && sameTarget(a.target, b.target) && a.model == b.model &&
           a.owner == b.owner && a.sequence == b.sequence && a.generation == b.generation &&
           a.acquiredUs == b.acquiredUs;
}
Status shape(const ActionContext& c, uint64_t now) {
    if (!valid(c.reservation) || c.kind > StopKind::QUICK_STOP || !c.operation || !c.step ||
        c.state == ActionState::EMPTY || c.state > ActionState::FAILED ||
        c.phase > ActionPhase::WAITING || c.execution > ActionExecution::UNKNOWN ||
        c.completion > ActionCompletion::OBSERVED ||
        c.outcome > ActionOutcome::ITERATION_EXHAUSTED || !c.options.service.queueTimeoutUs ||
        !c.options.service.responseTimeoutUs || !c.options.observationIntervalUs ||
        !c.options.maximumAgeUs || c.deadlineUs <= c.startedUs || c.servicedUs < c.startedUs ||
        now < c.servicedUs || (c.phase == ActionPhase::COMMAND && c.step != 1))
        return error(ActionError::STATE, "invalid action context or time");
    if (c.phase != ActionPhase::COMMAND &&
        (!c.commandEvidencePresent || c.execution != ActionExecution::ACKNOWLEDGED || c.step < 2 ||
         !sameTarget(c.read.target, c.reservation.target) || c.read.operation != c.operation ||
         c.read.kind != ReadKind::STATE || c.read.source != ReadSource::SDO || c.read.count != 5 ||
         c.read.blockMask != uint8_t((1u << static_cast<unsigned>(StateBlock::MONITORS)) |
                                     (1u << static_cast<unsigned>(StateBlock::STATUS))) ||
         c.read.deadlineUs != c.deadlineUs ||
         c.read.options.queueTimeoutUs != c.options.service.queueTimeoutUs ||
         c.read.options.responseTimeoutUs != c.options.service.responseTimeoutUs))
        return error(ActionError::STATE, "stop observation plan differs from admitted action");
    return Ok();
}
Status current(const ActionContext& c, const SettingsReservation& live, const ReadTarget& target,
               uint64_t now) {
    Status s = shape(c, now);
    if (!s)
        return s;
    if (!sameReservation(c.reservation, live) || !sameTarget(c.reservation.target, target))
        return error(ActionError::TARGET, "action reservation or target changed");
    return Ok();
}
bool commonOverlap(const ActionContext& c, const SettingsReservation& live,
                   const ReadTarget& target) {
    return overlap(&c, sizeof c, &live, sizeof live) ||
           overlap(&c, sizeof c, &target, sizeof target);
}
bool after(const StateObservation& o, uint64_t at) {
    for (unsigned i = 0; i < o.fieldCount; ++i)
        if (o.fields[i].earliestUs < at)
            return false;
    return true;
}
bool controlEvidence(const CANOPEN::SdoCompletion& e, const SettingsReservation& r, uint64_t now,
                     uint64_t age) {
    const CANOPEN::SdoTransfer& t = e.transfer;
    return sameBinding(t.channel.binding, r.target.channel.binding) &&
           t.channel.clientToServerId == r.target.channel.clientToServerId &&
           t.channel.serverToClientId == r.target.channel.serverToClientId &&
           e.nodeGeneration == r.target.nodeGeneration &&
           e.outcome == CANOPEN::SdoOutcome::SUCCESS &&
           t.direction == CANOPEN::SdoDirection::DOWNLOAD && t.index == 0x6040 && !t.subindex &&
           t.size == 2 && t.data[0] == 0x0f && !t.data[1] && t.operation && t.request && t.step &&
           t.deadlineUs > e.completedUs && t.queueTimeoutUs && t.responseTimeoutUs && e.queued &&
           e.transmissionPossible && e.protocolAcknowledged && e.effectPossible &&
           !e.effectUncertain && !e.abortValid && !e.size && r.acquiredUs <= e.admittedUs &&
           e.admittedUs <= e.completedUs && e.completedUs <= now && now - e.admittedUs <= age;
}
CANOPEN::SdoTransfer command(const ActionContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.step = 1;
    t.index = 0x6040;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    t.size = 2;
    t.data[0] = c.kind == StopKind::HALT ? 0x0f : 0x0b;
    t.data[1] = c.kind == StopKind::HALT ? 1 : 0;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.service.queueTimeoutUs;
    t.responseTimeoutUs = c.options.service.responseTimeoutUs;
    return t;
}
bool sameWork(const CANOPEN::SdoTransfer& a, const CANOPEN::SdoTransfer& b) {
    if (!sameBinding(a.channel.binding, b.channel.binding) ||
        a.channel.clientToServerId != b.channel.clientToServerId ||
        a.channel.serverToClientId != b.channel.serverToClientId || a.operation != b.operation ||
        a.request != b.request || a.step != b.step || a.index != b.index ||
        a.subindex != b.subindex || a.direction != b.direction || a.size != b.size ||
        a.readEffect != b.readEffect || a.allowSegmentedUpload != b.allowSegmentedUpload ||
        a.deadlineUs != b.deadlineUs || a.queueTimeoutUs != b.queueTimeoutUs ||
        a.responseTimeoutUs != b.responseTimeoutUs)
        return false;
    if (a.direction == CANOPEN::SdoDirection::DOWNLOAD)
        for (unsigned i = 0; i < a.size; ++i)
            if (a.data[i] != b.data[i])
                return false;
    return true;
}
ActionOutcome outcome(CANOPEN::SdoOutcome o) {
    switch (o) {
    case CANOPEN::SdoOutcome::SERVER_ABORT:
        return ActionOutcome::PROTOCOL_REJECTED;
    case CANOPEN::SdoOutcome::TIMEOUT:
        return ActionOutcome::DEADLINE;
    case CANOPEN::SdoOutcome::CANCELLED:
        return ActionOutcome::CANCELLED;
    case CANOPEN::SdoOutcome::REBOOT:
    case CANOPEN::SdoOutcome::BINDING_INVALIDATED:
        return ActionOutcome::INVALIDATED;
    case CANOPEN::SdoOutcome::CLIENT_ABORT:
    case CANOPEN::SdoOutcome::UNSUPPORTED:
        return ActionOutcome::REPLY_ERROR;
    default:
        return ActionOutcome::TRANSPORT_ERROR;
    }
}
ActionOutcome outcome(ReadOutcome o) {
    switch (o) {
    case ReadOutcome::PROTOCOL_REJECTED:
        return ActionOutcome::PROTOCOL_REJECTED;
    case ReadOutcome::REPLY_ERROR:
        return ActionOutcome::REPLY_ERROR;
    case ReadOutcome::CANCELLED:
        return ActionOutcome::CANCELLED;
    case ReadOutcome::DEADLINE:
        return ActionOutcome::DEADLINE;
    case ReadOutcome::INVALIDATED:
        return ActionOutcome::INVALIDATED;
    default:
        return ActionOutcome::TRANSPORT_ERROR;
    }
}
void fail(ActionContext& c, ActionOutcome reason, uint64_t now, const char* message) {
    c.state = ActionState::FAILED;
    c.outcome = reason;
    c.status =
        reason == ActionOutcome::PROTOCOL_REJECTED
            ? Status(Err::PROTOCOL_ERROR, 0, message)
            : error(reason == ActionOutcome::DEADLINE ? ActionError::TIME : ActionError::STATE,
                    message);
    c.servicedUs = now;
    c.uncertain |= c.effectsPossible && c.completion != ActionCompletion::OBSERVED;
}
Status prepareCycle(ActionContext& c, uint64_t at) {
    const uint8_t mask = uint8_t((1u << static_cast<unsigned>(StateBlock::MONITORS)) |
                                 (1u << static_cast<unsigned>(StateBlock::STATUS)));
    return prepareState(c.read, c.reservation.target, c.operation, at, c.deadlineUs,
                        c.options.service, mask);
}
Status finishCycle(ActionContext& c, uint64_t now, bool anotherStepAvailable) {
    StateObservation status, monitors;
    if (!getStateBlock(c.read, StateBlock::STATUS, c.reservation.target, now,
                       c.options.maximumAgeUs, status) ||
        !getStateBlock(c.read, StateBlock::MONITORS, c.reservation.target, now,
                       c.options.maximumAgeUs, monitors) ||
        !after(status, c.commandEvidence.completedUs) ||
        !after(monitors, c.commandEvidence.completedUs)) {
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "stop observation freshness unavailable");
        return Ok();
    }
    c.statusObservation = status;
    c.monitorObservation = monitors;
    c.observationKnown = true;
    ++c.observations; // Five unique external identities per cycle bound this below UINT32_MAX.
    if (status.modeRaw != 1 || (monitors.monitor & 0x18u))
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now,
             "stop mode, alarm or release report outside supported observation scope");
    else if (!(monitors.monitor & 4u)) {
        c.state = ActionState::SUCCEEDED;
        c.outcome = ActionOutcome::OBSERVED;
        c.completion = ActionCompletion::OBSERVED;
        c.status = Ok();
    } else if (!anotherStepAvailable) {
        fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "stop service identity exhausted");
    } else {
        const uint64_t remaining = c.deadlineUs - now;
        c.eligibleUs = c.options.observationIntervalUs >= remaining
                           ? c.deadlineUs
                           : now + c.options.observationIntervalUs;
        c.phase = ActionPhase::WAITING;
        c.stepStartedUs = c.eligibleUs;
        if (c.eligibleUs < c.deadlineUs)
            return prepareCycle(c, c.eligibleUs);
    }
    return Ok();
}
} // namespace

Status prepareStop(ActionContext& output, const SettingsReservation& r, const ReadContext& state,
                   const CANOPEN::SdoCompletion& control, StopKind kind, uint32_t operation,
                   uint64_t now, uint64_t deadline, const ActionOptions& options) noexcept {
    if (overlap(&output, sizeof output, &r, sizeof r) ||
        overlap(&output, sizeof output, &state, sizeof state) ||
        overlap(&output, sizeof output, &control, sizeof control) ||
        overlap(&output, sizeof output, &options, sizeof options))
        return error(ActionError::OVERLAP, "stop preparation overlaps input");
    if (!valid(r) || kind > StopKind::QUICK_STOP || !operation || deadline <= now ||
        !options.service.queueTimeoutUs || !options.service.responseTimeoutUs ||
        !options.observationIntervalUs || !options.maximumAgeUs || r.acquiredUs > now)
        return error(ActionError::ARGUMENT,
                     "invalid stop reservation, selection or caller budgets");
    StateObservation status, monitors;
    if (!controlEvidence(control, r, now, options.maximumAgeUs) ||
        state.source != ReadSource::SDO || state.state != ReadState::SUCCEEDED || state.uncertain ||
        !getStateBlock(state, StateBlock::STATUS, r.target, now, options.maximumAgeUs, status) ||
        !getStateBlock(state, StateBlock::MONITORS, r.target, now, options.maximumAgeUs,
                       monitors) ||
        status.modeRaw != 1 || (status.statusword & 7u) != 7 || (monitors.monitor & 0x18u) ||
        !after(status, control.completedUs) || !after(monitors, control.completedUs))
        return error(ActionError::STATE,
                     "continuous acknowledged 000F and fresh mode 1 state required");
    ActionContext c;
    c.reservation = r;
    c.kind = kind;
    c.options = options;
    c.operation = operation;
    c.startedUs = c.servicedUs = c.eligibleUs = c.stepStartedUs = now;
    c.deadlineUs = deadline;
    c.state = ActionState::ACTIVE;
    c.controlEvidence = control;
    c.statusObservation = status;
    c.monitorObservation = monitors;
    output = c;
    return Ok();
}

Status nextAction(const ActionContext& c, const SettingsReservation& live, const ReadTarget& target,
                  uint64_t now, PreparedAction& output) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &live, sizeof live) ||
        overlap(&output, sizeof output, &target, sizeof target))
        return error(ActionError::OVERLAP, "action work overlaps input");
    const Status s = current(c, live, target, now);
    if (!s)
        return s;
    if (c.state == ActionState::ACTIVE && now >= c.deadlineUs)
        return error(ActionError::TIME, "action overall deadline");
    PreparedAction next;
    next.eligibleUs = c.eligibleUs;
    if (c.state != ActionState::ACTIVE || now < c.eligibleUs) {
        output = next;
        return Ok();
    }
    next.ready = true;
    if (c.phase == ActionPhase::COMMAND)
        next.transfer = command(c);
    else {
        PreparedRead read;
        const Status readStatus = nextRead(c.read, now, read);
        if (!readStatus)
            return readStatus;
        if (!read.ready)
            return error(ActionError::STATE, "action observation has no pending field");
        next.transfer = read.transfer;
        next.transfer.step = c.step;
    }
    output = next;
    return Ok();
}

Status noteActionTransmission(ActionContext& c, const SettingsReservation& live,
                              const ReadTarget& target, uint64_t now) noexcept {
    if (commonOverlap(c, live, target))
        return error(ActionError::OVERLAP, "action TX notice overlaps input");
    const Status s = current(c, live, target, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.phase != ActionPhase::COMMAND || c.transmissionNoted ||
        now >= c.deadlineUs || now < c.eligibleUs)
        return error(ActionError::STATE, "no new live stop download");
    c.transmissionNoted = c.effectsPossible = c.uncertain = true;
    c.execution = ActionExecution::UNKNOWN;
    c.servicedUs = now;
    return Ok();
}

Status advanceAction(ActionContext& c, const SettingsReservation& live, const ReadTarget& target,
                     const CANOPEN::SdoCompletion& input, uint64_t now) noexcept {
    if (commonOverlap(c, live, target) || overlap(&c, sizeof c, &input, sizeof input))
        return error(ActionError::OVERLAP, "action terminal overlaps input");
    Status s = current(c, live, target, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE)
        return error(ActionError::STATE, "action already terminal");
    PreparedAction work;
    // As with ordinary reads, validate correlation before consuming a deadline.
    s = nextAction(c, live, target, c.servicedUs < c.eligibleUs ? c.eligibleUs : c.servicedUs,
                   work);
    if (!s)
        return s;
    if (!work.ready || !sameWork(input.transfer, work.transfer) ||
        input.nodeGeneration != c.reservation.target.nodeGeneration ||
        input.admittedUs < c.stepStartedUs || input.admittedUs >= c.deadlineUs ||
        input.completedUs < input.admittedUs || input.completedUs > now ||
        input.completedUs < c.servicedUs)
        return error(ActionError::CORRELATION, "action terminal does not match current service");
    ActionContext candidate = c;
    if (c.phase == ActionPhase::COMMAND) {
        const bool success = input.outcome == CANOPEN::SdoOutcome::SUCCESS;
        if (input.outcome > CANOPEN::SdoOutcome::UNSUPPORTED ||
            (input.transmissionPossible && (!input.queued || !c.transmissionNoted)) ||
            (input.transmissionConfirmed && !input.transmissionPossible) || input.size ||
            input.effectPossible != input.transmissionPossible ||
            input.effectUncertain != (input.transmissionPossible && !success) ||
            (success &&
             (!input.protocolAcknowledged || !input.transmissionPossible || input.abortValid)) ||
            (!success && input.protocolAcknowledged) ||
            (input.outcome == CANOPEN::SdoOutcome::SERVER_ABORT &&
             (!input.abortValid || !input.transmissionPossible)))
            return error(ActionError::CORRELATION, "inconsistent stop command terminal");
        candidate.commandEvidence = input;
        candidate.commandEvidencePresent = true;
        candidate.effectsPossible = input.effectPossible;
        candidate.uncertain = input.effectUncertain;
        candidate.execution = success ? ActionExecution::ACKNOWLEDGED
                              : input.outcome == CANOPEN::SdoOutcome::SERVER_ABORT
                                  ? ActionExecution::REJECTED
                              : input.transmissionPossible ? ActionExecution::UNKNOWN
                                                           : ActionExecution::NOT_TRANSMITTED;
        if (now >= c.deadlineUs || input.completedUs >= c.deadlineUs)
            fail(candidate, ActionOutcome::DEADLINE, now, "stop overall deadline");
        else if (!success)
            fail(candidate, outcome(input.outcome), now, "stop command service failed");
        else {
            candidate.phase = ActionPhase::OBSERVING;
            candidate.step = 2;
            candidate.eligibleUs = candidate.stepStartedUs = now;
            s = prepareCycle(candidate, now);
            if (!s)
                return s;
        }
    } else {
        CANOPEN::SdoCompletion local = input;
        local.transfer.step = uint32_t(candidate.read.cursor) + 1;
        s = advanceRead(candidate.read, local, now);
        if (!s)
            return s;
        candidate.read.evidence = input; // Preserve actual external identity in retained history.
        candidate.observationEvidence = input;
        candidate.observationEvidencePresent = true;
        candidate.uncertain |= candidate.read.uncertain;
        if (candidate.read.state == ReadState::FAILED)
            fail(candidate, outcome(candidate.read.outcome), now,
                 "stop observation service failed");
        else if (c.step == UINT32_MAX) {
            // The last representable identity may still close an observed stop;
            // only work requiring another distinct identity is exhausted.
            if (candidate.read.state == ReadState::SUCCEEDED) {
                s = finishCycle(candidate, now, false);
                if (!s)
                    return s;
            } else
                fail(candidate, ActionOutcome::ITERATION_EXHAUSTED, now,
                     "stop service identity exhausted");
        } else {
            ++candidate.step;
            candidate.stepStartedUs = now;
            candidate.phase = ActionPhase::OBSERVING;
            if (candidate.read.state == ReadState::SUCCEEDED) {
                s = finishCycle(candidate, now, true);
                if (!s)
                    return s;
            }
        }
    }
    candidate.servicedUs = now;
    c = candidate;
    return Ok();
}

Status advanceAction(ActionContext& c, const SettingsReservation& live, const ReadTarget& target,
                     ActionOutcome reason, uint64_t now) noexcept {
    if (commonOverlap(c, live, target))
        return error(ActionError::OVERLAP, "action withdrawal overlaps input");
    Status s = reason == ActionOutcome::INVALIDATED ? shape(c, now) : current(c, live, target, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE ||
        (reason != ActionOutcome::CANCELLED && reason != ActionOutcome::DEADLINE &&
         reason != ActionOutcome::INVALIDATED && reason != ActionOutcome::DRIVE_FAULT) ||
        (reason == ActionOutcome::DEADLINE && now < c.deadlineUs))
        return error(ActionError::STATE, "invalid local action termination");
    ActionContext candidate = c;
    if (c.phase != ActionPhase::COMMAND && c.read.state == ReadState::ACTIVE) {
        // During WAITING the read is prepared at a future eligible time and has
        // not been admitted; cancelling then must not fabricate read effects.
        if (now >= c.eligibleUs) {
            const ReadOutcome readReason =
                reason == ActionOutcome::INVALIDATED ? ReadOutcome::INVALIDATED
                : reason == ActionOutcome::DEADLINE  ? ReadOutcome::DEADLINE
                                                     : ReadOutcome::CANCELLED;
            s = advanceRead(candidate.read, readReason, now);
            if (!s)
                return s;
            candidate.uncertain |= candidate.read.uncertain;
        }
    }
    if (now >= c.deadlineUs)
        reason = ActionOutcome::DEADLINE;
    fail(candidate, reason, now, "stop locally withdrawn; no replay or physical stop proof");
    c = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
