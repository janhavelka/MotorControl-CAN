// SPDX-License-Identifier: MIT
// Native use of the installed core only: the TPDO1 event-timer change with scripted replies.
#include <MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
// One SDO exchange with node 2 of the bench, as retained: the request, then the reply.
struct Exchange {
    uint8_t request[8];
    uint8_t reply[8];
};
// TPDO1 record before the change: docs/bench/20261106T1311Z-pdo-node2-event-timer/rx.txt lines
// 246-272, in step order 1800:01/02/03/05, 1A00:00..04. Event timer raw 0.
const Exchange kBaseline[9] = {
    {{0x40, 0x00, 0x18, 0x01, 0, 0, 0, 0}, {0x43, 0x00, 0x18, 0x01, 0x82, 0x01, 0x00, 0x00}},
    {{0x40, 0x00, 0x18, 0x02, 0, 0, 0, 0}, {0x4f, 0x00, 0x18, 0x02, 0xff, 0x00, 0x00, 0x00}},
    {{0x40, 0x00, 0x18, 0x03, 0, 0, 0, 0}, {0x4b, 0x00, 0x18, 0x03, 0x64, 0x00, 0x00, 0x00}},
    {{0x40, 0x00, 0x18, 0x05, 0, 0, 0, 0}, {0x4b, 0x00, 0x18, 0x05, 0x00, 0x00, 0x00, 0x00}},
    {{0x40, 0x00, 0x1a, 0x00, 0, 0, 0, 0}, {0x4f, 0x00, 0x1a, 0x00, 0x03, 0x00, 0x00, 0x00}},
    {{0x40, 0x00, 0x1a, 0x01, 0, 0, 0, 0}, {0x43, 0x00, 0x1a, 0x01, 0x10, 0x00, 0x41, 0x60}},
    {{0x40, 0x00, 0x1a, 0x02, 0, 0, 0, 0}, {0x43, 0x00, 0x1a, 0x02, 0x08, 0x00, 0x61, 0x60}},
    {{0x40, 0x00, 0x1a, 0x03, 0, 0, 0, 0}, {0x43, 0x00, 0x1a, 0x03, 0x20, 0x00, 0x64, 0x60}},
    {{0x40, 0x00, 0x1a, 0x04, 0, 0, 0, 0}, {0x43, 0x00, 0x1a, 0x04, 0x00, 0x00, 0x00, 0x00}}};
// The write and its acknowledgement: docs/bench/20261106T1316Z-pdo-both-streams-soak/rx.txt lines 26
// and 28. The final reads reuse the 1311Z replies, except 1800:05, which answers raw 100 as in
// that file's line 31.
const Exchange kWrite = {{0x2b, 0x00, 0x18, 0x05, 0x64, 0x00, 0x00, 0x00},
                         {0x60, 0x00, 0x18, 0x05, 0x00, 0x00, 0x00, 0x00}};
