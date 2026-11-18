// SPDX-License-Identifier: MIT
// Grammar follows MotorControl-RS ProbeConsole; native policies remain in the typed CAN APIs.
#include "ProbeConsoleText.h"
#include <new>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
bool eq(const char* a, const char* b) {
    return !std::strcmp(a, b);
}
bool positiveInteger(const char* text, uint64_t& value) {
    if (!*text)
        return false;
    uint64_t n = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9' || n > (UINT64_MAX - unsigned(*text - '0')) / 10)
            return false;
        n = n * 10 + unsigned(*text - '0');
    }
    value = n;
    return n != 0;
}
bool spatialUnit(const char* text, M::PositionUnit& value) {
    const char* names[] = {"steps", "fullsteps", "counts", "turn", "deg", "rad", "mm"};
    for (unsigned n = 0; n < 7; ++n)
        if (eq(text, names[n])) {
            value = static_cast<M::PositionUnit>(n);
            return true;
        }
    return false;
}
bool rateUnit(const char* text, M::VelocityUnit& value) {
    if (eq(text, "rpm")) {
        value = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
        return true;
    }
    if (eq(text, "turns/s")) {
        value = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::SECOND);
        return true;
    }
    const size_t length = std::strlen(text);
    char spatial[12] = {};
    if (length < 3 || length - 2 >= sizeof spatial || std::strcmp(text + length - 2, "/s"))
        return false;
    std::memcpy(spatial, text, length - 2);
    value.time = M::TimeUnit::SECOND;
    return spatialUnit(spatial, value.position);
}
bool frame(const char* text, M::CoordinateFrame& value) {
    if (eq(text, "native"))
        value = M::CoordinateFrame::NATIVE;
    else if (eq(text, "motor"))
        value = M::CoordinateFrame::MOTOR;
    else if (eq(text, "load"))
        value = M::CoordinateFrame::LOAD;
    else
        return false;
    return true;
}
bool basis(const char* text, M::RelativeBasis& value) {
    if (eq(text, "actual"))
        value = M::RelativeBasis::ACTUAL;
    else if (eq(text, "commanded"))
        value = M::RelativeBasis::COMMANDED;
    else if (eq(text, "queued"))
        value = M::RelativeBasis::QUEUED;
    else
        return false;
    return true;
}
bool setup(const char* text, L::MoveSetup& value) {
    if (eq(text, "write"))
        value = L::MoveSetup::WRITE_ALL;
    else if (eq(text, "verify"))
        value = L::MoveSetup::REUSE_STORED;
    else
        return false;
    return true;
}
template <class Request>
bool rounding(const ConsoleLine& line, size_t& next, Request& request, M::PositionUnit unit) {
    if (next == line.count)
        return true;
    if (!eq(line.token(next++), "round") || next + 1 >= line.count)
        return false;
    const char* policy = line.token(next++);
    if (eq(policy, "exact"))
        request.rounding = M::Rounding::EXACT;
    else if (eq(policy, "nearest"))
        request.rounding = M::Rounding::NEAREST;
    else if (eq(policy, "zero"))
        request.rounding = M::Rounding::TOWARD_ZERO;
    else if (eq(policy, "floor"))
        request.rounding = M::Rounding::FLOOR;
    else if (eq(policy, "ceil"))
        request.rounding = M::Rounding::CEIL;
    else
        return false;
    if (!M::parseExactNumber(line.token(next++), request.maximumQuantizationError) ||
        request.maximumQuantizationError.numerator < 0)
        return false;
    if (next < line.count) {
        M::Rational allowance;
        if (unit != M::PositionUnit::RADIANS || !eq(line.token(next++), "approx") ||
            !M::parseExactNumber(line.token(next++), allowance) || allowance.numerator <= 0 ||
            !M::rationalToDouble(allowance, request.maximumApproximationError))
            return false;
        request.approximate = true;
    }
    return next == line.count;
}
M::VelocityRequest rpm(const M::Rational& value, const M::AxisConfig& axis) {
    M::VelocityRequest request;
    request.value = value;
    request.unit = M::VelocityUnit(M::PositionUnit::TURNS, M::TimeUnit::MINUTE);
    request.configurationGeneration = axis.binding.configurationGeneration;
    return request;
}
bool parseMove(const ConsoleLine& line, const M::AxisConfig& axis, const SimpleMoveIntent& intent,
               M::MoveRequest& request, L::MoveProfile& profile, size_t offset = 0) {
    auto& p = request.position;
    p.configurationGeneration = axis.binding.configurationGeneration;
    p.relative = eq(line.token(1 + offset), "relative");
    p.wrapped = eq(line.token(1 + offset), "angle");
    if ((!p.relative && !p.wrapped && !eq(line.token(1 + offset), "absolute")) ||
        !M::parseExactNumber(line.token(2 + offset), p.value) || !spatialUnit(line.token(3 + offset), p.unit) ||
        !frame(line.token(4 + offset), p.frame))
        return false;
    size_t next = 5 + offset;
    if (p.wrapped) {
        const char* path = line.token(next++);
        const char* tie = line.token(next++);
        if (eq(path, "positive"))
            p.path = M::AnglePath::POSITIVE;
        else if (eq(path, "negative"))
            p.path = M::AnglePath::NEGATIVE;
        else if (!eq(path, "shortest"))
            return false;
        if (eq(tie, "positive"))
            p.tie = M::HalfTurnTie::POSITIVE;
        else if (eq(tie, "negative"))
            p.tie = M::HalfTurnTie::NEGATIVE;
        else if (!eq(tie, "reject"))
            return false;
    }
    M::Rational speed;
    if (!M::parseExactNumber(line.token(next++), speed) || !eq(line.token(next++), "configured"))
        return false;
    request.speed = rpm(speed, axis);
    profile.rampUpTimeMs = intent.rampUpKnown ? intent.rampUpMs : UINT32_MAX;
    profile.rampDownTimeMs = intent.rampDownKnown ? intent.rampDownMs : UINT32_MAX;
    profile.setup = intent.setup;
    if (eq(line.token(next), "setup")) {
        ++next;
        if (!setup(line.token(next++), profile.setup))
            return false;
    }
    if (eq(line.token(next), "basis")) {
        ++next;
        if (!p.relative || !basis(line.token(next++), p.basis))
            return false;
    }
    return rounding(line, next, p, p.unit) && next == line.count;
}
const char* outcomeName(M::ActionOutcome outcome) {
    const char* names[] = {"NONE",
                           "OBSERVED",
                           "PROTOCOL_REJECTED",
                           "REPLY_ERROR",
                           "TRANSPORT_ERROR",
                           "CANCELLED",
                           "DEADLINE",
                           "INVALIDATED",
                           "OBSERVATION_SCOPE",
                           "DRIVE_FAULT",
                           "ITERATION_EXHAUSTED",
                           "STOP_SENT_UNCONFIRMED",
                           "SERVICE_TIMEOUT"};
    const unsigned n = static_cast<unsigned>(outcome);
    return n < sizeof names / sizeof names[0] ? names[n] : "UNKNOWN";
}
// Keep replacement-only parsing storage off ordinary move and home dispatch chains.
// Inlining it into dispatchMotion exceeded the measured 6144-byte Xtensa chain budget of the
// 12288-byte stack (2026-11-17 the stack is 16384 and the budget 8192); it stays off that chain.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
M::Status requestReplacement(const ConsoleLine& line, const SimpleMoveIntent& intent, ProbeMotion& motion,
                   uint32_t originalOperation, uint64_t now) {
    const M::AxisConfig& axis = motion.axisConfig();
    const L::MoveContext* native = motion.nativeMove();
    const MotionResult* retained = motion.normalResult(originalOperation);
    if (native && retained && retained->id == motion.normalResult().id &&
        retained->kind == MotionKind::MOVE) {
        L::MoveReplacementRequest request;
        request.request.position = native->parameters.target.requested;
        request.request.speed = native->speed.requested;
        request.profile = native->profile;
        request.profile.setup = L::MoveSetup::WRITE_ALL;
        request.mode = L::MoveReplacementMode::IMMEDIATE;
        bool parsed = false;
        if (eq(line.token(1), "speed")) {
            M::Rational value;
            parsed =
                (line.count == 4 || line.count == 5) && M::parseExactNumber(line.token(3), value);
            request.request.speed = rpm(value, axis);
            request.fields = 2;
            if (line.count == 5) {
                parsed =
                    parsed && (eq(line.token(4), "immediate") || eq(line.token(4), "buffered"));
                if (eq(line.token(4), "buffered"))
                    request.mode = L::MoveReplacementMode::BUFFERED;
            }
        } else if (eq(line.token(1), "ramps")) {
            uint64_t upTime = 0, downTime = 0;
            parsed = (line.count == 5 || line.count == 6) &&
                     positiveInteger(line.token(3), upTime) && upTime <= 2000 &&
                     positiveInteger(line.token(4), downTime) && downTime <= 2000;
            request.profile.rampUpTimeMs = uint32_t(upTime);
            request.profile.rampDownTimeMs = uint32_t(downTime);
            request.fields = 12;
            if (line.count == 6) {
                parsed =
                    parsed && (eq(line.token(5), "immediate") || eq(line.token(5), "buffered"));
                if (eq(line.token(5), "buffered"))
                    request.mode = L::MoveReplacementMode::BUFFERED;
            }
        } else {
            const bool policy = eq(line.token(3), "immediate") || eq(line.token(3), "buffered");
            if (eq(line.token(3), "buffered"))
                request.mode = L::MoveReplacementMode::BUFFERED;
            SimpleMoveIntent retainedIntent = intent;
            retainedIntent.rampUpKnown = retainedIntent.rampDownKnown = true;
            retainedIntent.rampUpMs = native->profile.rampUpTimeMs;
            retainedIntent.rampDownMs = native->profile.rampDownTimeMs;
            retainedIntent.setup = L::MoveSetup::WRITE_ALL;
            parsed = parseMove(line, axis, retainedIntent, request.request, request.profile,
                               policy ? 3 : 2) &&
                     !request.request.position.relative && !request.request.position.wrapped;
            request.fields = 3;
        }
        if (parsed)
            return motion.requestMoveReplacement(originalOperation, request, now);
    }
    return M::Status(M::Err::ILLEGAL_VALUE, 0, "invalid_arguments_or_gated_policy");
}
const char* truth(bool value) {
    return value ? "true" : "false";
}
} // namespace

