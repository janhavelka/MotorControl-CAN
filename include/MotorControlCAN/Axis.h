/** @file Axis.h
 * @brief Pure host coordinate configuration and checked target preparation.
 * No function performs I/O, reads a clock, allocates, or changes a drive counter.
 * Inputs are borrowed during the call only. Callers serialize access to shared
 * configuration/evidence. Explicit approximate paths require IEEE round-to-nearest,
 * may raise ordinary floating exception flags, and never change the rounding mode.
 * Exact integer paths have no floating-environment precondition. Outputs remain unchanged on failure except the
 * explicitly fail-closed invalidateAxisReference operation.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Endpoint.h>
#include <MotorControlCAN/Units.h>

namespace MotorControlCAN {
/** Coordinate interpretation, independent of the endpoint route. */
enum class CoordinateFrame : uint8_t {
    NATIVE, ///< Command increments with no host-origin or polarity adjustment.
    MOTOR, ///< Mechanical motor coordinates relative to the host origin.
    LOAD ///< Mechanical load coordinates with explicit gearing/travel.
};
/** Selected position witness; no automatic fallback between these bases. */
enum class RelativeBasis : uint8_t {
    ACTUAL, ///< Qualified observed command-coordinate relation.
    COMMANDED, ///< Qualified commanded position, not necessarily actual position.
    QUEUED ///< Qualified endpoint of previously queued work.
};
/** Absolute requests may opt into a required position witness. */
enum class ReferencePolicy : uint8_t {
    OPTIONAL, ///< Absolute unwrapped arithmetic needs no witness.
    REQUIRED ///< Require a fresh matching native-position witness.
};
/** Physical angular direction; command polarity is applied separately. */
enum class AnglePath : uint8_t {
    POSITIVE, ///< Select increasing physical orientation.
    NEGATIVE, ///< Select decreasing physical orientation.
    SHORTEST ///< Select shortest displacement, with an explicit half-turn policy.
};
/** Equal shortest-path distances never choose a direction implicitly. */
enum class HalfTurnTie : uint8_t {
    REJECT, ///< Reject an exactly ambiguous half turn.
    POSITIVE, ///< Select positive physical direction.
    NEGATIVE ///< Select negative physical direction.
};
/** Explicit native quantization; profiles still check actual wire widths. */
enum class NativeRepresentation : uint8_t {
    INTEGER, ///< Checked signed int64 command increments/rate.
    BINARY32 ///< IEEE binary32, nearest-even with measured error.
};
/** Why the application revokes position confidence. No event is inferred. */
enum class ReferenceLoss : uint8_t {
    POSITION, ///< Uncertain movement/release: preserve the known coordinate origins.
    COORDINATES ///< Reboot/rebinding/dialect/mapping/scale/counter clear: forget origins too.
};
/** Independent origin relation being established with qualified evidence. */
enum class AxisOrigin : uint8_t {
    MECHANICAL, ///< Host motor/load zero in command coordinates.
    ENCODER ///< Selected encoder zero in command coordinates; requires encoder scale.
};
/** Function-specific Status::detail; unit failures add UNIT_ERROR_BASE. */
enum class AxisError : int32_t {
    NONE, ///< No error.
    INVALID_ARGUMENT, ///< Malformed policy, representation, bounds or allowance.
    INVALID_TARGET, ///< Invalid binding/configuration.
    STALE_GENERATION, ///< Binding/configuration/reference epoch differs.
    STALE_REFERENCE, ///< Absent provenance, future observation or expired age.
    NOT_STATIONARY, ///< Configuration/origin change needs idle stationary evidence.
    MISSING_ORIGIN, ///< Requested absolute coordinate zero is unknown.
    MISSING_REFERENCE, ///< Required native relation/basis is unavailable.
    UNSUPPORTED_BASIS, ///< Selected relative basis is not admitted by policy.
    ARITHMETIC_OVERFLOW, ///< Fixed exact storage or checked addition overflow.
    LIMIT, ///< Requested or effective endpoint exceeds inclusive limits.
    AMBIGUOUS, ///< Approximation/rounding cannot resolve a path or target.
    GENERATION_EXHAUSTED, ///< Epoch cannot advance without reuse; fail closed.
    CLOCK_ERROR, ///< Supplied time moved backwards or exceeds the time domain.
    UNIT_ERROR_BASE = 100 ///< Preserve UnitError plus this offset and original Err/msg.
};
/** Host interpretation only. Default binding is invalid. Native/soft limits are
 * inclusive absolute command coordinates, never relative displacement limits.
 * Initial construction is caller-supplied evidence; subsequent changes use the
 * checked mutators. No full-step geometry or encoder resolution is inferred. */
