// SPDX-License-Identifier: MIT
#include "ProbeSettings.h"
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
bool sameControl(const C::SdoCompletion& a, const C::SdoCompletion& b) {
    return sameDownload(a.transfer, b.transfer) && a.outcome == b.outcome &&
           a.admittedUs == b.admittedUs && a.completedUs == b.completedUs &&
           a.nodeGeneration == b.nodeGeneration &&
           a.protocolAcknowledged == b.protocolAcknowledged &&
           a.transmissionPossible == b.transmissionPossible &&
           a.transmissionConfirmed == b.transmissionConfirmed &&
           a.effectPossible == b.effectPossible && a.effectUncertain == b.effectUncertain;
}
bool sameReservation(const L::AxisReservation& a, const L::AxisReservation& b) {
    return a.owner == b.owner && a.sequence == b.sequence && a.generation == b.generation &&
           a.acquiredUs == b.acquiredUs && a.model == b.model &&
           a.target.nodeGeneration == b.target.nodeGeneration &&
           M::sameBinding(a.target.channel.binding, b.target.channel.binding) &&
           a.target.channel.clientToServerId == b.target.channel.clientToServerId &&
           a.target.channel.serverToClientId == b.target.channel.serverToClientId;
}
} // namespace

ProbeSettings::ProbeSettings(O::CanBusOwner& owner, C::SdoClient& services, AxisSession& session,
                             L::ReadTarget& current) noexcept
    : owner_(owner), services_(services), session_(session), current_(current) {}

M::Status ProbeSettings::setPrerequisites(const L::DriverContext& prior,
                                          const L::ReadContext& state,
                                          const C::SdoCompletion* control, L::SettingsModel model,
                                          const L::ReadOptions& options,
                                          uint64_t maximumAgeUs) noexcept {
    if (retained_)
        return refused(M::ReadError::STATE,
                       "release retained settings before replacing prerequisites");
    if (!session_.isOpen() || model != session_.model())
        return refused(M::ReadError::TARGET,
                       "prerequisite model contradicts the open axis session");
    if (!maximumAgeUs || !options.queueTimeoutUs || !options.responseTimeoutUs ||
        (model != L::SettingsModel::CL86_C && model != L::SettingsModel::OL86_C))
        return refused(M::ReadError::ARGUMENT,
                       "explicit model, age limit and service budgets required");
    prior_ = &prior;
    state_ = &state;
    control_ = control;
    model_ = model;
    options_ = options;
    maximumAgeUs_ = maximumAgeUs;
    return M::Ok();
}

bool ProbeSettings::timeValid(uint64_t now) const noexcept {
    return now >= lastUs_ && now >= owner_.lastTimeUs();
}
bool ProbeSettings::contextActive() const noexcept {
    return context_.state == L::SettingsState::WRITING ||
           context_.state == L::SettingsState::READING_BACK;
}
bool ProbeSettings::active() const noexcept {
    return retained_ && (pending_ || contextActive());
}
bool ProbeSettings::configurationEligible(uint64_t now) const noexcept {
    const O::Sequence* held = owner_.sequence(session_.sequence());
    if (!held)
        return false;
    for (uint8_t slot = 0; slot < C::SdoClient::CHANNELS; ++slot) {
        const C::SdoChannel* route = services_.channel(slot);
        if (route && M::sameBinding(route->binding, held->binding) &&
            route->clientToServerId == current_.channel.clientToServerId &&
            route->serverToClientId == current_.channel.serverToClientId)
            return services_.configurationEligible(slot, held->binding, now);
    }
    return false;
}
bool ProbeSettings::reservationCurrent() const noexcept {
    if (!session_.isOpen() || !sameSequence(session_.sequence(), sequence_))
        return false;
    const O::Sequence* held = owner_.sequence(sequence_);
    const M::Binding& expected =
        ownerFollowed_ ? current_.channel.binding : live_.target.channel.binding;
    return held && held->acquiredUs == live_.acquiredUs && held->operation == context_.operation &&
           held->nodeGeneration == live_.target.nodeGeneration &&
           current_.nodeGeneration == live_.target.nodeGeneration &&
           M::sameBinding(held->binding, expected);
}

