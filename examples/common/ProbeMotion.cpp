// SPDX-License-Identifier: MIT
#include "ProbeMotion.h"
#include <new>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace C = MotorControlCAN::CANOPEN;
namespace O = MotorControlCANOwner;
namespace {
M::Status refused(M::ActionError detail, const char* message) {
    return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(detail), message);
}
bool terminal(M::ActionState state) {
    return state == M::ActionState::SUCCEEDED || state == M::ActionState::FAILED;
}
bool sameTarget(const L::ReadTarget& a, const L::ReadTarget& b) {
    return a.nodeGeneration == b.nodeGeneration &&
           M::sameBinding(a.channel.binding, b.channel.binding) &&
           a.channel.clientToServerId == b.channel.clientToServerId &&
           a.channel.serverToClientId == b.channel.serverToClientId;
}
void abortEvidence(MotionResult& result, const C::SdoCompletion& evidence) {
    if (!result.abortKnown && evidence.abortValid) {
        result.abortKnown = true;
        result.abortCode = evidence.abortCode;
    }
}
void rateValue(MotionValue& result, const M::PreparedVelocityTarget& value) {
    result.present = value.representation == M::NativeRepresentation::INTEGER;
    result.native = value.effectiveNative;
    result.requested = value.requestedNative;
    result.quantization = value.quantization;
    result.exact = value.exactArithmetic;
    result.errorBound = value.absoluteErrorBound;
    result.rateBasis = value.rateBasis;
    result.rateTime = value.rateTime;
}
void targetValue(MotionValue& result, const M::PreparedTarget& value) {
    result.present = value.representation == M::NativeRepresentation::INTEGER;
    result.native = value.effectiveNative;
    result.requested = value.requestedNative;
    result.relative = value.requested.relative;
    result.endpointNative = value.endpointNative;
    result.quantization = value.quantization;
    result.exact = value.exactArithmetic;
    result.errorBound = value.absoluteErrorBound;
}
void actionResult(MotionResult& result, const L::ActionContext& action) {
    result.state = action.state;
    result.outcome = action.outcome;
    result.status = action.status;
    result.execution = action.execution;
    result.completion = action.completion;
    result.uncertain = action.uncertain;
    result.completedUs = action.servicedUs;
    for (unsigned n = 0; n < 3; ++n)
        if (action.commandEvidenceMask & (1u << n))
            abortEvidence(result, action.commandHistory[n]);
    if (action.observationEvidencePresent)
        abortEvidence(result, action.observationEvidence);
}
void stopAttempt(StopAttempt& result, const L::ActionContext& action) {
    result.present = true;
    result.operation = action.operation;
    result.state = action.state;
    result.outcome = action.outcome;
    result.status = action.status;
    result.execution = action.execution;
    result.completion = action.completion;
    result.uncertain = action.uncertain;
    result.commandPresent = (action.commandEvidenceMask & 1u) != 0;
    if (result.commandPresent)
        result.command = action.commandHistory[0];
}
void stopAttempt(StopAttempt& result, const L::BestEffortStop& action) {
    result.present = true;
    result.operation = action.operation;
    result.state = action.state;
    result.outcome = action.outcome;
    result.status = action.status;
    result.execution = action.execution;
    result.completion = M::ActionCompletion::NOT_OBSERVED;
    result.uncertain = action.effectsPossible;
    result.commandPresent = action.commandEvidencePresent;
    if (result.commandPresent)
        result.command = action.commandEvidence;
}
void finishStop(MotionStopResult& result, const StopAttempt& final, uint64_t completedUs) {
    result.result.state = final.state;
    result.result.outcome = final.outcome;
    result.result.status = final.status;
    result.result.execution = final.execution;
    result.result.completion = final.completion;
    result.result.uncertain = final.uncertain;
    result.result.completedUs = completedUs;
    if (result.sequenced.commandPresent)
        abortEvidence(result.result, result.sequenced.command);
    if (result.firstBestEffort.commandPresent)
        abortEvidence(result.result, result.firstBestEffort.command);
    if (result.bestEffort.commandPresent)
        abortEvidence(result.result, result.bestEffort.command);
}
void invalidateStop(MotionStopResult& result, uint64_t now) {
    result.path = StopPath::FINISHED;
    result.result.state = M::ActionState::FAILED;
    result.result.outcome = M::ActionOutcome::INVALIDATED;
    result.result.status =
        refused(M::ActionError::CORRELATION, "retained native stop chain replaced externally");
    result.result.uncertain = true;
    result.result.completedUs = now;
}
} // namespace

ProbeMotion::ProbeMotion(ProbeApp& app, O::CanBusOwner& owner, AxisSession& session,
                         L::ReadTarget& current, M::AxisConfig& axis,
                         M::AxisReference& reference) noexcept
    : app_(app), owner_(owner), session_(session), current_(current), axis_(axis),
      reference_(reference) {}
