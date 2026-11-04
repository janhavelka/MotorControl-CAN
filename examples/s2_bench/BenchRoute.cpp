// SPDX-License-Identifier: MIT
#include "BenchRoute.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

#if defined(__GNUC__)
#define ROUTE_PRINTF __attribute__((format(printf, 3, 4)))
#else
#define ROUTE_PRINTF
#endif

namespace MotorControlCANExample {
using MotorControlCANOwner::BusAdmission;
using MotorControlCANOwner::Completion;
using MotorControlCANOwner::Diagnostic;
using MotorControlCANOwner::DiagnosticKind;
using MotorControlCANOwner::Sequence;
using MotorControlCANOwner::SequenceId;
using MotorControlCANOwner::TransportTxId;
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
// StateBlock bits: ERRORS 0, ERROR_HISTORY 1, MONITORS 2, STATUS 3, FEEDBACK 4.
constexpr uint8_t FULL = 0x1d;  // 1001, 603F, 2100..2102, 6041, 6061, 6064, 606C.
constexpr uint8_t IMAGE = 0x1c; // The DEC-1 accepted image: 2100..2102, 6041, 6061, 6064, 606C.
constexpr uint8_t STOP_STATE = 0x0c; // MONITORS and STATUS, what requestStop judges.
constexpr uint8_t ERROR_STATE = 0x01;   // 1001 and 603F.
constexpr uint8_t HISTORY_STATE = 0x02; // 1003:00 and only the entries its count names.
constexpr uint8_t INPUTS_STATE = 0x24;  // MONITORS (2100..2102) and INPUTS (60FD).
constexpr uint8_t ALARM_STATE = 0x0f;   // ERRORS, ERROR_HISTORY, MONITORS, STATUS: requestClearAlarm's read.
static_assert(FULL == (1u << unsigned(L::StateBlock::ERRORS) | 1u << unsigned(L::StateBlock::MONITORS) |
                       1u << unsigned(L::StateBlock::STATUS) | 1u << unsigned(L::StateBlock::FEEDBACK)),
              "FULL is ERRORS, MONITORS, STATUS and FEEDBACK");
static_assert(IMAGE == (FULL & ~(1u << unsigned(L::StateBlock::ERRORS))), "IMAGE drops ERRORS");
static_assert(ERROR_STATE == 1u << unsigned(L::StateBlock::ERRORS) &&
                  HISTORY_STATE == 1u << unsigned(L::StateBlock::ERROR_HISTORY),
              "ERROR_STATE and HISTORY_STATE are single blocks");
static_assert(INPUTS_STATE == (1u << unsigned(L::StateBlock::MONITORS) | 1u << unsigned(L::StateBlock::INPUTS)),
              "INPUTS_STATE is MONITORS and INPUTS");
// The settings prerequisites, the prior group and then the state, must both be younger than this when
// the candidate is prepared and published: fourteen exchanges at the route budgets.
constexpr uint64_t SETTINGS_AGE_US = 14 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
// IO_DI3 changes DI3: 2400:05 is its function (group field 5), and bit 3 of 2400:01 its polarity.
constexpr unsigned DI3 = 3;
// 1003:00 is one byte and each entry four, as the typed read declares them.
static_assert(L::OBJECT_1003_00.widthBytes == 1 && L::OBJECT_1003_01.widthBytes == 4,
              "history widths follow the generated declarations");
enum ReadMode : uint8_t { READ_STATE, READ_GROUP, READ_TUNING, READ_SINGLE, READ_IDENTITY };
// Typed reads whose context is the plain ReadContext; the others run through a DriverContext.
bool plainRead(uint8_t mode) { return mode == READ_STATE || mode == READ_IDENTITY; }
// Single granted uploads with no typed read. Widths: EDS 1017 U16 and 607A I32; 60FF has no
// generated declaration because its I32 and U32 claims conflict, and both are four bytes.
struct Single {
    uint16_t index;
    uint8_t width;
};
const Single singles[] = {{0x1017, 2}, {0x607a, 4}, {0x60ff, 4}};
static_assert(L::OBJECT_1017_00.widthBytes == 2 && L::OBJECT_607A_00.widthBytes == 4,
              "single upload widths follow the generated declarations");
// POSITION writes in MotionPreview's reviewed order: 607A, 6081, 6083, 6084, then the 001F trigger.
static_assert(L::OBJECT_607A_00.widthBytes == 4 && L::OBJECT_607A_00.signedValue &&
                  L::OBJECT_6081_00.widthBytes == 4 && L::OBJECT_6083_00.widthBytes == 4 &&
                  L::OBJECT_6084_00.widthBytes == 4 && L::OBJECT_6040_00.widthBytes == 2,
              "position write widths follow the generated declarations");
// REARM, RELEASED_QUICK and WORD67 write 6040 like the POSITION trigger, one word per step: REARM the
// word the move lifecycle uses as its rearm, RELEASED_QUICK the quick stop word, WORD67 the enable
// sequence's first two words. Each is its own operation under its own grant.
static_assert(L::OBJECT_6040_00.widthBytes == 2 && !L::OBJECT_6040_00.signedValue,
              "word write width follows the generated declaration");
enum Word : uint8_t { WORD_REARM, WORD_QUICK, WORD_SIX, WORD_SEVEN };
constexpr uint16_t WORDS[] = {0x000f, 0x000b, 0x0006, 0x0007};
// STEP_OBSERVE's argument: every planned cycle, or WORD67's cycles after its 0006 only.
enum Observe : uint8_t { OBSERVE_ALL, OBSERVE_WORD_SIX };
// MOVE takes these enumerations as numbers from the host line.
static_assert(unsigned(L::MoveSignalSource::STATUSWORD) == 1 && unsigned(L::MoveSignalSource::MONITOR) == 2 &&
                  unsigned(L::CompletionRule::AFTER_ABSENCE) == 1 &&
                  unsigned(L::CompletionRule::AFTER_ACCEPTANCE) == 2,
              "the host line numbers MoveSignalSource and CompletionRule as declared");
const char* const names[] = {"none",   "actions", "io",       "tuning", "mode1",
                             "enable", "stop",    "position", "rearm",  "move", "history",
                             "released-quick", "word67", "relative", "settings-2202", "io-di3",
                             "counter-zero", "clear-alarm"};
static_assert(sizeof names / sizeof names[0] == unsigned(RouteKind::CLEAR_ALARM) + 1, "one name per route");
// MODE_ONE..MOVE and RELEASED_QUICK..RELATIVE: READ_HISTORY is a read, and the settings routes after
// RELATIVE change drive state without motion and have caps of their own.
// COUNTER_ZERO moves the position reference and CLEAR_ALARM may let a drive act again: motion class too.
bool isMotion(RouteKind kind) {
    return (kind >= RouteKind::MODE_ONE && kind <= RouteKind::MOVE) ||
           (kind >= RouteKind::RELEASED_QUICK && kind <= RouteKind::RELATIVE) || kind == RouteKind::COUNTER_ZERO ||
           kind == RouteKind::CLEAR_ALARM;
}
bool isClear(RouteKind kind) { return kind == RouteKind::COUNTER_ZERO || kind == RouteKind::CLEAR_ALARM; }
bool isSettings(RouteKind kind) { return kind == RouteKind::SETTINGS_2202 || kind == RouteKind::IO_DI3; }
// The two isolated experiments that write 607A, 6081, 6083, 6084 and a trigger themselves.
bool writesPosition(RouteKind kind) { return kind == RouteKind::POSITION || kind == RouteKind::RELATIVE; }
// Routes that may plan a stop of their own, counted against STOP as well.
bool plansStop(const RoutePlan& p) {
    return (writesPosition(p.kind) || p.kind == RouteKind::MOVE) && p.stopKind;
}
// A truncated line is failed evidence, never a plausible shortened observation.
bool put(char* output, size_t size, const char* pattern, ...) ROUTE_PRINTF;
bool put(char* output, size_t size, const char* pattern, ...) {
    va_list arguments;
    va_start(arguments, pattern);
    const int count = std::vsnprintf(output, size, pattern, arguments);
    va_end(arguments);
    if (count < 0 || size_t(count) >= size) {
        static const char failure[] = "OUTPUT_FORMAT_ERROR; evidence incomplete\n";
        if (size >= sizeof failure)
            std::memcpy(output, failure, sizeof failure);
        else if (size)
            output[0] = 0;
    }
    return true;
}
// One request field: where it lives in RoutePlan, its width and its accepted range. A negative
// lower bound is the only thing that admits a leading minus, and it makes a four-byte field signed.
struct Field {
    uint16_t offset;
    uint8_t width;
    int64_t low, high;
};
struct Range {
    int64_t low, high;
};
#define ROUTE_FIELD(member, range)                                                                  \
    {                                                                                               \
        uint16_t(offsetof(RoutePlan, member)),                                                      \
            uint8_t(sizeof(static_cast<RoutePlan*>(nullptr)->member)), range.low, range.high        \
    }
constexpr Range NODE = {1, 2}, TRIAL = {1, UINT32_MAX}, FLAG = {0, 1}, SIGNED32 = {INT32_MIN, INT32_MAX};
constexpr Range ACTION_MS = {1, 55000}, OPTIONAL_MS = {0, 55000}, ACTION_INTERVAL = {10, 10000};
constexpr Range U16 = {0, UINT16_MAX}, MASK = {1, UINT16_MAX}, U32 = {0, UINT32_MAX}, SOURCE = {1, 2};
constexpr Range TARGET = {-1000000, 1000000}, CYCLE_MS = {20, 5000}, STOP_KIND = {0, 2};
constexpr Range STOP_AFTER = {0, 16}, STOP_INTERVAL = {0, 10000}, MODE_CYCLES = {1, 10};
constexpr Range MODE_INTERVAL = {100, 5000}, CYCLES = {1, 16}, START = {1, 1}, SHAFT = {1, 2}, KIND = {1, 2};
constexpr Range WORD_CYCLES = {1, 4}, PREDECESSOR = {1, 3}, RELATIVE_BASE = {0, 2};
// 2202 codes 0 and 1 only (EN86/ZH86 p18); the EDS default 10 is not a documented code. DI3 cases 1..4.
constexpr Range LIMIT_CODE = {0, 1}, DI3_CASE = {1, 4}, OBSERVER = {0, 2};
// Documentary native ranges (EN86/ZH86 p11). A move admits what prepareMoveAbsolute does (Q11).
constexpr Range RPM = {0, 3000}, RAMP_MS = {0, 2000}, MOVE_RPM = {1, 3000}, MOVE_RAMP_MS = {1, 2000};
// One fixed field list per route letter, in flash and in line order. The parser stores each field
// through it and routePlanValid reads the same fields back, so both judge the same ranges.
constexpr Field READ_FIELDS[] = {ROUTE_FIELD(node, NODE), ROUTE_FIELD(trial, TRIAL)};
constexpr Field MODE_FIELDS[] = {ROUTE_FIELD(node, NODE), ROUTE_FIELD(trial, TRIAL), ROUTE_FIELD(attended, FLAG),
                                 ROUTE_FIELD(cycles, MODE_CYCLES), ROUTE_FIELD(intervalMs, MODE_INTERVAL)};
constexpr Field ENABLE_FIELDS[] = {ROUTE_FIELD(node, NODE),
                                   ROUTE_FIELD(trial, TRIAL),
                                   ROUTE_FIELD(attended, FLAG),
                                   ROUTE_FIELD(start, START),
                                   ROUTE_FIELD(shaft, SHAFT),
                                   ROUTE_FIELD(reviewedStatus, U16),
                                   ROUTE_FIELD(reviewedPositionTarget, SIGNED32),
                                   ROUTE_FIELD(reviewedVelocityTarget, SIGNED32),
                                   ROUTE_FIELD(deadlineMs, ACTION_MS),
                                   ROUTE_FIELD(intervalMs, ACTION_INTERVAL)};
constexpr Field STOP_FIELDS[] = {ROUTE_FIELD(node, NODE),           ROUTE_FIELD(trial, TRIAL),
                                 ROUTE_FIELD(attended, FLAG),       ROUTE_FIELD(stopKind, KIND),
                                 ROUTE_FIELD(deadlineMs, ACTION_MS), ROUTE_FIELD(bestEffortMs, OPTIONAL_MS),
                                 ROUTE_FIELD(intervalMs, ACTION_INTERVAL)};
constexpr Field POSITION_FIELDS[] = {ROUTE_FIELD(node, NODE),
                                     ROUTE_FIELD(trial, TRIAL),
                                     ROUTE_FIELD(attended, FLAG),
                                     ROUTE_FIELD(startPosition, SIGNED32),
                                     ROUTE_FIELD(target, TARGET),
                                     ROUTE_FIELD(rpm, RPM),
                                     ROUTE_FIELD(rampUpMs, RAMP_MS),
                                     ROUTE_FIELD(rampDownMs, RAMP_MS),
                                     ROUTE_FIELD(cycles, CYCLES),
                                     ROUTE_FIELD(intervalMs, CYCLE_MS),
                                     ROUTE_FIELD(stopKind, STOP_KIND),
                                     ROUTE_FIELD(stopAfter, STOP_AFTER),
                                     ROUTE_FIELD(stopDeadlineMs, OPTIONAL_MS),
                                     ROUTE_FIELD(bestEffortMs, OPTIONAL_MS),
                                     ROUTE_FIELD(stopIntervalMs, STOP_INTERVAL)};
constexpr Field REARM_FIELDS[] = {ROUTE_FIELD(node, NODE),         ROUTE_FIELD(trial, TRIAL),
                                  ROUTE_FIELD(attended, FLAG),     ROUTE_FIELD(predecessor, PREDECESSOR),
                                  ROUTE_FIELD(cycles, CYCLES),     ROUTE_FIELD(intervalMs, CYCLE_MS)};
constexpr Field QUICK_FIELDS[] = {ROUTE_FIELD(node, NODE), ROUTE_FIELD(trial, TRIAL), ROUTE_FIELD(attended, FLAG),
                                  ROUTE_FIELD(cycles, CYCLES), ROUTE_FIELD(intervalMs, CYCLE_MS)};
constexpr Field WORD_FIELDS[] = {ROUTE_FIELD(node, NODE),          ROUTE_FIELD(trial, TRIAL),
                                 ROUTE_FIELD(attended, FLAG),      ROUTE_FIELD(wordCycles, WORD_CYCLES),
                                 ROUTE_FIELD(cycles, CYCLES),      ROUTE_FIELD(intervalMs, CYCLE_MS)};
constexpr Field LIMIT_FIELDS[] = {ROUTE_FIELD(node, NODE), ROUTE_FIELD(trial, TRIAL), ROUTE_FIELD(attended, FLAG),
                                  ROUTE_FIELD(code, LIMIT_CODE)};
constexpr Field DI3_FIELDS[] = {ROUTE_FIELD(node, NODE), ROUTE_FIELD(trial, TRIAL), ROUTE_FIELD(attended, FLAG),
                                ROUTE_FIELD(code, DI3_CASE)};
constexpr Field CLEAR_FIELDS[] = {ROUTE_FIELD(node, NODE), ROUTE_FIELD(trial, TRIAL), ROUTE_FIELD(attended, FLAG),
                                  ROUTE_FIELD(deadlineMs, ACTION_MS), ROUTE_FIELD(intervalMs, ACTION_INTERVAL)};
constexpr Field MOVE_FIELDS[] = {ROUTE_FIELD(node, NODE),
                                 ROUTE_FIELD(trial, TRIAL),
                                 ROUTE_FIELD(attended, FLAG),
                                 ROUTE_FIELD(startPosition, SIGNED32),
                                 ROUTE_FIELD(target, TARGET),
                                 ROUTE_FIELD(rpm, MOVE_RPM),
                                 ROUTE_FIELD(rampUpMs, MOVE_RAMP_MS),
                                 ROUTE_FIELD(rampDownMs, MOVE_RAMP_MS),
                                 ROUTE_FIELD(deadlineMs, ACTION_MS),
                                 ROUTE_FIELD(intervalMs, CYCLE_MS),
                                 ROUTE_FIELD(record, TRIAL),
                                 ROUTE_FIELD(acceptSource, SOURCE),
                                 ROUTE_FIELD(acceptMask, MASK),
                                 ROUTE_FIELD(acceptValue, U16),
                                 ROUTE_FIELD(completeSource, SOURCE),
                                 ROUTE_FIELD(completeMask, MASK),
                                 ROUTE_FIELD(completeValue, U16),
                                 ROUTE_FIELD(rule, SOURCE),
                                 ROUTE_FIELD(firmware.deviceType, U32),
                                 ROUTE_FIELD(firmware.hardwareVersion, U16),
                                 ROUTE_FIELD(firmware.softwareVersion, U16),
                                 ROUTE_FIELD(firmware.vendorId, U32),
                                 ROUTE_FIELD(firmware.productCode, U32),
                                 ROUTE_FIELD(firmware.revision, U32),
                                 ROUTE_FIELD(firmware.modelCode, U16),
                                 ROUTE_FIELD(firmware.firmwareCode, U16),
                                 ROUTE_FIELD(relativeBase, RELATIVE_BASE),
                                 ROUTE_FIELD(base, SIGNED32),
                                 ROUTE_FIELD(observer, OBSERVER),
                                 ROUTE_FIELD(stopKind, STOP_KIND),
                                 ROUTE_FIELD(stopAfter, STOP_AFTER),
                                 ROUTE_FIELD(stopDeadlineMs, OPTIONAL_MS),
                                 ROUTE_FIELD(bestEffortMs, OPTIONAL_MS),
                                 ROUTE_FIELD(stopIntervalMs, STOP_INTERVAL)};
#undef ROUTE_FIELD
struct Grammar {
    char letter;
    RouteKind kind;
    const Field* fields;
    uint8_t count;
};
template <size_t N> constexpr uint8_t countOf(const Field (&)[N]) { return uint8_t(N); }
constexpr Grammar GRAMMARS[] = {{'A', RouteKind::READ_ACTIONS, READ_FIELDS, countOf(READ_FIELDS)},
                                {'I', RouteKind::READ_IO, READ_FIELDS, countOf(READ_FIELDS)},
                                {'U', RouteKind::READ_TUNING, READ_FIELDS, countOf(READ_FIELDS)},
                                {'M', RouteKind::MODE_ONE, MODE_FIELDS, countOf(MODE_FIELDS)},
                                {'E', RouteKind::ENABLE, ENABLE_FIELDS, countOf(ENABLE_FIELDS)},
                                {'S', RouteKind::STOP, STOP_FIELDS, countOf(STOP_FIELDS)},
                                {'P', RouteKind::POSITION, POSITION_FIELDS, countOf(POSITION_FIELDS)},
                                {'R', RouteKind::REARM, REARM_FIELDS, countOf(REARM_FIELDS)},
                                {'G', RouteKind::MOVE, MOVE_FIELDS, countOf(MOVE_FIELDS)},
                                {'H', RouteKind::READ_HISTORY, READ_FIELDS, countOf(READ_FIELDS)},
                                {'Q', RouteKind::RELEASED_QUICK, QUICK_FIELDS, countOf(QUICK_FIELDS)},
                                {'W', RouteKind::WORD67, WORD_FIELDS, countOf(WORD_FIELDS)},
                                {'L', RouteKind::RELATIVE, POSITION_FIELDS, countOf(POSITION_FIELDS)},
                                {'D', RouteKind::SETTINGS_2202, LIMIT_FIELDS, countOf(LIMIT_FIELDS)},
                                {'J', RouteKind::IO_DI3, DI3_FIELDS, countOf(DI3_FIELDS)},
                                {'Z', RouteKind::COUNTER_ZERO, CLEAR_FIELDS, countOf(CLEAR_FIELDS)},
                                {'C', RouteKind::CLEAR_ALARM, CLEAR_FIELDS, countOf(CLEAR_FIELDS)}};
static_assert(sizeof GRAMMARS / sizeof GRAMMARS[0] == unsigned(RouteKind::CLEAR_ALARM), "one grammar per route");
// One field's value, read from or written to the plan through its declared width.
int64_t load(const RoutePlan& p, const Field& f) {
    const unsigned char* at = reinterpret_cast<const unsigned char*>(&p) + f.offset;
    if (f.width == 1) {
        uint8_t v;
        std::memcpy(&v, at, 1); // Also the bool attended, which holds 0 or 1.
        return v;
    }
    if (f.width == 2) {
        uint16_t v;
        std::memcpy(&v, at, 2);
        return v;
    }
    if (f.low < 0) {
        int32_t v;
        std::memcpy(&v, at, 4);
        return v;
    }
    uint32_t v;
    std::memcpy(&v, at, 4);
    return v;
}
void store(RoutePlan& p, const Field& f, int64_t value) {
    unsigned char* at = reinterpret_cast<unsigned char*>(&p) + f.offset;
    if (f.width == 1) {
        const uint8_t v = uint8_t(value);
        std::memcpy(at, &v, 1);
    } else if (f.width == 2) {
        const uint16_t v = uint16_t(value);
        std::memcpy(at, &v, 2);
    } else {
        const uint32_t v = uint32_t(value); // A signed field's two's complement, as int32_t holds it.
        std::memcpy(at, &v, 4);
    }
}
// One decimal field and its separator. Digits only, a minus only where signed, ten digits at most.
bool field(const char*& p, const char* end, const Field& spec, bool last, int64_t& output) {
    bool negative = false;
    if (spec.low < 0 && p < end && *p == '-') {
        negative = true;
        ++p;
    }
    if (p == end || *p < '0' || *p > '9')
        return false;
    int64_t value = 0;
    unsigned digits = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (++digits > 10)
            return false;
        value = value * 10 + (*p++ - '0');
    }
    if (negative)
        value = -value;
    if (value < spec.low || value > spec.high)
        return false;
    if (last) {
        if (p != end)
            return false;
    } else {
        if (p == end || *p != ',')
            return false;
        ++p;
    }
    output = value;
    return true;
}
// A planned stop names all of its policy; no stop names none of it. `cycles` bounds stopAfter.
bool stopPolicyValid(const RoutePlan& p, unsigned cycles) {
    return p.stopKind ? p.stopAfter && p.stopAfter <= cycles && p.stopDeadlineMs && p.stopIntervalMs >= 10 &&
                            (!p.bestEffortMs || p.bestEffortMs >= p.stopDeadlineMs)
                      : !p.stopAfter && !p.stopDeadlineMs && !p.bestEffortMs && !p.stopIntervalMs;
}
// Terminals each plan can produce at most, against ROUTE_TERMINALS. A session action's observation
// cycles are bounded by its deadline over its interval, plus the immediate first cycle(s). A stop cycle
// reads MONITORS and STATUS, five fields, and two FEEDBACK fields more once a QUICK_STOP observes
// standstill by 6064 and 606C (released, low bits 000, running still set), so seven are bounded.
unsigned stopTerminals(uint32_t deadlineMs, uint32_t intervalMs) {
    return 1 + 7 * (1 + deadlineMs / intervalMs) + 2; // Word, cycles, a held best-effort pair.
}
uint64_t stopSpanUs(const RoutePlan& p);
unsigned terminalBound(const RoutePlan& p) {
    switch (p.kind) {
    case RouteKind::READ_ACTIONS:
        return 9 + 3 + 3 + 7;
    case RouteKind::READ_IO:
        return 7 + 9 + 4 + 7 + 3 + 7;
    case RouteKind::READ_TUNING:
        return 7 + 6 + 6 + 12 + 7 + 7;
    case RouteKind::READ_HISTORY:
        // The full read, 1003:00 and at most four entries (a larger count fails the read), 1001 and 603F.
        return 9 + 1 + 4 + 2;
    case RouteKind::MODE_ONE:
        return 15 + 1 + 9 + 7u * (p.cycles - 1u);
    case RouteKind::ENABLE:
        return 9 + 2 + 3 + 7 * (3 + p.deadlineMs / p.intervalMs);
    case RouteKind::STOP:
        return 5 + stopTerminals(p.deadlineMs, p.intervalMs);
    case RouteKind::POSITION:
    case RouteKind::RELATIVE:
        return 9 + 3 + 1 + 5 + 7u * p.cycles +
               (p.stopKind ? stopTerminals(p.stopDeadlineMs, p.stopIntervalMs) : 0);
    case RouteKind::REARM:
        return 9 + 1 + 7u * p.cycles;
    case RouteKind::RELEASED_QUICK:
        // Two full reads, the 000B and its cycles.
        return 9 + 9 + 1 + 7u * p.cycles;
    case RouteKind::SETTINGS_2202:
        // SOFT_LIMITS (2202, 607D:00..02), STATUS and MONITORS, the 2202 write and the group's readback.
        return 4 + 5 + 1 + 4;
    case RouteKind::IO_DI3:
        // IO_INPUT_CONFIGURATION (2400:00..08), STATUS and MONITORS, the one changed field's write, the
        // group's readback, then MONITORS and INPUTS once.
        return 9 + 5 + 1 + 9 + 4;
    case RouteKind::WORD67:
        // Two full reads, the 0006 and its cycles, the 0007 and its cycles.
        return 9 + 9 + 1 + 7u * p.wordCycles + 1 + 7u * p.cycles;
    case RouteKind::MOVE:
        // Identity, full read (two for a relative move), four parameters, the trigger, the move's
        // cycles (one begins at most every interval, plus one cut short at the deadline) and its rearm.
        // An observer node adds its check read and one image cycle at most every interval while the
        // move or its planned stop runs, plus one cut short at the end.
        return 9 + 9 + (p.relativeBase ? 9 : 0) + 4 + 1 + 7 * (2 + p.deadlineMs / p.intervalMs) + 1 +
               (p.stopKind ? stopTerminals(p.stopDeadlineMs, p.stopIntervalMs) : 0) +
               (p.observer ? 9 + 7 * (2 + (p.deadlineMs + uint32_t(stopSpanUs(p) / 1000)) / p.intervalMs) : 0);
    case RouteKind::COUNTER_ZERO:
        // MONITORS, STATUS and FEEDBACK; 0, 1, 0; then cycles of the same seven fields to the deadline.
        return 7 + 3 + 7 * (2 + p.deadlineMs / p.intervalMs);
    case RouteKind::CLEAR_ALARM:
        // ERRORS, 1003:00 and at most four entries, MONITORS and STATUS; 1, 0; cycles of MONITORS and
        // STATUS to the deadline; then ERRORS once.
        return 2 + 5 + 5 + 2 + 5 * (2 + p.deadlineMs / p.intervalMs) + 2;
    default:
        return ROUTE_TERMINALS + 1;
    }
}
// A planned stop's whole span after its request: the sequenced deadline or the later best effort.
uint64_t stopSpanUs(const RoutePlan& p) {
    return p.stopKind ? uint64_t(p.bestEffortMs > p.stopDeadlineMs ? p.bestEffortMs : p.stopDeadlineMs) * 1000 : 0;
}
// POSITION or RELATIVE with a planned stop: the latest that stop can end, after the first write. The
// writes share one operation deadline. Each route cycle before the stop begins at most an interval, or one read's
// operation deadline, after the previous one; a read that misses its deadline ends the route with no
// stop. The stop is asked for when cycle stopAfter has been read.
uint64_t positionStopUs(const RoutePlan& p) {
    const uint64_t interval = uint64_t(p.intervalMs) * 1000;
    const uint64_t cycle = interval > ROUTE_OPERATION_US ? interval : uint64_t(ROUTE_OPERATION_US);
    return ROUTE_OPERATION_US + p.stopAfter * cycle + stopSpanUs(p);
}
// Longest time a plan's own steps or session action can need after admission, in milliseconds.
uint32_t durationBound(const RoutePlan& p) {
    const uint32_t stop = uint32_t(stopSpanUs(p) / 1000);
    switch (p.kind) {
    case RouteKind::ENABLE:
    case RouteKind::COUNTER_ZERO:
    case RouteKind::CLEAR_ALARM:
        return p.deadlineMs;
    case RouteKind::STOP:
        return p.bestEffortMs > p.deadlineMs ? p.bestEffortMs : p.deadlineMs;
    case RouteKind::POSITION:
    case RouteKind::RELATIVE: {
        const uint32_t cycles = p.cycles * p.intervalMs + stop;
        const uint32_t planned = p.stopKind ? uint32_t(positionStopUs(p) / 1000) : 0;
        return cycles > planned ? cycles : planned;
    }
    case RouteKind::MOVE:
        return p.deadlineMs + stop;
    case RouteKind::MODE_ONE:
    case RouteKind::REARM:
    case RouteKind::RELEASED_QUICK:
        return p.cycles * p.intervalMs;
    case RouteKind::WORD67:
        return (p.wordCycles + p.cycles) * p.intervalMs;
    default:
        return 0;
    }
}
} // namespace

