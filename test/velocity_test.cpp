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
        if (lag && control == 15)
            return index == 0x6041 ? 0x8033u : index == 0x2100 ? 0x0001u : 0u;
        if (index == 0x6041)
            return control == 15 ? 0x4007u : control == 7 ? 3u : 1u;
        if (index == 0x2100)
            return control == 15 ? L::MONITOR_RUNNING : L::MONITOR_RELEASED;
        if (index == 0x606c)
            return control == 15 ? uint32_t(request.velocity.value.numerator < 0 ? -120 : 120) : 0u;
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
} // namespace
int main() {
    runStopFollowsTheAcknowledgedFinalWord();
    attendedExperimentScope();
    exactPollCoverageIncludesTheWholeDuration();
    bestEffortSettlementRequiresQuickStopPolicy();
    preparationBoundaries();
    timingAndRetainedFailure();
    definiteRetirementRefinesOnlyItsOwnNotice();
    finalEnableCannotOutliveDuration();
    std::printf("finite velocity: %u checks passed\n", checks);
}
