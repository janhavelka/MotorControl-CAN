// SPDX-License-Identifier: MIT
#include "PdoWatch.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace MotorControlCANExample {
using MotorControlCANOwner::ReceivedFrame;
namespace {
// Watch-time grid of the periodic lines.
const uint64_t SUM_US = 1000000, ID_US = 10000000;
const uint8_t FLAG_EXTENDED = 1, FLAG_RTR = 2, FLAG_TIMED = 4, FLAG_EXACT = 8;

void add(uint32_t& value, uint32_t amount = 1) noexcept {
    value = amount > UINT32_MAX - value ? UINT32_MAX : value + amount;
}
// First grid tick after now; begin keeps it inside the clock range.
uint64_t nextTick(uint64_t startUs, uint64_t now, uint64_t period) noexcept {
    return startUs + ((now - startUs) / period + 1) * period;
}
uint64_t root(uint64_t value) noexcept {
    uint64_t result = 0, bit = uint64_t(1) << 62;
    while (bit > value)
        bit >>= 2;
    for (; bit; bit >>= 2) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else
            result >>= 1;
    }
    return result;
}
// Population standard deviation from the gap sum and the sum of squared gaps, both 64-bit.
// The squares never exceed the squared sum, so they fit while the sum fits 32 bits; past that
// the result is the documented sentinel. squares >= sum * sum / count, so nothing underflows.
uint32_t deviation(uint64_t sum, uint64_t squares, uint32_t count) noexcept {
    if (!count)
        return 0;
    if (sum > UINT32_MAX)
        return UINT32_MAX;
    return uint32_t(root((squares - sum * sum / count) / count));
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
} // namespace

PdoWatch::PdoWatch(SoakTwaiTap& tap, const PdoWatchOutput& output) noexcept
    : tap_(tap), output_(output) {
    static_assert(unsigned(PDO_WATCH_LINE_BYTES) <= unsigned(ProbePlatform::OUTPUT_CAPACITY),
                  "a watch line must fit the console whole");
    static_assert(unsigned(PDO_WATCH_IDS) <= UINT8_MAX && unsigned(PDO_WATCH_PRINTS) <= UINT8_MAX,
                  "cursors and counts are 8-bit");
    std::memset(static_cast<void*>(&s_), 0, sizeof s_);
    text_[0] = 0;
}

PdoWatch::~PdoWatch() {
    if (s_.phase == PHASE_RUNNING)
        tap_.copyReceives(nullptr);
}

PdoWatchAdmission PdoWatch::begin(uint32_t seconds, uint64_t now) noexcept {
    if (active())
        return PdoWatchAdmission::BUSY;
    // Room for the end and for the next ID tick after it, so no schedule time can wrap.
    if (!seconds || seconds > PDO_WATCH_SECONDS || !output_.write ||
        now > UINT64_MAX - uint64_t(seconds) * 1000000u - ID_US)
        return PdoWatchAdmission::INVALID;
    std::memset(static_cast<void*>(&s_), 0, sizeof s_);
    s_.phase = PHASE_RUNNING;
    s_.seconds = seconds;
    s_.startUs = s_.lastUs = now;
    s_.endUs = now + uint64_t(seconds) * 1000000u;
    s_.nextSumUs = now + SUM_US;
    s_.nextIdUs = now + ID_US;
    s_.beginPending = true;
    tap_.copyReceives(&ring_);
    return PdoWatchAdmission::ACCEPTED;
}

bool PdoWatch::requestStop() noexcept {
    if (s_.phase != PHASE_RUNNING)
        return false;
    s_.stopRequested = true;
    return true;
}

PdoWatch::IdStats* PdoWatch::find(uint32_t id, bool extended) noexcept {
    for (unsigned i = 0; i < s_.ids; ++i)
        if (s_.idStats[i].id == id && s_.idStats[i].extended == extended)
            return &s_.idStats[i];
    if (s_.ids == PDO_WATCH_IDS)
        return nullptr;
    IdStats& stats = s_.idStats[s_.ids++];
    stats.id = id;
    stats.extended = extended;
    return &stats;
}

