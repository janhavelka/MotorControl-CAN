// SPDX-License-Identifier: MIT
// Finite board bring-up on OPTAK_FGI_2025. No motor command or startup CAN request.
#include "EspProbeMain.h"
#include "EspProbePlatform.h"
#include "FirmwareProbe.h"
#include "IdfTwaiDriver.h"
#include "RawEvidence.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "sdkconfig.h"
#include <cstdio>
#include <cstring>

#if !CONFIG_IDF_TARGET_ESP32S2
#error "This fixture is the user's ESP32-S2 board"
#endif

namespace E = MotorControlCANExample;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
DRAM_ATTR StackType_t stack[E::FIRMWARE_STACK_BYTES / sizeof(StackType_t)];
DRAM_ATTR StaticTask_t task;

// Output is copied once into the ordinary bounded platform queue. The caller
// sends one request at a time; CAN service never waits for serial output.
void line(E::FirmwareProbe& firmware, const char* text) {
    firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(text), std::strlen(text));
}
L::ReadTarget target(unsigned node) {
    L::ReadTarget result;
    result.channel.binding.logicalAxisId = node;
    result.channel.binding.endpoint.profile = 1;
    result.channel.binding.endpoint.address = node;
    result.channel.binding.bindingGeneration = 1;
    result.channel.binding.configurationGeneration = 1;
    result.channel.clientToServerId = 0x600 + node;
    result.channel.serverToClientId = 0x580 + node;
    result.nodeGeneration = 1;
    return result;
}
bool identity(E::FirmwareProbe& firmware, C::SdoClient& services, E::RawEvidence& raw, bool rawPlan,
              unsigned node, bool& configured, uint32_t id, uint64_t now) {
    if (!firmware.running() || firmware.evidence().record() || raw.record())
        return false;
    const L::ReadTarget selected = target(node);
    if (!configured) {
        // Explicit bench premise: exclusive, initially unused server session.
        // This is NOT service confirmation. Unknown-effect uploads still need
        // an exact finite experiment grant before ordinary owner admission.
        if (!services.configure(node - 1, selected.channel, false, true))
            return false;
        C::NetworkRoute route;
        route.binding = selected.channel.binding;
        route.generation = 1;
        route.heartbeatId = 0x700 + node;
        route.emcyId = 0x80 + node;
        if (!services.configureNetwork(node - 1, route, C::NetworkObserver(), now))
            return false;
        configured = true;
    }
    if (!firmware.app().select(selected))
        return false;
    // Static scratch avoids stacking the acquisition plan on top of typed reads.
    static E::ReadExperiment review;
    review.plan.id = id;
    std::strcpy(review.plan.fixtureId, "OPTAK-S2-20261102-two-CL86");
    std::strcpy(review.plan.softwareRevision, MOTORCONTROLCAN_SOURCE_REVISION);
    std::strcpy(review.plan.firmware, "unknown; exact binary hash in host record");
    std::strcpy(review.plan.dialect, "raw; unresolved");
    std::strcpy(review.plan.authorization, "user: upload, communicate and check both drives");
    std::strcpy(
        review.plan.observations,
        "retain exact upload bytes/errors; no inferred motion or harmless-read qualification");
    review.plan.target = selected;
    review.plan.kind = L::ReadKind::IDENTITY;
    review.plan.deadlineUs = now + 3000000;
    review.plan.options.queueTimeoutUs = 250000;
    review.plan.options.responseTimeoutUs = 250000;
    std::strcpy(review.sourceReview, "EDS 1000/1009/100A/1018:0..3/2000/2001; Q20 remains open");
    std::strcpy(review.independentReview,
                "bench_independent; retained review in board evidence report");
    std::strcpy(review.fixtureReview, "hardware_bench; S2 TX17 RX16 S18; nodes1/2 1Mbit");
    std::strcpy(review.independentControl,
                "No actuation writes; user reports bolted free shafts; no firmware stop claim");
    std::strcpy(review.failurePlan,
                "One finite attempt; terminate on first failure; retain raw evidence; no retry");
    std::strcpy(review.reconciliation,
                "Inspect retained outcome; unresolved SDO stays inhibited; no reset/replay");
    if (rawPlan) {
        static E::RawEvidencePlan plan;
        std::strcpy(review.sourceReview,
                    "EDS identity/1800/1A00/6041/6061; actual width retained; Q01/Q17/Q20 open");
        std::strcpy(review.independentControl,
                    "User confirms drive power cutoff and bolted free shafts; no motion writes");
        plan.review = review;
        plan.slot = node - 1;
        plan.firstAuthorizationId = 1000;
        plan.includeState = true;
        return raw.begin(plan, now) == E::EvidenceAdmission::ACCEPTED;
    }
    return firmware.evidence().requestExperiment(review, now) == E::EvidenceAdmission::ACCEPTED;
}

