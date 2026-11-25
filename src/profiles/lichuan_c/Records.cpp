// SPDX-License-Identifier: MIT
#include "../../canopen/SdoEvidence.h"
#include <MotorControlCAN/profiles/lichuan_c/Records.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
using CANOPEN::overlaps;

// The key table, in encoding order. The kind bits say which record kinds use a key; BASE and the
// stop pairs have the further conditions of uses() and allowed().
enum : uint8_t { USE_ENABLE = 1, USE_POSITION = 2, USE_VELOCITY = 4, USE_ALL = 7 };
enum Field : uint8_t {
    KIND, EVIDENCE, MODEL, IDENTITY, MODE, MODE_BASIS, MOVE, BASE, BASE_BASIS, LAYOUT, LAYOUT_BASIS,
    RPM, RPM_BASIS, DIRECTION, DIRECTION_BASIS, RAMP, RAMP_BASIS, DISTANCE, DISTANCE_BASIS, DURATION,
    DURATION_BASIS, SCALE, SCALE_BASIS, ACCEPTANCE, COMPLETION, RULE, SIGNALS_BASIS, HALT,
    HALT_BASIS, QUICK, QUICK_BASIS, WINDOW, WINDOW_BASIS, STARTS, STARTS_BASIS, FIELDS
};
struct Key {
    const char* name;
    uint8_t kinds;
};
const Key KEYS[FIELDS] = {
    {"kind", USE_ALL},
    {"evidence", USE_ALL},
    {"model", USE_ALL},
    {"identity", USE_ALL},
    {"mode", USE_ENABLE},
    {"mode_basis", USE_ENABLE},
    {"move", USE_POSITION},
    {"base", USE_POSITION},
    {"base_basis", USE_POSITION},
    {"layout", USE_VELOCITY},
    {"layout_basis", USE_VELOCITY},
    {"rpm", USE_POSITION | USE_VELOCITY},
    {"rpm_basis", USE_POSITION | USE_VELOCITY},
    {"direction", USE_VELOCITY},
    {"direction_basis", USE_VELOCITY},
    {"ramp_ms", USE_POSITION | USE_VELOCITY},
    {"ramp_basis", USE_POSITION | USE_VELOCITY},
    {"distance_max", USE_POSITION},
    {"distance_basis", USE_POSITION},
    {"duration_max_us", USE_VELOCITY},
    {"duration_basis", USE_VELOCITY},
    {"scale_2201", USE_VELOCITY},
    {"scale_basis", USE_VELOCITY},
    {"acceptance", USE_POSITION},
    {"completion", USE_POSITION},
    {"rule", USE_POSITION},
    {"signals_basis", USE_POSITION},
    {"halt_rpm", USE_VELOCITY},
    {"halt_basis", USE_VELOCITY},
    {"quick_stop_rpm", USE_VELOCITY},
    {"quick_stop_basis", USE_VELOCITY},
    {"window_max_us", USE_VELOCITY},
    {"window_basis", USE_VELOCITY},
    {"starts", USE_ENABLE | USE_VELOCITY},
    {"starts_basis", USE_ENABLE | USE_VELOCITY},
};

// Enumerators and their words. A value without a word (NONE, UNSPECIFIED, ZH57) has no text form.
struct Word {
    const char* text;
    uint8_t value;
};
const Word KINDS[] = {{"enable", uint8_t(RecordKind::ENABLE)},
                      {"position", uint8_t(RecordKind::POSITION)},
                      {"velocity", uint8_t(RecordKind::VELOCITY)}};
const Word BASES[] = {{"measured", uint8_t(RecordBasis::MEASURED)},
                      {"documented", uint8_t(RecordBasis::DOCUMENTED)},
                      {"inferred", uint8_t(RecordBasis::INFERRED)}};
const Word MODELS[] = {{"cl86_c", uint8_t(SettingsModel::CL86_C)},
                       {"ol86_c", uint8_t(SettingsModel::OL86_C)}};
const Word MOVES[] = {{"absolute", uint8_t(RecordMoveKind::ABSOLUTE)},
                      {"relative", uint8_t(RecordMoveKind::RELATIVE)}};
const Word RELATIVE_BASES[] = {{"actual", uint8_t(RelativeBase::ACTUAL)},
                               {"commanded", uint8_t(RelativeBase::COMMANDED)}};
const Word LAYOUTS[] = {{"en86", uint8_t(StatusLayout::EN86)},
                        {"zh86", uint8_t(StatusLayout::ZH86)},
                        {"op86", uint8_t(StatusLayout::OP86)}};
const Word DIRECTIONS[] = {{"forward", uint8_t(RecordDirection::FORWARD)},
                           {"reverse", uint8_t(RecordDirection::REVERSE)},
                           {"both", uint8_t(RecordDirection::BOTH)}};
const Word RULES[] = {{"after_absence", uint8_t(CompletionRule::AFTER_ABSENCE)},
                      {"after_acceptance", uint8_t(CompletionRule::AFTER_ACCEPTANCE)}};
const Word SOURCES[] = {{"statusword", uint8_t(MoveSignalSource::STATUSWORD)},
                        {"monitor", uint8_t(MoveSignalSource::MONITOR)}};

