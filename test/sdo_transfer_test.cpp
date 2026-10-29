// SPDX-License-Identifier: MIT
// Fake controller/server vectors derived from the pinned CO_SDOserver.h wire tables.
#include "SdoClient.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;

namespace {
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)

C::SdoChannel route(uint8_t node = 1) {
    C::SdoChannel result;
    result.binding.logicalAxisId = 100u + node;
    result.binding.endpoint.profile = 1;
    result.binding.endpoint.address = node;
    result.binding.bindingGeneration = 2;
    result.binding.configurationGeneration = 3;
    result.clientToServerId = 0x600u + node;
    result.serverToClientId = 0x580u + node;
    return result;
}
C::SdoTransfer work(uint8_t size = 2, bool download = false, uint8_t node = 1) {
    // This synthetic server varies width and access at the same index to test
    // protocol forms. These transfers grant no Lichuan object permission.
    C::SdoTransfer result;
    result.channel = route(node);
    result.operation = 7; result.request = 11; result.step = 13;
    result.index = 0x6041; result.subindex = 0;
    result.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
    result.size = size;
    result.deadlineUs = 100000;
    result.queueTimeoutUs = 1000;
    result.responseTimeoutUs = 1000;
    for (unsigned i = 0; i < sizeof result.data; ++i) result.data[i] = static_cast<uint8_t>(i + 1);
    return result;
}
M::Frame reply(uint8_t command, uint8_t node = 1, uint16_t index = 0x6041) {
    M::Frame result;
    result.id = 0x580u + node;
    result.dlc = result.payloadLength = 8;
    result.data[0] = command;
    result.data[1] = static_cast<uint8_t>(index);
    result.data[2] = static_cast<uint8_t>(index >> 8);
    return result;
}
M::Frame segment(uint8_t command, const uint8_t* data, unsigned count, uint8_t node = 1) {
    CHECK(count <= 7);
    M::Frame result = reply(command, node);
    // Segment bytes 1..7 are data/padding, never an index/subindex envelope.
    std::memset(result.data + 1, 0xa5, 7);
    if (count) std::memcpy(result.data + 1, data, count);
    return result;
}
bool sameFrame(const M::Frame& a, const M::Frame& b) {
    return a.id == b.id && a.dlc == b.dlc && a.payloadLength == b.payloadLength &&
        a.extended == b.extended && a.rtr == b.rtr && a.fd == b.fd && a.brs == b.brs &&
        a.esi == b.esi && std::memcmp(a.data, b.data, sizeof a.data) == 0;
}
bool sameId(C::SdoRequestId a, C::SdoRequestId b) {
    return a.owner == b.owner && a.channel == b.channel && a.generation == b.generation;
}
bool sameTx(C::SdoTxId a, C::SdoTxId b) {
    return sameId(a.request, b.request) && a.frame == b.frame;
}
C::SdoRequestId admit(C::SdoClient& client, const C::SdoTransfer& transfer, uint8_t slot = 0,
                      uint64_t now = 0) {
    C::SdoRequestId id;
    CHECK(client.admit(slot, transfer, now, id));
    CHECK(id.owner == &client && id.channel == slot && id.generation != 0);
    CHECK(!client.result(id));
    CHECK(!client.release(id));
    return id;
}
M::Frame take(C::SdoClient& client, uint64_t now, uint8_t slot = 0, C::SdoTxId* saved = nullptr) {
    M::Frame frame;
    C::SdoTxId id;
    CHECK(client.nextTx(slot, now, frame, id));
    CHECK(M::validateFrame(frame));
    CHECK(frame.dlc == 8 && frame.payloadLength == 8 && !frame.extended && !frame.rtr);
    if (saved) *saved = id;
    return frame;
}
void terminal(const C::SdoClient& client, C::SdoRequestId id, C::SdoOutcome outcome) {
    const C::SdoCompletion* result = client.result(id);
    CHECK(result && result->outcome == outcome);
    CHECK(result->transfer.operation == 7 && result->transfer.request == 11 && result->transfer.step == 13);
}

