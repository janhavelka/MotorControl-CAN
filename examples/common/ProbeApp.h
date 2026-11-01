/** @file ProbeApp.h
 * @brief Explicit native read application over the sole existing CAN owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "CanBusOwner.h"
#include "ProbePlatform.h"
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>

namespace MotorControlCANExample {
/** One selected target and one finite typed read. No startup request, command
 * parser, firmware startup, discovery, motion or retry engine. The application
 * owns read stepping; the owner alone receives and retains service terminals.
 * One cooperative caller serializes mutation, including platform callbacks.
 */
class ProbeApp {
  public:
    /** Borrow stationary resources; construction performs no I/O or selection.
     * @param[in] owner Sole CAN receive/scheduling owner, outlives this application.
     * @param[in] platform Bounded byte/time facility, outlives this application.
     */
    ProbeApp(CanBusOwner& owner, ProbePlatform& platform) noexcept;
    ProbeApp(const ProbeApp&) = delete;            ///< No copying live reservation/result handles.
    ProbeApp& operator=(const ProbeApp&) = delete; ///< Stationary application storage.
    /** Select explicit configured route facts without device traffic.
     * @param[in] target Copied valid channel/node epoch; no presence/model claim.
     * @return False unchanged while a read is retained or for invalid/unknown binding.
     * Channel currentness and session eligibility are additionally checked at admission.
     */
    bool select(const MotorControlCAN::LICHUAN_C::ReadTarget& target) noexcept;
    /** @return Borrowed selection until successful select/destruction; null before selection. */
    const MotorControlCAN::LICHUAN_C::ReadTarget* selected() const noexcept;
    /** Explicitly prepare finite raw identity reads, without transmitting.
     * @param[in] nowUs Nondecreasing supplied monotonic us; zero valid.
     * @param[in] deadlineUs Exclusive immutable overall cutoff, greater than nowUs.
     * @param[in] options Copied nonzero queue/response budgets; not vendor timing.
     * @return False unchanged for no selection, retained read, stale reservation,
     * invalid budgets or exhausted operation IDs. No bootstrap permission is implied.
     */
    bool requestIdentity(uint64_t nowUs, uint64_t deadlineUs,
                         const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** Explicitly prepare six raw communication configuration uploads, never writes.
     * @param[in] nowUs Same monotonic time domain as requestIdentity.
     * @param[in] deadlineUs Exclusive future overall cutoff.
     * @param[in] options Copied nonzero queue/response budgets.
     * @return Same atomic local-preparation contract as requestIdentity.
     */
    bool requestConfig(uint64_t nowUs, uint64_t deadlineUs,
                       const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** Run owner first with fixed nonzero budgets, then bounded platform I/O.
     * @param[in] nowUs Supplied nondecreasing us; callers may acquire it via platform.nowUs.
     * @return False for backward time/owner rejection, with no platform callbacks.
     * At most one new read field is admitted per turn. Full result capacity waits
     * within the original deadline; no already-admitted work is replayed. A pending
     * terminal is never consumed here. Output pressure never gates owner service.
     */
    bool service(uint64_t nowUs) noexcept;
    /** @return Current owner request handle by value; default invalid between fields. */
    RequestId pendingRequest() const noexcept { return pending_; }
    /** Explicitly consume an inspected pending terminal and release its owner slot.
     * @param[in] nowUs Same nondecreasing us, at least terminal delivery time.
     * @return True advances the typed context, including failure, then releases the
     * service result. False leaves it retained. Borrowed owner result pointers expire
     * on success; context-owned raw evidence remains until releaseRead.
     */
    bool advanceRead(uint64_t nowUs) noexcept;
    /** @return Borrowed concrete context/history until next advance/release/destruction.
     * Use checked typed getters below for current observations; history is not freshness.
     */
    const MotorControlCAN::LICHUAN_C::ReadContext& readContext() const noexcept { return read_; }
    /** Publish current successful raw identity, not model applicability.
     * @param[in] nowUs Nondecreasing supplied us.
     * @param[in] maximumAgeUs Nonzero inclusive per-field age.
     * @param[out] output Owned copy, unchanged for incomplete/wrong/stale read.
     * @return True only with a still-current reservation and checked typed evidence.
     */
    bool getIdentity(uint64_t nowUs, uint64_t maximumAgeUs,
                     MotorControlCAN::LICHUAN_C::IdentityObservation& output) const noexcept;
    /** Publish current six-field raw configuration, without adopting routes/timing.
     * @param[in] nowUs Nondecreasing supplied us.
     * @param[in] maximumAgeUs Nonzero inclusive per-field age.
     * @param[out] output Owned copy, unchanged on wrong/incomplete/stale context.
     * @return True only for checked current CONFIG evidence.
     */
    bool getConfig(uint64_t nowUs, uint64_t maximumAgeUs,
                   MotorControlCAN::LICHUAN_C::ConfigObservation& output) const noexcept;
    /** @param[out] output Cached selected-binding health; unchanged without a current binding.
     * @return Owner cached health result; no clock, I/O or service progress.
     */
    bool health(Health& output) const noexcept;
    /** Release a terminal read/reservation after inspection, never cancel active work.
     * @return True clears local history only when no pending request remains and
     * owner reservation release succeeds. Does not clear quarantine or uncertainty.
     */
    bool releaseRead() noexcept;

  private:
    CanBusOwner& owner_;
    ProbePlatform& platform_;
    MotorControlCAN::LICHUAN_C::ReadTarget target_;
    MotorControlCAN::LICHUAN_C::ReadContext read_;
    SequenceId sequence_;
    RequestId pending_;
    uint64_t lastUs_ = 0;
    uint32_t operation_ = 0;
    bool selected_ = false;
    bool request(bool identity, uint64_t nowUs, uint64_t deadlineUs,
                 const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    bool current() const noexcept;
};
} // namespace MotorControlCANExample