bool ProbeMotion::matches(const ProbeApp& app, const O::CanBusOwner& owner) const noexcept {
    return &app == &app_ && &owner == &owner_ && app_.usesOwner(owner_) &&
           session_.usesOwner(owner_);
}
bool ProbeMotion::graph() const noexcept {
    return matches(app_, owner_) && session_.isOpen() && app_.axis(session_.binding()) == &session_;
}
bool ProbeMotion::current() const noexcept {
    const O::Sequence* sequence = owner_.sequence(session_.sequence());
    return graph() && sequence && M::sameBinding(current_.channel.binding, session_.binding()) &&
           M::sameBinding(axis_.binding, session_.binding()) &&
           sequence->nodeGeneration == current_.nodeGeneration &&
           owner_.matchesChannel(current_.channel, current_.nodeGeneration);
}
bool ProbeMotion::timeValid(uint64_t now) const noexcept {
    return now >= lastUs_ && now >= owner_.lastTimeUs();
}
M::Status ProbeMotion::prerequisite() const noexcept {
    if (retained())
        return refused(M::ActionError::STATE, "release the retained ordinary motion result first");
    if (!current())
        return refused(M::ActionError::TARGET,
                       "motion needs the registered session and exact live target");
    return M::Ok();
}
M::Status ProbeMotion::admission(uint64_t now, uint64_t deadline) const noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    if (!timeValid(now) || deadline <= now)
        return refused(M::ActionError::TIME, "invalid motion time or exclusive deadline");
    if (nextId_ == UINT32_MAX)
        return refused(M::ActionError::STATE, "motion identities exhausted");
    // A retained stop owns native contexts and history until explicit release. In particular,
    // a velocity run must not overwrite a retained stop when its own duration expires.
    if (stopRetained(L::StopKind::HALT) || stopRetained(L::StopKind::QUICK_STOP))
        return refused(M::ActionError::STATE, "release retained stop results before new motion");
    return M::Ok();
}
M::Status ProbeMotion::hostChange(uint64_t now) const noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    if (!timeValid(now))
        return refused(M::ActionError::TIME, "host axis time precedes the application or owner");
    if (session_.busy() || stopRetained(L::StopKind::HALT) ||
        stopRetained(L::StopKind::QUICK_STOP) || app_.readContext().state != M::ReadState::EMPTY)
        return refused(M::ActionError::STATE,
                       "host axis changes require idle released operation storage");
    return M::Ok();
}
void ProbeMotion::forgetPrerequisites() noexcept {
    moveState_ = velocityState_ = homeState_ = enableState_ = clearState_ = nullptr;
    moveIdentity_ = velocityIdentity_ = homeIdentity_ = enableIdentity_ = actionIdentity_ = nullptr;
    actionState_ = nullptr;
    actionLoopPresent_ = false;
    stored_ = inputs_ = driver_ = nullptr;
    moveReplacementEvidence_ = L::MoveReplacementEvidence();
    moveFeedback_ = L::MoveFeedbackRelation();
    moveFeedbackPresent_ = false;
    velocityChangeEvidence_ = L::VelocityChangeEvidence();
}
M::Status ProbeMotion::configureCommandScale(const M::UnitScale& scale, uint64_t now) noexcept {
    const M::Status valid = hostChange(now);
    if (!valid)
        return valid;
    M::AxisConfig candidate = axis_;
    candidate.units.commandStepsPerMotorTurn = scale;
    M::AxisConfig changed = axis_;
    M::AxisReference witness = reference_;
    witness.nowUs = now;
    M::AxisReference retained;
    const M::Status checked = M::configureAxis(changed, candidate, witness, &retained);
    if (!checked)
        return checked;
    if (!session_.followHostConfiguration(changed.binding, now))
        return refused(M::ActionError::TARGET,
                       "owner/session cannot advance the idle host configuration");
    const L::ReadTarget previous = current_;
    axis_ = changed;
    reference_ = retained;
    current_.channel.binding = changed.binding;
    if (app_.selected() && sameTarget(*app_.selected(), previous))
        app_.followSelectionConfiguration(previous, current_);
    forgetPrerequisites();
    lastUs_ = now;
    return M::Ok();
}
M::Status ProbeMotion::setOrigin(int64_t native, uint64_t now) noexcept {
    const M::Status valid = hostChange(now);
    if (!valid)
        return valid;
    M::AxisConfig candidate = axis_;
    M::AxisReference witness = reference_;
    witness.nowUs = now;
    const M::Status checked = M::setAxisOrigin(candidate, native, witness);
    if (!checked)
        return checked;
    if (!session_.followHostConfiguration(candidate.binding, now))
        return refused(M::ActionError::TARGET, "owner/session cannot advance the idle host origin");
    const L::ReadTarget previous = current_;
    axis_ = candidate;
    reference_ = M::AxisReference();
    current_.channel.binding = candidate.binding;
    if (app_.selected() && sameTarget(*app_.selected(), previous))
        app_.followSelectionConfiguration(previous, current_);
    forgetPrerequisites();
    lastUs_ = now;
    return M::Ok();
}
M::Status ProbeMotion::setMovePrerequisites(const L::ReadContext& state,
                                            const L::IdentityObservation& identity,
                                            const L::MoveEvidence& evidence,
                                            const L::MoveProfile& profile,
                                            const L::DriverContext* stored,
                                            const L::MoveOptions& options) noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    moveState_ = &state;
    moveIdentity_ = &identity;
    moveEvidence_ = evidence;
    moveProfile_ = profile;
    moveOptions_ = options;
    stored_ = stored;
    return M::Ok();
}
M::Status ProbeMotion::setMoveReplacementPrerequisites(const L::MoveReplacementEvidence& evidence,
                                                      const L::MoveFeedbackRelation* feedback) noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    moveReplacementEvidence_ = evidence;
    moveFeedbackPresent_ = feedback != nullptr;
    moveFeedback_ = feedback ? *feedback : L::MoveFeedbackRelation();
    return M::Ok();
}
M::Status ProbeMotion::setVelocityPrerequisites(const L::ReadContext& state,
                                                const L::IdentityObservation& identity,
                                                const L::VelocityEvidence& evidence,
                                                const L::VelocityProfile& profile,
                                                const L::VelocityOptions& options) noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    velocityState_ = &state;
    velocityIdentity_ = &identity;
    velocityEvidence_ = evidence;
    velocityProfile_ = profile;
    velocityOptions_ = options;
    return M::Ok();
}
M::Status ProbeMotion::setVelocityChangePrerequisites(const L::VelocityChangeEvidence& evidence) noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    velocityChangeEvidence_ = evidence;
    return M::Ok();
}
M::Status ProbeMotion::setHomePrerequisites(const L::ReadContext& state,
                                            const L::IdentityObservation& identity,
                                            const L::DriverContext& inputs,
                                            const L::DriverContext& driver,
                                            const L::HomeEvidence& evidence,
                                            const L::HomeOptions& options) noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    homeState_ = &state;
    homeIdentity_ = &identity;
    inputs_ = &inputs;
    driver_ = &driver;
    homeEvidence_ = evidence;
    homeOptions_ = options;
    return M::Ok();
}
M::Status ProbeMotion::setEnablePrerequisites(const L::ReadContext& state,
                                              const L::IdentityObservation& identity,
                                              const L::EnableEvidence& evidence,
                                              const L::ActionOptions& options) noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    enableState_ = &state;
    enableIdentity_ = &identity;
    enableEvidence_ = evidence;
    enableOptions_ = options;
    return M::Ok();
}
M::Status ProbeMotion::setClearPrerequisites(const L::ReadContext& state,
                                             const L::ActionOptions& options) noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    clearState_ = &state;
    clearOptions_ = options;
    return M::Ok();
}
M::Status ProbeMotion::setStopPrerequisites(const L::ReadContext& state,
                                            const StopPolicy& policy) noexcept {
    if (!graph())
        return refused(M::ActionError::TARGET, "stop needs the registered axis session");
    L::ActionOptions checked = policy.options;
    checked.service.identityWidths = L::IdentityWidths::DECLARED_U16;
    checked.service.countWidths = L::CountWidths::DECLARED_U8;
    if (!L::stopOptionsValid(checked) || !policy.deadlineUs ||
        (policy.bestEffortDeadlineUs && policy.bestEffortDeadlineUs < policy.deadlineUs))
        return refused(M::ActionError::ARGUMENT, "invalid explicit stop policy");
    stopState_ = &state;
    stopPolicy_ = policy;
    return M::Ok();
}
RecordSlot recordSlotOf(const L::MeasuredRecord& record) noexcept {
    return record.kind == L::RecordKind::ENABLE     ? RecordSlot::ENABLE
           : record.kind == L::RecordKind::VELOCITY ? RecordSlot::VELOCITY
           : record.position.move == L::RecordMoveKind::RELATIVE ? RecordSlot::POSITION_RELATIVE
                                                                 : RecordSlot::POSITION_ABSOLUTE;
}
bool ProbeMotion::recordCurrent(const RecordEntry& entry) const noexcept {
    // The node generation as the owner sees it: a boot-up advances it there before any application
    // follows it into its live target.
    M::Generation node = 0;
    uint32_t sessions = 0;
    return M::sameBinding(entry.binding.binding, axis_.binding) &&
           M::sameBinding(axis_.binding, current_.channel.binding) &&
           owner_.nodeEpochs(axis_.binding, node, sessions) && entry.binding.nodeGeneration == node &&
           entry.binding.nodeGeneration == current_.nodeGeneration &&
           entry.binding.controllerEpoch == owner_.controllerEpoch();
}
void ProbeMotion::markEvidence() noexcept {
    normal_.attendedExperiment = issuedAttended_;
    normal_.record = issuedRecord_;
}
// A bound, current record for slot, or none (record null). A record bound to older generations refuses:
// falling back to the setter's evidence would silently replace the record the application chose.
M::Status ProbeMotion::boundRecord(RecordSlot slot, const L::MeasuredRecord*& record,
                                   uint32_t& id) const noexcept {
    record = nullptr;
    id = 0;
    if (!records_)
        return M::Ok();
    const RecordEntry& entry = records_->slots[unsigned(slot)];
    if (!entry.loaded || !entry.bound)
        return M::Ok();
    if (!recordCurrent(entry))
        return refused(M::ActionError::STATE,
                       "the bound record's binding, configuration, node or controller epoch changed: use it "
                       "again or drop it");
    record = &entry.record;
    id = entry.id;
    return M::Ok();
}
M::Status ProbeMotion::store(const L::MeasuredRecord& record, RecordSlot& slot) noexcept {
    uint32_t id = 0;
    const M::Status s = L::recordId(id, record);
    if (!s)
        return s;
    const RecordSlot filled = recordSlotOf(record);
    RecordEntry& entry = records_->slots[unsigned(filled)];
    if (entry.loaded)
        return refused(M::ActionError::STATE, "that record slot is occupied: drop it first");
    new (&entry) RecordEntry();
    entry.record = record;
    entry.id = id;
    entry.loaded = true;
    slot = filled;
    return M::Ok();
}
M::Status ProbeMotion::loadRecord(const char* text, size_t size, L::RecordRefusal& refusal,
                                  RecordSlot& slot) noexcept {
    if (!records_)
        return refused(M::ActionError::STATE, "no record storage is attached to this axis");
    // Decoded whole into a local candidate: nothing is stored unless the entire line is valid.
    L::MeasuredRecord candidate;
    const M::Status decoded = L::decodeRecord(candidate, refusal, text, size);
    if (!decoded)
        return decoded;
    return store(candidate, slot);
}
M::Status ProbeMotion::loadRecord(const L::MeasuredRecord& record, L::RecordRefusal& refusal,
                                  RecordSlot& slot) noexcept {
    if (!records_)
        return refused(M::ActionError::STATE, "no record storage is attached to this axis");
    const M::Status valid = L::validateRecord(refusal, record);
    if (!valid)
        return valid;
    return store(record, slot);
}
M::Status ProbeMotion::useRecord(RecordSlot slot, uint64_t now) noexcept {
    if (!records_ || unsigned(slot) >= RECORD_SLOTS || !records_->slots[unsigned(slot)].loaded)
        return refused(M::ActionError::STATE, "no record is loaded in that slot");
    if (!current())
        return refused(M::ActionError::TARGET, "a record binds to the registered session and exact live target");
    if (!timeValid(now))
        return refused(M::ActionError::TIME, "record binding time precedes the application or owner");
    RecordEntry& entry = records_->slots[unsigned(slot)];
    if (entry.record.model != session_.model())
        return refused(M::ActionError::TARGET, "the record was made on another model than the session's");
    entry.binding.binding = axis_.binding;
    entry.binding.nodeGeneration = current_.nodeGeneration;
    entry.binding.controllerEpoch = owner_.controllerEpoch();
    entry.binding.boundUs = now;
    entry.bound = true;
    return M::Ok();
}
bool ProbeMotion::showRecord(RecordSlot slot, RecordView& output) const noexcept {
    if (!records_ || unsigned(slot) >= RECORD_SLOTS || !records_->slots[unsigned(slot)].loaded)
        return false;
    const RecordEntry& entry = records_->slots[unsigned(slot)];
    output.slot = slot;
    output.state = !entry.bound ? RecordState::LOADED
                   : recordCurrent(entry) ? RecordState::BOUND
                                          : RecordState::INVALIDATED;
    output.id = entry.id;
    output.record = entry.record;
    output.binding = entry.bound ? entry.binding : RecordBinding();
    return true;
}
bool ProbeMotion::dropRecord(RecordSlot slot) noexcept {
    if (!records_ || unsigned(slot) >= RECORD_SLOTS || !records_->slots[unsigned(slot)].loaded)
        return false;
    new (&records_->slots[unsigned(slot)]) RecordEntry();
    return true;
}
void ProbeMotion::accept(MotionKind kind, uint32_t operation, uint64_t now) noexcept {
    new (&normal_) MotionResult();
    normal_.kind = kind;
    normal_.id = ++nextId_;
    normal_.nativeOperation = operation;
    normal_.state = M::ActionState::ACTIVE;
    lastUs_ = now;
    snapshotNormal();
}
M::Status ProbeMotion::requestMove(const M::MoveRequest& request, uint64_t now,
                                   uint64_t deadline) noexcept {
    return requestMove(request, moveProfile_, now, deadline);
}
M::Status ProbeMotion::requestMove(const M::MoveRequest& request, const L::MoveProfile& profile,
                                   uint64_t now, uint64_t deadline) noexcept {
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    if (!moveState_ || !moveIdentity_)
        return refused(M::ActionError::STATE, "explicit move prerequisites are missing");
    const M::AxisReference* reference = reference_.nativeKnown ? &reference_ : nullptr;
    // A bound record of the move's slot (prompt 58) supplies the evidence and narrows the request first;
    // the session's own preparation still applies every library limit.
    const bool relative = request.position.relative && !request.position.wrapped;
    const L::MeasuredRecord* record = nullptr;
    uint32_t recordId = 0;
    M::Status checked = boundRecord(relative ? RecordSlot::POSITION_RELATIVE : RecordSlot::POSITION_ABSOLUTE,
                                    record, recordId);
    L::MoveEvidence recordEvidence;
    if (checked && record)
        checked = L::makeMoveEvidence(recordEvidence, *record);
    if (checked && record)
        checked = L::checkRecordMove(*record, recordEvidence, request, profile, axis_,
                                     request.position.wrapped || relative ? &reference_ : reference, now);
    if (!checked)
        return checked;
    const L::MoveEvidence& evidence = record ? recordEvidence : moveEvidence_;
    const M::Status status =
        request.position.wrapped
            ? session_.requestMoveAngle(*moveState_, *moveIdentity_, evidence, request,
                                        profile, stored_, axis_, reference_, moveOptions_, deadline,
                                        now)
        : request.position.relative
            ? session_.requestMoveRelative(*moveState_, *moveIdentity_, evidence, request,
                                           profile, stored_, axis_, reference_, moveOptions_,
                                           deadline, now)
            : session_.requestMove(*moveState_, *moveIdentity_, evidence, request, profile,
                                   stored_, axis_, reference, moveOptions_, deadline, now);
    if (status) {
        moveState_ = nullptr;
        moveIdentity_ = nullptr;
        stored_ = nullptr;
        accept(MotionKind::MOVE, session_.moveAction().operation, now);
        issuedAttended_ = false; // MoveGate has no attended experiment.
        issuedRecord_ = recordId;
        markEvidence();
    }
    return status;
}
M::Status ProbeMotion::requestMoveReplacement(uint32_t id, const L::MoveReplacementRequest& request,
                                             uint64_t now) noexcept {
    if (!timeValid(now) || !current())
        return refused(M::ActionError::TARGET, "replacement needs the current live motion graph and time");
    if (!id || normal_.id != id || normal_.kind != MotionKind::MOVE ||
        normal_.state != M::ActionState::ACTIVE || !nativeMove() || originalMove_.id ||
        nextId_ == UINT32_MAX || session_.stopActive())
        return refused(M::ActionError::STATE, "replacement ID is not an unused active move");
    // A move under a bound measured record admits no replacement: the records measured none (prompt 58).
    if (normal_.record)
        return refused(M::ActionError::STATE,
                       "a move under a measured record admits no change; the record measured none");
    const M::Status status = session_.requestMoveReplacement(
        request, moveReplacementEvidence_, moveFeedbackPresent_ ? &moveFeedback_ : nullptr, now);
    if (!status)
        return status;
    originalMove_ = normal_;
    const L::MoveContext& original = session_.originalMoveAction();
    originalMove_.state = original.state;
    originalMove_.outcome = original.outcome;
    originalMove_.status = original.status;
    originalMove_.execution = original.execution;
    originalMove_.completion = original.completion;
    originalMove_.uncertain = original.uncertain;
    originalMove_.completedUs = original.servicedUs;
    targetValue(originalMove_.target, original.parameters.target);
    rateValue(originalMove_.rate, original.speed);
    accept(MotionKind::MOVE, session_.moveAction().operation, now);
    normal_.relation.originalOperationId = originalMove_.id;
    normal_.relation.replacementOperationId = normal_.id;
    normal_.relation.observationSuperseded = original.relation.observationSuperseded;
    originalMove_.relation = normal_.relation;
    snapshotNormal();
    return M::Ok();
}
M::Status ProbeMotion::requestVelocity(const M::FiniteVelocityRequest& request, L::StopKind kind,
                                       uint64_t now, uint64_t deadline) noexcept {
    return requestVelocity(request, velocityProfile_, kind, now, deadline);
}
M::Status ProbeMotion::requestVelocity(const M::FiniteVelocityRequest& request,
                                       const L::VelocityProfile& profile, L::StopKind kind,
                                       uint64_t now, uint64_t deadline) noexcept {
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    if (!velocityState_ || !velocityIdentity_ || kind > L::StopKind::QUICK_STOP)
        return refused(M::ActionError::ARGUMENT,
                       "velocity prerequisites or stop selection missing");
    L::VelocityOptions options = velocityOptions_;
    options.stopKind = kind;
    // An attended experiment the setter named runs as named: unmeasured ranges stay under it (prompt 58).
    // Otherwise a bound velocity record supplies the evidence for this stop kind and narrows the run.
    const bool attended = velocityEvidence_.gate == L::VelocityGate::ATTENDED_EXPERIMENT;
    const L::MeasuredRecord* record = nullptr;
    uint32_t recordId = 0;
    M::Status checked = attended ? M::Ok() : boundRecord(RecordSlot::VELOCITY, record, recordId);
    L::VelocityEvidence recordEvidence;
    if (checked && record)
        checked = L::makeVelocityEvidence(recordEvidence, *record, kind);
    if (checked && record)
        checked = L::checkRecordVelocity(*record, recordEvidence, request, profile, axis_,
                                         *velocityState_, current_, now, options);
    if (!checked)
        return checked;
    const M::Status status =
        session_.requestVelocity(*velocityState_, *velocityIdentity_,
                                 record ? recordEvidence : velocityEvidence_, request,
                                 profile, axis_, options, deadline, now);
    if (status) {
        velocityState_ = nullptr;
        velocityIdentity_ = nullptr;
        accept(MotionKind::VELOCITY, session_.velocityAction().operation, now);
        issuedAttended_ = attended;
        issuedRecord_ = recordId;
        markEvidence();
    }
    return status;
}
M::Status ProbeMotion::requestVelocityChange(uint32_t id, const M::VelocityRequest& request,
                                            uint64_t now) noexcept {
    if (!timeValid(now) || !current())
        return refused(M::ActionError::TARGET, "velocity change needs the current live motion graph and time");
    if (!id || normal_.id != id || normal_.kind != MotionKind::VELOCITY ||
        normal_.state != M::ActionState::ACTIVE || !nativeVelocity() || session_.stopActive())
        return refused(M::ActionError::STATE, "velocity change ID is not this active velocity run");
    // A run under a bound measured record admits no rate change: the records measured none (prompt 58).
    if (normal_.record)
        return refused(M::ActionError::STATE,
                       "a run under a measured record admits no change; the record measured none");
    const M::Status status = session_.requestVelocityChange(request, velocityChangeEvidence_, now);
    if (status) {
        lastUs_ = now;
        snapshotNormal();
    }
    return status;
}
M::Status ProbeMotion::requestHome(const L::HomeRequest& request, uint64_t now,
                                   uint64_t deadline) noexcept {
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    if (!homeState_ || !homeIdentity_ || !inputs_ || !driver_)
        return refused(M::ActionError::STATE, "method-specific home prerequisites are missing");
    const M::Status status =
        session_.requestHome(*homeState_, *homeIdentity_, *inputs_, *driver_, homeEvidence_,
                             request, axis_, reference_, homeOptions_, deadline, now);
    if (status) {
        homeState_ = nullptr;
        homeIdentity_ = nullptr;
        inputs_ = driver_ = nullptr;
        accept(MotionKind::HOME, session_.homeAction().operation, now);
        coordinateOperation_ = session_.homeAction().operation;
        coordinateHome_ = true;
    }
    return status;
}
M::Status ProbeMotion::setActionPrerequisites(const L::ReadContext& state,
                                              const L::IdentityObservation& identity,
                                              const L::ControlObservation* loop,
                                              const L::ActionOptions& options) noexcept {
    const M::Status valid = prerequisite();
    if (!valid)
        return valid;
    actionState_ = &state;
    actionIdentity_ = &identity;
    actionLoopPresent_ = loop != nullptr;
    actionLoop_ = loop ? *loop : L::ControlObservation();
    actionOptions_ = options;
    return M::Ok();
}
// The three regular requests after admission (prompt 57): the session request on the named report, which a
// successful request consumes. The caller accepts or updates the result.
M::Status ProbeMotion::issueAction(MotionKind kind, uint8_t modeRaw, uint64_t now, uint64_t deadline) noexcept {
    if (kind == MotionKind::ENABLE) {
        // Explicit evidence first (an attended trial); otherwise the regular measured enable from the named
        // action report, which binds to the session's next operation and nothing else.
        const bool regular = !enableState_ || !enableIdentity_;
        if (regular && (!actionState_ || !actionIdentity_))
            return refused(M::ActionError::STATE, "explicit enable or action prerequisites are missing");
        L::EnableEvidence evidence = enableEvidence_;
        if (regular) {
            evidence = L::EnableEvidence();
            evidence.gate = L::EnableGate::MEASURED;
            evidence.modeRaw = 1;
            evidence.operation = session_.nextOperation();
        }
        L::ActionOptions options = regular ? actionOptions_ : enableOptions_;
        // An attended trial named for this enable runs as named (prompt 58). Otherwise a bound enable record
        // replaces the evidence with its own MEASURED evidence and identity reference, and narrows the start
        // report first, on the same named report.
        const bool attended = !regular && evidence.gate == L::EnableGate::ATTENDED_EXPERIMENT;
        const L::MeasuredRecord* record = nullptr;
        uint32_t recordId = 0;
        M::Status checked = attended ? M::Ok() : boundRecord(RecordSlot::ENABLE, record, recordId);
        if (checked && record)
            checked = L::makeEnableEvidence(evidence, options.identityReference, *record,
                                            session_.nextOperation());
        if (checked && record)
            checked = L::checkRecordEnable(*record, evidence, options, regular ? *actionState_ : *enableState_,
                                           regular ? *actionIdentity_ : *enableIdentity_, current_, now);
        if (!checked)
            return checked;
        const M::Status status = regular ? session_.requestEnable(*actionState_, *actionIdentity_, evidence,
                                                                  options, deadline, now)
                                         : session_.requestEnable(*enableState_, *enableIdentity_, evidence,
                                                                  options, deadline, now);
        if (status && regular) {
            actionState_ = nullptr;
            actionIdentity_ = nullptr;
        } else if (status) {
            enableState_ = nullptr;
            enableIdentity_ = nullptr;
        }
        if (status) {
            issuedAttended_ = attended;
            issuedRecord_ = recordId;
        }
        return status;
    }
    if (!actionState_ || !actionIdentity_)
        return refused(M::ActionError::STATE, "explicit action prerequisites are missing");
    const M::Status status =
        kind == MotionKind::MODE_SELECT
            ? session_.requestModeSelect(*actionState_, *actionIdentity_, modeRaw, actionOptions_, deadline, now)
            : session_.requestRelease(*actionState_, *actionIdentity_, actionLoopPresent_ ? &actionLoop_ : nullptr,
                                      L::ReleaseExperiment(), actionOptions_, deadline, now);
    if (status) {
        actionState_ = nullptr;
        actionIdentity_ = nullptr;
        actionLoopPresent_ = false;
    }
    return status;
}
M::Status ProbeMotion::requestEnable(uint64_t now, uint64_t deadline) noexcept {
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    const M::Status status = issueAction(MotionKind::ENABLE, 0, now, deadline);
    if (status) {
        accept(MotionKind::ENABLE, session_.enableAction().operation, now);
        markEvidence();
    }
    return status;
}
M::Status ProbeMotion::requestModeSelect(uint8_t modeRaw, uint64_t now, uint64_t deadline) noexcept {
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    const M::Status status = issueAction(MotionKind::MODE_SELECT, modeRaw, now, deadline);
    if (status)
        accept(MotionKind::MODE_SELECT, session_.clearAction().operation, now);
    return status;
}
M::Status ProbeMotion::requestRelease(uint64_t now, uint64_t deadline) noexcept {
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    const M::Status status = issueAction(MotionKind::RELEASE, 0, now, deadline);
    if (status)
        accept(MotionKind::RELEASE, session_.clearAction().operation, now);
    return status;
}
namespace {
// startAction's reads, in order: the identity, CONTROL for a release (row 2's 2500), then the full state the
// action judges, last so that it is the freshest and follows every earlier write.
enum PreparationRead : uint8_t { READ_IDENTITY, READ_CONTROL, READ_STATE };
uint8_t preparationReads(MotionKind kind) { return kind == MotionKind::RELEASE ? 3 : 2; }
PreparationRead preparationRead(MotionKind kind, uint8_t step) {
    return step == 0 ? READ_IDENTITY : kind == MotionKind::RELEASE && step == 1 ? READ_CONTROL : READ_STATE;
}
} // namespace
M::Status ProbeMotion::startAction(MotionKind kind, uint8_t modeRaw, const L::ReadOptions& reads,
                                   const L::ActionOptions& options, uint64_t now, uint64_t deadline) noexcept {
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    if (kind != MotionKind::ENABLE && kind != MotionKind::MODE_SELECT && kind != MotionKind::RELEASE)
        return refused(M::ActionError::ARGUMENT, "only enable, mode select and release read their own report");
    if (actionReportNamed())
        return refused(M::ActionError::STATE, "a report is named: the request of that kind consumes it");
    if (!reads.queueTimeoutUs || !reads.responseTimeoutUs || !options.maximumAgeUs)
        return refused(M::ActionError::ARGUMENT, "read budgets and an age cap are required");
    if (session_.busy())
        return refused(M::ActionError::STATE, "the axis session is busy");
    const uint32_t first = session_.nextOperation();
    const uint8_t count = preparationReads(kind);
    if (!first || first > UINT32_MAX - count)
        return refused(M::ActionError::STATE, "operation identities exhausted");
    plannedKind_ = kind;
    plannedMode_ = modeRaw;
    plannedReads_ = reads;
    plannedOptions_ = options;
    plannedDeadlineUs_ = deadline;
    prepareStep_ = 0;
    const M::Status started = startPreparationRead(now);
    if (!started)
        return started;
    preparing_ = true;
    readPending_ = prepareWithdrawn_ = false;
    // The action runs under the operation after the reads'; nothing of it exists before the last read.
    accept(kind, first + count, now);
    return M::Ok();
}
M::Status ProbeMotion::startPreparationRead(uint64_t now) noexcept {
    const uint32_t operation = session_.beginOperation(plannedDeadlineUs_, now);
    if (!operation)
        return refused(M::ActionError::STATE, "the session cannot begin a prerequisite read");
    L::ReadOptions declared = plannedReads_;
    declared.identityWidths = L::IdentityWidths::DECLARED_U16;
    declared.countWidths = L::CountWidths::DECLARED_U8;
    declared.pdoNumber = 0;
    const uint8_t blocks =
        uint8_t((1u << unsigned(L::StateBlock::ERRORS)) | (1u << unsigned(L::StateBlock::MONITORS)) |
                (1u << unsigned(L::StateBlock::STATUS)) | (1u << unsigned(L::StateBlock::FEEDBACK)));
    switch (preparationRead(plannedKind_, prepareStep_)) {
    case READ_IDENTITY:
        return L::prepareIdentity(ownRead_.read, current_, operation, now, plannedDeadlineUs_, plannedReads_);
    case READ_CONTROL:
        return L::prepareDriverRead(ownRead_, current_, operation, now, plannedDeadlineUs_, declared,
                                    L::DriverGroup::CONTROL);
    case READ_STATE:
        break;
    }
    return L::prepareState(ownRead_.read, current_, operation, now, plannedDeadlineUs_, declared, blocks);
}
void ProbeMotion::endPreparation(M::ActionOutcome outcome, const M::Status& status, uint64_t now) noexcept {
    preparing_ = readPending_ = false;
    normal_.state = M::ActionState::FAILED;
    normal_.outcome = outcome;
    normal_.status = status ? refused(M::ActionError::STATE, "regular action preparation ended") : status;
    normal_.execution = M::ActionExecution::NOT_TRANSMITTED;
    normal_.completion = M::ActionCompletion::NOT_OBSERVED;
    normal_.uncertain = false;
    normal_.completedUs = now;
}
void ProbeMotion::servicePreparation(uint64_t now) noexcept {
    L::ReadContext& read = ownRead_.read;
    if (readPending_) {
        C::SdoCompletion terminal;
        if (!session_.settle(terminal))
            return; // The owner settles it under the read's own deadline.
        readPending_ = false;
        const M::Status consumed = L::advanceRead(read, terminal, now);
        if (!consumed)
            return endPreparation(M::ActionOutcome::INVALIDATED, consumed, now);
    }
    if (prepareWithdrawn_)
        return endPreparation(M::ActionOutcome::CANCELLED,
                              refused(M::ActionError::STATE, "regular action preparation cancelled"), now);
    if (now >= plannedDeadlineUs_)
        return endPreparation(M::ActionOutcome::DEADLINE,
                              refused(M::ActionError::TIME, "regular action preparation reached its cutoff"), now);
    if (read.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        const M::Status yielded = L::nextRead(read, now, work);
        if (!yielded)
            return endPreparation(M::ActionOutcome::INVALIDATED, yielded, now);
        if (!work.ready)
            return;
        const O::BusAdmission admitted = session_.admitPrepared(work.transfer, now);
        if (admitted == O::BusAdmission::RESULTS_FULL)
            return;
        if (admitted != O::BusAdmission::ACCEPTED)
            return endPreparation(M::ActionOutcome::INVALIDATED,
                                  refused(M::ActionError::STATE, "the owner refused a prerequisite read"), now);
        readPending_ = true;
        return;
    }
    const PreparationRead kind = preparationRead(plannedKind_, prepareStep_);
    const bool succeeded = read.state == M::ReadState::SUCCEEDED;
    if (kind == READ_IDENTITY) {
        if (!succeeded ||
            !L::getIdentity(read, current_, now, plannedOptions_.maximumAgeUs, ownIdentity_))
            return endPreparation(M::ActionOutcome::INVALIDATED,
                                  read.status ? refused(M::ActionError::STATE, "the identity read failed")
                                              : read.status,
                                  now);
    } else if (kind == READ_CONTROL) {
        // A failed or unpublished 2500 read leaves the loop absent, and the table's row 2 then refuses.
        actionLoopPresent_ =
            succeeded && L::getControl(ownRead_, current_, now, plannedOptions_.maximumAgeUs, actionLoop_).isOk();
    } else if (!succeeded) {
        return endPreparation(M::ActionOutcome::INVALIDATED,
                              read.status ? refused(M::ActionError::STATE, "the state read failed") : read.status,
                              now);
    }
    if (kind != READ_STATE) {
        ++prepareStep_;
        const M::Status started = startPreparationRead(now);
        if (!started)
            endPreparation(M::ActionOutcome::INVALIDATED, started, now);
        return;
    }
    // Every read completed: request at this time, on these reads, exactly as a named report would be.
    actionState_ = &read;
    actionIdentity_ = &ownIdentity_;
    actionOptions_ = plannedOptions_;
    preparing_ = false;
    const M::Status status = issueAction(plannedKind_, plannedMode_, now, plannedDeadlineUs_);
    if (!status) {
        actionState_ = nullptr;
        actionIdentity_ = nullptr;
        actionLoopPresent_ = false;
        return endPreparation(M::ActionOutcome::INVALIDATED, status, now);
    }
    normal_.nativeOperation = plannedKind_ == MotionKind::ENABLE ? session_.enableAction().operation
                                                                 : session_.clearAction().operation;
    if (plannedKind_ == MotionKind::ENABLE)
        markEvidence();
    lastUs_ = now;
    snapshotNormal();
}
M::Status ProbeMotion::requestAlarmClear(uint64_t now, uint64_t deadline) noexcept {
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    if (!clearState_)
        return refused(M::ActionError::STATE, "explicit clear prerequisites are missing");
    const M::Status status = session_.requestClearAlarm(*clearState_, clearOptions_, deadline, now);
    if (status) {
        clearState_ = nullptr;
        accept(MotionKind::ALARM_CLEAR, session_.clearAction().operation, now);
    }
    return status;
}
M::Status ProbeMotion::requestPositionClear(int64_t zero, uint64_t now,
                                            uint64_t deadline) noexcept {
    if (zero)
        return M::Status(M::Err::UNSUPPORTED, static_cast<int32_t>(M::ActionError::ARGUMENT),
                         "device position clear supports exactly zero");
    const M::Status valid = admission(now, deadline);
    if (!valid)
        return valid;
    if (!clearState_)
        return refused(M::ActionError::STATE, "explicit counter-clear prerequisites are missing");
    const M::Status status = session_.requestCounterZero(*clearState_, zero, axis_, reference_,
                                                         clearOptions_, deadline, now);
    if (status) {
        clearState_ = nullptr;
        accept(MotionKind::POSITION_CLEAR, session_.clearAction().operation, now);
        coordinateOperation_ = session_.clearAction().operation;
        coordinateHome_ = false;
    }
    return status;
}
bool ProbeMotion::stopRetained(L::StopKind kind) const noexcept {
    return kind <= L::StopKind::QUICK_STOP && stops_[static_cast<unsigned>(kind)].result.id != 0;
}
const MotionStopResult* ProbeMotion::stopResult(L::StopKind kind) const noexcept {
    return stopRetained(kind) ? &stops_[static_cast<unsigned>(kind)] : nullptr;
}
M::Status ProbeMotion::requestStop(L::StopKind kind, uint64_t now) noexcept {
    if (!graph() || !M::sameBinding(current_.channel.binding, session_.binding()))
        return refused(M::ActionError::TARGET, "stop needs the attached axis's exact binding");
    // A controller recovery may have advanced nodeGeneration. Verify the actual route at its
    // current epoch without demanding the unusable reservation that best effort exists to bypass.
    M::Generation node = 0;
    uint32_t sessions = 0;
    if (!owner_.nodeEpochs(current_.channel.binding, node, sessions) ||
        !owner_.matchesChannel(current_.channel, node))
        return refused(M::ActionError::TARGET,
                       "stop route differs from the configured owner channel");
    const L::ActionContext& halt = session_.stopAction(L::StopKind::HALT);
    const bool escalation = session_.stopActive() && kind == L::StopKind::QUICK_STOP &&
                            session_.stopPath() == StopPath::CONFIRMED &&
                            halt.state == M::ActionState::ACTIVE;
    const bool ownHalt = stopRetained(L::StopKind::HALT) && !terminal(stops_[0].result.state) &&
                         stops_[0].sequenced.present &&
                         stops_[0].sequenced.operation == halt.operation &&
                         stops_[0].result.nativeOperation == session_.stopRequestOperation();
    // Acceptance releases the caller's borrowed state. An owned HALT can still escalate:
    // the native validator uses its retained command/cycle evidence, with the same cutoff.
    const L::ReadContext* state =
        stopState_ ? stopState_ : (escalation && ownHalt ? &halt.read : nullptr);
    if (!timeValid(now) || !state || stopPolicy_.deadlineUs <= now)
        return refused(M::ActionError::TIME,
                       "explicit fresh stop policy and nondecreasing time required");
    if (kind > L::StopKind::QUICK_STOP || nextId_ == UINT32_MAX || stopRetained(kind))
        return refused(M::ActionError::STATE,
                       "stop kind, identity or reserved result slot unavailable");
    snapshotStops();
    if ((stopRetained(L::StopKind::HALT) || stopRetained(L::StopKind::QUICK_STOP)) && !escalation)
        return refused(M::ActionError::STATE, "only fast may supersede a running normal stop");
    const uint32_t oldSequence = session_.stopAction(kind).operation;
    const uint32_t oldBest = session_.bestEffort().operation;
    const StopPath path = session_.requestStop(kind, *state, stopPolicy_, now);
    lastUs_ = now;
    // Native refusal may still have withdrawn the ordinary operation. Its own eventual
    // terminal, not this call's boolean, establishes whether any frame could have left.
    snapshotNormal();
    if (path == StopPath::NONE)
        return refused(M::ActionError::STATE,
                       "native stop refused; inspect interrupted motion evidence");
    const unsigned slot = static_cast<unsigned>(kind);
    MotionStopResult& retained = stops_[slot];
    new (&retained) MotionStopResult();
    retained.result.kind =
        kind == L::StopKind::HALT ? MotionKind::STOP_NORMAL : MotionKind::STOP_FAST;
    retained.result.id = ++nextId_;
    retained.result.nativeOperation = session_.stopRequestOperation();
    retained.result.state = M::ActionState::ACTIVE;
    retained.path = path;
    const L::ActionContext& sequenced = session_.stopAction(kind);
    if (sequenced.operation != oldSequence) {
        stopAttempt(retained.sequenced, sequenced);
    }
    if (session_.bestEffort().operation != oldBest) {
        stopAttempt(retained.bestEffort, session_.bestEffort());
    }
    if (escalation && ownHalt)
        stops_[0].superseded = true;
    activeStop_ = static_cast<int8_t>(slot);
    stopState_ = nullptr;
    snapshotStops();
    return M::Ok();
}
const L::MoveContext* ProbeMotion::nativeMove() const noexcept {
    return normal_.kind == MotionKind::MOVE &&
                   session_.moveAction().operation == normal_.nativeOperation
               ? &session_.moveAction()
               : nullptr;
}
const L::VelocityContext* ProbeMotion::nativeVelocity() const noexcept {
    return normal_.kind == MotionKind::VELOCITY &&
                   session_.velocityAction().operation == normal_.nativeOperation
               ? &session_.velocityAction()
               : nullptr;
}
const L::HomeContext* ProbeMotion::nativeHome() const noexcept {
    return normal_.kind == MotionKind::HOME &&
                   session_.homeAction().operation == normal_.nativeOperation
               ? &session_.homeAction()
               : nullptr;
}
const L::ActionContext* ProbeMotion::nativeAction() const noexcept {
    const L::ActionContext* action =
        normal_.kind == MotionKind::ENABLE ? &session_.enableAction()
        : (normal_.kind == MotionKind::ALARM_CLEAR || normal_.kind == MotionKind::POSITION_CLEAR ||
           normal_.kind == MotionKind::MODE_SELECT || normal_.kind == MotionKind::RELEASE)
            ? &session_.clearAction()
            : nullptr;
    return action && action->operation == normal_.nativeOperation ? action : nullptr;
}
void ProbeMotion::snapshotNormal() noexcept {
    // startAction's reads run before any native context exists; servicePreparation keeps the result.
    if (!retained() || terminal(normal_.state) || preparing_)
        return;
    if (const L::MoveContext* c = nativeMove()) {
        normal_.retryClass = c->retryClass;
        normal_.state = c->state;
        normal_.outcome = c->outcome;
        normal_.status = c->status;
        normal_.execution = c->execution;
        normal_.completion = c->completion;
        normal_.uncertain = c->uncertain;
        normal_.completedUs = c->servicedUs;
        normal_.identityCompared = c->identity.outcome != L::IdentityOutcome::NONE;
        normal_.firmwareUnqualified = c->identity.firmwareUnqualified;
        targetValue(normal_.target, c->parameters.target);
        rateValue(normal_.rate, c->speed);
        for (unsigned n = 0; n < 4; ++n)
            if (c->setupEvidenceMask & (1u << n))
                abortEvidence(normal_, c->setupHistory[n]);
        if (c->triggerPresent)
            abortEvidence(normal_, c->trigger);
        if (c->clearPresent)
            abortEvidence(normal_, c->clearEvidence);
        if (c->rearmPresent)
            abortEvidence(normal_, c->rearmEvidence);
        if (c->observationEvidencePresent)
            abortEvidence(normal_, c->observationEvidence);
    } else if (const L::VelocityContext* c = nativeVelocity()) {
        normal_.retryClass = c->retryClass;
        normal_.state = c->state;
        normal_.outcome = c->outcome;
        normal_.status = c->status;
        normal_.execution = c->execution;
        normal_.completion = c->completion;
        normal_.uncertain = c->uncertain;
        normal_.completedUs = c->servicedUs;
        normal_.identityCompared = c->identity.outcome != L::IdentityOutcome::NONE;
        normal_.firmwareUnqualified = c->identity.firmwareUnqualified || c->change.identity.firmwareUnqualified;
        rateValue(normal_.rate, c->rate);
        if (c->modeEvidencePresent)
            abortEvidence(normal_, c->modeEvidence);
        for (unsigned n = 0; n < 3; ++n) {
            if (c->setupEvidenceMask & (1u << n))
                abortEvidence(normal_, c->setupHistory[n]);
            if (c->commandEvidenceMask & (1u << n))
                abortEvidence(normal_, c->commandHistory[n]);
        }
        if (c->observationEvidencePresent)
            abortEvidence(normal_, c->observationEvidence);
        if (c->change.serviceEvidencePresent)
            abortEvidence(normal_, c->change.serviceEvidence);
        if (c->stopOperation)
            abortEvidence(normal_, c->stopEvidence);
    } else if (const L::HomeContext* c = nativeHome()) {
        normal_.retryClass = c->retryClass;
        normal_.state = c->state;
        normal_.outcome = c->outcome;
        normal_.status = c->status;
        normal_.execution = c->execution;
        normal_.completion = c->completion;
        normal_.uncertain = c->uncertain;
        normal_.completedUs = c->servicedUs;
        normal_.identityCompared = c->identity.outcome != L::IdentityOutcome::NONE;
        normal_.firmwareUnqualified = c->identity.firmwareUnqualified;
        rateValue(normal_.rate, c->searchSpeed);
        rateValue(normal_.creepRate, c->creepSpeed);
        for (unsigned n = 0; n < 5; ++n)
            if (c->setupEvidenceMask & (1u << n))
                abortEvidence(normal_, c->setupHistory[n]);
        if (c->triggerEvidencePresent)
            abortEvidence(normal_, c->trigger);
        if (c->observationEvidencePresent)
            abortEvidence(normal_, c->observationEvidence);
    } else if (const L::ActionContext* c = nativeAction()) {
        normal_.retryClass = c->retryClass;
        actionResult(normal_, *c);
        // Enable, mode select and release compare the identity (prompt 57); the clears make none.
        const L::IdentityCheck* identity = normal_.kind == MotionKind::ENABLE        ? &c->enable.identity
                                           : normal_.kind == MotionKind::MODE_SELECT ? &c->modeSelect.identity
                                           : normal_.kind == MotionKind::RELEASE     ? &c->release.identity
                                                                                     : nullptr;
        if (identity) {
            normal_.identityCompared = identity->outcome != L::IdentityOutcome::NONE;
            normal_.firmwareUnqualified = identity->firmwareUnqualified;
        }
    } else {
        normal_.state = M::ActionState::FAILED;
        normal_.outcome = M::ActionOutcome::INVALIDATED;
        normal_.status = refused(M::ActionError::CORRELATION,
                                 "retained native motion context replaced externally");
        normal_.uncertain = true;
    }
}
void ProbeMotion::snapshotStops() noexcept {
    for (unsigned n = 0; n < 2; ++n) {
        MotionStopResult& held = stops_[n];
        if (!held.result.id || terminal(held.result.state))
            continue;
        const L::ActionContext& action = session_.stopAction(static_cast<L::StopKind>(n));
        if (held.sequenced.present && action.operation == held.sequenced.operation)
            stopAttempt(held.sequenced, action);
        if (held.superseded) {
            // The old urgent request still owns its slot until AxisSession settles it.
            // It belongs to this normal result even if the fast stop already finished.
            if (!held.sequenced.present || action.operation != held.sequenced.operation) {
                invalidateStop(held, lastUs_);
            } else if (terminal(held.sequenced.state)) {
                held.path = StopPath::FINISHED;
                finishStop(held, held.sequenced, action.servicedUs);
            }
            continue;
        }
        if (activeStop_ != static_cast<int8_t>(n))
            continue;
        if (session_.stopRequestOperation() != held.result.nativeOperation) {
            invalidateStop(held, lastUs_);
            activeStop_ = -1;
            continue;
        }
        held.path = session_.stopPath();
        const L::BestEffortStop& best = session_.bestEffort();
        const L::BestEffortStop& first = session_.firstBestEffort();
        // Native identities strictly increase. Older best-effort history survives in the
        // session, so never attribute it to a newer request that stayed sequenced.
        if (held.result.nativeOperation && best.operation >= held.result.nativeOperation &&
            best.state != M::ActionState::EMPTY)
            stopAttempt(held.bestEffort, best);
        if (held.result.nativeOperation && first.operation >= held.result.nativeOperation &&
            first.state != M::ActionState::EMPTY)
            stopAttempt(held.firstBestEffort, first);
        if (held.path != StopPath::FINISHED)
            continue;
        if (held.bestEffort.present && terminal(held.bestEffort.state))
            finishStop(held, held.bestEffort, best.servicedUs);
        else if (held.sequenced.present && terminal(held.sequenced.state))
            finishStop(held, held.sequenced, action.servicedUs);
        else {
            // A native accepted request may fail to compose its fallback after an epoch change.
            // Preserve the acceptance identity and report the absence of terminal evidence.
            held.result.state = M::ActionState::FAILED;
            held.result.outcome = M::ActionOutcome::INVALIDATED;
            held.result.status = refused(M::ActionError::TARGET,
                                         "stop path ended without a native component terminal");
            held.result.uncertain = true;
            held.result.completedUs = lastUs_;
        }
        activeStop_ = -1;
    }
}
void ProbeMotion::followCoordinates() noexcept {
    if (!coordinateOperation_ || !graph())
        return;
    const L::HomeContext& home = session_.homeAction();
    const L::ActionContext& clear = session_.clearAction();
    const bool matches =
        coordinateHome_ ? home.operation == coordinateOperation_ && home.coordinatesInvalidated
                        : clear.operation == coordinateOperation_ && clear.coordinatesInvalidated;
    const L::ReadTarget& original =
        coordinateHome_ ? home.reservation.target : clear.reservation.target;
    if (!matches || !sameTarget(current_, original) ||
        original.channel.binding.configurationGeneration == UINT32_MAX)
        return;
    M::Binding expected = original.channel.binding;
    ++expected.configurationGeneration;
    if (!M::sameBinding(axis_.binding, expected) || !M::sameBinding(session_.binding(), expected))
        return; // The native session still owes its owner synchronization; never move it here.
    C::SdoChannel route = original.channel;
    route.binding = expected;
    if (!owner_.matchesChannel(route, original.nodeGeneration))
        return;
    const L::ReadTarget previous = current_;
    current_.channel.binding = expected;
    if (app_.selected() && sameTarget(*app_.selected(), previous))
        app_.followSelectionConfiguration(previous, current_);
    coordinateOperation_ = 0;
    // Previously acquired state/settings still name the old epoch. Keep the operation's
    // frozen evidence, but require new ordinary prerequisites. An explicitly armed stop
    // retains its old state/time: the native stop may use its permitted stale-state fallback.
    forgetPrerequisites();
}
bool ProbeMotion::service(uint64_t now) noexcept {
    if (!timeValid(now))
        return false;
    lastUs_ = now;
    if (preparing_)
        servicePreparation(now);
    followCoordinates();
    snapshotNormal();
    snapshotStops();
    return true;
}
bool ProbeMotion::cancelNormal(uint64_t now) noexcept {
    if (!timeValid(now) || !retained() || terminal(normal_.state))
        return false;
    if (preparing_) {
        // Nothing was requested yet: a read in flight settles first, then the result ends CANCELLED.
        prepareWithdrawn_ = true;
        lastUs_ = now;
        if (!readPending_)
            endPreparation(M::ActionOutcome::CANCELLED,
                           refused(M::ActionError::STATE, "regular action preparation cancelled"), now);
        return true;
    }
    // A direct native caller may have replaced the session context since our last
    // snapshot. Cancellation is intent for this retained identity, not for whatever
    // operation currently occupies the same kind's slot.
    if (!nativeMove() && !nativeVelocity() && !nativeHome() && !nativeAction())
        return false;
    // A running velocity has no endpoint of its own: AxisSession::cancelVelocity would withdraw it
    // before its due-time stop, and nothing would then stop the motor (BenchRoute::abort refuses too).
    if (normal_.kind == MotionKind::VELOCITY)
        return false;
    const bool cancelled =
        normal_.kind == MotionKind::MOVE       ? session_.cancelMove(now)
        : normal_.kind == MotionKind::HOME     ? session_.cancelHome(now)
        : normal_.kind == MotionKind::ENABLE   ? session_.cancelEnable(now)
        : (normal_.kind == MotionKind::ALARM_CLEAR || normal_.kind == MotionKind::POSITION_CLEAR ||
           normal_.kind == MotionKind::MODE_SELECT || normal_.kind == MotionKind::RELEASE)
            ? session_.cancelClear(now)
            : false;
    if (cancelled) {
        lastUs_ = now;
        snapshotNormal();
    }
    return cancelled;
}
bool ProbeMotion::ownsDownload(O::SequenceId sequence,
                               const C::SdoTransfer& transfer) const noexcept {
    if (!graph() || !session_.ownsDownload(sequence, transfer))
        return false;
    if (retained() && transfer.operation == normal_.nativeOperation)
        return true;
    if (nativeVelocity() && session_.velocityActive() && session_.stopActive()) {
        const L::ActionContext& halt = session_.stopAction(L::StopKind::HALT);
        const L::ActionContext& fast = session_.stopAction(L::StopKind::QUICK_STOP);
        if ((halt.state == M::ActionState::ACTIVE && transfer.operation == halt.operation) ||
            (fast.state == M::ActionState::ACTIVE && transfer.operation == fast.operation))
            return true;
    }
    for (unsigned n = 0; n < 2; ++n)
        if (stops_[n].result.id && stops_[n].sequenced.present &&
            transfer.operation == stops_[n].sequenced.operation)
            return true;
    return false;
}
bool ProbeMotion::preflight(O::SequenceId sequence, const C::SdoTransfer& transfer, uint64_t now,
                            bool driveFault) noexcept {
    return timeValid(now) && ownsDownload(sequence, transfer) &&
           session_.preflight(sequence, transfer, now, driveFault);
}
bool ProbeMotion::releaseNormal() noexcept {
    if (!retained() || !terminal(normal_.state) || originalMove_.id)
        return false;
    new (&normal_) MotionResult();
    return true;
}
const MotionResult* ProbeMotion::normalResult(uint32_t id) const noexcept {
    return id && normal_.id == id ? &normal_
           : id && originalMove_.id == id ? &originalMove_ : nullptr;
}
bool ProbeMotion::releaseNormal(uint32_t id) noexcept {
    if (id && originalMove_.id == id) {
        if (!terminal(originalMove_.state))
            return false;
        new (&originalMove_) MotionResult();
        return true;
    }
    return id && normal_.id == id && releaseNormal();
}
bool ProbeMotion::releaseStop(L::StopKind kind) noexcept {
    if (!stopRetained(kind))
        return false;
    MotionStopResult& stop = stops_[static_cast<unsigned>(kind)];
    if (!terminal(stop.result.state))
        return false;
    new (&stop) MotionStopResult();
    return true;
}
} // namespace MotorControlCANExample
