/** @file Records.h
 * @brief Measured records of the Lichuan C profile and their one-line text codec, without I/O.
 * SPDX-License-Identifier: MIT
 *
 * A record states what a reviewed body of evidence established for one operation kind (enable,
 * position completion or velocity): the evidence it comes from, the basis of each field group
 * (MEASURED, DOCUMENTED or INFERRED), the identity tuple of the specimen it was made on (prompt 57
 * policy: the product fields must match a drive's, a firmware difference is reported) and the
 * ranges it covers. This header defines the record, its codec, the library evidence a record builds
 * (makeMoveEvidence, makeVelocityEvidence, makeEnableEvidence, with recordId as the record id) and the
 * checks of a request against a record (checkRecordMove, checkRecordVelocity, checkRecordEnable). A
 * record by itself opens no gate and enables or moves nothing; nothing here loads, binds, stores or
 * restores one. A caller checks a request against its record before the library's own preparation,
 * which still checks every request, so a record can narrow what a run may do but never widen a
 * library limit.
 *
 * Text form, version 1: one line of `key=value` fields separated by exactly one space, printable
 * ASCII only (0x21..0x7E besides the separators). `v=1` is the first field and `crc=<8 lowercase hex>`
 * the last: the CRC-32 (crc32Ieee, zlib-compatible) of every byte before the space that precedes
 * `crc=`, as Python's `zlib.crc32(line[:line.rindex(" crc=")].encode())`. The other fields may come in
 * any order; encodeRecord writes the order below. Every number is canonical: decimal without sign or
 * leading zero, hexadecimal uppercase with exactly the digits of its field, the CRC lowercase. A
 * range is `<minimum>..<maximum>`, both inclusive. A basis is `measured`, `documented` or `inferred`.
 *
 * | Key | Kinds | Value |
 * | --- | --- | --- |
 * | `kind` | all | `enable`, `position` or `velocity` |
 * | `evidence` | all | Evidence reference, 1..47 ASCII letters, digits, periods, underscores, hyphens, colons and slashes |
 * | `model` | all | `cl86_c` or `ol86_c` |
 * | `identity` | all | 1000.1009.100A.1018:01.1018:02.1018:03.2000.2001 raw, 8.4.4.8.8.8.4.4 hex digits |
 * | `mode`, `mode_basis` | enable | Enable mode, 1 or 6 |
 * | `move` | position | `absolute` or `relative` |
 * | `base`, `base_basis` | position, relative only | Relative base, `actual` or `commanded` |
 * | `layout`, `layout_basis` | velocity | Status layout, `en86`, `zh86` or `op86` |
 * | `rpm`, `rpm_basis` | position, velocity | 6081 (position) or 60FF magnitude (velocity), motor rpm, within 1..3000 |
 * | `direction`, `direction_basis` | velocity | `forward`, `reverse` or `both` |
 * | `ramp_ms`, `ramp_basis` | position, velocity | 6083 and 6084 each, ms, within 1..2000 |
 * | `distance_max`, `distance_basis` | position | Largest travel, 607A command increments: 1..1000000 relative, 1..2000000 absolute |
 * | `duration_max_us`, `duration_basis` | velocity | Longest run, microseconds, 1..INT64_MAX |
 * | `scale_2201`, `scale_basis` | velocity | 2201 subdivision the record was made at, SUBDIVISION_MINIMUM..SUBDIVISION_MAXIMUM |
 * | `acceptance`, `completion` | position | `<statusword or monitor>/<mask>/<value>`, 4 hex digits each |
 * | `rule`, `signals_basis` | position | `after_absence` or `after_acceptance`; basis of both signals and the rule |
 * | `halt_rpm`, `halt_basis` | velocity, optional pair | Rate range within which HALT was established |
 * | `quick_stop_rpm`, `quick_stop_basis` | velocity, optional pair | Rate range within which QUICK_STOP was established |
 * | `window_max_us`, `window_basis` | velocity | Largest enable-report window, microseconds, 0..`duration_max_us` |
 * | `starts`, `starts_basis` | enable, velocity | 1..RECORD_START_REPORTS `<6041>/<2100>` exact raw pairs, comma-separated |
 *
 * Every key of a record's kind is required, except the stop pairs, of which a velocity record names
 * at least one. Example (CRC elided): `v=1 kind=enable evidence=p58-enable model=cl86_c
 * identity=00040192.000C.0020.00001000.00000001.00000000.0400.0200 mode=1 mode_basis=measured
 * starts=8437/0001,4050/0014 starts_basis=measured crc=...`, all on one line.
 *
 * The ranges are those of a record: the documentary device limits named above, and where the library
 * already refuses more (the ZH57 velocity layout, a zero ramp), its limit. They say nothing about
 * what a drive does outside a record.
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/Position.h>
#include <MotorControlCAN/profiles/lichuan_c/Velocity.h>
#include <stddef.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Text format version this codec writes and reads: the `v` field. */
constexpr uint8_t RECORD_VERSION = 1;
/** Fixed capacities of a record and its text. */
enum {
    RECORD_EVIDENCE_CAPACITY = 48, ///< Evidence reference bytes including the NUL: 47 characters.
    RECORD_START_REPORTS = 8,      ///< Most start reports one record admits.
    RECORD_KEY_CAPACITY = 24,      ///< RecordRefusal::key bytes including the NUL: 23 characters.
    /** Most text bytes a record line may have, without a NUL. Every valid record encodes shorter
     * (test/records_test.cpp encodes the longest), so a buffer of RECORD_TEXT_CAPACITY + 1 always
     * holds encodeRecord's line and its NUL. */
    RECORD_TEXT_CAPACITY = 768
};
/** Highest rate a record may name, motor rpm: 6081 0..3000 rpm (EN86/ZH86 p11, OP86 p9) and 60FF
 * 0~3000 (EN86/ZH86 p13). A record's minimum is at least 1: zero is no trajectory. */
