// SPDX-License-Identifier: MIT
// HOST_FAKE: real shared console/owner/session paths, synthetic drive evidence only.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
static unsigned checks = 0;
static bool asyncTraffic = false;
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); std::abort(); } } while (false)
#include "ProbeConsole.h"
#include "support/MoveRig.h"

namespace {
using namespace MoveRig;
struct Bytes {
    std::string output;
    bool blocked = false;
    static bool write(void* context, uint8_t byte) {
        Bytes& self = *static_cast<Bytes*>(context);
        if (self.blocked) return false;
        self.output += char(byte);
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
    Bytes bytes;
    E::ProbePlatform platform;
    E::ProbeApp app;
    L::ReadTarget first, second;
    E::ProbeMotion motionOne, motionTwo;
    E::GroupStopMember members[2];
    E::GroupStop group;
    E::ProbeConsole console;
    explicit ConsoleRig(bool selected = true)
        : Rig(true), platform(bytes.callbacks()), app(owner, platform), first(current(1)),
          second(current(2)), motionOne(app, owner, one.a, first, one.coordinates, one.witness),
          motionTwo(app, owner, two.a, second, two.coordinates, two.witness),
          group(app, owner, members, 2), console(app, owner, platform) {
        serviceSession = serviceTwo = false;
        CHECK(app.openAxis(one.a, first, L::SettingsModel::CL86_C, now) == &one.a);
        CHECK(app.openAxis(two.a, second, L::SettingsModel::CL86_C, now) == &two.a);
        CHECK(console.attachMotion(motionOne));
        CHECK(console.attachMotion(motionTwo));
        CHECK(console.attachGroupStop(group));
        if (selected)
            CHECK(app.select(first));
        CHECK(!wire.sentCount);
    }
    void prepare() {
        for (unsigned node = 1; node <= 2; ++node) {
            readIdentity(node);
            writeThrough(0x6040, 2, 0x000f, node);
        }
        for (unsigned node = 1; node <= 2; ++node) {
            Axis& x = axis(node);
            readState(x.state, STATE_MASK, node);
            x.coordinates.originKnown = true;
            x.coordinates.originSource = M::ScaleSource::QUALIFIED;
            x.witness = witness(*this, x.drive.position, M::RelativeBasis::ACTUAL, node);
            L::MoveOptions opt = options();
            opt.maximumAgeUs = 100000;
            opt.observationIntervalUs = 10000;
            CHECK((node == 1 ? motionOne : motionTwo).setMovePrerequisites(
                x.state, x.identity, evidence(), profile(), nullptr, opt));
            L::MoveReplacementEvidence replacement;
            replacement.gate = L::MoveChangeGate::MEASURED;
            replacement.record = 5301;
            replacement.model = L::SettingsModel::CL86_C;
            replacement.firmware = evidence().firmware;
            replacement.parameterMask = 15;
            replacement.fallingEdgeQualified = replacement.immediateQualified = true;
            replacement.completionQualified = replacement.stopQualified = true;
            L::MoveFeedbackRelation relation;
            relation.record = 5302;
            relation.binding = x.coordinates.binding;
            relation.nodeGeneration = current(node).nodeGeneration;
            relation.referenceGeneration = x.coordinates.referenceGeneration;
            relation.sameIncrement = true;
            CHECK((node == 1 ? motionOne : motionTwo).setMoveReplacementPrerequisites(replacement, &relation));
            x.drive.travelReads = 1000;
            x.drive.velocity = 30;
        }
    }
    void tick() {
        now += TICK;
        if (wire.rxRead == wire.rxWrite) wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (holdTx && wire.pending[served]) break;
            if (dropped[served]) continue;
            M::Frame reply;
            FakeDrive& d = wire.sent[served].id == 0x602 ? two.drive : one.drive;
            if (d.answer && !d.mute && replyTo(wire.sent[served], d, reply))
                wire.push(reply, now, owner.controllerEpoch());
        }
        CHECK(app.service(now));
        console.service(now);
    }
    void turns(unsigned count) { while (count--) tick(); }
    void line(const std::string& text) {
        for (char byte : text) console.feed(uint8_t(byte), now);
        console.feed('\n', now);
    }
    std::string command(const std::string& text) {
        const size_t start = bytes.output.size();
        const std::string identity = "\"id\":" + text.substr(1, text.find(' ') - 1) + ",";
        line(text);
        for (unsigned n = 0; n < 4000; ++n) {
            tick();
            const std::string output = bytes.output.substr(start);
            const size_t reply = output.find(identity);
            if (reply != std::string::npos && output.find('\n', reply) != std::string::npos)
                return output;
        }
        CHECK(false);
        return "";
    }
    E::StopPolicy policy() const {
        E::StopPolicy result;
        result.options.service = reads();
        result.options.observationIntervalUs = 1000;
        result.options.maximumAgeUs = 100000;
        result.deadlineUs = result.bestEffortDeadlineUs = now + LEAD;
        return result;
    }
    void startBoth() {
        prepare();
        CHECK(app.select(first));
        command("@1 accel 100 ms");
        command("@2 decel 200 ms");
        CHECK(app.select(second));
        command("@3 accel 100 ms");
        command("@4 decel 200 ms");
        CHECK(app.select(first));
        line("@11 move absolute 16000 steps native 120 configured");
        CHECK(app.select(second));
        line("@12 move absolute 24000 steps native 90 configured");
        for (unsigned n = 0; n < 300 && (!one.drive.edges || !two.drive.edges ||
                one.a.moveObservation().state != M::ReadState::SUCCEEDED ||
                two.a.moveObservation().state != M::ReadState::SUCCEEDED); ++n) tick();
        if (one.drive.edges != 1 || two.drive.edges != 1)
            std::fprintf(stderr, "console output: %s\n", bytes.output.c_str());
        CHECK(one.drive.edges == 1 && two.drive.edges == 1);
        CHECK(one.drive.moving && two.drive.moving);
        CHECK(motionOne.normalResult().state == M::ActionState::ACTIVE);
        CHECK(motionTwo.normalResult().state == M::ActionState::ACTIVE);
    }
};
void contains(const std::string& text, const char* expected) {
    if (text.find(expected) == std::string::npos)
        std::fprintf(stderr, "missing %s in %s\n", expected, text.c_str());
    CHECK(text.find(expected) != std::string::npos);
}
void independentStopAndSelection() {
    ConsoleRig r;
    CHECK(r.console.motionForAxis(101) == &r.motionOne);
    CHECK(r.console.motionForAxis(102) == &r.motionTwo);
    CHECK(r.console.attachMotion(r.motionOne));
    CHECK(r.console.selectedMotion() == &r.motionOne);
    r.startBoth();
    CHECK(r.console.selectedMotion() == &r.motionTwo);
    CHECK(!r.console.detachMotion(101));
    const uint32_t otherId = r.motionTwo.normalResult().id;
    CHECK(r.motionOne.setStopPrerequisites(r.one.a.moveObservation(), r.policy()));
    const unsigned start = r.wire.sentCount;
    // Explicit logical axis ignores the current selection; omitted kind means fast.
    r.line("@13 stop 101");
    CHECK(r.motionOne.stopRetained(L::StopKind::QUICK_STOP));
    CHECK(!r.motionTwo.stopRetained(L::StopKind::QUICK_STOP));
    r.turns(50);
    CHECK(wordsFrom(r, start, 0x000b, 1) == 1);
    CHECK(wordsFrom(r, start, 0x000b, 2) == 0);
    CHECK(r.two.drive.moving && r.motionTwo.normalResult().id == otherId);
    contains(r.bytes.output, "\"command\":\"stop\",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\",\"operation_id\":3,\"axis_id\":101");
    CHECK(r.app.selected() && r.app.selected()->channel.binding.logicalAxisId == 102);
    const std::string pending = r.command("@14 result 2");
    contains(pending, "\"result\":\"pending\"");
    const unsigned sent = r.wire.sentCount;
    contains(r.command("@15 stop 99"), "\"ok\":false");
    CHECK(!downloadsFrom(r, sent, 0x6040, 2));
}
void groupHasIndependentOutputAndResults() {
    ConsoleRig r;
    r.startBoth();
    CHECK(r.motionOne.setStopPrerequisites(r.one.a.moveObservation(), r.policy()));
    // Axis two deliberately lacks stop prerequisites. Its refusal cannot delay axis one.
    r.bytes.blocked = true;
    uint8_t padding[E::ProbePlatform::OUTPUT_CAPACITY];
    std::memset(padding, ' ', sizeof padding);
    CHECK(r.platform.writeOutput(padding, sizeof padding - r.platform.stats().outputUsed));
    r.line("@20 stats");
    r.line("@21 version");
    const unsigned start = r.wire.sentCount;
    r.line("@22 stop all");
    CHECK(r.group.memberCount() == 2);
    CHECK(r.group.member(0)->accepted && !r.group.member(1)->accepted);
    r.turns(50);
    CHECK(wordsFrom(r, start, 0x000b, 1) == 1);
    CHECK(!wordsFrom(r, start, 0x000b, 2));
    CHECK(r.two.drive.moving);
    CHECK(r.group.result().state == M::ActionState::FAILED && r.group.result().uncertain);
    // End the unrelated synthetic curve so the finite wire capture stays bounded while
    // the deliberately blocked byte sink drains all retained records.
    r.two.drive.remaining = 0;
    r.bytes.blocked = false;
    r.turns(400);
    contains(r.bytes.output, "\"type\":\"group_stop_axis\"");
    contains(r.bytes.output, "\"axis_id\":101,\"accepted\":true");
    contains(r.bytes.output, "\"axis_id\":102,\"accepted\":false");
    contains(r.bytes.output, "\"axis_count\":2");
    CHECK(r.motionOne.stopRetained(L::StopKind::QUICK_STOP));
    contains(r.command("@23 release 3"), "\"ok\":true");
    CHECK(!r.motionOne.stopRetained(L::StopKind::QUICK_STOP));
    CHECK(r.motionOne.retained() && r.motionTwo.retained());
}
void changeAndCancelFollowCapturedAxis() {
    ConsoleRig r;
    r.startBoth();
    CHECK(r.console.selectedMotion() == &r.motionTwo);
    const uint32_t oldOne = r.motionOne.normalResult().id;
    const uint32_t oldTwo = r.motionTwo.normalResult().id;
    const unsigned start = r.wire.sentCount;
    // The operation identity, rather than the current selection, chooses the axis.
    r.line("@30 move speed 1 60 immediate");
    CHECK(r.motionOne.normalResult().id != oldOne);
    CHECK(r.motionOne.normalResult(oldOne));
    CHECK(r.motionTwo.normalResult().id == oldTwo);
    r.turns(16);
    CHECK(downloadsFrom(r, start, 0x6081, 1) == 1);
    CHECK(!downloadsFrom(r, start, 0x6081, 2));
    CHECK(r.app.selected()->channel.binding.logicalAxisId == 102);
    // Cancel axis one's replacement while axis two is selected. It is a local
    // withdrawal and cannot overwrite or stop axis two's live result.
    r.line("@31 cancel 3");
    r.turns(2);
    CHECK(r.motionOne.normalResult().state != M::ActionState::ACTIVE);
    CHECK(r.motionTwo.normalResult().id == oldTwo);
    CHECK(r.two.drive.moving);
}
void ambiguousSelectionNeverChoosesFirstAxis() {
    ConsoleRig r(false);
    r.prepare();
    CHECK(!r.app.selected() && !r.console.selectedMotion());
    const unsigned before = r.wire.sentCount;
    contains(r.command("@40 move absolute 16000 steps native 30 configured"), "\"ok\":false");
    contains(r.command("@41 stop fast"), "\"ok\":false");
    CHECK(r.wire.sentCount == before);
    r.readState(r.one.state, ENABLE_MASK, 1);
    r.readState(r.two.state, ENABLE_MASK, 2);
    CHECK(r.motionOne.setStopPrerequisites(r.one.state, r.policy()));
    CHECK(r.motionTwo.setStopPrerequisites(r.two.state, r.policy()));
    r.line("@42 stop 101");
    CHECK(r.motionOne.stopRetained(L::StopKind::QUICK_STOP));
    r.turns(30);
    r.line("@43 stop all");
    CHECK(r.group.memberCount() == 2);
    CHECK(r.group.member(1)->accepted);
    r.turns(40);
    CHECK(!r.app.selected());
    CHECK(wordsFrom(r, before, 0x000b, 1) == 1);
    CHECK(wordsFrom(r, before, 0x000b, 2) == 1);
}
}
int main() {
    independentStopAndSelection();
    groupHasIndependentOutputAndResults();
    changeAndCancelFollowCapturedAxis();
    ambiguousSelectionNeverChoosesFirstAxis();
    std::printf("multi-axis console: %u checks\n", checks);
}
