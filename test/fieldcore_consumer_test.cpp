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
    CHECK(!owner.admit(transfer, 1, uint64_t(INT64_MAX) - 499));
    CHECK(owner.pending == before.pending && owner.submitted == before.submitted &&
          owner.dueUs == before.dueUs && owner.nextOutcome == before.nextOutcome &&
          owner.terminal.admittedUs == before.terminal.admittedUs);
    CHECK(owner.admit(transfer, 1, uint64_t(INT64_MAX) - 500));
    CHECK(owner.dueUs == uint64_t(INT64_MAX));
    CHECK(!owner.poll(uint64_t(INT64_MAX) - 1));
    CHECK(owner.poll(uint64_t(INT64_MAX)));
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
    // Reject the first invalid domain value as well as arithmetic near uint64 overflow.
    const uint64_t invalid[] = {uint64_t(INT64_MAX) + 1, UINT64_MAX - 499, UINT64_MAX};
    for (uint64_t time : invalid) {
        CHECK(!owner.canAdmit(time));
        CHECK(!F::service(module, fixture.live, owner, time));
        CHECK(std::memcmp(savedModule, &module, sizeof module) == 0);
        CHECK(std::memcmp(savedOwner, &owner, sizeof owner) == 0);
    }
    CHECK(!module.outstanding && !owner.pending && !module.resultReady);
    // Rejected time never entered either clock history. The genuine cutoff can still terminate.
    CHECK(F::service(module, fixture.live, owner, module.move.deadlineUs));
    F::MoveResult result;
    CHECK(F::takeResult(module, 4, result));
    CHECK(result.outcome == M::ActionOutcome::DEADLINE &&
          result.execution == M::ActionExecution::NOT_TRANSMITTED && !result.uncertain);
    return true;
}

bool invalidTimePreservesOutputsAndPendingTerminal() {
    static F::Fixture fixture;
    static F::MotorModule module;
    static unsigned char saved[sizeof module];
    F::FakeOwner owner;
    uint64_t now = 0;
    const uint64_t invalid = uint64_t(INT64_MAX) + 1;
    CHECK(F::validateTime(0) && F::validateTime(uint64_t(INT64_MAX)));
    CHECK(!F::validateTime(invalid));
    CHECK(F::initialize(fixture, owner, now));
    std::memcpy(saved, &module, sizeof module);
    CHECK(!F::start(module, fixture, invalid));
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    CHECK(!F::prepareMove(module, fixture.live, fixture.stateRead, fixture.control,
                         fixture.identity, fixture.evidence, fixture.request, fixture.profile,
                         fixture.options, 4, now, invalid));
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    CHECK(F::start(module, fixture, now));
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(module.outstanding && owner.pending && !owner.ready);
    std::memcpy(saved, &module, sizeof module);
    unsigned char savedOwner[sizeof owner];
    std::memcpy(savedOwner, &owner, sizeof owner);
    L::PreparedMove work;
    work.ready = true;
    work.eligibleUs = 123;
    unsigned char savedWork[sizeof work];
    std::memcpy(savedWork, &work, sizeof work);
    C::SdoTransfer transfer;
    transfer.operation = 999;
    unsigned char savedTransfer[sizeof transfer];
    std::memcpy(savedTransfer, &transfer, sizeof transfer);
    CHECK(!F::nextStep(module, fixture.live, invalid, work));
    CHECK(std::memcmp(savedWork, &work, sizeof work) == 0);
    CHECK(!F::admitStep(module, fixture.live, invalid, transfer));
    CHECK(std::memcmp(savedTransfer, &transfer, sizeof transfer) == 0);
    CHECK(!F::interrupt(module, fixture.live, M::MoveInterruption::CANCELLED, invalid));
    CHECK(!F::advance(module, fixture.live, owner.terminal, owner.wire, invalid));
    CHECK(!owner.poll(invalid));
    CHECK(!F::service(module, fixture.live, owner, invalid));
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    CHECK(std::memcmp(savedOwner, &owner, sizeof owner) == 0);
    now += 500;
    CHECK(F::service(module, fixture.live, owner, now));
    CHECK(!module.outstanding && !owner.pending && module.move.state == M::ActionState::ACTIVE);
    // The final valid timestamp can retire expired unsent work without reserving fake latency.
    CHECK(F::service(module, fixture.live, owner, uint64_t(INT64_MAX)));
    CHECK(module.resultReady && module.result.outcome == M::ActionOutcome::DEADLINE);
    const unsigned submitted = owner.submitted;
    std::memcpy(saved, &module, sizeof module);
    CHECK(F::service(module, fixture.live, owner, uint64_t(INT64_MAX)));
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    F::MoveResult result;
    CHECK(F::takeResult(module, 4, result));
    std::memcpy(saved, &module, sizeof module);
    CHECK(F::service(module, fixture.live, owner, uint64_t(INT64_MAX)));
    CHECK(std::memcmp(saved, &module, sizeof module) == 0);
    CHECK(owner.submitted == submitted && !owner.pending);
    return true;
}

