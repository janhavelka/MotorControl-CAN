// SPDX-License-Identifier: MIT
// Private to this profile's sources; never installed. One row per native read group, so adding a
// group is a table row plus its getter's field mapping.
#pragma once
#include "ProfileEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace Detail {
struct Address {
    uint16_t index;
    uint8_t subindex;
};
// Which typed getter publishes a group.
enum class GroupFamily : uint8_t { DRIVER, IO, CONTROL, TUNING };
struct GroupPlan {
    DriverGroup group;
    ReadKind kind;
    GroupFamily family;
    TuningGroup tuning; // Meaningful for the TUNING family only.
    const Address* fields;
    uint8_t count;
};
// Null for an unknown group, or a kind that is not a native group. Defined in DriverReads.cpp.
const GroupPlan* groupPlan(DriverGroup group) noexcept;
const GroupPlan* groupPlan(ReadKind kind) noexcept;

inline Status readInvalid(ReadError detail, const char* message) noexcept {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(detail), message);
}
// Prepare any finite read, leaving output untouched on refusal. Defined in Reads.cpp.
Status prepareRead(ReadContext& output, const ReadTarget& target, uint32_t operation, uint64_t now,
                   uint64_t deadline, const ReadOptions& options, ReadKind kind, uint8_t mask,
                   const CANOPEN::PdoRoute* route) noexcept;
// The entry shared by every whole-group getter, publishing nothing. On OK every field is present,
// current and within age, and window bounds them. Partial state blocks use validateStateBlock.
Status beginGet(const ReadContext& c, const void* output, size_t outputSize,
                const ReadTarget& target, uint64_t now, uint64_t age, bool kindMatches,
                ReadWindow& window) noexcept;
inline bool inFamily(const ReadContext& c, GroupFamily family) noexcept {
    const GroupPlan* plan = groupPlan(c.kind);
    return plan && plan->family == family;
}
inline void copyProvenance(const ReadContext& c, ReadProvenance* output) noexcept {
    for (unsigned i = 0; i < c.count; ++i)
        output[i] = c.values[i].provenance;
}
inline int32_t signed32(uint32_t raw) noexcept {
    return static_cast<int32_t>(raw <= INT32_MAX ? int64_t(raw)
                                                 : int64_t(raw) - INT64_C(4294967296));
}
} // namespace Detail
} // namespace LICHUAN_C
} // namespace MotorControlCAN