constexpr uint32_t RECORD_MAXIMUM_RPM = 3000;
/** Longest 6083/6084 ramp a record may name, ms: 0..2000 ms (EN86/ZH86 p11, OP86 p9). A record's
 * minimum is at least 1, as MoveProfile and VelocityProfile refuse zero (Q11). */
constexpr uint32_t RECORD_MAXIMUM_RAMP_MS = 2000;
/** Largest relative displacement a record may name, command increments: 607A spans
 * -1000000..1000000 pulses (EN86/ZH86 p11, OP86 p9), and a relative move writes it there. */
constexpr uint32_t RECORD_MAXIMUM_RELATIVE_DISTANCE = 1000000;
/** Largest absolute travel a record may name, command increments: the span between the two 607A
 * endpoints. */
constexpr uint32_t RECORD_MAXIMUM_ABSOLUTE_DISTANCE = 2000000;
/** Operation a record covers. */
enum class RecordKind : uint8_t {
    NONE,     ///< Default: refused.
    ENABLE,   ///< The enable sequence: MeasuredRecord::enable.
    POSITION, ///< Finite position moves with their completion: MeasuredRecord::position.
    VELOCITY  ///< Finite mode 3 velocity: MeasuredRecord::velocity.
};
/** What a field group rests on. */
enum class RecordBasis : uint8_t {
    NONE,       ///< Default: refused.
    MEASURED,   ///< Retained raw device records of the stated identity.
    DOCUMENTED, ///< A manufacturer document or EDS, not a measurement.
    INFERRED    ///< Reasoned from other evidence, neither measured nor documented as such.
};
/** Inclusive unsigned range with its basis. */
struct RecordRange {
    uint32_t minimum = 0;                    ///< Inclusive lower bound, in the owning field's unit.
    uint32_t maximum = 0;                    ///< Inclusive upper bound, at least minimum.
    RecordBasis basis = RecordBasis::NONE;   ///< Basis of both bounds.
};
/** One exact start report: the raw 6041 and 2100 words a run may start from. No mask: both words
 * must equal these values. Which classes of start the library itself admits is unchanged. */
