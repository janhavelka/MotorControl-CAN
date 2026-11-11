/** @file ProbeMotion.h
 * @brief Retained console motion intent over one existing application axis session.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "ProbeApp.h"

namespace MotorControlCANExample {
/** Concrete retained operation; stop results have their own two slots. */
enum class MotionKind : uint8_t {
    NONE,           ///< No retained ordinary operation.
    MOVE,           ///< Absolute, relative or wrapped finite move.
    VELOCITY,       ///< Finite velocity run, including its own stop.
    HOME,           ///< Method-specific homing.
    ENABLE,         ///< Explicit gated enable.
    ALARM_CLEAR,    ///< Explicit native alarm clear.
    POSITION_CLEAR, ///< Device counter zero, not host origin.
    STOP_NORMAL,    ///< Explicit normal stop request.
    STOP_FAST       ///< Explicit fast stop request, possibly superseding normal.
};
/** Small owned arithmetic projection. Native getters retain the complete typed preparation. */
struct MotionValue {
    bool present = false; ///< False means every following field is inactive.
    int64_t native = 0;   ///< Integer destination value, never converted through floating point.
    MotorControlCAN::NativeQuantity
        requested; ///< Exact endpoint for position; exact destination rate otherwise.
    bool relative =
        false; ///< Position only: native is a displacement, requested is still an endpoint.
    int64_t endpointNative =
        0; ///< Position only: quantized absolute endpoint in command increments.
    MotorControlCAN::IntegerQuantization quantization; ///< Signed exact destination error.
    bool exact = true;     ///< Whether requested and quantization are exact evidence.
    double errorBound = 0; ///< Absolute error in command increments or the named native rate.
    MotorControlCAN::NativeRateBasis rateBasis = MotorControlCAN::NativeRateBasis::
        COMMAND_INCREMENTS; ///< Rate destination spatial dimension.
    MotorControlCAN::TimeUnit rateTime =
        MotorControlCAN::TimeUnit::SECOND; ///< Rate destination time denominator.
};
/** Owned status projection, frozen at terminal. Never a physical standstill claim. */
struct MotionResult {
    MotionKind kind = MotionKind::NONE; ///< Actual requested operation.
    uint32_t id = 0; ///< Stable helper identity; independent of console and native operation IDs.
    uint32_t nativeOperation = 0; ///< Original native operation, never replaced by a fallback ID.
    MotorControlCAN::ActionState state =
        MotorControlCAN::ActionState::EMPTY; ///< Native operation lifecycle.
    MotorControlCAN::ActionOutcome outcome =
        MotorControlCAN::ActionOutcome::NONE; ///< First terminal cause.
    MotorControlCAN::Status status; ///< Operation status, not the request call's admission status.
    MotorControlCAN::ActionExecution execution =
        MotorControlCAN::ActionExecution::NOT_TRANSMITTED; ///< Effecting command evidence.
    MotorControlCAN::ActionCompletion completion =
        MotorControlCAN::ActionCompletion::NOT_OBSERVED; ///< Qualified controller observation only.
    bool uncertain = false;   ///< Retained possible write/read/motion effects.
    bool abortKnown = false;  ///< Full unsigned abortCode is meaningful.
    uint32_t abortCode = 0;   ///< Never narrowed into Status::detail.
    uint64_t completedUs = 0; ///< Native last processing time; zero is valid.
    MotionValue target;       ///< Move target, in command increments.
    MotionValue rate;      ///< Move/velocity rate or home search rate, with destination dimensions.
    MotionValue creepRate; ///< Home creep rate only.
};
/** One native component of a requested stop, including its actual command terminal. */
struct StopAttempt {
    bool present = false;   ///< This request created the native component.
    uint32_t operation = 0; ///< Component's native identity.
    MotorControlCAN::ActionState state =
        MotorControlCAN::ActionState::EMPTY; ///< Component lifecycle.
    MotorControlCAN::ActionOutcome outcome =
        MotorControlCAN::ActionOutcome::NONE; ///< Component terminal cause.
    MotorControlCAN::Status status; ///< Component status, independent of earlier attempts.
    MotorControlCAN::ActionExecution execution =
        MotorControlCAN::ActionExecution::NOT_TRANSMITTED; ///< Native command evidence.
    MotorControlCAN::ActionCompletion completion = MotorControlCAN::ActionCompletion::
        NOT_OBSERVED;            ///< Native observation; always absent for best effort.
    bool uncertain = false;      ///< Possible unconfirmed effects.
    bool commandPresent = false; ///< command is an actual retained owner terminal.
    MotorControlCAN::CANOPEN::SdoCompletion command; ///< Owned exact service terminal when present.
};
/** One stable explicit stop request; all fallback/recovery components belong to it. */
struct MotionStopResult {
    MotionResult result;            ///< Requested kind and stable identity with final path outcome.
    StopPath path = StopPath::NONE; ///< Last session path, not proof of physical stopping.
    StopAttempt sequenced;          ///< Original HALT/QUICK_STOP, when one could be composed.
    StopAttempt firstBestEffort;    ///< Controller-fault attempt proved unsent, if any.
    StopAttempt bestEffort;         ///< Last 000B component; no reply ever qualifies its execution.
    bool superseded = false;        ///< A fast request replaced this running normal stop.
};
/** One stationary attachment, not an executor. ProbeApp owns session progression.
 * All motion/action commands of this session must enter here while attached; external
 * calls that replace its contexts violate retention. The app, owner, registered session,
 * live target, configuration and reference outlive this object. Configuration/reference
 * remain stationary through native terminal settlement. Results retain one ordinary
 * operation and one stop of each kind; only normal-to-fast can overlap stop requests.
 * Release frees helper history only, never resets/reopens the session or clears a fence.
 *
 * Prerequisite reads/identity are borrowed until their setter replaces them or the
 * corresponding request succeeds. Policies and qualification records are copied;
 * nothing here invents qualification, acquires prerequisites or renews observation time.
 * Supplied times are monotonic microseconds, at least owner time, with exclusive deadlines.
 */
