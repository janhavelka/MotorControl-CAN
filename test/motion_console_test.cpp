// SPDX-License-Identifier: MIT
// HOST_FAKE only: the shared console uses the real app, owner, session and SDO engine.
// The server and every qualification record below are synthetic, never bench evidence.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "MotionIntent.h"
#include "ProbeConsole.h"
#include "ProbeMotion.h"
#include "support/MoveRig.h"

namespace {
using namespace MoveRig;
std::string transcript;

void contains(const std::string& value, const char* expected) {
    if (value.find(expected) == std::string::npos)
        std::fprintf(stderr, "Missing [%s] in:\n%s\n", expected, value.c_str());
    CHECK(value.find(expected) != std::string::npos);
}
struct Bytes {
    std::string output;
    bool blocked = false;
    static bool write(void* context, uint8_t byte) {
        Bytes& self = *static_cast<Bytes*>(context);
        if (self.blocked)
            return false;
        self.output += static_cast<char>(byte);
        return true;
    }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks value;
        value.context = this;
        value.writeByte = write;
        return value;
    }
};
struct ConsoleRig : Rig {
    enum class Environment { HOST_FAKE };
    const Environment environment = Environment::HOST_FAKE;
    Bytes bytes;
    E::ProbePlatform platform;
    E::ProbeApp app;
    L::ReadTarget live;
    E::ProbeMotion motion;
    E::ProbeConsole console;

    ConsoleRig()
        : platform(bytes.callbacks()), app(owner, platform), live(current()),
          motion(app, owner, a, live, one.coordinates, one.witness), console(app, owner, platform) {
        serviceSession = serviceTwo = false;
        CHECK(environment == Environment::HOST_FAKE);
        CHECK(app.select(live));
        CHECK(app.openAxis(a, live, L::SettingsModel::CL86_C, now) == &a);
        CHECK(motion.current());
        CHECK(console.attachMotion(motion));
        CHECK(wire.sentCount == 0);
    }
    void prepare() {
        readIdentity();
        writeThrough(0x6040, 2, 0x000f);
        readState(state);
        one.coordinates.originKnown = true;
        one.coordinates.originSource = M::ScaleSource::QUALIFIED;
        one.witness = witness(*this, drive.position);
        L::MoveEvidence synthetic = evidence();
        synthetic.relativeBase = L::RelativeBase::ACTUAL;
        CHECK(
            motion.setMovePrerequisites(state, identity, synthetic, profile(), nullptr, options()));
    }
    void tick() {
        now += TICK;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (holdTx && wire.pending[served])
                break;
            if (dropped[served])
                continue;
            M::Frame reply;
            if (drive.answer && replyTo(wire.sent[served], drive, reply) && !drive.mute)
                wire.push(reply, now, owner.controllerEpoch());
        }
        if (asyncTraffic) {
            M::Frame heartbeat = frame(0x701, 1);
            heartbeat.data[0] = 5;
            wire.push(heartbeat, now, owner.controllerEpoch());
        }
        CHECK(app.service(now));
        CHECK(motion.service(now));
        console.service(now);
    }
    void drain(unsigned turns = 1000) {
        for (unsigned i = 0; i < turns; ++i)
            tick();
    }
    void line(const std::string& value) {
        for (char c : value)
            console.feed(uint8_t(c), now);
        console.feed('\n', now);
    }
    std::string command(const std::string& value) {
        const size_t first = bytes.output.size();
        line(value);
        for (unsigned i = 0; i < 4000; ++i) {
            tick();
            if (console.drained() && !platform.stats().outputUsed)
                return bytes.output.substr(first);
        }
        CHECK(false); // A bounded fake must settle; no hidden deadline extension or retry.
        return bytes.output.substr(first);
    }
};

