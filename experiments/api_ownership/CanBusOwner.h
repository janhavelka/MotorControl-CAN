// SPDX-License-Identifier: MIT
/** @file CanBusOwner.h
 * @brief Fixed application ownership proof; never installed with the pure core.
 */
#ifndef MOTORCONTROL_CAN_API_PROOF_OWNER_H
#define MOTORCONTROL_CAN_API_PROOF_OWNER_H
#include "Contracts.h"
#include "fit_proof.h"

/** Application example boundary; stack types are confined to this directory. */
namespace MotorControlCANExample {
using namespace MotorControlCAN;
class CanBusOwner;

/** Stationary owner and nonwrapping slot generation; never transmitted on CAN. */
struct RequestId {
    const CanBusOwner* owner = nullptr; /**< Borrowed owner, valid during its lifetime. */
    uint8_t slot = 0; /**< Canonical request/result slot. */
    uint32_t generation = 0; /**< Zero is invalid. */
};
/** Reservation spans a whole operation, including gaps between service requests. */
struct SequenceId {
    const CanBusOwner* owner = nullptr; /**< Borrowed stationary owner. */
    uint8_t slot = 0; /**< Axis reservation slot. */
    uint32_t generation = 0; /**< Zero is invalid; never wraps. */
};
/** Raw asynchronous observation; no generic axis status is inferred. */
struct Observation {
    Frame frame; /**< Copied Classical heartbeat, EMCY or PDO frame from node 1. */
    uint64_t observedUs = 0; /**< Owner-supplied monotonic microseconds. */
};

/** One interface, one CANopen channel, two axes and three retained work slots.
 * Application-owned and stationary. No allocation, I/O, clock reads or threads.
 * Call cooperatively from one owner loop; not reentrant/ISR safe. One private
 * prompt-5 bridge holds the actual CANopenNode client. Its terminal is copied to
 * this owner's reserved slot and released before another transfer can start.
 * Construction requires a fresh qualified fake node-1 route. There is no recovery
 * shortcut for a channel with unresolved replies; destruction is not recovery.
 */
class CanBusOwner {
public:
    /** Compile-time proof capacities; prompts 13/15/23 extend this same table. */
    enum {
        AXES = 2, /**< Simultaneous logical axis reservations. */
        ORDINARY = 2, /**< Ordinary request/result slots. */
        REQUESTS = 3, /**< All slots, including one reserved urgent slot. */
        OBSERVATIONS = 4 /**< Copied native event slots. */
    };
    /** Initialize fixed storage without sending anything. */
    CanBusOwner() = default;
    CanBusOwner(const CanBusOwner&) = delete;
    CanBusOwner& operator=(const CanBusOwner&) = delete;