template <size_t N> const char* wordText(const Word (&words)[N], uint8_t value) {
    for (size_t i = 0; i < N; ++i)
        if (words[i].value == value)
            return words[i].text;
    return nullptr;
}
bool same(const char* text, size_t size, const char* word) {
    size_t i = 0;
    for (; i < size && word[i]; ++i)
        if (text[i] != word[i])
            return false;
    return i == size && !word[i];
}
template <size_t N> bool parseWord(const Word (&words)[N], const char* text, size_t size, uint8_t& value) {
    for (size_t i = 0; i < N; ++i)
        if (same(text, size, words[i].text)) {
            value = words[i].value;
            return true;
        }
    return false;
}

// The first refusal found: a key that is either one of the table's static names or a span of the
// borrowed text (an unknown key), and a static message.
struct Fault {
    RecordError reason = RecordError::NONE;
    const char* key = "";
    size_t keySize = 0;
    const char* msg = "";
};
size_t length(const char* text) {
    size_t n = 0;
    while (text[n])
        ++n;
    return n;
}
bool fail(Fault& f, RecordError reason, const char* key, const char* msg) {
    f.reason = reason;
    f.key = key;
    f.keySize = length(key);
    f.msg = msg;
    return false;
}
bool failSpan(Fault& f, RecordError reason, const char* key, size_t keySize, const char* msg) {
    fail(f, reason, "", msg);
    f.key = key;
    f.keySize = keySize;
    return false;
}
Err category(RecordError reason) {
    switch (reason) {
    case RecordError::NONE:
        return Err::OK;
    case RecordError::VERSION:
        return Err::UNSUPPORTED;
    case RecordError::RANGE:
    case RecordError::LIMIT:
        return Err::ILLEGAL_VALUE;
    default:
        return Err::INVALID_CONFIG;
    }
}
Status report(RecordRefusal& refusal, const Fault& f) {
    refusal.reason = f.reason;
    size_t n = 0;
    for (; n < f.keySize && n + 1 < RECORD_KEY_CAPACITY; ++n)
        refusal.key[n] = f.key[n];
    for (size_t i = n; i < RECORD_KEY_CAPACITY; ++i)
        refusal.key[i] = '\0';
    return Status(category(f.reason), int32_t(f.reason), f.reason == RecordError::NONE ? "" : f.msg);
}
Status overlapRefusal() {
    return Status(Err::INVALID_CONFIG, int32_t(RecordError::OVERLAP),
                  "record codec arguments overlap; nothing written");
}

