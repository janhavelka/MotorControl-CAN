/** @file RecordStore.h
 * @brief Example storage hook for measured record lines (prompt 58), with a native file implementation.
 * SPDX-License-Identifier: MIT
 *
 * A store keeps one canonical record line per axis and slot, as bytes only. The console calls it on an
 * explicit `record save SLOT` or `record restore SLOT` and on nothing else: not at construction, not at boot,
 * not on selection or recovery. A restored line is decoded and validated like `record load` and stored
 * LOADED; it is never bound, and nothing is enabled or moved. Implementations: FileRecordStore (native,
 * below), EspRecordStore (ESP-IDF NVS, or Arduino Preferences in an Arduino build; EspRecordStore.h).
 */
#pragma once
#include "ProbeMotion.h"
#include <stddef.h>
#include <stdint.h>

namespace MotorControlCANExample {
/** Outcome of one storage call. */
enum class RecordStoreStatus : uint8_t {
    OK,        ///< Saved, or restored into the caller's buffer.
    NOT_FOUND, ///< restore: nothing is stored for this axis and slot.
    /** restore: the stored line does not fit the caller's buffer, so nothing was copied; save: the line
     * is longer than the medium accepts. */
    TOO_LARGE,
    FAILED ///< The medium refused or failed (open, read, write, commit); its contents are unknown.
};
/** Caller-supplied storage of record lines. Calls are synchronous on the console's task and bounded by
 * the medium; the store retains no borrowed pointer. */
struct RecordStore {
    void* context = nullptr; ///< Borrowed; outlives the console.
    /** Replace the stored line of (axisId, slot).
     * @param context The store's context. @param axisId Nonzero logical axis identity.
     * @param slot The record's slot. @param text Borrowed canonical line of size bytes, no NUL needed.
     * @param size 1..RECORD_TEXT_CAPACITY. @return OK, TOO_LARGE or FAILED. */
    RecordStoreStatus (*save)(void* context, uint32_t axisId, RecordSlot slot, const char* text,
                              size_t size) = nullptr;
    /** Copy the stored line of (axisId, slot) into text, NUL terminated.
     * @param context The store's context. @param axisId Nonzero logical axis identity.
     * @param slot The slot. @param text Caller buffer of capacity bytes, written only on OK.
     * @param capacity Buffer bytes, the NUL included. @param size Line length without the NUL, written
     * only on OK. @return OK, NOT_FOUND, TOO_LARGE or FAILED. */
    RecordStoreStatus (*restore)(void* context, uint32_t axisId, RecordSlot slot, char* text,
                                 size_t capacity, size_t& size) = nullptr;
};
/** The key a store files (axisId, slot) under: `r<axisId>s<slot>`, at most 13 characters, inside the
 * 15-character NVS key limit.
 * @param axisId Logical axis identity. @param slot The slot.
 * @param key Destination, written NUL terminated on success. @param capacity At least 14.
 * @return False with key unchanged when capacity is too small. */
inline bool recordStoreKey(uint32_t axisId, RecordSlot slot, char* key, size_t capacity) noexcept {
    if (!key || capacity < 14)
        return false;
    char digits[10];
    unsigned count = 0;
    do {
        digits[count++] = char('0' + axisId % 10);
        axisId /= 10;
    } while (axisId);
    size_t at = 0;
    key[at++] = 'r';
    while (count)
        key[at++] = digits[--count];
    key[at++] = 's';
    key[at++] = char('0' + unsigned(slot) % 10);
    key[at] = '\0';
    return true;
}
/** Native implementation: one file per key, `<directory>/<key>.txt`, holding the line without a line
 * ending. A save writes and closes `<key>.tmp`, removes `<key>.txt` (a Windows rename refuses an existing
 * destination, so this is done on every platform) and renames the new file into place. An interruption
 * before the removal leaves the old line; between the removal and the rename it leaves no `<key>.txt` and
 * the complete new line in `<key>.tmp`, so restore reports NOT_FOUND until the next save (renaming the file
 * recovers it); after the rename, the new line. The medium's own durability is not claimed. Stationary
 * while attached. */
class FileRecordStore {
  public:
    /** @param directory Borrowed existing directory path, at most PATH_CAPACITY - 24 characters;
     * it outlives this object. Construction touches no file. */
    explicit FileRecordStore(const char* directory) noexcept : directory_(directory) {}
    /** @return The hook, with this object as its context. */
    RecordStore store() noexcept;
    enum {
        PATH_CAPACITY = 512 ///< Largest file path, NUL included; a longer one fails the call.
    };

  private:
    const char* directory_;
    bool path(uint32_t axisId, RecordSlot slot, const char* suffix, char* output) const noexcept;
    static RecordStoreStatus save(void* context, uint32_t axisId, RecordSlot slot, const char* text,
                                  size_t size);
    static RecordStoreStatus restore(void* context, uint32_t axisId, RecordSlot slot, char* text,
                                     size_t capacity, size_t& size);
};
} // namespace MotorControlCANExample
