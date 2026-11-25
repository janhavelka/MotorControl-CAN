/** @file ProbeMotion.h
 * @brief Retained console motion intent over one existing application axis session.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "ProbeApp.h"
#include <MotorControlCAN/profiles/lichuan_c/Records.h>

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
    STOP_FAST,      ///< Explicit fast stop request, possibly superseding normal.
    MODE_SELECT,    ///< Typed 6060 mode select (prompt 57); never enables.
    RELEASE         ///< Release from the measured table (prompt 57); never a stop or standstill claim.
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
    /** The operation compared a current identity read (prompt 57): moves, velocity, homing, enable, a mode
     * select and a release that sent their word. False for a clear or stop, which compare none, and for an
     * already selected or released answer made without a current identity. */
    bool identityCompared = false;
    /** identityCompared only: firmware fields (1009, 100A, 1018:03, 2001) differ from the reference the
     * operation's qualification was made on; it ran regardless. The native context keeps both tuples. */
    bool firmwareUnqualified = false;
    /** The native context's retry class (prompt 55), copied for an application's repeat policy; the
     * library never repeats work. MOTION_LIKE for moves, velocities, homing and enables. */
    MotorControlCAN::RetryClass retryClass = MotorControlCAN::RetryClass::NOT_REPEATABLE;
    bool abortKnown = false;  ///< Full unsigned abortCode is meaningful.
    uint32_t abortCode = 0;   ///< Never narrowed into Status::detail.
    uint64_t completedUs = 0; ///< Native last processing time; zero is valid.
    MotionValue target;       ///< Move target, in command increments.
    MotionValue rate;      ///< Move/velocity rate or home search rate, with destination dimensions.
    MotionValue creepRate; ///< Home creep rate only.
    /** Application helper IDs of the retained original and replacement. Native contexts retain
     * their own operation IDs separately. Supersession withdraws only the original observer. */
    MotorControlCAN::MoveRelation relation;
    /** The operation ran under an attended experiment the caller set explicitly for it
     * (VelocityGate or EnableGate ATTENDED_EXPERIMENT; prompt 58). That trial takes precedence over a
     * bound record of the same slot, and a result under it qualifies nothing. */
    bool attendedExperiment = false;
    /** Nonzero: the id (LICHUAN_C::recordId) of the bound measured record whose evidence the operation
     * ran under; zero for none. */
    uint32_t record = 0;
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
/** One axis's measured record slots (prompt 58): one per operation a record covers. A position record
 * covers one move kind, so absolute and relative moves have a slot each. */
enum class RecordSlot : uint8_t {
    ENABLE,            ///< MeasuredRecord of RecordKind::ENABLE: requestEnable and startAction's enable.
    POSITION_ABSOLUTE, ///< RecordKind::POSITION, RecordMoveKind::ABSOLUTE: absolute and wrapped moves.
    POSITION_RELATIVE, ///< RecordKind::POSITION, RecordMoveKind::RELATIVE: relative moves.
    VELOCITY           ///< RecordKind::VELOCITY: requestVelocity.
};
/** Record slots per axis: at most one record per slot is held, loaded or bound. An application holds at
 * most RECORD_SLOTS records per attached AxisRecords, so its capacity is that times its axes. */
constexpr unsigned RECORD_SLOTS = 4;
/** What a slot holds, as ProbeMotion::showRecord reports it. */
enum class RecordState : uint8_t {
    EMPTY,  ///< Nothing loaded.
    LOADED, ///< Validated whole and held; no request uses it until useRecord binds it.
    BOUND,  ///< Bound to the axis's current generations: every request of its slot uses it.
    /** Bound earlier, and since then the axis binding, its configuration, the drive's boot (node
     * generation) or the owner's controller epoch (a recovery) changed. Every request of its slot is
     * refused, before any work, until useRecord binds it again or dropRecord forgets it. */
    INVALIDATED
};
/** The generations a record was bound to. A change of any of them invalidates it; none is ever reused,
 * so an invalidated record cannot become current again by itself. */
