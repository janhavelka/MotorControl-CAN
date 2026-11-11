// SPDX-License-Identifier: MIT
#include "ProbeDevice.h"
#include "ProbeSettings.h"
namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
M::Status invalid(const char* reason) {
    return M::Status(M::Err::INVALID_CONFIG, 0, reason);
}
} // namespace
ProbeDevice::ProbeDevice(ProbeApp& app, CanBusOwner& owner, C::SdoClient& services,
                         DeviceEvidence* evidence) noexcept
    : app_(app), owner_(owner), services_(services), scan_(app, owner, services),
      evidence_(evidence) {
    attached_ =
        app_.usesOwner(owner_) && owner_.usesServices(services_) && app_.attachDevice(*this);
    readOptions_.queueTimeoutUs = readOptions_.responseTimeoutUs = 100000;
}
bool ProbeDevice::setReadOptions(const L::ReadOptions& options) noexcept {
    if (!options.queueTimeoutUs || !options.responseTimeoutUs || options.pdoNumber ||
        options.identityWidths > L::IdentityWidths::MEASURED_U8 ||
        options.countWidths > L::CountWidths::MEASURED_U16 || scan_.active() || monitorRemaining_ ||
        app_.readContext().state != M::ReadState::EMPTY || (evidence_ && evidence_->record()))
        return false;
    readOptions_ = options;
    return true;
}
L::ReadOptions ProbeDevice::readOptions(L::ReadKind kind) const noexcept {
    L::ReadOptions result = readOptions_;
    if (kind != L::ReadKind::IDENTITY)
        result.identityWidths = L::IdentityWidths::DECLARED_U16;
    if (kind != L::ReadKind::DRIVER_SOFT_LIMITS && kind != L::ReadKind::IO_OUTPUT_COMMAND)
        result.countWidths = L::CountWidths::DECLARED_U8;
    return result;
}
bool ProbeDevice::target(uint32_t slot, L::ReadTarget& output) const noexcept {
    if (slot >= C::SdoClient::CHANNELS)
        return false;
    const auto* channel = services_.channel(static_cast<uint8_t>(slot));
    const auto* node = services_.node(static_cast<uint8_t>(slot));
    if (!channel || !node || !M::sameBinding(channel->binding, node->route.binding))
        return false;
    output.channel = *channel;
    output.nodeGeneration = node->route.generation;
    return true;
}
M::Status ProbeDevice::selectSlot(uint32_t slot) noexcept {
    L::ReadTarget candidate;
    if (!attached_ || monitorRemaining_ || scan_.active() || (evidence_ && evidence_->record()) ||
        !target(slot, candidate))
        return invalid("configured binding required; discovery/capture must be released");
    return app_.select(candidate) ? M::Ok() : invalid("release retained read before selection");
}
M::Status ProbeDevice::selectAddress(uint32_t address, uint32_t subaxis) noexcept {
    if (!address || address > 127 || subaxis > UINT16_MAX)
        return invalid("CANopen address 1..127 and separate configured subaxis required");
    uint32_t found = C::SdoClient::CHANNELS;
    for (uint32_t s = 0; s < C::SdoClient::CHANNELS; ++s) {
        L::ReadTarget candidate;
        if (!target(s, candidate))
            continue;
        const auto& endpoint = candidate.channel.binding.endpoint;
        if (endpoint.protocol == M::Protocol::CANOPEN && endpoint.address == address &&
            endpoint.subaxis == subaxis) {
            if (found != C::SdoClient::CHANNELS)
                return invalid("ambiguous configured channels; select host slot");
            found = s;
        }
    }
    return selectSlot(found);
}
const C::NodeSnapshot* ProbeDevice::node() const noexcept {
    const auto* selected = app_.selected();
    if (!selected)
        return nullptr;
    for (uint8_t s = 0; s < C::SdoClient::CHANNELS; ++s) {
        L::ReadTarget current;
        if (target(s, current) && current.nodeGeneration == selected->nodeGeneration &&
            M::sameBinding(current.channel.binding, selected->channel.binding))
            return services_.node(s);
    }
    return nullptr;
}
const C::PdoRecord* ProbeDevice::pdo(uint8_t slot) const noexcept {
    const auto* record = services_.pdo(slot);
    const auto* selected = app_.selected();
    return record && selected &&
                   M::sameBinding(record->map.route.binding, selected->channel.binding)
               ? record
               : nullptr;
}
DiscoveryInterface ProbeDevice::interface() const noexcept {
    DiscoveryInterface result;
    const auto facts = app_.hostInterfaceFacts();
    if (facts.interfaceKnown && facts.bitrateKnown) {
        result.bus = facts.interfaceId;
        result.bitrate = facts.bitrateBitsPerSecond;
    }
    return result;
}
M::Status ProbeDevice::discover(DiscoveryMode mode, const uint8_t* slots, uint8_t count,
                                uint64_t now, uint64_t deadline,
                                const L::ReadOptions& options) noexcept {
    if (monitorRemaining_ || !slots || !count || count > DiscoveryPlan::CAPACITY ||
        (evidence_ && evidence_->record()))
        return invalid("explicit configured target slots required");
    DiscoveryPlan plan;
    plan.interface = interface();
    plan.mode = mode;
    plan.count = count;
    plan.options = options;
    plan.maximumAgeUs = 1000000;
    plan.maximumServices = mode == DiscoveryMode::IDENTITY ? uint8_t(9 * count) : count;
    for (uint8_t i = 0; i < count; ++i) {
        if (target(slots[i], plan.targets[i].target))
            continue;
        // Passive network evidence needs no fabricated SDO route or fresh-session premise.
        const auto* node = mode == DiscoveryMode::PASSIVE ? services_.node(slots[i]) : nullptr;
        if (!node)
            return invalid("active target needs an established channel/session; passive needs a "
                           "network route");
        plan.targets[i].target.channel.binding = node->route.binding;
        plan.targets[i].target.nodeGeneration = node->route.generation;
    }
    if (!attached_ || discoveryOperation_ == UINT32_MAX ||
        !scan_.begin(plan, plan.interface, now, deadline))
        return invalid("discovery plan/budget/interface or retained result rejected");
    scanInterface_ = plan.interface;
    ++discoveryOperation_;
    return M::Ok();
}
bool ProbeDevice::beforeService(uint64_t now) noexcept {
    if (!scan_.active())
        return true;
    const auto actual = interface();
    if (actual.protocol != scanInterface_.protocol || actual.bus != scanInterface_.bus ||
        actual.bitrate != scanInterface_.bitrate)
        return scan_.cancel(now);
    return true;
}
bool ProbeDevice::advance(uint64_t now) noexcept {
    if (scan_.active())
        return scan_.advance(interface(), now);
    if (monitorRemaining_ && app_.readContext().state == M::ReadState::EMPTY &&
        now >= monitorNextUs_)
        return startMonitorSample(now);
    return true;
}
bool ProbeDevice::requestNmt(C::NmtCommand command, bool broadcast, uint32_t request, uint64_t now,
                             uint64_t deadline, C::NmtRequestId& output) noexcept {
    const C::NodeSnapshot* selected = node();
    const DiscoveryInterface actual = interface();
    if (!attached_ || !selected || !actual.bitrate ||
        selected->route.binding.endpoint.bus != actual.bus)
        return false;
    C::NmtTransfer work;
    work.command = command;
    work.broadcast = broadcast;
    work.bus = actual.bus;
    work.target = selected->route;
    work.request = request;
    work.queueTimeoutUs = 100000;
    work.deadlineUs = deadline;
    return owner_.admitNmt(work, now, output);
}
bool ProbeDevice::setCapturePlan(const AcquisitionPlan& plan) noexcept {
    if (!evidence_ || evidence_->record())
        return false;
    capturePlan_ = plan;
    captureReady_ = true;
    return true;
}
EvidenceAdmission ProbeDevice::capture(uint64_t now) noexcept {
    if (!attached_ || !evidence_ || !captureReady_ || scan_.active() || monitorRemaining_)
        return EvidenceAdmission::INVALID;
    const EvidenceAdmission result = evidence_->request(capturePlan_, now);
    if (result == EvidenceAdmission::ACCEPTED)
        captureReady_ = false;
    return result;
}
bool ProbeDevice::diagnostic(MotorControlCANOwner::Diagnostic& output) noexcept {
    return (!evidence_ || !evidence_->record()) && owner_.takeDiagnostic(output);
}