class ProbeMotion {
  public:
    /** Borrow the actual stationary graph; construction does not open a session or perform I/O.
     * @param app Stationary application. @param owner Its sole owner.
     * @param session Session registered through app.openAxis.
     * @param current Mutable actual route. @param axis Live conversion facts.
     * @param reference Actual qualified witness storage, never synthesized. */
    ProbeMotion(ProbeApp& app, MotorControlCANOwner::CanBusOwner& owner, AxisSession& session,
                MotorControlCAN::LICHUAN_C::ReadTarget& current, MotorControlCAN::AxisConfig& axis,
                MotorControlCAN::AxisReference& reference) noexcept;
    ProbeMotion(const ProbeMotion&) = delete;            ///< Live storage cannot be copied.
    ProbeMotion& operator=(const ProbeMotion&) = delete; ///< Live storage cannot be copied.
    /** @param app Expected application. @param owner Expected owner.
     * @return Exact app/owner identity, independent of route freshness. */
    bool matches(const ProbeApp& app,
                 const MotorControlCANOwner::CanBusOwner& owner) const noexcept;
    /** @return Actual borrowed live target; never an offline workspace. */
    const MotorControlCAN::LICHUAN_C::ReadTarget& target() const noexcept { return current_; }
    /** @return Borrowed live conversion facts; no implied origin or geometry. */
    const MotorControlCAN::AxisConfig& axisConfig() const noexcept { return axis_; }
    /** Change explicit live command scale with the core's stationary witness checks.
     * @param scale Exact increments per motor turn and caller provenance.
     * @param nowUs Nondecreasing microseconds; original observation time is never renewed.
     * @return Atomic local status. Requires no retained operations/read. Owner and session
     * advance together before publishing the candidate; old control/ordinary prerequisites expire.
     * An armed stop keeps its actual older state and fixed fallback policy. */
    MotorControlCAN::Status configureCommandScale(const MotorControlCAN::UnitScale& scale,
                                                  uint64_t nowUs) noexcept;
    /** Establish explicit live host origin, never device zero/save.
     * @param native Native command coordinate of host zero. @param nowUs Supplied microseconds.
     * @return Same atomic host-change contract; consumed reference is withdrawn after success. */
    MotorControlCAN::Status setOrigin(int64_t native, uint64_t nowUs) noexcept;
    /** @return Actual borrowed session, for evidence inspection only. */
    const AxisSession& session() const noexcept { return session_; }
    /** @return True only for registered session and complete current route/configuration facts. */
    bool current() const noexcept;

