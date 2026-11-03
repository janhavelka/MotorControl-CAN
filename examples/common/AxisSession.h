/** @file AxisSession.h
 * @brief One axis's application state: reservation, latest controlword and stop work.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>

namespace MotorControlCANExample {
/** Where a requested stop stands. Never a claim about the drive. */
enum class StopPath : uint8_t {
    NONE,        ///< No stop requested, or the request was refused with nothing changed.
    CONFIRMED,   ///< Sequenced stop in progress: stopAction(kind) holds its evidence.
    BEST_EFFORT, ///< The owner holds the 000B frame; only its terminal can settle it.
    HELD,        ///< This 000B has not been admitted; it waits for the owner or one recovery.
    FINISHED     ///< Terminal; contexts keep every outcome. Only a new request can stop again.
};
/** Caller limits for one stop request, fixed for its whole life. */
struct StopPolicy {
    MotorControlCAN::LICHUAN_C::ActionOptions options; ///< Nonzero service and observation budgets.
    /** Exclusive cutoff of the sequenced stop, and of a held stop that came from it; future
     * and immutable. */
    uint64_t deadlineUs = 0;
    /** Exclusive cutoff of the branch taken for missing history, stale state or an
     * unresolved route, at or after deadlineUs. Zero refuses that branch, so such a request is
     * refused. A stop proved never sent by a controller fault still waits under deadlineUs. */
    uint64_t bestEffortDeadlineUs = 0;
};
/** The application session of one axis over the sole owner. It keeps the history
 * prepareStop cannot see. Every command this axis sends goes through admitPrepared, so
 * the latest 6040 terminal here is the actual latest, and a copied terminal is never
 * authority. Writes outside the session need the reservation it holds. No clock, I/O,
 * retry or replay. One cooperative caller serializes all calls.
 *
 * A stop waits for in-flight work to settle and never cancels it. If that leaves no
 * usable route before any stop frame could leave, the request takes the 000B best-effort
 * branch, when the policy allows. A stop proved never sent by a controller fault waits
 * across one explicit recovery and goes out once as 000B. A second recovery ends it. A
 * possibly sent frame is never sent again.
 */
