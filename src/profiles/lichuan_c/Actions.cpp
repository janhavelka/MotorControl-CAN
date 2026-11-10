// SPDX-License-Identifier: MIT
#include "ReadPlan.h"
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <new>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
Status error(ActionError reason, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}
uint8_t commandCount(const ActionContext& c) {
    return c.actionKind == ActionKind::STOP ? 1 : c.actionKind == ActionKind::CLEAR_ALARM ? 2 : 3;
}
bool enabling(const ActionContext& c) {
    return c.actionKind == ActionKind::ENABLE;
}
bool effecting(const ActionContext& c) {
    return enabling(c) || c.commandCursor == (c.actionKind == ActionKind::SET_DEVICE_POSITION ? 1 : 0);
}
// The two actions that write a 1 and then return their object to 0.
bool twoValued(const ActionContext& c) {
    return c.actionKind == ActionKind::CLEAR_ALARM || c.actionKind == ActionKind::SET_DEVICE_POSITION;
}
// D-27 and Q14: may the object still hold the 1? True when the 1 may have reached the drive and
// the 0 after it has no positive acknowledgement. Read only once the action fails.
bool oneMayRemain(const ActionContext& c) {
    if (!twoValued(c))
        return false;
    const unsigned one = c.actionKind == ActionKind::SET_DEVICE_POSITION ? 1 : 0;
    const bool oneSettled = (c.commandEvidenceMask >> one) & 1u;
    const bool onePossible = oneSettled ? c.commandHistory[one].effectPossible
                                        : c.commandCursor == one && c.transmissionNoted;
    const bool zeroAcknowledged = ((c.commandEvidenceMask >> (one + 1)) & 1u) &&
                                  c.commandHistory[one + 1].outcome == CANOPEN::SdoOutcome::SUCCESS;
    return onePossible && !zeroAcknowledged;
}
// The documented word of each enable step and the low bits manuals show beside it.
const uint16_t ENABLE_WORDS[3] = {6, 7, 15};
const uint16_t ENABLE_LOW_BITS[3] = {1, 3, 7};
// 6041 bits 5 and 6, which word 6 moved from every released report so far: bits 0..6 went from 0x50 to
// 0x31 (trial 1782, mode 3: 8050 to 8031; trial 308, mode 1: 4050 to 4031). They are compared only after
// a power-up start, whose reports keep them (8037, 8031, 8033). No meaning is assigned to either bit.
constexpr uint16_t RELEASED_START_STATUS_BITS = 0x0060;
bool releasedStart(const EnableEvidence& e) { return e.start == EnableStart::RELEASED_BY_RECORDED_STOP; }
ReadTarget expectedTarget(const ActionContext& c) {
    ReadTarget target = c.reservation.target;
    if (c.coordinatesInvalidated)
        ++target.channel.binding.configurationGeneration;
    return target;
}
uint8_t observationMask(const ActionContext& c) {
    if (c.finalErrorRead)
        return uint8_t(1u << static_cast<unsigned>(StateBlock::ERRORS));
    uint8_t mask = uint8_t((1u << static_cast<unsigned>(StateBlock::MONITORS)) |
                           (1u << static_cast<unsigned>(StateBlock::STATUS)));
    if (c.actionKind == ActionKind::SET_DEVICE_POSITION || enabling(c) || c.standstillObserving ||
        (c.actionKind == ActionKind::STOP && c.stopModeRaw == 6))
        mask |= uint8_t(1u << static_cast<unsigned>(StateBlock::FEEDBACK));
    return mask;
}
Status shape(const ActionContext& c, uint64_t now) {
    if (!validReservation(c.reservation) || c.kind > StopKind::QUICK_STOP ||
        c.actionKind > ActionKind::ENABLE || !c.operation || !c.step ||
        (c.actionKind == ActionKind::STOP && c.stopModeRaw != 1 && c.stopModeRaw != 3 &&
         c.stopModeRaw != 6) ||
        c.state == ActionState::EMPTY || c.state > ActionState::FAILED ||
        c.phase > ActionPhase::TARGETS || c.execution > ActionExecution::UNKNOWN ||
        c.completion > ActionCompletion::OBSERVED ||
        c.outcome == ActionOutcome::STOP_SENT_UNCONFIRMED ||
        c.outcome > ActionOutcome::SERVICE_TIMEOUT ||
        (c.withdrawal != ActionOutcome::NONE && c.withdrawal != ActionOutcome::CANCELLED &&
         c.withdrawal != ActionOutcome::DEADLINE && c.withdrawal != ActionOutcome::INVALIDATED &&
         c.withdrawal != ActionOutcome::DRIVE_FAULT) ||
        !c.options.service.queueTimeoutUs || !c.options.service.responseTimeoutUs ||
        !c.options.observationIntervalUs || !c.options.maximumAgeUs ||
        c.deadlineUs <= c.startedUs || c.servicedUs < c.startedUs || now < c.servicedUs ||
        c.commandCursor >= commandCount(c) ||
        (c.commandEvidenceMask & ~uint8_t((1u << commandCount(c)) - 1u)) ||
        (c.phase == ActionPhase::COMMAND && !enabling(c) &&
         c.step != uint32_t(c.commandCursor) + 1) ||
        (c.phase == ActionPhase::TARGETS &&
         (!enabling(c) || c.commandCursor || c.commandEvidenceMask || c.effectsPossible ||
          c.execution != ActionExecution::NOT_TRANSMITTED || c.enable.targetCursor > 1)) ||
        (c.actionKind == ActionKind::SET_DEVICE_POSITION &&
         (!c.originalReferenceGeneration || c.originalReferenceGeneration == UINT32_MAX ||
          c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX ||
          (c.commandCursor == 2 && !c.coordinatesInvalidated))) ||
        (c.actionKind != ActionKind::SET_DEVICE_POSITION && c.coordinatesInvalidated) ||
        (c.finalErrorRead && c.actionKind != ActionKind::CLEAR_ALARM) ||
        (c.standstillObserving &&
         (c.actionKind != ActionKind::STOP || c.kind != StopKind::QUICK_STOP ||
          (c.stopModeRaw != 1 &&
           (c.stopModeRaw != 3 || (c.predecessorWord != 0x000f && c.predecessorWord != 0x010f))))) ||
        c.valueOneMayRemain != (c.state == ActionState::FAILED && oneMayRemain(c)))
        return error(ActionError::STATE, "invalid action context or time");
    if (c.phase != ActionPhase::COMMAND && c.phase != ActionPhase::TARGETS &&
        (c.execution != ActionExecution::ACKNOWLEDGED ||
         c.commandEvidenceMask != uint8_t((1u << (c.commandCursor + 1u)) - 1u) ||
         (!enabling(c) && (c.commandCursor + 1 != commandCount(c) || c.step <= commandCount(c))) ||
         !sameTarget(c.read.target, expectedTarget(c)) || c.read.operation != c.operation ||
         c.read.kind != ReadKind::STATE || c.read.source != ReadSource::SDO ||
         c.read.count != (c.finalErrorRead ? 2
                          : c.actionKind == ActionKind::SET_DEVICE_POSITION || enabling(c) ||
                                  c.standstillObserving ||
                                  (c.actionKind == ActionKind::STOP && c.stopModeRaw == 6)
                              ? 7
                              : 5) ||
         c.read.blockMask != observationMask(c) || c.read.deadlineUs != c.deadlineUs ||
         c.read.options.queueTimeoutUs != c.options.service.queueTimeoutUs ||
         c.read.options.responseTimeoutUs != c.options.service.responseTimeoutUs))
        return error(ActionError::STATE, "observation plan differs from admitted action");
    return Ok();
}
Status current(const ActionContext& c, const AxisReservation& live, const ReadTarget& target,
               uint64_t now) {
    Status s = shape(c, now);
    if (!s)
        return s;
    if (!sameReservation(c.reservation, live) || !sameTarget(expectedTarget(c), target))
        return error(ActionError::TARGET, "action reservation or target changed");
    return Ok();
}
bool commonOverlap(const ActionContext& c, const AxisReservation& live,
                   const ReadTarget& target) {
    return overlaps(&c, sizeof c, &live, sizeof live) ||
           overlaps(&c, sizeof c, &target, sizeof target);
}
bool after(const StateObservation& o, uint64_t at) {
    for (unsigned i = 0; i < o.fieldCount; ++i)
        if (o.fields[i].earliestUs < at)
            return false;
    return true;
}
// Each term is at most 2^33 and the field count is single digit, so this cannot wrap.
// Actual cycle time is unknown here, so service gaps can still expire a cycle.
bool ageCoversCycle(const ActionOptions& o, unsigned fields) {
    return o.maximumAgeUs >=
           fields * (uint64_t(o.service.queueTimeoutUs) + o.service.responseTimeoutUs);
}
bool fresh(const StateObservation& o, uint64_t now, uint64_t age) {
    for (unsigned i = 0; i < o.fieldCount; ++i)
        if (o.fields[i].earliestUs > now || now - o.fields[i].earliestUs > age)
            return false;
    return o.fieldCount != 0;
}
// A stop must work on a running drive, and an acknowledged word no longer expires by age.
bool controlEvidence(const CANOPEN::SdoCompletion& e, const AxisReservation& r, StopKind kind,
                     uint64_t now) {
    return acknowledgedControlWrite(e, r, now) && stopPredecessorAllowed(controlWord(e.transfer), kind);
}
// 607A and 60FF are single uploads accepted only as this action's bounded prerequisite, never
// as a poll; their effects stay unknown. The separate typed 60FF read stays Q20-unqualified.
// A ledger change that withdraws acceptance for 607A stops this compiling. 60FF has no generated
// descriptor because its I32/U32 claims conflict, so the enable test pins its wire width.
static_assert(OBJECT_607A_00.readEffect == ReadEffect::ACCEPTED_UNQUALIFIED,
              "enable target reads need the ledger's accepted action prerequisite");
