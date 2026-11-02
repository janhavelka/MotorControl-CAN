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
uint8_t commandCount(const ActionContext& c) {
    return c.actionKind == ActionKind::STOP ? 1 : c.actionKind == ActionKind::CLEAR_ALARM ? 2 : 3;
}
bool effecting(const ActionContext& c) {
    return c.commandCursor == (c.actionKind == ActionKind::SET_DEVICE_POSITION ? 1 : 0);
}
ReadTarget expectedTarget(const ActionContext& c) {
    ReadTarget target = c.reservation.target;
    if (c.coordinatesInvalidated)
        ++target.channel.binding.configurationGeneration;
    return target;
}
uint8_t observationMask(const ActionContext& c) {
    uint8_t mask = uint8_t((1u << static_cast<unsigned>(StateBlock::MONITORS)) |
                           (1u << static_cast<unsigned>(StateBlock::STATUS)));
    if (c.actionKind == ActionKind::CLEAR_ALARM)
        mask |= uint8_t(1u << static_cast<unsigned>(StateBlock::ERRORS));
    if (c.actionKind == ActionKind::SET_DEVICE_POSITION)
        mask |= uint8_t(1u << static_cast<unsigned>(StateBlock::FEEDBACK));
    return mask;
}
Status shape(const ActionContext& c, uint64_t now) {
    if (!valid(c.reservation) || c.kind > StopKind::QUICK_STOP ||
        c.actionKind > ActionKind::SET_DEVICE_POSITION || !c.operation || !c.step ||
        c.state == ActionState::EMPTY || c.state > ActionState::FAILED ||
        c.phase > ActionPhase::WAITING || c.execution > ActionExecution::UNKNOWN ||
        c.completion > ActionCompletion::OBSERVED ||
        c.outcome > ActionOutcome::ITERATION_EXHAUSTED || !c.options.service.queueTimeoutUs ||
        !c.options.service.responseTimeoutUs || !c.options.observationIntervalUs ||
        !c.options.maximumAgeUs || c.deadlineUs <= c.startedUs || c.servicedUs < c.startedUs ||
        now < c.servicedUs || c.commandCursor >= commandCount(c) ||
        (c.commandEvidenceMask & ~uint8_t((1u << commandCount(c)) - 1u)) ||
        (c.phase == ActionPhase::COMMAND && c.step != uint32_t(c.commandCursor) + 1) ||
        (c.actionKind == ActionKind::SET_DEVICE_POSITION &&
         (!c.originalReferenceGeneration || c.originalReferenceGeneration == UINT32_MAX ||
          c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX ||
          (c.commandCursor == 2 && !c.coordinatesInvalidated))) ||
        (c.actionKind != ActionKind::SET_DEVICE_POSITION && c.coordinatesInvalidated))
        return error(ActionError::STATE, "invalid action context or time");
    if (c.phase != ActionPhase::COMMAND &&
        (!c.commandEvidencePresent || c.execution != ActionExecution::ACKNOWLEDGED ||
         c.commandEvidenceMask != uint8_t((1u << commandCount(c)) - 1u) ||
         c.commandCursor + 1 != commandCount(c) || c.step <= commandCount(c) ||
         !sameTarget(c.read.target, expectedTarget(c)) || c.read.operation != c.operation ||
         c.read.kind != ReadKind::STATE || c.read.source != ReadSource::SDO ||
         c.read.count != (c.actionKind == ActionKind::STOP ? 5 : 7) ||
         c.read.blockMask != observationMask(c) || c.read.deadlineUs != c.deadlineUs ||
         c.read.options.queueTimeoutUs != c.options.service.queueTimeoutUs ||
         c.read.options.responseTimeoutUs != c.options.service.responseTimeoutUs))
        return error(ActionError::STATE, "observation plan differs from admitted action");
    return Ok();
}
Status current(const ActionContext& c, const SettingsReservation& live, const ReadTarget& target,
               uint64_t now) {
    Status s = shape(c, now);
    if (!s)
        return s;
    if (!sameReservation(c.reservation, live) || !sameTarget(expectedTarget(c), target))
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
    t.step = c.step;
    t.index = c.actionKind == ActionKind::STOP          ? 0x6040
              : c.actionKind == ActionKind::CLEAR_ALARM ? 0x2301
                                                        : 0x2302;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    t.size = 2;
    if (c.actionKind == ActionKind::STOP) {
        t.data[0] = c.kind == StopKind::HALT ? 0x0f : 0x0b;
        t.data[1] = c.kind == StopKind::HALT ? 1 : 0;
    } else
        t.data[0] = c.actionKind == ActionKind::CLEAR_ALARM ? uint8_t(c.commandCursor == 0)
                                                            : uint8_t(c.commandCursor == 1);
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
    return prepareState(c.read, expectedTarget(c), c.operation, at, c.deadlineUs, c.options.service,
                        observationMask(c));
}
Status finishCycle(ActionContext& c, uint64_t now, bool anotherStepAvailable) {
    StateObservation status, monitors, extra;
    const ReadTarget target = expectedTarget(c);
    const uint64_t lastCommandUs = c.commandHistory[commandCount(c) - 1].completedUs;
    if (!getStateBlock(c.read, StateBlock::STATUS, target, now, c.options.maximumAgeUs, status) ||
        !getStateBlock(c.read, StateBlock::MONITORS, target, now, c.options.maximumAgeUs,
                       monitors) ||
        !after(status, lastCommandUs) || !after(monitors, lastCommandUs) ||
        (c.actionKind != ActionKind::STOP &&
         (!getStateBlock(c.read,
                         c.actionKind == ActionKind::CLEAR_ALARM ? StateBlock::ERRORS
                                                                 : StateBlock::FEEDBACK,
                         target, now, c.options.maximumAgeUs, extra) ||
          !after(extra, lastCommandUs)))) {
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "action observation freshness unavailable");
        return Ok();
    }
    c.statusObservation = status;
    c.monitorObservation = monitors;
    if (c.actionKind == ActionKind::CLEAR_ALARM)
        c.errorObservation = extra;
    if (c.actionKind == ActionKind::SET_DEVICE_POSITION)
        c.feedbackObservation = extra;
    c.observationKnown = true;
    ++c.observations; // Distinct external identities per cycle bound this below UINT32_MAX.
    const bool scope =
        status.modeRaw != 1 ||
        (c.actionKind == ActionKind::STOP
             ? (monitors.monitor & 0x18u) != 0
             : (monitors.monitor & 4u) != 0 ||
                   (c.actionKind == ActionKind::SET_DEVICE_POSITION && (monitors.monitor & 8u)));
    const bool done = c.actionKind == ActionKind::STOP          ? !(monitors.monitor & 4u)
                      : c.actionKind == ActionKind::CLEAR_ALARM ? !(monitors.monitor & 8u)
                                                                : extra.actualPosition == 0;
    if (scope)
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now,
             "drive report outside this action's supported observation scope");
    else if (done) {
        c.state = ActionState::SUCCEEDED;
        c.outcome = ActionOutcome::OBSERVED;
        c.completion = ActionCompletion::OBSERVED;
        c.status = Ok();
    } else if (!anotherStepAvailable) {
        fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "action service identity exhausted");
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
Status prepareExplicit(ActionContext& c, const SettingsReservation& r, const ReadContext& state,
                       ActionKind kind, uint32_t operation, uint64_t now, uint64_t deadline,
                       const ActionOptions& options) {
    if (!valid(r) || !operation || deadline <= now || r.acquiredUs > now ||
        !options.service.queueTimeoutUs || !options.service.responseTimeoutUs ||
        !options.observationIntervalUs || !options.maximumAgeUs)
        return error(ActionError::ARGUMENT, "invalid explicit action reservation or budgets");
    StateObservation status, monitors, errors, history;
    if (state.source != ReadSource::SDO || state.state != ReadState::SUCCEEDED || state.uncertain ||
        state.startedUs < r.acquiredUs ||
        !getStateBlock(state, StateBlock::STATUS, r.target, now, options.maximumAgeUs, status) ||
        !getStateBlock(state, StateBlock::MONITORS, r.target, now, options.maximumAgeUs,
                       monitors) ||
        !after(status, r.acquiredUs) || !after(monitors, r.acquiredUs) || status.modeRaw != 1 ||
        (monitors.monitor & 4u))
        return error(ActionError::STATE, "fresh mode 1 not-running SDO evidence required");
    if (kind == ActionKind::CLEAR_ALARM) {
        if (!(monitors.monitor & 8u) ||
            !getStateBlock(state, StateBlock::ERRORS, r.target, now, options.maximumAgeUs,
                           errors) ||
            !getStateBlock(state, StateBlock::ERROR_HISTORY, r.target, now, options.maximumAgeUs,
                           history) ||
            !after(errors, r.acquiredUs) || !after(history, r.acquiredUs) ||
            errors.errorCode != 0xff05)
            return error(ActionError::STATE,
                         "retained history and documented resettable FF05 alarm required");
    } else if (monitors.monitor & 8u)
        return error(ActionError::STATE, "counter clear requires no reported alarm");
    c.reservation = r;
    c.actionKind = kind;
    c.options = options;
    c.operation = operation;
    c.startedUs = c.servicedUs = c.eligibleUs = c.stepStartedUs = now;
    c.deadlineUs = deadline;
    c.state = ActionState::ACTIVE;
    c.originalState = state;
    c.statusObservation = status;
    c.monitorObservation = monitors;
    c.errorObservation = errors;
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

Status prepareClearAlarm(ActionContext& output, const SettingsReservation& r,
                         const ReadContext& state, uint32_t operation, uint64_t now,
                         uint64_t deadline, const ActionOptions& options) noexcept {
    if (overlap(&output, sizeof output, &r, sizeof r) ||
        overlap(&output, sizeof output, &state, sizeof state) ||
        overlap(&output, sizeof output, &options, sizeof options))
        return error(ActionError::OVERLAP, "alarm preparation overlaps input");
    ActionContext c;
    const Status s =
        prepareExplicit(c, r, state, ActionKind::CLEAR_ALARM, operation, now, deadline, options);
    if (!s)
        return s;
    output = c;
    return Ok();
}

Status prepareSetDevicePosition(ActionContext& output, const SettingsReservation& r,
                                const ReadContext& state, int64_t nativePosition,
                                const AxisConfig& axis, uint32_t operation, uint64_t now,
                                uint64_t deadline, const ActionOptions& options) noexcept {
    if (overlap(&output, sizeof output, &r, sizeof r) ||
        overlap(&output, sizeof output, &state, sizeof state) ||
        overlap(&output, sizeof output, &axis, sizeof axis) ||
        overlap(&output, sizeof output, &options, sizeof options))
        return error(ActionError::OVERLAP, "counter preparation overlaps input");
    if (nativePosition)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ActionError::ARGUMENT),
                      "documented device position action supports zero only");
    if (!validateAxisConfig(axis) || !sameBinding(axis.binding, r.target.channel.binding) ||
        axis.exhausted || !axis.referenceGeneration || axis.referenceGeneration == UINT32_MAX ||
        axis.binding.configurationGeneration == UINT32_MAX)
        return error(ActionError::TARGET, "counter clear needs matching nonexhausted axis epochs");
    ActionContext c;
    const Status s = prepareExplicit(c, r, state, ActionKind::SET_DEVICE_POSITION, operation, now,
                                     deadline, options);
    if (!s)
        return s;
    c.originalReferenceGeneration = axis.referenceGeneration;
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
    if (c.actionKind == ActionKind::SET_DEVICE_POSITION)
        return error(ActionError::STATE, "counter action requires axis-aware transmission notice");
    if (c.state != ActionState::ACTIVE || c.phase != ActionPhase::COMMAND || c.transmissionNoted ||
        now >= c.deadlineUs || now < c.eligibleUs)
        return error(ActionError::STATE, "no new live action download");
    c.transmissionNoted = c.effectsPossible = c.uncertain = true;
    if (effecting(c))
        c.execution = ActionExecution::UNKNOWN;
    c.servicedUs = now;
    return Ok();
}

