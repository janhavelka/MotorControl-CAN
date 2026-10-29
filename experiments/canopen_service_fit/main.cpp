// SPDX-License-Identifier: MIT
#include "fit_proof.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace CanopenFit;

// Active in Release builds too; success is never manufactured by NDEBUG.
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "line %d failed: %s\n", __LINE__, #condition); \
    std::exit(1); } } while (false)

static FitFrame reply(uint8_t command, uint16_t index = 0x2000) {
    FitFrame frame = {};
    frame.ident = 0x581;
    frame.DLC = 8;
    frame.data[0] = command;
    frame.data[1] = static_cast<uint8_t>(index);
    frame.data[2] = static_cast<uint8_t>(index >> 8);
    return frame;
}

static RequestId begin(CanBusOwner& owner, bool download, const uint8_t* bytes = nullptr,
                       size_t size = 0, uint16_t timeoutMs = 20, size_t expectedUploadSize = 4) {
    RequestId id = {};
    CHECK(owner.admit(download, 0x2000, 0, bytes, size, 0, timeoutMs, id, expectedUploadSize));
    CHECK(owner.result(id) == nullptr);
    return id;
}

static void send(CanBusOwner& owner, uint64_t nowUs, uint8_t command) {
    owner.service(nowUs);
    CHECK(owner.controller().queueOccupied);
    CHECK(owner.controller().queued.data[0] == command);
    CHECK(owner.handoff(nowUs));
}

static void readAndEvents() {
    CanBusOwner owner;
    const RequestId id = begin(owner, false);
    send(owner, 0, 0x40); // Correct upload request, independent of returned width.
    FitFrame heartbeat = {};
    heartbeat.ident = 0x701;
    heartbeat.DLC = 1;
    heartbeat.data[0] = 5;
    owner.receive(heartbeat, 100);
    heartbeat.data[0] = 127;
    CHECK(owner.heartbeats() == 1 && owner.lastEvent().data[0] == 5);
    CHECK(owner.result(id) == nullptr);
    FitFrame event = {};
    event.ident = 0x81;
    event.DLC = 7;
    owner.receive(event, 200); // Wrong EMCY envelope cannot become an event.
    CHECK(owner.emergencies() == 0);
    event.DLC = 8;
    event.data[7] = 0xA5;
    owner.receive(event, 200);
    event.ident = 0x181;
    event.DLC = 4;
    owner.receive(event, 300);
    CHECK(owner.emergencies() == 1 && owner.pdos() == 1);
    FitFrame response = reply(0x43);
    response.data[4] = 0x78;
    response.data[5] = 0x56;
    response.data[6] = 0x34;
    response.data[7] = 0x12;
    owner.receive(response, 400);
    const Completion* completed = owner.result(id);
    CHECK(completed != nullptr && completed->outcome == Outcome::Success);
    CHECK(completed->size == 4 && completed->data[0] == 0x78 && completed->data[3] == 0x12);
    response.data[4] = 0;
    owner.service(99999);
    CHECK(owner.result(id) == completed && completed->data[0] == 0x78);
    RequestId refused = {nullptr, 777};
    CHECK(!owner.admit(false, 0x2000, 0, nullptr, 0, 99999, 20, refused));
    CHECK(refused.owner == nullptr && refused.serial == 777);
    CanBusOwner foreign;
    CHECK(foreign.result(id) == nullptr && !foreign.release(id));
    CHECK(owner.release(id) && owner.result(id) == nullptr && !owner.release(id));
    CHECK(owner.admit(false, 0x2000, 0, nullptr, 0, 99999, 20, refused));
    CHECK(refused.serial != id.serial && owner.result(id) == nullptr);
}

static void writeCopyAndBackpressure() {
    CanBusOwner owner;
    uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    RequestId id = begin(owner, true, payload, sizeof(payload));
    std::memset(payload, 0, sizeof(payload));
    owner.service(0);
    CHECK(owner.controller().queued.data[0] == 0x23);
    CHECK(owner.controller().queued.data[4] == 0x11 && owner.controller().queued.data[7] == 0x44);
    owner.service(100); // Buffer full waits; no second accepted frame or retry.
    CHECK(owner.controller().accepted == 1 && !owner.controller().sendFault);
    CHECK(owner.handoff(100));
    owner.receive(reply(0x60), 200);
    CHECK(owner.result(id)->outcome == Outcome::Success);
    CHECK(!owner.result(id)->executionUnknown && owner.controller().handedOff == 1);

    CanBusOwner unsolicited;
    const RequestId unsent = begin(unsolicited, true, payload, sizeof(payload));
    unsolicited.service(0);
    unsolicited.receive(reply(0x60), 100); // Pre-handoff traffic disproves session freshness.
    CHECK(unsolicited.result(unsent)->outcome == Outcome::Transport);
    CHECK(!unsolicited.result(unsent)->executionUnknown);
    CHECK(!unsolicited.handoff(100) && !unsolicited.controller().queueOccupied);
    CHECK(unsolicited.release(unsent));
    RequestId blocked = {nullptr, 0};
    CHECK(!unsolicited.admit(true, 0x2000, 0, payload, sizeof(payload), 200, 20, blocked));
}

