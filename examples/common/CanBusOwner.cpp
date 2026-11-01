// SPDX-License-Identifier: MIT
#include "CanBusOwner.h"
#include "OwnerCounters.h"

namespace MotorControlCANExample {
namespace C = MotorControlCAN::CANOPEN;
namespace {
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool same(SequenceId a, SequenceId b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
bool same(C::SdoRequestId a, C::SdoRequestId b) {
    return a.owner == b.owner && a.channel == b.channel && a.generation == b.generation;
}
bool same(C::SdoTxId a, C::SdoTxId b) {
    return same(a.request, b.request) && a.frame == b.frame;
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
      controllerEpoch_(controllerEpoch), inhibited_(!controllerEpoch) {}
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
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        if (route && peer(route->binding, s.binding) && services_.unresolved(uint8_t(i)))
            return nullptr;
    }
    return node && !node->exhausted && node->route.generation == s.nodeGeneration &&
                   !services_.unresolved(uint8_t(c))
               ? &s
               : nullptr;
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
    for (unsigned i = 0; i < C::SdoClient::CHANNELS; ++i) {
        const auto* route = services_.channel(uint8_t(i));
        if (route && peer(route->binding, binding) && services_.unresolved(uint8_t(i)))
            return false;
    }
    int free = -1;
    for (unsigned i = 0; i < SEQUENCES; ++i) {
        const Reservation& s = sequences_[i];
        if (s.occupied && (s.value.binding.logicalAxisId == binding.logicalAxisId ||
                           peer(s.value.binding, binding)))
            return false;
        if (!s.occupied && free < 0)
            free = int(i);
    }
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
bool CanBusOwner::releaseSequence(SequenceId id) noexcept {
    if (entered_ || !valid(id) || occupied(id))
        return false;
    sequences_[id.slot].occupied = false;
    return true;
}
BusAdmission CanBusOwner::admit(const BusRequest& request, uint64_t now,
                                RequestId& output) noexcept {
    const BusAdmission result = admit(request, now, output, false);
    if (!entered_ && result != BusAdmission::ACCEPTED) {
        detail::add(stats_.queueRejected);
        if (result == BusAdmission::RESULTS_FULL)
            detail::add(stats_.resultsRejected);
    }
    return result;
}
BusAdmission CanBusOwner::admitUrgent(const BusRequest& request, uint64_t now,
                                      RequestId& output) noexcept {
    const BusAdmission result = admit(request, now, output, true);
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
    if (c >= 0 && external_[c].blocked)
        return BusAdmission::INHIBITED;
    if (c < 0 || !MotorControlCAN::sameBinding(s->binding, request.transfer.channel.binding) ||
        request.transfer.operation != s->operation ||
        request.transfer.deadlineUs != s->deadlineUs ||
        request.transfer.step <= sequences_[request.sequence.slot].lastStep ||
        !services_.checkAdmission(uint8_t(c), request.transfer, now))
        return BusAdmission::INVALID;
    if (lastId_ == UINT32_MAX)
        return BusAdmission::IDS_EXHAUSTED;
    int free = -1;
    for (unsigned i = urgent ? REQUESTS : 0; i < (urgent ? TOTAL_REQUESTS : REQUESTS); ++i)
        if (!slots_[i].occupied) {
            free = int(i);
            break;
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
        recordTerminal(slot);
        services_.release(slot.native);
        active_[c] = -1;
    }
}
void CanBusOwner::expire(uint64_t now) noexcept {
    for (unsigned i = 0; i < TOTAL_REQUESTS; ++i) {
        const Slot& slot = slots_[i];
        if (!slot.occupied || slot.terminal)
            continue;
        if (slot.submitted) {
            if (!sequence(slot.request.sequence)) {
                slots_[i].completion.reason = CompletionReason::INVALIDATED;
                services_.invalidate(slot.native.channel, now);
                collect();
            }
        } else if (now >= slot.request.transfer.deadlineUs)
            finish(i, CompletionReason::DEADLINE, now);
        else if (!sequence(slot.request.sequence))
            finish(i, CompletionReason::INVALIDATED, now);
        else if (now >= slot.queueDeadlineUs)
            finish(i, CompletionReason::QUEUE_EXPIRED, now);
    }
}
bool CanBusOwner::eligible(unsigned index, uint64_t now) const noexcept {
    const Slot& slot = slots_[index];
    if (!slot.occupied || slot.terminal || slot.submitted || !sequence(slot.request.sequence))
        return false;
    const int c = channel(slot.request.transfer.channel);
    if (c < 0 || active_[c] >= 0 || external_[c].pending || external_[c].blocked ||
        !services_.checkAdmission(uint8_t(c), slot.request.transfer, now))
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
    if (!port || !port->queueOccupied)
        return false;
    const int index = active_[c];
    const bool cleanup = port->queued.data[0] == 0x80;
    if (index >= 0 && !cleanup) {
        Slot& slot = slots_[index];
        if (!slot.guarded && slot.request.transfer.direction == C::SdoDirection::DOWNLOAD) {
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
    external_[c].id = id;
    const bool accepted = transport_.transmit(transport_.context, frame, id, controllerEpoch_);
    external_[c].rejected = !accepted;
    if (!accepted)
        detail::add(stats_.txRejected);
    Diagnostic diagnostic;
    diagnostic.kind = DiagnosticKind::TX;
    diagnostic.frame = frame;
    diagnostic.txId = id;
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
    entered_ = true;
    if (!services_.service(now)) {
        entered_ = false;
        return false;
    }
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
                report.id.request.channel >= C::SdoClient::CHANNELS)
                continue;
            ExternalTx& tx = external_[report.id.request.channel];
            if (!tx.pending || !same(tx.id, report.id))
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
            if (report.transmitted)
                services_.confirmTx(report.id, now);
            else if (services_.canTransmit(report.id, now))
                services_.transportFailure(report.id.request, now);
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
                (!frame.receivedTimeKnown || frame.receivedUs <= now)) {
                if (frame.receivedTimeKnown)
                    accepted = services_.receive(frame.frame, frame.receivedUs, now);
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
        nextProducer_ = uint8_t(slot.completion.producer % PRODUCERS + 1);
        if (!services_.admit(uint8_t(c), slot.request.transfer, now, slot.native,
                             slot.queueDeadlineUs)) {
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
    refreshHealth(now);
    entered_ = false;
    return true;
}
const Completion* CanBusOwner::result(RequestId id) const noexcept {
    return valid(id) && slots_[id.slot].terminal ? &slots_[id.slot].completion : nullptr;
}
bool CanBusOwner::release(RequestId id) noexcept {
    if (entered_ || !valid(id) || !slots_[id.slot].terminal)
        return false;
    slots_[id.slot].occupied = false;
    return true;
}
void CanBusOwner::settleExternal(uint64_t now, bool allowCallback) noexcept {
    for (ExternalTx& tx : external_) {
        if (!tx.pending || services_.canTransmit(tx.id, now))
            continue;
        tx.blocked = true;
        if (!allowCallback || tx.retirementTried)
            continue;
        tx.retirementTried = true;
        if (transport_.retire && transport_.retire(transport_.context, tx.id, tx.epoch)) {
            tx = ExternalTx(); // Exact local frame and all its final reports are retired.
        }
    }
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
    entered_ = true;
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
    entered_ = false;
    return disposition;
}
bool CanBusOwner::invalidate(SequenceId id, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || !valid(id))
        return false;
    entered_ = true;
    lastUs_ = now;
    collect();
    for (unsigned i = 0; i < TOTAL_REQUESTS; ++i)
        if (slots_[i].occupied && same(slots_[i].request.sequence, id))
            cancelSlot(i, CompletionReason::INVALIDATED, now);
    sequences_[id.slot].invalidated = true;
    settleExternal(now, true);
    refreshHealth(now);
    entered_ = false;
    return true;
}
bool CanBusOwner::invalidateProducer(uint8_t producer, uint64_t now) noexcept {
    if (entered_ || now < lastUs_ || !producer || producer > PRODUCERS)
        return false;
    entered_ = true;
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
    entered_ = false;
    return true;
}
bool CanBusOwner::recover(uint64_t now, uint64_t deadline, RecoveryId& output) noexcept {
    if (entered_ || now < lastUs_ || deadline <= now || recoveryOccupied_ || !identity_ ||
        lastId_ == UINT32_MAX || !controllerEpoch_ || controllerEpoch_ == UINT32_MAX ||
        overlap(&output, sizeof output, this, sizeof *this) ||
        overlap(&output, sizeof output, &services_, sizeof services_))
        return false;
    entered_ = true;
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
    entered_ = false;
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
    entered_ = true;
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
    entered_ = false;
    return true;
}
bool CanBusOwner::receiveOverflow(uint64_t now, uint32_t droppedFrames) noexcept {
    if (!droppedFrames || !reportController(C::ControllerState::RECOVERING, now))
        return false;
    detail::add(stats_.rxDropped, droppedFrames);
    return true;
}
bool CanBusOwner::advanceConfiguration(SequenceId id, const Binding& current,
                                       uint64_t now) noexcept {
    if (entered_ || inhibited_ || now < lastUs_ || !sequence(id) || occupied(id))
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
    entered_ = true;
    const bool changed = services_.advanceConfiguration(uint8_t(c), s.value.binding, next, now);
    entered_ = false;
    if (!changed)
        return false;
    s.value.binding = next;
    lastUs_ = now;
    refreshHealth(now);
    return true;
}
} // namespace MotorControlCANExample
