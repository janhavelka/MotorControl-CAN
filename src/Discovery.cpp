// SPDX-License-Identifier: MIT
#include <MotorControlCAN/Discovery.h>

namespace MotorControlCAN {
std::size_t discoveryProfileCount() noexcept {
    return 1;
}
Status getDiscoveryProfile(std::size_t index, DiscoveryProfile& output) noexcept {
    if (index >= discoveryProfileCount())
        return Status(Err::UNSUPPORTED, 0, "profile is not in the implemented inventory");
    output = DiscoveryProfile();
    return Ok();
}
Status getDiscoveryCapabilities(DriveProfile profile, DiscoveryCapabilities& output) noexcept {
    if (profile != DriveProfile::LICHUAN_C)
        return Status(Err::UNSUPPORTED, 0, "discovery profile is not implemented");
    DiscoveryCapabilities candidate;
    candidate.probe = true;
    candidate.identity = true;
    candidate.minimumAddress = 1;
    candidate.maximumAddress = 127;
    candidate.probeIndex = 0x1000;
    candidate.probeSubindex = 0;
    candidate.probeWidthBytes = 4;
    output = candidate;
    return Ok();
}
} // namespace MotorControlCAN