void admissionAndRoutes() {
    C::SdoClient client;
    C::SdoChannel invalid = route(); invalid.clientToServerId = 0x800;
    CHECK(!client.configure(0, invalid, true, true));
    CHECK(!client.configure(0, route(), true, false));
    CHECK(!client.configure(0, route(), false, false));
    CHECK(!client.configure(C::SdoClient::CHANNELS, route(), true, true));
    CHECK(client.configure(0, route(), true, true));
    CHECK(!client.configure(0, route(), true, true));
    C::SdoChannel duplicate = route(2);
    duplicate.serverToClientId = route().serverToClientId;
    CHECK(!client.configure(1, duplicate, true, true));
    duplicate = route(2); duplicate.clientToServerId = route().serverToClientId;
    CHECK(!client.configure(1, duplicate, true, true));
    duplicate = route(2); duplicate.serverToClientId = route().clientToServerId;
    CHECK(!client.configure(1, duplicate, true, true));
    duplicate = route(2); duplicate.clientToServerId = route().clientToServerId;
    duplicate.binding.endpoint.profile = 99; duplicate.binding.endpoint.subaxis = 71;
    ++duplicate.binding.bindingGeneration; ++duplicate.binding.configurationGeneration;
    CHECK(!client.configure(1, duplicate, true, true));
    duplicate = route(2); ++duplicate.binding.endpoint.bus;
    CHECK(!client.configure(1, duplicate, true, true));
    CHECK(client.configure(1, route(2), true, true));
    for (unsigned fault = 0; fault < 12; ++fault) {
        C::SdoTransfer bad = work();
        switch (fault) {
        case 0: bad.size = 0; break;
        case 1: bad.size = 33; break;
        case 2: bad.operation = 0; break;
        case 3: bad.request = 0; break;
        case 4: bad.step = 0; break;
        case 5: bad.queueTimeoutUs = 0; break;
        case 6: bad.responseTimeoutUs = 0; break;
        case 7: bad.deadlineUs = 0; break;
        case 8: ++bad.channel.binding.configurationGeneration; break;
        case 9: ++bad.channel.clientToServerId; break;
        case 10: bad.direction = static_cast<C::SdoDirection>(255); break;
        default: bad.readEffect = static_cast<C::SdoReadEffect>(255); break;
        }
        C::SdoRequestId sentinel; sentinel.channel = 91; sentinel.generation = 123;
        const C::SdoRequestId before = sentinel;
        CHECK(!client.admit(0, bad, 0, sentinel)); CHECK(sameId(sentinel, before));
        CHECK(client.port(0)->accepted == 0);
    }
    C::SdoTransfer tooLarge = work(33, true);
    C::SdoRequestId sentinel;
    CHECK(!client.admit(0, tooLarge, 0, sentinel)); CHECK(client.port(0)->accepted == 0);
    const C::SdoRequestId first = admit(client, work());
    CHECK(!client.admit(0, work(), 0, sentinel));
    const C::SdoRequestId second = admit(client, work(2, false, 2), 1);
    CHECK(!client.admit(1, work(2, false, 2), 0, sentinel));
    C::SdoClient foreign;
    CHECK(!foreign.result(first) && !foreign.release(first) && !foreign.cancel(first, 0));
    CHECK(!client.release(second));
    M::Frame output = reply(0xa5); C::SdoTxId tx; tx.frame = 91;
    const M::Frame original = output; const C::SdoTxId oldTx = tx;
    CHECK(!client.nextTx(99, 0, output, tx));
    CHECK(sameFrame(output, original) && sameTx(tx, oldTx));
}