bool invalidatedPendingExchangeRetires() {
    // Each phase owns a different kind of evidence. Changing actual ownership must require
    // explicit withdrawal before any original terminal can be consumed, even at the cutoff.
    const L::MovePhase phases[] = {L::MovePhase::PARAMETERS, L::MovePhase::TRIGGER,
                                   L::MovePhase::OBSERVING};
    static F::Fixture fixture;
    static F::MotorModule module;
    static unsigned char saved[sizeof module];
    for (unsigned phase = 0; phase < 3; ++phase) {
        for (unsigned expired = 0; expired < 2; ++expired) {
            fixture = F::Fixture();
            module = F::MotorModule();
            F::FakeOwner owner;
            uint64_t now = 0;
            CHECK(F::initialize(fixture, owner, now));
            CHECK(F::start(module, fixture, now));
            for (unsigned turn = 0;
                 turn < 80 && !(module.outstanding && module.move.phase == phases[phase]); ++turn) {
                CHECK(F::service(module, fixture.live, owner, now));
                now += 500;
            }
            CHECK(module.outstanding && owner.pending && module.move.phase == phases[phase]);
            // Exercise distinct live changes without editing the frozen admitted identity.
            if (phase == 0) {
                ++fixture.live.target.channel.binding.bindingGeneration;
                fixture.live.reservation.target = fixture.live.target;
                fixture.live.axis.binding = fixture.live.target.channel.binding;
            } else if (phase == 1) {
                ++fixture.live.reservation.sequence;
            } else {
                ++fixture.live.target.nodeGeneration;
                fixture.live.reservation.target = fixture.live.target;
            }
            now = expired ? module.move.deadlineUs : now;
            CHECK(owner.poll(now));
            std::memcpy(saved, &module, sizeof module);
            CHECK(!F::advance(module, fixture.live, owner.terminal, owner.wire, now));
            CHECK(std::memcmp(saved, &module, sizeof module) == 0);
            CHECK(F::interrupt(module, fixture.live, M::MoveInterruption::INVALIDATED, now));
            CHECK(!module.resultReady && module.outstanding);
            L::PreparedMove work;
            CHECK(F::nextStep(module, fixture.live, now, work));
            CHECK(!work.ready && !work.observe);
            C::SdoTransfer forbidden;
            CHECK(!F::admitStep(module, fixture.live, now, forbidden));
            C::SdoCompletion wrong = owner.terminal;
            ++wrong.nodeGeneration;
            std::memcpy(saved, &module, sizeof module);
            CHECK(!F::advance(module, fixture.live, wrong, owner.wire, now));
            CHECK(std::memcmp(saved, &module, sizeof module) == 0);
            const unsigned submitted = owner.submitted;
            CHECK(F::service(module, fixture.live, owner, now));
            CHECK(module.resultReady && !module.outstanding && !owner.pending);
            CHECK(owner.submitted == submitted);
            F::MoveResult result;
            CHECK(F::takeResult(module, 4, result));
            CHECK(result.state == M::ActionState::FAILED);
            CHECK(result.outcome == (expired ? M::ActionOutcome::DEADLINE
                                             : M::ActionOutcome::INVALIDATED));
            CHECK(result.uncertain == (phase != 0));
            CHECK(result.wire == F::WireSettlement::SETTLED);
            CHECK(result.completion == M::ActionCompletion::NOT_OBSERVED);
            if (phase == 0) {
                CHECK(module.move.setupEvidenceMask == 1);
                CHECK(module.move.setupHistory[0].protocolAcknowledged);
                CHECK(result.execution == M::ActionExecution::NOT_TRANSMITTED);
            } else {
                CHECK(result.execution == M::ActionExecution::ACKNOWLEDGED);
                CHECK(module.move.triggerPresent && module.move.trigger.protocolAcknowledged);
                if (phase == 2) {
                    CHECK(module.move.observationEvidencePresent);
                    CHECK(module.move.observationEvidence.transfer.index == 0x2100);
                }
            }
            CHECK(!F::service(module, fixture.live, owner, now));
            CHECK(owner.submitted == submitted); // No replay or admission on the frozen route.
        }
    }
    return true;
}
} // namespace

int main() {
    if (!preparationIsAtomic() || !correlationAndRetention() || !interruptedTriggerStaysUnknown() ||
        !configurationChangeRejectsUnsentWork() || !expiredUnsentMovePublishesItsTerminal() ||
        !pendingTriggerSettlesAtDeadline() || !fakeLatencyCannotWrap() ||
        !invalidOwnerTimeLeavesTheLaneRecoverable() ||
        !invalidTimePreservesOutputsAndPendingTerminal() || !invalidatedPendingExchangeRetires())
        return 1;
    std::printf("FieldCore-like consumer: %u checks\n", checks);
    return 0;
}
