// SPDX-License-Identifier: MIT
#include "FakeTransport.h"
#include "ProbeApp.h"
#include "ProbeConsole.h"
#include <cstdio>
#include <cstring>
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
bool clockUs(void* context, uint64_t& output) {
    output = static_cast<E::FakeTransport*>(context)->nowUs();
    return true;
}
struct BatchConsole {
    enum { INPUT_LIMIT = 4096, OUTPUT_LIMIT = 65536, TURN_LIMIT = 2000000 };
    uint8_t input[INPUT_LIMIT + 1] = {};
    uint8_t output[OUTPUT_LIMIT] = {};
    size_t inputSize = 0, outputSize = 0;
    bool outputFull = false;
    E::FakeTransport* fake = nullptr;
    static bool clock(void* context, uint64_t& output) {
        output = static_cast<BatchConsole*>(context)->fake->nowUs();
        return true;
    }
    static bool capture(void* context, uint8_t byte) {
        BatchConsole& batch = *static_cast<BatchConsole*>(context);
        if (batch.outputSize == OUTPUT_LIMIT) {
            batch.outputFull = true;
            return false;
        }
        batch.output[batch.outputSize++] = byte;
        return true;
    }
    bool load(const char* path) {
        std::FILE* file = std::fopen(path, "rb");
        if (!file)
            return false;
        inputSize = std::fread(input, 1, sizeof input, file);
        const bool readFailed = std::ferror(file) != 0;
        const bool closeFailed = std::fclose(file) != 0;
        if (readFailed || closeFailed || inputSize > INPUT_LIMIT)
            return false;
        // Validate every physical line before constructing the application. Grammar
        // rejection still comes from the shared console, with its normal correlation.
        size_t begin = 0;
        for (size_t n = 0; n <= inputSize; ++n) {
            if (n != inputSize && input[n] != '\r' && input[n] != '\n')
                continue;
            if (n != begin) {
                E::ConsoleLine parsed;
                if (!E::parseConsoleLine(reinterpret_cast<const char*>(input + begin), n - begin,
                                         parsed))
                    return false;
            }
            begin = n + 1;
        }
        if (inputSize && input[inputSize - 1] != '\r' && input[inputSize - 1] != '\n')
            return false; // EOF cannot turn a truncated command prefix into intent.
        return true;
    }
    void trimPartialRecord() {
        while (outputSize && output[outputSize - 1] != '\n')
            --outputSize;
    }
};
int runConsoleFake(const char* path) {
    // This application-only capture is deliberately outside the owner's task stack.
    static BatchConsole batch;
    if (!batch.load(path)) {
        std::fputs(
            "Console script refused: unreadable, over 4096 bytes, or invalid ASCII/line bounds.\n",
            stderr);
        return 2;
    }
#ifdef _WIN32
    // Preserve the captured JSONL bytes and its bound across the native CRT.
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        std::fputs("Console fake refused: binary output mode unavailable.\n", stderr);
        return 1;
    }
