// SPDX-License-Identifier: MIT
// Measured record codec (prompt 58, slice b1): CRC-32 vectors, golden text, round trips, every
// refusal reason with its key, partial and out-of-range records, unchanged outputs and no heap.
// The fixture values below are test data in the shape of the stage (a) evidence, not the reviewed
// records of deliverable 4; the golden CRCs were computed with Python's zlib.crc32.
#include <MotorControlCAN/profiles/lichuan_c/Records.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;

namespace {
// Every allocation while watching counts; the codec must make none.
bool watching = false;
unsigned long allocations = 0;
} // namespace
void* operator new(std::size_t size) {
    if (watching)
        ++allocations;
    void* p = std::malloc(size ? size : 1);
    if (!p)
        throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t size) {
    if (watching)
        ++allocations;
    void* p = std::malloc(size ? size : 1);
    if (!p)
        throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }

namespace {
void check(bool valid, int line) {
    if (!valid) {
        std::fprintf(stderr, "records_test.cpp:%d\n", line);
        std::abort();
    }
}
#define CHECK(value) check(static_cast<bool>(value), __LINE__)

#define IDENTITY_TEXT "identity=00040192.000C.0020.00001000.00000001.00000000.0400.0200"
const char* const ENABLE_BODY =
    "v=1 kind=enable evidence=p58-enable-fixture model=cl86_c " IDENTITY_TEXT
    " mode=1 mode_basis=measured starts=8437/0001,4050/0014,8050/0011,C050/0015,0050/0010"
    " starts_basis=measured";
const char* const POSITION_BODY =
    "v=1 kind=position evidence=p58-position-fixture model=cl86_c " IDENTITY_TEXT
    " move=relative base=actual base_basis=measured rpm=2..300 rpm_basis=measured"
    " ramp_ms=100..2000 ramp_basis=measured distance_max=169410 distance_basis=measured"
    " acceptance=statusword/1000/1000 completion=monitor/0001/0001 rule=after_absence"
    " signals_basis=measured";
const char* const VELOCITY_BODY =
    "v=1 kind=velocity evidence=p58-velocity-fixture model=cl86_c " IDENTITY_TEXT
    " layout=zh86 layout_basis=documented rpm=6..60 rpm_basis=measured direction=forward"
    " direction_basis=measured ramp_ms=200..200 ramp_basis=measured duration_max_us=12000000"
    " duration_basis=measured scale_2201=1000 scale_basis=measured halt_rpm=6..60"
    " halt_basis=measured quick_stop_rpm=6..60 quick_stop_basis=measured window_max_us=800000"
    " window_basis=inferred starts=8050/0011 starts_basis=measured";

void setEvidence(L::MeasuredRecord& r, const char* text) {
    std::memset(r.evidence, 0, sizeof r.evidence);
    std::strncpy(r.evidence, text, sizeof r.evidence - 1);
}
L::MeasuredRecord enableRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::ENABLE;
    setEvidence(r, "p58-enable-fixture");
    r.model = L::SettingsModel::CL86_C;
    r.identity = L::measuredIdentity();
    r.enable.modeRaw = 1;
    r.enable.modeBasis = L::RecordBasis::MEASURED;
    const uint16_t reports[5][2] = {
        {0x8437, 0x0001}, {0x4050, 0x0014}, {0x8050, 0x0011}, {0xC050, 0x0015}, {0x0050, 0x0010}};
    for (unsigned i = 0; i < 5; ++i) {
        r.enable.starts.reports[i].statusword = reports[i][0];
        r.enable.starts.reports[i].monitor = reports[i][1];
    }
    r.enable.starts.count = 5;
    r.enable.starts.basis = L::RecordBasis::MEASURED;
    return r;
}
L::RecordRange range(uint32_t minimum, uint32_t maximum, L::RecordBasis basis) {
    L::RecordRange r;
    r.minimum = minimum;
    r.maximum = maximum;
    r.basis = basis;
    return r;
}
L::MeasuredRecord positionRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::POSITION;
    setEvidence(r, "p58-position-fixture");
    r.model = L::SettingsModel::CL86_C;
    r.identity = L::measuredIdentity();
    L::PositionScope& p = r.position;
    p.move = L::RecordMoveKind::RELATIVE;
    p.relativeBase = L::RelativeBase::ACTUAL;
    p.baseBasis = L::RecordBasis::MEASURED;
    p.rpm = range(2, 300, L::RecordBasis::MEASURED);
    p.rampMs = range(100, 2000, L::RecordBasis::MEASURED);
    p.maximumDistance = 169410;
    p.distanceBasis = L::RecordBasis::MEASURED;
    p.acceptance.source = L::MoveSignalSource::STATUSWORD;
    p.acceptance.mask = p.acceptance.value = 0x1000;
    p.completion.source = L::MoveSignalSource::MONITOR;
    p.completion.mask = p.completion.value = 0x0001;
    p.rule = L::CompletionRule::AFTER_ABSENCE;
    p.signalsBasis = L::RecordBasis::MEASURED;
    return r;
}
L::MeasuredRecord absoluteRecord() {
    L::MeasuredRecord r = positionRecord();
    setEvidence(r, "p58-absolute-fixture");
    r.position.move = L::RecordMoveKind::ABSOLUTE;
    r.position.relativeBase = L::RelativeBase::UNMEASURED;
    r.position.baseBasis = L::RecordBasis::NONE;
    r.position.rpm = range(1, 60, L::RecordBasis::MEASURED);
    r.position.maximumDistance = 2000;
    r.position.rule = L::CompletionRule::AFTER_ACCEPTANCE;
    r.position.signalsBasis = L::RecordBasis::INFERRED;
    return r;
}
L::MeasuredRecord velocityRecord() {
    L::MeasuredRecord r;
    r.kind = L::RecordKind::VELOCITY;
    setEvidence(r, "p58-velocity-fixture");
    r.model = L::SettingsModel::CL86_C;
    r.identity = L::measuredIdentity();
    L::VelocityScope& v = r.velocity;
    v.layout = L::StatusLayout::ZH86;
    v.layoutBasis = L::RecordBasis::DOCUMENTED;
    v.rpm = range(6, 60, L::RecordBasis::MEASURED);
    v.direction = L::RecordDirection::FORWARD;
    v.directionBasis = L::RecordBasis::MEASURED;
    v.rampMs = range(200, 200, L::RecordBasis::MEASURED);
    v.maximumDurationUs = 12000000;
    v.durationBasis = L::RecordBasis::MEASURED;
    v.scale2201 = 1000;
    v.scaleBasis = L::RecordBasis::MEASURED;
    v.halt.admitted = true;
    v.halt.rpm = range(6, 60, L::RecordBasis::MEASURED);
    v.quickStop.admitted = true;
    v.quickStop.rpm = range(6, 60, L::RecordBasis::MEASURED);
    v.maximumWindowUs = 800000;
    v.windowBasis = L::RecordBasis::INFERRED;
    v.starts.count = 1;
    v.starts.reports[0].statusword = 0x8050;
    v.starts.reports[0].monitor = 0x0011;
    v.starts.basis = L::RecordBasis::MEASURED;
    return r;
}
// Halt only, both directions, a 2201 at each documented end: the optional pair and other words.
L::MeasuredRecord velocityHaltOnly() {
    L::MeasuredRecord r = velocityRecord();
    setEvidence(r, "p58-velocity-halt-only");
    r.velocity.layout = L::StatusLayout::OP86;
    r.velocity.direction = L::RecordDirection::BOTH;
    r.velocity.quickStop = L::RecordStop();
    r.velocity.scale2201 = L::SUBDIVISION_MAXIMUM;
    r.velocity.maximumWindowUs = 0;
    return r;
}

