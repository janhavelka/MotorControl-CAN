// SPDX-License-Identifier: MIT
#include "Soak.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace MotorControlCANExample {
using MotorControlCANOwner::Completion;
using MotorControlCANOwner::DiagnosticKind;
using MotorControlCANOwner::DriverState;
using MotorControlCANOwner::Health;
using MotorControlCANOwner::OwnerResources;
using MotorControlCANOwner::RequestId;
using MotorControlCANOwner::Stats;
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
// Host policies, not vendor timing. The operation deadline is well inside the
// sixty seconds packet 11 allows a baseline trial.
const uint32_t QUEUE_TIMEOUT_US = 250000, RESPONSE_TIMEOUT_US = 250000;
const uint64_t OPERATION_DEADLINE_US = 2000000;
const uint64_t DUMP_POST_US = 50000;      // Traffic kept after the anomaly, such as a late TX report.
const uint64_t DUMP_DEADLINE_US = 5000000; // A stalled console cannot hold reads longer than this.
const uint64_t FINAL_TIMEOUT_US = 5000000; // A stalled console cannot hold SOAK_END longer than this.
const uint64_t FINISH_TIMEOUT_US = 10000000; // Past every operation deadline, for a stopped firmware.
// Value records: ten lines per second with a burst, enough for every first observation.
const uint32_t TOKEN_US = 100000, TOKEN_BURST = SOAK_EVENTS;
const unsigned VALUE_QUEUE_LIMIT = SOAK_EVENTS * 3 / 4;
const uint32_t LATENCY_LIMITS[SOAK_BUCKETS - 1] = {2000, 5000, 10000, 20000, 50000, 100000, 250000};

// Trace entry kinds beyond owner DiagnosticKind, which supplies RX..RECOVERY as 0..5.
const uint8_t TRACE_SUBMIT = 6;
const uint8_t FLAG_OK = 1, FLAG_EXTENDED = 2, FLAG_RTR = 4, FLAG_BOUNDS = 8, FLAG_EXACT = 16;

enum EventType : uint8_t { EVENT_TRANSITION, EVENT_ANOMALY, EVENT_OPERATION, EVENT_EMCY, EVENT_BOOT,
                           EVENT_NODE_STOP, EVENT_DUMP_ABANDONED };
enum Kind : uint8_t { KIND_NONE, KIND_TERMINAL, KIND_STATUS, KIND_VALUE, KIND_UNMATCHED, KIND_TX,
                      KIND_RX, KIND_TRACE, KIND_EMCY, KIND_BOOT, KIND_CONTROLLER, KIND_ADMISSION,
                      KIND_BUS, KIND_INTERNAL };
const char* const KIND_NAMES[] = {"none", "terminal", "status", "value", "unmatched", "tx", "rx",
                                  "trace", "emcy", "boot", "controller", "admission", "bus",
                                  "internal"};