struct AxisConfig {
    Binding binding; ///< Logical/route identity and configuration epoch; all nonzero where required.
    UnitConfig units; ///< Independent command, feedback, gear and travel facts.
    Generation referenceGeneration = 1; ///< Nonzero position-confidence epoch; never reused.
    bool exhausted = false; ///< Permanent fail-closed latch after invalidation exhausts an epoch.
    int64_t nativeMinimum = INT64_MIN; ///< Inclusive command-coordinate minimum.
    int64_t nativeMaximum = INT64_MAX; ///< Inclusive command-coordinate maximum.
    uint8_t supportedRelativeBases = 0; ///< Reviewed policy bits: actual=1, commanded=2, queued=4.
    bool originKnown = false; ///< Qualifies mechanical host origin below.
    int64_t originNative = 0; ///< Command coordinate at motor/load host zero.
    ScaleSource originSource = ScaleSource::UNKNOWN; ///< Host-origin provenance when known.
    bool encoderOriginKnown = false; ///< Qualifies independent feedback-to-command zero below.
    int64_t encoderOriginNative = 0; ///< Command coordinate corresponding to selected encoder zero.
    ScaleSource encoderOriginSource = ScaleSource::UNKNOWN; ///< Feedback-origin provenance when known.
    bool softLimitsKnown = false; ///< Qualifies the following absolute coordinate interval.
    int64_t softMinimum = 0; ///< Inclusive software minimum within native limits.
    int64_t softMaximum = 0; ///< Inclusive software maximum within native limits.
    NativeRepresentation positionRepresentation = NativeRepresentation::INTEGER; ///< Selected pure position format.
    NativeRepresentation velocityRepresentation = NativeRepresentation::INTEGER; ///< Selected pure rate format.
    TimeUnit nativeVelocityTime = TimeUnit::SECOND; ///< Native rate is command increments per this time unit.
};
/** Caller-qualified position evidence. Times are monotonic microseconds in one
 * application domain, 0..INT64_MAX (no wrapping). nowUs may advance; observedUs
 * advances only with a new qualified position observation. This is not motion
 * completion evidence. No timestamp or confidence is synthesized by this API. */
struct AxisReference {
    Binding binding; ///< Full matching route and configuration provenance.
    Generation referenceGeneration = 0; ///< Must match the current confidence epoch.
    bool idle = false; ///< Qualified application admission/quiescence fact.
    bool stationary = false; ///< Qualified stationary fact; not inferred from zero displacement.
    bool nativeKnown = false; ///< Exact command-coordinate relation is established.
    int64_t nativePosition = 0; ///< Exact selected-basis command position, not raw encoder counts.
    RelativeBasis basis = RelativeBasis::ACTUAL; ///< Position witness basis; never substituted.
    ScaleSource source = ScaleSource::UNKNOWN; ///< Qualified provenance, not a lifecycle state.
    uint64_t observedUs = 0; ///< Original position observation time.
    uint64_t nowUs = 0; ///< Supplied preparation/reconfiguration time.
    uint64_t maximumAgeUs = 0; ///< Inclusive maximum age; zero disables expiry, never future checks.
};
/** Exact position intent. Absolute MOTOR/LOAD requests add the appropriate
 * origin before quantization; relative requests quantize displacement first.
 * Wrapped orientation accepts exact turns/degrees, or explicitly approximated
 * radians. Relative and wrapped requests always require the chosen basis.
 * Exact zero radians follows exact arithmetic after all scale validation. */
