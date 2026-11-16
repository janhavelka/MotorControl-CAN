/** @file GroupStop.h
 * @brief Application-owned fan-out of independent, retained per-axis stops.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "ProbeMotion.h"
#include <cstddef>

namespace MotorControlCANExample {
/** One reserved group result. Storage is private to GroupStop until release. */
struct GroupStopMember {
    ProbeMotion* motion = nullptr;     ///< Borrowed stationary attachment.
    uint32_t logicalAxisId = 0;        ///< Logical identity captured at admission.
    MotorControlCAN::Status admission; ///< Per-axis request call result.
    bool accepted = false;             ///< A new per-axis stop identity was admitted.
    bool finished = false;             ///< Refusal or immutable terminal snapshot retained.
    MotionStopResult stop;             ///< Owned evidence; refusal has no native stop identity.
};
/** Reserved group metadata; success means every member's own stop was observed. */
struct GroupStopResult {
    uint32_t id = 0; ///< Nonwrapping helper identity, zero when empty.
    MotorControlCAN::LICHUAN_C::StopKind kind =
        MotorControlCAN::LICHUAN_C::StopKind::HALT; ///< Requested stop kind.
    MotorControlCAN::ActionState state =
        MotorControlCAN::ActionState::EMPTY; ///< Aggregate lifecycle.
    bool uncertain = false;   ///< True until every member has qualified stopped evidence.
    uint64_t completedUs = 0; ///< Caller processing time when all members became terminal.
};
/** One retained group in caller-provided reserved storage, independent of ordinary results.
 * The app, owner, row storage and attachments outlive this object. One cooperative caller
 * owns every method. ProbeApp progresses sessions; service only snapshots their evidence.
 * Each admitted member's stop slot belongs to this group until release: no external release
 * or replacement. No NMT, enabling, fault clearing, automatic retry or deadline extension.
 * No physical standstill or safety-rated stop is inferred from controller observations.
 */
class GroupStop {
  public:
    /** Construct without I/O; rows must be distinct storage of capacity elements,
     * disjoint from this object and all request member-pointer arrays.
     * @param app Borrowed stationary application. @param owner Its sole bus owner.
     * @param rows Reserved caller-owned result array. @param capacity Number of rows. */
    GroupStop(ProbeApp& app, MotorControlCANOwner::CanBusOwner& owner, GroupStopMember* rows,
              std::size_t capacity) noexcept;
    GroupStop(const GroupStop&) = delete;
    GroupStop& operator=(const GroupStop&) = delete;
    /** Exact stationary graph comparison for application attachment.
     * @param app Expected application. @param owner Expected owner.
     * @return True exactly when both borrowed objects match. */
    bool matches(const ProbeApp& app,
                 const MotorControlCANOwner::CanBusOwner& owner) const noexcept;
    /** Validate the entire nonempty unique member list before any stop request. Then call
     * every member once, regardless of other refusals. OK means this fan-out occurred,
     * not that every request succeeded. Each member supplies its existing stop prerequisites
     * and immutable deadlines. Invalid list/time or retained group leaves all work unchanged.
     * @param members Borrowed pointer list, copied during this call only.
     * @param count Nonzero count no greater than reserved capacity.
     * @param kind HALT or QUICK_STOP, independently admitted on every member.
     * @param nowUs Nondecreasing microseconds, at least owner time.
     * @return Admission of the group fan-out, independent of each member's admission. */
    MotorControlCAN::Status request(ProbeMotion* const* members, std::size_t count,
                                    MotorControlCAN::LICHUAN_C::StopKind kind,
                                    uint64_t nowUs) noexcept;
    /** Snapshot at nondecreasing caller time; no bus service or command retry.
     * @param nowUs Monotonic microseconds, at least owner time.
     * @return False unchanged on backward time; true otherwise, including empty/terminal groups. */
    bool service(uint64_t nowUs) noexcept;
    /** Explicitly free terminal group and its still-matching per-axis stop slots.
     * Active groups cannot be released; unrelated or already released slots are untouched.
     * If any matching slot refuses release, the group's owned copies remain for retry.
     * @return True when this terminal group was released, false otherwise. */
    bool release() noexcept;
    /** @return Retained summary, valid until explicit release. */
    const GroupStopResult& result() const noexcept { return result_; }
    /** @return Number of retained per-axis rows. */
    std::size_t memberCount() const noexcept { return count_; }
    /** @param index Zero-based row index. @return Retained row, or null outside memberCount(). */
    const GroupStopMember* member(std::size_t index) const noexcept;

  private:
    ProbeApp& app_;
    MotorControlCANOwner::CanBusOwner& owner_;
    GroupStopMember* rows_;
    std::size_t capacity_, count_ = 0;
    uint32_t nextId_ = 0;
    uint64_t lastUs_ = 0;
    GroupStopResult result_;
};
} // namespace MotorControlCANExample
