/** @file ProbeAppLocal.cpp
 * @brief Offline host facts and pure coordinate previews; no owner or platform I/O.
 * SPDX-License-Identifier: MIT
 */
// Host field dispatch follows MotorControl-RS ProbeApp.cpp, copyright (c) 2026 THYMOS Solution s.r.o., Jan Havelka.
#include "ProbeApp.h"

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace {
M::Status invalid(const char* message) {
    return M::Status(M::Err::INVALID_CONFIG, static_cast<int32_t>(M::AxisError::INVALID_ARGUMENT),
                     message);
}
M::AxisConfig offlineAxis() {
    M::AxisConfig config;
    config.binding.logicalAxisId = 1;
    config.binding.endpoint.protocol = M::Protocol::PROPRIETARY;
    config.binding.endpoint.profile = 1;
    config.binding.bindingGeneration = config.binding.configurationGeneration = 1;
    return config;
}
bool integer(const M::Rational& value) {
    return value.denominator == 1;
}
M::Status applyField(const HostAxisCommand& command, M::AxisConfig& candidate) {
    M::UnitScale* scale = nullptr;
    switch (command.field) {
    case HostAxisField::COMMAND_SCALE:
        scale = &candidate.units.commandStepsPerMotorTurn;
        break;
    case HostAxisField::GEAR:
        scale = &candidate.units.motorTurnsPerLoadTurn;
        break;
    case HostAxisField::FULL_STEP_SCALE:
        scale = &candidate.units.fullStepsPerMotorTurn;
        break;
    case HostAxisField::LEAD:
        scale = &candidate.units.millimetresPerLoadTurn;
        break;
    case HostAxisField::ENCODER_SCALE:
        scale = &candidate.units.encoder.countsPerUnit;
        break;
    case HostAxisField::ENCODER_ID:
        if (!integer(command.value) || command.value.numerator < 0 ||
            uint64_t(command.value.numerator) > UINT32_MAX)
            return invalid("encoder identity must be an exact uint32");
        candidate.units.encoder.sourceId = static_cast<uint32_t>(command.value.numerator);
        break;
    case HostAxisField::ENCODER_BASIS:
        candidate.units.encoder.basis = command.encoderBasis;
        break;
    case HostAxisField::ENCODER_POLARITY:
    case HostAxisField::POLARITY:
        if (!integer(command.value) ||
            (command.value.numerator != -1 && command.value.numerator != 1))
            return invalid("polarity must be exactly -1 or 1");
        if (command.field == HostAxisField::POLARITY)
            candidate.units.commandPolarity = static_cast<int8_t>(command.value.numerator);
        else
            candidate.units.encoder.polarity = static_cast<int8_t>(command.value.numerator);
        break;
    case HostAxisField::NATIVE_LIMITS:
    case HostAxisField::SOFT_LIMITS:
        if (!command.clear && (!integer(command.value) || !integer(command.secondValue)))
            return invalid("limits must be exact native integers");
        if (command.field == HostAxisField::NATIVE_LIMITS) {
            candidate.nativeMinimum = command.value.numerator;
            candidate.nativeMaximum = command.secondValue.numerator;
        } else {
            candidate.softLimitsKnown = !command.clear;
            candidate.softMinimum = command.value.numerator;
            candidate.softMaximum = command.secondValue.numerator;
        }
        break;
    case HostAxisField::RELATIVE_BASES:
        if (!integer(command.value) || command.value.numerator < 0 || command.value.numerator > 7)
            return invalid("relative bases must be an exact bitmask 0..7");
        candidate.supportedRelativeBases = static_cast<uint8_t>(command.value.numerator);
        break;
    default:
        return invalid("unknown host axis field");
    }
    if (command.clear && !scale && command.field != HostAxisField::SOFT_LIMITS)
        return invalid("only optional scales and soft limits can be cleared");
    if (scale) {
        if (command.clear)
            *scale = M::UnitScale();
        else {
            if (command.value.numerator <= 0 || uint64_t(command.value.numerator) > UINT32_MAX ||
                !command.value.denominator || command.value.denominator > UINT32_MAX)
                return invalid("scale must be a positive uint32 rational");
            *scale = M::UnitScale(static_cast<uint32_t>(command.value.numerator),
                                  static_cast<uint32_t>(command.value.denominator),
                                  M::ScaleSource::ASSUMED);
        }
    }
    return M::validateAxisConfig(candidate);
}
} // namespace

