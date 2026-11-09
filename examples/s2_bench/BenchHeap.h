/** @file BenchHeap.h
 * @brief The bench image's permanent heap plan: one reservation pass at boot, largest block first,
 * and one shared arena for the records that never coexist.
 * SPDX-License-Identifier: MIT
 * Application only: no I/O, clock or SDK call; the caller supplies the allocator. Nothing here
 * frees memory. On the S2 the internal heap is a few regions, and the SDK allocates first-fit
 * across them, so a large block allocated after smaller ones can find no region left that holds
 * it. Reserving the large, always-needed blocks first, before the console, the controller or any
 * command allocates, keeps the layout of every boot the same and measurable.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <new>

namespace MotorControlCANExample {
/** Caller's aligned allocator; a null result is a refused allocation, never retried here. */
struct BenchAllocator {
    void* context = nullptr; ///< Borrowed by allocate.
    void* (*allocate)(void* context, size_t alignment, size_t size) = nullptr; ///< Null refuses all.
};
/** One requested permanent block. */
struct BenchBlock {
    size_t size = 0;        ///< Bytes; zero is never allocated.
    size_t alignment = 0;   ///< Power of two required by the type placed there.
    void* memory = nullptr; ///< Set by reserveLargestFirst; null when refused.
};
/** Allocate each block once, the largest first and, for equal sizes, in the given order.
 * @param allocator Caller's allocator. @param blocks Requests; memory is set for each one granted.
 * @param count Number of blocks, at most 32.
 * @return Bit i set when blocks[i] was allocated. A refusal leaves that block null and continues
 * with the next smaller one, which may still fit elsewhere.
 */
inline uint32_t reserveLargestFirst(const BenchAllocator& allocator, BenchBlock* blocks,
                                    unsigned count) noexcept {
    uint32_t granted = 0, done = 0;
    if (!allocator.allocate || !blocks || count > 32)
        return 0;
    for (unsigned pass = 0; pass < count; ++pass) {
        unsigned next = count;
        for (unsigned i = 0; i < count; ++i)
            if (!(done >> i & 1u) && (next == count || blocks[i].size > blocks[next].size))
                next = i;
        done |= 1u << next;
        BenchBlock& b = blocks[next];
        b.memory = b.size ? allocator.allocate(allocator.context, b.alignment, b.size) : nullptr;
        if (b.memory)
            granted |= 1u << next;
    }
    return granted;
}
/** One block that holds at most one of several record types at a time. The bench admits a new
 * record only when none is held, so these records never coexist; sharing one block saves the
 * sum of the others. A kind is the caller's nonzero type tag. Replacing the occupant destroys it,
 * so its per-object state (for example a last-operation check) starts again with the next one.
 */
class BenchArena {
  public:
    BenchArena() noexcept = default;
    BenchArena(const BenchArena&) = delete;
    BenchArena& operator=(const BenchArena&) = delete;
    /** Adopt reserved memory once. @param memory Aligned for every type placed here; null keeps
     * the arena empty. @param capacity Its bytes. */
    void adopt(void* memory, size_t capacity) noexcept {
        if (!memory_) {
            memory_ = memory;
            capacity_ = memory ? capacity : 0;
        }
    }
    /** @return Adopted capacity in bytes, zero without memory. */
    size_t capacity() const noexcept { return capacity_; }
    /** @return The current occupant's kind, zero for none. */
    unsigned kind() const noexcept { return kind_; }
    /** @param kind Type tag. @return The occupant when it is of this kind, else null. */
    template <class T> T* get(unsigned kind) const noexcept {
        return kind && kind_ == kind ? static_cast<T*>(memory_) : nullptr;
    }
    /** Return the occupant of this kind, constructing it in place when the arena holds another.
     * @param kind Nonzero type tag of T. @param previousIdle The caller's statement that the
     * current occupant holds no record; false refuses any replacement.
     * @param args Constructor arguments of T, forwarded only when T is constructed.
     * @return The occupant, or null without memory, with too small a capacity, for kind zero or
     * while a held occupant of another kind would have to be replaced.
     */
    template <class T, class... Args> T* acquire(unsigned kind, bool previousIdle, Args&&... args) noexcept {
        if (!memory_ || !kind || sizeof(T) > capacity_)
            return nullptr;
        if (kind_ == kind)
            return static_cast<T*>(memory_);
        if (kind_ && !previousIdle)
            return nullptr;
        if (destroy_)
            destroy_(memory_);
        kind_ = 0;
        destroy_ = nullptr;
        T* occupant = new (memory_) T(static_cast<Args&&>(args)...);
        kind_ = kind;
        destroy_ = &destroyAs<T>;
        return occupant;
    }

  private:
    template <class T> static void destroyAs(void* object) noexcept { static_cast<T*>(object)->~T(); }
    void* memory_ = nullptr;
    size_t capacity_ = 0;
    unsigned kind_ = 0;
    void (*destroy_)(void*) = nullptr;
};
} // namespace MotorControlCANExample
