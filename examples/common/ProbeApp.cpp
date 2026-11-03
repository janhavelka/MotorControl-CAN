// SPDX-License-Identifier: MIT
#include "ProbeApp.h"

namespace MotorControlCANExample {
using MotorControlCANOwner::BusRequest;
using MotorControlCANOwner::BusAdmission;
using MotorControlCANOwner::Cancel;
using MotorControlCANOwner::Completion;
using MotorControlCANOwner::ServiceBudget;
using MotorControlCANOwner::Sequence;
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
bool same(SequenceId a, SequenceId b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
} // namespace
ProbeApp::ProbeApp(CanBusOwner& owner, ProbePlatform& platform) noexcept
    : owner_(owner), platform_(platform) {}
AxisSession* ProbeApp::axis(const M::Binding& binding) noexcept {
    for (AxisSession* session : axes_)
        if (session && session->isOpen() && M::sameBinding(session->binding(), binding))
            return session;
    return nullptr;
}
AxisSession* ProbeApp::openAxis(AxisSession& session, const L::ReadTarget& target,
                                L::SettingsModel model, uint64_t now) noexcept {
    if (axis(target.channel.binding))
        return nullptr;
    for (AxisSession*& held : axes_) {
        if (held && held->isOpen())
            continue;
        if (!session.open(target, model, 1, now))
            return nullptr;
        held = &session;
        if (readOwnsSequence_ && read_.state != M::ReadState::EMPTY &&
            same(session.sequence(), sequence_)) {
            session.adoptReservation();
            readOwnsSequence_ = false;
        }
        return &session;
    }
    return nullptr;
}
bool ProbeApp::select(const L::ReadTarget& target) noexcept {
    Health known;
    if (read_.state != M::ReadState::EMPTY || !target.nodeGeneration ||
        !M::CANOPEN::validateSdoChannel(target.channel) ||
        !owner_.health(target.channel.binding, known))
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
    return request(L::ReadKind::IDENTITY, now, deadline, options);
}
bool ProbeApp::requestConfig(uint64_t now, uint64_t deadline,
                             const L::ReadOptions& options) noexcept {
    return request(L::ReadKind::CONFIG, now, deadline, options);
}
bool ProbeApp::requestState(uint64_t now, uint64_t deadline, const L::ReadOptions& options,
                            uint8_t mask) noexcept {
    return request(L::ReadKind::STATE, now, deadline, options, mask);
}
bool ProbeApp::request(L::ReadKind kind, uint64_t now, uint64_t deadline,
                       const L::ReadOptions& options, uint8_t mask) noexcept {
    if (!selected_ || read_.state != M::ReadState::EMPTY || now < lastUs_)
        return false;
    // An open session holds the axis's reservation, and its stop outranks any read.
    const AxisSession* session = axis(target_.channel.binding);
    if (session && session->busy())
        return false;
    SequenceId held;
    if (session)
        held = session->sequence();
    const Sequence* shared = session ? owner_.sequence(held) : nullptr;
    if (session && !shared)
        return false;
    const uint32_t last = shared && shared->operation > operation_ ? shared->operation : operation_;
    if (last == UINT32_MAX)
        return false;
    if (!L::validateRead(kind, target_, last + 1, now, deadline, options, mask))
        return false;
    SequenceId sequence = held;
    if (!session && !owner_.beginSequence(target_.channel.binding, 1, now, sequence))
        return false;
    const Sequence* reservation = owner_.sequence(sequence);
    if (!reservation || reservation->nodeGeneration != target_.nodeGeneration ||
        !owner_.beginOperation(sequence, last + 1, deadline, now)) {
        if (!session)
            owner_.releaseSequence(sequence);
        return false;
    }
    sequence_ = sequence;
    readOwnsSequence_ = !session;
    // Same values as the check above; read_ was EMPTY, and only the owner has moved.
    if (kind == L::ReadKind::IDENTITY)
        L::prepareIdentity(read_, target_, last + 1, now, deadline, options);
    else if (kind == L::ReadKind::CONFIG)
        L::prepareConfig(read_, target_, last + 1, now, deadline, options);
    else
        L::prepareState(read_, target_, last + 1, now, deadline, options, mask);
    operation_ = last + 1;
    lastUs_ = now;
    return true;
}
bool ProbeApp::cancelRead(uint64_t now) noexcept {
    if (read_.state != M::ReadState::ACTIVE || now < lastUs_ || now < owner_.lastTimeUs())
        return false;
    if (pending_.owner) {
        if (owner_.cancel(pending_, now) == Cancel::INVALID)
            return false;
    } else if (!L::advanceRead(read_, M::ReadOutcome::CANCELLED, now))
        return false;
    lastUs_ = now;
    return true;
}
bool ProbeApp::current() const noexcept {
    const Sequence* reservation = owner_.sequence(sequence_);
    return reservation && reservation->nodeGeneration == target_.nodeGeneration &&
           M::sameBinding(reservation->binding, target_.channel.binding);
}
bool ProbeApp::service(uint64_t now) noexcept {
    if (now < lastUs_)
        return false;
    ServiceBudget budget;
    budget.rx = CanBusOwner::RX_LIMIT;
    budget.tx = CanBusOwner::TX_LIMIT;
    budget.events = CanBusOwner::EVENT_LIMIT;
    budget.starts = CanBusOwner::START_LIMIT;
    budget.recovery = 1;
    if (!owner_.service(now, budget))
        return false;
    lastUs_ = now;
    for (AxisSession* session : axes_)
        if (session)
            session->service(now);
    if (read_.state == M::ReadState::ACTIVE && !pending_.owner) {
        if (now >= read_.deadlineUs) {
            L::advanceRead(read_, M::ReadOutcome::DEADLINE, now);
        } else if (!current()) {
            L::advanceRead(read_, M::ReadOutcome::INVALIDATED, now);
        } else {
            L::PreparedRead prepared;
            if (!L::nextRead(read_, now, prepared))
                return false;
            if (prepared.ready) {
                BusRequest work;
                work.sequence = sequence_;
                work.transfer = prepared.transfer;
                const BusAdmission admission = owner_.admit(work, now, pending_);
                if (admission != BusAdmission::ACCEPTED && admission != BusAdmission::RESULTS_FULL)
                    L::advanceRead(read_, M::ReadOutcome::INVALIDATED, now);
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
    if (!L::advanceRead(read_, terminal->service, now) || !owner_.release(pending_))
        return false;
    pending_ = RequestId();
    lastUs_ = now;
    return true;
}
bool ProbeApp::getIdentity(uint64_t now, uint64_t age,
                           L::IdentityObservation& output) const noexcept {
    return now >= lastUs_ && current() && L::getIdentity(read_, target_, now, age, output).isOk();
}
bool ProbeApp::getConfig(uint64_t now, uint64_t age, L::ConfigObservation& output) const noexcept {
    return now >= lastUs_ && current() && L::getConfig(read_, target_, now, age, output).isOk();
}
bool ProbeApp::getStateBlock(L::StateBlock block, uint64_t now, uint64_t age,
                             L::StateObservation& output) const noexcept {
    return now >= lastUs_ && current() &&
           L::getStateBlock(read_, block, target_, now, age, output).isOk();
}
bool ProbeApp::health(Health& output) const noexcept {
    return selected_ && owner_.health(target_.channel.binding, output);
}
bool ProbeApp::releaseRead() noexcept {
    if (read_.state == M::ReadState::EMPTY || read_.state == M::ReadState::ACTIVE ||
        pending_.owner || (readOwnsSequence_ && !owner_.releaseSequence(sequence_)))
        return false;
    readOwnsSequence_ = false;
    sequence_ = SequenceId();
    L::resetRead(read_);
    return true;
}
} // namespace MotorControlCANExample
