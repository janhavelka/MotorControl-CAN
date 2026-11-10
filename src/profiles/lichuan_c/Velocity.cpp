// SPDX-License-Identifier: MIT
#include "ReadPlan.h"
#include <MotorControlCAN/profiles/lichuan_c/Velocity.h>
#include <cmath>
#include <new>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
constexpr uint8_t CYCLE_FIELDS = 7;
constexpr uint8_t CYCLE_MASK =
    uint8_t((1u << unsigned(StateBlock::MONITORS)) | (1u << unsigned(StateBlock::STATUS)) |
            (1u << unsigned(StateBlock::FEEDBACK)));
constexpr uint16_t ENABLE_WORDS[3] = {6, 7, 15};
// 60FF and 6060 have no writable generated descriptor because their signedness claims conflict.
// This qualified sequence owns only their agreed wire widths: 60FF four bytes, 6060 one byte
// (EDS [60FF]/[6060], ZH86 PDF p13). Signed velocity remains separately evidence-gated.
constexpr uint16_t PARAMETER_INDEXES[3] = {0x60ff, 0x6083, 0x6084};
static_assert(OBJECT_6083_00.widthBytes == 4 && OBJECT_6084_00.widthBytes == 4 &&
                  OBJECT_6040_00.widthBytes == 2,
              "velocity widths must remain those in the reviewed EDS");

