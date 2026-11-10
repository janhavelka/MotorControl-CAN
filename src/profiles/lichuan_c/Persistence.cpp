// SPDX-License-Identifier: MIT
#include "ProfileEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Persistence.h>
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <new>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
Status error(ReadError reason, const char* text) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), text);
}
bool optionsValid(const ReadOptions& o) {
    return o.queueTimeoutUs && o.responseTimeoutUs && !o.pdoNumber &&
           o.identityWidths == IdentityWidths::DECLARED_U16 &&
           o.countWidths == CountWidths::DECLARED_U8;
}
bool reviewed(const PersistenceReview& v, const AxisReservation& r, PersistenceKind kind) {
    return sameTarget(v.target, r.target) && v.kind == kind && v.fixtureRecord &&
           v.firmwareRecord && v.procedureRecord && v.authorizationRecord &&
           (kind == PersistenceKind::SAVE ||
            (kind == PersistenceKind::FACTORY_RESTORE && v.backupRecord && v.routeBackRecord));
}
bool sameIdentity(const IdentityObservation& a, const IdentityObservation& b);
Status identitySnapshot(const ReadContext& read, const ReadTarget& target, uint64_t after,
                        uint64_t now, uint64_t age, const IdentityObservation* before,
                        IdentityObservation* output) {
    IdentityObservation value;
    if (!getIdentity(read, target, now, age, value) || value.window.earliestUs < after)
        return error(ReadError::CORRELATION, "fresh reserved identity required");
    if (before && !sameIdentity(*before, value))
        return error(ReadError::TARGET, "reconciliation identity differs from original specimen");
    if (output)
        *output = value;
    return Ok();
}
Status configurationSnapshot(const DriverContext& read, const ReadTarget& target, uint64_t after,
                             uint64_t now, uint64_t age, DriverObservation* output) {
    DriverObservation value;
    if (!getDriver(read, target, now, age, value) ||
        value.group != DriverGroup::MOTION_PARAMETERS || value.window.earliestUs < after)
        return error(ReadError::CORRELATION, "fresh reserved motion parameters required");
    if (output)
        *output = value;
    return Ok();
}
Status stateSnapshot(const ReadContext& read, StateBlock block, const ReadTarget& target,
                     uint64_t after, uint64_t now, uint64_t age, StateObservation* output) {
    StateObservation value;
    if (!getStateBlock(read, block, target, now, age, value) || value.window.earliestUs < after)
        return error(ReadError::CORRELATION, "fresh reserved state required");
    // Same conservative host policy as settings, without an enable preamble or torque inference.
    if (block == StateBlock::MONITORS &&
        (value.monitor & (MONITOR_RUNNING | MONITOR_ALARM | ~uint32_t(MONITOR_DOCUMENTED_BITS))))
        return error(ReadError::STATE,
                     "persistence requires no running, alarm or undocumented monitor bit");
    if (output)
        *output = value;
    return Ok();
}
// Validate without an output first, then publish from the same stable borrowed input. Keeping
// each checked observation in its own helper avoids a 2496-byte aggregate stack temporary.
Status snapshots(const PersistencePrerequisites& p, const ReadTarget& target, uint64_t after,
                 uint64_t now, uint64_t age, const IdentityObservation* before,
                 PersistenceRecord* output) {
    if (!age || p.identity.source != ReadSource::SDO ||
        p.configuration.read.source != ReadSource::SDO || p.state.source != ReadSource::SDO ||
        p.state.state != ReadState::SUCCEEDED || p.identity.uncertain ||
        p.configuration.read.uncertain || p.state.uncertain)
        return error(ReadError::CORRELATION,
                     "fresh complete reserved persistence snapshots required");
    Status s = identitySnapshot(p.identity, target, after, now, age, before,
                                output ? &output->identity : nullptr);
    if (!s)
        return s;
    s = configurationSnapshot(p.configuration, target, after, now, age,
                              output ? &output->configuration : nullptr);
    if (!s)
        return s;
    s = stateSnapshot(p.state, StateBlock::STATUS, target, after, now, age,
                      output ? &output->status : nullptr);
    if (!s)
        return s;
    return stateSnapshot(p.state, StateBlock::MONITORS, target, after, now, age,
                         output ? &output->monitors : nullptr);
}
Binding expectedBinding(const PersistenceContext& c) {
    Binding b = c.reservation.target.channel.binding;
    if (c.transmissionNoted)
        ++b.configurationGeneration;
    return b;
}
Status current(const PersistenceContext& c, const AxisReservation& live, const Binding& binding,
               uint64_t now) {
    if (!sameReservation(c.reservation, live) || !sameBinding(binding, expectedBinding(c)))
        return error(ReadError::TARGET, "persistence reservation or configuration changed");
    if (c.state == PersistenceState::EMPTY || c.state > PersistenceState::FAILED ||
        c.kind > PersistenceKind::FACTORY_RESTORE || !reviewed(c.review, c.reservation, c.kind) ||
        !c.operation || !c.attemptOrdinal || !c.maximumAgeUs || !optionsValid(c.options) ||
        c.deadlineUs <= c.startedUs || now < c.servicedUs ||
        c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX ||
        c.attempts != (c.transmissionNoted ? 1 : 0) || c.effectsPossible != c.transmissionNoted ||
        c.prerequisitesWithdrawn != c.transmissionNoted)
        return error(ReadError::STATE, "invalid persistence context or time");
    return Ok();
}
CANOPEN::SdoTransfer work(const PersistenceContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.step = 1;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    t.index = 0x2300;
    t.size = 2;
    t.data[0] = c.kind == PersistenceKind::SAVE ? 2 : 1;
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.options.queueTimeoutUs;
    t.responseTimeoutUs = c.options.responseTimeoutUs;
    return t;
}
bool freshBefore(const PersistenceContext& c, uint64_t now) {
    return now >= c.before.identity.window.earliestUs &&
           now - c.before.identity.window.earliestUs <= c.maximumAgeUs &&
           now >= c.before.configuration.window.earliestUs &&
           now - c.before.configuration.window.earliestUs <= c.maximumAgeUs &&
           now >= c.before.status.window.earliestUs &&
           now - c.before.status.window.earliestUs <= c.maximumAgeUs &&
           now >= c.before.monitors.window.earliestUs &&
           now - c.before.monitors.window.earliestUs <= c.maximumAgeUs;
}
void finish(PersistenceContext& c, ReadOutcome outcome, uint64_t now) {
    c.outcome = outcome;
    c.servicedUs = now;
    c.state =
        outcome == ReadOutcome::SUCCESS ? PersistenceState::ACKNOWLEDGED : PersistenceState::FAILED;
    c.status =
        outcome == ReadOutcome::SUCCESS ? Ok()
        : outcome == ReadOutcome::PROTOCOL_REJECTED
            ? Status(Err::PROTOCOL_ERROR, 0, "persistence native service rejected")
            : error(outcome == ReadOutcome::DEADLINE || outcome == ReadOutcome::SERVICE_TIMEOUT
                        ? ReadError::TIME
                        : ReadError::STATE,
                    "persistence command failed");
}
Status prepare(PersistenceContext& output, const AxisReservation& r, const ReadTarget& target,
               const PersistenceReview& review, const PersistencePrerequisites& prior,
               PersistenceKind kind, uint32_t operation, uint32_t ordinal, uint64_t now,
               uint64_t deadline, const ReadOptions& options, uint64_t age) {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &target, sizeof target) ||
        overlaps(&output, sizeof output, &review, sizeof review) ||
        overlaps(&output, sizeof output, &prior, sizeof prior) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "persistence preparation overlaps input");
    if (!validReservation(r) || !sameTarget(r.target, target) || !operation || !ordinal || !age ||
        r.target.channel.binding.configurationGeneration == UINT32_MAX || !optionsValid(options) ||
        !exchangeFits(deadline, now, options.queueTimeoutUs, options.responseTimeoutUs))
        return error(ReadError::ARGUMENT, "invalid persistence candidate, reservation or budget");
    if (!reviewed(review, r, kind))
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(ReadError::TARGET),
                      "exact persistence procedure, firmware and restoration route require review");
    const Status checked = snapshots(prior, target, r.acquiredUs, now, age, nullptr, nullptr);
    if (!checked)
        return checked;
    // Inputs are disjoint and every rejectable condition is checked; avoid a second full context.
    new (static_cast<void*>(&output)) PersistenceContext();
    output.reservation = r;
    output.review = review;
    // The same immutable, nonoverlapping reads just passed; this second pass cannot reject.
    (void)snapshots(prior, target, r.acquiredUs, now, age, nullptr, &output.before);
    output.before.model = r.model;
    output.before.fixtureRecord = review.fixtureRecord;
    output.before.firmwareRecord = review.firmwareRecord;
    output.kind = kind;
    output.state = PersistenceState::WRITING;
    output.operation = operation;
    output.attemptOrdinal = ordinal;
    output.startedUs = output.servicedUs = now;
    output.deadlineUs = deadline;
    output.maximumAgeUs = age;
    output.options = options;
    return Ok();
}
bool sameIdentity(const IdentityObservation& a, const IdentityObservation& b) {
    return a.deviceType == b.deviceType && a.hardwareVersion == b.hardwareVersion &&
           a.hardwareVersionWidth == b.hardwareVersionWidth &&
           a.softwareVersion == b.softwareVersion &&
           a.softwareVersionWidth == b.softwareVersionWidth &&
           a.identityEntries == b.identityEntries && a.vendorId == b.vendorId &&
           a.productCode == b.productCode && a.revision == b.revision &&
           a.modelCode == b.modelCode && a.firmwareCode == b.firmwareCode;
}
} // namespace

