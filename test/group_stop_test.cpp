// SPDX-License-Identifier: MIT
// HOST_FAKE: real owner/session/stop lifecycles; reports below do not qualify hardware.
#include <cstdio>
#include <cstdlib>
static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "";
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d %s [%s]\n", __FILE__, __LINE__, #c, scenario);             \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "GroupStop.h"
#include "WiredStopOutput.h"
#include "support/MoveRig.h"

namespace {
using namespace MoveRig;
struct GroupRig : Rig {
    E::ProbePlatform platform;
    E::ProbeApp app;
    L::ReadTarget first, second;
    E::ProbeMotion motion1, motion2;
    E::GroupStopMember rows[2];
    E::GroupStop group;
    E::ProbeMotion* members[2];
    GroupRig()
        : Rig(C::SdoClient::CHANNELS >= 2), platform(E::ProbeCallbacks()), app(owner, platform),
          first(current()), second(nodes == 2 ? current(2) : current()),
          motion1(app, owner, one.a, first, one.coordinates, one.witness),
          motion2(app, owner, two.a, second, two.coordinates, two.witness),
          group(app, owner, rows, 2), members{&motion1, &motion2} {
        CHECK(app.openAxis(one.a, first, L::SettingsModel::CL86_C, now));
        if (nodes == 2)
            CHECK(app.openAxis(two.a, second, L::SettingsModel::CL86_C, now));
        for (unsigned n = 1; n <= nodes; ++n) {
            writeThrough(0x6040, 2, 15, n);
            readState(axis(n).state, STATE_MASK, n);
        }
    }
    void arm(unsigned n) {
        E::StopPolicy policy;
        policy.options.service = reads();
        policy.options.observationIntervalUs = 1000;
        policy.options.maximumAgeUs = LEAD;
        policy.deadlineUs = now + LEAD;
        policy.bestEffortDeadlineUs = now + LEAD;
        CHECK(members[n - 1]->setStopPrerequisites(axis(n).state, policy));
    }
    void finish() {
        for (unsigned i = 0; group.result().state == M::ActionState::ACTIVE && i < 1600; ++i) {
            steps(1);
            CHECK(group.service(now));
        }
        CHECK(group.result().state != M::ActionState::ACTIVE);
    }
};
void successAndRetention() {
    scenario = "all observed independently, immutable retained group and explicit release";
    GroupRig r;
    for (unsigned n = 1; n <= r.nodes; ++n)
        r.arm(n);
    CHECK(r.group.matches(r.app, r.owner));
    CHECK(r.group.request(r.members, r.nodes, L::StopKind::HALT, r.now));
    CHECK(!r.group.release());
    CHECK(!r.group.request(r.members, r.nodes, L::StopKind::QUICK_STOP, r.now));
    r.finish();
    CHECK(r.group.result().state == M::ActionState::SUCCEEDED);
    CHECK(!r.group.result().uncertain);
    CHECK(!r.group.member(r.nodes));
    for (unsigned i = 0; i < r.nodes; ++i) {
        const E::GroupStopMember* row = r.group.member(i);
        CHECK(row && row->accepted && row->finished);
        CHECK(row->stop.result.completion == M::ActionCompletion::OBSERVED);
        CHECK(row->stop.sequenced.commandPresent);
        CHECK(row->logicalAxisId == r.members[i]->target().channel.binding.logicalAxisId);
    }
    Saved<E::GroupStopResult> held(r.group.result());
    CHECK(r.group.service(r.now + LEAD));
    held.unchanged(r.group.result());
    CHECK(!r.group.service(r.now));
    CHECK(r.group.release());
    CHECK(!r.group.release());
    CHECK(!r.motion1.stopRetained(L::StopKind::HALT));
}
void refusalAndLoss() {
    scenario = "first member refusal does not prevent another stop";
    GroupRig r;
    if (r.nodes < 2)
        return;
    r.arm(2); // First member deliberately has no stop prerequisites.
    CHECK(r.group.request(r.members, 2, L::StopKind::HALT, r.now));
    CHECK(!r.group.member(0)->accepted && r.group.member(0)->finished);
    CHECK(r.group.member(1)->accepted);
    r.finish();
    CHECK(r.group.member(1)->stop.result.state == M::ActionState::SUCCEEDED);
    CHECK(r.group.result().state == M::ActionState::FAILED && r.group.result().uncertain);
    CHECK(r.group.release());

    scenario = "lost first stop response leaves other member observed and no replay";
    GroupRig lost;
    lost.arm(1);
    lost.arm(2);
    lost.one.drive.mute = true;
    CHECK(lost.group.request(lost.members, 2, L::StopKind::HALT, lost.now));
    lost.finish();
    CHECK(lost.group.member(0)->stop.result.uncertain);
    CHECK(lost.group.member(1)->stop.result.state == M::ActionState::SUCCEEDED);
    CHECK(lost.group.result().state == M::ActionState::FAILED && lost.group.result().uncertain);
    unsigned stopWrites[2] = {};
    for (unsigned i = 0; i < lost.wire.sentCount; ++i) {
        const M::Frame& f = lost.wire.sent[i];
        if (f.data[0] == 0x2b && f.data[1] == 0x40 && f.data[2] == 0x60 && f.data[4] == 0x0f &&
            f.data[5] == 1)
            ++stopWrites[f.id == 0x602 ? 1 : 0];
        CHECK(f.id != 0); // No NMT stop/reset/start fallback.
    }
    CHECK(stopWrites[0] == 1 && stopWrites[1] == 1);
}
void atomicListAndCapacity() {
    scenario = "malformed group rejected before any member is stopped";
    GroupRig r;
    r.arm(1);
    E::ProbeMotion* duplicate[] = {&r.motion1, &r.motion1};
    CHECK(!r.group.request(duplicate, 2, L::StopKind::HALT, r.now));
    CHECK(!r.motion1.stopRetained(L::StopKind::HALT));
    CHECK(!r.group.request(r.members, 3, L::StopKind::HALT, r.now));
    CHECK(!r.group.request(nullptr, 1, L::StopKind::HALT, r.now));
    CHECK(!r.group.request(r.members, 0, L::StopKind::HALT, r.now));
    CHECK(r.group.result().state == M::ActionState::EMPTY);
    CHECK(r.group.request(r.members, 1, L::StopKind::HALT, r.now));
    r.finish();
    // Result release is intentionally external misuse: the owned snapshot must survive it.
    CHECK(r.motion1.releaseStop(L::StopKind::HALT));
    CHECK(r.group.member(0)->stop.result.completion == M::ActionCompletion::OBSERVED);
    CHECK(r.group.release());
}
void faultAndOrdinaryPressure() {
    scenario = "one drive fault cannot delay the other stop";
    GroupRig fault;
    if (fault.nodes < 2)
        return;
    fault.arm(1);
    fault.arm(2);
    CHECK(fault.group.request(fault.members, 2, L::StopKind::HALT, fault.now));
    fault.one.drive.monitor |= L::MONITOR_ALARM;
    fault.one.drive.errorCode = 1;
    fault.finish();
    CHECK(fault.group.member(0)->stop.result.state == M::ActionState::FAILED);
    CHECK(fault.group.member(1)->stop.result.state == M::ActionState::SUCCEEDED);
    CHECK(fault.group.result().uncertain);

    scenario = "all ordinary result slots occupied, urgent group still settles";
    GroupRig full;
    full.arm(1);
    full.arm(2);
    O::RequestId retained[O::CanBusOwner::REQUESTS];
    const uint32_t operation = full.a.beginOperation(full.now + LEAD, full.now);
    CHECK(operation);
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        O::BusRequest request;
        request.sequence = full.a.sequence();
        request.transfer.channel = full.first.channel;
        request.transfer.operation = request.transfer.request = operation;
        request.transfer.step = i + 1;
        request.transfer.index = 0x2100;
        request.transfer.direction = C::SdoDirection::UPLOAD;
        request.transfer.size = 2;
        request.transfer.deadlineUs = full.now + LEAD;
        request.transfer.queueTimeoutUs = QUEUE;
        request.transfer.responseTimeoutUs = RESPONSE;
        CHECK(full.owner.admit(request, full.now, retained[i]) == O::BusAdmission::ACCEPTED);
        CHECK(full.owner.cancel(retained[i], full.now) == O::Cancel::CANCELLED);
    }
    CHECK(full.owner.resources().retained == O::CanBusOwner::REQUESTS);
    CHECK(full.group.request(full.members, 2, L::StopKind::QUICK_STOP, full.now));
    full.finish();
    CHECK(full.group.result().state == M::ActionState::SUCCEEDED);
    for (unsigned i = 0; i < O::CanBusOwner::REQUESTS; ++i) {
        CHECK(full.owner.result(retained[i]));
        CHECK(full.owner.release(retained[i]));
    }
}
void originalEvidenceAndLostIdentity() {
    scenario = "group stop cannot erase earlier motion effects";
    GroupRig r;
    r.readIdentity();
    r.readState(r.state);
    r.one.coordinates.originKnown = true;
    r.one.coordinates.originSource = M::ScaleSource::QUALIFIED;
    r.one.witness = witness(r, r.drive.position);
    CHECK(r.motion1.setMovePrerequisites(r.state, r.identity, evidence(), profile(), nullptr,
                                         options()));
    r.drive.travelReads = 10000;
    CHECK(r.motion1.requestMove(moveRequest(), r.now, r.now + LEAD));
    for (unsigned i = 0; i < 300 && !r.a.moveAction().latest.present; ++i) {
        r.steps(1);
        CHECK(r.motion1.service(r.now));
    }
    CHECK(r.a.moveAction().execution == M::ActionExecution::ACKNOWLEDGED);
    E::StopPolicy policy;
    policy.options.service = reads();
    policy.options.observationIntervalUs = 1000;
    policy.options.maximumAgeUs = LEAD;
    policy.deadlineUs = r.now + LEAD;
    policy.bestEffortDeadlineUs = r.now + LEAD;
    CHECK(r.motion1.setStopPrerequisites(r.a.moveObservation(), policy));
    CHECK(r.group.request(r.members, 1, L::StopKind::HALT, r.now));
    r.finish();
    CHECK(r.group.result().state == M::ActionState::SUCCEEDED);
    CHECK(r.motion1.normalResult().state == M::ActionState::FAILED);
    CHECK(r.motion1.normalResult().uncertain);
    Saved<E::MotionResult> original(r.motion1.normalResult());
    CHECK(r.group.release());
    original.unchanged(r.motion1.normalResult());

    scenario = "premature external release cannot substitute another stop identity";
    GroupRig stolen;
    stolen.arm(1);
    CHECK(stolen.group.request(stolen.members, 1, L::StopKind::HALT, stolen.now));
    // Progress the real session and attachment but deliberately withhold group snapshotting.
    for (unsigned i = 0; i < 200 && stolen.motion1.stopResult(L::StopKind::HALT)->result.state ==
                                        M::ActionState::ACTIVE;
         ++i) {
        stolen.steps(1);
        CHECK(stolen.motion1.service(stolen.now));
    }
    CHECK(stolen.motion1.releaseStop(L::StopKind::HALT));
    CHECK(stolen.group.service(stolen.now));
    CHECK(stolen.group.result().state == M::ActionState::FAILED && stolen.group.result().uncertain);
    CHECK(stolen.group.release());
}
struct Output {
    unsigned calls = 0;
    bool high = false, fail = false;
    static M::Status write(void* p, bool high) {
        Output& o = *static_cast<Output*>(p);
        ++o.calls;
        o.high = high;
        return o.fail ? M::Status(M::Err::INVALID_CONFIG, 17, "host write uncertain") : M::Ok();
    }
};
void wired() {
    scenario = "wired signal needs explicit facts and action, never implies stopped";
    Output out;
    E::WiredStopSignal signal;
    signal.context = &out;
    signal.writeLevel = Output::write;
    E::WiredStopOutput unknown(signal);
    CHECK(!unknown.setAsserted(true) && out.calls == 0);
    CHECK(!unknown.result().attempted);
    signal.wiringKnown = true;
    for (unsigned active = 0; active != 2; ++active) {
        signal.activeHigh = bool(active);
        E::WiredStopOutput stop(signal);
        const unsigned before = out.calls;
        CHECK(stop.setAsserted(true) && out.high == bool(active));
        CHECK(stop.setAsserted(false) && out.high != bool(active));
        CHECK(out.calls == before + 2);
        out.fail = true;
        CHECK(!stop.setAsserted(true) && stop.result().uncertain);
        CHECK(out.calls == before + 3); // No hidden retry, including destruction.
        out.fail = false;
    }
}
} // namespace
int main() {
    successAndRetention();
    refusalAndLoss();
    atomicListAndCapacity();
    faultAndOrdinaryPressure();
    originalEvidenceAndLostIdentity();
    wired();
    std::printf("group stop: %u checks\n", checks);
}
