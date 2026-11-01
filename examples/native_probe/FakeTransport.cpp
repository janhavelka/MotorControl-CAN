// SPDX-License-Identifier: MIT
#include "FakeTransport.h"

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace {
struct Field {
    uint16_t index;
    uint8_t subindex, width;
    uint32_t raw;
};
// Widths follow the public raw read types. Values are invented fixture data,
// not vendor defaults or identifiers. No absent 1018:04 is synthesized.
const Field fields[] = {
    {0x1000, 0, 4, 0x12345678}, {0x1009, 0, 2, 0x1122},     {0x100a, 0, 2, 0x3344},
    {0x1018, 0, 1, 3},          {0x1018, 1, 4, 0xa1b2c3d4}, {0x1018, 2, 4, 0x55667788},
    {0x1018, 3, 4, 0x99aabbcc}, {0x2000, 0, 2, 0x4567},     {0x2001, 0, 2, 0x6789},
    {0x1005, 0, 4, 0x80},       {0x1014, 0, 4, 0x85},       {0x1017, 0, 2, 250},
    {0x1200, 0, 1, 2},          {0x1200, 1, 4, 0x605},      {0x1200, 2, 4, 0x585}};
M::Frame frame(uint32_t id, uint8_t size) noexcept {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = size;
    return value;
}
} // namespace
M::LICHUAN_C::ReadTarget FakeTransport::target() noexcept {
    M::LICHUAN_C::ReadTarget value;
    value.channel.binding.logicalAxisId = 9;
    value.channel.binding.endpoint.profile = 1;
    value.channel.binding.endpoint.address = 5;
    value.channel.binding.bindingGeneration = 1;
    value.channel.binding.configurationGeneration = 1;
    value.channel.clientToServerId = 0x605;
    value.channel.serverToClientId = 0x585;
    value.nodeGeneration = 1;
    return value;
}
Transport FakeTransport::callbacks() noexcept {
    Transport value;
    value.context = this;
    value.receive = receive;
    value.transmit = transmit;
    value.takeTxReport = takeReport;
    value.observe = observe;
    return value;
}
bool FakeTransport::setTime(uint64_t now) noexcept {
    if (now < now_)
        return false;
    now_ = now;
    return true;
}
bool FakeTransport::push(const M::Frame& value) noexcept {
    if (!M::validateFrame(value) || rxCount_ == INGRESS_CAPACITY)
        return false;
    ReceivedFrame item;
    item.frame = value;
    item.receivedTimeKnown = true;
    item.receivedUs = now_;
    incoming_[(rxHead_ + rxCount_) % INGRESS_CAPACITY] = item;
    ++rxCount_;
    return true;
}
bool FakeTransport::receive(void* context, ReceivedFrame& output) noexcept {
    FakeTransport& fake = *static_cast<FakeTransport*>(context);
    ++fake.receiveCalls_;
    if (!fake.rxCount_)
        return false;
    output = fake.incoming_[fake.rxHead_];
    fake.rxHead_ = (fake.rxHead_ + 1) % INGRESS_CAPACITY;
    --fake.rxCount_;
    return true;
}
bool FakeTransport::takeReport(void* context, TxReport& output) noexcept {
    FakeTransport& fake = *static_cast<FakeTransport*>(context);
    if (!fake.reportCount_)
        return false;
    output = fake.reports_[fake.reportHead_];
    fake.reportHead_ = (fake.reportHead_ + 1) % REPORT_CAPACITY;
    --fake.reportCount_;
    return true;
}
void FakeTransport::observe(void* context, const ReceivedFrame&, uint64_t) noexcept {
    ++static_cast<FakeTransport*>(context)->observed_;
}
bool FakeTransport::transmit(void* context, const M::Frame& request, C::SdoTxId id,
                             uint32_t epoch) noexcept {
    FakeTransport& fake = *static_cast<FakeTransport*>(context);
    ++fake.transmitCalls_;
    if (!M::validateFrame(request) || request.extended || request.rtr || request.id != 0x605 ||
        request.payloadLength != 8 || request.data[0] != 0x40 || epoch != 1)
        return false;
    const Field* selected = nullptr;
    const uint16_t index = uint16_t(request.data[1]) | uint16_t(request.data[2]) << 8;
    for (unsigned i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        if (fields[i].index == index && fields[i].subindex == request.data[3])
            selected = &fields[i];
    const unsigned needed = fake.mode_ == Mode::SILENT ? 3 : 4;
    if (!selected || fake.rxCount_ > INGRESS_CAPACITY - needed ||
        fake.reportCount_ == REPORT_CAPACITY)
        return false;
    // Preflight all queues before publishing any side effect. Each accepted
    // handoff has separate actual-TX evidence and interleaved passive traffic.
    M::Frame heartbeat = frame(0x705, 1);
    heartbeat.data[0] = 5;
    M::Frame emergency = frame(0x85, 8);
    emergency.data[0] = 0x10;
    emergency.data[1] = 0x23;
    emergency.data[2] = 1;
    M::Frame unrelated = frame(0x706, 1);
    unrelated.data[0] = 0x7f;
    fake.push(heartbeat);
    fake.push(emergency);
    fake.push(unrelated);
    if (fake.mode_ != Mode::SILENT) {
        M::Frame reply = frame(0x585, 8);
        reply.data[0] = fake.mode_ == Mode::ABORT ? 0x80
                        : selected->width == 1    ? 0x4f
                        : selected->width == 2    ? 0x4b
                                                  : 0x43;
        reply.data[1] = request.data[1];
        reply.data[2] = request.data[2];
        reply.data[3] = request.data[3];
        const uint32_t raw = fake.mode_ == Mode::ABORT ? 0xfedcba98u : selected->raw;
        for (unsigned i = 0; i < 4; ++i)
            reply.data[4 + i] = static_cast<uint8_t>(raw >> (8 * i));
        fake.push(reply);
    }
    TxReport report;
    report.id = id;
    report.controllerEpoch = epoch;
    report.transmitted = true;
    fake.reports_[(fake.reportHead_ + fake.reportCount_) % REPORT_CAPACITY] = report;
    ++fake.reportCount_;
    ++fake.transmitted_;
    return true;
}
} // namespace MotorControlCANExample
