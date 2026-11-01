/** @file TwaiAdapter.h
 * @brief Bounded application transport policy for the pinned TWAI node driver.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "CanBusOwner.h"

namespace MotorControlCANExample {
/** Explicit controller facts; no board, pin or bitrate defaults. */
struct TwaiConfig {
    int txPin = -1;       ///< Application-established transceiver TX GPIO.
    int rxPin = -1;       ///< Application-established transceiver RX GPIO, distinct from TX.
    uint32_t bitrate = 0; ///< Classical bits/second; SDK must accept the timing.
    uint16_t samplePointPermill = 0; ///< Explicit timing selection; zero asks SDK to calculate.
    uint8_t sdkTxQueueDepth = 0;     ///< Must be one; adapter never queues a second SDK frame.
    uint8_t rxQueueDepth = 0;        ///< Application-selected copied RX capacity, 1..16.
};
/** Atomically copied ISR evidence, reset on successful controller destruction. */
struct TwaiEvents {
    bool txDone = false;      ///< One exact final report, never enqueue acceptance.
    bool transmitted = false; ///< Successful hardware transmission for serial.
    uint32_t serial = 0;      ///< Adapter handoff serial, copied before SDK submission.
    bool busOff = false;      ///< Sticky bus-off transition, independent of txDone.
    bool fault = false;       ///< Lost/corrupt callback correlation; requires explicit recovery.
    uint32_t rxDropped =
        0; ///< Saturating observed ring/format drops, not hidden hardware overruns.
    uint32_t busErrors = 0;  ///< Saturating raw SDK error callbacks, not failed logical services.
    uint32_t errorFlags = 0; ///< Bitwise union of raw SDK error flags in this snapshot.
    bool rxPending = false;  ///< Unread authoritative RX exists at this snapshot; defer new TX.
};
/** Small projection of the selected SDK API, injectable for host tests.
 * RX/TX use zero-wait calls in the sole cooperative owner task. Each lifecycle
 * call performs one finite SDK operation without an application wait/retry loop;
 * SDK allocation/internal locks have no established elapsed-time bound. Driver
 * storage is stationary and outlives the adapter. Successful stop destroys the
 * old node, retires callbacks/TX/RX/reports and permits buffer reuse. Failure
 * retains all potentially borrowed storage and inhibits transmission.
 */
struct TwaiDriver {
    void* context = nullptr; ///< Stationary driver storage, internal RAM for the SDK shim.
    bool (*start)(void*, const TwaiConfig&, uint32_t) = nullptr; ///< Create fresh node for epoch.
    bool (*stop)(void*) =
        nullptr; ///< No future old TX/callbacks only on true; never re-enable old node.
    bool (*submit)(void*, const MotorControlCAN::Frame&, uint32_t) = nullptr;
    ///< Copy frame; one outstanding. False proves no borrowing, future TX or callback for this attempt.
    TwaiEvents (*events)(void*) = nullptr; ///< One bounded atomic mailbox/counter snapshot.
    bool (*receive)(void*,
                    ReceivedFrame&) = nullptr; ///< One copied ingress stamped at ISR enqueue.
};
/** Cached adapter diagnostics; no hardware queries and no inferred wire completion. */
struct TwaiDiagnostics {
    uint32_t rxDropped = 0; ///< Observed software/invalid-frame drops, saturating.
    uint32_t busErrors = 0; ///< Raw error callback count, saturating.
    uint32_t errorFlags =
        0; ///< Lifetime bitwise union of raw SDK error flags; busErrors qualifies it.
    bool inhibited = true; ///< Local transport cannot accept new work.
};
/** Application-only bounded TWAI transport; no installed core dependency on an SDK.
 * Construct without I/O, then explicitly start. Call service(owner, nowUs) after
 * owner.service at the same supplied monotonic microseconds. It copies ISR facts,
 * reports controller faults outside owner callbacks, and submits at most one
 * queued frame after rechecking the same SDO store. Owner remains sole RX router.
 * No automatic retry/recovery, clocks, device commands or heap allocation here.
 */
class TwaiAdapter {
  public:
    enum {
        TX_CAPACITY = 2 ///< Total waiting/active/report slots; no unread report eviction.
    };
    /** Borrow stationary resources; construction performs no I/O.
     * @param services Same exclusive service store as the CAN owner.
     * @param driver Copied callbacks; context outlives this adapter and its owner.
     */
    TwaiAdapter(MotorControlCAN::CANOPEN::SdoClient& services, TwaiDriver driver) noexcept;
    TwaiAdapter(const TwaiAdapter&) = delete;            ///< Stationary callback context.
    TwaiAdapter& operator=(const TwaiAdapter&) = delete; ///< No ownership transfer.
    /** One initialization attempt per adapter lifetime; later restart requires owner recovery.
     * Failure inhibits and requires shutdown/recovery. Shutdown does not permit a
     * second start with old owner freshness or epochs.
     * @param config Copied only if structurally valid; pins/bitrate supplied by application.
     * @param epoch Nonzero owner transport epoch; no remote freshness claim.
     * @return True only after SDK initialization; sends no device request.
     */
    bool start(const TwaiConfig& config, uint32_t epoch = 1) noexcept;
    /** Explicit destruction/retirement.
     * @return True only after retirement; false preserves borrowed storage and inhibition.
     */
    bool shutdown() noexcept;
    /** @return Copied callbacks borrowing this stationary adapter for owner lifetime. */
    Transport transport() noexcept;
    /** Bounded cooperative progression, supplied time only; backward time inhibits.
     * @param owner Sole owner using transport() and the same service storage.
     * @param nowUs Nondecreasing monotonic microseconds, zero valid.
     */
    void service(CanBusOwner& owner, uint64_t nowUs) noexcept;
    /** @return Cached saturating diagnostics, without I/O. */
    TwaiDiagnostics diagnostics() const noexcept { return diagnostics_; }

  private:
    enum class State : uint8_t { FREE, QUEUED, ACTIVE, REPORT };
    struct Slot {
        State state = State::FREE;
        MotorControlCAN::Frame frame;
        TxReport report;
        uint32_t serial = 0;
    } slots_[TX_CAPACITY];
    MotorControlCAN::CANOPEN::SdoClient& services_;
    TwaiDriver driver_;
    TwaiConfig config_;
    TwaiDiagnostics diagnostics_;
    uint32_t epoch_ = 0, serial_ = 0, recoveryEpoch_ = 0;
    uint64_t now_ = 0;
    bool initialized_ = false, attempted_ = false, haveTime_ = false;
    enum class Recovery : uint8_t { IDLE, STOP, START, FAILED } recovery_ = Recovery::IDLE;
    bool enqueue(const MotorControlCAN::Frame&, MotorControlCAN::CANOPEN::SdoTxId,
                 uint32_t) noexcept;
    bool retire(MotorControlCAN::CANOPEN::SdoTxId, uint32_t) noexcept;
    bool beginRecovery(uint32_t) noexcept;
    RecoveryProgress pollRecovery(uint32_t) noexcept;
};
} // namespace MotorControlCANExample