struct PositionRequest {
    Rational value; ///< Exact requested quantity; denominator must be positive.
    PositionUnit unit = PositionUnit::STEPS; ///< Quantity dimension.
    CoordinateFrame frame = CoordinateFrame::NATIVE; ///< Native/motor/load interpretation.
    bool relative = false; ///< True requests displacement from the explicitly selected witness.
    bool wrapped = false; ///< True selects an absolute orientation path, retaining current turns.
    AnglePath path = AnglePath::SHORTEST; ///< Physical path when wrapped.
    HalfTurnTie tie = HalfTurnTie::REJECT; ///< Exact half-turn policy.
    RelativeBasis basis = RelativeBasis::ACTUAL; ///< Required witness basis when used.
    ReferencePolicy referencePolicy = ReferencePolicy::OPTIONAL; ///< Absolute witness requirement.
    Generation configurationGeneration = 0; ///< Must match binding.configurationGeneration.
    Rounding rounding = Rounding::EXACT; ///< Integer policy; binary32 always uses nearest-even.
    Rational maximumQuantizationError; ///< Nonnegative native-increment allowance, including approximation.
    bool approximate = false; ///< Explicit permission for nonzero radians conversion.
    double maximumApproximationError = 0; ///< Positive finite native-increment bound for radians.
};
/** Prepared arithmetic, not drive permission or completion. Integer fields are
 * active only for INTEGER; float fields only for BINARY32. requestedNative is
 * the exact endpoint for exactArithmetic, otherwise only an inactive default.
 * A used reference is frozen for revalidation without renewing its observation. */
struct PreparedTarget {
    PositionRequest requested; ///< Original exact intent and policies.
    Binding binding; ///< Frozen configuration/route provenance.
    Generation referenceGeneration = 0; ///< Frozen confidence epoch; dependency only when referenceUsed.
    NativeQuantity requestedNative; ///< Exact endpoint, including origin or relative basis.
    NativeRepresentation representation = NativeRepresentation::INTEGER; ///< Active output format.
    int64_t effectiveNative = 0; ///< Integer absolute target, or relative displacement.
    int64_t endpointNative = 0; ///< Integer endpoint; always known for INTEGER.
    float effectiveFloat = 0; ///< Binary32 absolute target, or relative displacement.
    double endpointFloat = 0; ///< Sum of binary32 relative displacement and exact basis; display only.
    IntegerQuantization quantization; ///< Exact integer error for INTEGER exact arithmetic.
    double absoluteErrorBound = 0; ///< Upward total quantization/approximation bound in command increments.
    bool exactArithmetic = true; ///< False only for explicitly approximated nonzero radians.
    double approximateRequestedNative = 0; ///< Approximate endpoint when exactArithmetic is false.
    double approximationErrorBound = 0; ///< Conservative endpoint conversion error bound.
    bool referenceUsed = false; ///< True makes retainedReference an active dependency.
    AxisReference retainedReference; ///< Frozen observed time/basis/position/provenance when used.
};
/** Exact signed rate, including zero. Zero is arithmetic, not a stop command. */
struct VelocityRequest {
    Rational value; ///< Exact requested rate; no int16 RPM bound.
    VelocityUnit unit; ///< Input spatial/time dimension.
    CoordinateFrame frame = CoordinateFrame::NATIVE; ///< Native/motor/load rate interpretation.
    Generation configurationGeneration = 0; ///< Required configuration epoch.
    Rounding rounding = Rounding::EXACT; ///< Integer rounding policy.
    Rational maximumQuantizationError; ///< Nonnegative native-rate allowance including approximation.
    bool approximate = false; ///< Explicit permission for nonzero radians/time conversion.
    double maximumApproximationError = 0; ///< Positive native-rate allowance for radians.
};
/** Pure native rate evidence. Profiles subsequently check their own wire width,
 * configured native scale/unit and operational permission. No position limits. */
struct PreparedVelocityTarget {
    VelocityRequest requested; ///< Original intent.
    Binding binding; ///< Frozen route/configuration provenance.
    NativeQuantity requestedNative; ///< Exact native rate when exactArithmetic.
    NativeRepresentation representation = NativeRepresentation::INTEGER; ///< Active result format.
    int64_t effectiveNative = 0; ///< Signed wide native command increments/time for INTEGER.
    float effectiveFloat = 0; ///< Native binary32 rate for BINARY32.
    IntegerQuantization quantization; ///< Integer exact-error evidence when applicable.
    double absoluteErrorBound = 0; ///< Upward native-rate quantization/approximation error bound.
    bool exactArithmetic = true; ///< False for nonzero radians approximation.
    double approximateRequestedNative = 0; ///< Approximate rate when not exactArithmetic.
};
/** Validate facts and policies without requiring unused geometry.
 * @param[in] config Caller-owned configuration. @return Local status. */
