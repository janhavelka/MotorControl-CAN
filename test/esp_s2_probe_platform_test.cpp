// SPDX-License-Identifier: MIT
// Exercise the actual S2 branch; these are SDK fakes, not hardware evidence.
#include "EspProbePlatform.h"
#include "driver/uart.h"
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
    int core = 0, taskCore = 0, paramError = 0, pinError = 0, installError = 0, deleteError = 0;
    unsigned params = 0, pins = 0, installs = 0, deletes = 0, reads = 0, writes = 0, idles = 0;
    bool installed = false, input = false, full = false;
    uint8_t inputByte = 0, outputByte = 0;
    int64_t time = 0;
} sdk;
void reset() {
    sdk = FakeSdk();
}
void lifecycleAndFailures() {
    for (unsigned failure = 0; failure < 5; ++failure) {
        reset();
        if (failure == 0)
            sdk.installed = true;
        if (failure == 1)
            sdk.paramError = 61;
        if (failure == 2)
            sdk.pinError = 62;
        if (failure == 3)
            sdk.installError = 63;
        E::EspProbePlatform platform;
        CHECK(!sdk.params && !sdk.installs);
        const bool success = failure == 4;
        CHECK(platform.begin() == success);
        CHECK(sdk.params == (failure == 0 ? 0u : 1u));
        CHECK(sdk.pins == (failure < 2 ? 0u : 1u));
        CHECK(sdk.installs == (failure < 3 ? 0u : 1u));
        CHECK(platform.diagnostics().lastError == (failure == 1   ? 61
                                                   : failure == 2 ? 62
                                                   : failure == 3 ? 63
                                                                  : 0));
        CHECK(!platform.begin());
        if (success) {
            sdk.deleteError = 64;
            CHECK(!platform.stop() && sdk.installed);
            CHECK(platform.diagnostics().lastError == 64);
            sdk.deleteError = 0;
            CHECK(platform.stop() && !sdk.installed && sdk.deletes == 2);
        } else
            CHECK(platform.stop() && !sdk.deletes);
        CHECK(platform.stop());
    }
}
void bytesTimeAndOwner() {
    reset();
    E::EspProbePlatform platform;
    const auto cb = platform.callbacks();
    uint64_t time = 42;
    uint8_t byte = 99;
    CHECK(!cb.nowUs(cb.context, time) && time == 42);
    CHECK(!cb.readByte(cb.context, byte) && byte == 99);
    CHECK(!cb.writeByte(cb.context, byte) && !sdk.writes);
    CHECK(cb.idle == nullptr && platform.begin());
    CHECK(!cb.readByte(cb.context, byte) && byte == 99);
    sdk.input = true;
    sdk.inputByte = 0;
    CHECK(cb.readByte(cb.context, byte) && byte == 0);
    sdk.full = true;
    CHECK(!cb.writeByte(cb.context, 91) && platform.diagnostics().writeRefused == 1);
    sdk.full = false;
    CHECK(cb.writeByte(cb.context, 0) && sdk.outputByte == 0);
    CHECK(cb.nowUs(cb.context, time) && time == 0);
    sdk.time = 5;
    CHECK(cb.nowUs(cb.context, time) && time == 5);
    CHECK(cb.nowUs(cb.context, time) && time == 5);
    sdk.task = reinterpret_cast<TaskHandle_t>(2);
    const unsigned reads = sdk.reads, writes = sdk.writes;
    CHECK(!cb.readByte(cb.context, byte) && !cb.writeByte(cb.context, byte));
    CHECK(!platform.stop() && !sdk.deletes);
    platform.idle();
    CHECK(!sdk.idles && reads == sdk.reads && writes == sdk.writes);
    sdk.task = reinterpret_cast<TaskHandle_t>(1);
    sdk.core = 1;
    CHECK(!cb.nowUs(cb.context, time) && time == 5);
    sdk.core = 0;
    sdk.taskCore = -1;
    CHECK(!platform.stop());
    sdk.taskCore = 0;
    platform.idle();
    CHECK(sdk.idles == 1);
    sdk.time = 4;
    CHECK(!cb.nowUs(cb.context, time) && time == 5 && platform.diagnostics().clockFailed);
    sdk.time = 6;
    CHECK(!cb.nowUs(cb.context, time) && time == 5);
    CHECK(platform.stop());
}
void invalidTask() {
    for (unsigned bad = 0; bad < 3; ++bad) {
        reset();
        if (bad == 0)
            sdk.task = nullptr;
        if (bad == 1)
            sdk.core = 1;
        if (bad == 2)
            sdk.taskCore = 1;
        E::EspProbePlatform platform;
        CHECK(!platform.begin() && !sdk.params && !sdk.installs);
    }
}
void negativeClock() {
    reset();
    E::EspProbePlatform platform;
    CHECK(platform.begin());
    const auto cb = platform.callbacks();
    sdk.time = -1;
    uint64_t output = 72;
    CHECK(!cb.nowUs(cb.context, output) && output == 72);
    CHECK(platform.diagnostics().clockFailed);
    sdk.time = 0;
    CHECK(!cb.nowUs(cb.context, output) && output == 72);
    CHECK(platform.stop());
}
} // namespace
TaskHandle_t xTaskGetCurrentTaskHandle() {
    return sdk.task;
}
int xTaskGetCoreID(TaskHandle_t) {
    return sdk.taskCore;
}
int xPortGetCoreID() {
    return sdk.core;
}
void vTaskDelay(TickType_t ticks) {
    CHECK(ticks == 1);
    ++sdk.idles;
}
int64_t esp_timer_get_time() {
    return sdk.time;
}
bool uart_is_driver_installed(uart_port_t port) {
    CHECK(port == UART_NUM_0);
    return sdk.installed;
}
esp_err_t uart_param_config(uart_port_t port, const uart_config_t* c) {
    CHECK(port == UART_NUM_0 && c && c->baud_rate == 115200 && c->data_bits == UART_DATA_8_BITS &&
          c->parity == UART_PARITY_DISABLE && c->stop_bits == UART_STOP_BITS_1 &&
          c->flow_ctrl == UART_HW_FLOWCTRL_DISABLE && c->source_clk == UART_SCLK_DEFAULT);
    ++sdk.params;
    return sdk.paramError;
}
esp_err_t uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts) {
    CHECK(port == UART_NUM_0 && tx == 43 && rx == 44 && rts == UART_PIN_NO_CHANGE &&
          cts == UART_PIN_NO_CHANGE);
    ++sdk.pins;
    return sdk.pinError;
}
esp_err_t uart_driver_install(uart_port_t port, int rx, int tx, int queue, void* events,
                              int flags) {
    CHECK(port == UART_NUM_0 && rx == 256 && tx == 0 && queue == 0 && !events && flags == 0);
    ++sdk.installs;
    sdk.installed = !sdk.installError;
    return sdk.installError;
}
esp_err_t uart_driver_delete(uart_port_t port) {
    CHECK(port == UART_NUM_0 && sdk.installed);
    ++sdk.deletes;
    if (!sdk.deleteError)
        sdk.installed = false;
    return sdk.deleteError;
}
int uart_read_bytes(uart_port_t port, void* out, uint32_t length, TickType_t ticks) {
    CHECK(port == UART_NUM_0 && sdk.installed && length == 1 && ticks == 0);
    ++sdk.reads;
    if (!sdk.input)
        return 0;
    *static_cast<uint8_t*>(out) = sdk.inputByte;
    sdk.input = false;
    return 1;
}
int uart_tx_chars(uart_port_t port, const char* bytes, uint32_t length) {
    CHECK(port == UART_NUM_0 && sdk.installed && length == 1);
    ++sdk.writes;
    if (sdk.full)
        return 0;
    sdk.outputByte = static_cast<uint8_t>(*bytes);
    return 1;
}
int main() {
    lifecycleAndFailures();
    bytesTimeAndOwner();
    invalidTask();
    negativeClock();
    std::printf("S2 platform: %u checks; SDK fake only\n", checks);
}
