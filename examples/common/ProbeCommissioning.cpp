// SPDX-License-Identifier: MIT
#include "ProbeCommissioning.h"
#include <new>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace O = MotorControlCANOwner;
namespace {
M::Status refused(M::ReadError reason, const char* message) {
    return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}
bool sameSequence(O::SequenceId a, O::SequenceId b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
bool sameReservation(const L::AxisReservation& a, const L::AxisReservation& b) {
    return a.owner == b.owner && a.sequence == b.sequence && a.generation == b.generation &&
           a.acquiredUs == b.acquiredUs && a.model == b.model &&
           a.target.nodeGeneration == b.target.nodeGeneration &&
           M::sameBinding(a.target.channel.binding, b.target.channel.binding) &&
           a.target.channel.clientToServerId == b.target.channel.clientToServerId &&
           a.target.channel.serverToClientId == b.target.channel.serverToClientId;
}
bool pristine(const L::CommunicationContext& c) {
    return c.state == L::CommunicationState::WRITING && c.outcome == M::ReadOutcome::NONE &&
           c.writeOutcome == M::ReadOutcome::NONE && !c.transmissionNoted && !c.effectsPossible &&
           !c.prerequisitesWithdrawn && !c.writeAcknowledged && !c.uncertain &&
           !c.writeEvidencePresent && !c.confirmationPresent && !c.readbackKnown &&
           !c.readbackMatched && c.stepStartedUs == c.startedUs;
}
bool pristine(const L::PersistenceContext& c) {
    return c.state == L::PersistenceState::WRITING && c.outcome == M::ReadOutcome::NONE &&
           c.execution == M::ActionExecution::NOT_TRANSMITTED && !c.transmissionNoted &&
           !c.effectsPossible && !c.prerequisitesWithdrawn && !c.uncertain && !c.attempts &&
           !c.writeEvidencePresent && !c.verificationKnown;
}
bool pristine(const L::Tpdo1ConfigurationContext& c) {
    return c.state == L::PdoConfigurationState::WRITING && c.outcome == M::ReadOutcome::NONE &&
           !c.cursor && !c.acknowledgedMask && !c.confirmedStep && !c.firstEffectStep &&
           !c.failedStep && !c.readbackCount && !c.transmissionNoted && !c.effectsPossible &&
           !c.uncertain && !c.mapConfirmed && !c.evidencePresent && !c.readbackStartedUs &&
           !c.readbackCompletedUs && c.stepStartedUs == c.startedUs;
}
bool ownTpdo1(const C::PdoRecord& record, const M::Binding& target) {
    M::Binding binding = record.map.route.binding;
    if (binding.configurationGeneration > target.configurationGeneration)
        return false;
    binding.configurationGeneration = target.configurationGeneration;
    return M::sameBinding(binding, target) &&
           record.map.route.layout.mapping.direction == C::PdoDirection::TPDO &&
           record.map.route.layout.mapping.number == 1;
}
} // namespace

ProbeCommissioning::ProbeCommissioning(O::CanBusOwner& owner, C::SdoClient& services,
                                       AxisSession& session, L::ReadTarget& current) noexcept
    : owner_(owner), services_(services), session_(session), current_(current) {}

M::Status ProbeCommissioning::armCommunication(const L::CommunicationContext& prepared) noexcept {
    if (kind_ != CommissioningKind::NONE || !pristine(prepared))
        return refused(M::ReadError::STATE,
                       "release the held context and supply pristine communication preparation");
    communication_ = prepared;
    kind_ = CommissioningKind::COMMUNICATION;
    return M::Ok();
}
M::Status ProbeCommissioning::armPersistence(const L::PersistenceContext& prepared) noexcept {
    if (kind_ != CommissioningKind::NONE || !pristine(prepared))
        return refused(M::ReadError::STATE,
                       "release the held context and supply pristine persistence preparation");
    persistence_ = prepared;
    kind_ = CommissioningKind::PERSISTENCE;
    return M::Ok();
}
M::Status ProbeCommissioning::armPdo(const L::Tpdo1ConfigurationContext& prepared) noexcept {
    if (kind_ != CommissioningKind::NONE || !pristine(prepared))
        return refused(M::ReadError::STATE,
                       "release the held context and supply pristine TPDO1 preparation");
    pdo_ = prepared;
    kind_ = CommissioningKind::PDO;
    return M::Ok();
}
const L::CommunicationContext* ProbeCommissioning::communication() const noexcept {
    return kind_ == CommissioningKind::COMMUNICATION ? &communication_ : nullptr;
}
const L::PersistenceContext* ProbeCommissioning::persistence() const noexcept {
    return kind_ == CommissioningKind::PERSISTENCE ? &persistence_ : nullptr;
}
const L::Tpdo1ConfigurationContext* ProbeCommissioning::pdo() const noexcept {
    return kind_ == CommissioningKind::PDO ? &pdo_ : nullptr;
}
bool ProbeCommissioning::timeValid(uint64_t now) const noexcept {
    return now >= lastUs_ && now >= owner_.lastTimeUs();
}
bool ProbeCommissioning::nativeActive() const noexcept {
    switch (kind_) {
    case CommissioningKind::COMMUNICATION:
        return communication_.state == L::CommunicationState::WRITING ||
               communication_.state == L::CommunicationState::AWAITING_CONFIRMATION ||
               communication_.state == L::CommunicationState::READING_BACK;
    case CommissioningKind::PERSISTENCE:
        return persistence_.state == L::PersistenceState::WRITING;
    case CommissioningKind::PDO:
        return pdo_.state == L::PdoConfigurationState::WRITING ||
               pdo_.state == L::PdoConfigurationState::READING_BACK;
    case CommissioningKind::NONE:
        return false;
    }
    return false;
}
bool ProbeCommissioning::active() const noexcept {
    return retained_ && (pending_ || nativeActive());
}
bool ProbeCommissioning::effectsPossible() const noexcept {
    switch (kind_) {
    case CommissioningKind::COMMUNICATION:
        return communication_.effectsPossible;
    case CommissioningKind::PERSISTENCE:
        return persistence_.effectsPossible;
    case CommissioningKind::PDO:
        return pdo_.effectsPossible;
    case CommissioningKind::NONE:
        return false;
    }
    return false;
}
uint64_t ProbeCommissioning::deadline() const noexcept {
    switch (kind_) {
    case CommissioningKind::COMMUNICATION:
        return communication_.deadlineUs;
    case CommissioningKind::PERSISTENCE:
        return persistence_.deadlineUs;
    case CommissioningKind::PDO:
        return pdo_.deadlineUs;
    case CommissioningKind::NONE:
        return 0;
    }
    return 0;
}
uint32_t ProbeCommissioning::operation() const noexcept {
    switch (kind_) {
    case CommissioningKind::COMMUNICATION:
        return communication_.operation;
    case CommissioningKind::PERSISTENCE:
        return persistence_.operation;
    case CommissioningKind::PDO:
        return pdo_.operation;
    case CommissioningKind::NONE:
        return 0;
    }
    return 0;
}
const L::AxisReservation& ProbeCommissioning::reservation() const noexcept {
    switch (kind_) {
    case CommissioningKind::COMMUNICATION:
        return communication_.reservation;
    case CommissioningKind::PERSISTENCE:
        return persistence_.reservation;
    case CommissioningKind::PDO:
        return pdo_.reservation;
    case CommissioningKind::NONE:
        return live_;
    }
    return live_;
}
int ProbeCommissioning::slot() const noexcept {
    const O::Sequence* held = owner_.sequence(session_.sequence());
    if (!held)
        return -1;
    for (uint8_t i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const C::SdoChannel* route = services_.channel(i);
        if (route && M::sameBinding(route->binding, held->binding) &&
            route->clientToServerId == current_.channel.clientToServerId &&
            route->serverToClientId == current_.channel.serverToClientId)
            return i;
    }
    return -1;
}
bool ProbeCommissioning::reservationCurrent() const noexcept {
    if (!session_.isOpen() || !sameSequence(session_.sequence(), sequence_))
        return false;
    const O::Sequence* held = owner_.sequence(sequence_);
    const M::Binding& expected =
        ownerFollowed_ ? current_.channel.binding : live_.target.channel.binding;
    return held && held->acquiredUs == live_.acquiredUs && held->operation == operation() &&
           held->nodeGeneration == live_.target.nodeGeneration &&
           current_.nodeGeneration == live_.target.nodeGeneration &&
           M::sameBinding(held->binding, expected);
}
bool ProbeCommissioning::currentNmt(L::CommunicationNmt& output) const noexcept {
    const int channel = slot();
    const C::NodeSnapshot* node = channel < 0 ? nullptr : services_.node(uint8_t(channel));
    if (!node || node->exhausted || node->nmtOrderingUncertain || node->transitionPending)
        return false;
    L::CommunicationNmt candidate;
    candidate.known = node->nmtTime.present;
    if (candidate.known) {
        if (node->nmtTime.generation != node->route.generation)
            return false;
        candidate.state = node->nmt;
        candidate.time = node->nmtTime;
    }
    output = candidate;
    return true;
}
bool ProbeCommissioning::configurationEligible(uint64_t now) const noexcept {
    const int channel = slot();
    const O::Sequence* held = owner_.sequence(session_.sequence());
    return channel >= 0 && held &&
           services_.configurationEligible(uint8_t(channel), held->binding, now);
}

M::Status ProbeCommissioning::next(uint64_t now, L::PreparedRead& output) const noexcept {
    const L::AxisReservation& live = retained_ ? live_ : reservation();
    if (kind_ == CommissioningKind::PERSISTENCE)
        return L::nextPersistence(persistence_, live, current_.channel.binding, now, output);
    L::CommunicationNmt nmt;
    if (!currentNmt(nmt))
        return refused(M::ReadError::TARGET, "current qualified owner NMT evidence is unavailable");
    if (kind_ == CommissioningKind::COMMUNICATION)
        return L::nextCommunication(communication_, live, current_.channel.binding, nmt, now,
                                    output);
    if (kind_ == CommissioningKind::PDO && nmt.known)
        return L::nextPdoConfiguration(pdo_, live, current_.channel.binding, nmt.state, nmt.time,
                                       now, output);
    return refused(M::ReadError::STATE,
                   "one prepared workflow and current Pre-operational evidence required");
}

M::Status ProbeCommissioning::request(CommissioningKind requested, uint64_t now,
                                      uint64_t cutoff) noexcept {
    if (!usesOwner(owner_) || kind_ != requested || retained_ || !session_.isOpen() ||
        session_.busy() || session_.clearBlocked())
        return refused(M::ReadError::STATE,
                       "matching armed workflow and idle open axis session required");
    if (!timeValid(now) || cutoff <= now || cutoff != deadline())
        return refused(M::ReadError::TIME,
                       "request needs the original fixed future cutoff and monotonic time");
    const uint64_t started = requested == CommissioningKind::COMMUNICATION
                                 ? communication_.startedUs
                             : requested == CommissioningKind::PERSISTENCE ? persistence_.startedUs
                                                                           : pdo_.startedUs;
    const uint64_t serviced =
        requested == CommissioningKind::COMMUNICATION ? communication_.servicedUs
        : requested == CommissioningKind::PERSISTENCE ? persistence_.servicedUs
                                                      : pdo_.servicedUs;
    if (started != now || serviced != now)
        return refused(M::ReadError::TIME, "request must use the actual external preparation time");
    const O::SequenceId id = session_.sequence();
    const O::Sequence* held = owner_.sequence(id);
    if (!held || !M::sameBinding(session_.binding(), current_.channel.binding) ||
        !M::sameBinding(held->binding, current_.channel.binding) ||
        held->nodeGeneration != current_.nodeGeneration ||
        !owner_.matchesChannel(current_.channel, current_.nodeGeneration))
        return refused(M::ReadError::TARGET,
                       "commissioning target or actual session reservation is stale");
    L::AxisReservation actual;
    actual.target = current_;
    actual.model = session_.model(); // The armed plan must agree with the existing model fact.
    actual.owner = owner_.identity();
    actual.sequence = uint32_t(id.slot) + 1;
    actual.generation = id.generation;
    actual.acquiredUs = held->acquiredUs;
    if (!sameReservation(actual, reservation()) || operation() != session_.nextOperation())
        return refused(M::ReadError::CORRELATION,
                       "prepared workflow names another owner reservation or operation");
    L::PreparedRead work;
    const M::Status checked = next(now, work);
    if (!checked)
        return checked;
    if (!work.ready || work.transfer.direction != C::SdoDirection::DOWNLOAD)
        return refused(M::ReadError::STATE,
                       "pristine workflow must yield its first explicit download");
    if (requested != CommissioningKind::PERSISTENCE && !configurationEligible(now))
        return M::Status(M::Err::UNSUPPORTED, static_cast<int32_t>(M::ReadError::TARGET),
                         "ordinary node premises for later configuration migration required");
    const uint8_t producer = held->producer;
    if (session_.beginOperation(cutoff, now) != operation())
        return refused(M::ReadError::STATE, "session cannot begin the exact prepared operation");
    live_ = actual;
    sequence_ = id;
    producer_ = producer;
    submittedBinding_ = current_.channel.binding;
    retained_ = true;
    pending_ = withdrawn_ = ownerFollowed_ = sessionFollowed_ = snapshotsWithdrawn_ = false;
    completionPresent_ = false;
    bindingStatus_ = M::Ok();
    lastUs_ = now;
    return M::Ok();
}
M::Status ProbeCommissioning::requestCommunication(uint64_t now, uint64_t cutoff) noexcept {
    return request(CommissioningKind::COMMUNICATION, now, cutoff);
}
M::Status ProbeCommissioning::requestPersistence(uint64_t now, uint64_t cutoff) noexcept {
    return request(CommissioningKind::PERSISTENCE, now, cutoff);
}
M::Status ProbeCommissioning::requestPdo(uint64_t now, uint64_t cutoff) noexcept {
    return request(CommissioningKind::PDO, now, cutoff);
}

M::Status ProbeCommissioning::withdraw(M::ReadOutcome reason, uint64_t now) noexcept {
    switch (kind_) {
    case CommissioningKind::COMMUNICATION:
        return L::advanceCommunication(communication_, reason, now);
    case CommissioningKind::PERSISTENCE:
        return L::advancePersistence(persistence_, reason, now);
    case CommissioningKind::PDO:
        return L::advancePdoConfiguration(pdo_, reason, now);
    case CommissioningKind::NONE:
        return refused(M::ReadError::STATE, "no commissioning workflow to withdraw");
    }
    return refused(M::ReadError::STATE, "unknown commissioning workflow");
}
M::Status ProbeCommissioning::withdrawSnapshots(uint64_t now) noexcept {
    if (snapshotsWithdrawn_)
        return M::Ok();
    const int channel = slot();
    const C::NodeSnapshot* node = channel < 0 ? nullptr : services_.node(uint8_t(channel));
    if (!node)
        return refused(M::ReadError::TARGET,
                       "current adapter node required to withdraw commissioning caches");
    if (kind_ != CommissioningKind::PDO &&
        !services_.invalidateProducerInterval(uint8_t(channel), node->route, now))
        return refused(M::ReadError::TARGET, "could not withdraw effective producer timing");
    const M::Binding& original = live_.target.channel.binding;
    for (uint8_t i = 0; i < C::SdoClient::PDO_ROUTES; ++i) {
        const C::PdoRecord* record = services_.pdo(i);
        if (!record)
            continue;
        const M::Endpoint& endpoint = record->map.route.binding.endpoint;
        const bool affected = kind_ == CommissioningKind::PDO
                                  ? ownTpdo1(*record, original)
                                  : endpoint.bus == original.endpoint.bus &&
                                        endpoint.address == original.endpoint.address;
        if (affected && !services_.invalidatePdo(i, true, now))
            return refused(M::ReadError::TARGET,
                           "could not withdraw affected PDO map/sample evidence");
    }
    snapshotsWithdrawn_ = true;
    return M::Ok();
}

bool ProbeCommissioning::preflight(O::SequenceId sequence, const C::SdoTransfer& transfer,
                                   uint64_t now) noexcept {
    if (!retained_ || !pending_ || withdrawn_ || !nativeActive() || !snapshotsWithdrawn_ ||
        !timeValid(now) || !sameSequence(sequence, sequence_) ||
        !sameDownload(expected_, transfer) || !session_.ownsDownload(sequence, transfer) ||
        !reservationCurrent())
        return false;
    if (kind_ != CommissioningKind::PERSISTENCE && !configurationEligible(now)) {
        bindingStatus_ =
            refused(M::ReadError::TARGET, "migration premises changed before commissioning TX");
        return false;
    }
    M::Status status;
    if (kind_ == CommissioningKind::PERSISTENCE) {
        status = L::notePersistenceTransmission(persistence_, live_, current_.channel.binding, now);
    } else {
        L::CommunicationNmt nmt;
        if (!currentNmt(nmt)) {
            bindingStatus_ = refused(M::ReadError::TARGET,
                                     "actual NMT evidence changed before commissioning TX");
            return false;
        }
        status =
            kind_ == CommissioningKind::COMMUNICATION
                ? L::noteCommunicationTransmission(communication_, live_, current_.channel.binding,
                                                   nmt, now)
            : nmt.known
                ? L::notePdoConfigurationTransmission(pdo_, live_, current_.channel.binding,
                                                      nmt.state, nmt.time, now)
                : refused(M::ReadError::TARGET, "TPDO1 write requires observed Pre-operational");
    }
    if (!status) {
        bindingStatus_ = status;
        return false;
    }
    submittedBinding_ = current_.channel.binding;
    return true;
}

M::Status ProbeCommissioning::followOwner(uint64_t now) noexcept {
    if (!effectsPossible() || ownerFollowed_)
        return M::Ok();
    const int channel = slot();
    if (channel < 0 || !owner_.localTxRetired(uint8_t(channel))) {
        bindingStatus_ = refused(M::ReadError::STATE,
                                 "previous commissioning controller handoff is not retired");
        return bindingStatus_;
    }
    if (!reservationCurrent() ||
        !owner_.advanceConfiguration(sequence_, current_.channel.binding, now)) {
        bindingStatus_ = refused(M::ReadError::TARGET,
                                 "owner cannot follow withdrawn commissioning configuration");
        return bindingStatus_;
    }
    ownerFollowed_ = true;
    bindingStatus_ = M::Ok();
    return M::Ok();
}

M::Status ProbeCommissioning::service(uint64_t now) noexcept {
    if (!timeValid(now))
        return refused(M::ReadError::TIME, "commissioning time precedes application or owner time");
    if (!retained_)
        return M::Ok();
    lastUs_ = now;
    if (pending_) {
        C::SdoCompletion terminal;
        if (!session_.settle(terminal))
            return M::Ok();
        completion_ = terminal;
        completionPresent_ = true;
        pending_ = false;
        // This frozen submission binding correlates the terminal only. A reboot/route change
        // must not erase native failure evidence; reservationCurrent still forbids further work.
        M::Status consumed;
        switch (kind_) {
        case CommissioningKind::COMMUNICATION:
            consumed =
                L::advanceCommunication(communication_, live_, submittedBinding_, terminal, now);
            break;
        case CommissioningKind::PERSISTENCE:
            consumed = L::advancePersistence(persistence_, live_, submittedBinding_, terminal, now);
            break;
        case CommissioningKind::PDO:
            consumed = L::advancePdoConfiguration(pdo_, live_, submittedBinding_, terminal, now);
            break;
        case CommissioningKind::NONE:
            consumed =
                refused(M::ReadError::STATE, "pending terminal has no commissioning context");
            break;
        }
        if (!consumed) {
            bindingStatus_ = consumed;
            if (nativeActive())
                withdraw(M::ReadOutcome::INVALIDATED, now);
        } else if (nativeActive() && withdrawn_) {
            withdraw(M::ReadOutcome::CANCELLED, now);
        }
        return consumed;
    }
    if (!nativeActive())
        return M::Ok();
    if (now >= deadline())
        return withdraw(M::ReadOutcome::DEADLINE, now);
    if (withdrawn_)
        return withdraw(M::ReadOutcome::CANCELLED, now);
    if (!reservationCurrent())
        return withdraw(M::ReadOutcome::INVALIDATED, now);
    const int channel = slot();
    if (channel < 0)
        return withdraw(M::ReadOutcome::INVALIDATED, now);
    if (!owner_.localTxRetired(uint8_t(channel)))
        return M::Ok();
    const bool communicationConfirmation =
        kind_ == CommissioningKind::COMMUNICATION &&
        communication_.state == L::CommunicationState::AWAITING_CONFIRMATION;
    const bool pdoReadback = kind_ == CommissioningKind::PDO &&
                             pdo_.state == L::PdoConfigurationState::READING_BACK &&
                             !ownerFollowed_;
    if (communicationConfirmation || pdoReadback) {
        const M::Status followed = followOwner(now);
        if (!followed) {
            withdraw(M::ReadOutcome::INVALIDATED, now);
            return followed;
        }
        if (communicationConfirmation) {
            L::CommunicationNmt nmt;
            const M::Status prepared =
                currentNmt(nmt)
                    ? L::prepareCommunicationConfirmation(communication_, live_, current_, nmt, now)
                    : refused(M::ReadError::TARGET,
                              "actual NMT evidence unavailable for communication confirmation");
            if (!prepared) {
                bindingStatus_ = prepared;
                withdraw(M::ReadOutcome::INVALIDATED, now);
            }
            return prepared;
        }
        return M::Ok(); // New-epoch read permission is supplied outside this service phase.
    }
    L::PreparedRead work;
    const M::Status yielded = next(now, work);
    if (!yielded) {
        bindingStatus_ = yielded;
        withdraw(M::ReadOutcome::INVALIDATED, now);
        return yielded;
    }
    if (!work.ready)
        return M::Ok();
    if (work.transfer.direction == C::SdoDirection::DOWNLOAD &&
        kind_ != CommissioningKind::PERSISTENCE && !configurationEligible(now)) {
        bindingStatus_ = refused(M::ReadError::TARGET,
                                 "migration premises changed before commissioning admission");
        withdraw(M::ReadOutcome::INVALIDATED, now);
        return bindingStatus_;
    }
    const O::BusAdmission admitted = session_.admitPrepared(work.transfer, now);
    if (admitted == O::BusAdmission::RESULTS_FULL)
        return M::Ok();
    if (admitted != O::BusAdmission::ACCEPTED) {
        withdraw(M::ReadOutcome::INVALIDATED, now);
        bindingStatus_ =
            refused(M::ReadError::STATE, "owner refused exact commissioning service; actual "
                                         "one-use permission and current route required");
        return bindingStatus_;
    }
    expected_ = work.transfer;
    submittedBinding_ = current_.channel.binding;
    pending_ = true;
    if (work.transfer.direction == C::SdoDirection::DOWNLOAD && !snapshotsWithdrawn_) {
        const M::Status invalidated = withdrawSnapshots(now);
        if (!invalidated) {
            withdrawn_ = true; // Exact guard refuses it; its own owner terminal still settles.
            bindingStatus_ = invalidated;
            return invalidated;
        }
    }
    return M::Ok();
}

bool ProbeCommissioning::cancel(uint64_t now) noexcept {
    if (!active() || !timeValid(now))
        return false;
    withdrawn_ = true;
    lastUs_ = now;
    if (!pending_)
        withdraw(M::ReadOutcome::CANCELLED, now);
    return true;
}
M::Status ProbeCommissioning::finishBinding(uint64_t now) noexcept {
    if (!timeValid(now))
        return refused(M::ReadError::TIME,
                       "commissioning follow time precedes application or owner time");
    if (!retained_ || active())
        return refused(M::ReadError::STATE,
                       "settled commissioning result required before binding follow");
    if (!effectsPossible() || sessionFollowed_)
        return M::Ok();
    const M::Status followed = followOwner(now);
    if (!followed)
        return followed;
    if (session_.busy() || !reservationCurrent() || !session_.close()) {
        bindingStatus_ = refused(M::ReadError::STATE,
                                 "commissioning session cannot close for configuration follow");
        return bindingStatus_;
    }
    if (!session_.open(current_, live_.model, producer_, now)) {
        bindingStatus_ =
            refused(M::ReadError::TARGET, "commissioning session cannot reopen on current target");
        return bindingStatus_;
    }
    sessionFollowed_ = true;
    lastUs_ = now;
    bindingStatus_ = M::Ok();
    return M::Ok();
}
bool ProbeCommissioning::release() noexcept {
    if (kind_ == CommissioningKind::NONE || active())
        return false;
    switch (kind_) {
    case CommissioningKind::COMMUNICATION:
        new (static_cast<void*>(&communication_)) L::CommunicationContext();
        break;
    case CommissioningKind::PERSISTENCE:
        new (static_cast<void*>(&persistence_)) L::PersistenceContext();
        break;
    case CommissioningKind::PDO:
        new (static_cast<void*>(&pdo_)) L::Tpdo1ConfigurationContext();
        break;
    case CommissioningKind::NONE:
        break;
    }
    kind_ = CommissioningKind::NONE;
    retained_ = pending_ = withdrawn_ = ownerFollowed_ = sessionFollowed_ = false;
    snapshotsWithdrawn_ = completionPresent_ = false;
    live_ = L::AxisReservation();
    sequence_ = O::SequenceId();
    expected_ = C::SdoTransfer();
    completion_ = C::SdoCompletion();
    submittedBinding_ = M::Binding();
    return true;
}
} // namespace MotorControlCANExample