Status validateAxisConfig(const AxisConfig& config);
/** Validate stationary idle evidence and publish a new configuration epoch.
 * Binding and epochs must match current; only this function advances epochs.
 * Origins cannot be injected through candidate. Position-scale/format changes clear
 * origins and soft limits. Unchanged interpretation may retain evidence with
 * its ORIGINAL observedUs/maximumAgeUs and updated epochs. Both outputs are
 * atomic on failure; candidate may alias current and retainedReference evidence.
 * @param[in,out] current Current configuration. @param[in] candidate Desired facts.
 * @param[in] evidence Current stationary witness. @param[out] retainedReference Optional retained witness.
 * @return Local status; no counter, motor or device configuration is changed. */
Status configureAxis(AxisConfig& current, const AxisConfig& candidate,
    const AxisReference& evidence, AxisReference* retainedReference = nullptr);
/** Establish mechanical host zero with stationary native evidence; advances
 * configuration/confidence epochs, clears soft limits, preserves independent
 * other zero. @param[in,out] config Configuration. @param[in] originNative Command zero.
 * @param[in] evidence Qualified witness. @param[in] origin Relation to establish.
 * @return Local status; atomic on failure. */
Status setAxisOrigin(AxisConfig& config, int64_t originNative, const AxisReference& evidence,
    AxisOrigin origin = AxisOrigin::MECHANICAL);
/** Revoke position-dependent prepared work, preserving origins only for POSITION.
 * COORDINATES also revokes both origins/soft limits and advances configuration.
 * At epoch exhaustion confidence still clears, exhausted latches permanently,
 * and failure is returned: the sole output-on-failure exception. Application
 * must retire the binding and all old work before constructing a new epoch.
 * @param[in,out] config Current interpretation. @param[in,out] reference Witness to clear.
 * @param[in] loss Actual dependency loss. @return Local status; no I/O or stop. */
Status invalidateAxisReference(AxisConfig& config, AxisReference& reference,
    ReferenceLoss loss = ReferenceLoss::COORDINATES);
/** Prepare exact/explicitly approximate position arithmetic atomically. A supplied
 * witness is checked, never silently ignored. Relative requests cannot fall back
 * to another basis. Requested AND effective endpoints must satisfy inclusive
 * native/soft limits; negative displacement is not an absolute lower-limit test.
 * @param[in] request Intent. @param[in] config Interpretation.
 * @param[in] reference Optional witness (required for relative/wrapped/REQUIRED).
 * @param[out] output Prepared evidence, unchanged on failure. @return Local status. */
Status preparePosition(const PositionRequest& request, const AxisConfig& config,
    const AxisReference* reference, PreparedTarget& output);
/** Recheck frozen binding/epochs, time and any used position witness before
 * admission. Does not establish motion completion or renew observedUs.
 * @param[in] prepared Frozen target. @param[in] config Current interpretation.
 * @param[in] reference Current witness if used. @param[in] nowUs Monotonic admission time, 0..INT64_MAX.
 * @return Local validity; supplied time must not precede retained preparation time.
 * POSITION loss leaves reference-free absolute targets valid; COORDINATES loss
 * invalidates their configuration epoch too. */
Status validatePreparedTarget(const PreparedTarget& prepared, const AxisConfig& config,
    const AxisReference* reference, uint64_t nowUs);
/** Convert and quantize a pure signed rate using configured command increments
 * per nativeVelocityTime. Native frame requires exactly these dimensions.
 * @param[in] request Intent. @param[in] config Scale/configuration facts.
 * @param[out] output Rate evidence, unchanged on failure. @return Local status. */
Status prepareVelocityTarget(const VelocityRequest& request, const AxisConfig& config,
    PreparedVelocityTarget& output);
} // namespace MotorControlCAN
