// SPDX-License-Identifier: MIT
/**
 * @file Objects.h
 * @brief Compact resolved native declarations; no action routes or catalogue.
 * Generated from docs/reference/lichuan_objects.json. Do not edit.
 * Regenerate with: python scripts/generate_lichuan_objects.py
 */
#pragma once

#include "MotorControlCAN/profiles/lichuan_c/Types.h"

namespace MotorControlCAN {
namespace LICHUAN_C {

/** @brief 0x1000:00 U32 value declaration; EDS [1000] lines 158-166.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1000_00 = {
    0x1000, 0x00, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1001:00 U8 value declaration; EDS [1001] lines 167-175.
 * No operation is implemented or authorized; gates Q01, Q18, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1001_00 = {
    0x1001, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x000A0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1003:00 U8 count declaration; EDS [1003sub0] lines 182-192.
 * No operation is implemented or authorized; gates Q01, Q18, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1003_00 = {
    0x1003, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x000A0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1003:01 U32 value declaration; EDS [1003sub1] lines 193-200.
 * No operation is implemented or authorized; gates Q01, Q18, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1003_01 = {
    0x1003, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x000A0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1003:02 U32 value declaration; EDS [1003sub2] lines 201-208.
 * No operation is implemented or authorized; gates Q01, Q18, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1003_02 = {
    0x1003, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x000A0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1003:03 U32 value declaration; EDS [1003sub3] lines 209-216.
 * No operation is implemented or authorized; gates Q01, Q18, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1003_03 = {
    0x1003, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x000A0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1003:04 U32 value declaration; EDS [1003sub4] lines 217-224.
 * No operation is implemented or authorized; gates Q01, Q18, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1003_04 = {
    0x1003, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x000A0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1005:00 U32 value declaration; EDS [1005] lines 225-233.
 * No operation is implemented or authorized; gates Q01, Q16, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1005_00 = {
    0x1005, 0x00, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    38, 0x00088001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1009:00 U16 value declaration; EDS [1009] lines 234-242.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1009_00 = {
    0x1009, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x100A:00 U16 value declaration; EDS [100A] lines 243-251.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_100A_00 = {
    0x100A, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1014:00 U32 value declaration; EDS [1014] lines 252-260.
 * No operation is implemented or authorized; gates Q01, Q16, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1014_00 = {
    0x1014, 0x00, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    38, 0x00088001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1017:00 U16 value declaration; EDS [1017] lines 261-269.
 * No operation is implemented or authorized; gates Q01, Q18, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1017_00 = {
    0x1017, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    38, 0x000A0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1018:00 U8 count declaration; EDS [1018sub0] lines 276-286.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1018_00 = {
    0x1018, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1018:01 U32 value declaration; EDS [1018sub1] lines 287-295.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1018_01 = {
    0x1018, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1018:02 U32 value declaration; EDS [1018sub2] lines 296-304.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1018_02 = {
    0x1018, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1018:03 U32 value declaration; EDS [1018sub3] lines 305-313.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1018_03 = {
    0x1018, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1200:00 U8 count declaration; EDS [1200sub0] lines 320-328.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1200_00 = {
    0x1200, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    13, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1200:01 U32 value declaration; EDS [1200sub1] lines 329-337.
 * No operation is implemented or authorized; gates Q01, Q16, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1200_01 = {
    0x1200, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    38, 0x00088001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1200:02 U32 value declaration; EDS [1200sub2] lines 338-346.
 * No operation is implemented or authorized; gates Q01, Q16, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1200_02 = {
    0x1200, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    38, 0x00088001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1400:00 U8 count declaration; EDS [1400sub0] lines 353-361.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1400_00 = {
    0x1400, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    17, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1400:01 U32 value declaration; EDS [1400sub1] lines 362-370.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1400_01 = {
    0x1400, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1400:02 U8 value declaration; EDS [1400sub2] lines 371-379.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1400_02 = {
    0x1400, 0x02, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1400:03 U16 value declaration; EDS [1400sub3] lines 380-388.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1400_03 = {
    0x1400, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1400:04 U8 value declaration; EDS [1400sub4] lines 389-397.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1400_04 = {
    0x1400, 0x04, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1400:05 U16 value declaration; EDS [1400sub5] lines 398-406.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1400_05 = {
    0x1400, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1401:00 U8 count declaration; EDS [1401sub0] lines 413-421.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1401_00 = {
    0x1401, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    17, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1401:01 U32 value declaration; EDS [1401sub1] lines 422-430.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1401_01 = {
    0x1401, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1401:02 U8 value declaration; EDS [1401sub2] lines 431-439.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1401_02 = {
    0x1401, 0x02, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1401:03 U16 value declaration; EDS [1401sub3] lines 440-448.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1401_03 = {
    0x1401, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1401:04 U8 value declaration; EDS [1401sub4] lines 449-457.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1401_04 = {
    0x1401, 0x04, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1401:05 U16 value declaration; EDS [1401sub5] lines 458-466.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1401_05 = {
    0x1401, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1402:00 U8 count declaration; EDS [1402sub0] lines 473-481.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1402_00 = {
    0x1402, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    17, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1402:01 U32 value declaration; EDS [1402sub1] lines 482-490.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1402_01 = {
    0x1402, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1402:02 U8 value declaration; EDS [1402sub2] lines 491-499.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1402_02 = {
    0x1402, 0x02, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1402:03 U16 value declaration; EDS [1402sub3] lines 500-508.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1402_03 = {
    0x1402, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1402:04 U8 value declaration; EDS [1402sub4] lines 509-517.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1402_04 = {
    0x1402, 0x04, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1402:05 U16 value declaration; EDS [1402sub5] lines 518-526.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1402_05 = {
    0x1402, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1403:00 U8 count declaration; EDS [1403sub0] lines 533-541.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1403_00 = {
    0x1403, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    17, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1403:01 U32 value declaration; EDS [1403sub1] lines 542-550.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1403_01 = {
    0x1403, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1403:02 U8 value declaration; EDS [1403sub2] lines 551-559.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1403_02 = {
    0x1403, 0x02, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1403:03 U16 value declaration; EDS [1403sub3] lines 560-568.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1403_03 = {
    0x1403, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1403:04 U8 value declaration; EDS [1403sub4] lines 569-577.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1403_04 = {
    0x1403, 0x04, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1403:05 U16 value declaration; EDS [1403sub5] lines 578-586.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1403_05 = {
    0x1403, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1600:00 U8 count declaration; EDS [1600sub0] lines 593-603.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1600_00 = {
    0x1600, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1600:01 U32 value declaration; EDS [1600sub1] lines 604-612.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1600_01 = {
    0x1600, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1600:02 U32 value declaration; EDS [1600sub2] lines 613-621.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1600_02 = {
    0x1600, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1600:03 U32 value declaration; EDS [1600sub3] lines 622-630.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1600_03 = {
    0x1600, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1600:04 U32 value declaration; EDS [1600sub4] lines 631-638.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1600_04 = {
    0x1600, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1601:00 U8 count declaration; EDS [1601sub0] lines 645-655.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1601_00 = {
    0x1601, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1601:01 U32 value declaration; EDS [1601sub1] lines 656-664.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1601_01 = {
    0x1601, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1601:02 U32 value declaration; EDS [1601sub2] lines 665-673.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1601_02 = {
    0x1601, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1601:03 U32 value declaration; EDS [1601sub3] lines 674-681.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1601_03 = {
    0x1601, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1601:04 U32 value declaration; EDS [1601sub4] lines 682-689.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1601_04 = {
    0x1601, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1602:00 U8 count declaration; EDS [1602sub0] lines 696-706.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1602_00 = {
    0x1602, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1602:01 U32 value declaration; EDS [1602sub1] lines 707-714.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1602_01 = {
    0x1602, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1602:02 U32 value declaration; EDS [1602sub2] lines 715-722.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1602_02 = {
    0x1602, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1602:03 U32 value declaration; EDS [1602sub3] lines 723-730.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1602_03 = {
    0x1602, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1602:04 U32 value declaration; EDS [1602sub4] lines 731-738.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1602_04 = {
    0x1602, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1603:00 U8 count declaration; EDS [1603sub0] lines 745-755.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1603_00 = {
    0x1603, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1603:01 U32 value declaration; EDS [1603sub1] lines 756-763.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1603_01 = {
    0x1603, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1603:02 U32 value declaration; EDS [1603sub2] lines 764-771.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1603_02 = {
    0x1603, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1603:03 U32 value declaration; EDS [1603sub3] lines 772-779.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1603_03 = {
    0x1603, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1603:04 U32 value declaration; EDS [1603sub4] lines 780-787.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1603_04 = {
    0x1603, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1800:00 U8 count declaration; EDS [1800sub0] lines 794-802.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1800_00 = {
    0x1800, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    17, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1800:01 U32 value declaration; EDS [1800sub1] lines 803-811.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1800_01 = {
    0x1800, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1800:02 U8 value declaration; EDS [1800sub2] lines 812-820.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1800_02 = {
    0x1800, 0x02, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1800:03 U16 value declaration; EDS [1800sub3] lines 821-829.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1800_03 = {
    0x1800, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1800:04 U8 value declaration; EDS [1800sub4] lines 830-838.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1800_04 = {
    0x1800, 0x04, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1800:05 U16 value declaration; EDS [1800sub5] lines 839-847.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1800_05 = {
    0x1800, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1801:00 U8 count declaration; EDS [1801sub0] lines 854-862.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1801_00 = {
    0x1801, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    17, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1801:01 U32 value declaration; EDS [1801sub1] lines 863-871.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1801_01 = {
    0x1801, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1801:02 U8 value declaration; EDS [1801sub2] lines 872-880.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1801_02 = {
    0x1801, 0x02, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1801:03 U16 value declaration; EDS [1801sub3] lines 881-889.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1801_03 = {
    0x1801, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1801:04 U8 value declaration; EDS [1801sub4] lines 890-898.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1801_04 = {
    0x1801, 0x04, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1801:05 U16 value declaration; EDS [1801sub5] lines 899-907.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1801_05 = {
    0x1801, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1802:00 U8 count declaration; EDS [1802sub0] lines 914-922.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1802_00 = {
    0x1802, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    17, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1802:01 U32 value declaration; EDS [1802sub1] lines 923-931.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1802_01 = {
    0x1802, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1802:02 U8 value declaration; EDS [1802sub2] lines 932-940.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1802_02 = {
    0x1802, 0x02, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1802:03 U16 value declaration; EDS [1802sub3] lines 941-949.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1802_03 = {
    0x1802, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1802:04 U8 value declaration; EDS [1802sub4] lines 950-958.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1802_04 = {
    0x1802, 0x04, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1802:05 U16 value declaration; EDS [1802sub5] lines 959-967.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1802_05 = {
    0x1802, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1803:00 U8 count declaration; EDS [1803sub0] lines 974-982.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1803_00 = {
    0x1803, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    17, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1803:01 U32 value declaration; EDS [1803sub1] lines 983-991.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1803_01 = {
    0x1803, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1803:02 U8 value declaration; EDS [1803sub2] lines 992-1000.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1803_02 = {
    0x1803, 0x02, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1803:03 U16 value declaration; EDS [1803sub3] lines 1001-1009.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1803_03 = {
    0x1803, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1803:04 U8 value declaration; EDS [1803sub4] lines 1010-1018.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1803_04 = {
    0x1803, 0x04, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1803:05 U16 value declaration; EDS [1803sub5] lines 1019-1027.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1803_05 = {
    0x1803, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A00:00 U8 count declaration; EDS [1A00sub0] lines 1034-1044.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A00_00 = {
    0x1A00, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A00:01 U32 value declaration; EDS [1A00sub1] lines 1045-1053.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A00_01 = {
    0x1A00, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A00:02 U32 value declaration; EDS [1A00sub2] lines 1054-1062.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A00_02 = {
    0x1A00, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A00:03 U32 value declaration; EDS [1A00sub3] lines 1063-1071.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A00_03 = {
    0x1A00, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A00:04 U32 value declaration; EDS [1A00sub4] lines 1072-1079.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A00_04 = {
    0x1A00, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A01:00 U8 count declaration; EDS [1A01sub0] lines 1086-1096.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A01_00 = {
    0x1A01, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A01:01 U32 value declaration; EDS [1A01sub1] lines 1097-1105.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A01_01 = {
    0x1A01, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A01:02 U32 value declaration; EDS [1A01sub2] lines 1106-1114.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A01_02 = {
    0x1A01, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A01:03 U32 value declaration; EDS [1A01sub3] lines 1115-1122.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A01_03 = {
    0x1A01, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A01:04 U32 value declaration; EDS [1A01sub4] lines 1123-1130.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A01_04 = {
    0x1A01, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A02:00 U8 count declaration; EDS [1A02sub0] lines 1137-1147.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A02_00 = {
    0x1A02, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A02:01 U32 value declaration; EDS [1A02sub1] lines 1148-1155.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A02_01 = {
    0x1A02, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A02:02 U32 value declaration; EDS [1A02sub2] lines 1156-1163.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A02_02 = {
    0x1A02, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A02:03 U32 value declaration; EDS [1A02sub3] lines 1164-1171.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A02_03 = {
    0x1A02, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A02:04 U32 value declaration; EDS [1A02sub4] lines 1172-1179.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A02_04 = {
    0x1A02, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A03:00 U8 count declaration; EDS [1A03sub0] lines 1186-1196.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A03_00 = {
    0x1A03, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A03:01 U32 value declaration; EDS [1A03sub1] lines 1197-1204.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A03_01 = {
    0x1A03, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A03:02 U32 value declaration; EDS [1A03sub2] lines 1205-1212.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A03_02 = {
    0x1A03, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A03:03 U32 value declaration; EDS [1A03sub3] lines 1213-1220.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A03_03 = {
    0x1A03, 0x03, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x1A03:04 U32 value declaration; EDS [1A03sub4] lines 1221-1228.
 * No operation is implemented or authorized; gates Q01, Q17, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_1A03_04 = {
    0x1A03, 0x04, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::FORBIDDEN,
    18, 0x00090001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2000:00 U16 value declaration; EDS [2000] lines 1229-1237; EN86 PDF page 17; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2000_00 = {
    0x2000, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2001:00 U16 value declaration; EDS [2001] lines 1238-1246; EN86 PDF page 17; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2001_00 = {
    0x2001, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2100:00 U16 value declaration; EDS [2100] lines 1247-1255; EN86 PDF page 17; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q02, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2100_00 = {
    0x2100, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00080003u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2101:00 U16 value declaration; EDS [2101] lines 1256-1264; EN86 PDF page 17; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2101_00 = {
    0x2101, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2102:00 U16 value declaration; EDS [2102] lines 1265-1273; EN86 PDF page 17; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2102_00 = {
    0x2102, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2200:00 U16 value declaration; EDS [2200] lines 1274-1282; EN86 PDF page 18; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q04, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2200_00 = {
    0x2200, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    20, 0x00080009u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2201:00 U16 value declaration; EDS [2201] lines 1283-1291; EN86 PDF page 18; ZH86 PDF page 18; EN86 PDF page 11.
 * No operation is implemented or authorized; gates Q01, Q04, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2201_00 = {
    0x2201, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    20, 0x00080009u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2202:00 U16 value declaration; EDS [2202] lines 1292-1300; EN86 PDF page 18; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q05, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2202_00 = {
    0x2202, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    20, 0x00080011u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2203:00 U16 value declaration; EDS [2203] lines 1301-1309; EN86 PDF page 18; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q16, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2203_00 = {
    0x2203, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    38, 0x00088001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2300:00 U16 value declaration; EDS [2300] lines 1310-1318; EN86 PDF page 15; EN86 PDF page 18; ZH86 PDF page 15; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q15, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2300_00 = {
    0x2300, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    39, 0x00084001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2301:00 U16 value declaration; EDS [2301] lines 1319-1327; EN86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q13, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2301_00 = {
    0x2301, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    32, 0x00081001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2302:00 U16 value declaration; EDS [2302] lines 1328-1336; EN86 PDF page 15; EN86 PDF page 18; ZH86 PDF page 15; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q14, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2302_00 = {
    0x2302, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    32, 0x00082001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:00 U16 count declaration; EDS [2400sub0] lines 1343-1351.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_00 = {
    0x2400, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    21, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:01 U16 value declaration; EDS [2400sub1] lines 1352-1360; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 18.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_01 = {
    0x2400, 0x01, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    21, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:02 U16 value declaration; EDS [2400sub2] lines 1361-1369; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_02 = {
    0x2400, 0x02, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    21, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:03 U16 value declaration; EDS [2400sub3] lines 1370-1378; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_03 = {
    0x2400, 0x03, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    21, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:04 U16 value declaration; EDS [2400sub4] lines 1379-1387; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_04 = {
    0x2400, 0x04, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    21, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:05 U16 value declaration; EDS [2400sub5] lines 1388-1396; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_05 = {
    0x2400, 0x05, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    21, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:06 U16 value declaration; EDS [2400sub6] lines 1397-1405; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_06 = {
    0x2400, 0x06, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    21, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:07 U16 value declaration; EDS [2400sub7] lines 1406-1414; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_07 = {
    0x2400, 0x07, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    21, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:08 U16 value declaration; EDS [2400sub8] lines 1415-1423; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_08 = {
    0x2400, 0x08, ScalarType::U16, 2, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    21, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:0C U16 value declaration; EDS [2400subC] lines 1451-1459; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q06, Q20, Q19 remain open. */
constexpr ObjectDescriptor OBJECT_2400_0C = {
    0x2400, 0x0C, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x000C0021u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:0D U16 value declaration; EDS [2400subD] lines 1460-1468; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q06, Q20, Q19 remain open. */
constexpr ObjectDescriptor OBJECT_2400_0D = {
    0x2400, 0x0D, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x000C0021u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:0E U16 value declaration; EDS [2400subE] lines 1469-1477; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q06, Q20, Q19 remain open. */
constexpr ObjectDescriptor OBJECT_2400_0E = {
    0x2400, 0x0E, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x000C0021u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2400:0F U16 value declaration; EDS [2400subF] lines 1478-1486; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q06, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2400_0F = {
    0x2400, 0x0F, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080021u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2401:00 U16 value declaration; EDS [2401] lines 1496-1504; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q08, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2401_00 = {
    0x2401, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080081u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2402:00 U16 value declaration; EDS [2402] lines 1505-1513; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q08, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2402_00 = {
    0x2402, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080081u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2403:00 U16 value declaration; EDS [2403] lines 1514-1522; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q08, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2403_00 = {
    0x2403, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080081u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2404:00 U16 value declaration; EDS [2404] lines 1523-1531; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q08, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2404_00 = {
    0x2404, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080081u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2405:00 U16 value declaration; EDS [2405] lines 1532-1540; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q08, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2405_00 = {
    0x2405, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080081u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2406:00 U16 value declaration; EDS [2406] lines 1541-1549; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q08, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2406_00 = {
    0x2406, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080081u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2407:00 U16 value declaration; EDS [2407] lines 1550-1558; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q08, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2407_00 = {
    0x2407, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080081u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2500:00 U16 value declaration; EDS [2500] lines 1559-1566; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2500_00 = {
    0x2500, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2501:00 U16 value declaration; EDS [2501] lines 1567-1574; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q09, Q04, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2501_00 = {
    0x2501, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080109u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2502:00 U16 value declaration; EDS [2502] lines 1575-1582; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2502_00 = {
    0x2502, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2503:00 U16 value declaration; EDS [2503] lines 1583-1590; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2503_00 = {
    0x2503, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2504:00 U16 value declaration; EDS [2504] lines 1591-1598; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2504_00 = {
    0x2504, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2505:00 U16 value declaration; EDS [2505] lines 1599-1606; EN86 PDF page 19; ZH86 PDF page 19.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2505_00 = {
    0x2505, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2506:00 U16 value declaration; EDS [2506] lines 1607-1614; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2506_00 = {
    0x2506, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2507:00 U16 value declaration; EDS [2507] lines 1615-1622; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2507_00 = {
    0x2507, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2508:00 U16 value declaration; EDS [2508] lines 1623-1630; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2508_00 = {
    0x2508, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2509:00 U16 value declaration; EDS [2509] lines 1631-1638; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2509_00 = {
    0x2509, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x250A:00 U16 value declaration; EDS [250A] lines 1639-1646; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q04, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_250A_00 = {
    0x250A, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080109u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x250B:00 U16 value declaration; EDS [250B] lines 1647-1654; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q04, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_250B_00 = {
    0x250B, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080109u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x250C:00 U16 value declaration; EDS [250C] lines 1655-1662; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_250C_00 = {
    0x250C, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x250D:00 U16 value declaration; EDS [250D] lines 1663-1670; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_250D_00 = {
    0x250D, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x250E:00 U16 value declaration; EDS [250E] lines 1671-1678; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_250E_00 = {
    0x250E, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x250F:00 U16 value declaration; EDS [250F] lines 1679-1686; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_250F_00 = {
    0x250F, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2510:00 U16 value declaration; EDS [2510] lines 1687-1694; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2510_00 = {
    0x2510, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2511:00 U16 value declaration; EDS [2511] lines 1695-1702; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2511_00 = {
    0x2511, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2512:00 U16 value declaration; EDS [2512] lines 1703-1710; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2512_00 = {
    0x2512, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2513:00 U16 value declaration; EDS [2513] lines 1711-1718; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2513_00 = {
    0x2513, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2514:00 U16 value declaration; EDS [2514] lines 1719-1726; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2514_00 = {
    0x2514, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2515:00 U16 value declaration; EDS [2515] lines 1727-1734; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2515_00 = {
    0x2515, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2516:00 U16 value declaration; EDS [2516] lines 1735-1742; EN86 PDF page 19; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2516_00 = {
    0x2516, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2517:00 U16 value declaration; EDS [2517] lines 1743-1750; EN86 PDF page 20; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2517_00 = {
    0x2517, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2518:00 U16 value declaration; EDS [2518] lines 1751-1758; EN86 PDF page 20; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2518_00 = {
    0x2518, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2519:00 U16 value declaration; EDS [2519] lines 1759-1766; EN86 PDF page 20; ZH86 PDF page 20.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2519_00 = {
    0x2519, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x251A:00 U16 value declaration; EDS [251A] lines 1767-1774.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_251A_00 = {
    0x251A, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x251B:00 U16 value declaration; EDS [251B] lines 1775-1782.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_251B_00 = {
    0x251B, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x251C:00 U16 value declaration; EDS [251C] lines 1783-1790.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_251C_00 = {
    0x251C, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x251D:00 U16 value declaration; EDS [251D] lines 1791-1798.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_251D_00 = {
    0x251D, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x251E:00 U16 value declaration; EDS [251E] lines 1799-1806.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_251E_00 = {
    0x251E, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x251F:00 U16 value declaration; EDS [251F] lines 1807-1814.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_251F_00 = {
    0x251F, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x2520:00 U16 value declaration; EDS [2520] lines 1815-1822.
 * No operation is implemented or authorized; gates Q01, Q09, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_2520_00 = {
    0x2520, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    22, 0x00080101u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x603F:00 U16 value declaration; EDS [603F] lines 1823-1831; EN86 PDF page 16; ZH86 PDF page 16.
 * No operation is implemented or authorized; gates Q01, Q20, Q13 remain open. */
constexpr ObjectDescriptor OBJECT_603F_00 = {
    0x603F, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00081001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6040:00 U16 value declaration; EDS [6040] lines 1832-1840; EN86 PDF page 11; EN86 PDF page 12; EN86 PDF page 15; EN86 PDF page 16; ZH86 PDF page 12; ZH86 PDF page 15; ZH86 PDF page 17; OP86 PDF page 9; OP86 PDF page 10; OP86 PDF page 13.
 * No operation is implemented or authorized; gates Q01, Q11, Q12, Q13, Q03 remain open. */
constexpr ObjectDescriptor OBJECT_6040_00 = {
    0x6040, 0x00, ScalarType::U16, 2, false, SdoAccess::WRITE_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    30, 0x00001C05u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6041:00 U16 value declaration; EDS [6041] lines 1841-1849; EN86 PDF page 12; ZH86 PDF page 12; OP86 PDF page 10; ZH57 PDF page 12; OP57 PDF page 10.
 * No operation is implemented or authorized; gates Q01, Q02, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_6041_00 = {
    0x6041, 0x00, ScalarType::U16, 2, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00080003u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x605D:00 I16 value declaration; EDS [605D] lines 1850-1858; EN86 PDF page 16; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q12, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_605D_00 = {
    0x605D, 0x00, ScalarType::I16, 2, true, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    31, 0x00080801u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6064:00 I32 value declaration; EDS [6064] lines 1877-1885; EN86 PDF page 3; EN86 PDF page 16; ZH86 PDF page 3; ZH86 PDF page 17; ZH57 PDF page 3; ZH57 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q04, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_6064_00 = {
    0x6064, 0x00, ScalarType::I32, 4, true, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00080009u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x606C:00 I32 value declaration; EDS [606C] lines 1886-1893; EN86 PDF page 3; EN86 PDF page 16; ZH86 PDF page 3; ZH86 PDF page 17; ZH57 PDF page 3; ZH57 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q04, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_606C_00 = {
    0x606C, 0x00, ScalarType::I32, 4, true, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x00080009u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x607A:00 I32 value declaration; EDS [607A] lines 1894-1902; EN86 PDF page 11; EN86 PDF page 16; ZH86 PDF page 11; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q04, Q11, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_607A_00 = {
    0x607A, 0x00, ScalarType::I32, 4, true, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    33, 0x00080409u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x607C:00 I32 value declaration; EDS [607C] lines 1903-1911; EN86 PDF page 13; EN86 PDF page 16; ZH86 PDF page 13; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q14, Q21, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_607C_00 = {
    0x607C, 0x00, ScalarType::I32, 4, true, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    32, 0x00182001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x607D:00 U8 count declaration; EDS [607Dsub0] lines 1918-1926.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_607D_00 = {
    0x607D, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    20, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x607D:01 I32 value declaration; EDS [607Dsub1] lines 1927-1934; EN86 PDF page 16; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q05, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_607D_01 = {
    0x607D, 0x01, ScalarType::I32, 4, true, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    20, 0x00080011u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x607D:02 I32 value declaration; EDS [607Dsub2] lines 1935-1942; EN86 PDF page 17; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q05, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_607D_02 = {
    0x607D, 0x02, ScalarType::I32, 4, true, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    20, 0x00080011u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6081:00 U32 value declaration; EDS [6081] lines 1943-1951; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q11, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_6081_00 = {
    0x6081, 0x00, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    20, 0x00080401u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6083:00 U32 value declaration; EDS [6083] lines 1952-1960; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q11, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_6083_00 = {
    0x6083, 0x00, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    20, 0x00080401u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6084:00 U32 value declaration; EDS [6084] lines 1961-1969; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q11, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_6084_00 = {
    0x6084, 0x00, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    20, 0x00080401u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6099:00 U8 count declaration; EDS [6099sub0] lines 1985-1993.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_6099_00 = {
    0x6099, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    36, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6099:01 U32 value declaration; EDS [6099sub1] lines 1994-2002; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q21, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_6099_01 = {
    0x6099, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    36, 0x00180001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x6099:02 U32 value declaration; EDS [6099sub2] lines 2003-2011; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q21, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_6099_02 = {
    0x6099, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    36, 0x00180001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x609A:00 U32 value declaration; EDS [609A] lines 2012-2020; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q11, Q21, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_609A_00 = {
    0x609A, 0x00, ScalarType::U32, 4, false, SdoAccess::READ_WRITE,
    ReadEffect::UNQUALIFIED, WritePolicy::QUALIFICATION_REQUIRED, PdoPolicy::UNQUALIFIED,
    36, 0x00180401u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x60FD:00 U32 value declaration; EDS [60FD] lines 2021-2028; EN86 PDF page 17; ZH86 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q19, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_60FD_00 = {
    0x60FD, 0x00, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    19, 0x000C0001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x60FE:00 U8 count declaration; EDS [60FEsub0] lines 2035-2043.
 * No operation is implemented or authorized; gates Q01, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_60FE_00 = {
    0x60FE, 0x00, ScalarType::U8, 1, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::FORBIDDEN,
    21, 0x00080001u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x60FE:01 U32 value declaration; EDS [60FEsub1] lines 2044-2051; EN86 PDF page 17; ZH86 PDF page 17; OP86 PDF page 7; ZH57 PDF page 8; ZH57 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q06, Q07, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_60FE_01 = {
    0x60FE, 0x01, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080061u, SourceScope::EDS_FAMILY_UNBOUND
};

/** @brief 0x60FE:02 U32 value declaration; EDS [60FEsub2] lines 2052-2059; EN86 PDF page 17; ZH86 PDF page 17; OP86 PDF page 7; ZH57 PDF page 8; ZH57 PDF page 17.
 * No operation is implemented or authorized; gates Q01, Q06, Q07, Q20 remain open. */
constexpr ObjectDescriptor OBJECT_60FE_02 = {
    0x60FE, 0x02, ScalarType::U32, 4, false, SdoAccess::READ_ONLY,
    ReadEffect::UNQUALIFIED, WritePolicy::FORBIDDEN, PdoPolicy::UNQUALIFIED,
    21, 0x00080061u, SourceScope::EDS_FAMILY_UNBOUND
};

} // namespace LICHUAN_C
} // namespace MotorControlCAN
