/** @file SdoClient.h
 * @brief Optional cooperative CANopenNode adapter; not an installed-core dependency.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/canopen/SdoTransfer.h>
#include "NetworkState.h"
#include "Nmt.h"
#include "PdoState.h"
#include "301/CO_SDOclient.h"
#include "301/CO_NMT_Heartbeat.h"

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
 * Every exchange requires a qualified peer/controller premise that old responses
 * cannot arrive as a later exchange's reply. This includes individual segments
 * within one transfer; a repeated toggle or host token cannot establish freshness.
 */
class SdoClient {
public:
    /** Fixed storage capacities, independent of the number of network nodes or PDOs. */
    enum { CHANNELS = 2, ///< Channels on this physical interface.
           NETWORK_HISTORY = 4, ///< Copied passive/unknown frame records; overflow drops newest.
           PDO_ROUTES = 8 ///< Permanently bound PDO wire routes on this interface; distinct from profile set count.
    };
    SdoClient() = default; ///< Initializes storage without configuring or transmitting a route.
    SdoClient(const SdoClient&) = delete; ///< Callback addresses and owner identities cannot be copied.
    SdoClient& operator=(const SdoClient&) = delete; ///< Assignment cannot transfer live ownership.

    /** Admit one explicit manager request, without TX or remote state changes.
     * @param[in] work Copied command, bus/target and immutable future budgets.
     * Every local SDO binding must have an existing network anchor. Membership
     * is frozen until release; no node-zero binding is created for broadcast.
     * @param[in] nowUs Nondecreasing caller microseconds.
     * @param[out] id Published on success only; no overlap with work or owner.
     * @return False atomically on invalid target, occupied result, pending target,
     * stale epoch, controller failure, invalid budget or generation exhaustion.
     */
    bool admitNmt(const NmtTransfer& work, uint64_t nowUs, NmtRequestId& id);
    /** Yield the one native-producer frame, conservatively marking possible TX.
     * @param[in] id Exact live admission.
     * @param[in] nowUs Same clock; cutoffs win at equality.
     * @param[out] frame Owned copy, unchanged on failure; no overlap with owner/id.
     * @return True once only. Lifecycle effects execute before returning bytes.
     * Serialize canTransmitNmt/enqueue with receive/lifecycle; never replay.
     */
    bool nextNmtTx(NmtRequestId id, uint64_t nowUs, Frame& frame);
    /** Check an unconfirmed live NMT handoff without mutation or clock reads.
     * @param[in] id Exact current handoff/admission.
     * @param[in] nowUs Same clock, at least last mutation and before deadline.
     * @return False after cancellation, deadline, TX confirmation or invalidation.
     */
    bool canTransmitNmt(NmtRequestId id, uint64_t nowUs) const;
    /** Record actual TX and publish local TRANSMITTED terminal, never remote state.
     * @param[in] id Exact live handed-off request.
     * @param[in] nowUs Same clock; deadline priority applies.
     * @return True only for first valid confirmation; result remains until release.
     */
    bool confirmNmtTx(NmtRequestId id, uint64_t nowUs);
    /** Cancel local work without transmitting a network/motor stop.
     * @param[in] id Exact nonterminal request.
     * @param[in] nowUs Same supplied clock; expiration takes precedence.
     * @return True if cancellation wins; possible earlier effects remain possible.
     */
    bool cancelNmt(NmtRequestId id, uint64_t nowUs);
    /** Inspect immutable local terminal; borrowed until release/destruction.
     * @param[in] id Exact retained admission.
     * @return Null while pending or for stale/foreign IDs; no observation is consumed.
     */
    const NmtCompletion* nmtResult(NmtRequestId id) const;
    /** Release only the retained local result; intent/evidence are independent.
     * @param[in] id Exact terminal admission.
     * @return False unchanged for invalid/pending IDs; never requalifies a node/channel.
     */
    bool releaseNmt(NmtRequestId id);
    /** Inspect native producer staging for application diagnostics/preflight.
     * @return Borrowed fixed port for owner lifetime; no RX callback is registered.
     */
    const CO_CANmodule_t* nmtPort() const { return &nmtPort_; }
    /** Select explicit inclusive evidence age, independently of heartbeat producer timing.
     * @param[in] slot Node anchor or its SDO slot.
     * @param[in] maximumAgeUs Nonzero microseconds; UINT64_MAX selects lifecycle-only validity.
     * @param[in] nowUs Same clock; this does not refresh observations.
     * @return False atomically for invalid slot/age/time.
     */
    bool setServiceAge(uint8_t slot, uint64_t maximumAgeUs, uint64_t nowUs);
    /** Install explicitly established actual map contents, without any writes or automatic sample qualification.
     * @param[in] slot PDO route slot 0..PDO_ROUTES-1. Once bound, its identifier/frame kind cannot be reused/rebound.
     * @param[in] nodeSlot Existing shared node/SDO slot, with exactly matching binding.
     * @param[in] evidence Owned actual-map candidate, complete communication facts and nonzero evidence identity.
     * Template provenance rejects. Replacement mapping epoch must strictly increase;
     * evidence time cannot predate the node/route boundary or lie in the future.
     * @param[in] profile Call-only reviewed layout facts; no pointer is retained.
     * @param[in] observer Borrowed stationary bounded cache consumer/invalidator.
     * @param[in] nowUs Supplied nondecreasing monotonic microseconds.
     * @return False atomically for incomplete/stale/invalid evidence, collision,
     * occupied NMT membership, invalid observer or exhausted epoch. Success
     * revokes prior sample validity and notifies its consumer, never confirms a sample.
     */
    bool configurePdo(uint8_t slot, uint8_t nodeSlot, const PdoMapEvidence& evidence,
                      const PdoProfile& profile, PdoObserver observer, uint64_t nowUs);
    /** Establish external sample provenance independently of actual-map contents.
     * @param[in] slot Bound PDO route with current confirmed map.
     * @param[in] evidence Exact route/lifecycle scope, nonzero source record and
     * reviewed producer+all-ingress retirement or genuinely distinct non-reused
     * route premise. A new host epoch, readback or timestamp alone is insufficient.
     * After map replacement/lifecycle/NMT withdrawal, this fixed reused route
     * requires producer+ingress retirement; distinct-route proof cannot reopen it.
     * @param[in] nowUs Same clock; evidence boundary must be current, not future.
     * @return False atomically on invalid/stale evidence or an already qualified
     * route; explicitly invalidatePdo(false) before replacing its proof, so
     * dependent consumers are revoked too. Success affects only
     * future frames with known capture strictly after its boundary; no diagnostic
     * or old accepted sample is promoted retroactively.
     */
    bool qualifyPdo(uint8_t slot, const PdoQualification& evidence, uint64_t nowUs);
    /** Withdraw one uncertain map or its sample provenance; other routes stay independent.
     * @param[in] slot Bound PDO route.
     * @param[in] mapUncertain True revokes map confirmation too; false preserves map contents.
     * @param[in] nowUs Same supplied clock; raw observation history remains retained.
     * @return False atomically on invalid slot/time; no reconfiguration, traffic or retry.
     */
    bool invalidatePdo(uint8_t slot, bool mapUncertain, uint64_t nowUs);
    /** Inspect confirmed-map/sample/history facts without I/O or mutation.
     * @param[in] slot Bound PDO route.
     * @return Borrowed record until destruction; subsequent mutation overwrites it, so copy retained history.
     */
    const PdoRecord* pdo(uint8_t slot) const;

