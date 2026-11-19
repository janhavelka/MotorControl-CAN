// SPDX-License-Identifier: MIT
// Actual platform implementation against narrow SDK fakes; no SDK/hardware I/O.
#include "EspProbePlatform.h"
#include "driver/usb_serial_jtag.h"
#include "hal/usb_serial_jtag_ll.h"
#include <cstdio>
#include <cstdlib>

namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
struct FakeSdk {
    TaskHandle_t task = reinterpret_cast<TaskHandle_t>(1);
    int core = 1, taskCore = 1;
    bool installed = false, input = false, busy = false;
    int installError = 0, uninstallError = 0;
    int64_t time = 0;
    unsigned installs = 0, uninstalls = 0, reads = 0, writes = 0, clocks = 0, idles = 0;
    unsigned queries = 0, masks = 0;
    unsigned polls = 0, takes = 0, delays = 0; // idles counts the two blocking waits.
    uint32_t notified = 0;                     // ulTaskNotifyTake's result: a pending notification.
    uint8_t inputByte = 0, outputByte = 0;
    uint32_t mask = 0;
    usb_serial_jtag_driver_config_t config = {};
} sdk;
void reset() { sdk = FakeSdk(); }
unsigned ioCalls() { return sdk.reads + sdk.writes + sdk.clocks + sdk.queries; }