bool sameRange(const L::RecordRange& a, const L::RecordRange& b) {
    return a.minimum == b.minimum && a.maximum == b.maximum && a.basis == b.basis;
}
bool sameSignal(const L::MoveSignal& a, const L::MoveSignal& b) {
    return a.source == b.source && a.mask == b.mask && a.value == b.value;
}
bool sameStarts(const L::RecordStarts& a, const L::RecordStarts& b) {
    if (a.count != b.count || a.basis != b.basis)
        return false;
    for (unsigned i = 0; i < a.count; ++i)
        if (a.reports[i].statusword != b.reports[i].statusword ||
            a.reports[i].monitor != b.reports[i].monitor)
            return false;
    return true;
}
bool sameStop(const L::RecordStop& a, const L::RecordStop& b) {
    return a.admitted == b.admitted && sameRange(a.rpm, b.rpm);
}
// Every field, those a kind ignores included: the fixtures keep them at their defaults.
bool sameRecord(const L::MeasuredRecord& a, const L::MeasuredRecord& b) {
    const L::IdentityValues &i = a.identity, &j = b.identity;
    const L::PositionScope &p = a.position, &q = b.position;
    const L::VelocityScope &v = a.velocity, &w = b.velocity;
    return a.kind == b.kind && a.version == b.version &&
           std::strcmp(a.evidence, b.evidence) == 0 && a.model == b.model &&
           i.deviceType == j.deviceType && i.hardwareVersion == j.hardwareVersion &&
           i.softwareVersion == j.softwareVersion && i.vendorId == j.vendorId &&
           i.productCode == j.productCode && i.revision == j.revision &&
           i.modelCode == j.modelCode && i.firmwareCode == j.firmwareCode &&
           a.enable.modeRaw == b.enable.modeRaw && a.enable.modeBasis == b.enable.modeBasis &&
           sameStarts(a.enable.starts, b.enable.starts) && p.move == q.move &&
           p.relativeBase == q.relativeBase && p.baseBasis == q.baseBasis &&
           sameRange(p.rpm, q.rpm) && sameRange(p.rampMs, q.rampMs) &&
           p.maximumDistance == q.maximumDistance && p.distanceBasis == q.distanceBasis &&
           sameSignal(p.acceptance, q.acceptance) && sameSignal(p.completion, q.completion) &&
           p.rule == q.rule && p.signalsBasis == q.signalsBasis && v.layout == w.layout &&
           v.layoutBasis == w.layoutBasis && sameRange(v.rpm, w.rpm) &&
           v.direction == w.direction && v.directionBasis == w.directionBasis &&
           sameRange(v.rampMs, w.rampMs) && v.maximumDurationUs == w.maximumDurationUs &&
           v.durationBasis == w.durationBasis && v.scale2201 == w.scale2201 &&
           v.scaleBasis == w.scaleBasis && sameStop(v.halt, w.halt) &&
           sameStop(v.quickStop, w.quickStop) && v.maximumWindowUs == w.maximumWindowUs &&
           v.windowBasis == w.windowBasis && sameStarts(v.starts, w.starts);
}

struct Line {
    char text[L::RECORD_TEXT_CAPACITY + 64];
    size_t size;
};
// body plus its own crc field.
Line seal(const char* body) {
    Line line;
    const size_t n = std::strlen(body);
    CHECK(n + 14 <= sizeof line.text);
    std::memcpy(line.text, body, n);
    std::snprintf(line.text + n, sizeof line.text - n, " crc=%08lx",
                  static_cast<unsigned long>(L::crc32Ieee(0, body, n)));
    line.size = std::strlen(line.text);
    return line;
}
// body with the field named key replaced by replacement ("" removes it; an absent key appends).
Line variant(const char* body, const char* key, const char* replacement) {
    Line out;
    out.size = 0;
    const size_t keySize = std::strlen(key);
    bool found = false;
    for (const char* field = body; *field;) {
        const char* end = std::strchr(field, ' ');
        const size_t size = end ? size_t(end - field) : std::strlen(field);
        const bool match = size > keySize && field[keySize] == '=' &&
                           std::strncmp(field, key, keySize) == 0;
        const char* text = match ? replacement : field;
        const size_t textSize = match ? std::strlen(replacement) : size;
        found = found || match;
        if (textSize) {
            if (out.size)
                out.text[out.size++] = ' ';
            CHECK(out.size + textSize < sizeof out.text);
            std::memcpy(out.text + out.size, text, textSize);
            out.size += textSize;
        }
        field += size + (end ? 1 : 0);
    }
    if (!found && *replacement) {
        const size_t textSize = std::strlen(replacement);
        CHECK(out.size + textSize + 1 < sizeof out.text);
        out.text[out.size++] = ' ';
        std::memcpy(out.text + out.size, replacement, textSize);
        out.size += textSize;
    }
    out.text[out.size] = '\0';
    return out;
}
Line sealedVariant(const char* body, const char* key, const char* replacement) {
    return seal(variant(body, key, replacement).text);
}

