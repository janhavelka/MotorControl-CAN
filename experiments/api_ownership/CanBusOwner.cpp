// SPDX-License-Identifier: MIT
/** @file CanBusOwner.cpp @brief Single-table application proof around the selected client. */
#include "CanBusOwner.h"

namespace MotorControlCANExample {
namespace {
bool routeSupported(const Binding& b) {
    return validateBinding(b).isOk() && b.endpoint.protocol == Protocol::CANOPEN &&
        b.endpoint.bus == 0 && b.endpoint.profile == 1 && b.endpoint.address == 1 &&
        b.endpoint.channel == 0;
}
bool sameEndpoint(const Endpoint& a, const Endpoint& b) {
    return a.protocol == b.protocol && a.bus == b.bus && a.profile == b.profile &&
        a.address == b.address && a.channel == b.channel && a.subaxis == b.subaxis;
}
FitFrame toNative(const Frame& f) {
    FitFrame result = {};
    result.ident = f.id;
    result.DLC = f.dlc;
    result.extended = f.extended;
    result.rtr = f.rtr;
    for (unsigned i = 0; i < f.payloadLength; ++i) result.data[i] = f.data[i];
    return result;
}
Frame fromNative(const FitFrame& f) {
    Frame result;
    result.id = f.ident;
    result.dlc = result.payloadLength = f.DLC;
    result.extended = f.extended;
    result.rtr = f.rtr;
    for (unsigned i = 0; i < f.DLC; ++i) result.data[i] = f.data[i];
    return result;
}
}

bool CanBusOwner::valid(SequenceId id) const {
    return id.owner == this && id.slot < AXES && storage_.axes[id.slot].occupied &&
        id.generation == storage_.axes[id.slot].generation;
}
bool CanBusOwner::valid(RequestId id) const {
    return id.owner == this && id.slot < REQUESTS && storage_.slots[id.slot].occupied &&
        id.generation == storage_.slots[id.slot].generation;
}
bool CanBusOwner::beginSequence(const Binding& binding, OperationToken operation, SequenceId& id) {
    if (!routeSupported(binding) || !operation.value || storage_.unresolved) return false;
    int free = -1;
    for (unsigned i = 0; i < AXES; ++i) {
        const Reservation& r = storage_.axes[i];
        if (r.occupied && (r.binding.logicalAxisId == binding.logicalAxisId ||
                          sameEndpoint(r.binding.endpoint, binding.endpoint))) return false;
        if (!r.occupied && r.generation != UINT32_MAX && free < 0) free = static_cast<int>(i);
    }
    if (free < 0) return false;
    Reservation& r = storage_.axes[free];
    ++r.generation;
    r.occupied = true;
    r.binding = binding;
    r.operation = operation;
    r.lastStep = 0;
    r.deadlineUs = 0;
    id.owner = this; id.slot = static_cast<uint8_t>(free); id.generation = r.generation;
    return true;
}
bool CanBusOwner::releaseSequence(SequenceId id) {
    if (!valid(id)) return false;
    for (const Slot& s : storage_.slots) if (s.occupied && s.sequence == id.slot) return false;
    storage_.axes[id.slot].occupied = false;
    return true;
}
bool CanBusOwner::admit(SequenceId sequence, const ServiceWork& work, uint64_t nowUs, RequestId& id) {
    return admitInto(sequence, work, nowUs, id, false);
}
bool CanBusOwner::admitUrgent(SequenceId sequence, const ServiceWork& work, uint64_t nowUs, RequestId& id) {
    return admitInto(sequence, work, nowUs, id, true);
}
bool CanBusOwner::admitInto(SequenceId sequence, const ServiceWork& work, uint64_t nowUs,
                          RequestId& id, bool urgent) {
    if (!valid(sequence) || storage_.unresolved || nowUs < storage_.lastUs ||
        work.deadlineUs <= nowUs || work.deadlineUs - nowUs > 65535000ULL ||
        work.deadlineUs > UINT64_MAX - 65535000ULL || !work.step.value ||
        !sameBinding(work.binding, storage_.axes[sequence.slot].binding) ||
        work.operation.value != storage_.axes[sequence.slot].operation.value ||
        work.step.value <= storage_.axes[sequence.slot].lastStep ||
        (storage_.axes[sequence.slot].lastStep != 0 &&
         work.deadlineUs != storage_.axes[sequence.slot].deadlineUs) ||
        !routeSupported(work.binding) || work.payload.length > sizeof work.payload.bytes ||
        (work.kind != WorkKind::SDO_UPLOAD && work.kind != WorkKind::SDO_DOWNLOAD) ||
        (work.kind == WorkKind::SDO_UPLOAD && work.payload.length != 0) ||
        (work.kind == WorkKind::SDO_DOWNLOAD && work.payload.length == 0)) return false;
    const unsigned begin = urgent ? ORDINARY : 0, end = urgent ? REQUESTS : ORDINARY;
    for (unsigned i = begin; i < end; ++i) {
        Slot& s = storage_.slots[i];
        if (s.occupied || s.generation == UINT32_MAX) continue;
        ++s.generation;
        s.occupied = true; s.terminal = false; s.submitted = false;
        s.sequence = sequence.slot; s.work = work; s.evidence = ServiceEvidence();
        storage_.axes[sequence.slot].lastStep = work.step.value;
        storage_.axes[sequence.slot].deadlineUs = work.deadlineUs;
        storage_.lastUs = nowUs;
        id.owner = this; id.slot = static_cast<uint8_t>(i); id.generation = s.generation;
        return true;
    }
    return false;
}
bool CanBusOwner::eligible(unsigned slot) const {
    const Slot& candidate = storage_.slots[slot];
    if (!candidate.occupied || candidate.terminal) return false;
    for (const Slot& s : storage_.slots) {
        if (s.occupied && !s.terminal && s.sequence == candidate.sequence &&
            s.work.step.value < candidate.work.step.value) return false;
    }
    return true;
}
int CanBusOwner::select() const {
    int ordinary = -1;
    for (unsigned offset = 0; offset < AXES && ordinary < 0; ++offset) {
        unsigned axis = (storage_.nextAxis + offset) % AXES;
        for (unsigned i = 0; i < ORDINARY; ++i)
            if (eligible(i) && storage_.slots[i].sequence == axis) { ordinary = static_cast<int>(i); break; }
    }
    if (eligible(ORDINARY) && (!storage_.urgentStarted || ordinary < 0)) return ORDINARY;
    return ordinary;
}
void CanBusOwner::finish(unsigned slot, EventKind kind, uint64_t nowUs) {
    Slot& s = storage_.slots[slot];
    s.evidence.binding = s.work.binding;
    s.evidence.operation = s.work.operation;
    s.evidence.step = s.work.step;
    s.evidence.kind = kind;
    s.evidence.observedUs = nowUs;
    s.evidence.queued = true;
    s.evidence.transmissionPossible = s.submitted;
    s.terminal = true;
}
void CanBusOwner::collect(uint64_t nowUs, bool overrideCause, EventKind cause) {
    if (storage_.active < 0) return;
    const CanopenFit::Completion* terminal = storage_.channel.result(storage_.activeId);
    if (!terminal) return;
    const unsigned index = static_cast<unsigned>(storage_.active);
    Slot& s = storage_.slots[index];
    EventKind kind = EventKind::TRANSPORT_FAILURE;
    switch (terminal->outcome) {
    case CanopenFit::Outcome::Success: kind = EventKind::ACKNOWLEDGED; break;
    case CanopenFit::Outcome::ServerAbort: kind = EventKind::PROTOCOL_REJECTED; break;
    case CanopenFit::Outcome::ClientAbort: kind = EventKind::REPLY_ERROR; break;
    case CanopenFit::Outcome::Timeout: kind = EventKind::DEADLINE; break;
    case CanopenFit::Outcome::Cancelled: kind = EventKind::CANCEL; break;
    case CanopenFit::Outcome::Transport: break;
    }
    finish(index, overrideCause ? cause : kind, nowUs);
    s.evidence.transportDetail = static_cast<int32_t>(terminal->stackReturn);
    if (!overrideCause) {
        s.evidence.protocolAcknowledged = kind == EventKind::ACKNOWLEDGED;
        s.evidence.abortValid = kind == EventKind::PROTOCOL_REJECTED || kind == EventKind::REPLY_ERROR;
        s.evidence.abortCode = terminal->abortCode;
        s.evidence.payload.length = static_cast<uint8_t>(terminal->size);
        for (unsigned i = 0; i < terminal->size; ++i) s.evidence.payload.bytes[i] = terminal->data[i];
    }
    if ((s.submitted && s.evidence.kind != EventKind::ACKNOWLEDGED) ||
        terminal->outcome == CanopenFit::Outcome::Transport) storage_.unresolved = true;
    storage_.channel.release(storage_.activeId);
    storage_.active = -1;
}
bool CanBusOwner::service(uint64_t nowUs) {
    if (nowUs < storage_.lastUs) return false;
    storage_.lastUs = nowUs;
    for (unsigned i = 0; i < REQUESTS; ++i) {
        Slot& s = storage_.slots[i];
        if (!s.occupied || s.terminal || nowUs < s.work.deadlineUs) continue;
        if (storage_.active == static_cast<int>(i)) {
            storage_.channel.cancel(storage_.activeId);
            collect(nowUs, true, EventKind::DEADLINE);
        } else finish(i, EventKind::DEADLINE, nowUs);
    }
    if (storage_.active >= 0) { storage_.channel.service(nowUs); collect(nowUs); }
    if (storage_.unresolved) {
        for (unsigned i = 0; i < REQUESTS; ++i)
            if (storage_.slots[i].occupied && !storage_.slots[i].terminal)
                finish(i, EventKind::TRANSPORT_FAILURE, nowUs);
        return true;
    }
    if (storage_.active >= 0) return true;
    const int chosen = select();
    if (chosen < 0) return true;
    Slot& s = storage_.slots[chosen];
    const bool write = s.work.kind == WorkKind::SDO_DOWNLOAD;
    // Outer absolute deadline governs queue and transfer time. The bridge's much
    // coarser timeout is never earlier; no millisecond truncation extends a lease.
    if (!storage_.channel.admit(write, s.work.index, s.work.subindex,
                               write ? s.work.payload.bytes : nullptr, s.work.payload.length,
                               nowUs, UINT16_MAX, storage_.activeId)) {
        storage_.unresolved = true;
        finish(static_cast<unsigned>(chosen), EventKind::TRANSPORT_FAILURE, nowUs);
        return true;
    }
    storage_.active = chosen;
    storage_.urgentStarted = chosen == ORDINARY;
    storage_.nextAxis = static_cast<uint8_t>((s.sequence + 1) % AXES);
    storage_.channel.service(nowUs);
    collect(nowUs);
    return true;
}
bool CanBusOwner::receive(const Frame& frame, uint64_t nowUs) {
    if (!validateFrame(frame).isOk() || frame.extended || frame.rtr || frame.fd ||
        !service(nowUs)) return false;
    storage_.channel.receive(toNative(frame), nowUs);
    if (storage_.active >= 0) collect(nowUs);
    const bool event = (frame.id == 0x701 && frame.payloadLength == 1) ||
        (frame.id == 0x81 && frame.payloadLength == 8) ||
        (frame.id == 0x181 && frame.payloadLength >= 1);
    if (event) {
        // The production table consumed the frame once; this ring is only a copy.
        if (storage_.observationCount == OBSERVATIONS) {
            if (storage_.dropped != UINT32_MAX) ++storage_.dropped;
        } else {
            const unsigned tail = (storage_.observationHead + storage_.observationCount) % OBSERVATIONS;
            storage_.observations[tail].frame = frame;
            storage_.observations[tail].observedUs = nowUs;
            ++storage_.observationCount;
        }
        // Boot-up invalidates this route qualification even if the event ring is
        // full. Keep results, retire local TX and require external reconciliation.
        if (storage_.channel.channelUnresolved()) {
            storage_.unresolved = true;
            if (storage_.active >= 0) {
                collect(nowUs, true, EventKind::TRANSPORT_FAILURE);
            }
            service(nowUs);
        }
    }
    return true;
}
bool CanBusOwner::handoff(uint64_t nowUs) {
    if (!service(nowUs) || storage_.active < 0) return false;
    if (!storage_.channel.handoff(nowUs)) { collect(nowUs); return false; }
    storage_.slots[storage_.active].submitted = true;
    return true;
}
bool CanBusOwner::cancel(RequestId id, uint64_t nowUs) {
    if (!valid(id) || storage_.slots[id.slot].terminal || nowUs < storage_.lastUs) return false;
    if (!service(nowUs) || storage_.slots[id.slot].terminal) return false;
    if (storage_.active == id.slot) {
        storage_.channel.cancel(storage_.activeId);
        collect(nowUs, true, EventKind::CANCEL);
    } else finish(id.slot, EventKind::CANCEL, nowUs);
    return true;
}
const ServiceEvidence* CanBusOwner::result(RequestId id) const {
    return valid(id) && storage_.slots[id.slot].terminal ? &storage_.slots[id.slot].evidence : nullptr;
}
bool CanBusOwner::release(RequestId id) {
    if (!result(id)) return false;
    storage_.slots[id.slot].occupied = false;
    return true;
}
bool CanBusOwner::takeObservation(Observation& out) {
    if (!storage_.observationCount) return false;
    out = storage_.observations[storage_.observationHead];
    storage_.observationHead = static_cast<uint8_t>((storage_.observationHead + 1) % OBSERVATIONS);
    --storage_.observationCount;
    return true;
}
bool CanBusOwner::lastTransmitted(Frame& out) const {
    if (!storage_.channel.controller().handedOff) return false;
    out = fromNative(storage_.channel.controller().lastHandoff);
    return true;
}
} // namespace MotorControlCANExample