const uint8_t kReadback100[8] = {0x4b, 0x00, 0x18, 0x05, 0x64, 0x00, 0x00, 0x00};
const uint16_t kIndex[9] = {0x1800, 0x1800, 0x1800, 0x1800, 0x1a00, 0x1a00, 0x1a00, 0x1a00, 0x1a00};
const uint8_t kSub[9] = {1, 2, 3, 5, 0, 1, 2, 3, 4};
const uint8_t kWidth[9] = {4, 1, 2, 2, 1, 4, 4, 4, 4};
L::ReadTarget target() {
    L::ReadTarget t;
    t.channel.binding.logicalAxisId = 2;
    t.channel.binding.endpoint.profile = 1;
    t.channel.binding.endpoint.address = 2;
    t.channel.binding.bindingGeneration = 1;
    t.channel.binding.configurationGeneration = 1;
    t.channel.clientToServerId = 0x602;
    t.channel.serverToClientId = 0x582;
    t.nodeGeneration = 1;
    return t;
}
bool checked(M::Status status, const char* operation) {
    if (status)
        return true;
    std::fprintf(stderr, "%s: %s (%" PRId32 "): %s\n", operation, M::errToString(status.code),
                 status.detail, status.msg);
    return false;
}
void frame(const char* label, uint32_t id, const uint8_t (&data)[8]) {
    std::printf("  %s %03" PRIx32 ":", label, id);
    for (uint8_t byte : data)
        std::printf(" %02x", unsigned(byte));
}
// Check the scripted frames against the yielded work with the installed codec, then build the
// terminal a sole service owner would retain for that reply. Times are supplied fixtures.
bool exchange(const C::SdoTransfer& t, const uint8_t (&request)[8], const uint8_t (&reply)[8],
              uint64_t at, C::SdoCompletion& out) {
    C::SdoExpectation e;
    e.channel = t.channel;
    e.index = t.index;
    e.subindex = t.subindex;
    e.width = t.size;
    e.direction = t.direction;
    M::Frame tx, rx;
    tx.id = t.channel.clientToServerId;
    rx.id = t.channel.serverToClientId;
    tx.dlc = tx.payloadLength = rx.dlc = rx.payloadLength = 8;
    std::memcpy(tx.data, request, 8);
    std::memcpy(rx.data, reply, 8);
    C::SdoReply decoded;
    const bool upload = t.direction == C::SdoDirection::UPLOAD;
    if (!checked(C::checkRequest(tx, e), "checkRequest") ||
        (!upload && std::memcmp(tx.data + 4, t.data, t.size) != 0) ||
        !checked(C::checkReply(rx, e, decoded), "checkReply") ||
        decoded.kind != (upload ? C::SdoReplyKind::EXPEDITED_UPLOAD : C::SdoReplyKind::DOWNLOAD_ACK))
        return false;
    frame("TX", tx.id, request);
    frame("RX", rx.id, reply);
    std::printf("  step %" PRIu32 "\n", t.step);
    out = C::SdoCompletion();
    out.transfer = t;
    out.outcome = C::SdoOutcome::SUCCESS;
    out.nodeGeneration = target().nodeGeneration;
    out.admittedUs = at;
    out.completedUs = at + 3;
    out.queued = out.transmissionPossible = out.transmissionConfirmed = true;
    out.protocolAcknowledged = out.effectPossible = true; // Upload effects stay unqualified.
    out.size = upload ? decoded.length : 0;
    std::memcpy(out.data, decoded.data, out.size);
    return true;
}
} // namespace
int main() {
    std::puts("Offline TPDO1 event-timer change, node 2: 1800:05 raw 0 -> raw 100.");
    std::puts("Replies are retained bench bytes (1311Z, 1316Z); no controller, stack or clock.");
    const L::ReadTarget route = target();
    L::AxisReservation reservation; // The owner's actual exclusion, here a fixture.
    reservation.target = route;
    reservation.model = L::SettingsModel::CL86_C;
    reservation.owner = reservation.sequence = reservation.generation = 1;
    L::Tpdo1ConfigurationReview review;
    review.target = route;
    review.scope = L::PdoConfigurationScope::FIXTURE_QUALIFIED;
    review.fixtureRecord = 20261102;
    review.firmwareRecord = review.procedureRecord = 7;
    review.authorizationRecord = 20261106;
    C::ObservationTime preOperational; // A qualified 7F heartbeat received after the reservation.
    preOperational.present = preOperational.receiveBoundsKnown = true;
    preOperational.receivedEarliestUs = 5;
    preOperational.receivedLatestUs = 9;
    preOperational.processedUs = 10;
    preOperational.generation = route.nodeGeneration;
    L::ReadOptions options;
    options.queueTimeoutUs = options.responseTimeoutUs = 250000;
    const uint64_t deadline = 8000000, age = 3000000;
    std::puts("Baseline, nine exact uploads:");
    C::SdoCompletion baseline[9];
    uint64_t now = 20;
    for (unsigned i = 0; i < 9; ++i) {
        C::SdoTransfer t;
        t.channel = route.channel;
        t.operation = t.request = 1;
        t.step = i + 1;
        t.index = kIndex[i];
        t.subindex = kSub[i];
        t.size = kWidth[i];
        t.requirePreOperational = true;
        t.deadlineUs = deadline;
        t.queueTimeoutUs = t.responseTimeoutUs = 250000;
        if (!exchange(t, kBaseline[i].request, kBaseline[i].reply, now, baseline[i]))
            return 1;
        now += 10;
    }
    // Refusals before any write: the baseline already holds the target, and an unmeasured value.
    L::Tpdo1ConfigurationContext context;
    unsigned char before[sizeof context];
    std::memcpy(before, &context, sizeof context);
    L::Tpdo1ConfigurationRequest request;
    request.transition = L::Tpdo1Transition::EVENT_TIMER;
    const uint16_t refused[2] = {0, 200};
    for (uint16_t value : refused) {
        request.eventTimer = L::Tpdo1EventTimerRaw(value);
        const M::Status status = L::prepareTpdo1Configuration(
            context, reservation, review, request, baseline, 9, C::NmtState::PRE_OPERATIONAL,
            preOperational, 2, now, deadline, options, age);
        const bool unchanged = std::memcmp(before, &context, sizeof context) == 0;
        std::printf("Refused target raw %u: %s detail %" PRId32 " (%s); context unchanged=%s\n",
                    unsigned(value), M::errToString(status.code), status.detail, status.msg,
                    unchanged ? "yes" : "no");
        if (status || !unchanged)
            return 2;
    }
    request.eventTimer = L::Tpdo1EventTimerRaw::RAW_100;
    if (!checked(L::prepareTpdo1Configuration(context, reservation, review, request, baseline, 9,
                                              C::NmtState::PRE_OPERATIONAL, preOperational, 2, now,
                                              deadline, options, age),
                 "prepareTpdo1Configuration"))
        return 1;
    std::puts("One write, then nine exact uploads at the next configuration epoch:");
    M::Binding current = route.channel.binding;
    for (unsigned read = 0;; now += 10) {
        L::PreparedRead work;
        if (!checked(L::nextPdoConfiguration(context, reservation, current,
                                             C::NmtState::PRE_OPERATIONAL, preOperational, now,
                                             work),
                     "nextPdoConfiguration"))
            return 1;
        if (!work.ready)
            break;
        C::SdoCompletion terminal;
        bool scripted = false;
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD) {
            // The caller withdraws its dependent maps and samples here, before the frame can leave.
            if (!checked(L::notePdoConfigurationTransmission(context, reservation, current,
                                                             C::NmtState::PRE_OPERATIONAL,
                                                             preOperational, now),
                         "notePdoConfigurationTransmission"))
                return 1;
            scripted = exchange(work.transfer, kWrite.request, kWrite.reply, now, terminal);
        } else if (read < 9) {
            const uint8_t(&reply)[8] = read == 3 ? kReadback100 : kBaseline[read].reply;
            scripted = exchange(work.transfer, kBaseline[read].request, reply, now, terminal);
            ++read;
        }
        if (!scripted ||
            !checked(L::advancePdoConfiguration(context, reservation, current, terminal, now + 4),
                     "advancePdoConfiguration"))
            return 1;
    }
    C::PdoMapEvidence map;
    L::Tpdo1EventTimerRaw literal = L::Tpdo1EventTimerRaw::RAW_0;
    if (!checked(L::getTpdo1Configuration(context, reservation, current, now, 2, 1316, map, literal),
                 "getTpdo1Configuration"))
        return 1;
    std::printf("Published 1800:05 raw %u; COB-ID %03" PRIx32 ", type %u, map count %u, %u bytes\n",
                unsigned(literal), map.route.cobId, unsigned(map.transmissionType),
                unsigned(map.route.layout.mapping.count), unsigned(map.route.layout.payloadLength));
    std::printf("Configuration epoch %" PRIu32 " -> %" PRIu32 "; confirmed step %u, first effect "
                "step %u, failed step %u, uncertain %u\n",
                route.channel.binding.configurationGeneration, current.configurationGeneration,
                unsigned(context.confirmedStep), unsigned(context.firstEffectStep),
                unsigned(context.failedStep), unsigned(context.uncertain));
    std::puts("A raw value, no unit; the confirmed map qualifies no PDO sample.");
    return context.state == L::PdoConfigurationState::SUCCEEDED && literal == request.eventTimer &&
                   context.confirmedStep == 10 && !context.uncertain
               ? 0
               : 2;
}
