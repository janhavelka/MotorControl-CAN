/** @file ProbeSettings.h
 * @brief Explicit bounded typed settings over a borrowed axis session and the sole CAN owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "AxisSession.h"
#include "ConfigurationRestore.h"
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>

namespace MotorControlCANExample {
/** One retained driver, soft-limit, DI or control transaction. This application helper never
 * acquires prerequisites, grants reviewed reads, receives frames or services the CAN owner.
 * Call service after ProbeApp::service, and route the owner's beforeTransmit callback here for
 * this bridge's downloads. All calls and borrowed resources belong to one cooperative context.
 *
 * A possible settings transmission withdraws the actual target's configuration epoch once. Old
 * snapshots then fail their ordinary epoch checks. The owner follows only after the writes'
 * terminals have been released, before readback. finishBinding explicitly follows the terminal
 * configuration with the same AxisSession object, preserving its object-life motion fences.
 * The application must invalidate any additional physical-reference/map caches at this boundary;
 * this helper cannot reach unlisted application storage. No save, reset, enable or retry occurs.
 */
class ProbeSettings {
  public:
    /** Borrow existing stationary resources; construction performs no work.
     * @param[in] owner Sole receive/service owner, outlives this helper.
     * @param[in] services The exact service store borrowed by owner; used for read-only premises.
     * @param[in,out] session Existing axis session, kept open and exclusively used by this helper
     * while a request is retained. A separately requested stop may supersede the work.
     * @param[in,out] current Actual application target, outlives this helper. Its binding advances
     * at possible TX; do not pass a disposable copy or mutate it during an active transaction.
     */
    ProbeSettings(MotorControlCANOwner::CanBusOwner& owner,
                  MotorControlCAN::CANOPEN::SdoClient& services, AxisSession& session,
                  MotorControlCAN::LICHUAN_C::ReadTarget& current) noexcept;
    /** @param owner Expected sole owner. @return True only for this exact owner/native graph. */
    bool usesOwner(const MotorControlCANOwner::CanBusOwner& owner) const noexcept {
        return &owner == &owner_ && owner_.usesServices(services_) && session_.usesOwner(owner_);
    }
    ProbeSettings(const ProbeSettings&) = delete;            ///< Stationary borrowed resources.
    ProbeSettings& operator=(const ProbeSettings&) = delete; ///< No copying live work.

    /** Name actual externally acquired prerequisites, without acquiring or qualifying them.
     * @param[in] prior Successful complete native group under the session's current reservation;
     * borrowed until replaced here or the next request succeeds. It must outlive that borrowing.
     * @param[in] state Actual STATUS/MONITORS read, borrowed for the same lifetime as prior.
     * @param[in] control Actual acknowledged 6040 terminal, or null for groups that do not need it.
     * Driver settings require the session's still-latest 000F acknowledgement. Borrowed as prior.
     * @param[in] model Explicit application-established model; no identity-code inference.
     * @param[in] options Copied finite queue/response budgets and applicable native width policy.
     * @param[in] maximumAgeUs Nonzero inclusive age limit on original observations, in supplied us.
     * @return Local status, unchanged on rejection. Core preparation rechecks provenance, state,
     * group, reservation acquisition time and freshness on every request. Naming these contexts
     * grants no upload permission and fabricates no read, motion state or physical standstill.
     */
    MotorControlCAN::Status setPrerequisites(const MotorControlCAN::LICHUAN_C::DriverContext& prior,
                                             const MotorControlCAN::LICHUAN_C::ReadContext& state,
                                             const MotorControlCAN::CANOPEN::SdoCompletion* control,
                                             MotorControlCAN::LICHUAN_C::SettingsModel model,
                                             const MotorControlCAN::LICHUAN_C::ReadOptions& options,
                                             uint64_t maximumAgeUs) noexcept;

