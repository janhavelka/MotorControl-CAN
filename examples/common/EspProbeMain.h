/** @file EspProbeMain.h
 * @brief One static pinned firmware task shared by Arduino and ESP-IDF entry points.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "FirmwareProbe.h"
#include "freertos/FreeRTOS.h"

namespace MotorControlCANExample {
enum {
    FIRMWARE_STACK_BYTES =
        12288, ///< Internal static task stack; S2 acquisition call-chain evidence in bench report.
    FIRMWARE_OWNER_CORE =
        portNUM_PROCESSORS > 1 ? 1 : 0 ///< One pinned owner; S2 uses its sole core.
};
/** Create the one static pinned task; no controller initialization in the caller.
 * The task first runs FirmwareProbe::prepare (board check, silent level), then the
 * console, then the service loop. Startup never starts the controller: an explicit
 * FirmwareProbe::start from the hook (or a later console) does.
 * @param config Copied local configuration; default disables CAN.
 * @param hook Copied optional owner-task work after each turn; default none.
 * @return True only if task creation succeeded; inspect firmwareBootStatus for
 * asynchronous startup. Call once from the serialized startup context; concurrent
 * startup callers are not supported. Never flashes or issues device work.
 */
bool startProbeFirmware(const FirmwareConfig& config,
                        const FirmwareTurnHook& hook = FirmwareTurnHook()) noexcept;
/** @return Atomic cached status (FirmwareBootStatus in FirmwareProbe.h), updated by the
 * owner task after every turn through firmwareStatus; performs no I/O or clock acquisition. */
FirmwareBootStatus firmwareBootStatus() noexcept;
} // namespace MotorControlCANExample
