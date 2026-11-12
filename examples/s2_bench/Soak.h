/** @file Soak.h
 * @brief Finite communication-only soak through the ordinary typed read path and sole owner.
 * SPDX-License-Identifier: MIT
 *
 * Application only: no SDK headers, heap, retries, recovery or writes. One cooperative caller
 * services the firmware first, then this object, on the owner task. It sends nothing itself:
 * reads go through ProbeApp and CanBusOwner, and the trace is a copy of the owner's diagnostics.
 *
 * The read set is on the owner's accepted list (DEC-1, "accepted, polling allowed";
 * docs/reference/lichuan_objects.json read_disposition accepted_not_qualified):
 * 6041:00, 6061:00, 6064:00, 606C:00 every state operation; 2100:00, 2101:00, 2102:00 when
 * monitors are selected; 1000:00, 1009:00, 100A:00, 1018:00..03, 2000:00, 2001:00 once per node
 * when identity is selected. Their effects stay unknown. 603F, 1001 and 1003 are never read:
 * DEC-1 keeps them explicit single reads. Nothing here enables, moves, writes, sends NMT,
 * clears a fault, saves, reconfigures a PDO or reopens a route.
 */
#pragma once
#include "BenchNodes.h"
#include "FirmwareProbe.h"

#if defined(__GNUC__)
#define SOAK_PRINTF __attribute__((format(printf, 2, 3)))
#else
#define SOAK_PRINTF
#endif

namespace MotorControlCANExample {
/** Fixed bench selection of read operations; not an object selector. */
struct SoakPlan {
    uint32_t trial = 0;      ///< Nonzero identity from the host, strictly increasing per boot.
    uint32_t durationS = 0;  ///< Stated finite duration, 1..172800 seconds.
    uint32_t intervalMs = 0; ///< Spacing of scheduled read operations, 20..60000 ms, across both nodes.
    uint32_t summaryS = 0;   ///< Spacing of summaries, 1..3600 seconds.
    bool monitors = false;   ///< Alternate 2100/2101/2102 operations with the state operations.
    bool identity = false;   ///< One typed identity read per node first, with MEASURED_U8 widths.
};
/** Parse the text after the command byte: `TRIAL,DURATION_S,INTERVAL_MS,SUMMARY_S,READS`.
 * READS is 0 state only, 1 plus monitors, 2 plus identity, 3 plus both.
 * @param text Borrowed characters; no terminator required.
 * @param size Character count.
 * @param output Published only on success.
 * @return False for anything else, including a value outside its stated range.
 */
bool parseSoakCommand(const char* text, size_t size, SoakPlan& output) noexcept;
/** One SDK submission attempt, copied by SoakTwaiTap. */
struct SoakSubmission {
    MotorControlCAN::Frame frame; ///< Frame offered to the SDK.
    uint32_t serial = 0;          ///< Adapter handoff serial.
    TwaiSubmit result = TwaiSubmit::DEFERRED; ///< SDK boundary result.
};
/** Caller storage for copies of received frames, borrowed by SoakTwaiTap while it copies. */
struct ReceiveRing {
    enum {
        CAPACITY = 32 ///< Copies held between drains; a full ring drops the newest copy.
    };
    MotorControlCANOwner::ReceivedFrame frames[CAPACITY]; ///< Copies, oldest at head.
    uint8_t head = 0;     ///< Index of the oldest copy.
    uint8_t count = 0;    ///< Copies not yet taken.
    uint32_t dropped = 0; ///< Saturating copies refused while full; the owner still had each frame.
};
/** Copies each submission result, and optionally each received frame, between the TWAI adapter
 * and its driver. The adapter has no public view of this boundary, and owner diagnostics stop at
 * its queue. A forwarding shim: it adds no retry, no delay and no second receive path.
 * Stationary, and it outlives the adapter that borrows its callbacks.
 *
 * It is also the last software point before the SDK, so it holds the bench's transmit gate
 * (SilentCapture): while closed, every submission is refused here as FAILED_UNSENT without
 * calling the inner driver. That result proves to the adapter and owner that the frame never
 * reached the SDK, so nothing is left uncertain by the refusal. The gate does not stop the
 * controller acknowledging frames or sending error flags: those are bits, not submissions.
 */
class SoakTwaiTap {
  public:
    /** @param inner Driver callbacks to forward; context outlives this object. */
    explicit SoakTwaiTap(TwaiDriver inner) noexcept;
    SoakTwaiTap(const SoakTwaiTap&) = delete;            ///< Callbacks borrow this address.
    SoakTwaiTap& operator=(const SoakTwaiTap&) = delete; ///< Callbacks borrow this address.
    /** @return Callbacks for TwaiAdapter, borrowing this object. */
    TwaiDriver driver() noexcept;
    /** Take the latest SENT or failed submission. DEFERRED is only counted.
     * @param output Published only on true.
     * @return True for one retained record not yet taken.
     */
    bool take(SoakSubmission& output) noexcept;
    /** @return Saturating count of attempts deferred with no SDK contact. */
    uint32_t deferred() const noexcept { return deferred_; }
    /** @return Saturating count of records replaced before they were taken. */
    uint32_t overwritten() const noexcept { return overwritten_; }
    /** Refuse every later submission at this boundary until openTransmit. No I/O. */
    void closeTransmit() noexcept { closed_ = true; }
    /** Forward submissions to the inner driver again. No I/O; sends nothing itself. */
    void openTransmit() noexcept { closed_ = false; }
    /** @return True while submissions are refused here. */
    bool transmitClosed() const noexcept { return closed_; }
    /** @return Saturating count of submissions refused by the closed gate; none reached the SDK. */
    uint32_t refused() const noexcept { return refused_; }
    /** Driver-boundary retirement evidence, independent of the owner's own.
     * @return False while a frame the inner driver accepted (SENT) has had no final report with
     * its serial, or after a FAILED_UNCERTAIN, until a confirmed inner stop deleted the node. True
     * means no frame this tap handed over can still be in the SDK or controller, so the
     * controller has nothing of ours to retransmit. Nothing here observes the bus.
     */
    bool sdkRetired() const noexcept { return !outstanding_ && !uncertain_; }
    /** Start or stop copying received frames. Off by default.
     * A copy is made only when the inner receive returned a frame, after it returned, and the
     * owner still gets that frame unchanged. Copying never consumes, delays or repeats a receive.
     * @param ring Borrowed until the next call; emptied and its dropped count zeroed here.
     * nullptr stops copying and leaves the previous ring as it was.
     */
    void copyReceives(ReceiveRing* ring) noexcept;
    /** Take the oldest copy.
     * @param output Published only on true.
     * @return False when no copy is waiting or copying is off.
     */
    bool takeReceived(MotorControlCANOwner::ReceivedFrame& output) noexcept;

