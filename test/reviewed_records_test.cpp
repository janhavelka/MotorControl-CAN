// SPDX-License-Identifier: MIT
// The reviewed records of prompt 58 (deliverable 4): every committed line in docs/records decodes,
// every value equals the reviewed one stated here, and encodeRecord of the reviewed values writes the
// committed line byte for byte, so a stale CRC, a hand edit or a drifted value fails. The values and
// their raw evidence are in the review note beside each record (docs/records/*.md, the record's
// evidence field). `reviewed_records_test --write DIR` writes the lines with encodeRecord; that is
// the only way they are made.
#include <MotorControlCAN/profiles/lichuan_c/Records.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef MOTORCONTROLCAN_SOURCE_DIR
#error "MOTORCONTROLCAN_SOURCE_DIR names the source tree that holds docs/records"
#endif

namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;

namespace {
void check(bool valid, const char* name, int line) {
    if (!valid) {
        std::fprintf(stderr, "reviewed_records_test.cpp:%d (%s)\n", line, name);
        std::abort();
    }
}
#define CHECK(name, value) check(static_cast<bool>(value), name, __LINE__)

const L::RecordBasis MEASURED = L::RecordBasis::MEASURED;

void setEvidence(L::MeasuredRecord& r, const char* text) {
    std::memset(r.evidence, 0, sizeof r.evidence);
    std::strncpy(r.evidence, text, sizeof r.evidence - 1);
}
// The identity read 687 times on both specimens, spelled out rather than taken from
// measuredIdentity(), so that a change there cannot silently change a reviewed record.
L::IdentityValues specimenIdentity() {
    L::IdentityValues i;
    i.deviceType = 0x00040192;
    i.hardwareVersion = 0x000C;
    i.softwareVersion = 0x0020;
    i.vendorId = 0x00001000;
    i.productCode = 0x00000001;
    i.revision = 0x00000000;
    i.modelCode = 0x0400;
    i.firmwareCode = 0x0200;
    return i;
}
L::RecordRange range(uint32_t minimum, uint32_t maximum, L::RecordBasis basis) {
    L::RecordRange r;
    r.minimum = minimum;
    r.maximum = maximum;
    r.basis = basis;
    return r;
}
void setStarts(L::RecordStarts& starts, const uint16_t (*reports)[2], unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        starts.reports[i].statusword = reports[i][0];
        starts.reports[i].monitor = reports[i][1];
    }
    starts.count = static_cast<uint8_t>(count);
    starts.basis = MEASURED;
}