// Semantic rules, shared by validateRecord, encodeRecord and decodeRecord, in key table order.
bool basisValid(RecordBasis basis) {
    return basis == RecordBasis::MEASURED || basis == RecordBasis::DOCUMENTED ||
           basis == RecordBasis::INFERRED;
}
bool checkBasis(Fault& f, RecordBasis basis, const char* key) {
    return basisValid(basis) || fail(f, RecordError::VALUE, key, "basis is not measured, documented or inferred");
}
bool checkRange(Fault& f, const RecordRange& range, uint32_t maximum, const char* key,
                const char* basisKey) {
    if (range.minimum < 1 || range.minimum > maximum || range.maximum < 1 || range.maximum > maximum)
        return fail(f, RecordError::LIMIT, key, "range outside its documented or library limits");
    if (range.minimum > range.maximum)
        return fail(f, RecordError::RANGE, key, "range minimum above its maximum");
    return checkBasis(f, range.basis, basisKey);
}
bool checkStarts(Fault& f, const RecordStarts& starts) {
    if (!starts.count || starts.count > RECORD_START_REPORTS)
        return fail(f, RecordError::VALUE, "starts", "start list empty or longer than its capacity");
    for (unsigned i = 0; i < starts.count; ++i)
        for (unsigned j = 0; j < i; ++j)
            if (starts.reports[i].statusword == starts.reports[j].statusword &&
                starts.reports[i].monitor == starts.reports[j].monitor)
                return fail(f, RecordError::VALUE, "starts", "start list repeats a report");
    return checkBasis(f, starts.basis, "starts_basis");
}
bool checkSignal(Fault& f, const MoveSignal& signal, const char* key) {
    if ((signal.source != MoveSignalSource::STATUSWORD && signal.source != MoveSignalSource::MONITOR) ||
        !signal.mask || (signal.value & ~signal.mask))
        return fail(f, RecordError::VALUE, key, "signal needs a source, a nonzero mask and a value within it");
    return true;
}
bool evidenceCharacter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
           c == '_' || c == '-' || c == ':' || c == '/';
}
bool evidenceValid(const char* evidence) {
    size_t n = 0;
    while (n < RECORD_EVIDENCE_CAPACITY && evidence[n]) {
        if (!evidenceCharacter(evidence[n]))
            return false;
        ++n;
    }
    return n && n < RECORD_EVIDENCE_CAPACITY;
}
bool checkStop(Fault& f, const RecordStop& stop, const char* key, const char* basisKey) {
    return !stop.admitted || checkRange(f, stop.rpm, RECORD_MAXIMUM_RPM, key, basisKey);
}
bool checkEnable(Fault& f, const EnableScope& e) {
    if (e.modeRaw != 1 && e.modeRaw != 6)
        return fail(f, RecordError::VALUE, "mode", "enable mode is neither 1 nor 6");
    return checkBasis(f, e.modeBasis, "mode_basis") && checkStarts(f, e.starts);
}
bool checkPosition(Fault& f, const PositionScope& p) {
    const bool relative = p.move == RecordMoveKind::RELATIVE;
    if (!relative && p.move != RecordMoveKind::ABSOLUTE)
        return fail(f, RecordError::VALUE, "move", "move is neither absolute nor relative");
    if (relative && p.relativeBase != RelativeBase::ACTUAL && p.relativeBase != RelativeBase::COMMANDED)
        return fail(f, RecordError::VALUE, "base", "a relative record needs its measured base");
    if ((relative && !checkBasis(f, p.baseBasis, "base_basis")) ||
        !checkRange(f, p.rpm, RECORD_MAXIMUM_RPM, "rpm", "rpm_basis") ||
        !checkRange(f, p.rampMs, RECORD_MAXIMUM_RAMP_MS, "ramp_ms", "ramp_basis"))
        return false;
    const uint32_t distance =
        relative ? RECORD_MAXIMUM_RELATIVE_DISTANCE : RECORD_MAXIMUM_ABSOLUTE_DISTANCE;
    if (!p.maximumDistance || p.maximumDistance > distance)
        return fail(f, RecordError::LIMIT, "distance_max", "distance outside the 607A span");
    if (!checkBasis(f, p.distanceBasis, "distance_basis") ||
        !checkSignal(f, p.acceptance, "acceptance") || !checkSignal(f, p.completion, "completion"))
        return false;
    if (p.rule != CompletionRule::AFTER_ABSENCE && p.rule != CompletionRule::AFTER_ACCEPTANCE)
        return fail(f, RecordError::VALUE, "rule", "completion rule missing");
    return checkBasis(f, p.signalsBasis, "signals_basis");
}
bool checkVelocity(Fault& f, const VelocityScope& v) {
    // ZH57 has no word: the velocity preparation refuses it, so a record cannot name it.
    if (!wordText(LAYOUTS, uint8_t(v.layout)))
        return fail(f, RecordError::VALUE, "layout", "layout is not en86, zh86 or op86");
    if (!checkBasis(f, v.layoutBasis, "layout_basis") ||
        !checkRange(f, v.rpm, RECORD_MAXIMUM_RPM, "rpm", "rpm_basis"))
        return false;
    if (!wordText(DIRECTIONS, uint8_t(v.direction)))
        return fail(f, RecordError::VALUE, "direction", "direction missing");
    if (!checkBasis(f, v.directionBasis, "direction_basis") ||
        !checkRange(f, v.rampMs, RECORD_MAXIMUM_RAMP_MS, "ramp_ms", "ramp_basis"))
        return false;
    if (!v.maximumDurationUs || v.maximumDurationUs > uint64_t(INT64_MAX))
        return fail(f, RecordError::LIMIT, "duration_max_us", "duration outside 1..INT64_MAX us");
    if (!checkBasis(f, v.durationBasis, "duration_basis"))
        return false;
    if (v.scale2201 < SUBDIVISION_MINIMUM || v.scale2201 > SUBDIVISION_MAXIMUM)
        return fail(f, RecordError::LIMIT, "scale_2201", "2201 outside its documented range");
    if (!checkBasis(f, v.scaleBasis, "scale_basis") ||
        !checkStop(f, v.halt, "halt_rpm", "halt_basis") ||
        !checkStop(f, v.quickStop, "quick_stop_rpm", "quick_stop_basis"))
        return false;
    if (!v.halt.admitted && !v.quickStop.admitted)
        return fail(f, RecordError::MISSING_KEY, "halt_rpm", "a velocity record admits at least one stop kind");
    // The window adds to the setup the run's duration must cover, so it cannot exceed that duration.
    if (v.maximumWindowUs > v.maximumDurationUs)
        return fail(f, RecordError::LIMIT, "window_max_us", "enable window longer than the duration");
    return checkBasis(f, v.windowBasis, "window_basis") && checkStarts(f, v.starts);
}
bool check(Fault& f, const MeasuredRecord& r) {
    if (r.version != RECORD_VERSION)
        return fail(f, RecordError::VERSION, "v", "record version is not 1");
    if (!wordText(KINDS, uint8_t(r.kind)))
        return fail(f, RecordError::VALUE, "kind", "kind is not enable, position or velocity");
    if (!evidenceValid(r.evidence))
        return fail(f, RecordError::VALUE, "evidence", "evidence reference empty, too long or with other characters");
    if (!wordText(MODELS, uint8_t(r.model)))
        return fail(f, RecordError::VALUE, "model", "model is not cl86_c or ol86_c");
    const IdentityValues& i = r.identity;
    if (!i.deviceType && !i.vendorId && !i.productCode && !i.modelCode)
        return fail(f, RecordError::IDENTITY, "identity", "identity has no product field");
    switch (r.kind) {
    case RecordKind::ENABLE:
        return checkEnable(f, r.enable);
    case RecordKind::POSITION:
        return checkPosition(f, r.position);
    default:
        return checkVelocity(f, r.velocity);
    }
}