void expeditedTransfers() {
    const uint8_t uploadCommands[] = {0x4f, 0x4b, 0x47, 0x43};
    const uint8_t downloadCommands[] = {0x2f, 0x2b, 0x27, 0x23};
    for (uint8_t width = 1; width <= 4; ++width) {
        for (unsigned unsized = 0; unsized < 2; ++unsized) {
            C::SdoClient client; CHECK(client.configure(0, route(), true, true));
            const C::SdoRequestId id = admit(client, work(width));
            C::SdoTxId tx; const M::Frame request = take(client, 0, 0, &tx);
            const uint8_t literal[] = {0x40, 0x41, 0x60, 0, 0, 0, 0, 0};
            CHECK(std::memcmp(request.data, literal, sizeof literal) == 0);
            CHECK(!client.result(id));
            if (!unsized) CHECK(client.confirmTx(tx, 1));
            CHECK(!client.result(id)); // CAN ACK is not SDO confirmation.
            M::Frame response = reply(unsized ? 0x42 : uploadCommands[width - 1]);
            const uint8_t bytes[] = {0x80, 0xff, 0x34, 0xf1};
            std::memcpy(response.data + 4, bytes, sizeof bytes);
            CHECK(client.receive(response, 2)); terminal(client, id, C::SdoOutcome::SUCCESS);
            const C::SdoCompletion saved = *client.result(id);
            CHECK(saved.queued && saved.transmissionPossible && saved.protocolAcknowledged);
            CHECK(saved.transmissionConfirmed == !unsized);
            CHECK(saved.size == width && std::memcmp(saved.data, bytes, width) == 0);
            CHECK(saved.replyValid && sameFrame(saved.reply, response));
            CHECK(saved.effectPossible && !saved.effectUncertain && !client.unresolved(0));
            CHECK(client.service(3000)); CHECK(!client.confirmTx(tx, 3000));
            CHECK(client.result(id)->outcome == saved.outcome && client.result(id)->completedUs == saved.completedUs);
            CHECK(sameFrame(client.result(id)->reply, saved.reply));
            CHECK(client.release(id) && !client.result(id) && !client.release(id));
        }
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(width, true);
        transfer.index = width == 4 ? 0x609a : 0x2000;
        const C::SdoRequestId id = admit(client, transfer);
        std::memset(transfer.data, 0, sizeof transfer.data);
        M::Frame request = take(client, 0);
        CHECK(request.data[0] == downloadCommands[width - 1]);
        CHECK(request.data[1] == (width == 4 ? 0x9a : 0) && request.data[2] == (width == 4 ? 0x60 : 0x20));
        for (unsigned i = 0; i < width; ++i) CHECK(request.data[4 + i] == i + 1);
        for (unsigned i = width; i < 4; ++i) CHECK(request.data[4 + i] == 0);
        M::Frame ack = reply(0x60, 1, width == 4 ? 0x609a : 0x2000);
        ack.data[4] = 0xa5; ack.data[7] = 0x5a; // Receive padding is opaque.
        CHECK(client.receive(ack, 1)); terminal(client, id, C::SdoOutcome::SUCCESS);
        CHECK(client.result(id)->protocolAcknowledged && client.result(id)->size == 0);
        CHECK(client.result(id)->effectPossible && !client.result(id)->effectUncertain);
        CHECK(client.result(id)->transfer.data[0] == 1);
    }
    // A reply cannot qualify work that remains in retirable software staging.
    C::SdoClient client; CHECK(client.configure(0, route(), true, true));
    const C::SdoRequestId id = admit(client, work());
    CHECK(!client.receive(reply(0x4b), 0)); CHECK(!client.result(id));
    CHECK(client.port(0)->queueOccupied && client.port(0)->handedOff == 0);
    CHECK(take(client, 1).data[0] == 0x40);
    CHECK(client.receive(reply(0x4b), 2)); terminal(client, id, C::SdoOutcome::SUCCESS);
}

void segmentedUploads() {
    const uint8_t bytes[32] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
                              17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32};
    for (uint8_t size = 1; size <= 32; ++size) {
        for (unsigned indicated = 0; indicated < 2; ++indicated) {
            C::SdoClient client; CHECK(client.configure(0, route(), true, true));
            const C::SdoRequestId id = admit(client, work(size));
            CHECK(take(client, 0).data[0] == 0x40);
            M::Frame initial = reply(indicated ? 0x41 : 0x40);
            initial.data[4] = indicated ? size : 0xa5;
            initial.data[5] = indicated ? 0 : 0x5a;
            CHECK(client.receive(initial, 1)); CHECK(!client.result(id));
            unsigned offset = 0, number = 0; uint64_t now = 2;
            while (offset < size) {
                const M::Frame request = take(client, now++);
                CHECK(request.data[0] == (number % 2 == 0 ? 0x60 : 0x70));
                for (unsigned i = 1; i < 8; ++i) CHECK(request.data[i] == 0);
                const unsigned count = size - offset < 7 ? size - offset : 7;
                const bool last = offset + count == size;
                const uint8_t command = static_cast<uint8_t>((number % 2 ? 0x10 : 0) | ((7 - count) << 1) | (last ? 1 : 0));
                CHECK(client.receive(segment(command, bytes + offset, count), now++));
                offset += count; ++number;
                if (!last) CHECK(!client.result(id));
            }
            terminal(client, id, C::SdoOutcome::SUCCESS);
            CHECK(client.result(id)->size == size && std::memcmp(client.result(id)->data, bytes, size) == 0);
            CHECK(!client.unresolved(0));
        }
    }
    // Valid short nonfinal segments exercise toggling within a two-byte scalar.
    C::SdoClient client; CHECK(client.configure(0, route(), true, true));
    const C::SdoRequestId id = admit(client, work(2));
    take(client, 0); M::Frame initial = reply(0x41); initial.data[4] = 2;
    CHECK(client.receive(initial, 1)); CHECK(take(client, 2).data[0] == 0x60);
    const uint8_t low = 0x34, high = 0x12;
    CHECK(client.receive(segment(0x0c, &low, 1), 3)); CHECK(!client.result(id));
    CHECK(take(client, 4).data[0] == 0x70);
    CHECK(client.receive(segment(0x1d, &high, 1), 5));
    terminal(client, id, C::SdoOutcome::SUCCESS);
    CHECK(client.result(id)->size == 2 && client.result(id)->data[0] == 0x34 && client.result(id)->data[1] == 0x12);
}