// A decode failure that leaves the output's bytes unchanged and names reason and key.
void refused(const char* text, size_t size, L::RecordError reason, const char* key, M::Err code) {
    L::MeasuredRecord output = velocityRecord();
    unsigned char before[sizeof output];
    std::memcpy(before, &output, sizeof output);
    L::RecordRefusal refusal;
    const M::Status s = L::decodeRecord(output, refusal, text, size);
    if (refusal.reason != reason || std::strcmp(refusal.key, key) != 0)
        std::fprintf(stderr, "refusal %d '%s' for: %.*s\n", int(refusal.reason), refusal.key,
                     int(size), text);
    CHECK(s.code == code);
    CHECK(s.detail == int32_t(reason));
    CHECK(refusal.reason == reason);
    CHECK(std::strcmp(refusal.key, key) == 0);
    CHECK(std::memcmp(before, &output, sizeof output) == 0);
}
void refusedLine(const Line& line, L::RecordError reason, const char* key, M::Err code) {
    refused(line.text, line.size, reason, key, code);
}
// validateRecord and encodeRecord both refuse a candidate built in code, encode writing nothing.
void invalid(const L::MeasuredRecord& r, L::RecordError reason, const char* key, M::Err code) {
    L::RecordRefusal refusal;
    M::Status s = L::validateRecord(refusal, r);
    CHECK(s.code == code && s.detail == int32_t(reason));
    CHECK(refusal.reason == reason && std::strcmp(refusal.key, key) == 0);
    char text[L::RECORD_TEXT_CAPACITY + 1];
    std::memset(text, 'x', sizeof text);
    size_t length = 77;
    s = L::encodeRecord(text, sizeof text, length, refusal, r);
    CHECK(s.code == code && s.detail == int32_t(reason) && length == 77);
    CHECK(refusal.reason == reason && std::strcmp(refusal.key, key) == 0);
    for (size_t i = 0; i < sizeof text; ++i)
        CHECK(text[i] == 'x');
}

void crcVectors() {
    CHECK(L::crc32Ieee(0, "", 0) == 0);
    CHECK(L::crc32Ieee(0, "123456789", 9) == 0xCBF43926u);
    CHECK(L::crc32Ieee(0, "T", 1) == 0xbe047a60u); // zlib.crc32(b"T"), the bench line's first byte
    const char* fox = "The quick brown fox jumps over the lazy dog";
    CHECK(L::crc32Ieee(0, fox, std::strlen(fox)) == 0x414fa339u);
    // Chaining over any split equals the whole, as zlib's running value does.
    for (size_t split = 0; split <= 9; ++split)
        CHECK(L::crc32Ieee(L::crc32Ieee(0, "123456789", split), &"123456789"[split], 9 - split) ==
              0xCBF43926u);
    CHECK(L::crc32Ieee(0x12345678u, nullptr, 5) == 0x12345678u);
    CHECK(L::crc32Ieee(0x12345678u, "abc", 0) == 0x12345678u);
    const char bytes[2] = {'\0', char(0xff)}; // Bytes, not a C string: NUL and high bit included.
    CHECK(L::crc32Ieee(0, bytes, 2) == 0x6cdbfd72u); // zlib.crc32(b"\x00\xff")
}

// Golden text: encode writes exactly the documented canonical line, CRC included.
void goldenText() {
    struct Golden {
        L::MeasuredRecord record;
        const char* body;
        const char* crc;
    };
    const Golden golden[] = {{enableRecord(), ENABLE_BODY, " crc=1639d61f"},
                             {positionRecord(), POSITION_BODY, " crc=faa7993d"},
                             {velocityRecord(), VELOCITY_BODY, " crc=32f092c1"}};
    for (const Golden& g : golden) {
        char expected[L::RECORD_TEXT_CAPACITY + 1];
        std::snprintf(expected, sizeof expected, "%s%s", g.body, g.crc);
        char text[L::RECORD_TEXT_CAPACITY + 1];
        size_t length = 0;
        L::RecordRefusal refusal;
        CHECK(L::encodeRecord(text, sizeof text, length, refusal, g.record).isOk());
        CHECK(std::strcmp(text, expected) == 0 && length == std::strlen(expected));
        CHECK(refusal.reason == L::RecordError::NONE && refusal.key[0] == '\0');
        L::MeasuredRecord decoded;
        CHECK(L::decodeRecord(decoded, refusal, expected, std::strlen(expected)).isOk());
        CHECK(sameRecord(decoded, g.record));
    }
}

void roundTrips() {
    const L::MeasuredRecord records[] = {enableRecord(), positionRecord(), absoluteRecord(),
                                         velocityRecord(), velocityHaltOnly()};
    for (const L::MeasuredRecord& r : records) {
        L::RecordRefusal refusal;
        CHECK(L::validateRecord(refusal, r).isOk());
        char text[L::RECORD_TEXT_CAPACITY + 1];
        size_t length = 0;
        CHECK(L::encodeRecord(text, sizeof text, length, refusal, r).isOk());
        CHECK(length == std::strlen(text) && length <= L::RECORD_TEXT_CAPACITY);
        CHECK(std::strncmp(text, "v=1 ", 4) == 0 && std::strncmp(text + length - 13, " crc=", 5) == 0);
        L::MeasuredRecord decoded;
        CHECK(L::decodeRecord(decoded, refusal, text, length).isOk());
        CHECK(sameRecord(decoded, r));
        char again[L::RECORD_TEXT_CAPACITY + 1];
        size_t againLength = 0;
        CHECK(L::encodeRecord(again, sizeof again, againLength, refusal, decoded).isOk());
        CHECK(againLength == length && std::strcmp(again, text) == 0);
    }
    // Field order is free between v and crc: a reversed middle decodes to the same record.
    char text[L::RECORD_TEXT_CAPACITY + 1];
    size_t length = 0;
    L::RecordRefusal refusal;
    const L::MeasuredRecord original = positionRecord();
    CHECK(L::encodeRecord(text, sizeof text, length, refusal, original).isOk());
    const char* fields[40];
    size_t sizes[40], count = 0;
    for (const char* field = std::strchr(text, ' ') + 1; count < 40;) {
        const char* end = std::strchr(field, ' ');
        if (std::strncmp(field, "crc=", 4) == 0)
            break;
        fields[count] = field;
        sizes[count++] = size_t(end - field);
        field = end + 1;
    }
    char body[L::RECORD_TEXT_CAPACITY + 1] = "v=1";
    size_t at = 3;
    while (count--) {
        body[at++] = ' ';
        std::memcpy(body + at, fields[count], sizes[count]);
        at += sizes[count];
    }
    body[at] = '\0';
    const Line reversed = seal(body);
    L::MeasuredRecord decoded;
    CHECK(L::decodeRecord(decoded, refusal, reversed.text, reversed.size).isOk());
    CHECK(sameRecord(decoded, original));
}

