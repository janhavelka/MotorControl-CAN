/** @file FirmwareBoard.h
 * @brief Compile fixture versus explicitly supplied controller configuration for the
 * current target. Include only from a firmware entry compiled with sdkconfig.h.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "FirmwareProbe.h"
#include "esp_bit_defs.h" // soc_caps.h masks use BIT22 etc. without including it.
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "soc/spi_pins.h"
#include "soc/uart_pins.h" // Includes soc/io_mux_reg.h, which also names the S3 USB pads.

#if CONFIG_IDF_TARGET_ESP32S2
/** Console and system pin policy of the target this entry is compiled for. */
#define MOTORCONTROLCAN_FIRMWARE_TARGET ::MotorControlCANExample::FirmwareTarget::ESP32S2
#elif CONFIG_IDF_TARGET_ESP32S3
/** Console and system pin policy of the target this entry is compiled for. */
#define MOTORCONTROLCAN_FIRMWARE_TARGET ::MotorControlCANExample::FirmwareTarget::ESP32S3
#else
#error "Shared firmware board policy covers only reviewed ESP32-S2 and ESP32-S3 consoles"
#endif

#ifndef MOTORCONTROLCAN_COMPILE_FIXTURE
#error "Select compile-only fixture or explicitly supply controller configuration"
#endif

namespace MotorControlCANExample {
/** Compile-time board checks. Capability bits come from the pinned SDK's own soc_caps.h
 * for this target; the reservations come from the shared firmwareConsolePin and
 * firmwareSystemPin, whose literals are checked here against the SDK's pin constants. */
namespace board {
/** @param pin GPIO number. @return True when the SDK lists pin as a GPIO of this target. */
constexpr bool sdkGpio(int pin) {
    return pin >= 0 && pin < SOC_GPIO_PIN_COUNT && ((SOC_GPIO_VALID_GPIO_MASK >> pin) & 1) != 0;
}
/** @param pin GPIO number. @return True when the SDK lists pin as an output of this target. */
constexpr bool sdkOutput(int pin) {
    return pin >= 0 && pin < SOC_GPIO_PIN_COUNT &&
           ((SOC_GPIO_VALID_OUTPUT_GPIO_MASK >> pin) & 1) != 0;
}
/** @param pin GPIO number. @return True when pin is this target's console GPIO. */
constexpr bool consolePin(int pin) {
    return firmwareConsolePin(MOTORCONTROLCAN_FIRMWARE_TARGET, pin);
}
/** @param pin GPIO number. @return True when the chip drives pin itself (flash, ROM print). */
constexpr bool systemPin(int pin) {
    return firmwareSystemPin(MOTORCONTROLCAN_FIRMWARE_TARGET, pin);
}
/** @param pin GPIO number. @return True when pin is neither a console nor a system pin. */
constexpr bool freePin(int pin) {
    return !consolePin(pin) && !systemPin(pin);
}
// Checked in every real build, compile fixtures included. Over-reservation is the safe
// side, so only the SDK's named pins are required to be reserved.
#if CONFIG_IDF_TARGET_ESP32S2
static_assert(consolePin(U0TXD_GPIO_NUM) && consolePin(U0RXD_GPIO_NUM),
              "S2 console reservation must be the SDK's UART0 pins");
#else
static_assert(consolePin(USB_INT_PHY0_DM_GPIO_NUM) && consolePin(USB_INT_PHY0_DP_GPIO_NUM),
              "S3 console reservation must be the SDK's USB Serial/JTAG pads");
static_assert(systemPin(U0TXD_GPIO_NUM), "S3 ROM boot-message pin must be the SDK's U0TXD");
#endif
static_assert(systemPin(MSPI_IOMUX_PIN_NUM_CS1) && systemPin(MSPI_IOMUX_PIN_NUM_HD) &&
                  systemPin(MSPI_IOMUX_PIN_NUM_WP) && systemPin(MSPI_IOMUX_PIN_NUM_CS0) &&
                  systemPin(MSPI_IOMUX_PIN_NUM_CLK) && systemPin(MSPI_IOMUX_PIN_NUM_MISO) &&
                  systemPin(MSPI_IOMUX_PIN_NUM_MOSI),
              "Flash-bus reservation must cover the SDK's MSPI IOMUX pins");
} // namespace board
} // namespace MotorControlCANExample

