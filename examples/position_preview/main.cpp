// SPDX-License-Identifier: MIT
// Native use of the installed core only: no CLI, SDK, CAN controller, owner or stack.
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <cstdio>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;

namespace {
// A host simulation of one drive's replies. It is not a drive, a status dialect or a measurement:
// its acceptance (6041 bit 12) and completion (bit 15) behavior is invented for this example.
struct Simulated {
    uint16_t status = 0x8037, monitor = 1, word = 0;
    int32_t target = 0, position = 0;
    unsigned travel = 0;
    bool moving = false;
    uint32_t read(uint16_t index, uint8_t sub) {
        switch (index) {
        case 0x1018: return sub == 0 ? 3 : 0x300 + sub;
        case 0x2100: return monitor;
        case 0x6041:
            if (moving && !travel--) {
                moving = false;
                status = uint16_t(status | 0x8000);
                position = target;
            }
            return status;
        case 0x6061: return 1;
        case 0x6064: return uint32_t(position);
        default: return index == 0x1000 ? 0x00040192 : 7;
        }
    }
    void write(const C::SdoTransfer& t) {
        const uint32_t value = uint32_t(t.data[0]) | (uint32_t(t.data[1]) << 8) |
                               (uint32_t(t.data[2]) << 16) | (uint32_t(t.data[3]) << 24);
        if (t.index == 0x607a)
            target = int32_t(value);
        if (t.index != 0x6040)
            return;
        if (value == 0x001f && word == 0x000f) {
            status = uint16_t((status | 0x1000) & ~0x8000);
            moving = true;
            travel = 1;
        } else if (value == 0x000f) {
            status = uint16_t(status & ~0x1000);
        }
        word = uint16_t(value);
    }
};
// The terminal a sole owner would publish for a successful service, built by hand for the example.
C::SdoCompletion answer(const C::SdoTransfer& t, Simulated& drive, uint64_t admitted, uint64_t now,
                        M::Generation node) {
    C::SdoCompletion c;
    c.transfer = t;
    c.outcome = C::SdoOutcome::SUCCESS;
    c.nodeGeneration = node;
    c.admittedUs = admitted;
    c.completedUs = now;
    c.queued = c.transmissionPossible = c.protocolAcknowledged = c.effectPossible = true;
    if (t.direction == C::SdoDirection::UPLOAD) {
        const uint32_t value = drive.read(t.index, t.subindex);
        c.size = t.size;
        for (unsigned i = 0; i < c.size; ++i)
            c.data[i] = uint8_t(value >> (8 * i));
    } else {
        drive.write(t);
    }
    return c;
}
// Runs one ordinary read to its end against the simulation.
bool run(L::ReadContext& read, Simulated& drive, uint64_t& now, M::Generation node) {
    while (read.state == M::ReadState::ACTIVE) {
        L::PreparedRead work;
        if (!L::nextRead(read, now, work) || !work.ready)
            return false;
        const uint64_t admitted = now;
        now += 500;
        if (!L::advanceRead(read, answer(work.transfer, drive, admitted, now, node), now))
            return false;
    }
    return read.state == M::ReadState::SUCCEEDED;
}
} // namespace