const char* routeName(RouteKind kind) noexcept {
    return unsigned(kind) < sizeof names / sizeof names[0] ? names[unsigned(kind)] : "none";
}
bool parseRouteCommand(const char* text, size_t size, RoutePlan& output) noexcept {
    if (!text || size < 2 || size > ROUTE_LINE)
        return false;
    const Grammar* grammar = nullptr;
    for (const Grammar& g : GRAMMARS)
        if (g.letter == text[0])
            grammar = &g;
    if (!grammar)
        return false;
    RoutePlan plan;
    plan.kind = grammar->kind;
    const char* p = text + 1;
    const char* end = text + size;
    for (unsigned i = 0; i < grammar->count; ++i) {
        int64_t value = 0;
        if (!field(p, end, grammar->fields[i], i + 1 == grammar->count, value))
            return false;
        store(plan, grammar->fields[i], value);
    }
    if (!routePlanValid(plan))
        return false;
    output = plan;
    return true;
}
bool routePlanValid(const RoutePlan& p) noexcept {
    const Grammar* grammar = nullptr;
    for (const Grammar& g : GRAMMARS)
        if (g.kind == p.kind)
            grammar = &g;
    if (!grammar)
        return false;
    for (unsigned i = 0; i < grammar->count; ++i) {
        const Field& f = grammar->fields[i];
        const int64_t value = load(p, f);
        if (value < f.low || value > f.high)
            return false;
    }
    switch (p.kind) {
    case RouteKind::STOP:
        return !p.bestEffortMs || p.bestEffortMs >= p.deadlineMs;
    case RouteKind::POSITION:
        return stopPolicyValid(p, p.cycles);
    case RouteKind::RELATIVE:
        // A zero displacement writes nothing a base could be measured from (R-08).
        return p.target && stopPolicyValid(p, p.cycles);
    case RouteKind::MOVE: {
        // A signal value outside its own mask can never be reported; the library refuses it too. An
        // ACTUAL witness is the start position both fresh reads show, and an absolute move has none.
        const bool base = p.relativeBase == 0 ? p.base == 0 : p.relativeBase == 1 ? p.base == p.startPosition : true;
        // An observer is the other node, never the moving one.
        return !(p.acceptValue & ~p.acceptMask) && !(p.completeValue & ~p.completeMask) && base &&
               (!p.relativeBase || p.target) && p.observer != p.node && stopPolicyValid(p, 16);
    }
    default:
        return true;
    }
}
uint32_t routeLineCrc(const char* text, size_t size) noexcept {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i <= size; ++i) {
        crc ^= i ? uint8_t(text[i - 1]) : uint8_t('T'); // The command byte first, as the host sent it.
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
bool checkRouteIntegrity(const char* text, size_t size, size_t& body) noexcept {
    // `,K` and eight lowercase hexadecimal digits close the line; nothing may follow them.
    if (!text || size < 11 || size > ROUTE_LINE)
        return false;
    const char* field = text + size - 10;
    if (field[0] != ',' || field[1] != 'K')
        return false;
    uint32_t stated = 0;
    for (unsigned i = 2; i < 10; ++i) {
        const char c = field[i];
        const unsigned digit = c >= '0' && c <= '9' ? unsigned(c - '0') : c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10) : 16u;
        if (digit > 15)
            return false;
        stated = stated << 4 | digit;
    }
    if (routeLineCrc(text, size - 10) != stated)
        return false;
    body = size - 10;
    return true;
}

