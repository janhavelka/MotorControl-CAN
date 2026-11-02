// SPDX-License-Identifier: MIT
// Independently selected original EDS/manual facts, never generated expectations.
#include <MotorControlCAN/profiles/lichuan_c/Objects.h>
#include <MotorControlCAN/canopen/Sdo.h>
#include <MotorControlCAN/canopen/SdoTransfer.h>
#include <type_traits>

namespace L = MotorControlCAN::LICHUAN_C;
namespace C = MotorControlCAN::CANOPEN;

static_assert(L::OBJECT_1009_00.type == L::ScalarType::U16 &&
    L::OBJECT_100A_00.widthBytes == 2, "Lichuan versions are U16 codes, not strings");
static_assert(L::OBJECT_1018_00.type == L::ScalarType::U8 &&
    L::OBJECT_2400_00.type == L::ScalarType::U16, "count widths are independent");
static_assert(L::OBJECT_1600_00.sdoAccess == L::SdoAccess::READ_WRITE &&
    L::OBJECT_1600_00.writePolicy == L::WritePolicy::QUALIFICATION_REQUIRED,
    "mapping counts are gated settings, not universally read-only");
static_assert(L::OBJECT_6040_00.sdoAccess == L::SdoAccess::WRITE_ONLY &&
    L::OBJECT_6040_00.pdoPolicy == L::PdoPolicy::UNQUALIFIED,
    "RPDO source membership does not add SDO reads or approve mapping");
static_assert(L::OBJECT_2300_00.index == 0x2300 &&
    L::OBJECT_2300_00.writePolicy == L::WritePolicy::QUALIFICATION_REQUIRED &&
    L::OBJECT_2300_00.ownerPrompt == 39 &&
    (L::OBJECT_2300_00.gateMask & (1u << 14)),
    "Lichuan save/reset is profile scoped and retains Q15, not NanoJ control");
// DEC-1, checked by hand against the audit list. Acceptance is risk acceptance and grants no writes.
// 6061 has no object constant here because its generation type is withheld; reads_test covers it.
static_assert(L::OBJECT_1009_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_1009_00.writePolicy == L::WritePolicy::FORBIDDEN,
    "an accepted RO identity row polls under accepted risk and is still not writable");
static_assert(L::OBJECT_1000_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_100A_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_1018_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_1018_01.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_1018_02.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_1018_03.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_2000_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_2001_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_2100_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_2101_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_2102_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_6041_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_6064_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED &&
    L::OBJECT_606C_00.readEffect == L::ReadEffect::ACCEPTED_UNQUALIFIED,
    "the remaining DEC-1 rows with object constants");
static_assert(L::OBJECT_603F_00.readEffect == L::ReadEffect::UNQUALIFIED &&
    L::OBJECT_1001_00.readEffect == L::ReadEffect::UNQUALIFIED &&
    L::OBJECT_1003_00.readEffect == L::ReadEffect::UNQUALIFIED &&
    L::OBJECT_1003_01.readEffect == L::ReadEffect::UNQUALIFIED &&
    L::OBJECT_1005_00.readEffect == L::ReadEffect::UNQUALIFIED &&
    L::OBJECT_2200_00.readEffect == L::ReadEffect::UNQUALIFIED &&
    L::OBJECT_60FD_00.readEffect == L::ReadEffect::UNQUALIFIED,
    "fault, history and every other unlisted read stays explicit and unqualified");
// Neither state can claim an absent effect: the profile vocabulary has no such member.
template <typename E> struct StatesAbsentEffect {
    template <typename T> static auto test(int) -> decltype(T::REVIEWED_NON_CHANGING, std::true_type());
    template <typename> static std::false_type test(...);
    static constexpr bool value = decltype(test<E>(0))::value;
};
static_assert(!StatesAbsentEffect<L::ReadEffect>::value &&
    StatesAbsentEffect<C::SdoReadEffect>::value,
    "the profile vocabulary cannot state an absent effect; this probe does detect the service member");
static_assert(static_cast<unsigned>(L::ReadEffect::UNQUALIFIED) == 0 &&
    static_cast<unsigned>(L::ReadEffect::ACCEPTED_UNQUALIFIED) == 1,
    "a value-initialized read effect stays unqualified");
static_assert(L::OBJECT_607A_00.type == L::ScalarType::I32 &&
    L::OBJECT_607A_00.signedValue && L::OBJECT_609A_00.widthBytes == 4,
    "reviewed I32 normalization and U32 ramp time retain native widths");

int main() {
    // Representation can feed pure envelope checks; it still grants no I/O.
    C::SdoExpectation expected;
    expected.channel.binding.logicalAxisId = 1;
    expected.channel.binding.endpoint.profile = 1;
    expected.channel.binding.endpoint.address = 5;
    expected.channel.binding.bindingGeneration = expected.channel.binding.configurationGeneration = 1;
    expected.channel.clientToServerId = 0x605; expected.channel.serverToClientId = 0x585;
    expected.index = L::OBJECT_6041_00.index;
    expected.subindex = L::OBJECT_6041_00.subindex;
    expected.width = L::OBJECT_6041_00.widthBytes;
    MotorControlCAN::Frame reply;
    reply.id = 0x585; reply.dlc = reply.payloadLength = 8;
    reply.data[0] = 0x4b; reply.data[1] = 0x41; reply.data[2] = 0x60;
    reply.data[4] = 0x34; reply.data[5] = 0x12;
    C::SdoReply checked;
    uint32_t value = 0;
    return C::checkReply(reply, expected, checked) &&
        C::readWord(checked.data, checked.length, expected.width, value) && value == 0x1234 ? 0 : 1;
}
