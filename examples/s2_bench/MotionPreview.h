/** @file MotionPreview.h
 * @brief Offline review values for one future isolated position experiment.
 * No admission, execution, grant, controller call or firmware command route exists here.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "RawEvidence.h"

namespace MotorControlCANExample {
/** One proposed literal request, not permission to submit it. */
struct MotionPreviewStep {
    MotorControlCAN::CANOPEN::SdoExpectation
        expectation;              ///< Exact route/object/native width/direction.
    MotorControlCAN::Frame frame; ///< Owned source-reviewed request bytes, checked by public codec.
    uint32_t nativeBits = 0; ///< Proposed value, or required mode-display value for the upload.
    uint32_t delayAfterPreviousAckUs =
        0; ///< Application scheduling choice: 10000 for enable intervals.
};
/** Fixed review candidate only; it does not certify admission or motion completion. */
struct MotionPreview {
    MotorControlCAN::LICHUAN_C::ReadTarget
        target; ///< Actual acquisition route/epochs, not a default node.
    uint16_t subdivisionReadback =
        0; ///< Raw observed 2201; no pulse/angle/feedback scale inference.
    uint32_t retainedPositionTargetBits = 0; ///< Raw 607A, never zeroed implicitly.
    uint32_t retainedVelocityTargetBits =
        0;                               ///< Raw 60FF; signedness/start order remain unresolved.
    uint16_t statusword = 0;             ///< Raw 6041; no status dialect is chosen.
    uint16_t motionMonitor = 0;          ///< Raw 2100, separate from physical motion.
    uint8_t displayedMode = 0;           ///< Raw 6061; zero is undefined, not ready position mode.
    bool retainedTargetsNonzero = false; ///< At least one retained target is nonzero.
    bool executionReady = false; ///< Always false: this artifact never establishes live admission.
    uint32_t absoluteTargetNative =
        0; ///< Candidate absolute 607A coordinate 16; no displacement/angle promise.
    uint32_t observationBudgetUs = 0; ///< Fixed 5000000 caller policy, not a device timeout.
    uint8_t count = 0;                ///< Ten proposed steps; no automatic progression is supplied.
    MotionPreviewStep
        steps[10]; ///< Mode write/read, enable 6/7/15, target/speed/ramps and one trigger.
    MotionPreviewStep
        stationaryQuickStop; ///< Conditional 000F->000B value; independent enabled/stationary proof required.
    uint8_t stopBranchAfterStep = 0; ///< Zero-based index 4: the acknowledged 000F enable phase.
    bool postTriggerStopReviewed = false; ///< False: no invented 001F->000F/000B fallback.
};
/** Prepare the fixed non-executable review candidate from actual retained read evidence.
 * @param output Owned review values, unchanged on failure/no overlap with input.
 * @param readiness Successful exact thirty-field MOTION_READINESS collector record,
 * borrowed only during call and not edited by callers. Every service must be a
 * successful, exact-inventory raw upload with documentary width on the same
 * binding/node epoch. Width contradictions conservatively reject this preview.
 * Index 2201 must actually
 * indicate two bytes and contain 1000 for this specimen-scoped candidate.
 * @return Local preparation status only. The candidate uses absolute native
 * coordinate 16, one rpm, 1000 ms ramps and one 001F trigger. EN86/ZH86 pp11-12 supply
 * their native meaning; command/feedback/native-counter/reference qualification,
 * motor scale, stop after trigger, firmware state dialect, physical bounds and
 * execution authorization remain independent prerequisites. Actual 6064=0 alone
 * does not establish a small physical move. Relative-base behavior remains gated
 * for a separate future experiment and is not used by this candidate.
 * Neither emitted frames nor successful readiness reads qualify those facts.
 * Nonzero retained 607A/60FF values are copied explicitly; neither mode change nor
 * the enable prefix is admitted by this artifact. The stop branch is an
 * alternative only after independently authorized enable work already succeeded
 * and fresh state plus physical observation establish its prerequisites. Never
 * run 6/7/15 merely to prepare a stop. No branch after 001F is supplied.
 */
MotorControlCAN::Status prepareMotionPreview(MotionPreview& output,
                                             const RawEvidenceRecord& readiness) noexcept;
} // namespace MotorControlCANExample
