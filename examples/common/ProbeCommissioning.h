/** @file ProbeCommissioning.h
 * @brief Three explicit reviewed commissioning workflows over the existing axis/owner path.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "AxisSession.h"
#include <MotorControlCAN/profiles/lichuan_c/Communication.h>
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>
#include <MotorControlCAN/profiles/lichuan_c/Persistence.h>

namespace MotorControlCANExample {
/** One armed or retained concrete workflow; never an unrestricted object sender. */
enum class CommissioningKind : uint8_t {
    NONE,          ///< No context is held.
    COMMUNICATION, ///< Native reviewed heartbeat transition and one confirmation.
    PERSISTENCE,   ///< Native one-attempt save or restore factory settings.
    PDO            ///< Native exact TPDO1 map or event-timer transition and readback.
};
/** What startPersistence needs beyond the target: the caller's own command policy (prompt 57). */
struct PersistencePlan {
    /** Its kind selects SAVE or FACTORY_RESTORE; its target must be target(). Guards default as the
     * library's: no review guard, and the restore's backup guard set. */
    MotorControlCAN::LICHUAN_C::PersistenceReview review;
    /** Budgets of the three prerequisite reads. identityWidths applies to the identity read only; the
     * state and motion parameter reads use the declared widths. */
    MotorControlCAN::LICHUAN_C::ReadOptions reads;
    /** Budgets of the one 2300 write: nonzero, declared widths, no PDO. */
    MotorControlCAN::LICHUAN_C::ReadOptions write;
    uint64_t maximumAgeUs = 0;   ///< Nonzero inclusive prerequisite age cap, caller policy.
    uint32_t attemptOrdinal = 0; ///< Caller's nonzero cumulative command number; no hidden count.
};
/** Optional stationary application storage for three native contexts, with at most one in use.
 * External application code prepares the actual typed context from reviewed specimen/baseline
 * evidence, then arms and explicitly requests it at that exact supplied preparation time. No
 * baseline/read grant, fixture review, model, NMT state or physical fact is manufactured here.
 *
 * Call service after the sole owner's application turn. Route this helper's exact downloads
 * through preflight in Transport::beforeTransmit. Service returns after consuming each terminal
 * and after migration/confirmation preparation, so an external reviewed caller can inspect the
 * native context and supply the exact next one-use service permission before the next admission.
 * This class never authorizes an experiment, services the owner, receives frames, retries, saves
 * implicitly, restores host records or installs/requalifies a PDO map/sample.
 *
 * At the first admitted download it conservatively withdraws affected adapter caches. At its
 * exact possible-TX boundary it invokes the native notice on the actual application binding.
 * Additional application coordinate/reference caches must reject that changed epoch. All calls
 * are serialized by one cooperative application context; resources and this object stay still.
 */
