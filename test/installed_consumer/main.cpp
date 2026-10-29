// SPDX-License-Identifier: MIT
#include <MotorControlCAN/MotorControlCAN.h>
#include <MotorControlCAN/canopen/SdoTransfer.h>
#include <MotorControlCAN/canopen/Network.h>
#include <MotorControlCAN/profiles/lichuan_c/Objects.h>
#include <MotorControlCAN/profiles/lichuan_c/Pdo.h>
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
    return 0;
}
