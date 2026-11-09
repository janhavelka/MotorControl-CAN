// SPDX-License-Identifier: MIT
#include "BenchTerminal.h"
#include <cstdio>
#include <cstring>

namespace MotorControlCANExample {
void copyTerminal(BenchTerminal& out, const MotorControlCANOwner::Completion& done) noexcept {
    const MotorControlCAN::CANOPEN::SdoCompletion& t = done.service;
    BenchTerminal value;
    value.operation = t.transfer.operation;
    value.request = t.transfer.request;
    value.ownerRequest = done.id.generation;
    value.step = t.transfer.step;
    value.index = t.transfer.index;
    value.subindex = t.transfer.subindex;
    value.direction = uint8_t(t.transfer.direction);
    value.outcome = uint8_t(t.outcome);
    value.size = uint8_t(t.size);
    std::memcpy(value.data, t.data, sizeof value.data);
    value.reason = uint8_t(done.reason);
    // The same stage bits as main.cpp resultLine: adapter admitted, controller accepted, queued,
    // possible TX, confirmed TX, acknowledged, uncertain, cleanup queued.
    value.stages = uint8_t(unsigned(done.adapterAdmitted) | unsigned(done.controllerAccepted) << 1 |
                           unsigned(t.queued) << 2 | unsigned(t.transmissionPossible) << 3 |
                           unsigned(t.transmissionConfirmed) << 4 |
                           unsigned(t.protocolAcknowledged) << 5 | unsigned(t.effectUncertain) << 6 |
                           unsigned(t.clientAbortQueued) << 7);
    value.acknowledged = t.protocolAcknowledged;
    value.possible = t.effectPossible;
    value.uncertain = t.effectUncertain;
    value.cleanup = t.clientAbortQueued;
    value.abortCode = t.abortCode;
    value.nodeGeneration = t.nodeGeneration;
    value.admittedUs = t.admittedUs;
    value.completedUs = t.completedUs;
    out = value;
}
int formatTerminal(char* out, size_t size, const BenchTerminal& t) noexcept {
    return std::snprintf(
        out, size,
        "RESULT op=%lu req=%lu oreq=%lu step=%lu object=%04x:%02x dir=%u outcome=%u ack=%u "
        "possible=%u uncertain=%u abort=%08lx cleanup=%u size=%u data=%02x%02x%02x%02x "
        "admitted=%llu completed=%llu ne=%lu reason=%u st=%02x\n",
        static_cast<unsigned long>(t.operation), static_cast<unsigned long>(t.request),
        static_cast<unsigned long>(t.ownerRequest), static_cast<unsigned long>(t.step),
        unsigned(t.index), unsigned(t.subindex), unsigned(t.direction), unsigned(t.outcome),
        unsigned(t.acknowledged), unsigned(t.possible), unsigned(t.uncertain),
        static_cast<unsigned long>(t.abortCode), unsigned(t.cleanup), unsigned(t.size),
        unsigned(t.data[0]), unsigned(t.data[1]), unsigned(t.data[2]), unsigned(t.data[3]),
        static_cast<unsigned long long>(t.admittedUs),
        static_cast<unsigned long long>(t.completedUs),
        static_cast<unsigned long>(t.nodeGeneration), unsigned(t.reason), unsigned(t.stages));
}
int formatTraffic(char* out, size_t size, const EvidenceTraffic& t, const char* label) noexcept {
    using MotorControlCANOwner::DiagnosticKind;
    using MotorControlCANOwner::TransportTxId;
    // Only the fields this kind defines: a default state is not an observed one.
    char suffix[48] = {};
    if (t.kind == DiagnosticKind::CONTROLLER)
        std::snprintf(suffix, sizeof suffix, " ctrl=%u", unsigned(t.controller));
    else if (t.kind == DiagnosticKind::RECOVERY)
        std::snprintf(suffix, sizeof suffix, " rec=%u", unsigned(t.recovery));
    else if (t.kind == DiagnosticKind::TX || t.kind == DiagnosticKind::TX_REPORT)
        std::snprintf(suffix, sizeof suffix, " txk=%u txr=%lu txf=%lu", unsigned(t.txId.kind),
                      static_cast<unsigned long>(t.txId.kind == TransportTxId::Kind::NMT
                                                     ? t.txId.nmt.generation
                                                     : t.txId.sdo.request.generation),
                      static_cast<unsigned long>(t.txId.sdo.frame));
    return std::snprintf(
        out, size,
        "%s k=%u p=%llu id=%lx ext=%u rtr=%u dlc=%u data=%02x%02x%02x%02x%02x%02x%02x%02x "
        "ok=%u exact=%u t=%llu bounds=%u lo=%llu hi=%llu epoch=%lu%s\n",
        label, unsigned(t.kind), static_cast<unsigned long long>(t.processedUs),
        static_cast<unsigned long>(t.frame.id), unsigned(t.frame.extended), unsigned(t.frame.rtr),
        unsigned(t.frame.dlc), unsigned(t.frame.data[0]), unsigned(t.frame.data[1]),
        unsigned(t.frame.data[2]), unsigned(t.frame.data[3]), unsigned(t.frame.data[4]),
        unsigned(t.frame.data[5]), unsigned(t.frame.data[6]), unsigned(t.frame.data[7]),
        unsigned(t.accepted), unsigned(t.receivedTimeKnown),
        static_cast<unsigned long long>(t.receivedUs), unsigned(t.receiveBoundsKnown),
        static_cast<unsigned long long>(t.receivedEarliestUs),
        static_cast<unsigned long long>(t.receivedLatestUs),
        static_cast<unsigned long>(t.controllerEpoch), suffix);
}
void copyTraffic(EvidenceTraffic& out, const MotorControlCANOwner::Diagnostic& in) noexcept {
    out.kind = in.kind;
    out.frame = in.frame;
    out.txId = in.txId;
    out.controllerEpoch = in.controllerEpoch;
    out.processedUs = in.processedUs;
    out.receivedUs = in.receivedUs;
    out.receivedTimeKnown = in.receivedTimeKnown;
    out.receiveBoundsKnown = in.receiveBoundsKnown;
    out.receivedEarliestUs = in.receivedEarliestUs;
    out.receivedLatestUs = in.receivedLatestUs;
    out.accepted = in.accepted;
    out.controller = in.controller;
    out.recovery = in.recovery;
}
bool fitLine(int count, char* out, size_t size) noexcept {
    if (count >= 0 && size_t(count) < size)
        return true;
    static const char failure[] = "OUTPUT_FORMAT_ERROR; evidence incomplete\n";
    if (size >= sizeof failure)
        std::memcpy(out, failure, sizeof failure);
    else if (size)
        out[0] = 0;
    return true; // The cursor still advances: the record line exists and was reported as damaged.
}
} // namespace MotorControlCANExample
