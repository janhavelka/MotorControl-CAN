// SPDX-License-Identifier: MIT
// Console tests use the real application/owner and synthetic wire evidence only.
#include "FakeTransport.h"
#include "ProbeConsole.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
template <class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof value); }
    void unchanged(const T& value) const { CHECK(!std::memcmp(bytes, &value, sizeof value)); }
};
struct Bytes {
    std::string input, output;
    size_t inputRead = 0;
    bool blocked = false;
    unsigned writes = 0;
    static bool read(void* context, uint8_t& value) {
        Bytes& bytes = *static_cast<Bytes*>(context);
        if (bytes.inputRead == bytes.input.size())
            return false;
        value = static_cast<uint8_t>(bytes.input[bytes.inputRead++]);
        return true;
    }
    static bool write(void* context, uint8_t value) {
        Bytes& bytes = *static_cast<Bytes*>(context);
        ++bytes.writes;
        if (bytes.blocked)
            return false;
        bytes.output += static_cast<char>(value);
        return true;
    }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks result;
        result.context = this;
        result.readByte = read;
        result.writeByte = write;
        return result;
    }
};
struct Wire {
    E::FakeTransport fake;
    unsigned begins = 0, polls = 0;
    bool beginAccepted = true;
    O::RecoveryProgress recovery = O::RecoveryProgress::PENDING;
    explicit Wire(E::FakeTransport::Mode mode) : fake(mode) {}
    static bool receive(void* context, O::ReceivedFrame& output) {
        E::FakeTransport& f = static_cast<Wire*>(context)->fake;
        return f.callbacks().receive(&f, output);
    }
    static bool transmit(void* context, const M::Frame& frame, O::TransportTxId id,
                         uint32_t epoch) {
        E::FakeTransport& f = static_cast<Wire*>(context)->fake;
        return f.callbacks().transmit(&f, frame, id, epoch);
    }
    static bool report(void* context, O::TxReport& output) {
        E::FakeTransport& f = static_cast<Wire*>(context)->fake;
        return f.callbacks().takeTxReport(&f, output);
    }
    static void observe(void* context, const O::ReceivedFrame& input, uint64_t now) {
        E::FakeTransport& f = static_cast<Wire*>(context)->fake;
        f.callbacks().observe(&f, input, now);
    }
    static bool begin(void* context, uint32_t) {
        Wire& wire = *static_cast<Wire*>(context);
        ++wire.begins;
        return wire.beginAccepted;
    }
    static O::RecoveryProgress poll(void* context, uint32_t) {
        Wire& wire = *static_cast<Wire*>(context);
        ++wire.polls;
        return wire.recovery;
    }
    O::Transport callbacks() {
        O::Transport result;
        result.context = this;
        result.receive = receive;
        result.transmit = transmit;
        result.takeTxReport = report;
        result.observe = observe;
        result.beginRecovery = begin;
        result.pollRecovery = poll;
        return result;
    }
};
L::ReadOptions options() {
    L::ReadOptions value;
    value.queueTimeoutUs = 50;
    value.responseTimeoutUs = 20;
    return value;
}
struct Rig {
    C::SdoClient services;
    Wire wire;
    Bytes bytes;
    E::ProbePlatform platform;
    O::CanBusOwner owner;
    E::ProbeApp app;
    E::ProbeConsole console;
    uint64_t now = 0;
    explicit Rig(E::FakeTransport::Mode mode = E::FakeTransport::Mode::SUCCESS)
        : wire(mode), platform(bytes.callbacks()), owner(services, wire.callbacks(), 127),
          app(owner, platform), console(app, owner, platform) {
        const L::ReadTarget target = E::FakeTransport::target();
        // Explicit synthetic fixture premises; no real drive evidence is implied.
        CHECK(services.configure(0, target.channel, true, true));
        C::NetworkRoute network;
        network.binding = target.channel.binding;
        network.generation = target.nodeGeneration;
        network.heartbeatId = 0x705;
        network.emcyId = 0x85;
        CHECK(services.configureNetwork(0, network, C::NetworkObserver(), 0));
    }
    void feed(const std::string& text) {
        for (size_t i = 0; i < text.size(); ++i)
            console.feed(static_cast<uint8_t>(text[i]), now);
    }
    void line(const std::string& text) { feed(text + "\n"); }
    void turn() {
        ++now;
        CHECK(wire.fake.setTime(now));
        CHECK(app.service(now));
        console.service(now);
    }
    void drain(unsigned turns = 1000) {
        for (unsigned i = 0; i < turns; ++i)
            turn();
    }
    std::string command(const std::string& text) {
        const size_t start = bytes.output.size();
        line(text);
        drain();
        return bytes.output.substr(start);
    }
    void finishRead() {
        for (unsigned i = 0; i < 150 && app.readContext().state == M::ReadState::ACTIVE; ++i) {
            turn();
            if (owner.result(app.pendingRequest()))
                CHECK(app.advanceRead(now));
        }
        CHECK(app.readContext().state != M::ReadState::ACTIVE);
        drain();
    }
};
void wholeLineParser() {
    E::ConsoleLine line;
    CHECK(E::parseConsoleLine("\t @4294967295  status \t", 23, line));
    CHECK(line.json && line.id == UINT32_MAX && line.count == 1);
    CHECK(!std::strcmp(line.token(0), "status") && !std::strcmp(line.token(1), ""));
    char input[] = "@17 prepare 9007199254740993 steps native";
    CHECK(E::parseConsoleLine(input, sizeof input - 1, line));
    std::memset(input, 'x', sizeof input - 1);
    CHECK(!std::strcmp(line.token(1), "9007199254740993"));
    const Saved<E::ConsoleLine> saved(line);
    const char* invalid[] = {
        "",   " ",        "\t",       "@0 status", "@-1 status", "@+1 status", "@4294967296 status",
        "@1", "@ status", "status\r", "status\n",  "status\x7f", "status\x01", "status\x80"};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        CHECK(!E::parseConsoleLine(invalid[i], std::strlen(invalid[i]), line));
        saved.unchanged(line);
    }
    const char nul[] = {'s', 't', 'a', 't', 'u', 's', 0, 'x'};
    CHECK(!E::parseConsoleLine(nul, sizeof nul, line));
    saved.unchanged(line);
    CHECK(!E::parseConsoleLine(nullptr, 1, line));
    saved.unchanged(line);
    std::string maximum(255, 'x');
    CHECK(E::parseConsoleLine(maximum.data(), maximum.size(), line));
    CHECK(line.count == 1 && std::strlen(line.token(0)) == 255);
    maximum += 'x';
    const Saved<E::ConsoleLine> boundary(line);
    CHECK(!E::parseConsoleLine(maximum.data(), maximum.size(), line));
    boundary.unchanged(line);
    std::string tokens = "@1";
    for (unsigned i = 0; i < E::ProbeConsole::TOKEN_CAPACITY; ++i)
        tokens += " x";
    CHECK(E::parseConsoleLine(tokens.data(), tokens.size(), line));
    CHECK(line.count == E::ProbeConsole::TOKEN_CAPACITY && line.json && line.id == 1);
    const Saved<E::ConsoleLine> full(line);
    tokens += " x";
    CHECK(!E::parseConsoleLine(tokens.data(), tokens.size(), line));
    full.unchanged(line);
}
void escapedStrings() {
    char text[100] = "sentinel";
    CHECK(E::consoleJsonString("a\"\\\n\r\t", text, sizeof text));
    CHECK(!std::strcmp(text, "\"a\\\"\\\\\\u000a\\u000d\\u0009\""));
    CHECK(E::consoleJsonString("", text, 3) && !std::strcmp(text, "\"\""));
    const Saved<decltype(text)> saved(text);
    CHECK(!E::consoleJsonString("", text, 2));
    saved.unchanged(text);
    CHECK(!E::consoleJsonString(nullptr, text, sizeof text));
    saved.unchanged(text);
    CHECK(!E::consoleJsonString("x", nullptr, sizeof text));
    CHECK(!E::consoleJsonString("x", text, 0));
    saved.unchanged(text);
    CHECK(E::consoleJsonString("\x01", text, sizeof text));
    CHECK(!std::strcmp(text, "\"\\u0001\""));
}
void contains(const std::string& output, const char* expected) {
    if (output.find(expected) == std::string::npos)
        std::fprintf(stderr, "Expected [%s] in output:\n%s\n", expected, output.c_str());
    CHECK(output.find(expected) != std::string::npos);
}
void reply(const std::string& output, bool ok) {
    contains(output, "\"schema\":\"motorcontrolcan.console/1\"");
    contains(output, "\"protocol\":3");
    contains(output, "\"profile\":\"lichuan_c\"");
    contains(output, ok ? "\"ok\":true" : "\"ok\":false");
    CHECK(!output.empty() && output.back() == '\n');
}
void passiveRoutesAndGrammar() {
    Rig rig;
    const char* passive[] = {"version",
                             "profile list",
                             "caps",
                             "config",
                             "status",
                             "health",
                             "stats",
                             "drv",
                             "memory",
                             "result",
                             "stats reset",
                             "axis config",
                             "wiring",
                             "wiring di0 unknown",
                             "wiring di6 unconnected",
                             "wiring do0 connected",
                             "wiring do2 unknown"};
    for (size_t i = 0; i < sizeof passive / sizeof passive[0]; ++i)
        reply(rig.command(std::string("@10 ") + passive[i]), true);
    CHECK(!rig.wire.fake.transmitCalls());
    const E::HostInterfaceFacts before = rig.app.hostInterfaceFacts();
    const Saved<E::HostInterfaceFacts> facts(before);
    const char* invalid[] = {"reset",
                             "ping",
                             "ver",
                             "?",
                             "health check",
                             "help advanced",
                             "profile ess_rs",
                             "profile ess_rs enable",
                             "stop direct",
                             "motor stop",
                             "motor",
                             "version junk",
                             "profile",
                             "profile list junk",
                             "caps junk",
                             "config junk",
                             "status junk",
                             "drv junk",
                             "memory junk",
                             "stats clear",
                             "stats reset junk",
                             "result 0",
                             "result -1",
                             "result 1x",
                             "result 4294967296",
                             "result 1 extra",
                             "release",
                             "release 0",
                             "release 1 extra",
                             "cancel 0",
                             "cancel -1",
                             "cancel 1 extra",
                             "recover extra",
                             "wiring di7 connected",
                             "wiring do3 connected",
                             "wiring di0",
                             "wiring di0 enabled",
                             "wiring di0 connected extra",
                             "axis",
                             "axis config set",
                             "axis config set command",
                             "axis config set invalid 1",
                             "axis config set command 1 junk",
                             "axis origin",
                             "axis origin 1x",
                             "axis origin 1/2",
                             "prepare",
                             "prepare absolute",
                             "prepare absolute 1 steps",
                             "prepare absolute 1 steps bad",
                             "prepare absolute 1e2 steps native",
                             "prepare absolute 1/0 steps native",
                             "prepare absolute 9223372036854775808 steps native",
                             "prepare absolute 1 steps native bad",
                             "prepare absolute 1 steps native exact 0 extra extra",
                             "prepare relative 1 steps native",
                             "prepare relative 1 steps native bad",
                             "prepare angle 1 turn motor shortest",
                             "prepare angle 1 turn motor bad reject",
                             "prepare angle 1 turn motor shortest bad"};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
        reply(rig.command(std::string("@11 ") + invalid[i]), false);
    CHECK(!rig.wire.fake.transmitCalls());
    // An invalid complete mutation cannot publish any prefix of its candidate.
    E::HostWiringUse wiring;
    CHECK(rig.app.hostWiring(0, wiring) && wiring == E::HostWiringUse::UNKNOWN);
    const E::HostInterfaceFacts after = rig.app.hostInterfaceFacts();
    facts.unchanged(after);
}
void inventoryAndHelp() {
    Rig rig;
    const std::string help = rig.command("@20 help");
    const std::string caps = rig.command("@21 caps");
    size_t count = 0;
    const E::ConsoleCommand* commands = E::consoleCommands(count);
    CHECK(commands && count > 10);
    for (size_t i = 0; i < count; ++i) {
        const E::ConsoleCommand& command = commands[i];
        CHECK(command.name && command.grammar && command.api && command.capability && command.help);
        contains(caps, (std::string("\"command\":\"") + command.name + "\"").c_str());
        if (command.gate) {
            CHECK(command.owner > 40);
            contains(caps, command.gate);
            CHECK(help.find(std::string("\"command\":\"") + command.name + "\"") ==
                  std::string::npos);
            reply(rig.command(std::string("@22 ") + command.name), false);
        } else {
            contains(help, command.grammar);
            reply(rig.command(std::string("@23 help ") + command.name), true);
        }
    }
    CHECK(!rig.wire.fake.transmitCalls());
}
void exactLocalTargets() {
    Rig rig;
    reply(rig.command("@30 axis config set command 1"), true);
    const char* settings[] = {"gear 1",
                              "fullsteps 200",
                              "lead 5/2",
                              "encoder-id 7",
                              "encoder-scale 1000",
                              "encoder-basis motor",
                              "encoder-polarity -1",
                              "polarity 1",
                              "native-limits -9223372036854775808 9223372036854775807",
                              "relative-bases 7",
                              "soft-limits -100 100",
                              "fullsteps none",
                              "lead none",
                              "encoder-scale none",
                              "soft-limits none"};
    for (size_t i = 0; i < sizeof settings / sizeof settings[0]; ++i)
        reply(rig.command(std::string("@31 axis config set ") + settings[i]), true);
    const M::AxisConfig* config = rig.app.axisConfig();
    CHECK(config && config->nativeMaximum == INT64_MAX && config->nativeMinimum == INT64_MIN);
    const std::string exact = rig.command("@32 prepare absolute 9007199254740993 steps native");
    reply(exact, true);
    contains(exact, "9007199254740993");
    contains(exact, "\"9007199254740993\"");
    const Saved<M::AxisConfig> old(*config);
    const char* invalid[] = {"command 0",
                             "command -1",
                             "gear 0",
                             "lead -1",
                             "polarity 0",
                             "encoder-polarity 2",
                             "encoder-id 4294967296",
                             "encoder-basis invalid",
                             "native-limits 2 1",
                             "native-limits 1/2 9",
                             "relative-bases 8"};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        reply(rig.command(std::string("@33 axis config set ") + invalid[i]), false);
        old.unchanged(*rig.app.axisConfig());
    }
    reply(rig.command("@34 axis origin 0"), false); // No stationary witness is fabricated.
    M::AxisReference reference;
    reference.binding = config->binding;
    reference.referenceGeneration = config->referenceGeneration;
    reference.idle = reference.stationary = reference.nativeKnown = true;
    reference.source = M::ScaleSource::QUALIFIED;
    reference.observedUs = reference.nowUs = rig.now;
    CHECK(rig.app.setHostAxisReference(reference));
    reply(rig.command("@35 axis origin 0"), true);
    reply(rig.command("@36 prepare relative 3/2 steps native actual nearest 1/2"), true);
    reply(rig.command("@37 prepare absolute 1 turn motor exact"), true);
    reply(rig.command("@38 prepare angle 90 deg motor shortest reject nearest 1/2"), true);
    const std::string approximate =
        rig.command("@39 prepare absolute 1 rad motor nearest 1 1/1000");
    reply(approximate, true);
    contains(approximate, "\"exact_arithmetic\":false");
    contains(approximate, "\"error_numerator\":null");
    contains(approximate, "\"error_denominator\":null");
    contains(approximate, "\"error_negative\":null");
    reply(rig.command("@40 prepare absolute 1 rad motor nearest 1"), false);
    CHECK(!rig.wire.fake.transmitCalls());
}
void roundingAndBinaryPreviews() {
    Rig integers;
    reply(integers.command("@41 axis config set command 1"), true);
    const char* request[] = {"3/2 steps native zero 1",  "-3/2 steps native zero 1",
                             "3/2 steps native floor 1", "-3/2 steps native floor 1",
                             "3/2 steps native ceil 1",  "-3/2 steps native ceil 1"};
    const char* expected[] = {"1", "-1", "1", "-2", "2", "-1"};
    for (size_t i = 0; i < sizeof request / sizeof request[0]; ++i) {
        const std::string output =
            integers.command(std::string("@42 prepare absolute ") + request[i]);
        reply(output, true);
        contains(output, (std::string("\"effective_native\":\"") + expected[i] + "\"").c_str());
    }
    Rig binary;
    M::AxisConfig config;
    config.binding.logicalAxisId = 99;
    config.binding.endpoint.protocol = M::Protocol::PROPRIETARY;
    config.binding.endpoint.profile = 99;
    config.binding.bindingGeneration = config.binding.configurationGeneration = 1;
    config.positionRepresentation = M::NativeRepresentation::BINARY32;
    CHECK(binary.app.establishHostAxis(config));
    const std::string output = binary.command("@43 prepare absolute 1/2 steps native");
    reply(output, true);
    contains(output, "\"effective_native\":null");
    contains(output, "\"effective_float\":0.5");
    contains(output, "\"error_numerator\":null");
    contains(output, "\"error_denominator\":null");
    contains(output, "\"error_negative\":null");
    const std::string human = binary.command("prepare absolute 1/2 steps native");
    contains(human, "binary32=0.5");
    CHECK(human.find("native=0") == std::string::npos);
    CHECK(!integers.wire.fake.transmitCalls() && !binary.wire.fake.transmitCalls());
}
void inputLossLatchPreservesBuffersAndCounters() {
    Bytes bytes;
    bytes.input = std::string(64, 'a') + std::string(8, 'b');
    bytes.blocked = true;
    E::ProbePlatform platform(bytes.callbacks());
    const uint8_t retained[] = {'k', 'e', 'p', 't'};
    CHECK(platform.writeOutput(retained, sizeof retained));
    CHECK(!platform.takeInputLoss());
    for (unsigned i = 0; i < 9; ++i)
        platform.service();
    CHECK(platform.stats().inputUsed == 64 && platform.stats().inputDropped == 8);
    const E::ProbeIoStats first = platform.stats();
    const Saved<E::ProbeIoStats> firstStats(first);
    CHECK(platform.takeInputLoss());
    CHECK(!platform.takeInputLoss());
    const E::ProbeIoStats firstConsumed = platform.stats();
    firstStats.unchanged(firstConsumed);
    // Consuming the latch preserves the full FIFO and cumulative counters.
    // A later dropped span must publish another independent loss interval.
    bytes.input += std::string(8, 'c');
    platform.service();
    CHECK(platform.stats().inputUsed == 64 && platform.stats().inputDropped == 16);
    const E::ProbeIoStats second = platform.stats();
    const Saved<E::ProbeIoStats> secondStats(second);
    CHECK(platform.takeInputLoss());
    CHECK(!platform.takeInputLoss());
    const E::ProbeIoStats secondConsumed = platform.stats();
    secondStats.unchanged(secondConsumed);
    for (unsigned i = 0; i < 64; ++i) {
        uint8_t byte = 0;
        CHECK(platform.takeInput(byte) && byte == 'a');
    }
    bytes.input += std::string(8, 'd');
    platform.service();
    CHECK(!platform.takeInputLoss() && platform.stats().inputDropped == 16);
    for (unsigned i = 0; i < 8; ++i) {
        uint8_t byte = 0;
        CHECK(platform.takeInput(byte) && byte == 'd');
    }
    CHECK(platform.stats().outputUsed == sizeof retained);
    bytes.blocked = false;
    platform.service();
    CHECK(bytes.output == "kept");
}
void damagedLinesNeverDispatch() {
    Rig rig;
    std::string overlong = "wiring di0 connected";
    overlong.append(256, ' ');
    rig.line(overlong);
    rig.line(std::string("wiring di0 connected\0", 21));
    rig.line("wiring di0 connected\x01");
    std::string tokens = "wiring di0 connected";
    for (unsigned i = 0; i < E::ProbeConsole::TOKEN_CAPACITY; ++i)
        tokens += " x";
    rig.line(tokens);
    rig.drain();
    E::HostWiringUse use;
    CHECK(rig.app.hostWiring(0, use) && use == E::HostWiringUse::UNKNOWN);
    CHECK(rig.console.stats().invalid >= 4 && !rig.wire.fake.transmitCalls());
    rig.feed("@50 version\r\n");
    rig.drain();
    contains(rig.bytes.output, "\"id\":50");
    Rig overflow;
    overflow.bytes.input = "wiring di0 connected" + std::string(200, ' ');
    // Deliberately starve console ingress while ordinary owner/platform service
    // fills the finite input queue. Its dropped suffix cannot become a command.
    for (unsigned i = 0; i < 20; ++i) {
        ++overflow.now;
        CHECK(overflow.wire.fake.setTime(overflow.now));
        CHECK(overflow.app.service(overflow.now));
    }
    CHECK(overflow.platform.stats().inputDropped > 0);
    overflow.drain();
    overflow.feed("\n");
    overflow.drain();
    CHECK(overflow.console.stats().inputLost > 0);
    CHECK(overflow.app.hostWiring(0, use) && use == E::HostWiringUse::UNKNOWN);
    Rig earlierLine;
    const std::string damaged = "axis config set command 1";
    const std::string prefix = "version\n";
    for (unsigned interval = 0; interval < 2; ++interval) {
        earlierLine.bytes.input +=
            prefix + std::string(64 - prefix.size() - damaged.size(), ' ') + damaged + "00000000";
        for (unsigned i = 0; i < 9; ++i) {
            ++earlierLine.now;
            CHECK(earlierLine.wire.fake.setTime(earlierLine.now));
            CHECK(earlierLine.app.service(earlierLine.now));
        }
        CHECK(earlierLine.platform.stats().inputUsed == 64 &&
              earlierLine.platform.stats().inputDropped == 8 * (interval + 1));
        earlierLine.bytes.input += '\n';
        earlierLine.console.service(earlierLine.now);
        earlierLine.drain();
        // The earlier intact newline precedes the loss. It cannot qualify a later
        // truncated numeric mutation: intended 100000000 must never become 1.
        CHECK(!earlierLine.app.axisConfig());
        CHECK(earlierLine.console.stats().inputLost == interval + 1);
        CHECK(!earlierLine.wire.fake.transmitCalls());
    }
}
void retainedReadCorrelation(E::FakeTransport::Mode mode) {
    Rig rig(mode);
    CHECK(rig.app.select(E::FakeTransport::target()));
    CHECK(!rig.console.trackRead(60, true, 700));
    CHECK(rig.app.requestProbe(0, 100000, options()));
    CHECK(rig.console.trackRead(60, true, 700));
    CHECK(!rig.console.trackRead(60, true, 701));
    CHECK(!rig.console.trackRead(61, true, 700));
    CHECK(!rig.console.trackRead(61, true, 701)); // Same underlying app work cannot be aliased.
    reply(rig.command("@61 result 999"), false);
    reply(rig.command("@62 release 700"), false); // A pending result is not releasable.
    const std::string pending = rig.command("@63 result 700");
    contains(pending, "pending");
    // Owner service may retain wire completion, but only explicit app advancement can
    // settle the typed read and permit a console terminal.
    CHECK(rig.app.readContext().state == M::ReadState::ACTIVE);
    CHECK(rig.owner.result(rig.app.pendingRequest()));
    CHECK(rig.bytes.output.find("\"type\":\"terminal\"") == std::string::npos);
    rig.command("version"); // Human output cannot change the admitted JSON mode.
    rig.finishRead();
    contains(rig.bytes.output, "\"type\":\"terminal\"");
    contains(rig.bytes.output, "\"id\":60");
    contains(rig.bytes.output, "\"command_id\":60");
    contains(rig.bytes.output, "\"operation_id\":700");
    const std::string retained = rig.command("@64 result 700");
    contains(retained, "\"id\":64");
    contains(retained, "\"command_id\":60");
    contains(retained, "\"operation_id\":700");
    if (mode == E::FakeTransport::Mode::ABORT) {
        CHECK(rig.app.readContext().state == M::ReadState::FAILED);
        CHECK(rig.app.readContext().evidence.abortCode == 0xfedcba98u);
        contains(retained, "4275878552");
        contains(retained, "PROTOCOL_REJECTED");
    } else if (mode == E::FakeTransport::Mode::SILENT) {
        CHECK(rig.app.readContext().state == M::ReadState::FAILED);
        contains(retained, "\"uncertain\":true");
    } else {
        CHECK(rig.app.readContext().state == M::ReadState::SUCCEEDED);
        contains(retained, "SUCCESS");
    }
    CHECK(rig.services.node(0)->heartbeat.present && rig.services.node(0)->emcyCount == 1);
    CHECK(rig.wire.fake.transmitted() == 1 && rig.wire.fake.observed() >= 3);
    const Saved<L::ReadContext> evidence(rig.app.readContext());
    reply(rig.command("@68 stats reset"), true);
    evidence.unchanged(rig.app.readContext());
    CHECK(rig.wire.fake.transmitted() == 1); // Counters reset cannot replay a request.
    reply(rig.command("@65 release 701"), false);
    reply(rig.command("@66 release 700"), true);
    reply(rig.command("@67 result 700"), false);
    CHECK(rig.app.readContext().state == M::ReadState::EMPTY);
}
void recoverCorrelationAndCapacity() {
    Rig rig;
    const std::string admitted = rig.command("@70 recover");
    reply(admitted, true);
    contains(admitted, "\"result\":\"accepted\"");
    contains(admitted, "\"operation_id\":1");
    CHECK(rig.wire.begins == 1 && rig.owner.recoveryActive());
    reply(rig.command("@70 version"), false); // Original request is reserved until release.
    const std::string pending = rig.command("@71 result 1");
    contains(pending, "\"result\":\"pending\"");
    rig.command("version");
    rig.wire.recovery = O::RecoveryProgress::RETIRED;
    rig.drain();
    contains(rig.bytes.output, "\"id\":70");
    contains(rig.bytes.output, "\"command_id\":70");
    contains(rig.bytes.output, "\"outcome\":\"RETIRED\"");
    const std::string retained = rig.command("@72 result 1");
    contains(retained, "\"id\":72");
    contains(retained, "\"command_id\":70");
    contains(retained, "RETIRED");
    reply(rig.command("@73 release 1"), true);
    reply(rig.command("@74 result 1"), false);
    CHECK(!rig.wire.fake.transmitCalls());
    Rig failed;
    failed.wire.beginAccepted = false;
    const std::string failure = failed.command("@75 recover");
    contains(failure, "\"result\":\"accepted\"");
    contains(failure, "\"type\":\"terminal\"");
    contains(failure, "\"outcome\":\"FAILED\"");
    CHECK(failed.wire.begins == 1 && failed.wire.polls == 0 && !failed.wire.fake.transmitCalls());
}
void humanOperationKeepsItsMode() {
    Rig rig;
    CHECK(rig.app.select(E::FakeTransport::target()));
    CHECK(rig.app.requestProbe(0, 100000, options()));
    CHECK(rig.console.trackRead(0, false, 900));
    reply(rig.command("@90 version"), true);
    const size_t begin = rig.bytes.output.size();
    rig.finishRead();
    const std::string terminal = rig.bytes.output.substr(begin);
    CHECK(!terminal.empty() && terminal.find('{') == std::string::npos);
    contains(terminal, "SUCCESS");
    const std::string lookedUp = rig.command("@91 result 900");
    contains(lookedUp, "\"id\":91");
    contains(lookedUp, "\"command_id\":0");
    contains(lookedUp, "\"operation_id\":900");
}
void implicitLocalCancel() {
    Rig rig;
    CHECK(rig.app.select(E::FakeTransport::target()));
    CHECK(rig.app.requestProbe(0, 10000, options()));
    CHECK(rig.console.trackRead(95, true, 950));
    const std::string cancelled = rig.command("@96 cancel");
    reply(cancelled, true);
    CHECK(rig.app.readContext().outcome == M::ReadOutcome::CANCELLED);
    CHECK(!rig.wire.fake.transmitCalls());
    const std::string latest = rig.command("@97 result");
    contains(latest, "\"operation_id\":950");
    contains(latest, "\"command_id\":95");
    reply(rig.command("@98 release 950"), true);
    reply(rig.command("@99 cancel"), false);
}
void pressureAndLocalCancel() {
    Rig rig;
    CHECK(rig.app.select(E::FakeTransport::target()));
    CHECK(rig.app.requestProbe(0, 1000, options()));
    CHECK(rig.console.trackRead(80, true, 800));
    rig.bytes.blocked = true;
    const uint8_t filler[E::ProbePlatform::OUTPUT_CAPACITY] = {};
    CHECK(rig.platform.writeOutput(filler, sizeof filler));
    rig.turn();
    rig.turn();
    CHECK(rig.wire.fake.transmitted() == 1);
    rig.line("@81 version");
    rig.line("@82 status");
    const M::AxisConfig* before = rig.app.axisConfig();
    rig.line("@83 axis config set command 100");
    CHECK(rig.app.axisConfig() == before); // Ordinary queue pressure prevents mutation.
    CHECK(rig.console.stats().outputDropped > 0);
    rig.line("@84 cancel 800"); // Reserved control slot remains available.
    rig.drain(60);
    CHECK(rig.app.readContext().state != M::ReadState::ACTIVE ||
          rig.owner.result(rig.app.pendingRequest()));
    if (rig.owner.result(rig.app.pendingRequest()))
        CHECK(rig.app.advanceRead(rig.now));
    CHECK(rig.app.readContext().state == M::ReadState::FAILED &&
          rig.app.readContext().outcome == M::ReadOutcome::CANCELLED);
    CHECK(rig.wire.fake.receiveCalls() > 0 && rig.wire.fake.observed() > 0 &&
          rig.console.stats().queued <= 4);
    rig.bytes.blocked = false;
    rig.drain();
    contains(rig.bytes.output, "\"id\":84");
    contains(rig.bytes.output, "\"command_id\":80");
    contains(rig.bytes.output, "CANCELLED");
    // All records after the deliberately queued non-JSON prefix remain whole lines.
    const size_t first = rig.bytes.output.find('{');
    CHECK(first != std::string::npos);
    size_t cursor = first;
    while (cursor < rig.bytes.output.size()) {
        const size_t end = rig.bytes.output.find('\n', cursor);
        CHECK(end != std::string::npos && rig.bytes.output[cursor] == '{' &&
              rig.bytes.output[end - 1] == '}');
        CHECK(end - cursor + 1 < E::ProbeConsole::RECORD_CAPACITY);
        cursor = end + 1;
    }
}
void releaseWaitsForPhysicalOutput() {
    Rig rig;
    rig.bytes.blocked = true;
    rig.line("@110 recover");
    rig.turn();
    rig.wire.recovery = O::RecoveryProgress::RETIRED;
    rig.drain(10);
    CHECK(rig.platform.stats().outputUsed && rig.platform.stats().outputWritten == 0);
    rig.line("@111 release 1");
    rig.drain(10);
    rig.bytes.blocked = false;
    rig.drain();
    contains(rig.bytes.output, "result_not_releasable");
    const std::string retained = rig.command("@112 result 1");
    contains(retained, "\"command_id\":110");
    contains(retained, "\"outcome\":\"RETIRED\"");
    reply(rig.command("@113 release 1"), true);
}
void externallyReleasedReadDoesNotOwnReplacement() {
    Rig rig;
    CHECK(rig.app.select(E::FakeTransport::target()));
    CHECK(rig.app.requestProbe(0, 10000, options()));
    CHECK(rig.console.trackRead(120, true, 1200));
    const uint32_t original = rig.app.readContext().operation;
    // Progress the ordinary application owner without polling the console. The
    // application is allowed to inspect and release its own retained context.
    for (unsigned i = 0; i < 100 && rig.app.readContext().state == M::ReadState::ACTIVE; ++i) {
        ++rig.now;
        CHECK(rig.wire.fake.setTime(rig.now));
        CHECK(rig.app.service(rig.now));
        if (rig.owner.result(rig.app.pendingRequest()))
            CHECK(rig.app.advanceRead(rig.now));
    }
    CHECK(rig.app.readContext().state == M::ReadState::SUCCEEDED);
    CHECK(rig.app.releaseRead());
    rig.console.service(rig.now);
    rig.drain();
    const std::string invalidated = rig.command("@121 result 1200");
    contains(invalidated, "\"outcome\":\"INVALIDATED\"");
    contains(invalidated, "\"command_id\":120");
    CHECK(rig.app.requestProbe(rig.now, rig.now + 10000, options()));
    const uint32_t replacement = rig.app.readContext().operation;
    CHECK(replacement != original && rig.app.readContext().state == M::ReadState::ACTIVE);
    reply(rig.command("@122 release 1200"), true);
    CHECK(rig.app.readContext().operation == replacement &&
          rig.app.readContext().state == M::ReadState::ACTIVE);
    reply(rig.command("@123 result 1200"), false);
    // The replacement still owns its real pending result and can finish normally.
    CHECK(rig.owner.result(rig.app.pendingRequest()));
    rig.finishRead();
    CHECK(rig.app.readContext().state == M::ReadState::SUCCEEDED && rig.app.releaseRead());
}
void axisListingOwnsItsSnapshot() {
    Rig rig;
    const char* settings[] = {"command 3/2",      "gear 5/7",          "lead 2",
                              "encoder-id 9",     "encoder-scale 100", "native-limits -1000 1000",
                              "soft-limits -9 11"};
    for (size_t i = 0; i < sizeof settings / sizeof settings[0]; ++i)
        reply(rig.command(std::string("@130 axis config set ") + settings[i]), true);
    const uint32_t generation = rig.app.axisConfig()->binding.configurationGeneration;
    const size_t begin = rig.bytes.output.size();
    rig.line("@131 axis config");
    E::HostAxisCommand mutation;
    mutation.kind = E::HostAxisCommandKind::CONFIGURE;
    mutation.field = E::HostAxisField::COMMAND_SCALE;
    mutation.value = M::Rational(99, 1);
    E::HostAxisView changed;
    CHECK(rig.app.executeHostAxis(mutation, rig.now, changed));
    CHECK(!rig.app.axisConfig()->softLimitsKnown);
    rig.drain();
    const std::string snapshot = rig.bytes.output.substr(begin);
    contains(snapshot, "\"command\":\"axis\"");
    contains(snapshot, "\"command_scale\":[\"3\",\"2\",1]");
    contains(snapshot, "\"gear\":[\"5\",\"7\",1]");
    contains(snapshot, "\"encoder_scale\":[\"100\",\"1\",1]");
    contains(snapshot, "\"encoder_id\":9");
    contains(snapshot, "\"native_minimum\":\"-1000\"");
    contains(snapshot, "\"soft_limits\":[\"-9\",\"11\"]");
    contains(snapshot,
             (std::string("\"configuration_generation\":") + std::to_string(generation)).c_str());
    contains(snapshot, "\"result\":\"complete\"");
    const std::string help = rig.command("@132 help");
    contains(help, "\"type\":\"help\"");
    CHECK(help.find("\"type\":\"axis\"") == std::string::npos);
    const std::string caps = rig.command("@133 caps");
    contains(caps, "\"type\":\"capability\"");
    CHECK(caps.find("\"type\":\"axis\"") == std::string::npos);
    CHECK(!rig.wire.fake.transmitCalls());
}
void jsonTranscript() {
    Rig rig;
    rig.command("@1 version");
    rig.command("@2 status");
    rig.command("@3 caps");
    rig.command("@4 help");
    rig.command("@5 config");
    rig.command("@6 memory");
    rig.command("@7 axis config set command 1");
    rig.command("@8 prepare absolute 9007199254740993 steps native");
    rig.command("@9 wiring di0 connected");
    rig.command("@10 prepare absolute 1e2 steps native");
    rig.command("@11 recover");
    rig.wire.recovery = O::RecoveryProgress::RETIRED;
    rig.drain();
    rig.command("@12 result 1");
    rig.command("@13 release 1");
    rig.command("@14 axis config");
    std::fwrite(rig.bytes.output.data(), 1, rig.bytes.output.size(), stdout);
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "--json-transcript")) {
#ifdef _WIN32
        CHECK(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
        jsonTranscript();
        return 0;
    }
    wholeLineParser();
    escapedStrings();
    passiveRoutesAndGrammar();
    inventoryAndHelp();
    exactLocalTargets();
    roundingAndBinaryPreviews();
    inputLossLatchPreservesBuffersAndCounters();
    damagedLinesNeverDispatch();
    retainedReadCorrelation(E::FakeTransport::Mode::SUCCESS);
    retainedReadCorrelation(E::FakeTransport::Mode::ABORT);
    retainedReadCorrelation(E::FakeTransport::Mode::SILENT);
    recoverCorrelationAndCapacity();
    humanOperationKeepsItsMode();
    implicitLocalCancel();
    pressureAndLocalCancel();
    releaseWaitsForPhysicalOutput();
    externallyReleasedReadDoesNotOwnReplacement();
    axisListingOwnsItsSnapshot();
    std::printf("probe_console: %u checks\n", checks);
}