struct StartReport {
    uint16_t statusword = 0; ///< 6041 raw.
    uint16_t monitor = 0;    ///< 2100 raw.
};
/** The start reports a record admits, without duplicates. */
struct RecordStarts {
    uint8_t count = 0;                             ///< Meaningful entries, 1..RECORD_START_REPORTS.
    StartReport reports[RECORD_START_REPORTS];     ///< The first count entries; the rest ignored.
    RecordBasis basis = RecordBasis::NONE;         ///< Basis of every listed report.
};
/** ENABLE scope. */
struct EnableScope {
    uint8_t modeRaw = 0;                           ///< 6061 mode the enables ran in: 1 or 6.
    RecordBasis modeBasis = RecordBasis::NONE;     ///< Basis of modeRaw.
    RecordStarts starts;                           ///< Start reports the enables ran from.
};
/** Which position move a POSITION record covers; one kind per record. */
enum class RecordMoveKind : uint8_t {
    NONE,     ///< Default: refused.
    ABSOLUTE, ///< prepareMoveAbsolute.
    RELATIVE  ///< prepareMoveRelative, with the relative base.
};
/** POSITION scope: the move kind, its ranges and its acceptance and completion reports. */
struct PositionScope {
    RecordMoveKind move = RecordMoveKind::NONE; ///< Covered move kind.
    /** RELATIVE only: the base measured for the 000F to 005F edge, ACTUAL or COMMANDED. Ignored, and
     * decoded as UNMEASURED, for ABSOLUTE. */
    RelativeBase relativeBase = RelativeBase::UNMEASURED;
    RecordBasis baseBasis = RecordBasis::NONE;  ///< RELATIVE only: basis of relativeBase.
    RecordRange rpm;    ///< 6081 profile velocity, motor rpm, within 1..RECORD_MAXIMUM_RPM.
    RecordRange rampMs; ///< 6083 and 6084 each, ms, within 1..RECORD_MAXIMUM_RAMP_MS.
    /** Largest travel of one move, 607A command increments: the relative displacement's magnitude,
     * or for ABSOLUTE the distance from the start position to the target. 1..
     * RECORD_MAXIMUM_RELATIVE_DISTANCE or 1..RECORD_MAXIMUM_ABSOLUTE_DISTANCE. */
    uint32_t maximumDistance = 0;
    RecordBasis distanceBasis = RecordBasis::NONE; ///< Basis of maximumDistance.
    /** MoveEvidence::acceptance: a source, a nonzero mask and a value within the mask. */
    MoveSignal acceptance;
    MoveSignal completion; ///< MoveEvidence::completion, as acceptance.
    CompletionRule rule = CompletionRule::NONE;    ///< MoveEvidence::rule; NONE refused.
    RecordBasis signalsBasis = RecordBasis::NONE;  ///< Basis of acceptance, completion and rule.
};
/** Directions a VELOCITY record covers. The rpm ranges are magnitudes for each covered direction. */
enum class RecordDirection : uint8_t {
    NONE,    ///< Default: refused.
    FORWARD, ///< Positive 60FF only.
    REVERSE, ///< Negative 60FF only.
    BOTH     ///< Either sign.
};
/** One stop kind's admission in a VELOCITY record. */
struct RecordStop {
    bool admitted = false; ///< The record established this stop kind; false ignores rpm.
    /** Run rates, motor rpm magnitude within 1..RECORD_MAXIMUM_RPM, from which the stop was
     * established, with its basis. */
    RecordRange rpm;
};
/** VELOCITY scope. */
struct VelocityScope {
    /** VelocityEvidence::layout: EN86, ZH86 or OP86; UNSPECIFIED and ZH57 are refused, as the
     * velocity preparation refuses them. */
    StatusLayout layout = StatusLayout::UNSPECIFIED;
    RecordBasis layoutBasis = RecordBasis::NONE;      ///< Basis of layout.
    RecordRange rpm; ///< 60FF magnitude, motor rpm, within 1..RECORD_MAXIMUM_RPM.
    RecordDirection direction = RecordDirection::NONE; ///< Covered signs of 60FF.
    RecordBasis directionBasis = RecordBasis::NONE;   ///< Basis of direction.
    RecordRange rampMs; ///< 6083 and 6084 each, ms, within 1..RECORD_MAXIMUM_RAMP_MS.
    /** Longest preparation-anchored run duration, microseconds, 1..INT64_MAX: the unit of
     * FiniteVelocityRequest::durationUs. */
    uint64_t maximumDurationUs = 0;
    RecordBasis durationBasis = RecordBasis::NONE;    ///< Basis of maximumDurationUs.
    /** Raw 2201 subdivision the record was made at, SUBDIVISION_MINIMUM..SUBDIVISION_MAXIMUM
     * (EN86/ZH86 p18). It claims no physical scale: SubdivisionSettings gives what was measured (Q04). */
    uint16_t scale2201 = 0;
    RecordBasis scaleBasis = RecordBasis::NONE;       ///< Basis of scale2201.
    RecordStop halt;      ///< StopKind::HALT; at least one of halt and quickStop is admitted.
    RecordStop quickStop; ///< StopKind::QUICK_STOP.
    /** Largest VelocityOptions::enableWindowUs a run under this record may use, microseconds, 0 (the
     * one-cycle rule) to maximumDurationUs. */
    uint64_t maximumWindowUs = 0;
    RecordBasis windowBasis = RecordBasis::NONE;      ///< Basis of maximumWindowUs.
    RecordStarts starts;                              ///< Start reports the runs began from.
};
/** One measured record. Only the scope of kind is meaningful: validateRecord and encodeRecord ignore
 * the other two, and decodeRecord leaves them at their defaults, as it does every field its kind,
 * move or stop admission ignores. Defaults are refused. */
