/** @file MotorControlCAN.h
 * @brief Convenience include for every installed core header: status, units, axis targets, the
 * common read, action and move vocabulary, CAN frames and the CANopen service codecs.
 * SPDX-License-Identifier: MIT
 * Profile headers under profiles/ are not included. Each belongs to one drive family, and a minimal
 * consumer may omit the optional descriptive catalogue, so include the ones you use directly.
 */
#pragma once

#include <MotorControlCAN/Status.h>
#include <MotorControlCAN/Units.h>
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/Endpoint.h>
#include <MotorControlCAN/ReadOperation.h>
#include <MotorControlCAN/ActionOperation.h>
#include <MotorControlCAN/MoveOperation.h>
#include <MotorControlCAN/VelocityOperation.h>
#include <MotorControlCAN/HomeOperation.h>
#include <MotorControlCAN/can/Frame.h>
#include <MotorControlCAN/canopen/Sdo.h>
#include <MotorControlCAN/canopen/SdoTransfer.h>
#include <MotorControlCAN/canopen/Network.h>
#include <MotorControlCAN/canopen/Pdo.h>
#include <MotorControlCAN/canopen/PdoState.h>
#include <MotorControlCAN/Version.h>
