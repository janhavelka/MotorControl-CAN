/** @file State.h
 * @brief Passive documentary interpretation of checked Lichuan state reads.
 * No layout is a qualified firmware binding or permission to enable a drive.
 * No I/O, time acquisition, allocation or command generation occurs here.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Explicit source table for comparison; never selected from a firmware/language guess. */
enum class StatusLayout : uint8_t {
    UNSPECIFIED, ///< Keep status meanings unknown; still retain raw bits and monitor meanings.
    EN86,        ///< EN86 PDF p12: alarm7, zero-speed10, target-effective12.
    ZH86,        ///< ZH86 PDF p12: alarm3, running14.
    ZH57,        ///< ZH57 PDF p12: alarm7, running14; not proof of 86C applicability.
    OP86         ///< OP86 PDF p10: alarm7, running14; that page has a CL57/OL57 header.
};
/** Meaning under the selected document, not a qualified physical observation. */
enum class StateIndication : uint8_t {
    UNKNOWN, ///< Selected source does not define this field, or no source is selected.
    NO,      ///< Documented meaning is false (including explicitly inverted bits).
    YES      ///< Documented meaning is true.
};
/** Only the three unambiguous positive 6061 encodings; unknown bytes stay raw. */
enum class ObservedMode : uint8_t {
    UNKNOWN = 0,          ///< Raw byte has no reviewed mode meaning; not an assumed mode zero.
    PROFILE_POSITION = 1, ///< Documented position mode, not evidence of an idle trajectory.
    PROFILE_VELOCITY = 3, ///< Documented velocity mode, not permission to enable a retained target.
    HOMING = 6            ///< Documented homing mode, not proof of homed reference.
};
/** Bits 0..2 of 6041, the one status field all retained manuals list alike: words 0, 6, 7 and 15 are
 * shown with 000, 001, 011 and 111 (EN86 and ZH86 PDF p12, OP86 p10). Only an example table, not a
 * CiA402 decoder: two specimens read 111 after a power cycle with no 6040 write in any retained
 * record, so these bits alone never show that a word was written. */
constexpr uint16_t STATUS_LOW_BITS = 0x0007;
/** Bit 10 of 6041. EN86 PDF p12 labels it "0: motor in operating, 1: motor velocity 0"; ZH86 p12
 * and OP86 p10 do not list it. On node 1 on 2026-11-03 it read 0 in mode 0 (0x8037), 1 in mode 1
 * while energized and stationary (0x8437), and 0 again once 6040=6 released the motor (0x8031,
 * 2100 released set). It follows the drive's state, so an enable never compares it across its
 * words; 606C, not this bit, decides velocity. One specimen and firmware, not a qualification. */
constexpr uint16_t STATUS_EN86_ZERO_SPEED = 0x0400;
/** @name Object 2100 monitor bits
 * EN86 PDF p17 and ZH86 PDF p18 agree on these. They are not 6041 dialect bits, and a monitor
 * report is what the controller says, not an independent physical measurement.
 * @{
 */
constexpr uint16_t MONITOR_POSITION_COMPLETE = 0x0001;   ///< Bit0; no operation correlation.
constexpr uint16_t MONITOR_HOMING_COMPLETE = 0x0002;     ///< Bit1; no reference qualification.
constexpr uint16_t MONITOR_RUNNING = 0x0004;             ///< Bit2; controller report only.
constexpr uint16_t MONITOR_ALARM = 0x0008;               ///< Bit3; separate from status bits.
constexpr uint16_t MONITOR_RELEASED = 0x0010;            ///< Bit4; set released, clear enabled.
constexpr uint16_t MONITOR_POSITIVE_SOFT_LIMIT = 0x0020; ///< Bit5; no enforcement claim.
constexpr uint16_t MONITOR_NEGATIVE_SOFT_LIMIT = 0x0040; ///< Bit6; no enforcement claim.
constexpr uint16_t MONITOR_DOCUMENTED_BITS = 0x007f;     ///< Bits 0..6; all others stay unlisted.
/** @} */
/** One 6041 word read through one explicitly selected document. It is a value, with no route, time
 * or firmware behind it. A motion caller with a STATUS block or a qualified TPDO sample decodes
 * here instead of copying masks, and must still prove its own route, freshness and firmware
 * applicability. Decoding alone is never admission or completion evidence.
 */
struct StatusIndications {
    StatusLayout layout = StatusLayout::UNSPECIFIED; ///< Document this was read through.
    uint16_t statusword = 0;                         ///< Entire decoded 6041 value.
    uint8_t lowStatusBits = 0; ///< Raw bits 0..2, not a complete CiA402 state machine.
    bool lowPatternListed =
        false; ///< Selected table lists 000/001/011/111; no command acknowledgement.
    uint16_t unlistedStatusBits =
        0; ///< Set bits outside the selected table; all bits if UNSPECIFIED.
    StateIndication alarm = StateIndication::UNKNOWN;   ///< Status alarm under this source only.
    StateIndication running = StateIndication::UNKNOWN; ///< EN86 zero-speed bit is inverted here.
    StateIndication homingComplete =
        StateIndication::UNKNOWN; ///< Raw documented flag, not a new home.
    StateIndication positionComplete =
        StateIndication::UNKNOWN; ///< Raw flag, not current move completion.
    StateIndication targetEffective =
        StateIndication::UNKNOWN; ///< EN86 only; no full handshake implied.
};
/** Decode one raw 6041 word under an explicitly chosen document, with no I/O or stored state.
 * @param[in] statusword Entire raw word from any source the caller has already qualified.
 * @param[in] layout Documentary comparison. UNSPECIFIED keeps every meaning UNKNOWN and every set
 * bit unlisted; choosing a layout does not establish model or firmware applicability.
 * @param[out] output Replaced on success, unchanged on refusal.
 * @return OK for any word under a known layout, including unlisted bits and unlisted low patterns.
 * An unknown layout returns INVALID_CONFIG with ReadError::ARGUMENT.
 */