// The fields a kind, move or stop admission ignores: validated and encoded as absent, decoded as
// defaults.
void ignoredFields() {
    L::MeasuredRecord r = absoluteRecord();
    r.position.relativeBase = L::RelativeBase::COMMANDED; // Absolute: ignored.
    r.enable.modeRaw = 99;                               // Other kinds' scopes: ignored.
    r.velocity.layout = L::StatusLayout::ZH57;
    L::RecordRefusal refusal;
    CHECK(L::validateRecord(refusal, r).isOk());
    char text[L::RECORD_TEXT_CAPACITY + 1];
    size_t length = 0;
    CHECK(L::encodeRecord(text, sizeof text, length, refusal, r).isOk());
    CHECK(!std::strstr(text, " base=") && !std::strstr(text, " mode=") && !std::strstr(text, " layout="));
    L::MeasuredRecord decoded;
    CHECK(L::decodeRecord(decoded, refusal, text, length).isOk());
    CHECK(sameRecord(decoded, absoluteRecord()));
    L::MeasuredRecord v = velocityHaltOnly();
    v.velocity.quickStop.rpm = range(9000, 1, L::RecordBasis::NONE); // Not admitted: ignored.
    CHECK(L::validateRecord(refusal, v).isOk());
    CHECK(L::encodeRecord(text, sizeof text, length, refusal, v).isOk());
    CHECK(!std::strstr(text, "quick_stop"));
    CHECK(L::decodeRecord(decoded, refusal, text, length).isOk());
    CHECK(sameRecord(decoded, velocityHaltOnly()));
}

void crcRefusals() {
    const Line good = seal(ENABLE_BODY);
    // Any changed byte of the covered text, without a new CRC.
    Line line = good;
    line.text[30] = line.text[30] == 'a' ? 'b' : 'a';
    refusedLine(line, L::RecordError::CRC, "crc", M::Err::INVALID_CONFIG);
    line = good;
    line.text[line.size - 1] = line.text[line.size - 1] == '0' ? '1' : '0';
    refusedLine(line, L::RecordError::CRC, "crc", M::Err::INVALID_CONFIG);
    // A CRC that is not eight lowercase digits.
    line = good;
    for (size_t i = line.size - 8; i < line.size; ++i)
        if (line.text[i] >= 'a' && line.text[i] <= 'f')
            line.text[i] = char(line.text[i] - 'a' + 'A');
    refusedLine(line, L::RecordError::NUMBER, "crc", M::Err::INVALID_CONFIG);
    line = good;
    line.text[--line.size] = '\0';
    refusedLine(line, L::RecordError::NUMBER, "crc", M::Err::INVALID_CONFIG);
    // No crc field, or crc not last.
    refused(ENABLE_BODY, std::strlen(ENABLE_BODY), L::RecordError::MISSING_KEY, "crc",
            M::Err::INVALID_CONFIG);
    char moved[sizeof(Line::text) + 16]; // Room for any Line, so the copy cannot truncate.
    std::snprintf(moved, sizeof moved, "%s mode=1", good.text);
    refused(moved, std::strlen(moved), L::RecordError::MISSING_KEY, "crc", M::Err::INVALID_CONFIG);
    refused("v=1", 3, L::RecordError::MISSING_KEY, "crc", M::Err::INVALID_CONFIG);
    // The CRC covers the text before its separating space, as zlib.crc32(line[:rindex(" crc=")]).
    char spaced[L::RECORD_TEXT_CAPACITY + 32];
    std::snprintf(spaced, sizeof spaced, "%s ", ENABLE_BODY);
    char wide[L::RECORD_TEXT_CAPACITY + 32];
    std::snprintf(wide, sizeof wide, "%scrc=%08lx", spaced,
                  static_cast<unsigned long>(L::crc32Ieee(0, spaced, std::strlen(spaced))));
    refused(wide, std::strlen(wide), L::RecordError::CRC, "crc", M::Err::INVALID_CONFIG);
}

void versionRefusals() {
    refusedLine(sealedVariant(ENABLE_BODY, "v", "v=2"), L::RecordError::VERSION, "v",
                M::Err::UNSUPPORTED);
    refusedLine(sealedVariant(ENABLE_BODY, "v", "v=01"), L::RecordError::VERSION, "v",
                M::Err::UNSUPPORTED);
    // v must be first; elsewhere it is a second v.
    char body[L::RECORD_TEXT_CAPACITY];
    std::snprintf(body, sizeof body, "%s v=1", ENABLE_BODY + 4);
    refusedLine(seal(body), L::RecordError::MISSING_KEY, "v", M::Err::INVALID_CONFIG);
    std::snprintf(body, sizeof body, "%s v=1", ENABLE_BODY);
    refusedLine(seal(body), L::RecordError::DUPLICATE_KEY, "v", M::Err::INVALID_CONFIG);
    L::MeasuredRecord r = enableRecord();
    r.version = 2;
    invalid(r, L::RecordError::VERSION, "v", M::Err::UNSUPPORTED);
    r.version = 0;
    invalid(r, L::RecordError::VERSION, "v", M::Err::UNSUPPORTED);
}