    /** Configure a previously unused slot, with no traffic or guessed default IDs.
     * Duplicate/crossing CAN-IDs or aliases of the same peer node/server channel
     * reject, irrespective of logical axis/subaxis/host epochs. A configured slot
     * cannot be rebound or reopened. Distinct server channels need device evidence.
     * @param[in] slot Fixed slot 0..CHANNELS-1.
     * @param[in] channel Source-backed route; copied after full validation.
     * @param[in] externallyConfirmed True only for an externally established fresh
     * route/session premise plus bounded reviewed non-changing confirmation.
     * Its conservative timestamp is the owner's last supplied processing time;
     * installing a network anchor later does not refresh that evidence.
     * False admits only a reviewed non-changing upload until one succeeds.
     * No current Lichuan read is thereby declared qualified.
     * @param[in] freshSessionQualified Required external premise that the local
     * TX/RX queues and remote session contain no indistinguishable old work.
     * A fresh C++ object or successful probe cannot establish this premise.
     * Also requires no delayed duplicate response from any completed initiation or
     * segment exchange, and exclusive ownership of this peer's SDO server channel.
     * These premises apply during a transfer as well as between transfers.
     * A different host epoch is not a new server.
     * @return False atomically for invalid, duplicate or already configured route.
     */
    bool configure(uint8_t slot, const SdoChannel& channel, bool externallyConfirmed,
                   bool freshSessionQualified);
    /** Configure passive routing in an existing unused network slot, without I/O.
     * A passive-only slot needs no SDO session premise. One route per physical
     * node; other SDO channels share that anchor. All identifiers must be unique
     * across network and SDO routes on this interface, in either configure order.
     * @param[in] slot Fixed channel slot; existing SDO binding must match route.
     * @param[in] route Explicit current binding, event epoch and actual identifiers.
     * @param[in] observer Borrowed stationary cache invalidator; no reentrancy/I/O.
     * @param[in] nowUs Nondecreasing monotonic microseconds; establishes the boundary.
     * Existing usable SDO service evidence retains its original time and origin;
     * unknown capture remains unknown and quarantined channels supply no premise.
     * @return False atomically for invalid/duplicate route, observer or time.
     */
    bool configureNetwork(uint8_t slot, const NetworkRoute& route, NetworkObserver observer, uint64_t nowUs);
    /** Attach one invalidator to a configured SDO channel sharing its node anchor.
     * @param[in] slot Existing SDO/network slot.
     * @param[in] observer Stationary borrowed context/callback; both null detaches.
     * @return False for invalid slot/observer; no callback is invoked on attachment.
     */
    bool setNetworkObserver(uint8_t slot, NetworkObserver observer);
    /** Inspect shared physical-node facts without clocks or I/O.
     * @param[in] slot Passive or SDO slot with a configured network anchor.
     * @return Borrowed view until destruction; mutations overwrite fields, so copy history.
     */
    const NodeSnapshot* node(uint8_t slot) const;
    /** Accept explicit earliest-ingress provenance for a passive frame.
     * @param[in] frame Borrowed once; copied before mutation.
     * @param[in] source Binding/epoch captured at ingress, not a wire generation tag.
     * @param[in] receivedUs Source-qualified capture microseconds, at most nowUs.
     * @param[in] nowUs Nondecreasing processing time. Future time rejects atomically.
     * @return True only for a checked current event; stale/rejected data remain diagnostic.
     */
    bool receiveNetwork(const Frame& frame, const NetworkRoute& source, uint64_t receivedUs, uint64_t nowUs);
    /** Set externally established effective producer timing, without writing a node.
     * @param[in] slot Node anchor or one of its SDO slots.
     * @param[in] source Exact current route binding/epoch.
     * @param[in] milliseconds Effective U16 interval, zero explicitly disables production.
     * @param[in] nowUs Same supplied processing clock.
     * @return False atomically for stale/invalid input; heartbeat facts are not renewed.
     */
    bool setProducerInterval(uint8_t slot, const NetworkRoute& source, uint16_t milliseconds, uint64_t nowUs);
    /** Revoke node-dependent evidence and all its SDO channels, with no reopening.
     * Synchronous callbacks run before later TX and regardless of history capacity.
     * @param[in] slot Node anchor or matching SDO slot.
     * @param[in] cause Explicit reset/binding/dialect/configuration/recovery reason.
     * @param[in] nowUs Same processing clock; epochs never wrap, exhaustion fails closed.
     * @return False atomically for invalid slot/reason/time; otherwise invalidates.
     */
    bool invalidateNetwork(uint8_t slot, NetworkInvalidation cause, uint64_t nowUs);
    /** Report controller state independently of NMT/drive state; no I/O occurs.
     * @param[in] state Application-reported controller state.
     * @param[in] nowUs Same clock. BUS_OFF/RECOVERING revoke every node/channel;
     * AVAILABLE restores no node evidence or service qualification.
     * @return False atomically for invalid state/backward time.
     */
    bool reportController(ControllerState state, uint64_t nowUs);
    /** @return Cached application-reported controller state; never probes hardware. */
    ControllerState controllerState() const { return controllerState_; }
    /** Consume one copied observer record, independent of results and lifecycle.
     * @param[out] output Owned copy on success; unchanged if empty or overlapping owner.
     * @return True when one record was copied; never calls a controller or callback.
     */
    bool takeNetworkObservation(NetworkObservation& output);
    /** @return Saturating count of newest observer records dropped on full history. */
    uint32_t droppedNetworkObservations() const { return networkDropped_; }
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
     * A matching abort after possible TX retires even a not-yet-handed-off next segment.
     * @param[in] frame Borrowed frame; copied before any selected-engine callback.
     * @param[in] nowUs Processing time; deadline wins before callback at equality.
     * Capture time is unknown in this overload, never inferred from processing time.
     * @return True only if live protocol validation consumed the frame. False can
     * still retain a raw observation or quarantine unsolicited/late route traffic.
     */
    bool receive(const Frame& frame, uint64_t nowUs);
    /** Offer one frame with an externally qualified capture time in the same clock domain.
     * Processing-time expiry still wins even if capture preceded the cutoff.
     * Capture before the relevant handoff proves stale traffic and quarantines;
     * equality cannot resolve ordering finer than the timestamp resolution.
     * @param[in] frame Borrowed raw frame, copied before any mutation/callback.
     * @param[in] receivedUs Nonwrapping capture microseconds; zero valid, at most nowUs.
     * @param[in] nowUs Nondecreasing processing time; no cutoff can be backdated.
     * @return True only for a live response consumed by protocol validation. Invalid
     * time rejects without mutation; late/unexpected traffic may be retained and
     * quarantine its route while returning false. Neither overload reopens a channel.
     */
    bool receive(const Frame& frame, uint64_t receivedUs, uint64_t nowUs);
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
    /** Recheck copied TX eligibility immediately before an application/controller enqueue.
     * Pure check: no progression, queue mutation, time acquisition or physical retirement.
     * Caller serializes check/enqueue with lifecycle events and enqueues each handoff
     * at most once. True is not permission to replay a previously accepted frame.
     * @param[in] id Exact most recent live handoff, including bounded cleanup after release.
     * @param[in] nowUs Same processing clock, before its cutoff. Values before the
     * last mutating entry reject; this pure check does not advance that clock.
     * The caller must still supply nondecreasing time across its calls.
     * @return False after reply, confirmation, cancellation, expiry, invalidation or
     * superseding handoff. A controller's already accepted queue still needs actual
     * retirement evidence; this function cannot recall bytes outside the adapter.
     */
    bool canTransmit(SdoTxId id, uint64_t nowUs) const;
    /** Record actual controller TX/ACK separately from protocol confirmation.
     * A report can follow a nonfinal reply until the next frame handoff supersedes
     * its token. Recording it neither renews a cutoff nor rewrites a terminal.
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
    /** Withdraw one binding without overwriting the route or clearing possible effects.
     * @param[in] slot Configured fixed channel; other physical nodes remain independent.
     * @param[in] nowUs Same processing clock; expiry wins before invalidation.
     * @return False atomically for invalid slot/time; true quarantines permanently,
     * retires local staging and closes live work. With a passive anchor, shared
     * node-dependent evidence and its channels are revoked too. Other physical
     * nodes and existing terminals stay independent/immutable.
     */
    bool invalidate(uint8_t slot, uint64_t nowUs);
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
    /** Inspect the latest raw observation for a configured response identifier.
     * Storage is separate from terminal results and survives release/new admission.
     * @param[in] slot Fixed channel slot.
     * @return Null before any observation or for invalid/unconfigured slots; otherwise
     * a borrowed view valid until the next observation on this route or destruction.
     * Copy it for history. Reading performs no I/O, clock read or consumption.
     */
    const SdoRxObservation* observation(uint8_t slot) const;
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
        bool handoffLive = false, observationValid = false;
        uint32_t generation = 0, frame = 0;
        uint64_t queueCutoff = 0, responseCutoff = 0;
        uint64_t handoffUs = 0, handoffCutoff = 0;
        uint64_t admittedUs = 0;
        Generation nodeGeneration = 0;
        SdoTransfer work;
        SdoCompletion completion;
        uint8_t upload[32] = {}, uploaded = 0;
        Frame lastReply;
        bool replyValid = false;
        SdoRxObservation observation;
        // Before passive routing exists, retain service provenance for initial attachment.
        // It is consumed when a node anchor is installed, not a second current cache.
        ObservationTime unanchoredService;
        ObservationTime unanchoredSdo;
        bool unanchoredServiceFromSdo = false;
        bool networkConfigured = false;
        NodeSnapshot network;
        NetworkObserver observer;
    } channels_[CHANNELS];
    uint64_t lastUs_ = 0;
    ControllerState controllerState_ = ControllerState::UNKNOWN;
    NetworkObservation networkHistory_[NETWORK_HISTORY];
    uint8_t networkHead_ = 0, networkCount_ = 0;
    uint32_t networkDropped_ = 0;
    CO_NMT_t nmtEngine_ = {};
    CO_CANmodule_t nmtPort_ = {};
    NmtCompletion nmtCompletion_;
    bool nmtOccupied_ = false, nmtTerminal_ = false;
    uint32_t nmtGeneration_ = 0;
    uint64_t nmtQueueCutoff_ = 0;
    bool validNmt(NmtRequestId id) const;
    void serviceNmt(uint64_t nowUs);
    void expireNmt(uint64_t nowUs);
    void finishNmt(NmtOutcome outcome, uint64_t nowUs);
    void applyNmt(uint64_t nowUs);
    void observeNmt(NodeSnapshot& state, const ObservationTime& time, NmtState observed);
    bool eligibleSdo(uint8_t slot, const SdoTransfer& work, uint64_t nowUs) const;
    struct PdoSlot {
        bool bound = false, reused = false;
        uint8_t anchor = 0;
        PdoRecord record;
        PdoObserver observer;
    } pdos_[PDO_ROUTES];
    void revokePdo(unsigned slot, bool mapUncertain, uint64_t nowUs);
    void revokeNodePdos(unsigned anchor, bool mapUncertain, uint64_t nowUs);
    bool dispatchPdo(const Frame& frame, bool timeKnown, uint64_t receivedUs, uint64_t nowUs, bool& matched);
    bool pdoIdentifier(uint32_t id, bool extended) const;
    bool valid(SdoRequestId id) const;
    void expire(Channel& c, uint64_t nowUs);
    void pump(Channel& c, uint64_t nowUs);
    void finish(Channel& c, SdoOutcome outcome, uint64_t nowUs, int32_t returned = 0,
                bool abortValid = false, uint32_t abortCode = 0);
    void abort(Channel& c, SdoOutcome outcome, uint32_t code, uint64_t nowUs);
    void retire(Channel& c);
    bool receiveFrame(const Frame& frame, bool receivedTimeKnown, uint64_t receivedUs, uint64_t nowUs);
    int networkAnchor(uint8_t slot) const;
    void recordNetwork(const NetworkObservation& observation);
    bool dispatchNetwork(const Frame& frame, const NetworkRoute* source, bool timeKnown,
                         uint64_t receivedUs, uint64_t nowUs);
    void invalidateNode(unsigned anchor, NetworkInvalidation cause, uint64_t nowUs, bool resetObserved = false);
    void stopSdo(unsigned anchor, uint64_t nowUs);
    void recordSdo(Channel& channel, bool timeKnown, uint64_t receivedUs, uint64_t nowUs);
};
} // namespace CANOPEN
} // namespace MotorControlCAN
