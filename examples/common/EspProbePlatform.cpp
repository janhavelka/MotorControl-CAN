// SPDX-License-Identifier: MIT
#include "EspProbePlatform.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S2
#include "driver/uart.h"
#else
#include "driver/usb_serial_jtag.h"
#include "hal/usb_serial_jtag_ll.h"
#endif

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 5)
#error "Console platform requires reviewed ESP-IDF 5.5.5"
#endif
#if (!CONFIG_IDF_TARGET_ESP32S3 && !CONFIG_IDF_TARGET_ESP32S2) || CONFIG_FREERTOS_SMP
#error "Console platform requires reviewed ESP32-S2/S3 non-SMP scheduler"
#endif
#if CONFIG_IDF_TARGET_ESP32S2 && !CONFIG_ESP_CONSOLE_NONE
#error "S2 UART0 platform requires the SDK console disabled for exclusive ownership"
#endif

namespace MotorControlCANExample {
bool EspProbePlatform::taskValid() const noexcept {
    return task_ && xTaskGetCurrentTaskHandle() == task_ && xTaskGetCoreID(task_) == core_ &&
           xPortGetCoreID() == core_;
}
bool EspProbePlatform::begin() noexcept {
    if (attempted_)
        return false;
    attempted_ = true;
    const auto task = xTaskGetCurrentTaskHandle();
    if (!task)
        return false;
    const int core = xTaskGetCoreID(task);
    if (core < 0 || core >= portNUM_PROCESSORS || xPortGetCoreID() != core)
        return false;
    task_ = task;
    core_ = core;
#if CONFIG_IDF_TARGET_ESP32S2
    if (uart_is_driver_installed(UART_NUM_0))
        return false;
    uart_config_t config = {};
    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    diagnostics_.lastError = uart_param_config(UART_NUM_0, &config);
    if (diagnostics_.lastError != ESP_OK)
        return false;
    diagnostics_.lastError =
        uart_set_pin(UART_NUM_0, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (diagnostics_.lastError != ESP_OK)
        return false;
    // No TX ring: uart_tx_chars accepts only currently available FIFO capacity.
    diagnostics_.lastError = uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0);
#else
    if (usb_serial_jtag_is_driver_installed())
        return false;
    usb_serial_jtag_driver_config_t config = {};
    config.tx_buffer_size = 1024;
    config.rx_buffer_size = 256;
    // CPU resets can retain HWCDC BUS_RESET enable, which this driver does not
    // service. Preserve status/FIFO contents, including initial TX-empty status.
    usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    diagnostics_.lastError = usb_serial_jtag_driver_install(&config);
#endif
    ownsDriver_ = diagnostics_.lastError == ESP_OK;
    return ownsDriver_;
}
ProbeCallbacks EspProbePlatform::callbacks() noexcept {
    ProbeCallbacks result;
    result.context = this;
    result.nowUs = [](void* p, uint64_t& output) {
        return static_cast<EspProbePlatform*>(p)->nowUs(output);
    };
    result.readByte = [](void* p, uint8_t& output) {
        return static_cast<EspProbePlatform*>(p)->readByte(output);
    };
    result.writeByte = [](void* p, uint8_t byte) {
        return static_cast<EspProbePlatform*>(p)->writeByte(byte);
    };
    return result;
}
bool EspProbePlatform::nowUs(uint64_t& output) noexcept {
    if (!ownsDriver_ || !taskValid() || diagnostics_.clockFailed)
        return false;
    const int64_t sample = esp_timer_get_time();
    if (sample < 0 || (haveTime_ && static_cast<uint64_t>(sample) < lastTimeUs_)) {
        diagnostics_.clockFailed = true;
        return false;
    }
    lastTimeUs_ = static_cast<uint64_t>(sample);
    haveTime_ = true;
    output = lastTimeUs_;
    return true;
}
bool EspProbePlatform::readByte(uint8_t& output) noexcept {
    if (!ownsDriver_ || !taskValid())
        return false;
    uint8_t byte = 0;
#if CONFIG_IDF_TARGET_ESP32S2
    if (uart_read_bytes(UART_NUM_0, &byte, 1, 0) != 1)
#else
    if (usb_serial_jtag_read_bytes(&byte, 1, 0) != 1)
#endif
        return false;
    output = byte;
    return true;
}
bool EspProbePlatform::writeByte(uint8_t byte) noexcept {
    if (!ownsDriver_ || !taskValid())
        return false;
#if CONFIG_IDF_TARGET_ESP32S2
    // Only this task uses UART0. Its SDK TX mutex is therefore uncontended;
    // this call never waits for FIFO space or physical transmission completion.
    if (uart_tx_chars(UART_NUM_0, reinterpret_cast<const char*>(&byte), 1) == 1)
#else
    if (usb_serial_jtag_write_bytes(&byte, 1, 0) == 1)
#endif
        return true;
    if (diagnostics_.writeRefused != UINT32_MAX)
        ++diagnostics_.writeRefused;
    return false;
}
void EspProbePlatform::idle() noexcept {
    if (ownsDriver_ && taskValid())
        vTaskDelay(1);
}
bool EspProbePlatform::stop() noexcept {
    if (!ownsDriver_)
        return true;
    if (!taskValid())
        return false;
#if CONFIG_IDF_TARGET_ESP32S2
    diagnostics_.lastError = uart_driver_delete(UART_NUM_0);
#else
    diagnostics_.lastError = usb_serial_jtag_driver_uninstall();
#endif
    if (diagnostics_.lastError != ESP_OK)
        return false;
    ownsDriver_ = false;
    return true;
}
} // namespace MotorControlCANExample