struct Case {
    const char* body;
    const char* key;
    const char* replacement;
    L::RecordError reason;
    const char* refusedKey;
};
void fieldRefusals() {
    using E = L::RecordError;
    const char* const N = ENABLE_BODY;
    const char* const P = POSITION_BODY;
    const char* const V = VELOCITY_BODY;
    const Case cases[] = {
        // Keys: unknown, duplicate, missing, unexpected.
        {N, "speed", "speed=5", E::UNKNOWN_KEY, "speed"},
        {N, "Mode", "Mode=1", E::UNKNOWN_KEY, "Mode"},
        {N, "x", "abcdefghijklmnopqrstuvwxyz0123456789=1", E::UNKNOWN_KEY, "abcdefghijklmnopqrstuvw"},
        {N, "crc", "crc=00000000", E::DUPLICATE_KEY, "crc"},
        {N, "mode_basis", "mode_basis=measured mode=6", E::DUPLICATE_KEY, "mode"},
        {N, "kind", "", E::MISSING_KEY, "kind"},
        {N, "evidence", "", E::MISSING_KEY, "evidence"},
        {N, "model", "", E::MISSING_KEY, "model"},
        {N, "identity", "", E::MISSING_KEY, "identity"},
        {N, "mode", "", E::MISSING_KEY, "mode"},
        {N, "starts_basis", "", E::MISSING_KEY, "starts_basis"},
        {P, "move", "", E::MISSING_KEY, "move"},
        {P, "base", "", E::MISSING_KEY, "base"},
        {P, "signals_basis", "", E::MISSING_KEY, "signals_basis"},
        {V, "halt_basis", "", E::MISSING_KEY, "halt_basis"},
        {V, "quick_stop_rpm", "", E::MISSING_KEY, "quick_stop_rpm"},
        {N, "mode", "mode=1 layout=zh86", E::UNEXPECTED_KEY, "layout"},
        {V, "starts", "starts=8050/0011 mode=1", E::UNEXPECTED_KEY, "mode"},
        {P, "move", "move=absolute", E::UNEXPECTED_KEY, "base"},
        {P, "rule", "rule=after_absence scale_2201=1000", E::UNEXPECTED_KEY, "scale_2201"},
        // Syntax inside the fields.
        {N, "mode", "mode", E::SYNTAX, "mode"},
        {N, "mode", "=1", E::SYNTAX, ""},
        {N, "mode", "mode=", E::SYNTAX, "mode"},
        // Numbers.
        {N, "mode", "mode=256", E::NUMBER, "mode"},
        {N, "mode", "mode=01", E::NUMBER, "mode"},
        {N, "mode", "mode=+1", E::NUMBER, "mode"},
        {P, "rpm", "rpm=02..300", E::NUMBER, "rpm"},
        {P, "rpm", "rpm=2-300", E::NUMBER, "rpm"},
        {P, "rpm", "rpm=2..", E::NUMBER, "rpm"},
        {P, "rpm", "rpm=..300", E::NUMBER, "rpm"},
        {P, "rpm", "rpm=2...300", E::NUMBER, "rpm"},
        {P, "rpm", "rpm=2..4294967296", E::NUMBER, "rpm"},
        {P, "distance_max", "distance_max=4294967296", E::NUMBER, "distance_max"},
        {P, "acceptance", "acceptance=statusword/1000/100", E::NUMBER, "acceptance"},
        {P, "acceptance", "acceptance=statusword/1000/10a0", E::NUMBER, "acceptance"},
        {V, "scale_2201", "scale_2201=65536", E::NUMBER, "scale_2201"},
        {V, "duration_max_us", "duration_max_us=18446744073709551616", E::NUMBER, "duration_max_us"},
        {N, "starts", "starts=8437/0001,", E::NUMBER, "starts"},
        {N, "starts", "starts=8437-0001", E::NUMBER, "starts"},
        {N, "starts", "starts=8437/01", E::NUMBER, "starts"},
        {N, "starts", "starts=c050/0015", E::NUMBER, "starts"},
        {N, "identity", "identity=00040192.000c.0020.00001000.00000001.00000000.0400.0200", E::NUMBER,
         "identity"},
        {N, "identity", "identity=40192.000C.0020.00001000.00000001.00000000.0400.0200", E::NUMBER,
         "identity"},
        // Words, lists, signals and evidence.
        {N, "kind", "kind=homing", E::VALUE, "kind"},
        {N, "model", "model=dm86", E::VALUE, "model"},
        {N, "mode_basis", "mode_basis=guessed", E::VALUE, "mode_basis"},
        {N, "mode", "mode=3", E::VALUE, "mode"},
        {N, "starts", "starts=8437/0001,8437/0001", E::VALUE, "starts"},
        {N, "starts", "starts=0001/0001,0002/0001,0003/0001,0004/0001,0005/0001,0006/0001,0007/0001,"
                      "0008/0001,0009/0001",
         E::VALUE, "starts"},
        {N, "evidence", "evidence=bad*id", E::VALUE, "evidence"},
        {N, "evidence", "evidence=abcdefghijklmnopqrstuvwxyz0123456789abcdefghijkl", E::VALUE,
         "evidence"},
        {P, "move", "move=jog", E::VALUE, "move"},
        {P, "base", "base=unmeasured", E::VALUE, "base"},
        {P, "rule", "rule=none", E::VALUE, "rule"},
        {P, "acceptance", "acceptance=6041/1000/1000", E::VALUE, "acceptance"},
        {P, "acceptance", "acceptance=statusword/1000", E::VALUE, "acceptance"},
        {P, "acceptance", "acceptance=statusword/1000/1000/1", E::VALUE, "acceptance"},
        {P, "acceptance", "acceptance=statusword/0000/0000", E::VALUE, "acceptance"},
        {P, "completion", "completion=monitor/0001/0003", E::VALUE, "completion"},
        {V, "layout", "layout=zh57", E::VALUE, "layout"},
        {V, "layout", "layout=ZH86", E::VALUE, "layout"},
        {V, "direction", "direction=left", E::VALUE, "direction"},
        // Ranges and limits.
        {P, "rpm", "rpm=300..2", E::RANGE, "rpm"},
        {P, "ramp_ms", "ramp_ms=2000..100", E::RANGE, "ramp_ms"},
        {V, "halt_rpm", "halt_rpm=60..6", E::RANGE, "halt_rpm"},
        {P, "rpm", "rpm=0..300", E::LIMIT, "rpm"},
        {P, "rpm", "rpm=2..3001", E::LIMIT, "rpm"},
        {P, "rpm", "rpm=5000..10", E::LIMIT, "rpm"},
        {P, "ramp_ms", "ramp_ms=0..2000", E::LIMIT, "ramp_ms"},
        {P, "ramp_ms", "ramp_ms=100..2001", E::LIMIT, "ramp_ms"},
        {P, "distance_max", "distance_max=0", E::LIMIT, "distance_max"},
        {P, "distance_max", "distance_max=1000001", E::LIMIT, "distance_max"},
        {V, "rpm", "rpm=6..4000", E::LIMIT, "rpm"},
        {V, "quick_stop_rpm", "quick_stop_rpm=6..4500", E::LIMIT, "quick_stop_rpm"},
        {V, "scale_2201", "scale_2201=399", E::LIMIT, "scale_2201"},
        {V, "scale_2201", "scale_2201=51201", E::LIMIT, "scale_2201"},
        {V, "duration_max_us", "duration_max_us=0", E::LIMIT, "duration_max_us"},
        {V, "duration_max_us", "duration_max_us=9223372036854775808", E::LIMIT, "duration_max_us"},
        {V, "window_max_us", "window_max_us=12000001", E::LIMIT, "window_max_us"},
        // Identity tuple.
        {N, "identity", "identity=00040192.000C.0020.00001000.00000001.00000000.0400", E::IDENTITY,
         "identity"},
        {N, "identity", "identity=00040192.000C.0020.00001000.00000001.00000000.0400.0200.0000",
         E::IDENTITY, "identity"},
        {N, "identity", "identity=00000000.000C.0020.00000000.00000000.00000000.0000.0200", E::IDENTITY,
         "identity"},
    };
    for (const Case& c : cases) {
        const M::Err code = c.reason == E::RANGE || c.reason == E::LIMIT ? M::Err::ILLEGAL_VALUE
                                                                         : M::Err::INVALID_CONFIG;
        refusedLine(sealedVariant(c.body, c.key, c.replacement), c.reason, c.refusedKey, code);
    }
    // Boundaries just inside the limits decode.
    const Case accepted[] = {
        {P, "rpm", "rpm=1..3000", E::NONE, ""},
        {P, "ramp_ms", "ramp_ms=1..1", E::NONE, ""},
        {P, "distance_max", "distance_max=1000000", E::NONE, ""},
        {V, "scale_2201", "scale_2201=400", E::NONE, ""},
        {V, "window_max_us", "window_max_us=12000000", E::NONE, ""},
        {V, "window_max_us", "window_max_us=0", E::NONE, ""},
        {V, "quick_stop_rpm", "", E::NONE, ""}, // with quick_stop_basis also removed below
        {N, "mode", "mode=6", E::NONE, ""},
        {N, "evidence", "evidence=docs/bench/20261110T0948Z-s17-velocity-node1", E::NONE, ""},
    };
    for (const Case& c : accepted) {
        Line body = variant(c.body, c.key, c.replacement);
        if (std::strcmp(c.key, "quick_stop_rpm") == 0)
            body = variant(body.text, "quick_stop_basis", "");
        const Line line = seal(body.text);
        L::MeasuredRecord decoded;
        L::RecordRefusal refusal;
        refusal.reason = E::VALUE;
        refusal.key[0] = 'k';
        if (!L::decodeRecord(decoded, refusal, line.text, line.size).isOk())
            std::fprintf(stderr, "refused %d '%s': %s\n", int(refusal.reason), refusal.key, line.text);
        CHECK(refusal.reason == E::NONE && refusal.key[0] == '\0'); // A success clears the refusal.
    }
    // An absolute move spans both 607A ends.
    Line absolute = variant(P, "move", "move=absolute");
    absolute = variant(absolute.text, "base", "");
    absolute = variant(absolute.text, "base_basis", "");
    L::MeasuredRecord decoded;
    L::RecordRefusal refusal;
    Line line = seal(variant(absolute.text, "distance_max", "distance_max=2000000").text);
    CHECK(L::decodeRecord(decoded, refusal, line.text, line.size).isOk());
    CHECK(decoded.position.maximumDistance == 2000000 && decoded.position.move == L::RecordMoveKind::ABSOLUTE);
    refusedLine(seal(variant(absolute.text, "distance_max", "distance_max=2000001").text),
                E::LIMIT, "distance_max", M::Err::ILLEGAL_VALUE);
    // Neither stop pair.
    Line noStop = variant(V, "halt_rpm", "");
    noStop = variant(noStop.text, "halt_basis", "");
    noStop = variant(noStop.text, "quick_stop_rpm", "");
    noStop = variant(noStop.text, "quick_stop_basis", "");
    refusedLine(seal(noStop.text), E::MISSING_KEY, "halt_rpm", M::Err::INVALID_CONFIG);
}

