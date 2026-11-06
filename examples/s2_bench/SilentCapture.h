/** @file SilentCapture.h
 * @brief One explicit, finite receive capture through the ordinary owner, with transmission closed.
 * SPDX-License-Identifier: MIT
 *
 * Application only: no SDK headers, heap, clock, I/O of its own, retries or recovery. The capture
 * uses the bench's one controller, driver, owner and tap. Received frames reach the owner exactly
 * as before and are dispatched as before; the capture keeps copies taken at the tap
 * (SoakTwaiTap::copyReceives), as the PDO watch does. There is no second receive task, TWAI node
 * or adapter.
 *
 * What it does, in order. Entry is refused unless the controller runs, the owner is neither
 * inhibited nor recovering, no ordinary, urgent or NMT work is queued or active, every configured
 * SDO slot and the NMT handoff have proved local TX retirement (CanBusOwner::localTxRetired,
 * OwnerResources::nmtTxPending), and the tap holds no frame the SDK may still transmit
 * (SoakTwaiTap::sdkRetired). Retirement that is unknown refuses entry. A GPIO change is never
 * taken as proof that queued work disappeared. Then the tap's transmit gate closes, and only then
 * is the transceiver's silent level requested. While the capture runs every path to the SDK is
 * refused at the gate (SDO requests, segments and client aborts, drain markers, best-effort
 * stops, urgent work, NMT); the owner sees each refusal as proved unsent. At the end the copies
 * stop first, then the normal level is requested, and the gate opens only once the owner and tap
 * are again idle and retired, so no work admitted before or during the capture can leave across
 * the boundary. Entry and exit send nothing, resolve no ambiguous SDO channel, release no
 * retained result and replay nothing.
 *
 * Caller contract. The level held before entry must be the normal one: a refused silent request
 * and an orderly end both request it again. The bench admits a capture only after `c` lowered
 * S18 and with no route, soak, record or watch held, because ready() sees the owner, application
 * and tap, not an AxisSession that defers its next admission; such a late admission would meet the
 * closed gate and close unsent, cutting its action short.
 *
 * What it does not do. The controller stays in normal mode: in the pinned SDK the listen-only
 * mode is fixed when a node is created, and the S2 build lacks the SDK's listen-only errata
 * workaround (docs/adapters/twai_sdk.md). So the controller still drives acknowledgements and
 * error flags onto its TX pin. Whether the TJA1051 keeps them off the bus depends on the silent
 * input really reaching level and pin; a successful GPIO call requests a level, it measures
 * nothing. Physical no-ACK and no-TX need an independent observer. A capture that sees no frames
 * proves neither that capture nor that silence works.
 *
 * Times are microseconds of the S2's monotonic clock (esp_timer), as supplied to begin and
 * service and as stamped by IdfTwaiDriver: a frame's hi is the ISR clock sample at enqueue and lo
 * the last check that proved the controller FIFO empty. Neither is a bus timestamp.
 */
#pragma once
#include "Soak.h"