Status error(VelocityError detail, const char* message, Err code = Err::INVALID_CONFIG) {
    return Status(code, static_cast<int32_t>(detail), message);
}
uint64_t budget(const VelocityOptions& o, uint64_t count) {
    return count * (uint64_t(o.service.queueTimeoutUs) + o.service.responseTimeoutUs);
}
// The enable report window (S17): none, or a spacing of 1..window with a bounded cycle count.
bool windowValid(const VelocityOptions& o) {
    return !o.enableWindowUs
               ? !o.enableSpacingUs
               : o.enableSpacingUs && o.enableSpacingUs <= o.enableWindowUs &&
                     o.enableWindowUs <= VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US &&
                     o.enableWindowUs / o.enableSpacingUs <= VELOCITY_ENABLE_WINDOW_MAXIMUM_CYCLES;
}
// Extra final-enable cycles a valid window can schedule. Cycle j of the window is eligible no earlier
// than j spacings after word 15's acknowledgement and no later than the window, so j <= window/spacing.
uint32_t windowCycles(const VelocityOptions& o) {
    return o.enableWindowUs ? uint32_t(o.enableWindowUs / o.enableSpacingUs) : 0;
}
bool optionsValid(const VelocityOptions& o) {
    return windowValid(o) && o.service.queueTimeoutUs && o.service.responseTimeoutUs &&
           o.service.identityWidths <= IdentityWidths::MEASURED_U8 &&
           o.service.countWidths <= CountWidths::MEASURED_U16 && o.pollIntervalUs &&
           o.maxPolls && o.pollIntervalUs <= uint64_t(INT64_MAX) / o.maxPolls &&
           o.maximumAgeUs >= budget(o, CYCLE_FIELDS) && o.maximumAgeUs <= uint64_t(INT64_MAX) &&
           o.requiredStopBudgetUs >= budget(o, CYCLE_FIELDS + 1) &&
           o.requiredStopBudgetUs <= uint64_t(INT64_MAX) &&
           o.commandIntervalUs >= ENABLE_WORD_INTERVAL_US &&
           o.commandIntervalUs <= uint64_t(INT64_MAX) / 2 && o.stopKind <= StopKind::QUICK_STOP;
}
bool sameFirmware(const VelocityFirmware& f, const IdentityObservation& i) {
    return f.deviceType == i.deviceType && f.hardwareVersion == i.hardwareVersion &&
           f.softwareVersion == i.softwareVersion && f.vendorId == i.vendorId &&
           f.productCode == i.productCode && f.revision == i.revision &&
           f.modelCode == i.modelCode && f.firmwareCode == i.firmwareCode;
}
uint32_t raw(const ReadContext& c, StateBlock block, unsigned field) {
    return c.values[c.blockStart[unsigned(block)] + field].raw;
}
Status sample(const ReadContext& read, const AxisReservation& r, uint64_t now, uint64_t age,
              uint64_t after, VelocitySample& result) {
    ReadWindow windows[3];
    const StateBlock blocks[] = {StateBlock::MONITORS, StateBlock::STATUS, StateBlock::FEEDBACK};
    if (read.source != ReadSource::SDO || read.state != ReadState::SUCCEEDED || read.uncertain ||
        read.startedUs < r.acquiredUs)
        return error(VelocityError::STATE, "velocity needs a complete ordinary SDO state cycle");
    for (unsigned i = 0; i < 3; ++i)
        if (!validateStateBlock(read, blocks[i], r.target, now, age, windows[i]) ||
            windows[i].earliestUs < after || windows[i].earliestUs < r.acquiredUs)
            return error(VelocityError::STATE, "velocity state freshness is unavailable");
    VelocitySample s;
    s.present = true;
    s.monitor = uint16_t(raw(read, StateBlock::MONITORS, 0));
    s.statusword = uint16_t(raw(read, StateBlock::STATUS, 0));
    s.modeRaw = uint8_t(raw(read, StateBlock::STATUS, 1));
    s.position = signed32(raw(read, StateBlock::FEEDBACK, 0));
    s.velocity = signed32(raw(read, StateBlock::FEEDBACK, 1));
    s.earliestUs = windows[0].earliestUs;
    s.latestUs = windows[0].latestUs;
    for (unsigned i = 1; i < 3; ++i) {
        if (windows[i].earliestUs < s.earliestUs)
            s.earliestUs = windows[i].earliestUs;
        if (windows[i].latestUs > s.latestUs)
            s.latestUs = windows[i].latestUs;
    }
    result = s;
    return Ok();
}
bool clean(const VelocitySample& s, StatusLayout layout) {
    StatusIndications interpreted;
    return interpretStatusword(s.statusword, layout, interpreted) &&
           interpreted.alarm == StateIndication::NO &&
           !(s.monitor &
             uint16_t(MONITOR_ALARM | MONITOR_POSITIVE_SOFT_LIMIT | MONITOR_NEGATIVE_SOFT_LIMIT |
                      uint16_t(~MONITOR_DOCUMENTED_BITS)));
}
bool released(const VelocitySample& s) {
    const uint16_t low = uint16_t(s.statusword & STATUS_LOW_BITS);
    return (s.monitor & MONITOR_RELEASED) && !(s.monitor & MONITOR_RUNNING) && !s.velocity &&
           (low == 0 || low == 1 || low == 3);
}
// The release a moving quick stop leaves, with 2100 running still latched: after trials 1513 and 1607
// both specimens read 2100=0015, 6041=C050, mode 3, 606C 0 and 6064 still (reads 1514, 1608). Admitted
// only in mode 3 and only with low bits 000; word 6's cycle must then repeat the admission's 6064.
bool latchedRelease(const VelocitySample& s) {
    return (s.monitor & MONITOR_RELEASED) && (s.monitor & MONITOR_RUNNING) && !s.velocity &&
           !(s.statusword & STATUS_LOW_BITS);
}
// The report an ordinary mode 3 stop admission needs (validateStop): enabled, running, no alarm or
// release. Only a stop that followed such a report can show, by a later not-running report, that it
// acted; after any other report the drive was not reported running before the stop either.
bool runningReport(const VelocitySample& s, StatusLayout layout) {
    return s.present && s.modeRaw == 3 && (s.statusword & STATUS_LOW_BITS) == STATUS_LOW_BITS &&
           (s.monitor & MONITOR_RUNNING) && !(s.monitor & MONITOR_RELEASED) && clean(s, layout);
}
Status rateRange(const PreparedVelocityTarget& rate, const VelocityEvidence& e) {
    if (rate.rateBasis != NativeRateBasis::MOTOR_TURNS || rate.rateTime != TimeUnit::MINUTE ||
        rate.representation != NativeRepresentation::INTEGER)
        return error(VelocityError::RATE_BASIS, "60FF requires integer motor turns per minute");
    const int64_t lower = e.reverseQualified ? -int64_t(e.maximumReverseRpm) : 0;
    const int64_t upper = e.maximumForwardRpm;
    const int64_t value = rate.effectiveNative;
    if (!value || value < lower || value > upper)
        return error(VelocityError::RANGE, "zero or unqualified effective velocity",
                     Err::ILLEGAL_VALUE);
    if (rate.exactArithmetic) {
        const NativeQuantity& q = rate.requestedNative;
        const bool positive = q.integral > 0 || (!q.integral && q.numerator && !q.negative);
        const bool negative = q.integral < 0 || (!q.integral && q.numerator && q.negative);
        if ((!positive && !negative) || (positive != (value > 0)) || q.integral < lower ||
            q.integral > upper || (q.integral == lower && q.numerator && q.negative) ||
            (q.integral == upper && q.numerator && !q.negative))
            return error(VelocityError::RANGE,
                         "requested velocity exceeds the qualified signed range",
                         Err::ILLEGAL_VALUE);
    } else {
        const double limit = value > 0 ? double(value) : double(-value);
        if (!std::isfinite(rate.absoluteErrorBound) || rate.absoluteErrorBound < 0 ||
            rate.absoluteErrorBound >= limit || rate.absoluteErrorBound > double(value - lower) ||
            rate.absoluteErrorBound > double(upper - value))
            return error(VelocityError::RANGE,
                         "approximate velocity crosses zero or its qualified range",
                         Err::ILLEGAL_VALUE);
    }
    return Ok();
}
struct Checked {
    PreparedVelocityTarget rate;
    VelocitySample admission;
};
Status check(const VelocityContext& output, const AxisReservation& r, const ReadTarget& current,
             const ReadContext& state, const IdentityObservation& identity,
             const VelocityEvidence& e, const FiniteVelocityRequest& request,
             const VelocityProfile& profile, const AxisConfig& axis, uint32_t operation,
             uint64_t now, uint64_t deadline, const VelocityOptions& o, Checked& result) {
    const void* inputs[] = {&r, &current, &state, &identity, &e, &request, &profile, &axis, &o};
    const size_t sizes[] = {sizeof r,       sizeof current, sizeof state, sizeof identity, sizeof e,
                            sizeof request, sizeof profile, sizeof axis,  sizeof o};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
        if (overlaps(&output, sizeof output, inputs[i], sizes[i]))
            return error(VelocityError::OVERLAP, "velocity preparation overlaps input");
    // An attended experiment measures start and stop; it never carries a claim that they are
    // established, nor the separate mode-change and reverse facts.
    const bool experiment = e.gate == VelocityGate::ATTENDED_EXPERIMENT;
    if (e.gate == VelocityGate::CLOSED ||
        (e.gate == VelocityGate::MEASURED && (!e.startQualified || !e.stopQualified)))
        return error(VelocityError::CAPABILITY,
                     "mode3 start and stop need selected-firmware evidence", Err::UNSUPPORTED);
    if ((e.gate != VelocityGate::MEASURED && !experiment) || !e.record ||
        e.layout == StatusLayout::UNSPECIFIED || e.layout > StatusLayout::OP86 ||
        e.layout == StatusLayout::ZH57 || !e.maximumForwardRpm || e.maximumForwardRpm > 3000 ||
        (e.reverseQualified ? !e.maximumReverseRpm || e.maximumReverseRpm > 3000
                            : e.maximumReverseRpm != 0) ||
        e.stopKind != o.stopKind)
        return error(VelocityError::ARGUMENT, "invalid selected velocity evidence scope");
    if (experiment && (e.startQualified || e.stopQualified || e.modeChangeQualified ||
                       e.reverseQualified || e.maximumForwardRpm > VELOCITY_EXPERIMENT_MAXIMUM_RPM))
        return error(VelocityError::ARGUMENT,
                     "an attended velocity experiment claims no qualification and stays slow");
    if (experiment && request.durationUs > VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US)
        return error(VelocityError::TIME, "an attended velocity experiment is at most 20 s");
    // Outside the experiment a window would claim that the enable report arrives within it; no
    // record establishes that.
    if (o.enableWindowUs && !experiment)
        return error(VelocityError::ARGUMENT,
                     "an enable report window belongs to an attended velocity experiment");
    static_assert(VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US == 20000000, "the message names 20 s");
    if (!validReservation(r) || !operation || !optionsValid(o) || r.acquiredUs > now ||
        now > uint64_t(INT64_MAX) || deadline > uint64_t(INT64_MAX) || deadline <= now ||
        !request.durationUs || request.durationUs >= deadline - now ||
        request.durationUs > uint64_t(o.maxPolls) * o.pollIntervalUs)
        return error(VelocityError::TIME, "invalid velocity identity, duration, cutoff or budgets");
    const uint64_t due = now + request.durationUs;
    if (o.requiredStopBudgetUs >= deadline - due || o.pollIntervalUs > uint64_t(INT64_MAX) - due)
        return error(VelocityError::TIME, "velocity deadline lacks the fixed stop reserve");
    if (!sameTarget(r.target, current) || !sameBinding(axis.binding, r.target.channel.binding) ||
        axis.exhausted ||
        request.velocity.configurationGeneration != axis.binding.configurationGeneration)
        return error(VelocityError::TARGET, "velocity axis or rate generation is stale");
    if (e.model != r.model || !sameTarget(identity.target, r.target) ||
        !sameFirmware(e.firmware, identity) || identity.window.latestUs > now)
        return error(VelocityError::CAPABILITY,
                     "velocity evidence does not match selected model and firmware",
                     Err::UNSUPPORTED);
    if (!profile.rampUpTimeMs || profile.rampUpTimeMs > 2000 || !profile.rampDownTimeMs ||
        profile.rampDownTimeMs > 2000)
        return error(VelocityError::RANGE, "velocity ramp times must be 1..2000 ms",
                     Err::ILLEGAL_VALUE);
    Status s = sample(state, r, now, o.maximumAgeUs, r.acquiredUs, result.admission);
    if (!s)
        return s;
    if (!clean(result.admission, e.layout) ||
        (!released(result.admission) &&
         !(latchedRelease(result.admission) && result.admission.modeRaw == 3)))
        return error(VelocityError::STATE, "velocity requires a fresh released stationary start");
    if (result.admission.modeRaw != 0 && result.admission.modeRaw != 1 &&
        result.admission.modeRaw != 3 && result.admission.modeRaw != 6)
        return error(VelocityError::STATE, "velocity start mode is not reviewed");
    if (result.admission.modeRaw != 3 &&
        (!e.modeChangeQualified || e.modeChangeFromRaw != result.admission.modeRaw))
        return error(VelocityError::CAPABILITY,
                     "released mode change needs evidence for this exact source mode",
                     Err::UNSUPPORTED);
    // The window's last cycle is eligible at most enableWindowUs after word 15's acknowledgement and
    // has the first enable cycle's budget, so the window adds at most its own length.
    const uint64_t setup = budget(o, result.admission.modeRaw == 3 ? 27 : 35) + o.enableWindowUs;
    if (setup >= request.durationUs || o.commandIntervalUs > (request.durationUs - setup) / 2)
        return error(VelocityError::TIME,
                     "duration cannot cover target/setup and enable observation budgets");
    s = prepareVelocityTarget(request.velocity, axis, result.rate);
    if (!s) {
        s.detail += int32_t(VelocityError::RATE_ERROR_BASE);
        return s;
    }
    return rateRange(result.rate, e);
}
bool downloading(const VelocityContext& c) {
    return c.phase == VelocityPhase::MODE || c.phase == VelocityPhase::PARAMETERS ||
           c.phase == VelocityPhase::ENABLE;
}
Status shape(const VelocityContext& c, uint64_t now) {
    if (!validReservation(c.reservation) || !c.operation || !c.step || !optionsValid(c.options) ||
        c.state == ActionState::EMPTY || c.state > ActionState::FAILED ||
        c.phase > VelocityPhase::STOPPING || c.observation > VelocityObservation::RUNNING ||
        c.setupCursor > 2 || c.commandCursor > 2 || (c.setupEvidenceMask & ~7u) ||
        (c.commandEvidenceMask & ~7u) || c.withdrawal > VelocityInterruption::DEADLINE ||
        c.interruption > VelocityInterruption::DEADLINE ||
        (c.evidence.gate != VelocityGate::MEASURED &&
         c.evidence.gate != VelocityGate::ATTENDED_EXPERIMENT) ||
        (c.options.enableWindowUs && c.evidence.gate != VelocityGate::ATTENDED_EXPERIMENT) ||
        c.stopDueUs <= c.startedUs ||
        c.stopDueUs >= c.deadlineUs || c.deadlineUs > uint64_t(INT64_MAX) ||
        now > uint64_t(INT64_MAX) || c.servicedUs < c.startedUs || now < c.servicedUs ||
        c.runningPolls > c.options.maxPolls || c.runningPolls > c.observations ||
        c.observations - c.runningPolls >
            (c.admission.modeRaw == 3 ? 3u : 4u) + windowCycles(c.options) ||
        c.enablePending > (c.options.enableWindowUs ? windowCycles(c.options) + 1 : 0))
        return error(VelocityError::STATE, "invalid velocity context or processing time");
    return Ok();
}
Status currentContext(const VelocityContext& c, const AxisReservation& live,
                      const ReadTarget& target, uint64_t now) {
    const Status s = shape(c, now);
    if (!s)
        return s;
    if (!sameReservation(c.reservation, live) || !sameTarget(c.reservation.target, target))
        return error(VelocityError::TARGET, "velocity reservation or route changed");
    return Ok();
}
bool observationMatches(const VelocityContext& c, const ReadContext& read) {
    return read.operation == c.operation && read.kind == ReadKind::STATE &&
           read.source == ReadSource::SDO && sameTarget(read.target, c.reservation.target) &&
           read.blockMask == CYCLE_MASK && read.count == CYCLE_FIELDS &&
           read.deadlineUs == c.deadlineUs && read.startedUs == c.observationStartedUs &&
           read.options.queueTimeoutUs == c.options.service.queueTimeoutUs &&
           read.options.responseTimeoutUs == c.options.service.responseTimeoutUs;
}
CANOPEN::SdoTransfer download(const VelocityContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.step = c.step;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    // A queued target/enable must expire at the preparation-anchored stop boundary. The
    // enclosing operation keeps its later deadline exclusively for explicit stop settlement.
    t.deadlineUs = c.stopDueUs;
    t.queueTimeoutUs = c.options.service.queueTimeoutUs;
    t.responseTimeoutUs = c.options.service.responseTimeoutUs;
    t.index = OBJECT_6040_00.index;
    t.size = OBJECT_6040_00.widthBytes;
    uint32_t value = ENABLE_WORDS[c.commandCursor];
    if (c.phase == VelocityPhase::MODE) {
        t.index = 0x6060;
        t.size = 1;
        value = 3;
    } else if (c.phase == VelocityPhase::PARAMETERS) {
        t.index = PARAMETER_INDEXES[c.setupCursor];
        t.size = 4;
        value = c.setupCursor == 0   ? uint32_t(int32_t(c.rate.effectiveNative))
                : c.setupCursor == 1 ? c.profile.rampUpTimeMs
                                     : c.profile.rampDownTimeMs;
    }
    for (unsigned i = 0; i < t.size; ++i)
        t.data[i] = uint8_t(value >> (8 * i));
    return t;
}
ActionOutcome outcomeOf(ReadOutcome o) {
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
ActionOutcome outcomeOf(VelocityInterruption r) {
    switch (r) {
    case VelocityInterruption::DRIVE_FAULT:
        return ActionOutcome::DRIVE_FAULT;
    case VelocityInterruption::INVALIDATED:
        return ActionOutcome::INVALIDATED;
    case VelocityInterruption::DEADLINE:
        return ActionOutcome::DEADLINE;
    default:
        return ActionOutcome::CANCELLED;
    }
}
void settleEffects(VelocityContext& c) {
    bool possible = c.transmissionNoted, uncertain = c.transmissionNoted;
    bool enablingPossible = c.transmissionNoted && c.phase == VelocityPhase::ENABLE;
    if (c.modeEvidencePresent) {
        possible |= c.modeEvidence.effectPossible;
        uncertain |= c.modeEvidence.effectUncertain;
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (c.setupEvidenceMask & (1u << i)) {
            possible |= c.setupHistory[i].effectPossible;
            uncertain |= c.setupHistory[i].effectUncertain;
        }
        if (c.commandEvidenceMask & (1u << i)) {
            possible |= c.commandHistory[i].effectPossible;
            uncertain |= c.commandHistory[i].effectUncertain;
            enablingPossible |= c.commandHistory[i].transmissionPossible;
        }
    }
    // A controller-proved non-transmission may refine the first possible-TX notice. Earlier
    // possibly sent words stay in the history; only this operation's checked stop settles them.
    c.needsStop = enablingPossible && c.completion != ActionCompletion::OBSERVED;
    c.effectsPossible = possible;
    c.uncertain = uncertain || c.readUncertain || c.needsStop;
}
void failure(VelocityContext& c, ActionOutcome reason, uint64_t now, const char* message,
             bool allowStop) {
    settleEffects(c);
    if (c.firstFailure == ActionOutcome::NONE) {
        c.firstFailure = reason;
        c.status = error(
            reason == ActionOutcome::DEADLINE || reason == ActionOutcome::SERVICE_TIMEOUT
                ? VelocityError::TIME
                : VelocityError::STATE,
            message,
            reason == ActionOutcome::PROTOCOL_REJECTED ? Err::PROTOCOL_ERROR : Err::INVALID_CONFIG);
    }
    c.outcome = c.firstFailure;
    c.servicedUs = now;
    if (allowStop && c.needsStop && now < c.deadlineUs) {
        c.phase = VelocityPhase::STOPPING;
        c.stopRequested = true;
        c.stopRequestedUs = now;
    } else
        c.state = ActionState::FAILED;
    settleEffects(c);
}
void waitFor(VelocityContext& c, VelocityObservation purpose, uint64_t now, uint64_t delay) {
    c.observation = purpose;
    c.phase = VelocityPhase::WAITING;
    c.eligibleUs = delay >= c.stopDueUs - now ? c.stopDueUs : now + delay;
    c.stepStartedUs = c.eligibleUs;
}
// Word 7's report still showing after word 15 (trials 1009 and 1308: 6041 8033, 2100 0001), with no
// sign of torque release, running, speed or travel since word 7's cycle (word7Position). Word 7 itself
// may move 6064 before any observation: re-energizing pulled node 2's rotor from 8412 and from 8419 to
// 8424, the same in word 15's first cycle (trials 1742 and 1762). mode 3 and clean are checked first.
bool pendingReport(const VelocitySample& s, int32_t word7Position) {
    return (s.statusword & STATUS_LOW_BITS) == 3 &&
           !(s.monitor & (MONITOR_RELEASED | MONITOR_RUNNING)) && !s.velocity && s.position == word7Position;
}
// Another final-enable cycle fits the window: eligible one spacing from now, at most the window after
// word 15's acknowledgement, before stop due, within the window's cycle count.
bool windowAdmits(const VelocityContext& c, uint64_t now) {
    const uint64_t spacing = c.options.enableSpacingUs;
    return c.options.enableWindowUs && c.enablePending <= windowCycles(c.options) &&
           now < c.stopDueUs && spacing < c.stopDueUs - now &&
           now + spacing - c.commandHistory[2].completedUs <= c.options.enableWindowUs;
}
void finishCycle(VelocityContext& c, const ReadContext& read, uint64_t now) {
    const uint64_t after = c.observation == VelocityObservation::MODE
                               ? c.modeEvidence.completedUs
                               : c.commandHistory[c.commandCursor].completedUs;
    VelocitySample s;
    if (!sample(read, c.reservation, now, c.options.maximumAgeUs, after, s)) {
        failure(c, ActionOutcome::OBSERVATION_SCOPE, now, "velocity observation freshness failed",
                true);
        return;
    }
    if (c.observations == UINT32_MAX) {
        failure(c, ActionOutcome::ITERATION_EXHAUSTED, now,
                "velocity observation identity exhausted", true);
        return;
    }
    s.cycle = ++c.observations;
    // Mode and enable confirmations are immediate setup work. Only the cycles delayed by
    // pollIntervalUs consume the running budget used to validate duration coverage.
    if (c.observation == VelocityObservation::RUNNING)
        ++c.runningPolls;
    // Before word 15's first cycle replaces it, latest is word 7's cycle; every later window cycle equals
    // pendingFirst, which equalled it.
    const int32_t word7Position = c.enablePending ? c.pendingFirst.position : c.latest.position;
    c.latest = s;
    StatusIndications interpreted;
    interpretStatusword(s.statusword, c.evidence.layout, interpreted);
    if ((s.monitor & MONITOR_ALARM) || interpreted.alarm == StateIndication::YES) {
        failure(c, ActionOutcome::DRIVE_FAULT, now, "velocity observation reports an alarm", true);
        return;
    }
    if (!clean(s, c.evidence.layout) || s.modeRaw != 3) {
        failure(c, ActionOutcome::OBSERVATION_SCOPE, now,
                "velocity mode/alarm/monitor scope changed", true);
        return;
    }
    if (c.observation == VelocityObservation::MODE) {
        if (!released(s)) {
            failure(c, ActionOutcome::OBSERVATION_SCOPE, now,
                    "mode change did not preserve released standstill", false);
            return;
        }
        if (now < c.stopDueUs) {
            c.phase = VelocityPhase::PARAMETERS;
            c.eligibleUs = c.stepStartedUs = now;
        }
        return;
    }
    const uint16_t low = uint16_t(s.statusword & STATUS_LOW_BITS);
    if (c.commandCursor < 2) {
        const uint16_t expected = c.commandCursor == 0 ? 1 : 3;
        // Word 6 must freshly report release. Word 7's low011 example does not establish
        // a torque/release guarantee: accept either release bit, but still require no running
        // indication and zero feedback before the independently qualified final enable.
        // A running latch admitted with the release (latchedRelease) may remain in word 6's cycle, and
        // that cycle must repeat the admission's 6064: two fresh reports of one standstill before any
        // enabling word. From word 7's cycle on the latch must be clear; in mode 1 word 7 cleared it
        // (trial 308: 6041 0033, 2100 0000).
        const bool latchedStart = (c.admission.monitor & MONITOR_RUNNING) != 0;
        VelocitySample seen = s;
        if (c.commandCursor == 0 && latchedStart)
            seen.monitor = uint16_t(seen.monitor & ~MONITOR_RUNNING);
        const bool stationary = !(seen.monitor & MONITOR_RUNNING) && !s.velocity;
        if (!stationary || low != expected || (c.commandCursor == 0 && !released(seen)) ||
            (c.commandCursor == 0 && latchedStart && s.position != c.admission.position)) {
            failure(c, ActionOutcome::OBSERVATION_SCOPE, now, "enable prefix not freshly confirmed",
                    true);
            return;
        }
        if (now >= c.stopDueUs)
            return;
        const uint64_t acknowledged = c.commandHistory[c.commandCursor].completedUs;
        ++c.commandCursor;
        c.phase = VelocityPhase::ENABLE;
        c.eligibleUs = c.options.commandIntervalUs >= c.stopDueUs - acknowledged
                           ? c.stopDueUs
                           : acknowledged + c.options.commandIntervalUs;
        if (c.eligibleUs < now)
            c.eligibleUs = now;
        c.stepStartedUs = c.eligibleUs;
        return;
    }
    if (low != STATUS_LOW_BITS || (s.monitor & MONITOR_RELEASED)) {
        // Inside the enable report window word 7's report is not yet a failure (S17); every other
        // report, and the last one the window admits, fails as without a window.
        const bool pending = c.observation == VelocityObservation::ENABLE &&
                             c.options.enableWindowUs && pendingReport(s, word7Position);
        if (pending) {
            if (!c.enablePending)
                c.pendingFirst = s;
            c.pendingLast = s;
            ++c.enablePending;
            if (windowAdmits(c, now)) {
                waitFor(c, VelocityObservation::ENABLE, now, c.options.enableSpacingUs);
                return;
            }
        }
        failure(c, ActionOutcome::OBSERVATION_SCOPE, now,
                pending ? "final enable not reported within its window"
                        : "final enable state is not confirmed",
                true);
        return;
    }
    if (c.observation == VelocityObservation::ENABLE)
        c.enableReport = s;
    const bool moving = (s.monitor & MONITOR_RUNNING) &&
                        interpreted.running == StateIndication::YES &&
                        (c.rate.effectiveNative > 0 ? s.velocity > 0 : s.velocity < 0);
    if (moving)
        c.runningObserved = true;
    else if (c.runningObserved) {
        failure(c, ActionOutcome::OBSERVATION_SCOPE, now, "running velocity evidence contradicted",
                true);
        return;
    }
    if (now >= c.stopDueUs)
        return;
    if (c.runningPolls >= c.options.maxPolls) {
        failure(c, ActionOutcome::ITERATION_EXHAUSTED, now, "velocity observation bound reached",
                true);
        return;
    }
    waitFor(c, VelocityObservation::RUNNING, now, c.options.pollIntervalUs);
}
Status yield(const VelocityContext& c, const ReadContext& read, const AxisReservation& live,
             const ReadTarget& target, uint64_t now, PreparedVelocity& output, bool correlating) {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &read, sizeof read) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &target, sizeof target))
        return error(VelocityError::OVERLAP, "velocity work overlaps input");
    Status s = currentContext(c, live, target, now);
    if (!s)
        return s;
    PreparedVelocity next;
    next.eligibleUs = c.eligibleUs;
    if (c.state != ActionState::ACTIVE || c.phase == VelocityPhase::STOPPING ||
        now < c.eligibleUs ||
        (!correlating && (c.withdrawal != VelocityInterruption::NONE || now >= c.stopDueUs ||
                          (downloading(c) && c.transmissionNoted)))) {
        output = next;
        return Ok();
    }
    if (now >= c.deadlineUs)
        return error(VelocityError::TIME, "velocity deadline reached");
    if (!correlating && !exchangeFits(c.stopDueUs, now, c.options.service.queueTimeoutUs,
                                      c.options.service.responseTimeoutUs)) {
        output = next;
        return Ok();
    }
    if (downloading(c)) {
        next.transfer = download(c);
        next.ready = true;
    } else if (c.phase == VelocityPhase::WAITING)
        next.observe = true;
    else {
        if (!observationMatches(c, read))
            return error(VelocityError::CORRELATION, "read is not this velocity cycle");
        PreparedRead pending;
        s = nextRead(read, now, pending);
        if (!s)
            return s;
        if (!pending.ready)
            return error(VelocityError::STATE, "velocity cycle has no current service");
        next.ready = true;
        next.transfer = pending.transfer;
        next.transfer.step = c.step;
    }
    output = next;
    return Ok();
}
} // namespace