// Line-level syntax: printable ASCII, single spaces, no line ending.
void syntaxRefusals() {
    const Line good = seal(ENABLE_BODY);
    const char* cases[] = {" ", "\t", "\n", "\r", "\x7f", "\x80"};
    for (const char* c : cases) {
        Line line = good;
        line.text[16] = c[0]; // Beside the space after kind=enable: a space doubles it.
        refusedLine(line, L::RecordError::SYNTAX, "", M::Err::INVALID_CONFIG);
    }
    char text[sizeof(Line::text) + 16]; // Room for any Line plus one byte.
    std::snprintf(text, sizeof text, " %s", good.text);
    refused(text, std::strlen(text), L::RecordError::SYNTAX, "", M::Err::INVALID_CONFIG);
    std::snprintf(text, sizeof text, "%s ", good.text);
    refused(text, std::strlen(text), L::RecordError::SYNTAX, "", M::Err::INVALID_CONFIG);
    std::snprintf(text, sizeof text, "%s\n", good.text);
    refused(text, std::strlen(text), L::RecordError::SYNTAX, "", M::Err::INVALID_CONFIG);
    refused(good.text, good.size + 1, L::RecordError::SYNTAX, "", M::Err::INVALID_CONFIG); // its NUL
    refusedLine(seal(variant(ENABLE_BODY, "mode", "mode=1 ").text), L::RecordError::SYNTAX, "",
                M::Err::INVALID_CONFIG);
    refused("v1 crc=00000000", 15, L::RecordError::SYNTAX, "v1", M::Err::INVALID_CONFIG);
    refused("v=1 crc", 7, L::RecordError::SYNTAX, "crc", M::Err::INVALID_CONFIG);
    refused("kind=enable crc=00000000", 24, L::RecordError::MISSING_KEY, "v", M::Err::INVALID_CONFIG);
    // Empty, null and too long.
    refused("", 0, L::RecordError::LENGTH, "", M::Err::INVALID_CONFIG);
    refused(nullptr, 0, L::RecordError::LENGTH, "", M::Err::INVALID_CONFIG);
    refused(nullptr, 10, L::RecordError::ARGUMENT, "", M::Err::INVALID_CONFIG);
    static char longText[L::RECORD_TEXT_CAPACITY + 2];
    std::memset(longText, 'a', sizeof longText);
    refused(longText, L::RECORD_TEXT_CAPACITY + 1, L::RecordError::LENGTH, "", M::Err::INVALID_CONFIG);
}

