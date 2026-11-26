// SPDX-License-Identifier: MIT
// Prompt 58: the `record` routes. Each one calls ProbeMotion's record API on the selected axis
// (loadRecord, showRecord, useRecord, dropRecord) and, for save and restore, the application's RecordStore;
// the console holds no record and never touches a record slot itself. Nothing here sends a frame.
#include "ProbeConsoleText.h"

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
// Line bytes per record_line part: with the JSON envelope every part stays one record of at most
// RECORD_CAPACITY - 1 bytes, as the console's JSONL contract requires.
constexpr size_t RECORD_LINE_PART = 320;
const char* const SLOT_NAMES[RECORD_SLOTS] = {"enable", "absolute", "relative", "velocity"};
bool eq(const char* a, const char* b) {
    return !std::strcmp(a, b);
}
bool slotNamed(const char* word, RecordSlot& slot) {
    for (unsigned n = 0; n < RECORD_SLOTS; ++n)
        if (eq(word, SLOT_NAMES[n])) {
            slot = RecordSlot(n);
            return true;
        }
    return false;
}
const char* stateName(RecordState state) {
    switch (state) {
    case RecordState::EMPTY: return "empty";
    case RecordState::LOADED: return "loaded";
    case RecordState::BOUND: return "bound";
    case RecordState::INVALIDATED: return "invalidated";
    }
    return "unknown";
}
const char* kindName(L::RecordKind kind) {
    return kind == L::RecordKind::ENABLE ? "enable" : kind == L::RecordKind::POSITION ? "position" : "velocity";
}
// decodeRecord's reason as the console names it, in RecordError order.
const char* reasonName(L::RecordError reason) {
    static const char* const names[] = {"none",        "argument",      "overlap",     "capacity",
                                        "length",      "syntax",        "version",     "crc",
                                        "unknown_key", "duplicate_key", "missing_key", "unexpected_key",
                                        "number",      "value",         "range",       "limit",
                                        "identity",    "zero_crc"};
    const unsigned n = unsigned(reason);
    return n < sizeof names / sizeof names[0] ? names[n] : "unknown";
}
const char* storeFailure(RecordStoreStatus status) {
    return status == RecordStoreStatus::NOT_FOUND   ? "record_not_stored"
           : status == RecordStoreStatus::TOO_LARGE ? "record_store_too_large"
                                                    : "record_store_failed";
}
using Text = ConsoleText;
void saturate(uint32_t& value) {
    if (value != UINT32_MAX)
        ++value;
}
} // namespace

