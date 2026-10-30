/** @file ReadOperation.h
 * @brief Protocol-neutral read phase, outcome and local validation vocabulary.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Status.h>

namespace MotorControlCAN {
/** Caller-owned read phase; local OK does not mean a completed observation. */
enum class ReadState : uint8_t {
    EMPTY,     ///< Unprepared.
    ACTIVE,    ///< Work or passive evidence is pending.
    SUCCEEDED, ///< All selected fields completed.
    FAILED     ///< Terminal failure; independent completed state blocks survive.
};
/** Retained read outcome, independent of native drive state or health. */
enum class ReadOutcome : uint8_t {
    NONE,              ///< No terminal outcome.
    SUCCESS,           ///< Selected raw fields acquired, interpretation may remain unknown.
    REPLY_ERROR,       ///< Malformed value or service reply.
    PROTOCOL_REJECTED, ///< Full native rejection retained separately.
    TRANSPORT_ERROR,   ///< Controller or channel failure.
    CANCELLED,         ///< Local cancellation, never a motor stop.
    DEADLINE,          ///< Immutable overall cutoff reached.
    INVALIDATED        ///< Binding, configuration or node evidence changed.
};
/** Status detail for the concrete checked read calls. */
enum class ReadError : int32_t {
    TARGET = 1,  ///< Invalid/stale target or epoch.
    ARGUMENT,    ///< Invalid selection, budgets or capacity.
    STATE,       ///< Wrong kind, phase or incomplete block.
    CORRELATION, ///< Service/object/operation provenance mismatch.
    TIME,        ///< Invalid/future/expired supplied time.
    VALUE,       ///< Source count exceeds the bounded record.
    OVERLAP,     ///< Output aliases borrowed input.
    READ_EFFECT  ///< Q20 does not establish the required non-changing read.
};
} // namespace MotorControlCAN
