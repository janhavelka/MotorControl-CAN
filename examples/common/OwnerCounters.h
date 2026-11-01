// SPDX-License-Identifier: MIT
// Private arithmetic shared by owner boundaries; tests exercise extreme seeds directly.
#pragma once
#include <stdint.h>

namespace MotorControlCANExample {
namespace detail {
inline void add(uint32_t& value, uint32_t amount = 1) noexcept {
    value = amount > UINT32_MAX - value ? UINT32_MAX : value + amount;
}
inline void add(uint8_t& value) noexcept {
    if (value != UINT8_MAX)
        ++value;
}
} // namespace detail
} // namespace MotorControlCANExample
