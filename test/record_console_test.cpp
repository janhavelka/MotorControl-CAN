// SPDX-License-Identifier: MIT
// HOST_FAKE: the console's `record` routes (prompt 58, slice b4) over the actual owner, SDO engine, AxisSession
// and ProbeMotion: every route acts through the ProbeMotion record API, show prints the canonical line in
// bounded parts, use sends nothing, save and restore run only on their command, restore loads and never
// binds, and stored lines that are oversized, corrupt or of another slot are refused. The records are test
// inputs; none qualifies a drive.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
static unsigned checks = 0;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #condition, scenario);    \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "ProbeConsole.h"
#include "ProbeMotion.h"
#include "RecordStore.h"
#include "support/VelocityRig.h"

namespace {
using namespace VelocityRig;
struct Bytes {
    std::string text;
    static bool write(void* context, uint8_t value) {
        static_cast<Bytes*>(context)->text += char(value);
        return true;
    }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks c;
        c.context = this;
        c.writeByte = write;
        return c;
    }
};
void contains(const std::string& value, const char* expected) {
    if (value.find(expected) == std::string::npos)
        std::fprintf(stderr, "Missing [%s] in %s\n", expected, value.c_str());
    CHECK(value.find(expected) != std::string::npos);
}
// An in-memory store that counts every call, so that a test sees exactly when the console uses it.
struct MemoryStore {
    std::string lines[E::RECORD_SLOTS];
    bool present[E::RECORD_SLOTS] = {};
    unsigned saves = 0, restores = 0;
    uint32_t axis = 0;
    static E::RecordStoreStatus save(void* context, uint32_t axisId, E::RecordSlot slot, const char* text,
                                     size_t size) {
        MemoryStore& m = *static_cast<MemoryStore*>(context);
        ++m.saves;
        m.axis = axisId;
        m.lines[unsigned(slot)].assign(text, size);
        m.present[unsigned(slot)] = true;
        return E::RecordStoreStatus::OK;
    }
    static E::RecordStoreStatus restore(void* context, uint32_t axisId, E::RecordSlot slot, char* text,
                                        size_t capacity, size_t& size) {
        MemoryStore& m = *static_cast<MemoryStore*>(context);
        ++m.restores;
        m.axis = axisId;
        if (!m.present[unsigned(slot)])
            return E::RecordStoreStatus::NOT_FOUND;
        const std::string& line = m.lines[unsigned(slot)];
        if (line.size() + 1 > capacity)
            return E::RecordStoreStatus::TOO_LARGE;
        std::memcpy(text, line.data(), line.size());
        text[line.size()] = '\0';
        size = line.size();
        return E::RecordStoreStatus::OK;
    }
    E::RecordStore store() {
        E::RecordStore s;
        s.context = this;
        s.save = save;
        s.restore = restore;
        return s;
    }
};
struct ConsoleRig : Rig {
    Bytes bytes;
    E::ProbePlatform platform;
    E::ProbeApp app;
    L::ReadTarget live;
    M::AxisReference reference;
    E::ProbeMotion motion;
    E::ProbeConsole console;
    E::AxisRecords records;
    ConsoleRig()
        : platform(bytes.callbacks()), app(owner, platform), live(target()),
          motion(app, owner, session, live, axis, reference), console(app, owner, platform) {
        serviceSession = false;
        CHECK(app.select(live));
        CHECK(app.openAxis(session, live, L::SettingsModel::CL86_C, now) == &session);
        motion.attachRecords(records);
        CHECK(console.attachMotion(motion));
        CHECK(wire.sentCount == 0);
    }
    void tick() {
        now += TICK;
        CHECK(app.service(now));
        CHECK(motion.service(now));
        console.service(now);
    }
    std::string command(const std::string& line) {
        const size_t first = bytes.text.size();
        for (char c : line)
            console.feed(uint8_t(c), now);
        console.feed('\n', now);
        for (unsigned i = 0; i < 4000; ++i) {
            tick();
            if (console.drained() && !platform.stats().outputUsed)
                return bytes.text.substr(first);
        }
        CHECK(false);
        return std::string();
    }
};
L::IdentityValues rigIdentity() {
    L::IdentityValues i;
    i.deviceType = DEVICE_TYPE;
    i.hardwareVersion = HARDWARE;
    i.softwareVersion = SOFTWARE;
    i.vendorId = VENDOR;
    i.productCode = PRODUCT;
    i.revision = REVISION;
    i.modelCode = MODEL;
    i.firmwareCode = FIRMWARE;
    return i;
}
L::RecordRange range(uint32_t minimum, uint32_t maximum) {
    L::RecordRange r;
    r.minimum = minimum;
    r.maximum = maximum;
    r.basis = L::RecordBasis::MEASURED;
    return r;
}
L::MeasuredRecord velocityRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::VELOCITY;
    std::strncpy(r.evidence, "record-console-velocity", sizeof r.evidence - 1);
    r.model = L::SettingsModel::CL86_C;
    r.identity = rigIdentity();
    L::VelocityScope& v = r.velocity;
    v.layout = L::StatusLayout::ZH86;
    v.layoutBasis = L::RecordBasis::DOCUMENTED;
    v.rpm = range(6, 60);
    v.direction = L::RecordDirection::FORWARD;
    v.directionBasis = v.durationBasis = v.scaleBasis = L::RecordBasis::MEASURED;
    v.rampMs = range(100, 200);
    v.maximumDurationUs = DURATION;
    v.scale2201 = 3200;
    v.halt.admitted = true;
    v.halt.rpm = range(6, 60);
    v.windowBasis = L::RecordBasis::INFERRED;
    v.starts.count = 1;
    v.starts.reports[0].statusword = 0x0001;
    v.starts.reports[0].monitor = L::MONITOR_RELEASED;
    v.starts.basis = L::RecordBasis::MEASURED;
    return r;
}
// The longest valid record: it fills RECORD_TEXT_CAPACITY nearly, so the console line must hold it.
L::MeasuredRecord longestRecord() {
    L::MeasuredRecord r = velocityRecord();
    std::memset(r.evidence, 'e', L::RECORD_EVIDENCE_CAPACITY - 1);
    r.evidence[L::RECORD_EVIDENCE_CAPACITY - 1] = '\0';
    L::VelocityScope& v = r.velocity;
    v.layoutBasis = v.rpm.basis = v.directionBasis = v.rampMs.basis = v.durationBasis = v.scaleBasis =
        v.windowBasis = v.starts.basis = L::RecordBasis::DOCUMENTED;
    v.rpm = v.halt.rpm = v.quickStop.rpm = range(L::RECORD_MAXIMUM_RPM, L::RECORD_MAXIMUM_RPM);
    v.halt.rpm.basis = v.quickStop.rpm.basis = L::RecordBasis::DOCUMENTED;
    v.quickStop.admitted = true;
    v.rampMs = range(L::RECORD_MAXIMUM_RAMP_MS, L::RECORD_MAXIMUM_RAMP_MS);
    v.rampMs.basis = L::RecordBasis::DOCUMENTED;
    v.maximumDurationUs = v.maximumWindowUs = uint64_t(INT64_MAX);
    v.scale2201 = L::SUBDIVISION_MAXIMUM;
    v.starts.count = L::RECORD_START_REPORTS;
    for (unsigned i = 0; i < L::RECORD_START_REPORTS; ++i) {
        v.starts.reports[i].statusword = uint16_t(0xFF00 + i);
        v.starts.reports[i].monitor = 0xFFFF;
    }
    return r;
}
L::MeasuredRecord enableRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::ENABLE;
    std::strncpy(r.evidence, "record-console-enable", sizeof r.evidence - 1);
    r.model = L::SettingsModel::CL86_C;
    r.identity = rigIdentity();
    r.enable.modeRaw = 1;
    r.enable.modeBasis = L::RecordBasis::MEASURED;
    r.enable.starts.count = 1;
    r.enable.starts.reports[0].statusword = 0x8437;
    r.enable.starts.reports[0].monitor = 0x0001;
    r.enable.starts.basis = L::RecordBasis::MEASURED;
    return r;
}
std::string encoded(const L::MeasuredRecord& record) {
    std::vector<char> text(L::RECORD_TEXT_CAPACITY + 1);
    size_t length = 0;
    L::RecordRefusal refusal;
    CHECK(L::encodeRecord(text.data(), text.size(), length, refusal, record));
    return std::string(text.data(), length);
}
// The text fields of the record_line parts, concatenated in order.
std::string lineParts(const std::string& output, unsigned& parts) {
    std::string line;
    parts = 0;
    for (size_t at = output.find("\"type\":\"record_line\""); at != std::string::npos;
         at = output.find("\"type\":\"record_line\"", at + 1)) {
        const size_t text = output.find("\"text\":\"", at);
        CHECK(text != std::string::npos);
        const size_t end = output.find('"', text + 8);
        line += output.substr(text + 8, end - text - 8);
        ++parts;
    }
    return line;
}
// Every output line is one record within the console's 511-byte JSONL bound.
void bounded(const std::string& output) {
    size_t begin = 0;
    for (size_t end = output.find('\n'); end != std::string::npos; end = output.find('\n', begin)) {
        CHECK(end + 1 - begin < size_t(E::ProbeConsole::RECORD_CAPACITY));
        begin = end + 1;
    }
}

