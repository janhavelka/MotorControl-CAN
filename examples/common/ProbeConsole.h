/** @file ProbeConsole.h
 * @brief Shared bounded ASCII console; application behavior stays in ProbeApp.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "MotionIntent.h"
#include "GroupStop.h"
#include "ProbeApp.h"
#include "ProbeCommissioning.h"
#include "ProbeDevice.h"
#include "ProbeMotion.h"
#include "ProbeSettings.h"
#include "ProbeSync.h"

namespace MotorControlCANExample {
/** Effect classification used by this inventory and later device routes. */
enum class ConsoleEffect : uint8_t {
    CACHE,              ///< Inspect existing evidence; no device or host mutation.
    PURE,               ///< Checked arithmetic, with no admission or device traffic.
    HOST_CONFIGURATION, ///< Explicit host facts or result/counter storage only.
    LOCAL_CANCEL,       ///< Withdraw local work, preserving possible remote effects.
    HOST_RECOVERY,      ///< Explicit controller retirement/recovery; no device reset.
    DEVICE_READ,        ///< Explicit reviewed upload; read effects remain separately qualified.
    DEVICE_SETTING,     ///< Reviewed settings workflow, including partial/unknown effects.
    DEVICE_ACTION       ///< Explicit reviewed device action with separate completion evidence.
};
/** Immediate inspection, host mutation, or admitted asynchronous work. */
enum class ConsoleCompletion : uint8_t {
    REPLY,       ///< Synchronous local reply; no retained operation created.
    ASYNCHRONOUS ///< Local admission plus an independently retained terminal operation.
};
/** One canonical route. Gated rows have a reason/owner and cannot dispatch. */
struct ConsoleCommand {
    const char* name;       ///< Exact first token; static lifetime.
    const char* grammar;    ///< Complete working grammar, or future route label when gated.
    const char* api;        ///< Actual public/application path, or the named future obligation.
    const char* capability; ///< Requirement independent of installed hardware qualification.
    ConsoleEffect effect;   ///< Side effects; never inferred from a command name.
    ConsoleCompletion completion; ///< Local reply versus retained asynchronous operation.
    const char* help;             ///< Static human help text.
    const char* gate;             ///< Null for implemented rows; exact reason otherwise.
    uint8_t owner;                ///< Owning prompt, including outstanding gated routes.
};
/** @param count Receives finite row count. @return Static immutable inventory. */
const ConsoleCommand* consoleCommands(size_t& count) noexcept;
/** Whole-line pure parse result. Token pointers refer into owned text below. */
struct ConsoleLine {
    char text[256] = {};       ///< Owned ASCII text; at most 255 bytes before terminator.
    uint16_t offsets[24] = {}; ///< Up to 24 tokens, excluding the correlation prefix.
    uint8_t count = 0;         ///< Actual command token count.
    bool json = false;         ///< True only for strict nonzero correlation prefix.
    uint32_t id = 0;           ///< Request ID, distinct from asynchronous operation IDs.
    /** @param n Token index. @return Borrowed token, or empty text outside count. */
    const char* token(size_t n) const noexcept { return n < count ? text + offsets[n] : ""; }
};
/** Parse a complete borrowed span, without newline, I/O or application mutation.
 * Printable ASCII and TAB only; no quotes/escapes, NUL, trailing CR or truncation.
 * @param bytes Nonnull span of size bytes. @param size 1..255 bytes.
 * @param output Owned candidate, unchanged on failure. @return Local parsing status.
 */
MotorControlCAN::Status parseConsoleLine(const char* bytes, size_t size,
                                         ConsoleLine& output) noexcept;
/** Bounded JSON string escaping, including surrounding quotes and NUL termination.
 * @param text Borrowed NUL-terminated ASCII text, at most 511 characters;
 * an unterminated input must provide 512 readable bytes.
 * @param output Destination, cannot overlap text.
 * @param capacity Available bytes. @return True complete; false leaves output unchanged.
 */
