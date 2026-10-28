// SPDX-License-Identifier: MIT
#define OD_DEFINITION
#include "fit_proof.h"
#include <limits>

namespace CanopenFit {
CanBusOwner::CanBusOwner()
    : ready_(false), controller_(), client_(), completion_(), occupied_(false),
      terminal_(false), download_(false), interlocked_(false), id_(0), index_(0),
      subIndex_(0), deadlineUs_(0), lastUs_(0), handoffsAtAdmission_(0),
      heartbeats_(0), emergencies_(0), pdos_(0), lastEvent_() {
    uint8_t count = 3, node = 1;
    uint32_t clientToServer = 0x601, serverToClient = 0x581;
    OD_obj_record_t fields[] = {
        {&count, 0, ODA_SDO_R, 1}, {&clientToServer, 1, ODA_SDO_R, 4},
        {&serverToClient, 2, ODA_SDO_R, 4}, {&node, 3, ODA_SDO_R, 1}};
    OD_entry_t entry = {0x1280, 4, ODT_REC, fields, nullptr};
    // Dynamic/local OD access is disabled. Init copies these values; no local
    // OD pointer survives. The callback retains only the stable client_ address.
    ready_ = CO_SDOclient_init(&client_, nullptr, &entry, 2, &controller_, 0,
                              &controller_, 0, nullptr) == CO_ERROR_NO;
}

bool CanBusOwner::admit(bool download, uint16_t index, uint8_t subIndex,
                       const uint8_t* bytes, size_t size, uint64_t nowUs,
                       uint16_t timeoutMs, RequestId& id) {
    const uint64_t intervalUs = static_cast<uint64_t>(timeoutMs) * 1000;
    if (!ready_ || occupied_ || interlocked_ || timeoutMs == 0 || nowUs < lastUs_ ||
        nowUs > std::numeric_limits<uint64_t>::max() - intervalUs ||
        id_ == std::numeric_limits<uint32_t>::max() ||
        (download ? (bytes == nullptr || size == 0 || size > 32) : size != 0)) {
        return false;
    }
    CO_SDO_return_t returned = download
        ? CO_SDOclientDownloadInitiate(&client_, index, subIndex, size, timeoutMs, false)
        : CO_SDOclientUploadInitiate(&client_, index, subIndex, timeoutMs, false);
    if (returned != CO_SDO_RT_ok_communicationEnd) {
        return false;
    }
    if (download) {
        // Validated size fits the configured FIFO in full, before any service.
        const size_t written = CO_SDOclientDownloadBufWrite(&client_, bytes, size);
        if (written != size) {
            CO_SDOclientClose(&client_);
            return false;
        }
    }
    occupied_ = true;
    terminal_ = false;
    download_ = download;
    index_ = index;
    subIndex_ = subIndex;
    deadlineUs_ = nowUs + intervalUs;
    lastUs_ = nowUs;
    handoffsAtAdmission_ = controller_.handedOff;
    ++id_;
    id.owner = this;
    id.serial = id_;
    return true;
}

void CanBusOwner::retireQueued() {
    controller_.queueOccupied = false;
    controller_.tx.bufferFull = false;
}

void CanBusOwner::finish(Outcome outcome, CO_SDO_return_t returned, uint32_t abortCode) {
    if (terminal_) return;
    Completion candidate = {};
    candidate.outcome = outcome;
    candidate.stackReturn = returned;
    candidate.abortCode = abortCode;
    const bool handedOff = controller_.handedOff != handoffsAtAdmission_;
    candidate.executionUnknown = download_ && handedOff && outcome != Outcome::Success;
    if (!download_ && outcome == Outcome::Success) {
        candidate.size = CO_SDOclientUploadBufRead(&client_, candidate.data, sizeof(candidate.data));
    }
    // Close alone cannot remove queued controller work. Our fake has a strong
    // retirement guarantee only before handoff. Abort TX is also retired here.
    retireQueued();
    CO_SDOclientClose(&client_);
    if ((handedOff && outcome != Outcome::Success) || outcome == Outcome::Transport) {
        interlocked_ = true;
    }
    completion_ = candidate;
    terminal_ = true;
}

void CanBusOwner::service(uint64_t nowUs) {
    if (nowUs < lastUs_) return; // Invalid clock input never rewinds a deadline.
    const uint64_t delta = nowUs - lastUs_;
    lastUs_ = nowUs;
    if (!occupied_ || terminal_) return;
    if (nowUs >= deadlineUs_) {
        finish(Outcome::Timeout, CO_SDO_RT_endedWithClientAbort, CO_SDO_AB_TIMEOUT);
        return;
    }
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
    const uint32_t elapsedUs = static_cast<uint32_t>(delta);
    // Remaining admission interval is at most 65535ms, so an active predeadline
    // turn always fits uint32_t. No retry loop or hidden clock is involved.
    const CO_SDO_return_t returned = download_
        ? CO_SDOclientDownload(&client_, elapsedUs, false, false, &abortCode, nullptr, nullptr)
        : CO_SDOclientUpload(&client_, elapsedUs, false, &abortCode, nullptr, nullptr, nullptr);
    if (controller_.sendFault) {
        finish(Outcome::Transport, returned, abortCode);
    } else if (returned == CO_SDO_RT_ok_communicationEnd) {
        finish(Outcome::Success, returned, abortCode);
    } else if (returned < 0) {
        finish(returned == CO_SDO_RT_endedWithServerAbort ? Outcome::ServerAbort : Outcome::ClientAbort,
               returned, abortCode);
    }
}

void CanBusOwner::receive(const FitFrame& frame, uint64_t nowUs) {
    if (nowUs < lastUs_) return;
    service(nowUs); // Owner's absolute deadline precedes received-frame processing.
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
    if (!occupied_ || terminal_ || controller_.queueOccupied ||
        controller_.handedOff == handoffsAtAdmission_ ||
        frame.ident != controller_.rx.ident || frame.DLC != 8) return;
    // Upstream accepts server aborts without checking their object address.
    // Initial replies and aborts have addresses; segmented data does not.
    if (frame.data[0] == 0x80 || client_.state == CO_SDO_ST_UPLOAD_INITIATE_RSP ||
        client_.state == CO_SDO_ST_DOWNLOAD_INITIATE_RSP) {
        const uint16_t index = static_cast<uint16_t>(frame.data[1]) |
                               static_cast<uint16_t>(frame.data[2]) << 8;
        if (index != index_ || frame.data[3] != subIndex_) return;
    }
    FitFrame callbackFrame = frame;
    controller_.rx.CANrx_callback(controller_.rx.object, &callbackFrame);
    // The upstream mailbox holds one frame. Drain it through the one engine
    // before accepting another matching SDO; the callback retains no RX pointer.
    service(nowUs);
}

bool CanBusOwner::handoff(uint64_t nowUs) {
    if (nowUs < lastUs_) return false;
    service(nowUs);
    if (!controller_.queueOccupied) return false;
    controller_.lastHandoff = controller_.queued;
    ++controller_.handedOff;
    retireQueued();
    return true;
}

bool CanBusOwner::cancel(RequestId id) {
    if (!occupied_ || terminal_ || id.owner != this || id.serial != id_) return false;
    finish(Outcome::Cancelled, CO_SDO_RT_endedWithClientAbort, 0);
    return true;
}

const Completion* CanBusOwner::result(RequestId id) const {
    return occupied_ && terminal_ && id.owner == this && id.serial == id_ ? &completion_ : nullptr;
}

bool CanBusOwner::release(RequestId id) {
    if (result(id) == nullptr) return false;
    occupied_ = false;
    terminal_ = false;
    return true;
}
} // namespace CanopenFit
