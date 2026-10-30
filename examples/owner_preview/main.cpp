// SPDX-License-Identifier: MIT
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <cstdio>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;

namespace {
// One bounded fake ingress queue and one bounded fake controller-report queue.
// No callback consumes the adapter's normal receive path or reads a clock.
struct FakeTransport {
    enum { CAPACITY = 4 };
    E::ReceivedFrame incoming[CAPACITY];
    E::TxReport reports[CAPACITY];
    unsigned rxHead = 0, rxCount = 0, reportHead = 0, reportCount = 0;
    unsigned transmitted = 0;
    uint64_t nowUs = 0;

    bool push(const M::Frame& frame) {
        if (rxCount == CAPACITY)
            return false;
        E::ReceivedFrame& item = incoming[(rxHead + rxCount) % CAPACITY];
        item.frame = frame;
        item.receivedTimeKnown = true;
        item.receivedUs = nowUs;
        ++rxCount;
        return true;
    }
    static bool receive(void* context, E::ReceivedFrame& output) {
        FakeTransport& fake = *static_cast<FakeTransport*>(context);
        if (!fake.rxCount)
            return false;
        output = fake.incoming[fake.rxHead];
        fake.rxHead = (fake.rxHead + 1) % CAPACITY;
        --fake.rxCount;
        return true;
    }
    static bool takeReport(void* context, E::TxReport& output) {
        FakeTransport& fake = *static_cast<FakeTransport*>(context);
        if (!fake.reportCount)
            return false;
        output = fake.reports[fake.reportHead];
        fake.reportHead = (fake.reportHead + 1) % CAPACITY;
        --fake.reportCount;
        return true;
    }
    static bool transmit(void* context, const M::Frame& request, C::SdoTxId id) {
        FakeTransport& fake = *static_cast<FakeTransport*>(context);
        // This fixture implements only the two explicit expedited uploads below.
        if (request.id != 0x605 || request.payloadLength != 8 || request.data[0] != 0x40 ||
            request.data[2] != 0x25 || request.data[1] > 1 || request.data[3] != 0 ||
            fake.rxCount > CAPACITY - 3 || fake.reportCount == CAPACITY)
            return false;
        M::Frame heartbeat;
        heartbeat.id = 0x705;
        heartbeat.dlc = heartbeat.payloadLength = 1;
        heartbeat.data[0] = 5;
        M::Frame emcy;
        emcy.id = 0x085;
        emcy.dlc = emcy.payloadLength = 8;
        // Synthetic zero-code EMCY: event history, never a fault-clear operation.
        M::Frame reply;
        reply.id = 0x585;
        reply.dlc = reply.payloadLength = 8;
        reply.data[0] = 0x4b;
        reply.data[1] = request.data[1];
        reply.data[2] = 0x25;
        const uint16_t raw = request.data[1] == 0 ? 2 : 1000;
        reply.data[4] = static_cast<uint8_t>(raw);
        reply.data[5] = static_cast<uint8_t>(raw >> 8);
        if (!fake.push(heartbeat) || !fake.push(emcy) || !fake.push(reply))
            return false;
        E::TxReport& report = fake.reports[(fake.reportHead + fake.reportCount) % CAPACITY];
        report.id = id;
        report.transmitted = true; // Explicit fake actual-TX evidence, separate from return true.
        ++fake.reportCount;
        ++fake.transmitted;
        return true;
    }
    E::Transport callbacks() {
        E::Transport transport;
        transport.context = this;
        transport.receive = receive;
        transport.transmit = transmit;
        transport.takeTxReport = takeReport;
        return transport;
    }
};
} // namespace

int main() {
    std::puts("Offline application owner: real SDO adapter, synthetic controller/server traffic.");
    std::puts(
        "Fresh session and usable service are illustrative external premises; Q20 stays open.");
    L::ReadTarget target;
    target.channel.binding.logicalAxisId = 9;
    target.channel.binding.endpoint.protocol = M::Protocol::CANOPEN;
    target.channel.binding.endpoint.profile = 1;
    target.channel.binding.endpoint.address = 5;
    target.channel.binding.bindingGeneration = 1;
    target.channel.binding.configurationGeneration = 1;
    target.channel.clientToServerId = 0x605;
    target.channel.serverToClientId = 0x585;
    target.nodeGeneration = 1;
    C::NetworkRoute network;
    network.binding = target.channel.binding;
    network.generation = target.nodeGeneration;
    network.heartbeatId = 0x705;
    network.emcyId = 0x085;
    C::SdoClient services;
    if (!services.configure(0, target.channel, true, true) ||
        !services.configureNetwork(0, network, C::NetworkObserver(), 0))
        return 1;
    FakeTransport fake;
    E::CanBusOwner owner(services, fake.callbacks(), 17);
    E::SequenceId sequence;
    if (!owner.beginSequence(target.channel.binding, 1, 0, sequence) ||
        !owner.beginOperation(sequence, 1, 1000, 0))
        return 1;
    L::ReadOptions options;
    options.queueTimeoutUs = 100;
    options.responseTimeoutUs = 100;
    L::DriverContext context;
    if (!L::prepareControlRead(context, target, 1, 0, 1000, options))
        return 1;
    E::ServiceBudget budget;
    budget.rx = 4;
    budget.tx = 1;
    budget.events = 4;
    budget.starts = 1;
    uint64_t nowUs = 0;
    for (;;) {
        L::PreparedRead prepared;
        if (!L::nextDriver(context, nowUs, prepared))
            return 1;
        if (!prepared.ready)
            break;
        E::BusRequest request;
        request.sequence = sequence;
        request.transfer = prepared.transfer;
        E::RequestId id;
        if (owner.admit(request, nowUs, id) != E::BusAdmission::ACCEPTED)
            return 1;
        // Admission copied this transfer. The owner retains its terminal until release.
        const E::Completion* retained = nullptr;
        for (unsigned turn = 0; turn < 20 && !retained; ++turn) {
            fake.nowUs = ++nowUs;
            if (!owner.service(nowUs, budget))
                return 1;
            retained = owner.result(id);
        }
        if (!retained || retained->reason != E::CompletionReason::SERVICE)
            return 1;
        const C::SdoCompletion copied = retained->service;
        // Typed stepping belongs to the application, outside owner callbacks.
        if (!L::advanceDriver(context, copied, nowUs) || !owner.release(id))
            return 1;
    }
    L::ControlObservation observed;
    if (!L::getControl(context, target, nowUs, 1000, observed) || !owner.releaseSequence(sequence))
        return 1;
    const C::NodeSnapshot* node = services.node(0);
    if (!node || node->nmt != C::NmtState::OPERATIONAL || node->emcyCount != 2 ||
        fake.transmitted != 2 || observed.loopModeRaw != 2 || observed.encoderResolutionRaw != 1000)
        return 1;
    std::printf("Two retained uploads: loop=%u, encoder raw=%u; passive EMCYs=%u.\n",
                static_cast<unsigned>(observed.loopModeRaw),
                static_cast<unsigned>(observed.encoderResolutionRaw),
                static_cast<unsigned>(node->emcyCount));
    std::puts(
        "Raw readback is not physical activation, encoder scale or harmless-read qualification.");
    return 0;
}