// Emit one complete retained record line per turn. No diagnostic queue is
// drained here: DeviceEvidence already copied the sole owner's observations.
bool dump(E::FirmwareProbe& firmware, unsigned& cursor) {
    const E::AcquisitionRecord* r = firmware.evidence().record();
    if (!r || !r->terminal)
        return false;
    char out[240] = {};
    if (cursor == 0) {
        std::snprintf(out, sizeof out,
                      "EVIDENCE id=%lu node=%lu outcome=%u uncertain=%u lost=%u terminals=%u "
                      "traffic=%u gap=%llu\n",
                      static_cast<unsigned long>(r->plan.id),
                      static_cast<unsigned long>(r->plan.target.channel.binding.endpoint.address),
                      unsigned(r->read.outcome), unsigned(r->read.uncertain),
                      unsigned(r->evidenceLost), unsigned(r->terminalCount),
                      unsigned(r->trafficCount),
                      static_cast<unsigned long long>(r->maxServiceGapUs));
    } else if (cursor <= r->terminalCount) {
        const C::SdoCompletion& t = r->terminals[cursor - 1].service;
        std::snprintf(out, sizeof out,
                      "RESULT step=%lu object=%04x:%02x outcome=%u ack=%u possible=%u uncertain=%u "
                      "abort=%08lx size=%u data=%02x%02x%02x%02x admitted=%llu completed=%llu\n",
                      static_cast<unsigned long>(t.transfer.step), unsigned(t.transfer.index),
                      unsigned(t.transfer.subindex), unsigned(t.outcome),
                      unsigned(t.protocolAcknowledged), unsigned(t.effectPossible),
                      unsigned(t.effectUncertain), static_cast<unsigned long>(t.abortCode),
                      unsigned(t.size), unsigned(t.data[0]), unsigned(t.data[1]),
                      unsigned(t.data[2]), unsigned(t.data[3]),
                      static_cast<unsigned long long>(t.admittedUs),
                      static_cast<unsigned long long>(t.completedUs));
    } else if (cursor <= unsigned(r->terminalCount) + r->trafficCount) {
        const E::EvidenceTraffic& t = r->traffic[cursor - r->terminalCount - 1];
        std::snprintf(
            out, sizeof out,
            "TRAFFIC kind=%u time=%llu id=%08lx ext=%u rtr=%u dlc=%u "
            "data=%02x%02x%02x%02x%02x%02x%02x%02x accepted=%u captureKnown=%u\n",
            unsigned(t.kind), static_cast<unsigned long long>(t.processedUs),
            static_cast<unsigned long>(t.frame.id), unsigned(t.frame.extended),
            unsigned(t.frame.rtr), unsigned(t.frame.dlc), unsigned(t.frame.data[0]),
            unsigned(t.frame.data[1]), unsigned(t.frame.data[2]), unsigned(t.frame.data[3]),
            unsigned(t.frame.data[4]), unsigned(t.frame.data[5]), unsigned(t.frame.data[6]),
            unsigned(t.frame.data[7]), unsigned(t.accepted), unsigned(t.receivedTimeKnown));
    } else {
        std::strcpy(out, "END evidence retained; r=release\n");
    }
    if (!firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out), std::strlen(out)))
        return true;
    return ++cursor <= unsigned(r->terminalCount) + r->trafficCount + 1;
}
bool dumpRaw(E::FirmwareProbe& firmware, E::RawEvidence& raw, unsigned& cursor) {
    const E::RawEvidenceRecord* r = raw.record();
    if (!r || !r->terminal)
        return false;
    char out[240] = {};
    if (cursor == 0) {
        std::snprintf(
            out, sizeof out,
            "RAW id=%lu node=%lu failure=%u lost=%u terminals=%u traffic=%u tail=%u tailLost=%u "
            "gap=%llu\n",
            static_cast<unsigned long>(r->plan.review.plan.id),
            static_cast<unsigned long>(r->plan.review.plan.target.channel.binding.endpoint.address),
            unsigned(r->failure), unsigned(r->evidenceLost), unsigned(r->terminalCount),
            unsigned(r->trafficCount), unsigned(r->tailCount), unsigned(r->tailOverflow),
            static_cast<unsigned long long>(r->maxServiceGapUs));
    } else if (cursor <= r->terminalCount) {
        const C::SdoCompletion& t = r->terminals[cursor - 1].service;
        std::snprintf(
            out, sizeof out,
            "RESULT step=%lu object=%04x:%02x outcome=%u ack=%u possible=%u uncertain=%u "
            "abort=%08lx cleanup=%u size=%u data=%02x%02x%02x%02x admitted=%llu completed=%llu\n",
            static_cast<unsigned long>(t.transfer.step), unsigned(t.transfer.index),
            unsigned(t.transfer.subindex), unsigned(t.outcome), unsigned(t.protocolAcknowledged),
            unsigned(t.effectPossible), unsigned(t.effectUncertain),
            static_cast<unsigned long>(t.abortCode), unsigned(t.clientAbortQueued),
            unsigned(t.size), unsigned(t.data[0]), unsigned(t.data[1]), unsigned(t.data[2]),
            unsigned(t.data[3]), static_cast<unsigned long long>(t.admittedUs),
            static_cast<unsigned long long>(t.completedUs));
    } else if (cursor <= unsigned(r->terminalCount) + r->trafficCount + r->tailCount) {
        const unsigned index = cursor - r->terminalCount - 1;
        const bool tail = index >= r->trafficCount;
        const E::EvidenceTraffic& t = tail ? r->tail[index - r->trafficCount] : r->traffic[index];
        std::snprintf(
            out, sizeof out,
            "%s kind=%u time=%llu id=%08lx ext=%u rtr=%u dlc=%u "
            "data=%02x%02x%02x%02x%02x%02x%02x%02x accepted=%u captureKnown=%u\n",
            tail ? "TAIL" : "TRAFFIC", unsigned(t.kind),
            static_cast<unsigned long long>(t.processedUs), static_cast<unsigned long>(t.frame.id),
            unsigned(t.frame.extended), unsigned(t.frame.rtr), unsigned(t.frame.dlc),
            unsigned(t.frame.data[0]), unsigned(t.frame.data[1]), unsigned(t.frame.data[2]),
            unsigned(t.frame.data[3]), unsigned(t.frame.data[4]), unsigned(t.frame.data[5]),
            unsigned(t.frame.data[6]), unsigned(t.frame.data[7]), unsigned(t.accepted),
            unsigned(t.receivedTimeKnown));
    } else {
        std::snprintf(out, sizeof out,
                      "END raw terminals=%u traffic=%u tail=%u tailLost=%u lost=%u; r=release\n",
                      unsigned(r->terminalCount), unsigned(r->trafficCount), unsigned(r->tailCount),
                      unsigned(r->tailOverflow), unsigned(r->evidenceLost));
    }
    if (!firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out), std::strlen(out)))
        return true;
    return ++cursor <= unsigned(r->terminalCount) + r->trafficCount + r->tailCount + 1;
}
void ownerTask(void*) {
    static DRAM_ATTR C::SdoClient services;
    static DRAM_ATTR E::IdfTwaiDriver driver;
    static DRAM_ATTR E::EspProbePlatform console;
    static DRAM_ATTR E::FirmwareProbe firmware(services, driver.driver(), console.callbacks());
    static DRAM_ATTR E::RawEvidence raw(firmware.owner(), services);
    // TJA1051 S=1 suppresses bus transmission. Set the latch before enabling GPIO.
    const bool silentReady = gpio_set_level(GPIO_NUM_18, 1) == ESP_OK &&
                             gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT) == ESP_OK;
    if (!console.begin()) {
        for (;;)
            vTaskDelay(1);
    }
    line(firmware, "S2_BENCH ready; UART115200; CAN disabled; p=ping c=start s=stats 3/4=raw-nodes\n");
    bool attempted = false, configured[2] = {}, used[2] = {}, dumping = false, reported = false;
    bool silent = true, controllerReady = false;
    uint32_t acquisition = 0, grants[2] = {}, grantedOperation[2] = {}, grantedStep[2] = {};
    unsigned dumpCursor = 0;
    for (;;) {
        uint64_t now = 0;
        if (firmware.platform().nowUs(now)) {
            const L::ReadContext& read = firmware.app().readContext();
            if (read.state == MotorControlCAN::ReadState::ACTIVE &&
                !firmware.app().pendingRequest().owner) {
                L::PreparedRead next;
                if (L::nextRead(read, now, next) && next.ready) {
                    const unsigned slot = read.target.channel.binding.endpoint.address - 1;
                    if (slot < 2 && (next.transfer.operation != grantedOperation[slot] ||
                                     next.transfer.step != grantedStep[slot])) {
                        if (grants[slot] == UINT32_MAX ||
                            !services.authorizeUploadExperiment(slot, next.transfer,
                                                                read.target.nodeGeneration,
                                                                grants[slot] + 1, now)) {
                            firmware.evidence().interrupt(now);
                        } else {
                            ++grants[slot];
                            grantedOperation[slot] = next.transfer.operation;
                            grantedStep[slot] = next.transfer.step;
                        }
                    }
                }
            }
        }
        firmware.service();
        raw.service(firmware.owner().lastTimeUs());
        if (firmware.clockFailed() && !silent)
            silent = gpio_set_level(GPIO_NUM_18, 1) == ESP_OK;
        const E::AcquisitionRecord* record = firmware.evidence().record();
        const E::RawEvidenceRecord* rawRecord = raw.record();
        if (((record && record->terminal) || (rawRecord && rawRecord->terminal)) && !reported) {
            reported = true;
            dumping = true;
            dumpCursor = 0;
            services.clearUploadExperiment(0);
            services.clearUploadExperiment(1);
        }
        if (dumping)
            dumping = rawRecord ? dumpRaw(firmware, raw, dumpCursor) : dump(firmware, dumpCursor);
        uint8_t command = 0;
        if (firmware.platform().takeInput(command)) {
            char output[240] = {};
            if (command >= '1' && command <= '4') {
                const bool rawPlan = command >= '3';
                const unsigned node = command - (rawPlan ? '2' : '0');
                const bool accepted = controllerReady && !dumping && !used[node - 1] &&
                                      acquisition != UINT32_MAX && firmware.platform().nowUs(now) &&
                                      identity(firmware, services, raw, rawPlan, node,
                                               configured[node - 1], acquisition + 1, now);
                if (accepted) {
                    ++acquisition;
                    used[node - 1] = true;
                    reported = false;
                }
                line(firmware, accepted
                                   ? "IDENTITY admitted; finite experimental uploads; no retry\n"
                                   : "IDENTITY refused; no work\n");
            } else if (command == 'd') {
                if (((record && record->terminal) || (rawRecord && rawRecord->terminal)) &&
                    !dumping) {
                    dumping = true;
                    dumpCursor = 0;
                } else {
                    line(firmware, "DUMP unavailable\n");
                }
            } else if (command == 'r') {
                line(firmware,
                     !dumping && (rawRecord ? raw.release() : firmware.evidence().release())
                         ? "RELEASED record; channel uncertainty unchanged\n"
                         : "RELEASE refused\n");
            } else if (command == 'p') {
                const esp_app_desc_t* app = esp_app_get_description();
                char hash[65] = {};
                for (unsigned i = 0; i < 32; ++i)
                    std::snprintf(hash + i * 2, 3, "%02x", unsigned(app->app_elf_sha256[i]));
                std::snprintf(output, sizeof(output),
                              "PONG target=esp32s2 base=%s elf=%s sdk=%s running=%u active=%u "
                              "silent=%u pinsReady=%u\n",
                              MOTORCONTROLCAN_SOURCE_REVISION, hash, app->idf_ver,
                              unsigned(firmware.running()), unsigned(controllerReady),
                              unsigned(silent), unsigned(silentReady));
                line(firmware, output);
            } else if (command == 'c') {
                bool ready = false;
                if (!attempted && silentReady) {
                    attempted = true;
                    E::FirmwareConfig config;
                    config.configured = true;
                    config.controller.txPin = 17;
                    config.controller.rxPin = 16;
                    config.controller.bitrate = 1000000;
                    config.controller.sdkTxQueueDepth = 1;
                    config.controller.rxQueueDepth = 16;
                    ready = firmware.begin(config);
                    if (ready) {
                        if (gpio_set_level(GPIO_NUM_18, 0) != ESP_OK) {
                            firmware.transport().shutdown();
                            ready = false;
                        } else {
                            silent = false;
                        }
                    }
                    controllerReady = ready;
                }
                line(firmware, ready ? "CAN ready bitrate=1000000 TX17 RX16 S18; no request\n"
                                     : "CAN start refused or failed; no automatic retry\n");
            } else if (command == 's') {
                const E::TwaiDiagnostics diagnostics = firmware.transport().diagnostics();
                const E::Stats stats = firmware.owner().stats();
                std::snprintf(output, sizeof(output),
                              "STATS running=%u inhibited=%u rx=%lu tx=%lu dropped=%lu errors=%lu "
                              "flags=%08lx stackFree=%lu\n",
                              unsigned(firmware.running()), unsigned(diagnostics.inhibited),
                              static_cast<unsigned long>(stats.rxFrames),
                              static_cast<unsigned long>(stats.txFrames),
                              static_cast<unsigned long>(diagnostics.rxDropped),
                              static_cast<unsigned long>(diagnostics.busErrors),
                              static_cast<unsigned long>(diagnostics.errorFlags),
                              static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
                line(firmware, output);
            } else if (command != '\r' && command != '\n') {
                line(firmware, "UNKNOWN command; no action\n");
            }
        }
        console.idle();
    }
}
} // namespace
extern "C" void app_main() {
    xTaskCreateStaticPinnedToCore(ownerTask, "can_bench", sizeof(stack), nullptr, 1, stack, &task,
                                  0);
}
