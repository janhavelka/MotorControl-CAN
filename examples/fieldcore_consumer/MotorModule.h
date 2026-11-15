/** @file MotorModule.h
 * @brief Bounded application consumer of the installed finite-move lifecycle.
 * SPDX-License-Identifier: MIT
 * These application-local types belong to neither FieldCore nor the installed library.
 * Calls perform no I/O, allocation, clock access, automatic retry or implicit motor action.
 * Supplied timestamps outside 0..INT64_MAX refuse before changing contexts or outputs.
 */
#pragma once
#include <MotorControlCAN/profiles/lichuan_c/Position.h>

/** Offline application example; supplied synthetic facts do not qualify hardware or FieldCore. */
namespace FieldCoreConsumer {
/** Installed common value and exact-unit contracts. */
namespace M = MotorControlCAN;
/** Installed CANopen service intents and retained terminals. */
namespace C = MotorControlCAN::CANOPEN;
/** Installed Lichuan finite-move and checked-read contracts. */
namespace L = MotorControlCAN::LICHUAN_C;

/** Check the supplied nonwrapping microsecond domain without reading a clock or changing storage.
 * @param[in] nowUs Caller timestamp, valid inclusively from zero through INT64_MAX.
 * @return OK only within that domain; other values return INVALID_CONFIG without side effects.
 */
M::Status validateTime(uint64_t nowUs);

/** Actual current owner facts, separate from the move's frozen preparation snapshot.
 * The caller serializes updates and supplies this live value on every owner turn.
 */
struct LiveAxis {
    M::AxisConfig axis; ///< Current units, scale, coordinate policy and configuration/reference epochs.
    L::ReadTarget target; ///< Current configured service route, binding and node lifecycle epoch.
    L::AxisReservation reservation; ///< Actual continuous exclusion held by the sole owner.
};
/** Local operation admission, independent of any controller transmission. */
enum class Admission : uint8_t {
    NONE, ///< No operation admitted; preparation refusal is returned through Status.
    ACCEPTED ///< Preparation succeeded and the ordinary lane owns its operation.
};
/** Physical settlement supplied by the owner, never native acknowledgement or motion completion. */
enum class WireSettlement : uint8_t {
    NOT_SENT, ///< No exchange admitted for this operation yet.
    UNKNOWN, ///< An exchange is outstanding or its physical settlement is unknown.
    SETTLED, ///< Owner reports physical transmission settled; no SDO success follows from this alone.
    FAILED ///< Owner reports failed physical settlement of the latest exchange.
};

/** Retained application result, copied before its slot is released.
 * Admission and wire fields are maintained during work. Other result fields become terminal
 * facts when MotorModule::resultReady is true. Full raw histories remain in MotorModule::move
 * until another preparation replaces them; copy required history before reuse.
 */
struct MoveResult {
    uint32_t operation = 0; ///< Exact nonzero application operation identity; zero before admission.
    M::Binding binding; ///< Frozen logical axis, endpoint and binding/configuration epochs.
    Admission admission = Admission::NONE; ///< Application admission; never a transmission claim.
    WireSettlement wire = WireSettlement::NOT_SENT; ///< Latest exchange's owner settlement only.
    M::ActionState state = M::ActionState::EMPTY; ///< Terminal move state, independent of drive state.
    M::ActionOutcome outcome = M::ActionOutcome::NONE; ///< Retained first terminal cause.
    M::ActionExecution setup = M::ActionExecution::NOT_TRANSMITTED; ///< Parameter-write evidence.
    M::ActionExecution execution = M::ActionExecution::NOT_TRANSMITTED; ///< Trigger evidence only.
    M::ActionCompletion completion = M::ActionCompletion::NOT_OBSERVED; ///< Controller report only.
    M::Status status; ///< Terminal operation status, independent of an advancing call's local status.
    int32_t effectiveTarget = 0; ///< Checked absolute target in native command increments.
    bool uncertain = false; ///< Retained possible effects; taking the result does not resolve them.
};

/** Caller-owned ordinary lane with one outstanding exchange and one retained terminal.
 * Keep this storage stationary through preparation, asynchronous completion and result take.
 * One cooperative owner serializes all mutation. It also owns transport/result storage and time.
 */
struct MotorModule {
    L::MoveContext move; ///< Owned production operation and its complete retained service history.
    L::ReadContext observation; ///< Stationary read context borrowed by move observation cycles.
    MoveResult result; ///< Application admission/wire facts and projected terminal result.
    bool outstanding = false; ///< One owner-admitted exchange awaits an accepted terminal.
    bool resultReady = false; ///< Typed terminal retained until exact take; blocks replacement.
};

/** Prepare one absolute move through the ordinary installed production API.
 * Every supplied fact is borrowed for this call only; the prepared operation owns its copies.
 * This example consumes no position witness and retains no settings contexts.
 * @param[in,out] module Stationary unused lane; unchanged on refusal, including an occupied lane.
 * @param[in] live Actual current axis, route and continuous owner reservation.
 * @param[in] state Fresh complete state read acquired under that reservation after control.
 * @param[in] control Actual latest acknowledged 6040=000F history; no enable is sent here.
 * @param[in] identity Current checked firmware identity on the same target and node epoch.
 * @param[in] evidence Explicit firmware-scoped acceptance/completion qualification; defaults refuse.
 * @param[in] request Exact unwrapped absolute position and positive profile-speed intent.
 * @param[in] profile Explicit native ramps in milliseconds and parameter-write policy.
 * @param[in] options Caller queue/response, observation-interval and maximum-age budgets in us.
 * @param[in] operation Nonzero application identity, not reused while old owner work exists.
 * @param[in] nowUs Supplied nondecreasing monotonic microseconds, 0..INT64_MAX, in one owner domain.
 * @param[in] deadlineUs Immutable exclusive processing cutoff in the same domain; no clock is read.
 * @return OK means application admission only. Refusal preserves the module and admits no work.
 */
M::Status prepareMove(MotorModule& module, const LiveAxis& live, const L::ReadContext& state,
                      const C::SdoCompletion& control, const L::IdentityObservation& identity,
                      const L::MoveEvidence& evidence, const M::MoveRequest& request,
                      const L::MoveProfile& profile, const L::MoveOptions& options,
                      uint32_t operation, uint64_t nowUs, uint64_t deadlineUs);
/** Yield copied work or an observation wait without admitting or transmitting an exchange.
 * A due cycle uses the module's stationary observation context. Supplied live facts are borrowed
 * only during this call. Repeated calls cannot offer an already admitted step again.
 * @param[in,out] module Prepared lane; may begin one due observation cycle.
 * @param[in] live Actual current route and continuous reservation, not an expired frozen copy.
 * @param[in] nowUs Current nondecreasing monotonic microseconds in the preparation domain.
 * @param[out] output Unchanged on refusal. A wait publishes ready=false and its eligibleUs.
 * If an exchange is outstanding or a terminal result is retained, the whole output is explicitly
 * reset to a default PreparedMove: both work flags and eligibleUs become zero/false.
 * @return Local yield status, never admission or operation completion. An expired active move
 * refuses until the owner explicitly interrupts it with DEADLINE after retiring outstanding work.
 */
M::Status nextStep(MotorModule& module, const LiveAxis& live, uint64_t nowUs,
                   L::PreparedMove& output);
/** Copy one eligible step into already reserved owner storage and mark it outstanding.
 * The sole owner reserves its queue/result slot first, then calls before possible transmission.
 * Downloads recheck live axis admission and record their possible transmission; no I/O occurs.
 * All supplied facts are borrowed only during the call. The owner keeps the copied transfer until
 * settlement. A delayed controller handoff must repeat the production admission check before TX.
 * @param[in,out] module Prepared lane; one successful admission marks outstanding=true.
 * @param[in] live Actual current axis, route and continuous reservation.
 * @param[in] nowUs Nondecreasing monotonic microseconds, before the immutable cutoff.
 * @param[out] ownerCopy Complete owned transfer on success; unchanged on refusal.
 * @return Local admission status; no eligible step refuses. Success establishes possible
 * transmission only, never actual TX, SDO acknowledgement or controller completion.
 */
M::Status admitStep(MotorModule& module, const LiveAxis& live, uint64_t nowUs,
                    C::SdoTransfer& ownerCopy);
/** Consume the actual terminal of the single outstanding exchange.
 * Supplied evidence is borrowed only during this call; the production context retains its copy.
 * The owner keeps its terminal and storage on refusal. Accepted service failure can finish the
 * move, so inspect the typed result rather than interpreting this local Status as motor success.
 * @param[in,out] module Outstanding lane; unchanged when correlation or evidence refuses.
 * @param[in] live Actual current route and reservation, including any qualified invalidation.
 * After an explicit withdrawal is recorded, the frozen admitted route identifies this terminal
 * for retirement only. Without withdrawal, changed live facts refuse; they never admit new work.
 * @param[in] completion Actual copied owner terminal, including identities, times and raw evidence.
 * @param[in] wire Owner's separate physical settlement classification for this exchange.
 * @param[in] nowUs Nondecreasing processing microseconds, at least completion.completedUs.
 * At/after deadline, accepted evidence is retained but cannot publish move success.
 * @return OK means evidence consumed and the owner may release its terminal storage. The lane
 * clears outstanding; a terminal move also publishes resultReady without losing uncertainty.
 */
M::Status advance(MotorModule& module, const LiveAxis& live, const C::SdoCompletion& completion,
                  WireSettlement wire, uint64_t nowUs);
/** Withdraw local work without a stop, rearm, replay or other motor command.
 * With outstanding work, record withdrawal and consume its actual terminal before publishing.
 * The supplied live value is borrowed only during this call. No pending owner storage is released.
 * @param[in,out] module Active lane whose outcome/evidence must remain retained.
 * @param[in] live Current route/reservation; INVALIDATED permits changed live epochs.
 * @param[in] reason CANCELLED, STOP, DRIVE_FAULT or INVALIDATED. Explicit DEADLINE is accepted
 * only with no outstanding exchange and nowUs at/after its cutoff. A cutoff also takes precedence
 * over another valid reason. STOP is a local reason; a real stop needs its own operation/result.
 * @param[in] nowUs Nondecreasing processing microseconds in the original owner domain.
 * @return Local withdrawal status. Refusal preserves the module. Without an outstanding exchange,
 * local termination publishes immediately; otherwise its reason remains until accepted settlement.
 */
M::Status interrupt(MotorModule& module, const LiveAxis& live, M::MoveInterruption reason,
                    uint64_t nowUs);
/** Copy the retained typed terminal before releasing the application result slot.
 * @param[in,out] module Lane with resultReady; success clears that flag only, preserving histories.
 * @param[in] operation Exact nonzero identity of the result to take.
 * @param[out] output Complete owned value on success; unchanged on refusal.
 * @return True for one exact take; false preserves both output and the retained slot. Neither
 * success nor elapsed time resolves command uncertainty or authorizes another motor operation.
 */
bool takeResult(MotorModule& module, uint32_t operation, MoveResult& output);
} // namespace FieldCoreConsumer
