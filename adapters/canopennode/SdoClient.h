/** @file SdoClient.h
 * @brief Optional cooperative CANopenNode adapter; not an installed-core dependency.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/canopen/SdoTransfer.h>
#include "301/CO_SDOclient.h"

namespace MotorControlCAN {
namespace CANOPEN {
class SdoClient;
/** Nonwrapping local handle; no field is transmitted on CAN. */
struct SdoRequestId {
    const SdoClient* owner = nullptr; ///< Stationary originating adapter, valid for its lifetime.
    uint8_t channel = 0; ///< Fixed physical channel slot.
    uint32_t generation = 0; ///< Nonzero local admission number; exhaustion rejects admission.
};
/** Per-frame handoff identity, protecting controller reports from stale work. */
struct SdoTxId {
    SdoRequestId request; ///< Exact owning admission.
    uint32_t frame = 0; ///< Nonzero monotonic frame number within this admission.
};
/** Bounded optional stack adapter extracted from the prompt-6 channel storage.
 * Two physical channels, each with one active/retained request, one 32-byte
 * upstream FIFO, owned 32-byte upload and retirable software TX staging.
 * One cooperative application context dispatches every frame on this interface.
 * No controller I/O, heap, clock reads, automatic retries, ISR use or reentrancy.
 * Contexts cannot move while callback pointers/handles exist. Application TX
 * acceptance may already permit transmission; nextTx conservatively marks that
 * boundary before returning bytes. Quarantine survives release and all tokens.
 * Success reuse requires a qualified peer/controller premise that completed
 * exchanges will not later be duplicated; host tokens cannot establish it.
 */
class SdoClient {
public:
    /** Fixed storage capacities, independent of the number of network nodes or PDOs. */
    enum { CHANNELS = 2 ///< Channels on this physical interface.
    };
    SdoClient() = default; ///< Initializes storage without configuring or transmitting a route.
    SdoClient(const SdoClient&) = delete; ///< Callback addresses and owner identities cannot be copied.
    SdoClient& operator=(const SdoClient&) = delete; ///< Assignment cannot transfer live ownership.

