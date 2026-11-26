/** @file EspRecordStore.h
 * @brief ESP32 storage hook for measured record lines (prompt 58): ESP-IDF NVS, or Arduino Preferences in
 * an Arduino build.
 * SPDX-License-Identifier: MIT
 *
 * One NVS string per axis and slot, in the namespace RECORD_NAMESPACE under recordStoreKey's key. An ESP-IDF
 * build uses the NVS API and initializes the default NVS partition on the first explicit save or restore
 * (nvs_flash_init), never erasing it: a partition that needs erasing makes the call fail. An Arduino build
 * uses Preferences, over the same NVS, which the Arduino core initializes; restore opens it read-only in
 * both builds and reports a line that does not fit as TOO_LARGE. Nothing is read at construction or boot;
 * the console calls the hook only on `record save` and `record restore`.
 */
#pragma once
#include "RecordStore.h"

namespace MotorControlCANExample {
/** NVS namespace of the record lines. */
constexpr const char* RECORD_NAMESPACE = "mcc_records";
/** The NVS or Preferences hook. Stateless; one static instance serves every axis. */
class EspRecordStore {
  public:
    /** @return The hook; construction and this call touch no flash. */
    RecordStore store() noexcept;

  private:
    static RecordStoreStatus save(void* context, uint32_t axisId, RecordSlot slot, const char* text,
                                  size_t size);
    static RecordStoreStatus restore(void* context, uint32_t axisId, RecordSlot slot, char* text,
                                     size_t capacity, size_t& size);
};
} // namespace MotorControlCANExample