void routesActThroughTheApi() {
    scenario = "load, show, use and drop through ProbeMotion's record API";
    ConsoleRig r;
    const std::string line = encoded(velocityRecord());
    uint32_t id = 0;
    CHECK(L::recordId(id, velocityRecord()));
    char expectedId[32];
    std::snprintf(expectedId, sizeof expectedId, "\"record_id\":\"%08lx\"", static_cast<unsigned long>(id));
    std::string out = r.command("@1 record load " + line);
    contains(out, "\"ok\":true");
    contains(out, "\"action\":\"load\",\"slot\":\"velocity\",\"state\":\"loaded\"");
    contains(out, expectedId);
    E::RecordView view;
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::LOADED && view.id == id);
    // show: the reply, then the canonical line in parts that concatenate to it exactly.
    out = r.command("@2 record show velocity");
    contains(out, "\"state\":\"loaded\"");
    contains(out, "\"bound\":null");
    unsigned parts = 0;
    CHECK(lineParts(out, parts) == line && parts == (line.size() + 319) / 320);
    bounded(out);
    // use binds and sends nothing; the generations it shows are the axis's.
    const unsigned sent = r.wire.sentCount;
    out = r.command("@3 record use velocity");
    contains(out, "\"action\":\"use\",\"slot\":\"velocity\",\"state\":\"bound\"");
    CHECK(r.wire.sentCount == sent && !r.session.busy() && !r.motion.retained());
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::BOUND);
    out = r.command("@4 record show velocity");
    char bound[160];
    std::snprintf(bound, sizeof bound,
                  "\"bound\":{\"binding_generation\":%lu,\"configuration_generation\":%lu,\"node_generation\":%lu,"
                  "\"controller_epoch\":%lu,",
                  static_cast<unsigned long>(r.axis.binding.bindingGeneration),
                  static_cast<unsigned long>(r.axis.binding.configurationGeneration),
                  static_cast<unsigned long>(r.live.nodeGeneration),
                  static_cast<unsigned long>(r.owner.controllerEpoch()));
    contains(out, bound);
    contains(out, "\"state\":\"bound\"");
    // Refusals: a corrupted line names decodeRecord's reason and key, and leaves the slot as it was.
    std::string bad = encoded(enableRecord());
    bad[30] = bad[30] == 'a' ? 'b' : 'a';
    out = r.command("@5 record load " + bad);
    contains(out, "\"ok\":false");
    contains(out, "\"error\":\"record_crc\",\"key\":\"crc\"");
    CHECK(!r.motion.showRecord(E::RecordSlot::ENABLE, view));
    out = r.command("@6 record load " + line);
    contains(out, "\"error\":\"that record slot is occupied: drop it first\"");
    contains(r.command("@7 record show sideways"), "\"error\":\"invalid_arguments\"");
    contains(r.command("@8 record bind velocity"), "\"error\":\"invalid_arguments\"");
    contains(r.command("@9 record load"), "\"error\":\"invalid_arguments\"");
    contains(r.command("@10 record show enable"), "\"error\":\"record_slot_empty\"");
    contains(r.command("@11 record save velocity"), "\"error\":\"no_record_storage\"");
    contains(r.command("@12 record restore velocity"), "\"error\":\"no_record_storage\"");
    // drop forgets; the API and the console agree.
    out = r.command("@13 record drop velocity");
    contains(out, "\"action\":\"drop\",\"slot\":\"velocity\",\"state\":\"empty\"");
    CHECK(!r.motion.showRecord(E::RecordSlot::VELOCITY, view));
    contains(r.command("@14 record drop velocity"), "\"error\":\"record_slot_empty\"");
    // Human mode prints the same facts as plain lines.
    contains(r.command("record load " + line), "record: OK load velocity state=loaded");
    out = r.command("record show velocity");
    contains(out, "record: OK show velocity state=loaded");
    contains(out, "record: line 1/");
    CHECK(r.wire.sentCount == sent && !r.motion.retained());
}