CANOPEN::SdoTransfer targetUpload(const ActionContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.step = c.step;
    t.index = c.enable.targetCursor ? 0x60ff : 0x607a;
    t.direction = CANOPEN::SdoDirection::UPLOAD;
    t.size = 4;
    t.readEffect = CANOPEN::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.service.queueTimeoutUs;
    t.responseTimeoutUs = c.options.service.responseTimeoutUs;
    return t;
}
CANOPEN::SdoTransfer command(const ActionContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.step = c.step;
    t.index = c.actionKind == ActionKind::STOP || enabling(c) ? 0x6040
              : c.actionKind == ActionKind::CLEAR_ALARM      ? 0x2301
                                                             : 0x2302;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    t.size = 2;
    if (c.actionKind == ActionKind::STOP) {
        t.data[0] = c.kind == StopKind::HALT ? 0x0f : 0x0b;
        t.data[1] = c.kind == StopKind::HALT && c.stopModeRaw != 6 ? 1 : 0;
    } else if (enabling(c))
        t.data[0] = uint8_t(ENABLE_WORDS[c.commandCursor]);
    else
        t.data[0] = c.actionKind == ActionKind::CLEAR_ALARM ? uint8_t(c.commandCursor == 0)
                                                            : uint8_t(c.commandCursor == 1);
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.service.queueTimeoutUs;
    t.responseTimeoutUs = c.options.service.responseTimeoutUs;
    return t;
}
ActionOutcome outcome(CANOPEN::SdoOutcome o) {
    switch (o) {
    case CANOPEN::SdoOutcome::SERVER_ABORT:
        return ActionOutcome::PROTOCOL_REJECTED;
    case CANOPEN::SdoOutcome::TIMEOUT:
        // Every caller settles the overall cutoff first, so this is a queue or response budget.
        return ActionOutcome::SERVICE_TIMEOUT;
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
    case ReadOutcome::SERVICE_TIMEOUT:
        return ActionOutcome::SERVICE_TIMEOUT;
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
            : error(reason == ActionOutcome::DEADLINE || reason == ActionOutcome::SERVICE_TIMEOUT
                        ? ActionError::TIME
                        : ActionError::STATE,
                    message);
    c.servicedUs = now;
    c.uncertain |= c.effectsPossible && c.completion != ActionCompletion::OBSERVED;
    c.valueOneMayRemain = oneMayRemain(c);
}
Status prepareCycle(ActionContext& c, uint64_t at) {
    return prepareState(c.read, expectedTarget(c), c.operation, at, c.deadlineUs, c.options.service,
                        observationMask(c));
}
// True when this block of the current cycle is current and began after `at`.
bool cycleBlock(const ActionContext& c, StateBlock block, const ReadTarget& target, uint64_t now,
                uint64_t at) {
    ReadWindow window;
    return validateStateBlock(c.read, block, target, now, c.options.maximumAgeUs, window).isOk() &&
           window.earliestUs >= at;
}
uint32_t raw(const ReadContext& c, StateBlock block, unsigned field) {
    return c.values[c.blockStart[static_cast<unsigned>(block)] + field].raw;
}
// prepareCycle was proved able to succeed before this cycle's read moved (advanceAction).
void nextCycle(ActionContext& c, uint64_t at, uint64_t now) {
    if (!prepareCycle(c, at))
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "action observation could not be prepared");
}
// Every bit but 2100 released and 6041 bits 0..2 stays as admitted, and raw feedback stays still.
// Disputed bits decide nothing here: a change only ends the sequence, with both words retained.
bool enableScopeHolds(const ActionContext& c) {
    // Bit 10 changes with the enable state itself on node 1 (STATUS_EN86_ZERO_SPEED), and 606C below
    // already requires zero velocity, so comparing it would end every enable at its first word.
    constexpr uint16_t lowBits = STATUS_LOW_BITS | STATUS_EN86_ZERO_SPEED;
    constexpr uint16_t released = MONITOR_RELEASED;
    const StateObservation& status = c.statusObservation;
    const StateObservation& monitors = c.monitorObservation;
    // From a release, word 7 energizes the drive and its position report then moves to the drive's
    // internal command: before the first cycle in mode 3 (trials 1742..1862), over about half a second in
    // mode 1 (trial 308). So only word 6's cycles, still released, must repeat the admission 6064. Each
    // step keeps what its cycle reported.
    const bool fromRelease = releasedStart(c.enable.evidence);
    const uint16_t ignored = fromRelease ? uint16_t(lowBits | RELEASED_START_STATUS_BITS) : lowBits;
    const bool positionHeld = !fromRelease || c.commandCursor == 0;
    return status.modeRaw == c.enable.evidence.modeRaw &&
           ((status.statusword ^ c.enable.startStatusword) & uint16_t(~ignored)) == 0 &&
           ((monitors.monitor ^ c.enable.startMonitor) & uint16_t(~released)) == 0 &&
           (!positionHeld || c.extraObservation.actualPosition == c.enable.startPosition) &&
           c.extraObservation.actualVelocity == 0;
}
// A word was observed; keep its evidence and schedule the next one. A step that reached the cutoff
// waits here, and the caller's next advance settles DEADLINE.
void acceptEnableStep(ActionContext& c, uint64_t now) {
    EnableStepRecord& step = c.enable.steps[c.commandCursor];
    step.present = true;
    step.statusword = c.statusObservation.statusword;
    step.monitor = c.monitorObservation.monitor;
    step.position = c.extraObservation.actualPosition;
    step.velocity = c.extraObservation.actualVelocity;
    step.earliestUs = c.monitorObservation.window.earliestUs < c.statusObservation.window.earliestUs
                          ? c.monitorObservation.window.earliestUs
                          : c.statusObservation.window.earliestUs;
    step.latestUs = c.extraObservation.window.latestUs;
    if (c.commandCursor + 1 >= commandCount(c)) {
        c.state = ActionState::SUCCEEDED;
        c.outcome = ActionOutcome::OBSERVED;
        c.completion = ActionCompletion::OBSERVED;
        c.status = Ok();
        return;
    }
    const uint64_t ack = c.commandHistory[c.commandCursor].completedUs;
    const uint64_t interval = c.options.commandIntervalUs;
    const uint64_t remaining = c.deadlineUs - ack;
    uint64_t eligible = interval >= remaining ? c.deadlineUs : ack + interval;
    if (eligible < now)
        eligible = now;
    ++c.commandCursor;
    c.phase = ActionPhase::COMMAND;
    c.transmissionNoted = false;
    c.eligibleUs = c.stepStartedUs = eligible;
}
void finishCycle(ActionContext& c, uint64_t now, bool anotherStepAvailable) {
    const ReadTarget target = expectedTarget(c);
    const uint64_t lastCommandUs = c.commandHistory[c.commandCursor].completedUs;
    const uint64_t age = c.options.maximumAgeUs;
    if (c.finalErrorRead) {
        if (!cycleBlock(c, StateBlock::ERRORS, target, now, lastCommandUs)) {
            fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "action observation freshness unavailable");
            return;
        }
        getStateBlock(c.read, StateBlock::ERRORS, target, now, age, c.extraObservation);
        c.state = ActionState::SUCCEEDED;
        c.outcome = ActionOutcome::OBSERVED;
        c.completion = ActionCompletion::OBSERVED;
        c.status = Ok();
        return;
    }
    const bool feedback = (c.read.blockMask & (1u << static_cast<unsigned>(StateBlock::FEEDBACK))) != 0;
    // A quick stop's standstill compares this cycle's FEEDBACK with the one before it.
    const bool previousFeedback = c.standstillObserving && c.extraObservation.fieldCount != 0;
    const int32_t previousPosition = c.extraObservation.actualPosition;
    const int32_t previousVelocity = c.extraObservation.actualVelocity;
    // In mode 3 both compared cycles must report the release; read before this cycle replaces it.
    const bool previousReleased = previousFeedback && (c.monitorObservation.monitor & MONITOR_RELEASED);
    if (!cycleBlock(c, StateBlock::STATUS, target, now, lastCommandUs) ||
        !cycleBlock(c, StateBlock::MONITORS, target, now, lastCommandUs) ||
        (feedback && !cycleBlock(c, StateBlock::FEEDBACK, target, now, lastCommandUs))) {
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "action observation freshness unavailable");
        return;
    }
    // All required blocks are good, so no earlier observation is torn from here on.
    getStateBlock(c.read, StateBlock::STATUS, target, now, age, c.statusObservation);
    getStateBlock(c.read, StateBlock::MONITORS, target, now, age, c.monitorObservation);
    if (feedback)
        getStateBlock(c.read, StateBlock::FEEDBACK, target, now, age, c.extraObservation);
    c.observationKnown = true;
    ++c.observations; // Distinct external identities per cycle bound this below UINT32_MAX.
    const uint16_t monitor = c.monitorObservation.monitor;
    const bool running = (monitor & MONITOR_RUNNING) != 0;
    const bool alarm = (monitor & MONITOR_ALARM) != 0;
    const uint8_t expectedMode = c.actionKind == ActionKind::STOP ? c.stopModeRaw
                                 : enabling(c)                    ? c.enable.evidence.modeRaw
                                                                  : 1;
    bool outsideScope = c.statusObservation.modeRaw != expectedMode;
    bool done = false;
    bool wantStandstill = false;
    switch (c.actionKind) {
    case ActionKind::STOP: {
        // The measured release in mode 1: on both specimens, 000B from 000F and from 010F left 2100
        // released with 6041 low bits 000 (2026-11-03). A HALT holds torque, so a release stays
        // outside its scope. The release itself stays in monitorObservation for the caller.
        const bool released = (monitor & MONITOR_RELEASED) != 0;
        const bool lowZero = (c.statusObservation.statusword & STATUS_LOW_BITS) == 0;
        // Mode 3 (backlog S18): a QUICK_STOP prepared with standstillObserving, which only one that
        // followed a running report gets. Measured on both specimens, one firmware: 28 ms after 000B
        // from a running run, 2100=0014 and 6041=C050 in mode 3 (trial 1513, node 2, 30 rpm; 1607,
        // node 1, 60 rpm), and later 2100=0015, 6041=C050, 606C 0 and 6064 still (reads 1514, 1608).
        const bool releaseScope = c.kind == StopKind::QUICK_STOP &&
                                  (c.stopModeRaw == 1 || (c.stopModeRaw == 3 && c.standstillObserving));
        outsideScope |= alarm || (released && (!releaseScope || !lowZero));
        // After 000B during a move the same release kept running set (standstillObserving). Two
        // cycles in a row must each read 606C zero, with 6064 unchanged between them.
        const bool latched = c.stopModeRaw == 1 && c.kind == StopKind::QUICK_STOP && running &&
                             released && lowZero;
        const bool still = feedback && previousFeedback && previousVelocity == 0 &&
                           c.extraObservation.actualVelocity == 0 &&
                           c.extraObservation.actualPosition == previousPosition;
        done = !running || (latched && still);
        // A mode 3 release never completes on one report, running set or clear: two released cycles in a
        // row must show that standstill. After 010F (prepareHaltedVelocityQuickStop) the drive reported
        // itself halted and idle before 000B, so a not-running report shows nothing; only the release does.
        if (c.stopModeRaw == 3 && released)
            done = previousReleased && still;
        else if (c.stopModeRaw == 3 && c.standstillObserving && c.predecessorWord == 0x010f)
            done = false;
        if (c.stopModeRaw == 6)
            done = !running && feedback && c.extraObservation.actualVelocity == 0;
        wantStandstill = latched && !c.standstillObserving && ageCoversCycle(c.options, 7);
        break;
    }
    case ActionKind::CLEAR_ALARM:
        outsideScope |= running;
        done = !alarm;
        break;
    case ActionKind::SET_DEVICE_POSITION:
        outsideScope |= running || alarm;
        done = c.extraObservation.actualPosition == 0;
        break;
    case ActionKind::ENABLE:
        outsideScope = !enableScopeHolds(c);
        done = (c.statusObservation.statusword & STATUS_LOW_BITS) == ENABLE_LOW_BITS[c.commandCursor] &&
               (c.commandCursor + 1u < commandCount(c) || (monitor & MONITOR_RELEASED) == 0);
        break;
    }
    if (outsideScope)
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now,
             "drive report outside this action's supported observation scope");
    else if (done && enabling(c)) {
        if (!anotherStepAvailable && c.commandCursor + 1u < commandCount(c))
            fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "action service identity exhausted");
        else
            acceptEnableStep(c, now);
    } else if (done && c.actionKind == ActionKind::CLEAR_ALARM) {
        // These reads may consume data, so take them once, after the alarm has cleared.
        if (!anotherStepAvailable) {
            fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "action service identity exhausted");
        } else {
            c.finalErrorRead = true;
            nextCycle(c, now, now);
        }
    } else if (done) {
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
        // The wider plan starts only with a cycle actually prepared for it, so a retained read
        // always matches observationMask().
        if (c.eligibleUs < c.deadlineUs) {
            c.standstillObserving |= wantStandstill;
            if (!prepareCycle(c, c.eligibleUs)) {
                if (wantStandstill)
                    c.standstillObserving = false; // prepareState left the old read in place.
                fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "action observation could not be prepared");
            }
        }
    }
}
// Every caller has validated; output is disjoint from inputs, and nothing here can fail.
void publishStop(ActionContext& output, const AxisReservation& r,
                 const CANOPEN::SdoCompletion& control, StopKind kind, uint8_t mode, uint32_t operation,
                 uint64_t now, uint64_t deadline, const ActionOptions& options) {
    new (static_cast<void*>(&output)) ActionContext();
    output.reservation = r;
    output.kind = kind;
    output.stopModeRaw = mode;
    output.predecessorWord = controlWord(control.transfer);
    output.predecessorDocumented = mode == 1 && output.predecessorWord == 0x000f;
    output.predecessorBasis = !output.predecessorDocumented ? PredecessorBasis::PROJECT_DECISION
                              : kind == StopKind::QUICK_STOP ? PredecessorBasis::EN86_ZH86_OP86
                                                             : PredecessorBasis::ZH86_OP86;
    output.options = options;
    output.operation = operation;
    output.startedUs = output.servicedUs = output.eligibleUs = output.stepStartedUs = now;
    output.deadlineUs = deadline;
    output.state = ActionState::ACTIVE;
    output.controlEvidence = control;
    // Mode 3 is admitted only on a fresh running report after 000F (validateStop), so a later release
    // at standstill shows that this 000B acted (S18). Escalation follows 010F, which no record covers.
    output.standstillObserving = mode == 3 && kind == StopKind::QUICK_STOP &&
                                 output.predecessorWord == 0x000f && ageCoversCycle(options, 7);
}
Status validateExplicit(const AxisReservation& r, const ReadContext& state, ActionKind kind,
                     uint32_t operation, uint64_t now, uint64_t deadline,
                     const ActionOptions& options) {
    if (!validReservation(r) || !operation || deadline <= now || r.acquiredUs > now ||
        !options.service.queueTimeoutUs || !options.service.responseTimeoutUs ||
        options.service.identityWidths != IdentityWidths::DECLARED_U16 ||
        options.service.countWidths != CountWidths::DECLARED_U8 ||
        !options.observationIntervalUs || !options.maximumAgeUs ||
        !ageCoversCycle(options, kind == ActionKind::SET_DEVICE_POSITION ? 7 : 5))
        return error(ActionError::ARGUMENT, "invalid explicit action reservation or budgets");
    ReadWindow status, monitors;
    const uint64_t age = options.maximumAgeUs;
    if (state.source != ReadSource::SDO || state.state != ReadState::SUCCEEDED || state.uncertain ||
        state.startedUs < r.acquiredUs ||
        !validateStateBlock(state, StateBlock::STATUS, r.target, now, age, status) ||
        !validateStateBlock(state, StateBlock::MONITORS, r.target, now, age, monitors) ||
        status.earliestUs < r.acquiredUs || monitors.earliestUs < r.acquiredUs ||
        uint8_t(raw(state, StateBlock::STATUS, 1)) != 1 ||
        (raw(state, StateBlock::MONITORS, 0) & MONITOR_RUNNING))
        return error(ActionError::STATE, "fresh mode 1 not-running SDO evidence required");
    const bool alarmed = (raw(state, StateBlock::MONITORS, 0) & MONITOR_ALARM) != 0;
    if (kind == ActionKind::CLEAR_ALARM) {
        // EN86 p16 names FF05 position deviation and p20 lists a resettable position
        // out-of-tolerance LED class; linking them is an inference (D-26). FF05 only narrows the
        // attempt: 603F may keep an earlier code, so it cannot say the reported alarm is that event.
        ReadWindow errors, history;
        if (!alarmed || !validateStateBlock(state, StateBlock::ERRORS, r.target, now, age, errors) ||
            !validateStateBlock(state, StateBlock::ERROR_HISTORY, r.target, now, age, history) ||
            errors.earliestUs < r.acquiredUs || history.earliestUs < r.acquiredUs ||
            uint16_t(raw(state, StateBlock::ERRORS, 1)) != 0xff05)
            return error(ActionError::STATE,
                         "alarm reported with raw 603F FF05 and error history required; "
                         "the code does not identify the alarm");
        return Ok();
    }
    if (alarmed)
        return error(ActionError::STATE, "counter clear requires no reported alarm");
    ReadWindow feedback;
    if (!validateStateBlock(state, StateBlock::FEEDBACK, r.target, now, age, feedback) ||
        feedback.earliestUs < r.acquiredUs || raw(state, StateBlock::FEEDBACK, 1))
        return error(ActionError::STATE, "counter clear requires fresh 6064 and 606C zero");
    return Ok();
}
void publishExplicit(ActionContext& c, const AxisReservation& r, const ReadContext& state,
                     ActionKind kind, uint32_t operation, uint64_t now, uint64_t deadline,
                     const ActionOptions& options) {
    new (static_cast<void*>(&c)) ActionContext();
    c.reservation = r;
    c.actionKind = kind;
    c.options = options;
    c.operation = operation;
    c.startedUs = c.servicedUs = c.eligibleUs = c.stepStartedUs = now;
    c.deadlineUs = deadline;
    c.state = ActionState::ACTIVE;
    const uint64_t age = options.maximumAgeUs;
    getStateBlock(state, StateBlock::STATUS, r.target, now, age, c.statusObservation);
    getStateBlock(state, StateBlock::MONITORS, r.target, now, age, c.monitorObservation);
    if (kind != ActionKind::CLEAR_ALARM) {
        // Later cycles replace extraObservation; the baseline keeps what admission saw.
        getStateBlock(state, StateBlock::FEEDBACK, r.target, now, age, c.extraObservation);
        CounterBaseline& b = c.baseline;
        b.present = true;
        b.startPosition = c.extraObservation.actualPosition;
        b.startVelocity = c.extraObservation.actualVelocity;
        b.earliestUs = c.extraObservation.window.earliestUs;
        b.latestUs = c.extraObservation.window.latestUs;
        return;
    }
    getStateBlock(state, StateBlock::ERRORS, r.target, now, age, c.extraObservation);
    // The incident record outlives them: later cycles replace the observations above.
    StateObservation history;
    getStateBlock(state, StateBlock::ERROR_HISTORY, r.target, now, age, history);
    AlarmIncident& i = c.incident;
    i.present = true;
    i.codeLink = AlarmCodeLink::UNRESOLVED;
    i.readOperation = state.operation;
    i.errorRegister = c.extraObservation.errorRegister;
    i.errorCode = c.extraObservation.errorCode;
    i.errorCount = history.errorCount;
    for (unsigned n = 0; n < 4; ++n)
        i.errorHistory[n] = history.errorHistory[n];
    i.monitor = c.monitorObservation.monitor;
    i.logicalInputs = c.monitorObservation.logicalInputs;
    i.logicalOutputs = c.monitorObservation.logicalOutputs;
    i.statusword = c.statusObservation.statusword;
    i.modeRaw = c.statusObservation.modeRaw;
    i.earliestUs = i.latestUs = c.statusObservation.window.earliestUs;
    const ReadWindow* windows[] = {&c.statusObservation.window, &c.monitorObservation.window,
                                   &c.extraObservation.window, &history.window};
    for (const ReadWindow* w : windows) {
        if (w->earliestUs < i.earliestUs)
            i.earliestUs = w->earliestUs;
        if (w->latestUs > i.latestUs)
            i.latestUs = w->latestUs;
    }
}
} // namespace