// Enable in mode 1 from the five measured start reports (177 enables, nodes 1 and 2).
L::MeasuredRecord enableMode1() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::ENABLE;
    setEvidence(r, "docs/records/lichuan_c_enable_mode1.md");
    r.model = L::SettingsModel::CL86_C;
    r.identity = specimenIdentity();
    r.enable.modeRaw = 1;
    r.enable.modeBasis = MEASURED;
    const uint16_t reports[5][2] = {
        {0x8437, 0x0001}, {0x4050, 0x0014}, {0x8050, 0x0011}, {0xC050, 0x0015}, {0x0050, 0x0010}};
    setStarts(r.enable.starts, reports, 5);
    return r;
}
// Relative moves with base ACTUAL, 6..300 rpm, ramps 100..1000 ms, up to 38200 counts (217 moves).
L::MeasuredRecord positionRelative() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::POSITION;
    setEvidence(r, "docs/records/lichuan_c_position_relative.md");
    r.model = L::SettingsModel::CL86_C;
    r.identity = specimenIdentity();
    L::PositionScope& p = r.position;
    p.move = L::RecordMoveKind::RELATIVE;
    p.relativeBase = L::RelativeBase::ACTUAL;
    p.baseBasis = MEASURED;
    p.rpm = range(6, 300, MEASURED);
    p.rampMs = range(100, 1000, MEASURED);
    p.maximumDistance = 38200;
    p.distanceBasis = MEASURED;
    p.acceptance.source = L::MoveSignalSource::STATUSWORD;
    p.acceptance.mask = p.acceptance.value = 0x1000;
    p.completion.source = L::MoveSignalSource::MONITOR;
    p.completion.mask = p.completion.value = 0x0001;
    p.rule = L::CompletionRule::AFTER_ABSENCE;
    p.signalsBasis = MEASURED;
    return r;
}
// Forward mode 3 runs, 6..60 rpm, 200 ms ramps, at most 12 s, HALT 6..60 and quick stop 6..40 rpm,
// released starts only (104 runs). 2201 = 1000 is inferred: no run read 2201.
L::MeasuredRecord velocityForward() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::VELOCITY;
    setEvidence(r, "docs/records/lichuan_c_velocity_forward.md");
    r.model = L::SettingsModel::CL86_C;
    r.identity = specimenIdentity();
    L::VelocityScope& v = r.velocity;
    v.layout = L::StatusLayout::ZH86;
    v.layoutBasis = L::RecordBasis::DOCUMENTED;
    v.rpm = range(6, 60, MEASURED);
    v.direction = L::RecordDirection::FORWARD;
    v.directionBasis = MEASURED;
    v.rampMs = range(200, 200, MEASURED);
    v.maximumDurationUs = 12000000;
    v.durationBasis = MEASURED;
    v.scale2201 = 1000;
    v.scaleBasis = L::RecordBasis::INFERRED;
    v.halt.admitted = true;
    v.halt.rpm = range(6, 60, MEASURED);
    v.quickStop.admitted = true;
    v.quickStop.rpm = range(6, 40, MEASURED);
    v.maximumWindowUs = 2000000;
    v.windowBasis = MEASURED;
    const uint16_t reports[2][2] = {{0x8050, 0x0011}, {0xC050, 0x0015}};
    setStarts(v.starts, reports, 2);
    return r;
}

struct Reviewed {
    const char* file; // Under docs/records.
    L::MeasuredRecord (*make)();
};
const Reviewed REVIEWED[] = {
    {"lichuan_c_enable_mode1.record", enableMode1},
    {"lichuan_c_position_relative.record", positionRelative},
    {"lichuan_c_velocity_forward.record", velocityForward},
};

