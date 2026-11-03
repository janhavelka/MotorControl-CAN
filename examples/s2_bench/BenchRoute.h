/** @file BenchRoute.h
 * @brief Named finite bench routes for attended trials, through the sole owner and AxisSession.
 * SPDX-License-Identifier: MIT
 *
 * Application only: no SDK headers, heap, clock, retry or recovery. One cooperative caller services
 * the firmware first, then this object, on the owner task. Every frame goes through an AxisSession
 * opened with ProbeApp::openAxis on the ordinary owner: typed reads with their own read effect,
 * one-use experiment grants for every unqualified upload, and the session's own enable and stop.
 * There is no raw sender, no object selector and no command language: each route is a fixed table.
 *
 * Routes, one at a time, each with a new trial identity that rises for the life of this object:
 * - READ_ACTIONS, packet 15 P1. STATE(1001, 603F, 2100..2102, 6041, 6061, 6064, 606C),
 *   MOTION_PARAMETERS(6081, 6083, 6084), granted single uploads 1017, 607A, 60FF, then the
 *   accepted image (2100..2102, 6041, 6061, 6064, 606C) again. No writes.
 * - READ_IO, packet 13 P1. Image, IO_INPUT_CONFIGURATION(2400:00..08), IO_OUTPUT_CONFIGURATION
 *   (2400:0C..0F), IO_INPUT_FILTERS(2401..2407), IO_OUTPUT_COMMAND(60FE:00..02), image. 2400:0B is
 *   never read. No writes.
 * - READ_TUNING, packet 14 T0. Image, the four tuning groups 2502..2520, image. No writes.
 * - MODE_ONE, packet 15 P2. The READ_ACTIONS reads, a prerequisite check, one 6060:00=1 through
 *   AxisSession::admitPrepared, then the stated number of fresh cycles. Never enables.
 * - ENABLE, packet 15 P3. A fresh ERRORS/MONITORS/STATUS/FEEDBACK read, then
 *   AxisSession::requestEnable with the host's trial, reviewed 6041, 607A and 60FF and the
 *   attendant's shaft observation. The firmware never supplies an observation itself.
 * - STOP, packet 15 P4 and packet 16. A fresh MONITORS/STATUS read, then AxisSession::requestStop
 *   with the host's kind and policy. The path taken and every context it wrote are reported.
 * - POSITION, packet 19. An application-local isolated experiment, not a move engine: a fresh read,
 *   a prerequisite check against the host's reviewed start position and the session's acknowledged
 *   000F, the earlier 6081/6083/6084/607A for the record, then 607A, 6081, 6083, 6084 and one
 *   6040=001F trigger as host-reviewed native values, each after the previous acknowledgement, then
 *   fresh cycles and an optional named stop. MotionPreview stays an offline review artifact.
 *
 * Every motion route (MODE_ONE, ENABLE, STOP, POSITION) needs an attended flag that the host sets
 * only after the operator typed a confirmation. At most ROUTE_MOTION_TOTAL motion routes and
 * ROUTE_MOTION_PER_CASE per route and node are admitted per boot; a planned stop inside POSITION
 * counts as one more. Any failed terminal, lost reply, grant or admission refusal, changed epoch,
 * nonzero EMCY from the node or local abort ends the route and keeps its evidence; nothing is
 * retried, enabled, cleared, zeroed, restored or switched to another mode, and an abort sends no
 * stop. A request already on its way always settles first.
 *
 * Sessions stay open for the life of the firmware once a route has opened them: their history,
 * latest controlword and trial record must survive between routes. While one is open, routes that
 * take their own reservation on that node (RawEvidence, PdoExperiment) refuse. Grant identities
 * start at ROUTE_GRANT_BASE, above every other bench range, so a later DeviceEvidence grant on the
 * same channel is refused, which fails closed.
 */
#pragma once
#include "FirmwareProbe.h"
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>

