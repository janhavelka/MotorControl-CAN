/** @file LinkWatch.h
 * @brief Prompt 55 bench facilities: the link status lines and the bounded unplug watch.
 * SPDX-License-Identifier: MIT
 *
 * Application only: no SDK headers, heap, clock or retry. One cooperative caller services the firmware
 * first, then this object, on the owner task.
 *
 * formatLink prints the owner's cached prompt 55 facts, `L`: one interface line (TX lifetime,
 * NO_LISTENER, expired handoffs, markers, refusals, recoveries) and one line per fixture node (its
 * route's reuse policy and state and the owner's presence observations). It sends nothing.
 *
 * LinkWatch is the unplug check (the single byte `U`, a fixed 120 s window): for that window it watches the owner, and when a
 * TX lifetime expires (Stats::txExpired rises or NO_LISTENER begins) it calls the one restart entry
 * point, CanBusOwner::recover, itself, exactly once in the window, and releases its result. It never
 * sends a frame, never repeats the restart, and decides nothing about the drives: the routes resync
 * under their own reuse policy after the retired recovery, and the end line reports what was observed.
 * After its one restart the watch probes each node whose route is still unresolved with an explicit
 * marker read (CanBusOwner::admitDrainMarker, policy form), the application's own repair decision. It
 * does not wait for NO_LISTENER to end: the owner starts no automatic resync step while NO_LISTENER
 * holds, and on this bench nothing else would end it (no heartbeat, no PDO), while a drain marker is
 * admitted and its ACK ends it. A probe that ends without reopening is followed by another only after
 * LINK_WATCH_PROBE_INTERVAL_US and only while its node's earlier probe frame is locally retired, at most
 * LINK_WATCH_PROBES per node and window; each probe and its result is an event line.
 * "No stale frame" can only be reported as no unmatched or reuse-refusing frame after the restart; a
 * frame the controller kept and sent late would show as such a frame, but nothing here proves its
 * absence on the wire.
 */
#pragma once
#include "BenchNodes.h"
#include "FirmwareProbe.h"

namespace MotorControlCANExample {
enum {
    LINK_WATCH_EVENTS = 16,           ///< Queued event lines; a full queue counts the rest as dropped.
    LINK_WATCH_MAXIMUM_SECONDS = 600, ///< Longest watch window, host policy.
    /** Budget of the watch's one recovery, in microseconds: host policy, not an SDK or drive timing. */
    LINK_WATCH_RECOVERY_US = 1000000,
    LINK_WATCH_PROBES = 4, ///< Most explicit marker probes per node in one window, host policy.
    /** Least spacing of a node's probes, in microseconds, host policy: above a probe's 500 ms budget. */
    LINK_WATCH_PROBE_INTERVAL_US = 1000000
};
/** Format link line `index` (0 the interface, then one per fixture node, 1..BENCH_NODES).
 * @param firmware The assembly whose owner is read; nothing is sent or changed.
 * @param services That assembly's service store.
 * @param index Line number.
 * @param out Output buffer, NUL-terminated on success.
 * @param size Its size; LINE bytes suffice.
 * @return False for an index past the last line or a buffer too small.
 */
bool formatLink(const FirmwareProbe& firmware, const MotorControlCAN::CANOPEN::SdoClient& services,
                unsigned index, char* out, size_t size) noexcept;
/** Admit one explicit drain marker read on a fixture node's unresolved route through the policy form of
 * CanBusOwner::admitDrainMarker: the first object of the binding's SdoReuse marker list (default 1000:00,
 * 1018:01, 1018:02, 1018:03, accepted DEC-1 U32 reads) that the owner admits, which skips retained and
 * last completed pairs, with the bench routes' 250 ms queue and response budgets. The application's
 * explicit decision, as after a best-effort stop or a client abort, where no owner resync step runs.
 * Sends at most one marker; nothing else.
 * @param firmware Assembly whose owner admits it. @param services Its service store.
 * @param slot Adapter slot, node - 1. @param nowUs Owner-monotonic microseconds.
 * @param identity Host-only operation identity for the marker, nonzero and unique.
 * @param output Request identity, written on ACCEPTED.
 * @param object Index << 8 | subindex of the admitted object, written on ACCEPTED.
 * @return The owner's admission of the last object tried; INVALID when none is admissible.
 */
MotorControlCANOwner::BusAdmission admitExplicitMarker(FirmwareProbe& firmware,
                                                       const MotorControlCAN::CANOPEN::SdoClient& services,
                                                       unsigned slot, uint64_t nowUs, uint32_t identity,
                                                       MotorControlCANOwner::RequestId& output,
                                                       uint32_t& object) noexcept;
/** The bench's `y` and `z`: one explicit marker for node 1 or 2 at a time, its admission line and, after
 * its terminal (released at once), its result line with whether the route reopened. Stationary. */
class MarkerCommand {
  public:
    /** @param firmware Assembly. @param services Its service store. */
    MarkerCommand(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services) noexcept;
    MarkerCommand(const MarkerCommand&) = delete;            ///< Borrows stationary references.
    MarkerCommand& operator=(const MarkerCommand&) = delete; ///< Borrows stationary references.
    /** Admit one marker for node; prints MARKER admitted or MARKER refused into out.
     * @param node 1..BENCH_NODES. @param nowUs Owner time. @param out Line buffer. @param size Its size.
     * @return True when the marker was admitted. */
    bool begin(unsigned node, uint64_t nowUs, char* out, size_t size) noexcept;
    /** After the firmware turn: once its terminal is in, format the result line, release it and end.
     * @param nowUs Owner time. @param out Line buffer. @param size Its size.
     * @return True when out holds the result line. */
    bool service(uint64_t nowUs, char* out, size_t size) noexcept;
    /** @return True while a marker this command admitted has not printed its result. */
    bool active() const noexcept { return active_; }

