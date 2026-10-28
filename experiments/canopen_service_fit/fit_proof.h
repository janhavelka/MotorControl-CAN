// SPDX-License-Identifier: MIT
/** @file
 * Provisional single-channel owner for the native service fit experiment.
 */
#ifndef MOTORCONTROL_CAN_FIT_PROOF_H
#define MOTORCONTROL_CAN_FIT_PROOF_H

#include "301/CO_SDOclient.h"

/** Local experiment declarations, excluded from any installed library ABI. */
namespace CanopenFit {

class CanBusOwner;
/** Local ownership handle; never appears on CAN and cannot qualify a stale reply. */
struct RequestId {
    const CanBusOwner* owner; /**< Originating live, stationary owner. */
    uint32_t serial; /**< Nonwrapping local admission number. */
};

/** Logical service terminal; success says nothing about motor motion. */
enum class Outcome {
    Success, /**< SDO protocol exchange completed, with no motion-completion claim. */
    ServerAbort, /**< Peer returned a matching raw SDO abort. */
    ClientAbort, /**< Stack rejected a response or transfer. */
    Timeout, /**< Absolute admission deadline expired. */
    Cancelled, /**< Caller ended local work; any prior handoff remains relevant. */
    Transport /**< Fake driver rejected a send; evidence retained independently. */
};

/** Immutable first terminal, retained until explicit release. */
struct Completion {
    Outcome outcome; /**< Terminal cause; subsequent service calls cannot replace it. */
    CO_SDO_return_t stackReturn; /**< Native stack return, or owner timeout/cancel marker. */
    uint32_t abortCode; /**< Native abort code, zero when inapplicable. */
    bool executionUnknown; /**< Download may have reached the peer without confirmation. */
    uint8_t data[32]; /**< Owned upload bytes; only data[0..size) is valid. */
    size_t size; /**< Uploaded bytes on success; zero on other outcomes. */
};

/** Single-slot cooperative owner; caller keeps its address stable for its lifetime.
 * No heap, clocks, threads, device I/O or automatic retry. One service, one result,
 * one 32-byte FIFO and one retirable fake TX slot. Failed/uncertain transmitted
 * work permanently interlocks this proof instance; release never reconciles a wire
 * channel. Construction assumes a fresh externally qualified route to fake node 1.
 */
class CanBusOwner {
public:
    /** Set up the actual upstream SDO client using a local 0x1280 record. */
    CanBusOwner();
    CanBusOwner(const CanBusOwner&) = delete;
    CanBusOwner& operator=(const CanBusOwner&) = delete;

    /** Admit one service atomically; refusal leaves slot and id unchanged.
     * @param[in] download True for a write, false for a read.
     * @param[in] index Synthetic test object, no manufacturer applicability claimed.
     * @param[in] subIndex Synthetic test subindex.
     * @param[in] bytes Non-null write buffer, borrowed only during this call and
     * copied into the upstream FIFO. Ignored for reads; pass null.
     * @param[in] size Write length 1..32; must be zero for reads.
     * @param[in] nowUs Supplied monotonic microseconds, including valid zero.
     * @param[in] timeoutMs Explicit nonzero policy, not a drive timing guarantee.
     * @param[out] id Local handle, assigned on success only; no wire correlation.
     * @return True if admitted; retained results block further admission.
     */
    bool admit(bool download, uint16_t index, uint8_t subIndex, const uint8_t* bytes,
               size_t size, uint64_t nowUs, uint16_t timeoutMs, RequestId& id);

    /** Advance at most one stack call, no RX/TX handoff loop.
     * @param[in] nowUs Supplied nondecreasing monotonic microseconds. The absolute
     * admission deadline wins before any received evidence at the same time.
     */
    void service(uint64_t nowUs);

    /** Sole RX dispatch entry; copies events and synchronously processes one SDO.
     * @param[in] frame Borrowed only during this call; callback and event data copied.
     * @param[in] nowUs Supplied monotonic microseconds, same clock as admit/service.
     * Unmatched/wrong-object/extended/RTR frames never enter the SDO callback.
     */
    void receive(const FitFrame& frame, uint64_t nowUs);

    /** Move queued TX to a possibly transmitted state; no CAN ACK is inferred.
     * @param[in] nowUs Supplied monotonic microseconds; deadline is enforced before
     * transfer. Backwards time refuses the handoff without mutation.
     * @return False if no eligible frame remains, including deadline closure.
     */
    bool handoff(uint64_t nowUs);

    /** Cancel locally, retiring queued fake TX; never sends a motor stop.
     * @param[in] id Must identify current nonterminal work.
     * @return True when cancelled; prior irreversible write handoff stays uncertain.
     */
    bool cancel(RequestId id);

    /** Non-consuming terminal view; valid until release or owner destruction.
     * @param[in] id Current admitted local handle.
     * @return Null for pending, foreign or stale handle; performs no I/O/time read.
     */
    const Completion* result(RequestId id) const;

    /** Release matching terminal only; does not make an ambiguous route reusable.
     * @param[in] id Current local handle.
     * @return True on release; false leaves all storage unchanged.
     */
    bool release(RequestId id);

    /** Read-only fake controller inspection, valid for owner lifetime. */
    const CO_CANmodule_t& controller() const { return controller_; }
    /** Count of checked synthetic heartbeat events, independent of SDO results. */
    uint32_t heartbeats() const { return heartbeats_; }
    /** Count of copied synthetic EMCY events. */
    uint32_t emergencies() const { return emergencies_; }
    /** Count of copied synthetic PDO events; no profile mapping is inferred. */
    uint32_t pdos() const { return pdos_; }
    /** Last copied asynchronous frame; valid when any event count is nonzero. */
    const FitFrame& lastEvent() const { return lastEvent_; }
    /** Inject deterministic controller send failure; not a production API. */
    void refuseSend() { controller_.refuseSend = true; }

private:
    void finish(Outcome outcome, CO_SDO_return_t returned, uint32_t abortCode);
    void retireQueued();
    bool ready_;
    CO_CANmodule_t controller_;
    CO_SDOclient_t client_;
    Completion completion_;
    bool occupied_, terminal_, download_, interlocked_;
    uint32_t id_;
    uint16_t index_;
    uint8_t subIndex_;
    uint64_t deadlineUs_, lastUs_;
    uint32_t handoffsAtAdmission_;
    uint32_t heartbeats_, emergencies_, pdos_;
    FitFrame lastEvent_;
};
} // namespace CanopenFit
#endif
