/** @file ProbeApp.h
 * @brief Explicit native read application over the sole existing CAN owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "AxisSession.h"
#include "CanBusOwner.h"
#include "ProbePlatform.h"
#include <MotorControlCAN/Axis.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>

namespace MotorControlCANExample {
class ProbeDevice;
using MotorControlCANOwner::CanBusOwner;
using MotorControlCANOwner::Health;
using MotorControlCANOwner::RequestId;
using MotorControlCANOwner::SequenceId;
/** Local coordinate command; never admits owner work or changes a drive. */
enum class HostAxisCommandKind : uint8_t {
    QUERY,     ///< Inspect cached offline interpretation.
    CONFIGURE, ///< Reconstruct explicit offline coordinate facts.
    ORIGIN,    ///< Establish host zero from supplied stationary evidence.
    PREPARE    ///< Preview target arithmetic without admitting motion.
};
/** Explicit host facts supported by the common AxisConfig. */
enum class HostAxisField : uint8_t {
    COMMAND_SCALE,    ///< Command increments per motor turn.
    GEAR,             ///< Motor turns per load turn.
    FULL_STEP_SCALE,  ///< Optional full motor steps per motor turn.
    LEAD,             ///< Millimetres of load travel per load turn.
    ENCODER_SCALE,    ///< Selected encoder counts per configured feedback basis.
    ENCODER_ID,       ///< Application identity of the feedback source.
    ENCODER_BASIS,    ///< Motor turn, load turn or millimetre feedback basis.
    ENCODER_POLARITY, ///< Feedback polarity, exactly +1 or -1.
    POLARITY,         ///< Command polarity, exactly +1 or -1.
    NATIVE_LIMITS,    ///< Inclusive absolute command-coordinate bounds.
    SOFT_LIMITS,      ///< Optional inclusive absolute command-coordinate bounds.
    RELATIVE_BASES    ///< Explicit actual/commanded/queued witness-policy bitmask.
};
/** Copied exact local intent. Unused fields do not affect a command. */
struct HostAxisCommand {
    HostAxisCommandKind kind = HostAxisCommandKind::QUERY; ///< Selected operation.
    HostAxisField field = HostAxisField::COMMAND_SCALE;    ///< CONFIGURE fact.
    MotorControlCAN::Rational value;       ///< Scale, integer setting or exact native origin.
    MotorControlCAN::Rational secondValue; ///< Inclusive maximum for a limits field.
    bool clear = false;                    ///< Clear optional scale or software limits only.
    MotorControlCAN::EncoderBasis encoderBasis = MotorControlCAN::EncoderBasis::
        MOTOR_TURN;                            ///< ENCODER_BASIS selection, not an inferred scale.
    MotorControlCAN::PositionRequest position; ///< PREPARE arithmetic intent.
};
/** Owned output, published only on successful local dispatch. */
struct HostAxisView {
    MotorControlCAN::AxisConfig configuration; ///< Active only when configured.
    MotorControlCAN::PreparedTarget prepared;  ///< Active for successful PREPARE.
    bool configured = false;                   ///< Explicit host interpretation exists.
    bool offline = true;         ///< This workspace is never the selected live binding.
    bool referenceKnown = false; ///< Stored qualified native witness; not a freshness claim.
};
/** Explicit application interface facts; unknown fields are not zero-valued evidence. */
struct HostInterfaceFacts {
    bool interfaceKnown = false;       ///< Qualifies interfaceId.
    uint32_t interfaceId = 0;          ///< Application physical-interface identity.
    bool bitrateKnown = false;         ///< Qualifies bitrateBitsPerSecond.
    uint32_t bitrateBitsPerSecond = 0; ///< Supplied controller configuration, never changed here.
};
/** Host wiring declaration only; no terminal function, electrical state or model claim. */
enum class HostWiringUse : uint8_t {
    UNKNOWN,  ///< No host wiring declaration exists.
    UNUSED,   ///< Application declares the host label unconnected.
    CONNECTED ///< Application declares a connection, without electrical qualification.
};
/** One selected target and one finite typed read. No startup request, command
 * parser, firmware startup, discovery, motion or retry engine. The application
 * owns read stepping; the owner alone receives and retains service terminals.
 * One cooperative caller serializes mutation, including platform callbacks.
 */
