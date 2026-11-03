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

} // namespace Detail
} // namespace MotorControlCAN
