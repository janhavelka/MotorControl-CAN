/** @file ProbeSync.h
 * @brief One finite application SYNC schedule through the sole existing bus owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "CanBusOwner.h"
#include <MotorControlCAN/canopen/Sync.h>

namespace MotorControlCANExample {
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
     * @param nowUs Supplied monotonic microseconds, after the owner's turn.
     * @return Local progression status, separate from retained schedule outcome.
     * Deadline failure cancels the exact pending owner request; neither is a motor stop.
     */
    MotorControlCAN::Status service(uint64_t nowUs) noexcept;
    /** Withdraw locally and cancel the exact owner request; never send a motor stop.
     * @param nowUs Supplied monotonic microseconds.
     * @return True when a retained active schedule was locally disarmed.
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

  private:
    MotorControlCANOwner::CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SyncContext context_;
    MotorControlCANOwner::SyncRequestId pending_;
    bool retained_ = false;
    bool completionConsumed_ = false;
    MotorControlCAN::Generation armGeneration_ = 0;
};
} // namespace MotorControlCANExample