bool sameRange(const L::RecordRange& a, const L::RecordRange& b) {
    return a.minimum == b.minimum && a.maximum == b.maximum && a.basis == b.basis;
}
bool sameStarts(const L::RecordStarts& a, const L::RecordStarts& b) {
    if (a.count != b.count || a.basis != b.basis)
        return false;
    for (unsigned i = 0; i < a.count; ++i)
        if (a.reports[i].statusword != b.reports[i].statusword || a.reports[i].monitor != b.reports[i].monitor)
            return false;
    return true;
}
bool sameSignal(const L::MoveSignal& a, const L::MoveSignal& b) {
    return a.source == b.source && a.mask == b.mask && a.value == b.value;
}
// Every field of the record's kind, one CHECK each, so a failure names the drifted value.
void compare(const L::MeasuredRecord& got, const L::MeasuredRecord& want) {
    CHECK("kind", got.kind == want.kind);
    CHECK("version", got.version == want.version);
    CHECK("evidence", std::strcmp(got.evidence, want.evidence) == 0);
    CHECK("model", got.model == want.model);
    const L::IdentityValues& a = got.identity;
    const L::IdentityValues& b = want.identity;
    CHECK("identity 1000", a.deviceType == b.deviceType);
    CHECK("identity 1009", a.hardwareVersion == b.hardwareVersion);
    CHECK("identity 100A", a.softwareVersion == b.softwareVersion);
    CHECK("identity 1018:01", a.vendorId == b.vendorId);
    CHECK("identity 1018:02", a.productCode == b.productCode);
    CHECK("identity 1018:03", a.revision == b.revision);
    CHECK("identity 2000", a.modelCode == b.modelCode);
    CHECK("identity 2001", a.firmwareCode == b.firmwareCode);
    if (want.kind == L::RecordKind::ENABLE) {
        CHECK("mode", got.enable.modeRaw == want.enable.modeRaw);
        CHECK("mode_basis", got.enable.modeBasis == want.enable.modeBasis);
        CHECK("starts", sameStarts(got.enable.starts, want.enable.starts));
    } else if (want.kind == L::RecordKind::POSITION) {
        const L::PositionScope& p = got.position;
        const L::PositionScope& q = want.position;
        CHECK("move", p.move == q.move);
        CHECK("base", p.relativeBase == q.relativeBase);
        CHECK("base_basis", p.baseBasis == q.baseBasis);
        CHECK("rpm", sameRange(p.rpm, q.rpm));
        CHECK("ramp_ms", sameRange(p.rampMs, q.rampMs));
        CHECK("distance_max", p.maximumDistance == q.maximumDistance);
        CHECK("distance_basis", p.distanceBasis == q.distanceBasis);
        CHECK("acceptance", sameSignal(p.acceptance, q.acceptance));
        CHECK("completion", sameSignal(p.completion, q.completion));
        CHECK("rule", p.rule == q.rule);
        CHECK("signals_basis", p.signalsBasis == q.signalsBasis);
    } else {
        const L::VelocityScope& v = got.velocity;
        const L::VelocityScope& w = want.velocity;
        CHECK("layout", v.layout == w.layout);
        CHECK("layout_basis", v.layoutBasis == w.layoutBasis);
        CHECK("rpm", sameRange(v.rpm, w.rpm));
        CHECK("direction", v.direction == w.direction);
        CHECK("direction_basis", v.directionBasis == w.directionBasis);
        CHECK("ramp_ms", sameRange(v.rampMs, w.rampMs));
        CHECK("duration_max_us", v.maximumDurationUs == w.maximumDurationUs);
        CHECK("duration_basis", v.durationBasis == w.durationBasis);
        CHECK("scale_2201", v.scale2201 == w.scale2201);
        CHECK("scale_basis", v.scaleBasis == w.scaleBasis);
        CHECK("halt", v.halt.admitted == w.halt.admitted && sameRange(v.halt.rpm, w.halt.rpm));
        CHECK("quick_stop", v.quickStop.admitted == w.quickStop.admitted &&
                                sameRange(v.quickStop.rpm, w.quickStop.rpm));
        CHECK("window_max_us", v.maximumWindowUs == w.maximumWindowUs);
        CHECK("window_basis", v.windowBasis == w.windowBasis);
        CHECK("starts", sameStarts(v.starts, w.starts));
    }
}

// The committed file: exactly one line, its LF or CRLF ending removed (Git may check it out either
// way). Returns the line length, or a negative value when the file is missing or malformed.
long readLine(const char* path, char* text, size_t capacity) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f)
        return -1;
    const size_t size = std::fread(text, 1, capacity, f);
    const bool more = std::fgetc(f) != EOF;
    std::fclose(f);
    if (more || size == capacity)
        return -2;
    size_t length = size;
    if (length && text[length - 1] == '\n')
        --length;
    if (length && text[length - 1] == '\r')
        --length;
    for (size_t i = 0; i < length; ++i)
        if (text[i] == '\n' || text[i] == '\r')
            return -3;
    text[length] = '\0';
    return static_cast<long>(length);
}

int writeAll(const char* directory) {
    for (const Reviewed& r : REVIEWED) {
        char text[L::RECORD_TEXT_CAPACITY + 1];
        size_t length = 0;
        L::RecordRefusal refusal;
        const L::MeasuredRecord record = r.make();
        if (!L::encodeRecord(text, sizeof text, length, refusal, record)) {
            std::fprintf(stderr, "%s: encodeRecord refused (%s)\n", r.file, refusal.key);
            return 1;
        }
        char path[512];
        if (std::snprintf(path, sizeof path, "%s/%s", directory, r.file) >= static_cast<int>(sizeof path))
            return 1;
        std::FILE* f = std::fopen(path, "wb");
        if (!f || std::fwrite(text, 1, length, f) != length || std::fputc('\n', f) == EOF) {
            if (f)
                std::fclose(f);
            return 1;
        }
        std::fclose(f);
        std::printf("%s\n", path);
    }
    return 0;
}

