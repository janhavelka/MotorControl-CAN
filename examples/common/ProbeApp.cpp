// SPDX-License-Identifier: MIT
#include "ProbeApp.h"

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
ProbeApp::ProbeApp(CanBusOwner& owner, ProbePlatform& platform) noexcept
    : owner_(owner), platform_(platform) {}
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
    return request(true, now, deadline, options);
}
bool ProbeApp::requestConfig(uint64_t now, uint64_t deadline,
                             const L::ReadOptions& options) noexcept {
    return request(false, now, deadline, options);
}
bool ProbeApp::request(bool identity, uint64_t now, uint64_t deadline,
                       const L::ReadOptions& options) noexcept {
    if (!selected_ || read_.state != M::ReadState::EMPTY || now < lastUs_ ||
        operation_ == UINT32_MAX)
        return false;
    L::ReadContext candidate;
    const M::Status prepared =
        identity ? L::prepareIdentity(candidate, target_, operation_ + 1, now, deadline, options)
                 : L::prepareConfig(candidate, target_, operation_ + 1, now, deadline, options);
    if (!prepared)
        return false;
    SequenceId sequence;
    if (!owner_.beginSequence(target_.channel.binding, 1, now, sequence))
        return false;
    const Sequence* reservation = owner_.sequence(sequence);
    if (!reservation || reservation->nodeGeneration != target_.nodeGeneration ||
        !owner_.beginOperation(sequence, operation_ + 1, deadline, now)) {
        owner_.releaseSequence(sequence);
        return false;
    }
    sequence_ = sequence;
    read_ = candidate;
    ++operation_;
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
    L::ReadContext candidate = read_;
    // Owner-local closures also retain the exact transfer/epochs/times and honest
    // no-TX flags. Consume that evidence rather than relabel a queue timeout.
    const M::Status advanced = L::advanceRead(candidate, terminal->service, now);
    if (!advanced || !owner_.release(pending_))
        return false;
    read_ = candidate;
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
bool ProbeApp::health(Health& output) const noexcept {
    return selected_ && owner_.health(target_.channel.binding, output);
}
bool ProbeApp::releaseRead() noexcept {
    if (read_.state == M::ReadState::EMPTY || read_.state == M::ReadState::ACTIVE ||
        pending_.owner || !owner_.releaseSequence(sequence_))
        return false;
    sequence_ = SequenceId();
    read_ = L::ReadContext();
    return true;
}
} // namespace MotorControlCANExample