    /** Configure a previously unused slot, with no traffic or guessed default IDs.
     * Duplicate/crossing CAN-IDs on this interface reject, irrespective of logical
     * axis/subaxis/host epochs. A configured slot cannot be rebound or reopened.
     * @param[in] slot Fixed slot 0..CHANNELS-1.
     * @param[in] channel Source-backed route; copied after full validation.
     * @param[in] externallyConfirmed True only for an externally established fresh
     * route/session premise plus bounded reviewed non-changing confirmation.
     * False admits only a reviewed non-changing upload until one succeeds.
     * No current Lichuan read is thereby declared qualified.
     * @param[in] freshSessionQualified Required external premise that the local
     * TX/RX queues and remote session contain no indistinguishable old work.
     * A fresh C++ object or successful probe cannot establish this premise.
     * @return False atomically for invalid, duplicate or already configured route.
     */
    bool configure(uint8_t slot, const SdoChannel& channel, bool externallyConfirmed,
                   bool freshSessionQualified);
    /** Copy complete work and reserve its result, without initiating TX.
     * @param[in] slot Configured unique channel, not unresolved or occupied.
     * @param[in] transfer Borrowed complete request with explicit size 1..32,
     * nonzero identities/budgets and future exclusive overall deadline.
     * Download data are copied before return. Block and larger forms reject.
     * @param[in] nowUs Nondecreasing supplied microseconds, including zero; never wraps.
     * @param[out] id Published only on success; no alias with input or adapter storage.
     * @return False leaves all storage/output unchanged, including admission refusal.
     */
    bool admit(uint8_t slot, const SdoTransfer& transfer, uint64_t nowUs, SdoRequestId& id);
    /** At most one initiation, two engine progression calls and one FIFO drain per channel.
     * Overall expiry wins at equality, before queue/response work. All subcutoffs
     * use remaining-budget subtraction, so UINT64_MAX cannot overflow additions.
     * @param[in] nowUs Same nondecreasing clock; backward input changes nothing.
     * @return False only for invalid time; terminal results remain immutable.
     */
    bool service(uint64_t nowUs);
    /** Sole application dispatcher offers one borrowed frame; no RX pointer retained.
     * Bounded by service for both channels plus one receive callback, two engine
     * progression calls and one FIFO drain on the selected channel.
     * Unmatched IDs, extended/RTR and other nodes remain available to the caller.
     * A matching malformed envelope fails locally; wrong-object replies are ignored.
     * @param[in] frame Borrowed frame; copied before any selected-engine callback.
     * @param[in] nowUs Processing time; deadline wins before callback at equality.
     * @return True only if a live matching SDO route consumed the frame.
     */
    bool receive(const Frame& frame, uint64_t nowUs);
    /** Hand off at most one staged request/cleanup frame, marking possible TX.
     * Caller must never transmit cached bytes after a failed call or replay them.
     * Queue acceptance/CAN ACK cannot produce protocol success. A pending cleanup
     * abort is bounded by the original deadline and never reopens its channel.
     * @param[in] slot Configured channel to service.
     * @param[in] nowUs Same supplied time; expiry retires software staging first.
     * @param[out] frame Owned copy on success; unchanged on failure.
     * @param[out] id Handoff identity on success; unchanged on failure.
     * @return False for invalid slot/time, no eligible TX, overlap or expired work.
     */
    bool nextTx(uint8_t slot, uint64_t nowUs, Frame& frame, SdoTxId& id);
    /** Record actual controller TX/ACK separately from protocol confirmation.
     * @param[in] id Most recent live request frame handoff; stale/foreign reports reject.
     * @param[in] nowUs Same supplied time; deadline has priority.
     * @return True on recorded confirmation; terminal evidence is never rewritten.
     */
    bool confirmTx(SdoTxId id, uint64_t nowUs);
    /** Local cancel without pumping unsent work or sending a motor stop.
     * @param[in] id Current nonterminal request.
     * @param[in] nowUs Same time; cutoff closure has priority over cancellation.
     * @return True only if cancellation won; queued software TX is retired.
     */
    bool cancel(SdoRequestId id, uint64_t nowUs);
    /** Report controller failure on this interface; retire local staging, never replay.
     * @param[in] nowUs Supplied monotonic time.
     * @return False for backward time; otherwise all configured channels quarantine.
     */
    bool busOff(uint64_t nowUs);
    /** Invalidate every channel of a rebooted node, independent of event-ring space.
     * @param[in] node Explicit CANopen node 1..127, not a generic axis identity.
     * @param[in] nowUs Same supplied time.
     * @return False for invalid node/time; first terminal results remain stable.
     */
    bool reboot(uint8_t node, uint64_t nowUs);
    /** Retain an adapter/controller failure without implicit retry or requalification.
     * @param[in] id Current nonterminal request.
     * @param[in] nowUs Same supplied time; timeouts take priority.
     * @return True when the failure terminates work, false for stale/expired handles.
     */
    bool transportFailure(SdoRequestId id, uint64_t nowUs);
    /** Non-consuming immutable result, valid until release or adapter destruction.
     * @param[in] id Current admitted handle.
     * @return Null for pending/stale/foreign; never changes time or storage.
     */
    const SdoCompletion* result(SdoRequestId id) const;
    /** Release terminal result only; quarantine and bounded cleanup remain independent.
     * @param[in] id Exact current terminal handle.
     * @return False leaves storage unchanged; true invalidates its borrowed result view.
     */
    bool release(SdoRequestId id);
    /** Inspect persistent ambiguity; never infers recovery from time or client abort.
     * @param[in] slot Fixed channel slot.
     * @return True for invalid/unconfigured or permanently unresolved channel.
     */
    bool unresolved(uint8_t slot) const;
    /** Read-only stack-port staging inspection; no controller/SDK is owned here.
     * @param[in] slot Valid configured slot; invalid slot returns null.
     * @return Borrowed native staging view for the stationary adapter lifetime.
     */
    const CO_CANmodule_t* port(uint8_t slot) const;

private:
    struct Channel {
        CO_CANmodule_t port = {};
        CO_SDOclient_t engine = {};
        SdoChannel route;
        bool configured = false, confirmed = false, unresolved = false;
        bool occupied = false, terminal = false, started = false, awaiting = false;
        bool queued = false, possible = false, txConfirmed = false, cleanup = false;
        bool unsizedExpedited = false;
        uint32_t generation = 0, frame = 0;
        uint64_t queueCutoff = 0, responseCutoff = 0;
        SdoTransfer work;
        SdoCompletion completion;
        uint8_t upload[32] = {}, uploaded = 0;
        Frame lastReply;
        bool replyValid = false;
    } channels_[CHANNELS];
    uint64_t lastUs_ = 0;
    bool valid(SdoRequestId id) const;
    void expire(Channel& c, uint64_t nowUs);
    void pump(Channel& c, uint64_t nowUs);
    void finish(Channel& c, SdoOutcome outcome, uint64_t nowUs, int32_t returned = 0,
                bool abortValid = false, uint32_t abortCode = 0);
    void abort(Channel& c, SdoOutcome outcome, uint32_t code, uint64_t nowUs);
    void retire(Channel& c);
};
} // namespace CANOPEN
} // namespace MotorControlCAN