void checkCommitted() {
    for (const Reviewed& r : REVIEWED) {
        const L::MeasuredRecord want = r.make();
        char path[512];
        CHECK("path", std::snprintf(path, sizeof path, "%s/docs/records/%s", MOTORCONTROLCAN_SOURCE_DIR,
                                    r.file) < static_cast<int>(sizeof path));
        char text[L::RECORD_TEXT_CAPACITY + 2];
        const long length = readLine(path, text, sizeof text - 1);
        if (length < 0)
            std::fprintf(stderr, "%s: missing or not one line (%ld)\n", path, length);
        CHECK("committed line", length > 0);

        // Decodes, CRC included, to exactly the reviewed values.
        L::MeasuredRecord got;
        L::RecordRefusal refusal;
        const M::Status decoded = L::decodeRecord(got, refusal, text, static_cast<size_t>(length));
        if (!decoded)
            std::fprintf(stderr, "%s: decodeRecord refused at %s\n", r.file, refusal.key);
        CHECK("decode", decoded);
        compare(got, want);

        // The committed line is encodeRecord's own canonical form of the reviewed values.
        char encoded[L::RECORD_TEXT_CAPACITY + 1];
        size_t size = 0;
        CHECK("encode", L::encodeRecord(encoded, sizeof encoded, size, refusal, want));
        CHECK("canonical line", size == static_cast<size_t>(length) && std::memcmp(encoded, text, size) == 0);
        uint32_t id = 0;
        CHECK("record id", L::recordId(id, want) && id != 0);

        // The review note the evidence field names lies beside the record.
        char note[512];
        CHECK("note path", std::snprintf(note, sizeof note, "%s/%s", MOTORCONTROLCAN_SOURCE_DIR,
                                         want.evidence) < static_cast<int>(sizeof note));
        std::FILE* f = std::fopen(note, "rb");
        CHECK("review note", f != nullptr);
        if (f)
            std::fclose(f);
    }
}

// What each reviewed record opens through the evidence builders, as committed.
void checkConsequences() {
    L::EnableEvidence enable;
    L::IdentityReference reference;
    CHECK("enable evidence", L::makeEnableEvidence(enable, reference, enableMode1(), 7));
    CHECK("enable gate", enable.gate == L::EnableGate::MEASURED && enable.modeRaw == 1);
    CHECK("enable identity", reference.present && reference.values.deviceType == 0x00040192);

    L::MoveEvidence move;
    CHECK("move evidence", L::makeMoveEvidence(move, positionRelative()));
    CHECK("move gate", move.gate == L::MoveGate::MEASURED && move.relativeBase == L::RelativeBase::ACTUAL);
    CHECK("move rule", move.rule == L::CompletionRule::AFTER_ABSENCE);

    // 2201 = 1000 is INFERRED, and a measured velocity run needs a measured 2201: the reviewed
    // velocity record opens nothing until a run on this identity reads 2201 (docs/records note).
    L::VelocityEvidence velocity;
    velocity.record = 99;
    const M::Status refused = L::makeVelocityEvidence(velocity, velocityForward(), L::StopKind::HALT);
    CHECK("velocity refused", !refused && refused.code == M::Err::UNSUPPORTED &&
                                  refused.detail == static_cast<int32_t>(L::RecordCheckError::BASIS));
    CHECK("velocity unchanged", velocity.record == 99);
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--write") == 0)
        return writeAll(argv[2]);
    if (argc != 1) {
        std::fprintf(stderr, "usage: reviewed_records_test [--write DIR]\n");
        return 2;
    }
    checkCommitted();
    checkConsequences();
    std::puts("reviewed_records_test: ok");
    return 0;
}
