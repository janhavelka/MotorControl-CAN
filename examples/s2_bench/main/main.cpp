// SPDX-License-Identifier: MIT
// Finite board bring-up on OPTAK_FGI_2025. No motor command or startup CAN request.
#include "EspProbeMain.h"
#include "EspProbePlatform.h"
#include "FirmwareProbe.h"
#include "IdfTwaiDriver.h"
#include "PdoExperiment.h"
#include "RawEvidence.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "sdkconfig.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

#if !CONFIG_IDF_TARGET_ESP32S2
#error "This fixture is the user's ESP32-S2 board"
#endif

namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
DRAM_ATTR StackType_t stack[E::FIRMWARE_STACK_BYTES / sizeof(StackType_t)];
DRAM_ATTR StaticTask_t task;
template <size_t N, size_t M> void label(char (&output)[N], const char (&value)[M]) {
    static_assert(M <= N, "Bench metadata must fit without truncation");
    std::memcpy(output, value, M);
}
// A truncated line is failed evidence, never a plausible shortened observation.
void formatLine(char* output, size_t capacity, const char* pattern, ...) {
    va_list arguments;
    va_start(arguments, pattern);
    const int count = std::vsnprintf(output, capacity, pattern, arguments);
    va_end(arguments);
    if (count < 0 || size_t(count) >= capacity) {
        static const char failure[] = "OUTPUT_FORMAT_ERROR; evidence incomplete\n";
        if (capacity >= sizeof failure)
            std::memcpy(output, failure, sizeof failure);
        else if (capacity)
            output[0] = 0;
    }
}

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
              unsigned node, bool& configured, uint32_t id, uint64_t now, bool heartbeat = false,
              bool motionReadiness = false) {
    if (!firmware.running() || firmware.evidence().record() || raw.record())
        return false;
    L::ReadTarget selected = target(node);
    if (configured && services.channel(node - 1) && services.node(node - 1)) {
        selected.channel = *services.channel(node - 1);
        selected.nodeGeneration = services.node(node - 1)->route.generation;
    }
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
    label(review.plan.fixtureId, "OPTAK-S2-20261102-two-CL86");
    label(review.plan.softwareRevision, MOTORCONTROLCAN_SOURCE_REVISION);
    label(review.plan.firmware, "unknown; exact binary hash in host record");
    label(review.plan.dialect, "raw; unresolved");
    label(review.plan.authorization, "user: upload, communicate and check both drives");
    label(review.plan.observations,
          "retain exact upload bytes/errors; no inferred motion or harmless-read qualification");
    review.plan.target = selected;
    review.plan.kind = L::ReadKind::IDENTITY;
    review.plan.deadlineUs = now + 5000000;
    review.plan.options.queueTimeoutUs = 250000;
    review.plan.options.responseTimeoutUs = 250000;
    label(review.sourceReview, "EDS 1000/1009/100A/1018:0..3/2000/2001; Q20 remains open");
    label(review.independentReview, "bench_independent; retained review in board evidence report");
    label(review.fixtureReview, "hardware_bench; S2 TX17 RX16 S18; nodes1/2 1Mbit");
    label(review.independentControl,
          "No actuation writes; user reports bolted free shafts; no firmware stop claim");
    label(review.failurePlan,
          "One finite attempt; terminate on first failure; retain raw evidence; no retry");
    label(review.reconciliation,
          "Inspect retained outcome; unresolved SDO stays inhibited; no reset/replay");
    if (rawPlan) {
        static E::RawEvidencePlan plan;
        label(review.sourceReview,
              "EDS identity/1800/1A00/6041/6061; actual width retained; Q01/Q17/Q20 open");
        label(review.independentControl,
              "User confirms drive power cutoff and bolted free shafts; no motion writes");
        plan.review = review;
        plan.slot = node - 1;
        plan.inventory = motionReadiness ? E::RawEvidenceInventory::MOTION_READINESS
                                         : E::RawEvidenceInventory::IDENTITY_NETWORK;
        plan.firstAuthorizationId = motionReadiness ? 4000 : heartbeat ? 2000 : 1000;
        plan.includeState = !heartbeat && !motionReadiness;
        plan.includeNetwork = !heartbeat && !motionReadiness;
        plan.enableHeartbeat = heartbeat;
        if (motionReadiness) {
            label(review.plan.authorization,
                  "user: finite motion-readiness reads; no motor action");
            label(review.sourceReview,
                  "EDS motion/feedback/DI; EN86 p11/16-19; Q20 raw unknown effects");
            label(review.plan.observations, "Thirty raw uploads; retain widths/config/input state; "
                                            "no motion or scale inference");
            plan.review = review;
        } else if (heartbeat) {
            label(review.plan.authorization,
                  "user: NMT, heartbeat evidence and limited TPDO experiment");
            label(review.sourceReview,
                  "EDS1017 U16 RW; CANopen heartbeat ms; exact0->1000 experiment");
            plan.review = review;
        }
        return raw.begin(plan, now) == E::EvidenceAdmission::ACCEPTED;
    }
    return firmware.evidence().requestExperiment(review, now) == E::EvidenceAdmission::ACCEPTED;
}

