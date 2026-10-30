/** @file Pdo.h
 * @brief Pure bounded Classical PDO layouts and raw native observations.
 * No I/O, clock acquisition, allocation, configuration or motion completion.
 * All pointers are borrowed during calls only; successful outputs own their data.
 * Independent calls are reentrant; callers serialize shared input/output mutation.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/canopen/SdoTransfer.h>

namespace MotorControlCAN {
namespace CANOPEN {
/** Maximum complete byte fields in an eight-byte Classical payload. */
constexpr uint8_t PDO_ENTRIES = 8;
/** Direction is relative to the drive, independent of SDO access. */
enum class PdoDirection : uint8_t { RPDO, ///< Host command bytes received by the drive.
                                  TPDO ///< Drive-produced bytes observed by the host.
};
/** Unresolved signedness permits raw bytes only, never a guessed numeric type. */
enum class PdoSign : uint8_t { UNSIGNED, ///< Resolved unsigned native integer.
                             SIGNED, ///< Resolved two's-complement native integer.
                             RAW ///< Width known, signedness unresolved.
};
/** Dictionary meaning, without physical units, scale, drive acceptance or completion. */
enum class PdoRole : uint8_t { RAW, ///< No further interpretation is reviewed.
                             COMMANDED, ///< Commanded quantity, not actual feedback.
                             ACTUAL, ///< Dictionary actual quantity, not a qualified physical measurement.
                             ESTIMATED ///< Explicitly estimated quantity.
};
/** Reviewed passive layout fact. This grants no SDO access, RPDO TX or action. */
struct PdoObject {
    /** Construct documentary facts; validation/permissions are separate.
     * @param[in] i Native index, zero invalid.
     * @param[in] s Native subindex.
     * @param[in] w Complete width in bytes, 1..4.
     * @param[in] signValue Explicit resolved or raw-only signedness.
     * @param[in] roleValue Source-backed quantity role.
     * @param[in] receive Source includes an RPDO layout claim.
     * @param[in] transmit Source includes a TPDO layout claim.
     */
    constexpr PdoObject(uint16_t i = 0, uint8_t s = 0, uint8_t w = 0, PdoSign signValue = PdoSign::RAW,
                        PdoRole roleValue = PdoRole::RAW, bool receive = false, bool transmit = false) noexcept
        : index(i), subindex(s), widthBytes(w), sign(signValue), role(roleValue), rpdo(receive), tpdo(transmit) {}
    uint16_t index = 0; ///< Nonzero object index; no global action dispatch.
    uint8_t subindex = 0; ///< Exact source subindex.
    uint8_t widthBytes = 0; ///< Complete native byte width, 1..4; default invalid.
    PdoSign sign = PdoSign::RAW; ///< Explicit resolved or raw-only representation.
    PdoRole role = PdoRole::RAW; ///< Source-backed meaning of this field.
    bool rpdo = false; ///< Reviewed source layout includes this object in RPDO direction.
    bool tpdo = false; ///< Reviewed source layout includes this object in TPDO direction.
};
/** Call-only profile facts; no installed registry or manufacturer assumptions. */
struct PdoProfile {
    uint32_t profile = 0; ///< Nonzero identity matching the binding's selected profile.
    uint16_t sets = 0; ///< Reviewed sets in each direction, 1..512; four is not universal.
    uint8_t slots = 0; ///< Reviewed mapping entries per set, 1..8.
    const PdoObject* objects = nullptr; ///< Borrowed reviewed passive-layout facts.
    size_t objectCount = 0; ///< Number of objects, 1..256, bounding lookup work.
};
/** Template or candidate map, never evidence of a current device configuration. */
struct PdoMap {
    PdoDirection direction = PdoDirection::TPDO; ///< Native direction, not SDO access.
    uint16_t number = 0; ///< One-based PDO set within the selected profile.
    uint8_t count = 0; ///< Active entries; zero is a valid disabled template.
    uint32_t words[PDO_ENTRIES] = {}; ///< Index:bits31..16, subindex:15..8, width bits:7..0; tail must be zero.
};
/** Checked complete layout, still not current map or sample evidence. */
struct PdoLayout {
    uint32_t profile = 0; ///< Selected nonzero profile identity.
    PdoMap mapping; ///< Owned mapping words and direction.
    uint8_t payloadLength = 0; ///< Exact accumulated byte length, 0..8.
    PdoObject fields[PDO_ENTRIES]; ///< Owned resolved layout facts for active entries only.
};
/** Explicit current route candidate; generation is absent from the wire. */
struct PdoRoute {
    Binding binding; ///< Complete selected identity/configuration provenance.
    Generation generation = 0; ///< Nonzero mapping epoch, never wrap or reuse old work.
    uint32_t cobId = 0; ///< Actual configured standard data identifier, 1..0x7ff; not EDS default.
    bool extended = false; ///< Must be false in this selected service subset; common Frame still preserves extended/RTR/FD distinctions.
    PdoLayout layout; ///< Owned checked layout; binding profile must match.
};
/** Exact raw field plus optional numeric meaning, without physical scaling. */
struct PdoValue {
    PdoObject object; ///< Source-backed identity, width, sign and actual/commanded/estimated role.
    uint32_t raw = 0; ///< Complete little-endian native bits, zero-extended to U32.
    bool numericKnown = false; ///< False for unresolved signedness; raw bits remain available.
    int64_t native = 0; ///< Exact signed/unsigned native integer only when numericKnown.
};
/** Complete atomic decoded candidate; no time, liveness or operation completion. */
struct PdoSample {
    PdoRoute route; ///< Full copied mapping/binding provenance, not an on-wire tag.
    Frame frame; ///< Owned original envelope and bytes.
    PdoValue values[PDO_ENTRIES]; ///< Only layout.mapping.count values are meaningful.
};
/** Validate complete byte-field layouts against reviewed profile facts.
 * @param[in] map Borrowed candidate; zero count allowed only as disabled layout.
 * @param[in] profile Borrowed bounded facts; duplicate addresses reject.
 * @param[out] output Owned layout, unchanged on failure; no overlap with either input or object array.
 * @return OK proves layout only. INVALID_CONFIG rejects identities/limits/policy,
 * ILLEGAL_VALUE rejects overlap, unsupported native/partial widths or invalid enums;
 * all detail values zero. Count, mapping length and total Classical size must agree.
 */
