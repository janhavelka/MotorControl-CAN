/** @file Status.h
 * @brief Allocation-free local call results shared by the portable core.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdint.h>

/** Portable validation and value contracts; no device I/O or application policy. */
namespace MotorControlCAN {

/** Pure-call errors, separate from protocol/drive state and application health. */
enum class Err : uint8_t {
    OK,             ///< The local calculation or validation succeeded.
    INVALID_CONFIG, ///< Invalid argument, configuration, provenance or sequence.
    ILLEGAL_VALUE,  ///< Requested value exceeds exact representation or limits.
    UNSUPPORTED,    ///< Required capability or conversion is unavailable.
    FRAME_ERROR,    ///< Native reply or CAN frame shape is invalid.
    PROTOCOL_ERROR  ///< Checked native service rejection; raw abort stays separate.
};

/** Local call result with borrowed static diagnostic text.
 * Success establishes only the called function's documented local result,
 * independent of service acknowledgement, motor behavior or application health.
 * Construction and inspection perform no I/O, allocation or clock access. */
struct Status {
    Err code;       ///< Local call category; default OK.
    int32_t detail; ///< Function-specific numeric reason; default zero, never a raw SDO abort.
    const char* msg; ///< Borrowed static-lifetime diagnostic text; default empty string.

    /** Construct local success with zero detail and an empty diagnostic string. */
    constexpr Status() : code(Err::OK), detail(0), msg("") {}

    /** Construct an explicit local result without copying or owning text.
     * @param[in] c Local call-result category.
     * @param[in] d Function-specific numeric reason; interpreted by the called API.
     * @param[in] m Static-lifetime diagnostic string, borrowed for this result's lifetime.
     */
    constexpr Status(Err c, int32_t d, const char* m) : code(c), detail(d), msg(m) {}

    /** @return True exactly when code is OK, independent of operation outcome. */
    constexpr bool isOk() const { return code == Err::OK; }

    /** @return The same local-call success predicate as isOk(). */
    constexpr explicit operator bool() const { return isOk(); }
};

/** @return Default local-call success; performs no I/O, allocation or clock access. */
constexpr Status Ok() { return Status(); }

/** Return a diagnostic label without allocation or retained input.
 * @param[in] code Local-call error value; unrecognized enum values are allowed.
 * @return Static-lifetime label, or UNKNOWN for an unrecognized value.
 */
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

} // namespace MotorControlCAN
