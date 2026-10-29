// SPDX-License-Identifier: MIT
#include "fit_proof.h"
#include <cstring>
#include <limits>

namespace CanopenFit {
namespace C = MotorControlCAN::CANOPEN;

CanBusOwner::CanBusOwner() {
    channel_.binding.logicalAxisId = 1;
    channel_.binding.endpoint.profile = 1;
    channel_.binding.endpoint.address = 1;
    channel_.binding.bindingGeneration = 1;
    channel_.binding.configurationGeneration = 1;
    channel_.clientToServerId = 0x601;
    channel_.serverToClientId = 0x581;
    // This fake fixture starts with an explicitly qualified, fresh route.
    // Construction alone is not such evidence for a real controller/server.
    ready_ = client_.configure(0, channel_, true, true);
}

bool CanBusOwner::admit(bool download, uint16_t index, uint8_t subIndex,
                       const uint8_t* bytes, size_t size, uint64_t nowUs,
                       uint16_t timeoutMs, RequestId& id, size_t expectedUploadSize) {
    const uint64_t intervalUs = static_cast<uint64_t>(timeoutMs) * 1000;
    if (!ready_ || timeoutMs == 0 || nowUs < lastUs_ ||
        nowUs > std::numeric_limits<uint64_t>::max() - intervalUs ||
        (download ? (bytes == nullptr || size == 0 || size > 32)
                  : (size != 0 || expectedUploadSize == 0 || expectedUploadSize > 32)))
        return false;
    C::SdoTransfer work;
    work.channel = channel_;
    work.operation = work.request = work.step = 1;
    work.index = index;
    work.subindex = subIndex;
    work.direction = download ? C::SdoDirection::DOWNLOAD : C::SdoDirection::UPLOAD;
    work.size = static_cast<uint8_t>(download ? size : expectedUploadSize);
    work.deadlineUs = nowUs + intervalUs;
    work.queueTimeoutUs = work.responseTimeoutUs = static_cast<uint32_t>(intervalUs);
    if (download) std::memcpy(work.data, bytes, size);
    C::SdoRequestId admitted;
    if (!client_.admit(0, work, nowUs, admitted)) return false;
    request_ = admitted;
    completionReady_ = false;
    lastUs_ = nowUs;
    id.owner = this;
    id.serial = admitted.generation;
    return true;
}

bool CanBusOwner::matches(RequestId id) const {
    return id.owner == this && id.serial != 0 && id.serial == request_.generation;
}

void CanBusOwner::collect() {
    if (completionReady_) return;
    const C::SdoCompletion* source = client_.result(request_);
    if (source == nullptr) return;
    Completion candidate = {};
    switch (source->outcome) {
    case C::SdoOutcome::SUCCESS: candidate.outcome = Outcome::Success; break;
    case C::SdoOutcome::SERVER_ABORT: candidate.outcome = Outcome::ServerAbort; break;
    case C::SdoOutcome::TIMEOUT: candidate.outcome = Outcome::Timeout; break;
    case C::SdoOutcome::CANCELLED: candidate.outcome = Outcome::Cancelled; break;
    case C::SdoOutcome::CLIENT_ABORT:
    case C::SdoOutcome::UNSUPPORTED: candidate.outcome = Outcome::ClientAbort; break;
    case C::SdoOutcome::BUS_OFF:
    case C::SdoOutcome::REBOOT:
    case C::SdoOutcome::BINDING_INVALIDATED:
    case C::SdoOutcome::CORRELATION_LOST:
    case C::SdoOutcome::TRANSPORT_ERROR: candidate.outcome = Outcome::Transport; break;
    }
    candidate.stackReturn = static_cast<CO_SDO_return_t>(source->stackReturn);
    candidate.abortCode = source->abortCode;
    if (source->outcome == C::SdoOutcome::TIMEOUT) {
        // The retained experiment used this local diagnostic marker. Production
        // evidence keeps abortValid false because no peer abort was observed.
        candidate.stackReturn = CO_SDO_RT_endedWithClientAbort;
        candidate.abortCode = CO_SDO_AB_TIMEOUT;
    }
    candidate.executionUnknown = source->effectUncertain;
    candidate.size = source->size;
    std::memcpy(candidate.data, source->data, source->size);
    completion_ = candidate;
    completionReady_ = true;
}

void CanBusOwner::service(uint64_t nowUs) {
    if (nowUs < lastUs_) return;
    lastUs_ = nowUs;
    // Legacy failure injection is preflight-only: never pump a frame first.
    if (refuseSend_ && !completionReady_)
        client_.transportFailure(request_, nowUs);
    else
        client_.service(nowUs);
    collect();
}

void CanBusOwner::receive(const FitFrame& frame, uint64_t nowUs) {
    if (nowUs < lastUs_) return;
    service(nowUs);
    if (frame.extended || frame.rtr || frame.DLC > 8) return;
    if ((frame.ident == 0x701 && frame.DLC == 1) ||
        (frame.ident == 0x81 && frame.DLC == 8) ||
        (frame.ident == 0x181 && frame.DLC == 4)) {
        lastEvent_ = frame;
        if (frame.ident == 0x701) ++heartbeats_;
        else if (frame.ident == 0x81) ++emergencies_;
        else ++pdos_;
        return;
    }
    // Preserve the retained proof's envelope filter. The production adapter has
    // its own matching-route malformed-frame settlement tests.
    if (frame.DLC != 8) return;
    MotorControlCAN::Frame borrowed;
    borrowed.id = frame.ident;
    borrowed.dlc = borrowed.payloadLength = frame.DLC;
    std::memcpy(borrowed.data, frame.data, frame.DLC);
    client_.receive(borrowed, nowUs);
    collect();
}

bool CanBusOwner::handoff(uint64_t nowUs) {
    if (nowUs < lastUs_) return false;
    service(nowUs);
    MotorControlCAN::Frame frame;
    C::SdoTxId id;
    const bool handedOff = client_.nextTx(0, nowUs, frame, id);
    collect();
    return handedOff;
}

bool CanBusOwner::cancel(RequestId id) {
    if (!matches(id)) return false;
    const bool cancelled = client_.cancel(request_, lastUs_);
    collect();
    return cancelled;
}

const Completion* CanBusOwner::result(RequestId id) const {
    return matches(id) && completionReady_ && client_.result(request_) != nullptr
        ? &completion_ : nullptr;
}

bool CanBusOwner::release(RequestId id) {
    if (!matches(id) || !client_.release(request_)) return false;
    completionReady_ = false;
    return true;
}
} // namespace CanopenFit