Status validatePdoMap(const PdoMap& map, const PdoProfile& profile, PdoLayout& output) noexcept;
/** Revalidate an owned route/layout without a catalogue or retained pointers.
 * @param[in] route Borrowed complete route; active count must be nonzero.
 * @return OK proves representation only; INVALID_CONFIG otherwise, detail zero.
 */
Status validatePdoRoute(const PdoRoute& route) noexcept;
/** Compare complete route, binding, epochs and copied active layout facts.
 * Performs no validation, I/O, clock access or allocation. Counts beyond the
 * fixed mapping capacity compare unequal; other invalid equal fields can compare
 * equal. Inputs are borrowed only during this bounded comparison.
 * @param[in] a First complete route.
 * @param[in] b Second complete route.
 * @return True only for equal semantic route fields, ignoring inactive field storage.
 */
bool samePdoRoute(const PdoRoute& a, const PdoRoute& b) noexcept;
/** Decode one complete byte-aligned native sample atomically.
 * @param[in] frame Borrowed Classical data frame, exact identifier/format/length.
 * @param[in] route Borrowed reviewed current candidate; sample qualification is owner policy.
 * @param[out] output Owned sample, unchanged on failure; no overlap with either input.
 * @return OK proves envelope/layout/native bits only; raw-only fields are not numeric.
 * validateFrame errors propagate; other malformed envelopes are FRAME_ERROR;
 * invalid route is INVALID_CONFIG, overlap ILLEGAL_VALUE. Detail zero throughout.
 */
Status decodePdo(const Frame& frame, const PdoRoute& route, PdoSample& output) noexcept;

/** How map contents were explicitly established, independent of sample provenance. */
enum class PdoMapSource : uint8_t { TEMPLATE, ///< Unverified source template; never installs an active map.
                                  REVIEWED_CONFIGURATION, ///< Caller establishes complete actual configuration.
                                  SDO_READBACK ///< All required matched successful SDO readbacks.
};
/** Copied actual-map evidence; template construction cannot establish this fact. */
struct PdoMapEvidence {
    PdoRoute route; ///< Complete actual configured route/layout and mapping epoch.
    Generation nodeGeneration = 0; ///< Matching passive node lifecycle epoch.
    uint64_t observedUs = 0; ///< Supplied evidence acquisition microseconds, zero valid.
    uint64_t startedUs = 0; ///< Earliest relevant acquisition/admission microseconds; all readbacks must follow the lifecycle boundary.
    PdoMapSource source = PdoMapSource::TEMPLATE; ///< Explicit acquisition provenance.
    uint32_t evidenceId = 0; ///< Nonzero application evidence record, not a wire transaction ID.
    bool communicationKnown = false; ///< Actual communication parameter facts established, never defaulted from EDS.
    uint32_t cobIdParameter = 0; ///< Complete communication:01 value, including valid/format/RTR capability flags.
    uint8_t transmissionType = 0; ///< Actual communication:02 value; no SYNC or watchdog guarantee follows.
};
/** Check complete communication/mapping readbacks; never issues their uploads.
 * @param[in] results Call-only array of retained SDO completions, exact count 3+map.count:
 * comm:01 COB-ID(U32), comm:02 transmission(U8), map:00 count(U8), map:01..count(U32), in order.
 * @param[in] count Number of supplied completions; null/short/extra arrays reject.
 * @param[in] expected Explicit actual route/layout to compare, not a template declaration.
 * @param[in] nodeGeneration Current nonzero node epoch supplied by its owner.
 * @param[in] evidenceId Nonzero source record identity.
 * @param[out] output Owned evidence only after all matching successful uploads; no overlap with inputs/array.
 * @return OK establishes matching readback contents only; INVALID_CONFIG otherwise,
 * overlap ILLEGAL_VALUE, detail zero. No mapped payload target is uploaded here.
 * Caller supplies coherent readback/configuration evidence: this helper cannot
 * exclude concurrent external reconfiguration or prove non-changing read effects.
 * Every completion must carry the specified node epoch and admission time;
 * the output retains earliest admission and latest completion separately.
 */
Status pdoMapFromReadback(const SdoCompletion* results, size_t count, const PdoRoute& expected,
                         Generation nodeGeneration, uint32_t evidenceId, PdoMapEvidence& output) noexcept;
} // namespace CANOPEN
} // namespace MotorControlCAN
