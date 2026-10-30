// SPDX-License-Identifier: MIT
#include <MotorControlCAN/profiles/lichuan_c/Settings.h>
#include <MotorControlCAN/profiles/lichuan_c/Tuning.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
namespace {
Status error(ReadError detail, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(detail), message);
}
bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    const uintptr_t x = reinterpret_cast<uintptr_t>(a), y = reinterpret_cast<uintptr_t>(b);
    return x >= y ? x - y < bn : y - x < an;
}
bool sameTarget(const ReadTarget& a, const ReadTarget& b) {
    return sameBinding(a.channel.binding, b.channel.binding) &&
           a.nodeGeneration == b.nodeGeneration &&
           a.channel.clientToServerId == b.channel.clientToServerId &&
           a.channel.serverToClientId == b.channel.serverToClientId;
}
bool valid(const SettingsReservation& r) {
    return r.owner && r.sequence && r.generation && r.target.nodeGeneration &&
           (r.model == SettingsModel::CL86_C || r.model == SettingsModel::OL86_C) &&
           CANOPEN::validateSdoChannel(r.target.channel);
}
bool sameReservation(const SettingsReservation& a, const SettingsReservation& b) {
    return valid(a) && valid(b) && sameTarget(a.target, b.target) && a.model == b.model &&
           a.owner == b.owner && a.sequence == b.sequence && a.generation == b.generation &&
           a.acquiredUs == b.acquiredUs;
}
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
    if ((!io && !control && read.kind != ReadKind::DRIVER_MOTION_PARAMETERS) ||
        read.count != (io        ? 9
                       : control ? 2
                                 : 3))
        return false;
    const uint16_t allowed = io ? 0x1fe : control ? 1 : 7;
    if ((c.changed & ~allowed) || (c.confirmed & ~c.changed) || c.cursor > read.count ||
        (c.state == SettingsState::WRITING &&
         (c.cursor >= read.count || !(c.changed & (1u << c.cursor)) ||
          (c.confirmed & (1u << c.cursor)))))
        return false;
    if (control && (c.requested[0] > 2 ||
                    (c.reservation.model == SettingsModel::OL86_C && c.requested[0] == 2)))
        return false;
    const uint16_t motion[] = {0x6081, 0x6083, 0x6084};
    for (unsigned i = 0; i < read.count; ++i) {
        const ReadDescriptor& actual = read.values[i].object;
        ReadDescriptor expected;
        const uint16_t index = io ? 0x2400 : control ? uint16_t(0x2500 + i) : motion[i];
        if (!readDescriptor(index, io ? uint8_t(i) : 0, expected) ||
            actual.index != expected.index || actual.subindex != expected.subindex ||
            actual.widthBytes != expected.widthBytes ||
            actual.representation != expected.representation ||
            actual.readEffect != expected.readEffect || actual.scope != expected.scope)
            return false;
    }
    return true;
}
Status current(const DriverSettingsContext& c, const SettingsReservation& live,
               const Binding& binding, uint64_t now) {
    if (!sameReservation(c.reservation, live) || !sameBinding(binding, expectedBinding(c)))
        return error(ReadError::TARGET, "settings reservation or configuration changed");
    if (c.state == SettingsState::EMPTY || c.state > SettingsState::FAILED ||
        c.deadlineUs <= c.startedUs || now < c.servicedUs || !c.operation ||
        c.reservation.target.channel.binding.configurationGeneration == UINT32_MAX ||
        !settingsShape(c))
        return error(ReadError::STATE, "invalid settings context or time");
    return Ok();
}
Status preparation(DriverSettingsContext& c, const SettingsReservation& r,
                   const DriverContext& prior, DriverGroup group, uint32_t operation, uint64_t now,
                   uint64_t deadline, const ReadOptions& options, uint64_t age) {
    if (!valid(r) || r.acquiredUs > prior.read.startedUs || prior.read.startedUs > now || !age ||
        !operation || deadline <= now || !options.queueTimeoutUs || !options.responseTimeoutUs ||
        r.target.channel.binding.configurationGeneration == UINT32_MAX)
        return error(ReadError::ARGUMENT,
                     "invalid settings model, reservation, budgets or generation");
    c.reservation = r;
    c.operation = operation;
    c.startedUs = c.servicedUs = c.stepStartedUs = now;
    c.deadlineUs = deadline;
    c.state = SettingsState::WRITING;
    return prepareDriverRead(c.readback, r.target, operation, now, deadline, options, group);
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
bool sameWork(const CANOPEN::SdoTransfer& a, const CANOPEN::SdoTransfer& b) {
    if (!sameBinding(a.channel.binding, b.channel.binding) ||
        a.channel.clientToServerId != b.channel.clientToServerId ||
        a.channel.serverToClientId != b.channel.serverToClientId || a.operation != b.operation ||
        a.request != b.request || a.step != b.step || a.index != b.index ||
        a.subindex != b.subindex || a.direction != b.direction || a.size != b.size ||
        a.readEffect != b.readEffect || a.allowSegmentedUpload != b.allowSegmentedUpload ||
        a.deadlineUs != b.deadlineUs || a.queueTimeoutUs != b.queueTimeoutUs ||
        a.responseTimeoutUs != b.responseTimeoutUs)
        return false;
    if (a.direction == CANOPEN::SdoDirection::DOWNLOAD)
        for (unsigned i = 0; i < a.size; ++i)
            if (a.data[i] != b.data[i])
                return false;
    return true;
}
ReadOutcome outcome(CANOPEN::SdoOutcome o) {
    switch (o) {
    case CANOPEN::SdoOutcome::SERVER_ABORT:
        return ReadOutcome::PROTOCOL_REJECTED;
    case CANOPEN::SdoOutcome::TIMEOUT:
        return ReadOutcome::DEADLINE;
    case CANOPEN::SdoOutcome::CANCELLED:
        return ReadOutcome::CANCELLED;
    case CANOPEN::SdoOutcome::REBOOT:
    case CANOPEN::SdoOutcome::BINDING_INVALIDATED:
        return ReadOutcome::INVALIDATED;
    case CANOPEN::SdoOutcome::CLIENT_ABORT:
    case CANOPEN::SdoOutcome::UNSUPPORTED:
        return ReadOutcome::REPLY_ERROR;
    default:
        return ReadOutcome::TRANSPORT_ERROR;
    }
}
void fail(DriverSettingsContext& c, ReadOutcome reason, uint64_t now, const char* message) {
    c.state = SettingsState::FAILED;
    c.outcome = reason;
    c.servicedUs = now;
    c.status =
        reason == ReadOutcome::PROTOCOL_REJECTED
            ? Status(Err::PROTOCOL_ERROR, 0, message)
            : error(reason == ReadOutcome::DEADLINE ? ReadError::TIME : ReadError::STATE, message);
}
bool active(const DriverSettingsContext& c) {
    return c.state == SettingsState::WRITING || c.state == SettingsState::READING_BACK;
}
bool controlEvidence(const CANOPEN::SdoCompletion& e, const SettingsReservation& r, uint64_t now,
                     uint64_t age) {
    const CANOPEN::SdoTransfer& t = e.transfer;
    return sameBinding(t.channel.binding, r.target.channel.binding) &&
           t.channel.clientToServerId == r.target.channel.clientToServerId &&
           t.channel.serverToClientId == r.target.channel.serverToClientId &&
           e.nodeGeneration == r.target.nodeGeneration &&
           e.outcome == CANOPEN::SdoOutcome::SUCCESS &&
           t.direction == CANOPEN::SdoDirection::DOWNLOAD && t.index == 0x6040 && !t.subindex &&
           t.size == 2 && t.data[0] == 0x0f && !t.data[1] && t.operation && t.request && t.step &&
           t.deadlineUs > e.completedUs && t.queueTimeoutUs && t.responseTimeoutUs && e.queued &&
           e.transmissionPossible && e.protocolAcknowledged && e.effectPossible &&
           !e.effectUncertain && !e.abortValid && !e.size && r.acquiredUs <= e.admittedUs &&
           e.admittedUs <= e.completedUs && e.completedUs <= now && now - e.admittedUs <= age;
}
bool after(const StateObservation& o, uint64_t time) {
    for (unsigned i = 0; i < o.fieldCount; ++i)
        if (o.fields[i].earliestUs < time)
            return false;
    return true;
}
} // namespace

