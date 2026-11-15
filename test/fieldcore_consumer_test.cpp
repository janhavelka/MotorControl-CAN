// SPDX-License-Identifier: MIT
#include "FakeOwner.h"
#include <cstdio>
#include <cstring>

namespace F = FieldCoreConsumer;
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace C = MotorControlCAN::CANOPEN;

namespace {
unsigned checks = 0;
#define CHECK(condition)                                                                            \
    do {                                                                                            \
        ++checks;                                                                                   \
        if (!(condition)) {                                                                         \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition);                             \
            return false;                                                                           \
        }                                                                                           \
    } while (false)

bool preparationIsAtomic() {
    static F::Fixture fixture;
    static F::MotorModule module;
    static unsigned char saved[sizeof(module)];
    F::FakeOwner owner;
    uint64_t now = 0;
    CHECK(F::initialize(fixture, owner, now));
    std::memcpy(saved, &module, sizeof module);
    fixture.evidence.gate = L::MoveGate::CLOSED;
    CHECK(F::start(module, fixture, now).code == M::Err::UNSUPPORTED);
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    fixture.evidence.gate = L::MoveGate::MEASURED;
    fixture.request.position.value = M::Rational(501, 2);
    CHECK(!F::start(module, fixture, now)); // EXACT refuses a half native increment.
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    fixture.request.position.value = M::Rational(250);
    CHECK(F::start(module, fixture, now));
    CHECK(module.result.admission == F::Admission::ACCEPTED);
    CHECK(module.result.wire == F::WireSettlement::NOT_SENT);
    CHECK(module.move.parameters.target.exactArithmetic);
    CHECK(module.move.parameters.targetNative == 250);
    CHECK(module.move.execution == M::ActionExecution::NOT_TRANSMITTED);
    CHECK(!module.resultReady && !module.outstanding);
    std::memcpy(saved, &module, sizeof module);
    CHECK(!F::start(module, fixture, now)); // An active lane cannot be overwritten.
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    return true;
}