struct MeasuredRecord {
    RecordKind kind = RecordKind::NONE;     ///< Covered operation.
    uint8_t version = RECORD_VERSION;       ///< Text format version; only RECORD_VERSION is valid.
    /** NUL-terminated evidence reference, 1..RECORD_EVIDENCE_CAPACITY - 1 ASCII letters, digits,
     * periods, underscores, hyphens, colons and slashes. It names the reviewed records; the library
     * never opens it. */
    char evidence[RECORD_EVIDENCE_CAPACITY] = {};
    SettingsModel model = SettingsModel::UNKNOWN; ///< CL86_C or OL86_C.
    /** Raw identity of the specimen the evidence was measured on (MEASURED basis); at least one
     * product field (1000, 1018:01, 1018:02, 2000) is nonzero. */
    IdentityValues identity;
    EnableScope enable;     ///< ENABLE only.
    PositionScope position; ///< POSITION only.
    VelocityScope velocity; ///< VELOCITY only.
};
/** Why a record or its text was refused: Status::detail of the codec functions. */
enum class RecordError : int32_t {
    NONE,          ///< No refusal.
    ARGUMENT,      ///< Null text with a nonzero size or capacity.
    OVERLAP,       ///< The text or an output overlaps another argument; nothing is written.
    CAPACITY,      ///< encodeRecord: the buffer cannot hold the line and its NUL.
    LENGTH,        ///< decodeRecord: empty text, or longer than RECORD_TEXT_CAPACITY.
    /** A byte outside printable ASCII, a space that is leading, trailing or doubled, or a field
     * without `=` or with an empty key or value. */
    SYNTAX,
    VERSION,       ///< The `v` field, or MeasuredRecord::version, is not RECORD_VERSION.
    CRC,           ///< The stated CRC differs from the text's.
    UNKNOWN_KEY,   ///< A key this version does not define.
    DUPLICATE_KEY, ///< A key given twice, `v` and `crc` included.
    /** A key the record needs is absent: `v` not first, `crc` not last, a key of its kind, one half
     * of a stop pair, or both stop pairs of a velocity record. */
    MISSING_KEY,
    UNEXPECTED_KEY, ///< A defined key that this record's kind or move does not use.
    NUMBER,        ///< A number that is not canonical for its field or beyond its type.
    /** A word, list, signal or text that is not admitted: an unknown enumerator, a default enum value,
     * a basis NONE, a start list that is empty, repeats a report or exceeds RECORD_START_REPORTS, a
     * signal that is not three parts, has a zero mask or a value outside it, evidence characters. */
    VALUE,
    RANGE,         ///< A range's minimum above its maximum.
    LIMIT,         ///< A value outside the documented device limits or the library's own.
    /** The identity is not eight fields, or all its product fields are zero. */
    IDENTITY,
    /** The CRC-32 of the record's canonical line is 0, which cannot serve as its nonzero record id
     * (recordId): change the evidence reference. Key `crc`. */
    ZERO_CRC
};
/** Diagnostic of the last codec call. */
struct RecordRefusal {
    RecordError reason = RecordError::NONE; ///< NONE after a success.
    /** NUL-terminated key of the offending field (`v`, `crc` and `identity` included), truncated to
     * RECORD_KEY_CAPACITY - 1 characters; empty when no field is at fault. */
    char key[RECORD_KEY_CAPACITY] = {};
};
/** CRC-32 IEEE 802.3 (reflected polynomial 0xEDB88320, initial and final XOR 0xFFFFFFFF), bitwise
 * without a table, compatible with zlib's crc32: crc32Ieee(0, "123456789", 9) is 0xCBF43926, and
 * crc32Ieee(crc32Ieee(0, a, n), b, m) equals the CRC of a followed by b. Pure, bounded by size, no I/O.
 * @param[in] previous 0 to start, or the result over the preceding bytes.
 * @param[in] data Borrowed bytes, read during the call only; may be null only when size is 0.
 * @param[in] size Byte count.
 * @return The CRC over the bytes so far; previous unchanged for size 0 or null data. */
