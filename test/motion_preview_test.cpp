// SPDX-License-Identifier: MIT
// Offline candidate review through public codecs and the actual selected SDO encoder.
#include "MotionPreview.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <typename T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& v) { std::memcpy(bytes, &v, sizeof v); }
    void unchanged(const T& v) const { CHECK(std::memcmp(bytes, &v, sizeof v) == 0); }
};
void readiness(E::RawEvidenceRecord& r) {
    r = E::RawEvidenceRecord();
    r.plan.inventory = E::RawEvidenceInventory::MOTION_READINESS;
    auto& t = r.plan.review.plan.target;
    auto& b = t.channel.binding;
    b.logicalAxisId = 41;
    b.endpoint.profile = 1;
    b.endpoint.address = 2;
    b.bindingGeneration = 5;
    b.configurationGeneration = 7;
    t.channel.clientToServerId = 0x612;
    t.channel.serverToClientId = 0x592;
    t.nodeGeneration = 9;
    r.plan.review.plan.deadlineUs = 1000;
    r.plan.review.plan.id = 4000;
    r.startedUs = 10;
    r.completedUs = 100;
    r.terminal = true;
    r.intendedCount = r.attemptedCount = r.terminalCount = 30;
    const uint16_t indexes[30] = {0x1001, 0x603f, 0x6041, 0x6061, 0x2100, 0x2101, 0x6064, 0x606c,
                                  0x607a, 0x6081, 0x6083, 0x6084, 0x605d, 0x60ff, 0x2200, 0x2201,
                                  0x2202, 0x607d, 0x607d, 0x2500, 0x2501, 0x2400, 0x2400, 0x2400,
                                  0x2400, 0x2400, 0x2400, 0x2400, 0x2400, 0x60fd};
    const uint8_t widths[30] = {1, 2, 2, 1, 2, 2, 4, 4, 4, 4, 4, 4, 2, 4, 2,
                                2, 2, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4};
    for (unsigned i = 0; i < 30; ++i) {
        auto& e = r.terminals[i].service;
        e.transfer.channel = t.channel;
        e.transfer.operation = 4000;
        e.transfer.request = e.transfer.step = i + 1;
        e.transfer.deadlineUs = 1000;
        e.transfer.queueTimeoutUs = e.transfer.responseTimeoutUs = 250000;
        e.transfer.index = indexes[i];
        e.transfer.subindex = i == 17 ? 1 : i == 18 ? 2 : i >= 21 && i <= 28 ? uint8_t(i - 20) : 0;
        e.transfer.size = 4;
        e.transfer.uploadSize = C::SdoUploadSize::SERVER_INDICATED;
        e.outcome = C::SdoOutcome::SUCCESS;
        e.size = widths[i];
        e.nodeGeneration = t.nodeGeneration;
        e.admittedUs = 20 + i;
        e.completedUs = 21 + i;
        e.queued = e.transmissionPossible = true;
        e.protocolAcknowledged = e.effectPossible = true;
    }
    r.terminals[15].service.data[0] = 0xe8;
    r.terminals[15].service.data[1] = 3;
    r.terminals[8].service.data[0] = 0x88;
    r.terminals[8].service.data[1] = 0x13;
    r.terminals[13].service.data[0] = 120;
    r.terminals[2].service.data[0] = 0x37;
    r.terminals[2].service.data[1] = 0x80;
    r.terminals[4].service.data[0] = 1;
}
void encodeWithSelectedEngine(C::SdoClient& client, const E::MotionPreviewStep& step,
                              unsigned number) {
    C::SdoTransfer work;
    work.channel = step.expectation.channel;
    work.operation = 5000;
    work.request = work.step = number;
    work.index = step.expectation.index;
    work.subindex = step.expectation.subindex;
    work.direction = step.expectation.direction;
    work.size = step.expectation.width;
    work.queueTimeoutUs = work.responseTimeoutUs = 100;
    work.deadlineUs = 1000;
    if (work.direction == C::SdoDirection::DOWNLOAD)
        for (unsigned i = 0; i < work.size; ++i)
            work.data[i] = uint8_t(step.nativeBits >> (8 * i));
    const uint64_t now = number * 10;
    C::SdoRequestId id;
    CHECK(client.admit(0, work, now, id));
    CHECK(client.service(now));
    M::Frame tx;
    C::SdoTxId txId;
    CHECK(client.nextTx(0, now, tx, txId));
    CHECK(tx.id == step.frame.id && tx.dlc == 8 && tx.payloadLength == 8 && !tx.extended &&
          !tx.rtr);
    CHECK(std::memcmp(tx.data, step.frame.data, 8) == 0);
    CHECK(C::checkRequest(tx, step.expectation));
    CHECK(client.confirmTx(txId, now));
    M::Frame response;
    response.id = work.channel.serverToClientId;
    response.dlc = response.payloadLength = 8;
    response.data[0] = work.direction == C::SdoDirection::DOWNLOAD ? 0x60 : 0x4f;
    response.data[1] = uint8_t(work.index);
    response.data[2] = uint8_t(work.index >> 8);
    response.data[3] = work.subindex;
    if (work.direction == C::SdoDirection::UPLOAD)
        response.data[4] = 1;
    CHECK(client.receive(response, now + 1, now + 1));
    CHECK(client.result(id) && client.result(id)->outcome == C::SdoOutcome::SUCCESS);
    CHECK(client.release(id));
}
void candidateAndStopBranch() {
    E::RawEvidenceRecord source;
    readiness(source);
    Saved<E::RawEvidenceRecord> retained(source);
    E::MotionPreview p;
    CHECK(E::prepareMotionPreview(p, source));
    retained.unchanged(source);
    CHECK(p.count == 10 && p.absoluteTargetNative == 16 && p.subdivisionReadback == 1000);
    CHECK(p.observationBudgetUs == 5000000 && !p.postTriggerStopReviewed);
    CHECK(p.retainedPositionTargetBits == 5000 && p.retainedVelocityTargetBits == 120);
    CHECK(p.retainedTargetsNonzero && !p.executionReady && p.displayedMode == 0);
    CHECK(p.statusword == 0x8037 && p.motionMonitor == 1);
    CHECK(p.target.channel.clientToServerId == 0x612 &&
          p.target.channel.binding.configurationGeneration == 7);
    CHECK(p.steps[0].expectation.index == 0x6060 && p.steps[0].nativeBits == 1);
    CHECK(p.steps[1].expectation.index == 0x6061 &&
          p.steps[1].expectation.direction == C::SdoDirection::UPLOAD);
    CHECK(p.steps[2].nativeBits == 6 && p.steps[3].nativeBits == 7 && p.steps[4].nativeBits == 15);
    CHECK(p.steps[3].delayAfterPreviousAckUs == 10000 &&
          p.steps[4].delayAfterPreviousAckUs == 10000);
    CHECK(p.steps[5].expectation.index == 0x607a && p.steps[5].nativeBits == 16);
    CHECK(p.steps[6].expectation.index == 0x6081 && p.steps[6].nativeBits == 1);
    CHECK(p.steps[7].expectation.index == 0x6083 && p.steps[7].nativeBits == 1000);
    CHECK(p.steps[8].expectation.index == 0x6084 && p.steps[8].nativeBits == 1000);
    CHECK(p.steps[9].expectation.index == 0x6040 && p.steps[9].nativeBits == 0x1f);
    unsigned triggerEdges = 0;
    uint32_t lastControl = 0;
    for (unsigned i = 0; i < p.count; ++i) {
        CHECK(C::checkRequest(p.steps[i].frame, p.steps[i].expectation));
        CHECK(p.steps[i].expectation.index != 0x2300 && p.steps[i].expectation.index != 0x2301 &&
              p.steps[i].expectation.index != 0x2302);
        if (p.steps[i].expectation.index == 0x6040) {
            const uint32_t v = p.steps[i].nativeBits;
            if (!(lastControl & 0x10u) && (v & 0x10u))
                ++triggerEdges;
            lastControl = v;
        }
    }
    CHECK(triggerEdges == 1 && !(lastControl & 0x40u) && !(lastControl & 0x20u));
    CHECK(p.stopBranchAfterStep == 4 && p.steps[p.stopBranchAfterStep].nativeBits == 15);
    CHECK(p.stationaryQuickStop.expectation.index == 0x6040 &&
          p.stationaryQuickStop.nativeBits == 11);
    CHECK((p.steps[4].nativeBits ^ p.stationaryQuickStop.nativeBits) == 4); // Only bit2 clears.
    CHECK(p.stopBranchAfterStep <
          5); // Alternative after separately authorized enable, before parameters/trigger.
    C::SdoClient client;
    CHECK(client.configure(0, p.target.channel, true, true));
    for (unsigned i = 0; i < p.count; ++i)
        encodeWithSelectedEngine(client, p.steps[i], i + 1);
    C::SdoClient branchEncoder;
    CHECK(branchEncoder.configure(0, p.target.channel, true, true));
    encodeWithSelectedEngine(branchEncoder, p.stationaryQuickStop,
                             1); // Independent byte proof, never a post-trigger transition test.
}
void rejectedEvidenceIsAtomic() {
    for (unsigned kind = 0; kind < 11; ++kind) {
        E::RawEvidenceRecord r;
        readiness(r);
        switch (kind) {
        case 0:
            r.terminal = false;
            break;
        case 1:
            r.failure = E::RawEvidenceFailure::SERVICE;
            break;
        case 2:
            r.plan.inventory = E::RawEvidenceInventory::IDENTITY_NETWORK;
            break;
        case 3:
            r.terminalCount = 29;
            break;
        case 4:
            r.terminals[15].service.size = 1;
            break;
        case 5:
            r.terminals[15].service.data[0]--;
            break;
        case 6:
            r.terminals[2].service.nodeGeneration++;
            break;
        case 7:
            r.terminals[2].service.transfer.direction = C::SdoDirection::DOWNLOAD;
            break;
        case 8:
            r.terminals[2].service.transfer.channel.binding.configurationGeneration++;
            break;
        case 9:
            r.completedUs = r.plan.review.plan.deadlineUs;
            break;
        default:
            r.evidenceLost = true;
            break;
        }
        E::MotionPreview out;
        Saved<E::MotionPreview> saved(out);
        CHECK(!E::prepareMotionPreview(out, r));
        saved.unchanged(out);
    }
    for (unsigned field = 0; field < 30; ++field) {
        for (unsigned fault = 0; fault < 6; ++fault) {
            E::RawEvidenceRecord r;
            readiness(r);
            auto& e = r.terminals[field].service;
            if (fault == 0)
                ++e.transfer.index;
            if (fault == 1)
                ++e.transfer.subindex;
            if (fault == 2)
                ++e.transfer.step;
            if (fault == 3)
                ++e.transfer.operation;
            if (fault == 4)
                e.transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
            if (fault == 5)
                e.size = e.size == 1 ? 2 : 1;
            E::MotionPreview out;
            Saved<E::MotionPreview> saved(out);
            CHECK(!E::prepareMotionPreview(out, r));
            saved.unchanged(out);
        }
    }
}
} // namespace
int main() {
    candidateAndStopBranch();
    rejectedEvidenceIsAtomic();
    std::printf("motion preview: %u checks passed\n", checks);
}
