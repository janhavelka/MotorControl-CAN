// SPDX-License-Identifier: MIT
// Startup policy only; fake branch selection is never physical PSRAM placement evidence.
#include "StartupColdStorage.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
namespace E = MotorControlCANExample;
static unsigned checks = 0;
#define CHECK(v) do { ++checks; if (!(v)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #v); std::abort(); } } while (false)
namespace {
struct Allocator {
    alignas(64) unsigned char external[256] = {};
    alignas(64) unsigned char internal[256] = {};
    bool externalWorks = true, internalWorks = true, misaligned = false;
    unsigned calls = 0, releases = 0;
    bool branches[2] = {};
    size_t alignments[2] = {}, sizes[2] = {};
    static void* allocate(void* context, size_t alignment, size_t bytes, bool external) {
        Allocator& self = *static_cast<Allocator*>(context);
        CHECK(self.calls < 2);
        self.branches[self.calls] = external;
        self.alignments[self.calls] = alignment;
        self.sizes[self.calls++] = bytes;
        if ((external && !self.externalWorks) || (!external && !self.internalWorks))
            return nullptr;
        return (external ? self.external : self.internal) + (self.misaligned ? 1 : 0);
    }
    static void release(void* context, void* memory) {
        Allocator& self = *static_cast<Allocator*>(context);
        CHECK(memory == self.external || memory == self.internal ||
              memory == self.external + 1 || memory == self.internal + 1);
        ++self.releases;
    }
    E::ColdStorageAllocator callbacks() {
        E::ColdStorageAllocator result;
        result.context = this;
        result.allocate = allocate;
        result.release = release;
        return result;
    }
};
void startupBranchesAndStableStorage() {
    for (unsigned branch = 0; branch < 4; ++branch) {
        Allocator fake;
        fake.externalWorks = branch == 0;
        fake.internalWorks = branch != 3;
        {
            E::StartupColdStorage storage(fake.callbacks(), 129, 64, branch != 2);
            CHECK(storage.bytes() == 192);
            CHECK(fake.calls == (branch == 0 || branch == 2 ? 1u : 2u));
            CHECK(fake.branches[0] == (branch != 2));
            if (fake.calls == 2)
                CHECK(!fake.branches[1]);
            for (unsigned i = 0; i < fake.calls; ++i)
                CHECK(fake.alignments[i] == 64 && fake.sizes[i] == 192);
            if (branch == 3) {
                CHECK(!storage.data() && storage.placement() == E::ColdStoragePlacement::NONE);
            } else {
                CHECK(storage.data() == (branch == 0 ? fake.external : fake.internal));
                CHECK(storage.placement() == (branch == 0 ? E::ColdStoragePlacement::EXTERNAL
                                                        : E::ColdStoragePlacement::INTERNAL));
                std::memset(storage.data(), 0x53, 129);
                for (unsigned n = 0; n < 100; ++n)
                    CHECK(storage.data() == (branch == 0 ? fake.external : fake.internal));
            }
            CHECK(!fake.releases);
        }
        CHECK(fake.releases == (branch == 3 ? 0u : 1u));
    }
}
void invalidInputAndMalformedAllocator() {
    for (unsigned bad = 0; bad < 6; ++bad) {
        Allocator fake;
        E::ColdStorageAllocator callbacks = fake.callbacks();
        const size_t bytes = bad == 0 ? 0 : bad == 3 ? SIZE_MAX : 129;
        const size_t alignment = bad == 1 ? 0 : bad == 2 ? 3 : 64;
        if (bad == 4)
            callbacks.allocate = nullptr;
        if (bad == 5)
            callbacks.release = nullptr;
        E::StartupColdStorage storage(callbacks, bytes, alignment, true);
        CHECK(!storage.data() && !storage.bytes() && !fake.calls && !fake.releases);
    }
    Allocator fake;
    fake.misaligned = true;
    {
        E::StartupColdStorage storage(fake.callbacks(), 128, 64, true);
        CHECK(!storage.data() && fake.calls == 2 && fake.releases == 2);
    }
    CHECK(fake.releases == 2);
}
} // namespace
int main() {
    static_assert(!std::is_copy_constructible<E::StartupColdStorage>::value, "startup owner must not copy");
    static_assert(!std::is_move_constructible<E::StartupColdStorage>::value, "startup storage stays stationary");
    startupBranchesAndStableStorage();
    invalidInputAndMalformedAllocator();
    std::printf("storage placement policy: %u checks; no physical memory placement claim\n", checks);
}
