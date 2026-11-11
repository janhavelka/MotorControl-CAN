// SPDX-License-Identifier: MIT
#include "BenchConsole.h"
#include "ProbeConsoleText.h"
#include <MotorControlCAN/profiles/lichuan_c/Velocity.h>
#include <cstdio>
#include <cstring>
#include <new>

// Helpers whose locals must not join take()'s frame, which sits under the console's deepest dispatch
// chain (scripts/check_stack.py): they return before take() feeds the console.
#if defined(__GNUC__) || defined(__clang__)
#define BENCH_CONSOLE_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define BENCH_CONSOLE_NOINLINE __declspec(noinline)
#else
#define BENCH_CONSOLE_NOINLINE
#endif

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
bool is(const char* a, const char* b) {
    return !std::strcmp(a, b);
}
bool oneOf(const char* name, const char* const* names, size_t count) {
    for (size_t n = 0; n < count; ++n)
        if (is(name, names[n]))
            return true;
    return false;
}
bool sameTarget(const L::ReadTarget& a, const L::ReadTarget& b) {
    return a.nodeGeneration == b.nodeGeneration && M::sameBinding(a.channel.binding, b.channel.binding) &&
           a.channel.clientToServerId == b.channel.clientToServerId &&
           a.channel.serverToClientId == b.channel.serverToClientId;
}
ProbeCallbacks consoleCallbacks(void* self, bool (*clock)(void*, uint64_t&), bool (*write)(void*, uint8_t)) {
    ProbeCallbacks callbacks;
    callbacks.context = self;
    callbacks.nowUs = clock;
    callbacks.writeByte = write; // No readByte: complete lines are fed directly, after the gate.
    return callbacks;
}
// The console routes that admit a motion operation, as the route kind whose caps count them. Home has
// no motion weight on the bench and no console prerequisite here: the console refuses it itself.
RouteKind admissionKind(const ConsoleLine& line) {
    const char* name = line.token(0);
    if (is(name, "moveby") || is(name, "moveto") || is(name, "move"))
        return RouteKind::MOVE;
    if (is(name, "velocity") && !is(line.token(1), "read"))
        return RouteKind::VELOCITY;
    if (is(name, "enable"))
        return RouteKind::ENABLE;
    if (is(name, "alarm-clear"))
        return RouteKind::CLEAR_ALARM;
    if (is(name, "position-clear"))
        return RouteKind::COUNTER_ZERO;
    return RouteKind::NONE;
}
// The ids that show what a forwarded line admitted: the ordinary result and each stop's.
struct Admitted {
    uint32_t normal = 0, halt = 0, fast = 0;
};
Admitted admitted(const BenchAxis* axis) {
    Admitted ids;
    if (!axis)
        return ids;
    const ProbeMotion& m = axis->motion;
    ids.normal = m.retained() ? m.normalResult().id : 0;
    ids.halt = m.stopRetained(L::StopKind::HALT) ? m.stopResult(L::StopKind::HALT)->result.id : 0;
    ids.fast = m.stopRetained(L::StopKind::QUICK_STOP) ? m.stopResult(L::StopKind::QUICK_STOP)->result.id : 0;
    return ids;
}
} // namespace

ConsoleGate benchConsoleGate(const ConsoleLine& line) noexcept {
    const char* name = line.token(0);
    const char* first = line.token(1);
    if (is(name, "recover"))
        return ConsoleGate::NEVER;
    if (is(name, "discover"))
        return is(first, "inspect") || is(first, "cancel") || is(first, "release") ? ConsoleGate::ALWAYS
                                                                                   : ConsoleGate::IDLE_LISTING;
    // Every route that admits owner work, changes the shared selection, the owner's counters or a
    // binding's configuration epoch, or drains the owner's diagnostic copies.
    static const char* const work[] = {"host",        "useaddr",       "probe",          "read",
                                       "nmt",         "capture-read",  "communication",  "persistence",
                                       "driver",      "io",            "control",        "tuning",
                                       "velocity",    "move",          "moveby",         "moveto",
                                       "enable",      "stop",          "alarm-clear",    "position-clear",
                                       "stepsperturn"};
    if (oneOf(name, work, sizeof work / sizeof work[0]))
        return ConsoleGate::IDLE_BENCH;
    if ((is(name, "home") && !is(first, "methods")) || (is(name, "monitor") && !is(first, "off")) ||
        (is(name, "pdo") && is(first, "apply")) || (is(name, "axis") && is(first, "origin")) ||
        (is(name, "stats") && line.count > 1) || (is(name, "debug") && line.count > 1 && !is(first, "off")) ||
        (is(name, "motion-profile") && (is(first, "read") || is(first, "restore"))))
        return ConsoleGate::IDLE_BENCH;
    return ConsoleGate::ALWAYS;
}