// Every prefix of a valid line is refused, and so is each record missing one of its fields.
void partialRecords() {
    const char* const bodies[] = {ENABLE_BODY, POSITION_BODY, VELOCITY_BODY};
    for (const char* body : bodies) {
        const Line good = seal(body);
        for (size_t size = 1; size < good.size; ++size) {
            L::MeasuredRecord output = enableRecord();
            unsigned char before[sizeof output];
            std::memcpy(before, &output, sizeof output);
            L::RecordRefusal refusal;
            CHECK(!L::decodeRecord(output, refusal, good.text, size).isOk());
            CHECK(refusal.reason != L::RecordError::NONE);
            CHECK(std::memcmp(before, &output, sizeof output) == 0);
        }
        // Each field removed, with the CRC made anew: a key of the kind is missing.
        for (const char* field = std::strchr(body, ' ') + 1; field;) {
            const char* end = std::strchr(field, ' ');
            char key[32] = {};
            const char* equal = std::strchr(field, '=');
            std::memcpy(key, field, size_t(equal - field));
            const bool optional = std::strncmp(key, "halt", 4) == 0 || std::strncmp(key, "quick", 5) == 0;
            const Line line = sealedVariant(body, key, "");
            L::MeasuredRecord output;
            L::RecordRefusal refusal;
            const M::Status s = L::decodeRecord(output, refusal, line.text, line.size);
            if (optional) { // Half a pair is missing its partner.
                CHECK(refusal.reason == L::RecordError::MISSING_KEY && !s.isOk());
            } else if (std::strcmp(key, "move") == 0) {
                CHECK(refusal.reason == L::RecordError::MISSING_KEY && std::strcmp(refusal.key, "move") == 0);
            } else {
                CHECK(refusal.reason == L::RecordError::MISSING_KEY && std::strcmp(refusal.key, key) == 0);
            }
            field = end ? end + 1 : nullptr;
        }
    }
}

void structRefusals() {
    using E = L::RecordError;
    const M::Err C = M::Err::INVALID_CONFIG, I = M::Err::ILLEGAL_VALUE;
    L::MeasuredRecord r = enableRecord();
    r.kind = L::RecordKind::NONE;
    invalid(r, E::VALUE, "kind", C);
    r = enableRecord();
    r.evidence[0] = '\0';
    invalid(r, E::VALUE, "evidence", C);
    std::memset(r.evidence, 'a', sizeof r.evidence); // No NUL within the capacity.
    invalid(r, E::VALUE, "evidence", C);
    r = enableRecord();
    r.model = L::SettingsModel::UNKNOWN;
    invalid(r, E::VALUE, "model", C);
    r = enableRecord();
    r.identity = L::IdentityValues();
    invalid(r, E::IDENTITY, "identity", C);
    r.identity.modelCode = 0x0400; // One product field suffices.
    L::RecordRefusal refusal;
    CHECK(L::validateRecord(refusal, r).isOk());
    r = enableRecord();
    r.enable.starts.count = 0;
    invalid(r, E::VALUE, "starts", C);
    r.enable.starts.count = L::RECORD_START_REPORTS + 1;
    invalid(r, E::VALUE, "starts", C);
    r = enableRecord();
    r.enable.starts.basis = L::RecordBasis(9);
    invalid(r, E::VALUE, "starts_basis", C);
    r = positionRecord();
    r.position.relativeBase = L::RelativeBase::UNMEASURED;
    invalid(r, E::VALUE, "base", C);
    r = positionRecord();
    r.position.baseBasis = L::RecordBasis::NONE;
    invalid(r, E::VALUE, "base_basis", C);
    r = positionRecord();
    r.position.acceptance.source = L::MoveSignalSource::NONE;
    invalid(r, E::VALUE, "acceptance", C);
    r = positionRecord();
    r.position.rule = L::CompletionRule::NONE;
    invalid(r, E::VALUE, "rule", C);
    r = positionRecord();
    r.position.rpm.basis = L::RecordBasis::NONE;
    invalid(r, E::VALUE, "rpm_basis", C);
    r = positionRecord();
    r.position.move = L::RecordMoveKind::NONE;
    invalid(r, E::VALUE, "move", C);
    r = positionRecord();
    r.position.rpm = range(3000, 3001, L::RecordBasis::MEASURED);
    invalid(r, E::LIMIT, "rpm", I);
    r = velocityRecord();
    r.velocity.layout = L::StatusLayout::UNSPECIFIED;
    invalid(r, E::VALUE, "layout", C);
    r = velocityRecord();
    r.velocity.direction = L::RecordDirection::NONE;
    invalid(r, E::VALUE, "direction", C);
    r = velocityRecord();
    r.velocity.halt.admitted = r.velocity.quickStop.admitted = false;
    invalid(r, E::MISSING_KEY, "halt_rpm", C);
    r = velocityRecord();
    r.velocity.quickStop.rpm.basis = L::RecordBasis::NONE;
    invalid(r, E::VALUE, "quick_stop_basis", C);
    r = velocityRecord();
    r.velocity.maximumDurationUs = uint64_t(INT64_MAX) + 1;
    invalid(r, E::LIMIT, "duration_max_us", I);
    r = velocityRecord();
    r.velocity.maximumWindowUs = r.velocity.maximumDurationUs + 1;
    invalid(r, E::LIMIT, "window_max_us", I);
    r = velocityRecord();
    r.velocity.rampMs = range(201, 200, L::RecordBasis::MEASURED);
    invalid(r, E::RANGE, "ramp_ms", I);
}

