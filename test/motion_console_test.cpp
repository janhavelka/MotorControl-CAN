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
    unsigned writes = 0;
    static bool write(void* context, uint8_t byte) {
        Bytes& self = *static_cast<Bytes*>(context);
        ++self.writes;
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
    bool pressure = false; // Saturated observer bursts and deliberately smaller service work.

    explicit ConsoleRig(bool twoNodes = false)
        : Rig(twoNodes), platform(bytes.callbacks()), app(owner, platform), live(current()),
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
        const bool drained = wire.rxRead == wire.rxWrite;
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
        // A finite burst goes behind the ordinary response. The next turn drains its tail;
        // this exercises observer dispatch at a small CPU work budget without an unbounded queue.
        if (pressure && drained) {
            M::Frame heartbeat = frame(nodes == 2 ? 0x702 : 0x701, 1);
            heartbeat.data[0] = 5;
            M::Frame emcy = frame(0x82, 8);
            emcy.data[0] = 0x23;
            emcy.data[1] = 0x81;
            for (unsigned i = 0; i < 2; ++i) {
                wire.push(heartbeat, now, owner.controllerEpoch());
                // Node 2's fault traffic must not become node 1's move fault. A one-channel
                // capacity fixture retains the same pressure using heartbeat alone.
                wire.push(nodes == 2 ? emcy : heartbeat, now, owner.controllerEpoch());
            }
        }
        const unsigned received = wire.rxRead, reports = wire.reportRead;
        const unsigned sent = wire.sentCount, observed = wire.observed, writes = bytes.writes;
        CHECK(pressure ? app.service(now, budget(4, 1, 1, 1, 1)) : app.service(now));
        CHECK(motion.service(now));
        console.service(now);
        if (pressure) {
            CHECK(wire.rxRead - received <= 4 && wire.observed - observed <= 4);
            CHECK(wire.reportRead - reports <= 1 && wire.sentCount - sent <= 1);
            CHECK(bytes.writes - writes <= E::ProbePlatform::OUTPUT_BUDGET);
        }
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
                             "enable",
                             "mode 101",
                             "mode extra 1",
                             "mode 101 1 extra",
                             "motor-release",
                             "motor-release 101 extra",
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
                             "enable 7", // No attached axis 7; enable, mode and motor-release read their own.
                             "alarm-clear",
                             "position-clear",
                             "stop normal",
                             "stop fast"};
    for (const char* command : unarmed) {
        contains(r.command(std::string("@11 ") + command), "\"ok\":false");
        CHECK(!r.motion.retained());
        CHECK(r.wire.sentCount == 0);
    }
    // Prompt 57: motor-release is a regular command now, through prepareRelease; release ID stays the
    // retained-result command.
    contains(r.command("@12 help motor-release"), "motor-release AXIS_ID");
    const std::string caps = r.command("@13 caps");
    contains(caps, "\"command\":\"motor-release\",\"implemented\":true");
    contains(caps, "\"command\":\"mode\",\"implemented\":true");
    CHECK(caps.find("D-17") == std::string::npos);
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

