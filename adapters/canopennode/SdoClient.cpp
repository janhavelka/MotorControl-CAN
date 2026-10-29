// SPDX-License-Identifier: MIT
#define OD_DEFINITION
#include "SdoClient.h"
#include <cstring>

namespace MotorControlCAN {
namespace CANOPEN {
namespace {
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
uint64_t cutoff(uint64_t now, uint64_t deadline, uint32_t interval) {
    const uint64_t remaining = deadline - now;
    return now + (remaining < interval ? remaining : interval);
}
Frame wire(const StackFrame& native) {
    Frame f;
    f.id = native.ident;
    f.dlc = f.payloadLength = native.DLC;
    f.extended = native.extended;
    f.rtr = native.rtr;
    if (!f.rtr && f.payloadLength <= 8) std::memcpy(f.data, native.data, f.payloadLength);
    return f;
}
StackFrame native(const Frame& f) {
    StackFrame n = {};
    n.ident = f.id;
    n.DLC = f.payloadLength;
    std::memcpy(n.data, f.data, f.payloadLength);
    return n;
}
uint32_t word(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 |
           uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}
SdoExpectation expectation(const SdoTransfer& work) {
    SdoExpectation out;
    out.channel = work.channel;
    out.index = work.index;
    out.subindex = work.subindex;
    out.width = work.size;
    out.direction = work.direction;
    return out;
}
} // namespace

bool SdoClient::configure(uint8_t slot, const SdoChannel& route, bool confirmed, bool fresh) {
    if (slot >= CHANNELS || channels_[slot].configured || !fresh || !validateSdoChannel(route))
        return false;
    if (channels_[slot].networkConfigured &&
        (!sameBinding(channels_[slot].network.route.binding, route.binding) ||
         channels_[slot].network.strictBoundary || channels_[slot].network.exhausted)) return false;
    for (const Channel& other : channels_) {
        if (other.networkConfigured) {
            const NetworkRoute& network = other.network.route;
            if (network.binding.endpoint.bus == route.binding.endpoint.bus &&
                network.binding.endpoint.address == route.binding.endpoint.address &&
                (other.network.strictBoundary || other.network.exhausted)) return false;
            if (network.binding.endpoint.bus != route.binding.endpoint.bus ||
                route.clientToServerId == network.heartbeatId || route.clientToServerId == network.emcyId ||
                route.serverToClientId == network.heartbeatId || route.serverToClientId == network.emcyId) return false;
        }
        if (!other.configured) continue;
        if (other.route.binding.endpoint.bus != route.binding.endpoint.bus ||
            (other.route.binding.endpoint.address == route.binding.endpoint.address &&
             other.route.binding.endpoint.channel == route.binding.endpoint.channel) ||
            other.route.clientToServerId == route.clientToServerId ||
            other.route.serverToClientId == route.serverToClientId ||
            other.route.clientToServerId == route.serverToClientId ||
            other.route.serverToClientId == route.clientToServerId) return false;
    }
    Channel& c = channels_[slot];
    uint8_t count = 3, node = static_cast<uint8_t>(route.binding.endpoint.address);
    uint32_t tx = route.clientToServerId, rx = route.serverToClientId;
    OD_obj_record_t fields[] = {{&count, 0, ODA_SDO_R, 1}, {&tx, 1, ODA_SDO_R, 4},
                               {&rx, 2, ODA_SDO_R, 4}, {&node, 3, ODA_SDO_R, 1}};
    OD_entry_t entry = {0x1280, 4, ODT_REC, fields, nullptr};
    // Dynamic/local OD and callback wakeups are disabled; no temporary OD pointer survives init.
    if (CO_SDOclient_init(&c.engine, nullptr, &entry, 0, &c.port, 0, &c.port, 0, nullptr) != CO_ERROR_NO) {
        c.engine = CO_SDOclient_t();
        c.port = CO_CANmodule_t(); // Preserve any passive route and borrowed cache observer.
        return false;
    }
    c.route = route;
    c.configured = true;
    c.confirmed = confirmed;
    return true;
}

bool SdoClient::valid(SdoRequestId id) const {
    return id.owner == this && id.channel < CHANNELS && channels_[id.channel].occupied &&
           id.generation != 0 && channels_[id.channel].generation == id.generation;
}

bool SdoClient::admit(uint8_t slot, const SdoTransfer& work, uint64_t now, SdoRequestId& id) {
    if (slot >= CHANNELS || overlap(&id, sizeof id, this, sizeof *this) ||
        overlap(&id, sizeof id, &work, sizeof work) || now < lastUs_) return false;
    Channel& c = channels_[slot];
    if (!c.configured || c.unresolved || c.occupied || c.cleanup || c.generation == UINT32_MAX ||
        !sameBinding(work.channel.binding, c.route.binding) ||
        work.channel.clientToServerId != c.route.clientToServerId ||
        work.channel.serverToClientId != c.route.serverToClientId ||
        work.operation == 0 || work.request == 0 || work.step == 0 || work.size == 0 || work.size > 32 ||
        work.deadlineUs <= now || work.queueTimeoutUs == 0 || work.responseTimeoutUs == 0 ||
        (work.direction != SdoDirection::UPLOAD && work.direction != SdoDirection::DOWNLOAD) ||
        (work.readEffect != SdoReadEffect::UNQUALIFIED &&
         work.readEffect != SdoReadEffect::REVIEWED_NON_CHANGING) ||
        (!c.confirmed && (work.direction != SdoDirection::UPLOAD ||
                         work.readEffect != SdoReadEffect::REVIEWED_NON_CHANGING))) return false;
    c.work = work;
    ++c.generation;
    c.occupied = true;
    c.terminal = c.started = c.awaiting = c.queued = c.possible = c.txConfirmed = false;
    c.unsizedExpedited = c.replyValid = c.handoffLive = false;
    c.uploaded = 0;
    c.frame = 0;
    c.queueCutoff = cutoff(now, work.deadlineUs, work.queueTimeoutUs);
    c.responseCutoff = work.deadlineUs;
    c.completion = SdoCompletion();
    c.port.sendFault = false;
    c.port.refuseSend = false;
    lastUs_ = now;
    id.owner = this;
    id.channel = slot;
    id.generation = c.generation;
    return true;
}

void SdoClient::retire(Channel& c) {
    c.port.queueOccupied = false;
    c.port.tx.bufferFull = false;
    c.cleanup = false;
    c.handoffLive = false;
}

void SdoClient::finish(Channel& c, SdoOutcome outcome, uint64_t now, int32_t returned,
                       bool abortValid, uint32_t abortCode) {
    if (!c.occupied || c.terminal) return;
    SdoCompletion out;
    out.transfer = c.work;
    out.outcome = outcome;
    out.completedUs = now;
    out.queued = c.queued;
    out.transmissionPossible = c.possible;
    out.transmissionConfirmed = c.txConfirmed;
    out.protocolAcknowledged = outcome == SdoOutcome::SUCCESS;
    out.effectPossible = c.possible &&
        (c.work.direction == SdoDirection::DOWNLOAD ||
         c.work.readEffect != SdoReadEffect::REVIEWED_NON_CHANGING);
    out.effectUncertain = out.effectPossible && outcome != SdoOutcome::SUCCESS;
    out.stackReturn = returned;
    out.abortValid = abortValid;
    out.abortCode = abortCode;
    out.reply = c.lastReply;
    out.replyValid = c.replyValid;
    if (outcome == SdoOutcome::SUCCESS && c.work.direction == SdoDirection::UPLOAD) {
        out.size = c.work.size;
        std::memcpy(out.data, c.upload, out.size);
        c.confirmed = true; // Fresh-session premise was required before any initial confirmation.
    }
    c.cleanup = c.port.queueOccupied && c.port.queued.data[0] == 0x80 && now < c.work.deadlineUs;
    out.clientAbortQueued = c.cleanup;
    c.handoffLive = false;
    if (!c.cleanup) retire(c);
    if ((c.possible && outcome != SdoOutcome::SUCCESS) || outcome == SdoOutcome::TRANSPORT_ERROR ||
        outcome == SdoOutcome::BUS_OFF || outcome == SdoOutcome::REBOOT) c.unresolved = true;
    CO_SDOclientClose(&c.engine);
    c.awaiting = false;
    c.completion = out;
    c.terminal = true;
}

void SdoClient::expire(Channel& c, uint64_t now) {
    if (!c.configured) return;
    if (c.cleanup && (now >= c.work.deadlineUs || now >= c.queueCutoff)) retire(c);
    if (!c.occupied || c.terminal) return;
    if (now >= c.work.deadlineUs ||
        ((!c.started || c.port.queueOccupied) && now >= c.queueCutoff) ||
        (c.awaiting && now >= c.responseCutoff)) {
        retire(c);
        finish(c, SdoOutcome::TIMEOUT, now);
    }
}

void SdoClient::abort(Channel& c, SdoOutcome outcome, uint32_t code, uint64_t now) {
    // Only the selected engine encodes aborts. Abort staging cannot qualify effects or reopen a route.
    retire(c);
    CO_SDO_abortCode_t nativeCode = CO_SDO_AB_NONE;
    static_assert(sizeof nativeCode == sizeof code, "SDO abort must preserve 32 bits");
    std::memcpy(&nativeCode, &code, sizeof code);
    const CO_SDO_return_t returned = c.work.direction == SdoDirection::DOWNLOAD
        ? CO_SDOclientDownload(&c.engine, 0, true, false, &nativeCode, nullptr, nullptr)
        : CO_SDOclientUpload(&c.engine, 0, true, &nativeCode, nullptr, nullptr, nullptr);
    c.queueCutoff = cutoff(now, c.work.deadlineUs, c.work.queueTimeoutUs);
    finish(c, outcome, now, static_cast<int32_t>(returned), true, code);
}

void SdoClient::pump(Channel& c, uint64_t now) {
    if (!c.occupied || c.terminal) return;
    if (!c.started) {
        const CO_SDO_return_t initiated = c.work.direction == SdoDirection::DOWNLOAD
            ? CO_SDOclientDownloadInitiate(&c.engine, c.work.index, c.work.subindex, c.work.size, UINT16_MAX, false)
            : CO_SDOclientUploadInitiate(&c.engine, c.work.index, c.work.subindex, UINT16_MAX, false);
        if (initiated != CO_SDO_RT_ok_communicationEnd ||
            (c.work.direction == SdoDirection::DOWNLOAD &&
             CO_SDOclientDownloadBufWrite(&c.engine, c.work.data, c.work.size) != c.work.size)) {
            finish(c, SdoOutcome::TRANSPORT_ERROR, now, static_cast<int32_t>(initiated));
            return;
        }
        c.started = true;
    }
    const bool wasQueued = c.port.queueOccupied;
    CO_SDO_abortCode_t nativeCode = CO_SDO_AB_NONE;
    // Native per-step timers are intentionally not advanced. Owner microsecond cutoffs
    // cover admission, staging and response and cannot be renewed by native progress.
    const CO_SDO_return_t returned = c.work.direction == SdoDirection::DOWNLOAD
        ? CO_SDOclientDownload(&c.engine, 0, false, false, &nativeCode, nullptr, nullptr)
        : CO_SDOclientUpload(&c.engine, 0, false, &nativeCode, nullptr, nullptr, nullptr);
    uint32_t code = 0;
    std::memcpy(&code, &nativeCode, sizeof code); // Arbitrary vendor abort bits are not a C++ enum load.
    if (!wasQueued && c.port.queueOccupied) {
        // The first frame shares the admission queue cutoff: delaying the first
        // service turn must not grant it another queue interval.
        if (c.queued) c.queueCutoff = cutoff(now, c.work.deadlineUs, c.work.queueTimeoutUs);
        if (c.port.queued.data[0] != 0x80) c.queued = true;
    }
    if (c.work.direction == SdoDirection::UPLOAD) {
        uint8_t bytes[32];
        const size_t count = CO_SDOclientUploadBufRead(&c.engine, bytes, sizeof bytes);
        const size_t usable = c.unsizedExpedited ? c.work.size : count;
        if (usable > count || usable > size_t(c.work.size - c.uploaded)) {
            abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_DATA_LONG, now);
            return;
        }
        std::memcpy(c.upload + c.uploaded, bytes, usable);
        c.uploaded = static_cast<uint8_t>(c.uploaded + usable);
        c.unsizedExpedited = false;
    }
    if (c.port.sendFault) {
        finish(c, SdoOutcome::TRANSPORT_ERROR, now, static_cast<int32_t>(returned));
    } else if (returned == CO_SDO_RT_ok_communicationEnd) {
        if (c.work.direction == SdoDirection::UPLOAD && c.uploaded != c.work.size)
            abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_DATA_SHORT, now);
        else finish(c, SdoOutcome::SUCCESS, now, static_cast<int32_t>(returned));
    } else if (returned < 0) {
        finish(c, returned == CO_SDO_RT_endedWithServerAbort ? SdoOutcome::SERVER_ABORT : SdoOutcome::CLIENT_ABORT,
               now, static_cast<int32_t>(returned), true, code);
    }
}