bool stopOptionsValid(const ActionOptions& options) noexcept {
    return options.service.queueTimeoutUs && options.service.responseTimeoutUs &&
           options.service.identityWidths == IdentityWidths::DECLARED_U16 &&
           options.service.countWidths == CountWidths::DECLARED_U8 &&
           options.observationIntervalUs && options.maximumAgeUs && ageCoversCycle(options, 5);
}

// DEC-8: only 000F has documented arrows, and only in the cited manuals. The others rest
// on derived words, allowed because a running move with no stop is worse.
bool stopPredecessorAllowed(uint16_t word, StopKind kind) noexcept {
    switch (word) {
    case 0x000f:
    case 0x001f:
    case 0x005f:
    case 0x003f:
    case 0x007f:
        return true;
    case 0x010f:
        return kind == StopKind::QUICK_STOP;
    default:
        return false;
    }
}

Status validateStop(const ActionContext& output, const AxisReservation& r, const ReadContext& state,
                 const CANOPEN::SdoCompletion& control, StopKind kind, uint32_t operation,
                 uint64_t now, uint64_t deadline, const ActionOptions& options) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &control, sizeof control) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ActionError::OVERLAP, "stop preparation overlaps input");
    if (!validReservation(r) || kind > StopKind::QUICK_STOP || !operation || deadline <= now ||
        !stopOptionsValid(options) || r.acquiredUs > now)
        return error(ActionError::ARGUMENT,
                     "invalid stop reservation, selection or caller budgets");
    ReadWindow status, monitors;
    if (!controlEvidence(control, r, kind, now) ||
        state.source != ReadSource::SDO || state.state != ReadState::SUCCEEDED || state.uncertain ||
        !validateStateBlock(state, StateBlock::STATUS, r.target, now, options.maximumAgeUs, status) ||
        !validateStateBlock(state, StateBlock::MONITORS, r.target, now, options.maximumAgeUs, monitors) ||
        (raw(state, StateBlock::STATUS, 0) & 7u) != 7 ||
        (raw(state, StateBlock::MONITORS, 0) & (MONITOR_ALARM | MONITOR_RELEASED)) ||
        status.earliestUs < control.completedUs || monitors.earliestUs < control.completedUs)
        return error(ActionError::STATE,
                     "allowed acknowledged predecessor and fresh drive state required");
    const uint8_t mode = uint8_t(raw(state, StateBlock::STATUS, 1));
    const bool velocity = mode == 3 && controlWord(control.transfer) == 0x000f &&
                          (raw(state, StateBlock::MONITORS, 0) & MONITOR_RUNNING);
    // EN86/ZH86 p15 document the homing bit4 falling edge. 001F -> 000F is a
    // derived whole-word transition, not the position section's normal-stop arrow.
    const bool home = mode == 6 && kind == StopKind::HALT &&
                      controlWord(control.transfer) == 0x001f && ageCoversCycle(options, 7);
    if (mode != 1 && !velocity && !home)
        return error(ActionError::STATE,
                     "no reviewed stop transition for this mode and predecessor");
    return Ok();
}

