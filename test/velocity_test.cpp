// SPDX-License-Identifier: MIT
// Synthetic source-backed shapes exercise public finite-velocity contracts, never firmware facts.
#include <MotorControlCAN/profiles/lichuan_c/Velocity.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
unsigned checks = 0;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                       \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
const uint64_t START = 1000, DURATION = 500000, DEADLINE = 1000000;
const uint8_t BLOCKS =
    uint8_t((1u << unsigned(L::StateBlock::MONITORS)) | (1u << unsigned(L::StateBlock::STATUS)) |
            (1u << unsigned(L::StateBlock::FEEDBACK)));

C::SdoCompletion reply(const C::SdoTransfer& work, uint64_t start, uint64_t end,
                       uint32_t value = 0) {
    C::SdoCompletion e;
    e.transfer = work;
    e.nodeGeneration = 11;
    e.admittedUs = start;
    e.completedUs = end;
    e.outcome = C::SdoOutcome::SUCCESS;
    e.queued = e.transmissionPossible = e.protocolAcknowledged = e.effectPossible = true;
    if (work.direction == C::SdoDirection::UPLOAD) {
        e.size = work.size;
        for (unsigned i = 0; i < e.size; ++i)
            e.data[i] = uint8_t(value >> (8u * i));
    }
    return e;
}
struct Rig {
    L::ReadTarget target;
    L::AxisReservation live;
    L::ReadContext initial, observation;
    L::IdentityObservation identity;
    L::VelocityEvidence evidence;
    M::FiniteVelocityRequest request;
    L::VelocityProfile profile;
    L::VelocityOptions options;
    M::AxisConfig axis;
    L::VelocityContext context;
    uint8_t mode = 3;
    uint16_t control = 0;
    bool lag = false; // After 15, keep word 7's report, as trial 1009's drive did (6041 8033, 2100 0001).
    // After 15, keep word 7's report for this many cycles, then report the enable (backlog S17).
    unsigned lagCycles = 0;
    // The lagging report, changeable between cycles; the enable report's 6041.
    uint16_t lagStatus = 0x8033, lagMonitor = 0x0001, runningStatus = 0x4007;
    int32_t lagPosition = 0, lagVelocity = 0;
    int32_t runningVelocity = 0; // Nonzero: 606C after word 15 instead of the commanded rpm.
    // Backlog S18: the admission reports the release a moving quick stop left, 2100 latchedMonitor (0015,
    // running latched) with 6041 latchedStatus; 2100 0015 stays in the cycles of words below
    // latchClearedBy. 6064 reads latchedPosition, plus creep from word 6's cycle on; 606C reads
    // latchedVelocity at admission.
    bool latched = false;
    uint16_t latchedMonitor = 0x0015, latchedStatus = 0xc050, latchClearedBy = 7;
    int32_t latchedPosition = 5608, creep = 0, latchedVelocity = 0;
    // Added to 6064 from word 7's cycle on: re-energizing pulled the rotor (trials 1742, 1762).
    int32_t pull = 0;
    uint64_t now = START;
    Rig() {
        target.channel.binding.logicalAxisId = 17;
        target.channel.binding.endpoint.profile = 41;
        target.channel.binding.endpoint.address = 5;
        target.channel.binding.endpoint.bus = 2;
        target.channel.binding.bindingGeneration = 3;
        target.channel.binding.configurationGeneration = 7;
        target.channel.clientToServerId = 0x605;
        target.channel.serverToClientId = 0x585;
        target.nodeGeneration = 11;
        live.target = target;
        live.model = L::SettingsModel::CL86_C;
        live.owner = 41;
        live.sequence = 3;
        live.generation = 2;
        live.acquiredUs = 100;
        axis.binding = target.channel.binding;
        axis.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
        axis.nativeVelocityTime = M::TimeUnit::MINUTE;
        request.velocity.configurationGeneration = axis.binding.configurationGeneration;
        request.velocity.value = M::Rational(120);
        request.velocity.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
        request.durationUs = DURATION;
        profile.rampUpTimeMs = 100;
        profile.rampDownTimeMs = 200;
        options.service.queueTimeoutUs = options.service.responseTimeoutUs = 100;
        options.pollIntervalUs = 10000;
        options.maxPolls = 100;
        options.maximumAgeUs = 1000000;
        options.requiredStopBudgetUs = 100000;
        options.commandIntervalUs = 10000;
        evidence.gate = L::VelocityGate::MEASURED;
        evidence.record = 5;
        evidence.model = live.model;
        evidence.layout = L::StatusLayout::ZH86;
        evidence.startQualified = evidence.stopQualified = true;
        evidence.maximumForwardRpm = 3000;
        evidence.firmware.deviceType = 0x00040192;
        evidence.firmware.hardwareVersion = 12;
        evidence.firmware.softwareVersion = 32;
        evidence.firmware.vendorId = 1000;
        evidence.firmware.productCode = 1;
        evidence.firmware.modelCode = 0x400;
        evidence.firmware.firmwareCode = 0x200;
        identity.target = target;
        identity.deviceType = evidence.firmware.deviceType;
        identity.hardwareVersion = evidence.firmware.hardwareVersion;
        identity.softwareVersion = evidence.firmware.softwareVersion;
        identity.vendorId = evidence.firmware.vendorId;
        identity.productCode = evidence.firmware.productCode;
        identity.modelCode = evidence.firmware.modelCode;
        identity.firmwareCode = evidence.firmware.firmwareCode;
        acquireInitial();
    }
    uint32_t value(uint16_t index) const {
        if (index == 0x6061)
            return mode;
        if ((lag || lagCycles) && control == 15)
            return index == 0x6041   ? lagStatus
                   : index == 0x2100 ? lagMonitor
                   : index == 0x6064 ? uint32_t(lagPosition)
                   : index == 0x606c ? uint32_t(lagVelocity)
                                     : 0u;
        const bool latch = latched && control < latchClearedBy;
        if (index == 0x6041)
            return control == 15 ? runningStatus : control == 7 ? 3u : control == 6 || !latched ? 1u : latchedStatus;
        if (index == 0x2100)
            return control == 15 ? L::MONITOR_RUNNING
                   : !latched    ? L::MONITOR_RELEASED
                   : !control    ? latchedMonitor
                   : latch       ? 0x0015u
                                 : L::MONITOR_RELEASED;
        if (index == 0x6064)
            return uint32_t((latched ? latchedPosition + (control ? creep : 0) : 0) +
                            (control == 7 || control == 15 ? pull : 0));
        // Running reports the commanded whole rpm, as both specimens did at cruise, so the over-speed check
        // (VELOCITY_OVERSPEED_MARGIN_RPM) is not what a test meets unless it sets runningVelocity.
        if (index == 0x606c)
            return control == 15 ? uint32_t(runningVelocity ? runningVelocity
                                                            : int32_t(request.velocity.value.numerator /
                                                                      int64_t(request.velocity.value.denominator)))
                   : latched && !control ? uint32_t(latchedVelocity)
                                         : 0u;
        return 0;
    }
    void acquireInitial(uint64_t time = 200) {
        CHECK(L::prepareState(initial, target, 1, time, time + 1000, options.service, BLOCKS));
        while (initial.state == M::ReadState::ACTIVE) {
            L::PreparedRead p;
            CHECK(L::nextRead(initial, time, p) && p.ready);
            CHECK(L::advanceRead(initial, reply(p.transfer, time, time, value(p.transfer.index)),
                                 time));
            ++time;
        }
    }
    M::Status prepare(uint64_t deadline = DEADLINE) {
        return L::prepareVelocity(context, live, target, initial, identity, evidence, request,
                                  profile, axis, 42, now, deadline, options);
    }
    void reject(uint64_t deadline = DEADLINE) {
        context.operation = 0xbeef;
        const Saved<L::VelocityContext> before(context);
        CHECK(!prepare(deadline));
        before.unchanged(context);
    }
    L::PreparedVelocity next(uint64_t time) {
        L::PreparedVelocity work;
        CHECK(L::nextVelocity(context, observation, live, target, time, work));
        return work;
    }
    L::PreparedVelocity reachWord(uint16_t wanted) {
        CHECK(prepare());
        for (unsigned step = 0; step < 100; ++step) {
            L::PreparedVelocity work = next(now);
            if (work.observe) {
                CHECK(L::beginVelocityObservation(context, observation, live, target, now));
                continue;
            }
            if (!work.ready) {
                now = work.eligibleUs > now ? work.eligibleUs : now + 100;
                continue;
            }
            if (work.transfer.direction == C::SdoDirection::DOWNLOAD) {
                if (work.transfer.index == 0x6040) {
                    const uint16_t word =
                        uint16_t(work.transfer.data[0] | (uint16_t(work.transfer.data[1]) << 8));
                    if (word == wanted)
                        return work;
                    control = word;
                }
                CHECK(L::noteVelocityTransmission(context, live, target, axis, nullptr, 0, now));
            }
            CHECK(L::advanceVelocity(context, observation, live, target,
                                     reply(work.transfer, now, now + 1, value(work.transfer.index)),
                                     now + 1));
            now += 100;
        }
        CHECK(false);
        return L::PreparedVelocity();
    }
    // One step of the sequence against this fake: begin a cycle, wait, or serve the yielded work.
    void serveOne() {
        L::PreparedVelocity work = next(now);
        if (work.observe) {
            CHECK(L::beginVelocityObservation(context, observation, live, target, now));
            return;
        }
        if (!work.ready) {
            now = work.eligibleUs > now ? work.eligibleUs : now + 100;
            return;
        }
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD) {
            CHECK(L::noteVelocityTransmission(context, live, target, axis, nullptr, 0, now));
            if (work.transfer.index == 0x6040)
                control = uint16_t(work.transfer.data[0] | (uint16_t(work.transfer.data[1]) << 8));
            if (work.transfer.index == 0x6060)
                mode = work.transfer.data[0];
        }
        CHECK(L::advanceVelocity(context, observation, live, target,
                                 reply(work.transfer, now, now + 1, value(work.transfer.index)),
                                 now + 1));
        if (work.transfer.direction == C::SdoDirection::UPLOAD && work.transfer.index == 0x606c &&
            control == 15 && lagCycles)
            --lagCycles;
        now += 100;
    }
    void reachRunning() {
        CHECK(prepare());
        for (unsigned step = 0; step < 100 && !context.runningObserved; ++step)
            serveOne();
        CHECK(context.runningObserved && context.needsStop &&
              context.state == M::ActionState::ACTIVE);
        CHECK(context.startedUs == START && context.stopDueUs == START + DURATION);
    }
    // Trial 1009's shape: the first cycle after the acknowledged 15 still reports word 7's state.
    void reachLaggingFailure() {
        lag = true;
        CHECK(prepare());
        for (unsigned step = 0; step < 100 && context.phase != L::VelocityPhase::STOPPING; ++step)
            serveOne();
        CHECK(context.state == M::ActionState::ACTIVE && context.phase == L::VelocityPhase::STOPPING &&
              context.stopRequested && context.needsStop && !context.runningObserved);
        CHECK(context.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
              context.commandEvidenceMask == 7 && context.latest.statusword == 0x8033 &&
              context.latest.monitor == 0x0001 && context.stopRequestedUs == context.servicedUs);
    }
};

