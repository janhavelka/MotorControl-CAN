// SPDX-License-Identifier: MIT
// Public velocity core through the actual owner and a synthetic drive; no firmware qualification.
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)
#include "support/VelocityRig.h"

namespace {
using namespace VelocityRig;
const uint8_t BLOCKS = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                              (1u << unsigned(L::StateBlock::STATUS)) |
                              (1u << unsigned(L::StateBlock::FEEDBACK)));
struct Rig {
    C::SdoClient services;
    OwnerRig::WireBase wire;
    O::CanBusOwner owner;
    Drive drive;
    O::SequenceId sequence;
    L::AxisReservation live;
    L::ReadTarget current = target();
    M::AxisConfig axis = axisConfig();
    L::ReadContext initial, observation, identityRead;
    L::IdentityObservation identity;
    L::VelocityContext run;
    L::VelocityChangeEvidence proof;
    M::VelocityRequest replacement = request(180).velocity;
    uint64_t now = 0;
    unsigned served = 0;
    bool applyChange = true;
    Rig(int64_t initialRate = 120, int8_t polarity = 1, const L::VelocityProfile& ramps = profile())
        : owner(services, wire.plumbing(), 73) {
        CHECK(services.configure(0, current.channel, true, true));
        C::NetworkRoute network;
        network.binding = current.channel.binding;
        network.generation = current.nodeGeneration;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        CHECK(owner.beginSequence(current.channel.binding, 1, now, sequence));
        live.target = current;
        live.model = L::SettingsModel::CL86_C;
        live.owner = owner.identity();
        live.sequence = uint32_t(sequence.slot) + 1;
        live.generation = sequence.generation;
        live.acquiredUs = owner.sequence(sequence)->acquiredUs;
        CHECK(owner.beginOperation(sequence, 1, 1000000, now));
        CHECK(L::prepareIdentity(identityRead, current, 1, now, 1000000, reads()));
        read(identityRead);
        CHECK(L::getIdentity(identityRead, current, now, 1000000, identity));
        CHECK(owner.beginOperation(sequence, 2, 1000000, now));
        CHECK(L::prepareState(initial, current, 2, now, 1000000, reads(), BLOCKS));
        read(initial);
        CHECK(owner.beginOperation(sequence, 3, now + DEADLINE, now));
        axis.units.commandPolarity = polarity;
        M::FiniteVelocityRequest initialRequest = request(initialRate);
        if (polarity < 0) {
            initialRequest.velocity.frame = M::CoordinateFrame::MOTOR;
            initialRequest.velocity.value = M::Rational(-initialRate);
        }
        CHECK(L::prepareVelocity(run, live, current, initial, identity, evidence(),
                                  initialRequest, ramps, axis, 3, now,
                                  now + DEADLINE, options()));
        proof.gate = L::VelocityGate::MEASURED;
        proof.record = 5201;
        proof.model = live.model;
        proof.firmware = run.evidence.firmware;
        proof.rateChangeQualified = true;
        proof.maximumForwardRpm = 300;
    }
    void turn() {
        now += TICK;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (wire.pending[served])
                break;
            const M::Frame& sent = wire.sent[served];
            M::Frame response;
            const bool reply = drive.respond(sent, response);
            if (applyChange && sent.data[0] != 0x40 && indexOf(sent) == 0x60ff &&
                (drive.monitor & L::MONITOR_RUNNING))
                drive.velocity = drive.targetRpm;
            if (reply)
                wire.push(response, now, owner.controllerEpoch());
        }
        M::Frame heartbeat = frame(0x701, 1);
        heartbeat.data[0] = 5;
        wire.push(heartbeat, now, owner.controllerEpoch());
        CHECK(owner.service(now, budget()));
    }
    C::SdoCompletion transfer(const C::SdoTransfer& work) {
        O::BusRequest input;
        input.sequence = sequence;
        input.transfer = work;
        O::RequestId id;
        CHECK(owner.admit(input, now, id) == O::BusAdmission::ACCEPTED);
        for (unsigned bound = 0; !owner.result(id) && bound < 30; ++bound)
            turn();
        const O::Completion* terminal = owner.result(id);
        CHECK(terminal);
        const C::SdoCompletion result = terminal->service;
        const Saved<O::Completion> held(*terminal);
        turn();
        held.unchanged(*owner.result(id));
        CHECK(owner.canRelease(id) && owner.release(id));
        return result;
    }
    void read(L::ReadContext& into) {
        while (into.state == M::ReadState::ACTIVE) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            const C::SdoCompletion terminal = transfer(work.transfer);
            CHECK(L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    void step() {
        L::PreparedVelocity work;
        CHECK(L::nextVelocity(run, observation, live, current, now, work));
        if (work.observe) {
            CHECK(L::beginVelocityObservation(run, observation, live, current, now));
        } else if (work.ready) {
            if (work.transfer.direction == C::SdoDirection::DOWNLOAD) {
                CHECK(L::validateVelocityAdmission(run, live, current, axis, now));
                CHECK(L::noteVelocityTransmission(run, live, current, axis, nullptr, 0, now));
                if (run.phase == L::VelocityPhase::CHANGE)
                    CHECK(run.change.uncertain && run.change.effectsPossible &&
                          run.change.execution == M::ActionExecution::UNKNOWN &&
                          run.change.firstUncertainUs == now);
                L::PreparedVelocity duplicate;
                CHECK(L::nextVelocity(run, observation, live, current, now, duplicate) && !duplicate.ready);
            }
            const C::SdoCompletion terminal = transfer(work.transfer);
            CHECK(L::advanceVelocity(run, observation, live, current, terminal, now));
        } else {
            CHECK(work.eligibleUs > now);
            now = work.eligibleUs;
        }
    }
    void running() {
        for (unsigned bound = 0; !run.runningObserved && bound < 100; ++bound)
            step();
        CHECK(run.runningObserved && run.phase == L::VelocityPhase::WAITING);
    }
    M::Status change() {
        const M::Status s =
            L::prepareVelocityChange(run, live, current, axis, replacement, proof, now);
        CHECK(run.retryClass == M::RetryClass::MOTION_LIKE); // A change keeps the run's class.
        return s;
    }
    void reject() {
        const Saved<L::VelocityContext> before(run);
        const unsigned sent = wire.sentCount;
        CHECK(!L::validateVelocityChange(run, live, current, axis, replacement, proof, now));
        CHECK(!change());
        before.unchanged(run);
        CHECK(wire.sentCount == sent);
    }
    // 60FF downloads. A client abort (80) naming 60FF is no write; the failed-change case
    // asserts it separately.
    unsigned changes() const {
        unsigned count = 0;
        for (unsigned i = 0; i < wire.sentCount; ++i)
            if (wire.sent[i].data[0] != 0x40 && wire.sent[i].data[0] != 0x80 &&
                indexOf(wire.sent[i]) == 0x60ff)
                ++count;
        return count;
    }
};

void changedRateUsesOneWriteAndOriginalStop() {
    for (unsigned decrease = 0; decrease != 2; ++decrease) {
        Rig r;
        r.running();
        const Saved<M::FiniteVelocityRequest> initial(r.run.request);
        const Saved<M::PreparedVelocityTarget> initialRate(r.run.rate);
        const Saved<L::VelocityOptions> policy(r.run.options);
        const uint64_t due = r.run.stopDueUs, deadline = r.run.deadlineUs;
        r.replacement = request(decrease ? 60 : 180).velocity;
        CHECK(L::validateVelocityChange(r.run, r.live, r.current, r.axis, r.replacement, r.proof, r.now));
        CHECK(r.change());
        r.replacement.value = M::Rational(1); // Admission owns the exact original input.
        r.step();
        CHECK(r.run.change.execution == M::ActionExecution::ACKNOWLEDGED &&
              r.run.change.serviceEvidencePresent && r.changes() == 2);
        CHECK(!r.run.change.firstRequestedRateReport.present);
        // A transition can retain the old velocity after ACK, including on a decrease.
        r.drive.velocity = 120;
        for (unsigned bound = 0; !r.run.change.firstPostChange.present && bound < 12; ++bound)
            r.step();
        CHECK(r.run.change.firstPostChange.present && !r.run.change.firstRequestedRateReport.present);
        CHECK(r.run.firstFailure == M::ActionOutcome::NONE &&
              r.run.change.firstPostChange.velocity == 120);
        r.drive.velocity = decrease ? 60 : 180;
        for (unsigned bound = 0; !r.run.change.firstRequestedRateReport.present && bound < 12; ++bound)
            r.step();
        CHECK(r.run.change.firstRequestedRateReport.velocity == (decrease ? 60 : 180));
        CHECK(r.run.change.firstRequestedRateReport.earliestUs >= r.run.change.serviceEvidence.completedUs);
        CHECK(r.services.node(0)->heartbeat.present);
        r.reject(); // Retained history is never overwritten.
        initial.unchanged(r.run.request);
        initialRate.unchanged(r.run.rate);
        policy.unchanged(r.run.options);
        CHECK(r.run.stopDueUs == due && r.run.deadlineUs == deadline);
        bool stopDue = false;
        CHECK(L::serviceVelocity(r.run, r.live, r.current, due, false, stopDue));
        CHECK(stopDue && r.run.phase == L::VelocityPhase::STOPPING && r.changes() == 2);
        r.now = due;
        r.reject();
    }
}
void refusalsAreAtomic() {
    for (unsigned bad = 0; bad < 21; ++bad) {
        Rig r;
        r.running();
        switch (bad) {
        case 0: r.proof.gate = L::VelocityGate::CLOSED; break;
        case 1: r.proof.record = 0; break;
        case 2: r.proof.rateChangeQualified = false; break;
        case 3: r.proof.firmware.productCode++; break; // A product field; firmware only flags.
        case 4: r.proof.model = L::SettingsModel::UNKNOWN; break;
        case 5: r.replacement.value = M::Rational(0); break;
        case 6: r.replacement.value = M::Rational(-60); break;
        case 7: r.replacement.value = M::Rational(3001); break;
        case 8: r.replacement.configurationGeneration++; break;
        case 9: r.axis.referenceGeneration++; break;
        case 10: r.live.target.nodeGeneration++; break;
        case 11: r.current.channel.binding.bindingGeneration++; break;
        case 12: r.run.stopRequested = true; break;
        case 13: r.run.withdrawal = M::VelocityInterruption::STOP; break;
        case 14: r.run.readUncertain = true; break;
        case 15: r.run.commandHistory[1].effectUncertain = true; break;
        case 16: r.now = r.run.stopDueUs; break;
        case 17: r.now = r.run.stopDueUs - 8 * (QUEUE + RESPONSE); break;
        case 18: r.run.runningPolls = r.run.options.maxPolls; r.run.observations += r.run.runningPolls; break;
        case 19: CHECK(L::beginVelocityObservation(r.run, r.observation, r.live, r.current, r.run.eligibleUs));
                 r.now = r.run.servicedUs; break;
        case 20: r.run.latest.earliestUs = 0; r.run.options.maximumAgeUs = 20000; break;
        }
        r.reject();
    }
    Rig notRunning;
    notRunning.reject();
}
void explicitExperimentCannotQualifyItself() {
    Rig r(30);
    r.running();
    r.replacement = request(60).velocity;
    r.proof.gate = L::VelocityGate::ATTENDED_EXPERIMENT;
    r.proof.maximumForwardRpm = 60;
    r.reject(); // Qualification claim is forbidden in an experiment.
    r.proof.rateChangeQualified = false;
    CHECK(r.change());
    for (unsigned bound = 0; !r.run.change.firstRequestedRateReport.present && bound < 15; ++bound)
        r.step();
    CHECK(r.run.change.firstRequestedRateReport.present &&
          r.run.change.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT &&
          !r.run.change.evidence.rateChangeQualified);
}
void ignoredChangeRemainsRaw() {
    Rig r;
    r.running();
    r.applyChange = false;
    CHECK(r.change());
    for (unsigned bound = 0; !r.run.change.firstPostChange.present && bound < 15; ++bound)
        r.step();
    CHECK(r.run.change.execution == M::ActionExecution::ACKNOWLEDGED &&
          !r.run.change.firstRequestedRateReport.present && r.run.firstFailure == M::ActionOutcome::NONE);
}
void engineeringPolarityDoesNotChangeTheWireDirection() {
    Rig r(120, -1);
    r.running();
    CHECK(r.run.request.velocity.value.numerator == -120 && r.run.rate.effectiveNative == 120);
    r.replacement.value = M::Rational(180);
    r.replacement.frame = M::CoordinateFrame::MOTOR;
    r.reject(); // This positive engineering rate would reverse the positive native run.
    r.replacement.value = M::Rational(-180);
    CHECK(r.change());
    CHECK(r.run.change.request.value.numerator == -180 && r.run.change.rate.effectiveNative == 180);
    r.step();
    CHECK(r.run.change.execution == M::ActionExecution::ACKNOWLEDGED && r.drive.targetRpm == 180);
    CHECK(r.run.rate.effectiveNative == 120 && r.changes() == 2);
}
void failedChangeRequestsStopWithoutReplay() {
    for (unsigned lost = 0; lost != 2; ++lost) {
        Rig r;
        r.running();
        CHECK(r.change());
        if (lost)
            r.drive.muteIndex = 0x60ff;
        else
            r.drive.abortIndex = 0x60ff;
        const unsigned write = r.wire.sentCount;
        r.step();
        CHECK(r.run.phase == L::VelocityPhase::STOPPING && r.run.stopRequested && r.run.needsStop);
        CHECK(r.run.change.serviceEvidencePresent && r.changes() == 2);
        CHECK(OwnerRig::sdoFrame(r.wire.sent[write], 0x601, 0x23, 0x60ff, 0) &&
              valueOf(r.wire.sent[write]) == 180);
        if (lost) {
            // Prompt 55, default reuse policy: the response timeout stages one cleanup abort
            // naming 60FF:00, then the owner issues its one marker read of 1000:00. Neither is a
            // replay of the change.
            CHECK(r.wire.sentCount == write + 3 && r.drive.clientAborts == 1 &&
                  OwnerRig::isTimeoutAbort(r.wire.sent[write + 1], 0x601, 0x60ff) &&
                  OwnerRig::isMarkerRead(r.wire.sent[write + 2], 0x601));
            CHECK(r.owner.stats().markers == 1 && r.services.unresolved(0));
        } else {
            // A server abort settles the exchange: no client abort, no marker, route open.
            CHECK(r.wire.sentCount == write + 1 && !r.drive.clientAborts &&
                  !r.owner.stats().markers && !r.services.unresolved(0));
        }
        CHECK(r.run.firstFailure == (lost ? M::ActionOutcome::SERVICE_TIMEOUT : M::ActionOutcome::PROTOCOL_REJECTED));
        CHECK(r.run.change.firstFailure == r.run.firstFailure);
        if (lost)
            CHECK(r.run.change.uncertain && r.run.change.firstUncertainUs &&
                  r.run.change.execution == M::ActionExecution::UNKNOWN);
        else
            CHECK(r.run.change.execution == M::ActionExecution::REJECTED &&
                  r.run.change.serviceEvidence.abortCode == 0x08000023);
        const unsigned sent = r.wire.sentCount;
        L::PreparedVelocity work;
        CHECK(L::nextVelocity(r.run, r.observation, r.live, r.current, r.now, work) && !work.ready);
        r.reject();
        CHECK(r.wire.sentCount == sent);
        if (!lost)
            continue;
        // The fake answers the marker, so the route reopens under accepted premises: the node's
        // reservation is invalidated and the change's uncertainty and stop request stay.
        const Saved<L::VelocityContext> failed(r.run);
        for (unsigned bound = 0; r.services.unresolved(0) && bound < 10; ++bound)
            r.turn();
        O::Health health;
        CHECK(!r.services.unresolved(0) && r.services.requalifications(0) == 1 &&
              r.owner.health(r.current.channel.binding, health) && health.acceptedCorrelation &&
              health.reusePolicy == O::SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER);
        CHECK(r.owner.sequence(r.sequence) == nullptr && r.wire.sentCount == sent &&
              r.changes() == 2 && r.owner.stats().markers == 1);
        failed.unchanged(r.run);
        CHECK(r.run.change.uncertain && r.run.needsStop && r.run.stopRequested);
    }
}
// Prompt 57 identity policy: a change record of another firmware is admitted, and its own comparison with
// the run's observed identity says so; the run's comparison stays its own.
void changeRecordOfAnotherFirmwareOnlyFlags() {
    for (unsigned field = 0; field < 4; ++field) {
        Rig r;
        r.running();
        CHECK(!r.run.identity.firmwareUnqualified && r.run.identity.outcome == L::IdentityOutcome::CONFIRMED);
        if (field == 0)
            ++r.proof.firmware.hardwareVersion;
        else if (field == 1)
            ++r.proof.firmware.softwareVersion;
        else if (field == 2)
            ++r.proof.firmware.revision;
        else
            ++r.proof.firmware.firmwareCode;
        r.replacement = request(180).velocity;
        CHECK(r.change());
        const L::IdentityCheck& id = r.run.change.identity;
        CHECK(id.outcome == L::IdentityOutcome::CONFIRMED && id.expectedMatch && id.firmwareUnqualified &&
              id.basis == L::IdentityBasis::MEASURED);
        CHECK(id.expected.revision == r.proof.firmware.revision &&
              id.expected.firmwareCode == r.proof.firmware.firmwareCode &&
              id.observed.firmwareCode == r.run.identity.observed.firmwareCode);
        CHECK(!r.run.identity.firmwareUnqualified);
    }
}
// Prompt 60 stage (c): a run whose ramps came from the acceleration form admits no change, before
// any frame, because the measured change takes the whole ramp time; a native run still changes.
void accelerationFormRunAdmitsNoChange() {
    const M::AxisConfig axis = axisConfig();
    M::AccelerationRequest a;
    a.configurationGeneration = axis.binding.configurationGeneration;
    a.value = M::Rational(600); // 120 rpm at 600 rpm/s: 200 ms.
    a.unit = M::AccelerationUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::TimeUnit::SECOND);
    a.frame = M::CoordinateFrame::NATIVE;
    a.maximumAccelerationError = M::Rational(1);
    L::PreparedAccelerationRamp ramp;
    CHECK(L::prepareAccelerationRamp(ramp, L::RampUse::VELOCITY_START, a, request(120).velocity, axis));
    L::VelocityProfile converted;
    converted.rampUpTimeMs = converted.rampDownTimeMs = ramp.time.effectiveMs;
    converted.rampSource = ramp.source;
    Rig r(120, 1, converted);
    CHECK(r.run.profile.rampSource.acceleration);
    r.running();
    const M::Status s = L::validateVelocityChange(r.run, r.live, r.current, r.axis, r.replacement, r.proof, r.now);
    CHECK(!s && s.code == M::Err::INVALID_CONFIG && s.detail == int32_t(M::VelocityError::RAMP_CHANGE));
    r.reject();
    CHECK(r.changes() == 1);
    // The same ramp times as native milliseconds change as before.
    L::VelocityProfile native;
    native.rampUpTimeMs = native.rampDownTimeMs = ramp.time.effectiveMs;
    Rig n(120, 1, native);
    n.running();
    CHECK(L::validateVelocityChange(n.run, n.live, n.current, n.axis, n.replacement, n.proof, n.now));
}
} // namespace

int main() {
    accelerationFormRunAdmitsNoChange();
    changedRateUsesOneWriteAndOriginalStop();
    refusalsAreAtomic();
    changeRecordOfAnotherFirmwareOnlyFlags();
    explicitExperimentCannotQualifyItself();
    ignoredChangeRemainsRaw();
    engineeringPolarityDoesNotChangeTheWireDirection();
    failedChangeRequestsStopWithoutReplay();
    std::printf("velocity change: %u checks\n", checks);
}
