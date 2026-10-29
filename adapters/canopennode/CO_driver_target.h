// SPDX-License-Identifier: MIT
/** @file
 * Fixed storage and callbacks for the cooperative application TX staging port.
 */
#ifndef MOTORCONTROL_CAN_STACK_TARGET_H
#define MOTORCONTROL_CAN_STACK_TARGET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Selected adapter configuration: no block/local OD transfer, dynamic OD or wake callback.
#define CO_CONFIG_SDO_CLI (CO_CONFIG_SDO_CLI_ENABLE | CO_CONFIG_SDO_CLI_SEGMENTED)
#define CO_CONFIG_SDO_CLI_BUFFER_SIZE 32U
#define CO_CONFIG_FIFO CO_CONFIG_FIFO_ENABLE
#define CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC 0
#define CO_CONFIG_SDO_SRV 0
#if !defined(_WIN32) && (!defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "This selected CANopenNode port requires an explicitly supported little-endian target"
#endif
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) (x)
#define CO_SWAP_32(x) (x)
#define CO_SWAP_64(x) (x)
typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;

/** One copied Classical CAN frame; the staging port preserves format and RTR. */
typedef struct {
    uint32_t ident; /**< Full identifier; the SDO route accepts standard data only. */
    uint8_t data[8]; /**< Inline owned bytes. */
    uint8_t DLC; /**< Classical DLC, 0..8; RTR carries no data and is rejected here. */
    bool extended; /**< True for a 29-bit identifier. */
    bool rtr; /**< True for a remote frame. */
} StackFrame;
#define CO_CANrxMsg_readIdent(msg) (((const StackFrame*)(msg))->ident)
#define CO_CANrxMsg_readDLC(msg) (((const StackFrame*)(msg))->DLC)
#define CO_CANrxMsg_readData(msg) (((const StackFrame*)(msg))->data)

/** One SDO receive registration, owned by the cooperative application. */
typedef struct {
    uint16_t ident; /**< Registered 11-bit identifier. */
    uint16_t mask; /**< Identifier mask. */
    bool rtr; /**< Required frame kind. */
    void* object; /**< Borrowed live SDO context; owner cannot move. */
    void (*CANrx_callback)(void*, void*); /**< Synchronous callback, no reentry. */
} CO_CANrx_t;

/** One stack TX staging buffer, distinct from the copied controller queue. */
typedef struct {
    uint32_t ident; /**< 11-bit identifier. */
    uint8_t DLC; /**< Payload length. */
    uint8_t data[8]; /**< Inline staging bytes. */
    volatile bool_t bufferFull; /**< Queue occupies this buffer until retirement/handoff. */
    volatile bool_t syncFlag; /**< Preserved stack flag; unused by this port. */
    bool rtr; /**< Frame kind. */
} CO_CANtx_t;

/** Retirable single-slot software staging port; queue acceptance never claims bus transmission. */
typedef struct {
    CO_CANrx_t rx; /**< One installed SDO callback. */
    CO_CANtx_t tx; /**< One stack staging buffer. */
    StackFrame queued; /**< Copy made at queue acceptance. */
    StackFrame lastHandoff; /**< Copy of the last possibly transmitted frame. */
    bool queueOccupied; /**< Retirable only while true. */
    uint32_t accepted; /**< Queue accept count, including client aborts. */
    uint32_t handedOff; /**< Irreversible handoff count; no ACK claim. */
    bool refuseSend; /**< Application preflight refusal; no queued frame is accepted while true. */
    bool sendFault; /**< Adapter-latched fault, because upstream ignores send return. */
} CO_CANmodule_t;

// All calls and receive callbacks run in one cooperative context; these macros
// are deliberately unsuitable for an interrupt/multithreaded production port.
#define CO_LOCK_CAN_SEND(module)
#define CO_UNLOCK_CAN_SEND(module)
#define CO_LOCK_EMCY(module)
#define CO_UNLOCK_EMCY(module)
#define CO_LOCK_OD(module)
#define CO_UNLOCK_OD(module)
#define CO_FLAG_READ(flag) ((flag) != NULL)
#define CO_FLAG_SET(flag) ((flag) = (void*)1)
#define CO_FLAG_CLEAR(flag) ((flag) = NULL)

#endif