Status prepareSave(PersistenceContext& c, const AxisReservation& r, const ReadTarget& target,
                   const PersistenceReview& review, const PersistencePrerequisites& prior,
                   uint32_t operation, uint32_t ordinal, uint64_t now, uint64_t deadline,
                   const ReadOptions& options, uint64_t age) noexcept {
    return prepare(c, r, target, review, prior, PersistenceKind::SAVE, operation, ordinal, now,
                   deadline, options, age);
}
Status prepareFactoryRestore(PersistenceContext& c, const AxisReservation& r,
                             const ReadTarget& target, const PersistenceReview& review,
                             const PersistencePrerequisites& prior, uint32_t operation,
                             uint32_t ordinal, uint64_t now, uint64_t deadline,
                             const ReadOptions& options, uint64_t age) noexcept {
    return prepare(c, r, target, review, prior, PersistenceKind::FACTORY_RESTORE, operation,
                   ordinal, now, deadline, options, age);
}
Status nextPersistence(const PersistenceContext& c, const AxisReservation& live,
                       const Binding& binding, uint64_t now, PreparedRead& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &binding, sizeof binding))
        return error(ReadError::OVERLAP, "persistence work overlaps input");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    if (c.state != PersistenceState::WRITING || c.transmissionNoted) {
        output.ready = false;
        return Ok();
    }
    if (!freshBefore(c, now) ||
        !exchangeFits(c.deadlineUs, now, c.options.queueTimeoutUs, c.options.responseTimeoutUs))
        return error(ReadError::TIME, "persistence prerequisites or service budget expired");
    PreparedRead p;
    p.ready = true;
    p.transfer = work(c);
    output = p;
    return Ok();
}
Status notePersistenceTransmission(PersistenceContext& c, const AxisReservation& live,
                                   Binding& binding, uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &binding, sizeof binding) ||
        overlaps(&binding, sizeof binding, &live, sizeof live))
        return error(ReadError::OVERLAP, "persistence notice overlaps mutable input");
    const Status checked = current(c, live, binding, now);
    if (!checked)
        return checked;
    if (c.state != PersistenceState::WRITING || c.transmissionNoted || !freshBefore(c, now) ||
        !exchangeFits(c.deadlineUs, now, c.options.queueTimeoutUs, c.options.responseTimeoutUs))
        return error(ReadError::STATE, "no eligible persistence command handoff");
    ++binding.configurationGeneration;
    c.transmissionNoted = c.effectsPossible = c.prerequisitesWithdrawn = c.uncertain = true;
    c.execution = ActionExecution::UNKNOWN;
    c.attempts = 1;
    c.servicedUs = now;
    return Ok();
}
Status advancePersistence(PersistenceContext& c, const AxisReservation& original,
                          const Binding& submittedBinding, const CANOPEN::SdoCompletion& e,
                          uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &original, sizeof original) ||
        overlaps(&c, sizeof c, &submittedBinding, sizeof submittedBinding) ||
        overlaps(&c, sizeof c, &e, sizeof e))
        return error(ReadError::OVERLAP, "persistence terminal overlaps input");
    // Historical owner results remain ingestible after reboot revokes the actual reservation.
    // These snapshots correlate old work only; next/note still need current live authority.
    const Status checked = current(c, original, submittedBinding, now);
    if (!checked)
        return checked;
    if (c.state != PersistenceState::WRITING || !CANOPEN::sameSdoWork(e.transfer, work(c)) ||
        e.nodeGeneration != c.reservation.target.nodeGeneration || e.admittedUs < c.startedUs ||
        e.admittedUs > e.completedUs || e.admittedUs >= c.deadlineUs ||
        e.completedUs < c.servicedUs || e.completedUs > now ||
        !CANOPEN::checkDownloadTerminal(e, c.transmissionNoted))
        return error(ReadError::CORRELATION, "persistence terminal does not match exact command");
    c.writeEvidence = e;
    c.writeEvidencePresent = true;
    c.execution = e.outcome == CANOPEN::SdoOutcome::SUCCESS        ? ActionExecution::ACKNOWLEDGED
                  : e.outcome == CANOPEN::SdoOutcome::SERVER_ABORT ? ActionExecution::REJECTED
                  : c.effectsPossible                              ? ActionExecution::UNKNOWN
                                      : ActionExecution::NOT_TRANSMITTED;
    c.uncertain = c.effectsPossible && e.outcome != CANOPEN::SdoOutcome::SUCCESS;
    finish(c,
           now >= c.deadlineUs || e.completedUs >= c.deadlineUs ? ReadOutcome::DEADLINE
                                                                : readOutcome(e.outcome),
           now);
    return Ok();
}
Status advancePersistence(PersistenceContext& c, ReadOutcome reason, uint64_t now) noexcept {
    if (c.state != PersistenceState::WRITING || now < c.servicedUs ||
        (reason != ReadOutcome::CANCELLED && reason != ReadOutcome::DEADLINE &&
         reason != ReadOutcome::INVALIDATED) ||
        (reason == ReadOutcome::DEADLINE && now < c.deadlineUs))
        return error(ReadError::STATE, "invalid persistence local withdrawal");
    finish(c, now >= c.deadlineUs ? ReadOutcome::DEADLINE : reason, now);
    return Ok();
}
Status verifyPersistence(PersistenceContext& c, const PersistenceVerification& v,
                         uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &v, sizeof v))
        return error(ReadError::OVERLAP, "persistence verification overlaps context");
    if ((c.state != PersistenceState::ACKNOWLEDGED && c.state != PersistenceState::FAILED) ||
        !c.effectsPossible || now < c.servicedUs || !v.routeRecord || !v.maximumAgeUs ||
        v.model != c.before.model || v.fixtureRecord != c.before.fixtureRecord ||
        v.firmwareRecord != c.before.firmwareRecord || !v.current.nodeGeneration ||
        !CANOPEN::validateSdoChannel(v.current.channel))
        return error(ReadError::STATE,
                     "terminal effect and exact current specimen review required");
    const ReadTarget& old = c.reservation.target;
    const Binding& a = old.channel.binding;
    const Binding& b = v.current.channel.binding;
    if (a.logicalAxisId != b.logicalAxisId || a.endpoint.profile != b.endpoint.profile ||
        a.endpoint.bus != b.endpoint.bus || b.bindingGeneration < a.bindingGeneration ||
        b.configurationGeneration <= a.configurationGeneration ||
        v.current.nodeGeneration < old.nodeGeneration)
        return error(ReadError::TARGET,
                     "reconciliation lost original axis or withdrew no configuration");
    CANOPEN::SdoChannel expected = old.channel;
    expected.binding.configurationGeneration = b.configurationGeneration;
    if (b.bindingGeneration == a.bindingGeneration &&
        !CANOPEN::sameSdoChannel(expected, v.current.channel))
        return error(ReadError::TARGET,
                     "changed reconciliation route needs a fresh binding generation");
    uint64_t after = c.writeEvidencePresent ? c.writeEvidence.completedUs : c.servicedUs;
    if (v.powerCycleObserved) {
        if (!v.powerCycleRecord || !v.noInterveningWritesRecord ||
            !sameTarget(v.powerCycleTarget, v.current) || v.powerCycleUs < after ||
            v.powerCycleUs > now || v.current.nodeGeneration <= old.nodeGeneration)
            return error(ReadError::TIME,
                         "power-cycle boundary, uninterrupted history or retirement missing");
        after = v.powerCycleUs;
    } else if (v.powerCycleRecord || v.powerCycleUs || v.noInterveningWritesRecord ||
               v.powerCycleTarget.nodeGeneration) {
        return error(ReadError::ARGUMENT, "power-cycle metadata without actual observation");
    }
    const Status checked = snapshots(v.observations, v.current, after, now, v.maximumAgeUs,
                                     &c.before.identity, nullptr);
    if (!checked)
        return checked;
    // All checks are complete, including identity equality; stable input makes publication infallible.
    (void)snapshots(v.observations, v.current, after, now, v.maximumAgeUs, &c.before.identity,
                    &c.observed);
    c.observed.model = v.model;
    c.observed.fixtureRecord = v.fixtureRecord;
    c.observed.firmwareRecord = v.firmwareRecord;
    const DriverObservation& x = c.before.configuration;
    const DriverObservation& y = c.observed.configuration;
    uint8_t matching = 0;
    if (x.positionSpeedRpm == y.positionSpeedRpm)
        matching |= 1;
    if (x.rampUpTimeMs == y.rampUpTimeMs)
        matching |= 2;
    if (x.rampDownTimeMs == y.rampDownTimeMs)
        matching |= 4;
    c.verificationKnown = true;
    c.matchingFields = matching;
    c.survivedRestart = c.kind == PersistenceKind::SAVE && v.powerCycleObserved ? matching : 0;
    c.verificationRecord = v.routeRecord;
    c.powerCycleRecord = v.powerCycleRecord;
    c.powerCycleTarget = v.powerCycleTarget;
    c.noInterveningWritesRecord = v.noInterveningWritesRecord;
    c.servicedUs = now;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
