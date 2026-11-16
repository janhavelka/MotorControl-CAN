// SPDX-License-Identifier: MIT
// Synthetic set-point behavior exercises shared console/owner retention, never drive qualification.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "move change console";
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario); std::abort(); } } while (false)
#include "ProbeConsole.h"
#include "ProbeMotion.h"
#include "support/MoveRig.h"
namespace {
using namespace MoveRig;
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
    L::ReadTarget live;
    E::ProbeMotion motion;
    E::ProbeConsole console;
    uint32_t clearAbortCode = 0;
    explicit ConsoleRig(bool gate = true, bool feedback = true)
        : platform(bytes.callbacks()), app(owner, platform), live(current()),
          motion(app, owner, a, live, one.coordinates, one.witness), console(app, owner, platform) {
        serviceSession = serviceTwo = false;
        CHECK(app.select(live));
        CHECK(app.openAxis(a, live, L::SettingsModel::CL86_C, now) == &a);
        CHECK(console.attachMotion(motion));
        readIdentity();
        writeThrough(0x6040, 2, 0x000f);
        readState(state);
        one.coordinates.originKnown = true;
        one.coordinates.originSource = M::ScaleSource::QUALIFIED;
        one.witness = witness(*this, drive.position);
        CHECK(motion.setMovePrerequisites(state, identity, evidence(), profile(), nullptr, options()));
        if (gate) {
            L::MoveReplacementEvidence proof;
            proof.gate = L::MoveChangeGate::MEASURED;
            proof.record = 5202;
            proof.model = L::SettingsModel::CL86_C;
            proof.firmware = evidence().firmware;
            proof.parameterMask = 15;
            proof.fallingEdgeQualified = proof.immediateQualified = proof.bufferedQualified = true;
            proof.completionQualified = proof.stopQualified = true;
            L::MoveFeedbackRelation relation;
            relation.record = 5203;
            relation.binding = one.coordinates.binding;
            relation.nodeGeneration = live.nodeGeneration;
            relation.referenceGeneration = one.coordinates.referenceGeneration;
            relation.sameIncrement = true;
            CHECK(motion.setMoveReplacementPrerequisites(proof, feedback ? &relation : nullptr));
        }
        drive.travelReads = 100;
        drive.velocity = 30;
    }
    void tick() {
        now += TICK;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (holdTx && wire.pending[served])
                break;
            const M::Frame& sent = wire.sent[served];
            const bool word = frameIndex(sent) == 0x6040 && sent.data[0] != 0x40;
            const uint32_t value = frameValue(sent);
            if (word && value == 0x000f && drive.lastWord == 0x003f)
                drive.lastWord = 0x001f;
            M::Frame response;
            const bool reply = replyTo(sent, drive, response);
            if (reply && word && value == 0x000f && response.data[0] == 0x80 && clearAbortCode)
                for (unsigned byte = 0; byte != 4; ++byte)
                    response.data[4 + byte] = uint8_t(clearAbortCode >> (8 * byte));
            if (word && (value == 0x003f || (value == 0x001f && drive.edges > 1))) {
                if (value == 0x003f) {
                    drive.lastWord = 0x000f;
                    drive.word(0x001f); // Manufactured immediate target adoption for this fixture only.
                }
                drive.lastWord = uint16_t(value);
                drive.remaining = 2;
            }
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
        line("@10 accel 100 ms");
        line("@11 decel 200 ms");
        drain(12);
        line("@1 move absolute 16000 steps native 30 configured");
        for (unsigned n = 0; n < 200 && !a.moveAction().latest.present; ++n)
            tick();
        if (!a.moveAction().acceptance.present)
            std::fprintf(stderr, "start output: %s native=%s\n", bytes.output.c_str(), a.moveAction().status.msg);
        CHECK(a.moveAction().acceptance.present && a.moveAction().phase == L::MovePhase::WAITING);
        CHECK(motion.normalResult().state == M::ActionState::ACTIVE);
    }
    void drain(unsigned count = 200) {
        for (unsigned n = 0; n < count; ++n)
            tick();
    }
    std::string command(const char* text) {
        const size_t begin = bytes.output.size();
        line(text);
        drain(35);
        return bytes.output.substr(begin);
    }
};
void replacementRoutesRetainBothResults() {
    const char* commands[] = {
        "@2 move speed 1 60",
        "@2 move speed 1 60 buffered",
        "@2 move ramps 1 300 400 immediate",
        "@2 move retarget 1 absolute 32000 steps native 30 configured",
        "@2 move retarget 1 buffered absolute 32000 steps native 30 configured"
    };
    for (unsigned row = 0; row < 5; ++row) {
        ConsoleRig r;
        r.start();
        const uint64_t deadline = r.a.moveAction().deadlineUs;
        const uint32_t oldHelper = r.motion.normalResult().id;
        const unsigned first = r.wire.sentCount;
        r.line(commands[row]);
        const uint32_t replacement = r.motion.normalResult().id;
        CHECK(replacement != oldHelper && r.motion.normalResult(oldHelper));
        CHECK(r.a.originalMoveAction().interruption == M::MoveInterruption::REPLACED);
        CHECK(r.motion.normalResult(oldHelper)->state == M::ActionState::FAILED);
        CHECK(r.motion.normalResult(oldHelper)->relation.observationSuperseded);
        CHECK(r.motion.normalResult().relation.originalOperationId == oldHelper);
        CHECK(r.motion.normalResult().relation.replacementOperationId == replacement);
        const Saved<E::MotionResult> original(*r.motion.normalResult(oldHelper));
        r.drain();
        original.unchanged(*r.motion.normalResult(oldHelper));
        if (r.a.moveAction().state != M::ActionState::SUCCEEDED)
            std::fprintf(stderr, "row=%u phase=%u state=%u outcome=%u status=%s\n%s\n", row,
                         unsigned(r.a.moveAction().phase), unsigned(r.a.moveAction().state),
                         unsigned(r.a.moveAction().outcome), r.a.moveAction().status.msg, r.bytes.output.c_str());
        if (r.a.moveAction().state != M::ActionState::SUCCEEDED)
            std::fprintf(stderr, "now=%llu due=%llu words=%u word=%x remaining=%u obs=%lu trigger=%u accept=%u complete=%u sent=%u served=%u\n",
                         static_cast<unsigned long long>(r.now), static_cast<unsigned long long>(deadline),
                         r.drive.words, r.drive.lastWord, r.drive.remaining,
                         static_cast<unsigned long>(r.a.moveAction().observations), unsigned(r.a.moveAction().triggerPresent),
                         unsigned(r.a.moveAction().acceptance.present), unsigned(r.a.moveAction().completed.present),
                         r.wire.sentCount, r.served);
        CHECK(r.a.moveAction().state == M::ActionState::SUCCEEDED);
        CHECK(r.a.moveAction().deadlineUs == deadline);
        contains(r.bytes.output, "\"original\":1,\"replacement\":2,\"observation_superseded\":true");
        contains(r.bytes.output, "\"physical_interruption_observed\":false");
        CHECK(wordsFrom(r, first, row == 1 || row == 4 ? 0x001f : 0x003f) == 1);
        CHECK(wordsFrom(r, first, 6) == 0 && wordsFrom(r, first, 7) == 0);
        CHECK(downloadsFrom(r, first, 0x6060) == 0 && downloadsFrom(r, first, 0x2300) == 0);
        if (row < 2)
            CHECK(downloadsFrom(r, first, 0x6081) == 1 && !downloadsFrom(r, first, 0x607a));
        if (row == 2)
            CHECK(downloadsFrom(r, first, 0x6083) == 1 && downloadsFrom(r, first, 0x6084) == 1 &&
                  !downloadsFrom(r, first, 0x6081));
        if (row >= 3)
            CHECK(downloadsFrom(r, first, 0x607a) == 1);
        contains(r.command("@3 result 1"), "\"operation_id\":1");
        contains(r.command("@4 result 2"), "\"operation_id\":2");
        contains(r.command("@5 release 2"), "\"ok\":false");
        contains(r.command("@6 release 1"), "\"ok\":true");
        CHECK(!r.motion.normalResult(oldHelper));
        contains(r.command("@7 release 2"), "\"ok\":true");
        CHECK(!r.motion.retained());
    }
}
void closedWrongAndUnmappedChangesRefuseBeforeWrites() {
    for (unsigned row = 0; row < 7; ++row) {
        ConsoleRig r(row != 0, row != 1);
        r.start();
        const char* commands[] = {
            "@2 move speed 1 60", "@2 move retarget 1 absolute 32000 steps native 30 configured",
            "@2 move speed 99 60", "@2 move speed 1 0", "@2 move ramps 1 0 100",
            "@2 move retarget 1 relative 100 steps native 30 configured",
            "@2 move speed 1 60 extra"
        };
        const unsigned first = r.wire.sentCount;
        const uint32_t helper = r.motion.normalResult().id;
        r.line(commands[row]);
        CHECK(r.motion.normalResult().id == helper && !r.a.moveAction().relation.originalOperationId);
        r.drain(35);
        contains(r.bytes.output, "\"id\":2,\"command\":\"move\",\"ok\":false");
        CHECK(downloadsFrom(r, first, 0x607a) == 0 && downloadsFrom(r, first, 0x6081) == 0 &&
              downloadsFrom(r, first, 0x6083) == 0 && downloadsFrom(r, first, 0x6084) == 0);
    }
}
void fallingEdgeAbortSurvivesConsoleProjection() {
    const char* commands[] = {"@2 move speed 1 60 immediate", "@2 move speed 1 60 buffered"};
    for (const char* command : commands) {
        ConsoleRig r;
        r.start();
        const unsigned first = r.wire.sentCount;
        r.drive.abortIndex = 0x6040;
        r.drive.abortWord = 0x000f;
        r.clearAbortCode = 0x06090030;
        r.line(command);
        const Saved<E::MotionResult> original(*r.motion.normalResult(1));
        r.drain();
        const L::MoveContext& native = r.a.moveAction();
        CHECK(native.state == M::ActionState::FAILED && native.clearPresent && native.uncertain);
        CHECK(native.clearEvidence.outcome == C::SdoOutcome::SERVER_ABORT &&
              native.clearEvidence.abortCode == 0x06090030);
        CHECK(!native.triggerPresent && native.setupHistory[1].outcome == C::SdoOutcome::SUCCESS);
        CHECK(r.motion.normalResult().abortKnown && r.motion.normalResult().abortCode == 0x06090030);
        original.unchanged(*r.motion.normalResult(1));
        contains(r.bytes.output, "\"command_id\":2,\"operation_id\":2");
        contains(r.bytes.output, "\"abort_code\":101253168");
        const std::string retained = r.command("@3 result 2");
        contains(retained, "\"abort_code\":101253168");
        contains(retained, "\"uncertain\":true");
        CHECK(downloadsFrom(r, first, 0x6081) == 1 && wordsFrom(r, first, 0x000f) == 1);
        CHECK(wordsFrom(r, first, 0x003f) == 0 && wordsFrom(r, first, 0x001f) == 0);
    }
}
} // namespace
int main() {
    replacementRoutesRetainBothResults();
    closedWrongAndUnmappedChangesRefuseBeforeWrites();
    fallingEdgeAbortSurvivesConsoleProjection();
    std::printf("move change console: %u checks\n", checks);
}