uint32_t crc32Ieee(uint32_t previous, const char* data, size_t size) noexcept;
/** Check a record as encodeRecord and decodeRecord do: version, kind, evidence, model, identity and
 * every field its kind uses, against the rules above. Pure; no I/O or allocation.
 * @param[out] refusal Reason and key of the first refusal in the order of the key table above, or
 * NONE and an empty key on success. Must not overlap record.
 * @param[in] record Borrowed for the call.
 * @return OK when the record is valid. INVALID_CONFIG with VALUE, IDENTITY or MISSING_KEY (no stop
 * pair); UNSUPPORTED with VERSION; ILLEGAL_VALUE with RANGE or LIMIT; INVALID_CONFIG with OVERLAP,
 * refusal then unwritten. detail is the RecordError value. */
Status validateRecord(RecordRefusal& refusal, const MeasuredRecord& record) noexcept;
/** Encode a valid record as one line of version 1 text, in the key table's order, with its CRC and a
 * terminating NUL. Pure; no I/O or allocation; the line's length is computed before any byte is
 * written.
 * @param[out] text Caller buffer of capacity bytes. Written only on success: the line and a NUL.
 * @param[in] capacity Buffer bytes; at least the line's length plus 1. RECORD_TEXT_CAPACITY + 1
 * always suffices.
 * @param[out] length Line length without the NUL; written only on success.
 * @param[out] refusal As validateRecord's, or CAPACITY.
 * @param[in] record Borrowed for the call; must not overlap text, length or refusal.
 * @return OK with text and length written; validateRecord's refusals; INVALID_CONFIG with ARGUMENT for
 * a null text with a nonzero capacity, CAPACITY when the buffer is too small, OVERLAP. text and length
 * are unchanged on every failure. */
Status encodeRecord(char* text, size_t capacity, size_t& length, RecordRefusal& refusal,
                    const MeasuredRecord& record) noexcept;
