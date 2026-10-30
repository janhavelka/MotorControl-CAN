/** @file CanBusOwner.h
 * @brief Bounded cooperative application scheduling over one existing CANopen service store.
 * SPDX-License-Identifier: MIT
 * Not installed core. Resources and callbacks are stationary, borrowed for owner
 * lifetime. One context calls this owner; callbacks are bounded/nonreentrant.
 */
#pragma once
#include "SdoClient.h"

namespace MotorControlCANExample {
using MotorControlCAN::Binding;
using MotorControlCAN::Frame;
class CanBusOwner;
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
    ACCEPTED,     ///< Complete input copied and terminal slot reserved.
    INVALID,      ///< Invalid owner/route/operation/policy/shape or overlapping output.
    EXPIRED,      ///< Overall or operation deadline already reached.
    RESULTS_FULL, ///< Every request/result slot occupied; no unread terminal is evicted.
    IDS_EXHAUSTED ///< Nonwrapping owner serial exhausted.
};
/** Local owner closure, independent of the native service's outcome/effect fields. */
enum class CompletionReason : uint8_t {
    SERVICE,           ///< Actual adapter terminal, including abort/timeout/uncertain effects.
    QUEUE_EXPIRED,     ///< Owner waiting consumed initial queue budget without transmission.
    DEADLINE,          ///< Immutable overall deadline expired while waiting in the owner.
    INVALIDATED,       ///< Current binding/node/session no longer matches queued input.
    ADMISSION_FAILED,  ///< Adapter refused dispatch after owner admission; nothing replayed.
    PREFLIGHT_REJECTED ///< Application pre-handoff guard withdrew work before handoff.
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
/** One ingress copy; processing time is supplied separately to service(). */
struct ReceivedFrame {
    Frame frame; ///< Complete owned frame, including standard/extended and data/RTR shape.
    bool receivedTimeKnown = false; ///< No invented capture timestamp when false.
    uint64_t receivedUs = 0;        ///< Qualified capture microseconds, zero valid, not future.
};
/** Actual controller report, never inferred from enqueue acceptance. */
struct TxReport {
    MotorControlCAN::CANOPEN::SdoTxId id; ///< Exact copied native handoff identity.
    bool transmitted = false; ///< True actual TX/ACK; false failure with uncertain effect.
};
/** Narrow synchronous application/controller boundary; no hidden receiver/task.
 * Callback contexts outlive the owner. Calls never reenter it or service storage.
 * receive/takeTxReport publish complete outputs only on true and retain no pointers.
 */
struct Transport {
    void* context = nullptr; ///< Borrowed stationary application/controller context.
    bool (*receive)(void*, ReceivedFrame&) = nullptr; ///< At most one frame per call; false empty.
    bool (*transmit)(void*, const Frame&, MotorControlCAN::CANOPEN::SdoTxId) = nullptr;
    ///< One-shot enqueue, borrowing frame only during call; true is queue acceptance only.
    bool (*takeTxReport)(void*, TxReport&) = nullptr; ///< One copied actual controller report.
    bool (*beforeTransmit)(void*, SequenceId, const MotorControlCAN::CANOPEN::SdoTransfer&,
                           uint64_t) = nullptr;
    ///< Optional first-download guard; notify typed settings before possible TX.
    void (*observe)(void*, const ReceivedFrame&, uint64_t) = nullptr;
    ///< Optional raw copy after dispatch/rejection, including malformed/future diagnostics;
    ///< not qualified evidence and cannot consume authoritative input.
};
/** Explicit per-turn budgets; values above fixed limits clamp, zero invokes no corresponding callback. */
struct ServiceBudget {
    uint8_t rx = 0;     ///< Max receive callbacks, clamped to RX_LIMIT; false stops draining.
    uint8_t tx = 0;     ///< Max one-shot enqueues, clamped to TX_LIMIT.
    uint8_t events = 0; ///< Max controller report callbacks, clamped to EVENT_LIMIT.
    uint8_t starts = 0; ///< Max new adapter admissions, clamped to START_LIMIT; needs TX budget.
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
/** Application-owned request/result scheduler using the sole existing adapter store.
 * The borrowed adapter contains all node/channel/PDO/event storage and is used
 * exclusively through this owner for admit/receive/TX/result progression. Configure
 * routes/observers before use or in this same cooperative context while idle.
 * No second receiver, node table, heap, clocks, automatic retry, urgent API or
 * motion executor. Transport callbacks are synchronous and nonreentrant.
 */
class CanBusOwner {
  public:
    enum {
        REQUESTS = 6,    ///< Queued + active + unread terminal capacity, shared ordinary slots.
        SEQUENCES = 2,   ///< Logical-axis reservations, independent of wire node numbering.
        PRODUCERS = 4,   ///< Cyclic eligible-producer fairness, FIFO within each sequence.
        RX_LIMIT = 8,    ///< Maximum receive callback calls per service turn.
        TX_LIMIT = 2,    ///< Maximum one-shot enqueue callbacks per turn.
        EVENT_LIMIT = 8, ///< Maximum TX report callback calls per turn.
        START_LIMIT = 2  ///< Maximum new native service admissions per turn.
    };
    /** Borrow stationary exclusive resources; construction performs no I/O.
     * @param[in] services Sole existing adapter, already configured or configured while idle.
     * @param[in] transport Copied callback bundle; context remains borrowed for owner lifetime.
     * @param[in] identity Nonzero application owner identity, suitable for settings reservations.
     * @param[in] lastIssued Last local serial; never wraps. Nonzero may preserve an application
     * serial domain, but never establishes wire freshness or revives prior handles.
     */
    CanBusOwner(MotorControlCAN::CANOPEN::SdoClient& services, const Transport& transport,
                uint32_t identity, uint32_t lastIssued = 0) noexcept;
    CanBusOwner(const CanBusOwner&) = delete;            ///< Stationary handles cannot be copied.
    CanBusOwner& operator=(const CanBusOwner&) = delete; ///< Cannot move live ownership.
    /** Reserve one actual logical binding, without a service or device action.
     * @param[in] binding Exact configured current channel binding; no node zero.
     * @param[in] producer Application identity 1..PRODUCERS.
     * @param[in] nowUs Nondecreasing supplied monotonic microseconds, zero valid.
     * @param[out] output Changed on success only; no overlap with owner or binding.
     * @return False atomically for stale/duplicate binding, capacity, time or exhausted IDs.
     */
    bool beginSequence(const Binding& binding, uint8_t producer, uint64_t nowUs,
                       SequenceId& output) noexcept;
    /** Start the next operation while retaining axis exclusion across prerequisite/settings reads.
     * @param[in] id Exact current non-invalidated reservation with no retained requests.
     * @param[in] operation Strictly greater nonzero operation identity than its predecessor.
     * @param[in] deadlineUs Exclusive immutable absolute cutoff, strictly after nowUs.
     * @param[in] nowUs Supplied nondecreasing microseconds.
     * @return False unchanged on invalid input; step sequence restarts only on success.
     */
    bool beginOperation(SequenceId id, uint32_t operation, uint64_t deadlineUs,
                        uint64_t nowUs) noexcept;
    /** Borrow current reservation facts without consuming them.
     * @param[in] id Exact current reservation.
     * @return Null if stale, foreign, invalidated or underlying route is unresolved/changed.
     */
    const Sequence* sequence(SequenceId id) const noexcept;
    /** Release axis exclusion only when all its request/results have been released.
     * @param[in] id Exact reserved identity; invalidated reservations may be released.
     * @return False unchanged for wrong ID, live work or unread terminals.
     */
    bool releaseSequence(SequenceId id) noexcept;
    /** Copy one yielded native request and reserve its immutable terminal slot.
     * @param[in] request Complete input; matching operation/deadline and increasing step.
     * @param[in] nowUs Same nondecreasing clock; queue/overall age begins here.
     * @param[out] output Unchanged on rejection; no overlap with owner or request.
     * @return ACCEPTED is local only. Rejection changes neither queue nor reservation.
     */
    BusAdmission admit(const BusRequest& request, uint64_t nowUs, RequestId& output) noexcept;
    /** Run one bounded cooperative turn; deadlines progress even with all budgets zero.
     * @param[in] nowUs Nondecreasing nonwrapping caller microseconds, zero valid.
     * @param[in] budget Clamped RX/TX/report/start budgets; zero prevents that work.
     * @return False without mutation for backward time/reentrancy/overlap. Accepted
     * frames enter normal adapter routing exactly once before observer copies.
     */
    bool service(uint64_t nowUs, const ServiceBudget& budget) noexcept;
    /** Non-consuming canonical result view.
     * @param[in] id Exact admitted request identity.
     * @return Immutable pointer until release/destruction; null pending/stale/foreign.
     */
    const Completion* result(RequestId id) const noexcept;
    /** Release one terminal slot only; never clears channel uncertainty or axis exclusion.
     * @param[in] id Exact terminal identity.
     * @return False unchanged for pending/stale/foreign IDs or callback reentrancy.
     */
    bool release(RequestId id) noexcept;
    /** Synchronize a completed exclusive settings transaction's configuration epoch.
     * @param[in] id Live reservation; every owner request on this node must be released.
     * @param[in] current Same binding with exactly next configuration epoch, already
     * withdrawn by the application's pre-TX settings notification.
     * @param[in] nowUs Same monotonic caller time.
     * @return Adapter's checked idle advance only; preserves frozen settings token,
     * node history and quarantine. No device action, reference renewal or recovery.
     */
    bool advanceConfiguration(SequenceId id, const Binding& current, uint64_t nowUs) noexcept;
    /** @return Explicit application owner identity, not a wire token. */
    uint32_t identity() const noexcept { return identity_; }

