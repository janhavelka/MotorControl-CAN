/** @file Endpoint.h
 * @brief Logical axis identity and copied protocol route provenance.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <MotorControlCAN/Status.h>

namespace MotorControlCAN {

/** Nonzero host generation; never wrap or reuse while older work may exist.
 * Zero denotes invalid provenance. A host generation is never a wire tag. */
typedef uint32_t Generation;

/** Selected protocol domain without native network state or drive state. */
enum class Protocol : uint8_t {
    CANOPEN,    ///< Selected Classical CANopen domain; route rules remain protocol-local.
    PROPRIETARY ///< Non-CANopen domain with its own addressing and native services.
};

/** Copied protocol-owned route; generic code imposes no CANopen node range.
 * The application establishes the physical interface and selected profile.
 * Default profile zero makes the enclosing binding invalid; other route fields
 * may be zero. These fields provide identity, not evidence of device presence. */
struct Endpoint {
    Protocol protocol = Protocol::CANOPEN; ///< Selected protocol validation boundary.
    uint32_t bus = 0;     ///< Application physical-interface identity; zero is permitted.
    uint32_t profile = 0; ///< Application-assigned profile identity; zero is invalid in a binding.
    uint32_t address = 0; ///< Protocol-owned wire address; no common 1..127 restriction.
    uint16_t channel = 0; ///< Independent protocol-owned channel selector; zero is permitted.
    uint16_t subaxis = 0; ///< Independent axis selector within the endpoint; zero is permitted.
};

/** Logical axis and copied route/interpretation provenance for admitted work.
 * Default is invalid. Changing a route or its interpretation requires a fresh
 * generation; applications own invalidation and serialization. Native NMT,
 * statusword and fault observations are not common axis identity fields. */
struct Binding {
    uint32_t logicalAxisId = 0; ///< Nonzero logical identity independent of wire addressing.
    Endpoint endpoint; ///< Copied physical route, protocol and profile identity.
    Generation bindingGeneration = 0; ///< Nonzero route/identity epoch; never wrap or reuse live work.
    Generation configurationGeneration = 0; ///< Nonzero coordinate-interpretation epoch.
};

/** Numeric Status detail returned by common binding validation. */
enum class BindingError : int32_t {
    NONE = 0,          ///< No binding validation failure.
    INVALID_TARGET = 1 ///< A required identity/generation is zero or protocol is unknown.
};

/** Compare every identity, route and generation field without validation.
 * Performs bounded comparison only, with no allocation, I/O or clock access.
 * Independent calls share no mutable state; callers serialize concurrent writes
 * to either input. Neither reference is retained.
 * @param[in] a First caller-owned binding, borrowed only during this call.
 * @param[in] b Second caller-owned binding, borrowed only during this call.
 * @return True when every field matches; matching invalid values also compare equal.
 */
bool sameBinding(const Binding& a, const Binding& b) noexcept;

/** Validate common identity without probing a device or applying protocol route rules.
 * Requires nonzero logicalAxisId, profile and both generations, plus a recognized
 * Protocol. Bus, address, channel and subaxis are deliberately unrestricted here;
 * protocol/profile owners additionally validate their route before admission.
 * Pure, bounded and reentrant for stable input; no allocation, I/O or clock access.
 * @param[in] binding Caller-owned route/identity; unchanged and borrowed only during this call.
 * @return OK with zero detail when common identity is valid. INVALID_CONFIG with
 * BindingError::INVALID_TARGET otherwise. No device presence, capability or
 * generation freshness is established by success.
 */
Status validateBinding(const Binding& binding) noexcept;

} // namespace MotorControlCAN