/** Decode and validate one whole record line: syntax, `v=1` first, `crc=` last and matching, every
 * key known, unique and of the record's kind, every required key present, every value canonical and
 * within its limits (validateRecord). A line feed or carriage return is refused (SYNTAX): strip the
 * line ending first. Pure; no I/O or allocation. Checks run in that order, so a corrupted line reports CRC
 * before its field errors, after the version.
 * @param[out] output The record; written only on success, with the fields its kind, move and stop
 * admission ignore at their defaults. Must not overlap text or refusal.
 * @param[out] refusal Reason and offending key on failure (an unknown key copied, truncated); NONE
 * and an empty key on success. Written on every return except OVERLAP.
 * @param[in] text Borrowed characters, no NUL required, read during the call only.
 * @param[in] size Character count, 1..RECORD_TEXT_CAPACITY.
 * @return OK with output written. INVALID_CONFIG with ARGUMENT, OVERLAP, LENGTH, SYNTAX, CRC,
 * UNKNOWN_KEY, DUPLICATE_KEY, MISSING_KEY, UNEXPECTED_KEY, NUMBER, VALUE or IDENTITY; UNSUPPORTED with
 * VERSION; ILLEGAL_VALUE with RANGE or LIMIT. detail is the RecordError value; output is unchanged on
 * every failure. */
Status decodeRecord(MeasuredRecord& output, RecordRefusal& refusal, const char* text,
                    size_t size) noexcept;
/** The record id: the CRC-32 of the record's canonical line, the value encodeRecord writes after
 * `crc=`. A line with its fields in another order decodes to the same record and the same id. It
 * fills MoveEvidence::record and VelocityEvidence::record, the dialect epoch of a run, so a record
 * whose content changes gets another id. Pure; no I/O or allocation.
 * @param[out] output The id, nonzero; written only on success. Must not overlap record.
 * @param[in] record Borrowed for the call.
 * @return OK; validateRecord's refusals otherwise, ZERO_CRC included, with output unchanged. */
Status recordId(uint32_t& output, const MeasuredRecord& record) noexcept;

/** Status::detail of the record checks and evidence builders below (prompt 58 slice b2). */
enum class RecordCheckError : int32_t {
    NONE,     ///< No refusal.
    OVERLAP,  ///< An output overlaps an input; nothing is written.
    RECORD,   ///< The record itself is invalid (validateRecord, ZERO_CRC included).
    KIND,     ///< The record covers another operation kind.
    /** A fact the MEASURED gate relies on has a basis other than MEASURED: position rpm, ramps,
     * distance, signals and base; velocity rpm, direction, ramps, duration, 2201 scale, the selected
     * stop's range and the starts; enable mode and starts. Layout and window bound may rest on any
     * basis. */
    BASIS,
    /** The supplied library evidence is not the one the record builds: gate, record id, model,
     * identity, and for velocity every field; for enable gate, mode and the identity reference. */
    EVIDENCE,
    MOVE_KIND, ///< A relative request under an absolute record, or the reverse.
    BASE,      ///< Relative basis or MoveEvidence::relativeBase not the record's base.
    SIGNALS,   ///< MoveEvidence acceptance or completion not the record's.
    RULE,      ///< MoveEvidence::rule not the record's.
    RATE_BASIS, ///< The rate is not prepared as integer motor turns per minute.
    SPEED,     ///< Written 6081 or 60FF magnitude outside the record's rpm range, or zero.
    DIRECTION, ///< 60FF sign the record does not cover; or a reverse-only record (no forward range).
    RAMP,      ///< 6083 or 6084 outside the record's ramp range.
    DISTANCE,  ///< Travel beyond the record's distance bound.
    /** An absolute move without a known witness: its travel, and so the record's distance bound,
     * cannot be checked. */
    DISTANCE_UNKNOWN,
    DURATION,  ///< Requested duration beyond the record's longest.
    /** The axis command scale (UnitConfig::commandStepsPerMotorTurn) is not exactly the record's 2201
     * value: the record was made at another subdivision. The scale is the caller's configuration, not
     * a 2201 read. */
    SCALE,
    STOP_KIND,  ///< The selected stop kind is not admitted by the record.
    STOP_SPEED, ///< The rate is outside the selected stop's established rpm range.
    WINDOW,     ///< VelocityOptions::enableWindowUs beyond the record's window bound.
    START,      ///< The fresh 6041/2100 start report is not one the record admits.
    MODE,       ///< The fresh 6061 mode is not the record's enable mode.
    STATE,      ///< The state read has no current STATUS and MONITORS block for this check.
    /** Added to an AxisError of prepareVelocityTarget, keeping its Err and message. */
    RATE_ERROR_BASE = 10000,
    /** Added to a PositionParameterError of preparePositionParameters, keeping its Err and message. */
    PARAMETER_ERROR_BASE = 20000
};
/** Build the MoveEvidence a POSITION record opens: gate MEASURED, record recordId(record), its
 * model, identity, signals and rule, and for a RELATIVE record its base (UNMEASURED for ABSOLUTE).
 * The evidence carries no range: checkRecordMove applies them. Pure; no I/O.
 * @param[out] output Written only on success; must not overlap record.
 * @param[in] record Borrowed for the call.
 * @return OK; INVALID_CONFIG with RECORD, KIND or OVERLAP; UNSUPPORTED with BASIS. Output unchanged on
 * failure. */
