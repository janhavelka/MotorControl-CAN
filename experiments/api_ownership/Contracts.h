/** @file Contracts.h
 * @brief Prompt-6 prototype value contracts; no installed driver or device I/O.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <MotorControlCAN/Endpoint.h>
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/Capabilities.h>
#include <MotorControlCAN/Native.h>
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/ReadOperation.h>
#include <MotorControlCAN/can/Frame.h>

/** Portable proof values and concrete synthetic operations. */
namespace MotorControlCAN {
namespace ProofProfile {

/** Host-only identities, never wire correlation tags. Zero is invalid. */
struct OperationToken {
    uint32_t value = 0; ///< Application-assigned nonzero operation identity.
};
/** Host-only step identity within one operation. Zero is invalid. */
struct StepToken {
    uint32_t value = 0; ///< Nonzero step identity; synthetic operations use one.
};
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
struct ServiceReply {
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

/** Synthetic profile configuration only; production coordinates live in Axis.h.
 * The proof adds capabilities and an I32 wire limit, never a production profile. */
struct ProofAxisConfig : AxisConfig {
    Capabilities capabilities; ///< Test-only declared capability facts.
    /** Synthetic I32 target register bounds; unrelated to common axis width. */
    ProofAxisConfig() { nativeMinimum = INT32_MIN; nativeMaximum = INT32_MAX; }
};

/** Acknowledged synthetic write ends with completion still NOT_OBSERVED. */
enum class CommandOutcome : uint8_t {
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
/** Synthetic move permission/sequencing details. Production coordinate
 * preparation preserves AxisError, including its UnitError offset. */
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
    OUT_OF_RANGE,            ///< Exact arithmetic storage or inclusive native limit exceeded.
    MISSING_ORIGIN,          ///< Motor/load coordinate lacks a known native host origin.
    CALIBRATION_UNQUALIFIED, ///< Required calibration evidence was not supplied.
    UNIT_ERROR_BASE = 100    ///< Numeric failures add UnitError to this base; Err/msg preserved.
};

} // namespace ProofProfile
} // namespace MotorControlCAN