Status prepareDriverSettings(DriverSettingsContext& output, const SettingsReservation& r,
                             const DriverContext& prior, const ReadContext& state,
                             const CANOPEN::SdoCompletion& control, const DriverSettings& settings,
                             uint32_t operation, uint64_t now, uint64_t deadline,
                             const ReadOptions& options, uint64_t age) noexcept {
    if (overlap(&output, sizeof output, &r, sizeof r) ||
        overlap(&output, sizeof output, &prior, sizeof prior) ||
        overlap(&output, sizeof output, &state, sizeof state) ||
        overlap(&output, sizeof output, &control, sizeof control) ||
        overlap(&output, sizeof output, &settings, sizeof settings) ||
        overlap(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "settings preparation overlaps input");
    DriverSettingsContext c;
    Status s = preparation(c, r, prior, DriverGroup::MOTION_PARAMETERS, operation, now, deadline,
                           options, age);
    if (!s)
        return s;
    DriverObservation old;
    StateObservation status, monitors;
    if (!getDriver(prior, r.target, now, age, old) || old.group != DriverGroup::MOTION_PARAMETERS ||
        !controlEvidence(control, r, now, age) || prior.read.startedUs < control.completedUs ||
        !getStateBlock(state, StateBlock::STATUS, r.target, now, age, status) ||
        !getStateBlock(state, StateBlock::MONITORS, r.target, now, age, monitors) ||
        status.modeRaw != 1 || (status.statusword & 7u) != 7 || (monitors.monitor & 0x1cu) ||
        !after(status, control.completedUs) || !after(monitors, control.completedUs))
        return error(ReadError::STATE, "current reserved position pre-trigger evidence required");
    if (settings.positionSpeedRpm > 3000 || settings.rampUpTimeMs > 2000 ||
        settings.rampDownTimeMs > 2000)
        return error(ReadError::VALUE, "native speed/ramp candidate outside reviewed range");
    c.requested[0] = settings.positionSpeedRpm;
    c.requested[1] = settings.rampUpTimeMs;
    c.requested[2] = settings.rampDownTimeMs;
    select(c, prior, 0);
    output = c;
    return Ok();
}
Status prepareIoSettings(DriverSettingsContext& output, const SettingsReservation& r,
                         const DriverContext& prior, const IoSettings& settings, uint32_t operation,
                         uint64_t now, uint64_t deadline, const ReadOptions& options,
                         uint64_t age) noexcept {
    if (overlap(&output, sizeof output, &r, sizeof r) ||
        overlap(&output, sizeof output, &prior, sizeof prior) ||
        overlap(&output, sizeof output, &settings, sizeof settings) ||
        overlap(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "DI settings preparation overlaps input");
    DriverSettingsContext c;
    Status s = preparation(c, r, prior, DriverGroup::IO_INPUT_CONFIGURATION, operation, now,
                           deadline, options, age);
    if (!s)
        return s;
    IoObservation old;
    if (!getIo(prior, r.target, now, age, old) || old.group != DriverGroup::IO_INPUT_CONFIGURATION)
        return error(ReadError::STATE, "fresh reserved complete input configuration required");
    if (settings.invertedInputs & ~INPUT_POLARITY_BITS)
        return error(ReadError::VALUE, "DI polarity candidate contains undocumented bits");
    c.requested[0] = old.ioEntries;
    c.requested[1] = (old.inputPolarityRaw & ~INPUT_POLARITY_BITS) | settings.invertedInputs;
    for (unsigned i = 0; i < 7; ++i) {
        const uint16_t code = static_cast<uint16_t>(settings.functions[i]);
        if (code > 13)
            return error(ReadError::VALUE, "undocumented DI function candidate");
        c.requested[i + 2] = code;
    }
    select(c, prior, 1);
    output = c;
    return Ok();
}
Status prepareControlSettings(DriverSettingsContext& output, const SettingsReservation& r,
                              const DriverContext& prior, const ControlSettings& settings,
                              uint32_t operation, uint64_t now, uint64_t deadline,
                              const ReadOptions& options, uint64_t age) noexcept {
    if (overlap(&output, sizeof output, &r, sizeof r) ||
        overlap(&output, sizeof output, &prior, sizeof prior) ||
        overlap(&output, sizeof output, &settings, sizeof settings) ||
        overlap(&output, sizeof output, &options, sizeof options))
        return error(ReadError::OVERLAP, "control settings preparation overlaps input");
    DriverSettingsContext c;
    Status s =
        preparation(c, r, prior, DriverGroup::CONTROL, operation, now, deadline, options, age);
    if (!s)
        return s;
    ControlObservation old;
    if (!getControl(prior, r.target, now, age, old))
        return error(ReadError::STATE, "fresh reserved complete control observation required");
    const uint16_t mode = static_cast<uint16_t>(settings.loopMode);
    if (mode > 2 ||
        (r.model == SettingsModel::OL86_C && settings.loopMode == LoopMode::CLOSED_LOOP))
        return error(ReadError::VALUE, "loop selector or model lacks reviewed applicability");
    c.requested[0] = mode;
    c.requested[1] = old.encoderResolutionRaw;
    select(c, prior, 0);
    output = c;
    return Ok();
}
Status nextDriver(const DriverSettingsContext& c, const SettingsReservation& live,
                  const Binding& binding, uint64_t now, PreparedRead& output) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &live, sizeof live) ||
        overlap(&output, sizeof output, &binding, sizeof binding))
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
        candidate.transfer.step += 9;
    } else {
        if (c.cursor >= c.readback.read.count || !(c.changed & (1u << c.cursor)))
            return error(ReadError::STATE, "invalid settings cursor");
        candidate.ready = true;
        candidate.transfer = write(c);
    }
    output = candidate;
    return Ok();
}
Status noteDriverTransmission(DriverSettingsContext& c, const SettingsReservation& live,
                              Binding& binding, uint64_t now) noexcept {
    if (overlap(&binding, sizeof binding, &c, sizeof c) ||
        overlap(&binding, sizeof binding, &live, sizeof live) ||
        overlap(&c, sizeof c, &live, sizeof live))
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
Status advanceDriver(DriverSettingsContext& c, const SettingsReservation& live,
                     const Binding& binding, const CANOPEN::SdoCompletion& input,
                     uint64_t now) noexcept {
    if (overlap(&c, sizeof c, &input, sizeof input) || overlap(&c, sizeof c, &live, sizeof live) ||
        overlap(&c, sizeof c, &binding, sizeof binding))
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
    if (!sameWork(input.transfer, prepared.transfer) ||
        input.nodeGeneration != c.reservation.target.nodeGeneration ||
        input.admittedUs < c.stepStartedUs || input.admittedUs >= c.deadlineUs ||
        input.completedUs < input.admittedUs || input.completedUs < c.servicedUs ||
        input.completedUs > now)
        return error(ReadError::CORRELATION, "settings terminal does not match live work");
    DriverSettingsContext candidate = c;
    if (c.state == SettingsState::READING_BACK) {
        CANOPEN::SdoCompletion local = input;
        local.transfer.step -= 9;
        s = advanceDriver(candidate.readback, local, now);
        if (!s)
            return s;
        candidate.readback.read.evidence = input; // Retain the actual external step identity.
        candidate.uncertain |= candidate.readback.read.uncertain;
        if (candidate.readback.read.state == ReadState::FAILED)
            fail(candidate, candidate.readback.read.outcome, now, "settings readback failed");
        else {
            const unsigned field = c.readback.read.cursor;
            const bool compare =
                c.readback.read.kind != ReadKind::IO_INPUT_CONFIGURATION || field != 0;
            if (compare && candidate.readback.read.values[field].raw != c.requested[field])
                fail(candidate, ReadOutcome::REPLY_ERROR, now,
                     "settings readback contradicts candidate");
            else if (candidate.readback.read.state == ReadState::SUCCEEDED) {
                candidate.state = SettingsState::SUCCEEDED;
                candidate.outcome = ReadOutcome::SUCCESS;
                candidate.status = Ok();
            }
        }
    } else {
        const bool success = input.outcome == CANOPEN::SdoOutcome::SUCCESS;
        if (input.outcome > CANOPEN::SdoOutcome::UNSUPPORTED ||
            (input.transmissionPossible && (!input.queued || !c.transmissionNoted)) ||
            (input.transmissionConfirmed && !input.transmissionPossible) || input.size ||
            input.effectPossible != input.transmissionPossible ||
            input.effectUncertain != (input.transmissionPossible && !success) ||
            (success &&
             (!input.protocolAcknowledged || !input.transmissionPossible || input.abortValid)) ||
            (!success && input.protocolAcknowledged) ||
            (input.outcome == CANOPEN::SdoOutcome::SERVER_ABORT &&
             (!input.abortValid || !input.transmissionPossible)))
            return error(ReadError::CORRELATION, "inconsistent settings service evidence");
        candidate.uncertain |= input.effectUncertain || (c.transmissionNoted && !success);
        if (success) {
            candidate.confirmed |= uint16_t(1u << c.cursor);
            candidate.acknowledgedUs[c.cursor] = input.completedUs;
            candidate.transmissionNoted = false;
        }
        if (now >= c.deadlineUs || input.completedUs >= c.deadlineUs)
            fail(candidate, ReadOutcome::DEADLINE, now, "settings overall deadline");
        else if (!success)
            fail(candidate, outcome(input.outcome), now, "settings download failed");
        else {
            candidate.stepStartedUs = now;
            ++candidate.cursor;
            while (candidate.cursor < candidate.readback.read.count &&
                   !(candidate.changed & (1u << candidate.cursor)))
                ++candidate.cursor;
            if (candidate.cursor == candidate.readback.read.count) {
                ReadTarget target = c.reservation.target;
                target.channel.binding = binding;
                const DriverGroup group = c.readback.read.kind == ReadKind::IO_INPUT_CONFIGURATION
                                              ? DriverGroup::IO_INPUT_CONFIGURATION
                                          : c.readback.read.kind == ReadKind::CONTROL
                                              ? DriverGroup::CONTROL
                                              : DriverGroup::MOTION_PARAMETERS;
                s = prepareDriverRead(candidate.readback, target, c.operation, now, c.deadlineUs,
                                      c.readback.read.options, group);
                if (!s)
                    return s;
                candidate.state = SettingsState::READING_BACK;
            }
        }
    }
    candidate.evidence = input;
    candidate.evidencePresent = true;
    candidate.servicedUs = now;
    c = candidate;
    return Ok();
}
Status advanceDriver(DriverSettingsContext& c, ReadOutcome reason, uint64_t now) noexcept {
    if (!active(c) || now < c.servicedUs ||
        (reason != ReadOutcome::CANCELLED && reason != ReadOutcome::DEADLINE &&
         reason != ReadOutcome::INVALIDATED) ||
        (reason == ReadOutcome::DEADLINE && now < c.deadlineUs))
        return error(ReadError::STATE, "invalid local settings termination");
    DriverSettingsContext candidate = c;
    if (c.state == SettingsState::READING_BACK) {
        const Status s = advanceDriver(candidate.readback, reason, now);
        if (!s)
            return s;
        candidate.uncertain |= candidate.readback.read.uncertain;
    }
    candidate.uncertain |= c.transmissionNoted;
    if (now >= c.deadlineUs)
        reason = ReadOutcome::DEADLINE;
    fail(candidate, reason, now, "settings locally withdrawn; no rollback or motor stop");
    c = candidate;
    return Ok();
}
Status getDriver(const DriverSettingsContext& c, const SettingsReservation& live,
                 const ReadTarget& target, uint64_t now, uint64_t age,
                 DriverObservation& output) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &live, sizeof live) ||
        overlap(&output, sizeof output, &target, sizeof target))
        return error(ReadError::OVERLAP, "settings result overlaps input");
    Status s = current(c, live, target.channel.binding, now);
    if (!s)
        return s;
    if (c.state != SettingsState::SUCCEEDED)
        return error(ReadError::STATE, "settings not complete and matched");
    return getDriver(c.readback, target, now, age, output);
}
Status getIo(const DriverSettingsContext& c, const SettingsReservation& live,
             const ReadTarget& target, uint64_t now, uint64_t age, IoObservation& output) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &live, sizeof live) ||
        overlap(&output, sizeof output, &target, sizeof target))
        return error(ReadError::OVERLAP, "settings result overlaps input");
    Status s = current(c, live, target.channel.binding, now);
    if (!s)
        return s;
    if (c.state != SettingsState::SUCCEEDED)
        return error(ReadError::STATE, "settings not complete and matched");
    return getIo(c.readback, target, now, age, output);
}
Status getControl(const DriverSettingsContext& c, const SettingsReservation& live,
                  const ReadTarget& target, uint64_t now, uint64_t age,
                  ControlObservation& output) noexcept {
    if (overlap(&output, sizeof output, &c, sizeof c) ||
        overlap(&output, sizeof output, &live, sizeof live) ||
        overlap(&output, sizeof output, &target, sizeof target))
        return error(ReadError::OVERLAP, "control settings result overlaps input");
    Status s = current(c, live, target.channel.binding, now);
    if (!s)
        return s;
    if (c.state != SettingsState::SUCCEEDED)
        return error(ReadError::STATE, "control settings not complete and matched");
    return getControl(c.readback, target, now, age, output);
}
} // namespace LICHUAN_C
} // namespace MotorControlCAN
