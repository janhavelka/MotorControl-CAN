// SPDX-License-Identifier: MIT
#include <MotorControlCAN/Capabilities.h>
#include <MotorControlCAN/Native.h>
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/can/Frame.h>
int main() {
    MotorControlCAN::Capabilities facts;
    facts.motor = MotorControlCAN::MotorKind::BLDC;
    facts.feedback = MotorControlCAN::FeedbackKind::ENCODER;
    facts.velocity = true;
    MotorControlCAN::ServiceEvidence evidence;
    MotorControlCAN::ServiceOutcome outcome;
    return MotorControlCAN::requireCapability(facts, MotorControlCAN::Capability::VELOCITY) &&
        MotorControlCAN::checkNativeInteger(70000, 4, true) &&
        MotorControlCAN::checkServiceEvidence(evidence, outcome) ? 0 : 1;
}
