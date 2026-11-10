/** @file Persistence.h
 * @brief Explicit 2300 commands and independently submitted reconciliation; no I/O or retries.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Distinct vendor intents; no generic integer command or restart API. */
enum class PersistenceKind : uint8_t {
    SAVE,           ///< 2300:00 U16 value 2, save parameters.
    FACTORY_RESTORE ///< 2300:00 U16 value 1, restore factory settings.
};
/** Documentary requirements are not inferred from a successful service. */
enum class PersistenceRequirement : uint8_t {
    UNKNOWN,     ///< No applicable evidence establishes the requirement.
    REQUIRED,    ///< Applicable evidence establishes the requirement.
    NOT_REQUIRED ///< Applicable evidence establishes its absence.
};
/** Finite command phase, distinct from durability knowledge. */
enum class PersistenceState : uint8_t {
    EMPTY,        ///< No admitted intent.
    WRITING,      ///< One command awaits its exact terminal.
    ACKNOWLEDGED, ///< Protocol success only; persistence remains unverified.
    FAILED        ///< Immutable failed intent; later observations cannot replay or replace it.
};
/** Exact caller-reviewed specimen/procedure scope, never fabricated by an identity read. */
struct PersistenceReview {
    ReadTarget target;                            ///< Actual original route and all epochs.
    PersistenceKind kind = PersistenceKind::SAVE; ///< Only this intent was reviewed.
    uint32_t fixtureRecord = 0;   ///< Nonzero retained actual nameplate/wiring/specimen record.
    uint32_t firmwareRecord = 0;  ///< Nonzero exact model/firmware applicability record.
    uint32_t procedureRecord = 0; ///< Nonzero reviewed bounded procedure and effect-risk record.
    uint32_t authorizationRecord = 0; ///< Nonzero explicit execution authorization record.
    uint32_t backupRecord = 0; ///< Factory restore: external complete configuration backup record.
    uint32_t routeBackRecord = 0; ///< Factory restore: reviewed actual recommissioning route plan.
};
/** Actual independently acquired prerequisites; preparation copies checked observations only.
 * These reads do not prove a stopped shaft or a complete backup. No enable/stop preamble exists.
 */
struct PersistencePrerequisites {
    ReadContext identity; ///< Fresh complete identity read under the reservation.
    ReadContext state;    ///< Fresh successful STATUS/MONITORS services under the reservation.
    DriverContext configuration; ///< Fresh MOTION_PARAMETERS read: 6081/6083/6084 only.
};
/** Owned partial configuration record; never presented as every saved or restored parameter. */
struct PersistenceRecord {
    SettingsModel model = SettingsModel::UNKNOWN; ///< Actual caller-established model.
    uint32_t fixtureRecord = 0;                   ///< Original external fixture provenance.
    uint32_t firmwareRecord = 0;                  ///< Original external firmware provenance.
    IdentityObservation identity;    ///< Original raw identity and per-field routes/timestamps.
    DriverObservation configuration; ///< Three original motion fields and provenance.
    StateObservation monitors;       ///< Original 2100..2102 observations.
    StateObservation status;         ///< Original 6041/6061 observations, no torque guarantee.
};
/** Explicit later evidence from the actual current route; no bus work is generated. */
struct PersistenceVerification {
    ReadTarget current; ///< Actual reacquired route and current owner epochs.
    SettingsModel model =
        SettingsModel::UNKNOWN;  ///< Independently established same specimen model.
    uint32_t fixtureRecord = 0;  ///< Must match original specimen record.
    uint32_t firmwareRecord = 0; ///< Must match original firmware record.
    uint32_t routeRecord = 0;    ///< Nonzero external identity/route reconciliation record.
    PersistencePrerequisites observations; ///< Fresh post-command identity/state/motion reads.
    uint64_t maximumAgeUs = 0; ///< Nonzero inclusive observation age limit, caller policy.
    bool powerCycleObserved =
        false;                 ///< Actual drive power cycle, never MCU restart or NMT inference.
    uint64_t powerCycleUs = 0; ///< Supplied completion time of the externally observed power cycle.
    uint32_t powerCycleRecord = 0; ///< Required nonzero retained independent physical evidence.
    ReadTarget powerCycleTarget;   ///< Actual route/epochs established immediately after the cycle,
                                   ///< before any configuration write; unchanged through the reads.
    uint32_t noInterveningWritesRecord = 0; ///< Nonzero reviewed continuous observation/exclusion
        ///< record proving no configuration writes between that cycle boundary and these reads.
        ///< Bus silence alone is insufficient; the history covers other producers and replay too.
};
/** Caller-owned single attempt and immutable historical write outcome.
 * All public fields are read-only between calls. Verification may record surviving fields even
 * after a lost reply, but cannot prove this command caused them or clear execution uncertainty.
 */
