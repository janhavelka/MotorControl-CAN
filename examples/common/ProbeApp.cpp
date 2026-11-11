// SPDX-License-Identifier: MIT
#include "ProbeApp.h"
#include "ProbeDevice.h"

namespace MotorControlCANExample {
using MotorControlCANOwner::BusAdmission;
using MotorControlCANOwner::BusRequest;
using MotorControlCANOwner::Cancel;
using MotorControlCANOwner::Completion;
using MotorControlCANOwner::Sequence;
using MotorControlCANOwner::ServiceBudget;
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
bool ProbeApp::followSelectionConfiguration(const L::ReadTarget& previous,
                                            const L::ReadTarget& current) noexcept {
    if (!selected_ || previous.channel.binding.configurationGeneration == UINT32_MAX)
        return false;
    const auto sameRoute = [](const L::ReadTarget& a, const L::ReadTarget& b) {
        return a.nodeGeneration == b.nodeGeneration &&
               M::sameBinding(a.channel.binding, b.channel.binding) &&
               a.channel.clientToServerId == b.channel.clientToServerId &&
               a.channel.serverToClientId == b.channel.serverToClientId;
    };
    L::ReadTarget next = previous;
    ++next.channel.binding.configurationGeneration;
    if (!sameRoute(target_, previous) || !sameRoute(next, current) ||
        !owner_.matchesChannel(current.channel, current.nodeGeneration) ||
        !axis(current.channel.binding))
        return false;
    target_ = current;
    return true;
}
namespace {
bool same(SequenceId a, SequenceId b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
struct ReadPreparation {
    L::ReadKind kind;
    const L::ReadTarget& target;
    uint32_t operation;
    uint64_t now;
    uint64_t deadline;
    const L::ReadOptions& options;
    uint8_t mask;
};
M::Status prepareRead(L::DriverContext& output, const ReadPreparation& request) {
    const L::ReadKind kind = request.kind;
    const L::ReadTarget& target = request.target;
    const uint32_t operation = request.operation;
    const uint64_t now = request.now, deadline = request.deadline;
    const L::ReadOptions& options = request.options;
    switch (kind) {
    case L::ReadKind::PROBE:
        return L::prepareProbe(output.read, target, operation, now, deadline, options);
    case L::ReadKind::IDENTITY:
        return L::prepareIdentity(output.read, target, operation, now, deadline, options);
    case L::ReadKind::CONFIG:
        return L::prepareConfig(output.read, target, operation, now, deadline, options);
    case L::ReadKind::STATE:
        return L::prepareState(output.read, target, operation, now, deadline, options,
                               request.mask);
    default:
        break;
    }
    L::DriverGroup group;
    switch (kind) {
    case L::ReadKind::DRIVER:
        group = L::DriverGroup::DRIVER;
        break;
    case L::ReadKind::DRIVER_SOFT_LIMITS:
        group = L::DriverGroup::SOFT_LIMITS;
        break;
    case L::ReadKind::DRIVER_MOTION_PARAMETERS:
        group = L::DriverGroup::MOTION_PARAMETERS;
        break;
    case L::ReadKind::IO_INPUT_CONFIGURATION:
        group = L::DriverGroup::IO_INPUT_CONFIGURATION;
        break;
    case L::ReadKind::IO_OUTPUT_CONFIGURATION:
        group = L::DriverGroup::IO_OUTPUT_CONFIGURATION;
        break;
    case L::ReadKind::IO_INPUT_FILTERS:
        group = L::DriverGroup::IO_INPUT_FILTERS;
        break;
    case L::ReadKind::IO_OUTPUT_COMMAND:
        group = L::DriverGroup::IO_OUTPUT_COMMAND;
        break;
    case L::ReadKind::CONTROL:
        group = L::DriverGroup::CONTROL;
        break;
    case L::ReadKind::TUNING_CURRENT_HOLDING:
        group = L::DriverGroup::TUNING_CURRENT_HOLDING;
        break;
    case L::ReadKind::TUNING_FILTER_POSITION:
        group = L::DriverGroup::TUNING_FILTER_POSITION;
        break;
    case L::ReadKind::TUNING_LOOP_GAINS:
        group = L::DriverGroup::TUNING_LOOP_GAINS;
        break;
    case L::ReadKind::TUNING_EDS_GAINS:
        group = L::DriverGroup::TUNING_EDS_GAINS;
        break;
    case L::ReadKind::VELOCITY_TARGET:
        group = L::DriverGroup::VELOCITY_TARGET;
        break;
    case L::ReadKind::HOMING:
        group = L::DriverGroup::HOMING;
        break;
    case L::ReadKind::PERSISTENCE:
        group = L::DriverGroup::PERSISTENCE;
        break;
    default:
        return M::Status(M::Err::UNSUPPORTED, 0, "read kind has no application route");
    }
    return L::prepareDriverRead(output, target, operation, now, deadline, options, group);
}
// Inlining this 1944-byte target candidate into the owner-reservation frame breaches
// the measured 2048-byte target frame limit. Keep the two bounded frames separate.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
M::Status
ordinaryReadPolicy(const ReadPreparation& request) {
    L::DriverContext candidate;
    const M::Status prepared = prepareRead(candidate, request);
    if (!prepared)
        return prepared;
    for (unsigned i = 0; i < candidate.read.count; ++i)
        if (candidate.read.values[i].object.readEffect != L::ReadEffect::ACCEPTED_UNQUALIFIED)
            return M::Status(M::Err::UNSUPPORTED, static_cast<int32_t>(M::ReadError::READ_EFFECT),
                             "Q20: read plan contains an object outside DEC-1; "
                             "reviewed acquisition required");
    return M::Ok();
}
} // namespace
ProbeApp::ProbeApp(CanBusOwner& owner, ProbePlatform& platform) noexcept
    : owner_(owner), platform_(platform) {}
bool ProbeApp::attachDevice(ProbeDevice& device) noexcept {
    if ((device_ && device_ != &device) || !device.usesGraph(*this, owner_))
        return false;
    device_ = &device;
    return true;
}
AxisSession* ProbeApp::axis(const M::Binding& binding) noexcept {
    for (AxisSession* session : axes_)
        if (session && session->isOpen() && M::sameBinding(session->binding(), binding))
            return session;
    return nullptr;
}
AxisSession* ProbeApp::openAxis(AxisSession& session, const L::ReadTarget& target,
                                L::SettingsModel model, uint64_t now) noexcept {
    if (!session.usesOwner(owner_) || axis(target.channel.binding))
        return nullptr;
    for (AxisSession*& held : axes_) {
        if (held && held->isOpen())
            continue;
        if (!session.open(target, model, 1, now))
            return nullptr;
        held = &session;
        if (readOwnsSequence_ && driverRead_.read.state != M::ReadState::EMPTY &&
            same(session.sequence(), sequence_)) {
            session.adoptReservation();
            readOwnsSequence_ = false;
        }
        return &session;
    }
    return nullptr;
}
bool ProbeApp::select(const L::ReadTarget& target) noexcept {
    if (driverRead_.read.state != M::ReadState::EMPTY || !target.nodeGeneration ||
        !M::CANOPEN::validateSdoChannel(target.channel) ||
        !owner_.matchesChannel(target.channel, target.nodeGeneration))
        return false;
    target_ = target;
    selected_ = true;
    return true;
}
const L::ReadTarget* ProbeApp::selected() const noexcept {
    return selected_ ? &target_ : nullptr;
}
bool ProbeApp::requestIdentity(uint64_t now, uint64_t deadline,
                               const L::ReadOptions& options) noexcept {
    return request(L::ReadKind::IDENTITY, now, deadline, options).isOk();
}
bool ProbeApp::requestProbe(uint64_t now, uint64_t deadline,
                            const L::ReadOptions& options) noexcept {
    return request(L::ReadKind::PROBE, now, deadline, options).isOk();
}
bool ProbeApp::requestConfig(uint64_t now, uint64_t deadline,
                             const L::ReadOptions& options) noexcept {
    return request(L::ReadKind::CONFIG, now, deadline, options).isOk();
}
bool ProbeApp::requestState(uint64_t now, uint64_t deadline, const L::ReadOptions& options,
                            uint8_t mask) noexcept {
    return request(L::ReadKind::STATE, now, deadline, options, mask).isOk();
}
M::Status ProbeApp::requestOrdinaryRead(L::ReadKind kind, uint64_t now, uint64_t deadline,
                                        const L::ReadOptions& options, uint8_t mask) noexcept {
    return request(kind, now, deadline, options, mask, true);
}
M::Status ProbeApp::request(L::ReadKind kind, uint64_t now, uint64_t deadline,
                            const L::ReadOptions& options, uint8_t mask, bool ordinary) noexcept {
    if (!selected_)
        return M::Status(M::Err::INVALID_CONFIG, 0, "select an explicitly configured target first");
    if (driverRead_.read.state != M::ReadState::EMPTY)
        return M::Status(M::Err::INVALID_CONFIG, 0,
                         "release the retained read before another request");
    if (now < lastUs_ || now < owner_.lastTimeUs())
        return M::Status(M::Err::INVALID_CONFIG, 0, "read time precedes application or owner time");
    if (kind != L::ReadKind::STATE && mask)
        return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::ReadError::ARGUMENT),
                         "state mask is only valid for a state read");
    // An open session holds the axis's reservation, and its stop outranks any read.
    const AxisSession* session = axis(target_.channel.binding);
    if (session && session->busy())
        return M::Status(M::Err::INVALID_CONFIG, 0, "axis session is busy");
    SequenceId held;
    if (session)
        held = session->sequence();
    const Sequence* shared = session ? owner_.sequence(held) : nullptr;
    if (session && !shared)
        return M::Status(M::Err::INVALID_CONFIG, 0, "axis reservation is no longer current");
    const uint32_t last = shared && shared->operation > operation_ ? shared->operation : operation_;
    if (last == UINT32_MAX)
        return M::Status(M::Err::ILLEGAL_VALUE, 0, "read operation identifiers exhausted");
    const M::Status validated =
        L::validateRead(kind, target_, last + 1, now, deadline, options, mask);
    if (!validated)
        return validated;
    const ReadPreparation preparation = {kind, target_, last + 1, now, deadline, options, mask};
    if (ordinary) {
        const M::Status permitted = ordinaryReadPolicy(preparation);
        if (!permitted)
            return permitted;
    }
    SequenceId sequence = held;
    if (!session && !owner_.beginSequence(target_.channel.binding, 1, now, sequence))
        return M::Status(M::Err::INVALID_CONFIG, 0, "owner cannot reserve the selected binding");
    const Sequence* reservation = owner_.sequence(sequence);
    if (!reservation || reservation->nodeGeneration != target_.nodeGeneration ||
        !owner_.beginOperation(sequence, last + 1, deadline, now)) {
        if (!session)
            owner_.releaseSequence(sequence);
        return M::Status(M::Err::INVALID_CONFIG, 0,
                         "selected binding or operation is no longer available");
    }
    sequence_ = sequence;
    readOwnsSequence_ = !session;
    // Whole-plan validation already succeeded; owner reservation changes none of these inputs.
    prepareRead(driverRead_, preparation);
    readWithdrawn_ = false;
    operation_ = last + 1;
    lastUs_ = now;
    return M::Ok();
}
bool ProbeApp::cancelRead(uint64_t now) noexcept {
    if (driverRead_.read.state != M::ReadState::ACTIVE || now < lastUs_ ||
        now < owner_.lastTimeUs())
        return false;
    if (pending_.owner) {
        // Withdrawing future fields does not interrupt correlation for bytes already sent.
        if (!owner_.mayHaveTransmitted(pending_) && owner_.cancel(pending_, now) == Cancel::INVALID)
            return false;
    } else if (!L::advanceRead(driverRead_.read, M::ReadOutcome::CANCELLED, now))
        return false;
    readWithdrawn_ = true;
    lastUs_ = now;
    return true;
}
bool ProbeApp::interruptRead(uint64_t now) noexcept {
    if (driverRead_.read.state != M::ReadState::ACTIVE || now < lastUs_ ||
        now < owner_.lastTimeUs())
        return false;
    if (pending_.owner) {
        if (owner_.cancel(pending_, now) == Cancel::INVALID)
            return false;
    } else if (!L::advanceRead(driverRead_.read, M::ReadOutcome::CANCELLED, now))
        return false;
    readWithdrawn_ = true;
    lastUs_ = now;
    return true;
}
bool ProbeApp::current() const noexcept {
    const Sequence* reservation = owner_.sequence(sequence_);
    return reservation && reservation->nodeGeneration == target_.nodeGeneration &&
           M::sameBinding(reservation->binding, target_.channel.binding);
}
bool ProbeApp::service(uint64_t now) noexcept {
    ServiceBudget budget;
    budget.rx = CanBusOwner::RX_LIMIT;
    budget.tx = CanBusOwner::TX_LIMIT;
    budget.events = CanBusOwner::EVENT_LIMIT;
    budget.starts = CanBusOwner::START_LIMIT;
    budget.recovery = 1;
    return service(now, budget);
}
bool ProbeApp::service(uint64_t now, const ServiceBudget& budget) noexcept {
    if (now < lastUs_)
        return false;
    if (device_ && !device_->beforeService(now))
        return false;
    if (!owner_.service(now, budget))
        return false;
    lastUs_ = now;
    for (AxisSession* session : axes_)
        if (session)
            session->service(now);
    if (driverRead_.read.state == M::ReadState::ACTIVE && !pending_.owner) {
        if (now >= driverRead_.read.deadlineUs) {
            L::advanceRead(driverRead_.read, M::ReadOutcome::DEADLINE, now);
        } else if (!current()) {
            L::advanceRead(driverRead_.read, M::ReadOutcome::INVALIDATED, now);
        } else if (readWithdrawn_) {
            L::advanceRead(driverRead_.read, M::ReadOutcome::CANCELLED, now);
        } else if (budget.starts) {
            L::PreparedRead prepared;
            if (!L::nextRead(driverRead_.read, now, prepared))
                return false;
            if (prepared.ready) {
                BusRequest work;
                work.sequence = sequence_;
                work.transfer = prepared.transfer;
                const BusAdmission admission = owner_.admit(work, now, pending_);
                if (admission != BusAdmission::ACCEPTED && admission != BusAdmission::RESULTS_FULL)
                    L::advanceRead(driverRead_.read, M::ReadOutcome::INVALIDATED, now);
            }
        }
    }
    platform_.service();
    return true;
}
bool ProbeApp::advanceRead(uint64_t now) noexcept {
    const Completion* terminal = owner_.result(pending_);
    if (!terminal || now < lastUs_ || now < terminal->service.completedUs)
        return false;
    // Release can refuse only inside an owner call, and the read is atomic on rejection.
    if (!owner_.canRelease(pending_))
        return false;
    // Owner-local closures also retain the exact transfer/epochs/times and honest
    // no-TX flags. Consume that evidence rather than relabel a queue timeout.
    if (!L::advanceRead(driverRead_.read, terminal->service, now) || !owner_.release(pending_))
        return false;
    pending_ = RequestId();
    if (readWithdrawn_ && driverRead_.read.state == M::ReadState::ACTIVE)
        L::advanceRead(driverRead_.read, M::ReadOutcome::CANCELLED, now);
    lastUs_ = now;
    return true;
}
bool ProbeApp::getIdentity(uint64_t now, uint64_t age,
                           L::IdentityObservation& output) const noexcept {
    return now >= lastUs_ && current() &&
           L::getIdentity(driverRead_.read, target_, now, age, output).isOk();
}
bool ProbeApp::getProbe(uint64_t now, uint64_t age, L::ProbeObservation& output) const noexcept {
    return now >= lastUs_ && current() &&
           L::checkProbe(driverRead_.read, target_, now, age, output).isOk();
}
bool ProbeApp::getConfig(uint64_t now, uint64_t age, L::ConfigObservation& output) const noexcept {
    return now >= lastUs_ && current() &&
           L::getConfig(driverRead_.read, target_, now, age, output).isOk();
}
bool ProbeApp::getStateBlock(L::StateBlock block, uint64_t now, uint64_t age,
                             L::StateObservation& output) const noexcept {
    return now >= lastUs_ && current() &&
           L::getStateBlock(driverRead_.read, block, target_, now, age, output).isOk();
}
bool ProbeApp::getDriver(uint64_t now, uint64_t age, L::DriverObservation& output) const noexcept {
    return now >= lastUs_ && current() &&
           L::getDriver(driverRead_, target_, now, age, output).isOk();
}
bool ProbeApp::getIo(uint64_t now, uint64_t age, L::IoObservation& output) const noexcept {
    return now >= lastUs_ && current() && L::getIo(driverRead_, target_, now, age, output).isOk();
}
bool ProbeApp::getControl(uint64_t now, uint64_t age,
                          L::ControlObservation& output) const noexcept {
    return now >= lastUs_ && current() &&
           L::getControl(driverRead_, target_, now, age, output).isOk();
}
bool ProbeApp::getTuning(uint64_t now, uint64_t age, L::TuningObservation& output) const noexcept {
    return now >= lastUs_ && current() &&
           L::getTuning(driverRead_, target_, now, age, output).isOk();
}
bool ProbeApp::health(Health& output) const noexcept {
    return selected_ && owner_.health(target_.channel.binding, output);
}
bool ProbeApp::releaseRead() noexcept {
    if (driverRead_.read.state == M::ReadState::EMPTY ||
        driverRead_.read.state == M::ReadState::ACTIVE || pending_.owner ||
        (readOwnsSequence_ && !owner_.releaseSequence(sequence_)))
        return false;
    readOwnsSequence_ = false;
    readWithdrawn_ = false;
    sequence_ = SequenceId();
    L::resetRead(driverRead_.read);
    return true;
}
} // namespace MotorControlCANExample