bool correlationAndRetention() {
    static F::Fixture fixture;
    static F::MotorModule module;
    static unsigned char saved[sizeof(module)];
    F::FakeOwner owner;
    uint64_t now = 0;
    CHECK(F::initialize(fixture, owner, now));
    CHECK(F::start(module, fixture, now));
    const unsigned before = owner.submitted;
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(module.outstanding && owner.pending);
    CHECK(owner.submitted == before + 1);
    L::PreparedMove work;
    CHECK(F::nextStep(module, fixture.live, now, work));
    CHECK(!work.ready && !work.observe);
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(owner.submitted == before + 1); // A poll before its terminal cannot replay it.
    now += 500;
    CHECK(owner.poll(now));
    C::SdoCompletion wrong = owner.terminal;
    ++wrong.transfer.operation;
    std::memcpy(saved, &module, sizeof module);
    CHECK(!F::advance(module, fixture.live, wrong, owner.wire, now));
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    CHECK(owner.pending && owner.ready);
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(!module.outstanding && !owner.pending);
    bool acknowledgedWithoutCompletion = false;
    for (unsigned turn = 0; turn < 200 && !module.resultReady; ++turn) {
        now += 500;
        CHECK(F::service(module, fixture.live, owner, now));
        if (module.move.execution == M::ActionExecution::ACKNOWLEDGED &&
            module.move.completion == M::ActionCompletion::NOT_OBSERVED) {
            CHECK(!module.resultReady && module.move.state == M::ActionState::ACTIVE);
            acknowledgedWithoutCompletion = true;
        }
    }
    CHECK(acknowledgedWithoutCompletion);
    CHECK(module.resultReady);
    CHECK(module.result.state == M::ActionState::SUCCEEDED);
    CHECK(module.result.setup == M::ActionExecution::ACKNOWLEDGED);
    CHECK(module.result.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(module.result.completion == M::ActionCompletion::OBSERVED);
    CHECK(module.move.acceptance.present && module.move.completed.present);
    CHECK(!module.result.uncertain);
    F::MoveResult result;
    result.operation = 999;
    CHECK(!F::takeResult(module, 99, result));
    CHECK(result.operation == 999 && module.resultReady);
    CHECK(F::takeResult(module, 4, result));
    CHECK(result.binding.logicalAxisId == 42 && result.binding.endpoint.address == 9);
    CHECK(result.wire == F::WireSettlement::SETTLED);
    CHECK(!F::takeResult(module, 4, result));
    return true;
}

bool interruptedTriggerStaysUnknown() {
    static F::Fixture fixture;
    static F::MotorModule module;
    F::FakeOwner owner;
    uint64_t now = 0;
    CHECK(F::initialize(fixture, owner, now));
    CHECK(F::start(module, fixture, now));
    for (unsigned turn = 0; turn < 40 && module.move.phase != L::MovePhase::TRIGGER; ++turn) {
        CHECK(F::service(module, fixture.live, owner, now));
        now += 500;
    }
    CHECK(module.move.phase == L::MovePhase::TRIGGER && !module.outstanding);
    owner.nextOutcome = C::SdoOutcome::TIMEOUT;
    // A physically settled TX still cannot establish SDO success or drive acceptance.
    owner.nextWire = F::WireSettlement::SETTLED;
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(module.outstanding && owner.pending);
    CHECK(owner.terminal.transfer.index == 0x6040 && owner.terminal.transfer.data[0] == 0x1f);
    const unsigned submitted = owner.submitted;
    CHECK(F::interrupt(module, fixture.live, M::MoveInterruption::CANCELLED, now));
    CHECK(!module.resultReady && module.outstanding); // Wait for actual owner retirement.
    now += 500;
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(module.resultReady && !module.outstanding && !owner.pending);
    F::MoveResult result;
    CHECK(F::takeResult(module, 4, result));
    CHECK(result.state == M::ActionState::FAILED);
    CHECK(result.outcome == M::ActionOutcome::CANCELLED);
    CHECK(result.execution == M::ActionExecution::UNKNOWN);
    CHECK(result.setup == M::ActionExecution::ACKNOWLEDGED);
    CHECK(result.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(result.uncertain && result.wire == F::WireSettlement::SETTLED);
    CHECK(module.move.triggerPresent && module.move.trigger.transmissionConfirmed);
    CHECK(!module.move.trigger.protocolAcknowledged);
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(owner.submitted == submitted); // No retry, automatic stop or enable.
    return true;
}

bool configurationChangeRejectsUnsentWork() {
    static F::Fixture fixture;
    static F::MotorModule module;
    F::FakeOwner owner;
    uint64_t now = 0;
    CHECK(F::initialize(fixture, owner, now));
    CHECK(F::start(module, fixture, now));
    ++fixture.live.axis.binding.configurationGeneration;
    const unsigned submitted = owner.submitted;
    CHECK(!F::service(module, fixture.live, owner, now));
    CHECK(!module.outstanding && !owner.pending && owner.submitted == submitted);
    CHECK(F::interrupt(module, fixture.live, M::MoveInterruption::INVALIDATED, now));
    F::MoveResult result;
    CHECK(F::takeResult(module, 4, result));
    CHECK(result.outcome == M::ActionOutcome::INVALIDATED);
    CHECK(result.execution == M::ActionExecution::NOT_TRANSMITTED);
    CHECK(result.wire == F::WireSettlement::NOT_SENT && !result.uncertain);
    return true;
}

bool expiredUnsentMovePublishesItsTerminal() {
    static F::Fixture fixture;
    static F::MotorModule module;
    F::FakeOwner owner;
    uint64_t now = 0;
    CHECK(F::initialize(fixture, owner, now));
    CHECK(F::start(module, fixture, now));
    const unsigned submitted = owner.submitted;
    now = module.move.deadlineUs;
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(module.resultReady && !module.outstanding && !owner.pending);
    CHECK(owner.submitted == submitted);
    F::MoveResult result;
    CHECK(F::takeResult(module, 4, result));
    CHECK(result.state == M::ActionState::FAILED && result.outcome == M::ActionOutcome::DEADLINE);
    CHECK(result.execution == M::ActionExecution::NOT_TRANSMITTED && !result.uncertain);
    CHECK(result.wire == F::WireSettlement::NOT_SENT);
    CHECK(F::service(module, fixture.live, owner, now + 1));
    CHECK(owner.submitted == submitted);
    return true;
}

bool pendingTriggerSettlesAtDeadline() {
    static F::Fixture fixture;
    static F::MotorModule module;
    F::FakeOwner owner;
    uint64_t now = 0;
    CHECK(F::initialize(fixture, owner, now));
    CHECK(F::start(module, fixture, now));
    for (unsigned turn = 0; turn < 40 && module.move.phase != L::MovePhase::TRIGGER; ++turn) {
        CHECK(F::service(module, fixture.live, owner, now));
        now += 500;
    }
    CHECK(module.move.phase == L::MovePhase::TRIGGER && !module.outstanding);
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(module.outstanding && owner.pending);
    const unsigned submitted = owner.submitted;
    // Delivery is late. The pending owner terminal must be consumed before local expiry wins.
    now = module.move.deadlineUs;
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(module.resultReady && !module.outstanding && !owner.pending);
    CHECK(owner.submitted == submitted);
    F::MoveResult result;
    CHECK(F::takeResult(module, 4, result));
    CHECK(result.state == M::ActionState::FAILED && result.outcome == M::ActionOutcome::DEADLINE);
    CHECK(result.execution == M::ActionExecution::ACKNOWLEDGED && result.uncertain);
    CHECK(result.completion == M::ActionCompletion::NOT_OBSERVED);
    CHECK(result.wire == F::WireSettlement::SETTLED);
    CHECK(module.move.triggerPresent && module.move.trigger.protocolAcknowledged);
    CHECK(module.move.trigger.completedUs == now);
    return true;
}

bool fakeLatencyCannotWrap() {
    F::FakeOwner owner;
    C::SdoTransfer transfer;
    transfer.direction = C::SdoDirection::DOWNLOAD;
    owner.nextOutcome = C::SdoOutcome::TIMEOUT;
    const F::FakeOwner before = owner;
    CHECK(!owner.admit(transfer, 1, UINT64_MAX - 499));
    CHECK(owner.pending == before.pending && owner.submitted == before.submitted &&
          owner.dueUs == before.dueUs && owner.nextOutcome == before.nextOutcome &&
          owner.terminal.admittedUs == before.terminal.admittedUs);
    CHECK(owner.admit(transfer, 1, UINT64_MAX - 500));
    CHECK(owner.dueUs == UINT64_MAX);
    CHECK(!owner.poll(UINT64_MAX - 1));
    CHECK(owner.poll(UINT64_MAX));
    CHECK(owner.terminal.outcome == C::SdoOutcome::TIMEOUT);
    return true;
}

bool invalidOwnerTimeLeavesTheLaneRecoverable() {
    static F::Fixture fixture;
    static F::MotorModule module;
    static unsigned char savedModule[sizeof module];
    F::FakeOwner owner;
    uint64_t now = 0;
    CHECK(F::initialize(fixture, owner, now));
    CHECK(F::start(module, fixture, now));
    owner.nextOutcome = C::SdoOutcome::TIMEOUT;
    unsigned char savedOwner[sizeof owner];
    std::memcpy(savedModule, &module, sizeof module);
    std::memcpy(savedOwner, &owner, sizeof owner);
    // Genuine production contexts use 0..INT64_MAX. An out-of-domain supplied time cannot create
    // a possible-TX notice or consume fake owner policy while its 500-us latency would wrap.
    CHECK(!owner.canAdmit(UINT64_MAX - 499));
    CHECK(!F::service(module, fixture.live, owner, UINT64_MAX - 499));
    CHECK(std::memcmp(savedModule, &module, sizeof module) == 0);
    CHECK(std::memcmp(savedOwner, &owner, sizeof owner) == 0);
    CHECK(!module.outstanding && !owner.pending && !module.resultReady);
    // Rejected time never entered either clock history. The genuine cutoff can still terminate.
    CHECK(F::service(module, fixture.live, owner, module.move.deadlineUs));
    F::MoveResult result;
    CHECK(F::takeResult(module, 4, result));
    CHECK(result.outcome == M::ActionOutcome::DEADLINE &&
          result.execution == M::ActionExecution::NOT_TRANSMITTED && !result.uncertain);
    return true;
}
} // namespace

int main() {
    if (!preparationIsAtomic() || !correlationAndRetention() || !interruptedTriggerStaysUnknown() ||
        !configurationChangeRejectsUnsentWork() || !expiredUnsentMovePublishesItsTerminal() ||
        !pendingTriggerSettlesAtDeadline() || !fakeLatencyCannotWrap() ||
        !invalidOwnerTimeLeavesTheLaneRecoverable())
        return 1;
    std::printf("FieldCore-like consumer: %u checks\n", checks);
    return 0;
}
