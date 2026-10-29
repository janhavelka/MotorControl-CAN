/** @file NetworkState.h
 * @brief Copied passive evidence for the optional cooperative CANopen owner.
 * No type implies drive readiness, application health or watchdog behavior.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/canopen/Network.h>

namespace MotorControlCAN {
namespace CANOPEN {
/** Controller evidence reported by the application, independently of node state. */
enum class ControllerState : uint8_t {
    UNKNOWN, ///< No controller report.
    AVAILABLE, ///< Controller available; no node or drive readiness implied.
    BUS_OFF, ///< Physical interface unavailable.
    RECOVERING ///< Application recovery in progress; no automatic restart.
};
/** Why dependent application evidence is revoked. */
enum class NetworkInvalidation : uint8_t {
    BOOTUP, ///< Observed boot-up or explicitly reported node reset.
    BINDING, ///< Route/identity withdrawn; old SDO routes remain frozen.
    DIALECT, ///< Interpretation changed.
    CONFIGURATION, ///< Configuration/mapping/scale dependency changed.
    RECOVERY, ///< Controller/application recovery revokes old observations.
    BUS_OFF ///< Interface failure revokes current evidence.
};
/** Independent observation time; zero is valid and unknown capture is explicit. */
struct ObservationTime {
    bool present = false; ///< A checked observation exists.
    bool receivedTimeKnown = false; ///< Source-qualified capture clock is available.
    uint64_t receivedUs = 0; ///< Original capture microseconds, meaningful only when known.
    uint64_t processedUs = 0; ///< Supplied owner processing microseconds, never substituted for capture.
    Generation generation = 0; ///< Source event epoch; compare with current route before qualification.
};
/** Synchronous notification, copied before callbacks and never retained by them. */
struct NetworkInvalidationEvent {
    NetworkRoute previous; ///< Frozen route/epoch whose dependencies are being revoked.
    Generation generation = 0; ///< New epoch, or unchanged exhausted maximum.
    NetworkInvalidation cause = NetworkInvalidation::BOOTUP; ///< Actual revocation reason.
    uint64_t processedUs = 0; ///< Supplied processing boundary in monotonic microseconds.
};
/** Optional application cache invalidator. Both fields must be null or nonnull.
 * Context is borrowed for the stationary owner's lifetime. The callback must be
 * bounded, synchronous, nonreentrant and perform no I/O. It revokes actual cached
 * configuration/PDO/scale/reference/prepared evidence while preserving outcomes
 * and raw faults. Equal callback/context pairs are notified once per node event.
 */
struct NetworkObserver {
    void* context = nullptr; ///< Caller-owned dependent cache/axis context.
    void (*invalidate)(void*, const NetworkInvalidationEvent&) = nullptr; ///< Call-only copied notification.
};
/** One physical node's facts, embedded in an existing channel slot.
 * Other channels of that node share this view. A heartbeat changes only NMT and
 * liveness; it never qualifies configuration, drive state/mode or position.
 */
struct NodeSnapshot {
    NetworkRoute route; ///< Current host event epoch; not an on-wire tag.
    uint64_t boundaryUs = 0; ///< Processing boundary of configuration or last invalidation.
    bool strictBoundary = false; ///< After invalidation, capture must be strictly later.
    bool exhausted = false; ///< Epoch exhaustion permanently prevents current qualification.
    bool producerKnown = false; ///< Effective producer interval was externally established.
    uint16_t producerTimeMs = 0; ///< Effective interval, zero disabled; never inferred from cadence/EDS.
    ObservationTime heartbeat; ///< Last valid periodic heartbeat; boot-up is excluded.
    ObservationTime nmtTime; ///< Independent native NMT observation time.
    NmtState nmt = NmtState::INITIALIZING; ///< Current only if present, epoch matches and not exhausted; otherwise history.
    ObservationTime sdo; ///< Last checked matching SDO evidence, including valid rejection.
    ObservationTime emcyTime; ///< Time of latest EMCY; history can outlive the current epoch.
    NetworkEvent emcy; ///< Latest owned raw EMCY; its route preserves the originating epoch.
    uint32_t emcyCount = 0; ///< Saturating accepted EMCYs, separate from health/fault policy.
};
/** Application view of heartbeat age; producer interval is not a consumer timeout. */
enum class HeartbeatFreshness : uint8_t {
    PRODUCER_UNKNOWN, ///< No effective interval fact.
    DISABLED, ///< Effective interval is explicitly zero.
    NEVER_OBSERVED, ///< Nonzero producer interval, no qualified periodic heartbeat.
    UNQUALIFIED, ///< Only unknown-capture or invalidated evidence exists.
    FRESH, ///< Qualified capture age is within the supplied inclusive policy.
    EXPIRED ///< Capture age exceeds that policy; no drive action is inferred.
};
/** Classify copied heartbeat evidence without clocks, I/O or mutation.
 * @param[in] node Stable snapshot borrowed only during this call.
 * @param[in] nowUs Monotonic processing microseconds, at least its known observations.
 * @param[in] maximumAgeUs Explicit nonzero application age policy; not derived from producerTimeMs.
 * @param[out] output Published only on success; must not overlap node.
 * @return False for invalid/future time, zero policy or overlap; output unchanged.
 */
bool heartbeatFreshness(const NodeSnapshot& node, uint64_t nowUs, uint64_t maximumAgeUs,
                        HeartbeatFreshness& output);
/** Disposition of a copied observer record; no diagnostic is an operation result. */
enum class NetworkDisposition : uint8_t {
    ACCEPTED, ///< Checked current event reached its normal consumer.
    REJECTED, ///< Matching route with unsupported or malformed representation.
    STALE, ///< Old binding/epoch/capture cannot update current evidence.
    UNQUALIFIED, ///< Capture/ordering cannot qualify current facts.
    UNKNOWN_ROUTE ///< No configured network identifier; no discovery is attempted.
};
/** Owned history entry. Full history drops newest; state/invalidation still execute. */
struct NetworkObservation {
    NetworkRoute route; ///< Explicit ingress provenance when supplied; otherwise configured, invalid if unknown.
    Frame frame; ///< Copied raw wire representation, even when rejected.
    ObservationTime time; ///< Capture and processing times retained independently.
    NetworkDisposition disposition = NetworkDisposition::UNKNOWN_ROUTE; ///< Qualification outcome.
    bool decoded = false; ///< Makes event meaningful.
    NetworkEvent event; ///< Checked decoded fields when available; raw bytes always survive above.
};
} // namespace CANOPEN
} // namespace MotorControlCAN