int main() {
    M::AxisConfig axis;
    axis.binding.logicalAxisId = 1;
    axis.binding.endpoint.profile = 1;
    axis.binding.endpoint.address = 9;
    axis.binding.bindingGeneration = 1;
    axis.binding.configurationGeneration = 1;
    axis.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::ASSUMED);
    axis.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS; // 6081 is rpm: motor turns per minute.
    axis.nativeVelocityTime = M::TimeUnit::MINUTE;
    M::PositionRequest request;
    request.configurationGeneration = 1;
    request.value = M::Rational(250);
    L::DriverSettings settings;
    settings.positionSpeedRpm = 120;
    settings.rampUpTimeMs = 100;
    settings.rampDownTimeMs = 200;
    L::PreparedPositionParameters prepared;
    if (!L::preparePositionParameters(prepared, L::SettingsModel::CL86_C, request, axis, nullptr,
                                      settings, 0))
        return 1;
    std::printf("Prepared absolute target: %ld pulses, %lu rpm, ramps %lu/%lu ms\n",
                static_cast<long>(prepared.targetNative),
                static_cast<unsigned long>(prepared.settings.positionSpeedRpm),
                static_cast<unsigned long>(prepared.settings.rampUpTimeMs),
                static_cast<unsigned long>(prepared.settings.rampDownTimeMs));

    // The inputs of a move. The route, reservation and acknowledged 000F are fabricated here; an
    // application takes them from its sole owner and its own command history.
    L::ReadTarget target;
    target.channel.binding = axis.binding;
    target.channel.binding.endpoint.protocol = M::Protocol::CANOPEN;
    target.channel.clientToServerId = 0x609;
    target.channel.serverToClientId = 0x589;
    target.nodeGeneration = 1;
    axis.binding = target.channel.binding;
    L::AxisReservation reservation;
    reservation.target = target;
    reservation.model = L::SettingsModel::CL86_C;
    reservation.owner = reservation.sequence = reservation.generation = 1;
    L::ReadOptions service;
    service.queueTimeoutUs = service.responseTimeoutUs = 2000;
    Simulated drive;
    uint64_t now = 1000;
    C::SdoTransfer enableWord;
    enableWord.channel = target.channel;
    enableWord.operation = enableWord.request = enableWord.step = 1;
    enableWord.index = 0x6040;
    enableWord.direction = C::SdoDirection::DOWNLOAD;
    enableWord.size = 2;
    enableWord.data[0] = 0x0f;
    enableWord.deadlineUs = 100000;
    enableWord.queueTimeoutUs = enableWord.responseTimeoutUs = 2000;
    const C::SdoCompletion control = answer(enableWord, drive, now, now + 500, target.nodeGeneration);
    now += 500;
    static L::ReadContext identityRead, state, observation;
    L::IdentityObservation identity;
    if (!L::prepareIdentity(identityRead, target, 2, now, now + 100000, service) ||
        !run(identityRead, drive, now, target.nodeGeneration) ||
        !L::getIdentity(identityRead, target, now, 100000, identity))
        return 2;
    const uint8_t blocks = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                   (1u << unsigned(L::StateBlock::STATUS)));
    if (!L::prepareState(state, target, 3, now, now + 100000, service, blocks) ||
        !run(state, drive, now, target.nodeGeneration))
        return 3;
    M::MoveRequest move;
    move.position = request;
    move.speed.value = M::Rational(120);
    move.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    move.speed.configurationGeneration = 1;
    L::MoveProfile profile;
    profile.rampUpTimeMs = 100;
    profile.rampDownTimeMs = 200;
    L::MoveOptions options;
    options.service = service;
    options.observationIntervalUs = 1000;
    options.maximumAgeUs = 40000;
    static L::MoveContext context;

    // Without the reviewed measurement of acceptance and completion the gate stays closed.
    const M::Status gated = L::prepareMoveAbsolute(
        context, reservation, target, state, control, identity, L::MoveEvidence(), move, profile,
        nullptr, axis, nullptr, 4, now, now + 400000, options);
    std::printf("Closed gate: %s (UNSUPPORTED is expected; no specimen has qualified a move)\n",
                gated.msg);
    if (gated.code != M::Err::UNSUPPORTED)
        return 4;

    // SIMULATION ONLY. This record is invented to show the call sequence against the fake above.
    // A real application supplies the reviewed record of its own firmware, or the gate stays closed.
    L::MoveEvidence simulated;
    simulated.gate = L::MoveGate::MEASURED;
    simulated.record = 1;
    simulated.model = L::SettingsModel::CL86_C;
    simulated.firmware.deviceType = identity.deviceType;
    simulated.firmware.hardwareVersion = identity.hardwareVersion;
    simulated.firmware.softwareVersion = identity.softwareVersion;
    simulated.firmware.vendorId = identity.vendorId;
    simulated.firmware.productCode = identity.productCode;
    simulated.firmware.revision = identity.revision;
    simulated.firmware.modelCode = identity.modelCode;
    simulated.firmware.firmwareCode = identity.firmwareCode;
    simulated.acceptance.source = simulated.completion.source = L::MoveSignalSource::STATUSWORD;
    simulated.acceptance.mask = simulated.acceptance.value = 0x1000;
    simulated.completion.mask = simulated.completion.value = 0x8000;
    simulated.rule = L::CompletionRule::AFTER_ABSENCE;
    if (!L::prepareMoveAbsolute(context, reservation, target, state, control, identity, simulated,
                                move, profile, nullptr, axis, nullptr, 4, now, now + 400000, options))
        return 5;
    while (context.state == M::ActionState::ACTIVE) {
        L::PreparedMove work;
        if (!L::nextMove(context, observation, reservation, target, now, work))
            return 6;
        if (work.observe && (!L::beginMoveObservation(context, observation, reservation, target, now) ||
                             !L::nextMove(context, observation, reservation, target, now, work)))
            return 7;
        if (!work.ready) {
            now = work.eligibleUs > now ? work.eligibleUs : now + 1;
            continue;
        }
        // An application calls this after owner admission, naming its retained settings contexts.
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD &&
            !L::noteMoveTransmission(context, reservation, target, nullptr, 0, now))
            return 8;
        if (work.transfer.direction == C::SdoDirection::DOWNLOAD)
            std::printf("SIMULATED download %04X: %02X %02X %02X %02X\n",
                        unsigned(work.transfer.index), unsigned(work.transfer.data[0]),
                        unsigned(work.transfer.data[1]), unsigned(work.transfer.data[2]),
                        unsigned(work.transfer.data[3]));
        const uint64_t admitted = now;
        now += 500;
        if (!L::advanceMove(context, observation, reservation, target,
                            answer(work.transfer, drive, admitted, now, target.nodeGeneration), now))
            return 9;
    }
    std::printf("SIMULATED move: %s after %lu cycles; completion is a controller report, never a "
                "measured position or standstill\n",
                context.state == M::ActionState::SUCCEEDED ? "completed" : "failed",
                static_cast<unsigned long>(context.observations));
    return context.state == M::ActionState::SUCCEEDED ? 0 : 10;
}