bool ProbeConsole::enqueueGroupStop(const char* text, uint32_t request, uint32_t terminal) noexcept {
    const size_t size = std::strlen(text);
    if (groupReplyUsed_ || !size || size >= RECORD_CAPACITY || text[size - 1] != '\n')
        return false;
    std::memcpy(groupReply_.text, text, size + 1);
    groupReply_.length = uint16_t(size);
    groupReply_.request = request;
    groupReply_.terminal = terminal;
    groupReplyUsed_ = true;
    return true;
}
void ProbeConsole::dispatchGroupStop(const ConsoleLine& line, uint64_t now) noexcept {
    const char* kind = line.count == 2 ? "fast" : line.token(2);
    if ((line.count != 2 && line.count != 3) || (!eq(kind, "normal") && !eq(kind, "fast"))) {
        error(line.id, line.json, "stop", "invalid_group_stop_arguments");
        return;
    }
    Operation& op = operations_[GROUP_OPERATION_SLOT];
    if (!groupStop_ || op.used || groupReplyUsed_ || nextOperation_ == UINT32_MAX) {
        error(line.id, line.json, "stop", "group_stop_storage_unavailable");
        return;
    }
    size_t count = 0;
    for (const MotionAttachment& attachment : motions_)
        if (attachment.motion)
            groupMembers_[count++] = attachment.motion;
    const M::Status status = groupStop_->request(groupMembers_, count,
        eq(kind, "normal") ? L::StopKind::HALT : L::StopKind::QUICK_STOP, now);
    if (!status) {
        error(line.id, line.json, "stop", status.msg ? status.msg : M::errToString(status.code),
              false, status.code, status.detail);
        return;
    }
    op.used = op.groupStop = true;
    op.json = line.json;
    op.request = line.id;
    op.id = ++nextOperation_;
    op.nativeOperation = groupStop_->result().id;
    std::snprintf(op.command, sizeof op.command, "stop");
    for (bool& printed : groupMemberPrinted_)
        printed = false;
    motionAccepted(op);
}
bool ProbeConsole::groupStopTerminal(Operation& op) noexcept {
    const bool matching = groupStop_ && groupStop_->result().id == op.nativeOperation;
    if (!op.terminal && matching) {
        bool pendingOutput = false;
        for (size_t n = 0; n < groupStop_->memberCount(); ++n) {
            const GroupStopMember& member = *groupStop_->member(n);
            if (groupMemberPrinted_[n])
                continue;
            pendingOutput = true;
            if (!member.finished)
                continue;
            ConsoleText text;
            text.start(op.json, op.request, "group_stop_axis", op.command);
            text.add(op.json ? ",\"operation_id\":%lu,\"axis_id\":%lu,\"accepted\":%s,\"status\":\"%s\","
                               "\"detail\":%ld,\"outcome\":\"%s\",\"uncertain\":%s,\"native_operation\":%lu"
                             : "operation=%lu axis=%lu accepted=%s status=%s detail=%ld outcome=%s uncertain=%s native-operation=%lu",
                     static_cast<unsigned long>(op.id), static_cast<unsigned long>(member.logicalAxisId),
                     truth(member.accepted), M::errToString(member.admission.code), static_cast<long>(member.admission.detail),
                     member.accepted ? outcomeName(member.stop.result.outcome) : "REFUSED",
                     truth(!member.accepted || member.stop.result.uncertain ||
                           member.stop.result.state != M::ActionState::SUCCEEDED),
                     static_cast<unsigned long>(member.accepted ? member.stop.result.nativeOperation : 0));
            text.end(op.json);
            if (text.valid && enqueueGroupStop(text.bytes, op.request))
                groupMemberPrinted_[n] = true;
            return false;
        }
        if (pendingOutput || groupStop_->result().state == M::ActionState::ACTIVE)
            return false;
    }
    if (!op.terminal) {
        const bool success = matching && groupStop_->result().state == M::ActionState::SUCCEEDED;
        ConsoleText body;
        body.add(",\"ok\":%s,\"status\":\"%s\",\"detail\":0,\"outcome\":\"%s\",\"uncertain\":%s,"
                 "\"command_id\":%lu,\"operation_id\":%lu,\"completed_us\":\"%llu\",\"abort_code\":null,\"axis_count\":%lu",
                 truth(success), success ? "OK" : "INVALID_CONFIG", success ? "OBSERVED" : matching ? "UNCONFIRMED" : "INVALIDATED",
                 truth(!matching || groupStop_->result().uncertain), static_cast<unsigned long>(op.request),
                 static_cast<unsigned long>(op.id), static_cast<unsigned long long>(matching ? groupStop_->result().completedUs : lastUs_),
                 static_cast<unsigned long>(matching ? groupStop_->memberCount() : 0));
        if (!body.valid)
            return false;
        std::memcpy(op.terminalRecord, body.bytes, body.used + 1);
        op.terminal = true;
    }
    if (!op.queuedTerminal && !op.delivered) {
        ConsoleText text;
        text.start(op.json, op.request, "terminal", op.command);
        if (op.json)
            text.add("%s", op.terminalRecord);
        else
            text.add("operation %lu%s", static_cast<unsigned long>(op.id), op.terminalRecord);
        text.end(op.json);
        if (text.valid && enqueueGroupStop(text.bytes, op.request, op.id))
            op.queuedTerminal = true;
    }
    return op.terminal;
}
bool ProbeConsole::enqueueStop(const char* text, ProbeMotion* motion, uint8_t stop, uint32_t request,
                               uint32_t terminal) noexcept {
    const unsigned axis = motionSlot(motion);
    if (stop < 1 || stop > 2 || axis == MOTION_CAPACITY)
        return false;
    const unsigned slot = 2 * axis + stop - 1;
    if (stopReplyUsed_[slot])
        return false;
    const size_t size = std::strlen(text);
    if (!size || size >= RECORD_CAPACITY || text[size - 1] != '\n')
        return false;
    Record& record = stopReplies_[slot];
    std::memcpy(record.text, text, size + 1);
    record.length = static_cast<uint16_t>(size);
    record.request = request;
    record.terminal = terminal;
    stopReplyUsed_[slot] = true;
    return true;
}