namespace MotorControlCANExample {
/** Fixed sizes of one capture. */
enum {
    CAPTURE_FRAMES = 128,     ///< Most frames one record stores; later frames are counted only.
    CAPTURE_SECONDS = 600,    ///< Longest capture.
    CAPTURE_LINE_BYTES = 320, ///< Line buffer; the longest line with every field at its maximum fits.
    CAPTURE_CONTROL_LINES = 8 ///< Record lines before the first CAP_F line.
};
/** One requested capture; every field is required. */
struct CapturePlan {
    uint32_t id = 0;      ///< Host identity, nonzero and above every earlier accepted capture.
    uint32_t seconds = 0; ///< Duration, 1..CAPTURE_SECONDS.
    uint16_t frames = 0;  ///< Frame capacity of the record, 1..CAPTURE_FRAMES.
};
/** Local start decision; none of these is a device result. */
enum class CaptureAdmission : uint8_t {
    ACCEPTED,   ///< Gate closed, silent level accepted, copies started; nothing was sent.
    BUSY,       ///< A capture runs, or its record is held until release.
    INVALID,    ///< Plan out of range, repeated identity, missing silent input, clock out of range.
    CONTROLLER, ///< Controller not running, owner inhibited, or a recovery pending.
    WORK,       ///< Ordinary, urgent or NMT work queued or active, or a typed read or acquisition active.
    UNRETIRED,  ///< A slot, the NMT handoff or the tap cannot prove local TX retirement.
    SILENT,     ///< The silent-level request was refused; see level() for what followed.
    CLOSED      ///< The gate is still closed after an earlier capture (see SilentCapture::latched).
};
/** @param value Admission result. @return Its console word, such as "work". */
const char* captureAdmissionName(CaptureAdmission value) noexcept;
/** Why a capture ended. */
enum class CaptureEnd : uint8_t {
    NONE,      ///< Still collecting, or never began.
    DURATION,  ///< The stated duration elapsed.
    STOPPED,   ///< Explicit stop request.
    CONTROLLER ///< The firmware stopped running, the owner became inhibited, or a recovery began.
};
/** @param value End reason. @return "none", "duration", "stop" or "controller". */
const char* captureEndName(CaptureEnd value) noexcept;
/** What the end did with the silent input. */
enum class CaptureExit : uint8_t {
    NONE,       ///< Not ended.
    NORMAL,     ///< Normal level requested and accepted; the gate opens once owner and tap are idle.
    FAILED,     ///< Normal-level request refused: level unknown, gate stays closed for this boot.
    LEFT_SILENT ///< Ended by CONTROLLER: no normal level requested, gate stays closed for this boot.
};
/** The silent input this capture drives, application-owned (main.cpp: GPIO18, active high). */
struct CaptureTransceiver {
    int gpio = -1;                               ///< Silent input GPIO.
    SilentLevel level = SilentLevel::NONE;       ///< Which level requests silence.
    SilentPin pin;                               ///< Requests a level; never reads one back.
};
/** Controller error counters, sampled at begin and end only. */
struct CaptureSample {
    bool counters = false; ///< True when the SDK answered; TEC/REC meaningful only then.
    uint32_t tec = 0;      ///< TX error counter.
    uint32_t rec = 0;      ///< RX error counter.
};
/** Optional sampling callback; called at begin and at end, never per turn. */
struct CaptureCallbacks {
    void* context = nullptr;                          ///< Borrowed for this object's lifetime.
    void (*sample)(void*, CaptureSample&) = nullptr;  ///< Fills a zeroed sample; bounded, no CAN.
};
/** Interface facts at one boundary of a capture, copied without I/O except the sample callback. */
struct CaptureEndpoint {
    uint64_t atUs = 0;            ///< Processing time of the boundary.
    uint32_t epoch = 0;           ///< Owner controller epoch.
    CaptureSample sample;         ///< TEC/REC from the callback.
    bool ownerInhibited = false;  ///< OwnerResources::inhibited.
    bool adapterInhibited = false; ///< TwaiDiagnostics::inhibited.
    uint32_t twaiDropped = 0;     ///< TwaiDiagnostics::rxDropped, lifetime.
    uint32_t twaiErrors = 0;      ///< TwaiDiagnostics::busErrors, lifetime.
    uint32_t twaiFlags = 0;       ///< TwaiDiagnostics::errorFlags, lifetime union.
    MotorControlCANOwner::Stats owner; ///< Owner interface totals, lifetime.
    uint32_t tapRefused = 0;      ///< SoakTwaiTap::refused, lifetime.
};
/** Flag bits of a stored frame. */
enum : uint8_t {
    CAPTURE_EXTENDED = 1, ///< 29-bit identifier.
    CAPTURE_RTR = 2,      ///< Remote frame; dlc is the requested length, no payload.
    CAPTURE_BOUNDS = 4,   ///< lowUs..highUs is a receive interval.
    CAPTURE_EXACT = 8,    ///< A driver-supplied exact receive time, lowUs == highUs.
    CAPTURE_PRE = 16,     ///< Received wholly before the begin boundary, copied after it.
    CAPTURE_POST = 32     ///< Received wholly after the nominal end, copied in the end turn.
};
/** One stored copy. Unknown identifiers and formats are kept as diagnostic data. */
struct CapturedFrame {
    uint64_t lowUs;  ///< Receive lower bound, or the exact time; 0 when untimed.
    uint64_t highUs; ///< Receive upper bound, or the exact time; 0 when untimed.
    uint32_t id;     ///< Identifier as received.
    uint32_t epoch;  ///< Controller epoch stamped at ingress.
    uint8_t data[8]; ///< Payload bytes; only dlc of them (none for RTR) are meaningful.
    uint8_t dlc;     ///< DLC as received (normalized to 8 by the driver above 8).
    uint8_t flags;   ///< CAPTURE_* bits.
};
/** Retained record of one capture, stable once terminal except the gate fields noted. */
struct CaptureRecord {
    CapturePlan plan;           ///< As accepted.
    bool terminal = false;      ///< True once ended.
    CaptureEnd end = CaptureEnd::NONE;
    CaptureExit exit = CaptureExit::NONE;
    bool reopened = false;      ///< Gate opened again: in the end turn when the owner was idle,
                                ///< later when work admitted during the capture still had to close.
    uint64_t reopenedUs = 0;    ///< When it opened. A line printed before that shows reopened=0.
    uint64_t beginUs = 0;       ///< Begin boundary: copies cover frames the owner took after it.
    uint64_t nominalEndUs = 0;  ///< beginUs plus the duration.
    uint64_t endUs = 0;         ///< Processing time of the turn that ended the capture.
    uint64_t maxGapUs = 0;      ///< Largest interval between service calls while collecting.
    uint16_t stored = 0;        ///< Frames in frames[].
    uint16_t pre = 0, post = 0, untimed = 0; ///< Stored frames with those flags, and without time.
    uint32_t overflow = 0;      ///< Frames copied after the record was full: counted, not stored.
    uint32_t tapDropped = 0;    ///< Copies the tap ring had no room for: frames never seen here.
    CaptureEndpoint first;      ///< At begin, before the gate closed and the level changed.
    CaptureEndpoint last;       ///< At end, after the copies stopped and before the exit level.
    CapturedFrame frames[CAPTURE_FRAMES]; ///< Oldest first.
};
/** One capture at a time; see the file comment for what it guarantees and what it cannot.
 * The record is held until release and printed with formatLine, one line per call, by the
 * caller. The object is stationary and allocates nothing; the caller places it.
 */
class SilentCapture {
  public:
    /** Borrow stationary resources; construction does nothing else.
     * @param firmware Assembly whose owner, application and adapter the capture checks.
     * @param services That assembly's service store, for the configured SDO slots.
     * @param tap The tap the adapter submits and receives through.
     * @param transceiver Silent input; copied.
     * @param callbacks Optional counter sampling; copied.
     * @param revision Static source revision text, printed in CAP_IMAGE.
     * @param image Static image identity text (the ELF SHA-256), printed in CAP_IMAGE.
     */
    SilentCapture(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services,
                  SoakTwaiTap& tap, const CaptureTransceiver& transceiver,
                  const CaptureCallbacks& callbacks, const char* revision, const char* image) noexcept;
    /** Stops the tap copying into this object if a capture is still collecting. */
    ~SilentCapture();
    SilentCapture(const SilentCapture&) = delete;            ///< The tap borrows this object's ring.
    SilentCapture& operator=(const SilentCapture&) = delete; ///< The tap borrows this object's ring.
    /** Start one capture. Sends nothing, now or later.
     * On ACCEPTED: the gate is closed, the silent level was accepted, the copies run and the
     * previous record is gone. On SILENT the gate closed and the silent request was refused; the
     * normal level was then requested once, and the gate opened again only if that was accepted
     * (level() tells which); no record is kept. Every other refusal changes nothing.
     * @param plan Copied on acceptance.
     * @param nowUs Monotonic microseconds, at least the owner's last time.
     */
    CaptureAdmission begin(const CapturePlan& plan, uint64_t nowUs) noexcept;
    /** One call after each firmware service turn: copy the frames the owner took, end when due,
     * and open the gate once allowed. Never sends, never changes owner or route state.
     * @param nowUs Monotonic microseconds; an earlier value than the last call's is ignored.
     */
    void service(uint64_t nowUs) noexcept;
    /** End early at the next service call, with reason STOPPED.
     * @return False when no capture is collecting; then nothing changes.
     */
    bool requestStop() noexcept;
    /** @return True from an accepted begin until the end turn. */
    bool active() const noexcept { return active_; }
    /** @return The held record, collecting or terminal; null when none is held. */
    const CaptureRecord* record() const noexcept { return held_ ? &r_ : nullptr; }
    /** Release a terminal record. Touches neither the gate nor the silent input.
     * @return False while collecting or with nothing held.
     */
    bool release() noexcept;
    /** @return Last level this object requested: NOT_CONTROLLED before any request. */
    TransceiverState level() const noexcept { return level_; }
    /** @return True when the gate stays closed for this boot (CaptureExit FAILED or LEFT_SILENT,
     * or a refused entry whose normal-level restore was refused too). */
    bool latched() const noexcept { return latched_; }
    /** @return Why bench work must wait: "active", "held", "closed", or null when nothing does. */
    const char* blocking() const noexcept;
    /** Format one record line.
     * @param cursor 0 for CAP_REC, then CAP_TIME, CAP_LEVEL, two CAP_CTRL, two CAP_OWNER and
     * CAP_IMAGE, then one CAP_F per stored frame, then END capture.
     * @param output Buffer, at least CAPTURE_LINE_BYTES for every line to fit.
     * @param size Its size.
     * @return False past the last line or without a terminal record; output untouched then.
     */
    bool formatLine(unsigned cursor, char* output, size_t size) const noexcept;

  private:
    CaptureAdmission ready() const noexcept;
    bool controllerUsable() const noexcept;
    bool request(bool silent) noexcept;
    void snapshot(CaptureEndpoint& output, uint64_t nowUs) const noexcept;
    void copy() noexcept;
    void finish(CaptureEnd reason, uint64_t nowUs) noexcept;
    void reopen(uint64_t nowUs) noexcept;
    FirmwareProbe& firmware_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    SoakTwaiTap& tap_;
    CaptureTransceiver transceiver_;
    CaptureCallbacks callbacks_;
    const char* revision_;
    const char* image_;
    ReceiveRing ring_;
    MotorControlCANOwner::ReceivedFrame received_;
    uint32_t lastId_ = 0;
    uint64_t lastServiceUs_ = 0;
    TransceiverState level_ = TransceiverState::NOT_CONTROLLED;
    bool active_ = false, held_ = false, stopRequested_ = false, reopenPending_ = false;
    bool latched_ = false;
    CaptureRecord r_;
};
} // namespace MotorControlCANExample