    /** Reserve one logical axis and endpoint for an operation, copying the binding.
     * @param[in] binding Qualified synthetic profile 1, bus 0, CANopen node 1/channel 0.
     * @param[in] operation Nonzero local operation identity.
     * @param[out] id Published only on success; refusal changes no storage.
     * @return False for invalid/duplicate binding, exhausted generation or capacity.
     */
    bool beginSequence(const Binding& binding, OperationToken operation, SequenceId& id);
    /** Release a reservation only after every associated request/result is released.
     * @param[in] id Current reservation. False leaves storage unchanged.
     * @return True if released; false for stale, foreign or still occupied work.
     */
    bool releaseSequence(SequenceId id);
    /** Copy work into an ordinary slot. All pointed-to caller state may then change.
     * @param[in] sequence Reservation for work's exact binding/operation.
     * @param[in] work Borrowed for this call; strictly increasing nonzero step.
     * Deadline is immutable absolute microseconds, at most 65,535,000 us ahead,
     * and at most UINT64_MAX - 65,535,000 (bridge timer headroom at dispatch).
     * The first admission fixes the reservation's deadline for every later step.
     * @param[in] nowUs Nondecreasing owner time; deadline must be in the future.
     * @param[out] id Published only on success. Result storage is reserved here.
     * @return False atomically for bad work/route/time, capacity or unresolved channel.
     */
    bool admit(SequenceId sequence, const ServiceWork& work, uint64_t nowUs, RequestId& id);
    /** Same contract as admit, using the one reserved urgent slot.
     * Does not preempt an active transfer or an earlier step of the same sequence.
     * One urgent start is followed by eligible ordinary work when available.
     * @param[in] sequence Existing reservation; not a success-dependency graph.
     * @param[in] work Borrowed complete work, copied with admit's validations.
     * @param[in] nowUs Same monotonic clock and bounds as admit.
     * @param[out] id Published on success only.
     * @return True when admitted; false atomically for invalid work or full slot.
     */
    bool admitUrgent(SequenceId sequence, const ServiceWork& work, uint64_t nowUs, RequestId& id);
    /** Bounded cooperative progress; expires queued work before starting a transfer.
     * Scans the three request slots and starts at most one transfer. Consumes no
     * RX input, copies no observation and performs no fake TX handoff; it may
     * stage a frame in the single retirable TX slot. At most two bridge service calls.
     * @param[in] nowUs Nondecreasing microseconds; backwards input returns false
     * without mutation. At equality the absolute deadline wins over any response.
     * @return True for accepted time, even when work terminates with a failure.
     */
    bool service(uint64_t nowUs);
    /** Sole RX entry; asynchronous dispatch continues with no active SDO request.
     * Accepts at most one supplied frame and copies at most one raw event. Performs
     * no fake TX handoff; service may start one transfer. At most four bridge service
     * calls. A zero supplied-RX budget means the caller omits this entry for the turn.
     * @param[in] frame Borrowed for this call; checked and copied before return.
     * @param[in] nowUs Same clock as service. Invalid frame/time returns false.
     * Only Classical standard data frames enter this CANopen proof adapter.
     * @return True for valid input dispatch, including unmatched/ignored frames.
     */
    bool receive(const Frame& frame, uint64_t nowUs);
    /** Explicit fake controller handoff: first point transmission becomes possible.
     * Hands off at most one frame, with no RX input or observation copy; service
     * may start one transfer. At most three bridge service calls. Omit this entry
     * for a zero TX budget; queued work can still expire through service.
     * @param[in] nowUs Same clock; expiry retires queued frames before handoff.
     * @return True for a handoff, never proof of CAN ACK or SDO/motion success.
     */
    bool handoff(uint64_t nowUs);
    /** Local cancellation only; never sends stop, disable, reset or retry.
     * @param[in] id Current nonterminal request.
     * @param[in] nowUs Same clock; deadline closure takes precedence.
     * @return True for cancellation. Pending fake TX is retired; prior handoff
     * remains uncertain and interlocks the channel, including for a read.
     */
    bool cancel(RequestId id, uint64_t nowUs);
    /** Non-consuming retained evidence; pointer valid until release/destruction.
     * @param[in] id Current request. Null for pending, stale or foreign handles.
     * @return Borrowed immutable terminal view, or null when no result is available.
     */
    const ServiceEvidence* result(RequestId id) const;
    /** Release only a matching terminal slot; does not release its axis reservation.
     * @param[in] id Current request. False leaves storage unchanged.
     * No terminal release clears channel uncertainty or authorizes retransmission.
     * @return True if released; false for nonterminal, stale or foreign handles.
     */
    bool release(RequestId id);
    /** Consume oldest copied event; output unchanged if empty. Overflow drops newest.
     * @param[out] out Copy valid independently of owner storage on success.
     * @return True if an observation was consumed; false if the ring is empty.
     */
    bool takeObservation(Observation& out);
    /** Saturating observation-loss count; no motor/health policy implied.
     * @return Number of dropped newest events, saturating at UINT32_MAX. */
    uint32_t droppedObservations() const { return storage_.dropped; }
    /** Inspect channel qualification without I/O or mutation.
     * @return True when no new transfer can safely use this channel. */
    bool channelUnresolved() const { return storage_.unresolved; }
    /** Inspection of last fake handoff; false and output unchanged before any handoff.
     * @param[out] out Copied wire bytes, not acknowledgement evidence.
     * @return True when a fake handoff has occurred, false with output unchanged otherwise.
     */
    bool lastTransmitted(Frame& out) const;

private:
    struct Reservation {
        bool occupied = false;
        uint32_t generation = 0, lastStep = 0;
        uint64_t deadlineUs = 0;
        Binding binding;
        OperationToken operation;
    };
    struct Slot {
        bool occupied = false, terminal = false, submitted = false;
        uint32_t generation = 0;
        uint8_t sequence = 0;
        ServiceWork work;
        ServiceEvidence evidence;
    };
    // One authoritative application table. The bridge's terminal is transient
    // client scratch, not a second retained request registry or RX dispatcher.
    struct OwnerStorage {
        CanopenFit::CanBusOwner channel;
        CanopenFit::RequestId activeId = {nullptr, 0};
        int active = -1;
        bool unresolved = false, urgentStarted = false;
        uint8_t nextAxis = 0;
        Reservation axes[AXES];
        Slot slots[REQUESTS];
        Observation observations[OBSERVATIONS];
        uint8_t observationHead = 0, observationCount = 0;
        uint32_t dropped = 0;
        uint64_t lastUs = 0;
    } storage_;
    bool valid(SequenceId id) const;
    bool valid(RequestId id) const;
    bool admitInto(SequenceId sequence, const ServiceWork& work, uint64_t nowUs,
                   RequestId& id, bool urgent);
    bool eligible(unsigned slot) const;
    int select() const;
    void finish(unsigned slot, EventKind kind, uint64_t nowUs);
    void collect(uint64_t nowUs, bool overrideCause = false, EventKind cause = EventKind::CANCEL);
};
} // namespace MotorControlCANExample
#endif
