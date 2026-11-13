// SPDX-License-Identifier: MIT
#include "ProbeConsole.h"
#include "ProbeConsoleText.h"
#include <MotorControlCAN/Version.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace O = MotorControlCANOwner;
namespace L = MotorControlCAN::LICHUAN_C;
namespace N = MotorControlCAN::CANOPEN;
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
    MEMORY,
    USEADDR,
    HOST,
    PROBE,
    READ,
    DISCOVER,
    SETTINGS,
    COMMUNICATION,
    PERSISTENCE,
    MOTION,
    MOVE,
    VELOCITY,
    HOME,
    ENABLE,
    STOP,
    MOTOR_RELEASE,
    SUBDIVISION,
    DRIVER,
    IO,
    CONTROL,
    TUNING,
    MOTION_PROFILE,
    MONITOR,
    DEBUG,
    LOAD,
    CAPTURE_READ,
    NMT,
    PDO,
    HEARTBEAT,
    NETWORK
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
    {"useaddr", "useaddr NODE [SUBAXIS]", "ProbeDevice::selectAddress",
     "existing CANopen binding only", E::HOST_CONFIGURATION, C::REPLY,
     "Select an established endpoint without wire work.", nullptr, 41},
    {"host", "host SLOT", "ProbeDevice::selectSlot", "existing application slot",
     E::HOST_CONFIGURATION, C::REPLY,
     "Select an existing host binding; no controller or drive change.", nullptr, 41},
    {"probe", "probe [TIMEOUT_MS]", "ProbeApp::requestOrdinaryRead/checkProbe",
     "DEC-1 accepted; effects remain unknown", E::DEVICE_READ, C::ASYNCHRONOUS,
     "Explicit minimal 1000 upload; no identity sweep.", nullptr, 41},
    {"read", "read identity|config|state [BLOCK] [TIMEOUT_MS]",
     "ProbeApp::requestOrdinaryRead/typed getters", "DEC-1 whole-plan gate before admission",
     E::DEVICE_READ, C::ASYNCHRONOUS,
     "Read raw typed facts; config and unaccepted blocks are Q20-gated.", nullptr, 41},
    {"discover",
     "discover passive|probe|identity TIMEOUT_MS SLOT [SLOT] | discover inspect [ORDINAL]|cancel",
     "DiscoveryScan", "configured slots/current bitrate/finite budget", E::DEVICE_READ,
     C::ASYNCHRONOUS, "Explicit finite discovery; findings never select a model.", nullptr, 41},
    {"settings", "settings", "typed driver/I/O/control APIs", "per-row evidence", E::DEVICE_SETTING,
     C::ASYNCHRONOUS, "Inspect or request reviewed settings.",
     "No generic settings union; use the typed driver, io, control and tuning groups.", 41},
    {"communication", "communication heartbeat 1000 [TIMEOUT_MS]",
     "ProbeCommissioning/prepareCommunication",
     "exact external typed preparation and one-use service authority", E::DEVICE_SETTING,
     C::ASYNCHRONOUS,
     "Consume the exact externally reviewed heartbeat plan; no arbitrary interval or route change.",
     nullptr, 41},
    {"persistence",
     "persistence save parameters [TIMEOUT_MS] | persistence restore factory settings [TIMEOUT_MS]",
     "ProbeCommissioning/prepareSave/prepareFactoryRestore",
     "exact external typed preparation and one-use service authority", E::DEVICE_ACTION,
     C::ASYNCHRONOUS, "One explicit reviewed intent; ACK is not durability, restart or rollback.",
     nullptr, 41},
    {"motion", "motion [write|verify]", "setSimpleSetup", "explicit host intent",
     E::HOST_CONFIGURATION, C::REPLY, "Inspect or choose native setup; verify writes differences.",
     nullptr, 42},
    {"move",
     "move relative|absolute VALUE UNIT FRAME RPM configured [setup write|verify] [basis "
     "actual|commanded|queued] [round POLICY ERROR [approx ERROR]] | move angle VALUE UNIT FRAME "
     "PATH TIE RPM configured [setup write|verify] [round POLICY ERROR [approx ERROR]]",
     "ProbeMotion::requestMove/AxisSession",
     "actual axis; explicit ramps, firmware and reference evidence", E::DEVICE_ACTION,
     C::ASYNCHRONOUS, "Prepare and request checked finite position or wrapped-angle motion.",
     nullptr, 42},
    {"velocity",
     "velocity read | velocity VALUE RATE FRAME DURATION_MS configured normal|fast [round POLICY "
     "ERROR [approx ERROR]]",
     "makeFiniteVelocity/ProbeMotion::requestVelocity",
     "integer motor rpm; finite duration; native start/reverse/stop gates", E::DEVICE_ACTION,
     C::ASYNCHRONOUS, "Request finite velocity; native means the configured rate dimension.",
     nullptr, 42},
    {"home", "home methods | home read | home METHOD SEARCH_RPM RETURN_RPM RAMP_MS zero",
     "ProbeMotion::requestHome/prepareDriverRead",
     "exact method, input, parameter and completion qualification", E::DEVICE_ACTION,
     C::ASYNCHRONOUS, "Inspect methods or explicitly request qualified zero-offset homing.",
     nullptr, 42},
    {"enable", "enable", "ProbeMotion::requestEnable", "explicit per-firmware enable evidence",
     E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Request reviewed enable; never implied by motion or startup.", nullptr, 42},
    {"stop", "stop normal|fast", "ProbeMotion::requestStop/AxisSession",
     "explicit bounded stop policy", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Request HALT or QUICK_STOP; independent retained result, never local cancel.", nullptr, 42},
    {"motor-release", "motor-release", "no reviewed bus-release command",
     "missing native release semantics", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Release drive power only with reviewed semantics.",
     "No manual documents a bus release command (D-17); prompt 42 retains this gate.", 42},
    {"subdivision", "subdivision [INTEGER|options]", "typed driver settings",
     "model/firmware-specific scale", E::DEVICE_SETTING, C::ASYNCHRONOUS,
     "Inspect or request reviewed subdivision.",
     "Prompt 41 route; unresolved 2201 scope/range is not an arbitrary integer setter.", 41},
    {"driver",
     "driver read driver|soft-limits|motion-parameters|velocity-target|homing|persistence | driver "
     "set RPM UP_MS DOWN_MS | driver limits 0|1",
     "prepareDriverRead/ProbeSettings::requestDriver/requestSoftLimits",
     "external fresh prerequisites; Q20 ordinary reads", E::DEVICE_SETTING, C::ASYNCHRONOUS,
     "Typed native groups; writes require an attached reviewed settings session.", nullptr, 41},
    {"io", "io read inputs|outputs|filters|command | io set POLARITY F0 F1 F2 F3 F4 F5 F6",
     "prepareDriverRead/ProbeSettings::requestIo",
     "full DI candidate; external fresh prerequisites; DO actions gated", E::DEVICE_SETTING,
     C::ASYNCHRONOUS, "Read raw groups or explicitly assign all DI functions/polarity.", nullptr,
     41},
    {"control", "control read | control set 0|1|2",
     "prepareControlRead/ProbeSettings::requestControl",
     "external fresh model/encoder/state prerequisites", E::DEVICE_SETTING, C::ASYNCHRONOUS,
     "Read loop words or explicitly select reviewed 2500 only.", nullptr, 41},
    {"tuning", "tuning current-holding|filter-position|loop-gains|eds-gains read",
     "prepareTuningRead/getTuning", "Q20 acquisition; no reviewed tuning setter", E::DEVICE_SETTING,
     C::ASYNCHRONOUS,
     "Explicit typed read plan; ordinary unaccepted fields are rejected before work.", nullptr, 41},
    {"motion-profile", "motion-profile read|inspect|restore|forget",
     "ConfigurationRecord application storage",
     "recorded three-field snapshot; acquisition Q20-gated", E::DEVICE_SETTING, C::ASYNCHRONOUS,
     "Inspect or forget a host record; no save or factory restore.", nullptr, 41},
    {"monitor", "monitor INTERVAL_MS COUNT [TIMEOUT_MS] | monitor off",
     "ProbeDevice::monitor/nextMonitor", "finite STATUS samples; DEC-1; 1..16 samples",
     E::DEVICE_READ, C::ASYNCHRONOUS,
     "Explicit bounded sampling; output pressure delays new samples without renewing deadline.",
     nullptr, 41},
    {"debug", "debug [off|raw|decoded]", "ProbeDevice::diagnostic",
     "bounded owner copies; capture owns its drain", E::CACHE, C::REPLY,
     "Copy passive trace; never receives CAN or sends requests.", nullptr, 41},
    {"load", "load", "CanBusOwner::stats/resources",
     "cached bounded workload/resource measurements", E::CACHE, C::REPLY,
     "Inspect service load; does not generate device traffic.", nullptr, 41},
    {"capture-read", "capture-read", "DeviceEvidence::request",
     "externally supplied exact finite plan; DEC-1", E::DEVICE_READ, C::ASYNCHRONOUS,
     "Acquire a configured diagnostic read; no raw object access.", nullptr, 41},
    {"nmt", "nmt start|stop|preop|reset-node|reset-communication addressed|broadcast [TIMEOUT_MS]",
     "CanBusOwner::admitNmt/nmtResult/releaseNmt",
     "configured CANopen domain; explicit manager intent", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Request network state; controller TX is not remote confirmation or motor stop.", nullptr, 41},
    {"pdo", "pdo SLOT | pdo apply map3-to-map2 [TIMEOUT_MS] | pdo apply timer 0|100 [TIMEOUT_MS]",
     "SdoClient::pdo; ProbeCommissioning::requestPdo",
     "cached evidence or exact externally reviewed TPDO1 plan", E::DEVICE_SETTING, C::ASYNCHRONOUS,
     "Inspect cached maps or explicitly apply the reviewed TPDO1 subset; samples stay unqualified.",
     nullptr, 41},
    {"heartbeat", "heartbeat", "SdoClient::node/heartbeatFreshness",
     "cached producer/receive evidence", E::CACHE, C::REPLY,
     "Inspect heartbeat without enabling/configuring it.", nullptr, 41},
    {"network", "network", "SdoClient::node/serviceEligibility",
     "cached exact state and independent serviceability", E::CACHE, C::REPLY,
     "Inspect CANopen network facts; no state promotion.", nullptr, 41},
    {"speed", "speed [VALUE [rpm]]", "setSimpleSpeed", "explicit integer motor-rpm destination",
     E::HOST_CONFIGURATION, C::REPLY, "Inspect/set exact host speed intent; no device write.",
     nullptr, 42},
    {"accel", "accel [VALUE [ms]]", "setSimpleRamp", "native ramp-up milliseconds",
     E::HOST_CONFIGURATION, C::REPLY,
     "Inspect/set host ramp-up time; zero remains unusable for motion.", nullptr, 42},
    {"decel", "decel [VALUE [ms]]", "setSimpleRamp", "native ramp-down milliseconds",
     E::HOST_CONFIGURATION, C::REPLY,
     "Inspect/set host ramp-down time; no physical acceleration conversion.", nullptr, 42},
    {"stepsperturn", "stepsperturn POSITIVE_NUMBER",
     "ProbeMotion::configureCommandScale/configureAxis",
     "explicit stationary witness and live host axis", E::HOST_CONFIGURATION, C::REPLY,
     "Set assumed command increments per motor turn; never encoder geometry.", nullptr, 42},
    {"moveby", "moveby VALUE [steps|deg|turn|mm]", "buildSimpleMove/ProbeMotion::requestMove",
     "explicit speed, ramps, scale and actual-position basis", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Request relative host intent without boot defaults.", nullptr, 42},
    {"moveto", "moveto VALUE [steps|deg|turn|mm]", "buildSimpleMove/ProbeMotion::requestMove",
     "explicit host origin, speed, ramps and scale", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Request an absolute host target; axis origin is required.", nullptr, 42},
    {"alarm-clear", "alarm-clear", "ProbeMotion::requestAlarmClear",
     "fresh acquired state and native action gates", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Explicit alarm clear, 6040 0x0080 then 0; success never re-enables motion.", nullptr, 42},
    {"position-clear", "position-clear", "ProbeMotion::requestPositionClear",
     "stationary state and native counter-clear gates", E::DEVICE_ACTION, C::ASYNCHRONOUS,
     "Clear device counter to zero; no save or host-origin shortcut.", nullptr, 42},
    {"motion stored", "motion stored", "no stored-only native setup policy",
     "stored-only lifecycle unavailable", E::HOST_CONFIGURATION, C::REPLY,
     "Use write or verify only with explicit intent.",
     "The native API only offers write-all or verify-and-update; stored-only is not implemented.",
     42},
    {"home qualification", "home qualification", "HomeEvidence/setHomePrerequisites",
     "exact measured native record", E::HOST_CONFIGURATION, C::REPLY,
     "No console-created qualification.",
     "Q04/Q11/Q14/Q21 require exact method, wiring and completion evidence; CLI cannot attest a "
     "measured record.",
     42},
    {"velocity qualification", "velocity qualification",
     "VelocityEvidence/setVelocityPrerequisites", "exact measured native record",
     E::HOST_CONFIGURATION, C::REPLY, "No console-created qualification.",
     "Native start/reverse/stop qualification requires exact firmware evidence supplied by the "
     "application.",
     42}};
bool equal(const char* a, const char* b) {
    return !std::strcmp(a, b);
}
bool sameReadTarget(const L::ReadTarget& a, const L::ReadTarget& b) {
    return a.nodeGeneration == b.nodeGeneration &&
           M::sameBinding(a.channel.binding, b.channel.binding) &&
           a.channel.clientToServerId == b.channel.clientToServerId &&
           a.channel.serverToClientId == b.channel.serverToClientId;
}
void increment(uint32_t& value) {
    if (value != UINT32_MAX)
        ++value;
}
bool unsignedDecimal(const char* text, uint32_t& output) {
    if (!*text)
        return false;
    uint32_t candidate = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9' || candidate > (UINT32_MAX - unsigned(*text - '0')) / 10)
            return false;
        candidate = candidate * 10 + unsigned(*text - '0');
    }
    output = candidate;
    return true;
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
using Text = ConsoleText;
void findingFields(Text& text, bool json, const DiscoveryFinding& finding) {
    text.add(json ? ",\"address\":%lu,\"outcome_code\":%u,\"confidence\":%u,\"binding_current\":%s,"
                    "\"unresolved\":%s,\"suspected_duplicate\":%s,\"raw_identity\":["
                  : "address=%lu outcome=%u confidence=%u binding-current=%s unresolved=%s "
                    "suspected-duplicate=%s raw=[",
             static_cast<unsigned long>(finding.requested.target.channel.binding.endpoint.address),
             unsigned(finding.outcome), unsigned(finding.confidence),
             boolean(finding.bindingCurrent), boolean(finding.unresolved),
             boolean(finding.suspectedDuplicate));
    if (finding.read.kind == L::ReadKind::IDENTITY)
        for (unsigned n = 0; n < finding.read.count; ++n) {
            if (n)
                text.add(",");
            if (finding.read.values[n].valid)
                text.add(json ? "\"%lu\"" : "%lu",
                         static_cast<unsigned long>(finding.read.values[n].raw));
            else
                text.add("null");
        }
    text.add("]");
}
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
    result.queued =
        ordinaryCount_ + controlCount_ + unsigned(stopReplyUsed_[0]) + unsigned(stopReplyUsed_[1]);
    return result;
}
bool ProbeConsole::drained() const noexcept {
    if (used_ || discard_ || listing_.active || ordinaryCount_ || controlCount_ ||
        stopReplyUsed_[0] || stopReplyUsed_[1])
        return false;
    for (const Operation& op : operations_)
        if (op.used && (!op.terminal || !op.delivered))
            return false;
    return true;
}
bool ProbeConsole::retainsOperation() const noexcept {
    for (const Operation& op : operations_)
        if (op.used)
            return true;
    return false;
}
bool ProbeConsole::detachMotion() noexcept {
    // A retained motion operation names its attachment's results by identity; it keeps that attachment.
    for (const Operation& op : operations_)
        if (op.used && op.motion)
            return false;
    motion_ = nullptr;
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
                         bool control, M::Err code, int32_t detail) noexcept {
    Text text;
    text.start(json, id, "reply", command);
    if (json) {
        text.add(",\"ok\":false,\"status\":\"%s\",\"detail\":%ld,\"error\":", M::errToString(code),
                 static_cast<long>(detail));
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
    for (unsigned n = 0; n < 2; ++n)
        if (stopReplyUsed_[n] && stopReplies_[n].request == id)
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
    for (unsigned slot = 0; slot < OPERATION_CAPACITY; ++slot)
        if (!operations_[slot].used) {
            available = &operations_[slot];
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
        if (listing_.caps && listing_.json) {
            M::DiscoveryCapabilities c;
            if (M::getDiscoveryCapabilities(M::DriveProfile::LICHUAN_C, c))
                text.add(",\"probe\":%s,\"identity\":%s,\"non_changing\":%s,\"can_match_exact_"
                         "model\":%s,"
                         "\"can_match_firmware\":%s,\"model_match\":null,\"firmware_match\":null,"
                         "\"endpoint_range\":[%lu,%lu],\"motor_kind\":null,"
                         "\"feedback\":null,\"hardware_qualified\":false",
                         boolean(c.probe), boolean(c.identity), boolean(c.nonChanging),
                         boolean(c.exactModel), boolean(c.firmware),
                         static_cast<unsigned long>(c.minimumAddress),
                         static_cast<unsigned long>(c.maximumAddress));
        }
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
        if (op.motion) {
            motionTerminal(op);
            continue;
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
            } else if (op.settings) {
                if (!settings_ || !settings_->retained() ||
                    settings_->context().operation != op.nativeOperation) {
                    outcome = "INVALIDATED";
                    uncertain = true;
                } else {
                    settings_->service(now);
                    if (settings_->active())
                        continue;
                    const auto& result = settings_->context();
                    succeeded = result.state == L::SettingsState::SUCCEEDED;
                    outcome = succeeded ? "SETTINGS_READBACK_MATCHED" : "SETTINGS_FAILED";
                    status = M::errToString(result.status.code);
                    detail = result.confirmed;
                    uncertain = result.uncertain;
                    completed = result.servicedUs;
                    abortKnown = result.evidencePresent && result.evidence.abortValid;
                    abortCode = result.evidence.abortCode;
                }
            } else if (op.commissioning) {
                if (!commissioning_ || !commissioning_->retained() ||
                    (commissioning_->communication() ? commissioning_->communication()->operation
                     : commissioning_->persistence() ? commissioning_->persistence()->operation
                     : commissioning_->pdo()         ? commissioning_->pdo()->operation
                                                     : 0) != op.nativeOperation) {
                    outcome = "INVALIDATED";
                    uncertain = true;
                } else {
                    commissioning_->service(now);
                    if (commissioning_->active())
                        continue;
                    if (const auto* c = commissioning_->communication()) {
                        succeeded = c->state == L::CommunicationState::SUCCEEDED;
                        outcome = succeeded ? "READBACK_MATCHED" : "COMMUNICATION_FAILED";
                        status = M::errToString(c->status.code);
                        detail = static_cast<int32_t>(c->outcome);
                        uncertain = c->uncertain;
                        completed = c->servicedUs;
                    } else if (const auto* c = commissioning_->persistence()) {
                        succeeded = c->state == L::PersistenceState::ACKNOWLEDGED;
                        outcome = succeeded ? "ACKNOWLEDGED" : "PERSISTENCE_FAILED";
                        status = M::errToString(c->status.code);
                        detail = static_cast<int32_t>(c->outcome);
                        uncertain = c->uncertain;
                        completed = c->servicedUs;
                    } else if (const auto* c = commissioning_->pdo()) {
                        succeeded = c->state == L::PdoConfigurationState::SUCCEEDED;
                        outcome = succeeded ? "PDO_READBACK_MATCHED" : "PDO_CONFIGURATION_FAILED";
                        status = M::errToString(c->status.code);
                        detail = c->acknowledgedMask;
                        uncertain = c->uncertain;
                        completed = c->servicedUs;
                    }
                    const auto* terminal = commissioning_->lastCompletion();
                    abortKnown = terminal && terminal->abortValid;
                    abortCode = abortKnown ? terminal->abortCode : 0;
                }
            } else if (op.nmtId.owner) {
                const auto* terminal = owner_.nmtResult(op.nmtId);
                if (!terminal)
                    continue;
                succeeded = terminal->transmissionConfirmed;
                uncertain = terminal->transmissionPossible;
                const char* labels[] = {"TRANSMITTED",    "QUEUE_REJECTED", "TIMEOUT",
                                        "CANCELLED",      "BUS_OFF",        "INVALIDATED",
                                        "TRANSPORT_ERROR"};
                const unsigned index = static_cast<unsigned>(terminal->outcome);
                outcome = index < sizeof labels / sizeof labels[0] ? labels[index]
                                                                   : "UNKNOWN_NMT_OUTCOME";
                status = succeeded ? "OK" : "PROTOCOL_ERROR";
                detail = static_cast<int32_t>(terminal->outcome);
                completed = terminal->completedUs;
            } else if (op.discovery) {
                if (!device_ || device_->discoveryOperation() != op.nativeOperation ||
                    (!device_->discovery().active() && !device_->discovery().complete())) {
                    outcome = "INVALIDATED";
                    uncertain = true;
                } else {
                    if (!device_->discovery().complete())
                        continue;
                    const auto& summary = device_->discovery().summary();
                    if (op.valueCursor < summary.targets) {
                        if (ordinaryCount_ < ORDINARY_REPLIES) {
                            const auto* finding = device_->discovery().result(op.valueCursor);
                            Text record;
                            record.start(op.json, op.request, "finding", op.command);
                            record.add(op.json ? ",\"operation_id\":%lu,\"ordinal\":%u"
                                               : "operation=%lu ordinal=%u ",
                                       static_cast<unsigned long>(op.id), unsigned(op.valueCursor));
                            if (finding)
                                findingFields(record, op.json, *finding);
                            record.end(op.json);
                            if (record.valid && enqueue(record.bytes, false, op.request))
                                ++op.valueCursor;
                        }
                        continue;
                    }
                    succeeded = !summary.cancelled && !summary.interfaceChanged;
                    outcome = "DISCOVERY_FINISHED";
                    status = "OK";
                    detail = summary.unresolvedSlots;
                }
            } else {
                if (op.monitor && device_ && device_->monitoring() &&
                    app_.readContext().state == M::ReadState::EMPTY)
                    continue;
                if (op.monitor && device_ &&
                    app_.readContext().operation == device_->monitorOperation() &&
                    app_.readContext().operation != op.readOperation &&
                    app_.readContext().state != M::ReadState::EMPTY) {
                    op.readOperation = app_.readContext().operation;
                    op.valueCursor = 0;
                }
                if (op.automatic && app_.readContext().operation == op.readOperation &&
                    app_.pendingRequest().owner)
                    app_.advanceRead(now);
                const auto& read = app_.readContext();
                if (read.operation == op.readOperation && read.state == M::ReadState::ACTIVE)
                    continue;
                if (read.operation != op.readOperation || read.state == M::ReadState::EMPTY) {
                    if (op.monitor && device_ && device_->monitoring())
                        device_->stopMonitor(now);
                    outcome = "INVALIDATED";
                    uncertain = true;
                } else {
                    if ((op.automatic || op.capture) && !readValues(op, now))
                        continue;
                    if (op.monitor && device_ && device_->nextMonitor(now))
                        continue;
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
            if (op.monitor && device_ && !device_->monitoring() &&
                device_->monitorOutcome() != M::ReadOutcome::NONE) {
                const auto result = device_->monitorOutcome();
                succeeded = result == M::ReadOutcome::SUCCESS;
                outcome = succeeded                             ? "SUCCESS"
                          : result == M::ReadOutcome::CANCELLED ? "CANCELLED"
                          : result == M::ReadOutcome::DEADLINE  ? "DEADLINE"
                                                                : "MONITOR_FAILED";
                status = succeeded ? "OK" : "PROTOCOL_ERROR";
                detail = device_->monitorSamples();
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
            if (op.settings && settings_ && settings_->retained() &&
                settings_->context().operation == op.nativeOperation) {
                const auto& settings = settings_->context();
                body.add(",\"confirmed_mask\":%u,\"changed_mask\":%u,\"effects_possible\":%s",
                         unsigned(settings.confirmed), unsigned(settings.changed),
                         boolean(settings.effectsPossible));
            }
            if (op.commissioning && commissioning_ && commissioning_->retained() &&
                (commissioning_->communication() ? commissioning_->communication()->operation
                 : commissioning_->persistence() ? commissioning_->persistence()->operation
                 : commissioning_->pdo()         ? commissioning_->pdo()->operation
                                                 : 0) == op.nativeOperation) {
                if (const auto* c = commissioning_->communication())
                    body.add(",\"write_acknowledged\":%s,\"readback_matched\":%s,\"restart\":null,"
                             "\"persistent\":null",
                             boolean(c->writeAcknowledged), boolean(c->readbackMatched));
                else if (const auto* c = commissioning_->persistence())
                    body.add(",\"execution_code\":%u,\"attempts\":%u,\"durability_known\":false,"
                             "\"restart\":null",
                             unsigned(c->execution), unsigned(c->attempts));
                else if (const auto* c = commissioning_->pdo())
                    body.add(",\"acknowledged_mask\":%u,\"map_confirmed\":%s,\"samples_qualified\":"
                             "false",
                             unsigned(c->acknowledgedMask), boolean(c->mapConfirmed));
            }
            if (op.capture && device_ && device_->evidence() && device_->evidence()->record() &&
                device_->evidence()->record()->plan.id == op.nativeOperation) {
                const auto& record = *device_->evidence()->record();
                body.add(",\"acquisition_id\":%lu,\"evidence_lost\":%s,\"traffic_count\":%u,"
                         "\"dropped_traffic\":%lu",
                         static_cast<unsigned long>(record.plan.id), boolean(record.evidenceLost),
                         unsigned(record.trafficCount),
                         static_cast<unsigned long>(record.droppedTraffic));
            }
            if (!body.valid)
                continue;
            std::memcpy(op.terminalRecord, body.bytes, body.used + 1);
            op.terminal = true;
        }
        if (!op.queuedTerminal && !op.delivered && ordinaryCount_ < ORDINARY_REPLIES) {
            // Admission receipt is already queued before polling may enqueue a terminal.
            Text text;
            text.start(op.json, op.request, "terminal", op.recovery ? "recover" : op.command);
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
    if (device_)
        device_->advance(now);
    if (motion_)
        motion_->service(now);
    poll(now);
    diagnostic(now);
    const int stopSlot = stopReplyUsed_[1] ? 1 : stopReplyUsed_[0] ? 0 : -1;
    Record* records = stopSlot >= 0   ? &stopReplies_[stopSlot]
                      : controlCount_ ? control_
                                      : ordinary_;
    uint8_t one = 1;
    uint8_t& count = stopSlot >= 0 ? one : controlCount_ ? controlCount_ : ordinaryCount_;
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
        if (stopSlot >= 0)
            stopReplyUsed_[stopSlot] = false;
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
    const bool stop = equal(name, "stop");
    if (!stop &&
        (control ? controlCount_ == CONTROL_REPLIES : ordinaryCount_ == ORDINARY_REPLIES)) {
        increment(stats_.outputDropped);
        return;
    }
    size_t index = 0;
    for (; index < sizeof commands / sizeof commands[0] && !equal(name, commands[index].name);
         ++index) {
    }
    if (index == sizeof commands / sizeof commands[0] || commands[index].gate) {
        increment(stats_.invalid);
        error(id, json, name, "unknown_command");
        return;
    }
    if ((equal(name, "home") || equal(name, "velocity")) && line.count == 2 &&
        equal(line.token(1), "read")) {
        dispatchDevice(line, now);
        return;
    }
    if (equal(name, "motion") || equal(name, "move") || equal(name, "moveby") ||
        equal(name, "moveto") || equal(name, "speed") || equal(name, "accel") ||
        equal(name, "decel") || equal(name, "stepsperturn") || equal(name, "velocity") ||
        equal(name, "home") || equal(name, "enable") || equal(name, "alarm-clear") ||
        equal(name, "position-clear") || stop ||
        (equal(name, "axis") && equal(line.token(1), "origin") && motion_)) {
        dispatchMotion(line, now);
        return;
    }
    if (index > MEMORY) {
        dispatchDevice(line, now);
        return;
    }
    dispatchLocal(line, now, index);
}
void ProbeConsole::dispatchLocal(const ConsoleLine& line, uint64_t now, size_t index) noexcept {
    const bool json = line.json;
    const uint32_t id = line.id;
    const char* name = line.token(0);
    const bool control = equal(name, "cancel") || equal(name, "recover");
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
            for (; selected < sizeof commands / sizeof commands[0] &&
                   !equal(line.token(1), commands[selected].name);
                 ++selected) {
            }
            if (selected == sizeof commands / sizeof commands[0] || commands[selected].gate) {
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
    case PROFILE: {
        if (count != 2 || !equal(line.token(1), "list")) {
            reject("invalid_arguments");
            return;
        }
        M::DiscoveryProfile profile;
        M::DiscoveryCapabilities capabilities;
        if (!M::getDiscoveryProfile(0, profile) ||
            !M::getDiscoveryCapabilities(profile.profile, capabilities)) {
            reject("compiled_inventory_invalid");
            return;
        }
        ok();
        if (json) {
            text.add(",\"profiles\":[{\"name\":\"%s\",\"protocol\":\"canopen\",\"model_match\":"
                     "null,\"firmware_match\":null,\"hardware_qualified\":false}],\"motor_kind\":"
                     "null,\"feedback\":null,\"supported_modes\":null,\"probe\":%s,\"identity\":%s,"
                     "\"selected_protocol\":%s",
                     profile.profileId, boolean(capabilities.probe), boolean(capabilities.identity),
                     app_.selected() ? "\"canopen\"" : "null");
        } else
            text.add("%s (CANopen); raw probe/identity; selected=%s; model/firmware/motor/feedback "
                     "unknown; unqualified",
                     profile.profileId, boolean(app_.selected() != nullptr));
        break;
    }
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
        Operation* op = &operations_[TOTAL_OPERATION_CAPACITY - 1];
        if (op->used) {
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
            const MotionStopResult* oldStop =
                motion_ && op->stop ? motion_->stopResult(op->stop == 1 ? L::StopKind::HALT
                                                                        : L::StopKind::QUICK_STOP)
                                    : nullptr;
            const bool originalMotion =
                motion_ && (op->stop ? oldStop && oldStop->result.id == op->nativeOperation
                                     : motion_->retained() &&
                                           motion_->normalResult().id == op->nativeOperation);
            const bool originalReadStillPresent = app_.readContext().state != M::ReadState::EMPTY &&
                                                  app_.readContext().operation == op->readOperation;
            const bool originalSettings = settings_ && settings_->retained() &&
                                          settings_->context().operation == op->nativeOperation;
            const bool originalCommissioning =
                commissioning_ && commissioning_->retained() &&
                (commissioning_->communication() ? commissioning_->communication()->operation
                 : commissioning_->persistence() ? commissioning_->persistence()->operation
                 : commissioning_->pdo()         ? commissioning_->pdo()->operation
                                                 : 0) == op->nativeOperation;
            const bool originalDiscovery =
                device_ && device_->discoveryOperation() == op->nativeOperation &&
                (device_->discovery().active() || device_->discovery().complete());
            const bool originalCapture =
                device_ && device_->evidence() && device_->evidence()->record() &&
                device_->evidence()->record()->plan.id == op->nativeOperation;
            if (!op->terminal || !op->delivered ||
                (op->motion ? originalMotion &&
                                  (op->stop ? !motion_->releaseStop(op->stop == 1
                                                                        ? L::StopKind::HALT
                                                                        : L::StopKind::QUICK_STOP)
                                            : !motion_->releaseNormal())
                 : op->recovery      ? !owner_.releaseRecovery(op->recoveryId)
                 : op->commissioning ? originalCommissioning && !commissioning_->release()
                 : op->settings      ? originalSettings && !settings_->release()
                 : op->nmtId.owner   ? !owner_.releaseNmt(op->nmtId)
                 : op->discovery     ? originalDiscovery && !device_->discovery().release()
                 : op->capture       ? originalCapture && !device_->evidence()->release()
                                     : originalReadStillPresent && !app_.releaseRead())) {
                reject("result_not_releasable");
                return;
            }
            *op = Operation();
            ok();
        } else {
            const MotionStopResult* oldStop =
                motion_ && op->stop ? motion_->stopResult(op->stop == 1 ? L::StopKind::HALT
                                                                        : L::StopKind::QUICK_STOP)
                                    : nullptr;
            const bool replacedMotion =
                op->motion &&
                (!motion_ || (op->stop ? !oldStop || oldStop->result.id != op->nativeOperation
                                       : !motion_->retained() ||
                                             motion_->normalResult().id != op->nativeOperation));
            const bool replacedSettings =
                op->settings && (!settings_ || !settings_->retained() ||
                                 settings_->context().operation != op->nativeOperation);
            const bool replacedCommissioning =
                op->commissioning &&
                (!commissioning_ || !commissioning_->retained() ||
                 (commissioning_->communication() ? commissioning_->communication()->operation
                  : commissioning_->persistence() ? commissioning_->persistence()->operation
                  : commissioning_->pdo()         ? commissioning_->pdo()->operation
                                                  : 0) != op->nativeOperation);
            const bool replacedDiscovery =
                op->discovery && (!device_ || device_->discoveryOperation() != op->nativeOperation);
            const bool replacedCapture =
                op->capture &&
                (!device_ || !device_->evidence() || !device_->evidence()->record() ||
                 device_->evidence()->record()->plan.id != op->nativeOperation);
            if (replacedMotion || replacedSettings || replacedCommissioning || replacedDiscovery ||
                replacedCapture) {
                reject("operation_context_replaced");
                return;
            }
            const bool cancelled =
                !op->recovery && !op->terminal &&
                (op->motion          ? !op->stop && motion_ && motion_->cancelNormal(now)
                 : op->commissioning ? commissioning_ && commissioning_->cancel(now)
                 : op->settings      ? settings_ && settings_->cancel(now)
                 : op->nmtId.owner   ? owner_.cancelNmt(op->nmtId, now)
                 : op->discovery     ? device_ && device_->discovery().cancel(now)
                 : op->capture
                     ? device_ && device_->evidence() && device_->evidence()->interrupt(now)
                 : op->monitor
                     ? device_ && device_->stopMonitor(now)
                     : app_.readContext().operation == op->readOperation && app_.cancelRead(now));
            if (!cancelled) {
                reject("not_cancellable");
                return;
            }
            ok();
            text.add(json ? ",\"motor_stop\":false" : "local only; motor not stopped");
        }
        break;
    }
    default:
        reject("unknown_command");
        return;
    }
    text.end(json);
    if (!text.valid) {
        error(id, json, name, "record_capacity", control);
        return;
    }
    if (!enqueue(text.bytes, control, id))
        increment(stats_.outputDropped);
}

bool ProbeConsole::readValues(Operation& op, uint64_t now) noexcept {
    const auto& read = app_.readContext();
    if (op.valueCursor >= read.count)
        return true;
    if (ordinaryCount_ == ORDINARY_REPLIES)
        return false;
    bool current = false;
    if (read.kind == L::ReadKind::PROBE) {
        L::ProbeObservation value;
        current = app_.getProbe(now, 1000000, value);
    } else if (read.kind == L::ReadKind::IDENTITY) {
        L::IdentityObservation value;
        current = app_.getIdentity(now, 1000000, value);
    } else if (read.kind == L::ReadKind::CONFIG) {
        L::ConfigObservation value;
        current = app_.getConfig(now, 1000000, value);
    } else if (read.kind == L::ReadKind::STATE) {
        current = true;
        for (unsigned b = 0; b < 6; ++b)
            if (read.blockMask & (1u << b)) {
                L::StateObservation value;
                current = app_.getStateBlock(static_cast<L::StateBlock>(b), now, 1000000, value) &&
                          current;
            }
    }
    const auto& value = read.values[op.valueCursor];
    const char* unit = value.object.index == 0x6064   ? "native-position-scale-unknown"
                       : value.object.index == 0x606C ? "native-velocity-unit-unconfirmed"
                       : value.object.index == 0x2100 ? "raw-bits"
                       : value.object.index == 0x2101 || value.object.index == 0x2102 ||
                               value.object.index == 0x6041
                           ? "raw-bits"
                           : "raw-code";
    Text text;
    text.start(op.json, op.request, "value", op.command);
    text.add(op.json ? ",\"operation_id\":%lu,\"index\":%u,\"subindex\":%u,\"width\":%u,\"raw\":"
                     : "operation=%lu object=%04X:%02X width=%u raw=",
             static_cast<unsigned long>(op.id), unsigned(value.object.index),
             unsigned(value.object.subindex), unsigned(value.object.widthBytes));
    if (value.valid)
        text.add(op.json ? "\"%lu\"" : "%lu", static_cast<unsigned long>(value.raw));
    else
        text.add("null");
    if (op.json) {
        text.add(",\"native_signed\":");
        if (value.valid && value.object.representation == L::ReadRepresentation::SIGNED &&
            value.object.widthBytes <= 4 && value.object.widthBytes) {
            const unsigned bits = unsigned(value.object.widthBytes) * 8;
            const int64_t signedValue = value.raw & (UINT32_C(1) << (bits - 1))
                                            ? int64_t(value.raw) - (INT64_C(1) << bits)
                                            : int64_t(value.raw);
            text.add("\"%lld\"", static_cast<long long>(signedValue));
        } else
            text.add("null");
    }
    text.add(op.json ? ",\"unit\":\"%s\",\"current\":%s,\"interpretation\":null,\"earliest_us\":\"%"
                       "llu\",\"latest_us\":\"%llu\""
                     : " unit=%s current=%s interpretation=unknown time=[%llu,%llu]",
             unit, boolean(current && value.valid),
             static_cast<unsigned long long>(value.provenance.earliestUs),
             static_cast<unsigned long long>(value.provenance.latestUs));
    text.end(op.json);
    if (text.valid && enqueue(text.bytes, false, op.request))
        ++op.valueCursor;
    return false; // Terminal follows all value records, in order.
}
void ProbeConsole::diagnostic(uint64_t) noexcept {
    if (!debug_ || !device_ || ordinaryCount_ == ORDINARY_REPLIES)
        return;
    O::Diagnostic copy;
    if (!device_->diagnostic(copy))
        return;
    Text text;
    // Debug is unsolicited and independent of any command's frozen output mode.
    text.start(true, 0, "trace", "debug");
    text.add(",\"kind\":%u,\"can_id\":%lu,\"dlc\":%u,\"accepted\":%s,\"processed_us\":\"%llu\","
             "\"extended\":%s,\"rtr\":%s,\"payload\":\"",
             unsigned(copy.kind), static_cast<unsigned long>(copy.frame.id),
             unsigned(copy.frame.dlc), boolean(copy.accepted),
             static_cast<unsigned long long>(copy.processedUs), boolean(copy.frame.extended),
             boolean(copy.frame.rtr));
    if (!copy.frame.rtr)
        for (unsigned n = 0; n < copy.frame.payloadLength && n < 8; ++n)
            text.add("%02X", unsigned(copy.frame.data[n]));
    // Wire format is retained independently of decoding and controller evidence.
    N::NetworkEvent event;
    const auto* selected = device_->node();
    const bool decoded =
        debug_ == 2 && selected && copy.kind == O::DiagnosticKind::RX &&
        bool(N::checkNetworkEvent(copy.frame, selected->route, selected->route.binding,
                                  selected->route.generation, event));
    text.add("\",\"decoded\":%s", boolean(decoded));
    if (decoded) {
        text.add(",\"event_kind\":%u", unsigned(event.kind));
        if (event.kind == N::NetworkEventKind::EMCY)
            text.add(",\"emcy_code\":%u,\"error_register\":%u", unsigned(event.errorCode),
                     unsigned(event.errorRegister));
        else
            text.add(",\"raw_nmt\":%u", unsigned(event.state));
    }
    text.end(true);
    if (text.valid)
        enqueue(text.bytes);
}
void ProbeConsole::dispatchDevice(const ConsoleLine& line, uint64_t now) noexcept {
    const char* name = line.token(0);
    const size_t count = line.count;
    const bool json = line.json;
    const uint32_t id = line.id;
    const auto reject = [&](const char* reason) {
        increment(stats_.invalid);
        error(id, json, name, reason);
    };
    const auto rejectStatus = [&](const M::Status& status) {
        error(id, json, name, status.msg ? status.msg : M::errToString(status.code), false,
              status.code, status.detail);
    };
    Text text;
    text.start(json, id, "reply", name);
    const auto ok = [&]() { text.add(json ? ",\"ok\":true,\"status\":\"OK\"" : "OK "); };
    const auto available = [&]() -> Operation* {
        if (nextOperation_ == UINT32_MAX)
            return nullptr;
        for (unsigned slot = 0; slot < OPERATION_CAPACITY; ++slot)
            if (!operations_[slot].used)
                return &operations_[slot];
        return nullptr;
    };
    const auto admit = [&](Operation& op) {
        op.used = true;
        op.json = json;
        op.request = id;
        op.id = ++nextOperation_;
        std::snprintf(op.command, sizeof op.command, "%s", name);
        ok();
        text.add(json ? ",\"result\":\"accepted\",\"operation_id\":%lu" : "accepted operation %lu",
                 static_cast<unsigned long>(op.id));
    };
    const auto deadline = [&](size_t token, uint64_t& result) -> bool {
        uint32_t ms = 1000;
        if (token < count && !decimal(line.token(token), ms))
            return false;
        if (!ms || ms > 60000 || now > uint64_t(INT64_MAX) - uint64_t(ms) * 1000)
            return false;
        result = now + uint64_t(ms) * 1000;
        return true;
    };
    if ((settings_ && settings_->active()) || (commissioning_ && commissioning_->active())) {
        if (equal(name, "host") || equal(name, "useaddr") || equal(name, "probe") ||
            equal(name, "read") || equal(name, "discover") || equal(name, "capture-read") ||
            equal(name, "monitor")) {
            reject("typed_workflow_active");
            return;
        }
    }
    const bool applyingPdo = equal(name, "pdo") && equal(line.token(1), "apply");
    if (equal(name, "communication") || equal(name, "persistence") || applyingPdo) {
        size_t end = 0;
        if (equal(name, "communication")) {
            if (!equal(line.token(1), "heartbeat") || !equal(line.token(2), "1000")) {
                reject("only_reviewed_heartbeat_raw_0_to_1000");
                return;
            }
            end = 3;
        } else if (equal(name, "persistence")) {
            if (equal(line.token(1), "save") && equal(line.token(2), "parameters"))
                end = 3;
            else if (equal(line.token(1), "restore") && equal(line.token(2), "factory") &&
                     equal(line.token(3), "settings"))
                end = 4;
            else {
                reject("explicit_save_parameters_or_restore_factory_settings_required");
                return;
            }
        } else {
            if (equal(line.token(2), "map3-to-map2"))
                end = 3;
            else if (equal(line.token(2), "timer") &&
                     (equal(line.token(3), "0") || equal(line.token(3), "100")))
                end = 4;
            else {
                reject("only_exact_reviewed_tpdo1_transition");
                return;
            }
        }
        uint64_t cutoff = 0;
        if (count < end || count > end + 1 || !deadline(end, cutoff)) {
            reject("invalid_workflow_deadline");
            return;
        }
        Operation* op = available();
        if (!op) {
            reject("retained_results_full");
            return;
        }
        if (!commissioning_ || !commissioning_->armed() || !app_.selected() ||
            !sameReadTarget(commissioning_->target(), *app_.selected()) ||
            app_.readContext().state != M::ReadState::EMPTY || (settings_ && settings_->active()) ||
            (device_ && (device_->monitoring() || device_->discovery().active()))) {
            reject("exact_externally_reviewed_plan_prerequisites_and_handoff_guard_required");
            return;
        }
        M::Status status;
        if (equal(name, "communication")) {
            if (!commissioning_->communication()) {
                reject("wrong_armed_workflow");
                return;
            }
            status = commissioning_->requestCommunication(now, cutoff);
        } else if (equal(name, "persistence")) {
            const auto* c = commissioning_->persistence();
            const auto expected =
                end == 3 ? L::PersistenceKind::SAVE : L::PersistenceKind::FACTORY_RESTORE;
            if (!c || c->kind != expected) {
                reject("wrong_armed_persistence_intent");
                return;
            }
            status = commissioning_->requestPersistence(now, cutoff);
        } else {
            const auto* c = commissioning_->pdo();
            const bool map = equal(line.token(2), "map3-to-map2");
            if (!c ||
                c->request.transition !=
                    (map ? L::Tpdo1Transition::MAP3_TO_MAP2 : L::Tpdo1Transition::EVENT_TIMER) ||
                (!map &&
                 unsigned(c->request.eventTimer) != (equal(line.token(3), "100") ? 100u : 0u))) {
                reject("wrong_armed_pdo_intent");
                return;
            }
            status = commissioning_->requestPdo(now, cutoff);
        }
        if (!status) {
            rejectStatus(status);
            return;
        }
        *op = Operation();
        op->commissioning = true;
        op->nativeOperation =
            commissioning_->communication() ? commissioning_->communication()->operation
            : commissioning_->persistence() ? commissioning_->persistence()->operation
                                            : commissioning_->pdo()->operation;
        admit(*op);
    } else if (equal(name, "driver") || equal(name, "io") || equal(name, "control") ||
               equal(name, "tuning") || equal(name, "home") || equal(name, "velocity")) {
        L::ReadKind kind = L::ReadKind::PROBE;
        bool reading =
            equal(line.token(1), "read") || (equal(name, "tuning") && equal(line.token(2), "read"));
        if (reading) {
            if (equal(name, "home"))
                kind = L::ReadKind::HOMING;
            else if (equal(name, "velocity"))
                kind = L::ReadKind::VELOCITY_TARGET;
            else if (equal(name, "driver")) {
                const char* group = line.token(2);
                if (equal(group, "driver"))
                    kind = L::ReadKind::DRIVER;
                else if (equal(group, "soft-limits"))
                    kind = L::ReadKind::DRIVER_SOFT_LIMITS;
                else if (equal(group, "motion-parameters"))
                    kind = L::ReadKind::DRIVER_MOTION_PARAMETERS;
                else if (equal(group, "velocity-target"))
                    kind = L::ReadKind::VELOCITY_TARGET;
                else if (equal(group, "homing"))
                    kind = L::ReadKind::HOMING;
                else if (equal(group, "persistence"))
                    kind = L::ReadKind::PERSISTENCE;
            } else if (equal(name, "io")) {
                if (equal(line.token(2), "inputs"))
                    kind = L::ReadKind::IO_INPUT_CONFIGURATION;
                else if (equal(line.token(2), "outputs"))
                    kind = L::ReadKind::IO_OUTPUT_CONFIGURATION;
                else if (equal(line.token(2), "filters"))
                    kind = L::ReadKind::IO_INPUT_FILTERS;
                else if (equal(line.token(2), "command"))
                    kind = L::ReadKind::IO_OUTPUT_COMMAND;
            } else if (equal(name, "control"))
                kind = L::ReadKind::CONTROL;
            else {
                if (equal(line.token(1), "current-holding"))
                    kind = L::ReadKind::TUNING_CURRENT_HOLDING;
                else if (equal(line.token(1), "filter-position"))
                    kind = L::ReadKind::TUNING_FILTER_POSITION;
                else if (equal(line.token(1), "loop-gains"))
                    kind = L::ReadKind::TUNING_LOOP_GAINS;
                else if (equal(line.token(1), "eds-gains"))
                    kind = L::ReadKind::TUNING_EDS_GAINS;
            }
            if (count != (equal(name, "control") || equal(name, "home") || equal(name, "velocity")
                              ? 2u
                              : 3u) ||
                kind == L::ReadKind::PROBE || now > uint64_t(INT64_MAX) - 1000000) {
                reject("invalid_native_read_group");
                return;
            }
            Operation* op = available();
            if (!op) {
                reject("retained_results_full");
                return;
            }
            L::ReadOptions options;
            if (device_)
                options = device_->readOptions(kind);
            else
                options.queueTimeoutUs = options.responseTimeoutUs = 100000;
            const auto status = app_.requestOrdinaryRead(kind, now, now + 1000000, options);
            if (!status) {
                rejectStatus(status);
                return;
            }
            *op = Operation();
            op->automatic = true;
            op->readOperation = app_.readContext().operation;
            admit(*op);
        } else {
            uint32_t values[8] = {};
            const bool soft = equal(name, "driver") && equal(line.token(1), "limits");
            const unsigned required =
                equal(name, "io") ? 8 : (equal(name, "driver") && !soft ? 3 : 1);
            if ((!equal(line.token(1), "set") && !soft) || equal(name, "tuning") ||
                count != 2 + required) {
                reject("invalid_complete_settings_candidate");
                return;
            }
            for (unsigned n = 0; n < required; ++n)
                if (!unsignedDecimal(line.token(2 + n), values[n])) {
                    reject("invalid_native_integer");
                    return;
                }
            if ((equal(name, "driver") && !soft &&
                 (values[0] > 3000 || values[1] > 2000 || values[2] > 2000)) ||
                (soft && values[0] > 1) || (equal(name, "control") && values[0] > 2) ||
                (equal(name, "io") && values[0] > 127)) {
                reject("native_settings_range");
                return;
            }
            if (equal(name, "io"))
                for (unsigned n = 1; n < 8; ++n)
                    if (values[n] > 13) {
                        reject("input_function_range_0_13");
                        return;
                    }
            Operation* op = available();
            if (!op) {
                reject("retained_results_full");
                return;
            }
            if (!settings_ || !app_.selected() ||
                !sameReadTarget(*app_.selected(), settings_->target()) ||
                now > uint64_t(INT64_MAX) - 1000000) {
                reject("Q20_external_fresh_prerequisites_and_handoff_guard_required");
                return;
            }
            M::Status status;
            if (soft) {
                L::SoftLimitSettings candidate;
                candidate.enable = static_cast<L::SoftLimitEnable>(values[0]);
                status = settings_->requestSoftLimits(candidate, now, now + 1000000);
            } else if (equal(name, "driver")) {
                L::DriverSettings candidate;
                candidate.positionSpeedRpm = values[0];
                candidate.rampUpTimeMs = values[1];
                candidate.rampDownTimeMs = values[2];
                status = settings_->requestDriver(candidate, now, now + 1000000);
            } else if (equal(name, "io")) {
                L::IoSettings candidate;
                candidate.invertedInputs = static_cast<uint16_t>(values[0]);
                for (unsigned n = 0; n < 7; ++n)
                    candidate.functions[n] = static_cast<L::InputFunction>(values[n + 1]);
                status = settings_->requestIo(candidate, now, now + 1000000);
            } else {
                L::ControlSettings candidate;
                candidate.loopMode = static_cast<L::LoopMode>(values[0]);
                status = settings_->requestControl(candidate, now, now + 1000000);
            }
            if (!status) {
                rejectStatus(status);
                return;
            }
            *op = Operation();
            op->settings = true;
            op->nativeOperation = settings_->context().operation;
            admit(*op);
        }
    } else if (equal(name, "probe") || equal(name, "read")) {
        L::ReadKind kind = L::ReadKind::PROBE;
        uint8_t mask = 0;
        size_t end = 1;
        if (equal(name, "read")) {
            if (equal(line.token(1), "identity"))
                kind = L::ReadKind::IDENTITY;
            else if (equal(line.token(1), "config"))
                kind = L::ReadKind::CONFIG;
            else if (equal(line.token(1), "state")) {
                kind = L::ReadKind::STATE;
                const char* blocks[] = {"errors", "history",  "monitors",
                                        "status", "feedback", "inputs"};
                unsigned b = 0;
                for (; b < 6 && !equal(line.token(2), blocks[b]); ++b) {
                }
                uint32_t timeout = 0;
                if (b == 6 && count > 2 && !decimal(line.token(2), timeout)) {
                    reject("state_block_required");
                    return;
                }
                mask = uint8_t(1u << (b == 6 ? 3 : b));
                if (b != 6)
                    ++end;
            } else {
                reject("invalid_read_kind");
                return;
            }
            ++end;
        }
        uint64_t cutoff = 0;
        if (count < end || count > end + 1 || !deadline(end, cutoff)) {
            reject("invalid_arguments");
            return;
        }
        Operation* op = available();
        if (!op) {
            reject("retained_results_full");
            return;
        }
        if (device_ && (device_->monitoring() || device_->discovery().active() ||
                        (device_->evidence() && device_->evidence()->record()))) {
            reject("acquisition_busy");
            return;
        }
        L::ReadOptions options;
        if (device_)
            options = device_->readOptions(kind);
        else
            options.queueTimeoutUs = options.responseTimeoutUs = 100000;
        const M::Status prepared = app_.requestOrdinaryRead(kind, now, cutoff, options, mask);
        if (!prepared) {
            rejectStatus(prepared);
            return;
        }
        *op = Operation();
        op->automatic = true;
        op->readOperation = app_.readContext().operation;
        admit(*op);
    } else if (equal(name, "host") || equal(name, "useaddr")) {
        uint32_t address = 0, subaxis = 0;
        if (!device_ || count < 2 || count > (equal(name, "host") ? 2u : 3u) ||
            !unsignedDecimal(line.token(1), address) ||
            (count == 3 && !unsignedDecimal(line.token(2), subaxis))) {
            reject("configured_device_interface_and_valid_target_required");
            return;
        }
        const M::Status selected = equal(name, "host") ? device_->selectSlot(address)
                                                       : device_->selectAddress(address, subaxis);
        if (!selected) {
            rejectStatus(selected);
            return;
        }
        ok();
        text.add(json ? ",\"wire_work\":false" : "host selection; no wire work");
    } else if (equal(name, "discover")) {
        if (!device_) {
            reject("configured_device_interface_required");
            return;
        }
        auto& scan = device_->discovery();
        if (count == 2 && equal(line.token(1), "cancel")) {
            if (!scan.cancel(now)) {
                reject("no_active_discovery");
                return;
            }
            ok();
        } else if (count == 2 && equal(line.token(1), "release")) {
            // Correlated operations own release through their normal delivery fence.
            reject("use_release_operation_id");
            return;
        } else if ((count == 2 || count == 3) && equal(line.token(1), "inspect")) {
            if (count == 3) {
                uint32_t ordinal = 0;
                if (!unsignedDecimal(line.token(2), ordinal) ||
                    ordinal >= DiscoveryPlan::CAPACITY ||
                    !scan.result(static_cast<uint8_t>(ordinal))) {
                    reject("no_retained_discovery_finding");
                    return;
                }
                ok();
                findingFields(text, json, *scan.result(static_cast<uint8_t>(ordinal)));
                text.end(json);
                if (text.valid)
                    enqueue(text.bytes, false, id);
                else
                    reject("record_capacity");
                return;
            }
            ok();
            const auto& summary = scan.summary();
            text.add(
                json ? ",\"active\":%s,\"complete\":%s,\"targets\":%u,\"completed\":%u,"
                       "\"services\":%u,\"unresolved_slots\":%u"
                     : "active=%s complete=%s targets=%u completed=%u services=%u unresolved=%u",
                boolean(scan.active()), boolean(scan.complete()), unsigned(summary.targets),
                unsigned(summary.completed), unsigned(summary.servicesAdmitted),
                unsigned(summary.unresolvedSlots));
        } else {
            DiscoveryMode mode;
            if (equal(line.token(1), "passive"))
                mode = DiscoveryMode::PASSIVE;
            else if (equal(line.token(1), "probe"))
                mode = DiscoveryMode::PROBE;
            else if (equal(line.token(1), "identity"))
                mode = DiscoveryMode::IDENTITY;
            else {
                reject("invalid_discovery_mode");
                return;
            }
            uint64_t cutoff = 0;
            uint8_t slots[DiscoveryPlan::CAPACITY] = {};
            if (count < 4 || count > 3 + DiscoveryPlan::CAPACITY || !deadline(2, cutoff)) {
                reject("explicit_targets_and_deadline_required");
                return;
            }
            for (size_t n = 3; n < count; ++n) {
                uint32_t slot = 0;
                // A target is any configured channel slot; CAPACITY bounds only the count.
                if (!unsignedDecimal(line.token(n), slot) || slot >= N::SdoClient::CHANNELS) {
                    reject("invalid_slot");
                    return;
                }
                slots[n - 3] = static_cast<uint8_t>(slot);
            }
            Operation* op = available();
            if (!op) {
                reject("retained_results_full");
                return;
            }
            const auto options = device_->readOptions(
                mode == DiscoveryMode::IDENTITY ? L::ReadKind::IDENTITY : L::ReadKind::PROBE);
            const auto status =
                device_->discover(mode, slots, uint8_t(count - 3), now, cutoff, options);
            if (!status) {
                rejectStatus(status);
                return;
            }
            *op = Operation();
            op->discovery = true;
            op->nativeOperation = device_->discoveryOperation();
            admit(*op);
        }
    } else if (equal(name, "nmt")) {
        N::NmtCommand command;
        if (equal(line.token(1), "start"))
            command = N::NmtCommand::START;
        else if (equal(line.token(1), "stop"))
            command = N::NmtCommand::STOP;
        else if (equal(line.token(1), "preop"))
            command = N::NmtCommand::PRE_OPERATIONAL;
        else if (equal(line.token(1), "reset-node"))
            command = N::NmtCommand::RESET_NODE;
        else if (equal(line.token(1), "reset-communication"))
            command = N::NmtCommand::RESET_COMMUNICATION;
        else {
            reject("invalid_nmt_command");
            return;
        }
        const bool broadcast = equal(line.token(2), "broadcast");
        uint64_t cutoff = 0;
        if (!device_ || count < 3 || count > 4 ||
            (!broadcast && !equal(line.token(2), "addressed")) || !deadline(3, cutoff)) {
            reject("explicit_canopen_target_scope_and_deadline_required");
            return;
        }
        Operation* op = available();
        if (!op) {
            reject("retained_results_full");
            return;
        }
        N::NmtRequestId native;
        if (!device_->requestNmt(command, broadcast, nextOperation_ + 1, now, cutoff, native)) {
            reject("nmt_admission_rejected");
            return;
        }
        *op = Operation();
        op->nmtId = native;
        admit(*op);
    } else if (equal(name, "network") || equal(name, "heartbeat")) {
        const auto* node = device_ ? device_->node() : nullptr;
        if (count != 1 || !node) {
            reject("current_configured_canopen_binding_required");
            return;
        }
        N::ServiceEligibility eligibility;
        if (!N::serviceEligibility(*node, now, eligibility)) {
            reject("invalid_evidence_time");
            return;
        }
        ok();
        text.add(
            json ? ",\"exact_nmt_known\":%s,\"observed_nmt\":%u,\"requested_present\":%s,"
                   "\"requested_command\":%u,\"transition_pending\":%s,\"ordinary_sdo\":%s,"
                   "\"producer_ms\":"
                 : "exact-nmt=%s observed=%u intent=%s requested=%u pending=%s sdo=%s producer-ms=",
            boolean(eligibility.exactNmtKnown), unsigned(node->nmt), boolean(node->intentPresent),
            unsigned(node->requestedCommand), boolean(node->transitionPending),
            boolean(eligibility.ordinarySdo));
        if (node->producerKnown)
            text.add("%u", unsigned(node->producerTimeMs));
        else
            text.add("null");
        text.add(json ? ",\"heartbeat_seen\":%s,\"emcy_count\":%lu" : " heartbeat=%s emcy=%lu",
                 boolean(node->heartbeat.present), static_cast<unsigned long>(node->emcyCount));
    } else if (equal(name, "pdo")) {
        uint32_t slot = 0;
        if (count != 2 || !unsignedDecimal(line.token(1), slot) || slot > UINT8_MAX) {
            reject("invalid_pdo_slot");
            return;
        }
        const auto* record = device_ ? device_->pdo(static_cast<uint8_t>(slot)) : nullptr;
        if (!record) {
            reject("no_configured_pdo_map");
            return;
        }
        ok();
        text.add(json ? ",\"map_confirmed\":%s,\"sample_qualified\":%s,\"sample_present\":%s"
                      : "map=%s qualified=%s sample=%s",
                 boolean(record->confirmed), boolean(record->qualified),
                 boolean(record->samplePresent));
    } else if (equal(name, "debug")) {
        if (!device_ || count > 2) {
            reject("configured_device_interface_required");
            return;
        }
        if (count == 2) {
            if (equal(line.token(1), "off"))
                debug_ = 0;
            else if (equal(line.token(1), "raw"))
                debug_ = 1;
            else if (equal(line.token(1), "decoded"))
                debug_ = 2;
            else {
                reject("invalid_debug_mode");
                return;
            }
        }
        ok();
        text.add(json ? ",\"mode\":%u,\"trace_overwritten\":%lu" : "mode=%u overwritten=%lu",
                 unsigned(debug_), static_cast<unsigned long>(owner_.stats().traceOverwritten));
    } else if (equal(name, "load")) {
        if (count != 1) {
            reject("invalid_arguments");
            return;
        }
        ok();
        const auto stats = owner_.stats();
        text.add(json ? ",\"rx_frames\":%lu,\"tx_frames\":%lu,\"max_service_gap_us\":\"%llu\""
                      : "rx=%lu tx=%lu max-gap-us=%llu",
                 static_cast<unsigned long>(stats.rxFrames),
                 static_cast<unsigned long>(stats.txFrames),
                 static_cast<unsigned long long>(stats.maxServiceGapUs));
    } else if (equal(name, "monitor")) {
        if (count == 2 && equal(line.token(1), "off")) {
            if (!device_ || !device_->stopMonitor(now)) {
                reject("no_active_monitor");
                return;
            }
            ok();
        } else {
            uint32_t interval = 0, samples = 0;
            uint64_t cutoff = 0;
            if (!device_ || count < 3 || count > 4 || !decimal(line.token(1), interval) ||
                !interval || interval > 60000 || !decimal(line.token(2), samples) || !samples ||
                samples > 16 || !deadline(3, cutoff)) {
                reject("invalid_finite_monitor_bounds");
                return;
            }
            Operation* op = available();
            if (!op) {
                reject("retained_results_full");
                return;
            }
            const auto status = device_->monitor(uint64_t(interval) * 1000,
                                                 static_cast<uint8_t>(samples), now, cutoff);
            if (!status) {
                rejectStatus(status);
                return;
            }
            *op = Operation();
            op->monitor = op->automatic = true;
            op->readOperation = app_.readContext().operation;
            admit(*op);
        }
    } else if (equal(name, "capture-read")) {
        if (count != 1 || !device_) {
            reject("externally_configured_capture_plan_required");
            return;
        }
        Operation* op = available();
        if (!op) {
            reject("retained_results_full");
            return;
        }
        const auto status = device_->capture(now);
        if (status != EvidenceAdmission::ACCEPTED) {
            reject(status == EvidenceAdmission::READ_EFFECT_UNQUALIFIED
                       ? "Q20_read_effect_unqualified"
                       : "capture_plan_busy_invalid_or_missing");
            return;
        }
        *op = Operation();
        op->capture = true;
        op->readOperation = app_.readContext().operation;
        op->nativeOperation = device_->evidence()->record()->plan.id;
        admit(*op);
    } else if (equal(name, "motion-profile")) {
        if (count != 2 || !device_) {
            reject("configured_device_interface_required");
            return;
        }
        if (equal(line.token(1), "read")) {
            if (now > uint64_t(INT64_MAX) - 1000000) {
                reject("invalid_time");
                return;
            }
            Operation* op = available();
            if (!op) {
                reject("retained_results_full");
                return;
            }
            L::ReadOptions options;
            options.queueTimeoutUs = options.responseTimeoutUs = 100000;
            const auto status = app_.requestOrdinaryRead(L::ReadKind::DRIVER_MOTION_PARAMETERS, now,
                                                         now + 1000000, options);
            if (!status) {
                rejectStatus(status);
                return;
            }
            *op = Operation();
            op->automatic = true;
            op->readOperation = app_.readContext().operation;
            admit(*op);
        } else if (equal(line.token(1), "restore")) {
            if (!settings_ || !app_.selected() ||
                !sameReadTarget(*app_.selected(), settings_->target()) ||
                now > uint64_t(INT64_MAX) - 1000000) {
                reject("fresh_external_restore_plan_and_settings_bridge_required");
                return;
            }
            Operation* op = available();
            if (!op) {
                reject("retained_results_full");
                return;
            }
            const auto status = device_->restoreMotionProfile(*settings_, now, now + 1000000);
            if (!status) {
                rejectStatus(status);
                return;
            }
            *op = Operation();
            op->settings = true;
            op->nativeOperation = settings_->context().operation;
            admit(*op);
        } else if (equal(line.token(1), "inspect") || equal(line.token(1), "forget")) {
            if (equal(line.token(1), "forget")) {
                if (settings_ && settings_->retained()) {
                    reject("settings_result_retained");
                    return;
                }
                device_->forgetMotionProfile();
            }
            ok();
            const auto* record = device_->motionProfile();
            if (record)
                text.add(
                    json ? ",\"record_id\":%lu,\"captured_us\":\"%llu\",\"complete_backup\":false"
                         : "record=%lu captured-us=%llu partial-three-field-record",
                    static_cast<unsigned long>(record->recordId),
                    static_cast<unsigned long long>(record->capturedUs));
            else
                text.add(json ? ",\"record\":null,\"restore_available\":false"
                              : "no recorded snapshot; acquisition Q20-gated");
        } else {
            reject("invalid_motion_profile_intent");
            return;
        }
    } else {
        reject("unknown_command");
        return;
    }
    text.end(json);
    if (text.valid)
        enqueue(text.bytes, false, id);
    else
        error(id, json, name, "record_capacity");
}
} // namespace MotorControlCANExample
