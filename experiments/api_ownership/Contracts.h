/** @file Contracts.h
 * @brief Prompt-6 prototype value contracts; no installed driver or device I/O.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdint.h>

/** Portable proof values and concrete synthetic operations. */
namespace MotorControlCAN {

/** Pure-call errors, separate from protocol/drive state and application health. */
enum class Err : uint8_t {
    OK,             ///< The local calculation/validation succeeded.
    INVALID_CONFIG, ///< Invalid argument, configuration, provenance or sequence.
    ILLEGAL_VALUE,  ///< Requested value exceeds exact representation or limits.
    UNSUPPORTED,    ///< Required capability or conversion is unavailable.
    FRAME_ERROR,    ///< Native reply or CAN frame shape is invalid.
    PROTOCOL_ERROR  ///< Checked native service rejection; raw abort stays separate.
};

/** Local call result. msg has static lifetime; detail is a typed reason value.
 * Success does not establish a successful service or physical motor behavior. */
struct Status {
    Err code;       ///< Local call category; default OK.
    int32_t detail; ///< Typed numeric reason, not raw SDO abort; default zero.
    const char* msg; ///< Static-lifetime diagnostic text; default empty string.
    /** Construct local success without side effects. */
    constexpr Status() : code(Err::OK), detail(0), msg("") {}
    /** Construct an explicit local-call result without owning text.
     * @param[in] c Call-result category.
     * @param[in] d Typed numeric reason associated with the called function.
     * @param[in] m Static-lifetime diagnostic string supplied by the caller. */
    constexpr Status(Err c, int32_t d, const char* m) : code(c), detail(d), msg(m) {}
    /** @return True exactly when code is OK, independent of operation outcome. */
    constexpr bool isOk() const { return code == Err::OK; }
    /** @return The same local-call success predicate as isOk(). */
    constexpr explicit operator bool() const { return isOk(); }
};

/** @return Default local-call success; allocates nothing and performs no I/O. */
constexpr Status Ok() { return Status(); }

/** Return a static diagnostic label without allocation or retained input.
 * @param[in] code Local-call error value.
 * @return Static label, or UNKNOWN for an unrecognized value. */
inline const char* errToString(Err code) {
    switch (code) {
    case Err::OK: return "OK";
    case Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case Err::ILLEGAL_VALUE: return "ILLEGAL_VALUE";
    case Err::UNSUPPORTED: return "UNSUPPORTED";
    case Err::FRAME_ERROR: return "FRAME_ERROR";
    case Err::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
    }
    return "UNKNOWN";
}

/** Host-only identities, never wire correlation tags. Zero is invalid. */
struct OperationToken {
    uint32_t value = 0; ///< Application-assigned nonzero operation identity.
};
/** Host-only step identity within one operation. Zero is invalid. */
struct StepToken {
    uint32_t value = 0; ///< Nonzero step identity; synthetic operations use one.
};
/** Nonzero application generation. Never wrap/reuse while old work may exist. */
typedef uint32_t Generation;

/** Selected protocol domain, without native network or drive state. */
enum class Protocol : uint8_t {
    CANOPEN,    ///< Selected Classical CANopen domain.
    PROPRIETARY ///< Non-CANopen domain with its own protocol-local services.
};

/** Protocol-owned wire addressing. Generic code imposes no CANopen node range.
 * bus identifies a physical interface; channel/subaxis are independent selectors.
 * profile is an application-assigned nonzero identity, not a runtime registry. */
struct Endpoint {
    Protocol protocol = Protocol::CANOPEN; ///< Selects the protocol validation boundary.
    uint32_t bus = 0;     ///< Application physical-interface identity; zero is permitted.
    uint32_t profile = 0; ///< Application profile identity; zero is invalid in a binding.
    uint32_t address = 0; ///< Protocol-owned address; no generic 1..127 restriction.
    uint16_t channel = 0; ///< Protocol-owned channel selector; zero is permitted.
    uint16_t subaxis = 0; ///< Independent axis selector within the endpoint.
};

/** Logical axis and immutable route/interpretation provenance for admitted work.
 * Default is invalid. Native NMT/statusword/EMCY observations are not axis fields. */
struct Binding {
    uint32_t logicalAxisId = 0; ///< Nonzero logical identity independent of wire address.
    Endpoint endpoint; ///< Copied route and profile identity.
    Generation bindingGeneration = 0; ///< Nonzero route/identity epoch; never wrap/reuse live work.
    Generation configurationGeneration = 0; ///< Nonzero coordinate-interpretation epoch.
};

/** Compare all identity, route and generation fields; acquires no external data.
 * @param[in] a First caller-owned binding, borrowed only for this call.
 * @param[in] b Second caller-owned binding, borrowed only for this call.
 * @return True when all fields match; matching invalid values are not validation. */
bool sameBinding(const Binding& a, const Binding& b) noexcept;
/** Check common identity only. Protocol owners additionally validate their route.
 * @param[in] binding Caller-owned route/identity, borrowed only during this call.
 * @return INVALID_CONFIG for zero identity/generation or an unknown protocol. */
Status validateBinding(const Binding& binding) noexcept;

/** Owned frame. dlc is the wire DLC, payloadLength the actual byte count.
 * RTR has no payload even when dlc is nonzero. The fd flag expresses an
 * unsupported requirement for rejection before admission; this Classical frame
 * has no FD payload storage. Default is a valid empty standard data frame.
 * A future FD path requires its own bounded frame and adapter contract. */
struct Frame {
    uint32_t id = 0; ///< Identifier bits, <=0x7ff standard or <=0x1fffffff extended.
    uint8_t dlc = 0; ///< Classical wire DLC, 0..8, including RTR requested length.
    uint8_t payloadLength = 0; ///< Actual copied data bytes, 0..8; zero for RTR.
    bool extended = false; ///< True preserves 29-bit wire format even for a small ID.
    bool rtr = false; ///< True denotes remote request with no data payload.
    bool fd = false; ///< Unsupported FD requirement; rejected before admission.
    uint8_t data[8] = {}; ///< Owned payload; only payloadLength bytes are meaningful.
};

/** Check CAN identifier/format/DLC/payload invariants; no protocol restrictions.
 * @param[in] frame Caller-owned Classical frame, unchanged and never retained.
 * @return UNSUPPORTED for fd=true; otherwise FRAME_ERROR for invalid shape and
 * OK for representable Classical frames. No frame is sent, changed or retained. */
Status validateFrame(const Frame& frame) noexcept;

/** Fixed owned service bytes. length is 0..32; no call retains borrowed storage. */
struct Payload {
    uint8_t length = 0; ///< Active prefix length, 0..32 bytes; default empty.
    uint8_t bytes[32] = {}; ///< Owned prefix; bytes beyond length are inactive storage.
};

/** Native SDO descriptors and raw requests remain below common motion. */
enum class WorkKind : uint8_t {
    SDO_UPLOAD,   ///< Read selected index/subindex; no outgoing value payload.
    SDO_DOWNLOAD, ///< Write payload to selected index/subindex.
    RAW_FRAME     ///< Send the complete frame; SDO fields are inactive.
};

/** Copied request yielded by an operation. The owner copies the complete value.
 * deadlineUs is immutable, exclusive, in supplied monotonic microseconds; zero
 * is a valid time. operation/step identify local ownership, not an SDO wire tag.
 * mayChange is explicit policy even for reads; uploads are not universally
 * side-effect-free. payload owns download bytes; frame owns RAW_FRAME bytes.
 * A successful queue admission proves no controller transmission or device ACK. */
struct ServiceWork {
    Binding binding; ///< Complete frozen identity/route/generations; default invalid.
    OperationToken operation; ///< Nonzero host operation identity.
    StepToken step; ///< Nonzero host step identity, not a wire transaction tag.
    uint64_t deadlineUs = 0; ///< Immutable exclusive supplied-microsecond deadline.
    WorkKind kind = WorkKind::SDO_UPLOAD; ///< Selects the active descriptor fields.
    bool mayChange = false; ///< Explicit possible effect; not inferred from read/write name.
    uint16_t index = 0; ///< Native object index for SDO work only.
    uint8_t subindex = 0; ///< Native object subindex for SDO work only.
    Payload payload; ///< Owned outgoing value for SDO_DOWNLOAD; otherwise inactive.
    Frame frame; ///< Owned complete RAW_FRAME descriptor; otherwise inactive.
};

/** Owner-qualified service termination, distinct from controller TX completion. */
enum class EventKind : uint8_t {
    ACKNOWLEDGED,      ///< Checked selected-service success, independent of CAN ACK.
    PROTOCOL_REJECTED, ///< Checked server rejection with a raw abort code.
    REPLY_ERROR,       ///< Malformed reply/local client abort, not server rejection.
    TRANSPORT_FAILURE, ///< Owner-reported controller/service transport failure.
    CANCEL,            ///< Local cancellation; never a physical motor stop.
    DEADLINE           ///< Owner service reached/exceeded the exclusive deadline.
};

/** Copied owner evidence. observedUs is owner delivery/qualification time, not
 * drive sample or exact physical wire time. It uses the operation's supplied
 * monotonic domain. queued and transmissionPossible are separate evidence;
 * transmissionPossible starts when the owner can no longer exclude external TX.
 * protocolAcknowledged requires selected-service confirmation, never CAN ACK.
 * An SDO abort has its full unsigned bits in abortCode when abortValid is true:
 * PROTOCOL_REJECTED is a checked server abort; REPLY_ERROR may retain a local
 * client's abort from a malformed reply, without inventing a server rejection.
 * Matching host tokens alone cannot qualify an indistinguishable delayed reply.
 * Default evidence is invalid until correlated and qualified by the owner. */
struct ServiceEvidence {
    Binding binding; ///< Original work's full immutable provenance.
    OperationToken operation; ///< Original work's local operation identity.
    StepToken step; ///< Original work's local step identity.
    EventKind kind = EventKind::TRANSPORT_FAILURE; ///< Terminal owner event category.
    uint64_t observedUs = 0; ///< Supplied current owner-service microseconds; zero is valid.
    bool queued = false; ///< Request was accepted locally; alone proves no transmission.
    bool transmissionPossible = false; ///< Owner can no longer exclude external transmission.
    bool protocolAcknowledged = false; ///< Checked native service success, never CAN ACK alone.
    bool abortValid = false; ///< True makes abortCode meaningful for server/local client abort.
    uint32_t abortCode = 0; ///< Entire unsigned native abort, including high bit when valid.
    int32_t transportDetail = 0; ///< Uninterpreted application/controller failure reason.
    Payload payload; ///< Copied upload bytes or retained malformed-reply prefix.
};

/** Independent motor technology; does not select transport or feedback. */
enum class MotorKind : uint8_t {
    UNKNOWN,   ///< Motor technology has not been supplied/established.
    STEPPER,   ///< Stepper motor; no full-step metadata is implied.
    BLDC,      ///< Brushless DC motor; no selected feedback is implied.
    BRUSHED_DC ///< Brushed DC motor.
};
/** Selected feedback fact, independent of command scale or full-step metadata. */
enum class FeedbackKind : uint8_t {
    UNKNOWN, ///< Selected feedback has not been established.
    NONE,    ///< Explicit configuration without a selected feedback source.
    ENCODER, ///< Encoder feedback; resolution/basis require separate evidence.
    HALL     ///< Hall feedback; does not imply encoder command scale.
};
/** Reviewed capabilities/configuration facts supplied by the caller.
 * False means unavailable; selection does not calibrate, enable or brake. */
struct Capabilities {
    MotorKind motor = MotorKind::UNKNOWN; ///< Independent selected motor fact.
    FeedbackKind feedback = FeedbackKind::UNKNOWN; ///< Independent selected feedback fact.
    bool position = false; ///< Explicit position capability availability.
    bool velocity = false; ///< Explicit velocity capability availability.
    bool torque = false; ///< Explicit torque capability; not inferred from current settings.
    bool brake = false; ///< Explicit brake capability; no output action is implied.
    bool calibrationRequired = false; ///< Selected operation requires calibration evidence.
    bool calibrationQualified = false; ///< Caller-qualified required calibration evidence exists.
};

/** One requested capability; no action is executed by checking availability. */
enum class Capability : uint8_t {
    POSITION, ///< Check position capability and required calibration evidence.
    VELOCITY, ///< Check velocity capability and required calibration evidence.
    TORQUE,   ///< Check torque capability and required calibration evidence.
    BRAKE     ///< Check brake capability and required calibration evidence.
};
/** Check one declared capability and its supplied calibration requirement.
 * @param[in] facts Caller-supplied independent capability/qualification facts.
 * @param[in] requested Single required capability, with no implicit side effect.
 * @return OK for available and qualified, UNSUPPORTED for absent/unknown
 * capability, INVALID_CONFIG for required unqualified calibration. Detail uses
 * MoveError::UNSUPPORTED_CAPABILITY or CALIBRATION_UNQUALIFIED. No state changes. */
Status requireCapability(const Capabilities& facts, Capability requested) noexcept;

/** Exact signed value; positive denominator. Default denotes exact zero. */
struct Rational {
    int64_t numerator = 0; ///< Exact signed numerator; prototype consumers bound magnitude.
    uint64_t denominator = 1; ///< Positive exact denominator; zero is invalid.
    /** Construct exact zero with denominator one. */
    Rational() = default;
    /** Preserve exact components without normalization or validation.
     * @param[in] n Signed numerator in the selected quantity's unit.
     * @param[in] d Positive denominator; consumers reject zero/out-of-bound values. */
    Rational(int64_t n, uint64_t d = 1) : numerator(n), denominator(d) {}
};
/** Retained scale provenance; metadata does not verify active drive settings. */
enum class ScaleSource : uint8_t {
    UNKNOWN,    ///< No usable scale provenance supplied.
    ASSUMED,    ///< Application explicitly accepts an assumption.
    DOCUMENTED, ///< Supplied scale comes from applicable documentation.
    READBACK,   ///< Supplied scale comes from checked device readback.
    QUALIFIED   ///< Supplied scale has additional application qualification.
};
/** Positive rational scale; numerator zero denotes unavailable optional data. */
struct UnitScale {
    uint32_t numerator = 0; ///< Positive known-scale numerator, or zero for unavailable.
    uint32_t denominator = 1; ///< Positive known-scale denominator; zero is invalid if used.
    ScaleSource source = ScaleSource::UNKNOWN; ///< Retained evidence class, not active-state proof.
};
/** Independent command, gear and feedback scales. No full-step prerequisite.
 * Command increments and feedback counts are per motor turn. Geometry is
 * optional metadata; gear is motor turns per load turn. No scale is inferred. */
struct UnitConfig {
    UnitScale commandIncrementsPerMotorTurn; ///< Selected native command increments per motor turn.
    UnitScale motorTurnsPerLoadTurn; ///< Gear ratio in motor turns per load turn.
    UnitScale feedbackCountsPerMotorTurn; ///< Independent feedback counts per motor turn.
    UnitScale fullStepsPerMotorTurn; ///< Optional stepper geometry, unused by proof conversion.
};
/** Input spatial dimension. NATIVE_INCREMENTS is profile command representation. */
enum class PositionUnit : uint8_t {
    NATIVE_INCREMENTS, ///< Exact selected-profile command increments.
    TURNS,             ///< Mechanical turns in the declared motor/load frame.
    DEGREES            ///< Mechanical degrees, exactly 360 per turn.
};
/** Reference basis. Ordinary absolute targets preserve all turns without wrap. */
enum class CoordinateFrame : uint8_t {
    NATIVE, ///< Already in selected native command coordinates, without host-origin addition.
    MOTOR,  ///< Mechanical motor coordinate relative to the supplied host origin.
    LOAD    ///< Mechanical load coordinate using the supplied motor/load ratio and origin.
};
/** Quantization intent; this prototype implements EXACT only. */
enum class Rounding : uint8_t {
    EXACT,       ///< Require exact integer native representation; supported in this proof.
    NEAREST,     ///< Nearest-integer intent; rejected by this prototype.
    TOWARD_ZERO, ///< Truncation intent; rejected by this prototype.
    FLOOR,       ///< Round toward negative infinity; rejected by this prototype.
    CEIL         ///< Round toward positive infinity; rejected by this prototype.
};

/** Caller-owned host interpretation; changing it causes no drive traffic.
 * Native bounds are inclusive; originNative is the established command zero for
 * MOTOR/LOAD coordinates. Default binding and scales are invalid for a move. */
struct AxisConfig {
    Binding binding; ///< Current route/identity/coordinate provenance; default invalid.
    Capabilities capabilities; ///< Independently supplied selected configuration facts.
    UnitConfig units; ///< Explicit scales; used values need positive components/provenance.
    bool originKnown = false; ///< True qualifies originNative for motor/load conversion.
    int32_t originNative = 0; ///< Native command position corresponding to host coordinate zero.
    int32_t nativeMinimum = INT32_MIN; ///< Inclusive lower native command-increment bound.
    int32_t nativeMaximum = INT32_MAX; ///< Inclusive upper native command-increment bound.
};

/** Exact absolute multi-turn target. configurationGeneration must match binding.
 * Quantization error is in native increments; prototype requires exactly zero.
 * No wrapping/relative/reference or production arithmetic is implemented here. */
struct PositionRequest {
    Rational value; ///< Requested exact absolute multi-turn coordinate.
    PositionUnit unit = PositionUnit::NATIVE_INCREMENTS; ///< Dimension of value.
    CoordinateFrame frame = CoordinateFrame::NATIVE; ///< Coordinate basis of value.
    Generation configurationGeneration = 0; ///< Must match the current binding; zero invalid.
    Rounding rounding = Rounding::EXACT; ///< Explicit conversion intent; only EXACT is supported.
    Rational maximumQuantizationError; ///< Allowed native-increment error; proof requires exact zero.
};

/** Wide signed exact velocity intent only; no prototype velocity engine.
 * position/time dimensions are explicit; a profile must check native narrowing. */
enum class TimeUnit : uint8_t {
    SECOND,     ///< SI second, used as a velocity denominator.
    MINUTE,     ///< Exactly 60 seconds, without implying native RPM representation.
    MILLISECOND ///< Exactly 1/1000 second.
};
/** Velocity dimension, independent of RS native RPM widths. */
struct VelocityUnit {
    PositionUnit position = PositionUnit::TURNS; ///< Numerator spatial dimension.
    TimeUnit time = TimeUnit::SECOND; ///< Denominator time dimension.
};
/** Unconverted exact signed request; retains values such as +/-70000 intact. */
struct VelocityRequest {
    Rational value; ///< Wide signed exact requested rate; no native narrowing here.
    VelocityUnit unit; ///< Explicit spatial quantity per declared time unit.
    CoordinateFrame frame = CoordinateFrame::MOTOR; ///< Motor/load/native rate basis.
};

/** Fixture dimensions; identical object numbers do not equate these values. */
enum class RampKind : uint8_t {
    TIME_MS,                    ///< Native ramp duration in milliseconds.
    ACCELERATION_RPM_PER_SECOND  ///< Native acceleration in mechanical rpm/s.
};
/** Dimension-tagged documentary ramp input. No conversion/trajectory is implied.
 * TIME_MS value is milliseconds; ACCELERATION_RPM_PER_SECOND value is rpm/s. */
struct NativeRamp {
    RampKind kind = RampKind::TIME_MS; ///< Dimension tag; no inferred conversion.
    uint32_t value = 0; ///< Unsigned native value in the dimension selected by kind.
};

/** Actual operation states; default contexts are EMPTY and yield no work. */
enum class ReadState : uint8_t {
    EMPTY,     ///< Not prepared; next/advance reject without mutation.
    ACTIVE,    ///< Prepared and awaiting one terminal owner event.
    SUCCEEDED, ///< Complete valid typed read retained for inspection.
    FAILED     ///< Complete failed read with retained status/evidence.
};
/** Read terminal outcome, independent of Status call validity. */
enum class ReadOutcome : uint8_t {
    NONE,              ///< No terminal read result yet.
    SUCCESS,           ///< Complete synthetic identity decoded successfully.
    REPLY_ERROR,       ///< Invalid reply content/shape or local client abort.
    PROTOCOL_REJECTED, ///< Checked native server rejection.
    TRANSPORT_ERROR,   ///< Owner-reported transport failure.
    CANCELLED,         ///< Locally cancelled; no physical stop is implied.
    DEADLINE           ///< Absolute operation deadline reached.
};
/** Move state follows the established action vocabulary. */
enum class ActionState : uint8_t {
    EMPTY,     ///< No prepared write; next/advance reject.
    ACTIVE,    ///< Write prepared and awaiting a terminal event.
    SUCCEEDED, ///< Synthetic write acknowledged; not physical completion.
    FAILED     ///< Failed terminal write with retained uncertainty/evidence.
};
/** Trigger/service confirmation, never a shaft movement claim. */
enum class ActionExecution : uint8_t {
    NOT_TRANSMITTED, ///< No possible external transmission was reported.
    ACKNOWLEDGED,    ///< Native selected-service success was checked.
    REJECTED,        ///< Native rejection checked; partial effects may remain uncertain.
    UNKNOWN          ///< Possible transmission without checked acknowledgement/rejection.
};
/** Native completion observation; never independent physical qualification. */
enum class ActionCompletion : uint8_t {
    NOT_OBSERVED, ///< No native motion-completion evidence; always used by this proof.
    OBSERVED      ///< Native completion evidence observed; no proof path produces this.
};
/** Acknowledged synthetic write ends with completion still NOT_OBSERVED. */
enum class ActionOutcome : uint8_t {
    NONE,              ///< No terminal write outcome yet.
    OBSERVED,          ///< Reserved observed-completion outcome; unused by this proof.
    ACKNOWLEDGED,      ///< Synthetic command-only write confirmed; completion unobserved.
    REPLY_ERROR,       ///< Malformed reply/local client abort or invalid ACK value shape.
    PROTOCOL_REJECTED, ///< Checked native server rejection with separate raw abort.
    TRANSPORT_ERROR,   ///< Owner-reported transport failure.
    CANCELLED,         ///< Local cancellation, preserving possible external effects.
    DEADLINE           ///< Exclusive absolute deadline reached.
};

/** Numeric Status detail for read sequencing and typed-getter failures. */
enum class ReadError : int32_t {
    NONE,              ///< No read-specific reason.
    INVALID_TARGET,    ///< Invalid common binding or incompatible proof protocol.
    INVALID_OPERATION, ///< Zero operation token.
    INVALID_DEADLINE,  ///< Deadline does not strictly follow preparation time.
    INVALID_STATE,     ///< Call is not legal in the current context state.
    WRONG_CORRELATION, ///< Evidence binding/operation/step differs from the context.
    INVALID_EVENT,     ///< Invalid event kind/flags or out-of-bound payload.
    CLOCK_ERROR,       ///< Backward service time or event/deadline-kind inconsistency.
    DEADLINE_EXPIRED,  ///< Supplied microsecond deadline was reached.
    TRANSPORT_FAILURE, ///< Retained owner-reported transport failure.
    CANCELLED,         ///< Retained local read cancellation.
    NOT_COMPLETE,      ///< Typed getter lacks a successful complete read.
    WRONG_KIND,        ///< Context is not the requested typed read kind.
    STALE_GENERATION   ///< Typed getter's complete current binding differs.
};
/** Numeric Status detail for prototype move preparation/advancement. */
enum class MoveError : int32_t {
    NONE,                    ///< No move-specific reason.
    INVALID_TARGET,          ///< Invalid binding or incompatible proof protocol.
    INVALID_OPERATION,       ///< Zero local operation identity.
    INVALID_DEADLINE,        ///< Deadline does not strictly follow preparation time.
    INVALID_STATE,           ///< Current context phase does not permit this call.
    WRONG_CORRELATION,       ///< Event binding/operation/step differs from context.
    INVALID_EVENT,           ///< Invalid event flags/kind or out-of-bound payload.
    CLOCK_ERROR,             ///< Backward time or event/deadline-kind inconsistency.
    DEADLINE_EXPIRED,        ///< Supplied exclusive microsecond deadline was reached.
    TRANSPORT_FAILURE,       ///< Retained owner-reported transport failure.
    CANCELLED,               ///< Local cancellation, not a physical stop.
    STALE_CONFIGURATION,     ///< Request coordinate generation differs from binding.
    UNRESOLVED_UNITS,        ///< A used scale lacks valid components/provenance.
    UNSUPPORTED_CAPABILITY,  ///< Requested position/velocity/torque/brake capability absent.
    INVALID_REQUEST,         ///< Unsupported coordinate/rounding intent or invalid limits.
    FRACTIONAL_TARGET,       ///< Exact target is not an integer native command increment.
    OUT_OF_RANGE,            ///< Prototype arithmetic bound or inclusive native limit exceeded.
    MISSING_ORIGIN,          ///< Motor/load coordinate lacks a known native host origin.
    CALIBRATION_UNQUALIFIED  ///< Required calibration evidence was not supplied.
};

} // namespace MotorControlCAN