class ProbeCommissioning {
  public:
    /** Borrow the same stationary owner graph; construction performs no work.
     * @param[in] owner Sole physical CAN owner, outlives this helper.
     * @param[in] services The exact service store owned by owner, outlives this helper.
     * @param[in,out] session Open existing axis session; used exclusively while retained.
     * @param[in,out] current Actual application target, advanced once before possible write TX.
     * It outlives this helper; do not pass a disposable copy or replace it during active work.
     */
    ProbeCommissioning(MotorControlCANOwner::CanBusOwner& owner,
                       MotorControlCAN::CANOPEN::SdoClient& services, AxisSession& session,
                       MotorControlCAN::LICHUAN_C::ReadTarget& current) noexcept;
    /** @param owner Expected sole owner. @return True only for this exact owner/native graph. */
    bool usesOwner(const MotorControlCANOwner::CanBusOwner& owner) const noexcept {
        return &owner == &owner_ && owner_.usesServices(services_) && session_.usesOwner(owner_);
    }
    ProbeCommissioning(const ProbeCommissioning&) = delete;            ///< Stationary resources.
    ProbeCommissioning& operator=(const ProbeCommissioning&) = delete; ///< No live-handle copies.
    /** Copy one actual pristine output of prepareCommunication, without owner admission.
     * @param[in] prepared Immutable externally prepared context; borrowed only during this call.
     * @return Local status; unchanged while another context is held or for a progressed context.
     * Exact owner/time/native checks follow at requestCommunication. External metadata remains
     * caller evidence; arming cannot authenticate it or grant service permission.
     */
    MotorControlCAN::Status
    armCommunication(const MotorControlCAN::LICHUAN_C::CommunicationContext& prepared) noexcept;
    /** Copy one actual pristine output of prepareSave or prepareFactoryRestore.
     * @param[in] prepared Immutable external context with its distinct kind and full review.
     * @return Same atomic copy-only arming contract as armCommunication. No save/restore occurs.
     */
    MotorControlCAN::Status
    armPersistence(const MotorControlCAN::LICHUAN_C::PersistenceContext& prepared) noexcept;
    /** Copy one actual pristine output of prepareTpdo1Configuration.
     * @param[in] prepared Immutable external exact-transition context and baseline-derived plan.
     * @return Same atomic copy-only arming contract as armCommunication. No map is installed.
     */
    MotorControlCAN::Status
    armPdo(const MotorControlCAN::LICHUAN_C::Tpdo1ConfigurationContext& prepared) noexcept;
    /** Start the armed communication context at its exact external preparation time.
     * @param[in] nowUs Nondecreasing nonwrapping supplied us, zero valid, at least owner time;
     * exactly the armed context's startedUs and servicedUs.
     * @param[in] deadlineUs Exactly its immutable exclusive future cutoff, including confirmation.
     * @return Local/native validation status. Rejection preserves the armed context and owner
     * operation. Success begins the actual next session operation, retains the context and admits
     * no frame yet. Ordinary migration premises are required before the first write can leave.
     */
    MotorControlCAN::Status requestCommunication(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Start the armed distinct persistence intent; success never proves durability.
     * @param[in] nowUs As requestCommunication. @param[in] deadlineUs As requestCommunication.
     * @return Same atomic request contract. Factory restore's external backup/recommissioning
     * review comes from its actual native preparation. One write only; no rearm/readback/restart.
     */
    MotorControlCAN::Status requestPersistence(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Prepare and start a save or a factory restore here, with no external arm (prompt 57). Only this
     * call starts it: nothing here saves on startup, selection, probe or recovery. The helper reads
     * identity, then STATUS and MONITORS, then MOTION_PARAMETERS through the session, each under its own
     * operation, and when the last read completes prepares the context with prepareSave or
     * prepareFactoryRestore and requests it at that time. Everything runs under deadlineUs; service
     * drives it. A read failure or a preparation refusal ends it before any write, with
     * preparationStatus. The write then follows requestPersistence's contract: one 2300 write, never
     * repeated, its acknowledgement not persistence.
     * @param[in] plan Copied review, budgets, age cap and attempt number.
     * @param[in] nowUs Nondecreasing supplied us, at least owner time.
     * @param[in] deadlineUs Immutable exclusive cutoff of the reads and the write together.
     * @return Local status; the review is checked first (validatePersistenceReview), so a set guard
     * without its record, such as the restore's default backup guard, refuses before any read. Refused
     * while another context is held or the session is busy. Success admits nothing yet.
     */
    MotorControlCAN::Status startPersistence(const PersistencePlan& plan, uint64_t nowUs,
                                             uint64_t deadlineUs) noexcept;
    /** Start the armed exact TPDO1 transition; no arbitrary map, value or identifier edit exists.
     * @param[in] nowUs As requestCommunication. @param[in] deadlineUs As requestCommunication.
     * @return Same atomic request contract. Native current qualified Pre-operational evidence
     * and ordinary migration premises are rechecked; map contents never qualify delayed samples.
     */
    MotorControlCAN::Status requestPdo(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Progress one concrete phase after the ordinary application owner/session service turn.
     * @param[in] nowUs Nondecreasing supplied us, at least owner time. Cutoff equality is expired.
     * @return Local progression status, separate from native terminal outcome. Consumes at most
     * one exact terminal, or migrates/prepares confirmation, or admits one yielded service per
     * call. Retained native write/partial effects and uncertainty survive every later failure.
     * Full result capacity waits under the original cutoff. No owner service or permission grant.
     */
    MotorControlCAN::Status service(uint64_t nowUs) noexcept;
    /** Exact handoff guard; call inside owner's beforeTransmit for this helper's work.
     * @param[in] sequence Actual reservation of the pending work.
     * @param[in] transfer Exact borrowed pending download; unrelated work is rejected.
     * @param[in] nowUs Owner turn's supplied us.
     * @return True only after current owner/native checks and the appropriate note...Transmission
     * updated the actual binding. Performs no owner/service-store mutation or I/O. A false result
     * must not be overridden by a permissive session fallback for the same download.
     */
    bool preflight(MotorControlCANOwner::SequenceId sequence,
                   const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t nowUs) noexcept;
    /** Withdraw remaining local intent; never a motor stop, rollback or command replay.
     * @param[in] nowUs Nondecreasing supplied us, at least owner time.
     * @return False unchanged unless active. An admitted terminal stays correlated until it
     * settles; preflight refuses any still-unsent download. Current uploads may settle/timeout
     * under the original deadline before local withdrawal finishes.
     */
    bool cancel(uint64_t nowUs) noexcept;
    /** Explicitly follow terminal configuration with the same AxisSession object.
     * @param[in] nowUs Nondecreasing supplied us, at least owner time.
     * @return Local status. Waits for actual local TX retirement, then advances an eligible idle
     * owner and closes/reopens the session on current, preserving object-life motion fences.
     * A changed/uncertain restore route needs external reconciliation; no route is guessed.
     * ProbeApp selection separately adopts current. No PDO evidence/producer timing is reinstated.
     */
    MotorControlCAN::Status finishBinding(uint64_t nowUs) noexcept;
    /** Discard inspected armed or terminal history; never active/pending work.
     * @return False unchanged while active. Success does not repair binding/uncertainty,
     * quarantine, durable state or PDO sample qualification; bindingStatus remains visible.
     */
    bool release() noexcept;
    /** @return Which concrete native context is held, including an armed unrequested plan. */
    CommissioningKind kind() const noexcept { return kind_; }
    /** @return True while an unrequested pristine context is held. */
    bool armed() const noexcept { return kind_ != CommissioningKind::NONE && !retained_; }
    /** @return True while startPersistence's prerequisite reads run, before its context exists. */
    bool preparing() const noexcept { return preparing_; }
    /** @return Why a self-prepared persistence ended before its write was prepared or requested; OK
     * otherwise. persistence() then returns an EMPTY context. Expires on release. */
    const MotorControlCAN::Status& preparationStatus() const noexcept { return preparationStatus_; }
    /** @return The operation identity of the held workflow: the armed or retained context's, or for a
     * self-prepared persistence while it reads or after it ended before its write, the identity its write
     * runs or would have run under; zero when none. */
    uint32_t heldOperation() const noexcept {
        return preparing_ || !preparationStatus_.isOk() ? plannedOperation_ : operation();
    }
    /** @return True from successful request through explicit release. */
    bool retained() const noexcept { return retained_; }
    /** @return True while native intent or one admitted terminal is unsettled. */
    bool active() const noexcept;
    /** @return Borrowed matching armed/retained context, otherwise null; expires on release.
     * Native original write evidence, scope, partial effects and unknown requirements remain intact.
     */
    const MotorControlCAN::LICHUAN_C::CommunicationContext* communication() const noexcept;
    /** @return Matching armed/retained native history, otherwise null; expires on release.
     * Verification remains a separate call to verifyPersistence on caller-owned history/evidence.
     */
    const MotorControlCAN::LICHUAN_C::PersistenceContext* persistence() const noexcept;
    /** @return Matching armed/retained native history, otherwise null; expires on release. */
    const MotorControlCAN::LICHUAN_C::Tpdo1ConfigurationContext* pdo() const noexcept;
    /** @return Borrowed actual application target, including a withdrawn configuration epoch. */
    const MotorControlCAN::LICHUAN_C::ReadTarget& target() const noexcept { return current_; }
    /** @return Exact last consumed native terminal, or null; expires on release/request. */
    const MotorControlCAN::CANOPEN::SdoCompletion* lastCompletion() const noexcept {
        return completionPresent_ ? &completion_ : nullptr;
    }
    /** @return Last application-boundary refusal, independent of native outcome/health. */
    const MotorControlCAN::Status& bindingStatus() const noexcept { return bindingStatus_; }

  private:
    MotorControlCANOwner::CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    AxisSession& session_;
    MotorControlCAN::LICHUAN_C::ReadTarget& current_;
    MotorControlCAN::LICHUAN_C::CommunicationContext communication_;
    MotorControlCAN::LICHUAN_C::PersistenceContext persistence_;
    MotorControlCAN::LICHUAN_C::Tpdo1ConfigurationContext pdo_;
    MotorControlCAN::LICHUAN_C::PersistencePrerequisites prior_; ///< startPersistence's own reads.
    PersistencePlan plan_;
    MotorControlCAN::Status preparationStatus_;
    uint64_t planDeadlineUs_ = 0;
    uint32_t plannedOperation_ = 0;
    uint8_t prepareStep_ = 0; ///< 0 identity, 1 state, 2 motion parameters.
    bool preparing_ = false;
    bool readPending_ = false;
    MotorControlCAN::LICHUAN_C::AxisReservation live_;
    MotorControlCANOwner::SequenceId sequence_;
    MotorControlCAN::CANOPEN::SdoTransfer expected_;
    MotorControlCAN::CANOPEN::SdoCompletion completion_;
    MotorControlCAN::Binding submittedBinding_;
    MotorControlCAN::Status bindingStatus_;
    CommissioningKind kind_ = CommissioningKind::NONE;
    uint64_t lastUs_ = 0;
    uint8_t producer_ = 0;
    bool retained_ = false;
    bool pending_ = false;
    bool withdrawn_ = false;
    bool ownerFollowed_ = false;
    bool sessionFollowed_ = false;
    bool snapshotsWithdrawn_ = false;
    bool completionPresent_ = false;
    bool timeValid(uint64_t nowUs) const noexcept;
    bool nativeActive() const noexcept;
    bool effectsPossible() const noexcept;
    uint64_t deadline() const noexcept;
    uint32_t operation() const noexcept;
    const MotorControlCAN::LICHUAN_C::AxisReservation& reservation() const noexcept;
    int slot() const noexcept;
    bool reservationCurrent() const noexcept;
    bool currentNmt(MotorControlCAN::LICHUAN_C::CommunicationNmt& output) const noexcept;
    bool configurationEligible(uint64_t nowUs) const noexcept;
    MotorControlCAN::Status request(CommissioningKind kind, uint64_t nowUs,
                                    uint64_t deadlineUs) noexcept;
    MotorControlCAN::Status sessionReservation(MotorControlCAN::LICHUAN_C::AxisReservation& output) const noexcept;
    MotorControlCAN::LICHUAN_C::ReadContext& prerequisite() noexcept;
    MotorControlCAN::Status startPrerequisite(uint64_t nowUs) noexcept;
    MotorControlCAN::Status servicePreparation(uint64_t nowUs) noexcept;
    MotorControlCAN::Status failPreparation(const MotorControlCAN::Status& status) noexcept;
    MotorControlCAN::Status next(uint64_t nowUs,
                                 MotorControlCAN::LICHUAN_C::PreparedRead& output) const noexcept;
    MotorControlCAN::Status withdraw(MotorControlCAN::ReadOutcome reason, uint64_t nowUs) noexcept;
    MotorControlCAN::Status withdrawSnapshots(uint64_t nowUs) noexcept;
    MotorControlCAN::Status followOwner(uint64_t nowUs) noexcept;
};
} // namespace MotorControlCANExample