namespace MotorControlCANExample {
/** Fixed route selection; not an object selector. */
enum class RouteKind : uint8_t {
    NONE,         ///< Unparsed or empty plan; refused.
    READ_ACTIONS, ///< Packet 15 P1 reads; `TA`.
    READ_IO,      ///< Packet 13 P1 reads; `TI`.
    READ_TUNING,  ///< Packet 14 T0 reads; `TU`.
    MODE_ONE,     ///< Packet 15 P2, one 6060:00=1; `TM`.
    ENABLE,       ///< Packet 15 P3, AxisSession::requestEnable; `TE`.
    STOP,         ///< Packet 15 P4 and packet 16, AxisSession::requestStop; `TS`.
    POSITION      ///< Packet 19 isolated absolute experiment; `TP`.
};
/** One parsed host request. Fields a route does not use are zero. Ranges are host campaign
 * policy and the documentary native ranges, never a drive timing or safety claim. */
struct RoutePlan {
    RouteKind kind = RouteKind::NONE; ///< Selected route.
    uint8_t node = 0;                 ///< Node 1 or 2, adapter slot node - 1.
    uint32_t trial = 0;               ///< Nonzero trial identity; must rise for this object's life.
    bool attended = false; ///< Motion routes: the host's record of a typed attended confirmation.
    uint8_t start = 0;     ///< ENABLE: 1 = EnableStart::NO_MOTION_WRITE_RECORDED, the host's record.
    uint8_t shaft = 0;     ///< ENABLE: attendant's typed shaft observation, 1 FREE or 2 HOLDING.
    uint16_t reviewedStatus = 0;        ///< ENABLE: exact reviewed 6041.
    int32_t reviewedPositionTarget = 0; ///< ENABLE: exact reviewed 607A, history and not intent.
    int32_t reviewedVelocityTarget = 0; ///< ENABLE: exact reviewed 60FF, history and not intent.
    uint32_t deadlineMs = 0; ///< ENABLE and STOP: action deadline after the request, 1..55000.
    /** MODE_ONE and POSITION: cycle spacing. ENABLE and STOP: the action's observation interval. */
    uint32_t intervalMs = 0;
    uint16_t cycles = 0;   ///< MODE_ONE 1..10 and POSITION 1..16 fresh observation cycles.
    uint8_t stopKind = 0;  ///< STOP: 1 HALT or 2 QUICK_STOP. POSITION: 0 none, 1 HALT, 2 QUICK_STOP.
    uint32_t bestEffortMs = 0;   ///< STOP and POSITION stop: best-effort cutoff, 0 refuses that branch.
    uint16_t stopAfter = 0;      ///< POSITION: request the stop after this many cycles, 1..cycles.
    uint32_t stopDeadlineMs = 0; ///< POSITION: the stop's sequenced deadline.
    uint32_t stopIntervalMs = 0; ///< POSITION: the stop's observation interval.
    int32_t startPosition = 0;   ///< POSITION: exact reviewed 6064 the fresh read must show.
    int32_t target = 0;          ///< POSITION: 607A, -1000000..1000000 pulses (EN86/ZH86 p11).
    uint32_t rpm = 0;            ///< POSITION: 6081, 0..3000 rpm (EN86/ZH86 p11).
    uint32_t rampUpMs = 0;       ///< POSITION: 6083, 0..2000 ms (EN86/ZH86 p11).
    uint32_t rampDownMs = 0;     ///< POSITION: 6084, 0..2000 ms (EN86/ZH86 p11).
};
/** Parse the text after the `T` command byte: a route letter, then decimal fields separated by
 * commas, with a leading minus only on signed fields and nothing else:
 * `A|I|U<node>,<trial>`;
 * `M<node>,<trial>,<attended>,<cycles>,<intervalMs>`;
 * `E<node>,<trial>,<attended>,<start>,<shaft>,<status>,<positionTarget>,<velocityTarget>,
 * <deadlineMs>,<intervalMs>`;
 * `S<node>,<trial>,<attended>,<kind>,<deadlineMs>,<bestEffortMs>,<intervalMs>`;
 * `P<node>,<trial>,<attended>,<startPosition>,<target>,<rpm>,<upMs>,<downMs>,<cycles>,<intervalMs>,
 * <stopKind>,<stopAfter>,<stopDeadlineMs>,<stopBestEffortMs>,<stopIntervalMs>` (one line each).
 * @param text Borrowed characters; no terminator required.
 * @param size Character count.
 * @param output Published only on success.
 * @return False for anything else, including a value outside its stated range.
 */
bool parseRouteCommand(const char* text, size_t size, RoutePlan& output) noexcept;
/** @param kind Route. @return Static lowercase name printed in records; "none" for NONE. */
const char* routeName(RouteKind kind) noexcept;
/** Why a route ended, as printed in its header. Device facts stay in the terminals. */
enum class RouteFailure : uint8_t {
    NONE,          ///< Every planned step completed with success evidence; not a qualification.
    AUTHORIZATION, ///< A one-use grant was refused; that upload was not admitted.
    ADMISSION,     ///< The session or owner refused a step, or a context refused preparation.
    SERVICE,       ///< A terminal other than success; kept, with any uncertainty it carries.
    CORRELATION,   ///< A settled terminal was not the request this route admitted.
    EVIDENCE_LOSS, ///< Copied traffic or terminals were lost; no further step was admitted.
    INVALIDATED,   ///< Reservation, node epoch or firmware progression ended before the next step.
    DEADLINE,      ///< The route cutoff or an operation deadline was reached between steps.
    PREREQUISITE,  ///< A fresh read before any write did not meet the route's stated condition.
    OBSERVATION,   ///< After a write, a fresh read or a nonzero EMCY met a stated stop condition.
    ACTION,        ///< The session's enable or stop ended without its success outcome.
    REFUSED,       ///< The library refused the enable or stop request; nothing of it was sent.
    ABORTED        ///< Local abort; no stop was sent.
};
/** Local resource sample; the SDK calls behind it live in the firmware entry. */
struct RouteResources {
    uint32_t stackFree = 0;   ///< Least bytes ever free on the owner task's stack.
    uint32_t heapFree = 0;    ///< Internal heap bytes free now.
    uint32_t heapMinimum = 0; ///< Least internal heap bytes ever free.
    uint32_t largestBlock = 0; ///< Largest internal heap block free now: the next session needs one.
};
/** Optional sampling, called at admission and at the terminal only. */
struct RouteCallbacks {
    void* context = nullptr; ///< Borrowed for this object's lifetime.
    void (*sample)(void*, RouteResources&) = nullptr; ///< Fills a zeroed sample; bounded, no I/O.
};
/** Fixed capacities and campaign caps. */
enum {
    ROUTE_TERMINALS = 128,      ///< Copied owner terminals; a plan that could exceed it is refused.
    ROUTE_TRAFFIC = 448,        ///< Copied wire and controller records; overflow is evidence loss.
    ROUTE_TAIL = 16,            ///< Records copied after the terminal, until release.
    ROUTE_MOTION_TOTAL = 12,    ///< Motion admissions per boot (packet 22).
    ROUTE_MOTION_PER_CASE = 3,  ///< Motion admissions per route and node per boot (packet 22).
    ROUTE_CUTOFF_MS = 60000,    ///< Route cutoff after admission (packet 22: sixty seconds a case).
    ROUTE_QUEUE_US = 100000,    ///< Queue budget per service, host policy.
    ROUTE_RESPONSE_US = 250000, ///< Response budget per service, host policy as for the soak.
    ROUTE_OPERATION_US = 5000000 ///< Deadline of one read or write operation, clamped to the cutoff.
};
/** First grant identity; RawEvidence uses 1000..4029 and ProbeApp reads count from 1. */
constexpr uint32_t ROUTE_GRANT_BASE = 100000;
/** One owner terminal, compacted from Completion for the RESULT line. */
struct RouteTerminal {
    uint64_t admittedUs = 0;    ///< Native admission time.
    uint64_t completedUs = 0;   ///< Native terminal time.
    uint32_t operation = 0;     ///< Transfer operation.
    uint32_t request = 0;       ///< Transfer request.
    uint32_t ownerRequest = 0;  ///< Owner result handle generation.
    uint32_t step = 0;          ///< Transfer step.
    uint32_t abortCode = 0;     ///< Raw abort code.
    uint32_t nodeGeneration = 0; ///< Node epoch at admission.
    uint16_t index = 0;         ///< Object index.
    uint8_t subindex = 0;       ///< Object subindex.
    uint8_t direction = 0;      ///< SdoDirection.
    uint8_t outcome = 0;        ///< SdoOutcome.
    uint8_t flags = 0;          ///< Bit0 acknowledged, bit1 effect possible, bit2 uncertain, bit3 cleanup.
    uint8_t size = 0;           ///< Uploaded bytes.
    uint8_t reason = 0;         ///< Owner CompletionReason.
    uint8_t stages = 0;         ///< Stage bits as the RESULT line's st field.
    uint8_t data[4] = {};       ///< First four uploaded bytes.
};
/** One owner diagnostic other than a terminal, compacted from Diagnostic for the TRAFFIC line. */
struct RouteTraffic {
    uint64_t processedUs = 0; ///< Owner processing time.
    uint64_t receivedUs = 0;  ///< RX capture when exact.
    uint64_t earliestUs = 0;  ///< RX lower bound when bounded.
    uint64_t latestUs = 0;    ///< RX upper bound when bounded.
    uint32_t id = 0;          ///< Frame identifier.
    uint32_t epoch = 0;       ///< Controller epoch.
    uint32_t txRequest = 0;   ///< Local TX identity: SDO request or NMT generation.
    uint32_t txFrame = 0;     ///< Local TX frame number.
    uint8_t data[8] = {};     ///< Frame bytes.
    uint8_t kind = 0;         ///< DiagnosticKind.
    uint8_t dlc = 0;          ///< Wire DLC.
    uint8_t flags = 0;        ///< Bit0 ext, bit1 rtr, bit2 accepted, bit3 exact time, bit4 bounds.
    uint8_t state = 0;        ///< ControllerState or RecoveryOutcome, by kind.
    uint8_t txKind = 0;       ///< TransportTxId kind.
};
/** Raw values of one fresh state read, kept for the prerequisite check and its summary line. */
struct RouteSnapshot {
    uint8_t blocks = 0;        ///< StateBlock bits present.
    uint8_t errorRegister = 0; ///< 1001.
    uint16_t errorCode = 0;    ///< 603F.
    uint16_t monitor = 0;      ///< 2100.
    uint16_t statusword = 0;   ///< 6041.
    uint8_t mode = 0;          ///< 6061 complete byte.
    int32_t position = 0;      ///< 6064 raw.
    int32_t velocity = 0;      ///< 606C raw.
};
/** Owned record of one route; only count prefixes are valid. Terminal fields never change after
 * the terminal; tail copies continue until release. */
struct RouteRecord {
    RoutePlan plan;                            ///< Copied host request.
    RouteFailure failure = RouteFailure::NONE; ///< First terminal cause.
    const char* why = "none";                  ///< Static token naming the condition.
    bool terminal = false;                     ///< The route has ended.
    bool evidenceLost = false;                 ///< Traffic or terminal capacity or owner trace lost.
    bool tailOverflow = false;                 ///< Tail capacity or owner trace lost after terminal.
    bool unsettled = false; ///< The route ended with its own request not settled (firmware failure).
    uint8_t step = 0;       ///< Route step index reached.
    uint8_t motionTotal = 0; ///< Motion admissions this boot, including this one.
    uint8_t caseCount = 0;   ///< Motion admissions of this route and node this boot.
    uint16_t cycles = 0;     ///< Completed observation cycles.
    uint64_t startedUs = 0, completedUs = 0; ///< Admission and terminal times.
    uint64_t maxServiceGapUs = 0;            ///< Largest interval between service calls.
    RouteResources before; ///< Sample at admission.
    RouteResources after;  ///< Sample at the terminal: stackFree is the least ever, so a lower
                           ///< value than before was reached during this route.
    uint32_t dropped = 0;  ///< Lost records, saturating.
    uint8_t admission = 0; ///< BusAdmission of the last refused admission; meaningful for ADMISSION.
    uint8_t terminalCount = 0; ///< Valid terminals.
    uint16_t trafficCount = 0; ///< Valid traffic.
    uint8_t tailCount = 0;     ///< Valid tail.
    RouteTerminal terminals[ROUTE_TERMINALS]; ///< Every owner terminal in publication order.
    RouteTraffic traffic[ROUTE_TRAFFIC];      ///< Every other owner diagnostic in order.
    RouteTraffic tail[ROUTE_TAIL];            ///< Diagnostics after the terminal.
    bool checkPresent = false; ///< A prerequisite check ran on snapshot below.
    bool checkPassed = false;  ///< That check passed.
    RouteSnapshot check;       ///< Values the check judged.
    RouteSnapshot last;        ///< Latest successful fresh read.
    bool statusPresent = false; ///< A library request was refused; the next fields hold it.
    uint8_t statusCode = 0;     ///< Err of that refusal.
    int32_t statusDetail = 0;   ///< Detail of that refusal.
    const char* statusMessage = ""; ///< Static message of that refusal.
    bool enableRequested = false;   ///< requestEnable accepted.
    bool stopRequested = false;     ///< requestStop returned a path other than NONE.
    uint8_t stopRequestKind = 0;    ///< StopKind asked for.
    uint8_t stopPathRequested = 0;  ///< StopPath requestStop returned.
    uint64_t actionRequestedUs = 0; ///< When the enable or stop was requested.
    bool emcy = false;              ///< A nonzero EMCY of this node was copied.
    uint8_t emcyData[8] = {};       ///< Its bytes.
};
/** One attended route at a time over borrowed sessions. Large and stationary; allocates nothing.
 * The caller places it, attaches one session per node before that node's first route, services the
 * firmware and then this object each turn, and prints records with formatLine.
 */
class BenchRoute {
  public:
    /** Borrow stationary resources; construction does nothing else.
     * @param firmware Assembly whose application and owner carry every route.
     * @param services That assembly's service store, with routes the caller configured.
     * @param callbacks Optional resource sampling.
     */
    BenchRoute(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services,
               const RouteCallbacks& callbacks) noexcept;
    BenchRoute(const BenchRoute&) = delete;            ///< Holds live evidence and borrowed sessions.
    BenchRoute& operator=(const BenchRoute&) = delete; ///< Holds live evidence and borrowed sessions.
    /** Attach the stationary session of one node; it is opened on that node's first route.
     * @param node Node 1 or 2.
     * @param session Closed session on this firmware's owner, outliving this object.
     * @return False when the node is invalid or already has one.
     */
    bool attach(unsigned node, AxisSession& session) noexcept;
    /** @param node Node 1 or 2. @return Whether that node has a session attached. */
    bool attached(unsigned node) const noexcept {
        return node >= 1 && node <= 2 && sessions_[node - 1] != nullptr;
    }
    /** @param node Node 1 or 2. @return The attached session, or null. */
    const AxisSession* session(unsigned node) const noexcept {
        return attached(node) ? sessions_[node - 1] : nullptr;
    }
    /** Admit one route. Sends nothing during this call.
     * @param plan Copied request.
     * @param nowUs Owner time, at least its last time.
     * @return Null when admitted; otherwise a static refusal token, with nothing changed except
     * that a session opened here stays open.
     */
    const char* begin(const RoutePlan& plan, uint64_t nowUs) noexcept;
    /** One turn after the firmware's: copy diagnostics, settle, check, admit at most one step.
     * @param nowUs Owner time; an earlier value is ignored for this turn.
     */
    void service(uint64_t nowUs) noexcept;
    /** Withdraw the active route locally. Sends no stop. A request on its way settles first; an
     * active enable is withdrawn through AxisSession::cancelEnable.
     * @param nowUs Owner time.
     * @return Null when the route will end ABORTED; otherwise a static refusal token. A stop in
     * progress refuses: it settles by itself.
     */
    const char* abort(uint64_t nowUs) noexcept;
    /** Release a terminal record once local TX has retired. Sessions stay open.
     * @return False while active, with nothing retained, or while local TX is outstanding.
     */
    bool release() noexcept;
    /** @return Borrowed record from admission until release, or null. */
    const RouteRecord* record() const noexcept { return occupied_ ? &record_ : nullptr; }
    /** @return True from admission until the terminal. */
    bool active() const noexcept { return occupied_ && !record_.terminal; }
    /** @return Motion admissions this boot. */
    uint8_t motionAdmissions() const noexcept { return motionTotal_; }
    /** Format one line of the terminal record: header, summaries, RESULT, TRAFFIC, TAIL, END.
     * @param cursor Zero-based line index.
     * @param output Destination.
     * @param size Its capacity, at least 256 bytes for every line to fit.
     * @return False past the END line or before the terminal; output is then untouched.
     */
    bool formatLine(unsigned cursor, char* output, size_t size) const noexcept;