Status prepareStop(ActionContext& output, const AxisReservation& r, const ReadContext& state,
                   const CANOPEN::SdoCompletion& control, StopKind kind, uint32_t operation,
                   uint64_t now, uint64_t deadline, const ActionOptions& options) noexcept {
    const Status s = validateStop(output, r, state, control, kind, operation, now, deadline, options);
    if (!s)
        return s;
    publishStop(output, r, control, kind, uint8_t(raw(state, StateBlock::STATUS, 1)),
                operation, now, deadline, options);
    getStateBlock(state, StateBlock::STATUS, r.target, now, options.maximumAgeUs, output.statusObservation);
    getStateBlock(state, StateBlock::MONITORS, r.target, now, options.maximumAgeUs, output.monitorObservation);
    return Ok();
}

Status validateEscalation(const ActionContext& output, const ActionContext& running, uint32_t operation,
                       uint64_t now, uint64_t deadline, const ActionOptions& options) noexcept {
    if (overlaps(&output, sizeof output, &running, sizeof running) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ActionError::OVERLAP, "stop escalation overlaps input");
    if (!validReservation(running.reservation) || !operation || deadline <= now ||
        !stopOptionsValid(options) || running.reservation.acquiredUs > now)
        return error(ActionError::ARGUMENT, "invalid escalation reservation or budgets");
    const CANOPEN::SdoCompletion& control = running.commandHistory[0]; // A stop has one command.
    const StateObservation& status = running.statusObservation;
    const StateObservation& monitors = running.monitorObservation;
    // A withdrawn read also withdraws the cycle before it, which came down the same route.
    if (running.actionKind != ActionKind::STOP || running.kind != StopKind::HALT ||
        (running.stopModeRaw != 1 && running.stopModeRaw != 3) ||
        running.read.invalidated || !(running.commandEvidenceMask & 1u) || running.execution != ActionExecution::ACKNOWLEDGED ||
        !running.observationKnown || !controlEvidence(control, running.reservation, StopKind::QUICK_STOP, now) ||
        controlWord(control.transfer) != 0x010f || !fresh(status, now, options.maximumAgeUs) ||
        !fresh(monitors, now, options.maximumAgeUs) || !after(status, control.completedUs) ||
        !after(monitors, control.completedUs) || status.modeRaw != running.stopModeRaw ||
        (running.stopModeRaw == 3 && !(monitors.monitor & MONITOR_RUNNING)) ||
        (status.statusword & 7u) != 7 || (monitors.monitor & (MONITOR_ALARM | MONITOR_RELEASED)))
        return error(ActionError::STATE, "acknowledged HALT and a fresh later cycle required");
    return Ok();
}

