/** @file IoSettings.h
 * @brief Typed I/O observations through the existing native group read lifecycle.
 * SPDX-License-Identifier: MIT
 * Use prepareDriverRead/nextDriver/advanceDriver with one IO group. No I/O,
 * clock, allocation, hidden reassignment or output action occurs in this API.
 * The caller owns contexts/results and serializes mutation. Source code labels,
 * configured polarity, observed logical level and physical wiring are separate.
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/DriverSettings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Documentary DI function codes; none establishes current firmware or wiring. */
enum class InputFunction : uint16_t {
    UNDEFINED = 0,      ///< Undefined assignment, not proof a protection is disabled.
    ORIGIN = 1,         ///< Origin signal function, without a qualified homing reference.
    POSITIVE_LIMIT = 2, ///< Positive limit signal assignment.
    NEGATIVE_LIMIT = 3, ///< Negative limit signal assignment.
    ENABLE_MF = 4,      ///< Motor enabling/MF, electrical behavior not established.
    STOP = 5,           ///< Stop signal assignment, not a motion-stop result.
    EMERGENCY_STOP = 6, ///< Native emergency-stop input function, no safety rating.
    USER0 = 7,          ///< User input 0.
    USER1 = 8,          ///< User input 1.
    USER2 = 9,          ///< User input 2.
    USER3 = 10,         ///< User input 3.
    USER4 = 11,         ///< User input 4.
    USER5 = 12,         ///< User input 5.
    USER6 = 13,         ///< User input 6.
    UNKNOWN = 0xffff    ///< Unlisted raw code; retain inputFunctionRaw unchanged.
};
/** Documented DI0..6 polarity bits only; not a writable whole-word mask. */
constexpr uint16_t INPUT_POLARITY_BITS = 0x007f;
/** One complete selected I/O group; all other group's fields are unavailable.
 * Raw values, including unknown/reserved bits, are never normalized or written.
 * Per-field source intervals expose skew, not an atomic device snapshot.
 */
struct IoObservation {
    DriverGroup group = DriverGroup::IO_INPUT_CONFIGURATION; ///< Selects meaningful fields.
    ReadTarget target;             ///< Checked binding/configuration/node and service route epochs.
    uint16_t ioEntries = 0;        ///< INPUT_CONFIGURATION: U16 2400:00 raw highest subindex.
    uint16_t inputPolarityRaw = 0; ///< INPUT_CONFIGURATION: complete 2400:01 word.
    bool inputInverted[7] =
        {}; ///< INPUT_CONFIGURATION: documented bits0..6, no level/wiring claim.
    uint16_t inputFunctionRaw[7] = {};    ///< INPUT_CONFIGURATION: 2400:02..08, DI0..6.
    InputFunction inputFunctions[7] = {}; ///< INPUT_CONFIGURATION: documentary label or UNKNOWN.
    uint16_t outputConfig0CRaw = 0;       ///< OUTPUT_CONFIGURATION: 2400:0C, meaning unresolved.
    uint16_t outputConfig0DRaw = 0;       ///< OUTPUT_CONFIGURATION: 2400:0D, meaning unresolved.
    uint16_t outputConfig0ERaw = 0;       ///< OUTPUT_CONFIGURATION: 2400:0E, meaning unresolved.
    uint16_t outputConfig0FRaw = 0;       ///< OUTPUT_CONFIGURATION: 2400:0F, meaning unresolved.
    uint16_t inputFilterRaw[7] = {};      ///< INPUT_FILTERS: 2401..2407 coefficients, no time unit.
    uint8_t outputEntries = 0;            ///< OUTPUT_COMMAND: U8 60FE:00, never drives enumeration.
    uint32_t outputValueRaw = 0;          ///< OUTPUT_COMMAND: 60FE:01 readback, not pin voltage.
    uint32_t outputMaskRaw = 0; ///< OUTPUT_COMMAND: 60FE:02 readback, no inferred clear behavior.
    bool outputMappingKnown =
        false;                ///< Remains false; selected-model output dialect is unqualified.
    bool wiringKnown = false; ///< Remains false; no register read establishes wiring.
    bool electricalLevelsKnown =
        false;                ///< Remains false; configuration/readback is not electrical evidence.
    uint8_t fieldCount = 0;   ///< 9/4/7/3 fields for the selected group.
    ReadProvenance fields[9]; ///< Owned per-object evidence in fixed source order.
    ReadWindow window;        ///< Conservative acquisition interval and skew across the group.
};
/** Publish one complete current I/O group through the existing native context.
 * @param[in] context Successful I/O group read, prepared with prepareDriverRead.
 * @param[in] current Actual current target/epochs, not an obsolete stored copy.
 * @param[in] nowUs Supplied nonwrapping microseconds, at least retained delivery times.
 * @param[in] maximumAgeUs Nonzero inclusive age cap for each earliest observation bound.
 * @param[out] output Owned typed result on success; unchanged on failure, no overlap with inputs.
 * @return OK proves raw acquisition/currentness and documentary DI decoding only.
 * Wrong group, incomplete, stale or invalidated context fails without publication.
 * Does not validate a writable candidate, terminal mapping, actual level or a
 * homing/stop prerequisite. Upload effects remain independently unqualified.
 */
Status getIo(const DriverContext& context, const ReadTarget& current, uint64_t nowUs,
             uint64_t maximumAgeUs, IoObservation& output) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
