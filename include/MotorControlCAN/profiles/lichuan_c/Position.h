/** @file Position.h
 * @brief Pure position-parameter preparation for documented 86C models.
 * No work is admitted or transmitted, and no drive state, trajectory, firmware
 * dialect, relative wire basis or physical completion is established. Functions
 * perform bounded C++11 calculations without I/O, allocation or clock access.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Function-specific Status::detail for pure parameter preparation. */
enum class PositionParameterError : int32_t {
    NONE,           ///< No parameter error.
    ARGUMENT,       ///< Wrapped request or supplied time outside its monotonic domain.
    MODEL,          ///< No explicit CL86_C or OL86_C documentary applicability.
    REPRESENTATION, ///< Integer native representation is required; binary32 is unsupported.
    RANGE,          ///< Requested/effective target, profile rpm or ramp time exceeds its range.
    OVERLAP,        ///< Mutable output overlaps a borrowed input.
    AXIS_ERROR_BASE = 1000 ///< Added to an underlying AxisError, preserving its Err and message.
};
/** Owned parameter intent, independent of settings readback or device admission.
 * EN86/ZH86 PDF p11 and EDS [607A]/[6081]/[6083]/[6084] provide the object
 * widths and documentary ranges. Neither zero speed/ramp nor zero displacement
 * establishes a useful trajectory, standstill or completion. Retain this value
 * without editing fields; later admission must independently check current
 * binding, configuration, reference, firmware and operation prerequisites.
 */
struct PreparedPositionParameters {
    SettingsModel model =
        SettingsModel::UNKNOWN; ///< Explicit documented model, not firmware qualification.
    PreparedTarget target; ///< Exact intent, endpoint, conversion error and any frozen reference.
    AxisConfig axis; ///< Complete original units, origins, limits, binding and confidence epochs.
    DriverSettings settings; ///< Owned requested 6081 rpm and 6083/6084 ramp times in milliseconds.
    int32_t targetNative =
        0; ///< 607A candidate: absolute coordinate or relative displacement in pulses.
    uint64_t preparedUs = 0; ///< Supplied preparation time; zero is valid, not an observation time.
};
/** Prepare an immutable native parameter candidate atomically.
 * Target position 607A is I32 with inclusive -1000000..1000000 command pulses;
 * 6081 is U32 with 0..3000 rpm, and 6083/6084 are U32 with 0..2000 ms.
 * Requested AND quantized encoded targets must fit the documentary range.
 * Exact arithmetic checks the requested value exactly. Explicit approximate
 * arithmetic uses the existing upward total error bound around the effective
 * integer, rejecting conservatively when that interval crosses either bound.
 * Relative range checks apply to displacement; preparePosition separately
 * checks requested/effective absolute endpoints against the supplied axis limits.
 * All three relative bases describe host arithmetic only when the supplied axis
 * policy and matching witness allow them; no drive FIFO or wire basis is inferred.
 * Absolute targets without a witness retain unknown displacement. Zero is valid
 * parameter arithmetic and never yields a completed-move result.
 *
 * @param[out] output Owned snapshot, unchanged on every failure; must not overlap
 * request, axis, reference or settings, including their nested subobjects.
 * @param[in] model Explicit CL86_C or OL86_C selection; other values reject.
 * @param[in] request Unwrapped position intent, with the existing explicit
 * approximation policy for nonzero radians; no implicit approximation is added.
 * @param[in] axis Complete caller-owned host coordinate configuration.
 * @param[in] reference Optional witness, required by preparePosition policies.
 * A supplied witness is always checked. Its observation time, age cap, basis and
 * provenance are preserved; a local copy uses nowUs as the validation time.
 * A cached reference.nowUs later than nowUs rejects as a backward clock.
 * @param[in] settings Explicit native profile values; invalid defaults reject.
 * @param[in] nowUs Actual current nonwrapping monotonic microseconds in the same
 * domain as reference, inclusive 0..INT64_MAX. No timestamp is sampled or renewed.
 * @return OK proves parameter arithmetic only. Local errors use
 * PositionParameterError. Axis failures retain their Err/message and add
 * AXIS_ERROR_BASE to detail. Inputs are borrowed for this call only; the result
 * owns its data. Callers serialize mutation of shared inputs/output. No service
 * route, deadline, reservation, mode, acceptance or completion is established.
 */
Status preparePositionParameters(PreparedPositionParameters& output, SettingsModel model,
                                 const PositionRequest& request, const AxisConfig& axis,
                                 const AxisReference* reference, const DriverSettings& settings,
                                 uint64_t nowUs) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