void ProbeConsole::dispatchRecord(const ConsoleLine& line, uint64_t now) noexcept {
    const bool json = line.json;
    const uint32_t id = line.id;
    const char* action = line.token(1);
    // One reply candidate for every outcome: the frame stays small on firmware stacks.
    Text text;
    const auto publish = [&]() {
        text.end(json);
        if (!text.valid || !enqueue(text.bytes, false, id))
            saturate(stats_.outputDropped);
    };
    const auto reject = [&](const char* reason, M::Err code, int32_t detail) {
        saturate(stats_.invalid);
        error(id, json, "record", reason, false, code, detail);
    };
    // decodeRecord's refusal, with its reason and offending key.
    const auto refuseLine = [&](const M::Status& status, const L::RecordRefusal& refusal) {
        saturate(stats_.invalid);
        text.reset();
        text.start(json, id, "reply", "record");
        if (json) {
            text.add(",\"ok\":false,\"status\":\"%s\",\"detail\":%ld,\"error\":\"record_%s\",\"key\":",
                     M::errToString(status.code), static_cast<long>(status.detail), reasonName(refusal.reason));
            text.string(refusal.key);
        } else
            text.add("rejected (record_%s key=%s)", reasonName(refusal.reason), refusal.key);
        publish();
    };
    const auto accepted = [&](const char* verb, RecordSlot slot, RecordState state, uint32_t record) {
        text.reset();
        text.start(json, id, "reply", "record");
        text.add(json ? ",\"ok\":true,\"status\":\"OK\",\"action\":\"%s\",\"slot\":\"%s\",\"state\":\"%s\","
                        "\"record_id\":\"%08lx\""
                      : "OK %s %s state=%s id=%08lx",
                 verb, SLOT_NAMES[unsigned(slot)], stateName(state), static_cast<unsigned long>(record));
        publish();
    };
    const bool load = eq(action, "load"), show = eq(action, "show"), use = eq(action, "use"),
               drop = eq(action, "drop"), save = eq(action, "save"), restore = eq(action, "restore");
    RecordSlot slot = RecordSlot::ENABLE;
    if (!(load || show || use || drop || save || restore) ||
        (load ? line.count != 3 : line.count != 3 || !slotNamed(line.token(2), slot))) {
        reject("invalid_arguments", M::Err::ILLEGAL_VALUE, 0);
        return;
    }
    ProbeMotion* motion = selectedMotion();
    if (!motion) {
        reject("axis_id_of_an_attached_axis_required", M::Err::INVALID_CONFIG, 0);
        return;
    }
    // show, save and restore use the one line buffer, which a show still printing holds.
    if ((show || save || restore) && (recordLine_.active || listing_.active)) {
        reject("record_output_busy", M::Err::INVALID_CONFIG, 0);
        return;
    }
    RecordView view;
    L::RecordRefusal refusal;
    if (load || use || restore) {
        M::Status status;
        if (load) {
            const char* tail = line.token(2);
            status = motion->loadRecord(tail, std::strlen(tail), refusal, slot);
        } else if (use) {
            status = motion->useRecord(slot, now);
        } else {
            if (!recordStore_.restore) {
                reject("no_record_storage", M::Err::UNSUPPORTED, 0);
                return;
            }
            size_t size = 0;
            const RecordStoreStatus stored =
                recordStore_.restore(recordStore_.context, motion->target().channel.binding.logicalAxisId, slot,
                                     recordLine_.text, sizeof recordLine_.text, size);
            if (stored != RecordStoreStatus::OK) {
                reject(storeFailure(stored), M::Err::INVALID_CONFIG, int32_t(stored));
                return;
            }
            // The stored bytes are decoded and validated whole, as `record load` does, and must be a
            // record of the slot named; then they are loaded, never bound.
            L::MeasuredRecord candidate;
            status = L::decodeRecord(candidate, refusal, recordLine_.text, size);
            if (status && recordSlotOf(candidate) != slot) {
                reject("record_slot_mismatch", M::Err::INVALID_CONFIG, 0);
                return;
            }
            RecordSlot loaded = slot;
            if (status)
                status = motion->loadRecord(candidate, refusal, loaded);
        }
        if (!status && refusal.reason != L::RecordError::NONE) {
            refuseLine(status, refusal);
            return;
        }
        if (!status || !motion->showRecord(slot, view)) {
            reject(status ? "record_slot_empty" : status.msg, status.code, status.detail);
            return;
        }
        accepted(action, slot, view.state, view.id);
        return;
    }
    if (drop) {
        const uint32_t record = motion->showRecord(slot, view) ? view.id : 0;
        if (!motion->dropRecord(slot)) {
            reject("record_slot_empty", M::Err::INVALID_CONFIG, 0);
            return;
        }
        accepted(action, slot, RecordState::EMPTY, record);
        return;
    }
    // show and save: the slot's canonical line.
    if (!motion->showRecord(slot, view)) {
        reject("record_slot_empty", M::Err::INVALID_CONFIG, 0);
        return;
    }
    if (save && !recordStore_.save) {
        reject("no_record_storage", M::Err::UNSUPPORTED, 0);
        return;
    }
    size_t length = 0;
    const M::Status encoded =
        L::encodeRecord(recordLine_.text, sizeof recordLine_.text, length, refusal, view.record);
    if (!encoded) {
        refuseLine(encoded, refusal);
        return;
    }
    const uint32_t axisId = motion->target().channel.binding.logicalAxisId;
    if (save) {
        const RecordStoreStatus stored =
            recordStore_.save(recordStore_.context, axisId, slot, recordLine_.text, length);
        if (stored != RecordStoreStatus::OK) {
            reject(storeFailure(stored), M::Err::INVALID_CONFIG, int32_t(stored));
            return;
        }
        accepted(action, slot, view.state, view.id);
        return;
    }
    const RecordBinding& b = view.binding;
    const unsigned parts = unsigned((length + RECORD_LINE_PART - 1) / RECORD_LINE_PART);
    text.start(json, id, "reply", "record");
    text.add(json ? ",\"ok\":true,\"status\":\"OK\",\"action\":\"show\",\"slot\":\"%s\",\"state\":\"%s\","
                    "\"record_id\":\"%08lx\",\"kind\":\"%s\",\"axis_id\":%lu,\"bound\":"
                  : "OK show %s state=%s id=%08lx kind=%s axis=%lu bound=",
             SLOT_NAMES[unsigned(slot)], stateName(view.state), static_cast<unsigned long>(view.id),
             kindName(view.record.kind), static_cast<unsigned long>(axisId));
    if (view.state == RecordState::BOUND || view.state == RecordState::INVALIDATED)
        text.add(json ? "{\"binding_generation\":%lu,\"configuration_generation\":%lu,\"node_generation\":%lu,"
                        "\"controller_epoch\":%lu,\"bound_us\":\"%llu\"}"
                      : "binding:%lu,configuration:%lu,node:%lu,epoch:%lu,us:%llu",
                 static_cast<unsigned long>(b.binding.bindingGeneration),
                 static_cast<unsigned long>(b.binding.configurationGeneration),
                 static_cast<unsigned long>(b.nodeGeneration), static_cast<unsigned long>(b.controllerEpoch),
                 static_cast<unsigned long long>(b.boundUs));
    else
        text.add(json ? "null" : "none");
    text.add(json ? ",\"line_bytes\":%u,\"line_parts\":%u" : " bytes=%u parts=%u", unsigned(length), parts);
    text.end(json);
    if (!text.valid || !enqueue(text.bytes, false, id)) {
        saturate(stats_.outputDropped);
        return;
    }
    recordLine_.active = true;
    recordLine_.json = json;
    recordLine_.id = id;
    recordLine_.slot = uint8_t(slot);
    recordLine_.part = 0;
    recordLine_.parts = uint8_t(parts);
    recordLine_.length = uint16_t(length);
}
// One record_line part per turn while the ordinary queue has room. Concatenated in order, the parts are
// exactly the canonical line.
void ProbeConsole::recordLineNext() noexcept {
    if (!recordLine_.active || ordinaryCount_ == ORDINARY_REPLIES)
        return;
    const size_t begin = size_t(recordLine_.part) * RECORD_LINE_PART;
    const size_t left = recordLine_.length - begin;
    const size_t size = left < RECORD_LINE_PART ? left : RECORD_LINE_PART;
    char part[RECORD_LINE_PART + 1];
    std::memcpy(part, recordLine_.text + begin, size);
    part[size] = '\0';
    const unsigned number = unsigned(recordLine_.part) + 1u, parts = unsigned(recordLine_.parts);
    Text text;
    text.start(recordLine_.json, recordLine_.id, "record_line", "record");
    if (recordLine_.json) {
        text.add(",\"slot\":\"%s\",\"part\":%u,\"parts\":%u,\"text\":", SLOT_NAMES[recordLine_.slot], number, parts);
        text.string(part);
    } else
        text.add("line %u/%u %s", number, parts, part);
    text.end(recordLine_.json);
    if (!text.valid || !enqueue(text.bytes, false, recordLine_.id)) {
        // A part that cannot be queued ends the parts rather than skipping one.
        recordLine_.active = false;
        saturate(stats_.outputDropped);
        return;
    }
    if (++recordLine_.part == recordLine_.parts)
        recordLine_.active = false;
}
} // namespace MotorControlCANExample
