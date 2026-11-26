// SPDX-License-Identifier: MIT
#include "EspProbeMain.h"
#include "EspProbePlatform.h"
#include "EspRecordStore.h"
#include "EspTransceiverPin.h"
#include "IdfTwaiDriver.h"
#include "ProbeConsole.h"
#include "SelectedAxis.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include <atomic>
#include <new>

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
// Prompt 57: the console's regular commands and persistence act on the first selected axis. Its storage
// (session, motion and commissioning helpers, about 60 KB) is placed in internal RAM on that first
// selection, never at boot, and never freed; when it cannot be placed those commands keep refusing.
struct SelectedDispatch {
    SelectedAxis* axis = nullptr;
    const MotorControlCAN::CANOPEN::NodeSnapshot* node = nullptr; ///< Its node, captured on attachment.
    uint32_t emcyBaseline = 0;                                    ///< That node's EMCY count then.
};
// The application's maybe-fault decision for the attached axis, as the bench console's: more than one
// EMCY since attachment, or one with a nonzero error code. A fault refuses the motion helper's downloads.
bool attachedFault(const SelectedDispatch& d) {
    return !d.node ||
           (d.node->emcyCount != d.emcyBaseline &&
            (d.node->emcyCount != d.emcyBaseline + 1 || d.node->emcy.errorCode != 0));
}
void ownerTask(void*) {
    // Lifetime continues even after failed initialization/deletion. SDK callback
    // storage never resides in optional PSRAM or on a returning task stack.
    static DRAM_ATTR MotorControlCAN::CANOPEN::SdoClient services;
    static DRAM_ATTR IdfTwaiDriver driver;
    static DRAM_ATTR EspProbePlatform console;
    static DRAM_ATTR AcquisitionRecord acquisitionStorage;
    // Prompt 56: TX done and RX end console.idle()'s wait early; nothing else on this task takes
    // notifications.
    driver.setOwnerWake(true);
    static DRAM_ATTR FirmwareProbe firmware(services, driver.driver(), console.callbacks(),
                                            &acquisitionStorage, espSilentPin());
    static DRAM_ATTR ProbeConsole commands(firmware.app(), firmware.owner(), firmware.platform());
    static DRAM_ATTR ProbeDevice device(firmware.app(), firmware.owner(), services,
                                        &firmware.evidence());
    static DRAM_ATTR ProbeSync sync(firmware.owner());
    static DRAM_ATTR GroupStopMember groupRows[MotorControlCAN::CANOPEN::SdoClient::CHANNELS];
    static DRAM_ATTR GroupStop groupStop(firmware.app(), firmware.owner(), groupRows,
                                          MotorControlCAN::CANOPEN::SdoClient::CHANNELS);
    commands.attachSync(sync);
    commands.attachGroupStop(groupStop);
    commands.attachDevice(device);
    ConsoleBusSource bus; // `drv bus`: the TWAI error counters, bus evidence.
    bus.context = &firmware;
    bus.read = readConsoleBus;
    commands.attachBus(bus);
    // Prompt 58: record lines in NVS (Preferences in an Arduino build), on `record save` and `record restore`
    // only; attaching touches no flash and nothing is restored at boot.
    static EspRecordStore recordStore;
    commands.attachRecordStore(recordStore.store());
    // Every ordinary download is the attached axis's or refused unsent: no other producer downloads here.
    static DRAM_ATTR SelectedDispatch selected;
    DownloadPreflight preflight;
    preflight.context = &selected;
    preflight.check = [](void* context, MotorControlCANOwner::SequenceId sequence,
                         const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t now) {
        SelectedDispatch& d = *static_cast<SelectedDispatch*>(context);
        return d.axis && d.axis->ownsDownload(sequence, transfer) &&
               d.axis->preflight(sequence, transfer, now, attachedFault(d));
    };
    firmware.setPreflight(preflight);
    bool selectedRefused = false;
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
            if (firmware.platform().nowUs(now)) {
                commands.service(now);
                if (!selected.axis && !selectedRefused && firmware.app().selected()) {
                    void* memory = heap_caps_aligned_alloc(alignof(SelectedAxis), sizeof(SelectedAxis),
                                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                    if (memory) {
                        selected.axis = new (memory) SelectedAxis(firmware.app(), firmware.owner(), services,
                                                                 commands, MotorControlCAN::LICHUAN_C::SettingsModel::CL86_C);
                    } else {
                        selectedRefused = true;
                        write(firmware, "MotorControl-CAN: no memory for the selected axis; its commands refuse.\n");
                    }
                }
                if (selected.axis && !selected.axis->attached() && selected.axis->follow(now)) {
                    selected.node = device.node();
                    selected.emcyBaseline = selected.node ? selected.node->emcyCount : 0;
                }
            }
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
