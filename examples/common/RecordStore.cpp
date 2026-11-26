// SPDX-License-Identifier: MIT
#include "RecordStore.h"
#include <cstdio>
#include <cstring>

namespace MotorControlCANExample {
namespace {
bool exists(const char* name) {
    std::FILE* file = std::fopen(name, "rb");
    if (!file)
        return false;
    std::fclose(file);
    return true;
}
} // namespace
RecordStore FileRecordStore::store() noexcept {
    RecordStore hook;
    hook.context = this;
    hook.save = save;
    hook.restore = restore;
    return hook;
}
bool FileRecordStore::path(uint32_t axisId, RecordSlot slot, const char* suffix, char* output) const noexcept {
    char key[16];
    if (!directory_ || !recordStoreKey(axisId, slot, key, sizeof key))
        return false;
    const int size = std::snprintf(output, PATH_CAPACITY, "%s/%s%s", directory_, key, suffix);
    return size > 0 && size < int(PATH_CAPACITY);
}
RecordStoreStatus FileRecordStore::save(void* context, uint32_t axisId, RecordSlot slot, const char* text,
                                        size_t size) {
    const FileRecordStore& self = *static_cast<const FileRecordStore*>(context);
    if (!text || !size)
        return RecordStoreStatus::FAILED;
    if (size > MotorControlCAN::LICHUAN_C::RECORD_TEXT_CAPACITY)
        return RecordStoreStatus::TOO_LARGE;
    char temporary[PATH_CAPACITY], target[PATH_CAPACITY];
    if (!self.path(axisId, slot, ".tmp", temporary) || !self.path(axisId, slot, ".txt", target))
        return RecordStoreStatus::FAILED;
    std::FILE* file = std::fopen(temporary, "wb");
    if (!file)
        return RecordStoreStatus::FAILED;
    const bool written = std::fwrite(text, 1, size, file) == size;
    const bool closed = std::fclose(file) == 0;
    // Windows rename refuses an existing destination; the old line is removed only once the new one is
    // complete on the medium.
    if (!written || !closed || (std::remove(target) != 0 && exists(target)) ||
        std::rename(temporary, target) != 0) {
        std::remove(temporary);
        return RecordStoreStatus::FAILED;
    }
    return RecordStoreStatus::OK;
}
RecordStoreStatus FileRecordStore::restore(void* context, uint32_t axisId, RecordSlot slot, char* text,
                                           size_t capacity, size_t& size) {
    const FileRecordStore& self = *static_cast<const FileRecordStore*>(context);
    char name[PATH_CAPACITY];
    if (!text || capacity < 2 || !self.path(axisId, slot, ".txt", name))
        return RecordStoreStatus::FAILED;
    std::FILE* file = std::fopen(name, "rb");
    if (!file)
        return RecordStoreStatus::NOT_FOUND;
    // Read one byte more than fits, into a local buffer, so that an oversized file is refused whole and
    // the caller's buffer stays unchanged on every failure.
    char buffer[MotorControlCAN::LICHUAN_C::RECORD_TEXT_CAPACITY + 2];
    const size_t limit = capacity - 1 < sizeof buffer - 1 ? capacity - 1 : sizeof buffer - 1;
    const size_t read = std::fread(buffer, 1, limit + 1, file);
    const bool failed = std::ferror(file) != 0;
    const bool closed = std::fclose(file) == 0;
    if (failed || !closed)
        return RecordStoreStatus::FAILED;
    if (read > limit)
        return RecordStoreStatus::TOO_LARGE;
    std::memcpy(text, buffer, read);
    text[read] = '\0';
    size = read;
    return RecordStoreStatus::OK;
}
} // namespace MotorControlCANExample
