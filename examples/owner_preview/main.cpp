// SPDX-License-Identifier: MIT
#include "CanBusOwner.h"
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>
#include <cstdio>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace O = MotorControlCANOwner;

namespace {
// One bounded fake ingress queue and one bounded fake controller-report queue.
// No callback consumes the adapter's normal receive path or reads a clock.
struct FakeTransport {
    enum { CAPACITY = 4 };
    O::ReceivedFrame incoming[CAPACITY];
    O::TxReport reports[CAPACITY];
    unsigned rxHead = 0, rxCount = 0, reportHead = 0, reportCount = 0;
    unsigned transmitted = 0, receiveCalls = 0, reportCalls = 0;
    uint64_t nowUs = 0;

    bool push(const M::Frame& frame) {
        if (rxCount == CAPACITY)
            return false;
        O::ReceivedFrame& item = incoming[(rxHead + rxCount) % CAPACITY];
        item.frame = frame;
        item.receivedTimeKnown = true;
        item.receivedUs = nowUs;
        ++rxCount;
        return true;
    }
    static bool receive(void* context, O::ReceivedFrame& output) {
        FakeTransport& fake = *static_cast<FakeTransport*>(context);
        ++fake.receiveCalls;
        if (!fake.rxCount)
            return false;
        output = fake.incoming[fake.rxHead];
        fake.rxHead = (fake.rxHead + 1) % CAPACITY;
        --fake.rxCount;
        return true;
    }
    static bool takeReport(void* context, O::TxReport& output) {
        FakeTransport& fake = *static_cast<FakeTransport*>(context);
        ++fake.reportCalls;
        if (!fake.reportCount)
            return false;
        output = fake.reports[fake.reportHead];
        fake.reportHead = (fake.reportHead + 1) % CAPACITY;
        --fake.reportCount;
        return true;
    }
    static bool transmit(void* context, const M::Frame& request, O::TransportTxId id, uint32_t epoch) {
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
        O::TxReport& report = fake.reports[(fake.reportHead + fake.reportCount) % CAPACITY];
        report.id = id;
        report.controllerEpoch = epoch;
        report.transmitted = true; // Explicit fake actual-TX evidence, separate from return true.
        ++fake.reportCount;
        ++fake.transmitted;
        return true;
    }
    O::Transport callbacks() {
        O::Transport transport;
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
    O::CanBusOwner owner(services, fake.callbacks(), 17);
    O::SequenceId sequence;
    if (!owner.beginSequence(target.channel.binding, 1, 0, sequence) ||
        !owner.beginOperation(sequence, 1, 1000, 0))
        return 1;
    L::ReadOptions options;
    options.queueTimeoutUs = 100;
    options.responseTimeoutUs = 100;
    L::DriverContext context;
    if (!L::prepareControlRead(context, target, 1, 0, 1000, options))
        return 1;
    O::ServiceBudget budget;
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
        O::BusRequest request;
        request.sequence = sequence;
        request.transfer = prepared.transfer;
        O::RequestId id;
        if (owner.admit(request, nowUs, id) != O::BusAdmission::ACCEPTED)
            return 1;
        // Admission copied this transfer. The owner retains its terminal until release.
        const O::Completion* retained = nullptr;
        for (unsigned turn = 0; turn < 20 && !retained; ++turn) {
            fake.nowUs = ++nowUs;
            if (!owner.service(nowUs, budget))
                return 1;
            retained = owner.result(id);
        }
        if (!retained || retained->reason != O::CompletionReason::SERVICE)
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

    // Cached diagnostics copy owner evidence without another service turn or callback.
    const unsigned receiveCalls = fake.receiveCalls, reportCalls = fake.reportCalls;
    O::Health health;
    O::DriverState state = O::DriverState::UNINIT;
    const O::Stats stats = owner.stats();
    if (!owner.health(target.channel.binding, health) ||
        !owner.driverState(target.channel.binding, state) || state != O::DriverState::READY ||
        health.totalSuccess != 2 || health.totalFailures || health.consecutiveFailures ||
        !health.lastOkKnown || !health.lastSeenKnown || stats.txFrames != 2 || stats.rxFrames != 6)
        return 1;
    std::printf("Cached communication=READY; services=%u/%u, confirmed TX=%u, RX=%u.\n",
                static_cast<unsigned>(health.totalSuccess),
                static_cast<unsigned>(health.totalFailures), static_cast<unsigned>(stats.txFrames),
                static_cast<unsigned>(stats.rxFrames));
    const O::OwnerResources before = owner.resources();
    O::Diagnostic diagnostic;
    unsigned copied = 0;
    // A real console uses its own output budget and leaves transport service scheduled.
    while (copied < 3 && owner.takeDiagnostic(diagnostic))
        ++copied;
    // Two exchanges produce 12 records: 2 TX, 2 TX reports, 6 RX and 2 terminals. The trace
    // holds well over one turn, so an ordinary session like this loses none of them.
    if (copied != 3 || before.traceUsed != 12 ||
        owner.resources().traceUsed != before.traceUsed - copied || stats.traceOverwritten ||
        !owner.clearStats())
        return 1;
    O::Health after;
    if (!owner.health(target.channel.binding, after) || after.totalSuccess || after.totalFailures ||
        after.state != health.state || after.consecutiveFailures != health.consecutiveFailures ||
        after.lastOkKnown != health.lastOkKnown || after.lastOkUs != health.lastOkUs ||
        after.lastErrorKnown != health.lastErrorKnown || after.lastErrorUs != health.lastErrorUs ||
        after.lastError.code != health.lastError.code ||
        after.lastError.detail != health.lastError.detail ||
        after.lastSeenKnown != health.lastSeenKnown || after.lastSeenUs != health.lastSeenUs ||
        owner.stats().txFrames || owner.stats().rxFrames || owner.stats().traceOverwritten ||
        owner.stats().maxServiceGapUs || owner.resources().traceUsed != before.traceUsed - copied ||
        fake.receiveCalls != receiveCalls || fake.reportCalls != reportCalls ||
        fake.transmitted != 2)
        return 1;
    std::printf(
        "Copied %u diagnostics; stats reset preserves communication evidence and %u copies.\n",
        copied, static_cast<unsigned>(owner.resources().traceUsed));
    return 0;
}
