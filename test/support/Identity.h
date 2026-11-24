// SPDX-License-Identifier: MIT
// A synthetic current identity observation for a test whose fake drive answers no identity read. It
// holds the measured CL86-C values (measuredIdentity()) for the given target, with a one-instant
// window. checkIdentity judges only the target, the window and the values, so nothing here is a read
// of any drive, and a getter-produced observation stays the path an application uses.
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>

namespace TestIdentity {
inline MotorControlCAN::LICHUAN_C::IdentityObservation
measured(const MotorControlCAN::LICHUAN_C::ReadTarget& target, uint64_t atUs) {
    const MotorControlCAN::LICHUAN_C::IdentityValues v = MotorControlCAN::LICHUAN_C::measuredIdentity();
    MotorControlCAN::LICHUAN_C::IdentityObservation o;
    o.target = target;
    o.deviceType = v.deviceType;
    o.hardwareVersion = v.hardwareVersion;
    o.hardwareVersionWidth = 1;
    o.softwareVersion = v.softwareVersion;
    o.softwareVersionWidth = 1;
    o.identityEntries = 3;
    o.vendorId = v.vendorId;
    o.productCode = v.productCode;
    o.revision = v.revision;
    o.modelCode = v.modelCode;
    o.firmwareCode = v.firmwareCode;
    o.window.earliestUs = o.window.latestUs = atUs;
    return o;
}
} // namespace TestIdentity
