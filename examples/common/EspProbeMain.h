/** @file EspProbeMain.h
 * @brief One static pinned firmware task shared by Arduino and ESP-IDF entry points.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "FirmwareProbe.h"

namespace MotorControlCANExample {
/** Local startup diagnostics only, never drive/communication health. */
enum class FirmwareBootStatus : uint8_t {
    NOT_STARTED,            ///< No task creation attempted.
    TASK_FAILED,            ///< Static task could not be created; controller was not initialized.
    PLATFORM_FAILED,        ///< USB/platform initialization failed; controller was not initialized.
    CONFIGURATION_REQUIRED, ///< Compile fixture; USB available, CAN disabled.
    CONTROLLER_FAILED,      ///< Local initialization failed; no automatic retry.
    RUNNING,     ///< Shared local service loop running; no selected drive/request implied.
    CLOCK_FAILED ///< Clock/progression failure inhibited work and attempted retirement.
};
enum {
    FIRMWARE_STACK_BYTES =
        8192,               ///< Internal static task stack; runtime high-water mark unmeasured.
    FIRMWARE_OWNER_CORE = 1 ///< Reviewed S3 allocation/retirement core, shared across frameworks.
};
/** Create the one static pinned task; no controller initialization in the caller.
 * @param config Copied local configuration; default disables CAN.
 * @return True only if task creation succeeded; inspect firmwareBootStatus for
 * asynchronous startup. Call once from the serialized startup context; concurrent
 * startup callers are not supported. Never flashes or issues device work.
 */
bool startProbeFirmware(const FirmwareConfig& config) noexcept;
/** @return Atomic cached startup state; performs no I/O or clock acquisition. */
FirmwareBootStatus firmwareBootStatus() noexcept;
} // namespace MotorControlCANExample