const M::AxisConfig* ProbeApp::axisConfig() const noexcept {
    return hostAxisConfigured_ ? &hostAxis_ : nullptr;
}
const M::AxisReference* ProbeApp::axisReference() const noexcept {
    return hostAxisConfigured_ && hostReference_.nativeKnown ? &hostReference_ : nullptr;
}
M::Status ProbeApp::initializeOfflineAxis() noexcept {
    return establishHostAxis(offlineAxis());
}
M::Status ProbeApp::establishHostAxis(const M::AxisConfig& config) noexcept {
    if (hostAxisConfigured_)
        return invalid("host axis is already established");
    M::Status status = M::validateAxisConfig(config);
    if (!status)
        return status;
    if (config.originKnown || config.encoderOriginKnown || config.softLimitsKnown)
        return invalid("initial host axis cannot inject origins or soft limits");
    hostAxis_ = config;
    hostReference_ = M::AxisReference();
    hostAxisConfigured_ = true;
    return M::Ok();
}
M::Status ProbeApp::configureHostAxis(const M::AxisConfig& candidate,
                                      const M::AxisReference& evidence) noexcept {
    return M::configureAxis(hostAxis_, candidate, evidence, &hostReference_);
}
M::Status ProbeApp::setHostAxisReference(const M::AxisReference& reference) noexcept {
    // A witness may be outside soft limits: the core permits a later inward move.
    M::AxisConfig validation = hostAxis_;
    validation.softLimitsKnown = false;
    M::PositionRequest request;
    request.value = M::Rational(reference.nativePosition, 1);
    request.configurationGeneration = validation.binding.configurationGeneration;
    request.referencePolicy = M::ReferencePolicy::REQUIRED;
    request.basis = reference.basis;
    M::PreparedTarget checked;
    M::Status status = M::preparePosition(request, validation, &reference, checked);
    if (!status)
        return status;
    if (hostReference_.nativeKnown && reference.nowUs < hostReference_.nowUs)
        return invalid("host reference time moved backwards");
    hostReference_ = reference;
    return M::Ok();
}
M::Status ProbeApp::setHostAxisOrigin(int64_t native, const M::AxisReference& evidence) noexcept {
    M::Status status = M::setAxisOrigin(hostAxis_, native, evidence);
    if (!status)
        return status;
    hostReference_ = evidence;
    hostReference_.binding = hostAxis_.binding;
    hostReference_.referenceGeneration = hostAxis_.referenceGeneration;
    return M::Ok();
}
M::Status ProbeApp::previewPosition(const M::PositionRequest& request, uint64_t nowUs,
                                    M::PreparedTarget& output) const noexcept {
    const bool needsReference = request.relative || request.wrapped ||
                                request.referencePolicy == M::ReferencePolicy::REQUIRED;
    M::AxisReference reference = hostReference_;
    if (needsReference && reference.nativeKnown) {
        if (nowUs < reference.nowUs)
            return invalid("preview time moved backwards");
        reference.nowUs = nowUs;
    }
    return M::preparePosition(
        request, hostAxis_, needsReference && reference.nativeKnown ? &reference : nullptr, output);
}
M::Status ProbeApp::executeHostAxis(const HostAxisCommand& command, uint64_t nowUs,
                                    HostAxisView& output) noexcept {
    HostAxisView view;
    switch (command.kind) {
    case HostAxisCommandKind::QUERY:
        break;
    case HostAxisCommandKind::CONFIGURE: {
        M::AxisConfig candidate = hostAxisConfigured_ ? hostAxis_ : offlineAxis();
        if (hostAxisConfigured_) {
            // An offline reconstruction retires every previous coordinate relation.
            // It is never presented as stationary live reconfiguration.
            M::AxisReference discarded;
            const M::Status retired = M::invalidateAxisReference(candidate, discarded);
            if (!retired)
                return retired;
        }
        const M::Status status = applyField(command, candidate);
        if (!status)
            return status;
        hostAxis_ = candidate;
        hostReference_ = M::AxisReference();
        hostAxisConfigured_ = true;
        break;
    }
    case HostAxisCommandKind::ORIGIN: {
        if (!integer(command.value))
            return invalid("host origin must be an exact native integer");
        M::AxisReference reference = hostReference_;
        if (reference.nativeKnown && nowUs < reference.nowUs)
            return invalid("origin time moved backwards");
        reference.nowUs = nowUs;
        const M::Status status = setHostAxisOrigin(command.value.numerator, reference);
        if (!status)
            return status;
        break;
    }
    case HostAxisCommandKind::PREPARE: {
        M::PositionRequest request = command.position;
        request.configurationGeneration = hostAxis_.binding.configurationGeneration;
        const M::Status status = previewPosition(request, nowUs, view.prepared);
        if (!status)
            return status;
        break;
    }
    default:
        return invalid("unknown host axis operation");
    }
    view.configured = hostAxisConfigured_;
    view.configuration = hostAxis_;
    view.referenceKnown = axisReference() != nullptr;
    output = view;
    return M::Ok();
}
bool ProbeApp::setHostInterfaceFacts(const HostInterfaceFacts& facts) noexcept {
    if (facts.bitrateKnown && !facts.bitrateBitsPerSecond)
        return false;
    hostInterface_ = facts;
    if (!facts.interfaceKnown)
        hostInterface_.interfaceId = 0;
    if (!facts.bitrateKnown)
        hostInterface_.bitrateBitsPerSecond = 0;
    return true;
}
bool ProbeApp::setHostWiring(uint8_t terminal, HostWiringUse use) noexcept {
    if (terminal >= 10 || use > HostWiringUse::CONNECTED)
        return false;
    hostWiring_[terminal] = use;
    return true;
}
bool ProbeApp::hostWiring(uint8_t terminal, HostWiringUse& output) const noexcept {
    if (terminal >= 10)
        return false;
    output = hostWiring_[terminal];
    return true;
}
} // namespace MotorControlCANExample
