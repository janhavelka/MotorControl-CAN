// SPDX-License-Identifier: MIT
/** @file api_ownership_test.cpp @brief Native boundary tests; only test reporting performs I/O. */
#include "CanBusOwner.h"
#include "Operations.h"
#include <cstdio>
#include <cstdlib>
using namespace MotorControlCAN;
using namespace MotorControlCAN::ProofProfile;
using MotorControlCANExample::CanBusOwner;
using MotorControlCANExample::RequestId;
using MotorControlCANExample::SequenceId;
using MotorControlCANExample::Observation;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (false)

namespace {
Binding binding(uint32_t axis = 1) {
    Binding b; b.logicalAxisId = axis; b.bindingGeneration = b.configurationGeneration = 1;
    b.endpoint.profile = 1; b.endpoint.address = 1; b.endpoint.subaxis = static_cast<uint16_t>(axis - 1);
    return b;
}
OperationToken operation(uint32_t value = 1) { OperationToken t; t.value = value; return t; }
ServiceWork work(const Binding& b, uint32_t step = 1, uint64_t deadline = 10000) {
    ServiceWork w; w.binding = b; w.operation = operation(); w.step.value = step;
    w.index = IDENTITY_OBJECT; w.deadlineUs = deadline; return w;
}
Frame response(uint16_t index = IDENTITY_OBJECT, uint8_t command = 0x43) {
    Frame f; f.id = 0x581; f.dlc = f.payloadLength = 8; f.data[0] = command;
    f.data[1] = static_cast<uint8_t>(index); f.data[2] = static_cast<uint8_t>(index >> 8);
    f.data[4] = 0x78; f.data[5] = 0x56; f.data[6] = 0x34; f.data[7] = 0x12; return f;
}
void completeRead(CanBusOwner& owner, uint64_t now) {
    CHECK(owner.handoff(now)); CHECK(owner.receive(response(), now + 1));
}
void flows() {
    CanBusOwner owner;
    ReadContext read;
    CHECK(prepareIdentity(binding(), operation(), 0, 10000, read));
    ServiceWork yielded; bool ready = false;
    CHECK(nextRead(read, 0, yielded, ready) && ready);
    SequenceId sequence; RequestId request;
    CHECK(owner.beginSequence(read.binding, read.operation, sequence));
    CHECK(owner.admit(sequence, yielded, 0, request));
    yielded.index = 0xFFFF; yielded.binding.endpoint.address = 99; // Caller storage is borrowed only.
    CHECK(!owner.release(request) && !owner.releaseSequence(sequence));
    CHECK(owner.service(0) && owner.handoff(1));
    Frame tx; CHECK(owner.lastTransmitted(tx) && tx.data[2] == 0x20 && tx.data[1] == 0);
    Frame heartbeat; heartbeat.id = 0x701; heartbeat.dlc = heartbeat.payloadLength = 1; heartbeat.data[0] = 5;
    CHECK(owner.receive(heartbeat, 2)); heartbeat.data[0] = 127;
    Observation event; CHECK(owner.takeObservation(event) && event.frame.data[0] == 5 && event.observedUs == 2);
    CHECK(!owner.result(request));
    CHECK(owner.receive(response(), 3));
    const ServiceEvidence* evidence = owner.result(request);
    CHECK(evidence && evidence->queued && evidence->transmissionPossible && evidence->protocolAcknowledged);
    CHECK(advanceRead(read, *evidence));
    Identity identity; CHECK(getIdentity(read, binding(), identity) && identity.value == 0x12345678);
    CHECK(owner.service(4) && owner.result(request) == evidence);
    CHECK(owner.release(request) && !owner.result(request));
    CHECK(getIdentity(read, binding(), identity) && identity.value == 0x12345678);
    CHECK(owner.releaseSequence(sequence));

    ProofAxisConfig config; config.binding = binding(); config.capabilities.position = true; config.originKnown = true; config.originSource = ScaleSource::ASSUMED;
    PositionRequest intent; intent.value = Rational(70000); intent.configurationGeneration = 1;
    MoveContext move;
    CHECK(prepareMoveAbsolute(intent, config, operation(2), 5, 10000, move));
    CHECK(nextMove(move, 5, yielded, ready) && ready);
    CHECK(owner.beginSequence(move.binding, move.operation, sequence));
    CHECK(owner.admit(sequence, yielded, 5, request));
    yielded.payload.bytes[0] = 0; intent.value = Rational(1); config.nativeMaximum = 1;
    CHECK(owner.handoff(6)); CHECK(owner.lastTransmitted(tx));
    CHECK(tx.data[0] == 0x23 && tx.data[4] == 0x70 && tx.data[5] == 0x11 && tx.data[6] == 1);
    CHECK(owner.receive(response(TARGET_OBJECT, 0x60), 7));
    CHECK(advanceMove(move, *owner.result(request)));
    CHECK(move.outcome == ActionOutcome::ACKNOWLEDGED && move.execution == ActionExecution::ACKNOWLEDGED);
    CHECK(move.completion == ActionCompletion::NOT_OBSERVED && !move.uncertain);
    CHECK(owner.release(request) && owner.releaseSequence(sequence));
    CHECK(move.evidence.protocolAcknowledged && move.nativeTarget == 70000);
}
void schedulingAndRetention() {
    CanBusOwner owner;
    SequenceId a, b, rejected; RequestId first, second, urgent, rejectedRequest;
    CHECK(owner.beginSequence(binding(1), operation(), a));
    CHECK(owner.beginSequence(binding(2), operation(), b));
    CHECK(!owner.beginSequence(binding(3), operation(), rejected));
    CHECK(owner.admit(a, work(binding(1)), 0, first));
    CHECK(owner.admit(b, work(binding(2)), 0, second));
    CHECK(!owner.admit(a, work(binding(1), 2), 0, rejectedRequest));
    CHECK(owner.admitUrgent(a, work(binding(1), 2), 0, urgent));
    CHECK(!owner.admitUrgent(b, work(binding(2), 2), 0, rejectedRequest));
    completeRead(owner, 1); // Earlier same-axis ordinary step precedes urgent.
    CHECK(owner.result(first) && !owner.result(second) && !owner.result(urgent));
    const ServiceEvidence retained = *owner.result(first);
    completeRead(owner, 3); // Urgent gets the next settled channel boundary.
    CHECK(owner.result(urgent) && !owner.result(second));
    CHECK(owner.release(urgent));
    CHECK(owner.admitUrgent(a, work(binding(1), 3), 5, urgent));
    completeRead(owner, 5); // One urgent burst only: ordinary axis B now wins.
    CHECK(owner.result(second) && !owner.result(urgent));
    CHECK(owner.result(first)->payload.bytes[0] == retained.payload.bytes[0]);
    CHECK(owner.release(first));
    CHECK(!owner.admit(a, work(binding(1), 1), 7, rejectedRequest)); // No replay after release.
    CHECK(!owner.admit(a, work(binding(1), 4, 20000), 7, rejectedRequest)); // No renewed operation lease.
    completeRead(owner, 7);
    CHECK(owner.release(urgent) && owner.release(second));
    CHECK(owner.releaseSequence(a) && owner.releaseSequence(b));
    CHECK(!owner.release(first));
    CHECK(owner.beginSequence(binding(1), operation(), a));
    CanBusOwner other;
    CHECK(!other.releaseSequence(a));
}
void cancellationAndDeadlines() {
    for (unsigned transmitted = 0; transmitted < 2; ++transmitted) {
        CanBusOwner owner; SequenceId seq; RequestId id;
        CHECK(owner.beginSequence(binding(), operation(), seq));
        CHECK(owner.admit(seq, work(binding(), 1, 100), 0, id));
        CHECK(owner.service(1));
        if (transmitted) CHECK(owner.handoff(2));
        CHECK(owner.cancel(id, 3));
        CHECK(owner.result(id)->kind == EventKind::CANCEL);
        CHECK(owner.result(id)->transmissionPossible == (transmitted != 0));
        CHECK(owner.channelUnresolved() == (transmitted != 0));
        CHECK(!owner.handoff(4));
        CHECK(owner.release(id));
        CHECK(owner.admit(seq, work(binding(), 2, 100), 5, id) == (transmitted == 0));
        CHECK(owner.receive(response(), 6)); // Failed read cannot be reopened by a late response.
        CHECK(!owner.result(id));
    }
    CanBusOwner owner; SequenceId a, b; RequestId first, pending;
    CHECK(owner.beginSequence(binding(1), operation(), a));
    CHECK(owner.beginSequence(binding(2), operation(), b));
    CHECK(owner.admit(a, work(binding(1), 1, 100), 0, first));
    CHECK(owner.admit(b, work(binding(2), 1, 50), 0, pending));
    CHECK(owner.service(0));
    CHECK(owner.service(50));
    CHECK(owner.result(pending)->kind == EventKind::DEADLINE && !owner.result(pending)->transmissionPossible);
    CHECK(!owner.handoff(100)); // Absolute deadline is checked at last TX boundary.
    CHECK(owner.result(first)->kind == EventKind::DEADLINE && !owner.channelUnresolved());
    CHECK(!owner.service(99));

    CanBusOwner sent; SequenceId seq; RequestId id;
    CHECK(sent.beginSequence(binding(), operation(), seq));
    CHECK(sent.admit(seq, work(binding(), 1, 100), 0, id));
    CHECK(sent.handoff(1)); CHECK(sent.receive(response(), 100));
    CHECK(sent.result(id)->kind == EventKind::DEADLINE && sent.channelUnresolved());
    CHECK(!sent.result(id)->protocolAcknowledged);

    CanBusOwner edge; CHECK(edge.beginSequence(binding(), operation(), seq));
    CHECK(!edge.admit(seq, work(binding(), 1, UINT64_MAX - 1), UINT64_MAX - 1000, id));
    CHECK(edge.admit(seq, work(binding(), 1, 100), 0, id)); // Failed admission did not mutate clock or step.
}
void rawAbortRepresentation() {
    // These vendor bits are not C++ enum values. Exercise both C API outputs
    // under UBSan without narrowing or interpreting the wire abort word.
    const uint32_t codes[] = {0x80000000u, 0xDE345678u, 0xFFFFFFFFu};
    for (unsigned mode = 0; mode < 2; ++mode) {
        for (uint32_t code : codes) {
            CanBusOwner owner; SequenceId sequence; RequestId request;
            CHECK(owner.beginSequence(binding(), operation(), sequence));
            ServiceWork item = work(binding());
            if (mode) { item.kind = WorkKind::SDO_DOWNLOAD; item.mayChange = true; item.payload.length = 1; }
            CHECK(owner.admit(sequence, item, 0, request) && owner.handoff(1));
            Frame abort = response(IDENTITY_OBJECT, 0x80);
            for (unsigned byte = 0; byte < 4; ++byte)
                abort.data[4 + byte] = static_cast<uint8_t>(code >> (8 * byte));
            CHECK(owner.receive(abort, 2));
            CHECK(owner.result(request)->kind == EventKind::PROTOCOL_REJECTED);
            CHECK(owner.result(request)->abortValid && owner.result(request)->abortCode == code);
        }
    }
}
void eventsAndRejection() {
    CanBusOwner owner; SequenceId seq; RequestId id;
    CHECK(owner.beginSequence(binding(), operation(), seq));
    ServiceWork raw = work(binding()); raw.kind = WorkKind::RAW_FRAME;
    CHECK(!owner.admit(seq, raw, 0, id));
    CHECK(owner.admit(seq, work(binding()), 0, id));
    CHECK(owner.handoff(1));
    CHECK(owner.receive(response(IDENTITY_OBJECT + 1), 2) && !owner.result(id));
    Frame abort = response(IDENTITY_OBJECT, 0x80); abort.data[7] = 0xDE;
    CHECK(owner.receive(abort, 3));
    CHECK(owner.result(id)->abortValid && owner.result(id)->abortCode == 0xDE345678);
    CHECK(owner.channelUnresolved());
    Frame emcy; emcy.id = 0x81; emcy.dlc = emcy.payloadLength = 8;
    for (unsigned i = 0; i < 5; ++i) { emcy.data[0] = static_cast<uint8_t>(i); CHECK(owner.receive(emcy, 4 + i)); }
    CHECK(owner.droppedObservations() == 1);
    Observation event;
    for (unsigned i = 0; i < 4; ++i) CHECK(owner.takeObservation(event) && event.frame.data[0] == i);
    CHECK(!owner.takeObservation(event) && event.frame.data[0] == 3);

    CanBusOwner reboot; CHECK(reboot.beginSequence(binding(), operation(), seq));
    CHECK(reboot.admit(seq, work(binding()), 0, id)); CHECK(reboot.handoff(1));
    for (unsigned i = 0; i < 4; ++i) CHECK(reboot.receive(emcy, 1));
    Frame boot; boot.id = 0x701; boot.dlc = boot.payloadLength = 1;
    CHECK(reboot.receive(boot, 2));
    CHECK(reboot.droppedObservations() == 1 && reboot.channelUnresolved() && reboot.result(id)->transmissionPossible);
    CHECK(reboot.result(id)->kind == EventKind::TRANSPORT_FAILURE);
    CHECK(!reboot.handoff(3));
}
}
int main() {
    flows(); schedulingAndRetention(); cancellationAndDeadlines(); rawAbortRepresentation(); eventsAndRejection();
    std::printf("API ownership proof passed; sizeof(core work)=%zu, evidence=%zu, owner=%zu bytes\n",
                sizeof(ServiceWork), sizeof(ServiceEvidence), sizeof(CanBusOwner));
}
