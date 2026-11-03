// SPDX-License-Identifier: MIT
#include "ReadPlan.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <new>

namespace MotorControlCAN {
namespace LICHUAN_C {
using namespace Detail;
namespace {
Status error(ReadError detail, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(detail), message);
}
// Write steps are field + 1, at most one per field `requested` holds. Readback steps follow after
// all of them, so no readback reuses a write's step identity within one operation.
constexpr uint32_t READBACK_STEP_OFFSET = sizeof(DriverSettingsContext::requested) / sizeof(uint32_t);
static_assert(READBACK_STEP_OFFSET == 9, "a readback step must not reuse a write step");
Binding expectedBinding(const DriverSettingsContext& c) {
    Binding binding = c.reservation.target.channel.binding;
    if (c.effectsPossible)
        ++binding.configurationGeneration;
    return binding;
}
bool settingsShape(const DriverSettingsContext& c) {
    const ReadContext& read = c.readback.read;
    const bool io = read.kind == ReadKind::IO_INPUT_CONFIGURATION;
    const bool control = read.kind == ReadKind::CONTROL;
    const bool selector = read.kind == ReadKind::DRIVER_SOFT_LIMITS;
    const GroupPlan* plan = groupPlan(read.kind);
    if ((!io && !control && !selector && read.kind != ReadKind::DRIVER_MOTION_PARAMETERS) || !plan ||
        read.count != plan->count)
        return false;
    const uint16_t allowed = io ? 0x1fe : control || selector ? 1 : 7;
    if ((c.changed & ~allowed) || (c.confirmed & ~c.changed) || c.cursor > read.count ||
        (c.state == SettingsState::WRITING &&
         (c.cursor >= read.count || !(c.changed & (1u << c.cursor)) ||
          (c.confirmed & (1u << c.cursor)))))
        return false;
    if (control && (c.requested[0] > 2 ||
                    (c.reservation.model == SettingsModel::OL86_C && c.requested[0] == 2)))
        return false;
    if (selector && c.requested[0] > 1)
        return false;
    // Prepared words are U16 and DI codes documented. High polarity bits have no bound to check.
    if (io && (c.requested[0] > UINT16_MAX || c.requested[1] > UINT16_MAX))
        return false;
    for (unsigned i = 2; io && i < read.count; ++i)
        if (c.requested[i] > 13)
            return false;
    for (unsigned i = 0; i < read.count; ++i) {
        const ReadDescriptor& actual = read.values[i].object;
        ReadDescriptor expected;
        if (!readDescriptor(plan->fields[i].index, plan->fields[i].subindex, expected) ||
            actual.index != expected.index || actual.subindex != expected.subindex ||
            actual.widthBytes != expected.widthBytes ||
            actual.representation != expected.representation ||
            actual.readEffect != expected.readEffect || actual.scope != expected.scope)
            return false;
    }
    return true;
}
Status current(const DriverSettingsContext& c, const AxisReservation& live,
               const Binding& binding, uint64_t now) {
    if (!sameReservation(c.reservation, live) || !sameBinding(binding, expectedBinding(c)))
        return error(ReadError::TARGET, "axis reservation or configuration changed");
    if (c.state == SettingsState::EMPTY || c.state > SettingsState::FAILED ||
        c.deadlineUs <= c.startedUs || now < c.servicedUs || !c.operation ||
        c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX ||
        !settingsShape(c))
        return error(ReadError::STATE, "invalid settings context or time");
    return Ok();
}
// A token keeps its frozen target. If that is no longer the actual one, its old evidence would still
// agree with it, and an unchanged candidate would report success with no write against a drive that
// may hold anything. The same comparison stops a token left from a finished or uncertain transaction.
Status validatePreparation(const AxisReservation& r, const ReadTarget& currentTarget,
                           const DriverContext& prior, ReadKind kind, uint32_t operation,
                           uint64_t now, uint64_t deadline, const ReadOptions& options,
                           uint64_t age) {
    if (validReservation(r) && !sameTarget(r.target, currentTarget))
        return error(ReadError::TARGET, "reservation is not at the actual current target");
    if (!validReservation(r) || r.acquiredUs > prior.read.startedUs || prior.read.startedUs > now || !age ||
        !operation || deadline <= now || !options.queueTimeoutUs || !options.responseTimeoutUs ||
        r.target.channel.binding.configurationGeneration == UINT32_MAX)
        return error(ReadError::ARGUMENT,
                     "invalid settings model, reservation, budgets or generation");
    return validateRead(kind, r.target, operation, now, deadline, options);
}
// Everything refusable has been checked; output is disjoint from every input.
void publish(DriverSettingsContext& c, const AxisReservation& r, DriverGroup group,
             uint32_t operation, uint64_t now, uint64_t deadline, const ReadOptions& options) {
    new (static_cast<void*>(&c)) DriverSettingsContext();
    c.reservation = r;
    c.operation = operation;
    c.startedUs = c.servicedUs = c.stepStartedUs = now;
    c.deadlineUs = deadline;
    c.state = SettingsState::WRITING;
    prepareDriverRead(c.readback, r.target, operation, now, deadline, options, group);
}
void select(DriverSettingsContext& c, const DriverContext& prior, unsigned first) {
    for (unsigned i = first; i < c.readback.read.count; ++i)
        if (c.requested[i] != prior.read.values[i].raw)
            c.changed |= uint16_t(1u << i);
    if (!c.changed) {
        c.readback = prior;
        c.state = SettingsState::SUCCEEDED;
        c.outcome = ReadOutcome::SUCCESS;
    } else {
        while (!(c.changed & (1u << c.cursor)))
            ++c.cursor;
    }
}
CANOPEN::SdoTransfer write(const DriverSettingsContext& c) {
    CANOPEN::SdoTransfer t;
    t.channel = c.reservation.target.channel;
    t.operation = t.request = c.operation;
    t.step = c.cursor + 1;
    const ReadDescriptor& object = c.readback.read.values[c.cursor].object;
    t.index = object.index;
    t.subindex = object.subindex;
    t.size = object.widthBytes;
    t.direction = CANOPEN::SdoDirection::DOWNLOAD;
    for (unsigned i = 0; i < 4 && i < t.size; ++i)
        t.data[i] = uint8_t(c.requested[c.cursor] >> (8 * i));
    t.deadlineUs = c.deadlineUs;
    t.queueTimeoutUs = c.readback.read.options.queueTimeoutUs;
    t.responseTimeoutUs = c.readback.read.options.responseTimeoutUs;
    return t;
}
void fail(DriverSettingsContext& c, ReadOutcome reason, uint64_t now, const char* message) {
    c.state = SettingsState::FAILED;
    c.outcome = reason;
    c.servicedUs = now;
    c.status =
        reason == ReadOutcome::PROTOCOL_REJECTED
            ? Status(Err::PROTOCOL_ERROR, 0, message)
            : error(reason == ReadOutcome::DEADLINE || reason == ReadOutcome::SERVICE_TIMEOUT
                        ? ReadError::TIME
                        : ReadError::STATE,
                    message);
}
bool active(const DriverSettingsContext& c) {
    return c.state == SettingsState::WRITING || c.state == SettingsState::READING_BACK;
}
// Settings wants 000F, no older than its other prerequisites. Its nonrunning report is separate.
bool controlEvidence(const CANOPEN::SdoCompletion& e, const AxisReservation& r, uint64_t now,
                     uint64_t age) {
    return acknowledgedControlWrite(e, r, now) && controlWord(e.transfer) == 0x000f &&
           now - e.admittedUs <= age;
}
uint32_t stateWord(const ReadContext& state, StateBlock block, unsigned field) {
    return state.values[state.blockStart[static_cast<unsigned>(block)] + field].raw;
}
// The same read validateStop demands. A settings call is never urgent, so a failed or uncertain
// sibling block costs one fresh read, not an admission from a window with a known service problem.
bool cleanState(const ReadContext& state, const AxisReservation& r, uint64_t now, uint64_t age,
                ReadWindow& status, ReadWindow& monitors) {
    return state.source == ReadSource::SDO && state.state == ReadState::SUCCEEDED &&
           !state.uncertain &&
           validateStateBlock(state, StateBlock::STATUS, r.target, now, age, status) &&
           validateStateBlock(state, StateBlock::MONITORS, r.target, now, age, monitors);
}
// Project policy for an object with no documented state: a clean read under this reservation, no
// running or alarm report and no undocumented bit. Bit 4 is deliberately unused: both specimens
// report it clear before any enable, so it does not say what its label says.
Status validateStopped(const AxisReservation& r, const ReadTarget& currentTarget,
                       const DriverContext& prior, ReadKind kind, const ReadContext& state,
                       uint32_t operation, uint64_t now, uint64_t deadline,
                       const ReadOptions& options, uint64_t age) {
    const Status s =
        validatePreparation(r, currentTarget, prior, kind, operation, now, deadline, options, age);
    if (!s)
        return s;
    ReadWindow status, monitors;
    if (!cleanState(state, r, now, age, status, monitors) || status.earliestUs < r.acquiredUs ||
        monitors.earliestUs < r.acquiredUs ||
        (stateWord(state, StateBlock::MONITORS, 0) &
         (MONITOR_RUNNING | MONITOR_ALARM | ~uint32_t(MONITOR_DOCUMENTED_BITS))))
        return error(ReadError::STATE,
                     "clean reserved state with no running, alarm or undocumented report required");
    return Ok();
}
} // namespace

Status prepareDriverSettings(DriverSettingsContext& output, const AxisReservation& r,
                             const ReadTarget& currentTarget, const DriverContext& prior,
                             const ReadContext& state,
                             const CANOPEN::SdoCompletion& control, const DriverSettings& settings,
                             uint32_t operation, uint64_t now, uint64_t deadline,
                             const ReadOptions& options, uint64_t age) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &currentTarget, sizeof currentTarget) ||
        overlaps(&output, sizeof output, &prior, sizeof prior) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &control, sizeof control) ||
        overlaps(&output, sizeof output, &settings, sizeof settings) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "settings preparation overlaps input");
    Status s = validatePreparation(r, currentTarget, prior, ReadKind::DRIVER_MOTION_PARAMETERS,
                                   operation, now, deadline, options, age);
    if (!s)
        return s;
    DriverObservation old;
    ReadWindow status, monitors;
    if (!getDriver(prior, r.target, now, age, old) || old.group != DriverGroup::MOTION_PARAMETERS ||
        !controlEvidence(control, r, now, age) || prior.read.startedUs < control.completedUs ||
        !cleanState(state, r, now, age, status, monitors) ||
        uint8_t(stateWord(state, StateBlock::STATUS, 1)) != 1 ||
        (stateWord(state, StateBlock::STATUS, 0) & 7u) != 7 ||
        (stateWord(state, StateBlock::MONITORS, 0) &
         (MONITOR_RUNNING | MONITOR_ALARM | MONITOR_RELEASED)) ||
        status.earliestUs < control.completedUs ||
        monitors.earliestUs < control.completedUs)
        return error(ReadError::STATE, "current reserved position pre-trigger evidence required");
    if (settings.positionSpeedRpm > 3000 || settings.rampUpTimeMs > 2000 ||
        settings.rampDownTimeMs > 2000)
        return error(ReadError::VALUE, "native speed/ramp candidate outside reviewed range");
    publish(output, r, DriverGroup::MOTION_PARAMETERS, operation, now, deadline, options);
    output.requested[0] = settings.positionSpeedRpm;
    output.requested[1] = settings.rampUpTimeMs;
    output.requested[2] = settings.rampDownTimeMs;
    select(output, prior, 0);
    return Ok();
}
Status prepareSoftLimitSettings(DriverSettingsContext& output, const AxisReservation& r,
                                const ReadTarget& currentTarget, const DriverContext& prior,
                                const ReadContext& state,
                                const SoftLimitSettings& settings, uint32_t operation, uint64_t now,
                                uint64_t deadline, const ReadOptions& options,
                                uint64_t age) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &currentTarget, sizeof currentTarget) ||
        overlaps(&output, sizeof output, &prior, sizeof prior) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &settings, sizeof settings) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "soft-limit settings preparation overlaps input");
    Status s = validateStopped(r, currentTarget, prior, ReadKind::DRIVER_SOFT_LIMITS, state, operation,
                               now, deadline, options, age);
    if (!s)
        return s;
    DriverObservation old;
    if (!getDriver(prior, r.target, now, age, old) || old.group != DriverGroup::SOFT_LIMITS ||
        old.softLimitSelectorRaw > 1)
        return error(ReadError::STATE, "fresh reserved read of a documented 2202 code required");
    const uint16_t code = static_cast<uint16_t>(settings.enable);
    if (code > 1 || (settings.enable == SoftLimitEnable::AFTER_HOMING &&
                     old.minimumNative >= old.maximumNative))
        return error(ReadError::VALUE, "2202 candidate undocumented or limits not ordered");
    // p18's "effective after homing" defers the effect. Once 2100 reports homing complete nothing
    // defers it, and an immediate limit in an unqualified frame is not what the candidate names.
    if (settings.enable == SoftLimitEnable::AFTER_HOMING &&
        (stateWord(state, StateBlock::MONITORS, 0) & MONITOR_HOMING_COMPLETE))
        return error(ReadError::STATE, "2202 arming refused while 2100 reports homing complete");
    publish(output, r, DriverGroup::SOFT_LIMITS, operation, now, deadline, options);
    output.requested[0] = code;
    output.requested[1] = old.softLimitEntries;
    output.requested[2] = uint32_t(old.minimumNative);
    output.requested[3] = uint32_t(old.maximumNative);
    select(output, prior, 0);
    return Ok();
}
Status prepareIoSettings(DriverSettingsContext& output, const AxisReservation& r,
                         const ReadTarget& currentTarget, const DriverContext& prior,
                         const ReadContext& state, const IoSettings& settings, uint32_t operation,
                         uint64_t now, uint64_t deadline, const ReadOptions& options,
                         uint64_t age) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &currentTarget, sizeof currentTarget) ||
        overlaps(&output, sizeof output, &prior, sizeof prior) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &settings, sizeof settings) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "DI settings preparation overlaps input");
    // A reassignment can change enable, stop and limit inputs at once, and no source says when 2400
    // may be written, so it takes 2202's project policy. RS refuses every settings group, I/O
    // included, unless stationary with no alarm (validatePriorSettingsAndStandstill).
    Status s = validateStopped(r, currentTarget, prior, ReadKind::IO_INPUT_CONFIGURATION, state,
                               operation, now, deadline, options, age);
    if (!s)
        return s;
    IoObservation old;
    if (!getIo(prior, r.target, now, age, old) || old.group != DriverGroup::IO_INPUT_CONFIGURATION)
        return error(ReadError::STATE, "fresh reserved complete input configuration required");
    if (settings.invertedInputs & ~INPUT_POLARITY_BITS)
        return error(ReadError::VALUE, "DI polarity candidate contains undocumented bits");
    for (unsigned i = 0; i < 7; ++i)
        if (static_cast<uint16_t>(settings.functions[i]) > 13)
            return error(ReadError::VALUE, "undocumented DI function candidate");
    publish(output, r, DriverGroup::IO_INPUT_CONFIGURATION, operation, now, deadline, options);
    output.requested[0] = old.ioEntries;
    output.requested[1] = (old.inputPolarityRaw & ~INPUT_POLARITY_BITS) | settings.invertedInputs;
    for (unsigned i = 0; i < 7; ++i)
        output.requested[i + 2] = static_cast<uint16_t>(settings.functions[i]);
    select(output, prior, 1);
    return Ok();
}
Status prepareControlSettings(DriverSettingsContext& output, const AxisReservation& r,
                              const ReadTarget& currentTarget, const DriverContext& prior,
                              const ReadContext& state, const ControlSettings& settings,
                              uint32_t operation, uint64_t now, uint64_t deadline,
                              const ReadOptions& options, uint64_t age) noexcept {
    if (overlaps(&output, sizeof output, &r, sizeof r) ||
        overlaps(&output, sizeof output, &currentTarget, sizeof currentTarget) ||
        overlaps(&output, sizeof output, &prior, sizeof prior) ||
        overlaps(&output, sizeof output, &state, sizeof state) ||
        overlaps(&output, sizeof output, &settings, sizeof settings) ||
        overlaps(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "control settings preparation overlaps input");
    // The selector can change torque at once and no source says when it may be written, so it
    // takes 2202's project policy; RS refuses this feature unless stationary with no alarm.
    Status s = validateStopped(r, currentTarget, prior, ReadKind::CONTROL, state, operation, now,
                               deadline, options, age);
    if (!s)
        return s;
    ControlObservation old;
    if (!getControl(prior, r.target, now, age, old))
        return error(ReadError::STATE, "fresh reserved complete control observation required");
    const uint16_t mode = static_cast<uint16_t>(settings.loopMode);
    if (mode > 2 ||
        (r.model == SettingsModel::OL86_C && settings.loopMode == LoopMode::CLOSED_LOOP))
        return error(ReadError::VALUE, "loop selector or model lacks reviewed applicability");
    // Project policy, not a vendor rule: whatever 2501 counts, zero of them cannot scale feedback.
    if (settings.loopMode == LoopMode::CLOSED_LOOP && !old.encoderResolutionRaw)
        return error(ReadError::VALUE, "closed loop refused while 2501 reads zero");
    publish(output, r, DriverGroup::CONTROL, operation, now, deadline, options);
    output.requested[0] = mode;
    output.requested[1] = old.encoderResolutionRaw;
    select(output, prior, 0);
    return Ok();
}
Status nextDriver(const DriverSettingsContext& c, const AxisReservation& live,
                  const Binding& binding, uint64_t now, PreparedRead& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &binding, sizeof binding))
        return error(ReadError::OVERLAP, "settings work overlaps input");
    Status s = current(c, live, binding, now);
    if (!s)
        return s;
    if (active(c) && now >= c.deadlineUs)
        return error(ReadError::TIME, "settings overall deadline");
    if (!active(c)) {
        output.ready = false;
        return Ok();
    }
    PreparedRead candidate;
    if (c.state == SettingsState::READING_BACK) {
        s = nextDriver(c.readback, now, candidate);
        if (!s)
            return s;
        candidate.transfer.step += READBACK_STEP_OFFSET;
    } else {
        if (c.cursor >= c.readback.read.count || !(c.changed & (1u << c.cursor)))
            return error(ReadError::STATE, "invalid settings cursor");
        candidate.ready = true;
        candidate.transfer = write(c);
    }
    output = candidate;
    return Ok();
}
Status noteDriverTransmission(DriverSettingsContext& c, const AxisReservation& live,
                              Binding& binding, uint64_t now) noexcept {
    if (overlaps(&binding, sizeof binding, &c, sizeof c) ||
        overlaps(&binding, sizeof binding, &live, sizeof live) ||
        overlaps(&c, sizeof c, &live, sizeof live))
        return error(ReadError::OVERLAP, "settings TX notice overlaps input");
    Status s = current(c, live, binding, now);
    if (!s)
        return s;
    if (c.state != SettingsState::WRITING || c.transmissionNoted || now >= c.deadlineUs)
        return error(ReadError::STATE, "no new live settings download");
    if (!c.effectsPossible)
        ++binding.configurationGeneration;
    c.effectsPossible = c.transmissionNoted = true;
    c.servicedUs = now;
    return Ok();
}
Status advanceDriver(DriverSettingsContext& c, const AxisReservation& live,
                     const Binding& binding, const CANOPEN::SdoCompletion& input,
                     uint64_t now) noexcept {
    if (overlaps(&c, sizeof c, &input, sizeof input) || overlaps(&c, sizeof c, &live, sizeof live) ||
        overlaps(&c, sizeof c, &binding, sizeof binding))
        return error(ReadError::OVERLAP, "settings terminal overlaps input");
    Status s = current(c, live, binding, now);
    if (!s)
        return s;
    if (!active(c))
        return error(ReadError::STATE, "settings transaction is terminal");
    PreparedRead prepared;
    // Correlation is checked before deadline settlement, as in the read lifecycle.
    s = nextDriver(c, live, binding, c.servicedUs, prepared);
    if (!s)
        return s;
    if (!CANOPEN::sameSdoWork(input.transfer, prepared.transfer) ||
        input.nodeGeneration != c.reservation.target.nodeGeneration ||
        input.admittedUs < c.stepStartedUs || input.admittedUs >= c.deadlineUs ||
        input.completedUs < input.admittedUs || input.completedUs < c.servicedUs ||
        input.completedUs > now)
        return error(ReadError::CORRELATION, "settings terminal does not match live work");
    if (c.state == SettingsState::READING_BACK) {
        CANOPEN::SdoCompletion local = input;
        local.transfer.step -= READBACK_STEP_OFFSET;
        const unsigned field = c.readback.read.cursor;
        // Atomic on c.readback, and nothing after it can refuse.
        s = advanceDriver(c.readback, local, now);
        if (!s)
            return s;
        c.readback.read.evidence = input; // Retain the actual external step identity.
        c.uncertain |= c.readback.read.uncertain;
        if (c.readback.read.state == ReadState::FAILED)
            fail(c, c.readback.read.outcome, now, "settings readback failed");
        else {
            // Every field, including retained words and the 2400:00 count no write can touch.
            if (c.readback.read.values[field].raw != c.requested[field])
                fail(c, ReadOutcome::REPLY_ERROR, now, "settings readback contradicts candidate");
            else if (c.readback.read.state == ReadState::SUCCEEDED) {
                c.state = SettingsState::SUCCEEDED;
                c.outcome = ReadOutcome::SUCCESS;
                c.status = Ok();
            }
        }
    } else {
        const bool success = input.outcome == CANOPEN::SdoOutcome::SUCCESS;
        if (!CANOPEN::checkDownloadTerminal(input, c.transmissionNoted))
            return error(ReadError::CORRELATION, "inconsistent settings service evidence");
        const bool expired = now >= c.deadlineUs || input.completedUs >= c.deadlineUs;
        unsigned nextCursor = c.cursor + 1u;
        while (nextCursor < c.readback.read.count && !(c.changed & (1u << nextCursor)))
            ++nextCursor;
        const bool lastWrite = success && !expired && nextCursor == c.readback.read.count;
        if (lastWrite) {
            // The only refusal left. It replaces c.readback alone, atomically, before anything else.
            ReadTarget readTarget = c.reservation.target;
            readTarget.channel.binding = binding;
            const ReadOptions readOptions = c.readback.read.options; // Would overlap its output.
            const DriverGroup group = groupPlan(c.readback.read.kind)->group; // Shape checked it.
            s = prepareDriverRead(c.readback, readTarget, c.operation, now, c.deadlineUs,
                                  readOptions, group);
            if (!s)
                return s;
        }
        const unsigned cursor = c.cursor;
        const bool noted = c.transmissionNoted;
        c.uncertain |= input.effectUncertain || (noted && !success);
        if (success) {
            c.confirmed |= uint16_t(1u << cursor);
            c.acknowledgedUs[cursor] = input.completedUs;
            c.transmissionNoted = false;
        }
        if (expired)
            fail(c, ReadOutcome::DEADLINE, now, "settings overall deadline");
        else if (!success)
            fail(c, readOutcome(input.outcome), now, "settings download failed");
        else {
            c.stepStartedUs = now;
            c.cursor = uint8_t(nextCursor);
            if (lastWrite)
                c.state = SettingsState::READING_BACK;
        }
    }
    c.evidence = input;
    c.evidencePresent = true;
    c.servicedUs = now;
    return Ok();
}
Status advanceDriver(DriverSettingsContext& c, ReadOutcome reason, uint64_t now) noexcept {
    if (!active(c) || now < c.servicedUs ||
        (reason != ReadOutcome::CANCELLED && reason != ReadOutcome::DEADLINE &&
         reason != ReadOutcome::INVALIDATED) ||
        (reason == ReadOutcome::DEADLINE && now < c.deadlineUs))
        return error(ReadError::STATE, "invalid local settings termination");
    if (c.state == SettingsState::READING_BACK) {
        const Status s = advanceDriver(c.readback, reason, now);
        if (!s)
            return s;
        c.uncertain |= c.readback.read.uncertain;
    }
    c.uncertain |= c.transmissionNoted;
    if (now >= c.deadlineUs)
        reason = ReadOutcome::DEADLINE;
    fail(c, reason, now, "settings locally withdrawn; no rollback or motor stop");
    return Ok();
}
Status getDriver(const DriverSettingsContext& c, const AxisReservation& live,
                 const ReadTarget& target, uint64_t now, uint64_t age,
                 DriverObservation& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &target, sizeof target))
        return error(ReadError::OVERLAP, "settings result overlaps input");
    Status s = current(c, live, target.channel.binding, now);
    if (!s)
        return s;
    if (c.state != SettingsState::SUCCEEDED)
        return error(ReadError::STATE, "settings not complete and matched");
    return getDriver(c.readback, target, now, age, output);
}
Status getIo(const DriverSettingsContext& c, const AxisReservation& live,
             const ReadTarget& target, uint64_t now, uint64_t age, IoObservation& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &target, sizeof target))
        return error(ReadError::OVERLAP, "settings result overlaps input");
    Status s = current(c, live, target.channel.binding, now);
    if (!s)
        return s;
    if (c.state != SettingsState::SUCCEEDED)
        return error(ReadError::STATE, "settings not complete and matched");
    return getIo(c.readback, target, now, age, output);
}
Status getControl(const DriverSettingsContext& c, const AxisReservation& live,
                  const ReadTarget& target, uint64_t now, uint64_t age,
                  ControlObservation& output) noexcept {
    if (overlaps(&output, sizeof output, &c, sizeof c) ||
        overlaps(&output, sizeof output, &live, sizeof live) ||
        overlaps(&output, sizeof output, &target, sizeof target))
        return error(ReadError::OVERLAP, "control settings result overlaps input");
    Status s = current(c, live, target.channel.binding, now);
    if (!s)
        return s;
    if (c.state != SettingsState::SUCCEEDED)
        return error(ReadError::STATE, "control settings not complete and matched");
    return getControl(c.readback, target, now, age, output);
}
Status retireMotionParameters(DriverContext& retained, const ReadTarget& current,
                              const CANOPEN::SdoTransfer& work) noexcept {
    if (overlaps(&retained, sizeof retained, &current, sizeof current) ||
        overlaps(&retained, sizeof retained, &work, sizeof work))
        return error(ReadError::OVERLAP, "motion parameter retirement overlaps input");
    if (!current.nodeGeneration || !CANOPEN::validateSdoChannel(current.channel) ||
        !CANOPEN::sameSdoChannel(work.channel, current.channel))
        return error(ReadError::TARGET, "move write is not on the current route");
    // EDS widths. 607A and 60FF are targets outside this group, so they retire nothing. Homing's
    // writes are outside it too, and retire because prompt 36 requires it: their effect on the
    // group is unestablished.
    struct MoveWrite {
        uint16_t index;
        uint8_t subindex, size;
        bool retires;
    };
    static const MoveWrite writes[] = {{0x607a, 0, 4, false}, {0x60ff, 0, 4, false},
                                       {0x6081, 0, 4, true},  {0x6083, 0, 4, true},
                                       {0x6084, 0, 4, true},  {0x6098, 0, 1, true},
                                       {0x6099, 1, 4, true},  {0x6099, 2, 4, true},
                                       {0x609a, 0, 4, true},  {0x607c, 0, 4, true}};
    const MoveWrite* write = nullptr;
    for (const MoveWrite& candidate : writes)
        if (candidate.index == work.index && candidate.subindex == work.subindex &&
            candidate.size == work.size)
            write = &candidate;
    if (!write || work.direction != CANOPEN::SdoDirection::DOWNLOAD || !work.operation ||
        !work.request || !work.step)
        return error(ReadError::ARGUMENT, "not an operation's own motion-phase download");
    const ReadContext& read = retained.read;
    if (read.state == ReadState::EMPTY || read.kind != ReadKind::DRIVER_MOTION_PARAMETERS)
        return Ok();
    // Older generations of this axis count too: only another axis's observation is left alone.
    const Binding& a = read.target.channel.binding;
    const Binding& b = current.channel.binding;
    if (a.logicalAxisId != b.logicalAxisId || a.endpoint.bus != b.endpoint.bus ||
        a.endpoint.profile != b.endpoint.profile || a.endpoint.address != b.endpoint.address ||
        a.endpoint.channel != b.endpoint.channel || a.endpoint.subaxis != b.endpoint.subaxis ||
        a.endpoint.protocol != b.endpoint.protocol ||
        read.target.channel.clientToServerId != current.channel.clientToServerId ||
        read.target.channel.serverToClientId != current.channel.serverToClientId)
        return error(ReadError::TARGET, "retained observation belongs to another axis");
    if (write->retires)
        retained.read.invalidated = true;
    return Ok();
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
