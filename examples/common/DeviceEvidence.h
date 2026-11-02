/** @file DeviceEvidence.h
 * @brief Finite typed-read acquisition and owned evidence through the ordinary application.
 * SPDX-License-Identifier: MIT
 * Application code only. No I/O, clock, allocation, parser, retry or device writes.
 * One cooperative caller owns the app, owner diagnostic drain and this harness.
 */
#pragma once
#include "ProbeApp.h"

namespace MotorControlCANExample {
/** Provenance fixed at construction, not a per-request permission switch. */
enum class EvidenceEnvironment : uint8_t {
    DEVICE,   ///< Physical controller; ordinary acquisition requires reviewed read effects.
    HOST_FAKE ///< Only an offline fake transport; never qualifies a physical capability.
};
/** Local request disposition; no value claims device success or safe read effects. */
enum class EvidenceAdmission : uint8_t {
    ACCEPTED,               ///< Exact finite plan copied; service is still explicit.
    INVALID,                ///< Invalid metadata, time, selection or review; no work admitted.
    BUSY,                   ///< An acquisition/result is retained or the application is occupied.
    READ_EFFECT_UNQUALIFIED ///< Q20: no ordinary non-changing acquisition permission.
};
/** Fixed finite request. Strings are required nonempty, NUL-terminated references.
 * The caller retains the referenced fixture/authorization/observation documents.
 * A reference records an assertion; the library cannot authenticate its contents.
 */
struct AcquisitionPlan {
    uint32_t id = 0;         ///< Nonzero strictly increasing acquisition ID per harness lifetime.
    char fixtureId[48] = {}; ///< Exact fixture record, including actual controller/wiring facts.
    char softwareRevision[41] =
        {};                 ///< Forty hexadecimal Git digits; not running-image attestation.
    char firmware[48] = {}; ///< Known label/code or explicit unknown-before-identity description.
    char dialect[48] = {};  ///< Selected interpretation or explicit raw/unresolved dialect.
    char authorization[64] = {}; ///< Finite procedure authorization, not general bench access.
    char observations[96] = {};  ///< Required independent observations and their retained record.
    MotorControlCAN::LICHUAN_C::ReadTarget target; ///< Exact preselected binding/route/node epoch.
    MotorControlCAN::LICHUAN_C::ReadKind kind = MotorControlCAN::LICHUAN_C::ReadKind::IDENTITY;
    ///< Only IDENTITY, CONFIG or explicit STATE blocks; no arbitrary object selection.
    uint8_t stateMask = 0;   ///< Nonzero StateBlock bits for STATE; zero for other kinds.
    uint64_t deadlineUs = 0; ///< Exclusive immutable cutoff in supplied monotonic microseconds.
    MotorControlCAN::LICHUAN_C::ReadOptions options; ///< Explicit queue/response policy, no retry.
};
/** Independently reviewed, exact unqualified-read experiment; not a write executor.
 * The plan is embedded so authorization cannot be applied to a different request.
 * Each required reference documents the actual fixture, bounds and independent
 * provisions; nonempty strings alone do not establish those external facts.
 */
struct ReadExperiment {
    AcquisitionPlan plan;       ///< Entire reviewed plan, copied unchanged at explicit admission.
    char sourceReview[96] = {}; ///< Applicable original pages/EDS and remaining contradictions.
    char independentReview[64] = {};  ///< Reviewer and reviewed procedure revision.
    char fixtureReview[64] = {};      ///< Established electrical/mechanical/session facts.
    char independentControl[96] = {}; ///< Stop/control independent of disputed firmware semantics.
    char failurePlan[96] = {}; ///< Stop further acquisition; preserve possible effects, no replay.
    char reconciliation[96] = {}; ///< Exact evidence needed after interruption/unknown effects.
};
/** Small owner traffic copy; terminals are retained separately without duplicating them here. */
struct EvidenceTraffic {
    DiagnosticKind kind =
        DiagnosticKind::RX;       ///< Wire/controller/recovery event; terminals stored separately.
    MotorControlCAN::Frame frame; ///< Exact RX/TX envelope; meaningful for those two kinds.
    TransportTxId txId; ///< Local TX/report correlation, absent from CAN wire.
    uint32_t controllerEpoch = 0;           ///< Original ingress/handoff epoch.
    uint64_t processedUs = 0;               ///< Supplied owner processing microseconds.
    uint64_t receivedUs = 0;                ///< RX capture only when receivedTimeKnown.
    bool receivedTimeKnown = false;         ///< No inferred hardware timestamp.
    bool receiveBoundsKnown = false;        ///< Conservative completed-ingress interval, not exact capture.
    uint64_t receivedEarliestUs = 0;         ///< Last certified empty ingress boundary.
    uint64_t receivedLatestUs = 0;           ///< ISR observation upper bound.
    bool accepted = false; ///< Owner dispatch, enqueue or actual TX according to kind.
    MotorControlCAN::CANOPEN::ControllerState controller =
        MotorControlCAN::CANOPEN::ControllerState::UNKNOWN; ///< CONTROLLER only, not drive state.
    RecoveryOutcome recovery =
        RecoveryOutcome::FAILED; ///< RECOVERY only, not remote qualification.
};
enum {
    EVIDENCE_TRAFFIC = 64 ///< Fixed acquisition traffic capacity; excess is counted, never hidden.
};
/** Owned historical record. Only entries below counts are valid, including after reuse.
 * Raw success is not physical qualification, model detection or motion completion.
 */
struct AcquisitionRecord {
    AcquisitionPlan plan; ///< Immutable requested fixture/intent/bounds.
    EvidenceEnvironment environment = EvidenceEnvironment::DEVICE; ///< Fixed provenance.
    bool experimental =
        false; ///< Explicit unknown-effect read experiment, not ordinary acquisition.
    ReadExperiment
        review; ///< Meaningful only when experimental; exact retained approval assertions.
    uint64_t startedUs = 0;   ///< Admission time; zero valid.
    uint64_t completedUs = 0; ///< First terminal delivery time; meaningful when terminal.
    bool terminal = false;    ///< Immutable record until release after terminal publication.
    bool interrupted = false; ///< Local interruption requested; earlier terminal evidence can win.
    bool evidenceLost =
        false; ///< Trace overwrite/counter reset/capacity loss; no complete-evidence claim.
    uint32_t droppedTraffic = 0; ///< Saturating minimum loss count; unknown reset adds one.
    uint64_t maxServiceGapUs =
        0;              ///< Largest collector-call interval during acquisition; not a device bound.
    Stats initialStats; ///< Cumulative owner counters at admission, not reset by acquisition.
    Stats
        finalStats; ///< Latest cumulative counters, frozen at terminal; rxLost excludes retained rejects.
    OwnerResources initialResources; ///< Fixed resource snapshot after app reservation.
    OwnerResources
        finalResources;        ///< Latest resource snapshot; not a measured peak or stack budget.
    uint8_t intendedCount = 0; ///< Exact initial typed field inventory, at most READ_FIELDS.
    MotorControlCAN::LICHUAN_C::ReadDescriptor intended[MotorControlCAN::LICHUAN_C::READ_FIELDS];
    ///< Copied public typed descriptors, never a writable object manifest.
    MotorControlCAN::LICHUAN_C::ReadContext
        read; ///< Owned raw values, validity and final native evidence.
    uint8_t terminalCount =
        0; ///< Copied owner terminals, at most READ_FIELDS, in consumption order.
    Completion
        terminals[MotorControlCAN::LICHUAN_C::READ_FIELDS]; ///< Retains all failed/prefix effects.
    uint8_t trafficCount = 0; ///< Valid traffic prefix, at most EVIDENCE_TRAFFIC.
    EvidenceTraffic
        traffic[EVIDENCE_TRAFFIC]; ///< Exact diagnostic copies; source owner retains RX authority.
};
/** Bounded evidence collector around one existing application/owner, never a second service engine.
 * Do not advance/release/request the borrowed app or drain diagnostics independently
 * or clear owner statistics during an acquisition. Service the ordinary application
 * first, then this collector. Resource snapshots do not replace board measurements.
 */
class DeviceEvidence {
  public:
    /** Borrow stationary resources; construction performs no traffic or selection.
     * @param app Existing concrete application, outlives this harness.
     * @param owner That application's sole receive owner, outlives this harness.
     * @param environment DEVICE by default; HOST_FAKE is restricted to offline fake controllers.
     */
    DeviceEvidence(ProbeApp& app, CanBusOwner& owner,
                   EvidenceEnvironment environment = EvidenceEnvironment::DEVICE) noexcept;
    DeviceEvidence(const DeviceEvidence&) = delete; ///< No copying live ownership/history.
    DeviceEvidence& operator=(const DeviceEvidence&) = delete; ///< Stationary storage.
    /** Explicit ordinary acquisition; DEVICE currently rejects Q20 before app work.
     * @param plan Borrowed only during call; target must equal the application's explicit selection.
     * @param nowUs Nondecreasing nonwrapping microseconds, zero valid, before deadline.
     * @return Atomic local admission; no record/selection/traffic change on rejection.
     */
    EvidenceAdmission request(const AcquisitionPlan& plan, uint64_t nowUs) noexcept;
    /** Explicit finite unknown-effect read experiment through the same typed app.
     * @param review Exact plan and required reviewed fixture/control/failure references.
     * @param nowUs Same monotonic time domain; no implicit enable, save, reset or motion.
     * @return Local admission only. Does not establish the truth of external assertions,
     * change generated read policy, qualify firmware or close Q20.
     */
    EvidenceAdmission requestExperiment(const ReadExperiment& review, uint64_t nowUs) noexcept;
    /** Copy diagnostic traffic and at most one terminal, then advance the ordinary typed read.
     * @param nowUs Same supplied clock, after the ordinary application/owner service turn.
     * @return False unchanged for backward time; false on unexpected app advancement failure
     * retains the owner terminal. Trace loss interrupts further acquisition without replay.
     * Never receives, sends, polls hardware or services the owner a second time.
     */
    bool service(uint64_t nowUs) noexcept;
    /** Interrupt this read through owner cancellation before consuming its evidence.
     * @param nowUs Nondecreasing supplied time; reached deadlines/earlier terminals win.
     * @return True for an active acquisition withdrawn/settled, never a physical stop.
     */
    bool interrupt(uint64_t nowUs) noexcept;
    /** @return Borrowed historical record or null before admission/after release. No I/O. */
    const AcquisitionRecord* record() const noexcept { return occupied_ ? &record_ : nullptr; }
    /** Explicitly release a terminal acquisition and its app reservation/history.
     * @return False while pending; true does not clear quarantine, faults or unknown effects.
     */
    bool release() noexcept;

  private:
    ProbeApp& app_;
    CanBusOwner& owner_;
    EvidenceEnvironment environment_;
    AcquisitionRecord record_;
    uint64_t lastUs_ = 0;
    uint32_t lastId_ = 0, traceOverwritten_ = 0, rxLost_ = 0;
    bool occupied_ = false;
    EvidenceAdmission start(const AcquisitionPlan&, const ReadExperiment*, uint64_t) noexcept;
    void capture() noexcept;
    void lost(uint32_t count) noexcept;
};
} // namespace MotorControlCANExample
