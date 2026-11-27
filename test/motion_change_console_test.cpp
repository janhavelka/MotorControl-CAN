// SPDX-License-Identifier: MIT
// Shared console over the actual app/owner/session; all drive behavior and records are synthetic.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)
#include "ProbeConsole.h"
#include "ProbeMotion.h"
#include "support/VelocityRig.h"
namespace {
using namespace VelocityRig;
struct Bytes {
    std::string output;
    static bool write(void* context, uint8_t byte) {
        static_cast<Bytes*>(context)->output += char(byte);
        return true;
    }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks value;
        value.context = this;
        value.writeByte = write;
        return value;
    }
};
void contains(const std::string& value, const char* text) {
    if (value.find(text) == std::string::npos)
        std::fprintf(stderr, "Missing [%s] in %s\n", text, value.c_str());
    CHECK(value.find(text) != std::string::npos);
}
struct ConsoleRig : Rig {
    Bytes bytes;
    E::ProbePlatform platform;
    E::ProbeApp app;
    L::ReadTarget live = target();
    M::AxisReference reference;
    E::ProbeMotion motion;
    E::ProbeConsole console;
    ConsoleRig(bool changeOpen = true)
        : platform(bytes.callbacks()), app(owner, platform),
          motion(app, owner, session, live, axis, reference), console(app, owner, platform) {
        serviceSession = false;
        CHECK(app.select(live));
        CHECK(app.openAxis(session, live, L::SettingsModel::CL86_C, now) == &session);
        const uint32_t operation = session.beginOperation(now + DEADLINE, now);
        CHECK(operation);
        CHECK(L::prepareIdentity(identityRead, target(), operation, now, now + DEADLINE, reads()));
        read(identityRead);
        CHECK(L::getIdentity(identityRead, target(), now, DEADLINE, identity));
        readState();
        CHECK(console.attachMotion(motion));
        CHECK(motion.setVelocityPrerequisites(state, identity, evidence(), profile(), options()));
        if (changeOpen) {
            L::VelocityChangeEvidence proof;
            proof.gate = L::VelocityGate::MEASURED;
            proof.record = 5201;
            proof.model = L::SettingsModel::CL86_C;
            proof.firmware = evidence().firmware;
            proof.rateChangeQualified = true;
            proof.maximumForwardRpm = 300;
            CHECK(motion.setVelocityChangePrerequisites(proof));
        }
    }
    void tick() {
        now += TICK;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (wire.pending[served])
                break;
            const M::Frame& sent = wire.sent[served];
            M::Frame response;
            const bool reply = drive.respond(sent, response);
            if (indexOf(sent) == 0x60ff && sent.data[0] != 0x40 &&
                (drive.monitor & L::MONITOR_RUNNING))
                drive.velocity = drive.targetRpm;
            if (reply)
                wire.push(response, now, owner.controllerEpoch());
        }
        CHECK(app.service(now));
        console.service(now);
    }
    void line(const char* text) {
        for (; *text; ++text)
            console.feed(uint8_t(*text), now);
        console.feed('\n', now);
    }
    void start() {
        startFrame = wire.sentCount;
        line("@1 velocity 120 rpm native 120 configured normal");
        for (unsigned n = 0; n < 200 && !session.velocityAction().runningObserved; ++n)
            tick();
        CHECK(session.velocityAction().runningObserved && motion.normalResult().state == M::ActionState::ACTIVE);
    }
    void finish() {
        for (unsigned n = 0; n < 500; ++n)
            tick();
        CHECK(motion.normalResult().state != M::ActionState::ACTIVE && console.drained());
    }
};
void explicitVelocityChangeAndRetention() {
    ConsoleRig r;
    r.start();
    const uint64_t due = r.session.velocityAction().stopDueUs;
    const uint64_t deadline = r.session.velocityAction().deadlineUs;
    r.line("@2 velocity change 1 3 turns/s motor configured");
    CHECK(r.session.velocityAction().change.present);
    r.line("@3 velocity change 1 60 rpm native configured"); // One retained change, never overwrite.
    r.finish();
    contains(r.bytes.output, "\"id\":2,\"command\":\"velocity\"");
    contains(r.bytes.output, "\"operation_id\":1,\"change_index\":1");
    contains(r.bytes.output, "\"type\":\"velocity_change\"");
    contains(r.bytes.output, "\"native_rpm\":\"180\"");
    contains(r.bytes.output, "\"exact\":true,\"error\":[\"0\",\"1\",false]");
    contains(r.bytes.output, "\"report_present\":true,\"qualified\":false");
    contains(r.bytes.output, "\"id\":3,\"command\":\"velocity\",\"ok\":false");
    CHECK(r.writes(0x60ff, 120) == 1 && r.writes(0x60ff, 180) == 1 && !r.writes(0x60ff, 60));
    CHECK(r.writes(0x6040, 0x010f) == 1);
    CHECK(r.session.velocityAction().stopDueUs == due && r.session.velocityAction().deadlineUs == deadline);
    CHECK(r.motion.normalResult().rate.native == 120);
    CHECK(r.session.velocityAction().change.firstRequestedRateReport.present);
}
void approximateChangePublishesItsBound() {
    ConsoleRig r;
    r.start();
    r.line("@2 velocity change 1 1 rad/s motor configured round nearest 1 approx 0.001");
    CHECK(r.session.velocityAction().change.present &&
          !r.session.velocityAction().change.rate.exactArithmetic &&
          r.session.velocityAction().change.rate.absoluteErrorBound > 0);
    r.finish();
    const size_t begin = r.bytes.output.find("\"type\":\"velocity_change\"");
    CHECK(begin != std::string::npos);
    const size_t end = r.bytes.output.find('\n', begin);
    const std::string record = r.bytes.output.substr(begin, end - begin);
    contains(record, "\"exact\":false,\"error_bound\":\"");
    CHECK(record.find("\"error\":") == std::string::npos);
    CHECK(r.writes(0x60ff, 10) == 1 && r.writes(0x6040, 0x010f) == 1);
}
void closedAndInvalidChangesSendNothing() {
    for (unsigned test = 0; test < 7; ++test) {
        ConsoleRig r(test != 0);
        r.start();
        const char* cases[] = {
            "@2 velocity change 1 180 rpm native configured",
            "@2 velocity change 99 180 rpm native configured",
            "@2 velocity change 1 0 rpm native configured",
            "@2 velocity change 1 -60 rpm native configured",
            "@2 velocity change 1 180 rpm native",
            "@2 velocity change 1 180 rpm native configured extra",
            "@2 velocity change 1 180 rpm native configured round exact 0 extra"
        };
        r.line(cases[test]);
        CHECK(!r.session.velocityAction().change.present);
        r.finish();
        contains(r.bytes.output, "\"id\":2,\"command\":\"velocity\",\"ok\":false");
        CHECK(r.writes(0x60ff, 120) == 1 && !r.writes(0x60ff, 180));
        CHECK(r.writes(0x6040, 0x010f) == 1);
    }
}
} // namespace
// Prompt 60: `accel` in a velocity command fills both ramps from one acceleration through the library
// (6083 = 6084, the measured condition), giving the frames of the same milliseconds set natively.
std::string velocityFrames(ConsoleRig& r, const char* command, std::string& output) {
    const unsigned first = r.wire.sentCount;
    const size_t start = r.bytes.output.size();
    r.line(command);
    r.finish();
    output = r.bytes.output.substr(start);
    std::string bytes;
    for (unsigned i = first; i < r.wire.sentCount; ++i) {
        const M::Frame& f = r.wire.sent[i];
        bytes.append(reinterpret_cast<const char*>(&f.id), sizeof f.id);
        bytes += char(f.dlc);
        bytes.append(reinterpret_cast<const char*>(f.data), sizeof f.data);
    }
    return bytes;
}
void velocityAccelerationForm() {
    std::string output;
    ConsoleRig native(false);
    L::VelocityProfile equal;
    equal.rampUpTimeMs = equal.rampDownTimeMs = 200;
    CHECK(native.motion.setVelocityPrerequisites(native.state, native.identity, evidence(), equal, options()));
    const std::string expected = velocityFrames(native, "@1 velocity 120 rpm native 120 configured normal", output);
    CHECK(native.writes(0x6083, 200) == 1 && native.writes(0x6084, 200) == 1);
    // The named profile is 100/200 ms here; 120 rpm at 600 rpm/s is 200 ms for both ramps.
    ConsoleRig units(false);
    CHECK(velocityFrames(units, "@1 velocity 120 rpm native 120 accel 600 rpm/s native configured normal", output) ==
          expected);
    contains(output, "\"type\":\"ramp_value\",\"id\":1,\"command\":\"velocity\",\"operation_id\":1,"
                     "\"field\":\"ramps\",\"unit\":\"ms\",\"native\":\"200\"");
    contains(output, "\"speed_rpm\":\"120\",\"acceleration_unit\":\"rpm/s\",\"frame\":\"native\"");
    // A separate deceleration, a time below the measured 200 ms and a rate outside the law refuse
    // before any frame; the run without a clause keeps the named profile.
    ConsoleRig refused(false);
    const unsigned first = refused.wire.sentCount;
    const char* commands[] = {"@2 velocity 120 rpm native 120 decel 600 rpm/s native configured normal",
                              "@3 velocity 120 rpm native 120 accel 1200 rpm/s native configured normal",
                              "@4 velocity 5 rpm native 120 accel 50 rpm/s native configured normal",
                              "@5 velocity 120 rpm native 120 accel 600 rpm/s native normal configured"};
    const char* reasons[] = {"only with 6083 = 6084", "\"status\":\"ILLEGAL_VALUE\"",
                             "measured at 6 to 4500 rpm", "\"ok\":false"};
    for (unsigned i = 0; i < 4; ++i) {
        const size_t start = refused.bytes.output.size();
        refused.line(commands[i]);
        for (unsigned n = 0; n < 50; ++n)
            refused.tick();
        const std::string reply = refused.bytes.output.substr(start);
        contains(reply, "\"ok\":false");
        contains(reply, reasons[i]);
        CHECK(reply.find("ramp_value") == std::string::npos);
        CHECK(refused.wire.sentCount == first && !refused.motion.retained());
    }
}

int main() {
    explicitVelocityChangeAndRetention();
    approximateChangePublishesItsBound();
    closedAndInvalidChangesSendNothing();
    velocityAccelerationForm();
    std::printf("motion change console: %u checks\n", checks);
}