// Which fields a valid record encodes.
uint8_t kindBit(RecordKind kind) {
    return kind == RecordKind::ENABLE ? USE_ENABLE
                                      : kind == RecordKind::POSITION ? USE_POSITION : USE_VELOCITY;
}
bool uses(Field field, const MeasuredRecord& r) {
    if (!(KEYS[field].kinds & kindBit(r.kind)))
        return false;
    switch (field) {
    case BASE:
    case BASE_BASIS:
        return r.position.move == RecordMoveKind::RELATIVE;
    case HALT:
    case HALT_BASIS:
        return r.velocity.halt.admitted;
    case QUICK:
    case QUICK_BASIS:
        return r.velocity.quickStop.admitted;
    default:
        return true;
    }
}

// Encoding. With a null buffer the writer only counts, so the length is known before any write.
struct Writer {
    char* out;
    size_t length;
    void put(char c) {
        if (out)
            out[length] = c;
        ++length;
    }
    void text(const char* s) {
        while (*s)
            put(*s++);
    }
    void decimal(uint64_t value) {
        char digits[20];
        unsigned n = 0;
        do {
            digits[n++] = char('0' + value % 10);
            value /= 10;
        } while (value);
        while (n)
            put(digits[--n]);
    }
    void hex(uint32_t value, unsigned digits, bool upper) {
        for (unsigned i = digits; i-- > 0;) {
            const unsigned d = (value >> (4 * i)) & 15u;
            put(char(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10));
        }
    }
    void range(const RecordRange& r) {
        decimal(r.minimum);
        text("..");
        decimal(r.maximum);
    }
    void signal(const MoveSignal& s) {
        text(wordText(SOURCES, uint8_t(s.source)));
        put('/');
        hex(s.mask, 4, true);
        put('/');
        hex(s.value, 4, true);
    }
    void starts(const RecordStarts& s) {
        for (unsigned i = 0; i < s.count; ++i) {
            if (i)
                put(',');
            hex(s.reports[i].statusword, 4, true);
            put('/');
            hex(s.reports[i].monitor, 4, true);
        }
    }
    void basis(RecordBasis b) { text(wordText(BASES, uint8_t(b))); }
};
const RecordRange& rangeOf(const MeasuredRecord& r, Field field) {
    switch (field) {
    case HALT:
        return r.velocity.halt.rpm;
    case QUICK:
        return r.velocity.quickStop.rpm;
    case RPM:
        return r.kind == RecordKind::POSITION ? r.position.rpm : r.velocity.rpm;
    default:
        return r.kind == RecordKind::POSITION ? r.position.rampMs : r.velocity.rampMs;
    }
}
const RecordStarts& startsOf(const MeasuredRecord& r) {
    return r.kind == RecordKind::ENABLE ? r.enable.starts : r.velocity.starts;
}
void writeValue(Writer& w, Field field, const MeasuredRecord& r) {
    const PositionScope& p = r.position;
    const VelocityScope& v = r.velocity;
    const IdentityValues& i = r.identity;
    switch (field) {
    case KIND:
        return w.text(wordText(KINDS, uint8_t(r.kind)));
    case EVIDENCE:
        return w.text(r.evidence);
    case MODEL:
        return w.text(wordText(MODELS, uint8_t(r.model)));
    case IDENTITY: {
        // 1000.1009.100A.1018:01.1018:02.1018:03.2000.2001, each at its field's width.
        const uint32_t values[8] = {i.deviceType, i.hardwareVersion, i.softwareVersion, i.vendorId,
                                    i.productCode, i.revision, i.modelCode, i.firmwareCode};
        const unsigned widths[8] = {8, 4, 4, 8, 8, 8, 4, 4};
        for (unsigned n = 0; n < 8; ++n) {
            if (n)
                w.put('.');
            w.hex(values[n], widths[n], true);
        }
        return;
    }
    case MODE:
        return w.decimal(r.enable.modeRaw);
    case MODE_BASIS:
        return w.basis(r.enable.modeBasis);
    case MOVE:
        return w.text(wordText(MOVES, uint8_t(p.move)));
    case BASE:
        return w.text(wordText(RELATIVE_BASES, uint8_t(p.relativeBase)));
    case BASE_BASIS:
        return w.basis(p.baseBasis);
    case LAYOUT:
        return w.text(wordText(LAYOUTS, uint8_t(v.layout)));
    case LAYOUT_BASIS:
        return w.basis(v.layoutBasis);
    case RPM:
    case RAMP:
    case HALT:
    case QUICK:
        return w.range(rangeOf(r, field));
    case RPM_BASIS:
    case RAMP_BASIS:
    case HALT_BASIS:
    case QUICK_BASIS:
        return w.basis(rangeOf(r, Field(field - 1)).basis);
    case DIRECTION:
        return w.text(wordText(DIRECTIONS, uint8_t(v.direction)));
    case DIRECTION_BASIS:
        return w.basis(v.directionBasis);
    case DISTANCE:
        return w.decimal(p.maximumDistance);
    case DISTANCE_BASIS:
        return w.basis(p.distanceBasis);
    case DURATION:
        return w.decimal(v.maximumDurationUs);
    case DURATION_BASIS:
        return w.basis(v.durationBasis);
    case SCALE:
        return w.decimal(v.scale2201);
    case SCALE_BASIS:
        return w.basis(v.scaleBasis);
    case ACCEPTANCE:
        return w.signal(p.acceptance);
    case COMPLETION:
        return w.signal(p.completion);
    case RULE:
        return w.text(wordText(RULES, uint8_t(p.rule)));
    case SIGNALS_BASIS:
        return w.basis(p.signalsBasis);
    case WINDOW:
        return w.decimal(v.maximumWindowUs);
    case WINDOW_BASIS:
        return w.basis(v.windowBasis);
    case STARTS:
        return w.starts(startsOf(r));
    case STARTS_BASIS:
        return w.basis(startsOf(r).basis);
    default:
        return;
    }
}
void writeBody(Writer& w, const MeasuredRecord& r) {
    w.text("v=1");
    for (unsigned field = 0; field < FIELDS; ++field) {
        if (!uses(Field(field), r))
            continue;
        w.put(' ');
        w.text(KEYS[field].name);
        w.put('=');
        writeValue(w, Field(field), r);
    }
}
// " crc=" and eight digits.
constexpr size_t CRC_FIELD = 13;