bool ProbeDevice::startMonitorSample(uint64_t now) noexcept {
    const auto* selection = app_.selected();
    if (!selection || selection->nodeGeneration != monitorTarget_.nodeGeneration ||
        !M::sameBinding(selection->channel.binding, monitorTarget_.channel.binding)) {
        monitorRemaining_ = 0;
        monitorOutcome_ = M::ReadOutcome::INVALIDATED;
        return false;
    }
    const auto options = readOptions(L::ReadKind::STATE);
    const auto status = app_.requestOrdinaryRead(L::ReadKind::STATE, now, monitorDeadline_, options,
                                                 uint8_t(1u << unsigned(L::StateBlock::STATUS)));
    if (!status) {
        monitorRemaining_ = 0;
        monitorOutcome_ =
            now >= monitorDeadline_ ? M::ReadOutcome::DEADLINE : M::ReadOutcome::INVALIDATED;
        return false;
    }
    monitorOperation_ = app_.readContext().operation;
    monitorNextUs_ = now > UINT64_MAX - monitorInterval_ ? UINT64_MAX : now + monitorInterval_;
    return true;
}
M::Status ProbeDevice::monitor(uint64_t interval, uint8_t count, uint64_t now,
                               uint64_t deadline) noexcept {
    if (!attached_ || !interval || !count || count > 16 || monitorRemaining_ || scan_.active() ||
        (evidence_ && evidence_->record()) || app_.readContext().state != M::ReadState::EMPTY)
        return invalid("finite monitor requires empty read storage and 1..16 samples");
    const auto options = readOptions(L::ReadKind::STATE);
    const auto status = app_.requestOrdinaryRead(L::ReadKind::STATE, now, deadline, options,
                                                 uint8_t(1u << unsigned(L::StateBlock::STATUS)));
    if (!status)
        return status;
    monitorRemaining_ = count;
    monitorSamples_ = 0;
    monitorOutcome_ = M::ReadOutcome::NONE;
    monitorOperation_ = app_.readContext().operation;
    monitorTarget_ = *app_.selected();
    monitorInterval_ = interval;
    monitorDeadline_ = deadline;
    monitorNextUs_ = now > UINT64_MAX - interval ? UINT64_MAX : now + interval;
    return M::Ok();
}
bool ProbeDevice::nextMonitor(uint64_t now) noexcept {
    if (!monitorRemaining_)
        return false;
    const auto& read = app_.readContext();
    if (read.operation != monitorOperation_ || read.state != M::ReadState::SUCCEEDED) {
        monitorRemaining_ = 0;
        monitorOutcome_ =
            read.operation == monitorOperation_ ? read.outcome : M::ReadOutcome::INVALIDATED;
        return false;
    }
    ++monitorSamples_;
    if (--monitorRemaining_ == 0) {
        monitorOutcome_ = M::ReadOutcome::SUCCESS;
        return false;
    }
    if (now >= monitorDeadline_) {
        monitorRemaining_ = 0;
        monitorOutcome_ = M::ReadOutcome::DEADLINE;
        return false;
    }
    if (!app_.releaseRead()) {
        monitorRemaining_ = 0;
        monitorOutcome_ = M::ReadOutcome::INVALIDATED;
        return false;
    }
    return true;
}
bool ProbeDevice::stopMonitor(uint64_t now) noexcept {
    if (!monitorRemaining_)
        return false;
    monitorRemaining_ = 0;
    monitorOutcome_ = M::ReadOutcome::CANCELLED;
    if (app_.readContext().operation == monitorOperation_ &&
        app_.readContext().state == M::ReadState::ACTIVE)
        app_.cancelRead(now);
    return true;
}

