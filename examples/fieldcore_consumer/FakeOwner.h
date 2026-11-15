// SPDX-License-Identifier: MIT
#pragma once
#include "MotorModule.h"

namespace FieldCoreConsumer {
// A one-slot, deferred terminal supplier shared by main and the consumer regression. These are
// invented controller reports and owner premises, never captured traffic or firmware qualification.
struct FakeOwner {
    C::SdoCompletion terminal;
    WireSettlement wire = WireSettlement::SETTLED;
    C::SdoOutcome nextOutcome = C::SdoOutcome::SUCCESS;
    WireSettlement nextWire = WireSettlement::SETTLED;
    uint64_t dueUs = 0;
    unsigned submitted = 0;
    unsigned cycles = 0;
    bool pending = false;
    bool ready = false;
    bool triggered = false;
    int32_t target = 0;

    bool canAdmit(uint64_t nowUs) const {
        return !pending && nowUs <= UINT64_MAX - 500;
    }
    bool admit(const C::SdoTransfer& transfer, M::Generation node, uint64_t nowUs) {
        if (!canAdmit(nowUs))
            return false;
        terminal = C::SdoCompletion();
        terminal.transfer = transfer;
        terminal.nodeGeneration = node;
        terminal.admittedUs = nowUs;
        terminal.outcome = nextOutcome;
        wire = nextWire;
        nextOutcome = C::SdoOutcome::SUCCESS;
        nextWire = WireSettlement::SETTLED;
        dueUs = nowUs + 500; // Explicit simulation latency, not manufacturer timing.
        pending = true;
        ready = false;
        ++submitted;
        return true;
    }
    bool poll(uint64_t nowUs) {
        if (!pending || nowUs < dueUs)
            return false;
        if (ready)
            return true;
        auto& c = terminal;
        const auto& t = c.transfer;
        c.completedUs = nowUs;
        c.queued = c.transmissionPossible = true;
        c.transmissionConfirmed = wire == WireSettlement::SETTLED;
        c.protocolAcknowledged = c.outcome == C::SdoOutcome::SUCCESS;
        c.effectPossible = true;
        c.effectUncertain = !c.protocolAcknowledged;
        if (c.protocolAcknowledged && t.direction == C::SdoDirection::UPLOAD) {
            uint32_t value = 7; // Synthetic identity, no real model/firmware inference.
            switch (t.index) {
            case 0x1000: value = 0x12345678; break;
            case 0x1018: value = t.subindex == 0 ? 3 : 0x300 + t.subindex; break;
            case 0x2100:
                if (triggered)
                    ++cycles;
                value = 1;
                break;
            case 0x2101:
            case 0x2102: value = 0; break;
            case 0x6041:
                value = !triggered ? 0x8037 : cycles < 2 ? 0x1037 : 0x9037;
                break;
            case 0x6061: value = 1; break;
            case 0x6064: value = cycles >= 2 ? uint32_t(target) : 0; break;
            case 0x606c: value = 0; break;
            default: break;
            }
            c.size = t.size;
            for (unsigned i = 0; i < c.size; ++i)
                c.data[i] = uint8_t(value >> (8 * i));
        } else if (c.protocolAcknowledged && t.direction == C::SdoDirection::DOWNLOAD) {
            if (t.index == 0x607a) {
                const uint32_t value = uint32_t(t.data[0]) | (uint32_t(t.data[1]) << 8) |
                                       (uint32_t(t.data[2]) << 16) | (uint32_t(t.data[3]) << 24);
                target = static_cast<int32_t>(value);
            }
            if (t.index == 0x6040 && t.data[0] == 0x1f)
                triggered = true;
        }
        ready = true;
        return true;
    }
    // Only after the module consumes the terminal; rejected correlation retains this slot.
    void release() { pending = ready = false; }
};

struct Fixture {
    LiveAxis live;
    L::ReadContext identityRead;
    L::ReadContext stateRead;
    L::IdentityObservation identity;
    C::SdoCompletion control;
    L::MoveEvidence evidence;
    M::MoveRequest request;
    L::MoveProfile profile;
    L::MoveOptions options;
};

inline bool runRead(L::ReadContext& read, FakeOwner& owner, uint64_t& nowUs) {
    for (unsigned i = 0; i < L::READ_FIELDS && read.state == M::ReadState::ACTIVE; ++i) {
        L::PreparedRead work;
        if (!L::nextRead(read, nowUs, work) || !work.ready ||
            !owner.admit(work.transfer, read.target.nodeGeneration, nowUs))
            return false;
        nowUs += 500;
        if (!owner.poll(nowUs) || !L::advanceRead(read, owner.terminal, nowUs))
            return false;
        owner.release();
    }
    return read.state == M::ReadState::SUCCEEDED;
}

inline bool initialize(Fixture& f, FakeOwner& owner, uint64_t& nowUs) {
    auto& binding = f.live.target.channel.binding;
    binding.logicalAxisId = 42; // Application identity is independent of node 9.
    binding.endpoint.protocol = M::Protocol::CANOPEN;
    binding.endpoint.profile = 1;
    binding.endpoint.address = 9;
    binding.bindingGeneration = binding.configurationGeneration = 1;
    f.live.target.channel.clientToServerId = 0x609;
    f.live.target.channel.serverToClientId = 0x589;
    f.live.target.nodeGeneration = 1;
    f.live.axis.binding = binding;
    f.live.axis.units.commandStepsPerMotorTurn = M::UnitScale(3200, 1, M::ScaleSource::ASSUMED);
    f.live.axis.nativeRateBasis = M::NativeRateBasis::MOTOR_TURNS;
    f.live.axis.nativeVelocityTime = M::TimeUnit::MINUTE;
    f.live.reservation.target = f.live.target;
    f.live.reservation.model = L::SettingsModel::CL86_C;
    f.live.reservation.owner = f.live.reservation.sequence = f.live.reservation.generation = 1;
    f.options.service.queueTimeoutUs = f.options.service.responseTimeoutUs = 2000;
    f.options.observationIntervalUs = 1000;
    f.options.maximumAgeUs = 40000;

    // Fabricated pre-existing acknowledged enable history. Initialization sends no enable command.
    auto& control = f.control;
    control.transfer.channel = f.live.target.channel;
    control.transfer.operation = control.transfer.request = control.transfer.step = 1;
    control.transfer.index = 0x6040;
    control.transfer.direction = C::SdoDirection::DOWNLOAD;
    control.transfer.size = 2;
    control.transfer.data[0] = 0x0f;
    control.transfer.deadlineUs = 100000;
    control.transfer.queueTimeoutUs = control.transfer.responseTimeoutUs = 2000;
    control.nodeGeneration = 1;
    control.outcome = C::SdoOutcome::SUCCESS;
    control.completedUs = 500;
    control.queued = control.transmissionPossible = control.protocolAcknowledged = true;
    control.effectPossible = true;
    nowUs = 1000;
    if (!L::prepareIdentity(f.identityRead, f.live.target, 2, nowUs, nowUs + 100000,
                            f.options.service) ||
        !runRead(f.identityRead, owner, nowUs) ||
        !L::getIdentity(f.identityRead, f.live.target, nowUs, 100000, f.identity))
        return false;
    const uint8_t blocks = uint8_t((1u << unsigned(L::StateBlock::MONITORS)) |
                                   (1u << unsigned(L::StateBlock::STATUS)));
    if (!L::prepareState(f.stateRead, f.live.target, 3, nowUs, nowUs + 100000,
                         f.options.service, blocks) || !runRead(f.stateRead, owner, nowUs))
        return false;

    // This gate is valid only inside this invented world. It is not a reviewed hardware grant.
    auto& e = f.evidence;
    e.gate = L::MoveGate::MEASURED;
    e.record = 1;
    e.model = L::SettingsModel::CL86_C;
    e.firmware.deviceType = f.identity.deviceType;
    e.firmware.hardwareVersion = f.identity.hardwareVersion;
    e.firmware.softwareVersion = f.identity.softwareVersion;
    e.firmware.vendorId = f.identity.vendorId;
    e.firmware.productCode = f.identity.productCode;
    e.firmware.revision = f.identity.revision;
    e.firmware.modelCode = f.identity.modelCode;
    e.firmware.firmwareCode = f.identity.firmwareCode;
    e.acceptance.source = e.completion.source = L::MoveSignalSource::STATUSWORD;
    e.acceptance.mask = e.acceptance.value = 0x1000;
    e.completion.mask = e.completion.value = 0x8000;
    e.rule = L::CompletionRule::AFTER_ABSENCE;
    f.request.position.value = M::Rational(250);
    f.request.position.configurationGeneration = 1;
    f.request.speed.value = M::Rational(120);
    f.request.speed.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    f.request.speed.configurationGeneration = 1;
    f.profile.rampUpTimeMs = 100;
    f.profile.rampDownTimeMs = 200;
    return true;
}

inline M::Status start(MotorModule& module, const Fixture& f, uint64_t nowUs) {
    return prepareMove(module, f.live, f.stateRead, f.control, f.identity, f.evidence, f.request,
                       f.profile, f.options, 4, nowUs, nowUs + 400000);
}

// One cooperative owner turn. Other heartbeat/PDO/EMCY dispatch belongs beside this in real code.
inline M::Status service(MotorModule& module, const LiveAxis& live, FakeOwner& owner,
                         uint64_t nowUs) {
    if (owner.pending) {
        if (!owner.poll(nowUs))
            return M::Ok();
        const M::Status consumed = advance(module, live, owner.terminal, owner.wire, nowUs);
        if (consumed)
            owner.release();
        return consumed;
    }
    // Reserve the complete fake admission premise before a cycle or possible-TX notice mutates
    // the module. The same serialized owner then copies the transfer without another rejection.
    if (!owner.canAdmit(nowUs))
        return M::Status(M::Err::INVALID_CONFIG, 0, "fake owner cannot admit at supplied time");
    // A yielded wait can expire without another owner terminal. End it locally instead of
    // repeatedly returning nextMove's TIME error while leaving an ACTIVE operation retained.
    if (!module.outstanding && module.move.state == M::ActionState::ACTIVE &&
        nowUs >= module.move.deadlineUs)
        return interrupt(module, live, M::MoveInterruption::DEADLINE, nowUs);
    L::PreparedMove work;
    const M::Status next = nextStep(module, live, nowUs, work);
    if (!next || !work.ready)
        return next;
    C::SdoTransfer transfer;
    const M::Status admitted = admitStep(module, live, nowUs, transfer);
    if (!admitted)
        return admitted;
    // This fake owns a free slot and copies synchronously, with no delayed controller queue.
    if (!owner.admit(transfer, live.target.nodeGeneration, nowUs))
        return M::Status(M::Err::INVALID_CONFIG, 0, "fake owner slot unexpectedly occupied");
    return M::Ok();
}
} // namespace FieldCoreConsumer
