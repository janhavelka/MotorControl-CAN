// SPDX-License-Identifier: MIT
// Private arithmetic and guards shared by owner boundaries; tests exercise extreme seeds directly.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MotorControlCANOwner {
namespace detail {
inline void add(uint32_t& value, uint32_t amount = 1) noexcept {
    value = amount > UINT32_MAX - value ? UINT32_MAX : value + amount;
}
inline void add(uint8_t& value) noexcept {
    if (value != UINT8_MAX)
        ++value;
}
inline bool overlap(const void* a, size_t an, const void* b, size_t bn) noexcept {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
/** Marks owner entry for one scope, so a callback cannot reenter on any exit path. */
class EnteredScope {
  public:
    explicit EnteredScope(bool& flag) noexcept : flag_(flag) { flag_ = true; }
    ~EnteredScope() { flag_ = false; }
    EnteredScope(const EnteredScope&) = delete;
    EnteredScope& operator=(const EnteredScope&) = delete;

  private:
    bool& flag_;
};
} // namespace detail
} // namespace MotorControlCANOwner
