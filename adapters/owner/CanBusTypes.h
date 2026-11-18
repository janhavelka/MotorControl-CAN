/** @file CanBusTypes.h
 * @brief Value types, identities and the transport boundary of the application CAN owner.
 * SPDX-License-Identifier: MIT
 * Not installed core. These are copied values and callback tables; the owner in
 * CanBusOwner.h gives their progression and lifetime rules.
 */
#pragma once
#include "SdoClient.h"
#include <MotorControlCAN/canopen/Sync.h>

namespace MotorControlCANOwner {
using MotorControlCAN::Binding;
using MotorControlCAN::Frame;
class CanBusOwner;
/** Passive communication projection of one node; never drive readiness or an admission policy.
 * Node-level evidence only (guide rule C2): a bus-level fault never sets OFFLINE. While one is
 * active the projection keeps its last value, marked Health::notObserved; the interface carries
 * the fault (OwnerResources::interfaceState).
 */
enum class DriverState : uint8_t {
    UNINIT,   ///< No current qualified endpoint evidence.
    READY,    ///< Current communication evidence, no live failed-service streak.
    DEGRADED, ///< An answered-with-error or no-answer service has not been followed by an answered one.
    OFFLINE   ///< The evidenced heartbeat deadline expired (setHeartbeatMaximumAge).
};
/** Interface communication state: a bus fact that names no node (guide rule C2). */
enum class InterfaceState : uint8_t {
    AVAILABLE, ///< No bus-level fault is active.
    OFFLINE    ///< A bus-level fault is active: the recovery interlock (controller bus-off, recovering or reported unavailable, a recovery running or failed, an RX overflow episode) or NO_LISTENER. Node rows keep their last state, marked not observed.
};
/** Evidence class of one terminal, assigned once by the owner (prompt 55, guide rule C3). */
enum class EvidenceClass : uint8_t {
    ANSWERED,            ///< A valid response completed the service: the server is present.
    ANSWERED_WITH_ERROR, ///< The server answered, but not with a success: server abort, wrong multiplexer, toggle error, malformed or wrong-size reply, a contradicting frame, or a response timeout after a frame arrived for the awaited request. Present; an error, not a miss.
    NO_ANSWER,           ///< Response timeout of a request frame confirmed by a positive TX report, with no frame on the response identifier since, while no bus-level fault was active. The only miss.
    LOCAL_END            ///< Everything else: cancelled, expired in the queue, deadline before handoff, invalidated (boot-up, rebinding, controller event), refused or failed admission, a request frame never confirmed (no ACK is bus evidence, not node absence), a best-effort stop, a capability the client refused, or any terminal while a bus-level fault was active. Changes no count.
};
/** How an SDO route may be reused after an unclear end (prompt 55, amending DEC-3).
 * Classic SDO carries no transaction tag. Both accepted settings state the premises "sole
 * client on these COB-IDs" and "server answers one request at a time, in order" as an accepted
 * risk, never as established fact (Q18 stays open), and neither can detect a delayed reply that
 * names the same object as a later request. Results on a route reopened under either carry
 * Completion::acceptedCorrelation for the rest of that server session.
 */
enum class SdoReusePolicy : uint8_t {
    NONE, ///< No reuse: an unresolved route stays quarantined and every marker is refused.
    ACCEPTED_RISK_WITH_MARKER, ///< Default. After a quarantine with possible TX (timeout, cancellation, bus-off, a controller event before a retired recovery), once local TX is retired, the owner issues one marker read of a different accepted object; its success reopens. Never after a best-effort stop.
    ACCEPTED_RISK_WITHOUT_MARKER ///< After the same quarantines, local retirement of the request and its abort alone reopens, as CANopenNode and python-canopen reuse a channel. Pairs retained before it stay expected until the route's first success. Never after a best-effort stop.
};
/** One object the owner may read as a drain marker. It must be an accepted read (DEC-1) of 1-4
 * bytes: the owner reads it with SdoReadEffect::ACCEPTED_UNQUALIFIED, EXACT size and no segmented
 * transfer. Choosing another object is the application's responsibility; its read effect stays
 * whatever the application reviewed, and the read stays effectPossible.
 */
struct SdoMarkerObject {
    uint16_t index = 0x1000; ///< Object index, nonzero. Default 1000 (device type), a DEC-1 object.
    uint8_t subindex = 0;    ///< Object subindex.
    uint8_t size = 4;        ///< Exact width in bytes, 1..4. 1000:00 and 1018:01..03 are U32.
    bool expectedKnown = false; ///< Compare the successful marker value with expectedValue.
    uint32_t expectedValue = 0; ///< Commissioned value, little-endian over size bytes. A mismatch means another drive answers: the owner invalidates the binding instead of reopening.
};
/** Per-binding route reuse setting, settable through CanBusOwner::setSdoReuse. */
struct SdoReuse {
    /** Fixed list capacity. */
    enum {
        MARKER_OBJECTS = 4 ///< Capacity of the marker list.
    };
    SdoReusePolicy policy =
        SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER; ///< Documented default; NONE keeps today's quarantine.
    /** Marker objects, tried in order. The owner reads the first one that is neither the route's
     * last completed pair nor a retained pair; with none left, the step reports MARKER_UNAVAILABLE.
     * Default 1000:00 (device type), 1018:01 (vendor ID), 1018:02 (product code), 1018:03
     * (revision number): all DEC-1 accepted reads, all U32, no expected values. Only the first
     * markerCount entries are used. */
    SdoMarkerObject markers[MARKER_OBJECTS];
    uint8_t markerCount = MARKER_OBJECTS; ///< Entries in use, 1..MARKER_OBJECTS, pairwise distinct.
    /** Queue budget of the owner's marker in microseconds, counted from its admission. Zero, the
     * documented default, inherits the queue budget of the route's last request. */
    uint32_t markerQueueTimeoutUs = 0;
    /** Response budget of the owner's marker in microseconds after its handoff. Zero, the
     * documented default, inherits the response budget of the route's last request; set it when
     * that request's budget was deliberately short, or the marker times out the same way. The
     * marker's own deadline is admission plus both budgets; no earlier deadline is extended. */
    uint32_t markerResponseTimeoutUs = 0;
    /** The documented default: ACCEPTED_RISK_WITH_MARKER, the four DEC-1 objects above, budgets
     * inherited from the route's last request. */
    SdoReuse() noexcept {
        for (unsigned i = 1; i < MARKER_OBJECTS; ++i) {
            markers[i].index = 0x1018;
            markers[i].subindex = uint8_t(i);
        }
    }
};
/** Cached binding-local service history, counted by evidence class (guide rule C3).
 * Timestamp zero is valid when its flag is true. CanBusOwner::clearStats clears the totals and
 * the node counters with their times; the streaks, the service and presence times, state,
 * notObserved and lastError survive it. A binding identity change resets the whole record.
 */
struct Health {
    uint32_t totalSuccess = 0;  ///< Saturating ANSWERED services, not frames.
    uint32_t totalErrors = 0;   ///< Saturating ANSWERED_WITH_ERROR services.
    uint32_t totalMisses = 0;   ///< Saturating NO_ANSWER services.
    uint32_t totalFailures = 0; ///< Saturating totalErrors plus totalMisses, kept for one failure total; LOCAL_END is never counted.
    uint8_t consecutiveFailures = 0; ///< Saturating streak of errors and misses; only an ANSWERED service clears it.
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
    // Presence observations of this binding's physical node. The owner records them and decides
    // nothing from them; times are owner processing times, never wire capture times.
    uint64_t lastResponseUs = 0; ///< Last frame received on this node's SDO response identifier, accepted or not.
    bool lastResponseKnown = false; ///< Makes lastResponseUs meaningful, including zero.
    uint64_t lastHeartbeatUs = 0; ///< Last heartbeat (not boot-up) the adapter accepted for this node.
    bool lastHeartbeatKnown = false; ///< Makes lastHeartbeatUs meaningful, including zero.
    uint32_t bootUps = 0; ///< Saturating boot-up frames the adapter accepted; each still invalidates the node as before.
    uint64_t lastBootUpUs = 0; ///< Processing time of the latest of them.
    bool lastBootUpKnown = false; ///< Makes lastBootUpUs meaningful, including zero.
    uint8_t consecutiveNoResponse =
        0; ///< Miss streak: saturating NO_ANSWER services of this node. An ANSWERED or ANSWERED_WITH_ERROR service, any frame on the node's SDO response identifier and a heartbeat (not a boot-up or EMCY) clear it; LOCAL_END changes it neither way.
    // Node counters (guide rule C13), from node-level evidence of this binding's node only. Each
    // saturates; its time is the processing time of the latest event, meaningful while the
    // counter is nonzero. clearStats clears both.
    uint32_t abortsSent = 0; ///< Client aborts on this node's routes confirmed transmitted by a positive TX report, cleanup aborts included.
    uint64_t lastAbortSentUs = 0; ///< Time of the latest of them.
    uint32_t abortsReceived = 0; ///< Abort frames received on this node's SDO response identifiers, matched or not.
    uint64_t lastAbortReceivedUs = 0; ///< Time of the latest of them.
    uint32_t resyncs = 0; ///< Routes of this node reopened (ResyncStep::REOPENED), with a marker or without.
    uint64_t lastResyncUs = 0; ///< Time of the latest of them.
    /** A bus-level fault is active (OwnerResources::interfaceState is OFFLINE): state keeps its
     * last value and no miss is counted until the fault ends; then the row is observed again. */
    bool notObserved = false;
    uint64_t notObservedSinceUs = 0; ///< Processing time the current fault began; meaningful while notObserved.
    // Route reuse (prompt 55). Policy premises are accepted, not established.
    SdoReusePolicy reusePolicy = SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER; ///< This binding's setting.
    bool reuseStopped = false; ///< Contradicting traffic refused reuse; only reacceptReuse withdraws it.
    bool resyncPending = false; ///< An armed resync step waits for local retirement, a free urgent slot or a start budget.
    bool acceptedCorrelation = false; ///< The route was reopened under an accepted setting in this server session.
    bool noListener = false; ///< Interface-wide NO_LISTENER copy; see CanBusOwner::noListener.
};
/** Interface lifetime totals, saturating at UINT32_MAX; no duplicate service totals.
 * Each `last...Us` time is the processing time of the latest event of the counter before it,
 * meaningful while that counter is nonzero. clearStats clears every field.
 */
struct Stats {
    uint32_t txFrames = 0; ///< Exact matching final reports confirming actual transmission.
    uint32_t rxFrames = 0; ///< Frames returned by the receive callback, including rejected ingress.
    uint32_t txRejected = 0; ///< Refused enqueue or negative final report, once per frame.
    uint32_t rxDropped = 0;  ///< Invalid/stale/future ingress plus explicitly reported lost frames.
    uint32_t rxLost =
        0; ///< Subset explicitly reported missing by receiveOverflow; no raw frame exists.
    uint32_t unmatchedFrames = 0; ///< Valid ingress accepted by neither native dispatch nor passive SYNC.
    uint32_t queueRejected =
        0; ///< Rejected ordinary/urgent admission attempts, including invalid input.
    uint32_t resultsRejected = 0; ///< Subset of queueRejected refused for occupied result capacity.
    uint32_t cancelled = 0; ///< Winning local request withdrawals, including group/recovery/guard.
    uint32_t timeouts = 0; ///< SDO request/recovery timeouts, including local SDO queue expiry.
    uint32_t busOffEvents =
        0;                   ///< Accepted transitions into reported BUS_OFF, not repeated reports.
    uint64_t lastBusOffUs = 0; ///< Time of the latest bus-off event.
    uint32_t recoveriesStarted = 0; ///< Accepted recover() calls: controller restarts started.
    uint64_t lastRecoveryStartedUs = 0; ///< Time of the latest of them.
    uint32_t recoveries = 0; ///< Controller restarts succeeded: recoveries closed RETIRED.
    uint64_t lastRecoveryUs = 0; ///< Time of the latest of them.
    uint32_t recoveriesFailed = 0; ///< Controller restarts failed: recoveries closed FAILED or TIMEOUT.
    uint64_t lastRecoveryFailedUs = 0; ///< Time of the latest of them.
    uint32_t rxOverflows = 0; ///< Accepted receiveOverflow reports (rxLost counts their frames).
    uint64_t lastRxOverflowUs = 0; ///< Time of the latest of them.
    uint32_t abortsSent = 0; ///< Client abort frames confirmed transmitted by a positive TX report.
    uint64_t lastAbortSentUs = 0; ///< Time of the latest of them.
    uint32_t abortsReceived = 0; ///< Abort frames received on configured SDO response identifiers.
    uint64_t lastAbortReceivedUs = 0; ///< Time of the latest of them.
    uint32_t resyncs = 0; ///< Routes reopened (ResyncStep::REOPENED), with a marker or without.
    uint64_t lastResyncUs = 0; ///< Time of the latest of them.
    uint32_t traceOverwritten = 0; ///< Oldest diagnostic records replaced at fixed trace capacity.
    uint64_t maxServiceGapUs = 0;  ///< Largest gap between accepted service turns; not a guarantee.
    uint32_t markers = 0; ///< Drain markers the owner issued itself as a resync step; explicit ones are not counted.
    uint32_t reuseStopped = 0; ///< Routes whose reuse contradicting traffic refused, once per refusal.
    uint32_t txExpired = 0; ///< Accepted handoffs without a final TX report within the TX lifetime, once per frame; none while a recovery is pending.
    uint64_t lastTxExpiredUs = 0; ///< Time of the latest of them.
};
/** Exact local SYNC admission identity; absent from the wire. */
struct SyncRequestId {
    const CanBusOwner* owner = nullptr; ///< Stationary owner, borrowed for its lifetime.
    uint32_t generation = 0; ///< Nonzero nonwrapping serial.
};
/** First immutable local terminal; release never proves transport retirement. */
struct SyncTerminal {
    SyncRequestId id; ///< Exact admission.
    MotorControlCAN::CANOPEN::SyncWork work; ///< Owned finite request.
    MotorControlCAN::CANOPEN::SyncCompletion service; ///< Local TX evidence, never drive effect.
    uint64_t admittedUs = 0; ///< Supplied admission time, zero valid.
    uint64_t handoffUs = 0; ///< First application handoff time; meaningful after handoff.
    uint64_t completedUs = 0; ///< First terminal processing time.
};
/** Last foreign SYNC observation; time bounds concern frame completion, not sampling. */
struct SyncObservation {
    bool present = false; ///< A valid standard 0x80 DLC-zero or counter-bearing frame was received.
    uint32_t serial = 0; ///< Nonwrapping local observation identity, never a wire counter.
    Frame frame; ///< Owned foreign frame.
    MotorControlCAN::CANOPEN::ObservationTime time; ///< Exact, bounded or unknown receive time.
    uint32_t controllerEpoch = 0; ///< Local ingress epoch, not sender identity.
};
/** Exact application transport identity; the selected payload alone is meaningful.
 * SDO frame IDs and NMT/SYNC admission IDs are local correlation, absent from CAN.
 */
struct TransportTxId {
    /** Concrete service, never inferred from CAN frame bytes. */
    enum class Kind : uint8_t {
        SDO, ///< SDO admission plus exact frame number.
        NMT, ///< One-shot explicit NMT manager admission.
        SYNC ///< Explicit unconfirmed SYNC production.
    };
    Kind kind = Kind::SDO; ///< Selects sdo, nmt or sync.
    SyncRequestId sync; ///< Exact owner SYNC handoff when kind is SYNC.
    MotorControlCAN::CANOPEN::SdoTxId sdo; ///< Exact SDO admission/frame when kind is SDO.
    MotorControlCAN::CANOPEN::NmtRequestId nmt; ///< One-shot NMT handoff when kind is NMT.
    /** @param id Exact SYNC handoff. @return Explicit tagged identity. */
    static TransportTxId fromSync(SyncRequestId id) noexcept {
        TransportTxId result;
        result.kind = Kind::SYNC;
        result.sync = id;
        return result;
    }
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
    return a.kind == b.kind && (a.kind == TransportTxId::Kind::SYNC
        ? a.sync.owner == b.sync.owner && a.sync.generation == b.sync.generation
        : a.kind == TransportTxId::Kind::NMT
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
    INHIBITED,     ///< Controller recovery/retirement has not established safe local resumption.
    NO_LISTENER    ///< Ordinary work refused, nothing sent: no frame has been received or ACKed since a TX lifetime expired. Urgent work, best-effort stops and drain markers stay admissible.
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
/** Conditions reported since the previous recovery began, moved into a recovery when it begins.
 * One reported while a recovery runs stays for the next; a failed or timed-out recovery hands its
 * own on. Bus-off, receive loss and TX expiry share the one recovery entry point and keep their
 * own cause here and in their own counters. None of them starts a recovery by itself.
 */
struct RecoveryCauses {
    bool busOff = false;      ///< reportController(BUS_OFF) was accepted.
    bool receiveLost = false; ///< receiveOverflow reported lost authoritative ingress.
    bool txExpired = false;   ///< An accepted handoff outlived the TX lifetime without a final report.
};
/** Retained copied recovery evidence until releaseRecovery; no device effect is certified. */
struct RecoveryCompletion {
    RecoveryId id;                ///< Canonical identity.
    uint32_t controllerEpoch = 0; ///< Newly allocated local epoch; cannot tag remote wire replies.
    uint64_t startedUs = 0;       ///< Supplied processing time; zero valid.
    uint64_t deadlineUs = 0;      ///< Immutable exclusive absolute cutoff.
    uint64_t completedUs = 0;     ///< First terminal processing time.
    RecoveryOutcome outcome = RecoveryOutcome::FAILED; ///< No implicit success default.
    RecoveryCauses causes; ///< Conditions pending when this recovery began; a retired recovery clears them.
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
    SdoReusePolicy reusePolicy =
        SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER; ///< The binding's reuse setting at owner admission.
    bool acceptedCorrelation = false; ///< The route had been reopened under an accepted setting in this server session: a reply's correlation rests on accepted premises. Always equal to service.acceptedCorrelation. No detection is claimed either way.
    bool automaticMarker = false; ///< The owner issued this drain marker itself; its slot is released at its terminal, so only the TERMINAL diagnostic keeps it.
    EvidenceClass evidence = EvidenceClass::LOCAL_END; ///< Assigned once at the terminal; Health counts by it.
};
/** Step of the owner's route resync, reported in a RESYNC diagnostic. */
enum class ResyncStep : uint8_t {
    MARKER_ADMITTED, ///< The owner admitted its one marker read for a timed-out route.
    MARKER_FAILED,   ///< That marker ended without success; the route stays unresolved and no further marker follows by itself.
    MARKER_MISMATCH, ///< A marker read returned another value than the configured one; the binding was invalidated instead of reopened.
    REOPENED,        ///< The route reopened (after a marker or without one); reservations on the node were invalidated.
    REUSE_STOPPED,   ///< Contradicting traffic refused reuse on the route.
    REUSE_ACCEPTED,  ///< The application re-accepted reuse with reacceptReuse.
    MARKER_UNAVAILABLE ///< The step ended without a marker: every listed object is a retained or last completed pair, no budget is configured or known for the route, or no identity is left. admitDrainMarker remains.
};
/** One owned passive diagnostic; completion is meaningful only for TERMINAL. */
enum class DiagnosticKind : uint8_t {
    RX,         ///< Raw ingress after normal dispatch or owner rejection.
    TX,         ///< One enqueue attempt, not actual transmission evidence.
    TX_REPORT,  ///< Matching final controller report; never a protocol acknowledgement.
    TERMINAL,   ///< Complete copied request terminal, including raw abort and uncertainty.
    CONTROLLER, ///< Explicit application controller report.
    RECOVERY,   ///< Explicit host recovery closure.
    RESYNC,     ///< Route resync step on channel; see resync.
    TX_EXPIRED, ///< An accepted handoff, txId, reached the TX lifetime without a final report. It stays possibly sent.
    LISTENER    ///< NO_LISTENER began or ended; see noListener.
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
    RecoveryCauses recoveryCauses; ///< RECOVERY only: conditions pending when it began.
    uint8_t channel = 0; ///< RESYNC only: adapter channel slot, not a wire node ID.
    ResyncStep resync = ResyncStep::MARKER_ADMITTED; ///< RESYNC only.
    bool noListener = false; ///< LISTENER only: true when NO_LISTENER began, false when it ended.
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
    bool syncArmed = false; ///< Explicit production permission; recovery never restores it.
    bool syncConflict = false; ///< Foreign producer latch, cleared only by explicit resolution.
    bool syncPending = false; ///< Independent copied request awaits a terminal.
    bool syncRetained = false; ///< Independent terminal awaits release.
    bool syncTxPending = false; ///< Exact external SYNC handoff remains unretired.
    bool nmtPending = false; ///< Sole explicit manager intent awaits its native terminal.
    bool nmtRetained = false; ///< Sole native manager terminal is retained until releaseNmt.
    bool nmtTxPending = false; ///< Manager handoff lacks exact final report/retirement proof.
    bool noListener = false; ///< NO_LISTENER interface state; see CanBusOwner::noListener.
    bool receiveLost = false; ///< RX overflow episode: from an accepted receiveOverflow until a recovery closes RETIRED. Evidence the lost frames may have carried is invalidated.
    InterfaceState interfaceState = InterfaceState::AVAILABLE; ///< OFFLINE while inhibited or noListener.
    uint64_t offlineSinceUs = 0; ///< Processing time the current OFFLINE began; meaningful while OFFLINE.
    uint8_t txExpired = 0; ///< Pending handoffs past the TX lifetime, still possibly sent, until a final report or a retired recovery.
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
    ///< Must exclude local self-echo: an indistinguishable received SYNC is foreign.
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
                        ///< at most CHANNELS+2 one-shot SDO/NMT/SYNC retirement callbacks.
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
