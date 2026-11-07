// SPDX-License-Identifier: MIT
#include "ReadPlan.h"
#include <MotorControlCAN/profiles/lichuan_c/Homing.h>
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
// EN86/ZH86 PDF p15, homing table, not the generic CiA402 homing-attained bit.
constexpr uint16_t HOME_COMPLETE = 0x0100;
constexpr uint16_t INDEXES[5] = {0x6098, 0x6099, 0x6099, 0x609a, 0x607c};
constexpr uint8_t SUBINDEXES[5] = {0, 1, 2, 0, 0};
// 6098 has no generated write descriptor because signedness conflicts; its agreed wire width is
// one byte (EDS [6098], EN86/ZH86 p13), used only for the four positive methods selected here.
static_assert(OBJECT_6099_01.widthBytes == 4 && OBJECT_6099_02.widthBytes == 4 &&
                  OBJECT_609A_00.widthBytes == 4 && OBJECT_607C_00.widthBytes == 4 &&
                  OBJECT_6040_00.widthBytes == 2,
              "homing widths must remain those in the reviewed EDS");
Status error(HomeError detail, const char* message, Err code = Err::INVALID_CONFIG) {
    return Status(code, int32_t(detail), message);
}
uint8_t functionFor(HomingMethod method) {
    switch (method) {
    case HomingMethod::NEGATIVE_LIMIT:
        return 3;
    case HomingMethod::POSITIVE_LIMIT:
        return 2;
    case HomingMethod::POSITIVE_ORIGIN:
    case HomingMethod::NEGATIVE_ORIGIN:
        return 1;
    default:
        return 0;
    }
}
uint8_t requiredInputs(const HomeEvidence& e) {
    uint8_t result = 0;
    for (unsigned i = 0; i < 7; ++i)
        if (e.inputFunctions[i] == functionFor(e.method))
            result = uint8_t(result | (1u << i));
    return result;
}
bool validOptions(const HomeOptions& o) {
    return o.service.queueTimeoutUs && o.service.responseTimeoutUs && o.pollIntervalUs &&
           o.maxPolls >= 2 && o.maximumAgeUs && o.maximumAgeUs <= uint64_t(INT64_MAX) &&
           o.pollIntervalUs <= uint64_t(INT64_MAX) &&
           o.service.identityWidths == IdentityWidths::DECLARED_U16 &&
           o.service.countWidths == CountWidths::DECLARED_U8;
}
ReadTarget expectedTarget(const HomeContext& c) {
    ReadTarget t = c.reservation.target;
    if (c.coordinatesInvalidated)
        ++t.channel.binding.configurationGeneration;
    return t;
}
bool sameFirmware(const HomeFirmware& f, const IdentityObservation& i) {
    return f.deviceType == i.deviceType && f.hardwareVersion == i.hardwareVersion &&
           f.softwareVersion == i.softwareVersion && f.vendorId == i.vendorId &&
           f.productCode == i.productCode && f.revision == i.revision &&
           f.modelCode == i.modelCode && f.firmwareCode == i.firmwareCode;
}
uint32_t raw(const ReadContext& c, StateBlock b, unsigned field) {
    return c.values[c.blockStart[unsigned(b)] + field].raw;
}
Status sample(const ReadContext& read, const ReadTarget& target, uint64_t now, uint64_t age,
              uint64_t after, HomeSample& output) {
    if (read.source != ReadSource::SDO || read.state != ReadState::SUCCEEDED || read.uncertain ||
        read.startedUs < after)
        return error(HomeError::STATE, "home requires a fresh ordinary SDO state cycle");
    const StateBlock blocks[] = {StateBlock::MONITORS, StateBlock::STATUS, StateBlock::FEEDBACK};
    ReadWindow windows[3];
    for (unsigned i = 0; i < 3; ++i)
        if (!validateStateBlock(read, blocks[i], target, now, age, windows[i]) ||
            windows[i].earliestUs < after)
            return error(HomeError::STATE, "home state freshness unavailable");
    HomeSample s;
    s.present = true;
    s.monitor = uint16_t(raw(read, StateBlock::MONITORS, 0));
    s.logicalInputs = uint16_t(raw(read, StateBlock::MONITORS, 1));
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
    output = s;
    return Ok();
}
bool clean(const HomeSample& s, StatusLayout layout) {
    StatusIndications interpreted;
    return interpretStatusword(s.statusword, layout, interpreted) &&
           interpreted.alarm == StateIndication::NO && s.modeRaw == 6 &&
           (s.statusword & STATUS_LOW_BITS) == STATUS_LOW_BITS &&
           !(s.monitor &
             uint16_t(MONITOR_ALARM | MONITOR_RELEASED | MONITOR_POSITIVE_SOFT_LIMIT |
                      MONITOR_NEGATIVE_SOFT_LIMIT | uint16_t(~MONITOR_DOCUMENTED_BITS)));
}
Status inputsReady(const HomeEvidence& e, uint16_t rawInputs, bool admission) {
    if (rawInputs & ~uint16_t(0x7f))
        return error(HomeError::INPUT, "2101 contains unqualified logical input bits");
    const uint8_t required = requiredInputs(e);
    if (!required || (required & e.wiredInputs) != required)
        return error(HomeError::INPUT, "method-specific assigned physical input unavailable");
    if (admission && (rawInputs & required) && !e.startOnActiveSwitchQualified)
        return error(HomeError::INPUT, "starting on an active homing switch is unqualified");
    for (unsigned i = 0; i < 7; ++i) {
        const uint16_t function = e.inputFunctions[i];
        if (function > 13)
            return error(HomeError::INPUT, "input assignment contains an unreviewed function");
        if (!(rawInputs & (1u << i)))
            continue;
        if (function >= 4 && function <= 6)
            return error(HomeError::INPUT, "enable/stop input is active during homing");
        if ((function == 2 || function == 3) && !(required & (1u << i)) &&
            !(functionFor(e.method) == 1 && e.originLimitEncounterQualified))
            return error(HomeError::INPUT, "unexpected limit input during homing");
    }
    return Ok();
}
Status rateRange(const PreparedVelocityTarget& rate, uint32_t qualified) {
    if (rate.rateBasis != NativeRateBasis::MOTOR_TURNS || rate.rateTime != TimeUnit::MINUTE ||
        rate.representation != NativeRepresentation::INTEGER)
        return error(HomeError::RATE_BASIS, "6099 requires integer motor turns per minute");
    const int64_t value = rate.effectiveNative;
    if (value <= 0 || value > 3000 || value != qualified)
        return error(HomeError::RANGE, "home rate is zero or outside exact qualified rpm scope",
                     Err::ILLEGAL_VALUE);
    if (rate.exactArithmetic) {
        const NativeQuantity& q = rate.requestedNative;
        if (q.integral < 0 || (!q.integral && (!q.numerator || q.negative)) || q.integral > 3000 ||
            (q.integral == 3000 && q.numerator && !q.negative))
            return error(HomeError::RANGE, "requested home rate is outside positive 0..3000 rpm",
                         Err::ILLEGAL_VALUE);
    } else if (!std::isfinite(rate.absoluteErrorBound) || rate.absoluteErrorBound < 0 ||
               rate.absoluteErrorBound >= double(value) ||
               rate.absoluteErrorBound > double(3000 - value))
        return error(HomeError::RANGE, "approximate home rate crosses zero or maximum",
                     Err::ILLEGAL_VALUE);
    return Ok();
}
struct Checked {
    PreparedVelocityTarget search, creep;
    HomeSample admission;
    uint64_t prerequisiteUs = 0;
};
Status check(const HomeContext& output, const AxisReservation& live, const ReadTarget& current,
             const ReadContext& state, const CANOPEN::SdoCompletion& control,
             const IdentityObservation& identity, const DriverContext& inputs,
             const DriverContext& driver, const HomeEvidence& e, const HomeRequest& request,
             const AxisConfig& axis, uint32_t operation, uint64_t now, uint64_t deadline,
             const HomeOptions& options, Checked& checked) {
    const void* objects[] = {&live,   &current, &state,   &control, &identity, &inputs,
                             &driver, &e,       &request, &axis,    &options};
    const size_t sizes[] = {sizeof live,     sizeof current, sizeof state,  sizeof control,
                            sizeof identity, sizeof inputs,  sizeof driver, sizeof e,
                            sizeof request,  sizeof axis,    sizeof options};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; ++i)
        if (overlaps(&output, sizeof output, objects[i], sizes[i]))
            return error(HomeError::OVERLAP, "home preparation overlaps input");
    if (!functionFor(request.method))
        return error(HomeError::CAPABILITY, "homing method has no supported 86C sequence",
                     Err::UNSUPPORTED);
    if (e.gate == HomeGate::CLOSED || !e.methodQualified || !e.parametersQualified ||
        !e.inputsQualified)
        return error(HomeError::CAPABILITY, "homing needs exact selected-firmware qualification",
                     Err::UNSUPPORTED);
    if (e.gate != HomeGate::MEASURED || !e.record || e.layout == StatusLayout::UNSPECIFIED ||
        e.layout > StatusLayout::OP86 || e.layout == StatusLayout::ZH57 ||
        e.method != request.method || (e.wiredInputs & ~0x7fu))
        return error(HomeError::ARGUMENT, "invalid exact home qualification");
    if (!validReservation(live) || !sameTarget(live.target, current) ||
        !sameBinding(axis.binding, current.channel.binding) || axis.exhausted ||
        !axis.referenceGeneration || axis.referenceGeneration == UINT32_MAX ||
        axis.binding.configurationGeneration == UINT32_MAX || !validateAxisConfig(axis))
        return error(HomeError::TARGET, "invalid home axis or no room for coordinate withdrawal");
    if (!operation || !validOptions(options) || now > uint64_t(INT64_MAX) ||
        deadline > uint64_t(INT64_MAX) || deadline <= now || live.acquiredUs > now ||
        deadline - now <=
            13 * (uint64_t(options.service.queueTimeoutUs) + options.service.responseTimeoutUs))
        return error(HomeError::TIME, "invalid home operation, cutoff or service budget");
    if (e.model != live.model || !sameTarget(e.specimen, current) ||
        !sameTarget(identity.target, current) || !sameFirmware(e.firmware, identity) ||
        identity.window.earliestUs > identity.window.latestUs || identity.window.latestUs > now ||
        now - identity.window.earliestUs > options.maximumAgeUs)
        return error(HomeError::CAPABILITY,
                     "home qualification does not match current specimen/firmware",
                     Err::UNSUPPORTED);
    if (request.searchSpeed.value.numerator <= 0 || request.creepSpeed.value.numerator <= 0 ||
        !request.rampTimeMs || request.rampTimeMs > 2000 || request.rampTimeMs != e.rampTimeMs ||
        request.offsetNative < -1000000 || request.offsetNative > 1000000 ||
        request.offsetNative != e.offsetNative ||
        (e.referenceQualified &&
         (e.referenceNative < axis.nativeMinimum || e.referenceNative > axis.nativeMaximum)))
        return error(HomeError::RANGE, "home ramp/offset/reference outside qualified scope",
                     Err::ILLEGAL_VALUE);
    if (!acknowledgedControlWrite(control, live, now) || controlWord(control.transfer) != 0x000f ||
        control.transfer.operation >= operation)
        return error(HomeError::STATE, "home needs latest reserved acknowledged 000F");
    Status s =
        sample(state, current, now, options.maximumAgeUs, control.completedUs, checked.admission);
    if (!s)
        return s;
    StatusIndications admissionStatus;
    interpretStatusword(checked.admission.statusword, e.layout, admissionStatus);
    if (!clean(checked.admission, e.layout) || admissionStatus.running != StateIndication::NO ||
        (checked.admission.monitor & MONITOR_RUNNING) || checked.admission.velocity)
        return error(HomeError::STATE,
                     "home requires enabled stationary mode6; no implicit enable/mode change");
    IoObservation io;
    DriverObservation native;
    if (!getIo(inputs, current, now, options.maximumAgeUs, io) ||
        io.group != DriverGroup::IO_INPUT_CONFIGURATION ||
        !getDriver(driver, current, now, options.maximumAgeUs, native) ||
        native.group != DriverGroup::DRIVER || native.directionRaw != e.directionRaw ||
        io.inputPolarityRaw != e.inputPolarityRaw || io.window.earliestUs < live.acquiredUs ||
        native.window.earliestUs < live.acquiredUs)
        return error(HomeError::INPUT, "fresh matching 2400 and 2200 acquisitions required");
    for (unsigned i = 0; i < 7; ++i)
        if (io.inputFunctionRaw[i] != e.inputFunctions[i])
            return error(HomeError::INPUT,
                         "input function mapping differs from qualified specimen");
    s = inputsReady(e, checked.admission.logicalInputs, true);
    if (!s)
        return s;
    checked.prerequisiteUs = checked.admission.earliestUs;
    if (io.window.earliestUs < checked.prerequisiteUs)
        checked.prerequisiteUs = io.window.earliestUs;
    if (native.window.earliestUs < checked.prerequisiteUs)
        checked.prerequisiteUs = native.window.earliestUs;
    s = prepareVelocityTarget(request.searchSpeed, axis, checked.search);
    if (s)
        s = prepareVelocityTarget(request.creepSpeed, axis, checked.creep);
    if (!s) {
        s.detail += int32_t(HomeError::RATE_ERROR_BASE);
        return s;
    }
    s = rateRange(checked.search, e.searchSpeedRpm);
    return s ? rateRange(checked.creep, e.creepSpeedRpm) : s;
}
bool downloading(const HomeContext& c) {
    return c.phase == HomePhase::PARAMETERS || c.phase == HomePhase::TRIGGER;
}
Status shape(const HomeContext& c, uint64_t now) {
    if (!validReservation(c.reservation) || !c.operation || !c.step || !validOptions(c.options) ||
        c.state == ActionState::EMPTY || c.state > ActionState::FAILED ||
        c.phase > HomePhase::OBSERVING || c.setupCursor > 4 || (c.setupEvidenceMask & ~31u) ||
        c.evidence.gate != HomeGate::MEASURED || !functionFor(c.request.method) ||
        c.interruption > HomeInterruption::DEADLINE || c.withdrawal > HomeInterruption::DEADLINE ||
        c.deadlineUs <= c.startedUs || c.deadlineUs > uint64_t(INT64_MAX) ||
        now > uint64_t(INT64_MAX) || c.servicedUs < c.startedUs || now < c.servicedUs ||
        c.observations > c.options.maxPolls || !c.axis.referenceGeneration ||
        c.axis.referenceGeneration == UINT32_MAX ||
        c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX)
        return error(HomeError::STATE, "invalid home context or processing time");
    return Ok();
}
Status currentContext(const HomeContext& c, const AxisReservation& live, const ReadTarget& current,
                      uint64_t now) {
    const Status s = shape(c, now);
    if (!s)
        return s;
    if (!sameReservation(c.reservation, live) || !sameTarget(expectedTarget(c), current))
        return error(HomeError::TARGET, "home reservation or expected coordinate epoch changed");
    return Ok();
}
bool observationMatches(const HomeContext& c, const ReadContext& read) {
    return read.operation == c.operation && read.kind == ReadKind::STATE &&
           read.source == ReadSource::SDO && sameTarget(read.target, expectedTarget(c)) &&
           read.blockMask == CYCLE_MASK && read.count == CYCLE_FIELDS &&
           read.deadlineUs == c.deadlineUs && read.startedUs == c.observationStartedUs &&
           read.options.queueTimeoutUs == c.options.service.queueTimeoutUs &&
           read.options.responseTimeoutUs == c.options.service.responseTimeoutUs;
}
CANOPEN::SdoTransfer download(const HomeContext& c) {
    CANOPEN::SdoTransfer t;
    // Coordinate withdrawal precedes trigger admission. The actual trigger, acknowledgement and
    // subsequent reads therefore share the new binding; no terminal needs fabricated provenance.
    t.channel = expectedTarget(c).channel;
    t.operation = t.request = c.operation;
    t.step = c.step;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.service.queueTimeoutUs;
    t.responseTimeoutUs = c.options.service.responseTimeoutUs;
    t.index = 0x6040;
    t.size = 2;
    uint32_t value = 0x001f;
    if (c.phase == HomePhase::PARAMETERS) {
        t.index = INDEXES[c.setupCursor];
        t.subindex = SUBINDEXES[c.setupCursor];
        t.size = c.setupCursor == 0 ? 1 : 4;
        value = c.setupCursor == 0   ? uint32_t(c.request.method)
                : c.setupCursor == 1 ? uint32_t(c.searchSpeed.effectiveNative)
                : c.setupCursor == 2 ? uint32_t(c.creepSpeed.effectiveNative)
                : c.setupCursor == 3 ? c.request.rampTimeMs
                                     : uint32_t(c.request.offsetNative);
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
ActionOutcome outcomeOf(HomeInterruption r) {
    switch (r) {
    case HomeInterruption::DRIVE_FAULT:
        return ActionOutcome::DRIVE_FAULT;
    case HomeInterruption::INVALIDATED:
        return ActionOutcome::INVALIDATED;
    case HomeInterruption::DEADLINE:
        return ActionOutcome::DEADLINE;
    default:
        return ActionOutcome::CANCELLED;
    }
}
void settleEffects(HomeContext& c) {
    bool possible = c.transmissionNoted, uncertain = c.transmissionNoted;
    for (unsigned i = 0; i < 5; ++i)
        if (c.setupEvidenceMask & (1u << i)) {
            possible |= c.setupHistory[i].effectPossible;
            uncertain |= c.setupHistory[i].effectUncertain;
        }
    bool triggered = c.transmissionNoted && c.phase == HomePhase::TRIGGER;
    if (c.triggerEvidencePresent) {
        possible |= c.trigger.effectPossible;
        uncertain |= c.trigger.effectUncertain;
        triggered |= c.trigger.transmissionPossible;
    }
    c.needsStop = triggered && c.completion != ActionCompletion::OBSERVED &&
                  !(c.stopOutcome == ActionOutcome::OBSERVED &&
                    c.stopExecution == ActionExecution::ACKNOWLEDGED);
    c.effectsPossible = possible;
    c.uncertain = uncertain || c.readUncertain || c.needsStop;
}
void fail(HomeContext& c, ActionOutcome reason, uint64_t now, const char* message) {
    c.state = ActionState::FAILED;
    c.outcome = reason;
    c.status = error(
        reason == ActionOutcome::DEADLINE ? HomeError::TIME : HomeError::STATE, message,
        reason == ActionOutcome::PROTOCOL_REJECTED ? Err::PROTOCOL_ERROR : Err::INVALID_CONFIG);
    c.servicedUs = now;
    settleEffects(c);
}
void finishCycle(HomeContext& c, const ReadContext& read, uint64_t now) {
    HomeSample s;
    if (!sample(read, expectedTarget(c), now, c.options.maximumAgeUs, c.trigger.completedUs, s)) {
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "home observation freshness unavailable");
        return;
    }
    if (c.observations == UINT32_MAX) {
        fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "home observation identity exhausted");
        return;
    }
    s.cycle = ++c.observations;
    c.latest = s;
    StatusIndications interpreted;
    interpretStatusword(s.statusword, c.evidence.layout, interpreted);
    if ((s.monitor & MONITOR_ALARM) || interpreted.alarm == StateIndication::YES) {
        fail(c, ActionOutcome::DRIVE_FAULT, now, "home observed a drive alarm");
        return;
    }
    if (!clean(s, c.evidence.layout) || !inputsReady(c.evidence, s.logicalInputs, false)) {
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now, "home mode, input or status scope changed");
        return;
    }
    const bool activeInput = (s.logicalInputs & requiredInputs(c.evidence)) != 0;
    const bool inputWasActive = c.requiredInputObserved;
    c.requiredInputObserved |= activeInput;
    if (!(s.statusword & HOME_COMPLETE) && !c.absence.present)
        c.absence = s;
    if ((s.monitor & MONITOR_RUNNING) && !c.activity.present)
        c.activity = s;
    // Conservative host evidence policy: missed low/activity/input transitions never manufacture
    // a fresh completion. Neither sampled input edges nor bit8 qualify creep direction by themselves.
    const bool finalInput = c.request.method == HomingMethod::NEGATIVE_ORIGIN
                                ? inputWasActive && !activeInput
                                : activeInput;
    if ((s.statusword & HOME_COMPLETE) && c.absence.present && c.absence.cycle < s.cycle &&
        c.activity.present && c.activity.cycle < s.cycle && finalInput &&
        (s.monitor & MONITOR_HOMING_COMPLETE) && !(s.monitor & MONITOR_RUNNING) && !s.velocity &&
        interpreted.running == StateIndication::NO) {
        c.completed = s;
        c.completion = ActionCompletion::OBSERVED;
        c.state = ActionState::SUCCEEDED;
        c.outcome = ActionOutcome::OBSERVED;
        c.status = Ok();
        settleEffects(c);
        return;
    }
    if (c.observations >= c.options.maxPolls) {
        fail(c, ActionOutcome::OBSERVATION_SCOPE, now,
             "home completion not established within poll bound");
        return;
    }
    c.phase = HomePhase::WAITING;
    c.eligibleUs = c.options.pollIntervalUs >= c.deadlineUs - now ? c.deadlineUs
                                                                  : now + c.options.pollIntervalUs;
    c.stepStartedUs = c.eligibleUs;
}
Status yield(const HomeContext& c, const ReadContext& read, const AxisReservation& live,
             const ReadTarget& current, uint64_t now, PreparedHome& output, bool correlating) {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &read, sizeof read) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &current, sizeof current))
        return error(HomeError::OVERLAP, "home work overlaps input");
    Status s = currentContext(c, live, current, now);
    if (!s)
        return s;
    PreparedHome next;
    next.eligibleUs = c.eligibleUs;
    if (c.state != ActionState::ACTIVE || now < c.eligibleUs ||
        (!correlating &&
         (c.withdrawal != HomeInterruption::NONE || (downloading(c) && c.transmissionNoted)))) {
        output = next;
        return Ok();
    }
    if (now >= c.deadlineUs)
        return error(HomeError::TIME, "home deadline reached");
    if (!correlating && !exchangeFits(c.deadlineUs, now, c.options.service.queueTimeoutUs,
                                      c.options.service.responseTimeoutUs)) {
        output = next;
        return Ok();
    }
    if (c.phase == HomePhase::TRIGGER && !c.coordinatesInvalidated)
        next.prepareTrigger = true;
    else if (downloading(c)) {
        next.transfer = download(c);
        next.ready = true;
    } else if (c.phase == HomePhase::WAITING)
        next.observe = true;
    else {
        if (!observationMatches(c, read))
            return error(HomeError::CORRELATION, "read is not this home's observation cycle");
        PreparedRead pending;
        s = nextRead(read, now, pending);
        if (!s)
            return s;
        if (!pending.ready)
            return error(HomeError::STATE, "home cycle has no pending service");
        next.ready = true;
        next.transfer = pending.transfer;
        next.transfer.step = c.step;
    }
    output = next;
    return Ok();
}
} // namespace