void startupAndLifecycle() {
    reset();
    E::EspProbePlatform platform;
    auto callbacks = platform.callbacks();
    CHECK(ioCalls() == 0 && sdk.installs == 0 && sdk.masks == 0);
    CHECK(callbacks.context == &platform && callbacks.nowUs && callbacks.readByte &&
          callbacks.writeByte && callbacks.idle == nullptr);
    // Prompt 56: the cycle counter for the pass-cost diagnostics advances between two reads.
    CHECK(callbacks.cycles);
    const uint32_t firstCycles = callbacks.cycles(callbacks.context);
    CHECK(callbacks.cycles(callbacks.context) - firstCycles == 3);
    uint64_t time = 91;
    uint8_t byte = 92;
    CHECK(!callbacks.nowUs(callbacks.context, time) && time == 91);
    CHECK(!callbacks.readByte(callbacks.context, byte) && byte == 92);
    CHECK(!callbacks.writeByte(callbacks.context, byte));
    platform.idle();
    CHECK(sdk.idles == 0 && ioCalls() == 0);
    CHECK(platform.stop() && sdk.uninstalls == 0);
    CHECK(platform.begin());
    CHECK(sdk.installs == 1 && sdk.masks == 1 && sdk.mask == USB_SERIAL_JTAG_LL_INTR_MASK);
    CHECK(sdk.config.tx_buffer_size == 1024 && sdk.config.rx_buffer_size == 256);
    CHECK(!platform.begin() && sdk.installs == 1);
    const unsigned beforeIdle = ioCalls();
    platform.idle();
    CHECK(sdk.idles == 1 && ioCalls() == beforeIdle);
    const unsigned beforeDiagnostics = ioCalls();
    CHECK(platform.diagnostics().lastError == 0 && !platform.diagnostics().clockFailed);
    CHECK(ioCalls() == beforeDiagnostics);
    sdk.task = reinterpret_cast<TaskHandle_t>(2);
    CHECK(!platform.stop() && sdk.uninstalls == 0);
    CHECK(!callbacks.nowUs(callbacks.context, time) && time == 91);
    CHECK(!callbacks.readByte(callbacks.context, byte) && byte == 92);
    CHECK(!callbacks.writeByte(callbacks.context, byte));
    platform.idle();
    CHECK(sdk.idles == 1 && ioCalls() == beforeIdle);
    sdk.task = reinterpret_cast<TaskHandle_t>(1);
    sdk.core = 0;
    CHECK(!platform.stop() && sdk.uninstalls == 0);
    sdk.core = 1;
    sdk.taskCore = -1;
    CHECK(!platform.stop() && sdk.uninstalls == 0);
    sdk.taskCore = 1;
    sdk.uninstallError = 73;
    CHECK(!platform.stop() && sdk.installed && sdk.uninstalls == 1);
    CHECK(platform.diagnostics().lastError == 73);
    sdk.uninstallError = 0;
    CHECK(platform.stop() && !sdk.installed && sdk.uninstalls == 2);
    CHECK(platform.diagnostics().lastError == 0);
    CHECK(platform.stop() && sdk.uninstalls == 2);
    CHECK(!platform.begin() && sdk.installs == 1);
    CHECK(!callbacks.nowUs(callbacks.context, time) && time == 91);
    CHECK(!callbacks.readByte(callbacks.context, byte) && byte == 92);
    CHECK(!callbacks.writeByte(callbacks.context, byte));
}
void initializationFailures() {
    reset();
    sdk.installed = true;
    E::EspProbePlatform external;
    CHECK(!external.begin() && sdk.installs == 0 && sdk.masks == 0);
    CHECK(external.stop() && sdk.uninstalls == 0 && sdk.installed);
    reset();
    sdk.installError = 61;
    E::EspProbePlatform failed;
    CHECK(!failed.begin() && sdk.installs == 1 && !sdk.installed);
    CHECK(failed.diagnostics().lastError == 61);
    sdk.installError = 0;
    CHECK(!failed.begin() && sdk.installs == 1);
    CHECK(failed.stop() && sdk.uninstalls == 0);
    for (int invalid = 0; invalid < 4; ++invalid) {
        reset();
        if (invalid == 0)
            sdk.task = nullptr;
        else if (invalid == 1)
            sdk.taskCore = -1;
        else if (invalid == 2)
            sdk.taskCore = 2;
        else
            sdk.core = 0;
        E::EspProbePlatform unpinned;
        CHECK(!unpinned.begin() && sdk.queries == 0 && sdk.installs == 0 && sdk.masks == 0);
        sdk.task = reinterpret_cast<TaskHandle_t>(1);
        sdk.taskCore = sdk.core = 1;
        CHECK(!unpinned.begin() && sdk.installs == 0);
    }
}
void clockEvidence() {
    reset();
    E::EspProbePlatform platform;
    CHECK(platform.begin());
    auto callbacks = platform.callbacks();
    uint64_t output = 77;
    CHECK(callbacks.nowUs(callbacks.context, output) && output == 0);
    CHECK(callbacks.nowUs(callbacks.context, output) && output == 0);
    sdk.time = UINT32_MAX;
    CHECK(callbacks.nowUs(callbacks.context, output) && output == UINT32_MAX);
    ++sdk.time;
    CHECK(callbacks.nowUs(callbacks.context, output) && output == uint64_t(UINT32_MAX) + 1);
    sdk.time = INT64_MAX;
    CHECK(callbacks.nowUs(callbacks.context, output) && output == uint64_t(INT64_MAX));
    --sdk.time;
    CHECK(!callbacks.nowUs(callbacks.context, output) && output == uint64_t(INT64_MAX));
    CHECK(platform.diagnostics().clockFailed);
    const unsigned before = sdk.clocks;
    sdk.time = INT64_MAX;
    CHECK(!callbacks.nowUs(callbacks.context, output) && sdk.clocks == before);
    CHECK(platform.stop());
    reset();
    E::EspProbePlatform negative;
    CHECK(negative.begin());
    callbacks = negative.callbacks();
    sdk.time = -1;
    output = 88;
    CHECK(!callbacks.nowUs(callbacks.context, output) && output == 88);
    CHECK(negative.diagnostics().clockFailed);
    sdk.time = 0;
    CHECK(!callbacks.nowUs(callbacks.context, output) && output == 88 && sdk.clocks == 1);
    CHECK(negative.stop());
}
void bytesUnderPressure() {
    reset();
    E::EspProbePlatform platform;
    CHECK(platform.begin());
    auto callbacks = platform.callbacks();
    uint8_t byte = 91;
    CHECK(!callbacks.readByte(callbacks.context, byte) && byte == 91 && sdk.reads == 1);
    sdk.input = true;
    sdk.inputByte = 0;
    CHECK(callbacks.readByte(callbacks.context, byte) && byte == 0 && sdk.reads == 2);
    CHECK(!callbacks.readByte(callbacks.context, byte) && byte == 0 && sdk.reads == 3);
    CHECK(callbacks.writeByte(callbacks.context, 255) && sdk.outputByte == 255 && sdk.writes == 1);
    sdk.busy = true; // Includes an absent host whose bounded SDK TX ring filled.
    for (unsigned i = 0; i < 1000; ++i)
        CHECK(!callbacks.writeByte(callbacks.context, 17));
    CHECK(sdk.writes == 1001 && platform.diagnostics().writeRefused == 1000);
    CHECK(sdk.outputByte == 255);
    sdk.busy = false;
    CHECK(callbacks.writeByte(callbacks.context, 0) && sdk.outputByte == 0);
    CHECK(platform.diagnostics().writeRefused == 1000);
    CHECK(platform.stop());
}
} // namespace

