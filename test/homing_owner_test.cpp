// SPDX-License-Identifier: MIT
// Synthetic homing through the real owner; no fixture or firmware qualification.
#include <cstdio>
#include <cstdlib>
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
#include "support/VelocityRig.h"
namespace {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
using VelocityRig::indexOf;
using VelocityRig::reads;
using VelocityRig::Saved;
using VelocityRig::target;
using VelocityRig::valueOf;
const uint64_t DEADLINE = 300000;
struct Drive {
    uint16_t monitor = 0, status = 7, inputs = 0;
    int32_t velocity = 0, position = 0;
    unsigned cycle = 0;
    bool triggered = false, complete = true, muteTrigger = false, muteStop = false;
    bool respond(const M::Frame& sent, M::Frame& out) {
        const uint16_t index = indexOf(sent);
        const uint8_t sub = sent.data[3];
        out = OwnerRig::frame(sent.id - 0x600 + 0x580, 8);
        out.data[1] = sent.data[1];
        out.data[2] = sent.data[2];
        out.data[3] = sub;
        if (sent.data[0] != 0x40) {
            if (index == 0x6040) {
                const uint32_t word = valueOf(sent);
                if (word == 0x001f) {
                    triggered = true;
                    cycle = 0;
                    monitor = L::MONITOR_RUNNING;
                    status = 7;
                    velocity = 120;
                    if (muteTrigger)
                        return false;
                } else if (word == 0x000f || word == 0x000b) {
                    triggered = false;
                    monitor = 0;
                    status = 7;
                    velocity = 0;
                    if (muteStop)
                        return false;
                } else
                    CHECK(false);
            } else
                CHECK(index == 0x6098 || index == 0x6099 || index == 0x609a || index == 0x607c);
            out.data[0] = 0x60;
            return true;
        }
        uint32_t raw = 0;
        unsigned size = 2;
        switch (index) {
        case 0x1000:
            raw = VelocityRig::DEVICE_TYPE;
            size = 4;
            break;
        case 0x1009:
            raw = VelocityRig::HARDWARE;
            break;
        case 0x100a:
            raw = VelocityRig::SOFTWARE;
            break;
        case 0x1018:
            size = sub ? 4 : 1;
            raw = !sub       ? 3
                  : sub == 1 ? VelocityRig::VENDOR
                  : sub == 2 ? VelocityRig::PRODUCT
                             : VelocityRig::REVISION;
            break;
        case 0x2000:
            raw = VelocityRig::MODEL;
            break;
        case 0x2001:
            raw = VelocityRig::FIRMWARE;
            break;
        case 0x2100:
            if (triggered) {
                ++cycle;
                if (complete && cycle >= 3) {
                    monitor = L::MONITOR_HOMING_COMPLETE;
                    status = 0x107;
                    velocity = 0;
                    inputs = 0;
                } else {
                    monitor = L::MONITOR_RUNNING;
                    status = 7;
                    velocity = 120;
                    inputs = 1;
                }
            }
            raw = monitor;
            break;
        case 0x2101:
            raw = inputs;
            break;
        case 0x2102:
            break;
        case 0x6041:
            raw = status;
            break;
        case 0x6061:
            raw = 6;
            size = 1;
            break;
        case 0x6064:
            raw = uint32_t(position);
            size = 4;
            break;
        case 0x606c:
            raw = uint32_t(velocity);
            size = 4;
            break;
        case 0x2200:
            break;
        case 0x2201:
            raw = 3200;
            break;
        case 0x2400:
            raw = !sub ? 16 : sub == 2 ? 1 : 0;
            break;
        default:
            CHECK(false);
        }
        out.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
        for (unsigned i = 0; i < 4; ++i)
            out.data[4 + i] = uint8_t(raw >> (8 * i));
        return true;
    }
};
struct Rig {
    C::SdoClient services;
    VelocityRig::Wire wire;
    O::CanBusOwner owner;
    E::AxisSession session;
    Drive drive;
    L::ReadContext state, identityRead;
    L::IdentityObservation identity;
    L::DriverContext inputs, driver;
    M::AxisConfig axis = VelocityRig::axisConfig();
    M::AxisReference reference;
    uint64_t now = 0;
    unsigned served = 0, startFrame = 0;
    Rig() : owner(services, wire.callbacks(), 73), session(owner) {
        wire.session = &session;
        wire.services = &services;
        CHECK(services.configure(0, target().channel, true, true));
        C::NetworkRoute network;
        network.binding = target().channel.binding;
        network.generation = 11;
        network.heartbeatId = 0x701;
        network.emcyId = 0x81;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
        reference.nativeKnown = reference.idle = reference.stationary = true;
        reference.binding = axis.binding;
        reference.referenceGeneration = axis.referenceGeneration;
    }
    void step() {
        now += 500;
        if (wire.rxRead == wire.rxWrite)
            wire.rxRead = wire.rxWrite = 0;
        for (; served < wire.sentCount; ++served) {
            if (wire.pending[served])
                break;
            M::Frame reply;
            if (drive.respond(wire.sent[served], reply))
                wire.push(reply, now, owner.controllerEpoch());
        }
        CHECK(owner.service(now, OwnerRig::budget()));
        session.service(now);
    }
    L::ReadTarget current() const {
        L::ReadTarget t = target();
        t.channel.binding = session.binding();
        return t;
    }
    void read(L::ReadContext& into) {
        for (unsigned i = 0; into.state == M::ReadState::ACTIVE && i < 32; ++i) {
            L::PreparedRead work;
            CHECK(L::nextRead(into, now, work) && work.ready);
            CHECK(session.admitPrepared(work.transfer, now) == O::BusAdmission::ACCEPTED);
            step();
            step();
            C::SdoCompletion terminal;
            CHECK(session.settle(terminal) && L::advanceRead(into, terminal, now));
        }
        CHECK(into.state == M::ReadState::SUCCEEDED);
    }
    void readState() {
        const uint32_t op = session.beginOperation(now + DEADLINE, now);
        CHECK(op);
        const uint8_t mask = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                     (1u << unsigned(L::StateBlock::STATUS)) |
                                     (1u << unsigned(L::StateBlock::FEEDBACK)));
        CHECK(L::prepareState(state, current(), op, now, now + DEADLINE, reads(), mask));
        read(state);
    }
    void ready() {
        CHECK(session.open(target(), L::SettingsModel::CL86_C, 1, now));
        uint32_t op = session.beginOperation(now + DEADLINE, now);
        CHECK(op);
        C::SdoTransfer word;
        word.channel = target().channel;
        word.operation = word.request = op;
        word.step = 1;
        word.index = 0x6040;
        word.direction = C::SdoDirection::DOWNLOAD;
        word.size = 2;
        word.data[0] = 15;
        word.deadlineUs = now + DEADLINE;
        word.queueTimeoutUs = word.responseTimeoutUs = 1000;
        CHECK(session.admitPrepared(word, now) == O::BusAdmission::ACCEPTED);
        step();
        step();
        C::SdoCompletion terminal;
        CHECK(session.settle(terminal));
        op = session.beginOperation(now + DEADLINE, now);
        CHECK(op);
        CHECK(L::prepareIdentity(identityRead, current(), op, now, now + DEADLINE, reads()));
        read(identityRead);
        CHECK(L::getIdentity(identityRead, current(), now, DEADLINE, identity));
        op = session.beginOperation(now + DEADLINE, now);
        CHECK(op);
        CHECK(L::prepareDriverRead(inputs, current(), op, now, now + DEADLINE, reads(),
                                   L::DriverGroup::IO_INPUT_CONFIGURATION));
        read(inputs.read);
        op = session.beginOperation(now + DEADLINE, now);
        CHECK(op);
        CHECK(L::prepareDriverRead(driver, current(), op, now, now + DEADLINE, reads(),
                                   L::DriverGroup::DRIVER));
        read(driver.read);
        readState();
    }
    L::HomeEvidence evidence() const {
        L::HomeEvidence e;
        e.gate = L::HomeGate::MEASURED;
        e.record = 361;
        e.model = L::SettingsModel::CL86_C;
        e.specimen = current();
        e.method = L::HomingMethod::NEGATIVE_ORIGIN;
        e.layout = L::StatusLayout::ZH86;
        e.searchSpeedRpm = 120;
        e.creepSpeedRpm = 60;
        e.rampTimeMs = 100;
        e.inputFunctions[0] = 1;
        e.wiredInputs = 1;
        e.methodQualified = e.parametersQualified = e.inputsQualified = e.referenceQualified = true;
        const L::VelocityFirmware f = VelocityRig::evidence().firmware;
        e.firmware.deviceType = f.deviceType;
        e.firmware.hardwareVersion = f.hardwareVersion;
        e.firmware.softwareVersion = f.softwareVersion;
        e.firmware.vendorId = f.vendorId;
        e.firmware.productCode = f.productCode;
        e.firmware.revision = f.revision;
        e.firmware.modelCode = f.modelCode;
        e.firmware.firmwareCode = f.firmwareCode;
        return e;
    }
    L::HomeRequest request() const {
        L::HomeRequest q;
        q.method = L::HomingMethod::NEGATIVE_ORIGIN;
        q.searchSpeed = VelocityRig::request(120).velocity;
        q.creepSpeed = VelocityRig::request(60).velocity;
        q.searchSpeed.configurationGeneration = q.creepSpeed.configurationGeneration =
            axis.binding.configurationGeneration;
        q.rampTimeMs = 100;
        return q;
    }
    L::HomeOptions options() const {
        L::HomeOptions o;
        o.service = reads();
        o.pollIntervalUs = 10000;
        o.maxPolls = 10;
        o.maximumAgeUs = DEADLINE;
        return o;
    }
    M::Status start() {
        const unsigned first = wire.sentCount;
        const M::Status status =
            session.requestHome(state, identity, inputs, driver, evidence(), request(), axis,
                                reference, options(), now + DEADLINE, now);
        if (status)
            startFrame = first;
        return status;
    }
    void finish() {
        for (unsigned i = 0; session.busy() && i < 1000; ++i)
            step();
        CHECK(!session.busy());
    }
    unsigned writes(uint16_t index, uint32_t value) const {
        unsigned count = 0;
        for (unsigned i = startFrame; i < wire.sentCount; ++i)
            count += wire.sent[i].data[0] != 0x40 && indexOf(wire.sent[i]) == index &&
                     valueOf(wire.sent[i]) == value;
        return count;
    }
    void running() {
        for (unsigned i = 0;
             i < 250 && !session.homeAction().activity.present && session.homeActive(); ++i)
            step();
        CHECK(session.homeActive() && session.homeAction().activity.present);
    }
    E::StopPolicy stopPolicy(bool bestEffort = false) const {
        E::StopPolicy p;
        p.options.service = reads();
        p.options.observationIntervalUs = 10000;
        p.options.maximumAgeUs = DEADLINE;
        p.deadlineUs = now + 100000;
        p.bestEffortDeadlineUs = bestEffort ? p.deadlineUs : 0;
        return p;
    }
};
void completeHomeHasRealNewEpochEvidence() {
    scenario = "method29 real owner and reference";
    Rig r;
    r.ready();
    CHECK(r.start());
    r.finish();
    const L::HomeContext& c = r.session.homeAction();
    if (c.state != M::ActionState::SUCCEEDED)
        std::fprintf(stderr, "home phase%u outcome%u: %s\n", unsigned(c.phase), unsigned(c.outcome),
                     c.status.msg);
    CHECK(c.state == M::ActionState::SUCCEEDED && c.completion == M::ActionCompletion::OBSERVED);
    CHECK(c.absence.present && c.activity.present && c.requiredInputObserved &&
          !c.completed.logicalInputs);
    CHECK(r.axis.binding.configurationGeneration == 8 && r.axis.referenceGeneration == 2 &&
          !r.reference.nativeKnown);
    CHECK(c.trigger.transfer.channel.binding.configurationGeneration == 8 &&
          c.observationEvidence.transfer.channel.binding.configurationGeneration == 8 &&
          c.reservation.target.channel.binding.configurationGeneration == 7);
    CHECK(r.session.binding().configurationGeneration == 8 && r.session.latestControl() &&
          r.session.latestControl()->transfer.channel.binding.configurationGeneration == 8);
    CHECK(!r.session.homeNeedsStop() && r.writes(0x6040, 31) == 1 && !r.writes(0x6040, 15));
    M::AxisReference result;
    CHECK(L::getHomeReference(c, r.current(), r.axis, r.now, DEADLINE, result) &&
          result.nativeKnown);
    const Saved<M::AxisReference> saved(result);
    CHECK(M::invalidateAxisReference(r.axis, r.reference, M::ReferenceLoss::COORDINATES));
    CHECK(!L::getHomeReference(c, r.current(), r.axis, r.now, DEADLINE, result));
    saved.unchanged(result);
}
void explicitStopKeepsHomeAndStopSeparate() {
    scenario = "mode6 stop uses current reservation";
    Rig r;
    r.drive.complete = false;
    r.ready();
    CHECK(r.start());
    r.running();
    CHECK(r.session.requestStop(L::StopKind::HALT, r.session.homeObservation(), r.stopPolicy(),
                                r.now) == E::StopPath::CONFIRMED);
    r.finish();
    const L::HomeContext& c = r.session.homeAction();
    CHECK(c.state == M::ActionState::FAILED && c.interruption == M::HomeInterruption::STOP);
    CHECK(c.stopOperation > c.operation && c.stopExecution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(c.stopEvidence.transfer.channel.binding.configurationGeneration == 8);
    CHECK(r.writes(0x6040, 15) == 1 && r.writes(0x6040, 31) == 1 && !r.session.homeNeedsStop());
}
void cancelledQueuedTriggerKeepsWithdrawalWithoutMotion() {
    scenario = "queued trigger cancellation boundary";
    Rig r;
    r.ready();
    CHECK(r.start());
    for (unsigned i = 0; i < 100 && !r.session.homeAction().coordinatesInvalidated; ++i)
        r.step();
    CHECK(r.session.homeAction().coordinatesInvalidated && r.session.homeActive());
    CHECK(r.session.cancelHome(r.now));
    r.finish();
    CHECK(r.axis.binding.configurationGeneration == 8 && !r.reference.nativeKnown &&
          !r.writes(0x6040, 31));
    CHECK(r.session.homeAction().execution == M::ActionExecution::NOT_TRANSMITTED &&
          !r.session.homeNeedsStop());
}
void escalationRetainsTheFirstStopFailure() {
    scenario = "first HALT failure survives explicit best-effort escalation";
    Rig r;
    r.drive.complete = false;
    r.ready();
    CHECK(r.start());
    r.running();
    CHECK(r.session.requestStop(L::StopKind::HALT, r.session.homeObservation(), r.stopPolicy(true),
                                r.now) == E::StopPath::CONFIRMED);
    const uint32_t first = r.session.stopAction(L::StopKind::HALT).operation;
    CHECK(r.session.requestStop(L::StopKind::QUICK_STOP, r.session.homeObservation(),
                                r.stopPolicy(true), r.now) != E::StopPath::NONE);
    r.finish();
    CHECK(r.session.homeAction().stopOperation == first &&
          r.session.homeAction().stopOutcome == M::ActionOutcome::CANCELLED);
    CHECK(r.session.homeAction().interruption == M::HomeInterruption::STOP &&
          r.session.homeNeedsStop());
    CHECK(r.writes(0x6040, 11) == 1);
}
void unknownTriggerSurvivesReopenAndBestEffortStop() {
    scenario = "lost trigger reply persistent uncertainty";
    Rig r;
    r.drive.muteTrigger = true;
    r.ready();
    CHECK(r.start());
    r.finish();
    const L::HomeContext& c = r.session.homeAction();
    CHECK(c.state == M::ActionState::FAILED && c.execution == M::ActionExecution::UNKNOWN &&
          r.session.homeNeedsStop());
    CHECK(r.writes(0x6040, 31) == 1 && !r.start());
    CHECK(r.session.requestStop(L::StopKind::QUICK_STOP, r.session.homeObservation(),
                                r.stopPolicy(true), r.now) != E::StopPath::NONE);
    r.finish();
    CHECK(r.session.homeNeedsStop() &&
          r.session.homeAction().execution == M::ActionExecution::UNKNOWN);
    CHECK(r.writes(0x6040, 31) == 1 && r.writes(0x6040, 11) == 1);
    const Saved<L::HomeContext> terminal(r.session.homeAction());
    CHECK(r.session.close());
    CHECK(r.session.open(r.current(), L::SettingsModel::CL86_C, 1, r.now));
    terminal.unchanged(r.session.homeAction());
    CHECK(r.session.homeNeedsStop() && !r.start());
}
void handoverRejectsChangedAxis() {
    scenario = "configuration mutation before controller handover";
    Rig r;
    r.ready();
    CHECK(r.start());
    r.session.service(r.now);
    ++r.axis.binding.configurationGeneration;
    r.finish();
    CHECK(r.session.homeAction().state == M::ActionState::FAILED && r.wire.preflightRefusals == 1);
    CHECK(!r.session.homeRefusal().isOk() && !r.writes(0x6098, 29));
}
void staleReferenceRefusesBeforeAnySetup() {
    scenario = "known stale trigger witness refuses atomically before setup";
    Rig r;
    r.ready();
    ++r.reference.referenceGeneration;
    const unsigned frames = r.wire.sentCount;
    const uint32_t operation = r.session.nextOperation();
    const Saved<L::HomeContext> before(r.session.homeAction());
    CHECK(!r.start());
    before.unchanged(r.session.homeAction());
    CHECK(r.session.nextOperation() == operation && r.wire.sentCount == frames &&
          !r.session.busy());
    r.step();
    CHECK(r.wire.sentCount == frames);
}
void lostStopReplyKeepsIndependentFailure() {
    scenario = "lost mode6 interrupt reply keeps home and stop uncertainty";
    Rig r;
    r.drive.complete = false;
    r.ready();
    CHECK(r.start());
    r.running();
    r.drive.muteStop = true;
    CHECK(r.session.requestStop(L::StopKind::HALT, r.session.homeObservation(), r.stopPolicy(),
                                r.now) == E::StopPath::CONFIRMED);
    r.finish();
    const L::HomeContext& c = r.session.homeAction();
    CHECK(c.interruption == M::HomeInterruption::STOP && c.stopOperation > c.operation &&
          c.stopExecution == M::ActionExecution::UNKNOWN && !c.stopStatus.isOk() &&
          r.session.homeNeedsStop());
    CHECK(r.writes(0x6040, 15) == 1 && !r.writes(0x6040, 11) && !r.start());
}
void bootUpRevokesTheRunningHome() {
    scenario = "boot-up invalidates possible home execution";
    Rig r;
    r.drive.complete = false;
    r.ready();
    CHECK(r.start());
    r.running();
    M::Frame boot = OwnerRig::frame(0x701, 1);
    boot.data[0] = 0;
    r.wire.push(boot, r.now + 500, r.owner.controllerEpoch());
    r.step();
    r.finish();
    const L::HomeContext& c = r.session.homeAction();
    CHECK(c.state == M::ActionState::FAILED && c.completion == M::ActionCompletion::NOT_OBSERVED &&
          r.session.homeNeedsStop() && !r.reference.nativeKnown);
    CHECK(r.writes(0x6040, 31) == 1 && !r.writes(0x6040, 15) && !r.writes(0x6040, 11));
    M::AxisReference reference;
    CHECK(!L::getHomeReference(c, r.current(), r.axis, r.now, DEADLINE, reference));
}
void controllerLossRetainsTheLaterStopFailure() {
    scenario = "bus-off home failure and later held-stop failure remain separate";
    Rig r;
    r.drive.complete = false;
    r.ready();
    CHECK(r.start());
    r.running();
    CHECK(r.owner.reportController(C::ControllerState::BUS_OFF, r.now));
    r.session.service(r.now);
    r.finish();
    const M::ActionOutcome homeOutcome = r.session.homeAction().outcome;
    const M::HomeInterruption homeInterruption = r.session.homeAction().interruption;
    CHECK(r.session.homeAction().state == M::ActionState::FAILED && r.session.homeNeedsStop());
    CHECK(r.session.requestStop(L::StopKind::QUICK_STOP, r.session.homeObservation(),
                                r.stopPolicy(true), r.now) == E::StopPath::HELD);
    r.finish();
    const L::BestEffortStop& stop = r.session.bestEffort();
    const L::HomeContext& home = r.session.homeAction();
    CHECK(stop.state == M::ActionState::FAILED && stop.outcome == M::ActionOutcome::DEADLINE &&
          stop.execution == M::ActionExecution::NOT_TRANSMITTED);
    CHECK(stop.target.nodeGeneration > home.reservation.target.nodeGeneration &&
          M::sameBinding(stop.target.channel.binding, r.session.binding()));
    CHECK(home.stopOperation == stop.operation && home.stopOutcome == stop.outcome &&
          home.stopExecution == stop.execution && home.stopStatus.code == stop.status.code &&
          home.stopStatus.detail == stop.status.detail);
    CHECK(home.outcome == homeOutcome && home.interruption == homeInterruption &&
          home.state == M::ActionState::FAILED && r.session.homeNeedsStop());
    CHECK(r.writes(0x6040, 31) == 1 && !r.writes(0x6040, 15) && !r.writes(0x6040, 11));
}
void routedFaultAndPreflightNeverResumeHome() {
    scenario = "routed home fault and guard withdrawal";
    for (unsigned late = 0; late < 2; ++late) {
        Rig r;
        r.drive.complete = false;
        r.ready();
        CHECK(r.start());
        if (late) {
            r.running();
            CHECK(r.session.cancelHome(r.now, M::HomeInterruption::DRIVE_FAULT));
        } else {
            r.session.service(r.now);
            r.wire.driveFault = true;
        }
        r.finish();
        CHECK(r.session.faultBlocked() && r.session.homeAction().state == M::ActionState::FAILED &&
              r.session.homeAction().interruption == M::HomeInterruption::DRIVE_FAULT);
        CHECK(r.session.homeNeedsStop() == bool(late) && !r.writes(0x6040, 15) &&
              !r.writes(0x6040, 11));
        if (!late)
            CHECK(r.wire.preflightRefusals == 1 && !r.writes(0x6098, 29));
        const unsigned frames = r.wire.sentCount;
        r.step();
        CHECK(r.wire.sentCount == frames && !r.start());
    }
}
void deadlineAfterStartCannotPublishReference() {
    scenario = "processing cutoff after possible home trigger";
    Rig r;
    r.drive.complete = false;
    r.ready();
    CHECK(r.start());
    r.running();
    r.now = r.session.homeAction().deadlineUs - 500;
    r.step();
    r.finish();
    const L::HomeContext& c = r.session.homeAction();
    CHECK(c.state == M::ActionState::FAILED && c.outcome == M::ActionOutcome::DEADLINE &&
          r.session.homeNeedsStop() && c.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(r.writes(0x6040, 31) == 1 && !r.writes(0x6040, 15) && !r.writes(0x6040, 11));
}
} // namespace
int main() {
    completeHomeHasRealNewEpochEvidence();
    explicitStopKeepsHomeAndStopSeparate();
    cancelledQueuedTriggerKeepsWithdrawalWithoutMotion();
    escalationRetainsTheFirstStopFailure();
    unknownTriggerSurvivesReopenAndBestEffortStop();
    handoverRejectsChangedAxis();
    staleReferenceRefusesBeforeAnySetup();
    lostStopReplyKeepsIndependentFailure();
    bootUpRevokesTheRunningHome();
    controllerLossRetainsTheLaterStopFailure();
    routedFaultAndPreflightNeverResumeHome();
    deadlineAfterStartCannotPublishReference();
    std::printf("homing owner: %u checks passed\n", checks);
}
