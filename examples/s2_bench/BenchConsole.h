/** @file BenchConsole.h
 * @brief The shared JSONL console on the S2 bench image, beside the bench's own line protocol.
 * SPDX-License-Identifier: MIT
 *
 * Application only: no SDK headers, heap, clock, retries or writes of its own. One ProbeConsole
 * with one ProbeDevice runs over the firmware's own application, owner and service store, exactly
 * as EspProbeMain wires them; nothing here sends a frame. Every console request still goes through
 * the console's own typed routes, read-effect gate and retained results.
 *
 * Input. main.cpp keeps the bench's single-byte commands and its collected lines unchanged. A
 * line whose first byte at idle is `@` is collected here whole, up to its CR or LF, and only then
 * classified and either fed to the console or refused here (benchConsoleGate). A line longer than
 * LINE_CAPACITY - 1 bytes, or one still incomplete after COLLECT_TIMEOUT_US, is dropped unseen by
 * the console: a partial command is never dispatched.
 *
 * Output. The console writes complete records into its own ProbePlatform; this object collects
 * each record's bytes and hands the whole record, newline included, to the firmware's UART output
 * queue in one ProbePlatform::writeOutput call. Every bench line is queued the same way, so a
 * console record and a bench line never interleave: each line appears whole, in queue order. A
 * record waits while BenchActivity::holdOutput (a bench record dump, network listing, one-shot
 * response or NMT result waiting for room), so bench multi-line records stay contiguous apart from
 * the one-shot responses the bench already prints between their lines. The console counts a
 * record delivered once its newline has been queued for the UART.
 *
 * Exclusion. While BenchActivity::wire, every line except the inspection set is refused here with
 * a console-format reply and reaches nothing; `discover` is also refused during a bench network
 * listing, which drains the same network observations; `recover` is always refused, because this
 * image has no host recovery path. While holds() the bench refuses every command that would admit
 * owner work (main.cpp). A console line other than `host`/`useaddr` is also refused when the
 * application's selection is no longer the one the console's last line left, as after a bench
 * identity read selected another node: the console then never reads or acts on a node it did not
 * select.
 *
 * Motion. The console's motion routes act on one BenchAxis per node: a ProbeMotion over the bench
 * route's own AxisSession of that node, with its live route, AxisConfig and AxisReference. Nothing
 * here acquires a prerequisite: a successful ARM_MOVE or ARM_VELOCITY route (BenchRoute.h) acquires
 * the identity and full read(s) exactly as MOVE and VELOCITY do, and service() then names them, with
 * the arming line's evidence, through ProbeMotion's setters, attaches that node's ProbeMotion to the
 * console and reports one notice line. ARM_WITNESS's two reads only give the AxisReference the console's
 * stepsperturn and axis origin consume; it names no motion prerequisite. A motion line is refused unless that arming's reads are still
 * the route's (no route has begun since) and the per-boot motion caps admit it; each admission the
 * console then makes is counted with the bench routes' (BenchRoute::countConsoleMotion). Before a
 * `stop` is fed, the stop prerequisites are named again, from the arming's stop budgets and the
 * latest complete state: the session's last move or velocity cycle, else the arming's read while it
 * stands, else the session's empty read, which leaves only the policy's best-effort fallback.
 */
#pragma once
#include "BenchRoute.h"
#include "ProbeConsole.h"

