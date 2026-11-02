// SPDX-License-Identifier: MIT
#include "EspProbeMain.h"
#include "EspProbePlatform.h"
#include "IdfTwaiDriver.h"
#include "esp_attr.h"
#include <atomic>

#if !configSUPPORT_STATIC_ALLOCATION || portNUM_PROCESSORS < 1
#error "This example requires static task allocation and an owner core"
#endif

namespace MotorControlCANExample {
namespace {
DRAM_ATTR StackType_t stack[FIRMWARE_STACK_BYTES / sizeof(StackType_t)];
DRAM_ATTR StaticTask_t taskState;
DRAM_ATTR FirmwareConfig configuration;
std::atomic<FirmwareBootStatus> boot(FirmwareBootStatus::NOT_STARTED);
bool attempted = false;
void ownerTask(void*) {
    // Lifetime continues even after failed initialization/deletion. SDK callback
    // storage never resides in optional PSRAM or on a returning task stack.
    static DRAM_ATTR MotorControlCAN::CANOPEN::SdoClient services;
    static DRAM_ATTR IdfTwaiDriver driver;
    static DRAM_ATTR EspProbePlatform console;
    static DRAM_ATTR FirmwareProbe firmware(services, driver.driver(), console.callbacks());
    const bool platformReady = console.begin();
    const bool controllerReady = platformReady && firmware.begin(configuration);
    boot.store(!platformReady              ? FirmwareBootStatus::PLATFORM_FAILED
               : !configuration.configured ? FirmwareBootStatus::CONFIGURATION_REQUIRED
               : controllerReady           ? FirmwareBootStatus::RUNNING
                                           : FirmwareBootStatus::CONTROLLER_FAILED);
    const char ready[] = "MotorControl-CAN: local controller ready; no target or request.\n";
    const char disabled[] =
        "MotorControl-CAN: compile fixture; CAN disabled, configuration required.\n";
    const char failed[] = "MotorControl-CAN: local initialization failed; no retry.\n";
    const char* message = controllerReady ? ready : !configuration.configured ? disabled : failed;
    size_t length = 0;
    while (message[length])
        ++length; // Three fixed bounded messages, each shorter than 96 bytes.
    firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(message), length);
    bool clockReported = false;
    for (;;) {
        firmware.service();
        if (firmware.clockFailed() && !clockReported) {
            clockReported = true;
            boot.store(FirmwareBootStatus::CLOCK_FAILED);
            const uint8_t fault[] = "MotorControl-CAN: clock fault; controller inhibited.\n";
            firmware.platform().writeOutput(fault, sizeof(fault) - 1);
        }
        // ProbeCallbacks.idle is null. Scheduling follows owner + transport.
        if (platformReady)
            console.idle();
        else
            vTaskDelay(1); // Failed console setup must not create a busy loop.
    }
}
} // namespace
bool startProbeFirmware(const FirmwareConfig& config) noexcept {
    if (attempted)
        return false;
    attempted = true;
    configuration = config;
    if (!xTaskCreateStaticPinnedToCore(ownerTask, "can_owner", sizeof(stack), nullptr, 1, stack,
                                       &taskState, FIRMWARE_OWNER_CORE)) {
        boot.store(FirmwareBootStatus::TASK_FAILED);
        return false;
    }
    return true;
}
FirmwareBootStatus firmwareBootStatus() noexcept {
    return boot.load();
}
} // namespace MotorControlCANExample