  private:
    struct Reservation {
        bool occupied = false;
        uint32_t generation = 0, lastStep = 0;
        Sequence value;
    } sequences_[SEQUENCES];
    struct Slot {
        bool occupied = false, terminal = false, submitted = false, guarded = false;
        uint32_t generation = 0, nodeGeneration = 0;
        uint64_t queueDeadlineUs = 0;
        BusRequest request;
        MotorControlCAN::CANOPEN::SdoRequestId native;
        Completion completion;
    } slots_[REQUESTS];
    MotorControlCAN::CANOPEN::SdoClient& services_;
    Transport transport_;
    uint32_t identity_, lastId_;
    uint64_t lastUs_ = 0;
    uint8_t nextProducer_ = 1, nextChannel_ = 0;
    int active_[MotorControlCAN::CANOPEN::SdoClient::CHANNELS] = {-1, -1};
    bool entered_ = false;
    bool valid(SequenceId id) const noexcept;
    bool valid(RequestId id) const noexcept;
    int channel(const Binding& binding) const noexcept;
    int channel(const MotorControlCAN::CANOPEN::SdoChannel& route) const noexcept;
    bool occupied(SequenceId id) const noexcept;
    int select(uint64_t nowUs) const noexcept;
    void collect() noexcept;
    void expire(uint64_t nowUs) noexcept;
    void finish(unsigned slot, CompletionReason reason, uint64_t nowUs) noexcept;
    bool transmit(uint8_t channel, uint64_t nowUs) noexcept;
};
} // namespace MotorControlCANExample