bool SdoClient::service(uint64_t now) {
    if (now < lastUs_) return false;
    lastUs_ = now;
    for (Channel& c : channels_) {
        expire(c, now);
        pump(c, now);
    }
    return true;
}

bool SdoClient::receive(const Frame& f, uint64_t now) {
    return receiveFrame(f, false, 0, now);
}

bool SdoClient::receive(const Frame& f, uint64_t received, uint64_t now) {
    return receiveFrame(f, true, received, now);
}

bool SdoClient::receiveFrame(const Frame& input, bool timeKnown, uint64_t received, uint64_t now) {
    if (now < lastUs_ || (timeKnown && received > now)) return false;
    // A caller may return our borrowed observation frame; copy before replacing it.
    const Frame f = input;
    service(now);
    bool networkIdentifier = false;
    for (const Channel& c : channels_)
        if (c.networkConfigured && (f.id == c.network.route.heartbeatId || f.id == c.network.route.emcyId))
            networkIdentifier = true;
    const bool networkAccepted = dispatchNetwork(f, nullptr, timeKnown, received, now);
    if (networkIdentifier) return networkAccepted;
    for (Channel& c : channels_) {
        if (!c.configured || f.id != c.route.serverToClientId) continue;
        c.observation.channel = c.route;
        c.observation.frame = f;
        c.observation.receivedTimeKnown = timeKnown;
        c.observation.receivedUs = received;
        c.observation.processedUs = now;
        c.observation.disposition = SdoRxDisposition::REJECTED;
        c.observationValid = true;
        if (!validateFrame(f) || f.extended || f.rtr || f.fd) return false;
        const bool serverAbort = c.possible && f.payloadLength == 8 && f.data[0] == 0x80;
        if (c.unresolved || !c.occupied || c.terminal ||
            (!serverAbort && (!c.awaiting || c.port.queueOccupied)) ||
            (timeKnown && received < c.handoffUs)) {
            // Observed old/unsolicited traffic disproves the external reuse premise.
            // Host tokens cannot classify a later indistinguishable response safely.
            retire(c);
            c.unresolved = true;
            c.observation.disposition = SdoRxDisposition::UNRESOLVED;
            finish(c, SdoOutcome::CORRELATION_LOST, now);
            return false;
        }
        if (f.payloadLength != 8) {
            c.observation.disposition = SdoRxDisposition::PROCESSED;
            c.lastReply = f; c.replyValid = true;
            abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_CMD, now);
            return true;
        }
        const bool initial = c.engine.state == CO_SDO_ST_UPLOAD_INITIATE_RSP ||
                             c.engine.state == CO_SDO_ST_DOWNLOAD_INITIATE_RSP;
        if (initial || f.data[0] == 0x80) {
            if ((uint16_t(f.data[1]) | uint16_t(f.data[2]) << 8) != c.work.index ||
                f.data[3] != c.work.subindex) return false;
        }
        c.lastReply = f; c.replyValid = true;
        c.observation.disposition = SdoRxDisposition::PROCESSED;
        if (initial && f.data[0] != 0x80) {
            if (c.work.size <= 4) {
                SdoReply checked;
                const Status status = checkReply(f, expectation(c.work), checked);
                if (!status) {
                    const uint32_t code = status.detail == static_cast<int32_t>(SdoError::WIDTH)
                        ? CO_SDO_AB_TYPE_MISMATCH : CO_SDO_AB_CMD;
                    abort(c, SdoOutcome::CLIENT_ABORT, code, now);
                    return true;
                }
                c.unsizedExpedited = checked.kind == SdoReplyKind::EXPEDITED_UPLOAD && !checked.sizeIndicated;
            } else if ((c.work.direction == SdoDirection::DOWNLOAD && f.data[0] != 0x60) ||
                       (c.work.direction == SdoDirection::UPLOAD && f.data[0] != 0x40 && f.data[0] != 0x41)) {
                abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_CMD, now);
                return true;
            }
            if (c.work.direction == SdoDirection::UPLOAD && (f.data[0] == 0x40 || f.data[0] == 0x41)) {
                if (!c.work.allowSegmentedUpload) {
                    recordSdo(c, timeKnown, received, now);
                    abort(c, SdoOutcome::UNSUPPORTED, CO_SDO_AB_UNSUPPORTED_ACCESS, now);
                    return true;
                }
                if (f.data[0] == 0x41 && word(f.data + 4) != c.work.size) {
                    abort(c, SdoOutcome::CLIENT_ABORT, CO_SDO_AB_TYPE_MISMATCH, now);
                    return true;
                }
            }
        } else if (c.engine.state == CO_SDO_ST_UPLOAD_SEGMENT_RSP && f.data[0] != 0x80 &&
                   (f.data[0] & 0xE0) == 0 && (f.data[0] & 0x10) == c.engine.toggle) {
            const uint8_t count = static_cast<uint8_t>(7 - ((f.data[0] >> 1) & 7));
            if (count > c.work.size - c.uploaded ||
                ((f.data[0] & 1) && count != c.work.size - c.uploaded)) {
                abort(c, SdoOutcome::CLIENT_ABORT,
                      count > c.work.size - c.uploaded ? CO_SDO_AB_DATA_LONG : CO_SDO_AB_DATA_SHORT, now);
                return true;
            }
        }
        // A peer abort can arrive while its next segment is still in our staging
        // slot. After object correlation, retire that slot before closing the engine.
        if (serverAbort) retire(c);
        c.handoffLive = false;
        c.awaiting = false;
        StackFrame incoming = native(f);
        c.port.rx.CANrx_callback(c.port.rx.object, &incoming);
        pump(c, now); // Drain the single native mailbox before accepting another frame.
        if (!c.terminal || c.completion.outcome == SdoOutcome::SUCCESS || c.completion.outcome == SdoOutcome::SERVER_ABORT)
            recordSdo(c, timeKnown, received, now);
        return true;
    }
    return false;
}

