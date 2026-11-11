/** @file DiscoveryScan.h
 * @brief Finite discovery through the ordinary ProbeApp and its single CAN owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "ProbeApp.h"
#include <MotorControlCAN/Discovery.h>

namespace MotorControlCANExample {
/** Explicit current physical interface; discovery never switches its bitrate/domain. */
struct DiscoveryInterface {
    MotorControlCAN::Protocol protocol = MotorControlCAN::Protocol::CANOPEN; ///< Selected domain.
    uint32_t bus = 0;     ///< Physical-interface identity used by every target.
    uint32_t bitrate = 0; ///< Established current bits/second; zero is unknown and refuses.
};
/** Explicit acquisition intent; identity is never an implicit probe continuation. */
enum class DiscoveryMode : uint8_t {
    PASSIVE, ///< Observe configured network routes without requesting a service.
    PROBE,   ///< Explicit single-object presence upload per selected target.
    IDENTITY ///< Explicit nine-field raw identity acquisition per selected target.
};
/** Discovery finding, separate from read outcome, drive state and application health. */
enum class DiscoveryOutcome : uint8_t {
    PENDING,                ///< No terminal classification yet.
    PRESENCE,               ///< Valid responder or configured passive evidence, no exact identity.
    GENERIC_IDENTITY,       ///< Raw identity read; no automatic model or firmware qualification.
    NO_RESPONSE,            ///< No eligible evidence before the response or passive cutoff.
    PROTOCOL_REJECTED,      ///< Valid native abort proves a responder but rejects this read.
    UNSUPPORTED_FIRMWARE,   ///< Raw revision/firmware differs from explicit caller support policy.
    CONTRADICTORY_IDENTITY, ///< Raw identity differs from explicit caller expectations.
    SERVICE_ERROR,          ///< Other retained native service failure.
    LOCAL_REJECTION,        ///< Preparation or admission refused without a completed read.
    INVALIDATED,            ///< Required route, node or controller provenance changed.
    CANCELLED,              ///< Explicit local cancellation; no stop or retry implied.
    DEADLINE                ///< Overall immutable processing cutoff reached.
};
/** What the evidence establishes; no value identifies a motor or feedback capability. */
enum class DiscoveryConfidence : uint8_t {
    NONE,             ///< No qualified positive finding.
    RESPONDER_ONLY,   ///< Presence evidence without identity interpretation.
    RAW_IDENTITY,     ///< Completed raw identity acquisition.
    EXPECTATION_MATCH ///< Caller-supplied identity keys matched; still no automatic binding.
};
/** Optional caller evidence, never guessed from EDS defaults or numeric model-looking values. */
struct DiscoveryExpectation {
    bool identityKnown =
        false;             ///< Compare the three raw identity keys below when explicitly supplied.
    uint32_t vendorId = 0; ///< Expected vendor, not a library identification rule.
    uint32_t productCode = 0;   ///< Expected product code.
    uint16_t modelCode = 0;     ///< Expected vendor model raw value.
    bool firmwareKnown = false; ///< Caller supports only the revision/firmware pair below.
    uint32_t revision =
        0; ///< Explicit supported revision; mismatch is unsupported for this policy.
    uint16_t firmwareCode = 0; ///< Explicit supported vendor firmware code.
};
/** One exact already configured candidate; no route or model is automatically bound. */
struct DiscoveryTarget {
    MotorControlCAN::LICHUAN_C::ReadTarget target; ///< Full route/node/configuration provenance.
    DiscoveryExpectation expected; ///< Optional explicit interpretation constraints.
};
/** Frozen finite plan. The target limit is the current owner capacity, not CANopen node capacity. */
struct DiscoveryPlan {
    enum {
        CAPACITY = MotorControlCAN::CANOPEN::SdoClient::CHANNELS ///< Existing permanent slots.
    };
    DiscoveryTarget targets[CAPACITY]; ///< Copied input; unused elements have no meaning.
    uint8_t count = 0;                 ///< 1..CAPACITY, unique physical channels and node bindings.
    MotorControlCAN::DriveProfile profile =
        MotorControlCAN::DriveProfile::LICHUAN_C; ///< Implemented family.
    DiscoveryInterface
        interface; ///< Requested domain/interface/current bitrate; must match actual.
    DiscoveryMode mode =
        DiscoveryMode::PASSIVE; ///< No traffic unless PROBE/IDENTITY explicitly selected.
    MotorControlCAN::LICHUAN_C::ReadOptions
        options; ///< Queue/response/width policy for active reads.
    uint8_t maximumServices =
        0; ///< Finite admitted-service budget; identity needs nine per target.
    uint64_t maximumAgeUs =
        0; ///< Nonzero evidence age policy, independent of heartbeat production.
    MotorControlCANOwner::ServiceBudget
        serviceBudget; ///< Explicit bounded owner work per turn, zero allowed.
};
/** Retained copied evidence. Getters on read still require current binding/time; this is history. */
struct DiscoveryFinding {
    DiscoveryTarget requested; ///< Original exact selection and caller expectations.
    uint8_t slot = 0; ///< Permanent configured channel slot used; never freed by this object.
    /** Final batch classification. Epoch loss withdraws a positive finding before publication;
     * negative policy classifications and the native read outcome remain retained. */
    DiscoveryOutcome outcome = DiscoveryOutcome::PENDING;
    DiscoveryConfidence confidence = DiscoveryConfidence::NONE; ///< Evidenced scope only.
    MotorControlCAN::LICHUAN_C::ReadContext
        read; ///< Owned raw values, partial failure, abort and effect history.
    MotorControlCAN::CANOPEN::ObservationTime
        heartbeat; ///< Original checked heartbeat bounds, not scan time.
    MotorControlCAN::CANOPEN::ObservationTime
        bootup; ///< Boot-up observed during this scan, independently retained.
    MotorControlCAN::CANOPEN::ObservationTime pdo; ///< Qualified configured TPDO evidence only.
    MotorControlCAN::Generation mappingGeneration =
        0; ///< Current sample's map epoch when pdo.present.
    bool suspectedDuplicate =
        false; ///< Conflicting ordered-state evidence; never proves a collision.
    bool unresolved =
        false; ///< Channel is quarantined at batch completion; result release cannot clear it.
    bool bindingCurrent =
        false;                ///< Exact channel/node/controller epochs match at batch completion.
    uint64_t completedUs = 0; ///< Supplied local terminal time, zero valid.
};
/** Retained finite resource accounting; selecting existing slots allocates no new channel. */
struct DiscoverySummary {
    uint8_t targets = 0;   ///< Copied target count.
    uint8_t attempted = 0; ///< Active reads prepared, or passive targets inspected.
    uint8_t completed = 0; ///< Findings settled, including unattempted cancellation/deadline rows.
    uint8_t servicesAdmitted = 0; ///< Owner-admitted fields, never queue waiting or passive frames.
    uint8_t occupiedSlotsAtStart = 0; ///< Permanent configured slots already consumed.
    uint8_t freeSlotsAtStart =
        0; ///< Remaining unconfigured slots; scan never allocates or reuses one.
    uint8_t unresolvedSlots = 0; ///< Selected unresolved slots at completion.
    uint32_t passiveHistoryDrops =
        0; ///< Copied network-history overflow during scan; normal routing still ran.
    bool cancelled = false;        ///< Explicit local cancellation, no stop or retry.
    bool deadlineExpired = false;  ///< Overall processing cutoff reached.
    bool interfaceChanged = false; ///< Caller supplied a different bitrate/domain/bus during work.
};
/** Stationary application scan. Borrowed app/owner/services must be the same existing owner graph.
 * It owns no controller/receiver/node registry and sends no downloads, NMT or drain markers.
 * The app is exclusively borrowed during a scan; ordinary owner passive dispatch continues.
 */
