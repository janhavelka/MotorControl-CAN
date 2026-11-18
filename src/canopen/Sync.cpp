// SPDX-License-Identifier: MIT
#include <MotorControlCAN/canopen/Sync.h>

#include "SdoEvidence.h"

#include <stdint.h>

namespace MotorControlCAN {
namespace CANOPEN {
namespace {

Status validateSyncId(uint32_t configured1005) noexcept {
    // Pinned CANopenNode CO_SYNC.c uses CO_driver.h's CiA301 restricted-ID rule.
    if (configured1005 <= 0x7f || configured1005 > 0x7ff ||
        (configured1005 >= 0x101 && configured1005 <= 0x180) ||
        (configured1005 >= 0x581 && configured1005 <= 0x5ff) ||
        (configured1005 >= 0x601 && configured1005 <= 0x67f) ||
        (configured1005 >= 0x6e0 && configured1005 <= 0x6ff) || configured1005 >= 0x701)
        return Status(Err::INVALID_CONFIG, 0, "invalid standard SYNC COB-ID");
    return Ok();
}

bool active(const SyncContext& context) noexcept {
    return context.state == SyncState::ACTIVE || context.state == SyncState::WAITING_TX;
}

void expire(SyncContext& context, uint64_t nowUs) noexcept {
    if (!active(context))
        return;
    const uint64_t cutoff = context.options.originUs +
                            (uint64_t(context.nextSlot) + 1) * context.options.periodUs;
    if (nowUs >= cutoff) {
        context.state = SyncState::FAILED;
        context.outcome = SyncOutcome::MISSED_DEADLINE;
    }
}

bool validCompletion(const SyncCompletion& completion) noexcept {
    if (completion.outcome == SyncOutcome::LOCAL_TX)
        return completion.transmissionPossible && completion.transmissionConfirmed;
    if (completion.transmissionConfirmed)
        return false;
    switch (completion.outcome) {
    case SyncOutcome::REFUSED: return !completion.transmissionPossible;
    case SyncOutcome::BUS_OFF:
    case SyncOutcome::TRANSPORT_ERROR:
    case SyncOutcome::MISSED_DEADLINE:
    case SyncOutcome::CANCELLED:
    case SyncOutcome::PRODUCER_CONFLICT:
    case SyncOutcome::INVALIDATED: return true;
    default: return false;
    }
}

} // namespace

Status makeSyncRequest(uint32_t configured1005, Frame& output) noexcept {
    const Status status = validateSyncId(configured1005);
    if (!status)
        return status;
    Frame candidate;
    candidate.id = configured1005;
    output = candidate;
    return Ok();
}

Status checkSyncFrame(const Frame& frame, uint32_t configured1005) noexcept {
    Status status = validateSyncId(configured1005);
    if (!status)
        return status;
    status = validateFrame(frame);
    if (!status)
        return status;
    if (frame.extended || frame.rtr)
        return Status(Err::UNSUPPORTED, 0, "SYNC requires standard data frame");
    if (frame.id != configured1005 || frame.dlc || frame.payloadLength)
        return Status(Err::FRAME_ERROR, 0, "SYNC identifier or length mismatch");
    return Ok();
}

Status prepareSync(const SyncOptions& options, uint64_t nowUs, SyncContext& context) noexcept {
    if (overlaps(&options, sizeof options, &context, sizeof context))
        return Status(Err::ILLEGAL_VALUE, 0, "SYNC options overlap context");
    if (!validateSyncId(options.configured1005) || !options.generation ||
        options.generation <= context.options.generation || !options.periodUs || !options.count ||
        options.originUs < nowUs || nowUs < context.lastNowUs || active(context) ||
        context.awaitingCompletion)
        return Status(Err::INVALID_CONFIG, 0, "invalid SYNC schedule or state");
    // The final exclusive cutoff must fit too, not merely the last due instant.
    if (options.periodUs > (UINT64_MAX - options.originUs) / options.count)
        return Status(Err::ILLEGAL_VALUE, 0, "SYNC schedule overflows microseconds");
    SyncContext candidate;
    candidate.options = options;
    candidate.state = SyncState::ACTIVE;
    // Each SYNC is a trigger: synchronous PDOs sample or apply on it, so a repeat is a new trigger.
    candidate.retryClass = RetryClass::NOT_REPEATABLE;
    candidate.lastNowUs = nowUs;
    context = candidate;
    return Ok();
}

Status nextSync(SyncContext& context, uint64_t nowUs, SyncWork& output) noexcept {
    if (overlaps(&output, sizeof output, &context, sizeof context))
        return Status(Err::ILLEGAL_VALUE, 0, "SYNC output overlaps context");
    if (context.state == SyncState::EMPTY || nowUs < context.lastNowUs)
        return Status(Err::INVALID_CONFIG, 0, "invalid SYNC state or time");
    SyncWork candidate;
    expire(context, nowUs);
    if (context.state == SyncState::ACTIVE) {
        const uint64_t dueUs = context.options.originUs +
                               uint64_t(context.nextSlot) * context.options.periodUs;
        if (nowUs >= dueUs) {
            candidate.available = true;
            candidate.generation = context.options.generation;
            candidate.slot = context.nextSlot;
            candidate.dueUs = dueUs;
            candidate.expiresUs = dueUs + context.options.periodUs;
            makeSyncRequest(context.options.configured1005, candidate.frame);
            context.pending = candidate;
            context.awaitingCompletion = true;
            context.state = SyncState::WAITING_TX;
        }
    }
    context.lastNowUs = nowUs;
    output = candidate;
    return Ok();
}

Status advanceSync(SyncContext& context, const SyncCompletion& completion,
                   uint64_t nowUs) noexcept {
    if (overlaps(&completion, sizeof completion, &context, sizeof context))
        return Status(Err::ILLEGAL_VALUE, 0, "SYNC completion overlaps context");
    if (!context.awaitingCompletion || nowUs < context.lastNowUs ||
        completion.generation != context.pending.generation ||
        completion.slot != context.pending.slot || !validCompletion(completion))
        return Status(Err::INVALID_CONFIG, 0, "invalid SYNC terminal evidence");
    expire(context, nowUs);
    context.lastNowUs = nowUs;
    context.lastCompletion = completion;
    context.completionPresent = true;
    context.transmissionPossible = context.transmissionPossible || completion.transmissionPossible;
    context.awaitingCompletion = false;
    if (completion.outcome == SyncOutcome::LOCAL_TX) {
        ++context.transmittedCount;
        ++context.nextSlot;
    }
    if (context.state == SyncState::FAILED)
        return Ok(); // A late report adds evidence without replacing the terminal cause.
    if (completion.outcome != SyncOutcome::LOCAL_TX) {
        context.state = SyncState::FAILED;
        context.outcome = completion.outcome;
    } else if (context.nextSlot == context.options.count) {
        context.state = SyncState::SUCCEEDED;
        context.outcome = SyncOutcome::LOCAL_TX;
    } else {
        context.state = SyncState::ACTIVE;
    }
    return Ok();
}

Status advanceSync(SyncContext& context, SyncOutcome interruption, uint64_t nowUs) noexcept {
    if (context.state == SyncState::EMPTY || nowUs < context.lastNowUs ||
        (interruption != SyncOutcome::BUS_OFF && interruption != SyncOutcome::PRODUCER_CONFLICT &&
         interruption != SyncOutcome::INVALIDATED && interruption != SyncOutcome::CANCELLED))
        return Status(Err::INVALID_CONFIG, 0, "invalid SYNC state or time");
    expire(context, nowUs);
    if (active(context)) {
        context.state = SyncState::FAILED;
        context.outcome = interruption;
    }
    context.lastNowUs = nowUs;
    return Ok();
}

Status cancelSync(SyncContext& context, uint64_t nowUs) noexcept {
    return advanceSync(context, SyncOutcome::CANCELLED, nowUs);
}

} // namespace CANOPEN
} // namespace MotorControlCAN
