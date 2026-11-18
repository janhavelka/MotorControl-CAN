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
BusAdmission CanBusOwner::admitDrainMarker(const C::SdoTransfer& transfer, uint64_t now,
                                           RequestId& output) noexcept {
    C::SdoDrainPremises accepted; // Stated by the reuse policy as an accepted risk (Q18).
    accepted.soleClient = accepted.orderedServer = true;
    return countAdmission(admitMarker(transfer, accepted, now, output));
}
BusAdmission CanBusOwner::admitDrainMarker(const C::SdoTransfer& transfer,
                                           const C::SdoDrainPremises& premises, uint64_t now,
                                           RequestId& output) noexcept {
    if (overlap(&output, sizeof output, &premises, sizeof premises))
        return countAdmission(BusAdmission::INVALID);
    const C::SdoDrainPremises stated = premises;
    return countAdmission(admitMarker(transfer, stated, now, output));
}
BusAdmission CanBusOwner::admitMarker(const C::SdoTransfer& transfer,
                                      const C::SdoDrainPremises& premises, uint64_t now,
                                      RequestId& output) noexcept {
    if (entered_ || !transport_.transmit || now < lastUs_ ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &transfer, sizeof transfer))
        return BusAdmission::INVALID;
    if (inhibited_)
        return BusAdmission::INHIBITED;
    const int c = channel(transfer.channel);
    if (c < 0)
        return BusAdmission::INVALID;
    if (active_[c] >= 0 || external_[c].pending || external_[c].blocked || nmtBlocks(transfer.channel.binding))
        return BusAdmission::INHIBITED;
    if (resync_[c].reuse.policy == SdoReusePolicy::NONE ||
        !services_.checkDrain(uint8_t(c), transfer, premises, now))
        return BusAdmission::INVALID;
    const BusAdmission placed = placeOutside(uint8_t(c), transfer, now, output);
    if (placed == BusAdmission::ACCEPTED) {
        slots_[output.slot].premises = premises;
        resync_[c].pending = false; // Any further marker is the application's explicit intent.
        resync_[c].mismatch = false; // A value check belongs to the marker whose terminal set it.
    }
    return placed;
}
bool CanBusOwner::setSdoReuse(const Binding& binding, const SdoReuse& reuse, uint64_t now) noexcept {
    const int c = channel(binding);
    const auto valid = [](const SdoMarkerObject& object) {
        return object.index && object.size >= 1 && object.size <= 4 &&
               (!object.expectedKnown || object.size == 4 ||
                object.expectedValue < (uint32_t(1) << (8 * object.size)));
    };
    if (entered_ || c < 0 || now < lastUs_ || overlap(&reuse, sizeof reuse, this, sizeof *this) ||
        reuse.policy > SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER || !reuse.markerCount ||
        reuse.markerCount > SdoReuse::MARKER_OBJECTS)
        return false;
    for (unsigned i = 0; i < reuse.markerCount; ++i) {
        if (!valid(reuse.markers[i]))
            return false;
        for (unsigned k = 0; k < i; ++k)
            if (reuse.markers[k].index == reuse.markers[i].index &&
                reuse.markers[k].subindex == reuse.markers[i].subindex)
                return false;
    }
    resync_[c].reuse = reuse;
    if (reuse.policy == SdoReusePolicy::NONE)
        resync_[c].pending = false;
    lastUs_ = now;
    refreshHealth(now);
    return true;
}
bool CanBusOwner::sdoReuse(const Binding& binding, SdoReuse& output) const noexcept {
    const int c = channel(binding);
    if (c < 0 || overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &binding, sizeof binding))
        return false;
    output = resync_[c].reuse;
    return true;
}
bool CanBusOwner::reacceptReuse(const Binding& binding, uint64_t now) noexcept {
    const int c = channel(binding);
    if (entered_ || c < 0 || now < lastUs_ || resync_[c].reuse.policy == SdoReusePolicy::NONE ||
        !services_.reacceptReuse(uint8_t(c)))
        return false;
    lastUs_ = now;
    resync_[c].refusedSeen = false;
    resync_[c].stopped = false; // This call is the explicit intent a stop's route waits for.
    if (services_.unresolved(uint8_t(c)))
        armResync(unsigned(c));
    resyncTrace(unsigned(c), ResyncStep::REUSE_ACCEPTED, now);
    refreshHealth(now);
    return true;
}
bool CanBusOwner::setTxLifetime(uint64_t lifetime, uint64_t now) noexcept {
    if (entered_ || now < lastUs_)
        return false;
    txLifetimeUs_ = lifetime;
    lastUs_ = now;
    return true;
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
    // Stops keep priority over repair: the owner's own markers on this node give way, queued or
    // in flight, but only once placement cannot fail; anything else in the slot is kept.
    if (lastId_ != UINT32_MAX &&
        (!slots_[REQUESTS + c].occupied || slots_[REQUESTS + c].automaticMarker))
        for (unsigned k = 0; k < C::SdoClient::CHANNELS; ++k) {
            const auto* route = services_.channel(uint8_t(k));
            const Slot& held = slots_[REQUESTS + k];
            if (route && peer(route->binding, transfer.channel.binding) && held.occupied &&
                !held.terminal && held.automaticMarker) {
                lastUs_ = now;
                cancelSlot(REQUESTS + k, CompletionReason::CANCELLED, now);
            }
        }
    // Live work on the channel settles first, and dispatch rechecks all of the above.
    const BusAdmission placed = placeOutside(uint8_t(c), transfer, now, output);
    if (placed == BusAdmission::ACCEPTED) {
        slots_[output.slot].bestEffortStop = true;
        // The stop's own reply may still come and could confirm a later write of 6040, so no
        // policy reopens this node's routes by itself any more: that is explicit intent only.
        for (unsigned k = 0; k < C::SdoClient::CHANNELS; ++k) {
            const auto* route = services_.channel(uint8_t(k));
            if (route && peer(route->binding, transfer.channel.binding)) {
                resync_[k].pending = false;
                resync_[k].stopped = true;
            }
        }
    }
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
    candidate.completion.reusePolicy = resync_[c].reuse.policy;
    candidate.completion.acceptedCorrelation = candidate.completion.service.acceptedCorrelation =
        markActive(c);
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
    // Fail fast with nothing sent: its frame could only queue behind one nobody acknowledged.
    if (!urgent && noListener_)
        return BusAdmission::NO_LISTENER;
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
    candidate.completion.reusePolicy = resync_[c].reuse.policy;
    candidate.completion.acceptedCorrelation = candidate.completion.service.acceptedCorrelation =
        markActive(unsigned(c));
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
    if (slot.automaticMarker)
        markerTerminal(index, now);
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
        // The mark is the adapter's; the owner's admission copy can only add to it.
        slot.completion.acceptedCorrelation = slot.completion.service.acceptedCorrelation =
            slot.completion.acceptedCorrelation || e->acceptedCorrelation;
        // A quarantine with possible TX arms the binding's resync step: a timeout, local
        // cancellation (recover() cancels this way) and bus-off. A marker or a best-effort stop
        // never does: a further marker is the application's explicit intent.
        if (!slot.outsideSequence && e->transmissionPossible && services_.unresolved(uint8_t(c)) &&
            (e->outcome == C::SdoOutcome::TIMEOUT || e->outcome == C::SdoOutcome::CANCELLED ||
             e->outcome == C::SdoOutcome::BUS_OFF))
            armResync(c);
        recordTerminal(slot);
        services_.release(slot.native);
        active_[c] = -1;
        if (slot.outsideSequence && !slot.bestEffortStop)
            markerTerminal(unsigned(index), lastUs_);
    }
}
bool CanBusOwner::markerCurrent(const Slot& slot) const noexcept {
    const int c = channel(slot.request.transfer.channel);
    const auto* node = c < 0 ? nullptr : services_.node(uint8_t(c));
    return c >= 0 && (node ? node->route.generation : 0) == slot.completion.service.nodeGeneration;
}
// The marker's read proves nothing about ordering, but a configured value that differs proves
// that another drive answers on this route: invalidate the binding rather than reopen it.
void CanBusOwner::markerTerminal(unsigned index, uint64_t now) noexcept {
    Slot& slot = slots_[index];
    const unsigned c = index - REQUESTS; // Markers use the urgent slot of their channel.
    const C::SdoCompletion& e = slot.completion.service;
    Resync& r = resync_[c];
    if (slot.completion.reason == CompletionReason::SERVICE && e.outcome == C::SdoOutcome::SUCCESS) {
        uint32_t value = 0;
        for (unsigned i = 0; i < e.size && i < 4; ++i)
            value |= uint32_t(e.data[i]) << (8 * i);
        for (unsigned i = 0; i < r.reuse.markerCount; ++i) {
            const SdoMarkerObject* object = &r.reuse.markers[i];
            if (object->expectedKnown && object->index == e.transfer.index &&
                object->subindex == e.transfer.subindex && value != object->expectedValue) {
                resyncTrace(c, ResyncStep::MARKER_MISMATCH, now);
                // Retirement clears the settled marker, so the route cannot reopen. When the
                // adapter refuses this time, the reopening step retries it at the turn's time.
                r.mismatch = !services_.invalidate(uint8_t(c), now);
                break;
            }
        }
        // Until the reopening: a later route frame can still withdraw the settlement.
        r.settling = !r.mismatch && services_.drainSettled(uint8_t(c));
    } else if (slot.automaticMarker) {
        resyncTrace(c, ResyncStep::MARKER_FAILED, now);
    }
    // The owner's own marker has no other reader; free the urgent slot for a stop at once.
    if (slot.automaticMarker)
        slot.occupied = false;
}
bool CanBusOwner::markActive(unsigned c) const noexcept {
    // The adapter's mark: cleared by a new server session, not by a local controller event.
    return services_.acceptedCorrelation(uint8_t(c));
}
void CanBusOwner::reopenSettledRoutes(uint64_t now) noexcept {
    for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c) {
        // The marker's own frame must be retired first, or its duplicate could answer new work.
        if (!services_.drainSettled(uint8_t(c)) || external_[c].pending || external_[c].blocked)
            continue;
        const auto* route = services_.channel(uint8_t(c));
        if (!route)
            continue;
        if (resync_[c].mismatch) {
            resync_[c].mismatch = !services_.invalidate(uint8_t(c), now);
            continue;
        }
        // Otherwise sequence() would make an old reservation, and its old command
        // history, live again the moment the route reopens.
        for (Reservation& s : sequences_)
            if (s.occupied && peer(s.value.binding, route->binding))
                s.invalidated = true;
        if (services_.reopenAfterDrain(uint8_t(c))) {
            resync_[c].settling = resync_[c].stopped = false;
            resyncTrace(c, ResyncStep::REOPENED, now);
        }
    }
}
bool CanBusOwner::stopWaits(const Binding& binding) const noexcept {
    for (unsigned k = 0; k < C::SdoClient::CHANNELS; ++k) {
        const auto* route = services_.channel(uint8_t(k));
        const Slot& urgent = slots_[REQUESTS + k];
        if (route && peer(route->binding, binding) && urgent.occupied && !urgent.terminal &&
            urgent.bestEffortStop)
            return true;
    }
    return false;
}
bool CanBusOwner::cleanupStaged(uint8_t c) const noexcept {
    const auto* port = services_.port(c);
    return port && port->queueOccupied && port->queued.data[0] == 0x80 &&
           (active_[c] < 0 || slots_[active_[c]].terminal);
}
void CanBusOwner::armResync(unsigned c) noexcept {
    Resync& r = resync_[c];
    if (r.reuse.policy == SdoReusePolicy::NONE || r.stopped)
        return;
    const auto* node = services_.node(uint8_t(c));
    r.pending = true;
    r.sessions = node ? node->serverSessions : 0;
}
// The one resync step of a timed-out route, under its binding's policy. Waiting sends nothing,
// so it is no retry; the step itself happens at most once per arming.
void CanBusOwner::resyncRoutes(uint64_t now) noexcept {
    C::SdoDrainPremises accepted; // Stated by the policy as an accepted risk (Q18), not established.
    accepted.soleClient = accepted.orderedServer = true;
    for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c) {
        Resync& r = resync_[c];
        if (!r.pending)
            continue;
        const auto* route = services_.channel(uint8_t(c));
        const auto* node = services_.node(uint8_t(c));
        // A new server session, an explicit reopening or refused reuse ends the step unused.
        if (!route || r.reuse.policy == SdoReusePolicy::NONE || r.stopped ||
            !services_.unresolved(uint8_t(c)) || services_.reuseRefused(uint8_t(c)) ||
            services_.drainSettled(uint8_t(c)) || (node && node->serverSessions != r.sessions)) {
            r.pending = false;
            continue;
        }
        // The request's frame and its abort must be locally retired, the controller usable and
        // no stop waiting on the node: stops keep priority over every repair step.
        if (inhibited_ || active_[c] >= 0 || !localTxRetired(uint8_t(c)) ||
            nmtBlocks(route->binding) || stopWaits(route->binding))
            continue;
        if (r.reuse.policy == SdoReusePolicy::ACCEPTED_RISK_WITHOUT_MARKER) {
            if (services_.settleWithoutMarker(uint8_t(c), accepted, now)) {
                r.pending = false;
                reopenSettledRoutes(now);
            }
            continue;
        }
        if (slots_[REQUESTS + c].occupied)
            continue; // An unread stop or marker result; nothing is sent meanwhile.
        // The configured marker budgets, or else those the application chose for the route's
        // last request; no deadline of earlier work is extended.
        const uint32_t queueUs = r.reuse.markerQueueTimeoutUs ? r.reuse.markerQueueTimeoutUs
                                                              : r.queueTimeoutUs;
        const uint32_t responseUs = r.reuse.markerResponseTimeoutUs
                                        ? r.reuse.markerResponseTimeoutUs
                                        : r.responseTimeoutUs;
        if (!queueUs || !responseUs || lastId_ == UINT32_MAX) {
            // No budget configured or known for this route (the owner invents none) or no
            // identity left: report and end the step; admitDrainMarker remains.
            r.pending = false;
            resyncTrace(c, ResyncStep::MARKER_UNAVAILABLE, now);
            continue;
        }
        C::SdoTransfer marker;
        marker.channel = *route;
        marker.operation = marker.request = marker.step = lastId_ + 1; // Host-only identities.
        marker.direction = C::SdoDirection::UPLOAD;
        marker.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
        marker.uploadSize = C::SdoUploadSize::EXACT;
        marker.allowSegmentedUpload = false;
        marker.queueTimeoutUs = queueUs;
        marker.responseTimeoutUs = responseUs;
        const uint64_t budget = uint64_t(queueUs) + responseUs;
        marker.deadlineUs = now > UINT64_MAX - budget ? UINT64_MAX : now + budget;
        bool ready = false;
        // The first listed object that is neither the last completed nor a retained pair.
        for (unsigned i = 0; i < r.reuse.markerCount && !ready; ++i) {
            marker.index = r.reuse.markers[i].index;
            marker.subindex = r.reuse.markers[i].subindex;
            marker.size = r.reuse.markers[i].size;
            ready = services_.checkDrain(uint8_t(c), marker, accepted, now);
        }
        if (!ready && services_.bestEffortEligible(uint8_t(c), now)) {
            // Every object is refused while the controller and node state would allow a read:
            // they are retained or last completed pairs. Report it rather than wait silently.
            r.pending = false;
            resyncTrace(c, ResyncStep::MARKER_UNAVAILABLE, now);
            continue;
        }
        RequestId id;
        if (!ready || placeOutside(uint8_t(c), marker, now, id) != BusAdmission::ACCEPTED)
            continue;
        Slot& slot = slots_[id.slot];
        slot.premises = accepted;
        slot.automaticMarker = slot.completion.automaticMarker = true;
        r.pending = r.mismatch = false;
        detail::add(stats_.markers);
        resyncTrace(c, ResyncStep::MARKER_ADMITTED, now);
    }
}
void CanBusOwner::noteReuse() noexcept {
    for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c) {
        const bool refused = services_.reuseRefused(uint8_t(c));
        if (refused && !resync_[c].refusedSeen) {
            resync_[c].refusedSeen = true;
            resync_[c].pending = false;
            detail::add(stats_.reuseStopped);
            resyncTrace(c, ResyncStep::REUSE_STOPPED, lastUs_);
        } else if (!refused) {
            resync_[c].refusedSeen = false;
        }
        if (resync_[c].settling && !services_.drainSettled(uint8_t(c))) {
            // Route traffic after the marker's success withdrew it before the reopening. A
            // refusal was reported above; otherwise the marker counts as failed.
            resync_[c].settling = resync_[c].pending = false;
            if (!refused && services_.unresolved(uint8_t(c)))
                resyncTrace(c, ResyncStep::MARKER_FAILED, lastUs_);
        }
    }
}
void CanBusOwner::resyncTrace(unsigned c, ResyncStep step, uint64_t now) noexcept {
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::RESYNC;
    diagnostic.channel = uint8_t(c);
    diagnostic.resync = step;
    diagnostic.processedUs = now;
    diagnostic.controllerEpoch = controllerEpoch_;
    trace(diagnostic);
}
// An accepted frame with no final report within the lifetime may still leave at any time
// (no-ACK retransmission has no limit). Report it once and enter NO_LISTENER; only an
// explicit recovery retires it, as possibly sent.
void CanBusOwner::expireHandoffs(uint64_t now) noexcept {
    // A pending recovery is retiring every handoff already, so none is reported then.
    if (!txLifetimeUs_ || (recoveryOccupied_ && !recoveryTerminal_))
        return;
    ExternalTx* handoffs[C::SdoClient::CHANNELS + 2];
    for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c)
        handoffs[c] = &external_[c];
    handoffs[C::SdoClient::CHANNELS] = &nmtExternal_;
    handoffs[C::SdoClient::CHANNELS + 1] = &syncExternal_;
    for (ExternalTx* tx : handoffs) {
        if (!tx->pending || tx->rejected || tx->expired)
            continue;
        // The lifetime runs from the SDK submission an adapter reports, otherwise from the
        // owner's handoff. A frame an adapter still holds behind another has not started.
        uint64_t start = tx->handoffUs;
        if (tx->id.kind == TransportTxId::Kind::SDO ? !services_.txStart(tx->id.sdo, start)
            : tx->id.kind == TransportTxId::Kind::NMT ? !services_.nmtTxStart(tx->id.nmt, start)
                                                      : syncHeld_)
            continue;
        if (now < start || now - start < txLifetimeUs_)
            continue;
        tx->expired = true;
        causes_.txExpired = true;
        detail::add(stats_.txExpired);
        Diagnostic diagnostic;
        diagnostic.kind = DiagnosticKind::TX_EXPIRED;
        diagnostic.txId = tx->id;
        diagnostic.processedUs = now;
        diagnostic.controllerEpoch = tx->epoch;
        trace(diagnostic);
        listener(true, now);
    }
}
void CanBusOwner::listener(bool absent, uint64_t now) noexcept {
    if (noListener_ == absent)
        return;
    noListener_ = absent;
    if (absent)
        noListenerSinceUs_ = now;
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::LISTENER;
    diagnostic.noListener = absent;
    diagnostic.processedUs = now;
    diagnostic.controllerEpoch = controllerEpoch_;
    trace(diagnostic);
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
    expireSync(now);
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
    // Ordinary work already queued waits out NO_LISTENER under its own queue cutoff.
    if (noListener_ && index < REQUESTS)
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
    // Priority changes scheduling between reservations, never same-axis intent order. Stops and
    // other urgent work go before drain markers: stops keep priority over every repair step.
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned i = REQUESTS; i < TOTAL_REQUESTS; ++i)
            if ((slots_[i].outsideSequence && !slots_[i].bestEffortStop) == (pass == 1) &&
                eligible(i, now))
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
    external_[c].handoffUs = now;
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
                (report.id.kind != TransportTxId::Kind::SDO && report.id.kind != TransportTxId::Kind::NMT &&
                 report.id.kind != TransportTxId::Kind::SYNC) ||
                (report.id.kind == TransportTxId::Kind::SDO &&
                 report.id.sdo.request.channel >= C::SdoClient::CHANNELS))
                continue;
            ExternalTx& tx = report.id.kind == TransportTxId::Kind::SYNC ? syncExternal_
                : report.id.kind == TransportTxId::Kind::NMT ? nmtExternal_
                : external_[report.id.sdo.request.channel];
            if (!tx.pending || !sameTransportTxId(tx.id, report.id))
                continue;
            if (report.transmitted) {
                detail::add(stats_.txFrames);
                listener(false, now); // Some node acknowledged this frame: controller evidence only.
            } else if (!tx.rejected)
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
            if (report.id.kind == TransportTxId::Kind::SYNC) {
                if (validSync(report.id.sync) && !syncTerminal_) {
                    if (report.transmitted) {
                        sync_.service.transmissionPossible = true;
                        sync_.service.transmissionConfirmed = true;
                        finishSync(C::SyncOutcome::LOCAL_TX, now);
                    } else if (report.provedUnsent) {
                        sync_.service.transmissionPossible = false;
                        finishSync(C::SyncOutcome::REFUSED, now);
                    } else {
                        sync_.service.transmissionPossible = true;
                        finishSync(C::SyncOutcome::TRANSPORT_ERROR, now);
                    }
                }
            } else if (report.id.kind == TransportTxId::Kind::NMT) {
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
            const bool current = frame.controllerEpoch == controllerEpoch_ &&
                MotorControlCAN::validateFrame(frame.frame) &&
                !(frame.receivedTimeKnown && frame.receiveBoundsKnown) &&
                (!frame.receivedTimeKnown || frame.receivedUs <= now) &&
                (!frame.receiveBoundsKnown || (frame.receivedEarliestUs <= frame.receivedLatestUs &&
                                               frame.receivedLatestUs <= now));
            if (current) {
                // SYNC observation must not consume a configured native route.
                // A colliding heartbeat/PDO still carries its ordinary lifecycle
                // or sample evidence while the local producer stays inhibited.
                const bool syncObserved = observeSync(frame, now);
                if (frame.receivedTimeKnown)
                    accepted = services_.receive(frame.frame, frame.receivedUs, now);
                else if (frame.receiveBoundsKnown)
                    accepted = services_.receive(frame.frame, frame.receivedEarliestUs,
                                                 frame.receivedLatestUs, now);
                else
                    accepted = services_.receive(frame.frame, now);
                accepted = accepted || syncObserved;
                if (!accepted)
                    detail::add(stats_.unmatchedFrames);
            } else
                detail::add(stats_.rxDropped);
            refreshHealth(now);
            if (current) {
                // Another node transmits: controller evidence only. A frame whose capture is not
                // known to follow the start of NO_LISTENER cannot end it.
                const bool before = frame.receivedTimeKnown
                    ? frame.receivedUs <= noListenerSinceUs_
                    : frame.receiveBoundsKnown && frame.receivedEarliestUs <= noListenerSinceUs_;
                if (!before)
                    listener(false, now);
                observePresence(frame, accepted, now);
            }
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
    // Reports and receptions of this turn come first: a report taken now prevents expiry.
    expireHandoffs(now);
    noteReuse();
    const uint8_t tx = limit(budget.tx, TX_LIMIT);
    const uint8_t startLimit = limit(budget.starts, START_LIMIT);
    unsigned sent = 0, starts = 0;
    if (sent < tx && transmitNmt(now)) {
        ++sent;
        collect();
        expire(now);
        settleExternal(now, budget.events != 0);
    }
    if (sent < tx && transmitSync(now)) {
        ++sent;
        settleExternal(now, budget.events != 0);
    }
    // A best-effort stop drops an unsent cleanup abort on its own channel: the abort settles
    // nothing and stops keep priority over every repair step. One already handed off stays.
    for (unsigned k = 0; k < C::SdoClient::CHANNELS; ++k) {
        const Slot& stop = slots_[REQUESTS + k];
        if (stop.occupied && !stop.terminal && !stop.submitted && stop.bestEffortStop)
            services_.dropCleanup(uint8_t(k));
    }
    // A cleanup abort waits while a stop or other urgent work can start this turn, so the stop
    // reaches the controller queue first; the abort follows if budget remains.
    bool urgentReady = false;
    for (unsigned i = REQUESTS; startLimit && i < TOTAL_REQUESTS && !urgentReady; ++i)
        urgentReady = !(slots_[i].outsideSequence && !slots_[i].bestEffortStop) && eligible(i, now);
    bool deferred[C::SdoClient::CHANNELS] = {};
    const uint8_t firstChannel = nextChannel_;
    for (unsigned scan = 0; scan < C::SdoClient::CHANNELS && sent < tx; ++scan) {
        const uint8_t c = uint8_t((firstChannel + scan) % C::SdoClient::CHANNELS);
        if (urgentReady && cleanupStaged(c)) {
            deferred[c] = true;
            continue;
        }
        if (transmit(c, now)) {
            ++sent;
            nextChannel_ = uint8_t((c + 1) % C::SdoClient::CHANNELS);
        }
    }
    // The resync step admits a marker only when it can start in this same turn.
    // Not in the turn a recovery retired: an application that sees the closure can admit a
    // held stop before the next turn, and stops keep priority over every repair step. Not
    // during NO_LISTENER either: on an absent bus an automatic marker would be one more frame
    // nobody acknowledges; an explicit marker stays the application's probe that can end it.
    if (!inhibited_ && !retiredThisTurn_ && !noListener_ && sent < tx && startLimit)
        resyncRoutes(now);
    while (!inhibited_ && sent < tx && starts < startLimit) {
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
        slot.completion.acceptedCorrelation |= markActive(unsigned(c));
        if (!slot.outsideSequence) {
            // A later resync marker uses the budgets the application chose for this route.
            resync_[c].queueTimeoutUs = slot.request.transfer.queueTimeoutUs;
            resync_[c].responseTimeoutUs = slot.request.transfer.responseTimeoutUs;
        }
        active_[c] = index;
        services_.service(now);
        collect();
        if (transmit(uint8_t(c), now))
            ++sent;
    }
    for (unsigned c = 0; c < C::SdoClient::CHANNELS && sent < tx; ++c)
        if (deferred[c] && transmit(uint8_t(c), now))
            ++sent;
    reopenSettledRoutes(now);
    noteReuse();
    refreshHealth(now);
    retiredThisTurn_ = false; // Resync steps are eligible again from the next turn.
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
    return id.kind == TransportTxId::Kind::SYNC ? canTransmitSync(id.sync, now) :
        id.kind == TransportTxId::Kind::NMT ? services_.canTransmitNmt(id.nmt, now) :
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
    settle(syncExternal_, now, allowCallback);
}
bool CanBusOwner::validSync(SyncRequestId id) const noexcept {
    return syncOccupied_ && id.owner == this && id.generation &&
           id.generation == sync_.id.generation;
}
bool CanBusOwner::syncRoutesCompatible() const noexcept {
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        const auto* node = services_.node(uint8_t(i));
        if ((route && (route->clientToServerId == 0x80 || route->serverToClientId == 0x80)) ||
            (node && (node->route.heartbeatId == 0x80 || node->route.emcyId == 0x80))) return false;
    }
    for (unsigned i = 0; i < C::SdoClient::PDO_ROUTES; ++i) {
        const auto* record = services_.pdo(uint8_t(i));
        if (record && !record->map.route.extended && record->map.route.cobId == 0x80) return false;
    }
    return true;
}
bool CanBusOwner::armSync(uint32_t cobId, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || inhibited_ || !identity_ || !transport_.transmit ||
        cobId != 0x80 || syncArmGeneration_ == UINT32_MAX || syncConflict_ || syncOccupied_ || syncExternal_.pending ||
        syncExternal_.blocked || !syncRoutesCompatible() ||
        services_.controllerState() == C::ControllerState::BUS_OFF ||
        services_.controllerState() == C::ControllerState::RECOVERING) return false;
    lastUs_ = now;
    ++syncArmGeneration_;
    syncArmed_ = true;
    return true;
}
bool CanBusOwner::disarmSync(uint64_t now) noexcept {
    if (entered_ || now < lastUs_) return false;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    expireSync(now);
    syncArmed_ = false;
    syncDisarmOutcome_ = C::SyncOutcome::CANCELLED;
    finishSync(C::SyncOutcome::CANCELLED, now);
    settleExternal(now, true);
    return true;
}
bool CanBusOwner::resolveSyncConflict(uint32_t evidenceId, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || !evidenceId || syncArmed_ || syncOccupied_ ||
        syncExternal_.pending || syncExternal_.blocked) return false;
    lastUs_ = now;
    syncResolutionEvidence_ = evidenceId;
    syncConflict_ = false;
    return true;
}
BusAdmission CanBusOwner::admitSync(const C::SyncWork& work, uint64_t now,
                                  SyncRequestId& output) noexcept {
    if (entered_ || now < lastUs_ || !identity_ || !transport_.transmit || !work.available ||
        !work.generation || work.dueUs > now || work.expiresUs <= work.dueUs ||
        !C::checkSyncFrame(work.frame, 0x80) ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_) ||
        overlap(&output, sizeof output, &work, sizeof work)) return BusAdmission::INVALID;
    if (now >= work.expiresUs) return BusAdmission::EXPIRED;
    if (!syncArmed_ || syncConflict_ || inhibited_ || syncExternal_.pending ||
        syncExternal_.blocked || !syncRoutesCompatible()) return BusAdmission::INHIBITED;
    if (syncOccupied_) return BusAdmission::RESULTS_FULL;
    if (lastId_ == UINT32_MAX) return BusAdmission::IDS_EXHAUSTED;
    SyncTerminal candidate;
    candidate.id.owner = this;
    candidate.id.generation = ++lastId_;
    candidate.work = work;
    candidate.service.generation = work.generation;
    candidate.service.slot = work.slot;
    candidate.admittedUs = now;
    sync_ = candidate;
    syncOccupied_ = true;
    syncTerminal_ = syncHanded_ = syncHeld_ = false;
    lastUs_ = now;
    output = candidate.id;
    return BusAdmission::ACCEPTED;
}
void CanBusOwner::finishSync(C::SyncOutcome outcome, uint64_t now) noexcept {
    if (!syncOccupied_ || syncTerminal_) return;
    sync_.service.outcome = outcome;
    sync_.completedUs = now;
    syncTerminal_ = true;
    if (outcome != C::SyncOutcome::LOCAL_TX) {
        syncArmed_ = false;
        syncDisarmOutcome_ = outcome;
    }
}
void CanBusOwner::expireSync(uint64_t now) noexcept {
    if (!syncOccupied_ || syncTerminal_) return;
    if (now >= sync_.work.expiresUs) finishSync(C::SyncOutcome::MISSED_DEADLINE, now);
    else if (!syncRoutesCompatible()) finishSync(C::SyncOutcome::INVALIDATED, now);
}
bool CanBusOwner::canTransmitSync(SyncRequestId id, uint64_t now) const noexcept {
    return validSync(id) && !syncTerminal_ && syncArmed_ && !syncConflict_ && !inhibited_ &&
           now >= lastUs_ && now >= sync_.work.dueUs && now < sync_.work.expiresUs &&
           syncRoutesCompatible();
}
bool CanBusOwner::syncTxPriorityReady() const noexcept {
    for (unsigned i = REQUESTS; i < TOTAL_REQUESTS; ++i)
        if (slots_[i].occupied && !slots_[i].terminal) return false;
    return true;
}
bool CanBusOwner::holdSyncHandoff(SyncRequestId id) noexcept {
    if (!syncHandoffCallback_ || !validSync(id) || syncTerminal_ || !syncHanded_ || !syncExternal_.pending ||
        syncHeld_) return false;
    syncHeld_ = true;
    sync_.service.transmissionPossible = false;
    return true;
}
bool CanBusOwner::noteSyncSubmitted(SyncRequestId id, uint64_t now) noexcept {
    if (entered_ || !canTransmitSync(id, now) || !syncHanded_ || !syncHeld_ ||
        !syncTxPriorityReady()) return false;
    lastUs_ = now;
    syncHeld_ = false;
    sync_.service.transmissionPossible = true;
    syncExternal_.handoffUs = now; // The TX lifetime starts at this submission.
    return true;
}
bool CanBusOwner::withdrawSyncUnsent(SyncRequestId id, uint64_t now) noexcept {
    if (entered_ || !validSync(id) || now < lastUs_ || syncTerminal_) return false;
    lastUs_ = now;
    expireSync(now);
    if (syncTerminal_) return false;
    sync_.service.transmissionPossible = false;
    finishSync(C::SyncOutcome::REFUSED, now);
    return true;
}
bool CanBusOwner::transmitSync(uint64_t now) noexcept {
    if (!canTransmitSync(sync_.id, now) || syncHanded_ || !syncTxPriorityReady() ||
        syncExternal_.pending || syncExternal_.blocked || !transport_.transmit) return false;
    syncHanded_ = true;
    sync_.handoffUs = now;
    sync_.service.transmissionPossible = true;
    syncExternal_ = ExternalTx();
    syncExternal_.pending = true;
    syncExternal_.epoch = controllerEpoch_;
    syncExternal_.handoffUs = now;
    syncExternal_.id = TransportTxId::fromSync(sync_.id);
    syncHandoffCallback_ = true;
    const bool accepted = transport_.transmit(transport_.context, sync_.work.frame,
                                             syncExternal_.id, controllerEpoch_);
    syncHandoffCallback_ = false;
    syncExternal_.rejected = !accepted;
    if (!accepted) {
        detail::add(stats_.txRejected);
        finishSync(C::SyncOutcome::TRANSPORT_ERROR, now);
    }
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::TX;
    diagnostic.frame = sync_.work.frame;
    diagnostic.txId = syncExternal_.id;
    diagnostic.accepted = accepted;
    diagnostic.processedUs = now;
    diagnostic.controllerEpoch = controllerEpoch_;
    trace(diagnostic);
    return true;
}
bool CanBusOwner::observeSync(const ReceivedFrame& frame, uint64_t now) noexcept {
    const Frame& f = frame.frame;
    if (f.id != 0x80 || f.extended || f.rtr || f.fd || f.dlc > 1) return false;
    syncObservation_.present = true;
    if (syncObservation_.serial != UINT32_MAX) ++syncObservation_.serial;
    syncObservation_.frame = f;
    syncObservation_.controllerEpoch = frame.controllerEpoch;
    C::ObservationTime time;
    time.present = true;
    time.receivedTimeKnown = frame.receivedTimeKnown;
    time.receivedUs = frame.receivedUs;
    time.receiveBoundsKnown = frame.receiveBoundsKnown;
    time.receivedEarliestUs = frame.receivedEarliestUs;
    time.receivedLatestUs = frame.receivedLatestUs;
    time.processedUs = now;
    time.generation = controllerEpoch_;
    syncObservation_.time = time;
    syncConflict_ = true;
    syncArmed_ = false;
    syncDisarmOutcome_ = C::SyncOutcome::PRODUCER_CONFLICT;
    finishSync(C::SyncOutcome::PRODUCER_CONFLICT, now);
    return true;
}
bool CanBusOwner::cancelSync(SyncRequestId id, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || !validSync(id)) return false;
    const detail::EnteredScope entered(entered_);
    lastUs_ = now;
    expireSync(now);
    const bool won = !syncTerminal_;
    if (won) finishSync(C::SyncOutcome::CANCELLED, now);
    settleExternal(now, true);
    return won;
}
const SyncTerminal* CanBusOwner::syncResult(SyncRequestId id) const noexcept {
    return validSync(id) && syncTerminal_ ? &sync_ : nullptr;
}
bool CanBusOwner::releaseSync(SyncRequestId id) noexcept {
    if (entered_ || !syncResult(id)) return false;
    syncOccupied_ = false;
    return true;
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
    nmtExternal_.handoffUs = now;
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
    notePossible();
    for (unsigned i = 0; i < TOTAL_REQUESTS; ++i)
        cancelSlot(i, CompletionReason::RECOVERY, now);
    for (Reservation& s : sequences_)
        s.invalidated = true;
    syncArmed_ = false;
    syncDisarmOutcome_ = C::SyncOutcome::INVALIDATED;
    expireSync(now);
    finishSync(C::SyncOutcome::INVALIDATED, now);
    services_.reportController(C::ControllerState::RECOVERING, now);
    collect();
    recovery_ = RecoveryCompletion();
    recovery_.id.owner = this;
    recovery_.id.generation = ++lastId_;
    recovery_.controllerEpoch = ++controllerEpoch_;
    recovery_.startedUs = now;
    recovery_.deadlineUs = deadline;
    // This recovery owns the causes so far; one reported while it runs stays for the next.
    recovery_.causes = causes_;
    causes_ = RecoveryCauses();
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
    diagnostic.recoveryCauses = recovery_.causes;
    trace(diagnostic);
    if (outcome != RecoveryOutcome::RETIRED) {
        // Nothing was retired, so the causes stay for the next recovery.
        causes_.busOff |= recovery_.causes.busOff;
        causes_.receiveLost |= recovery_.causes.receiveLost;
        causes_.txExpired |= recovery_.causes.txExpired;
    }
    if (outcome == RecoveryOutcome::RETIRED) {
        detail::add(stats_.recoveries);
        // Expired handoffs too: the old node is gone, so nothing can leave later, but each
        // stays possibly sent, and recover()'s RECOVERING report quarantined its route. The
        // adapter learns of the retirement as from a final report; nothing settles remotely.
        for (ExternalTx& tx : external_) {
            if (tx.pending && tx.id.kind == TransportTxId::Kind::SDO)
                services_.noteTxRetired(tx.id.sdo);
            tx = ExternalTx();
        }
        nmtExternal_ = ExternalTx();
        syncExternal_ = ExternalTx();
        services_.reportController(C::ControllerState::AVAILABLE, now);
        inhibited_ = false;
        retiredThisTurn_ = true;
        // Every route the controller event left unresolved with possible TX gets its policy's
        // one resync step, so the routes of a restarted interface resync (not after a stop).
        for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c) {
            if (resync_[c].possible && services_.unresolved(uint8_t(c)) &&
                !services_.reuseRefused(uint8_t(c)))
                armResync(c);
            resync_[c].possible = false;
        }
    }
}
// Sampled before a controller event retires local work: which routes may have a frame on the
// bus, whether or not an owner terminal will record it.
void CanBusOwner::notePossible() noexcept {
    for (unsigned c = 0; c < C::SdoClient::CHANNELS; ++c)
        resync_[c].possible = resync_[c].possible || external_[c].pending || external_[c].blocked ||
            (active_[c] >= 0 && services_.transmissionPossible(slots_[active_[c]].native));
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
    if (state == C::ControllerState::BUS_OFF)
        causes_.busOff = true;
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::CONTROLLER;
    diagnostic.controller = state;
    diagnostic.controllerEpoch = controllerEpoch_;
    diagnostic.processedUs = now;
    trace(diagnostic);
    if (state != C::ControllerState::AVAILABLE) {
        notePossible();
        expireSync(now);
        syncArmed_ = false;
        syncDisarmOutcome_ = state == C::ControllerState::BUS_OFF ? C::SyncOutcome::BUS_OFF
                                                                : C::SyncOutcome::INVALIDATED;
        finishSync(state == C::ControllerState::BUS_OFF ? C::SyncOutcome::BUS_OFF
                                                       : C::SyncOutcome::INVALIDATED, now);
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
    causes_.receiveLost = true;
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
