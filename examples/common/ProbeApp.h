/** @file ProbeApp.h
 * @brief Explicit native read application over the sole existing CAN owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "AxisSession.h"
#include "CanBusOwner.h"
#include "ProbePlatform.h"
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>

namespace MotorControlCANExample {
using MotorControlCANOwner::Health;
using MotorControlCANOwner::RequestId;
using MotorControlCANOwner::SequenceId;
using MotorControlCANOwner::CanBusOwner;
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
    /** Explicit minimal 1000 upload, not an identity sweep or non-changing-read assertion.
     * @param nowUs As requestIdentity. @param deadlineUs As requestIdentity.
     * @param options As requestIdentity. @return Same preparation/retention contract.
     */
    bool requestProbe(uint64_t nowUs, uint64_t deadlineUs,
                      const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** Explicitly prepare six raw communication configuration uploads, never writes.
     * @param[in] nowUs Same monotonic time domain as requestIdentity.
     * @param[in] deadlineUs Exclusive future overall cutoff.
     * @param[in] options Copied nonzero queue/response budgets.
     * @return Same atomic local-preparation contract as requestIdentity.
     */
    bool requestConfig(uint64_t nowUs, uint64_t deadlineUs,
                       const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** Prepare explicit raw state blocks, with no status dialect or harmless-read inference.
     * @param nowUs Same supplied monotonic microseconds.
     * @param deadlineUs Exclusive immutable future cutoff.
     * @param options Copied nonzero queue/response budgets.
     * @param blockMask Nonzero StateBlock bits 0..5, no implicit selection.
     * @return Same atomic preparation contract as requestIdentity; no traffic yet.
     */
    bool requestState(uint64_t nowUs, uint64_t deadlineUs,
                      const MotorControlCAN::LICHUAN_C::ReadOptions& options,
                      uint8_t blockMask) noexcept;
    /** Cancel the current read locally, retaining any pending owner terminal for advanceRead.
     * @param nowUs Nondecreasing supplied microseconds; earlier terminal/deadline wins.
     * @return False for no active read or invalid time; true performs no receive/new TX.
     * Between fields, settles the typed context directly; never means motor stop.
     */
    bool cancelRead(uint64_t nowUs) noexcept;
    /** Run owner first with fixed nonzero budgets, then bounded platform I/O.
     * @param[in] nowUs Supplied nondecreasing us; callers may acquire it via platform.nowUs.
     * @return False for backward time/owner rejection, with no platform callbacks.
     * At most one new read field is admitted per turn. Full result capacity waits
     * within the original deadline; no already-admitted work is replayed. A pending
     * terminal is never consumed here. Output pressure never gates owner service.
     */
    bool service(uint64_t nowUs) noexcept;
    /** Service with explicit owner work bounds instead of the default nonzero budgets.
     * @param nowUs Supplied nondecreasing microseconds.
     * @param budget Caller work bounds, clamped/validated by the same owner; zero work stays zero.
     * @return Same service result/retention contract; at most one read field is admitted.
     */
    bool service(uint64_t nowUs, const MotorControlCANOwner::ServiceBudget& budget) noexcept;
    /** Open one axis's session: latest controlword and stop work. An active read's
     * reservation passes to the session, and later reads use the session's.
     * @param[in] session Caller-owned closed session, borrowed until closed. Sessions are
     * large, so an application that sends no commands stores none.
     * @param[in] target Current route and node epoch.
     * @param[in] model Applicable model, supplied from application evidence.
     * @param[in] nowUs Supplied nondecreasing us.
     * @return The session, or null when two are already open, this binding has one, or
     * the target is stale.
     */
    AxisSession* openAxis(AxisSession& session,
                          const MotorControlCAN::LICHUAN_C::ReadTarget& target,
                          MotorControlCAN::LICHUAN_C::SettingsModel model, uint64_t nowUs) noexcept;
    /** @param[in] binding Exact axis binding.
     * @return Open session of that binding, or null. Sessions advance in service.
     */
    AxisSession* axis(const MotorControlCAN::Binding& binding) noexcept;
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
    /** Publish current successful minimal presence, not model/firmware qualification.
     * @param nowUs Supplied nondecreasing us. @param maximumAgeUs Nonzero original-observation age.
     * @param output Owned result unchanged on failure. @return Checked current PROBE only; no I/O.
     */
    bool getProbe(uint64_t nowUs, uint64_t maximumAgeUs,
                  MotorControlCAN::LICHUAN_C::ProbeObservation& output) const noexcept;
    /** Publish current six-field raw configuration, without adopting routes/timing.
     * @param[in] nowUs Nondecreasing supplied us.
     * @param[in] maximumAgeUs Nonzero inclusive per-field age.
     * @param[out] output Owned copy, unchanged on wrong/incomplete/stale context.
     * @return True only for checked current CONFIG evidence.
     */
    bool getConfig(uint64_t nowUs, uint64_t maximumAgeUs,
                   MotorControlCAN::LICHUAN_C::ConfigObservation& output) const noexcept;
    /** Publish one complete current raw state block without interpreted motion semantics.
     * @param block Exact selected StateBlock.
     * @param nowUs Supplied nondecreasing microseconds.
     * @param maximumAgeUs Nonzero inclusive per-field age.
     * @param output Owned result, unchanged on incomplete/stale/wrong evidence.
     * @return True only for checked typed evidence and a still-current reservation.
     */
    bool getStateBlock(MotorControlCAN::LICHUAN_C::StateBlock block, uint64_t nowUs,
                       uint64_t maximumAgeUs,
                       MotorControlCAN::LICHUAN_C::StateObservation& output) const noexcept;
    /** @param[out] output Cached selected-binding health; unchanged without a current binding.
     * @return Owner cached health result; no clock, I/O or service progress.
     */
    bool health(Health& output) const noexcept;
    /** Release a terminal read/reservation after inspection, never cancel active work.
     * @return True clears local history only when no pending request remains and
     * owner reservation release succeeds. A reservation held by an axis session stays
     * with the session. Does not clear quarantine or uncertainty.
     */
    bool releaseRead() noexcept;

  private:
    CanBusOwner& owner_;
    ProbePlatform& platform_;
    AxisSession* axes_[2] = {nullptr, nullptr};
    MotorControlCAN::LICHUAN_C::ReadTarget target_;
    MotorControlCAN::LICHUAN_C::ReadContext read_;
    SequenceId sequence_;
    RequestId pending_;
    uint64_t lastUs_ = 0;
    uint32_t operation_ = 0;
    bool selected_ = false;
    bool readOwnsSequence_ = false;
    bool request(MotorControlCAN::LICHUAN_C::ReadKind kind, uint64_t nowUs, uint64_t deadlineUs,
                 const MotorControlCAN::LICHUAN_C::ReadOptions& options, uint8_t mask = 0) noexcept;
    bool current() const noexcept;
};
} // namespace MotorControlCANExample