M::Status ProbeSettings::preparation(uint64_t now, uint64_t deadline,
                                     L::AxisReservation& reservation,
                                     uint32_t& operation) const noexcept {
    if (!usesOwner(owner_) || retained_ || !session_.isOpen() || session_.busy() ||
        session_.clearBlocked())
        return refused(M::ReadError::STATE,
                       "idle open session and released settings result required");
    if (!timeValid(now) || deadline <= now)
        return refused(M::ReadError::TIME,
                       "settings need monotonic time and a future fixed deadline");
    if (!prior_ || !state_)
        return refused(M::ReadError::STATE,
                       "supply actual fresh reserved settings and state reads first");
    const O::SequenceId id = session_.sequence();
    const O::Sequence* held = owner_.sequence(id);
    if (!held || !M::sameBinding(session_.binding(), current_.channel.binding) ||
        !M::sameBinding(held->binding, current_.channel.binding) ||
        held->nodeGeneration != current_.nodeGeneration ||
        !owner_.matchesChannel(current_.channel, current_.nodeGeneration))
        return refused(M::ReadError::TARGET,
                       "settings target or actual session reservation is stale");
    const uint32_t next = session_.nextOperation();
    if (!next)
        return refused(M::ReadError::STATE, "settings operation identity is unavailable");
    L::AxisReservation candidate;
    candidate.target = current_;
    candidate.model = model_;
    candidate.owner = owner_.identity();
    candidate.sequence = uint32_t(id.slot) + 1;
    candidate.generation = id.generation;
    candidate.acquiredUs = held->acquiredUs;
    reservation = candidate;
    operation = next;
    return M::Ok();
}

M::Status ProbeSettings::accept(uint64_t now) noexcept {
    // An experiment grant does not establish the ordinary node premises needed to migrate
    // the configuration. Refuse before an operation or download can be admitted.
    if (context_.changed && !configurationEligible(now)) {
        new (static_cast<void*>(&context_)) L::DriverSettingsContext();
        return M::Status(M::Err::UNSUPPORTED, static_cast<int32_t>(M::ReadError::TARGET),
                         "settings require ordinary node evidence for configuration migration");
    }
    const O::Sequence* held = owner_.sequence(session_.sequence());
    if (!held) {
        new (static_cast<void*>(&context_)) L::DriverSettingsContext();
        return refused(M::ReadError::TARGET, "settings reservation is no longer current");
    }
    const uint8_t producer = held->producer;
    if (session_.beginOperation(context_.deadlineUs, now) != context_.operation) {
        new (static_cast<void*>(&context_)) L::DriverSettingsContext();
        return refused(M::ReadError::STATE, "session cannot begin the prepared settings operation");
    }
    live_ = context_.reservation;
    sequence_ = session_.sequence();
    producer_ = producer;
    retained_ = true;
    pending_ = withdrawn_ = ownerFollowed_ = sessionFollowed_ = completionPresent_ = false;
    bindingStatus_ = M::Ok();
    lastUs_ = now;
    // Admitted work owns every candidate byte; prerequisite borrowing ends here.
    prior_ = nullptr;
    state_ = nullptr;
    control_ = nullptr;
    return M::Ok();
}

