/** @file PdoExperiment.h
 * @brief Finite explicit TPDO1 configuration through the ordinary single owner.
 * The bench caller of the profile's TPDO1 lifecycle: the isolated map3-to-map2 experiment (`m`)
 * and the supported 1800:05 event-timer change (`E`). Application only; no device I/O, NMT,
 * clocks, heap, hidden retries or motion.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "DeviceEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>

namespace MotorControlCANExample {
using MotorControlCANOwner::Stats;
using MotorControlCANOwner::RequestId;
using MotorControlCANOwner::SequenceId;
using MotorControlCANOwner::BusAdmission;
using MotorControlCANOwner::Completion;
using MotorControlCANOwner::OwnerResources;
using MotorControlCANOwner::CanBusOwner;
/** Entire copied exact intent; records are external review attestations.
 * MAP3_TO_MAP2 is the isolated node-1 experiment: scope ISOLATED_EXPERIMENT, 24 services.
 * EVENT_TIMER is the supported change on node 1 or 2: scope FIXTURE_QUALIFIED, 19 services
 * (nine baseline reads, one 1800:05 write, nine final reads). Anything else refuses at begin.
 */
struct PdoExperimentPlan {
    ReadExperiment review; ///< Fixture/source/control/failure/reconciliation metadata and budgets.
    MotorControlCAN::LICHUAN_C::Tpdo1ConfigurationReview configurationReview;
    ///< Exact target, retained fixture/firmware records and the scope its transition requires.
    MotorControlCAN::LICHUAN_C::Tpdo1ConfigurationRequest request;
    ///< Explicit transition and 1800:05 literal; the default names none and refuses.
    uint8_t slot = 0; ///< Existing service slot whose current live route is the target.
    uint32_t firstAuthorizationId = 0; ///< First of 24 or 19 strictly increasing exact grants.
    uint64_t maximumAgeUs = 0;         ///< Nonzero conservative NMT/baseline/result age policy.
};
/** First terminal cause; remote partial/unknown effects remain in core and services. */
enum class PdoExperimentFailure : uint8_t {
    NONE, ///< Every service and the final exact readback succeeded, not sample qualification.
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
/** Owned acquisition and exact retained prefix, stationary until explicit release.
 * The step evidence (confirmed, first-effect and failed step, acknowledgements, uncertainty)
 * is the core context's; this record adds owner terminals, traffic and resources.
 */
struct PdoExperimentRecord {
    PdoExperimentPlan plan; ///< Exact copied admission metadata; no pointers retained.
    MotorControlCAN::LICHUAN_C::AxisReservation reservation; ///< Built from the actual held sequence.
    MotorControlCAN::CANOPEN::ObservationTime baselinePreOperational;
    ///< Frozen qualified observation after reservation and before all baseline reads.
    MotorControlCAN::Binding
        current; ///< Actual application interpretation epoch, withdrawn before TX.
    MotorControlCAN::LICHUAN_C::Tpdo1ConfigurationContext configuration;
    ///< Public typed sequence result; its scope is the plan's: ISOLATED_EXPERIMENT for the map
    ///< change, FIXTURE_QUALIFIED for the event timer. Success never promotes it.
    bool configurationPrepared = false; ///< Baseline acquired and typed candidate admitted locally.
    bool configurationMigrated = false; ///< Idle owner/adapter synchronized to next configuration.
    bool terminal = false, evidenceLost = false, tailOverflow = false;
    ///< Prefix freezes at first terminal; only tail may then change.
    PdoExperimentFailure failure = PdoExperimentFailure::NONE; ///< First terminal cause.
    BusAdmission admission = BusAdmission::ACCEPTED; ///< Meaningful for ADMISSION failure.
    uint64_t startedUs = 0, completedUs = 0, maxServiceGapUs = 0; ///< Supplied processing times.
    uint8_t attemptedCount = 0, terminalCount = 0;   ///< Prefixes within the 24 or 19 services.
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
     * @param plan Exact transition, target, nonzero records, operation ID and age; id+1 is
     * reserved for the core phase. MAP3_TO_MAP2 needs node 1 and ISOLATED_EXPERIMENT;
     * EVENT_TIMER needs node 1 or 2, FIXTURE_QUALIFIED and a RAW_0 or RAW_100 literal.
     * Queue/response budgets must be 250000 us.
     * @param nowUs Supplied nondecreasing monotonic microseconds before deadline.
     * @return ACCEPTED locally only. Waits for a current qualified Pre-operational
     * observation after reservation acquisition, then nine exact baseline reads.
     * The caller must first set the native service evidence-age policy to a
     * nonzero cap no greater than plan.maximumAgeUs and must not broaden it while
     * this sequence is active. The adapter rechecks that same cap at SDK enqueue.
     */
    EvidenceAdmission begin(const PdoExperimentPlan& plan, uint64_t nowUs) noexcept;
    /** Set the node's service evidence age to plan.maximumAgeUs, then begin.
     * The application path of `m` and `E`: begin requires that policy already in force. On
     * any refusal the node's previous age is restored, so a refused request changes nothing;
     * an accepted one keeps the age after release, for the rest of the owner's life.
     * @param plan As begin. @param nowUs Supplied nondecreasing monotonic microseconds.
     * @return As begin; INVALID also for an unknown slot or a zero age.
     */
    EvidenceAdmission beginWithServiceAge(const PdoExperimentPlan& plan, uint64_t nowUs) noexcept;
    /** Advance at most one completion and one next admission; never services owner.
     * First possible-write notice occurs immediately after successful owner admission,
     * before any owner service/handoff. After the epoch migration that precedes the readback,
     * a withdrawn TPDO1 route of the node is withdrawn once more and the readback starts only
     * after that time, so the caller can install the published readback map later.
     * requirePreOperational is enforced again by
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
    /** Match the exact already-notified pending configuration download at owner handoff.
     * @param sequence Reservation identity. @param transfer Complete pending work.
     * @param nowUs Supplied owner time. @return False for unmatched or stale prerequisites.
     */
    bool preflight(SequenceId sequence, const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                   uint64_t nowUs) const noexcept;

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
    /** After a renewed TPDO1 route withdrawal at readbackAfterUs_, the readback starts later. */
    bool readbackWait_ = false;
    uint64_t readbackAfterUs_ = 0; ///< Route withdrawal boundary the readback must follow.
    PdoExperimentFailure requestedFailure_ = PdoExperimentFailure::NONE;
    void capture(bool tail) noexcept;
    void lose(uint32_t count, bool tail) noexcept;
    void finish(PdoExperimentFailure why, uint64_t nowUs) noexcept;
    bool current() const noexcept;
    bool currentPreOperational(uint64_t nowUs,
                               MotorControlCAN::CANOPEN::ObservationTime& output) const noexcept;
};
} // namespace MotorControlCANExample
