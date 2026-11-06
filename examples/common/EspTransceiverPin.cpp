// SPDX-License-Identifier: MIT
#include "EspTransceiverPin.h"
#include "driver/gpio.h"

namespace MotorControlCANExample {
namespace {
bool drive(void*, int gpio, bool high) {
    // The SDK capability macros shift by the pin number: bound it before evaluating them.
    if (gpio < 0 || gpio >= GPIO_NUM_MAX || !GPIO_IS_VALID_OUTPUT_GPIO(gpio))
        return false;
    const gpio_num_t pin = static_cast<gpio_num_t>(gpio);
    // Latch first. gpio_set_direction then routes plain GPIO output to the pad and
    // drives this level; the reverse order would briefly drive the reset value.
    return gpio_set_level(pin, high ? 1u : 0u) == ESP_OK &&
           gpio_set_direction(pin, GPIO_MODE_OUTPUT) == ESP_OK;
}
} // namespace
SilentPin espSilentPin() noexcept {
    SilentPin result;
    result.drive = drive;
    return result;
}
} // namespace MotorControlCANExample