void retainedNativeMove() {
    scenario = "same graph and native move retention";
    ConsoleRig r;
    r.prepare();
    CHECK(r.motion.matches(r.app, r.owner));
    const unsigned first = r.wire.sentCount;
    CHECK(r.motion.requestMove(moveRequest(16000), r.now, r.now + LEAD));
    CHECK(r.motion.retained() && r.wire.sentCount == first);
    CHECK(!r.motion.releaseNormal());
    asyncTraffic = true;
    r.drain();
    asyncTraffic = false;
    const E::MotionResult& result = r.motion.normalResult();
    CHECK(result.kind == E::MotionKind::MOVE && result.id && result.nativeOperation);
    CHECK(result.state == M::ActionState::SUCCEEDED);
    CHECK(result.outcome == M::ActionOutcome::OBSERVED && !result.uncertain);
    CHECK(result.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(result.target.present && result.target.native == 16000);
    CHECK(result.rate.present && result.rate.native == 120);
    CHECK(result.rate.rateBasis == M::NativeRateBasis::MOTOR_TURNS);
    CHECK(result.rate.rateTime == M::TimeUnit::MINUTE);
    CHECK(downloadAt(r, first, 0x607a, 4, 16000));
    CHECK(downloadAt(r, first + 1, 0x6081, 4, 120));
    CHECK(downloadAt(r, first + 2, 0x6083, 4, 100));
    CHECK(downloadAt(r, first + 3, 0x6084, 4, 200));
    CHECK(r.services.node(0)->heartbeat.present);
    const Saved<E::MotionResult> retained(result);
    const unsigned sent = r.wire.sentCount;
    r.drain(5);
    retained.unchanged(result);
    CHECK(r.wire.sentCount == sent);
    CHECK(r.motion.releaseNormal() && !r.motion.retained());
    CHECK(!r.motion.releaseNormal());
}

void startupAndRejectedGrammar() {
    scenario = "attachment and command rejection produce no traffic";
    ConsoleRig r;
    const char* invalid[] = {"moveby",
                             "moveby 1 steps extra",
                             "moveto",
                             "moveto 1 unknown",
                             "move relative",
                             "move absolute 1 steps native 120",
                             "move absolute 1 steps native 120 configured setup stored",
                             "move angle 1 deg motor shortest reject 120",
                             "move angle 180 deg motor wrong reject 120 configured",
                             "velocity",
                             "velocity 1 rpm native 0 configured normal",
                             "velocity 1 rpm native 1 configured wrong",
                             "home",
                             "home 1 2 3 4",
                             "home 1 2 3 4 zero extra",
                             "enable extra",
                             "alarm-clear extra",
                             "position-clear 1",
                             "stop",
                             "stop sideways",
                             "speed 1 rpm extra",
                             "accel 1 us",
                             "decel 1/2 ms",
                             "motion stored",
                             "stepsperturn 0",
                             "move absolute 9223372036854775808 steps native 120 configured",
                             "move absolute -9223372036854775809 steps native 120 configured",
                             "move absolute 1/0 steps native 120 configured",
                             "velocity 1 rpm native 18446744073709551616 configured normal"};
    for (const char* command : invalid) {
        const uint32_t invalidBefore = r.console.stats().invalid;
        contains(r.command(std::string("@10 ") + command), "\"ok\":false");
        CHECK(r.console.stats().invalid > invalidBefore);
        CHECK(!r.motion.retained());
        CHECK(r.wire.sentCount == 0);
    }
    const char* unarmed[] = {"moveby 1",
                             "moveto 1",
                             "move relative 1 steps native 120 configured",
                             "move absolute 1 steps native 120 configured",
                             "move angle 90 deg motor shortest reject 120 configured",
                             "velocity 1 rpm native 100 configured normal",
                             "home 1 120 60 100 zero",
                             "enable",
                             "alarm-clear",
                             "position-clear",
                             "stop normal",
                             "stop fast",
                             "motor-release"};
    for (const char* command : unarmed) {
        contains(r.command(std::string("@11 ") + command), "\"ok\":false");
        CHECK(!r.motion.retained());
        CHECK(r.wire.sentCount == 0);
    }
    contains(r.command("@12 help motor-release"), "\"ok\":false");
    const std::string caps = r.command("@13 caps");
    contains(caps, "D-17");
    CHECK(r.wire.sentCount == 0);
}

void consoleMoveRoutes() {
    scenario = "shared console move grammar reaches native preparation";
    const char* commands[] = {"move absolute 16000 steps native 120 configured",
                              "move relative 16000 steps native 120 configured basis actual",
                              "move angle 90 deg motor positive reject 120 configured",
                              "moveto 16000 steps", "moveby 16000 steps"};
    for (unsigned row = 0; row < sizeof commands / sizeof commands[0]; ++row) {
        ConsoleRig r;
        contains(r.command("@1 speed 120 rpm"), "\"ok\":true");
        contains(r.command("@2 accel 100 ms"), "\"ok\":true");
        contains(r.command("@3 decel 200 ms"), "\"ok\":true");
        r.prepare();
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command(std::string("@4 ") + commands[row]);
        if (row == 0)
            transcript += output;
        contains(output, "\"ok\":true");
        CHECK(r.motion.retained());
        const L::MoveContext* native = r.motion.nativeMove();
        CHECK(native && native->state == M::ActionState::SUCCEEDED);
        const int32_t target = row == 2 ? 800 : 16000;
        CHECK(native->parameters.target.effectiveNative == target);
        CHECK(native->speed.effectiveNative == 120);
        CHECK(native->parameters.settings.rampUpTimeMs == 100 &&
              native->parameters.settings.rampDownTimeMs == 200);
        CHECK(downloadAt(r, first, 0x607a, 4, uint32_t(target)));
        CHECK(downloadAt(r, first + 1, 0x6081, 4, 120));
        CHECK(!downloadsFrom(r, first, 0x2300));
        CHECK(!downloadsFrom(r, first, 0x2302));
    }
}

void simpleIntentNeverUsesDefaults() {
    scenario = "simple move requires every explicit host choice";
    for (unsigned missing = 0; missing < 7; ++missing) {
        ConsoleRig r;
        if (missing != 0)
            contains(r.command(missing == 4 ? "@1 speed 0 rpm" : "@1 speed 120 rpm"),
                     "\"ok\":true");
        if (missing != 1)
            contains(r.command(missing == 5 ? "@2 accel 0 ms" : "@2 accel 100 ms"), "\"ok\":true");
        if (missing != 2)
            contains(r.command("@3 decel 200 ms"), "\"ok\":true");
        r.prepare();
        if (missing == 3)
            r.one.coordinates.units.commandStepsPerMotorTurn = M::UnitScale();
        if (missing == 6)
            r.one.coordinates.originKnown = false;
        const unsigned first = r.wire.sentCount;
        contains(r.command(missing == 6 ? "@4 moveto 10 steps" : "@4 moveby 10 steps"),
                 "\"ok\":false");
        CHECK(r.wire.sentCount == first && !r.motion.retained());
    }
}

void rpmDestinationIsCheckedBeforeAdmission() {
    scenario = "default native rate dimension must not write the in-range wrong value 267";
    ConsoleRig r;
    r.prepare();
    r.one.coordinates.nativeRateBasis = M::NativeRateBasis::COMMAND_INCREMENTS;
    r.one.coordinates.nativeVelocityTime = M::TimeUnit::SECOND;
    M::VelocityRequest request;
    request.value = M::Rational(1);
    request.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    request.frame = M::CoordinateFrame::LOAD;
    request.configurationGeneration = r.one.coordinates.binding.configurationGeneration;
    request.rounding = M::Rounding::NEAREST;
    request.maximumQuantizationError = M::Rational(1);
    M::PreparedVelocityTarget arithmetic;
    CHECK(M::prepareVelocityTarget(request, r.one.coordinates, arithmetic));
    CHECK(arithmetic.effectiveNative == 267);
    const unsigned first = r.wire.sentCount;
    contains(r.command("@1 velocity 1 rpm load 100 configured normal"), "\"ok\":false");
    contains(r.command("@2 speed 120 rpm"), "\"ok\":false");
    CHECK(!r.motion.retained() && r.wire.sentCount == first);
    CHECK(!downloadsFrom(r, first, 0x60ff) && !downloadsFrom(r, first, 0x6081));
}

E::StopPolicy stopPolicy(const ConsoleRig& r) {
    E::StopPolicy policy;
    policy.options.service = reads();
    policy.options.observationIntervalUs = 1000;
    policy.options.maximumAgeUs = 60000;
    policy.deadlineUs = r.now + LEAD;
    policy.bestEffortDeadlineUs = r.now + LEAD;
    return policy;
}

void stopNeverReplacesUnknownMotion() {
    scenario = "explicit stop retains the earlier unknown trigger";
    ConsoleRig r;
    contains(r.command("@1 accel 100 ms"), "\"ok\":true");
    contains(r.command("@2 decel 200 ms"), "\"ok\":true");
    r.prepare();
    r.drive.silentIndex = 0x6040;
    r.drive.silentWord = 0x001f;
    const unsigned first = r.wire.sentCount;
    const std::string admission = r.command("@21 move absolute 16000 steps native 120 configured");
    contains(admission, "\"result\":\"accepted\"");
    transcript += admission;
    CHECK(r.motion.normalResult().state == M::ActionState::FAILED);
    CHECK(r.motion.normalResult().execution == M::ActionExecution::UNKNOWN);
    CHECK(r.motion.normalResult().uncertain);
    const Saved<E::MotionResult> failed(r.motion.normalResult());
    CHECK(r.motion.setStopPrerequisites(r.state, stopPolicy(r)));
    transcript += r.command("@22 stop fast");
    const E::MotionStopResult* stopped = r.motion.stopResult(L::StopKind::QUICK_STOP);
    CHECK(stopped && stopped->result.kind == E::MotionKind::STOP_FAST);
    CHECK(stopped->result.state != M::ActionState::ACTIVE);
    CHECK(stopped->result.id != r.motion.normalResult().id);
    failed.unchanged(r.motion.normalResult());
    CHECK(wordsFrom(r, first, 0x001f) == 1);
    CHECK(wordsFrom(r, first, 0x000b) == 1);
    CHECK(!downloadsFrom(r, first, 0x2300));
    const unsigned sent = r.wire.sentCount;
    r.drain(10);
    CHECK(r.wire.sentCount == sent);
    failed.unchanged(r.motion.normalResult());
}

void congestedEscalationKeepsIndependentResults() {
    scenario = "reserved normal and fast stop slots under ordinary result/output pressure";
    ConsoleRig r;
    // A real completed read occupies the first ordinary console correlation slot.
    contains(r.command("@30 read identity"), "\"result\":\"accepted\"");
    contains(r.command("@1 accel 100 ms"), "\"ok\":true");
    contains(r.command("@2 decel 200 ms"), "\"ok\":true");
    r.prepare();
    r.drive.travelReads = 100;
    r.line("@31 move absolute 16000 steps native 120 configured");
    for (unsigned i = 0;
         i < 100 && (!r.drive.edges || r.a.moveObservation().state != M::ReadState::SUCCEEDED); ++i)
        r.tick();
    CHECK(r.drive.edges == 1 && r.drive.moving);
    CHECK(r.a.moveObservation().state == M::ReadState::SUCCEEDED);
    CHECK(r.motion.retained() && r.motion.normalResult().state == M::ActionState::ACTIVE);
    // Both ordinary correlations stay retained; neither may be evicted to admit stop.
    CHECK(E::ProbeConsole::OPERATION_CAPACITY == 2);
    CHECK(r.motion.setStopPrerequisites(r.a.moveObservation(), stopPolicy(r)));
    r.bytes.blocked = true;
    uint8_t padding[E::ProbePlatform::OUTPUT_CAPACITY];
    std::memset(padding, ' ', sizeof padding);
    CHECK(r.platform.writeOutput(padding, sizeof padding - r.platform.stats().outputUsed));
    r.line("@32 stats");
    r.line("@33 version");
    CHECK(r.console.stats().queued >= E::ProbeConsole::ORDINARY_REPLIES);
    const uint32_t dropped = r.console.stats().outputDropped;
    r.line("@34 version");
    CHECK(r.console.stats().outputDropped == dropped + 1);
    for (unsigned i = 0; i < 8; ++i)
        r.line("@200 stop invalid extra");
    r.line(std::string(300, 'x'));
    const unsigned first = r.wire.sentCount;
    r.holdAdapter();
    r.line("@35 stop normal");
    CHECK(r.motion.stopRetained(L::StopKind::HALT));
    for (unsigned i = 0; i < 20 && r.wire.sentCount == first; ++i)
        r.tick();
    CHECK(r.wire.sentCount > first);
    CHECK(r.wire.pending[first]);
    // Fast escalation reuses the owned HALT chain, without extending its original cutoff
    // or reacquiring a borrowed external state after normal-stop acceptance consumed it.
    const uint64_t normalDeadline = r.a.stopAction(L::StopKind::HALT).deadlineUs;
    r.line("@36 stop fast");
    CHECK(r.motion.stopRetained(L::StopKind::HALT));
    CHECK(r.motion.stopRetained(L::StopKind::QUICK_STOP));
    CHECK(r.motion.stopResult(L::StopKind::HALT)->superseded);
    CHECK(r.a.stopAction(L::StopKind::HALT).deadlineUs == normalDeadline);
    CHECK(r.a.bestEffort().deadlineUs == normalDeadline);
    // The fast request cannot recall, replay or pretend completion of the held frame.
    CHECK(r.wire.sentCount == first + 1 && r.wire.pending[first]);
    r.releaseAdapter(first, true);
    r.drain(150);
    const E::MotionStopResult* normal = r.motion.stopResult(L::StopKind::HALT);
    const E::MotionStopResult* fast = r.motion.stopResult(L::StopKind::QUICK_STOP);
    CHECK(normal && fast && normal->result.id != fast->result.id);
    CHECK(normal->result.state != M::ActionState::ACTIVE);
    CHECK(fast->result.state != M::ActionState::ACTIVE);
    CHECK(fast->result.execution == M::ActionExecution::UNKNOWN);
    CHECK(fast->result.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(normal->result.outcome == M::ActionOutcome::CANCELLED);
    CHECK(r.motion.normalResult().state == M::ActionState::FAILED);
    const Saved<E::MotionResult> original(r.motion.normalResult());
    r.bytes.blocked = false;
    r.drain();
    original.unchanged(r.motion.normalResult());
    CHECK(wordsFrom(r, first, 0x010f) <= 1 && wordsFrom(r, first, 0x000b) == 1);
    const bool fenced = r.a.moveFenced();
    CHECK(r.motion.releaseNormal());
    CHECK(r.a.moveFenced() == fenced);
}

void cancelSettlesTheAcceptedDownload() {
    scenario = "local cancel preserves in-flight settlement without a stop";
    ConsoleRig r;
    contains(r.command("@1 accel 100 ms"), "\"ok\":true");
    contains(r.command("@2 decel 200 ms"), "\"ok\":true");
    r.prepare();
    const unsigned first = r.wire.sentCount;
    r.holdAdapter();
    r.line("@40 move absolute 16000 steps native 120 configured");
    for (unsigned i = 0; i < 20 && r.wire.sentCount == first; ++i)
        r.tick();
    CHECK(r.wire.sentCount == first + 1 && r.wire.pending[first]);
    CHECK(downloadAt(r, first, 0x607a, 4, 16000));
    r.line("@41 cancel 1");
    CHECK(r.motion.normalResult().state == M::ActionState::ACTIVE);
    CHECK(r.wire.pending[first] && !r.motion.releaseNormal());
    r.releaseAdapter(first, true);
    r.drain();
    CHECK(r.motion.normalResult().state == M::ActionState::FAILED);
    CHECK(r.motion.normalResult().outcome == M::ActionOutcome::CANCELLED);
    CHECK(r.drive.target == 16000 && !r.drive.edges);
    CHECK(r.wire.sentCount == first + 1);
    CHECK(!wordsFrom(r, first, 0x000b) && !wordsFrom(r, first, 0x010f));
    CHECK(!downloadsFrom(r, first, 0x2300));
}

void deviceZeroNeverSavesOrSetsAHostOrigin() {
    scenario = "device zero uses only 2302 and never saves parameters";
    ConsoleRig r;
    r.drive.position = 123;
    r.prepare();
    r.readState(r.state, uint8_t(STATE_MASK | bit(L::StateBlock::FEEDBACK)));
    L::ActionOptions policy;
    policy.service = reads();
    policy.observationIntervalUs = 1000;
    policy.maximumAgeUs = 60000;
    CHECK(r.motion.setClearPrerequisites(r.state, policy));
    const unsigned first = r.wire.sentCount;
    const std::string output = r.command("@50 position-clear");
    contains(output, "\"result\":\"accepted\"");
    CHECK(r.motion.normalResult().kind == E::MotionKind::POSITION_CLEAR);
    CHECK(r.motion.normalResult().state == M::ActionState::SUCCEEDED);
    CHECK(downloadAt(r, first, 0x2302, 2, 0));
    CHECK(downloadAt(r, first + 1, 0x2302, 2, 1));
    CHECK(downloadAt(r, first + 2, 0x2302, 2, 0));
    CHECK(!downloadsFrom(r, first, 0x2300));
    CHECK(!downloadsFrom(r, first, 0x6040));
    CHECK(!r.one.coordinates.originKnown && !r.one.witness.nativeKnown);
    transcript += output;
}

void explicitEnableAndAlarmClear() {
    scenario = "explicit synthetic enable evidence reaches the public lifecycle";
    {
        ConsoleRig r;
        r.drive.followEnable = true;
        r.readIdentity();
        r.readState(r.state, ENABLE_MASK);
        L::EnableEvidence proof;
        proof.gate = L::EnableGate::ATTENDED_EXPERIMENT;
        proof.trial = 1;
        proof.start = L::EnableStart::NO_MOTION_WRITE_RECORDED;
        proof.powerUpShaft = L::ShaftObservation::FREE;
        proof.reviewed = true;
        proof.reviewedStatusword = IDLE_STATUS;
        proof.reviewedPositionTarget = 5000;
        proof.reviewedVelocityTarget = 120;
        proof.reservation = r.reservation();
        proof.operation = r.a.nextOperation();
        proof.expiresUs = r.now + 1000000;
        L::ActionOptions policy;
        policy.service = reads();
        policy.observationIntervalUs = 1000;
        policy.maximumAgeUs = 60000;
        policy.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
        CHECK(r.motion.setEnablePrerequisites(r.state, proof, policy));
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@60 enable");
        contains(output, "\"result\":\"accepted\"");
        CHECK(r.motion.normalResult().kind == E::MotionKind::ENABLE);
        CHECK(r.motion.normalResult().state == M::ActionState::SUCCEEDED);
        CHECK(wordsFrom(r, first, 6) == 1 && wordsFrom(r, first, 7) == 1 &&
              wordsFrom(r, first, 15) == 1);
        CHECK(!downloadsFrom(r, first, 0x2300) && !downloadsFrom(r, first, 0x607a));
        transcript += output;
    }
    scenario = "explicit alarm clear has no enable or parameter save";
    {
        ConsoleRig r;
        r.drive.monitor = L::MONITOR_ALARM;
        r.drive.errorCode = 0xff05; // Native action's required raw code, manufactured by this fake.
        r.readState(r.state, uint8_t(STATE_MASK | bit(L::StateBlock::ERRORS) |
                                     bit(L::StateBlock::ERROR_HISTORY)));
        L::ActionOptions policy;
        policy.service = reads();
        policy.observationIntervalUs = 1000;
        policy.maximumAgeUs = 60000;
        CHECK(r.motion.setClearPrerequisites(r.state, policy));
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@61 alarm-clear");
        contains(output, "\"result\":\"accepted\"");
        CHECK(r.motion.normalResult().kind == E::MotionKind::ALARM_CLEAR);
        CHECK(r.motion.normalResult().state == M::ActionState::SUCCEEDED);
        CHECK(downloadAt(r, first, 0x2301, 2, 1));
        CHECK(downloadAt(r, first + 1, 0x2301, 2, 0));
        CHECK(!downloadsFrom(r, first, 0x2300) && !downloadsFrom(r, first, 0x6040));
        transcript += output;
    }
}

void liveHostChangesFollowOnlyTheirOwnEpoch() {
    scenario = "live host scale and origin follow the actual owner without traffic";
    {
        ConsoleRig unchanged;
        unchanged.prepare();
        const uint64_t observed = unchanged.one.witness.observedUs;
        const unsigned first = unchanged.wire.sentCount;
        CHECK(unchanged.motion.configureCommandScale(
            unchanged.one.coordinates.units.commandStepsPerMotorTurn, unchanged.now));
        CHECK(unchanged.one.witness.nativeKnown);
        CHECK(unchanged.one.witness.observedUs == observed);
        CHECK(unchanged.motion.current() && unchanged.wire.sentCount == first);
    }
    ConsoleRig r;
    r.prepare();
    const uint32_t generation = r.live.channel.binding.configurationGeneration;
    const unsigned first = r.wire.sentCount;
    contains(r.command("@70 stepsperturn 6400"), "\"ok\":true");
    CHECK(r.motion.current());
    CHECK(r.live.channel.binding.configurationGeneration == generation + 1);
    CHECK(M::sameBinding(r.live.channel.binding, r.a.binding()));
    CHECK(M::sameBinding(r.app.selected()->channel.binding, r.live.channel.binding));
    CHECK(r.one.coordinates.units.commandStepsPerMotorTurn.numerator == 6400);
    CHECK(!r.one.witness.nativeKnown && !r.one.coordinates.originKnown);
    CHECK(r.wire.sentCount == first);
    CHECK(!r.motion.requestMove(moveRequest(100), r.now, r.now + LEAD));
    CHECK(r.wire.sentCount == first && !r.motion.retained());
    // A new explicit HOST_FAKE observation supplies the witness consumed by host origin.
    r.one.witness = witness(r, r.drive.position);
    r.one.witness.binding = r.one.coordinates.binding;
    r.one.witness.referenceGeneration = r.one.coordinates.referenceGeneration;
    contains(r.command("@71 axis origin 10"), "\"ok\":true");
    CHECK(r.motion.current());
    CHECK(r.live.channel.binding.configurationGeneration == generation + 2);
    CHECK(r.one.coordinates.originKnown && r.one.coordinates.originNative == 10);
    CHECK(!r.one.witness.nativeKnown);
    CHECK(r.wire.sentCount == first);
    const Saved<M::AxisConfig> configured(r.one.coordinates);
    const L::ReadTarget actual = r.live;
    ++r.live.channel.clientToServerId;
    CHECK(!r.motion.current());
    CHECK(!r.motion.requestMove(moveRequest(100), r.now, r.now + LEAD));
    contains(r.command("@72 speed 120 rpm"), "\"ok\":false");
    configured.unchanged(r.one.coordinates);
    CHECK(r.wire.sentCount == first && !r.motion.retained());
    r.live = actual;
    CHECK(r.motion.current());
}

void replacedNativeContextCannotBeCancelledOrReleased() {
    scenario = "hostile external native context replacement is never mistaken for owned work";
    ConsoleRig r;
    r.prepare();
    CHECK(r.motion.requestMove(moveRequest(10000), r.now, r.now + LEAD));
    const uint32_t owned = r.motion.normalResult().nativeOperation;
    // Deliberately violate attachment ownership through public native calls: finish without
    // refreshing the helper, then admit another native move in the same session.
    r.serviceSession = true;
    r.finishMove();
    CHECK(r.motion.normalResult().state == M::ActionState::ACTIVE);
    r.readIdentity();
    r.readState(r.state);
    CHECK(r.a.requestMove(r.state, r.identity, evidence(), moveRequest(20000), profile(), nullptr,
                          r.one.coordinates, nullptr, options(), r.now + LEAD, r.now));
    CHECK(r.a.moveAction().operation != owned && r.a.moveActive());
    const Saved<L::MoveContext> unrelated(r.a.moveAction());
    CHECK(!r.motion.cancelNormal(r.now));
    unrelated.unchanged(r.a.moveAction());
    CHECK(!r.motion.releaseNormal());
    CHECK(r.motion.service(r.now));
    CHECK(r.motion.normalResult().outcome == M::ActionOutcome::INVALIDATED);
    CHECK(r.motion.releaseNormal());
    unrelated.unchanged(r.a.moveAction());
    CHECK(r.a.moveActive());
}

void lookupReleaseAndFrozenOutputMode() {
    scenario = "human admission, explicit JSON lookup, release and delayed duplicate ACK";
    ConsoleRig r;
    contains(r.command("@1 accel 100 ms"), "\"ok\":true");
    contains(r.command("@2 decel 200 ms"), "\"ok\":true");
    r.prepare();
    const std::string human = r.command("move absolute 10000 steps native 120 configured");
    contains(human, "accepted operation 1");
    CHECK(human.find("{\"schema\"") == std::string::npos);
    CHECK(!human.empty() && human.back() == '\n');
    const unsigned sent = r.wire.sentCount;
    const Saved<E::MotionResult> result(r.motion.normalResult());
    M::Frame duplicate = frame(0x581, 8);
    duplicate.data[0] = 0x60;
    duplicate.data[1] = 0x7a;
    duplicate.data[2] = 0x60;
    r.wire.push(duplicate, r.now, r.owner.controllerEpoch());
    r.drain(5);
    result.unchanged(r.motion.normalResult());
    const std::string lookup = r.command("@80 result 1");
    contains(lookup, "\"id\":80");
    contains(lookup, "\"operation_id\":1");
    contains(lookup, "\"command_id\":0");
    contains(lookup, "\"outcome\":\"OBSERVED\"");
    contains(r.command("@81 release 4294967295"), "\"ok\":false");
    contains(r.command("@82 release 1"), "\"ok\":true");
    CHECK(!r.motion.retained());
    contains(r.command("@83 result 1"), "\"ok\":false");
    CHECK(r.wire.sentCount == sent);
    transcript += lookup;
}

void unsentStopRemainsActiveAcrossOneExplicitRecovery() {
    scenario = "a failed unsent sequenced component is not a terminal held stop";
    ConsoleRig r;
    r.prepare();
    CHECK(r.motion.setStopPrerequisites(r.state, stopPolicy(r)));
    const unsigned first = r.wire.sentCount;
    r.line("@90 stop normal");
    CHECK(r.motion.stopRetained(L::StopKind::HALT));
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
    r.tick();
    CHECK(r.a.stopPath() == E::StopPath::HELD);
    const E::MotionStopResult* held = r.motion.stopResult(L::StopKind::HALT);
    CHECK(held && held->result.state == M::ActionState::ACTIVE);
    CHECK(!r.motion.releaseStop(L::StopKind::HALT));
    CHECK(r.wire.sentCount == first);
    O::RecoveryId recovery;
    CHECK(r.owner.recover(r.now, r.now + LEAD, recovery));
    r.drain(100);
    held = r.motion.stopResult(L::StopKind::HALT);
    CHECK(held && held->result.state != M::ActionState::ACTIVE);
    CHECK(held->result.outcome == M::ActionOutcome::STOP_SENT_UNCONFIRMED);
    CHECK(held->result.execution == M::ActionExecution::UNKNOWN && held->result.uncertain);
    CHECK(held->sequenced.present && held->bestEffort.present);
    CHECK(held->sequenced.execution == M::ActionExecution::NOT_TRANSMITTED);
    CHECK(!wordsFrom(r, first, 0x010f) && wordsFrom(r, first, 0x000b) == 1);
    const unsigned sent = r.wire.sentCount;
    r.drain(20);
    CHECK(r.wire.sentCount == sent);
}

void replacedNativeStopKeepsItsOwnChainIdentity() {
    scenario = "hostile external stop replacement cannot become the old retained result";
    ConsoleRig r;
    L::ReadContext empty;
    CHECK(r.motion.setStopPrerequisites(empty, stopPolicy(r)));
    CHECK(r.motion.requestStop(L::StopKind::HALT, r.now));
    const uint32_t first = r.a.stopRequestOperation();
    const uint32_t retained = r.motion.stopResult(L::StopKind::HALT)->result.id;
    CHECK(first && r.motion.stopResult(L::StopKind::HALT)->result.state == M::ActionState::ACTIVE);
    // Finish A through the native public path without refreshing the helper's cached A.
    r.serviceSession = true;
    for (unsigned i = 0; i < 100 && r.a.stopActive(); ++i)
        r.Rig::step();
    CHECK(r.a.stopPath() == E::StopPath::FINISHED);
    CHECK(r.motion.stopResult(L::StopKind::HALT)->result.state == M::ActionState::ACTIVE);
    const E::StopPath admitted = r.a.requestStop(L::StopKind::HALT, empty, stopPolicy(r), r.now);
    CHECK(admitted != E::StopPath::NONE && admitted != E::StopPath::FINISHED);
    CHECK(r.a.stopRequestOperation() != first);
    const Saved<L::BestEffortStop> replacement(r.a.bestEffort());
    const uint32_t second = r.a.stopRequestOperation();
    CHECK(r.motion.service(r.now));
    const E::MotionStopResult* stale = r.motion.stopResult(L::StopKind::HALT);
    CHECK(stale && stale->result.id == retained);
    CHECK(stale->result.state == M::ActionState::FAILED);
    CHECK(stale->result.outcome == M::ActionOutcome::INVALIDATED);
    CHECK(!stale->superseded);
    CHECK(!stale->bestEffort.present || stale->bestEffort.operation != second);
    replacement.unchanged(r.a.bestEffort());
    CHECK(!r.motion.cancelNormal(r.now));
    CHECK(r.motion.releaseStop(L::StopKind::HALT));
    replacement.unchanged(r.a.bestEffort());
    CHECK(r.a.stopRequestOperation() == second && r.a.stopActive());
}

void simpleCoordinatesPreserveMotorAndLoadMeaning() {
    scenario = "simple moves apply the explicit motor origin and preserve relative dimensions";
    struct Vector {
        const char* command;
        int32_t native;
        M::CoordinateFrame frame;
    };
    const Vector vectors[] = {
        {"moveto 100 steps", 200, M::CoordinateFrame::MOTOR},
        {"moveto 1 turn", 3300, M::CoordinateFrame::MOTOR},
        {"moveto 5 mm", 16100, M::CoordinateFrame::LOAD},
        {"moveby 90 deg", 800, M::CoordinateFrame::MOTOR},
        {"moveby 3/2 steps", 2, M::CoordinateFrame::NATIVE},
        {"moveby 5/2 steps", 2, M::CoordinateFrame::NATIVE},
    };
    for (const Vector& vector : vectors) {
        ConsoleRig r;
        contains(r.command("@1 speed 120 rpm"), "\"ok\":true");
        contains(r.command("@2 accel 100 ms"), "\"ok\":true");
        contains(r.command("@3 decel 200 ms"), "\"ok\":true");
        r.prepare();
        r.one.coordinates.originNative = 100;
        r.one.coordinates.units.millimetresPerLoadTurn =
            M::UnitScale(5, 1, M::ScaleSource::QUALIFIED);
        const unsigned first = r.wire.sentCount;
        contains(r.command(std::string("@100 ") + vector.command), "\"result\":\"accepted\"");
        const L::MoveContext* native = r.motion.nativeMove();
        CHECK(native && native->state == M::ActionState::SUCCEEDED);
        CHECK(native->parameters.targetNative == vector.native);
        CHECK(native->parameters.target.requested.frame == vector.frame);
        CHECK(downloadAt(r, first, 0x607a, 4, uint32_t(vector.native)));
    }
    for (const char* invalid : {"moveby 1/2 steps", "moveby 1000001 steps"}) {
        ConsoleRig r;
        contains(r.command("@1 speed 120 rpm"), "\"ok\":true");
        contains(r.command("@2 accel 100 ms"), "\"ok\":true");
        contains(r.command("@3 decel 200 ms"), "\"ok\":true");
        r.prepare();
        const unsigned first = r.wire.sentCount;
        contains(r.command(std::string("@101 ") + invalid), "\"ok\":false");
        CHECK(!r.motion.retained() && r.wire.sentCount == first);
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 1 && (argc != 3 || std::strcmp(argv[1], "--transcript")))
        return 2;
    retainedNativeMove();
    startupAndRejectedGrammar();
    consoleMoveRoutes();
    simpleIntentNeverUsesDefaults();
    rpmDestinationIsCheckedBeforeAdmission();
    stopNeverReplacesUnknownMotion();
    congestedEscalationKeepsIndependentResults();
    cancelSettlesTheAcceptedDownload();
    deviceZeroNeverSavesOrSetsAHostOrigin();
    explicitEnableAndAlarmClear();
    liveHostChangesFollowOnlyTheirOwnEpoch();
    replacedNativeContextCannotBeCancelledOrReleased();
    lookupReleaseAndFrozenOutputMode();
    unsentStopRemainsActiveAcrossOneExplicitRecovery();
    replacedNativeStopKeepsItsOwnChainIdentity();
    simpleCoordinatesPreserveMotorAndLoadMeaning();
    if (argc == 3) {
        FILE* output = std::fopen(argv[2], "wb");
        CHECK(output);
        CHECK(std::fwrite(transcript.data(), 1, transcript.size(), output) == transcript.size());
        CHECK(!std::fclose(output));
    } else
        std::printf("motion_console_test: %u checks; HOST_FAKE only\n", checks);
}