void preparationBoundaries() {
    for (int64_t rate :
         {INT64_MIN, int64_t(-3001), int64_t(-1), int64_t(0), int64_t(3001), INT64_MAX}) {
        Rig r;
        r.request.velocity.value = M::Rational(rate);
        r.reject();
    }
    for (int64_t rate : {int64_t(1), int64_t(3000), int64_t(-1), int64_t(-3000)}) {
        Rig r;
        r.evidence.reverseQualified = true;
        r.evidence.maximumReverseRpm = 3000;
        r.request.velocity.value = M::Rational(rate);
        CHECK(r.prepare());
        CHECK(r.context.rate.effectiveNative == rate &&
              r.context.rate.quantization.errorNumerator == 0);
    }
    for (unsigned bad = 0; bad < 24; ++bad) {
        Rig r;
        switch (bad) {
        case 0:
            r.axis.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
            break;
        case 1:
            r.axis.nativeVelocityTime = M::TimeUnit::SECOND;
            break;
        case 2:
            r.axis.velocityRepresentation = M::NativeRepresentation::BINARY32;
            break;
        case 3:
            ++r.request.velocity.configurationGeneration;
            break;
        case 4:
            ++r.axis.binding.configurationGeneration;
            break;
        case 5:
            ++r.axis.binding.bindingGeneration;
            break;
        case 6:
            ++r.identity.target.nodeGeneration;
            break;
        case 7:
            ++r.identity.firmwareCode;
            break;
        case 8:
            r.evidence.gate = L::VelocityGate::CLOSED;
            break;
        case 9:
            r.evidence.startQualified = false;
            break;
        case 10:
            r.evidence.stopQualified = false;
            break;
        case 11:
            r.evidence.record = 0;
            break;
        case 12:
            r.evidence.layout = L::StatusLayout::UNSPECIFIED;
            break;
        case 13:
            r.evidence.maximumForwardRpm = 0;
            break;
        case 14:
            r.evidence.maximumForwardRpm = 3001;
            break;
        case 15:
            r.request.durationUs = 0;
            break;
        case 16:
            r.request.durationUs = DEADLINE - START;
            break;
        case 17:
            r.options.maxPolls = 49;
            break;
        case 18:
            r.options.pollIntervalUs = 0;
            break;
        case 19:
            r.options.requiredStopBudgetUs = 0;
            break;
        case 20:
            r.options.requiredStopBudgetUs = 1599;
            break;
        case 21:
            r.profile.rampUpTimeMs = 0;
            break;
        case 22:
            r.profile.rampDownTimeMs = 2001;
            break;
        default:
            r.options.commandIntervalUs = 9999;
            break;
        }
        r.reject();
    }
    {
        Rig r;
        r.options.maxPolls = 50; // Exact duration coverage is sufficient.
        CHECK(r.prepare());
    }
    {
        Rig r;
        r.reject(START + DURATION + r.options.requiredStopBudgetUs - 1);
        r.reject(START);
        r.reject(UINT64_MAX);
        r.request.durationUs = UINT64_MAX;
        r.reject();
    }
    {
        Rig r;
        r.request.velocity.value = M::Rational(6001, 2);
        r.request.velocity.rounding = M::Rounding::TOWARD_ZERO;
        r.request.velocity.maximumQuantizationError = M::Rational(1, 2);
        r.reject(); // Requested3000.5 must not round into the accepted3000 boundary.
        r.request.velocity.value = M::Rational(241, 2);
        CHECK(r.prepare());
        CHECK(r.context.rate.effectiveNative == 120 &&
              r.context.rate.quantization.errorNumerator == 1 &&
              r.context.rate.quantization.errorDenominator == 2);
    }
    {
        Rig r;
        r.mode = 1;
        r.acquireInitial();
        r.reject(); // Separate mode-change evidence is required even while released.
        r.evidence.modeChangeQualified = true;
        r.reject(); // A mode-0 transition record does not cover the observed mode 1.
        r.evidence.modeChangeFromRaw = 6;
        r.reject();
        r.evidence.modeChangeFromRaw = 1;
        CHECK(r.prepare() && r.context.phase == L::VelocityPhase::MODE);
        r.control = 15;
        r.acquireInitial();
        r.reject(); // Mode-change qualification never allows an enabled mode switch.
    }
    {
        Rig r;
        CHECK(r.prepare());
        const Saved<L::VelocityContext> before(r.context);
        CHECK(!L::prepareVelocity(r.context, r.context.reservation, r.target, r.initial, r.identity,
                                  r.evidence, r.request, r.profile, r.axis, 42, r.now, DEADLINE,
                                  r.options));
        before.unchanged(r.context);
        CHECK(L::validateVelocityAdmission(r.context, r.live, r.target, r.axis, START));
        ++r.axis.binding.configurationGeneration;
        CHECK(!L::validateVelocityAdmission(r.context, r.live, r.target, r.axis, START));
        before.unchanged(r.context);
    }
}