Status prepareEscalation(ActionContext& output, const ActionContext& running, uint32_t operation,
                         uint64_t now, uint64_t deadline, const ActionOptions& options) noexcept {
    const Status s = validateEscalation(output, running, operation, now, deadline, options);
    if (!s)
        return s;
    publishStop(output, running.reservation, running.commandHistory[0], StopKind::QUICK_STOP,
                running.stopModeRaw, operation, now, deadline, options);
    output.statusObservation = running.statusObservation;
    output.monitorObservation = running.monitorObservation;
    return Ok();
}

void resetAction(ActionContext& c) noexcept {
    new (static_cast<void*>(&c)) ActionContext();
}

Status validateEnable(const ActionContext& output, const AxisReservation& r, const ReadContext& state,
                      const EnableEvidence& evidence, uint32_t operation, uint64_t now,
                      uint64_t deadline, const ActionOptions& options) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &evidence, sizeof evidence) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ActionError::OVERLAP, "enable preparation overlaps input");
    if (evidence.gate == EnableGate::CLOSED)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ActionError::STATE),
                      "enable is gated: not qualified and no pending-motion rule is known");
    if (evidence.gate > EnableGate::ATTENDED_EXPERIMENT ||
        evidence.start > EnableStart::RELEASED_BY_RECORDED_STOP ||
        evidence.powerUpShaft > ShaftObservation::HOLDING || !evidence.trial ||
        (evidence.modeRaw != 1 && evidence.modeRaw != 6) || !validReservation(r) || !operation ||
        deadline <= now || r.acquiredUs > now || !stopOptionsValid(options) ||
        !ageCoversCycle(options, 9))
        return error(ActionError::ARGUMENT, "invalid enable trial, reservation or budgets");
    // Room for three intervals and every scheduled service at full budget. Each term is bounded
    // first, so nothing here can wrap.
    const uint64_t interval = options.commandIntervalUs;
    const uint64_t span = deadline - now;
    const uint64_t services =
        ENABLE_SERVICE_STEPS *
        (uint64_t(options.service.queueTimeoutUs) + options.service.responseTimeoutUs);
    if (interval < ENABLE_WORD_INTERVAL_US || interval > span / 3 || span - 3 * interval <= services)
        return error(ActionError::ARGUMENT,
                     "enable deadline does not cover three intervals and every scheduled service");
    if (!sameReservation(evidence.reservation, r))
        return error(ActionError::TARGET, "enable evidence was reviewed for another reservation");
    if (now >= evidence.expiresUs)
        return error(ActionError::TIME, "enable evidence has expired");
    if (evidence.operation != operation)
        return error(ActionError::STATE, "enable evidence names another operation");
    if (evidence.start == EnableStart::UNKNOWN || !evidence.reviewed)
        return error(ActionError::STATE, "enable needs the caller's start record and exact reviewed values");
    // The gate is open here: a closed one returned UNSUPPORTED above, and the only other is the attended
    // experiment, the one gate this start is defined for.
    const bool fromRelease = releasedStart(evidence);
    if (fromRelease && evidence.modeRaw != 1)
        return error(ActionError::STATE, "a released-by-recorded-stop enable is a mode 1 experiment only");
    const uint64_t age = options.maximumAgeUs;
    ReadWindow window;
    const StateBlock blocks[] = {StateBlock::ERRORS, StateBlock::MONITORS, StateBlock::STATUS,
                                 StateBlock::FEEDBACK};
    if (state.source != ReadSource::SDO || state.state != ReadState::SUCCEEDED || state.uncertain ||
        state.startedUs < r.acquiredUs)
        return error(ActionError::STATE, "enable needs a complete SDO state read under this reservation");
    for (StateBlock block : blocks) {
        if (!validateStateBlock(state, block, r.target, now, age, window) ||
            window.earliestUs < r.acquiredUs)
            return error(ActionError::STATE, "enable needs fresh ERRORS, MONITORS, STATUS and FEEDBACK");
    }
    const uint8_t mode = uint8_t(raw(state, StateBlock::STATUS, 1));
    if (mode != evidence.modeRaw)
        return error(ActionError::STATE, "enable mode differs from the explicitly reviewed mode");
    if (mode == 6 && !evidence.homingEnableQualified)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ActionError::STATE),
                      "mode6 enable needs its own selected-firmware evidence");
    const uint16_t monitor = uint16_t(raw(state, StateBlock::MONITORS, 0));
    constexpr uint16_t ignored = MONITOR_RELEASED | MONITOR_HOMING_COMPLETE;
    const uint16_t allowed = mode == 6 ? uint16_t(ignored | MONITOR_POSITION_COMPLETE) : ignored;
    if ((monitor & uint16_t(~allowed)) != (mode == 6 ? 0 : MONITOR_POSITION_COMPLETE))
        return error(ActionError::STATE, "enable monitor is outside the reviewed stationary scope");
    if (mode == 6 && (!(monitor & MONITOR_RELEASED) ||
                      (raw(state, StateBlock::STATUS, 0) & STATUS_LOW_BITS) == STATUS_LOW_BITS))
        return error(ActionError::STATE, "mode6 enable requires a non-enabled released start");
    const uint16_t statusword = uint16_t(raw(state, StateBlock::STATUS, 0));
    // The released standstill a recorded stop left: 2100 released and 6041 low bits 000, on top of the
    // monitor rule above, which keeps running clear and the in-place bit set.
    if (fromRelease && (!(monitor & MONITOR_RELEASED) || (statusword & STATUS_LOW_BITS)))
        return error(ActionError::STATE,
                     "a released-by-recorded-stop enable needs 2100 released and 6041 low bits 000");
    if (statusword != evidence.reviewedStatusword)
        return error(ActionError::STATE, "6041 differs from the reviewed word");
    // The manual pairs 111 with an enabled drive, and 2100 then reads enabled. Both specimens report
    // that at power-up, so word 6 may remove holding torque: only an attendant can say what is there.
    if ((statusword & STATUS_LOW_BITS) == STATUS_LOW_BITS && !(monitor & MONITOR_RELEASED) &&
        evidence.powerUpShaft == ShaftObservation::NONE)
        return error(ActionError::STATE,
                     "start report reads as enabled: an attended power-up shaft observation is required");
    if (raw(state, StateBlock::ERRORS, 0) || raw(state, StateBlock::ERRORS, 1))
        return error(ActionError::STATE, "enable needs 1001 and 603F zero");
    if (raw(state, StateBlock::FEEDBACK, 1))
        return error(ActionError::STATE, "enable needs 606C zero");
    return Ok();
}