// Prompt 60: the unit forms reach native traffic only through the library's conversion. MoveRig's axis
// has 3200 steps per motor turn and a 5:1 gear, so 24 load turn/min is 120 motor rpm and 4 load
// turn/s2 (20 motor turns/s2) is 1200 rpm/s: 100 ms at 120 rpm, the same as `accel 100 ms`.
std::string framesFrom(const ConsoleRig& r, unsigned first) {
    std::string bytes;
    for (unsigned i = first; i < r.wire.sentCount; ++i) {
        const M::Frame& f = r.wire.sent[i];
        bytes.append(reinterpret_cast<const char*>(&f.id), sizeof f.id);
        bytes += char(f.dlc);
        bytes.append(reinterpret_cast<const char*>(f.data), sizeof f.data);
    }
    return bytes;
}
std::string runMoveConsole(const char* const* setup, unsigned count, const char* move, std::string* output) {
    ConsoleRig r;
    for (unsigned i = 0; i < count; ++i)
        contains(r.command(setup[i]), "\"ok\":true");
    r.prepare();
    const unsigned first = r.wire.sentCount;
    const std::string reply = r.command(move);
    contains(reply, "\"ok\":true");
    const L::MoveContext* native = r.motion.nativeMove();
    CHECK(native && native->state == M::ActionState::SUCCEEDED);
    CHECK(native->parameters.settings.rampUpTimeMs == 100 && native->parameters.settings.rampDownTimeMs == 200);
    CHECK(downloadAt(r, first + 2, 0x6083, 4, 100));
    if (output)
        *output = reply;
    return framesFrom(r, first);
}
void accelerationFormsGiveTheNativeTraffic() {
    scenario = "acceleration and speed unit forms give the millisecond form's traffic";
    const char* native[] = {"@1 speed 120 rpm", "@2 accel 100 ms", "@3 decel 200 ms"};
    const char* units[] = {"@1 speed 24 turn/min load", "@2 accel 4 turn/s2 load", "@3 decel 200 ms"};
    const std::string simple = runMoveConsole(native, 3, "@4 moveby 16000 steps", nullptr);
    std::string reply;
    CHECK(runMoveConsole(units, 3, "@4 moveby 16000 steps", &reply) == simple);
    transcript += reply; // console_protocol_test parses this real ramp_value record.
    // The record reports the library's own result for the same inputs.
    contains(reply, "\"type\":\"ramp_value\",\"id\":4,\"command\":\"moveby\",\"operation_id\":1,"
                    "\"field\":\"ramp_up\",\"unit\":\"ms\",\"native\":\"100\",\"exact\":[\"100\",\"1\"],"
                    "\"error\":[\"0\",\"1\",false],\"speed_rpm\":\"120\",\"acceleration_unit\":\"turn/s2\","
                    "\"frame\":\"load\",\"effective\":[\"4\",\"1\"],\"acceleration_error\":[\"0\",\"1\",false]");
    const std::string detailed = runMoveConsole(native, 3, "@4 move absolute 16000 steps native 120 configured", nullptr);
    const char* decelOnly[] = {"@3 decel 200 ms"};
    CHECK(runMoveConsole(decelOnly, 1,
                         "@4 move absolute 16000 steps native speed 24 turn/min load accel 4 turn/s2 load configured",
                         &reply) == detailed);
    contains(reply, "\"type\":\"ramp_value\"");
    contains(reply, "\"command\":\"move\"");
    // rpm/s in the native frame and an inline clause with its own rounding: 120 rpm at 1199 rpm/s is
    // 120000/1199 = 100.08 ms, nearest 100; the effective acceleration is 1200 rpm/s, error 1 rpm/s.
    CHECK(runMoveConsole(decelOnly, 1,
                         "@4 move absolute 16000 steps native 120 accel 1199 rpm/s native round nearest 1 configured",
                         &reply) == detailed);
    contains(reply, "\"exact\":[\"120000\",\"1199\"],\"error\":[\"100\",\"1199\",true]");
    contains(reply, "\"effective\":[\"1200\",\"1\"],\"acceleration_error\":[\"1\",\"1\",false]");
    L::PreparedAccelerationRamp expected;
    M::AccelerationRequest acceleration;
    acceleration.value = M::Rational(1199);
    acceleration.unit = M::AccelerationUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE, M::TimeUnit::SECOND);
    acceleration.configurationGeneration = axisConfig().binding.configurationGeneration;
    acceleration.rounding = M::Rounding::NEAREST;
    acceleration.maximumAccelerationError = M::Rational(1);
    CHECK(L::prepareAccelerationRamp(expected, L::RampUse::MOVE_START, acceleration, moveRequest(16000, 120).speed,
                                     axisConfig()));
    CHECK(expected.time.effectiveMs == 100 && expected.time.exactMs.numerator == 120000 &&
          expected.time.exactMs.denominator == 1199);
}
void accelerationFormRefusalsBeforeTraffic() {
    scenario = "acceleration-form refusals before any frame";
    ConsoleRig r;
    contains(r.command("@1 speed 120 rpm"), "\"ok\":true");
    contains(r.command("@2 accel 100 ms"), "\"ok\":true");
    contains(r.command("@3 decel 200 ms"), "\"ok\":true");
    r.prepare();
    const unsigned first = r.wire.sentCount;
    struct Refusal { const char* command; const char* reason; };
    const Refusal refusals[] = {
        {"@4 decel 4 turn/s2 load", "move deceleration (6084) is unmeasured"},
        {"@5 move absolute 16000 steps native 120 decel 4 turn/s2 load configured",
         "move deceleration (6084) is unmeasured"},
        {"@6 move absolute 16000 steps native 400 accel 4000 rpm/s native configured", "measured at 2 to 300 rpm"},
        {"@7 move absolute 16000 steps native 120 accel 12000 rpm/s native configured", "\"status\":\"ILLEGAL_VALUE\""},
        {"@8 home 17 120 60 accel 600 rpm/s native zero", "the homing ramp (609A) is unmeasured"},
        {"@9 move absolute 16000 steps native 120 accel 4 rad/s2 motor configured", "\"status\":\"UNSUPPORTED\""},
        {"@10 move absolute 16000 steps native 120 accel 0 rpm/s native configured", "\"ok\":false"},
        {"@11 move absolute 16000 steps native 120 accel 4 us load configured", "\"ok\":false"},
        {"@12 accel 4 turn/s2 sideways", "\"ok\":false"},
        {"@13 speed 5 mm/s load", "\"ok\":false"}, // No lead on this axis.
    };
    for (const Refusal& refusal : refusals) {
        const std::string output = r.command(refusal.command);
        contains(output, "\"ok\":false");
        contains(output, refusal.reason);
        CHECK(output.find("ramp_value") == std::string::npos);
        CHECK(!r.motion.retained() && r.wire.sentCount == first);
    }
    // An acceleration set for 6083 converts per move: a speed outside the law refuses the move, and
    // `accel MS ms` returns 6083 to milliseconds.
    contains(r.command("@14 accel 4 turn/s2 load"), "\"ok\":true");
    const std::string inspected = r.command("@15 accel");
    contains(inspected, "\"value\":[\"4\",\"1\"],\"unit\":\"turn/s2\",\"frame\":\"load\"");
    contains(r.command("@16 speed 400 rpm"), "\"ok\":true");
    const std::string refused = r.command("@17 moveby 16000 steps");
    contains(refused, "\"ok\":false");
    contains(refused, "measured at 2 to 300 rpm");
    CHECK(!r.motion.retained() && r.wire.sentCount == first);
    contains(r.command("@18 accel 100 ms"), "\"ok\":true");
    contains(r.command("@19 accel"), "\"value\":\"100\",\"unit\":\"ms\"");
    // A set speed in a unit previews its whole rpm and names its unit and frame.
    contains(r.command("@20 speed 25 turn/min load"), "\"rpm\":\"125\",\"rpm_error\":[\"0\",\"1\",false]");
    contains(r.command("@21 speed"), "\"value\":[\"25\",\"1\"],\"unit\":\"rpm\",\"frame\":\"load\"");
}

