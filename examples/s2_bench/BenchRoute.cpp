// SPDX-License-Identifier: MIT
#include "BenchRoute.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdarg>
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
// StateBlock bits: ERRORS 0, MONITORS 2, STATUS 3, FEEDBACK 4.
constexpr uint8_t FULL = 0x1d;  // 1001, 603F, 2100..2102, 6041, 6061, 6064, 606C.
constexpr uint8_t IMAGE = 0x1c; // The DEC-1 accepted image: 2100..2102, 6041, 6061, 6064, 606C.
constexpr uint8_t STOP_STATE = 0x0c; // MONITORS and STATUS, what requestStop judges.
static_assert(FULL == (1u << unsigned(L::StateBlock::ERRORS) | 1u << unsigned(L::StateBlock::MONITORS) |
                       1u << unsigned(L::StateBlock::STATUS) | 1u << unsigned(L::StateBlock::FEEDBACK)),
              "FULL is ERRORS, MONITORS, STATUS and FEEDBACK");
static_assert(IMAGE == (FULL & ~(1u << unsigned(L::StateBlock::ERRORS))), "IMAGE drops ERRORS");
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
// REARM writes 6040 like the POSITION trigger, with the word the move lifecycle uses as its rearm.
static_assert(L::OBJECT_6040_00.widthBytes == 2 && !L::OBJECT_6040_00.signedValue,
              "rearm write width follows the generated declaration");
// MOVE takes these enumerations as numbers from the host line.
static_assert(unsigned(L::MoveSignalSource::STATUSWORD) == 1 && unsigned(L::MoveSignalSource::MONITOR) == 2 &&
                  unsigned(L::CompletionRule::AFTER_ABSENCE) == 1 &&
                  unsigned(L::CompletionRule::AFTER_ACCEPTANCE) == 2,
              "the host line numbers MoveSignalSource and CompletionRule as declared");
const char* const names[] = {"none", "actions", "io",       "tuning", "mode1",
                             "enable", "stop",  "position", "rearm",  "move"};
