// SPDX-License-Identifier: MIT
// Synthetic owner terminals prove host contracts only; no fixture qualification is claimed.
#include <MotorControlCAN/profiles/lichuan_c/Homing.h>
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
constexpr uint64_t START = 1000, DEADLINE = 1000000;
constexpr uint8_t BLOCKS =
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
    M::AxisConfig axis;
    M::AxisReference reference;
    L::ReadContext initial, observation;
    L::DriverContext inputs, driver;
    L::IdentityObservation identity;
    C::SdoCompletion control;
    L::HomeEvidence evidence;
    L::HomeRequest request;
    L::HomeOptions options;
    L::HomeContext home;
    uint64_t now = START;
    uint8_t mode = 6;
    uint16_t status = 7, monitor = 0, logical = 0;
    int32_t position = 123, velocity = 0;
    Rig(L::HomingMethod method = L::HomingMethod::NEGATIVE_LIMIT) {
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
        axis.supportedRelativeBases = 1;
        axis.originKnown = true;
        axis.originSource = M::ScaleSource::QUALIFIED;
        reference.binding = axis.binding;
        reference.referenceGeneration = axis.referenceGeneration;
        reference.nativeKnown = reference.idle = reference.stationary = true;
        reference.source = M::ScaleSource::QUALIFIED;
        reference.observedUs = 500;
        reference.nowUs = START;
        reference.maximumAgeUs = DEADLINE;
        request.method = method;
        request.searchSpeed.configurationGeneration = request.creepSpeed.configurationGeneration =
            7;
        request.searchSpeed.value = M::Rational(120);
        request.creepSpeed.value = M::Rational(30);
        request.searchSpeed.unit = request.creepSpeed.unit =
            M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
        request.rampTimeMs = 100;
        options.service.queueTimeoutUs = options.service.responseTimeoutUs = 100;
        options.pollIntervalUs = 100;
        options.maxPolls = 10;
        options.maximumAgeUs = DEADLINE;
        evidence.gate = L::HomeGate::MEASURED;
        evidence.record = 42;
        evidence.model = live.model;
        evidence.specimen = target;
        evidence.method = method;
        evidence.layout = L::StatusLayout::ZH86;
        evidence.searchSpeedRpm = 120;
        evidence.creepSpeedRpm = 30;
        evidence.rampTimeMs = 100;
        evidence.methodQualified = evidence.parametersQualified = evidence.inputsQualified = true;
        evidence.referenceQualified = true;
        evidence.finalFeedbackNative = 10;
        evidence.referenceNative = 20;
        evidence.wiredInputs = 1;
        evidence.inputFunctions[0] = method == L::HomingMethod::NEGATIVE_LIMIT   ? 3
                                     : method == L::HomingMethod::POSITIVE_LIMIT ? 2
                                                                                 : 1;
        evidence.firmware.deviceType = 0x40192;
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
        identity.window.earliestUs = identity.window.latestUs = 150;
        C::SdoTransfer word;
        word.channel = target.channel;
        word.operation = word.request = word.step = 1;
        word.index = 0x6040;
        word.size = 2;
        word.direction = C::SdoDirection::DOWNLOAD;
        word.data[0] = 15;
        word.deadlineUs = 1000;
        word.queueTimeoutUs = word.responseTimeoutUs = 100;
        control = reply(word, 110, 120);
        acquire();
    }
    uint32_t value(const C::SdoTransfer& t) const {
        switch (t.index) {
        case 0x6061:
            return mode;
        case 0x6041:
            return status;
        case 0x2100:
            return monitor;
        case 0x2101:
            return logical;
        case 0x6064:
            return uint32_t(position);
        case 0x606c:
            return uint32_t(velocity);
        case 0x2200:
            return evidence.directionRaw;
        case 0x2400:
            if (!t.subindex)
                return 15;
            if (t.subindex == 1)
                return evidence.inputPolarityRaw;
            return t.subindex <= 8 ? evidence.inputFunctions[t.subindex - 2] : 0;
        default:
            return 0;
        }
    }
    void acquireRead(L::ReadContext& read, uint64_t time) {
        while (read.state == M::ReadState::ACTIVE) {
            L::PreparedRead p;
            CHECK(L::nextRead(read, time, p) && p.ready);
            CHECK(L::advanceRead(read, reply(p.transfer, time, time, value(p.transfer)), time));
            ++time;
        }
    }
    void acquire() {
        CHECK(L::prepareState(initial, target, 2, 200, 900, options.service, BLOCKS));
        acquireRead(initial, 200);
        CHECK(L::prepareDriverRead(inputs, target, 3, 300, 900, options.service,
                                   L::DriverGroup::IO_INPUT_CONFIGURATION));
        acquireRead(inputs.read, 300);
        CHECK(L::prepareDriverRead(driver, target, 4, 400, 900, options.service,
                                   L::DriverGroup::DRIVER));
        acquireRead(driver.read, 400);
    }
    M::Status prepare() {
        return L::prepareHome(home, live, target, initial, control, identity, inputs, driver,
                              evidence, request, axis, 42, now, DEADLINE, options);
    }
    void reject() {
        home.operation = 0xbeef;
        const Saved<L::HomeContext> before(home);
        CHECK(!prepare());
        before.unchanged(home);
    }
    L::PreparedHome next() {
        L::PreparedHome work;
        CHECK(L::nextHome(home, observation, live, target, now, work));
        return work;
    }
    void settle(const L::PreparedHome& p) {
        if (p.transfer.direction == C::SdoDirection::DOWNLOAD)
            CHECK(L::noteHomeTransmission(home, live, target, axis, reference, nullptr, 0, now));
        CHECK(L::advanceHome(home, observation, live, target,
                             reply(p.transfer, now, now + 1, value(p.transfer)), now + 1));
        now += 2;
    }
    void toTrigger() {
        CHECK(prepare());
        const uint16_t indexes[] = {0x6098, 0x6099, 0x6099, 0x609a, 0x607c};
        const uint8_t subindexes[] = {0, 1, 2, 0, 0};
        for (unsigned i = 0; i < 5; ++i) {
            const L::PreparedHome p = next();
            CHECK(p.ready && p.transfer.index == indexes[i] &&
                  p.transfer.subindex == subindexes[i]);
            CHECK(p.transfer.size == (i == 0 ? 1 : 4));
            settle(p);
            CHECK(axis.binding.configurationGeneration == 7 && reference.nativeKnown);
        }
        CHECK(home.phase == L::HomePhase::TRIGGER);
        CHECK(next().prepareTrigger && !next().ready);
        CHECK(L::beginHomeTrigger(home, live, target, axis, reference, now));
        CHECK(axis.binding.configurationGeneration == 8 && axis.referenceGeneration == 2);
        CHECK(!axis.originKnown && !reference.nativeKnown && home.coordinatesInvalidated);
        const Saved<L::HomeContext> saved(home);
        CHECK(!L::beginHomeTrigger(home, live, target, axis, reference, now));
        saved.unchanged(home);
        const L::PreparedHome p = next();
        CHECK(p.ready && p.transfer.index == 0x6040 && p.transfer.size == 2 &&
              p.transfer.data[0] == 31);
        CHECK(p.transfer.channel.binding.configurationGeneration == 8);
    }
    void start() {
        toTrigger();
        settle(next());
        CHECK(home.needsStop && home.phase == L::HomePhase::WAITING);
    }
    void cycle() {
        if (now < home.eligibleUs)
            now = home.eligibleUs;
        CHECK(next().observe);
        CHECK(L::beginHomeObservation(home, observation, live, target, now));
        for (unsigned i = 0; i < 7; ++i) {
            const L::PreparedHome p = next();
            CHECK(p.ready && p.transfer.direction == C::SdoDirection::UPLOAD);
            CHECK(p.transfer.channel.binding.configurationGeneration == 8);
            settle(p);
        }
    }
    void complete() {
        start();
        monitor = L::MONITOR_RUNNING;
        velocity = 30;
        logical = 1;
        cycle();
        monitor = L::MONITOR_HOMING_COMPLETE;
        velocity = 0;
        status = 0x107;
        logical = request.method == L::HomingMethod::NEGATIVE_ORIGIN ? 0 : 1;
        position = evidence.finalFeedbackNative;
        cycle();
        CHECK(home.state == M::ActionState::SUCCEEDED &&
              home.completion == M::ActionCompletion::OBSERVED);
    }
};