M::Status ProbeSettings::requestDriver(const L::DriverSettings& settings, uint64_t now,
                                       uint64_t deadline) noexcept {
    L::AxisReservation reservation;
    uint32_t operation = 0;
    M::Status status = preparation(now, deadline, reservation, operation);
    if (!status)
        return status;
    const C::SdoCompletion* actual = session_.latestControl();
    if (!control_ || !actual || !sameControl(*control_, *actual))
        return refused(M::ReadError::STATE,
                       "driver settings require the session's actual latest 000F acknowledgement");
    status = L::prepareDriverSettings(context_, reservation, current_, *prior_, *state_, *control_,
                                      settings, operation, now, deadline, options_, maximumAgeUs_);
    return status ? accept(now) : status;
}
M::Status ProbeSettings::requestSoftLimits(const L::SoftLimitSettings& settings, uint64_t now,
                                           uint64_t deadline) noexcept {
    L::AxisReservation reservation;
    uint32_t operation = 0;
    M::Status status = preparation(now, deadline, reservation, operation);
    if (!status)
        return status;
    status =
        L::prepareSoftLimitSettings(context_, reservation, current_, *prior_, *state_, settings,
                                    operation, now, deadline, options_, maximumAgeUs_);
    return status ? accept(now) : status;
}
M::Status ProbeSettings::requestIo(const L::IoSettings& settings, uint64_t now,
                                   uint64_t deadline) noexcept {
    L::AxisReservation reservation;
    uint32_t operation = 0;
    M::Status status = preparation(now, deadline, reservation, operation);
    if (!status)
        return status;
    status = L::prepareIoSettings(context_, reservation, current_, *prior_, *state_, settings,
                                  operation, now, deadline, options_, maximumAgeUs_);
    return status ? accept(now) : status;
}
M::Status ProbeSettings::requestControl(const L::ControlSettings& settings, uint64_t now,
                                        uint64_t deadline) noexcept {
    L::AxisReservation reservation;
    uint32_t operation = 0;
    M::Status status = preparation(now, deadline, reservation, operation);
    if (!status)
        return status;
    status = L::prepareControlSettings(context_, reservation, current_, *prior_, *state_, settings,
                                       operation, now, deadline, options_, maximumAgeUs_);
    return status ? accept(now) : status;
}
M::Status ProbeSettings::requestRestore(const ConfigurationRestoreContext& prepared, uint64_t now,
                                        uint64_t deadline) noexcept {
    L::AxisReservation reservation;
    uint32_t operation = 0;
    M::Status status = preparation(now, deadline, reservation, operation);
    if (!status)
        return status;
    const C::SdoCompletion* actual = session_.latestControl();
    if (!control_ || !actual || !sameControl(*control_, *actual))
        return refused(M::ReadError::STATE, "configuration re-application requires the session's "
                                            "actual latest 000F acknowledgement");
    const L::DriverSettingsContext& settings = prepared.settings;
    if (!sameReservation(settings.reservation, reservation) || settings.operation != operation ||
        settings.startedUs != now || settings.servicedUs != now ||
        settings.deadlineUs != deadline || settings.effectsPossible || settings.transmissionNoted ||
        settings.confirmed || settings.evidencePresent ||
        (settings.state != L::SettingsState::WRITING &&
         settings.state != L::SettingsState::SUCCEEDED) ||
        settings.readback.read.kind != L::ReadKind::DRIVER_MOTION_PARAMETERS)
        return refused(M::ReadError::CORRELATION, "re-application needs the pristine helper output "
                                                  "for this exact reservation, operation and time");
    L::PreparedRead next;
    status = L::nextDriver(settings, reservation, current_.channel.binding, now, next);
    if (!status)
        return status;
    context_ = settings;
    return accept(now);
}

bool ProbeSettings::preflight(O::SequenceId sequence, const C::SdoTransfer& transfer,
                              uint64_t now) noexcept {
    if (!retained_ || !pending_ || withdrawn_ || !contextActive() || !timeValid(now) ||
        !sameSequence(sequence, sequence_) || !sameDownload(expected_, transfer) ||
        !session_.ownsDownload(sequence, transfer) || !reservationCurrent())
        return false;
    if (!configurationEligible(now)) {
        bindingStatus_ = refused(M::ReadError::TARGET,
                                 "configuration migration premises changed before settings TX");
        return false;
    }
    const M::Status status =
        L::noteDriverTransmission(context_, live_, current_.channel.binding, now);
    if (!status)
        bindingStatus_ = status;
    return status.isOk();
}

M::Status ProbeSettings::followOwner(uint64_t now) noexcept {
    if (!context_.effectsPossible || ownerFollowed_)
        return M::Ok();
    if (!reservationCurrent() ||
        !owner_.advanceConfiguration(sequence_, current_.channel.binding, now)) {
        bindingStatus_ = refused(M::ReadError::TARGET,
                                 "owner cannot follow the withdrawn settings configuration");
        return bindingStatus_;
    }
    ownerFollowed_ = true;
    bindingStatus_ = M::Ok();
    return M::Ok();
}