#endif
    C::SdoClient services;
    // This --console-fake-only route is an explicitly synthetic session, never device discovery.
    const auto target = E::FakeTransport::target();
    C::NetworkRoute route;
    route.binding = target.channel.binding;
    route.generation = target.nodeGeneration;
    route.heartbeatId = 0x705;
    route.emcyId = 0x85;
    if (!services.configure(0, target.channel, true, true) ||
        !services.configureNetwork(0, route, C::NetworkObserver(), 0))
        return 1;
    E::FakeTransport fake;
    batch.fake = &fake;
    O::CanBusOwner owner(services, fake.callbacks(), 40);
    E::ProbeCallbacks callbacks;
    callbacks.context = &batch;
    callbacks.nowUs = BatchConsole::clock;
    callbacks.writeByte = BatchConsole::capture;
    E::ProbePlatform platform(callbacks);
    E::ProbeApp app(owner, platform);
    E::ProbeConsole console(app, owner, platform);
    E::ProbeDevice device(app, owner, services);
    console.attachDevice(device);
    E::HostInterfaceFacts interface;
    interface.interfaceKnown = interface.bitrateKnown = true;
    interface.interfaceId = target.channel.binding.endpoint.bus;
    interface.bitrateBitsPerSecond = 500000; // Offline fake fixture, not a board default.
    app.setHostInterfaceFacts(interface);
    if (fake.transmitCalls())
        return 1; // Construction/selection metadata cannot issue a request.
    size_t consumed = 0;
    bool waitForLine = false, complete = false;
    for (unsigned turn = 0; turn < BatchConsole::TURN_LIMIT; ++turn) {
        if (!fake.setTime(fake.nowUs() + 1) || !app.service(fake.nowUs()))
            break;
        if (batch.outputFull)
            break;
        if (waitForLine && console.drained() && !platform.stats().outputUsed)
            waitForLine = false;
        if (!waitForLine) {
            // Pace whole commands behind their complete replies; an expanded help
            // listing cannot overflow the console's bounded ordinary reply queue.
            for (unsigned n = 0; n < E::ProbePlatform::INPUT_BUDGET && consumed < batch.inputSize;
                 ++n) {
                const uint8_t byte = batch.input[consumed++];
                console.feed(byte, fake.nowUs());
                if (byte == '\r' || byte == '\n') {
                    waitForLine = true;
                    break;
                }
            }
        }
        console.service(fake.nowUs());
        if (consumed == batch.inputSize && console.drained() && !platform.stats().outputUsed) {
            complete = true;
            break;
        }
    }
    // The real console alone constructs records. Never publish a captured prefix
    // as a complete JSON response if the bounded sink or turn budget was exhausted.
    if (!complete)
        batch.trimPartialRecord();
    const bool written = !batch.outputSize ||
                         std::fwrite(batch.output, 1, batch.outputSize, stdout) == batch.outputSize;
    const bool flushed = std::fflush(stdout) == 0;
    if (!complete || !written || !flushed || console.stats().outputDropped ||
        platform.stats().inputDropped) {
        std::fputs("Console fake failed: capture/turn budget or output.\n", stderr);
        return 1;
    }
    return 0;
}
// The application explicitly inspects each retained owner terminal twice,
// advances the concrete typed context, then releases the owner result slot.
bool finishRead(E::ProbeApp& app, O::CanBusOwner& owner, E::FakeTransport& fake,
                E::ProbePlatform& platform) {
    for (unsigned turn = 0; turn < 100 && app.readContext().state == M::ReadState::ACTIVE; ++turn) {
        if (!fake.setTime(fake.nowUs() + 1))
            return false;
        uint64_t now = 0;
        if (!platform.nowUs(now) || !app.service(now))
            return false;
        const O::RequestId pending = app.pendingRequest();
        const O::Completion* terminal = owner.result(pending);
        if (!terminal)
            continue;
        if (terminal != owner.result(pending) || terminal->reason != O::CompletionReason::SERVICE ||
            terminal->service.outcome != C::SdoOutcome::SUCCESS || !app.advanceRead(now) ||
            owner.result(pending))
            return false;
    }
    return app.readContext().state == M::ReadState::SUCCEEDED;
}
} // namespace
int runFake() {
    const L::ReadTarget target = E::FakeTransport::target();
    C::NetworkRoute network;
    network.binding = target.channel.binding;
    network.generation = target.nodeGeneration;
    network.heartbeatId = 0x705;
    network.emcyId = 0x85;
    C::SdoClient services;
    // These true flags are explicit synthetic fixture premises, never a startup
    // qualification procedure for hardware. Unknown read effects/Q20 remain open.
    if (!services.configure(0, target.channel, true, true) ||
        !services.configureNetwork(0, network, C::NetworkObserver(), 0))
        return 1;
    E::FakeTransport fake;
    O::CanBusOwner owner(services, fake.callbacks(), 26);
    E::ProbeCallbacks callbacks;
    callbacks.context = &fake;
    callbacks.nowUs = clockUs;
    E::ProbePlatform platform(callbacks);
    E::ProbeApp app(owner, platform);
    O::Health health;
    if (!app.select(target) || !app.health(health) || fake.transmitCalls())
        return 1;
    L::ReadOptions options;
    options.queueTimeoutUs = options.responseTimeoutUs = 100;
    if (!app.requestIdentity(0, 1000, options) || fake.transmitCalls() ||
        !finishRead(app, owner, fake, platform))
        return 1;
    L::IdentityObservation identity;
    if (!app.getIdentity(fake.nowUs(), 1000, identity) || identity.modelMatched ||
        !app.releaseRead())
        return 1;
    if (!app.requestConfig(fake.nowUs(), fake.nowUs() + 1000, options) ||
        !finishRead(app, owner, fake, platform))
        return 1;
    L::ConfigObservation config;
    if (!app.getConfig(fake.nowUs(), 1000, config) || !app.releaseRead() ||
        fake.transmitted() != 15 || services.node(0)->emcyCount != 15 ||
        !services.node(0)->heartbeat.present || !owner.stats().unmatchedFrames)
        return 1;
    std::puts("FAKE ONLY: externally fresh/usable session premise; Q20 remains unresolved.");
    std::printf("Synthetic identity: 9 fields, device type 0x%08lx, modelMatched=false.\n",
                static_cast<unsigned long>(identity.deviceType));
    std::printf("Synthetic config: 6 fields, producer %u ms; routes observed only.\n",
                unsigned(config.producerTimeMs));
    std::printf("15 explicit uploads; heartbeat/EMCY interleaved; %lu unrelated frames.\n",
                static_cast<unsigned long>(owner.stats().unmatchedFrames));
    std::printf("Native storage: fake %lu, service %lu, owner %lu, app %lu, platform %lu bytes.\n",
                static_cast<unsigned long>(sizeof fake),
                static_cast<unsigned long>(sizeof services),
                static_cast<unsigned long>(sizeof owner), static_cast<unsigned long>(sizeof app),
                static_cast<unsigned long>(sizeof platform));
    std::puts("All owner terminals inspected and released; no hardware qualification.");
    return 0;
}
int main(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "--fake"))
        return runFake();
    if (argc == 3 && !std::strcmp(argv[1], "--console-fake"))
        return runConsoleFake(argv[2]);
    std::fputs("Usage: native_probe --fake | --console-fake FILE (synthetic only)\n", stderr);
    return 2;
}