Status makeMoveEvidence(MoveEvidence& output, const MeasuredRecord& record) noexcept;
/** Build the VelocityEvidence a VELOCITY record opens for one stop kind: gate MEASURED, record
 * recordId(record), model, identity, layout, startQualified and stopQualified true, stopKind,
 * reverseQualified for REVERSE or BOTH, maximumForwardRpm and maximumReverseRpm the record's rpm
 * maximum for each covered direction, maximumEnableWindowUs the record's window bound; no mode change,
 * range probe or reverse probe. Pure; no I/O.
 * @param[out] output Written only on success; must not overlap record.
 * @param[in] record Borrowed for the call.
 * @param[in] stopKind The stop the run will use; the record must admit it.
 * @return OK; INVALID_CONFIG with RECORD, KIND or OVERLAP; UNSUPPORTED with BASIS, STOP_KIND, or
 * DIRECTION for a reverse-only record, which VelocityEvidence cannot express (it needs a forward
 * range). Output unchanged on failure. */
Status makeVelocityEvidence(VelocityEvidence& output, const MeasuredRecord& record,
                            StopKind stopKind) noexcept;
/** Build the enable inputs an ENABLE record opens: EnableEvidence with gate MEASURED, the record's
 * mode, operation and start UNKNOWN (the fresh report decides it), and the IdentityReference for
 * ActionOptions::identityReference holding the record's identity. prepareEnable still admits MEASURED
 * in mode 1 only. Pure; no I/O.
 * @param[out] evidence Written only on success.
 * @param[out] reference Written only on success.
 * @param[in] record Borrowed for the call; no output may overlap it or the other output.
 * @param[in] operation The enable's operation identity, copied into evidence.operation.
 * @return OK; INVALID_CONFIG with RECORD, KIND or OVERLAP; UNSUPPORTED with BASIS. Outputs unchanged
 * on failure. */
Status makeEnableEvidence(EnableEvidence& evidence, IdentityReference& reference,
                          const MeasuredRecord& record, uint32_t operation) noexcept;