  private:
    FirmwareProbe& firmware_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    MotorControlCANOwner::RequestId request_;
    uint32_t identity_ = 0, object_ = 0;
    uint8_t slot_ = 0;
    bool active_ = false;
};
/** What the watch observed, in order. */
enum class LinkEventKind : uint8_t {
    EXPIRED,         ///< A TX lifetime expired, or NO_LISTENER began; value: Stats::txExpired.
    RESTART,         ///< The watch called recover(); value: the recovery identity's generation.
    RESTART_REFUSED, ///< recover() refused (single flight or state); not called again in this window.
    RECOVERY,        ///< The recovery closed; value: RecoveryOutcome, node: its causes as bits 1, 2, 4.
    LISTENER_END,    ///< NO_LISTENER ended: a frame was received or ACKed.
    RESYNCED,        ///< A fixture node's route is open again after the restart; value: Stats::markers.
    MARKER,          ///< The watch admitted an explicit marker probe for node; value: index << 8 | subindex. Values print in hexadecimal.
    MARKER_RESULT    ///< That probe's terminal; value: SdoOutcome << 8 | transmission possible.
};
/** One bounded unplug watch at a time; see the file comment. Stationary; allocates nothing. */
class LinkWatch {
  public:
    /** @param firmware Assembly whose owner this watches and restarts. @param services Its service store. */
    LinkWatch(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services) noexcept;
    LinkWatch(const LinkWatch&) = delete;            ///< Borrows stationary references.
    LinkWatch& operator=(const LinkWatch&) = delete; ///< Borrows stationary references.
    /** Arm one window. Sends nothing.
     * @param seconds Window length, 1..LINK_WATCH_MAXIMUM_SECONDS.
     * @param nowUs Owner-monotonic microseconds.
     * @return Null when armed; "active", "invalid" or "state" (no running firmware) otherwise.
     */
    const char* begin(uint32_t seconds, uint64_t nowUs) noexcept;
    /** One turn after the firmware's: detect expiry, restart once, follow the recovery and the routes. */
    void service(uint64_t nowUs) noexcept;
    /** @return True from begin until the end line has been taken. */
    bool active() const noexcept { return armed_ || endPending_ || count_; }
    /** Take the next line: the armed line, events, then the end line once the window closed.
     * @param out Buffer, NUL-terminated on success. @param size Its size.
     * @return False when no line is ready or the buffer is too small.
     */
    bool takeLine(char* out, size_t size) noexcept;
    /** @return Restarts this watch called in its current or last window: 0 or 1. */
    unsigned restarts() const noexcept { return restarts_; }

  private:
    struct Event {
        uint64_t timeUs;
        uint32_t value;
        uint8_t kind, node;
    };
    FirmwareProbe& firmware_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    MotorControlCANOwner::RecoveryId recovery_;
    Event events_[LINK_WATCH_EVENTS];
    uint64_t startUs_ = 0, endUs_ = 0, listenerUs_ = 0;
    uint32_t expired0_ = 0, unmatched0_ = 0, refused0_ = 0, markers0_ = 0, unmatchedRestart_ = 0;
    uint32_t refusedRestart_ = 0, dropped_ = 0;
    uint8_t head_ = 0, count_ = 0, restarts_ = 0, outcome_ = 0xff, causes_ = 0;
    bool armed_ = false, armedLine_ = false, endPending_ = false, restartTried_ = false;
    bool recoveryPending_ = false, recovered_ = false, noListener_ = false, listenerEnded_ = false;
    bool resynced_[BENCH_NODES] = {};
    // The watch's one explicit marker per node, after NO_LISTENER ended (see the file comment).
    MotorControlCANOwner::RequestId marker_[BENCH_NODES];
    bool markerActive_[BENCH_NODES] = {};
    uint8_t probes_[BENCH_NODES] = {};
    uint64_t nextProbeUs_[BENCH_NODES] = {};
    uint32_t markerIdentity_ = 0, explicit_ = 0, explicitOk_ = 0;
    void push(LinkEventKind kind, unsigned node, uint32_t value, uint64_t nowUs) noexcept;
    void repair(unsigned slot, uint64_t nowUs) noexcept;
};
} // namespace MotorControlCANExample
