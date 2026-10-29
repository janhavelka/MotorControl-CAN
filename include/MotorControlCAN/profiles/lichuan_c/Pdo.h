// SPDX-License-Identifier: MIT
/**
 * @file Pdo.h
 * @brief Reviewed passive layout facts and unverified EDS PDO templates.
 * Generated from docs/reference/lichuan_objects.json. Do not edit.
 * Regenerate with: python scripts/generate_lichuan_objects.py
 */
#pragma once

#include <MotorControlCAN/canopen/Pdo.h>

/** Portable library value contracts; no device I/O. */
namespace MotorControlCAN {
/** Lichuan documentary PDO layouts; model/firmware and live evidence remain unqualified. */
namespace LICHUAN_C {

/** Return reviewed raw-layout facts without authorizing any service or action.
 * The returned table has static lifetime and contains ten default-mapped objects.
 * Unresolved mode/velocity signedness remains raw-only. No time, I/O or allocation.
 * @param[in] profileId Caller-selected profile identity, nonzero for subsequent validation.
 * Zero remains invalid; this constructor does not establish a model binding.
 * @return Value containing four sets per direction and four slots per set, plus
 * borrowed immutable static facts. Validation and live qualification are separate.
 */
inline CANOPEN::PdoProfile pdoProfile(uint32_t profileId) noexcept {
    static const CANOPEN::PdoObject objects[] = {
        CANOPEN::PdoObject(0x6040, 0x00, 2, CANOPEN::PdoSign::UNSIGNED, CANOPEN::PdoRole::RAW, true, false),
        CANOPEN::PdoObject(0x6041, 0x00, 2, CANOPEN::PdoSign::UNSIGNED, CANOPEN::PdoRole::RAW, false, true),
        CANOPEN::PdoObject(0x6060, 0x00, 1, CANOPEN::PdoSign::RAW, CANOPEN::PdoRole::RAW, true, false),
        CANOPEN::PdoObject(0x6061, 0x00, 1, CANOPEN::PdoSign::RAW, CANOPEN::PdoRole::RAW, false, true),
        CANOPEN::PdoObject(0x6064, 0x00, 4, CANOPEN::PdoSign::SIGNED, CANOPEN::PdoRole::ACTUAL, false, true),
        CANOPEN::PdoObject(0x606C, 0x00, 4, CANOPEN::PdoSign::SIGNED, CANOPEN::PdoRole::ACTUAL, false, true),
        CANOPEN::PdoObject(0x607A, 0x00, 4, CANOPEN::PdoSign::SIGNED, CANOPEN::PdoRole::COMMANDED, true, false),
        CANOPEN::PdoObject(0x6081, 0x00, 4, CANOPEN::PdoSign::UNSIGNED, CANOPEN::PdoRole::COMMANDED, true, false),
        CANOPEN::PdoObject(0x60FD, 0x00, 4, CANOPEN::PdoSign::UNSIGNED, CANOPEN::PdoRole::RAW, false, true),
        CANOPEN::PdoObject(0x60FF, 0x00, 4, CANOPEN::PdoSign::RAW, CANOPEN::PdoRole::COMMANDED, true, false),
    };
    CANOPEN::PdoProfile profile;
    profile.profile = profileId;
    profile.sets = 4;
    profile.slots = 4;
    profile.objects = objects;
    profile.objectCount = sizeof(objects) / sizeof(objects[0]);
    return profile;
}

/** Copy an original EDS layout template; never confirms live map contents.
 * Performs no device writes, reads, time acquisition or allocation. Independent
 * calls are reentrant; the caller serializes mutation of shared output.
 * @param[in] direction RPDO or TPDO, relative to the drive.
 * @param[in] number One-based set number, 1..4. Sets 3/4 have disabled count zero.
 * @param[out] output Owned complete candidate, unchanged on rejection. No input
 * pointer is retained. Inactive mapping words are always zero.
 * @return OK proves source-template construction only. INVALID_CONFIG rejects
 * invalid direction/number, detail zero; no live qualification follows.
 */
inline Status pdoTemplate(CANOPEN::PdoDirection direction, uint16_t number,
                          CANOPEN::PdoMap& output) noexcept {
    if ((direction != CANOPEN::PdoDirection::RPDO && direction != CANOPEN::PdoDirection::TPDO) ||
        number < 1 || number > 4) {
        return Status(Err::INVALID_CONFIG, 0, nullptr);
    }
    CANOPEN::PdoMap candidate;
    candidate.direction = direction;
    candidate.number = number;
    if (direction == CANOPEN::PdoDirection::RPDO) {
        switch (number) {
        case 1:
            candidate.count = 3;
            candidate.words[0] = 0x60400010u;
            candidate.words[1] = 0x60FF0020u;
            candidate.words[2] = 0x60600008u;
            break;
        case 2:
            candidate.count = 2;
            candidate.words[0] = 0x607A0020u;
            candidate.words[1] = 0x60810020u;
            break;
        case 3:
            candidate.count = 0;
            break;
        case 4:
            candidate.count = 0;
            break;
        }
    }
    if (direction == CANOPEN::PdoDirection::TPDO) {
        switch (number) {
        case 1:
            candidate.count = 3;
            candidate.words[0] = 0x60410010u;
            candidate.words[1] = 0x60FD0020u;
            candidate.words[2] = 0x60610008u;
            break;
        case 2:
            candidate.count = 2;
            candidate.words[0] = 0x60640020u;
            candidate.words[1] = 0x606C0020u;
            break;
        case 3:
            candidate.count = 0;
            break;
        case 4:
            candidate.count = 0;
            break;
        }
    }
    output = candidate;
    return Ok();
}

} // namespace LICHUAN_C
} // namespace MotorControlCAN
