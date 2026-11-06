/** @file EspTransceiverPin.h
 * @brief ESP-IDF GPIO drive for an application-owned CAN transceiver silent input.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "FirmwareProbe.h"

namespace MotorControlCANExample {
/** SilentPin callbacks over the pinned ESP-IDF GPIO driver; no context, no I/O on return.
 * Each drive call checks the SDK output capability, latches the requested level and only
 * then enables the output, so the pin never drives its reset level first. It configures
 * a plain push-pull output with input disabled: the level is requested, not read back,
 * and nothing here observes the transceiver mode or the bus. Call it only from the task
 * that owns the firmware assembly; it never touches the CAN controller.
 * @return Callbacks valid for the program lifetime.
 */
SilentPin espSilentPin() noexcept;
} // namespace MotorControlCANExample