static void segmented() {
    CanBusOwner writer;
    uint8_t payload[10] = {1,2,3,4,5,6,7,8,9,10};
    const RequestId writeId = begin(writer, true, payload, sizeof(payload));
    std::memset(payload, 0, sizeof(payload));
    send(writer, 0, 0x21);
    writer.receive(reply(0x60), 10);
    CHECK(writer.controller().queued.data[0] == 0x00);
    CHECK(writer.controller().queued.data[1] == 1 && writer.controller().queued.data[7] == 7);
    CHECK(writer.handoff(10));
    writer.receive(reply(0x20), 20);
    CHECK(writer.controller().queued.data[0] == 0x19);
    CHECK(writer.controller().queued.data[1] == 8 && writer.controller().queued.data[3] == 10);
    CHECK(writer.handoff(20));
    writer.receive(reply(0x30), 30);
    CHECK(writer.result(writeId)->outcome == Outcome::Success);

    CanBusOwner reader;
    const RequestId readId = begin(reader, false, nullptr, 0, 20, 10);
    send(reader, 0, 0x40);
    FitFrame initial = reply(0x41);
    initial.data[4] = 10;
    reader.receive(initial, 10);
    CHECK(reader.controller().queued.data[0] == 0x60 && reader.handoff(10));
    FitFrame segment = reply(0);
    for (uint8_t i = 1; i <= 7; ++i) segment.data[i] = i;
    reader.receive(segment, 20);
    CHECK(reader.controller().queued.data[0] == 0x70 && reader.handoff(20));
    segment = reply(0x19);
    segment.data[1] = 8; segment.data[2] = 9; segment.data[3] = 10;
    reader.receive(segment, 30);
    const Completion* completed = reader.result(readId);
    CHECK(completed != nullptr && completed->outcome == Outcome::Success && completed->size == 10);
    for (uint8_t i = 0; i < 10; ++i) CHECK(completed->data[i] == i + 1);
}

static void rejectionAndDeadline() {
    CanBusOwner owner;
    const RequestId id = begin(owner, false);
    send(owner, 0, 0x40);
    FitFrame abort = reply(0x80, 0x2001);
    abort.data[4] = 0; abort.data[5] = 0; abort.data[6] = 2; abort.data[7] = 6;
    owner.receive(abort, 10); // Wrong object abort must not terminate this request.
    CHECK(owner.result(id) == nullptr);
    abort.data[1] = 0;
    owner.receive(abort, 20);
    CHECK(owner.result(id)->outcome == Outcome::ServerAbort);
    CHECK(owner.result(id)->abortCode == 0x06020000);
    owner.service(30);
    CHECK(owner.result(id)->outcome == Outcome::ServerAbort); // No idle-success overwrite.

    const uint8_t badCommands[] = {0x60, 0x23, 0x2B, 0x2F};
    for (size_t i = 0; i < sizeof(badCommands); ++i) {
        CanBusOwner invalid;
        RequestId invalidId = begin(invalid, false);
        send(invalid, 0, 0x40);
        invalid.receive(reply(badCommands[i]), 1);
        CHECK(invalid.result(invalidId)->outcome == Outcome::ClientAbort);
        CHECK(invalid.result(invalidId)->abortCode == CO_SDO_AB_CMD);
    }
    // The third-party example's 0x43/0x4B/0x4F request bytes are response
    // commands; all actual upload requests above remain the correct 0x40.
    const uint8_t goodResponses[] = {0x43, 0x4B, 0x4F};
    const size_t widths[] = {4, 2, 1};
    for (size_t i = 0; i < sizeof(goodResponses); ++i) {
        CanBusOwner read;
        RequestId readId = begin(read, false, nullptr, 0, 20, widths[i]);
        send(read, 0, 0x40);
        read.receive(reply(goodResponses[i]), 1);
        CHECK(read.result(readId)->outcome == Outcome::Success);
        CHECK(read.result(readId)->size == widths[i]);
    }
    CanBusOwner expired;
    const RequestId expiredId = begin(expired, false, nullptr, 0, 1);
    send(expired, 0, 0x40);
    expired.receive(reply(0x43), 1000); // At deadline: owner wins before stack RX reset.
    CHECK(expired.result(expiredId)->outcome == Outcome::Timeout);
    CHECK(expired.result(expiredId)->abortCode == CO_SDO_AB_TIMEOUT);
    CanBusOwner blocked;
    const RequestId blockedId = begin(blocked, false, nullptr, 0, 1);
    blocked.service(0);
    blocked.service(1000); // TX never handed off; full buffer cannot postpone closure.
    CHECK(blocked.result(blockedId)->outcome == Outcome::Timeout);
    CHECK(!blocked.handoff(1000));
    CanBusOwner expiredHandoff;
    const RequestId expiredHandoffId = begin(expiredHandoff, false, nullptr, 0, 1);
    expiredHandoff.service(0);
    CHECK(!expiredHandoff.handoff(1000)); // No intervening service is required.
    CHECK(expiredHandoff.result(expiredHandoffId)->outcome == Outcome::Timeout);
    CHECK(expiredHandoff.controller().handedOff == 0);
    CanBusOwner writeTimeout;
    const uint8_t payload[] = {1, 2};
    const RequestId writeTimeoutId = begin(writeTimeout, true, payload, sizeof(payload), 1);
    send(writeTimeout, 0, 0x2B);
    writeTimeout.service(1000);
    CHECK(writeTimeout.result(writeTimeoutId)->outcome == Outcome::Timeout);
    CHECK(writeTimeout.result(writeTimeoutId)->executionUnknown);
    const uint32_t accepted = writeTimeout.controller().accepted;
    writeTimeout.service(2000);
    CHECK(writeTimeout.controller().accepted == accepted);
}

