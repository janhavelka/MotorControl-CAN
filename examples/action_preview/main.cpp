// SPDX-License-Identifier: MIT
// The gated enable sequence through the ordinary public calls, with no console, board or drive.
// A small fake answers on the real owner and SDO engine. Its replies are this file's, so the output
// shows the exact service order and nothing about what a drive does. No specimen has run this.
#include "AxisSession.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdio>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace O = MotorControlCANOwner;
namespace E = MotorControlCANExample;

namespace {
// A fake transport: one request at a time, answered on the next receive poll.
struct FakeDrive {
    enum { CAPACITY = 4 };
    O::ReceivedFrame incoming[CAPACITY];
    O::TxReport report;
    unsigned head = 0, count = 0;
    bool reportPending = false;
    uint64_t nowUs = 0;
    uint16_t statusword = 0x8037, monitor = 1, word = 0;
    unsigned downloads = 0;

    void reply(const M::Frame& sent) {
        M::Frame out;
        out.id = 0x581;
        out.dlc = out.payloadLength = 8;
        out.data[1] = sent.data[1];
        out.data[2] = sent.data[2];
        out.data[3] = sent.data[3];
        const uint16_t index = uint16_t(sent.data[1] | (sent.data[2] << 8));
        uint32_t raw = 0;
        unsigned size = 2;
        if (sent.data[0] != 0x40) {
            out.data[0] = 0x60;
            ++downloads;
            word = uint16_t(sent.data[4] | (sent.data[5] << 8));
            std::printf("  write %04X = %04X\n", unsigned(index), unsigned(word));
            const uint16_t low = word == 6 ? 1 : word == 7 ? 3 : 7;
            statusword = uint16_t((statusword & ~L::STATUS_LOW_BITS) | low);
            push(out);
            return;
        }
        switch (index) {
        case 0x2100:
            raw = monitor;
            break;
        case 0x6041:
            raw = statusword;
            break;
        case 0x6061:
            raw = 1;
            size = 1;
            break;
        case 0x1001:
            size = 1;
            break;
        case 0x6064:
        case 0x606c:
            size = 4;
            break;
        case 0x607a:
            raw = 5000;
            size = 4;
            break;
        case 0x60ff:
            raw = 120;
            size = 4;
            break;
        default:
            break;
        }
        out.data[0] = size == 1 ? 0x4f : size == 2 ? 0x4b : 0x43;
        for (unsigned i = 0; i < 4; ++i)
            out.data[4 + i] = uint8_t(raw >> (8 * i));
        push(out);
    }
    void push(const M::Frame& frame) {
        O::ReceivedFrame& item = incoming[(head + count++) % CAPACITY];
        item.frame = frame;
        item.receivedTimeKnown = true;
        item.receivedUs = nowUs;
    }
    static bool receive(void* context, O::ReceivedFrame& output) {
        FakeDrive& fake = *static_cast<FakeDrive*>(context);
        if (!fake.count)
            return false;
        output = fake.incoming[fake.head];
        fake.head = (fake.head + 1) % CAPACITY;
        --fake.count;
        return true;
    }
    static bool transmit(void* context, const M::Frame& frame, O::TransportTxId id, uint32_t epoch) {
        FakeDrive& fake = *static_cast<FakeDrive*>(context);
        fake.report.id = id;
        fake.report.controllerEpoch = epoch;
        fake.report.transmitted = true;
        fake.reportPending = true;
        fake.reply(frame);
        return true;
    }
    static bool takeReport(void* context, O::TxReport& output) {
        FakeDrive& fake = *static_cast<FakeDrive*>(context);
        if (!fake.reportPending)
            return false;
        output = fake.report;
        fake.reportPending = false;
        return true;
    }
    static bool retire(void*, O::TransportTxId, uint32_t) { return true; }
    O::Transport callbacks() {
        O::Transport value;
        value.context = this;
        value.receive = receive;
        value.transmit = transmit;
        value.takeTxReport = takeReport;
        value.retire = retire;
        return value;
    }
};
// Stationary application storage: contexts are too large for a task stack by design.
C::SdoClient services;
FakeDrive drive;
L::ReadContext state;
} // namespace