Status noteActionTransmission(ActionContext& c, const SettingsReservation& live, ReadTarget& target,
                              AxisConfig& axis, AxisReference& reference, uint64_t now) noexcept {
    const void* objects[] = {&c, &target, &axis, &reference};
    const size_t sizes[] = {sizeof c, sizeof target, sizeof axis, sizeof reference};
    for (unsigned i = 0; i < 4; ++i) {
        if (overlap(objects[i], sizes[i], &live, sizeof live))
            return error(ActionError::OVERLAP, "counter notice overlaps reservation");
        for (unsigned j = 0; j < i; ++j)
            if (overlap(objects[i], sizes[i], objects[j], sizes[j]))
                return error(ActionError::OVERLAP, "counter notice outputs overlap");
    }
    Status s = current(c, live, target, now);
    if (!s)
        return s;
    if (c.actionKind != ActionKind::SET_DEVICE_POSITION || c.state != ActionState::ACTIVE ||
        c.phase != ActionPhase::COMMAND || c.transmissionNoted || now >= c.deadlineUs ||
        now < c.eligibleUs || !validateAxisConfig(axis) || axis.exhausted ||
        !sameBinding(axis.binding, target.channel.binding) ||
        axis.referenceGeneration !=
            c.originalReferenceGeneration + (c.coordinatesInvalidated ? 1u : 0u) ||
        (reference.nativeKnown && (!sameBinding(reference.binding, axis.binding) ||
                                   reference.referenceGeneration != axis.referenceGeneration)) ||
        (c.coordinatesInvalidated && (axis.originKnown || axis.encoderOriginKnown ||
                                      axis.softLimitsKnown || reference.nativeKnown)))
        return error(ActionError::STATE, "counter notice requires frozen axis and fresh command");
    ActionContext candidate = c;
    AxisConfig nextAxis = axis;
    AxisReference nextReference = reference;
    ReadTarget nextTarget = target;
    if (c.commandCursor == 1 && !c.coordinatesInvalidated) {
        s = invalidateAxisReference(nextAxis, nextReference, ReferenceLoss::COORDINATES);
        if (!s)
            return s; // Preparation reserved room; no partial public invalidation on rejection.
        nextTarget.channel.binding = nextAxis.binding;
        candidate.coordinatesInvalidated = true;
    }
    candidate.transmissionNoted = candidate.effectsPossible = candidate.uncertain = true;
    if (effecting(c))
        candidate.execution = ActionExecution::UNKNOWN;
    candidate.servicedUs = now;
    axis = nextAxis;
    reference = nextReference;
    target = nextTarget;
    c = candidate;
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
            return error(ActionError::CORRELATION, "inconsistent action command terminal");
        candidate.commandHistory[c.commandCursor] = input;
        candidate.commandEvidenceMask |= uint8_t(1u << c.commandCursor);
        if (effecting(c)) {
            candidate.commandEvidence = input;
            candidate.commandEvidencePresent = true;
            candidate.execution = success ? ActionExecution::ACKNOWLEDGED
                                  : input.outcome == CANOPEN::SdoOutcome::SERVER_ABORT
                                      ? ActionExecution::REJECTED
                                  : input.transmissionPossible ? ActionExecution::UNKNOWN
                                                               : ActionExecution::NOT_TRANSMITTED;
        }
        // Each settled terminal refines its own possible-TX notice. Earlier
        // prefix effects and a coordinate withdrawal are never rolled back.
        candidate.effectsPossible = input.effectPossible;
        candidate.uncertain = input.effectUncertain;
        for (unsigned i = 0; i < c.commandCursor; ++i) {
            candidate.effectsPossible |= c.commandHistory[i].effectPossible;
            candidate.uncertain |= c.commandHistory[i].effectUncertain;
        }
        if (now >= c.deadlineUs || input.completedUs >= c.deadlineUs)
            fail(candidate, ActionOutcome::DEADLINE, now, "action overall deadline");
        else if (!success)
            fail(candidate, outcome(input.outcome), now, "action command service failed");
        else if (c.commandCursor + 1 < commandCount(c)) {
            ++candidate.commandCursor;
            ++candidate.step;
            candidate.transmissionNoted = false;
            candidate.eligibleUs = candidate.stepStartedUs = now;
        } else {
            candidate.phase = ActionPhase::OBSERVING;
            ++candidate.step;
            candidate.transmissionNoted = false;
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
                 "action observation service failed");
        else if (c.step == UINT32_MAX) {
            // The last representable identity may still close an observed stop;
            // only work requiring another distinct identity is exhausted.
            if (candidate.read.state == ReadState::SUCCEEDED) {
                s = finishCycle(candidate, now, false);
                if (!s)
                    return s;
            } else
                fail(candidate, ActionOutcome::ITERATION_EXHAUSTED, now,
                     "action service identity exhausted");
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
    fail(candidate, reason, now, "action locally withdrawn; no replay or physical stop proof");
    c = candidate;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