bool SdoClient::nextTx(uint8_t slot, uint64_t now, Frame& f, SdoTxId& id) {
    if (slot >= CHANNELS || now < lastUs_ || overlap(&f, sizeof f, this, sizeof *this) ||
        overlap(&id, sizeof id, this, sizeof *this) || overlap(&f, sizeof f, &id, sizeof id)) return false;
    service(now);
    Channel& c = channels_[slot];
    if (!c.configured || !c.port.queueOccupied || c.frame == UINT32_MAX) return false;
    const bool cleanup = c.cleanup;
    const uint64_t cleanupCutoff = c.queueCutoff;
    if ((!c.occupied || c.terminal) && !cleanup) return false;
    const Frame candidate = wire(c.port.queued);
    if (!validateFrame(candidate) ||
        (c.frame == 0 && c.work.size <= 4 && !checkRequest(candidate, expectation(c.work)))) {
        retire(c); finish(c, SdoOutcome::TRANSPORT_ERROR, now); return false;
    }
    ++c.frame;
    id.request.owner = this; id.request.channel = slot; id.request.generation = c.generation;
    id.frame = c.frame;
    f = candidate;
    c.port.lastHandoff = c.port.queued;
    if (c.port.handedOff != UINT32_MAX) ++c.port.handedOff;
    retire(c);
    c.handoffLive = true;
    c.handoffUs = now;
    c.handoffCutoff = cleanupCutoff;
    if (!cleanup) {
        c.possible = true;
        c.awaiting = true;
        c.responseCutoff = cutoff(now, c.work.deadlineUs, c.work.responseTimeoutUs);
        c.handoffCutoff = c.responseCutoff;
    }
    return true;
}

