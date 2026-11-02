/** @file EspProbePlatform.h
 * @brief Example-only ESP32-S2 UART and ESP32-S3 USB time/byte I/O.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "ProbePlatform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace MotorControlCANExample {
/** Cached diagnostics; no field proves host delivery or complete ingress. */
struct EspProbeDiagnostics {
    int lastError = 0; ///< Raw latest configuration/install/uninstall error; initially zero.
    uint32_t writeRefused = 0; ///< Zero-byte SDK write attempts, saturating at UINT32_MAX.
    bool clockFailed = false;  ///< Latched negative or backward ESP timer observation.
};
/** Stationary example platform owned by one pinned task/core. Construction has no
 * I/O. Explicit begin installs one SDK console driver; explicit stop must succeed
 * before releasing this object. A failed stop preserves ownership for a later
 * same-task retry. No destructor performs teardown. SDK-owned rings allocate at
 * begin; this class creates no task and never receives CAN. S3 reserves GPIO19/20
 * for USB. S2 exclusively owns UART0 on GPIO43 TX / GPIO44 RX at 115200 8N1;
 * SDK console is disabled and no other UART0 user is permitted. All calls and
 * cached observers require owner serialization.
 */
class EspProbePlatform {
  public:
    /** Construct inactive without I/O, clock access or SDK allocation. */
    EspProbePlatform() noexcept = default;
    EspProbePlatform(const EspProbePlatform&) = delete; ///< Borrowed callback identity cannot move.
    EspProbePlatform& operator=(const EspProbePlatform&) = delete; ///< No ownership transfer.
    /** Attempt initialization once in this object's lifetime. Requires a pinned
     * current task and no installed console driver. S3 masks inherited USB
     * interrupt enables without clearing status, then installs 1024-byte TX and
     * 256-byte RX SDK rings. S2 configures UART0 with a 256-byte RX ring and no
     * TX ring or event queue. Does not wait for a host or send device traffic.
     * @return True only when the SDK driver was installed by this object.
     */
    bool begin() noexcept;
    /** @return Byte/clock callbacks borrowing this stationary object. Each byte
     * call makes one SDK attempt without waiting for data or FIFO space. The S2
     * SDK TX mutex is uncontended under exclusive same-task UART0 ownership.
     * nowUs publishes monotonic
     * nonnegative microseconds; zero/equal values are valid. Negative/backward
     * observations latch failure and preserve output. Read failure preserves its
     * output. Write true means copied into the S3 SDK ring or S2 UART FIFO;
     * physical transmission and host delivery are not established. RX overflow
     * is not fully observed; this console is not a lossless evidence channel.
     * idle is null: the caller invokes idle() after application and TWAI service.
     * Callbacks require successful begin and the same pinned task/core.
     */
    ProbeCallbacks callbacks() noexcept;
    /** Yield exactly one scheduler tick on the owning task after application and
     * TWAI service; no console read/write/flush or CAN access. Before successful
     * begin or on a different task/core this is a no-op. Not an elapsed-time bound.
     */
    void idle() noexcept;
    /** Explicitly uninstall on the same pinned task/core.
     * @return True after successful uninstall or when nothing is owned. False
     * preserves driver ownership/storage; do not destroy the object or callbacks.
     * No CAN recovery, device command or automatic initialization follows.
     */
    bool stop() noexcept;
    /** @return Cached diagnostics copy only; no clock, console or reset operation. */
    EspProbeDiagnostics diagnostics() const noexcept { return diagnostics_; }

  private:
    TaskHandle_t task_ = nullptr;
    int core_ = -1;
    bool attempted_ = false, ownsDriver_ = false, haveTime_ = false;
    uint64_t lastTimeUs_ = 0;
    EspProbeDiagnostics diagnostics_;
    bool taskValid() const noexcept;
    bool nowUs(uint64_t&) noexcept;
    bool readByte(uint8_t&) noexcept;
    bool writeByte(uint8_t) noexcept;
};
} // namespace MotorControlCANExample
