// SPDX-License-Identifier: MIT
// HOST_FAKE: shared console/ProbeMotion over the actual owner, SDO engine and AxisSession.
// Qualification and server reports are synthetic; none qualify a drive or a physical movement.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static unsigned checks = 0;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "ProbeConsole.h"
#include "ProbeMotion.h"
#include "support/VelocityRig.h"

namespace {
using namespace VelocityRig;
struct Bytes {
    std::string text;
    static bool write(void* context, uint8_t value) {
        static_cast<Bytes*>(context)->text += char(value);
        return true;
    }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks c;
        c.context = this;
        c.writeByte = write;
        return c;
    }
};
void contains(const std::string& value, const char* expected) {
    if (value.find(expected) == std::string::npos)
        std::fprintf(stderr, "Missing [%s] in %s\n", expected, value.c_str());
    CHECK(value.find(expected) != std::string::npos);
}
struct ConsoleRig : Rig {
    Bytes bytes;
    E::ProbePlatform platform;
    E::ProbeApp app;
    L::ReadTarget live;
    M::AxisReference reference;
    E::ProbeMotion motion;
    E::ProbeConsole console;
    L::DriverContext inputs, driver;
    bool home = false, triggered = false, muteTrigger = false;
    unsigned homeCycles = 0;
    ConsoleRig()
        : platform(bytes.callbacks()), app(owner, platform), live(target()),
          motion(app, owner, session, live, axis, reference), console(app, owner, platform) {
        serviceSession = false;
        CHECK(app.select(live));
        CHECK(app.openAxis(session, live, L::SettingsModel::CL86_C, now) == &session);
        CHECK(console.attachMotion(motion));
        CHECK(wire.sentCount == 0);
    }
    // Reuse the velocity server for transport, identity and ordinary observation fields. This
    // finite extension supplies only the method-29 waveform already used by homing_owner_test.
    bool answer(const M::Frame& sent, M::Frame& reply) {
        const uint16_t index = indexOf(sent);
        const unsigned sub = sent.data[3];
        if (!home)
            return drive.respond(sent, reply);
        const bool download = sent.data[0] != 0x40;
        bool custom = false;
        uint32_t raw = 0;
        if (download) {
            if (index == 0x6040) {
                const uint32_t word = valueOf(sent);
                CHECK(sent.data[0] == 0x2b && sub == 0 && (word == 15 || word == 31));
                if (word == 31) {
                    triggered = true;
                    homeCycles = 0;
                    if (muteTrigger)
                        return false;
                }
            } else {
                CHECK(sent.data[0] == (index == 0x6098 ? 0x2f : 0x23));
                CHECK((index == 0x6098 && sub == 0 && valueOf(sent) == 29) ||
                      (index == 0x6099 && sub == 1 && valueOf(sent) == 120) ||
                      (index == 0x6099 && sub == 2 && valueOf(sent) == 60) ||
                      (index == 0x609a && sub == 0 && valueOf(sent) == 100) ||
                      (index == 0x607c && sub == 0 && valueOf(sent) == 0));
            }
            custom = true;
        } else if (index == 0x2400) {
            raw = !sub ? 16 : sub == 2 ? 1 : 0; // DI0 origin function; other configuration zero.
            custom = true;
        } else if (index == 0x2200 || index == 0x2201) {
            raw = index == 0x2201 ? 3200 : 0;
            custom = true;
        } else if (index == 0x2101) {
            raw = triggered && homeCycles < 3 ? 1 : 0;
            custom = true;
        } else if (index == 0x2100 && triggered) {
            ++homeCycles;
            drive.monitor = homeCycles < 3 ? L::MONITOR_RUNNING : L::MONITOR_HOMING_COMPLETE;
            drive.status = homeCycles < 3 ? 7 : 0x107;
            drive.velocity = homeCycles < 3 ? 120 : 0;
        }
        if (!custom)
            return drive.respond(sent, reply);
        reply = frame(sent.id - 0x600 + 0x580, 8);
        reply.data[0] = download ? 0x60 : 0x4b;
        reply.data[1] = sent.data[1];
        reply.data[2] = sent.data[2];
        reply.data[3] = sent.data[3];
        for (unsigned i = 0; i < 4; ++i)
            reply.data[4 + i] = uint8_t(raw >> (8 * i));
        return true;
    }
    void tick() {
        now += TICK;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (wire.pending[served])
                break;
            M::Frame reply;
            if (answer(wire.sent[served], reply))
                wire.push(reply, now, owner.controllerEpoch());
        }
        CHECK(app.service(now));
        CHECK(motion.service(now));
        console.service(now);
    }
    void acquire(L::ReadContext& into) {
        for (unsigned i = 0; into.state == M::ReadState::ACTIVE && i < 64; ++i) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            CHECK(session.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            tick();
            tick();
            C::SdoCompletion terminal;
            CHECK(session.settle(terminal) && L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    void acquiredIdentityAndState() {
        uint32_t operation = session.beginOperation(now + DEADLINE, now);
        CHECK(operation &&
              L::prepareIdentity(identityRead, live, operation, now, now + DEADLINE, reads()));
        acquire(identityRead);
        CHECK(L::getIdentity(identityRead, live, now, DEADLINE, identity));
        operation = session.beginOperation(now + DEADLINE, now);
        const uint8_t mask = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                     (1u << unsigned(L::StateBlock::STATUS)) |
                                     (1u << unsigned(L::StateBlock::FEEDBACK)));
        CHECK(operation &&
              L::prepareState(state, live, operation, now, now + DEADLINE, reads(), mask));
        acquire(state);
    }
    std::string command(const char* line) {
        const size_t first = bytes.text.size();
        for (const char* p = line; *p; ++p)
            console.feed(uint8_t(*p), now);
        console.feed('\n', now);
        for (unsigned i = 0; i < 2000; ++i) {
            tick();
            if (console.drained() && !platform.stats().outputUsed)
                return bytes.text.substr(first);
        }
        CHECK(false);
        return std::string();
    }
    void finish() {
        for (unsigned i = 0; session.busy() && i < 1500; ++i)
            tick();
        CHECK(!session.busy());
        for (unsigned i = 0; i < 300; ++i)
            tick();
    }
    void armVelocity(bool reverse = false) {
        acquiredIdentityAndState();
        CHECK(motion.setVelocityPrerequisites(state, identity, evidence(reverse), profile(),
                                              options()));
        startFrame = wire.sentCount;
    }
    void armHome() {
        home = true;
        drive.mode = 6;
        drive.status = 7;
        drive.monitor = 0;
        uint32_t op = session.beginOperation(now + DEADLINE, now);
        C::SdoTransfer word;
        word.channel = live.channel;
        word.operation = word.request = op;
        word.step = 1;
        word.index = 0x6040;
        word.size = 2;
        word.direction = C::SdoDirection::DOWNLOAD;
        word.data[0] = 15;
        word.deadlineUs = now + DEADLINE;
        word.queueTimeoutUs = word.responseTimeoutUs = 1000;
        CHECK(op && session.admitPrepared(word, now) == O::BusAdmission::ACCEPTED);
        tick();
        tick();
        C::SdoCompletion terminal;
        CHECK(session.settle(terminal));
        for (unsigned i = 0; i < 2; ++i) {
            L::DriverContext& context = i ? driver : inputs;
            op = session.beginOperation(now + DEADLINE, now);
            CHECK(op && L::prepareDriverRead(context, live, op, now, now + DEADLINE, reads(),
                                             i ? L::DriverGroup::DRIVER
                                               : L::DriverGroup::IO_INPUT_CONFIGURATION));
            acquire(context.read);
        }
        acquiredIdentityAndState();
        L::HomeEvidence proof;
        proof.gate = L::HomeGate::MEASURED;
        proof.record = 361;
        proof.model = L::SettingsModel::CL86_C;
        proof.specimen = live;
        proof.method = L::HomingMethod::NEGATIVE_ORIGIN;
        proof.layout = L::StatusLayout::ZH86;
        proof.searchSpeedRpm = 120;
        proof.creepSpeedRpm = 60;
        proof.rampTimeMs = 100;
        proof.inputFunctions[0] = proof.wiredInputs = 1;
        proof.methodQualified = proof.parametersQualified = proof.inputsQualified =
            proof.referenceQualified = true;
        const L::VelocityFirmware fw = evidence().firmware;
        proof.firmware.deviceType = fw.deviceType;
        proof.firmware.hardwareVersion = fw.hardwareVersion;
        proof.firmware.softwareVersion = fw.softwareVersion;
        proof.firmware.vendorId = fw.vendorId;
        proof.firmware.productCode = fw.productCode;
        proof.firmware.revision = fw.revision;
        proof.firmware.modelCode = fw.modelCode;
        proof.firmware.firmwareCode = fw.firmwareCode;
        L::HomeOptions policy;
        policy.service = reads();
        policy.pollIntervalUs = 10000;
        policy.maxPolls = 10;
        policy.maximumAgeUs = DEADLINE;
        CHECK(motion.setHomePrerequisites(state, identity, inputs, driver, proof, policy));
        startFrame = wire.sentCount;
    }
};
void finiteVelocity() {
    scenario = "finite velocity console native payload and retained terminal";
    for (unsigned reverse = 0; reverse < 2; ++reverse) {
        ConsoleRig r;
        r.armVelocity(reverse != 0);
        contains(r.command(reverse ? "@51 velocity -120 rpm native 120 configured normal"
                                   : "@51 velocity 120 rpm native 120 configured normal"),
                 "\"ok\":true");
        const uint64_t prepared = r.session.velocityAction().startedUs;
        r.finish();
        const E::MotionResult result = r.motion.normalResult();
        CHECK(result.kind == E::MotionKind::VELOCITY && result.state == M::ActionState::SUCCEEDED);
        CHECK(result.execution == M::ActionExecution::ACKNOWLEDGED &&
              result.completion == M::ActionCompletion::OBSERVED);
        CHECK(!result.uncertain && result.rate.native == (reverse ? -120 : 120));
        CHECK(result.rate.rateBasis == M::NativeRateBasis::MOTOR_TURNS &&
              result.rate.rateTime == M::TimeUnit::MINUTE);
        CHECK(r.session.velocityAction().stopDueUs == prepared + 120000);
        CHECK(r.writes(0x60ff, reverse ? uint32_t(-120) : 120) == 1);
        CHECK(r.writes(0x6083, 100) == 1 && r.writes(0x6084, 200) == 1);
        CHECK(r.writes(0x6040, 6) == 1 && r.writes(0x6040, 7) == 1 && r.writes(0x6040, 15) == 1);
        CHECK(r.writes(0x6040, 0x010f) == 1 && !r.writes(0x6081, 0, false) &&
              !r.writes(0x2300, 0, false));
        for (unsigned i = r.startFrame; i < r.wire.sentCount; ++i) {
            const M::Frame& f = r.wire.sent[i];
            if (f.data[0] != 0x40) {
                CHECK(f.data[3] == 0);
                CHECK(f.data[0] == (indexOf(f) == 0x6040 ? 0x2b : 0x23));
            }
        }
        const unsigned sent = r.wire.sentCount;
        contains(r.command("@52 result 1"), "\"ok\":true");
        CHECK(r.wire.sentCount == sent);
        contains(r.command("@53 release 1"), "\"ok\":true");
        CHECK(!r.motion.retained());
    }
}
void velocityRejectionAndLostReply() {
    scenario = "velocity preflight, stale source and unconfirmed write";
    for (unsigned item = 0; item < 4; ++item) {
        ConsoleRig r;
        r.armVelocity();
        if (item == 1)
            ++r.state.target.channel.binding.configurationGeneration;
        if (item == 2)
            r.now += DEADLINE + 1;
        const unsigned sent = r.wire.sentCount;
        if (item < 3) {
            contains(r.command(item == 0 ? "@61 velocity -120 rpm native 120 configured normal"
                                         : "@61 velocity 120 rpm native 120 configured normal"),
                     "\"ok\":false");
            CHECK(!r.motion.retained() && r.wire.sentCount == sent);
        } else {
            r.drive.muteIndex = 0x60ff;
            contains(r.command("@62 velocity 120 rpm native 120 configured normal"), "\"ok\":true");
            r.finish();
            const E::MotionResult result = r.motion.normalResult();
            CHECK(result.state == M::ActionState::FAILED && result.uncertain);
            CHECK(r.writes(0x60ff, 120) == 1 && !r.writes(0x6040, 15));
            const unsigned terminalSent = r.wire.sentCount;
            contains(r.command("@63 result 1"), "\"uncertain\":true");
            CHECK(r.wire.sentCount == terminalSent);
        }
    }
}
void typedReadRoutesKeepTheOrdinaryGate() {
    scenario = "velocity/home reads use the established Q20 whole-plan gate";
    ConsoleRig r;
    const unsigned before = r.wire.sentCount;
    contains(r.command("@81 velocity read"), "Q20");
    contains(r.command("@82 home read"), "Q20");
    CHECK(r.wire.sentCount == before && !r.motion.retained());
}
void homing() {
    scenario = "homing method29 console route, source gates and terminal evidence";
    for (unsigned lost = 0; lost < 2; ++lost) {
        ConsoleRig r;
        r.armHome();
        const unsigned before = r.wire.sentCount;
        contains(r.command("@71 home 17 120 60 100 zero"), "\"ok\":false");
        CHECK(!r.motion.retained() && r.wire.sentCount == before);
        r.muteTrigger = lost != 0;
        contains(r.command("@72 home 29 120 60 100 zero"), "\"ok\":true");
        r.finish();
        const E::MotionResult result = r.motion.normalResult();
        CHECK(result.kind == E::MotionKind::HOME && result.rate.native == 120 &&
              result.creepRate.native == 60);
        CHECK(r.writes(0x6098, 29) == 1 && r.writes(0x6099, 120) == 1 && r.writes(0x6099, 60) == 1);
        CHECK(r.writes(0x609a, 100) == 1 && r.writes(0x607c, 0) == 1 && r.writes(0x6040, 31) == 1);
        CHECK(!r.writes(0x6040, 15) && !r.writes(0x2300, 0, false) && !r.writes(0x6060, 0, false));
        CHECK(r.axis.binding.configurationGeneration == 8 &&
              r.live.channel.binding.configurationGeneration == 8);
        if (lost)
            CHECK(result.state == M::ActionState::FAILED && result.uncertain &&
                  result.completion == M::ActionCompletion::NOT_OBSERVED);
        else {
            CHECK(result.state == M::ActionState::SUCCEEDED && !result.uncertain &&
                  result.completion == M::ActionCompletion::OBSERVED);
            CHECK(r.session.homeAction().requiredInputObserved &&
                  r.session.homeAction().activity.present);
        }
        const unsigned sent = r.wire.sentCount;
        contains(r.command("@73 result 1"), lost ? "\"uncertain\":true" : "\"ok\":true");
        CHECK(r.wire.sentCount == sent);
    }
}
} // namespace
int main() {
    finiteVelocity();
    velocityRejectionAndLostReply();
    homing();
    typedReadRoutesKeepTheOrdinaryGate();
    std::printf("probe_motion_actions_test: %u checks passed\n", checks);
}