enum Signal : uint8_t {
    // Per node, communication.
    SIG_HEALTH, SIG_STREAK, SIG_ERROR, SIG_NMT, SIG_ELIGIBILITY, SIG_HEARTBEAT, SIG_UNRESOLVED,
    SIG_EPOCH, SIG_SESSIONS, SIG_PRODUCER,
    // Per node, native values with their object in the name.
    SIG_6041, SIG_6061, SIG_2100, SIG_2101, SIG_2102, SIG_6064, SIG_606C, SIG_1000, SIG_1009,
    SIG_100A, SIG_1018_0, SIG_1018_1, SIG_1018_2, SIG_1018_3, SIG_2000, SIG_2001,
    // Interface.
    SIG_CONTROLLER, SIG_CONTROLLER_EPOCH, SIG_INHIBITED, SIG_RECOVERY, SIG_FIRMWARE, SIG_BUS_OFF,
    SIG_RECOVERIES, SIG_TWAI_ERRORS, SIG_TWAI_FLAGS, SIG_TWAI_DROP, SIG_RX_DROPPED, SIG_RX_LOST,
    SIG_TX_REJECTED, SIG_UNMATCHED, SIG_QUEUE_REJECTED, SIG_RESULTS_REJECTED, SIG_TRACE_OVERWRITTEN,
    SIG_NETWORK_DROPPED, SIG_SUBMIT_OVERWRITTEN, SIG_COUNT
};
struct SignalInfo {
    const char* name;
    bool limited;  // Value or counter: subject to the rate limit and the queue reserve.
    uint8_t kind;  // Anomaly kind of a change after the first observation; KIND_NONE for no anomaly.
};
const SignalInfo SIGNALS[SIG_COUNT] = {
    {"health", false, KIND_NONE}, // Judged by value in track().
    {"streak", false, KIND_NONE},
    {"lastError", false, KIND_NONE},
    {"nmt", false, KIND_STATUS},
    {"eligibility", false, KIND_NONE},
    {"heartbeat", false, KIND_NONE}, // Judged by value in track().
    {"unresolved", false, KIND_NONE}, // Judged by value in track().
    {"nodeEpoch", false, KIND_STATUS},
    {"serverSessions", false, KIND_STATUS},
    {"producer", false, KIND_NONE},
    {"6041", true, KIND_VALUE},
    {"6061", true, KIND_VALUE},
    {"2100", true, KIND_VALUE},
    {"2101", true, KIND_VALUE},
    {"2102", true, KIND_VALUE},
    {"6064", true, KIND_NONE},
    {"606c", true, KIND_NONE},
    {"1000", true, KIND_VALUE},
    {"1009", true, KIND_VALUE},
    {"100a", true, KIND_VALUE},
    {"1018:00", true, KIND_VALUE},
    {"1018:01", true, KIND_VALUE},
    {"1018:02", true, KIND_VALUE},
    {"1018:03", true, KIND_VALUE},
    {"2000", true, KIND_VALUE},
    {"2001", true, KIND_VALUE},
    {"controller", false, KIND_NONE}, // Judged by value in track().
    {"controllerEpoch", false, KIND_CONTROLLER},
    {"inhibited", false, KIND_NONE}, // Judged by value in track().
    {"recovery", false, KIND_CONTROLLER},
    {"firmware", false, KIND_INTERNAL},
    {"busOff", false, KIND_CONTROLLER},
    {"recoveries", false, KIND_CONTROLLER},
    {"twaiErrors", true, KIND_BUS},
    {"twaiFlags", true, KIND_BUS},
    {"twaiDropped", true, KIND_RX},
    {"rxDropped", true, KIND_RX},
    {"rxLost", true, KIND_RX},
    {"txRejected", true, KIND_TX},
    {"unmatched", true, KIND_UNMATCHED},
    {"queueRejected", true, KIND_ADMISSION},
    {"resultsRejected", true, KIND_ADMISSION},
    {"traceOverwritten", true, KIND_TRACE},
    {"networkDropped", true, KIND_TRACE},
    {"submitOverwritten", true, KIND_TRACE},
};
void add(uint32_t& value, uint32_t amount = 1) noexcept {
    value = amount > UINT32_MAX - value ? UINT32_MAX : value + amount;
}
template <class T> void highWater(T& value, T candidate) noexcept {
    if (candidate > value)
        value = candidate;
}
uint16_t offset16(uint64_t later, uint64_t earlier, bool& fits) noexcept {
    if (later < earlier || later - earlier > UINT16_MAX) {
        fits = false;
        return 0;
    }
    return uint16_t(later - earlier);
}
uint32_t word(const uint8_t* bytes) noexcept {
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 |
           uint32_t(bytes[3]) << 24;
}
bool number(const char*& text, const char* end, uint32_t& output) noexcept {
    if (text == end || *text < '0' || *text > '9')
        return false;
    uint32_t value = 0;
    for (; text != end && *text >= '0' && *text <= '9'; ++text) {
        const uint32_t digit = uint32_t(*text - '0');
        if (value > (UINT32_MAX - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    output = value;
    return true;
}
bool separated(const char*& text, const char* end, uint32_t& output) noexcept {
    if (text == end || *text != ',')
        return false;
    ++text;
    return number(text, end, output);
}
} // namespace

bool parseSoakCommand(const char* text, size_t size, SoakPlan& output) noexcept {
    if (!text || !size)
        return false;
    const char* cursor = text;
    const char* const end = text + size;
    uint32_t trial = 0, duration = 0, interval = 0, summary = 0, reads = 0;
    if (!number(cursor, end, trial) || !separated(cursor, end, duration) ||
        !separated(cursor, end, interval) || !separated(cursor, end, summary) ||
        !separated(cursor, end, reads) || cursor != end || !trial || !duration ||
        duration > 172800 || interval < 20 || interval > 60000 || !summary || summary > 3600 ||
        reads > 3)
        return false;
    output.trial = trial;
    output.durationS = duration;
    output.intervalMs = interval;
    output.summaryS = summary;
    output.monitors = (reads & 1) != 0;
    output.identity = (reads & 2) != 0;
    return true;
}

SoakTwaiTap::SoakTwaiTap(TwaiDriver inner) noexcept : inner_(inner) {}
TwaiDriver SoakTwaiTap::driver() noexcept {
    TwaiDriver result;
    result.context = this;
    result.start = [](void* p, const TwaiConfig& config, uint32_t epoch) {
        const SoakTwaiTap& self = *static_cast<SoakTwaiTap*>(p);
        return self.inner_.start(self.inner_.context, config, epoch);
    };
    result.stop = [](void* p) {
        const SoakTwaiTap& self = *static_cast<SoakTwaiTap*>(p);
        return self.inner_.stop(self.inner_.context);
    };
    result.submit = [](void* p, const M::Frame& frame, uint32_t serial) {
        SoakTwaiTap& self = *static_cast<SoakTwaiTap*>(p);
        const TwaiSubmit submitted = self.inner_.submit(self.inner_.context, frame, serial);
        if (submitted == TwaiSubmit::DEFERRED) {
            add(self.deferred_);
            return submitted;
        }
        if (self.present_)
            add(self.overwritten_);
        self.latest_.frame = frame;
        self.latest_.serial = serial;
        self.latest_.result = submitted;
        self.present_ = true;
        return submitted;
    };
    result.events = [](void* p) {
        const SoakTwaiTap& self = *static_cast<SoakTwaiTap*>(p);
        return self.inner_.events(self.inner_.context);
    };
    // Runs on the owner task, never in an ISR: CanBusOwner::service calls Transport::receive,
    // TwaiAdapter forwards it here with its turn time as the cutoff, and IdfTwaiDriver::receive
    // refuses any other task. Its ISR fills only the driver's own FIFO, so the copy needs no lock.
    result.receive = [](void* p, MotorControlCANOwner::ReceivedFrame& frame, uint64_t cutoff) {
        SoakTwaiTap& self = *static_cast<SoakTwaiTap*>(p);
        const bool received = self.inner_.receive(self.inner_.context, frame, cutoff);
        if (received && self.ring_) {
            ReceiveRing& ring = *self.ring_;
            if (ring.count == ReceiveRing::CAPACITY)
                add(ring.dropped);
            else {
                ring.frames[(ring.head + ring.count) % ReceiveRing::CAPACITY] = frame;
                ++ring.count;
            }
        }
        return received;
    };
    return result;
}
bool SoakTwaiTap::take(SoakSubmission& output) noexcept {
    if (!present_)
        return false;
    output = latest_;
    present_ = false;
    return true;
}
void SoakTwaiTap::copyReceives(ReceiveRing* ring) noexcept {
    if (ring) {
        ring->head = ring->count = 0;
        ring->dropped = 0;
    }
    ring_ = ring;
}
bool SoakTwaiTap::takeReceived(MotorControlCANOwner::ReceivedFrame& output) noexcept {
    if (!ring_ || !ring_->count)
        return false;
    output = ring_->frames[ring_->head];
    ring_->head = uint8_t((ring_->head + 1) % ReceiveRing::CAPACITY);
    --ring_->count;
    return true;
}

Soak::Soak(FirmwareProbe& firmware, C::SdoClient& services, SoakTwaiTap& tap,
           const SoakCallbacks& callbacks, const char* revision) noexcept
    : firmware_(firmware), services_(services), tap_(tap), callbacks_(callbacks),
      revision_(revision) {
    static_assert(unsigned(SIG_COUNT) <= unsigned(SIGNAL_SLOTS), "Every signal needs a retained value");
    static_assert(unsigned(SIG_COUNT) <= 64, "Known bits are one 64-bit word per scope");
    std::memset(static_cast<void*>(&s_), 0, sizeof s_);
}

SoakAdmission Soak::begin(const SoakPlan& plan, uint64_t now) noexcept {
    if (active())
        return SoakAdmission::BUSY;
    if (!plan.trial || plan.trial <= lastTrial_ || !plan.durationS || plan.durationS > 172800 ||
        plan.intervalMs < 20 || plan.intervalMs > 60000 || !plan.summaryS || plan.summaryS > 3600 ||
        !firmware_.running() || now < firmware_.owner().lastTimeUs() ||
        now > UINT64_MAX - uint64_t(plan.durationS) * 1000000u)
        return SoakAdmission::INVALID;
    for (unsigned slot = 0; slot < SOAK_NODES; ++slot)
        if (!services_.channel(uint8_t(slot)) || !services_.node(uint8_t(slot)))
            return SoakAdmission::INVALID;
    if (firmware_.app().readContext().state != M::ReadState::EMPTY || firmware_.evidence().record())
        return SoakAdmission::BUSY;
    const OwnerResources resources = firmware_.owner().resources();
    if (resources.queued || resources.active || resources.retained || resources.reservations ||
        resources.nmtPending || resources.nmtRetained)
        return SoakAdmission::BUSY;
    std::memset(static_cast<void*>(&s_), 0, sizeof s_);
    s_.phase = PHASE_RUNNING;
    s_.plan = plan;
    s_.startUs = s_.lastServiceUs = s_.tokenUs = now;
    s_.endUs = now + uint64_t(plan.durationS) * 1000000u;
    s_.nextOpUs = now + uint64_t(plan.intervalMs) * 1000u;
    s_.nextSummaryUs = now + uint64_t(plan.summaryS) * 1000000u;
    s_.tokens = TOKEN_BURST;
    s_.beginPending = true;
    for (SoakNodeStats& node : s_.nodes)
        node.latencyMinUs = UINT32_MAX;
    lastTrial_ = plan.trial;
    if (callbacks_.sample)
        callbacks_.sample(callbacks_.context, s_.resources);
    // The first status pass prints every first observation at this boundary.
    trackInterface(now);
    trackNodes(now);
    return SoakAdmission::ACCEPTED;
}

bool Soak::requestStop() noexcept {
    if (s_.phase != PHASE_RUNNING)
        return false;
    s_.stopRequested = true;
    return true;
}

Soak::TraceEntry& Soak::addTrace(uint64_t processedUs) noexcept {
    TraceEntry& entry = s_.trace[s_.traceNext % SOAK_TRACE];
    ++s_.traceNext;
    std::memset(static_cast<void*>(&entry), 0, sizeof entry);
    entry.processedUs = processedUs;
    return entry;
}

void Soak::copyDiagnostics(uint64_t now) noexcept {
    CanBusOwner& owner = firmware_.owner();
    highWater(s_.pressure.traceHighWater, owner.resources().traceUsed);
    for (unsigned i = 0; i < CanBusOwner::TRACE_CAPACITY && owner.takeDiagnostic(diagnostic_); ++i) {
        const auto& in = diagnostic_;
        if (in.kind == DiagnosticKind::TERMINAL)
            continue; // recordStep copies it when the terminal is consumed.
        TraceEntry& entry = addTrace(in.processedUs);
        entry.kind = uint8_t(in.kind);
        entry.id = in.frame.id;
        entry.dlc = in.frame.dlc;
        std::memcpy(entry.data, in.frame.data, sizeof entry.data);
        entry.flags = uint8_t((in.accepted ? FLAG_OK : 0) | (in.frame.extended ? FLAG_EXTENDED : 0) |
                              (in.frame.rtr ? FLAG_RTR : 0));
        entry.epoch = uint8_t(in.controllerEpoch > UINT8_MAX ? UINT8_MAX : in.controllerEpoch);
        if (in.kind == DiagnosticKind::CONTROLLER)
            entry.state = uint8_t(in.controller);
        else if (in.kind == DiagnosticKind::RECOVERY)
            entry.state = uint8_t(in.recovery);
        else if (in.kind == DiagnosticKind::TX || in.kind == DiagnosticKind::TX_REPORT) {
            entry.state = uint8_t(in.txId.kind);
            entry.txRequest = uint16_t(in.txId.kind == MotorControlCANOwner::TransportTxId::Kind::NMT
                                           ? in.txId.nmt.generation
                                           : in.txId.sdo.request.generation);
            entry.txFrame = uint8_t(in.txId.sdo.frame > UINT8_MAX ? UINT8_MAX : in.txId.sdo.frame);
        } else if (in.kind == DiagnosticKind::RX) {
            bool fits = true;
            const uint64_t earliest = in.receivedTimeKnown ? in.receivedUs : in.receivedEarliestUs;
            const uint64_t latest = in.receivedTimeKnown ? in.receivedUs : in.receivedLatestUs;
            if (in.receivedTimeKnown || in.receiveBoundsKnown) {
                entry.lowOffset = offset16(in.processedUs, earliest, fits);
                entry.highOffset = offset16(in.processedUs, latest, fits);
                // A saturated offset would claim a tighter bound than the evidence.
                if (fits)
                    entry.flags = uint8_t(entry.flags | FLAG_BOUNDS |
                                          (in.receivedTimeKnown ? FLAG_EXACT : 0));
            }
            if (!in.accepted)
                s_.lastRejectedId = in.frame.id;
        }
        if ((in.kind == DiagnosticKind::TX || in.kind == DiagnosticKind::TX_REPORT) && !in.accepted)
            anomaly(KIND_TX, 0, in.frame.id, uint32_t(in.kind), now);
    }
}

void Soak::copySubmission(uint64_t now) noexcept {
    SoakSubmission submitted;
    if (!tap_.take(submitted))
        return;
    TraceEntry& entry = addTrace(now);
    entry.kind = TRACE_SUBMIT;
    entry.id = submitted.frame.id;
    entry.dlc = submitted.frame.dlc;
    std::memcpy(entry.data, submitted.frame.data, sizeof entry.data);
    entry.flags = submitted.result == TwaiSubmit::SENT ? FLAG_OK : 0;
    entry.state = uint8_t(submitted.result);
    entry.txRequest = uint16_t(submitted.serial);
    entry.epoch = uint8_t(firmware_.owner().controllerEpoch());
    if (submitted.result != TwaiSubmit::SENT)
        anomaly(KIND_TX, 0, submitted.frame.id, uint32_t(submitted.result), now);
}

void Soak::copyObservations(uint64_t now) noexcept {
    for (unsigned i = 0; i < C::SdoClient::NETWORK_HISTORY &&
                         services_.takeNetworkObservation(observation_);
         ++i) {
        const C::NetworkObservation& seen = observation_;
        const unsigned address = unsigned(seen.frame.id & 0x7f);
        const unsigned scope = address >= 1 && address <= SOAK_NODES ? address : 0;
        const uint64_t processed = seen.time.processedUs;
        if (seen.decoded && seen.event.kind == C::NetworkEventKind::HEARTBEAT &&
            seen.disposition == C::NetworkDisposition::ACCEPTED && scope) {
            const unsigned slot = scope - 1;
            add(s_.nodes[slot].heartbeats);
            if (s_.heartbeatSeen[slot] && processed >= s_.heartbeatUs[slot]) {
                const uint64_t gap = processed - s_.heartbeatUs[slot];
                highWater<uint32_t>(s_.nodes[slot].heartbeatGapMaxUs,
                                    gap > UINT32_MAX ? uint32_t(UINT32_MAX) : uint32_t(gap));
            }
            s_.heartbeatSeen[slot] = true;
            s_.heartbeatUs[slot] = processed;
            continue;
        }
        const bool boot = seen.decoded && seen.event.kind == C::NetworkEventKind::BOOTUP;
        const bool emcy = seen.decoded && seen.event.kind == C::NetworkEventKind::EMCY;
        if (boot || emcy)
            push(boot ? EVENT_BOOT : EVENT_EMCY, scope, 0, false, word(seen.frame.data),
                 word(seen.frame.data + 4), uint32_t(seen.disposition), seen.frame.dlc, processed,
                 false);
        // An unknown identifier reaches the owner's unmatched counter too, which reports it.
        if (seen.disposition != C::NetworkDisposition::UNKNOWN_ROUTE || boot || emcy)
            anomaly(boot ? KIND_BOOT : emcy ? KIND_EMCY : KIND_RX, scope, seen.frame.id,
                    uint32_t(seen.disposition), now);
    }
}

void Soak::anomaly(unsigned kind, unsigned scope, uint32_t a, uint32_t b, uint64_t now) noexcept {
    add(s_.pressure.anomalies);
    if (s_.anomalyId != UINT32_MAX)
        ++s_.anomalyId;
    bool full = false;
    if (s_.dump == DUMP_NONE && s_.kindDumps[kind] < SOAK_DUMPS_PER_KIND &&
        s_.pressure.dumps < SOAK_DUMPS && s_.phase != PHASE_IDLE) {
        full = true;
        ++s_.kindDumps[kind];
        add(s_.pressure.dumps);
        s_.dump = DUMP_COLLECT;
        s_.dumpId = s_.anomalyId;
        s_.dumpStartUs = now;
        s_.dumpPostUs = now + DUMP_POST_US;
        s_.dumpFirst = s_.traceNext > SOAK_DUMP_ENTRIES ? s_.traceNext - SOAK_DUMP_ENTRIES : 0;
    } else
        add(s_.pressure.dumpsSkipped);
    // The anomaly line is status: it is kept even when the value class is limited.
    push(EVENT_ANOMALY, scope, kind, full, s_.anomalyId, a, b, 0, now, false);
}

void Soak::push(uint8_t type, unsigned node, unsigned signal, bool flag, uint32_t a, uint32_t b,
                uint32_t c, uint32_t d, uint64_t now, bool limited) noexcept {
    if (limited) {
        // Refill in whole tokens; the remainder stays in tokenUs.
        if (now > s_.tokenUs) {
            const uint64_t earned = (now - s_.tokenUs) / TOKEN_US;
            if (earned) {
                s_.tokenUs += earned * TOKEN_US;
                s_.tokens = earned >= TOKEN_BURST - s_.tokens ? uint32_t(TOKEN_BURST)
                                                              : s_.tokens + uint32_t(earned);
            }
        }
        if (!s_.tokens) {
            add(s_.pressure.eventsLimited);
            return;
        }
        if (s_.eventCount >= VALUE_QUEUE_LIMIT) {
            add(s_.pressure.eventsFull);
            return;
        }
        --s_.tokens;
    } else if (s_.eventCount == SOAK_EVENTS) {
        add(s_.pressure.eventsFull);
        return;
    }
    Event& event = s_.events[(s_.eventHead + s_.eventCount) % SOAK_EVENTS];
    ++s_.eventCount;
    highWater(s_.pressure.eventHighWater, s_.eventCount);
    event.timeUs = now;
    event.a = a;
    event.b = b;
    event.c = c;
    event.d = d;
    event.type = type;
    event.node = uint8_t(node);
    event.signal = uint8_t(signal);
    event.flag = flag ? 1 : 0;
}

void Soak::track(unsigned scope, unsigned signal, uint32_t value, uint64_t now) noexcept {
    const uint64_t bit = uint64_t(1) << signal;
    const bool first = (s_.known[scope] & bit) == 0;
    const uint32_t previous = s_.last[scope][signal];
    if (!first && previous == value)
        return;
    s_.known[scope] |= bit;
    s_.last[scope][signal] = value;
    const uint32_t nodeEpoch = scope ? s_.last[scope][SIG_EPOCH] : 0;
    const uint32_t controllerEpoch = firmware_.owner().controllerEpoch();
    push(EVENT_TRANSITION, scope, signal, first, previous, value, nodeEpoch, controllerEpoch, now,
         SIGNALS[signal].limited);
    if (first)
        return;
    unsigned kind = SIGNALS[signal].kind;
    if (signal == SIG_HEALTH && value != uint32_t(DriverState::READY))
        kind = KIND_STATUS;
    else if (signal == SIG_HEARTBEAT &&
             (value == uint32_t(C::HeartbeatFreshness::EXPIRED) ||
              value == uint32_t(C::HeartbeatFreshness::UNQUALIFIED)))
        kind = KIND_STATUS;
    else if ((signal == SIG_UNRESOLVED || signal == SIG_INHIBITED) && value)
        kind = signal == SIG_INHIBITED ? KIND_CONTROLLER : KIND_STATUS;
    else if (signal == SIG_CONTROLLER &&
             (value == uint32_t(C::ControllerState::BUS_OFF) ||
              value == uint32_t(C::ControllerState::RECOVERING)))
        kind = KIND_CONTROLLER;
    // A value or counter raises one anomaly per summary window. Under a lasting fault, such as
    // a missing ACK, twaiErrors changed on every pass, and one line per change starved the
    // summaries for 3 h (2026-11-03). Its transitions and the summary totals carry the rest.
    if (kind != KIND_NONE && SIGNALS[signal].limited) {
        if (s_.raised[scope] & bit)
            return;
        s_.raised[scope] |= bit;
    }
    // A counter says how many; the rejected frame's identifier says which.
    if (kind != KIND_NONE)
        anomaly(kind, scope, signal == SIG_UNMATCHED ? s_.lastRejectedId : previous, value, now);
}

void Soak::trackInterface(uint64_t now) noexcept {
    CanBusOwner& owner = firmware_.owner();
    const Stats& stats = owner.stats();
    const OwnerResources resources = owner.resources();
    const TwaiDiagnostics twai = firmware_.transport().diagnostics();
    highWater(s_.pressure.queuedHighWater, resources.queued);
    highWater(s_.pressure.activeHighWater, resources.active);
    highWater(s_.pressure.retainedHighWater, resources.retained);
    track(0, SIG_CONTROLLER, uint32_t(services_.controllerState()), now);
    track(0, SIG_CONTROLLER_EPOCH, owner.controllerEpoch(), now);
    track(0, SIG_INHIBITED, (resources.inhibited || twai.inhibited) ? 1 : 0, now);
    track(0, SIG_RECOVERY, resources.recoveryPending ? 1 : 0, now);
    track(0, SIG_FIRMWARE, uint32_t(firmware_.failure()), now);
    track(0, SIG_BUS_OFF, stats.busOffEvents, now);
    track(0, SIG_RECOVERIES, stats.recoveries, now);
    track(0, SIG_TWAI_ERRORS, twai.busErrors, now);
    track(0, SIG_TWAI_FLAGS, twai.errorFlags, now);
    track(0, SIG_TWAI_DROP, twai.rxDropped, now);
    track(0, SIG_RX_DROPPED, stats.rxDropped, now);
    track(0, SIG_RX_LOST, stats.rxLost, now);
    track(0, SIG_TX_REJECTED, stats.txRejected, now);
    track(0, SIG_UNMATCHED, stats.unmatchedFrames, now);
    track(0, SIG_QUEUE_REJECTED, stats.queueRejected, now);
    track(0, SIG_RESULTS_REJECTED, stats.resultsRejected, now);
    track(0, SIG_TRACE_OVERWRITTEN, stats.traceOverwritten, now);
    track(0, SIG_NETWORK_DROPPED, services_.droppedNetworkObservations(), now);
    track(0, SIG_SUBMIT_OVERWRITTEN, tap_.overwritten(), now);
}

void Soak::trackNodes(uint64_t now) noexcept {
    for (unsigned slot = 0; slot < SOAK_NODES; ++slot) {
        const unsigned scope = slot + 1;
        const C::SdoChannel* channel = services_.channel(uint8_t(slot));
        const C::NodeSnapshot* node = services_.node(uint8_t(slot));
        if (!channel || !node)
            continue;
        // Node epoch first, so that every later line of this pass carries the new one.
        track(scope, SIG_EPOCH, node->route.generation, now);
        Health health;
        if (firmware_.owner().health(channel->binding, health)) {
            track(scope, SIG_HEALTH, uint32_t(health.state), now);
            track(scope, SIG_STREAK, health.consecutiveFailures, now);
            track(scope, SIG_ERROR,
                  uint32_t(health.lastError.code) << 16 | (uint32_t(health.lastError.detail) & 0xffff),
                  now);
        }
        C::ServiceEligibility eligibility;
        const bool eligible = C::serviceEligibility(*node, now, eligibility);
        // Raw observed state, with 0xff for no observation in this epoch.
        track(scope, SIG_NMT, node->nmtTime.present ? uint32_t(node->nmt) : 0xffu, now);
        if (eligible)
            track(scope, SIG_ELIGIBILITY,
                  uint32_t(eligibility.exactNmtKnown) | uint32_t(eligibility.ordinarySdo) << 1 |
                      uint32_t(eligibility.probeUpload) << 2 |
                      uint32_t(node->serviceQualification.present) << 3,
                  now);
        C::HeartbeatFreshness freshness;
        // Host policy: three producer intervals, or a placeholder age while none is known.
        const uint64_t age =
            node->producerKnown && node->producerTimeMs ? uint64_t(node->producerTimeMs) * 3000u : 1000000u;
        if (C::heartbeatFreshness(*node, now, age, freshness))
            track(scope, SIG_HEARTBEAT, uint32_t(freshness), now);
        track(scope, SIG_UNRESOLVED, services_.unresolved(uint8_t(slot)) ? 1 : 0, now);
        track(scope, SIG_SESSIONS, node->serverSessions, now);
        track(scope, SIG_PRODUCER,
              uint32_t(node->producerKnown) << 16 | uint32_t(node->producerTimeMs), now);
        if (s_.phase == PHASE_RUNNING)
            stopIfUnusable(slot, now);
    }
}

bool Soak::stopIfUnusable(unsigned slot, uint64_t now) noexcept {
    SoakNodeStats& stats = s_.nodes[slot];
    if (stats.stopped)
        return true;
    const C::NodeSnapshot* node = services_.node(uint8_t(slot));
    const bool unresolved = services_.unresolved(uint8_t(slot));
    if (!unresolved && node && !node->exhausted)
        return false;
    stats.stopped = true;
    push(EVENT_NODE_STOP, slot + 1, unresolved ? 0 : 1, false, s_.operation, 0, 0, 0, now, false);
    return true;
}

void Soak::recordStep(const Completion& done, uint64_t now) noexcept {
    const C::SdoCompletion& service = done.service;
    SoakNodeStats& stats = s_.nodes[s_.opSlot];
    add(stats.steps);
    if (unsigned(service.outcome) < SOAK_OUTCOMES)
        add(stats.outcomes[unsigned(service.outcome)]);
    if (unsigned(done.reason) < SOAK_REASONS)
        add(stats.reasons[unsigned(done.reason)]);
    if (service.transmissionPossible && service.completedUs >= done.admittedUs) {
        const uint64_t elapsed = service.completedUs - done.admittedUs;
        const uint32_t latency = elapsed > UINT32_MAX ? uint32_t(UINT32_MAX) : uint32_t(elapsed);
        add(stats.latencyCount);
        stats.latencySumUs += latency;
        if (latency < stats.latencyMinUs)
            stats.latencyMinUs = latency;
        highWater<uint32_t>(stats.latencyMaxUs, latency);
        unsigned bucket = 0;
        while (bucket < SOAK_BUCKETS - 1 && latency > LATENCY_LIMITS[bucket])
            ++bucket;
        add(stats.buckets[bucket]);
    }
    if (service.abortValid) {
        bool counted = false;
        for (unsigned i = 0; i < stats.abortCodes; ++i)
            if (stats.abortCode[i] == service.abortCode) {
                add(stats.abortCount[i]);
                counted = true;
            }
        if (!counted && stats.abortCodes < SOAK_ABORTS) {
            stats.abortCode[stats.abortCodes] = service.abortCode;
            stats.abortCount[stats.abortCodes++] = 1;
        } else if (!counted)
            add(stats.abortOther);
    }
    // Stages as flag bits: admission and handoff, TX, reply, effect, cleanup.
    const uint8_t flags = uint8_t((done.adapterAdmitted ? 1 : 0) | (done.controllerAccepted ? 2 : 0) |
                                  (service.queued ? 4 : 0) | (service.transmissionPossible ? 8 : 0) |
                                  (service.transmissionConfirmed ? 16 : 0) |
                                  (service.protocolAcknowledged ? 32 : 0) |
                                  (service.effectUncertain ? 64 : 0) |
                                  (service.clientAbortQueued ? 128 : 0));
    TraceEntry& entry = addTrace(service.completedUs);
    entry.kind = uint8_t(DiagnosticKind::TERMINAL);
    entry.id = uint32_t(service.transfer.index) << 8 | service.transfer.subindex;
    entry.dlc = service.size;
    entry.data[0] = uint8_t(service.outcome);
    entry.data[1] = uint8_t(done.reason);
    entry.data[2] = flags;
    entry.data[3] = service.replyValid ? 1 : 0;
    entry.data[4] = uint8_t(service.abortCode);
    entry.data[5] = uint8_t(service.abortCode >> 8);
    entry.data[6] = uint8_t(service.abortCode >> 16);
    entry.data[7] = uint8_t(service.abortCode >> 24);
    bool fits = true;
    entry.lowOffset = offset16(service.completedUs, done.admittedUs, fits);
    entry.flags = uint8_t((service.outcome == C::SdoOutcome::SUCCESS ? FLAG_OK : 0) |
                          (fits ? FLAG_BOUNDS : 0));
    entry.txRequest = uint16_t(service.transfer.operation);
    entry.txFrame = uint8_t(service.transfer.step);
    entry.state = uint8_t(s_.opSlot + 1);
    entry.epoch = uint8_t(firmware_.owner().controllerEpoch());
    if (service.outcome == C::SdoOutcome::SUCCESS) {
        add(stats.stepsOk);
        // The ordering packet 5 asks to measure: on an abort, this same race quarantines.
        if (!service.transmissionConfirmed)
            add(stats.earlyReplies);
        return;
    }
    push(EVENT_OPERATION, s_.opSlot + 1, unsigned(service.outcome), false, service.transfer.operation,
         service.abortCode,
         uint32_t(service.transfer.index) << 16 | uint32_t(service.transfer.subindex) << 8 |
             (service.transfer.step & 0xff),
         uint32_t(done.reason) << 8 | flags, now, false);
    anomaly(KIND_TERMINAL, s_.opSlot + 1, service.transfer.operation, service.abortCode, now);
}

void Soak::value(unsigned slot, unsigned signal, uint32_t raw, uint64_t now) noexcept {
    track(slot + 1, signal, raw, now);
}

void Soak::publish(unsigned slot, uint64_t now) noexcept {
    ProbeApp& app = firmware_.app();
    const L::ReadContext& read = app.readContext();
    // Age covers the operation's own span; nothing here renews a value.
    const uint64_t age = OPERATION_DEADLINE_US * 2;
    bool complete = true;
    if (read.kind == L::ReadKind::IDENTITY) {
        complete = app.getIdentity(now, age, identityValues_);
        if (complete) {
            const L::IdentityObservation& v = identityValues_;
            value(slot, SIG_1000, v.deviceType, now);
            value(slot, SIG_1009, v.hardwareVersion, now);
            value(slot, SIG_100A, v.softwareVersion, now);
            value(slot, SIG_1018_0, v.identityEntries, now);
            value(slot, SIG_1018_1, v.vendorId, now);
            value(slot, SIG_1018_2, v.productCode, now);
            value(slot, SIG_1018_3, v.revision, now);
            value(slot, SIG_2000, v.modelCode, now);
            value(slot, SIG_2001, v.firmwareCode, now);
        }
    } else if (read.blockMask & (1u << unsigned(L::StateBlock::MONITORS))) {
        complete = app.getStateBlock(L::StateBlock::MONITORS, now, age, stateValues_);
        if (complete) {
            value(slot, SIG_2100, stateValues_.monitor, now);
            value(slot, SIG_2101, stateValues_.logicalInputs, now);
            value(slot, SIG_2102, stateValues_.logicalOutputs, now);
        }
    } else {
        complete = app.getStateBlock(L::StateBlock::STATUS, now, age, stateValues_);
        if (complete) {
            value(slot, SIG_6041, stateValues_.statusword, now);
            value(slot, SIG_6061, stateValues_.modeRaw, now);
            complete = app.getStateBlock(L::StateBlock::FEEDBACK, now, age, stateValues_);
            if (complete) {
                value(slot, SIG_6064, uint32_t(stateValues_.actualPosition), now);
                value(slot, SIG_606C, uint32_t(stateValues_.actualVelocity), now);
            }
        }
    }
    if (complete)
        add(s_.nodes[slot].operationsOk);
    else {
        // Every field succeeded and the checked getter still refused: the typed path disagrees.
        add(s_.nodes[slot].operationsFailed);
        anomaly(KIND_INTERNAL, slot + 1, read.operation, uint32_t(read.kind), now);
    }
}

void Soak::finishOperation(uint64_t now) noexcept {
    ProbeApp& app = firmware_.app();
    const L::ReadContext& read = app.readContext();
    const unsigned slot = s_.opSlot;
    if (unsigned(read.outcome) < SOAK_READ_OUTCOMES)
        add(s_.nodes[slot].readOutcomes[unsigned(read.outcome)]);
    if (read.state == M::ReadState::SUCCEEDED)
        publish(slot, now);
    else {
        add(s_.nodes[slot].operationsFailed);
        // A failure with no owner terminal is local: reservation, deadline or refused admission.
        if (!read.evidencePresent || read.evidence.transfer.operation != read.operation ||
            read.evidence.outcome == C::SdoOutcome::SUCCESS) {
            push(EVENT_OPERATION, slot + 1, unsigned(C::SdoOutcome::TRANSPORT_ERROR), true, read.operation,
                 0, uint32_t(read.kind) << 8 | read.cursor,
                 uint32_t(read.outcome) << 16 | (uint32_t(read.status.detail) & 0xffff), now, false);
            anomaly(KIND_ADMISSION, slot + 1, read.operation, uint32_t(read.outcome), now);
        }
    }
    s_.opActive = false;
    s_.retireSinceUs[slot] = now;
    s_.retireWait[slot] = true;
    s_.releasePending = !app.releaseRead();
}

void Soak::advance(uint64_t now) noexcept {
    ProbeApp& app = firmware_.app();
    if (s_.releasePending)
        s_.releasePending = !app.releaseRead();
    for (unsigned slot = 0; slot < SOAK_NODES; ++slot)
        if (s_.retireWait[slot] && firmware_.owner().localTxRetired(uint8_t(slot))) {
            s_.retireWait[slot] = false;
            const uint64_t waited = now - s_.retireSinceUs[slot];
            highWater<uint32_t>(s_.nodes[slot].retireMaxUs,
                                waited > UINT32_MAX ? uint32_t(UINT32_MAX) : uint32_t(waited));
        }
    if (!s_.opActive)
        return;
    const RequestId pending = app.pendingRequest();
    if (pending.owner) {
        const Completion* done = firmware_.owner().result(pending);
        if (done) {
            recordStep(*done, now);
            if (!app.advanceRead(now)) {
                anomaly(KIND_INTERNAL, s_.opSlot + 1, uint32_t(done->service.transfer.operation), 0, now);
                s_.opActive = false; // The terminal stays retained; nothing replays it.
                s_.end = SoakEnd::INTERNAL;
                s_.phase = PHASE_FINISHING;
                s_.finishUs = now;
                return;
            }
        }
    } else if (!firmware_.running() && app.readContext().state == M::ReadState::ACTIVE) {
        app.cancelRead(now); // A stopped firmware will never advance it; this is local only.
    }
    if (app.readContext().state != M::ReadState::ACTIVE)
        finishOperation(now);
}

void Soak::startOperation(unsigned slot, uint64_t now) noexcept {
    SoakNodeStats& stats = s_.nodes[slot];
    const C::SdoChannel* channel = services_.channel(uint8_t(slot));
    const C::NodeSnapshot* node = services_.node(uint8_t(slot));
    if (stopIfUnusable(slot, now) || !channel || !node)
        return;
    C::ServiceEligibility eligibility;
    if (firmware_.owner().resources().inhibited || !firmware_.running() ||
        !C::serviceEligibility(*node, now, eligibility) || !eligibility.probeUpload) {
        add(s_.pressure.deferInhibited);
        return;
    }
    if (!firmware_.owner().localTxRetired(uint8_t(slot))) {
        add(s_.pressure.deferTx);
        return;
    }
    L::ReadTarget target;
    target.channel = *channel;
    target.nodeGeneration = node->route.generation;
    ProbeApp& app = firmware_.app();
    L::ReadOptions options;
    options.queueTimeoutUs = QUEUE_TIMEOUT_US;
    options.responseTimeoutUs = RESPONSE_TIMEOUT_US;
    const uint64_t deadline = now + OPERATION_DEADLINE_US;
    bool prepared = app.select(target);
    if (prepared) {
        if (s_.plan.identity && !s_.identityDone[slot]) {
            // Attempted once per soak, whatever its result: a failure is retained, not retried.
            s_.identityDone[slot] = true;
            options.identityWidths = L::IdentityWidths::MEASURED_U8;
            prepared = app.requestIdentity(now, deadline, options);
        } else if (s_.plan.monitors && s_.monitorPhase[slot]) {
            s_.monitorPhase[slot] = 0;
            prepared = app.requestState(now, deadline, options,
                                        uint8_t(1u << unsigned(L::StateBlock::MONITORS)));
        } else {
            s_.monitorPhase[slot] = 1;
            prepared = app.requestState(now, deadline, options,
                                        uint8_t(1u << unsigned(L::StateBlock::STATUS) |
                                                1u << unsigned(L::StateBlock::FEEDBACK)));
        }
    }
    if (!prepared) {
        add(stats.refused);
        anomaly(KIND_ADMISSION, slot + 1, node->route.generation, 0, now);
        return;
    }
    add(stats.operations);
    s_.operation = app.readContext().operation;
    s_.opActive = true;
    s_.opSlot = uint8_t(slot);
}

void Soak::schedule(uint64_t now) noexcept {
    if (s_.phase != PHASE_RUNNING)
        return;
    if (now >= s_.endUs || s_.stopRequested) {
        s_.end = s_.stopRequested ? SoakEnd::STOPPED : SoakEnd::DURATION;
        s_.phase = PHASE_FINISHING;
        s_.finishUs = now;
        return;
    }
    if (firmware_.progressionFailed()) {
        s_.end = SoakEnd::INTERNAL;
        s_.phase = PHASE_FINISHING;
        s_.finishUs = now;
        return;
    }
    if (now < s_.nextOpUs)
        return;
    if (s_.dump != DUMP_NONE) {
        add(s_.pressure.deferDump);
        return;
    }
    if (s_.opActive || s_.releasePending) {
        add(s_.pressure.deferBusy);
        return;
    }
    // A stopped node keeps its tick, so the other node's cadence does not change.
    const unsigned slot = s_.turn;
    s_.turn = uint8_t((slot + 1) % SOAK_NODES);
    s_.nextOpUs += uint64_t(s_.plan.intervalMs) * 1000u;
    if (s_.nextOpUs <= now) {
        add(s_.pressure.slips);
        s_.nextOpUs = now + uint64_t(s_.plan.intervalMs) * 1000u; // No catch-up burst.
    }
    startOperation(slot, now);
}

void Soak::abandonDump(uint64_t now) noexcept {
    if (s_.dump == DUMP_NONE || now - s_.dumpStartUs < DUMP_DEADLINE_US)
        return;
    // A stalled console must not hold reads all night. Nothing in flight is touched.
    const uint32_t last = s_.dump == DUMP_EMIT ? s_.dumpLast : s_.traceNext;
    const uint32_t unprinted = last > s_.dumpFirst ? last - s_.dumpFirst : 0;
    add(s_.pressure.dumpsAbandoned);
    add(s_.pressure.traceLost, unprinted);
    s_.pressure.dumpUs += now - s_.dumpStartUs;
    s_.dump = DUMP_NONE;
    push(EVENT_DUMP_ABANDONED, 0, 0, true, s_.dumpId, uint32_t(now - s_.dumpStartUs), unprinted, 0, now,
         false);
}

void Soak::finishPhases(uint64_t now) noexcept {
    if (s_.phase == PHASE_FINISHING) {
        // A started operation settles; none is cancelled for the sake of stopping or the log.
        const bool timedOut = now - s_.finishUs >= FINISH_TIMEOUT_US;
        if ((s_.opActive || s_.releasePending) && !timedOut)
            return;
        s_.summaryPending = true;
        s_.summaryCursor = 0;
        s_.phase = PHASE_FINAL;
        s_.finishUs = now;
        return;
    }
    if (s_.phase == PHASE_FINAL && !s_.endPending) {
        const bool flushed = !s_.summaryPending && !s_.eventCount && s_.dump == DUMP_NONE && !s_.lineReady;
        if (flushed || now - s_.finishUs >= FINAL_TIMEOUT_US)
            s_.endPending = true;
    }
    // A console that stays stalled cannot keep the bench refusing commands. Whatever is
    // waiting, SOAK_END last, still prints if the console returns.
    if (s_.phase == PHASE_FINAL && now - s_.finishUs >= FINAL_TIMEOUT_US * 2) {
        s_.endPending = true;
        s_.phase = PHASE_IDLE;
    }
}

bool Soak::put(const char* pattern, ...) noexcept {
    va_list arguments;
    va_start(arguments, pattern);
    const int count = std::vsnprintf(text_, sizeof text_, pattern, arguments);
    va_end(arguments);
    if (count < 0 || size_t(count) >= sizeof text_) {
        // A truncated line is failed evidence, never a plausible shorter observation.
        static const char failure[] = "OUTPUT_FORMAT_ERROR; evidence incomplete\n";
        std::memcpy(text_, failure, sizeof failure);
        s_.lineLength = sizeof failure - 1;
    } else
        s_.lineLength = uint16_t(count);
    s_.lineReady = true;
    return true;
}

bool Soak::formatEvent(const Event& e) noexcept {
    const unsigned long long t = static_cast<unsigned long long>(e.timeUs);
    const unsigned long n = static_cast<unsigned long>(s_.line);
    switch (e.type) {
    case EVENT_TRANSITION:
        return put("SOAK_TR n=%lu t=%llu node=%u sig=%s from=%lx to=%lx first=%u ne=%lu ce=%lu\n", n, t,
                   unsigned(e.node), SIGNALS[e.signal].name, static_cast<unsigned long>(e.a),
                   static_cast<unsigned long>(e.b), unsigned(e.flag), static_cast<unsigned long>(e.c),
                   static_cast<unsigned long>(e.d));
    case EVENT_ANOMALY:
        return put("SOAK_ANOM n=%lu t=%llu id=%lu kind=%s node=%u dump=%u a=%lx b=%lx\n", n, t,
                   static_cast<unsigned long>(e.a), KIND_NAMES[e.signal], unsigned(e.node),
                   unsigned(e.flag), static_cast<unsigned long>(e.b), static_cast<unsigned long>(e.c));
    case EVENT_OPERATION:
        if (e.flag)
            return put("SOAK_OP n=%lu t=%llu node=%u op=%lu local=1 kind=%u cursor=%u read=%u detail=%u\n", n,
                       t, unsigned(e.node), static_cast<unsigned long>(e.a), unsigned(e.c >> 8),
                       unsigned(e.c & 0xff), unsigned(e.d >> 16), unsigned(e.d & 0xffff));
        return put("SOAK_OP n=%lu t=%llu node=%u op=%lu local=0 step=%u object=%04x:%02x outcome=%u "
                   "reason=%u stages=%02x abort=%08lx\n",
                   n, t, unsigned(e.node), static_cast<unsigned long>(e.a), unsigned(e.c & 0xff),
                   unsigned(e.c >> 16), unsigned((e.c >> 8) & 0xff), unsigned(e.signal),
                   unsigned(e.d >> 8), unsigned(e.d & 0xff), static_cast<unsigned long>(e.b));
    case EVENT_EMCY:
    case EVENT_BOOT:
        return put("%s n=%lu t=%llu node=%u dlc=%lu data=%08lx%08lx disposition=%lu\n",
                   e.type == EVENT_EMCY ? "SOAK_EMCY" : "SOAK_BOOT", n, t, unsigned(e.node),
                   static_cast<unsigned long>(e.d),
                   // Wire byte order, as the other frame records print it.
                   static_cast<unsigned long>((e.a & 0xffu) << 24 | (e.a & 0xff00u) << 8 |
                                              (e.a >> 8 & 0xff00u) | e.a >> 24),
                   static_cast<unsigned long>((e.b & 0xffu) << 24 | (e.b & 0xff00u) << 8 |
                                              (e.b >> 8 & 0xff00u) | e.b >> 24),
                   static_cast<unsigned long>(e.c));
    case EVENT_DUMP_ABANDONED:
        return put("SOAK_TEND n=%lu t=%llu a=%lu abandoned=1 held=%lu unprinted=%lu\n", n, t,
                   static_cast<unsigned long>(e.a), static_cast<unsigned long>(e.b),
                   static_cast<unsigned long>(e.c));
    case EVENT_NODE_STOP:
        return put("SOAK_NODE_STOP n=%lu t=%llu node=%u reason=%s lastOp=%lu\n", n, t, unsigned(e.node),
                   e.signal ? "exhausted" : "unresolved", static_cast<unsigned long>(e.a));
    default:
        return false;
    }
}

bool Soak::formatDump(uint64_t now) noexcept {
    if (s_.dump == DUMP_COLLECT) {
        if (now < s_.dumpPostUs)
            return false;
        s_.dump = DUMP_EMIT;
        // Fixed end, so that traffic arriving during output does not extend this dump.
        s_.dumpLast = s_.traceNext;
        if (s_.dumpLast - s_.dumpFirst > SOAK_DUMP_ENTRIES)
            s_.dumpFirst = s_.dumpLast - SOAK_DUMP_ENTRIES;
    }
    if (s_.dump != DUMP_EMIT)
        return false;
    if (s_.dumpFirst >= s_.dumpLast) {
        s_.pressure.dumpUs += now - s_.dumpStartUs;
        s_.dump = DUMP_NONE;
        return put("SOAK_TEND n=%lu t=%llu a=%lu abandoned=0 held=%llu unprinted=0\n",
                   static_cast<unsigned long>(s_.line), static_cast<unsigned long long>(now),
                   static_cast<unsigned long>(s_.dumpId),
                   static_cast<unsigned long long>(now - s_.dumpStartUs));
    }
    // Asynchronous frames may overwrite the oldest entries during output: count them.
    if (s_.traceNext - s_.dumpFirst > SOAK_TRACE) {
        const uint32_t oldest = s_.traceNext - SOAK_TRACE;
        add(s_.pressure.traceLost, oldest - s_.dumpFirst);
        s_.dumpFirst = oldest;
    }
    const uint32_t sequence = s_.dumpFirst++;
    const TraceEntry& e = s_.trace[sequence % SOAK_TRACE];
    return put("SOAK_T n=%lu a=%lu i=%lu k=%u p=%llu id=%lx ext=%u rtr=%u dlc=%u "
               "data=%02x%02x%02x%02x%02x%02x%02x%02x ok=%u ex=%u bounds=%u dlo=%u dhi=%u ce=%u st=%u "
               "tr=%u tf=%u\n",
               static_cast<unsigned long>(s_.line), static_cast<unsigned long>(s_.dumpId),
               static_cast<unsigned long>(sequence), unsigned(e.kind),
               static_cast<unsigned long long>(e.processedUs), static_cast<unsigned long>(e.id),
               unsigned((e.flags & FLAG_EXTENDED) != 0), unsigned((e.flags & FLAG_RTR) != 0),
               unsigned(e.dlc), unsigned(e.data[0]), unsigned(e.data[1]), unsigned(e.data[2]),
               unsigned(e.data[3]), unsigned(e.data[4]), unsigned(e.data[5]), unsigned(e.data[6]),
               unsigned(e.data[7]), unsigned((e.flags & FLAG_OK) != 0),
               unsigned((e.flags & FLAG_EXACT) != 0), unsigned((e.flags & FLAG_BOUNDS) != 0),
               unsigned(e.lowOffset), unsigned(e.highOffset), unsigned(e.epoch), unsigned(e.state),
               unsigned(e.txRequest), unsigned(e.txFrame));
}

bool Soak::formatSummary(uint64_t now) noexcept {
    if (!s_.summaryPending)
        return false;
    const unsigned long n = static_cast<unsigned long>(s_.line);
    const unsigned long long t = static_cast<unsigned long long>(now);
    const Stats& stats = firmware_.owner().stats();
    const SoakPressure& p = s_.pressure;
    const unsigned cursor = s_.summaryCursor++;
    if (cursor == 0) {
        const TwaiDiagnostics twai = firmware_.transport().diagnostics();
        if (callbacks_.sample) {
            s_.resources = SoakResources();
            callbacks_.sample(callbacks_.context, s_.resources);
        }
        return put("SOAK_SUM n=%lu t=%llu trial=%lu up=%llu rx=%lu tx=%lu txRej=%lu rxDrop=%lu rxLost=%lu "
                   "unmatched=%lu qRej=%lu resFull=%lu timeouts=%lu cancelled=%lu busOff=%lu recov=%lu "
                   "twaiErr=%lu twaiFlags=%lx twaiDrop=%lu\n",
                   n, t, static_cast<unsigned long>(s_.plan.trial),
                   static_cast<unsigned long long>(now - s_.startUs),
                   static_cast<unsigned long>(stats.rxFrames), static_cast<unsigned long>(stats.txFrames),
                   static_cast<unsigned long>(stats.txRejected),
                   static_cast<unsigned long>(stats.rxDropped), static_cast<unsigned long>(stats.rxLost),
                   static_cast<unsigned long>(stats.unmatchedFrames),
                   static_cast<unsigned long>(stats.queueRejected),
                   static_cast<unsigned long>(stats.resultsRejected),
                   static_cast<unsigned long>(stats.timeouts),
                   static_cast<unsigned long>(stats.cancelled),
                   static_cast<unsigned long>(stats.busOffEvents),
                   static_cast<unsigned long>(stats.recoveries),
                   static_cast<unsigned long>(twai.busErrors),
                   static_cast<unsigned long>(twai.errorFlags),
                   static_cast<unsigned long>(twai.rxDropped));
    }
    if (cursor == 1) {
        put("SOAK_GAP n=%lu t=%llu gapMax=%llu gapWin=%llu ownerGap=%llu qHw=%u actHw=%u retHw=%u trHw=%u "
            "trOver=%lu netDrop=%lu stack=%lu heap=%lu heapMin=%lu tec=%lu rec=%lu\n",
            n, t, static_cast<unsigned long long>(p.gapMaxUs),
            static_cast<unsigned long long>(p.gapWindowUs),
            static_cast<unsigned long long>(stats.maxServiceGapUs), unsigned(p.queuedHighWater),
            unsigned(p.activeHighWater), unsigned(p.retainedHighWater), unsigned(p.traceHighWater),
            static_cast<unsigned long>(stats.traceOverwritten),
            static_cast<unsigned long>(services_.droppedNetworkObservations()),
            static_cast<unsigned long>(s_.resources.stackFree),
            static_cast<unsigned long>(s_.resources.heapFree),
            static_cast<unsigned long>(s_.resources.heapMinimum),
            static_cast<unsigned long>(s_.resources.txErrors),
            static_cast<unsigned long>(s_.resources.rxErrors));
        s_.pressure.gapWindowUs = 0;
        return true;
    }
    if (cursor == 2) {
        const ProbeIoStats io = firmware_.platform().stats();
        return put("SOAK_CON n=%lu t=%llu lines=%lu blocked=%lu conRej=%lu conHw=%u evFull=%lu evLimit=%lu "
                   "evHw=%u anomalies=%lu dumps=%lu dumpSkip=%lu dumpAb=%lu trLost=%lu dumpUs=%llu "
                   "defDump=%lu defBusy=%lu defTx=%lu defInh=%lu slips=%lu sdkDefer=%lu uartBytes=%lu "
                   "queued=%u\n",
                   n, t, static_cast<unsigned long>(p.lines), static_cast<unsigned long>(p.blockedTurns),
                   static_cast<unsigned long>(io.outputRejected), unsigned(p.consoleHighWater),
                   static_cast<unsigned long>(p.eventsFull), static_cast<unsigned long>(p.eventsLimited),
                   unsigned(p.eventHighWater), static_cast<unsigned long>(p.anomalies),
                   static_cast<unsigned long>(p.dumps), static_cast<unsigned long>(p.dumpsSkipped),
                   static_cast<unsigned long>(p.dumpsAbandoned), static_cast<unsigned long>(p.traceLost),
                   static_cast<unsigned long long>(p.dumpUs), static_cast<unsigned long>(p.deferDump),
                   static_cast<unsigned long>(p.deferBusy), static_cast<unsigned long>(p.deferTx),
                   static_cast<unsigned long>(p.deferInhibited), static_cast<unsigned long>(p.slips),
                   static_cast<unsigned long>(tap_.deferred()), static_cast<unsigned long>(io.outputWritten),
                   unsigned(io.outputUsed));
    }
    const unsigned part = (cursor - 3) % 4, slot = (cursor - 3) / 4;
    const SoakNodeStats& node = s_.nodes[slot];
    if (slot == SOAK_NODES - 1 && part == 3)
        s_.summaryPending = false;
    const unsigned scope = slot + 1;
    if (part == 0)
        return put("SOAK_NODE n=%lu t=%llu node=%u ops=%lu opsOk=%lu opsFail=%lu refused=%lu steps=%lu "
                   "stepsOk=%lu min=%lu mean=%lu max=%lu retireMax=%lu earlyReply=%lu hb=%lu hbGapMax=%lu stopped=%u\n",
                   n, t, scope, static_cast<unsigned long>(node.operations),
                   static_cast<unsigned long>(node.operationsOk),
                   static_cast<unsigned long>(node.operationsFailed),
                   static_cast<unsigned long>(node.refused), static_cast<unsigned long>(node.steps),
                   static_cast<unsigned long>(node.stepsOk),
                   static_cast<unsigned long>(node.latencyCount ? node.latencyMinUs : 0),
                   static_cast<unsigned long>(node.latencyCount ? node.latencySumUs / node.latencyCount : 0),
                   static_cast<unsigned long>(node.latencyMaxUs),
                   static_cast<unsigned long>(node.retireMaxUs),
                   static_cast<unsigned long>(node.earlyReplies),
                   static_cast<unsigned long>(node.heartbeats),
                   static_cast<unsigned long>(node.heartbeatGapMaxUs), unsigned(node.stopped));
    if (part == 1)
        return put("SOAK_HIST n=%lu t=%llu node=%u b=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", n, t, scope,
                   static_cast<unsigned long>(node.buckets[0]), static_cast<unsigned long>(node.buckets[1]),
                   static_cast<unsigned long>(node.buckets[2]), static_cast<unsigned long>(node.buckets[3]),
                   static_cast<unsigned long>(node.buckets[4]), static_cast<unsigned long>(node.buckets[5]),
                   static_cast<unsigned long>(node.buckets[6]), static_cast<unsigned long>(node.buckets[7]));
    if (part == 2) {
        const uint32_t* o = node.outcomes;
        const uint32_t* r = node.reasons;
        const uint32_t* a = node.readOutcomes;
        return put("SOAK_OUT n=%lu t=%llu node=%u sdo=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu "
                   "reason=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu read=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                   n, t, scope, static_cast<unsigned long>(o[0]), static_cast<unsigned long>(o[1]),
                   static_cast<unsigned long>(o[2]), static_cast<unsigned long>(o[3]),
                   static_cast<unsigned long>(o[4]), static_cast<unsigned long>(o[5]),
                   static_cast<unsigned long>(o[6]), static_cast<unsigned long>(o[7]),
                   static_cast<unsigned long>(o[8]), static_cast<unsigned long>(o[9]),
                   static_cast<unsigned long>(o[10]), static_cast<unsigned long>(o[11]),
                   static_cast<unsigned long>(r[0]), static_cast<unsigned long>(r[1]),
                   static_cast<unsigned long>(r[2]), static_cast<unsigned long>(r[3]),
                   static_cast<unsigned long>(r[4]), static_cast<unsigned long>(r[5]),
                   static_cast<unsigned long>(r[6]), static_cast<unsigned long>(r[7]),
                   static_cast<unsigned long>(a[0]), static_cast<unsigned long>(a[1]),
                   static_cast<unsigned long>(a[2]), static_cast<unsigned long>(a[3]),
                   static_cast<unsigned long>(a[4]), static_cast<unsigned long>(a[5]),
                   static_cast<unsigned long>(a[6]), static_cast<unsigned long>(a[7]),
                   static_cast<unsigned long>(a[8]));
    }
    return put("SOAK_ABORT n=%lu t=%llu node=%u a0=%08lx:%lu a1=%08lx:%lu a2=%08lx:%lu a3=%08lx:%lu other=%lu\n",
               n, t, scope, static_cast<unsigned long>(node.abortCode[0]),
               static_cast<unsigned long>(node.abortCount[0]),
               static_cast<unsigned long>(node.abortCode[1]),
               static_cast<unsigned long>(node.abortCount[1]),
               static_cast<unsigned long>(node.abortCode[2]),
               static_cast<unsigned long>(node.abortCount[2]),
               static_cast<unsigned long>(node.abortCode[3]),
               static_cast<unsigned long>(node.abortCount[3]),
               static_cast<unsigned long>(node.abortOther));
}

bool Soak::formatNext(uint64_t now) noexcept {
    const unsigned long n = static_cast<unsigned long>(s_.line);
    const unsigned long long t = static_cast<unsigned long long>(now);
    if (s_.beginPending) {
        s_.beginPending = false;
        return put("SOAK_BEGIN n=%lu t=%llu trial=%lu dur=%lu interval=%lu summary=%lu monitors=%u identity=%u "
                   "opDeadline=%lu queue=%lu resp=%lu stack=%lu heap=%lu heapMin=%lu base=%s\n",
                   n, t, static_cast<unsigned long>(s_.plan.trial),
                   static_cast<unsigned long>(s_.plan.durationS),
                   static_cast<unsigned long>(s_.plan.intervalMs),
                   static_cast<unsigned long>(s_.plan.summaryS), unsigned(s_.plan.monitors),
                   unsigned(s_.plan.identity), static_cast<unsigned long>(OPERATION_DEADLINE_US),
                   static_cast<unsigned long>(QUEUE_TIMEOUT_US),
                   static_cast<unsigned long>(RESPONSE_TIMEOUT_US),
                   static_cast<unsigned long>(s_.resources.stackFree),
                   static_cast<unsigned long>(s_.resources.heapFree),
                   static_cast<unsigned long>(s_.resources.heapMinimum), revision_);
    }
    if (s_.eventCount) {
        const Event event = s_.events[s_.eventHead];
        s_.eventHead = uint8_t((s_.eventHead + 1) % SOAK_EVENTS);
        --s_.eventCount;
        return formatEvent(event);
    }
    if (formatDump(now))
        return true;
    if (formatSummary(now))
        return true;
    if (s_.endPending) {
        s_.endPending = false;
        s_.phase = PHASE_IDLE;
        const char* const reasons[] = {"none", "duration", "stopped", "internal"};
        return put("SOAK_END n=%lu t=%llu trial=%lu reason=%s up=%llu lines=%lu settled=%u\n", n, t,
                   static_cast<unsigned long>(s_.plan.trial), reasons[unsigned(s_.end)],
                   static_cast<unsigned long long>(now - s_.startUs),
                   static_cast<unsigned long>(s_.pressure.lines),
                   unsigned(!s_.opActive && !s_.releasePending));
    }
    return false;
}

void Soak::pump(uint64_t now) noexcept {
    static_assert(unsigned(LINE_BYTES) <= unsigned(ProbePlatform::OUTPUT_CAPACITY),
                  "a soak line must fit the console whole");
    ProbePlatform& console = firmware_.platform();
    const ProbeIoStats io = console.stats();
    highWater(s_.pressure.consoleHighWater, io.outputUsed);
    if (!s_.lineReady && !formatNext(now))
        return;
    if (s_.lineLength > size_t(ProbePlatform::OUTPUT_CAPACITY) - io.outputUsed) {
        add(s_.pressure.blockedTurns);
        return;
    }
    // Room was measured, so this cannot refuse and adds nothing to the console's refused count.
    if (console.writeOutput(reinterpret_cast<const uint8_t*>(text_), s_.lineLength)) {
        s_.lineReady = false;
        if (s_.line != UINT32_MAX)
            ++s_.line;
        add(s_.pressure.lines);
    }
}

void Soak::service(uint64_t now, bool holdOutput) noexcept {
    if (s_.phase == PHASE_IDLE) {
        // SOAK_END may still be waiting for room: it is the last line of the record.
        if (!holdOutput && (s_.lineReady || s_.endPending))
            pump(now);
        return;
    }
    if (now < s_.lastServiceUs || now < firmware_.owner().lastTimeUs())
        return;
    const uint64_t gap = now - s_.lastServiceUs;
    highWater(s_.pressure.gapMaxUs, gap);
    highWater(s_.pressure.gapWindowUs, gap);
    s_.lastServiceUs = now;
    copyDiagnostics(now);
    copySubmission(now);
    copyObservations(now);
    abandonDump(now);
    trackInterface(now);
    trackNodes(now);
    advance(now);
    schedule(now);
    if (s_.phase == PHASE_RUNNING && now >= s_.nextSummaryUs) {
        s_.nextSummaryUs = now + uint64_t(s_.plan.summaryS) * 1000000u;
        std::memset(s_.raised, 0, sizeof s_.raised);
        // A summary still printing is not restarted; its next one is simply later.
        if (!s_.summaryPending) {
            s_.summaryPending = true;
            s_.summaryCursor = 0;
        }
    }
    finishPhases(now);
    if (!holdOutput)
        pump(now);
}
} // namespace MotorControlCANExample
