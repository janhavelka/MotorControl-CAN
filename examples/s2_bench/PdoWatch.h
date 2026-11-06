/** @file PdoWatch.h
 * @brief Finite passive watch of received PDO, heartbeat and other frames, copied at the tap.
 * SPDX-License-Identifier: MIT
 *
 * Application only: no SDK headers, heap, clock, I/O of its own, retries or writes. The watch
 * sends nothing and changes no owner, route, NMT or drive state. Its frames are SoakTwaiTap
 * copies of what the driver handed the owner; the owner still dispatches each one unchanged.
 * With no PDO route configured the owner counts these frames unmatched. A watched frame is a
 * received copy, never a qualified PDO sample: no mapping, route, generation or freshness check
 * applies to it.
 *
 * Times are microseconds of the S2's monotonic clock (esp_timer): nowUs as supplied to service
 * and the stamps IdfTwaiDriver takes. That driver gives bounds, not an exact time: hi is the
 * ISR's clock sample when it enqueued the frame and lo the last check that proved the controller
 * FIFO empty. Neither is a bus timestamp. A frame's time is hi, or the exact receivedUs when a
 * driver supplies one, so gaps include interrupt latency and controller FIFO wait, which owner
 * service work in critical sections and other interrupts can lengthen.
 *
 * Classification: standard data frames 0x581..0x5ff (SDO server replies) and 0x601..0x67f (SDO
 * client requests) are only counted in sdo. Every other frame is watched, whatever its meaning:
 * PDO 0x180..0x57f, heartbeat or boot-up 0x701..0x77f, EMCY 0x081..0x0ff, NMT, SYNC, any other
 * identifier, every extended frame and every RTR frame.
 *
 * Output lines, one per service call at most. Each starts with its tag, then n, a contiguous
 * sequence number from 0 at PDOW_BEGIN, and t, the nowUs of the call that wrote the line, so the
 * host can detect console loss as with SOAK lines. Identifiers and data are lower-case hex without
 * 0x; data is two digits per payload byte with no spaces (2*dlc for a data frame), or - when there
 * is no payload byte (dlc 0 or RTR). Booleans are 0 or 1.
 *
 *     PDOW_BEGIN n t seconds ring ids
 *     PDOW_F n t id ext rtr dlc data timed exact lo hi gap
 *     PDOW_SUM n t frames printed unprinted tapDropped untimed sdo idOverflow ids
 *     PDOW_ID n t id ext count first last minGap maxGap meanGap sdGap changes dlc data
 *     PDOW_END n t reason frames printed unprinted tapDropped untimed sdo idOverflow
 *
 * Each field is printed as name=value. BEGIN: seconds as admitted, ring the tap ring capacity,
 * ids the per-identifier capacity. F, one per watched frame while the print queue has room: lo
 * and hi as above, both receivedUs when exact, both 0 when untimed; gap is the time since the
 * previous timed frame of the same identifier, 0 for its first frame, an untimed frame, a time
 * earlier than that previous one or an identifier without statistics. SUM, every second of watch
 * time: totals since PDOW_BEGIN. A SUM still waiting at its next tick is not repeated, and an ID
 * round still printing is not restarted. ID, one per identifier every 10 s and once more at the
 * end: count of frames, first and last frame time, gap minimum, maximum, mean (sum of gaps over
 * their number, which is (last-first)/(timed frames-1) for in-order times) and population standard
 * deviation, the number of data changes (dlc, RTR or payload differs from the identifier's
 * previous frame) and the last frame's dlc and data. Gap fields are 0 before an identifier has two
 * timed frames. sdGap is 4294967295 when the gaps sum to more than 2^32-1 us, which no 600 s watch
 * of in-order times reaches. END: reason duration or stop and the final totals.
 *
 * frames counts watched frames, printed their F lines written, unprinted those with no F line
 * (print queue full, or still queued when the watch ended), tapDropped copies the tap ring had no
 * room for, untimed watched frames with neither an exact time nor bounds (in no gap statistics),
 * sdo the SDO frames, idOverflow watched frames of identifiers beyond PDO_WATCH_IDS (they have F
 * lines but no statistics) and ids the identifiers with statistics. Counters saturate at
 * UINT32_MAX. tapDropped above zero means frames the watch never saw, so every statistic is
 * incomplete; unprinted above zero means F lines are missing while the statistics stay complete.
 */
#pragma once
#include "Soak.h"