  private:
    TwaiDriver inner_;
    SoakSubmission latest_;
    ReceiveRing* ring_ = nullptr;
    uint32_t deferred_ = 0, overwritten_ = 0, refused_ = 0, outstandingSerial_ = 0;
    bool present_ = false, closed_ = false, outstanding_ = false, uncertain_ = false;
};
/** Local resource sample; the SDK calls behind it live in the firmware entry. */
struct SoakResources {
    uint32_t stackFree = 0;   ///< Least bytes ever free on the owner task's stack.
    uint32_t heapFree = 0;    ///< Internal heap bytes free now.
    uint32_t heapMinimum = 0; ///< Least internal heap bytes ever free.
    uint32_t txErrors = 0;    ///< Controller TX error counter when sampled.
    uint32_t rxErrors = 0;    ///< Controller RX error counter when sampled.
};
/** Optional sampling callback, called at each summary and never per service turn. */
struct SoakCallbacks {
    void* context = nullptr;                       ///< Borrowed for this object's lifetime.
    void (*sample)(void*, SoakResources&) = nullptr; ///< Fills a zeroed sample; bounded, no I/O.
};
/** Local start decision; none of these is a device result. */
enum class SoakAdmission : uint8_t {
    ACCEPTED, ///< Reads begin at the first scheduled tick; the first output line is SOAK_BEGIN.
    BUSY,     ///< A soak, a typed read or an owner reservation is still present.
    INVALID   ///< Invalid plan, a repeated trial, no running controller or no configured route.
};
/** Why a soak ended, as printed in SOAK_END. */
enum class SoakEnd : uint8_t {
    NONE,     ///< Still running or never started.
    DURATION, ///< The stated duration elapsed.
    STOPPED,  ///< Explicit stop command.
    INTERNAL, ///< Refused typed advancement or firmware progression failure; evidence retained.
};
/** Fixed sizes of the soak's retained evidence. */
enum {
    SOAK_NODES = BENCH_NODES, ///< Nodes 1 and 2 in adapter slots 0 and 1.
    SOAK_TRACE = 128,        ///< Copied trace entries; the oldest is overwritten.
    SOAK_DUMP_ENTRIES = 96,  ///< Most entries in one anomaly dump.
    SOAK_DUMPS_PER_KIND = 2, ///< Full dumps kept for each anomaly kind.
    SOAK_DUMPS = 16,         ///< Full dumps kept in one soak; later anomalies are counted.
    SOAK_EVENTS = 96,        ///< Records waiting for the console.
    SOAK_ABORTS = 4,         ///< Distinct abort codes counted per node.
    SOAK_BUCKETS = 8,        ///< Latency histogram buckets.
    SOAK_OUTCOMES = 12,      ///< SdoOutcome values counted per node.
    SOAK_REASONS = 8,        ///< CompletionReason values counted per node.
    SOAK_READ_OUTCOMES = 9   ///< ReadOutcome values counted per node.
};
/** Per-node totals since the soak began; each saturates at UINT32_MAX. */
struct SoakNodeStats {
    uint32_t operations;     ///< Read operations prepared.
    uint32_t operationsOk;   ///< Operations that published checked typed values.
    uint32_t operationsFailed; ///< Operations that ended without publication.
    uint32_t steps;          ///< Owner terminals consumed.
    uint32_t stepsOk;        ///< Terminals with SdoOutcome::SUCCESS.
    uint32_t refused;        ///< Operations the application refused to prepare.
    uint32_t latencyCount;   ///< Terminals with a possible transmission, which the latency covers.
    uint32_t latencyMinUs;   ///< Least owner admission to terminal time.
    uint32_t latencyMaxUs;   ///< Greatest owner admission to terminal time.
    uint64_t latencySumUs;   ///< Sum for the mean.
    uint32_t retireMaxUs;    ///< Greatest time from operation end to local TX retirement.
    uint32_t earlyReplies;   ///< Successes whose reply was processed before their final TX report.
    uint32_t heartbeats;     ///< Accepted periodic heartbeats observed.
    uint32_t heartbeatGapMaxUs; ///< Greatest processing gap between them.
    uint32_t buckets[SOAK_BUCKETS];          ///< Latency histogram; see SOAK_HIST in the guide.
    uint32_t outcomes[SOAK_OUTCOMES];        ///< Terminals by SdoOutcome.
    uint32_t reasons[SOAK_REASONS];          ///< Terminals by CompletionReason.
    uint32_t readOutcomes[SOAK_READ_OUTCOMES]; ///< Operations by ReadOutcome.
    uint32_t abortCode[SOAK_ABORTS];         ///< Distinct abort codes, in order of first appearance.
    uint32_t abortCount[SOAK_ABORTS];        ///< Count of each.
    uint32_t abortOther;     ///< Aborts with a code beyond the table.
    uint8_t abortCodes;      ///< Used table entries.
    bool stopped;            ///< Reads ended for this node; nothing reopens the route.
};
/** Soak-wide console and scheduling pressure since the soak began. */
struct SoakPressure {
    uint32_t lines;          ///< Lines handed to the console queue.
    uint32_t blockedTurns;   ///< Turns with a line ready and no room for it.
    uint32_t eventsFull;     ///< Records dropped because the queue was full.
    uint32_t eventsLimited;  ///< Value records dropped by the rate limit.
    uint32_t anomalies;      ///< All anomalies, dumped or not.
    uint32_t dumps;          ///< Full dumps begun.
    uint32_t dumpsSkipped;   ///< Anomalies past the dump budget or inside a running dump.
    uint32_t dumpsAbandoned; ///< Dumps ended by the output deadline with entries unprinted.
    uint32_t traceLost;      ///< Dump entries overwritten before printing.
    uint32_t deferDump;      ///< Turns a due operation waited for a dump.
    uint32_t deferBusy;      ///< Turns a due operation waited for the previous one.
    uint32_t deferTx;        ///< Ticks skipped for unretired local TX.
    uint32_t deferInhibited; ///< Ticks skipped for an inhibited controller or a stopped NMT state.
    uint32_t slips;          ///< Schedule ticks that had already passed when the next was set.
    uint64_t dumpUs;         ///< Total time reads were held for dumps.
    uint64_t gapMaxUs;       ///< Greatest interval between service calls.
    uint64_t gapWindowUs;    ///< Same since the last summary.
    uint16_t consoleHighWater; ///< Most bytes seen in the console queue.
    uint8_t eventHighWater;  ///< Most records waiting.
    uint8_t queuedHighWater; ///< Most owner requests queued.
    uint8_t activeHighWater; ///< Most native requests in flight.
    uint8_t retainedHighWater; ///< Most unreleased owner terminals.
    uint8_t traceHighWater;  ///< Most owner diagnostics waiting at the start of a drain.
};
/** One finite soak at a time. See the file comment for its boundaries.
 * Output is bounded and never blocks service: one line per turn at most, only when the console
 * queue has room. Under pressure records are dropped and counted; a dump delays the next
 * scheduled read, up to a deadline, and never cancels one that has started.
 * The object is large and stationary, and allocates nothing. The caller places it.
 */
class Soak {
  public:
    /** Borrow stationary resources; construction does nothing else.
     * @param firmware Assembly whose application and owner carry the reads.
     * @param services That assembly's service store, with routes the caller configured.
     * @param tap Tap the assembly's adapter submits through.
     * @param callbacks Optional resource sampling.
     * @param revision Static source revision text printed in SOAK_BEGIN.
     */
    Soak(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services, SoakTwaiTap& tap,
         const SoakCallbacks& callbacks, const char* revision) noexcept;
    Soak(const Soak&) = delete;            ///< Holds live evidence and borrowed references.
    Soak& operator=(const Soak&) = delete; ///< Holds live evidence and borrowed references.
    /** Start one explicit finite soak. Sends nothing during this call.
     * @param plan Copied plan.
     * @param nowUs Monotonic microseconds, at least the owner's last time.
     * @return ACCEPTED only when the soak will run. Earlier totals are then gone.
     */
    SoakAdmission begin(const SoakPlan& plan, uint64_t nowUs) noexcept;
    /** One turn: copy diagnostics, track status, consume a terminal, schedule, print one line.
     * Call after the firmware service turn, also once the firmware has stopped running.
     * @param nowUs Monotonic microseconds; an earlier value is ignored for this turn.
     * @param holdOutput True prints no line this turn, so that a console response larger
     * than the room this output leaves can get through. Everything else still runs.
     */
    void service(uint64_t nowUs, bool holdOutput = false) noexcept;
    /** Stop after any started operation settles, then print the final summary and SOAK_END.
     * @return False when no soak is running.
     */
    bool requestStop() noexcept;
    /** @return True from an accepted begin until SOAK_END has been printed. */
    bool active() const noexcept { return s_.phase != PHASE_IDLE; }
    /** @return Why the last soak ended, or NONE. */
    SoakEnd ended() const noexcept { return s_.end; }
    /** @param node Node 1..SOAK_NODES.
     * @return Totals of the current or last soak. Other values read node 1's totals.
     */
    const SoakNodeStats& nodeStats(unsigned node) const noexcept {
        return s_.nodes[node >= 1 && node <= SOAK_NODES ? node - 1 : 0];
    }
    /** @return Pressure totals of the current or last soak. */
    const SoakPressure& pressure() const noexcept { return s_.pressure; }

