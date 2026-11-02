/** @file CanBusTypes.h
 * @brief Value types, identities and the transport boundary of the application CAN owner.
 * SPDX-License-Identifier: MIT
 * Not installed core. These are copied values and callback tables; the owner in
 * CanBusOwner.h gives their progression and lifetime rules.
 */
#pragma once
#include "SdoClient.h"

namespace MotorControlCANOwner {
using MotorControlCAN::Binding;
using MotorControlCAN::Frame;
class CanBusOwner;
/** Passive communication projection only; never drive readiness or an admission policy. */
enum class DriverState : uint8_t {
    UNINIT,   ///< No current qualified endpoint evidence.
    READY,    ///< Current communication evidence, no live failed-service streak.
    DEGRADED, ///< A failed attempted service has not been followed by a successful service.
    OFFLINE   ///< Explicitly unavailable controller or expired evidenced heartbeat deadline.
};
/** Cached binding-local service history. Timestamp zero is valid when its flag is true. */
struct Health {
    uint32_t totalSuccess = 0;  ///< Saturating completed attempted logical services, not frames.
    uint32_t totalFailures = 0; ///< Saturating failed attempted logical services.
    uint8_t consecutiveFailures = 0; ///< Saturating streak; only successful service clears it.
    uint64_t lastOkUs = 0;       ///< Supplied successful-service terminal processing microseconds.
    uint64_t lastErrorUs = 0;    ///< Supplied failed-service terminal processing microseconds.
    bool lastOkKnown = false;    ///< Makes lastOkUs meaningful, including zero.
    bool lastErrorKnown = false; ///< Makes lastErrorUs meaningful, including zero.
    MotorControlCAN::Status
        lastError; ///< Last failed service; detail is SdoOutcome, full abort stays in history.
    uint64_t lastSeenUs = 0;    ///< Checked delivery us, including qualified expired PDOs.
    bool lastSeenKnown = false; ///< Makes lastSeenUs meaningful; does not qualify freshness alone.
    DriverState state =
        DriverState::UNINIT; ///< Passive projection at latest supplied health-refresh time.
};
/** Interface lifetime totals, saturating at UINT32_MAX; no duplicate service totals. */
struct Stats {
    uint32_t txFrames = 0; ///< Exact matching final reports confirming actual transmission.
    uint32_t rxFrames = 0; ///< Frames returned by the receive callback, including rejected ingress.
    uint32_t txRejected = 0; ///< Refused enqueue or negative final report, once per frame.
    uint32_t rxDropped = 0;  ///< Invalid/stale/future ingress plus explicitly reported lost frames.
    uint32_t rxLost =
        0; ///< Subset explicitly reported missing by receiveOverflow; no raw frame exists.
    uint32_t unmatchedFrames = 0; ///< Valid ingress not accepted by normal protocol dispatch.
    uint32_t queueRejected =
        0; ///< Rejected ordinary/urgent admission attempts, including invalid input.
    uint32_t resultsRejected = 0; ///< Subset of queueRejected refused for occupied result capacity.
    uint32_t cancelled = 0; ///< Winning local request withdrawals, including group/recovery/guard.
    uint32_t timeouts = 0; ///< SDO request/recovery timeouts, including local SDO queue expiry.
    uint32_t busOffEvents =
        0;                   ///< Accepted transitions into reported BUS_OFF, not repeated reports.
    uint32_t recoveries = 0; ///< Successfully retired explicit host/controller recoveries.
    uint32_t traceOverwritten = 0; ///< Oldest diagnostic records replaced at fixed trace capacity.
    uint64_t maxServiceGapUs = 0;  ///< Largest gap between accepted service turns; not a guarantee.
};
/** Exact application transport identity; the selected payload alone is meaningful.
 * SDO frame IDs and NMT admission IDs are local correlation, absent from CAN.
 */
struct TransportTxId {
    /** Concrete service, never inferred from CAN frame bytes. */
    enum class Kind : uint8_t {
        SDO, ///< SDO admission plus exact frame number.
        NMT  ///< One-shot explicit NMT manager admission.
    };
    Kind kind = Kind::SDO; ///< Selects sdo or nmt.
    MotorControlCAN::CANOPEN::SdoTxId sdo; ///< Exact SDO admission/frame when kind is SDO.
    MotorControlCAN::CANOPEN::NmtRequestId nmt; ///< One-shot NMT handoff when kind is NMT.
    /** @param id Exact SDO handoff. @return Explicit tagged identity. */
    static TransportTxId fromSdo(MotorControlCAN::CANOPEN::SdoTxId id) noexcept {
        TransportTxId result;
        result.sdo = id;
        return result;
    }
    /** @param id Exact NMT handoff. @return Explicit tagged identity. */
    static TransportTxId fromNmt(MotorControlCAN::CANOPEN::NmtRequestId id) noexcept {
        TransportTxId result;
        result.kind = Kind::NMT;
        result.nmt = id;
        return result;
    }
};
/** @param a First identity. @param b Second identity. @return Exact selected-payload equality. */
inline bool sameTransportTxId(TransportTxId a, TransportTxId b) noexcept {
    return a.kind == b.kind && (a.kind == TransportTxId::Kind::NMT
        ? a.nmt.owner == b.nmt.owner && a.nmt.generation == b.nmt.generation
        : a.sdo.request.owner == b.sdo.request.owner &&
          a.sdo.request.channel == b.sdo.request.channel &&
          a.sdo.request.generation == b.sdo.request.generation && a.sdo.frame == b.sdo.frame);
}
/** Local retained-result identity; absent from the wire, invalid after owner destruction. */
struct RequestId {
    const CanBusOwner* owner = nullptr; ///< Stationary originating owner.
    uint8_t slot = 0;                   ///< Reserved request/result slot.
    uint32_t generation = 0;            ///< Nonzero nonwrapping local serial.
};
/** Exclusive logical-axis reservation across operations and gaps between requests. */
struct SequenceId {
    const CanBusOwner* owner = nullptr; ///< Stationary originating owner.
    uint8_t slot = 0;                   ///< Fixed reservation slot.
    uint32_t generation = 0; ///< Nonzero nonwrapping local serial, never wire correlation.
};
/** Fully copied ordinary service input; profile preparation/advancement stays outside owner. */
struct BusRequest {
    SequenceId sequence; ///< Live exclusive axis reservation, including its producer.
    MotorControlCAN::CANOPEN::SdoTransfer transfer; ///< Owned bytes/expectations/absolute deadline.
};
/** Local admission only; none indicates controller transmission or remote success. */
enum class BusAdmission : uint8_t {
    ACCEPTED,      ///< Complete input copied and terminal slot reserved.
    INVALID,       ///< Invalid owner/route/operation/policy/shape or overlapping output.
    EXPIRED,       ///< Overall or operation deadline already reached.
    RESULTS_FULL,  ///< Every request/result slot occupied; no unread terminal is evicted.
    IDS_EXHAUSTED, ///< Nonwrapping owner serial exhausted.
    INHIBITED      ///< Controller recovery/retirement has not established safe local resumption.
};
/** Local owner closure, independent of the native service's outcome/effect fields. */
enum class CompletionReason : uint8_t {
    SERVICE,            ///< Actual adapter terminal, including abort/timeout/uncertain effects.
    QUEUE_EXPIRED,      ///< Owner waiting consumed initial queue budget without transmission.
    DEADLINE,           ///< Immutable overall deadline expired while waiting in the owner.
    INVALIDATED,        ///< Current binding/node/session no longer matches queued input.
    ADMISSION_FAILED,   ///< Adapter refused dispatch after owner admission; nothing replayed.
    PREFLIGHT_REJECTED, ///< Application pre-handoff guard withdrew work before handoff.
    CANCELLED, ///< Explicit local cancellation; possible remote effects remain native evidence.
    RECOVERY   ///< Explicit host recovery interrupted work; not a device reset or motor stop.
};
/** Per-request cancellation disposition; no implicit group cancellation. */
enum class Cancel : uint8_t {
    CANCELLED,        ///< This call closed the request; external retirement may remain unproved.
    ALREADY_TERMINAL, ///< Earlier service/deadline/terminal won and is unchanged.
    INVALID           ///< Wrong handle, backward time or reentrant call; unchanged.
};
/** One retained host/controller recovery identity, independent of ordinary/urgent slots. */
struct RecoveryId {
    const CanBusOwner* owner = nullptr; ///< Stationary origin.
    uint32_t generation = 0;            ///< Nonzero nonwrapping serial; never a wire identity.
};
/** Bounded controller callback progress; neither completion state qualifies a remote session. */
enum class RecoveryProgress : uint8_t {
    PENDING, ///< Local queue retirement/controller preparation has not finished.
    RETIRED, ///< All older local TX/RX/reports are retired and controller can accept new work.
    FAILED   ///< Retirement/readiness cannot be proved; transmission stays inhibited.
};
/** Immutable recovery closure, separate from interrupted operation results. */
enum class RecoveryOutcome : uint8_t {
    RETIRED, ///< Host/controller boundary established only; no SDO/PDO requalification.
    FAILED,  ///< Adapter refused/failed; no safe resumption claimed.
    TIMEOUT  ///< Exclusive budget expired; a late callback cannot change this result.
};
/** Retained copied recovery evidence until releaseRecovery; no device effect is certified. */
struct RecoveryCompletion {
    RecoveryId id;                ///< Canonical identity.
    uint32_t controllerEpoch = 0; ///< Newly allocated local epoch; cannot tag remote wire replies.
    uint64_t startedUs = 0;       ///< Supplied processing time; zero valid.
    uint64_t deadlineUs = 0;      ///< Immutable exclusive absolute cutoff.
    uint64_t completedUs = 0;     ///< First terminal processing time.
    RecoveryOutcome outcome = RecoveryOutcome::FAILED; ///< No implicit success default.
};
/** Immutable first terminal, retained non-consumingly until explicit release. */
struct Completion {
    RequestId id;            ///< Canonical owner result identity.
    SequenceId sequence;     ///< Reservation/producer identity at admission.
    uint8_t producer = 0;    ///< Application producer 1..PRODUCERS.
    uint64_t admittedUs = 0; ///< Owner admission time; zero valid, queue age starts here.
    CompletionReason reason = CompletionReason::SERVICE; ///< Local closure provenance.
    bool adapterAdmitted = false;    ///< Separate from owner admission and TX staging.
    bool controllerAccepted = false; ///< At least one frame accepted by controller callback.
    MotorControlCAN::CANOPEN::SdoCompletion service; ///< Complete native evidence and owned bytes.
};
/** One owned passive diagnostic; completion is meaningful only for TERMINAL. */
enum class DiagnosticKind : uint8_t {
    RX,         ///< Raw ingress after normal dispatch or owner rejection.
    TX,         ///< One enqueue attempt, not actual transmission evidence.
    TX_REPORT,  ///< Matching final controller report; never a protocol acknowledgement.
    TERMINAL,   ///< Complete copied request terminal, including raw abort and uncertainty.
    CONTROLLER, ///< Explicit application controller report.
    RECOVERY    ///< Explicit host recovery closure.
};
/** Bounded historical copy; no pointer into controller/stack buffers is retained. */
struct Diagnostic {
    DiagnosticKind kind = DiagnosticKind::RX; ///< Selects meaningful evidence fields.
    uint64_t processedUs = 0;       ///< Supplied owner processing microseconds; zero valid.
    uint32_t controllerEpoch = 0;   ///< Local ingress/handoff epoch, never a remote wire tag.
    Frame frame;                    ///< RX/TX wire copy; RTR payload is not inspected.
    bool receivedTimeKnown = false; ///< RX only; capture was explicitly supplied.
    uint64_t receivedUs = 0; ///< RX capture microseconds, including stale/future diagnostics.
    bool receiveBoundsKnown = false; ///< RX conservative capture interval, independent of exact time.
    uint64_t receivedEarliestUs = 0; ///< RX lower bound when receiveBoundsKnown; zero valid.
    uint64_t receivedLatestUs = 0; ///< RX upper bound, no later than processing time for accepted RX.
    TransportTxId txId; ///< TX/TX_REPORT exact local frame identity.
    bool accepted = false; ///< RX dispatch, TX queue acceptance or TX_REPORT actual TX, by kind.
    Completion completion; ///< TERMINAL only; survives original result release.
    MotorControlCAN::CANOPEN::ControllerState controller =
        MotorControlCAN::CANOPEN::ControllerState::UNKNOWN; ///< CONTROLLER only.
    RecoveryOutcome recovery = RecoveryOutcome::FAILED;     ///< RECOVERY only.
};
/** Cached resource usage, computed by fixed scans; no controller queries. */
struct OwnerResources {
    uint8_t queued = 0;           ///< Admitted SDO requests not yet in the native service.
    uint8_t active = 0;           ///< Native in-flight SDO requests, at most CHANNELS.
    uint8_t retained = 0;         ///< Unreleased SDO terminals, ordinary plus urgent.
    uint8_t reservations = 0;     ///< Occupied logical sequence slots.
    uint8_t traceUsed = 0;        ///< Retained diagnostic copies, at most TRACE_CAPACITY.
    bool inhibited = false;       ///< Actual controller/recovery interlock, independent of Health.
    bool recoveryPending = false; ///< Explicit recovery awaiting its terminal.
    bool nmtPending = false; ///< Sole explicit manager intent awaits its native terminal.
    bool nmtRetained = false; ///< Sole native manager terminal is retained until releaseNmt.
    bool nmtTxPending = false; ///< Manager handoff lacks exact final report/retirement proof.
};
/** One ingress copy; processing time is supplied separately to service(). */
struct ReceivedFrame {
    Frame frame; ///< Complete owned frame, including standard/extended and data/RTR shape.
    bool receivedTimeKnown = false; ///< No invented capture timestamp when false.
    uint64_t receivedUs = 0;        ///< Qualified capture microseconds, zero valid, not future.
    bool receiveBoundsKnown = false; ///< True for conservative capture bounds without an exact timestamp.
    uint64_t receivedEarliestUs = 0; ///< Inclusive lower capture bound when known; zero valid.
    uint64_t receivedLatestUs = 0; ///< Inclusive upper capture bound, at most processing time.
    uint32_t controllerEpoch = 1;   ///< Stamp at adapter ingress, before queuing; never on dequeue.
};
/** Actual controller report, never inferred from enqueue acceptance. */
struct TxReport {
    TransportTxId id; ///< Exact copied native handoff identity.
    bool transmitted = false; ///< True actual TX/ACK; false failure with uncertain effect.
    bool provedUnsent = false; ///< With transmitted false: this frame never reached the SDK.
    uint32_t controllerEpoch =
        1; ///< Original enqueue epoch. Both results prove no later TX of this frame.
};
/** Narrow synchronous application/controller boundary; no hidden receiver/task.
 * Callback contexts outlive the owner. Calls never reenter it or service storage.
 * receive/takeTxReport publish complete outputs only on true and retain no pointers.
 */
struct Transport {
    void* context = nullptr; ///< Borrowed stationary application/controller context.
    bool (*receive)(void*, ReceivedFrame&) = nullptr; ///< At most one frame per call; false empty.
    bool (*transmit)(void*, const Frame&, TransportTxId, uint32_t) = nullptr;
    ///< One-shot enqueue, borrowing frame only during call; true is queue acceptance only.
    ///< Final argument is controller epoch; false conservatively permits uncertain acceptance.
    bool (*takeTxReport)(void*, TxReport&) = nullptr; ///< One copied actual controller report.
    bool (*beforeTransmit)(void*, SequenceId, const MotorControlCAN::CANOPEN::SdoTransfer&,
                           uint64_t) = nullptr;
    ///< Optional first-download guard; notify typed settings before possible TX.
    void (*observe)(void*, const ReceivedFrame&, uint64_t) = nullptr;
    ///< Optional raw copy after dispatch/rejection, including malformed/future diagnostics;
    ///< not qualified evidence and cannot consume authoritative input.
    bool (*retire)(void*, TransportTxId, uint32_t) = nullptr;
    ///< Bounded synchronous attempt: true proves this exact frame cannot leave any local
    ///< SDK/controller queue later, and retires all queued/future final reports for
    ///< this exact id/epoch. Earlier effects remain possible and may be unconfirmed.
    ///< If report retirement cannot be proved, return false. False/missing blocks
    ///< that channel until a matching final TX report or explicit successful recovery.
    ///< Never sends a CAN frame or resets the drive.
    bool (*beginRecovery)(void*, uint32_t) = nullptr;
    ///< Start bounded host/controller work for the new epoch, no CAN frames/replay.
    ///< False may have partial local effects; caller stays inhibited. Stop any older
    ///< recovery attempt before starting this one; callbacks retain no borrowed pointers.
    RecoveryProgress (*pollRecovery)(void*, uint32_t) = nullptr;
    ///< One bounded poll. RETIRED proves all prior local TX/RX/report queues retired,
    ///< including asynchronous callbacks, and controller readiness. New ingress must
    ///< carry the new epoch. It proves nothing about remote pending TX or SDO sessions.
};
/** Explicit per-turn budgets; values above fixed limits clamp, zero invokes no corresponding callback. */
struct ServiceBudget {
    uint8_t rx = 0;     ///< Max receive callbacks, clamped to RX_LIMIT; false stops draining.
    uint8_t tx = 0;     ///< Max one-shot enqueues, clamped to TX_LIMIT.
    uint8_t events = 0; ///< Max report callbacks, clamped to EVENT_LIMIT; nonzero also permits
                        ///< at most CHANNELS+1 one-shot SDO/NMT retirement callbacks.
    uint8_t starts = 0; ///< Max new adapter admissions, clamped to START_LIMIT; needs TX budget.
    uint8_t recovery =
        0; ///< Max one recovery poll per turn; zero does not poll, deadline still expires.
};
/** Read-only reservation facts, not a second current node/binding cache. */
struct Sequence {
    Binding binding; ///< Reserved binding; explicit successful configuration sync may advance it.
    uint32_t nodeGeneration = 0; ///< Frozen lifecycle epoch; invalidation never requalifies it.
    uint8_t producer = 0;        ///< Producer 1..PRODUCERS, selected independently of CAN node.
    uint64_t acquiredUs = 0;     ///< Reservation acquisition before prerequisite reads; zero valid.
    uint32_t operation = 0;  ///< Explicit current operation, strictly increasing per reservation.
    uint64_t deadlineUs = 0; ///< Immutable exclusive deadline for this operation.
};
} // namespace MotorControlCANOwner