// Emit one complete retained record line per turn. No diagnostic queue is
// drained here: DeviceEvidence already copied the sole owner's observations.
void trafficLine(char* out, size_t size, const E::EvidenceTraffic& t, const char* label) {
    formatLine(out, size,
               "%s k=%u p=%llu id=%lx ext=%u rtr=%u dlc=%u data=%02x%02x%02x%02x%02x%02x%02x%02x "
               "ok=%u exact=%u t=%llu bounds=%u lo=%llu hi=%llu\n",
               label, unsigned(t.kind), static_cast<unsigned long long>(t.processedUs),
               static_cast<unsigned long>(t.frame.id), unsigned(t.frame.extended),
               unsigned(t.frame.rtr), unsigned(t.frame.dlc), unsigned(t.frame.data[0]),
               unsigned(t.frame.data[1]), unsigned(t.frame.data[2]), unsigned(t.frame.data[3]),
               unsigned(t.frame.data[4]), unsigned(t.frame.data[5]), unsigned(t.frame.data[6]),
               unsigned(t.frame.data[7]), unsigned(t.accepted), unsigned(t.receivedTimeKnown),
               static_cast<unsigned long long>(t.receivedUs), unsigned(t.receiveBoundsKnown),
               static_cast<unsigned long long>(t.receivedEarliestUs),
               static_cast<unsigned long long>(t.receivedLatestUs));
}
bool dumpNetwork(E::FirmwareProbe& firmware, C::SdoClient& services, unsigned& cursor) {
    char out[240] = {};
    if (cursor < 2) {
        const auto* n = services.node(cursor);
        C::ServiceEligibility eligibility;
        if (n && C::serviceEligibility(*n, firmware.owner().lastTimeUs(), eligibility)) {
            formatLine(out, sizeof out,
                       "NETWORK node=%u epoch=%lu state=%u qualified=%u producerKnown=%u ms=%u "
                       "pending=%u observed=%u expired=%u exact=%u bounds=%u lo=%llu hi=%llu "
                       "p=%llu dropped=%lu\n",
                       cursor + 1, static_cast<unsigned long>(n->route.generation),
                       unsigned(n->nmt), unsigned(eligibility.exactNmtKnown),
                       unsigned(n->producerKnown), unsigned(n->producerTimeMs),
                       unsigned(n->transitionPending), unsigned(n->transitionObserved),
                       unsigned(n->transitionExpired), unsigned(n->nmtTime.receivedTimeKnown),
                       unsigned(n->nmtTime.receiveBoundsKnown),
                       static_cast<unsigned long long>(n->nmtTime.receivedEarliestUs),
                       static_cast<unsigned long long>(n->nmtTime.receivedLatestUs),
                       static_cast<unsigned long long>(n->nmtTime.processedUs),
                       static_cast<unsigned long>(services.droppedNetworkObservations()));
        } else {
            formatLine(out, sizeof out, "NETWORK node=%u unconfigured\n", cursor + 1);
        }
        if (!firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out),
                                             std::strlen(out)))
            return true;
        ++cursor;
        return true;
    }
    // Hold one consumed observer copy until its complete line fits the output queue.
    static C::NetworkObservation observation;
    static bool pending = false;
    if (!pending && cursor < 10)
        pending = services.takeNetworkObservation(observation);
    if (!pending) {
        line(firmware,
             "END network; boot-up requires an actual observed00 frame; no inferred reset\n");
        return false;
    }
    const auto& t = observation.time;
    formatLine(out, sizeof out,
               "EVENT id=%lx dlc=%u data=%02x%02x%02x%02x%02x%02x%02x%02x disposition=%u "
               "decoded=%u kind=%u p=%llu exact=%u bounds=%u lo=%llu hi=%llu\n",
               static_cast<unsigned long>(observation.frame.id), unsigned(observation.frame.dlc),
               unsigned(observation.frame.data[0]), unsigned(observation.frame.data[1]),
               unsigned(observation.frame.data[2]), unsigned(observation.frame.data[3]),
               unsigned(observation.frame.data[4]), unsigned(observation.frame.data[5]),
               unsigned(observation.frame.data[6]), unsigned(observation.frame.data[7]),
               unsigned(observation.disposition), unsigned(observation.decoded),
               unsigned(observation.event.kind), static_cast<unsigned long long>(t.processedUs),
               unsigned(t.receivedTimeKnown), unsigned(t.receiveBoundsKnown),
               static_cast<unsigned long long>(t.receivedEarliestUs),
               static_cast<unsigned long long>(t.receivedLatestUs));
    if (firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out), std::strlen(out))) {
        pending = false;
        ++cursor;
    }
    return true;
}
bool dump(E::FirmwareProbe& firmware, unsigned& cursor) {
    const E::AcquisitionRecord* r = firmware.evidence().record();
    if (!r || !r->terminal)
        return false;
    char out[240] = {};
    if (cursor == 0) {
        formatLine(out, sizeof out,
                   "EVIDENCE id=%lu node=%lu outcome=%u uncertain=%u lost=%u terminals=%u "
                   "traffic=%u gap=%llu\n",
                   static_cast<unsigned long>(r->plan.id),
                   static_cast<unsigned long>(r->plan.target.channel.binding.endpoint.address),
                   unsigned(r->read.outcome), unsigned(r->read.uncertain),
                   unsigned(r->evidenceLost), unsigned(r->terminalCount), unsigned(r->trafficCount),
                   static_cast<unsigned long long>(r->maxServiceGapUs));
    } else if (cursor <= r->terminalCount) {
        const C::SdoCompletion& t = r->terminals[cursor - 1].service;
        formatLine(out, sizeof out,
                   "RESULT step=%lu object=%04x:%02x outcome=%u ack=%u possible=%u uncertain=%u "
                   "abort=%08lx size=%u data=%02x%02x%02x%02x admitted=%llu completed=%llu\n",
                   static_cast<unsigned long>(t.transfer.step), unsigned(t.transfer.index),
                   unsigned(t.transfer.subindex), unsigned(t.outcome),
                   unsigned(t.protocolAcknowledged), unsigned(t.effectPossible),
                   unsigned(t.effectUncertain), static_cast<unsigned long>(t.abortCode),
                   unsigned(t.size), unsigned(t.data[0]), unsigned(t.data[1]), unsigned(t.data[2]),
                   unsigned(t.data[3]), static_cast<unsigned long long>(t.admittedUs),
                   static_cast<unsigned long long>(t.completedUs));
    } else if (cursor <= unsigned(r->terminalCount) + r->trafficCount) {
        const E::EvidenceTraffic& t = r->traffic[cursor - r->terminalCount - 1];
        trafficLine(out, sizeof out, t, "TRAFFIC");
    } else {
        label(out, "END evidence retained; r=release\n");
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
        formatLine(
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
        formatLine(
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
        trafficLine(out, sizeof out, t, tail ? "TAIL" : "TRAFFIC");
    } else {
        formatLine(out, sizeof out,
                   "END raw terminals=%u traffic=%u tail=%u tailLost=%u lost=%u; r=release\n",
                   unsigned(r->terminalCount), unsigned(r->trafficCount), unsigned(r->tailCount),
                   unsigned(r->tailOverflow), unsigned(r->evidenceLost));
    }
    if (!firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out), std::strlen(out)))
        return true;
    return ++cursor <= unsigned(r->terminalCount) + r->trafficCount + r->tailCount + 1;
}
bool startMap(E::FirmwareProbe& firmware, C::SdoClient& services, E::PdoExperiment& pdo,
              uint32_t operation, uint64_t now) {
    const auto* channel = services.channel(0);
    const auto* node = services.node(0);
    if (!channel || !node || now > UINT64_MAX - 8000000)
        return false;
    static E::PdoExperimentPlan plan;
    auto& r = plan.review;
    r.plan.id = operation;
    r.plan.target.channel = *channel;
    r.plan.target.nodeGeneration = node->route.generation;
    r.plan.kind = L::ReadKind::IDENTITY;
    r.plan.deadlineUs = now + 8000000;
    r.plan.options.queueTimeoutUs = r.plan.options.responseTimeoutUs = 250000;
    label(r.plan.fixtureId, "OPTAK-S2-20261102-two-CL86");
    label(r.plan.softwareRevision, MOTORCONTROLCAN_SOURCE_REVISION);
    label(r.plan.firmware, "CL86-C raw0C/20/0400/0200; evidence record2");
    label(r.plan.dialect, "raw; status dialect unresolved");
    label(r.plan.authorization, "user: reviewed limited TPDO1 configuration experiment");
    label(r.plan.observations,
          "24 exact services; retained wire traffic and partial effects; no motion");
    label(r.sourceReview,
          "2026-11-02_pdo_motion_plan; EN86p16/EDS1800/1A00; conditional CiA order");
    label(r.independentReview, "nmt_owner_path: sequence/grants/TX-state review");
    label(r.fixtureReview, "S2 TX17 RX16 S18; CL86-C node1; LC60H2127 bolted/free");
    label(r.independentControl,
          "User-confirmed independent drive power cutoff; no firmware stop assumption");
    label(r.failurePlan,
          "First failure ends sequence; retain partial map/uncertainty; no rollback/replay");
    label(r.reconciliation,
          "Read retained exact outcome; quarantine remains; no automatic reset or save");
    plan.configurationReview.target = r.plan.target;
    plan.configurationReview.scope = L::PdoConfigurationScope::ISOLATED_EXPERIMENT;
    plan.configurationReview.fixtureRecord = 20261102;
    plan.configurationReview.firmwareRecord = 2;
    plan.configurationReview.procedureRecord = 18;
    plan.configurationReview.authorizationRecord = 20261102;
    plan.slot = 0;
    plan.firstAuthorizationId = 3000;
    plan.maximumAgeUs = 3000000;
    // Freeze the same conservative age policy through native handoff and SDK
    // submission, not just in the collector's pre-admission snapshot check.
    return firmware.running() && services.setServiceAge(0, plan.maximumAgeUs, now) &&
           pdo.begin(plan, now) == E::EvidenceAdmission::ACCEPTED;
}
bool dumpPdo(E::FirmwareProbe& firmware, E::PdoExperiment& pdo, unsigned& cursor) {
    const auto* r = pdo.record();
    if (!r || !r->terminal)
        return false;
    char out[240] = {};
    if (!cursor) {
        formatLine(
            out, sizeof out,
            "PDO failure=%u lost=%u terminals=%u traffic=%u tail=%u tailLost=%u "
            "confirmed=%u effects=%u uncertain=%u ackMask=%02x cfg=%lu migrated=%u gap=%llu\n",
            unsigned(r->failure), unsigned(r->evidenceLost), unsigned(r->terminalCount),
            unsigned(r->trafficCount), unsigned(r->tailCount), unsigned(r->tailOverflow),
            unsigned(r->configuration.mapConfirmed), unsigned(r->configuration.effectsPossible),
            unsigned(r->configuration.uncertain), unsigned(r->configuration.acknowledgedMask),
            static_cast<unsigned long>(r->current.configurationGeneration),
            unsigned(r->configurationMigrated),
            static_cast<unsigned long long>(r->maxServiceGapUs));
    } else if (cursor <= r->terminalCount) {
        const auto& t = r->terminals[cursor - 1].service;
        formatLine(out, sizeof out,
                   "RESULT op=%lu step=%lu object=%04x:%02x dir=%u outcome=%u ack=%u "
                   "possible=%u uncertain=%u "
                   "abort=%08lx size=%u data=%02x%02x%02x%02x admitted=%llu completed=%llu\n",
                   static_cast<unsigned long>(t.transfer.operation),
                   static_cast<unsigned long>(t.transfer.step), unsigned(t.transfer.index),
                   unsigned(t.transfer.subindex), unsigned(t.transfer.direction),
                   unsigned(t.outcome), unsigned(t.protocolAcknowledged),
                   unsigned(t.effectPossible), unsigned(t.effectUncertain),
                   static_cast<unsigned long>(t.abortCode), unsigned(t.size), unsigned(t.data[0]),
                   unsigned(t.data[1]), unsigned(t.data[2]), unsigned(t.data[3]),
                   static_cast<unsigned long long>(t.admittedUs),
                   static_cast<unsigned long long>(t.completedUs));
    } else if (cursor <= unsigned(r->terminalCount) + r->trafficCount + r->tailCount) {
        const unsigned index = cursor - r->terminalCount - 1;
        const bool tail = index >= r->trafficCount;
        trafficLine(out, sizeof out, tail ? r->tail[index - r->trafficCount] : r->traffic[index],
                    tail ? "TAIL" : "TRAFFIC");
    } else
        label(out, "END PDO; map readback and sample qualification are separate; r=release\n");
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
    static DRAM_ATTR E::PdoExperiment pdo(firmware.owner(), services);
    // TJA1051 S=1 suppresses bus transmission. Set the latch before enabling GPIO.
    const bool silentReady = gpio_set_level(GPIO_NUM_18, 1) == ESP_OK &&
                             gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT) == ESP_OK;
    if (!console.begin()) {
        for (;;)
            vTaskDelay(1);
    }
    line(firmware, "S2_BENCH ready; UART115200; CAN disabled; p=ping c=start s=stats 3/4=raw "
                   "v/b=motion-reads\n");
    bool attempted = false, configured[2] = {}, used[2] = {}, heartbeatUsed[2] = {},
         readinessUsed[2] = {}, dumping = false, reported = false;
    C::NmtRequestId nmtRequest;
    bool nmtReported = false, networkDump = false;
    bool pdoUsed = false;
    unsigned networkCursor = 0;
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
        pdo.service(firmware.owner().lastTimeUs());
        if (firmware.progressionFailed() && !silent)
            silent = gpio_set_level(GPIO_NUM_18, 1) == ESP_OK;
        const E::AcquisitionRecord* record = firmware.evidence().record();
        const E::RawEvidenceRecord* rawRecord = raw.record();
        const auto* pdoRecord = pdo.record();
        if (nmtRequest.owner && !nmtReported) {
            const auto* result = firmware.owner().nmtResult(nmtRequest);
            if (result) {
                char out[220];
                formatLine(
                    out, sizeof out,
                    "NMT request=%lu target=%lu cmd=%u outcome=%u possible=%u txConfirmed=%u "
                    "completed=%llu; retained; network observation separate\n",
                    static_cast<unsigned long>(result->transfer.request),
                    static_cast<unsigned long>(result->transfer.target.binding.endpoint.address),
                    unsigned(result->transfer.command), unsigned(result->outcome),
                    unsigned(result->transmissionPossible), unsigned(result->transmissionConfirmed),
                    static_cast<unsigned long long>(result->completedUs));
                nmtReported = firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out),
                                                              std::strlen(out));
            }
        }
        if (((record && record->terminal) || (rawRecord && rawRecord->terminal) ||
             (pdoRecord && pdoRecord->terminal)) &&
            !reported) {
            reported = true;
            dumping = true;
            dumpCursor = 0;
            services.clearUploadExperiment(0);
            services.clearUploadExperiment(1);
        }
        if (dumping)
            dumping = pdoRecord   ? dumpPdo(firmware, pdo, dumpCursor)
                      : rawRecord ? dumpRaw(firmware, raw, dumpCursor)
                                  : dump(firmware, dumpCursor);
        else if (networkDump)
            networkDump = dumpNetwork(firmware, services, networkCursor);
        uint8_t command = 0;
        if (firmware.platform().takeInput(command)) {
            char output[240] = {};
            if (command == 'v' || command == 'b') {
                const unsigned node = command == 'v' ? 1 : 2;
                const bool accepted =
                    controllerReady && !pdoRecord && !dumping && !networkDump &&
                    !nmtRequest.owner && !readinessUsed[node - 1] && acquisition != UINT32_MAX &&
                    firmware.platform().nowUs(now) &&
                    identity(firmware, services, raw, true, node, configured[node - 1],
                             acquisition + 1, now, false, true);
                if (accepted) {
                    ++acquisition;
                    readinessUsed[node - 1] = true;
                    reported = false;
                }
                line(firmware, accepted ? "MOTION_READINESS admitted: thirty uploads only; no "
                                          "enable/target/stop/save/retry\n"
                                        : "MOTION_READINESS refused; no work\n");
            } else if (command == 'm') {
                const bool accepted = controllerReady && !pdoUsed && !dumping && !networkDump &&
                                      !nmtRequest.owner && !record && !rawRecord && !pdoRecord &&
                                      acquisition < UINT32_MAX - 1 &&
                                      firmware.platform().nowUs(now) &&
                                      startMap(firmware, services, pdo, acquisition + 1, now);
                if (accepted) {
                    acquisition += 2;
                    pdoUsed = true;
                    reported = false;
                }
                line(firmware, accepted ? "PDO admitted: node1 fixed TPDO1 status+position; six "
                                          "writes; no motion/save/retry\n"
                                        : "PDO refused; no work\n");
            } else if (command == 'h' || command == 'j') {
                const unsigned node = command == 'h' ? 1 : 2;
                const bool accepted = controllerReady && !pdoRecord && !dumping && !networkDump &&
                                      !nmtRequest.owner && !heartbeatUsed[node - 1] &&
                                      acquisition != UINT32_MAX && firmware.platform().nowUs(now) &&
                                      identity(firmware, services, raw, true, node,
                                               configured[node - 1], acquisition + 1, now, true);
                if (accepted) {
                    ++acquisition;
                    heartbeatUsed[node - 1] = true;
                    reported = false;
                }
                line(firmware, accepted ? "HEARTBEAT experiment admitted: exact1017 zero->1000ms "
                                          "thenreadback; no retry\n"
                                        : "HEARTBEAT refused; no work\n");
            } else if (command == 'q' || command == 'w') {
                const unsigned slot = command == 'q' ? 0 : 1;
                const auto* node = services.node(slot);
                C::NmtTransfer work;
                if (node)
                    work.target = node->route;
                work.command = C::NmtCommand::PRE_OPERATIONAL;
                work.bus = work.target.binding.endpoint.bus;
                work.request = acquisition + 1;
                work.queueTimeoutUs = 250000;
                const bool timed = firmware.platform().nowUs(now) && now < UINT64_MAX - 5000000;
                if (timed)
                    work.deadlineUs = now + 5000000;
                const bool accepted = timed && acquisition != UINT32_MAX && controllerReady &&
                                      node && !dumping && !networkDump && !rawRecord && !record &&
                                      !pdoRecord && !nmtRequest.owner &&
                                      firmware.owner().admitNmt(work, now, nmtRequest);
                if (accepted) {
                    ++acquisition;
                    nmtReported = false;
                }
                line(firmware, accepted ? "NMT Pre-operational admitted; no drive-stop claim\n"
                                        : "NMT refused; no work\n");
            } else if (command == 'n') {
                if (!dumping && !networkDump) {
                    networkDump = true;
                    networkCursor = 0;
                }
            } else if (command >= '1' && command <= '4') {
                const bool rawPlan = command >= '3';
                const unsigned node = command - (rawPlan ? '2' : '0');
                const bool accepted = controllerReady && !pdoRecord && !dumping && !networkDump &&
                                      !nmtRequest.owner && !used[node - 1] &&
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
                if (((record && record->terminal) || (rawRecord && rawRecord->terminal) ||
                     (pdoRecord && pdoRecord->terminal)) &&
                    !dumping) {
                    dumping = true;
                    dumpCursor = 0;
                } else {
                    line(firmware, "DUMP unavailable\n");
                }
            } else if (command == 'r') {
                if (pdoRecord) {
                    line(firmware, !dumping && pdo.release()
                                       ? "PDO record released; configuration/uncertainty retained\n"
                                       : "PDO release refused\n");
                    continue;
                }
                if (nmtRequest.owner) {
                    if (firmware.owner().releaseNmt(nmtRequest)) {
                        nmtRequest = C::NmtRequestId();
                        line(firmware, "NMT result released; state/uncertainty retained\n");
                    } else
                        line(firmware, "NMT release refused\n");
                    continue;
                }
                if (rawRecord && rawRecord->terminal &&
                    rawRecord->failure == E::RawEvidenceFailure::NONE) {
                    for (unsigned i = 0; i < rawRecord->terminalCount; ++i) {
                        const auto& done = rawRecord->terminals[i].service;
                        if (done.transfer.index == 0x1017 &&
                            done.transfer.direction == C::SdoDirection::UPLOAD && done.size == 2) {
                            const unsigned slot = rawRecord->plan.slot;
                            const auto* node = services.node(slot);
                            if (node && done.nodeGeneration == node->route.generation &&
                                MotorControlCAN::sameBinding(done.transfer.channel.binding,
                                                             node->route.binding))
                                services.setProducerInterval(
                                    slot, node->route,
                                    uint16_t(done.data[0] | uint16_t(done.data[1]) << 8),
                                    firmware.owner().lastTimeUs());
                        }
                    }
                }
                line(firmware,
                     !dumping && (rawRecord ? raw.release() : firmware.evidence().release())
                         ? "RELEASED record; channel uncertainty unchanged\n"
                         : "RELEASE refused\n");
            } else if (command == 'p') {
                const esp_app_desc_t* app = esp_app_get_description();
                char hash[65] = {};
                for (unsigned i = 0; i < 32; ++i)
                    formatLine(hash + i * 2, 3, "%02x", unsigned(app->app_elf_sha256[i]));
                formatLine(output, sizeof(output),
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
                const O::Stats stats = firmware.owner().stats();
                formatLine(output, sizeof(output),
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
