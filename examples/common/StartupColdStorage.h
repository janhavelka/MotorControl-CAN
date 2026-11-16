/** @file StartupColdStorage.h
 * @brief Application-only startup allocation for cold history; never active CAN or stop storage.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace MotorControlCANExample {
/** Explicit allocator boundary. Both callbacks and context must outlive the allocation owner. */
struct ColdStorageAllocator {
    void* context = nullptr; ///< Borrowed application allocator state.
    /** @param context Borrowed state. @param alignment Power-of-two alignment.
     * @param bytes Positive rounded size. @param external True requests only external RAM.
     * @return Aligned owned block, or null. False must request only internal RAM. */
    void* (*allocate)(void* context, size_t alignment, size_t bytes, bool external) = nullptr;
    /** @param context Same allocator state. @param memory Block returned by allocate. */
    void (*release)(void* context, void* memory) = nullptr;
};
/** Actual successful allocation branch; availability is never inferred from a build flag. */
enum class ColdStoragePlacement : uint8_t {
    NONE,     ///< Invalid input or both permitted allocation attempts were refused.
    INTERNAL, ///< The explicit internal-memory allocation succeeded.
    EXTERNAL  ///< The explicit external-memory allocation succeeded.
};
/** One stationary startup allocation. Construct once before control service starts, and keep it
 * alive while any cold record borrows it. No reallocation, retry loop or implicit initialization.
 * The external attempt is optional; refusal falls back once to explicitly internal RAM. Invalid
 * callbacks/size/alignment or both refusals leave data null. The destructor releases the one block;
 * callers destroy any object placed in it first. Never put an owner, active axis context, task stack,
 * ISR/SDK callback, driver, RX/TX queue or reserved stop storage here. */
class StartupColdStorage {
  public:
    /** @param allocator Copied callback bundle; resources remain borrowed.
     * @param bytes Positive requested bytes. @param alignment Power of two, at least pointer alignment.
     * @param externalAvailable Explicit runtime availability and application opt-in.
     */
    StartupColdStorage(const ColdStorageAllocator& allocator, size_t bytes, size_t alignment,
                       bool externalAvailable) noexcept : allocator_(allocator) {
        if (!allocator_.allocate || !allocator_.release || !bytes || alignment < alignof(void*) ||
            (alignment & (alignment - 1)) || bytes > SIZE_MAX - (alignment - 1))
            return;
        bytes_ = (bytes + alignment - 1) & ~(alignment - 1);
        if (externalAvailable)
            tryAllocate(alignment, true);
        if (!memory_)
            tryAllocate(alignment, false);
    }
    ~StartupColdStorage() noexcept {
        if (memory_)
            allocator_.release(allocator_.context, memory_);
    }
    StartupColdStorage(const StartupColdStorage&) = delete;
    StartupColdStorage& operator=(const StartupColdStorage&) = delete;
    /** @return Borrowed raw block; null on startup refusal. */
    void* data() const noexcept { return memory_; }
    /** @return Rounded allocation size, zero for invalid arguments. */
    size_t bytes() const noexcept { return bytes_; }
    /** @return Allocation branch selected, never hardware qualification. */
    ColdStoragePlacement placement() const noexcept { return placement_; }

  private:
    void tryAllocate(size_t alignment, bool external) noexcept {
        void* candidate = allocator_.allocate(allocator_.context, alignment, bytes_, external);
        if (!candidate)
            return;
        if (reinterpret_cast<uintptr_t>(candidate) & (alignment - 1)) {
            allocator_.release(allocator_.context, candidate);
            return;
        }
        memory_ = candidate;
        placement_ = external ? ColdStoragePlacement::EXTERNAL : ColdStoragePlacement::INTERNAL;
    }
    ColdStorageAllocator allocator_;
    void* memory_ = nullptr;
    size_t bytes_ = 0;
    ColdStoragePlacement placement_ = ColdStoragePlacement::NONE;
};
} // namespace MotorControlCANExample
