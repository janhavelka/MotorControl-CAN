// SPDX-License-Identifier: MIT
#include "EspProbeMain.h"
#include "EspProbePlatform.h"
#include "EspTransceiverPin.h"
#include "IdfTwaiDriver.h"
#include "ProbeConsole.h"
#include "esp_attr.h"
#include "sdkconfig.h"
#include <atomic>

#if !configSUPPORT_STATIC_ALLOCATION || portNUM_PROCESSORS < 1
#error "This example requires static task allocation and an owner core"
#endif
// The console platform's core rule, checked in each real target build.
#if CONFIG_IDF_TARGET_ESP32S2
static_assert(MotorControlCANExample::FIRMWARE_OWNER_CORE == 0, "S2 owner uses its sole core");
#elif CONFIG_IDF_TARGET_ESP32S3
static_assert(MotorControlCANExample::FIRMWARE_OWNER_CORE == 1, "S3 owner keeps core 1");
#endif

namespace MotorControlCANExample {
namespace {
DRAM_ATTR StackType_t stack[FIRMWARE_STACK_BYTES / sizeof(StackType_t)];
DRAM_ATTR StaticTask_t taskState;
DRAM_ATTR FirmwareConfig configuration;
FirmwareTurnHook turnHook;
std::atomic<FirmwareBootStatus> boot(FirmwareBootStatus::NOT_STARTED);
bool attempted = false;
void write(FirmwareProbe& firmware, const char* message) {
    size_t length = 0;
    while (message[length])
        ++length; // Fixed bounded messages, each shorter than 96 bytes.
    firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(message), length);
}
void ownerTask(void*) {
    // Lifetime continues even after failed initialization/deletion. SDK callback
    // storage never resides in optional PSRAM or on a returning task stack.
    static DRAM_ATTR MotorControlCAN::CANOPEN::SdoClient services;
    static DRAM_ATTR IdfTwaiDriver driver;
    static DRAM_ATTR EspProbePlatform console;
    static DRAM_ATTR FirmwareProbe firmware(services, driver.driver(), console.callbacks(),
                                            espSilentPin());
    static DRAM_ATTR ProbeConsole commands(firmware.app(), firmware.owner(), firmware.platform());
    // Silence a configured transceiver before anything else; the controller stays closed.
    firmware.prepare(configuration);
    if (firmware.prepared()) {
        HostInterfaceFacts facts;
        facts.interfaceKnown = true;
        facts.interfaceId = 0; // This assembly's one local physical interface.
        facts.bitrateKnown = configuration.controller.bitrate != 0;
        facts.bitrateBitsPerSecond = configuration.controller.bitrate;
        firmware.app().setHostInterfaceFacts(facts);
    }
    const bool platformReady = console.begin();
    FirmwareBootStatus status = firmwareStatus(firmware, platformReady);
    boot.store(status);
    write(firmware,
          status == FirmwareBootStatus::INACTIVE
              ? "MotorControl-CAN: board accepted; CAN inactive until explicit start.\n"
          : status == FirmwareBootStatus::CONFIGURATION_REQUIRED
              ? "MotorControl-CAN: compile fixture; CAN disabled, configuration required.\n"
          : status == FirmwareBootStatus::TRANSCEIVER_FAILED
              ? "MotorControl-CAN: transceiver silent level refused; CAN disabled.\n"
          : status == FirmwareBootStatus::PLATFORM_FAILED
              ? "MotorControl-CAN: platform refused; CAN disabled.\n"
              : "MotorControl-CAN: board configuration refused; CAN disabled.\n");
    bool failureReported = false;
    for (;;) {
        firmware.service();
        if (turnHook.turn)
            turnHook.turn(turnHook.context, firmware);
        else {
            // Test hooks own their input exclusively. Ordinary Arduino/IDF entries
            // share this console, including while CAN remains explicitly inactive.
            uint64_t now = 0;
            if (firmware.platform().nowUs(now))
                commands.service(now);
        }
        // Explicit start/stop happen only through the hook; startup itself never moves on.
        const FirmwareBootStatus next = firmwareStatus(firmware, platformReady);
        if (next != status) {
            status = next;
            boot.store(status);
        }
        if (firmware.progressionFailed() && !failureReported) {
            failureReported = true;
            write(firmware,
                  firmware.failure() == FirmwareFailure::CLOCK
                      ? "MotorControl-CAN: clock fault; controller inhibited.\n"
                  : firmware.failure() == FirmwareFailure::APPLICATION
                      ? "MotorControl-CAN: application service rejected; controller inhibited.\n"
                      : "MotorControl-CAN: evidence service rejected; controller inhibited.\n");
        }
        // ProbeCallbacks.idle is null. Scheduling follows owner + transport.
        if (platformReady)
            console.idle();
        else
            vTaskDelay(1); // Failed console setup must not create a busy loop.
    }
}
} // namespace
bool startProbeFirmware(const FirmwareConfig& config, const FirmwareTurnHook& hook) noexcept {
    if (attempted)
        return false;
    attempted = true;
    configuration = config;
    turnHook = hook;
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