#if !MOTORCONTROLCAN_COMPILE_FIXTURE
#if !defined(MOTORCONTROLCAN_CAN_TX_PIN) || !defined(MOTORCONTROLCAN_CAN_RX_PIN) ||                \
    !defined(MOTORCONTROLCAN_CAN_BITRATE) || !defined(MOTORCONTROLCAN_CAN_RX_DEPTH)
#error "Supply CAN TX/RX GPIOs, bitrate and RX depth for the actual board/transceiver"
#endif
#ifndef MOTORCONTROLCAN_CAN_SILENT_PIN
/** Transceiver silent-input GPIO; -1 means the board has none under application control. */
#define MOTORCONTROLCAN_CAN_SILENT_PIN -1
#endif
#ifndef MOTORCONTROLCAN_CAN_SILENT_LEVEL
/** Level (0 or 1) that silences the transceiver; -1 unknown, refused with a silent pin. */
#define MOTORCONTROLCAN_CAN_SILENT_LEVEL -1
#endif
static_assert(::MotorControlCANExample::board::sdkOutput(MOTORCONTROLCAN_CAN_TX_PIN) &&
                  ::MotorControlCANExample::board::sdkGpio(MOTORCONTROLCAN_CAN_RX_PIN),
              "CAN TX must be an SDK output GPIO and RX an SDK GPIO of this target");
static_assert(MOTORCONTROLCAN_CAN_TX_PIN != MOTORCONTROLCAN_CAN_RX_PIN,
              "CAN TX and RX must differ");
static_assert(::MotorControlCANExample::board::freePin(MOTORCONTROLCAN_CAN_TX_PIN) &&
                  ::MotorControlCANExample::board::freePin(MOTORCONTROLCAN_CAN_RX_PIN),
              "CAN pins cannot use this target's console, flash-bus or boot-output GPIOs");
static_assert(MOTORCONTROLCAN_CAN_BITRATE > 0 && MOTORCONTROLCAN_CAN_RX_DEPTH >= 1 &&
                  MOTORCONTROLCAN_CAN_RX_DEPTH <= 16,
              "Supply a nonzero bitrate and an RX depth of 1..16");
static_assert(MOTORCONTROLCAN_CAN_SILENT_PIN == -1 ||
                  (::MotorControlCANExample::board::sdkOutput(MOTORCONTROLCAN_CAN_SILENT_PIN) &&
                   ::MotorControlCANExample::board::freePin(MOTORCONTROLCAN_CAN_SILENT_PIN) &&
                   MOTORCONTROLCAN_CAN_SILENT_PIN != MOTORCONTROLCAN_CAN_TX_PIN &&
                   MOTORCONTROLCAN_CAN_SILENT_PIN != MOTORCONTROLCAN_CAN_RX_PIN),
              "A silent pin must be a free SDK output GPIO distinct from CAN TX/RX");
static_assert(MOTORCONTROLCAN_CAN_SILENT_PIN == -1
                  ? MOTORCONTROLCAN_CAN_SILENT_LEVEL == -1
                  : MOTORCONTROLCAN_CAN_SILENT_LEVEL == 0 || MOTORCONTROLCAN_CAN_SILENT_LEVEL == 1,
              "A silent pin needs its explicit silent level 0 or 1, and no pin needs none");
#endif

namespace MotorControlCANExample {
/** Build configuration only, without probing a board or opening a controller.
 * @return The compile fixture's disabled configuration, or the explicitly supplied
 * local facts for this target. No target node or live device evidence exists.
 */
inline FirmwareConfig firmwareBoardConfig() noexcept {
    FirmwareConfig config;
    config.target = MOTORCONTROLCAN_FIRMWARE_TARGET;
#if !MOTORCONTROLCAN_COMPILE_FIXTURE
    config.configured = true;
    config.controller.txPin = MOTORCONTROLCAN_CAN_TX_PIN;
    config.controller.rxPin = MOTORCONTROLCAN_CAN_RX_PIN;
    config.controller.bitrate = MOTORCONTROLCAN_CAN_BITRATE;
    config.controller.sdkTxQueueDepth = 1;
    config.controller.rxQueueDepth = MOTORCONTROLCAN_CAN_RX_DEPTH;
    config.transceiver.silentPin = MOTORCONTROLCAN_CAN_SILENT_PIN;
    config.transceiver.silentLevel = MOTORCONTROLCAN_CAN_SILENT_LEVEL == 1   ? SilentLevel::HIGH
                                     : MOTORCONTROLCAN_CAN_SILENT_LEVEL == 0 ? SilentLevel::LOW
                                                                             : SilentLevel::NONE;
#endif
    return config;
}
} // namespace MotorControlCANExample