void longestLineFits() {
    scenario = "the longest record line fits the console line";
    ConsoleRig r;
    const std::string line = encoded(longestRecord());
    CHECK(line.size() > 640 && line.size() <= L::RECORD_TEXT_CAPACITY);
    const std::string input = "@4294967295 record load " + line;
    CHECK(input.size() < E::CONSOLE_LINE_CAPACITY);
    contains(r.command(input), "\"ok\":true");
    const std::string out = r.command("@4294967294 record show velocity");
    unsigned parts = 0;
    CHECK(lineParts(out, parts) == line && parts == 3);
    bounded(out);
    // A line one byte past the console capacity is refused whole, before any route.
    std::string tooLong = "record load " + std::string(E::CONSOLE_LINE_CAPACITY, 'x');
    contains(r.command(tooLong), "invalid_line");
}

void saveAndRestoreOnlyOnCommand() {
    scenario = "save and restore run only on their command; restore loads and never binds";
    ConsoleRig r;
    MemoryStore memory;
    CHECK(r.console.attachRecordStore(memory.store()));
    const std::string line = encoded(velocityRecord());
    contains(r.command("@1 record load " + line), "\"ok\":true");
    contains(r.command("@2 record use velocity"), "\"state\":\"bound\"");
    contains(r.command("@3 record show velocity"), "\"ok\":true");
    for (unsigned i = 0; i < 200; ++i)
        r.tick();
    CHECK(!memory.saves && !memory.restores); // Nothing touched the store on its own.
    contains(r.command("@4 record save velocity"), "\"action\":\"save\",\"slot\":\"velocity\",\"state\":\"bound\"");
    CHECK(memory.saves == 1 && memory.lines[3] == line && memory.axis == r.live.channel.binding.logicalAxisId);
    contains(r.command("@5 record drop velocity"), "\"state\":\"empty\"");
    const unsigned sent = r.wire.sentCount;
    std::string out = r.command("@6 record restore velocity");
    contains(out, "\"action\":\"restore\",\"slot\":\"velocity\",\"state\":\"loaded\"");
    E::RecordView view;
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::LOADED);
    CHECK(memory.restores == 1 && r.wire.sentCount == sent && !r.motion.retained() && !r.session.busy());
    // Restore never replaces a held record, and finds nothing for an empty slot.
    contains(r.command("@7 record restore velocity"), "\"error\":\"that record slot is occupied: drop it first\"");
    contains(r.command("@8 record restore enable"), "\"error\":\"record_not_stored\"");
    // A stored line is decoded and validated whole: corrupt, of another slot, or oversized, it is refused.
    contains(r.command("@9 record drop velocity"), "\"state\":\"empty\"");
    std::string corrupt = line;
    corrupt[40] = corrupt[40] == 'a' ? 'b' : 'a';
    memory.lines[3] = corrupt;
    contains(r.command("@10 record restore velocity"), "\"error\":\"record_crc\",\"key\":\"crc\"");
    memory.lines[3] = encoded(enableRecord());
    contains(r.command("@11 record restore velocity"), "\"error\":\"record_slot_mismatch\"");
    memory.lines[3] = std::string(L::RECORD_TEXT_CAPACITY + 1, 'v');
    contains(r.command("@12 record restore velocity"), "\"error\":\"record_store_too_large\"");
    CHECK(!r.motion.showRecord(E::RecordSlot::VELOCITY, view));
    CHECK(r.wire.sentCount == sent);
}