void timingAndRetainedFailure() {
    for (uint64_t offset : {uint64_t(0), uint64_t(10000), uint64_t(10001)}) {
        Rig r;
        r.reachRunning();
        const uint64_t due = r.context.stopDueUs;
        bool stop = true;
        CHECK(L::serviceVelocity(r.context, r.live, r.target, due - 1, false, stop));
        CHECK(!stop && !r.context.stopRequested);
        CHECK(L::serviceVelocity(r.context, r.live, r.target, due + offset, false, stop));
        CHECK(stop && r.context.stopRequested && r.context.phase == L::VelocityPhase::STOPPING);
        CHECK(r.context.firstFailure ==
              (offset > 10000 ? M::ActionOutcome::SERVICE_TIMEOUT : M::ActionOutcome::NONE));
        CHECK(r.context.stopDueUs == due && r.context.deadlineUs == DEADLINE);
    }
    {
        Rig r;
        r.reachRunning();
        const Saved<L::VelocityContext> before(r.context);
        bool stop = true;
        CHECK(!L::serviceVelocity(r.context, r.live, r.target, r.context.stopDueUs, true, stop));
        CHECK(stop);
        before.unchanged(r.context);
        CHECK(!L::serviceVelocity(r.context, r.live, r.target, START - 1, false, stop));
        before.unchanged(r.context);
    }
    {
        Rig r;
        CHECK(r.prepare());
        L::PreparedVelocity first = r.next(START);
        CHECK(first.ready && first.transfer.index == 0x60ff && first.transfer.size == 4);
        CHECK(first.transfer.data[0] == 120 && first.transfer.data[1] == 0 &&
              first.transfer.data[2] == 0 && first.transfer.data[3] == 0);
        bool stop = true;
        CHECK(L::serviceVelocity(r.context, r.live, r.target, START + DURATION, false, stop));
        CHECK(!stop && r.context.state == M::ActionState::FAILED && !r.context.needsStop);
        L::PreparedVelocity after = r.next(START + DURATION);
        CHECK(!after.ready && !after.observe); // Setup consumed the run; no fresh start.
    }
    {
        Rig r;
        r.reachRunning();
        bool stop = false;
        CHECK(L::serviceVelocity(r.context, r.live, r.target, DEADLINE, false, stop));
        CHECK(r.context.state == M::ActionState::FAILED && r.context.needsStop &&
              r.context.outcome == M::ActionOutcome::DEADLINE);
        const Saved<L::VelocityContext> terminal(r.context);
        CHECK(L::serviceVelocity(r.context, r.live, r.target, DEADLINE + 1, false, stop));
        terminal.unchanged(r.context);
        CHECK(!r.next(DEADLINE + 1).ready);
    }
    {
        Rig r;
        r.reachRunning();
        CHECK(L::advanceVelocity(r.context, r.observation, r.live, r.target,
                                 M::VelocityInterruption::CANCELLED, r.now));
        CHECK(r.context.state == M::ActionState::FAILED && r.context.needsStop &&
              !r.context.stopRequested && r.context.outcome == M::ActionOutcome::CANCELLED);
        const Saved<L::VelocityContext> terminal(r.context);
        bool stop = false;
        CHECK(L::serviceVelocity(r.context, r.live, r.target, r.context.stopDueUs, false, stop));
        CHECK(!stop);
        terminal.unchanged(r.context);
        CHECK(!r.next(r.context.stopDueUs).ready);
    }
}
void definiteRetirementRefinesOnlyItsOwnNotice() {
    for (uint16_t word : {uint16_t(6), uint16_t(7)}) {
        Rig r;
        const L::PreparedVelocity work = r.reachWord(word);
        CHECK(L::noteVelocityTransmission(r.context, r.live, r.target, r.axis, nullptr, 0, r.now));
        CHECK(r.context.needsStop); // Admission has not yet proved whether this word left.
        C::SdoCompletion retired = reply(work.transfer, r.now, r.now + 1);
        retired.outcome = C::SdoOutcome::TIMEOUT;
        retired.protocolAcknowledged = retired.transmissionPossible = retired.effectPossible =
            false;
        CHECK(L::advanceVelocity(r.context, r.observation, r.live, r.target, retired, r.now + 1));
        CHECK(r.context.needsStop == (word == 7));
        CHECK(r.context.stopRequested == (word == 7));
        CHECK(r.context.state == (word == 6 ? M::ActionState::FAILED : M::ActionState::ACTIVE));
        CHECK(r.context.firstFailure == M::ActionOutcome::SERVICE_TIMEOUT);
        CHECK(r.context
                  .effectsPossible); // Earlier acknowledged parameters remain independent effects.
        CHECK(r.context.uncertain == (word == 7));
        if (word == 7)
            CHECK(r.context.phase == L::VelocityPhase::STOPPING &&
                  r.context.commandHistory[0].transmissionPossible);
        CHECK(!r.next(r.now + 1).ready);
    }
    // Local cancellation is still local, including when an earlier word genuinely may have left.
    for (uint16_t word : {uint16_t(6), uint16_t(7)}) {
        Rig r;
        const L::PreparedVelocity work = r.reachWord(word);
        CHECK(L::noteVelocityTransmission(r.context, r.live, r.target, r.axis, nullptr, 0, r.now));
        CHECK(L::noteVelocityWithdrawal(r.context, r.live, r.target,
                                        M::VelocityInterruption::CANCELLED, r.now));
        C::SdoCompletion retired = reply(work.transfer, r.now, r.now + 1);
        retired.outcome = C::SdoOutcome::CANCELLED;
        retired.protocolAcknowledged = retired.transmissionPossible = retired.effectPossible =
            false;
        CHECK(L::advanceVelocity(r.context, r.observation, r.live, r.target, retired, r.now + 1));
        CHECK(r.context.state == M::ActionState::FAILED &&
              r.context.outcome == M::ActionOutcome::CANCELLED);
        CHECK(r.context.needsStop == (word == 7) && !r.context.stopRequested);
        CHECK(!r.next(r.now + 1).ready);
    }
}
void finalEnableCannotOutliveDuration() {
    {
        Rig r;
        const L::PreparedVelocity prepared = r.reachWord(15);
        const uint64_t due = r.context.stopDueUs;
        CHECK(prepared.transfer.deadlineUs == due && r.context.deadlineUs == DEADLINE);
        CHECK(r.next(due - 200).ready); // Exact queue+response coverage remains admissible.
        CHECK(L::validateVelocityAdmission(r.context, r.live, r.target, r.axis, due - 200));
        for (uint64_t at : {due - 199, due - 1, due, due + 1}) {
            const L::PreparedVelocity late = r.next(at);
            CHECK(!late.ready && !late.observe);
            CHECK(!L::validateVelocityAdmission(r.context, r.live, r.target, r.axis, at));
        }
    }
    for (bool possible : {false, true}) {
        Rig r;
        r.reachWord(15);
        r.now = r.context.stopDueUs - 200;
        const L::PreparedVelocity prepared = r.next(r.now);
        CHECK(prepared.ready && prepared.transfer.deadlineUs == r.context.stopDueUs);
        CHECK(L::noteVelocityTransmission(r.context, r.live, r.target, r.axis, nullptr, 0, r.now));
        C::SdoCompletion expired = reply(prepared.transfer, r.now, r.context.stopDueUs);
        expired.outcome = C::SdoOutcome::TIMEOUT;
        expired.protocolAcknowledged = false;
        expired.transmissionPossible = expired.effectPossible = expired.effectUncertain = possible;
        CHECK(L::advanceVelocity(r.context, r.observation, r.live, r.target, expired,
                                 r.context.stopDueUs));
        CHECK(r.context.commandHistory[2].transfer.deadlineUs == r.context.stopDueUs);
        CHECK(r.context.execution ==
              (possible ? M::ActionExecution::UNKNOWN : M::ActionExecution::NOT_TRANSMITTED));
        CHECK(r.context.firstFailure == M::ActionOutcome::SERVICE_TIMEOUT && r.context.needsStop &&
              r.context.stopRequested && r.context.phase == L::VelocityPhase::STOPPING);
        CHECK(r.context.deadlineUs == DEADLINE && !r.next(r.context.stopDueUs).ready);
    }
}
void bestEffortSettlementRequiresQuickStopPolicy() {
    for (unsigned quick = 0; quick < 2; ++quick) {
        Rig r;
        r.evidence.stopKind = r.options.stopKind =
            quick ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
        r.reachRunning();
        bool due = false;
        CHECK(L::serviceVelocity(r.context, r.live, r.target, r.context.stopDueUs, false, due) &&
              due);
        L::BestEffortStop stop;
        CHECK(L::prepareBestEffortStop(stop, r.target, r.context.operation + 1, r.context.stopDueUs,
                                       r.context.deadlineUs, r.options.service));
        CHECK(L::advanceBestEffortStop(stop, M::ActionOutcome::CANCELLED, r.context.stopDueUs + 1));
        const Saved<L::VelocityContext> saved(r.context);
        const M::Status result = L::advanceVelocityStop(r.context, stop, r.context.stopDueUs + 1);
        if (quick) {
            CHECK(result && r.context.state == M::ActionState::FAILED && r.context.needsStop);
            CHECK(r.context.stopExecution == M::ActionExecution::NOT_TRANSMITTED);
        } else {
            CHECK(!result);
            saved.unchanged(r.context); // A well-formed 000B result cannot settle a HALT request.
        }
    }
}
void exactPollCoverageIncludesTheWholeDuration() {
    for (uint8_t startMode : {uint8_t(3), uint8_t(1)}) {
        Rig r;
        r.options.maxPolls = uint32_t(DURATION / r.options.pollIntervalUs);
        r.mode = startMode;
        r.acquireInitial();
        if (startMode != 3) {
            r.evidence.modeChangeQualified = true;
            r.evidence.modeChangeFromRaw = startMode;
        }
        CHECK(r.prepare());
        bool stopDue = false;
        // Zero-latency completions are valid supplied evidence. Slow fake replies must not hide
        // an observation-budget failure that would stop an otherwise successful run early.
        for (unsigned step = 0; step < 2000 && !stopDue; ++step) {
            CHECK(r.context.firstFailure == M::ActionOutcome::NONE);
            CHECK(L::serviceVelocity(r.context, r.live, r.target, r.now, false, stopDue));
            if (stopDue)
                break;
            const L::PreparedVelocity work = r.next(r.now);
            if (work.observe) {
                CHECK(
                    L::beginVelocityObservation(r.context, r.observation, r.live, r.target, r.now));
                continue;
            }
            if (!work.ready) {
                r.now = work.eligibleUs > r.now ? work.eligibleUs : r.context.stopDueUs;
                continue;
            }
            if (work.transfer.direction == C::SdoDirection::DOWNLOAD) {
                CHECK(L::noteVelocityTransmission(r.context, r.live, r.target, r.axis, nullptr, 0,
                                                  r.now));
                if (work.transfer.index == 0x6040)
                    r.control =
                        uint16_t(work.transfer.data[0] | (uint16_t(work.transfer.data[1]) << 8));
                else if (work.transfer.index == 0x6060)
                    r.mode = work.transfer.data[0];
            }
            CHECK(L::advanceVelocity(
                r.context, r.observation, r.live, r.target,
                reply(work.transfer, r.now, r.now, r.value(work.transfer.index)), r.now));
        }
        CHECK(stopDue && r.now == START + DURATION);
        CHECK(r.context.firstFailure == M::ActionOutcome::NONE &&
              r.context.state == M::ActionState::ACTIVE && r.context.runningObserved &&
              r.context.phase == L::VelocityPhase::STOPPING && r.context.needsStop);
        CHECK(r.context.startedUs == START && r.context.stopDueUs == START + DURATION &&
              r.context.deadlineUs == DEADLINE);
        CHECK(r.context.modeEvidencePresent == (startMode != 3));
    }
    Rig zero;
    zero.options.maxPolls = 0;
    zero.reject(); // Reject before dividing the duration coverage bound by maxPolls.
    Rig wide;
    wide.options.maxPolls = UINT32_MAX;
    CHECK(wide.prepare()); // The maximum running budget must remain valid without arithmetic wrap.
    Rig overflow;
    overflow.options.maxPolls = UINT32_MAX;
    overflow.options.pollIntervalUs = uint64_t(INT64_MAX) / UINT32_MAX + 1;
    overflow.reject();
}
// ATTENDED_EXPERIMENT admits one bounded run whose start and stop are measured, not claimed. The
// context keeps the gate, no qualification flag can ride along, and every other refusal stays.
void experiment(Rig& r) {
    r.evidence.gate = L::VelocityGate::ATTENDED_EXPERIMENT;
    r.evidence.startQualified = r.evidence.stopQualified = false;
    r.evidence.maximumForwardRpm = L::VELOCITY_EXPERIMENT_MAXIMUM_RPM;
    r.request.velocity.value = M::Rational(int64_t(L::VELOCITY_EXPERIMENT_MAXIMUM_RPM));
}
void attendedExperimentScope() {
    {
        Rig r;
        experiment(r);
        r.reachRunning();
        CHECK(r.context.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT &&
              !r.context.evidence.startQualified && !r.context.evidence.stopQualified &&
              !r.context.evidence.modeChangeQualified && !r.context.evidence.reverseQualified);
        CHECK(r.context.rate.effectiveNative == 60 && r.context.phase != L::VelocityPhase::MODE &&
              !r.context.modeEvidencePresent);
        bool stop = false;
        CHECK(L::serviceVelocity(r.context, r.live, r.target, r.context.stopDueUs, false, stop));
        CHECK(stop && r.context.stopRequested && r.context.phase == L::VelocityPhase::STOPPING &&
              r.context.needsStop && r.context.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT);
    }
    for (unsigned bad = 0; bad < 13; ++bad) {
        Rig r;
        experiment(r);
        M::Err code = M::Err::INVALID_CONFIG;
        M::VelocityError detail = M::VelocityError::ARGUMENT;
        switch (bad) {
        case 0:
            r.evidence.startQualified = true; // A claim the experiment exists to measure.
            break;
        case 1:
            r.evidence.stopQualified = true;
            break;
        case 2:
            r.evidence.startQualified = r.evidence.stopQualified = true;
            break;
        case 3:
            r.evidence.modeChangeQualified = true; // Even while the drive already reads mode 3.
            r.evidence.modeChangeFromRaw = 1;
            break;
        case 4:
            r.evidence.reverseQualified = true;
            r.evidence.maximumReverseRpm = 60;
            break;
        case 5:
            r.evidence.maximumForwardRpm = L::VELOCITY_EXPERIMENT_MAXIMUM_RPM + 1;
            break;
        case 6:
            r.request.velocity.value = M::Rational(int64_t(L::VELOCITY_EXPERIMENT_MAXIMUM_RPM + 1));
            code = M::Err::ILLEGAL_VALUE; // The evidence's own range, unchanged.
            detail = M::VelocityError::RANGE;
            break;
        case 7:
            r.evidence.gate = L::VelocityGate(3);
            break;
        case 8:
            r.evidence.record = 0;
            break;
        case 9:
            r.mode = 1; // No mode change is claimed, so mode 1 cannot be the start.
            r.acquireInitial();
            code = M::Err::UNSUPPORTED;
            detail = M::VelocityError::CAPABILITY;
            break;
        case 10:
            r.control = 15; // An enabled drive is never an admissible start.
            r.acquireInitial();
            detail = M::VelocityError::STATE;
            break;
        case 11:
            ++r.identity.firmwareCode;
            code = M::Err::UNSUPPORTED;
            detail = M::VelocityError::CAPABILITY;
            break;
        default:
            r.request.velocity.value = M::Rational(-60); // Reverse is never claimed.
            code = M::Err::ILLEGAL_VALUE;
            detail = M::VelocityError::RANGE;
            break;
        }
        const M::Status status = r.prepare();
        CHECK(!status && status.code == code && status.detail == int32_t(detail));
        r.reject();
    }
    {
        // The duration bound: exactly 20 s is admitted, one microsecond more is not, with the deadline
        // and running budget wide enough that only the experiment's bound decides. MEASURED has none.
        const uint64_t longest = L::VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US;
        for (unsigned variant = 0; variant < 3; ++variant) {
            Rig r;
            if (variant < 2)
                experiment(r);
            r.options.maxPolls = 3000;
            r.request.durationUs = longest + (variant ? 1 : 0);
            const M::Status status = r.prepare(START + longest + 5000000);
            if (variant == 1) {
                CHECK(!status && status.code == M::Err::INVALID_CONFIG &&
                      status.detail == int32_t(M::VelocityError::TIME));
                r.reject(START + longest + 5000000);
            } else {
                CHECK(status && r.context.stopDueUs == START + r.request.durationUs);
            }
        }
    }
    {
        // A MEASURED record keeps its own refusals: an experiment never stands in for it.
        Rig r;
        r.evidence.startQualified = false;
        const M::Status status = r.prepare();
        CHECK(!status && status.code == M::Err::UNSUPPORTED &&
              status.detail == int32_t(M::VelocityError::CAPABILITY));
        r.reject();
    }
}
L::ActionOptions stopOptions(const Rig& r) {
    L::ActionOptions value;
    value.service = r.options.service;
    value.observationIntervalUs = 1000;
    value.maximumAgeUs = 1000000;
    return value;
}
// One prepared stop through its command and MONITORS+STATUS cycles reporting monitor and status.
void runStop(Rig& r, L::ActionContext& stop, uint16_t monitor, uint16_t status) {
    for (unsigned guard = 0; guard < 40 && stop.state == M::ActionState::ACTIVE; ++guard) {
        L::PreparedAction work;
        CHECK(L::nextAction(stop, r.live, r.target, r.now, work));
        if (!work.ready) {
            r.now = work.eligibleUs > r.now ? work.eligibleUs : r.now + 100;
            continue;
        }
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD)
            CHECK(L::noteActionTransmission(stop, r.live, r.target, r.now));
        const uint16_t index = work.transfer.index;
        const uint32_t value = index == 0x2100 ? monitor : index == 0x6041 ? status : index == 0x6061 ? 3u : 0u;
        CHECK(L::advanceAction(stop, r.live, r.target, reply(work.transfer, r.now, r.now + 1, value),
                               r.now + 1));
        r.now += 100;
    }
}
// The stop that follows the run's own acknowledged final 000F, from any report (trial 1009, where the
// fresh-state admission refused it and nothing was sent), and the record of a stop never composed.
// Backlog S17: an explicit final-enable report window, the application's bounded wait, admitted only
// under ATTENDED_EXPERIMENT. 100 ms with cycles 20 ms apart: at most five cycles more.
void window(Rig& r, uint64_t length = 100000, uint64_t spacing = 20000) {
    experiment(r);
    r.options.enableWindowUs = length;
    r.options.enableSpacingUs = spacing;
}
void refusedWindow(Rig& r, M::VelocityError detail) {
    r.context.operation = 0xbeef;
    const Saved<L::VelocityContext> before(r.context);
    const M::Status s = r.prepare();
    CHECK(!s && s.code == M::Err::INVALID_CONFIG && s.detail == int32_t(detail));
    before.unchanged(r.context);
}
void enableWindowAdmission() {
    {
        // Outside the experiment no record establishes the lag, so a window is refused.
        Rig r;
        r.options.enableWindowUs = 100000;
        r.options.enableSpacingUs = 20000;
        refusedWindow(r, M::VelocityError::ARGUMENT);
        CHECK(std::strcmp(r.prepare().msg, "an enable report window belongs to an attended velocity experiment") ==
              0);
        r.options.enableWindowUs = r.options.enableSpacingUs = 0;
        CHECK(r.prepare()); // The same record without a window.
    }
    struct Bad {
        uint64_t length, spacing, duration;
    };
    const Bad bad[] = {
        {0, 20000, DURATION},       // A spacing without a window.
        {100000, 0, DURATION},      // A window without a spacing.
        {100000, 100001, DURATION}, // Spacing beyond the window.
        {170000, 10000, DURATION},  // 17 cycles, over the 16.
        // Longer than any experiment.
        {L::VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US + 16, L::VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US / 16 + 1,
         L::VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US},
        {480000, 30000, DURATION}, // 27 services, the window and two word intervals exceed 500 ms.
    };
    for (const Bad& b : bad) {
        Rig r;
        window(r, b.length, b.spacing);
        r.request.durationUs = b.duration;
        refusedWindow(r, M::VelocityError::TIME);
    }
    {
        // The largest admitted shapes: 16 cycles, and a window that leaves exactly the two word intervals:
        // 5400 us of setup services, 474600 us of window and 2 x 10000 us make the 500000 us duration.
        Rig r;
        window(r, 160000, 10000);
        CHECK(r.prepare());
        Rig edge;
        window(edge, 474600, 474600 / 16);
        CHECK(edge.prepare());
        Rig over;
        window(over, 474601, 474601 / 16);
        refusedWindow(over, M::VelocityError::TIME);
    }
}
// Serve until the context requests its own stop, ends, or (when asked) reports running.
void serveUntil(Rig& r, bool running) {
    for (unsigned step = 0; step < 2000 && r.context.phase != L::VelocityPhase::STOPPING &&
                            r.context.state == M::ActionState::ACTIVE && !(running && r.context.runningObserved);
         ++step)
        r.serveOne();
}
// Trial 1308's shape with a window: 8033/0001 for three cycles after 15, then C437/0004 with 606C
// positive. The run waits, runs, requests its HALT at due and settles it, because a running report
// preceded the stop.
void enableWindowThenRunning() {
    Rig r;
    window(r);
    r.lagCycles = 3;
    r.runningStatus = 0xc437;
    CHECK(r.prepare());
    serveUntil(r, true);
    const L::VelocityContext& c = r.context;
    CHECK(c.state == M::ActionState::ACTIVE && c.runningObserved && c.needsStop && !c.stopRequested);
    CHECK(c.enablePending == 3 && c.pendingFirst.present && c.pendingFirst.cycle == 3 &&
          c.pendingFirst.statusword == 0x8033 && c.pendingLast.cycle == 5 && c.pendingLast.monitor == 0x0001);
    CHECK(c.enableReport.present && c.enableReport.cycle == 6 && c.enableReport.statusword == 0xc437 &&
          c.enableReport.velocity == int32_t(L::VELOCITY_EXPERIMENT_MAXIMUM_RPM) && c.observations == 6 &&
          !c.runningPolls);
    const uint64_t ack = c.commandHistory[2].completedUs;
    CHECK(c.pendingFirst.earliestUs >= ack && c.pendingLast.earliestUs >= c.pendingFirst.latestUs + 2 * 20000 &&
          c.enableReport.earliestUs >= c.pendingLast.latestUs + 20000 && c.enableReport.earliestUs - ack <= 100000);
    // Running cycles then follow as without a window.
    while (r.context.runningPolls < 3)
        r.serveOne();
    CHECK(r.context.runningObserved && r.context.state == M::ActionState::ACTIVE &&
          r.context.observations == 6 + r.context.runningPolls);
    // A context whose window count was overstated is refused, as is a window under another gate.
    L::VelocityContext forged = r.context;
    L::PreparedVelocity work;
    forged.enablePending = 100000 / 20000 + 2;
    CHECK(!L::nextVelocity(forged, r.observation, r.live, r.target, r.now, work));
    forged = r.context;
    forged.evidence.gate = L::VelocityGate::MEASURED;
    forged.evidence.startQualified = forged.evidence.stopQualified = true;
    CHECK(!L::nextVelocity(forged, r.observation, r.live, r.target, r.now, work));
    forged = r.context;
    forged.observations = forged.runningPolls + 3 + 100000 / 20000 + 1;
    CHECK(!L::nextVelocity(forged, r.observation, r.live, r.target, r.now, work));
    forged.observations = forged.runningPolls + 3 + 100000 / 20000;
    CHECK(L::nextVelocity(forged, r.observation, r.live, r.target, r.now, work));
    bool stop = false;
    r.now = r.context.stopDueUs;
    CHECK(L::serviceVelocity(r.context, r.live, r.target, r.now, false, stop));
    CHECK(stop && r.context.stopRequested && r.context.phase == L::VelocityPhase::STOPPING);
    static L::ActionContext halt;
    CHECK(L::prepareVelocityStop(halt, r.context, r.live, r.target, r.context.commandHistory[2],
                                 L::StopKind::HALT, 43, r.now, DEADLINE, stopOptions(r)));
    runStop(r, halt, 0x0001, 0x9437);
    CHECK(halt.state == M::ActionState::SUCCEEDED && halt.completion == M::ActionCompletion::OBSERVED);
    CHECK(L::advanceVelocityStop(r.context, halt, r.now));
    CHECK(r.context.state == M::ActionState::SUCCEEDED && r.context.outcome == M::ActionOutcome::OBSERVED &&
          r.context.completion == M::ActionCompletion::OBSERVED && !r.context.needsStop &&
          r.context.firstFailure == M::ActionOutcome::NONE);
}
// 8033 to the end of the window: each cycle waits one spacing; the cycle after which no further one
// fits requests the run's own stop, once, and nothing more is yielded.
void enableWindowExpires() {
    Rig r;
    window(r);
    r.lag = true;
    CHECK(r.prepare());
    unsigned stops = 0;
    for (unsigned step = 0; step < 2000 && r.context.state == M::ActionState::ACTIVE &&
                            r.context.phase != L::VelocityPhase::STOPPING;
         ++step) {
        r.serveOne();
        stops += r.context.stopRequested;
    }
    const L::VelocityContext& c = r.context;
    const uint64_t ack = c.commandHistory[2].completedUs;
    CHECK(stops == 1 && c.state == M::ActionState::ACTIVE && c.phase == L::VelocityPhase::STOPPING &&
          c.stopRequested && c.needsStop && !c.runningObserved && !c.enableReport.present);
    CHECK(c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
          std::strcmp(c.status.msg, "final enable not reported within its window") == 0);
    // Cycles 0..4 fit 100 ms at about 20.7 ms each; a sixth would be eligible after the window.
    CHECK(c.enablePending == 5 && c.observations == 2 + c.enablePending && c.pendingLast.cycle == c.observations &&
          c.pendingLast.earliestUs - ack <= 100000 && c.servicedUs + 20000 - ack > 100000);
    CHECK(c.stopRequestedUs == c.servicedUs && c.latest.cycle == c.pendingLast.cycle);
    L::PreparedVelocity work;
    CHECK(L::nextVelocity(c, r.observation, r.live, r.target, r.now + 1000, work) && !work.ready && !work.observe);
    // The run's stop follows its acknowledged 000F as without a window.
    static L::ActionContext halt;
    CHECK(L::prepareVelocityStop(halt, r.context, r.live, r.target, r.context.commandHistory[2],
                                 L::StopKind::HALT, 43, r.now, DEADLINE, stopOptions(r)));
}
// Inside the window any other report fails at once with the run's own stop requested.
void enableWindowContradicted() {
    for (unsigned k = 0; k < 6; ++k) {
        Rig r;
        window(r);
        r.lag = true;
        CHECK(r.prepare());
        for (unsigned step = 0; step < 2000 && !r.context.enablePending; ++step)
            r.serveOne();
        CHECK(r.context.enablePending == 1 && r.context.phase == L::VelocityPhase::WAITING &&
              r.context.observation == L::VelocityObservation::ENABLE);
        switch (k) {
        case 0: r.lagVelocity = 2; break;     // 606C moving.
        case 1: r.lagPosition = 1; break;     // 6064 left word 7's cycle's.
        case 2: r.lagMonitor = 0x0011; break; // Released again.
        case 3: r.lagMonitor = 0x0005; break; // 2100 running without the enable.
        case 4: r.lagStatus = 0x8031; break;  // Low bits 001.
        default: r.lagMonitor = 0x0009; break; // Alarm.
        }
        serveUntil(r, false);
        const L::VelocityContext& c = r.context;
        CHECK(c.phase == L::VelocityPhase::STOPPING && c.stopRequested && c.needsStop && c.enablePending == 1 &&
              c.observations == 4 && !c.enableReport.present);
        CHECK(c.firstFailure == (k == 5 ? M::ActionOutcome::DRIVE_FAULT : M::ActionOutcome::OBSERVATION_SCOPE));
        if (k < 5)
            CHECK(std::strcmp(c.status.msg, "final enable state is not confirmed") == 0);
    }
}
void runStopFollowsTheAcknowledgedFinalWord() {
    {
        Rig r;
        r.reachLaggingFailure();
        const C::SdoCompletion control = r.context.commandHistory[2];
        const L::ActionOptions o = stopOptions(r);
        static L::ActionContext stop;
        const Saved<L::VelocityContext> run(r.context);
        CHECK(L::validateVelocityStop(stop, r.context, r.live, r.target, control, L::StopKind::HALT,
                                      43, r.now, DEADLINE, o));
        // Only the run's own kind, deadline and word, a newer identity and valid budgets.
        C::SdoCompletion later = control;
        ++later.completedUs;
        L::ActionOptions noAge = o;
        noAge.maximumAgeUs = 0;
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, control,
                                       L::StopKind::QUICK_STOP, 43, r.now, DEADLINE, o));
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, control, L::StopKind::HALT,
                                       43, r.now, DEADLINE - 1, o));
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, control, L::StopKind::HALT,
                                       42, r.now, DEADLINE, o));
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, later, L::StopKind::HALT,
                                       43, r.now, DEADLINE, o));
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, r.context.commandHistory[1],
                                       L::StopKind::HALT, 43, r.now, DEADLINE, o));
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, control, L::StopKind::HALT,
                                       43, r.now, DEADLINE, noAge));
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, control, L::StopKind::HALT,
                                       43, r.context.stopRequestedUs - 1, DEADLINE, o));
        run.unchanged(r.context);
        CHECK(L::prepareVelocityStop(stop, r.context, r.live, r.target, control, L::StopKind::HALT, 43,
                                     r.now, DEADLINE, o));
        run.unchanged(r.context); // Preparation writes only the stop.
        CHECK(stop.state == M::ActionState::ACTIVE && stop.actionKind == M::ActionKind::STOP &&
              stop.kind == L::StopKind::HALT && stop.stopModeRaw == 3 && stop.operation == 43 &&
              stop.predecessorWord == 0x000f && !stop.predecessorDocumented &&
              stop.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION &&
              stop.startedUs == r.now && stop.deadlineUs == DEADLINE && !stop.observationKnown);
        L::PreparedAction command;
        CHECK(L::nextAction(stop, r.live, r.target, r.now, command) && command.ready);
        CHECK(command.transfer.index == 0x6040 && command.transfer.size == 2 &&
              command.transfer.data[0] == 0x0f && command.transfer.data[1] == 0x01);
        // The drive keeps word 7's report: the stop sees nothing running, which settles nothing here.
        runStop(r, stop, 0x0001, 0x8033);
        CHECK(stop.state == M::ActionState::SUCCEEDED && stop.completion == M::ActionCompletion::OBSERVED);
        CHECK(L::advanceVelocityStop(r.context, stop, r.now));
        CHECK(r.context.state == M::ActionState::FAILED &&
              r.context.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
              r.context.completion == M::ActionCompletion::NOT_OBSERVED && r.context.needsStop &&
              r.context.uncertain && r.context.stopOutcome == M::ActionOutcome::OBSERVED &&
              r.context.stopExecution == M::ActionExecution::ACKNOWLEDGED && r.context.stopOperation == 43);
        // The ended run takes an application stop of its own kind and deadline, never after another word.
        static L::ActionContext second;
        CHECK(L::validateVelocityStop(second, r.context, r.live, r.target, control,
                                      L::StopKind::QUICK_STOP, 44, r.now, r.now + 5000, o));
        CHECK(!L::validateVelocityStop(second, r.context, r.live, r.target, stop.commandHistory[0],
                                       L::StopKind::QUICK_STOP, 44, r.now, r.now + 5000, o));
    }
    {
        // After a running report the same stop can settle the run: its not-running report follows
        // one that showed the drive running.
        Rig r;
        r.reachRunning();
        bool due = false;
        CHECK(L::serviceVelocity(r.context, r.live, r.target, r.context.stopDueUs, false, due) && due);
        r.now = r.context.stopDueUs;
        static L::ActionContext stop;
        CHECK(L::prepareVelocityStop(stop, r.context, r.live, r.target, r.context.commandHistory[2],
                                     L::StopKind::HALT, 43, r.now, DEADLINE, stopOptions(r)));
        runStop(r, stop, 0x0000, 0x0007);
        CHECK(L::advanceVelocityStop(r.context, stop, r.now));
        CHECK(r.context.state == M::ActionState::SUCCEEDED && !r.context.needsStop &&
              r.context.completion == M::ActionCompletion::OBSERVED);
    }
    {
        // A requested stop that could not be composed: recorded, nothing sent, the fence kept.
        Rig r;
        r.reachLaggingFailure();
        Saved<L::VelocityContext> requested(r.context);
        CHECK(!L::advanceVelocityStop(r.context, M::ActionOutcome::CANCELLED, r.now));
        CHECK(!L::advanceVelocityStop(r.context, M::ActionOutcome::DEADLINE, r.now));
        requested.unchanged(r.context);
        CHECK(L::advanceVelocityStop(r.context, M::ActionOutcome::INVALIDATED, r.now));
        CHECK(r.context.state == M::ActionState::FAILED && r.context.phase == L::VelocityPhase::STOPPING &&
              r.context.stopRequested && !r.context.stopOperation &&
              r.context.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
              r.context.outcome == M::ActionOutcome::OBSERVATION_SCOPE &&
              r.context.interruption == M::VelocityInterruption::NONE &&
              r.context.stopOutcome == M::ActionOutcome::INVALIDATED &&
              r.context.stopExecution == M::ActionExecution::NOT_TRANSMITTED && r.context.needsStop);
        const Saved<L::VelocityContext> terminal(r.context);
        CHECK(!L::advanceVelocityStop(r.context, M::ActionOutcome::INVALIDATED, r.now));
        terminal.unchanged(r.context);
        Rig late;
        late.reachLaggingFailure();
        CHECK(L::advanceVelocityStop(late.context, M::ActionOutcome::INVALIDATED, DEADLINE));
        CHECK(late.context.stopOutcome == M::ActionOutcome::DEADLINE);
        Rig idle;
        CHECK(idle.prepare());
        CHECK(!L::advanceVelocityStop(idle.context, M::ActionOutcome::INVALIDATED, START));
    }
    {
        // Word 15 never acknowledged: from 6 or 7 a 010F would be the first enable-operation word.
        Rig r;
        r.reachWord(15);
        r.now = r.context.stopDueUs - 200;
        const L::PreparedVelocity work = r.next(r.now);
        CHECK(L::noteVelocityTransmission(r.context, r.live, r.target, r.axis, nullptr, 0, r.now));
        C::SdoCompletion lost = reply(work.transfer, r.now, r.now + 150);
        lost.outcome = C::SdoOutcome::TIMEOUT;
        lost.protocolAcknowledged = false;
        lost.effectUncertain = true;
        CHECK(L::advanceVelocity(r.context, r.observation, r.live, r.target, lost, r.now + 150));
        r.now += 150;
        CHECK(r.context.phase == L::VelocityPhase::STOPPING && r.context.needsStop);
        static L::ActionContext stop;
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, r.context.commandHistory[1],
                                       L::StopKind::HALT, 43, r.now, DEADLINE, stopOptions(r)));
        CHECK(!L::validateVelocityStop(stop, r.context, r.live, r.target, r.context.commandHistory[2],
                                       L::StopKind::HALT, 43, r.now, DEADLINE, stopOptions(r)));
    }
}
// Backlog S18. One stop cycle as the drive reports it: 2100, 6041, 6061, 6064 and 606C.
struct Report {
    uint16_t monitor, status;
    uint8_t mode;
    int32_t position, velocity;
};
// The stop cycles of trials 1513 (node 2, 30 rpm) and 1607 (node 1, 60 rpm) in order. Each first cycle read
// 2100 0014, 6041 C050 and 6061 3 28 ms after 000B. That stop read no 6064 or 606C, so both cycles take
// those of the later reads 1514 and 1608 (2100 0015, 6041 C050, 6061 3, 606C 0), which give the second.
const Report TRIAL_1513[2] = {{0x0014, 0xc050, 3, 5608, 0}, {0x0015, 0xc050, 3, 5608, 0}};
const Report TRIAL_1607[2] = {{0x0014, 0xc050, 3, 11219, 0}, {0x0015, 0xc050, 3, 11219, 0}};
const Report HALTED = {0x0001, 0x9437, 3, 1131, 0}; // After a settled HALT: trials 1507, 1613, 1706.
// The bench plan gives the stop two cycles (4000 ms apart against 5000 ms after due). Here 300 ms apart
// against the rig's 499 ms after due does the same. The age cap covers `fields` fields.
L::ActionOptions planOptions(const Rig& r, uint64_t fields = 7) {
    L::ActionOptions value = stopOptions(r);
    value.observationIntervalUs = 300000;
    value.maximumAgeUs = fields * (uint64_t(r.options.service.queueTimeoutUs) + r.options.service.responseTimeoutUs);
    return value;
}
// Serves a prepared stop: cycle k answers with reports[k], the last repeated. At the deadline the caller's
// DEADLINE ends it. With limit, serving ends once that many cycles completed and the stop waits.
void runScripted(Rig& r, L::ActionContext& stop, const Report* reports, unsigned count, unsigned limit = 0) {
    unsigned cycle = 0;
    for (unsigned guard = 0; guard < 400 && stop.state == M::ActionState::ACTIVE; ++guard) {
        if (limit && stop.observations >= limit && stop.phase == L::ActionPhase::WAITING)
            return;
        if (r.now >= stop.deadlineUs) {
            CHECK(L::advanceAction(stop, r.live, r.target, M::ActionOutcome::DEADLINE, r.now));
            break;
        }
        L::PreparedAction work;
        CHECK(L::nextAction(stop, r.live, r.target, r.now, work));
        if (!work.ready) {
            r.now = work.eligibleUs > r.now ? work.eligibleUs : r.now + 100;
            continue;
        }
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD)
            CHECK(L::noteActionTransmission(stop, r.live, r.target, r.now));
        const uint16_t index = work.transfer.index;
        if (index == 0x2100)
            ++cycle;
        const Report& k = reports[cycle && cycle <= count ? cycle - 1 : cycle ? count - 1 : 0];
        const uint32_t value = index == 0x2100   ? k.monitor
                               : index == 0x6041 ? k.status
                               : index == 0x6061 ? k.mode
                               : index == 0x6064 ? uint32_t(k.position)
                               : index == 0x606c ? uint32_t(k.velocity)
                                                 : 0u;
        CHECK(L::advanceAction(stop, r.live, r.target, reply(work.transfer, r.now, r.now + 1, value), r.now + 1));
        r.now += 100;
    }
    CHECK(stop.state != M::ActionState::ACTIVE);
}
void quickStopRun(Rig& r) {
    experiment(r);
    r.evidence.stopKind = r.options.stopKind = L::StopKind::QUICK_STOP;
}
// The run's own stop at due after a running report, as the session composes it on the 000F.
void stopAtDue(Rig& r, L::ActionContext& stop, L::StopKind kind, const L::ActionOptions& o) {
    r.reachRunning();
    bool due = false;
    CHECK(L::serviceVelocity(r.context, r.live, r.target, r.context.stopDueUs, false, due) && due);
    r.now = r.context.stopDueUs;
    CHECK(L::prepareVelocityStop(stop, r.context, r.live, r.target, r.context.commandHistory[2], kind, 43, r.now,
                                 DEADLINE, o));
}
// A quick stop from a running mode 3 run settles on two released cycles in a row with 606C zero and 6064
// unchanged, running latched or not; anything else leaves the run fenced.
void quickStopSettlesOnReleasedStandstill() {
    struct Case {
        const char* name;
        Report reports[2];
        M::ActionOutcome outcome;
        uint32_t observations;
    };
    const Case cases[] = {
        {"1513", {TRIAL_1513[0], TRIAL_1513[1]}, M::ActionOutcome::OBSERVED, 2},
        {"1607", {TRIAL_1607[0], TRIAL_1607[1]}, M::ActionOutcome::OBSERVED, 2},
        {"6064 moves", {{0x0014, 0xc050, 3, 5600, 0}, {0x0015, 0xc050, 3, 5608, 0}}, M::ActionOutcome::DEADLINE, 2},
        {"606C nonzero", {{0x0014, 0xc050, 3, 5608, 3}, {0x0015, 0xc050, 3, 5608, 0}}, M::ActionOutcome::DEADLINE, 2},
        {"enabled first", {{0x0004, 0xc437, 3, 5608, 0}, {0x0015, 0xc050, 3, 5608, 0}}, M::ActionOutcome::DEADLINE, 2},
        {"alarm", {{0x0014, 0xc050, 3, 5608, 0}, {0x001d, 0xc050, 3, 5608, 0}}, M::ActionOutcome::OBSERVATION_SCOPE, 2},
        {"mode 1", {{0x0014, 0xc050, 3, 5608, 0}, {0x0015, 0xc050, 1, 5608, 0}}, M::ActionOutcome::OBSERVATION_SCOPE, 2},
        {"low bits 001", {{0x0014, 0xc051, 3, 5608, 0}, {0x0015, 0xc050, 3, 5608, 0}},
         M::ActionOutcome::OBSERVATION_SCOPE, 1},
    };
    for (const Case& k : cases) {
        Rig r;
        quickStopRun(r);
        static L::ActionContext stop;
        stopAtDue(r, stop, L::StopKind::QUICK_STOP, planOptions(r));
        CHECK(stop.standstillObserving && stop.stopModeRaw == 3 && stop.predecessorWord == 0x000f);
        L::PreparedAction command;
        CHECK(L::nextAction(stop, r.live, r.target, r.now, command) && command.ready);
        CHECK(command.transfer.index == 0x6040 && command.transfer.data[0] == 0x0b && command.transfer.data[1] == 0);
        runScripted(r, stop, k.reports, 2);
        if (stop.outcome != k.outcome || stop.observations != k.observations)
            std::fprintf(stderr, "%s: outcome %u observations %u\n", k.name, unsigned(stop.outcome),
                         unsigned(stop.observations));
        CHECK(stop.outcome == k.outcome && stop.observations == k.observations && stop.read.count == 7 &&
              stop.execution == M::ActionExecution::ACKNOWLEDGED);
        CHECK(L::advanceVelocityStop(r.context, stop, r.now));
        const L::VelocityContext& c = r.context;
        if (k.outcome == M::ActionOutcome::OBSERVED) {
            CHECK(stop.state == M::ActionState::SUCCEEDED && stop.completion == M::ActionCompletion::OBSERVED &&
                  stop.monitorObservation.monitor == 0x0015 &&
                  stop.extraObservation.actualPosition == k.reports[1].position);
            CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED &&
                  c.completion == M::ActionCompletion::OBSERVED && !c.needsStop &&
                  c.firstFailure == M::ActionOutcome::NONE && c.stopOperation == 43);
        } else {
            CHECK(stop.state == M::ActionState::FAILED && stop.completion == M::ActionCompletion::NOT_OBSERVED);
            CHECK(c.state == M::ActionState::FAILED && c.needsStop && c.uncertain &&
                  c.completion == M::ActionCompletion::NOT_OBSERVED && c.stopOutcome == k.outcome);
        }
    }
}
// Only a QUICK_STOP from 000F after a running report, with a seven-field age, gets the rule.
void releasedStandstillStaysNarrow() {
    const Report released[1] = {{0x0015, 0xc050, 3, 0, 0}};
    {
        // S16: the run failed on word 7's report after 15 (trial 1009's shape), so no running report
        // came before its QUICK_STOP, which then ends outside its scope on the release, fence kept.
        Rig r;
        r.evidence.stopKind = r.options.stopKind = L::StopKind::QUICK_STOP;
        r.reachLaggingFailure();
        static L::ActionContext stop;
        CHECK(L::prepareVelocityStop(stop, r.context, r.live, r.target, r.context.commandHistory[2],
                                     L::StopKind::QUICK_STOP, 43, r.now, DEADLINE, planOptions(r)));
        CHECK(!stop.standstillObserving);
        runScripted(r, stop, released, 1);
        CHECK(stop.outcome == M::ActionOutcome::OBSERVATION_SCOPE && stop.observations == 1 && stop.read.count == 5);
        CHECK(L::advanceVelocityStop(r.context, stop, r.now));
        CHECK(r.context.state == M::ActionState::FAILED && r.context.needsStop &&
              r.context.completion == M::ActionCompletion::NOT_OBSERVED);
    }
    {
        // A HALT keeps its rule: a release is outside its scope.
        Rig r;
        experiment(r);
        static L::ActionContext stop;
        stopAtDue(r, stop, L::StopKind::HALT, planOptions(r));
        CHECK(!stop.standstillObserving);
        runScripted(r, stop, TRIAL_1513, 2);
        CHECK(stop.outcome == M::ActionOutcome::OBSERVATION_SCOPE && stop.observations == 1);
        CHECK(L::advanceVelocityStop(r.context, stop, r.now) && r.context.needsStop);
    }
    {
        // A five-field age cap offers no standstill observation.
        Rig r;
        quickStopRun(r);
        static L::ActionContext stop;
        stopAtDue(r, stop, L::StopKind::QUICK_STOP, planOptions(r, 5));
        CHECK(!stop.standstillObserving);
        runScripted(r, stop, TRIAL_1513, 2);
        CHECK(stop.outcome == M::ActionOutcome::OBSERVATION_SCOPE && stop.observations == 1);
    }
    {
        // prepareStop's mode 3 admission is a fresh running report after 000F: the same rule from the
        // first cycle, with a seven-field age only, and never for HALT.
        for (unsigned variant = 0; variant < 3; ++variant) {
            Rig r;
            quickStopRun(r);
            r.reachRunning();
            L::ActionOptions use = stopOptions(r);
            use.observationIntervalUs = 300000;
            if (variant == 1)
                use.maximumAgeUs = 5 * 200;
            static L::ActionContext stop;
            CHECK(L::prepareStop(stop, r.live, r.observation, r.context.commandHistory[2],
                                 variant == 2 ? L::StopKind::HALT : L::StopKind::QUICK_STOP, 43, r.now, DEADLINE, use));
            CHECK(stop.stopModeRaw == 3 && stop.standstillObserving == (variant == 0));
            if (variant)
                continue;
            runScripted(r, stop, TRIAL_1607, 2);
            CHECK(stop.state == M::ActionState::SUCCEEDED && stop.observations == 2);
        }
    }
    {
        // An escalation from a running mode 3 HALT follows 010F, which no record covers: old scope.
        Rig r;
        experiment(r);
        r.reachRunning();
        L::ActionOptions o = stopOptions(r);
        o.observationIntervalUs = 300000;
        static L::ActionContext halt, quick;
        CHECK(L::prepareStop(halt, r.live, r.observation, r.context.commandHistory[2], L::StopKind::HALT, 43, r.now,
                             DEADLINE, o));
        const Report running[1] = {{0x0004, 0xc437, 3, 900, 30}};
        runScripted(r, halt, running, 1, 1);
        CHECK(halt.state == M::ActionState::ACTIVE && halt.observations == 1);
        CHECK(L::prepareEscalation(quick, halt, 44, r.now, DEADLINE, o));
        CHECK(quick.predecessorWord == 0x010f && !quick.standstillObserving);
    }
}
// After a moving quick stop the drive reports 2100 0015 (running latched), 6041 C050: a run may start
// there in mode 3, word 6's cycle repeating its 6064, with the latch clear from word 7's cycle on.
void latchedReleaseAdmitsAVelocityStart() {
    for (uint16_t clearedBy : {uint16_t(7), uint16_t(6)}) {
        Rig r;
        experiment(r);
        r.latched = true;
        r.latchClearedBy = clearedBy;
        r.acquireInitial();
        r.reachRunning();
        const L::VelocityContext& c = r.context;
        CHECK(c.admission.monitor == 0x0015 && c.admission.statusword == 0xc050 && c.admission.position == 5608 &&
              c.observations >= 3 && c.firstFailure == M::ActionOutcome::NONE);
    }
    for (unsigned k = 0; k < 6; ++k) {
        Rig r;
        experiment(r);
        r.latched = true;
        switch (k) {
        case 0: r.latchedStatus = 0xc051; break; // Low bits 001 with the latch.
        case 1: r.latchedStatus = 0xc053; break; // 011.
        case 2: r.latchedVelocity = 1; break;    // 606C moving.
        case 3: r.latchedMonitor = 0x0005; break; // Running without the release.
        case 4: r.latchedMonitor = 0x001d; break; // Alarm.
        default:
            // Mode 1, even with mode-change evidence: the latch is admitted in mode 3 only.
            r.evidence.gate = L::VelocityGate::MEASURED;
            r.evidence.startQualified = r.evidence.stopQualified = r.evidence.modeChangeQualified = true;
            r.evidence.modeChangeFromRaw = 1;
            r.mode = 1;
            break;
        }
        r.acquireInitial();
        r.reject();
    }
    {
        // The latch still reported in word 7's cycle fails the prefix, with the run's stop requested.
        Rig r;
        experiment(r);
        r.latched = true;
        r.latchClearedBy = 15;
        r.acquireInitial();
        CHECK(r.prepare());
        serveUntil(r, false);
        const L::VelocityContext& c = r.context;
        CHECK(c.phase == L::VelocityPhase::STOPPING && c.stopRequested && c.needsStop && c.commandCursor == 1 &&
              c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE && c.latest.monitor == 0x0015 &&
              std::strcmp(c.status.msg, "enable prefix not freshly confirmed") == 0);
    }
    {
        // 6064 moved between the admission and word 6's cycle: no second report of one standstill.
        Rig r;
        experiment(r);
        r.latched = true;
        r.creep = 1;
        r.acquireInitial();
        CHECK(r.prepare());
        serveUntil(r, false);
        const L::VelocityContext& c = r.context;
        CHECK(c.phase == L::VelocityPhase::STOPPING && c.stopRequested && c.commandCursor == 0 &&
              c.latest.position == 5609 && c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE);
    }
}
// MONITORS and STATUS read now: what a stop admission judges; with feedback, 6064 and 606C too.
void readStopState(Rig& r, L::ReadContext& into, const Report& report, bool feedback = false) {
    const uint8_t blocks =
        uint8_t((1u << unsigned(L::StateBlock::MONITORS)) | (1u << unsigned(L::StateBlock::STATUS)) |
                (feedback ? 1u << unsigned(L::StateBlock::FEEDBACK) : 0u));
    CHECK(L::prepareState(into, r.target, 1, r.now, r.now + 100000, r.options.service, blocks));
    while (into.state == M::ReadState::ACTIVE) {
        L::PreparedRead p;
        CHECK(L::nextRead(into, r.now, p) && p.ready);
        const uint16_t index = p.transfer.index;
        const uint32_t value = index == 0x2100   ? report.monitor
                               : index == 0x6041 ? report.status
                               : index == 0x6061 ? report.mode
                               : index == 0x6064 ? uint32_t(report.position)
                               : index == 0x606c ? uint32_t(report.velocity)
                                                 : 0u;
        CHECK(L::advanceRead(into, reply(p.transfer, r.now, r.now + 1, value), r.now + 1));
        r.now += 100;
    }
    CHECK(into.state == M::ReadState::SUCCEEDED);
}
// The attended experiment's one 000B of a drive the run's own HALT left halted (backlog S18).
void haltedVelocityQuickStop() {
    Rig r;
    experiment(r);
    static L::ActionContext halt, quick;
    stopAtDue(r, halt, L::StopKind::HALT, planOptions(r));
    runScripted(r, halt, &HALTED, 1);
    CHECK(halt.state == M::ActionState::SUCCEEDED && L::advanceVelocityStop(r.context, halt, r.now));
    CHECK(r.context.state == M::ActionState::SUCCEEDED && !r.context.needsStop);
    const C::SdoCompletion control = halt.commandHistory[0];
    const Saved<L::VelocityContext> run(r.context);
    static L::ReadContext state, other;
    readStopState(r, state, HALTED);
    const L::ActionOptions o = planOptions(r);
    const uint64_t deadline = r.now + 500000;
    const Saved<L::ActionContext> before(quick);
    CHECK(L::validateHaltedVelocityQuickStop(quick, r.context, r.live, r.target, state, control, 44, r.now,
                                             deadline, o));
    // Refused, with nothing written: budgets, identity, another word, another report or run.
    CHECK(!L::validateHaltedVelocityQuickStop(quick, r.context, r.live, r.target, state, control, 44, r.now,
                                              deadline, planOptions(r, 5)));
    CHECK(!L::prepareHaltedVelocityQuickStop(quick, r.context, r.live, r.target, state, control, 43, r.now,
                                             deadline, o));
    CHECK(!L::prepareHaltedVelocityQuickStop(quick, r.context, r.live, r.target, state, r.context.commandHistory[2],
                                             44, r.now, deadline, o));
    CHECK(!L::prepareHaltedVelocityQuickStop(quick, r.context, r.live, r.target, r.observation, control, 44, r.now,
                                             deadline, o)); // A running report from before the 010F.
    const Report reports[] = {{0x0005, 0x9437, 3, 0, 0}, {0x0001, 0x9037, 3, 0, 0}, {0x0001, 0x9437, 1, 0, 0},
                              {0x0011, 0x8050, 3, 0, 0}};
    for (const Report& k : reports) {
        readStopState(r, other, k);
        CHECK(!L::prepareHaltedVelocityQuickStop(quick, r.context, r.live, r.target, other, control, 44, r.now,
                                                 r.now + 500000, o));
    }
    readStopState(r, state, HALTED);
    for (unsigned k = 0; k < 3; ++k) {
        static L::VelocityContext forged;
        forged = r.context;
        if (k == 0)
            forged.evidence.gate = L::VelocityGate::MEASURED;
        else if (k == 1)
            forged.state = M::ActionState::FAILED;
        else
            forged.options.stopKind = forged.evidence.stopKind = L::StopKind::QUICK_STOP;
        CHECK(!L::prepareHaltedVelocityQuickStop(quick, forged, r.live, r.target, state, control, 44, r.now,
                                                 r.now + 500000, o));
    }
    before.unchanged(quick);
    // Released standstill completes it; staying halted completes nothing, to the deadline.
    for (unsigned variant = 0; variant < 2; ++variant) {
        readStopState(r, state, HALTED);
        CHECK(L::prepareHaltedVelocityQuickStop(quick, r.context, r.live, r.target, state, control, 44 + variant,
                                                r.now, r.now + 500000, o));
        CHECK(quick.kind == L::StopKind::QUICK_STOP && quick.stopModeRaw == 3 && quick.predecessorWord == 0x010f &&
              !quick.predecessorDocumented && quick.predecessorBasis == L::PredecessorBasis::PROJECT_DECISION &&
              quick.standstillObserving && quick.monitorObservation.monitor == 0x0001);
        L::PreparedAction command;
        CHECK(L::nextAction(quick, r.live, r.target, r.now, command) && command.ready &&
              command.transfer.data[0] == 0x0b && command.transfer.data[1] == 0);
        const Report released[2] = {{0x0010, 0x8050, 3, 1131, 0}, {0x0011, 0x8050, 3, 1131, 0}};
        if (variant)
            runScripted(r, quick, &HALTED, 1);
        else
            runScripted(r, quick, released, 2);
        CHECK(quick.state == (variant ? M::ActionState::FAILED : M::ActionState::SUCCEEDED) &&
              quick.observations == 2 && quick.read.count == 7);
        CHECK(quick.outcome == (variant ? M::ActionOutcome::DEADLINE : M::ActionOutcome::OBSERVED));
        run.unchanged(r.context); // The finished run is never rewritten.
    }
}
// The exact halted report, judged alone (validateHaltedVelocityReport): what the attendant's best-effort
// 000B after an MCU restart, and the experiment stop after a run's own HALT, both require.
void haltedReportRule() {
    Rig r;
    static L::ReadContext state;
    const uint64_t age = 1000000;
    readStopState(r, state, HALTED);
    CHECK(L::validateHaltedVelocityReport(state, r.target, 0, r.now, age));
    CHECK(!L::validateHaltedVelocityReport(state, r.target, r.now, r.now, age)); // Before the latest write.
    CHECK(!L::validateHaltedVelocityReport(state, r.target, 0, r.now + age + 1000, age)); // Stale.
    L::ReadTarget other = r.target;
    ++other.nodeGeneration;
    CHECK(!L::validateHaltedVelocityReport(state, other, 0, r.now, age));
    const Report reports[] = {{0x0005, 0x9437, 3, 0, 0},  // Running.
                              {0x0009, 0x9437, 3, 0, 0},  // Alarm.
                              {0x0011, 0x9437, 3, 0, 0},  // Released.
                              {0x0001, 0x9037, 3, 0, 0},  // Another 6041.
                              {0x0001, 0x9437, 1, 0, 0},  // Mode 1.
                              {0x0001, 0x8437, 3, 0, 0}}; // Mode 1's enabled report, in mode 3.
    for (const Report& k : reports) {
        readStopState(r, state, k);
        CHECK(!L::validateHaltedVelocityReport(state, r.target, 0, r.now, age));
    }
    // With FEEDBACK in the read's plan, 606C must read zero.
    readStopState(r, state, HALTED, true);
    CHECK(L::validateHaltedVelocityReport(state, r.target, 0, r.now, age));
    const Report moving = {0x0001, 0x9437, 3, 1131, 2};
    readStopState(r, state, moving, true);
    CHECK(!L::validateHaltedVelocityReport(state, r.target, 0, r.now, age));
}
// Trials 1742 and 1762 (node 2): word 6's cycle repeated the admission 6064 (8412 from 2100 0015, and 8419
// from 0011), word 7's read 8424 both times, and so did word 15's first cycle, 8033 with 606C 0. Against the
// admission both failed there; against word 7's cycle they wait in the window and run. A 6064 change after
// word 7 still fails, with the run's stop requested.
void windowReferenceIsWord7sCycle() {
    for (unsigned k = 0; k < 3; ++k) {
        Rig r;
        window(r);
        r.lagCycles = 2;
        r.runningStatus = 0xc437;
        r.latched = k == 0;
        r.latchedPosition = 8412;
        r.pull = k == 0 ? 12 : 5;
        const int32_t word7 = (k == 0 ? 8412 : 0) + r.pull;
        r.lagPosition = k == 2 ? word7 + 1 : word7;
        r.acquireInitial();
        CHECK(r.prepare());
        serveUntil(r, true);
        const L::VelocityContext& c = r.context;
        if (k < 2) {
            CHECK(c.state == M::ActionState::ACTIVE && c.runningObserved && c.enablePending == 2 &&
                  c.pendingFirst.position == word7 && c.pendingLast.position == word7 &&
                  c.admission.position == word7 - r.pull && c.firstFailure == M::ActionOutcome::NONE);
        } else {
            CHECK(c.phase == L::VelocityPhase::STOPPING && c.stopRequested && c.needsStop && !c.enablePending &&
                  c.latest.position == word7 + 1 && c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
                  std::strcmp(c.status.msg, "final enable state is not confirmed") == 0);
        }
    }
}
// Prompt 59's range probe: an ATTENDED_EXPERIMENT with rangeProbe, stating up to
// VELOCITY_RANGE_PROBE_MAXIMUM_RPM with both ramps at least VELOCITY_RANGE_PROBE_MINIMUM_RAMP_MS.
void probe(Rig& r, int64_t rpm = L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM) {
    experiment(r);
    r.evidence.rangeProbe = true;
    r.evidence.maximumForwardRpm = L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM;
    r.request.velocity.value = M::Rational(rpm);
    r.profile.rampUpTimeMs = r.profile.rampDownTimeMs = L::VELOCITY_RANGE_PROBE_MINIMUM_RAMP_MS;
}
// Refused by both validation and preparation with exactly this status, the candidate unchanged.
void refusedAs(Rig& r, M::Err code, M::VelocityError detail) {
    const M::Status checked = L::validateVelocity(r.context, r.live, r.target, r.initial, r.identity, r.evidence,
                                                  r.request, r.profile, r.axis, 42, r.now, DEADLINE, r.options);
    CHECK(!checked && checked.code == code && checked.detail == int32_t(detail));
    const M::Status status = r.prepare();
    CHECK(!status && status.code == code && status.detail == int32_t(detail));
    r.reject();
}
void rangeProbeScope() {
    {
        // Without the probe the experiment keeps its 60 rpm cap, and its ramps keep only the 1..2000 ms rule.
        Rig r;
        experiment(r);
        r.profile.rampUpTimeMs = r.profile.rampDownTimeMs = 1;
        CHECK(r.prepare() && r.context.rate.effectiveNative == 60 && !r.context.evidence.rangeProbe);
        Rig over;
        experiment(over);
        over.request.velocity.value = M::Rational(int64_t(L::VELOCITY_EXPERIMENT_MAXIMUM_RPM + 1));
        refusedAs(over, M::Err::ILLEGAL_VALUE, M::VelocityError::RANGE);
        for (uint32_t cap : {L::VELOCITY_EXPERIMENT_MAXIMUM_RPM + 1, L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM}) {
            Rig wide;
            experiment(wide);
            wide.evidence.maximumForwardRpm = cap; // The probe's cap needs the probe.
            refusedAs(wide, M::Err::INVALID_CONFIG, M::VelocityError::ARGUMENT);
        }
    }
    {
        // The probe admits its cap, with its 60FF bytes little-endian: 6000 = 0x1770.
        Rig r;
        probe(r);
        CHECK(r.prepare());
        CHECK(r.context.rate.effectiveNative == 6000 && r.context.evidence.rangeProbe &&
              r.context.profile.rampUpTimeMs == 200 && r.context.profile.rampDownTimeMs == 200);
        const L::PreparedVelocity first = r.next(START);
        CHECK(first.ready && first.transfer.direction == C::SdoDirection::DOWNLOAD &&
              first.transfer.index == 0x60ff && first.transfer.subindex == 0 && first.transfer.size == 4);
        CHECK(first.transfer.data[0] == 0x70 && first.transfer.data[1] == 0x17 && first.transfer.data[2] == 0 &&
              first.transfer.data[3] == 0);
    }
    {
        // One rpm over the cap, as a request or as the evidence's range, is refused; so is 6000.5, which must
        // not round into the admitted 6000.
        Rig rate;
        probe(rate, int64_t(L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM) + 1);
        refusedAs(rate, M::Err::ILLEGAL_VALUE, M::VelocityError::RANGE);
        Rig range;
        probe(range);
        range.evidence.maximumForwardRpm = L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM + 1;
        refusedAs(range, M::Err::INVALID_CONFIG, M::VelocityError::ARGUMENT);
        Rig half;
        probe(half);
        half.request.velocity.value = M::Rational(12001, 2);
        half.request.velocity.rounding = M::Rounding::TOWARD_ZERO;
        half.request.velocity.maximumQuantizationError = M::Rational(1, 2);
        refusedAs(half, M::Err::ILLEGAL_VALUE, M::VelocityError::RANGE);
    }
    {
        // Ramps: 199 ms on either side is refused, 200 and 2000 are admitted, and 2001 keeps the general refusal.
        struct Ramp {
            uint32_t up, down;
            bool admitted;
        };
        const Ramp ramps[] = {{199, 200, false}, {200, 199, false}, {199, 199, false}, {1, 2000, false},
                              {200, 200, true},  {2000, 2000, true}, {200, 2001, false}};
        for (const Ramp& k : ramps) {
            Rig r;
            probe(r);
            r.profile.rampUpTimeMs = k.up;
            r.profile.rampDownTimeMs = k.down;
            if (k.admitted)
                CHECK(r.prepare());
            else
                refusedAs(r, M::Err::ILLEGAL_VALUE, M::VelocityError::RANGE);
        }
    }
    {
        // The probe is still an experiment: no qualification, no reverse, no duration past 20 s.
        for (unsigned bad = 0; bad < 4; ++bad) {
            Rig r;
            probe(r);
            if (bad == 0)
                r.evidence.startQualified = true;
            else if (bad == 1)
                r.evidence.stopQualified = true;
            else if (bad == 2) {
                r.evidence.reverseQualified = true;
                r.evidence.maximumReverseRpm = 3000;
            } else
                r.evidence.modeChangeQualified = true;
            refusedAs(r, M::Err::INVALID_CONFIG, M::VelocityError::ARGUMENT);
        }
        Rig reverse;
        probe(reverse, -int64_t(L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM));
        refusedAs(reverse, M::Err::ILLEGAL_VALUE, M::VelocityError::RANGE);
        const uint64_t longest = L::VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US;
        for (uint64_t extra : {uint64_t(0), uint64_t(1)}) {
            Rig r;
            probe(r);
            r.options.maxPolls = 3000;
            r.request.durationUs = longest + extra;
            const M::Status status = r.prepare(START + longest + 5000000);
            if (extra) {
                CHECK(!status && status.code == M::Err::INVALID_CONFIG &&
                      status.detail == int32_t(M::VelocityError::TIME));
                r.reject(START + longest + 5000000);
            } else
                CHECK(status);
        }
    }
    {
        // No other gate takes the probe, even inside its own range; MEASURED keeps 3000 rpm.
        Rig measured;
        measured.evidence.rangeProbe = true;
        refusedAs(measured, M::Err::INVALID_CONFIG, M::VelocityError::ARGUMENT);
        Rig fast;
        fast.evidence.rangeProbe = true;
        fast.evidence.maximumForwardRpm = L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM;
        fast.request.velocity.value = M::Rational(int64_t(L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM));
        fast.profile.rampUpTimeMs = fast.profile.rampDownTimeMs = 200;
        refusedAs(fast, M::Err::INVALID_CONFIG, M::VelocityError::ARGUMENT);
        Rig closed;
        probe(closed);
        closed.evidence.gate = L::VelocityGate::CLOSED;
        refusedAs(closed, M::Err::UNSUPPORTED, M::VelocityError::CAPABILITY);
        Rig top;
        top.request.velocity.value = M::Rational(3000);
        CHECK(top.prepare() && top.context.rate.effectiveNative == 3000);
        Rig above;
        above.request.velocity.value = M::Rational(3001);
        refusedAs(above, M::Err::ILLEGAL_VALUE, M::VelocityError::RANGE);
        for (uint32_t cap : {uint32_t(3001), L::VELOCITY_RANGE_PROBE_MAXIMUM_RPM}) {
            Rig wide;
            wide.evidence.maximumForwardRpm = cap;
            refusedAs(wide, M::Err::INVALID_CONFIG, M::VelocityError::ARGUMENT);
        }
    }
}
// A probe run at 6000 rpm runs, stops and succeeds as any experiment, and reads as qualified nowhere.
void rangeProbeRunQualifiesNothing() {
    Rig r;
    probe(r);
    r.reachRunning();
    const L::VelocityContext& c = r.context;
    CHECK(c.latest.velocity == 6000 && c.enableReport.velocity == 6000 && c.firstFailure == M::ActionOutcome::NONE);
    // The ramps went out as stated, after the 60FF.
    CHECK(c.setupEvidenceMask == 7 && c.setupHistory[1].transfer.index == 0x6083 &&
          c.setupHistory[1].transfer.data[0] == 0xc8 && c.setupHistory[1].transfer.data[1] == 0 &&
          c.setupHistory[2].transfer.index == 0x6084 && c.setupHistory[2].transfer.data[0] == 0xc8 &&
          c.setupHistory[2].transfer.data[1] == 0);
    bool due = false;
    CHECK(L::serviceVelocity(r.context, r.live, r.target, c.stopDueUs, false, due) && due);
    r.now = c.stopDueUs;
    static L::ActionContext halt;
    CHECK(L::prepareVelocityStop(halt, r.context, r.live, r.target, c.commandHistory[2], L::StopKind::HALT, 43, r.now,
                                 DEADLINE, stopOptions(r)));
    runStop(r, halt, 0x0001, 0x9437);
    CHECK(L::advanceVelocityStop(r.context, halt, r.now));
    CHECK(c.state == M::ActionState::SUCCEEDED && c.completion == M::ActionCompletion::OBSERVED && !c.needsStop);
    CHECK(c.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT && c.evidence.rangeProbe &&
          !c.evidence.startQualified && !c.evidence.stopQualified && !c.evidence.modeChangeQualified &&
          !c.evidence.reverseQualified && !c.evidence.maximumReverseRpm);
}
// The over-speed rule of every gate: a cycle from the enable report on whose 606C magnitude exceeds
// |command| + |command| / 5 + VELOCITY_OVERSPEED_MARGIN_RPM ends the run with its stop requested.
enum class Gate { MEASURED, REVERSE, EXPERIMENT, PROBE };
void overSpeedRig(Rig& r, Gate gate, int64_t rpm) {
    if (gate == Gate::EXPERIMENT)
        experiment(r);
    else if (gate == Gate::PROBE)
        probe(r);
    else if (gate == Gate::REVERSE) {
        r.evidence.reverseQualified = true;
        r.evidence.maximumReverseRpm = 3000;
    }
    r.request.velocity.value = M::Rational(rpm);
}
int32_t overSpeedLimit(int64_t rpm) {
    const int64_t magnitude = rpm < 0 ? -rpm : rpm;
    return int32_t(magnitude + magnitude / 5 + int64_t(L::VELOCITY_OVERSPEED_MARGIN_RPM));
}
void overSpeedFailed(const L::VelocityContext& c, int32_t reported) {
    CHECK(c.state == M::ActionState::ACTIVE && c.phase == L::VelocityPhase::STOPPING && c.stopRequested &&
          c.needsStop && c.uncertain && c.stopRequestedUs == c.servicedUs);
    CHECK(c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE && c.outcome == M::ActionOutcome::OBSERVATION_SCOPE &&
          std::strcmp(c.status.msg, "reported velocity exceeds the command") == 0);
    CHECK(c.latest.present && c.latest.velocity == reported && c.latest.cycle == c.observations);
}
void overSpeedEndsTheRun() {
    struct Case {
        Gate gate;
        int64_t rpm;
    };
    // 120 + 24 + 10, 123 + 24 + 10 (the fifth rounds down), 60 + 12 + 10, 7 + 1 + 10, 6000 + 1200 + 10.
    const Case cases[] = {{Gate::MEASURED, 120}, {Gate::REVERSE, -123}, {Gate::EXPERIMENT, 60},
                          {Gate::EXPERIMENT, 7}, {Gate::PROBE, 6000}};
    for (const Case& k : cases) {
        const int32_t sign = k.rpm < 0 ? -1 : 1;
        const int32_t limit = overSpeedLimit(k.rpm);
        {
            // At the limit, in the enable report and every running cycle, the run continues.
            Rig r;
            overSpeedRig(r, k.gate, k.rpm);
            r.runningVelocity = sign * limit;
            r.reachRunning();
            for (unsigned step = 0; step < 200 && r.context.runningPolls < 3 &&
                                    r.context.state == M::ActionState::ACTIVE &&
                                    r.context.phase != L::VelocityPhase::STOPPING;
                 ++step)
                r.serveOne();
            CHECK(r.context.runningPolls == 3 && r.context.firstFailure == M::ActionOutcome::NONE &&
                  r.context.state == M::ActionState::ACTIVE && r.context.latest.velocity == sign * limit &&
                  r.context.enableReport.velocity == sign * limit);
        }
        {
            // One more, in the first cycle after word 15: that cycle reported the enable, then ended the run.
            Rig r;
            overSpeedRig(r, k.gate, k.rpm);
            r.runningVelocity = sign * (limit + 1);
            CHECK(r.prepare());
            serveUntil(r, false);
            const L::VelocityContext& c = r.context;
            overSpeedFailed(c, sign * (limit + 1));
            CHECK(c.enableReport.present && c.enableReport.cycle == c.latest.cycle && !c.runningPolls &&
                  !c.runningObserved && c.commandEvidenceMask == 7 && c.observations == 3);
            L::PreparedVelocity work;
            CHECK(L::nextVelocity(c, r.observation, r.live, r.target, r.now + 20000, work) && !work.ready &&
                  !work.observe);
            static L::ActionContext stop;
            CHECK(L::validateVelocityStop(stop, c, r.live, r.target, c.commandHistory[2], c.options.stopKind, 43, r.now,
                                          DEADLINE, stopOptions(r)));
        }
        {
            // One more in a later running cycle; the run's own HALT then settles the fence, the failure kept.
            Rig r;
            overSpeedRig(r, k.gate, k.rpm);
            r.reachRunning();
            r.runningVelocity = sign * (limit + 1);
            serveUntil(r, false);
            const L::VelocityContext& c = r.context;
            overSpeedFailed(c, sign * (limit + 1));
            CHECK(c.runningObserved && c.runningPolls == 1 && c.enableReport.velocity == int32_t(k.rpm));
            static L::ActionContext halt;
            CHECK(L::prepareVelocityStop(halt, r.context, r.live, r.target, c.commandHistory[2], L::StopKind::HALT, 43,
                                         r.now, DEADLINE, stopOptions(r)));
            runStop(r, halt, 0x0001, 0x9437);
            CHECK(halt.state == M::ActionState::SUCCEEDED && L::advanceVelocityStop(r.context, halt, r.now));
            CHECK(c.state == M::ActionState::FAILED && c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE &&
                  !c.needsStop && c.completion == M::ActionCompletion::OBSERVED && c.stopOperation == 43);
        }
    }
    {
        // The rule is on magnitude: reversed 606C within it is a contradicted direction, beyond it over-speed.
        const int32_t limit = overSpeedLimit(120);
        for (int32_t reported : {-limit, -(limit + 1)}) {
            Rig r;
            r.reachRunning();
            r.runningVelocity = reported;
            serveUntil(r, false);
            const L::VelocityContext& c = r.context;
            CHECK(c.phase == L::VelocityPhase::STOPPING && c.stopRequested && c.needsStop &&
                  c.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE && c.latest.velocity == reported);
            CHECK(std::strcmp(c.status.msg, reported == -limit ? "running velocity evidence contradicted"
                                                               : "reported velocity exceeds the command") == 0);
        }
    }
}
} // namespace
int main() {
    windowReferenceIsWord7sCycle();
    quickStopSettlesOnReleasedStandstill();
    releasedStandstillStaysNarrow();
    latchedReleaseAdmitsAVelocityStart();
    haltedVelocityQuickStop();
    haltedReportRule();
    enableWindowAdmission();
    enableWindowThenRunning();
    enableWindowExpires();
    enableWindowContradicted();
    runStopFollowsTheAcknowledgedFinalWord();
    attendedExperimentScope();
    exactPollCoverageIncludesTheWholeDuration();
    bestEffortSettlementRequiresQuickStopPolicy();
    preparationBoundaries();
    timingAndRetainedFailure();
    definiteRetirementRefinesOnlyItsOwnNotice();
    finalEnableCannotOutliveDuration();
    rangeProbeScope();
    rangeProbeRunQualifiesNothing();
    overSpeedEndsTheRun();
    std::printf("finite velocity: %u checks passed\n", checks);
}
