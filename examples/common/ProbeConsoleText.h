/** @file ProbeConsoleText.h
 * @brief Internal complete-record formatter shared by console dispatch paths.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "ProbeConsole.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace MotorControlCANExample {
/** Internal candidate: a failed append never publishes a truncated record. */
struct ConsoleText {
    char bytes[ProbeConsole::RECORD_CAPACITY] = {};
    size_t used = 0;
    bool valid = true;
    void add(const char* format, ...) {
        if (!valid)
            return;
        va_list arguments;
        va_start(arguments, format);
        const int size = std::vsnprintf(bytes + used, sizeof bytes - used, format, arguments);
        va_end(arguments);
        if (size < 0 || static_cast<size_t>(size) >= sizeof bytes - used)
            valid = false;
        else
            used += static_cast<size_t>(size);
    }
    void string(const char* value) {
        if (!valid)
            return;
        if (!consoleJsonString(value, bytes + used, sizeof bytes - used))
            valid = false;
        else
            used += std::strlen(bytes + used);
    }
    void start(bool json, uint32_t id, const char* type, const char* command) {
        if (json) {
            add("{\"schema\":\"motorcontrolcan.console/"
                "1\",\"protocol\":3,\"profile\":\"lichuan_c\",\"type\":\"%s\",\"id\":%lu,"
                "\"command\":",
                type, static_cast<unsigned long>(id));
            string(command);
        } else
            add("%s: ", command);
    }
    void end(bool json) { add(json ? "}\n" : "\n"); }
};
} // namespace MotorControlCANExample