Status prepareEnable(ActionContext& output, const AxisReservation& r, const ReadContext& state,
                     const EnableEvidence& evidence, uint32_t operation, uint64_t now,
                     uint64_t deadline, const ActionOptions& options) noexcept {
    const Status s = validateEnable(output, r, state, evidence, operation, now, deadline, options);
    if (!s)
        return s;
    new (static_cast<void*>(&output)) ActionContext();
    output.reservation = r;
    output.actionKind = ActionKind::ENABLE;
    output.options = options;
    output.operation = operation;
    output.startedUs = output.servicedUs = output.eligibleUs = output.stepStartedUs = now;
    output.deadlineUs = deadline;
    output.state = ActionState::ACTIVE;
    output.phase = ActionPhase::TARGETS;
    const uint64_t age = options.maximumAgeUs;
    getStateBlock(state, StateBlock::STATUS, r.target, now, age, output.statusObservation);
    getStateBlock(state, StateBlock::MONITORS, r.target, now, age, output.monitorObservation);
    getStateBlock(state, StateBlock::FEEDBACK, r.target, now, age, output.extraObservation);
    EnableRecord& record = output.enable;
    record.evidence = evidence;
    record.startStatusword = output.statusObservation.statusword;
    record.startMonitor = output.monitorObservation.monitor;
    record.startPosition = output.extraObservation.actualPosition;
    return Ok();
}

Status prepareClearAlarm(ActionContext& output, const AxisReservation& r,
                         const ReadContext& state, uint32_t operation, uint64_t now,
                         uint64_t deadline, const ActionOptions& options) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ActionError::OVERLAP, "alarm preparation overlaps input");
    const Status s =
        validateExplicit(r, state, ActionKind::CLEAR_ALARM, operation, now, deadline, options);
    if (!s)
        return s;
    publishExplicit(output, r, state, ActionKind::CLEAR_ALARM, operation, now, deadline, options);
    return Ok();
}

Status prepareSetDevicePosition(ActionContext& output, const AxisReservation& r,
                                const ReadContext& state, int64_t nativePosition,
                                const AxisConfig& axis, uint32_t operation, uint64_t now,
                                uint64_t deadline, const ActionOptions& options) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &axis, sizeof axis) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ActionError::OVERLAP, "counter preparation overlaps input");
    if (nativePosition)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ActionError::ARGUMENT),
                      "documented device position action supports zero only");
    if (!validateAxisConfig(axis) || !sameBinding(axis.binding, r.target.channel.binding) ||
        axis.exhausted || !axis.referenceGeneration || axis.referenceGeneration == UINT32_MAX ||
        axis.binding.configurationGeneration == UINT32_MAX)
        return error(ActionError::TARGET, "counter clear needs matching nonexhausted axis epochs");
    const Status s =
        validateExplicit(r, state, ActionKind::SET_DEVICE_POSITION, operation, now, deadline, options);
    if (!s)
        return s;
    publishExplicit(output, r, state, ActionKind::SET_DEVICE_POSITION, operation, now, deadline,
                    options);
    output.originalReferenceGeneration = axis.referenceGeneration;
    return Ok();
}

// The current work of an active context. A withdrawn one still has work in flight: the public call
// yields nothing new for it, while advanceAction needs the identity to correlate its terminal.
static Status yieldAction(const ActionContext& c, const AxisReservation& live,
                          const ReadTarget& target, uint64_t now, PreparedAction& output,
                          bool includeWithdrawn) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &target, sizeof target))
        return error(ActionError::OVERLAP, "action work overlaps input");
    const Status s = current(c, live, target, now);
    if (!s)
        return s;
    if (c.state == ActionState::ACTIVE && now >= c.deadlineUs)
        return error(ActionError::TIME, "action overall deadline");
    PreparedAction next;
    next.eligibleUs = c.eligibleUs;
    // A new observation read is held back once its budget no longer fits, and the action ends at its
    // deadline with nothing in flight. Commands keep their own cutoff, and correlation rebuilds work
    // already handed out, so neither is affected.
    const bool late = !includeWithdrawn && c.phase != ActionPhase::COMMAND &&
                      c.phase != ActionPhase::TARGETS &&
                      !exchangeFits(c.deadlineUs, now, c.options.service.queueTimeoutUs,
                                    c.options.service.responseTimeoutUs);
    if (c.state != ActionState::ACTIVE || now < c.eligibleUs || late ||
        (c.withdrawal != ActionOutcome::NONE && !includeWithdrawn)) {
        output = next;
        return Ok();
    }
    next.ready = true;
    if (c.phase == ActionPhase::COMMAND)
        next.transfer = command(c);
    else if (c.phase == ActionPhase::TARGETS)
        next.transfer = targetUpload(c);
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

Status nextAction(const ActionContext& c, const AxisReservation& live, const ReadTarget& target,
                  uint64_t now, PreparedAction& output) noexcept {
    return yieldAction(c, live, target, now, output, false);
}

Status noteActionWithdrawal(ActionContext& c, const AxisReservation& live, const ReadTarget& target,
                            ActionOutcome reason, uint64_t now) noexcept {
    if (commonOverlap(c, live, target))
        return error(ActionError::OVERLAP, "action withdrawal overlaps input");
    const Status s =
        reason == ActionOutcome::INVALIDATED ? shape(c, now) : current(c, live, target, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.withdrawal != ActionOutcome::NONE ||
        (reason != ActionOutcome::CANCELLED && reason != ActionOutcome::DRIVE_FAULT &&
         reason != ActionOutcome::INVALIDATED))
        return error(ActionError::STATE, "invalid action withdrawal");
    // servicedUs stays: the terminal in flight may have completed before this call.
    c.withdrawal = now >= c.deadlineUs ? ActionOutcome::DEADLINE : reason;
    return Ok();
}

Status noteActionTransmission(ActionContext& c, const AxisReservation& live,
                              const ReadTarget& target, uint64_t now) noexcept {
    if (commonOverlap(c, live, target))
        return error(ActionError::OVERLAP, "action TX notice overlaps input");
    const Status s = current(c, live, target, now);
    if (!s)
        return s;
    if (c.actionKind == ActionKind::SET_DEVICE_POSITION)
        return error(ActionError::STATE, "counter action requires axis-aware transmission notice");
    if (c.state != ActionState::ACTIVE || c.phase != ActionPhase::COMMAND || c.transmissionNoted ||
        now >= c.deadlineUs || now < c.eligibleUs || c.withdrawal != ActionOutcome::NONE)
        return error(ActionError::STATE, "no new live action download");
    c.transmissionNoted = c.effectsPossible = c.uncertain = true;
    if (effecting(c))
        c.execution = ActionExecution::UNKNOWN;
    c.servicedUs = now;
    return Ok();
}

