// SPDX-License-Identifier: MIT
#include "ProbeConsole.h"
#include <MotorControlCAN/Version.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace O = MotorControlCANOwner;
namespace {
using E = ConsoleEffect;
using C = ConsoleCompletion;
enum Command {
    HELP,
    VERSION,
    PROFILE,
    CAPS,
    STATUS,
    HEALTH,
    STATS,
    DRV,
    RESULT,
    RELEASE,
    CANCEL,
    RECOVER,
    AXIS,
    PREPARE,
    CONFIG,
    WIRING,
    MEMORY
};
const ConsoleCommand commands[] = {
    {"help", "help [command]", "consoleCommands", "compiled inventory", E::CACHE, C::REPLY,
     "Show working commands or one command's grammar.", nullptr, 40},
    {"version", "version", "MotorControlCAN::VERSION", "compiled identity", E::CACHE, C::REPLY,
     "Show package and CAN console schema identity.", nullptr, 40},
    {"profile", "profile list", "compiled profile inventory", "lichuan_c only", E::CACHE, C::REPLY,
     "List compiled profiles without detecting hardware.", nullptr, 40},
    {"caps", "caps", "consoleCommands", "compiled inventory", E::CACHE, C::REPLY,
     "List implemented routes and exact remaining gates.", nullptr, 40},
    {"status", "status", "ProbeApp::selected/readContext; owner.resources", "cached evidence",
     E::CACHE, C::REPLY, "Inspect separate cached application and read facts.", nullptr, 40},
    {"health", "health", "ProbeApp::health", "selected binding", E::CACHE, C::REPLY,
     "Inspect cached communication health without a read.", nullptr, 40},
    {"stats", "stats [reset]", "CanBusOwner::stats/clearStats", "interface counters",
     E::HOST_CONFIGURATION, C::REPLY, "Inspect counters; reset clears totals only.", nullptr, 40},
    {"drv", "drv", "CanBusOwner::resources; ProbePlatform::stats", "cached resources", E::CACHE,
     C::REPLY, "Inspect queue, result and console pressure.", nullptr, 40},
    {"result", "result [ID]", "retained read/recovery evidence", "retained operation", E::CACHE,
     C::REPLY, "Inspect without consuming the retained result.", nullptr, 40},
    {"release", "release ID", "ProbeApp::releaseRead; owner.releaseRecovery", "delivered terminal",
     E::HOST_CONFIGURATION, C::REPLY, "Release result storage; never releases a motor.", nullptr,
     40},
    {"cancel", "cancel [ID]", "ProbeApp::cancelRead", "active tracked read", E::LOCAL_CANCEL,
     C::REPLY, "Cancel local read work; this is not motor stop.", nullptr, 40},
    {"recover", "recover", "CanBusOwner::recover", "explicit host recovery", E::HOST_RECOVERY,
     C::ASYNCHRONOUS, "Attempt bounded host recovery without device commands.", nullptr, 40},
    {"axis", "axis config [set FIELD VALUE [MAXIMUM]] | axis origin INTEGER",
     "ProbeApp::executeHostAxis", "offline workspace; origin needs witness", E::HOST_CONFIGURATION,
     C::REPLY, "Configure offline coordinates without drive traffic.", nullptr, 40},
    {"prepare",
     "prepare absolute|relative|angle VALUE UNIT FRAME [BASIS|PATH TIE] [ROUND [MAX_ERROR "
     "[RADIAN_ERROR]]]",
     "preparePosition via ProbeApp::executeHostAxis", "explicit scales and required reference",
     E::PURE, C::REPLY, "Preview exact checked targets; never executes motion.", nullptr, 40},
    {"config", "config", "ProbeApp::hostInterfaceFacts/selected", "host facts", E::CACHE, C::REPLY,
     "Show supplied host interface and selection facts.", nullptr, 40},
    {"wiring", "wiring [di0..di6|do0..do2 unknown|unconnected|connected]",
     "ProbeApp::setWiring/wiring", "host declarations only", E::HOST_CONFIGURATION, C::REPLY,
     "Declare wiring use without changing terminal functions.", nullptr, 40},
    {"memory", "memory", "sizeof application/owner/console; owner.resources",
     "static capacities; runtime high-water unknown", E::CACHE, C::REPLY,
     "Show storage sizes; no invented free-heap measurement.", nullptr, 40},
    {"useaddr", "useaddr", "ProbeApp::select", "explicit host binding", E::HOST_CONFIGURATION,
     C::REPLY, "Select a host binding.", "Device/binding routes belong to prompt 41.", 41},
    {"host", "host", "explicit controller configuration", "established interface facts",
     E::HOST_CONFIGURATION, C::REPLY, "Configure the host interface.",
     "Controller configuration routes belong to prompt 41.", 41},
    {"probe", "probe", "prepareProbe/checkProbe", "DEC-1; unknown read effect", E::DEVICE_READ,
     C::ASYNCHRONOUS, "Explicit minimal typed upload.",
     "Typed device dispatch belongs to prompt 41; no implicit probe.", 41},
    {"read", "read identity|config|state", "ProbeApp typed requests/getters",
     "reviewed acquisition policy", E::DEVICE_READ, C::ASYNCHRONOUS,
     "Read typed device observations.",
     "Typed device dispatch and Q20 acquisition belong to prompt 41.", 41},
    {"discover", "discover", "bounded DiscoveryScan", "explicit target set/current bitrate",
     E::DEVICE_READ, C::ASYNCHRONOUS, "Explicit finite discovery.",
     "Discovery console route belongs to prompt 41.", 41},
    {"settings", "settings", "typed driver/I/O/control APIs", "per-row evidence", E::DEVICE_SETTING,
     C::ASYNCHRONOUS, "Inspect or request reviewed settings.",
     "Settings dispatch belongs to prompt 41; unresolved rows remain gated.", 41},
    {"communication", "communication", "LICHUAN_C::Communication", "exact fixture/procedure",
     E::DEVICE_SETTING, C::ASYNCHRONOUS, "Explicit communication workflow.",
     "Prompt 41 route; node/route activation facts remain unresolved in prompt 38.", 41},
    {"persistence", "persistence", "LICHUAN_C::Persistence", "exact recorded configuration",
     E::DEVICE_ACTION, C::ASYNCHRONOUS, "Explicit save or restore workflow.",
     "Persistence console route belongs to prompt 41; no implicit save/reset.", 41},
    {"motion", "speed|accel|decel|motion|stepsperturn", "explicit simple-move intent",
     "native rate/ramp/scale evidence", E::HOST_CONFIGURATION, C::REPLY,
     "Configure later motion intent.", "Simple-move intent belongs to prompt 42; no boot defaults.",
     42},
    {"move", "moveby|moveto|move relative|absolute|angle", "public finite move lifecycle",
     "reference, scale and trigger evidence", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Request finite motion.", "Motion console integration belongs to prompt 42.", 42},
    {"velocity", "velocity", "public finite velocity lifecycle",
     "native width/unit and stop evidence", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Request finite velocity.", "Velocity console integration belongs to prompt 42.", 42},
    {"home", "home", "public homing lifecycle", "profile method and input evidence",
     E::DEVICE_ACTION, C::ASYNCHRONOUS, "Request reviewed homing.",
     "Homing console integration belongs to prompt 42.", 42},
    {"enable", "enable|motor-release|alarm-clear|position-clear", "public action lifecycles",
     "per-action firmware evidence", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Request a reviewed drive action.",
     "Action console integration belongs to prompt 42; no generic release fallback.", 42},
    {"stop", "stop normal|fast", "public stop and AxisSession", "reviewed stop policy",
     E::DEVICE_ACTION, C::ASYNCHRONOUS, "Explicit drive stop.",
     "Stop console integration/reserved dispatch belongs to prompt 42; cancel is local.", 42},
    {"motor-release", "motor-release", "no reviewed bus-release command",
     "missing native release semantics", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Release drive power only with reviewed semantics.",
     "No manual documents a bus release command (D-17); prompt 42 retains this gate.", 42},
    {"subdivision", "subdivision [INTEGER|options]", "typed driver settings",
     "model/firmware-specific scale", E::DEVICE_SETTING, C::ASYNCHRONOUS,
     "Inspect or request reviewed subdivision.",
     "Prompt 41 route; unresolved 2201 scope/range is not an arbitrary integer setter.", 41},
    {"driver", "driver read|set", "prepareDriverRead/prepareDriverSettings",
     "confirmed native settings", E::DEVICE_SETTING, C::ASYNCHRONOUS, "Typed driver groups.",
     "Typed driver console dispatch belongs to prompt 41; per-field gates apply.", 41},
    {"io", "io read|set", "getIo/prepareIoSettings", "terminal/address evidence", E::DEVICE_SETTING,
     C::ASYNCHRONOUS, "Typed I/O configuration.",
     "Prompt 41 route; output numbering/functions remain gated where contradictory.", 41},
    {"control", "control read|set", "prepareControlRead/prepareControlSettings",
     "confirmed native loop settings", E::DEVICE_SETTING, C::ASYNCHRONOUS,
     "Typed loop configuration.", "Control console integration belongs to prompt 41.", 41},
    {"tuning", "tuning GROUP read|set", "prepareTuningRead/getTuning", "reviewed units/effects",
     E::DEVICE_SETTING, C::ASYNCHRONOUS, "Typed tuning groups.",
     "Prompt 41 route; undocumented tuning tail has no writable capability.", 41},
    {"motion-profile", "motion-profile read|inspect|restore|forget",
     "ConfigurationRestore/typed settings", "retained recorded configuration", E::DEVICE_SETTING,
     C::ASYNCHRONOUS, "Inspect or explicitly reapply recorded settings.",
     "Prompt 41 application route; restore is neither save nor factory reset.", 41},
    {"monitor", "monitor [off|interval count]", "finite typed reads", "reviewed read acquisition",
     E::DEVICE_READ, C::ASYNCHRONOUS, "Bounded explicit monitoring.",
     "Finite monitor console integration belongs to prompt 41.", 41},
    {"debug", "debug [off|raw|decoded]", "CanBusOwner::takeDiagnostic", "passive diagnostic copies",
     E::CACHE, C::REPLY, "Inspect copies of ordinary owner traffic.",
     "Passive trace console routing belongs to prompt 41; no second CAN receiver.", 41},
    {"load", "load", "optional bounded application workload", "application-local load fixture",
     E::HOST_CONFIGURATION, C::REPLY, "Inspect or configure explicit diagnostic load.",
     "Load diagnostics belong to prompt 41; no workload runs by default.", 41},
    {"capture-read", "capture-read", "reviewed finite typed acquisition", "reviewed CAN object set",
     E::DEVICE_READ, C::ASYNCHRONOUS, "Explicit bounded diagnostic read.",
     "Capture console route belongs to prompt 41; it cannot drain controller RX.", 41},
    {"nmt", "nmt", "typed explicit NMT owner lifecycle", "CANopen state/effect prerequisites",
     E::DEVICE_ACTION, C::ASYNCHRONOUS, "Explicit network intent, never motor stop.",
     "CANopen NMT console integration belongs to prompt 41.", 41},
    {"pdo", "pdo", "observed maps/fixed PdoConfiguration subset", "actual map/sample provenance",
     E::DEVICE_SETTING, C::ASYNCHRONOUS, "Inspect or request a reviewed PDO subset.",
     "Prompt 41 route; general remapping remains prompt 18, not EDS permission.", 41},
    {"heartbeat", "heartbeat", "Communication heartbeat workflow", "exact 0-to-1000 review",
     E::DEVICE_SETTING, C::ASYNCHRONOUS, "Explicit reviewed producer setting.",
     "Prompt 41 route; no arbitrary interval or host-loss stop guarantee.", 41},
    {"network", "network", "cached CANopen network evidence", "configured CANopen domain", E::CACHE,
     C::REPLY, "Inspect protocol-specific network facts.",
     "CANopen network console projection belongs to prompt 41.", 41}};
bool equal(const char* a, const char* b) {
    return !std::strcmp(a, b);
}
void increment(uint32_t& value) {
    if (value != UINT32_MAX)
        ++value;
}
bool decimal(const char* text, uint32_t& output) {
    uint32_t value = 0;
    if (!*text)
        return false;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9' || value > (UINT32_MAX - unsigned(*text - '0')) / 10)
            return false;
        value = value * 10 + unsigned(*text - '0');
    }
    if (!value)
        return false;
    output = value;
    return true;
}
M::Status invalid() {
    return M::Status(M::Err::ILLEGAL_VALUE, 0, "invalid console input");
}
const char* boolean(bool value) {
    return value ? "true" : "false";
}
const char* effect(E value) {
    const char* labels[] = {"cache",         "pure",        "host_configuration", "local_cancel",
                            "host_recovery", "device_read", "device_setting",     "device_action"};
    return labels[static_cast<unsigned>(value)];
}
// One bounded candidate; formatting never publishes a truncated prefix.
struct Text {
    char bytes[ProbeConsole::RECORD_CAPACITY] = {};
    size_t used = 0;
    bool valid = true;
    void add(const char* format, ...) {
        if (!valid)
            return;
        va_list arguments;
        va_start(arguments, format);
        const int size = std::vsnprintf(bytes + used, sizeof bytes - used, format, arguments);
        va_end(arguments);
        if (size < 0 || static_cast<size_t>(size) >= sizeof bytes - used)
            valid = false;
        else
            used += static_cast<size_t>(size);
    }
    void string(const char* value) {
        if (!valid)
            return;
        if (!consoleJsonString(value, bytes + used, sizeof bytes - used))
            valid = false;
        else
            used += std::strlen(bytes + used);
    }
    void start(bool json, uint32_t id, const char* type, const char* command) {
        if (json) {
            add("{\"schema\":\"motorcontrolcan.console/"
                "1\",\"protocol\":3,\"profile\":\"lichuan_c\",\"type\":\"%s\",\"id\":%lu,"
                "\"command\":",
                type, static_cast<unsigned long>(id));
            string(command);
        } else
            add("%s: ", command);
    }
    void end(bool json) { add(json ? "}\n" : "\n"); }
};
bool unit(const char* text, M::PositionUnit& result) {
    const char* names[] = {"steps", "fullsteps", "counts", "turn", "deg", "rad", "mm"};
    for (unsigned n = 0; n < 7; ++n)
        if (equal(text, names[n])) {
            result = static_cast<M::PositionUnit>(n);
            return true;
        }
    return false;
}
bool axisCommand(const ConsoleLine& line, HostAxisCommand& request) {
    const size_t count = line.count;
    if (equal(line.token(0), "prepare")) {
        if (count < 5 || count > 10)
            return false;
        request.kind = HostAxisCommandKind::PREPARE;
        M::PositionRequest& p = request.position;
        if (equal(line.token(1), "relative"))
            p.relative = true;
        else if (equal(line.token(1), "angle"))
            p.wrapped = true;
        else if (!equal(line.token(1), "absolute"))
            return false;
        if (!M::parseExactNumber(line.token(2), p.value) || !unit(line.token(3), p.unit))
            return false;
        if (equal(line.token(4), "native"))
            p.frame = M::CoordinateFrame::NATIVE;
        else if (equal(line.token(4), "motor"))
            p.frame = M::CoordinateFrame::MOTOR;
        else if (equal(line.token(4), "load"))
            p.frame = M::CoordinateFrame::LOAD;
        else
            return false;
        size_t next = 5;
        if (p.relative) {
            const char* basis = line.token(next++);
            if (equal(basis, "actual"))
                p.basis = M::RelativeBasis::ACTUAL;
            else if (equal(basis, "commanded"))
                p.basis = M::RelativeBasis::COMMANDED;
            else if (equal(basis, "queued"))
                p.basis = M::RelativeBasis::QUEUED;
            else
                return false;
        }
        if (p.wrapped) {
            const char* path = line.token(next++);
            const char* tie = line.token(next++);
            if (equal(path, "positive"))
                p.path = M::AnglePath::POSITIVE;
            else if (equal(path, "negative"))
                p.path = M::AnglePath::NEGATIVE;
            else if (!equal(path, "shortest"))
                return false;
            if (equal(tie, "positive"))
                p.tie = M::HalfTurnTie::POSITIVE;
            else if (equal(tie, "negative"))
                p.tie = M::HalfTurnTie::NEGATIVE;
            else if (!equal(tie, "reject"))
                return false;
        }
        if (next < count) {
            const char* round = line.token(next++);
            if (equal(round, "exact"))
                p.rounding = M::Rounding::EXACT;
            else if (equal(round, "nearest"))
                p.rounding = M::Rounding::NEAREST;
            else if (equal(round, "zero"))
                p.rounding = M::Rounding::TOWARD_ZERO;
            else if (equal(round, "floor"))
                p.rounding = M::Rounding::FLOOR;
            else if (equal(round, "ceil"))
                p.rounding = M::Rounding::CEIL;
            else
                return false;
        }
        if (next < count && (!M::parseExactNumber(line.token(next++), p.maximumQuantizationError) ||
                             p.maximumQuantizationError.numerator < 0))
            return false;
        if (next < count) {
            M::Rational allowance;
            if (p.unit != M::PositionUnit::RADIANS ||
                !M::parseExactNumber(line.token(next++), allowance) || allowance.numerator <= 0 ||
                !M::rationalToDouble(allowance, p.maximumApproximationError))
                return false;
            p.approximate = true;
        }
        return next == count;
    }
    if (count == 2 && equal(line.token(1), "config"))
        return true;
    if (count == 3 && equal(line.token(1), "origin")) {
        request.kind = HostAxisCommandKind::ORIGIN;
        return M::parseExactNumber(line.token(2), request.value) && request.value.denominator == 1;
    }
    if ((count != 5 && count != 6) || !equal(line.token(1), "config") ||
        !equal(line.token(2), "set"))
        return false;
    const char* fields[] = {"command",       "gear",          "fullsteps",     "lead",
                            "encoder-scale", "encoder-id",    "encoder-basis", "encoder-polarity",
                            "polarity",      "native-limits", "soft-limits",   "relative-bases"};
    size_t field = 0;
    for (; field < sizeof fields / sizeof fields[0] && !equal(line.token(3), fields[field]);
         ++field) {
    }
    if (field == sizeof fields / sizeof fields[0])
        return false;
    request.kind = HostAxisCommandKind::CONFIGURE;
    request.field = static_cast<HostAxisField>(field);
    const bool limits = request.field == HostAxisField::NATIVE_LIMITS ||
                        request.field == HostAxisField::SOFT_LIMITS;
    if (equal(line.token(4), "none")) {
        request.clear = true;
        return count == 5 && (field <= 4 || request.field == HostAxisField::SOFT_LIMITS);
    }
    if (count != (limits ? 6 : 5))
        return false;
    if (request.field == HostAxisField::ENCODER_BASIS) {
        if (equal(line.token(4), "motor"))
            request.encoderBasis = M::EncoderBasis::MOTOR_TURN;
        else if (equal(line.token(4), "load"))
            request.encoderBasis = M::EncoderBasis::LOAD_TURN;
        else if (equal(line.token(4), "linear"))
            request.encoderBasis = M::EncoderBasis::MILLIMETRE;
        else
            return false;
        return true;
    }
    return M::parseExactNumber(line.token(4), request.value) &&
           (!limits || M::parseExactNumber(line.token(5), request.secondValue));
}
} // namespace

const ConsoleCommand* consoleCommands(size_t& count) noexcept {
    count = sizeof commands / sizeof commands[0];
    return commands;
}
M::Status parseConsoleLine(const char* bytes, size_t size, ConsoleLine& output) noexcept {
    if (!bytes || !size || size >= sizeof output.text)
        return invalid();
    ConsoleLine candidate;
    for (size_t n = 0; n < size; ++n) {
        const unsigned char c = static_cast<unsigned char>(bytes[n]);
        if ((c < 32 && c != '\t') || c > 126)
            return invalid();
        candidate.text[n] = static_cast<char>(c);
    }
    size_t next = 0;
    bool first = true;
    while (next < size) {
        while (next < size && (candidate.text[next] == ' ' || candidate.text[next] == '\t'))
            candidate.text[next++] = 0;
        if (next == size)
            break;
        const size_t begin = next;
        while (next < size && candidate.text[next] != ' ' && candidate.text[next] != '\t')
            ++next;
        if (next < size)
            candidate.text[next++] = 0;
        if (first && candidate.text[begin] == '@') {
            if (!decimal(candidate.text + begin + 1, candidate.id))
                return invalid();
            candidate.json = true;
        } else {
            if (candidate.count == sizeof candidate.offsets / sizeof candidate.offsets[0])
                return invalid();
            candidate.offsets[candidate.count++] = static_cast<uint16_t>(begin);
        }
        first = false;
    }
    if (!candidate.count)
        return invalid();
    output = candidate;
    return M::Ok();
}
bool consoleJsonString(const char* text, char* output, size_t capacity) noexcept {
    if (!text || !output)
        return false;
    size_t size = 2, length = 0;
    for (; length < 512 && text[length]; ++length) {
        const unsigned char c = static_cast<unsigned char>(text[length]);
        if (c > 127)
            return false;
        size += c < 32 ? 6 : (c == '"' || c == '\\' ? 2 : 1);
    }
    if (length == 512 || size >= capacity)
        return false;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(text),
                    destination = reinterpret_cast<uintptr_t>(output);
    if ((destination >= begin && destination - begin <= length) ||
        (begin > destination && begin - destination < size + 1))
        return false;
    size_t at = 0;
    output[at++] = '"';
    const char* hex = "0123456789abcdef";
    for (size_t n = 0; n < length; ++n) {
        const unsigned char c = static_cast<unsigned char>(text[n]);
        if (c < 32) {
            output[at++] = '\\';
            output[at++] = 'u';
            output[at++] = '0';
            output[at++] = '0';
            output[at++] = hex[c >> 4];
            output[at++] = hex[c & 15];
        } else {
            if (c == '"' || c == '\\')
                output[at++] = '\\';
            output[at++] = static_cast<char>(c);
        }
    }
    output[at++] = '"';
    output[at] = 0;
    return true;
}

ProbeConsole::ProbeConsole(ProbeApp& app, CanBusOwner& owner, ProbePlatform& platform) noexcept
    : app_(app), owner_(owner), platform_(platform) {}
ConsoleStats ProbeConsole::stats() const noexcept {
    ConsoleStats result = stats_;
    result.queued = ordinaryCount_ + controlCount_;
    return result;
}
bool ProbeConsole::drained() const noexcept {
    if (used_ || discard_ || listing_.active || ordinaryCount_ || controlCount_)
        return false;
    for (const Operation& op : operations_)
        if (op.used && (!op.terminal || !op.delivered))
            return false;
    return true;
}
bool ProbeConsole::enqueue(const char* text, bool control, uint32_t request,
                           uint32_t terminal) noexcept {
    Record* records = control ? control_ : ordinary_;
    uint8_t& count = control ? controlCount_ : ordinaryCount_;
    const unsigned limit = control ? CONTROL_REPLIES : ORDINARY_REPLIES;
    const size_t size = std::strlen(text);
    if (!size || size >= RECORD_CAPACITY || text[size - 1] != '\n' || count == limit)
        return false;
    std::memcpy(records[count].text, text, size + 1);
    records[count].length = static_cast<uint16_t>(size);
    records[count].request = request;
    records[count].terminal = terminal;
    ++count;
    return true;
}
void ProbeConsole::error(uint32_t id, bool json, const char* command, const char* reason,
                         bool control) noexcept {
    Text text;
    text.start(json, id, "reply", command);
    if (json) {
        text.add(",\"ok\":false,\"status\":\"ILLEGAL_VALUE\",\"error\":");
        text.string(reason);
    } else
        text.add("rejected (%s)", reason);
    text.end(json);
    if (!text.valid || !enqueue(text.bytes, control, id))
        increment(stats_.outputDropped);
}
bool ProbeConsole::requestInUse(uint32_t id) const noexcept {
    if (!id)
        return false;
    for (const Operation& op : operations_)
        if (op.used && op.request == id)
            return true;
    for (unsigned n = 0; n < ordinaryCount_; ++n)
        if (ordinary_[n].request == id)
            return true;
    for (unsigned n = 0; n < controlCount_; ++n)
        if (control_[n].request == id)
            return true;
    return listing_.active && listing_.json && listing_.id == id;
}
ProbeConsole::Operation* ProbeConsole::operation(uint32_t id) noexcept {
    Operation* latest = nullptr;
    for (Operation& op : operations_) {
        if (op.used && op.id == id)
            return &op;
        if (op.used && (!latest || op.id > latest->id))
            latest = &op;
    }
    return id ? nullptr : latest;
}
bool ProbeConsole::trackRead(uint32_t requestId, bool json, uint32_t operationId) noexcept {
    const auto& read = app_.readContext();
    if (!operationId || operationId <= nextOperation_ || (json ? !requestId : requestId != 0) ||
        requestInUse(requestId) || read.state != M::ReadState::ACTIVE)
        return false;
    for (const Operation& op : operations_)
        if (op.used && !op.recovery && op.readOperation == read.operation)
            return false;
    Operation* available = nullptr;
    for (Operation& op : operations_)
        if (!op.used) {
            available = &op;
            break;
        }
    if (!available || ordinaryCount_ == ORDINARY_REPLIES)
        return false;
    Text reply;
    reply.start(json, requestId, "reply", "read");
    reply.add(json ? ",\"ok\":true,\"status\":\"OK\",\"result\":\"accepted\",\"operation_id\":%lu"
                   : "accepted operation %lu",
              static_cast<unsigned long>(operationId));
    reply.end(json);
    if (!reply.valid || !enqueue(reply.bytes, false, requestId))
        return false;
    *available = Operation();
    available->used = true;
    available->json = json;
    available->request = requestId;
    available->id = operationId;
    available->readOperation = read.operation;
    nextOperation_ = operationId;
    return true;
}
void ProbeConsole::feed(uint8_t byte, uint64_t now) noexcept {
    if (now < lastUs_)
        return;
    lastUs_ = now;
    if (byte == '\r' || byte == '\n') {
        if (byte == '\n' && previousCr_) {
            previousCr_ = false;
            return;
        }
        previousCr_ = byte == '\r';
        if (!used_ && !discard_)
            return;
        increment(stats_.lines);
        ConsoleLine parsed;
        if (discard_ || !parseConsoleLine(line_, used_, parsed)) {
            uint32_t id = 0;
            char prefix[12] = {};
            size_t n = 0;
            if (line_[0] == '@') {
                while (n < 10 && n + 1 < used_ && line_[n + 1] >= '0' && line_[n + 1] <= '9') {
                    prefix[n] = line_[n + 1];
                    ++n;
                }
                if (n + 1 == used_ || line_[n + 1] == ' ' || line_[n + 1] == '\t')
                    decimal(prefix, id);
            }
            increment(stats_.invalid);
            error(id, id != 0, "input", "invalid_line");
        } else
            dispatch(parsed, now);
        used_ = 0;
        discard_ = false;
        line_[0] = 0;
        return;
    }
    previousCr_ = false;
    if (discard_)
        return;
    if ((byte < 32 && byte != '\t') || byte > 126 || used_ == LINE_CAPACITY - 1) {
        discard_ = true;
        return;
    }
    line_[used_++] = static_cast<char>(byte);
    line_[used_] = 0;
}
void ProbeConsole::listNext() noexcept {
    if (!listing_.active || ordinaryCount_ == ORDINARY_REPLIES)
        return;
    if (listing_.axis && listing_.next < 4) {
        const auto& c = listing_.configuration;
        Text text;
        text.start(listing_.json, listing_.id, "axis", "axis");
        const auto scale = [&](const char* name, const M::UnitScale& value) {
            text.add(listing_.json ? ",\"%s\":" : "%s=", name);
            if (value.source == M::ScaleSource::UNKNOWN)
                text.add("null");
            else
                text.add(listing_.json ? "[\"%lu\",\"%lu\",%u]" : "%lu/%lu(source=%u) ",
                         static_cast<unsigned long>(value.numerator),
                         static_cast<unsigned long>(value.denominator), unsigned(value.source));
        };
        if (listing_.next == 0) {
            if (listing_.json)
                text.add(",\"part\":\"geometry\"");
            scale("command_scale", c.units.commandStepsPerMotorTurn);
            scale("gear", c.units.motorTurnsPerLoadTurn);
            scale("fullsteps", c.units.fullStepsPerMotorTurn);
            scale("lead", c.units.millimetresPerLoadTurn);
        } else if (listing_.next == 1) {
            if (listing_.json)
                text.add(",\"part\":\"feedback\"");
            scale("encoder_scale", c.units.encoder.countsPerUnit);
            text.add(listing_.json ? ",\"encoder_id\":%lu,\"encoder_basis\":%u,\"encoder_"
                                     "polarity\":%d,\"polarity\":%d"
                                   : "encoder-id=%lu basis=%u polarity=%d command-polarity=%d",
                     static_cast<unsigned long>(c.units.encoder.sourceId),
                     unsigned(c.units.encoder.basis), int(c.units.encoder.polarity),
                     int(c.units.commandPolarity));
        } else if (listing_.next == 2) {
            text.add(listing_.json ? ",\"part\":\"limits\",\"native_minimum\":\"%lld\",\"native_"
                                     "maximum\":\"%lld\",\"relative_bases\":%u,\"soft_limits\":"
                                   : "native=[%lld,%lld] relative-bases=%u soft=",
                     static_cast<long long>(c.nativeMinimum),
                     static_cast<long long>(c.nativeMaximum), unsigned(c.supportedRelativeBases));
            if (c.softLimitsKnown)
                text.add(listing_.json ? "[\"%lld\",\"%lld\"]" : "[%lld,%lld]",
                         static_cast<long long>(c.softMinimum),
                         static_cast<long long>(c.softMaximum));
            else
                text.add("null");
        } else {
            text.add(
                listing_.json
                    ? ",\"part\":\"provenance\",\"offline\":true,\"configured\":%s,\"configuration_"
                      "generation\":%lu,\"reference_known\":%s,\"origin_native\":"
                    : "offline configured=%s generation=%lu reference=%s origin=",
                boolean(listing_.configured),
                static_cast<unsigned long>(c.binding.configurationGeneration),
                boolean(listing_.referenceKnown));
            if (c.originKnown)
                text.add(listing_.json ? "\"%lld\"" : "%lld",
                         static_cast<long long>(c.originNative));
            else
                text.add("null");
        }
        text.end(listing_.json);
        if (text.valid && enqueue(text.bytes, false, listing_.id))
            ++listing_.next;
        return;
    }
    size_t count = 0;
    consoleCommands(count);
    if (listing_.axis)
        listing_.next = count;
    while (listing_.next < count && !listing_.caps && commands[listing_.next].gate)
        ++listing_.next;
    Text text;
    if (listing_.next == count) {
        text.start(listing_.json, listing_.id, "reply",
                   listing_.axis   ? "axis"
                   : listing_.caps ? "caps"
                                   : "help");
        text.add(listing_.json ? ",\"ok\":true,\"status\":\"OK\",\"result\":\"complete\""
                               : "complete");
        text.end(listing_.json);
        if (text.valid && enqueue(text.bytes, false, listing_.id))
            listing_.active = false;
        return;
    }
    const ConsoleCommand& row = commands[listing_.next];
    text.start(listing_.json, listing_.id, listing_.caps ? "capability" : "help", row.name);
    if (listing_.json) {
        text.add(",\"implemented\":%s,\"owner\":%u,\"effect\":\"%s\",\"completion\":\"%s\",\"%s\":",
                 boolean(!row.gate), unsigned(row.owner), effect(row.effect),
                 row.completion == C::REPLY ? "reply" : "asynchronous",
                 listing_.caps ? "reason" : "grammar");
        text.string(listing_.caps ? (row.gate ? row.gate : row.capability) : row.grammar);
    } else
        text.add("%s%s%s", listing_.caps ? (row.gate ? "gated: " : "implemented: ") : "",
                 listing_.caps ? row.name : row.grammar,
                 row.gate ? " (see caps JSON for reason)" : "");
    text.end(listing_.json);
    if (text.valid && enqueue(text.bytes, false, listing_.id))
        ++listing_.next;
    else if (!text.valid) {
        listing_.active = false;
        error(listing_.id, listing_.json, "help", "record_capacity");
    }
}
void ProbeConsole::poll(uint64_t now) noexcept {
    for (Operation& op : operations_) {
        if (!op.used)
            continue;
        if (op.deliveryPending &&
            uint32_t(platform_.stats().outputWritten - op.deliveryEnd) < UINT32_C(0x80000000)) {
            op.delivered = true;
            op.deliveryPending = false;
        }
        if (!op.terminal) {
            Text body;
            bool succeeded = false, uncertain = false;
            const char* outcome = nullptr;
            const char* status = "INVALID_CONFIG";
            int32_t detail = 0;
            uint64_t completed = now;
            uint32_t abortCode = 0;
            bool abortKnown = false;
            if (op.recovery) {
                const O::RecoveryCompletion* terminal = owner_.recoveryResult(op.recoveryId);
                if (!terminal)
                    continue;
                succeeded = terminal->outcome == O::RecoveryOutcome::RETIRED;
                outcome = succeeded                                          ? "RETIRED"
                          : terminal->outcome == O::RecoveryOutcome::TIMEOUT ? "TIMEOUT"
                                                                             : "FAILED";
                status = succeeded ? "OK" : "PROTOCOL_ERROR";
                detail = static_cast<int32_t>(terminal->outcome);
                completed = terminal->completedUs;
            } else {
                const auto& read = app_.readContext();
                if (read.operation == op.readOperation && read.state == M::ReadState::ACTIVE)
                    continue;
                if (read.operation != op.readOperation || read.state == M::ReadState::EMPTY) {
                    outcome = "INVALIDATED";
                    uncertain = true;
                } else {
                    succeeded = read.state == M::ReadState::SUCCEEDED;
                    const char* labels[] = {"NONE",
                                            "SUCCESS",
                                            "REPLY_ERROR",
                                            "PROTOCOL_REJECTED",
                                            "TRANSPORT_ERROR",
                                            "CANCELLED",
                                            "DEADLINE",
                                            "INVALIDATED",
                                            "SERVICE_TIMEOUT"};
                    const unsigned index = static_cast<unsigned>(read.outcome);
                    outcome = index < sizeof labels / sizeof labels[0] ? labels[index] : "unknown";
                    status = M::errToString(read.status.code);
                    detail = read.status.detail;
                    uncertain = read.uncertain;
                    completed = read.servicedUs;
                    abortKnown = read.evidencePresent && read.evidence.abortValid;
                    abortCode = read.evidence.abortCode;
                }
            }
            body.add(",\"ok\":%s,\"status\":\"%s\",\"detail\":%ld,\"outcome\":\"%s\",\"uncertain\":"
                     "%s,\"command_"
                     "id\":%lu,\"operation_id\":%lu,\"completed_us\":\"%llu\",\"abort_code\":",
                     boolean(succeeded), status, static_cast<long>(detail), outcome,
                     boolean(uncertain), static_cast<unsigned long>(op.request),
                     static_cast<unsigned long>(op.id), static_cast<unsigned long long>(completed));
            if (abortKnown)
                body.add("%lu", static_cast<unsigned long>(abortCode));
            else
                body.add("null");
            if (!body.valid)
                continue;
            std::memcpy(op.terminalRecord, body.bytes, body.used + 1);
            op.terminal = true;
        }
        if (!op.queuedTerminal && !op.delivered && ordinaryCount_ < ORDINARY_REPLIES) {
            // Admission receipt is already queued before polling may enqueue a terminal.
            Text text;
            text.start(op.json, op.request, "terminal", op.recovery ? "recover" : "read");
            if (op.json)
                text.add("%s", op.terminalRecord);
            else
                text.add("operation %lu%s", static_cast<unsigned long>(op.id), op.terminalRecord);
            text.end(op.json);
            if (text.valid && enqueue(text.bytes, false, op.request, op.id))
                op.queuedTerminal = true;
        }
    }
}
void ProbeConsole::service(uint64_t now) noexcept {
    if (now < lastUs_)
        return;
    lastUs_ = now;
    if (platform_.takeInputLoss()) {
        increment(stats_.inputLost);
        // Every buffered byte predates the observed loss. An earlier queued newline
        // cannot establish the end of the damaged command: retire that whole prefix,
        // then wait for a terminator received after the loss boundary.
        uint8_t retired = 0;
        for (unsigned n = 0; n < ProbePlatform::INPUT_CAPACITY && platform_.takeInput(retired);
             ++n) {
        }
        used_ = 0;
        line_[0] = 0;
        previousCr_ = false;
        discard_ = true;
    }
    poll(now);
    Record* records = controlCount_ ? control_ : ordinary_;
    uint8_t& count = controlCount_ ? controlCount_ : ordinaryCount_;
    if (count && platform_.writeOutput(reinterpret_cast<const uint8_t*>(records[0].text),
                                       records[0].length)) {
        if (records[0].terminal) {
            Operation* op = operation(records[0].terminal);
            if (op) {
                const ProbeIoStats io = platform_.stats();
                op->deliveryEnd = io.outputWritten + io.outputUsed;
                op->deliveryPending = true;
            }
        }
        for (unsigned n = 1; n < count; ++n)
            records[n - 1] = records[n];
        --count;
    }
    listNext();
    for (unsigned n = 0; n < INPUT_BUDGET; ++n) {
        uint8_t byte = 0;
        if (!platform_.takeInput(byte))
            break;
        feed(byte, now);
    }
}

void ProbeConsole::dispatch(const ConsoleLine& line, uint64_t now) noexcept {
    const bool json = line.json;
    const uint32_t id = line.id;
    const char* name = line.token(0);
    const bool control = equal(name, "cancel") || equal(name, "recover");
    if (requestInUse(id)) {
        error(id, json, name, "duplicate_id", control);
        return;
    }
    if ((control ? controlCount_ == CONTROL_REPLIES : ordinaryCount_ == ORDINARY_REPLIES)) {
        increment(stats_.outputDropped);
        return;
    }
    size_t index = 0;
    for (; index <= MEMORY && !equal(name, commands[index].name); ++index) {
    }
    if (index > MEMORY) {
        increment(stats_.invalid);
        error(id, json, name, "unknown_command");
        return;
    }
    const Command command = static_cast<Command>(index);
    const size_t count = line.count;
    Text text;
    text.start(json, id, "reply", name);
    const auto reject = [&](const char* reason) {
        increment(stats_.invalid);
        error(id, json, name, reason, control);
    };
    const auto ok = [&]() {
        if (json)
            text.add(",\"ok\":true,\"status\":\"OK\"");
        else
            text.add("OK ");
    };
    switch (command) {
    case HELP: {
        if (count > 2) {
            reject("invalid_arguments");
            return;
        }
        if (count == 2) {
            size_t selected = 0;
            for (; selected <= MEMORY && !equal(line.token(1), commands[selected].name);
                 ++selected) {
            }
            if (selected > MEMORY) {
                reject("unknown_command");
                return;
            }
            ok();
            if (json) {
                text.add(",\"grammar\":");
                text.string(commands[selected].grammar);
                text.add(",\"help\":");
                text.string(commands[selected].help);
            } else
                text.add("%s - %s", commands[selected].grammar, commands[selected].help);
        } else {
            if (listing_.active) {
                reject("listing_busy");
                return;
            }
            listing_.active = true;
            listing_.caps = false;
            listing_.axis = false;
            listing_.json = json;
            listing_.id = id;
            listing_.next = 0;
            return;
        }
        break;
    }
    case CAPS:
        if (count != 1 || listing_.active) {
            reject(count != 1 ? "invalid_arguments" : "listing_busy");
            return;
        }
        listing_.active = true;
        listing_.caps = true;
        listing_.axis = false;
        listing_.json = json;
        listing_.id = id;
        listing_.next = 0;
        return;
    case VERSION:
        if (count != 1) {
            reject("invalid_arguments");
            return;
        }
        ok();
        if (json)
            text.add(
                ",\"product\":\"MotorControl-CAN\",\"version\":\"%s\",\"operation_capacity\":%u",
                M::VERSION, unsigned(OPERATION_CAPACITY));
        else
            text.add("MotorControl-CAN %s; protocol 3; schema motorcontrolcan.console/1",
                     M::VERSION);
        break;
    case PROFILE:
        if (count != 2 || !equal(line.token(1), "list")) {
            reject("invalid_arguments");
            return;
        }
        ok();
        text.add(json ? ",\"profiles\":[{\"name\":\"lichuan_c\",\"protocol\":\"canopen\",\"model_"
                        "match\":null,\"firmware_match\":null,\"hardware_qualified\":false}],"
                        "\"motor_kind\":null,\"feedback\":null,\"supported_modes\":null"
                      : "lichuan_c (CANopen); model/firmware/motor/feedback/modes unknown; no "
                        "qualification claim");
        break;
    case CONFIG: {
        if (count != 1) {
            reject("invalid_arguments");
            return;
        }
        ok();
        const auto& facts = app_.hostInterfaceFacts();
        const auto* selected = app_.selected();
        if (json) {
            text.add(",\"interface\":");
            if (facts.interfaceKnown)
                text.add("%lu", static_cast<unsigned long>(facts.interfaceId));
            else
                text.add("null");
            text.add(",\"bitrate_bits_per_second\":");
            if (facts.bitrateKnown)
                text.add("%lu", static_cast<unsigned long>(facts.bitrateBitsPerSecond));
            else
                text.add("null");
            text.add(",\"selected_node\":");
            if (selected)
                text.add("%lu",
                         static_cast<unsigned long>(selected->channel.binding.endpoint.address));
            else
                text.add("null");
            text.add(",\"bus_traffic\":false");
        } else
            text.add("interface %s; bitrate %s; selected %s; host facts only",
                     facts.interfaceKnown ? "known" : "unknown",
                     facts.bitrateKnown ? "known" : "unknown", selected ? "yes" : "no");
        break;
    }
    case STATUS: {
        if (count != 1) {
            reject("invalid_arguments");
            return;
        }
        ok();
        const auto resources = owner_.resources();
        const char* states[] = {"EMPTY", "ACTIVE", "SUCCEEDED", "FAILED"};
        const auto& read = app_.readContext();
        const unsigned state = static_cast<unsigned>(read.state);
        text.add(json ? ",\"selected\":%s,\"read_state\":\"%s\",\"uncertain\":%s,\"controller_"
                        "inhibited\":%s,\"nmt\":null,\"drive_state\":null,\"mode\":null,\"position_"
                        "valid\":null"
                      : "selected=%s read=%s uncertain=%s controller_inhibited=%s; "
                        "NMT/drive/mode/position not projected",
                 boolean(app_.selected() != nullptr), state < 4 ? states[state] : "UNKNOWN",
                 boolean(read.uncertain), boolean(resources.inhibited));
        break;
    }
    case HEALTH: {
        if (count != 1) {
            reject("invalid_arguments");
            return;
        }
        Health health;
        const bool known = app_.health(health);
        ok();
        if (!known) {
            text.add(json ? ",\"known\":false,\"state\":null"
                          : "unknown binding; health unavailable");
            break;
        }
        const char* states[] = {"UNINIT", "READY", "DEGRADED", "OFFLINE"};
        const unsigned state = static_cast<unsigned>(health.state);
        text.add(json ? ",\"known\":true,\"state\":\"%s\",\"totalSuccess\":%lu,\"totalFailures\":%"
                        "lu,\"consecutiveFailures\":%u,\"lastError\":\"%s\",\"lastSeenUs\":"
                      : "%s services %lu/%lu streak %u lastError=%s lastSeenUs=",
                 state < 4 ? states[state] : "UNKNOWN",
                 static_cast<unsigned long>(health.totalSuccess),
                 static_cast<unsigned long>(health.totalFailures),
                 unsigned(health.consecutiveFailures), M::errToString(health.lastError.code));
        if (health.lastSeenKnown)
            text.add(json ? "\"%llu\"" : "%llu",
                     static_cast<unsigned long long>(health.lastSeenUs));
        else
            text.add("null");
        if (json) {
            text.add(",\"lastOkUs\":");
            if (health.lastOkKnown)
                text.add("\"%llu\"", static_cast<unsigned long long>(health.lastOkUs));
            else
                text.add("null");
            text.add(",\"lastErrorUs\":");
            if (health.lastErrorKnown)
                text.add("\"%llu\"", static_cast<unsigned long long>(health.lastErrorUs));
            else
                text.add("null");
        }
        break;
    }
    case STATS: {
        if (count > 2 || (count == 2 && !equal(line.token(1), "reset"))) {
            reject("invalid_arguments");
            return;
        }
        if (count == 2 && !owner_.clearStats()) {
            reject("busy");
            return;
        }
        ok();
        const auto& s = owner_.stats();
        // A positional array keeps the complete named contract within one bounded record.
        text.add(
            json ? ",\"counters\":[%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu],"
                   "\"maxServiceGapUs\":\"%llu\""
                 : "tx=%lu rx=%lu txRejected=%lu rxDropped=%lu rxLost=%lu unmatched=%lu "
                   "queueRejected=%lu resultsRejected=%lu cancelled=%lu timeouts=%lu busOff=%lu "
                   "recoveries=%lu traceOverwritten=%lu consoleDropped=%lu maxGapUs=%llu",
            static_cast<unsigned long>(s.txFrames), static_cast<unsigned long>(s.rxFrames),
            static_cast<unsigned long>(s.txRejected), static_cast<unsigned long>(s.rxDropped),
            static_cast<unsigned long>(s.rxLost), static_cast<unsigned long>(s.unmatchedFrames),
            static_cast<unsigned long>(s.queueRejected),
            static_cast<unsigned long>(s.resultsRejected), static_cast<unsigned long>(s.cancelled),
            static_cast<unsigned long>(s.timeouts), static_cast<unsigned long>(s.busOffEvents),
            static_cast<unsigned long>(s.recoveries),
            static_cast<unsigned long>(s.traceOverwritten),
            static_cast<unsigned long>(stats_.outputDropped),
            static_cast<unsigned long long>(s.maxServiceGapUs));
        break;
    }
    case DRV:
    case MEMORY: {
        if (count != 1) {
            reject("invalid_arguments");
            return;
        }
        ok();
        const auto r = owner_.resources();
        if (command == DRV) {
            const auto io = platform_.stats();
            text.add(json ? ",\"queued\":%u,\"active\":%u,\"retained\":%u,\"reservations\":%u,"
                            "\"inputDropped\":%lu,\"outputRejected\":%lu,\"consoleDropped\":%lu"
                          : "queued=%u active=%u retained=%u reservations=%u inputDropped=%lu "
                            "outputRejected=%lu consoleDropped=%lu",
                     unsigned(r.queued), unsigned(r.active), unsigned(r.retained),
                     unsigned(r.reservations), static_cast<unsigned long>(io.inputDropped),
                     static_cast<unsigned long>(io.outputRejected),
                     static_cast<unsigned long>(stats_.outputDropped));
        } else
            text.add(json ? ",\"app_bytes\":\"%llu\",\"owner_bytes\":\"%llu\",\"console_bytes\":\"%"
                            "llu\",\"platform_bytes\":\"%llu\",\"line_capacity\":256,\"record_"
                            "capacity\":512,\"runtime_stack_high_water\":null,\"free_heap\":null"
                          : "app=%llu owner=%llu console=%llu platform=%llu bytes; line=256 "
                            "record=512; runtime stack/heap unknown",
                     static_cast<unsigned long long>(sizeof app_),
                     static_cast<unsigned long long>(sizeof owner_),
                     static_cast<unsigned long long>(sizeof *this),
                     static_cast<unsigned long long>(sizeof platform_));
        break;
    }
    case WIRING: {
        if (count != 1 && count != 3) {
            reject("invalid_arguments");
            return;
        }
        if (count == 3) {
            const char* pin = line.token(1);
            unsigned slot = 10;
            if (std::strlen(pin) == 3 && pin[0] == 'd') {
                if (pin[1] == 'i' && pin[2] >= '0' && pin[2] <= '6')
                    slot = unsigned(pin[2] - '0');
                if (pin[1] == 'o' && pin[2] >= '0' && pin[2] <= '2')
                    slot = 7 + unsigned(pin[2] - '0');
            }
            HostWiringUse use = HostWiringUse::UNKNOWN;
            if (equal(line.token(2), "connected"))
                use = HostWiringUse::CONNECTED;
            else if (equal(line.token(2), "unconnected"))
                use = HostWiringUse::UNUSED;
            else if (!equal(line.token(2), "unknown")) {
                reject("invalid_arguments");
                return;
            }
            if (slot == 10 || !app_.setHostWiring(static_cast<uint8_t>(slot), use)) {
                reject("invalid_terminal");
                return;
            }
        }
        ok();
        text.add(json ? ",\"declarations\":[" : "declarations (unknown/unconnected/connected): ");
        for (unsigned n = 0; n < 10; ++n) {
            HostWiringUse use;
            app_.hostWiring(static_cast<uint8_t>(n), use);
            const char* label = use == HostWiringUse::CONNECTED ? "connected"
                                : use == HostWiringUse::UNUSED  ? "unconnected"
                                                                : "unknown";
            text.add(json ? "%s\"%s\"" : "%s%s", n ? "," : "", label);
        }
        if (json)
            text.add("],\"electrical_levels\":null,\"functions\":null,\"bus_traffic\":false");
        break;
    }
    case AXIS:
    case PREPARE: {
        if (command == AXIS && count == 2 && equal(line.token(1), "config") && listing_.active) {
            reject("listing_busy");
            return;
        }
        HostAxisCommand request;
        if (!axisCommand(line, request)) {
            reject("invalid_arguments");
            return;
        }
        HostAxisView view;
        const M::Status status = app_.executeHostAxis(request, now, view);
        if (status && request.kind == HostAxisCommandKind::QUERY && view.configured) {
            listing_.active = listing_.axis = true;
            listing_.caps = false;
            listing_.json = json;
            listing_.id = id;
            listing_.next = 0;
            listing_.configuration = view.configuration;
            listing_.configured = view.configured;
            listing_.referenceKnown = view.referenceKnown;
            return;
        }
        if (!status) {
            if (json) {
                text.add(",\"ok\":false,\"status\":\"%s\",\"detail\":%ld,\"error\":",
                         M::errToString(status.code), static_cast<long>(status.detail));
                text.string(status.msg ? status.msg : "axis_rejected");
            } else
                text.add("rejected %s (%ld): %s", M::errToString(status.code),
                         static_cast<long>(status.detail),
                         status.msg ? status.msg : "axis_rejected");
        } else {
            ok();
            if (command == PREPARE) {
                const auto& p = view.prepared;
                if (json) {
                    text.add(",\"offline\":true,\"exact_arithmetic\":%s,\"effective_native\":",
                             boolean(p.exactArithmetic));
                    if (p.representation == M::NativeRepresentation::INTEGER)
                        text.add("\"%lld\"", static_cast<long long>(p.effectiveNative));
                    else
                        text.add("null");
                    text.add(",\"effective_float\":");
                    if (p.representation == M::NativeRepresentation::BINARY32)
                        text.add("%.9g", static_cast<double>(p.effectiveFloat));
                    else
                        text.add("null");
                    if (p.exactArithmetic && p.representation == M::NativeRepresentation::INTEGER)
                        text.add(",\"error_numerator\":\"%llu\",\"error_denominator\":\"%llu\","
                                 "\"error_negative\":%s",
                                 static_cast<unsigned long long>(p.quantization.errorNumerator),
                                 static_cast<unsigned long long>(p.quantization.errorDenominator),
                                 boolean(p.quantization.errorNegative));
                    else
                        text.add(",\"error_numerator\":null,\"error_denominator\":null,\"error_"
                                 "negative\":null");
                    text.add(",\"error_bound\":%.17g", p.absoluteErrorBound);
                } else {
                    if (p.representation == M::NativeRepresentation::INTEGER)
                        text.add("offline native=%lld", static_cast<long long>(p.effectiveNative));
                    else
                        text.add("offline binary32=%.9g", static_cast<double>(p.effectiveFloat));
                    if (p.exactArithmetic && p.representation == M::NativeRepresentation::INTEGER)
                        text.add("; quantization %s%llu/%llu",
                                 p.quantization.errorNegative ? "-" : "+",
                                 static_cast<unsigned long long>(p.quantization.errorNumerator),
                                 static_cast<unsigned long long>(p.quantization.errorDenominator));
                    text.add("; bound %.17g", p.absoluteErrorBound);
                }
            } else {
                text.add(
                    json ? ",\"offline\":true,\"configured\":%s,\"reference_known\":%s,"
                           "\"configuration_generation\":%lu,\"origin_native\":"
                         : "offline configured=%s reference=%s generation=%lu origin=",
                    boolean(view.configured), boolean(view.referenceKnown),
                    static_cast<unsigned long>(view.configuration.binding.configurationGeneration));
                if (view.configured && view.configuration.originKnown)
                    text.add(json ? "\"%lld\"" : "%lld",
                             static_cast<long long>(view.configuration.originNative));
                else
                    text.add("null");
            }
        }
        break;
    }
    case RECOVER: {
        if (count != 1) {
            reject("invalid_arguments");
            return;
        }
        if (now > UINT64_MAX - 1000000 || nextOperation_ == UINT32_MAX) {
            reject("time_or_id_exhausted");
            return;
        }
        Operation* op = nullptr;
        for (Operation& candidate : operations_)
            if (!candidate.used) {
                op = &candidate;
                break;
            }
        if (!op) {
            reject("retained_results_full");
            return;
        }
        O::RecoveryId recovery;
        if (!owner_.recover(now, now + 1000000, recovery)) {
            reject("recovery_rejected");
            return;
        }
        *op = Operation();
        op->used = op->recovery = true;
        op->json = json;
        op->request = id;
        op->recoveryId = recovery;
        op->id = ++nextOperation_;
        ok();
        text.add(json ? ",\"result\":\"accepted\",\"operation_id\":%lu" : "accepted operation %lu",
                 static_cast<unsigned long>(op->id));
        break;
    }
    case RESULT:
    case RELEASE:
    case CANCEL: {
        uint32_t operationId = 0;
        if (count > 2 || (command == RELEASE && count != 2) ||
            (count == 2 && !decimal(line.token(1), operationId))) {
            reject("invalid_arguments");
            return;
        }
        Operation* op = operation(operationId);
        if (!op) {
            if (command == RESULT && count == 1) {
                ok();
                text.add(json ? ",\"result\":\"none\"" : "no retained operation");
                break;
            }
            reject("unknown_operation");
            return;
        }
        if (command == RESULT) {
            if (op->terminal) {
                if (json)
                    text.add("%s", op->terminalRecord);
                else
                    text.add("operation %lu%s", static_cast<unsigned long>(op->id),
                             op->terminalRecord);
            } else {
                ok();
                text.add(json ? ",\"result\":\"pending\",\"operation_id\":%lu,\"command_id\":%lu"
                              : "pending operation %lu command %lu",
                         static_cast<unsigned long>(op->id),
                         static_cast<unsigned long>(op->request));
            }
        } else if (command == RELEASE) {
            const bool originalReadStillPresent = app_.readContext().state != M::ReadState::EMPTY &&
                                                  app_.readContext().operation == op->readOperation;
            if (!op->terminal || !op->delivered ||
                (op->recovery ? !owner_.releaseRecovery(op->recoveryId)
                              : originalReadStillPresent && !app_.releaseRead())) {
                reject("result_not_releasable");
                return;
            }
            *op = Operation();
            ok();
        } else {
            if (op->recovery || op->terminal || app_.readContext().operation != op->readOperation ||
                !app_.cancelRead(now)) {
                reject("not_cancellable");
                return;
            }
            ok();
            text.add(json ? ",\"motor_stop\":false" : "local only; motor not stopped");
        }
        break;
    }
    }
    text.end(json);
    if (!text.valid) {
        error(id, json, name, "record_capacity", control);
        return;
    }
    if (!enqueue(text.bytes, control, id))
        increment(stats_.outputDropped);
}
} // namespace MotorControlCANExample