// Stage (c): the text form of ramp_value carries the same fields, and a held acceleration goes
// stale with its configuration epoch: after stepsperturn the move is refused before any frame.
void accelerationTextFormAndStaleness() {
    scenario = "ramp_value text form and a stale held acceleration";
    {
        ConsoleRig r;
        r.prepare();
        r.command("speed 24 turn/min load");
        r.command("accel 4 turn/s2 load");
        r.command("decel 200 ms");
        const std::string output = r.command("moveby 16000 steps");
        contains(output, "moveby: operation=1 ramp_up native=100 ms exact=100/1 error=0/1 negative=false rpm=120 "
                         "acceleration-unit=turn/s2 frame=load effective=4/1 acceleration-error=0/1 negative=false\n");
        CHECK(output.find("\"type\"") == std::string::npos);
    }
    ConsoleRig r;
    r.prepare();
    contains(r.command("@1 speed 120 rpm"), "\"ok\":true");
    contains(r.command("@2 accel 4 turn/s2 load"), "\"ok\":true");
    contains(r.command("@3 decel 200 ms"), "\"ok\":true");
    contains(r.command("@4 stepsperturn 6400"), "\"ok\":true");
    // A fresh speed, a fresh witness and origin: only the held acceleration is of the old epoch.
    contains(r.command("@5 speed 120 rpm"), "\"ok\":true");
    r.one.witness = witness(r, r.drive.position);
    r.one.witness.binding = r.one.coordinates.binding;
    r.one.witness.referenceGeneration = r.one.coordinates.referenceGeneration;
    const unsigned first = r.wire.sentCount;
    const std::string stale = r.command("@6 moveby 16000 steps");
    contains(stale, "\"ok\":false");
    contains(stale, "\"detail\":5003"); // AXIS_ERROR_BASE + STALE_GENERATION.
    CHECK(stale.find("ramp_value") == std::string::npos);
    CHECK(!r.motion.retained() && r.wire.sentCount == first);
    contains(r.command("@7 accel"), "\"unit\":\"turn/s2\"");
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
    CHECK(E::ProbeConsole::OPERATION_CAPACITY == 2 * C::SdoClient::CHANNELS);
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

// Prompt 45: combine phase interruption with blocked console output, observer traffic and
// constrained owner service. These are deterministic work counts, not a physical WCET claim.
void urgentStopAtEveryPhaseUnderPressure() {
    const L::MovePhase phases[] = {L::MovePhase::PARAMETERS, L::MovePhase::TRIGGER,
                                   L::MovePhase::OBSERVING, L::MovePhase::WAITING,
                                   L::MovePhase::REARM};
    for (L::MovePhase phase : phases) {
        char name[96];
        std::snprintf(name, sizeof name, "urgent console stop at move phase %u under bounded pressure",
                      unsigned(phase));
        scenario = name;
        ConsoleRig r(C::SdoClient::CHANNELS >= 2);
        contains(r.command("@1 accel 100 ms"), "\"ok\":true");
        contains(r.command("@2 decel 200 ms"), "\"ok\":true");
        r.prepare();
        L::MoveOptions policy = options();
        policy.observationIntervalUs = 4000; // Leave a service turn between complete cycles.
        CHECK(r.motion.setMovePrerequisites(r.state, r.identity, evidence(), profile(), nullptr, policy));
        const unsigned first = r.wire.sentCount;
        r.line("@31 move absolute 16000 steps native 120 configured");
        CHECK(r.motion.retained() && r.motion.normalResult().state == M::ActionState::ACTIVE);
        const uint32_t moveId = r.motion.normalResult().id;
        r.bytes.blocked = r.pressure = true;
        uint8_t padding[E::ProbePlatform::OUTPUT_CAPACITY];
        std::memset(padding, ' ', sizeof padding);
        CHECK(r.platform.writeOutput(padding, sizeof padding - r.platform.stats().outputUsed));
        bool reached = false;
        for (unsigned turn = 0; turn < 200 && r.a.moveActive(); ++turn) {
            r.tick();
            const L::MoveContext& c = r.a.moveAction();
            reached = c.phase == phase &&
                      (phase == L::MovePhase::WAITING ? c.observations == 1
                       : phase == L::MovePhase::OBSERVING ? c.observations == 0
                                                         : c.transmissionNoted);
            if (reached)
                break;
        }
        CHECK(reached && r.a.moveActive());
        r.line("@32 stats");
        r.line("@33 version");
        const uint32_t dropped = r.console.stats().outputDropped;
        r.line("@34 version");
        CHECK(r.console.stats().outputDropped > dropped);
        const L::ReadContext& state = r.a.moveObservation().state == M::ReadState::SUCCEEDED
                                          ? r.a.moveObservation() : r.state;
        CHECK(r.motion.setStopPrerequisites(state, stopPolicy(r)));
        const unsigned stopFrom = r.wire.sentCount;
        r.line("@35 stop fast");
        CHECK(r.motion.stopRetained(L::StopKind::QUICK_STOP));
        for (unsigned turn = 0; turn < 300 && r.a.busy(); ++turn)
            r.tick();
        CHECK(!r.a.busy() && !r.a.moveActive());
        const E::MotionStopResult* stopped = r.motion.stopResult(L::StopKind::QUICK_STOP);
        CHECK(stopped && stopped->result.id != moveId &&
              (stopped->result.state == M::ActionState::SUCCEEDED ||
               stopped->result.state == M::ActionState::FAILED));
        CHECK(r.motion.normalResult().id == moveId && r.motion.normalResult().state == M::ActionState::FAILED);
        CHECK(r.motion.normalResult().outcome == M::ActionOutcome::CANCELLED);
        CHECK(r.a.moveAction().interruption == M::MoveInterruption::STOP);
        CHECK(wordsFrom(r, stopFrom, 0x000b) == 1 && !wordsFrom(r, stopFrom, 0x010f));
        CHECK(!wordsFrom(r, stopFrom, 0x000f) && !wordsFrom(r, stopFrom, 0x001f));
        CHECK(wordsFrom(r, first, 0x001f) <= 1);
        for (unsigned i = stopFrom; i < r.wire.sentCount; ++i)
            CHECK(r.wire.sent[i].data[0] == 0x40 || wordAt(r, i, 0x000b));
        CHECK(r.wire.observed > 0 && r.services.node(0)->emcyCount == 0);
        if (r.nodes == 2) {
            CHECK(r.services.node(1)->heartbeat.present && r.services.node(1)->emcyCount > 0);
            CHECK(!r.services.node(0)->heartbeat.present && !r.a.faultBlocked());
        } else {
            CHECK(r.services.node(0)->heartbeat.present);
        }
        const Saved<E::MotionResult> original(r.motion.normalResult());
        const Saved<E::MotionStopResult> stop(*stopped);
        const unsigned sent = r.wire.sentCount;
        r.bytes.blocked = false;
        r.drain(100);
        original.unchanged(r.motion.normalResult());
        stop.unchanged(*r.motion.stopResult(L::StopKind::QUICK_STOP));
        CHECK(r.wire.sentCount == sent);
    }
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
        CHECK(r.motion.setEnablePrerequisites(r.state, r.identity, proof, policy));
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@60 enable 101");
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
        // The session's default method since 2026-11-13, CONTROLWORD_BIT7: exactly 2B 40 60 00 80 00 00 00,
        // then 2B 40 60 00 00 00 00 00, and no enable word (6, 7, 15), 2301 or save.
        const uint8_t reset[] = {0x2b, 0x40, 0x60, 0, 0x80, 0, 0, 0}, manual[] = {0x2b, 0x40, 0x60, 0, 0, 0, 0, 0};
        CHECK(frameIs(r, first, reset) && frameIs(r, first + 1, manual));
        CHECK(downloadsFrom(r, first, 0x6040) == 2 && wordsFrom(r, first, 0x0080) == 1 &&
              wordsFrom(r, first, 0) == 1);
        CHECK(!wordsFrom(r, first, 6) && !wordsFrom(r, first, 7) && !wordsFrom(r, first, 15));
        CHECK(!downloadsFrom(r, first, 0x2300) && !downloadsFrom(r, first, 0x2301));
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

// Prompt 57's regular console commands on the named fresh report: the measured enable, the typed mode select
// and the release table, each terminal with the unqualified-firmware flag (null where nothing is compared).
L::ActionOptions actionPolicy() {
    L::ActionOptions policy;
    policy.service = reads();
    policy.observationIntervalUs = 1000;
    policy.maximumAgeUs = 10000000; // Console policy for this fake: the named report stays current.
    policy.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    return policy;
}
void regularActionsThroughTheConsole() {
    scenario = "regular enable, mode select and release from the console";
    {
        ConsoleRig r; // The power-up report: a regular enable, flagged because this fake's firmware is its own.
        r.drive.followEnable = true;
        r.readIdentity();
        r.readState(r.state, ENABLE_MASK);
        CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, actionPolicy()));
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@70 enable 101");
        contains(output, "\"result\":\"accepted\"");
        contains(output, "\"firmware_unqualified\":true");
        CHECK(r.motion.normalResult().kind == E::MotionKind::ENABLE &&
              r.motion.normalResult().state == M::ActionState::SUCCEEDED);
        CHECK(r.motion.nativeAction()->enable.evidence.gate == L::EnableGate::MEASURED);
        CHECK(wordsFrom(r, first, 6) == 1 && wordsFrom(r, first, 7) == 1 && wordsFrom(r, first, 15) == 1);
        transcript += output;
    }
    {
        ConsoleRig r; // A not-enabled released report in mode 1: one 6060 write of 3, settled by 6061.
        r.drive.followMode = true;
        r.drive.monitor = uint16_t(L::MONITOR_RELEASED | L::MONITOR_POSITION_COMPLETE);
        r.drive.status = 0x8050;
        r.readIdentity();
        r.readState(r.state, ENABLE_MASK);
        CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, actionPolicy()));
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@71 mode 101 3");
        contains(output, "\"result\":\"accepted\"");
        contains(output, "\"firmware_unqualified\":true");
        CHECK(r.motion.normalResult().kind == E::MotionKind::MODE_SELECT &&
              r.motion.normalResult().state == M::ActionState::SUCCEEDED && r.drive.mode == 3);
        CHECK(downloadsFrom(r, first, 0x6060) == 1 && !downloadsFrom(r, first, 0x6040));
        transcript += output;
        // An undocumented code is the library's refusal, before any frame.
        r.readState(r.state, ENABLE_MASK);
        CHECK(r.motion.releaseNormal());
        CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, actionPolicy()));
        const unsigned before = r.wire.sentCount;
        contains(r.command("@72 mode 101 2"), "\"ok\":false");
        CHECK(r.wire.sentCount == before);
    }
    {
        ConsoleRig r; // The power-up report: release row 1, the one word 0x0006, settled by 8031/0011.
        r.drive.followRelease = true;
        r.readIdentity();
        r.readState(r.state, ENABLE_MASK);
        CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, actionPolicy()));
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@73 motor-release 101");
        contains(output, "\"result\":\"accepted\"");
        contains(output, "\"firmware_unqualified\":true");
        CHECK(r.motion.normalResult().kind == E::MotionKind::RELEASE &&
              r.motion.normalResult().state == M::ActionState::SUCCEEDED);
        const uint8_t word6[] = {0x2b, 0x40, 0x60, 0, 6, 0, 0, 0};
        CHECK(frameIs(r, first, word6) && downloadsFrom(r, first, 0x6040) == 1 && !downloadsFrom(r, first, 0x2300));
        transcript += output;
    }
}
// Fix round c, item 2: with no named report, `enable N`, `mode N M` and `motor-release N` read their own
// (identity, CONTROL for a release, then ERRORS, MONITORS, STATUS and FEEDBACK) and request the action on
// them, as `persistence save parameters` does. A drive with the measured firmware reports
// firmware_unqualified false; a foreign product is refused after the reads with nothing written.
unsigned uploadsOf(const ConsoleRig& r, unsigned first, uint16_t index) {
    unsigned count = 0;
    for (unsigned i = first; i < r.wire.sentCount; ++i)
        count += r.wire.sent[i].data[0] == 0x40 && frameIndex(r.wire.sent[i]) == index;
    return count;
}
void regularCommandsReadTheirOwnReport() {
    scenario = "regular commands read their own report";
    {
        ConsoleRig r; // A not-enabled released report in mode 1, the measured firmware: one 6060 write of 3.
        r.drive.followMode = true;
        r.drive.measuredFirmware = true;
        r.drive.monitor = uint16_t(L::MONITOR_RELEASED | L::MONITOR_POSITION_COMPLETE);
        r.drive.status = 0x8050;
        CHECK(!r.motion.actionReportNamed());
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@80 mode 101 3");
        contains(output, "\"result\":\"accepted\"");
        contains(output, "\"firmware_unqualified\":false");
        CHECK(r.motion.normalResult().kind == E::MotionKind::MODE_SELECT &&
              r.motion.normalResult().state == M::ActionState::SUCCEEDED && r.drive.mode == 3);
        CHECK(uploadsOf(r, first, 0x1000) == 1 && uploadsOf(r, first, 0x2000) == 1 && uploadsOf(r, first, 0x1001) == 1);
        CHECK(downloadsFrom(r, first, 0x6060) == 1 && !downloadsFrom(r, first, 0x6040));
        CHECK(!r.motion.preparing() && r.motion.nativeAction() &&
              r.motion.nativeAction()->modeSelect.identity.outcome == L::IdentityOutcome::CONFIRMED &&
              !r.motion.nativeAction()->modeSelect.identity.firmwareUnqualified);
        transcript += output;
    }
    {
        ConsoleRig r; // The power-up report: the measured enable, words 6, 7 and 15.
        r.drive.followEnable = true;
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@81 enable 101");
        contains(output, "\"result\":\"accepted\"");
        contains(output, "\"firmware_unqualified\":true"); // This fake's own firmware fields.
        CHECK(r.motion.normalResult().kind == E::MotionKind::ENABLE &&
              r.motion.normalResult().state == M::ActionState::SUCCEEDED);
        CHECK(r.motion.nativeAction()->enable.evidence.gate == L::EnableGate::MEASURED);
        CHECK(uploadsOf(r, first, 0x1000) == 1 && wordsFrom(r, first, 6) == 1 && wordsFrom(r, first, 7) == 1 &&
              wordsFrom(r, first, 15) == 1);
        transcript += output;
    }
    {
        ConsoleRig r; // The power-up report: release row 1, the one word 0x0006, after a CONTROL read.
        r.drive.followRelease = true;
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@82 motor-release 101");
        contains(output, "\"result\":\"accepted\"");
        CHECK(r.motion.normalResult().kind == E::MotionKind::RELEASE &&
              r.motion.normalResult().state == M::ActionState::SUCCEEDED);
        CHECK(uploadsOf(r, first, 0x1000) == 1 && uploadsOf(r, first, 0x2500) == 1 &&
              downloadsFrom(r, first, 0x6040) == 1);
        transcript += output;
    }
    {
        ConsoleRig r; // An enabled report: the library refuses the mode select after the reads, nothing written.
        r.drive.status = 0x8437;
        r.drive.monitor = 1;
        const unsigned first = r.wire.sentCount;
        const std::string output = r.command("@83 mode 101 3");
        contains(output, "\"result\":\"accepted\"");
        contains(output, "\"ok\":false");
        contains(output, "\"status\":\"INVALID_CONFIG\"");
        CHECK(r.motion.normalResult().state == M::ActionState::FAILED &&
              r.motion.normalResult().execution == M::ActionExecution::NOT_TRANSMITTED &&
              !r.motion.normalResult().uncertain);
        CHECK(!downloadsFrom(r, first, 0x6060) && !downloadsFrom(r, first, 0x6040));
        // A named report still takes the application path, not the console's reads.
        r.readIdentity();
        r.readState(r.state, ENABLE_MASK);
        CHECK(r.motion.releaseNormal());
        CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, actionPolicy()));
        CHECK(r.motion.actionReportNamed());
        CHECK(!r.motion.startAction(E::MotionKind::MODE_SELECT, 3, reads(), actionPolicy(), r.now, r.now + 1000000));
    }
}
// With an axis and a retained operation sharing one numeric ID, `release ID` frees the result and sends no
// CAN frame, and `motor-release ID` acts only on the axis: the console never infers one from the other.
void releaseAndMotorReleaseNeverMix() {
    scenario = "release ID and motor-release ID";
    ConsoleRig r;
    r.drive.monitor = uint16_t(L::MONITOR_RELEASED | L::MONITOR_POSITION_COMPLETE);
    r.drive.status = 0x8050;
    r.readIdentity();
    r.readState(r.state, ENABLE_MASK);
    // Operations 1..100: an already released report answers each release with nothing sent.
    const unsigned baseline = r.wire.sentCount;
    for (unsigned n = 1; n <= 100; ++n) {
        CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, actionPolicy()));
        contains(r.command("@" + std::to_string(1000 + n) + " motor-release 101"), "\"result\":\"accepted\"");
        contains(r.command("@" + std::to_string(2000 + n) + " release " + std::to_string(n)), "\"ok\":true");
    }
    CHECK(r.wire.sentCount == baseline);
    // Operation 101 is an ordinary identity read, retained; axis 101 is the motion's axis.
    contains(r.command("@3101 read identity"), "\"operation_id\":101");
    const unsigned reads = r.wire.sentCount;
    r.drive.monitor = 1;
    r.drive.status = IDLE_STATUS;
    r.drive.followRelease = true;
    r.readState(r.state, ENABLE_MASK);
    CHECK(r.motion.setActionPrerequisites(r.state, r.identity, nullptr, actionPolicy()));
    const unsigned first = r.wire.sentCount;
    const std::string released = r.command("@3102 motor-release 101");
    contains(released, "\"operation_id\":102");
    CHECK(downloadsFrom(r, first, 0x6040) == 1 && r.motion.normalResult().kind == E::MotionKind::RELEASE);
    contains(r.command("@3103 result 101"), "\"ok\":true"); // The read stays retained.
    const unsigned beforeRelease = r.wire.sentCount;
    contains(r.command("@3104 release 101"), "\"ok\":true");
    CHECK(r.wire.sentCount == beforeRelease); // No CAN frame.
    CHECK(r.motion.retained() && r.motion.normalResult().kind == E::MotionKind::RELEASE); // Untouched.
    CHECK(reads > 0);
}