Status noteActionTransmission(ActionContext& c, const AxisReservation& live, ReadTarget& target,
                              AxisConfig& axis, AxisReference& reference, uint64_t now) noexcept {
    const void* objects[] = {&c, &target, &axis, &reference};
    const size_t sizes[] = {sizeof c, sizeof target, sizeof axis, sizeof reference};
    for (unsigned i = 0; i < 4; ++i) {
        if (overlaps(objects[i], sizes[i], &live, sizeof live))
            return error(ActionError::OVERLAP, "counter notice overlaps reservation");
        for (unsigned j = 0; j < i; ++j)
            if (overlaps(objects[i], sizes[i], objects[j], sizes[j]))
                return error(ActionError::OVERLAP, "counter notice outputs overlap");
    }
    Status s = current(c, live, target, now);
    if (!s)
        return s;
    if (c.actionKind != ActionKind::SET_DEVICE_POSITION || c.state != ActionState::ACTIVE ||
        c.phase != ActionPhase::COMMAND || c.transmissionNoted || now >= c.deadlineUs ||
        now < c.eligibleUs || c.withdrawal != ActionOutcome::NONE || !validateAxisConfig(axis) ||
        axis.exhausted ||
        !sameBinding(axis.binding, target.channel.binding) ||
        axis.referenceGeneration !=
            c.originalReferenceGeneration + (c.coordinatesInvalidated ? 1u : 0u) ||
        (reference.nativeKnown && (!sameBinding(reference.binding, axis.binding) ||
                                   reference.referenceGeneration != axis.referenceGeneration)) ||
        (c.coordinatesInvalidated && (axis.originKnown || axis.encoderOriginKnown ||
                                      axis.softLimitsKnown || reference.nativeKnown)))
        return error(ActionError::STATE, "counter notice requires frozen axis and fresh command");
    const bool trigger = c.commandCursor == 1 && !c.coordinatesInvalidated;
    // invalidateAxisReference mutates even when it reports exhaustion, so prove room first.
    if (trigger && (!axis.referenceGeneration || axis.referenceGeneration == UINT32_MAX ||
                    !axis.binding.configurationGeneration ||
                    axis.binding.configurationGeneration == UINT32_MAX))
        return error(ActionError::STATE, "counter notice requires room in both axis epochs");
    if (trigger) {
        s = invalidateAxisReference(axis, reference, ReferenceLoss::COORDINATES);
        if (!s)
            return s; // Unreachable after the check above.
        target.channel.binding = axis.binding;
        c.coordinatesInvalidated = true;
    }
    c.transmissionNoted = c.effectsPossible = c.uncertain = true;
    if (effecting(c))
        c.execution = ActionExecution::UNKNOWN;
    c.servicedUs = now;
    return Ok();
}

Status advanceAction(ActionContext& c, const AxisReservation& live, const ReadTarget& target,
                     const CANOPEN::SdoCompletion& input, uint64_t now) noexcept {
    if (commonOverlap(c, live, target) || overlaps(&c, sizeof c, &input, sizeof input))
        return error(ActionError::OVERLAP, "action terminal overlaps input");
    Status s = current(c, live, target, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE)
        return error(ActionError::STATE, "action already terminal");
    PreparedAction work;
    // As with ordinary reads, validate correlation before consuming a deadline.
    s = yieldAction(c, live, target, c.servicedUs < c.eligibleUs ? c.eligibleUs : c.servicedUs, work,
                    true);
    if (!s)
        return s;
    // A withdrawn action keeps this terminal as evidence, and nothing in it can advance the action.
    const bool withdrawn = c.withdrawal != ActionOutcome::NONE;
    if (!work.ready || !CANOPEN::sameSdoWork(input.transfer, work.transfer) ||
        input.nodeGeneration != c.reservation.target.nodeGeneration ||
        input.admittedUs < c.stepStartedUs || input.admittedUs >= c.deadlineUs ||
        input.completedUs < input.admittedUs || input.completedUs > now ||
        input.completedUs < c.servicedUs)
        return error(ActionError::CORRELATION, "action terminal does not match current service");
    if (c.phase == ActionPhase::TARGETS) {
        const bool success = input.outcome == CANOPEN::SdoOutcome::SUCCESS;
        if (!CANOPEN::checkUploadTerminal(input, 4))
            return error(ActionError::CORRELATION, "inconsistent enable target terminal");
        const bool expired = now >= c.deadlineUs || input.completedUs >= c.deadlineUs;
        uint32_t value = 0;
        if (success && !expired) {
            s = CANOPEN::readWord(input.data, input.size, 4, value);
            if (!s)
                return s;
        }
        c.observationEvidence = input;
        c.observationEvidencePresent = true;
        c.uncertain |= input.effectUncertain;
        if (withdrawn) {
            if (success && !expired) {
                (c.enable.targetCursor ? c.enable.velocityTarget : c.enable.positionTarget) =
                    signed32(value);
                (c.enable.targetCursor ? c.enable.velocityTargetKnown
                                       : c.enable.positionTargetKnown) = true;
            }
        } else if (expired)
            fail(c, ActionOutcome::DEADLINE, now, "action overall deadline");
        else if (!success)
            fail(c, outcome(input.outcome), now, "enable target read failed");
        else {
            EnableRecord& record = c.enable;
            ++c.step;
            c.eligibleUs = c.stepStartedUs = now;
            if (!record.targetCursor) {
                record.positionTarget = signed32(value);
                record.positionTargetKnown = true;
                record.targetCursor = 1;
            } else {
                record.velocityTarget = signed32(value);
                record.velocityTargetKnown = true;
                // Never zeroed, never written: an unreviewed value refuses before any 6040 word.
                if (record.positionTarget != record.evidence.reviewedPositionTarget ||
                    record.velocityTarget != record.evidence.reviewedVelocityTarget)
                    fail(c, ActionOutcome::OBSERVATION_SCOPE, now,
                         "607A or 60FF differs from the reviewed value; no 6040 word was sent");
                else
                    c.phase = ActionPhase::COMMAND;
            }
        }
    } else if (c.phase == ActionPhase::COMMAND) {
        const bool success = input.outcome == CANOPEN::SdoOutcome::SUCCESS;
        if (!CANOPEN::checkDownloadTerminal(input, c.transmissionNoted))
            return error(ActionError::CORRELATION, "inconsistent action command terminal");
        const bool expired = now >= c.deadlineUs || input.completedUs >= c.deadlineUs;
        const unsigned cursor = c.commandCursor;
        // An enable observes after every word, each other action after its last.
        const bool last = enabling(c) || cursor + 1u >= commandCount(c);
        // The only refusal left. It replaces c.read alone, atomically, before anything else moves.
        if (success && !expired && last && !withdrawn) {
            s = prepareCycle(c, now);
            if (!s)
                return s;
        }
        c.commandHistory[cursor] = input;
        c.commandEvidenceMask |= uint8_t(1u << cursor);
        if (effecting(c)) {
            c.execution = success ? ActionExecution::ACKNOWLEDGED
                          : input.outcome == CANOPEN::SdoOutcome::SERVER_ABORT
                              ? ActionExecution::REJECTED
                          : input.transmissionPossible ? ActionExecution::UNKNOWN
                                                       : ActionExecution::NOT_TRANSMITTED;
        }
        // Each settled terminal refines its own possible-TX notice. Earlier
        // prefix effects and a coordinate withdrawal are never rolled back.
        c.effectsPossible = input.effectPossible;
        c.uncertain = input.effectUncertain;
        for (unsigned i = 0; i < cursor; ++i) {
            c.effectsPossible |= c.commandHistory[i].effectPossible;
            c.uncertain |= c.commandHistory[i].effectUncertain;
        }
        if (withdrawn) {
        } else if (expired)
            fail(c, ActionOutcome::DEADLINE, now, "action overall deadline");
        else if (!success)
            fail(c, outcome(input.outcome), now, "action command service failed");
        else {
            if (last)
                c.phase = ActionPhase::OBSERVING;
            else
                ++c.commandCursor;
            ++c.step;
            c.transmissionNoted = false;
            c.eligibleUs = c.stepStartedUs = now;
        }
    } else {
        // A finished cycle may have to prepare the next one, and by then this read has moved.
        // So prove it now: the outcome of prepareCycle does not depend on this terminal.
        if (now < c.deadlineUs && !withdrawn) {
            s = validateRead(ReadKind::STATE, expectedTarget(c), c.operation, now, c.deadlineUs,
                          c.options.service, observationMask(c));
            if (!s)
                return s;
        }
        CANOPEN::SdoCompletion local = input;
        local.transfer.step = uint32_t(c.read.cursor) + 1;
        s = advanceRead(c.read, local, now);
        if (!s)
            return s;
        c.read.evidence = input; // Preserve actual external identity in retained history.
        c.observationEvidence = input;
        c.observationEvidencePresent = true;
        c.uncertain |= c.read.uncertain;
        if (withdrawn) {
        } else if (c.read.state == ReadState::FAILED)
            fail(c, outcome(c.read.outcome), now, "action observation service failed");
        else if (c.step == UINT32_MAX) {
            // The last representable identity may still close an observed stop;
            // only work requiring another distinct identity is exhausted.
            if (c.read.state == ReadState::SUCCEEDED)
                finishCycle(c, now, false);
            else
                fail(c, ActionOutcome::ITERATION_EXHAUSTED, now,
                     "action service identity exhausted");
        } else {
            ++c.step;
            c.stepStartedUs = now;
            c.phase = ActionPhase::OBSERVING;
            if (c.read.state == ReadState::SUCCEEDED)
                finishCycle(c, now, true);
        }
    }
    if (withdrawn)
        fail(c, c.withdrawal, now, "action withdrawn; the terminal in flight is kept as evidence");
    c.servicedUs = now;
    return Ok();
}