Status interpretStatusword(uint16_t statusword, StatusLayout layout,
                           StatusIndications& output) noexcept;
/** Owned interpretation for inspection. By itself it is not admission or completion evidence.
 * Default construction has no acquired provenance. A successful getter validates
 * the raw reads against its supplied current target/time only. Layout selection
 * does not establish model/firmware applicability. Disagreement can arise from
 * sequential sampling; no winner, generic readiness or physical state is inferred.
 */
struct StateInterpretation {
    ReadTarget target; ///< Full acquired route/binding/configuration/node identity.
    StatusLayout layout = StatusLayout::UNSPECIFIED; ///< Explicit documentary comparison.
    uint16_t statusword = 0;     ///< Entire 6041 value; cannot reconstruct a written 6040.
    uint8_t modeRaw = 0;         ///< Entire 6061 byte; no guessed signed interpretation.
    uint16_t monitor = 0;        ///< Entire 2100 word.
    uint16_t logicalInputs = 0;  ///< Entire 2101 word; no terminal assignment inference.
    uint16_t logicalOutputs = 0; ///< Entire 2102 word; no electrical output evidence.
    ObservedMode mode = ObservedMode::UNKNOWN; ///< Positive documented mode, independent of layout.
    uint8_t lowStatusBits = 0; ///< Raw bits 0..2, not a complete CiA402 state machine.
    bool lowPatternListed =
        false; ///< Selected table lists 000/001/011/111; no command acknowledgement.
    uint16_t unlistedStatusBits =
        0; ///< Set bits outside the selected table; all bits if UNSPECIFIED.
    uint16_t unlistedMonitorBits = 0; ///< Set bits outside documented 2100 bits 0..6.
    StateIndication alarm = StateIndication::UNKNOWN;   ///< Status alarm under this source only.
    StateIndication running = StateIndication::UNKNOWN; ///< EN86 zero-speed bit is inverted here.
    StateIndication homingComplete =
        StateIndication::UNKNOWN; ///< Raw documented flag, not a new home.
    StateIndication positionComplete =
        StateIndication::UNKNOWN; ///< Raw flag, not current move completion.
    StateIndication targetEffective =
        StateIndication::UNKNOWN;         ///< EN86 only; no full handshake implied.
    bool monitorPositionComplete = false; ///< 2100 bit0; no operation correlation.
    bool monitorHomingComplete = false;   ///< 2100 bit1; no reference qualification.
    bool monitorRunning = false;          ///< 2100 bit2; controller report only.
    bool monitorAlarm = false;            ///< 2100 bit3; separate from status interpretation.
    bool monitorReleased =
        false; ///< 2100 bit4 set means released, clear means enabled in the manual.
    bool positiveSoftwareLimit = false; ///< 2100 bit5; no limit frame/enforcement claim.
    bool negativeSoftwareLimit = false; ///< 2100 bit6; no limit frame/enforcement claim.
    bool alarmDisagreement =
        false; ///< Known status meaning differs from monitor; no winner chosen.
    bool runningDisagreement = false; ///< Known motion indications differ, subject to sample skew.
    bool homingDisagreement =
        false; ///< Known completion indications differ, not a failed home verdict.
    bool positionDisagreement =
        false;                ///< Known completion indications differ, not a failed move verdict.
    ReadProvenance fields[5]; ///< Owned evidence in order 6041,6061,2100,2101,2102.
    ReadWindow window; ///< Earliest through latest of all five reads, never an atomic snapshot.
};

/** Interpret two complete raw state blocks using an explicitly chosen document.
 * @param[in] context Ordinary SDO STATE context containing STATUS and MONITORS.
 * Independent complete blocks remain usable after a later block fails, subject
 * to getStateBlock validity. All five fields total nine bytes, so this combined
 * getter does not support a single Classical TPDO source: a TPDO caller decodes its checked
 * sample with interpretStatusword instead.
 * @param[in] current Supplied current route and binding/configuration/node epochs.
 * @param[in] nowUs Supplied nonwrapping monotonic microseconds, at least delivery times.
 * @param[in] maximumAgeUs Nonzero inclusive age bound from each earliest acquisition time.
 * @param[in] layout Documentary comparison, including UNSPECIFIED; not firmware qualification.
 * @param[out] output Owned result, replaced atomically on success; must not overlap inputs.
 * @return OK validates current raw acquisition and transcribes the selected source.
 * Unknown modes/bits and disagreements are successful inspectable data, not readiness.
 * INVALID_CONFIG uses ReadError details; invalid layout uses ARGUMENT. Rejection
 * leaves output and context unchanged. Borrowed inputs live only for this call;
 * callers serialize access against mutation. No work, write, retry or side effect
 * is generated; raw reads retain their existing Q20 effect classification.
 */
Status getStateInterpretation(const ReadContext& context, const ReadTarget& current, uint64_t nowUs,
                              uint64_t maximumAgeUs, StatusLayout layout,
                              StateInterpretation& output) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