void PdoWatch::record(const ReceivedFrame& in) noexcept {
    const MotorControlCAN::Frame& frame = in.frame;
    if (!frame.extended && !frame.rtr &&
        ((frame.id >= 0x581 && frame.id <= 0x5ff) || (frame.id >= 0x601 && frame.id <= 0x67f))) {
        add(s_.sdo);
        return;
    }
    add(s_.frames);
    Print print;
    std::memset(static_cast<void*>(&print), 0, sizeof print);
    print.id = frame.id;
    print.dlc = frame.dlc;
    print.length = frame.payloadLength > 8 ? 8 : frame.payloadLength;
    std::memcpy(print.data, frame.data, print.length);
    print.flags = uint8_t((frame.extended ? FLAG_EXTENDED : 0) | (frame.rtr ? FLAG_RTR : 0));
    if (in.receivedTimeKnown) {
        print.flags = uint8_t(print.flags | FLAG_TIMED | FLAG_EXACT);
        print.lowUs = print.highUs = in.receivedUs;
    } else if (in.receiveBoundsKnown) {
        print.flags = uint8_t(print.flags | FLAG_TIMED);
        print.lowUs = in.receivedEarliestUs;
        print.highUs = in.receivedLatestUs;
    } else
        add(s_.untimed);
    IdStats* stats = find(frame.id, frame.extended);
    if (!stats)
        add(s_.idOverflow);
    else {
        if (stats->count && (stats->dlc != frame.dlc || stats->rtr != frame.rtr ||
                             stats->length != print.length ||
                             std::memcmp(stats->data, print.data, print.length) != 0))
            add(stats->changes);
        add(stats->count);
        stats->dlc = frame.dlc;
        stats->rtr = frame.rtr;
        stats->length = print.length;
        std::memcpy(stats->data, print.data, sizeof stats->data);
        const uint64_t time = print.highUs;
        if ((print.flags & FLAG_TIMED) && !stats->timed) {
            stats->timed = true;
            stats->firstUs = stats->lastUs = time;
        } else if ((print.flags & FLAG_TIMED) && time >= stats->lastUs) {
            // Gaps run from the latest time seen, so their sum is exactly last - first. An earlier
            // time adds none; the S2 driver stamps in FIFO order, so that needs another source.
            const uint64_t gap = time - stats->lastUs;
            stats->lastUs = time;
            print.gapUs = gap;
            add(stats->gaps);
            stats->gapSumUs += gap;
            if (stats->gapSumUs <= UINT32_MAX)
                stats->gapSquares += gap * gap;
            if (stats->gaps == 1 || gap < stats->minGapUs)
                stats->minGapUs = gap;
            if (gap > stats->maxGapUs)
                stats->maxGapUs = gap;
        }
    }
    if (s_.printCount == PDO_WATCH_PRINTS) {
        add(s_.unprinted);
        return;
    }
    s_.prints[(s_.printHead + s_.printCount) % PDO_WATCH_PRINTS] = print;
    ++s_.printCount;
}

void PdoWatch::schedule(uint64_t now) noexcept {
    if (now >= s_.nextSumUs) {
        // One pending summary at most: a summary still waiting is not repeated, the next is later.
        s_.sumPending = true;
        s_.nextSumUs = nextTick(s_.startUs, now, SUM_US);
    }
    if (now >= s_.nextIdUs) {
        // A round still printing is not restarted.
        if (!s_.idRound) {
            s_.idRound = true;
            s_.idCursor = 0;
        }
        s_.nextIdUs = nextTick(s_.startUs, now, ID_US);
    }
}

void PdoWatch::finish() noexcept {
    tap_.copyReceives(nullptr);
    // Queued F lines would follow PDOW_END: they are counted instead, so that at the end
    // printed + unprinted is frames.
    add(s_.unprinted, s_.printCount);
    s_.printCount = 0;
    s_.idRound = true; // The final round restarts from the first identifier.
    s_.idCursor = 0;
    s_.endPending = true;
    s_.phase = PHASE_FINAL;
}

bool PdoWatch::select() noexcept {
    if (s_.idRound && s_.idCursor >= s_.ids)
        s_.idRound = false;
    if (s_.beginPending) {
        s_.beginPending = false;
        s_.line = LINE_BEGIN;
    } else if (s_.sumPending) {
        s_.sumPending = false;
        s_.line = LINE_SUM;
    } else if (s_.idRound) {
        s_.line = LINE_ID;
        s_.idIndex = s_.idCursor++;
    } else if (s_.endPending) {
        s_.endPending = false;
        s_.line = LINE_END;
    } else if (s_.printCount) {
        s_.line = LINE_FRAME;
        s_.frame = s_.prints[s_.printHead];
        s_.printHead = uint8_t((s_.printHead + 1) % PDO_WATCH_PRINTS);
        --s_.printCount;
    } else
        return false;
    s_.selected = true;
    return true;
}

void PdoWatch::put(const char* pattern, ...) noexcept {
    va_list arguments;
    va_start(arguments, pattern);
    const int count = std::vsnprintf(text_, sizeof text_, pattern, arguments);
    va_end(arguments);
    if (count < 0 || size_t(count) >= sizeof text_) {
        // A truncated line is failed evidence, never a plausible shorter observation.
        static const char failure[] = "OUTPUT_FORMAT_ERROR; evidence incomplete\n";
        std::memcpy(text_, failure, sizeof failure);
        length_ = sizeof failure - 1;
    } else
        length_ = size_t(count);
}