BenchRoute::BenchRoute(FirmwareProbe& firmware, C::SdoClient& services,
                       const RouteCallbacks& callbacks) noexcept
    : firmware_(firmware), services_(services), callbacks_(callbacks) {
    DownloadPreflight check;
    check.context = this;
    check.check = &BenchRoute::preflight;
    firmware_.setPreflight(check);
}
bool BenchRoute::preflight(void* context, SequenceId sequence, const C::SdoTransfer& transfer,
                           uint64_t now) noexcept {
    BenchRoute& route = *static_cast<BenchRoute*>(context);
    // A session answers true for every sequence but its own, so asking both is exact.
    for (AxisSession* session : route.sessions_)
        if (session && !session->preflight(sequence, transfer, now))
            return false;
    return route.noteSettings(sequence, transfer, now);
}
// The settings transaction's notice, just before its download may leave: the axis binding advances to
// the next configuration epoch once, so that anything prepared at the old epoch goes stale. The owner
// guards each download once. A refused notice withdraws the download, provably unsent.
bool BenchRoute::noteSettings(SequenceId sequence, const C::SdoTransfer& transfer, uint64_t now) noexcept {
    if (!occupied_ || record_.terminal || !isSettings(record_.plan.kind) || !work_ ||
        steps_[stepIndex_].kind != STEP_SETTINGS || !stepActive_)
        return true;
    const SequenceId own = axis().sequence();
    L::DriverSettingsContext& tx = work_->transaction;
    if (sequence.owner != own.owner || sequence.slot != own.slot || sequence.generation != own.generation ||
        transfer.direction != C::SdoDirection::DOWNLOAD || transfer.operation != tx.operation)
        return true;
    return L::noteDriverTransmission(tx, work_->live, work_->binding, now).isOk();
}
bool BenchRoute::attach(unsigned node, AxisSession& session) noexcept {
    if (node < 1 || node > 2 || sessions_[node - 1] || session.isOpen())
        return false;
    sessions_[node - 1] = &session;
    return true;
}
bool BenchRoute::attachWork(RouteWork& work) noexcept {
    if (work_)
        return false;
    work_ = &work;
    return true;
}
bool BenchRoute::currentTarget(L::ReadTarget& output) const noexcept {
    const C::SdoChannel* channel = services_.channel(slot());
    const C::NodeSnapshot* node = services_.node(slot());
    if (!channel || !node || node->exhausted)
        return false;
    output.channel = *channel;
    output.nodeGeneration = node->route.generation;
    return true;
}
const char* BenchRoute::begin(const RoutePlan& plan, uint64_t now) noexcept {
    if (occupied_)
        return "busy";
    // A plan built in code is judged as the parser judges a line, before any bound divides by it.
    if (!routePlanValid(plan))
        return "plan";
    const uint8_t index = uint8_t(plan.node - 1);
    AxisSession* session = sessions_[index];
    if (!session)
        return "session";
    if (plan.trial <= lastTrial_)
        return "trial";
    MotorControlCANOwner::CanBusOwner& owner = firmware_.owner();
    if (now < lastUs_ || now < owner.lastTimeUs() || !firmware_.running() || firmware_.progressionFailed() ||
        firmware_.app().readContext().state != M::ReadState::EMPTY || firmware_.app().pendingRequest().owner)
        return "state";
    const bool motion = isMotion(plan.kind), settings = isSettings(plan.kind);
    if ((motion || settings) && !plan.attended)
        return "unattended";
    const unsigned stop = plansStop(plan) ? 1 : 0;
    const unsigned stopCase = unsigned(RouteKind::STOP);
    const unsigned settingsCase = plan.kind == RouteKind::IO_DI3 ? 1 : 0;
    if (motion && (motionTotal_ + 1u + stop > ROUTE_MOTION_TOTAL ||
                   motionCases_[unsigned(plan.kind)][index] + 1u > ROUTE_MOTION_PER_CASE ||
                   motionCases_[stopCase][index] + stop > ROUTE_MOTION_PER_CASE))
        return "budget";
    if (settings && settingsCases_[settingsCase][index] + 1u > ROUTE_SETTINGS_PER_CASE)
        return "budget";
    if (settings && !work_)
        return "memory";
    if (terminalBound(plan) > ROUTE_TERMINALS)
        return "capacity";
    if (durationBound(plan) > ROUTE_CUTOFF_MS - 5000)
        return "duration";
    const C::SdoChannel* channel = services_.channel(index);
    const C::NodeSnapshot* node = services_.node(index);
    if (!channel || !node || node->exhausted)
        return "route";
    L::ReadTarget target;
    target.channel = *channel;
    target.nodeGeneration = node->route.generation;
    // The user confirmed both drives read CL86-C on their labels (docs/hardware_bench.md).
    if (!session->isOpen() && !firmware_.app().openAxis(*session, target, L::SettingsModel::CL86_C, now))
        return "session";
    if (session->busy() || !owner.sequence(session->sequence()))
        return "session";
    if (plan.kind == RouteKind::MODE_ONE &&
        (session->motionHistoryWritten() || modeAdmitted_[index] || session->latestControl()))
        return "history";
    if (plan.kind == RouteKind::ENABLE && session->motionHistoryWritten())
        return "history";
    // The session admits its enable words as ordinary SDO work, which needs a current NMT state or
    // service qualification. On this fixture a bounded SDO reply strictly after its handoff gives
    // the latter (docs/adapters/twai_receive_time.md).
    // A move's words and cycles are the session's own ordinary SDO work, as an enable's are.
    C::ServiceEligibility eligibility;
    // A clear's writes and reads are the session's own ordinary SDO work too.
    if ((plan.kind == RouteKind::ENABLE || plan.kind == RouteKind::MOVE || isClear(plan.kind)) &&
        (!C::serviceEligibility(*node, now, eligibility) || !eligibility.ordinarySdo))
        return "eligibility";
    // The observer is read on its own route and session, opened here if this is its first route.
    if (plan.kind == RouteKind::MOVE && plan.observer) {
        const uint8_t other = uint8_t(plan.observer - 1);
        AxisSession* watcher = sessions_[other];
        const C::SdoChannel* otherChannel = services_.channel(other);
        const C::NodeSnapshot* otherNode = services_.node(other);
        if (!watcher || !work_ || !otherChannel || !otherNode || otherNode->exhausted)
            return "observer";
        L::ReadTarget otherTarget;
        otherTarget.channel = *otherChannel;
        otherTarget.nodeGeneration = otherNode->route.generation;
        if (!watcher->isOpen() && !firmware_.app().openAxis(*watcher, otherTarget, L::SettingsModel::CL86_C, now))
            return "observer";
        if (watcher->busy() || !owner.sequence(watcher->sequence()))
            return "observer";
    }
    const C::SdoCompletion* control = session->latestControl();
    if ((writesPosition(plan.kind) || plan.kind == RouteKind::MOVE) &&
        (!control || control->transfer.data[0] != 0x0f || control->transfer.data[1] != 0))
        return "control";
    // Only the acknowledged predecessor the host named, of a finished record of this object: missing,
    // uncertain or any other latest word refuses, so a lost word can never be followed by a 000F.
    if (plan.kind == RouteKind::REARM && !rearmable(index, *session, plan.predecessor))
        return "control";
    // A trigger that may have moved the drive without an observed completion fences new moves.
    if (plan.kind == RouteKind::MOVE && session->moveFenced())
        return "fenced";
    lastTrial_ = plan.trial;
    if (motion) {
        motionTotal_ = uint8_t(motionTotal_ + 1 + stop);
        ++motionCases_[unsigned(plan.kind)][index];
        motionCases_[stopCase][index] = uint8_t(motionCases_[stopCase][index] + stop);
    }
    if (settings)
        ++settingsCases_[settingsCase][index];
    RouteRecord& r = record_;
    r.plan = plan;
    r.failure = RouteFailure::NONE;
    r.why = "none";
    r.terminal = r.evidenceLost = r.lossCancel = r.tailOverflow = r.unsettled = false;
    r.step = 0;
    r.motionTotal = motionTotal_;
    // Only motion and settings routes have a row.
    r.caseCount = motion ? motionCases_[unsigned(plan.kind)][index] : settings ? settingsCases_[settingsCase][index] : 0;
    r.cycles = 0;
    r.startedUs = now;
    r.completedUs = r.maxServiceGapUs = 0;
    r.before = r.after = RouteResources();
    if (callbacks_.sample)
        callbacks_.sample(callbacks_.context, r.before);
    r.dropped = 0;
    r.admission = 0;
    r.terminalCount = r.tailCount = 0;
    r.trafficCount = 0;
    r.checkPresent = r.checkPassed = r.firstPresent = false;
    r.first = r.check = r.last = RouteSnapshot();
    r.historyPresent = false;
    r.historyCount = 0;
    std::memset(r.history, 0, sizeof r.history);
    r.statusPresent = false;
    r.statusCode = 0;
    r.statusDetail = 0;
    r.statusMessage = "";
    r.enableRequested = r.stopRequested = false;
    r.stopRequestKind = r.stopPathRequested = 0;
    r.actionRequestedUs = 0;
    r.emcy = false;
    std::memset(r.emcyData, 0, sizeof r.emcyData);
    r.moveRequested = false;
    r.movePhases = 0;
    r.moveRequestedUs = 0;
    r.rearmFollows = plan.kind == RouteKind::REARM ? control->transfer.operation : 0;
    r.rearmFollowsUs = plan.kind == RouteKind::REARM ? control->completedUs : 0;
    r.rearmFollowsWord = plan.kind == RouteKind::REARM ? uint16_t(control->transfer.data[0] |
                                                                  unsigned(control->transfer.data[1]) << 8)
                                                       : 0;
    r.priorPresent = control != nullptr;
    r.priorWord = control ? uint16_t(control->transfer.data[0] | unsigned(control->transfer.data[1]) << 8) : 0;
    r.priorOperation = control ? control->transfer.operation : 0;
    r.priorUs = control ? control->completedUs : 0;
    r.targetReadPresent = false;
    r.targetRead = 0;
    // Only a value acknowledged on the node epoch now current: a boot-up resets what the drive holds.
    r.commandedKnown = commandedKnown_[index] && commandedEpoch_[index] == uint32_t(target.nodeGeneration);
    r.commanded = r.commandedKnown ? commanded_[index] : 0;
    r.commandedTrial = r.commandedKnown ? commandedTrial_[index] : 0;
    r.commandedRelative = r.commandedKnown && commandedRelative_[index];
    r.witnessBasis = 0;
    r.witnessPosition = 0;
    r.settingsPrepared = r.settingsNoop = r.settingsEffects = r.settingsUncertain = false;
    r.settingsState = r.settingsOutcome = 0;
    r.settingsChanged = r.settingsConfirmed = 0;
    r.configurationBefore = r.configurationAfter = 0;
    r.configurationFollowed = false;
    r.groupPresent[0] = r.groupPresent[1] = false;
    std::memset(r.limitSelector, 0, sizeof r.limitSelector);
    std::memset(r.limitEntries, 0, sizeof r.limitEntries);
    std::memset(r.limitMinimum, 0, sizeof r.limitMinimum);
    std::memset(r.limitMaximum, 0, sizeof r.limitMaximum);
    std::memset(r.ioEntries, 0, sizeof r.ioEntries);
    std::memset(r.ioPolarity, 0, sizeof r.ioPolarity);
    std::memset(r.ioFunctions, 0, sizeof r.ioFunctions);
    r.inputsPresent = false;
    r.logicalInputs = 0;
    r.digitalInputs = 0;
    r.observerChecked = r.observerPassed = r.observerStopped = r.observerMoved = r.observerEmcy = false;
    r.observerWhy = "none";
    r.observerCycles = 0;
    r.observerMinimum = r.observerMaximum = r.observerVelocity = 0;
    r.observerMonitors = 0;
    std::memset(r.observerEmcyData, 0, sizeof r.observerEmcyData);
    r.observerCheck = r.observerLast = RouteSnapshot();
    r.nodeRx[0] = r.nodeRx[1] = r.nodeUnmatched[0] = r.nodeUnmatched[1] = 0;
    const MotorControlCANOwner::Stats counters = owner.stats();
    r.droppedBefore = r.droppedAfter = counters.rxDropped;
    r.unmatchedBefore = r.unmatchedAfter = counters.unmatchedFrames;
    r.lostBefore = r.lostAfter = counters.rxLost;
    r.clearRequested = false;
    // Older copies belong to earlier work; this record starts with an empty owner trace.
    for (unsigned i = 0; i < MotorControlCANOwner::CanBusOwner::TRACE_CAPACITY && owner.takeDiagnostic(diagnostic_);
         ++i) {
    }
    overwritten_ = owner.stats().traceOverwritten;
    rxLost_ = owner.stats().rxLost;
    if (overwritten_ == UINT32_MAX || rxLost_ == UINT32_MAX)
        lose(1, false);
    static const Step actions[] = {{STEP_STATE, FULL},
                                   {STEP_GROUP, uint8_t(L::DriverGroup::MOTION_PARAMETERS)},
                                   {STEP_SINGLE, 0},
                                   {STEP_SINGLE, 1},
                                   {STEP_SINGLE, 2},
                                   {STEP_STATE, IMAGE},
                                   {STEP_END, 0}};
    static const Step io[] = {{STEP_STATE, IMAGE},
                              {STEP_GROUP, uint8_t(L::DriverGroup::IO_INPUT_CONFIGURATION)},
                              {STEP_GROUP, uint8_t(L::DriverGroup::IO_OUTPUT_CONFIGURATION)},
                              {STEP_GROUP, uint8_t(L::DriverGroup::IO_INPUT_FILTERS)},
                              {STEP_GROUP, uint8_t(L::DriverGroup::IO_OUTPUT_COMMAND)},
                              {STEP_STATE, IMAGE},
                              {STEP_END, 0}};
    static const Step tuning[] = {{STEP_STATE, IMAGE},
                                  {STEP_TUNING, uint8_t(L::TuningGroup::CURRENT_HOLDING)},
                                  {STEP_TUNING, uint8_t(L::TuningGroup::FILTER_POSITION)},
                                  {STEP_TUNING, uint8_t(L::TuningGroup::LOOP_GAINS)},
                                  {STEP_TUNING, uint8_t(L::TuningGroup::EDS_GAINS)},
                                  {STEP_STATE, IMAGE},
                                  {STEP_END, 0}};
    static const Step mode[] = {{STEP_STATE, FULL},
                                {STEP_GROUP, uint8_t(L::DriverGroup::MOTION_PARAMETERS)},
                                {STEP_SINGLE, 0},
                                {STEP_SINGLE, 1},
                                {STEP_SINGLE, 2},
                                {STEP_CHECK_MODE, 0},
                                {STEP_MODE_WRITE, 0},
                                {STEP_OBSERVE, 0}};
    static const Step enable[] = {{STEP_STATE, FULL}, {STEP_ENABLE, 0}};
    static const Step stopSteps[] = {{STEP_STATE, STOP_STATE}, {STEP_STOP, 0}};
    static const Step position[] = {{STEP_STATE, FULL},
                                    {STEP_CHECK_POSITION, 0},
                                    {STEP_GROUP, uint8_t(L::DriverGroup::MOTION_PARAMETERS)},
                                    {STEP_SINGLE, 1},
                                    {STEP_POSITION_WRITES, 0},
                                    {STEP_OBSERVE, 0}};
    static const Step rearm[] = {
        {STEP_STATE, FULL}, {STEP_CHECK_REARM, 0}, {STEP_WORD_WRITE, WORD_REARM}, {STEP_OBSERVE, OBSERVE_ALL}};
    static const Step move[] = {{STEP_IDENTITY, 0}, {STEP_STATE, FULL}, {STEP_CHECK_MOVE, 0}, {STEP_MOVE, 0}};
    // A relative witness says the drive is stationary: two fresh reads must show the same idle state.
    static const Step moveObserved[] = {
        {STEP_IDENTITY, 0}, {STEP_OBSERVER_CHECK, 0}, {STEP_STATE, FULL}, {STEP_CHECK_MOVE, 0}, {STEP_MOVE, 0}};
    static const Step moveRelativeObserved[] = {{STEP_IDENTITY, 0}, {STEP_OBSERVER_CHECK, 0}, {STEP_STATE, FULL},
                                                {STEP_SECOND, FULL}, {STEP_CHECK_MOVE, 0},      {STEP_MOVE, 0}};
    // The library's own prerequisite reads, then its request; CLEAR_ALARM first needs the alarm.
    static const Step counter[] = {{STEP_STATE, IMAGE}, {STEP_CLEAR, 0}};
    static const Step alarm[] = {{STEP_STATE, ALARM_STATE}, {STEP_CHECK_ALARM, 0}, {STEP_CLEAR, 0}};
    static const Step moveRelative[] = {
        {STEP_IDENTITY, 0}, {STEP_STATE, FULL}, {STEP_SECOND, FULL}, {STEP_CHECK_MOVE, 0}, {STEP_MOVE, 0}};
    // 1001 and 603F before and after the first 1003 read; the history is read alone between them.
    static const Step history[] = {
        {STEP_STATE, FULL}, {STEP_STATE, HISTORY_STATE}, {STEP_STATE, ERROR_STATE}, {STEP_END, 0}};
    static const Step quick[] = {{STEP_STATE, FULL},
                                 {STEP_SECOND, FULL},
                                 {STEP_CHECK_RELEASED, 0},
                                 {STEP_WORD_WRITE, WORD_QUICK},
                                 {STEP_OBSERVE, OBSERVE_ALL}};
    static const Step words[] = {{STEP_STATE, FULL},
                                 {STEP_SECOND, FULL},
                                 {STEP_CHECK_RELEASED, 0},
                                 {STEP_WORD_WRITE, WORD_SIX},
                                 {STEP_OBSERVE, OBSERVE_WORD_SIX},
                                 {STEP_WORD_WRITE, WORD_SEVEN},
                                 {STEP_OBSERVE, OBSERVE_ALL}};
    // The prior group, then a clean STATUS and MONITORS read, so the group is the older of the two, as
    // the library's prerequisites order them; then the one transaction.
    static const Step limits[] = {{STEP_PRIOR, uint8_t(L::DriverGroup::SOFT_LIMITS)},
                                  {STEP_STATE, STOP_STATE},
                                  {STEP_SETTINGS, 0},
                                  {STEP_END, 0}};
    static const Step di3[] = {{STEP_PRIOR, uint8_t(L::DriverGroup::IO_INPUT_CONFIGURATION)},
                               {STEP_STATE, STOP_STATE},
                               {STEP_SETTINGS, 0},
                               {STEP_STATE, INPUTS_STATE},
                               {STEP_END, 0}};
    const Step* const tables[] = {nullptr, actions, io,    tuning, mode,     enable, stopSteps, position, rearm,
                                  move,    history, quick, words,  position, limits, di3,       counter,  alarm};
    static_assert(sizeof tables / sizeof tables[0] == unsigned(RouteKind::CLEAR_ALARM) + 1, "one table per route");
    const Step* const moves[] = {move, moveRelative, moveObserved, moveRelativeObserved};
    steps_ = plan.kind == RouteKind::MOVE ? moves[(plan.relativeBase ? 1 : 0) + (plan.observer ? 2 : 0)]
                                          : tables[unsigned(plan.kind)];
    stepIndex_ = subStep_ = 0;
    pending_ = stepActive_ = readActive_ = actionStarted_ = abortRequested_ = emcyHandled_ = false;
    lossHandled_ = stopAttempted_ = false;
    writtenOperation_ = 0;
    writtenUs_ = feedbackUs_ = 0;
    settingsSettled_ = ownerFollowed_ = false;
    observerPending_ = observerActive_ = false;
    observerNextUs_ = observerCycleUs_ = now;
    endFailure_ = RouteFailure::NONE;
    endWhy_ = "none";
    nextCycleUs_ = cycleStartUs_ = now;
    cutoffUs_ = now + uint64_t(ROUTE_CUTOFF_MS) * 1000;
    occupied_ = true;
    lastUs_ = now;
    return nullptr;
}
void BenchRoute::lose(uint32_t count, bool tail) noexcept {
    if (tail) {
        record_.tailOverflow = true;
        return;
    }
    record_.evidenceLost = true;
    record_.dropped = UINT32_MAX - record_.dropped < count ? UINT32_MAX : record_.dropped + count;
}
void BenchRoute::copyTerminal(const Completion& done) noexcept {
    if (record_.terminalCount == ROUTE_TERMINALS) {
        lose(1, false);
        return;
    }
    const C::SdoCompletion& t = done.service;
    RouteTerminal& out = record_.terminals[record_.terminalCount++];
    out.admittedUs = t.admittedUs;
    out.completedUs = t.completedUs;
    out.operation = t.transfer.operation;
    out.request = t.transfer.request;
    out.ownerRequest = done.id.generation;
    out.step = t.transfer.step;
    out.abortCode = t.abortCode;
    out.nodeGeneration = uint32_t(t.nodeGeneration);
    out.index = t.transfer.index;
    out.subindex = t.transfer.subindex;
    out.direction = uint8_t(t.transfer.direction);
    out.outcome = uint8_t(t.outcome);
    out.flags = uint8_t(unsigned(t.protocolAcknowledged) | unsigned(t.effectPossible) << 1 |
                        unsigned(t.effectUncertain) << 2 | unsigned(t.clientAbortQueued) << 3);
    out.size = t.size;
    out.reason = uint8_t(done.reason);
    out.node = uint8_t((t.transfer.channel.clientToServerId - 0x600u) & 0x7fu);
    out.stages = uint8_t(unsigned(done.adapterAdmitted) | unsigned(done.controllerAccepted) << 1 |
                         unsigned(t.queued) << 2 | unsigned(t.transmissionPossible) << 3 |
                         unsigned(t.transmissionConfirmed) << 4 | unsigned(t.protocolAcknowledged) << 5 |
                         unsigned(t.effectUncertain) << 6 | unsigned(t.clientAbortQueued) << 7);
    std::memcpy(out.data, t.data, sizeof out.data);
}
void BenchRoute::copyTraffic(const Diagnostic& in, bool tail) noexcept {
    uint8_t& count = record_.tailCount;
    if (tail ? count == ROUTE_TAIL : record_.trafficCount == ROUTE_TRAFFIC) {
        lose(1, tail);
        return;
    }
    RouteTraffic& out = tail ? record_.tail[count++] : record_.traffic[record_.trafficCount++];
    out.processedUs = in.processedUs;
    out.receivedUs = in.receivedUs;
    out.earliestUs = in.receivedEarliestUs;
    out.latestUs = in.receivedLatestUs;
    out.id = in.frame.id;
    out.epoch = in.controllerEpoch;
    out.txKind = uint8_t(in.txId.kind);
    out.txRequest = in.txId.kind == TransportTxId::Kind::NMT ? in.txId.nmt.generation
                                                            : in.txId.sdo.request.generation;
    out.txFrame = in.txId.sdo.frame;
    out.txChannel = in.txId.kind == TransportTxId::Kind::NMT ? 0 : in.txId.sdo.request.channel;
    // Copied frames per node, and those the owner's dispatch did not accept: CANopen identifiers carry
    // their node in the low seven bits.
    const unsigned from = in.frame.id & 0x7fu;
    if (!tail && in.kind == DiagnosticKind::RX && !in.frame.extended && in.frame.id >= 0x80 && in.frame.id < 0x780 &&
        from >= 1 && from <= 2) {
        ++record_.nodeRx[from - 1];
        record_.nodeUnmatched[from - 1] = uint16_t(record_.nodeUnmatched[from - 1] + !in.accepted);
    }
    std::memcpy(out.data, in.frame.data, sizeof out.data);
    out.kind = uint8_t(in.kind);
    out.dlc = in.frame.dlc;
    out.flags = uint8_t(unsigned(in.frame.extended) | unsigned(in.frame.rtr) << 1 | unsigned(in.accepted) << 2 |
                        unsigned(in.receivedTimeKnown) << 3 | unsigned(in.receiveBoundsKnown) << 4);
    out.state = in.kind == DiagnosticKind::CONTROLLER ? uint8_t(in.controller)
                : in.kind == DiagnosticKind::RECOVERY ? uint8_t(in.recovery)
                                                      : 0;
}
void BenchRoute::capture(bool tail) noexcept {
    const MotorControlCANOwner::Stats stats = firmware_.owner().stats();
    if (stats.traceOverwritten < overwritten_ || stats.traceOverwritten == UINT32_MAX)
        lose(1, tail);
    else if (stats.traceOverwritten > overwritten_)
        lose(stats.traceOverwritten - overwritten_, tail);
    if (stats.rxLost < rxLost_ || stats.rxLost == UINT32_MAX)
        lose(1, tail);
    else if (stats.rxLost > rxLost_)
        lose(stats.rxLost - rxLost_, tail);
    overwritten_ = stats.traceOverwritten;
    rxLost_ = stats.rxLost;
    const uint32_t emcyId = 0x80u + record_.plan.node;
    for (unsigned i = 0; i < MotorControlCANOwner::CanBusOwner::TRACE_CAPACITY &&
                         firmware_.owner().takeDiagnostic(diagnostic_);
         ++i) {
        const Diagnostic& d = diagnostic_;
        if (d.kind == DiagnosticKind::TERMINAL) {
            if (!tail)
                copyTerminal(d.completion);
            continue; // Terminals after the record's own terminal stay in the owner, not here.
        }
        copyTraffic(d, tail);
        // A nonzero EMCY error code from this node ends the route. Zero is the no-error form.
        const bool fault = !tail && d.kind == DiagnosticKind::RX && !d.frame.extended && !d.frame.rtr &&
                           d.frame.dlc >= 2 && (d.frame.data[0] | d.frame.data[1]);
        if (fault && !record_.emcy && d.frame.id == emcyId) {
            record_.emcy = true;
            std::memcpy(record_.emcyData, d.frame.data, sizeof record_.emcyData);
        }
        // One from an observer node ends only the observing: that node gets no request because of it.
        if (fault && record_.plan.observer && !record_.observerEmcy && d.frame.id == 0x80u + record_.plan.observer) {
            record_.observerEmcy = record_.observerStopped = true;
            record_.observerWhy = "emcy";
            observerActive_ = false;
            std::memcpy(record_.observerEmcyData, d.frame.data, sizeof record_.observerEmcyData);
        }
    }
}
void BenchRoute::finish(RouteFailure failure, const char* why, uint64_t now) noexcept {
    if (record_.terminal)
        return;
    // Success is never recorded over a decided failure or lost evidence, whichever step got here.
    if (failure == RouteFailure::NONE && endFailure_ != RouteFailure::NONE) {
        failure = endFailure_;
        why = endWhy_;
    } else if (failure == RouteFailure::NONE && record_.evidenceLost) {
        failure = RouteFailure::EVIDENCE_LOSS;
        why = "lost";
    }
    record_.failure = failure;
    record_.why = why;
    record_.terminal = true;
    record_.completedUs = now;
    record_.step = stepIndex_;
    record_.unsettled = pending_ || observerPending_;
    const MotorControlCANOwner::Stats counters = firmware_.owner().stats();
    record_.droppedAfter = counters.rxDropped;
    record_.unmatchedAfter = counters.unmatchedFrames;
    record_.lostAfter = counters.rxLost;
    if (callbacks_.sample)
        callbacks_.sample(callbacks_.context, record_.after);
    services_.clearUploadExperiment(slot());
    noteEnd(now);
}
// What later routes on this node may follow or report, taken from the session's settled history at
// the terminal: the 010F of a POSITION's planned HALT, and the 607A a move wrote. A settings transaction
// the route left active is settled locally, nothing sent, and the session follows its epoch.
void BenchRoute::noteEnd(uint64_t now) noexcept {
    const RoutePlan& p = record_.plan;
    if (isSettings(p.kind) && work_ && record_.settingsPrepared && !settingsSettled_ && !pending_) {
        L::DriverSettingsContext& tx = work_->transaction;
        if (tx.state == L::SettingsState::WRITING || tx.state == L::SettingsState::READING_BACK) {
            const M::ReadOutcome reason = now >= tx.deadlineUs                     ? M::ReadOutcome::DEADLINE
                                          : record_.failure == RouteFailure::ABORTED ? M::ReadOutcome::CANCELLED
                                                                                     : M::ReadOutcome::INVALIDATED;
            L::advanceDriver(tx, reason, now);
        }
        settleSettings(now);
    }
    const AxisSession& s = axis();
    if (p.kind == RouteKind::POSITION && record_.stopRequested && record_.stopRequestKind == 1 && !s.stopActive()) {
        // The latest word must be this stop's own acknowledged 010F, the slot-0 command of its context.
        const L::ActionContext& h = s.stopAction(L::StopKind::HALT);
        const C::SdoCompletion* c = s.latestControl();
        const C::SdoCompletion& word = h.commandHistory[0];
        if (c && (h.commandEvidenceMask & 1u) && h.state != M::ActionState::EMPTY &&
            h.startedUs >= record_.actionRequestedUs && c->transfer.operation == word.transfer.operation &&
            c->completedUs == word.completedUs && c->transfer.data[0] == 0x0f && c->transfer.data[1] == 0x01) {
            haltOperation_[slot()] = c->transfer.operation;
            haltUs_[slot()] = c->completedUs;
        }
    }
    if (record_.moveRequested && !s.moveActive()) {
        const L::MoveContext& m = s.moveAction();
        const C::SdoCompletion& h = m.setupHistory[0];
        if (m.startedUs >= record_.moveRequestedUs && (m.setupEvidenceMask & 1u) &&
            h.outcome == C::SdoOutcome::SUCCESS && h.protocolAcknowledged && !h.effectUncertain) {
            commanded_[slot()] = m.parameters.targetNative;
            commandedTrial_[slot()] = p.trial;
            commandedEpoch_[slot()] = uint32_t(h.nodeGeneration);
            commandedRelative_[slot()] = m.parameters.target.requested.relative;
            commandedKnown_[slot()] = true;
        }
    }
}
void BenchRoute::endAfterSettling(RouteFailure failure, const char* why) noexcept {
    if (endFailure_ == RouteFailure::NONE) {
        endFailure_ = failure;
        endWhy_ = why;
    }
}
// The session refuses a time behind its owner's, so withdraw at the latest time either has seen.
uint64_t BenchRoute::withdrawalTime(uint64_t now) const noexcept {
    uint64_t at = now < lastUs_ ? lastUs_ : now;
    if (at < firmware_.owner().lastTimeUs())
        at = firmware_.owner().lastTimeUs();
    return at;
}
void BenchRoute::service(uint64_t now) noexcept {
    if (!occupied_ || now < lastUs_)
        return;
    if (record_.terminal) {
        capture(true);
        // Only a firmware failure ends a record with this route's request unsettled.
        C::SdoCompletion late;
        if (pending_ && axis().settle(late))
            pending_ = false;
        // An observer request still on its way when the route ended settles here too, as evidence only:
        // until it does, release() refuses.
        if (observerPending_ && observerAxis().settle(late))
            observerPending_ = false;
        lastUs_ = now;
        return;
    }
    if (now - lastUs_ > record_.maxServiceGapUs)
        record_.maxServiceGapUs = now - lastUs_;
    lastUs_ = now;
    capture(false);
    if (firmware_.progressionFailed()) {
        finish(RouteFailure::INVALIDATED, "firmware", now);
        return;
    }
    if (record_.emcy && !emcyHandled_) {
        emcyHandled_ = true;
        // At the latest time either clock has seen: the session refuses an earlier one, and the
        // withdrawal must not be lost to that.
        const uint64_t at = withdrawalTime(now);
        if (axis().enableActive())
            axis().cancelEnable(at, M::ActionOutcome::DRIVE_FAULT);
        if (axis().moveActive())
            axis().cancelMove(at, M::MoveInterruption::DRIVE_FAULT);
        if (axis().clearActive())
            axis().cancelClear(at, M::ActionOutcome::DRIVE_FAULT);
        endAfterSettling(RouteFailure::OBSERVATION, "emcy");
    }
    if (record_.evidenceLost && !lossHandled_) {
        lossHandled_ = true;
        // The lost span may have held an EMCY or a status report that would have ended this work, so
        // withdraw an active enable or move locally, as an abort does: their unsent words and reads
        // are not admitted, and a request in flight settles first. Nothing is sent because of the
        // loss, and no stop.
        const uint64_t at = withdrawalTime(now);
        if (axis().enableActive() && axis().cancelEnable(at, M::ActionOutcome::CANCELLED))
            record_.lossCancel = true;
        if (axis().moveActive() && axis().cancelMove(at, M::MoveInterruption::CANCELLED))
            record_.lossCancel = true;
        if (axis().clearActive() && axis().cancelClear(at, M::ActionOutcome::CANCELLED))
            record_.lossCancel = true;
        endAfterSettling(RouteFailure::EVIDENCE_LOSS, "lost");
    }
    if (pending_ && !settle(now))
        return;
    if (record_.terminal)
        return;
    if (actionStarted_) {
        serviceAction(now);
        return;
    }
    if (endFailure_ != RouteFailure::NONE) {
        finish(endFailure_, endWhy_, now);
        return;
    }
    if (now >= cutoffUs_) {
        finish(RouteFailure::DEADLINE, "cutoff", now);
        return;
    }
    advanceStep(now);
}
const char* BenchRoute::abort(uint64_t now) noexcept {
    if (!active())
        return "none";
    if (axis().stopActive())
        return "stop";
    if (abortRequested_)
        return "already";
    abortRequested_ = true;
    endAfterSettling(RouteFailure::ABORTED, "abort");
    const uint64_t at = withdrawalTime(now);
    if (axis().enableActive())
        axis().cancelEnable(at, M::ActionOutcome::CANCELLED);
    if (axis().moveActive())
        axis().cancelMove(at, M::MoveInterruption::CANCELLED);
    if (axis().clearActive())
        axis().cancelClear(at, M::ActionOutcome::CANCELLED);
    return nullptr;
}
bool BenchRoute::release() noexcept {
    if (!occupied_ || !record_.terminal || pending_ || observerPending_ || !firmware_.owner().localTxRetired(slot()))
        return false;
    occupied_ = false;
    return true;
}
// Consume this route's own terminal. False while it has not arrived.
bool BenchRoute::settle(uint64_t now) noexcept {
    C::SdoCompletion done;
    if (!axis().settle(done))
        return false;
    pending_ = false;
    const C::SdoTransfer& t = done.transfer;
    const C::SdoTransfer& e = expected_;
    if (t.operation != e.operation || t.request != e.request || t.step != e.step || t.index != e.index ||
        t.subindex != e.subindex || t.direction != e.direction || t.size != e.size ||
        (t.direction == C::SdoDirection::DOWNLOAD && std::memcmp(t.data, e.data, t.size))) {
        finish(RouteFailure::CORRELATION, "terminal", now);
        return true;
    }
    if (steps_[stepIndex_].kind == STEP_SETTINGS) {
        // The transaction consumes every terminal of its own, failures included, and decides what follows.
        RouteWork& w = *work_;
        if (!L::advanceDriver(w.transaction, w.live, w.binding, done, now))
            finish(RouteFailure::CORRELATION, "advance", now);
        return true;
    }
    if (done.outcome != C::SdoOutcome::SUCCESS) {
        // A lost or refused reply leaves its effect unknown; nothing is sent after it.
        finish(RouteFailure::SERVICE, done.effectUncertain ? "uncertain" : "terminal", now);
        return true;
    }
    if (t.direction == C::SdoDirection::DOWNLOAD) {
        ++subStep_;
        const uint8_t kind = steps_[stepIndex_].kind;
        if (kind == STEP_POSITION_WRITES && subStep_ == 1) {
            // The acknowledged 607A, for a later TRIAL_BASE: the target this object last wrote.
            commanded_[slot()] = record_.plan.target;
            commandedTrial_[slot()] = record_.plan.trial;
            commandedEpoch_[slot()] = uint32_t(done.nodeGeneration);
            commandedRelative_[slot()] = record_.plan.kind == RouteKind::RELATIVE;
            commandedKnown_[slot()] = true;
        }
        if (kind == STEP_POSITION_WRITES && subStep_ == 5) {
            // The acknowledged trigger a later REARM may follow, while it stays the latest word.
            triggerOperation_[slot()] = t.operation;
            triggerUs_[slot()] = done.completedUs;
        }
        if (kind == STEP_WORD_WRITE) {
            writtenOperation_ = t.operation;
            writtenUs_ = done.completedUs;
        }
        if (kind == STEP_MODE_WRITE || kind == STEP_WORD_WRITE || subStep_ == 5)
            nextStep(now);
        return true;
    }
    if (readMode_ == READ_SINGLE) {
        if (t.index == L::OBJECT_607A_00.index && done.size == 4) {
            record_.targetReadPresent = true;
            record_.targetRead = int32_t(uint32_t(done.data[0]) | uint32_t(done.data[1]) << 8 |
                                         uint32_t(done.data[2]) << 16 | uint32_t(done.data[3]) << 24);
        }
        readDone(now);
        return true;
    }
    const M::Status advanced = plainRead(readMode_) ? L::advanceRead(context_.read, done, now)
                                                    : L::advanceDriver(context_, done, now);
    if (!advanced) {
        finish(RouteFailure::CORRELATION, "advance", now);
        return true;
    }
    if (context_.read.state == M::ReadState::SUCCEEDED)
        readDone(now);
    else if (context_.read.state != M::ReadState::ACTIVE)
        finish(RouteFailure::SERVICE, "read", now);
    return true;
}
void BenchRoute::nextStep(uint64_t now) noexcept {
    (void)now;
    ++stepIndex_;
    record_.step = stepIndex_;
    subStep_ = 0;
    stepActive_ = readActive_ = false;
}
void BenchRoute::advanceStep(uint64_t now) noexcept {
    const Step& step = steps_[stepIndex_];
    switch (step.kind) {
    case STEP_STATE:
    case STEP_SECOND:
    case STEP_GROUP:
    case STEP_PRIOR:
    case STEP_TUNING:
    case STEP_SINGLE:
    case STEP_IDENTITY: {
        const uint8_t mode = step.kind == STEP_STATE || step.kind == STEP_SECOND ? READ_STATE
                             : step.kind == STEP_GROUP || step.kind == STEP_PRIOR ? READ_GROUP
                             : step.kind == STEP_TUNING                        ? READ_TUNING
                             : step.kind == STEP_IDENTITY                      ? READ_IDENTITY
                                                                               : READ_SINGLE;
        if (!readActive_ && step.kind == STEP_SECOND) {
            // The read before this one is the first of the pair; the check judges both.
            record_.first = record_.last;
            record_.firstPresent = true;
        }
        if (!readActive_ && !startRead(mode, step.arg, now))
            return;
        serviceRead(now);
        return;
    }
    case STEP_CHECK_MODE:
    case STEP_CHECK_POSITION:
    case STEP_CHECK_REARM:
    case STEP_CHECK_MOVE:
    case STEP_CHECK_RELEASED: {
        record_.checkPresent = true;
        record_.check = record_.last;
        const char* why = step.kind == STEP_CHECK_MODE       ? checkMode()
                          : step.kind == STEP_CHECK_POSITION ? checkPosition()
                          : step.kind == STEP_CHECK_REARM    ? checkRearm()
                          : step.kind == STEP_CHECK_MOVE     ? checkMove()
                                                             : checkReleased();
        record_.checkPassed = why == nullptr;
        if (why)
            finish(RouteFailure::PREREQUISITE, why, now);
        else
            nextStep(now);
        return;
    }
    case STEP_MODE_WRITE:
    case STEP_POSITION_WRITES:
    case STEP_WORD_WRITE:
        if (!stepActive_ && !startWrite(now))
            return;
        stepActive_ = true;
        serviceWrite(now);
        return;
    case STEP_ENABLE:
        startEnable(now);
        return;
    case STEP_STOP:
        startStop(record_.plan.stopKind, record_.plan.deadlineMs, record_.plan.bestEffortMs,
                  record_.plan.intervalMs, context_.read, now);
        return;
    case STEP_MOVE:
        startMove(now);
        return;
    case STEP_SETTINGS:
        serviceSettings(now);
        return;
    case STEP_OBSERVER_CHECK:
        if (!observerActive_ && !observerPending_ && !startObserverRead(FULL, now)) {
            finish(RouteFailure::INVALIDATED, "observer", now);
            return;
        }
        serviceObserverRead(now);
        return;
    case STEP_CHECK_ALARM:
        // requestClearAlarm judges its own prerequisites; this route refuses first unless the fresh read
        // reports the alarm it is to clear.
        record_.checkPresent = true;
        record_.check = record_.last;
        record_.checkPassed = (record_.last.blocks & 0x0d) == 0x0d && (record_.last.monitor & L::MONITOR_ALARM);
        if (!record_.checkPassed)
            finish(RouteFailure::PREREQUISITE, (record_.last.blocks & 0x0d) == 0x0d ? "no-alarm" : "snapshot", now);
        else
            nextStep(now);
        return;
    case STEP_CLEAR:
        startClear(now);
        return;
    case STEP_OBSERVE:
        serviceObserve(now);
        return;
    default:
        finish(RouteFailure::NONE, "none", now);
        return;
    }
}
bool BenchRoute::startRead(uint8_t mode, uint8_t arg, uint64_t now) noexcept {
    L::ReadTarget target;
    if (!currentTarget(target)) {
        finish(RouteFailure::INVALIDATED, "route", now);
        return false;
    }
    const uint64_t deadline = now + ROUTE_OPERATION_US < cutoffUs_ ? now + ROUTE_OPERATION_US : cutoffUs_;
    const uint32_t operation = deadline > now ? axis().beginOperation(deadline, now) : 0;
    if (!operation) {
        finish(RouteFailure::INVALIDATED, "session", now);
        return false;
    }
    L::ReadOptions options;
    options.queueTimeoutUs = ROUTE_QUEUE_US;
    options.responseTimeoutUs = ROUTE_RESPONSE_US;
    M::Status prepared;
    switch (mode) {
    case READ_STATE:
        prepared = L::prepareState(context_.read, target, operation, now, deadline, options, arg);
        break;
    case READ_IDENTITY:
        // Both bench specimens answer 1009 and 100A in one byte (packet 4), as the firmware's
        // identity experiments read them. A two-byte reply then fails, and nothing is moved.
        options.identityWidths = L::IdentityWidths::MEASURED_U8;
        prepared = L::prepareIdentity(context_.read, target, operation, now, deadline, options);
        break;
    case READ_GROUP:
        prepared = L::prepareDriverRead(context_, target, operation, now, deadline, options,
                                        static_cast<L::DriverGroup>(arg));
        break;
    case READ_TUNING:
        prepared = L::prepareTuningRead(context_, target, operation, now, deadline, options,
                                        static_cast<L::TuningGroup>(arg));
        break;
    default: {
        if (arg >= sizeof singles / sizeof singles[0]) {
            finish(RouteFailure::ADMISSION, "prepare", now);
            return false;
        }
        C::SdoTransfer& t = single_;
        t = C::SdoTransfer();
        t.channel = target.channel;
        t.operation = t.request = operation;
        t.step = 1;
        t.index = singles[arg].index;
        t.size = singles[arg].width;
        t.direction = C::SdoDirection::UPLOAD;
        t.readEffect = C::SdoReadEffect::UNQUALIFIED; // A one-use grant, never the accepted list.
        t.deadlineUs = deadline;
        t.queueTimeoutUs = options.queueTimeoutUs;
        t.responseTimeoutUs = options.responseTimeoutUs;
        singleAdmitted_ = false;
        break;
    }
    }
    if (!prepared) {
        finish(RouteFailure::ADMISSION, "prepare", now);
        return false;
    }
    readMode_ = mode;
    readArg_ = arg;
    readActive_ = true;
    return true;
}
void BenchRoute::serviceRead(uint64_t now) noexcept {
    if (pending_ || record_.terminal)
        return;
    if (readMode_ == READ_SINGLE) {
        if (!singleAdmitted_)
            admit(single_, now);
        return;
    }
    L::PreparedRead next;
    const M::Status yielded = plainRead(readMode_) ? L::nextRead(context_.read, now, next)
                                                   : L::nextDriver(context_, now, next);
    if (!yielded) {
        finish(now >= context_.read.deadlineUs ? RouteFailure::DEADLINE : RouteFailure::ADMISSION, "next", now);
        return;
    }
    if (!next.ready) {
        if (context_.read.state == M::ReadState::SUCCEEDED)
            readDone(now);
        else
            finish(RouteFailure::SERVICE, "read", now);
        return;
    }
    admit(next.transfer, now);
}
void BenchRoute::readDone(uint64_t now) noexcept {
    if (readMode_ == READ_STATE && !snapshot(readArg_, now)) {
        finish(RouteFailure::CORRELATION, "snapshot", now);
        return;
    }
    L::ReadTarget target;
    if (readMode_ == READ_IDENTITY &&
        (!currentTarget(target) || !L::getIdentity(context_.read, target, now, ROUTE_OPERATION_US, identity_))) {
        finish(RouteFailure::CORRELATION, "identity", now);
        return;
    }
    readActive_ = false;
    if (steps_[stepIndex_].kind == STEP_PRIOR)
        work_->prior = context_; // Assigned in place: the next read reuses context_.
    if (steps_[stepIndex_].kind == STEP_OBSERVE)
        observeDone(now);
    else
        nextStep(now);
}
bool BenchRoute::snapshot(uint8_t mask, uint64_t now) noexcept {
    L::ReadTarget target;
    if (!currentTarget(target))
        return false;
    // ERROR_HISTORY has no RouteSnapshot field: it is kept apart, and a read of it alone leaves the
    // latest snapshot as it was. The library publishes only the active entries; nothing else is copied.
    if (mask & HISTORY_STATE) {
        if (!L::getStateBlock(context_.read, L::StateBlock::ERROR_HISTORY, target, now, ROUTE_OPERATION_US, block_) ||
            block_.errorCount > 4)
            return false;
        record_.historyPresent = true;
        record_.historyCount = block_.errorCount;
        for (unsigned i = 0; i < 4; ++i)
            record_.history[i] = i < block_.errorCount ? block_.errorHistory[i] : 0;
        if (mask == HISTORY_STATE)
            return true;
    }
    RouteSnapshot s;
    uint64_t feedback = feedbackUs_;
    uint16_t logical = 0;
    uint32_t digital = 0;
    const L::StateBlock blocks[] = {L::StateBlock::ERRORS, L::StateBlock::MONITORS, L::StateBlock::STATUS,
                                    L::StateBlock::FEEDBACK, L::StateBlock::INPUTS};
    for (L::StateBlock block : blocks) {
        const uint8_t bit = uint8_t(1u << unsigned(block));
        if (!(mask & bit))
            continue;
        if (!L::getStateBlock(context_.read, block, target, now, ROUTE_OPERATION_US, block_))
            return false;
        s.blocks = uint8_t(s.blocks | bit);
        switch (block) {
        case L::StateBlock::ERRORS:
            s.errorRegister = block_.errorRegister;
            s.errorCode = block_.errorCode;
            break;
        case L::StateBlock::MONITORS:
            s.monitor = block_.monitor;
            logical = block_.logicalInputs;
            break;
        case L::StateBlock::INPUTS:
            digital = block_.digitalInputs;
            break;
        case L::StateBlock::STATUS:
            s.statusword = block_.statusword;
            s.mode = block_.modeRaw;
            break;
        default:
            s.position = block_.actualPosition;
            s.velocity = block_.actualVelocity;
            // The conservative earliest bound: a witness from it can only look older than it is.
            feedback = block_.window.earliestUs;
            break;
        }
    }
    record_.last = s;
    feedbackUs_ = feedback;
    // 2101 and 60FD are kept only from a read that names INPUTS: IO_DI3's look after its readback.
    if (mask & (1u << unsigned(L::StateBlock::INPUTS))) {
        record_.inputsPresent = true;
        record_.logicalInputs = logical;
        record_.digitalInputs = digital;
    }
    return true;
}
void BenchRoute::admit(const C::SdoTransfer& transfer, uint64_t now) noexcept {
    // A reply can precede its final TX report; only the owner can say the last frame retired.
    if (!firmware_.owner().localTxRetired(slot()))
        return;
    // No exchange starts unless its whole queue and response budget fits before its own operation
    // deadline, which is never after the route cutoff: one cut there with its request possibly sent left
    // node 2's route unresolved until an MCU restart (trial 213). The route ends here instead, with
    // nothing in flight. A 000B is a stop word, and a stop word sent late is better than none.
    const bool stopWord = transfer.direction == C::SdoDirection::DOWNLOAD && transfer.index == 0x6040 &&
                          transfer.size == 2 && transfer.data[0] == 0x0b && transfer.data[1] == 0;
    const uint64_t budget = uint64_t(transfer.queueTimeoutUs) + transfer.responseTimeoutUs;
    if (!stopWord && (transfer.deadlineUs <= now || transfer.deadlineUs - now < budget)) {
        finish(RouteFailure::DEADLINE, transfer.deadlineUs >= cutoffUs_ ? "cutoff" : "operation", now);
        return;
    }
    // Every unqualified upload and every write of this route's own takes an exact one-use grant,
    // as the PDO and heartbeat experiments do. The session's enable and stop words do not.
    const bool upload = transfer.direction == C::SdoDirection::UPLOAD;
    if ((!upload || transfer.readEffect == C::SdoReadEffect::UNQUALIFIED) &&
        !(grantedOperation_ == transfer.operation && grantedStep_ == transfer.step)) {
        L::ReadTarget target;
        if (grant_ == UINT32_MAX || !currentTarget(target) ||
            !(upload ? services_.authorizeUploadExperiment(slot(), transfer, target.nodeGeneration, grant_ + 1, now)
                     : services_.authorizeDownloadExperiment(slot(), transfer, target.nodeGeneration, grant_ + 1,
                                                             now))) {
            finish(RouteFailure::AUTHORIZATION, "grant", now);
            return;
        }
        ++grant_;
        grantedOperation_ = transfer.operation;
        grantedStep_ = transfer.step;
    }
    const BusAdmission admission = axis().admitPrepared(transfer, now);
    if (admission == BusAdmission::RESULTS_FULL)
        return; // Unsent: the same work, under the same grant, is offered again.
    if (admission != BusAdmission::ACCEPTED) {
        record_.admission = uint8_t(admission);
        finish(RouteFailure::ADMISSION, "owner", now);
        return;
    }
    expected_ = transfer;
    pending_ = true;
    if (readMode_ == READ_SINGLE && transfer.direction == C::SdoDirection::UPLOAD)
        singleAdmitted_ = true;
    if (transfer.direction == C::SdoDirection::DOWNLOAD && transfer.index == 0x6060)
        modeAdmitted_[slot()] = true; // From here the mode may change, acknowledged or not.
}
bool BenchRoute::startWrite(uint64_t now) noexcept {
    // A planned stop refused for the cutoff would leave the trigger's motion running: refuse before
    // the first write unless the stop still fits when it is asked for at its latest.
    if (steps_[stepIndex_].kind == STEP_POSITION_WRITES && record_.plan.stopKind &&
        now + positionStopUs(record_.plan) > cutoffUs_) {
        finish(RouteFailure::REFUSED, "deadline", now);
        return false;
    }
    const uint64_t deadline = now + ROUTE_OPERATION_US < cutoffUs_ ? now + ROUTE_OPERATION_US : cutoffUs_;
    const uint32_t operation = deadline > now ? axis().beginOperation(deadline, now) : 0;
    if (!operation) {
        finish(RouteFailure::INVALIDATED, "session", now);
        return false;
    }
    writeOperation_ = operation;
    writeDeadlineUs_ = deadline;
    subStep_ = 0;
    readMode_ = READ_STATE; // Not a read; settle uses the transfer direction.
    return true;
}
bool BenchRoute::writeTransfer(C::SdoTransfer& output) const noexcept {
    L::ReadTarget target;
    if (!currentTarget(target))
        return false;
    const RoutePlan& p = record_.plan;
    C::SdoTransfer t;
    t.channel = target.channel;
    t.operation = t.request = writeOperation_;
    t.step = subStep_ + 1u;
    t.direction = C::SdoDirection::DOWNLOAD;
    uint32_t value = 0;
    if (steps_[stepIndex_].kind == STEP_MODE_WRITE) {
        t.index = 0x6060; // P2: 2F 60 60 00 01 00 00 00. EDS U8 WO against the manuals' I8 RW.
        t.size = 1;
        value = 1;
    } else if (steps_[stepIndex_].kind == STEP_WORD_WRITE) {
        const uint8_t word = steps_[stepIndex_].arg;
        if (subStep_ || word >= sizeof WORDS / sizeof WORDS[0])
            return false; // One word, never a second.
        t.index = L::OBJECT_6040_00.index; // 2B 40 60 00 0F 00 00 00, 0B, 06 or 07.
        t.size = L::OBJECT_6040_00.widthBytes;
        value = WORDS[word];
    } else {
        const L::ObjectDescriptor* const objects[] = {&L::OBJECT_607A_00, &L::OBJECT_6081_00,
                                                      &L::OBJECT_6083_00, &L::OBJECT_6084_00,
                                                      &L::OBJECT_6040_00};
        if (subStep_ >= 5)
            return false;
        t.index = objects[subStep_]->index;
        t.size = objects[subStep_]->widthBytes;
        // 15 to 31 for an absolute target, 15 to 95 for a relative one (bit 6, EN86/ZH86 p12).
        const uint32_t trigger = p.kind == RouteKind::RELATIVE ? 0x005f : 0x001f;
        const uint32_t values[] = {uint32_t(p.target), p.rpm, p.rampUpMs, p.rampDownMs, trigger};
        value = values[subStep_];
    }
    for (unsigned i = 0; i < t.size; ++i)
        t.data[i] = uint8_t(value >> (8 * i));
    t.deadlineUs = writeDeadlineUs_;
    t.queueTimeoutUs = ROUTE_QUEUE_US;
    t.responseTimeoutUs = ROUTE_RESPONSE_US;
    // The public codec checks the exact expedited request these values make, as MotionPreview does.
    M::Frame frame;
    frame.id = t.channel.clientToServerId;
    frame.dlc = frame.payloadLength = 8;
    frame.data[0] = uint8_t(0x23u | (4u - t.size) << 2);
    frame.data[1] = uint8_t(t.index);
    frame.data[2] = uint8_t(t.index >> 8);
    frame.data[3] = t.subindex;
    std::memcpy(frame.data + 4, t.data, t.size);
    C::SdoExpectation expected;
    expected.channel = t.channel;
    expected.index = t.index;
    expected.subindex = t.subindex;
    expected.width = t.size;
    expected.direction = C::SdoDirection::DOWNLOAD;
    if (!C::checkRequest(frame, expected))
        return false;
    output = t;
    return true;
}
void BenchRoute::serviceWrite(uint64_t now) noexcept {
    if (pending_ || record_.terminal)
        return;
    const Step& step = steps_[stepIndex_];
    // The check step judged the predecessor on an earlier turn. It must still hold now, at each
    // offer of the first write of each step, or nothing is written.
    if (subStep_ == 0) {
        if (const char* why = predecessor(step.kind, step.arg)) {
            finish(RouteFailure::PREREQUISITE, why, now);
            return;
        }
    }
    C::SdoTransfer t;
    if (!writeTransfer(t)) {
        finish(RouteFailure::ADMISSION, "codec", now);
        return;
    }
    admit(t, now);
    if (pending_ && subStep_ == 0 && step.kind != STEP_MODE_WRITE) {
        // From here no earlier trigger or halt can be the latest word.
        triggerOperation_[slot()] = haltOperation_[slot()] = 0;
    }
    if (pending_ && subStep_ == 0 && step.kind == STEP_POSITION_WRITES)
        commandedKnown_[slot()] = false; // This 607A may reach the drive unacknowledged.
}
// The session history each write needs, judged at its check step and again when the write is offered.
// MODE_ONE: no write or controlword of this session yet. POSITION and RELATIVE: the session's
// acknowledged 000F. REARM: the acknowledged predecessor it names, of a finished record. WORD67's
// 0007: its own acknowledged 0006. RELEASED_QUICK's 000B and WORD67's 0006 follow any word, or none,
// but only the one recorded at admission: another writer since would make the check's reads stale.
const char* BenchRoute::predecessor(uint8_t write, uint8_t word) const noexcept {
    const C::SdoCompletion* control = axis().latestControl();
    if (write == STEP_MODE_WRITE) {
        if (axis().motionHistoryWritten() || modeAdmitted_[slot()])
            return "history";
        return control ? "control" : nullptr;
    }
    if (write == STEP_POSITION_WRITES)
        return control && control->transfer.data[0] == 0x0f && control->transfer.data[1] == 0 ? nullptr : "control";
    if (record_.plan.kind == RouteKind::REARM)
        return rearmable(slot(), axis(), record_.plan.predecessor) ? nullptr : "control";
    if (word == WORD_SEVEN)
        return control && control->transfer.operation == writtenOperation_ && control->completedUs == writtenUs_ &&
                       control->transfer.data[0] == 0x06 && control->transfer.data[1] == 0
                   ? nullptr
                   : "control";
    const RouteRecord& r = record_;
    const bool same = control ? r.priorPresent && control->transfer.operation == r.priorOperation &&
                                    control->completedUs == r.priorUs
                              : !r.priorPresent;
    return same ? nullptr : "control";
}
const char* BenchRoute::checkMode() const noexcept {
    const RouteSnapshot& s = record_.last;
    if (const char* why = predecessor(STEP_MODE_WRITE))
        return why;
    if ((s.blocks & FULL) != FULL)
        return "snapshot";
    if (s.errorRegister)
        return "error-register";
    if (s.errorCode)
        return "fault-code";
    if (s.monitor & L::MONITOR_ALARM)
        return "alarm";
    if (s.monitor & L::MONITOR_RUNNING)
        return "running";
    if (s.velocity)
        return "velocity";
    if (s.mode != 0)
        return "mode"; // The packet 11 baseline read mode 0; anything else changed since.
    return nullptr;
}
// A fresh full read of an idle, enabled-looking drive in mode 1, from POSITION's prerequisite list.
// No mask beyond the documented 2100 bits and 6041 low bits is judged here.
const char* BenchRoute::checkIdle(const RouteSnapshot& s) const noexcept {
    if ((s.blocks & FULL) != FULL)
        return "snapshot";
    if (s.errorRegister)
        return "error-register";
    if (s.errorCode)
        return "fault-code";
    if (s.monitor & L::MONITOR_ALARM)
        return "alarm";
    if (s.monitor & L::MONITOR_RUNNING)
        return "running";
    if (s.monitor & L::MONITOR_RELEASED)
        return "released";
    if (s.mode != 1)
        return "mode";
    if ((s.statusword & L::STATUS_LOW_BITS) != L::STATUS_LOW_BITS)
        return "status";
    if (s.velocity)
        return "velocity";
    return nullptr;
}
const char* BenchRoute::checkPosition() const noexcept {
    if (const char* why = predecessor(STEP_POSITION_WRITES))
        return why;
    if (const char* why = checkIdle(record_.last))
        return why;
    if (record_.last.position != record_.plan.startPosition)
        return "position";
    return nullptr;
}
// Nothing about the position is asked: the 000F follows a finished experiment whose travel is the
// unknown. Every value read is kept in TRIAL_CHECK.
const char* BenchRoute::checkRearm() const noexcept {
    if (const char* why = predecessor(STEP_WORD_WRITE, WORD_REARM))
        return why;
    return checkIdle(record_.last);
}
// The route's own fail-closed check before the library's: the library judges the same 000F, mode,
// low bits and 2100 again with its own age, and the acceptance report against the host's mask. A
// relative witness also claims standstill, so its first read must pass the same check: two fresh reads
// with 606C 0 and the same reviewed 6064, running clear in both.
const char* BenchRoute::checkMove() const noexcept {
    if (axis().moveFenced())
        return "fenced";
    if (record_.plan.relativeBase) {
        if (!record_.firstPresent)
            return "snapshot";
        if (const char* why = checkIdle(record_.first))
            return why;
        if (record_.first.position != record_.plan.startPosition)
            return "position";
    }
    return checkPosition();
}
// RELEASED_QUICK's and WORD67's prerequisite list, on both fresh reads. No predecessor: the drive's
// own report is the evidence of the released state, whatever led to it.
const char* BenchRoute::checkReleased() const noexcept {
    if (!record_.firstPresent)
        return "snapshot";
    const RouteSnapshot* const reads[] = {&record_.first, &record_.check};
    for (const RouteSnapshot* s : reads) {
        if ((s->blocks & FULL) != FULL)
            return "snapshot";
        if (s->errorRegister)
            return "error-register";
        if (s->errorCode)
            return "fault-code";
        if (s->monitor & L::MONITOR_ALARM)
            return "alarm";
        if (!(s->monitor & L::MONITOR_RELEASED))
            return "unreleased";
        if (s->mode != 1)
            return "mode";
        if (s->statusword & L::STATUS_LOW_BITS)
            return "status";
        if (s->velocity)
            return "velocity";
    }
    // 2100 bit 2 stayed set after a 000B stopped a 30 rpm move (trial 213), with the target not
    // reached: there it reports no motion. So it is admitted only with 6064 unchanged across the reads.
    if (((record_.first.monitor | record_.check.monitor) & L::MONITOR_RUNNING) &&
        record_.first.position != record_.check.position)
        return "running";
    return nullptr;
}
// The acknowledged latest word of a finished record of this object on that node, as the host named
// it: 1 POSITION's 001F, 2 that record's planned HALT 010F, 3 RELATIVE's 005F. Its operation and
// completion time identify that terminal, so a later word of any writer is never mistaken for it.
bool BenchRoute::rearmable(uint8_t index, const AxisSession& session, uint8_t predecessor) const noexcept {
    const uint16_t words[] = {0x001f, 0x010f, 0x005f};
    if (predecessor < 1 || predecessor > 3)
        return false;
    const bool halt = predecessor == 2;
    const uint32_t operation = halt ? haltOperation_[index] : triggerOperation_[index];
    const uint64_t at = halt ? haltUs_[index] : triggerUs_[index];
    const uint16_t word = words[predecessor - 1];
    const C::SdoCompletion* c = session.latestControl();
    return c && operation && c->outcome == C::SdoOutcome::SUCCESS && c->protocolAcknowledged &&
           !c->effectUncertain && c->transfer.index == 0x6040 && !c->transfer.subindex && c->transfer.size == 2 &&
           c->transfer.data[0] == uint8_t(word) && c->transfer.data[1] == uint8_t(word >> 8) &&
           c->transfer.operation == operation && c->completedUs == at;
}
void BenchRoute::startEnable(uint64_t now) noexcept {
    const RoutePlan& p = record_.plan;
    L::ReadTarget target;
    const SequenceId id = axis().sequence();
    const Sequence* live = firmware_.owner().sequence(id);
    if (!currentTarget(target) || !live) {
        finish(RouteFailure::INVALIDATED, "session", now);
        return;
    }
    // The reservation and operation are binding facts the session holds. Every observation and
    // reviewed value comes from the host's command; none is made up here.
    L::EnableEvidence evidence;
    evidence.gate = p.attended ? L::EnableGate::ATTENDED_EXPERIMENT : L::EnableGate::CLOSED;
    evidence.trial = p.trial;
    evidence.reservation.target = target;
    evidence.reservation.model = L::SettingsModel::CL86_C;
    evidence.reservation.owner = firmware_.owner().identity();
    evidence.reservation.sequence = uint32_t(id.slot) + 1;
    evidence.reservation.generation = id.generation;
    evidence.reservation.acquiredUs = live->acquiredUs;
    evidence.operation = axis().nextOperation();
    evidence.expiresUs = now + 1000000;
    evidence.start = p.start == 1 ? L::EnableStart::NO_MOTION_WRITE_RECORDED : L::EnableStart::UNKNOWN;
    evidence.powerUpShaft = p.shaft == 1   ? L::ShaftObservation::FREE
                            : p.shaft == 2 ? L::ShaftObservation::HOLDING
                                           : L::ShaftObservation::NONE;
    evidence.reviewed = true;
    evidence.reviewedStatusword = p.reviewedStatus;
    evidence.reviewedPositionTarget = p.reviewedPositionTarget;
    evidence.reviewedVelocityTarget = p.reviewedVelocityTarget;
    L::ActionOptions options;
    options.service.queueTimeoutUs = ROUTE_QUEUE_US;
    options.service.responseTimeoutUs = ROUTE_RESPONSE_US;
    options.observationIntervalUs = uint64_t(p.intervalMs) * 1000;
    options.maximumAgeUs = 9 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
    options.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    const uint64_t deadline = now + uint64_t(p.deadlineMs) * 1000;
    if (deadline > cutoffUs_) {
        finish(RouteFailure::REFUSED, "deadline", now);
        return;
    }
    const M::Status status = axis().requestEnable(context_.read, evidence, options, deadline, now);
    if (!status) {
        record_.statusPresent = true;
        record_.statusCode = uint8_t(status.code);
        record_.statusDetail = status.detail;
        record_.statusMessage = status.msg ? status.msg : "";
        finish(RouteFailure::REFUSED, "enable", now);
        return;
    }
    record_.enableRequested = true;
    record_.actionRequestedUs = now;
    actionStarted_ = true;
}
void BenchRoute::startStop(uint8_t kind, uint32_t deadlineMs, uint32_t bestEffortMs, uint32_t intervalMs,
                           const L::ReadContext& state, uint64_t now) noexcept {
    StopPolicy policy;
    policy.options.service.queueTimeoutUs = ROUTE_QUEUE_US;
    policy.options.service.responseTimeoutUs = ROUTE_RESPONSE_US;
    policy.options.observationIntervalUs = uint64_t(intervalMs) * 1000;
    // Seven fields at the queue and response budgets: the library offers a QUICK_STOP's standstill
    // observation by 6064 and 606C only with an age cap that covers a seven-field cycle.
    policy.options.maximumAgeUs = 7 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
    policy.deadlineUs = now + uint64_t(deadlineMs) * 1000;
    policy.bestEffortDeadlineUs = bestEffortMs ? now + uint64_t(bestEffortMs) * 1000 : 0;
    record_.stopRequestKind = kind;
    // A move keeps its own request time until the stop is under way; the stop is matched to its own.
    const bool duringMove = record_.moveRequested;
    stopAttempted_ = true;
    if (policy.deadlineUs > cutoffUs_ || policy.bestEffortDeadlineUs > cutoffUs_) {
        // During a move nothing was asked of the session: the move runs to its own terminal first.
        if (duringMove)
            endAfterSettling(RouteFailure::REFUSED, "deadline");
        else
            finish(RouteFailure::REFUSED, "deadline", now);
        return;
    }
    const StopPath path = axis().requestStop(kind == 1 ? L::StopKind::HALT : L::StopKind::QUICK_STOP,
                                             state, policy, now);
    record_.stopPathRequested = uint8_t(path);
    if (path == StopPath::NONE) {
        // Even a refused stop ends an active move; its in-flight request settles into the move.
        if (duringMove)
            endAfterSettling(RouteFailure::REFUSED, "stop");
        else
            finish(RouteFailure::REFUSED, "stop", now);
        return;
    }
    record_.actionRequestedUs = now;
    record_.stopRequested = true;
    actionStarted_ = true;
}
void BenchRoute::startMove(uint64_t now) noexcept {
    const RoutePlan& p = record_.plan;
    L::ReadTarget target;
    if (!currentTarget(target) || axis().moveActive()) {
        finish(RouteFailure::INVALIDATED, "session", now);
        return;
    }
    // The session's own coordinates: exact STEPS in the native frame and rpm as motor turns per
    // minute need no scale, so none is claimed. Unchanged while this node's move borrows it. This is
    // the only writer of these members and it sets the same fields each time, so every other field
    // keeps its default from construction; nothing is reset through a stack temporary. A relative move
    // admits only the basis the host's record measured; an absolute move admits none, as before.
    const bool relative = p.relativeBase != 0;
    const M::RelativeBasis basis = p.relativeBase == 2 ? M::RelativeBasis::COMMANDED : M::RelativeBasis::ACTUAL;
    M::AxisConfig& axisConfig = moveAxes_[slot()];
    axisConfig.binding = axis().binding();
    axisConfig.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    axisConfig.nativeVelocityTime = M::TimeUnit::MINUTE;
    axisConfig.supportedRelativeBases = relative ? uint8_t(1u << unsigned(basis)) : uint8_t(0);
    // Every evidence value is the host's, from its reviewed measurement record; none is made up here.
    L::MoveEvidence& evidence = moveEvidence_;
    evidence.gate = L::MoveGate::MEASURED;
    evidence.record = p.record;
    evidence.model = L::SettingsModel::CL86_C;
    evidence.firmware = p.firmware;
    evidence.acceptance.source = static_cast<L::MoveSignalSource>(p.acceptSource);
    evidence.acceptance.mask = p.acceptMask;
    evidence.acceptance.value = p.acceptValue;
    evidence.completion.source = static_cast<L::MoveSignalSource>(p.completeSource);
    evidence.completion.mask = p.completeMask;
    evidence.completion.value = p.completeValue;
    evidence.rule = static_cast<L::CompletionRule>(p.rule);
    evidence.relativeBase = !relative            ? L::RelativeBase::UNMEASURED
                            : p.relativeBase == 1 ? L::RelativeBase::ACTUAL
                                                  : L::RelativeBase::COMMANDED;
    M::MoveRequest& request = moveRequest_;
    request.position.value = M::Rational(p.target);
    request.position.relative = relative;
    request.position.basis = basis;
    request.position.configurationGeneration = axisConfig.binding.configurationGeneration;
    request.speed.value = M::Rational(int64_t(p.rpm));
    request.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    request.speed.configurationGeneration = axisConfig.binding.configurationGeneration;
    L::MoveProfile& profile = moveProfile_;
    profile.rampUpTimeMs = p.rampUpMs;
    profile.rampDownTimeMs = p.rampDownMs;
    L::MoveOptions& options = moveOptions_;
    options.service.queueTimeoutUs = ROUTE_QUEUE_US;
    options.service.responseTimeoutUs = ROUTE_RESPONSE_US;
    options.observationIntervalUs = uint64_t(p.intervalMs) * 1000;
    options.maximumAgeUs = 9 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
    // The planned stop is asked for before the move's deadline. Refused for the cutoff it would leave the
    // move running on, so refuse here, before any move write, unless its whole span fits behind that.
    const uint64_t deadline = now + uint64_t(p.deadlineMs) * 1000;
    if (deadline + stopSpanUs(p) > cutoffUs_) {
        finish(RouteFailure::REFUSED, "deadline", now);
        return;
    }
    M::Status status;
    if (relative) {
        // The witness of the request's basis. Idle and stationary rest on this route's check: two fresh
        // full reads after the latest acknowledged 000F, each with running, alarm and released clear,
        // mode 1, low bits 111, 606C 0 and the reviewed 6064. It is observed at the second read's
        // earliest FEEDBACK bound and expires with the move's own age cap, so it can only look older
        // than it is. ACTUAL is that 6064, read back; COMMANDED is the host's reviewed commanded
        // position, which no read here can show, so it is the host's stated assumption.
        M::AxisReference& witness = moveReferences_[slot()];
        witness.binding = axisConfig.binding;
        witness.referenceGeneration = axisConfig.referenceGeneration;
        witness.idle = witness.stationary = witness.nativeKnown = true;
        witness.nativePosition = p.relativeBase == 1 ? record_.check.position : p.base;
        witness.basis = basis;
        witness.source = p.relativeBase == 1 ? M::ScaleSource::READBACK : M::ScaleSource::ASSUMED;
        witness.observedUs = feedbackUs_;
        witness.nowUs = now;
        witness.maximumAgeUs = options.maximumAgeUs;
        record_.witnessBasis = p.relativeBase;
        record_.witnessPosition = int32_t(witness.nativePosition);
        status = axis().requestMoveRelative(context_.read, identity_, evidence, request, profile, nullptr, axisConfig,
                                            witness, options, deadline, now);
    } else {
        status = axis().requestMove(context_.read, identity_, evidence, request, profile, nullptr, axisConfig,
                                    nullptr, options, deadline, now);
    }
    if (!status) {
        record_.statusPresent = true;
        record_.statusCode = uint8_t(status.code);
        record_.statusDetail = status.detail;
        record_.statusMessage = status.msg ? status.msg : "";
        finish(RouteFailure::REFUSED, "move", now);
        return;
    }
    commandedKnown_[slot()] = false; // The move's 607A may reach the drive unacknowledged.
    observerNextUs_ = now;           // The observer's first cycle at once.
    record_.moveRequested = true;
    record_.actionRequestedUs = record_.moveRequestedUs = now;
    actionStarted_ = true;
}
// SETTINGS_2202 and IO_DI3: the one candidate through the library, on the session's reservation, from
// the prior group and the clean STATUS and MONITORS read the two earlier steps left.
void BenchRoute::startSettings(uint64_t now) noexcept {
    RouteWork& w = *work_;
    const RoutePlan& p = record_.plan;
    L::ReadTarget target;
    const SequenceId id = axis().sequence();
    const Sequence* held = firmware_.owner().sequence(id);
    if (!currentTarget(target) || !held) {
        finish(RouteFailure::INVALIDATED, "session", now);
        return;
    }
    // The session's reservation, as startEnable builds it: acquired at its open, before the prior reads.
    // Assigned field by field, in place, so the transaction's frozen copy is the same token throughout.
    L::AxisReservation& live = w.live;
    live.target = target;
    live.model = L::SettingsModel::CL86_C;
    live.owner = firmware_.owner().identity();
    live.sequence = uint32_t(id.slot) + 1;
    live.generation = id.generation;
    live.acquiredUs = held->acquiredUs;
    w.binding = target.channel.binding;
    record_.configurationBefore = record_.configurationAfter = uint32_t(w.binding.configurationGeneration);
    publishGroup(0, &w.prior, now);
    if (!record_.groupPresent[0]) {
        finish(RouteFailure::PREREQUISITE, "prior", now);
        return;
    }
    L::IoSettings io;
    if (p.kind == RouteKind::IO_DI3) {
        // One thing changes; every other field is the prior readback's. A stored undocumented function
        // code cannot be named by a candidate, so the library would replace it: refused instead.
        const L::IoObservation& prior = w.io;
        for (unsigned i = 0; i < 7; ++i) {
            if (prior.inputFunctions[i] == L::InputFunction::UNKNOWN)
                return finish(RouteFailure::PREREQUISITE, "undocumented", now);
            io.functions[i] = prior.inputFunctions[i];
        }
        io.invertedInputs = uint16_t(prior.inputPolarityRaw & L::INPUT_POLARITY_BITS);
        const bool user0 = prior.inputFunctions[DI3] == L::InputFunction::USER0;
        const bool undefined = prior.inputFunctions[DI3] == L::InputFunction::UNDEFINED;
        const bool inverted = (prior.inputPolarityRaw >> DI3) & 1u;
        // Each case changes its field from the one state it names, never from another.
        if ((p.code == 1 && !undefined) || (p.code == 2 && !user0) || (p.code == 3 && inverted) ||
            (p.code == 4 && !inverted))
            return finish(RouteFailure::PREREQUISITE, "di3", now);
        if (p.code <= 2)
            io.functions[DI3] = p.code == 1 ? L::InputFunction::USER0 : L::InputFunction::UNDEFINED;
        else
            io.invertedInputs = uint16_t(p.code == 3 ? io.invertedInputs | 1u << DI3 : io.invertedInputs & ~(1u << DI3));
    }
    const uint64_t deadline = now + ROUTE_OPERATION_US < cutoffUs_ ? now + ROUTE_OPERATION_US : cutoffUs_;
    const uint32_t operation = deadline > now ? axis().beginOperation(deadline, now) : 0;
    if (!operation) {
        finish(RouteFailure::INVALIDATED, "session", now);
        return;
    }
    L::ReadOptions options;
    options.queueTimeoutUs = ROUTE_QUEUE_US;
    options.responseTimeoutUs = ROUTE_RESPONSE_US;
    M::Status status;
    if (p.kind == RouteKind::SETTINGS_2202) {
        L::SoftLimitSettings limits;
        limits.enable = static_cast<L::SoftLimitEnable>(p.code);
        status = L::prepareSoftLimitSettings(w.transaction, live, target, w.prior, context_.read, limits, operation, now,
                                             deadline, options, SETTINGS_AGE_US);
    } else {
        status = L::prepareIoSettings(w.transaction, live, target, w.prior, context_.read, io, operation, now, deadline,
                                      options, SETTINGS_AGE_US);
    }
    if (!status) {
        record_.statusPresent = true;
        record_.statusCode = uint8_t(status.code);
        record_.statusDetail = status.detail;
        record_.statusMessage = status.msg ? status.msg : "";
        finish(RouteFailure::REFUSED, "settings", now);
        return;
    }
    record_.settingsPrepared = true;
    const L::DriverSettingsContext& tx = w.transaction;
    // An unchanged candidate succeeds at once from the fresh prior, with nothing to write.
    record_.settingsNoop = tx.state == L::SettingsState::SUCCEEDED && !tx.changed;
    stepActive_ = true;
}
// One turn of the transaction: move the owner's reservation to the new epoch before the readback, offer
// the next write or readback upload, and end the step once the transaction is terminal.
void BenchRoute::serviceSettings(uint64_t now) noexcept {
    if (pending_ || record_.terminal)
        return;
    if (!stepActive_) {
        startSettings(now);
        return;
    }
    RouteWork& w = *work_;
    L::DriverSettingsContext& tx = w.transaction;
    if (tx.state != L::SettingsState::WRITING && tx.state != L::SettingsState::READING_BACK) {
        const bool followed = settleSettings(now);
        if (tx.state != L::SettingsState::SUCCEEDED)
            finish(RouteFailure::SERVICE, tx.uncertain ? "uncertain" : "settings", now);
        else if (!followed)
            finish(RouteFailure::INVALIDATED, "configuration", now);
        else if (record_.settingsNoop)
            finish(RouteFailure::NONE, "unchanged", now); // Nothing was sent, and nothing follows.
        else
            nextStep(now);
        return;
    }
    // The readback is at the new epoch, after the write's result was released: the owner's reservation
    // moves there first, as the library requires before it yields a readback upload.
    if (tx.state == L::SettingsState::READING_BACK && !ownerFollowed_) {
        if (!firmware_.owner().advanceConfiguration(axis().sequence(), w.binding, now)) {
            L::advanceDriver(tx, M::ReadOutcome::INVALIDATED, now);
            settleSettings(now);
            finish(RouteFailure::INVALIDATED, "configuration", now);
            return;
        }
        ownerFollowed_ = true;
    }
    L::PreparedRead next;
    if (!L::nextDriver(tx, w.live, w.binding, now, next)) {
        const bool late = now >= tx.deadlineUs;
        L::advanceDriver(tx, late ? M::ReadOutcome::DEADLINE : M::ReadOutcome::INVALIDATED, now);
        finish(late ? RouteFailure::DEADLINE : RouteFailure::ADMISSION, "next", now);
        return;
    }
    if (next.ready)
        admit(next.transfer, now);
}
// Record the transaction's result once, publish a matched readback while the reservation it was made
// under is still the session's, and move the owner and the session to the epoch a possible write
// reached. False when they cannot follow: later reads on this node would be at a withdrawn epoch.
bool BenchRoute::settleSettings(uint64_t now) noexcept {
    if (settingsSettled_)
        return !record_.settingsEffects || record_.configurationFollowed;
    settingsSettled_ = true;
    RouteWork& w = *work_;
    const L::DriverSettingsContext& tx = w.transaction;
    record_.settingsState = uint8_t(tx.state);
    record_.settingsOutcome = uint8_t(tx.outcome);
    record_.settingsChanged = tx.changed;
    record_.settingsConfirmed = tx.confirmed;
    record_.settingsEffects = tx.effectsPossible;
    record_.settingsUncertain = tx.uncertain;
    record_.configurationAfter = uint32_t(w.binding.configurationGeneration);
    if (tx.state == L::SettingsState::SUCCEEDED)
        publishGroup(1, nullptr, now);
    if (record_.configurationAfter == record_.configurationBefore)
        return true;
    record_.configurationFollowed = followConfiguration(now);
    return record_.configurationFollowed;
}
// The owner's reservation, then the session: AxisSession offers no epoch change of its own for a
// settings write, so it is closed, which keeps its object-life records (written motion, fences, blocked
// clears), and opened again on the current binding. A latest controlword does not survive an epoch
// change in any case. Nothing is sent.
bool BenchRoute::followConfiguration(uint64_t now) noexcept {
    AxisSession& session = axis();
    if (!ownerFollowed_) {
        if (!firmware_.owner().advanceConfiguration(session.sequence(), work_->binding, now))
            return false;
        ownerFollowed_ = true;
    }
    L::ReadTarget target;
    if (session.busy() || !session.close() || !currentTarget(target) ||
        !M::sameBinding(target.channel.binding, work_->binding))
        return false;
    return firmware_.app().openAxis(session, target, L::SettingsModel::CL86_C, now) != nullptr;
}
// Raw group values for the record: the prior read (`read`), or the transaction's matched readback.
void BenchRoute::publishGroup(unsigned which, const L::DriverContext* read, uint64_t now) noexcept {
    RouteWork& w = *work_;
    L::ReadTarget target;
    if (!currentTarget(target))
        return;
    RouteRecord& r = record_;
    if (r.plan.kind == RouteKind::SETTINGS_2202) {
        const M::Status ok = read ? L::getDriver(*read, target, now, SETTINGS_AGE_US, w.limits)
                                  : L::getDriver(w.transaction, w.live, target, now, SETTINGS_AGE_US, w.limits);
        if (!ok || w.limits.group != L::DriverGroup::SOFT_LIMITS)
            return;
        r.limitSelector[which] = w.limits.softLimitSelectorRaw;
        r.limitEntries[which] = w.limits.softLimitEntries;
        r.limitMinimum[which] = w.limits.minimumNative;
        r.limitMaximum[which] = w.limits.maximumNative;
    } else {
        const M::Status ok = read ? L::getIo(*read, target, now, SETTINGS_AGE_US, w.io)
                                  : L::getIo(w.transaction, w.live, target, now, SETTINGS_AGE_US, w.io);
        if (!ok || w.io.group != L::DriverGroup::IO_INPUT_CONFIGURATION)
            return;
        r.ioEntries[which] = w.io.ioEntries;
        r.ioPolarity[which] = w.io.inputPolarityRaw;
        for (unsigned i = 0; i < 7; ++i)
            r.ioFunctions[which][i] = w.io.inputFunctionRaw[i];
    }
    r.groupPresent[which] = true;
}
// While the session runs the move: note its phases, and ask for the planned stop once the stated number
// of cycles completed with no completion observed. The stop's state is the move's last complete cycle.
void BenchRoute::serviceMove(uint64_t now) noexcept {
    const L::MoveContext& m = axis().moveAction();
    if (!axis().moveActive())
        return;
    record_.movePhases = uint8_t(record_.movePhases | 1u << unsigned(m.phase));
    const RoutePlan& p = record_.plan;
    if (p.stopKind && !stopAttempted_ && endFailure_ == RouteFailure::NONE && m.observations >= p.stopAfter &&
        m.completion == M::ActionCompletion::NOT_OBSERVED &&
        (m.phase == L::MovePhase::WAITING || m.phase == L::MovePhase::OBSERVING))
        startStop(p.stopKind, p.stopDeadlineMs, p.bestEffortMs, p.stopIntervalMs, axis().moveObservation(), now);
}
// COUNTER_ZERO and CLEAR_ALARM through the session, which then admits every write and read itself.
void BenchRoute::startClear(uint64_t now) noexcept {
    const RoutePlan& p = record_.plan;
    L::ActionOptions options;
    options.service.queueTimeoutUs = ROUTE_QUEUE_US;
    options.service.responseTimeoutUs = ROUTE_RESPONSE_US;
    options.observationIntervalUs = uint64_t(p.intervalMs) * 1000;
    // The library's own floor: five fields of a clear's cycle, seven of a counter's.
    options.maximumAgeUs = (p.kind == RouteKind::COUNTER_ZERO ? 7 : 5) * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
    const uint64_t deadline = now + uint64_t(p.deadlineMs) * 1000;
    if (deadline > cutoffUs_) {
        finish(RouteFailure::REFUSED, "deadline", now);
        return;
    }
    M::Status status;
    if (p.kind == RouteKind::CLEAR_ALARM) {
        status = axis().requestClearAlarm(context_.read, options, deadline, now);
    } else {
        // The axis and witness the counter's 1 withdraws, borrowed until the clear is terminal: this
        // node's move configuration, at the session's binding, and a witness that knows no position.
        M::AxisConfig& axisConfig = moveAxes_[slot()];
        axisConfig.binding = axis().binding();
        axisConfig.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
        axisConfig.nativeVelocityTime = M::TimeUnit::MINUTE;
        M::AxisReference& reference = moveReferences_[slot()];
        reference.nativeKnown = reference.idle = reference.stationary = false;
        status = axis().requestCounterZero(context_.read, 0, axisConfig, reference, options, deadline, now);
    }
    if (!status) {
        record_.statusPresent = true;
        record_.statusCode = uint8_t(status.code);
        record_.statusDetail = status.detail;
        record_.statusMessage = status.msg ? status.msg : "";
        finish(RouteFailure::REFUSED, "clear", now);
        return;
    }
    record_.clearRequested = true;
    record_.actionRequestedUs = now;
    actionStarted_ = true;
}
// The observer node's state reads: its own read context, its own session and grants, never node A's.
bool BenchRoute::startObserverRead(uint8_t mask, uint64_t now) noexcept {
    const uint8_t other = uint8_t(record_.plan.observer - 1);
    const C::SdoChannel* channel = services_.channel(other);
    const C::NodeSnapshot* node = services_.node(other);
    if (!work_ || !channel || !node || node->exhausted)
        return false;
    L::ReadTarget target;
    target.channel = *channel;
    target.nodeGeneration = node->route.generation;
    const uint64_t deadline = now + ROUTE_OPERATION_US < cutoffUs_ ? now + ROUTE_OPERATION_US : cutoffUs_;
    const uint32_t operation = deadline > now ? observerAxis().beginOperation(deadline, now) : 0;
    if (!operation)
        return false;
    L::ReadOptions options;
    options.queueTimeoutUs = ROUTE_QUEUE_US;
    options.responseTimeoutUs = ROUTE_RESPONSE_US;
    if (!L::prepareState(work_->observer, target, operation, now, deadline, options, mask))
        return false;
    observerActive_ = true;
    return true;
}
// Settle the observer's request, or offer its next field, as admit() and settle() do for node A. A
// failure before the move ends the route; once the move runs it ends only the observing.
void BenchRoute::serviceObserverRead(uint64_t now) noexcept {
    const bool checking = steps_[stepIndex_].kind == STEP_OBSERVER_CHECK;
    const auto fail = [this, checking, now](RouteFailure failure, const char* why) {
        observerActive_ = false;
        record_.observerWhy = why;
        if (checking)
            finish(failure, "observer", now);
        else
            record_.observerStopped = true;
    };
    L::ReadContext& read = work_->observer;
    AxisSession& watcher = observerAxis();
    const uint8_t other = uint8_t(record_.plan.observer - 1);
    if (observerPending_) {
        C::SdoCompletion done;
        if (!watcher.settle(done))
            return;
        observerPending_ = false;
        if (record_.terminal || record_.observerStopped)
            return; // Evidence only: the terminal is in the record's RESULT lines.
        const C::SdoTransfer& t = done.transfer;
        const C::SdoTransfer& e = observerExpected_;
        if (t.operation != e.operation || t.request != e.request || t.step != e.step || t.index != e.index ||
            t.subindex != e.subindex || t.direction != e.direction)
            return fail(RouteFailure::CORRELATION, "terminal");
        if (!L::advanceRead(read, done, now))
            return fail(RouteFailure::CORRELATION, "advance");
        if (read.state == M::ReadState::SUCCEEDED) {
            observerActive_ = false;
            observerReadDone(now);
        } else if (read.state != M::ReadState::ACTIVE) {
            fail(RouteFailure::SERVICE, "read");
        }
        return;
    }
    if (!observerActive_ || record_.terminal)
        return;
    L::PreparedRead next;
    if (!L::nextRead(read, now, next))
        return fail(now >= read.deadlineUs ? RouteFailure::DEADLINE : RouteFailure::ADMISSION, "next");
    if (!next.ready) {
        if (read.state == M::ReadState::SUCCEEDED) {
            observerActive_ = false;
            observerReadDone(now);
        } else {
            fail(RouteFailure::SERVICE, "read");
        }
        return;
    }
    const C::SdoTransfer& transfer = next.transfer;
    if (!firmware_.owner().localTxRetired(other))
        return;
    // The route's hold-back: no exchange whose budget does not fit before its deadline.
    const uint64_t budget = uint64_t(transfer.queueTimeoutUs) + transfer.responseTimeoutUs;
    if (transfer.deadlineUs <= now || transfer.deadlineUs - now < budget)
        return fail(RouteFailure::DEADLINE, transfer.deadlineUs >= cutoffUs_ ? "cutoff" : "operation");
    if (transfer.readEffect == C::SdoReadEffect::UNQUALIFIED &&
        !(observerGrantOperation_ == transfer.operation && observerGrantStep_ == transfer.step)) {
        const C::NodeSnapshot* node = services_.node(other);
        if (grant_ == UINT32_MAX || !node ||
            !services_.authorizeUploadExperiment(other, transfer, node->route.generation, grant_ + 1, now))
            return fail(RouteFailure::AUTHORIZATION, "grant");
        ++grant_;
        observerGrantOperation_ = transfer.operation;
        observerGrantStep_ = transfer.step;
    }
    const BusAdmission admission = watcher.admitPrepared(transfer, now);
    if (admission == BusAdmission::RESULTS_FULL)
        return;
    if (admission != BusAdmission::ACCEPTED)
        return fail(RouteFailure::ADMISSION, "owner");
    observerExpected_ = transfer;
    observerPending_ = true;
}
void BenchRoute::observerReadDone(uint64_t now) noexcept {
    const uint8_t other = uint8_t(record_.plan.observer - 1);
    const C::SdoChannel* channel = services_.channel(other);
    const C::NodeSnapshot* node = services_.node(other);
    RouteSnapshot s;
    const L::StateBlock blocks[] = {L::StateBlock::ERRORS, L::StateBlock::MONITORS, L::StateBlock::STATUS,
                                    L::StateBlock::FEEDBACK};
    bool ok = channel && node;
    L::ReadTarget target;
    if (ok) {
        target.channel = *channel;
        target.nodeGeneration = node->route.generation;
    }
    for (L::StateBlock block : blocks) {
        const uint8_t bit = uint8_t(1u << unsigned(block));
        if (!ok || !(work_->observer.blockMask & bit))
            continue;
        ok = L::getStateBlock(work_->observer, block, target, now, ROUTE_OPERATION_US, block_).isOk();
        if (!ok)
            break;
        s.blocks = uint8_t(s.blocks | bit);
        if (block == L::StateBlock::ERRORS) {
            s.errorRegister = block_.errorRegister;
            s.errorCode = block_.errorCode;
        } else if (block == L::StateBlock::MONITORS) {
            s.monitor = block_.monitor;
        } else if (block == L::StateBlock::STATUS) {
            s.statusword = block_.statusword;
            s.mode = block_.modeRaw;
        } else {
            s.position = block_.actualPosition;
            s.velocity = block_.actualVelocity;
        }
    }
    RouteRecord& r = record_;
    if (steps_[stepIndex_].kind == STEP_OBSERVER_CHECK) {
        // Stationary as reported, released or not as found: no alarm, running clear, 606C 0.
        r.observerChecked = true;
        r.observerCheck = s;
        const char* why = !ok || (s.blocks & FULL) != FULL ? "snapshot"
                          : s.monitor & L::MONITOR_ALARM     ? "alarm"
                          : s.monitor & L::MONITOR_RUNNING   ? "running"
                          : s.velocity                       ? "velocity"
                                                             : nullptr;
        r.observerPassed = why == nullptr;
        r.observerWhy = why ? why : "none";
        r.observerMinimum = r.observerMaximum = s.position;
        if (why)
            finish(RouteFailure::PREREQUISITE, "observer", now);
        else
            nextStep(now);
        return;
    }
    if (!ok) {
        r.observerStopped = true;
        r.observerWhy = "snapshot";
        return;
    }
    // Recorded, never judged: what the other node reported while this one moved.
    ++r.observerCycles;
    r.observerLast = s;
    r.observerMonitors = uint16_t(r.observerMonitors | s.monitor);
    if (s.position < r.observerMinimum)
        r.observerMinimum = s.position;
    if (s.position > r.observerMaximum)
        r.observerMaximum = s.position;
    const int32_t speed = s.velocity < 0 ? -s.velocity : s.velocity;
    if (speed > r.observerVelocity)
        r.observerVelocity = speed;
    if (s.velocity || (s.monitor & L::MONITOR_RUNNING) || s.position != r.observerCheck.position)
        r.observerMoved = true;
    const uint64_t next = observerCycleUs_ + uint64_t(r.plan.intervalMs) * 1000;
    observerNextUs_ = next > now ? next : now;
}
// While node A's move or its planned stop runs, one image cycle of the observer at the move's interval.
void BenchRoute::serviceObserver(uint64_t now) noexcept {
    if (observerPending_ || observerActive_) {
        serviceObserverRead(now);
        return;
    }
    if (record_.observerStopped || !(axis().moveActive() || axis().stopActive()) || now < observerNextUs_)
        return;
    observerCycleUs_ = now;
    if (!startObserverRead(IMAGE, now)) {
        record_.observerStopped = true;
        record_.observerWhy = "session";
        return;
    }
    serviceObserverRead(now);
}
// The session advances its own action in ProbeApp::service; this only waits for it to end.
void BenchRoute::serviceAction(uint64_t now) noexcept {
    if (record_.clearRequested) {
        if (axis().clearActive())
            return;
        if (endFailure_ != RouteFailure::NONE) {
            finish(endFailure_, endWhy_, now);
            return;
        }
        const L::ActionContext& c = axis().clearAction();
        const bool ok = c.startedUs >= record_.actionRequestedUs && c.state == M::ActionState::SUCCEEDED;
        finish(ok ? RouteFailure::NONE : RouteFailure::ACTION, ok ? "none" : "clear", now);
        return;
    }
    if (record_.moveRequested) {
        serviceMove(now);
        if (record_.plan.observer)
            serviceObserver(now);
        // The observer's cycle in progress completes, and its request settles, before the record ends.
        if (axis().moveActive() || axis().stopActive() || observerPending_ || observerActive_)
            return;
        const L::MoveContext& m = axis().moveAction();
        record_.cycles = uint16_t(m.observations > UINT16_MAX ? UINT16_MAX : m.observations);
    } else if (record_.stopRequested ? axis().stopActive() : axis().enableActive()) {
        return;
    }
    if (endFailure_ != RouteFailure::NONE) {
        finish(endFailure_, endWhy_, now);
        return;
    }
    const uint64_t at = record_.actionRequestedUs;
    if (record_.enableRequested) {
        const L::ActionContext& c = axis().enableAction();
        const bool ok = c.startedUs >= at && c.state == M::ActionState::SUCCEEDED;
        finish(ok ? RouteFailure::NONE : RouteFailure::ACTION, ok ? "none" : "enable", now);
        return;
    }
    if (!record_.stopRequested) {
        // The move's own terminal, its rearm included. A completion is a controller report only.
        const L::MoveContext& m = axis().moveAction();
        const bool ok = m.startedUs >= record_.moveRequestedUs && m.state == M::ActionState::SUCCEEDED;
        finish(ok ? RouteFailure::NONE : RouteFailure::ACTION, ok ? "none" : "move", now);
        return;
    }
    const L::ActionContext& c =
        axis().stopAction(record_.stopRequestKind == 1 ? L::StopKind::HALT : L::StopKind::QUICK_STOP);
    const L::BestEffortStop& b = axis().bestEffort();
    const bool own = c.state != M::ActionState::EMPTY && c.startedUs >= at;
    const bool fallback = b.state != M::ActionState::EMPTY && b.startedUs >= at;
    const bool ok = own && c.state == M::ActionState::SUCCEEDED && !fallback;
    finish(ok ? RouteFailure::NONE : RouteFailure::ACTION, ok ? "none" : fallback ? "best-effort" : "stop",
           now);
}
void BenchRoute::serviceObserve(uint64_t now) noexcept {
    if (readActive_) {
        serviceRead(now);
        return;
    }
    if (now < nextCycleUs_)
        return;
    // P2's named fresh set includes 1001 and 603F; DEC-1 keeps them single reads, so only the
    // first cycle after the 6060 write reads them.
    const bool first = record_.plan.kind == RouteKind::MODE_ONE && record_.cycles == 0;
    cycleStartUs_ = now;
    if (!startRead(READ_STATE, first ? FULL : IMAGE, now))
        return;
    serviceRead(now);
}
void BenchRoute::observeDone(uint64_t now) noexcept {
    ++record_.cycles;
    const RouteSnapshot& s = record_.last;
    const RoutePlan& p = record_.plan;
    const char* why = nullptr;
    if (s.monitor & L::MONITOR_ALARM)
        why = "alarm";
    else if (p.kind == RouteKind::MODE_ONE) {
        // P2's abort list: alarm, 2100 running or nonzero 606C; and a fault after the write.
        if ((s.blocks & 1u) && s.errorRegister)
            why = "error-register";
        else if ((s.blocks & 1u) && s.errorCode)
            why = "fault-code";
        else if (s.monitor & L::MONITOR_RUNNING)
            why = "running";
        else if (s.velocity)
            why = "velocity";
    }
    if (why) {
        finish(RouteFailure::OBSERVATION, why, now);
        return;
    }
    // An abort, EMCY or lost evidence already ends the route: service() finishes it after this read,
    // and no planned stop or further cycle is asked for here.
    if (endFailure_ != RouteFailure::NONE)
        return;
    if (writesPosition(p.kind) && p.stopKind && record_.cycles == p.stopAfter) {
        startStop(p.stopKind, p.stopDeadlineMs, p.bestEffortMs, p.stopIntervalMs, context_.read, now);
        return;
    }
    // WORD67 counts its cycles across both words: those after 0006 first, then those after 0007.
    const bool wordSix = steps_[stepIndex_].arg == OBSERVE_WORD_SIX;
    const unsigned planned = wordSix ? p.wordCycles : p.kind == RouteKind::WORD67 ? p.wordCycles + p.cycles : p.cycles;
    if (record_.cycles >= planned) {
        if (wordSix) {
            // 0007 follows only a last 0006 cycle in mode 1 with low bits 001; otherwise nothing more.
            const char* stop = s.mode != 1                                   ? "mode"
                               : (s.statusword & L::STATUS_LOW_BITS) != 0x0001 ? "status"
                                                                              : nullptr;
            if (stop)
                finish(RouteFailure::OBSERVATION, stop, now);
            else
                nextStep(now);
            return;
        }
        finish(RouteFailure::NONE, "none", now);
        return;
    }
    const uint64_t next = cycleStartUs_ + uint64_t(p.intervalMs) * 1000;
    nextCycleUs_ = next > now ? next : now;
}

