// SPDX-License-Identifier: MIT
#include "301/CO_driver.h"

extern "C" CO_ReturnError_t
CO_CANrxBufferInit(CO_CANmodule_t* module, uint16_t index, uint16_t ident,
                   uint16_t mask, bool_t rtr, void* object,
                   void (*callback)(void*, void*)) {
    if (module == nullptr || index != 0 || callback == nullptr) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
    module->rx.ident = ident;
    module->rx.mask = mask;
    module->rx.rtr = rtr != 0;
    module->rx.object = object;
    module->rx.CANrx_callback = callback;
    return CO_ERROR_NO;
}

extern "C" CO_CANtx_t*
CO_CANtxBufferInit(CO_CANmodule_t* module, uint16_t index, uint16_t ident,
                   bool_t rtr, uint8_t length, bool_t syncFlag) {
    if (module == nullptr || index != 0 || length > 8) {
        return nullptr;
    }
    module->tx.ident = ident;
    module->tx.DLC = length;
    module->tx.rtr = rtr != 0;
    module->tx.syncFlag = syncFlag;
    return &module->tx;
}

extern "C" CO_ReturnError_t CO_CANsend(CO_CANmodule_t* module, CO_CANtx_t* buffer) {
    if (module->queueOccupied || module->refuseSend) {
        module->sendFault = true;
        return CO_ERROR_TX_OVERFLOW;
    }
    StackFrame candidate = {};
    candidate.ident = buffer->ident;
    candidate.DLC = buffer->DLC;
    candidate.rtr = buffer->rtr;
    memcpy(candidate.data, buffer->data, candidate.DLC);
    module->queued = candidate;
    module->queueOccupied = true;
    buffer->bufferFull = true;
    if (module->accepted != UINT32_MAX) ++module->accepted;
    return CO_ERROR_NO;
}
