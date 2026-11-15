// SPDX-License-Identifier: MIT
#include <MotorControlCAN/ActionOperation.h>
#include <stddef.h>
#include <stdint.h>

namespace MotorControlCAN {
Status checkServiceEvidence(const ServiceEvidence& e, ServiceOutcome& output) noexcept {
    const uintptr_t a = reinterpret_cast<uintptr_t>(&e), b = reinterpret_cast<uintptr_t>(&output);
    const bool overlap = a <= b ? b - a < sizeof e : a - b < sizeof output;
    if (overlap || e.acknowledgement > AcknowledgementSupport::RESPONSE ||
        (e.transmissionPossible && !e.admitted) ||
        (e.transmissionConfirmed && !e.transmissionPossible) ||
        ((e.acknowledged || e.rejected || e.completionObserved) && !e.transmissionPossible) ||
        (e.acknowledged && e.rejected) ||
        ((e.acknowledged || e.rejected) && e.acknowledgement != AcknowledgementSupport::RESPONSE) ||
        (e.rejected && e.completionObserved))
        return Status(Err::INVALID_CONFIG, int32_t(ActionError::ARGUMENT), "inconsistent service evidence");
    ServiceOutcome candidate;
    candidate.execution = e.rejected ? ActionExecution::REJECTED :
        e.acknowledged ? ActionExecution::ACKNOWLEDGED :
        e.transmissionPossible ? ActionExecution::UNKNOWN : ActionExecution::NOT_TRANSMITTED;
    candidate.completion = e.completionObserved ? ActionCompletion::OBSERVED : ActionCompletion::NOT_OBSERVED;
    output = candidate;
    return Ok();
}
} // namespace MotorControlCAN
