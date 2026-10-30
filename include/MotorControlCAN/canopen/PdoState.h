/** @file PdoState.h
 * @brief Application-owned actual-map and independently qualified sample evidence.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/canopen/Pdo.h>
#include <MotorControlCAN/canopen/Network.h>

namespace MotorControlCAN {
namespace CANOPEN {
/** Explicit external evidence resolving old producer and receive-queue ambiguity. */
enum class PdoSampleBasis : uint8_t {
    UNQUALIFIED, ///< Map readback, timestamps, host epochs and quiet intervals alone prove nothing.
    PRODUCER_AND_INGRESS_RETIRED, ///< Applicable evidence excludes old producer/controller/application queued bytes.
    DISTINCT_VERIFIED_ROUTE ///< Independently verified non-reused wire route; a free host slot is insufficient.
};
/** Call-only evidence attestation; stored copies never borrow a controller buffer. */
struct PdoQualification {
    PdoRoute route; ///< Exact current binding/map/identifier/layout scope of the evidence.
    Generation nodeGeneration = 0; ///< Exact current node lifecycle epoch.
    PdoSampleBasis basis = PdoSampleBasis::UNQUALIFIED; ///< No automatic inference or unsupported generic counter marker.
    uint64_t establishedUs = 0; ///< Supplied monotonic processing boundary, not proof of remote retirement itself.
    uint32_t evidenceId = 0; ///< Nonzero application record proving the selected basis and all queue domains.
};
/** One complete native observation; never establishes scaled physical position or completion. */
struct PdoObservation {
    PdoSample sample; ///< Owned decoded field values and source frame/mapping.
    ObservationTime time; ///< Qualified capture/processing microseconds; generation is the mapping epoch, not a wire tag.
    uint32_t evidenceId = 0; ///< Sample-provenance evidence in force at acceptance, not a wire tag.
};
/** Borrowed synchronous callbacks; retained consumers must copy arguments.
 * Both callbacks run bounded/nonreentrant in the sole cooperative receive owner,
 * without I/O or clocks. A null context requires both callbacks null. A nonnull
 * context requires both callbacks; map/sample invalidation cannot be omitted.
 */
struct PdoObserver {
    void* context = nullptr; ///< Stationary application cache/consumer context.
    void (*sample)(void*, const PdoObservation&) = nullptr; ///< Complete owned candidate, borrowed during callback only.
    void (*invalidate)(void*, const PdoRoute&) = nullptr; ///< Previous route whose dependent values are revoked, call-only.
};
/** Map confirmation and sample qualification are independent retained facts. */
struct PdoRecord {
    PdoMapEvidence map; ///< Last actual-map evidence, retained as history after invalidation.
    bool confirmed = false; ///< Current map contents/configuration are explicitly established.
    bool qualified = false; ///< Current external producer/ingress evidence qualifies future captured samples.
    PdoQualification qualification; ///< Provenance retained even after its validity is revoked.
    uint64_t boundaryUs = 0; ///< Latest map/lifecycle/sample invalidation processing boundary.
    bool samplePresent = false; ///< At least one fully qualified sample exists; may now be historical.
    PdoObservation observation; ///< Last complete accepted sample; invalid/malformed frames never overwrite it.
};
/** Independent sample age, with no heartbeat/SDO/EMCY renewal. */
enum class PdoFreshness : uint8_t { NO_MAP, ///< Actual mapping not currently confirmed.
                                  UNQUALIFIED, ///< Route sample provenance is unresolved.
                                  NEVER_OBSERVED, ///< Current qualification, no sample in this map/provenance epoch.
                                  FRESH, ///< Capture age within explicit inclusive policy.
                                  EXPIRED ///< Qualified capture older than caller policy.
};
/** Classify a copied PDO record; acquires no clock and performs no I/O.
 * @param[in] record Call-only stable retained record.
 * @param[in] nowUs Supplied nonwrapping microseconds, at least its retained processing times.
 * @param[in] maximumAgeUs Nonzero inclusive capture-age policy, independent of producer timing.
 * @param[out] output Published atomically; no overlap with record.
 * @return False for invalid/future time, zero policy or overlap; output unchanged.
 */
bool pdoFreshness(const PdoRecord& record, uint64_t nowUs, uint64_t maximumAgeUs, PdoFreshness& output);
} // namespace CANOPEN
} // namespace MotorControlCAN