  private:
    enum Phase : uint8_t { PHASE_IDLE, PHASE_RUNNING, PHASE_FINISHING, PHASE_FINAL };
    enum DumpPhase : uint8_t { DUMP_NONE, DUMP_COLLECT, DUMP_EMIT };
    // LINE_BYTES holds the longest line, SOAK_CON at 440 bytes, with every field at its type's
    // maximum. At 240, SOAK_CON overflowed at the end of a 3 h soak after a bus fault (2026-11-03).
    enum { SIGNAL_SLOTS = 46, LINE_BYTES = 448 };
    /** One compact copy of an owner diagnostic, a terminal or a submission. */
    struct TraceEntry {
        uint64_t processedUs;
        uint32_t id;
        uint8_t data[8];
        uint16_t lowOffset, highOffset;
        uint16_t txRequest;
        uint8_t txFrame, epoch, kind, dlc, flags, state;
    };
    /** One record waiting for the console; the type selects the meaning of a..d. */
    struct Event {
        uint64_t timeUs;
        uint32_t a, b, c, d;
        uint8_t type, node, signal, flag;
    };
    /** All per-trial state. Zeroing it is the reset, and a zeroed plan or sample is valid. */
    struct State {
        Phase phase;
        DumpPhase dump;
        SoakEnd end;
        SoakPlan plan;
        uint64_t startUs, endUs, nextOpUs, nextSummaryUs, lastServiceUs, finishUs;
        uint64_t dumpStartUs, dumpPostUs, tokenUs;
        uint32_t tokens, line, operation, anomalyId, dumpId, dumpFirst, dumpLast, traceNext;
        uint32_t lastRejectedId;
        uint32_t staleObservations; // Network observations copied before this soak began.
        uint32_t last[SOAK_NODES + 1][SIGNAL_SLOTS];
        uint64_t known[SOAK_NODES + 1];
        uint64_t raised[SOAK_NODES + 1]; // Limited signals with an anomaly in this summary window.
        uint64_t heartbeatUs[SOAK_NODES], retireSinceUs[SOAK_NODES];
        SoakNodeStats nodes[SOAK_NODES];
        SoakPressure pressure;
        SoakResources resources;
        TraceEntry trace[SOAK_TRACE];
        Event events[SOAK_EVENTS];
        uint8_t eventHead, eventCount, kindDumps[16], turn, opSlot, summaryCursor;
        uint8_t monitorPhase[SOAK_NODES];
        bool identityDone[SOAK_NODES], heartbeatSeen[SOAK_NODES], retireWait[SOAK_NODES];
        bool opActive, releasePending, stopRequested, beginPending, endPending, summaryPending;
        bool lineReady;
        uint16_t lineLength;
    };
    FirmwareProbe& firmware_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    SoakTwaiTap& tap_;
    SoakCallbacks callbacks_;
    const char* revision_;
    uint32_t lastTrial_ = 0;
    // Scratch outside State: too large for the task stack and not trivial to zero.
    MotorControlCANOwner::Diagnostic diagnostic_;
    MotorControlCAN::CANOPEN::NetworkObservation observation_;
    MotorControlCAN::LICHUAN_C::StateObservation stateValues_;
    MotorControlCAN::LICHUAN_C::IdentityObservation identityValues_;
    char text_[LINE_BYTES];
    State s_;