void PdoWatch::format(uint64_t now) noexcept {
    const unsigned long n = static_cast<unsigned long>(s_.sequence);
    const unsigned long long t = static_cast<unsigned long long>(now);
    char data[17];
    switch (s_.line) {
    case LINE_BEGIN:
        put("PDOW_BEGIN n=%lu t=%llu seconds=%lu ring=%u ids=%u\n", n, t,
            static_cast<unsigned long>(s_.seconds), unsigned(ReceiveRing::CAPACITY),
            unsigned(PDO_WATCH_IDS));
        return;
    case LINE_SUM:
        put("PDOW_SUM n=%lu t=%llu frames=%lu printed=%lu unprinted=%lu tapDropped=%lu untimed=%lu "
            "sdo=%lu idOverflow=%lu ids=%u\n",
            n, t, static_cast<unsigned long>(s_.frames), static_cast<unsigned long>(s_.printed),
            static_cast<unsigned long>(s_.unprinted), static_cast<unsigned long>(ring_.dropped),
            static_cast<unsigned long>(s_.untimed), static_cast<unsigned long>(s_.sdo),
            static_cast<unsigned long>(s_.idOverflow), unsigned(s_.ids));
        return;
    case LINE_ID: {
        const IdStats& s = s_.idStats[s_.idIndex];
        hex(s.data, s.length, data);
        put("PDOW_ID n=%lu t=%llu id=%lx ext=%u count=%lu first=%llu last=%llu minGap=%llu "
            "maxGap=%llu meanGap=%llu sdGap=%lu changes=%lu dlc=%u data=%s\n",
            n, t, static_cast<unsigned long>(s.id), unsigned(s.extended),
            static_cast<unsigned long>(s.count), static_cast<unsigned long long>(s.firstUs),
            static_cast<unsigned long long>(s.lastUs), static_cast<unsigned long long>(s.minGapUs),
            static_cast<unsigned long long>(s.maxGapUs),
            static_cast<unsigned long long>(s.gaps ? s.gapSumUs / s.gaps : 0),
            static_cast<unsigned long>(deviation(s.gapSumUs, s.gapSquares, s.gaps)),
            static_cast<unsigned long>(s.changes), unsigned(s.dlc), data);
        return;
    }
    case LINE_END:
        put("PDOW_END n=%lu t=%llu reason=%s frames=%lu printed=%lu unprinted=%lu tapDropped=%lu "
            "untimed=%lu sdo=%lu idOverflow=%lu\n",
            n, t, s_.stopRequested ? "stop" : "duration", static_cast<unsigned long>(s_.frames),
            static_cast<unsigned long>(s_.printed), static_cast<unsigned long>(s_.unprinted),
            static_cast<unsigned long>(ring_.dropped), static_cast<unsigned long>(s_.untimed),
            static_cast<unsigned long>(s_.sdo), static_cast<unsigned long>(s_.idOverflow));
        return;
    case LINE_FRAME: {
        const Print& f = s_.frame;
        hex(f.data, f.length, data);
        put("PDOW_F n=%lu t=%llu id=%lx ext=%u rtr=%u dlc=%u data=%s timed=%u exact=%u lo=%llu "
            "hi=%llu gap=%llu\n",
            n, t, static_cast<unsigned long>(f.id), unsigned((f.flags & FLAG_EXTENDED) != 0),
            unsigned((f.flags & FLAG_RTR) != 0), unsigned(f.dlc), data,
            unsigned((f.flags & FLAG_TIMED) != 0), unsigned((f.flags & FLAG_EXACT) != 0),
            static_cast<unsigned long long>(f.lowUs), static_cast<unsigned long long>(f.highUs),
            static_cast<unsigned long long>(f.gapUs));
        return;
    }
    }
}

void PdoWatch::emit(uint64_t now) noexcept {
    if (!s_.selected && !select())
        return;
    // Formatted at every attempt, so t is the call that writes the line; n and the selected
    // record stay the same until it is written.
    format(now);
    if (!output_.write(output_.context, text_, length_))
        return;
    s_.selected = false;
    if (s_.sequence != UINT32_MAX)
        ++s_.sequence;
    if (s_.line == LINE_FRAME)
        add(s_.printed);
    else if (s_.line == LINE_END)
        s_.phase = PHASE_IDLE;
}

void PdoWatch::service(uint64_t now, bool holdOutput) noexcept {
    if (s_.phase == PHASE_IDLE || now < s_.lastUs)
        return;
    s_.lastUs = now;
    // Nothing adds copies during this loop, so the bound drains the ring completely.
    for (unsigned i = 0; i < ReceiveRing::CAPACITY && tap_.takeReceived(received_); ++i)
        record(received_);
    if (s_.phase == PHASE_RUNNING) {
        if (s_.stopRequested || now >= s_.endUs)
            finish();
        else
            schedule(now);
    }
    if (!holdOutput)
        emit(now);
}
} // namespace MotorControlCANExample
