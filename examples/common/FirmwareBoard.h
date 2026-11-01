/** @file FirmwareBoard.h
 * @brief Compile fixture versus explicitly supplied S3 controller configuration.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "FirmwareProbe.h"

#ifndef MOTORCONTROLCAN_COMPILE_FIXTURE
#error "Select compile-only fixture or explicitly supply controller configuration"
#endif
#if !MOTORCONTROLCAN_COMPILE_FIXTURE
#if !defined(MOTORCONTROLCAN_CAN_TX_PIN) || !defined(MOTORCONTROLCAN_CAN_RX_PIN) ||                \
    !defined(MOTORCONTROLCAN_CAN_BITRATE) || !defined(MOTORCONTROLCAN_CAN_RX_DEPTH)
#error "Supply CAN TX/RX GPIOs, bitrate and RX depth for the actual board/transceiver"
#endif
static_assert(MOTORCONTROLCAN_CAN_TX_PIN >= 0 && MOTORCONTROLCAN_CAN_RX_PIN >= 0 &&
                  MOTORCONTROLCAN_CAN_TX_PIN != MOTORCONTROLCAN_CAN_RX_PIN &&
                  MOTORCONTROLCAN_CAN_TX_PIN != 19 && MOTORCONTROLCAN_CAN_TX_PIN != 20 &&
                  MOTORCONTROLCAN_CAN_RX_PIN != 19 && MOTORCONTROLCAN_CAN_RX_PIN != 20 &&
                  MOTORCONTROLCAN_CAN_BITRATE > 0 && MOTORCONTROLCAN_CAN_RX_DEPTH >= 1 &&
                  MOTORCONTROLCAN_CAN_RX_DEPTH <= 16,
              "Invalid S3 USB/CAN configuration");
#endif

namespace MotorControlCANExample {
/** Build configuration only, without probing a board or opening a controller.
 * @return Disabled invalid-pin configuration for the compile fixture; otherwise
 * explicitly supplied local facts. No target node or live device evidence exists.
 */
inline FirmwareConfig firmwareBoardConfig() noexcept {
    FirmwareConfig config;
#if !MOTORCONTROLCAN_COMPILE_FIXTURE
    config.configured = true;
    config.controller.txPin = MOTORCONTROLCAN_CAN_TX_PIN;
    config.controller.rxPin = MOTORCONTROLCAN_CAN_RX_PIN;
    config.controller.bitrate = MOTORCONTROLCAN_CAN_BITRATE;
    config.controller.sdkTxQueueDepth = 1;
    config.controller.rxQueueDepth = MOTORCONTROLCAN_CAN_RX_DEPTH;
#endif
    return config;
}
} // namespace MotorControlCANExample