/** Check one move request against a bound POSITION record before prepareMoveAbsolute,
 * prepareMoveRelative or prepareMoveAngle (an angle is an absolute move), which still apply every
 * library limit: a record narrows a request, never widens one. The native values are prepared with
 * the same pure functions and inputs the move preparation uses: prepareVelocityTarget for 6081 and
 * preparePositionParameters for 607A. Checks, in order: the evidence is makeMoveEvidence(record),
 * the move kind and relative basis, the ramps, the written rpm, then the travel: the relative
 * displacement, or for an absolute move the endpoint minus a known witness. Pure: no I/O, clock or
 * output; a refusal happens before any context changes or work is yielded.
 * @param[in] record Bound record.
 * @param[in] evidence The MoveEvidence the move will use.
 * @param[in] request As the move preparation's.
 * @param[in] profile As the move preparation's.
 * @param[in] axis As the move preparation's.
 * @param[in] reference As the move preparation's; an absolute move needs a known one here
 * (DISTANCE_UNKNOWN otherwise).
 * @param[in] nowUs As the move preparation's.
 * @return OK when the record admits the request. RECORD, KIND, EVIDENCE, RATE_BASIS with
 * INVALID_CONFIG; BASIS, MOVE_KIND, BASE, SIGNALS, RULE, DISTANCE_UNKNOWN with UNSUPPORTED; SPEED,
 * RAMP, DISTANCE with ILLEGAL_VALUE; conversion failures with their base added. */
Status checkRecordMove(const MeasuredRecord& record, const MoveEvidence& evidence,
                       const MoveRequest& request, const MoveProfile& profile,
                       const AxisConfig& axis, const AxisReference* reference,
                       uint64_t nowUs) noexcept;
/** Check one finite velocity request against a bound VELOCITY record before prepareVelocity, which
 * still applies every library limit. Checks, in order: the evidence is
 * makeVelocityEvidence(record, options.stopKind), the ramps, the duration, the axis command scale
 * against the record's 2201, the written 60FF (prepareVelocityTarget, as the preparation): its sign
 * against the direction, its magnitude against the rpm range and the selected stop's range, then the
 * enable-report window, then the fresh start report. Pure: no I/O, clock or output.
 * @param[in] record Bound record.
 * @param[in] evidence The VelocityEvidence the run will use.
 * @param[in] request As prepareVelocity's.
 * @param[in] profile As prepareVelocity's.
 * @param[in] axis As prepareVelocity's.
 * @param[in] state As prepareVelocity's; its STATUS and MONITORS blocks must be current
 * (getStateBlock with current, nowUs and options.maximumAgeUs).
 * @param[in] current As prepareVelocity's.
 * @param[in] nowUs As prepareVelocity's.
 * @param[in] options As prepareVelocity's.
 * @return OK when the record admits the request. RECORD, KIND, EVIDENCE, RATE_BASIS, STATE with
 * INVALID_CONFIG; BASIS, STOP_KIND, DIRECTION, SCALE, START with UNSUPPORTED; SPEED, STOP_SPEED, RAMP,
 * DURATION, WINDOW with ILLEGAL_VALUE; a rate conversion failure with RATE_ERROR_BASE added. */
Status checkRecordVelocity(const MeasuredRecord& record, const VelocityEvidence& evidence,
                           const FiniteVelocityRequest& request, const VelocityProfile& profile,
                           const AxisConfig& axis, const ReadContext& state,
                           const ReadTarget& current, uint64_t nowUs,
                           const VelocityOptions& options) noexcept;
/** Check one enable against a bound ENABLE record before prepareEnable, which still applies every
 * library rule (a MEASURED enable is mode 1 only). Checks, in order: evidence gate MEASURED with the
 * record's mode, options.identityReference the record's identity (makeEnableEvidence), the fresh
 * 6061 mode, then the exact 6041/2100 start report. Pure: no I/O, clock or output.
 * @param[in] record Bound record.
 * @param[in] evidence The EnableEvidence the enable will use.
 * @param[in] options The ActionOptions it will use.
 * @param[in] state As prepareEnable's; its STATUS and MONITORS blocks must be current (getStateBlock
 * with current, nowUs and options.maximumAgeUs).
 * @param[in] current The reservation's target.
 * @param[in] nowUs As prepareEnable's.
 * @return OK when the record admits the enable. RECORD, KIND, EVIDENCE, STATE with INVALID_CONFIG;
 * BASIS, MODE, START with UNSUPPORTED. */
Status checkRecordEnable(const MeasuredRecord& record, const EnableEvidence& evidence,
                         const ActionOptions& options, const ReadContext& state,
                         const ReadTarget& current, uint64_t nowUs) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