    /** Prepare explicit native position speed/ramp writes through prepareDriverSettings.
     * @param[in] settings Complete candidate: rpm and ramp times in ms, copied on success.
     * @param[in] nowUs Nondecreasing nonwrapping supplied us, at least owner time; zero valid.
     * @param[in] deadlineUs Exclusive immutable future cutoff of writes and full readback.
     * @return Core validation status, or INVALID_CONFIG for missing/stale owner prerequisites.
     * Rejection publishes no context and preserves the owner operation/prerequisite borrowing;
     * success retains one context until release. No work is admitted here. An unchanged candidate
     * can complete without TX. Unretained internal storage is used for atomic core preparation.
     */
    MotorControlCAN::Status
    requestDriver(const MotorControlCAN::LICHUAN_C::DriverSettings& settings, uint64_t nowUs,
                  uint64_t deadlineUs) noexcept;
    /** Prepare explicit 2202 selector through prepareSoftLimitSettings; never writes 607D.
     * @param[in] settings Complete documented selector candidate, copied on success.
     * @param[in] nowUs As requestDriver. @param[in] deadlineUs As requestDriver.
     * @return Same atomic preparation/retention contract as requestDriver. Matching readback
     * proves the stored code only; limit enforcement, frame and reaction remain unqualified.
     */
    MotorControlCAN::Status
    requestSoftLimits(const MotorControlCAN::LICHUAN_C::SoftLimitSettings& settings, uint64_t nowUs,
                      uint64_t deadlineUs) noexcept;
    /** Prepare complete explicit DI assignments/polarity through prepareIoSettings.
     * @param[in] settings All seven functions and documented polarity bits, copied on success.
     * @param[in] nowUs As requestDriver. @param[in] deadlineUs As requestDriver.
     * @return Same atomic preparation/retention contract as requestDriver. Intermediate writes
     * can immediately affect enable/stop/limit inputs; no electrical safety is inferred.
     */
    MotorControlCAN::Status requestIo(const MotorControlCAN::LICHUAN_C::IoSettings& settings,
                                      uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Prepare explicit 2500 loop selection through prepareControlSettings.
     * @param[in] settings Complete documented selector, copied on success.
     * @param[in] nowUs As requestDriver. @param[in] deadlineUs As requestDriver.
     * @return Same atomic preparation/retention contract as requestDriver. No encoder word is
     * written. Loop selection can change torque immediately; readback is register evidence only.
     */
    MotorControlCAN::Status
    requestControl(const MotorControlCAN::LICHUAN_C::ControlSettings& settings, uint64_t nowUs,
                   uint64_t deadlineUs) noexcept;
    /** Admit a caller-owned output of prepareConfigurationRestore at its exact preparation time.
     * @param[in] prepared Immutable actual output of prepareConfigurationRestore, borrowed only
     * for this call. The application prepares it in its own bounded storage using actual fresh
     * identity, current settings/state/control and explicit specimen/route review. This call
     * does not acquire or authenticate those external facts. Its pristine native plan must name
     * this live reservation, the next session operation and exactly nowUs/deadlineUs.
     * @param[in] nowUs As requestDriver. @param[in] deadlineUs As requestDriver.
     * @return Local validation/admission status with requestDriver's rejection/retention contract.
     * The existing helper has checked all prerequisites at this same time; this bridge checks
     * its native plan and actual owner boundary before copying it. Only its owned settings plan
     * is retained here; the application retains original/review history. This is host
     * re-application, never parameter save or restore factory settings. No large scratch context
     * is placed on the console stack or allocated by this bridge.
     */
    MotorControlCAN::Status requestRestore(const ConfigurationRestoreContext& prepared,
                                           uint64_t nowUs, uint64_t deadlineUs) noexcept;

    /** Progress once after the application's owner/session service turn; never calls owner.service.
     * @param[in] nowUs Nondecreasing supplied us, at least owner time. At equality the deadline wins.
     * @return Local progression status. Consumes at most one exact session terminal and admits at
     * most one next field. Full result capacity waits under the original deadline. Valid service
     * failures remain in context(), including partial confirmations, aborts and uncertainty.
     */
    MotorControlCAN::Status service(uint64_t nowUs) noexcept;
    /** Exact possible-transmission guard, called inside the owner's beforeTransmit callback.
     * @param[in] sequence Actual reservation of the pending download.
     * @param[in] transfer Exact borrowed download about to cross the controller boundary.
     * @param[in] nowUs Owner turn's supplied us.
     * @return True only for this session's exact active settings download, current migration
     * premises and a successful noteDriverTransmission on current's actual binding. False for
     * unrelated work, cancellation, changed binding, expired time or missing migration premises.
     * Performs no owner mutation/I/O; the caller must combine this with other owners' guards.
     */
    bool preflight(MotorControlCANOwner::SequenceId sequence,
                   const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t nowUs) noexcept;
    /** Withdraw remaining settings locally, without motor stop, rollback or replay.
     * @param[in] nowUs Nondecreasing supplied us, at least owner time.
     * @return False unchanged without active work or with backward time. An admitted service is
     * retained until its exact terminal; preflight refuses an unsent download. A current upload
     * may finish/timeout under the original deadline before withdrawal is terminal.
     */
    bool cancel(uint64_t nowUs) noexcept;
    /** Explicitly follow a terminal changed configuration with the existing session object.
     * @param[in] nowUs Nondecreasing supplied us, at least owner time.
     * @return Local status. The owner advances after all its results/handoffs are retired, then
     * the idle session closes/reopens on current using its actual producer/model. No device I/O
     * occurs. A failed migration stays visible in bindingStatus and can be tried after its
     * missing premise is established; settings work is never replayed. A stale ProbeApp selection
     * must separately select current. Unresolved/rebooted routes require external reconciliation.
     */
    MotorControlCAN::Status finishBinding(uint64_t nowUs) noexcept;
    /** Release inspected terminal history only, never active or pending work.
     * @return False unchanged unless one terminal is retained. Success clears this context and
     * prerequisite borrowing. A failed binding follow remains in bindingStatus; current stays
     * withdrawn and a new request still needs a reconciled live reservation. No uncertainty,
     * drive fault, session motion fence or owner quarantine is repaired by releasing a result.
     */
    bool release() noexcept;
    /** @return True from successful request through explicit release. */
    bool retained() const noexcept { return retained_; }
    /** @return True while settings or one admitted terminal still need settlement. */
    bool active() const noexcept;
    /** @return Borrowed actual application target, including any withdrawn configuration epoch.
     * No freshness, route repair or model claim is implied by inspection.
     */
    const MotorControlCAN::LICHUAN_C::ReadTarget& target() const noexcept { return current_; }
    /** @return Owned concrete history while retained(), borrowed until release/new request.
     * The storage is unretained preparation scratch otherwise; callers must not inspect it.
     * Readback is raw register evidence. Use the core's checked getters with actual live facts
     * while exclusion still holds, before finishBinding can acquire a new reservation.
     */
    const MotorControlCAN::LICHUAN_C::DriverSettingsContext& context() const noexcept {
        return context_;
    }
    /** @return Last exact consumed terminal, or null before one; borrowed until release/request.
     * Retained even when a separately changed target prevents the core from ingesting it.
     */
    const MotorControlCAN::CANOPEN::SdoCompletion* lastCompletion() const noexcept {
        return completionPresent_ ? &completion_ : nullptr;
    }
    /** @return Last binding-follow or preflight refusal, independent of the native outcome. */
    const MotorControlCAN::Status& bindingStatus() const noexcept { return bindingStatus_; }

  private:
    MotorControlCANOwner::CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    AxisSession& session_;
    MotorControlCAN::LICHUAN_C::ReadTarget& current_;
    const MotorControlCAN::LICHUAN_C::DriverContext* prior_ = nullptr;
    const MotorControlCAN::LICHUAN_C::ReadContext* state_ = nullptr;
    const MotorControlCAN::CANOPEN::SdoCompletion* control_ = nullptr;
    MotorControlCAN::LICHUAN_C::SettingsModel model_ =
        MotorControlCAN::LICHUAN_C::SettingsModel::UNKNOWN;
    MotorControlCAN::LICHUAN_C::ReadOptions options_;
    uint64_t maximumAgeUs_ = 0;
    MotorControlCAN::LICHUAN_C::DriverSettingsContext context_;
    MotorControlCAN::LICHUAN_C::AxisReservation live_;
    MotorControlCANOwner::SequenceId sequence_;
    MotorControlCAN::CANOPEN::SdoTransfer expected_;
    MotorControlCAN::CANOPEN::SdoCompletion completion_;
    MotorControlCAN::Status bindingStatus_;
    uint64_t lastUs_ = 0;
    uint8_t producer_ = 0;
    bool retained_ = false;
    bool pending_ = false;
    bool withdrawn_ = false;
    bool ownerFollowed_ = false;
    bool sessionFollowed_ = false;
    bool completionPresent_ = false;
    bool timeValid(uint64_t nowUs) const noexcept;
    bool contextActive() const noexcept;
    bool configurationEligible(uint64_t nowUs) const noexcept;
    bool reservationCurrent() const noexcept;
    MotorControlCAN::Status preparation(uint64_t nowUs, uint64_t deadlineUs,
                                        MotorControlCAN::LICHUAN_C::AxisReservation& reservation,
                                        uint32_t& operation) const noexcept;
    MotorControlCAN::Status accept(uint64_t nowUs) noexcept;
    MotorControlCAN::Status followOwner(uint64_t nowUs) noexcept;
};
} // namespace MotorControlCANExample