namespace MotorControlCANExample {
/** Console sink for PdoWatch lines. */
struct PdoWatchOutput {
    void* context = nullptr; ///< Borrowed for the watch's lifetime.
    bool (*write)(void*, const char*, size_t) = nullptr;
    ///< Queue one whole line of the given byte count, newline included, no terminator. Called on
    ///< the owner task from service. False means no room and nothing queued: the watch offers the
    ///< same line, with the same n, at its next unheld call. Must not call the watch.
};
/** Local start decision; none of these is a device result. */
enum class PdoWatchAdmission : uint8_t {
    ACCEPTED, ///< The watch runs; its first output line is PDOW_BEGIN.
    BUSY,     ///< A watch is active, including one whose PDOW_END is still waiting for room.
    INVALID   ///< Seconds outside 1..PDO_WATCH_SECONDS, no write callback or a clock near its end.
};
/** Fixed sizes of one watch. */
enum {
    PDO_WATCH_IDS = 16,        ///< Distinct (identifier, extended) pairs with statistics.
    PDO_WATCH_PRINTS = 32,     ///< Frames waiting for their PDOW_F line.
    PDO_WATCH_SECONDS = 600,   ///< Longest watch.
    PDO_WATCH_LINE_BYTES = 320 ///< Line buffer. The longest line, PDOW_ID with every field at its
                               ///< type's maximum, is 283 bytes; a longer one is never cut.
};
/** One finite watch at a time. See the file comment for its boundaries and lines.
 * Output is bounded and never blocks service: one line per call at most, and END, SUM and ID
 * lines before queued F lines, so a busy stream cannot starve the summaries. Under pressure F
 * lines are dropped and counted, never the statistics. The object is stationary and allocates
 * nothing; the caller places it.
 */
class PdoWatch {
  public:
    /** Borrow stationary resources; construction does nothing else.
     * @param tap Tap the adapter receives through; outlives this object.
     * @param output Copied console sink.
     */
    PdoWatch(SoakTwaiTap& tap, const PdoWatchOutput& output) noexcept;
    /** Stops the tap copying into this object if a watch is still collecting. */
    ~PdoWatch();
    PdoWatch(const PdoWatch&) = delete;            ///< The tap borrows this object's ring.
    PdoWatch& operator=(const PdoWatch&) = delete; ///< The tap borrows this object's ring.
    /** Start one finite watch. Sends nothing, now or later.
     * Clears every total of the previous watch, makes the tap copy received frames into this
     * object and queues PDOW_BEGIN.
     * @param seconds Watch time, 1..PDO_WATCH_SECONDS.
     * @param nowUs Monotonic microseconds; the watch ends at the first call at nowUs + seconds.
     * @return ACCEPTED only when the watch runs.
     */
    PdoWatchAdmission begin(uint32_t seconds, uint64_t nowUs) noexcept;
    /** One call: take every copy waiting at the tap, update the statistics, end the watch when
     * due, and write at most one line. Call on the owner task after each owner service turn.
     * @param nowUs Monotonic microseconds; an earlier value than the last call's is ignored, and
     * that call does nothing.
     * @param holdOutput True writes no line this call, so that a pending console response can get
     * through. Everything else still runs and nothing is lost; the next unheld call writes the
     * line that was next.
     */
    void service(uint64_t nowUs, bool holdOutput = false) noexcept;
    /** End early: the next service call stops copying, then the final ID lines and PDOW_END with
     * reason=stop follow.
     * @return False when no watch is collecting.
     */
    bool requestStop() noexcept;
    /** @return True from an accepted begin until PDOW_END has been written. */
    bool active() const noexcept { return s_.phase != PHASE_IDLE; }

  private:
    enum Phase : uint8_t { PHASE_IDLE, PHASE_RUNNING, PHASE_FINAL };
    enum Line : uint8_t { LINE_BEGIN, LINE_SUM, LINE_ID, LINE_END, LINE_FRAME };
    /** One frame waiting for its PDOW_F line. */
    struct Print {
        uint64_t lowUs, highUs, gapUs;
        uint32_t id;
        uint8_t data[8];
        uint8_t dlc, length, flags;
    };
    /** Statistics of one (identifier, extended) pair. */
    struct IdStats {
        uint64_t firstUs, lastUs, minGapUs, maxGapUs, gapSumUs, gapSquares;
        uint32_t id, count, gaps, changes;
        uint8_t data[8];
        uint8_t dlc, length;
        bool extended, rtr, timed;
    };
    /** All per-watch state. Zeroing it is the reset. */
    struct State {
        Phase phase;
        Line line; // Kind of the selected line.
        bool selected, beginPending, sumPending, idRound, endPending, stopRequested;
        uint8_t idCursor, idIndex, ids, printHead, printCount;
        uint32_t seconds, sequence, frames, printed, unprinted, untimed, sdo, idOverflow;
        uint64_t startUs, endUs, lastUs, nextSumUs, nextIdUs;
        Print frame; // The selected F line's frame, out of the queue.
        Print prints[PDO_WATCH_PRINTS];
        IdStats idStats[PDO_WATCH_IDS];
    };
    SoakTwaiTap& tap_;
    PdoWatchOutput output_;
    ReceiveRing ring_;
    MotorControlCANOwner::ReceivedFrame received_;
    char text_[PDO_WATCH_LINE_BYTES];
    size_t length_ = 0;
    State s_;

    void record(const MotorControlCANOwner::ReceivedFrame& in) noexcept;
    IdStats* find(uint32_t id, bool extended) noexcept;
    void schedule(uint64_t nowUs) noexcept;
    void finish() noexcept;
    bool select() noexcept;
    void format(uint64_t nowUs) noexcept;
    void emit(uint64_t nowUs) noexcept;
    void put(const char* pattern, ...) noexcept SOAK_PRINTF;
};
} // namespace MotorControlCANExample
