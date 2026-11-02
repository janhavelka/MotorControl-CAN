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
} // namespace OwnerRig