ProbeMotion* ProbeConsole::motionForChange(const ConsoleLine& line) const noexcept {
    if (!((eq(line.token(0), "velocity") && eq(line.token(1), "change")) ||
          (eq(line.token(0), "move") && (eq(line.token(1), "speed") ||
           eq(line.token(1), "ramps") || eq(line.token(1), "retarget")))))
        return nullptr;
    uint64_t id = 0;
    if (!positiveInteger(line.token(2), id) || id > UINT32_MAX)
        return nullptr;
    for (const Operation& op : operations_)
        if (op.used && op.id == id && op.motion && !op.stop)
            return op.attachment;
    return nullptr;
}

void ProbeConsole::dispatchMotion(const ConsoleLine& line, uint64_t now) noexcept {
    ProbeMotion* motion = selectedMotion();
    const char* name = line.token(0);
    const bool stop = eq(name, "stop");
    if (stop && eq(line.token(1), "all")) {
        dispatchGroupStop(line, now);
        return;
    }
    bool explicitStop = false;
    const char* stopKind = line.token(1);
    uint64_t explicitAxis = 0;
    if (stop && positiveInteger(line.token(1), explicitAxis) && explicitAxis <= UINT32_MAX) {
        explicitStop = true;
        motion = motionForAxis(uint32_t(explicitAxis));
        stopKind = line.count == 2 ? "fast" : line.token(2);
    }
    if (ProbeMotion* changed = motionForChange(line))
        motion = changed;
    const unsigned intentSlot = motionSlot(motion);
    SimpleMoveIntent& intent = intentSlot < MOTION_CAPACITY ? motions_[intentSlot].intent : intent_;
    const uint8_t stopSlot = stop && eq(stopKind, "normal") ? 1 : 2;
    const auto reject = [&](const M::Status& status) {
        if (status.code == M::Err::ILLEGAL_VALUE && stats_.invalid != UINT32_MAX)
            ++stats_.invalid;
        error(line.id, line.json, name, status.msg ? status.msg : M::errToString(status.code),
              false, status.code, status.detail);
    };
    const M::Status syntax(M::Err::ILLEGAL_VALUE, 0, "invalid_arguments_or_gated_policy");
    // Inspection needs no selected drive, native evidence or controller.
    if (eq(name, "home") && line.count == 2 && eq(line.token(1), "methods")) {
        motionIntentReply(line, true);
        return;
    }
    const bool speed = eq(name, "speed"), up = eq(name, "accel"), down = eq(name, "decel");
    if ((speed || up || down || eq(name, "motion")) && line.count == 1) {
        motionIntentReply(line, true);
        return;
    }
    if (eq(name, "motion")) {
        L::MoveSetup choice;
        const M::Status status = line.count == 2 && setup(line.token(1), choice)
                                     ? setSimpleSetup(intent, choice)
                                     : syntax;
        if (!status) {
            reject(status);
            return;
        }
    } else if (up || down) {
        M::Rational value;
        if ((line.count != 2 && line.count != 3) || (line.count == 3 && !eq(line.token(2), "ms")) ||
            !M::parseExactNumber(line.token(1), value)) {
            reject(syntax);
            return;
        }
        const M::Status status = setSimpleRamp(intent, up, value);
        if (!status) {
            reject(status);
            return;
        }
    } else {
        if (!motion) {
            reject(M::Status(M::Err::UNSUPPORTED, 0,
                             "attach_actual_axis_and_explicit_native_prerequisites"));
            return;
        }
        const M::AxisConfig& axis = motion->axisConfig();
        if (eq(name, "velocity") && eq(line.token(1), "change")) {
            uint64_t id = 0;
            size_t next = 7;
            M::VelocityRequest rate;
            rate.configurationGeneration = axis.binding.configurationGeneration;
            if (line.count < next || !positiveInteger(line.token(2), id) || id > UINT32_MAX ||
                !M::parseExactNumber(line.token(3), rate.value) ||
                !rateUnit(line.token(4), rate.unit) || !frame(line.token(5), rate.frame) ||
                !eq(line.token(6), "configured") || !rounding(line, next, rate, rate.unit.position)) {
                reject(syntax);
                return;
            }
            Operation* original = operation(static_cast<uint32_t>(id));
            if (!original || !original->motion || original->attachment != motion || original->stop || original->terminal) {
                reject(M::Status(M::Err::INVALID_CONFIG, 0, "velocity_change_requires_active_operation_id"));
                return;
            }
            const M::Status status = motion->requestVelocityChange(original->nativeOperation, rate, now);
            if (!status) {
                reject(status);
                return;
            }
            velocityChangeAccepted(line, original->id);
            return;
        }
        if (speed) {
            M::Rational value;
            if ((line.count != 2 && line.count != 3) ||
                (line.count == 3 && !eq(line.token(2), "rpm")) ||
                !M::parseExactNumber(line.token(1), value)) {
                reject(syntax);
                return;
            }
            const M::Status status = setSimpleSpeed(intent, rpm(value, axis), axis);
            if (!status) {
                reject(status);
                return;
            }
        } else if (eq(name, "stepsperturn")) {
            M::Rational value;
            M::UnitScale scale;
            M::Status status = line.count == 2 && M::parseExactNumber(line.token(1), value)
                                   ? makeCommandScale(value, scale)
                                   : syntax;
            if (status)
                status = motion->configureCommandScale(scale, now);
            if (status && !app_.select(motion->target()))
                status = M::Status(M::Err::INVALID_CONFIG, 0, "host_selection_cannot_follow_axis");
            if (!status) {
                reject(status);
                return;
            }
        } else if (eq(name, "axis")) {
            M::Rational value;
            M::Status status = line.count == 3 && eq(line.token(1), "origin") &&
                                       M::parseExactNumber(line.token(2), value) &&
                                       value.denominator == 1
                                   ? motion->setOrigin(value.numerator, now)
                                   : syntax;
            if (status && !app_.select(motion->target()))
                status = M::Status(M::Err::INVALID_CONFIG, 0, "host_selection_cannot_follow_axis");
            if (!status) {
                reject(status);
                return;
            }
        } else {
            const bool replacement = eq(name, "move") &&
                (eq(line.token(1), "speed") || eq(line.token(1), "ramps") || eq(line.token(1), "retarget"));
            Operation* original = nullptr;
            if (replacement) {
                uint64_t id = 0;
                if (!positiveInteger(line.token(2), id) || id > UINT32_MAX ||
                    !(original = operation(static_cast<uint32_t>(id))) || !original->motion ||
                    original->attachment != motion || original->stop || original->terminal) {
                    reject(M::Status(M::Err::INVALID_CONFIG, 0, "replacement_requires_active_move_id"));
                    return;
                }
            }
            const unsigned stopIndex = 2 * motionSlot(motion) + stopSlot - 1;
            Operation* op = stop ? &operations_[OPERATION_CAPACITY + stopIndex] : nullptr;
            if (!stop)
                for (unsigned n = 0; n < OPERATION_CAPACITY && !op; ++n)
                    if (!operations_[n].used)
                        op = &operations_[n];
            if (!op || op->used || nextOperation_ == UINT32_MAX ||
                (stop && stopReplyUsed_[stopIndex])) {
                reject(M::Status(M::Err::INVALID_CONFIG, 0, "retained_results_full"));
                return;
            }
            // An application deadline, not a manufacturer completion delay. Every native
            // request keeps its own immutable budget and rejects insufficient preparation time.
            const uint64_t tail = UINT64_C(10000000);
            if (now > uint64_t(INT64_MAX) - tail) {
                reject(syntax);
                return;
            }
            uint64_t cutoff = now + tail;
            M::Status status = syntax;
            if (eq(name, "moveby") || eq(name, "moveto")) {
                M::PositionRequest position;
                position.relative = eq(name, "moveby");
                position.configurationGeneration = axis.binding.configurationGeneration;
                if ((line.count == 2 || line.count == 3) &&
                    M::parseExactNumber(line.token(1), position.value) &&
                    (line.count == 2 || (spatialUnit(line.token(2), position.unit) &&
                                         position.unit != M::PositionUnit::FULL_STEPS &&
                                         position.unit != M::PositionUnit::ENCODER_COUNTS &&
                                         position.unit != M::PositionUnit::RADIANS))) {
                    M::MoveRequest request;
                    L::MoveProfile profile;
                    status = makeSimplePosition(position.value, position.unit, position.relative,
                        axis.binding.configurationGeneration, position);
                    if (status)
                        status = buildSimpleMove(intent, position, axis, !position.relative, request, profile);
                    if (status)
                        status = motion->requestMove(request, profile, now, cutoff);
                }
            } else if (eq(name, "move")) {
                if (replacement) {
                    status = requestReplacement(line, intent, *motion, original->nativeOperation, now);
                } else {
                    M::MoveRequest request;
                    L::MoveProfile profile;
                    if (parseMove(line, axis, intent, request, profile))
                        status = motion->requestMove(request, profile, now, cutoff);
                }
            } else if (eq(name, "velocity")) {
                M::VelocityRequest rate;
                uint64_t duration = 0;
                size_t next = 7;
                rate.configurationGeneration = axis.binding.configurationGeneration;
                if (line.count >= next && M::parseExactNumber(line.token(1), rate.value) &&
                    rateUnit(line.token(2), rate.unit) && frame(line.token(3), rate.frame) &&
                    positiveInteger(line.token(4), duration) && eq(line.token(5), "configured") &&
                    (eq(line.token(6), "normal") || eq(line.token(6), "fast")) &&
                    rounding(line, next, rate, rate.unit.position)) {
                    M::FiniteVelocityRequest request;
                    status = makeFiniteVelocity(rate, duration, axis, request);
                    if (status && request.durationUs > uint64_t(INT64_MAX) - cutoff)
                        status = syntax;
                    if (status) {
                        cutoff += request.durationUs;
                        status = motion->requestVelocity(request,
                                                          eq(line.token(6), "normal")
                                                              ? L::StopKind::HALT
                                                              : L::StopKind::QUICK_STOP,
                                                          now, cutoff);
                    }
                }
            } else if (eq(name, "home")) {
                uint64_t method = 0, ramp = 0;
                M::Rational search, creep;
                if (line.count == 6 && positiveInteger(line.token(1), method) &&
                    (method == 17 || method == 18 || method == 24 || method == 29) &&
                    M::parseExactNumber(line.token(2), search) &&
                    M::parseExactNumber(line.token(3), creep) &&
                    positiveInteger(line.token(4), ramp) && ramp <= 2000 &&
                    eq(line.token(5), "zero")) {
                    L::HomeRequest request;
                    request.method = static_cast<L::HomingMethod>(method);
                    request.searchSpeed = rpm(search, axis);
                    request.creepSpeed = rpm(creep, axis);
                    request.rampTimeMs = static_cast<uint32_t>(ramp);
                    status = motion->requestHome(request, now, cutoff);
                }
            } else if (stop) {
                if ((explicitStop ? line.count == 2 || line.count == 3 : line.count == 2) &&
                    (eq(stopKind, "normal") || eq(stopKind, "fast")))
                    status = motion->requestStop(
                        stopSlot == 1 ? L::StopKind::HALT : L::StopKind::QUICK_STOP, now);
            } else if (line.count == 1) {
                if (eq(name, "enable"))
                    status = motion->requestEnable(now, cutoff);
                else if (eq(name, "alarm-clear"))
                    status = motion->requestAlarmClear(now, cutoff);
                else if (eq(name, "position-clear"))
                    status = motion->requestPositionClear(0, now, cutoff);
            }
            if (!status) {
                reject(status);
                return;
            }
            new (static_cast<void*>(op)) Operation();
            op->used = op->motion = true;
            op->attachment = motion;
            op->axisId = motion->target().channel.binding.logicalAxisId;
            op->json = line.json;
            op->request = line.id;
            op->id = ++nextOperation_;
            op->stop = stop ? stopSlot : 0;
            op->nativeOperation =
                stop ? motion
                           ->stopResult(stopSlot == 1 ? L::StopKind::HALT : L::StopKind::QUICK_STOP)
                           ->result.id
                     : motion->normalResult().id;
            if (replacement) {
                op->originalMotion = original->id;
                op->replacementMotion = op->id;
                original->originalMotion = original->id;
                original->replacementMotion = op->id;
            }
            std::snprintf(op->command, sizeof op->command, "%s", name);
            motionAccepted(*op);
            return;
        }
    }
    motionIntentReply(line, false);
}
// As for motionAccepted, format only after the request returns so its 512-byte text buffer
// does not remain on the stack through native preparation and conversion.
void ProbeConsole::velocityChangeAccepted(const ConsoleLine& line, uint32_t operation) noexcept {
    ConsoleText text;
    text.start(line.json, line.id, "reply", line.token(0));
    const Operation* original = this->operation(operation);
    text.add(line.json ? ",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\",\"operation_id\":%lu,\"change_index\":1,\"axis_id\":%lu"
                       : "accepted operation %lu change 1 axis %lu",
             static_cast<unsigned long>(operation), static_cast<unsigned long>(original ? original->axisId : 0));
    text.end(line.json);
    if ((!text.valid || !enqueue(text.bytes, false, line.id)) && stats_.outputDropped != UINT32_MAX)
        ++stats_.outputDropped;
}
void ProbeConsole::motionAccepted(Operation& op) noexcept {
    ConsoleText text;
    text.start(op.json, op.request, "reply", op.command);
    text.add(op.json ? ",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\",\"operation_id\":%lu"
                     : "accepted operation %lu",
             static_cast<unsigned long>(op.id));
    if (op.groupStop)
        text.add(op.json ? ",\"axis_count\":%lu" : " axes=%lu", static_cast<unsigned long>(groupStop_->memberCount()));
    else
        text.add(op.json ? ",\"axis_id\":%lu" : " axis=%lu", static_cast<unsigned long>(op.axisId));
    text.end(op.json);
    if (!(op.groupStop ? enqueueGroupStop(text.bytes, op.request)
          : op.stop ? enqueueStop(text.bytes, op.attachment, op.stop, op.request)
                    : enqueue(text.bytes, false, op.request)) &&
        stats_.outputDropped != UINT32_MAX)
        ++stats_.outputDropped;
}
void ProbeConsole::motionIntentReply(const ConsoleLine& line, bool inspect) noexcept {
    SimpleMoveIntent& intent = motionIntent();
    const char* name = line.token(0);
    const bool speed = eq(name, "speed"), up = eq(name, "accel"), down = eq(name, "decel");
    ConsoleText text;
    text.start(line.json, line.id, "reply", name);
    text.add(line.json ? ",\"ok\":true,\"status\":\"OK\"" : "OK ");
    if (eq(name, "home")) {
        text.add(line.json ? ",\"methods\":[17,18,24,29],\"qualified\":false"
                           : "17 negative-limit; 18 positive-limit; 24 positive-origin; 29 "
                             "negative-origin; qualification required");
    } else if (inspect) {
        text.add(line.json ? ",\"value\":" : "value=");
        if (eq(name, "motion"))
            text.string(intent.setup == L::MoveSetup::WRITE_ALL ? "write" : "verify");
        else if (speed && intent.speedKnown)
            text.add("[\"%lld\",\"%llu\"]", static_cast<long long>(intent.speed.value.numerator),
                     static_cast<unsigned long long>(intent.speed.value.denominator));
        else if ((up && intent.rampUpKnown) || (down && intent.rampDownKnown))
            text.add("\"%lu\"",
                     static_cast<unsigned long>(up ? intent.rampUpMs : intent.rampDownMs));
        else
            text.add("null");
        if (speed || up || down)
            text.add(line.json ? ",\"unit\":\"%s\"" : " %s", speed ? "rpm" : "ms");
    } else
        text.add(line.json ? ",\"device_traffic\":false" : "host intent only");
    text.end(line.json);
    if ((!text.valid || !enqueue(text.bytes, false, line.id)) && stats_.outputDropped != UINT32_MAX)
        ++stats_.outputDropped;
}