struct RecordBinding {
    MotorControlCAN::Binding binding; ///< Axis binding, with its binding and configuration generations.
    MotorControlCAN::Generation nodeGeneration = 0; ///< Drive lifecycle epoch; a boot-up advances it.
    uint32_t controllerEpoch = 0; ///< CanBusOwner::controllerEpoch(); a controller recovery advances it.
    uint64_t boundUs = 0;         ///< useRecord's supplied time; zero valid.
};
/** One record slot. Written only through ProbeMotion; an application never edits it. */
struct RecordEntry {
    bool loaded = false; ///< record holds a validated record.
    bool bound = false;  ///< useRecord bound it; binding is meaningful.
    uint32_t id = 0;     ///< LICHUAN_C::recordId of record, nonzero while loaded.
    MotorControlCAN::LICHUAN_C::MeasuredRecord record; ///< The decoded record, as loaded.
    RecordBinding binding;                             ///< The generations bound to, while bound.
};
/** Caller-owned record storage of one axis, in RAM only: nothing saves or restores it, so an MCU restart
 * forgets every record and a fresh one starts EMPTY. About 1.3 KB on the host build (four 296-byte records
 * plus their bookkeeping), estimated, not measured on a board. Stationary while attached. */
struct AxisRecords {
    RecordEntry slots[RECORD_SLOTS]; ///< Indexed by RecordSlot.
};
/** ProbeMotion::showRecord's copy of one slot. */
struct RecordView {
    RecordSlot slot = RecordSlot::ENABLE;   ///< The slot shown.
    RecordState state = RecordState::EMPTY; ///< Its state now, invalidation included.
    uint32_t id = 0;                        ///< Record id (CRC-32 of the canonical line).
    MotorControlCAN::LICHUAN_C::MeasuredRecord record; ///< The decoded record.
    RecordBinding binding; ///< BOUND or INVALIDATED: the generations it was bound to.
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
     * @param evidence Copied explicit qualification; a record bound to the move's slot (useRecord)
     * replaces it for that move kind. @param profile Copied default profile.
     * @param stored Optional retained settings read. @param options Copied finite budgets.
     * @return Local status; unchanged while an ordinary result is retained or graph is invalid. */
    MotorControlCAN::Status
    setMovePrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                         const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                         const MotorControlCAN::LICHUAN_C::MoveEvidence& evidence,
                         const MotorControlCAN::LICHUAN_C::MoveProfile& profile,
                         const MotorControlCAN::LICHUAN_C::DriverContext* stored,
                         const MotorControlCAN::LICHUAN_C::MoveOptions& options) noexcept;
    /** Install explicit replacement field/edge evidence and an optional measured feedback mapping.
     * @param evidence Copied capability or experiment scope, closed by default.
     * @param feedback Optional owned copy for changed endpoints. @return No change while retained. */
    MotorControlCAN::Status setMoveReplacementPrerequisites(
        const MotorControlCAN::LICHUAN_C::MoveReplacementEvidence& evidence,
        const MotorControlCAN::LICHUAN_C::MoveFeedbackRelation* feedback = nullptr) noexcept;
    /** Name actual velocity prerequisites; no start/reverse/stop qualification is inferred.
     * @param state Acquired state. @param identity Acquired specimen identity.
     * @param evidence Copied qualification. A bound VELOCITY record (useRecord) replaces it, unless it is
     * an ATTENDED_EXPERIMENT, which takes precedence over the record.
     * @param profile Copied native ramps.
     * @param options Copied finite budgets. @return Same atomic setter contract as moves. */
    MotorControlCAN::Status
    setVelocityPrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                             const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                             const MotorControlCAN::LICHUAN_C::VelocityEvidence& evidence,
                             const MotorControlCAN::LICHUAN_C::VelocityProfile& profile,
                             const MotorControlCAN::LICHUAN_C::VelocityOptions& options) noexcept;
    /** Install a separate explicit mid-run change record before starting a run. Closed by default;
     * a successful change never manufactures qualification. No device traffic or mode change.
     * @param evidence Copied measured or experiment scope. @return Unchanged while a result is retained. */
    MotorControlCAN::Status setVelocityChangePrerequisites(
        const MotorControlCAN::LICHUAN_C::VelocityChangeEvidence& evidence) noexcept;
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
     * @param state Acquired state. @param identity Acquired identity, borrowed like state.
     * @param evidence Copied trial authorization and facts. An ATTENDED_EXPERIMENT trial takes precedence
     * over a bound ENABLE record; any other gate is replaced by the record's MEASURED evidence and identity
     * reference.
     * @param options Copied budgets. @return Same atomic setter contract as moves. */
    MotorControlCAN::Status
    setEnablePrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                           const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                           const MotorControlCAN::LICHUAN_C::EnableEvidence& evidence,
                           const MotorControlCAN::LICHUAN_C::ActionOptions& options) noexcept;
    /** Name the fresh report of the regular commands (prompt 57): requestModeSelect, requestRelease, and
     * requestEnable when no explicit enable evidence is named, which then runs EnableGate::MEASURED in mode 1
     * under the session's next operation. Nothing here reads, enables or selects.
     * @param state Complete fresh SDO read of ERRORS, MONITORS, STATUS and FEEDBACK, borrowed until replaced
     * or consumed by a successful request. @param identity Current identity read, borrowed likewise; the
     * enable, the mode select and the release compare it (prepareEnable, prepareModeSelect, prepareRelease).
     * @param loop Optional fresh CONTROL observation, copied, for the
     * release table's row 2 (2500=1); null for none. @param options Copied budgets: maximumAgeUs covering nine
     * fields and commandIntervalUs at least ENABLE_WORD_INTERVAL_US serve all three.
     * @return Same atomic setter contract as moves. */
    MotorControlCAN::Status
    setActionPrerequisites(const MotorControlCAN::LICHUAN_C::ReadContext& state,
                           const MotorControlCAN::LICHUAN_C::IdentityObservation& identity,
                           const MotorControlCAN::LICHUAN_C::ControlObservation* loop,
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

    /** Attach this axis's record storage (prompt 58). Without one every record call is refused and every
     * request runs on the evidence its setter named, as before. Nothing is read, sent or bound here.
     * @param storage Caller-owned, stationary while attached; its current contents are kept. */
    void attachRecords(AxisRecords& storage) noexcept { records_ = &storage; }
    /** Load one record line into its slot: decodeRecord validates the whole line, and only a valid
     * record is stored, LOADED, so it takes no effect until useRecord binds it. Never sends, enables or
     * moves anything.
     * @param text Borrowed characters of one line, without its line ending. @param size Character count.
     * @param refusal decodeRecord's reason and key on a decoding failure; NONE otherwise.
     * @param slot Written on success: the slot the record's kind and move fill.
     * @return OK; decodeRecord's status (RecordError detail) for an invalid line; ActionError STATE for no
     * attached storage or an occupied slot (drop it first). Storage unchanged on failure. */
    MotorControlCAN::Status loadRecord(const char* text, size_t size,
                                       MotorControlCAN::LICHUAN_C::RecordRefusal& refusal,
                                       RecordSlot& slot) noexcept;
    /** Load a record built in code, as the line overload, validated with validateRecord.
     * @param record Copied. @param refusal validateRecord's reason and key, NONE otherwise.
     * @param slot Written on success. @return As the line overload. */
    MotorControlCAN::Status loadRecord(const MotorControlCAN::LICHUAN_C::MeasuredRecord& record,
                                       MotorControlCAN::LICHUAN_C::RecordRefusal& refusal,
                                       RecordSlot& slot) noexcept;
    /** Bind a loaded record to this axis: from now on every request of its slot that is not an attended
     * experiment named for it builds its evidence from the record (makeMoveEvidence, makeVelocityEvidence, makeEnableEvidence) and is checked
     * against it (checkRecordMove, checkRecordVelocity, checkRecordEnable) before the library's own
     * preparation, which keeps every library limit. Records the current binding, node generation and
     * controller epoch; binding again after an invalidation records the new ones. Sends, enables and
     * moves nothing, and a retained operation keeps the evidence it was prepared with.
     * @param slot A LOADED, BOUND or INVALIDATED slot. @param nowUs Supplied microseconds, at least the
     * helper's and the owner's latest time.
     * @return OK; ActionError STATE for no storage or an empty slot, TARGET for a stale graph or a record of
     * another model than the session's, TIME for a backward clock. Unchanged on failure. */
    MotorControlCAN::Status useRecord(RecordSlot slot, uint64_t nowUs) noexcept;
    /** Copy one slot's record and binding state, judging invalidation now. No I/O.
     * @param slot Slot to show. @param output Written only when the slot holds a record.
     * @return False for no storage or an empty slot. */
    bool showRecord(RecordSlot slot, RecordView& output) const noexcept;
    /** Forget one slot's record, bound or not; requests of its slot then use their setter's evidence
     * again. A retained operation keeps the evidence it was prepared with. Nothing is sent.
     * @param slot Slot to empty. @return False for no storage or an already empty slot. */
    bool dropRecord(RecordSlot slot) noexcept;

    /** Dispatch wrapped/relative/absolute typed intent. Atomic refusal; no implicit configuration.
     * A record bound to the move's slot builds the evidence and checkRecordMove narrows the request
     * (RecordCheckError detail) before the session's preparation; an absolute move then needs a known
     * witness. A record bound to older generations refuses with ActionError STATE.
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
    /** Admit one explicit replacement of this helper's active move, keeping both results.
     * @param id Original normalResult().id. @param request Exact selected fields and edge policy.
     * @param nowUs Processing time; the original deadline is retained. @return Atomic admission. */
    MotorControlCAN::Status requestMoveReplacement(uint32_t id,
        const MotorControlCAN::LICHUAN_C::MoveReplacementRequest& request, uint64_t nowUs) noexcept;
    /** Dispatch finite duration and explicit stop selection, preserving exact native-rate units. An
     * attended experiment the setter named runs as named (MotionResult::attendedExperiment). Otherwise a
     * bound VELOCITY record builds the evidence for stopKind and checkRecordVelocity narrows the run
     * first (MotionResult::record); a record bound to older generations refuses with ActionError STATE.
     * @param request Exact rate and duration. @param stopKind Explicit normal/fast choice.
     * @param nowUs Supplied microseconds. @param deadlineUs Exclusive future cutoff.
     * @return Native preparation or local refusal, atomic on failure. */
    MotorControlCAN::Status requestVelocity(const MotorControlCAN::FiniteVelocityRequest& request,
                                            MotorControlCAN::LICHUAN_C::StopKind stopKind,
                                            uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Change the rate of this helper's retained active velocity run once, preserving its stop.
     * @param id Stable normalResult().id of that run. @param request Exact engineering rate.
     * @param nowUs Processing time. @return Atomic native admission or refusal; no new result identity. */
    MotorControlCAN::Status requestVelocityChange(uint32_t id,
        const MotorControlCAN::VelocityRequest& request, uint64_t nowUs) noexcept;
    /** Dispatch the typed home; native trigger invalidates the borrowed coordinates exactly once.
     * @param request Exact method/rates/ramp/offset. @param nowUs Supplied microseconds.
     * @param deadlineUs Exclusive future cutoff. @return Atomic native/local preparation status. */
    MotorControlCAN::Status requestHome(const MotorControlCAN::LICHUAN_C::HomeRequest& request,
                                        uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Dispatch explicit gated enable. No mode selection or automatic retry is added. An attended trial
     * named by setEnablePrerequisites runs as named. Otherwise (the regular MEASURED enable, or other
     * explicit evidence) a bound ENABLE record builds MEASURED evidence and the identity reference, and
     * checkRecordEnable narrows the start report first, on whichever report is named; a record bound to
     * older generations refuses.
     * @param nowUs Supplied microseconds. @param deadlineUs Exclusive future cutoff.
     * @return Atomic native/local preparation status. */
    MotorControlCAN::Status requestEnable(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Dispatch the typed mode select (AxisSession::requestModeSelect) from the named action report.
     * @param modeRaw 1, 3 or 6. @param nowUs Supplied microseconds. @param deadlineUs Exclusive cutoff.
     * @return Native/local status; a mode the report already shows succeeds with nothing sent. */
    MotorControlCAN::Status requestModeSelect(uint8_t modeRaw, uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Dispatch the release from the measured table (AxisSession::requestRelease) from the named action
     * report, with no experiment option. Never a stop, never a standstill claim, never resent.
     * @param nowUs Supplied microseconds. @param deadlineUs Exclusive cutoff.
     * @return Native/local status: the table's refusal reason, or OK with the row's word to come or nothing
     * sent for an already released report. */
    MotorControlCAN::Status requestRelease(uint64_t nowUs, uint64_t deadlineUs) noexcept;
    /** Run one regular action (prompt 57) on this helper's own fresh reads, for an application that names no
     * report: the identity, for a release CONTROL (2500 for the table's row 2), then ERRORS, MONITORS, STATUS
     * and FEEDBACK, each under its own session operation, as ProbeCommissioning::startPersistence reads its
     * prerequisites. When the last read completes, service() requests the action at that time exactly as
     * requestEnable, requestModeSelect or requestRelease would on a named report, with no explicit enable
     * evidence (EnableGate::MEASURED, mode 1). Only this call starts it: nothing here runs on startup,
     * selection or recovery, and no read or request is repeated. A read failure or the library's refusal
     * ends the result FAILED with execution NOT_TRANSMITTED and that status; cancelNormal withdraws it.
     * @param kind ENABLE, MODE_SELECT or RELEASE. @param modeRaw MODE_SELECT only: 1, 3 or 6, judged by
     * the library. @param reads Budgets of the identity read, its widths included; the later reads use the
     * declared widths. @param options Copied budgets of the action, as its request requires; maximumAgeUs
     * also caps the reads' age. @param nowUs Supplied microseconds. @param deadlineUs Exclusive cutoff of
     * the reads and the action together.
     * @return Local status, unchanged on refusal: refused while a report is named (actionReportNamed), a
     * result is retained or the session is busy. Success retains an ACTIVE result and admits nothing yet. */
    MotorControlCAN::Status startAction(MotionKind kind, uint8_t modeRaw,
                                        const MotorControlCAN::LICHUAN_C::ReadOptions& reads,
                                        const MotorControlCAN::LICHUAN_C::ActionOptions& options, uint64_t nowUs,
                                        uint64_t deadlineUs) noexcept;
    /** @return True while setActionPrerequisites or setEnablePrerequisites names a report not yet consumed:
     * the console then uses the request of that kind, not startAction. */
    bool actionReportNamed() const noexcept { return actionState_ || enableState_; }
    /** @return True while startAction's reads run, before the action's native context exists. */
    bool preparing() const noexcept { return preparing_; }
    /** Dispatch explicit alarm clear, retaining an unresolved value-1 effect. Uses
     * AxisSession::requestClearAlarm's default method, AlarmClearMethod::CONTROLWORD_BIT7 (6040 0x0080,
     * then 0x0000); no console grammar selects OBJECT_2301.
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
     * A finite velocity is never withdrawn: its own stop at the due time is its only software bound,
     * and withdrawing it would remove that stop while the run may already turn. Use requestStop.
     * @param nowUs Supplied microseconds.
     * @return False unchanged for invalid time, inactive work, an active velocity or replaced native
     * identity.
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
    /** @param id Stable application helper identity. @return Retained original/replacement result,
     * or null for a foreign/released identity; no service or observation is performed. */
    const MotionResult* normalResult(uint32_t id) const noexcept;
    /** @param kind Stop selection. @return True when a requested stop of this kind is retained. */
    bool stopRetained(MotorControlCAN::LICHUAN_C::StopKind kind) const noexcept;
    /** @param kind Stop selection.
     * @return Retained stop or null for invalid/unretained kind; view ends on releaseStop. */
    const MotionStopResult* stopResult(MotorControlCAN::LICHUAN_C::StopKind kind) const noexcept;
    /** Release terminal ordinary history only; native contexts/fences remain untouched.
     * @return False unchanged without a retained terminal. */
    bool releaseNormal() noexcept;
    /** Release one named result; the original must be released before its replacement.
     * @param id Stable helper identity. @return False unchanged while active, foreign or out of order. */
    bool releaseNormal(uint32_t id) noexcept;
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
    MotionResult originalMove_;
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
                                                          *homeIdentity_ = nullptr,
                                                          *enableIdentity_ = nullptr,
                                                          *actionIdentity_ = nullptr;
    const MotorControlCAN::LICHUAN_C::ReadContext* actionState_ = nullptr;
    MotorControlCAN::LICHUAN_C::ControlObservation actionLoop_;
    bool actionLoopPresent_ = false;
    MotorControlCAN::LICHUAN_C::ActionOptions actionOptions_;
    const MotorControlCAN::LICHUAN_C::DriverContext *stored_ = nullptr, *inputs_ = nullptr,
                                                    *driver_ = nullptr;
    MotorControlCAN::LICHUAN_C::MoveEvidence moveEvidence_;
    MotorControlCAN::LICHUAN_C::MoveReplacementEvidence moveReplacementEvidence_;
    MotorControlCAN::LICHUAN_C::MoveFeedbackRelation moveFeedback_;
    bool moveFeedbackPresent_ = false;
    MotorControlCAN::LICHUAN_C::MoveProfile moveProfile_;
    MotorControlCAN::LICHUAN_C::MoveOptions moveOptions_;
    MotorControlCAN::LICHUAN_C::VelocityEvidence velocityEvidence_;
    MotorControlCAN::LICHUAN_C::VelocityChangeEvidence velocityChangeEvidence_;
    MotorControlCAN::LICHUAN_C::VelocityProfile velocityProfile_;
    MotorControlCAN::LICHUAN_C::VelocityOptions velocityOptions_;
    MotorControlCAN::LICHUAN_C::HomeEvidence homeEvidence_;
    MotorControlCAN::LICHUAN_C::HomeOptions homeOptions_;
    MotorControlCAN::LICHUAN_C::EnableEvidence enableEvidence_;
    MotorControlCAN::LICHUAN_C::ActionOptions enableOptions_, clearOptions_;
    StopPolicy stopPolicy_;
    AxisRecords* records_ = nullptr;
    bool issuedAttended_ = false; // The latest admitted request's attended experiment, for its result.
    uint32_t issuedRecord_ = 0;   // The latest admitted request's bound record id, zero for none.
    /** startAction's reads, one context reused in turn: the identity, CONTROL, then the state it judges. */
    MotorControlCAN::LICHUAN_C::DriverContext ownRead_;
    MotorControlCAN::LICHUAN_C::IdentityObservation ownIdentity_;
    MotorControlCAN::LICHUAN_C::ReadOptions plannedReads_;
    MotorControlCAN::LICHUAN_C::ActionOptions plannedOptions_;
    uint64_t plannedDeadlineUs_ = 0;
    MotionKind plannedKind_ = MotionKind::NONE;
    uint8_t plannedMode_ = 0;
    uint8_t prepareStep_ = 0;
    bool preparing_ = false;
    bool readPending_ = false;
    bool prepareWithdrawn_ = false;
    bool timeValid(uint64_t nowUs) const noexcept;
    bool recordCurrent(const RecordEntry& entry) const noexcept;
    MotorControlCAN::Status boundRecord(RecordSlot slot, const MotorControlCAN::LICHUAN_C::MeasuredRecord*& record,
                                        uint32_t& id) const noexcept;
    void markEvidence() noexcept;
    MotorControlCAN::Status store(const MotorControlCAN::LICHUAN_C::MeasuredRecord& record,
                                  RecordSlot& slot) noexcept;
    bool graph() const noexcept;
    MotorControlCAN::Status prerequisite() const noexcept;
    MotorControlCAN::Status admission(uint64_t nowUs, uint64_t deadlineUs) const noexcept;
    MotorControlCAN::Status hostChange(uint64_t nowUs) const noexcept;
    void forgetPrerequisites() noexcept;
    void followCoordinates() noexcept;
    void accept(MotionKind kind, uint32_t nativeOperation, uint64_t nowUs) noexcept;
    MotorControlCAN::Status issueAction(MotionKind kind, uint8_t modeRaw, uint64_t nowUs,
                                        uint64_t deadlineUs) noexcept;
    MotorControlCAN::Status startPreparationRead(uint64_t nowUs) noexcept;
    void servicePreparation(uint64_t nowUs) noexcept;
    void endPreparation(MotorControlCAN::ActionOutcome outcome, const MotorControlCAN::Status& status,
                        uint64_t nowUs) noexcept;
    void snapshotNormal() noexcept;
    void snapshotStops() noexcept;
};
} // namespace MotorControlCANExample
