// SPDX-License-Identifier: MIT
#include "FakeOwner.h"
#include <cstdio>

int main() {
    namespace F = FieldCoreConsumer;
    // Large caller-owned contexts remain stationary; no SDK, controller, stack or clock is opened.
    static F::Fixture fixture;
    static F::MotorModule module;
    F::FakeOwner owner;
    uint64_t nowUs = 0;
    if (!F::initialize(fixture, owner, nowUs) || !F::start(module, fixture, nowUs))
        return 1;
    for (unsigned turn = 0; turn < 200 && !module.resultReady; ++turn) {
        if (!F::service(module, fixture.live, owner, nowUs))
            return 2;
        nowUs += 500;
    }
    F::MoveResult result;
    if (!F::takeResult(module, 4, result))
        return 3;
    std::printf("Synthetic consumer: axis %lu, target %ld, admitted=%u, wire=%u, execution=%u, "
                "completion=%u, uncertain=%u\n",
                static_cast<unsigned long>(result.binding.logicalAxisId),
                static_cast<long>(result.effectiveTarget), unsigned(result.admission),
                unsigned(result.wire), unsigned(result.execution), unsigned(result.completion),
                unsigned(result.uncertain));
    std::puts("Controller-report simulation only; no FieldCore integration or hardware qualification.");
    return result.state == F::M::ActionState::SUCCEEDED &&
                   result.completion == F::M::ActionCompletion::OBSERVED ? 0 : 4;
}