bool SdoClient::canTransmit(SdoTxId id, uint64_t now) const {
    if (id.request.owner != this || id.request.channel >= CHANNELS || now < lastUs_)
        return false;
    const Channel& c = channels_[id.request.channel];
    return c.handoffLive && id.request.generation != 0 &&
           id.request.generation == c.generation && id.frame != 0 && id.frame == c.frame &&
           now < c.handoffCutoff && now < c.work.deadlineUs;
}

bool SdoClient::confirmTx(SdoTxId id, uint64_t now) {
    if (now < lastUs_ || !valid(id.request)) return false;
    lastUs_ = now;
    Channel& c = channels_[id.request.channel];
    expire(c, now);
    if (c.terminal || id.frame == 0 || id.frame != c.frame || !c.possible) return false;
    c.txConfirmed = true;
    c.handoffLive = false;
    return true;
}

bool SdoClient::cancel(SdoRequestId id, uint64_t now) {
    if (now < lastUs_ || !valid(id)) return false;
    lastUs_ = now;
    Channel& c = channels_[id.channel];
    expire(c, now);
    if (c.terminal) return false;
    retire(c);
    finish(c, SdoOutcome::CANCELLED, now);
    return true;
}

bool SdoClient::busOff(uint64_t now) {
    return reportController(ControllerState::BUS_OFF, now);
}