Status validateVelocity(const VelocityContext& output, const AxisReservation& r,
                        const ReadTarget& current, const ReadContext& state,
                        const IdentityObservation& identity, const VelocityEvidence& evidence,
                        const FiniteVelocityRequest& request, const VelocityProfile& profile,
                        const AxisConfig& axis, uint32_t operation, uint64_t now, uint64_t deadline,
                        const VelocityOptions& options) noexcept {
    Checked result;
    return check(output, r, current, state, identity, evidence, request, profile, axis, operation,
                 now, deadline, options, result);
}
Status prepareVelocity(VelocityContext& output, const AxisReservation& r, const ReadTarget& current,
                       const ReadContext& state, const IdentityObservation& identity,
                       const VelocityEvidence& evidence, const FiniteVelocityRequest& request,
                       const VelocityProfile& profile, const AxisConfig& axis, uint32_t operation,
                       uint64_t now, uint64_t deadline, const VelocityOptions& options) noexcept {
    Checked result;
    const Status s = check(output, r, current, state, identity, evidence, request, profile, axis,
                           operation, now, deadline, options, result);
    if (!s)
        return s;
    new (static_cast<void*>(&output)) VelocityContext();
    output.reservation = r;
    output.evidence = evidence;
    output.request = request;
    output.profile = profile;
    output.options = options;
    output.axis = axis;
    output.rate = result.rate;
    output.admission = result.admission;
    output.latest = result.admission;
    output.operation = operation;
    output.state = ActionState::ACTIVE;
    output.phase = result.admission.modeRaw == 3 ? VelocityPhase::PARAMETERS : VelocityPhase::MODE;
    output.startedUs = output.servicedUs = output.eligibleUs = output.stepStartedUs = now;
    output.stopDueUs = now + request.durationUs;
    output.deadlineUs = deadline;
    return Ok();
}
void resetVelocity(VelocityContext& c) noexcept {
    new (static_cast<void*>(&c)) VelocityContext();
}
Status nextVelocity(const VelocityContext& c, const ReadContext& read, const AxisReservation& live,
                    const ReadTarget& current, uint64_t now, PreparedVelocity& output) noexcept {
    return yield(c, read, live, current, now, output, false);
}
Status beginVelocityObservation(VelocityContext& c, ReadContext& read, const AxisReservation& live,
                                const ReadTarget& current, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &read, sizeof read) || overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &current, sizeof current) ||
        overlaps(&read, sizeof read, &live, sizeof live) ||
        overlaps(&read, sizeof read, &current, sizeof current))
        return error(VelocityError::OVERLAP, "velocity observation overlaps input");
    Status s = currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.phase != VelocityPhase::WAITING ||
        c.withdrawal != VelocityInterruption::NONE || now < c.eligibleUs || now >= c.stopDueUs ||
        (c.observation == VelocityObservation::RUNNING && c.runningPolls >= c.options.maxPolls))
        return error(VelocityError::STATE, "no eligible velocity observation");
    // Identity/count width selections apply to neither object in this state cycle. The read
    // preparation deliberately refuses those overrides for unrelated kinds, so pass its defaults.
    ReadOptions readOptions = c.options.service;
    readOptions.identityWidths = IdentityWidths::DECLARED_U16;
    readOptions.countWidths = CountWidths::DECLARED_U8;
    s = prepareState(read, c.reservation.target, c.operation, now, c.deadlineUs, readOptions,
                     CYCLE_MASK);
    if (!s)
        return s;
    c.phase = VelocityPhase::OBSERVING;
    c.observationStartedUs = c.stepStartedUs = c.servicedUs = now;
    return Ok();
}
Status validateVelocityAdmission(const VelocityContext& c, const AxisReservation& live,
                                 const ReadTarget& current, const AxisConfig& axis,
                                 uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &axis, sizeof axis))
        return error(VelocityError::OVERLAP, "velocity admission overlaps axis");
    const Status s = currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || !downloading(c) ||
        c.withdrawal != VelocityInterruption::NONE || now < c.eligibleUs || now >= c.stopDueUs)
        return error(VelocityError::STATE, "no eligible new velocity download");
    if (!exchangeFits(c.stopDueUs, now, c.options.service.queueTimeoutUs,
                      c.options.service.responseTimeoutUs))
        return error(VelocityError::TIME,
                     "velocity download budget no longer fits before stop due");
    if (axis.exhausted || !sameBinding(axis.binding, c.rate.binding) ||
        c.rate.requested.configurationGeneration != axis.binding.configurationGeneration ||
        axis.nativeRateBasis != c.rate.rateBasis || axis.nativeVelocityTime != c.rate.rateTime ||
        axis.velocityRepresentation != c.rate.representation ||
        axis.referenceGeneration != c.axis.referenceGeneration)
        return error(VelocityError::TARGET, "velocity conversion or generation changed");
    if (now - c.latest.earliestUs > c.options.maximumAgeUs)
        return error(VelocityError::STATE, "velocity prerequisite observation expired");
    return Ok();
}
Status noteVelocityTransmission(VelocityContext& c, const AxisReservation& live,
                                const ReadTarget& current, const AxisConfig& axis,
                                DriverContext* const* retained, uint8_t count,
                                uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &current, sizeof current))
        return error(VelocityError::OVERLAP, "velocity transmission overlaps input");
    Status s = validateVelocityAdmission(c, live, current, axis, now);
    if (!s)
        return s;
    if (c.transmissionNoted)
        return error(VelocityError::STATE, "velocity download transmission was already noted");
    if (count && !retained)
        return error(VelocityError::ARGUMENT, "retained settings list missing");
    for (unsigned i = 0; i < count; ++i) {
        if (!retained[i] || overlaps(retained[i], sizeof *retained[i], &c, sizeof c) ||
            overlaps(retained[i], sizeof *retained[i], &live, sizeof live) ||
            overlaps(retained[i], sizeof *retained[i], &current, sizeof current) ||
            overlaps(retained[i], sizeof *retained[i], &axis, sizeof axis))
            return error(VelocityError::OVERLAP, "retained settings missing or overlapping");
        const ReadContext& r = retained[i]->read;
        if (c.phase == VelocityPhase::PARAMETERS && r.state != ReadState::EMPTY &&
            r.kind == ReadKind::DRIVER_MOTION_PARAMETERS) {
            Binding a = r.target.channel.binding, b = current.channel.binding;
            a.bindingGeneration = b.bindingGeneration;
            a.configurationGeneration = b.configurationGeneration;
            if (!sameBinding(a, b) ||
                r.target.channel.clientToServerId != current.channel.clientToServerId ||
                r.target.channel.serverToClientId != current.channel.serverToClientId)
                return error(VelocityError::TARGET,
                             "retained motion parameters belong to another axis");
        }
    }
    if (c.phase == VelocityPhase::PARAMETERS) {
        const CANOPEN::SdoTransfer work = download(c);
        for (unsigned i = 0; i < count; ++i) {
            s = retireMotionParameters(*retained[i], current, work);
            if (!s)
                return s;
        }
    }
    c.transmissionNoted = c.effectsPossible = c.uncertain = true;
    if (c.phase == VelocityPhase::ENABLE) {
        c.needsStop = true;
        if (c.commandCursor == 2)
            c.execution = ActionExecution::UNKNOWN;
    } else
        c.setup = ActionExecution::UNKNOWN;
    c.servicedUs = now;
    return Ok();
}
Status noteVelocityWithdrawal(VelocityContext& c, const AxisReservation& live,
                              const ReadTarget& current, VelocityInterruption reason,
                              uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &current, sizeof current))
        return error(VelocityError::OVERLAP, "velocity withdrawal overlaps input");
    const Status s = reason == VelocityInterruption::INVALIDATED
                         ? shape(c, now)
                         : currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.withdrawal != VelocityInterruption::NONE ||
        reason == VelocityInterruption::NONE || reason >= VelocityInterruption::DEADLINE)
        return error(VelocityError::STATE, "invalid velocity withdrawal");
    c.withdrawal = now >= c.deadlineUs ? VelocityInterruption::DEADLINE : reason;
    return Ok();
}
Status advanceVelocity(VelocityContext& c, ReadContext& read, const AxisReservation& live,
                       const ReadTarget& current, const CANOPEN::SdoCompletion& input,
                       uint64_t now) noexcept {
    const void* inputs[] = {&live, &current, &input};
    const size_t sizes[] = {sizeof live, sizeof current, sizeof input};
    for (unsigned i = 0; i < 3; ++i)
        if (overlaps(&c, sizeof c, inputs[i], sizes[i]) ||
            overlaps(&read, sizeof read, inputs[i], sizes[i]))
            return error(VelocityError::OVERLAP, "velocity terminal overlaps input");
    if (overlaps(&c, sizeof c, &read, sizeof read))
        return error(VelocityError::OVERLAP, "velocity terminal overlaps read");
    Status s = currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.phase == VelocityPhase::STOPPING)
        return error(VelocityError::STATE, "velocity has no current service");
    PreparedVelocity work;
    s = yield(c, read, live, current, c.servicedUs < c.eligibleUs ? c.eligibleUs : c.servicedUs,
              work, true);
    if (!s)
        return s;
    if (!work.ready || !CANOPEN::sameSdoWork(input.transfer, work.transfer) ||
        input.nodeGeneration != c.reservation.target.nodeGeneration ||
        input.admittedUs < c.stepStartedUs || input.admittedUs >= c.stopDueUs ||
        input.completedUs < input.admittedUs || input.completedUs < c.servicedUs ||
        input.completedUs > now ||
        (input.outcome == CANOPEN::SdoOutcome::SUCCESS &&
         input.completedUs >= work.transfer.deadlineUs))
        return error(VelocityError::CORRELATION,
                     "velocity terminal does not match current service");
    const bool expired = now >= c.deadlineUs || input.completedUs >= c.deadlineUs;
    const bool withdrawn = c.withdrawal != VelocityInterruption::NONE;
    if (c.phase == VelocityPhase::OBSERVING) {
        CANOPEN::SdoCompletion local = input;
        local.transfer.step = uint32_t(read.cursor) + 1;
        s = advanceRead(read, local, now);
        if (!s)
            return s;
        c.readUncertain |= read.uncertain;
        c.observationEvidence = input;
        c.observationEvidencePresent = true;
        if (!withdrawn && read.state == ReadState::FAILED)
            failure(c, outcomeOf(read.outcome), now, "velocity observation failed", !expired);
        else if (!withdrawn && !expired) {
            if (c.step != UINT32_MAX)
                ++c.step;
            c.stepStartedUs = now;
            // Due is the stop scheduling boundary, not the overall evidence deadline. Keep
            // fresh running/fault reports here; finishCycle never advances a write after due.
            if (read.state == ReadState::SUCCEEDED)
                finishCycle(c, read, now);
        }
    } else {
        if (!CANOPEN::checkDownloadTerminal(input, c.transmissionNoted))
            return error(VelocityError::CORRELATION, "inconsistent velocity download terminal");
        const bool success = input.outcome == CANOPEN::SdoOutcome::SUCCESS;
        const ActionExecution execution =
            success                                              ? ActionExecution::ACKNOWLEDGED
            : input.outcome == CANOPEN::SdoOutcome::SERVER_ABORT ? ActionExecution::REJECTED
            : input.transmissionPossible                         ? ActionExecution::UNKNOWN
                                                                 : ActionExecution::NOT_TRANSMITTED;
        if (c.phase == VelocityPhase::MODE) {
            c.modeEvidence = input;
            c.modeEvidencePresent = true;
            c.setup = success ? ActionExecution::UNKNOWN : execution;
        } else if (c.phase == VelocityPhase::PARAMETERS) {
            c.setupHistory[c.setupCursor] = input;
            c.setupEvidenceMask = uint8_t(c.setupEvidenceMask | (1u << c.setupCursor));
            c.setup = success && c.setupCursor == 2 ? ActionExecution::ACKNOWLEDGED
                                                    : ActionExecution::UNKNOWN;
        } else {
            c.commandHistory[c.commandCursor] = input;
            c.commandEvidenceMask = uint8_t(c.commandEvidenceMask | (1u << c.commandCursor));
            c.needsStop |= input.transmissionPossible;
            if (success)
                c.control = input;
            if (c.commandCursor == 2)
                c.execution = execution;
        }
        c.transmissionNoted = false;
        settleEffects(c);
        if (!withdrawn && !expired && !success)
            failure(c, outcomeOf(readOutcome(input.outcome)), now,
                    "velocity download failed; never retried", true);
        else if (!withdrawn && !expired && now < c.stopDueUs && c.step != UINT32_MAX) {
            ++c.step;
            c.eligibleUs = c.stepStartedUs = now;
            if (c.phase == VelocityPhase::MODE)
                waitFor(c, VelocityObservation::MODE, now, 0);
            else if (c.phase == VelocityPhase::ENABLE)
                waitFor(c, VelocityObservation::ENABLE, now, 0);
            else if (c.setupCursor < 2)
                ++c.setupCursor;
            else
                c.phase = VelocityPhase::ENABLE;
        }
    }
    if (expired) {
        c.interruption = VelocityInterruption::DEADLINE;
        failure(c, ActionOutcome::DEADLINE, now, "velocity processing deadline reached", false);
    } else if (withdrawn) {
        c.interruption = c.withdrawal;
        failure(c, outcomeOf(c.withdrawal), now, "velocity withdrawn; service evidence retained",
                false);
    } else if (c.state == ActionState::ACTIVE && c.phase != VelocityPhase::STOPPING &&
               c.step == UINT32_MAX && now < c.stopDueUs)
        failure(c, ActionOutcome::ITERATION_EXHAUSTED, now, "velocity service identity exhausted",
                true);
    c.servicedUs = now;
    settleEffects(c);
    return Ok();
}
Status advanceVelocity(VelocityContext& c, ReadContext& read, const AxisReservation& live,
                       const ReadTarget& current, VelocityInterruption reason,
                       uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &read, sizeof read) || overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &current, sizeof current) ||
        overlaps(&read, sizeof read, &live, sizeof live) ||
        overlaps(&read, sizeof read, &current, sizeof current))
        return error(VelocityError::OVERLAP, "velocity interruption overlaps input");
    Status s = reason == VelocityInterruption::INVALIDATED ? shape(c, now)
                                                           : currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || reason == VelocityInterruption::NONE ||
        reason > VelocityInterruption::DEADLINE ||
        (reason == VelocityInterruption::DEADLINE && now < c.deadlineUs))
        return error(VelocityError::STATE, "invalid local velocity interruption");
    if (c.phase == VelocityPhase::OBSERVING && observationMatches(c, read) &&
        read.state == ReadState::ACTIVE) {
        const ReadOutcome r = reason == VelocityInterruption::INVALIDATED ? ReadOutcome::INVALIDATED
                              : now >= c.deadlineUs                       ? ReadOutcome::DEADLINE
                                                                          : ReadOutcome::CANCELLED;
        s = advanceRead(read, r, now);
        if (!s)
            return s;
        c.readUncertain |= read.uncertain;
    } else if (c.phase == VelocityPhase::OBSERVING && reason != VelocityInterruption::INVALIDATED &&
               !observationMatches(c, read))
        return error(VelocityError::CORRELATION, "velocity interrupted with another read context");
    if (now >= c.deadlineUs)
        reason = VelocityInterruption::DEADLINE;
    c.interruption = reason;
    failure(c, outcomeOf(reason), now, "velocity ended locally; no implicit stop", false);
    return Ok();
}
Status serviceVelocity(VelocityContext& c, const AxisReservation& live, const ReadTarget& current,
                       uint64_t now, bool admitted, bool& stopDue) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &current, sizeof current) ||
        overlaps(&stopDue, sizeof stopDue, &c, sizeof c) ||
        overlaps(&stopDue, sizeof stopDue, &live, sizeof live) ||
        overlaps(&stopDue, sizeof stopDue, &current, sizeof current))
        return error(VelocityError::OVERLAP, "velocity service overlaps input");
    Status s = currentContext(c, live, current, now);
    if (!s)
        return s;
    if (admitted || c.transmissionNoted)
        return error(VelocityError::STATE, "velocity service requires no admitted transaction");
    if (c.state != ActionState::ACTIVE) {
        stopDue = false;
        return Ok();
    }
    if (c.withdrawal != VelocityInterruption::NONE)
        return error(VelocityError::STATE, "withdrawn velocity needs its local terminal");
    if (now >= c.deadlineUs) {
        c.interruption = VelocityInterruption::DEADLINE;
        failure(c, ActionOutcome::DEADLINE, now, "velocity processing deadline reached", false);
        stopDue = false;
        return Ok();
    }
    if (c.phase != VelocityPhase::STOPPING && now >= c.stopDueUs) {
        if (!c.runningObserved || now - c.stopDueUs > c.options.pollIntervalUs)
            failure(c, ActionOutcome::SERVICE_TIMEOUT, now,
                    "velocity setup or service missed its preparation-time duration", true);
        else {
            c.phase = VelocityPhase::STOPPING;
            c.stopRequested = true;
            c.stopRequestedUs = now;
        }
    }
    c.servicedUs = now;
    stopDue =
        c.state == ActionState::ACTIVE && c.phase == VelocityPhase::STOPPING && c.stopRequested;
    return Ok();
}
namespace {
Status stopShape(const VelocityContext& c, uint64_t now) {
    const Status s = shape(c, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.phase != VelocityPhase::STOPPING || !c.stopRequested ||
        c.stopOperation)
        return error(VelocityError::STATE, "velocity has no outstanding composed stop");
    return Ok();
}
void finishStop(VelocityContext& c, ActionOutcome outcome, ActionExecution execution, bool observed,
                uint32_t operation, const CANOPEN::SdoCompletion& terminal, uint64_t now) {
    c.stopOperation = operation;
    c.stopOutcome = outcome;
    c.stopExecution = execution;
    c.stopEvidence = terminal;
    if (now >= c.deadlineUs) {
        failure(c, ActionOutcome::DEADLINE, now, "velocity stop processed after deadline", false);
        return;
    }
    const bool settles = observed && runningReport(c.latest, c.evidence.layout);
    if (settles) {
        c.needsStop = false;
        c.completion = ActionCompletion::OBSERVED;
    }
    if (c.firstFailure != ActionOutcome::NONE)
        c.state = ActionState::FAILED;
    else if (settles) {
        c.state = ActionState::SUCCEEDED;
        c.outcome = ActionOutcome::OBSERVED;
        c.status = Ok();
    } else
        failure(c, observed ? ActionOutcome::OBSERVATION_SCOPE : outcome, now,
                observed ? "velocity stop observed without a running report before it"
                         : "finite velocity stop did not establish its controller observation",
                false);
    c.servicedUs = now;
    settleEffects(c);
}
bool sameTerminal(const CANOPEN::SdoCompletion& a, const CANOPEN::SdoCompletion& b) {
    return CANOPEN::sameSdoWork(a.transfer, b.transfer) && a.outcome == b.outcome &&
           a.nodeGeneration == b.nodeGeneration && a.admittedUs == b.admittedUs &&
           a.completedUs == b.completedUs;
}
// The run's own requested stop, not yet composed: its kind and deadline are the run's.
bool ownStopRequested(const VelocityContext& run) {
    return run.state == ActionState::ACTIVE && run.phase == VelocityPhase::STOPPING && run.stopRequested &&
           !run.stopOperation && run.withdrawal == VelocityInterruption::NONE;
}
// Backlog S18: the run's own QUICK_STOP after a running report reads FEEDBACK from its first cycle and
// may settle on a released standstill, as prepareStop's mode 3 admission does, when the age covers seven
// fields. Trials 1513 (node 2, 30 rpm) and 1607 (node 1, 60 rpm): 28 ms after 000B, 2100=0014 and
// 6041=C050 in mode 3, and later 0015 with 606C 0 and 6064 still. A stop on the 000F alone, or a later
// explicit stop of an ended run, keeps the release outside its scope.
bool coversCycle(const ActionOptions& o) {
    return o.maximumAgeUs >= CYCLE_FIELDS * (uint64_t(o.service.queueTimeoutUs) + o.service.responseTimeoutUs);
}
bool releaseStandstill(const VelocityContext& run, StopKind kind, const ActionOptions& o) {
    return kind == StopKind::QUICK_STOP && ownStopRequested(run) &&
           runningReport(run.latest, run.evidence.layout) && coversCycle(o);
}
// The halted report a settled velocity HALT left on both specimens (trials 1507, 1613, 1706).
constexpr uint16_t HALTED_STATUSWORD = 0x9437, HALTED_MONITOR = MONITOR_POSITION_COMPLETE;
Status haltedReport(const ReadContext& state, const ReadTarget& target, uint64_t after, uint64_t now,
                    uint64_t age) {
    StateObservation status, monitors, feedback;
    const bool withFeedback = (state.blockMask & (1u << unsigned(StateBlock::FEEDBACK))) != 0;
    if (state.source != ReadSource::SDO || state.state != ReadState::SUCCEEDED || state.uncertain ||
        !getStateBlock(state, StateBlock::STATUS, target, now, age, status) ||
        !getStateBlock(state, StateBlock::MONITORS, target, now, age, monitors) ||
        status.window.earliestUs < after || monitors.window.earliestUs < after ||
        status.statusword != HALTED_STATUSWORD || status.modeRaw != 3 || monitors.monitor != HALTED_MONITOR ||
        (withFeedback && (!getStateBlock(state, StateBlock::FEEDBACK, target, now, age, feedback) ||
                          feedback.window.earliestUs < after || feedback.actualVelocity != 0)))
        return error(VelocityError::STATE, "needs a fresh report of exactly 6041 9437, 2100 0001, mode 3");
    return Ok();
}
Status checkHaltedStop(const ActionContext& output, const VelocityContext& run, const AxisReservation& live,
                       const ReadTarget& current, const ReadContext& state,
                       const CANOPEN::SdoCompletion& control, uint32_t operation, uint64_t now,
                       uint64_t deadline, const ActionOptions& options) {
    const void* inputs[] = {&run, &live, &current, &state, &control, &options};
    const size_t sizes[] = {sizeof run, sizeof live, sizeof current, sizeof state, sizeof control, sizeof options};
    for (unsigned i = 0; i < 6; ++i)
        if (overlaps(&output, sizeof output, inputs[i], sizes[i]))
            return error(VelocityError::OVERLAP, "halted velocity stop preparation overlaps input");
    Status s = currentContext(run, live, current, now);
    if (!s)
        return s;
    if (operation <= run.operation || operation <= run.stopOperation || deadline <= now ||
        deadline > uint64_t(INT64_MAX) || !stopOptionsValid(options) || !coversCycle(options))
        return error(VelocityError::ARGUMENT, "invalid halted velocity stop identity or budgets");
    if (run.evidence.gate != VelocityGate::ATTENDED_EXPERIMENT || run.state != ActionState::SUCCEEDED ||
        run.options.stopKind != StopKind::HALT || !run.stopOperation ||
        run.stopExecution != ActionExecution::ACKNOWLEDGED || run.completion != ActionCompletion::OBSERVED)
        return error(VelocityError::STATE, "only after an attended velocity run settled by its own HALT");
    if (!acknowledgedControlWrite(control, run.reservation, now) || controlWord(control.transfer) != 0x010f ||
        control.transfer.operation != run.stopOperation || !sameTerminal(control, run.stopEvidence))
        return error(VelocityError::STATE, "the latest word must be that run's own acknowledged 010F");
    if (state.startedUs < live.acquiredUs)
        return error(VelocityError::STATE, "the halted report predates this reservation");
    return haltedReport(state, live.target, control.completedUs, now, options.maximumAgeUs);
}
Status checkRunStop(const ActionContext& output, const VelocityContext& run,
                    const AxisReservation& live, const ReadTarget& current,
                    const CANOPEN::SdoCompletion& control, StopKind kind, uint32_t operation,
                    uint64_t now, uint64_t deadline, const ActionOptions& options) {
    const void* inputs[] = {&run, &live, &current, &control, &options};
    const size_t sizes[] = {sizeof run, sizeof live, sizeof current, sizeof control, sizeof options};
    for (unsigned i = 0; i < 5; ++i)
        if (overlaps(&output, sizeof output, inputs[i], sizes[i]))
            return error(VelocityError::OVERLAP, "velocity stop preparation overlaps input");
    Status s = currentContext(run, live, current, now);
    if (!s)
        return s;
    if (kind > StopKind::QUICK_STOP || operation <= run.operation || deadline <= now ||
        deadline > uint64_t(INT64_MAX) || !stopOptionsValid(options))
        return error(VelocityError::ARGUMENT, "invalid velocity stop selection or budgets");
    // The run's own requested stop keeps its kind and fixed deadline; a later explicit stop of an
    // ended or withdrawn run is the application's own.
    const bool requested = ownStopRequested(run);
    const bool ended = run.state == ActionState::FAILED ||
                       (run.state == ActionState::ACTIVE && run.withdrawal != VelocityInterruption::NONE);
    if ((!requested && !ended) ||
        (requested && (kind != run.options.stopKind || deadline != run.deadlineUs ||
                       now < run.stopRequestedUs)))
        return error(VelocityError::STATE, "no velocity stop is outstanding for this run");
    // Only after this run's acknowledged 000F, still the latest word: from 6 or 7, 010F would be the
    // first enable-operation word, and no source gives a stop arrow from them.
    const CANOPEN::SdoCompletion& last = run.commandHistory[2];
    if (!run.needsStop || !(run.commandEvidenceMask & 4u) ||
        last.outcome != CANOPEN::SdoOutcome::SUCCESS || controlWord(last.transfer) != 0x000f ||
        !sameTerminal(control, last) || !acknowledgedControlWrite(control, run.reservation, now))
        return error(VelocityError::STATE,
                     "a velocity stop follows only this run's acknowledged 000F, still the latest word");
    return Ok();
}
} // namespace
Status advanceVelocityStop(VelocityContext& c, const ActionContext& stop, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &stop, sizeof stop))
        return error(VelocityError::OVERLAP, "velocity stop result overlaps context");
    Status s = stopShape(c, now);
    if (!s)
        return s;
    if (!sameReservation(c.reservation, stop.reservation) || stop.actionKind != ActionKind::STOP ||
        stop.kind != c.options.stopKind || stop.stopModeRaw != 3 || stop.operation <= c.operation ||
        stop.deadlineUs != c.deadlineUs || stop.startedUs < c.stopRequestedUs ||
        stop.startedUs >= c.deadlineUs || stop.servicedUs > now ||
        stop.servicedUs < stop.startedUs ||
        (stop.state != ActionState::FAILED && stop.state != ActionState::SUCCEEDED))
        return error(VelocityError::CORRELATION,
                     "stop result does not belong to this finite velocity");
    const bool observed = stop.state == ActionState::SUCCEEDED &&
                          stop.outcome == ActionOutcome::OBSERVED &&
                          stop.completion == ActionCompletion::OBSERVED &&
                          stop.execution == ActionExecution::ACKNOWLEDGED;
    if (observed && (!(stop.commandEvidenceMask & 1) ||
                     !CANOPEN::checkDownloadTerminal(stop.commandHistory[0], true)))
        return error(VelocityError::CORRELATION,
                     "successful stop lacks its actual command terminal");
    finishStop(c, stop.outcome, stop.execution, observed, stop.operation, stop.commandHistory[0],
               now);
    return Ok();
}
Status advanceVelocityStop(VelocityContext& c, const BestEffortStop& stop, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &stop, sizeof stop))
        return error(VelocityError::OVERLAP, "best-effort stop result overlaps velocity");
    Status s = stopShape(c, now);
    if (!s)
        return s;
    if (c.options.stopKind != StopKind::QUICK_STOP ||
        !sameTarget(c.reservation.target, stop.target) || stop.operation <= c.operation ||
        stop.deadlineUs != c.deadlineUs || stop.startedUs < c.stopRequestedUs ||
        stop.startedUs >= c.deadlineUs || stop.servicedUs > now ||
        stop.servicedUs < stop.startedUs || stop.state != ActionState::FAILED)
        return error(VelocityError::CORRELATION,
                     "best-effort stop does not belong to this velocity");
    finishStop(c, stop.outcome, stop.execution, false, stop.operation, stop.commandEvidence, now);
    return Ok();
}
Status advanceVelocityStop(VelocityContext& c, ActionOutcome reason, uint64_t now) noexcept {
    const Status s = stopShape(c, now);
    if (!s)
        return s;
    if ((reason != ActionOutcome::INVALIDATED && reason != ActionOutcome::DEADLINE) ||
        (reason == ActionOutcome::DEADLINE && now < c.deadlineUs))
        return error(VelocityError::STATE, "invalid reason for an uncomposed velocity stop");
    if (now >= c.deadlineUs)
        reason = ActionOutcome::DEADLINE;
    c.stopOutcome = reason;
    failure(c, reason, now, "the run's requested stop could not be composed; none of it was sent",
            false);
    return Ok();
}
Status validateVelocityStop(const ActionContext& output, const VelocityContext& run,
                            const AxisReservation& live, const ReadTarget& current,
                            const CANOPEN::SdoCompletion& control, StopKind kind,
                            uint32_t operation, uint64_t now, uint64_t deadline,
                            const ActionOptions& options) noexcept {
    return checkRunStop(output, run, live, current, control, kind, operation, now, deadline,
                        options);
}
Status prepareVelocityStop(ActionContext& output, const VelocityContext& run,
                           const AxisReservation& live, const ReadTarget& current,
                           const CANOPEN::SdoCompletion& control, StopKind kind, uint32_t operation,
                           uint64_t now, uint64_t deadline, const ActionOptions& options) noexcept {
    const Status s =
        checkRunStop(output, run, live, current, control, kind, operation, now, deadline, options);
    if (!s)
        return s;
    // prepareStop's mode 3 context, with no admission observation: the first cycle fills both.
    new (static_cast<void*>(&output)) ActionContext();
    output.reservation = run.reservation;
    output.kind = kind;
    output.stopModeRaw = 3;
    output.predecessorWord = 0x000f;
    output.predecessorBasis = PredecessorBasis::PROJECT_DECISION;
    output.options = options;
    output.operation = operation;
    output.startedUs = output.servicedUs = output.eligibleUs = output.stepStartedUs = now;
    output.deadlineUs = deadline;
    output.state = ActionState::ACTIVE;
    output.controlEvidence = control;
    output.standstillObserving = releaseStandstill(run, kind, options);
    return Ok();
}
Status validateHaltedVelocityReport(const ReadContext& state, const ReadTarget& current, uint64_t after,
                                    uint64_t now, uint64_t maximumAgeUs) noexcept {
    return haltedReport(state, current, after, now, maximumAgeUs);
}
Status validateHaltedVelocityQuickStop(const ActionContext& output, const VelocityContext& run,
                                       const AxisReservation& live, const ReadTarget& current,
                                       const ReadContext& state, const CANOPEN::SdoCompletion& control,
                                       uint32_t operation, uint64_t now, uint64_t deadline,
                                       const ActionOptions& options) noexcept {
    return checkHaltedStop(output, run, live, current, state, control, operation, now, deadline, options);
}
Status prepareHaltedVelocityQuickStop(ActionContext& output, const VelocityContext& run,
                                      const AxisReservation& live, const ReadTarget& current,
                                      const ReadContext& state, const CANOPEN::SdoCompletion& control,
                                      uint32_t operation, uint64_t now, uint64_t deadline,
                                      const ActionOptions& options) noexcept {
    const Status s = checkHaltedStop(output, run, live, current, state, control, operation, now, deadline, options);
    if (!s)
        return s;
    new (static_cast<void*>(&output)) ActionContext();
    output.reservation = run.reservation;
    output.kind = StopKind::QUICK_STOP;
    output.stopModeRaw = 3;
    output.predecessorWord = 0x010f;
    output.predecessorBasis = PredecessorBasis::PROJECT_DECISION;
    output.options = options;
    output.operation = operation;
    output.startedUs = output.servicedUs = output.eligibleUs = output.stepStartedUs = now;
    output.deadlineUs = deadline;
    output.state = ActionState::ACTIVE;
    output.controlEvidence = control;
    // Only a released standstill completes it (Actions.cpp): the drive reported idle before 000B.
    output.standstillObserving = true;
    getStateBlock(state, StateBlock::STATUS, live.target, now, options.maximumAgeUs, output.statusObservation);
    getStateBlock(state, StateBlock::MONITORS, live.target, now, options.maximumAgeUs, output.monitorObservation);
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
