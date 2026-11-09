// SPDX-License-Identifier: MIT
// The S2 bench heap plan (examples/s2_bench/BenchHeap.h): largest-first reservation over a
// first-fit multi-region allocator, and the shared record arena's occupancy rules. The region
// model uses the S2 numbers measured with the pinned compiler and the image's map; it is a model of
// first-fit placement, not the SDK's TLSF allocator, so the bench's HEAP line remains the proof.
#include "BenchHeap.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
// First-fit across regions in order, each a bump allocator with a per-block header: what matters
// here is only whether a block finds a region with room left.
struct Regions {
    enum { HEADER = 8, CAPACITY = 4 };
    size_t free[CAPACITY] = {};
    unsigned count = 0, calls = 0;
    size_t order[16] = {};
    alignas(16) static unsigned char pool[8];
    static void* allocate(void* context, size_t, size_t size) {
        Regions& r = *static_cast<Regions*>(context);
        r.order[r.calls++] = size;
        for (unsigned i = 0; i < r.count; ++i)
            if (r.free[i] >= size + HEADER) {
                r.free[i] -= size + HEADER;
                return pool; // Only the outcome is modeled; nothing is stored in these blocks.
            }
        return nullptr;
    }
};
alignas(16) unsigned char Regions::pool[8];
// S2, pinned compiler: BenchRoute 42624, AxisSession 35592, RouteWork 7904; the record arena is the
// largest of RawEvidence 22560, PdoExperiment 23688 and the D, K and H records (at most 10168).
enum : size_t { ROUTE = 42624, SESSION = 35592, WORK = 7904, ARENA = 23688, RAW = 22560, PDO = 23688 };
// The S2 image's map: the heap region after .bss is 0x3FFFC000 - 0x3FFDDC20 = 123872 bytes, the
// startup-stack block 0x3FFFC000..0x3FFFFA10 is 14864; about 10 KB of the first goes to the SDK's own
// allocations before the bench's first request (2026-11-09: 59360 free after route and one session).
enum : size_t { FIRST = 123872, STACK_BLOCK = 14864, EARLY = 10356 };

void largestFirstOrderAndRefusals() {
    E::BenchBlock blocks[5];
    const size_t sizes[5] = {ROUTE, SESSION, SESSION, ARENA, WORK};
    // Caller order differs from size order: the allocator must still see the largest first.
    const unsigned caller[5] = {4, 3, 1, 0, 2};
    for (unsigned i = 0; i < 5; ++i) {
        blocks[i].size = sizes[caller[i]];
        blocks[i].alignment = 8;
    }
    Regions r;
    r.count = 1;
    r.free[0] = 1000000;
    E::BenchAllocator a;
    a.context = &r;
    a.allocate = Regions::allocate;
    CHECK(E::reserveLargestFirst(a, blocks, 5) == 0x1f);
    CHECK(r.calls == 5 && r.order[0] == ROUTE && r.order[1] == SESSION && r.order[2] == SESSION &&
          r.order[3] == ARENA && r.order[4] == WORK);
    for (unsigned i = 0; i < 5; ++i)
        CHECK(blocks[i].memory != nullptr);
    // A refused block stays null and the smaller ones are still tried.
    Regions small;
    small.count = 2;
    small.free[0] = 40000; // Too small for the route; holds a session.
    small.free[1] = 24000; // Holds the arena.
    a.context = &small;
    E::BenchBlock two[3];
    two[0].size = ROUTE;
    two[1].size = SESSION;
    two[2].size = ARENA;
    CHECK(E::reserveLargestFirst(a, two, 3) == 0x6);
    CHECK(!two[0].memory && two[1].memory && two[2].memory && small.calls == 3);
    // No allocator, too many blocks or a zero size grant nothing and call nothing.
    E::BenchAllocator none;
    CHECK(E::reserveLargestFirst(none, blocks, 5) == 0);
    E::BenchBlock zero[1];
    small.calls = 0;
    CHECK(E::reserveLargestFirst(a, zero, 1) == 0 && small.calls == 0);
}

