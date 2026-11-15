// SPDX-License-Identifier: MIT
// All three headers must coexist; the proof has no duplicate public value types.
#include <MotorControlCAN/MotorControlCAN.h>
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include "Operations.h"
#include <type_traits>
int main() {
    static_assert(std::is_same<decltype(MotorControlCAN::ProofProfile::MoveContext().execution),
                  MotorControlCAN::ActionExecution>::value, "proof reuses production execution");
    MotorControlCAN::ProofProfile::MoveContext proof;
    MotorControlCAN::LICHUAN_C::MoveContext native;
    return proof.state == MotorControlCAN::ActionState::EMPTY &&
        native.state == MotorControlCAN::ActionState::EMPTY ? 0 : 1;
}