static_assert(sizeof names / sizeof names[0] == unsigned(RouteKind::MOVE) + 1, "one name per route");
bool isMotion(RouteKind kind) { return kind >= RouteKind::MODE_ONE; }
// Routes that may plan a stop of their own, counted against STOP as well.
bool plansStop(const RoutePlan& p) {
    return (p.kind == RouteKind::POSITION || p.kind == RouteKind::MOVE) && p.stopKind;
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
struct Spec {
    int64_t low, high;
    bool sign;
};
// One decimal field and its separator. Digits only, a minus only where signed, ten digits at most.
bool field(const char*& p, const char* end, const Spec& spec, bool last, int64_t& output) {
    bool negative = false;
    if (spec.sign && p < end && *p == '-') {
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
constexpr Spec NODE = {1, 2, false}, TRIAL = {1, UINT32_MAX, false}, FLAG = {0, 1, false};
constexpr Spec SIGNED32 = {INT32_MIN, INT32_MAX, true}, ACTION_MS = {1, 55000, false};
constexpr Spec OPTIONAL_MS = {0, 55000, false}, ACTION_INTERVAL = {10, 10000, false};
constexpr Spec U16 = {0, UINT16_MAX, false}, MASK = {1, UINT16_MAX, false}, U32 = {0, UINT32_MAX, false};
constexpr Spec SOURCE = {1, 2, false}, TARGET = {-1000000, 1000000, true}, CYCLE_MS = {20, 5000, false};
constexpr Spec STOP_KIND = {0, 2, false}, STOP_AFTER = {0, 16, false}, STOP_INTERVAL = {0, 10000, false};
// One fixed field list per route letter, in flash; the parser keeps only the values on its stack.
constexpr Spec READS[] = {NODE, TRIAL};
constexpr Spec MODE[] = {NODE, TRIAL, FLAG, {1, 10, false}, {100, 5000, false}};
constexpr Spec ENABLE[] = {NODE,     TRIAL,    FLAG,      {1, 1, false},  {1, 2, false}, {0, 65535, false},
                           SIGNED32, SIGNED32, ACTION_MS, ACTION_INTERVAL};
constexpr Spec STOP[] = {NODE, TRIAL, FLAG, {1, 2, false}, ACTION_MS, OPTIONAL_MS, ACTION_INTERVAL};
constexpr Spec POSITION[] = {NODE,        TRIAL,         FLAG,          SIGNED32,          TARGET,
                             {0, 3000, false}, {0, 2000, false}, {0, 2000, false}, {1, 16, false}, CYCLE_MS,
                             STOP_KIND,   STOP_AFTER,    OPTIONAL_MS,   OPTIONAL_MS,       STOP_INTERVAL};
constexpr Spec REARM[] = {NODE, TRIAL, FLAG, {1, 16, false}, CYCLE_MS};
// Native ranges as prepareMoveAbsolute admits them: rpm 1..3000 and ramps 1..2000 ms (Q11).
constexpr Spec MOVE[] = {NODE,        TRIAL,         FLAG,          SIGNED32,       TARGET,
                         {1, 3000, false}, {1, 2000, false}, {1, 2000, false}, ACTION_MS, CYCLE_MS,
                         TRIAL,       SOURCE,        MASK,          U16,            SOURCE,
                         MASK,        U16,           SOURCE,        U32,            U16,
                         U16,         U32,           U32,           U32,            U16,
                         U16,         STOP_KIND,     STOP_AFTER,    OPTIONAL_MS,    OPTIONAL_MS,
                         STOP_INTERVAL};
constexpr unsigned FIELDS = sizeof MOVE / sizeof MOVE[0];
static_assert(FIELDS >= sizeof POSITION / sizeof POSITION[0], "MOVE has the longest field list");
// A planned stop names all of its policy; no stop names none of it. `cycles` bounds stopAfter.
bool stopPolicyValid(const RoutePlan& p, unsigned cycles) {
    return p.stopKind ? p.stopAfter && p.stopAfter <= cycles && p.stopDeadlineMs && p.stopIntervalMs >= 10 &&
                            (!p.bestEffortMs || p.bestEffortMs >= p.stopDeadlineMs)
                      : !p.stopAfter && !p.stopDeadlineMs && !p.bestEffortMs && !p.stopIntervalMs;
}
// Terminals each plan can produce at most, against ROUTE_TERMINALS. A session action's observation
// cycles are bounded by its deadline over its interval, plus the immediate first cycle(s).
unsigned stopTerminals(uint32_t deadlineMs, uint32_t intervalMs) {
    return 1 + 5 * (1 + deadlineMs / intervalMs) + 2; // Word, cycles, a held best-effort pair.
}
unsigned terminalBound(const RoutePlan& p) {
    switch (p.kind) {
    case RouteKind::READ_ACTIONS:
        return 9 + 3 + 3 + 7;
    case RouteKind::READ_IO:
        return 7 + 9 + 4 + 7 + 3 + 7;
    case RouteKind::READ_TUNING:
        return 7 + 6 + 6 + 12 + 7 + 7;
    case RouteKind::MODE_ONE:
        return 15 + 1 + 9 + 7u * (p.cycles - 1u);
    case RouteKind::ENABLE:
        return 9 + 2 + 3 + 7 * (3 + p.deadlineMs / p.intervalMs);
    case RouteKind::STOP:
        return 5 + stopTerminals(p.deadlineMs, p.intervalMs);
    case RouteKind::POSITION:
        return 9 + 3 + 1 + 5 + 7u * p.cycles +
               (p.stopKind ? stopTerminals(p.stopDeadlineMs, p.stopIntervalMs) : 0);
    case RouteKind::REARM:
        return 9 + 1 + 7u * p.cycles;
    case RouteKind::MOVE:
        // Identity, full read, four parameters, the trigger, the move's cycles (one begins at most
        // every interval, plus one cut short at the deadline) and its rearm.
        return 9 + 9 + 4 + 1 + 7 * (2 + p.deadlineMs / p.intervalMs) + 1 +
               (p.stopKind ? stopTerminals(p.stopDeadlineMs, p.stopIntervalMs) : 0);
    default:
        return ROUTE_TERMINALS + 1;
    }
}
// Longest time a plan's own steps or session action can need after admission, in milliseconds.
uint32_t durationBound(const RoutePlan& p) {
    const uint32_t stop = p.stopKind ? (p.bestEffortMs > p.stopDeadlineMs ? p.bestEffortMs : p.stopDeadlineMs) : 0;
    switch (p.kind) {
    case RouteKind::ENABLE:
        return p.deadlineMs;
    case RouteKind::STOP:
        return p.bestEffortMs > p.deadlineMs ? p.bestEffortMs : p.deadlineMs;
    case RouteKind::POSITION:
        return p.cycles * p.intervalMs + stop;
    case RouteKind::MOVE:
        return p.deadlineMs + stop;
    case RouteKind::MODE_ONE:
    case RouteKind::REARM:
        return p.cycles * p.intervalMs;
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
    RoutePlan plan;
    const Spec* specs = nullptr;
    unsigned count = 0;
    switch (text[0]) {
    case 'A':
    case 'I':
    case 'U':
        plan.kind = text[0] == 'A' ? RouteKind::READ_ACTIONS : text[0] == 'I' ? RouteKind::READ_IO
                                                                              : RouteKind::READ_TUNING;
        specs = READS;
        count = sizeof READS / sizeof READS[0];
        break;
    case 'M':
        plan.kind = RouteKind::MODE_ONE;
        specs = MODE;
        count = sizeof MODE / sizeof MODE[0];
        break;
    case 'E':
        plan.kind = RouteKind::ENABLE;
        specs = ENABLE;
        count = sizeof ENABLE / sizeof ENABLE[0];
        break;
    case 'S':
        plan.kind = RouteKind::STOP;
        specs = STOP;
        count = sizeof STOP / sizeof STOP[0];
        break;
    case 'P':
        plan.kind = RouteKind::POSITION;
        specs = POSITION;
        count = sizeof POSITION / sizeof POSITION[0];
        break;
    case 'R':
        plan.kind = RouteKind::REARM;
        specs = REARM;
        count = sizeof REARM / sizeof REARM[0];
        break;
    case 'G':
        plan.kind = RouteKind::MOVE;
        specs = MOVE;
        count = FIELDS;
        break;
    default:
        return false;
    }
    int64_t v[FIELDS] = {};
    const char* p = text + 1;
    const char* end = text + size;
    for (unsigned i = 0; i < count; ++i)
        if (!field(p, end, specs[i], i + 1 == count, v[i]))
            return false;
    plan.node = uint8_t(v[0]);
    plan.trial = uint32_t(v[1]);
    plan.attended = count > 2 && v[2] != 0;
    switch (plan.kind) {
    case RouteKind::MODE_ONE:
    case RouteKind::REARM:
        plan.cycles = uint16_t(v[3]);
        plan.intervalMs = uint32_t(v[4]);
        break;
    case RouteKind::ENABLE:
        plan.start = uint8_t(v[3]);
        plan.shaft = uint8_t(v[4]);
        plan.reviewedStatus = uint16_t(v[5]);
        plan.reviewedPositionTarget = int32_t(v[6]);
        plan.reviewedVelocityTarget = int32_t(v[7]);
        plan.deadlineMs = uint32_t(v[8]);
        plan.intervalMs = uint32_t(v[9]);
        break;
    case RouteKind::STOP:
        plan.stopKind = uint8_t(v[3]);
        plan.deadlineMs = uint32_t(v[4]);
        plan.bestEffortMs = uint32_t(v[5]);
        plan.intervalMs = uint32_t(v[6]);
        if (plan.bestEffortMs && plan.bestEffortMs < plan.deadlineMs)
            return false;
        break;
    case RouteKind::POSITION:
        plan.startPosition = int32_t(v[3]);
        plan.target = int32_t(v[4]);
        plan.rpm = uint32_t(v[5]);
        plan.rampUpMs = uint32_t(v[6]);
        plan.rampDownMs = uint32_t(v[7]);
        plan.cycles = uint16_t(v[8]);
        plan.intervalMs = uint32_t(v[9]);
        plan.stopKind = uint8_t(v[10]);
        plan.stopAfter = uint16_t(v[11]);
        plan.stopDeadlineMs = uint32_t(v[12]);
        plan.bestEffortMs = uint32_t(v[13]);
        plan.stopIntervalMs = uint32_t(v[14]);
        if (!stopPolicyValid(plan, plan.cycles))
            return false;
        break;
    case RouteKind::MOVE: {
        plan.startPosition = int32_t(v[3]);
        plan.target = int32_t(v[4]);
        plan.rpm = uint32_t(v[5]);
        plan.rampUpMs = uint32_t(v[6]);
        plan.rampDownMs = uint32_t(v[7]);
        plan.deadlineMs = uint32_t(v[8]);
        plan.intervalMs = uint32_t(v[9]);
        plan.record = uint32_t(v[10]);
        plan.acceptSource = uint8_t(v[11]);
        plan.acceptMask = uint16_t(v[12]);
        plan.acceptValue = uint16_t(v[13]);
        plan.completeSource = uint8_t(v[14]);
        plan.completeMask = uint16_t(v[15]);
        plan.completeValue = uint16_t(v[16]);
        plan.rule = uint8_t(v[17]);
        L::MoveFirmware& f = plan.firmware;
        f.deviceType = uint32_t(v[18]);
        f.hardwareVersion = uint16_t(v[19]);
        f.softwareVersion = uint16_t(v[20]);
        f.vendorId = uint32_t(v[21]);
        f.productCode = uint32_t(v[22]);
        f.revision = uint32_t(v[23]);
        f.modelCode = uint16_t(v[24]);
        f.firmwareCode = uint16_t(v[25]);
        plan.stopKind = uint8_t(v[26]);
        plan.stopAfter = uint16_t(v[27]);
        plan.stopDeadlineMs = uint32_t(v[28]);
        plan.bestEffortMs = uint32_t(v[29]);
        plan.stopIntervalMs = uint32_t(v[30]);
        // A signal value outside its own mask can never be reported; the library refuses it too.
        if ((plan.acceptValue & ~plan.acceptMask) || (plan.completeValue & ~plan.completeMask) ||
            !stopPolicyValid(plan, 16))
            return false;
        break;
    }
    default:
        break;
    }
    output = plan;
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
    // A session answers true for every sequence but its own, so asking both is exact.
    for (AxisSession* session : static_cast<BenchRoute*>(context)->sessions_)
        if (session && !session->preflight(sequence, transfer, now))
            return false;
    return true;
}
bool BenchRoute::attach(unsigned node, AxisSession& session) noexcept {
    if (node < 1 || node > 2 || sessions_[node - 1] || session.isOpen())
        return false;
    sessions_[node - 1] = &session;
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
    if (plan.kind == RouteKind::NONE || plan.kind > RouteKind::MOVE || plan.node < 1 || plan.node > 2)
        return "invalid";
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
    const bool motion = isMotion(plan.kind);
    if (motion && !plan.attended)
        return "unattended";
    const unsigned stop = plansStop(plan) ? 1 : 0;
    const unsigned stopCase = unsigned(RouteKind::STOP);
    if (motion && (motionTotal_ + 1u + stop > ROUTE_MOTION_TOTAL ||
                   motionCases_[unsigned(plan.kind)][index] + 1u > ROUTE_MOTION_PER_CASE ||
                   motionCases_[stopCase][index] + stop > ROUTE_MOTION_PER_CASE))
        return "budget";
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
    if ((plan.kind == RouteKind::ENABLE || plan.kind == RouteKind::MOVE) &&
        (!C::serviceEligibility(*node, now, eligibility) || !eligibility.ordinarySdo))
        return "eligibility";
    const C::SdoCompletion* control = session->latestControl();
    if ((plan.kind == RouteKind::POSITION || plan.kind == RouteKind::MOVE) &&
        (!control || control->transfer.data[0] != 0x0f || control->transfer.data[1] != 0))
        return "control";
    // Only the acknowledged trigger of a finished POSITION record: missing, uncertain or any other
    // latest word refuses, so a lost 000F or 001F can never be followed by another 000F.
    if (plan.kind == RouteKind::REARM && !rearmable(index, *session))
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
    RouteRecord& r = record_;
    r.plan = plan;
    r.failure = RouteFailure::NONE;
    r.why = "none";
    r.terminal = r.evidenceLost = r.tailOverflow = r.unsettled = false;
    r.step = 0;
    r.motionTotal = motionTotal_;
    r.caseCount = motionCases_[unsigned(plan.kind)][index];
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
    r.checkPresent = r.checkPassed = false;
    r.check = r.last = RouteSnapshot();
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
    static const Step rearm[] = {{STEP_STATE, FULL}, {STEP_CHECK_REARM, 0}, {STEP_REARM_WRITE, 0}, {STEP_OBSERVE, 0}};
    static const Step move[] = {{STEP_IDENTITY, 0}, {STEP_STATE, FULL}, {STEP_CHECK_MOVE, 0}, {STEP_MOVE, 0}};
    const Step* const tables[] = {nullptr, actions, io, tuning, mode, enable, stopSteps, position, rearm, move};
    static_assert(sizeof tables / sizeof tables[0] == unsigned(RouteKind::MOVE) + 1, "one table per route");
    steps_ = tables[unsigned(plan.kind)];
    stepIndex_ = subStep_ = 0;
    pending_ = stepActive_ = readActive_ = actionStarted_ = abortRequested_ = emcyHandled_ = false;
    stopAttempted_ = false;
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
        if (!tail && !record_.emcy && d.kind == DiagnosticKind::RX && d.frame.id == emcyId &&
            !d.frame.extended && !d.frame.rtr && d.frame.dlc >= 2 && (d.frame.data[0] | d.frame.data[1])) {
            record_.emcy = true;
            std::memcpy(record_.emcyData, d.frame.data, sizeof record_.emcyData);
        }
    }
}
void BenchRoute::finish(RouteFailure failure, const char* why, uint64_t now) noexcept {
    if (record_.terminal)
        return;
    record_.failure = failure;
    record_.why = why;
    record_.terminal = true;
    record_.completedUs = now;
    record_.step = stepIndex_;
    record_.unsettled = pending_;
    if (callbacks_.sample)
        callbacks_.sample(callbacks_.context, record_.after);
    services_.clearUploadExperiment(slot());
}
void BenchRoute::endAfterSettling(RouteFailure failure, const char* why) noexcept {
    if (endFailure_ == RouteFailure::NONE) {
        endFailure_ = failure;
        endWhy_ = why;
    }
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
        if (axis().enableActive())
            axis().cancelEnable(now, M::ActionOutcome::DRIVE_FAULT);
        if (axis().moveActive())
            axis().cancelMove(now, M::MoveInterruption::DRIVE_FAULT);
        endAfterSettling(RouteFailure::OBSERVATION, "emcy");
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
    if (record_.evidenceLost) {
        finish(RouteFailure::EVIDENCE_LOSS, "lost", now);
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
    // The session refuses a time behind its owner's, so cancel at the latest time either has seen.
    uint64_t at = now < lastUs_ ? lastUs_ : now;
    if (at < firmware_.owner().lastTimeUs())
        at = firmware_.owner().lastTimeUs();
    if (axis().enableActive())
        axis().cancelEnable(at, M::ActionOutcome::CANCELLED);
    if (axis().moveActive())
        axis().cancelMove(at, M::MoveInterruption::CANCELLED);
    return nullptr;
}
bool BenchRoute::release() noexcept {
    if (!occupied_ || !record_.terminal || pending_ || !firmware_.owner().localTxRetired(slot()))
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
    if (done.outcome != C::SdoOutcome::SUCCESS) {
        // A lost or refused reply leaves its effect unknown; nothing is sent after it.
        finish(RouteFailure::SERVICE, done.effectUncertain ? "uncertain" : "terminal", now);
        return true;
    }
    if (t.direction == C::SdoDirection::DOWNLOAD) {
        ++subStep_;
        const uint8_t kind = steps_[stepIndex_].kind;
        if (kind == STEP_POSITION_WRITES && subStep_ == 5) {
            // The acknowledged trigger a later REARM may follow, while it stays the latest word.
            triggerOperation_[slot()] = t.operation;
            triggerUs_[slot()] = done.completedUs;
        }
        if (kind == STEP_MODE_WRITE || kind == STEP_REARM_WRITE || subStep_ == 5)
            nextStep(now);
        return true;
    }
    if (readMode_ == READ_SINGLE) {
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
    case STEP_GROUP:
    case STEP_TUNING:
    case STEP_SINGLE:
    case STEP_IDENTITY: {
        const uint8_t mode = step.kind == STEP_STATE     ? READ_STATE
                             : step.kind == STEP_GROUP   ? READ_GROUP
                             : step.kind == STEP_TUNING  ? READ_TUNING
                             : step.kind == STEP_IDENTITY ? READ_IDENTITY
                                                          : READ_SINGLE;
        if (!readActive_ && !startRead(mode, step.arg, now))
            return;
        serviceRead(now);
        return;
    }
    case STEP_CHECK_MODE:
    case STEP_CHECK_POSITION:
    case STEP_CHECK_REARM:
    case STEP_CHECK_MOVE: {
        record_.checkPresent = true;
        record_.check = record_.last;
        const char* why = step.kind == STEP_CHECK_MODE       ? checkMode()
                          : step.kind == STEP_CHECK_POSITION ? checkPosition()
                          : step.kind == STEP_CHECK_REARM    ? checkRearm()
                                                             : checkMove();
        record_.checkPassed = why == nullptr;
        if (why)
            finish(RouteFailure::PREREQUISITE, why, now);
        else
            nextStep(now);
        return;
    }
    case STEP_MODE_WRITE:
    case STEP_POSITION_WRITES:
    case STEP_REARM_WRITE:
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
    if (steps_[stepIndex_].kind == STEP_OBSERVE)
        observeDone(now);
    else
        nextStep(now);
}
bool BenchRoute::snapshot(uint8_t mask, uint64_t now) noexcept {
    L::ReadTarget target;
    if (!currentTarget(target))
        return false;
    RouteSnapshot s;
    const L::StateBlock blocks[] = {L::StateBlock::ERRORS, L::StateBlock::MONITORS, L::StateBlock::STATUS,
                                    L::StateBlock::FEEDBACK};
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
            break;
        case L::StateBlock::STATUS:
            s.statusword = block_.statusword;
            s.mode = block_.modeRaw;
            break;
        default:
            s.position = block_.actualPosition;
            s.velocity = block_.actualVelocity;
            break;
        }
    }
    record_.last = s;
    return true;
}
void BenchRoute::admit(const C::SdoTransfer& transfer, uint64_t now) noexcept {
    // A reply can precede its final TX report; only the owner can say the last frame retired.
    if (!firmware_.owner().localTxRetired(slot()))
        return;
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
    const uint64_t deadline = now + ROUTE_OPERATION_US < cutoffUs_ ? now + ROUTE_OPERATION_US : cutoffUs_;
    const uint32_t operation = deadline > now ? axis().beginOperation(deadline, now) : 0;
    if (!operation) {
        finish(RouteFailure::INVALIDATED, "session", now);
        return false;
    }
    writeOperation_ = operation;
    writeDeadlineUs_ = deadline;
    if (steps_[stepIndex_].kind == STEP_POSITION_WRITES || steps_[stepIndex_].kind == STEP_REARM_WRITE)
        triggerOperation_[slot()] = 0; // From here no earlier trigger can be the latest word.
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
    } else if (steps_[stepIndex_].kind == STEP_REARM_WRITE) {
        if (subStep_)
            return false; // One word, never a second.
        t.index = L::OBJECT_6040_00.index; // 2B 40 60 00 0F 00 00 00.
        t.size = L::OBJECT_6040_00.widthBytes;
        value = 0x000f;
    } else {
        const L::ObjectDescriptor* const objects[] = {&L::OBJECT_607A_00, &L::OBJECT_6081_00,
                                                      &L::OBJECT_6083_00, &L::OBJECT_6084_00,
                                                      &L::OBJECT_6040_00};
        if (subStep_ >= 5)
            return false;
        t.index = objects[subStep_]->index;
        t.size = objects[subStep_]->widthBytes;
        const uint32_t values[] = {uint32_t(p.target), p.rpm, p.rampUpMs, p.rampDownMs, 0x001f};
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
    C::SdoTransfer t;
    if (!writeTransfer(t)) {
        finish(RouteFailure::ADMISSION, "codec", now);
        return;
    }
    admit(t, now);
}
const char* BenchRoute::checkMode() const noexcept {
    const RouteSnapshot& s = record_.last;
    if (axis().motionHistoryWritten() || modeAdmitted_[slot()])
        return "history";
    if (axis().latestControl())
        return "control";
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
// The fresh full read of an idle, enabled-looking drive in mode 1, from POSITION's prerequisite list.
// No mask beyond the documented 2100 bits and 6041 low bits is judged here.
const char* BenchRoute::checkIdle() const noexcept {
    const RouteSnapshot& s = record_.last;
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
    const C::SdoCompletion* control = axis().latestControl();
    if (!control || control->transfer.data[0] != 0x0f || control->transfer.data[1] != 0)
        return "control";
    if (const char* why = checkIdle())
        return why;
    if (record_.last.position != record_.plan.startPosition)
        return "position";
    return nullptr;
}
// Nothing about the position is asked: the 000F follows a finished experiment whose travel is the
// unknown. Every value read is kept in TRIAL_CHECK.
const char* BenchRoute::checkRearm() const noexcept {
    if (!rearmable(slot(), axis()))
        return "control";
    return checkIdle();
}
// The route's own fail-closed check before the library's: the library judges the same 000F, mode,
// low bits and 2100 again with its own age, and the acceptance report against the host's mask.
const char* BenchRoute::checkMove() const noexcept {
    if (axis().moveFenced())
        return "fenced";
    return checkPosition();
}
bool BenchRoute::rearmable(uint8_t index, const AxisSession& session) const noexcept {
    const C::SdoCompletion* c = session.latestControl();
    return c && triggerOperation_[index] && c->outcome == C::SdoOutcome::SUCCESS && c->protocolAcknowledged &&
           !c->effectUncertain && c->transfer.index == 0x6040 && !c->transfer.subindex && c->transfer.size == 2 &&
           c->transfer.data[0] == 0x1f && c->transfer.data[1] == 0 &&
           c->transfer.operation == triggerOperation_[index] && c->completedUs == triggerUs_[index];
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
    policy.options.maximumAgeUs = 5 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
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
    // keeps its default from construction; nothing is reset through a stack temporary.
    M::AxisConfig& axisConfig = moveAxes_[slot()];
    axisConfig.binding = axis().binding();
    axisConfig.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    axisConfig.nativeVelocityTime = M::TimeUnit::MINUTE;
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
    M::MoveRequest& request = moveRequest_;
    request.position.value = M::Rational(p.target);
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
    const uint64_t deadline = now + uint64_t(p.deadlineMs) * 1000;
    if (deadline > cutoffUs_) {
        finish(RouteFailure::REFUSED, "deadline", now);
        return;
    }
    const M::Status status = axis().requestMove(context_.read, identity_, evidence, request, profile, nullptr,
                                                axisConfig, nullptr, options, deadline, now);
    if (!status) {
        record_.statusPresent = true;
        record_.statusCode = uint8_t(status.code);
        record_.statusDetail = status.detail;
        record_.statusMessage = status.msg ? status.msg : "";
        finish(RouteFailure::REFUSED, "move", now);
        return;
    }
    record_.moveRequested = true;
    record_.actionRequestedUs = record_.moveRequestedUs = now;
    actionStarted_ = true;
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
// The session advances its own action in ProbeApp::service; this only waits for it to end.
void BenchRoute::serviceAction(uint64_t now) noexcept {
    if (record_.moveRequested) {
        serviceMove(now);
        if (axis().moveActive() || axis().stopActive())
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
    if (p.kind == RouteKind::POSITION && p.stopKind && record_.cycles == p.stopAfter) {
        startStop(p.stopKind, p.stopDeadlineMs, p.bestEffortMs, p.stopIntervalMs, context_.read, now);
        return;
    }
    if (record_.cycles >= p.cycles) {
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
    unsigned count = 2 + r.checkPresent + (r.last.blocks != 0) + r.emcy + r.statusPresent;
    if (r.plan.kind == RouteKind::REARM)
        count += 1;
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
        case RouteKind::REARM:
            return put(out, size, "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u cycles=%u interval=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), unsigned(p.cycles), static_cast<unsigned long>(p.intervalMs));
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
                       "down=%lu deadline=%lu interval=%lu kind=%u after=%u sd=%lu be=%lu si=%lu\n",
                       routeName(p.kind), unsigned(p.node), static_cast<unsigned long>(p.trial),
                       unsigned(p.attended), static_cast<long>(p.startPosition), static_cast<long>(p.target),
                       static_cast<unsigned long>(p.rpm), static_cast<unsigned long>(p.rampUpMs),
                       static_cast<unsigned long>(p.rampDownMs), static_cast<unsigned long>(p.deadlineMs),
                       static_cast<unsigned long>(p.intervalMs), unsigned(p.stopKind), unsigned(p.stopAfter),
                       static_cast<unsigned long>(p.stopDeadlineMs), static_cast<unsigned long>(p.bestEffortMs),
                       static_cast<unsigned long>(p.stopIntervalMs));
        default:
            return put(out, size, "TRIAL_PLAN route=%s node=%u trial=%lu attended=%u\n", routeName(p.kind),
                       unsigned(p.node), static_cast<unsigned long>(p.trial), unsigned(p.attended));
        }
    }
    if (index == 1)
        return put(out, size,
                   "TRIAL_RES stack0=%lu stack1=%lu heap0=%lu heap1=%lu heapMin=%lu largest=%lu gap=%llu "
                   "dropped=%lu admission=%u started=%llu completed=%llu\n",
                   static_cast<unsigned long>(r.before.stackFree), static_cast<unsigned long>(r.after.stackFree),
                   static_cast<unsigned long>(r.before.heapFree), static_cast<unsigned long>(r.after.heapFree),
                   static_cast<unsigned long>(r.after.heapMinimum), static_cast<unsigned long>(r.after.largestBlock),
                   static_cast<unsigned long long>(r.maxServiceGapUs), static_cast<unsigned long>(r.dropped),
                   unsigned(r.admission), static_cast<unsigned long long>(r.startedUs),
                   static_cast<unsigned long long>(r.completedUs));
    index -= 2;
    const RouteSnapshot* shots[] = {r.checkPresent ? &r.check : nullptr, r.last.blocks ? &r.last : nullptr};
    const char* labels[] = {"TRIAL_CHECK", "TRIAL_LAST"};
    for (unsigned i = 0; i < 2; ++i) {
        if (!shots[i])
            continue;
        if (index--)
            continue;
        const RouteSnapshot& s = *shots[i];
        return put(out, size,
                   "%s passed=%u blocks=%02x 1001=%02x 603f=%04x 2100=%04x 6041=%04x 6061=%02x 6064=%ld "
                   "606c=%ld\n",
                   labels[i], unsigned(i == 0 ? r.checkPassed : 1), unsigned(s.blocks), unsigned(s.errorRegister),
                   unsigned(s.errorCode), unsigned(s.monitor), unsigned(s.statusword), unsigned(s.mode),
                   static_cast<long>(s.position), static_cast<long>(s.velocity));
    }
    if (r.emcy && !index--)
        return put(out, size, "TRIAL_EMCY data=%02x%02x%02x%02x%02x%02x%02x%02x\n", unsigned(r.emcyData[0]),
                   unsigned(r.emcyData[1]), unsigned(r.emcyData[2]), unsigned(r.emcyData[3]),
                   unsigned(r.emcyData[4]), unsigned(r.emcyData[5]), unsigned(r.emcyData[6]),
                   unsigned(r.emcyData[7]));
    if (r.statusPresent && !index--)
        return put(out, size, "TRIAL_STATUS code=%u detail=%ld; %s\n", unsigned(r.statusCode),
                   static_cast<long>(r.statusDetail), r.statusMessage);
    if (p.kind == RouteKind::REARM && !index--) {
        // The 001F this 000F followed, and the session's latest word now: a lost 000F leaves none.
        const C::SdoCompletion* latest = axis().latestControl();
        return put(out, size, "TRIAL_REARM predop=%lu predus=%llu latest=%u word=%04x latestop=%lu\n",
                   static_cast<unsigned long>(r.rearmFollows), static_cast<unsigned long long>(r.rearmFollowsUs),
                   unsigned(latest != nullptr),
                   latest ? unsigned(latest->transfer.data[0] | unsigned(latest->transfer.data[1]) << 8) : 0u,
                   static_cast<unsigned long>(latest ? latest->transfer.operation : 0));
    }
    if (p.kind == RouteKind::MOVE && !index--) {
        // The host's measurement record as the firmware received it; never a firmware default.
        const L::MoveFirmware& f = p.firmware;
        return put(out, size,
                   "TRIAL_EVIDENCE record=%lu as=%u am=%04x av=%04x cs=%u cm=%04x cv=%04x rule=%u dt=%08lx hw=%04x "
                   "sw=%04x vid=%08lx pc=%08lx rev=%08lx mc=%04x fc=%04x\n",
                   static_cast<unsigned long>(p.record), unsigned(p.acceptSource), unsigned(p.acceptMask),
                   unsigned(p.acceptValue), unsigned(p.completeSource), unsigned(p.completeMask),
                   unsigned(p.completeValue), unsigned(p.rule), static_cast<unsigned long>(f.deviceType),
                   unsigned(f.hardwareVersion), unsigned(f.softwareVersion), static_cast<unsigned long>(f.vendorId),
                   static_cast<unsigned long>(f.productCode), static_cast<unsigned long>(f.revision),
                   unsigned(f.modelCode), unsigned(f.firmwareCode));
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
                   "tailLost=%u summaries=%u step=%u cycles=%u motion=%u case=%u unsettled=%u gap=%llu\n",
                   static_cast<unsigned long>(r.plan.trial), unsigned(r.plan.node), routeName(r.plan.kind),
                   unsigned(r.failure), r.why, unsigned(r.evidenceLost), unsigned(r.terminalCount),
                   unsigned(r.trafficCount), unsigned(r.tailCount), unsigned(r.tailOverflow), summaries,
                   unsigned(r.step), unsigned(r.cycles), unsigned(r.motionTotal), unsigned(r.caseCount),
                   unsigned(r.unsettled), static_cast<unsigned long long>(r.maxServiceGapUs));
    unsigned index = cursor - 1;
    if (index < summaries)
        return formatSummary(index, out, size);
    index -= summaries;
    if (index < r.terminalCount) {
        const RouteTerminal& t = r.terminals[index];
        return put(out, size,
                   "RESULT op=%lu req=%lu oreq=%lu step=%lu object=%04x:%02x dir=%u outcome=%u ack=%u possible=%u "
                   "uncertain=%u abort=%08lx cleanup=%u size=%u data=%02x%02x%02x%02x admitted=%llu "
                   "completed=%llu ne=%lu reason=%u st=%02x\n",
                   static_cast<unsigned long>(t.operation), static_cast<unsigned long>(t.request),
                   static_cast<unsigned long>(t.ownerRequest), static_cast<unsigned long>(t.step),
                   unsigned(t.index), unsigned(t.subindex), unsigned(t.direction), unsigned(t.outcome),
                   unsigned(t.flags & 1u), unsigned((t.flags >> 1) & 1u), unsigned((t.flags >> 2) & 1u),
                   static_cast<unsigned long>(t.abortCode), unsigned((t.flags >> 3) & 1u), unsigned(t.size),
                   unsigned(t.data[0]), unsigned(t.data[1]), unsigned(t.data[2]), unsigned(t.data[3]),
                   static_cast<unsigned long long>(t.admittedUs), static_cast<unsigned long long>(t.completedUs),
                   static_cast<unsigned long>(t.nodeGeneration), unsigned(t.reason), unsigned(t.stages));
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
        put(suffix, sizeof suffix, " txk=%u txr=%lu txf=%lu", unsigned(t.txKind),
            static_cast<unsigned long>(t.txRequest), static_cast<unsigned long>(t.txFrame));
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