TaskHandle_t xTaskGetCurrentTaskHandle() {
    return sdk.task;
}
int xTaskGetCoreID(TaskHandle_t task) {
    CHECK(task == sdk.task);
    return sdk.taskCore;
}
int xPortGetCoreID() {
    return sdk.core;
}
void vTaskDelay(TickType_t ticks) {
    CHECK(ticks == 1);
    ++sdk.idles;
    ++sdk.delays;
}
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t ticks) {
    CHECK(clear == pdTRUE && ticks <= 1); // A poll, or at most one tick; the count is consumed.
    if (ticks == 0) {
        ++sdk.polls;
        return sdk.notified;
    }
    ++sdk.idles;
    ++sdk.takes;
    return sdk.notified;
}
int64_t esp_timer_get_time() {
    ++sdk.clocks;
    return sdk.time;
}
bool usb_serial_jtag_is_driver_installed() {
    ++sdk.queries;
    return sdk.installed;
}
void usb_serial_jtag_ll_disable_intr_mask(uint32_t mask) {
    CHECK(sdk.installs == 0 && !sdk.installed);
    ++sdk.masks;
    sdk.mask = mask;
}
esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t* config) {
    CHECK(config && sdk.masks == 1 && !sdk.installed);
    ++sdk.installs;
    sdk.config = *config;
    sdk.installed = sdk.installError == 0;
    return sdk.installError;
}
esp_err_t usb_serial_jtag_driver_uninstall() {
    CHECK(sdk.installed);
    ++sdk.uninstalls;
    if (!sdk.uninstallError)
        sdk.installed = false;
    return sdk.uninstallError;
}
int usb_serial_jtag_read_bytes(void* output, uint32_t length, TickType_t ticks) {
    CHECK(output && length == 1 && ticks == 0 && sdk.installed);
    ++sdk.reads;
    if (!sdk.input)
        return 0;
    *static_cast<uint8_t*>(output) = sdk.inputByte;
    sdk.input = false;
    return 1;
}
int usb_serial_jtag_write_bytes(const void* bytes, size_t length, TickType_t ticks) {
    CHECK(bytes && length == 1 && ticks == 0 && sdk.installed);
    ++sdk.writes;
    if (sdk.busy)
        return 0;
    sdk.outputByte = *static_cast<const uint8_t*>(bytes);
    return 1;
}
// Prompt 56: with nothing pending idle blocks at most one tick. A notification pending after a
// pass starts the next pass at once, but every UNBLOCKED_PASSES passes without blocking end in a
// one-tick sleep. The bound uses no tick count, so it holds however long a pass is.
void idleWakeIsBounded() {
    reset();
    E::EspProbePlatform platform;
    CHECK(platform.begin());
    const unsigned limit = E::EspProbePlatform::UNBLOCKED_PASSES;
    sdk.notified = 0;
    platform.idle();
    CHECK(sdk.polls == 1 && sdk.takes == 1 && sdk.delays == 0 && sdk.idles == 1);
    sdk.notified = 1; // An event during every pass.
    unsigned blocked = 0;
    for (unsigned pass = 1; pass <= 3 * limit; ++pass) {
        const unsigned before = sdk.idles;
        platform.idle();
        if (sdk.idles != before)
            ++blocked;
        CHECK(blocked == pass / limit); // A sleep exactly every limit passes.
    }
    CHECK(sdk.delays == 3 && sdk.takes == 1);
    // A blocking wait restarts the count.
    platform.idle();
    platform.idle();
    sdk.notified = 0;
    platform.idle(); // Blocks: nothing pending.
    sdk.notified = 1;
    const unsigned delays = sdk.delays;
    for (unsigned i = 0; i + 1 < limit; ++i)
        platform.idle();
    CHECK(sdk.delays == delays);
    platform.idle();
    CHECK(sdk.delays == delays + 1);
    CHECK(platform.stop());
}
int main() {
    startupAndLifecycle();
    idleWakeIsBounded();
    initializationFailures();
    clockEvidence();
    bytesUnderPressure();
    std::printf("ESP platform: %u checks passed; %u object bytes on this ABI; no SDK or hardware.\n",
                checks, static_cast<unsigned>(sizeof(E::EspProbePlatform)));
}