BenchConsole::BenchConsole(FirmwareProbe& firmware, MotorControlCAN::CANOPEN::SdoClient& services,
                           const BenchConsoleStorage& storage) noexcept
    : firmware_(firmware), platform_(consoleCallbacks(this, clock, write)),
      device_(*new (storage.device) ProbeDevice(firmware.app(), firmware.owner(), services, &firmware.evidence())),
      console_(*new (storage.console) ProbeConsole(firmware.app(), firmware.owner(), platform_)) {
    console_.attachDevice(device_);
    policy();
}
bool BenchConsole::policy() noexcept {
    if (policySet_)
        return true;
    // Both specimens answer 1009 and 100A in one byte (packet 4; sdkconfig.optak_s2_smoke and
    // FirmwareSmoke read them so). ProbeDevice scopes the width to identity reads; nothing detects
    // it. Budgets: the bench routes' own queue and response budgets, host policy. The device refuses
    // a change while any read is retained, so a bench record can postpone this until it is released.
    L::ReadOptions options;
    options.queueTimeoutUs = ROUTE_QUEUE_US;
    options.responseTimeoutUs = ROUTE_RESPONSE_US;
    options.identityWidths = L::IdentityWidths::MEASURED_U8;
    policySet_ = device_.setReadOptions(options);
    return policySet_;
}
bool BenchConsole::clock(void* context, uint64_t& output) {
    return static_cast<BenchConsole*>(context)->firmware_.platform().nowUs(output);
}
// The console platform's byte sink. Bytes collect until the record's newline; the newline is
// accepted only once the whole record is in the UART queue, so a held or full queue keeps it, and
// the console's delivery fence, unpassed.
bool BenchConsole::write(void* context, uint8_t byte) {
    BenchConsole& self = *static_cast<BenchConsole*>(context);
    if (self.stagedSize_ + 1u >= sizeof self.staged_)
        return false; // No console record is this long; refusing keeps the queue intact.
    if (byte != '\n') {
        self.staged_[self.stagedSize_++] = byte;
        return true;
    }
    self.staged_[self.stagedSize_] = '\n';
    if (self.hold_ || !self.firmware_.platform().writeOutput(self.staged_, self.stagedSize_ + 1u))
        return false;
    self.stagedSize_ = 0;
    return true;
}
bool BenchConsole::open(uint32_t bitrate) noexcept {
    HostInterfaceFacts facts;
    facts.interfaceKnown = true;
    facts.interfaceId = 0; // The image's one local physical interface.
    facts.bitrateKnown = true;
    facts.bitrateBitsPerSecond = bitrate;
    return bitrate != 0 && firmware_.app().setHostInterfaceFacts(facts);
}
bool BenchConsole::attachRoute(BenchRoute& route) noexcept {
    if (route_ && route_ != &route)
        return false;
    route_ = &route;
    return true;
}
bool BenchConsole::attachAxis(unsigned node, BenchAxis& axis) noexcept {
    if (node < 1 || node > 2 || axes_[node - 1] || !axis.motion.matches(firmware_.app(), firmware_.owner()))
        return false;
    axes_[node - 1] = &axis;
    return true;
}
void BenchConsole::remember() noexcept {
    const L::ReadTarget* current = firmware_.app().selected();
    selected_ = current != nullptr;
    if (current)
        selection_ = *current;
}
bool BenchConsole::sameSelection() const noexcept {
    const L::ReadTarget* current = firmware_.app().selected();
    return current ? selected_ && sameTarget(*current, selection_) : !selected_;
}
void BenchConsole::refuse(const ConsoleLine& line, const char* reason) noexcept {
    // The console's own refusal form (ProbeConsole::error), so a JSONL client correlates it by id.
    ConsoleText text;
    text.start(true, line.id, "reply", line.token(0));
    text.add(",\"ok\":false,\"status\":\"%s\",\"detail\":0,\"error\":", M::errToString(M::Err::INVALID_CONFIG));
    text.string(reason);
    text.end(true);
    static const char fallback[] = "CONSOLE_REFUSED reason=bench; no work\n";
    if (text.valid && text.used < sizeof reply_)
        std::memcpy(reply_, text.bytes, text.used + 1);
    else
        std::memcpy(reply_, fallback, sizeof fallback);
}
// The applied arming's reads are still the route's, of the attached node and this kind.
bool BenchConsole::armingCurrent(RouteKind kind) noexcept {
    return route_ && route_->armed(arming_) && arming_.plan.trial == appliedTrial_ &&
           arming_.plan.node == attached_ && arming_.plan.kind == kind;
}
BENCH_CONSOLE_NOINLINE const char* BenchConsole::motionRefusal(const ConsoleLine& line) noexcept {
    const RouteKind kind = admissionKind(line);
    // Without an attachment the console refuses the line itself and nothing is admitted.
    if (kind == RouteKind::NONE || !attached_ || !route_)
        return nullptr;
    if (!route_->consoleMotionAllowed(attached_, kind))
        return "bench_motion_budget_spent";
    // The prerequisites name the route's own read storage: once a later route replaced those reads,
    // or the sessions closed, only a new arming may name them again.
    if ((kind == RouteKind::MOVE && !armingCurrent(RouteKind::ARM_MOVE)) ||
        (kind == RouteKind::VELOCITY && !armingCurrent(RouteKind::ARM_VELOCITY)))
        return "arming_reads_replaced_arm_again";
    return nullptr;
}
// The stop's state is the application's latest complete state read (AxisSession::requestStop): the
// session's last move or velocity cycle, else the arming's own read while it stands, else the session's
// empty read, which leaves only the policy's best-effort fallback. The policy's deadlines run from this
// request, as BenchRoute::startStop sets them.
BENCH_CONSOLE_NOINLINE void BenchConsole::nameStop(uint64_t now) noexcept {
    if (!attached_ || !stopNamed_ || now > UINT64_MAX - 55000000ull * 2)
        return;
    ProbeMotion& motion = axes_[attached_ - 1]->motion;
    const L::ReadContext* state = &motion.session().moveObservation();
    if (state->state != M::ReadState::SUCCEEDED && armingCurrent(applied_.kind))
        state = arming_.state;
    StopPolicy policy;
    policy.options.service.queueTimeoutUs = ROUTE_QUEUE_US;
    policy.options.service.responseTimeoutUs = ROUTE_RESPONSE_US;
    policy.options.observationIntervalUs = uint64_t(applied_.stopIntervalMs) * 1000;
    // Seven fields at the queue and response budgets, as BenchRoute::startStop allows.
    policy.options.maximumAgeUs = 7 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
    policy.deadlineUs = now + uint64_t(applied_.stopDeadlineMs) * 1000;
    policy.bestEffortDeadlineUs = applied_.bestEffortMs ? now + uint64_t(applied_.bestEffortMs) * 1000 : 0;
    motion.setStopPrerequisites(*state, policy);
}
ConsoleInput BenchConsole::take(uint8_t byte, uint64_t now, const BenchActivity& bench) noexcept {
    reply_[0] = 0;
    if (!collecting_) {
        collecting_ = true;
        overlong_ = false;
        used_ = 0;
        startedUs_ = now;
    }
    if (byte != '\r' && byte != '\n') {
        if (used_ + 1u < sizeof line_)
            line_[used_++] = char(byte);
        else
            overlong_ = true;
        return ConsoleInput::COLLECTING;
    }
    collecting_ = false;
    if (overlong_) {
        static const char refusal[] = "CONSOLE_REFUSED reason=length; no work\n";
        std::memcpy(reply_, refusal, sizeof refusal);
        return ConsoleInput::REFUSED;
    }
    // A member, not a local: its 312 bytes would sit under the console's deepest dispatch chain.
    ConsoleLine& parsed = parsed_;
    // A line that does not parse goes to the console as it is: the console refuses it with its own
    // record and admits nothing.
    ConsoleGate gate = ConsoleGate::ALWAYS;
    RouteKind kind = RouteKind::NONE;
    bool stop = false;
    if (parseConsoleLine(line_, used_, parsed)) {
        gate = benchConsoleGate(parsed);
        const char* reason = gate == ConsoleGate::NEVER ? "bench_image_has_no_host_recovery"
                             : gate != ConsoleGate::ALWAYS && bench.wire ? "bench_wire_work_active"
                             : gate == ConsoleGate::IDLE_LISTING && bench.listing
                                 ? "bench_network_listing_active"
                                 : nullptr;
        if (!reason && gate != ConsoleGate::ALWAYS && !is(parsed.token(0), "host") &&
            !is(parsed.token(0), "useaddr") && !sameSelection())
            reason = "selection_changed_outside_console_select_again";
        // The measured width is for identity reads only; nothing else, a stop least of all, waits for it.
        if (!reason && is(parsed.token(1), "identity") &&
            (is(parsed.token(0), "read") || is(parsed.token(0), "discover")) && !policy())
            reason = "read_width_policy_pending_release_bench_record";
        if (!reason)
            reason = motionRefusal(parsed);
        if (reason) {
            refuse(parsed, reason);
            return ConsoleInput::REFUSED;
        }
        kind = admissionKind(parsed);
        stop = is(parsed.token(0), "stop");
        if (stop)
            nameStop(now);
    }
    BenchAxis* const axis = attached_ ? axes_[attached_ - 1] : nullptr;
    const Admitted before = admitted(axis);
    for (uint16_t n = 0; n < used_; ++n)
        console_.feed(uint8_t(line_[n]), now);
    console_.feed('\n', now);
    // Each admission counts against the bench's per-boot caps as a route of its kind; a stop is
    // counted and was never refused for them.
    const Admitted after = admitted(axis);
    if (route_ && kind != RouteKind::NONE && after.normal && after.normal != before.normal)
        route_->countConsoleMotion(attached_, kind);
    if (route_ && stop && ((after.halt && after.halt != before.halt) || (after.fast && after.fast != before.fast)))
        route_->countConsoleMotion(attached_, RouteKind::STOP);
    // host, useaddr, stepsperturn and axis origin move the selection; every other gated line passed
    // with the remembered one. An inspection line never adopts a selection the bench made.
    if (gate != ConsoleGate::ALWAYS)
        remember();
    return ConsoleInput::FORWARDED;
}
bool BenchConsole::expire(uint64_t now) noexcept {
    if (!collecting_ || now < startedUs_ || now - startedUs_ <= COLLECT_TIMEOUT_US)
        return false;
    collecting_ = false;
    used_ = 0;
    return true;
}
// Name one node's prerequisites from its arming, exactly as BenchRoute::startMove or startVelocity builds
// them from a MOVE or VELOCITY line; nothing is acquired, defaulted or sent here. @return Refusal or null.
const char* BenchConsole::arm(BenchAxis& x, uint64_t now) noexcept {
    const RoutePlan& p = arming_.plan;
    const uint8_t slot = uint8_t(p.node - 1);
    // The console's motion routes act on one attachment; another node's is withdrawn only while idle.
    if (attached_ != p.node) {
        if (attached_ && !console_.detachMotion())
            return "console_motion_retained";
        attached_ = 0;
        if (!console_.attachMotion(x.motion))
            return "attach";
        attached_ = p.node;
    }
    const C::SdoChannel* channel = firmware_.services().channel(slot);
    const C::NodeSnapshot* node = firmware_.services().node(slot);
    const AxisSession& session = x.motion.session();
    if (!channel || !node || !session.isOpen() || !M::sameBinding(channel->binding, session.binding()))
        return "route";
    x.target.channel = *channel;
    x.target.nodeGeneration = node->route.generation;
    // A scale or origin set through ProbeMotion stays with its own binding epoch; a session at another
    // epoch starts from no host coordinates. Exact integer rpm needs no scale (60FF, 6081: A-01).
    if (!M::sameBinding(x.axis.binding, session.binding())) {
        x.axis = M::AxisConfig();
        x.axis.binding = session.binding();
    }
    x.axis.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    x.axis.nativeVelocityTime = M::TimeUnit::MINUTE;
    x.reference = M::AxisReference();
    M::Status status;
    if (p.kind == RouteKind::ARM_MOVE) {
        const bool relative = p.relativeBase != 0;
        const M::RelativeBasis basis = p.relativeBase == 2 ? M::RelativeBasis::COMMANDED : M::RelativeBasis::ACTUAL;
        x.axis.supportedRelativeBases = relative ? uint8_t(1u << unsigned(basis)) : uint8_t(0);
        L::MoveEvidence evidence;
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
        L::MoveOptions options;
        options.service.queueTimeoutUs = ROUTE_QUEUE_US;
        options.service.responseTimeoutUs = ROUTE_RESPONSE_US;
        options.observationIntervalUs = uint64_t(p.intervalMs) * 1000;
        options.maximumAgeUs = 9 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
        if (relative) {
            // As MOVE's relative witness: idle and stationary rest on the route's check of two fresh full
            // reads, observed at the second's earliest FEEDBACK bound and expiring with the move's age cap.
            // ACTUAL is that 6064; COMMANDED is the host's reviewed base, which no read here can show.
            M::AxisReference& w = x.reference;
            w.binding = x.axis.binding;
            w.referenceGeneration = x.axis.referenceGeneration;
            w.idle = w.stationary = w.nativeKnown = true;
            w.nativePosition = p.relativeBase == 1 ? arming_.position : p.base;
            w.basis = basis;
            w.source = p.relativeBase == 1 ? M::ScaleSource::READBACK : M::ScaleSource::ASSUMED;
            w.observedUs = arming_.feedbackUs;
            w.nowUs = now;
            w.maximumAgeUs = options.maximumAgeUs;
        }
        status = x.motion.setMovePrerequisites(*arming_.state, *arming_.identity, evidence, L::MoveProfile(),
                                               nullptr, options);
    } else if (p.kind == RouteKind::ARM_WITNESS) {
        // A witness only, for the console's stepsperturn and axis origin, which consume it: idle and
        // stationary rest on the route's check of two fresh full reads at the host's reviewed 6064, read
        // back, observed at the second's earliest FEEDBACK bound, with the move's age cap. No motion
        // prerequisite and no stop budget is named; earlier stop budgets stay.
        M::AxisReference& w = x.reference;
        w.binding = x.axis.binding;
        w.referenceGeneration = x.axis.referenceGeneration;
        w.idle = w.stationary = w.nativeKnown = true;
        w.nativePosition = arming_.position;
        w.basis = M::RelativeBasis::ACTUAL;
        w.source = M::ScaleSource::READBACK;
        w.observedUs = arming_.feedbackUs;
        w.nowUs = now;
        w.maximumAgeUs = 9 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
        return nullptr;
    } else {
        L::VelocityEvidence evidence;
        // The attended flag opens the library's experiment scope, as for VELOCITY; it claims nothing.
        evidence.gate = p.attended ? L::VelocityGate::ATTENDED_EXPERIMENT : L::VelocityGate::CLOSED;
        evidence.record = p.record;
        evidence.model = L::SettingsModel::CL86_C;
        evidence.firmware.deviceType = p.firmware.deviceType;
        evidence.firmware.hardwareVersion = p.firmware.hardwareVersion;
        evidence.firmware.softwareVersion = p.firmware.softwareVersion;
        evidence.firmware.vendorId = p.firmware.vendorId;
        evidence.firmware.productCode = p.firmware.productCode;
        evidence.firmware.revision = p.firmware.revision;
        evidence.firmware.modelCode = p.firmware.modelCode;
        evidence.firmware.firmwareCode = p.firmware.firmwareCode;
        evidence.layout = static_cast<L::StatusLayout>(p.layout);
        evidence.startQualified = evidence.stopQualified = false;
        evidence.stopKind = p.stopKind == 2 ? L::StopKind::QUICK_STOP : L::StopKind::HALT;
        evidence.modeChangeQualified = evidence.reverseQualified = false;
        evidence.modeChangeFromRaw = 0;
        evidence.maximumForwardRpm = p.rpm;
        evidence.maximumReverseRpm = 0;
        L::VelocityProfile profile;
        profile.rampUpTimeMs = p.rampUpMs;
        profile.rampDownTimeMs = p.rampDownMs;
        L::VelocityOptions options;
        options.service.queueTimeoutUs = ROUTE_QUEUE_US;
        options.service.responseTimeoutUs = ROUTE_RESPONSE_US;
        options.pollIntervalUs = uint64_t(p.intervalMs) * 1000;
        // The console's duration is its own; this bound covers the longest the experiment admits.
        options.maxPolls = uint32_t((L::VELOCITY_EXPERIMENT_MAXIMUM_DURATION_US + options.pollIntervalUs - 1) /
                                    options.pollIntervalUs);
        options.maximumAgeUs = 9 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
        options.requiredStopBudgetUs = 8 * (uint64_t(ROUTE_QUEUE_US) + ROUTE_RESPONSE_US);
        options.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
        options.stopKind = evidence.stopKind;
        options.enableWindowUs = uint64_t(ROUTE_VELOCITY_WINDOW_MS) * 1000;
        options.enableSpacingUs = uint64_t(ROUTE_VELOCITY_SPACING_MS) * 1000;
        status = x.motion.setVelocityPrerequisites(*arming_.state, *arming_.identity, evidence, profile, options);
    }
    if (!status)
        return status.msg && *status.msg ? status.msg : "prerequisites";
    applied_ = p;
    stopNamed_ = true;
    emcyBaseline_[slot] = node->emcyCount; // An EMCY before the arming is not this console's.
    nameStop(now);
    return nullptr;
}
BENCH_CONSOLE_NOINLINE void BenchConsole::applyArming(uint64_t now) noexcept {
    if (!route_ || !route_->armed(arming_) || arming_.plan.trial == appliedTrial_)
        return;
    appliedTrial_ = arming_.plan.trial; // Once per arming record, whatever the outcome.
    const unsigned node = arming_.plan.node;
    BenchAxis* axis = axes_[node - 1];
    const char* refusal = axis ? arm(*axis, now) : "memory";
    if (refusal) {
        // Earlier stop budgets stay named: a refused arming never makes a stop unreachable.
        std::snprintf(notice_, sizeof notice_, "CONSOLE arm refused node=%u trial=%lu reason=%.80s; nothing named\n",
                      node, static_cast<unsigned long>(arming_.plan.trial), refusal);
        return;
    }
    std::snprintf(notice_, sizeof notice_,
                  "CONSOLE armed node=%u trial=%lu route=%s relative=%u witness=%u scale=%u origin=%u; "
                  "prerequisites named, nothing sent\n",
                  node, static_cast<unsigned long>(arming_.plan.trial), routeName(arming_.plan.kind),
                  unsigned(arming_.plan.relativeBase), unsigned(axis->reference.nativeKnown),
                  unsigned(axis->axis.units.commandStepsPerMotorTurn.source != M::ScaleSource::UNKNOWN),
                  unsigned(axis->axis.originKnown));
}
void BenchConsole::service(uint64_t now, const BenchActivity& bench) noexcept {
    hold_ = bench.holdOutput;
    applyArming(now);
    console_.service(now);
    // In memory and bounded: at most one record's bytes reach the UART queue per turn.
    for (unsigned n = 0; n < DRAIN_TURNS && platform_.stats().outputUsed; ++n) {
        const uint32_t before = platform_.stats().outputWritten;
        platform_.service();
        if (platform_.stats().outputWritten == before)
            break;
    }
    // Only the console can move the selection while it holds work (the bench admits none), as
    // ProbeMotion follows its own configuration advance; the bench may move it otherwise.
    if (holds())
        remember();
}
bool BenchConsole::holds() const noexcept {
    return console_.retainsOperation() || console_.tracing() || device_.monitoring() ||
           device_.discovery().active();
}
bool BenchConsole::fault(unsigned node) const noexcept {
    const C::NodeSnapshot* n = firmware_.services().node(uint8_t(node - 1));
    const uint32_t base = emcyBaseline_[node - 1];
    return n && n->emcyCount != base && (n->emcyCount != base + 1 || n->emcy.errorCode != 0);
}
bool BenchConsole::ownsDownload(MotorControlCANOwner::SequenceId sequence,
                                const MotorControlCAN::CANOPEN::SdoTransfer& transfer) const noexcept {
    for (const BenchAxis* axis : axes_)
        if (axis && axis->motion.ownsDownload(sequence, transfer))
            return true;
    return false;
}
bool BenchConsole::preflight(MotorControlCANOwner::SequenceId sequence,
                             const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t now) noexcept {
    for (unsigned node = 1; node <= 2; ++node) {
        BenchAxis* axis = axes_[node - 1];
        if (axis && axis->motion.ownsDownload(sequence, transfer))
            return axis->motion.preflight(sequence, transfer, now, fault(node));
    }
    return false;
}
} // namespace MotorControlCANExample