int main() {
    O::CanBusOwner owner(services, drive.callbacks(), 7);
    static E::AxisSession session(owner);
    C::SdoChannel channel;
    channel.binding.logicalAxisId = 101;
    channel.binding.endpoint.profile = 41;
    channel.binding.endpoint.address = 1;
    channel.binding.bindingGeneration = 3;
    channel.binding.configurationGeneration = 7;
    channel.clientToServerId = 0x601;
    channel.serverToClientId = 0x581;
    L::ReadTarget target;
    target.channel = channel;
    target.nodeGeneration = 11;
    C::NetworkRoute network;
    network.binding = channel.binding;
    network.generation = 11;
    network.heartbeatId = 0x701;
    network.emcyId = 0x81;
    if (!services.configure(0, channel, true, true) ||
        !services.configureNetwork(0, network, C::NetworkObserver(), 0) ||
        !session.open(target, L::SettingsModel::CL86_C, 1, drive.nowUs))
        return 1;
    O::ServiceBudget budget;
    budget.rx = 8;
    budget.tx = 2;
    budget.events = 8;
    budget.starts = 2;
    const auto turn = [&]() {
        drive.nowUs += 500;
        owner.service(drive.nowUs, budget);
        session.service(drive.nowUs);
    };

    // Caller's evidence comes from an ordinary read, under this reservation.
    L::ActionOptions options;
    options.service.queueTimeoutUs = options.service.responseTimeoutUs = 2000;
    options.observationIntervalUs = 1000;
    options.maximumAgeUs = 36000;
    options.commandIntervalUs = L::ENABLE_WORD_INTERVAL_US;
    const uint8_t blocks = uint8_t((1u << unsigned(L::StateBlock::ERRORS)) |
                                   (1u << unsigned(L::StateBlock::MONITORS)) |
                                   (1u << unsigned(L::StateBlock::STATUS)) |
                                   (1u << unsigned(L::StateBlock::FEEDBACK)));
    uint32_t operation = session.beginOperation(drive.nowUs + 100000, drive.nowUs);
    if (!operation || !L::prepareState(state, target, operation, drive.nowUs, drive.nowUs + 100000,
                                       options.service, blocks))
        return 2;
    while (state.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        if (!L::nextRead(state, drive.nowUs, work) ||
            session.admitPrepared(work.transfer, drive.nowUs) != O::BusAdmission::ACCEPTED)
            return 3;
        C::SdoCompletion terminal;
        while (!session.settle(terminal))
            turn();
        if (!L::advanceRead(state, terminal, drive.nowUs))
            return 4;
    }

    // Every default refuses. The gate is the first check, and it stays closed unless opened.
    L::EnableEvidence evidence;
    M::Status status = session.requestEnable(state, evidence, options, drive.nowUs + 200000, drive.nowUs);
    std::printf("default evidence: %s\n", status.msg);
    if (status.code != M::Err::UNSUPPORTED)
        return 5;
    // Opening it states an attended trial with an independent cutoff, bound to one trial record,
    // this reservation, the next operation and an expiry. The values are what the operator reviewed,
    // read again before any word, and refused if they differ. 0x8037 with 2100 reading enabled is
    // the manual's enabled pattern, so the attendant's power-up shaft observation is required too.
    evidence.gate = L::EnableGate::ATTENDED_EXPERIMENT;
    evidence.trial = 1;
    evidence.reservation.target = target;
    evidence.reservation.model = L::SettingsModel::CL86_C;
    evidence.reservation.owner = owner.identity();
    evidence.reservation.sequence = uint32_t(session.sequence().slot) + 1;
    evidence.reservation.generation = session.sequence().generation;
    evidence.reservation.acquiredUs = owner.sequence(session.sequence())->acquiredUs;
    evidence.operation = session.nextOperation();
    evidence.expiresUs = drive.nowUs + 1000000;
    evidence.start = L::EnableStart::NO_MOTION_WRITE_RECORDED;
    evidence.powerUpShaft = L::ShaftObservation::FREE;
    evidence.reviewed = true;
    evidence.reviewedStatusword = 0x8037;
    evidence.reviewedPositionTarget = 5000;
    evidence.reviewedVelocityTarget = 120;
    status = session.requestEnable(state, evidence, options, drive.nowUs + 200000, drive.nowUs);
    if (!status)
        return 6;
    while (session.enableActive())
        turn();
    const L::ActionContext& enable = session.enableAction();
    std::printf("enable outcome %u, execution %u, uncertain %u\n", unsigned(enable.outcome),
                unsigned(enable.execution), unsigned(enable.uncertain));
    std::printf("reported only: low bits %u; this is not torque or a specimen result\n",
                unsigned(enable.statusObservation.statusword & L::STATUS_LOW_BITS));
    if (enable.state != M::ActionState::SUCCEEDED || drive.downloads != 3)
        return 7;

    // The session recorded each word, so a stop can follow the acknowledged 000F.
    const C::SdoCompletion* control = session.latestControl();
    if (!control || control->transfer.data[0] != 0x0f)
        return 8;
    E::StopPolicy policy;
    policy.options = options;
    policy.deadlineUs = drive.nowUs + 20000;
    if (session.requestStop(L::StopKind::QUICK_STOP, enable.read, policy, drive.nowUs) !=
        E::StopPath::CONFIRMED)
        return 9;
    for (unsigned i = 0; i < 6; ++i)
        turn();
    if (drive.word != 0x000b)
        return 10;
    std::printf("stop word %04X, acknowledged only: no shaft claim\n", unsigned(drive.word));
    // A second enable has no fresh history here, whatever the next read says, and the trial is spent.
    evidence.operation = session.nextOperation();
    if (session.requestEnable(state, evidence, options, drive.nowUs + 200000, drive.nowUs))
        return 11;
    return 0;
}
