// SPDX-License-Identifier: MIT
#include <MotorControlCAN/MotorControlCAN.h>
#include <MotorControlCAN/canopen/SdoTransfer.h>
#include <MotorControlCAN/canopen/Network.h>
#include <MotorControlCAN/profiles/lichuan_c/Objects.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>
#include <MotorControlCAN/profiles/lichuan_c/DriverSettings.h>
#include <cstring>

int main() {
    using namespace MotorControlCAN;
    CANOPEN::SdoTransfer service;
    CANOPEN::SdoCompletion terminal;
    if (service.size != 0 || terminal.protocolAcknowledged || terminal.transmissionPossible) return 20;
    Binding binding;
    binding.logicalAxisId = 9;
    binding.endpoint.protocol = Protocol::PROPRIETARY;
    binding.endpoint.profile = 1;
    binding.endpoint.address = 500;
    binding.endpoint.subaxis = 2;
    binding.bindingGeneration = binding.configurationGeneration = 1;
    CANOPEN::NetworkRoute network;
    network.binding = binding; network.binding.endpoint.protocol = Protocol::CANOPEN;
    network.binding.endpoint.address = 5; network.heartbeatId = 0x705; network.emcyId = 0x85;
    Frame heartbeat; heartbeat.id = 0x705; heartbeat.dlc = heartbeat.payloadLength = 1; heartbeat.data[0] = 5;
    CANOPEN::NetworkEvent event;
    Frame nmt;
    if (!CANOPEN::makeNmtRequest(CANOPEN::NmtCommand::PRE_OPERATIONAL, 0, nmt) ||
        nmt.id != 0 || nmt.payloadLength != 2 || nmt.data[0] != 0x80 || nmt.data[1] != 0) return 22;
    if (!CANOPEN::checkNetworkEvent(heartbeat, network, network.binding, network.generation, event) ||
        event.kind != CANOPEN::NetworkEventKind::HEARTBEAT || event.state != CANOPEN::NmtState::OPERATIONAL) return 21;
    Frame frame;
    frame.id = 0x117;
    frame.extended = true;
    frame.dlc = frame.payloadLength = 4;
    if (!validateBinding(binding) || !sameBinding(binding, binding) || !validateFrame(frame)) return 1;
    frame.fd = true;
    if (validateFrame(frame).code != Err::UNSUPPORTED) return 2;
    if (std::strcmp(errToString(Err::OK), "OK") != 0 || VERSION_CODE == 0) return 3;
    Rational exact;
    if (!parseExactNumber("9007199254740993", exact) || exact.numerator != INT64_C(9007199254740993)) return 4;
    UnitConfig units;
    Rational converted;
    if (!convertVelocity(Rational(-70000), VelocityUnit(PositionUnit::TURNS, TimeUnit::MINUTE),
        VelocityUnit(PositionUnit::DEGREES, TimeUnit::SECOND), units, converted) ||
        converted.numerator != -420000 || converted.denominator != 1) return 5;
    FloatQuantization native;
    if (!quantizeFloat(Rational(16777217), Rational(1), native) || native.absoluteErrorBound != 1) return 6;
    AxisConfig config; config.binding = binding;
    config.units.commandStepsPerMotorTurn = UnitScale(4096, 1, ScaleSource::ASSUMED);
    config.units.motorTurnsPerLoadTurn = UnitScale(3, 2, ScaleSource::ASSUMED);
    config.units.millimetresPerLoadTurn = UnitScale(20, 1, ScaleSource::ASSUMED);
    config.originKnown = true; config.originSource = ScaleSource::ASSUMED;
    PositionRequest request; request.configurationGeneration = 1;
    request.value = Rational(5); request.unit = PositionUnit::MILLIMETRES; request.frame = CoordinateFrame::LOAD;
    PreparedTarget target;
    if (!preparePosition(request, config, nullptr, target) || target.endpointNative != 1536) return 7;
    if (!validatePreparedTarget(target, config, nullptr, 0)) return 8;
    CANOPEN::SdoExpectation sdo;
    sdo.channel.binding = binding; sdo.channel.binding.endpoint.protocol = Protocol::CANOPEN;
    sdo.channel.binding.endpoint.address = 5;
    sdo.channel.clientToServerId = 0x605; sdo.channel.serverToClientId = 0x585;
    sdo.index = 0x6041; sdo.width = 2;
    Frame response; response.id = 0x585; response.dlc = response.payloadLength = 8;
    response.data[0] = 0x4b; response.data[1] = 0x41; response.data[2] = 0x60;
    response.data[4] = 0x34; response.data[5] = 0x12;
    CANOPEN::SdoReply reply; uint32_t word = 0;
    if (!CANOPEN::checkReply(response, sdo, reply) ||
        !CANOPEN::readWord(reply.data, reply.length, sdo.width, word) || word != 0x1234) return 9;
    if (LICHUAN_C::OBJECT_1009_00.widthBytes != 2 ||
        LICHUAN_C::OBJECT_2300_00.writePolicy != LICHUAN_C::WritePolicy::QUALIFICATION_REQUIRED)
        return 10;
    CANOPEN::PdoMap pdoMap;
    CANOPEN::PdoRoute pdoRoute;
    pdoRoute.binding = network.binding; pdoRoute.generation = 1; pdoRoute.cobId = 0x285;
    if (!LICHUAN_C::pdoTemplate(CANOPEN::PdoDirection::TPDO, 2, pdoMap) ||
        !CANOPEN::validatePdoMap(pdoMap, LICHUAN_C::pdoProfile(network.binding.endpoint.profile), pdoRoute.layout)) return 23;
    Frame pdoFrame; pdoFrame.id = 0x285; pdoFrame.dlc = pdoFrame.payloadLength = 8;
    pdoFrame.data[0] = 0xfe; pdoFrame.data[1] = pdoFrame.data[2] = pdoFrame.data[3] = 0xff;
    pdoFrame.data[7] = 0x80;
    CANOPEN::PdoSample pdoSample;
    if (!CANOPEN::decodePdo(pdoFrame, pdoRoute, pdoSample) ||
        pdoSample.values[0].native != -2 || pdoSample.values[1].native != INT32_MIN) return 24;
    LICHUAN_C::ReadTarget readTarget;
    readTarget.channel = sdo.channel; readTarget.nodeGeneration = 1;
    LICHUAN_C::ReadOptions readOptions;
    readOptions.queueTimeoutUs = readOptions.responseTimeoutUs = 20;
    LICHUAN_C::ReadContext reads;
    if (!LICHUAN_C::prepareConfig(reads,readTarget,1,0,100,readOptions)) return 25;
    const uint32_t synthetic[] = {0x80,0x85,0,2,0x605,0x585};
    for (unsigned i = 0; i < 6; ++i) {
        LICHUAN_C::PreparedRead prepared;
        if (!LICHUAN_C::nextRead(reads,i,prepared) || !prepared.ready ||
            prepared.transfer.direction != CANOPEN::SdoDirection::UPLOAD ||
            prepared.transfer.readEffect != CANOPEN::SdoReadEffect::UNQUALIFIED) return 26;
        CANOPEN::SdoCompletion completed;
        completed.transfer = prepared.transfer; completed.nodeGeneration = 1;
        completed.admittedUs = completed.completedUs = i;
        completed.outcome = CANOPEN::SdoOutcome::SUCCESS;
        completed.queued = completed.transmissionPossible = completed.protocolAcknowledged = completed.effectPossible = true;
        completed.size = prepared.transfer.size;
        for (unsigned j = 0; j < completed.size; ++j) completed.data[j] = uint8_t(synthetic[i] >> (8*j));
        if (!LICHUAN_C::advanceRead(reads,completed,i)) return 27;
    }
    LICHUAN_C::ConfigObservation observedConfig;
    if (!LICHUAN_C::getConfig(reads,readTarget,5,10,observedConfig) ||
        observedConfig.producerTimeMs != 0 || observedConfig.clientToServer != 0x605 ||
        observedConfig.window.skewUs != 5) return 28;
    LICHUAN_C::DriverContext driver;
    if (!LICHUAN_C::prepareDriverRead(driver, readTarget, 2, 10, 100, readOptions,
                                    LICHUAN_C::DriverGroup::SOFT_LIMITS)) return 29;
    const uint32_t limitWords[] = {10, 2, 0x80000000u, 0x7fffffffu};
    for (unsigned i = 0; i < 4; ++i) {
        LICHUAN_C::PreparedRead prepared;
        if (!LICHUAN_C::nextDriver(driver, 10+i, prepared) || !prepared.ready ||
            prepared.transfer.direction != CANOPEN::SdoDirection::UPLOAD) return 30;
        CANOPEN::SdoCompletion completed;
        completed.transfer = prepared.transfer; completed.nodeGeneration = 1;
        completed.admittedUs = completed.completedUs = 10+i;
        completed.outcome = CANOPEN::SdoOutcome::SUCCESS;
        completed.queued = completed.transmissionPossible = completed.protocolAcknowledged = completed.effectPossible = true;
        completed.size = prepared.transfer.size;
        for (unsigned j = 0; j < 4 && j < completed.size; ++j)
            completed.data[j] = uint8_t(limitWords[i] >> (8*j));
        if (!LICHUAN_C::advanceDriver(driver, completed, 10+i)) return 31;
    }
    LICHUAN_C::DriverObservation observedDriver;
    if (!LICHUAN_C::getDriver(driver, readTarget, 13, 20, observedDriver) ||
        observedDriver.softLimitSelectorRaw != 10 || observedDriver.minimumNative != INT32_MIN ||
        observedDriver.maximumNative != INT32_MAX || observedDriver.window.skewUs != 3) return 32;
    return 0;
}
