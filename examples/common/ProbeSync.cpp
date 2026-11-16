// SPDX-License-Identifier: MIT
#include "ProbeSync.h"

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace O = MotorControlCANOwner;

M::Status ProbeSync::request(uint64_t periodUs, uint32_t count, uint64_t nowUs) noexcept {
    if (retained_ || nowUs < owner_.lastTimeUs() || !owner_.syncArmed() || owner_.syncConflict() ||
        context_.options.generation == UINT32_MAX)
        return M::Status(M::Err::INVALID_CONFIG, 0, "explicit SYNC arm and free result required");
    C::SyncOptions options;
    options.generation = context_.options.generation + 1;
    options.periodUs = periodUs;
    options.originUs = nowUs;
    options.count = count;
    const M::Status status = C::prepareSync(options, nowUs, context_);
    if (!status)
        return status;
    retained_ = true;
    armGeneration_ = owner_.syncArmGeneration();
    return M::Ok();
}

bool ProbeSync::active() const noexcept {
    return retained_ && (context_.state == C::SyncState::ACTIVE ||
                         context_.state == C::SyncState::WAITING_TX || pending_.owner);
}

M::Status ProbeSync::service(uint64_t nowUs) noexcept {
    if (!retained_)
        return M::Ok();
    if (nowUs < owner_.lastTimeUs() || nowUs < context_.lastNowUs)
        return M::Status(M::Err::INVALID_CONFIG, 0, "SYNC service time precedes owner or schedule");
    if (pending_.owner) {
        if (completionConsumed_) {
            if (owner_.releaseSync(pending_)) {
                pending_ = O::SyncRequestId();
                completionConsumed_ = false;
            }
            return M::Ok();
        }
        if (const O::SyncTerminal* terminal = owner_.syncResult(pending_)) {
            const M::Status status = C::advanceSync(context_, terminal->service, nowUs);
            if (!status)
                return status;
            completionConsumed_ = true;
            if (owner_.releaseSync(pending_)) {
                pending_ = O::SyncRequestId();
                completionConsumed_ = false;
            }
            return M::Ok();
        }
    }
    // Consume an already final owner report before a later lifecycle change can
    // replace success with cancellation. This path never admits the next slot.
    if ((context_.state == C::SyncState::ACTIVE || context_.state == C::SyncState::WAITING_TX) &&
        (!owner_.syncArmed() || owner_.syncArmGeneration() != armGeneration_)) {
        auto reason = owner_.syncArmed() ? C::SyncOutcome::INVALIDATED
                                         : owner_.syncDisarmOutcome();
        // A different direct owner request may have failed between our slots.
        // Its transport result is not evidence about a frame of this schedule.
        if (reason != C::SyncOutcome::BUS_OFF && reason != C::SyncOutcome::PRODUCER_CONFLICT &&
            reason != C::SyncOutcome::CANCELLED && reason != C::SyncOutcome::INVALIDATED)
            reason = C::SyncOutcome::INVALIDATED;
        const M::Status status = C::advanceSync(context_, reason, nowUs);
        if (!status)
            return status;
    }
    if (pending_.owner) {
        C::SyncWork unused;
        const M::Status status = C::nextSync(context_, nowUs, unused);
        if (!status)
            return status;
        if (context_.state == C::SyncState::FAILED)
            owner_.cancelSync(pending_, nowUs);
        return M::Ok();
    }
    C::SyncWork work;
    const M::Status status = C::nextSync(context_, nowUs, work);
    if (!status || !work.available)
        return status;
    if (owner_.admitSync(work, nowUs, pending_) != O::BusAdmission::ACCEPTED) {
        C::SyncCompletion refusal;
        refusal.generation = work.generation;
        refusal.slot = work.slot;
        refusal.outcome = owner_.syncConflict() ? C::SyncOutcome::PRODUCER_CONFLICT
                                               : C::SyncOutcome::REFUSED;
        return C::advanceSync(context_, refusal, nowUs);
    }
    return M::Ok();
}

bool ProbeSync::cancel(uint64_t nowUs) noexcept {
    if (!active() || nowUs < owner_.lastTimeUs() || nowUs < context_.lastNowUs)
        return false;
    if (pending_.owner && owner_.syncResult(pending_) && !service(nowUs))
        return false;
    if ((context_.state != C::SyncState::ACTIVE && context_.state != C::SyncState::WAITING_TX) ||
        !C::cancelSync(context_, nowUs))
        return false;
    if (pending_.owner)
        owner_.cancelSync(pending_, nowUs);
    return true;
}

bool ProbeSync::release() noexcept {
    if (!retained_ || active() || context_.awaitingCompletion)
        return false;
    retained_ = false;
    return true;
}
} // namespace MotorControlCANExample