bool consoleJsonString(const char* text, char* output, size_t capacity) noexcept;
/** Cached pressure counters; saturating except the current storage sizes. */
struct ConsoleStats {
    uint32_t lines = 0;         ///< Complete nonempty input lines, including rejected lines.
    uint32_t invalid = 0;       ///< Invalid characters, lengths, grammar or exact values.
    uint32_t outputDropped = 0; ///< Whole ordinary replies refused before any local mutation.
    uint32_t inputLost = 0;     ///< Platform overflow incidents; discard through next terminator.
    uint16_t queued = 0;        ///< Complete records retained locally, including per-axis stop reserves.
};
/** One cooperative application console. No CAN receiver, clock, allocation, retry
 * or independent operation executor. Call app.service(now) first, then console.service(now).
 * Records include their newline and are copied atomically into ProbePlatform.
 * Physical byte writes can be partial; the suffix is retained without interleaving.
 * Ordinary pressure never consumes local-control, per-axis stop or group stop reply storage.
 * All borrowed resources outlive this stationary object; callbacks cannot reenter.
 */
class ProbeConsole {
  public:
    enum {
        LINE_CAPACITY = 256,    ///< Includes NUL; at most 255 input text bytes.
        TOKEN_CAPACITY = 24,    ///< Command tokens excluding optional correlation prefix.
        RECORD_CAPACITY = 512,  ///< Includes NUL; at most 511 record bytes including newline.
        ORDINARY_REPLIES = 2,   ///< Complete ordinary records; no partial overwrite.
        CONTROL_REPLIES = 2,    ///< Additional records reserved for local cancel/recover replies.
        INPUT_BUDGET = 32,      ///< Maximum buffered input bytes consumed per service turn.
        TRACE_CAPACITY = 80,    ///< Application diagnostic copies; independent of UART pressure.
        MOTION_CAPACITY = MotorControlCAN::CANOPEN::SdoClient::CHANNELS, ///< Registered actual axes.
        OPERATION_CAPACITY = 2 * MOTION_CAPACITY, ///< Two ordinary correlations per configured axis.
        STOP_REPLY_CAPACITY = 2 * MOTION_CAPACITY, ///< Normal and fast output reserves per axis.
        GROUP_OPERATION_SLOT = OPERATION_CAPACITY + STOP_REPLY_CAPACITY, ///< Independently reserved group stop.
        TOTAL_OPERATION_CAPACITY = GROUP_OPERATION_SLOT + 2 ///< Group stop plus host recovery.
    };
    /** Borrow existing application/owner/platform; construction performs no I/O.
     * @param app Existing stationary shared application, outlives the console.
     * @param owner Its sole receive owner, outlives the console.
     * @param platform Its stationary byte buffers, outlives the console.
     */
    ProbeConsole(ProbeApp& app, CanBusOwner& owner, ProbePlatform& platform) noexcept;
    /** Attach existing optional device facilities; no selection, I/O or permission is created.
     * @param device Stationary helper sharing this application/owner, outlives the console.
     * @return False unchanged for a different graph or an already attached helper. */
    bool attachDevice(ProbeDevice& device) noexcept {
        if ((device_ && device_ != &device) || !device.matches(app_, owner_))
            return false;
        device_ = &device;
        return true;
    }
    /** Borrow an explicitly configured settings bridge; no prerequisite/read grant is created.
     * @param settings Stationary actual axis/session bridge, outlives this console.
     * @return False unchanged for a different owner graph or replacement attachment. */
    bool attachSettings(ProbeSettings& settings) noexcept {
        if ((settings_ && settings_ != &settings) || !settings.usesOwner(owner_))
            return false;
        settings_ = &settings;
        return true;
    }
    /** Borrow an externally armed typed commissioning bridge; no review/grant is invented.
     * @param commissioning Stationary same-selected-axis bridge, outlives the console.
     * @return False unchanged for a different owner graph or replacement attachment. */
    bool attachCommissioning(ProbeCommissioning& commissioning) noexcept {
        if ((commissioning_ && commissioning_ != &commissioning) ||
            !commissioning.usesOwner(owner_))
            return false;
        commissioning_ = &commissioning;
        return true;
    }
    /** Register one actual axis attachment without selecting it or creating qualification.
     * @param motion Stationary same application/owner attachment; outlives this console.
     * @return False unchanged for duplicate logical axis ID, another graph or full storage. */
    bool attachMotion(ProbeMotion& motion) noexcept;
    /** Inspect a registered logical axis, independently of node IDs or the current selection.
     * @param axisId Nonzero logical identity. @return Borrowed registered helper or null. */
    ProbeMotion* motionForAxis(uint32_t axisId) const noexcept;
    /** @return Registered helper matching the exact current application selection, or null.
     * With no application selection, a sole attachment preserves the legacy one-axis route. */
    ProbeMotion* selectedMotion() const noexcept;
    /** Borrow caller-owned group storage and graph; no stop or result is created.
     * @param group Stationary same-graph helper. @return False for another graph or helper. */
    bool attachGroupStop(GroupStop& group) noexcept {
        if ((groupStop_ && groupStop_ != &group) || !group.matches(app_, owner_))
            return false;
        groupStop_ = &group;
        return true;
    }
    /** Borrow a finite SYNC helper; no arm, producer grant or traffic is created.
     * @param sync Stationary same-owner helper, outlives this console.
     * @return False unchanged for replacement attachment or a different owner graph. */
    bool attachSync(ProbeSync& sync) noexcept {
        if ((sync_ && sync_ != &sync) || !sync.usesOwner(owner_))
            return false;
        sync_ = &sync;
        return true;
    }
    /** Withdraw the selected attachment, or the sole attachment when none is selected.
     * No traffic; the attachment's own results and native contexts stay as they are.
     * @return False unchanged while its console correlation is retained or selection is ambiguous. */
    bool detachMotion() noexcept;
    /** Withdraw one logical axis without affecting others. Retained correlations prevent removal.
     * @param axisId Actual logical identity. @return False unchanged when absent or retained. */
    bool detachMotion(uint32_t axisId) noexcept;
    ProbeConsole(const ProbeConsole&) = delete; ///< Stationary retained identities and buffers.
    ProbeConsole& operator=(const ProbeConsole&) = delete; ///< No mutable copies.
    /** Consume one byte. Invalid/overlong lines discard through CR/LF before dispatch.
     * @param byte Borrowed ASCII ingress value. @param nowUs Supplied monotonic us.
     * No owner progression; caller services owner independently of output pressure.
     */
    void feed(uint8_t byte, uint64_t nowUs) noexcept;
    /** Poll retained operations, drain at most one complete record and consume at
     * most INPUT_BUDGET buffered bytes. No clock, CAN receive or owner service.
     * @param nowUs Supplied nondecreasing us; zero valid. Backward time does nothing.
     */
    void service(uint64_t nowUs) noexcept;
    /** Associate an externally admitted typed read with its console correlation.
     * The app continues owning its context and advancement; no synthetic success.
     * @param requestId Original nonzero correlation ID, or zero for human mode.
     * @param json Frozen output mode. @param operationId Nonzero console operation ID.
     * @return False unchanged if no active read, duplicate identity or capacity exhausted.
     */
    bool trackRead(uint32_t requestId, bool json, uint32_t operationId) noexcept;
    /** @return Cached pressure copy; performs no I/O or progress. */
    ConsoleStats stats() const noexcept;
    /** @return True after complete input/output progression; delivered retained results
     * do not block it. No clock, I/O, release or mutation. Platform output is separate. */
    bool drained() const noexcept;
    /** @return True from an operation's admission until its explicit release, whether it is
     * active or terminal, delivered or not. An application that shares the owner with other
     * request producers keeps them off meanwhile. No clock, I/O or mutation. */
    bool retainsOperation() const noexcept;
    /** @return True while capturing or retaining copied diagnostics for output.
     * Other diagnostic consumers must wait; no I/O or mutation. */
    bool tracing() const noexcept { return debug_ != 0 || traceCount_ != 0; }