namespace MotorControlCANExample {
/** What the bench holds in this turn, as main.cpp derives it from its own flags. */
struct BenchActivity {
    /** A bench record (route, raw, PDO, D, K, H, identity), retained NMT request, soak or capture
     * holds or may use the owner. A terminal settled arming record does not. */
    bool wire = false;
    bool listing = false;    ///< A bench network listing (`n`) drains network observations.
    bool holdOutput = false; ///< A bench record dump, listing, response or NMT result waits for room.
};
/** How a complete console line may pass the bench. */
enum class ConsoleGate : uint8_t {
    ALWAYS,       ///< Inspection, local host intent, or the console's own retained results.
    IDLE_BENCH,   ///< Owner work or shared-state change: refused while the bench holds wire work.
    IDLE_LISTING, ///< Discovery: as IDLE_BENCH, and refused during a bench network listing.
    NEVER         ///< Host recovery: this image has none.
};
/** @param line Parsed console line. @return Its gate on this image; pure. Unknown names are ALWAYS:
 * the console itself refuses them. */
ConsoleGate benchConsoleGate(const ConsoleLine& line) noexcept;
/** What take() did with a byte. */
enum class ConsoleInput : uint8_t {
    COLLECTING, ///< The line continues.
    FORWARDED,  ///< The complete line went to the console, which replies itself.
    REFUSED,    ///< The line reached nothing; reply() holds the one-shot response to print.
};
/** One node's console motion attachment, placed once by the caller on that node's first arming. */
struct BenchAxis {
    MotorControlCAN::LICHUAN_C::ReadTarget target; ///< ProbeMotion's live route, set at each arming.
    /** Live conversion facts: the session's binding and integer motor rpm, until the console's
     * `stepsperturn` or `axis origin` adds a scale or origin through ProbeMotion. */
    MotorControlCAN::AxisConfig axis;
    /** The relative arming's stationary witness (two fresh reads, as MOVE takes one); none otherwise. */
    MotorControlCAN::AxisReference reference;
    ProbeMotion motion; ///< Over the bench route's own session of that node.
    /** @param firmware The assembly. @param session The route's session of this node, attached to it. */
    BenchAxis(FirmwareProbe& firmware, AxisSession& session) noexcept
        : motion(firmware.app(), firmware.owner(), session, target, axis, reference) {}
    BenchAxis(const BenchAxis&) = delete;            ///< ProbeMotion borrows the members.
    BenchAxis& operator=(const BenchAxis&) = delete; ///< ProbeMotion borrows the members.
};
/** The two large parts, each placed by the caller in a block of its own: on the S2 the largest free
 * internal heap block can be smaller than the whole console. Never freed. */
struct BenchConsoleStorage {
    void* device = nullptr;  ///< sizeof(ProbeDevice) bytes, aligned for it.
    void* console = nullptr; ///< sizeof(ProbeConsole) bytes, aligned for it.
};
/** The bench's console facility. Stationary; the caller places it once. */
class BenchConsole {
  public:
    enum {
        LINE_CAPACITY = ProbeConsole::LINE_CAPACITY, ///< With its NUL: at most 255 line bytes.
        REPLY_CAPACITY = 320, ///< With its NUL; main.cpp's one-shot response buffer is as large.
        NOTICE_CAPACITY = 192, ///< With its NUL: one arming notice.
        /** A console line still open this long after its `@` is dropped, as a T line is after 2 s. */
        COLLECT_TIMEOUT_US = 2000000,
        /** Console platform turns per service, each OUTPUT_BUDGET bytes: one whole record. */
        DRAIN_TURNS = unsigned(ProbeConsole::RECORD_CAPACITY) / unsigned(ProbePlatform::OUTPUT_BUDGET) + 1
    };
    /** Borrow the firmware's assembly; construction places the device helper and the console in the
     * caller's blocks, attaches the helper to the application and sets the read policy below. No
     * route, selection, request or output.
     * @param firmware Stationary assembly whose application and owner the console uses.
     * @param services That assembly's service store.
     * @param storage Two distinct blocks, borrowed for this object's life.
     */
    BenchConsole(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services,
                 const BenchConsoleStorage& storage) noexcept;
    BenchConsole(const BenchConsole&) = delete;            ///< Borrowed by its platform callbacks.
    BenchConsole& operator=(const BenchConsole&) = delete; ///< Holds live console state.
    /** The explicit console premise step (`o`), after main.cpp configured both bench routes:
     * record the controller's interface facts in the application. Sends nothing.
     * @param bitrate The bitrate `c` started the controller with. @return False for zero.
     */
    bool open(uint32_t bitrate) noexcept;
    /** Borrow the bench's route object, whose arming records this console consumes.
     * @param route Stationary, outliving this object. @return False for a different one. */
    bool attachRoute(BenchRoute& route) noexcept;
    /** Borrow one node's motion attachment, once. @param node 1 or 2. @param axis Stationary, over
     * that node's route session. @return False for an invalid node or a second attachment. */
    bool attachAxis(unsigned node, BenchAxis& axis) noexcept;
    /** @param node 1 or 2. @return Whether that node has a motion attachment. */
    bool hasAxis(unsigned node) const noexcept { return node >= 1 && node <= 2 && axes_[node - 1]; }
    /** Take one input byte; the first byte of a line must be `@`.
     * @param byte Input byte. @param nowUs Supplied platform time.
     * @param bench The bench's activity, judged when the line ends.
     * @return COLLECTING until CR or LF; then FORWARDED or REFUSED.
     */
    ConsoleInput take(uint8_t byte, uint64_t nowUs, const BenchActivity& bench) noexcept;
    /** @return After REFUSED, the response line with its newline, until the next take. */
    const char* reply() const noexcept { return reply_; }
    /** @return True while a line is being collected. */
    bool collecting() const noexcept { return collecting_; }
    /** Drop the line being collected after an input loss: its bytes may be spliced. The caller retires the
     * buffered input and discards through the next line end, as ProbeConsole does after a loss.
     * @return The refusal to print: the console's own form correlated by the line's @ID when its prefix
     * arrived whole (`"command":"input"`, `"error":"input_lost"`), else `CONSOLE_REFUSED reason=input-loss`. */
    const char* abandon() noexcept;
    /** Drop a line still open after COLLECT_TIMEOUT_US; the console never sees it.
     * @param nowUs Supplied platform time. @return True when a line was dropped now. */
    bool expire(uint64_t nowUs) noexcept;
    /** One turn after the firmware's and the route's: a new arming's handoff, console service, then
     * its records to the UART queue.
     * @param nowUs Supplied platform time, nondecreasing. @param bench The bench's activity.
     */
    void service(uint64_t nowUs, const BenchActivity& bench) noexcept;
    /** @return The pending arming notice line with its newline, or null. */
    const char* notice() const noexcept { return notice_[0] ? notice_ : nullptr; }
    /** Mark the notice printed. */
    void clearNotice() noexcept { notice_[0] = 0; }
    /** @return True while the console retains an operation, traces owner diagnostics, monitors or
     * scans: the bench then admits no owner work of its own. */
    bool holds() const noexcept;
    /** @return True while the console's discovery scan drains network observations. */
    bool scanning() const noexcept { return device_.discovery().active(); }
    /** @param sequence Reservation. @param transfer Pending download.
     * @return True when it is one of this console's motion attachments' own downloads. */
    bool ownsDownload(MotorControlCANOwner::SequenceId sequence,
                      const MotorControlCAN::CANOPEN::SdoTransfer& transfer) const noexcept;
    /** The owner handoff check of an owned download: ProbeMotion::preflight with this node's fault
     * decision: more than one EMCY since the arming, or one with a nonzero code. Stricter than
     * BenchRoute's own rule, which applies the same test per service turn and then moves its baseline:
     * this baseline is set once, at the node's successful move or velocity arming, so two no-error
     * (code 0) EMCYs at any time after it hold every later download too.
     * main.cpp returns this result and never falls through to a more permissive check.
     * @param sequence Reservation. @param transfer Pending download. @param nowUs Owner time.
     * @return False withdraws the download unsent. */
    bool preflight(MotorControlCANOwner::SequenceId sequence,
                   const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t nowUs) noexcept;
    /** @return The shared console, for inspection. */
    const ProbeConsole& console() const noexcept { return console_; }
    /** @return The device helper, for inspection. */
    const ProbeDevice& device() const noexcept { return device_; }
    /** @return The node whose motion the console's motion routes act on, 0 before any arming. */
    unsigned armedNode() const noexcept { return attached_; }

