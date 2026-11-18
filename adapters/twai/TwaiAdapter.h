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
/** Controller fault-confinement state (ISO 11898-1) as the SDK reports it. */
enum class TwaiErrorState : uint8_t {
    UNKNOWN, ///< Not read: no status callback, no node, or the SDK refused.
    ACTIVE,  ///< TEC and REC below 96 (SDK TWAI_ERROR_ACTIVE).
    WARNING, ///< TEC or REC at or above 96 and below 128 (TWAI_ERROR_WARNING).
    PASSIVE, ///< TEC or REC at or above 128 and below 256 (TWAI_ERROR_PASSIVE): no active error flags.
    BUS_OFF  ///< TEC at or above 256 (TWAI_ERROR_BUS_OFF): the node takes no part in bus traffic.
};
/** One copied SDK status read: error state and both error counters. */
struct TwaiControllerStatus {
    TwaiErrorState state = TwaiErrorState::UNKNOWN; ///< Fault-confinement state at the read.
    uint16_t transmitErrors = 0; ///< Transmit error counter (TEC).
    uint16_t receiveErrors = 0;  ///< Receive error counter (REC).
};
/** Controller error counters as bus evidence (guide rule C5), from TwaiAdapter::readErrorCounters.
 * They name no node and, unlike an EtherCAT port counter, locate no node or cable segment: never
 * node evidence and never a node miss. No threshold is applied; thresholds are caller settings,
 * still to measure. A controller restart creates a new node whose TEC and REC start at zero.
 */
struct TwaiErrorCounters {
    uint64_t readUs = 0;      ///< Supplied time of this read.
    bool statusKnown = false; ///< The SDK answered: state, transmitErrors and receiveErrors are meaningful.
    TwaiErrorState state = TwaiErrorState::UNKNOWN; ///< Error state at the read.
    uint16_t transmitErrors = 0; ///< TEC at the read.
    uint16_t receiveErrors = 0;  ///< REC at the read.
    uint32_t busErrors = 0;       ///< TwaiDiagnostics::busErrors as of the last pollEvents.
    uint32_t arbitrationLost = 0; ///< TwaiDiagnostics::arbitrationLost as of the last pollEvents.
    bool previousKnown = false;   ///< An earlier read exists: previousReadUs and the count deltas are meaningful.
    uint64_t previousReadUs = 0;  ///< Time of that earlier read.
    uint32_t busErrorsDelta = 0;  ///< Increase of busErrors since the earlier read; a saturated count hides further ones.
    uint32_t arbitrationLostDelta = 0; ///< Increase of arbitrationLost since the earlier read.
    bool statusDeltaKnown = false; ///< Both reads have known status: the fields below are meaningful.
    TwaiErrorState previousState = TwaiErrorState::UNKNOWN; ///< State at the earlier read.
    int32_t transmitErrorsDelta = 0; ///< TEC change since the earlier read; TEC is a level, so it can fall.
    int32_t receiveErrorsDelta = 0;  ///< REC change since the earlier read.
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
    uint32_t arbitrationLost = 0; ///< Saturating error callbacks whose flags include arbitration lost; a subset of busErrors.
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
    bool (*status)(void*, TwaiControllerStatus&) = nullptr;
    ///< Optional bounded SDK status read, never from an ISR; false, output unchanged, when no
    ///< node exists or the SDK refused. Without it readErrorCounters reports unknown status.
};
/** Cached adapter diagnostics; no hardware queries and no inferred wire completion. */
struct TwaiDiagnostics {
    uint32_t rxDropped = 0; ///< Observed software/invalid-frame drops, saturating.
    uint32_t busErrors = 0; ///< Raw error callback count, saturating. The SDK raises one callback for a bus error, an arbitration loss or both.
    uint32_t arbitrationLost = 0; ///< Error callbacks carrying the arbitration-lost flag, saturating; a subset of busErrors.
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
 * Hardware retransmits until ACK (DEC-4), so a no-ACK frame stays ACTIVE and stalls this
 * interface until an application-started owner recovery deletes the node. That retransmission
 * is link-level, not an application retry: it has no retry class and only the owner's TX
 * lifetime bounds it (guide rule C7); its external effects stay uncertain. Single-shot
 * transmission is an option to evaluate on the bench, not a default.
 * No automatic retry/recovery, clocks, device commands or heap allocation here.
 */
class TwaiAdapter {
  public:
    enum {
        /// Total waiting/active/report slots: one per SDO channel plus independent NMT and SYNC frames,
        /// the most the owner can have outstanding. No unread report eviction.
        TX_CAPACITY = MotorControlCAN::CANOPEN::SdoClient::CHANNELS + 2
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
    /** Read the controller error counters as bus evidence (guide rule C5): error state, TEC and
     * REC through TwaiDriver::status at nowUs, the cumulative error-callback counts as of the
     * last pollEvents, and each one's change since the previous read. Never node evidence; no
     * threshold is applied. Not for an ISR: call at a diagnostic rate, not per service turn.
     * @param nowUs Supplied time, not before the previous read's.
     * @param[out] output Complete reading on success, unchanged otherwise.
     * @return False, unchanged, for backward time; true otherwise, with statusKnown false when
     * the status callback is missing or failed. Each successful read becomes the next one's base.
     */
    bool readErrorCounters(uint64_t nowUs, TwaiErrorCounters& output) noexcept;

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
    TwaiErrorCounters lastRead_;
    bool readKnown_ = false;
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
