// SPDX-License-Identifier: MIT
// Finite board bring-up on OPTAK_FGI_2025. No motor command or startup CAN request.
#include "BenchConsole.h"
#include "BenchHeap.h"
#include "BenchRoute.h"
#include "CommunicationBench.h"
#include "DiscoveryBench.h"
#include "EspProbeMain.h"
#include "EspProbePlatform.h"
#include "EspTransceiverPin.h"
#include "FirmwareProbe.h"
#include "IdfTwaiDriver.h"
#include "PdoCommand.h"
#include "PdoExperiment.h"
#include "PdoWatch.h"
#include "RawEvidence.h"
#include "SilentCapture.h"
#include "Soak.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

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

// Output is copied once into the ordinary bounded platform queue; CAN service never
// waits for serial output. A one-shot response that does not fit waits here, and no
// further command is taken until it has gone, so a response is not silently lost.
// 320 bytes: PONG with every field at its widest is 300 with its newline (scripts/s2_pdo_test.py).
DRAM_ATTR char retainedResponse[320];
size_t retainedSize = 0;
bool written(E::FirmwareProbe& firmware, const char* text, size_t size) {
    return firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(text), size);
}
void line(E::FirmwareProbe& firmware, const char* text) {
    const size_t size = std::strlen(text);
    if (!retainedSize && written(firmware, text, size))
        return;
    if (!retainedSize && size < sizeof retainedResponse) {
        std::memcpy(retainedResponse, text, size);
        retainedSize = size;
    }
}
void flushResponse(E::FirmwareProbe& firmware) {
    if (retainedSize && written(firmware, retainedResponse, retainedSize))
        retainedSize = 0;
}
// The one path to GPIO18, the TJA1051 S input (high = transmitter off, receiver kept): boot, `c`,
// a progression failure and the capture all request its level here, so this one variable holds
// the last request. A request is what the GPIO driver accepted, never a measured pin or bus.
const int SILENT_GPIO = 18;
// The fixture's bitrate: the one controller configuration `c` applies, and the D line's interface fact.
const uint32_t BENCH_BITRATE = 1000000;
E::TransceiverState silentLevel = E::TransceiverState::NOT_CONTROLLED;
bool driveSilent(void*, int gpio, bool high) {
    const E::SilentPin pin = E::espSilentPin();
    const bool accepted = pin.drive(pin.context, gpio, high);
    silentLevel = !accepted ? E::TransceiverState::FAILED
                  : high    ? E::TransceiverState::SILENT_REQUESTED
                            : E::TransceiverState::NORMAL_REQUESTED;
    return accepted;
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
bool ensureRoute(C::SdoClient& services, unsigned node, bool& configured, uint64_t now) {
    if (configured)
        return true;
    // Explicit bench premise: exclusive, initially unused server session.
    // This is NOT service confirmation. An accepted upload (DEC-1) can confirm
    // the route when it succeeds. Any other upload needs an exact finite
    // experiment grant before ordinary owner admission.
    const L::ReadTarget selected = target(node);
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
    return true;
}
bool identity(E::FirmwareProbe& firmware, C::SdoClient& services, E::RawEvidence* raw, bool rawPlan,
              unsigned node, bool& configured, uint32_t id, uint64_t now, bool heartbeat = false,
              bool motionReadiness = false, const E::PdoCommand* pdoCommand = nullptr,
              uint32_t pdoGrant = 0) {
    // A raw plan needs the arena's raw collector; the typed plan runs through DeviceEvidence.
    if (!firmware.running() || firmware.evidence().record() || (rawPlan && !raw) ||
        (raw && raw->record()))
        return false;
    L::ReadTarget selected = target(node);
    if (configured && services.channel(node - 1) && services.node(node - 1)) {
        selected.channel = *services.channel(node - 1);
        selected.nodeGeneration = services.node(node - 1)->route.generation;
    }
    if (!ensureRoute(services, node, configured, now))
        return false;
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
    // Both bench specimens answer 1009 and 100A in one byte (packet 4). Nothing detects this.
    review.plan.options.identityWidths = L::IdentityWidths::MEASURED_U8;
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
        plan.pdo = decltype(plan.pdo)(); // The plan is static: no earlier selection may survive.
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
        } else if (pdoCommand) {
            // Grants share one increasing per-slot sequence with the fixed experiments above, so
            // these start above theirs: heartbeat and TPDO1 come first in a boot, or this refuses.
            const bool write = pdoCommand->kind == E::PdoCommandKind::WRITE;
            plan.inventory =
                write ? E::RawEvidenceInventory::PDO_COMM_WRITE : E::RawEvidenceInventory::PDO_RECORD;
            plan.firstAuthorizationId = pdoGrant;
            plan.includeState = plan.includeNetwork = false;
            plan.pdo.transmit = pdoCommand->transmit;
            plan.pdo.set = pdoCommand->set;
            plan.pdo.subindex = pdoCommand->subindex;
            plan.pdo.baseline = pdoCommand->baseline;
            plan.pdo.value = pdoCommand->value;
            label(review.plan.authorization, "user 2026-11-06: PDO stream tests, start/stop, timings");
            if (write) {
                label(review.sourceReview,
                      "EDS1800..1803 RW; exact read, one write, readback; units/effects measured");
                label(review.plan.observations,
                      "Exact baseline, one write, exact readback; abort kept; no sample, unit or "
                      "effect claim");
            } else {
                label(review.sourceReview,
                      "EDS1400..1403/1600..1603/1800..1803/1A00..1A03; raw widths; Q17 open");
                label(review.plan.observations,
                      "Nine raw uploads of one PDO set; map and timing as read, unqualified");
            }
            plan.review = review;
        }
        return raw->begin(plan, now) == E::EvidenceAdmission::ACCEPTED;
    }
    return firmware.evidence().requestExperiment(review, now) == E::EvidenceAdmission::ACCEPTED;
}