int main(int argc, char** argv) {
    if (argc != 1 && (argc != 3 || std::strcmp(argv[1], "--transcript")))
        return 2;
    retainedNativeMove();
    startupAndRejectedGrammar();
    consoleMoveRoutes();
    simpleIntentNeverUsesDefaults();
    accelerationFormsGiveTheNativeTraffic();
    accelerationFormRefusalsBeforeTraffic();
    accelerationTextFormAndStaleness();
    rpmDestinationIsCheckedBeforeAdmission();
    stopNeverReplacesUnknownMotion();
    congestedEscalationKeepsIndependentResults();
    urgentStopAtEveryPhaseUnderPressure();
    cancelSettlesTheAcceptedDownload();
    deviceZeroNeverSavesOrSetsAHostOrigin();
    explicitEnableAndAlarmClear();
    regularCommandsReadTheirOwnReport();
    liveHostChangesFollowOnlyTheirOwnEpoch();
    replacedNativeContextCannotBeCancelledOrReleased();
    lookupReleaseAndFrozenOutputMode();
    unsentStopRemainsActiveAcrossOneExplicitRecovery();
    replacedNativeStopKeepsItsOwnChainIdentity();
    simpleCoordinatesPreserveMotorAndLoadMeaning();
    regularActionsThroughTheConsole();
    releaseAndMotorReleaseNeverMix();
    if (argc == 3) {
        FILE* output = std::fopen(argv[2], "wb");
        CHECK(output);
        CHECK(std::fwrite(transcript.data(), 1, transcript.size(), output) == transcript.size());
        CHECK(!std::fclose(output));
    } else
        std::printf("motion_console_test: %u checks; HOST_FAKE only\n", checks);
}