class AxisSession {
  public:
    /** Borrow the owner; construction performs no I/O or reservation.
     * @param[in] owner Sole CAN owner, outlives this session.
     */
    AxisSession(MotorControlCANOwner::CanBusOwner& owner) noexcept : owner_(owner) {}
    AxisSession(const AxisSession&) = delete;            ///< Live handles are not copyable.
    AxisSession& operator=(const AxisSession&) = delete; ///< Stationary application storage.
    /** Open on the axis's usable reservation, or take a new one for producer. With none
     * available, as on an unresolved route, only the best-effort branch can run.
     * @param[in] target Current route and node epoch.
     * @param[in] model Applicable model, supplied by the application from its evidence.
     * @param[in] producer Fairness identity for a new reservation.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return False unchanged when already open or the target is not current.
     */
    bool open(const MotorControlCAN::LICHUAN_C::ReadTarget& target,
              MotorControlCAN::LICHUAN_C::SettingsModel model, uint8_t producer,
              uint64_t nowUs) noexcept;
    /** Close after every result is consumed, keeping no history. Releases only a
     * reservation this session took.
     * @return False unchanged while a command or stop is live or its result is unread.
     */
    bool close() noexcept;
    /** Take over an adopted reservation, so that close releases it. */
    void adoptReservation() noexcept { ownsReservation_ = sequence_.owner != nullptr; }
    /** @return True between a successful open and close. */
    bool isOpen() const noexcept { return open_; }
    /** @return The binding this session was opened on; meaningful only while open. */
    const MotorControlCAN::Binding& binding() const noexcept { return target_.channel.binding; }
    /** @return The reservation this session uses; meaningful only while open. */
    MotorControlCANOwner::SequenceId sequence() const noexcept { return sequence_; }
    /** @return True when a stop is in progress. */
    bool stopActive() const noexcept {
        return path_ != StopPath::NONE && path_ != StopPath::FINISHED;
    }
    /** @return True with a stop in progress or a request unsettled, which excludes new
     * operations and reads: starting one would orphan that terminal's history. */
    bool busy() const noexcept {
        return stopActive() || pending_.owner || urgent_.owner || superseded_.owner;
    }
    /** Start a new operation for the next commands on this reservation.
     * @param[in] deadlineUs Immutable exclusive future cutoff.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return Operation identity to put in the commands; zero when refused, including
     * while earlier work is live or a stop is active.
     */
    uint32_t beginOperation(uint64_t deadlineUs, uint64_t nowUs) noexcept;
    /** Admit one yielded command. A download withdraws the latest controlword at once,
     * because from here it may reach the drive before any report says so.
     * @param[in] transfer Complete yielded work for the current operation.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return Owner admission. INVALID while another command is unread or a stop is active.
     */
    MotorControlCANOwner::BusAdmission admitPrepared(const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                                                     uint64_t nowUs) noexcept;
    /** Consume the terminal of the admitted command and release its slot.
     * A 6040 acknowledgement of the current operation becomes the latest controlword.
     * @param[out] output Owned copy, published only when true.
     * @return False while no terminal is available.
     */
    bool settle(MotorControlCAN::CANOPEN::SdoCompletion& output) noexcept;
    /** Request a stop on this axis.
     * Upward only: QUICK_STOP may replace a running HALT, and nothing else replaces a
     * running stop. Queued work retires with explicit results; in-flight work settles.
     * The sequenced path needs the latest controlword and fresh state; escalation uses the
     * running HALT's own acknowledgement and latest cycle. A known unsupported predecessor,
     * or fresh state reporting an idle drive that is alarmed, released, in another mode or
     * not enabled, refuses the request. Otherwise a missing condition takes the 000B
     * branch, whatever kind was asked for, when the policy allows it.
     * @param[in] kind Native selection for the sequenced path.
     * @param[in] state Application's latest complete state read. A read still in progress
     * counts as missing: keep the last complete copy for this. It must not be held inside
     * stopAction(kind), which this request overwrites: that also counts as missing.
     * @param[in] requested Copied caller limits; invalid budgets refuse the request. Its identity
     * width setting is ignored, so options shared with identity reads cannot refuse a stop.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     * @return The path taken; NONE when refused, with only a stale controlword withdrawn.
     */
    StopPath requestStop(MotorControlCAN::LICHUAN_C::StopKind kind,
                         const MotorControlCAN::LICHUAN_C::ReadContext& state,
                         const StopPolicy& policy, uint64_t nowUs) noexcept;
    /** Advance session work after an owner service turn; performs no owner service.
     * Also withdraws the latest controlword when its route, reservation or epochs change.
     * @param[in] nowUs Nondecreasing supplied microseconds.
     */
    void service(uint64_t nowUs) noexcept;
    /** @return Current stop path. */
    StopPath stopPath() const noexcept { return path_; }
    /** @param[in] kind Selection to inspect.
     * @return Retained context of that sequenced stop; EMPTY if never prepared.
     */
    const MotorControlCAN::LICHUAN_C::ActionContext& stopAction(MotorControlCAN::LICHUAN_C::StopKind kind) const noexcept {
        return stops_[static_cast<unsigned>(kind)];
    }
    /** @return Retained context of the best-effort branch; EMPTY if never taken. */
    const MotorControlCAN::LICHUAN_C::BestEffortStop& bestEffort() const noexcept { return bestEffort_; }
    /** @return Earlier best-effort attempt a controller fault ended unsent; EMPTY if none. */
    const MotorControlCAN::LICHUAN_C::BestEffortStop& firstBestEffort() const noexcept {
        return firstBestEffort_;
    }
    /** @return Latest acknowledged controlword terminal, or null once withdrawn. */
    const MotorControlCAN::CANOPEN::SdoCompletion* latestControl() const noexcept {
        return controlPresent_ ? &control_ : nullptr;
    }

  private:
    MotorControlCANOwner::CanBusOwner& owner_;
    MotorControlCAN::LICHUAN_C::ReadTarget target_;
    MotorControlCAN::LICHUAN_C::SettingsReservation reservation_;
    MotorControlCANOwner::SequenceId sequence_;
    MotorControlCANOwner::RequestId pending_, urgent_, superseded_;
    MotorControlCAN::CANOPEN::SdoCompletion control_;
    MotorControlCAN::LICHUAN_C::ActionContext stops_[2];
    MotorControlCAN::LICHUAN_C::BestEffortStop bestEffort_, firstBestEffort_;
    StopPolicy policy_;
    MotorControlCAN::LICHUAN_C::StopKind activeKind_ = MotorControlCAN::LICHUAN_C::StopKind::HALT;
    StopPath path_ = StopPath::NONE;
    uint64_t lastUs_ = 0;
    uint32_t operation_ = 0;
    uint32_t controllerEpoch_ = 0, epochLimit_ = 0;
    uint32_t serverSessions_ = 0;
    bool open_ = false, ownsReservation_ = false, controlPresent_ = false, recoveryUsed_ = false;

    bool usable() const noexcept;
    bool stateRefuses(const MotorControlCAN::LICHUAN_C::ReadContext& state, const StopPolicy& policy,
                      uint64_t nowUs) const noexcept;
    bool controllerMoved(MotorControlCAN::Generation before) const noexcept;
    void settleSuperseded(uint64_t nowUs) noexcept;
    bool prepareHeld(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    void withdrawIfStale() noexcept;
    void serviceConfirmed(uint64_t nowUs) noexcept;
    void serviceBestEffort(uint64_t nowUs) noexcept;
    void serviceHeld(uint64_t nowUs) noexcept;
    void afterConfirmedEnd(uint64_t nowUs) noexcept;
    bool startBestEffort(uint64_t nowUs) noexcept;
    void hold(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    void endHeld(MotorControlCAN::ActionOutcome reason, uint64_t nowUs) noexcept;
};
} // namespace MotorControlCANExample
