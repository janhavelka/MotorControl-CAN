/** @file PdoExperiment.h
 * @brief Finite explicit TPDO1 experiment through the ordinary single owner.
 * Application only; no device I/O, NMT, clocks, heap, hidden retries or motion.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "DeviceEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>

namespace MotorControlCANExample {
/** Entire copied exact experiment intent; records are external review attestations. */
struct PdoExperimentPlan {
    ReadExperiment review; ///< Fixture/source/control/failure/reconciliation metadata and budgets.
    MotorControlCAN::LICHUAN_C::Tpdo1ConfigurationReview configurationReview;
    ///< Must be ISOLATED_EXPERIMENT for the exact target and retained fixture/firmware.
    uint8_t slot = 0; ///< Existing service slot; current live route must name node1.
    uint32_t firstAuthorizationId = 0; ///< First of 24 strictly increasing exact grants.
    uint64_t maximumAgeUs = 0;         ///< Nonzero conservative NMT/baseline/result age policy.
};
/** First terminal cause; remote partial/unknown effects remain in core and services. */
enum class PdoExperimentFailure : uint8_t {
    NONE, ///< All 24 services and final exact map readback succeeded, not sample qualification.
    AUTHORIZATION, ///< Exact one-use grant refused; never retried.
    ADMISSION,     ///< Owner rejected one step; unused grant withdrawn.
    SERVICE,       ///< First owner service failure retained.
    CORRELATION,   ///< Owner terminal differs from sole pending work.
    EVIDENCE_LOSS, ///< Bounded copied evidence lost; stop further admission.
    INVALIDATED,   ///< Actual route/reservation/current Pre-operational premise withdrawn.
    DEADLINE,      ///< Immutable overall deadline reached.
    CONFIGURATION, ///< Baseline/core preparation, progression or epoch migration rejected.
    READBACK       ///< Complete service reply contradicted the fixed candidate.
};
enum { PDO_EXPERIMENT_TERMINALS = 24, PDO_EXPERIMENT_TRAFFIC = 112, PDO_EXPERIMENT_TAIL = 16 };
/** Owned acquisition and exact retained prefix, stationary until explicit release. */
struct PdoExperimentRecord {
    PdoExperimentPlan plan; ///< Exact copied admission metadata; no pointers retained.
    MotorControlCAN::LICHUAN_C::SettingsReservation reservation; ///< Actual sequence-issued token.
    MotorControlCAN::CANOPEN::ObservationTime baselinePreOperational;
    ///< Frozen qualified observation after reservation and before all baseline reads.
    MotorControlCAN::Binding
        current; ///< Actual application interpretation epoch, withdrawn before TX.
    MotorControlCAN::LICHUAN_C::Tpdo1ConfigurationContext configuration;
    ///< Public typed sequence result; scope remains ISOLATED_EXPERIMENT.
    bool configurationPrepared = false; ///< Baseline acquired and typed candidate admitted locally.
    bool configurationMigrated = false; ///< Idle owner/adapter synchronized to next configuration.
    bool terminal = false, evidenceLost = false, tailOverflow = false;
    ///< Prefix freezes at first terminal; only tail may then change.
    PdoExperimentFailure failure = PdoExperimentFailure::NONE; ///< First terminal cause.
    BusAdmission admission = BusAdmission::ACCEPTED; ///< Meaningful for ADMISSION failure.
    uint64_t startedUs = 0, completedUs = 0, maxServiceGapUs = 0; ///< Supplied processing times.
    uint8_t attemptedCount = 0, terminalCount = 0;   ///< Prefixes within fixed 24-service bound.
    Completion terminals[PDO_EXPERIMENT_TERMINALS];  ///< All owner terminals, including failure.
    Stats initialStats, finalStats;                  ///< Frozen admission/terminal owner counters.
    OwnerResources initialResources, finalResources; ///< Resource snapshots, not measured peaks.
    uint32_t droppedTraffic = 0;                     ///< Saturating minimum lost diagnostic count.
    uint8_t trafficCount = 0, tailCount = 0;         ///< Valid fixed-storage prefixes only.
    EvidenceTraffic traffic[PDO_EXPERIMENT_TRAFFIC], tail[PDO_EXPERIMENT_TAIL];
};
/** Serialize owner service first, then this collector. Both dependencies outlive it.
 * No concurrent DeviceEvidence/RawEvidence/diagnostic drainer or fixture controller.
 */
class PdoExperiment {
  public:
    PdoExperiment(CanBusOwner& owner, MotorControlCAN::CANOPEN::SdoClient& services) noexcept;
    PdoExperiment(const PdoExperiment&) = delete;
    PdoExperiment& operator=(const PdoExperiment&) = delete;
    /** Explicitly reserve producer3/axis exclusion and baseline operation, without traffic.
     * @param plan Exact node1 experiment, nonzero records, operation ID and age;
     * id+1 is reserved for the core phase. Queue/response budgets must be250000us.
     * @param nowUs Supplied nondecreasing monotonic microseconds before deadline.
     * @return ACCEPTED locally only. Waits for a current qualified Pre-operational
     * observation after reservation acquisition, then nine exact baseline reads.
     * The caller must first set the native service evidence-age policy to a
     * nonzero cap no greater than plan.maximumAgeUs and must not broaden it while
     * this sequence is active. The adapter rechecks that same cap at SDK enqueue.
     */
    EvidenceAdmission begin(const PdoExperimentPlan& plan, uint64_t nowUs) noexcept;
    /** Advance at most one completion and one next admission; never services owner.
     * First possible-write notice occurs immediately after successful owner admission,
     * before any owner service/handoff. requirePreOperational is enforced again by
     * the selected service before handoff and controller enqueue. This conservative withdrawal is retained even
     * if later queue/transport refusal proves no transmission. No rollback or retry.
     * @param nowUs Supplied nondecreasing time. @return False only for caller time
     * or an owner-release invariant; ordinary experiment failures become records.
     */
    bool service(uint64_t nowUs) noexcept;
    const PdoExperimentRecord* record() const noexcept { return occupied_ ? &record_ : nullptr; }
    /** Release terminal history/reservation after all actual local TX retirement.
     * @return False while any exact handoff/report or owner result remains pending;
     * no quarantine, configuration, NMT, firmware qualification or remote effect clears.
     */
    bool release() noexcept;

  private:
    CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    PdoExperimentRecord record_;
    MotorControlCAN::CANOPEN::SdoCompletion baseline_[9]; ///< Fixed core preflight input copies.
    SequenceId sequence_;
    RequestId pending_;
    MotorControlCAN::CANOPEN::SdoTransfer expected_;
    uint64_t lastUs_ = 0;
    uint32_t lastOperation_ = 0, overwritten_ = 0, rxLost_ = 0;
    bool occupied_ = false, pendingValid_ = false;
    PdoExperimentFailure requestedFailure_ = PdoExperimentFailure::NONE;
    void capture(bool tail) noexcept;
    void lose(uint32_t count, bool tail) noexcept;
    void finish(PdoExperimentFailure why, uint64_t nowUs) noexcept;
    bool current() const noexcept;
    bool currentPreOperational(uint64_t nowUs,
                               MotorControlCAN::CANOPEN::ObservationTime& output) const noexcept;
};
} // namespace MotorControlCANExample
