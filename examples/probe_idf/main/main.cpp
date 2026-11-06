// SPDX-License-Identifier: MIT
#include "EspProbeMain.h"
#include "FirmwareBoard.h"
#if CONFIG_MOTORCONTROLCAN_SMOKE_HOOK
#include "FirmwareSmoke.h"
#include "esp_app_desc.h"
#include <cstdio>

namespace {
// Test-only finite check. Budgets are caller policy copied from the bench identity plan
// (examples/s2_bench), not vendor timing; node IDs and widths come from Kconfig.
MotorControlCANExample::SmokeConfig smokeConfig() {
    static char identity[160];
    const esp_app_desc_t* app = esp_app_get_description();
    char hash[65] = {};
    for (unsigned i = 0; i < 32; ++i)
        std::snprintf(hash + 2 * i, 3, "%02x", unsigned(app->app_elf_sha256[i]));
    std::snprintf(identity, sizeof identity, "elf=%s idf=%s project=%s", hash, app->idf_ver,
                  app->project_name);
    MotorControlCANExample::SmokeConfig config;
    config.nodes[0] = CONFIG_MOTORCONTROLCAN_SMOKE_NODE_A;
    config.nodes[1] = CONFIG_MOTORCONTROLCAN_SMOKE_NODE_B;
    config.options.queueTimeoutUs = 250000;
    config.options.responseTimeoutUs = 250000;
#if CONFIG_MOTORCONTROLCAN_SMOKE_IDENTITY_U8
    config.options.identityWidths = MotorControlCAN::LICHUAN_C::IdentityWidths::MEASURED_U8;
#endif
    config.readWindowUs = 5000000;
    config.identity = identity;
    return config;
}
} // namespace
#endif

extern "C" void app_main() {
#if CONFIG_MOTORCONTROLCAN_SMOKE_HOOK
    static MotorControlCANExample::FirmwareSmoke smoke(smokeConfig());
    MotorControlCANExample::startProbeFirmware(MotorControlCANExample::firmwareBoardConfig(),
                                               smoke.hook());
#else
    MotorControlCANExample::startProbeFirmware(MotorControlCANExample::firmwareBoardConfig());
#endif
}