// The longest valid record fits RECORD_TEXT_CAPACITY, and only a buffer one byte longer holds it.
void capacity() {
    L::MeasuredRecord r = velocityRecord();
    std::memset(r.evidence, 'e', L::RECORD_EVIDENCE_CAPACITY - 1);
    r.evidence[L::RECORD_EVIDENCE_CAPACITY - 1] = '\0';
    r.identity.deviceType = r.identity.vendorId = r.identity.productCode = r.identity.revision = 0xFFFFFFFFu;
    r.velocity.layoutBasis = r.velocity.rpm.basis = r.velocity.directionBasis = r.velocity.rampMs.basis =
        r.velocity.durationBasis = r.velocity.scaleBasis = r.velocity.halt.rpm.basis =
            r.velocity.quickStop.rpm.basis = r.velocity.windowBasis = r.velocity.starts.basis =
                L::RecordBasis::DOCUMENTED;
    r.velocity.rpm = r.velocity.halt.rpm = r.velocity.quickStop.rpm =
        range(L::RECORD_MAXIMUM_RPM, L::RECORD_MAXIMUM_RPM, L::RecordBasis::DOCUMENTED);
    r.velocity.rampMs = range(L::RECORD_MAXIMUM_RAMP_MS, L::RECORD_MAXIMUM_RAMP_MS, L::RecordBasis::DOCUMENTED);
    r.velocity.direction = L::RecordDirection::FORWARD;
    r.velocity.maximumDurationUs = r.velocity.maximumWindowUs = uint64_t(INT64_MAX);
    r.velocity.scale2201 = L::SUBDIVISION_MAXIMUM;
    r.velocity.starts.count = L::RECORD_START_REPORTS;
    for (unsigned i = 0; i < L::RECORD_START_REPORTS; ++i) {
        r.velocity.starts.reports[i].statusword = uint16_t(0xFF00 + i);
        r.velocity.starts.reports[i].monitor = 0xFFFF;
    }
    char text[L::RECORD_TEXT_CAPACITY + 1];
    size_t length = 0;
    L::RecordRefusal refusal;
    CHECK(L::encodeRecord(text, sizeof text, length, refusal, r).isOk());
    CHECK(length <= L::RECORD_TEXT_CAPACITY);
    L::MeasuredRecord decoded;
    CHECK(L::decodeRecord(decoded, refusal, text, length).isOk() && sameRecord(decoded, r));
    // Buffer too small by one: nothing written, length kept.
    char small[L::RECORD_TEXT_CAPACITY + 1];
    std::memset(small, 'x', sizeof small);
    size_t kept = 5;
    M::Status s = L::encodeRecord(small, length, kept, refusal, r);
    CHECK(s.code == M::Err::INVALID_CONFIG && s.detail == int32_t(L::RecordError::CAPACITY));
    CHECK(refusal.reason == L::RecordError::CAPACITY && refusal.key[0] == '\0' && kept == 5);
    for (size_t i = 0; i < sizeof small; ++i)
        CHECK(small[i] == 'x');
    CHECK(L::encodeRecord(small, length + 1, kept, refusal, r).isOk() && kept == length);
    CHECK(std::strcmp(small, text) == 0);
    CHECK(L::encodeRecord(nullptr, 0, kept, refusal, r).detail == int32_t(L::RecordError::CAPACITY));
    CHECK(L::encodeRecord(nullptr, 8, kept, refusal, r).detail == int32_t(L::RecordError::ARGUMENT));
    CHECK(kept == length);
    // An invalid record reports its field, not the buffer.
    r.velocity.scale2201 = 1;
    CHECK(L::encodeRecord(small, 1, kept, refusal, r).detail == int32_t(L::RecordError::LIMIT));
}

void overlaps() {
    L::MeasuredRecord r = enableRecord();
    char text[L::RECORD_TEXT_CAPACITY + 1];
    size_t length = 0;
    L::RecordRefusal refusal;
    refusal.reason = L::RecordError::RANGE;
    CHECK(L::encodeRecord(text, sizeof text, length, refusal, r).isOk());
    // Text inside the output or the refusal: refused before anything is written, the refusal included.
    L::MeasuredRecord output = positionRecord();
    unsigned char before[sizeof output];
    std::memcpy(before, &output, sizeof output);
    L::RecordRefusal untouched;
    untouched.reason = L::RecordError::RANGE;
    M::Status s = L::decodeRecord(output, untouched, output.evidence, 10);
    CHECK(s.code == M::Err::INVALID_CONFIG && s.detail == int32_t(L::RecordError::OVERLAP));
    CHECK(untouched.reason == L::RecordError::RANGE && std::memcmp(before, &output, sizeof output) == 0);
    s = L::decodeRecord(output, untouched, untouched.key, 5);
    CHECK(s.detail == int32_t(L::RecordError::OVERLAP) && untouched.reason == L::RecordError::RANGE);
    s = L::encodeRecord(r.evidence, sizeof r.evidence, length, untouched, r);
    CHECK(s.detail == int32_t(L::RecordError::OVERLAP) && untouched.reason == L::RecordError::RANGE);
    s = L::encodeRecord(untouched.key, sizeof untouched.key, length, untouched, r);
    CHECK(s.detail == int32_t(L::RecordError::OVERLAP) && untouched.reason == L::RecordError::RANGE);
    // An output overlapping the refusal, or the refusal the record: two objects in one buffer, used
    // by address only.
    alignas(L::MeasuredRecord) unsigned char storage[sizeof(L::MeasuredRecord) + 8];
    L::MeasuredRecord* shared = new (storage) L::MeasuredRecord(enableRecord());
    L::RecordRefusal* inside = reinterpret_cast<L::RecordRefusal*>(storage + 8);
    s = L::decodeRecord(*shared, *inside, text, length);
    CHECK(s.detail == int32_t(L::RecordError::OVERLAP));
    s = L::validateRecord(*inside, *shared);
    CHECK(s.detail == int32_t(L::RecordError::OVERLAP));
    s = L::encodeRecord(text, sizeof text, length, *inside, *shared);
    CHECK(s.detail == int32_t(L::RecordError::OVERLAP));
}

// No call of the codec allocates, on success or refusal.
void noHeap() {
    const L::MeasuredRecord records[] = {enableRecord(), positionRecord(), velocityRecord()};
    const Line bad = sealedVariant(POSITION_BODY, "rpm", "rpm=300..2");
    char text[L::RECORD_TEXT_CAPACITY + 1];
    watching = true;
    allocations = 0;
    for (const L::MeasuredRecord& r : records) {
        size_t length = 0;
        L::RecordRefusal refusal;
        L::MeasuredRecord decoded;
        CHECK(L::validateRecord(refusal, r).isOk());
        CHECK(L::encodeRecord(text, sizeof text, length, refusal, r).isOk());
        CHECK(L::decodeRecord(decoded, refusal, text, length).isOk());
        CHECK(!L::encodeRecord(text, 10, length, refusal, r).isOk());
        CHECK(!L::decodeRecord(decoded, refusal, bad.text, bad.size).isOk());
        CHECK(L::crc32Ieee(0, "123456789", 9) == 0xCBF43926u);
    }
    watching = false;
    CHECK(allocations == 0);
}
} // namespace

int main() {
    crcVectors();
    goldenText();
    roundTrips();
    ignoredFields();
    crcRefusals();
    versionRefusals();
    fieldRefusals();
    syntaxRefusals();
    partialRecords();
    structRefusals();
    capacity();
    overlaps();
    noHeap();
    std::puts("records_test passed");
    return 0;
}