void segmentedDownloads() {
    for (uint8_t size = 5; size <= 32; ++size) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(size, true);
        const C::SdoRequestId id = admit(client, transfer);
        std::memset(transfer.data, 0, sizeof transfer.data);
        const M::Frame initial = take(client, 0);
        CHECK(initial.data[0] == 0x21 && initial.data[4] == size);
        CHECK(initial.data[5] == 0 && initial.data[6] == 0 && initial.data[7] == 0);
        CHECK(client.receive(reply(0x60), 1)); CHECK(!client.result(id));
        unsigned offset = 0, number = 0; uint64_t now = 2;
        while (offset < size) {
            const M::Frame request = take(client, now++);
            const unsigned count = size - offset < 7 ? size - offset : 7;
            const bool last = offset + count == size;
            const uint8_t command = static_cast<uint8_t>((number % 2 ? 0x10 : 0) | ((7 - count) << 1) | (last ? 1 : 0));
            CHECK(request.data[0] == command);
            for (unsigned i = 0; i < count; ++i) CHECK(request.data[i + 1] == offset + i + 1);
            CHECK(!client.result(id));
            CHECK(client.receive(reply(number % 2 ? 0x30 : 0x20), now++));
            offset += count; ++number;
            if (!last) CHECK(!client.result(id));
        }
        terminal(client, id, C::SdoOutcome::SUCCESS);
        CHECK(client.result(id)->protocolAcknowledged && !client.result(id)->effectUncertain);
    }
}

void malformedAndAbort() {
    const uint32_t codes[] = {0, 0x06020000u, 0x80000000u, 0xf1234567u, 0xffffffffu};
    for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; ++i) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(client, work(2, true)); take(client, 0);
        M::Frame abort = reply(0x80);
        for (unsigned b = 0; b < 4; ++b) abort.data[4 + b] = static_cast<uint8_t>(codes[i] >> (8 * b));
        M::Frame wrong = abort; ++wrong.data[1];
        CHECK(!client.receive(wrong, 1)); CHECK(!client.result(id));
        CHECK(client.receive(abort, 2)); terminal(client, id, C::SdoOutcome::SERVER_ABORT);
        CHECK(client.result(id)->abortValid && client.result(id)->abortCode == codes[i]);
        CHECK(!client.result(id)->protocolAcknowledged && client.result(id)->effectUncertain);
        CHECK(client.result(id)->effectPossible);
    }
    const uint8_t badCommands[] = {0x46, 0x44, 0x4a, 0x50, 0x60};
    for (unsigned i = 0; i < sizeof badCommands; ++i) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(client, work()); take(client, 0);
        CHECK(client.receive(reply(badCommands[i]), 1));
        terminal(client, id, C::SdoOutcome::CLIENT_ABORT);
        CHECK(client.result(id)->size == 0 && client.unresolved(0));
    }
    {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(client, work()); take(client, 0);
        M::Frame wrong = reply(0x4b); ++wrong.data[1];
        CHECK(!client.receive(wrong, 1)); CHECK(!client.result(id));
        wrong = reply(0x4b); ++wrong.data[3];
        CHECK(!client.receive(wrong, 2)); CHECK(!client.result(id));
        // The server's explicit four-byte size conflicts with the reviewed U16.
        CHECK(client.receive(reply(0x43), 3)); terminal(client, id, C::SdoOutcome::CLIENT_ABORT);
        CHECK(client.result(id)->size == 0 && client.unresolved(0));
    }
    for (unsigned fault = 0; fault < 4; ++fault) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(client, work()); take(client, 0);
        M::Frame initial = reply(0x41); initial.data[4] = 2;
        CHECK(client.receive(initial, 1)); take(client, 2);
        const uint8_t bytes[] = {0x34, 0x12, 0x56};
        const uint8_t command = fault == 0 ? 0x1b : fault == 1 ? 0x0d : fault == 2 ? 0x09 : 0x21;
        CHECK(client.receive(segment(command, bytes, fault == 1 ? 1 : fault == 2 ? 3 : 2), 3));
        terminal(client, id, C::SdoOutcome::CLIENT_ABORT);
        const uint32_t expected = fault == 0 ? 0x05030000u : fault == 1 ? 0x06070013u : fault == 2 ? 0x06070012u : 0x05040001u;
        CHECK(client.result(id)->abortValid && client.result(id)->abortCode == expected);
        CHECK(client.result(id)->size == 0 && client.unresolved(0));
    }
    // Matching malformed lengths fail; unrelated frame formats are not stolen.
    C::SdoClient client; CHECK(client.configure(0, route(), true, true));
    const C::SdoRequestId id = admit(client, work()); take(client, 0);
    M::Frame frame = reply(0x4b); frame.extended = true;
    CHECK(!client.receive(frame, 1)); frame.extended = false; frame.rtr = true; frame.payloadLength = 0;
    CHECK(!client.receive(frame, 2)); frame.rtr = false; frame.payloadLength = frame.dlc = 7;
    CHECK(client.receive(frame, 3)); terminal(client, id, C::SdoOutcome::CLIENT_ABORT);
}

