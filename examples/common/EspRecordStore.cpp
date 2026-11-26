// SPDX-License-Identifier: MIT
// Firmware only: ESP-IDF NVS, or Arduino Preferences in an Arduino build (prompt 58). The console calls it
// on an explicit `record save` or `record restore` only.
#include "EspRecordStore.h"
#include <cstring>
#if defined(ARDUINO)
#include <Preferences.h>
#else
#include "nvs.h"
#include "nvs_flash.h"
#endif

namespace MotorControlCANExample {
namespace {
constexpr size_t LINE_BYTES = MotorControlCAN::LICHUAN_C::RECORD_TEXT_CAPACITY + 1;
#if !defined(ARDUINO)
// Open the namespace, initializing the default partition on first use. A partition that needs erasing
// (no free pages, a newer format) fails: erasing it here would destroy whatever else it holds.
esp_err_t openNamespace(nvs_open_mode_t mode, nvs_handle_t& handle) {
    esp_err_t error = nvs_open(RECORD_NAMESPACE, mode, &handle);
    if (error == ESP_ERR_NVS_NOT_INITIALIZED) {
        error = nvs_flash_init();
        if (error == ESP_OK)
            error = nvs_open(RECORD_NAMESPACE, mode, &handle);
    }
    return error;
}
#endif
} // namespace
RecordStore EspRecordStore::store() noexcept {
    RecordStore hook;
    hook.context = this;
    hook.save = save;
    hook.restore = restore;
    return hook;
}
RecordStoreStatus EspRecordStore::save(void*, uint32_t axisId, RecordSlot slot, const char* text, size_t size) {
    char key[16];
    if (!text || !size || !recordStoreKey(axisId, slot, key, sizeof key))
        return RecordStoreStatus::FAILED;
    if (size >= LINE_BYTES)
        return RecordStoreStatus::TOO_LARGE;
    // NVS strings are NUL terminated; the console's line is a counted span.
    char line[LINE_BYTES];
    std::memcpy(line, text, size);
    line[size] = '\0';
#if defined(ARDUINO)
    Preferences preferences;
    if (!preferences.begin(RECORD_NAMESPACE, false))
        return RecordStoreStatus::FAILED;
    const size_t written = preferences.putString(key, line);
    preferences.end();
    return written ? RecordStoreStatus::OK : RecordStoreStatus::FAILED;
#else
    nvs_handle_t handle;
    if (openNamespace(NVS_READWRITE, handle) != ESP_OK)
        return RecordStoreStatus::FAILED;
    esp_err_t error = nvs_set_str(handle, key, line);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK ? RecordStoreStatus::OK : RecordStoreStatus::FAILED;
#endif
}
RecordStoreStatus EspRecordStore::restore(void*, uint32_t axisId, RecordSlot slot, char* text, size_t capacity,
                                          size_t& size) {
    char key[16];
    if (!text || capacity < 2 || !recordStoreKey(axisId, slot, key, sizeof key))
        return RecordStoreStatus::FAILED;
    // Read into a local buffer first, so that the caller's buffer is unchanged on every failure.
    char line[LINE_BYTES];
#if defined(ARDUINO)
    Preferences preferences;
    if (!preferences.begin(RECORD_NAMESPACE, false))
        return RecordStoreStatus::FAILED;
    if (!preferences.isKey(key)) {
        preferences.end();
        return RecordStoreStatus::NOT_FOUND;
    }
    // Preferences returns 0 both for a value larger than the buffer and for a read error.
    const size_t length = preferences.getString(key, line, sizeof line);
    preferences.end();
    if (!length)
        return RecordStoreStatus::FAILED;
    const size_t bytes = length - 1; // getString counts the NUL.
#else
    nvs_handle_t handle;
    esp_err_t error = openNamespace(NVS_READONLY, handle);
    if (error == ESP_ERR_NVS_NOT_FOUND)
        return RecordStoreStatus::NOT_FOUND;
    if (error != ESP_OK)
        return RecordStoreStatus::FAILED;
    size_t length = 0;
    error = nvs_get_str(handle, key, nullptr, &length);
    if (error == ESP_OK && length > sizeof line) {
        nvs_close(handle);
        return RecordStoreStatus::TOO_LARGE;
    }
    if (error == ESP_OK)
        error = nvs_get_str(handle, key, line, &length);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND)
        return RecordStoreStatus::NOT_FOUND;
    if (error != ESP_OK || !length)
        return RecordStoreStatus::FAILED;
    const size_t bytes = length - 1; // nvs_get_str counts the NUL.
#endif
    if (bytes + 1 > capacity)
        return RecordStoreStatus::TOO_LARGE;
    std::memcpy(text, line, bytes + 1);
    size = bytes;
    return RecordStoreStatus::OK;
}
} // namespace MotorControlCANExample