void fileStoreRoundTrip() {
    scenario = "the native file store round trip and its refusals";
    ConsoleRig r;
    E::FileRecordStore files(".");
    CHECK(r.console.attachRecordStore(files.store()));
    char key[16];
    CHECK(E::recordStoreKey(r.live.channel.binding.logicalAxisId, E::RecordSlot::VELOCITY, key, sizeof key));
    const std::string name = std::string("./") + key + ".txt";
    std::remove(name.c_str());
    const std::string line = encoded(velocityRecord());
    contains(r.command("@1 record restore velocity"), "\"error\":\"record_not_stored\"");
    contains(r.command("@2 record load " + line), "\"ok\":true");
    contains(r.command("@3 record save velocity"), "\"ok\":true");
    std::FILE* file = std::fopen(name.c_str(), "rb");
    CHECK(file);
    std::vector<char> stored(L::RECORD_TEXT_CAPACITY + 2);
    const size_t size = std::fread(stored.data(), 1, stored.size(), file);
    std::fclose(file);
    CHECK(std::string(stored.data(), size) == line); // The canonical line, no line ending.
    contains(r.command("@4 record drop velocity"), "\"ok\":true");
    contains(r.command("@5 record restore velocity"), "\"state\":\"loaded\"");
    E::RecordView view;
    CHECK(r.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::LOADED);
    // Saving again replaces the file whole.
    contains(r.command("@6 record save velocity"), "\"ok\":true");
    // An oversized file is refused without touching the slot; so is a truncated one.
    contains(r.command("@7 record drop velocity"), "\"ok\":true");
    file = std::fopen(name.c_str(), "wb");
    CHECK(file);
    const std::string big(L::RECORD_TEXT_CAPACITY + 5, 'x');
    CHECK(std::fwrite(big.data(), 1, big.size(), file) == big.size());
    std::fclose(file);
    contains(r.command("@8 record restore velocity"), "\"error\":\"record_store_too_large\"");
    file = std::fopen(name.c_str(), "wb");
    CHECK(file);
    CHECK(std::fwrite(line.data(), 1, line.size() - 5, file) == line.size() - 5);
    std::fclose(file);
    contains(r.command("@9 record restore velocity"), "\"error\":\"record_number\",\"key\":\"crc\"");
    CHECK(!r.motion.showRecord(E::RecordSlot::VELOCITY, view));
    std::remove(name.c_str());
    // A directory that does not exist fails the save, and the record stays loaded.
    E::FileRecordStore missing("./no-such-directory-for-records");
    ConsoleRig other;
    CHECK(other.console.attachRecordStore(missing.store()));
    contains(other.command("@1 record load " + line), "\"ok\":true");
    contains(other.command("@2 record save velocity"), "\"error\":\"record_store_failed\"");
    CHECK(other.motion.showRecord(E::RecordSlot::VELOCITY, view) && view.state == E::RecordState::LOADED);
}
} // namespace

int main() {
    routesActThroughTheApi();
    longestLineFits();
    saveAndRestoreOnlyOnCommand();
    fileStoreRoundTrip();
    std::printf("record_console_test: %u checks passed\n", checks);
    return 0;
}
