/** @file MotionIntent.h
 * @brief Explicit host motion intent shared by application and console callers.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/VelocityOperation.h>
#include <MotorControlCAN/profiles/lichuan_c/Position.h>

namespace MotorControlCANExample {
/** Host choices only. Construction supplies no speed, ramp, origin or scale. Stored native
 * values and firmware qualification are checked separately by the ordinary operation path. */
struct SimpleMoveIntent {
    bool speedKnown = false;                ///< Explicit speed has been supplied.
    MotorControlCAN::VelocityRequest speed; ///< Original exact speed, never replaced by rounding.
    bool rampUpKnown = false;               ///< Explicit acceleration time has been supplied.
    bool rampDownKnown = false;             ///< Explicit deceleration time has been supplied.
    uint32_t rampUpMs = 0;   ///< Native 6083 milliseconds, not physical acceleration.
    uint32_t rampDownMs = 0; ///< Native 6084 milliseconds, not physical deceleration.
    MotorControlCAN::LICHUAN_C::MoveSetup setup =
        MotorControlCAN::LICHUAN_C::MoveSetup::WRITE_ALL; ///< Explicitly inspectable setup policy.
};
/** Build the shared simple-move coordinate/rounding policy, without conversion or I/O.
 * Relative steps are native displacement; absolute steps and angles use the motor
 * frame/host origin, and millimetres use the load frame. Nearest ties go to even,
 * with at most half a command increment error. No scale or origin is invented.
 * @param value Exact position intent. @param unit STEPS, DEGREES, TURNS or MILLIMETRES.
 * @param relative True for moveby, false for moveto. @param generation Actual axis epoch.
 * @param output Owned request, unchanged on refusal. @return Local validation status. */
MotorControlCAN::Status makeSimplePosition(const MotorControlCAN::Rational& value,
    MotorControlCAN::PositionUnit unit, bool relative, MotorControlCAN::Generation generation,
    MotorControlCAN::PositionRequest& output) noexcept;

/** Validate and retain an exact nonnegative speed within 0..3000 motor rpm. Zero is host intent
 * that prevents a move. No I/O occurs and output is unchanged on failure.
 * @param[in,out] intent Host choices. @param[in] speed Exact request with current generation.
 * @param[in] axis Current configured native rate destination; must be integer motor rpm.
 * @return Pure conversion/range status; no firmware permission is established. */
MotorControlCAN::Status setSimpleSpeed(SimpleMoveIntent& intent,
                                       const MotorControlCAN::VelocityRequest& speed,
                                       const MotorControlCAN::AxisConfig& axis) noexcept;
/** Validate an exact integer native ramp time in 0..2000 ms. Zero remains inspectable intent,
 * but buildSimpleMove refuses it because zero-ramp behavior is unqualified.
 * @param[in,out] intent Host choices. @param[in] acceleration True selects 6083, false 6084.
 * @param[in] milliseconds Exact time. @return Status; intent is unchanged on failure. */
MotorControlCAN::Status setSimpleRamp(SimpleMoveIntent& intent, bool acceleration,
                                      const MotorControlCAN::Rational& milliseconds) noexcept;
/** Set an existing native policy. REUSE_STORED verifies prior values and writes differences;
 * it does not mean an unchecked stored-only trajectory.
 * @param[in,out] intent Host choices. @param[in] setup Existing typed policy.
 * @return Status; unknown policies leave intent unchanged. */
MotorControlCAN::Status setSimpleSetup(SimpleMoveIntent& intent,
                                       MotorControlCAN::LICHUAN_C::MoveSetup setup) noexcept;
/** Construct an explicitly assumed host scale; the caller applies it through configureAxis.
 * @param[in] stepsPerTurn Positive exact command increments per motor turn, reduced to uint32 fields.
 * @param[out] output Scale unchanged on failure. @return Pure status; no live geometry is inferred. */
MotorControlCAN::Status makeCommandScale(const MotorControlCAN::Rational& stepsPerTurn,
                                         MotorControlCAN::UnitScale& output) noexcept;
/** Combine explicit host intent with a caller's position request. Position arithmetic, witnesses,
 * firmware gates and final rate quantization remain in the existing native move preparation.
 * @param[in] intent Complete speed/ramp choices.
 * @param[in] position Original request, including current generation and selected relative basis.
 * @param[in] axis Current host configuration with a declared command scale.
 * @param[in] simpleAbsolute True requires an unwrapped absolute request and explicit host origin;
 * false requires an unwrapped relative request with its supported basis. No basis fallback occurs.
 * @param[out] request Original exact position and speed; unchanged on failure.
 * @param[out] profile Native ramp/setup choices; unchanged on failure.
 * @return Local preflight only; no work is reserved or transmitted. */
MotorControlCAN::Status buildSimpleMove(const SimpleMoveIntent& intent,
                                        const MotorControlCAN::PositionRequest& position,
                                        const MotorControlCAN::AxisConfig& axis,
                                        bool simpleAbsolute, MotorControlCAN::MoveRequest& request,
                                        MotorControlCAN::LICHUAN_C::MoveProfile& profile) noexcept;
/** Validate finite velocity and retain the exact original request. The native lifecycle performs
 * its own single final preparation; a preview is never fed back as a new rounded request.
 * @param[in] velocity Exact signed rate; native frame means the configured native-rate dimension.
 * @param[in] durationMs Positive preparation-to-stop interval, at most INT64_MAX/1000 milliseconds.
 * @param[in] axis Current generation and integer motor-turn/minute destination.
 * @param[out] request Original rate and checked duration; unchanged on failure.
 * @param[out] preview Optional arithmetic-only rpm result, unchanged on failure. No terminal claim.
 * @return Local arithmetic/range status; reverse/start/stop firmware gates remain native obligations. */
MotorControlCAN::Status
makeFiniteVelocity(const MotorControlCAN::VelocityRequest& velocity, uint64_t durationMs,
                   const MotorControlCAN::AxisConfig& axis,
                   MotorControlCAN::FiniteVelocityRequest& request,
                   MotorControlCAN::PreparedVelocityTarget* preview = nullptr) noexcept;
} // namespace MotorControlCANExample