M::Status ProbeSettings::service(uint64_t now) noexcept {
    if (!timeValid(now))
        return refused(M::ReadError::TIME,
                       "settings service time precedes application or owner time");
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
        const M::Status status =
            L::advanceDriver(context_, live_, current_.channel.binding, terminal, now);
        if (!status) {
            bindingStatus_ = status;
            if (contextActive())
                L::advanceDriver(context_, M::ReadOutcome::INVALIDATED, now);
            return status;
        }
    }
    if (!contextActive())
        return M::Ok();
    if (now >= context_.deadlineUs)
        return L::advanceDriver(context_, M::ReadOutcome::DEADLINE, now);
    if (withdrawn_)
        return L::advanceDriver(context_, M::ReadOutcome::CANCELLED, now);
    if (!reservationCurrent())
        return L::advanceDriver(context_, M::ReadOutcome::INVALIDATED, now);
    if (context_.state == L::SettingsState::READING_BACK) {
        const M::Status followed = followOwner(now);
        if (!followed) {
            L::advanceDriver(context_, M::ReadOutcome::INVALIDATED, now);
            return followed;
        }
    }
    L::PreparedRead next;
    const M::Status status = L::nextDriver(context_, live_, current_.channel.binding, now, next);
    if (!status) {
        L::advanceDriver(context_, M::ReadOutcome::INVALIDATED, now);
        return status;
    }
    if (!next.ready)
        return M::Ok();
    if (next.transfer.direction == C::SdoDirection::DOWNLOAD && !configurationEligible(now)) {
        bindingStatus_ = refused(M::ReadError::TARGET,
                                 "configuration migration premises changed before admission");
        L::advanceDriver(context_, M::ReadOutcome::INVALIDATED, now);
        return bindingStatus_;
    }
    const O::BusAdmission admitted = session_.admitPrepared(next.transfer, now);
    if (admitted == O::BusAdmission::ACCEPTED) {
        expected_ = next.transfer;
        pending_ = true;
        return M::Ok();
    }
    if (admitted == O::BusAdmission::RESULTS_FULL)
        return M::Ok();
    L::advanceDriver(context_, M::ReadOutcome::INVALIDATED, now);
    return refused(M::ReadError::STATE, "owner refused the next typed settings field");
}

bool ProbeSettings::cancel(uint64_t now) noexcept {
    if (!active() || !timeValid(now))
        return false;
    withdrawn_ = true;
    lastUs_ = now;
    if (!pending_)
        L::advanceDriver(context_, M::ReadOutcome::CANCELLED, now);
    return true;
}

M::Status ProbeSettings::finishBinding(uint64_t now) noexcept {
    if (!timeValid(now))
        return refused(M::ReadError::TIME,
                       "binding follow time precedes application or owner time");
    if (!retained_ || active())
        return refused(M::ReadError::STATE,
                       "settled settings result required before binding follow");
    if (!context_.effectsPossible || sessionFollowed_)
        return M::Ok();
    const M::Status followed = followOwner(now);
    if (!followed)
        return followed;
    if (session_.busy() || !reservationCurrent() || !session_.close()) {
        bindingStatus_ =
            refused(M::ReadError::STATE, "settings session cannot close for configuration follow");
        return bindingStatus_;
    }
    if (!session_.open(current_, live_.model, producer_, now)) {
        bindingStatus_ =
            refused(M::ReadError::TARGET, "settings session cannot reopen on the current target");
        return bindingStatus_;
    }
    sessionFollowed_ = true;
    lastUs_ = now;
    bindingStatus_ = M::Ok();
    return M::Ok();
}

bool ProbeSettings::release() noexcept {
    if (!retained_ || active())
        return false;
    new (static_cast<void*>(&context_)) L::DriverSettingsContext();
    live_ = L::AxisReservation();
    sequence_ = O::SequenceId();
    expected_ = C::SdoTransfer();
    completion_ = C::SdoCompletion();
    completionPresent_ = retained_ = pending_ = withdrawn_ = ownerFollowed_ = sessionFollowed_ =
        false;
    prior_ = nullptr;
    state_ = nullptr;
    control_ = nullptr;
    return true;
}
} // namespace MotorControlCANExample
