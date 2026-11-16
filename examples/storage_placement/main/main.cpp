// SPDX-License-Identifier: MIT
// Compile fixture only. It never starts a controller, task, receive loop or motor operation.
#include "StartupColdStorage.h"
#include "IdfTwaiDriver.h"
#include <MotorControlCAN/profiles/lichuan_c/Actions.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_psram.h"
#include "sdkconfig.h"
#include <cstdio>
#include <cstdlib>
#include <new>

namespace E = MotorControlCANExample;
namespace M = MotorControlCAN;
namespace {
// These are actual driver and operation types. None is allocated through the cold allocator.
struct HotStorage {
    E::IdfTwaiDriver driver;
    MotorControlCANOwner::ReceivedFrame receive[E::IdfTwaiDriver::RX_CAPACITY];
    M::Frame transmit[4];
    M::LICHUAN_C::ActionContext reservedStops[2];
    StaticTask_t taskStorage;
    StackType_t taskStack[1024];
};
static DRAM_ATTR HotStorage hot;
// Standalone archival bytes, intentionally disconnected from live driver/owner/stop state.
struct ColdHistory {
    uint32_t used = 0;
    uint8_t bytes[32768] = {};
};
void* allocate(void*, size_t alignment, size_t bytes, bool external) {
    const uint32_t caps = MALLOC_CAP_8BIT | (external ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL);
    return heap_caps_aligned_alloc(alignment, bytes, caps);
}
void release(void*, void* memory) { heap_caps_free(memory); }
E::ColdStorageAllocator allocator() {
    E::ColdStorageAllocator result;
    result.allocate = allocate;
    result.release = release;
    return result;
}
bool externalAvailable() {
#if CONFIG_SPIRAM
    return esp_psram_is_initialized();
#else
    return false;
#endif
}
} // namespace
extern "C" void app_main() {
    // Initialization happens once in app_main, after SDK heap/optional PSRAM setup. No static
    // constructor requests external memory before initialization and no control path allocates it.
    static E::StartupColdStorage storage(allocator(), sizeof(ColdHistory), alignof(void*), externalAvailable());
    if (!storage.data() || !esp_ptr_internal(&hot) ||
        !esp_ptr_internal(reinterpret_cast<uint8_t*>(&hot) + sizeof hot - 1))
        std::abort();
    const bool external = storage.placement() == E::ColdStoragePlacement::EXTERNAL;
    if (external ? !esp_ptr_external_ram(storage.data()) : !esp_ptr_internal(storage.data()))
        std::abort();
    ColdHistory* history = new (storage.data()) ColdHistory();
    history->bytes[0] = 0x53;
    history->used = 1;
    std::printf("STORAGE hot=%u cold=%u allocated=%u placement=%u psram=%u used=%lu\n",
                unsigned(sizeof hot), unsigned(sizeof(ColdHistory)), unsigned(storage.bytes()),
                unsigned(storage.placement()), unsigned(externalAvailable()),
                static_cast<unsigned long>(history->used));
    // Storage lasts for this fixture's lifetime. Runtime branches above require a real board run;
    // successfully linking this source establishes neither allocation success nor placement.
}