// ------------------------------------------------------------------------------------- records
// TRIAL_MOVE_SAMPLE lines: admission, latest, acceptance, absence and completed.
constexpr unsigned MOVE_SAMPLES = 5;
unsigned BenchRoute::summaryCount() const noexcept {
    const RouteRecord& r = record_;
    unsigned count = 2 + r.firstPresent + r.checkPresent + (r.last.blocks != 0) + r.historyPresent + r.emcy +
                     r.statusPresent;
    // TRIAL_REARM, or TRIAL_CONTROL; then TRIAL_BASE.
    if (r.plan.kind == RouteKind::REARM || r.plan.kind == RouteKind::RELEASED_QUICK || r.plan.kind == RouteKind::WORD67)
        count += 1;
    if (r.plan.kind == RouteKind::RELATIVE || (r.plan.kind == RouteKind::MOVE && r.plan.relativeBase))
        count += 1;
    // TRIAL_SETTINGS and the prior and readback group lines; then IO_DI3's TRIAL_INPUTS.
    if (isSettings(r.plan.kind))
        count += 3 + (r.plan.kind == RouteKind::IO_DI3);
    // TRIAL_OBSERVER, its check and last reads, and TRIAL_NODES.
    if (r.plan.kind == RouteKind::MOVE && r.plan.observer)
        count += 4;
    // TRIAL_CLEAR, three TRIAL_CLEAR_STEP, TRIAL_INCIDENT, TRIAL_BASELINE and TRIAL_EXTRA.
    if (r.clearRequested)
        count += 7;
    if (r.plan.kind == RouteKind::MOVE)
        count += 1 + (r.moveRequested ? 1 + MOVE_SAMPLES : 0);
    if (r.enableRequested)
        count += 4;
    if (r.stopRequested)
        count += 3;
    return count;
}
bool BenchRoute::formatSummary(unsigned index, char* out, size_t size) const noexcept {
    const RouteRecord& r = record_;
    const RoutePlan& p = r.plan;
    if (index == 0) {
        switch (p.kind) {
        case RouteKind::MODE_ONE:
        case RouteKind::RELEASED_QUICK:
            return put(out, size, "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u cycles=%u interval=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), unsigned(p.cycles), static_cast<unsigned long>(p.intervalMs));
        case RouteKind::REARM:
            return put(out, size,
                       "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u pred=%u cycles=%u interval=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), unsigned(p.predecessor), unsigned(p.cycles),
                       static_cast<unsigned long>(p.intervalMs));
        case RouteKind::WORD67:
            return put(out, size,
                       "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u c6=%u cycles=%u interval=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), unsigned(p.wordCycles), unsigned(p.cycles),
                       static_cast<unsigned long>(p.intervalMs));
        case RouteKind::SETTINGS_2202:
        case RouteKind::IO_DI3:
            return put(out, size, "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u %s=%u\n", routeName(p.kind),
                       unsigned(p.node), static_cast<unsigned long>(p.trial), unsigned(p.attended),
                       p.kind == RouteKind::IO_DI3 ? "case" : "code", unsigned(p.code));
        case RouteKind::ENABLE:
            return put(out, size,
                       "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u start=%u shaft=%u status=%04x pt=%ld "
                       "vt=%ld deadline=%lu interval=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), unsigned(p.start), unsigned(p.shaft), unsigned(p.reviewedStatus),
                       static_cast<long>(p.reviewedPositionTarget), static_cast<long>(p.reviewedVelocityTarget),
                       static_cast<unsigned long>(p.deadlineMs), static_cast<unsigned long>(p.intervalMs));
        case RouteKind::STOP:
            return put(out, size,
                       "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u kind=%u deadline=%lu be=%lu "
                       "interval=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), unsigned(p.stopKind), static_cast<unsigned long>(p.deadlineMs),
                       static_cast<unsigned long>(p.bestEffortMs), static_cast<unsigned long>(p.intervalMs));
        case RouteKind::POSITION:
        case RouteKind::RELATIVE:
            return put(out, size,
                       "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u sp=%ld target=%ld rpm=%lu up=%lu "
                       "down=%lu cycles=%u interval=%lu kind=%u after=%u sd=%lu be=%lu si=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), static_cast<long>(p.startPosition), static_cast<long>(p.target),
                       static_cast<unsigned long>(p.rpm), static_cast<unsigned long>(p.rampUpMs),
                       static_cast<unsigned long>(p.rampDownMs), unsigned(p.cycles),
                       static_cast<unsigned long>(p.intervalMs), unsigned(p.stopKind), unsigned(p.stopAfter),
                       static_cast<unsigned long>(p.stopDeadlineMs), static_cast<unsigned long>(p.bestEffortMs),
                       static_cast<unsigned long>(p.stopIntervalMs));
        case RouteKind::MOVE:
            return put(out, size,
                       "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u sp=%ld target=%ld rpm=%lu up=%lu "
                       "down=%lu deadline=%lu interval=%lu kind=%u after=%u sd=%lu be=%lu si=%lu rel=%u base=%ld "
                       "obs=%u\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), static_cast<long>(p.startPosition), static_cast<long>(p.target),
                       static_cast<unsigned long>(p.rpm), static_cast<unsigned long>(p.rampUpMs),
                       static_cast<unsigned long>(p.rampDownMs), static_cast<unsigned long>(p.deadlineMs),
                       static_cast<unsigned long>(p.intervalMs), unsigned(p.stopKind), unsigned(p.stopAfter),
                       static_cast<unsigned long>(p.stopDeadlineMs), static_cast<unsigned long>(p.bestEffortMs),
                       static_cast<unsigned long>(p.stopIntervalMs), unsigned(p.relativeBase),
                       static_cast<long>(p.base), unsigned(p.observer));
        case RouteKind::COUNTER_ZERO:
        case RouteKind::CLEAR_ALARM:
            return put(out, size, "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u deadline=%lu interval=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), static_cast<unsigned long>(p.deadlineMs),
                       static_cast<unsigned long>(p.intervalMs));
        default:
            return put(out, size, "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u\n", routeName(p.kind),
                       unsigned(p.node), static_cast<unsigned long>(p.trial), unsigned(p.attended));
        }
    }
    if (index == 1)
        return put(out, size,
                   "TRIAL_RES stack0=%lu stack1=%lu heap0=%lu heap1=%lu heapMin=%lu largest=%lu gap=%llu "
                   "dropped=%lu admission=%u started=%llu completed=%llu tec0=%lu rec0=%lu tec1=%lu rec1=%lu\n",
                   static_cast<unsigned long>(r.before.stackFree), static_cast<unsigned long>(r.after.stackFree),
                   static_cast<unsigned long>(r.before.heapFree), static_cast<unsigned long>(r.after.heapFree),
                   static_cast<unsigned long>(r.after.heapMinimum), static_cast<unsigned long>(r.after.largestBlock),
                   static_cast<unsigned long long>(r.maxServiceGapUs), static_cast<unsigned long>(r.dropped),
                   unsigned(r.admission), static_cast<unsigned long long>(r.startedUs),
                   static_cast<unsigned long long>(r.completedUs),
                   static_cast<unsigned long>(r.before.txErrors), static_cast<unsigned long>(r.before.rxErrors),
                   static_cast<unsigned long>(r.after.txErrors), static_cast<unsigned long>(r.after.rxErrors));
    index -= 2;
    // The first of two check reads, the check's own read and the latest; passed is the check's verdict
    // on the reads it judged, and always 1 for the latest.
    const RouteSnapshot* shots[] = {r.firstPresent ? &r.first : nullptr, r.checkPresent ? &r.check : nullptr,
                                    r.last.blocks ? &r.last : nullptr};
    const char* labels[] = {"TRIAL_FIRST", "TRIAL_CHECK", "TRIAL_LAST"};
    for (unsigned i = 0; i < 3; ++i) {
        if (!shots[i])
            continue;
        if (index--)
            continue;
        const RouteSnapshot& s = *shots[i];
        return put(out, size,
                   "%s passed=%u blocks=%02x 1001=%02x 603f=%04x 2100=%04x 6041=%04x 6061=%02x 6064=%ld "
                   "606c=%ld\n",
                   labels[i], unsigned(i < 2 ? r.checkPassed : 1), unsigned(s.blocks), unsigned(s.errorRegister),
                   unsigned(s.errorCode), unsigned(s.monitor), unsigned(s.statusword), unsigned(s.mode),
                   static_cast<long>(s.position), static_cast<long>(s.velocity));
    }
    if (r.historyPresent && !index--) {
        // Raw 1003:00 and only the entries it names, never interpreted: an entry that was not read is
        // absent from the line, not printed as zero. Each " eN=xxxxxxxx" is twelve characters.
        constexpr int ENTRY = 12;
        char entries[4 * ENTRY + 1];
        put(entries, sizeof entries, " e1=%08lx e2=%08lx e3=%08lx e4=%08lx", static_cast<unsigned long>(r.history[0]),
            static_cast<unsigned long>(r.history[1]), static_cast<unsigned long>(r.history[2]),
            static_cast<unsigned long>(r.history[3]));
        const unsigned count = r.historyCount < 4 ? r.historyCount : 4u;
        return put(out, size, "TRIAL_HISTORY count=%u%.*s\n", unsigned(r.historyCount), ENTRY * int(count), entries);
    }
    if (r.emcy && !index--)
        return put(out, size, "TRIAL_EMCY data=%02x%02x%02x%02x%02x%02x%02x%02x\n", unsigned(r.emcyData[0]),
                   unsigned(r.emcyData[1]), unsigned(r.emcyData[2]), unsigned(r.emcyData[3]),
                   unsigned(r.emcyData[4]), unsigned(r.emcyData[5]), unsigned(r.emcyData[6]),
                   unsigned(r.emcyData[7]));
    if (r.statusPresent && !index--)
        return put(out, size, "TRIAL_STATUS code=%u detail=%ld; %s\n", unsigned(r.statusCode),
                   static_cast<long>(r.statusDetail), r.statusMessage);
    // The session's latest word now: a lost word write leaves none.
    const C::SdoCompletion* latest = axis().latestControl();
    const unsigned latestWord =
        latest ? unsigned(latest->transfer.data[0] | unsigned(latest->transfer.data[1]) << 8) : 0u;
    if (p.kind == RouteKind::REARM && !index--) {
        // The predecessor this 000F followed, and the latest word after it.
        return put(out, size, "TRIAL_REARM pword=%04x predop=%lu predus=%llu latest=%u word=%04x latestop=%lu\n",
                   unsigned(r.rearmFollowsWord), static_cast<unsigned long>(r.rearmFollows),
                   static_cast<unsigned long long>(r.rearmFollowsUs), unsigned(latest != nullptr), latestWord,
                   static_cast<unsigned long>(latest ? latest->transfer.operation : 0));
    }
    if ((p.kind == RouteKind::RELEASED_QUICK || p.kind == RouteKind::WORD67) && !index--) {
        // What the session knew at admission, which may be nothing, and the latest word after the route.
        return put(out, size,
                   "TRIAL_CONTROL prior=%u pword=%04x pop=%lu pus=%llu latest=%u word=%04x latestop=%lu\n",
                   unsigned(r.priorPresent), unsigned(r.priorWord), static_cast<unsigned long>(r.priorOperation),
                   static_cast<unsigned long long>(r.priorUs), unsigned(latest != nullptr), latestWord,
                   static_cast<unsigned long>(latest ? latest->transfer.operation : 0));
    }
    if ((p.kind == RouteKind::RELATIVE || (p.kind == RouteKind::MOVE && p.relativeBase)) && !index--) {
        // The base the displacement meets: the 607A uploaded before the writes, this object's record of
        // the 607A it last had acknowledged on that node, and a relative move's witness. The start 6064
        // is in TRIAL_CHECK. None of these is a measurement of what the drive adds the displacement to.
        return put(out, size,
                   "TRIAL_BASE read=%u target=%ld known=%u prev=%ld prevTrial=%lu prevRelative=%u basis=%u "
                   "witness=%ld\n",
                   unsigned(r.targetReadPresent), static_cast<long>(r.targetRead), unsigned(r.commandedKnown),
                   static_cast<long>(r.commanded), static_cast<unsigned long>(r.commandedTrial),
                   unsigned(r.commandedRelative), unsigned(r.witnessBasis), static_cast<long>(r.witnessPosition));
    }
    if (isSettings(p.kind)) {
        if (!index--)
            // The transaction's own result, the configuration epochs before and after, and whether the
            // owner's reservation and the session followed. Its terminals are the RESULT lines.
            return put(out, size,
                       "TRIAL_SETTINGS prepared=%u noop=%u state=%u outcome=%u changed=%x confirmed=%x effects=%u "
                       "uncertain=%u cfg0=%lu cfg1=%lu followed=%u\n",
                       unsigned(r.settingsPrepared), unsigned(r.settingsNoop), unsigned(r.settingsState),
                       unsigned(r.settingsOutcome), unsigned(r.settingsChanged), unsigned(r.settingsConfirmed),
                       unsigned(r.settingsEffects), unsigned(r.settingsUncertain),
                       static_cast<unsigned long>(r.configurationBefore),
                       static_cast<unsigned long>(r.configurationAfter), unsigned(r.configurationFollowed));
        if (index < 2) {
            // Raw, 0 the prior read and 1 the matched readback; a value not published reads present=0.
            const unsigned w = index;
            if (p.kind == RouteKind::SETTINGS_2202)
                return put(out, size, "TRIAL_LIMITS which=%u present=%u 2202=%04x entries=%u min=%ld max=%ld\n", w,
                           unsigned(r.groupPresent[w]), unsigned(r.limitSelector[w]), unsigned(r.limitEntries[w]),
                           static_cast<long>(r.limitMinimum[w]), static_cast<long>(r.limitMaximum[w]));
            const uint16_t* f = r.ioFunctions[w];
            return put(out, size,
                       "TRIAL_IO which=%u present=%u entries=%u polarity=%04x di0=%04x di1=%04x di2=%04x di3=%04x "
                       "di4=%04x di5=%04x di6=%04x\n",
                       w, unsigned(r.groupPresent[w]), unsigned(r.ioEntries[w]), unsigned(r.ioPolarity[w]),
                       unsigned(f[0]), unsigned(f[1]), unsigned(f[2]), unsigned(f[3]), unsigned(f[4]), unsigned(f[5]),
                       unsigned(f[6]));
        }
        index -= 2;
        if (p.kind == RouteKind::IO_DI3 && !index--)
            return put(out, size, "TRIAL_INPUTS present=%u 2101=%04x 60fd=%08lx\n", unsigned(r.inputsPresent),
                       unsigned(r.logicalInputs), static_cast<unsigned long>(r.digitalInputs));
    }
    if (p.kind == RouteKind::MOVE && p.observer) {
        if (!index--)
            // What the other node reported while this one moved: recorded, never judged. moved is any
            // cycle with 606C nonzero, 2100 running or a 6064 other than the check's.
            return put(out, size,
                       "TRIAL_OBSERVER node=%u checked=%u passed=%u why=%s cycles=%u stopped=%u moved=%u pmin=%ld "
                       "pmax=%ld vmax=%ld or2100=%04x emcy=%u edata=%02x%02x%02x%02x%02x%02x%02x%02x\n",
                       unsigned(p.observer), unsigned(r.observerChecked), unsigned(r.observerPassed),
                       r.observerWhy, unsigned(r.observerCycles), unsigned(r.observerStopped),
                       unsigned(r.observerMoved), static_cast<long>(r.observerMinimum),
                       static_cast<long>(r.observerMaximum), static_cast<long>(r.observerVelocity),
                       unsigned(r.observerMonitors), unsigned(r.observerEmcy), unsigned(r.observerEmcyData[0]),
                       unsigned(r.observerEmcyData[1]), unsigned(r.observerEmcyData[2]),
                       unsigned(r.observerEmcyData[3]), unsigned(r.observerEmcyData[4]),
                       unsigned(r.observerEmcyData[5]), unsigned(r.observerEmcyData[6]),
                       unsigned(r.observerEmcyData[7]));
        if (index < 2) {
            const RouteSnapshot& s = index ? r.observerLast : r.observerCheck;
            return put(out, size,
                       "%s passed=%u blocks=%02x 1001=%02x 603f=%04x 2100=%04x 6041=%04x 6061=%02x 6064=%ld "
                       "606c=%ld\n",
                       index ? "TRIAL_OBSERVER_LAST" : "TRIAL_OBSERVER_CHECK",
                       unsigned(index ? 1 : r.observerPassed), unsigned(s.blocks), unsigned(s.errorRegister),
                       unsigned(s.errorCode), unsigned(s.monitor), unsigned(s.statusword), unsigned(s.mode),
                       static_cast<long>(s.position), static_cast<long>(s.velocity));
        }
        index -= 2;
        if (!index--)
            // Both nodes' copied RX frames and those dispatch did not accept, and the interface's own
            // counters at admission and terminal: a dropped frame has no node.
            return put(out, size,
                       "TRIAL_NODES gap=%llu n1rx=%u n1unmatched=%u n2rx=%u n2unmatched=%u dropped0=%lu dropped1=%lu "
                       "unmatched0=%lu unmatched1=%lu lost0=%lu lost1=%lu\n",
                       static_cast<unsigned long long>(r.maxServiceGapUs), unsigned(r.nodeRx[0]),
                       unsigned(r.nodeUnmatched[0]), unsigned(r.nodeRx[1]), unsigned(r.nodeUnmatched[1]),
                       static_cast<unsigned long>(r.droppedBefore), static_cast<unsigned long>(r.droppedAfter),
                       static_cast<unsigned long>(r.unmatchedBefore), static_cast<unsigned long>(r.unmatchedAfter),
                       static_cast<unsigned long>(r.lostBefore), static_cast<unsigned long>(r.lostAfter));
    }
    if (r.clearRequested) {
        const L::ActionContext& c = axis().clearAction();
        if (!index--)
            return put(out, size,
                       "TRIAL_CLEAR own=%u kind=%u state=%u outcome=%u exec=%u completion=%u uncertain=%u "
                       "effects=%u one=%u coords=%u final=%u observations=%lu op=%lu blocked=%u\n",
                       unsigned(c.state != M::ActionState::EMPTY && c.startedUs >= r.actionRequestedUs),
                       unsigned(c.actionKind), unsigned(c.state), unsigned(c.outcome), unsigned(c.execution),
                       unsigned(c.completion), unsigned(c.uncertain), unsigned(c.effectsPossible),
                       unsigned(c.valueOneMayRemain), unsigned(c.coordinatesInvalidated),
                       unsigned(c.finalErrorRead), static_cast<unsigned long>(c.observations),
                       static_cast<unsigned long>(c.operation), unsigned(axis().clearBlocked()));
        if (index < 3) {
            // Each write's terminal by slot: alarm 1 then 0 in slots 0 and 1, counter 0, 1, 0.
            const unsigned w = index;
            const C::SdoCompletion& h = c.commandHistory[w];
            const bool held = (c.commandEvidenceMask >> w) & 1u;
            return put(out, size,
                       "TRIAL_CLEAR_STEP slot=%u present=%u object=%04x:%02x value=%04x outcome=%u ack=%u "
                       "possible=%u uncertain=%u\n",
                       w, unsigned(held), unsigned(held ? h.transfer.index : 0), unsigned(held ? h.transfer.subindex : 0),
                       held ? unsigned(h.transfer.data[0] | unsigned(h.transfer.data[1]) << 8) : 0u,
                       unsigned(held ? unsigned(h.outcome) : 255u), unsigned(held && h.protocolAcknowledged),
                       unsigned(held && h.effectPossible), unsigned(held && h.effectUncertain));
        }
        index -= 3;
        if (!index--) {
            const L::AlarmIncident& a = c.incident;
            return put(out, size,
                       "TRIAL_INCIDENT present=%u 1001=%02x 603f=%04x count=%u 2100=%04x 2101=%04x 2102=%04x "
                       "6041=%04x 6061=%02x\n",
                       unsigned(a.present), unsigned(a.errorRegister), unsigned(a.errorCode), unsigned(a.errorCount),
                       unsigned(a.monitor), unsigned(a.logicalInputs), unsigned(a.logicalOutputs),
                       unsigned(a.statusword), unsigned(a.modeRaw));
        }
        if (!index--)
            return put(out, size, "TRIAL_BASELINE present=%u 6064=%ld 606c=%ld\n", unsigned(c.baseline.present),
                       static_cast<long>(c.baseline.startPosition), static_cast<long>(c.baseline.startVelocity));
        if (!index--) {
            // Selected by its block, which reads 0 (ERRORS) with no fields when unused.
            const L::StateObservation& x = c.extraObservation;
            return put(out, size, "TRIAL_EXTRA block=%u fields=%u 1001=%02x 603f=%04x 6064=%ld 606c=%ld\n",
                       unsigned(x.block), unsigned(x.fieldCount), unsigned(x.errorRegister), unsigned(x.errorCode),
                       static_cast<long>(x.actualPosition), static_cast<long>(x.actualVelocity));
        }
    }
    if (p.kind == RouteKind::MOVE && !index--) {
        // The host's measurement record as the firmware received it; never a firmware default.
        const L::MoveFirmware& f = p.firmware;
        return put(out, size,
                   "TRIAL_EVIDENCE record=%lu as=%u am=%04x av=%04x cs=%u cm=%04x cv=%04x rule=%u dt=%08lx hw=%04x "
                   "sw=%04x vid=%08lx pc=%08lx rev=%08lx mc=%04x fc=%04x rb=%u\n",
                   static_cast<unsigned long>(p.record), unsigned(p.acceptSource), unsigned(p.acceptMask),
                   unsigned(p.acceptValue), unsigned(p.completeSource), unsigned(p.completeMask),
                   unsigned(p.completeValue), unsigned(p.rule), static_cast<unsigned long>(f.deviceType),
                   unsigned(f.hardwareVersion), unsigned(f.softwareVersion), static_cast<unsigned long>(f.vendorId),
                   static_cast<unsigned long>(f.productCode), static_cast<unsigned long>(f.revision),
                   unsigned(f.modelCode), unsigned(f.firmwareCode), unsigned(p.relativeBase));
    }
    if (p.kind == RouteKind::MOVE && r.moveRequested) {
        const L::MoveContext& m = axis().moveAction();
        if (!index--)
            return put(out, size,
                       "TRIAL_MOVE own=%u state=%u phase=%u phases=%02x outcome=%u int=%u setup=%u exec=%u "
                       "completion=%u rearm=%u uncertain=%u readUnc=%u effects=%u cycles=%lu op=%lu target=%ld "
                       "mask=%x ev=%x trigger=%u rearmed=%u fenced=%u code=%u detail=%ld\n",
                       unsigned(m.state != M::ActionState::EMPTY && m.startedUs >= r.moveRequestedUs),
                       unsigned(m.state), unsigned(m.phase), unsigned(r.movePhases), unsigned(m.outcome),
                       unsigned(m.interruption), unsigned(m.setup), unsigned(m.execution), unsigned(m.completion),
                       unsigned(m.rearm), unsigned(m.uncertain), unsigned(m.readUncertain),
                       unsigned(m.effectsPossible), static_cast<unsigned long>(m.observations),
                       static_cast<unsigned long>(m.operation), static_cast<long>(m.parameters.targetNative),
                       unsigned(m.setupMask), unsigned(m.setupEvidenceMask), unsigned(m.triggerPresent),
                       unsigned(m.rearmPresent), unsigned(axis().moveFenced()), unsigned(m.status.code),
                       static_cast<long>(m.status.detail));
        if (index < MOVE_SAMPLES) {
            // Raw kept samples only: which cycle reported what under the host's masks is the move's.
            const L::MoveSample* const samples[] = {&m.admission, &m.latest, &m.acceptance, &m.absence,
                                                    &m.completed};
            const L::MoveSample& s = *samples[index];
            return put(out, size,
                       "TRIAL_MOVE_SAMPLE which=%u present=%u cycle=%lu status=%04x monitor=%04x mode=%02x "
                       "feedback=%u position=%ld velocity=%ld lo=%llu hi=%llu\n",
                       index, unsigned(s.present), static_cast<unsigned long>(s.cycle), unsigned(s.statusword),
                       unsigned(s.monitor), unsigned(s.modeRaw), unsigned(s.feedback), static_cast<long>(s.position),
                       static_cast<long>(s.velocity), static_cast<unsigned long long>(s.earliestUs),
                       static_cast<unsigned long long>(s.latestUs));
        }
        index -= MOVE_SAMPLES;
    }
    if (r.enableRequested) {
        const L::ActionContext& c = axis().enableAction();
        if (!index--)
            return put(out, size,
                       "TRIAL_ENABLE own=%u state=%u outcome=%u exec=%u completion=%u uncertain=%u effects=%u "
                       "mask=%02x cursor=%u observations=%lu op=%lu pt=%ld vt=%ld s0=%04x m0=%04x p0=%ld\n",
                       unsigned(c.startedUs >= r.actionRequestedUs), unsigned(c.state), unsigned(c.outcome),
                       unsigned(c.execution), unsigned(c.completion), unsigned(c.uncertain),
                       unsigned(c.effectsPossible), unsigned(c.commandEvidenceMask), unsigned(c.commandCursor),
                       static_cast<unsigned long>(c.observations), static_cast<unsigned long>(c.operation),
                       static_cast<long>(c.enable.positionTarget), static_cast<long>(c.enable.velocityTarget),
                       unsigned(c.enable.startStatusword), unsigned(c.enable.startMonitor),
                       static_cast<long>(c.enable.startPosition));
        if (index < 3) {
            const unsigned w = index;
            const L::EnableStepRecord& s = c.enable.steps[w];
            const C::SdoCompletion& h = c.commandHistory[w];
            const bool held = (c.commandEvidenceMask >> w) & 1u;
            const uint16_t words[] = {6, 7, 15};
            return put(out, size,
                       "TRIAL_ENABLE_STEP word=%u present=%u status=%04x monitor=%04x position=%ld velocity=%ld "
                       "history=%u outcome=%u ack=%u possible=%u uncertain=%u\n",
                       unsigned(words[w]), unsigned(s.present), unsigned(s.statusword), unsigned(s.monitor),
                       static_cast<long>(s.position), static_cast<long>(s.velocity), unsigned(held),
                       unsigned(held ? unsigned(h.outcome) : 255u), unsigned(held && h.protocolAcknowledged),
                       unsigned(held && h.effectPossible), unsigned(held && h.effectUncertain));
        }
        index -= 3;
    }
    if (r.stopRequested) {
        const L::ActionContext& c =
            axis().stopAction(r.stopRequestKind == 1 ? L::StopKind::HALT : L::StopKind::QUICK_STOP);
        if (!index--)
            return put(out, size,
                       "TRIAL_STOP kind=%u path=%u final=%u own=%u state=%u outcome=%u exec=%u completion=%u "
                       "uncertain=%u effects=%u pred=%04x documented=%u basis=%u observations=%lu op=%lu\n",
                       unsigned(r.stopRequestKind), unsigned(r.stopPathRequested), unsigned(axis().stopPath()),
                       unsigned(c.state != M::ActionState::EMPTY && c.startedUs >= r.actionRequestedUs),
                       unsigned(c.state), unsigned(c.outcome), unsigned(c.execution), unsigned(c.completion),
                       unsigned(c.uncertain), unsigned(c.effectsPossible), unsigned(c.predecessorWord),
                       unsigned(c.predecessorDocumented), unsigned(c.predecessorBasis),
                       static_cast<unsigned long>(c.observations), static_cast<unsigned long>(c.operation));
        if (index < 2) {
            const L::BestEffortStop& b = index ? axis().firstBestEffort() : axis().bestEffort();
            return put(out, size,
                       "TRIAL_BEST_EFFORT which=%u own=%u state=%u outcome=%u exec=%u admitted=%u effects=%u "
                       "op=%lu\n",
                       index, unsigned(b.state != M::ActionState::EMPTY && b.startedUs >= r.actionRequestedUs),
                       unsigned(b.state), unsigned(b.outcome), unsigned(b.execution), unsigned(b.admitted),
                       unsigned(b.effectsPossible), static_cast<unsigned long>(b.operation));
        }
    }
    return false;
}
bool BenchRoute::formatLine(unsigned cursor, char* out, size_t size) const noexcept {
    if (!occupied_ || !record_.terminal || !out || !size)
        return false;
    const RouteRecord& r = record_;
    const unsigned summaries = summaryCount();
    if (cursor == 0)
        return put(out, size,
                   "TRIAL id=%lu node=%u route=%s failure=%u why=%s lost=%u terminals=%u traffic=%u tail=%u "
                   "tailLost=%u summaries=%u step=%u cycles=%u motion=%u case=%u unsettled=%u gap=%llu "
                   "lossCancel=%u\n",
                   static_cast<unsigned long>(r.plan.trial), unsigned(r.plan.node), routeName(r.plan.kind),
                   unsigned(r.failure), r.why, unsigned(r.evidenceLost), unsigned(r.terminalCount),
                   unsigned(r.trafficCount), unsigned(r.tailCount), unsigned(r.tailOverflow), summaries,
                   unsigned(r.step), unsigned(r.cycles), unsigned(r.motionTotal), unsigned(r.caseCount),
                   unsigned(r.unsettled), static_cast<unsigned long long>(r.maxServiceGapUs),
                   unsigned(r.lossCancel));
    unsigned index = cursor - 1;
    if (index < summaries)
        return formatSummary(index, out, size);
    index -= summaries;
    if (index < r.terminalCount) {
        const RouteTerminal& t = r.terminals[index];
        return put(out, size,
                   "RESULT op=%lu req=%lu oreq=%lu step=%lu object=%04x:%02x dir=%u outcome=%u ack=%u possible=%u "
                   "uncertain=%u abort=%08lx cleanup=%u size=%u data=%02x%02x%02x%02x admitted=%llu "
                   "completed=%llu ne=%lu reason=%u st=%02x node=%u\n",
                   static_cast<unsigned long>(t.operation), static_cast<unsigned long>(t.request),
                   static_cast<unsigned long>(t.ownerRequest), static_cast<unsigned long>(t.step),
                   unsigned(t.index), unsigned(t.subindex), unsigned(t.direction), unsigned(t.outcome),
                   unsigned(t.flags & 1u), unsigned((t.flags >> 1) & 1u), unsigned((t.flags >> 2) & 1u),
                   static_cast<unsigned long>(t.abortCode), unsigned((t.flags >> 3) & 1u), unsigned(t.size),
                   unsigned(t.data[0]), unsigned(t.data[1]), unsigned(t.data[2]), unsigned(t.data[3]),
                   static_cast<unsigned long long>(t.admittedUs), static_cast<unsigned long long>(t.completedUs),
                   static_cast<unsigned long>(t.nodeGeneration), unsigned(t.reason), unsigned(t.stages),
                   unsigned(t.node));
    }
    index -= r.terminalCount;
    const bool tail = index >= r.trafficCount;
    if (tail)
        index -= r.trafficCount;
    if (tail && index >= r.tailCount) {
        if (index != r.tailCount)
            return false;
        return put(out, size, "END trial terminals=%u traffic=%u tail=%u tailLost=%u lost=%u; r=release\n",
                   unsigned(r.terminalCount), unsigned(r.trafficCount), unsigned(r.tailCount),
                   unsigned(r.tailOverflow), unsigned(r.evidenceLost));
    }
    const RouteTraffic& t = tail ? r.tail[index] : r.traffic[index];
    const char* label = tail ? "TAIL" : "TRAFFIC";
    char suffix[48];
    const DiagnosticKind kind = static_cast<DiagnosticKind>(t.kind);
    if (kind == DiagnosticKind::CONTROLLER)
        put(suffix, sizeof suffix, " ctrl=%u", unsigned(t.state));
    else if (kind == DiagnosticKind::RECOVERY)
        put(suffix, sizeof suffix, " rec=%u", unsigned(t.state));
    else if (kind == DiagnosticKind::TX || kind == DiagnosticKind::TX_REPORT)
        put(suffix, sizeof suffix, " txk=%u txr=%lu txf=%lu txc=%u", unsigned(t.txKind),
            static_cast<unsigned long>(t.txRequest), static_cast<unsigned long>(t.txFrame), unsigned(t.txChannel));
    else
        suffix[0] = 0;
    // Only the fields each kind defines, exactly as main.cpp prints owner traffic.
    return put(out, size,
               "%s k=%u p=%llu id=%lx ext=%u rtr=%u dlc=%u data=%02x%02x%02x%02x%02x%02x%02x%02x ok=%u exact=%u "
               "t=%llu bounds=%u lo=%llu hi=%llu epoch=%lu%s\n",
               label, unsigned(t.kind), static_cast<unsigned long long>(t.processedUs),
               static_cast<unsigned long>(t.id), unsigned(t.flags & 1u), unsigned((t.flags >> 1) & 1u),
               unsigned(t.dlc), unsigned(t.data[0]), unsigned(t.data[1]), unsigned(t.data[2]),
               unsigned(t.data[3]), unsigned(t.data[4]), unsigned(t.data[5]), unsigned(t.data[6]),
               unsigned(t.data[7]), unsigned((t.flags >> 2) & 1u), unsigned((t.flags >> 3) & 1u),
               static_cast<unsigned long long>(t.receivedUs), unsigned((t.flags >> 4) & 1u),
               static_cast<unsigned long long>(t.earliestUs), static_cast<unsigned long long>(t.latestUs),
               static_cast<unsigned long>(t.epoch), suffix);
}
} // namespace MotorControlCANExample
