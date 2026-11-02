/** @file TwaiAdapter.h
 * @brief Bounded application transport policy for the pinned TWAI node driver.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "CanBusOwner.h"

namespace MotorControlCANExample {
using MotorControlCANOwner::TransportTxId;
using MotorControlCANOwner::RecoveryProgress;
using MotorControlCANOwner::ReceivedFrame;
using MotorControlCANOwner::TxReport;
using MotorControlCANOwner::Transport;
using MotorControlCANOwner::CanBusOwner;
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
    bool rxPending = false;  ///< Unread authoritative RX exists at this snapshot.
};
/** Result of one submission attempt at the SDK boundary. */
enum class TwaiSubmit : uint8_t {
    SENT,             ///< SDK accepted the frame; a later final report owns its outcome.
    DEFERRED,         ///< Proved no SDK contact, transient; the frame stays queued.
    FAILED_UNSENT,    ///< Proved no SDK acceptance, future TX or callback; terminal.
    FAILED_UNCERTAIN  ///< SDK may have accepted it; effects stay possible; terminal.
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
    TwaiSubmit (*submit)(void*, const MotorControlCAN::Frame&, uint32_t) = nullptr;
    ///< Copy frame; one outstanding. Only SENT and FAILED_UNCERTAIN can borrow or transmit.
    TwaiEvents (*events)(void*) = nullptr; ///< One bounded atomic mailbox/counter snapshot.
    bool (*receive)(void*, ReceivedFrame&, uint64_t) = nullptr;
    ///< One copied ingress stamped at ISR enqueue; final argument is inclusive
    ///< processing cutoff. Leave a newer head frame queued until a later turn.
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
 * Construct without I/O, then explicitly start. In each turn call pollEvents before
 * owner.service and promote after it, at the same supplied monotonic microseconds.
 * pollEvents copies ISR facts and reports controller faults outside owner callbacks,
 * so a TX report normally precedes its reply. A frame can still complete after the
 * poll: the owner's blocked handling for that race stays required. promote submits at
 * most one queued frame after rechecking the same SDO store, and starts the frame's
 * response clock only at SDK submission. Owner remains sole RX router.
 * Hardware retransmits until ACK, so a no-ACK frame stays ACTIVE and stalls this
 * interface until an application-started owner recovery deletes the node. That is
 * not an application retry, and its external effects stay uncertain.
 * No automatic retry/recovery, clocks, device commands or heap allocation here.
 */
class TwaiAdapter {
  public:
    enum {
        /// Total waiting/active/report slots: one per SDO channel plus the one NMT frame,
        /// the most the owner can have outstanding. No unread report eviction.
        TX_CAPACITY = MotorControlCAN::CANOPEN::SdoClient::CHANNELS + 1
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
    /** First half of a turn, before owner.service: copy ISR facts and publish TX reports.
     * Backward time inhibits and invalidates owner evidence at its last accepted
     * supplied time. It cannot retire an active SDK frame or settle its remote effect.
     * @param owner Sole owner using transport() and the same service storage.
     * @param nowUs Nondecreasing monotonic microseconds, zero valid.
     */
    void pollEvents(CanBusOwner& owner, uint64_t nowUs) noexcept;
    /** Second half of a turn, after owner.service: submit at most one queued frame.
     * A deferred frame stays queued and keeps no response clock. Nothing is resent.
     * @param owner Sole owner using transport() and the same service storage.
     * @param nowUs Same supplied time as this turn's pollEvents.
     */
    void promote(CanBusOwner& owner, uint64_t nowUs) noexcept;
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
    bool acceptTime(CanBusOwner& owner, uint64_t nowUs) noexcept;
    enum class Recovery : uint8_t { IDLE, STOP, START, FAILED } recovery_ = Recovery::IDLE;
    bool enqueue(const MotorControlCAN::Frame&, TransportTxId,
                 uint32_t) noexcept;
    bool retire(TransportTxId, uint32_t) noexcept;
    bool beginRecovery(uint32_t) noexcept;
    RecoveryProgress pollRecovery(uint32_t) noexcept;
};
} // namespace MotorControlCANExample