bool ProbeConsole::motionValues(Operation& op) noexcept {
    ProbeMotion* motion = op.attachment;
    if (op.stop) {
        const MotionStopResult* stop =
            motion
                ? motion->stopResult(op.stop == 1 ? L::StopKind::HALT : L::StopKind::QUICK_STOP)
                : nullptr;
        if (!stop)
            return true;
        const StopAttempt* attempts[] = {&stop->sequenced, &stop->firstBestEffort,
                                         &stop->bestEffort};
        const char* names[] = {"sequenced", "first_best_effort", "best_effort"};
        while (op.valueCursor < 3 && !attempts[op.valueCursor]->present)
            ++op.valueCursor;
        if (op.valueCursor == 3)
            return true;
        const StopAttempt& attempt = *attempts[op.valueCursor];
        ConsoleText text;
        text.start(op.json, op.request, "stop_attempt", op.command);
        text.add(op.json ? ",\"operation_id\":%lu,\"attempt\":\"%s\",\"native_operation\":%lu,"
                           "\"outcome\":\"%s\",\"execution_code\":%u,\"completion_code\":%u,"
                           "\"uncertain\":%s,"
                           "\"reply_qualifies\":%s,\"abort_code\":"
                         : "operation=%lu %s native-operation=%lu outcome=%s execution=%u "
                           "completion=%u uncertain=%s reply-qualifies=%s abort=",
                 static_cast<unsigned long>(op.id), names[op.valueCursor],
                 static_cast<unsigned long>(attempt.operation), outcomeName(attempt.outcome),
                 unsigned(attempt.execution), unsigned(attempt.completion),
                 truth(attempt.uncertain), truth(op.valueCursor == 0));
        if (attempt.commandPresent && attempt.command.abortValid)
            text.add("%lu", static_cast<unsigned long>(attempt.command.abortCode));
        else
            text.add("null");
        text.end(op.json);
        if (text.valid && enqueueStop(text.bytes, op.attachment, op.stop, op.request))
            ++op.valueCursor;
        return false;
    }
    if (!motion || !motion->retained())
        return true;
    const MotionResult* correlated = motion->normalResult(op.nativeOperation);
    if (!correlated)
        return true;
    const auto& result = *correlated;
    const MotionValue* values[] = {&result.target, &result.rate, &result.creepRate};
    const char* fields[] = {"target", "speed", "creep_speed"};
    while (op.valueCursor < 3 && !values[op.valueCursor]->present)
        ++op.valueCursor;
    if (op.valueCursor >= 3) {
        if (op.valueCursor == 3 && op.originalMotion && op.replacementMotion) {
            if (ordinaryCount_ == ORDINARY_REPLIES)
                return false;
            ConsoleText text;
            text.start(op.json, op.request, "move_relation", op.command);
            text.add(op.json ? ",\"operation_id\":%lu,\"original\":%lu,\"replacement\":%lu,"
                               "\"observation_superseded\":%s,\"physical_interruption_observed\":false"
                             : "operation=%lu original=%lu replacement=%lu observation-superseded=%s physical-interruption-observed=false",
                     static_cast<unsigned long>(op.id), static_cast<unsigned long>(op.originalMotion),
                     static_cast<unsigned long>(op.replacementMotion), truth(result.relation.observationSuperseded));
            text.end(op.json);
            if (text.valid && enqueue(text.bytes, false, op.request))
                ++op.valueCursor;
            return false;
        }
        const L::VelocityContext* velocity = motion->nativeVelocity();
        if (op.valueCursor != 3 || !velocity || !velocity->change.present)
            return true;
        if (ordinaryCount_ == ORDINARY_REPLIES)
            return false;
        const L::VelocityChange& change = velocity->change;
        ConsoleText text;
        text.start(op.json, op.request, "velocity_change", op.command);
        text.add(op.json ? ",\"operation_id\":%lu,\"change_index\":1,\"native_rpm\":\"%lld\",\"record\":%lu,"
                           "\"gate_code\":%u,\"execution_code\":%u,\"uncertain\":%s,\"effect_possible\":%s,"
                           "\"observed_cycle\":%lu,\"first_report_rpm\":%ld,\"abort_code\":"
                         : "operation=%lu change=1 native-rpm=%lld record=%lu gate=%u execution=%u "
                           "uncertain=%s effect-possible=%s observed-cycle=%lu first-report-rpm=%ld abort=",
                 static_cast<unsigned long>(op.id), static_cast<long long>(change.rate.effectiveNative),
                 static_cast<unsigned long>(change.evidence.record), unsigned(change.evidence.gate),
                 unsigned(change.execution), truth(change.uncertain), truth(change.effectsPossible),
                 static_cast<unsigned long>(change.firstRequestedRateReport.cycle),
                 static_cast<long>(change.firstPostChange.velocity));
        if (change.serviceEvidencePresent && change.serviceEvidence.outcome == M::CANOPEN::SdoOutcome::SERVER_ABORT)
            text.add("%lu", static_cast<unsigned long>(change.serviceEvidence.abortCode));
        else
            text.add("null");
        if (op.json) {
            text.add(",\"exact\":%s", truth(change.rate.exactArithmetic));
            if (change.rate.exactArithmetic)
                text.add(",\"error\":[\"%llu\",\"%llu\",%s]",
                         static_cast<unsigned long long>(change.rate.quantization.errorNumerator),
                         static_cast<unsigned long long>(change.rate.quantization.errorDenominator),
                         truth(change.rate.quantization.errorNegative));
            else
                text.add(",\"error_bound\":\"%.17g\"", change.rate.absoluteErrorBound);
            text.add(",\"report_present\":%s,\"qualified\":false", truth(change.firstPostChange.present));
        }
        text.end(op.json);
        if (text.valid && enqueue(text.bytes, false, op.request))
            ++op.valueCursor;
        return false;
    }
    if (ordinaryCount_ == ORDINARY_REPLIES)
        return false;
    const MotionValue& value = *values[op.valueCursor];
    ConsoleText text;
    text.start(op.json, op.request, "motion_value", op.command);
    text.add(op.json ? ",\"operation_id\":%lu,\"field\":\"%s\",\"unit\":\"%s\",\"native\":\"%lld\","
                       "\"exact\":%s"
                     : "operation=%lu %s %s native=%lld exact=%s",
             static_cast<unsigned long>(op.id), fields[op.valueCursor],
             op.valueCursor ? "rpm" : "steps", static_cast<long long>(value.native),
             truth(value.exact));
    if (op.valueCursor == 0)
        text.add(op.json ? ",\"relative\":%s,\"endpoint_native\":\"%lld\""
                         : " relative=%s endpoint=%lld",
                 truth(value.relative), static_cast<long long>(value.endpointNative));
    if (op.json && value.exact)
        text.add(",\"%s\":[\"%lld\",\"%llu\",\"%llu\",%s],\"error\":[\"%llu\",\"%llu\",%s]",
                 op.valueCursor ? "requested" : "requested_endpoint",
                 static_cast<long long>(value.requested.integral),
                 static_cast<unsigned long long>(value.requested.numerator),
                 static_cast<unsigned long long>(value.requested.denominator),
                 truth(value.requested.negative),
                 static_cast<unsigned long long>(value.quantization.errorNumerator),
                 static_cast<unsigned long long>(value.quantization.errorDenominator),
                 truth(value.quantization.errorNegative));
    if (!value.exact)
        text.add(op.json ? ",\"error_bound\":\"%.17g\"" : " error-bound=%.17g", value.errorBound);
    text.end(op.json);
    if (text.valid && enqueue(text.bytes, false, op.request))
        ++op.valueCursor;
    return false;
}
bool ProbeConsole::motionTerminal(Operation& op) noexcept {
    ProbeMotion* motion = op.attachment;
    if (!op.terminal) {
        const MotionStopResult* stopped =
            motion && op.stop
                ? motion->stopResult(op.stop == 1 ? L::StopKind::HALT : L::StopKind::QUICK_STOP)
                : nullptr;
        const MotionResult* result = stopped ? &stopped->result
                                     : motion && !op.stop
                                         ? motion->normalResult(op.nativeOperation)
                                         : nullptr;
        if (result && result->id == op.nativeOperation) {
            if (result->state == M::ActionState::ACTIVE || !motionValues(op))
                return false;
        }
        ConsoleText body;
        const bool matched = result && result->id == op.nativeOperation;
        body.add(
            ",\"ok\":%s,\"status\":\"%s\",\"detail\":%ld,\"outcome\":\"%s\",\"uncertain\":%s,"
            "\"command_id\":%lu,\"operation_id\":%lu,\"completed_us\":\"%llu\",\"abort_code\":",
            truth(matched && result->state == M::ActionState::SUCCEEDED),
            matched ? M::errToString(result->status.code) : "INVALID_CONFIG",
            static_cast<long>(matched ? result->status.detail : 0),
            matched ? outcomeName(result->outcome) : "INVALIDATED",
            truth(!matched || result->uncertain), static_cast<unsigned long>(op.request),
            static_cast<unsigned long>(op.id),
            static_cast<unsigned long long>(matched ? result->completedUs : lastUs_));
        if (matched && result->abortKnown)
            body.add("%lu", static_cast<unsigned long>(result->abortCode));
        else
            body.add("null");
        if (matched)
            body.add(",\"execution_code\":%u,\"completion_code\":%u,\"native_operation\":%lu",
                     unsigned(result->execution), unsigned(result->completion),
                     static_cast<unsigned long>(result->nativeOperation));
        // Prompt 55: the native context's retry class. A stop terminal carries its path instead, within
        // the record capacity; every stop is NOT_REPEATABLE, and a new stop is a new operation.
        if (matched && !stopped)
            body.add(",\"retry_class\":\"%s\"",
                     // Prompt 55's letters, as every console terminal prints them.
                     result->retryClass == M::RetryClass::MOTION_LIKE          ? "M"
                     : result->retryClass == M::RetryClass::REPEATABLE_READ    ? "R"
                     : result->retryClass == M::RetryClass::IDEMPOTENT_SETTING ? "W"
                                                                                : "N");
        body.add(",\"axis_id\":%lu", static_cast<unsigned long>(op.axisId));
        if (stopped)
            body.add(",\"stop_path_code\":%u,\"superseded\":%s", unsigned(stopped->path),
                     truth(stopped->superseded));
        if (!body.valid)
            return false;
        std::memcpy(op.terminalRecord, body.bytes, body.used + 1);
        op.terminal = true;
    }
    if (!op.queuedTerminal && !op.delivered) {
        ConsoleText text;
        text.start(op.json, op.request, "terminal", op.command);
        if (op.json)
            text.add("%s", op.terminalRecord);
        else
            text.add("operation %lu%s", static_cast<unsigned long>(op.id), op.terminalRecord);
        text.end(op.json);
        if (text.valid && (op.stop ? enqueueStop(text.bytes, op.attachment, op.stop, op.request, op.id)
                                   : enqueue(text.bytes, false, op.request, op.id)))
            op.queuedTerminal = true;
    }
    return op.terminal;
}
} // namespace MotorControlCANExample
