/** @file Native.h
 * @brief Checked native width and ramp dimensions; no device conversion policy or I/O.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Units.h>

namespace MotorControlCAN {
/** A time parameter cannot be substituted for dimensional acceleration. */
enum class RampKind : uint8_t {
    TIME_MS,     ///< Native duration in milliseconds.
    ACCELERATION ///< Explicit spatial unit per two explicit time denominators.
};
/** Exact documentary native ramp quantity, before any profile width/rounding check.
 * No default, geometry, firmware scope or acceleration is inferred from a duration.
 */
struct NativeRamp {
    RampKind kind = RampKind::TIME_MS; ///< Valid dimension; a profile supplies the choice.
    Rational value; ///< Exact quantity; duration must be nonnegative, denominator positive.
    AccelerationUnit accelerationUnit; ///< Meaningful only for ACCELERATION.
};
/** Convert only within the explicit ramp dimension, publishing atomically.
 * @param input Borrowed exact native quantity.
 * @param destinationKind Must equal input.kind; cross-dimensional conversion is unsupported.
 * @param destinationUnit Explicit acceleration unit, ignored for TIME_MS.
 * @param config Explicit scale facts for convertAcceleration; no defaults are invented.
 * @param output Unchanged on failure; must not overlap input or config.
 * @return Checked exact conversion, or argument/scale/overflow error. TIME_MS remains
 * milliseconds; ACCELERATION uses convertAcceleration. Pure, bounded and reentrant.
 */
Status convertNativeRamp(const NativeRamp& input, RampKind destinationKind,
                         AccelerationUnit destinationUnit, const UnitConfig& config,
                         NativeRamp& output) noexcept;
/** Check integer representability before a profile encodes/narrows a native value.
 * @param value Exact signed common integer, never converted through floating point.
 * @param widthBytes Supported width: 1, 2, 4 or 8 bytes.
 * @param signedValue Signed destination when true; unsigned rejects negative input.
 * @return OK when representable, ILLEGAL_VALUE for range, INVALID_CONFIG for width.
 * An unsigned 64-bit destination accepts the nonnegative int64 input subset;
 * this function does not represent unsigned values above INT64_MAX. No I/O or mutation.
 */
Status checkNativeInteger(int64_t value, uint8_t widthBytes, bool signedValue) noexcept;
} // namespace MotorControlCAN