// The 2026-11-09 failure, modeled: with RawEvidence and PdoExperiment in .bss, the first region
// holds the route and one session but not the second, in any order; with them moved into the arena,
// the whole reservation fits and leaves room for the lazy soak, watch and capture.
void s2RegionModel() {
    E::BenchAllocator a;
    a.allocate = Regions::allocate;
    {
        Regions head;
        head.count = 2;
        head.free[0] = FIRST - EARLY;
        head.free[1] = STACK_BLOCK;
        a.context = &head;
        E::BenchBlock blocks[3];
        blocks[0].size = ROUTE;
        blocks[1].size = blocks[2].size = SESSION;
        CHECK(E::reserveLargestFirst(a, blocks, 3) == 0x3); // The second session is refused.
    }
    {
        Regions plan;
        plan.count = 2;
        plan.free[0] = FIRST + RAW + PDO - EARLY;
        plan.free[1] = STACK_BLOCK;
        a.context = &plan;
        E::BenchBlock blocks[5];
        const size_t sizes[5] = {ROUTE, SESSION, SESSION, ARENA, WORK};
        for (unsigned i = 0; i < 5; ++i)
            blocks[i].size = sizes[i];
        CHECK(E::reserveLargestFirst(a, blocks, 5) == 0x1f);
        // What the lazy soak (11008), capture (7608) and watch (5152) then find.
        CHECK(plan.free[0] + plan.free[1] >= 11008 + 7608 + 5152 + 3 * Regions::HEADER);
    }
}

struct Counted {
    static unsigned built, destroyed;
    unsigned value;
    explicit Counted(unsigned v) : value(v) { ++built; }
    ~Counted() { ++destroyed; }
};
unsigned Counted::built = 0, Counted::destroyed = 0;
struct Other {
    static unsigned destroyed;
    char bytes[24] = {};
    ~Other() { ++destroyed; }
};
unsigned Other::destroyed = 0;
struct Large {
    char bytes[128];
};

void arenaOccupancy() {
    alignas(16) static unsigned char memory[64];
    E::BenchArena empty;
    CHECK(!empty.acquire<Counted>(1, true, 5u) && empty.kind() == 0 && empty.capacity() == 0);
    E::BenchArena arena;
    arena.adopt(memory, sizeof memory);
    arena.adopt(nullptr, 0); // Adopted once: a later call changes nothing.
    CHECK(arena.capacity() == sizeof memory);
    CHECK(!arena.acquire<Counted>(0, true, 1u)); // Kind zero means none.
    CHECK(!arena.acquire<Large>(3, true));       // Larger than the block.
    Counted* first = arena.acquire<Counted>(1, true, 7u);
    CHECK(first && first->value == 7 && Counted::built == 1 && arena.kind() == 1);
    // The same kind returns the same occupant without constructing it again.
    CHECK(arena.acquire<Counted>(1, false, 9u) == first && first->value == 7 && Counted::built == 1);
    CHECK(arena.get<Counted>(1) == first && !arena.get<Other>(2) && !arena.get<Counted>(0));
    // An occupant that holds a record is never replaced.
    CHECK(!arena.acquire<Other>(2, false) && arena.kind() == 1 && Counted::destroyed == 0);
    // An idle one is destroyed first, then the new kind takes the block.
    Other* second = arena.acquire<Other>(2, true);
    CHECK(second && arena.kind() == 2 && Counted::destroyed == 1 && !arena.get<Counted>(1));
    CHECK(static_cast<void*>(second) == static_cast<void*>(memory));
    Counted* third = arena.acquire<Counted>(1, true, 3u);
    CHECK(third && third->value == 3 && Other::destroyed == 1 && Counted::built == 2);
}
} // namespace
int main() {
    largestFirstOrderAndRefusals();
    s2RegionModel();
    arenaOccupancy();
    std::printf("bench heap: %u checks passed\n", checks);
}