void deadlinesAndCancellation() {
    // Delayed first service cannot renew the queue budget fixed at admission.
    {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(2, true); transfer.queueTimeoutUs = 5;
        const C::SdoRequestId id = admit(client, transfer);
        CHECK(client.service(4)); CHECK(client.port(0)->queueOccupied);
        M::Frame output = reply(0xa5); const M::Frame before = output;
        C::SdoTxId tx; tx.frame = 91; const C::SdoTxId oldTx = tx;
        CHECK(!client.nextTx(0, 5, output, tx));
        CHECK(sameFrame(output, before) && sameTx(tx, oldTx));
        terminal(client, id, C::SdoOutcome::TIMEOUT);
        CHECK(client.result(id)->queued && !client.result(id)->transmissionPossible);
        CHECK(!client.result(id)->effectPossible && !client.result(id)->effectUncertain);
        CHECK(client.port(0)->handedOff == 0 && !client.port(0)->queueOccupied);
        CHECK(!client.unresolved(0));
    }
    // Retirable software staging never implies transmission; exact queue cutoff wins.
    {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(2, true); transfer.queueTimeoutUs = 5;
        const C::SdoRequestId id = admit(client, transfer);
        CHECK(client.service(0)); CHECK(client.port(0)->queueOccupied);
        CHECK(client.service(5)); terminal(client, id, C::SdoOutcome::TIMEOUT);
        CHECK(client.result(id)->queued && !client.result(id)->transmissionPossible && !client.result(id)->effectUncertain);
        CHECK(!client.result(id)->effectPossible);
        CHECK(!client.unresolved(0));
        M::Frame out = reply(0xa5); C::SdoTxId tx; tx.frame = 91;
        const M::Frame old = out; const C::SdoTxId oldTx = tx;
        CHECK(!client.nextTx(0, 5, out, tx)); CHECK(sameFrame(out, old) && sameTx(tx, oldTx));
    }
    for (unsigned phase = 0; phase < 3; ++phase) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(client, work(2, true));
        if (phase) CHECK(client.service(0));
        if (phase == 2) take(client, 0);
        CHECK(client.cancel(id, 1)); terminal(client, id, C::SdoOutcome::CANCELLED);
        CHECK(client.result(id)->transmissionPossible == (phase == 2));
        CHECK(client.result(id)->effectPossible == (phase == 2));
        CHECK(client.result(id)->effectUncertain == (phase == 2));
        CHECK(client.unresolved(0) == (phase == 2));
        const uint32_t accepted = client.port(0)->accepted;
        CHECK(!client.cancel(id, 2)); CHECK(client.service(10000));
        CHECK(client.port(0)->accepted == accepted);
        CHECK(client.release(id));
        C::SdoRequestId next;
        CHECK(client.admit(0, work(2, true), 10000, next) == (phase != 2));
        if (phase == 2) CHECK(!client.receive(reply(0x60), 10001));
    }
    // Response time begins at possible TX; absent controller ACK cannot stall it.
    {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(2, true); transfer.responseTimeoutUs = 5;
        const C::SdoRequestId id = admit(client, transfer); take(client, 2);
        CHECK(client.service(6)); CHECK(!client.result(id));
        CHECK(!client.receive(reply(0x60), 7)); // Expiry closes the route before callback.
        terminal(client, id, C::SdoOutcome::TIMEOUT);
        CHECK(client.result(id)->effectUncertain && client.unresolved(0));
    }
    // Immutable admission deadline clamps delayed handoff, response and progress.
    {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(); transfer.deadlineUs = 10;
        const C::SdoRequestId id = admit(client, transfer); take(client, 8);
        CHECK(client.service(9)); CHECK(!client.result(id));
        CHECK(!client.cancel(id, 10)); terminal(client, id, C::SdoOutcome::TIMEOUT);
        CHECK(client.result(id)->completedUs == 10);
    }
    {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(); transfer.deadlineUs = 10;
        const C::SdoRequestId id = admit(client, transfer);
        M::Frame out; C::SdoTxId tx;
        CHECK(!client.nextTx(0, 10, out, tx)); terminal(client, id, C::SdoOutcome::TIMEOUT);
        CHECK(!client.result(id)->transmissionPossible && client.port(0)->handedOff == 0);
    }
    {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(10); transfer.deadlineUs = 10;
        const C::SdoRequestId id = admit(client, transfer); take(client, 0);
        M::Frame initial = reply(0x41); initial.data[4] = 10;
        CHECK(client.receive(initial, 1)); take(client, 2);
        const uint8_t bytes[] = {1,2,3,4,5,6,7,8,9,10};
        CHECK(client.receive(segment(0x00, bytes, 7), 8)); take(client, 9);
        // Progress did not renew the immutable overall deadline.
        CHECK(!client.receive(segment(0x19, bytes + 7, 3), 10));
        terminal(client, id, C::SdoOutcome::TIMEOUT);
        CHECK(client.result(id)->size == 0 && client.unresolved(0));
    }
    // Near UINT64_MAX cutoffs use subtraction/clamping; no addition may wrap.
    {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        const uint64_t maximum = std::numeric_limits<uint64_t>::max();
        C::SdoTransfer transfer = work(); transfer.deadlineUs = maximum;
        const C::SdoRequestId id = admit(client, transfer, 0, maximum - 2);
        take(client, maximum - 2); CHECK(client.service(maximum - 1)); CHECK(!client.result(id));
        CHECK(client.service(maximum)); terminal(client, id, C::SdoOutcome::TIMEOUT);
        CHECK(!client.service(maximum - 1));
    }
}

