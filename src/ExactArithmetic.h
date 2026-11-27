// SPDX-License-Identifier: MIT
// Private to the core library sources; never installed. Exact unsigned helpers shared by the unit
// conversions and the axis arithmetic. Free functions with no state.
#pragma once
#include <cstdint>

namespace MotorControlCAN {
namespace Detail {

// The absolute value of n as an unsigned word. INT64_MIN has no signed magnitude, so it goes
// through n + 1 and comes out as 2^63.
inline uint64_t magnitude(int64_t n) noexcept {
    return n < 0 ? uint64_t(-(n + 1)) + 1 : uint64_t(n);
}

// Euclid's algorithm. gcd(a, 0) is a, so gcd(0, 0) is 0. Every caller that divides by the result
// passes at least one nonzero denominator or factor, so it never divides by zero.
inline uint64_t gcd(uint64_t a, uint64_t b) noexcept {
    while (b) {
        const uint64_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

// The full 128-bit product a * b as two words, built from 32-bit partial products so it needs no
// compiler extension (32-bit targets have no 128-bit integer type).
inline void multiplyWide(uint64_t a, uint64_t b, uint64_t& high, uint64_t& low) noexcept {
    const uint64_t a0 = a & 0xffffffffu, a1 = a >> 32, b0 = b & 0xffffffffu, b1 = b >> 32;
    const uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    const uint64_t middle = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
    low = (middle << 32) | (p00 & 0xffffffffu);
    high = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
}

// The sign of a * b - c * d, compared exactly in 128 bits: -1, 0 or 1.
inline int compareProducts(uint64_t a, uint64_t b, uint64_t c, uint64_t d) noexcept {
    uint64_t leftHigh, leftLow, rightHigh, rightLow;
    multiplyWide(a, b, leftHigh, leftLow);
    multiplyWide(c, d, rightHigh, rightLow);
    if (leftHigh != rightHigh) return leftHigh < rightHigh ? -1 : 1;
    return leftLow == rightLow ? 0 : (leftLow < rightLow ? -1 : 1);
}

} // namespace Detail
} // namespace MotorControlCAN
