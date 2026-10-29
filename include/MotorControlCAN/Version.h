/**
 * @file Version.h
 * @brief Package version generated from library.json. Do not edit.
 */
#pragma once

#include <stdint.h>

namespace MotorControlCAN {

/** @brief Package major version, in the range 0 through 65535. */
static constexpr uint16_t VERSION_MAJOR = 0;

/** @brief Package minor version, in the range 0 through 99. */
static constexpr uint16_t VERSION_MINOR = 1;

/** @brief Package patch version, in the range 0 through 99. */
static constexpr uint16_t VERSION_PATCH = 0;

/** @brief Ordered numeric version: major * 10000 + minor * 100 + patch. */
static constexpr uint32_t VERSION_CODE = 100;

/** @brief Package MAJOR.MINOR.PATCH text with static storage duration. */
static constexpr const char* VERSION = "0.1.0";

}  // namespace MotorControlCAN