void unsupportedAndEffects() {
    for (unsigned reviewed = 0; reviewed < 2; ++reviewed) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(); transfer.allowSegmentedUpload = false;
        transfer.readEffect = reviewed ? C::SdoReadEffect::REVIEWED_NON_CHANGING : C::SdoReadEffect::UNQUALIFIED;
        const C::SdoRequestId id = admit(client, transfer); take(client, 0);
        M::Frame initial = reply(0x41); initial.data[4] = 2;
        CHECK(client.receive(initial, 1)); terminal(client, id, C::SdoOutcome::UNSUPPORTED);
        const C::SdoCompletion saved = *client.result(id);
        CHECK(saved.transmissionPossible && saved.effectUncertain == !reviewed && saved.clientAbortQueued);
        CHECK(saved.effectPossible == !reviewed);
        CHECK(saved.abortValid && saved.abortCode == 0x06010000u && client.unresolved(0));
        // Adapter-owned output storage is forbidden, even for a cleanup frame.
        M::Frame& overlapping = const_cast<M::Frame&>(client.result(id)->reply);
        C::SdoTxId unchanged; unchanged.frame = 91;
        const C::SdoTxId old = unchanged;
        CHECK(!client.nextTx(0, 2, overlapping, unchanged));
        CHECK(sameFrame(client.result(id)->reply, saved.reply) && sameTx(unchanged, old));
        CHECK(client.release(id)); // Cleanup and quarantine outlive retained-result release.
        const M::Frame abort = take(client, 2);
        const uint8_t literal[] = {0x80, 0x41, 0x60, 0, 0, 0, 1, 6};
        CHECK(std::memcmp(abort.data, literal, sizeof literal) == 0);
        C::SdoRequestId next; CHECK(!client.admit(0, work(), 3, next));
        CHECK(client.service(100000)); CHECK(client.unresolved(0));
    }
    for (unsigned overallClamps = 0; overallClamps < 2; ++overallClamps) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work(); transfer.allowSegmentedUpload = false;
        transfer.deadlineUs = overallClamps ? 3 : 100;
        transfer.queueTimeoutUs = 3;
        const C::SdoRequestId id = admit(client, transfer); take(client, 0);
        M::Frame initial = reply(0x41); initial.data[4] = 2;
        CHECK(client.receive(initial, 1)); terminal(client, id, C::SdoOutcome::UNSUPPORTED);
        CHECK(client.result(id)->clientAbortQueued && client.port(0)->queueOccupied);
        CHECK(client.release(id));
        const uint64_t expiry = overallClamps ? 3 : 4;
        M::Frame sentinel = reply(0xa5); const M::Frame old = sentinel;
        C::SdoTxId tx; tx.frame = 99; const C::SdoTxId oldTx = tx;
        CHECK(!client.nextTx(0, expiry, sentinel, tx));
        CHECK(sameFrame(sentinel, old) && sameTx(tx, oldTx));
        CHECK(!client.port(0)->queueOccupied && client.port(0)->handedOff == 1);
        CHECK(client.unresolved(0));
        C::SdoRequestId refused; CHECK(!client.admit(0, work(), expiry, refused));
    }
    for (unsigned reviewed = 0; reviewed < 2; ++reviewed) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        C::SdoTransfer transfer = work();
        transfer.readEffect = reviewed ? C::SdoReadEffect::REVIEWED_NON_CHANGING : C::SdoReadEffect::UNQUALIFIED;
        const C::SdoRequestId id = admit(client, transfer); take(client, 0);
        CHECK(client.cancel(id, 1)); terminal(client, id, C::SdoOutcome::CANCELLED);
        CHECK(client.result(id)->effectPossible == !reviewed);
        CHECK(client.result(id)->effectUncertain == !reviewed);
        CHECK(client.unresolved(0)); // Harmless effect does not establish wire freshness.
    }
    // Only an explicit reviewed non-changing bootstrap upload can confirm a route.
    C::SdoClient client; CHECK(client.configure(0, route(), false, true));
    C::SdoRequestId id;
    CHECK(!client.admit(0, work(), 0, id));
    CHECK(!client.admit(0, work(2, true), 0, id));
    C::SdoTransfer transfer = work(); transfer.readEffect = C::SdoReadEffect::REVIEWED_NON_CHANGING;
    id = admit(client, transfer); take(client, 0);
    CHECK(client.receive(reply(0x4b), 1)); terminal(client, id, C::SdoOutcome::SUCCESS);
    CHECK(!client.result(id)->effectPossible && !client.result(id)->effectUncertain);
    CHECK(client.release(id)); CHECK(client.admit(0, work(2, true), 2, id));
}

