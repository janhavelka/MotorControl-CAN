// SPDX-License-Identifier: MIT
#include "CanBusOwner.h"
#include "OwnerCounters.h"

namespace MotorControlCANOwner {
namespace C = MotorControlCAN::CANOPEN;
namespace {
using detail::overlap;
bool same(SequenceId a, SequenceId b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
bool same(C::SdoRequestId a, C::SdoRequestId b) {
    return a.owner == b.owner && a.channel == b.channel && a.generation == b.generation;
}
bool peer(const Binding& a, const Binding& b) {
    return a.endpoint.protocol == b.endpoint.protocol && a.endpoint.bus == b.endpoint.bus &&
           a.endpoint.address == b.endpoint.address;
}
uint8_t limit(uint8_t requested, unsigned maximum) {
    return requested < maximum ? requested : uint8_t(maximum);
}
uint64_t cutoff(uint64_t now, uint64_t deadline, uint32_t interval) {
    return deadline - now < interval ? deadline : now + interval;
}
} // namespace
CanBusOwner::CanBusOwner(C::SdoClient& services, const Transport& transport, uint32_t identity,
                         uint32_t lastIssued, uint32_t controllerEpoch) noexcept
    : services_(services), transport_(transport), identity_(identity), lastId_(lastIssued),
      controllerEpoch_(controllerEpoch), inhibited_(!controllerEpoch) {
    // Every channel starts idle, whatever the channel count: zero would mean slot 0 is active.
    for (int& index : active_)
        index = -1;
}
bool CanBusOwner::valid(SequenceId id) const noexcept {
    return id.owner == this && id.slot < SEQUENCES && id.generation &&
           sequences_[id.slot].occupied && sequences_[id.slot].generation == id.generation;
}
bool CanBusOwner::valid(RequestId id) const noexcept {
    return id.owner == this && id.slot < TOTAL_REQUESTS && id.generation &&
           slots_[id.slot].occupied && slots_[id.slot].generation == id.generation;
}
int CanBusOwner::channel(const Binding& binding) const noexcept {
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        if (route && MotorControlCAN::sameBinding(route->binding, binding))
            return int(i);
    }
    return -1;
}
int CanBusOwner::channel(const C::SdoChannel& wanted) const noexcept {
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        if (route && MotorControlCAN::sameBinding(route->binding, wanted.binding) &&
            route->clientToServerId == wanted.clientToServerId &&
            route->serverToClientId == wanted.serverToClientId)
            return int(i);
    }
    return -1;
}
bool CanBusOwner::occupied(SequenceId id) const noexcept {
    for (const Slot& slot : slots_)
        if (slot.occupied && same(slot.request.sequence, id))
            return true;
    return false;
}
bool CanBusOwner::live(SequenceId id) const noexcept {
    for (const Slot& slot : slots_)
        if (slot.occupied && !slot.terminal && same(slot.request.sequence, id))
            return true;
    return false;
}
const Sequence* CanBusOwner::sequence(SequenceId id) const noexcept {
    if (!valid(id) || sequences_[id.slot].invalidated)
        return nullptr;
    const Sequence& s = sequences_[id.slot].value;
    const int c = channel(s.binding);
    const auto* node = c < 0 ? nullptr : services_.node(uint8_t(c));
    if (!peersResolved(s.binding))
        return nullptr;
    return node && !node->exhausted && node->route.generation == s.nodeGeneration &&
                   !services_.unresolved(uint8_t(c))
               ? &s
               : nullptr;
}
bool CanBusOwner::peersResolved(const Binding& binding) const noexcept {
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        if (route && peer(route->binding, binding) && services_.unresolved(uint8_t(i)))
            return false;
    }
    return true;
}
bool CanBusOwner::beginSequence(const Binding& binding, uint8_t producer, uint64_t now,
                                SequenceId& output) noexcept {
    if (entered_ || inhibited_ || !identity_ || !producer || producer > PRODUCERS ||
        now < lastUs_ || lastId_ == UINT32_MAX ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &binding, sizeof binding))
        return false;
    const int c = channel(binding);
    const auto* node = c < 0 ? nullptr : services_.node(uint8_t(c));
    if (!node || node->exhausted || node->boundaryUs > now || services_.unresolved(uint8_t(c)))
        return false;
    if (!peersResolved(binding))
        return false;
    int free = -1, retired = -1;
    for (unsigned i = 0; i < SEQUENCES; ++i) {
        const Reservation& s = sequences_[i];
        // An invalidated reservation with nothing live stops blocking its axis. Its
        // results stay readable through their own request identities. A free slot goes
        // first, so that a holder keeps its reservation for as long as storage allows.
        SequenceId old;
        old.owner = this;
        old.slot = uint8_t(i);
        old.generation = s.generation;
        const bool reusable = s.occupied && s.invalidated && !live(old);
        if (s.occupied && !reusable &&
            (s.value.binding.logicalAxisId == binding.logicalAxisId || peer(s.value.binding, binding)))
            return false;
        if (!s.occupied && free < 0)
            free = int(i);
        if (reusable && retired < 0)
            retired = int(i);
    }
    if (free < 0)
        free = retired;
    if (free < 0)
        return false;
    Reservation candidate;
    candidate.occupied = true;
    candidate.generation = ++lastId_;
    candidate.value.binding = binding;
    candidate.value.nodeGeneration = node->route.generation;
    candidate.value.producer = producer;
    candidate.value.acquiredUs = now;
    sequences_[free] = candidate;
    output.owner = this;
    output.slot = uint8_t(free);
    output.generation = candidate.generation;
    lastUs_ = now;
    return true;
}
bool CanBusOwner::beginOperation(SequenceId id, uint32_t operation, uint64_t deadline,
                                 uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || deadline <= now || !operation || !sequence(id) || live(id) ||
        operation <= sequences_[id.slot].value.operation)
        return false;
    Reservation& s = sequences_[id.slot];
    s.value.operation = operation;
    s.value.deadlineUs = deadline;
    s.lastStep = 0;
    lastUs_ = now;
    return true;
}
bool CanBusOwner::beginUrgentOperation(SequenceId id, uint32_t operation, uint64_t deadline,
                                       uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || deadline <= now || !operation || !sequence(id) ||
        operation <= sequences_[id.slot].value.operation)
        return false;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    for (unsigned i = 0; i < TOTAL_REQUESTS; ++i)
        if (slots_[i].occupied && !slots_[i].terminal && !slots_[i].submitted &&
            !slots_[i].outsideSequence && same(slots_[i].request.sequence, id))
            cancelSlot(i, CompletionReason::CANCELLED, now);
    Reservation& s = sequences_[id.slot];
    s.value.operation = operation;
    s.value.deadlineUs = deadline;
    s.lastStep = 0;
    return true;
}
bool CanBusOwner::nodeEpochs(const Binding& binding, MotorControlCAN::Generation& nodeGeneration,
                             uint32_t& serverSessions) const noexcept {
    const int c = channel(binding);
    const auto* node = c < 0 ? nullptr : services_.node(uint8_t(c));
    if (!node || node->exhausted)
        return false;
    nodeGeneration = node->route.generation;
    serverSessions = node->serverSessions;
    return true;
}
bool CanBusOwner::matchesChannel(const C::SdoChannel& route,
                                 MotorControlCAN::Generation nodeGeneration) const noexcept {
    const int slot = channel(route);
    const auto* node = slot < 0 ? nullptr : services_.node(uint8_t(slot));
    return nodeGeneration && node && !node->exhausted && node->route.generation == nodeGeneration;
}
bool CanBusOwner::findSequence(const Binding& binding, SequenceId& output) const noexcept {
    if (overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &binding, sizeof binding))
        return false;
    for (unsigned i = 0; i < SEQUENCES; ++i) {
        const Reservation& s = sequences_[i];
        SequenceId id;
        id.owner = this;
        id.slot = uint8_t(i);
        id.generation = s.generation;
        if (s.occupied && MotorControlCAN::sameBinding(s.value.binding, binding) && sequence(id)) {
            output = id;
            return true;
        }
    }
    return false;
}
bool CanBusOwner::releaseSequence(SequenceId id) noexcept {
    if (entered_)
        return false;
    // Identities only grow, so an older one on this slot was reused and holds nothing.
    if (id.owner == this && id.slot < SEQUENCES && id.generation != 0 &&
        id.generation < sequences_[id.slot].generation)
        return true;
    if (!valid(id) || occupied(id))
        return false;
    sequences_[id.slot].occupied = false;
    return true;
}
BusAdmission CanBusOwner::admit(const BusRequest& request, uint64_t now,
                                RequestId& output) noexcept {
    return countAdmission(admit(request, now, output, false));
}
BusAdmission CanBusOwner::admitUrgent(const BusRequest& request, uint64_t now,
                                      RequestId& output) noexcept {
    return countAdmission(admit(request, now, output, true));
}
BusAdmission CanBusOwner::admitDrainMarker(const C::SdoTransfer& transfer,
                                           const C::SdoDrainPremises& premises, uint64_t now,
                                           RequestId& output) noexcept {
    if (entered_ || !transport_.transmit || now < lastUs_ ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &transfer, sizeof transfer) ||
        overlap(&output, sizeof output, &premises, sizeof premises))
        return countAdmission(BusAdmission::INVALID);
    if (inhibited_)
        return countAdmission(BusAdmission::INHIBITED);
    const int c = channel(transfer.channel);
    if (c < 0)
        return countAdmission(BusAdmission::INVALID);
    if (active_[c] >= 0 || external_[c].pending || external_[c].blocked || nmtBlocks(transfer.channel.binding))
        return countAdmission(BusAdmission::INHIBITED);
    if (!services_.checkDrain(uint8_t(c), transfer, premises, now))
        return countAdmission(BusAdmission::INVALID);
    const BusAdmission placed = placeOutside(uint8_t(c), transfer, now, output);
    if (placed == BusAdmission::ACCEPTED)
        slots_[output.slot].premises = premises;
    return countAdmission(placed);
}
BusAdmission CanBusOwner::admitBestEffortStop(const C::SdoTransfer& transfer, uint64_t now,
                                              RequestId& output) noexcept {
    if (entered_ || !transport_.transmit || now < lastUs_ ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &transfer, sizeof transfer))
        return countAdmission(BusAdmission::INVALID);
    if (inhibited_)
        return countAdmission(BusAdmission::INHIBITED);
    const int c = channel(transfer.channel);
    if (c < 0 || !services_.validBestEffortStop(uint8_t(c), transfer, now))
        return countAdmission(BusAdmission::INVALID);
    if (external_[c].blocked || nmtBlocks(transfer.channel.binding) ||
        !services_.bestEffortEligible(uint8_t(c), now))
        return countAdmission(BusAdmission::INHIBITED);
    // Live work on the channel settles first, and dispatch rechecks all of the above.
    const BusAdmission placed = placeOutside(uint8_t(c), transfer, now, output);
    if (placed == BusAdmission::ACCEPTED)
        slots_[output.slot].bestEffortStop = true;
    return countAdmission(placed);
}
BusAdmission CanBusOwner::placeOutside(uint8_t c, const C::SdoTransfer& transfer, uint64_t now,
                                       RequestId& output) noexcept {
    if (lastId_ == UINT32_MAX)
        return BusAdmission::IDS_EXHAUSTED;
    const unsigned free = REQUESTS + c;
    if (slots_[free].occupied)
        return BusAdmission::RESULTS_FULL;
    Slot candidate;
    candidate.occupied = candidate.outsideSequence = true;
    candidate.generation = ++lastId_;
    candidate.queueDeadlineUs = cutoff(now, transfer.deadlineUs, transfer.queueTimeoutUs);
    candidate.request.transfer = transfer;
    candidate.completion.id.owner = this;
    candidate.completion.id.slot = uint8_t(free);
    candidate.completion.id.generation = candidate.generation;
    candidate.completion.admittedUs = now;
    candidate.completion.service.transfer = transfer;
    candidate.completion.service.admittedUs = now;
    const auto* node = services_.node(c);
    candidate.completion.service.nodeGeneration = node ? node->route.generation : 0;
    slots_[free] = candidate;
    output = candidate.completion.id;
    lastUs_ = now;
    return BusAdmission::ACCEPTED;
}
BusAdmission CanBusOwner::countAdmission(BusAdmission result) noexcept {
    if (!entered_ && result != BusAdmission::ACCEPTED) {
        detail::add(stats_.queueRejected);
        if (result == BusAdmission::RESULTS_FULL)
            detail::add(stats_.resultsRejected);
    }
    return result;
}
BusAdmission CanBusOwner::admit(const BusRequest& request, uint64_t now, RequestId& output,
                                bool urgent) noexcept {
    if (entered_ || !transport_.transmit || now < lastUs_ ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &request, sizeof request))
        return BusAdmission::INVALID;
    if (inhibited_)
        return BusAdmission::INHIBITED;
    const Sequence* s = sequence(request.sequence);
    if (!s)
        return BusAdmission::INVALID;
    if (request.transfer.deadlineUs <= now || s->deadlineUs <= now)
        return BusAdmission::EXPIRED;
    const int c = channel(request.transfer.channel);
    if (c >= 0 && (external_[c].blocked || nmtBlocks(request.transfer.channel.binding)))
        return BusAdmission::INHIBITED;
    if (c < 0 || !MotorControlCAN::sameBinding(s->binding, request.transfer.channel.binding) ||
        request.transfer.operation != s->operation ||
        request.transfer.deadlineUs > s->deadlineUs ||
        request.transfer.step <= sequences_[request.sequence.slot].lastStep ||
        !services_.checkAdmission(uint8_t(c), request.transfer, now))
        return BusAdmission::INVALID;
    if (lastId_ == UINT32_MAX)
        return BusAdmission::IDS_EXHAUSTED;
    int free = -1;
    if (urgent) {
        if (!slots_[REQUESTS + c].occupied)
            free = REQUESTS + c;
    } else {
        for (unsigned i = 0; i < REQUESTS && free < 0; ++i)
            if (!slots_[i].occupied)
                free = int(i);
    }
    if (free < 0)
        return BusAdmission::RESULTS_FULL;
    Slot candidate;
    candidate.occupied = true;
    candidate.generation = ++lastId_;
    candidate.queueDeadlineUs =
        cutoff(now, request.transfer.deadlineUs, request.transfer.queueTimeoutUs);
    candidate.request = request;
    candidate.completion.id.owner = this;
    candidate.completion.id.slot = uint8_t(free);
    candidate.completion.id.generation = candidate.generation;
    candidate.completion.sequence = request.sequence;
    candidate.completion.producer = s->producer;
    candidate.completion.admittedUs = now;
    candidate.completion.service.transfer = request.transfer;
    candidate.completion.service.admittedUs = now;
    candidate.completion.service.nodeGeneration = s->nodeGeneration;
    slots_[free] = candidate;
    sequences_[request.sequence.slot].lastStep = request.transfer.step;
    output = candidate.completion.id;
    lastUs_ = now;
    return BusAdmission::ACCEPTED;
}
void CanBusOwner::finish(unsigned index, CompletionReason reason, uint64_t now) noexcept {
    Slot& slot = slots_[index];
    if (slot.terminal)
        return;
    slot.terminal = true;
    slot.completion.reason = reason;
    auto& e = slot.completion.service;
    e.completedUs = now;
    e.outcome = reason == CompletionReason::QUEUE_EXPIRED || reason == CompletionReason::DEADLINE
                    ? C::SdoOutcome::TIMEOUT
                : reason == CompletionReason::CANCELLED ? C::SdoOutcome::CANCELLED
                                                        : C::SdoOutcome::BINDING_INVALIDATED;
    recordTerminal(slot);
}
void CanBusOwner::collect() noexcept {
    for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c) {
        const int index = active_[c];
        if (index < 0)
            continue;
        Slot& slot = slots_[index];
        const auto* e = services_.result(slot.native);
        if (!e)
            continue;
        slot.completion.service = *e;
        slot.terminal = true;
        if (!slot.outsideSequence && e->outcome == C::SdoOutcome::SERVER_ABORT)
            // A rejected step ends what depends on it, though the route stays open. A later
            // urgent operation does not depend on it.
            for (unsigned i = 0; i < TOTAL_REQUESTS; ++i)
                if (slots_[i].occupied && !slots_[i].terminal && !slots_[i].submitted &&
                    same(slots_[i].request.sequence, slot.request.sequence) &&
                    slots_[i].request.transfer.operation == slot.request.transfer.operation)
                    finish(i, CompletionReason::INVALIDATED, e->completedUs);
        recordTerminal(slot);
        services_.release(slot.native);
        active_[c] = -1;
    }
}
bool CanBusOwner::markerCurrent(const Slot& slot) const noexcept {
    const int c = channel(slot.request.transfer.channel);
    const auto* node = c < 0 ? nullptr : services_.node(uint8_t(c));
    return c >= 0 && (node ? node->route.generation : 0) == slot.completion.service.nodeGeneration;
}
void CanBusOwner::reopenSettledRoutes() noexcept {
    for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c) {
        // The marker's own frame must be retired first, or its duplicate could answer new work.
        if (!services_.drainSettled(uint8_t(c)) || external_[c].pending || external_[c].blocked)
            continue;
        const auto* route = services_.channel(uint8_t(c));
        if (!route)
            continue;
        // Otherwise sequence() would make an old reservation, and its old command
        // history, live again the moment the route reopens.
        for (Reservation& s : sequences_)
            if (s.occupied && peer(s.value.binding, route->binding))
                s.invalidated = true;
        services_.reopenAfterDrain(uint8_t(c));
    }
}
bool CanBusOwner::mayHaveTransmitted(RequestId id) const noexcept {
    if (!valid(id))
        return false;
    const Slot& slot = slots_[id.slot];
    if (slot.terminal)
        return slot.completion.service.transmissionPossible;
    return slot.submitted && services_.transmissionPossible(slot.native);
}
void CanBusOwner::expire(uint64_t now) noexcept {
    for (unsigned i = 0; i < TOTAL_REQUESTS; ++i) {
        const Slot& slot = slots_[i];
        if (!slot.occupied || slot.terminal)
            continue;
        if (slot.submitted) {
            if (!slot.outsideSequence && !sequence(slot.request.sequence)) {
                slots_[i].completion.reason = CompletionReason::INVALIDATED;
                services_.invalidate(slot.native.channel, now);
                collect();
            }
        } else if (now >= slot.request.transfer.deadlineUs)
            finish(i, CompletionReason::DEADLINE, now);
        else if (slot.outsideSequence ? !markerCurrent(slot) : !sequence(slot.request.sequence))
            finish(i, CompletionReason::INVALIDATED, now);
        else if (now >= slot.queueDeadlineUs)
            finish(i, CompletionReason::QUEUE_EXPIRED, now);
    }
}
bool CanBusOwner::eligible(unsigned index, uint64_t now) const noexcept {
    const Slot& slot = slots_[index];
    if (!slot.occupied || slot.terminal || slot.submitted ||
        (!slot.outsideSequence && !sequence(slot.request.sequence)))
        return false;
    const int c = channel(slot.request.transfer.channel);
    if (c < 0 || active_[c] >= 0 || external_[c].pending || external_[c].blocked ||
        nmtBlocks(slot.request.transfer.channel.binding))
        return false;
    // A stop's route ends unresolved, and that ends in-flight work on every channel of the
    // node. So it waits for a peer's work to settle, and no new peer work starts around it.
    for (unsigned k = 0; k < C::SdoClient::CHANNELS; ++k) {
        const auto* route = services_.channel(uint8_t(k));
        if (int(k) == c || !route || !peer(route->binding, slot.request.transfer.channel.binding))
            continue;
        const Slot& urgent = slots_[REQUESTS + k];
        if (slot.bestEffortStop ? active_[k] >= 0
                                : urgent.occupied && !urgent.terminal && urgent.bestEffortStop)
            return false;
    }
    if (slot.bestEffortStop)
        return markerCurrent(slot) &&
               services_.checkBestEffortStop(uint8_t(c), slot.request.transfer, now);
    if (slot.outsideSequence)
        return markerCurrent(slot) &&
               services_.checkDrain(uint8_t(c), slot.request.transfer, slot.premises, now);
    if (!services_.checkAdmission(uint8_t(c), slot.request.transfer, now))
        return false;
    for (const Slot& other : slots_)
        if (other.occupied && !other.terminal && other.generation < slot.generation &&
            same(other.request.sequence, slot.request.sequence))
            return false;
    return true;
}
int CanBusOwner::select(uint64_t now) const noexcept {
    // Priority changes scheduling between reservations, never same-axis intent order.
    for (unsigned i = REQUESTS; i < TOTAL_REQUESTS; ++i)
        if (eligible(i, now))
            return int(i);
    for (unsigned p = 0; p < PRODUCERS; ++p) {
        const uint8_t producer = uint8_t((nextProducer_ - 1 + p) % PRODUCERS + 1);
        int first = -1;
        for (unsigned i = 0; i < REQUESTS; ++i) {
            const Slot& slot = slots_[i];
            if (slot.completion.producer != producer || !eligible(i, now))
                continue;
            if (first < 0 || slot.generation < slots_[first].generation)
                first = int(i);
        }
        if (first >= 0)
            return first;
    }
    return -1;
}
bool CanBusOwner::transmit(uint8_t c, uint64_t now) noexcept {
    if (inhibited_ || external_[c].pending || external_[c].blocked || !transport_.transmit)
        return false;
    expire(now);
    const auto* port = services_.port(c);
    const auto* route = services_.channel(c);
    if (!port || !port->queueOccupied || !route || nmtBlocks(route->binding))
        return false;
    const int index = active_[c];
    const bool cleanup = port->queued.data[0] == 0x80;
    if (index >= 0 && !cleanup) {
        Slot& slot = slots_[index];
        // A best-effort stop has no reservation for a guard to act on, and no guard may withdraw it.
        if (!slot.guarded && !slot.outsideSequence &&
            slot.request.transfer.direction == C::SdoDirection::DOWNLOAD) {
            if (transport_.beforeTransmit &&
                !transport_.beforeTransmit(transport_.context, slot.request.sequence,
                                           slot.request.transfer, now)) {
                slot.completion.reason = CompletionReason::PREFLIGHT_REJECTED;
                if (services_.cancel(slot.native, now))
                    slot.localCancellation = true;
                collect();
                return false;
            }
            slot.guarded = true;
        }
    }
    Frame frame;
    C::SdoTxId id;
    if (!services_.nextTx(c, now, frame, id) || !services_.canTransmit(id, now))
        return false;
    external_[c] = ExternalTx();
    external_[c].pending = true;
    external_[c].epoch = controllerEpoch_;
    external_[c].id = TransportTxId::fromSdo(id);
    const bool accepted = transport_.transmit(transport_.context, frame, TransportTxId::fromSdo(id), controllerEpoch_);
    external_[c].rejected = !accepted;
    if (!accepted)
        detail::add(stats_.txRejected);
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::TX;
    diagnostic.frame = frame;
    diagnostic.txId = TransportTxId::fromSdo(id);
    diagnostic.accepted = accepted;
    diagnostic.processedUs = now;
    diagnostic.controllerEpoch = controllerEpoch_;
    trace(diagnostic);
    if (index >= 0 && !slots_[index].terminal && same(slots_[index].native, id.request))
        slots_[index].completion.controllerAccepted |= accepted;
    if (!accepted)
        services_.transportFailure(id.request, now);
    collect();
    return true; // One enqueue attempt, even when refused; never retry cached bytes.
}
bool CanBusOwner::service(uint64_t now, const ServiceBudget& budget) noexcept {
    if (entered_ || now < lastUs_ || overlap(&budget, sizeof budget, this, sizeof *this) ||
        overlap(&budget, sizeof budget, &services_, sizeof services_))
        return false;
    const detail::EnteredScope entered(entered_);
    if (!services_.service(now))
        return false;
    if (serviceTimeKnown_ && now - serviceTimeUs_ > stats_.maxServiceGapUs)
        stats_.maxServiceGapUs = now - serviceTimeUs_;
    serviceTimeKnown_ = true;
    serviceTimeUs_ = now;
    lastUs_ = now;
    collect();
    expire(now);
    if (recoveryOccupied_ && !recoveryTerminal_) {
        if (now >= recovery_.deadlineUs)
            closeRecovery(RecoveryOutcome::TIMEOUT, now);
        else if (budget.recovery && transport_.pollRecovery) {
            const RecoveryProgress progress =
                transport_.pollRecovery(transport_.context, controllerEpoch_);
            if (progress == RecoveryProgress::RETIRED)
                closeRecovery(RecoveryOutcome::RETIRED, now);
            else if (progress != RecoveryProgress::PENDING)
                closeRecovery(RecoveryOutcome::FAILED, now);
        }
    }
    if (transport_.takeTxReport)
        for (unsigned i = 0; i < limit(budget.events, EVENT_LIMIT); ++i) {
            TxReport report;
            if (!transport_.takeTxReport(transport_.context, report))
                break;
            if (report.controllerEpoch != controllerEpoch_ ||
                (report.id.kind != TransportTxId::Kind::SDO && report.id.kind != TransportTxId::Kind::NMT) ||
                (report.id.kind == TransportTxId::Kind::SDO &&
                 report.id.sdo.request.channel >= C::SdoClient::CHANNELS))
                continue;
            ExternalTx& tx = report.id.kind == TransportTxId::Kind::NMT
                ? nmtExternal_ : external_[report.id.sdo.request.channel];
            if (!tx.pending || !sameTransportTxId(tx.id, report.id))
                continue;
            if (report.transmitted)
                detail::add(stats_.txFrames);
            else if (!tx.rejected)
                detail::add(stats_.txRejected);
            Diagnostic diagnostic;
            diagnostic.kind = DiagnosticKind::TX_REPORT;
            diagnostic.txId = report.id;
            diagnostic.accepted = report.transmitted;
            diagnostic.processedUs = now;
            diagnostic.controllerEpoch = report.controllerEpoch;
            trace(diagnostic);
            tx = ExternalTx(); // A matching final report proves this exact local frame is retired.
            if (report.id.kind == TransportTxId::Kind::SDO)
                services_.noteTxRetired(report.id.sdo);
            if (report.id.kind == TransportTxId::Kind::NMT) {
                if (report.transmitted)
                    services_.confirmNmtTx(report.id.nmt, now);
                else if (report.provedUnsent)
                    services_.withdrawNmtUnsent(report.id.nmt, now);
                else if (services_.canTransmitNmt(report.id.nmt, now))
                    services_.transportFailureNmt(report.id.nmt, now);
            } else if (report.transmitted)
                services_.confirmTx(report.id.sdo, now);
            else if (report.provedUnsent)
                services_.withdrawUnsent(report.id.sdo, now);
            else if (services_.canTransmit(report.id.sdo, now))
                services_.transportFailure(report.id.sdo.request, now);
            collect();
            expire(now);
        }
    if (transport_.receive)
        for (unsigned i = 0; i < limit(budget.rx, RX_LIMIT); ++i) {
            ReceivedFrame frame;
            if (!transport_.receive(transport_.context, frame))
                break;
            detail::add(stats_.rxFrames);
            bool accepted = false;
            if (frame.controllerEpoch == controllerEpoch_ &&
                MotorControlCAN::validateFrame(frame.frame) &&
                !(frame.receivedTimeKnown && frame.receiveBoundsKnown) &&
                (!frame.receivedTimeKnown || frame.receivedUs <= now) &&
                (!frame.receiveBoundsKnown || (frame.receivedEarliestUs <= frame.receivedLatestUs &&
                                               frame.receivedLatestUs <= now))) {
                if (frame.receivedTimeKnown)
                    accepted = services_.receive(frame.frame, frame.receivedUs, now);
                else if (frame.receiveBoundsKnown)
                    accepted = services_.receive(frame.frame, frame.receivedEarliestUs,
                                                 frame.receivedLatestUs, now);
                else
                    accepted = services_.receive(frame.frame, now);
                if (!accepted)
                    detail::add(stats_.unmatchedFrames);
            } else
                detail::add(stats_.rxDropped);
            refreshHealth(now);
            Diagnostic diagnostic;
            diagnostic.kind = DiagnosticKind::RX;
            diagnostic.frame = frame.frame;
            diagnostic.receivedTimeKnown = frame.receivedTimeKnown;
            diagnostic.receivedUs = frame.receivedUs;
            diagnostic.receiveBoundsKnown = frame.receiveBoundsKnown;
            diagnostic.receivedEarliestUs = frame.receivedEarliestUs;
            diagnostic.receivedLatestUs = frame.receivedLatestUs;
            diagnostic.accepted = accepted;
            diagnostic.processedUs = now;
            diagnostic.controllerEpoch = frame.controllerEpoch;
            trace(diagnostic);
            if (transport_.observe)
                transport_.observe(transport_.context, frame, now);
            collect();
            expire(now);
        }
    expire(now);
    settleExternal(now, budget.events != 0);
    const uint8_t tx = limit(budget.tx, TX_LIMIT);
    unsigned sent = 0, starts = 0;
    if (sent < tx && transmitNmt(now)) {
        ++sent;
        collect();
        expire(now);
        settleExternal(now, budget.events != 0);
    }
    const uint8_t firstChannel = nextChannel_;
    for (unsigned scan = 0; scan < C::SdoClient::CHANNELS && sent < tx; ++scan) {
        const uint8_t c = uint8_t((firstChannel + scan) % C::SdoClient::CHANNELS);
        if (transmit(c, now)) {
            ++sent;
            nextChannel_ = uint8_t((c + 1) % C::SdoClient::CHANNELS);
        }
    }
    while (!inhibited_ && sent < tx && starts < limit(budget.starts, START_LIMIT)) {
        const int index = select(now);
        if (index < 0)
            break;
        Slot& slot = slots_[index];
        const int c = channel(slot.request.transfer.channel);
        ++starts;
        if (!slot.outsideSequence)
            nextProducer_ = uint8_t(slot.completion.producer % PRODUCERS + 1);
        const bool admitted =
            slot.bestEffortStop
                ? services_.admitBestEffortStop(uint8_t(c), slot.request.transfer, now, slot.native,
                                                slot.queueDeadlineUs)
            : slot.outsideSequence
                ? services_.admitDrain(uint8_t(c), slot.request.transfer, slot.premises, now,
                                       slot.native, slot.queueDeadlineUs)
                : services_.admit(uint8_t(c), slot.request.transfer, now, slot.native,
                                  slot.queueDeadlineUs);
        if (!admitted) {
            finish(unsigned(index), CompletionReason::ADMISSION_FAILED, now);
            continue;
        }
        slot.submitted = slot.completion.adapterAdmitted = true;
        active_[c] = index;
        services_.service(now);
        collect();
        if (transmit(uint8_t(c), now))
            ++sent;
    }
    reopenSettledRoutes();
    refreshHealth(now);
    return true;
}
bool CanBusOwner::localTxRetired(uint8_t slot) const noexcept {
    const auto* port = services_.port(slot);
    return port && !port->queueOccupied && !external_[slot].pending && !external_[slot].blocked &&
        !nmtExternal_.pending && !nmtExternal_.blocked;
}
const Completion* CanBusOwner::result(RequestId id) const noexcept {
    return valid(id) && slots_[id.slot].terminal ? &slots_[id.slot].completion : nullptr;
}
bool CanBusOwner::canRelease(RequestId id) const noexcept {
    return !entered_ && valid(id) && slots_[id.slot].terminal;
}
bool CanBusOwner::release(RequestId id) noexcept {
    if (!canRelease(id))
        return false;
    slots_[id.slot].occupied = false;
    return true;
}
bool CanBusOwner::nmtBlocks(const Binding& binding) const noexcept {
    return (nmtExternal_.pending || nmtExternal_.blocked) &&
        binding.endpoint.bus == nmtTransfer_.bus &&
        (nmtTransfer_.broadcast || peer(binding, nmtTransfer_.target.binding));
}
bool CanBusOwner::canTransmit(TransportTxId id, uint64_t now) const noexcept {
    return id.kind == TransportTxId::Kind::NMT ? services_.canTransmitNmt(id.nmt, now) :
        id.kind == TransportTxId::Kind::SDO && services_.canTransmit(id.sdo, now);
}
void CanBusOwner::settle(ExternalTx& tx, uint64_t now, bool allowCallback) noexcept {
    if (!tx.pending || canTransmit(tx.id, now))
        return;
    tx.blocked = true;
    if (!allowCallback || tx.retirementTried)
        return;
    tx.retirementTried = true;
    if (transport_.retire && transport_.retire(transport_.context, tx.id, tx.epoch)) {
        if (tx.id.kind == TransportTxId::Kind::SDO)
            services_.noteTxRetired(tx.id.sdo);
        tx = ExternalTx(); // This exact frame and every future final report are retired.
    }
}
void CanBusOwner::settleExternal(uint64_t now, bool allowCallback) noexcept {
    for (ExternalTx& tx : external_)
        settle(tx, now, allowCallback);
    settle(nmtExternal_, now, allowCallback);
}
bool CanBusOwner::admitNmt(const C::NmtTransfer& work, uint64_t now,
                            C::NmtRequestId& output) noexcept {
    if (entered_ || inhibited_ || !identity_ || !transport_.transmit || now < lastUs_ ||
        nmtOccupied_ || nmtExternal_.pending || nmtExternal_.blocked ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &work, sizeof work))
        return false;
    const detail::EnteredScope entered(entered_);
    C::NmtRequestId candidate;
    const bool accepted = services_.admitNmt(work, now, candidate);
    if (accepted) {
        nmtId_ = candidate;
        nmtTransfer_ = work;
        nmtOccupied_ = true;
        output = candidate;
        lastUs_ = now;
    }
    return accepted;
}
bool CanBusOwner::transmitNmt(uint64_t now) noexcept {
    if (inhibited_ || !nmtOccupied_ || nmtExternal_.pending || nmtExternal_.blocked ||
        !transport_.transmit)
        return false;
    Frame frame;
    if (!services_.nextNmtTx(nmtId_, now, frame) || !services_.canTransmitNmt(nmtId_, now))
        return false;
    nmtExternal_ = ExternalTx();
    nmtExternal_.pending = true;
    nmtExternal_.epoch = controllerEpoch_;
    nmtExternal_.id = TransportTxId::fromNmt(nmtId_);
    const bool accepted = transport_.transmit(transport_.context, frame, nmtExternal_.id,
                                            controllerEpoch_);
    nmtExternal_.rejected = !accepted;
    if (!accepted) {
        detail::add(stats_.txRejected);
        services_.transportFailureNmt(nmtId_, now);
    }
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::TX;
    diagnostic.frame = frame;
    diagnostic.txId = nmtExternal_.id;
    diagnostic.accepted = accepted;
    diagnostic.processedUs = now;
    diagnostic.controllerEpoch = controllerEpoch_;
    trace(diagnostic);
    return true;
}
bool CanBusOwner::cancelNmt(C::NmtRequestId id, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || !nmtOccupied_ || id.owner != nmtId_.owner ||
        id.generation != nmtId_.generation)
        return false;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    const bool cancelled = services_.cancelNmt(id, now);
    if (cancelled)
        detail::add(stats_.cancelled);
    collect();
    expire(now);
    settleExternal(now, true);
    refreshHealth(now);
    return cancelled;
}
const C::NmtCompletion* CanBusOwner::nmtResult(C::NmtRequestId id) const noexcept {
    return nmtOccupied_ && id.owner == nmtId_.owner && id.generation == nmtId_.generation
        ? services_.nmtResult(id) : nullptr;
}
bool CanBusOwner::releaseNmt(C::NmtRequestId id) noexcept {
    if (entered_ || !nmtResult(id) || !services_.releaseNmt(id))
        return false;
    nmtOccupied_ = false;
    return true;
}
void CanBusOwner::cancelSlot(unsigned index, CompletionReason reason, uint64_t now) noexcept {
    Slot& slot = slots_[index];
    if (!slot.occupied || slot.terminal)
        return;
    if (slot.submitted) {
        // cancel expires first without pumping an unsent request. Set the owner
        // cause only if cancellation actually wins; an earlier native terminal survives.
        if (services_.cancel(slot.native, now)) {
            slot.completion.reason = reason;
            slot.localCancellation = true;
        }
        collect();
    } else if (now >= slot.request.transfer.deadlineUs)
        finish(index, CompletionReason::DEADLINE, now);
    else if (now >= slot.queueDeadlineUs)
        finish(index, CompletionReason::QUEUE_EXPIRED, now);
    else {
        slot.localCancellation = true;
        finish(index, reason, now);
    }
}
Cancel CanBusOwner::cancel(RequestId id, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || !valid(id))
        return Cancel::INVALID;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    collect();
    const bool alreadyTerminal = slots_[id.slot].terminal;
    cancelSlot(id.slot, CompletionReason::CANCELLED, now);
    const Cancel disposition =
        !alreadyTerminal && slots_[id.slot].completion.reason == CompletionReason::CANCELLED
            ? Cancel::CANCELLED
            : Cancel::ALREADY_TERMINAL;
    settleExternal(now, true);
    refreshHealth(now);
    return disposition;
}
bool CanBusOwner::invalidate(SequenceId id, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || !valid(id))
        return false;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    collect();
    for (unsigned i = 0; i < TOTAL_REQUESTS; ++i)
        if (slots_[i].occupied && same(slots_[i].request.sequence, id))
            cancelSlot(i, CompletionReason::INVALIDATED, now);
    sequences_[id.slot].invalidated = true;
    settleExternal(now, true);
    refreshHealth(now);
    return true;
}
bool CanBusOwner::invalidateProducer(uint8_t producer, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || !producer || producer > PRODUCERS)
        return false;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    collect();
    for (unsigned i = 0; i < TOTAL_REQUESTS; ++i)
        if (slots_[i].occupied && slots_[i].completion.producer == producer)
            cancelSlot(i, CompletionReason::INVALIDATED, now);
    for (Reservation& s : sequences_)
        if (s.occupied && s.value.producer == producer)
            s.invalidated = true;
    settleExternal(now, true);
    refreshHealth(now);
    return true;
}
bool CanBusOwner::recover(uint64_t now, uint64_t deadline, RecoveryId& output) noexcept {
    if (entered_ || now < lastUs_ || deadline <= now || recoveryOccupied_ || !identity_ ||
        lastId_ == UINT32_MAX || !controllerEpoch_ || controllerEpoch_ == UINT32_MAX ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_))
        return false;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    inhibited_ = true;
    collect();
    for (unsigned i = 0; i < TOTAL_REQUESTS; ++i)
        cancelSlot(i, CompletionReason::RECOVERY, now);
    for (Reservation& s : sequences_)
        s.invalidated = true;
    services_.reportController(C::ControllerState::RECOVERING, now);
    collect();
    recovery_ = RecoveryCompletion();
    recovery_.id.owner = this;
    recovery_.id.generation = ++lastId_;
    recovery_.controllerEpoch = ++controllerEpoch_;
    recovery_.startedUs = now;
    recovery_.deadlineUs = deadline;
    recoveryOccupied_ = true;
    recoveryTerminal_ = false;
    output = recovery_.id;
    if (!transport_.beginRecovery || !transport_.pollRecovery ||
        !transport_.beginRecovery(transport_.context, controllerEpoch_))
        closeRecovery(RecoveryOutcome::FAILED, now);
    refreshHealth(now);
    return true;
}
void CanBusOwner::closeRecovery(RecoveryOutcome outcome, uint64_t now) noexcept {
    if (!recoveryOccupied_ || recoveryTerminal_)
        return;
    recovery_.outcome = outcome;
    recovery_.completedUs = now;
    recoveryTerminal_ = true;
    if (outcome == RecoveryOutcome::TIMEOUT)
        detail::add(stats_.timeouts);
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::RECOVERY;
    diagnostic.processedUs = now;
    diagnostic.controllerEpoch = controllerEpoch_;
    diagnostic.recovery = outcome;
    trace(diagnostic);
    if (outcome == RecoveryOutcome::RETIRED) {
        detail::add(stats_.recoveries);
        for (ExternalTx& tx : external_)
            tx = ExternalTx();
        nmtExternal_ = ExternalTx();
        services_.reportController(C::ControllerState::AVAILABLE, now);
        inhibited_ = false;
    }
}
const RecoveryCompletion* CanBusOwner::recoveryResult(RecoveryId id) const noexcept {
    return id.owner == this && recoveryOccupied_ && recoveryTerminal_ &&
                   id.generation == recovery_.id.generation
               ? &recovery_
               : nullptr;
}
bool CanBusOwner::releaseRecovery(RecoveryId id) noexcept {
    if (entered_ || !recoveryResult(id))
        return false;
    recoveryOccupied_ = false;
    return true;
}
bool CanBusOwner::reportController(C::ControllerState state, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || state > C::ControllerState::RECOVERING ||
        (state == C::ControllerState::AVAILABLE && inhibited_))
        return false;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    if (state == C::ControllerState::BUS_OFF &&
        services_.controllerState() != C::ControllerState::BUS_OFF)
        detail::add(stats_.busOffEvents);
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::CONTROLLER;
    diagnostic.controller = state;
    diagnostic.controllerEpoch = controllerEpoch_;
    diagnostic.processedUs = now;
    trace(diagnostic);
    if (state != C::ControllerState::AVAILABLE) {
        inhibited_ = true;
        for (Reservation& s : sequences_)
            s.invalidated = true;
    }
    // UNKNOWN also withdraws freshness; the native UNKNOWN report alone is only
    // an observation and does not retire channels. Do that retirement explicitly.
    services_.reportController(
        state == C::ControllerState::UNKNOWN ? C::ControllerState::RECOVERING : state, now);
    if (state == C::ControllerState::UNKNOWN)
        services_.reportController(state, now);
    collect();
    expire(now);
    settleExternal(now, true);
    if (recoveryOccupied_ && !recoveryTerminal_ && state != C::ControllerState::RECOVERING)
        closeRecovery(
            now >= recovery_.deadlineUs ? RecoveryOutcome::TIMEOUT : RecoveryOutcome::FAILED, now);
    refreshHealth(now);
    return true;
}
bool CanBusOwner::receiveOverflow(uint64_t now, uint32_t droppedFrames) noexcept {
    if (!droppedFrames || !reportController(C::ControllerState::RECOVERING, now))
        return false;
    detail::add(stats_.rxDropped, droppedFrames);
    detail::add(stats_.rxLost, droppedFrames);
    return true;
}
bool CanBusOwner::advanceConfiguration(SequenceId id, const Binding& current,
                                       uint64_t now) noexcept {
    if (entered_ || inhibited_ || now < lastUs_ || !sequence(id) || occupied(id) ||
        nmtExternal_.pending || nmtExternal_.blocked)
        return false;
    Reservation& s = sequences_[id.slot];
    const Binding next = current;
    for (const Slot& slot : slots_)
        if (slot.occupied && peer(slot.request.transfer.channel.binding, s.value.binding))
            return false;
    const int c = channel(s.value.binding);
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        if (route && peer(route->binding, s.value.binding) &&
            (external_[i].pending || external_[i].blocked))
            return false;
    }
    bool changed;
    {
        const detail::EnteredScope entered(entered_);
        changed = services_.advanceConfiguration(uint8_t(c), s.value.binding, next, now);
    }
    if (!changed)
        return false;
    s.value.binding = next;
    lastUs_ = now;
    refreshHealth(now);
    return true;
}
} // namespace MotorControlCANOwner
