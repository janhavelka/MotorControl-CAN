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
 * owner work (main.cpp). Except for inspection, `host`/`useaddr` and explicit stops, a line is also
 * refused when the application's selection differs from the console's last line, as after a
 * bench identity read selected another node. A named stop uses its own axis attachment and budget;
 * it does not need to change selection or wait for unrelated selection repair.
 *
 * Paired arming (2026-11-16). ARM_MOVE_PAIR (`Ta`) and ARM_VELOCITY_PAIR (`Te`) acquire both nodes' reads in
 * one record; service() then arms each axis exactly as from its own TY or TB, the first node and then
 * the second, and reports one notice: `CONSOLE armed pair nodes=1,2 ...` or `CONSOLE arm refused node=N
 * ...; pair: this trial armed neither node`. A retained operation on either axis refuses both before
 * anything is named; a refusal of the second withdraws the first's motion prerequisites (armedTrial 0)
 * and keeps its stop named. main.cpp prints a one-line summary of that record instead of dumping it, so
 * neither the dump nor a second arming line delays the motion lines: both axes are armed before either
 * moves, within each arming's age cap. The full record stays retained: `d` dumps it once the console
 * retains no operation (dumpRefusal), and `r` is refused before that dump. Admissions and results stay
 * per axis: paired arming claims no simultaneous or atomic motion.
 *
 * Motion. The console's motion routes act on one BenchAxis per node: a ProbeMotion over the bench
 * route's own AxisSession of that node, with its live route, AxisConfig and AxisReference. Nothing
 * here acquires a prerequisite: a successful ARM_MOVE or ARM_VELOCITY route (BenchRoute.h) acquires
 * the identity and full read(s) exactly as MOVE and VELOCITY do, and service() then names them, with
 * the arming line's evidence, through ProbeMotion's setters, attaches that node's ProbeMotion to the
 * console and reports one notice line. ARM_WITNESS's two reads only give the AxisReference the console's
 * stepsperturn and axis origin consume; it names no motion prerequisite. Each axis owns copies of
 * its acquired reads and their original times; another axis may reuse the route scratch. A motion
 * line still needs current native prerequisites and any per-boot caps the route has set (RouteCaps, off by
 * default); each admission the
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
/** One turn's bench facts as main.cpp's own flags hold them. benchActivity, trialLineRefusal and dumpRefusal
 * are that image's admission policy (2026-11-16); the native console suite drives the same functions. */
struct BenchHolds {
    const RouteRecord* trial = nullptr; ///< BenchRoute's record, active or retained.
    bool records = false;       ///< An identity, raw, PDO or commissioning (D, K, H) record, active or retained.
    bool soak = false;          ///< A soak runs.
    bool nmt = false;           ///< An NMT request is retained.
    bool nmtUnreported = false; ///< That request's result line has not been written yet.
    bool capture = false;       ///< A capture blocks owner work (SilentCapture::blocking).
    bool dumping = false;       ///< A bench record dump is in progress.
    bool listing = false;       ///< A bench network listing (`n`) is in progress.
    bool retained = false;      ///< A one-shot response waits for UART room; no input is taken meanwhile.
    bool console = false;       ///< BenchConsole::holds(), after the console's turn.
    bool controller = false;    ///< The CAN controller is ready.
};
/** @return What the console sees of the bench in this turn. A terminal settled arming record, single or
 * paired, holds only its dump: the console's motion it armed may run. Pure. */
BenchActivity benchActivity(const BenchHolds& holds) noexcept;
/** @return Why a whole T line is refused before it is parsed ("console", "capture" or "state"), or null
 * when the route may begin. Pure. "console" refuses every T line while the console retains an operation
 * on any axis, a move of the other axis included: a paired arming (Ta, Te) prepares both axes first, so
 * no arming has to pass that guard while an axis moves. */
const char* trialLineRefusal(const BenchHolds& holds) noexcept;
/** @param terminal A bench record is terminal. @return Why `d` is refused ("unavailable", or "console"
 * for a paired arming's record while the console retains an operation), or null. That dump waits for
 * the console's results to be released, so it never holds a motion or stop reply. Pure. */
const char* dumpRefusal(const BenchHolds& holds, bool terminal) noexcept;
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
    /** Owned arming evidence. Another axis's acquisition may reuse the route's scratch,
     * but never these reads or their original timestamps. */
    MotorControlCAN::LICHUAN_C::ReadContext armedState;
    MotorControlCAN::LICHUAN_C::IdentityObservation armedIdentity;
    uint32_t armedTrial = 0; ///< Zero before a successful arming; never an observation refresh.
    RouteKind armedKind = RouteKind::NONE; ///< Exact prerequisite acquisition kind.
    /** Over the bench route's own session. Only an explicit TY/TB allowChanges flag installs
     * ATTENDED_EXPERIMENT change prerequisites, scoped to that fresh arming trial/firmware;
     * every ordinary rearming closes both change kinds. No feedback relation is invented. */
    ProbeMotion motion;
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
        LINE_CAPACITY = ProbeConsole::LINE_CAPACITY, ///< With its NUL: at most 831 line bytes (prompt 58).
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
    bool hasAxis(unsigned node) const noexcept { return node >= 1 && node <= BENCH_NODES && axes_[node - 1]; }
    /** Borrow the console's one commissioning helper (prompt 57), over node's route session and that node's
     * BenchAxis target, once, and attach it to the console: `persistence save parameters` and `persistence
     * restore factory settings` then read their own prerequisites on that node. Sends nothing. Without it
     * (main.cpp could not place its block, estimated at 13.5 KB, not measured on a board) those commands are
     * refused at once with their reason and nothing is transmitted.
     * @param node 1 or 2, with an attached axis. @param commissioning Stationary, over that axis's session
     * and target. @return False for an invalid node, no axis there, a second helper or another owner graph. */
    bool attachCommissioning(unsigned node, ProbeCommissioning& commissioning) noexcept;
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
     * @return True when it is one of this console's motion attachments' own downloads, or the commissioning
     * helper's held operation on its node's session. */
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
    void nameStop(unsigned node, uint64_t nowUs) noexcept;
    unsigned selectedNode() const noexcept;
    void applyArming(uint64_t nowUs) noexcept;
    const char* arm(BenchAxis& axis, uint64_t nowUs) noexcept;
    bool fault(unsigned node) const noexcept;
    bool armingCurrent(unsigned node, RouteKind kind) noexcept;
    FirmwareProbe& firmware_;
    ProbePlatform platform_; // The console's own byte queue; never the UART's.
    ProbeDevice& device_;
    ProbeConsole& console_;
    ProbeSync sync_; // Same sole owner; construction is passive.
    GroupStopMember groupRows_[BENCH_NODES]; // Reserved independently of normal console pressure.
    GroupStop groupStop_;
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
    BenchAxis* axes_[BENCH_NODES] = {};
    ProbeCommissioning* commissioning_ = nullptr; ///< attachCommissioning's helper, over commissioningNode_.
    unsigned commissioningNode_ = 0;
    unsigned attached_ = 0;
    uint32_t consumedTrial_ = 0; // The last arming record looked at, whatever its outcome.
    struct StopBudget {
        bool named = false; // A successful move or velocity arming of this node gave them.
        uint32_t deadlineMs = 0, bestEffortMs = 0, intervalMs = 0;
    } budgets_[BENCH_NODES];
    RouteArming arming_;
    RouteArming pairArming_; // A paired arming's second node, off the stack.
    uint32_t emcyBaseline_[BENCH_NODES] = {};
};
} // namespace MotorControlCANExample