M::Status ProbeDevice::captureMotionProfile(const L::AxisReservation& reservation,
                                            const L::ReadContext& identity,
                                            const L::DriverContext& parameters, uint32_t id,
                                            uint32_t fixture, uint32_t firmware, uint64_t now,
                                            uint64_t age) noexcept {
    if (!app_.selected() || scan_.active() || monitorRemaining_)
        return invalid("explicit stationary acquisition reservation required");
    const auto status = captureConfiguration(record_, reservation, *app_.selected(), identity,
                                             parameters, id, fixture, firmware, now, age);
    if (status) {
        recordPresent_ = true;
        restore_ = nullptr;
    }
    return status;
}
bool ProbeDevice::setRestorePlan(const ConfigurationRestoreContext& prepared) noexcept {
    if (!recordPresent_ || prepared.original.recordId != record_.recordId ||
        prepared.original.fixtureRecord != record_.fixtureRecord ||
        prepared.original.firmwareRecord != record_.firmwareRecord ||
        prepared.original.model != record_.model ||
        prepared.original.capturedUs != record_.capturedUs ||
        !M::sameBinding(prepared.original.configuration.target.channel.binding,
                        record_.configuration.target.channel.binding) ||
        prepared.original.configuration.positionSpeedRpm !=
            record_.configuration.positionSpeedRpm ||
        prepared.original.configuration.rampUpTimeMs != record_.configuration.rampUpTimeMs ||
        prepared.original.configuration.rampDownTimeMs != record_.configuration.rampDownTimeMs ||
        prepared.original.identity.vendorId != record_.identity.vendorId ||
        prepared.original.identity.productCode != record_.identity.productCode ||
        prepared.original.identity.revision != record_.identity.revision ||
        prepared.original.identity.modelCode != record_.identity.modelCode ||
        prepared.original.identity.firmwareCode != record_.identity.firmwareCode)
        return false;
    restore_ = &prepared;
    return true;
}
M::Status ProbeDevice::restoreMotionProfile(ProbeSettings& settings, uint64_t now,
                                            uint64_t deadline) noexcept {
    if (!restore_ || !recordPresent_)
        return invalid("fresh externally reviewed restoration and original snapshot required");
    const auto status = settings.requestRestore(*restore_, now, deadline);
    if (status)
        restore_ = nullptr;
    return status;
}
} // namespace MotorControlCANExample