  private:
    struct Record {
        char text[RECORD_CAPACITY] = {};
        uint16_t length = 0;
        uint32_t request = 0, terminal = 0;
    };
    struct Operation {
        bool used = false, json = false, terminal = false, delivered = false, recovery = false,
             queuedTerminal = false, deliveryPending = false;
        uint32_t request = 0, id = 0, readOperation = 0, nativeOperation = 0;
        uint32_t originalMotion = 0, replacementMotion = 0; ///< Retained console relation, not wire tags.
        uint32_t deliveryEnd =
            0; ///< Modulo outputWritten position after the complete terminal line.
        MotorControlCANOwner::RecoveryId recoveryId;
        MotorControlCAN::CANOPEN::NmtRequestId nmtId;
        bool groupStop = false, automatic = false, discovery = false, capture = false, monitor = false,
             settings = false, commissioning = false, motion = false, sync = false;
        uint32_t axisId = 0; ///< Captured logical identity, independent of address and selection.
        ProbeMotion* attachment = nullptr; ///< Frozen actual helper; selection changes cannot redirect results.
        uint8_t stop = 0; ///< Zero ordinary, one HALT, two QUICK_STOP; per-axis retained slots.
        uint8_t valueCursor = 0;
        char command[24] = "read";
        char terminalRecord[RECORD_CAPACITY] = {};
    } operations_[TOTAL_OPERATION_CAPACITY];
    ProbeDevice* device_ = nullptr;
    ProbeSettings* settings_ = nullptr;
    ProbeCommissioning* commissioning_ = nullptr;
    struct MotionAttachment {
        ProbeMotion* motion = nullptr;
        SimpleMoveIntent intent;
    } motions_[MOTION_CAPACITY];
    ProbeSync* sync_ = nullptr;
    GroupStop* groupStop_ = nullptr;
    ProbeMotion* groupMembers_[MOTION_CAPACITY] = {};
    bool groupMemberPrinted_[MOTION_CAPACITY] = {};
    Record groupReply_;
    bool groupReplyUsed_ = false;
    SimpleMoveIntent intent_;
    uint8_t debug_ = 0;
    // Only the fields the console publishes. Keeping Completion out saves scarce S2 RAM.
    struct TraceRecord {
        uint64_t processedUs = 0;
        uint32_t id = 0;
        uint8_t data[8] = {};
        uint8_t kind = 0, dlc = 0, payloadLength = 0, flags = 0;
    } trace_[TRACE_CAPACITY];
    uint8_t traceHead_ = 0;
    uint16_t traceCount_ = 0;
    uint32_t traceDropped_ = 0;
    ProbeApp& app_;
    CanBusOwner& owner_;
    ProbePlatform& platform_;
    char line_[LINE_CAPACITY] = {};
    uint16_t used_ = 0;
    bool discard_ = false, previousCr_ = false;
    uint64_t lastUs_ = 0;
    uint32_t nextOperation_ = 0;
    ConsoleStats stats_;
    Record ordinary_[ORDINARY_REPLIES], control_[CONTROL_REPLIES];
    Record stopReplies_[STOP_REPLY_CAPACITY];
    bool stopReplyUsed_[STOP_REPLY_CAPACITY] = {};
    unsigned nextStopReply_ = 0;
    uint8_t ordinaryCount_ = 0, controlCount_ = 0;
    struct Listing {
        bool active = false, caps = false, json = false, axis = false;
        bool configured = false, referenceKnown = false;
        uint32_t id = 0;
        size_t next = 0;
        MotorControlCAN::AxisConfig configuration;
    } listing_;
    bool enqueue(const char* text, bool control = false, uint32_t request = 0,
                 uint32_t terminal = 0) noexcept;
    void dispatch(const ConsoleLine& line, uint64_t now) noexcept;
    void dispatchLocal(const ConsoleLine& line, uint64_t now, size_t index) noexcept;
    void error(uint32_t id, bool json, const char* command, const char* reason,
               bool control = false,
               MotorControlCAN::Err code = MotorControlCAN::Err::ILLEGAL_VALUE,
               int32_t detail = 0) noexcept;
    void poll(uint64_t now) noexcept;
    void dispatchDevice(const ConsoleLine& line, uint64_t now) noexcept;
    void dispatchMotion(const ConsoleLine& line, uint64_t now) noexcept;
    void velocityChangeAccepted(const ConsoleLine& line, uint32_t operation) noexcept;
    void motionIntentReply(const ConsoleLine& line, bool inspect) noexcept;
    void motionAccepted(Operation& operation) noexcept;
    bool motionValues(Operation& operation) noexcept;
    bool motionTerminal(Operation& operation) noexcept;
    void dispatchGroupStop(const ConsoleLine& line, uint64_t now) noexcept;
    bool groupStopTerminal(Operation& operation) noexcept;
    bool enqueueGroupStop(const char* text, uint32_t request, uint32_t terminal = 0) noexcept;
    unsigned motionSlot(const ProbeMotion* motion) const noexcept;
    SimpleMoveIntent& motionIntent() noexcept;
    bool enqueueStop(const char* text, ProbeMotion* motion, uint8_t stop, uint32_t request,
                     uint32_t terminal = 0) noexcept;
    void diagnostic(uint64_t now) noexcept;
    void captureDiagnostics() noexcept;
    bool readValues(Operation& operation, uint64_t now) noexcept;
    void listNext() noexcept;
    Operation* operation(uint32_t id) noexcept;
    bool requestInUse(uint32_t id) const noexcept;
};
} // namespace MotorControlCANExample
