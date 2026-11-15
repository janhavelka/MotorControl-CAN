// SPDX-License-Identifier: MIT
#include "MotorModule.h"

namespace FieldCoreConsumer {
namespace {
M::Status unavailable() {
    return M::Status(M::Err::INVALID_CONFIG, 0, "consumer lane or step unavailable");
}
void projectTerminal(MotorModule& module) {
    const auto& move = module.move;
    if (move.state == M::ActionState::ACTIVE)
        return;
    auto& result = module.result;
    result.state = move.state;
    result.outcome = move.outcome;
    result.setup = move.setup;
    result.execution = move.execution;
    result.completion = move.completion;
    result.status = move.status;
    result.uncertain = move.uncertain;
    module.resultReady = true;
}
} // namespace

M::Status prepareMove(MotorModule& module, const LiveAxis& live, const L::ReadContext& state,
                      const C::SdoCompletion& control, const L::IdentityObservation& identity,
                      const L::MoveEvidence& evidence, const M::MoveRequest& request,
                      const L::MoveProfile& profile, const L::MoveOptions& options,
                      uint32_t operation, uint64_t nowUs, uint64_t deadlineUs) {
    if (module.outstanding || module.resultReady || module.move.state == M::ActionState::ACTIVE)
        return unavailable();
    const M::Status prepared = L::prepareMoveAbsolute(
        module.move, live.reservation, live.target, state, control, identity, evidence, request,
        profile, nullptr, live.axis, nullptr, operation, nowUs, deadlineUs, options);
    if (!prepared)
        return prepared;
    module.result = MoveResult();
    module.result.operation = operation;
    module.result.binding = live.target.channel.binding;
    module.result.admission = Admission::ACCEPTED;
    module.result.effectiveTarget = module.move.parameters.targetNative;
    return M::Ok();
}

M::Status nextStep(MotorModule& module, const LiveAxis& live, uint64_t nowUs,
                   L::PreparedMove& output) {
    if (module.outstanding || module.resultReady) {
        output = L::PreparedMove();
        return M::Ok();
    }
    L::PreparedMove work;
    M::Status status = L::nextMove(module.move, module.observation, live.reservation, live.target,
                                  nowUs, work);
    if (!status)
        return status;
    if (work.observe) {
        status = L::beginMoveObservation(module.move, module.observation, live.reservation,
                                         live.target, nowUs);
        if (!status)
            return status;
        status = L::nextMove(module.move, module.observation, live.reservation, live.target, nowUs,
                             work);
        if (!status)
            return status;
    }
    output = work;
    return M::Ok();
}

M::Status admitStep(MotorModule& module, const LiveAxis& live, uint64_t nowUs,
                    C::SdoTransfer& ownerCopy) {
    L::PreparedMove work;
    const M::Status yielded = nextStep(module, live, nowUs, work);
    if (!yielded)
        return yielded;
    if (!work.ready)
        return unavailable();
    if (work.transfer.direction == C::SdoDirection::DOWNLOAD) {
        const M::Status noted = L::noteMoveTransmission(module.move, live.reservation, live.target,
                                                       live.axis, nullptr, nullptr, 0, nowUs);
        if (!noted)
            return noted;
    }
    ownerCopy = work.transfer;
    module.outstanding = true;
    module.result.wire = WireSettlement::UNKNOWN;
    return M::Ok();
}

M::Status advance(MotorModule& module, const LiveAxis& live, const C::SdoCompletion& completion,
                  WireSettlement wire, uint64_t nowUs) {
    if (!module.outstanding)
        return unavailable();
    const M::Status consumed = L::advanceMove(module.move, module.observation, live.reservation,
                                             live.target, completion, nowUs);
    if (!consumed)
        return consumed;
    module.outstanding = false;
    module.result.wire = wire;
    projectTerminal(module);
    return M::Ok();
}

M::Status interrupt(MotorModule& module, const LiveAxis& live, M::MoveInterruption reason,
                    uint64_t nowUs) {
    if (module.outstanding)
        return L::noteMoveWithdrawal(module.move, live.reservation, live.target, reason, nowUs);
    const M::Status consumed = L::advanceMove(module.move, module.observation, live.reservation,
                                             live.target, reason, nowUs);
    if (consumed)
        projectTerminal(module);
    return consumed;
}

bool takeResult(MotorModule& module, uint32_t operation, MoveResult& output) {
    if (!module.resultReady || operation != module.result.operation)
        return false;
    output = module.result;
    module.resultReady = false;
    return true;
}
} // namespace FieldCoreConsumer