void channelsAndFailure() {
    // Explicit controller refusal/failure preserves whether a handoff ever occurred.
    for (unsigned handedOff = 0; handedOff < 2; ++handedOff) {
        C::SdoClient client; CHECK(client.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(client, work(2, true));
        CHECK(client.service(0)); CHECK(client.port(0)->queueOccupied);
        if (handedOff) take(client, 0);
        CHECK(client.transportFailure(id, 1));
        terminal(client, id, C::SdoOutcome::TRANSPORT_ERROR);
        CHECK(client.result(id)->transmissionPossible == (handedOff != 0));
        CHECK(client.result(id)->effectPossible == (handedOff != 0));
        CHECK(client.result(id)->effectUncertain == (handedOff != 0));
        CHECK(!client.result(id)->protocolAcknowledged && !client.port(0)->queueOccupied);
        CHECK(client.port(0)->handedOff == handedOff);
    }
    C::SdoClient client;
    CHECK(client.configure(0, route(), true, true) && client.configure(1, route(2), true, true));
    const C::SdoRequestId first = admit(client, work(2, true));
    const C::SdoRequestId second = admit(client, work(2, false, 2), 1);
    take(client, 0, 0); C::SdoTxId secondTx; take(client, 0, 1, &secondTx);
    M::Frame heartbeat; heartbeat.id = 0x701; heartbeat.dlc = heartbeat.payloadLength = 1; heartbeat.data[0] = 5;
    CHECK(!client.receive(heartbeat, 1)); // The same receive owner can dispatch it elsewhere.
    M::Frame unrelated = reply(0x4b, 3); CHECK(!client.receive(unrelated, 2));
    CHECK(client.cancel(first, 3)); CHECK(client.unresolved(0) && !client.unresolved(1));
    CHECK(client.receive(reply(0x4b, 2), 4)); terminal(client, second, C::SdoOutcome::SUCCESS);
    CHECK(client.result(first)->outcome == C::SdoOutcome::CANCELLED);
    const C::SdoCompletion saved = *client.result(second);
    CHECK(client.busOff(5));
    CHECK(client.unresolved(0) && client.unresolved(1));
    CHECK(client.result(second)->outcome == saved.outcome && client.result(second)->completedUs == saved.completedUs);
    CHECK(!client.confirmTx(secondTx, 6));
    CHECK(!client.busOff(4));
    for (unsigned event = 0; event < 3; ++event) {
        C::SdoClient other; CHECK(other.configure(0, route(), true, true));
        const C::SdoRequestId id = admit(other, work(2, true)); take(other, 0);
        if (event == 0) CHECK(other.busOff(1));
        else if (event == 1) CHECK(other.reboot(1, 1));
        else CHECK(other.transportFailure(id, 1));
        terminal(other, id, event == 0 ? C::SdoOutcome::BUS_OFF : event == 1 ? C::SdoOutcome::REBOOT : C::SdoOutcome::TRANSPORT_ERROR);
        CHECK(other.result(id)->effectUncertain && other.unresolved(0));
        const uint32_t accepted = other.port(0)->accepted;
        CHECK(other.release(id)); CHECK(other.service(10000));
        C::SdoRequestId refused; CHECK(!other.admit(0, work(2, true), 10000, refused));
        CHECK(other.port(0)->accepted == accepted);
    }
    // Reboot only invalidates that node, even when another node is active.
    C::SdoClient rebooted;
    CHECK(rebooted.configure(0, route(), true, true) && rebooted.configure(1, route(2), true, true));
    const C::SdoRequestId one = admit(rebooted, work());
    const C::SdoRequestId two = admit(rebooted, work(2, false, 2), 1);
    take(rebooted, 0, 0); take(rebooted, 0, 1);
    CHECK(rebooted.reboot(1, 1)); terminal(rebooted, one, C::SdoOutcome::REBOOT);
    CHECK(!rebooted.unresolved(1)); CHECK(rebooted.receive(reply(0x4b, 2), 2));
    terminal(rebooted, two, C::SdoOutcome::SUCCESS);
    CHECK(!rebooted.reboot(0, 3) && !rebooted.reboot(128, 3));

    // Synthetic CANopen routes serve the same node through two explicit channels.
    // Node reboot invalidates both; neither route derives node identity from COB-ID bits.
    C::SdoClient sameNode;
    C::SdoChannel custom = route();
    custom.binding.logicalAxisId = 1001;
    custom.binding.endpoint.channel = 1;
    custom.clientToServerId = 0x321;
    custom.serverToClientId = 0x456;
    CHECK(sameNode.configure(0, route(), true, true));
    CHECK(sameNode.configure(1, custom, true, true));
    const C::SdoRequestId primary = admit(sameNode, work());
    C::SdoTransfer customWork = work(); customWork.channel = custom;
    const C::SdoRequestId alternate = admit(sameNode, customWork, 1);
    CHECK(take(sameNode, 0, 0).id == 0x601);
    CHECK(take(sameNode, 0, 1).id == 0x321);
    CHECK(sameNode.reboot(1, 1));
    terminal(sameNode, primary, C::SdoOutcome::REBOOT);
    terminal(sameNode, alternate, C::SdoOutcome::REBOOT);
    CHECK(sameNode.unresolved(0) && sameNode.unresolved(1));
    CHECK(sameNode.result(alternate)->transfer.channel.binding.endpoint.address == 1);
    CHECK(sameNode.result(alternate)->transfer.channel.binding.endpoint.channel == 1);
    CHECK(sameNode.result(alternate)->transfer.channel.serverToClientId == 0x456);

    // A successful release permits fresh local work, but old handoff reports are stale.
    C::SdoClient reused; CHECK(reused.configure(0, route(), true, true));
    C::SdoRequestId old = admit(reused, work()); C::SdoTxId oldTx; take(reused, 0, 0, &oldTx);
    CHECK(reused.receive(reply(0x4b), 1)); CHECK(reused.release(old));
    const C::SdoRequestId fresh = admit(reused, work(), 0, 2);
    CHECK(fresh.generation != old.generation);
    CHECK(!reused.result(old) && !reused.release(old) && !reused.cancel(old, 2));
    CHECK(!reused.confirmTx(oldTx, 2));
    take(reused, 2);
    CHECK(!reused.service(1) && !reused.receive(reply(0x4b), 1));
    CHECK(!reused.result(fresh));
    CHECK(reused.receive(reply(0x4b), 3)); terminal(reused, fresh, C::SdoOutcome::SUCCESS);
}
} // namespace

int main() {
    admissionAndRoutes(); expeditedTransfers(); segmentedUploads(); segmentedDownloads();
    malformedAndAbort(); deadlinesAndCancellation(); unsupportedAndEffects(); channelsAndFailure();
    std::printf("SDO transfer vectors passed: %u checks\n", checks);
    return 0;
}