// Decoding. Numbers are canonical: decimal without sign or leading zero, hexadecimal at its exact
// width in the stated case.
bool parseDecimal(const char* s, size_t n, uint64_t maximum, uint64_t& value) {
    if (!n || n > 20 || (n > 1 && s[0] == '0'))
        return false;
    uint64_t v = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] < '0' || s[i] > '9')
            return false;
        const unsigned digit = unsigned(s[i] - '0');
        if (v > (maximum - digit) / 10)
            return false;
        v = v * 10 + digit;
    }
    value = v;
    return true;
}
bool parseHex(const char* s, size_t n, unsigned digits, bool upper, uint32_t& value) {
    if (n != digits)
        return false;
    uint32_t v = 0;
    for (size_t i = 0; i < n; ++i) {
        const char c = s[i];
        unsigned d;
        if (c >= '0' && c <= '9')
            d = unsigned(c - '0');
        else if (upper && c >= 'A' && c <= 'F')
            d = unsigned(c - 'A' + 10);
        else if (!upper && c >= 'a' && c <= 'f')
            d = unsigned(c - 'a' + 10);
        else
            return false;
        v = v << 4 | d;
    }
    value = v;
    return true;
}
bool parseRange(const char* s, size_t n, RecordRange& range) {
    size_t dot = 0;
    while (dot < n && s[dot] != '.')
        ++dot;
    uint64_t minimum, maximum;
    if (dot + 1 >= n || s[dot + 1] != '.' || !parseDecimal(s, dot, UINT32_MAX, minimum) ||
        !parseDecimal(s + dot + 2, n - dot - 2, UINT32_MAX, maximum))
        return false;
    range.minimum = uint32_t(minimum);
    range.maximum = uint32_t(maximum);
    return true;
}
// Index of the next separator at or after from, or n.
size_t next(const char* s, size_t n, size_t from, char separator) {
    while (from < n && s[from] != separator)
        ++from;
    return from;
}
bool parseIdentity(Fault& f, const char* s, size_t n, IdentityValues& identity) {
    unsigned separators = 0;
    for (size_t i = 0; i < n; ++i)
        separators += s[i] == '.';
    if (separators != 7)
        return fail(f, RecordError::IDENTITY, "identity", "identity is not eight fields");
    uint32_t values[8];
    const unsigned widths[8] = {8, 4, 4, 8, 8, 8, 4, 4};
    size_t start = 0;
    for (unsigned field = 0; field < 8; ++field) {
        const size_t end = next(s, n, start, '.');
        if (!parseHex(s + start, end - start, widths[field], true, values[field]))
            return fail(f, RecordError::NUMBER, "identity", "identity field is not uppercase hex at its width");
        start = end + 1;
    }
    identity.deviceType = values[0];
    identity.hardwareVersion = uint16_t(values[1]);
    identity.softwareVersion = uint16_t(values[2]);
    identity.vendorId = values[3];
    identity.productCode = values[4];
    identity.revision = values[5];
    identity.modelCode = uint16_t(values[6]);
    identity.firmwareCode = uint16_t(values[7]);
    return true;
}
bool parseSignal(Fault& f, const char* s, size_t n, const char* key, MoveSignal& signal) {
    const size_t first = next(s, n, 0, '/');
    const size_t second = next(s, n, first + 1, '/');
    if (first >= n || second >= n || next(s, n, second + 1, '/') != n)
        return fail(f, RecordError::VALUE, key, "signal is not source/mask/value");
    uint8_t source;
    uint32_t mask, value;
    if (!parseWord(SOURCES, s, first, source))
        return fail(f, RecordError::VALUE, key, "signal source is not statusword or monitor");
    if (!parseHex(s + first + 1, second - first - 1, 4, true, mask) ||
        !parseHex(s + second + 1, n - second - 1, 4, true, value))
        return fail(f, RecordError::NUMBER, key, "signal mask or value is not four uppercase hex digits");
    signal.source = MoveSignalSource(source);
    signal.mask = uint16_t(mask);
    signal.value = uint16_t(value);
    return true;
}
bool parseStarts(Fault& f, const char* s, size_t n, RecordStarts& starts) {
    uint8_t count = 0;
    for (size_t start = 0; start <= n; ++count) {
        const size_t end = next(s, n, start, ',');
        uint32_t statusword, monitor;
        if (count == RECORD_START_REPORTS)
            return fail(f, RecordError::VALUE, "starts", "start list longer than its capacity");
        if (end - start != 9 || s[start + 4] != '/' ||
            !parseHex(s + start, 4, 4, true, statusword) ||
            !parseHex(s + start + 5, 4, 4, true, monitor))
            return fail(f, RecordError::NUMBER, "starts", "start report is not XXXX/XXXX uppercase hex");
        starts.reports[count].statusword = uint16_t(statusword);
        starts.reports[count].monitor = uint16_t(monitor);
        start = end + 1;
    }
    starts.count = count;
    return true;
}
RecordBasis* basisField(MeasuredRecord& r, Field field) {
    switch (field) {
    case MODE_BASIS:
        return &r.enable.modeBasis;
    case BASE_BASIS:
        return &r.position.baseBasis;
    case LAYOUT_BASIS:
        return &r.velocity.layoutBasis;
    case RPM_BASIS:
        return r.kind == RecordKind::POSITION ? &r.position.rpm.basis : &r.velocity.rpm.basis;
    case DIRECTION_BASIS:
        return &r.velocity.directionBasis;
    case RAMP_BASIS:
        return r.kind == RecordKind::POSITION ? &r.position.rampMs.basis : &r.velocity.rampMs.basis;
    case DISTANCE_BASIS:
        return &r.position.distanceBasis;
    case DURATION_BASIS:
        return &r.velocity.durationBasis;
    case SCALE_BASIS:
        return &r.velocity.scaleBasis;
    case SIGNALS_BASIS:
        return &r.position.signalsBasis;
    case HALT_BASIS:
        return &r.velocity.halt.rpm.basis;
    case QUICK_BASIS:
        return &r.velocity.quickStop.rpm.basis;
    case WINDOW_BASIS:
        return &r.velocity.windowBasis;
    case STARTS_BASIS:
        return r.kind == RecordKind::ENABLE ? &r.enable.starts.basis : &r.velocity.starts.basis;
    default:
        return nullptr;
    }
}
// One value into the candidate. KIND and MOVE are parsed before this, to decide the key set.
bool parseValue(Fault& f, Field field, const char* s, size_t n, MeasuredRecord& r) {
    const char* key = KEYS[field].name;
    uint8_t word = 0;
    uint64_t number = 0;
    if (RecordBasis* basis = basisField(r, field)) {
        if (!parseWord(BASES, s, n, word))
            return fail(f, RecordError::VALUE, key, "basis is not measured, documented or inferred");
        *basis = RecordBasis(word);
        return true;
    }
    switch (field) {
    case EVIDENCE:
        if (n >= RECORD_EVIDENCE_CAPACITY)
            return fail(f, RecordError::VALUE, key, "evidence reference longer than 47 characters");
        for (size_t i = 0; i < n; ++i)
            r.evidence[i] = s[i];
        return true; // Its characters are checked with the whole record.
    case MODEL:
        if (!parseWord(MODELS, s, n, word))
            return fail(f, RecordError::VALUE, key, "model is not cl86_c or ol86_c");
        r.model = SettingsModel(word);
        return true;
    case IDENTITY:
        return parseIdentity(f, s, n, r.identity);
    case MODE:
        if (!parseDecimal(s, n, UINT8_MAX, number))
            return fail(f, RecordError::NUMBER, key, "mode is not a decimal byte");
        r.enable.modeRaw = uint8_t(number);
        return true;
    case BASE:
        if (!parseWord(RELATIVE_BASES, s, n, word))
            return fail(f, RecordError::VALUE, key, "base is not actual or commanded");
        r.position.relativeBase = RelativeBase(word);
        return true;
    case LAYOUT:
        if (!parseWord(LAYOUTS, s, n, word))
            return fail(f, RecordError::VALUE, key, "layout is not en86, zh86 or op86");
        r.velocity.layout = StatusLayout(word);
        return true;
    case RPM:
    case RAMP: {
        PositionScope& p = r.position;
        VelocityScope& v = r.velocity;
        const bool position = r.kind == RecordKind::POSITION;
        RecordRange& range = field == RPM ? (position ? p.rpm : v.rpm) : (position ? p.rampMs : v.rampMs);
        return parseRange(s, n, range) ||
               fail(f, RecordError::NUMBER, key, "range is not <decimal>..<decimal>");
    }
    case HALT:
    case QUICK: {
        RecordStop& stop = field == HALT ? r.velocity.halt : r.velocity.quickStop;
        stop.admitted = true;
        return parseRange(s, n, stop.rpm) ||
               fail(f, RecordError::NUMBER, key, "range is not <decimal>..<decimal>");
    }
    case DIRECTION:
        if (!parseWord(DIRECTIONS, s, n, word))
            return fail(f, RecordError::VALUE, key, "direction is not forward, reverse or both");
        r.velocity.direction = RecordDirection(word);
        return true;
    case DISTANCE:
        if (!parseDecimal(s, n, UINT32_MAX, number))
            return fail(f, RecordError::NUMBER, key, "distance is not a 32-bit decimal");
        r.position.maximumDistance = uint32_t(number);
        return true;
    case DURATION:
    case WINDOW:
        if (!parseDecimal(s, n, UINT64_MAX, number))
            return fail(f, RecordError::NUMBER, key, "time is not a 64-bit decimal");
        (field == DURATION ? r.velocity.maximumDurationUs : r.velocity.maximumWindowUs) = number;
        return true;
    case SCALE:
        if (!parseDecimal(s, n, UINT16_MAX, number))
            return fail(f, RecordError::NUMBER, key, "2201 is not a 16-bit decimal");
        r.velocity.scale2201 = uint16_t(number);
        return true;
    case ACCEPTANCE:
        return parseSignal(f, s, n, key, r.position.acceptance);
    case COMPLETION:
        return parseSignal(f, s, n, key, r.position.completion);
    case RULE:
        if (!parseWord(RULES, s, n, word))
            return fail(f, RecordError::VALUE, key, "rule is not after_absence or after_acceptance");
        r.position.rule = CompletionRule(word);
        return true;
    case STARTS:
        return parseStarts(f, s, n, r.kind == RecordKind::ENABLE ? r.enable.starts : r.velocity.starts);
    default:
        return true; // KIND and MOVE: parsed already.
    }
}
// Where each field's value lies in the text; size 0 means absent (a value is never empty).
struct Span {
    uint16_t start;
    uint16_t size;
};
bool optional(Field field) {
    return field == HALT || field == HALT_BASIS || field == QUICK || field == QUICK_BASIS;
}
bool decode(Fault& f, MeasuredRecord& r, const char* text, size_t size) {
    if (!size)
        return fail(f, RecordError::LENGTH, "", "record text empty");
    if (!text)
        return fail(f, RecordError::ARGUMENT, "", "record text null");
    if (size > RECORD_TEXT_CAPACITY)
        return fail(f, RecordError::LENGTH, "", "record text longer than RECORD_TEXT_CAPACITY");
    for (size_t i = 0; i < size; ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == ' ' ? !i || i + 1 == size || text[i - 1] == ' ' : c < 0x21 || c > 0x7e)
            return fail(f, RecordError::SYNTAX, "", "record text is not single-spaced printable ASCII");
    }
    // v=1 first.
    const size_t firstEnd = next(text, size, 0, ' ');
    const size_t firstEqual = next(text, firstEnd, 0, '=');
    if (!firstEqual || firstEqual + 1 >= firstEnd)
        return failSpan(f, RecordError::SYNTAX, text, firstEqual, "field is not key=value");
    if (!same(text, firstEqual, "v"))
        return fail(f, RecordError::MISSING_KEY, "v", "the first field is not v");
    if (!same(text + firstEqual + 1, firstEnd - firstEqual - 1, "1"))
        return fail(f, RecordError::VERSION, "v", "record version is not 1");
    // crc= last, over every byte before its space.
    if (firstEnd == size)
        return fail(f, RecordError::MISSING_KEY, "crc", "the last field is not crc");
    size_t lastSpace = size;
    while (text[lastSpace - 1] != ' ')
        --lastSpace;
    --lastSpace;
    const char* last = text + lastSpace + 1;
    const size_t lastSize = size - lastSpace - 1;
    const size_t lastEqual = next(last, lastSize, 0, '=');
    if (!lastEqual || lastEqual + 1 >= lastSize)
        return failSpan(f, RecordError::SYNTAX, last, lastEqual, "field is not key=value");
    if (!same(last, lastEqual, "crc"))
        return fail(f, RecordError::MISSING_KEY, "crc", "the last field is not crc");
    uint32_t stated;
    if (!parseHex(last + 4, lastSize - 4, 8, false, stated))
        return fail(f, RecordError::NUMBER, "crc", "crc is not eight lowercase hex digits");
    if (crc32Ieee(0, text, lastSpace) != stated)
        return fail(f, RecordError::CRC, "crc", "record CRC differs from its text");
    // The fields between, each key known and given once.
    Span spans[FIELDS] = {};
    for (size_t start = firstEnd + 1; start < lastSpace;) {
        const size_t end = next(text, lastSpace, start, ' ');
        const size_t equal = next(text, end, start, '=');
        const char* key = text + start;
        const size_t keySize = equal - start;
        if (!keySize || equal + 1 >= end)
            return failSpan(f, RecordError::SYNTAX, key, keySize, "field is not key=value");
        if (same(key, keySize, "v") || same(key, keySize, "crc"))
            return failSpan(f, RecordError::DUPLICATE_KEY, key, keySize, "v and crc appear once each");
        unsigned field = 0;
        while (field < FIELDS && !same(key, keySize, KEYS[field].name))
            ++field;
        if (field == FIELDS)
            return failSpan(f, RecordError::UNKNOWN_KEY, key, keySize, "key not defined by record version 1");
        if (spans[field].size)
            return fail(f, RecordError::DUPLICATE_KEY, KEYS[field].name, "key given twice");
        spans[field].start = uint16_t(equal + 1);
        spans[field].size = uint16_t(end - equal - 1);
        start = end + 1;
    }
    // The kind, and for a position record its move, decide which keys belong.
    uint8_t word = 0;
    if (!spans[KIND].size)
        return fail(f, RecordError::MISSING_KEY, "kind", "record kind missing");
    if (!parseWord(KINDS, text + spans[KIND].start, spans[KIND].size, word))
        return fail(f, RecordError::VALUE, "kind", "kind is not enable, position or velocity");
    r.kind = RecordKind(word);
    if (r.kind == RecordKind::POSITION) {
        if (!spans[MOVE].size)
            return fail(f, RecordError::MISSING_KEY, "move", "position move kind missing");
        if (!parseWord(MOVES, text + spans[MOVE].start, spans[MOVE].size, word))
            return fail(f, RecordError::VALUE, "move", "move is neither absolute nor relative");
        r.position.move = RecordMoveKind(word);
    }
    bool allowed[FIELDS];
    for (unsigned field = 0; field < FIELDS; ++field) {
        allowed[field] = (KEYS[field].kinds & kindBit(r.kind)) &&
                         ((field != BASE && field != BASE_BASIS) ||
                          r.position.move == RecordMoveKind::RELATIVE);
        if (spans[field].size && !allowed[field])
            return fail(f, RecordError::UNEXPECTED_KEY, KEYS[field].name, "key not used by this record");
    }
    for (unsigned field = 0; field < FIELDS; ++field) {
        // A stop pair is optional as a whole: either half without the other is missing its partner.
        const bool partnered = field == HALT || field == QUICK         ? spans[field + 1].size != 0
                               : field == HALT_BASIS || field == QUICK_BASIS ? spans[field - 1].size != 0
                                                                             : false;
        if (allowed[field] && !spans[field].size && (!optional(Field(field)) || partnered))
            return fail(f, RecordError::MISSING_KEY, KEYS[field].name, "required key missing");
    }
    for (unsigned field = 0; field < FIELDS; ++field)
        if (spans[field].size && field != KIND && field != MOVE &&
            !parseValue(f, Field(field), text + spans[field].start, spans[field].size, r))
            return false;
    return check(f, r);
}
} // namespace