// Emit one complete retained record line per turn. No diagnostic queue is
// drained here: DeviceEvidence already copied the sole owner's observations.
void trafficLine(char* out, size_t size, const E::EvidenceTraffic& t, const char* label) {
    formatLine(out, size,
               "%s k=%u p=%llu id=%lx ext=%u rtr=%u dlc=%u data=%02x%02x%02x%02x%02x%02x%02x%02x "
               "ok=%u exact=%u t=%llu bounds=%u lo=%llu hi=%llu epoch=%lu",
               label, unsigned(t.kind), static_cast<unsigned long long>(t.processedUs),
               static_cast<unsigned long>(t.frame.id), unsigned(t.frame.extended),
               unsigned(t.frame.rtr), unsigned(t.frame.dlc), unsigned(t.frame.data[0]),
               unsigned(t.frame.data[1]), unsigned(t.frame.data[2]), unsigned(t.frame.data[3]),
               unsigned(t.frame.data[4]), unsigned(t.frame.data[5]), unsigned(t.frame.data[6]),
               unsigned(t.frame.data[7]), unsigned(t.accepted), unsigned(t.receivedTimeKnown),
               static_cast<unsigned long long>(t.receivedUs), unsigned(t.receiveBoundsKnown),
               static_cast<unsigned long long>(t.receivedEarliestUs),
               static_cast<unsigned long long>(t.receivedLatestUs),
               static_cast<unsigned long>(t.controllerEpoch));
    const size_t used = std::strlen(out);
    // Only the fields this kind defines: a default state is not an observed one.
    if (t.kind == E::DiagnosticKind::CONTROLLER)
        formatLine(out + used, size - used, " ctrl=%u\n", unsigned(t.controller));
    else if (t.kind == E::DiagnosticKind::RECOVERY)
        formatLine(out + used, size - used, " rec=%u\n", unsigned(t.recovery));
    else if (t.kind == E::DiagnosticKind::TX || t.kind == E::DiagnosticKind::TX_REPORT)
        formatLine(out + used, size - used, " txk=%u txr=%lu txf=%lu\n", unsigned(t.txId.kind),
                   static_cast<unsigned long>(t.txId.kind == O::TransportTxId::Kind::NMT
                                                  ? t.txId.nmt.generation
                                                  : t.txId.sdo.request.generation),
                   static_cast<unsigned long>(t.txId.sdo.frame));
    else
        formatLine(out + used, size - used, "\n");
}
// Every stage the terminal can name: owner request, reservation epoch, local closure and
// the progress of its frame (st bits: adapter admitted, controller accepted, queued,
// possible TX, confirmed TX, acknowledged, uncertain, cleanup queued).
void resultLine(char* out, size_t size, const O::Completion& done) {
    const C::SdoCompletion& t = done.service;
    const unsigned stages =
        unsigned(done.adapterAdmitted) | unsigned(done.controllerAccepted) << 1 |
        unsigned(t.queued) << 2 | unsigned(t.transmissionPossible) << 3 |
        unsigned(t.transmissionConfirmed) << 4 | unsigned(t.protocolAcknowledged) << 5 |
        unsigned(t.effectUncertain) << 6 | unsigned(t.clientAbortQueued) << 7;
    formatLine(out, size,
               "RESULT op=%lu req=%lu oreq=%lu step=%lu object=%04x:%02x dir=%u outcome=%u ack=%u "
               "possible=%u uncertain=%u abort=%08lx cleanup=%u size=%u data=%02x%02x%02x%02x "
               "admitted=%llu completed=%llu ne=%lu reason=%u st=%02x\n",
               static_cast<unsigned long>(t.transfer.operation),
               static_cast<unsigned long>(t.transfer.request),
               static_cast<unsigned long>(done.id.generation),
               static_cast<unsigned long>(t.transfer.step), unsigned(t.transfer.index),
               unsigned(t.transfer.subindex), unsigned(t.transfer.direction), unsigned(t.outcome),
               unsigned(t.protocolAcknowledged), unsigned(t.effectPossible),
               unsigned(t.effectUncertain), static_cast<unsigned long>(t.abortCode),
               unsigned(t.clientAbortQueued), unsigned(t.size), unsigned(t.data[0]),
               unsigned(t.data[1]), unsigned(t.data[2]), unsigned(t.data[3]),
               static_cast<unsigned long long>(t.admittedUs),
               static_cast<unsigned long long>(t.completedUs),
               static_cast<unsigned long>(t.nodeGeneration), unsigned(done.reason), stages);
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
        if (!written(firmware, out, std::strlen(out)))
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
        // A refused END keeps this listing open, so the record cannot end without it.
        static const char end[] =
            "END network; boot-up requires an actual observed00 frame; no inferred reset\n";
        return !written(firmware, end, sizeof end - 1);
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
    char out[256] = {};
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
        resultLine(out, sizeof out, r->terminals[cursor - 1]);
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
    char out[256] = {};
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
        resultLine(out, sizeof out, r->terminals[cursor - 1]);
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
bool startMap(E::FirmwareProbe& firmware, C::SdoClient& services, E::PdoExperiment* pdo,
              uint32_t operation, uint64_t now) {
    const auto* channel = services.channel(0);
    const auto* node = services.node(0);
    if (!pdo || !channel || !node || now > UINT64_MAX - 8000000)
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
    plan.request.transition = L::Tpdo1Transition::MAP3_TO_MAP2;
    plan.request.eventTimer = L::Tpdo1EventTimerRaw::RAW_0;
    plan.slot = 0;
    plan.firstAuthorizationId = 3000;
    plan.maximumAgeUs = 3000000;
    // Freeze the same conservative age policy through native handoff and SDK
    // submission, not just in the collector's pre-admission snapshot check. A refusal restores it.
    return firmware.running() &&
           pdo->beginWithServiceAge(plan, now) == E::EvidenceAdmission::ACCEPTED;
}
// The supported TPDO1 event-timer change (`E`): the same harness and owner as `m`, on node 1 or 2,
// through the profile's typed lifecycle, never the raw `W` experiment. Its nineteen grants follow
// the R and W lines' sequence on that slot, so `m` (grants 3000..3023 on node 1) must come before
// any of them in a boot. Like `m`, an accepted line sets the node's service evidence age to 3 s
// for the boot; a refused one leaves it as it was.
bool startConfig(E::FirmwareProbe& firmware, C::SdoClient& services, E::PdoExperiment* pdo,
                 bool& configured, unsigned node, uint16_t eventTimer, uint32_t operation,
                 uint32_t firstGrant, uint64_t now) {
    const unsigned slot = node - 1;
    if (!pdo || !firmware.running() || now > UINT64_MAX - 8000000 ||
        (eventTimer != 0 && eventTimer != 100) ||
        !ensureRoute(services, node, configured, now))
        return false;
    const auto* channel = services.channel(slot);
    const auto* state = services.node(slot);
    if (!channel || !state)
        return false;
    // Static, set whole on every call: no field of an earlier request survives.
    static E::PdoExperimentPlan plan;
    auto& r = plan.review;
    r.plan.id = operation;
    r.plan.target.channel = *channel;
    r.plan.target.nodeGeneration = state->route.generation;
    r.plan.kind = L::ReadKind::IDENTITY;
    r.plan.stateMask = 0;
    r.plan.deadlineUs = now + 8000000;
    r.plan.options = L::ReadOptions();
    r.plan.options.queueTimeoutUs = r.plan.options.responseTimeoutUs = 250000;
    label(r.plan.fixtureId, "OPTAK-S2-20261102-two-CL86");
    label(r.plan.softwareRevision, MOTORCONTROLCAN_SOURCE_REVISION);
    label(r.plan.firmware, "CL86-C raw0C/20/0400/0200; packet6 record");
    label(r.plan.dialect, "raw; status dialect unresolved");
    label(r.plan.authorization, "user: bench continuation, TPDO1 1800:05 0<->100 only");
    label(r.plan.observations,
          "19 exact services, one 1800:05 write; traffic and effects retained; no motion");
    label(r.sourceReview,
          "EDS 1800:05 RW U16; packet6 1302Z/1315Z/1316Z/1317Z exact readback; unit open");
    label(r.independentReview, "packet6 transition record; packet7 core and owner vectors");
    label(r.fixtureReview, "S2 TX17 RX16 S18; CL86-C nodes1/2 1Mbit; LC60H2127");
    label(r.independentControl,
          "User-confirmed independent drive power cutoff; no firmware stop assumption");
    label(r.failurePlan,
          "First failure ends sequence; retain write uncertainty; no rollback/replay");
    label(r.reconciliation,
          "Read retained exact outcome; a restoration is a separate explicit E line");
    plan.configurationReview.target = r.plan.target;
    plan.configurationReview.scope = L::PdoConfigurationScope::FIXTURE_QUALIFIED;
    plan.configurationReview.fixtureRecord = 20261102;
    plan.configurationReview.firmwareRecord = 6;
    plan.configurationReview.procedureRecord = 7;
    plan.configurationReview.authorizationRecord = 20261106;
    plan.request.transition = L::Tpdo1Transition::EVENT_TIMER;
    plan.request.eventTimer =
        eventTimer ? L::Tpdo1EventTimerRaw::RAW_100 : L::Tpdo1EventTimerRaw::RAW_0;
    plan.slot = uint8_t(slot);
    plan.firstAuthorizationId = firstGrant;
    plan.maximumAgeUs = 3000000;
    return pdo->beginWithServiceAge(plan, now) == E::EvidenceAdmission::ACCEPTED;
}
bool dumpPdo(E::FirmwareProbe& firmware, E::PdoExperiment& pdo, unsigned& cursor) {
    const auto* r = pdo.record();
    if (!r || !r->terminal)
        return false;
    char out[256] = {};
    if (!cursor) {
        // transition: 1 map3-to-map2, 2 event timer; raw: the requested 1800:05 literal, no unit.
        // lastStep, effectStep and failedStep are the core's confirmedStep, firstEffectStep and
        // failedStep, step 1 being the first write; 0 means none.
        const auto& c = r->configuration;
        formatLine(
            out, sizeof out,
            "PDO failure=%u lost=%u terminals=%u traffic=%u tail=%u tailLost=%u "
            "confirmed=%u effects=%u uncertain=%u ackMask=%02x cfg=%lu migrated=%u gap=%llu "
            "node=%lu transition=%u raw=%u lastStep=%u effectStep=%u failedStep=%u\n",
            unsigned(r->failure), unsigned(r->evidenceLost), unsigned(r->terminalCount),
            unsigned(r->trafficCount), unsigned(r->tailCount), unsigned(r->tailOverflow),
            unsigned(c.mapConfirmed), unsigned(c.effectsPossible), unsigned(c.uncertain),
            unsigned(c.acknowledgedMask),
            static_cast<unsigned long>(r->current.configurationGeneration),
            unsigned(r->configurationMigrated),
            static_cast<unsigned long long>(r->maxServiceGapUs),
            static_cast<unsigned long>(r->plan.review.plan.target.channel.binding.endpoint.address),
            unsigned(r->plan.request.transition), unsigned(r->plan.request.eventTimer),
            unsigned(c.confirmedStep), unsigned(c.firstEffectStep), unsigned(c.failedStep));
    } else if (cursor <= r->terminalCount) {
        resultLine(out, sizeof out, r->terminals[cursor - 1]);
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
// Soak resources. The high-water scan is why this runs per summary, not per turn.
void soakSample(void* context, E::SoakResources& output) {
    static_cast<const E::IdfTwaiDriver*>(context)->errorCounters(output.txErrors, output.rxErrors);
    output.stackFree = uint32_t(uxTaskGetStackHighWaterMark(nullptr));
    output.heapFree = uint32_t(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    output.heapMinimum = uint32_t(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}
// One allocation on the first explicit soak request, never freed. The linker's
// dram0_0_seg limit is far smaller than the internal heap behind it, and a build that
// never soaks spends nothing. A failure refuses the soak; nothing else depends on it.
E::Soak* soakStorage(E::FirmwareProbe& firmware, C::SdoClient& services, E::SoakTwaiTap& tap,
                     E::IdfTwaiDriver& driver) {
    static E::Soak* soak = nullptr;
    if (soak)
        return soak;
    void* memory = heap_caps_aligned_alloc(alignof(E::Soak), sizeof(E::Soak),
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!memory)
        return nullptr;
    E::SoakCallbacks callbacks;
    callbacks.context = &driver;
    callbacks.sample = soakSample;
    soak = new (memory) E::Soak(firmware, services, tap, callbacks, MOTORCONTROLCAN_SOURCE_REVISION);
    return soak;
}
const char* startSoak(E::FirmwareProbe& firmware, C::SdoClient& services, E::SoakTwaiTap& tap,
                      E::IdfTwaiDriver& driver, E::Soak*& soak, bool configured[2], const char* text, size_t size,
                      uint64_t now) {
    E::SoakPlan plan;
    if (!E::parseSoakCommand(text, size, plan))
        return "parse";
    if (!ensureRoute(services, 1, configured[0], now) ||
        !ensureRoute(services, 2, configured[1], now))
        return "route";
    soak = soakStorage(firmware, services, tap, driver);
    if (!soak)
        return "memory";
    switch (soak->begin(plan, now)) {
    case E::SoakAdmission::ACCEPTED:
        return nullptr;
    case E::SoakAdmission::BUSY:
        return "busy";
    default:
        return "invalid";
    }
}
// The watch copies frames at the driver tap, so it can run beside any record, NMT request or soak.
bool watchWrite(void* context, const char* text, size_t size) {
    return static_cast<E::FirmwareProbe*>(context)->platform().writeOutput(
        reinterpret_cast<const uint8_t*>(text), size);
}
E::PdoWatch* watchStorage(E::FirmwareProbe& firmware, E::SoakTwaiTap& tap) {
    static E::PdoWatch* watch = nullptr;
    if (watch)
        return watch;
    void* memory = heap_caps_aligned_alloc(alignof(E::PdoWatch), sizeof(E::PdoWatch),
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!memory)
        return nullptr;
    E::PdoWatchOutput output;
    output.context = &firmware;
    output.write = watchWrite;
    watch = new (memory) E::PdoWatch(tap, output);
    return watch;
}
// One addressed NMT request: q and w send Pre-operational, an N line any of its three commands.
bool admitNmt(E::FirmwareProbe& firmware, C::SdoClient& services, unsigned slot,
              C::NmtCommand command, uint32_t id, C::NmtRequestId& output, uint64_t& admittedUs) {
    const auto* node = services.node(slot);
    if (!node)
        return false;
    C::NmtTransfer work;
    work.target = node->route;
    work.command = command;
    work.bus = work.target.binding.endpoint.bus;
    work.request = id;
    work.queueTimeoutUs = 250000;
    uint64_t now = 0;
    if (!firmware.platform().nowUs(now) || now >= UINT64_MAX - 5000000)
        return false;
    work.deadlineUs = now + 5000000;
    if (!firmware.owner().admitNmt(work, now, output))
        return false;
    admittedUs = now;
    return true;
}
// The heap plan (BenchHeap.h). The internal heap is a few regions, allocated first-fit, and the
// route, its two sessions and the shared record arena are each a large share of the largest region:
// with AxisSession at 35592 bytes, a second session allocated after the route and the first no longer
// found a region that held it (2026-11-09, TRIAL_REFUSED reason=memory). So these blocks are reserved
// once, largest first, at the start of the owner task, before the console, the controller or any
// command allocates, and the raw, PDO, D, K and H collectors share the arena instead of living in
// static DRAM: only one of them ever holds a record. The soak, watch and capture stay lazy.
enum ArenaKind : unsigned { ARENA_RAW = 1, ARENA_PDO, ARENA_DISCOVERY, ARENA_COMM, ARENA_HEARTBEAT };
template <class A, class B> constexpr size_t larger(A a, B b) { return a > b ? size_t(a) : size_t(b); }
const size_t ARENA_BYTES =
    larger(larger(larger(sizeof(E::RawEvidence), sizeof(E::PdoExperiment)),
                  larger(sizeof(E::DiscoveryBench), sizeof(E::CommunicationReads))),
           sizeof(E::HeartbeatCommission));
const size_t ARENA_ALIGN =
    larger(larger(larger(alignof(E::RawEvidence), alignof(E::PdoExperiment)),
                  larger(alignof(E::DiscoveryBench), alignof(E::CommunicationReads))),
           alignof(E::HeartbeatCommission));
// Bit order of BenchMemory::granted and HEAP reserved=: route, session 1, session 2, arena, work.
enum { RESERVE_ROUTE, RESERVE_SESSION1, RESERVE_SESSION2, RESERVE_ARENA, RESERVE_WORK, RESERVE_COUNT };
struct BenchMemory {
    E::BenchBlock blocks[RESERVE_COUNT];
    uint32_t granted = 0;
};
BenchMemory benchMemory;
void* internalAllocate(void*, size_t alignment, size_t size) {
    return heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
void reserveBenchHeap(E::BenchArena& arena) {
    E::BenchBlock* b = benchMemory.blocks;
    b[RESERVE_ROUTE].size = sizeof(E::BenchRoute);
    b[RESERVE_ROUTE].alignment = alignof(E::BenchRoute);
    b[RESERVE_SESSION1].size = b[RESERVE_SESSION2].size = sizeof(E::AxisSession);
    b[RESERVE_SESSION1].alignment = b[RESERVE_SESSION2].alignment = alignof(E::AxisSession);
    b[RESERVE_ARENA].size = ARENA_BYTES;
    b[RESERVE_ARENA].alignment = ARENA_ALIGN;
    b[RESERVE_WORK].size = sizeof(E::RouteWork);
    b[RESERVE_WORK].alignment = alignof(E::RouteWork);
    E::BenchAllocator allocator;
    allocator.allocate = internalAllocate;
    benchMemory.granted = E::reserveLargestFirst(allocator, b, RESERVE_COUNT);
    arena.adopt(b[RESERVE_ARENA].memory, ARENA_BYTES);
}
// A reserved block, taken once; afterwards, or when its reservation was refused, the ordinary heap.
void* benchBlock(unsigned which, size_t alignment, size_t size) {
    E::BenchBlock& b = benchMemory.blocks[which];
    void* memory = b.memory;
    b.memory = nullptr;
    return memory ? memory : internalAllocate(nullptr, alignment, size);
}
// One HEAP line: totals, then each internal region in the SDK's allocation order as
// start:size:free:largest-free-block, the reservation mask and the arena's occupant.
struct HeapRegions {
    enum { CAPACITY = 4 }; // Four regions at their widest still fit the 320-byte line.
    unsigned count = 0;
    intptr_t start[CAPACITY] = {}, end[CAPACITY] = {};
    size_t free[CAPACITY] = {}, largest[CAPACITY] = {};
    unsigned overflow = 0;
};
bool walkHeap(walker_heap_into_t heap, walker_block_info_t block, void* context) {
    HeapRegions& r = *static_cast<HeapRegions*>(context);
    unsigned i = 0;
    while (i < r.count && r.start[i] != heap.start)
        ++i;
    if (i == r.count) {
        if (r.count == HeapRegions::CAPACITY) {
            ++r.overflow;
            return true;
        }
        r.start[i] = heap.start;
        r.end[i] = heap.end;
        ++r.count;
    }
    if (!block.used) {
        r.free[i] += block.size;
        if (block.size > r.largest[i])
            r.largest[i] = block.size;
    }
    return true;
}
void heapLine(char* out, size_t size, const E::BenchArena& arena) {
    static HeapRegions regions;
    regions = HeapRegions();
    heap_caps_walk(MALLOC_CAP_INTERNAL, walkHeap, &regions);
    formatLine(out, size, "HEAP free=%lu largest=%lu minimum=%lu regions=%u",
               static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
               static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
               static_cast<unsigned long>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
               regions.count + regions.overflow);
    for (unsigned i = 0; i < regions.count; ++i) {
        const size_t used = std::strlen(out);
        formatLine(out + used, size - used, " r%u=%lx:%lu:%lu:%lu", i,
                   static_cast<unsigned long>(regions.start[i]),
                   static_cast<unsigned long>(regions.end[i] - regions.start[i]),
                   static_cast<unsigned long>(regions.free[i]),
                   static_cast<unsigned long>(regions.largest[i]));
    }
    const size_t used = std::strlen(out);
    formatLine(out + used, size - used, " reserved=%02lx arena=%u arenaBytes=%lu\n",
               static_cast<unsigned long>(benchMemory.granted), arena.kind(),
               static_cast<unsigned long>(arena.capacity()));
}
// Route resources, sampled at admission and at the terminal: the high-water mark is the least
// stack ever free, so a lower value at the terminal was reached during that route.
void routeSample(void* context, E::RouteResources& output) {
    static_cast<const E::IdfTwaiDriver*>(context)->errorCounters(output.txErrors, output.rxErrors);
    output.stackFree = uint32_t(uxTaskGetStackHighWaterMark(nullptr));
    output.heapFree = uint32_t(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    output.heapMinimum = uint32_t(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    output.largestBlock = uint32_t(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}
// Like the soak: one allocation on the first explicit route request and one per node's first
// route, never freed, because dram0_0_seg cannot hold them. A failure refuses the route.
E::BenchRoute* routeStorage(E::FirmwareProbe& firmware, C::SdoClient& services, E::IdfTwaiDriver& driver) {
    static E::BenchRoute* route = nullptr;
    if (route)
        return route;
    void* memory = benchBlock(RESERVE_ROUTE, alignof(E::BenchRoute), sizeof(E::BenchRoute));
    if (!memory)
        return nullptr;
    E::RouteCallbacks callbacks;
    callbacks.context = &driver;
    callbacks.sample = routeSample;
    route = new (memory) E::BenchRoute(firmware, services, callbacks);
    return route;
}
// Each node's session as placed, for that node's console motion attachment (BenchConsole.h).
E::AxisSession* placedSessions[2] = {nullptr, nullptr};
bool sessionStorage(E::FirmwareProbe& firmware, E::BenchRoute& route, unsigned node) {
    if (route.attached(node))
        return true;
    void* memory = benchBlock(node == 1 ? RESERVE_SESSION1 : RESERVE_SESSION2, alignof(E::AxisSession),
                              sizeof(E::AxisSession));
    if (!memory)
        return false;
    E::AxisSession* session = new (memory) E::AxisSession(firmware.owner());
    placedSessions[node - 1] = session;
    return route.attach(node, *session);
}
// One node's console motion attachment, on that node's first arming route: one allocation, never freed,
// beside the console's own blocks. A failure refuses the arming; nothing else depends on it.
bool axisStorage(E::FirmwareProbe& firmware, E::BenchConsole& console, unsigned node) {
    if (console.hasAxis(node))
        return true;
    E::AxisSession* session = placedSessions[node - 1];
    void* memory = session ? heap_caps_aligned_alloc(alignof(E::BenchAxis), sizeof(E::BenchAxis),
                                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
                           : nullptr;
    return memory && console.attachAxis(node, *new (memory) E::BenchAxis(firmware, *session));
}
// The route contexts' work block (settings routes, an observed move): attached on the first route
// from its reservation, or from the ordinary heap on the first route that needs it.
bool workStorage(E::BenchRoute& route, const E::RoutePlan& plan) {
    if (route.workAttached())
        return true;
    const bool needed = plan.kind == E::RouteKind::SETTINGS_2202 || plan.kind == E::RouteKind::IO_DI3 ||
                        plan.kind == E::RouteKind::VELOCITY || plan.kind == E::RouteKind::HOME ||
                        (plan.kind == E::RouteKind::MOVE && plan.observer);
    if (!needed && !benchMemory.blocks[RESERVE_WORK].memory)
        return true;
    void* memory = benchBlock(RESERVE_WORK, alignof(E::RouteWork), sizeof(E::RouteWork));
    return (memory && route.attachWork(*new (memory) E::RouteWork())) || !needed;
}
const char* startRoute(E::FirmwareProbe& firmware, C::SdoClient& services, E::IdfTwaiDriver& driver,
                       E::BenchRoute*& route, E::BenchConsole* console, bool configured[2], const char* text,
                       size_t size, uint64_t now) {
    // The host's CRC-32 of the whole line first: a corrupted digit can still parse as another plan.
    size_t body = 0;
    if (!E::checkRouteIntegrity(text, size, body))
        return "crc";
    E::RoutePlan plan;
    if (!E::parseRouteCommand(text, body, plan))
        return "parse";
    if (!ensureRoute(services, plan.node, configured[plan.node - 1], now))
        return "route";
    // An observed move reads the other node on its own route and session too.
    const unsigned observer = plan.kind == E::RouteKind::MOVE ? plan.observer : 0;
    if (observer && !ensureRoute(services, observer, configured[observer - 1], now))
        return "route";
    route = routeStorage(firmware, services, driver);
    // Both nodes' sessions on the first route, from their reservations: a later two-node route or
    // observed move never depends on what else the heap has given away meanwhile.
    if (route && !route->attached(3 - plan.node))
        sessionStorage(firmware, *route, 3 - plan.node);
    if (!route || !sessionStorage(firmware, *route, plan.node) ||
        (observer && !sessionStorage(firmware, *route, observer)) || !workStorage(*route, plan))
        return "memory";
    // An arming hands its reads to the shared console (BenchConsole.h): it needs the console (`o`) and
    // that node's motion attachment first, so that a successful record is never left unconsumed.
    const bool arming = E::isArming(plan.kind);
    if (arming && !console)
        return "console";
    if (console)
        console->attachRoute(*route);
    if (arming && !axisStorage(firmware, *console, plan.node))
        return "memory";
    return route->begin(plan, now);
}
// One record line per turn, only when it fits the output queue whole.
bool dumpTrial(E::FirmwareProbe& firmware, const E::BenchRoute& route, unsigned& cursor) {
    char out[256];
    if (!route.formatLine(cursor, out, sizeof out))
        return false;
    if (firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out), std::strlen(out)))
        ++cursor;
    return true;
}
// Capture counters at its begin and end only: twai_node_get_info, never per turn.
void captureSample(void* context, E::CaptureSample& output) {
    output.counters =
        static_cast<const E::IdfTwaiDriver*>(context)->errorCounters(output.tec, output.rec);
}
// Like the watch: one allocation on the first capture request, never freed, because dram0_0_seg
// cannot hold it. A failure refuses the capture; nothing else depends on it.
E::SilentCapture* captureStorage(E::FirmwareProbe& firmware, C::SdoClient& services,
                                 E::SoakTwaiTap& tap, E::IdfTwaiDriver& driver) {
    static E::SilentCapture* capture = nullptr;
    static char image[65] = {};
    if (capture)
        return capture;
    void* memory = heap_caps_aligned_alloc(alignof(E::SilentCapture), sizeof(E::SilentCapture),
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!memory)
        return nullptr;
    const esp_app_desc_t* app = esp_app_get_description();
    for (unsigned i = 0; i < 32; ++i)
        formatLine(image + i * 2, 3, "%02x", unsigned(app->app_elf_sha256[i]));
    E::CaptureTransceiver transceiver;
    transceiver.gpio = SILENT_GPIO;
    transceiver.level = E::SilentLevel::HIGH;
    transceiver.pin.drive = driveSilent;
    E::CaptureCallbacks callbacks;
    callbacks.context = &driver;
    callbacks.sample = captureSample;
    capture = new (memory) E::SilentCapture(firmware, services, tap, transceiver, callbacks,
                                            MOTORCONTROLCAN_SOURCE_REVISION, image);
    return capture;
}
bool dumpCapture(E::FirmwareProbe& firmware, const E::SilentCapture& capture, unsigned& cursor) {
    char out[E::CAPTURE_LINE_BYTES];
    if (!capture.formatLine(cursor, out, sizeof out))
        return false;
    if (firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out), std::strlen(out)))
        ++cursor;
    return true;
}
// The shared console (BenchConsole.h). Like the watch: allocated on the first `o` or `@` line, never
// freed, because dram0_0_seg cannot hold it. Three blocks, ProbeDevice, ProbeConsole and the rest,
// each about a third of the whole, because the internal heap's largest free block after the boot
// reservations can be smaller than the whole. A failure frees what it took and refuses the console;
// nothing else depends on it, and an image that never sees either byte allocates nothing and attaches
// no device.
E::BenchConsole* consoleStorage(E::FirmwareProbe& firmware, C::SdoClient& services) {
    static E::BenchConsole* console = nullptr;
    if (console)
        return console;
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    E::BenchConsoleStorage storage;
    storage.device = heap_caps_aligned_alloc(alignof(E::ProbeDevice), sizeof(E::ProbeDevice), caps);
    storage.console = heap_caps_aligned_alloc(alignof(E::ProbeConsole), sizeof(E::ProbeConsole), caps);
    void* memory = heap_caps_aligned_alloc(alignof(E::BenchConsole), sizeof(E::BenchConsole), caps);
    if (!storage.device || !storage.console || !memory) {
        heap_caps_free(storage.device); // Nothing was constructed in any of them yet.
        heap_caps_free(storage.console);
        heap_caps_free(memory);
        return nullptr;
    }
    console = new (memory) E::BenchConsole(firmware, services, storage);
    return console;
}
// One record line per turn, only when it fits the output queue whole.
template <class T> bool dumpLines(E::FirmwareProbe& firmware, const T& source, unsigned& cursor) {
    char out[E::COMMUNICATION_LINE];
    if (!source.formatLine(cursor, out, sizeof out))
        return false;
    if (firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out), std::strlen(out)))
        ++cursor;
    return true;
}
void ownerTask(void*) {
    static DRAM_ATTR C::SdoClient services;
    static DRAM_ATTR E::IdfTwaiDriver driver;
    static DRAM_ATTR E::EspProbePlatform console;
    static DRAM_ATTR E::SoakTwaiTap tap(driver.driver());
    static DRAM_ATTR E::FirmwareProbe firmware(services, tap.driver(), console.callbacks());
    // The raw, PDO, D, K and H collectors share one reserved block (the heap plan above).
    static E::BenchArena arena;
    reserveBenchHeap(arena);
    // TJA1051 S=1 requests silence. espSilentPin latches the level before enabling the output.
    const bool silentReady = driveSilent(nullptr, SILENT_GPIO, true);
    if (!console.begin()) {
        for (;;)
            vTaskDelay(1);
    }
    line(firmware, "S2_BENCH ready; UART115200; CAN disabled; p=ping c=start s=stats 3/4=raw "
                   "v/b=motion-reads S=soak X=soak-stop T=trial A=trial-abort\n");
    bool attempted = false, configured[2] = {}, used[2] = {}, heartbeatUsed[2] = {},
         readinessUsed[2] = {}, dumping = false, reported = false;
    C::NmtRequestId nmtRequest;
    // Admission time of the retained NMT request. A frame received between it and the TX completion
    // may precede or follow the NMT frame on the wire; the result line prints both.
    uint64_t nmtAdmittedUs = 0;
    bool nmtReported = false, networkDump = false;
    bool pdoUsed = false;
    unsigned networkCursor = 0;
    bool controllerReady = false;
    uint32_t acquisition = 0, grants[2] = {}, grantedOperation[2] = {}, grantedStep[2] = {};
    unsigned dumpCursor = 0;
    E::Soak* soak = nullptr;
    E::PdoWatch* watch = nullptr;
    // The bounded silent capture (SilentCapture.h, the C line): allocated on its first request.
    E::SilentCapture* capture = nullptr;
    // N, P, R, W, E and C lines (PdoCommand.h), collected whole like the soak and trial requests.
    char pdoText[48] = {};
    size_t pdoLength = 0;
    bool pdoCollecting = false;
    uint64_t pdoCollectUs = 0;
    // PDO record, write and E-line grants follow every fixed experiment's grants on the slot.
    uint32_t pdoGrant[2] = {10000, 10000};
    // The soak request is the one multi-byte command: `S` then text up to a line end.
    char soakText[40] = {};
    size_t soakLength = 0;
    bool soakCollecting = false;
    uint64_t soakCollectUs = 0;
    // The trial request is the other multi-byte command: `T` then text up to a line end. It is
    // drained whole each turn, because it can be longer than the platform's 64-byte input ring; a
    // move line is the longest, within E::ROUTE_LINE with its integrity field. A longer one is
    // refused whole, never cut. `routes=8` in PONG names this line protocol: the CRC-32 field (3), the
    // rearm's predecessor, the move's relative base and the Q, W and L routes (4), then the D and J
    // settings routes and the move's observer node (5), then the N, V and O routes of prompts 35 and
    // 36 (6), then the V line without the host's start and stop flags, run as the library's attended
    // experiment (7), then the F route, gate Q03's one 60FF write and readback on a released drive (8).
    E::BenchRoute* route = nullptr;
    // The arena's current occupant, refreshed each turn and after each acquisition. D and K never
    // download, so the dispatcher below asks only for the raw, PDO and H collectors: a download of
    // anything else is refused unsent.
    E::RawEvidence* raw = nullptr;
    E::PdoExperiment* pdo = nullptr;
    E::DiscoveryBench* discovery = nullptr;
    E::CommunicationReads* comm = nullptr;
    E::HeartbeatCommission* heartbeat = nullptr;
    auto occupant = [&]() {
        raw = arena.get<E::RawEvidence>(ARENA_RAW);
        pdo = arena.get<E::PdoExperiment>(ARENA_PDO);
        discovery = arena.get<E::DiscoveryBench>(ARENA_DISCOVERY);
        comm = arena.get<E::CommunicationReads>(ARENA_COMM);
        heartbeat = arena.get<E::HeartbeatCommission>(ARENA_HEARTBEAT);
    };
    // The shared console (BenchConsole.h): allocated on the first `o` or `@`. A line starting with
    // `@` belongs to it whole; consoleDiscard swallows such a line when the console has no memory, so
    // that none of its bytes is ever taken as a bench command.
    E::BenchConsole* commands = nullptr;
    bool consoleDiscard = false;
    // One bounded dispatcher owns ordinary download authorization. Constructing a later
    // route cannot replace it or silently authorize another producer's work.
    struct DownloadDispatch {
        E::BenchArena* arena;
        E::BenchRoute** route;
        E::BenchConsole** console;
    } downloads = {&arena, &route, &commands};
    E::DownloadPreflight preflight;
    preflight.context = &downloads;
    preflight.check = [](void* context, MotorControlCANOwner::SequenceId sequence,
                         const C::SdoTransfer& transfer, uint64_t now) {
        DownloadDispatch& d = *static_cast<DownloadDispatch*>(context);
        // The console's own motion downloads first, with its own fault decision: the route's check would
        // accept any session action download, with no fault policy outside a route. Never fall through.
        if (*d.console && (*d.console)->ownsDownload(sequence, transfer))
            return (*d.console)->preflight(sequence, transfer, now);
        const E::RawEvidence* raw = d.arena->get<E::RawEvidence>(ARENA_RAW);
        const E::PdoExperiment* pdo = d.arena->get<E::PdoExperiment>(ARENA_PDO);
        const E::HeartbeatCommission* heartbeat = d.arena->get<E::HeartbeatCommission>(ARENA_HEARTBEAT);
        return (raw && raw->preflight(sequence, transfer, now)) ||
               (pdo && pdo->preflight(sequence, transfer, now)) ||
               (*d.route && (*d.route)->preflight(sequence, transfer, now)) ||
               (heartbeat && heartbeat->preflight(sequence, transfer, now));
    };
    firmware.setPreflight(preflight);
    char trialText[E::ROUTE_LINE] = {};
    size_t trialLength = 0;
    bool trialCollecting = false;
    uint64_t trialCollectUs = 0;
    // The rest of a line whose first byte was refused, or of the line open at an input loss: discarded
    // through its line end, so that none of its bytes is taken as a single-byte bench command. A quiet
    // input for 2 s, as long as a collector waits for a line, ends it too.
    bool lineDiscard = false;
    uint64_t lineDiscardUs = 0;
    // The report of an input loss, printed once the retained one-shot response has gone.
    const char* lossReport = nullptr;
    for (;;) {
        uint64_t now = 0;
        if (firmware.platform().nowUs(now)) {
            const L::ReadContext& read = firmware.app().readContext();
            if (read.state == MotorControlCAN::ReadState::ACTIVE &&
                !firmware.app().pendingRequest().owner) {
                L::PreparedRead next;
                if (L::nextRead(read, now, next) && next.ready) {
                    const unsigned slot = read.target.channel.binding.endpoint.address - 1;
                    if (slot < 2 && next.transfer.readEffect == C::SdoReadEffect::UNQUALIFIED &&
                        (next.transfer.operation != grantedOperation[slot] ||
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
        occupant();
        // After the firmware turn, as every record: each consumes at most one terminal per turn.
        if (raw)
            raw->service(firmware.owner().lastTimeUs());
        if (pdo)
            pdo->service(firmware.owner().lastTimeUs());
        if (discovery)
            discovery->service(firmware.owner().lastTimeUs());
        if (comm)
            comm->service(firmware.owner().lastTimeUs());
        if (heartbeat)
            heartbeat->service(firmware.owner().lastTimeUs());
        if (soak) {
            // After a progression failure the owner's time stops, and the soak still reports.
            uint64_t clock = firmware.owner().lastTimeUs(), sample = 0;
            if (firmware.platform().nowUs(sample) && sample > clock)
                clock = sample;
            soak->service(clock, retainedSize != 0);
        }
        if (route)
            route->service(firmware.owner().lastTimeUs());
        // The watch holds its lines while a one-shot response, a record dump, a network listing or
        // an NMT result waits for room: a stream can fill the console alone (2026-11-06, a 10 ms
        // TPDO starved a write record's dump for 15 s). Held frames still count; the flags are the
        // previous turn's, which only delays the hold by one turn.
        if (watch)
            watch->service(firmware.owner().lastTimeUs(),
                           retainedSize != 0 || dumping || networkDump ||
                               (nmtRequest.owner && !nmtReported));
        if (capture) {
            // After a progression failure the owner's time stops; the capture then ends at once.
            uint64_t clock = firmware.owner().lastTimeUs(), sample = 0;
            if (!firmware.running() && firmware.platform().nowUs(sample) && sample > clock)
                clock = sample;
            capture->service(clock);
        }
        if (firmware.progressionFailed() && silentLevel != E::TransceiverState::SILENT_REQUESTED)
            driveSilent(nullptr, SILENT_GPIO, true);
        const E::AcquisitionRecord* record = firmware.evidence().record();
        const E::RawEvidenceRecord* rawRecord = raw ? raw->record() : nullptr;
        const E::PdoExperimentRecord* pdoRecord = pdo ? pdo->record() : nullptr;
        const E::RouteRecord* trialRecord = route ? route->record() : nullptr;
        const E::CaptureRecord* captureRecord = capture ? capture->record() : nullptr;
        const E::DiscoveryBenchRecord* discoveryRecord = discovery ? discovery->record() : nullptr;
        const E::CommunicationBenchRecord* commRecord = comm ? comm->record() : nullptr;
        const E::HeartbeatBenchRecord* heartbeatRecord = heartbeat ? heartbeat->record() : nullptr;
        // A D, K or H record, running or held, admits nothing else but inspection and release.
        const bool commissionHeld = discoveryRecord || commRecord || heartbeatRecord;
        // The arena may change occupant only while its occupant holds no record.
        const bool arenaIdle = !rawRecord && !pdoRecord && !commissionHeld;
        auto rawStorage = [&]() {
            E::RawEvidence* r =
                arena.acquire<E::RawEvidence>(ARENA_RAW, arenaIdle, firmware.owner(), services);
            occupant();
            return r;
        };
        auto pdoStorage = [&]() {
            E::PdoExperiment* r =
                arena.acquire<E::PdoExperiment>(ARENA_PDO, arenaIdle, firmware.owner(), services);
            occupant();
            return r;
        };
        const bool commissionTerminal = (discoveryRecord && discoveryRecord->terminal) ||
                                        (commRecord && commRecord->terminal) ||
                                        (heartbeatRecord && heartbeatRecord->terminal);
        if (nmtRequest.owner && !nmtReported) {
            const auto* result = firmware.owner().nmtResult(nmtRequest);
            if (result) {
                char out[220];
                formatLine(
                    out, sizeof out,
                    "NMT request=%lu target=%lu cmd=%u outcome=%u possible=%u txConfirmed=%u "
                    "completed=%llu admitted=%llu; retained; network observation separate\n",
                    static_cast<unsigned long>(result->transfer.request),
                    static_cast<unsigned long>(result->transfer.target.binding.endpoint.address),
                    unsigned(result->transfer.command), unsigned(result->outcome),
                    unsigned(result->transmissionPossible), unsigned(result->transmissionConfirmed),
                    static_cast<unsigned long long>(result->completedUs),
                    static_cast<unsigned long long>(nmtAdmittedUs));
                nmtReported = firmware.platform().writeOutput(reinterpret_cast<const uint8_t*>(out),
                                                              std::strlen(out));
            }
        }
        if (((record && record->terminal) || (rawRecord && rawRecord->terminal) ||
             (pdoRecord && pdoRecord->terminal) || (trialRecord && trialRecord->terminal) ||
             (captureRecord && captureRecord->terminal) || commissionTerminal) &&
            !reported) {
            reported = true;
            dumping = true;
            dumpCursor = 0;
            services.clearUploadExperiment(0);
            services.clearUploadExperiment(1);
        }
        if (dumping)
            dumping = trialRecord       ? dumpTrial(firmware, *route, dumpCursor)
                      : pdoRecord       ? dumpPdo(firmware, *pdo, dumpCursor)
                      : rawRecord       ? dumpRaw(firmware, *raw, dumpCursor)
                      : captureRecord   ? dumpCapture(firmware, *capture, dumpCursor)
                      : discoveryRecord ? dumpLines(firmware, *discovery, dumpCursor)
                      : commRecord      ? dumpLines(firmware, *comm, dumpCursor)
                      : heartbeatRecord ? dumpLines(firmware, *heartbeat, dumpCursor)
                                        : dump(firmware, dumpCursor);
        else if (networkDump)
            networkDump = dumpNetwork(firmware, services, networkCursor);
        flushResponse(firmware);
        uint8_t command = 0;
        const bool soakActive = soak && soak->active();
        if (soakCollecting && firmware.platform().nowUs(now) && now - soakCollectUs > 2000000) {
            soakCollecting = false;
            line(firmware, "SOAK_REFUSED reason=incomplete; no work\n");
        }
        if (trialCollecting && firmware.platform().nowUs(now) && now - trialCollectUs > 2000000) {
            trialCollecting = false;
            line(firmware, "TRIAL_REFUSED reason=incomplete; no work\n");
        }
        if (pdoCollecting && firmware.platform().nowUs(now) && now - pdoCollectUs > 2000000) {
            pdoCollecting = false;
            line(firmware, "PDO_LINE refused reason=incomplete; no work\n");
        }
        // A trial record, active or retained, admits nothing else but inspection and release.
        const bool trialHeld = trialRecord != nullptr;
        // A capture collecting, its record held or its gate still closed: only p, s, n, d, r and
        // the C0 stop line are served; whole lines are still collected and then refused.
        const char* captureBlock = capture ? capture->blocking() : nullptr;
        // The shared console's turn, after the firmware's and every bench record's (BenchConsole.h).
        // wire: the bench holds or may use the owner, and the console refuses all but inspection.
        // Its records wait while a bench record dump, listing, one-shot response or NMT result does.
        E::BenchActivity activity;
        // A terminal, settled arming record holds only its dump: the console's motion it armed may run.
        const bool armingHeld = trialRecord && trialRecord->terminal && !trialRecord->unsettled &&
                                E::isArming(trialRecord->plan.kind);
        activity.wire = soakActive || (trialHeld && !armingHeld) || commissionHeld || record || rawRecord ||
                        pdoRecord || nmtRequest.owner || captureBlock;
        activity.listing = networkDump;
        activity.holdOutput = retainedSize != 0 || dumping || networkDump || (nmtRequest.owner && !nmtReported);
        if (commands && firmware.platform().nowUs(now)) {
            if (commands->expire(now))
                line(firmware, "CONSOLE_REFUSED reason=incomplete; no work\n");
            commands->service(now, activity);
            // An arming's handoff line, as a one-shot response once the retained one has gone.
            if (commands->notice() && !retainedSize) {
                line(firmware, commands->notice());
                commands->clearNotice();
            }
        }
        // The console retains an operation, traces or scans: the bench admits no owner work of its own.
        const bool consoleHeld = commands && commands->holds();
        // An input loss: the platform's ring was full and dropped the newest bytes. As ProbeConsole does
        // after a loss, every buffered byte predates it and is retired unserved, the line open at the
        // boundary is dropped with its own refusal, and input is discarded through the next line end. A
        // console line's refusal names its @ID when that prefix arrived whole (BenchConsole::abandon).
        if (firmware.platform().nowUs(now) && firmware.platform().takeInputLoss()) {
            char open = consoleDiscard ? '#'
                        : commands && commands->collecting() ? '@'
                        : trialCollecting                    ? 'T'
                        : soakCollecting                     ? 'S'
                        : pdoCollecting                      ? 'N'
                                                             : 0;
            consoleDiscard = trialCollecting = soakCollecting = pdoCollecting = false;
            uint8_t byte = 0;
            for (unsigned n = 0; n < E::ProbePlatform::INPUT_CAPACITY && firmware.platform().takeInput(byte); ++n) {
                if (byte == '\r' || byte == '\n') {
                    if (commands && commands->collecting())
                        (void)commands->abandon(); // A whole line before the boundary: retired too.
                    open = 0;
                    continue;
                }
                if (!open) // A single-byte command is retired; a line start opens that line.
                    open = byte == '@' ? (commands ? '@' : '#')
                           : byte == 'T' || byte == 'S' ? char(byte)
                           : byte && std::strchr("NPRWECDKH", byte) ? 'N'
                                                            : 0;
                if (open == '@')
                    (void)commands->take(byte, now, activity); // No line end: only its @ID prefix is kept.
            }
            lossReport = open == '@'   ? commands->abandon()
                         : open == '#' ? "CONSOLE_REFUSED reason=input-loss; no work\n"
                         : open == 'T' ? "TRIAL_REFUSED reason=input-loss; no work\n"
                         : open == 'S' ? "SOAK_REFUSED reason=input-loss; no work\n"
                         : open == 'N' ? "PDO_LINE refused reason=input-loss; no work\n"
                                       : "INPUT_LOST; buffered input retired through the next line end, no work\n";
            lineDiscard = true;
            lineDiscardUs = now;
        }
        if (lossReport && !retainedSize) {
            line(firmware, lossReport);
            lossReport = nullptr;
        }
        if (lineDiscard && firmware.platform().nowUs(now) && now - lineDiscardUs > 2000000)
            lineDiscard = false;
        if (!retainedSize && !lossReport && firmware.platform().takeInput(command)) {
            char output[320] = {};
            if (lineDiscard) {
                uint8_t byte = command;
                do {
                    lineDiscardUs = now;
                    lineDiscard = byte != '\r' && byte != '\n';
                } while (lineDiscard && firmware.platform().takeInput(byte));
            } else if (consoleDiscard || (commands && commands->collecting())) {
                // One `@` line, drained whole: no byte of it is a bench command.
                const bool discarding = consoleDiscard;
                E::ConsoleInput taken = E::ConsoleInput::COLLECTING;
                uint8_t byte = command;
                do {
                    if (discarding)
                        consoleDiscard = byte != '\r' && byte != '\n';
                    else
                        taken = commands->take(byte, now, activity);
                } while ((discarding ? consoleDiscard : taken == E::ConsoleInput::COLLECTING) &&
                         firmware.platform().takeInput(byte));
                if (discarding && !consoleDiscard)
                    line(firmware, "CONSOLE_REFUSED reason=memory; no work\n");
                else if (!discarding && taken == E::ConsoleInput::REFUSED)
                    line(firmware, commands->reply());
            } else if (trialCollecting) {
                bool ended = false;
                uint8_t byte = command;
                do {
                    if (byte == '\r' || byte == '\n') {
                        ended = true;
                        break;
                    }
                    if (trialLength < sizeof trialText)
                        trialText[trialLength] = char(byte);
                    if (trialLength <= sizeof trialText)
                        ++trialLength;
                } while (firmware.platform().takeInput(byte));
                if (ended) {
                    trialCollecting = false;
                    const char* refusal = "length";
                    if (trialLength <= sizeof trialText) {
                        refusal = consoleHeld ? "console" : captureBlock ? "capture" : "state";
                        if (!consoleHeld && !captureBlock && controllerReady && !soakActive && !dumping && !networkDump &&
                            !nmtRequest.owner && !record && !rawRecord && !pdoRecord && !trialHeld &&
                            !commissionHeld && firmware.platform().nowUs(now))
                            refusal = startRoute(firmware, services, driver, route, commands, configured,
                                                 trialText, trialLength, now);
                    }
                    if (refusal) {
                        formatLine(output, sizeof output, "TRIAL_REFUSED reason=%s; no work\n",
                                   refusal);
                    } else {
                        reported = false;
                        const E::RouteRecord& admitted = *route->record();
                        formatLine(output, sizeof output,
                                   "TRIAL admitted route=%s node=%u trial=%lu motion=%u case=%u; "
                                   "finite typed route, no retry\n",
                                   E::routeName(admitted.plan.kind), unsigned(admitted.plan.node),
                                   static_cast<unsigned long>(admitted.plan.trial),
                                   unsigned(admitted.motionTotal), unsigned(admitted.caseCount));
                    }
                    line(firmware, output);
                }
            } else if (pdoCollecting) {
                bool ended = false;
                uint8_t byte = command;
                do {
                    if (byte == '\r' || byte == '\n') {
                        ended = true;
                        break;
                    }
                    if (pdoLength < sizeof pdoText)
                        pdoText[pdoLength] = char(byte);
                    if (pdoLength <= sizeof pdoText)
                        ++pdoLength;
                } while (firmware.platform().takeInput(byte));
                if (ended) {
                    pdoCollecting = false;
                    E::PdoCommand parsed;
                    if (pdoLength > sizeof pdoText ||
                        !E::parsePdoCommand(pdoText, pdoLength, parsed)) {
                        line(firmware, "PDO_LINE refused reason=syntax; no work\n");
                    } else if (parsed.kind == E::PdoCommandKind::CAPTURE && !parsed.capture) {
                        line(firmware, capture && capture->requestStop()
                                           ? "CAP_STOPPING; the record follows; nothing sent\n"
                                           : "CAP_STOP refused; none running\n");
                    } else if (parsed.kind == E::PdoCommandKind::CAPTURE) {
                        // The owner, adapter and tap evidence is checked by begin itself; here only
                        // the bench's own activities, each of which can still send or share the ring.
                        const char* refusal = captureBlock ? "busy" : consoleHeld ? "console" : "state";
                        if (!captureBlock && !consoleHeld && controllerReady && !soakActive && !trialHeld &&
                            !(watch && watch->active()) && !dumping && !networkDump &&
                            !nmtRequest.owner && !record && !rawRecord && !pdoRecord &&
                            !commissionHeld && firmware.platform().nowUs(now)) {
                            if (!capture)
                                capture = captureStorage(firmware, services, tap, driver);
                            refusal = "memory";
                            if (capture) {
                                E::CapturePlan plan;
                                plan.id = parsed.capture;
                                plan.seconds = parsed.seconds;
                                plan.frames = parsed.frames;
                                const E::CaptureAdmission admitted = capture->begin(plan, now);
                                refusal = admitted == E::CaptureAdmission::ACCEPTED
                                              ? nullptr
                                              : E::captureAdmissionName(admitted);
                            }
                        }
                        if (refusal) {
                            formatLine(output, sizeof output, "CAP refused reason=%s; no work\n",
                                       refusal);
                        } else {
                            reported = false;
                            formatLine(output, sizeof output,
                                       "CAP admitted cap=%lu seconds=%lu frames=%u gpio=%d "
                                       "silentHigh=1; TX gate closed, silent level requested, "
                                       "nothing sent\n",
                                       static_cast<unsigned long>(parsed.capture),
                                       static_cast<unsigned long>(parsed.seconds),
                                       unsigned(parsed.frames), SILENT_GPIO);
                        }
                        line(firmware, output);
                    } else if (captureBlock) {
                        // The watch shares the capture's tap ring; every other line can send.
                        line(firmware, "PDO_LINE refused reason=capture; no work\n");
                    } else if (parsed.kind == E::PdoCommandKind::WATCH) {
                        if (!parsed.seconds) {
                            line(firmware, watch && watch->requestStop()
                                               ? "PDOW_STOPPING; final summaries follow\n"
                                               : "PDOW_STOP refused; none running\n");
                        } else {
                            if (!watch)
                                watch = watchStorage(firmware, tap);
                            const char* refusal = "memory";
                            if (watch && firmware.platform().nowUs(now)) {
                                switch (watch->begin(parsed.seconds, now)) {
                                case E::PdoWatchAdmission::ACCEPTED:
                                    refusal = nullptr;
                                    break;
                                case E::PdoWatchAdmission::BUSY:
                                    refusal = "busy";
                                    break;
                                default:
                                    refusal = "invalid";
                                }
                            }
                            if (refusal)
                                formatLine(output, sizeof output,
                                           "PDOW refused reason=%s; no work\n", refusal);
                            else
                                formatLine(output, sizeof output,
                                           "PDOW admitted seconds=%lu; copies received frames, "
                                           "sends nothing\n",
                                           static_cast<unsigned long>(parsed.seconds));
                            line(firmware, output);
                        }
                    } else if (soakActive || trialHeld || commissionHeld) {
                        line(firmware, "PDO_LINE refused reason=active; no work\n");
                    } else if (consoleHeld) {
                        // D, K, H, E, N, R and W all admit owner work.
                        line(firmware, "PDO_LINE refused reason=console; no work\n");
                    } else if (parsed.kind == E::PdoCommandKind::DISCOVER) {
                        // Both configured slots, through the firmware's own app and owner; the
                        // scan sends uploads only in p and i modes and nothing in o.
                        const char* refusal = "state";
                        if (controllerReady && !dumping && !networkDump && !nmtRequest.owner &&
                            !record && !rawRecord && !pdoRecord && acquisition != UINT32_MAX &&
                            firmware.platform().nowUs(now)) {
                            refusal = "route";
                            if (ensureRoute(services, 1, configured[0], now) &&
                                ensureRoute(services, 2, configured[1], now)) {
                                discovery = arena.acquire<E::DiscoveryBench>(
                                    ARENA_DISCOVERY, arenaIdle, firmware.app(), firmware.owner(),
                                    services);
                                occupant();
                                refusal = discovery ? nullptr : "memory";
                                if (!refusal) {
                                    E::DiscoveryBenchRequest request;
                                    request.id = acquisition + 1;
                                    request.mode = E::DiscoveryMode(parsed.discovery);
                                    request.seconds = parsed.seconds;
                                    request.select = parsed.select;
                                    request.expect = parsed.expect;
                                    request.bitrate = BENCH_BITRATE; // As `c` configured it.
                                    refusal = discovery->begin(request, now);
                                }
                            }
                        }
                        if (refusal) {
                            formatLine(output, sizeof output, "DISC refused reason=%s; no work\n",
                                       refusal);
                        } else {
                            ++acquisition;
                            reported = false;
                            formatLine(output, sizeof output,
                                       "DISC admitted mode=%u seconds=%lu select=%u expect=%u; "
                                       "uploads only, selection sends nothing; no retry\n",
                                       unsigned(parsed.discovery),
                                       static_cast<unsigned long>(parsed.seconds),
                                       unsigned(parsed.select), unsigned(parsed.expect));
                        }
                        line(firmware, output);
                    } else if (parsed.kind == E::PdoCommandKind::COMMUNICATION ||
                               parsed.kind == E::PdoCommandKind::HEARTBEAT) {
                        // Grants as R, W and E: from this slot's next grant, 95 or 3 of them.
                        const bool reads = parsed.kind == E::PdoCommandKind::COMMUNICATION;
                        const unsigned slot = parsed.node - 1;
                        const uint32_t operations = reads ? 18 : 2, fields = reads ? 95 : 3;
                        // The channel grants only above its last grant, in authorization and in
                        // operation identity: after a route (closed with x) both counters continue
                        // from the channel's, which the route drew from its own higher range.
                        uint32_t lastGrant = 0, lastOperation = 0;
                        if (services.lastExperiment(uint8_t(slot), lastGrant, lastOperation)) {
                            if (pdoGrant[slot] < lastGrant)
                                pdoGrant[slot] = lastGrant;
                            if (acquisition < lastOperation)
                                acquisition = lastOperation;
                        }
                        const uint64_t window = reads ? 15000000 : 5000000;
                        const char* refusal = "state";
                        if (controllerReady && !dumping && !networkDump && !nmtRequest.owner &&
                            !record && !rawRecord && !pdoRecord &&
                            acquisition <= UINT32_MAX - operations &&
                            pdoGrant[slot] <= UINT32_MAX - fields && firmware.platform().nowUs(now) &&
                            now <= UINT64_MAX - window) {
                            refusal = "route";
                            if (ensureRoute(services, parsed.node, configured[slot], now)) {
                                refusal = "memory";
                                if (reads && (comm = arena.acquire<E::CommunicationReads>(
                                                  ARENA_COMM, arenaIdle, firmware.owner(), services))) {
                                    E::CommunicationBenchRequest request;
                                    request.id = acquisition + 1;
                                    request.node = parsed.node;
                                    request.firstGrant = pdoGrant[slot] + 1;
                                    request.deadlineUs = now + window;
                                    occupant();
                                    refusal = comm->begin(request, now);
                                } else if (!reads &&
                                           (heartbeat = arena.acquire<E::HeartbeatCommission>(
                                                ARENA_HEARTBEAT, arenaIdle, firmware.owner(), services))) {
                                    E::HeartbeatBenchRequest request;
                                    request.id = acquisition + 1;
                                    request.node = parsed.node;
                                    request.producerTimeRaw = parsed.producerTime;
                                    request.firstGrant = pdoGrant[slot] + 1;
                                    request.deadlineUs = now + window;
                                    occupant();
                                    refusal = heartbeat->begin(request, now);
                                }
                            }
                        }
                        if (refusal) {
                            formatLine(output, sizeof output, "%s refused reason=%s; no work\n",
                                       reads ? "COMM" : "HB", refusal);
                        } else {
                            acquisition += operations;
                            pdoGrant[slot] += fields;
                            reported = false;
                            if (reads)
                                formatLine(output, sizeof output,
                                           "COMM admitted node=%u groups=18 uploads=95; typed reads "
                                           "only, no write; no retry\n",
                                           unsigned(parsed.node));
                            else
                                formatLine(output, sizeof output,
                                           "HB admitted node=%u raw=%u; exact baseline 0, one "
                                           "1017:00 write, exact readback; no save, reset or "
                                           "retry\n",
                                           unsigned(parsed.node), unsigned(parsed.producerTime));
                        }
                        line(firmware, output);
                    } else if (parsed.kind == E::PdoCommandKind::CONFIGURE) {
                        // Grant sequencing as R and W: nineteen from this slot's next grant.
                        const unsigned slot = parsed.node - 1;
                        const bool accepted =
                            controllerReady && !pdoRecord && !dumping && !networkDump &&
                            !nmtRequest.owner && !record && !rawRecord &&
                            acquisition < UINT32_MAX - 1 && pdoGrant[slot] <= UINT32_MAX - 19 &&
                            firmware.platform().nowUs(now) &&
                            startConfig(firmware, services, pdoStorage(), configured[slot], parsed.node,
                                        parsed.eventTimer, acquisition + 1, pdoGrant[slot] + 1,
                                        now);
                        if (accepted) {
                            acquisition += 2;
                            pdoGrant[slot] += 19;
                            reported = false;
                            formatLine(output, sizeof output,
                                       "PDO_CONFIG admitted node=%u raw=%u; TPDO1 1800:05 raw "
                                       "only: nine exact reads, one write, nine exact reads; no "
                                       "retry\n",
                                       unsigned(parsed.node), unsigned(parsed.eventTimer));
                        } else {
                            formatLine(output, sizeof output, "PDO_CONFIG refused; no work\n");
                        }
                        line(firmware, output);
                    } else if (parsed.kind == E::PdoCommandKind::NMT) {
                        const bool accepted =
                            acquisition != UINT32_MAX && controllerReady && !dumping &&
                            !networkDump && !rawRecord && !record && !pdoRecord &&
                            !nmtRequest.owner &&
                            admitNmt(firmware, services, parsed.node - 1,
                                     C::NmtCommand(parsed.nmt), acquisition + 1, nmtRequest,
                                     nmtAdmittedUs);
                        if (accepted) {
                            ++acquisition;
                            nmtReported = false;
                            formatLine(output, sizeof output,
                                       "NMT admitted node=%u cmd=%u; addressed, no drive-stop "
                                       "claim; network observation separate\n",
                                       unsigned(parsed.node), unsigned(parsed.nmt));
                            line(firmware, output);
                        } else {
                            line(firmware, "NMT refused; no work\n");
                        }
                    } else {
                        const bool write = parsed.kind == E::PdoCommandKind::WRITE;
                        const unsigned slot = parsed.node - 1;
                        const uint32_t fields = write ? 3 : 9;
                        const bool accepted =
                            controllerReady && !pdoRecord && !dumping && !networkDump &&
                            !nmtRequest.owner && acquisition != UINT32_MAX &&
                            pdoGrant[slot] <= UINT32_MAX - fields &&
                            firmware.platform().nowUs(now) &&
                            identity(firmware, services, rawStorage(), true, parsed.node,
                                     configured[slot], acquisition + 1, now, false, false,
                                     &parsed, pdoGrant[slot] + 1);
                        if (accepted) {
                            ++acquisition;
                            pdoGrant[slot] += fields;
                            reported = false;
                        }
                        if (!accepted)
                            formatLine(output, sizeof output, "%s refused; no work\n",
                                       write ? "PDO_WRITE" : "PDO_READ");
                        else if (write)
                            formatLine(output, sizeof output,
                                       "PDO_WRITE admitted node=%u set=%u sub=%u baseline=%lu "
                                       "value=%lu; exact read, one write, exact readback; no "
                                       "retry\n",
                                       unsigned(parsed.node), unsigned(parsed.set),
                                       unsigned(parsed.subindex),
                                       static_cast<unsigned long>(parsed.baseline),
                                       static_cast<unsigned long>(parsed.value));
                        else
                            formatLine(output, sizeof output,
                                       "PDO_READ admitted node=%u dir=%c set=%u; nine uploads, "
                                       "no write\n",
                                       unsigned(parsed.node), parsed.transmit ? 't' : 'r',
                                       unsigned(parsed.set));
                        line(firmware, output);
                    }
                }
            } else if (soakCollecting) {
                if (command != '\r' && command != '\n') {
                    if (soakLength < sizeof soakText)
                        soakText[soakLength] = char(command);
                    ++soakLength;
                } else {
                    soakCollecting = false;
                    const char* refusal = "active";
                    if (!soakActive) {
                        refusal = consoleHeld ? "console" : captureBlock ? "capture" : "state";
                        if (!consoleHeld && !captureBlock && soakLength <= sizeof soakText && controllerReady && !dumping &&
                            !networkDump && !nmtRequest.owner && !record && !rawRecord &&
                            !pdoRecord && !trialHeld && !commissionHeld &&
                            firmware.platform().nowUs(now))
                            refusal = startSoak(firmware, services, tap, driver, soak, configured,
                                                soakText, soakLength, now);
                    }
                    if (refusal) {
                        formatLine(output, sizeof output, "SOAK_REFUSED reason=%s; no work\n",
                                   refusal);
                        line(firmware, output);
                    }
                }
            } else if (command == '@') {
                // A console line, in every bench state: before any hold below could refuse its first
                // byte and then serve the rest of the line as bench commands.
                if (!commands)
                    commands = consoleStorage(firmware, services);
                if (commands)
                    commands->take(command, now, activity);
                else
                    consoleDiscard = true;
            } else if (command == 'X') {
                line(firmware, soak && soak->requestStop()
                                   ? "SOAK_STOPPING; a started read settles first\n"
                                   : "SOAK_STOP refused; none running\n");
            } else if (command == 'S') {
                soakCollecting = true;
                soakLength = 0;
                soakCollectUs = firmware.platform().nowUs(now) ? now : 0;
            } else if (command == 'N' || command == 'P' || command == 'R' || command == 'W' ||
                       command == 'E' || command == 'C' || command == 'D' || command == 'K' ||
                       command == 'H') {
                // A soak or a trial admits only the watch line; the collector refuses the rest.
                pdoCollecting = true;
                pdoText[0] = char(command);
                pdoLength = 1;
                pdoCollectUs = firmware.platform().nowUs(now) ? now : 0;
            } else if (command == 'x') {
                // Close the idle route sessions (backlog S15), so that K, H and the raw and PDO
                // experiments can reserve those nodes again in this boot. Sends nothing; not a stop.
                // Its own reply in every state: no T line, PDO line or soak line contains an x.
                uint8_t closed = 0;
                const char* refusal = consoleHeld ? "console" : "state";
                if (!consoleHeld && !captureBlock && !soakActive && !dumping && !networkDump &&
                    !nmtRequest.owner && !record && !rawRecord && !pdoRecord && !commissionHeld)
                    refusal = route ? route->closeSessions(closed) : "none";
                E::formatSessionsLine(route, refusal, closed, output, sizeof output);
                line(firmware, output);
            } else if (captureBlock && command != 'p' && command != 's' && command != 'n' &&
                       command != 'd' && command != 'r' && command != 'T' && command != '\r' &&
                       command != '\n') {
                formatLine(output, sizeof output, "CAP refused reason=%s; no work\n", captureBlock);
                line(firmware, output);
            } else if (soakActive && command != 'p' && command != 's' && command != '\r' &&
                       command != '\n') {
                line(firmware, "SOAK_REFUSED reason=active; no work\n");
                // A refused T opens a line: its rest is discarded, never served byte by byte.
                lineDiscard = command == 'T';
                lineDiscardUs = now;
            } else if (trialHeld && command != 'p' && command != 's' && command != 'd' &&
                       command != 'r' && command != 'A' && command != '\r' && command != '\n') {
                line(firmware, "TRIAL_REFUSED reason=active; no work\n");
                lineDiscard = command == 'T';
                lineDiscardUs = now;
            } else if (command == 'T') {
                trialCollecting = true;
                trialLength = 0;
                trialCollectUs = firmware.platform().nowUs(now) ? now : 0;
            } else if (command == 'A') {
                const char* refusal =
                    route && route->active() ? route->abort(firmware.owner().lastTimeUs()) : "none";
                if (refusal)
                    formatLine(output, sizeof output,
                               "TRIAL_ABORT refused reason=%s; nothing changed\n", refusal);
                else
                    formatLine(output, sizeof output,
                               "TRIAL_ABORTING; local only, no stop sent; a request on its way "
                               "settles first\n");
                line(firmware, output);
            } else if (command == 'o') {
                // The console premise (BenchConsole.h): both nodes' bench routes, configured exactly
                // as every bench command configures its node on first use, and the controller's
                // interface facts. Local configuration only: nothing is sent, selected or granted.
                const char* refusal = "state";
                if (controllerReady && firmware.platform().nowUs(now)) {
                    refusal = "route";
                    if (ensureRoute(services, 1, configured[0], now) &&
                        ensureRoute(services, 2, configured[1], now)) {
                        if (!commands)
                            commands = consoleStorage(firmware, services);
                        refusal = !commands                       ? "memory"
                                  : !commands->open(BENCH_BITRATE) ? "facts"
                                                                   : nullptr;
                    }
                }
                if (refusal)
                    formatLine(output, sizeof output, "CONSOLE refused reason=%s; no work\n", refusal);
                else
                    formatLine(output, sizeof output,
                               "CONSOLE open slot0=node1 slot1=node2 bitrate=%lu; routes configured, "
                               "nothing sent or selected\n",
                               static_cast<unsigned long>(BENCH_BITRATE));
                line(firmware, output);
            } else if (command == 'v' || command == 'b') {
                const unsigned node = command == 'v' ? 1 : 2;
                const bool accepted =
                    controllerReady && !consoleHeld && !pdoRecord && !dumping && !networkDump && !commissionHeld &&
                    !nmtRequest.owner && !readinessUsed[node - 1] && acquisition != UINT32_MAX &&
                    firmware.platform().nowUs(now) &&
                    identity(firmware, services, rawStorage(), true, node, configured[node - 1],
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
                const bool accepted = controllerReady && !consoleHeld && !pdoUsed && !dumping && !networkDump &&
                                      !nmtRequest.owner && !record && !rawRecord && !pdoRecord &&
                                      !commissionHeld &&
                                      acquisition < UINT32_MAX - 1 &&
                                      firmware.platform().nowUs(now) &&
                                      startMap(firmware, services, pdoStorage(), acquisition + 1, now);
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
                const bool accepted = controllerReady && !consoleHeld && !pdoRecord && !dumping && !networkDump &&
                                      !commissionHeld && !nmtRequest.owner &&
                                      !heartbeatUsed[node - 1] &&
                                      acquisition != UINT32_MAX && firmware.platform().nowUs(now) &&
                                      identity(firmware, services, rawStorage(), true, node,
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
                const bool accepted = acquisition != UINT32_MAX && controllerReady && !consoleHeld && !dumping &&
                                      !networkDump && !rawRecord && !record && !pdoRecord &&
                                      !commissionHeld && !nmtRequest.owner &&
                                      admitNmt(firmware, services, slot,
                                               C::NmtCommand::PRE_OPERATIONAL, acquisition + 1,
                                               nmtRequest, nmtAdmittedUs);
                if (accepted) {
                    ++acquisition;
                    nmtReported = false;
                }
                line(firmware, accepted ? "NMT Pre-operational admitted; no drive-stop claim\n"
                                        : "NMT refused; no work\n");
            } else if (command == 'n') {
                // A running scan's passive evidence is the network observer history this listing
                // drains: the listing waits until the scan, the bench's or the console's, has ended.
                if ((discovery && discovery->active()) || (commands && commands->scanning())) {
                    line(firmware, "NETWORK refused reason=discovery; no listing\n");
                } else if (!dumping && !networkDump) {
                    networkDump = true;
                    networkCursor = 0;
                }
            } else if (command >= '1' && command <= '4') {
                const bool rawPlan = command >= '3';
                const unsigned node = command - (rawPlan ? '2' : '0');
                const bool accepted = controllerReady && !consoleHeld && !pdoRecord && !dumping && !networkDump &&
                                      !commissionHeld && !nmtRequest.owner && !used[node - 1] &&
                                      acquisition != UINT32_MAX && firmware.platform().nowUs(now) &&
                                      identity(firmware, services, rawPlan ? rawStorage() : raw, rawPlan, node,
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
                     (pdoRecord && pdoRecord->terminal) || (trialRecord && trialRecord->terminal) ||
                     (captureRecord && captureRecord->terminal) || commissionTerminal) &&
                    !dumping) {
                    dumping = true;
                    dumpCursor = 0;
                } else {
                    line(firmware, "DUMP unavailable\n");
                }
            } else if (command == 'r') {
                if (captureRecord) {
                    line(firmware, !dumping && capture->release()
                                       ? "CAP record released; level, gate and SDO state unchanged\n"
                                       : "CAP release refused\n");
                    continue;
                }
                if (trialRecord) {
                    line(firmware, !dumping && route->release()
                                       ? "TRIAL record released; sessions and uncertainty retained\n"
                                       : "TRIAL release refused\n");
                    continue;
                }
                if (pdoRecord) {
                    line(firmware, !dumping && pdo->release()
                                       ? "PDO record released; configuration/uncertainty retained\n"
                                       : "PDO release refused\n");
                    continue;
                }
                if (discoveryRecord) {
                    line(firmware, !dumping && discovery->release()
                                       ? "DISC record released; routes, epochs and quarantine "
                                         "unchanged\n"
                                       : "DISC release refused\n");
                    continue;
                }
                if (commRecord) {
                    line(firmware, !dumping && comm->release()
                                       ? "COMM record released; channel uncertainty unchanged\n"
                                       : "COMM release refused\n");
                    continue;
                }
                if (heartbeatRecord) {
                    line(firmware, !dumping && heartbeat->release()
                                       ? "HB record released; configuration and uncertainty "
                                         "retained, producer interval stays withdrawn\n"
                                       : "HB release refused\n");
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
                     !dumping && (rawRecord ? raw->release() : firmware.evidence().release())
                         ? "RELEASED record; channel uncertainty unchanged\n"
                         : "RELEASE refused\n");
            } else if (command == 'p') {
                const esp_app_desc_t* app = esp_app_get_description();
                char hash[65] = {};
                for (unsigned i = 0; i < 32; ++i)
                    formatLine(hash + i * 2, 3, "%02x", unsigned(app->app_elf_sha256[i]));
                formatLine(output, sizeof(output),
                           "PONG target=esp32s2 base=%s elf=%s sdk=%s running=%u active=%u "
                           "silent=%u pinsReady=%u soak=%u t=%llu routes=8 trial=%u motion=%u pdo=4 "
                           "watch=%u capture=%u\n",
                           MOTORCONTROLCAN_SOURCE_REVISION, hash, app->idf_ver,
                           unsigned(firmware.running()), unsigned(controllerReady),
                           unsigned(silentLevel != E::TransceiverState::NORMAL_REQUESTED),
                           unsigned(silentReady), unsigned(soakActive),
                           static_cast<unsigned long long>(firmware.owner().lastTimeUs()),
                           unsigned(trialHeld), unsigned(route ? route->motionAdmissions() : 0),
                           unsigned(watch && watch->active()), unsigned(captureBlock != nullptr));
                line(firmware, output);
            } else if (command == 'M') {
                // Heap only: no CAN traffic, no allocation, no record.
                heapLine(output, sizeof output, arena);
                line(firmware, output);
            } else if (command == 'c') {
                bool ready = false;
                if (!attempted && silentReady) {
                    attempted = true;
                    E::FirmwareConfig config;
                    config.configured = true;
                    config.controller.txPin = 17;
                    config.controller.rxPin = 16;
                    config.controller.bitrate = BENCH_BITRATE;
                    config.controller.sdkTxQueueDepth = 1;
                    config.controller.rxQueueDepth = 16;
                    ready = firmware.begin(config);
                    if (ready && !driveSilent(nullptr, SILENT_GPIO, false)) {
                        firmware.transport().shutdown();
                        ready = false;
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
                           "flags=%08lx stackFree=%lu t=%llu txRej=%lu rxLost=%lu unmatched=%lu "
                           "traceOver=%lu ownerGap=%llu conRej=%lu\n",
                           unsigned(firmware.running()), unsigned(diagnostics.inhibited),
                           static_cast<unsigned long>(stats.rxFrames),
                           static_cast<unsigned long>(stats.txFrames),
                           static_cast<unsigned long>(diagnostics.rxDropped),
                           static_cast<unsigned long>(diagnostics.busErrors),
                           static_cast<unsigned long>(diagnostics.errorFlags),
                           static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)),
                           static_cast<unsigned long long>(firmware.owner().lastTimeUs()),
                           static_cast<unsigned long>(stats.txRejected),
                           static_cast<unsigned long>(stats.rxLost),
                           static_cast<unsigned long>(stats.unmatchedFrames),
                           static_cast<unsigned long>(stats.traceOverwritten),
                           static_cast<unsigned long long>(stats.maxServiceGapUs),
                           static_cast<unsigned long>(firmware.platform().stats().outputRejected));
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
