// SPDX-License-Identifier: MIT
// Real selected-stack encoding plus a checked pure adapter boundary; no device I/O.
#include <MotorControlCAN/canopen/Sdo.h>
#include "fit_proof.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace {
void check(bool valid, int line) {
    if (!valid) { std::fprintf(stderr, "sdo_adapter_test.cpp:%d\n", line); std::abort(); }
}
#define CHECK(value) check(static_cast<bool>(value), __LINE__)

C::SdoExpectation expected(bool download, uint8_t width, uint16_t index) {
    C::SdoExpectation e;
    e.channel.binding.logicalAxisId = 9; e.channel.binding.endpoint.profile = 1;
    e.channel.binding.endpoint.address = 1;
    e.channel.binding.bindingGeneration = e.channel.binding.configurationGeneration = 1;
    e.channel.clientToServerId = 0x601; e.channel.serverToClientId = 0x581;
    e.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
    e.width = width; e.index = index;
    return e;
}
M::Frame frame(const FitFrame& native) {
    M::Frame result;
    result.id = native.ident; result.dlc = result.payloadLength = native.DLC;
    result.extended = native.extended; result.rtr = native.rtr;
    CHECK(native.DLC <= 8 && !native.rtr);
    std::memcpy(result.data, native.data, native.DLC);
    return result;
}
FitFrame native(const M::Frame& value) {
    CHECK(M::validateFrame(value) && !value.fd && !value.extended && !value.rtr);
    FitFrame result = {};
    result.ident = value.id; result.DLC = value.payloadLength;
    std::memcpy(result.data, value.data, value.payloadLength);
    return result;
}
// Mirrors the production boundary's responsibility: guard before one engine,
// leaving its raw unsized/segmented payload semantics untouched.
M::Status receive(CanopenFit::CanBusOwner& owner, const M::Frame& f,
    const C::SdoExpectation& e, C::SdoReply& reply) {
    const M::Status s = C::checkReply(f, e, reply);
    if (s) owner.receive(native(f), 1);
    return s;
}
void actualRequests() {
    const uint8_t commands[] = {0x2f, 0x2b, 0x27, 0x23};
    const uint8_t payload[] = {0x64, 0, 0, 0};
    for (uint8_t width = 1; width <= 4; ++width) {
        CanopenFit::CanBusOwner owner; CanopenFit::RequestId id = {};
        const C::SdoExpectation e = expected(true, width, width == 4 ? 0x609a : 0x2000);
        CHECK(owner.admit(true, e.index, e.subindex, payload, width, 0, 100, id));
        owner.service(0);
        CHECK(owner.controller().queueOccupied);
        M::Frame request = frame(owner.controller().queued);
        CHECK(C::checkRequest(request, e));
        const uint8_t prefix[] = {commands[width - 1], uint8_t(width == 4 ? 0x9a : 0),
            uint8_t(width == 4 ? 0x60 : 0x20), 0};
        CHECK(std::memcmp(request.data, prefix, sizeof prefix) == 0);
        CHECK(std::memcmp(request.data + 4, payload, width) == 0);
        for (uint8_t i = width; i < 4; ++i) CHECK(request.data[4 + i] == 0);
        // Widths1-3 use a synthetic object; width4 also checks the 609A:00 vector.
        C::SdoExpectation reviewed = e; reviewed.width = 4;
        CHECK(C::checkRequest(request, reviewed).isOk() == (width == 4));
    }
    for (uint8_t width = 1; width <= 4; ++width) {
        CanopenFit::CanBusOwner owner; CanopenFit::RequestId id = {};
        const C::SdoExpectation e = expected(false, width, width == 2 ? 0x6041 : 0x2000);
        CHECK(owner.admit(false, e.index, 0, nullptr, 0, 0, 100, id, width)); owner.service(0);
        const M::Frame request = frame(owner.controller().queued);
        const uint8_t literal[] = {0x40, uint8_t(width == 2 ? 0x41 : 0),
            uint8_t(width == 2 ? 0x60 : 0x20), 0, 0, 0, 0, 0};
        CHECK(std::memcmp(request.data, literal, sizeof literal) == 0);
        CHECK(C::checkRequest(request, e)); // Width never changes upload command.
    }
}
void guardedReplies() {
    for (unsigned unsized = 0; unsized < 2; ++unsized) {
        CanopenFit::CanBusOwner owner; CanopenFit::RequestId id = {};
        const C::SdoExpectation e = expected(false, 2, 0x6041);
        CHECK(owner.admit(false, e.index, 0, nullptr, 0, 0, 100, id, e.width)); CHECK(owner.handoff(0));
        M::Frame response; response.id = 0x581; response.dlc = response.payloadLength = 8;
        const uint8_t literal[] = {uint8_t(unsized ? 0x42 : 0x4b), 0x41, 0x60, 0, 0x34, 0x12, 0xa5, 0x5a};
        std::memcpy(response.data, literal, sizeof literal);
        C::SdoReply checked;
        response.data[0] = 0x46; // The stack alone tolerates noncanonical nn; the guard refuses it.
        CHECK(!receive(owner, response, e, checked)); CHECK(!owner.result(id));
        response.data[0] = literal[0];
        CHECK(receive(owner, response, e, checked));
        uint32_t value = 0;
        CHECK(C::readWord(checked.data, checked.length, e.width, value) && value == 0x1234);
        const CanopenFit::Completion* result = owner.result(id);
        CHECK(result && result->outcome == CanopenFit::Outcome::Success);
        // An unsized expedited reply consumes the explicit reviewed width;
        // opaque padding never becomes additional object data.
        CHECK(result->size == e.width);
    }
    CanopenFit::CanBusOwner owner; CanopenFit::RequestId id = {};
    const C::SdoExpectation e = expected(false, 2, 0x6041);
    CHECK(owner.admit(false, e.index, 0, nullptr, 0, 0, 100, id, e.width)); CHECK(owner.handoff(0));
    M::Frame abort; abort.id = 0x581; abort.dlc = abort.payloadLength = 8;
    const uint8_t literal[] = {0x80, 0x41, 0x60, 0, 0x67, 0x45, 0x23, 0xf1};
    std::memcpy(abort.data, literal, sizeof literal); C::SdoReply reply;
    ++abort.data[1]; CHECK(!receive(owner, abort, e, reply)); CHECK(!owner.result(id));
    --abort.data[1]; CHECK(receive(owner, abort, e, reply));
    CHECK(reply.kind == C::SdoReplyKind::ABORT && reply.abortCode == 0xf1234567u);
    CHECK(owner.result(id) && owner.result(id)->outcome == CanopenFit::Outcome::ServerAbort);
    CHECK(owner.result(id)->abortCode == 0xf1234567u);
}
} // namespace
int main() { actualRequests(); guardedReplies(); return 0; }