  private:
    enum StepKind : uint8_t {
        STEP_STATE, STEP_GROUP, STEP_TUNING, STEP_SINGLE, STEP_CHECK_MODE, STEP_CHECK_POSITION,
        STEP_MODE_WRITE, STEP_ENABLE, STEP_STOP, STEP_POSITION_WRITES, STEP_OBSERVE, STEP_END
    };
    struct Step {
        uint8_t kind;
        uint8_t arg;
    };
    FirmwareProbe& firmware_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    RouteCallbacks callbacks_;
    AxisSession* sessions_[2] = {nullptr, nullptr};
    const Step* steps_ = nullptr;
    // Scratch and work outside RouteRecord: too large for the task stack.
    MotorControlCAN::LICHUAN_C::DriverContext context_;
    MotorControlCAN::LICHUAN_C::StateObservation block_;
    MotorControlCANOwner::Diagnostic diagnostic_;
    MotorControlCAN::CANOPEN::SdoTransfer expected_, single_;
    RouteRecord record_;
    uint64_t lastUs_ = 0, cutoffUs_ = 0, nextCycleUs_ = 0, cycleStartUs_ = 0, writeDeadlineUs_ = 0;
    uint32_t lastTrial_ = 0, grant_ = ROUTE_GRANT_BASE - 1, grantedOperation_ = 0, grantedStep_ = 0;
    uint32_t writeOperation_ = 0, overwritten_ = 0, rxLost_ = 0;
    uint8_t motionTotal_ = 0, motionCases_[8][2] = {};
    uint8_t stepIndex_ = 0, subStep_ = 0, readMode_ = 0, readArg_ = 0;
    bool occupied_ = false, pending_ = false, stepActive_ = false, readActive_ = false;
    bool singleAdmitted_ = false, actionStarted_ = false, abortRequested_ = false, emcyHandled_ = false;
    bool modeAdmitted_[2] = {false, false};
    RouteFailure endFailure_ = RouteFailure::NONE; ///< Abort or EMCY, applied once work settles.
    const char* endWhy_ = "none";