    /** Name actual move prerequisites. stored must already be registered with retainSettings.
     * @param state Acquired state. @param identity Acquired specimen identity.
     * @param evidence Copied explicit qualification. @param profile Copied default profile.
     * @param stored Optional retained settings read. @param options Copied finite budgets.
     * @return Local status; unchanged while an ordinary result is retained or graph is invalid. */
    MotorControlCAN::Status
    setMovePrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                         const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                         const MotorControlCAN::LICHUAN_C::MoveEvidence& evidence,
                         const MotorControlCAN::LICHUAN_C::MoveProfile& profile,
                         const MotorControlCAN::LICHUAN_C::DriverContext* stored,
                         const MotorControlCAN::LICHUAN_C::MoveOptions& options) noexcept;
    /** Name actual velocity prerequisites; no start/reverse/stop qualification is inferred.
     * @param state Acquired state. @param identity Acquired specimen identity.
     * @param evidence Copied qualification. @param profile Copied native ramps.
     * @param options Copied finite budgets. @return Same atomic setter contract as moves. */
    MotorControlCAN::Status
    setVelocityPrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                             const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                             const MotorControlCAN::LICHUAN_C::VelocityEvidence& evidence,
                             const MotorControlCAN::LICHUAN_C::VelocityProfile& profile,
                             const MotorControlCAN::LICHUAN_C::VelocityOptions& options) noexcept;
    /** Name method-specific home prerequisites; reads remain borrowed until request succeeds.
     * @param state Acquired state. @param identity Acquired specimen identity.
     * @param inputs Acquired input configuration. @param driver Acquired direction configuration.
     * @param evidence Copied qualification. @param options Copied finite budgets.
     * @return Same atomic setter contract as moves. */
    MotorControlCAN::Status
    setHomePrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                         const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                         const MotorControlCAN::LICHUAN_C::DriverContext& inputs,
                         const MotorControlCAN::LICHUAN_C::DriverContext& driver,
                         const MotorControlCAN::LICHUAN_C::HomeEvidence& evidence,
                         const MotorControlCAN::LICHUAN_C::HomeOptions& options) noexcept;
    /** Name explicitly authorized enable evidence; operation/trial identities are not repaired.
     * @param state Acquired state. @param evidence Copied trial authorization and facts.
     * @param options Copied budgets. @return Same atomic setter contract as moves. */
    MotorControlCAN::Status
    setEnablePrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                           const MotorControlCAN::LICHUAN_C::EnableEvidence& evidence,
                           const MotorControlCAN::LICHUAN_C::ActionOptions& options) noexcept;
    /** Name acquired clear prerequisites, without reading error history or manufacturing idle.
     * @param state Acquired native prerequisites. @param options Copied budgets.
     * @return Same atomic setter contract as moves. */
    MotorControlCAN::Status
    setClearPrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                          const MotorControlCAN::LICHUAN_C::ActionOptions& options) noexcept;
    /** Name stop state and fixed policy; empty state explicitly permits only policy's fallback.
     * May be replaced during motion or a normal stop. No operation is started here.
     * @param state Actual complete read or explicitly empty state. @param policy Copied limits.
     * @return Local status, unchanged for invalid budgets or unregistered session. */
    MotorControlCAN::Status
    setStopPrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                         const StopPolicy& policy) noexcept;

    /** Dispatch wrapped/relative/absolute typed intent. Atomic refusal; no implicit configuration.
     * @param request Exact typed intent. @param nowUs Supplied microseconds.
     * @param deadlineUs Exclusive future cutoff. @return Native preparation or local refusal. */
    MotorControlCAN::Status requestMove(const MotorControlCAN::MoveRequest& request, uint64_t nowUs,
                                        uint64_t deadlineUs) noexcept;
    /** Use an explicit request profile (ramp times/setup) with the same acquired prerequisites.
     * @param request Exact typed intent. @param profile Explicit profile for this request.
     * @param nowUs Supplied microseconds. @param deadlineUs Exclusive future cutoff.
     * @return Same atomic request contract; no stored/default profile is silently substituted. */
    MotorControlCAN::Status requestMove(const MotorControlCAN::MoveRequest& request,
                                        const MotorControlCAN::LICHUAN_C::MoveProfile& profile,
                                        uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Dispatch finite duration and explicit stop selection, preserving exact native-rate units.
     * @param request Exact rate and duration. @param stopKind Explicit normal/fast choice.
     * @param nowUs Supplied microseconds. @param deadlineUs Exclusive future cutoff.
     * @return Native preparation or local refusal, atomic on failure. */
    MotorControlCAN::Status requestVelocity(const MotorControlCAN::FiniteVelocityRequest& request,
                                            MotorControlCAN::LICHUAN_C::StopKind stopKind,
                                            uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Dispatch the typed home; native trigger invalidates the borrowed coordinates exactly once.
     * @param request Exact method/rates/ramp/offset. @param nowUs Supplied microseconds.
     * @param deadlineUs Exclusive future cutoff. @return Atomic native/local preparation status. */
    MotorControlCAN::Status requestHome(const MotorControlCAN::LICHUAN_C::HomeRequest& request,
                                        uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Dispatch explicit gated enable. No mode selection or automatic retry is added.
     * @param nowUs Supplied microseconds. @param deadlineUs Exclusive future cutoff.
     * @return Atomic native/local preparation status. */
    MotorControlCAN::Status requestEnable(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Dispatch explicit alarm clear, retaining an unresolved value-1 effect.
     * @param nowUs Supplied microseconds. @param deadlineUs Exclusive future cutoff.
     * @return Native/local preparation status; no new work is sent on refusal. */
    MotorControlCAN::Status requestAlarmClear(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Dispatch device counter zero; any nonzero value is refused before calling the session.
     * @param zero Required native value zero. @param nowUs Supplied microseconds.
     * @param deadlineUs Exclusive future cutoff. @return Native/local preparation status. */
    MotorControlCAN::Status requestPositionClear(int64_t zero, uint64_t nowUs,
                                                 uint64_t deadlineUs) noexcept;
    /** Request independent stop. Prechecks refuse unchanged; a native semantic refusal can
     * withdraw active ordinary work, as AxisSession::requestStop documents. service snapshots
     * that work honestly. No automatic recovery is started.
     * An owned running HALT may escalate using its retained native evidence and original policy
     * after the borrowed prerequisite was consumed; neither deadline nor evidence age is renewed.
     * @param kind Explicit normal/fast selection. @param nowUs Supplied microseconds.
     * @return OK only for an accepted path. */
    MotorControlCAN::Status requestStop(MotorControlCAN::LICHUAN_C::StopKind kind,
                                        uint64_t nowUs) noexcept;
    /** Withdraw the retained ordinary operation locally; in-flight services settle, no stop is sent.
     * @param nowUs Supplied microseconds.
     * @return False unchanged for invalid time, inactive work or replaced native identity.
     * A later service reports a replaced active context as INVALIDATED, without touching its replacement. */
    bool cancelNormal(uint64_t nowUs) noexcept;
    /** Snapshot evidence after app.service; does not service owner/session, transmit, retry or recover.
     * Follows only this helper's home/counter operation's already completed owner configuration
     * advance into the borrowed target and matching selection; retained reads keep old provenance.
     * @param nowUs Supplied microseconds, at least owner time.
     * @return False unchanged on backward time; active results remain live, terminals freeze. */
    bool service(uint64_t nowUs) noexcept;
    /** Exact motion-owned download guard. Call inside owner.beforeTransmit with the actual
     * per-node unhandled/maybe-fault decision. False must not fall through to a permissive guard.
     * Unrelated work returns false; dispatch guards by ownsDownload before calling this method.
     * @param sequence Actual reservation. @param transfer Exact borrowed pending download.
     * @param nowUs Owner processing time. @param driveFault Actual unhandled/maybe-fault fact.
     * @return False for unrelated, invalid-time or native-refused handover. */
    bool preflight(MotorControlCANOwner::SequenceId sequence,
                   const MotorControlCAN::CANOPEN::SdoTransfer& transfer, uint64_t nowUs,
                   bool driveFault) noexcept;
    /** @param sequence Actual reservation. @param transfer Exact borrowed pending download.
     * @return Exact download belonging to this attachment's motion/action/stop context. */
    bool ownsDownload(MotorControlCANOwner::SequenceId sequence,
                      const MotorControlCAN::CANOPEN::SdoTransfer& transfer) const noexcept;
    /** @return Ordinary result retained through releaseNormal, whether active or terminal. */
    bool retained() const noexcept { return normal_.kind != MotionKind::NONE; }
    /** @return Stable ordinary projection; valid while retained(), updated by service until terminal. */
    const MotionResult& normalResult() const noexcept { return normal_; }
    /** @param kind Stop selection. @return True when a requested stop of this kind is retained. */
    bool stopRetained(MotorControlCAN::LICHUAN_C::StopKind kind) const noexcept;
    /** @param kind Stop selection.
     * @return Retained stop or null for invalid/unretained kind; view ends on releaseStop. */
    const MotionStopResult* stopResult(MotorControlCAN::LICHUAN_C::StopKind kind) const noexcept;
    /** Release terminal ordinary history only; native contexts/fences remain untouched.
     * @return False unchanged without a retained terminal. */
    bool releaseNormal() noexcept;
    /** Release one terminal stop; cannot release a superseded stop before its in-flight settlement.
     * @param kind Stop selection. @return False unchanged unless its terminal is retained. */
    bool releaseStop(MotorControlCAN::LICHUAN_C::StopKind kind) noexcept;
    /** @return Concrete native context for retained matching ordinary kind, otherwise null. */
    const MotorControlCAN::LICHUAN_C::MoveContext* nativeMove() const noexcept;
    /** @return Concrete native velocity while its ordinary result is retained, otherwise null. */
    const MotorControlCAN::LICHUAN_C::VelocityContext* nativeVelocity() const noexcept;
    /** @return Concrete native home while its ordinary result is retained, otherwise null. */
    const MotorControlCAN::LICHUAN_C::HomeContext* nativeHome() const noexcept;
    /** @return Concrete native enable/clear while its ordinary result is retained, otherwise null. */
    const MotorControlCAN::LICHUAN_C::ActionContext* nativeAction() const noexcept;

  private:
    ProbeApp& app_;
    MotorControlCANOwner::CanBusOwner& owner_;
    AxisSession& session_;
    MotorControlCAN::LICHUAN_C::ReadTarget& current_;
    MotorControlCAN::AxisConfig& axis_;
    MotorControlCAN::AxisReference& reference_;
    MotionResult normal_;
    MotionStopResult stops_[2];
    uint32_t nextId_ = 0;
    uint32_t coordinateOperation_ = 0;
    bool coordinateHome_ = false;
    uint64_t lastUs_ = 0;
    int8_t activeStop_ = -1;
    const MotorControlCAN::LICHUAN_C::ReadContext *moveState_ = nullptr, *velocityState_ = nullptr,
                                                  *homeState_ = nullptr, *enableState_ = nullptr,
                                                  *clearState_ = nullptr, *stopState_ = nullptr;
    const MotorControlCAN::LICHUAN_C::IdentityObservation *moveIdentity_ = nullptr,
                                                          *velocityIdentity_ = nullptr,
                                                          *homeIdentity_ = nullptr;
    const MotorControlCAN::LICHUAN_C::DriverContext *stored_ = nullptr, *inputs_ = nullptr,
                                                    *driver_ = nullptr;
    MotorControlCAN::LICHUAN_C::MoveEvidence moveEvidence_;
    MotorControlCAN::LICHUAN_C::MoveProfile moveProfile_;
    MotorControlCAN::LICHUAN_C::MoveOptions moveOptions_;
    MotorControlCAN::LICHUAN_C::VelocityEvidence velocityEvidence_;
    MotorControlCAN::LICHUAN_C::VelocityProfile velocityProfile_;
    MotorControlCAN::LICHUAN_C::VelocityOptions velocityOptions_;
    MotorControlCAN::LICHUAN_C::HomeEvidence homeEvidence_;
    MotorControlCAN::LICHUAN_C::HomeOptions homeOptions_;
    MotorControlCAN::LICHUAN_C::EnableEvidence enableEvidence_;
    MotorControlCAN::LICHUAN_C::ActionOptions enableOptions_, clearOptions_;
    StopPolicy stopPolicy_;
    bool timeValid(uint64_t nowUs) const noexcept;
    bool graph() const noexcept;
    MotorControlCAN::Status prerequisite() const noexcept;
    MotorControlCAN::Status admission(uint64_t nowUs, uint64_t deadlineUs) const noexcept;
    MotorControlCAN::Status hostChange(uint64_t nowUs) const noexcept;
    void forgetPrerequisites() noexcept;
    void followCoordinates() noexcept;
    void accept(MotionKind kind, uint32_t nativeOperation, uint64_t nowUs) noexcept;
    void snapshotNormal() noexcept;
    void snapshotStops() noexcept;
};
} // namespace MotorControlCANExample
