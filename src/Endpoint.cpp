// SPDX-License-Identifier: MIT
#include <MotorControlCAN/Endpoint.h>

namespace MotorControlCAN {

bool sameBinding(const Binding& a, const Binding& b) noexcept {
    return a.logicalAxisId == b.logicalAxisId &&
        a.bindingGeneration == b.bindingGeneration &&
        a.configurationGeneration == b.configurationGeneration &&
        a.endpoint.protocol == b.endpoint.protocol && a.endpoint.bus == b.endpoint.bus &&
        a.endpoint.profile == b.endpoint.profile && a.endpoint.address == b.endpoint.address &&
        a.endpoint.channel == b.endpoint.channel && a.endpoint.subaxis == b.endpoint.subaxis;
}

Status validateBinding(const Binding& binding) noexcept {
    if (!binding.logicalAxisId || !binding.endpoint.profile || !binding.bindingGeneration ||
        !binding.configurationGeneration ||
        (binding.endpoint.protocol != Protocol::CANOPEN &&
         binding.endpoint.protocol != Protocol::PROPRIETARY)) {
        return Status(Err::INVALID_CONFIG, static_cast<int32_t>(BindingError::INVALID_TARGET),
                      "invalid common binding");
    }
    return Ok();
}

} // namespace MotorControlCAN