static void cancelAndFault() {
    const uint8_t payload[] = {1,2};
    for (unsigned phase = 0; phase < 3; ++phase) {
        CanBusOwner owner;
        const RequestId id = begin(owner, true, payload, sizeof(payload));
        if (phase != 0) owner.service(0);
        if (phase == 2) CHECK(owner.handoff(0));
        CHECK(owner.cancel(id));
        CHECK(owner.result(id)->outcome == Outcome::Cancelled);
        CHECK(owner.result(id)->executionUnknown == (phase == 2));
        CHECK(!owner.handoff(0));
        CHECK(!owner.cancel(id));
        const uint32_t accepted = owner.controller().accepted;
        owner.service(100);
        CHECK(owner.controller().accepted == accepted);
        CHECK(owner.release(id));
        RequestId next = {};
        CHECK(owner.admit(true, 0x2000, 0, payload, sizeof(payload), 100, 1, next) == (phase != 2));
        if (phase == 2) {
            owner.receive(reply(0x60), 101); // Late reply never reconciles or auto-replays.
            CHECK(owner.result(next) == nullptr && owner.controller().accepted == accepted);
        }
    }
    CanBusOwner fault;
    const RequestId id = begin(fault, true, payload, sizeof(payload));
    fault.refuseSend();
    fault.service(0);
    CHECK(fault.result(id)->outcome == Outcome::Transport);
    CHECK(!fault.result(id)->executionUnknown && fault.controller().accepted == 0);
    CHECK(fault.controller().sendFault);
    CHECK(fault.release(id));
    RequestId next = {nullptr, 99};
    CHECK(!fault.admit(true, 0x2000, 0, payload, sizeof(payload), 1, 1, next));
    CHECK(next.owner == nullptr && next.serial == 99);
}

static void boundsAndFraming() {
    CanBusOwner owner;
    RequestId id = {nullptr, 123};
    const uint8_t payload[33] = {};
    CHECK(!owner.admit(true, 0x2000, 0, payload, sizeof(payload), 0, 1, id));
    CHECK(!owner.admit(true, 0x2000, 0, nullptr, 1, 0, 1, id));
    CHECK(!owner.admit(false, 0x2000, 0, nullptr, 0, 0, 0, id));
    CHECK(id.owner == nullptr && id.serial == 123 && owner.controller().accepted == 0);
    id = begin(owner, false);
    send(owner, 0, 0x40);
    FitFrame malformed = reply(0x43);
    malformed.extended = true;
    owner.receive(malformed, 10);
    malformed.extended = false; malformed.rtr = true;
    owner.receive(malformed, 20);
    malformed.rtr = false; malformed.DLC = 7;
    owner.receive(malformed, 30);
    malformed.DLC = 8; malformed.ident = 0x582;
    owner.receive(malformed, 40);
    CHECK(owner.result(id) == nullptr);
    owner.receive(reply(0x43), 50);
    CHECK(owner.result(id)->outcome == Outcome::Success);
}

int main() {
    readAndEvents();
    writeCopyAndBackpressure();
    segmented();
    rejectionAndDeadline();
    cancelAndFault();
    boundsAndFraming();
    std::printf("PASS: actual CANopenNode SDO client, copied payload/result, single RX owner, events, failures, retirement\n");
    std::printf("ABI bits: pointer=%zu size_t=%zu bool_t=%zu\n", sizeof(void*) * 8, sizeof(size_t) * 8, sizeof(bool_t) * 8);
    std::printf("sizeof bytes: Frame=%zu CO_CANmodule_t=%zu CO_SDOclient_t=%zu Completion=%zu CanBusOwner=%zu\n",
                sizeof(FitFrame), sizeof(CO_CANmodule_t), sizeof(CO_SDOclient_t), sizeof(Completion), sizeof(CanBusOwner));
    return 0;
}
