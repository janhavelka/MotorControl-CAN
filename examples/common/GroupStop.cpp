// SPDX-License-Identifier: MIT
#include "GroupStop.h"
#include <new>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace L = M::LICHUAN_C;
namespace {
bool terminal(M::ActionState state) noexcept {
    return state == M::ActionState::SUCCEEDED || state == M::ActionState::FAILED;
}
M::Status invalid(const char* reason) noexcept {
    return M::Status(M::Err::INVALID_CONFIG, int32_t(M::ActionError::STATE), reason);
}
void fail(GroupStopMember& row, M::Status status, uint64_t now) noexcept {
    row.finished = true;
    row.stop.result.state = M::ActionState::FAILED;
    row.stop.result.outcome = M::ActionOutcome::INVALIDATED;
    row.stop.result.status = status;
    row.stop.result.uncertain = true;
    row.stop.result.completedUs = now;
}
} // namespace
GroupStop::GroupStop(ProbeApp& app, MotorControlCANOwner::CanBusOwner& owner, GroupStopMember* rows,
                     std::size_t capacity) noexcept
    : app_(app), owner_(owner), rows_(rows), capacity_(capacity) {}
bool GroupStop::matches(const ProbeApp& app,
                        const MotorControlCANOwner::CanBusOwner& owner) const noexcept {
    return &app == &app_ && &owner == &owner_;
}
const GroupStopMember* GroupStop::member(std::size_t index) const noexcept {
    return index < count_ ? &rows_[index] : nullptr;
}
M::Status GroupStop::request(ProbeMotion* const* members, std::size_t count, L::StopKind kind,
                             uint64_t now) noexcept {
    if (result_.id || !rows_ || !members || !count || count > capacity_ ||
        kind > L::StopKind::QUICK_STOP || nextId_ == UINT32_MAX || now < lastUs_ ||
        now < owner_.lastTimeUs())
        return invalid("group storage, identity, kind or time unavailable");
    for (std::size_t i = 0; i < count; ++i) {
        if (!members[i] || !members[i]->matches(app_, owner_) ||
            !members[i]->target().channel.binding.logicalAxisId)
            return invalid("group member has no matching application graph or logical axis");
        for (std::size_t j = 0; j < i; ++j)
            if (members[i] == members[j] || members[i]->target().channel.binding.logicalAxisId ==
                                                members[j]->target().channel.binding.logicalAxisId)
                return invalid("group members must name distinct axes");
    }
    count_ = count;
    result_.id = ++nextId_;
    result_.kind = kind;
    result_.state = M::ActionState::ACTIVE;
    result_.uncertain = true;
    lastUs_ = now;
    for (std::size_t i = 0; i < count_; ++i) {
        GroupStopMember& row = rows_[i];
        new (&row) GroupStopMember();
        row.motion = members[i];
        row.logicalAxisId = row.motion->target().channel.binding.logicalAxisId;
        row.admission = row.motion->requestStop(kind, now);
        row.accepted = bool(row.admission);
        if (row.accepted) {
            const MotionStopResult* stop = row.motion->stopResult(kind);
            if (stop)
                row.stop = *stop;
            else
                fail(row, invalid("accepted stop has no retained identity"), now);
        } else {
            row.stop.result.kind =
                kind == L::StopKind::HALT ? MotionKind::STOP_NORMAL : MotionKind::STOP_FAST;
            fail(row, row.admission, now);
        }
    }
    service(now);
    return M::Ok();
}
bool GroupStop::service(uint64_t now) noexcept {
    if (now < lastUs_ || now < owner_.lastTimeUs())
        return false;
    lastUs_ = now;
    if (result_.state != M::ActionState::ACTIVE)
        return true;
    bool finished = true, observed = true;
    for (std::size_t i = 0; i < count_; ++i) {
        GroupStopMember& row = rows_[i];
        if (!row.finished) {
            row.motion->service(now);
            const MotionStopResult* stop = row.motion->stopResult(result_.kind);
            if (!stop || stop->result.id != row.stop.result.id)
                fail(row, invalid("group-owned stop identity was released or replaced"), now);
            else {
                row.stop = *stop;
                row.finished = terminal(stop->result.state);
            }
        }
        finished = finished && row.finished;
        observed = observed && row.accepted && row.finished &&
                   row.stop.result.state == M::ActionState::SUCCEEDED &&
                   row.stop.result.completion == M::ActionCompletion::OBSERVED &&
                   !row.stop.result.uncertain;
    }
    if (finished) {
        result_.state = observed ? M::ActionState::SUCCEEDED : M::ActionState::FAILED;
        result_.uncertain = !observed;
        result_.completedUs = now;
    }
    return true;
}
bool GroupStop::release() noexcept {
    if (!result_.id || !terminal(result_.state))
        return false;
    for (std::size_t i = 0; i < count_; ++i) {
        GroupStopMember& row = rows_[i];
        const MotionStopResult* stop = row.motion->stopResult(result_.kind);
        if (row.accepted && stop && stop->result.id == row.stop.result.id &&
            !row.motion->releaseStop(result_.kind))
            return false;
    }
    for (std::size_t i = 0; i < count_; ++i)
        new (&rows_[i]) GroupStopMember();
    count_ = 0;
    new (&result_) GroupStopResult();
    return true;
}
} // namespace MotorControlCANExample