void methodsAndReference() {
    for (L::HomingMethod method :
         {L::HomingMethod::NEGATIVE_LIMIT, L::HomingMethod::POSITIVE_LIMIT,
          L::HomingMethod::POSITIVE_ORIGIN, L::HomingMethod::NEGATIVE_ORIGIN}) {
        Rig r(method);
        r.complete();
        CHECK(!r.home.needsStop && !r.home.uncertain);
        CHECK(!r.next().ready && !r.next().observe);
        CHECK(r.home.control.transfer.data[0] == 31); // No position-style 000F rearm.
        M::AxisReference ref;
        CHECK(L::getHomeReference(r.home, r.target, r.axis, r.now, 1000, ref));
        CHECK(ref.nativeKnown && ref.nativePosition == 20 && ref.referenceGeneration == 2 &&
              ref.binding.configurationGeneration == 8 &&
              ref.observedUs == r.home.completed.earliestUs);
        M::PositionRequest move;
        move.value = M::Rational(5);
        move.relative = true;
        move.configurationGeneration = r.axis.binding.configurationGeneration;
        M::PreparedTarget prepared;
        CHECK(M::preparePosition(move, r.axis, &ref, prepared) && prepared.endpointNative == 25);
        const Saved<M::AxisReference> before(ref);
        CHECK(!L::getHomeReference(r.home, r.target, r.axis, r.now + 1001, 1000, ref));
        before.unchanged(ref);
        M::AxisReference invalid;
        CHECK(M::invalidateAxisReference(r.axis, invalid, M::ReferenceLoss::POSITION));
        CHECK(!M::preparePosition(move, r.axis, &ref, prepared));
        CHECK(!L::getHomeReference(r.home, r.target, r.axis, r.now, 1000, ref));
        before.unchanged(ref);
    }
}
void validation() {
    for (unsigned bad = 0; bad < 26; ++bad) {
        Rig r;
        switch (bad) {
        case 0:
            r.evidence.gate = L::HomeGate::CLOSED;
            break;
        case 1:
            r.request.method = static_cast<L::HomingMethod>(35);
            break;
        case 2:
            r.evidence.methodQualified = false;
            break;
        case 3:
            r.evidence.parametersQualified = false;
            break;
        case 4:
            r.evidence.inputsQualified = false;
            break;
        case 5:
            r.evidence.wiredInputs = 0;
            break;
        case 6:
            r.evidence.inputFunctions[0] = 1;
            break;
        case 7:
            r.evidence.directionRaw = 1;
            break;
        case 8:
            ++r.evidence.firmware.firmwareCode;
            break;
        case 9:
            ++r.evidence.specimen.nodeGeneration;
            break;
        case 10:
            r.request.rampTimeMs = 0;
            break;
        case 11:
            r.request.rampTimeMs = r.evidence.rampTimeMs = 5000;
            break;
        case 12:
            r.request.offsetNative = r.evidence.offsetNative = 1000001;
            break;
        case 13:
            r.request.searchSpeed.value = M::Rational(0);
            break;
        case 14:
            r.request.creepSpeed.value = M::Rational(0);
            break;
        case 15:
            r.request.searchSpeed.value = M::Rational(-1);
            break;
        case 16:
            r.request.searchSpeed.value = M::Rational(3001);
            break;
        case 17:
            r.axis.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
            break;
        case 18:
            r.axis.nativeVelocityTime = M::TimeUnit::SECOND;
            break;
        case 19:
            r.options.maxPolls = 0;
            break;
        case 20:
            r.axis.referenceGeneration = UINT32_MAX;
            break;
        case 21:
            r.control.transfer.data[0] = 31;
            break;
        case 22:
            r.mode = 1;
            r.acquire();
            break;
        case 23:
            r.monitor = L::MONITOR_RELEASED;
            r.acquire();
            break;
        case 24:
            r.logical = 1;
            r.acquire();
            break;
        case 25:
            r.inputs.read.invalidated = true;
            break;
        }
        r.reject();
    }
    Rig zero;
    zero.request.searchSpeed.value = M::Rational(1, 10);
    zero.request.searchSpeed.rounding = M::Rounding::FLOOR;
    zero.request.searchSpeed.maximumQuantizationError = M::Rational(1);
    zero.reject();
    Rig absent;
    absent.evidence.inputFunctions[0] = 0;
    absent.acquire();
    absent.reject(); // No-input home is unsupported for all four switch methods.
    Rig active;
    active.logical = 1;
    active.evidence.startOnActiveSwitchQualified = true;
    active.acquire();
    CHECK(active.prepare());
    Rig qualifiedOffset;
    qualifiedOffset.evidence.offsetNative = qualifiedOffset.request.offsetNative = -1000000;
    CHECK(qualifiedOffset.prepare());
    Rig wrongBasis;
    wrongBasis.axis.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
    wrongBasis.axis.nativeVelocityTime = M::TimeUnit::SECOND;
    wrongBasis.axis.units.commandStepsPerMotorTurn =
        M::UnitScale(3200, 1, M::ScaleSource::QUALIFIED);
    wrongBasis.axis.units.motorTurnsPerLoadTurn = M::UnitScale(5, 1, M::ScaleSource::QUALIFIED);
    wrongBasis.request.searchSpeed.frame = M::CoordinateFrame::LOAD;
    wrongBasis.request.searchSpeed.value = M::Rational(1);
    wrongBasis.request.searchSpeed.rounding = M::Rounding::NEAREST;
    wrongBasis.request.searchSpeed.maximumQuantizationError = M::Rational(1, 2);
    M::PreparedVelocityTarget converted;
    CHECK(M::prepareVelocityTarget(wrongBasis.request.searchSpeed, wrongBasis.axis, converted));
    CHECK(converted.effectiveNative ==
          267); // Plausible numeric value, wrong destination dimension.
    wrongBasis.evidence.searchSpeedRpm = 267;
    wrongBasis.reject();
    Rig reversed;
    reversed.axis.units.commandPolarity = -1;
    reversed.request.searchSpeed.frame = M::CoordinateFrame::MOTOR;
    reversed.reject(); // Positive physical rate converts to negative native rpm and is refused.
    reversed.request.searchSpeed.value = M::Rational(-120);
    reversed.reject(); // Negative intent cannot cancel command polarity to choose direction.
    reversed.request.searchSpeed.frame = M::CoordinateFrame::NATIVE;
    reversed.request.searchSpeed.value = M::Rational(120);
    CHECK(reversed.prepare()); // Explicit positive native magnitude is independent of polarity.
}
void failuresAndCorrelation() {
    for (unsigned index = 0; index < 5; ++index) {
        Rig r;
        CHECK(r.prepare());
        for (unsigned i = 0; i < index; ++i)
            r.settle(r.next());
        const L::PreparedHome p = r.next();
        C::SdoCompletion malformed = reply(p.transfer, r.now, r.now + 1);
        malformed.transfer.size = 2; // Public-code wrong-width examples must not correlate.
        const Saved<L::HomeContext> saved(r.home);
        CHECK(!L::advanceHome(r.home, r.observation, r.live, r.target, malformed, r.now + 1));
        saved.unchanged(r.home);
    }
    Rig trigger;
    trigger.toTrigger();
    L::PreparedHome p = trigger.next();
    CHECK(L::noteHomeTransmission(trigger.home, trigger.live, trigger.target, trigger.axis,
                                  trigger.reference, nullptr, 0, trigger.now));
    CHECK(!trigger.next().ready);
    CHECK(!L::noteHomeTransmission(trigger.home, trigger.live, trigger.target, trigger.axis,
                                   trigger.reference, nullptr, 0, trigger.now));
    C::SdoCompletion timeout = reply(p.transfer, trigger.now, trigger.now + 1);
    timeout.outcome = C::SdoOutcome::TIMEOUT;
    timeout.protocolAcknowledged = false;
    timeout.effectUncertain = true;
    CHECK(L::advanceHome(trigger.home, trigger.observation, trigger.live, trigger.target, timeout,
                         trigger.now + 1));
    CHECK(trigger.home.state == M::ActionState::FAILED && trigger.home.needsStop &&
          trigger.home.uncertain);
    CHECK(trigger.home.execution == M::ActionExecution::UNKNOWN && !trigger.reference.nativeKnown);
    const Saved<L::HomeContext> failed(trigger.home);
    CHECK(!L::advanceHome(trigger.home, trigger.observation, trigger.live, trigger.target,
                          reply(p.transfer, trigger.now, trigger.now + 2), trigger.now + 2));
    failed.unchanged(trigger.home);

    Rig late;
    late.toTrigger();
    p = late.next();
    CHECK(L::noteHomeTransmission(late.home, late.live, late.target, late.axis, late.reference,
                                  nullptr, 0, late.now));
    CHECK(L::advanceHome(late.home, late.observation, late.live, late.target,
                         reply(p.transfer, late.now, late.now + 1), DEADLINE));
    CHECK(late.home.outcome == M::ActionOutcome::DEADLINE && late.home.needsStop);

    Rig cancel;
    cancel.start();
    CHECK(L::beginHomeObservation(cancel.home, cancel.observation, cancel.live, cancel.target,
                                  cancel.now));
    p = cancel.next();
    CHECK(L::noteHomeWithdrawal(cancel.home, cancel.live, cancel.target, M::HomeInterruption::STOP,
                                cancel.now));
    CHECK(!cancel.next().ready);
    CHECK(L::advanceHome(cancel.home, cancel.observation, cancel.live, cancel.target,
                         reply(p.transfer, cancel.now, cancel.now + 1), cancel.now + 1));
    CHECK(cancel.home.state == M::ActionState::FAILED &&
          cancel.home.outcome == M::ActionOutcome::CANCELLED && cancel.home.needsStop &&
          cancel.home.observationEvidencePresent);
    const M::Status first = cancel.home.status;
    L::ActionContext stop;
    stop.reservation = cancel.live;
    stop.reservation.target = cancel.target;
    stop.actionKind = M::ActionKind::STOP;
    stop.kind = L::StopKind::HALT;
    stop.stopModeRaw = 6;
    stop.operation = 43;
    stop.startedUs = stop.servicedUs = cancel.now + 2;
    stop.state = M::ActionState::FAILED;
    stop.outcome = M::ActionOutcome::SERVICE_TIMEOUT;
    stop.execution = M::ActionExecution::UNKNOWN;
    stop.status = M::Status(M::Err::INVALID_CONFIG, 1234, "synthetic stop failure");
    CHECK(L::advanceHomeStop(cancel.home, stop, stop.servicedUs));
    CHECK(cancel.home.status.detail == first.detail && cancel.home.stopStatus.detail == 1234 &&
          cancel.home.stopOutcome == M::ActionOutcome::SERVICE_TIMEOUT && cancel.home.needsStop);
    CHECK(!L::advanceHomeStop(cancel.home, stop, stop.servicedUs));

    Rig stopSuccess;
    stopSuccess.start();
    CHECK(L::advanceHome(stopSuccess.home, stopSuccess.observation, stopSuccess.live,
                         stopSuccess.target, M::HomeInterruption::STOP, stopSuccess.now));
    stop.reservation = stopSuccess.live;
    stop.reservation.target = stopSuccess.target;
    stop.operation = 43;
    stop.startedUs = stopSuccess.now;
    stop.servicedUs = stopSuccess.now + 1;
    stop.state = M::ActionState::SUCCEEDED;
    stop.outcome = M::ActionOutcome::OBSERVED;
    stop.execution = M::ActionExecution::ACKNOWLEDGED;
    stop.completion = M::ActionCompletion::OBSERVED;
    stop.commandEvidenceMask = 1;
    C::SdoTransfer stopWord = stopSuccess.home.control.transfer;
    stopWord.operation = stopWord.request = 43;
    stopWord.step = 1;
    stopWord.data[0] = 15;
    stop.commandHistory[0] = reply(stopWord, stop.startedUs, stop.servicedUs);
    stop.status = M::Ok();
    CHECK(L::advanceHomeStop(stopSuccess.home, stop, stop.servicedUs));
    CHECK(!stopSuccess.home.needsStop && stopSuccess.home.state == M::ActionState::FAILED &&
          stopSuccess.home.outcome == M::ActionOutcome::CANCELLED &&
          stopSuccess.home.completion == M::ActionCompletion::NOT_OBSERVED);
}
void observationPolicy() {
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        Rig r(scenario == 4 ? L::HomingMethod::NEGATIVE_ORIGIN : L::HomingMethod::NEGATIVE_LIMIT);
        r.options.maxPolls = 2;
        r.start();
        r.monitor = L::MONITOR_RUNNING;
        r.logical = 1;
        r.velocity = 30;
        if (scenario == 0)
            r.status = 0x107; // Stale high never supplies absence.
        if (scenario == 1)
            r.monitor = r.velocity = 0; // No observed activity.
        if (scenario == 2)
            r.mode = 1;
        if (scenario == 3)
            r.logical = 0x80;
        r.cycle();
        if (r.home.state == M::ActionState::ACTIVE) {
            r.status = 0x107;
            r.monitor = L::MONITOR_HOMING_COMPLETE;
            r.velocity = 0;
            if (scenario == 5)
                r.logical = 0;
            // Method29 remains active in scenario4: completion cannot mean leaving the switch.
            r.cycle();
        }
        CHECK(r.home.state == M::ActionState::FAILED && r.home.needsStop);
    }
    Rig noReference;
    noReference.evidence.referenceQualified = false;
    noReference.complete();
    M::AxisReference ref;
    ref.nativePosition = 123;
    const Saved<M::AxisReference> saved(ref);
    CHECK(!L::getHomeReference(noReference.home, noReference.target, noReference.axis,
                               noReference.now, 1000, ref));
    saved.unchanged(ref);
    Rig changed;
    changed.start();
    ++changed.target.nodeGeneration;
    CHECK(L::advanceHome(changed.home, changed.observation, changed.live, changed.target,
                         M::HomeInterruption::INVALIDATED, changed.now));
    CHECK(changed.home.outcome == M::ActionOutcome::INVALIDATED && changed.home.needsStop);
    Rig overflow;
    overflow.toTrigger();
    overflow.home.step = UINT32_MAX;
    overflow.settle(overflow.next());
    CHECK(overflow.home.outcome == M::ActionOutcome::ITERATION_EXHAUSTED &&
          overflow.home.needsStop);
}
void retirementAndAtomicity() {
    Rig r;
    L::DriverContext retained;
    CHECK(L::prepareDriverRead(retained, r.target, 5, 500, 900, r.options.service,
                               L::DriverGroup::MOTION_PARAMETERS));
    r.acquireRead(retained.read, 500);
    CHECK(r.prepare());
    const Saved<L::HomeContext> before(r.home);
    CHECK(!L::prepareHome(r.home, r.live, r.target, r.initial, r.control, r.identity, r.inputs,
                          r.driver, r.home.evidence, r.request, r.axis, 42, r.now, DEADLINE,
                          r.options));
    before.unchanged(r.home);
    L::DriverContext other = retained;
    other.read.target.channel.binding.logicalAxisId = 999;
    L::DriverContext* snapshots[] = {&retained, &other};
    const Saved<L::DriverContext> saved(retained);
    CHECK(!L::noteHomeTransmission(r.home, r.live, r.target, r.axis, r.reference, snapshots, 2,
                                   r.now));
    saved.unchanged(retained);
    before.unchanged(r.home);
    CHECK(L::noteHomeTransmission(r.home, r.live, r.target, r.axis, r.reference, snapshots, 1,
                                  r.now));
    CHECK(retained.read.invalidated && r.axis.binding.configurationGeneration == 7 &&
          r.reference.nativeKnown);
    Rig fault;
    fault.start();
    fault.monitor = L::MONITOR_ALARM;
    fault.cycle();
    CHECK(fault.home.outcome == M::ActionOutcome::DRIVE_FAULT && fault.home.needsStop);
    Rig limit(L::HomingMethod::POSITIVE_ORIGIN);
    limit.evidence.inputFunctions[1] = 2;
    limit.acquire();
    limit.start();
    limit.logical = 2;
    limit.cycle();
    CHECK(limit.home.outcome == M::ActionOutcome::OBSERVATION_SCOPE);
    Rig mismatch;
    mismatch.complete();
    mismatch.home.evidence.finalFeedbackNative = 11;
    M::AxisReference ref;
    CHECK(!L::getHomeReference(mismatch.home, mismatch.target, mismatch.axis, mismatch.now, 1000,
                               ref));
}
} // namespace
int main() {
    methodsAndReference();
    validation();
    failuresAndCorrelation();
    observationPolicy();
    retirementAndAtomicity();
    std::printf("homing_test: %u checks\n", checks);
}