Status advanceAction(ActionContext& c, const AxisReservation& live, const ReadTarget& target,
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
    if (c.phase != ActionPhase::COMMAND && c.read.state == ReadState::ACTIVE) {
        // During WAITING the read is prepared at a future eligible time and has
        // not been admitted; cancelling then must not fabricate read effects.
        if (now >= c.eligibleUs) {
            const ReadOutcome readReason =
                reason == ActionOutcome::INVALIDATED ? ReadOutcome::INVALIDATED
                : reason == ActionOutcome::DEADLINE  ? ReadOutcome::DEADLINE
                                                     : ReadOutcome::CANCELLED;
            // Atomic on c.read, and nothing else has moved yet.
            s = advanceRead(c.read, readReason, now);
            if (!s)
                return s;
            c.uncertain |= c.read.uncertain;
        }
    }
    if (now >= c.deadlineUs)
        reason = ActionOutcome::DEADLINE;
    fail(c, reason, now, "action locally withdrawn; no replay or physical stop proof");
    return Ok();
}

Status prepareBestEffortStop(BestEffortStop& output, const ReadTarget& target, uint32_t operation,
                             uint64_t now, uint64_t deadline, const ReadOptions& service) noexcept {
    if (overlaps(&output, sizeof output, &target, sizeof target) ||
        overlaps(&output, sizeof output, &service, sizeof service))
        return error(ActionError::OVERLAP, "best-effort stop preparation overlaps input");
    if (!operation || deadline <= now || !service.queueTimeoutUs || !service.responseTimeoutUs ||
        !target.nodeGeneration || !CANOPEN::validateSdoChannel(target.channel))
        return error(ActionError::ARGUMENT, "invalid best-effort stop target or budgets");
    BestEffortStop c;
    c.target = target;
    c.service = service;
    c.operation = operation;
    c.startedUs = c.servicedUs = now;
    c.deadlineUs = deadline;
    c.state = ActionState::ACTIVE;
    output = c;
    return Ok();
}

static CANOPEN::SdoTransfer bestEffortWork(const BestEffortStop& c) noexcept {
    CANOPEN::SdoTransfer t;
    t.channel = c.target.channel;
    t.operation = t.request = c.operation;
    t.step = 1;
    t.index = 0x6040;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    t.size = 2;
    t.data[0] = 0x0b;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.service.queueTimeoutUs;
    t.responseTimeoutUs = c.service.responseTimeoutUs;
    return t;
}

Status nextBestEffortStop(const BestEffortStop& c, const ReadTarget& current, uint64_t now,
                          PreparedAction& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &current, sizeof current))
        return error(ActionError::OVERLAP, "best-effort stop work overlaps input");
    if (now < c.servicedUs)
        return error(ActionError::TIME, "best-effort stop time moved backward");
    if (!sameTarget(c.target, current))
        return error(ActionError::TARGET, "best-effort stop target changed");
    if (c.state == ActionState::ACTIVE && now >= c.deadlineUs)
        return error(ActionError::TIME, "best-effort stop deadline");
    PreparedAction next;
    next.eligibleUs = c.startedUs;
    if (c.state == ActionState::ACTIVE && !c.admitted) {
        next.ready = true;
        next.transfer = bestEffortWork(c);
    }
    output = next;
    return Ok();
}

Status noteBestEffortAdmission(BestEffortStop& c, uint64_t now) noexcept {
    if (c.state != ActionState::ACTIVE || c.admitted || now < c.servicedUs || now >= c.deadlineUs)
        return error(ActionError::STATE, "no unadmitted best-effort stop");
    c.admitted = true;
    c.servicedUs = now;
    return Ok();
}

Status advanceBestEffortStop(BestEffortStop& c, const CANOPEN::SdoCompletion& e,
                             uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &e, sizeof e))
        return error(ActionError::OVERLAP, "best-effort stop terminal overlaps input");
    if (c.state != ActionState::ACTIVE || !c.admitted || now < c.servicedUs)
        return error(ActionError::STATE, "no admitted best-effort stop");
    const bool sent = e.outcome == CANOPEN::SdoOutcome::UNCONFIRMED_SENT;
    if (!CANOPEN::sameSdoWork(e.transfer, bestEffortWork(c)) ||
        e.nodeGeneration != c.target.nodeGeneration || e.admittedUs < c.startedUs ||
        e.admittedUs >= c.deadlineUs || e.completedUs < e.admittedUs || e.completedUs > now ||
        e.size || e.abortValid || e.protocolAcknowledged ||
        (e.transmissionPossible && !e.queued) || e.effectUncertain != e.transmissionPossible ||
        e.outcome == CANOPEN::SdoOutcome::SUCCESS ||
        e.outcome == CANOPEN::SdoOutcome::SERVER_ABORT ||
        e.outcome == CANOPEN::SdoOutcome::UNSUPPORTED ||
        e.outcome > CANOPEN::SdoOutcome::UNCONFIRMED_SENT ||
        (e.transmissionConfirmed && !e.transmissionPossible) ||
        e.effectPossible != e.transmissionPossible || (sent && !e.transmissionConfirmed))
        return error(ActionError::CORRELATION, "best-effort stop terminal does not match");
    c.commandEvidence = e;
    c.commandEvidencePresent = true;
    c.effectsPossible = e.effectPossible;
    c.execution =
        e.transmissionPossible ? ActionExecution::UNKNOWN : ActionExecution::NOT_TRANSMITTED;
    // A cutoff timeout stays DEADLINE here too; only an earlier one is a spent service budget.
    c.outcome = sent ? ActionOutcome::STOP_SENT_UNCONFIRMED
                : e.outcome == CANOPEN::SdoOutcome::TIMEOUT &&
                        (now >= c.deadlineUs || e.completedUs >= c.deadlineUs)
                    ? ActionOutcome::DEADLINE
                    : outcome(e.outcome);
    c.state = ActionState::FAILED;
    c.status = sent ? error(ActionError::STATE, "stop frame sent; no reply can confirm it")
                    : error(ActionError::STATE, "best-effort stop did not complete");
    c.servicedUs = now;
    return Ok();
}

Status advanceBestEffortStop(BestEffortStop& c, ActionOutcome reason, uint64_t now) noexcept {
    if (c.state != ActionState::ACTIVE || c.admitted || now < c.servicedUs ||
        (reason != ActionOutcome::CANCELLED && reason != ActionOutcome::DEADLINE &&
         reason != ActionOutcome::INVALIDATED && reason != ActionOutcome::TRANSPORT_ERROR) ||
        (reason == ActionOutcome::DEADLINE && now < c.deadlineUs))
        return error(ActionError::STATE, "invalid local best-effort stop termination");
    c.outcome = now >= c.deadlineUs ? ActionOutcome::DEADLINE : reason;
    c.state = ActionState::FAILED;
    c.status = error(ActionError::STATE, "best-effort stop never admitted; nothing was sent");
    c.servicedUs = now;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