uint32_t crc32Ieee(uint32_t previous, const char* data, size_t size) noexcept {
    if (!data)
        return previous;
    uint32_t crc = ~previous;
    for (size_t i = 0; i < size; ++i) {
        crc ^= static_cast<unsigned char>(data[i]);
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

Status validateRecord(RecordRefusal& refusal, const MeasuredRecord& record) noexcept {
    if (overlaps(&refusal, sizeof refusal, &record, sizeof record))
        return overlapRefusal();
    Fault f;
    check(f, record);
    return report(refusal, f);
}

Status encodeRecord(char* text, size_t capacity, size_t& length, RecordRefusal& refusal,
                    const MeasuredRecord& record) noexcept {
    if ((text && (overlaps(text, capacity, &record, sizeof record) ||
                  overlaps(text, capacity, &refusal, sizeof refusal) ||
                  overlaps(text, capacity, &length, sizeof length))) ||
        overlaps(&refusal, sizeof refusal, &record, sizeof record) ||
        overlaps(&length, sizeof length, &record, sizeof record) ||
        overlaps(&length, sizeof length, &refusal, sizeof refusal))
        return overlapRefusal();
    Fault f;
    if (!text && capacity)
        fail(f, RecordError::ARGUMENT, "", "record buffer null");
    else if (check(f, record)) {
        Writer count = {nullptr, 0};
        writeBody(count, record);
        if (!text || count.length + CRC_FIELD >= capacity)
            fail(f, RecordError::CAPACITY, "", "record buffer too small for the line and its NUL");
    }
    if (f.reason != RecordError::NONE)
        return report(refusal, f);
    Writer w = {text, 0};
    writeBody(w, record);
    const uint32_t crc = crc32Ieee(0, text, w.length);
    w.text(" crc=");
    w.hex(crc, 8, false);
    text[w.length] = '\0';
    length = w.length;
    return report(refusal, f);
}

Status decodeRecord(MeasuredRecord& output, RecordRefusal& refusal, const char* text,
                    size_t size) noexcept {
    if ((text && (overlaps(text, size, &output, sizeof output) ||
                  overlaps(text, size, &refusal, sizeof refusal))) ||
        overlaps(&output, sizeof output, &refusal, sizeof refusal))
        return overlapRefusal();
    Fault f;
    MeasuredRecord candidate;
    if (!decode(f, candidate, text, size))
        return report(refusal, f);
    output = candidate;
    return report(refusal, f);
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