bool SdoClient::reboot(uint8_t node, uint64_t now) {
    if (node == 0 || node > 127 || now < lastUs_) return false;
    lastUs_ = now;
    for (unsigned i = 0; i < CHANNELS; ++i)
        if (channels_[i].networkConfigured && channels_[i].network.route.binding.endpoint.address == node)
            invalidateNode(i, NetworkInvalidation::BOOTUP, now);
    for (Channel& c : channels_) {
        expire(c, now);
        if (!c.configured || c.route.binding.endpoint.address != node) continue;
        retire(c);
        c.unresolved = true;
        finish(c, SdoOutcome::REBOOT, now);
    }
    return true;
}

bool SdoClient::invalidate(uint8_t slot, uint64_t now) {
    if (slot >= CHANNELS || !channels_[slot].configured || now < lastUs_) return false;
    if (networkAnchor(slot) >= 0) return invalidateNetwork(slot, NetworkInvalidation::BINDING, now);
    lastUs_ = now;
    Channel& c = channels_[slot];
    expire(c, now);
    retire(c);
    c.unresolved = true;
    finish(c, SdoOutcome::BINDING_INVALIDATED, now);
    return true;
}

bool SdoClient::transportFailure(SdoRequestId id, uint64_t now) {
    if (!valid(id) || now < lastUs_) return false;
    lastUs_ = now;
    Channel& c = channels_[id.channel];
    expire(c, now);
    if (c.terminal) return false;
    retire(c);
    c.port.sendFault = true;
    finish(c, SdoOutcome::TRANSPORT_ERROR, now);
    return true;
}

const SdoCompletion* SdoClient::result(SdoRequestId id) const {
    return valid(id) && channels_[id.channel].terminal ? &channels_[id.channel].completion : nullptr;
}
const SdoRxObservation* SdoClient::observation(uint8_t slot) const {
    return slot < CHANNELS && channels_[slot].observationValid ? &channels_[slot].observation : nullptr;
}
bool SdoClient::release(SdoRequestId id) {
    if (!result(id)) return false;
    channels_[id.channel].occupied = false;
    return true;
}
bool SdoClient::unresolved(uint8_t slot) const {
    return slot >= CHANNELS || !channels_[slot].configured || channels_[slot].unresolved;
}
const CO_CANmodule_t* SdoClient::port(uint8_t slot) const {
    return slot < CHANNELS && channels_[slot].configured ? &channels_[slot].port : nullptr;
}
} // namespace CANOPEN
} // namespace MotorControlCAN
