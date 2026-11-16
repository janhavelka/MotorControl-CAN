/** @file ProbeSync.h
 * @brief One finite application SYNC schedule through the sole existing bus owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "CanBusOwner.h"
#include <MotorControlCAN/canopen/Sync.h>

namespace MotorControlCANExample {
/** Bounded processing-time measurements for one finite SYNC schedule.
 * Counts change only when the helper consumes a matching owner terminal. Times
 * are supplied host microseconds, never CAN wire timestamps or sample identity.
 * A new successful request clears these measurements; inspection and release do not.
 */
struct ProbeSyncTiming {
    uint32_t terminals = 0; ///< Consumed local terminals, including failures.
    uint32_t confirmed = 0; ///< Terminals with actual local transmission evidence.
    uint32_t handoffs = 0; ///< Terminals whose evidence permits a local handoff.
    uint64_t firstHandoffUs = 0; ///< First handoff; meaningful when handoffs is nonzero.
    uint64_t lastHandoffUs = 0; ///< Latest handoff; meaningful when handoffs is nonzero.
    uint64_t minHandoffGapUs = 0; ///< Minimum consecutive gap; meaningful at handoffs >= 2.
    uint64_t maxHandoffGapUs = 0; ///< Maximum consecutive gap; meaningful at handoffs >= 2.
    uint64_t maxLatenessUs = 0; ///< Largest handoff minus its fixed slot's due time.
    uint64_t maxReportDelayUs = 0; ///< Largest terminal processing time minus handoff time.
    bool invalid = false; ///< Inconsistent timestamp ordering; measurements cannot qualify timing.
};
/** Stationary optional application helper; construction and inspection are passive.
 * The application explicitly arms the owner's one-shot service first and services
 * this helper after the owner. No receive loop, clocks, allocation, automatic
 * rearm, retries, PDO configuration, drive actions or protocol acknowledgement.
 */
class ProbeSync {
  public:
    /** @param owner Sole stationary physical-interface owner; outlives this helper. */
    explicit ProbeSync(MotorControlCANOwner::CanBusOwner& owner) noexcept : owner_(owner) {}
    ProbeSync(const ProbeSync&) = delete; ///< Retained handles cannot be copied.
    ProbeSync& operator=(const ProbeSync&) = delete; ///< Stationary owner graph.
    /** @param owner Expected owner. @return Whether this is the same graph; no mutation. */
    bool usesOwner(const MotorControlCANOwner::CanBusOwner& owner) const noexcept {
        return &owner == &owner_;
    }
    /** Prepare a finite schedule with its first slot due now; admits no frame.
     * @param periodUs Nonzero application slot period/window in microseconds.
     * @param count Finite nonzero slot count, with checked total schedule span.
     * @param nowUs Supplied nondecreasing monotonic microseconds.
     * @return Local validation status; failure leaves retained context unchanged.
     * Explicit owner arm/exclusive-producer policy is required; no implicit rearm.
     */
    MotorControlCAN::Status request(uint64_t periodUs, uint32_t count, uint64_t nowUs) noexcept;
    /** Progress at most one owner result or one new admission, independent of output room.
     * @param nowUs Supplied monotonic microseconds, at least the owner's latest time
     * and the schedule's last progression time.
     * @return Local progression status, separate from retained schedule outcome.
     * Backwards time rejects before consuming a terminal or changing either owner or schedule.
     * Deadline failure cancels the exact pending owner request; neither is a motor stop.
     */
    MotorControlCAN::Status service(uint64_t nowUs) noexcept;
    /** Withdraw locally and cancel the exact owner request; never send a motor stop.
     * @param nowUs Supplied monotonic microseconds, at least the owner's latest time
     * and the schedule's last progression time.
     * @return True when a retained active schedule was locally disarmed. Backwards
     * time returns false without changing the schedule or its pending owner request.
     */
    bool cancel(uint64_t nowUs) noexcept;
    /** @return True releases a schedule after its owner terminal was consumed.
     * Actual local TX retirement remains the owner's independent interlock; no rearm or device action. */
    bool release() noexcept;
    /** @return Whether this helper retains one active or terminal finite schedule. */
    bool retained() const noexcept { return retained_; }
    /** @return Whether progression or consumption of an owner terminal remains necessary.
     * False does not prove local TX retirement after cancellation, expiry or controller failure. */
    bool active() const noexcept;
    /** @return Retained schedule/evidence; meaningful when retained(), borrowed until mutation. */
    const MotorControlCAN::CANOPEN::SyncContext& context() const noexcept { return context_; }
    /** @return Cached timing of the last successfully prepared schedule, borrowed
     * until the next service/request. No I/O, allocation or terminal consumption. */
    const ProbeSyncTiming& timing() const noexcept { return timing_; }

  private:
    MotorControlCANOwner::CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SyncContext context_;
    MotorControlCANOwner::SyncRequestId pending_;
    bool retained_ = false;
    bool completionConsumed_ = false;
    MotorControlCAN::Generation armGeneration_ = 0;
    ProbeSyncTiming timing_;
    void recordTiming(const MotorControlCANOwner::SyncTerminal& terminal) noexcept;
};
} // namespace MotorControlCANExample