struct PersistenceContext {
    AxisReservation reservation; ///< Original continuous exclusion and frozen target.
    PersistenceReview review;    ///< Original bounded command review, never promoted by an ACK.
    PersistenceRecord before; ///< Original partial configuration, preserved through every failure.
    PersistenceRecord observed; ///< Last accepted independent reconciliation, if verificationKnown.
    PersistenceState state = PersistenceState::EMPTY; ///< Local phase only.
    PersistenceKind kind = PersistenceKind::SAVE;     ///< Frozen distinct intent.
    ReadOutcome outcome =
        ReadOutcome::NONE; ///< Original terminal cause, never overwritten by verification.
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED; ///< Protocol execution evidence.
    Status status;             ///< Original terminal call-independent status.
    ReadOptions options;       ///< Explicit finite queue/response budgets.
    uint32_t operation = 0;    ///< Nonzero caller identity, not a wire token.
    uint64_t startedUs = 0;    ///< Supplied preparation time, zero valid.
    uint64_t servicedUs = 0;   ///< Last accepted local mutation time.
    uint64_t deadlineUs = 0;   ///< Immutable exclusive command cutoff.
    uint64_t maximumAgeUs = 0; ///< Nonzero original prerequisite age policy.
    uint32_t attemptOrdinal =
        0;                ///< Caller's nonzero cumulative command number, not flash endurance.
    uint8_t attempts = 0; ///< Zero before possible TX, exactly one afterwards; no retry.
    bool transmissionNoted = false; ///< Owner crossed possible-TX boundary exactly once.
    bool effectsPossible = false;   ///< Persistent/volatile effect may exist, even with no reply.
    bool prerequisitesWithdrawn =
        false;              ///< Configuration/reference/maps/prepared work must be retired.
    bool uncertain = false; ///< Unconfirmed execution; independent verification never clears this.
    bool writeEvidencePresent = false;    ///< Makes exact retained writeEvidence meaningful.
    CANOPEN::SdoCompletion writeEvidence; ///< Original native terminal including unsigned abort.
    bool verificationKnown = false;       ///< A fresh explicit reconciliation was accepted.
    uint8_t matchingFields = 0; ///< Bits 0..2: current 6081/6083/6084 equal original values.
    uint8_t survivedRestart =
        0; ///< SAVE only: matching fields observed after qualified power cycle.
    uint32_t verificationRecord = 0; ///< Last accepted route reconciliation record.
    uint32_t powerCycleRecord = 0;   ///< Last accepted independently observed power-cycle record.
    ReadTarget powerCycleTarget; ///< Retained actual post-cycle boundary used for field survival.
    uint32_t noInterveningWritesRecord = 0; ///< Retained uninterrupted no-write history provenance.
    PersistenceRequirement restart =
        PersistenceRequirement::UNKNOWN; ///< Documentary requirement unknown.
    PersistenceRequirement rearm = PersistenceRequirement::UNKNOWN; ///< No zero write is generated.
    PersistenceRequirement completeSavedSet =
        PersistenceRequirement::UNKNOWN; ///< No complete scope known.
};
/** Prepare one explicit save, without writing or promising durability.
 * @param[out] output Unchanged on rejection; inputs must not overlap it.
 * @param[in] reservation Actual owner exclusion acquired before all prerequisite reads.
 * @param[in] current Actual current route/epochs, exactly matching the reservation.
 * @param[in] review Exact SAVE review and nonzero fixture/firmware/procedure/authorization records.
 * @param[in] prior Fresh identity, successful STATUS/MONITORS services and motion settings.
 * The 2100 report must have no running, alarm or undocumented bit. The disputed 6041 word is
 * retained without inferring torque/standstill from it; no physical stationary fact is invented.
 * @param[in] operation Nonzero unique live operation identity.
 * @param[in] attemptOrdinal Nonzero caller-maintained cumulative save number; no hidden reset/count.
 * @param[in] nowUs Supplied nonwrapping microseconds.
 * @param[in] deadlineUs Immutable exclusive cutoff, sufficient for one exchange.
 * @param[in] options Nonzero service budgets, no identity/count/PDO override.
 * @param[in] maximumAgeUs Nonzero inclusive prerequisite age limit.
 * @return Local validation only. State policy is conservative host policy, not vendor timing.
 */
Status prepareSave(PersistenceContext& output, const AxisReservation& reservation,
                   const ReadTarget& current, const PersistenceReview& review,
                   const PersistencePrerequisites& prior, uint32_t operation,
                   uint32_t attemptOrdinal, uint64_t nowUs, uint64_t deadlineUs,
                   const ReadOptions& options, uint64_t maximumAgeUs) noexcept;