Status validateHome(const HomeContext& output, const AxisReservation& live,
                    const ReadTarget& current, const ReadContext& state,
                    const CANOPEN::SdoCompletion& control, const IdentityObservation& identity,
                    const DriverContext& inputs, const DriverContext& driver,
                    const HomeEvidence& evidence, const HomeRequest& request,
                    const AxisConfig& axis, uint32_t operation, uint64_t now, uint64_t deadline,
                    const HomeOptions& options) noexcept {
    Checked checked;
    return check(output, live, current, state, control, identity, inputs, driver, evidence, request,
                 axis, operation, now, deadline, options, checked);
}
Status prepareHome(HomeContext& output, const AxisReservation& live, const ReadTarget& current,
                   const ReadContext& state, const CANOPEN::SdoCompletion& control,
                   const IdentityObservation& identity, const DriverContext& inputs,
                   const DriverContext& driver, const HomeEvidence& evidence,
                   const HomeRequest& request, const AxisConfig& axis, uint32_t operation,
                   uint64_t now, uint64_t deadline, const HomeOptions& options) noexcept {
    Checked checked;
    const Status s = check(output, live, current, state, control, identity, inputs, driver,
                           evidence, request, axis, operation, now, deadline, options, checked);
    if (!s)
        return s;
    new (static_cast<void*>(&output)) HomeContext();
    output.reservation = live;
    output.evidence = evidence;
    output.request = request;
    output.axis = axis;
    output.options = options;
    output.searchSpeed = checked.search;
    output.creepSpeed = checked.creep;
    output.admission = output.latest = checked.admission;
    output.prerequisiteUs = checked.prerequisiteUs;
    output.control = control;
    output.operation = operation;
    output.state = ActionState::ACTIVE;
    output.startedUs = output.servicedUs = output.eligibleUs = output.stepStartedUs = now;
    output.deadlineUs = deadline;
    return Ok();
}
void resetHome(HomeContext& c) noexcept {
    new (static_cast<void*>(&c)) HomeContext();
}
Status nextHome(const HomeContext& c, const ReadContext& observation, const AxisReservation& live,
                const ReadTarget& current, uint64_t now, PreparedHome& output) noexcept {
    return yield(c, observation, live, current, now, output, false);
}
Status beginHomeObservation(HomeContext& c, ReadContext& read, const AxisReservation& live,
                            const ReadTarget& current, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &read, sizeof read) || overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &current, sizeof current) ||
        overlaps(&read, sizeof read, &live, sizeof live) ||
        overlaps(&read, sizeof read, &current, sizeof current))
        return error(HomeError::OVERLAP, "home observation overlaps input");
    Status s = currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.phase != HomePhase::WAITING ||
        c.withdrawal != HomeInterruption::NONE || now < c.eligibleUs || now >= c.deadlineUs ||
        c.observations >= c.options.maxPolls)
        return error(HomeError::STATE, "no eligible home observation");
    s = prepareState(read, current, c.operation, now, c.deadlineUs, c.options.service, CYCLE_MASK);
    if (!s)
        return s;
    c.phase = HomePhase::OBSERVING;
    c.observationStartedUs = c.stepStartedUs = c.servicedUs = now;
    return Ok();
}
Status validateHomeAdmission(const HomeContext& c, const AxisReservation& live,
                             const ReadTarget& current, const AxisConfig& axis,
                             uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &axis, sizeof axis))
        return error(HomeError::OVERLAP, "home admission overlaps axis");
    const Status s = currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || !downloading(c) ||
        c.withdrawal != HomeInterruption::NONE || now < c.eligibleUs || now >= c.deadlineUs ||
        !exchangeFits(c.deadlineUs, now, c.options.service.queueTimeoutUs,
                      c.options.service.responseTimeoutUs))
        return error(HomeError::TIME, "home download is not eligible within the fixed budget");
    if (!validateAxisConfig(axis) || axis.exhausted ||
        !sameBinding(axis.binding, current.channel.binding) ||
        axis.referenceGeneration !=
            c.axis.referenceGeneration + (c.coordinatesInvalidated ? 1u : 0u) ||
        axis.nativeRateBasis != c.searchSpeed.rateBasis ||
        axis.nativeVelocityTime != c.searchSpeed.rateTime ||
        axis.velocityRepresentation != c.searchSpeed.representation || now < c.prerequisiteUs ||
        now - c.prerequisiteUs > c.options.maximumAgeUs ||
        (c.coordinatesInvalidated &&
         (axis.originKnown || axis.encoderOriginKnown || axis.softLimitsKnown)))
        return error(HomeError::TARGET, "home conversion or prerequisite generation/age changed");
    return Ok();
}
Status beginHomeTrigger(HomeContext& c, const AxisReservation& live, ReadTarget& current,
                        AxisConfig& axis, AxisReference& reference, uint64_t now) noexcept {
    const void* objects[] = {&c, &current, &axis, &reference};
    const size_t sizes[] = {sizeof c, sizeof current, sizeof axis, sizeof reference};
    for (unsigned i = 0; i < 4; ++i) {
        if (overlaps(objects[i], sizes[i], &live, sizeof live))
            return error(HomeError::OVERLAP, "home trigger overlaps reservation");
        for (unsigned j = 0; j < i; ++j)
            if (overlaps(objects[i], sizes[i], objects[j], sizes[j]))
                return error(HomeError::OVERLAP, "home trigger outputs overlap");
    }
    Status s = validateHomeAdmission(c, live, current, axis, now);
    if (!s)
        return s;
    if (c.phase != HomePhase::TRIGGER || c.coordinatesInvalidated || c.transmissionNoted ||
        axis.referenceGeneration == UINT32_MAX ||
        axis.binding.configurationGeneration == UINT32_MAX ||
        (reference.nativeKnown && (!sameBinding(reference.binding, axis.binding) ||
                                   reference.referenceGeneration != axis.referenceGeneration)))
        return error(HomeError::STATE, "home trigger requires original coordinate epochs");
    s = invalidateAxisReference(axis, reference, ReferenceLoss::COORDINATES);
    if (!s)
        return s; // Proven nonexhausted above; this call has no other failing precondition.
    current.channel.binding = axis.binding;
    c.coordinatesInvalidated = true;
    c.servicedUs = now;
    return Ok();
}
Status noteHomeTransmission(HomeContext& c, const AxisReservation& live, ReadTarget& current,
                            AxisConfig& axis, AxisReference& reference,
                            DriverContext* const* retained, uint8_t count, uint64_t now) noexcept {
    const void* objects[] = {&c, &current, &axis, &reference};
    const size_t sizes[] = {sizeof c, sizeof current, sizeof axis, sizeof reference};
    for (unsigned i = 0; i < 4; ++i) {
        if (overlaps(objects[i], sizes[i], &live, sizeof live))
            return error(HomeError::OVERLAP, "home transmission overlaps reservation");
        for (unsigned j = 0; j < i; ++j)
            if (overlaps(objects[i], sizes[i], objects[j], sizes[j]))
                return error(HomeError::OVERLAP, "home transmission outputs overlap");
    }
    Status s = validateHomeAdmission(c, live, current, axis, now);
    if (!s)
        return s;
    if (c.transmissionNoted || (count && !retained) ||
        (c.phase == HomePhase::TRIGGER && !c.coordinatesInvalidated) ||
        (reference.nativeKnown && (!sameBinding(reference.binding, axis.binding) ||
                                   reference.referenceGeneration != axis.referenceGeneration)))
        return error(HomeError::STATE, "home notice already supplied or reference/list invalid");
    for (unsigned i = 0; i < count; ++i) {
        if (!retained[i])
            return error(HomeError::ARGUMENT, "missing retained parameter observation");
        for (unsigned j = 0; j < 4; ++j)
            if (overlaps(retained[i], sizeof *retained[i], objects[j], sizes[j]))
                return error(HomeError::OVERLAP,
                             "retained parameter observation overlaps home input");
        if (overlaps(retained[i], sizeof *retained[i], &live, sizeof live) ||
            overlaps(retained[i], sizeof *retained[i], retained, size_t(count) * sizeof *retained))
            return error(HomeError::OVERLAP, "retained parameter list overlaps storage");
        const ReadContext& r = retained[i]->read;
        if (c.phase == HomePhase::PARAMETERS && r.state != ReadState::EMPTY &&
            r.kind == ReadKind::DRIVER_MOTION_PARAMETERS) {
            Binding a = r.target.channel.binding, b = current.channel.binding;
            a.bindingGeneration = b.bindingGeneration;
            a.configurationGeneration = b.configurationGeneration;
            if (!sameBinding(a, b) ||
                r.target.channel.clientToServerId != current.channel.clientToServerId ||
                r.target.channel.serverToClientId != current.channel.serverToClientId)
                return error(HomeError::TARGET,
                             "retained motion parameters belong to another axis");
        }
    }
    if (c.phase == HomePhase::PARAMETERS) {
        const CANOPEN::SdoTransfer work = download(c);
        for (unsigned i = 0; i < count; ++i) {
            s = retireMotionParameters(*retained[i], current, work);
            if (!s)
                return s;
        }
    }
    c.transmissionNoted = c.effectsPossible = c.uncertain = true;
    if (c.phase == HomePhase::TRIGGER) {
        c.execution = ActionExecution::UNKNOWN;
        c.needsStop = true;
    } else
        c.setup = ActionExecution::UNKNOWN;
    c.servicedUs = now;
    return Ok();
}
Status noteHomeWithdrawal(HomeContext& c, const AxisReservation& live, const ReadTarget& current,
                          HomeInterruption reason, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &current, sizeof current))
        return error(HomeError::OVERLAP, "home withdrawal overlaps input");
    const Status s = reason == HomeInterruption::INVALIDATED
                         ? shape(c, now)
                         : currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || c.withdrawal != HomeInterruption::NONE ||
        reason == HomeInterruption::NONE || reason >= HomeInterruption::DEADLINE)
        return error(HomeError::STATE, "invalid home withdrawal");
    c.withdrawal = now >= c.deadlineUs ? HomeInterruption::DEADLINE : reason;
    c.withdrawalUs = now;
    return Ok();
}
Status advanceHome(HomeContext& c, ReadContext& read, const AxisReservation& live,
                   const ReadTarget& current, const CANOPEN::SdoCompletion& input,
                   uint64_t now) noexcept {
    const void* inputs[] = {&live, &current, &input};
    const size_t sizes[] = {sizeof live, sizeof current, sizeof input};
    for (unsigned i = 0; i < 3; ++i)
        if (overlaps(&c, sizeof c, inputs[i], sizes[i]) ||
            overlaps(&read, sizeof read, inputs[i], sizes[i]))
            return error(HomeError::OVERLAP, "home terminal overlaps input");
    if (overlaps(&c, sizeof c, &read, sizeof read))
        return error(HomeError::OVERLAP, "home terminal overlaps external read");
    Status s = currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE)
        return error(HomeError::STATE, "home already terminal");
    PreparedHome work;
    s = yield(c, read, live, current, c.servicedUs < c.eligibleUs ? c.eligibleUs : c.servicedUs,
              work, true);
    if (!s)
        return s;
    if (!work.ready || !CANOPEN::sameSdoWork(input.transfer, work.transfer) ||
        input.nodeGeneration != c.reservation.target.nodeGeneration ||
        input.admittedUs < c.stepStartedUs || input.admittedUs >= c.deadlineUs ||
        input.completedUs < input.admittedUs || input.completedUs < c.servicedUs ||
        input.completedUs > now)
        return error(HomeError::CORRELATION, "home terminal does not match current service");
    const bool expired = now >= c.deadlineUs || input.completedUs >= c.deadlineUs;
    const bool withdrawn = c.withdrawal != HomeInterruption::NONE;
    if (c.phase == HomePhase::OBSERVING) {
        CANOPEN::SdoCompletion local = input;
        local.transfer.step = uint32_t(read.cursor) + 1;
        s = advanceRead(read, local, now);
        if (!s)
            return s;
        c.readUncertain |= read.uncertain;
        c.observationEvidence = input;
        c.observationEvidencePresent = true;
        if (!withdrawn && !expired && read.state == ReadState::FAILED)
            fail(c, outcomeOf(read.outcome), now, "home observation failed");
        else if (!withdrawn && !expired) {
            if (c.step != UINT32_MAX)
                ++c.step;
            c.stepStartedUs = now;
            if (read.state == ReadState::SUCCEEDED)
                finishCycle(c, read, now);
        }
    } else {
        if (!CANOPEN::checkDownloadTerminal(input, c.transmissionNoted) ||
            (c.phase == HomePhase::TRIGGER && input.transmissionPossible &&
             !c.coordinatesInvalidated))
            return error(HomeError::CORRELATION, "inconsistent home write/coordinate terminal");
        const bool success = input.outcome == CANOPEN::SdoOutcome::SUCCESS;
        const ActionExecution execution =
            success                                              ? ActionExecution::ACKNOWLEDGED
            : input.outcome == CANOPEN::SdoOutcome::SERVER_ABORT ? ActionExecution::REJECTED
            : input.transmissionPossible                         ? ActionExecution::UNKNOWN
                                                                 : ActionExecution::NOT_TRANSMITTED;
        if (c.phase == HomePhase::PARAMETERS) {
            c.setupHistory[c.setupCursor] = input;
            c.setupEvidenceMask = uint8_t(c.setupEvidenceMask | (1u << c.setupCursor));
            c.setup = success && c.setupCursor == 4 ? ActionExecution::ACKNOWLEDGED
                      : success || c.setupCursor    ? ActionExecution::UNKNOWN
                                                    : execution;
        } else {
            c.trigger = input;
            c.triggerEvidencePresent = true;
            c.execution = execution;
            if (success)
                c.control = input;
        }
        c.transmissionNoted = false;
        settleEffects(c);
        if (!withdrawn && !expired && !success)
            fail(c, outcomeOf(readOutcome(input.outcome)), now,
                 "home download failed; never retried");
        else if (!withdrawn && !expired && c.step != UINT32_MAX) {
            ++c.step;
            c.eligibleUs = c.stepStartedUs = now;
            if (c.phase == HomePhase::TRIGGER)
                c.phase = HomePhase::WAITING;
            else if (c.setupCursor < 4)
                ++c.setupCursor;
            else
                c.phase = HomePhase::TRIGGER;
        }
    }
    if (expired) {
        c.interruption = HomeInterruption::DEADLINE;
        fail(c, ActionOutcome::DEADLINE, now, "home processing deadline reached");
    } else if (withdrawn) {
        c.interruption = c.withdrawal;
        fail(c, outcomeOf(c.withdrawal), now, "home withdrawn; actual service evidence retained");
    } else if (c.state == ActionState::ACTIVE && c.step == UINT32_MAX)
        fail(c, ActionOutcome::ITERATION_EXHAUSTED, now, "home service identity exhausted");
    c.servicedUs = now;
    settleEffects(c);
    return Ok();
}
Status advanceHome(HomeContext& c, ReadContext& read, const AxisReservation& live,
                   const ReadTarget& current, HomeInterruption reason, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &read, sizeof read) || overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &current, sizeof current) ||
        overlaps(&read, sizeof read, &live, sizeof live) ||
        overlaps(&read, sizeof read, &current, sizeof current))
        return error(HomeError::OVERLAP, "home interruption overlaps input");
    Status s = reason == HomeInterruption::INVALIDATED ? shape(c, now)
                                                       : currentContext(c, live, current, now);
    if (!s)
        return s;
    if (c.state != ActionState::ACTIVE || reason == HomeInterruption::NONE ||
        reason > HomeInterruption::DEADLINE ||
        (reason == HomeInterruption::DEADLINE && now < c.deadlineUs))
        return error(HomeError::STATE, "invalid local home interruption");
    if (c.phase == HomePhase::OBSERVING && observationMatches(c, read) &&
        read.state == ReadState::ACTIVE) {
        const ReadOutcome r = reason == HomeInterruption::INVALIDATED ? ReadOutcome::INVALIDATED
                              : now >= c.deadlineUs                   ? ReadOutcome::DEADLINE
                                                                      : ReadOutcome::CANCELLED;
        s = advanceRead(read, r, now);
        if (!s)
            return s;
        c.readUncertain |= read.uncertain;
    } else if (c.phase == HomePhase::OBSERVING && reason != HomeInterruption::INVALIDATED &&
               !observationMatches(c, read))
        return error(HomeError::CORRELATION, "home interrupted with another read context");
    if (now >= c.deadlineUs)
        reason = HomeInterruption::DEADLINE;
    c.interruption = reason;
    fail(c, outcomeOf(reason), now, "home ended locally; no implicit stop");
    return Ok();
}
namespace {
Status stopShape(const HomeContext& c, uint64_t now) {
    const Status s = shape(c, now);
    if (!s)
        return s;
    if (c.state != ActionState::FAILED || c.stopOperation || !c.needsStop)
        return error(HomeError::STATE, "home has no unsettled independent stop");
    return Ok();
}
void retainStop(HomeContext& c, uint32_t operation, ActionOutcome outcome,
                ActionExecution execution, const Status& status,
                const CANOPEN::SdoCompletion& terminal, uint64_t now) {
    c.stopOperation = operation;
    c.stopOutcome = outcome;
    c.stopExecution = execution;
    c.stopStatus = status;
    c.stopEvidence = terminal;
    c.servicedUs = now;
    settleEffects(c);
}
} // namespace
Status advanceHomeStop(HomeContext& c, const ActionContext& stop, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &stop, sizeof stop))
        return error(HomeError::OVERLAP, "home stop overlaps context");
    const Status s = stopShape(c, now);
    if (!s)
        return s;
    if (!sameTarget(stop.reservation.target, expectedTarget(c)) ||
        stop.actionKind != ActionKind::STOP || stop.reservation.owner != c.reservation.owner ||
        stop.reservation.sequence != c.reservation.sequence ||
        stop.reservation.generation != c.reservation.generation ||
        stop.reservation.acquiredUs != c.reservation.acquiredUs ||
        stop.reservation.model != c.reservation.model || stop.kind != StopKind::HALT ||
        stop.stopModeRaw != 6 || stop.operation <= c.operation ||
        stop.startedUs < (c.withdrawal != HomeInterruption::NONE ? c.withdrawalUs : c.startedUs) ||
        stop.servicedUs < stop.startedUs || stop.servicedUs > now ||
        (stop.state != ActionState::FAILED && stop.state != ActionState::SUCCEEDED))
        return error(HomeError::CORRELATION, "stop does not belong to this interrupted home");
    if (stop.outcome == ActionOutcome::OBSERVED &&
        (stop.state != ActionState::SUCCEEDED || stop.completion != ActionCompletion::OBSERVED ||
         stop.execution != ActionExecution::ACKNOWLEDGED || !(stop.commandEvidenceMask & 1) ||
         !CANOPEN::checkDownloadTerminal(stop.commandHistory[0], true) ||
         controlWord(stop.commandHistory[0].transfer) != 0x000f))
        return error(HomeError::CORRELATION, "home stop lacks actual acknowledged 000F evidence");
    retainStop(c, stop.operation, stop.outcome, stop.execution, stop.status, stop.commandHistory[0],
               now);
    return Ok();
}
Status advanceHomeStop(HomeContext& c, const BestEffortStop& stop, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &stop, sizeof stop))
        return error(HomeError::OVERLAP, "home urgent stop overlaps context");
    const Status s = stopShape(c, now);
    if (!s)
        return s;
    // A controller loss can advance the node epoch before a later explicit best-effort stop.
    // Retain that failed attempt on the same binding/route without qualifying channel reuse,
    // stopped state or the earlier home. A different binding or an older epoch is unrelated.
    const ReadTarget expected = expectedTarget(c);
    if (!CANOPEN::sameSdoChannel(stop.target.channel, expected.channel) ||
        stop.target.nodeGeneration < expected.nodeGeneration || stop.operation <= c.operation ||
        stop.startedUs < (c.withdrawal != HomeInterruption::NONE ? c.withdrawalUs : c.startedUs) ||
        stop.servicedUs < stop.startedUs || stop.servicedUs > now ||
        stop.state != ActionState::FAILED || stop.outcome == ActionOutcome::OBSERVED ||
        stop.execution == ActionExecution::ACKNOWLEDGED)
        return error(HomeError::CORRELATION, "urgent stop does not belong to interrupted home");
    retainStop(c, stop.operation, stop.outcome, stop.execution, stop.status, stop.commandEvidence,
               now);
    return Ok();
}
Status getHomeReference(const HomeContext& c, const ReadTarget& current, const AxisConfig& axis,
                        uint64_t now, uint64_t age, AxisReference& output) noexcept {
    const void* objects[] = {&c, &current, &axis};
    const size_t sizes[] = {sizeof c, sizeof current, sizeof axis};
    for (unsigned i = 0; i < 3; ++i)
        if (overlaps(&output, sizeof output, objects[i], sizes[i]))
            return error(HomeError::OVERLAP, "home reference overlaps input");
    const Status s = shape(c, now);
    if (!s)
        return s;
    if (c.state != ActionState::SUCCEEDED || c.outcome != ActionOutcome::OBSERVED ||
        c.completion != ActionCompletion::OBSERVED || !c.completed.present ||
        c.execution != ActionExecution::ACKNOWLEDGED || !c.evidence.referenceQualified ||
        c.completed.position != c.evidence.finalFeedbackNative || !c.coordinatesInvalidated ||
        c.withdrawal != HomeInterruption::NONE || c.interruption != HomeInterruption::NONE ||
        !sameTarget(expectedTarget(c), current) ||
        !sameBinding(axis.binding, current.channel.binding) ||
        axis.referenceGeneration != c.axis.referenceGeneration + 1u || axis.exhausted ||
        !validateAxisConfig(axis) || !age || age > uint64_t(INT64_MAX) ||
        now < c.completed.earliestUs || now - c.completed.earliestUs > age ||
        c.evidence.referenceNative < axis.nativeMinimum ||
        c.evidence.referenceNative > axis.nativeMaximum)
        return error(HomeError::REFERENCE,
                     "home command-coordinate relation is absent, stale or invalidated");
    AxisReference ref;
    ref.binding = axis.binding;
    ref.referenceGeneration = axis.referenceGeneration;
    ref.idle = ref.stationary = ref.nativeKnown = true;
    ref.nativePosition = c.evidence.referenceNative;
    ref.basis = RelativeBasis::ACTUAL;
    ref.source = ScaleSource::QUALIFIED;
    ref.observedUs = c.completed.earliestUs;
    ref.nowUs = now;
    ref.maximumAgeUs = age;
    output = ref;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
