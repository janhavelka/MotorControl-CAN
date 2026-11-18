// SPDX-License-Identifier: MIT
// Fake-owner plumbing whose behavior is identical across the owner suites.
// Scenario fields, transmit guards and helpers with different defaults stay in
// each suite. Define `checks` and CHECK first: plumbing failures then count and
// report through the including suite's own macro. Test executables use one TU.
#pragma once
#ifndef CHECK
#error "Define the suite's checks counter and CHECK macro before including OwnerRig.h"
#endif
#include "CanBusOwner.h"

namespace OwnerRig {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace O = MotorControlCANOwner;

inline C::SdoChannel route(unsigned node = 1) {
    C::SdoChannel out;
    out.binding.logicalAxisId = 100 + node;
    out.binding.endpoint.profile = 41;
    out.binding.endpoint.address = node;
    out.binding.bindingGeneration = 3;
    out.binding.configurationGeneration = 7;
    out.clientToServerId = 0x600 + node;
    out.serverToClientId = 0x580 + node;
    return out;
}
inline M::Frame frame(uint32_t id, uint8_t size) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = size;
    return value;
}
inline O::ServiceBudget budget(uint8_t rx = 8, uint8_t tx = 2, uint8_t events = 8,
                               uint8_t starts = 2, uint8_t recovery = 0) {
    O::ServiceBudget value;
    value.rx = rx;
    value.tx = tx;
    value.events = events;
    value.starts = starts;
    value.recovery = recovery;
    return value;
}
inline bool same(O::TransportTxId a, O::TransportTxId b) {
    return O::sameTransportTxId(a, b);
}
// Scripted receive/transmit/report queues. A suite derives from this, adds the
// fields its guard needs and sets Transport::beforeTransmit on plumbing()'s
// result. The Transport context is a WireBase pointer, so a derived guard casts
// back through WireBase.
struct WireBase {
    enum { CAPACITY = 128 };
    O::ReceivedFrame incoming[CAPACITY];
    O::TxReport reports[CAPACITY];
    M::Frame sent[CAPACITY];
    O::TransportTxId ids[CAPACITY];
    uint32_t epochs[CAPACITY] = {};
    bool pending[CAPACITY] = {};
    unsigned rxRead = 0, rxWrite = 0, reportRead = 0, reportWrite = 0;
    unsigned sentCount = 0, retireCalls = 0, observed = 0;
    bool autoReport = true, retirement = true, accept = true;
    static bool receive(void* context, O::ReceivedFrame& output) {
        WireBase& w = *static_cast<WireBase*>(context);
        if (w.rxRead == w.rxWrite)
            return false;
        output = w.incoming[w.rxRead++];
        return true;
    }
    static bool transmit(void* context, const M::Frame& value, O::TransportTxId id, uint32_t epoch) {
        WireBase& w = *static_cast<WireBase*>(context);
        CHECK(w.sentCount < CAPACITY);
        const unsigned i = w.sentCount++;
        w.sent[i] = value;
        w.ids[i] = id;
        w.epochs[i] = epoch;
        w.pending[i] = true;
        if (w.autoReport)
            w.report(i, w.accept);
        return w.accept;
    }
    static bool takeReport(void* context, O::TxReport& output) {
        WireBase& w = *static_cast<WireBase*>(context);
        if (w.reportRead == w.reportWrite)
            return false;
        output = w.reports[w.reportRead++];
        return true;
    }
    static bool retire(void* context, O::TransportTxId id, uint32_t epoch) {
        WireBase& w = *static_cast<WireBase*>(context);
        ++w.retireCalls;
        if (!w.retirement)
            return false;
        for (unsigned i = 0; i < w.sentCount; ++i)
            if (same(w.ids[i], id) && w.epochs[i] == epoch)
                w.pending[i] = false;
        unsigned kept = w.reportRead;
        for (unsigned i = w.reportRead; i < w.reportWrite; ++i)
            if (!same(w.reports[i].id, id) || w.reports[i].controllerEpoch != epoch)
                w.reports[kept++] = w.reports[i];
        w.reportWrite = kept;
        return true;
    }
    static void observe(void* context, const O::ReceivedFrame&, uint64_t) {
        ++static_cast<WireBase*>(context)->observed;
    }
    O::Transport plumbing() {
        O::Transport value;
        value.context = this;
        value.receive = receive;
        value.transmit = transmit;
        value.takeTxReport = takeReport;
        value.retire = retire;
        value.observe = observe;
        return value;
    }
    void report(unsigned i, bool transmitted = true) {
        CHECK(i < sentCount && reportWrite < CAPACITY && pending[i]);
        O::TxReport& value = reports[reportWrite++];
        value.id = ids[i];
        value.controllerEpoch = epochs[i];
        value.transmitted = transmitted;
        pending[i] = false;
    }
    void push(const M::Frame& value, uint64_t now, uint32_t epoch = 1) {
        CHECK(rxWrite < CAPACITY);
        O::ReceivedFrame& input = incoming[rxWrite++];
        input.frame = value;
        input.receivedTimeKnown = true;
        input.receivedUs = now;
        input.controllerEpoch = epoch;
    }
};
// Prompt 55 self-repair helpers. They add no behavior to WireBase or any existing helper.
//
// Keep one binding's unresolved routes quarantined: SdoReusePolicy::NONE, the setting before
// prompt 55. A response timeout after possible TX still stages its one cleanup abort; no marker
// read or reopening follows, and every marker is refused.
inline bool keepQuarantine(O::CanBusOwner& owner, const C::SdoChannel& channel, uint64_t now) {
    O::SdoReuse reuse;
    reuse.policy = O::SdoReusePolicy::NONE;
    return owner.setSdoReuse(channel.binding, reuse, now);
}
// Exact classical 8-byte SDO frame with this command byte and multiplexer.
inline bool sdoFrame(const M::Frame& f, uint32_t id, uint8_t command, uint16_t index,
                     uint8_t subindex) {
    return f.id == id && !f.extended && !f.rtr && f.dlc == 8 && f.payloadLength == 8 &&
           f.data[0] == command && f.data[1] == uint8_t(index) && f.data[2] == uint8_t(index >> 8) &&
           f.data[3] == subindex;
}
// The one cleanup abort after a response timeout: 80 <index> <subindex> 00 00 04 05, the CiA 301
// code 0x05040000 (SDO protocol timed out) naming the timed-out request's own object.
inline bool isTimeoutAbort(const M::Frame& f, uint32_t id, uint16_t index, uint8_t subindex = 0) {
    return sdoFrame(f, id, 0x80, index, subindex) && f.data[4] == 0 && f.data[5] == 0 &&
           f.data[6] == 0x04 && f.data[7] == 0x05;
}
// The same abort while it is still staged in a channel's port (SdoClient::port()->queued).
inline bool isTimeoutAbort(const StackFrame& f, uint32_t id, uint16_t index, uint8_t subindex = 0) {
    M::Frame copy = frame(f.ident, f.DLC);
    copy.extended = f.extended;
    copy.rtr = f.rtr;
    for (unsigned i = 0; i < 8; ++i)
        copy.data[i] = f.data[i];
    return isTimeoutAbort(copy, id, index, subindex);
}
// The owner's own resync marker read: an expedited upload request of the default marker 1000:00
// (alternate 1018:01), 40 <index> <subindex> 00 00 00 00.
inline bool isMarkerRead(const M::Frame& f, uint32_t id, uint16_t index = 0x1000,
                         uint8_t subindex = 0) {
    return sdoFrame(f, id, 0x40, index, subindex) && !f.data[4] && !f.data[5] && !f.data[6] &&
           !f.data[7];
}
// A server's expedited four-byte upload reply, 43 <index> <subindex> <value LE>, such as the
// answer to a marker read of the U32 objects 1000:00 or 1018:01.
inline M::Frame uploadReply4(uint32_t id, uint16_t index, uint8_t subindex, uint32_t value) {
    M::Frame f = frame(id, 8);
    f.data[0] = 0x43;
    f.data[1] = uint8_t(index);
    f.data[2] = uint8_t(index >> 8);
    f.data[3] = subindex;
    for (unsigned i = 0; i < 4; ++i)
        f.data[4 + i] = uint8_t(value >> (8 * i));
    return f;
}
} // namespace OwnerRig