/** Prepare one explicit restore factory settings, with identical atomicity/state rules to save.
 * @param[out] output Unchanged on rejection, no input overlap.
 * @param[in] reservation Actual continuous owner exclusion.
 * @param[in] current Actual current route/epochs.
 * @param[in] review Exact FACTORY_RESTORE review plus external backup and route-back records.
 * @param[in] prior Fresh checked partial snapshots; these do not replace the complete backup.
 * @param[in] operation Nonzero unique live operation identity.
 * @param[in] attemptOrdinal Nonzero caller-maintained cumulative restore number.
 * @param[in] nowUs Supplied microseconds.
 * @param[in] deadlineUs Immutable exclusive cutoff.
 * @param[in] options Explicit service budgets, no unrelated width/PDO override.
 * @param[in] maximumAgeUs Nonzero inclusive age limit.
 * @return Local status. Never guesses defaults, active route, restart requirement or torque state.
 */
Status prepareFactoryRestore(PersistenceContext& output, const AxisReservation& reservation,
                             const ReadTarget& current, const PersistenceReview& review,
                             const PersistencePrerequisites& prior, uint32_t operation,
                             uint32_t attemptOrdinal, uint64_t nowUs, uint64_t deadlineUs,
                             const ReadOptions& options, uint64_t maximumAgeUs) noexcept;
/** Yield the single command; repeated inspection before admission is not replay permission.
 * @param[in] context Prepared intent. @param[in] live Actual continuous original reservation.
 * @param[in] current Actual binding, next configuration epoch after notice.
 * @param[in] nowUs Nondecreasing supplied time. @param[out] output Unchanged on error; terminal/noted calls set ready=false.
 * @return Local status; no automatic rearm, reset, readback or second command.
 */
Status nextPersistence(const PersistenceContext& context, const AxisReservation& live,
                       const Binding& current, uint64_t nowUs, PreparedRead& output) noexcept;
/** Withdraw prerequisites immediately before owner handoff, exactly once.
 * @param[in,out] context Prepared write. @param[in] live Actual continuous reservation.
 * @param[in,out] current Actual binding; configuration generation advances once.
 * @param[in] nowUs Supplied microseconds before cutoff with remaining exchange budget.
 * @return Atomic local status. Owner additionally invalidates producer/maps/reference/prepared work
 * and retains queued/native work until established retirement; no route is guessed or adopted.
 */
Status notePersistenceTransmission(PersistenceContext& context, const AxisReservation& live,
                                   Binding& current, uint64_t nowUs) noexcept;
/** Consume exact native evidence; ACK is protocol success only.
 * @param[in,out] context Active intent; historical ingestion never admits work.
 * @param[in] original Frozen original reservation associated with the admitted command. It need
 * not remain live: REBOOT/BINDING_INVALIDATED terminals must survive owner reservation retirement.
 * @param[in] submittedBinding Frozen application binding after any possible-TX notice. It is
 * correlation evidence, not authority to use a now-retired route or to migrate its owner binding.
 * @param[in] evidence Exact retained owner terminal from that original submission.
 * @param[in] nowUs Nondecreasing time at least completion; deadline wins at equality.
 * @return Atomic validation status; valid failures are retained terminal outcomes, never retries.
 * A matching ACK delivered at the cutoff retains ACKNOWLEDGED execution but FAILED/DEADLINE
 * intent. Execution uncertainty may clear from that actual ACK; durability remains unverified.
 */
Status advancePersistence(PersistenceContext& context, const AxisReservation& original,
                          const Binding& submittedBinding, const CANOPEN::SdoCompletion& evidence,
                          uint64_t nowUs) noexcept;
/** Withdraw local intent after separately retiring owner work; never a motor command.
 * @param[in,out] context Active intent. @param[in] reason CANCELLED, DEADLINE or INVALIDATED.
 * @param[in] nowUs Supplied nondecreasing time; cutoff takes precedence.
 * @return Atomic status, preserving possible effects and all historical evidence.
 */
Status advancePersistence(PersistenceContext& context, ReadOutcome reason, uint64_t nowUs) noexcept;
/** Reconcile independently acquired current-route evidence, even after terminal failure.
 * @param[in,out] context Terminal intent, original before/write/outcome/uncertainty preserved.
 * @param[in] verification Current route, same specimen identity and explicit post-command reads.
 * @param[in] nowUs Nondecreasing supplied microseconds; verification has its own age policy.
 * @return Atomic status. Matching live fields are not durable. Only SAVE matching fields after
 * independently evidenced actual power cycle receive survivedRestart bits. The exact post-cycle
 * target must remain unchanged, with externally established uninterrupted no-write history through
 * the reads; host re-application invalidates this proof. This proves field survival,
 * not command causality. Factory defaults and every other parameter remain unverified.
 */
Status verifyPersistence(PersistenceContext& context, const PersistenceVerification& verification,
                         uint64_t nowUs) noexcept;

} // namespace LICHUAN_C
} // namespace MotorControlCAN