    void copyDiagnostics(uint64_t nowUs) noexcept;
    void copySubmission(uint64_t nowUs) noexcept;
    void copyObservations(uint64_t nowUs) noexcept;
    void trackNodes(uint64_t nowUs) noexcept;
    void trackInterface(uint64_t nowUs) noexcept;
    void track(unsigned scope, unsigned signal, uint32_t value, uint64_t nowUs) noexcept;
    void value(unsigned slot, unsigned signal, uint32_t raw, uint64_t nowUs) noexcept;
    void advance(uint64_t nowUs) noexcept;
    void finishOperation(uint64_t nowUs) noexcept;
    void publish(unsigned slot, uint64_t nowUs) noexcept;
    void recordStep(const MotorControlCANOwner::Completion& done, uint64_t nowUs) noexcept;
    void schedule(uint64_t nowUs) noexcept;
    void startOperation(unsigned slot, uint64_t nowUs) noexcept;
    bool stopIfUnusable(unsigned slot, uint64_t nowUs) noexcept;
    void anomaly(unsigned kind, unsigned scope, uint32_t a, uint32_t b, uint64_t nowUs) noexcept;
    void push(uint8_t type, unsigned node, unsigned signal, bool flag, uint32_t a, uint32_t b,
              uint32_t c, uint32_t d, uint64_t nowUs, bool limited) noexcept;
    TraceEntry& addTrace(uint64_t processedUs) noexcept;
    void abandonDump(uint64_t nowUs) noexcept;
    void finishPhases(uint64_t nowUs) noexcept;
    void pump(uint64_t nowUs) noexcept;
    bool formatNext(uint64_t nowUs) noexcept;
    bool formatEvent(const Event& event) noexcept;
    bool formatDump(uint64_t nowUs) noexcept;
    bool formatSummary(uint64_t nowUs) noexcept;
    bool put(const char* pattern, ...) noexcept SOAK_PRINTF;
};
} // namespace MotorControlCANExample
