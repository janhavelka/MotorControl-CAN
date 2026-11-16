/** @file BenchRouteNmt.h
 * @brief Explicit moving NMT experiment inside an existing velocity polling gap.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "../common/AxisSession.h"

namespace MotorControlCANExample {
/** Fixed experiment policy, in microseconds; not a drive timing guarantee. */
constexpr uint64_t NMT_VELOCITY_STAGE_US = 1500000;
/** Bounded experiment progression; no phase enables or repeats a command. */
enum class NmtVelocityPhase : uint8_t {
    EMPTY, ///< No experiment prepared.
    STOP_READY, ///< Clean poll gap, STOP not admitted.
    STOP_TX, ///< Exact STOP request awaiting local terminal.
    STOP_STATE, ///< Awaiting fresh Stopped heartbeat.
    PREOP_READY, ///< Preplanned PREOP awaits local retirement.
    PREOP_TX, ///< Exact PREOP request awaiting local terminal.
    PREOP_STATE, ///< Awaiting fresh Pre-operational heartbeat.
    POST_SAMPLE, ///< Existing native run supplies the next complete cycle.
    DONE ///< No further experiment work; owned results remain retained.
};
/** First failure, retained independently of the velocity run's own stop. */
enum class NmtVelocityFailure : uint8_t {
    NONE, ///< No failure recorded; not physical motion qualification.
    WINDOW, ///< Original native polling window expired or did not fit.
    STOP_ADMISSION, ///< STOP could not be admitted inside its fixed budget.
    STOP_TX, ///< STOP local terminal did not confirm transmission.
    STOP_STATE, ///< Fresh Stopped heartbeat missing within the fixed budget.
    PREOP_ADMISSION, ///< Preplanned PREOP could not be admitted inside its budget.
    PREOP_TX, ///< PREOP local terminal did not confirm transmission.
    PREOP_STATE, ///< Fresh Pre-operational heartbeat missing within its budget.
    RUN_ENDED, ///< Original run/stop ended the observation opportunity.
    LIFECYCLE ///< Original identity, epoch or immutable run bounds changed.
};
/** Caller-owned stationary record. Native samples are sequential controller reports only. */
struct NmtVelocityRecord {
    NmtVelocityPhase phase = NmtVelocityPhase::EMPTY; ///< Current phase.
    NmtVelocityFailure failure = NmtVelocityFailure::NONE; ///< Sticky first failure.
    uint32_t trial = 0, operation = 0; ///< Experiment and original velocity identities.
    uint8_t slot = 0; ///< Selected service channel, never broadcast.
    MotorControlCAN::CANOPEN::NetworkRoute target; ///< Frozen route/epoch.
    uint64_t startedUs = 0, stopEndUs = 0, preopEndUs = 0; ///< Fixed experiment cutoffs.
    uint64_t resumeUs = 0, stopDueUs = 0, deadlineUs = 0; ///< Original immutable run times.
    uint64_t stoppedUs = 0, preoperationalUs = 0; ///< Qualified heartbeat lower bounds; zero absent.
    bool attempted[2] = {}, terminal[2] = {}; ///< STOP then PREOP admission/terminal presence.
    bool closing = false; ///< No further work; retire any owned NMT first.
    MotorControlCAN::CANOPEN::NmtRequestId pending; ///< Exact owned local request.
    MotorControlCAN::CANOPEN::NmtCompletion result[2]; ///< Owned immutable local terminals.
    MotorControlCAN::LICHUAN_C::VelocitySample before, after; ///< Before STOP and first post-PREOP cycle.
};
/** Prepare only at a complete running cycle with enough natural polling gap.
 * @param output Unchanged on refusal; EMPTY on entry.
 * @param axis Existing finite velocity session, borrowed only during the call.
 * @param services Same owner's service store with fresh Operational evidence.
 * @param trial Nonzero experiment ID, at most (UINT32_MAX-1)/2.
 * @param nowUs Supplied monotonic processing time in microseconds.
 * @return True publishes fixed bounds; admits no work and changes no run context.
 */
bool prepareNmtVelocity(NmtVelocityRecord& output, const AxisSession& axis,
                        const MotorControlCAN::CANOPEN::SdoClient& services,
                        uint32_t trial, uint64_t nowUs) noexcept;
/** Advance one bounded phase after ordinary session service.
 * @param record Stationary prepared record; retains first failure and exact terminals.
 * @param axis Original velocity session; never modified here.
 * @param owner Sole owner; only addressed STOP/PREOP requests are admitted.
 * @param services That owner's service store.
 * @param nowUs Supplied monotonic processing time in microseconds.
 * No retry, deadline extension, SDO, enable or implicit NMT Start occurs.
 */
void serviceNmtVelocity(NmtVelocityRecord& record, const AxisSession& axis,
                        MotorControlCANOwner::CanBusOwner& owner,
                        MotorControlCAN::CANOPEN::SdoClient& services, uint64_t nowUs) noexcept;
} // namespace MotorControlCANExample