class ProbeApp {
  public:
    /** Borrow stationary resources; construction performs no I/O or selection.
     * @param[in] owner Sole CAN receive/scheduling owner, outlives this application.
     * @param[in] platform Bounded byte/time facility, outlives this application.
     */
    ProbeApp(CanBusOwner& owner, ProbePlatform& platform) noexcept;
    /** Bind the one optional application device helper, without traffic.
     * @param device Stationary same-owner helper. Existing different attachment is refused.
     * @return True for the sole attachment; borrowed until app destruction. */
    bool attachDevice(ProbeDevice& device) noexcept;
    /** @param owner Expected sole owner. @return True only for this stationary owner graph. */
    bool usesOwner(const CanBusOwner& owner) const noexcept { return &owner == &owner_; }
    ProbeApp(const ProbeApp&) = delete;            ///< No copying live reservation/result handles.
    ProbeApp& operator=(const ProbeApp&) = delete; ///< Stationary application storage.
    /** @return Borrowed offline interpretation, null before explicit initialization. */
    const MotorControlCAN::AxisConfig* axisConfig() const noexcept;
    /** @return Borrowed stored witness, null before a qualified native relation is supplied. */
    const MotorControlCAN::AxisReference* axisReference() const noexcept;
    /** Establish an explicit offline arithmetic workspace without geometry or position evidence.
     * The synthetic identity never adopts selected(), reserves work or enters a live session.
     * @return Local validation status; unchanged if already initialized.
     */
    MotorControlCAN::Status initializeOfflineAxis() noexcept;
    /** Copy an initial explicit host interpretation, isolated from all live bindings.
     * @param config Valid caller facts; origins/soft limits must be unknown.
     * @return Local status; atomic failure, including an already established workspace.
     */
    MotorControlCAN::Status establishHostAxis(const MotorControlCAN::AxisConfig& config) noexcept;
    /** Checked core reconfiguration with caller-qualified evidence; never synthesizes standstill.
     * @param candidate Desired facts with current epochs. @param evidence Current witness.
     * @return configureAxis status; configuration and retained witness atomic on failure.
     */
    MotorControlCAN::Status
    configureHostAxis(const MotorControlCAN::AxisConfig& candidate,
                      const MotorControlCAN::AxisReference& evidence) noexcept;
    /** Retain a checked caller position witness with its original observation time.
     * @param reference Matching current native relation; supplied time/age retained unchanged.
     * @return Core validation status; no inference of idle, stationary or physical evidence.
     */
    MotorControlCAN::Status
    setHostAxisReference(const MotorControlCAN::AxisReference& reference) noexcept;
    /** Establish a host zero using the ordinary checked core API.
     * @param native Exact native coordinate of zero. @param evidence Qualified stationary witness.
     * @return Core status; atomic failure, no device-counter change.
     */
    MotorControlCAN::Status
    setHostAxisOrigin(int64_t native, const MotorControlCAN::AxisReference& evidence) noexcept;
    /** Pure target arithmetic with stored evidence only when required.
     * @param request Exact intent, including current configuration generation.
     * @param nowUs Supplied preparation microseconds, no observation-time renewal.
     * @param output Owned target, unchanged on failure. @return preparePosition status.
     */
    MotorControlCAN::Status previewPosition(const MotorControlCAN::PositionRequest& request,
                                            uint64_t nowUs,
                                            MotorControlCAN::PreparedTarget& output) const noexcept;
    /** Dispatch host commands. CONFIGURE edits an offline workspace by explicit reconstruction;
     * it cannot establish origin, reference or drive applicability. ORIGIN requires a supplied
     * stationary witness. PREPARE fills the current generation, never admits motion.
     * @param command Copied local intent. @param nowUs Supplied nondecreasing us for evidence.
     * @param output Owned snapshot, unchanged on failure. @return Local/core status.
     */
    MotorControlCAN::Status executeHostAxis(const HostAxisCommand& command, uint64_t nowUs,
                                            HostAxisView& output) noexcept;
    /** @param facts Explicit firmware/application facts; no controller configuration occurs.
     * @return False unchanged for known zero bitrate; unknown payload fields are ignored.
     */
    bool setHostInterfaceFacts(const HostInterfaceFacts& facts) noexcept;
    /** @return Cached application facts, with explicit unknown flags; no I/O. */
    HostInterfaceFacts hostInterfaceFacts() const noexcept { return hostInterface_; }
    /** Host labels di0..di6 use slots 0..6; do0..do2 use slots 7..9.
     * @param terminal Finite label slot, below 10. @param use Explicit host declaration.
     * @return False unchanged for invalid slot/value. No model/electrical claim or I/O.
     */
    bool setHostWiring(uint8_t terminal, HostWiringUse use) noexcept;
    /** @param terminal Slot below 10. @param output Declaration, unchanged for invalid slot.
     * @return True for a valid host label, independent of selected model applicability.
     */
    bool hostWiring(uint8_t terminal, HostWiringUse& output) const noexcept;
    /** Select explicit configured route facts without device traffic.
     * @param[in] target Copied valid channel/node epoch; no presence/model claim.
     * @return False unchanged while a read is retained or for invalid/unknown binding.
     * Channel currentness and session eligibility are additionally checked at admission.
     */
    bool select(const MotorControlCAN::LICHUAN_C::ReadTarget& target) noexcept;
    /** @return Borrowed selection until successful select/destruction; null before selection. */
    const MotorControlCAN::LICHUAN_C::ReadTarget* selected() const noexcept;
    /** Explicitly prepare finite raw identity reads, without transmitting.
     * @param[in] nowUs Nondecreasing supplied monotonic us; zero valid.
     * @param[in] deadlineUs Exclusive immutable overall cutoff, greater than nowUs.
     * @param[in] options Copied nonzero queue/response budgets; not vendor timing.
     * @return False unchanged for no selection, retained read, stale reservation,
     * invalid budgets or exhausted operation IDs. No bootstrap permission is implied.
     */
    bool requestIdentity(uint64_t nowUs, uint64_t deadlineUs,
                         const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** Explicit minimal 1000 upload, not an identity sweep or non-changing-read assertion.
     * @param nowUs As requestIdentity. @param deadlineUs As requestIdentity.
     * @param options As requestIdentity. @return Same preparation/retention contract.
     */
    bool requestProbe(uint64_t nowUs, uint64_t deadlineUs,
                      const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** Explicitly prepare six raw communication configuration uploads, never writes.
     * @param[in] nowUs Same monotonic time domain as requestIdentity.
     * @param[in] deadlineUs Exclusive future overall cutoff.
     * @param[in] options Copied nonzero queue/response budgets.
     * @return Same atomic local-preparation contract as requestIdentity.
     */
    bool requestConfig(uint64_t nowUs, uint64_t deadlineUs,
                       const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** Prepare explicit raw state blocks, with no status dialect or harmless-read inference.
     * @param nowUs Same supplied monotonic microseconds.
     * @param deadlineUs Exclusive immutable future cutoff.
     * @param options Copied nonzero queue/response budgets.
     * @param blockMask Nonzero StateBlock bits 0..5, no implicit selection.
     * @return Same atomic preparation contract as requestIdentity; no traffic yet.
     */
    bool requestState(uint64_t nowUs, uint64_t deadlineUs,
                      const MotorControlCAN::LICHUAN_C::ReadOptions& options,
                      uint8_t blockMask) noexcept;
    /** Prepare an ordinary finite read after checking every field against DEC-1.
     * @param kind PROBE, IDENTITY, CONFIG, STATE or a typed native driver read kind.
     * @param nowUs Nondecreasing supplied monotonic us, zero valid.
     * @param deadlineUs Exclusive immutable future cutoff.
     * @param options Explicit queue/response budgets and applicable width policy.
     * @param stateMask Nonzero StateBlock bits for STATE, zero for every other kind.
     * @return Local status. UNSUPPORTED/READ_EFFECT means Q20 refuses the complete plan,
     * before any reservation, operation or read publication. All refusal paths preserve
     * the retained read and admit no work. Accepted effects remain unknown; no retry.
     * Reviewed experiment callers retain the separate requestIdentity/config/state path.
     */
    MotorControlCAN::Status
    requestOrdinaryRead(MotorControlCAN::LICHUAN_C::ReadKind kind, uint64_t nowUs,
                        uint64_t deadlineUs, const MotorControlCAN::LICHUAN_C::ReadOptions& options,
                        uint8_t stateMask = 0) noexcept;
    /** Cancel the current read locally, retaining any pending owner terminal for advanceRead.
     * @param nowUs Nondecreasing supplied microseconds; earlier terminal/deadline wins.
     * @return False for no active read or invalid time; true performs no receive/new TX.
     * Between fields, settles the typed context directly. After possible TX, the exact
     * pending service settles normally, then cancellation withdraws any remaining fields.
     * No later field can be admitted after withdrawal; this never means motor stop.
     */
    bool cancelRead(uint64_t nowUs) noexcept;
    /** Interrupt an acquisition's current native service as well as withdrawing later fields.
     * Unlike ordinary console withdrawal, this deliberately settles possible TX as uncertain
     * and can quarantine its channel. It is used by explicit diagnostic experiment interruption.
     * @param nowUs Nondecreasing supplied us. @return False unchanged without active read or
     * on invalid time; retained native terminal/deadline wins. Never a motor stop or retry. */
    bool interruptRead(uint64_t nowUs) noexcept;
    /** Run owner first with fixed nonzero budgets, then bounded platform I/O.
     * @param[in] nowUs Supplied nondecreasing us; callers may acquire it via platform.nowUs.
     * @return False for backward time/owner rejection, with no platform callbacks.
     * At most one new read field is admitted per turn. Full result capacity waits
     * within the original deadline; no already-admitted work is replayed. A pending
     * terminal is never consumed here. Output pressure never gates owner service.
     */
    bool service(uint64_t nowUs) noexcept;
    /** Service with explicit owner work bounds instead of the default nonzero budgets.
     * @param nowUs Supplied nondecreasing microseconds.
     * @param budget Caller work bounds, clamped/validated by the same owner; zero work stays zero.
     * @return Same service result/retention contract; at most one read field is admitted.
     */
    bool service(uint64_t nowUs, const MotorControlCANOwner::ServiceBudget& budget) noexcept;
    /** Open one axis's session: latest controlword and stop work. An active read's
     * reservation passes to the session, and later reads use the session's.
     * @param[in] session Caller-owned closed session, borrowed until closed. Sessions are
     * large, so an application that sends no commands stores none.
     * @param[in] target Current route and node epoch.
     * @param[in] model Applicable model, supplied from application evidence.
     * @param[in] nowUs Supplied nondecreasing us.
     * @return The session, or null when two are already open, this binding has one, or
     * the target is stale.
     */
    AxisSession* openAxis(AxisSession& session,
                          const MotorControlCAN::LICHUAN_C::ReadTarget& target,
                          MotorControlCAN::LICHUAN_C::SettingsModel model, uint64_t nowUs) noexcept;
    /** @param[in] binding Exact axis binding.
     * @return Open session of that binding, or null. Sessions advance in service.
     */
    AxisSession* axis(const MotorControlCAN::Binding& binding) noexcept;
    /** @return Current owner request handle by value; default invalid between fields. */
    RequestId pendingRequest() const noexcept { return pending_; }
    /** Explicitly consume an inspected pending terminal and release its owner slot.
     * @param[in] nowUs Same nondecreasing us, at least terminal delivery time.
     * @return True advances the typed context, including failure, then releases the
     * service result. False leaves it retained. Borrowed owner result pointers expire
     * on success; context-owned raw evidence remains until releaseRead.
     */
    bool advanceRead(uint64_t nowUs) noexcept;
    /** @return Borrowed concrete context/history until next advance/release/destruction.
     * Use checked typed getters below for current observations; history is not freshness.
     */
    const MotorControlCAN::LICHUAN_C::ReadContext& readContext() const noexcept {
        return driverRead_.read;
    }
    /** Publish current successful raw identity, not model applicability.
     * @param[in] nowUs Nondecreasing supplied us.
     * @param[in] maximumAgeUs Nonzero inclusive per-field age.
     * @param[out] output Owned copy, unchanged for incomplete/wrong/stale read.
     * @return True only with a still-current reservation and checked typed evidence.
     */
    bool getIdentity(uint64_t nowUs, uint64_t maximumAgeUs,
                     MotorControlCAN::LICHUAN_C::IdentityObservation& output) const noexcept;
    /** Publish current successful minimal presence, not model/firmware qualification.
     * @param nowUs Supplied nondecreasing us. @param maximumAgeUs Nonzero original-observation age.
     * @param output Owned result unchanged on failure. @return Checked current PROBE only; no I/O.
     */
    bool getProbe(uint64_t nowUs, uint64_t maximumAgeUs,
                  MotorControlCAN::LICHUAN_C::ProbeObservation& output) const noexcept;
    /** Publish current six-field raw configuration, without adopting routes/timing.
     * @param[in] nowUs Nondecreasing supplied us.
     * @param[in] maximumAgeUs Nonzero inclusive per-field age.
     * @param[out] output Owned copy, unchanged on wrong/incomplete/stale context.
     * @return True only for checked current CONFIG evidence.
     */
    bool getConfig(uint64_t nowUs, uint64_t maximumAgeUs,
                   MotorControlCAN::LICHUAN_C::ConfigObservation& output) const noexcept;
    /** Publish one complete current raw state block without interpreted motion semantics.
     * @param block Exact selected StateBlock.
     * @param nowUs Supplied nondecreasing microseconds.
     * @param maximumAgeUs Nonzero inclusive per-field age.
     * @param output Owned result, unchanged on incomplete/stale/wrong evidence.
     * @return True only for checked typed evidence and a still-current reservation.
     */
    bool getStateBlock(MotorControlCAN::LICHUAN_C::StateBlock block, uint64_t nowUs,
                       uint64_t maximumAgeUs,
                       MotorControlCAN::LICHUAN_C::StateObservation& output) const noexcept;
    /** Publish a complete current native driver group; no units or applicability are inferred.
     * @param nowUs Supplied nondecreasing us. @param maximumAgeUs Nonzero original-evidence age.
     * @param output Owned typed observation, unchanged on refusal.
     * @return True only for a successful matching group under the still-current reservation.
     */
    bool getDriver(uint64_t nowUs, uint64_t maximumAgeUs,
                   MotorControlCAN::LICHUAN_C::DriverObservation& output) const noexcept;
    /** Publish a complete current I/O group, without electrical or terminal-layout inference.
     * @param nowUs Supplied nondecreasing us. @param maximumAgeUs Nonzero evidence age.
     * @param output Owned observation, unchanged on refusal. @return Checked current I/O only.
     */
    bool getIo(uint64_t nowUs, uint64_t maximumAgeUs,
               MotorControlCAN::LICHUAN_C::IoObservation& output) const noexcept;
    /** Publish complete current raw control words, without physical loop/encoder claims.
     * @param nowUs Supplied nondecreasing us. @param maximumAgeUs Nonzero evidence age.
     * @param output Owned observation, unchanged on refusal. @return Checked current CONTROL only.
     */
    bool getControl(uint64_t nowUs, uint64_t maximumAgeUs,
                    MotorControlCAN::LICHUAN_C::ControlObservation& output) const noexcept;
    /** Publish one complete current tuning group, retaining unknown ranges and units.
     * @param nowUs Supplied nondecreasing us. @param maximumAgeUs Nonzero evidence age.
     * @param output Owned observation, unchanged on refusal. @return Checked current tuning only.
     */
    bool getTuning(uint64_t nowUs, uint64_t maximumAgeUs,
                   MotorControlCAN::LICHUAN_C::TuningObservation& output) const noexcept;
    /** @param[out] output Cached selected-binding health; unchanged without a current binding.
     * @return Owner cached health result; no clock, I/O or service progress.
     */
    bool health(Health& output) const noexcept;
    /** Release a terminal read/reservation after inspection, never cancel active work.
     * @return True clears local history only when no pending request remains and
     * owner reservation release succeeds. A reservation held by an axis session stays
     * with the session. Does not clear quarantine or uncertainty.
     */
    bool releaseRead() noexcept;

  private:
    ProbeDevice* device_ = nullptr;
    CanBusOwner& owner_;
    ProbePlatform& platform_;
    MotorControlCAN::AxisConfig hostAxis_;
    MotorControlCAN::AxisReference hostReference_;
    HostInterfaceFacts hostInterface_;
    HostWiringUse hostWiring_[10] = {};
    bool hostAxisConfigured_ = false;
    AxisSession* axes_[2] = {nullptr, nullptr};
    MotorControlCAN::LICHUAN_C::ReadTarget target_;
    MotorControlCAN::LICHUAN_C::DriverContext driverRead_;
    SequenceId sequence_;
    RequestId pending_;
    uint64_t lastUs_ = 0;
    uint32_t operation_ = 0;
    bool selected_ = false;
    bool readOwnsSequence_ = false;
    bool readWithdrawn_ = false;
    MotorControlCAN::Status request(MotorControlCAN::LICHUAN_C::ReadKind kind, uint64_t nowUs,
                                    uint64_t deadlineUs,
                                    const MotorControlCAN::LICHUAN_C::ReadOptions& options,
                                    uint8_t mask = 0, bool ordinary = false) noexcept;
    bool current() const noexcept;
};
} // namespace MotorControlCANExample