class DiscoveryScan {
  public:
    /** @param app Existing shared app. @param owner Its sole owner. @param services Its native storage. */
    DiscoveryScan(ProbeApp& app, CanBusOwner& owner,
                  MotorControlCAN::CANOPEN::SdoClient& services) noexcept;
    DiscoveryScan(const DiscoveryScan&) = delete; ///< Stationary references and retained results.
    DiscoveryScan&
    operator=(const DiscoveryScan&) = delete; ///< No transfer of live scan ownership.
    /** Copy/validate the whole plan without preparing or transmitting a read.
     * @param plan Explicit configured target set and budgets.
     * @param actual Actual established interface configuration; must equal plan.interface.
     * @param nowUs Nondecreasing monotonic supplied us, zero valid.
     * @param deadlineUs Immutable future overall cutoff, bounded by INT64_MAX.
     * @return False unchanged for retained results, stale targets, unsupported domain, capacity,
     * bitrate mismatch, invalid/exhausted budgets or a nonempty app read. No route is configured.
     */
    bool begin(const DiscoveryPlan& plan, const DiscoveryInterface& actual, uint64_t nowUs,
               uint64_t deadlineUs) noexcept;
    /** Progress the existing app/owner with frozen budgets, consuming at most one service result.
     * @param actual Current interface facts; change cancels work before owner progression.
     * @param nowUs Nondecreasing processing us. now >= deadlineUs expires even earlier RX.
     * @return False for invalid time or failed owner progression. Results stay until release.
     */
    bool service(const DiscoveryInterface& actual, uint64_t nowUs) noexcept;
    /** Advance findings after one externally performed app.service turn, without servicing
     * the owner, platform or clock again. The caller exclusively owns that progression,
     * uses the plan's frozen serviceBudget and cancels before it if interface facts changed.
     * @param actual Current established interface facts; changes invalidate the scan.
     * @param nowUs Same supplied processing time as the completed application turn.
     * @return False for invalid time or settlement failure. Consumes at most one terminal;
     * deadlines, retained findings and admission accounting match service().
     */
    bool advance(const DiscoveryInterface& actual, uint64_t nowUs) noexcept;
    /** @param nowUs Nondecreasing supplied us. @return True records cancellation and withdraws
     * current work without new traffic; subsequent service settles pending evidence. */
    bool cancel(uint64_t nowUs) noexcept;
    /** @return True while scan is active, including cancellation settlement. */
    bool active() const noexcept { return active_; }
    /** @return True only after all selected findings have settled. */
    bool complete() const noexcept { return occupied_ && !active_; }
    /** @param index Selected target ordinal. @return Non-consuming retained result until release,
     * null while active, before begin or for an invalid index. No clock or I/O. */
    const DiscoveryFinding* result(uint8_t index) const noexcept;
    /** @return Borrowed bounded accounting, immutable once complete until release. */
    const DiscoverySummary& summary() const noexcept { return summary_; }
    /** @return False for active/absent scan, otherwise releases only local findings. Quarantine,
     * owner history outside this scan, node generations and configuration are preserved. */
    bool release() noexcept;

  private:
    ProbeApp& app_;
    CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    DiscoveryPlan plan_;
    DiscoveryFinding findings_[DiscoveryPlan::CAPACITY];
    DiscoverySummary summary_;
    uint32_t serverSessions_[DiscoveryPlan::CAPACITY] = {};
    uint32_t controllerEpoch_ = 0;
    uint32_t historyDropsAtStart_ = 0;
    MotorControlCANOwner::RequestId countedRequest_; ///< Latest field counted at advancement.
    uint64_t startedUs_ = 0, lastUs_ = 0, deadlineUs_ = 0;
    uint8_t cursor_ = 0;
    bool occupied_ = false, active_ = false, startedRead_ = false, stopping_ = false;
    DiscoveryOutcome stopReason_ = DiscoveryOutcome::CANCELLED;
    void passive(uint64_t nowUs) noexcept;
    void finish(uint8_t index, DiscoveryOutcome outcome, uint64_t nowUs) noexcept;
    void stop(DiscoveryOutcome reason, uint64_t nowUs) noexcept;
    void classify(DiscoveryFinding& finding, uint64_t nowUs) noexcept;
};
} // namespace MotorControlCANExample