    AxisSession& axis() noexcept { return *sessions_[record_.plan.node - 1]; }
    const AxisSession& axis() const noexcept { return *sessions_[record_.plan.node - 1]; }
    uint8_t slot() const noexcept { return uint8_t(record_.plan.node - 1); }
    bool currentTarget(MotorControlCAN::LICHUAN_C::ReadTarget& output) const noexcept;
    void finish(RouteFailure failure, const char* why, uint64_t nowUs) noexcept;
    void endAfterSettling(RouteFailure failure, const char* why) noexcept;
    void capture(bool tail) noexcept;
    void lose(uint32_t count, bool tail) noexcept;
    void copyTerminal(const MotorControlCANOwner::Completion& done) noexcept;
    void copyTraffic(const MotorControlCANOwner::Diagnostic& in, bool tail) noexcept;
    bool settle(uint64_t nowUs) noexcept;
    void advanceStep(uint64_t nowUs) noexcept;
    void nextStep(uint64_t nowUs) noexcept;
    bool startRead(uint8_t mode, uint8_t arg, uint64_t nowUs) noexcept;
    void serviceRead(uint64_t nowUs) noexcept;
    void readDone(uint64_t nowUs) noexcept;
    bool snapshot(uint8_t mask, uint64_t nowUs) noexcept;
    void admit(const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t nowUs) noexcept;
    bool startWrite(uint64_t nowUs) noexcept;
    void serviceWrite(uint64_t nowUs) noexcept;
    bool writeTransfer(MotorControlCAN::CANOPEN::SdoTransfer& output) const noexcept;
    const char* checkMode() const noexcept;
    const char* checkPosition() const noexcept;
    void startEnable(uint64_t nowUs) noexcept;
    void startStop(uint8_t kind, uint32_t deadlineMs, uint32_t bestEffortMs, uint32_t intervalMs,
                   uint64_t nowUs) noexcept;
    void serviceAction(uint64_t nowUs) noexcept;
    void serviceObserve(uint64_t nowUs) noexcept;
    void observeDone(uint64_t nowUs) noexcept;
    unsigned summaryCount() const noexcept;
    bool formatSummary(unsigned index, char* output, size_t size) const noexcept;
};
} // namespace MotorControlCANExample
