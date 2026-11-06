// SPDX-License-Identifier: MIT
#include "SilentCapture.h"
#include <cstdio>
#include <cstring>

namespace MotorControlCANExample {
using MotorControlCANOwner::OwnerResources;
using MotorControlCANOwner::ReceivedFrame;
namespace {
void add(uint32_t& value, uint32_t amount = 1) noexcept {
    value = amount > UINT32_MAX - value ? UINT32_MAX : value + amount;
}
void increment(uint16_t& value) noexcept {
    if (value != UINT16_MAX)
        ++value;
}
void hex(const uint8_t* data, unsigned length, char (&output)[17]) noexcept {
    static const char digits[] = "0123456789abcdef";
    if (!length) {
        output[0] = '-';
        output[1] = 0;
        return;
    }
    for (unsigned i = 0; i < length; ++i) {
        output[2 * i] = digits[data[i] >> 4];
        output[2 * i + 1] = digits[data[i] & 15];
    }
    output[2 * length] = 0;
}
// A truncated line is failed evidence, never a plausible shorter observation.
bool fits(int count, char* output, size_t size) noexcept {
    if (count >= 0 && size_t(count) < size)
        return true;
    static const char failure[] = "OUTPUT_FORMAT_ERROR; evidence incomplete\n";
    if (size >= sizeof failure)
        std::memcpy(output, failure, sizeof failure);
    else if (size)
        output[0] = 0;
    return true; // The cursor still advances: the record line exists and was reported as damaged.
}
} // namespace

const char* captureAdmissionName(CaptureAdmission value) noexcept {
    static const char* const names[] = {"accepted", "busy", "invalid", "controller",
                                        "work", "unretired", "silent", "closed"};
    return unsigned(value) < sizeof names / sizeof names[0] ? names[unsigned(value)] : "unknown";
}
const char* captureEndName(CaptureEnd value) noexcept {
    static const char* const names[] = {"none", "duration", "stop", "controller"};
    return unsigned(value) < sizeof names / sizeof names[0] ? names[unsigned(value)] : "unknown";
}

SilentCapture::SilentCapture(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services,
                             SoakTwaiTap& tap, const CaptureTransceiver& transceiver,
                             const CaptureCallbacks& callbacks, const char* revision,
                             const char* image) noexcept
    : firmware_(firmware), services_(services), tap_(tap), transceiver_(transceiver),
      callbacks_(callbacks), revision_(revision ? revision : ""), image_(image ? image : "") {
    static_assert(unsigned(CAPTURE_LINE_BYTES) <= unsigned(ProbePlatform::OUTPUT_CAPACITY),
                  "a capture line must fit the console whole");
    static_assert(unsigned(CAPTURE_FRAMES) <= UINT16_MAX, "stored counts are 16-bit");
    std::memset(static_cast<void*>(&r_), 0, sizeof r_);
}

SilentCapture::~SilentCapture() {
    if (active_)
        tap_.copyReceives(nullptr);
}

bool SilentCapture::controllerUsable() const noexcept {
    const OwnerResources resources = firmware_.owner().resources();
    // The adapter's own inhibit is reported to the owner on every path today; checking it too keeps
    // a capture from running with its receive path closed if such a report were ever refused.
    return firmware_.running() && !resources.inhibited && !resources.recoveryPending &&
           !firmware_.owner().recoveryActive() && !firmware_.transport().diagnostics().inhibited;
}

// Entry, and the gate's reopening, need the same facts: nothing queued or active anywhere, and
// established local retirement of every frame. Unknown retirement refuses.
CaptureAdmission SilentCapture::ready() const noexcept {
    if (!controllerUsable())
        return CaptureAdmission::CONTROLLER;
    const MotorControlCANOwner::CanBusOwner& owner = firmware_.owner();
    const OwnerResources resources = owner.resources();
    const DeviceEvidence& evidence = firmware_.evidence();
    if (resources.queued || resources.active || resources.nmtPending ||
        firmware_.app().readContext().state == MotorControlCAN::ReadState::ACTIVE ||
        firmware_.app().pendingRequest().owner || (evidence.record() && !evidence.record()->terminal))
        return CaptureAdmission::WORK;
    if (resources.nmtTxPending || !tap_.sdkRetired())
        return CaptureAdmission::UNRETIRED;
    for (uint8_t slot = 0; slot < MotorControlCAN::CANOPEN::SdoClient::CHANNELS; ++slot)
        // An unconfigured slot has no staging port, so no frame of it can exist. A configured
        // slot must prove retirement, including after an early reply whose TX report is pending.
        if (services_.channel(slot) && !owner.localTxRetired(slot))
            return CaptureAdmission::UNRETIRED;
    return CaptureAdmission::ACCEPTED;
}

bool SilentCapture::request(bool silent) noexcept {
    const bool high = (transceiver_.level == SilentLevel::HIGH) == silent;
    const bool accepted = transceiver_.pin.drive(transceiver_.pin.context, transceiver_.gpio, high);
    // A refused call leaves the level unknown, never the previous one.
    level_ = !accepted ? TransceiverState::FAILED
             : silent  ? TransceiverState::SILENT_REQUESTED
                       : TransceiverState::NORMAL_REQUESTED;
    return accepted;
}

void SilentCapture::snapshot(CaptureEndpoint& output, uint64_t now) const noexcept {
    std::memset(static_cast<void*>(&output), 0, sizeof output);
    output.atUs = now;
    output.epoch = firmware_.owner().controllerEpoch();
    if (callbacks_.sample)
        callbacks_.sample(callbacks_.context, output.sample);
    output.ownerInhibited = firmware_.owner().resources().inhibited;
    const TwaiDiagnostics diagnostics = firmware_.transport().diagnostics();
    output.adapterInhibited = diagnostics.inhibited;
    output.twaiDropped = diagnostics.rxDropped;
    output.twaiErrors = diagnostics.busErrors;
    output.twaiFlags = diagnostics.errorFlags;
    output.owner = firmware_.owner().stats();
    output.tapRefused = tap_.refused();
}

CaptureAdmission SilentCapture::begin(const CapturePlan& plan, uint64_t now) noexcept {
    if (active_ || held_)
        return CaptureAdmission::BUSY;
    if (!plan.id || plan.id <= lastId_ || !plan.seconds || plan.seconds > CAPTURE_SECONDS ||
        !plan.frames || plan.frames > CAPTURE_FRAMES || transceiver_.gpio < 0 ||
        transceiver_.level == SilentLevel::NONE || !transceiver_.pin.drive ||
        now < firmware_.owner().lastTimeUs() || now > UINT64_MAX - uint64_t(plan.seconds) * 1000000u)
        return CaptureAdmission::INVALID;
    if (latched_ || tap_.transmitClosed())
        return CaptureAdmission::CLOSED;
    const CaptureAdmission state = ready();
    if (state != CaptureAdmission::ACCEPTED)
        return state;
    // Interface facts before anything changes; the gate before the level, so nothing can be
    // handed to the SDK once silence has been requested.
    CaptureEndpoint first;
    snapshot(first, now);
    tap_.closeTransmit();
    if (!request(true)) {
        // The level is unknown. Request the level that held before entry once; only if that is
        // accepted is the gate opened again. Otherwise transmission stays closed for this boot.
        if (request(false))
            tap_.openTransmit();
        else
            latched_ = true;
        return CaptureAdmission::SILENT;
    }
    std::memset(static_cast<void*>(&r_), 0, sizeof r_);
    r_.plan = plan;
    r_.beginUs = lastServiceUs_ = now;
    r_.nominalEndUs = now + uint64_t(plan.seconds) * 1000000u;
    r_.first = first;
    lastId_ = plan.id;
    stopRequested_ = reopenPending_ = false;
    active_ = held_ = true;
    tap_.copyReceives(&ring_);
    return CaptureAdmission::ACCEPTED;
}

bool SilentCapture::requestStop() noexcept {
    if (!active_)
        return false;
    stopRequested_ = true;
    return true;
}

bool SilentCapture::release() noexcept {
    if (active_ || !held_)
        return false;
    held_ = false;
    return true;
}

const char* SilentCapture::blocking() const noexcept {
    return active_ ? "active" : held_ ? "held" : tap_.transmitClosed() || latched_ ? "closed" : nullptr;
}

void SilentCapture::copy() noexcept {
    // Nothing adds copies during this loop, so the bound drains the ring completely.
    for (unsigned i = 0; i < ReceiveRing::CAPACITY && tap_.takeReceived(received_); ++i) {
        if (r_.stored >= r_.plan.frames) {
            add(r_.overflow);
            continue;
        }
        CapturedFrame& out = r_.frames[r_.stored++];
        std::memset(static_cast<void*>(&out), 0, sizeof out);
        const MotorControlCAN::Frame& f = received_.frame;
        out.id = f.id;
        out.epoch = received_.controllerEpoch;
        out.dlc = f.dlc;
        std::memcpy(out.data, f.data, f.payloadLength > 8 ? 8 : f.payloadLength);
        out.flags = uint8_t((f.extended ? CAPTURE_EXTENDED : 0) | (f.rtr ? CAPTURE_RTR : 0));
        if (received_.receivedTimeKnown) {
            out.flags = uint8_t(out.flags | CAPTURE_EXACT);
            out.lowUs = out.highUs = received_.receivedUs;
        } else if (received_.receiveBoundsKnown) {
            out.flags = uint8_t(out.flags | CAPTURE_BOUNDS);
            out.lowUs = received_.receivedEarliestUs;
            out.highUs = received_.receivedLatestUs;
        } else {
            increment(r_.untimed);
            continue;
        }
        // A frame still in the driver FIFO at begin is copied after it; one that arrived after
        // the nominal end can be copied in the end turn. Both stay, marked.
        if (out.highUs < r_.beginUs) {
            out.flags = uint8_t(out.flags | CAPTURE_PRE);
            increment(r_.pre);
        } else if (out.lowUs > r_.nominalEndUs) {
            out.flags = uint8_t(out.flags | CAPTURE_POST);
            increment(r_.post);
        }
    }
    r_.tapDropped = ring_.dropped;
}

void SilentCapture::finish(CaptureEnd reason, uint64_t now) noexcept {
    // Copies stop first: nothing taken after this turn belongs to the record.
    tap_.copyReceives(nullptr);
    active_ = false;
    r_.end = reason;
    r_.endUs = now;
    snapshot(r_.last, now);
    r_.terminal = true;
    // The normal level only on an orderly end with a usable controller. A failed or interrupted
    // controller keeps silence requested, as every FirmwareProbe failure path does.
    if (reason != CaptureEnd::CONTROLLER && controllerUsable()) {
        if (request(false)) {
            r_.exit = CaptureExit::NORMAL;
            reopenPending_ = true;
            reopen(now);
        } else {
            r_.exit = CaptureExit::FAILED;
            latched_ = true;
        }
    } else {
        r_.exit = CaptureExit::LEFT_SILENT;
        latched_ = true;
    }
}

void SilentCapture::reopen(uint64_t now) noexcept {
    // Work admitted before entry was refused there; work admitted during the capture meets the
    // closed gate and closes unsent. Until the owner and tap prove idle again, it stays closed.
    if (!reopenPending_ || latched_ || level_ != TransceiverState::NORMAL_REQUESTED ||
        ready() != CaptureAdmission::ACCEPTED)
        return;
    tap_.openTransmit();
    reopenPending_ = false;
    if (held_) {
        r_.reopened = true;
        r_.reopenedUs = now;
    }
}

void SilentCapture::service(uint64_t now) noexcept {
    if (active_ && now < lastServiceUs_ && !controllerUsable()) {
        // A failed clock freezes the owner's time below the begin time the platform supplied; the
        // failed controller must still end the capture, at the last time it accepted.
        copy();
        finish(CaptureEnd::CONTROLLER, lastServiceUs_);
        return;
    }
    if (now < lastServiceUs_)
        return;
    if (active_ && now - lastServiceUs_ > r_.maxGapUs)
        r_.maxGapUs = now - lastServiceUs_;
    lastServiceUs_ = now;
    if (active_) {
        copy();
        if (!controllerUsable())
            finish(CaptureEnd::CONTROLLER, now);
        else if (stopRequested_)
            finish(CaptureEnd::STOPPED, now);
        else if (now >= r_.nominalEndUs)
            finish(CaptureEnd::DURATION, now);
        return;
    }
    reopen(now);
}

bool SilentCapture::formatLine(unsigned cursor, char* out, size_t size) const noexcept {
    if (!held_ || !r_.terminal || !out || !size)
        return false;
    const unsigned long id = static_cast<unsigned long>(r_.plan.id);
    typedef unsigned long long U64;
    typedef unsigned long U32;
    const bool lost = r_.tapDropped || r_.last.twaiDropped != r_.first.twaiDropped ||
                      r_.last.owner.rxLost != r_.first.owner.rxLost;
    int count = 0;
    if (cursor == 0) {
        count = std::snprintf(
            out, size,
            "CAP_REC cap=%lu reason=%s seconds=%lu capacity=%u stored=%u overflow=%lu tapDropped=%lu "
            "pre=%u post=%u untimed=%u txRefused=%lu lost=%u\n",
            id, captureEndName(r_.end), U32(r_.plan.seconds), unsigned(r_.plan.frames),
            unsigned(r_.stored), U32(r_.overflow), U32(r_.tapDropped), unsigned(r_.pre),
            unsigned(r_.post), unsigned(r_.untimed), U32(r_.last.tapRefused - r_.first.tapRefused),
            unsigned(lost));
    } else if (cursor == 1) {
        count = std::snprintf(out, size,
                              "CAP_TIME cap=%lu begin=%llu end=%llu nominal=%llu gap=%llu "
                              "clock=esp_timer_us\n",
                              id, U64(r_.beginUs), U64(r_.endUs), U64(r_.nominalEndUs), U64(r_.maxGapUs));
    } else if (cursor == 2) {
        // begin is always 1 in a record: a refused silent request leaves none.
        count = std::snprintf(out, size,
                              "CAP_LEVEL cap=%lu gpio=%d silentHigh=%u begin=1 exit=%u reopened=%u at=%llu "
                              "latched=%u\n",
                              id, transceiver_.gpio, unsigned(transceiver_.level == SilentLevel::HIGH),
                              unsigned(r_.exit), unsigned(r_.reopened), U64(r_.reopenedUs),
                              unsigned(latched_));
    } else if (cursor == 3 || cursor == 4) {
        const CaptureEndpoint& e = cursor == 3 ? r_.first : r_.last;
        count = std::snprintf(out, size,
                              "CAP_CTRL cap=%lu which=%u t=%llu epoch=%lu counters=%u tec=%lu rec=%lu "
                              "inhibited=%u adapterInhibited=%u twaiDropped=%lu twaiErrors=%lu "
                              "twaiFlags=%08lx\n",
                              id, cursor - 3, U64(e.atUs), U32(e.epoch), unsigned(e.sample.counters),
                              U32(e.sample.tec), U32(e.sample.rec), unsigned(e.ownerInhibited),
                              unsigned(e.adapterInhibited), U32(e.twaiDropped), U32(e.twaiErrors),
                              U32(e.twaiFlags));
    } else if (cursor == 5 || cursor == 6) {
        const CaptureEndpoint& e = cursor == 5 ? r_.first : r_.last;
        const MotorControlCANOwner::Stats& s = e.owner;
        count = std::snprintf(out, size,
                              "CAP_OWNER cap=%lu which=%u rx=%lu dropped=%lu lost=%lu unmatched=%lu tx=%lu "
                              "txRej=%lu busOff=%lu refused=%lu\n",
                              id, cursor - 5, U32(s.rxFrames), U32(s.rxDropped), U32(s.rxLost),
                              U32(s.unmatchedFrames), U32(s.txFrames), U32(s.txRejected),
                              U32(s.busOffEvents), U32(e.tapRefused));
    } else if (cursor == 7) {
        // Whole strings: a longer identity than expected fails the line, it is never cut short.
        count = std::snprintf(out, size, "CAP_IMAGE cap=%lu base=%s elf=%s\n", id, revision_, image_);
    } else if (cursor < CAPTURE_CONTROL_LINES + unsigned(r_.stored)) {
        const unsigned index = cursor - CAPTURE_CONTROL_LINES;
        const CapturedFrame& f = r_.frames[index];
        char data[17];
        hex(f.data, (f.flags & CAPTURE_RTR) ? 0 : (f.dlc > 8 ? 8 : f.dlc), data);
        count = std::snprintf(out, size,
                              "CAP_F cap=%lu i=%u id=%lx ext=%u rtr=%u dlc=%u data=%s bounds=%u exact=%u "
                              "lo=%llu hi=%llu epoch=%lu pre=%u post=%u\n",
                              id, index, U32(f.id), unsigned((f.flags & CAPTURE_EXTENDED) != 0),
                              unsigned((f.flags & CAPTURE_RTR) != 0), unsigned(f.dlc), data,
                              unsigned((f.flags & CAPTURE_BOUNDS) != 0),
                              unsigned((f.flags & CAPTURE_EXACT) != 0), U64(f.lowUs), U64(f.highUs),
                              U32(f.epoch), unsigned((f.flags & CAPTURE_PRE) != 0),
                              unsigned((f.flags & CAPTURE_POST) != 0));
    } else if (cursor == CAPTURE_CONTROL_LINES + unsigned(r_.stored)) {
        count = std::snprintf(out, size, "END capture cap=%lu stored=%u overflow=%lu lost=%u; r=release\n", id,
                              unsigned(r_.stored), U32(r_.overflow), unsigned(lost));
    } else {
        return false;
    }
    return fits(count, out, size);
}
} // namespace MotorControlCANExample
