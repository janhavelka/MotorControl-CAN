// SPDX-License-Identifier: MIT
#include "MotionPreview.h"

namespace MotorControlCANExample {
namespace C = MotorControlCAN::CANOPEN;
namespace M = MotorControlCAN;
namespace {
M::Status invalid(const char* message) {
    return M::Status(M::Err::INVALID_CONFIG, 0, message);
}
bool same(const C::SdoChannel& a, const C::SdoChannel& b) {
    return M::sameBinding(a.binding, b.binding) && a.clientToServerId == b.clientToServerId &&
           a.serverToClientId == b.serverToClientId;
}
M::Status step(MotionPreviewStep& out, const C::SdoChannel& channel, const uint8_t (&bytes)[8],
               uint8_t width, bool upload, uint32_t value, uint32_t delay = 0) {
    out.expectation.channel = channel;
    out.expectation.index = uint16_t(bytes[1]) | uint16_t(bytes[2]) << 8;
    out.expectation.subindex = bytes[3];
    out.expectation.width = width;
    out.expectation.direction = upload ? C::SdoDirection::UPLOAD : C::SdoDirection::DOWNLOAD;
    out.frame.id = channel.clientToServerId;
    out.frame.dlc = out.frame.payloadLength = 8;
    for (unsigned i = 0; i < 8; ++i)
        out.frame.data[i] = bytes[i];
    out.nativeBits = value;
    out.delayAfterPreviousAckUs = delay;
    return C::checkRequest(out.frame, out.expectation);
}
} // namespace
M::Status prepareMotionPreview(MotionPreview& output, const RawEvidenceRecord& r) noexcept {
    const uintptr_t x = reinterpret_cast<uintptr_t>(&output), y = reinterpret_cast<uintptr_t>(&r);
    if (x >= y ? x - y < sizeof r : y - x < sizeof output)
        return invalid("motion preview overlaps source record");
    const auto& target = r.plan.review.plan.target;
    if (!r.terminal || r.failure != RawEvidenceFailure::NONE || r.evidenceLost || r.tailOverflow ||
        r.plan.inventory != RawEvidenceInventory::MOTION_READINESS || r.plan.includeState ||
        r.plan.includeNetwork || r.plan.enableHeartbeat || !r.plan.review.plan.id ||
        r.terminalCount != 30 || r.intendedCount != 30 || r.attemptedCount != 30 ||
        r.completedUs < r.startedUs || r.completedUs >= r.plan.review.plan.deadlineUs ||
        !target.nodeGeneration || !C::validateSdoChannel(target.channel))
        return invalid("motion preview requires complete retained readiness evidence");
    uint64_t earliest = r.startedUs;
    for (unsigned i = 0; i < 30; ++i) {
        const auto& e = r.terminals[i].service;
        RawEvidenceField field;
        if (!motionReadinessField(i, field))
            return invalid("motion preview inventory row unavailable");
        if (!same(e.transfer.channel, target.channel) ||
            e.nodeGeneration != target.nodeGeneration ||
            e.transfer.operation != r.plan.review.plan.id || e.transfer.request != i + 1 ||
            e.transfer.step != i + 1 || e.transfer.index != field.index ||
            e.transfer.subindex != field.subindex ||
            e.transfer.deadlineUs != r.plan.review.plan.deadlineUs || !e.transfer.queueTimeoutUs ||
            !e.transfer.responseTimeoutUs || e.transfer.direction != C::SdoDirection::UPLOAD ||
            e.transfer.size != 4 || e.transfer.uploadSize != C::SdoUploadSize::SERVER_INDICATED ||
            e.transfer.readEffect != C::SdoReadEffect::UNQUALIFIED ||
            e.outcome != C::SdoOutcome::SUCCESS || !e.protocolAcknowledged || !e.queued ||
            !e.transmissionPossible || !e.effectPossible || e.effectUncertain || e.abortValid ||
            e.clientAbortQueued || !e.size || e.size > 4 || e.admittedUs < earliest ||
            e.completedUs < e.admittedUs || e.completedUs > r.completedUs)
            return invalid("motion preview readiness contains inconsistent service evidence");
        if (e.size != field.documentedWidth)
            return invalid("motion preview rejects documentary/actual readiness width conflict");
        earliest = e.completedUs;
    }
    const auto& subdivision = r.terminals[15].service;
    uint32_t raw = 0;
    if (subdivision.transfer.index != 0x2201 || subdivision.transfer.subindex ||
        subdivision.size != 2 || !C::readWord(subdivision.data, subdivision.size, 2, raw) ||
        raw != 1000)
        return invalid("motion preview is scoped to observed U16 subdivision 1000");
    // These fixed vectors are review artifacts, not a second encoder or live executor.
    // EN86/ZH86 pp11-12: observe 6061 (never write it), then 6/7/15,
    // absolute target 16, profile 1 rpm,1000ms ramps, one bit4 edge with bit6 clear.
    const uint8_t bytes[10][8] = {
        {0x2f, 0x60, 0x60, 0, 1, 0, 0, 0},    {0x40, 0x61, 0x60, 0, 0, 0, 0, 0},
        {0x2b, 0x40, 0x60, 0, 6, 0, 0, 0},    {0x2b, 0x40, 0x60, 0, 7, 0, 0, 0},
        {0x2b, 0x40, 0x60, 0, 15, 0, 0, 0},   {0x23, 0x7a, 0x60, 0, 16, 0, 0, 0},
        {0x23, 0x81, 0x60, 0, 1, 0, 0, 0},    {0x23, 0x83, 0x60, 0, 0xe8, 3, 0, 0},
        {0x23, 0x84, 0x60, 0, 0xe8, 3, 0, 0}, {0x2b, 0x40, 0x60, 0, 31, 0, 0, 0}};
    const uint8_t widths[10] = {1, 1, 2, 2, 2, 4, 4, 4, 4, 2};
    const uint32_t values[10] = {1, 1, 6, 7, 15, 16, 1, 1000, 1000, 31};
    const uint8_t quickStop[8] = {0x2b, 0x40, 0x60, 0, 11, 0, 0, 0};
    MotionPreview candidate;
    candidate.target = target;
    candidate.subdivisionReadback = 1000;
    C::readWord(r.terminals[8].service.data, r.terminals[8].service.size, 4,
                candidate.retainedPositionTargetBits);
    C::readWord(r.terminals[13].service.data, r.terminals[13].service.size, 4,
                candidate.retainedVelocityTargetBits);
    uint32_t status = 0, monitor = 0;
    C::readWord(r.terminals[2].service.data, r.terminals[2].service.size, 2, status);
    C::readWord(r.terminals[4].service.data, r.terminals[4].service.size, 2, monitor);
    candidate.statusword = uint16_t(status);
    candidate.motionMonitor = uint16_t(monitor);
    candidate.displayedMode = r.terminals[3].service.data[0];
    candidate.retainedTargetsNonzero =
        candidate.retainedPositionTargetBits || candidate.retainedVelocityTargetBits;
    candidate.absoluteTargetNative = 16;
    candidate.observationBudgetUs = 5000000;
    candidate.count = 10;
    candidate.stopBranchAfterStep = 4;
    for (unsigned i = 0; i < 10; ++i) {
        const M::Status checked = step(candidate.steps[i], target.channel, bytes[i], widths[i],
                                       i == 1, values[i], i == 3 || i == 4 ? 10000 : 0);
        if (!checked)
            return checked;
    }
    const M::Status checked =
        step(candidate.stationaryQuickStop, target.channel, quickStop, 2, false, 11);
    if (!checked)
        return checked;
    output = candidate;
    return M::Ok();
}
} // namespace MotorControlCANExample