  private:
    static bool clock(void* context, uint64_t& output); ///< The firmware platform's clock.
    static bool write(void* context, uint8_t byte);
    void refuse(const ConsoleLine& line, const char* reason) noexcept;
    void remember() noexcept;
    bool sameSelection() const noexcept;
    bool policy() noexcept;
    const char* motionRefusal(const ConsoleLine& line) noexcept;
    void nameStop(uint64_t nowUs) noexcept;
    void applyArming(uint64_t nowUs) noexcept;
    const char* arm(BenchAxis& axis, uint64_t nowUs) noexcept;
    bool fault(unsigned node) const noexcept;
    bool armingCurrent(RouteKind kind) noexcept;
    FirmwareProbe& firmware_;
    ProbePlatform platform_; // The console's own byte queue; never the UART's.
    ProbeDevice& device_;
    ProbeConsole& console_;
    char line_[LINE_CAPACITY] = {};
    uint16_t used_ = 0;
    ConsoleLine parsed_; // The classified line; here, not on the stack under the console's dispatch.
    bool collecting_ = false, overlong_ = false, hold_ = false;
    bool policySet_ = false; // The measured identity width policy reached the device helper.
    uint64_t startedUs_ = 0;
    char reply_[REPLY_CAPACITY] = {};
    char notice_[NOTICE_CAPACITY] = {};
    // One record as the console's platform writes it, until its newline is queued for the UART.
    uint8_t staged_[ProbeConsole::RECORD_CAPACITY] = {};
    uint16_t stagedSize_ = 0;
    // The application's selection after the console's last line: what the console selected.
    MotorControlCAN::LICHUAN_C::ReadTarget selection_;
    bool selected_ = false;
    // Motion: the route whose arming records are consumed, one attachment per node, the node the
    // console's motion routes act on, the last arming consumed and its line, scratch for armed().
    BenchRoute* route_ = nullptr;
    BenchAxis* axes_[2] = {nullptr, nullptr};
    unsigned attached_ = 0;
    uint32_t consumedTrial_ = 0; // The last arming record looked at, whatever its outcome.
    uint32_t appliedTrial_ = 0;  // The last arming applied successfully, to the attached node.
    RouteKind appliedKind_ = RouteKind::NONE;
    struct StopBudget {
        bool named = false; // A successful move or velocity arming of this node gave them.
        uint32_t deadlineMs = 0, bestEffortMs = 0, intervalMs = 0;
    } budgets_[2];
    RouteArming arming_;
    uint32_t emcyBaseline_[2] = {0, 0};
};
} // namespace MotorControlCANExample
