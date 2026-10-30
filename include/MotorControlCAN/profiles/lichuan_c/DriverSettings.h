/** @file DriverSettings.h
 * @brief Finite native settings reads without I/O, allocation or clock access.
 * SPDX-License-Identifier: MIT
 * Upload effects remain unqualified. Contexts own their data; one cooperative
 * caller serializes mutation and retires owner work before reusing a context.
 * No write permission, enabling preamble or physical scale is inferred.
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/Reads.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Native groups; motor technology does not select common coordinate geometry. */
enum class DriverGroup : uint8_t {
    DRIVER,                  ///< 2200 direction selector and 2201 subdivision, raw U16.
    SOFT_LIMITS,             ///< 2202 selector and 607D count/signed native limits.
    MOTION_PARAMETERS,       ///< 6081 U32 rpm and 6083/6084 U32 ramp times in ms.
    IO_INPUT_CONFIGURATION,  ///< 2400 count, input polarity and seven input functions.
    IO_OUTPUT_CONFIGURATION, ///< 2400:0C..0F raw words; output dialect unresolved.
    IO_INPUT_FILTERS,        ///< 2401..2407 raw coefficients, no inferred time unit.
    IO_OUTPUT_COMMAND        ///< 60FE count/value/mask readback, not electrical levels.
};
/** Concrete caller-owned group lifecycle; terminal history survives until reuse. */
struct DriverContext {
    ReadContext read; ///< Existing owned phase/outcome/status, epochs and evidence.
};
/** Complete raw group; only fields selected by group are meaningful.
 * No observation is normalized to a permitted setting. Unknown selectors,
 * counts, out-of-range speeds/times and contradictory limits remain raw facts.
 */
struct DriverObservation {
    DriverGroup group = DriverGroup::DRIVER; ///< Selects valid fields below.
    ReadTarget target;                       ///< Checked channel/binding/configuration/node epochs.
    uint16_t directionRaw = 0;               ///< DRIVER: 2200, no clockwise convention.
    uint16_t subdivisionRaw = 0;             ///< DRIVER: 2201, no counts/turn inference.
    uint16_t softLimitSelectorRaw = 0;       ///< SOFT_LIMITS: 2202, including unknown 10.
    uint8_t softLimitEntries = 0;  ///< SOFT_LIMITS: 607D:00 raw highest subindex, no discovery.
    int32_t minimumNative = 0;     ///< SOFT_LIMITS: 607D:01 coordinate, basis unknown.
    int32_t maximumNative = 0;     ///< SOFT_LIMITS: 607D:02 coordinate, basis unknown.
    uint32_t positionSpeedRpm = 0; ///< MOTION_PARAMETERS: 6081 native U32 rpm.
    uint32_t rampUpTimeMs = 0;     ///< MOTION_PARAMETERS: 6083 ms, not acceleration rate.
    uint32_t rampDownTimeMs = 0;   ///< MOTION_PARAMETERS: 6084 ms, not stop evidence.
    bool interpreted = false;      ///< Remains false; no scale/state/enforcement qualification.
    uint8_t fieldCount = 0;        ///< 2, 4 or 3 fields in source order for the group.
    ReadProvenance fields[4];      ///< Owned per-object provenance, not an atomic snapshot.
    ReadWindow window;             ///< Conservative acquisition interval/skew of the group.
};
/** Prepare a complete group read atomically, without writing settings.
 * @param[out] output Replaced on success only; no overlap with inputs.
 * @param[in] target Explicit usable channel and current binding/configuration/node epochs.
 * @param[in] operation Nonzero identity, unique while prior work can exist.
 * @param[in] nowUs Nonwrapping monotonic microseconds, zero valid.
 * @param[in] deadlineUs Exclusive immutable cutoff, strictly after nowUs.
 * @param[in] options Nonzero queue/response budgets, clamped by the owner.
 * @param[in] group Declared driver/I/O group only; no 6060 upload or current/tuning sweep.
 * @return OK prepares owned work only; errors preserve output with ReadError detail.
 */
Status prepareDriverRead(DriverContext& output, const ReadTarget& target, uint32_t operation,
                         uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options,
                         DriverGroup group) noexcept;
/** Yield a copied upload through the existing ordinary service boundary.
 * @param[in] context Prepared driver context, borrowed for this call.
 * @param[in] nowUs Time at least last mutation, before an active deadline.
 * @param[out] output Unchanged on error; terminal success sets ready=false only.
 * @return Local OK is not admission/transmission/confirmation. No overlap with
 * context; repeated inspection is not permission to replay work.
 */
Status nextDriver(const DriverContext& context, uint64_t nowUs, PreparedRead& output) noexcept;
/** Consume a copied sole-owner terminal using the existing advanceRead checks.
 * @param[in,out] context Active driver read; invalid evidence leaves it unchanged.
 * @param[in] evidence Call-only terminal with matching request/epochs/timing; no overlap.
 * @param[in] nowUs Time at least completion/last mutation; deadline wins at equality.
 * @return OK consumes valid evidence including failure; inspect read.outcome,
 * full unsigned abort and unknown effects. No retry or hidden write.
 */
Status advanceDriver(DriverContext& context, const CANOPEN::SdoCompletion& evidence,
                     uint64_t nowUs) noexcept;
/** Settle local waiting after the caller retires separately admitted work.
 * @param[in,out] context Active driver context; history stays retained.
 * @param[in] outcome CANCELLED, DEADLINE or INVALIDATED only, as advanceRead.
 * @param[in] nowUs Nondecreasing supplied time; cutoff wins at equality.
 * @return OK advances the terminal; errors preserve context. No controller
 * retirement, motor stop or undo of possible upload effects is performed.
 */
Status advanceDriver(DriverContext& context, ReadOutcome outcome, uint64_t nowUs) noexcept;
/** Publish one complete current group atomically; partial fields stay historical.
 * @param[in] context Successful non-I/O native group read; I/O groups use getIo.
 * @param[in] current Actual current target/epochs, not obsolete copied provenance.
 * @param[in] nowUs Supplied time at least all retained delivery times.
 * @param[in] maximumAgeUs Nonzero inclusive age cap for each earliest observation bound.
 * @param[out] output Owned raw group, unchanged on failure; no overlap with inputs.
 * @return OK proves acquisition/currentness only, not interpretation or write permission.
 */
Status getDriver(const DriverContext& context, const ReadTarget& current, uint64_t nowUs,
                 uint64_t maximumAgeUs, DriverObservation& output) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
